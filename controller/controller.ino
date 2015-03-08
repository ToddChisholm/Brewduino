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
unsigned int pump_cycle_millis = 5000;
unsigned int sol_update_millis = 100;
unsigned int flow_update_millis = 2000;

unsigned int pump_percent = 0;
boolean pump_on = false;
unsigned long pump_transition_time = 0;
unsigned long sol_update_time = 0;
unsigned long flow_update_time = 0;

volatile unsigned long int flow_count = 0;

int pump_pin = 7;
int solenoid_pin = 4;
int wtr_rqst_pin = 3;
int flow_pin = 2;

int message_bytes[4];
int message_ptr = 0;

struct float_data {
  // To convert to float, (int_part - 100) + dec_part*10
  byte int_part;
  byte dec_part;
};

float_data temp1 = {0+100, 0};
float_data temp2 = {0+100, 0};
float_data heater1_percent = {0+100,0};

byte strt_mark = 255;

float float_data_to_float(struct float_data fd) {
  return float(fd.int_part)-100. + float(fd.dec_part)/10.;
}

float_data float_to_float_data(float f) {
  float_data fd;
  fd.int_part = byte(f+100.);
  fd.dec_part = byte((f+100.-float(fd.int_part))*10.);
  return fd;
}

void send_heater1_percent(float percent) {
  // Also sets the current heater1_percent
  heater1_percent = float_to_float_data(percent);
  Serial1.write(strt_mark);
  // Temperature 1 percentage
  byte heater1_percent_mark = 2;
  Serial1.write(heater1_percent_mark);
  Serial1.write(heater1_percent.int_part);
  Serial1.write(heater1_percent.dec_part);
}

void change_pump(boolean oo) {
  if (oo) {
    digitalWrite(pump_pin, HIGH);  
  }
  else {
    digitalWrite(pump_pin, LOW);  
  }    
}

void service_flow_pin() {
  flow_count += 1;
}

void display_temp1() {
  tft.fillRect(60,200,100,40,ILI9341_BLACK);
  tft.setCursor(60, 200);
  tft.println(String(int(temp1.int_part)-100)+"."+String(temp1.dec_part)+"C");
}
void display_temp2() {
  //tft.fillRect(60,200,100,40,ILI9341_BLACK);
  //tft.setCursor(60, 200);
  //tft.println(String(int(temp1.int_part)-100)+"."+String(temp1.dec_part)+"C");
}

void display_heater1() {
  tft.fillRect(25,140,100,40,ILI9341_BLACK);
  tft.setCursor(25, 140);
  tft.println(String(int(heater1_percent.int_part)-100)+"."+String(heater1_percent.dec_part));
}

void setup() {
  Serial1.begin(9600);
  pinMode(pump_pin, OUTPUT);
  pinMode(solenoid_pin, OUTPUT);  
  pinMode(wtr_rqst_pin, INPUT);
  digitalWrite(wtr_rqst_pin, HIGH);
  
  pinMode(flow_pin, INPUT);
  digitalWrite(solenoid_pin, LOW);
  attachInterrupt(0, service_flow_pin, RISING);
  tft.begin();
  ctp.begin(40);
  // read diagnostics (optional but can help debug problems)
  //uint8_t x = tft.readcommand8(ILI9341_RDMODE);
  //x = tft.readcommand8(ILI9341_RDMADCTL);
  //x = tft.readcommand8(ILI9341_RDPIXFMT);
  //x = tft.readcommand8(ILI9341_RDIMGFMT);
  //x = tft.readcommand8(ILI9341_RDSELFDIAG);

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

  display_temp1();
  display_temp2();

  sol_update_time = millis()+sol_update_millis;
  flow_update_time = millis()+flow_update_millis;
  
  // Clear the serial buffer
  char message[61];
  Serial1.readBytes(message, 61);

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
      float heater_percent_f = 100-((yy-40)/2);
      send_heater1_percent(heater_percent_f);
      display_heater1();
    }
    // Heater On/Off
    else if (xx >= 35 && xx <=135 && yy >= 40 && yy <=120) {
      tft.fillRect(25,140,100,40,ILI9341_BLACK);
      tft.fillRect(25,140,80,40,ILI9341_BLACK);
      tft.setCursor(25, 140);
      if (yy >= 80) {
        send_heater1_percent(100.0);
      }
      else {
        send_heater1_percent(0.0);
      }
      display_heater1();
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

  if (millis() > flow_update_time) {
    tft.fillRect(60,160,140,40,ILI9341_BLACK);
    tft.setCursor(60, 160);
    tft.println(flow_count/10);
    flow_update_time = millis()+flow_update_millis;  
  }

  if (millis() > sol_update_time) {
    if (digitalRead(wtr_rqst_pin) == LOW) {
      digitalWrite(solenoid_pin, HIGH);
    }
    else {
      digitalWrite(solenoid_pin, LOW);
    }
    sol_update_time = millis()+sol_update_millis;  
  }

  // Handle input from the trinket
  while (Serial1.available() > 0) {
    if (message_ptr == 0 && Serial1.read() == 255) {
      message_bytes[message_ptr] = 255;
      message_ptr += 1;
    }
    else if (message_ptr == 3) {
      message_bytes[message_ptr] = Serial1.read();
      message_ptr += 1;
      // Handle the messages here

      if (message_bytes[1] == 5) {
        // Receive temperature 1
	if (message_bytes[2] != temp1.int_part && message_bytes[3] != temp1.dec_part) {
	  temp1.int_part = message_bytes[2];
	  temp1.dec_part = message_bytes[3];
	  display_temp1();
	}
      }
      else if (message_bytes[1] == 6) {
        // Receive heater1 percentage
	if (message_bytes[2] != heater1_percent.int_part && message_bytes[3] != heater1_percent.dec_part) {
	  heater1_percent.int_part = message_bytes[2];
	  heater1_percent.dec_part = message_bytes[3];
	  display_heater1();
	}
      }
      if (message_bytes[1] == 7) {
        // Receive temperature 2
	if (message_bytes[2] != temp2.int_part && message_bytes[3] != temp2.dec_part) {
	  temp2.int_part = message_bytes[2];
	  temp2.dec_part = message_bytes[3];
	  display_temp2();
	}
      }
      // Clear message_bytes
      message_ptr = 0;
    }    
    else {
      message_bytes[message_ptr] = Serial1.read();
      message_ptr += 1;
    }
  }
}
