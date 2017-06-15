#ifndef STEAMLINK_H
#define STEAMLINK_H

#include <Arduino.h>

typedef void (*on_receive_handler_function)(uint8_t* buffer, uint8_t size); // user
typedef void (*on_receive_bridge_handler_function)(uint8_t* packet, uint8_t packet_length, uint32_t to_slid); // admin

#define  MIN(a,b) (((a)<(b))?(a):(b))
#define  MAX(a,b) (((a)>(b))?(a):(b))

// admin_control message types: EVEN, 0 bottom bit
#define SL_OP_DN 0x30  // data to node, ACK for qos 2
#define SL_OP_BN 0x32  // slid precedes payload, bridge forward to node
#define SL_OP_GS 0x34  // get status, reply with SS message
#define SL_OP_TD 0x36  // transmit a test message via radio
#define SL_OP_SR 0x38  // set radio paramter to x, acknowlegde with AK or NK
#define SL_OP_BC 0x3A  // restart node, no reply
#define SL_OP_BR 0x3C  // reset the radio, acknowlegde with AK or NK

// admin_data message types: ODD, 1 bottom bit
#define SL_OP_DS 0x31  // data to store
#define SL_OP_BS 0x33  // bridge to store
#define SL_OP_ON 0x35  // send status on to store
#define SL_OP_AK 0x37  // acknowledge the last control message
#define SL_OP_NK 0x39  // negative acknowlegde the last control message
#define SL_OP_TR 0x3B  // Received Test Data
#define SL_OP_SS 0x3D  // status info and counters

// DEBUG INSTRUCTIONS:
//     Change below line or comment out to change the level of debugging
//     Make sure Serial is defined if DEBUG is enabled
//     Debugger uses Serial.println() to debug
#define DEBUG_ENABLED DEBUG_LEVEL_INFO

#define DEBUG_LEVEL_INFO   4
#define DEBUG_LEVEL_WARN   3
#define DEBUG_LEVEL_ERR    2
#define DEBUG_LEVEL_FATAL  1
#define DEBUG_LEVEL_NONE   0

#if DEBUG_ENABLED >= DEBUG_LEVEL_INFO
 #define INFONL(text) Serial.println(text)
 #define INFO(text) Serial.print(text)
 #define INFOPHEX(data, len) phex(data, len)
#else 
 #define INFONL(text) ((void)0)
 #define INFO(text) ((void)0)
 #define INFOPHEX(data, len) ((void)0)
#endif

#if DEBUG_ENABLED >= DEBUG_LEVEL_WARN
 #define WARNNL(text) Serial.println(text)
 #define WARN(text) Serial.print(text)
 #define WARNPHEX(data, len) phex(data, len)
#else 
 #define WARNNL(text) ((void)0)
 #define WARN(text) ((void)0)
 #define WARNPHEX(data, len) ((void)0)
#endif

#if DEBUG_ENABLED >= DEBUG_LEVEL_ERR
 #define ERRNL(text) Serial.println(text)
 #define ERR(text) Serial.print(text)
 #define ERRPHEX(data, len) phex(data, len)
#else 
 #define ERRNL(text) ((void)0)
 #define ERR(text) ((void)0)
 #define ERRPHEX(data, len) ((void)0)
#endif

#if DEBUG_ENABLED >= DEBUG_LEVEL_FATAL
 #define FATALNL(text) Serial.println(text)
 #define FATAL(text) Serial.print(text)
 #define FATALPHEX(data, len) phex(data, len)
#else 
 #define FATALNL(text) ((void)0)
 #define FATAL(text) ((void)0)
 #define FATALPHEX(data, len) ((void)0)
#endif



////////////////////////////////////////
// HEADER STRUCTURES
////////////////////////////////////////

/// DATA PACKETS ///

#pragma pack(push,1)
struct ds_header {
  uint8_t op;
  uint32_t slid;
  uint8_t qos;
};
#pragma pack(pop)

#pragma pack(push,1)
struct bs_header {
  uint8_t op;
  uint32_t slid;
  uint8_t rssi;
  uint8_t qos;
};
#pragma pack(pop)

#pragma pack(push,1)
struct on_header {
  uint8_t op;
  uint32_t slid;
};
#pragma pack(pop)

#pragma pack(push,1)
struct ak_header { // no payload required
  uint8_t op;
  uint32_t slid;
};
#pragma pack(pop)

#pragma pack(push,1)
struct nk_header { // no payload required
  uint8_t op;
  uint32_t slid;
};
#pragma pack(pop)

#pragma pack(push,1)
struct tr_header { // data received is payload
  uint8_t op;
  uint32_t slid;
  uint8_t rssi;
};
#pragma pack(pop)

#pragma pack(push,1)
struct ss_header { // payload contains queue and fill information
  uint8_t op;
  uint32_t slid;
};
#pragma pack(pop)

//////////////////////////////

// CONTROL PACKETS

#pragma pack(push,1)
struct dn_header {
  uint8_t op;
  uint32_t slid;
  uint8_t qos;
};
#pragma pack(pop)

#pragma pack(push,1)
struct bn_header {
  uint8_t op;
  uint32_t slid;
};
#pragma pack(pop)

#pragma pack(push,1)
struct gs_header { // no payload required
  uint8_t op;
};
#pragma pack(pop)

#pragma pack(push,1)
struct td_header { // no payload required
  uint8_t op;
};
#pragma pack(pop)

#pragma pack(push,1)
struct sr_header { // payload contains radio parameters
  uint8_t op;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct bc_header {  // no payload required
  uint8_t op;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct br_header { // no payload required
  uint8_t op;
};
#pragma pack(pop)

void phex(uint8_t *data, unsigned int length);

enum BridgeMode { unbridged, storeside, nodeside };

#endif
