
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <AltSoftSerial.h>

#include <TinyGPS++.h>
//MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
const int totalPhoneNo = 5;
const byte phoneLength = 13;
//total memory consumed by phone numbers in EEPROM
const byte totalMemory = (phoneLength * 5) - 1;
//set starting address of each phone number in EEPROM
int offsetPhone[totalPhoneNo] = {0,phoneLength,phoneLength*2,phoneLength*3,phoneLength*4};
//MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM

//GSM Module RX pin to Arduino 3
//GSM Module TX pin to Arduino 2
#define rxPin 2
#define txPin 3

SoftwareSerial sim800(rxPin,txPin);
AltSoftSerial neogps;
TinyGPSPlus gps;
//GPS Module RX pin to Arduino 9
//GPS Module TX pin to Arduino 8
//--------------------------------------------------------------
boolean DEBUG_MODE = 1;
boolean isReply = false;
String latitude, longitude;
unsigned long previousMillis = 0; // last time update
long interval = 60000; // interval at which to do something (milliseconds)
//MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM


/*******************************************************************************
 * getResponse function
 ******************************************************************************/
boolean getResponse(String expected_answer, unsigned int timeout=1000,boolean reset=false){
  boolean flag = false;
  String response = "";
  unsigned long previous;
  //*************************************************************
  for(previous=millis(); (millis() - previous) < timeout;){
    while(sim800.available()){
      response = sim800.readString();
      //----------------------------------------
      //Used in resetSIM800L function
      //If there is some response data
      if(response != ""){
        Serial.println(response);
        if(reset == true)
          return true;
      }
      //----------------------------------------
      if(response.indexOf(expected_answer) > -1){
        return true;
      }
    }
  }
  //*************************************************************
  return false;
}

/*******************************************************************************
 * resetSIM800L function
 ******************************************************************************/

/*******************************************************************************
 * setup function
 ******************************************************************************/
void setup() {
  //-----------------------------------------------------
  Serial.begin(115200);
  Serial.println("Arduino serial initialize");
  //-----------------------------------------------------
  sim800.begin(9600);
  Serial.println("SIM800L software serial initialize");
  //-----------------------------------------------------
  neogps.begin(9600);
  Serial.println("NEOGPS software serial initialize");
  
  
  delay(100);

  
  sim800.println("AT");
  getResponse("OK",1000);
  sim800.println("AT+CMGF=1");
  getResponse("OK",1000);
  sim800.println("AT+CNMI=1,2,0,0,0");
  getResponse("OK",1000);
  //-------------------------------------------------------------------
  Serial.println(GetRegisteredPhoneNumbersList());
  //-------------------------------------------------------------------
}


/*******************************************************************************
 * Loop Function
 ******************************************************************************/
void loop() {
//------------------------------------------------------------------------------
while(sim800.available()){
  String response = sim800.readString();
  Serial.println(response);
  //__________________________________________________________________________
  //if there is an incoming SMS
  if(response.indexOf("+CMT:") > -1){
    String callerID = getCallerID(response);
    String cmd = getMsgContent(response);
    //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
    //this if statement will execute only if sim800l received "r" command 
    //and there will be no any phone number stored in the EEPROM
    if(cmd.equals("r")){
      String admin_phone = readFromEEPROM(offsetPhone[0]);
      if(admin_phone.length() != phoneLength){
        RegisterPhoneNumber(1, callerID, callerID);
        break;
      }
      else {
        String text = "Error: Admin phone number is failed to register";
        Serial.println(text);
        Reply(text, callerID);
        break;
      }
    }
    //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
    //The action is performed only if the caller ID will matched with
    //any one of the phone numbers stored in the eeprom.
    if(comparePhone(callerID)){
      doAction(cmd, callerID);
    }
    else {
      String text = "Error: Please register your phone number first";
      Serial.println(text);
      Reply(text, callerID);
    }
    //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
  }
  //__________________________________________________________________________
}
//------------------------------------------------------------------------------
while(Serial.available())  {
  String response = Serial.readString();
  if(response.indexOf("clear") > -1){
    Serial.println(response);
    DeletePhoneNumberList();
    Serial.println(GetRegisteredPhoneNumbersList());
    break;
  }
  sim800.println(response);
}
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
} //main loop ends

/*******************************************************************************
 * GetRegisteredPhoneNumbersList function:
 ******************************************************************************/
String GetRegisteredPhoneNumbersList(){
  String text = "Registered Numbers List: \r\n";
  String temp = "";
  for(int i = 0; i < totalPhoneNo; i++){
    temp = readFromEEPROM(offsetPhone[i]);
    if(temp == "")
      { text = text + String(i+1)+". Empty\r\n"; }
    else if(temp.length() != phoneLength)
      { text = text + String(i+1)+". Wrong Format\r\n"; }
    else
      {text = text + String(i+1)+". "+temp+"\r\n";}
  }

  return text;
}
  
void RegisterPhoneNumber(int index, String eeprom_phone, String caller_id){
  if(eeprom_phone.length() == phoneLength){
    writeToEEPROM(offsetPhone[index-1],eeprom_phone);
    String text = "Phone"+String(index)+" is Registered: ";
    //text = text + phoneNumber;
    Serial.println(text);
    Reply(text, caller_id);
  }
  else {
    String text = "Error: Phone number must be "+String(phoneLength)+" digits long";
    Serial.println(text);
    Reply(text, caller_id);
  }
}

/*******************************************************************************
 * UnRegisterPhoneNumber function:
 ******************************************************************************/
void DeletePhoneNumber(int index, String caller_id){
  writeToEEPROM(offsetPhone[index-1],"");
  String text = "Phone"+String(index)+" is deleted";
  Serial.println(text);
  Reply(text, caller_id);
}
/*******************************************************************************
 * DeletePhoneNumberList function:
 ******************************************************************************/
void DeletePhoneNumberList(){
  for (int i = 0; i < totalPhoneNo; i++){
    writeToEEPROM(offsetPhone[i],"");
  }
}
  
/*******************************************************************************
 * doAction function:
 * Performs action according to the received sms
 ******************************************************************************/
void doAction(String cmd, String caller_id){
  //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
  if(cmd.indexOf("g") > -1){
     sendLocation();
    
  }
  else if(cmd.indexOf("r2=") > -1){
    RegisterPhoneNumber(2, getNumber(cmd), caller_id);
  }
  else if(cmd.indexOf("r3=") > -1){
    RegisterPhoneNumber(3, getNumber(cmd), caller_id);
  }
  else if(cmd.indexOf("r4=") > -1){ 
    RegisterPhoneNumber(4, getNumber(cmd), caller_id);
  }
  else if(cmd.indexOf("r5=") > -1){
    RegisterPhoneNumber(5, getNumber(cmd), caller_id);
  }
  //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
  else if(cmd == "list"){
    String list = GetRegisteredPhoneNumbersList();
    Serial.println(list);
    Reply(list, caller_id);
  }
  //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
  //else if(cmd == "del=1"){  
    //DeletePhoneNumber(1, caller_id);
  //}
  else if(cmd == "del=2"){  
    DeletePhoneNumber(2, caller_id);
  }
  else if(cmd == "del=3"){  
    DeletePhoneNumber(3, caller_id);
  }
  else if(cmd == "del=4"){  
    DeletePhoneNumber(4, caller_id);
  }
  else if(cmd == "del=5"){  
    DeletePhoneNumber(5, caller_id);
  }
  //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
  else if(cmd == "del=all"){
    DeletePhoneNumberList();
    String text = "All the phone numbers are deleted.";
    Serial.println(text);
    Reply(text, caller_id);
  }
  //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
  else{
    String text = "Error: Unknown command: "+cmd;
    Serial.println(text);
    Reply(text, caller_id);
  }
  //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
}

/*******************************************************************************
 * listen_push_buttons function:
 ******************************************************************************/


/*******************************************************************************
 * getCallerID function:
 ******************************************************************************/
String getCallerID(String buff){
//+CMT: "+923001234567","","22/05/20,11:59:15+20"
//Hello
  unsigned int index, index2;
  index = buff.indexOf("\"");
  index2 = buff.indexOf("\"", index+1);
  String callerID = buff.substring(index+1, index2);
  callerID.trim();
  //Serial.print("index+1= "); Serial.println(index+1);
  //Serial.print("index2= "); Serial.println(index2);
  //Serial.print("length= "); Serial.println(callerID.length());
  Serial.println("Caller ID: "+callerID);
  return callerID;
}
/*******************************************************************************
 * getMsgContent function:
 ******************************************************************************/
String getMsgContent(String buff){
  //+CMT: "+923001234567","","22/05/20,11:59:15+20"
  //Hello  
  unsigned int index, index2;
  index = buff.lastIndexOf("\"");
  index2 = buff.length();
  String command = buff.substring(index+1, index2);
  command.trim();
  command.toLowerCase();
  //Serial.print("index+1= "); Serial.println(index+1);
  //Serial.print("index2= "); Serial.println(index2);
  //Serial.print("length= "); Serial.println(msg.length());
  Serial.println("Command:"+command);
  return command;
}
/*******************************************************************************
 * getNumber function:
 ******************************************************************************/
String getNumber(String text){
  //r=+923001234567
  String temp = text.substring(3, 16);
  //Serial.println(temp);
  temp.trim();
  return temp;
}
void sendLocation( String caller_id)
  
{
  //-----------------------------------------------------------------
  // Can take up to 60 seconds
  boolean newData = false;
  for (unsigned long start = millis(); millis() - start < 2000;)
  {
    while (neogps.available())
    {
      if (gps.encode(neogps.read()))
        {newData = true;break;}
    }
  }
  //-----------------------------------------------------------------

  //-----------------------------------------------------------------
  //If newData is true
  if(newData)
  {
    newData = false;
    String latitude = String(gps.location.lat(), 6);
    String longitude = String(gps.location.lng(), 6);
    //String speed = String(gps.speed.kmph());
    
    String text = "Latitude= " + latitude;
    text += "\n\r";
    text += "Longitude= " + longitude;
    text += "\n\r";
    text += "Speed= " + String(gps.speed.kmph()) + " km/h";
    text += "\n\r";
    text += "Altitude= " + String(gps.altitude.meters()) + " meters";
    //text += "\n\r";
    //text += "Connected Satellites= " + String(gps.satellites.value());
    text += "\n\r";
    text += "http://maps.google.com/maps?q=loc:" + latitude + "," + longitude;
       
    
    
    Reply(text,  caller_id);
  }
  //-----------------------------------------------------------------
}


void Reply(String text, String caller_id)
{
  //return;
    sim800.print("AT+CMGF=1\r");
    delay(1000);
    sim800.print("AT+CMGS=\""+caller_id+"\"\r");
    delay(1000);
    sim800.print(text);
    delay(100);
    sim800.write(0x1A); //ascii code for ctrl-26 //sim800.println((char)26); //ascii code for ctrl-26
    delay(1000);
    Serial.println("SMS Sent Successfully.");
}

/*******************************************************************************
 * writeToEEPROM function:
 * Store registered phone numbers in EEPROM
 ******************************************************************************/
void writeToEEPROM(int addrOffset, const String &strToWrite)
{
  //byte phoneLength = strToWrite.length();
  //EEPROM.write(addrOffset, phoneLength);
  for (int i = 0; i < phoneLength; i++)
    { EEPROM.write(addrOffset + i, strToWrite[i]); }
}

/*******************************************************************************
 * readFromEEPROM function:
 * Store phone numbers in EEPROM
 ******************************************************************************/
String readFromEEPROM(int addrOffset)
{
  //byte phoneLength = strToWrite.length();
  char data[phoneLength + 1];
  for (int i = 0; i < phoneLength; i++)
    { data[i] = EEPROM.read(addrOffset + i); }
  data[phoneLength] = '\0';
  return String(data);
}




/*******************************************************************************
 * comparePhone function:
 * compare phone numbers stored in EEPROM
 ******************************************************************************/
boolean comparePhone(String number)
{
  boolean flag = 0;
  String tempPhone = "";
  //--------------------------------------------------
  for (int i = 0; i < totalPhoneNo; i++){
    tempPhone = readFromEEPROM(offsetPhone[i]);
    if(tempPhone.equals(number)){
      flag = 1;
      break;
    }
  }
  //--------------------------------------------------
  return flag;
}
