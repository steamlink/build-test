#include <SteamLinkESP.h>
#ifdef ESP8266

SteamLinkESP::SteamLinkESP(uint32_t slid) : SteamLinkGeneric(slid) {
  _slid = slid;
  create_pub_str(_pub_str, slid);
  create_sub_str(_sub_str, slid);
}

void SteamLinkESP::init(void *vconf) {
  INFO("Initializing SteamLinkESP\n");
  _conf = (struct SteamLinkESPConfig *) vconf;
  _creds = _conf->creds;

  _mqtt = new Adafruit_MQTT_Client(&_client, _conf->sl_server, _conf->sl_serverport, _conf->sl_conid,  _conf->sl_username, _conf->sl_key);
  _pub = new Adafruit_MQTT_Publish(_mqtt, _pub_str);
  _sub = new Adafruit_MQTT_Subscribe(_mqtt, _sub_str);

  _mqtt->subscribe(_sub);

  // set up callbacks
  _sub->setCallback(_sub_callback);

  // Connect Network
  wifi_connect();

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
   _last_rssi = WiFi.RSSI();
   packet = driverbuffer;
   packet_size = rcvlen;
   // TODO: see comment above re: MQTT nodes can only speak to store for now
   slid = SL_DEFAULT_STORE_ADDR;
   // TODO: mqtt nodes cannot send test packets for now
   is_test = false;
   // flip available
   available = false;
   return true;
  } else {
    return false;
  }
}

bool SteamLinkESP::driver_send(uint8_t* packet, uint8_t packet_size, uint32_t slid, bool is_test) {
  INFO("Sending user string over mqtt\n");
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
//  struct Credentials* endPtr = _creds + sizeof(_creds)/sizeof(_creds[0]);
  while (WiFi.status() != WL_CONNECTED) {
    INFO("Connecting to WiFi\n");
    struct SteamLinkWiFi *ptr = _creds;
    int cnt0 = 5;
    while ((ptr->ssid) && (WiFi.status() != WL_CONNECTED)) {
      INFO("WiFi Network: ");
      INFONL(ptr->ssid);
      WiFi.begin(ptr->ssid, ptr->pass);
      int cnt = WIFI_WAITSECONDS;
      while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        INFO(".");
        if (cnt-- == 0) {
          break;
        }
      }
      INFO("\n");
      if (WiFi.status() == WL_CONNECTED)
        break;
      if (cnt0-- == 0) {
        FATAL("Could not connect to WiFi");
        while (1);
      }
      INFO("Trying next network!\n");
      ptr++;
    }
  }
  INFO("WiFi Connected, IP address: ");
  INFO(WiFi.localIP());
  INFO(" rssi: ");
  INFONL(WiFi.RSSI());
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
  INFO("MQTT connecting to:\n");
  INFO("Server: ");
  INFO(_conf->sl_server);
  INFO(":");
  INFONL(_conf->sl_serverport);
  INFO("At millis: ");
  INFONL(millis());

  if ((ret = _mqtt->connect()) != 0) { // connect will return 0 for connected
    WARN(_mqtt->connectErrorString(ret));
    _mqtt->disconnect();
    return false;
  } else {
    INFO("Connected to MQTT\n");
  }
  return true;
}

void SteamLinkESP::_sub_callback(char* data, uint16_t len) {
  INFO("_sub_callback len: ");
  INFONL(len);
  memcpy(driverbuffer, data, len);
  rcvlen = len;
  available = true;
}

uint8_t SteamLinkESP::driverbuffer[] = {0};
uint8_t SteamLinkESP::rcvlen = 0;
bool SteamLinkESP::available = false;

#endif
