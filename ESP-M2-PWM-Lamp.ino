/*  
 *   For a LED lamp with an ESP8285 M2 chip.
 *   Most code by Thomas Friberg
 *   Leverages code from https://github.com/mertenats/Open-Home-Automation/blob/master/ha_mqtt_rgbw_light_with_discovery/ha_mqtt_rgbw_light_with_discovery.ino
 *   Updated 07/03/2021
 *   
 */ 

//Import libraries
#include <WiFi.h>  //SWAP THIS FOR AN ESP8266 WIFI library!!!
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Replace the next variables with your SSID/Password combination
char* wifi_ssid = "[replace this]";
char* wifi_password = "[replace this]";

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "192.168.1.117";
const int mqtt_port = 1883;
const char* mqtt_clientname = "espclient";
const char* mqtt_username = "user";
const char* mqtt_password = "passwd";
const char topicPrefix[] = "homeassistant/";
char ledRootTopic[200] = "";

// Device preferences
const char devName[] = "kitchenBenchLamp"; //machine device name. Set human readable names in discovery
const char devLocation[] = "kitchen"; //Not currently used
//const int ledPin = 12;
//const int defaultFade = 15; //Milliseconds between fade intervals
//static const unsigned int PWMTable[101] = {0,1,2,3,5,6,7,8,9,10,12,13,14,16,18,20,21,24,26,28,31,33,36,39,42,45,49,52,56,60,64,68,72,77,82,87,92,98,103,109,115,121,128,135,142,149,156,164,172,180,188,197,206,215,225,235,245,255,266,276,288,299,311,323,336,348,361,375,388,402,417,432,447,462,478,494,510,527,544,562,580,598,617,636,655,675,696,716,737,759,781,803,826,849,872,896,921,946,971,997,1023}; //0 to 100 values for brightnes

// Initialise instances
WiFiClient espClient;
PubSubClient client(espClient);

// Initialise global operating variables
//long lastMsgTime = 0;

// Initialise global LED variables
//int ledPinState = 0;

// Initialise global button variables
//bool lastButtonState=0;
//bool buttonState=0;
//long buttonTriggerTime=millis();
//long currentTime=millis();

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP
/////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  //setupPins();
  setupWifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void setupWifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  delay(10);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupPins() {
  //Set pins and turn off LED by default
  //pinMode(ledPin, OUTPUT); //Set as output
  //digitalWrite(ledPin, 0); //Turn off LED while connecting
  //TODO - reinstate state from MQTT server if available - after wifi connection
  
  //button setup
  //pinMode(devPin[devices],INPUT_PULLUP);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_clientname, mqtt_username, mqtt_password)) {
      Serial.println("connected");
      // Discovery and subscription
      publishLightDiscovery("light", "light", devName, "led", "Kitchen bench light");
      //publishButtonDiscovery("switch", "light", devName, "led", "Kitchen bench light");
      
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

char publishLightDiscovery(const char *component, const char *device_class, const char *device_id, const char *entity_id, const char *name_suffix) {
  //component says what sort of discovery fields to include as well as setting MQTT path prefix, device_class sets icon default, config_Key defines the MQTT path and uniqueID and must be unique, name_suffix is the human description of the entity excluding location

  DynamicJsonDocument json(1024);
  //if (device_class) json["device_class"] = device_class;
  json["name"] = name_suffix;
  char tempTopicBuilder1[200] = "";
  strcat(tempTopicBuilder1,device_id);
  strcat(tempTopicBuilder1,"_");
  strcat(tempTopicBuilder1,entity_id);
  json["uniq_id"] = tempTopicBuilder1;
  char tempTopicBuilder2[200] = ""; //Reset the variable before concatinating
  strcat(tempTopicBuilder2,topicPrefix);
  strcat(tempTopicBuilder2,component);
  strcat(tempTopicBuilder2,"/");
  strcat(tempTopicBuilder2,device_id);
  json["~"] = tempTopicBuilder2;  //eg. "homeassistant/light/kitchen/espLamp/led";
  char* subTopicTemplate = tempTopicBuilder2;
  char tempTopicBuilder4[200] = ""; //Reset the variable before concatinating
  strcat(tempTopicBuilder4,"~/state");
  json["stat_t"] = tempTopicBuilder4;  //"homeassistant/light/kitchen/espLamp/led/state";
  char tempTopicBuilder5[200] = ""; //Reset the variable before concatinating
  strcat(tempTopicBuilder5,"~/set");
  json["cmd_t"] = tempTopicBuilder5; 
  json["brightness"] = true; 
  json["brightness_scale"] = 100; 

  //prep for send
  char data[200];
  serializeJson(json, data);
  //size_t n = serializeJson(json, data);
  char tempTopicBuilder3[200] = ""; //Reset the variable before concatinating
  strcat(tempTopicBuilder3,subTopicTemplate);
  strcat(tempTopicBuilder3,"/");
  strcat(tempTopicBuilder3,entity_id);
  strcat(tempTopicBuilder3,"/config");
  client.publish(tempTopicBuilder3, data, true); //publishing the discovery note
  
  Serial.print("Discovery pub complete: ");
  Serial.println(data);

  //Subscribe
  char tempTopicBuilder6[200] = ""; //Reset the variable before concatinating
  strcat(tempTopicBuilder6,subTopicTemplate);
  strcat(tempTopicBuilder6,"/set");
  client.subscribe(tempTopicBuilder6);
  Serial.print("Subscribed to: ");
  Serial.println(tempTopicBuilder6);

  strcpy(ledRootTopic,subTopicTemplate);
  Serial.print("Saved root topic: ");
  Serial.println(ledRootTopic);

}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// LOOP FUNCTIONS
/////////////////////////////////////////////////////////////////////////////////////////////////////////


void loop() {
  //Get connected to MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  //Parse JSON datagram
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, messageTemp);
  const char* stateMessage = doc["state"];
  //Serial.println(stateMessage);
  String stateString(stateMessage);

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
    if(stateString == "ON"){
      Serial.println("ON state detected");
//      digitalWrite(ledPin, HIGH);
      //Send state update back to base
      char tempStateTopic[200] = ""; //Reset the variable before concatinating
      strcat(tempStateTopic,ledRootTopic);
      strcat(tempStateTopic,"/state");
      DynamicJsonDocument json2(1024);
      json2["state"] = "ON";
      char data[200];
      serializeJson(json2, data);
      client.publish(tempStateTopic, data, true);
    }
    else if(stateString == "OFF"){
      Serial.println("OFF state detected");
//      digitalWrite(ledPin, LOW);
      //Send state update back to base
      char tempStateTopic[200] = ""; //Reset the variable before concatinating
      strcat(tempStateTopic,ledRootTopic);
      strcat(tempStateTopic,"/state");
      DynamicJsonDocument json2(1024);
      json2["state"] = "OFF";
      char data[200];
      serializeJson(json2, data);
      client.publish(tempStateTopic, data, true);
    }
}
