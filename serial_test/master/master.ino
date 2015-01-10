/*
  LiquidCrystal Library - display() and noDisplay()
 
 Demonstrates the use a 16x2 LCD display.  The LiquidCrystal
 library works with all LCD displays that are compatible with the 
 Hitachi HD44780 driver. There are many of them out there, and you
 can usually tell them by the 16-pin interface.
 
 This sketch prints "Hello World!" to the LCD and uses the 
 display() and noDisplay() functions to turn on and off
 the display.
 
 The circuit:
 * LCD RS pin to digital pin 12
 * LCD Enable pin to digital pin 11
 * LCD D4 pin to digital pin 5
 * LCD D5 pin to digital pin 4
 * LCD D6 pin to digital pin 3
 * LCD D7 pin to digital pin 2
 * LCD R/W pin to ground
 * 10K resistor:
 * ends to +5V and ground
 * wiper to LCD VO pin (pin 3)
 
 Library originally added 18 Apr 2008
 by David A. Mellis
 library modified 5 Jul 2009
 by Limor Fried (http://www.ladyada.net)
 example added 9 Jul 2009
 by Tom Igoe 
 modified 22 Nov 2010
 by Tom Igoe

 This example code is in the public domain.

 http://arduino.cc/en/Tutorial/LiquidCrystalDisplay

 */

// include the library code:
#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(8,9,4,5,6,7);

unsigned int temp1_int = 0;
unsigned int temp1_rmndr = 0;

int message_bytes[4];
int message_ptr = 0;

unsigned int temp_update_millis = 1000;
unsigned long temp_update_time = 0;
unsigned int num_updates = 0;

void setup() {
  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.clear();
  lcd.print(String(temp1_int)+"  "+String(num_updates));
  lcd.setCursor(0,1);
  lcd.print(temp1_rmndr);

  Serial.begin(9600);
  // Clear the serial buffer
  char message[61];
  Serial.readBytes(message, 61);
  temp_update_time = millis()+temp_update_millis;
  lcd.begin(16, 2);
  lcd.display();
}

void loop() {
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
	num_updates += 1;
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
  if (millis() > temp_update_time) {
    lcd.clear();
    lcd.print(temp1_int);
    lcd.setCursor(6,0);
    lcd.print(num_updates);
    lcd.setCursor(0,1);
    lcd.print(temp1_rmndr);

    byte upper = num_updates/10;
    byte lower = num_updates - upper*10;
    lcd.setCursor(6,1);
    lcd.print(upper);
    lcd.setCursor(10,1);
    lcd.print(lower);
    temp_update_time = millis()+temp_update_millis;
  }
}

