#include <SteamLinkGeneric.h>

SteamLinkGeneric::SteamLinkGeneric(uint32_t slid) {
  _slid = slid;
  _encrypted = false;
  _key = NULL;
}

void SteamLinkGeneric::init(void *conf, uint8_t config_length) {
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
    INFO("received slid: ");
    INFONL(slid);
    if (!is_test) {
      if ((slid == _slid) || ((slid == SL_DEFAULT_STORE_ADDR) && (_bridge_mode != unbridged))) {
        INFONL("update->handle_admin_packet");
        handle_admin_packet(packet, packet_length);
      } else {
        INFONL("SteamLinkGeneric::update dropping packet 1");
	  }
    } else { // is test packet
      send_tr(packet, packet_length);
    }
	free(packet);
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
  INFONL("SteamLinkGeneric::send_ds packet: ");
  INFOPHEX(packet, packet_length);
  bool sent = generic_send(packet, packet_length, SL_DEFAULT_STORE_ADDR);
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
  INFONL("SteamLinkGeneric::send_bs packet: ");
  INFOPHEX(packet, packet_length);
  bool sent = generic_send(packet, packet_length, SL_DEFAULT_STORE_ADDR);
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
  INFONL("SteamLinkGeneric::send_on packet: ");
  INFOPHEX(packet, packet_length);
  bool sent = generic_send(packet, packet_length, SL_DEFAULT_STORE_ADDR);
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
  INFONL("SteamLinkGeneric::send_ak packet: ");
  INFOPHEX(packet, packet_length);
  bool sent = generic_send(packet, packet_length, SL_DEFAULT_STORE_ADDR);
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
  INFONL("SteamLinkGeneric::send_nk packet: ");
  INFOPHEX(packet, packet_length);
  bool sent = generic_send(packet, packet_length, SL_DEFAULT_STORE_ADDR);
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
  INFONL("SteamLinkGeneric::send_tr packet: ");
  INFOPHEX(packet, packet_length);
  bool sent = generic_send(packet, packet_length, SL_DEFAULT_STORE_ADDR);
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
  INFONL("SteamLinkGeneric::send_ss packet: ");
  INFOPHEX(packet, packet_length);
  bool sent = generic_send(packet, packet_length, SL_DEFAULT_STORE_ADDR);
  free(packet);
  return sent;
}

void SteamLinkGeneric::handle_admin_packet(uint8_t* packet, uint8_t packet_length) {
  uint8_t op = packet[0];
  if (op == SL_OP_DN) {          // CONTROL PACKETS
    INFONL("DN Packet Received");
    uint8_t* pkt_header;
    uint8_t* payload;
    uint8_t payload_length = SteamLinkPacket::get_packet(packet, packet_length, payload, pkt_header, (uint8_t) sizeof(dn_header));
    // TODO Suppress Duplicates
    INFONL("SteamLinkGeneric::handle_admin_packet DN  packet: ");
    INFOPHEX(pkt_header, packet_length);
    if (_on_receive != NULL) {
      _on_receive(payload, payload_length);
    }
    free(pkt_header);

  } else if (op == SL_OP_BN) {
    INFONL("BN Packet Received");
    uint8_t* pkt_header;
    uint8_t* payload;
    uint8_t payload_length = SteamLinkPacket::get_packet(packet, packet_length, payload, pkt_header, (uint8_t) sizeof(bn_header));
    INFONL("SteamLinkGeneric::handle_admin_packet BN  packet: ");
    INFOPHEX(pkt_header, packet_length);
    generic_send(payload, payload_length, ((bn_header*) pkt_header)->slid);
    free(pkt_header);

  } else if (op == SL_OP_GS) {
    INFONL("GetStatus Received");
//AEW    delay(100);
    send_on();

  } else if (op == SL_OP_TD) {
    INFONL("Transmit Test Received");
    uint8_t* pkt_header;
    uint8_t* payload;
    uint8_t payload_length = SteamLinkPacket::get_packet(packet, packet_length, payload, pkt_header, (uint8_t) sizeof(td_header));
//AEW   delay(100);
	send_ak();
    delay(100);
    driver_send(payload, payload_length, SL_DEFAULT_TEST_ADDR, true);

  } else if (op == SL_OP_SR) {
    INFONL("SetRadio Received");
    uint8_t* pkt_header;
    uint8_t* payload;
    uint8_t payload_length = SteamLinkPacket::get_packet(packet, packet_length, payload, pkt_header, (uint8_t) sizeof(sr_header));
//AEW   delay(100);
    send_ak();
//AEW   delay(100); // TODO: assure ack is transmitted before changing radio params
    INFONL("Passing payload as config to init");
    init(payload, payload_length);
    free(pkt_header);

  } else if (op == SL_OP_BC) {
    INFONL("BootCold Received");
    while(1);	// watchdog will reset us

  } else if (op == SL_OP_BR) {
    INFONL("BootReset Received");
	// TODO: actually reset the radio
    send_ak();

  }  
  else if ((op & 0x1) == 1) {     // we've received a DATA PACKET
    send_bs(packet, packet_length);
  }
}

void SteamLinkGeneric::sign_on_procedure() {
  send_on();
}

void SteamLinkGeneric::set_bridge(BridgeMode mode) {
  _bridge_mode = mode;
}

uint32_t SteamLinkGeneric::get_slid() {
  return _slid;
}

bool SteamLinkGeneric::generic_send(uint8_t* packet, uint8_t packet_length, uint32_t slid) {
  bool is_data = ((packet[0] & 0x1) == 1); // data or control?
  bool rc = true;

  if ( is_data ) { // DATA
    if  (_bridge_mode == storeside ) {
      rc = driver_send(packet, packet_length, slid, false);
    } else if ( _bridge_mode == nodeside  ) {
      _bridge_handler(packet, packet_length, slid);
    } else if ( _bridge_mode == unbridged ) {
      rc = driver_send(packet, packet_length, slid, false);
    }
  } else { // CONTROL
    if ( _bridge_mode == nodeside  ) {
      rc = driver_send(packet, packet_length, slid, false);
    } else if ( _bridge_mode == unbridged ) {
      WARNNL("Sending a control packet as an unbridged node");
      rc = driver_send(packet, packet_length, slid, false); // TODO: is this even a valid case?
    } else  if ( _bridge_mode == storeside ) {
      _bridge_handler(packet, packet_length, slid);
    }
  }
  if (!rc) {
    WARNNL("SteamLinkGeneric::generic_send: send failed!!");
  }
  return rc;
}
