// Sample RFM69 sender/node sketch for mailbox motion sensor, with addition of
//   Moteino WeatherShield for temp/humidity/atm pressure readings
// http://www.lowpowerlab.com/mailbox
// PIR motion sensor connected to D3 (INT1)
// When RISE happens on D3, the sketch transmits a "MOTION" msg to receiver Moteino and goes back to sleep
// It then wakes up every 32 seconds and sends a message indicating when the last
//    motion event happened (days, hours, minutes, seconds ago) and the battery level
// In sleep mode, Moteino + PIR motion sensor use about ~78uA
// Make sure you adjust the settings in the configuration section below !!!
// **********************************************************************************
// Copyright Felix Rusu, LowPowerLab.com
// Library and code by Felix Rusu - felix@lowpowerlab.com
// Get the RFM69 and SPIFlash library at: https://github.com/LowPowerLab/
// **********************************************************************************
// License
// **********************************************************************************
// This program is free software; you can redistribute it 
// and/or modify it under the terms of the GNU General    
// Public License as published by the Free Software       
// Foundation; either version 3 of the License, or        
// (at your option) any later version.                    
//                                                        
// This program is distributed in the hope that it will   
// be useful, but WITHOUT ANY WARRANTY; without even the  
// implied warranty of MERCHANTABILITY or FITNESS FOR A   
// PARTICULAR PURPOSE. See the GNU General Public        
// License for more details.                              
//                                                        
// You should have received a copy of the GNU General    
// Public License along with this program.
// If not, see <http://www.gnu.org/licenses/>.
//                                                        
// Licence can be viewed at                               
// http://www.gnu.org/licenses/gpl-3.0.txt
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code
// **********************************************************************************
#include <SFE_BMP180.h>    //get it here: https://github.com/LowPowerLab/SFE_BMP180
#include <SI7021.h>        //get it here: https://github.com/LowPowerLab/SI7021
#include <Wire.h>

#include <RFM69.h>    //get it here: https://github.com/LowPowerLab/RFM69
#include <SPI.h>      //get it here: https://github.com/LowPowerLab/SPIFlash
#include <LowPower.h> //get library from: https://github.com/LowPowerLab/LowPower
                      //writeup here: http://www.rocketscream.com/blog/2011/07/04/lightweight-low-power-arduino-library/

//*********************************************************************************************
// *********** IMPORTANT SETTINGS - YOU MUST CHANGE/ONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
#define NODEID            57    //unique for each node on same network
#define NETWORKID         250  //the same on all nodes that talk to each other
#define GATEWAYID         1
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
//#define FREQUENCY         RF69_433MHZ
//#define FREQUENCY         RF69_868MHZ
#define FREQUENCY         RF69_915MHZ
#define IS_RFM69HW        //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ENCRYPTKEY        "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
#define SENDEVERYXLOOPS   4 //each loop sleeps 8 seconds, so send status message every this many sleep cycles (default "4" = 32 seconds)
//*********************************************************************************************
#define MOTION_PIN     3  // D3
#define MOTION_IRQ     1  // hardware interrupt 1 (D3) - where motion sensors OUTput is connected, this will generate an interrupt every time there is MOTION
#define BATT_MONITOR_EN A3//enables battery voltage divider to get a reading from a battery, disable it to save power
#define BATT_MONITOR  A7  //through 1Meg+470Kohm and 0.1uF cap from battery VCC - this ratio divides the voltage to bring it below 3.3V where it is scaled to a readable range
#define BATT_CYCLES   30  // read and report battery voltage every this many sleep cycles (ex 30cycles * 8sec sleep = 240sec/4min). For 450 cyclesyou would get ~1 hour intervals
#define BATT_FORMULA(reading) reading * 0.00322 * 1.47
#define LED           5  // Moteinos have LEDs on D9
//#define BLINK_EN         //uncomment to make LED flash when messages are sent, leave out if you want low power

//#define SERIAL_EN      //uncomment this line to enable serial IO debug messages, leave out if you want low power
#ifdef SERIAL_EN
  #define SERIAL_BAUD   115200
  #define DEBUG(input)   {Serial.print(input); delay(1);}
  #define DEBUGln(input) {Serial.println(input); delay(1);}
#else
  #define DEBUG(input);
  #define DEBUGln(input);
#endif

SI7021 weatherShield_SI7021;
SFE_BMP180 weatherShield_BMP180;
RFM69 radio;
volatile boolean motionDetected=false;
char sendBuf[61];
byte sendLen;
byte sendLoops=0;
unsigned long MLO=0; //MailLastOpen (ago, in ms)
unsigned long now = 0, time=0, lastSend = 0, temp = 0;
float batteryVolts = 5;
char* BATstr="BAT:5.00v";
char* Pstr = "123.45";

void setup() {
#ifdef SERIAL_EN
  Serial.begin(SERIAL_BAUD);
#endif  
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);
  radio.sleep();
  pinMode(MOTION_PIN, INPUT);
  pinMode(BATT_MONITOR, INPUT);
  attachInterrupt(MOTION_IRQ, motionIRQ, RISING);
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  DEBUGln(buff);

  //initialize weather shield sensors  
  weatherShield_SI7021.begin();
  if (weatherShield_BMP180.begin())
  { DEBUGln("BMP180 init success"); }
  else { DEBUGln("BMP180 init fail\n"); }

  checkBattery();
}

void motionIRQ()
{
  motionDetected=true;
}

void loop() {
  now = millis();

  if (motionDetected && time-MLO > 30000) //avoid duplicates in 30second intervals (ie mailman sometimes spends 20+ seconds at mailbox)
  {
    DEBUG("MOTION..");
    MLO = time; //save timestamp of event
    sprintf(sendBuf, "MOTION BAT:%sv", BATstr);
    sendLen = strlen(sendBuf);
    if (radio.sendWithRetry(GATEWAYID, sendBuf, sendLen))
    {
     DEBUGln(" ok!");
     #ifdef BLINK_EN
       Blink(LED,3);
     #endif
    }
    else DEBUGln(" nothing...");
    radio.sleep();
  }
  else sendLoops++;
  
  //send readings every SENDEVERYXLOOPS
  if (sendLoops>=SENDEVERYXLOOPS)
  {
    checkBattery();
    sendLoops=0;
    char periodO='X', periodC='X';
    unsigned long lastOpened = (time - MLO) / 1000; //get seconds
    unsigned long LO = lastOpened;
    char* MLOstr="LO:99d23h59m";
    char status;
    
    if (lastOpened <= 59) periodO = 's'; //1-59 seconds
    else if (lastOpened <= 3599) { periodO = 'm'; lastOpened/=60; } //1-59 minutes
    else if (lastOpened <= 259199) { periodO = 'h'; lastOpened/=3600; } // 1-71 hours
    else if (lastOpened >= 259200) { periodO = 'd'; lastOpened/=86400; } // >=3 days

    if (periodO == 'd')
      sprintf(MLOstr, "LO:%ldd%ldh", lastOpened, (LO%86400)/3600);
    else if (periodO == 'h')
      sprintf(MLOstr, "LO:%ldh%ldm", lastOpened, (LO%3600)/60);
    else sprintf(MLOstr, "LO:%ld%c", lastOpened, periodO);
    
    double P = getPressure();
    P*=0.0295333727; //transform to inHg
    dtostrf(P, 3,2, Pstr);
    
    sprintf(sendBuf, "%s BAT:%sv F:%d H:%d P:%s", MLOstr, BATstr, weatherShield_SI7021.getFahrenheitHundredths(), weatherShield_SI7021.getHumidityPercent(), Pstr);
    sendLen = strlen(sendBuf);
    radio.send(GATEWAYID, sendBuf, sendLen);
    radio.sleep();
    DEBUG(sendBuf); DEBUG(" ("); DEBUG(sendLen); DEBUGln(")"); 
    lastSend = time;
    #ifdef BLINK_EN
      Blink(LED, 5);
    #endif
  }

  motionDetected=false;
  time = time + 8000 + millis()-now + 480; //correct millis() resonator drift, may need to be tweaked to be accurate
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  DEBUGln("WAKEUP");
}

void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}

byte cycleCount=BATT_CYCLES;
void checkBattery()
{
  if (cycleCount++ == BATT_CYCLES) //only read battery every BATT_CYCLES sleep cycles
  {
    unsigned int reading=0;
    
    //enable battery monitor
    pinMode(BATT_MONITOR_EN, OUTPUT);
    digitalWrite(BATT_MONITOR_EN, LOW);
    
    for (byte i=0; i<10; i++)
      reading += analogRead(BATT_MONITOR);
    
    //disable battery monitor
    pinMode(BATT_MONITOR_EN, INPUT); //highZ mode will allow p-mosfet to be pulled high and disconnect the voltage divider on the weather shield
    
    batteryVolts = BATT_FORMULA(reading/10);
    DEBUG("reading:"); DEBUGln(reading);
    dtostrf(batteryVolts, 3,2, BATstr);
    cycleCount = 0;
  }
}

double getPressure()
{
  char status;
  double T,P,p0,a;
  // If you want sea-level-compensated pressure, as used in weather reports,
  // you will need to know the altitude at which your measurements are taken.
  // We're using a constant called ALTITUDE in this sketch:
  
  // If you want to measure altitude, and not pressure, you will instead need
  // to provide a known baseline pressure. This is shown at the end of the sketch.
  // You must first get a temperature measurement to perform a pressure reading.
  // Start a temperature measurement:
  // If request is successful, the number of ms to wait is returned.
  // If request is unsuccessful, 0 is returned.
  status = weatherShield_BMP180.startTemperature();
  if (status != 0)
  {
    // Wait for the measurement to complete:
    delay(status);

    // Retrieve the completed temperature measurement:
    // Note that the measurement is stored in the variable T.
    // Function returns 1 if successful, 0 if failure.
    status = weatherShield_BMP180.getTemperature(T);
    if (status != 0)
    {
      // Start a pressure measurement:
      // The parameter is the oversampling setting, from 0 to 3 (highest res, longest wait).
      // If request is successful, the number of ms to wait is returned.
      // If request is unsuccessful, 0 is returned.
      status = weatherShield_BMP180.startPressure(3);
      if (status != 0)
      {
        // Wait for the measurement to complete:
        delay(status);

        // Retrieve the completed pressure measurement:
        // Note that the measurement is stored in the variable P.
        // Note also that the function requires the previous temperature measurement (T).
        // (If temperature is stable, you can do one temperature measurement for a number of pressure measurements.)
        // Function returns 1 if successful, 0 if failure.
        status = weatherShield_BMP180.getPressure(P,T);
        if (status != 0)
        {
          return P;
        }
      }
    }        
  }
  return 0;
}
