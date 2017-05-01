#include <SteamLinkGeneric.h>
#include <SteamLinkBridge.h>

bool SteamLinkBridge::_init_done = false;
SteamLinkGeneric* SteamLinkBridge::_storeDriver;
SteamLinkGeneric* SteamLinkBridge::_nodeDriver;

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
  _storeDriver->update();
  _nodeDriver->update();
}

void SteamLinkBridge::init() {
  _storeDriver->register_bridge_handler(&store_to_node);
  _nodeDriver->register_bridge_handler(&node_to_store);
  _storeDriver->register_admin_handler(&handle_admin_packet);
  SendAdminOp(SteamLinkBridge::SL_DATA_ON);  // report us Online
  _init_done = true;
}

void SteamLinkBridge::node_to_store(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi) {
  _storeDriver->bridge_send(packet, packet_length, slid, flags, rssi);
}

void SteamLinkBridge::store_to_node(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi) {
  _nodeDriver->bridge_send(packet, packet_length, slid, flags, rssi);
}

void SteamLinkBridge::handle_admin_packet(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi) {

  if (packet_length < 1) {
    return;  // TODO: error handling
  }
  if (packet[0] == SteamLinkBridge::SL_CTRL_GS) {        // Get Status
    UpdStatus((uint8_t*) "online");
  } else if (packet[0] == SteamLinkBridge::SL_CTRL_TD) { // Transmit test Data packet
    TransmitTestDataPacket(&packet[1], packet_length - 1);
  } else if (packet[0] == SteamLinkBridge::SL_CTRL_SR) { // Set Radio paramater
    SetRadioParam(packet[1]);
  } else if (packet[0] == SteamLinkBridge::SL_CTRL_BC) { // Boot Cold
        // TODO
  } else if (packet[0] == SteamLinkBridge::SL_CTRL_BR) { // Boot Radio
        // TODO
  } else {
    return;
  }
}

// Admin Operations

void SteamLinkBridge::SendAdminOp(uint8_t stat) {
    _storeDriver->admin_send(&stat, 1, 0, 0, 0);
}

void SteamLinkBridge::UpdStatus(uint8_t* newstatus) {
  uint8_t buf[40];
  uint8_t len;

  len = snprintf((char *)buf, sizeof(buf), "%c%s/%li/%li/%li/%li/%i/%i", \
                 //TODO:      SteamLinkBridge::SL_DATA_SS, newstatus, slsent, slreceived, mqttsent, mqttreceived, mqttQ.queuelevel(), loraQ.queuelevel());
                 SteamLinkBridge::SL_DATA_SS, newstatus, 0, 0, 0, 0, 0, 0);
  // we only need to send the length we use in the buf
  len = MIN(len+1,sizeof(buf));
  _storeDriver->admin_send(buf, len+1, 0, 0, 0);
}

void SteamLinkBridge::TransmitTestDataPacket(uint8_t* pkt, uint8_t len ) {
  store_to_node((uint8_t*)pkt, len, 0/*slid*/, SL_LORA_FLAG_TEST, 0);
  SendAdminOp(SteamLinkBridge::SL_DATA_AK);
}

void SteamLinkBridge::SetRadioParam(uint8_t param ) {
 _nodeDriver->set_modem_config(param);
 SendAdminOp(SteamLinkBridge::SL_DATA_AK);
}
