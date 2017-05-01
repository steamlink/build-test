#include <SteamLinkLora.h>
#include <SteamLinkPacket.h>

SteamLinkLora::SteamLinkLora(uint32_t slid) : SteamLinkGeneric(slid) {
  // initialize slid and set _node_addr
  _slid = slid;
  _node_addr = get_node_from_slid(slid);
  // lora driver cannot connect to store
  _is_primary = false;
}

bool SteamLinkLora::send(uint8_t* buf) {
  // TODO: short circuit if I'm a bridge!
  uint8_t* packet;
  uint8_t packet_size;
  uint8_t len = strlen((char*) buf);
  uint8_t to_addr = SL_LORA_DEFAULT_BRIDGE_ADDR;
  bool sent = 1;
  // early exit if the message is to big!
  // TODO: change with actual determined error codes
  if (len + 1 >= SL_LORA_MAX_MESSAGE_LEN) return false;

  if (_encrypted) {
    packet_size = SteamLinkPacket::set_encrypted_packet(packet, buf, len + 1,  _key);
    sent = _manager->sendto(packet, packet_size, to_addr);
    free(packet);
  } else {
    sent = _manager->sendto(buf, len + 1, to_addr);
  }

  // TODO: figure out error codes
  if (sent == 0)  {
    return true;
  }  else {
    return false;
  }
}

void SteamLinkLora::init(bool encrypted, uint8_t* key) {
  if (encrypted) {
    _encrypted = true;
    _key = key;
  }
  _driver = new RH_RF95(_cs_pin, _interrupt_pin);
  _manager = new RHDatagram(*_driver, _node_addr);

  // Set frequency
  if (!_driver->setFrequency(SL_LORA_DEFAULT_FREQUENCY)) {
    Serial.println("SL_FATAL: setFrequency failed");
    while (1);
  }
  if (!update_modem_config()){
    Serial.println("SL_FATAL: modemConfig failed");
    while (1);
  }
  Serial.println("Modem config done!");
  randomSeed(analogRead(A0));
  _driver->setCADTimeout(10000);
  _driver->setTxPower(SL_LORA_DEFAULT_TXPWR, false);
}

void SteamLinkLora::set_modem_config(uint8_t mod_conf) {
  _mod_conf = mod_conf;
}

bool SteamLinkLora::update_modem_config() {
  // Set modem configuration
  bool rc;
  if (_mod_conf > 3) {
    _driver->setModemRegisters(&modem_config[_mod_conf-4]);
    rc = true;
  }
  else
    rc = _driver->setModemConfig((RH_RF95::ModemConfigChoice) _mod_conf);
  if (!rc) {
    Serial.println("SL_FATAL: setModemConfig failed with invalid configuration");
    while (1);
  }
  return rc;
}

void SteamLinkLora::update() {
  // allocate max size
  uint8_t rcvlen = sizeof(driverbuffer);
  uint8_t payload_len;
  uint8_t from; // TODO: might need to "validate" sender!
  uint8_t to;
  bool received = _manager->recvfrom(driverbuffer, &rcvlen, &from, &to);
  uint8_t *payload;
  uint8_t flags = _driver->headerFlags();
  uint32_t slid;
  if (to == _node_addr) {
    // message is for me!
    payload_len = SteamLinkPacket::get_encrypted_packet(driverbuffer, rcvlen, payload, _key);
    _on_receive(payload, payload_len);
  } else if (to == SL_LORA_DEFAULT_BRIDGE_ADDR) {
    // message is for bridge and we should forward it!
    // TODO: or if testflag is set?
    uint8_t lastRssi = _driver->lastRssi();
    _on_bridge_receive(driverbuffer, rcvlen, _slid, flags, lastRssi);
  } else {
    Serial.println("SL_INFO: dropping packet");
  }
}

// bridge send is used to send from bridge to node
bool SteamLinkLora::bridge_send(uint8_t* packet, uint8_t packet_size, uint32_t slid, uint8_t flags, uint8_t rssi) {
  // ignore rssi
  bool sent;
  _manager->setHeaderFlags(flags);
  uint8_t to_addr = get_node_from_slid(slid);
  sent = _manager->sendto(packet, packet_size, to_addr);
  _manager->setHeaderFlags(SL_LORA_DEFAULT_FLAGS);
  free(packet);
  // TODO: figure out error codes
  if (sent == 0)  {
    return true;
  }  else {
    return false;
  }
}

// TODO: these are one-way functions
uint8_t SteamLinkLora::get_node_from_slid(uint32_t slid) {
  // use 0xFF mask to get the last 8 bits
  return (uint8_t) (slid & 0xFF);
}

// TODO: these are one-way functions
uint32_t SteamLinkLora::get_mesh_from_slid(uint32_t slid) {
  // drop the last 8 bits of slid
  return (uint32_t) (slid >> 8);
}

void SteamLinkLora::set_pins(uint8_t cs=8, uint8_t reset=4, uint8_t interrupt=3) {
  _cs_pin = cs;
  _reset_pin = reset;
  _interrupt_pin = interrupt;
}
