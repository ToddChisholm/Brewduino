#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include <Wire.h>      // this is needed for FT6206
#include <Adafruit_FT6206.h>

// For the Adafruit shield, these are the default.
#define TFT_DC 9
#define TFT_CS 10

#include <LiquidCrystal.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 13

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal lcd(46,48,44);

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
unsigned long last_flow_touch_time = 0;

volatile unsigned long int flow_count = 0;
unsigned long int prev_flow_count = 0;
unsigned long int target_flow_count = 0;

// Factor to convert flow count to gallons
float flow_factor = .0005164;

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
float heater1_target = 0.;
float heater2_target = 0.;

enum MenuPages { TOP, BOIL, PIDBOIL, PUMPACROSS, MASH, SPARGE, PUMPOUT, ADDWATER, PHCAL};

MenuPages menu = TOP;

byte strt_mark = 255;

boolean current_sparge = false;
boolean current_add_water = false;

String sensorstring = "";                             //a string to hold the data from the Atlas Scientific product
boolean input_string_complete = false;                //have we received all the data from the PC
boolean sensor_string_complete = false;               //have we received all the data from the Atlas Scientific product
float pH;                                             //used to hold a floating point number that is the pH
unsigned int ds_temp_millis = 10000;
unsigned long ds_temp_update_time = 0;

int first_digit = -1;
boolean first_digit_entered = false;
unsigned int first_digit_millis = 5000;
long unsigned first_digit_timeout = 0;

boolean button_must_clear = false;
unsigned int debounce_millis = 80;
unsigned long debounce_time = 0;

int keyPressed() {
  // Check the keypad
  if (digitalRead(22) == LOW) {
    // This is the asterisk
  }    
  if (digitalRead(23) == LOW) {
    return 7;
  }  
  if (digitalRead(25) == LOW) {
    return 4;
  }  
  if (digitalRead(27) == LOW) {
    return 1;
  }  
  if (digitalRead(29) == LOW) {
    return 0;
  }  
  if (digitalRead(31) == LOW) {
    return 8;
  }  
  if (digitalRead(33) == LOW) {
    return 5;
  }  
  if (digitalRead(35) == LOW) {
    return 2;
  }  
  if (digitalRead(37) == LOW) {
    // This is the hash
  }  
  if (digitalRead(39) == LOW) {
    return 9;
  }  
  if (digitalRead(41) == LOW) {
    return 6;
  }  
  if (digitalRead(43) == LOW) {
    return 3;
  }
  return -1;
}

boolean isValidNumber(String str){
   boolean isNum=false;
   for(byte i=0;i<str.length();i++)
   {
       isNum = isDigit(str.charAt(i)) || str.charAt(i) == '+' || str.charAt(i) == '.' || str.charAt(i) == '-';
       if(!isNum) return false;
   }
   return isNum;
}

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

void send_heater1_temp(float temp) {
  float_data heater1_temp = float_to_float_data(temp);
  Serial1.write(strt_mark);
  // Temperature 2 percentage
  byte heater1_pid_temp_mark = 1;
  Serial1.write(heater1_pid_temp_mark);
  Serial1.write(heater1_temp.int_part);
  Serial1.write(heater1_temp.dec_part);
}
 
void send_heater2_temp(float temp) {
  float_data heater2_temp = float_to_float_data(temp);
  Serial1.write(strt_mark);
  // Temperature 2 percentage
  byte heater2_pid_temp_mark = 3;
  Serial1.write(heater2_pid_temp_mark);
  Serial1.write(heater2_temp.int_part);
  Serial1.write(heater2_temp.dec_part);
}

void send_heater2_temp_100prct(float temp) {
  float_data heater2_temp = float_to_float_data(temp);
  Serial1.write(strt_mark);
  // Temperature 2 percentage
  byte heater2_pid_temp_mark_100prct = 9;
  Serial1.write(heater2_pid_temp_mark_100prct);
  Serial1.write(heater2_temp.int_part);
  Serial1.write(heater2_temp.dec_part);
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
  if (menu == BOIL || menu == PIDBOIL) {
    tft.fillRect(60,200,100,40,ILI9341_BLACK);
    tft.setCursor(60, 200);
    tft.println(String(int(temp1.int_part)-100)+"."+String(temp1.dec_part)+"C");
  }
  lcd.setCursor(5,0);
  lcd.print(String(int(temp1.int_part)-100)+"."+String(temp1.dec_part)+"C");
}
void display_temp2() {
  if (menu == PUMPACROSS || menu == MASH || menu == SPARGE) {
    tft.fillRect(60,200,100,40,ILI9341_BLACK);
    tft.setCursor(60, 200);
    tft.println(String(int(temp2.int_part)-100)+"."+String(temp2.dec_part)+"C");
  }
  lcd.setCursor(5,1);
  lcd.print(String(int(temp2.int_part)-100)+"."+String(temp2.dec_part)+"C");
}

void display_heater1() {
  if (menu == BOIL || menu == PIDBOIL) {
    tft.fillRect(25,120,120,40,ILI9341_BLACK);
    tft.setCursor(25, 120);
    tft.println(String(int(heater1_percent.int_part)-100)+"."+String(heater1_percent.dec_part));
  }
}
void display_heater2() {
  if (menu == PUMPACROSS || menu == MASH || menu == SPARGE) {
    tft.fillRect(25,120,120,40,ILI9341_BLACK);
    tft.setCursor(25, 120);
    tft.println(String(int(heater2_percent.int_part)-100)+"."+String(heater2_percent.dec_part)+"%");
  }
}
void display_heater1_target() {
  if (menu == BOIL || menu == PIDBOIL) {
    float_data heater1_target_fd = float_to_float_data(heater1_target);
    tft.fillRect(25,160,120,40,ILI9341_BLACK);
    tft.setCursor(25, 160);
    tft.println(String(int(heater1_target_fd.int_part)-100)+"."+String(heater1_target_fd.dec_part)+"C");
  }
}
void display_heater2_target() {
  if (menu == PUMPACROSS || menu == MASH || menu == SPARGE) {
    float_data heater2_target_fd = float_to_float_data(heater2_target);
    tft.fillRect(25,160,120,40,ILI9341_BLACK);
    tft.setCursor(25, 160);
    tft.println(String(int(heater2_target_fd.int_part)-100)+"."+String(heater2_target_fd.dec_part)+"C");
  }
}
void display_water_count(bool force) {
  if ( force || ((menu==SPARGE||menu==ADDWATER) && prev_flow_count != flow_count) ) {
    tft.fillRect(20,200,300,40,ILI9341_BLACK);
    tft.setCursor(20, 200);
    float gallons = float(flow_count)*flow_factor;
    int gallons_int_part = int(gallons);
    int gallons_dec_part1 = int((gallons-float(gallons_int_part))*10.);
    int gallons_dec_part2 = int((gallons-float(gallons_dec_part1)/10.-float(gallons_int_part))*100.);
    prev_flow_count = flow_count;
    tft.println(String(gallons_int_part)+"."+String(gallons_dec_part1)+String(gallons_dec_part2)+"G");

    tft.setCursor(160, 200);
    if (target_flow_count > flow_count) {
      float gallons = float(target_flow_count-flow_count)*flow_factor;
      int gallons_int_part = int(gallons);
      int gallons_dec_part1 = int((gallons-float(gallons_int_part))*10.);
      int gallons_dec_part2 = int((gallons-float(gallons_dec_part1)/10.-float(gallons_int_part))*100.);
      prev_flow_count = flow_count;
      tft.println(String(gallons_int_part)+"."+String(gallons_dec_part1)+String(gallons_dec_part2)+"G");
    }
    else {
      tft.println("---G");
    }
  }
}

void prepare_top_menu() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(3);
  tft.setCursor(0, 0);
  tft.println("BOIL");  
  tft.setCursor(160, 0);
  tft.println("PID BOIL");  
  tft.setCursor(0, 45);
  tft.println("PUMP TO BOIL");  
  tft.setCursor(0, 90);
  tft.println("MASH");  
  tft.setCursor(160, 90);
  tft.println("SPRG");  
  tft.setCursor(0, 135);
  tft.println("PUMP OUT");  
  tft.setCursor(0, 180);
  tft.println("ADD WATER");  
  tft.setCursor(160, 180);
  tft.println("PH CAL");  
}

void prepare_phcal_menu() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(4);
  tft.setCursor(110, 0);
  tft.setTextColor(ILI9341_BLUE);  tft.setTextSize(4);
  tft.println("MENU");
  tft.setCursor(0, 45);
  tft.println("C4");  
  tft.setCursor(0, 112);
  tft.println("C7");  
  tft.setCursor(0, 180);
  tft.println("C10");  

  // Clear the serial buffer to the Ph meter
  if (Serial2.available() > 0) {                     //if we see that the Atlas Scientific product has sent a character
    while (Serial2.available() > 0) {
      if (sensor_string_complete == true) {
	// If we have read a complete line but still have remaining data, clear the current line
	sensorstring = "";
	sensor_string_complete = false;
      }
      char inchar = (char)Serial2.read();              //get the char we just received
      sensorstring += inchar;                           //add the char to the var called sensorstring
      if (inchar == '\r') {                             //if the incoming character is a <CR>
	sensor_string_complete = true;                  //set the flag
      }
    }
  }
  
  Serial2.print("SLOPE,?\r");

  // Check for response
  boolean slope_string_complete = false;
  unsigned long response_timeout = millis()+500;
  String slopestring="";
  slopestring.reserve(30);                           //set aside some bytes for receiving data from Atlas Scientific product
  while (!slope_string_complete && millis()<response_timeout ) {
    if (Serial2.available() > 0) {
      char inchar = (char)Serial2.read();              //get the char we just received
      slopestring += inchar;                           //add the char to the var called slopestring
      if (inchar == '\r') {                             //if the incoming character is a <CR>
	slope_string_complete = true;                  //set the flag
      }
    }
  }
  tft.setTextSize(3);
  tft.setCursor(140, 90);
  if (slope_string_complete) {
    tft.println(slopestring.substring(6,slopestring.length()));
  }
  else {
    tft.println("No slope");
    tft.println(slopestring);
  }
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
  tft.setCursor(25, 120);
  tft.println("0%");
  tft.setCursor(35, 40);
  tft.println("OFF");  
  tft.setCursor(42, 80);
  tft.println("ON");  
  
  tft.setCursor(220, 0);
  tft.setTextColor(ILI9341_BLUE);
  tft.println("PUMP");
  tft.setCursor(220, 40);
  tft.setTextColor(ILI9341_GREEN);
  tft.println("OFF");
  tft.setCursor(220, 100);
  tft.setTextColor(ILI9341_GREEN);
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
  tft.setCursor(25, 120);
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
  tft.setCursor(25, 120);
  tft.println("0%");
  tft.setCursor(35, 40);
  tft.println("OFF");  
  tft.setCursor(42, 80);
  tft.println("ON");  

  tft.setCursor(200, 40);
  tft.println("40");  
  tft.setCursor(200, 80);
  tft.println("60");  
  tft.setCursor(200, 120);
  tft.println("70");  
  
  display_temp2();
}

void prepare_pidboil_menu() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(110, 0);
  tft.setTextColor(ILI9341_BLUE);  tft.setTextSize(4);
  tft.println("MENU");
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_BLUE);  tft.setTextSize(4);
  tft.println("HEAT");
  tft.setTextColor(ILI9341_WHITE);
  tft.drawRect(0,40,20,200,ILI9341_RED);
  tft.setCursor(25, 120);
  tft.println("0%");
  tft.setCursor(35, 40);
  tft.println("OFF");  
  tft.setCursor(42, 80);
  tft.println("ON");  
  
  tft.setCursor(220, 0);
  tft.setTextColor(ILI9341_BLUE);
  tft.println("PUMP");
  tft.setCursor(220, 40);
  tft.setTextColor(ILI9341_GREEN);
  tft.println("OFF");
  tft.setCursor(220, 100);
  tft.setTextColor(ILI9341_GREEN);
  tft.println("ON");

  display_temp1();

}

void prepare_sparge_menu() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(110, 0);
  tft.setTextColor(ILI9341_RED);  tft.setTextSize(4);
  tft.println("MENU");
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_RED);  tft.setTextSize(4);
  tft.println("HEAT");
  tft.setTextColor(ILI9341_RED);
  tft.drawRect(0,40,20,200,ILI9341_RED);
  tft.setCursor(25, 120);
  tft.println("0%");
  tft.setCursor(35, 40);
  tft.println("OFF");  
  tft.setCursor(42, 80);
  tft.println("ON");  
  
  display_temp2();
  display_heater2();
  display_heater2_target();
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
  tft.setCursor(0, 60);
  tft.setTextColor(ILI9341_BLUE);
  tft.println("RESET");

  tft.setCursor(0, 160);
  tft.setTextSize(3);
  tft.println("5G");
  tft.setCursor(80, 160);
  tft.println("1G");
  tft.setCursor(160, 160);
  tft.println("1/2G");
  tft.setCursor(240, 160);
  tft.println("1/4G");
  tft.setTextSize(4);
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

void check_add_water_touch(int xx, int yy, boolean sparging) {
  if (xx<=125 && yy >= 60 && yy<=100) {
    flow_count = 0;
    target_flow_count = 0;
    display_water_count(true);
  }
  else if ( millis() > last_flow_touch_time+750) { // Prevent double bouncing 
    if (xx> 0 && xx<=60 && yy >= 160 && yy<=200) {
      // Add 5G
      if (target_flow_count < flow_count) {
	target_flow_count = flow_count;
      }
      target_flow_count += int(5.0/flow_factor);
      display_water_count(true);
      last_flow_touch_time = millis();
    }
    else if (xx> 80 && xx<=140 && yy >= 160 && yy<=200) {
      // Add 1G
      if (target_flow_count < flow_count) {
	target_flow_count = flow_count;
      }
      target_flow_count += int(1.0/flow_factor);
      display_water_count(true);
      last_flow_touch_time = millis();
    }
    else if (xx> 160 && xx<=220 && yy >= 160 && yy<=200) {
      // Add 0.5G
      if (target_flow_count < flow_count) {
	target_flow_count = flow_count;
      }
      target_flow_count += int(0.5/flow_factor);
      display_water_count(true);
      last_flow_touch_time = millis();
    }
    else if (xx> 240 && yy >= 160 && yy<=200) {
      // Add 0.25G
      if (target_flow_count < flow_count) {
	target_flow_count = flow_count;
      }
      target_flow_count += int(0.25/flow_factor);
      display_water_count(true);
      last_flow_touch_time = millis();
    }
  }
}

void check_pump_onoff_touch(int xx, int yy) {
  // Pump change
  if (xx>=220 && yy>=40 && yy<80) {
    pump_on = false;  
    change_pump(pump_on);
  }
  else if (xx>=220 && yy>=100 && yy<140) {
    pump_on = true;  
    change_pump(pump_on);
  }
}

void check_phcal_touch(int xx, int yy)
{
  boolean cal_sent = false;
  if ( xx <=100 && yy >= 45 && yy <90) {
    // Calibrate to 4
    Serial2.print("Cal,mid,4.00\r");
    cal_sent = true;
  }
  else if ( xx <=100 && yy >= 112 && yy <147) {
    // Calibrate to 7
    Serial2.print("Cal,mid,7.00\r");
    cal_sent = true;
  }
  else if ( xx <=100 && yy >= 180 && yy <225) {
    // Calibrate to 10
    Serial2.print("Cal,mid,10.00\r");
    cal_sent = true;
  }
  if (cal_sent) {
    // Check for the OK
    boolean ok_string_complete = false;
    unsigned long response_timeout = millis()+250;
    String okstring="";
    okstring.reserve(30);                           //set aside some bytes for receiving data from Atlas Scientific product
    while (!ok_string_complete && millis()<response_timeout ) {
      if (Serial2.available() > 0) {
	char inchar = (char)Serial2.read();              //get the char we just received
	okstring += inchar;                           //add the char to the var called okstring
	if (inchar == '\r') {                             //if the incoming character is a <CR>
	  ok_string_complete = true;                  //set the flag
	}
      }
    }
    if (ok_string_complete) {
      tft.setTextSize(3);
      tft.setCursor(160, 90);
      if (ok_string_complete) {
	tft.println(okstring);
      }
      else {
	tft.println("No OK");
      }
    }
  }
}

void check_heater_touch(int xx, int yy, int heater_num, int heater_range, float heater_offset, bool pid) 
{
  // Heater slider
  if (xx <= 30 && yy >= 40) {
    tft.fillRect(25,140,100,40,ILI9341_BLACK);
    tft.setCursor(25, 140);
    float heater_set_f = float(100-((yy-40)/2))/100.*float(heater_range)+heater_offset;
    if (heater_num == 1) {
      if (pid) {
	send_heater1_temp(heater_set_f);
	heater1_target = heater_set_f;
	display_heater1_target();
      }
      else {
	send_heater1_percent(heater_set_f);
      }
      display_heater1();
    }
    else {
      if (pid) {
	send_heater2_temp(heater_set_f);
	heater2_target = heater_set_f;
	display_heater2_target();
      }
      else {
	send_heater2_percent(heater_set_f);
      }
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

void check_temp_touch(int xx, int yy, int heater_num) 
{
  bool point_found = false;
  float heater_set_f = 0.0;
  if (xx >= 200 && yy >= 40 && yy <80 ) {
    // Target temp 40
    heater_set_f = 40.0;
    point_found = true;
  }
  else if (xx >= 200 && yy >= 80 && yy <120 ) {
    // Target temp 60
    heater_set_f = 60.0;
    point_found = true;
  }
  else if (xx >= 200 && yy >= 120 && yy <160 ) {
    // Target temp 70
    heater_set_f = 70.0;
    point_found = true;
  }
  if (point_found) {
    tft.fillRect(25,140,100,40,ILI9341_BLACK);
    tft.setCursor(25, 140);
    if (heater_num == 1) {
      send_heater1_temp(heater_set_f);
      heater1_target = heater_set_f;
      display_heater1_target();
      display_heater1();
    }
    else {
      send_heater2_temp(heater_set_f);
      heater2_target = heater_set_f;
      display_heater2_target();
      display_heater2();
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
  for  (int keypin = 23; keypin <=43; keypin+=2) { 
    pinMode(keypin, INPUT);
    digitalWrite(keypin, HIGH);
  } 
  
  pinMode(22, INPUT);
  digitalWrite(22, HIGH);
  lcd.begin(20,4);
  Serial.begin(9600);     
  Serial2.begin(9600);                                 //set baud rate for the hardware serial port_0 to 9600
  sensorstring.reserve(30);                           //set aside some bytes for receiving data from Atlas Scientific product
  sensors.begin();

  lcd.setCursor(12,0);
  lcd.print("PH");
  lcd.setCursor(0,0);
  lcd.print("Boil");  
  lcd.setCursor(0,1);
  lcd.print("RIMS");  
  lcd.setCursor(0,2);
  lcd.print("Out");  
  lcd.setCursor(0,3);
  lcd.print("Coil");  
  

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
  ds_temp_update_time = millis()+ds_temp_millis;

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

  if (first_digit_entered && millis() > first_digit_timeout) {
    first_digit_entered = false;
    // Return the display to the previous set, since the second digit timed out
    if ( menu==PIDBOIL || menu==MASH ) {
      display_heater1_target();
      display_heater2_target();  
    } 
    else {
      display_heater1();
      display_heater2();
    }
  }
  int current_key = keyPressed();
  if (button_must_clear) {
    if (millis() > debounce_time) {
      if (current_key < 0) {
	button_must_clear = false;
      }
      else {
	debounce_time = millis()+debounce_millis;
      }
    }
  }
  else if ( current_key >=0 && ( menu==BOIL || menu==PIDBOIL || menu==PUMPACROSS || menu==MASH )) {
    if ( first_digit_entered ) {
      // We've already got the first digit, so this completes the entry
      float heater_set_f = first_digit*10 + current_key;
      boolean pid = false;
      int heater_num = 0;
      switch (menu) {
      case BOIL:
	pid = false;
	heater_num = 1;
	break;
      case PIDBOIL:
	pid = true;
	heater_num = 1;
	break;
      case PUMPACROSS:
	pid = false;
	heater_num = 2;
	break;
      case MASH:
	pid = true;
	heater_num = 2;
	break;
      }
      if (heater_num == 1) {
	if (pid) {
	  send_heater1_temp(heater_set_f);
	  heater1_target = heater_set_f;
	  display_heater1_target();
	}
	else {
	  send_heater1_percent(heater_set_f);
	}
	display_heater1();
      }
      else {
	if (pid) {
	  send_heater2_temp(heater_set_f);
	  heater2_target = heater_set_f;
	  display_heater2_target();
	}
	else {
	  send_heater2_percent(heater_set_f);
	}
	display_heater2();
      }
      first_digit_entered = false;
    }
    else {
      // Get the first digit
      first_digit_entered = true;
      first_digit = current_key;
      first_digit_timeout = millis() + first_digit_millis;
      // Show the first digit
      if (menu==PIDBOIL || menu==MASH) {
	tft.fillRect(25,160,120,40,ILI9341_BLACK);
	tft.setCursor(25, 160);
	tft.println(String(first_digit));
      }
      else {
	tft.fillRect(25,120,120,40,ILI9341_BLACK);
	tft.setCursor(25, 120);
	tft.println(String(first_digit));
      }
    }
    button_must_clear = true;
    debounce_time = millis() + debounce_millis;
  }
	
  if (Serial2.available() > 0) {                     //if we see that the Atlas Scientific product has sent a character
    // Put a time limit on, just in case
    Serial.println("Getting");
    unsigned long max_wait = millis()+250;
    while (Serial2.available() > 0 && millis()<max_wait) {
      if (sensor_string_complete == true) {
	// If we have read a complete line but still have remaining data, clear the current line
	sensorstring = "";
	sensor_string_complete = false;
      }
      char inchar = (char)Serial2.read();              //get the char we just received
      sensorstring += inchar;                           //add the char to the var called sensorstring
      if (inchar == '\r') {                             //if the incoming character is a <CR>
	sensor_string_complete = true;                  //set the flag
      }
    }
    Serial.println("Done"+sensorstring);
  }

  if (sensor_string_complete == true) {               //if a string from the Atlas Scientific product has been received in its entirety
    if (isValidNumber(sensorstring.substring(0,4)) == true) {
      lcd.setCursor(15,0);
      lcd.print(sensorstring.substring(0,4));
    }
    sensorstring = "";                                //clear the string
    sensor_string_complete = false;                   //reset the flag used to tell if we have received a completed string from the Atlas Scientific product
  }
  
  if (millis() > ds_temp_update_time) {
    sensors.requestTemperatures(); // Send the command to get temperatures
    lcd.setCursor(5,2);
    lcd.print(sensors.getTempCByIndex(0));    
    lcd.setCursor(5,3);
    lcd.print(sensors.getTempCByIndex(1));  
    ds_temp_update_time = millis()+ds_temp_millis;  
  }

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
	else if ( xx > 160 && yy >= 0 && yy <45) {
	  // PID Boil
	  clear_all();
	  menu = PIDBOIL;
	  prepare_pidboil_menu();
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
	else if ( xx > 160 && yy >= 90 && yy <135) {
	  // Sparge
	  clear_all();
	  menu = SPARGE;
	  prepare_sparge_menu();
	  // pump 100%
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
	else if ( xx >160 && yy >= 180 && yy <225) {
	  // Ph Calibration
	  clear_all();
	  menu = PHCAL;
	  prepare_phcal_menu();
	}
	break;
      case BOIL:
	check_heater_touch(xx, yy, 1, 100, 0., false);
	check_menu_touch(xx,yy);
	check_pump_onoff_touch(xx,yy);
	break;
      case PIDBOIL:
	check_heater_touch(xx, yy, 1, 60, 40., true);
	//check_temp_touch(xx, yy, 1);
	check_menu_touch(xx,yy);
	check_pump_onoff_touch(xx,yy);
	break;
      case PUMPACROSS:
	check_heater_touch(xx, yy, 2, 100, 0., false);
	check_pump_touch(xx, yy);
	check_menu_touch(xx,yy);
	break;
      case MASH:
	check_heater_touch(xx, yy, 2, 60., 40., true);
	check_temp_touch(xx, yy, 2);
	check_menu_touch(xx,yy);
	break;
      case PUMPOUT:
	check_pump_touch(xx, yy);
	check_menu_touch(xx, yy);
	break;
      case ADDWATER:
	check_menu_touch(xx,yy);
	check_add_water_touch(xx, yy, false);
	break;
      case SPARGE:
	check_menu_touch(xx,yy);
	check_add_water_touch(xx, yy, true);
	break;
      case PHCAL:
	check_menu_touch(xx,yy);
	check_phcal_touch(xx, yy);
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
    if ( target_flow_count > flow_count ) {
      digitalWrite(solenoid_pin, HIGH);
      if ( !current_add_water ) {
	current_add_water = true;
	if ( menu==SPARGE ) {
	  current_sparge = true;
	  // Turn on the 100% PID control on the RIMS
	  // Fix at 170F/77C for now
	  send_heater2_temp_100prct(77.0);
	  heater2_target = 77.0;
	  display_heater2_target();
	}
      }
    }
    else {
      if (digitalRead(wtr_rqst_pin) == LOW) {
	digitalWrite(solenoid_pin, HIGH);
      }
      else {
	digitalWrite(solenoid_pin, LOW);
	if ( current_add_water ) {
	  current_add_water = false;
	  if ( current_sparge ) {
	    current_sparge = false;
	    // Turn off the 100% PID control on the RIMS
	    send_heater2_percent(0.0);
	  }
	}
      }
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
	if (message_bytes[2] != temp1.int_part || message_bytes[3] != temp1.dec_part) {
	  temp1.int_part = message_bytes[2];
	  temp1.dec_part = message_bytes[3];
	  display_temp1();
	}
      }
      else if (message_bytes[1] == 0x06) {
        // Receive heater1 percentage
	if (message_bytes[2] != heater1_percent.int_part || message_bytes[3] != heater1_percent.dec_part) {
	  heater1_percent.int_part = message_bytes[2];
	  heater1_percent.dec_part = message_bytes[3];
	  display_heater1();
	}
      }
      else if (message_bytes[1] == 0x07) {
        // Receive temperature 2
	if (message_bytes[2] != temp2.int_part || message_bytes[3] != temp2.dec_part) {
	  temp2.int_part = message_bytes[2];
	  temp2.dec_part = message_bytes[3];
	  display_temp2();
	}
      }
      else if (message_bytes[1] == 0x08) {
        // Receive heater2 percentage

	if (message_bytes[2] != heater2_percent.int_part || message_bytes[3] != heater2_percent.dec_part) {
	  heater2_percent.int_part = message_bytes[2];
	  heater2_percent.dec_part = message_bytes[3];
	  display_heater2();
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
