#include "SteamLink.h"

bool SteamLink::init() {
  // Class to manage message delivery and receipt, using the driver declared above
  driver = new RH_RF95(pins.cs, pins.interrupt);
  manager = new RHMesh(*driver, sl_client_address);

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
  driver->setTxPower(conf.tx_power, false);
}

bool SteamLink::send(uint8_t* buf, uint8_t len) {
  return manager->sendtoWait(buf, len, bridge_address);
}

bool SteamLink::receive(uint8_t* buf, uint8_t* len, uint8_t timeout) {
  uint8_t from;
  return manager->recvfromAckTimeout(buf, len, timeout, &from);
}

void SteamLink::set_pins(uint8_t cs=8, uint8_t reset=4, uint8_t interrupt=3) {
  pins.cs = cs;
  pins.reset = reset;
  pins.interrupt = interrupt;
}
