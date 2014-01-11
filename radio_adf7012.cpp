/* pecan copyright (C) 2012  KT5TK
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define SPI_BITBANG
#include "config.h"
#include <math.h>
#include "radio_adf7012.h"


#if !defined(SPI_BITBANG)
#include <SPI.h>
#endif


#if defined(ARDUINO) && ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

//#include <SoftwareSerial.h>
//SoftwareSerial mySerial(ADC2_PIN, ADC1_PIN); // RX, TX




const int MAX_RES = 16;
char res_adf7012[MAX_RES];
unsigned int powerlevel;

// Configuration storage structs =============================================
struct {
    struct {
        unsigned int  frequency_error_correction;
        unsigned char r_divider;
        unsigned char crystal_doubler;
        unsigned char crystal_oscillator_disable;
        unsigned char clock_out_divider;
        unsigned char vco_adjust;
        unsigned char output_divider;
    } r0;

    struct {
        unsigned int  fractional_n;
        unsigned char integer_n;
        unsigned char prescaler;
    } r1;

    struct {
        unsigned char mod_control;
        unsigned char gook;
        unsigned char power_amplifier_level;
        unsigned int  modulation_deviation;
        unsigned char gfsk_modulation_control;
        unsigned char index_counter;
    } r2;

    struct {
        unsigned char pll_enable;
        unsigned char pa_enable;
        unsigned char clkout_enable;
        unsigned char data_invert;
        unsigned char charge_pump_current;
        unsigned char bleed_up;
        unsigned char bleed_down;
        unsigned char vco_disable;
        unsigned char muxout;
        unsigned char ld_precision;
        unsigned char vco_bias;
        unsigned char pa_bias;
        unsigned char pll_test_mode;
        unsigned char sd_test_mode;
    } r3;
} adf_config;




// Write directly to AVR port in SPIwrite() instead of using digitalWrite()
//#define FAST_IO

// Configuration functions ===================================================

// Config resetting functions --------------------------------------------
void RadioAdf7012::adf_reset_config(void) 
{
  
    adf_reset_register_zero();
    adf_reset_register_one();
    adf_reset_register_two();
    adf_reset_register_three();

    adf_reset();  


//    while(!adf_reg_ready());

}

// Power up default settings are defined here:

void RadioAdf7012::adf_reset_register_zero(void) {
    adf_config.r0.frequency_error_correction = 0;               // Don't bother for now...
    adf_config.r0.r_divider = ADF7012_CRYSTAL_DIVIDER;          // Whatever works best for 2m, 1.25m and 70 cm ham bands
    adf_config.r0.crystal_doubler = 0;                          // Who would want that? Lower f_pfd means finer channel steps.
    adf_config.r0.crystal_oscillator_disable = 1;               // Disable internal crystal oscillator because we have an external VCXO
    adf_config.r0.clock_out_divider = 2;                        // Don't bother for now...
    adf_config.r0.vco_adjust = 0;                               // Don't bother for now... (Will be automatically adjusted until PLL lock is achieved)
    adf_config.r0.output_divider = ADF_OUTPUT_DIVIDER_BY_4;     // Pre-set div 4 for 2m. Will be changed according tx frequency on the fly
}

void RadioAdf7012::adf_reset_register_one(void) {
    adf_config.r1.integer_n = 111;                              // Pre-set for 144.390 MHz APRS. Will be changed according tx frequency on the fly
    adf_config.r1.fractional_n = 1687;                          // Pre-set for 144.390 MHz APRS. Will be changed according tx frequency on the fly
    adf_config.r1.prescaler = ADF_PRESCALER_8_9;                // 8/9 requires an integer_n > 91; 4/5 only requires integer_n > 31
}

void RadioAdf7012::adf_reset_register_two(void) {
    adf_config.r2.mod_control = ADF_MODULATION_ASK;             // For AFSK the modulation is done through the external VCXO we don't want any FM generated by the ADF7012 itself
    adf_config.r2.gook = 0;                                     // Whatever... This might give us a nicer swing in phase maybe...
    adf_config.r2.power_amplifier_level = 16;                   // 16 is about half maximum power. Output −20dBm at 0x0, and 13 dBm at 0x7E at 868 MHz

    adf_config.r2.modulation_deviation = 16;                    // 16 is about half maximum amplitude @ ASK.
    adf_config.r2.gfsk_modulation_control = 0;                  // Don't bother for now...
    adf_config.r2.index_counter = 0;                            // Don't bother for now...
}

void RadioAdf7012::adf_reset_register_three(void) {
    adf_config.r3.pll_enable = 0;                               // Switch off PLL (will be switched on after Ureg is checked and confirmed ok)
    adf_config.r3.pa_enable = 0;                                // Switch off PA  (will be switched on when PLL lock is confirmed)
    adf_config.r3.clkout_enable = 0;                            // No clock output needed at the moment
    adf_config.r3.data_invert = 1;                              // Results in a TX signal when TXDATA input is low
    adf_config.r3.charge_pump_current = ADF_CP_CURRENT_2_1;     // 2.1 mA. This is the maximum
    adf_config.r3.bleed_up = 0;                                 // Don't worry, be happy...
    adf_config.r3.bleed_down = 0;                               // Dito
    adf_config.r3.vco_disable = 0;                              // VCO is on
    
    adf_config.r3.muxout = ADF_MUXOUT_REG_READY;                // Lights up the green LED if the ADF7012 is properly powered (changes to lock detection in a later stage)

    adf_config.r3.ld_precision = ADF_LD_PRECISION_3_CYCLES;     // What the heck? It is recommended that LDP be set to 1; 0 is more relaxed
    adf_config.r3.vco_bias = 6;                                 // In 0.5 mA steps; Default 6 means 3 mA; Maximum (15) is 8 mA
    adf_config.r3.pa_bias = 4;                                  // In 1 mA steps; Default 4 means 8 mA; Minimum (0) is 5 mA; Maximum (7) is 12 mA (Datasheet says uA which is bullshit)
    adf_config.r3.pll_test_mode = 0;
    adf_config.r3.sd_test_mode = 0;
}

void RadioAdf7012::adf_reset(void) {
//    digitalWrite(PTT_PIN, LOW); 
    digitalWrite(SSpin,   HIGH);
    digitalWrite(ADF7012_TX_DATA_PIN, HIGH);
    digitalWrite(SCKpin,  HIGH);
    digitalWrite(MOSIpin, HIGH);
    
//    delay(1);
    
//    digitalWrite(PTT_PIN, HIGH);
    
    delay(100);
}




// Configuration writing functions ---------------------------------------
void RadioAdf7012::adf_write_config(void) {
    adf_write_register_zero();
    adf_write_register_one();
    adf_write_register_two();
    adf_write_register_three();
}

void RadioAdf7012::adf_write_register_zero(void) {
  
    unsigned long reg =
        (0) |
        ((unsigned long)(adf_config.r0.frequency_error_correction & 0x7FF) << 2U) |
        ((unsigned long)(adf_config.r0.r_divider & 0xF ) << 13U) |
        ((unsigned long)(adf_config.r0.crystal_doubler & 0x1 ) << 17U) |
        ((unsigned long)(adf_config.r0.crystal_oscillator_disable & 0x1 ) << 18U) |
        ((unsigned long)(adf_config.r0.clock_out_divider & 0xF ) << 19U) |
        ((unsigned long)(adf_config.r0.vco_adjust & 0x3 ) << 23U) |
        ((unsigned long)(adf_config.r0.output_divider & 0x3 ) << 25U);
        
//    Serial.print("r0 = ");
//    Serial.print(reg, HEX);
//    Serial.print(" = ");
//    Serial.print(reg, BIN);
//    Serial.println();
    
    adf_write_register(reg);
}

void RadioAdf7012::adf_write_register_one(void) {
    unsigned long reg =
        (1) |
        ((unsigned long)(adf_config.r1.fractional_n & 0xFFF) << 2) |
        ((unsigned long)(adf_config.r1.integer_n & 0xFF ) << 14) |
        ((unsigned long)(adf_config.r1.prescaler & 0x1 ) << 22);
        
//    Serial.print("r1 = ");
//    Serial.print(reg, HEX);
//    Serial.print(" = ");
//    Serial.print(reg, BIN);
//    Serial.println();

    adf_write_register(reg);
}

void RadioAdf7012::adf_write_register_two(void) {
    unsigned long reg =
        (2) |
        ((unsigned long)(adf_config.r2.mod_control & 0x3 ) << 2) |
        ((unsigned long)(adf_config.r2.gook & 0x1 ) << 4) |
        ((unsigned long)(adf_config.r2.power_amplifier_level & 0x3F ) << 5) |
        ((unsigned long)(adf_config.r2.modulation_deviation & 0x1FF) << 11) |
        ((unsigned long)(adf_config.r2.gfsk_modulation_control & 0x7 ) << 20) |
        ((unsigned long)(adf_config.r2.index_counter & 0x3 ) << 23);
        
//    Serial.print("r2 = ");
//    Serial.print(reg, HEX);
//    Serial.print(" = ");
//    Serial.print(reg, BIN);
//    Serial.println();

    adf_write_register(reg);
}

void RadioAdf7012::adf_write_register_three(void) {
    unsigned long reg =
        (3) |
        ((unsigned long)(adf_config.r3.pll_enable & 0x1 ) << 2) |
        ((unsigned long)(adf_config.r3.pa_enable & 0x1 ) << 3) |
        ((unsigned long)(adf_config.r3.clkout_enable & 0x1 ) << 4) |
        ((unsigned long)(adf_config.r3.data_invert & 0x1 ) << 5) |
        ((unsigned long)(adf_config.r3.charge_pump_current & 0x3 ) << 6) |
        ((unsigned long)(adf_config.r3.bleed_up & 0x1 ) << 8) |
        ((unsigned long)(adf_config.r3.bleed_down & 0x1 ) << 9) |
        ((unsigned long)(adf_config.r3.vco_disable & 0x1 ) << 10) |
        ((unsigned long)(adf_config.r3.muxout & 0xF ) << 11) |
        ((unsigned long)(adf_config.r3.ld_precision & 0x1 ) << 15) |
        ((unsigned long)(adf_config.r3.vco_bias & 0xF ) << 16) |
        ((unsigned long)(adf_config.r3.pa_bias & 0x7 ) << 20) |
        ((unsigned long)(adf_config.r3.pll_test_mode & 0x1F ) << 23) |
        ((unsigned long)(adf_config.r3.sd_test_mode & 0xF ) << 28);
        
//    Serial.print("r3 = ");
//    Serial.print(reg, HEX);
//    Serial.print(" = ");
//    Serial.print(reg, BIN);
//    Serial.println();

    adf_write_register(reg);
}

void RadioAdf7012::adf_write_register(unsigned long data) 
{
  //adf_reset();
  digitalWrite(SSpin,   HIGH);
  digitalWrite(ADF7012_TX_DATA_PIN, HIGH);
  digitalWrite(SCKpin,  HIGH);
  digitalWrite(MOSIpin, HIGH);
  
#if !defined(SPI_BITBANG)   // use SPI library
  // take the SS pin low to select the ADF7012 chip:
  digitalWrite(SSpin,LOW);
  
  for (int j = 3; j >= 0; j--) // Loop through the 4 bytes of the unsigned long
  {
    byte wordb = (byte) (data >> (j * 8));
    SPI.transfer(wordb);
    //Serial.print(wordb, HEX);
    //Serial.print("j");
    // delay(1000);
  } 

  // take the SS pin high to de-select the ADF7012 chip:
  digitalWrite(SSpin,HIGH);

#else
 
  // Bit bang SPI to ADF7012 
    int i;
    digitalWrite(SCKpin,  LOW );
    delayMicroseconds(2);
    digitalWrite(SSpin, LOW);
    delayMicroseconds(10);

    for(i=31; i>=0; i--) {
        if((data & (unsigned long)(1UL<<i))>>i)
            digitalWrite(MOSIpin, HIGH);
        else
            digitalWrite(MOSIpin, LOW);
        delayMicroseconds(2);
        digitalWrite(SCKpin, HIGH);
        delayMicroseconds(10);
        digitalWrite(SCKpin,  LOW );
        delayMicroseconds(10);
    }

    delayMicroseconds(2);
    digitalWrite(SSpin, HIGH);
    
#endif

}




int RadioAdf7012::adf_lock(void) 
{
    // fiddle around with bias and adjust capacity until the vco locks
    
    int adj = adf_config.r0.vco_adjust; // use default start values from setup
    int bias = adf_config.r3.vco_bias;  // or the updated ones that worked last time
    
    adf_config.r3.pll_enable = 1;
    adf_config.r3.muxout = ADF_MUXOUT_DIGITAL_LOCK;
    adf_write_config();
    delay(50);
    adf_locked();
   
    while(!adf_locked()) {
//        Serial.print("VCO not in lock. Trying adj: "); 
//        Serial.print(adj);
//        Serial.print(" and bias: ");
//        Serial.println(bias);   
        adf_config.r0.vco_adjust = adj;
        adf_config.r3.vco_bias = bias;
        adf_config.r3.muxout = ADF_MUXOUT_DIGITAL_LOCK;
        adf_write_config();
        delay(50);
        if(++bias == 14) {
            bias = 1;
            if(++adj == 4) {
                Serial.println("Couldn't achieve PLL lock :( ");
                // Using best guess defaults:
                adf_config.r0.vco_adjust = 0;
                adf_config.r3.vco_bias = 5;
                
                return 0;
            }
        }
    }
    //Serial.println("VCO was locked");
    
    
    return 1;
}

int RadioAdf7012::adf_locked(void)
{
  analogReference(DEFAULT);
  analogRead(ADC6_PIN);
  int adc = analogRead(ADC6_PIN);
//  Serial.print("A6 lock: ");
  Serial.println(adc);
  delay(500);
  if (adc > 500U)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}


void RadioAdf7012::set_freq(unsigned long freq)
{ 
  
  // Set the output divider according to recommended ranges given in ADF7012 datasheet  
  // 2012-08-10 TK lowered the borders a bit in order to keep n high enough for 144, 222 and 430 MHz amateur bands 
  // with a constant crystal divider of 8
  adf_config.r0.output_divider = ADF_OUTPUT_DIVIDER_BY_1;
  if (freq < 410000000) { adf_config.r0.output_divider = ADF_OUTPUT_DIVIDER_BY_2; };
  if (freq < 210000000) { adf_config.r0.output_divider = ADF_OUTPUT_DIVIDER_BY_4; };
  if (freq < 130000000) { adf_config.r0.output_divider = ADF_OUTPUT_DIVIDER_BY_8; };
 
  unsigned long f_pfd = ADF7012_CRYSTAL_FREQ / adf_config.r0.r_divider;
  
  unsigned int n = (unsigned int)(freq / f_pfd);
  
  float ratio = (float)freq / (float)f_pfd;
  float rest  = ratio - (float)n;
  

  unsigned long m = (unsigned long)(rest * 4096); 
  


  adf_config.r1.integer_n = n;
  adf_config.r1.fractional_n = m;
/* 
  Serial.println("ADF set Freq:");
  Serial.print("n = ");
  Serial.println(n);
  Serial.print("m = ");
  Serial.println(m);
  Serial.print("f_pfd = ");
  Serial.println(f_pfd);
*/
}


void RadioAdf7012::setup()
{
  Serial.println();
  Serial.println();
  Serial.println("==============================================================");
  Serial.println("ADF7012 setup start");
  Serial.println("==============================================================");
  
//  mySerial.begin(9600);
//  mySerial.println("SS setup start");  

  pinMode(PTT_PIN, OUTPUT);
//  pinMode(TX_PA_PIN, OUTPUT); 
  pinMode(SCKpin,  OUTPUT);
  pinMode(SSpin,   OUTPUT);
  pinMode(MOSIpin, OUTPUT);
  pinMode(ADF7012_TX_DATA_PIN, OUTPUT);

#if !defined(SPI_BITBANG)  
  // Set up SPI
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(SPI_CLOCK_DIV32);
 
  // initialize SPI:
  SPI.begin();
#endif  
  
  adf_reset_config(); 
  set_freq(RADIO_FREQUENCY); // Set the default frequency
  adf_write_config();
  digitalWrite(ADF7012_TX_DATA_PIN, LOW);
//  digitalWrite(TX_PA_PIN, HIGH);  // HIGH = off => make sure the PA is off after boot.
  delay(100);
  
  Serial.println("ADF7012 setup done");

}

void RadioAdf7012::ptt_on()
{
  
  digitalWrite(PTT_PIN, HIGH);
  digitalWrite(ADF7012_TX_DATA_PIN, LOW);
  adf_config.r3.pa_enable = 0;
  adf_config.r2.power_amplifier_level = 0;
  adf_config.r3.muxout = ADF_MUXOUT_REG_READY;
  
  adf_write_config();
  delay(100);
  
  // Do we have good power on the ADF7012 voltage regulator?
  analogReference(DEFAULT);
  analogRead(ADC6_PIN);
  int adc = analogRead(ADC6_PIN);
  Serial.print("ADC6 = ");
  Serial.println(adc);
  if (adc < 500U)  // Power is bad
  {
    Serial.println("ERROR: Can't power up the ADF7012!");
  }
  else              // Power is good apparently
  {
/*
    // Do some logic checks to see if we can properly talk to the ADF7012
    adf_config.r3.muxout = ADF_MUXOUT_LOGIC_LOW;
    adf_write_config();
    delay(100);
    analogRead(ADC6_PIN);
    adc = analogRead(ADC6_PIN);
    Serial.print("MUXOUT should be LOW now. Measuring ADC6 = ");
    Serial.println(adc);

    adf_config.r3.muxout = ADF_MUXOUT_LOGIC_HIGH;
    adf_write_config();
    delay(100);
    analogRead(ADC6_PIN);
    adc = analogRead(ADC6_PIN);
    Serial.print("MUXOUT should be HIGH now. Measuring ADC6 = ");
    Serial.println(adc);
*/    
    
    //predict the exact output frequency
//    unsigned long freq_calculated = (ADF7012_CRYSTAL_FREQ / adf_config.r0.r_divider) * ((float)adf_config.r1.integer_n + ((float)adf_config.r1.fractional_n / 4096.0));
  
//    Serial.print("ADF7012 configured for ");
//    Serial.print(freq_calculated);
//    Serial.println(" Hz");
  
    if (adf_lock())
    {
      adf_config.r3.pa_enable = 1;
      adf_config.r2.power_amplifier_level = 63; //63 is max power
//      Serial.println("Turning on the PA");
      adf_write_config();
      delay(50);
      //digitalWrite(TX_PA_PIN, LOW); // Switch on the ADL5531 final PA (LOW = on)
      //delay(100);      
      // Measure HF output
      analogRead(ADC6_PIN); // blank read for equilibration
      powerlevel = analogRead(ADC6_PIN);
      
//      Serial.print("HF output ADC6 = ");
//      Serial.println(powerlevel);
      if (powerlevel > 255)
      {
        powerlevel = 255;
      }
    }
    else
    {
      
      ptt_off();
      
/*      
      // Testing with low power
      adf_config.r3.pa_enable = 1;
      adf_config.r2.power_amplifier_level = 20; //63 is max power
      Serial.println("Low power");
      adf_write_config();
      delay(100);
      // Measure HF output
      analogRead(ADC6_PIN);
      adc = analogRead(ADC6_PIN);
      Serial.print("HF output ADC6 = ");
      Serial.println(adc);
*/      
    }
    
  }
}

void RadioAdf7012::ptt_off()
{
//  divider_test();
//  change();
//  digitalWrite(TX_PA_PIN, HIGH); // Switch off the ADL5531 final PA (HIGH = off)
  adf_config.r3.pa_enable = 0;
  adf_config.r2.power_amplifier_level = 0; 
  adf_write_config();
  delay(100);
  
  digitalWrite(PTT_PIN, LOW);
  digitalWrite(ADF7012_TX_DATA_PIN, LOW);

}

int RadioAdf7012::get_powerlevel()
{
  return powerlevel;
}

/*
void RadioAdf7012::lock_test(void)
{
  analogReference(DEFAULT);
  adf_config.r3.muxout = ADF_MUXOUT_ANALOGUE_LOCK;
  adf_config.r3.pll_enable = 1;
 
  int max_a3 = 0;
  int max_divider = 0;
  int max_adjust = 0;
  int max_bias = 0;
  
  for( int divider = 1; divider < 16U; divider++)
  {
    adf_config.r0.r_divider = divider;
    set_freq(144390000UL);
    for (int adjust = 0U; adjust < 4U; adjust++)
    {
      adf_config.r0.vco_adjust = adjust;
      for (int bias = 1U; bias < 16; bias++)
      {  
        adf_config.r3.vco_bias = bias;
        adf_write_config();
        delay(1500);
        //Serial.print(".");
        digitalWrite(LED_PIN, LOW);
        delay(100);
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);


        analogRead(A3);
        if (analogRead(A3) > max_a3)
        {
           max_a3 = analogRead(A3);
           max_divider = divider;
           max_adjust = adjust;
           max_bias = bias;
        }
      }
      //Serial.println("*");      
    }
    //Serial.println("#");    
  }
  
  Serial.println();
  Serial.print("Lock @ ");
  Serial.print(max_a3);
  Serial.print(",");
  Serial.print(max_divider);
  Serial.print(",");
  Serial.print(max_adjust);
  Serial.print(",");
  Serial.println(max_bias);
 

}


void RadioAdf7012::divider_test(void)
{
  adf_config.r3.muxout = ADF_MUXOUT_RF_R_DIVIDER;
  adf_config.r3.pll_enable = 1;
  adf_config.r0.r_divider = 15;
  set_freq(144390000UL);
  adf_config.r3.clkout_enable = 1;
  adf_config.r0.clock_out_divider = 3;
  adf_config.r3.vco_bias = 6;
  adf_write_config();

}




void RadioAdf7012::change(void)
{

  mySerial.println("This is your chance to change the ADF7012 registers:");

  int timeout = 1000;
  while (timeout > 0)
  {
    if (mySerial.available())
    {
      char c = mySerial.read(); 
      if ( c == 'a' )
      {
         mySerial.println("Option a");
         timeout = 0;
      }     
      if ( c == 'b' )
      {
         mySerial.println("Option b");
         timeout = 0;
      }     
    }
    delay(10);
    timeout--;
  }
  mySerial.println("game over...");
  //mySerial.end();
}

*/

