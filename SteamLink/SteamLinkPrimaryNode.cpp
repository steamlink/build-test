void SteamLinkPrimaryNode::init() {
  wifi_connect();
  verifyFingerprint();

  mqtt = new Adafruit_MQTT_Client(client, SL_SERVER, SL_SERVERPORT, SL_CONID, SL_USERNAME, SL_KEY);
  publish = new Adafruit_MQTT_publish(mqtt, "SL/" SL_MESHID "/data");
  subscribe = new Adafruit_MQTT_subscribe(mqtt, "SL/" SL_MESHID "/control");

  // set callbacks and subscribe to the config topics
  publish->setCallback(&message_sent);
  subscribe->setCallback(&message_received);

  // subscribe
  mqtt->subscribe(&mqtt_subs_control);

  // set lwt
  mqtt->will("SL/" SL_MESHID "/status", "OFFLINE", 0, 1);
}

bool SteamLinkPrimaryNode::send(uint8_t* buf) {
  // buf must be a string
  uint8_t* ntype0;
  uint8_t* btype0;

  // TODO: SL_ENCRYPT_KEY comes from creds.h file
  uint8_t ntype0_length = SteamLinkPacket::set_ntype0_packet(ntype0, buf, strlen(buf), my_slid, true, SL_ENCRYPT_KEY);
  // encapsulate in btype packet

  // TODO: node addr and rssi should be 0 when coming from no physical layer?
  uint8_t btype0_length = SteamLinkPacket::set_btype0_packet(btype0, ntype0, ntype0_length, 0, 0);

  return publish->publish(btype0, btype0_length);

  // TODO: free memory!
}

void SteamLinkPrimaryNode::wifi_connect() {
  struct Credentials* endPtr = creds + sizeof(creds)/sizeof(creds[0]);
  while (WiFi.status() != WL_CONNECTED) {
    struct Credentials* ptr = creds;
    cnt0 = WIFI_RETRY_COUNT;
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

void SteamLinkPrimaryNode::verifyFingerprint() {
  const char* host = SL_SERVER;
  Serial.print(F("verifying SSL cert for "));
  Serial.println(host);
  if (! client.connect(host, SL_SERVERPORT)) {
    Serial.println(F("Connection failed. Halting execution."));
    while(1);
  }
  if (client.verify(sl_server_fingerprint, host)) {
    Serial.println(F("Connection secure."));
  } else {
    Serial.println(F("Connection insecure! Halting execution."));
    while(1);
  }
}
