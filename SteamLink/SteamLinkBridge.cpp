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
    INFO("In Bridge Update, running init()\n");
    init();
    INFO("init done\nl");
  }
  _storeSideDriver->update();
  _nodeSideDriver->update();
}


void SteamLinkBridge::init() {
  _storeSideDriver->register_bridge_handler(&router);
  _nodeSideDriver->register_bridge_handler(&router);
  _storeSideDriver->set_bridge(storeside);
  _nodeSideDriver->set_bridge(nodeside);
  _init_done = true;
}

void SteamLinkBridge::router(uint8_t* packet, uint8_t packet_length, uint32_t slid) {
  INFO("SteamLinkBridge::router: slid ");
  INFO(slid);
  INFONL(" packet: ");
  INFOPHEX(packet, packet_length);
  if (slid == SL_DEFAULT_STORE_ADDR) {
    _storeSideDriver->handle_admin_packet(packet, packet_length);
  } else if (slid == _storeSideDriver->get_slid()) {
    _storeSideDriver->handle_admin_packet(packet, packet_length);
  } else if (slid == _nodeSideDriver->get_slid()) {
    _nodeSideDriver->handle_admin_packet(packet, packet_length);
  } else {
    FATAL("SteamLinkBridge::router  unroutable slid:");
    FATALNL(slid);
  }
}

bool SteamLinkBridge::_init_done = false;
SteamLinkGeneric* SteamLinkBridge::_storeSideDriver;
SteamLinkGeneric* SteamLinkBridge::_nodeSideDriver;


