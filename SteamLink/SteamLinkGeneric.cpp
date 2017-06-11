#include <SteamLinkGeneric.h>

SteamLinkGeneric::SteamLinkGeneric(uint32_t slid) {
  _slid = slid;
  _encrypted = false;
  _key = NULL;
}

void SteamLinkGeneric::init(void *conf) {
}

bool SteamLinkGeneric::send(uint8_t* buf) {
  uint8_t len = strlen((char*) buf) + 1; // +1 for trailing nul
  return send_ds(buf, len);
}

void SteamLinkGeneric::update() {
  if (!sign_on_complete) {
    sign_on_procedure();
    sign_on_complete = true;
  }
  uint8_t* packet;
  uint8_t packet_length;
  uint32_t slid;
  bool is_test;
  bool received = driver_receive(packet, packet_length, slid, is_test);
  if (received) {
    if (!is_test) {
      if ((slid == _slid) || ((slid == SL_DEFAULT_STORE_ADDR) && _is_bridge )) {
        handle_admin_packet(packet, packet_length, true);
      }
    } else { // is test packet
      if (slid == _slid) {
        send_tr(packet, packet_length);
      }
    }
  }
}

void SteamLinkGeneric::register_receive_handler(on_receive_handler_function on_receive) {
  _on_receive = on_receive;
}


void SteamLinkGeneric::register_bridge_handler(on_receive_bridge_handler_function on_receive) {
  _bridge_handler = on_receive;
}

bool SteamLinkGeneric::driver_send(uint8_t* packet, uint8_t packet_size, uint32_t slid, bool is_test) {
  return false;
}

bool SteamLinkGeneric::driver_receive(uint8_t* &packet, uint8_t &packet_size, uint32_t &slid, bool &is_test) {
  return false;
}

bool SteamLinkGeneric::send_ds(uint8_t* payload, uint8_t payload_length) {
  uint8_t* packet;
  uint8_t packet_length;
  ds_header header;
  header.op = SL_OP_DS;
  header.slid = _slid;
  header.qos = SL_DEFAULT_QOS;
  packet_length = SteamLinkPacket::set_packet(packet, payload, payload_length, (uint8_t*) &header, sizeof(header));
  bool sent = driver_send(packet, packet_length, SL_DEFAULT_STORE_ADDR, false);
  free(packet);
  return sent;
}

bool SteamLinkGeneric::send_bs(uint8_t* payload, uint8_t payload_length) {
  uint8_t* packet;
  uint8_t packet_length;
  bs_header header;
  header.op = SL_OP_BS;
  header.slid = _slid;
  header.rssi = _last_rssi;
  header.qos = SL_DEFAULT_QOS;
  packet_length = SteamLinkPacket::set_packet(packet, payload, payload_length, (uint8_t*) &header, sizeof(header));
  bool sent = driver_send(packet, packet_length, SL_DEFAULT_STORE_ADDR, false);
  free(packet);
  return sent;
}

bool SteamLinkGeneric::send_on() {
  uint8_t* packet;
  uint8_t packet_length;
  on_header header;
  header.op = SL_OP_ON;
  header.slid = _slid;
  packet_length = SteamLinkPacket::set_packet(packet, NULL, 0, (uint8_t*) &header, sizeof(header));
  bool sent = driver_send(packet, packet_length, SL_DEFAULT_STORE_ADDR, false);
  free(packet);
  return sent;
}

bool SteamLinkGeneric::send_ak() {
  uint8_t* packet;
  uint8_t packet_length;
  ak_header header;
  header.op = SL_OP_AK;
  header.slid = _slid;
  packet_length = SteamLinkPacket::set_packet(packet, NULL, 0, (uint8_t*) &header, sizeof(header));
  bool sent = driver_send(packet, packet_length, SL_DEFAULT_STORE_ADDR, false);
  free(packet);
  return sent;
}

bool SteamLinkGeneric::send_nk() {
  uint8_t* packet;
  uint8_t packet_length;
  nk_header header;
  header.op = SL_OP_NK;
  header.slid = _slid;
  packet_length = SteamLinkPacket::set_packet(packet, NULL, 0, (uint8_t*) &header, sizeof(header));
  bool sent = driver_send(packet, packet_length, SL_DEFAULT_STORE_ADDR, false);
  free(packet);
  return sent;
}

bool SteamLinkGeneric::send_tr(uint8_t* payload, uint8_t payload_length) {
  uint8_t* packet;
  uint8_t packet_length;
  tr_header header;
  header.op = SL_OP_TR;
  header.slid = _slid;
  header.rssi = _last_rssi;
  packet_length = SteamLinkPacket::set_packet(packet, payload, payload_length, (uint8_t*) &header, sizeof(header));
  bool sent = driver_send(packet, packet_length, SL_DEFAULT_STORE_ADDR, false);
  free(packet);
  return sent;
}

bool SteamLinkGeneric::send_ss(uint8_t* payload, uint8_t payload_length) {
uint8_t* packet;
uint8_t packet_length;
  tr_header header;
  header.op = SL_OP_SS;
  header.slid = _slid;
  packet_length = SteamLinkPacket::set_packet(packet, payload, payload_length, (uint8_t*) &header, sizeof(header));
  bool sent = driver_send(packet, packet_length, SL_DEFAULT_STORE_ADDR, false);
  free(packet);
  return sent;
}

void SteamLinkGeneric::handle_admin_packet(uint8_t* packet, uint8_t packet_length, bool is_physical) {
  uint8_t op = packet[0];
  if (op == SL_OP_DN) {          // CONTROL PACKETS
    INFONL("DN Packet Received");
    dn_header header;
    uint8_t* payload;
    uint8_t payload_length = SteamLinkPacket::get_packet(packet, packet_length, payload, (uint8_t*) &header, sizeof(header));
    // TODO Suppress Duplicates
    if (_on_receive != NULL) {
      _on_receive(payload, payload_length);
    }
    free(payload);

  } else if (op == SL_OP_BN) {
    INFONL("BN Packet Received");
    bn_header header;
    uint8_t* payload;
    uint8_t payload_length = SteamLinkPacket::get_packet(packet, packet_length, payload, (uint8_t*) &header, sizeof(header));
    // if possible, send it up to the bridge level
    if (is_physical) {
      _bridge_handler(payload, payload_length, header.slid);
    } else {
      // we need to forward it out the physical layer
      driver_send(payload, payload_length, header.slid, false);
    }
    free(payload);

  } else if (op == SL_OP_GS) {
    INFONL("GetStatus Received");
	send_on();

  } else if (op == SL_OP_SR) {
    INFONL("SetRadio Received");
	// TODO: actuuall set Radio
	send_ak();
  }  // TODO FINISH CONTROL PACKETS

  else if ((op % 2) == 1) {     // we've received a DATA PACKET
    // build encapsulated packet and pass it up
    if (is_physical) {
      if (_bridge_handler != NULL) {
        uint8_t* enc_packet;
        uint8_t enc_packet_length;
        bs_header header;
        header.op = SL_OP_BS;
        header.slid = _slid;
        header.rssi = _last_rssi;
        header.qos = SL_DEFAULT_QOS;
        enc_packet_length = SteamLinkPacket::set_packet(enc_packet, packet, packet_length, (uint8_t*) &header, sizeof(header));
        _bridge_handler(enc_packet, enc_packet_length, SL_DEFAULT_STORE_ADDR);
      }
    } else { // we got it from the bridge
      send_bs(packet, packet_length);
    }
  }
}

void SteamLinkGeneric::sign_on_procedure() {
  send_on();
}

void SteamLinkGeneric::set_bridge() {
  _is_bridge = true;
}

uint32_t SteamLinkGeneric::get_slid() {
  return _slid;
}
