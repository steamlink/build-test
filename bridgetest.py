#!/usr/bin/python

import paho.mqtt.client as mqtt
import time
import sys
import json

from Crypto.Cipher import AES
import binascii


MQPREFIX = "SL"
bridgetopics = [MQPREFIX+"/+/data", MQPREFIX+"/+/status"]

N_TYP_VER = 0
B_TYP_VER = 0


class Mesh:
  def __init__(self, mid, mname):
	mesh_table[mid] = self
	self.mesh_id = mid
	self.mesh_name = mname


class Swarm:
  def __init__(self, sid, sname, skey, sowner, smesh_id):
	swarm_table[sid] = self
	self.sl_id = binascii.unhexlify(sid)
	self.swarm_name = sname
	self.swarm_key = skey
	self.swarm_mesh_id = smesh_id

	self.swarm_bkey =  binascii.unhexlify(self.swarm_key)


# Simulate mongo swarm and mesh collections
mesh_table = {}
swarm_table = {}

Mesh(1, 'mesh_1')
Swarm('00000099', 'testnode1', b'2b7e151628aed2a6abf7158809cf4f3c0000009900c06444000300', 'andreas', 1)



class SLException(BaseException):
	pass


class B_typ_0:
  """ store to bridge packet format """
  def __init__(self, pkt=None):
	
	if not pkt:
		pkt=bytearray("%c%c%c%c" % ((N_TYP_VER << 4) | B_TYP_VER,0,0,0))
		bpkt = pkt
	elif type(pkt) == type(""):
		bpkt = bytearray(pkt)

	self.n_ver = (bpkt[0] & 0xF0) >> 4
	self.b_ver = bpkt[0] & 0x0F
	if self.n_ver != N_TYP_VER or self.b_ver != B_TYP_VER:
		raise SLException("B/N type version mismatch")
	self.node_id = bpkt[1]
	self.rssi = bpkt[2] - 256

	self.sl_id = bpkt[3:7]
	if len(pkt) >= 20:	# N.B. 4 bytes header and minimum of 16 bytes encrypted data
		if not self.sl_id in swarm_table:
			raise SLException("init: swarm %s not in table" % self.sl_id)
		bkey = swarm_table[self.sl_id].swarm_bkey
		self.payload = AES128_decrypt(pkt[4:], bkey).rstrip('\0')
	elif len(pkt) > 4:
		raise SLException("init: impossible payload length")
	else:
		self.payload = ''


  def pack(self):
	""" return a binary on-wire packet """
	header = bytearray("%c%c%c%c%c%c%c" % ((self.n_ver << 4) | self.b_ver, self.node_id, self.rssi + 256, self.sl_id[0], self.sl_id[1], self.sl_id[2], self.sl_id[4]))
	
	if not self.sl_id in swarm_table:
		raise SLException("pack: swarm %s not in table" % binascii.hexlify(self.sl_id))
	bkey = swarm_table[self.sl_id].swarm_bkey
	payload = AES128_encrypt(self.payload, bkey)
	return header + payload


  def dict(self):
	return {'node_id': self.node_id, 'rssi': self.rssi, \
		  'sl_id': binascii.hexlify(self.sl_id), 'payload': self.payload }


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
		print "%02x" % ord(c),
	print


def AES128_decrypt(pkt, bkey):
	if len(pkt) % 16 != 0:
		print "pkt len error: ", len(pkt)
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


def process(from_mesh, pkt, timestamp):
	""" process a pkt received on the data topic """

	# log the pkt
	ts=time.strftime("%Y-%m-%d %H:%M:%S",time.localtime(timestamp))
	print("%s: data %s %s" % (ts, from_mesh, pkt.json()))

	m = pkt.payload.split()
	if m[0] == "Button":
		state = int(m[1])
		opkt = B_typ_0_new(pkt.node_id, pkt.sl_id, "%s" % state)
		otopic = "%s/%s/control" % (MQPREFIX, from_mesh)
		client.publish(otopic, opkt.pack())
					

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):

	print("Connected with result code "+str(rc))
	for topic in bridgetopics:
		client.subscribe(topic)
		print "Subscribing %s" % topic


# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
	ts=time.strftime("%Y-%m-%d %H:%M:%S",time.localtime(msg.timestamp))
	topic = msg.topic.split('/')
	if len(topic) != 3 or topic[0] != MQPREFIX:
		print "bogus msg, %s: %s" % (msg.topic, msg.payload)
		return
	origin_mesh = topic[1]
	origin_topic = topic[2]
	if origin_topic == "data":
#		try:
		pkt = B_typ_0(msg.payload)
#		except Exception as e:
#			print "payload format or decrypt error: %s" % e
#			return

		process(origin_mesh, pkt, msg.timestamp)
	elif origin_topic == "status":
		print "%s: status %s %s" % (ts, origin_mesh, msg.payload)
	else:
		print("%s: UNKNOWN %s payload %s" % (ts,msg.topic, msg.payload))


def getstatus(mesh):
	topic = "%s/%s/state" % (MQPREFIX, mesh)
	client.publish(topic,"state" )
	
#
# Main
#
fullFlag = True

client = mqtt.Client(client_id="sl_store1")
# client.tls_set("/usr/local/etc/mosquitto/CA/ca.crt")
client.tls_set("/home/steamlink/auth/mqtt/ca.crt")
client.username_pw_set("andreas","1foot123")
client.on_connect = on_connect
client.on_message = on_message

client.tls_insecure_set(False)
client.connect("mqtt.steamlink.net", 8883, 60)

# starting mqtt processing thread
client.loop_start()

time.sleep(1)
getstatus("mesh_1")
while 1:
	time.sleep(30)
	getstatus("mesh_1")

print("loop exit")
client.loop_stop()
