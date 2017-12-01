const char firmware_name[]     = "ESP_OTA_SecMQ_Encrypted";
const char firmware_version[]  = "0.0.1";
const char source_filename[]   = __FILE__;
const char compile_date[]      = __DATE__ " " __TIME__;


// WIFI, WIFI configuration and OTA
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "WiFiManager.h"
#include <xxtea-iot-crypt.h>
uint8_t MAC_array[6];
char MAC_char[18];

// MQTT
#include <PubSubClient.h>
#define SERVER      "secmq.com"
#define SERVERPORT  1883
#define USERNAME    "test"
#define PASSWORD    "test"
#define CLIENT_ID   "ESP_OTA_SecMQ_Encrypted"

char topic_deploy_send[100];
char topic_sensor_send[100];
char topic_sensor_receive[100];
char topic_update_receive[100];
// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
PubSubClient mqttclient(client);

// Sensors
#include <ArduinoJson.h>

#define pinRED   15
#define pinGREEN 12
#define pinBLUE  13

unsigned long previousMillis = 0;   
const long interval = 10*60*1000;

char *letters = "abcdefghijklmnopqrstuvwxyz0123456789";

/***
  WIFI configuration callback
***/
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println(F("Entered config mode"));
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

/***
  Update firmware function
***/
void updateFirmware() {
  Serial.println(F("Update command received"));
  t_httpUpdate_return ret = ESPhttpUpdate.update(SERVER, 80, "/esp/update.php", "0.0.1");
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.println(F("[update] Update failed."));
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(F("[update] Update no Update."));
      break;
    case HTTP_UPDATE_OK:
      Serial.println(F("[update] Update ok.")); // may not called we reboot the ESP
      break;
  }
}

/***
  MQTT receiver callback
***/
void mqttReceiveCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println(F("=================="));
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  Serial.println(F("=================="));
  Serial.println(F("Kind of message:"));

  /* update message */
  if (strcmp(topic, topic_update_receive) == 0) {
    updateFirmware();
  }
  /* received message */
  else if (strcmp(topic, topic_sensor_receive) == 0) {
    /* update leds */
     if (((char)payload[0]=='O') && ((char)payload[1]=='K')) {
        Serial.println("OK");
        /* green */
        analogWrite(pinRED, 0);
        analogWrite(pinGREEN, 255);
        analogWrite(pinBLUE, 0);

        // Create JSON message
        StaticJsonBuffer<500> jsonBuffer2;
        char sens_buff2[500];
        JsonObject& root2 = jsonBuffer2.createObject();
        root2["Type"] = firmware_name;
        root2["Version"] = firmware_version;
        root2["ChipId"] = String(ESP.getChipId(), HEX);
        JsonObject& data = root2.createNestedObject("d");
        data["Result"] = "Ok";
        data["Battery"] = (unsigned int) analogRead(A0);
        root2.printTo(sens_buff2, 500);
        Serial.print(F("Topic: "));
        Serial.println(topic_sensor_send);
        Serial.print(F("Message: "));
        Serial.println(sens_buff2);
        if (! mqttclient.publish(topic_sensor_send, sens_buff2)) {
            Serial.println(F("Send: Failed"));
        } else {
            Serial.println(F("Send: OK!"));
        }
      }
      else {
        Serial.println("Not OK");
        /* red */        
        analogWrite(pinRED, 255);
        analogWrite(pinGREEN, 0);
        analogWrite(pinBLUE, 0);

        // Create JSON message
        StaticJsonBuffer<500> jsonBuffer2;
        char sens_buff2[500];
        JsonObject& root2 = jsonBuffer2.createObject();
        root2["Type"] = firmware_name;
        root2["Version"] = firmware_version;
        root2["ChipId"] = String(ESP.getChipId(), HEX);
        JsonObject& data = root2.createNestedObject("d");
        data["Result"] = "Not Ok";
        data["Battery"] = (unsigned int) analogRead(A0);
        root2.printTo(sens_buff2, 500);
        Serial.print(F("Topic: "));
        Serial.println(topic_sensor_send);
        Serial.print(F("Message: "));
        Serial.println(sens_buff2);
        if (! mqttclient.publish(topic_sensor_send, sens_buff2)) {
            Serial.println(F("Send: Failed"));
        } else {
            Serial.println(F("Send: OK!"));
        }
        
      }
    //deep sleep
    //ESP.deepSleep(60000000, WAKE_RF_DEFAULT); //60 sec*/
  }
  /*default*/
  else {
    
  }

}

void setup() {
  //String(ESP.getChipId(), HEX).toCharArray(CLIENT_ID, 8);
  pinMode(pinRED, OUTPUT);
  pinMode(pinGREEN, OUTPUT);
  pinMode(pinBLUE, OUTPUT);
  /* blue */        
  analogWrite(pinRED, 0);
  analogWrite(pinGREEN, 0);
  analogWrite(pinBLUE, 255);
  
  Serial.begin(115200);
  Serial.println(F("=================="));
  Serial.println(source_filename);
  Serial.println(compile_date);
  Serial.println(F("=================="));
  Serial.println(F("Starting wifi"));
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback); //if it fail go to AP
  if (!wifiManager.autoConnect()) {
    Serial.println(F("failed to connect and hit timeout"));
    ESP.reset();
    delay(1000);
  }
  //if you get here you have connected to the WiFi
  Serial.println(F("connected!"));
  Serial.print(F("MAC address"));
  WiFi.macAddress(MAC_array);
  for (int i = 0; i < sizeof(MAC_array); ++i) {
    sprintf(MAC_char, "%s%02x:", MAC_char, MAC_array[i]);
  }
  Serial.println(MAC_char);
  Serial.println(F("=================="));

  // MQTT
  mqttclient.setServer(SERVER, SERVERPORT);
  mqttclient.setCallback(mqttReceiveCallback);
  String buf0 = String("secmqtest/") + USERNAME + String("/") + CLIENT_ID + String("/") + String(ESP.getChipId(), HEX) + String("/sensor/in");
  buf0.toCharArray(topic_sensor_send, 100);
  String buf1 = String("secmqtest/") + USERNAME + String("/") + CLIENT_ID + String("/") +  String(ESP.getChipId(), HEX) + String("/deploy/in");
  buf1.toCharArray(topic_deploy_send, 100);
  String buf2 = String("secmqtest/") + USERNAME + String("/") + CLIENT_ID + String("/") + String(ESP.getChipId(), HEX) + String("/commands/in");
  buf2.toCharArray(topic_sensor_receive, 100);
  String buf3 = String("secmqtest/") + USERNAME + String("/") + CLIENT_ID + String("/") + String(ESP.getChipId(), HEX) + String("/update/in");
  buf3.toCharArray(topic_update_receive, 100);

  if (! mqttclient.connected() ) {
    reconnect();
  }
  // Create JSON message
  StaticJsonBuffer<500> jsonBuffer;
  char sens_buff[500];
  JsonObject& root = jsonBuffer.createObject();
  root["Type"] = firmware_name;
  root["Version"] = firmware_version;
  root["Filename"] = source_filename;
  root["CompilationTime"] = compile_date;
  root["ChipId"] = String(ESP.getChipId(), HEX);
  root["Deploy"] = topic_deploy_send;
  root["Update"] = topic_update_receive;
  JsonArray& Pub = root.createNestedArray("Pub");
  Pub.add(topic_sensor_receive);
  JsonArray& Sub = root.createNestedArray("Sub");
  Sub.add(topic_sensor_send);
  root.printTo(sens_buff, 500);
  Serial.print(F("Topic: "));
  Serial.println(topic_deploy_send);
  Serial.print(F("Message: "));
  Serial.println(sens_buff);
  if (! mqttclient.publish(topic_deploy_send, sens_buff)) {
    Serial.println(F("Send: Failed"));
  } else {
    Serial.println(F("Send: OK!"));
  }

  // Set the Password
  xxtea.setKey("Test");

}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttclient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttclient.connect(CLIENT_ID, USERNAME, PASSWORD)) {
      Serial.println("connected");
      // esubscribe
      mqttclient.subscribe(topic_sensor_receive);
      mqttclient.subscribe(topic_update_receive);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) { //send message 
    previousMillis = currentMillis;
     /* red */        
    analogWrite(pinRED, 255);
    analogWrite(pinGREEN, 0);
    analogWrite(pinBLUE, 0);
    // Create JSON message
    StaticJsonBuffer<500> jsonBuffer;
    char sens_buff[500];
    JsonObject& root = jsonBuffer.createObject();
    String value = String(analogRead(A0));
    String salt = "";
    int i;
    for(i = 0; i<20; i++){
      salt = salt + letters[random(0, 36)];
    }
    String en_value = xxtea.encrypt(value+"|"+salt);
    root["LightXXTEA"] = en_value;
    root.printTo(sens_buff, 500);
    Serial.print(F("Topic: "));
    Serial.println(topic_deploy_send);
    Serial.print(F("Message: "));
    Serial.println(sens_buff);
    if (! mqttclient.connected() ) {
      reconnect();
    }
    if (! mqttclient.publish(topic_sensor_send, sens_buff)) {
      Serial.println(F("Send: Failed"));
    } else {
      Serial.println(F("Send: OK!"));
    }
    /* green */        
    analogWrite(pinRED, 0);
    analogWrite(pinGREEN, 255);
    analogWrite(pinBLUE, 0);
    }
    
    if (! mqttclient.connected() ) {
      reconnect();
    }
    mqttclient.loop();
  
  /* black */        
  analogWrite(pinRED, 0);
  analogWrite(pinGREEN, 0);
  analogWrite(pinBLUE, 0);
  delay(100);
}


