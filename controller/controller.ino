#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include <Wire.h>      // this is needed for FT6206
#include <Adafruit_FT6206.h>

// For the Adafruit shield, these are the default.
#define TFT_DC 9
#define TFT_CS 10

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
// If using the breakout, change pins as desired
//Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);
Adafruit_FT6206 ctp = Adafruit_FT6206();
unsigned int heater_cycle_millis = 1200;
unsigned int pump_cycle_millis = 5000;
unsigned int temp_update_millis = 10000;
unsigned int sol_update_millis = 100;

unsigned int pump_percent = 0;
unsigned int heater_percent = 0;
boolean pump_on = false;
boolean heater_on = false;
unsigned long pump_transition_time = 0;
unsigned long heater_transition_time = 0;
unsigned long temp_update_time = 0;
unsigned long sol_update_time = 0;

unsigned int temp1_int = 0;
unsigned int temp1_rmndr = 0;

int pump_pin = 7;
int heater_pin = 6;
int solenoid_pin = 2;
int flow_pin = 3;
int temp_pin = 0;
int message_bytes[4];
int message_ptr = 0;

void change_pump(boolean oo) {
  if (oo) {
    digitalWrite(pump_pin, HIGH);  
  }
  else {
    digitalWrite(pump_pin, LOW);  
  }    
}

void change_heater(boolean oo) {
  if (oo) {
    digitalWrite(heater_pin, HIGH);  
  }
  else {
    digitalWrite(heater_pin, LOW);  
  }    
}

void setup() {
  Serial.begin(9600);
  pinMode(pump_pin, OUTPUT);
  pinMode(heater_pin, OUTPUT);
  pinMode(solenoid_pin, OUTPUT);  
  pinMode(flow_pin, INPUT);
  digitalWrite(solenoid_pin, LOW);

  tft.begin();
  ctp.begin(40);
  // read diagnostics (optional but can help debug problems)
  uint8_t x = tft.readcommand8(ILI9341_RDMODE);
  x = tft.readcommand8(ILI9341_RDMADCTL);
  x = tft.readcommand8(ILI9341_RDPIXFMT);
  x = tft.readcommand8(ILI9341_RDIMGFMT);
  x = tft.readcommand8(ILI9341_RDSELFDIAG);

  tft.fillScreen(ILI9341_BLACK);
  //unsigned long start = micros();
  tft.setRotation(3);
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_BLUE);  tft.setTextSize(4);
  tft.println("HEATER");
  tft.setTextColor(ILI9341_WHITE);
  tft.drawRect(0,40,20,200,ILI9341_RED);
  tft.setCursor(25, 140);
  tft.println("0%");
  tft.setCursor(35, 40);
  tft.println("OFF");  
  tft.setCursor(42, 80);
  tft.println("ON");  
  
  tft.setCursor(220, 0);
  tft.setTextColor(ILI9341_BLUE);
  tft.println("PUMP");
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(220, 40);
  tft.setTextColor(ILI9341_GREEN);
  tft.println("0%");
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(220, 80);
  tft.println("25%");
  tft.setCursor(220, 120);
  tft.println("50%");
  tft.setCursor(220, 160);
  tft.println("75%");  
  tft.setCursor(220, 200);
  tft.println("100%");

  //float temp=calc_temp();
  //int temp_1 = temp;
  //int temp_2 = int((temp-float(temp_1))*10.);
  tft.setCursor(60, 200);
  tft.println(String(temp1_int)+"."+String(temp1_rmndr)+"F");
  temp_update_time = millis()+temp_update_millis;
  sol_update_time = millis()+sol_update_millis;
  
  // Clear the serial buffer
  char message[61];
  Serial.readBytes(message, 61);
  
  return;
}

void loop(void) {
  if (ctp.touched()) {

    // Retrieve a point  
    TS_Point p = ctp.getPoint();
  
    // flip it around to match the screen.
    p.x = map(p.x, 0, 240, 240, 0);
    //p.y = map(p.y, 0, 320, 320, 0);
    int xx = p.y;
    int yy = p.x;

    // Heater slider
    if (xx <= 30 && yy >= 40) {
      tft.fillRect(25,140,100,40,ILI9341_BLACK);
      tft.setCursor(25, 140);
      heater_percent = 100-((yy-40)/2);
      tft.println(String(heater_percent)+"%");
      heater_on = false;  
      heater_transition_time = millis()+int(float(heater_cycle_millis)*(1.-float(heater_percent)/100.));
      change_heater(heater_on);      
    }
    // Heater On/Off
    else if (xx >= 35 && xx <=135 && yy >= 40 && yy <=120) {
      tft.fillRect(25,140,100,40,ILI9341_BLACK);
      if (yy >= 80) {
        heater_percent = 100;
        heater_on = true;  
        //heater_transition_time = millis()+int(float(heater_cycle_millis)*(1.-float(heater_percent)/100.));
        change_heater(heater_on);
      }
      else {
        heater_percent = 0;
        heater_on = false;  
        //heater_transition_time = millis()+int(float(heater_cycle_millis)*(1.-float(heater_percent)/100.));
        change_heater(heater_on);
      }
      tft.fillRect(25,140,80,40,ILI9341_BLACK);
      tft.setCursor(25, 140);
      tft.println(String(heater_percent)+"%");
    }
    // Pump change
    else if (xx>=220 && yy>=40) {
      int pos = (yy-40)/40;
      tft.fillRect(220,40,100,200,ILI9341_BLACK);
      pump_percent = pos*25;
      for (int ii=0; ii<5; ii++) {
        tft.setCursor(220, ii*40+40);
        if (ii==pos) {
          tft.setTextColor(ILI9341_GREEN);
        }
        else {
          tft.setTextColor(ILI9341_WHITE);
        }
        tft.println(String(ii*25)+"%");
      }
      tft.setTextColor(ILI9341_WHITE);
      if (pump_percent != 100) {
        pump_on = false;  
        pump_transition_time = millis()+int(float(pump_cycle_millis)*(1.-float(pump_percent)/100.));
        change_pump(pump_on);
      }
      else {
        // pump 100%
        pump_on = true;  
        change_pump(pump_on);
      }
    }
  } 
  if (pump_percent != 0 && pump_percent != 100) {
    if (millis() > pump_transition_time) {
      if (pump_on) {
        pump_on = false;
        change_pump(pump_on);
        pump_transition_time = millis()+int(float(pump_cycle_millis)*(1.-float(pump_percent)/100.));
      }
      else {
        pump_on = true;
        change_pump(pump_on);
        pump_transition_time = millis()+int(float(pump_cycle_millis)*float(pump_percent)/100.);
      }
    }
  }
  if (heater_percent != 0 && heater_percent != 100) {
    if (millis() > heater_transition_time) {
      if (heater_on) {
        heater_on = false;
        change_heater(heater_on);
        heater_transition_time = millis()+int(float(heater_cycle_millis)*(1.-float(heater_percent)/100.));
      }
      else {
        heater_on = true;
        change_heater(heater_on);
        heater_transition_time = millis()+int(float(heater_cycle_millis)*float(heater_percent)/100.);
      }
    }
  }
  if (millis() > temp_update_time) {
    //float temp=calc_temp();
    //int temp_1 = temp;
    //int temp_2 = int((temp-float(temp_1))*10.);
    tft.fillRect(60,200,140,40,ILI9341_BLACK);
    tft.setCursor(60, 200);
    tft.println(String(temp1_int)+"."+String(temp1_rmndr)+"F");
    temp_update_time = millis()+temp_update_millis;  
  }

  if (millis() > temp_update_time) {
    if (digitalRead(flow_pin)) {
      digitalWrite(solenoid_pin, HIGH);
    }
    else {
      digitalWrite(solenoid_pin, LOW);
    }
    sol_update_time = millis()+sol_update_millis;  
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
      if (message_bytes[1] == 5) {
        //Temperature 1
        temp1_int = message_bytes[2];
        temp1_rmndr = message_bytes[3];
      }
      // Clear message_bytes
      message_ptr = 0;
    }    
    else {
      message_bytes[message_ptr] = Serial.read();
      message_ptr += 1;
    }
  }

  

}

