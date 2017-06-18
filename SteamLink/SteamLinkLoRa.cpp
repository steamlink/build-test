#include <SteamLinkLoRa.h>
//#include <SteamLinkPacket.h>
//#include <SteamLink.h>

#define LORA_SEND_BLUE_LED 2
#define LORA_RECEIVE_RED_LED 0

SteamLinkLora::SteamLinkLora(uint32_t slid) : SteamLinkGeneric(slid) {
  // initialize slid and set _node_addr
  _slid = slid;
  _node_addr = get_node_from_slid(slid);
  if (_node_addr == get_node_from_slid(SL_DEFAULT_STORE_ADDR)) {
    FATAL("slid cannot be SL_DEFAULT_STORE_ADDR addr! ");
    while (1);
  }
}


bool SteamLinkLora::driver_can_send() {
  return _driver->waitPacketSent(0); // wait max 0 us
}

void SteamLinkLora::init(void *vconf, uint8_t config_length) {

  struct SteamLinkLoraConfig *_conf = (struct SteamLinkLoraConfig *) vconf;
  _encrypted = _conf->encrypted;
  _key = _conf->key;
  _mod_conf = _conf->mod_conf;

  if (config_length != sizeof(SteamLinkLoraConfig)) {
    FATAL("Received bad config struct, len should be: ");
    FATAL(sizeof(SteamLinkLoraConfig));
    FATAL(" is: ");
    FATALNL(config_length);
    while(1);
  }
  
  if (!_driver) {
    INFONL("Resetting driver by toggling reset pin");
    pinMode(_reset_pin, OUTPUT);
    digitalWrite(_reset_pin, HIGH);
    delay(100);
    digitalWrite(_reset_pin, LOW);
    delay(100);
    digitalWrite(_reset_pin, HIGH);
    delay(10);
    
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
  
  if(_driver->waitPacketSent(0)){
    INFONL("LoRa driver ready to send!");
  } else {
    WARNNL("LoRa driver initialize but not ready to send");
    WARNNL("Is the RadioHead library patched?");
  }
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

bool SteamLinkLora::driver_receive(uint8_t* &packet, uint8_t &packet_size, uint32_t &slid) {
  
#ifdef LORA_SEND_BLUE_LED
  if(driver_can_send()) {
    pinMode(LORA_SEND_BLUE_LED, OUTPUT);
    digitalWrite(LORA_SEND_BLUE_LED, HIGH);
  }
#endif //LORA_SEND_BLUE_LED

#ifdef LORA_RECEIVE_RED_LED
  pinMode(LORA_RECEIVE_RED_LED, OUTPUT);
  digitalWrite(LORA_RECEIVE_RED_LED, HIGH);
#endif //LORA_RECEIVE_RED_LED

  uint8_t rcvlen = sizeof(driverbuffer);
  uint8_t from;
  uint8_t to;
  bool received = _manager->recvfrom(driverbuffer, &rcvlen, &from, &to);
  if (received) {

#ifdef LORA_RECEIVE_RED_LED
    pinMode(LORA_RECEIVE_RED_LED, OUTPUT);
    digitalWrite(LORA_RECEIVE_RED_LED, LOW);
#endif //LORA_RECEIVE_RED_LED

    INFO("SteamLinkLora::driver_receive len: ");
    INFO(rcvlen);
    INFO(" to: ");
    INFO(to);
    INFO(" packet: ");
    INFOPHEX(driverbuffer, rcvlen);
    _last_rssi = _driver->lastRssi();
    packet = (uint8_t *) malloc(rcvlen);
    INFO("SSteamLinkLora::driver_receive: malloc "); Serial.println((unsigned int) packet, HEX);
    memcpy(packet,driverbuffer, rcvlen);
    packet_size = rcvlen;
    if (to == get_node_from_slid(SL_DEFAULT_STORE_ADDR)) {
        slid = SL_DEFAULT_STORE_ADDR;
    } else if (to == get_node_from_slid(SL_DEFAULT_TEST_ADDR)) {
      slid = SL_DEFAULT_TEST_ADDR;
    } else {
      slid = (uint32_t) to | (get_mesh_from_slid(_slid) << 8);
    }
    return true;
  } else {
    return false;
  }
}

bool SteamLinkLora::driver_send(uint8_t* packet, uint8_t packet_size, uint32_t slid) {

#ifdef LORA_SEND_BLUE_LED
  pinMode(LORA_SEND_BLUE_LED, OUTPUT);
  digitalWrite(LORA_SEND_BLUE_LED, LOW);
#endif //LORA_SEND_BLUE_LED

  uint8_t to_addr = get_node_from_slid(slid);
  bool sent;
  INFO("SteamLinkLora::driver_send len: ");
  INFO(packet_size);
  INFO(" to: ");
  INFO(slid);
  INFONL(" packet: ");
  INFOPHEX(packet, packet_size);
  return (_manager->sendto(packet, packet_size, to_addr));
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
