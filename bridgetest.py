#!/usr/bin/python

import paho.mqtt.client as mqtt
import time
import sys
import json

from Crypto.Cipher import AES
import binascii

key = b'2b7e151628aed2a6abf7158809cf4f3c'
sl_id = binascii.unhexlify("00000099")
bkey = binascii.unhexlify(key)


MQPREFIX = "SL"
bridgetopics = [MQPREFIX+"/+/data", MQPREFIX+"/+/status"]

N_TYP_VER = 0
B_TYP_VER = 0

class SLException(BaseException):
	pass

class B_typ_0:
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
	if len(pkt) >= 23:
		self.payload = AES128_decrypt(pkt[7:]).rstrip('\0')
	else:
		self.payload = ''

  def new(self, node_id, sl_id, pkt):
	self.__init__()
	self.node_id = node_id
	self.sl_id = sl_id 
	self.payload = AES128_encrypt(pkt)


  def pack(self):
	header = bytearray("%c%c%c%c%c%c%c" % ((self.n_ver << 4) | self.b_ver, self.node_id, self.rssi + 256, self.sl_id[0], self.sl_id[1], self.sl_id[2], self.sl_id[4]))
	print header
	return header + self.payload


  def dict(self):
	return {'node_id': self.node_id, 'rssi': self.rssi, \
		  'sl_id': self.sl_id, 'payload': self.payload }

  def json(self):
	return  json.dumps(self.dict())


class B_typ_0_new(B_typ_0):
  def __init__(self, node_id, sl_id, pkt):
	B_typ_0.__init__(self)
	self.node_id = node_id
	self.sl_id = sl_id 	#??
	self.payload = AES128_encrypt(pkt)


def hexprint(pkt):
	for c in pkt:
		print "%02x" % ord(c),
	print

def AES128_decrypt(pkt):

	if len(pkt) % 16 != 0:
		print "pkt len error: ", len(pkt)
		hexprint(pkt)
		return ""
	decryptor = AES.new(bkey, AES.MODE_ECB)
	return decryptor.decrypt(pkt)

def AES128_encrypt(msg):
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
