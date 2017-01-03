#!/usr/bin/python3

import paho.mqtt.client as mqtt
import repubmqtt
import time
import sys
import json
import struct

from Crypto.Cipher import AES
import binascii



N_TYP_VER = 0
B_TYP_VER = 0

DBG = 1

class Mesh:
	def __init__(self, mid, mname):
		mesh_table[mid] = self
		self.mesh_id = mid
		self.mesh_name = mname


class Swarm:
	def __init__(self, sid, sname, skey, sowner, smesh_id):
		swarm_table[sid] = self
		self.sl_id = sid
		self.swarm_name = sname
		self.swarm_key = skey
		self.swarm_mesh_id = smesh_id

		self.swarm_bkey =  bytes.fromhex(self.swarm_key)


# Simulate mongo swarm and mesh collections
mesh_table = {}
swarm_table = {}


class SLException(BaseException):
	pass


class B_typ_0:
	""" store to bridge packet format """

	sfmt = '<BBBL'
	def __init__(self, pkt=None):
		self.mesh = "nomesh"
		if pkt:
			dlen = len(pkt) - struct.calcsize(B_typ_0.sfmt)
			sfmt = "%s%is" % (B_typ_0.sfmt, dlen)
			nb_ver, self.node_id, self.rssi, self.sl_id, epayload =  struct.unpack(sfmt, pkt)
			self.n_ver = (nb_ver & 0xF0) >> 4
			self.b_ver = nb_ver & 0x0F
			if self.n_ver != N_TYP_VER or self.b_ver != B_TYP_VER:
				raise SLException("B/N type version mismatch")
			self.rssi = self.rssi - 256
			if DBG > 2:
				print("pkt self.n_ver %s self.b_ver %s self.rssi %s self.node_id %s self.sl_id %s" % \
					(self.n_ver, self.b_ver, self.rssi, self.node_id, self.sl_id ))
		else:
			pkt = ''
			self.n_ver = N_TYP_VER
			self.b_ver = B_TYP_VER
			self.rssi = -10
			self.node_id = 0
			self.sl_id = 1
			epayload = b''

		if len(epayload) > 0:
			if not self.sl_id in swarm_table:
				raise SLException("init: swarm %s not in table" % self.sl_id)
			bkey = swarm_table[self.sl_id].swarm_bkey
			self.payload = AES128_decrypt(epayload, bkey).decode().rstrip('\0')
		elif len(pkt) > 8:
			raise SLException("init: impossible payload length")
		else:
			self.payload = ''

	def setmesh(self, mesh):
		self.mesh = mesh


	def pack(self):
		""" return a binary on-wire packet """
		header = struct.pack(B_typ_0.sfmt, (self.n_ver << 4) | self.b_ver, self.node_id, self.rssi + 256, self.sl_id)

		if not self.sl_id in swarm_table:
			raise SLException("pack: swarm %s not in table" % binascii.hexlify(self.sl_id))
		bkey = swarm_table[self.sl_id].swarm_bkey
		payload = AES128_encrypt(self.payload, bkey)
		return header + payload


	def dict(self):
		return {'_mesh': self.mesh, '_node_id': self.node_id, '_rssi': self.rssi, \
			'_sl_id': self.sl_id, 'payload': self.payload }


	def json(self):
		return  json.dumps(self.dict())


	# alternate initializer for B_typ
class B_typ_0_new(B_typ_0):
	def __init__(self, node_id, sl_id, pkt):
		B_typ_0.__init__(self)
		self.node_id = node_id
		self.sl_id = sl_id
		self.payload = pkt


def hexprint(pkt):
	for c in pkt:
		print("%02x" % ord(c),)
	print()


def AES128_decrypt(pkt, bkey):
	if len(pkt) % 16 != 0:
		print("pkt len error: ", len(pkt))
		hexprint(pkt)
		return ""
	decryptor = AES.new(bkey, AES.MODE_ECB)
	return decryptor.decrypt(pkt)


def AES128_encrypt(msg, bkey):
	if len(msg) % 16 != 0:
		pmsg = msg + "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"[len(msg) % 16:]
	else:
		pmsg = msg
	encryptor = AES.new(bkey, AES.MODE_ECB)
	return bytearray(encryptor.encrypt(pmsg))


def process(client, pkt, timestamp):
	""" process a pkt received on the data topic """

	# log the pkt
	ts=time.strftime("%Y-%m-%d %H:%M:%S",time.localtime(timestamp))
	if DBG > 0: print("%s: %s" % (ts, pkt.json()))

	m = pkt.payload.split()
	if m[0] == "Button":
		state = int(m[1])
		opkt = B_typ_0_new(pkt.node_id, pkt.sl_id, "%s" % state)
		otopic = "%s/%s/control" % (conf['MQPREFIX'], pkt.mesh)
		pac = opkt.pack()
		client.publish(otopic, bytearray(pac))


# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):

	print("Connected with result code "+str(rc))
	for topic in conf['TOPICS']:
		client.subscribe(topic)
		print("Subscribing %s" % topic)


# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
	ts=time.strftime("%Y-%m-%d %H:%M:%S",time.localtime(msg.timestamp))
	topic = msg.topic.split('/')
	if len(topic) != 3 or topic[0] != conf['MQPREFIX']:
		print("bogus msg, %s: %s" % (msg.topic, msg.payload))
		return
	origin_mesh = topic[1]
	origin_topic = topic[2]
	if origin_topic == "data":
#		try:
		pkt = B_typ_0(msg.payload)
#		except Exception as e:
#			print("payload format or decrypt error: %s" % e)
#			return

		pkt.setmesh(origin_mesh)
		process(client, pkt, msg.timestamp)
		client.repub.process_message(pkt.dict())
	elif origin_topic == "status":
		status = jsonstatus(origin_mesh, msg.payload)
		print("%s: %s" % (ts, status))
	else:
		print("%s: UNKNOWN %s payload %s" % (ts, msg.topic, msg.payload))


def getstatus(client, mesh):
	topic = "%s/%s/state" % (conf['MQPREFIX'], mesh)
	client.publish(topic,"state" )

def jsonstatus(mesh, payload):
	sfields = ['status', 'slsent', 'slreceived', 'mqttsent', 'mqttreceived', 'mqttqcount', 'loraqcount']
	status =  payload.decode("utf-8").strip('\x00').split('/')
	if len(status) != len(sfields):
		return json.dumps({'status': payload, 'from_mesh': mesh})
	else:
		res = {}
		for i in range(len(status)):
			res[sfields[i]] = status[i]
		res['_sl_id'] = 1
		return json.dumps(res)

#
# Main
#
conf = {}
DBG = 0
REQUIRED_NAMES = ['MQTT_SERVER', 'MQTT_PORT', 'MQTT_USERNAME', \
	'MQTT_PASSWORD', 'MQTT_CLIENTID', 'TOPICS', 'RULES', 'xlate', \
    'MQPREFIX', 'POLLINTERVAL']

def main():
	if len(sys.argv) != 2:
		print("usage: %s <conffile>")
		return 1

	conffile = sys.argv[1]
	conf['DBG'] = 0
	conf['Mesh'] = Mesh
	conf['Swarm'] = Swarm
	try:
		exec(open(conffile).read(), conf )
	except Exception as e:
		print("Load of config failed: %s" % e)
		return(1)

	err = False
	for name in REQUIRED_NAMES:
		if not name in conf:
			print("required entry '%s' missing in config file '%s'" % (name, conffile))
			err = True
	if err:
		sys.exit(1)
	DBG = conf['DBG']

	client = mqtt.Client(client_id=conf['MQTT_CLIENTID'])
	client.tls_set("/home/steamlink/auth/mqtt/ca.crt")
	client.username_pw_set(conf['MQTT_USERNAME'], conf['MQTT_PASSWORD'])
	client.tls_insecure_set(False)
	client.on_connect = on_connect
	client.on_message = on_message

	repub = repubmqtt.Republish(conf['RULES'], client, conf['xlate'], conf['DBG'])
	client.repub = repub
	client.connect(conf['MQTT_SERVER'], conf['MQTT_PORT'], 60)

	# starting mqtt processing thread
#	client.loop_start()

	time.sleep(1)
	interval = conf['POLLINTERVAL']
	now = time.time() - interval	# force a get status
	rc = 0
	while 1:
		waitt = now + interval - time.time()
		if waitt < 0:
			waitt = 0
		rc = client.loop(timeout=waitt)
		if rc == mqtt.MQTT_ERR_UNKNOWN:		# like: KeyboardInterrupt
			rc = 1
			break
		if waitt == 0:
			now = time.time()
			for mid in mesh_table:
				getstatus(client, mesh_table[mid].mesh_name)

	print("loop exit")
#	client.loop_stop()
	return rc;


rc = main()
sys.exit(rc)
