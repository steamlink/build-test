/*
This is the standard packet class for SteamLink
All packets have a header and a payload

NOTE FOR ALL PACKET TYPES:
The header size + format is predetermined.

// binary packet if encrypted
+------------------------+
|   encrypted_payload....|
+------------------------+

+------+------+-----+-----+---------------------+
| slid | rssi |  op | qos | encrypted_payload...|
+------+------+-----+-----+---------------------+
|<------ HEADER --------->|

store to bridge like bridge to store, with rssi = 0
bridge to node like node to bridge

*/

#ifndef STEAMLINK_PACKET_H
#define STEAMLINK_PACKET_H

#include "aes.h"
#include <SteamLink.h>

#define SL_SIZEOF_KEY 16

#define SET_VAL2BIT(_var, _val) ( (_var) | (((_val) & 3) << 6) )
#define SET_VAL6BIT(_var, _val) ( (_var) | ((_val) & 63) )

#define GET_VAL2BIT(_var) ( ((_var) >> 6) & 3 )
#define GET_VAL6BIT(_var) ( (_var) & 63 )


class SteamLinkPacket {

 public:

  /// set_packet
  /// \param packet pointer to output packet
  /// \param payload pointer to payload
  /// \param payload_length size of payload
  /// \param header pointer to header
  /// \param header_length size of header
  /// \returns the size of the packet created
  static uint8_t set_packet(uint8_t* &packet, uint8_t* payload, uint8_t payload_length, uint8_t* header, uint8_t header_length);

  /// set_encrypted_packet
  /// \brief encrypt a payload with no additional header
  /// \param packet pointer to packet
  /// \param payload pointer to payload
  /// \param payload_length size of payload
  /// \param key 16 byte encryption key
  /// \returns the size of the packet created
  static uint8_t set_encrypted_packet(uint8_t* &packet, uint8_t* payload, uint8_t payload_length, uint8_t* key);

  /// get_encrypted_packet
  /// \brief decrypt a payload when packet has no additional header
  /// \param packet pointer to packet
  /// \param packet_length size of packet
  /// \param payload pointer to payload
  /// \param key 16 byte encryption key
  /// \returns the size of the payload extracted
  static uint8_t get_encrypted_packet(uint8_t* packet, uint8_t packet_length, uint8_t* &payload, uint8_t* key);


  //////////// NOT REQUIRED //////////////
  /*
  /// set_bridge_packet
  /// \param packet pointer to packet
  /// \param payload pointer to payload
  /// \param payload_length size of payload
  /// \param slid by reference
  /// \param flags by reference
  /// \param rssi
  /// \returns the size of the packet created
  static uint8_t set_bridge_packet(uint8_t* &packet, uint8_t* payload, uint8_t payload_length, uint8_t rssi, uint8_t qos);

  /// get_bridge_packet
  /// \param packet pointer to packet
  /// \param packet_length size of packet
  /// \param payload pointer to payload
  /// \param slid by reference
  /// \param flags by reference
  /// \param rssi by reference
  /// \returns the size of payload extracted
  static uint8_t get_bridge_packet(uint8_t* packet, uint8_t packet_length, uint8_t* &payload, uint8_t &rssi, uint8_t &qos);

  static uint8_t set_op_packet(uint8_t* packet, uint8_t* payload, uint8_t payload_length, uint8_t op);
  static uint8_t get_op_packet(uint8_t* packet, uint8_t packet_length, uint8_t* &payload, uint8_t &op);

  static uint8_t set_node_packet(uint8_t* &packet, uint8_t* payload, uint8_t payload_length, uint8_t to_addr, uint8_t from_addr, uint8_t flags, bool encrypt = false, uint8_t* key = NULL);
  static uint8_t get_node_packet(uint8_t* packet, uint8_t packet_length, uint8_t* &payload, uint8_t &to_addr, uint8_t &from_addr, uint8_t &flags, bool encrypt = false, uint8_t* key = NULL);
  */

 private:

  static uint8_t* encrypt_alloc(uint8_t* outlen, uint8_t* in, uint8_t inlen, uint8_t* key);

  static void decrypt(uint8_t* in, uint8_t inlen, uint8_t* key);

};

#endif
