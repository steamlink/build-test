#ifdef ESP8266
#include <SteamLinkESP.h>
#include <SteamLinkPacket.h>
#include <SteamLink.h>

SteamLinkESP::SteamLinkESP(uint32_t slid) : SteamLinkGeneric(slid) {
  _slid = slid;
  create_direct_publish_str(_direct_publish_str, slid);
  create_direct_subscriber_str(_direct_subscribe_str, slid);

  create_transport_publish_str(_transport_publish_str, slid);
  create_transport_subscriber_str(_transport_subscribe_str, slid);

  create_admin_publish_str(_admin_publish_str, slid);
  create_admin_subscriber_str(_admin_subscribe_str, slid);
}

void SteamLinkESP::init(bool encrypted, uint8_t* key) {
  INFO("Initializing SteamLinkESP");
  wifi_connect();

  _mqtt = new Adafruit_MQTT_Client(&_client, SL_SERVER, SL_SERVERPORT, SL_CONID,  SL_USERNAME, SL_KEY);
  _direct_publish = new Adafruit_MQTT_Publish(_mqtt, _direct_publish_str);
  _direct_subscribe = new Adafruit_MQTT_Subscribe(_mqtt, _direct_subscribe_str);
  _transport_publish = new Adafruit_MQTT_Publish(_mqtt, _transport_publish_str);
  _transport_subscribe = new Adafruit_MQTT_Subscribe(_mqtt, _transport_subscribe_str);
  _admin_publish = new Adafruit_MQTT_Publish(_mqtt, _admin_publish_str);
  _admin_subscribe = new Adafruit_MQTT_Subscribe(_mqtt, _admin_subscribe_str);

  // set up callbacks
  _direct_subscribe->setCallback(_direct_sub_callback);
  _transport_subscribe->setCallback(_transport_sub_callback);
  _admin_subscribe->setCallback(_admin_sub_callback);
}

void SteamLinkESP::update() {
  wifi_connect();
  if (mqtt_connect()) {
  // process packets
  _mqtt->processPackets(10);
  }
}

bool SteamLinkESP::send(uint8_t* buf) {
  INFO("Sending user string over mqtt");
  // buf must be a string
  uint8_t len = strlen((char*) buf);
  return _direct_publish->publish(buf, len + 1); // len + 1 for trailing null
  return true;
}

bool SteamLinkESP::bridge_send(uint8_t* buf, uint8_t len, uint32_t slid, uint8_t flags, uint8_t rssi) {
  INFO("Bridge is forwarding a packet over mqtt");
  uint8_t* packet;
  uint8_t packet_length;
  packet_length =  SteamLinkPacket::set_bridge_packet(packet, buf, len, slid, flags, rssi);
  _transport_publish->publish(packet, packet_length);
  free(packet);
  return true;
}

bool SteamLinkESP::admin_send(uint8_t* buf, uint8_t len, uint32_t slid, uint8_t flags, uint8_t rssi) {
  INFO("Sending an admin packet over mqtt");
  uint8_t* packet;
  uint8_t packet_length;
  packet_length =  SteamLinkPacket::set_bridge_packet(packet, buf, len, slid, flags, rssi);
  _admin_publish->publish(packet, packet_length);
  free(packet);
  return true;
}

void SteamLinkESP::wifi_connect() {
  // try connecting to one of the WiFi networks
  INFO("Connecting to WiFi");
  struct Credentials* endPtr = creds + sizeof(creds)/sizeof(creds[0]);
  while (WiFi.status() != WL_CONNECTED) {
  struct Credentials* ptr = creds;
  int cnt0 = 5;
  while ((ptr<endPtr) && (WiFi.status() != WL_CONNECTED)) {
    INFO("WiFi Network");
    INFO(ptr->ssid);
    WiFi.begin(ptr->ssid, ptr->pass);
    int cnt = WIFI_WAITSECONDS;
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      INFO("*");
      if (cnt-- == 0) {
        break;
      }
    }
    if (WiFi.status() == WL_CONNECTED)
      break;
    if (cnt0-- == 0) {
      FATAL("Could not connect to WiFi");
      while (1);
    }
    INFO("Trying next network!");
    ptr++;
  }
  }
  INFO("WiFi Connected, IP address: ");
  INFO(WiFi.localIP());
}

void SteamLinkESP::create_direct_publish_str(char* topic, uint32_t slid) {
  snprintf(topic, SL_ESP_DEFAULT_TOPIC_LEN, "SteamLink/%u/data", slid);
}

void SteamLinkESP::create_direct_subscriber_str(char* topic, uint32_t slid) {
  snprintf(topic, SL_ESP_DEFAULT_TOPIC_LEN, "SteamLink/%u/control", slid);
}

void SteamLinkESP::create_transport_publish_str(char* topic, uint32_t slid) {
  snprintf(topic, SL_ESP_DEFAULT_TOPIC_LEN, "SL/%u/data", slid);
}

void SteamLinkESP::create_transport_subscriber_str(char* topic, uint32_t slid) {
  snprintf(topic, SL_ESP_DEFAULT_TOPIC_LEN, "SL/%u/control", slid);
}

void SteamLinkESP::create_admin_publish_str(char* topic, uint32_t slid) {
  snprintf(topic, SL_ESP_DEFAULT_TOPIC_LEN, "SL/%u/admin_data", slid);
}

void SteamLinkESP::create_admin_subscriber_str(char* topic, uint32_t slid) {
  snprintf(topic, SL_ESP_DEFAULT_TOPIC_LEN, "SL/%u/admin_control", slid);
}

bool SteamLinkESP::mqtt_connect() {
  int8_t ret;
  if (_mqtt->connected()) {
    return true;
  }
  INFO("MQTT CONNECTING TO:");
  INFO("Server: ");
  INFO(SL_SERVER);
  INFO("Port: ");
  INFO(SL_SERVERPORT);
  INFO("At millis: ");
  INFO(millis());

  if ((ret = _mqtt->connect()) != 0) { // connect will return 0 for connected
    WARN(_mqtt->connectErrorString(ret));
    _mqtt->disconnect();
    return false;
  } else {
    INFO("Connected to MQTT");
  }
  return true;
}

void SteamLinkESP::_direct_sub_callback(char* data, uint16_t len) {
  INFO("MQTT Direct packet received from store");
  _on_local_receive((uint8_t*) data, (uint8_t) len);
}

// this will be a transport packet from the store
void SteamLinkESP::_transport_sub_callback(char* data, uint16_t len) {
  INFO("MQTT Transport packet received from store");
  uint32_t slid = slid;
  uint8_t flags;
  uint8_t rssi;
  uint8_t* payload;
  uint8_t payload_length;
  payload_length = SteamLinkPacket::get_bridge_packet((uint8_t*) data, (uint8_t) len, payload, slid, flags, rssi);
  _on_local_bridge_receive(payload, payload_length, slid, flags, rssi);
}

void SteamLinkESP::_admin_sub_callback(char* data, uint16_t len) {
  INFO("MQTT Admin packet received from store");
  uint32_t slid = slid;
  uint8_t flags;
  uint8_t rssi;
  uint8_t* payload;
  uint8_t payload_length;
  payload_length = SteamLinkPacket::get_bridge_packet((uint8_t*) data, (uint8_t) len, payload, slid, flags, rssi);
  _on_local_admin_receive(payload, payload_length, slid, flags, rssi);
}

void SteamLinkESP::register_receive_handler(on_receive_handler_function on_receive) {
  _on_local_receive = on_receive;
}

void SteamLinkESP::register_bridge_handler(on_receive_bridge_handler_function on_receive) {
  _on_local_bridge_receive = on_receive;
}

void SteamLinkESP::register_admin_handler(on_receive_bridge_handler_function on_receive) {
  _on_local_admin_receive = on_receive;
}

on_receive_handler_function SteamLinkESP::_on_local_receive = NULL;
on_receive_bridge_handler_function SteamLinkESP::_on_local_bridge_receive = NULL;
on_receive_bridge_handler_function SteamLinkESP::_on_local_admin_receive = NULL;

#endif
