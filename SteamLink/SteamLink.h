// NOTE: Only works with RadioHead v1.6.2 and later

#ifndef STEAMLINK_H
#define STEAMLINK_H

#define DEBUG

#include <RHMesh.h>
#include <RH_RF95.h>
#include "SteamLink_types.h"
#include <SPI.h>
#include "aes.h"

#define SL_TOKEN_LENGTH 23
#define SL_DEFAULT_BRIDGE 1
#define SL_DEFAULT_TXPWR 23
#define SL_MAX_MESSAGE_LEN 64

class SteamLink {

  // TODO: Document class functions!
  public:
  // Automatic initialization
  bool init(uint8_t* token);

  // send message
  bool send(uint8_t* buf);

  // receive
  bool receive(uint8_t* buf, uint8_t len, uint8_t timeout);
  bool receive(uint8_t* buf, uint8_t len);

  // available
  bool available();
  
  //set pins
  void set_pins(uint8_t cs, uint8_t reset, uint8_t interrupt);
  
 private:
  uint8_t bridge_address = SL_DEFAULT_BRIDGE;
  uint8_t tx_power = SL_DEFAULT_TXPWR;
  sl_config conf;
  sl_pins pins;
  RH_RF95* driver;
  RHMesh* manager;
  void debug(char* string);
  uint8_t* encrypt_alloc(uint8_t* outlen, uint8_t* in, uint8_t inlen, uint8_t* key);
  void decrypt(uint8_t* in, uint8_t inlen, uint8_t* key);
  void phex(uint8_t* str, unsigned int size);

  // extract fields from token, take in string pointer and length and writes to sl_config conf
  void extract_token_fields(uint8_t* str, uint8_t size);

  // validate token (functional valididation), returns true if it's valid
  bool validate_token(uint8_t* str, uint8_t size);

  // take in a buffer and 
  void shex(uint8_t* buf, uint8_t* str, unsigned int size);

  // TODO: Create packet

  // TODO: Extract packet

  // TODO: Dynamically adjust tx_power

};

#endif
