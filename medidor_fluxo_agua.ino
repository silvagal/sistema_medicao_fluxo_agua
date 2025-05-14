/*
 * SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 4 (for MKRZero SD: SDCARD_SS_PIN)
*/

#include <SPI.h>
#include <SD.h>
#include <Wire.h> 
#include <LiquidCrystal.h>
#include "RTClib.h" 

#define BYTES_PER_LINE 36
#define READ_MODE A0
#define LAST_READ A1
#define FLOWSENSOR 3 // Sensor Input

void flow();
void getData(File myFile, DateTime now);
void saveData(File myFile, DateTime now, float waterFlow, int totalSecondOn);
void secondsToHMS(const int seconds, unsigned short int &h, unsigned short int &m, unsigned short int &s);
void joinString(String fromStr, String &toStr, int imin, int imax);

RTC_DS3231 rtc; 
File myFile;
LiquidCrystal lcd(8, 7, 6, 5, 2, 1);

volatile int flow_frequency=0; // Measures flow sensor pulses
int posLast = 0, cont = 0, numLeadingZero=0, totalSecondOn=0, flag=0, flagReadMode=0, flagChange=0, flagLastRead=0;
unsigned short int hourOn=0, minuteOn=0, secondOn=0; 
String dayStr="", buffer="", waterFlowStr="", hStr="", mStr="", sStr="", timeStr="";
float waterFlow = 0.000000;

void setup() {
  pinMode(3, INPUT);
  pinMode(0, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP);
  pinMode(READ_MODE, INPUT_PULLUP);
  lcd.begin(16, 2);

  if (!SD.begin(4))
    while (1);
  
  myFile = SD.open("medicoes.txt", FILE_READ);
  cont = (myFile.size() / BYTES_PER_LINE) - 1;
  myFile.close();  
  
  if (!rtc.begin()) { 
    while (1); 
  }

  if (rtc.lostPower()) { 
    // rtc.adjust(DateTime(2025, 5, 13, 19, 00, 00)); 
  }

  delay(100);
  attachInterrupt(1, flow, RISING);
  sei(); // Enable interrupts
}

void loop() {
  if (digitalRead(READ_MODE) == LOW) {
    myFile = SD.open("medicoes.txt", FILE_READ);

    if (digitalRead(9) == LOW) {
      delay(120);
      if (digitalRead(9) == LOW) {
        cont--;               
        if (cont < 0)
          cont = 0; 
        lcd.clear();
      }
    }

    if (digitalRead(10) == LOW) {
      delay(120);
      if (digitalRead(10) == LOW) {
        if (cont < (myFile.size() / BYTES_PER_LINE) - 1)
          cont++;   
        lcd.clear();
      }
    }

    if (digitalRead(0) == LOW) {
      delay(120);
      if (digitalRead(0) == LOW) {    
        lcd.clear();
        if (flagChange == 0) {
          flag = 1;
          flagChange = 1;
        } else {
          flag = 0;
          flagChange = 0;
        }        
      }    
    }

    posLast = BYTES_PER_LINE * cont;
    myFile.seek(posLast);
    buffer = myFile.readStringUntil('\n');
    joinString(buffer, dayStr, 0, 9);

    if (flag == 0) {
      joinString(buffer, waterFlowStr, 11, 19);
      lcd.setCursor(0, 0);
      lcd.print("   ");
      lcd.print(dayStr);
      lcd.setCursor(0, 1);
      lcd.print("  V:");
      lcd.print(waterFlowStr);
    } else {
      joinString(buffer, timeStr, 26, 33);
      lcd.setCursor(0, 0);
      lcd.print("   ");
      lcd.print(dayStr);
      lcd.setCursor(0, 1);
      lcd.print("   T:");
      lcd.print(timeStr);
    }

    waterFlowStr = "";
    hStr = "";
    mStr = "";
    sStr = "";
    timeStr = "";
    dayStr = "";
    myFile.close();
  } else {
    if (analogRead(LAST_READ) > 50) { 
      DateTime now = rtc.now();
      myFile = SD.open("medicoes.txt", O_READ | O_WRITE);
      posLast = myFile.size() - BYTES_PER_LINE;
      myFile.seek(posLast);
      buffer = myFile.readStringUntil('\n');                    
      joinString(buffer, dayStr, 0, 1);
      
      if (dayStr.toInt() == now.day()) {
        joinString(buffer, waterFlowStr, 11, 23);
        joinString(buffer, hStr, 26, 28);
        joinString(buffer, mStr, 29, 31);
        joinString(buffer, sStr, 32, 34);
        totalSecondOn = hStr.toInt()*3600 + mStr.toInt()*60 + sStr.toInt();
        waterFlow = waterFlowStr.toFloat();
        myFile.seek(posLast);
      }

      totalSecondOn += 1;
      waterFlow += (((flow_frequency * 60 / 7.5)) / 3600) * 1.72;
      saveData(myFile, now, waterFlow, totalSecondOn);
      flow_frequency = 0;
      waterFlow = 0;
      totalSecondOn = 0;
      dayStr = "";
      waterFlowStr = "";
      hStr = "";
      mStr = "";
      sStr = "";
      myFile.close();
      delay(1000);
    } 
  }
}

void flow() {
  flow_frequency++;
}

void getData(File myFile, DateTime now) {
  if (now.day() < 10)
    myFile.print("0"); 
  myFile.print(now.day(), DEC); 
  myFile.print('/'); 
  if (now.month() < 10) 
    myFile.print("0"); 
  myFile.print(now.month(), DEC); 
  myFile.print('/'); 
  myFile.print(now.year(), DEC); 
}

void saveData(File myFile, DateTime now, float waterFlow, int totalSecondOn) {
  getData(myFile, now);
  myFile.print(' ');

  if (waterFlow < 10)
    numLeadingZero = 5;
  else if (waterFlow < 100)
    numLeadingZero = 4; 
  else if (waterFlow < 1000)
    numLeadingZero = 3; 
  else if (waterFlow < 10000)
    numLeadingZero = 2; 
  else if (waterFlow < 100000)
    numLeadingZero = 1; 
  else  
    numLeadingZero = 0;

  for (int i = 0; i < numLeadingZero; i++)
    myFile.print(0);
  myFile.print(waterFlow, 6);
  myFile.print("L");

  secondsToHMS(totalSecondOn, hourOn, minuteOn, secondOn);
  myFile.print(' ');
  if (hourOn < 10)
    myFile.print("0");
  myFile.print(hourOn);
  myFile.print(':');  
  if (minuteOn < 10)
    myFile.print("0");
  myFile.print(minuteOn);
  myFile.print(':');   
  if (secondOn < 10)
    myFile.print("0"); 
  myFile.println(secondOn);
} 

void secondsToHMS(const int seconds, unsigned short int &h, unsigned short int &m, unsigned short int &s) {
  int t = seconds;
  s = t % 60;
  t = (t - s) / 60;
  m = t % 60;
  t = (t - m) / 60;
  h = t;
}

void joinString(String fromStr, String &toStr, int imin, int imax) {
  for (int i = imin; i < imax + 1; i++)
    toStr += fromStr[i];
}
