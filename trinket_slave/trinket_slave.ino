/***************************************************************************
* File Name: serial_MAX31865.h
* Processor/Platform: Arduino Uno R3 (tested)
* Development Environment: Arduino 1.0.5
*
* Designed for use with with Playing With Fusion MAX31865 Resistance
* Temperature Device (RTD) breakout board: SEN-30202 (PT100 or PT1000)
*   ---> http://playingwithfusion.com/productview.php?pdid=25
*   ---> http://playingwithfusion.com/productview.php?pdid=26
*
* Copyright © 2014 Playing With Fusion, Inc.
* SOFTWARE LICENSE AGREEMENT: This code is released under the MIT License. 
* 
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
* DEALINGS IN THE SOFTWARE.
* **************************************************************************
* REVISION HISTORY:
* Author			Date		Comments
* J. Steinlage			2014Feb17	Original version
* J. Steinlage                  2014Aug07       Reduced SPI clock to 1MHz - fixed occasional missing bit
*                                               Fixed temp calc to give only unsigned resistance values
*                                               Removed unused #defines for chip config (they're hard coded) 
*
* Playing With Fusion, Inc. invests time and resources developing open-source
* code. Please support Playing With Fusion and continued open-source 
* development by buying products from Playing With Fusion!
*
* **************************************************************************
* ADDITIONAL NOTES:
* This file configures then runs a program on an Arduino Uno to read a 2-ch
* MAX31865 RTD-to-digital converter breakout board and print results to
* a serial port. Communication is via SPI built-in library.
*    - Configure Arduino Uno
*    - Configure and read resistances and statuses from MAX31865 IC 
*      - Write config registers (MAX31865 starts up in a low-power state)
*      - RTD resistance register
*      - High and low status thresholds 
*      - Fault statuses
*    - Write formatted information to serial port
*  Circuit:
*    Arduino Uno   Arduino Mega  -->  SEN-30201
*    CS0: pin  9   CS0: pin  9   -->  CS, CH0
*    CS1: pin 10   CS1: pin 10   -->  CS, CH1
*    MOSI: pin 11  MOSI: pin 51  -->  SDI (must not be changed for hardware SPI)
*    MISO: pin 12  MISO: pin 50  -->  SDO (must not be changed for hardware SPI)
*    SCK:  pin 13  SCK:  pin 52  -->  SCLK (must not be changed for hardware SPI)
*    GND           GND           -->  GND
*    5V            5V            -->  Vin (supply with same voltage as Arduino I/O, 5V)
***************************************************************************/

// the sensor communicates using SPI, so include the hardware SPI library:
#include <SPI.h>
#include <PID_v1.h>

// include Playing With Fusion MAX31865 libraries
#include <PlayingWithFusion_MAX31865.h>              // core library
#include <PlayingWithFusion_MAX31865_STRUCT.h>       // struct library
// CS pin used for the connection with the sensor
// other connections are controlled by the SPI library)

const int CS0_PIN = 9;
const int CS1_PIN = 10;

PWFusion_MAX31865_RTD rtd_ch0(CS0_PIN);
PWFusion_MAX31865_RTD rtd_ch1(CS1_PIN);

struct float_data {
  // To convert to float, (int_part - 100) + dec_part*10
  byte int_part;
  byte dec_part;
};

float float_data_to_float(struct float_data fd) {
  return float(fd.int_part)-100. + float(fd.dec_part)/10.;
}

float float_data_to_float(int int_part, int dec_part) {
  return float(int_part)-100. + float(dec_part)/10.;
}

float_data float_to_float_data(float f) {
  float_data fd;
  fd.int_part = byte(f+100.);
  fd.dec_part = byte((f+100.-float(fd.int_part))*10.);
  return fd;
}

unsigned int heater_cycle_millis = 1200;
float_data heater1_percent = {0+100, 0};
boolean heater1_on = false;
unsigned long heater1_transition_time = 0;
int heater1_pin = A5;
bool heater1_pid_mode = false;
double heater1_target_temp = 0.0;

float_data heater2_percent = {0+100, 0};
boolean heater2_on = false;
unsigned long heater2_transition_time = 0;
int heater2_pin = A4;
bool heater2_pid_mode = false;
double heater2_target_temp = 0.0;


float_data temp1;
float_data temp2;
double pid1_temp;
double pid1_output;
double pid2_temp;
double pid2_output;

unsigned int temp_update_millis = 1000;
unsigned long temp_update_time = 0;

byte strt_mark = 255;

int message_bytes[4];
int message_ptr = 0;

PID pid1(&pid1_temp, &pid1_output, &heater1_target_temp, 100./3., 1./300., 0.0, DIRECT);
PID pid2(&pid2_temp, &pid2_output, &heater2_target_temp, 50./3.,  1./300., 0.0, DIRECT);

void change_heater1(boolean oo) {
  if (oo) {
    digitalWrite(heater1_pin, HIGH);  
  }
  else {
    digitalWrite(heater1_pin, LOW);  
  }    
}

void change_heater2(boolean oo) {
  if (oo) {
    digitalWrite(heater2_pin, HIGH);  
  }
  else {
    digitalWrite(heater2_pin, LOW);  
  }    
}

void setup() {
  Serial.begin(9600);
  pinMode(heater1_pin, OUTPUT);
  pinMode(heater2_pin, OUTPUT);
  temp_update_time = millis()+temp_update_millis;

  // setup for the the SPI library:
  SPI.begin();                            // begin SPI
  SPI.setClockDivider(SPI_CLOCK_DIV16);   // SPI speed to SPI_CLOCK_DIV16 (1MHz)
  SPI.setDataMode(SPI_MODE3);             // MAX31865 works in MODE1 or MODE3
  
  // initalize the chip select pin
  pinMode(CS0_PIN, OUTPUT);
  pinMode(CS1_PIN, OUTPUT);
  rtd_ch0.MAX31865_config();
  rtd_ch1.MAX31865_config();

  pid1.SetMode(MANUAL);
  pid2.SetMode(MANUAL);
  pid1.SetSampleTime(temp_update_millis-1);
  pid2.SetSampleTime(temp_update_millis-1);
  // PID 1 controls the boil element
  pid1.SetOutputLimits(0, 100);

  // PID 2 controls the RIMS element
  pid2.SetOutputLimits(0, 50);
 
  // Clear the serial buffer
  char message[61];
  Serial.readBytes(message, 61);
  
  // give the sensor time to set up
  delay(1000);
}

void loop() {
  if ((heater1_percent.int_part-100 != 0 || heater1_percent.dec_part != 0 ) && heater1_percent.int_part-100 != 100) {
    if (millis() > heater1_transition_time) {
      if (heater1_on) {
        heater1_on = false;
        change_heater1(heater1_on);
        heater1_transition_time = millis()+int(float(heater_cycle_millis)*(1.-float_data_to_float(heater1_percent)/100.));
      }
      else {
        heater1_on = true;
        change_heater1(heater1_on);
        heater1_transition_time = millis()+int(float(heater_cycle_millis)*float_data_to_float(heater1_percent)/100.);
      }
    }
  }

  if ((heater2_percent.int_part-100 != 0 || heater2_percent.dec_part != 0 ) && heater2_percent.int_part-100 != 100) {
    if (millis() > heater2_transition_time) {
      if (heater2_on) {
        heater2_on = false;
        change_heater2(heater2_on);
        heater2_transition_time = millis()+int(float(heater_cycle_millis)*(1.-float_data_to_float(heater2_percent)/100.));
      }
      else {
        heater2_on = true;
        change_heater2(heater2_on);
        heater2_transition_time = millis()+int(float(heater_cycle_millis)*float_data_to_float(heater2_percent)/100.);
      }
    }
  }

  if (millis() > temp_update_time) {

    temp_update_time = millis()+temp_update_millis;  

    static struct var_max31865 RTD_CH0;
    static struct var_max31865 RTD_CH1;

    RTD_CH0.RTD_type = 1;                         // un-comment for PT100 RTD
  //   RTD_CH0.RTD_type = 2;                        // un-comment for PT1000 RTD
    RTD_CH1.RTD_type = 1;                         // un-comment for PT100 RTD
  // RTD_CH0.RTD_type = 2;                        // un-comment for PT1000 RTD
  
  struct var_max31865 *rtd_ptr;
  rtd_ptr = &RTD_CH0;
  rtd_ch0.MAX31865_full_read(rtd_ptr);          // Update MAX31855 readings 
  
  rtd_ptr = &RTD_CH1;
  rtd_ch1.MAX31865_full_read(rtd_ptr);          // Update MAX31855 readings 

    double temp1_f;
    if(0 == RTD_CH0.status)                       // no fault, print info to serial port
    {
      // calculate RTD temperature (simple calc, +/- 2 deg C from -100C to 100C)
      // more accurate curve can be used outside that range
      temp1_f = ((double)RTD_CH0.rtd_res_raw / 32) - 256;
    }  // end of no-fault handling
    else 
    {
      if(0x80 & RTD_CH0.status)
      {
	temp1_f = -99.0;
      }
      else if(0x40 & RTD_CH0.status)
      {
	temp1_f = -98.0;
      }
      else if(0x20 & RTD_CH0.status)
      {
	temp1_f = -97.0;
      }
      else if(0x10 & RTD_CH0.status)
      {
	temp1_f = -96.0;
      }
      else if(0x08 & RTD_CH0.status)
      {
	temp1_f = -95.0;
      }
      else if(0x04 & RTD_CH0.status)
      {
	temp1_f = -94.0;
      }
      else
      {
	temp1_f = -93.0;
      }
    }  // end of fault handling
    temp1 = float_to_float_data(temp1_f);
    Serial.write(strt_mark);
    // Temperature 1
    byte temp1_mark = 5;
    Serial.write(temp1_mark);
    Serial.write(temp1.int_part);
    Serial.write(temp1.dec_part);

    Serial.write(strt_mark);
    // Percent 1
    byte percent1_mark = 6;
    Serial.write(percent1_mark);
    Serial.write(heater1_percent.int_part);
    Serial.write(heater1_percent.dec_part);

    double temp2_f;
    if(0 == RTD_CH1.status)                       // no fault, print info to serial port
    {
      // calculate RTD temperature (simple calc, +/- 2 deg C from -100C to 100C)
      // more accurate curve can be used outside that range
      temp2_f = ((double)RTD_CH1.rtd_res_raw / 32) - 256;
    }  // end of no-fault handling
    else 
    {
      if(0x80 & RTD_CH1.status)
      {
	temp2_f = -99.0;
      }
      else if(0x40 & RTD_CH1.status)
      {
	temp2_f = -98.0;
      }
      else if(0x20 & RTD_CH1.status)
      {
	temp2_f = -97.0;
      }
      else if(0x10 & RTD_CH1.status)
      {
	temp2_f = -96.0;
      }
      else if(0x08 & RTD_CH1.status)
      {
	temp2_f = -95.0;
      }
      else if(0x04 & RTD_CH1.status)
      {
	temp2_f = -94.0;
      }
      else
      {
	temp2_f = -93.0;
      }
    }  // end of fault handling

    temp2 = float_to_float_data(temp2_f);
    Serial.write(strt_mark);
    // Temperature 2
    byte temp2_mark = 7;
    Serial.write(temp2_mark);
    Serial.write(temp2.int_part);
    Serial.write(temp2.dec_part);

    Serial.write(strt_mark);
    // Percent 2
    byte percent2_mark = 8;
    Serial.write(percent2_mark);
    Serial.write(heater2_percent.int_part);
    Serial.write(heater2_percent.dec_part);

    // PID handling
    //PID pid1(&pid1_temp, &pid1_output, &heater1_target_temp, 2,5,1, DIRECT); 
    if (heater1_pid_mode) {
      pid1_temp = temp1_f;
      pid1.Compute();
      heater1_percent = float_to_float_data(pid1_output);
    }
    else if (heater2_pid_mode) {
      pid2_temp = temp2_f;
      pid2.Compute();
      heater2_percent = float_to_float_data(pid2_output);
    }
  }

  while (Serial.available() > 0) {
    if (message_ptr == 0 && Serial.read() == 255) {
      message_bytes[message_ptr] = 255;
      message_ptr += 1;
    }
    else if (message_ptr == 3) {
      message_bytes[message_ptr] = Serial.read();
      message_ptr += 1;
      // Handle the messages here
      if (message_bytes[1] == 1) {
	// Set heater1 PID temperature
	// Make sure heater 2 is off
	heater2_on = false;
	heater2_percent.int_part = 100;
	heater2_percent.dec_part = 0;
	change_heater2(heater2_on);
	heater2_pid_mode = false;
	pid2.SetMode(MANUAL);

	heater1_target_temp = float_data_to_float(message_bytes[2], message_bytes[3]);
	heater1_pid_mode = true;
	pid2.SetMode(AUTOMATIC);
	heater1_percent.int_part = 100;
	heater1_percent.dec_part = 0;
	heater1_on = false;
	change_heater1(heater1_on);

      }
      else if (message_bytes[1] == 2) {
	// Set heater1 percentage
	// Make sure heater 1 is off
	heater2_on = false;
	heater2_percent.int_part = 100;
	heater2_percent.dec_part = 0;
	change_heater2(heater2_on);
	heater2_pid_mode = false;
	heater1_pid_mode = false;
	pid1.SetMode(MANUAL);
	pid2.SetMode(MANUAL);
        heater1_percent.int_part = message_bytes[2];
        heater1_percent.dec_part = message_bytes[3];
	if (heater1_percent.int_part-100 == 0 and heater1_percent.dec_part==0) {
	  heater1_on = false;
	  change_heater1(heater1_on);
	}
	else if (heater1_percent.int_part-100 >= 100) {
	  heater1_percent.int_part = 100+100;
	  heater1_percent.dec_part = 0;
	  heater1_on = true;
	  change_heater1(heater1_on);
	}
      }
      else if (message_bytes[1] == 3) {
	// Set heater2 PID temperature
	// Make sure heater 1 is off
	heater1_on = false;
	heater1_percent.int_part = 100;
	heater1_percent.dec_part = 0;
	change_heater1(heater1_on);
	heater1_pid_mode = false;
	pid1.SetMode(MANUAL);

	heater2_target_temp = float_data_to_float(message_bytes[2], message_bytes[3]);
	heater2_pid_mode = true;
	pid2.SetMode(AUTOMATIC);
	
	heater2_percent.int_part = 100;
	heater2_percent.dec_part = 0;
	heater2_on = false;
	change_heater2(heater2_on);
      }
      else if (message_bytes[1] == 4) {
	// Set heater2 percentage
	// Make sure heater 1 is off
	heater1_on = false;
	heater1_percent.int_part = 100;
	heater2_percent.dec_part = 0;
	change_heater1(heater1_on);
	heater1_pid_mode = false;
	heater2_pid_mode = false;
	pid1.SetMode(MANUAL);
	pid2.SetMode(MANUAL);
        heater2_percent.int_part = message_bytes[2];
        heater2_percent.dec_part = message_bytes[3];
	if (heater2_percent.int_part-100 == 0 and heater2_percent.dec_part==0) {
	  heater2_on = false;
	  change_heater2(heater2_on);
	}
	else if (heater2_percent.int_part-100 >= 100) {
	  heater2_percent.int_part = 100+100;
	  heater2_percent.dec_part = 0;
	  heater2_on = true;
	  change_heater2(heater2_on);
	}
      }
      // Clear message_bytes
      message_ptr = 0;
    }    

    else {
      message_bytes[message_ptr] = Serial.read();
      message_ptr += 1;
    }
  }
  delay(10);
}

