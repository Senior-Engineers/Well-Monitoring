#include <LiquidCrystal_I2C.h>
// configure the LCD address to 0x27 address; 20 chars and 4 line display 
LiquidCrystal_I2C lcd(0x27,20,4); 

#include <SPIFFS.h>

// configure for Bluetooth
#include <BluetoothSerial.h>
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif
BluetoothSerial SerialBT;

// SIM800L Module connected to Hardware Serial Port 2

// LCD 20 chars by 4 lines connected to I2C bus

/*______________________________________________________________________

re-coded for ESP-32 (WiFi) controller; second type of SIM800L module and 20 chars by 4 line LCD display (I2C)

Prototype program for hardware SMS Master Version 1.6, Software Version 1.6.b (Production Release - Stage 1)
  26th April 2017 to 15th June 2020
  System Engineering by Andrew Ainger, FIET
  Hardware Engineering by Maurice Smith MBE
  Code written by Maurice Smith MBE

  this sketch has tested and found to be functional with SIM800L Software Version 1418B04SIM800L24
     
  This software is licensed under Creative Commons BY-SA 4.0, http://creativecommons.org/licenses/by-sa/4.0/


  modified to respond to requests by either USB or BT Serial Comms
  
  AIMS:

  To operate in a standalone manner and respond to the following serially received commands:
  
  1.  ACK?
      Reply with OK(number of the Base Station). This is to enable several Base Stations to correctly function in different Workbooks on one PC
  2.  VERIFY()
      Return value is "'status','subscriber number','battery status','rssi','ber'
  3.  FIND(n)
      Where 'n' is the starting number of the SMS that is to be processed
      if locations are 'empty', then range limited to start plus five
      Return value is the SMS details from the Phone Module
  4.  COUNT()
      Returns the number of messages stored. Due to internal constrains, 
      the response is only accurate below the number of 20.
      If the number read is '20' then the separator is changed from '=' to '>'
  5.  CLEAR(n)
      The passed parameter memory location will be cleared
  6.  SEND(telNo,text)
      Send a text to the telephone number with the text as detailed in the passed parameters 
  7.  NETLIGHT?
      Returns to current status of the Netlight; 1=On & 0=OFF
  8.  NETLIGHT(n)
      Control the function of the SIM800L Module's Status Light, where n=1 will enable the Status Lamp and n=0 disable the Status Light
  9.  DISPLAY(line,text)
      Where:
        'line' is which line the text will be displayed; range is 0 to 3
        'text' is the text to be displayed starting at the left most position and limited to 20 characters
 10.  LINE_CLEAR(line)
      This will cause the passed line number to be cleared
 11.  ALL_CLEAR()
      This will clear all the display
 12.  RESET()
      Resets the ESP32
 13.  REPORT()
      Returns the following details:
        a) the SERIAL number of the Base Station
        b) the CODE Number of the Base Station
        c) the Bluetooth Broadcast Name of ESP32
        d) the Subscriber Telephone Number
        e) the Network Name
        f) the Signal Quality (RSSI & BER)
        g) the IMEI Number
        h) the Software Version of the SIM800L Module
___________________________________________________
*/
// constants
const int BaseNo = 1;                   // used to hold the number of the Base Station (1 to 4 only)
const String NAME = "Base Station No";  // used to hold the Bluetooth Name
const String Version = "1.6b";          // used to hold the version number of the Base Station
const int LED_BT_STATUS = 2;            // used to hold the pin number of the built in blue LED
const int LED_BT_STANDBY = 5;           // used to hold the standby value of the Bluetooth Status LED
const int LED_BT_ON = 255;              // used to hold the bright value of the Bluetooth Status LED  
const long DISPLAY_TIMEOUT = 600000;    // used to hold the number of mS before the LCD Backlight is turned off
                                        // 10 mins is 600000
// flags
int verifyCommand;                  // used to hold the return value when checking a command

// variables
String command ;                    // used to hold the string received from the Serial Input
String ATstring;                    // used to hold the string received from the SIM800L Module
String message;                     // used to hold the message being prepared
String phoneStatus;                 // used to hold the status word
String SubscriberNo;                // used to hold the subscriber number
String battery;                     // used to hold the battery voltage in millivolts
String signalRSSI;                  // used to hold the RSSI (acceptable values are 1 to 30 & 99)
String signalBER;                   // used to hold the BER (acceptable values are 0 to 7 & 99)
String networkStatus;               // used to hold the status of the network
String networkName;                 // used to hold the name of the network
String MobileNumber;                // used to hold the mobile number to send the text to
String MobileText;                  // used to hold the text to be sent
String passedNumber;                // used to hold the value of the number that was passed to a function
String lineNo;                      // used to hold the value of the line number that was passed to a function
String workString;                  // used as a general work string
String strNo;                       // used to hold the value of the number returned as a string 
String ReportMessage;               // used to prepare the message returned by the REPORT Command
String SerialNo;                    // used to hold the serial number read from SPIFFS
String read_file;                   // used to hold a char read from a file
 

int openingBracket;                 // used to hold the position of the opening bracket in the string
int closingBracket;                 // used to hold the position of the closing bracket in the string
int intComma;                       // used to hold the position of the comma in the string
int intColon;                       // used to hold the position of the colon in the string
int intPlus;                        // used to hold the position of the plus sign in the string
int intPos;                         // used to hold the position in the string 
int counter1;                       // used as a counter
int freq = 5000;                    // used to hold the frequency of the PWM Channel
int ledChannel = 0;                 // used to hold the channel number of the PWM Channel
int resolution = 8;                 // used to hold the resolution of the PWM Channel

long DisplayTime;                   // used to hold the number of millis() when the last command was processed
char readChar;                      // used to hold the character read from a string

bool BTstatus = true;               // used to determine if to flash the bluetooth status LED
bool serialRdy;                     // used to indicate that the command was received from SERIAL (true) or BT (false)

//______________________________________________________________________
   
void setup() {
  
  // initialise the PCW LED
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(LED_BT_STATUS, ledChannel);

  // set up serial link to host Workstation
  Serial.begin(115200);

  // Configure SPI Flash File System 
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    SerialBT.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // set up Bluetooth serial link
  message = NAME;
  message += BaseNo;
  SerialBT.begin(message); //Bluetooth device name
  
  // it can take upto 30 Sec for the name to be broadcast so flash the LED while waiting
  for (int n = 0; n < 60; n++){
    ledcWrite(ledChannel, LED_BT_ON);
    delay(250);
    ledcWrite(ledChannel, 0);
    delay(250);
  }

  // reserve space for the reply from the SIM800L Module
  ATstring.reserve(254);

  // initialize the lcd 
  lcd.init(); 
   
  // open the backlight
  lcd.backlight(); 
  
  // request NETLIGHT status
  talkToPhone("AT+CNETLIGHT?");
 
  // find ':'
  intPlus = (ATstring.lastIndexOf(':'));

  // read value
  intPlus = intPlus + 2;
  message = ATstring.charAt(intPlus);

  if(message.toInt() == 1){
    BTstatus = true;
    ledcWrite(ledChannel, LED_BT_STANDBY);
  }
  else{
    BTstatus = false;
    ledcWrite(ledChannel, 0);
  }
  
  // display initial message
  // reset backlight timer
  DisplayTime = millis();
  
  // go to begining of the line; write the text
  lcd.setCursor (0,0);             
  lcd.print("Awaiting for"); 
  lcd.setCursor (0,1);             
  lcd.print("Bluetooth or USB"); 
  lcd.setCursor (0,2);          
  lcd.print("Connection...");  
}
//______________________________________________________________________
void loop() {
  // put your main code here, to run repeatedly:

  // check backlight status
  if (DisplayTime + DISPLAY_TIMEOUT < millis()){
    lcd.noBacklight();
  }else{
    lcd.backlight();
  }
  // check for command received, read string and convert to Upper Case
  if (Serial.available ())
  {
    serialRdy = true;
    command  = Serial.readString();
  }
  
  if (SerialBT.available ())
  {
    serialRdy = false;
    command  = SerialBT.readString();
  }

  command.toUpperCase();
  
  // check for the "?" as the first char as a spare "?" is loaded into the serial buffer after 'reset'
  verifyCommand = (command .startsWith("?ACK?"));
  if (verifyCommand == 1) {
    command .substring(2);
    replyACK();
  }

  // check for the ACK? command
  verifyCommand = (command .startsWith("ACK?"));
  if (verifyCommand == 1) replyACK();

  // check for the Verify command
  verifyCommand = (command .startsWith("VERIFY"));
  if (verifyCommand == 1) verifyConnect();

  // check for the Process SMS command
  verifyCommand = (command .startsWith("FIND"));
  if (verifyCommand == 1) findSms();

  // check for the Netlight? command
  verifyCommand = (command .startsWith("NETLIGHT?"));
  if (verifyCommand == 1) showNetlightStatus();

  // check for the Netlight(n) command
  verifyCommand = (command .startsWith("NETLIGHT("));
  if (verifyCommand == 1) setNetlight();

  // check for the Clear SMS command
  verifyCommand = (command .startsWith("CLEAR"));
  if (verifyCommand == 1) clearSms();

  // check for the Send SMS command
  verifyCommand = (command .startsWith("SEND"));
  if (verifyCommand == 1) sendText();

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

  // check for the RESET command
  verifyCommand = (command .startsWith("RESET"));
  if (verifyCommand == 1) BTreset();

  // check for the REPORT command
  verifyCommand = (command .startsWith("REPORT"));
  if (verifyCommand == 1) reportInfo();
}
//______________________________________________________________________
void talkToPhone(String ATcommand) {
  
  // setup communications with the SIM800L
  HardwareSerial SIM800L(2);
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

  // clear display
  lcd.clear();
  
  message = "OK(";
  message += BaseNo;
  message += ")";

  if (serialRdy == false){
    SerialBT.println(message);
    SerialBT.flush();
  }
  else{
    Serial.println(message);
    Serial.flush();
  }
  command = "";
  BTflash();
}
//______________________________________________________________________
void verifyConnect(){

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
  // read characters until carriage return
  do{
    readChar = ATstring.charAt(intComma);
    battery += readChar;
    intComma = intComma + 1;
    }while (readChar != 13);
  // remove carriage return
  battery.remove(battery.length() -1);
    
  // request phone signal quality
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

noSIM:
  if (serialRdy == false){
    SerialBT.println(message);
    SerialBT.flush();
  }
  else{
    Serial.println(message);
    Serial.flush();
  }
  command = "";
  BTflash();
}
//______________________________________________________________________
void findSms() {
int passedNo;

  // obtain passed parameter
  openingBracket = (command .indexOf('('));
  closingBracket = (command .indexOf(')'));
  passedNumber = (command .substring((openingBracket+1),(closingBracket))); 

  // create range from start to end of five (need to add six (five + 1) because of loop conditions
  passedNo =  passedNumber.toInt();

  // set to text mode
  ATstring = "";
  talkToPhone("AT+CMGF=1");

  // request message
  message = "AT+CMGR=" + String(passedNo);
  talkToPhone(message);
  delay(1000);
    
  // output found message 
  if (serialRdy == false){
    SerialBT.println(ATstring);
    SerialBT.flush();
  }
  else{
    Serial.println(ATstring);
    Serial.flush();
  }
  command = "";
  BTflash(); 
}
//______________________________________________________________________
void clearSms() {

  // obtain passed parameter
  openingBracket = (command .indexOf('('));
  closingBracket = (command .indexOf(')'));
  passedNumber = (command .substring((openingBracket+1),(closingBracket))); 
  
  // request text for specified SMS and print reply
  message = "AT+CMGD=" + passedNumber + ",0";
  talkToPhone(message);
  
  // output found message 
  if (serialRdy == false){
    SerialBT.println(ATstring);
    SerialBT.flush();
  }
  else{
    Serial.println(ATstring);
    Serial.flush();
  }
  command = "";
  BTflash(); 
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
  
  // output reply
  if (serialRdy == false){
    SerialBT.println(message);
    SerialBT.flush();
  }
  else{
    Serial.println(message);
    Serial.flush();
  }
  command = "";
  BTflash();
}
//______________________________________________________________________
void showNetlightStatus(){

  // request NETLIGHT status
  talkToPhone("AT+CNETLIGHT?");

  // find second '+'
  intPlus = (ATstring.lastIndexOf('+'));

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
  
  // output reply
  if (serialRdy == false){
    SerialBT.println(message);
    SerialBT.flush();
  }
  else{
    Serial.println(message);
    Serial.flush();
  }
  command = "";
  BTflash();
}
//______________________________________________________________________
void setNetlight(){

  // obtain passed parameter
  openingBracket = (command .indexOf('('));
  closingBracket = (command .indexOf(')'));
  passedNumber = (command .substring((openingBracket+1),(closingBracket))); 

  // set NETLIGHT status
  talkToPhone("AT+CNETLIGHT=" + passedNumber);

  // find '+'
  intPlus = (ATstring.indexOf('+'));
  intPlus = intPlus + 2;

  // read text
  message="";
  do{
    readChar = ATstring.charAt(intPlus);
    message += readChar;
    intPlus = intPlus + 1;
    }while (readChar != 13);
  message.remove(10); // remove /n
  
  if(message == "NETLIGHT=1"){
    BTstatus = true;
  }
  else{
    BTstatus = false;
  }
  // save setting
  talkToPhone("AT&W0");

  // output reply
  if (serialRdy == false){
    SerialBT.println(message);
    SerialBT.flush();
  }
  else{
    Serial.println(message);
    Serial.flush();
  }
  command = "";
  BTflash();
}
//______________________________________________________________________
void count(){

  // set initial values
  message = "Count";
  
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

  //check for > 20
  if (strNo == "20"){
    message += ">";
  }
  else
  {
    message += "=";
  }
  message += strNo;

  // output reply
  if (serialRdy == false){
    SerialBT.println(message);
    SerialBT.flush();
  }
  else{
    Serial.println(message);
    Serial.flush();
  }
  command = "";
  BTflash();
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
  // go to begining and write the text 
  lcd.setCursor ( 0, lineNo .toInt() );             
  lcd.print(DisplayText); 

displayError:
  //return display status
  if (serialRdy == false){
    SerialBT.println(message);
    SerialBT.flush();
  }
  else{
    Serial.println(message);
    Serial.flush();
  }
  command = "";
  BTflash();
}
//______________________________________________________________________
void clearLine(){

  // obtain passed parameter
  openingBracket = (command .indexOf('('));
  closingBracket = (command .indexOf(')'));
  lineNo = (command .substring((openingBracket+1),(closingBracket))); 

  message = "CLEARED LINE " + lineNo;
  
  // go to the begining and clear the line 
  lcd.setCursor ( 0, lineNo .toInt() );             
  lcd.print("                    "); 

  //return display status
  if (serialRdy == false){
    SerialBT.println(message);
    SerialBT.flush();
  }
  else{
    Serial.println(message);
    Serial.flush();
  }
  command = "";
  BTflash();
}
//______________________________________________________________________
void clearAll(){

  message = "DISP_CLEARED";
  
  // clear all the display
  lcd.clear();

  //return display status
  if (serialRdy == false){
    SerialBT.println(message);
    SerialBT.flush();
  }
  else{
    Serial.println(message);
    Serial.flush();
  }
  command = "";
  BTflash();
}
//______________________________________________________________________
void BTflash() {
  
  if(BTstatus == true){
    ledcWrite(ledChannel, LED_BT_ON);
    delay(250);
    ledcWrite(ledChannel, LED_BT_STANDBY); 
  }
  else{
    ledcWrite(ledChannel, 0);
  }
  DisplayTime = millis();
}
//______________________________________________________________________
void BTreset(){

  // USB Serial link will display normal start-up data
  // Bluetooth link will need to be closed and then opened to receive Bluetooth Serial Link
  SerialBT.println("Restarting");
  SerialBT.println();
  SerialBT.println("Please re-enable the Bluetooth Connection when LCD displays message...");
  SerialBT.flush();
  delay(500);
  ESP.restart();
}
//______________________________________________________________________
void reportInfo(){
  // show information about the Base Station

  ReportMessage = "This Base Station's parameter's are:\n";
  ReportMessage += "\n\tBase Station Serial Number  :  ";

  // reads the serial number from the serial.txt file 
  //##
  if (SPIFFS.exists("/serial.txt") == 0){
    ReportMessage += "UNSET";
  }else{
    File myFile = SPIFFS.open("/serial.txt",FILE_READ);
  
    if(!myFile) ReportMessage += "Failed to open serial.txt for reading";
       
    // find serial number
    SerialNo = "";
    read_file ="";
    while(read_file != "\n") {
      read_file = char(myFile.read());
      SerialNo += read_file;
    }
    SerialNo.remove(SerialNo.length ()-1);
    ReportMessage += SerialNo;
  }

  ReportMessage += "\n\tBase Station CODE Number    :  ";
  ReportMessage += BaseNo;
  ReportMessage += "\n\tSoftware Version            :  ";
  ReportMessage += Version;
  ReportMessage += "\n\tBluetooth Broadcast Name    :  ";
  ReportMessage += NAME;
  ReportMessage += BaseNo;

  // request subscriber number
  ATstring = "";
  talkToPhone("AT+CNUM");
  
  // check for ERROR indicating no SIM card
  intComma = 0;
  intComma = ATstring.indexOf("ERROR");

  if (intComma > -1){
    // no SIM found
    ReportMessage += "\n\tNO SIM FOUND";
  }
  else
  {
    // SIM found so continue
    
    // parse subscriber number
    intComma = ATstring.indexOf(",");
    SubscriberNo = "";
    // increment the pointer by 2 to bypass the comma and opening quotation marks and read the next 11 characters
    intComma = intComma + 2;
    for (counter1 = intComma; counter1 < intComma + 11; counter1++){
      SubscriberNo += ATstring.charAt(counter1);
    }
    ReportMessage += "\n\tSubscriber Number           :  ";
    ReportMessage += SubscriberNo;

    // parse network name
    ATstring = "";
    talkToPhone("AT+COPS?");
    intComma = ATstring.indexOf(",");
    networkName = "";
    // increment the pointer by 3 to bypass the comma; next parameter and comma. read chars until carriage return
    intComma = intComma + 3;
    do{
    readChar = ATstring.charAt(intComma);
    networkName += readChar;
    intComma = intComma + 1;
    }while (readChar != 10);
    ReportMessage += "\n\tNetwork Name                :  ";
    ReportMessage += networkName;

    // parse signal quality RSSI
    // request phone signal quality
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
    ReportMessage += "\tSignal Quality - RSSI       :  ";
    ReportMessage += signalRSSI;
    
    // parse signal quality BER
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
  
    ReportMessage += "\n\tSignal Quality - BER        :  ";
    ReportMessage += signalBER;
    }

  // read serial number (IMEI)
    ATstring = "";
    message = "";
    talkToPhone("AT+CGSN");
    intPos = ATstring.indexOf(char(13));
    intPos = intPos + 3;  // increment by 3 to bypass Carriage Return and other non-printing characters
    do{
      readChar = ATstring.charAt(intPos);
      message += readChar;
      intPos = intPos + 1;
    } while (readChar != 13);
  ReportMessage += "\n\tIMEI No.                    :  ";
  ReportMessage += message;
  ReportMessage += "\n";

  // read software version
  talkToPhone("AT+GSV");
  // process the fourth line that contains the details of the software revision
  message = ""; // clear message
  intColon = ATstring.indexOf(':');  // find the colon
  intColon = intColon + 1;  // increment by 1 to bypass space
  do{
    readChar = ATstring.charAt(intColon);
    message += readChar;
    intColon = intColon + 1;
  } while (readChar != 13);
  ReportMessage += "\tSoftware Revision           :  ";
  ReportMessage += message;

  //display message
  if (serialRdy == false){
    SerialBT.println(ReportMessage);
    SerialBT.flush();
  }
  else{
    Serial.println(ReportMessage);
    Serial.flush();
  }
  command = "";
  BTflash();
}
