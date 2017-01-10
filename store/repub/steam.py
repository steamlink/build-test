#!/usr/bin/python3

import paho.mqtt.client as mqtt
from pymongo import MongoClient
import repubmqtt
import time
import os
import sys
import json
import struct
import signal
import traceback
import pprint

from Crypto.Cipher import AES



N_TYP_VER = 0
B_TYP_VER = 0

DBG = 1

class Mesh:
	sfields = ['status', 'slsent', 'slreceived', \
			'mqttsent', 'mqttreceived', 'mqttqcount', 'loraqcount']
	def __init__(self, mid, mname):
		mesh_table[mname] = self
		self.mesh_id = mid
		self.mesh_name = mname
		self.lastcontact = None
		self.status = {}
		self.status["loraqcount"] = 0
		self.status["slreceived"] = 0
		self.status["_sl_id"] = 1
		self.status["status"] = "Offline"
		self.status["slsent"] = 0
		self.status["mqttreceived"] = 0
		self.status["mqttsent"] = 0
		self.status["mqttqcount"] = 0

	def reportstatus(self):
		v = {}
		v['mesh'] = self.mesh_name
		for i in self.status:
			v[i] = self.status[i]
		return json.dumps(v)


	def updatestatus(self, payload):
		self.lastcontact = time.time()
		status =  payload.decode("utf-8").strip('\x00').split('/')
		if len(status) != len(Mesh.sfields):
			return json.dumps({'status': payload, 'from_mesh': mesh})
		else:
			for i in range(len(status)):
				self.status[Mesh.sfields[i]] = status[i]


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


# use the libray's log function
log = repubmqtt.log

def dbgprint(lvl, *args, **kwargs):
	if DBG >= lvl: log('dbg'+str(lvl), *args, **kwargs)


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
			dbgprint(3, "pkt self.n_ver %s self.b_ver %s self.rssi %s self.node_id %s self.sl_id %s" % \
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
			raise SLException("pack: swarm %s not in table" % self.sl_id)
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


class MongoDB:
	mongoclient = None
	def __init__(self, mongourl, db):
		self.mongourl = mongourl
		self.startmongo()
		self.mongodb = MongoDB.mongoclient[db]
		self.collections = {}


	def startmongo(self):
		if not MongoDB.mongoclient:
			MongoDB.mongoclient = MongoClient(self.mongourl)


	def collection(self, collname):
		if not self.mongodb:
			self.startmongo()
		self.collections[collname] = self.mongodb[collname]
		return self.collections[collname]
		

	def insert(self, collection, item):
		if not collection in self.collections:
			self.collection(collection)
		tb = self.collections[collection] 
		_id = tb.insert_one(item).inserted_id
		return _id


	def find(self, collection, what=None):
		if not collection in self.collections:
			self.collection(collection)
		tb = self.collections[collection] 
		res = tb.find(what)
		return res


	def find_one(self, collection, what):
		if not collection in self.collections:
			self.collection(collection)
		tb = self.collections[collection] 
		res = tb.find_one(what)
		return res


mongop = None
def publish_mongo(publish, output_data, record):
	global mongop

	if publish['_testmode']:
		log('test', "publish_mongo %s %s" % (url, output_data))
	if not mongop:
		mongop = MongoDB(publish['url'], publish['db'])
	#N.B. we want to pass a dict to ..insert(..)
	_id = mongop.insert(publish['collection'], json.loads(output_data))
	return _id
	

def publish_mqtt(publish, output_data, data):
	topic = publish['topic'] % data
	retain = publish.get('retain',False)
	if publish['_testmode']:
		log('test', "publish_mqtt %s %s" % (topic, output_data))
		return
	client.publish(topic, output_data, retain=retain)


def AES128_decrypt(pkt, bkey):
	if len(pkt) % 16 != 0:
		log('error', "decrypt pkt len (%s)error: %s" % \
			(len(pkt)), " ".join("{:02x}".format(ord(c)) for c in pkt))
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
	dbgprint(1, "%s: %s" % (ts, pkt.json()))
	# "native" publish
	otopic = "%s/%s/data" % (conf['SL_NATIVE_PREFIX'], pkt.sl_id)
	client.publish(otopic,  pkt.json())


# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):

	log('notice',"connected to MQTT boker,code "+str(rc))
	for topic in conf['TOPICS']:
		client.subscribe(topic)
		log('notice', "subscribe %s" % topic)


# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
	ts=time.strftime("%Y-%m-%d %H:%M:%S",time.localtime(msg.timestamp))
	topic = msg.topic.split('/')
	if len(topic) != 3 or not topic[0] in [conf['SL_TRANSPORT_PREFIX'], conf['SL_NATIVE_PREFIX']]:
		log('error', "bogus msg, %s: %s" % (msg.topic, msg.payload))
		return
	origin_mesh = topic[1]
	origin_topic = topic[2]

	if topic[0] == conf['SL_NATIVE_PREFIX']:
		if origin_topic == "data":
			pkt = json.loads(msg.payload.decode('utf-8'))
			pkt['topic'] = msg.topic
			userdata.process_message(pkt)
		elif origin_topic != "control":
			log('error', "bogus msg, %s: %s" % (msg.topic, msg.payload))
			return
		try:
			pkt = json.loads(msg.payload.decode('utf-8'))
		except:
			log('error', "control msg to '%s' not json: '%s'" % (msg.topic, msg.payload))
			return

		try:
			opkt = B_typ_0_new(pkt['_node_id'], int(topic[1]), pkt['payload'])
		except Exception as e:
			log('error', "could not build binary pkt from '%s' '%s', cause '%s'" % (msg.topic, msg.payload, e))
			return
		try:
			otopic = "%s/mesh_%s/control" % (conf['SL_TRANSPORT_PREFIX'], pkt['_mesh'])
			pac = opkt.pack()
		except Exception as e:
			log('error', "could not build packet, cause %s" % e)
			return
		client.publish(otopic, bytearray(pac))

	elif topic[0] == conf['SL_TRANSPORT_PREFIX']:
		if origin_topic == "data":
			try:
				pkt = B_typ_0(msg.payload)
			except SLException as e:
				log('error', "packet format error: %s" % e)
				return
			except Exception as e:
				log('error', "payload format or decrypt error: %s" % e)
				return

	
			pkt.setmesh(origin_mesh)
			process(client, pkt, msg.timestamp)
			dbgprint(1, "on_message: pkg is %s" % str(pkt.dict()))
		elif origin_topic == "status":
			status = mesh_table[origin_mesh].updatestatus(msg.payload)
			log('notice', "%s" % (mesh_table[origin_mesh].reportstatus()))
		else:
			log('error', "UNKNOWN %s payload %s" % (msg.topic, msg.payload))


def getstatus(client, mesh):
	topic = "%s/%s/state" % (conf['SL_TRANSPORT_PREFIX'], mesh)
	client.publish(topic,"state" )


sigseen = None
def handler(signum, frame):
	global sigseen 
	log('notice', 'Signal handler called with signal %s' % signum)
	sigseen = signum

#
# Main
#
conf = {}

REQUIRED_NAMES = ['MQTT_SERVER', 'MQTT_PORT', 'MQTT_USERNAME', \
	'MQTT_PASSWORD', 'MQTT_CLIENTID', 'RULES', 'XLATE', \
	'SL_TRANSPORT_PREFIX', 'SL_NATIVE_PREFIX', 'POLLINTERVAL', \
	'MONGO_URL', 'MONGO_DB', 'PIDFILE']


def loadmongoconfig(mongourl, mongodb):
	dbgprint(1, "loadmongoconfig(%s, %s)" % (mongourl, mongodb))

	mdb = MongoDB(mongourl, mongodb)
	mdb_transforms =  mdb.collection('transforms')
	mdb_filters =  mdb.collection('filters')
	mdb_selectors =  mdb.collection('selectors')
	mdb_publishers =  mdb.collection('publishers')

	rules = []
	for tr in mdb_transforms.find():
		dbgprint(2, "tranform: ", tr)
		te = {'name': tr['transform_name']}
		fil = mdb_filters.find_one({'_id': tr['filter']})
		te[fil['filter_name']] = fil['filter_string']
		sel = mdb_selectors.find_one({'_id': tr['selector']})
		te[sel['selector_name']] = sel['selector_string']
		pub = mdb_publishers.find_one({'_id': tr['publisher']})
		te[pub['publisher_name']] = pub['publisher_string']
		te['rules'] = [[fil['filter_string'],sel['selector_string'], pub['publisher_string']]]
		pprint.pprint(te)
		rules.append(te)

iXXXX	mdb_nodes = mdb.collection(':

	return rules


def loadconf(conffile):
	conf = {}
	conf['DBG'] = 0
	# Temportary
	conf['Mesh'] = Mesh
	conf['Swarm'] = Swarm
	try:
		exec(open(conffile).read(), conf )
	except Exception as e:
		print("Load of config failed: %s" % e)
		return None

	err = False
	for name in REQUIRED_NAMES:
		if not name in conf:
			print("required entry '%s' missing in config file '%s'" % (name, conffile))
			err = True

	if err:
		return None

	conf['TOPICS'] = [conf['SL_TRANSPORT_PREFIX']+"/+/data", \
					  conf['SL_TRANSPORT_PREFIX']+"/+/status", \
					  conf['SL_NATIVE_PREFIX']+"/+/control",\
					  conf['SL_NATIVE_PREFIX']+"/+/data"]
	mongo_rules = loadmongoconfig(conf['MONGO_URL'], conf['MONGO_DB'])

	if mongo_rules:
		conf['RULES'] += mongo_rules
	print("rules: ", end="")
	pprint.pprint(conf['RULES'])
	return conf


def main():
	global client, conf, sigseen
	if len(sys.argv) != 2:
		print("usage: %s <conffile>")
		return 1

	signal.signal(signal.SIGHUP, handler)
	signal.signal(signal.SIGTERM, handler)

	running = True
	while running:
		conf = loadconf( sys.argv[1])
	
		if not conf:
			sys.exit(1)
		DBG = conf['DBG']
	
		pid = os.getpid()
		os.open(conf['PIDFILE'],"w").write("%s" % pid)

		repub = repubmqtt.Republish(conf['RULES'], conf['XLATE'])
		repub.setdebuglevel(conf['DBG'])
		repub.register_publish_protocol('mongo', publish_mongo)
		repub.register_publish_protocol('mqtt', publish_mqtt)

		# pass republish instance to mqtt Client as userdata
		client = mqtt.Client(client_id=conf['MQTT_CLIENTID'], userdata=repub)
		client.tls_set("/home/steamlink/auth/mqtt/ca.crt")
		client.username_pw_set(conf['MQTT_USERNAME'], conf['MQTT_PASSWORD'])
		client.tls_insecure_set(False)
		client.on_connect = on_connect
		client.on_message = on_message

		client.connect(conf['MQTT_SERVER'], conf['MQTT_PORT'], 60)
		interval = conf['POLLINTERVAL']
		now = time.time() - interval	# force a get status
		rc = 0
		while running:
			if sigseen:
				if sigseen == signal.SIGHUP:
					log('notice', 'restart on SIGHUP')
					sigseen = None
					break
				if sigseen == signal.SIGTERM:
					log('notice', 'restart on SIGTERM')
					sigseen = None
					running = False
					break
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
		client.disconnect()
	
	print("loop exit")
	return rc;


#
# main
#

startts = time.time()
while True:
	try:
		rc = main()
		break
	except KeyboardInterrupt:
		rc = 1
		break
	except Exception as e:
		log('error', 'main exit with error %s' % e)
		traceback.print_exc(file=sys.stderr)
		if DBG > 0 or time.time() > (startts + 1):
			log('error', 'exit')
			rc = 4
			break
		log('notice', 'restarting in 5 seconds')
		time.sleep(5)

sys.exit(rc)
