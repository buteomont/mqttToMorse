#define LED_ON LOW
#define LED_OFF HIGH
#define WIFI_CONNECTION_ATTEMPTS 150
#define VALID_SETTINGS_FLAG 0xDAB0
#define SSID_SIZE 100
#define PASSWORD_SIZE 50
#define ADDRESS_SIZE 30
#define USERNAME_SIZE 50

#define MQTT_CLIENTID_SIZE 25
#define DEFAULT_MQTT_BROKER_PORT 1883
#define MQTT_MAX_TOPIC_SIZE 100
#define MQTT_MAX_MESSAGE_SIZE 15
#define HISTORY_BUFFER_SIZE 100
#define DEFAULT_MQTT_TOPIC "morse_code"
#define MQTT_CLIENT_ID_ROOT "morseCode"
#define MQTT_TOPIC_RSSI "rssi"
#define MQTT_TOPIC_STATUS "status"
#define DEFAULT_MQTT_LWT_MESSAGE "disconnected"
#define DEFAULT_MQTT_COMMAND_TOPIC "morse_code/command"

#define DEFAULT_TONE_PITCH 1000
#define DEFAULT_DOT_LENGTH 200


//prototypes
void showSettings();
String getConfigCommand();
boolean saveSettings();
void initializeSettings();
void processCommand(String cmd);
void loadSettings();
void playMorse(const char* morse_code);
void convert_to_morse(String message);
void callback(char* topic, byte* payload, unsigned int length);
bool initInternals();
void setup();
void loop();
void incomingData();
void checkForCommand();
void initConnections();