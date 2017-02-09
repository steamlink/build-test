import struct
import logging
import random
import traceback
import time
import sys
import os

from threading import Thread, Event
from Crypto.Cipher import AES

class SLException(BaseException):
	pass

DBG = 0

N_TYP_VER = 0
B_TYP_VER = 0

# empty transport packet
class B_typ_0:
	sfmt = '<BBBL'

	def __init__(self, logger, sl_id=None, pkt=None):
		self.logger = logger
		self.sl_id = sl_id
		self.timestamp = time.time()
		self._ts = time.strftime("%Y-%m-%d %H:%M:%S",time.localtime(self.timestamp))
		self.payload = pkt
		self.epayload = None
		self.n_ver = N_TYP_VER
		self.b_ver = B_TYP_VER
		self.rssi = -10
		self.mesh = None
		self.bkey = None
		self.node_id = None
		self.bkey = None


	def setfields(self, node_id, mesh, bkey):
		self.mesh = mesh
		self.node_id = node_id
		self.bkey = bkey
		if self.epayload:		# we have encrypted payload, decrypt
			decrypted = self.AES128_decrypt(self.epayload, self.bkey)
			try:
				self.payload = decrypted.decode().rstrip('\0')
			except:
				raise SLException("B_typ_0: node sl_id %s: wrong crypto key??" % self.sl_id)

	def unpack(self, epkt):
		dlen = len(epkt) - struct.calcsize(B_typ_0.sfmt)
		sfmt = "%s%is" % (B_typ_0.sfmt, dlen)
		nb_ver, self.node_id, rssi, self.sl_id, self.epayload = \
				 struct.unpack(sfmt, epkt)
		self.n_ver = (nb_ver & 0xF0) >> 4
		self.b_ver = nb_ver & 0x0F
		if self.n_ver != N_TYP_VER or self.b_ver != B_TYP_VER:
			raise SLException("B_typ_0: B/N type version mismatch: %s %s" % (self.n_ver, self.b_ver))
		self.rssi = rssi - 256
		if DBG >= 3: self.logger.debug("pkt self.n_ver %s self.b_ver %s self.rssi %s self.node_id %s self.sl_id %s", \
			self.n_ver, self.b_ver, self.rssi, self.node_id, self.sl_id)


	def pack(self):
		""" return a binary on-wire packet """
		if not self.node_id:
			raise SLException("B_typ_0: no node_id!")

		header = struct.pack(B_typ_0.sfmt, (self.n_ver << 4) | self.b_ver, self.node_id, self.rssi + 256, self.sl_id)
		self.epayload = self.AES128_encrypt(self.payload, self.bkey)
		return header + self.epayload


	def AES128_decrypt(self, pkt, bkey):
		if len(pkt) % 16 != 0:
			self.logger.error( "decrypt pkt len (%s)error: %s" % \
				(len(pkt)), " ".join("{:02x}".format(ord(c)) for c in pkt))
			return ""
		decryptor = AES.new(bkey, AES.MODE_ECB)
		return decryptor.decrypt(pkt)


	def AES128_encrypt(self, msg, bkey):
		if len(msg) % 16 != 0:
			pmsg = msg + "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"[len(msg) % 16:]
		else:
			pmsg = msg
		encryptor = AES.new(bkey, AES.MODE_ECB)
		return bytearray(encryptor.encrypt(pmsg))


	def dict(self):
		return {'_mesh': self.mesh, '_node_id': self.node_id, '_rssi': self.rssi, \
			'_sl_id': self.sl_id, '_ts': self._ts, 'payload': self.payload }


	def __str__(self):
		return str(self.dict())



class SLToken:
	from binascii import unhexlify
	fmt = '<16sifBB'
	def __init__(self, token_str):
		btoken = SLToken.unhexlify(token_str)
		self.key, self.sl_id, self.freq, self.mod_conf, self.node_id = \
				 struct.unpack(SLToken.fmt, btoken)

	def __str__(self):
		return "%s %s %s %s %s" % (self.key, self.sl_id, self.freq, self.mod_conf, self.node_id)


def loadconf(conffile):

	global_env = {'__builtin__': None }
	conf = {}
	try:
		exec(open(conffile).read(), global_env, conf )
	except Exception as e:
		print("Load of config %s failed: %s" % (conffile, e), file=sys.stderr)
		traceback.print_exc(file=sys.stderr)
		return None
	return conf


class MqttCon:
	import paho.mqtt.client as mqtt
	def __init__(self, logger, conf, subscriptions, on_message):
		self.logger = logger
		self.conf = conf
		self.mqtt_ready = Event()
		self.mqtt_ready.clear()
		self.subscriptions = subscriptions
		self.cb_on_message = on_message
		client_id = self.conf['MQTT_CLIENTID'] + str(int(random.uniform(1, 1000)))
		self.con = MqttCon.mqtt.Client(client_id=self.conf['MQTT_CLIENTID'])
		self.con.tls_set(self.conf['MQTT_CERT']),
		self.con.username_pw_set(self.conf['MQTT_USERNAME'], self.conf['MQTT_PASSWORD'])
		self.con.tls_insecure_set(False)
		self.con.on_connect = self.on_connect
		self.con.on_disconnect = self.on_disconnect
		self.con.on_message = self.on_message

		self.logger.info("connecting to MQTT")
		self.con.connect(self.conf['MQTT_SERVER'], self.conf['MQTT_PORT'], 60)
		self.con.loop_start()
		self.con._thread.name = "mqtt_th"  # N.B.

	def on_connect(self, client, userdata, flags, rc):
		self.logger.warn("connected to MQTT boker,code "+str(rc))
		for sub in self.subscriptions:
			self.con.subscribe(sub)
		self.mqtt_ready.set()

	def on_disconnect(self, client, userdata, flags):
		self.mqtt_ready.clear()
		self.logger.warn("disconnected from MQTT boker")

	def on_message(self, client, userdata, msg):
		return self.cb_on_message(client, userdata, msg)

	def subscribe(self, service_topic):
		self.con.subscribe(str(service_topic))

	def publish(self, topic, pkt, retain=False):
		self.mqtt_ready.wait()
		self.logger.warn("publish %s", topic)
		self.con.publish(topic, pkt, retain)

	def shutdown(self):
		self.con.disconnect()
		self.con.loop_stop()

#
# SteamLink class
class SteamLink:
	def __init__(self, token, mesh, encrypted=True, logger=None):
		if logger is None:
			logging.basicConfig()
			logger = logging.getLogger()
			logger.setLevel(logging.INFO)
		self.logger = logger
		self.token = SLToken(token)
		self.mesh = mesh
		self.encrypted = encrypted
		self.on_receive = None

		self.pubtopic = "SL/%s/data" % self.mesh
		self.subscriptions = ["SL/%s/control" % self.mesh, "SL/%s/state" % self.mesh]

		if __name__ == "__main__":
			rcfile = "~/.steamlink"
		else:
			rcfile = "~/."+os.path.basename(sys.argv[0]).rstrip('.py')+'rc'

		conffile = os.path.expanduser(rcfile)
		if not os.path.exists(conffile):
			print("config %s not found" % conffile)
			sys.exit(1)

		self.conf = loadconf(conffile)

		self.mqtt_con = MqttCon(logger, self.conf, self.subscriptions, self.on_message)
		self.mqtt_con.publish("SL/%s/status" % self.mesh, "Online/-/-/-/-/-/-")
	

	def send(self, buf, to_addr=None, len=None):
		pkt = B_typ_0(self.logger, self.token.sl_id, buf)

		pkt.setfields(self.token.node_id, self.mesh, self.token.key)

		ppkt = pkt.pack()
		self.mqtt_con.publish(self.pubtopic, ppkt)
		return 0


	def on_message(self, client, userdata, msg):
		topic_parts = msg.topic.split('/')
		if topic_parts[0] == "SL" and topic_parts[2] == "state":
			self.mqtt_con.publish("SL/%s/status" % self.mesh, "Online/-/-/-/-/-/-")
			return
		if not self.on_receive:
			self.logger.warn("no on_receive, pkt dropped")
			return
		pkt = B_typ_0(self.logger)
		pkt.unpack(msg.payload)

		if pkt.node_id != self.token.node_id:
			self.logger.error("got pkt for %s but we are %s" % (pkt.node_id,self.token.node_id))
			return
		pkt.setfields(self.token.node_id, self.mesh, self.token.key)
		self.on_receive(pkt.payload)
		
	
	def register_handler(self, on_receive, func=None):
		self.on_receive = on_receive


	def update(self):
		pass


	def set_pin(self, *args):
		pass


	def get_last_rssi(self):
		return 0;

	def shutdown(self):
		self.mqtt_con.publish("SL/%s/status" % self.mesh, "OFFLINE/-/-/-/-/-/-")
		self.mqtt_con.shutdown()
