// Moteino based MailboxNotifier - RFM69 TRANSMITTER
// This is the code running on the sensor side
// LowPowerLab.com  -  2013-2-28 (C) felix@lowpowerlab.com
// http://opensource.org/licenses/mit-license.php
// uses the RFM12B library found at: https://github.com/LowPowerLab/RFM12B

#include <RFM69.h>
#include <SPI.h>
#include <avr\sleep.h>
#include <avr\delay.h>
#include <LowPower.h> //get library from: https://github.com/rocketscream/Low-Power
                      //writeup here: http://www.rocketscream.com/blog/2011/07/04/lightweight-low-power-arduino-library/


#define NODEID            9
#define NETWORKID         100
#define GATEWAYID         1
#define FREQUENCY         RF69_915MHZ //Match this with the version of your Moteino! (others: RF69_433MHZ, RF69_868MHZ)
#define KEY               "sampleEncryptKey" //has to be same 16 characters/bytes on all nodes, not more not less!
//#define IS_RFM69HW        //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ACK_TIME          30        // # of ms to wait for an ack

#define LED               9
#define SENSORREADPERIOD  SLEEP_500MS
#define SENDINTERVAL      16000 //interval for sending readings without ACK
#define HALLSENSOR        A0
#define HALLSENSOR_EN     A1
#define BATTERYSENSE      A2

#define SERIAL_BAUD 115200
//#define SERIAL_EN            //uncomment this line to enable serial IO debug messages
//#define BLINK_EN             //uncomment this to blink onboard LED every on every sensor reading
                               // WARNING: even though onboard LED is only 2ma, blinking will cause a magnitude higher current consumption

#ifdef SERIAL_EN
  #define DEBUG(input)   {Serial.print(input); delay(1);}
  #define DEBUGln(input) {Serial.println(input); delay(1);}
#else
  #define DEBUG(input);
  #define DEBUGln(input);
#endif

typedef struct {
  unsigned long lastOpen;
  unsigned long lastClosed;
  unsigned short battery;
} Payload;
Payload theData;

RFM69 radio;
char sendBuf[32];
byte sendLen;
byte temperatureCounter = 0;
float temperature = 0;
long doorPulseCount = 0;
unsigned long MLO=0, MLC=0; //MailLastOpen, MailLastClosed (ms)
unsigned long now = 0, lastSend = 0, temp = 0;

void setup(void)
{
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  radio.encrypt(KEY);
  radio.sleep();
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  #ifdef SERIAL_EN
    Serial.begin(SERIAL_BAUD);
    DEBUGln("\nTransmitting...");
  #endif
  pinMode(A0, INPUT);
  pinMode(A1, OUTPUT);
  Blink(LED,5);
  delay(100);
  Blink(LED,5);
}

void loop()
{
  byte reading = hallSensorReading();
  int batteryReading = analogRead(BATTERYSENSE);
  temp = millis();
  
  if (reading == 1)
  {
    if (++doorPulseCount == 1)
    {
      MLO = now; //save timestamp of event
      //retry send up to 3 times when door event detected
      DEBUG("OPEN! - ACK ... ");
      //radio.wakeup();
      if (radio.sendWithRetry(GATEWAYID, "MAIL:OPN", 8))
      { DEBUG("OK!"); }
      else DEBUG("nothing...");
      DEBUGln();
      //radio.sleep();
      LowPower.powerDown(SENSORREADPERIOD, ADC_OFF, BOD_ON);
    }
  }
  else if (doorPulseCount >=1)
  {
    MLC = now; //save timestamp of event
    //retry send up to 3 times when door event detected
    DEBUG("CLOSED! - ACK ... ");
    //radio.wakeup();
    if (radio.sendWithRetry(GATEWAYID, "MAIL:CLS", 8))
    { DEBUG("OK!"); }
    else DEBUG("nothing...");
    DEBUGln();
    radio.sleep();
    LowPower.powerDown(SENSORREADPERIOD, ADC_OFF, BOD_ON);
    doorPulseCount = 0; //reset counter
  }
  
  //send readings every SENDINTERVAL
  if (abs(now - lastSend) > SENDINTERVAL)
  {
    char periodO='X', periodC='X';
    unsigned long lastOpened = (now - MLO) / 1000; //get seconds
    unsigned long lastClosed = (now - MLC) / 1000; //get seconds
    unsigned long LO = lastOpened;
    unsigned long LC = lastClosed;
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
    
    sprintf(BATstr, "BAT:%i", batteryReading);
    
    sprintf(sendBuf, "%s %s %s", MLOstr, MLCstr, BATstr); //sprintf(sendBuf, "MLO:%ld%c MLC:%ld%c", lastOpened, periodO, lastClosed, periodC);
    
    DEBUG(sendBuf); DEBUG(" ("); DEBUG(sendLen); DEBUGln(")"); 
    
    theData.lastOpen = LO;
    theData.lastClosed = LC;
    theData.battery = batteryReading;
    
    //radio.wakeup();
    radio.send(GATEWAYID, sendBuf, sendLen);
    radio.sleep();
    lastSend = now;
  }
  
  #ifdef BLINK_EN
    Blink(LEDPIN, 5);
  #endif
      
  now = now + 500 + 22 + (millis()-temp); //correct millis() drift. Add 22ms to compensate time lost in other peripheral code, may need to be tweaked to be accurate
  LowPower.powerDown(SENSORREADPERIOD, ADC_OFF, BOD_ON);
}

byte hallSensorReading()
{
  digitalWrite(HALLSENSOR_EN, 1); //turn sensor ON
  delay(1); //wait a little
  byte reading = digitalRead(HALLSENSOR);
  digitalWrite(HALLSENSOR_EN, 0); //turn sensor OFF
  return reading;
}

void Blink(byte PIN, byte DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}
