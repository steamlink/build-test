/*
Driver for LoRa subnodes

*/
#ifndef STEAMLINK_SUBNODE_DRIVER
#define STEAMLINK_SUBNODE_DRIVER

#include <RHDatagram.h>
#include <RH_RF95.h>
#include "SteamLink_types.h"

class SteamLinkSubnodeDriver {

 public:

  /// send
  /// \returns false when it succeeds, and true when it fails
  bool send(uint8_t* buf, uint8_t len, uint8_t to_addr);

  /// receive
  /// \returns true when it receives
  bool receive(uint8_t* buf, uint8_t &len, uint8_t &from, uint8_t &to);

  /// set_pins
  void set_pins(uint8_t cs, uint8_t reset, uint8_t interrupt);

  /// set_node_address
  void set_node_addr(uint8_t node_addr);

  /// get_last_rssi
  uint8_t get_last_rssi();

  /// set_mod_conf
  void set_mod_conf(uint8_t modconf);

  /// init
  void init();

  /// set_frequency
  void set_frequency(float f);

  /// set_tx_power
  void set_tx_power(uint8_t tx_power);

 private:

  RH_RF95* driver;
  RHDatagram* manager;
  sl_pins pins;
  uint8_t mod_conf;
  uint8_t node_addr;
  uint8_t last_rssi;
  float frequency;
  uint8_t tx_power = 20;

  RH_RF95::ModemConfig modem_config[1] = {
    { // conf number 4, see https://lowpowerlab.com/forum/general-topics/issue-with-lora-long-range-modes-and-rfm95rfm96/
      .reg_1d = 0x78, // Reg 0x1D: BW=125kHz, Coding=4/8, Header=explicit
      .reg_1e = 0xc4, // Reg 0x1E: Spread=4096chips/symbol, CRC=enable
      .reg_26 = 0x0c  // Reg 0x26: LowDataRate=On, Agc=On
    }
  };
}

#endif
