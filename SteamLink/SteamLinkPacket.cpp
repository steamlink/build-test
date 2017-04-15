#include "SteamLinkPacket.h"

uint8_t SteamLinkPacket::set_packet(uint8_t* packet, uint8_t* payload, uint8_t payload_length, uint8_t* header, uint8_t header_length) {
  uint8_t packet_length = payload_length + header_length;
  packet = (uint8_t*) malloc(packet_length);
  memcpy(&packet[0], header, header_length);
  memcpy(&packet[header_length], payload, payload_length);
  return packet_length;
}

uint8_t SteamLinkPacket::get_packet(uint8_t* packet, uint8_t packet_length, uint8_t* payload, uint8_t* header, uint8_t header_length) {
  uint8_t payload_length = packet_length - header_length;
  payload = (uint8_t*) malloc(payload_length);
  memcpy(header, &packet[0], header_length)
  memcpy(payload, &packet[header_length], payload_length);
  return payload_length;
}

uint8_t SteamLinkPacket::set_encrypted_packet(uint8_t* packet, uint8_t* payload, uint8_t payload_length, uint8_t* key) {
  uint8_t* encrypted_payload = NULL;
  uint8_t encrypted_payload_length;
  packet = encrypt_alloc(&encrypted_payload_length, payload, payload_length, key);
  return encrypted_payload_length;
}

uint8_t SteamLinkPacket::get_encrypted_packet(uint8_t* packet, uint8_t packet_length, uint8_t* payload, uint8_t* key) {
  decrypt(packet, packet_length, key);
  payload = packet;
  // payload_length == packet_length, so return directly
  return packet_length;
}


uint8_t SteamLinkPacket::set_btype0_packet(uint8_t* packet, uint8_t* payload, uint8_t* payload_length, uint8_t node_addr, uint8_t rssi) {
  //The first field of btyp0 header is two nibbles [btyp][ntyp] or 0
  header_length = sizeof(struct btype_0_header);
  btype_0_header *header = malloc(header_length);
  header->btype = 0;
  header->ntype = 0;
  header->node_addr = node_addr;
  header->rssi = rssi;
  uint8_t packet_size = set_packet(packet, payload, payload_length, (uint8_t*) header, header_length);
  free(header);

}

uint8_t SteamLinkPacket::get_btype0_packet(uint8_t* packet, uint8_t packet_length, uint8_t* payload, uint8_t &node_addr, uint8_t &rssi) {
  btype_0_header *header;
  header_length = sizeof(btype_0_header);
  uint8_t payload_length = get_packet(packet, packet_length, payload, (uint8_t*) header, header_length);
  node_addr = header->node_addr;
  rssi = header->rssi;
  return payload_length;
}


uint8_t* SteamLinkPacket::encrypt_alloc(uint8_t* outlen, uint8_t* in, uint8_t inlen, uint8_t* key) {
  // TODO: remove magic numbers and replace with SL_SIZEOF_KEY
  uint8_t num_blocks = int((inlen+15)/16);
  uint8_t* out = (uint8_t*) malloc(num_blocks*16);
  *outlen = num_blocks*16;
  memcpy(out, in, inlen);
  memset(out + inlen, 0, *outlen - inlen);
  for(int i = 0; i < num_blocks; ++i) {
    AES128_ECB_encrypt(out + i*16, key, out + i*16);
  }
  return out;
}

void SteamLinkPacket::decrypt(uint8_t* in, uint8_t inlen, uint8_t* key) {
  // TODO: remove magic numbers and replace with SL_SIZEOF_KEY
  uint8_t num_blocks = inlen/16;
  for(int i = 0; i < num_blocks; i++) {
    AES128_ECB_decrypt(in+i*16, key, in + i*16);
  }
}
