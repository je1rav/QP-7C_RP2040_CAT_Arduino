/*
 * Copyright (C) 2023- Hitoshi Kawaji <je1rav@gmail.com>
 * 
 * QP-7C_RP2040_cat.ino.
 * 
 * QP-7C_RP2040_cat.ino is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 * 
 * QP-7C_RP2040_cat.ino is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// ====superheterodyne==============================
#define Superheterodyne
// =================================================

#define Si5351_CAL -22000 // Calibrate for your Si5351 module.
#define FREQ_BFO 446400   // in Hz: Calibrate for your IF Filter.

#include "Arduino.h"
#include "mbed.h"
#include "PluggableUSBAudio.h"
#include <Wire.h>
#include "hardware/adc.h"

#define AUDIOSAMPLING 48000  // USB Audio sampling frequency

#define N_FREQ 2 // number of RF frequencies selected by push switch　(<= 7)
#define FREQ_0 7041000 // RF freqency in Hz
#define FREQ_1 7074000 // in Hz
//#define FREQ_2 7074000 // in Hz
//#define FREQ_3 7074000 // in Hz
//#define FREQ_4 7074000 // in Hz
//#define FREQ_5 7074000 // in Hz
//#define FREQ_6 7074000 // in Hz
extern uint64_t Freq_table[N_FREQ]={FREQ_0,FREQ_1}; // Freq_table[N_FREQ]={FREQ_0,FREQ_1, ...}

#define pin_RX 27    //pin for RX switch (D2,output)
#define pin_TX 28    //pin for TX switch (D3,output)
#define pin_SW 3     //pin for freq change switch (D10,input)
#define pin_RED 17   //pin for Red LED (output)
#define pin_GREEN 16 //pin for GREEN LED (output)
#define pin_BLUE 25  //pin for BLUE LED (output)
#define pin_LED_POWER 11 //pin for NEOPIXEL LED power (output)
#define pin_LED 12   //pin for NEOPIXEL LED (output)

#include <si5351.h>  //Si5351 Library for Arduino, https://github.com/etherkit/Si5351Arduino
Si5351 si5351;

#include <Adafruit_NeoPixel.h>  //Adafruit NeoPixel Library, https://github.com/adafruit/Adafruit_NeoPixel
Adafruit_NeoPixel pixels(1, pin_LED);
uint8_t color = 0;
#define BRIGHTNESS 5  //max 255
uint32_t colors[] = {pixels.Color(BRIGHTNESS, 0, 0), pixels.Color(0, BRIGHTNESS, 0), pixels.Color(0, 0, BRIGHTNESS), pixels.Color(BRIGHTNESS, BRIGHTNESS, 0), pixels.Color(BRIGHTNESS, 0, BRIGHTNESS), pixels.Color(0, BRIGHTNESS, BRIGHTNESS), pixels.Color(BRIGHTNESS, BRIGHTNESS, BRIGHTNESS)};
  
uint64_t RF_freq;   // RF frequency (Hz)
#ifdef Superheterodyne
  int64_t BFO_freq = FREQ_BFO;   // BFO frequency (Hz)
#else
  int64_t BFO_freq = 0;
#endif
int C_freq = 0;  //FREQ_x: In this case, FREQ_0 is selected as the initial frequency.
int Tx_Status = 0; //0=RX, 1=TX
int Tx_Start = 0;  //0=RX, 1=TX
int not_TX_first = 0;
uint32_t Tx_last_mod_time;
uint32_t Tx_last_time;

//Audio signal frequency determination
int16_t mono_prev=0;  
int16_t mono_preprev=0;  
float delta_prev=0;
int16_t sampling=0;
int16_t cycle=0;
int32_t cycle_frequency[34];

//ADC offset for Reciever
int16_t adc_offset = 0;   

//USB Audio 
USBAudio audio(true, AUDIOSAMPLING, 2, AUDIOSAMPLING, 2);
static uint8_t readBuffer[192];  //48 sampling (=  0.5 ms at 48000 Hz sampling) data sent from PC are recived (16bit stero; 48*2*2).
int16_t writeBuffer16[96];       //48 sampling date are written to PC in one packet (stero).
uint16_t writeCounter=0;
uint16_t nBytes=0;
int16_t monodata[48];  
bool USBAudio_read;


void setup() {
  //Digital pin setting ----- 
  pinMode(A0, INPUT); //ADC input pin
  pinMode(pin_SW, INPUT_PULLUP); //SW (freq. change)
  pinMode(pin_RX, OUTPUT); //RX →　High, TX →　Low (for RX switch)
  pinMode(pin_TX, OUTPUT); //TX →　High, RX →　Low (for Driver switch)
  pinMode(pin_RED, OUTPUT); //On →　LOW (for RED LED)
  pinMode(pin_GREEN, OUTPUT); //On →　LOW (for GREEN LED)
  pinMode(pin_BLUE, OUTPUT); //On →　LOW (for BLUE LED)
  pinMode(pin_LED_POWER, OUTPUT); //NEOPIXEL LED

  //serial initialization----- 
  Serial.begin(115200); 
  Serial.setTimeout(100);

  //I2c initialization-----  
  Wire.begin();

  //ADC initialize ----- 
  adc_init();
  adc_select_input(0);                        //ADC input pin A0
  adc_set_clkdiv(249.0);                      // 192kHz sampling  (48000 / (249.0 +1) = 192)
  adc_fifo_setup(true,false,0,false,false);   // fifo
  adc_run(true);                              //ADC free running

  //si5351 initialization-----  
  int32_t cal_factor = Si5351_CAL;
  RF_freq = Freq_table[C_freq];
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0); 
  si5351.set_correction(cal_factor, SI5351_PLL_INPUT_XO);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.set_freq(RF_freq*100ULL, SI5351_CLK0);  //for TX
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  si5351.output_enable(SI5351_CLK0, 0);
  si5351.set_freq((RF_freq-BFO_freq)*100ULL, SI5351_CLK1);  //for RX
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA);
  si5351.output_enable(SI5351_CLK1, 0);
#ifdef Superheterodyne
  si5351.set_freq(BFO_freq*100ULL, SI5351_CLK2);  //for BFO
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);
  si5351.output_enable(SI5351_CLK2, 0);
#endif

  //NEOPIXEL LED  initialization-----
  digitalWrite(pin_LED_POWER, HIGH);  //NEOPIXEL LED ON
  pixels.begin();  // initialize the NEOPIXEL
  pixels.setPixelColor(0, colors[C_freq]);
  pixels.show();

  //transciver initialization-----
  si5351.output_enable(SI5351_CLK0, 0);   //TX osc. off
  si5351.output_enable(SI5351_CLK1, 1);   //RX osc. on
#ifdef Superheterodyne
  si5351.output_enable(SI5351_CLK2, 1);   
#endif
  digitalWrite(pin_TX,0);  //TX off
  digitalWrite(pin_RX,1);  //RX on
  digitalWrite(pin_RED, HIGH);
  digitalWrite(pin_GREEN, LOW); //Green LED ON
  digitalWrite(pin_BLUE, HIGH);

  //read the DC offset value of ADC input (about 0.6V)----- 
  delay(500);
  adc_fifo_drain ();
  adc_offset = adc();
}


void loop() {
  if (Tx_Start==0) receiving();
  else transmitting();
  if(Serial.available() > 0) cat();
}


void transmitting(){
  int64_t audio_freq;
  if (USBAudio_read) {
  //if (USBAudio_read_length != 0) {
    for (int i=0;i< 48 ;i++){
      int16_t mono = monodata[i];
      if ((mono_prev < 0) && (mono >= 0)) {
        if ((mono == 0) && (((float)mono_prev * 1.8 - (float)mono_preprev < 0.0) || ((float)mono_prev * 2.02 - (float)mono_preprev > 0.0))) {    //Detect the sudden drop to zero due to the end of transmission
          Tx_Start = 0;
          break;
        }
        int16_t difference = mono - mono_prev;
        // x=0付近のsin関数をテーラー展開の第1項まで(y=xで近似）
        float delta = (float)mono_prev / (float)difference;
        float period = (1.0 + delta_prev) + (float)sampling - delta;
        audio_freq = AUDIOSAMPLING*100.0/period; // in 0.01Hz    
        if ((audio_freq>20000) && (audio_freq<300000)){
          cycle_frequency[cycle]=audio_freq;
          cycle++;
        }
        delta_prev = delta;
        sampling=0;
        mono_preprev = mono_prev;
        mono_prev = mono;     
      }
      else if ((not_TX_first == 1) && (mono_prev == 0) && (mono == 0)) {        //Detect non-transmission
        Tx_Start = 0;
        break;
      }
      else {
        sampling++;
        mono_preprev = mono_prev;
        mono_prev = mono;
      }
    }
    if (Tx_Start == 0){
      cycle = 0;
      receive();
      return;
    }
    if ((cycle > 0) && (millis() - Tx_last_mod_time > 5)){          //inhibit the frequency change faster than 5mS
      audio_freq = 0;
      for (int i=0;i<cycle;i++){
        audio_freq += cycle_frequency[i];
      }
      audio_freq = audio_freq / cycle;
      transmit(audio_freq);
      cycle = 0;
      Tx_last_mod_time = millis();
    }
    not_TX_first = 1;
    Tx_last_time = millis();
  }
  else if (millis()-Tx_last_time > 50) {     // If USBaudio data is not received for more than 50 ms during transmission, the system moves to receive. 
    Tx_Start = 0;
    cycle = 0;
    receive();
    return;
  }  
  USBAudioRead();
}

void receiving() {
  USBAudioRead();  // read in the USB Audio buffer (myRawBuffer) to check the transmitting
  if (USBAudio_read) 
  {
    uint16_t monomax = 0;
    for (int i=0; i<48; i++){
      if (abs(monodata[i]) > monomax) monomax = abs(monodata[i]);
    }
    if (monomax > 22938)
    {                       // VOX if singanl exceeds 70% of 2^15
      Tx_Start = 1;
      not_TX_first = 0;
      return;
    }
  }
  freqChange();
  int16_t rx_adc = adc() - adc_offset; //read ADC data (8kHz sampling)
  // write the same 6 stereo data to PC for 48kHz sampling (up-sampling: 8kHz x 6 = 48 kHz)
  for (int i=0;i<6;i++){
    USBAudioWrite(rx_adc, rx_adc);
  }
}

void transmit(int64_t freq){
  if (Tx_Status==0){
    digitalWrite(pin_RX,0);   //RX off
    digitalWrite(pin_TX,1);   //TX on
    si5351.output_enable(SI5351_CLK1, 0);   //RX osc. off
  #ifdef Superheterodyne
    si5351.output_enable(SI5351_CLK2, 0);   //BFO osc. off
  #endif
    si5351.output_enable(SI5351_CLK0, 1);   //TX osc. on
    Tx_Status=1;
    digitalWrite(pin_RED, 0);
    digitalWrite(pin_GREEN, 1);
    //digitalWrite(pin_BLUE, 1);
    adc_run(false);                         //stop ADC free running
  }
  si5351.set_freq((RF_freq*100 + freq), SI5351_CLK0);  
}

void receive(){
  digitalWrite(pin_TX,0);  //TX off
  digitalWrite(pin_RX,1);  //RX on
  si5351.output_enable(SI5351_CLK0, 0);   //TX osc. off
  si5351.set_freq((RF_freq-BFO_freq)*100ULL, SI5351_CLK1);
  si5351.output_enable(SI5351_CLK1, 1);   //RX osc. on
#ifdef Superheterodyne
  si5351.output_enable(SI5351_CLK2, 1);   //BFO osc. on
#endif
  Tx_Status=0;
  digitalWrite(pin_RED, 1);
  digitalWrite(pin_GREEN, 0);
  //digitalWrite(pin_BLUE, 1);

  // initialization of monodata[]
  for (int i = 0; i < 48; i++) {
    monodata[i] = 0;
  } 
  // initializaztion of ADC data write counter
  writeCounter=0;
  nBytes=0;
  adc_fifo_drain ();                     //initialization of adc fifo
  adc_run(true);                         //start ADC free running
}

void freqChange(){
  if (digitalRead(pin_SW)==LOW){
    delay(100);
    if  (digitalRead(pin_SW)==LOW){
      C_freq++;
      if  (C_freq >= N_FREQ){
        C_freq = 0;
      }
    }
    RF_freq = Freq_table[C_freq];
    si5351.set_freq((RF_freq-BFO_freq)*100ULL, SI5351_CLK1);
    //NEOPIXEL LED change
    pixels.setPixelColor(0, colors[C_freq]);
    pixels.show();
    delay(100);
    adc_fifo_drain ();
    adc_offset = adc();
  }
}

int16_t adc() {
  int32_t adc = 0;
  for (int i=0;i<24;i++){                    // 192kHz/24 = 8kHz
  #ifdef Superheterodyne
    adc += adc_fifo_get_blocking() -1862 ;   // read from ADC fifo (offset about 1.5 V: DET OUT)
  #else
    adc += adc_fifo_get_blocking() -745 ;    // read from ADC fifo (offset about 0.6 V: AM MIX OUT)
  #endif
  }  
  int16_t devision = 1;
  return (int16_t)(adc/devision);    // if the audio input is too large, please reduce the adc output with increasing the "devision".
}

void USBAudioRead() {
  USBAudio_read = audio.read(readBuffer, sizeof(readBuffer));
  int32_t monosum=0;
  if (USBAudio_read) {
    for (int i = 0; i < 48 ; i++) {
      int16_t outL = (readBuffer[4*i]) + (readBuffer[4*i+1] << 8);
      int16_t outR = (readBuffer[4*i+2]) + (readBuffer[4*i+3] << 8);
      int16_t mono = (outL+outR)/2;
      monosum += mono;
      monodata[i] = mono;
    }
  }
  if (monosum == 0) USBAudio_read=0;
}

void USBAudioWrite(int16_t left,int16_t right) {
  if(nBytes>191){
    uint8_t *writeBuffer =  (uint8_t *)writeBuffer16;
    audio.write(writeBuffer, 192);
    writeCounter =0;
    nBytes =0;
  }
  writeBuffer16[writeCounter]=left;
  writeCounter++;
  nBytes+=2;
  writeBuffer16[writeCounter]=right;
  writeCounter++;
  nBytes+=2;
}

//remote contol (simulating TS-2000)
//Original: "ft8qrp_cat11.ico" https://www.elektronik-labor.de/HF/FT8QRP.html
//modified slightly
void cat(void) {     
  char received;
  String receivedPart1;
  String receivedPart2;    
  String command;
  String command2;  
  String parameter;
  String parameter2; 
  String sent;
  String sent2;
  String data = "";
  bool read_loop = 1;
  long int freq = (long int)RF_freq;
  int bufferIndex = 0;
  
  while(Serial.available() > 0){    
    received = Serial.read();  
    if (received != ';') {
      if('a' <= received && received <= 'z'){
        received = received - ('a' - 'A');
      }
      if(('A' <= received && received <= 'Z') || ('0' <= received && received <= '9')){
        data += received;
      }
    }
    else { 
      if (bufferIndex == 0) {        
        data += '\0';
        receivedPart1 = data;
        data = "";
        bufferIndex ++;
      }
      else {
        data += '\0';
        receivedPart2 = data;
        data = "";
        bufferIndex ++;
      }
    }
  }     

  command = receivedPart1.substring(0,2);
  command2 = receivedPart2.substring(0,2);    
  parameter = receivedPart1.substring(2,receivedPart1.length());
  parameter2 = receivedPart2.substring(2,receivedPart2.length());

  if (command == "FA")  {          
    if (parameter != "")  {          
      long int freqset = parameter.toInt();
      if (freqset >= 1000000 && freqset <= 54000000) freq = freqset;
        RF_freq=(uint64_t)freq;
        si5351.set_freq((RF_freq-BFO_freq)*100ULL, SI5351_CLK1);  //for RX
        digitalWrite(pin_LED_POWER, LOW);  //NEOPIXEL LED OFF        
        delay(200);
        adc_fifo_drain ();
        adc_offset = adc();
    }          
    sent = "FA" // Return 11 digit frequency in Hz.  
    + String("00000000000").substring(0,11-(String(freq).length()))   
    + String(freq) + ";";     
  }
  else if (command == "FB")  {          
    if (parameter != "")  {          
      long int freqset = parameter.toInt();
      if (freqset >= 1000000 && freqset <= 54000000) freq = freqset;
        RF_freq=(uint64_t)freq;
        si5351.set_freq((RF_freq-BFO_freq)*100ULL, SI5351_CLK1);  //for RX
        digitalWrite(pin_LED_POWER, LOW);  //NEOPIXEL LED OFF        
        delay(200);
        adc_fifo_drain ();
        adc_offset = adc();
    }          
    sent = "FB" // Return 11 digit frequency in Hz.  
    + String("00000000000").substring(0,11-(String(freq).length()))   
    + String(freq) + ";";     
  }  
  else if (command == "IF")  {          
    sent = "IF" // Return 11 digit frequency in Hz.  
    + String("00000000000").substring(0,11-(String((long int)freq).length()))   
    + String((long int)freq) + "0001+00000" + "00000" + String(Tx_Status).substring(0,1) + "20000000;";     //USB  
  }
  else if (command == "MD")  {          
    sent = "MD2;";                     //USB  
  }
  else  if (command == "ID")  {  
    sent = "ID019;";
  }
  else  if (command == "PS")  {  
    sent = "PS1;";
  }
  else  if (command == "AI")  {  
    sent = "AI0;";
  }
  else  if (command == "RX")  {  
    sent = "RX0;";
  }
  else  if (command == "TX")  {  
    sent = "TX0;";
  }
  else  if (command == "AG")  {  
    sent = "AG0000;";
  }
  else  if (command == "XT")  {  
    sent = "XT0;";
  }
  else  if (command == "RT")  {  
    sent = "RT0;";
  }
  else  if (command == "RC")  {  
    sent = ";";
  }
  else  if (command == "RS")  {  
    sent = "RS0;";
  }
  else  if (command == "VX")  {  
    sent = "VX0;";
  }
  else  if (command == "SA")  {  
    sent = "SA000000000000000;";
  }
  //else  {
  //  sent = String("?;");
  //}
//------------------------------------------------------------------------------ 
  if (command2 == "ID")   {  
    sent2 = "ID019;";
  }
  //else  {
  //  sent2 = String("?;");
  //}               
  Serial.print(sent);
  if (bufferIndex == 2)  {
    Serial.print(sent2);
  }        
}

//checking for the prevention of out-of-band transmission (in JA)
int freqcheck(long int frequency)  // retern 1=out-of-band, 0=in-band
{
  if (frequency < 135700) {
    return 1;
  }
  else if (frequency > 135800 && frequency < 472000) {
    return 1;
  }
  else if (frequency > 479000 && frequency < 1800000) {
    return 1;
  }
  else if (frequency > 1875000 && frequency < 1907500) {
    return 1;
  }
  else if (frequency > 1912500 && frequency < 3500000) {
    return 1;
  }
  else if (frequency > 3580000 && frequency < 3662000) {
    return 1;
  }
  else if (frequency > 3687000 && frequency < 3716000) {
    return 1;
  }
  else if (frequency > 3770000 && frequency < 3791000) {
    return 1;
  }
  else if (frequency > 3805000 && frequency < 7000000) {
    return 1;
  }
  else if (frequency > 7200000 && frequency < 10100000) {
    return 1;
  }
  else if (frequency > 10150000 && frequency < 14000000) {
    return 1;
  }
  else if (frequency > 14350000 && frequency < 18068000) {
    return 1;
  }
  else if (frequency > 18168000 && frequency < 21000000) {
    return 1;
  }
  else if (frequency > 21450000 && frequency < 24890000) {
    return 1;
  }
  else if (frequency > 24990000 && frequency < 28000000) {
    return 1;
  }
  else if (frequency > 29700000 && frequency < 50000000) {
    return 1;
  }
  else if (frequency > 54000000) {
    return 1;
  }
  else return 0;
}
