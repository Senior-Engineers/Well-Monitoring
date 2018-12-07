# Well-Monitoring
How to monitor a Hand Pumped Wells in a Rural Location using Point-to-point Communication via GSM SMS
Introduction

There is a need to monitor hand pumped wells in rural locations with out any connection to the water pumped or mechanical connection to the pump. In this rural location there is limited infrastructure but there is a GSM Service, so it was decided to monitor the Well Pumps by means of an Arduino Microprocessor with an attached GSM Telephone Module (SIM800L).  Each Well Head Module transmits a Text SMS to a Base Station Module via the available Network Service Provider.

The system  currently exists as prototype modules that will be trialled in their rural locations early in 2019.

The aim is to maintain a web site that display the last fourteen days of each monitored well’s activity on a Web Site. The Trials Data can be viewed at http://www.wellmonitoringservice.co.uk/ (follow the Maps trail).

Well Head Module

The Well Head Module uses an Arduino Microprocessor with a Doppler Radar and GSM Telephone Modules connected. It is powered by a NiMH Battery that is recharged by a Solar Cell. The Arduino monitors the activity around the Well by means of the Doppler Radar, keeping a count of the number of 30 Second intervals in which activity is detected. By assuming the distribution of Men, Women and Children, this number can be converted into Litres by using an EXCEL Worksheet.

The Arduino monitors the output of the solar cell; on an interrupt as voltage is detected (sun shining), the “Day Number” is incremented by One; on an interrupt when no voltage is detected, a SMS is sent.

The SMS contains the “Doppler Radar Activity Number”, the “Day Number” and other engineering data.

Base Station Module

The Base Station Module uses an Arduino Microprocessor with a GSM Telephone Module connected. It is Mains powered and is connected to a Workstation by a USB Cable.

The Base Station responds to commands serially transmitted over the USB Serial Link, with the reply being returned. 

The Base Station can be controlled by using a “Serial Terminal Emulator”, but the team have developed an EXCEL Workbook (enhanced with Visual Basic Code) to automate this activity. This Workbook must be adapted for each individual application to cater for: 

I.	FTP Account Details for the Website Server
II.	Mobile Telephone numbers of all the Well Head Modules
III.	The Number of Base Stations controlled by the Workbook
IV.	Details of the text to send to receive a Balance Enquire; how to deal with the Balance Text (With the SIMs used in the Prototype, it is necessary to send a SMS at least once every three months to maintain the Service).

If you would like to discuss how the EXCEL Workbook could help you, please contact the team at well.monitoring.team@outlook.com 



This is only an overview, please look at the other documentation that has been uploaded. 
