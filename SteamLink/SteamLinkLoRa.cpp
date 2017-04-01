bool SteamLinkLoRa::send(uint8_t* buf) {
  // TODO: short circuit if I'm a bridge!
  uint8_t* packet;
  uint8_t packet_size;
  uint8_t len = strlen(buf);
  uint8_t to_addr = SL_DEFAULT_BRIDGE_ADDR;

  // early exit if the message is to big!
  // TODO: change with actual determined error codes
  if (len >= SL_MAX_MESSAGE_LEN) return false;

  packet_size = SteamLinkPacket::set_ntype0_packet(packet, buf, _slid, _encrypted, _key);
  sent = manager->sendto(packet, packet_size, to_addr);
  free(packet);

  // TODO: figure out error codes
  if (sent == 0)  {
    return true;
  }  else {
    return false;
  }
}

bool SteamLinkLora::init(bool encrypted, uint8_t* key) {
  if (encrypted) {
    _encrypted = true;
    _key = key;
  }
  driver = new RH_RF95(_cs_pin, _interrupt_pin);
  manager = new RHDatagram(*driver, conf.node_address);

  // initialize manager
  if (!manager->init()) {
    Serial.println("SL_FATAL: manager initialization failed");
    while (1);
  }

  Serial.println("manager init done!");
  // Set frequency
  if (!driver->setFrequency(SL_DEFAULT_FREQUENCY)) {
    Serial.println("SL_FATAL: setFrequency failed");
    while (1);
  }

  // Set modem configuration
  bool rc;
  if (conf.mod_conf > 3) {
    driver->setModemRegisters(&modem_config[conf.mod_conf-4]);
    rc = true;
  }
  else
    rc = driver->setModemConfig((RH_RF95::ModemConfigChoice) conf.mod_conf);
  if (!rc) {
    Serial.println("SL_FATAL: setModemConfig failed with invalid configuration");
    while (1);
  }

  Serial.println("Modem config done!");
  randomSeed(analogRead(A0));
  driver->setCADTimeout(10000);
  driver->setTxPower(SL_DEFAULT_TX_POWER, false);
}

void SteamLinkLora::update() {
  // allocate max size
  uint8_t rcvlen = sizeof(driverbuffer);
  uint8_t payload_len;
  uint8_t from; // TODO: might need to "validate" sender!
  uint8_t to;
  bool received = manager->recvfrom(driverbuffer, &rcvlen, &from, &to);
  uint8_t *payload;
  uint32_t slid;

  payload_len = SteamLinkPacket::get_ntype0_packet(driverbuffer, rcvlen, payload, slid,  _encrypted, _key);

  // All messages must match my node_addr
  // TODO: does this mean we don't have a default bridge address?
  if (to == _node_addr) {
    // check if the message is for me
    uint8_t node_addr;
    node_addr = get_addrs_from_slid(slid);

    if (node_addr == _node_addr) {
      // message is for me!
      _on_receive(payload, payload_len);
    } else if (_is_bridge) {
      // send to bridge
      _on_receive_bridge(driverbuffer, rcvlen);
    } else {
      Serial.println("SL_INFO: dropping packet");
    }
  }
}

void SteamLinkLoRa::bridge_send(uint8_t* packet, uint8_t packet_size, uint32_t slid) {
  bool sent;
  uint8_t to_addr = get_addrs_from_slid(slid);

  sent = manager->sendto(packet, packet_size, to_addr);
  free(packet);

  // TODO: figure out error codes
  if (sent == 0)  {
    return true;
  }  else {
    return false;
  }
}

void SteamLinkLoRa::register_receive_handler(on_receive_handler_function on_receive) {
  _on_receive = on_receive;
}

void SteamLinkLoRa::register_bridge_handler(on_receive_handler_function on_receive) {
  _on_receive_bridge = on_receive;
}

uint8_t SteamLinkLora::get_addrs_from_slid(uint32_t slid) {
  //TODO: returns last 8 bits of the slid
  return 0;
}

void set_bridge() {
  _is_bridge = true;
}
