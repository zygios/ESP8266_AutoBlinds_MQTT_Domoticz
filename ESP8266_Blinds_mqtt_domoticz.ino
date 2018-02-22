#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Stepper2.h>

#define DEBUG

#ifdef DEBUG
#  define DEBUG_LOG(x) Serial.print(x)
#  define DEBUG_LOGLN(x) Serial.println(x)
#else
#  define DEBUG_LOG(x)
#  define DEBUG_LOGLN(x)
#endif

// Update these with values suitable for your network.
// WiFi parameters
const char* ssid = "xxx";
const char* password = "xxx";

// MQTT parameters
const char* mqtt_server = "192.168.1.zz";
const char* topic_Domoticz_IN  = "domoticz/in";   
const char* topic_Domoticz_OUT = "domoticz/out";  
byte          willQoS            = 0;
char          willMessage[MQTT_MAX_PACKET_SIZE+1];
boolean       willRetain         = false;

int device_num = 2;
int  Domoticz_IDX[] = { 35, 36 }; 
String  Device_prefix = "Living_room";
String  FullNames[] = {Device_prefix + "_Left", Device_prefix + "_Right"};
int     cur_state[] ={0,0};  //current device status      

//stepper parameters
const int rpm = 15; // max rpm on 28BYJ-48 is ~15
int pinOut[4] = { 14, 12, 13, 15 };
Stepper2 myStepper(pinOut);     
byte maxStep = 4;
boolean motor_runing = false;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(100);
  // We start by connecting to a WiFi network
  DEBUG_LOG("Connecting to ");
  DEBUG_LOGLN(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    DEBUG_LOG(".");
  }
  randomSeed(micros());
  DEBUG_LOGLN("");
  DEBUG_LOGLN("WiFi connected");
  DEBUG_LOGLN("IP address: ");
  DEBUG_LOGLN(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  DynamicJsonBuffer jsonBuffer( MQTT_MAX_PACKET_SIZE );
  String messageReceived="";
  
  // Affiche le topic entrant - display incoming Topic
  DEBUG_LOG("MQTT Message arrived [");
  DEBUG_LOG(topic);
  DEBUG_LOGLN("] ");

  // decode payload message
  for (int i = 0; i < length; i++) {
    messageReceived+=((char)payload[i]); 
  }
  // display incoming message
  DEBUG_LOG(messageReceived);
  
  if ( strcmp(topic, topic_Domoticz_OUT) == 0 ) {
    JsonObject& root = jsonBuffer.parseObject(messageReceived);
    if (!root.success()) {
      DEBUG_LOGLN("parsing Domoticz/out JSON Received Message failed");
      return;
    }

    int idx = root["idx"];
    byte Dom_cmd = root["nvalue"];
    byte Dom_level;
    byte loc_dev_num ;
    for (byte i=0; i < device_num; i++) {
      if (idx == Domoticz_IDX[i]) {
        loc_dev_num = i;
        break;
     }
   }

   if( (idx == Domoticz_IDX[0]) || (idx == Domoticz_IDX[1])){
      DEBUG_LOGLN("DEV_NUM:"+String(loc_dev_num));
      
      if(Dom_cmd == 0){ //domoticz command open blinds
        Dom_level = 0;  //setting level 0%
        DEBUG_LOGLN("Opening blinds");
        blinds_set_level(loc_dev_num, Dom_level);
        DEBUG_LOGLN("Blinds are open!");
      }else if(Dom_cmd == 1){  //domoticz command close blinds
        Dom_level = 100;  //setting level 100%
        DEBUG_LOGLN("Closing blinds");
        blinds_set_level(loc_dev_num, Dom_level);
        DEBUG_LOGLN("Blinds are closed!");
      }else if(Dom_cmd == 2){  //domoticz command set percentage values
        Dom_level = root["svalue1"];
        DEBUG_LOGLN("Blind starting to set to "+String(Dom_level)+"%");
        blinds_set_level(loc_dev_num, Dom_level);
        DEBUG_LOGLN("Blind is set to "+String(Dom_level)+"%");
      }
    }
  }
}

void blinds_set_level(byte dev_num, byte gotoLevel){
  int goToStep = map (gotoLevel, 0, 100, 0, maxStep);
  if(goToStep >= cur_state[dev_num] ) {
        myStepper.setDirection(1); // clock-wise
        for (int tempPos = cur_state[dev_num]; tempPos < goToStep; tempPos++) {
          myStepper.turn(); // one full turn
          cur_state[dev_num]++;
        }
    }else{
        myStepper.setDirection(0);
        for (int tempPos = cur_state[dev_num]; tempPos > goToStep; tempPos--) {
          myStepper.turn(); // one full turn
          cur_state[dev_num]--;
        }
  }
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) 
  {
    DEBUG_LOG("Attempting MQTT connection...");
    // Create client ID device prefix + a random number
    String clientId = Device_prefix + String(random(0xffff), HEX);
    // Attempt to connect
    //if you MQTT broker has clientID,username and password
    //please change following line to    if (client.connect(clientId,userName,passWord))
    if (client.connect(clientId.c_str()))
    {
      DEBUG_LOGLN("connected");
     //once connected to MQTT broker, subscribe command if any
      client.subscribe(topic_Domoticz_IN);
      client.subscribe(topic_Domoticz_OUT);
    } else {
      DEBUG_LOGLN("failed, rc=");
      DEBUG_LOG(client.state());
      DEBUG_LOGLN(" try again in 5 seconds");
      // Wait 6 seconds before retrying
      delay(6000);
    }
  }
} //end reconnect()


void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  myStepper.setSpeed(rpm);
}
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(100);
}

