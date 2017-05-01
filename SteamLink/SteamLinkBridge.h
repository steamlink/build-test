#ifndef STEAMLINKBRIDGE_H
#define STEAMLINKBRIDGE_H

#include <SteamLinkGeneric.h>
#include <SteamLink.h>

// admin_control message types
#define SL_CTRL_GS 0x31   // get status, reply with SS message
#define SL_CTRL_TD 0x32   // transmit a test message via radio
#define SL_CTRL_SR 0x33   // set radio paramter to x, acknowlegde with AK or NK
#define SL_CTRL_BC 0x34   // restart node, no reply
#define SL_CTRL_BR 0x35   //  reset the radio, acknowlegde with AK or NK

// admin data messages type
#define SL_DATA_ON 0x31   // onlines, send on startup
#define SL_DATA_AK 0x32   // acknowlegde the last control message
#define SL_DATA_NK 0x33   // negative acknowlegde the last control message
#define SL_DATA_TR 0x34   // Received Test Data
#define SL_DATA_SS 0x35   // status info and counters

#define SL_TEST_FLAG 0b00000001

class SteamLinkBridge {

public:

  SteamLinkBridge(SteamLinkGeneric *storeDriver);

  void bridge(SteamLinkGeneric *nodeDriver);

  void update();

  void init();

private:


  static SteamLinkGeneric *_storeDriver;
  static SteamLinkGeneric *_nodeDriver;
  static bool _init_done;

  static void node_to_store(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi);
  static void store_to_node(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi);
  static void handle_admin_packet(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi);
  static void UpdStatus(uint8_t* newstatus);
  static void TransmitTestDataPacket(uint8_t* pkt, uint8_t len );
  static void SetRadioParam(uint8_t param );
};

bool SteamLinkBridge::_init_done = false;

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
        UpdStatus((uint8_t*) "online");
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


void SteamLinkBridge::UpdStatus(uint8_t* newstatus) {
  uint8_t buf[40];
  uint8_t len;

  len = snprintf((char *)buf, sizeof(buf), "%c%s/%li/%li/%li/%li/%i/%i", \
//TODO:      SL_DATA_SS, newstatus, slsent, slreceived, mqttsent, mqttreceived, mqttQ.queuelevel(), loraQ.queuelevel());
     SL_DATA_SS, newstatus, 0, 0, 0, 0, 0, 0);
  len = MIN(len+1,sizeof(buf));
  _storeDriver->admin_send(buf, len+1, 0, 0, 0);
}

void SteamLinkBridge::TransmitTestDataPacket(uint8_t* pkt, uint8_t len ) {

        _nodeDriver->bridge_send((uint8_t*)pkt, len, 0/*slid*/, SL_TEST_FLAG, 0);
}

void SteamLinkBridge::SetRadioParam(uint8_t param ) {

        _nodeDriver->set_modem_config(param);
}
#endif
