// NOTE: Only works with RadioHead v1.6.2 and later

#ifndef STEAMLINK_H
#define STEAMLINK_H

#include <RHMesh.h>
#include <RH_RF95.h>
#include "SteamLink_types.h"
#include <SPI.h>

class SteamLink {

  public:
  // Automatic initialization
  // TODO: This needs to take in a token to modify configurations
  bool init();

  // send message
  bool send(uint8_t* buf, uint8_t len);

  // receive message
  bool receive(uint8_t* buf, uint8_t* len, uint8_t timeout);

  //set pins
  void set_pins(uint8_t cs, uint8_t reset, uint8_t interrupt);

 private:
  // TODO: Bridge and client address should be derived from sl_token
  uint8_t bridge_address = 4;
  uint8_t sl_client_address = 9;

  sl_config conf;

  sl_pins pins;

  RH_RF95* driver;

  RHMesh* manager;

  // TODO: extract fields from token
  /* Extracted fields:
     node address - yes
     SL_IID - maybe
   */

  // TODO: validate token (functional valididation)

  // TODO: Create packet

  // TODO: Extract packet

  // TODO: Dynamically adjust tx_power

};

#endif
