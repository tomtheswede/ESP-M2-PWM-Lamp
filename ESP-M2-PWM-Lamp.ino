
/*  
 *   For a LED lamp with an ESP8285 M2 chip.
 *   Most code by Thomas Friberg
 *   Leverages code from https://github.com/mertenats/Open-Home-Automation/blob/master/ha_mqtt_rgbw_light_with_discovery/ha_mqtt_rgbw_light_with_discovery.ino
 *   Updated 08/03/2021
 *   
 */ 

//Import libraries
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Replace the next variables with your SSID/Password combination
char* wifi_ssid = "[Replace this]";
char* wifi_password = "[Replace this]";

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
//LED
const int ledPin = 15;
//const int ledChannel = 1; //ESP32 specific
//const int pwmFreq = 5000; //ESP32 specific
//const int pwmResolution = 10; //ESP32 specific
const int defaultFade = 20; //Milliseconds between fade intervals
static const unsigned int PWMTable[101] = {0,1,2,3,5,6,7,8,9,10,12,13,14,16,18,20,21,24,26,28,31,33,36,39,42,45,49,52,56,60,64,68,72,77,82,87,92,98,103,109,115,121,128,135,142,149,156,164,172,180,188,197,206,215,225,235,245,255,266,276,288,299,311,323,336,348,361,375,388,402,417,432,447,462,478,494,510,527,544,562,580,598,617,636,655,675,696,716,737,759,781,803,826,849,872,896,921,946,971,997,1023}; //0 to 100 values for brightnes
//Button
const int buttonPin = 4;

// Initialise instances
WiFiClient espClient;
PubSubClient client(espClient);

// Initialise global operating variables
//long lastMsgTime = 0;

// Initialise global LED variables
int ledPinState = 0;
int ledSetPoint = 0;
int lastBrightness = 100;

// Initialise global button variables
bool lastButtonState=0;
bool buttonState=0;
long buttonTriggerTime=millis();
long currentTime=millis();
bool primer[4]={0,0,0,0};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP
/////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  setupPins();
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
  pinMode(ledPin, OUTPUT); //Set as output
  digitalWrite(ledPin, 0); //Turn off LED while connecting
  //ledcSetup(ledChannel, pwmFreq, pwmResolution); // configure LED PWM functionalitites
  //ledcAttachPin(ledPin, ledChannel); // attach the channel to the GPIO to be controlled
  //TODO - reinstate state from MQTT server if available - after wifi connection
  
  //button setup
  pinMode(buttonPin,INPUT_PULLUP); //pullup is suitable for momentary type tactile buttons
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_clientname, mqtt_username, mqtt_password)) {
      Serial.println("connected");
      // Discovery and subscription
      publishLightDiscovery("light", "light", devName, "led", "Bench light");
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
  json["schema"] = "json"; 
  json["bri_scl"] = 100;


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
  //Serial.print("Saved root topic: ");
  //Serial.println(ledRootTopic);

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

  fadeLEDs();
  buttonCheck();
  
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

  parseMessage(messageTemp);

}

void parseMessage(String messageIn) { //Based on https://arduinojson.org/v6/example/parser/
  //Parse JSON datagram
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, messageIn);
  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  
  const char* stateMessage = doc["state"];
  int brightnessIn = doc["brightness"];
  //Serial.println(stateMessage);
  //Serial.println(brightnessIn);
  String stateString(stateMessage);

  // Feel free to add more if statements to control more GPIOs with MQTT

  publishLedState(stateString,brightnessIn);
  
}

void publishLedState(String ledState, int ledBrightness) {
  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if(ledState == "ON"){
    //Serial.println("ON state detected");
    StaticJsonDocument<200> json;
    json["state"] = "ON";
    if (ledBrightness==0) {
      ledSetPoint = lastBrightness; //Set new brightness for LED
      json["brightness"] = lastBrightness;
    }
    else {
      lastBrightness = ledBrightness;
      ledSetPoint = lastBrightness; //Set new brightness for LED
      json["brightness"] = lastBrightness;
    }
    //Send state update back to base
    char tempStateTopic[200] = ""; //Reset the variable before concatinating
    strcat(tempStateTopic,ledRootTopic);
    strcat(tempStateTopic,"/state");
    char data[200];
    serializeJson(json, data);
    client.publish(tempStateTopic, data, true);
    Serial.print("Sent state: ");
    Serial.println(data);
  }
  else if(ledState == "OFF"){
    //Serial.println("OFF state detected");
    ledSetPoint = 0;
    //Send state update back to base
    char tempStateTopic[200] = ""; //Reset the variable before concatinating
    strcat(tempStateTopic,ledRootTopic);
    strcat(tempStateTopic,"/state");
    StaticJsonDocument<200> json;
    json["state"] = "OFF";
    //json2["brightness"] = lastBrightness;
    char data[200];
    serializeJson(json, data);
    client.publish(tempStateTopic, data, true);
    Serial.print("Sent state: ");
    Serial.println(data);
  }
}

void fadeLEDs() {
  if ((millis() % defaultFade == 0) && (ledPinState < ledSetPoint)) {
    ledPinState = ledPinState + 1;
    analogWrite(ledPin,PWMTable[ledPinState]);
    //ledcWrite(ledChannel, PWMTable[ledPinState]); //AnalogOut is the alternative for ESP8266 devices
    //Serial.println("LED state is now set to " + String(ledPinState));
    delay(1); //Consider less wasteful timer
  }
  else if ((millis() % defaultFade == 0) && (ledPinState > ledSetPoint)) {
    ledPinState = ledPinState - 1;
    analogWrite(ledPin,PWMTable[ledPinState]);
    //ledcWrite(ledChannel, PWMTable[ledPinState]);
    //Serial.println("LED state is now set to " + String(ledPinState));
    delay(1);
  }
}

void buttonCheck() {
  buttonState=(!digitalRead(buttonPin)); //Use (!digitalRead(buttonPin) for a momentary button
  //Serial.println(buttonState);
  //delay(100);
  currentTime=millis();
  StaticJsonDocument<200> json;
  if (buttonState!=lastButtonState && currentTime>buttonTriggerTime+30) {
    if (buttonState && currentTime>buttonTriggerTime+200) {
      Serial.print("Short press ");
      defaultToggleBehaviour();
      buttonTriggerTime=currentTime;
      primer[0]=1;
      primer[1]=1;
      primer[2]=1;
      primer[3]=1;
    }
    else if (!buttonState) {
      primer[0]=0;
      primer[1]=0;
      primer[2]=0;
      primer[3]=0;
    }
  }
  lastButtonState=buttonState;
  if (primer[0] && (currentTime>buttonTriggerTime+700)) {
    Serial.print("Mid press ");
    publishLedState("ON",1);
    primer[0]=0;
  }
  else if (primer[1] && (currentTime>buttonTriggerTime+1500)) {
    Serial.print("Long press ");
    publishLedState("ON",100);
    primer[1]=0;
  }
  else if (primer[2] && (currentTime>buttonTriggerTime+4000)) {
    //blank action
    primer[2]=0;
  }
  else if (primer[3] && (currentTime>buttonTriggerTime+8000)) { //8 second delay for 
    //Blank action
    primer[3]=0;
  }
}

void defaultToggleBehaviour() { //Pauses dimming behaviour if button pressed mid-dimming
  if (ledPinState==ledSetPoint && ledSetPoint==0) {
    publishLedState("ON",lastBrightness); //regular toggle on when off
  }
  else if (ledPinState==ledSetPoint) {
    publishLedState("OFF",lastBrightness); //regular toggle off when on
  }
  else if (ledPinState!=ledSetPoint)
    publishLedState("ON",ledPinState); //pause dimming at current pin state
}
