#include <SteamLinkGeneric.h>

SteamLinkGeneric::SteamLinkGeneric(SL_NodeCfgStruct *config) : sendQ(SENDQSIZE) {
	_config = config;
	_slid = _config->slid;
	_encrypted = false;
	_key = NULL;
	_pkt_count_data = 1;		// don't use pkt_count 0
	_pkt_count_control = 1;		// don't use pkt_count 0
}

void SteamLinkGeneric::init(void *conf, uint8_t config_length) {
}

bool SteamLinkGeneric::send(uint8_t* buf) {
	return send_data(SL_OP_DS, buf, strlen((char*) buf) + 1);
}

#define UPDATE_INTERVAL 30000
uint32_t  last_update_time = 0;

void SteamLinkGeneric::update() {
	
	// Sign on
	if (!sign_on_complete) {
		sign_on_procedure();
		sign_on_complete = true;
		last_update_time = millis();
	}
	uint8_t* packet;
	uint8_t packet_length;
	uint32_t slid;
	bool received = driver_receive(packet, packet_length, slid);
	
	if (received) { // RECEIVED NEW PACKET ON THE PHY
		if (slid != SL_DEFAULT_TEST_ADDR) {
			// Determine if we are data or control?
			if ((packet[0] & 0x01) == 0x01) { // DATA
				INFO("SteamLinkGeneric:: update():: Data packet received ");
				if (_bridge_mode != unbridged) { // CAN BRIDGE
					handle_admin_packet(packet, packet_length);
				} else { // CANNOT BRIDGE
					WARN("Unable to bridge data packet addressed to me: ");
					WARNPHEX(packet, packet_length);
				}
			} else { // CONTROL PACKETS
				// Determine if the packet is for me
				if (slid == _slid) { // MY PACKET
					handle_admin_packet(packet, packet_length);
				} else { // NOT MY PACKET
					if (_bridge_mode != unbridged) { // CAN BRIDGE
						handle_admin_packet(packet, packet_length);
					} else { // CANNOT BRIDGE
						WARN("Unable to bridge data packet addressed to me: ");
						WARNPHEX(packet, packet_length);
					}
				}
			}
		} else { // Tell the store we saw a test packet
			send_tr(packet, packet_length);
		}
	}

	if (sendQ.queuelevel() && driver_can_send()) {
		// TODO is driver_can_send functioning properly?
		INFONL("SteamLinkGeneric::update: about to dequeue");
		packet = sendQ.dequeue(&packet_length, &slid);
		if (!driver_send(packet, packet_length, slid)) {
			WARNNL("SteamLinkGeneric::update driver_send dropping packet!!");
		}
		INFO("SteamLinkGeneric::update send free: "); Serial.println((unsigned int)packet, HEX);
		last_update_time = millis();
		free(packet);
	}
	if (millis() > (last_update_time + UPDATE_INTERVAL)) {
		last_update_time = millis();
	send_ss("OK");
	}
}

void SteamLinkGeneric::register_receive_handler(on_receive_handler_function on_receive) {
	_on_receive = on_receive;
}


void SteamLinkGeneric::register_bridge_handler(on_receive_bridge_handler_function on_receive) {
	_bridge_handler = on_receive;

}

bool SteamLinkGeneric::driver_send(uint8_t* packet, uint8_t packet_size, uint32_t slid) {
	return false;
}

bool SteamLinkGeneric::driver_can_send() {
	return true;
}

bool SteamLinkGeneric::send_enqueue(uint8_t* packet, uint8_t packet_length, uint32_t slid) {
	if (sendQ.enqueue(packet, packet_length, slid) == 0) {
		WARNNL("SteamLinkGeneric::send_enqueue: WARN: sendQ FULL, pkt dropped");
	}
}

bool SteamLinkGeneric::driver_receive(uint8_t* &packet, uint8_t &packet_size, uint32_t &slid) {
	return false;
}

bool SteamLinkGeneric::send_data(uint8_t op, uint8_t* payload, uint8_t payload_length) {
	uint8_t* packet;
	uint8_t packet_length = payload_length + sizeof(data_header);
	INFONL("SteamLinkGeneric::send_data packet: ");
	packet = (uint8_t*) malloc(packet_length);
	data_header *header = (data_header *)packet;
	header->op = op;
	header->slid = _slid;
	header->pkg_num = _pkt_count_data++;
	if (_pkt_count_data == 0) {		// skip 0
		_pkt_count_data++;
	}
	header->rssi = _last_rssi;
	header->qos = SL_DEFAULT_QOS;
	if (payload_length > 0) {
		memcpy(&packet[sizeof(data_header)], payload, payload_length);
	}
	INFOPHEX(packet, packet_length);
	bool sent = generic_send(packet, packet_length, SL_DEFAULT_STORE_ADDR);
	return sent;
}

bool SteamLinkGeneric::send_td(uint8_t *td, uint8_t len) {
	uint8_t* packet = (uint8_t *)malloc(len);
	INFO("SteamLinkGeneric::send_td malloc: "); Serial.println((unsigned int)packet, HEX);
	memcpy(packet, td, len);
	uint8_t packet_length;
	INFONL("SteamLinkGeneric::send_td packet: ");
	INFOPHEX(packet, len);
	bool sent = send_enqueue(packet, len, SL_DEFAULT_TEST_ADDR);  // update will free this
	return sent;
}

bool SteamLinkGeneric::send_on() {
	INFONL("SteamLinkGeneric::send_on packet: ");
	bool sent = send_data(SL_OP_ON, (uint8_t *)_config, sizeof(*_config));
	return sent;
}

bool SteamLinkGeneric::send_ak() {
	INFONL("SteamLinkGeneric::send_ak packet: ");
	bool sent = send_data(SL_OP_AK, NULL, 0);
	return sent;
}

bool SteamLinkGeneric::send_nk() {
	INFONL("SteamLinkGeneric::send_nk packet: ");
	bool sent = send_data(SL_OP_NK, NULL, 0);
	return sent;
}

bool SteamLinkGeneric::send_tr(uint8_t* payload, uint8_t payload_length) {
	INFONL("SteamLinkGeneric::send_tr packet: ");
	bool sent = send_data(SL_OP_TR, payload, payload_length);
	return sent;
}

bool SteamLinkGeneric::send_ss(char* status) {
	INFONL("SteamLinkGeneric::send_ss packet: ");
	bool sent = send_data(SL_OP_SS, (uint8_t *) status, strlen(status)+1);
	return sent;
}

void SteamLinkGeneric::handle_admin_packet(uint8_t* packet, uint8_t packet_length) {
	uint8_t op = packet[0];
	if (op == SL_OP_DN) {          // CONTROL PACKETS
		INFONL("DN Packet Received");
		uint8_t* payload = packet + sizeof(control_header);
		uint8_t payload_length = packet_length - sizeof(control_header);
		memcpy(packet, payload, payload_length);    // move payload to start of allc'd pkt
		if (_on_receive != NULL) {
			_on_receive(packet, payload_length);
		}

	} else if (op == SL_OP_BN) {
		INFONL("BN Packet Received");
		uint8_t* payload = packet + sizeof(control_header);
		uint8_t payload_length = packet_length - sizeof(control_header);
		uint32_t to_slid = ((control_header*) packet)->slid;
		memcpy(packet, payload, payload_length);    // move payload to start of allc'd pkt
		generic_send(packet, payload_length, to_slid);

	} else if (op == SL_OP_GS) {
		INFONL("GetStatus Received");
		send_ss("OK");

	} else if (op == SL_OP_TD) {
		INFONL("Transmit Test Received");
		send_ak();
		uint8_t* payload = packet + sizeof(control_header);
		uint8_t payload_length = packet_length - sizeof(control_header);
		memcpy(packet, payload, payload_length);    // move payload to start of allc'd pkt
		send_td(packet, payload_length);

	} else if (op == SL_OP_SR) {
		send_ak();
		INFONL("SetRadio Received");
		uint8_t* payload = packet + sizeof(control_header);
		uint8_t payload_length = packet_length - sizeof(control_header);
		memcpy(packet, payload, payload_length);    // move payload to start of allc'd pkt
		INFONL("Passing payload as config to init");
		init(packet, payload_length);

	} else if (op == SL_OP_BC) {
		INFONL("BootCold Received");
		while(1);    // watchdog will reset us

	} else if (op == SL_OP_BR) {
		INFONL("BootReset Received");
		// TODO: actually reset the radio
		send_ak();

	}
	else if ((op & 0x1) == 1) {     // we've received a DATA PACKET
		send_data(SL_OP_BS, packet, packet_length);
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
			rc = send_enqueue(packet, packet_length, slid);
		} else if ( _bridge_mode == nodeside  ) {
			_bridge_handler(packet, packet_length, slid);
		} else if ( _bridge_mode == unbridged ) {
			INFONL("sending_enqueue");
			rc = send_enqueue(packet, packet_length, slid);
		}
	} else { // CONTROL
		if ( _bridge_mode == nodeside  ) {
			rc = send_enqueue(packet, packet_length, slid);
		} else if ( _bridge_mode == unbridged ) {
			WARNNL("Sending a control packet as an unbridged node");
			rc = send_enqueue(packet, packet_length, slid); // TODO: is this even a valid case?
		} else  if ( _bridge_mode == storeside ) {
			_bridge_handler(packet, packet_length, slid);
		}
	}
	if (!rc) {
		WARNNL("SteamLinkGeneric::generic_send: send failed!!");
	}
	return rc;
}
