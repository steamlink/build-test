#ifndef STEAMLINK_PRIMARY_NODE_H
#define STEAMLINK_PRIMARY_NODE_H


#include <SPI.h>
#include <ESP8266WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

#include "SteamLinkPackets.h"
#include "SL_Credentials.h"

#define WIFI_RETRY_COUNT 5

class SteamLinkPrimaryNode {

 public:

  // on receive strings
  typedef void (*on_receive_handler_function)(uint8_t* buffer);

  // initialize
  bool init();

  // send strings
  bool send(uint8_t* buf);

  // run update in loop
  void update();

  // register_handler(on_receive)
  void register_handler(on_receive_handler_function on_receive);

 private:

  uint8_t my_slid;

  WiFiClientSecure *client;
  Adafruit_MQTT_Client *mqtt;
  Adafruit_MQTT_Publish *publish;
  Adafruit_MQTT_Subscribe *subscribe;

  // connect to wifi
  void wifi_connect();

  // connect to mqtt
  void mqtt_connect();

  // verify fingerprint
  void verifyFingerprint();
}

#endif







