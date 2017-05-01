#ifndef STEAMLINKBRIDGE_H
#define STEAMLINKBRIDGE_H

#include <SteamLinkGeneric.h>
#include <SteamLink.h>

#define SL_LORA_FLAG_TEST 0b00000001

class SteamLinkBridge {

public:

  SteamLinkBridge(SteamLinkGeneric *storeDriver);

  void bridge(SteamLinkGeneric *nodeDriver);

  void update();

  void init();

private:

// admin_control message types
  static const uint8_t SL_CTRL_GS = 0x31;   // get status, reply with SS message
  static const uint8_t SL_CTRL_TD = 0x32;   // transmit a test message via radio
  static const uint8_t SL_CTRL_SR = 0x33;   // set radio paramter to x, acknowlegde with AK or NK
  static const uint8_t SL_CTRL_BC = 0x34;   // restart node, no reply
  static const uint8_t SL_CTRL_BR = 0x35;   //  reset the radio, acknowlegde with AK or NK

// admin data messages type
  static const uint8_t SL_DATA_ON = 0x31;   // onlines, send on startup
  static const uint8_t SL_DATA_AK = 0x32;   // acknowlegde the last control message
  static const uint8_t SL_DATA_NK = 0x33;   // negative acknowlegde the last control message
  static const uint8_t SL_DATA_TR = 0x34;   // Received Test Data
  static const uint8_t SL_DATA_SS = 0x35;   // status info and counters

  static SteamLinkGeneric *_storeDriver;
  static SteamLinkGeneric *_nodeDriver;
  static bool _init_done;

  static void node_to_store(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi);
  static void store_to_node(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi);
  static void handle_admin_packet(uint8_t* packet, uint8_t packet_length, uint32_t slid, uint8_t flags, uint8_t rssi);

  static void SendAdminOp(uint8_t stat);
  static void UpdStatus(uint8_t* newstatus);
  static void TransmitTestDataPacket(uint8_t* pkt, uint8_t len );
  static void SetRadioParam(uint8_t param );
};

#endif
