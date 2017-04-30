#ifndef STEAMLINKBRIDGE_H
#define STEAMLINKBRIDGE_H

#include "SteamLinkGeneric.h"

// TODO: add queues?

class SteamLinkBridge {

 public:

  SteamLinkBridge(SteamLinkGeneric *storeDriver);

  void bridge(SteamLinkGeneric *nodeDriver);

  void update();

 private:

  void init();

  SteamLinkGeneric *_storeDriver;
  SteamLinkGeneric *_nodeDriver;
  bool _init_done = false;

  void node_to_store(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi);
  void store_to_node(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi);
}

SteamLinkBridge::SteamLinkBridge(SteamLinkGeneric *storeDriver) {
  _storeDriver = storeDriver;
}

void SteamLinkBridge::bridge(SteamLinkGeneric *nodeDriver) {
  _nodeDriver = nodeDriver;
}

void SteamLinkBridge::update() {
  if (!_init_done) {
    init();
  }
  *_storeDriver->update();
  *_nodeDriver->update();
}

void SteamLinkBridge::init() {
  _storeDriver->register_bridge_handler(&store_to_node);
  _nodeDriver->register_bridge_handler(&node_to_store);
}

void SteamLinkBridge::node_to_store(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi) {
  _storeDriver->bridge_send(packet, packet_length, slid, flags, rssi);
}

void SteamLinkBridge::store_to_node(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi) {
  _nodeDriver->bridge_send(packet, packet_length, slid, flags, rssi);
}

#endif
