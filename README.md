MailboxNotifier
----------------
By Felix Rusu (felix@lowpowerlab.com)

###Features:
This is the source code used to make a [MailboxNotifier](http://lowpowerlab.com/?p=519), which consists of a Moteino 
based sensor that detects when your mail is delivered. It uses a hall effect sensor and earth magnet, feeding from a 9V battery.
The receiver is another Moteino that runs the receiver sketch, passing the information to the python script which sends an email/sms message when a door-open event is detected.

To send an sms message you simply have to send an email to your cell number's sms mailbox. Mine is with AT&T so I send an email to mynumber@txt.att.net

##Update:
I improved the hardware for the sensor side, and enclosed the unit in a weather proof box. I also added MailboxNotifier2 sketches for a sender and receiver which is uses structs to send the data and also has the ability to measure battery voltage. The receiving end sketch is for a standalone LCD unit which displays the last open/closed events and battery voltage. More details here:
http://lowpowerlab.com/blog/2013/08/27/mailbox-notifier-project-upgrade/
