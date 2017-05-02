#ifndef STEAMLINKESP_H
#define STEAMLINKESP_H

#ifdef ESP8266

#include <SPI.h>
#include <ESP8266WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <SteamLinkGeneric.h>
#include <SteamLink.h>

#define SL_ESP_DEFAULT_TOPIC_LEN 100

///////////////////////////////////////////////////////////////////////////
//   STEAMLINK CREDENTIALS
///////////////////////////////////////////////////////////////////////////

#ifndef VER

// WiFi Creds
struct Credentials {
  char *ssid;
  char *pass;
};

// WiFI credential entries
struct Credentials creds[] =
  {
    { "SSID1", "PPP" },
    { "SSID2", "PPP" },
    { "SSID3", "PPP"}
  };

// numer of secods to wait for a WiFi connection
#define WIFI_WAITSECONDS 15

// MQTT broker
#define SL_SERVER      "mqtt.steamlink.net"
#define SL_SERVERPORT  8883                   // 8883 for MQTTS

// SHA1 fingerprint for mqtt.steamlink.net's SSL certificate
const char* sl_server_fingerprint = "E3 B9 24 8E 45 B1 D2 1B 4C EF 10 61 51 35 B2 DE 46 F1 8A 3D";

// Bridge Token
#define SL_USERNAME    "UUUU"
#define SL_CONID       "CON"
#define SL_KEY         "PPPP"

#endif
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////


class SteamLinkESP : public SteamLinkGeneric {
  public:

  /// constructor
  SteamLinkESP(uint32_t slid);

  virtual void init(bool encrypted=false, uint8_t* key=NULL);

  /// send
  virtual bool send(uint8_t* buf);

  virtual void update();

  /// bridge_send
  virtual bool bridge_send(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi);

  virtual bool admin_send(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi);

  virtual void register_receive_handler(on_receive_handler_function on_receive);

  virtual void register_bridge_handler(on_receive_bridge_handler_function on_receive);

  virtual void register_admin_handler(on_receive_bridge_handler_function on_receive);

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

  void wifi_connect();
  bool mqtt_connect();

  void create_direct_publish_str(char* topic, uint32_t slid);
  void create_direct_subscriber_str(char* topic, uint32_t slid);
  void create_transport_publish_str(char* topic, uint32_t slid);
  void create_transport_subscriber_str(char* topic, uint32_t slid);
  void create_admin_publish_str(char* topic, uint32_t slid);
  void create_admin_subscriber_str(char* topic, uint32_t slid);

  static void _direct_sub_callback(char* data, uint16_t len);
  static void _transport_sub_callback(char* data, uint16_t len);
  static void _admin_sub_callback(char* data, uint16_t len);

  static on_receive_handler_function  _on_local_receive;
  static on_receive_bridge_handler_function  _on_local_bridge_receive;
  static on_receive_bridge_handler_function  _on_local_admin_receive;

};

#endif
#endif
