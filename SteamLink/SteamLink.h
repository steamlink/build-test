// NOTE: Only works with RadioHead v1.6.2 and later

#ifndef STEAMLINK_H
#define STEAMLINK_H

#define DEBUG

// #include <RHMesh.h>
#include <RHDatagram.h>
#include <RH_RF95.h>
#include "SteamLink_types.h"
#include <SPI.h>
#include "aes.h"

#define SL_DEFAULT_BRIDGE 1
#define SL_DEFAULT_TXPWR 23
#define SL_MAX_MESSAGE_LEN 64

class SteamLink {

  // TODO: Document class functions!
  public:

  // create a typedef for on receive functions
  typedef void (*on_receive_handler_function)(uint8_t* buffer, uint8_t size);
  typedef void (*on_receive_from_handler_function)(uint8_t* buffer, uint8_t size, uint8_t from);

  // Automatic initialization
  void init(char* token, bool encrypted=true);

  // send message
  SL_ERROR send(uint8_t* buf);
  SL_ERROR send(uint8_t* buf, uint8_t to_addr, uint8_t len);

  // register_handler(on_receive)
  void register_handler(on_receive_handler_function on_receive);
  void register_handler(on_receive_from_handler_function on_receive_from);

  // update()
  void update();
  
  //set pins
  void set_pins(uint8_t cs, uint8_t reset, uint8_t interrupt);
  
  // expose last rssi
  uint8_t get_last_rssi();

  // temporary publics
  void phex(uint8_t* str, unsigned int size);

 private:
  uint8_t last_rssi;
  uint8_t bridge_address = SL_DEFAULT_BRIDGE;
  uint8_t tx_power = SL_DEFAULT_TXPWR;
  sl_config conf;
  sl_pins pins;
  RH_RF95* driver;
//  RHMesh* manager;
  RHDatagram* manager;
  uint8_t slrcvbuffer[SL_MAX_MESSAGE_LEN];
  bool encryption_mode = true;

  on_receive_handler_function _on_receive = NULL;
  on_receive_from_handler_function _on_receive_from = NULL;

  void debug(char* string);
  // encrypt allocates memory, remember to free after processing
  uint8_t* encrypt_alloc(uint8_t* outlen, uint8_t* in, uint8_t inlen, uint8_t* key);
  // decrypt happens in place
  void decrypt(uint8_t* in, uint8_t inlen, uint8_t* key);


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
