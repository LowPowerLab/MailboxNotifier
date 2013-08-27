// Standalone receiver with LCD Moteino based MailboxNotifier
// This is the code running on the receiving end that displays info on LCD
// LowPowerLab.com  -  2013-8-25 (C) felix@lowpowerlab.com
// http://opensource.org/licenses/mit-license.php
// uses the RFM12B library found at: https://github.com/LowPowerLab/RFM12B
// the LCD libraries are found in separate ZIP file (source: https://bitbucket.org/fmalpartida/st7036-display-driver/wiki/Home)

#include <Arduino.h>
#include "ST7036.h"
#include "LCD_C0220BiZ.h"
#include <Wire.h>
#include <RFM12B.h>

#define SERIAL_BAUD  115200
#define NODEID            1                 // network ID used for this unit
#define NETWORKID       100
#define FREQUENCY  RF12_915MHZ //Match this with the version of your Moteino! (others: RF12_433MHZ, RF12_915MHZ)
#define KEY  "ABCDABCDABCDABCD"
#define ACK_TIME         50  // # of ms to wait for an ack
#define LED               9
#define LDRPIN           A0 //light sensor on pin A0 for backlight purposes
#define BACKLIGHTPIN      6

char temp[RF12_MAXDATA];
RFM12B radio;
ST7036 lcd = ST7036 (2, 20, 0x78, BACKLIGHTPIN); //row count, column count, I2C addr, pin for backlight PWM

typedef struct {		
  unsigned long lastOpen;
  unsigned long lastClosed;
  unsigned short battery;
} Payload;
Payload theData;

byte battChar[8] = {0b00000,0b01110,0b11111,0b11111,0b11111,0b11111,0b11111,0};

void setup()
{
  pinMode(LED, OUTPUT);
  radio.Initialize(NODEID, FREQUENCY, NETWORKID);
  radio.Encrypt((byte*)KEY);
  Serial.begin(SERIAL_BAUD);
  char buff[50];
  sprintf(buff, "Listening at %d Mhz...", FREQUENCY == RF12_433MHZ ? 433 : FREQUENCY== RF12_868MHZ ? 868 : 915);
  Serial.println(buff);
  
  lcd.init();
  lcd.setContrast(10);
  lcd.clear();
  
  lcd.load_custom_character(0, battChar);
  
}

byte recvCount = 0;
int milliCount=0;

void loop()
{
  if (radio.ReceiveComplete())
  {
    if (radio.CRCPass())
    {
      digitalWrite(LED,HIGH);
      Serial.print('[');Serial.print(radio.GetSender(), DEC);Serial.print("] ");
      
      if (*radio.DataLen != sizeof(Payload))
        for (byte i = 0; i < *radio.DataLen; i++)
          Serial.print((char)radio.Data[i]);
      else
      {
        theData = *(Payload*)radio.Data; //assume radio.DATA actually contains our struct and not something else
        char periodO='X', periodC='X';
        long lastOpened = theData.lastOpen;
        long lastClosed = theData.lastClosed;
        long LO = lastOpened;
        long LC = lastClosed;
        char* MLOstr="LO:99d23h59m";
        char* MLCstr="LC:99d23h59m";
        char* BATstr="BAT:1024";
        
        if (lastOpened <= 59) periodO = 's'; //1-59 seconds
        else if (lastOpened <= 3599) { periodO = 'm'; lastOpened/=60; } //1-59 minutes
        else if (lastOpened <= 259199) { periodO = 'h'; lastOpened/=3600; } // 1-71 hours
        else if (lastOpened >= 259200) { periodO = 'd'; lastOpened/=86400; } // >=3 days
    
        if (lastClosed <= 59) periodC = 's';
        else if (lastClosed <= 3599) { periodC = 'm'; lastClosed/=60; }
        else if (lastClosed <= 259199) { periodC = 'h'; lastClosed/=3600; }
        else if (lastClosed >= 259200) { periodC = 'd'; lastClosed/=86400; }
        
        if (periodO == 'd')
          sprintf(MLOstr, "LO:%ldd%ldh", lastOpened, (LO%86400)/3600);
        else if (periodO == 'h')
          sprintf(MLOstr, "LO:%ldh%ldm", lastOpened, (LO%3600)/60);
        else sprintf(MLOstr, "LO:%ld%c", lastOpened, periodO);
    
        if (periodC == 'd')
          sprintf(MLCstr, "LC:%ldd%ldh", lastClosed, (LC%86400)/3600);
        else if (periodC == 'h')
          sprintf(MLCstr, "LC:%ldh%ldm", lastClosed, (LC%3600)/60);
        else sprintf(MLCstr, "LC:%ld%c", lastClosed, periodC);
        
        sprintf(BATstr, "BAT:%i", theData.battery);
        
        sprintf(temp, "%s %s %s", MLOstr, MLCstr, BATstr); //sprintf(sendBuf, "MLO:%ld%c MLC:%ld%c", lastOpened, periodO, lastClosed, periodC);
        byte len = strlen(temp);
        Serial.print(temp); Serial.print(" ("); Serial.print(len); Serial.print(")"); 
        
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(MLOstr);
        lcd.setCursor(1,0);
        lcd.print(MLCstr);

        float battV = ((float)theData.battery * 3.3 * 9)/(1023*2.8776);
        dtostrf(battV, 3, 2, BATstr);

        Serial.print(BATstr);Serial.print("v");

        //sprintf(temp, "BAT:%sv", BATstr);
        //lcd.setCursor(0,11);
        //lcd.print(temp);
        
        lcd.setCursor(0,14);
        lcd.print(char(0));
        lcd.setCursor(0,15);
        sprintf(temp, "%sv", BATstr);
        lcd.print(temp);
      }

      if (radio.ACKRequested())
      {
        byte theNodeID = radio.GetSender();
        radio.SendACK();
      }
      delay(5);
      digitalWrite(LED,LOW);
    }
    else Serial.print("BAD-CRC");
    
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

// wait a few milliseconds for proper ACK to me, return true if indeed received
static bool waitForAck(byte theNodeID) {
  long now = millis();
  while (millis() - now <= ACK_TIME) {
    if (radio.ACKReceived(theNodeID))
      return true;
  }
  return false;
}
