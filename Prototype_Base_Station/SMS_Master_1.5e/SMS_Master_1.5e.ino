#include <SoftwareSerial.h>     //this is necessary for communication with the Sim800l Module 
#include <Wire.h>               //this is necessary for communication with the Liquid Crystal Display on I2C
#include <LiquidCrystal_I2C.h>  //this is necessary controlling the Liquid Crystal Display

// set the LCD address to 0x27 address; 20 chars and 4 line display 
LiquidCrystal_I2C lcd(0x27,20,4);  

/*______________________________________________________________________

corrected coding for "measuring phone battery voltage" as it incorrectly read when 100%

Prototype program for hardware SMS Master Version 1.5b, Software Version 1.5.e (Prototype Release - Stage 1)
  26th April 2017 to 10th February 2019
  System Engineering by Andrew Ainger, FIET
  Hardware Engineering by Maurice Smith MBE
  Code written by Maurice Smith MBE

  this sketch has tested and found to be functional with SIM800L Software Version 1418B04SIM800L24
     
  This software is licensed under Creative Commons BY-SA 4.0, http://creativecommons.org/licenses/by-sa/4.0/

  AIMS:

  To operate in a standalone manner and respond to the following serially received commands:
  
  1.  ACK?
      Reply with OK(number of the Base Station). This is to enable several Base Stations to correctly function in different Workbooks on one PC
  2.  VERIFY()
      Return value is "'status','subscriber number','battery status','rssi','ber'
  3.  FIND(n)
      Where 'n' is the starting number of the SMS that is to be processed (range limited to start plus five)
      Return value is the SMS details from the Phone Module
  4.  CLEAR(n)
      The Phone's memory will be cleared of the SMS related to the passed parameter
  5.  SEND(telNo,text)
      Send a text to the telephone number with the text as detailed in the passed parameters 
  6.  NETLIGHT?
      Returns to current status of the Netlight; 1=On & 0=OFF
  7.  NETLIGHT(n)
      Control the function of the SIM800L Module's Status Light, where n=1 will enable the Status Lamp and n=0 disable the Status Light
  8.  COUNT()
      Returns the number of messages stored. Due to internal constrains, the response is only accurate below the number of 20.
      If the number read is '20' then the separator is changed from '=' to '>'
  9.  DISPLAY(line,text)
      Where:
        'line' is which line the text will be displayed; range is 0 to 3
        'text' is the text to be displayed; text is truncated at 20 characters
 10.  LINE_CLEAR(line)
      This will cause the passed line numer to be cleared
 11.  ALL_CLEAR()
      This will clear all the display
___________________________________________________
*/
// constants
const int BaseNo = 1;               // used to hold the number of the Base Station (1 to 4 only)

// UNO pins
int PhoneRX = 8;                    // the pin to which the SIM800L Module's RX connection
int PhoneTX = 7;                    // the pin to which the SIM800L Module's TX connection
byte PhoneVoltage = A3;             // pin number that is used to read the value of the Phone Voltage

// flags
int verifyCommand;                  // used to hold the return value when checking a command

// variables
String command;                     // used to hold the string received from the Serial Input
String ATstring;                    // used to hold the string received from the SIM800L Module
String message;                     // used to hold the message being prepared
String phoneStatus;                 // used to hold the status word
String SubscriberNo;                // used to hold the subscriber number
String battery;                     // used to hold the battery voltage in millivolts
String signalRSSI;                  // used to hold the RSSI (acceptable values are 1 to 30 & 99)
String signalBER;                   // used to hold the BER (acceptable values are 0 to 7 & 99)
String networkStatus;               // used to hold the status of the network
String MobileNumber;                // used to hold the mobile number to send the text to
String MobileText;                  // used to hold the text to be sent
String passedNumber;                // used to hold the value of the number that was passed to a function
String lineNo;                     // used to hold the value of the line number that was passed to a function

int openingBracket;                 // used to hold the position of the opening bracket in the string
int closingBracket;                 // used to hold the position of the closing bracket in the string
int intComma;                       // used to hold the position of the comma in the string
int intColon;                       // used to hold the position of the colon in the string
int counter1;                       // used as a counter

char readChar;                      // used to hold the character read from a string

//______________________________________________________________________
   
void setup() {
  // put your setup code here, to run once:

  // set up serial link to host Workstation
  Serial.begin(9600);
  
  // set the built in LED to LOW (normally off)
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,LOW);

  // reserve space for the reply from the SIM800L Module
  ATstring.reserve(254);

  // initialize the lcd 
  lcd.init(); 
   
  // open the backlight
  lcd.backlight();  
}
//______________________________________________________________________
void loop() {
  // put your main code here, to run repeatedly:
 
  // read string and convert to Upper Case
  command  = Serial.readString();
  command.toUpperCase();
  
  // check for the ACK? command
  verifyCommand = (command .startsWith("ACK?"));
  if (verifyCommand == 1) replyACK();

  // check for the Verify command
  verifyCommand = (command .startsWith("VERIFY"));
  if (verifyCommand == 1) verifyConnect();

  // check for the Process SMS command
  verifyCommand = (command .startsWith("FIND"));
  if (verifyCommand == 1) findSms();

  // check for the Clear SMS command
  verifyCommand = (command .startsWith("CLEAR"));
  if (verifyCommand == 1) clearSms();

  // check for the Send SMS command
  verifyCommand = (command .startsWith("SEND"));
  if (verifyCommand == 1) sendText();
  
  // check for the Netlight? command
  verifyCommand = (command .startsWith("NETLIGHT?"));
  if (verifyCommand == 1) showNetlightStatus();
  
  // check for the Netlight(n) command
  verifyCommand = (command .startsWith("NETLIGHT("));
  if (verifyCommand == 1) setNetlight();
  
  // check for the COUNT command
  verifyCommand = (command .startsWith("COUNT"));
  if (verifyCommand == 1) count();

  // check for the DISPLAY command
  verifyCommand = (command .startsWith("DISPLAY"));
  if (verifyCommand == 1) displayText();

  // check for the LINE_CLEAR command
  verifyCommand = (command .startsWith("LINE_CLEAR"));
  if (verifyCommand == 1) clearLine();

  // check for the ALL_CLEAR command
  verifyCommand = (command .startsWith("ALL_CLEAR"));
  if (verifyCommand == 1) clearAll();
}
//______________________________________________________________________
void talkToPhone(String ATcommand) {
  
  // setup communications with the SIM800L
  SoftwareSerial SIM800L =  SoftwareSerial(PhoneRX, PhoneTX);
  SIM800L.begin(9600);

  // send command
  SIM800L.println(ATcommand);
  SIM800L.flush();
  
  // get reply
  ATstring = SIM800L.readString();
} 
//______________________________________________________________________
void replyACK() {

  // respond to ACK? command
  // this is used by the EXCEL Worksheet to confirm that a Base Station is connected on the currently selected COM Port
  // the number returned is the code for this Base Station so that multiple Base Stations may be connected to one Workstation
  // the allowable range is only 1 to 4 inclusive
  
  message = "OK(";
  message += BaseNo;
  message += ")";
  Serial.println(message);
}
//______________________________________________________________________
void verifyConnect(){

  if(analogRead(PhoneVoltage) > 400)
  {
    // request subscriber number
    ATstring = "";
    talkToPhone("AT+CNUM");
  
    // check for ERROR indicating no SIM card
    intComma = 0;
    intComma = ATstring.indexOf("ERROR");
  
    if (intComma > -1){
      // no SIM found
      message = "NO SIM";
      goto noSIM;
    }
    else
    {
      // SIM found so parse subscriber number
      intComma = ATstring.indexOf(",");
      SubscriberNo = "";
      // increment the pointer by 2 to bypass the comma and opening quotation marks and read the next 11 characters
      intComma = intComma + 2;
      for (counter1 = intComma; counter1 < intComma + 11; counter1++){
        SubscriberNo += ATstring.charAt(counter1);
      }
    }
    // parse for registration status
    ATstring = "";
    talkToPhone("AT+CGREG?");
  
    intComma = ATstring.indexOf(",");
    networkStatus = ATstring.charAt(intComma + 1);
    phoneStatus = "CODE=" + networkStatus;
  
    // request phone battery voltage
    ATstring = "";
    talkToPhone("AT+CBC");
    // find second comma
    intComma = 0;
    intComma = ATstring.indexOf(",");
    intComma = ATstring.indexOf(",", intComma+1);
    battery = "";
    intComma = intComma + 1;
    // read characters until carrige return
    do{
      readChar = ATstring.charAt(intComma);
      battery += readChar;
      intComma = intComma + 1;
      }while (readChar != 13);
    // remove carrige return
    battery.remove(battery.length() -1);
    
    //  request phone signal quality
    ATstring = "";
    talkToPhone("AT+CSQ");

    // find RSSI
    intColon = 0;
    intColon = ATstring.indexOf(":");
    signalRSSI = "";
    // increment the pointer by 2 to bypass the colon and the space and read the next characters until a ',' is detected
    intColon = intColon + 2;
    do{
      readChar = ATstring.charAt(intColon);
      signalRSSI += readChar;
      intColon = intColon + 1;
      }while (readChar != 44);
    signalRSSI.remove(signalRSSI.length() -1);
      
    // find BER
    intComma = ATstring.indexOf(",");
    signalBER = "";
    // increment the pointer by 1 to bypass the comma and read the next characters until carriage return
    intComma = intComma +1;
    do{
      readChar = ATstring.charAt(intComma);
      signalBER += readChar;
      intComma = intComma + 1;
      }while (readChar != 13);
  
    // create message and send
    message="";
    message += phoneStatus;
    message += ",";
    message += SubscriberNo;
    message += ",";
    message += battery;
    message += ",";
    message += signalRSSI;
    message += ",";
    message += signalBER;
    }
    else
    {
      message = "SIM800L NOT POWERED";
    }
noSIM:
  Serial.println(message);
  Serial.flush();
}
//______________________________________________________________________
void findSms() {
int startNo;
int endNo;

  // obtain passed parameter
  openingBracket = (command .indexOf('('));
  closingBracket = (command .indexOf(')'));
  passedNumber = (command .substring((openingBracket+1),(closingBracket))); 

  if(analogRead(PhoneVoltage) > 400)
  {
    // create range from start to end of five (need to add six (five + 1) because of loop conditions
    startNo =  passedNumber.toInt();
    endNo = startNo + 6;

    // set to text mode
    ATstring = "";
    talkToPhone("AT+CMGF=1");

    // request message until found or end
    do{
      message = "AT+CMGR=" + String(startNo);
      talkToPhone(message);
      delay(1000);
      if (ATstring.length() > 25) break;
      startNo = startNo + 1;  
    } while (startNo < endNo);
  }
  else
  {
    ATstring = ("SIM800L NOT POWERED");
  }
  // output found message  
  Serial.println(ATstring);
  Serial.flush();
}
//______________________________________________________________________
void clearSms() {

  // obtain passed parameter
  openingBracket = (command .indexOf('('));
  closingBracket = (command .indexOf(')'));
  passedNumber = (command .substring((openingBracket+1),(closingBracket))); 
  
  if(analogRead(PhoneVoltage) > 400)
  {
    // request text for specified SMS and print reply
    message = "AT+CMGD=" + passedNumber + ",0";
    talkToPhone(message);
  }
  else
  {
    ATstring = "SIM800L NOT POWERED";
  }
  // output reply
  Serial.println(ATstring);
  Serial.flush();
}
//______________________________________________________________________
void sendText() {
String ctlZ="\x1A";     //the code for Ctrl-Z

  // obtain sending mobile number and text message
  openingBracket = (command .indexOf('('));
  closingBracket = (command .indexOf(')'));
  intComma = (command .indexOf(','));
  MobileNumber = (command .substring((openingBracket+1),(intComma))); 
  MobileText = (command .substring((intComma+1),(closingBracket)));

  if(analogRead(PhoneVoltage) > 400)
  {
    // prepare message 
    message = "AT+CMGS=";
    message += char(34);  // ASCII code for "
    message += MobileNumber;
    message += char(34);
    message += "\n";
    message += MobileText;
    message += ctlZ;
  
    // set to text mode
    talkToPhone("AT+CMGF=1");
  
    // sent text message
    talkToPhone(message);
  }
  else
  {
    ATstring = "SIM800L NOT POWERED";  
  }
  // output reply
  Serial.println(ATstring);
  Serial.flush();
}
//______________________________________________________________________
void showNetlightStatus(){

  if(analogRead(PhoneVoltage) > 400)
  {
    // request NETLIGHT status
    talkToPhone("AT+CNETLIGHT?");

    // find second '+'
    int intPlus = (ATstring.lastIndexOf('+'));

    // read text
    intPlus = intPlus + 2;
    message="";
    do{
      readChar = ATstring.charAt(intPlus);
      message += readChar;
      intPlus = intPlus + 1;
    }while (readChar != ':');

    // change ':' to '='
    message.setCharAt(8, '=');
    intColon = (message.indexOf(':'));

    // add the found value
    readChar = ATstring.charAt(intPlus + 1);
    message += readChar;
  }
  else
  {
    ATstring = "SIM800L NOT POWERED";  
  }
    // output reply
    Serial.println(message);
    Serial.flush();
    
  }
//______________________________________________________________________
void setNetlight(){

  // obtain passed parameter
  openingBracket = (command .indexOf('('));
  closingBracket = (command .indexOf(')'));
  passedNumber = (command .substring((openingBracket+1),(closingBracket))); 

  if(analogRead(PhoneVoltage) > 400)
  {
    // set NETLIGHT status
    talkToPhone("AT+CNETLIGHT=" + passedNumber);

    // find ':'
    int intPlus = (ATstring.indexOf('+'));
    intPlus = intPlus + 2;

    // read text
    message="";
    do{
      readChar = ATstring.charAt(intPlus);
      message += readChar;
      intPlus = intPlus + 1;
    }while (readChar != 13);
  }
  else
  {
    ATstring = "SIM800L NOT POWERED";  
  }
  // output reply
  Serial.println(message);
  Serial.flush();
}
//______________________________________________________________________
void count(){

  // local variables
  String workString;
  String strNo;

  // set initial values
  message = "Count";
  
  if(analogRead(PhoneVoltage) > 400)
  {
    // request preferred SMS Message Storage 
    talkToPhone("AT+CPMS?");

    // parse reply
    workString="";
    intColon = 0;
    intColon = ATstring.indexOf(",");
    intColon = intColon + 1;
    do{
      readChar = ATstring.charAt(intColon);
      workString += readChar;
      intColon = intColon + 1;
    }while (readChar != char(','));
    workString.remove(workString.length()-1);
    strNo = workString;

    // check for > 20
    if (strNo == "20")
    {
      message += ">";
    }
    else{
      message += "=";
    }
    message += strNo;
  }
  else
  {
    ATstring = "SIM800L NOT POWERED";  
  }
  // output reply
  Serial.println(message);
  Serial.flush();
}
//______________________________________________________________________
void displayText(){

  message = "DISPLAY_OK";
  String DisplayText;
  
  // obtain passed parameters
  openingBracket = (command .indexOf('('));
  closingBracket = (command .indexOf(')'));
  intComma = (command .indexOf(','));
  lineNo = (command .substring((openingBracket+1),intComma)); 

  // check for low line number
  if(lineNo .toInt() < 0){
    message = "DISP_LN_LO";
    goto displayError;
  }
  // check for high line number
  if(lineNo .toInt() > 3){
    message = "DISP_LN_HI";
    goto displayError;
  }
  DisplayText = (command .substring((intComma+1),closingBracket)); 
 
  // check for length
  if(DisplayText.length() > 20){
    message = "DISP_TXT_TRUN";
    DisplayText.remove(20,(DisplayText.length()-20));
  }
  // display text
  // go to the begining and clear the line 
  lcd.setCursor ( 0, lineNo .toInt() );             
  lcd.print("                   "); 
  // go to the begining and write the text 
  lcd.setCursor ( 0, lineNo .toInt() );             
  lcd.print(DisplayText); 

displayError:
  //return display status
  Serial.println(message);
  Serial.flush();
}
//______________________________________________________________________
void clearLine(){

  // obtain passed parameter
  openingBracket = (command .indexOf('('));
  closingBracket = (command .indexOf(')'));
  lineNo = (command .substring((openingBracket+1),(closingBracket))); 

  message = "CLEARED LINE " + lineNo;
  
  // go to position and clear the line 
  lcd.setCursor ( 0, lineNo .toInt() );             
  lcd.print("                   "); 

  //return display status
  Serial.println(message);
  Serial.flush();
}
//______________________________________________________________________
void clearAll(){

  message = "DISP_CLEARED";
  
  // clear all the display
  lcd.clear();

  //return display status
  Serial.println(message);
  Serial.flush();
}
//______________________________________________________________________
