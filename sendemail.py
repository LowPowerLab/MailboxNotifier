import serial
import smtplib
import string
import sys

### GENERAL SETTINGS ###
SERIALPORT = "COM100"  # the default com/serial port the receiver is connected to
BAUDRATE = 115200            # default baud rate we talk to Moteino 
SUBJECT = "You got snail mail!"
TO = "yournumber@txt.att.net"
FROM = "monty@python.com"
text = "The mailbox was opened!"
BODY = string.join((
        "From: %s" % FROM,
        "To: %s" % TO,
        "Subject: %s" % SUBJECT ,
        "",
        text
        ), "\r\n")

ser = serial.Serial(SERIALPORT, BAUDRATE, timeout=10)

def sendMail():
  server = smtplib.SMTP('smtp.gmail.com:587')
  server.ehlo()
  server.starttls()
  server.login('you@gmail.com', 'your gmail password or gmail app specific password')
  server.sendmail(FROM, [TO], BODY)
  server.quit()

while True:
  line = ser.readline()
  data = line.rstrip().split()  #no argument = split by whitespace

  print line
  if len(data)>=2:
    for i in range(1, len(data)):
      if data[i]=='MAIL:O':
        print '\n\nGOT MAIL!!!!\n\n'
        sendMail()