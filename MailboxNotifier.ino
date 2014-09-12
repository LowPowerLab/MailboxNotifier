// Moteino based MailboxNotifier
// This is the code running on the sensor side
// LowPowerLab.com  -  2013-2-28 (C) felix@lowpowerlab.com
// http://opensource.org/licenses/mit-license.php
// uses the RFM12B library found at: https://github.com/LowPowerLab/RFM12B

#include <RFM12B.h>
#include <avr\sleep.h>
#include <avr\delay.h>
#include <LowPower.h>  //get it here: https://github.com/LowPowerLab/LowPower

#define NETWORKID         100  //what network this node is on
#define NODEID              2  //this node's ID, should be unique among nodes on this NETWORKID
#define GATEWAYID           1  //central node to report data to
#define KEY "ABCDABCDABCDABCD" //(16 bytes of your choice - keep the same on all encrypted nodes)
#define ACK_TIME     50        // # of ms to wait for an ack
#define SENSORREADPERIOD  SLEEP_250MS  //this will cause transmissions every 2s
#define SENDINTERVAL   5000    //interval for sending readings without ACK

#define LEDPIN             9   //pin connected to onboard LED
//#define BLINK_EN             //uncomment this to blink onboard LED every on every sensor reading
                               // WARNING: even though onboard LED is only 2ma, blinking will cause a magnitude higher current consumption
//#define SERIAL_EN            //uncomment this line to enable serial IO debug messages
#define SERIAL_BAUD    115200

#ifdef SERIAL_EN
  #define DEBUG(input)   {Serial.print(input); delay(1);}
  #define DEBUGln(input) {Serial.println(input); delay(1);}
#else
  #define DEBUG(input);
  #define DEBUGln(input);
#endif

RFM12B radio;
char sendBuf[RF12_MAXDATA];
byte sendLen;
byte temperatureCounter = 0;
float temperature = 0;
long doorPulseCount = 0;
unsigned long MLO=0, MLC=0; //MailLastOpen, MailLastClosed (ms)
unsigned long now = 0, lastSend = 0, temp = 0;

void setup(void)
{
  radio.Initialize(NODEID, RF12_915MHZ, NETWORKID);
  radio.Encrypt((uint8_t*)KEY);
  radio.Sleep();
  #ifdef SERIAL_EN
    Serial.begin(SERIAL_BAUD);
    DEBUGln("\nTransmitting...");
  #endif
  pinMode(14, INPUT);
  pinMode(15, OUTPUT);
  Blink(9,10);
  delay(100);
  Blink(9,10);
}

void loop()
{
  byte reading = hallSensorReading();
  temp = millis();
  
  if (reading == 1)
  {
    if (++doorPulseCount == 1)
    {
      MLO = now; //save timestamp of event
      DEBUG("MAIL:O");
      sendLen = 6;
      
      //retry send up to 3 times when door event detected
      for(byte i=1; i<=3; i++)
      {
        radio.Wakeup();  
        radio.Send(GATEWAYID, "MAIL:O", sendLen, 1);
        DEBUG(" - waiting for ACK...");
        if (waitForAck())
        {
          DEBUG("ok!");
          break;
        }
        else DEBUG("nothing...");

        radio.Sleep();
        LowPower.powerDown(SENSORREADPERIOD, ADC_OFF, BOD_ON); //retry every 2s until ACK received
      }
      radio.Sleep();
      DEBUGln();
    }
  }
  else if (doorPulseCount >=1)
  {
    MLC = now; //save timestamp of event
    DEBUG("MAIL:C");
    sendLen = 6;

    //retry send up to 3 times when door event detected
    for(byte i=1; i<=3; i++)
    {
      radio.Wakeup();
      radio.Send(GATEWAYID, "MAIL:C", sendLen, 1);
      DEBUG(" - waiting for ACK...");
      if (waitForAck())
      {
        DEBUG("ok!");
        break;
      }
      else DEBUG("nothing...");

      radio.Sleep();
      LowPower.powerDown(SENSORREADPERIOD, ADC_OFF, BOD_ON); //retry every 2s until ACK received
    }
    radio.Sleep();
    doorPulseCount = 0; //reset counter
    DEBUGln();
  }
  
  //send readings every SENDINTERVAL
  if (now - lastSend > SENDINTERVAL)
  {
    char periodO='X', periodC='X';
    long lastOpened = (now - MLO) / 1000; //get seconds
    long lastClosed = (now - MLC) / 1000; //get seconds
    long LO = lastOpened;
    long LC = lastClosed;
    char* MLOstr="MLO:99d23h59m";
    char* MLCstr="MLC:99d23h59m";
    
    if (lastOpened <= 59) periodO = 's'; //1-59 seconds
    else if (lastOpened <= 3599) { periodO = 'm'; lastOpened/=60; } //1-59 minutes
    else if (lastOpened <= 259199) { periodO = 'h'; lastOpened/=3600; } // 1-71 hours
    else if (lastOpened >= 259200) { periodO = 'd'; lastOpened/=86400; } // >=3 days

    if (lastClosed <= 59) periodC = 's';
    else if (lastClosed <= 3599) { periodC = 'm'; lastClosed/=60; }
    else if (lastClosed <= 259199) { periodC = 'h'; lastClosed/=3600; }
    else if (lastClosed >= 259200) { periodC = 'd'; lastClosed/=86400; }
    
    if (periodO == 'd')
      sprintf(MLOstr, "MLO:%ldd%ldh", lastOpened, (LO%86400)/3600);
    else if (periodO == 'h')
      sprintf(MLOstr, "MLO:%ldh%ldm", lastOpened, (LO%3600)/60);
    else sprintf(MLOstr, "MLO:%ld%c", lastOpened, periodO);

    if (periodC == 'd')
      sprintf(MLCstr, "MLC:%ldd%ldh", lastClosed, (LC%86400)/3600);
    else if (periodC == 'h')
      sprintf(MLCstr, "MLC:%ldh%ldm", lastClosed, (LC%3600)/60);
    else sprintf(MLCstr, "MLC:%ld%c", lastClosed, periodC);
    
    sprintf(sendBuf, "%s %s", MLOstr, MLCstr); //sprintf(sendBuf, "MLO:%ld%c MLC:%ld%c", lastOpened, periodO, lastClosed, periodC);
    sendLen = strlen(sendBuf);
    DEBUG(sendBuf); DEBUG(" ("); DEBUG(sendLen); DEBUGln(")"); 
    
    radio.Wakeup();
    radio.Send(GATEWAYID, sendBuf, sendLen, 0); //0=noACK
    radio.Sleep();
    lastSend = now;
  }
  
  #ifdef BLINK_EN
    Blink(LEDPIN, 5);
  #endif
      
  now = now + 250 + 21 + (millis()-temp); //correct millis(). Add 21ms to compensate time lost in other peripheral code, may need to be tweaked to be accurate
  LowPower.powerDown(SENSORREADPERIOD, ADC_OFF, BOD_ON);
}

// wait up to ACK_TIME for proper ACK, return true if received
static bool waitForAck() {
  long now = millis();
  while (millis() - now <= ACK_TIME)
    if (radio.ACKReceived(GATEWAYID))
      return true;
  return false;
}

byte hallSensorReading()
{
  //BJT transistor on A1 turns power ON/OFF to hall sensor to save power
  digitalWrite(15, 1); //turn sensor ON
  delay(1); //wait a little
  byte reading = digitalRead(14);
  digitalWrite(15, 0); //turn sensor OFF
  return reading;
}

void Blink(byte PIN, byte DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}
