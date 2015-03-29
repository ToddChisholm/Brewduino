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
unsigned int flow_update_millis = 1000;

unsigned int pump_percent = 0;
boolean pump_on = false;
unsigned long pump_transition_time = 0;
unsigned long sol_update_time = 0;
unsigned long flow_update_time = 0;

volatile unsigned long int flow_count = 0;
unsigned long int prev_flow_count = 0;

// Factor to convert flow count to gallons
float flow_factor = .001;

int pump_pin = 7;
int solenoid_pin = 4;
int wtr_rqst_pin = 3;
int flow_pin = 2;

int message_bytes[4];
int message_ptr = 0;
char itoabuf[6];
struct float_data {
  // To convert to float, (int_part - 100) + dec_part*10
  byte int_part;
  byte dec_part;
};

float_data temp1 = {0+100, 0};
float_data temp2 = {0+100, 0};
float_data heater1_percent = {0+100,0};
float_data heater2_percent = {0+100,0};

enum MenuPages { TOP, BOIL, PUMPACROSS, MASH, PUMPOUT, ADDWATER};

MenuPages menu = TOP;

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

void send_heater2_percent(float percent) {
  // Also sets the current heater2_percent
  heater2_percent = float_to_float_data(percent);
  Serial1.write(strt_mark);
  // Temperature 2 percentage
  byte heater2_percent_mark = 4;
  Serial1.write(heater2_percent_mark);
  Serial1.write(heater2_percent.int_part);
  Serial1.write(heater2_percent.dec_part);
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

// Each of the display routines is responsible for checking the menu
void display_temp1() {
  if (menu == BOIL) {
    tft.fillRect(60,200,100,40,ILI9341_BLACK);
    tft.setCursor(60, 200);
    tft.println(String(int(temp1.int_part)-100)+"."+String(temp1.dec_part)+"C");
  }
}
void display_temp2() {
  if (menu == PUMPACROSS || menu == MASH) {
    tft.fillRect(60,200,100,40,ILI9341_BLACK);
    tft.setCursor(60, 200);
    tft.println(String(int(temp2.int_part)-100)+"."+String(temp2.dec_part)+"C");
  }
}

void display_heater1() {
  if (menu == BOIL) {
    tft.fillRect(25,140,120,40,ILI9341_BLACK);
    tft.setCursor(25, 140);
    tft.println(String(int(heater1_percent.int_part)-100)+"."+String(heater1_percent.dec_part));
  }
}
void display_heater2() {
  if (menu == PUMPACROSS || menu == MASH) {
    tft.fillRect(25,140,120,40,ILI9341_BLACK);
    tft.setCursor(25, 140);
    tft.println(String(int(heater2_percent.int_part)-100)+"."+String(heater2_percent.dec_part));
  }
}
void display_water_count(bool force) {
  if ( force || (menu==ADDWATER && prev_flow_count != flow_count) ) {
    tft.fillRect(110,150,125,40,ILI9341_BLACK);
    tft.setCursor(110, 150);
    float gallons = float(flow_count)*flow_factor;
    int gallons_int_part = int(gallons);
    int gallons_dec_part1 = int((gallons-float(gallons_int_part))*10.);
    int gallons_dec_part2 = int((gallons-float(gallons_dec_part1)/10.-float(gallons_int_part))*100.);
    prev_flow_count = flow_count;
    tft.println(String(gallons_int_part)+"."+String(gallons_dec_part1)+String(gallons_dec_part2)+"G");
  }
}

void prepare_top_menu() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(4);
  tft.setCursor(0, 0);
  tft.println("BOIL");  
  tft.setCursor(0, 45);
  tft.println("PUMP TO BOIL");  
  tft.setCursor(0, 90);
  tft.println("MASH");  
  tft.setCursor(0, 135);
  tft.println("PUMP OUT");  
  tft.setCursor(0, 180);
  tft.println("ADD WATER");  
}

void prepare_boil_menu() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(110, 0);
  tft.setTextColor(ILI9341_BLUE);  tft.setTextSize(4);
  tft.println("MENU");
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_BLUE);  tft.setTextSize(4);
  tft.println("HEAT");
  tft.setTextColor(ILI9341_WHITE);
  tft.drawRect(0,40,20,200,ILI9341_RED);
  tft.setCursor(25, 140);
  tft.println("0%");
  tft.setCursor(35, 40);
  tft.println("OFF");  
  tft.setCursor(42, 80);
  tft.println("ON");  
  
  display_temp1();
}

void prepare_pumpacross_menu() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(110, 0);
  tft.setTextColor(ILI9341_BLUE);  tft.setTextSize(4);
  tft.println("MENU");
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_BLUE);  tft.setTextSize(4);
  tft.println("HEAT");
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

  display_temp2();
}

void prepare_mash_menu() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(110, 0);
  tft.setTextColor(ILI9341_BLUE);  tft.setTextSize(4);
  tft.println("MENU");
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_BLUE);  tft.setTextSize(4);
  tft.println("HEAT");
  tft.setTextColor(ILI9341_WHITE);
  tft.drawRect(0,40,20,200,ILI9341_RED);
  tft.setCursor(25, 140);
  tft.println("0%");
  tft.setCursor(35, 40);
  tft.println("OFF");  
  tft.setCursor(42, 80);
  tft.println("ON");  
  
  display_temp2();
}

void prepare_pumpout_menu() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(110, 0);
  tft.setTextColor(ILI9341_BLUE);  tft.setTextSize(4);
  tft.println("MENU");
  tft.setTextColor(ILI9341_BLUE);  tft.setTextSize(4);
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
}

void prepare_addwater_menu() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(110, 0);
  tft.setTextColor(ILI9341_BLUE);  tft.setTextSize(4);
  tft.println("MENU");
  tft.setTextColor(ILI9341_BLUE);  tft.setTextSize(4);
  tft.setCursor(0, 100);
  tft.setTextColor(ILI9341_BLUE);
  tft.println("RESET");
  display_water_count(true);
}

void clear_all() {
  // Pump off
  pump_percent = 0;
  pump_on = false;  
  change_pump(pump_on);

  // Heaters off
  send_heater1_percent(0.0);
  send_heater2_percent(0.0);

}

void check_pump_touch(int xx, int yy) {
  // Pump change
  if (xx>=220 && yy>=40) {
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

void check_heater_touch(int xx, int yy, int heater_num, int heater_range) 
{
  // Heater slider
  if (xx <= 30 && yy >= 40) {
    tft.fillRect(25,140,100,40,ILI9341_BLACK);
    tft.setCursor(25, 140);
    float heater_percent_f = float(100-((yy-40)/2))/100.*heater_range;
    if (heater_num == 1) {
      send_heater1_percent(heater_percent_f);
      display_heater1();
    }
    else {
      send_heater2_percent(heater_percent_f);
      display_heater2();
    }
  }
  // Heater On/Off
  else if (xx >= 35 && xx <=135 && yy >= 40 && yy <=120) {
    tft.fillRect(25,140,100,40,ILI9341_BLACK);
    tft.fillRect(25,140,80,40,ILI9341_BLACK);
    tft.setCursor(25, 140);
    if (yy >= 80) {
      if (heater_num == 1) {
	send_heater1_percent(float(heater_range));
	display_heater1();
      }
      else {
	send_heater2_percent(float(heater_range));
	display_heater2();
      }
    }
    else {
      if (heater_num == 1) {
	send_heater1_percent(0.0);
	display_heater1();
      }
      else {
	send_heater2_percent(0.0);
	display_heater2();
      }
    }
  }
}

void check_menu_touch(int xx, int yy) {
  if (xx>=110 && xx <= 210 && yy<=40) {
    menu = TOP;
    clear_all();
    prepare_top_menu();
  }
}

void setup() {
  Serial1.begin(9600);
  Serial.begin(9600);

  pinMode(pump_pin, OUTPUT);
  pinMode(solenoid_pin, OUTPUT);  
  pinMode(wtr_rqst_pin, INPUT);
  digitalWrite(wtr_rqst_pin, HIGH);
  
  pinMode(flow_pin, INPUT);
  digitalWrite(solenoid_pin, LOW);
  attachInterrupt(0, service_flow_pin, RISING);
  tft.begin();
  ctp.begin(40);

  sol_update_time = millis()+sol_update_millis;
  flow_update_time = millis()+flow_update_millis;
  
  // Clear the serial buffer
  char message[61];
  Serial1.readBytes(message, 61);
  tft.fillScreen(ILI9341_BLACK);

  tft.setRotation(3);
  clear_all();
  prepare_top_menu();
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

    switch (menu)
      {
      case TOP:
	if ( xx <=160 && yy >= 0 && yy <45) {
	  // Boil
	  clear_all();
	  menu = BOIL;
	  prepare_boil_menu();
	}
	else if ( xx <=160 && yy >= 45 && yy <90) {
	  // Pump across
	  clear_all();
	  menu = PUMPACROSS;
	  prepare_pumpacross_menu();
	}
	else if ( xx <=160 && yy >= 90 && yy <135) {
	  // Mash
	  clear_all();
	  menu = MASH;
	  prepare_mash_menu();
	  // pump 100%
	  pump_percent = 100;
	  pump_on = true;  
	  change_pump(pump_on);
	}
	else if ( xx <=160 && yy >= 135 && yy <180) {
	  // Pump out
	  clear_all();
	  menu = PUMPOUT;
	  prepare_pumpout_menu();
	}
	else if ( xx <=160 && yy >= 180 && yy <225) {
	  // Add water
	  clear_all();
	  menu = ADDWATER;
	  prepare_addwater_menu();
	}
	break;
      case BOIL:
	check_heater_touch(xx, yy, 1, 100);
	check_menu_touch(xx,yy);
	break;
      case PUMPACROSS:
	check_heater_touch(xx, yy, 2, 100);
	check_pump_touch(xx, yy);
	check_menu_touch(xx,yy);
	break;
      case MASH:
	check_heater_touch(xx, yy, 2, 25);
	check_menu_touch(xx,yy);
	break;
      case PUMPOUT:
	check_pump_touch(xx, yy);
	check_menu_touch(xx,yy);
	break;
      case ADDWATER:
	check_menu_touch(xx,yy);
	if (xx<=125 && yy >= 100 && yy<=140) {
	  flow_count = 0;
	  display_water_count(true);
	}
	break;
      }
  } 

  // Check for pump transition
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

  // Flow display update
  if (millis() > flow_update_time) {
    display_water_count(false);
    flow_update_time = millis()+flow_update_millis;  
  }

  // Water solenoid update
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
      itoa(message_bytes[1], itoabuf, 10);
      message_ptr += 1;
      // Handle the messages here

      if (message_bytes[1] == 0x05) {
        // Receive temperature 1
	if (message_bytes[2] != temp1.int_part && message_bytes[3] != temp1.dec_part) {
	  temp1.int_part = message_bytes[2];
	  temp1.dec_part = message_bytes[3];
	  display_temp1();
	}
      }
      else if (message_bytes[1] == 0x06) {
        // Receive heater1 percentage
	if (message_bytes[2] != heater1_percent.int_part && message_bytes[3] != heater1_percent.dec_part) {
	  heater1_percent.int_part = message_bytes[2];
	  heater1_percent.dec_part = message_bytes[3];
	  display_heater1();
	}
      }
      else if (message_bytes[1] == 0x07) {
        // Receive temperature 2
	if (message_bytes[2] != temp2.int_part && message_bytes[3] != temp2.dec_part) {
	  temp2.int_part = message_bytes[2];
	  temp2.dec_part = message_bytes[3];
	  display_temp2();
	}
      }
      else {
         Serial.println("Unhandled");
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
