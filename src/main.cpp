/*mqttToMorse*/
/* Converts an MQTT message to morse code. */
/* author: David E. Powell */

#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "mqttToMorse.h"
#include <EEPROM.h>

// Define the MQTT broker and topic
// const char* mqtt_server = "10.10.6.15";
// const char* mqtt_topic = "morse_code";

// Define the LED pin
const int led_pin = 2;

// Define the Morse code lookup tables
const char* morse_text_table[] = {
  ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.."
};
const char* morse_number_table[] = {
  "-----",".----", "..---", "...--", "....-", ".....","-....","--...","---..","----."
};
const char* morse_special_table[] = {
".-.-.-", //Period          
"--..--", //Comma           
"..--..", //Question Mark    
"-.-.-.", //Semicolon       
"---...", //Colon           
"-....-", //Dash            
"-..-." , //Slash           
".----.", //Apostrophe      
".-..-.", //Quotations      
"..--.-", //Underscore      
".-.-." , //Addition        
"-..-" ,  //Multiplication  
"-...-" , //Equal           
"-.--.-", //Right Parenthesis
"-.--."   //Left Parenthesis   
};

const char morse_special_lookup[] = {
  '.', ',', '?', ';', ':', '-', '/', '\'', '"', '_', '+', '*', '=', ')', '('
};
int special_lookup_size=sizeof(morse_special_lookup)/sizeof(morse_special_lookup[0]);

// Set up the ESP8266 and MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// These are the settings that get stored in EEPROM.  They are all in one struct which
// makes it easier to store and retrieve.
typedef struct 
  {
  unsigned int validConfig=0; 
  char ssid[SSID_SIZE+1] = "";
  char wifiPassword[PASSWORD_SIZE+1] = "";
  char brokerAddress[ADDRESS_SIZE+1]="";
  int brokerPort=DEFAULT_MQTT_BROKER_PORT;
  char mqttUsername[USERNAME_SIZE+1]="";
  char mqttUserPassword[PASSWORD_SIZE+1]="";
  char mqttTopic[MQTT_MAX_TOPIC_SIZE+1]="";
  char mqttMessage[MQTT_MAX_MESSAGE_SIZE+1]="";
  char mqttLWTMessage[MQTT_MAX_MESSAGE_SIZE+1]="";
  char mqttCommandTopic[MQTT_MAX_TOPIC_SIZE+1]=DEFAULT_MQTT_COMMAND_TOPIC;
  char mqttClientId[MQTT_CLIENTID_SIZE+1]=""; //will be the same across reboots
  int dotLength=DEFAULT_DOT_LENGTH;
  int pitch=DEFAULT_TONE_PITCH;
  boolean debug=false;
  } conf;

conf settings; //all settings in one struct makes it easier to store in EEPROM
boolean settingsAreValid=false;
String commandString = "";     // a String to hold incoming commands from serial
bool commandComplete = false;  // goes true when enter is pressed

void myDelay(unsigned long dly)
  {
  checkForCommand();
  delay(dly);
  }

void showSettings()
  {
  Serial.print("ssid=<wifi ssid> (");
  Serial.print(settings.ssid);
  Serial.println(")");
  Serial.print("wifipass=<wifi password> (");
  Serial.print(settings.wifiPassword);
  Serial.println(")");
  Serial.print("broker=<address of MQTT broker> (");
  Serial.print(settings.brokerAddress);
  Serial.println(")");
  Serial.print("brokerPort=<port number MQTT broker> (");
  Serial.print(settings.brokerPort);
  Serial.println(")");
  Serial.print("userName=<user ID for MQTT broker> (");
  Serial.print(settings.mqttUsername);
  Serial.println(")");
  Serial.print("userPass=<user password for MQTT broker> (");
  Serial.print(settings.mqttUserPassword);
  Serial.println(")");
  Serial.print("topic1=<MQTT topic for which to subscribe> (");
  Serial.print(settings.mqttTopic);
  Serial.println(")");
  Serial.print("lwtMessage=<status message to send when power is removed> (");
  Serial.print(settings.mqttLWTMessage);
  Serial.println(")");
  Serial.print("mqttCommandTopic=<mqtt message for commands to this device> (");
  Serial.print(settings.mqttCommandTopic);
  Serial.println(")");
  Serial.print("debug=<print debug messages to serial port> (");
  Serial.print(settings.debug?"true":"false");
  Serial.println(")");
  Serial.print("pitch=<frequency in Hz for tone pitch> (");
  Serial.print(settings.pitch);
  Serial.println(")");
  Serial.print("dotLength=<number of milliseconds for dot> (");
  Serial.print(settings.dotLength);
  Serial.println(")");
  Serial.print("MQTT client ID=<automatically generated client ID> (");
  Serial.print(settings.mqttClientId);
  Serial.println(") **Use \"resetmqttid=yes\" to regenerate");
  Serial.println("\n*** Use \"factorydefaults=yes\" to reset all settings ***");
  Serial.print("\nIP Address=");
  Serial.println(WiFi.localIP());
  }
/*
 * Check for configuration input via the serial port.  Return a null string 
 * if no input is available or return the complete line otherwise.
 */
String getConfigCommand()
  {
  if (commandComplete) 
    {
    String newCommand=commandString;

    commandString = "";
    commandComplete = false;
    return newCommand;
    }
  else return "";
  }

//Generate an MQTT client ID.  This should not be necessary very often
char* generateMqttClientId(char* mqttId)
  {
  String ext=String(random(0xffff), HEX);
  const char* extc=ext.c_str();
  strcpy(mqttId,MQTT_CLIENT_ID_ROOT);
  strcat(mqttId,extc);
  if (settings.debug)
    {
    Serial.print("New MQTT userid is ");
    Serial.println(mqttId);
    }
  return mqttId;
  }

/*
 * Save the settings to EEPROM. Set the valid flag if everything is filled in.
 */
boolean saveSettings()
  {
  static boolean wasIncomplete=false;
  if (strlen(settings.ssid)>0 &&
    strlen(settings.ssid)<=SSID_SIZE &&
    strlen(settings.wifiPassword)>0 &&
    strlen(settings.wifiPassword)<=PASSWORD_SIZE &&
    strlen(settings.brokerAddress)>0 &&
    strlen(settings.brokerAddress)<ADDRESS_SIZE &&
    strlen(settings.mqttLWTMessage)>0 &&
    strlen(settings.mqttLWTMessage)<MQTT_MAX_MESSAGE_SIZE &&
    strlen(settings.mqttTopic)<MQTT_MAX_TOPIC_SIZE &&
    strlen(settings.mqttCommandTopic)>0 &&
    strlen(settings.mqttCommandTopic)<MQTT_MAX_TOPIC_SIZE &&
    settings.brokerPort > 0 &&
    settings.dotLength > 0 &&
    settings.pitch > 0)
    {
    Serial.println("Settings deemed complete");
    settings.validConfig=VALID_SETTINGS_FLAG;
    settingsAreValid=true;
    if (wasIncomplete)
      {
      wasIncomplete=false;
      }

    initConnections();
    }
  else
    {
    Serial.println("Settings still incomplete");
    settings.validConfig=0;
    settingsAreValid=false;
    wasIncomplete=true;
    }

  //The mqttClientId is not set by the user, but we need to make sure it's set  
  if (strlen(settings.mqttClientId)==0)
    {
    generateMqttClientId(settings.mqttClientId);
    }

  EEPROM.put(0,settings);
  return EEPROM.commit();
  }

void initializeSettings()
  {
  settings.validConfig=0; 
  strcpy(settings.ssid,"");
  strcpy(settings.wifiPassword,"");
  strcpy(settings.brokerAddress,"");
  settings.brokerPort=DEFAULT_MQTT_BROKER_PORT;
  strcpy(settings.mqttLWTMessage,DEFAULT_MQTT_LWT_MESSAGE);
  strcpy(settings.mqttMessage,"");
  strcpy(settings.mqttTopic,DEFAULT_MQTT_TOPIC);
  strcpy(settings.mqttUsername,"");
  strcpy(settings.mqttUserPassword,"");
  strcpy(settings.mqttCommandTopic,DEFAULT_MQTT_COMMAND_TOPIC);
  settings.dotLength=DEFAULT_DOT_LENGTH;
  settings.pitch=DEFAULT_TONE_PITCH;
  generateMqttClientId(settings.mqttClientId);
  settings.debug=false;
  saveSettings();
  }


/// @brief Accepts a KV pair to change a setting or perform an action. Minimal input checking, be careful.
/// @param cmd 
/// @return true if a reset is needed to activate the change
void processCommand(String cmd)
  {
  const char *str=cmd.c_str();
  char *val=NULL;
  char *nme=strtok((char *)str,"=");
  if (nme!=NULL)
    val=strtok(NULL,"=");

  char zero[]=""; //zero length string

  //Get rid of the carriage return and/or linefeed. Twice because could have both.
  if (val!=NULL && strlen(val)>0 && (val[strlen(val)-1]==13 || val[strlen(val)-1]==10))
    val[strlen(val)-1]=0; 
  if (val!=NULL && strlen(val)>0 && (val[strlen(val)-1]==13 || val[strlen(val)-1]==10))
    val[strlen(val)-1]=0; 

  //do it for the command as well.  Might not even have a value.
  if (nme!=NULL && strlen(nme)>0 && (nme[strlen(nme)-1]==13 || nme[strlen(nme)-1]==10))
    nme[strlen(nme)-1]=0; 
  if (nme!=NULL && strlen(nme)>0 && (nme[strlen(nme)-1]==13 || nme[strlen(nme)-1]==10))
    nme[strlen(nme)-1]=0; 

  if (settings.debug)
    {
    Serial.print("Processing command \"");
    Serial.print(nme);
    Serial.println("\"");
    Serial.print("Length:");
    Serial.println(strlen(nme));
    Serial.print("Hex:");
    Serial.println(nme[0],HEX);
    Serial.print("Value is \"");
    Serial.print(val);
    Serial.println("\"\n");
    }

  if (val==NULL)
    val=zero;

  if (nme==NULL || val==NULL || strlen(nme)==0) //empty string is a valid val value
    {
    showSettings();
    return;   //not a valid command, or it's missing
    }
  else if (strcmp(nme,"ssid")==0)
    {
    strncpy(settings.ssid,val,SSID_SIZE);
    settings.ssid[SSID_SIZE]='\0';
    saveSettings();
    }
  else if (strcmp(nme,"wifipass")==0)
    {
    strncpy(settings.wifiPassword,val,PASSWORD_SIZE);
    settings.wifiPassword[PASSWORD_SIZE]='\0';
    saveSettings();
    }
  else if (strcmp(nme,"broker")==0)
    {
    strncpy(settings.brokerAddress,val,ADDRESS_SIZE);
    settings.brokerAddress[ADDRESS_SIZE]='\0';
    saveSettings();
    }
  else if (strcmp(nme,"brokerPort")==0)
    {
    settings.brokerPort=atoi(val);
    saveSettings();
    }
  else if (strcmp(nme,"userName")==0)
    {
    strncpy(settings.mqttUsername,val,USERNAME_SIZE);
    settings.mqttUsername[USERNAME_SIZE]='\0';
    saveSettings();
    }
  else if (strcmp(nme,"userPass")==0)
    {
    strncpy(settings.mqttUserPassword,val,PASSWORD_SIZE);
    settings.mqttUserPassword[PASSWORD_SIZE]='\0';
    saveSettings();
    }
  else if (strcmp(nme,"lwtMessage")==0)
    {
    strncpy(settings.mqttLWTMessage,val,MQTT_MAX_MESSAGE_SIZE);
    settings.mqttLWTMessage[MQTT_MAX_MESSAGE_SIZE]='\0';
    saveSettings();
    }
  else if (strcmp(nme,"topic")==0)
    {
    strncpy(settings.mqttTopic,val,MQTT_MAX_TOPIC_SIZE);
    settings.mqttTopic[MQTT_MAX_TOPIC_SIZE]='\0';
    saveSettings();
    }
  else if ((strcmp(nme,"resetmqttid")==0)&& (strcmp(val,"yes")==0))
    {
    generateMqttClientId(settings.mqttClientId);
    saveSettings();
    }
  else if (strcmp(nme,"mqttCommandTopic")==0)
    {
    strcpy(settings.mqttCommandTopic,val);
    saveSettings();
    }
  else if (strcmp(nme,"debug")==0)
    {
    settings.debug=strcmp(val,"false")==0?false:true;
    saveSettings();
    }
  else if (strcmp(nme,"pitch")==0)
    {
    settings.pitch=atoi(val);
    saveSettings();
    }
  else if (strcmp(nme,"dotLength")==0)
    {
    settings.dotLength=atoi(val);
    saveSettings();
    }
  else if ((strcmp(nme,"factorydefaults")==0) && (strcmp(val,"yes")==0)) //reset all eeprom settings
    {
    Serial.println("\n*********************** Resetting EEPROM Values ************************");
    initializeSettings();
    saveSettings();
    myDelay(2000);
    ESP.restart();
    }
  else if ((strcmp(nme,"reset")==0) && (strcmp(val,"yes")==0)) //reset the device
    {
    Serial.println("\n*********************** Resetting Device ************************");
    myDelay(1000);
    ESP.restart();
    }
  else
    {
    showSettings();
    }
  return;
  }


  /*
*  Initialize the settings from eeprom and determine if they are valid
*/
void loadSettings()
  {
  EEPROM.get(0,settings);
  if (settings.validConfig==VALID_SETTINGS_FLAG)    //skip loading stuff if it's never been written
    {
    settingsAreValid=true;
    if (settings.debug)
      Serial.println("Loaded configuration values from EEPROM");
    }
  else
    {
    Serial.println("Skipping load from EEPROM, device not configured.");    
    settingsAreValid=false;
    }
  }

int getSpecialCharIndex(char c)
  {
  int index=-1;
  for (int i=0; i<special_lookup_size; i++)
    {
    if (c==morse_special_lookup[i])
      {
      index=i;
      break;
      }
    }
  return index;
  }

void playMorse(const char* morse_code) {
  for (int j = 0; j < strlen(morse_code); j++) {
    if (morse_code[j] == '.') 
      {
      tone(TONE_PIN, settings.pitch, settings.dotLength);
      digitalWrite(led_pin, LOW);
      delay(settings.dotLength); // Dot duration
      digitalWrite(led_pin, HIGH);
      delay(settings.dotLength); // Inter-element gap
      }
    else if (morse_code[j] == '-') 
      {
      tone(TONE_PIN, settings.pitch, settings.dotLength*3);
      digitalWrite(led_pin, LOW);
      delay(settings.dotLength*3); // Dash duration
      digitalWrite(led_pin, HIGH);
      delay(settings.dotLength); // Inter-element gap
      }
    }
  delay(settings.dotLength*2); //three dot units, but one is already done above
  }


// Define the Morse code conversion function
void convert_to_morse(String message) {
  for (int i = 0; i < message.length(); i++) 
    {
    char c = message.charAt(i);
    Serial.print(c);
    if (c == ' ') 
      {
      delay(settings.dotLength*7); // Pause between words
      }
    else if (c >= '0' && c <= '9') //numbers
      { 
      int index = toupper(c) - '0';
      if (index >= 0 && index < 9) 
        {
        const char* morse_code = morse_number_table[index];
        playMorse(morse_code);   
        }
      }
    else if (toupper(c)>='A' && toupper(c)<='Z')
      {
      int index = toupper(c) - 'A';
      if (index >= 0 && index < 26) 
        {
        const char* morse_code = morse_text_table[index];
        playMorse(morse_code);
        }
      }
    else //might be punctuation or other character
      {
      int index=getSpecialCharIndex(c);
      if (index > -1)
        {
        const char* morse_code = morse_special_table[index];
        playMorse(morse_code);   
        }
      }
    client.loop(); //stay connected to broker
    }
  Serial.println("");
  }

// Define the MQTT callback function
void callback(char* topic, byte* payload, unsigned int length) 
  {
  if (settings.debug)
    {
    Serial.print("Topic is \"");
    Serial.print(topic);
    Serial.println("\"");
    }
  payload[length]='\0';
  String message = "";
  for (int i = 0; i < length; i++) 
    {
    message += (char)payload[i];
    }

  if (strcmp(topic,settings.mqttTopic)==0)
    {
    Serial.print(message);
    Serial.print("\n-->");
    convert_to_morse(message);
    }
  else if (strcmp(topic,settings.mqttCommandTopic)==0)
    {
    processCommand(message);
    }
  }


bool initInternals()
  {
  EEPROM.begin(sizeof(settings)); //fire up the eeprom section of flash
  commandString.reserve(200); // reserve 200 bytes of serial buffer space for incoming command string

  if (settings.debug)
    Serial.println(F("Loading settings"));
  loadSettings(); //set the values from eeprom

  Serial.print("Performing settings sanity check...");
  if ((settings.validConfig!=0 && 
      settings.validConfig!=VALID_SETTINGS_FLAG) || //should always be one or the other
      settings.brokerPort<0 ||
      settings.brokerPort>65535)
    {
    Serial.println("\nSettings in eeprom failed sanity check, initializing.");
    initializeSettings(); //must be a new board or flash was erased
    }
  else
    Serial.println("passed.");
  
  showSettings();
  return settingsAreValid;
  }


void setup() 
  {
  Serial.begin(115200);
  Serial.setTimeout(10000);
  Serial.println();

  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin,HIGH); //dp
  myDelay(2000);

  initInternals();

  initConnections();

  Serial.println(F("\nDavid's Super Morse Code Converter"));
  Serial.print(F("Publish any text to broker at "));
  Serial.print(settings.brokerAddress);
  Serial.print(F(" using topic \""));
  Serial.print(settings.mqttTopic);
  Serial.println(F("\""));

  }

void initConnections()
  {
  if (settingsAreValid)
    {
    if (WiFi.status() != WL_CONNECTED)
      {
      if (settings.debug)
        {
        Serial.print("Connecting to wifi using ");
        Serial.print(settings.ssid);
        Serial.print("/");
        Serial.print(settings.wifiPassword);
        }
      WiFi.begin(settings.ssid, settings.wifiPassword);
      while (WiFi.status() != WL_CONNECTED) 
        {
        myDelay(1500);
        if (settings.debug)
          Serial.print(".");
        }
      if (settings.debug)
        Serial.println("connected!");
      }

    if (WiFi.status() == WL_CONNECTED) //connect to mqtt broker
      {
      client.setServer(settings.brokerAddress, settings.brokerPort);
      client.setCallback(callback);
      while(!client.connected())
        {
        if (settings.debug)
          {
          Serial.print("Connecting to broker at ");
          Serial.println(settings.brokerAddress);
          }
        client.connect(settings.mqttClientId);
        myDelay(1000);
        }
      
      if (settings.debug)
        {
        Serial.print("Subscribing to topic \"");
        Serial.print(settings.mqttCommandTopic);
        Serial.println("\"");
        }
      if (!client.subscribe(settings.mqttCommandTopic))
        {
        Serial.print("Unable to subscribe to topic ");
        Serial.println(settings.mqttCommandTopic);
        }
      if (settings.debug)
        {
        Serial.print("Subscribing to topic \"");
        Serial.print(settings.mqttTopic);
        Serial.println("\"");
        }
      if (!client.subscribe(settings.mqttTopic))
        {
        Serial.print("Unable to subscribe to topic ");
        Serial.println(settings.mqttTopic);
        }
      }
    }
  }

void loop() 
  {
  checkForCommand(); // Check for input in case something needs to be changed to work

  if (settingsAreValid)
    {
    if (WiFi.status() != WL_CONNECTED)
      {
      initConnections();
      }

    client.loop();
    //myDelay(50);
    }
  }

/*
  SerialEvent occurs whenever a new data comes in the hardware serial RX. This
  routine is run between each time loop() runs, so using delay inside loop can
  delay response. Multiple bytes of data may be available.
  This didn't work correctly when I first used it so I renamed the function to
  incomingData() and the above statement no longer applies.
*/
void incomingData() 
  {
  while (Serial.available()) 
    {
    // get the new byte
    char inChar = (char)Serial.read();
    Serial.print(inChar);

    // if the incoming character is a newline, set a flag so the main loop can
    // do something about it 
    if (inChar == '\n') 
      {
      commandComplete = true;
      }
    else
      {
      // add it to the inputString 
      commandString += inChar;
      }
    }
  }

void checkForCommand()
  {
  incomingData();
  String cmd=getConfigCommand();
  if (cmd.length()>0)
    {
    processCommand(cmd);
    }
  }
