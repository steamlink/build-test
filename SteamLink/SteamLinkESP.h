#ifndef STEAMLINKESP_H
#define STEAMLINKESP_H

#include <ESP8266WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

#define SL_ESP_DEFAULT_TOPIC_LEN 100

class SteamLinkESP : public SteamLinkGeneric {
  public:

  /// constructor
  SteamLinkESP(uint32_t slid);

  virtual void init(bool encrypted=false, uint8_t* key=NULL);

  /// \send
  virtual bool send(uint8_t* buf);

  virtual void update();

  /// bridge_send
  virtual bool bridge_send(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi);

 private:

  char _direct_publish_str[SL_ESP_DEFAULT_TOPIC_LEN];
  char _direct_subscribe_str[SL_ESP_DEFAULT_TOPIC_LEN];
  char _transport_publish_str[SL_ESP_DEFAULT_TOPIC_LEN];
  char _transport_subscribe_str[SL_ESP_DEFAULT_TOPIC_LEN];

  char _admin_publish_str[SL_ESP_DEFAULT_TOPIC_LEN];
  char _admin_subscribe_str[SL_ESP_DEFAULT_TOPIC_LEN];

  WiFiClientSecure _client;
  Adafruit_MQTT_Client* _mqtt;

  Adafruit_MQTT_Publish* _direct_publish;
  Adafruit_MQTT_Subscribe* _direct_subscribe;

  Adafruit_MQTT_Publish* _transport_publish;
  Adafruit_MQTT_Subscribe* _transport_subscribe;

  Adafruit_MQTT_Publish* _admin_publish;
  Adafruit_MQTT_Subscribe* _admin_subscribe;

  on_receive_handler_function _on_receive = NULL;
  on_receive_handler_function _on_bridge_receive = NULL;

  void wifi_connect();
  void mqtt_connect();

  void create_direct_publish_str(char* topic, uint32_t slid);
  void create_direct_subscriber_str(char* topic, uint32_t slid);
  void create_transport_publish_str(char* topic, uint32_t slid);
  void create_transport_subscriber_str(char* topic, uint32_t slid);
  void create_admin_publish_str(char* topic, uint32_t slid);
  void create_admin_subscriber_str(char* topic, uint32_t slid);

}

#endif
