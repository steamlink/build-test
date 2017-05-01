#include <SteamLinkGeneric.h>

SteamLinkGeneric::SteamLinkGeneric(uint32_t slid) {
  _slid = slid;
}

void SteamLinkGeneric::init(bool encrypted, uint8_t* key) {
  _encrypted = encrypted;
  _key = key;
}

void SteamLinkGeneric::update() {
  return;
}

void SteamLinkGeneric::register_receive_handler(on_receive_handler_function on_receive) {
  _on_receive = on_receive;
}

void SteamLinkGeneric::register_bridge_handler(on_receive_bridge_handler_function on_receive) {
  _on_bridge_receive = on_receive;
}

void SteamLinkGeneric::register_admin_handler(on_receive_bridge_handler_function on_receive) {
  _on_admin_receive = on_receive;
}

/*
void SteamLinkGeneric::set_bridge() {
  _is_bridge = true;
}

void SteamLinkGeneric::unset_bridge() {
  _is_bridge = false;
}
*/

bool SteamLinkGeneric::is_primary() {
  return _is_primary;
}

bool SteamLinkGeneric::bridge_send(uint8_t* packet, uint8_t packet_size, uint32_t slid, uint8_t flags, uint8_t rssi) {
  return false;
}

bool SteamLinkGeneric::admin_send(uint8_t* packet, uint8_t packet_size, uint32_t slid, uint8_t flags, uint8_t rssi) {
  return false;
}


void SteamLinkGeneric::set_modem_config(uint8_t param) {
  return;
}
