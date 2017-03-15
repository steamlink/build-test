#ifndef STEAMLINK_PRIMARY_H
#define STEAMLINK_PRIMARY_H

#include "SteamLink_types.h"
#include "aes.h"

// TODO: Move to driver?
#define SL_MAX_MESSAGE_LEN 64
#define SL_DEFAULT_BRIDGE 1

#ifndef SL_FORWARD
#define SL_FORWARD 1
#endif

/*
A SteamLinkPrimary node:
Must speak MQTT!
- Can send a b_type packet to the store via it's send method
- Can receives a b_type packet from the store
- If received b_type packet from store is for itself, pass the unencrypted string to on_receive callback
- If received b_type packet is for someone else, convert to n_type and use driver to pass it on

Send and Receive are user functions, always


*/

class SteamLinkPrimary {

  public:

  // create a typedef for on receive functions
  typedef void (*on_receive_handler_function)(uint8_t* buffer, uint8_t size);
  typedef void (*on_receive_from_handler_function)(uint8_t* buffer, uint8_t size, uint8_t from);

  // receive from secondary, if secondary driver provided
  // send btypes to store
  // on receive from store


}

