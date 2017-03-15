#include "SteamLinkSubnodeDriver.h"

bool SteamLinkSubnodeDriver::send(uint8_t* buf, uint8_t len, uint8_t to_addr) {

}

bool SteamLinkSubnodeDriver::receive(uint8_t* buf, uint8_t &len, uint8_t &from, uint8_t &to);

/// set_pins
void SteamLinkSubnodeDriver::set_pins(uint8_t cs, uint8_t reset, uint8_t interrupt);

/// set_node_address
void SteamLinkSubnodeDriver::set_node_addr(uint8_t node_addr);

/// get_last_rssi
uint8_t SteamLinkSubnodeDriver::get_last_rssi() {
  return last_rssi;
}

/// set_mod_conf
void SteamLinkSubnodeDriver::set_mod_conf(uint8_t modconf) {
  mod_conf = modconf;
}

/// init
void SteamLinkSubnodeDriver::init() {
  driver = new RH_RF95(pins.cs, pins.interrupt);
  manager = new RHDatagram(*driver, node_addr);
  if (!manager->init()) {
    while (1);
  }
  if (!driver->setFrequency(frequency)) {
    while (1);
  }
  // Set modem configuration
  bool rc;
  if (mod_conf > 3) {
    driver->setModemRegisters(&modem_config[mod_conf-4]);
    rc = true;
  }
  else
    rc = driver->setModemConfig((RH_RF95::ModemConfigChoice) conf.mod_conf);
  if (!rc) {
    while (1);
  }
  driver->setCADTimeout(10000);
  // set antenna power
  driver->setTxPower(tx_power, false);
}

