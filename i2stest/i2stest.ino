// This is for Teensy 3.0
// using ARM Cortex M4 math routines
#define ARM_MATH_CM4
#include <arm_math.h>

/* I2S digital audio */
#include <i2s.h>

/* Wolfson audio codec controlled by I2C */
/* Library here: https://github.com/hughpyle/machinesalem-arduino-libs/tree/master/WM8731 */
#include <Wire.h>
#include <WM8731.h>


/*
  SDA -> Teensy pin 18
  SCL -> Teensy pin 19
  SCK -> Teensy 11 (ALT6 I2S0_MCLK) (PTC6/LLWU_P10)
  MOSI -> Teensy 3 (ALT6 I2S0_TXD0) (PTA12) // 22
*/

const int isDMA = 0;
const int codecIsMaster = 1;


void setup()
{
  initsinevalue(); 
  Serial.println( "Initializing" );
  
  delay(2000); 
  Serial.println( "Initializing." );

  delay(1000);
  unsigned char interface = WM8731_INTERFACE_FORMAT(I2S) | WM8731_INTERFACE_WORDLEN(bits16);
  if( codecIsMaster )
  {
    interface |= WM8731_INTERFACE_MASTER;
  }  
  WM8731.begin( low, WM8731_SAMPLING_RATE(hz48000), interface );
  WM8731.setActive();
  WM8731.setOutputVolume( 255 );
  Serial.println( "Initialized I2C" );
  
  delay(1000);
  if( codecIsMaster )
  {
    // External clock, 48k sample rate
    Serial.println( "Initializing for external clock" );
    Serial.println( i2s_init(I2S_CLOCK_EXTERNAL), DEC );
  }
  else
  {
    // Internal clock, directly writing to the i2s FIFO
    Serial.println( "Initializing for internal clock" );
    Serial.println( i2s_init(I2S_CLOCK_48K_INTERNAL), DEC );  
  }
  Serial.println( "Initialized I2S." );  

  if( isDMA )
  {  
    delay(1000);
    dma_init();
    Serial.println( "Initialized DMA." );  
  }
  i2s_start(isDMA);
}


// audio data
q15_t audx = 0;
q15_t audy = 0.9 * 32767;
q15_t audd = 2.0 * sin(PI*440/48000) * 32767;
q15_t audf = 1.0 * 32767;
uint32_t a = 0;
uint32_t b = 0;
uint32_t p = 0;
uint32_t nnn=0;
void initsinevalue()
{
  int   x = 45 + random(48);                             // midi note number
  float f = (440.0 / 32) * pow(2, ((float)x - 9) / 12);  // Hz.  For realz, use a lookup table.
  audd = 2.0 * sin(PI*f/48000) * 32767;                  // delta (q15_t)
  audx = 0;
  audy = 32767;
  a = __PKHBT( audf, audd, 16 );
  b = __PKHBT( audx, audy, 16 );
}
void nextsinevalue() 
{
  // b = 0xACCF0010; audx=0xACCF; return;
  //  http://cabezal.com/misc/minsky-circles.html
  p = __SMUAD(a,b)<<1;
  b = (b>>16) + (p & 0xFFFF0000);
  p = __SMUSD(a,b)<<1;
  b = (b>>16) + (p & 0xFFFF0000);
  audx = (q15_t)(p & 0xFFFF);
  nnn++;
  if(nnn>48000) {nnn=0;initsinevalue();};
}

/* --------------------- Direct I2S data transfer, we get the FIFO callback ----- */

/*
Writes to a TDR are ignored if the corresponding bit of TCR3[TCE] is clear or if the
FIFO is full. If the Transmit FIFO is empty, the TDR must be written at least three bit
clocks before the start of the next unmasked word to avoid a FIFO underrun.
*/

/*
The transmit data ready flag is set when the number of entries in any of the enabled
transmit FIFOs is less than or equal to the transmit FIFO watermark configuration and is
cleared when the number of entries in each enabled transmit FIFO is greater than the
transmit FIFO watermark configuration.
*/

int8_t ever_called_tx = 0;
void i2s0_tx_isr(void)
{
  // Clear the FIFO error flag
  if(I2S0_TCSR & I2S_TCSR_FEF)  // underrun
     I2S0_TCSR |= I2S_TCSR_FEF; // clear

  if(I2S0_TCSR & I2S_TCSR_SEF)  // frame sync error
     I2S0_TCSR |= I2S_TCSR_SEF; // clear

  // Send two words of data (the FIFO buffer is just 2 words)
  I2S0_TDR0 = (uint32_t)b; //audx;
  //nextsinevalue();
  I2S0_TDR0 = (uint32_t)b; //audx;
  nextsinevalue();

//  Serial.println( I2S0_TCSR, HEX );  
}





/* ----------------------- DMA transfer ------------------------- */


void dma_fill( int isA, int16_t *pBuf, int16_t len )
{
  uint32_t es;

//Serial.println(isA);

  es = DMA_ES;
  if(es>0) Serial.println( String("ES:") + es );  // DMA error status
  es = DMA_ERR;
  if(es>0) Serial.println( String("ERR:") + es );  // DMA error status

  while( len>0 )
  {
    *pBuf++ = audx;
    *pBuf++ = audx;
    nextsinevalue();
    len--;
    len--;
  }
  // Serial.println("fills");
}




/* --------------------- main loop ------------------ */
void loop()
{
  uint32_t es;
  
  if( isDMA )
  {
    delay(5000);
    dma_play();
    Serial.println( "DMA playing." );
    
    es = DMA_ES;
    if(es>0) Serial.println( String("ES:") + es );  // DMA error status
    es = DMA_ERR;
    if(es>0) Serial.println( String("ERR:") + es );  // DMA error status
  
    delay(5000);
    dma_stop();
    Serial.println( "DMA stopped." );  
  }
  else
  {
    // Bang some data at the I2S port directly
    delay(1000);
    Serial.println( "I2S playing." );
  }
}


void dma_error_isr(void)
{
  DMA_CINT = DMA_CINT_CINT(0);

  Serial.println("Error ISR");
}

