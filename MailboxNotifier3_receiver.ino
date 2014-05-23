// Standalone receiver with LCD Moteino based MailboxNotifier - RFM69 RECEIVER
// This is the code running on the sensor side
// LowPowerLab.com  -  2013-2-28 (C) felix@lowpowerlab.com
// http://opensource.org/licenses/mit-license.php
// uses the RFM12B library found at: https://github.com/LowPowerLab/RFM12B
// the LCD libraries are found in separate ZIP file (source: https://bitbucket.org/fmalpartida/st7036-display-driver/wiki/Home)

#include <Arduino.h>
#include "ST7036.h"
#include "LCD_C0220BiZ.h"
#include <Wire.h>
#include <RFM69.h>
#include <SPI.h>

#define NODEID      1
#define NETWORKID   100
#define FREQUENCY   RF69_915MHZ //Match this with the version of your Moteino! (others: RF69_433MHZ, RF69_868MHZ)
#define KEY         "sampleEncryptKey" //has to be same 16 characters/bytes on all nodes, not more not less!
#define IS_RFM69HW        //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ACK_TIME          30 //of ms to wait for an ack


#define LED               9
#define LDRPIN           A0 //light sensor on pin A0 for backlight purposes
#define BACKLIGHTPIN      6
#define SERIAL_BAUD  115200

char LO[20];
char LC[20];
int BAT=0;
char BATstr[20];
char temp[25];
RFM69 radio;
ST7036 lcd = ST7036(2, 20, 0x78, BACKLIGHTPIN); //row count, column count, I2C addr, pin for backlight PWM

byte battChar[8] = {0b00000,0b01110,0b11111,0b11111,0b11111,0b11111,0b11111,0};
byte rssiChar[8] = {0b00000,0b00100,0b10101,0b01110,0b00100,0b00100,0b00100,0};

void setup()
{
  pinMode(LED, OUTPUT);
  
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.encrypt(KEY);
  radio.sleep();
  
  Serial.begin(SERIAL_BAUD);
  char buff[50];
  sprintf(buff, "Listening @ %d Mhz", FREQUENCY == RF69_433MHZ ? 433 : FREQUENCY== RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  
  lcd.init();
  lcd.setContrast(10);
  lcd.clear();
  lcd.load_custom_character(0, battChar);
  lcd.load_custom_character(1, rssiChar);
  lcd.setCursor(0,0);
  lcd.print(buff);
}

byte recvCount = 0;
int milliCount=0;

void loop()
{
  if (radio.receiveDone())
  {
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
    for (byte i = 0; i < radio.DATALEN; i++)
      Serial.print((char)radio.DATA[i]);
    Serial.print("   [RX_RSSI:");Serial.print(radio.RSSI);Serial.print("]");
    
    byte matches = sscanf((const char*)radio.DATA, "%s %s BAT:%u", LO, LC, &BAT);
    if (matches==3)
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(LO);
      lcd.setCursor(1,0);
      lcd.print(LC);

      float battV = ((float)BAT * 3.3 * 9)/(1023*2.8776);
      dtostrf(battV, 3, 2, BATstr);
      
      lcd.setCursor(0,14);
      lcd.print(char(0));
      lcd.setCursor(0,15);
      sprintf(temp, "%sv", BATstr);
      lcd.print(temp);
      
      lcd.setCursor(1,14);
      lcd.print(char(1));
      lcd.setCursor(1,16);
      lcd.print(radio.RSSI);
      
      //Serial.print(BATstr);Serial.print("v");
      Blink(LED,5);
    }

    if (radio.ACK_REQUESTED)
    {
      byte theNodeID = radio.SENDERID;
      radio.sendACK();
      Serial.print(" - ACK sent.");
    }
    
    Serial.println();
  }
  
  //update backlight every second
  if (millis() - milliCount > 1000)
  {
    milliCount = millis();
    int LDR = analogRead(LDRPIN);
    lcd.setBacklight(map(LDR, 0, 1024, 0, 255));
    
    lcd.setCursor(1,19);
  }
}

void Blink(byte PIN, byte DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}
