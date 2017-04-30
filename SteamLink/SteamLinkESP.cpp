/*
TODO:
clean up connect to wifi
status sub callback

direct - send / rcv for my node direct to store
pub: SteamLink/slid/data
sub: SteamLink/slid/control

admin - send / rcv for bridge control to store
pub: SL/slid/admin_data
sub: SL/slid/admin_control

// transport
bridge - send / rcv for another node via bridging function
pub: SL/slid/data
sub: SL/slid/control

*/

#ifdef ESP8266
#include <SteamLinkESP.h>

SteamLinkESP::SteamLinkESP(uint32_t slid) : SteamLinkGeneric(slid) {
  _slid = slid;
  create_direct_publish_str(_direct_publish_str, slid);
  create_direct_subscribe_str(_direct_subscribe_str, slid);

  create_transport_publish_str(_transport_publish_str, slid);
  create_transport_subscribe_str(_transport_subscribe_str, slid);

  create_admin_publish_str(_admin_publish_str, slid);
  create_admin_subscribe_str(_admin_subscribe_str, slid);

  // ESP's can talk to the store directly!
  _is_primary = true;
}

void SteamLinkESP::init(bool encrypted, uint8_t* key) {
  wifi_connect();

  _mqtt = new Adafruit_MQTT_Client(&_client, SL_SERVER, SL_SERVERPORT, SL_CONID,  SL_USERNAME, SL_KEY);
  _direct_publish = new Adafruit_MQTT_Publish(&_mqtt, _direct_publish_str);
  _direct_subscribe = new Adafruit_MQTT_Subscribe(&_mqtt, _direct_subscribe_str);
  _transport_publish = new Adafruit_MQTT_Publish(&_mqtt, _transport_publish_str);
  _transport_subscribe = new Adafruit_MQTT_Publish(&_mqtt, _transport_subscribe_str);
  _admin_publish = new Adafruit_MQTT_Publish(&_mqtt, _admin_publish_str);
  _admin_subscribe = new Adafruit_MQTT_Publish(&_mqtt, _admin_subscribe_str);

  // set up callbacks
  _direct_subscribe.setCallback(&direct_sub_callback);
  _transport_subscribe.setCallback(&transport_sub_callback);
  _admin_subscribe.setCallback(); // TODO: send admin packets to bridge and bridge responds
}

void SteamLinkESP::update() {
  wifi_connect();
  if (mqtt_connect()) {
    // process packets
    _mqtt.processPackets(10);
  }
}

bool SteamLinkESP::send(uint8_t* buf) {
  // buf must be a string
  uint8_t len = strlen(buf);
  return _direct_publish.publish(buf, len + 1); // len + 1 for trailing null
  return true;
}

bool SteamLinkESP::bridge_send(uint8_t* buf, uint8_t len, uint32_t slid, uint8_t flags, uint8_t rssi) {
  uint8_t* packet;
  uint8_t packet_length;
  packet_length =  SteamLinkPacket::set_bridge_packet(packet, buf, len, slid, flags, rssi);
  _transport_publish.publish(packet, packet_length);
  free(packet);
  return true;
}

bool SteamLinkESP::admin_send(uint8_t* buf, uint8_t len, uint32_t slid, uint8_t flags, uint8_t rssi) {
  uint8_t* packet;
  uint8_t packet_length;
  packet_length =  SteamLinkPacket::set_bridge_packet(packet, buf, len, slid, flags, rssi);
  _admin_publish.publish(packet, packet_length);
  free(packet);
  return true;
}

void SteamLinkESP::wifi_connect() {
  // try connecting to one of the WiFi networks
  struct Credentials* endPtr = creds + sizeof(creds)/sizeof(creds[0]);
  while (WiFi.status() != WL_CONNECTED) {
    struct Credentials* ptr = creds;
    cnt0 = 5;
    while ((ptr<endPtr) && (WiFi.status() != WL_CONNECTED)) {
      Serial.print(F("WiFi Network "));
      Serial.println(ptr->ssid);
      WiFi.begin(ptr->ssid, ptr->pass);
      cnt = WIFI_WAITSECONDS;
      while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
        if (cnt-- == 0) {
          break;
        }
      }
      if (WiFi.status() == WL_CONNECTED)
        break;
      if (cnt0-- == 0) {
        Serial.println(F("\ndie, wait for reset"));
        while (1);
      }
      Serial.println(F("\ntry next nextwork"));
      ptr++;
    }
  }
  Serial.println("");

  Serial.print(F("WiFi connected, IP address: "));
  Serial.println(WiFi.localIP());
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

void SteamLinkESP::mqtt_connect() {
  int8_t ret;

  if (_mqtt.connected()) {
    return true;
  }

  Serial.print(F("MQTT connect " SL_SERVER ":"));
  Serial.print(SL_SERVERPORT);
  Serial.print(" at ");
  Serial.print(millis());
  Serial.print("millis: ");

  if ((ret = _mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.print(_mqtt.connectErrorString(ret));
    _mqtt.disconnect();
    return false;
  } else {
    Serial.println("OK");
    //UpdStatus("Online");
  }

  return true;
}

void SteamLinkESP::direct_sub_callback(char* data, uint16_t len) {
  _on_receive((uint8_t)* data, (uint8_t) len);
}

// this will be a transport packet from the store
void SteamLinkESP::transport_sub_callback(char* data, uint16_t len) {
  uint32_t slid = slid;
  uint8_t flags;
  uint8_t rssi;
  uint8_t* payload;
  uint8_t payload_length;
  payload_length = SteamLinkPacket::get_bridge_packet(data, (uint8_t) len, payload, slid, flags, rssi);
  _on_bridge_receive(payload, payload_length, slid, flags, rssi);
}

void SteamLinkESP::admin_sub_callback(char* data, uint16_t len) {
  if (_is_bridge) {
    uint32_t slid = slid;
    uint8_t flags;
    uint8_t rssi;
    uint8_t* payload;
    uint8_t payload_length;
    payload_length = SteamLinkPacket::get_bridge_packet(data, (uint8_t) len, payload, slid, flags, rssi);
    _on_admin_receive(payload, payload_length, slid, flags, rssi);
  }
}

#endif
