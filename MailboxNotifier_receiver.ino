// Moteino based MailboxNotifier
// This is the code running on the receiver side
// LowPowerLab.com  -  2013-2-28 (C) felix@lowpowerlab.com
// http://opensource.org/licenses/mit-license.php
// uses the RFM12B library found at: https://github.com/LowPowerLab/RFM12B

#include <RFM12B.h>

#define SERIAL_BAUD 115200
#define NETWORKID       100
#define NODEID            1                 // network ID used for this unit

uint8_t input[RF12_MAXDATA];
#define KEY  "ABCDABCDABCDABCD"
RFM12B radio;

void setup()
{
  pinMode(9, OUTPUT);
  radio.Initialize(NODEID, RF12_915MHZ, NETWORKID);
  radio.Encrypt((uint8_t*)KEY);
  Serial.begin(SERIAL_BAUD);
  Serial.println("Listening...");
}

byte recvCount = 0;

void loop()
{
  if (radio.ReceiveComplete())
  {
    if (radio.CRCPass())
    {
      digitalWrite(9,1);
      Serial.print('[');Serial.print(radio.GetSender(), DEC);Serial.print("] ");
      for (byte i = 0; i < *radio.DataLen; i++)
        Serial.print((char)radio.Data[i]);

      if (radio.ACKRequested())
      {
        radio.SendACK();
        Serial.print(" - ACK sent");
      }
      delay(5);
      digitalWrite(9,0);
    }
    else Serial.print("BAD-CRC");
    
    Serial.println();
  }
}
