#include <SteamLinkESP.h>
#ifdef ESP8266

SteamLinkESP::SteamLinkESP(uint32_t slid) : SteamLinkGeneric(slid) {
  _slid = slid;
  create_pub_str(_pub_str, slid);
  create_sub_str(_sub_str, slid);
}

void SteamLinkESP::init(bool encrypted, uint8_t* key) {
  INFO("Initializing SteamLinkESP");
  wifi_connect();

  _mqtt = new Adafruit_MQTT_Client(&_client, SL_SERVER, SL_SERVERPORT, SL_CONID,  SL_USERNAME, SL_KEY);
  _pub = new Adafruit_MQTT_Publish(_mqtt, _pub_str);
  _sub = new Adafruit_MQTT_Subscribe(_mqtt, _sub_str);

  // set up callbacks
  _sub->setCallback(_sub_callback);

  // Connect to MQTT
  mqtt_connect();
}

bool SteamLinkESP::driver_receive(uint8_t* &packet, uint8_t &packet_size, uint32_t &slid, bool &is_test) {
  // first make sure we're still connected
  wifi_connect();
  if (mqtt_connect()) {
  // process packets
  _mqtt->processPackets(10);
  }
  if (available) {
   packet = driverbuffer;
   packet_size = rcvlen;
   // TODO: see comment above re: MQTT nodes can only speak to store for now
   slid = SL_DEFAULT_STORE_ADDR;
   // TODO: mqtt nodes cannot send test packets for now
   is_test = false;
   // flip available
   available = false;
  } else {
    return false;
  }
}

bool SteamLinkESP::driver_send(uint8_t* packet, uint8_t packet_size, uint32_t slid, bool is_test) {
  INFO("Sending user string over mqtt");
  // TODO: for now, MQTT nodes can only send to one SLID, ie store_slid
  // this can be exanded in the future. We ignore the input SLID

  // TODO also MQTT nodes cannot send MQTT test packages for now
  if (_mqtt->connected()){
    _pub->publish(packet, packet_size);
    return true;
  } else {
    ERR("MQTT not connected, Dropping user packet");
    return false;
  }
}

void SteamLinkESP::wifi_connect() {
  // early exit if we're connected
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  // try connecting to one of the WiFi networks
  struct Credentials* endPtr = creds + sizeof(creds)/sizeof(creds[0]);
  while (WiFi.status() != WL_CONNECTED) {
    INFO("Connecting to WiFi");
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

void SteamLinkESP::create_pub_str(char* topic, uint32_t slid) {
  snprintf(topic, SL_ESP_DEFAULT_TOPIC_LEN, "SteamLink/%u/data", slid);
}

void SteamLinkESP::create_sub_str(char* topic, uint32_t slid) {
  snprintf(topic, SL_ESP_DEFAULT_TOPIC_LEN, "SteamLink/%u/control", slid);
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

void SteamLinkESP::_sub_callback(char* data, uint16_t len) {
  memcpy(driverbuffer, data, len);
  rcvlen = len;
  available = true;
}

uint8_t SteamLinkESP::driverbuffer[] = {0};
uint8_t SteamLinkESP::rcvlen = 0;
bool SteamLinkESP::available = false;

#endif
