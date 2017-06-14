#!/usr/bin/python3


# Control program to run Stealink radio tests

import sys
import logging
from threading import Thread, Event
import struct
import queue
import paho.mqtt.client as mqtt
import json
import time
import yaml
import argparse



# op codes
class SL_OP:
	'''
	admin_control message types: EVEN, 0 bottom bit 
	admin_data message types: ODD, 1 bottom bit
	'''

	DN = 0x30		# data to node, ACK for qos 2
	BN = 0x32		# slid precedes payload, bridge forward to node
	GS = 0x34		# get status, reply with SS message
	TD = 0x36		# transmit a test message via radio
	SR = 0x38		# set radio paramter to x, acknowlegde with AK or NK
	BC = 0x3A		# restart node, no reply
	BR = 0x3C		# reset the radio, acknowlegde with AK or NK

	DS = 0x31		# data to store
	BS = 0x33		# bridge to store
	ON = 0x35		# send status on to store, send on startup
	AK = 0x37		# acknowlegde the last control message
	NK = 0x39		# negative acknowlegde the last control message
	TR = 0x3B		# Received Test Data
	SS = 0x3D		# status info and counters

	def code(code):
		try:
			return list(SL_OP.__dict__.keys())[list(SL_OP.__dict__.values()).index(code)] 
		except:
			pass
		return '??'

class Mqtt(Thread):
	"""" run an MQTT connection in a thread """
	def __init__(self, conf, name):
		super(Mqtt, self).__init__()
		self.name = name
		self.conf = conf
		self.mq_connected = Event()
		for c in ["clientid", "username", "password"]:
			if not c in conf:
				logging.error("error: %s mqtt %s not specified", self.name, c)
				raise KeyError

		self.mq = mqtt.Client(client_id=self.conf["clientid"])
		if "cert" in self.conf:
			self.mq.tls_set(self.conf["cert"])
			self.mq.tls_insecure_set(False)
		self.mq.username_pw_set(self.conf["username"],self.conf["password"])
		self.mq.on_connect = self.on_connect
		self.mq.on_message = self.on_message
		self.mq.on_disconnect = self.on_disconnect
		self.running = True
		self.task = Thread(target=self.run)
		self.task.daemon = True
		self.subscription_list = []


	def run(self):
		logging.info("%s task running" % self.name)
		while self.running:
			self.mq.connect(self.conf["server"], self.conf["port"], 60)
			logger.info("%s mqtt connecting" % self.name)
			self.mq.loop_forever()


	def stop(self):
		self.running = False
		if self.mq_connected.is_set():
			self.mq.disconnect()


	def wait_connect(self):
		logging.debug("%s mqtt waiting for connect", self.name)
		self.mq_connected.wait()
		logging.info("%s mqtt got connected", self.name)


	def on_connect(self, client, userdata, flags, result):
		logging.info("%s mqtt b  connected %s", self.name, result)
		if result == 0:
			self.mq_connected.set()

			for topic in self.subscription_list:
				logging.debug("%s on_connect subscribe %s", self.name, topic)
				self.mq.subscribe(topic)


	def on_disconnect(self, client, userdata, flags):
		self.mq_connected.clear()


	def on_message(self, client, userdata, msg):
		logging.info("%s got %s  %s", self.name, msg,topic, json.loads(msg.payload.decode('utf-8')))


	def publish(self, topic, msg, qos=0, retain=False):
		logging.debug("%s publish %s %s", self.name, topic, SteamLinkPacket(pkt=msg))
		self.mq.publish(topic, payload=msg, qos=qos, retain=retain)


class GpsMqtt(Mqtt):
	def __init__(self, conf):
		self.conf = conf
		super(GpsMqtt, self).__init__(conf, "Gps")
		if not "subscription_list" in conf:
			logging.error("error: %s mqtt 'topic' not specified", self.name)
			raise KeyError
		self.subscription_list = conf["subscription_list"]

		self.gps = {'lat': 0, 'lon': 0}


	def on_message(self, client, userdata, msg):
		gpsinfo = json.loads(msg.payload.decode('utf-8'))
		self.gps = {'lon': gpsinfo['lon'], 'lat': gpsinfo['lat'], 'tst': gpsinfo['tst'] }
		logging.debug("gps update %s", str(self.gps))


	def get_gps(self):
		return self.gps


class SteamLinkMqtt(Mqtt):
	def __init__(self, conf, sl_conf,  nodes, sl_log):
		self.conf = conf
		self.sl_conf = sl_conf
		self.sl_log = sl_log
		self.nodes = nodes
		super(SteamLinkMqtt, self).__init__(conf, "SteamLink")

		self.subscription_list = [self.sl_conf.admin_data_topic]
		self.mq.message_callback_add(self.sl_conf.admin_data_topic, self.on_admin_data_msg)


	def mk_json_msg(self, msg):
		try:
			payload = msg.payload.decode('utf-8')
			jmsg = {'topic': msg.topic, 'payload': payload }
		except:
			jmsg = {'topic': msg.topic, 'raw': msg.payload }

		if DBG > 0: logging.debug("steamlink msg %s", str(jmsg))
		return jmsg


	def on_admin_data_msg(self, client, userdata, msg):
		topic_parts = msg.topic.split('/', 2)
#		sl_id = int(topic_parts[1])
		sl_pkt = SteamLinkPacket(pkt=msg.payload)
		sl_id = sl_pkt.slid
		if not sl_id in self.nodes:
			logging.warning("SteamLinkMqtt on_message sl_id 0x%0x not in nodes", sl_id)
			return
		self.nodes[sl_id].post_admin_data(sl_pkt)
				
	

class SteamLink:
	""" config data for steamlink mqtt """
	def __init__(self, conf):

		for c in ["prefix", "admin_data", "admin_control"]: # , "data", "control"]:
			if not c in conf:
				logging.error("error: %s steamlink_mqtt %s not specified", self.name, c)
				raise KeyError

		self.prefix = conf['prefix']
		self.admin_control_topic = "%s/%%s/%s" % (self.prefix, conf['admin_control'])
		self.admin_data_topic = "%s/+/%s" % (self.prefix, conf['admin_data'])


class SteamLinkPacket:

	def __init__(self, slnode = None, opcode = None, rssi = None, payload = None, pkt = None):
		self.opcode = None
		self.slid = None
		self.rssi = None
		self.qos = None
		self.pkt = None
		self.via = []		# sl_ids of route

		if pkt == None:						# construct pkt
			self.slid = slnode.sl_id
			self.opcode = opcode
			self.rssi = rssi
			self.payload = payload
			logging.debug("SteamLinkPaktet payload = %s", payload);
			if self.payload:
				if type(self.payload) == type(b''):
					bpayload = self.payload
				else:
					bpayload = self.payload.encode('utf8')
			else:
				bpayload = b''

			if opcode == SL_OP.DS:
				sfmt = '<BLB%is' % len(bpayload)
				self.pkt = struct.pack(sfmt, self.opcode, self.slid, self.qos, bpayload)
			elif opcode == SL_OP.BS:
				sfmt = '<BLBB%is' % len(bpayload)
				self.pkt = struct.pack(sfmt, self.opcode, self.slid, self.rssi, self.qos, bpayload)
			elif opcode == SL_OP.ON:
				sfmt = '<BL%is' % len(bpayload)
				self.pkt = struct.pack(sfmt, self.opcode, self.slid, bpayload)
			elif opcode in [SL_OP.AK, SL_OP.NK]:
				sfmt = '<BL' 
				self.pkt = struct.pack(sfmt, self.opcode, self.slid)
			elif opcode == SL_OP.TR:
				sfmt = '<BLB%is' % len(bpayload)
				self.pkt = struct.pack(sfmt, self.opcode, self.slid, self.rssi, bpayload)
			elif opcode == SL_OP.SS:
				sfmt = '<BL%is' % len(bpayload)
				self.opcode, self.slid, bpayload = struct.unpack(sfmt, self.pkt)
				self.payload = bpayload.decode('utf8')

			elif opcode == SL_OP.DN:
				sfmt = '<BLB%is' % len(bpayload)
				self.pkt = struct.pack(sfmt, self.opcode, self.slid, self.qos, self.rssi, bpayload)
			elif opcode in [SL_OP.GS, SL_OP.TD, SL_OP.BC, SL_OP.BR]:
				sfmt = '<B' 
				self.pkt = struct.pack(sfmt, self.opcode)
			elif opcode == SL_OP.SR:
				sfmt = '<B%is' % len(bpayload)
				self.pkt = struct.pack(sfmt, self.opcode, bpayload)

			else:
				logging.error("SteamLinkPacket unknown opcode in pkt %s", self.pkt)

			if len(slnode.via) > 0:
				for via in [self.slid]+slnode.via[::-1][:-1]:
					bpayload = self.pkt
					sfmt = '<BL%is' % len(bpayload)
					self.pkt = struct.pack(sfmt, SL_OP.BN, via, bpayload)
				self.slid = slnode.via[0]
				

			logging.debug("pkt out\n%s", "\n".join(phex(self.pkt, 4)))
			
		else:								# deconstruct pkt
			self.pkt = pkt
			logging.debug("pkt\n%s", "\n".join(phex(pkt, 4)))

			if pkt[0] == SL_OP.BS:		# un-ecap all
				while pkt[0] == SL_OP.BS:
					sfmt = '<BLBB%is' % (len(pkt) - 7)
					self.opcode, self.slid, self.rssi,  self.qos, bpayload = struct.unpack(sfmt, pkt)
					self.via.append(self.slid)
					pkt = bpayload
					logging.debug("pkg encap BS, len %s\n%s", len(pkt), "\n".join(phex(pkt, 4)))
#				self.payload = bpayload.decode('utf8')

			if pkt[0] == SL_OP.DS:
				sfmt = '<BLB%is' % (len(pkt) - 6)
				self.opcode, self.slid, self.qos, bpayload = struct.unpack(sfmt, pkt)
				self.payload = bpayload.decode('utf8')
			elif pkt[0] == SL_OP.ON:
				sfmt = '<BL%is' % (len(pkt) - 5)
				self.opcode, self.slid, bpayload = struct.unpack(sfmt, pkt)
				self.payload = bpayload.decode('utf8')
			elif pkt[0] in [SL_OP.AK, SL_OP.NK]:
				sfmt = '<BL' 
				self.opcode, self.slid = struct.unpack(sfmt, pkt)
				self.payload = None
			elif pkt[0] == SL_OP.TR:
				sfmt = '<BLB%is' % (len(pkt) - 6)
				self.opcode, self.slid, self.rssi, bpayload = struct.unpack(sfmt, pkt)
				self.payload = bpayload.decode('utf8')
			elif pkt[0] == SL_OP.SS:
				sfmt = '<BL%is' % (len(pkt) - 5)
				self.opcode, self.slid, bpayload = struct.unpack(sfmt, pkt)
				self.payload = bpayload.decode('utf8')

			elif pkt[0] == SL_OP.DN:
				sfmt = '<BLB%is' % (len(pkt) - 6)
				self.opcode, self.slid, self.qos, bpayload = struct.unpack(sfmt, pkt)
				self.payload = bpayload.decode('utf8')
			elif pkt[0] == SL_OP.BN:
				sfmt = '<BL%is' % (len(pkt) - 5)
				self.opcode, self.slid, bpayload = struct.unpack(sfmt, pkt)
				self.payload = bpayload.decode('utf8')
			elif pkt[0] in [SL_OP.GS, SL_OP.TD, SL_OP.BC, SL_OP.BR]:
				sfmt = '<B' 
				self.opcode = struct.unpack(sfmt, pkt)
				self.payload = None
			elif pkt[0] == SL_OP.SR:
				sfmt = '<B%is' % (len(pkt) - 1)
				self.opcode, bpayload = struct.unpack(sfmt, pkt)
				self.payload = bpayload.decode('utf8')
			else:
				logging.error("SteamLinkPacket unknowm opcode in pkt %s", pkt)

			

	def __str__(self):
		via = "0x%0x" % self.slid
		if len(self.via) > 0:
			for v in self.via[::-1]: via += "->0x%0x" % v
		return "SL(op %s, id %s rssi %s %s)" % (SL_OP.code(self.opcode), via,  self.rssi, self.payload)


class TestPkt:
	packet_counter = 1
	def __init__(self, gps=None, text=None, from_slid=None, pkt=None):
		self.pkt = {}
		if text != None and from_slid != None:	# construct pkt
			self.pkt['lat'] = gps['lat']
			self.pkt['lon'] = gps['lon']
			self.pkt['slid'] = from_slid
			self.pkt['pktno'] = TestPkt.packet_counter
			self.pkt['text'] = text
			self.pkt['directon'] = 'send'
			TestPkt.packet_counter += 1
		else:									# deconstruct string
			r = pkt.split('|',4)
			self.pkt['lat'] = float(r[0])
			self.pkt['lon'] = float(r[1])
			self.pkt['slid'] = int(r[2])
			self.pkt['pktno'] = int(r[3])
			self.pkt['directon'] = 'recv'
			self.pkt['text'] = r[4]
		ts = time.strftime("%Y-%m-%d_%H:%M:%S", time.localtime())
		self.pkt['ts'] = ts


	def get_pktno(self):
		return self.pkt['pktno']


	def set_rssi(self, rssu):
		self.pkt['rssi'] = rssi


	def pkt_string(self):
		return "%(lat)0.4f|%(lon)0.4f|%(slid)s|%(pktno)s|%(text)s" % self.pkt


	def json(self):
		return json.dumps(self.pkt)


class Node:
	""" a node in the test set """
	def __init__(self, sl_conf, name, sl_id, antenna, ntype, via):
		self.name = name
		self.sl_conf = sl_conf
		self.sl_id = sl_id
		self.via = via		# routing to get to this node
		self.antenna = antenna
		self.ntype = ntype
		if not ntype in ["LoRa", "ESP"]:
			logging.error("unknown node type %s for node %s", ntype, name);
			return
		self.response_q = queue.Queue(maxsize=1)
		if len(via) == 0:
			firsthop = self.sl_id
		else:
			firsthop = self.via[0]
		self.admin_control_topic = self.sl_conf.admin_control_topic % firsthop

		self.state = "DOWN"	
		self.status = []


	def set_state(self, new_state):
		if self.state != new_state:
			self.state = new_state
			logging.info("node %s state %s", self.name, self.state)
			sl_log.log_state(self.sl_id, "ONLINE" if self.state == "UP" else "offline")


	def is_up(self):
		return self.state == "UP"


	def set_brokers(self, get_gps, steamlink):
		self.get_gps = get_gps
		self.steamlink = steamlink


	def admin_send_get_status(self):
		sl_pkt = SteamLinkPacket(slnode=self, opcode=SL_OP.GS)
		self.steamlink.publish(self.admin_control_topic, sl_pkt.pkt)
		rc = self.get_response(timeout=2)
		return rc


	def admin_send_set_radio_param(self, radio):
		if self.state != "UP": return "NC"
		lorainit = struct.pack('<BLB', 0, 0, radio)
		logging.debug("admin_send_set_radio_param: len %s, pkt %s", len(lorainit), lorainit)
		sl_pkt = SteamLinkPacket(slnode=self, opcode=SL_OP.SR, payload=lorainit)
		self.steamlink.publish(self.admin_control_topic, sl_pkt.pkt)

		rc = self.get_response(timeout=2)
		return rc


	def admin_send_testpacket(self, pkt):
		if self.state != "UP": return "NC"
		sl_pkt = SteamLinkPacket(slnode=self, opcode=SL_OP.TD, payload=pkt)
		self.steamlink.publish(self.admin_control_topic, sl_pkt.pkt)
		rc = self.get_response(timeout=2)
		logging.debug("send_packet %s got %s", sl_pkt, rc)
		return rc


	def __repr__(self):
		return "%s: %s %s" % (self.name, self.sl_id, self.antenna)


	def post_admin_data(self, sl_pkt):
		""" handle incoming messages on the ../admin_data topic """
		logging.debug("post_admin_data %s", sl_pkt)

		# any pkt from node indicates it's up
		self.set_state('UP')

		opcode = sl_pkt.opcode
		opargs = sl_pkt.payload

		if opcode == SL_OP.ON:
			logging.debug('post_admin_data: slid 0x%0x ONLINE', self.sl_id)

		elif opcode == SL_OP.SS:
			logging.debug('post_admin_data: slid 0x%0x status %s', self.sl_id,opargs)
			self.status = opargs.split(',')

		elif opcode in  [SL_OP.AK, SL_OP.NK]:
			logging.debug('post_admin_data: slid 0x%0x answer %s', self.sl_id, opcode)
			try:
				self.response_q.put(opcode, block=False)
			except queue.Full:
				logging.warning('post_admin_data: node %s queue, dropping: %s', self.sl_id, sl_pkt)
		elif opcode == SL_OP.TR:
			logging.debug('post_admin_data: node %s test msg', opargs)
			rssi, jmsg = opargs.split(',',1)

			try:
				pkt = TestPkt(pkt=jmsg)
			except ValueError as e:
				logging.warning("post_incoming: cannot convert %s to pkt", jmsg)
				return
			pkt.set_rssi(rssi)
			sl_log.post_incoming(pkt)
			
	
	def get_response(self, timeout):
		try:
			data = self.response_q.get(block=True, timeout=timeout)
		except queue.Empty:
			data = "NR"
		return data
		

class LogData:
	""" Handle incoming pkts on the ../admin_data topic """
	def __init__(self, conf):
		self.conf = conf
		self.logfile = open(conf["file"],"a+")
		self.pkt_inq = queue.Queue()
		self.nodes_online = 0
		

	def log_state(self, sl_id, new_state):
		logging.debug("logdata node 0x%0x %s", sl_id, new_state)
		self.nodes_online += 1 if new_state == "ONLINE" else -1


	def post_incoming(self, pkt):
		""" a pkt arrives """
		self.log_pkt(pkt)
		self.pkt_inq.put(pkt, "recv")


	def post_outgoing(self, pkt):
		""" a pkt is sent """
		self.log_pkt(pkt, "send")


	def log_pkt(self, pkt, direction):
		self.logfile.write(pkt.json()+"\n")
		self.logfile.flush()


	def wait_pkt_number(self, pktnumber, timeout=10):
		""" wait for pkt with number pktnumber for a max of timeout seconds """
		lwait = timeout
		while True:
			now = time.time()
			try:
				pkt = self.pkt_inq.get(block=True, timeout=lwait)
			except queue.Empty:
				pkt = None
			waited = time.time() - now
			if pkt and pkt['pktno'] == pktnumber:
				return pktnumber
			if waited >= lwait or pkt['pktno'] > pktnumber:	# our pkt will never arrive
				return None
			lwait -= waited
			
#
# Utility
#

def phex(p, l=0):
	if type(p) == type(""):
		pp = p.encode()
	else:
		pp = p
	hh = ""
	cc = ""
	head = " " * l
	lines = []
	i = 0
	for c in pp:
		hh += "%02x " % c
		if c >= ord(' ') and pp[i] <= ord('~'):
			cc += chr(c)
		else:
			cc += '.'
		if i % 16 == 15:
			lines.append("%s%s %s" % (head, hh, cc))
			hh = ""
			cc = ""
		i += 1
	if cc != "":
		lines.append("%s%-48s %s" % (head, hh, cc))
	return lines


def getargs():
	parser = argparse.ArgumentParser()
	parser.add_argument("-c", "--config", help="config file, default TestControl.yaml")
	parser.add_argument("-l", "--log", help="set loglevel, default is info")
	parser.add_argument("-X", "--debug", help="increase debug level",
					default=0, action="count")
#	parser.add_argument("conf", help="config file to use",  default=None)
	return parser.parse_args()


def get_base_gps():
	gps = {'lon': conf['fixed']['location']['lon'], 'lat': conf['fixed']['location']['lat'], 'tst': 0 }
	return gps


def loadconfig(conf_fname):
	try:
		conf_f = "\n".join(open(conf_fname, "r").readlines())
		return yaml.load(conf_f)
	except Exception as e:
		print("error: config load: %s" % e)
		sys.exit(1)
	

def setup():
	steamlink.start()
	gps.start()

	gps.wait_connect()


	logging.debug("waiting for  gps location")
	while gps.gps['lat'] == 0:
		time.sleep(0.1)
	logging.info("got gps location %s", gps.gps)

	steamlink.wait_connect()

	for loc in locations:
		for node in locations[loc]['nodes']:
			if loc == 'mobile':
				nodes[node].set_brokers(gps.get_gps, steamlink)
			else:
				nodes[node].set_brokers(get_base_gps, steamlink)


def runtest():
	# establish nodes online
	nodes_defined = 0
	for loc in locations:
		for node in locations[loc]['nodes']:
			rc = nodes[node].admin_send_get_status()
			nodes_defined += 1
#			if rc != "AK":
#				logging.warning("get_status for node 0x%0x failed: %s", node, rc)
#			self.set_state('UP')

	# wait for all nodes to send their status updates
	if sl_log.nodes_online < nodes_defined:
		while sl_log.nodes_online < nodes_defined:
			print("waiting for %s node to come online" % (nodes_defined - sl_log.nodes_online), end="\r")
			time.sleep(1)

	for radio in radio_params:
		for loc in locations:
			for node in locations[loc]['nodes']:
				if not nodes[node].is_up():
					continue
				rc = nodes[node].admin_send_set_radio_param(radio)
				if rc != "AK":
					logging.warning("set_radio to %s for %s failed: %s", radio, node, rc)

		wait = int(radio_params[radio]['wait'])
		for loc in locations:
			for node in locations[loc]['nodes']:
				if not nodes[node].is_up():
					continue
				pkt = TestPkt(nodes[node].get_gps(), "TEST", nodes[node].sl_id)
				pktno = pkt.get_pktno()
				rc = nodes[node].admin_send_testpacket(pkt.pkt_string())
				if rc != "AK":
					logging.warning("send_packet for node %s failed: %s", node, rc)
				else:
					sl_log.post_outgoing(pkt)
					sl_log.wait_pkt_number(pktno, wait)

#
# Main
#
nodes = {}
nodes_by_id = {}
locations = {}
radio_param = {}

DBG = 0

cl_args = getargs()
if not cl_args.log:
	if cl_args.debug > 0:
		loglevel = logging.DEBUG
	else:
		loglevel = logging.WARN
else:
	try:
		loglevel = getattr(logging, cl_args.log.upper())
	except Exception as e:
		print("invalid logging level, use debug, info, warning, error or critical")
		sys.exit(1)

DBG = cl_args.debug 
logging.basicConfig()
logger = logging.getLogger()
logger.setLevel(loglevel)

conff = cl_args.config if cl_args.config else "TestControl.yaml"
conf = loadconfig(conff)
if DBG: print(conf)
sl_conf = SteamLink(conf['steamlink'])

for node_name in conf['nodes']:
	nconf = conf['nodes'][node_name]
	nvia = nconf.get('via', [])
	nodes_by_id[nconf['sl_id']] = nodes[node_name] = Node(sl_conf, node_name, nconf['sl_id'], nconf.get('antenna',None), nconf['type'], nvia)

logging.info("%s nodes loaded" % len(nodes))

for loc in  conf['locations']:
	locations[loc] = conf[loc]

if DBG: print(locations)

radio_params = conf['radio_params']
if DBG: print(radio_params)

sl_log = LogData(conf['logdata'])

try:
	steamlink = SteamLinkMqtt(conf['steamlink_mqtt'], sl_conf, nodes_by_id, sl_log)
	gps = GpsMqtt(conf['gps_mqtt'])
except KeyError as e:
	logging.error("Gps config key missing: %s", e)
	sys.exit(1)

try:
	setup()
except KeyboardInterrupt as e:
	print("exit")
	sys.exit(1)
except Exception as e:
	print("setup exception %s" % e)
	sys.exit(2)
 
logging.debug("starting test run")
try:
	runtest()
except KeyboardInterrupt as e:
	print("exit")
except Exception as e:
	logging.warn("test exception %s", e, exc_info=True)

logging.debug("stopping steamlink")
steamlink.stop()
logging.debug("stopping gps")
gps.stop()

logging.debug("waiting for steamlink to finish")
steamlink.join()
logging.debug("waiting for gps to finish")
gps.join()
logging.info("done")
