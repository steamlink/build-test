#include <SteamLinkPacket.h>
#include <SteamLink.h>

uint8_t SteamLinkPacket::set_packet(uint8_t* &packet, uint8_t* payload, uint8_t payload_length, uint8_t* header, uint8_t header_length) {
  INFO("SteamLinkPacket - setting packet");
  uint8_t packet_length = payload_length + header_length;
  packet = (uint8_t*) malloc(packet_length);
  INFO("SteamLinkPacket - memory allocated for packet");
  memcpy(&packet[0], header, header_length);
  INFO("SteamLinkPacket - header copied in to packet");
  memcpy(&packet[header_length], payload, payload_length);
  INFO("SteamLinkPacket - payload copied in to packet");
  return packet_length;
}

uint8_t SteamLinkPacket::get_packet(uint8_t* packet, uint8_t packet_length, uint8_t* &payload, uint8_t* header, uint8_t header_length) {
  INFO("SteamLinkPacket - getting packet");
  uint8_t payload_length = packet_length - header_length;
  payload = (uint8_t*) malloc(payload_length);
  memcpy(header, &packet[0], header_length);
  memcpy(payload, &packet[header_length], payload_length);
  return payload_length;
}

uint8_t SteamLinkPacket::set_encrypted_packet(uint8_t* &packet, uint8_t* payload, uint8_t payload_length, uint8_t* key) {
  INFO("SteamLinkPacket - setting encrypted packet");
  uint8_t* encrypted_payload = NULL;
  uint8_t encrypted_payload_length;
  packet = encrypt_alloc(&encrypted_payload_length, payload, payload_length, key);
  return encrypted_payload_length;
}

uint8_t SteamLinkPacket::get_encrypted_packet(uint8_t* packet, uint8_t packet_length, uint8_t* &payload, uint8_t* key) {
  INFO("SteamLinkPacket - decrypting packet");
  decrypt(packet, packet_length, key);
  payload = packet;
  return packet_length;
}

uint8_t SteamLinkPacket::set_bridge_packet(uint8_t* &packet, uint8_t* payload, uint8_t payload_length, uint32_t slid, uint8_t flags, uint8_t rssi) {
  INFO("SteamLinkPacket - setting bridge packet");
  bridge_header header;
  header.slid = slid;
  header.flags = flags;
  header.rssi = rssi;
  INFO("SteamLinkPacket - calling set packet with bridge header and payload");
  uint8_t packet_size = set_packet(packet, payload, payload_length, (uint8_t*) &header, sizeof(header));
}

uint8_t SteamLinkPacket::get_bridge_packet(uint8_t* packet, uint8_t packet_length, uint8_t* &payload, uint32_t &slid, uint8_t &flags, uint8_t &rssi) {
  INFO("SteamLinkPacket - getting bridge packet");
  bridge_header header;
  INFO("SteamLinkPacket - getting packet with bridge header parameters");
  uint8_t payload_length = get_packet(packet, packet_length, payload, (uint8_t*) &header, sizeof(header));
  slid = header.slid;
  flags = header.flags;
  rssi = header.rssi;
  return payload_length;
}

uint8_t SteamLinkPacket::set_node_packet(uint8_t* &packet, uint8_t* payload, uint8_t payload_length, uint8_t to_addr, uint8_t from_addr, uint8_t flags, bool encrypt, uint8_t* key) {
  node_header header;
  header.to = to_addr;
  header.from = from_addr;
  header.flags = flags;
  if (encrypt) {
    uint8_t encrypted_payload_length;
    uint8_t* encrypted_payload;
    encrypted_payload = encrypt_alloc(&encrypted_payload_length, payload, payload_length, key);
    return set_packet(packet, encrypted_payload, encrypted_payload_length, (uint8_t*) &header, sizeof(header));
  } else {
    return set_packet(packet, payload, payload_length, (uint8_t*) &header, sizeof(header));
  }
}

uint8_t SteamLinkPacket::get_node_packet(uint8_t* packet, uint8_t packet_length, uint8_t* &payload, uint8_t &to_addr, uint8_t &from_addr, uint8_t &flags, bool encrypt, uint8_t* key) {
  node_header header;
  uint8_t payload_length;
  payload_length = get_packet(packet, packet_length, payload, (uint8_t*) &header, sizeof(header));
  to_addr = header.to;
  from_addr = header.from;
  flags = header.flags;
  if (encrypt) {
    decrypt(payload, payload_length, key);
  }
  return payload_length;
}

uint8_t* SteamLinkPacket::encrypt_alloc(uint8_t* outlen, uint8_t* in, uint8_t inlen, uint8_t* key) {
  uint8_t num_blocks = int((inlen+SL_SIZEOF_KEY-1)/SL_SIZEOF_KEY);
  uint8_t* out = (uint8_t*) malloc(num_blocks*SL_SIZEOF_KEY);
  *outlen = num_blocks*SL_SIZEOF_KEY;
  memcpy(out, in, inlen);
  memset(out + inlen, 0, *outlen - inlen);
  for(int i = 0; i < num_blocks; ++i) {
    AES128_ECB_encrypt(out + i*SL_SIZEOF_KEY, key, out + i*SL_SIZEOF_KEY);
  }
  return out;
}

void SteamLinkPacket::decrypt(uint8_t* in, uint8_t inlen, uint8_t* key) {
  uint8_t num_blocks = inlen/SL_SIZEOF_KEY;
  for(int i = 0; i < num_blocks; i++) {
    AES128_ECB_decrypt(in+i*SL_SIZEOF_KEY, key, in + i*SL_SIZEOF_KEY);
  }
}
