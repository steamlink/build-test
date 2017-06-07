#include <SteamLinkGeneric.h>
#include <SteamLinkBridge.h>
#include <SteamLink.h>

SteamLinkBridge::SteamLinkBridge(SteamLinkGeneric *storeSideDriver) {
  _storeSideDriver = storeSideDriver;
}

void SteamLinkBridge::bridge(SteamLinkGeneric *nodeSideDriver) {
  _nodeSideDriver = nodeSideDriver;
}

void SteamLinkBridge::update() {
  if (!_init_done) {
    INFO("In Bridge Update, running init()");
    init();
    INFO("init done");
  }
  _storeSideDriver->update();
  _nodeSideDriver->update();
}


void SteamLinkBridge::init() {
  _storeSideDriver->register_bridge_handler(&router);
  _nodeSideDriver->register_bridge_handler(&router);
  _storeSideDriver->set_bridge();
  _nodeSideDriver->set_bridge();
  _init_done = true;
  _storeSideDriver->send_on();
}

void SteamLinkBridge::router(uint8_t* packet, uint8_t packet_length, uint32_t slid) {
  if (slid == SL_DEFAULT_STORE_ADDR) {
    _storeSideDriver->handle_admin_packet(packet, packet_length, false); // last arg because we are not coming from phys layer
  } if (slid == _storeSideDriver->get_slid()) {
    _storeSideDriver->handle_admin_packet(packet, packet_length, false); // last arg because we are not coming from phys layer
  } else if (slid == _nodeSideDriver->get_slid()) {
    _nodeSideDriver->handle_admin_packet(packet, packet_length, false); // last arg because we are not coming from phys layer
  }
  // else something went wrong! we don't have a routing table to handle this!
}

bool SteamLinkBridge::_init_done = false;
SteamLinkGeneric* SteamLinkBridge::_storeSideDriver;
SteamLinkGeneric* SteamLinkBridge::_nodeSideDriver;


