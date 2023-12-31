#include <Arduino.h>
/*
  Modified by Murat Gungor
  Azure IoT Hub WiFi

  This sketch securely connects to an Azure IoT Hub using MQTT over WiFi.
  It uses a private key stored in the ATECC508A and a self signed public
  certificate for SSL/TLS authetication.

  It publishes a message every 5 seconds to "devices/{deviceId}/messages/events/" topic
  and subscribes to messages on the "devices/{deviceId}/messages/devicebound/#"
  topic.

  The circuit:
  - Arduino MKR WiFi 1010 or MKR1000

  This example code is in the public domain.
*/
#include <Arduino.h>

#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <utility/ECCX08SelfSignedCert.h>
#include <ArduinoMqttClient.h>
#include <WiFiNINA.h>

#include "arduino_secrets.h"
#include "zone_lighting_controller.h"

#include <string> //C++ std::string
#include <cstring> //C style string funcs

/////// Enter your sensitive data in arduino_secrets.h
const char ssid[]        = SECRET_WIFI_SSID;
const char pass[]        = SECRET_WIFI_PASS;
const char broker[]      = SECRET_BROKER;
String     deviceId      = SECRET_DEVICE_ID;
String     devicePass    = SECRET_DEVICE_PASSWORD;

const int max_mqtt_buffer_len = 256;
char mqtt_buffer[max_mqtt_buffer_len] = {0};

WiFiClient    wifiClient;            // Used for the TCP socket connection
BearSSLClient sslClient(wifiClient); // Used for SSL/TLS connection, integrates with ECC508
MqttClient    mqttClient(sslClient);
ZoneLightingController lightingController;

unsigned long lastMillis = 0;

unsigned long getTime() {
  // get the current time from the WiFi module
  return WiFi.getTime();
}

void connectWiFi() {
  Serial.print("Attempting to connect to SSID: ");
  Serial.print(ssid);
  Serial.print(" ");

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(5000);
  }
  Serial.println();

  Serial.println("You're connected to the network");
  Serial.println();
}

void connectMQTT() {
  Serial.print("Attempting to MQTT broker: ");
  Serial.print(broker);
  Serial.println(" ");

  while (!mqttClient.connect(broker, 8883)) {
    // failed, retry
    Serial.print("Connecting to MQTT broker - Failed Error Code:");
    Serial.println(mqttClient.connectError());
    delay(5000);
  }
  Serial.println();

  Serial.println("You're connected to the MQTT broker");
  Serial.println();

  // subscribe to a topic
  mqttClient.subscribe("devices/" + deviceId + "/messages/devicebound/#");
}

// void publishMessage() {
//   Serial.println("Publishing message");

//   // send message, the Print interface can be used to set the message contents
//   mqttClient.beginMessage("devices/" + deviceId + "/messages/events/");
//   mqttClient.print("hello ");
//   mqttClient.print(millis());
//   mqttClient.endMessage();
// }

void onMessageReceived(int messageSize) {
  // we received a message, print out the topic and contents
  Serial.print("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.print("', length ");
  Serial.print(messageSize);

 int buffer_len = mqttClient.available();

 if(buffer_len == 0)
 {
  Serial.println("nothing in mqtt buffer");
  return;
 }
 
 if(buffer_len > max_mqtt_buffer_len)
 {
  Serial.println("incoming message too big, update max_mqtt_buffer_len and recompile");
  mqttClient.flush();
  return;
 }

  mqttClient.read((uint8_t*)mqtt_buffer, buffer_len);
  Serial.println(" bytes:");
  Serial.print(mqtt_buffer);
  Serial.println();
  Serial.println();

  bool the_message_was_understood_and_acted_on = lightingController.HighlightZone(std::string(mqtt_buffer, strnlen(mqtt_buffer, max_mqtt_buffer_len)));

  mqttClient.beginMessage("devices/" + deviceId + "/messages/events/");
  mqttClient.print("the command string <");
  mqttClient.print(mqtt_buffer);
  mqttClient.print("> was ");
  mqttClient.print(the_message_was_understood_and_acted_on?"processed successfully":"processed unsuccessfully");
  mqttClient.endMessage();
  if(lightingController.HighlightZone(std::string(mqtt_buffer, strnlen(mqtt_buffer, max_mqtt_buffer_len))))
  {
    Serial.println("the command string <");
    Serial.print(mqtt_buffer);
    Serial.print("> did not equate to any known operation");
  }

}

void setup() {

  delay(5000); //initial delay so mistakes don't make a board unprogrammable

  Serial.begin(9600);
  while (!Serial);

  lightingController.Initialize();

  if (!ECCX08.begin()) {
    Serial.println("No ECCX08 present!");
    while (1);
  }

  // reconstruct the self signed cert
  ECCX08SelfSignedCert.beginReconstruction(0, 8);
  ECCX08SelfSignedCert.setCommonName(ECCX08.serialNumber());
  ECCX08SelfSignedCert.endReconstruction();

  // Set a callback to get the current time
  // used to validate the servers certificate
  ArduinoBearSSL.onGetTime(getTime);

  // Set the ECCX08 slot to use for the private key
  // and the accompanying public certificate for it
  sslClient.setEccSlot(0, ECCX08SelfSignedCert.bytes(), ECCX08SelfSignedCert.length());

  // Set the client id used for MQTT as the device id
  mqttClient.setId(deviceId);

  // Set the username to "<broker>/<device id>/api-version=2018-06-30" and empty password
  String username;

  username += broker;
  username += "/";
  username += deviceId;
  username += "/api-version=2018-06-30";
 
  Serial.print("Username:\t");  Serial.println(username);
  Serial.print("Device Pass:\t");  Serial.println(devicePass);

  mqttClient.setUsernamePassword(username, devicePass);
  
  // Set the message callback, this function is
  // called when the MQTTClient receives a message
  mqttClient.onMessage(onMessageReceived);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!mqttClient.connected()) {
    // MQTT client is disconnected, connect
    connectMQTT();
  }

  // poll for new MQTT messages and send keep alives
  mqttClient.poll();

  // publish a message roughly every 5 seconds.
  if (millis() - lastMillis > 5000) {
    lastMillis = millis();

    // publishMessage();

    // lightingController.HighlightZone("Zone1");
    // lightingController.HighlightZone("Zone2");
    // lightingController.HighlightZone("Zone3");
    // lightingController.HighlightZone("Zone4");
    
  }
}