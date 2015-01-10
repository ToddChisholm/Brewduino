void setup() {
  Serial.begin(9600);
  
}

void loop() {
  int val = analogRead(5);
  byte strt_mark = 255;
  Serial.write(strt_mark);
  byte temp_mark = 5;
  Serial.write(temp_mark);
  byte first_elemnt = val/10;
  byte second_elemnt = val-first_elemnt*10;
  Serial.write(first_elemnt);
  Serial.write(second_elemnt);
  delay(1000);
}

