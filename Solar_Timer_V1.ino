// Solar Timer by Chiu Fang 2018

// Include Libraries
#include <Wire.h>              // For DS1307 Real Time Clock
#include <DS3231.h>
#include <LiquidCrystalFast.h> // For 16x1 LCD Display
//#include <Keypad.h>            // For Keypad
//#include <EEPROMex.h>          // For EEPROM
#include <SPI.h>
#include <SD.h>
#include <ModbusMaster.h>

// Set constants
const byte blPin = 9;          // Backlight pin
                               // Pin A4 → I2C SDA, Pin A5 → I2C SCL
const byte chipSelect = 10;
const byte MAX485_DE = 14;
const byte buttonPin = 15;
const byte relayPin = 16;
LiquidCrystalFast lcd (2, 3, 4, 5, 6, 7, 8); // rs,rw,en1,d4,d5,d6,d7 Init LCD
/*const byte ROWS = 1; // Keypad one rows
const byte COLS = 2; // Keypad four columns
char keys[ROWS][COLS] = {
  {'1','2'}
};
byte rowPins[ROWS] = {15}; // Row pinouts of the keypad
byte colPins[COLS] = {17, 16}; // Column pinouts of the keypad (A0, A1, A2, A3)
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS ); // Initialize keypad
*/
ModbusMaster node;
RTClib RTC;

// Declare Variables
unsigned long blDelay = 0;
//char key;
byte currentHour = 0;
unsigned int resultMain;
bool buttonInput = 0;
float solarVolt = 0;
float battVolt = 0;
float solarAmp = 0;
float battAmp = 0;
unsigned int chargeMode;
byte relayOn = 18;  //time to turn relay on
byte relayOff = 20;  //time to turn relay off

void setup () {
  Wire.begin();
  lcd.begin(20, 4);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(blPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, 0);
  pinMode(MAX485_DE, OUTPUT);
  digitalWrite(MAX485_DE, 0);
  digitalWrite(blPin, HIGH);
  blDelay = millis();
  Serial.begin(115200);
  node.begin(1, Serial);  // Modbus slave ID 1
  // Callbacks allow us to configure the RS485 transceiver correctly
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  //relayOn = EEPROM.read(0);
  //relayOff = EEPROM.read(1);
  
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
    digitalWrite(relayPin, 0);
    lcd.setCursor(17, 2);
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
    lcd.print(F("S:                "));
    chargeMode = (node.getResponseBuffer(0x01));
    lcd.setCursor(2, 2);
    lcd.print(chargeMode, BIN);
  }

/***** Relay Control *****/
  if (chargeMode == B111 && now.hour() == relayOn && now.minute() == 0 && now.second() < 10)
  {
    digitalWrite(relayPin, 1);
    lcd.setCursor(17, 2);
    lcd.print(F("R1"));
  }

  if (now.hour() == relayOff)
  {
    digitalWrite(relayPin, 0);
    lcd.setCursor(17, 2);
    lcd.print(F("R0"));
  }
  
/***** Backlight Timer *****/
  if ((millis() - blDelay) >= 10000000){    // 60 seconds 60000
    digitalWrite(blPin, LOW);
  }

/***** SD Write *****/
  if(now.hour() != currentHour) {
    File dataFile = SD.open("solar.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.print(now.year());
      dataFile.print(',');
      dataFile.print(now.month());
      dataFile.print(',');
      dataFile.print(now.day());
      dataFile.print(',');
      dataFile.print(now.hour());
      dataFile.print(',');
      dataFile.print(now.minute());
      dataFile.print(',');
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

/***** Menu System *****/
/*  key = keypad.getKey();
  if(key) {
    digitalWrite(blPin, HIGH);
    switch(key) {
      
      case '1':
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("1 = Set On Hour"));
        lcd.setCursor(0, 1);
        lcd.print(F("2 = Set Off Hour"));
        key = keypad.waitForKey();
        if(key == '1') {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(F("Set On Hour"));
          lcd.setCursor(0, 1);
          lcd.print(F("Press 1 to increment"));
          lcd.setCursor(0, 2);
          lcd.print(F("Press 2 to set"));
          lcd.setCursor(0, 3);
          lcd.print(relayOn);
          do {
            key = keypad.waitForKey();
            if(key == '1' && relayOn < 23) {
              relayOn++;
              lcd.setCursor(0, 3);
              lcd.print(F("   "));
              lcd.setCursor(0, 3);
              lcd.print(relayOn);
            } else if(key == '1' && relayOn == 23) {
              relayOn = 0;
              lcd.setCursor(0, 3);
              lcd.print(F("   "));
              lcd.setCursor(0, 3);
              lcd.print(relayOn);
            }
          } while(key != '2');
          EEPROM.updateByte(0,relayOn);
          blDelay = millis();
          break;
        }
        
        if(key == '2') {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(F("Set Off Hour"));
          lcd.setCursor(0, 1);
          lcd.print(F("Press 1 to increment"));
          lcd.setCursor(0, 2);
          lcd.print(F("Press 2 to set"));
          lcd.setCursor(0, 3);
          lcd.print(relayOff);
          do {
            key = keypad.waitForKey();
            if(key == '1' && relayOff < 23) {
              relayOff++;
              lcd.setCursor(0, 3);
              lcd.print(F("   "));
              lcd.setCursor(0, 3);
              lcd.print(relayOff);
            } else if(key == '1' && relayOff == 23) {
              relayOff = 0;
              lcd.setCursor(0, 3);
              lcd.print(F("   "));
              lcd.setCursor(0, 3);
              lcd.print(relayOff);
            }
          } while(key != '2');
          EEPROM.updateByte(1,relayOff);
          blDelay = millis();
          break;
        }
      
      case '2':
      blDelay = millis();
      break;
    }
  }
  */
}

void preTransmission()
{
  //digitalWrite(MAX485_RE_NEG, 1);
  digitalWrite(MAX485_DE, 1);
}

void postTransmission()
{
  //digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);
}

void lcd2digits(int number) {
  if (number >= 0 && number < 10) {
    lcd.print('0');
  }
  lcd.print(number);
}
