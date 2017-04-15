/*
This is the standard packet class for SteamLink
All packets have a header and a payload

NOTE FOR ALL PACKET TYPES:
The header size + format is predetermined.

// binary packet if encrypted
+------------------------+
|   encrypted_payload....|
+------------------------+

// binary packet
+-----+-----+-----+----+----------------------+
|b_typ|slid |rssi |flag|  encrypted_payload...|
+-----+-----+-----+----+----------------------+
|<---BRIDGE_WRAP------>|<----NODE_PKT------>|

store to bridge like bridge to store, with rssi = 0
bridge to node like node to bridge

TODO: make functions static?

*/

#ifndef STEAMLINK_PACKET_H
#define STEAMLINK_PACKET_H

#include "aes.h"

#define SL_SIZEOF_KEY 16

class SteamLinkPacket {

 public:

  /// set_packet
  /// \param packet pointer to output packet
  /// \param payload pointer to payload
  /// \param payload_length size of payload
  /// \param header pointer to header
  /// \param header_length size of header
  /// \returns the size of the packet created
  uint8_t set_packet(uint8_t* packet, uint8_t* payload, uint8_t payload_length, uint8_t* header, uint8_t header_length);

  /// set_packet
  /// \param packet pointer to input packet
  /// \param packet_length size of input packet
  /// \param payload pointer to payload
  /// \param header pointer to header
  /// \param header_length known size of header
  /// \returns the size of the payload
  uint8_t get_packet(uint8_t* packet, uint8_t packet_length, uint8_t* payload, uint8_t* header, uint8_t header_length);

  /// set_encrypted_packet
  /// \brief encrypt a payload with no additional header
  /// \param packet pointer to packet
  /// \param payload pointer to payload
  /// \param payload_length size of payload
  /// \param key 16 byte encryption key
  /// \returns the size of the packet created
  uint8_t set_encrypted_packet(uint8_t* packet, uint8_t* payload, uint8_t payload_length, uint8_t* key);

  /// get_encrypted_packet
  /// \brief decrypt a payload when packet has no additional header
  /// \param packet pointer to packet
  /// \param packet_length size of packet
  /// \param payload pointer to payload
  /// \param key 16 byte encryption key
  /// \returns the size of the payload extracted
  uint8_t get_encrypted_packet(uint8_t* packet, uint8_t packet_length, uint8_t* payload, uint8_t* key);

  /// set_btype_packet
  /// \param packet pointer to packet
  /// \param payload pointer to payload
  /// \param payload_length size of payload
  /// \param node_addr
  /// \param rssi
  /// \returns the size of the packet created
  uint8_t set_btype0_packet(uint8_t* packet, uint8_t* payload, uint8_t payload_length, uint8_t node_addr, uint8_t rssi);

  /// get_btype0_packet
  /// \param packet pointer to packet
  /// \param packet_length size of packet
  /// \param payload pointer to payload
  /// \param node_addr by reference
  /// \param rssi by reference
  /// \returns the size of payload extracted
  uint8_t get_btype0_packet(uint8_t* packet, uint8_t packet_length, uint8_t* payload, uint8_t &node_addr, uint8_t &rssi);

 private:

  uint8_t* encrypt_alloc(uint8_t* outlen, uint8_t* in, uint8_t inlen, uint8_t* key);

  void decrypt(uint8_t* in, uint8_t inlen, uint8_t* key);

}

#pragma pack(push,1)
struct btype_0_header {
  unsigned  btype: 4;
  unsigned  ntype: 4;
  uint8_t  node_addr;
  uint8_t  rssi;
}
#pragma pack(pop)

#pragma pack(push,1)
  struct ntype_0_header {
    uint32_t  slid;
  }
#pragma pack(pop)

#endif
