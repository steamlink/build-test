#include <SteamLinkLoRa.h>
//#include <SteamLinkPacket.h>
//#include <SteamLink.h>

SteamLinkLora::SteamLinkLora(uint32_t slid) : SteamLinkGeneric(slid) {
  // initialize slid and set _node_addr
  _slid = slid;
  _node_addr = get_node_from_slid(slid);
  if (_node_addr == get_node_from_slid(SL_DEFAULT_STORE_ADDR)) {
    FATAL("slid cannot be SL_DEFAULT_STORE_ADDR addr! ");
    while (1);
  }
}


void SteamLinkLora::init(void *vconf, uint8_t config_length) {

  struct SteamLinkLoraConfig *_conf = (struct SteamLinkLoraConfig *) vconf;
  _encrypted = _conf->encrypted;
  _key = _conf->key;
  _mod_conf = _conf->mod_conf;

  if (!_driver) {
    /*
    // TODO: Not used, full initialization hangs on 3rd
    // Memory leak?
    pinMode(_reset_pin, OUTPUT);
    digitalWrite(_reset_pin, LOW);
    delay(200); // TODO
    digitalWrite(_reset_pin, HIGH);
    */

    if (config_length != sizeof(SteamLinkLoraConfig)) {
      FATAL("Received bad config struct, len should be: ");
      FATAL(sizeof(SteamLinkLoraConfig));
      FATAL(" is: ");
      FATALNL(config_length);
      while(1);
    }

    _driver = new RH_RF95(_cs_pin, _interrupt_pin);
    _manager = new RHDatagram(*_driver, _node_addr);

    if (!_manager->init()) {
      FATAL("RH manager init failed");
      while (1);
    }
    INFO("RH Initialized\n");
  }

  INFONL("Setting Radio Parameters...");

  // Set frequency
  if (!_driver->setFrequency(SL_LORA_DEFAULT_FREQUENCY)) {
    FATAL("setFrequency failed");
    while (1);
  }
  INFO("Frequency set done\n");
  if (!update_modem_config()){
    FATAL("modemConfig failed");
    while (1);
  }
  INFO("Modem config done\n");
  randomSeed(analogRead(A0));
  //_driver->setCADTimeout(10000);
  INFO("set CAD timeout\n");
  _driver->setTxPower(SL_LORA_DEFAULT_TXPWR, false);
  INFO("set lora tx power\n");
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
    FATAL("setModemConfig failed with invalid config choice");
    while (1);
  }
  return rc;
}

bool SteamLinkLora::driver_receive(uint8_t* &packet, uint8_t &packet_size, uint32_t &slid, bool &is_test) {
  uint8_t rcvlen = sizeof(driverbuffer);
  uint8_t from;
  uint8_t to;
  bool received = _manager->recvfrom(driverbuffer, &rcvlen, &from, &to);
  if (received) {
    INFO("SteamLinkLora::driver_receive len: ");
    INFO(rcvlen);
    INFO(" to: ");
    INFO(to);
    INFO(" packet: ");
    INFOPHEX(driverbuffer, rcvlen);
    _last_rssi = _driver->lastRssi();
    is_test = (_driver->headerFlags() & SL_LORA_TEST_FLAGS);
	packet = (uint8_t *) malloc(rcvlen);
    memcpy(packet,driverbuffer, rcvlen);
    packet_size = rcvlen;
    if (to == get_node_from_slid(SL_DEFAULT_STORE_ADDR)) {
        slid = SL_DEFAULT_STORE_ADDR;
    } else {
        slid = (uint32_t) to | (get_mesh_from_slid(_slid) << 8);
    }
    return true;
  } else {
    return false;
  }
}

bool SteamLinkLora::driver_send(uint8_t* packet, uint8_t packet_size, uint32_t slid, bool is_test) {
  uint8_t to_addr = get_node_from_slid(slid);
  bool sent;
  INFO("SteamLinkLora::driver_send len: ");
  INFO(packet_size);
  INFO(" to: ");
  INFO(slid);
  INFO(" test: ");
  INFO((uint8_t) is_test);
  INFONL(" packet: ");
  INFOPHEX(packet, packet_size);

  if (is_test) {
    _manager->setHeaderFlags(SL_LORA_TEST_FLAGS, 0);
    sent = _manager->sendto(packet, packet_size, to_addr);
    _manager->setHeaderFlags(0, SL_LORA_TEST_FLAGS);
  } else {
    sent = _manager->sendto(packet, packet_size, to_addr);
  }
  if (sent)  {
    if (! _driver->waitPacketSent(5000)) {	// wait max 5 sec for xmit to finish
      ERRNL("SteamLinkLora::driver_send waitPacketSent failed!");
      return false;
    }
    return true;
  }  else {
    ERRNL("SteamLinkLora::driver_send failed!");
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
