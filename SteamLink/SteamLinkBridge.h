#ifndef STEAMLINKBRIDGE_H
#define STEAMLINKBRIDGE_H

#include "SteamLinkGeneric.h"

// TODO: add queues?

typedef

class SteamLinkBridge {

public:

  SteamLinkBridge(SteamLinkGeneric *storeDriver);

  void bridge(SteamLinkGeneric *nodeDriver);

  void update();

private:

// admin_control message types
  const uint_8 SL_CTRL_GS = 0x31;   // get status, reply with SS message
  const uint_8 SL_CTRL_TD = 0x32;   // transmit a test message via radio
  const uint_8 SL_CTRL_SR = 0x33;   // set radio paramter to x, acknowlegde with AK or NK
  const uint_8 SL_CTRL_BC = 0x34;   // restart node, no reply
  const uint_8 SL_CTRL_BR = 0x35;   //  reset the radio, acknowlegde with AK or NK

// admin data messages type
  const uint_8 SL_DATA_ON = 0x31;   // onlines, send on startup
  const uint_8 SL_DATA_AK = 0x32;   // acknowlegde the last control message
  const uint_8 SL_DATA_NK = 0x33;   // negative acknowlegde the last control message
  const uint_8 SL_DATA_TR = 0x34;   // Received Test Data
  const uint_8 SL_DATA_SS = 0x35;   // status info and counters

  void init();

  SteamLinkGeneric *_storeDriver;
  SteamLinkGeneric *_nodeDriver;
  bool _init_done = false;

  void node_to_store(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi);
  void store_to_node(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi);
  void handle_admin_packet(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi);
  void UpdStatus(char *newstatus);
  void TransmitTestDataPacket(char *pkt, uint8_t len );
  void SetRadioParam(uint8_t param );
}

SteamLinkBridge::SteamLinkBridge(SteamLinkGeneric *storeDriver) {
  _storeDriver = storeDriver;
  _storeDriver->set_bridge();
}

void SteamLinkBridge::bridge(SteamLinkGeneric *nodeDriver) {
  _nodeDriver = nodeDriver;
  _nodeDriver->set_bridge();
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
  _storeDriver->register_admin_handler(&handle_admin_packet);
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
  if (packet[0] == SL_CTRL_GS) {        // Get Status
	UpdateStatus("online");
  } else if (packet[0] == SL_CTRL_TD) { // Transmit test Data packet
    TransmitTestDataPacket(&packet[1], packet_length - 1);
  } else if (packet[0] == SL_CTRL_SR) { // Set Radio paramater
    SetRadioParam(packet[1]);
  } else if (packet[0] == SL_CTRL_BC) { // Boot Cold 
	// TODO
  } else if (packet[0] == SL_CTRL_BR) { // Boot Radio
	// TODO
  } else {
    return;
  }
}


void SteamLinkBridge::UpdStatus(char *newstatus) {
  uint8_t buf[40];
  uint8_t len;

  len = snprintf((char *)buf, sizeof(buf), "%c%s/%li/%li/%li/%li/%i/%i", \
//TODO:      SL_DATA_SS, newstatus, slsent, slreceived, mqttsent, mqttreceived, mqttQ.queuelevel(), loraQ.queuelevel());
     SL_DATA_SS, newstatus, 0, 0, 0, 0, 0, 0);
  len = MIN(len+1,sizeof(buf));
  _storeDriver->admin_send(buf, len+1 0, 0, 0);
}

void SteamLinkBridge::TransmitTestDataPacket(char *pkt, uint8_t len ) {

	_nodeDriver->bridge_send((uint8_t*)pkt, len, 0/*slid*/, SL_LORA_FLAG_TEST, 0);
}

void SteamLinkBridge::SetRadioParam(uint8_t param ) {

	_nodeDriver->set_modem_config(param);
}
#endif
