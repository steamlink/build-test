#include "SteamLink.h"

bool SteamLink::init(uint8_t* token) {
  // Class to manage message delivery and receipt, using the driver declared above

  extract_token_fields(token, SL_TOKEN_LENGTH);
  
  driver = new RH_RF95(pins.cs, pins.interrupt);
  manager = new RHMesh(*driver, conf.node_address);

  // initialize manager
  if (!manager->init()) {
    Serial.println("SL_FATAL: manager initialization failed");
    while (1);
  }

  // Set frequency
  if (!driver->setFrequency(conf.freq)) {
    Serial.println("SL_FATAL: setFrequency failed");
    while (1);
  }

  // Set modem configuration
  if (!driver->setModemConfig(conf.mod_conf)) {
    Serial.println("SL_FATAL: setModemConfig failed with invalid configuration");
    while (1);
  }

  // set timeout for CAD to 10s
  driver->setCADTimeout(10000);
  // set antenna power
  driver->setTxPower(tx_power, false);
}

bool SteamLink::send(uint8_t* buf, uint8_t len) {
  return manager->sendtoWait(buf, len, bridge_address);
}

bool SteamLink::receive(uint8_t* buf, uint8_t* len, uint8_t timeout) {
  uint8_t from;
  return manager->recvfromAckTimeout(buf, len, timeout, &from);
}

bool SteamLink::receive(uint8_t* buf, uint8_t* len) {
  return manager->recvfromAck(buf, len);
}

bool SteamLink::available() {
  return driver->available();
}

void SteamLink::extract_token_fields(uint8_t* str, uint8_t size){
  uint8_t buf[SL_TOKEN_LENGTH]; 

  // Make sure we're validated
  if(!validate_token(str, size)){
    Serial.println("SL_FATAL: Token invalid!");
    while(1);
  }

  // scan in the values
  shex(buf, str, SL_TOKEN_LENGTH);

  // copy values in to struct
   memcpy(&conf, buf, SL_TOKEN_LENGTH);
}

// Takes in buf to write to, str with hex to convert from, and length of buf (i.e. num of hex bytes)
void SteamLink::shex(uint8_t* buf, uint8_t* str, unsigned int size) {
  unsigned char i;
  for (i = 0; i < size; i++)
    sscanf((char*)(str+i*2),"%2hhx", &buf[i]);
}

bool SteamLink::validate_token(uint8_t* str, uint8_t size){
  // TODO: actually validate the token!
  return true;
}
void SteamLink::set_pins(uint8_t cs=8, uint8_t reset=4, uint8_t interrupt=3) {
  pins.cs = cs;
  pins.reset = reset;
  pins.interrupt = interrupt;
}
