// Solar Timer by Chiu Fang 2018

/***** Include Libraries *****/
#include <Wire.h>               //For RTC
#include <DS3231.h>             //For RTC
#include <LiquidCrystalFast.h>  //For 20x4 LCD Display
#include <SPI.h>                //For SD Card
#include <SD.h>                 //For SD Card
#include <ModbusMaster.h>       //For RS485

/***** Set Constants *****/
// Pin A4 → I2C SDA, Pin A5 → I2C SCL
const byte blPin = 9;           //Backlight pin
const byte chipSelect = 10;     //SD Card chip select
const byte MAX485_DE = 14;      //RS485 driver enable
const byte buttonPin = 15;      //Push button
const byte relayPin = 16;       //Relay control
LiquidCrystalFast lcd (2, 3, 4, 5, 6, 7, 8); // rs,rw,en1,d4,d5,d6,d7 Init LCD
ModbusMaster node;              //Init RS485
RTClib RTC;                     //Init RTC

/***** Declare Variables *****/
unsigned long blDelay = 0;
byte currentHour = 0;
unsigned int resultMain;
bool buttonInput = 0;
float solarVolt = 0;
float battVolt = 0;
float solarAmp = 0;
float battAmp = 0;
unsigned int chargeMode;
byte relayOn = 17;              //time to turn relay on
byte relayOff = 20;             //time to turn relay off

void setup () {
  Wire.begin();
  lcd.begin(20, 4);
  pinMode(buttonPin, INPUT);    //internal pullup isn't strong enough?
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, 0);
  lcd.setCursor(18, 2);
  lcd.print(F("R0"));
  pinMode(blPin, OUTPUT);
  digitalWrite(blPin, HIGH);
  blDelay = millis();
  pinMode(MAX485_DE, OUTPUT);
  digitalWrite(MAX485_DE, 0);
  Serial.begin(115200);
  node.begin(1, Serial);        // Modbus slave ID 1
  // Callbacks allow us to configure the RS485 transceiver correctly
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  
  if (!SD.begin(chipSelect)) {
    lcd.setCursor(0, 0);
    lcd.print(F("SD failed"));
    delay(1000);
    lcd.clear();
    return;
  }
  lcd.setCursor(0, 0);
  lcd.print(F("SD Init"));
  delay(1000);
  lcd.clear();
}

void loop () {
  delay(500);
    
/***** Button Service *****/
  buttonInput = digitalRead(buttonPin);
  if (buttonInput == 0) {
    digitalWrite(blPin, HIGH);
    digitalWrite(relayPin, 0);
    lcd.setCursor(18, 2);
    lcd.print(F("R0"));
    blDelay = millis();
  }
  
/***** Get Time *****/
  DateTime now = RTC.now();
  lcd.setCursor(0, 3);
  lcd.print(now.year());
  lcd.print(F("/"));
  lcd2digits(now.month());
  lcd.print(F("/"));
  lcd2digits(now.day());
  lcd.print(F(" "));
  lcd2digits(now.hour());
  lcd.print(F(":"));
  lcd2digits(now.minute());
  lcd.print(F(":"));
  lcd2digits(now.second());

/***** Data Pull *****/
  resultMain = node.readInputRegisters(0x3100, 6);
  if (resultMain == node.ku8MBSuccess)
  {
    lcd.setCursor(0, 0);
    lcd.print(F("SV:     "));
    solarVolt = (node.getResponseBuffer(0x00) / 100.0f);
    lcd.setCursor(3, 0);
    lcd.print(solarVolt);
    lcd.setCursor(9, 0);
    lcd.print(F("SA:    "));
    solarAmp = (node.getResponseBuffer(0x01) / 100.0f);
    lcd.setCursor(12, 0);
    lcd.print(solarAmp);
    lcd.setCursor(0, 1);
    lcd.print(F("BV:     "));
    battVolt = (node.getResponseBuffer(0x04) / 100.0f);
    lcd.setCursor(3, 1);
    lcd.print(battVolt);
    lcd.setCursor(9, 1);
    lcd.print(F("BA:    "));
    battAmp = (node.getResponseBuffer(0x05) / 100.0f);
    lcd.setCursor(12, 1);
    lcd.print(battAmp);
    /*lcd.print("SW: ");
    solarWatt = ((node.getResponseBuffer(0x02) + node.getResponseBuffer(0x03) << 16)/100.0f);*/
  }

  node.clearResponseBuffer();

  resultMain = node.readInputRegisters(0x3200, 2);
  if (resultMain == node.ku8MBSuccess)
  {
    lcd.setCursor(0, 2);
    lcd.print(F("S:     "));
    chargeMode = (node.getResponseBuffer(0x01));
    lcd.setCursor(2, 2);
    if (chargeMode == B11) {
      lcd.print(F("None"));
    } else if (chargeMode == B111) {
      lcd.print(F("Float"));
    } else if (chargeMode == B1011) {
      lcd.print(F("Boost"));
    } else if (chargeMode == B1111) {
      lcd.print(F("Equal"));
    } else {
      lcd.print(chargeMode, BIN);
    }
  }

  //ChrgStatus = ChrgStatus >> 2;// shift right 2 places to get bits D3 D2 into D1 D0
  //ChrgStatus = ChrgStatus & 3; // and it, bit wise with, 0000000000000011, to get just D1 D0, loose th rest

/***** Relay Control *****/
  if (chargeMode == B111 && now.hour() == relayOn && now.minute() == 0 && now.second() < 10) {
    digitalWrite(relayPin, 1);
    lcd.setCursor(18, 2);
    lcd.print(F("R1"));
  }

  if (now.hour() == relayOff || battVolt < 12) {
    digitalWrite(relayPin, 0);
    lcd.setCursor(18, 2);
    lcd.print(F("R0"));
  }
  
/***** Backlight Timer *****/
  if ((millis() - blDelay) >= 60000) {    // 60 seconds 60000
    digitalWrite(blPin, LOW);
  }

/***** SD Write *****/
  if(now.hour() != currentHour) {
    File dataFile = SD.open("solar.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.print(now.year());
      dataFile.print('/');
      dataFile.print(now.month());
      dataFile.print('/');
      dataFile.print(now.day());
      dataFile.print('-');
      dataFile.print(now.hour());
      dataFile.print(':');
      dataFile.print(now.minute());
      dataFile.print(':');
      dataFile.print(now.second());
      dataFile.print(",");
      dataFile.print(solarVolt);
      dataFile.print(",");
      dataFile.print(solarAmp);
      dataFile.print(",");
      dataFile.print(battVolt);
      dataFile.print(",");
      dataFile.print(battAmp);
      dataFile.print(",");
      dataFile.println(chargeMode, BIN);
      dataFile.close();
    } else {
      lcd.setCursor(18, 0);
      lcd.print(F("SD"));
      lcd.setCursor(18, 1);
      lcd.print(F("XX"));
    }
  currentHour = now.hour();
  }
}

void preTransmission() {
  digitalWrite(MAX485_DE, 1);
}

void postTransmission() {
  digitalWrite(MAX485_DE, 0);
}

void lcd2digits(int number) {
  if (number >= 0 && number < 10) {
    lcd.print('0');
  }
  lcd.print(number);
}
