#!/usr/bin/python3

from gevent import monkey
monkey.patch_all()

import logging
import logging.handlers
from threading import Thread, Event
import paho.mqtt.client as mqtt
import repubmqtt
import signal
import json
import struct
import queue
import time
import cProfile
import os
import sys
import traceback
import pprint
import hashlib
import socket
import io
import inspect
import argparse

from pymongo import MongoClient
from bson.json_util import dumps
from bson.json_util import loads
from bson.objectid import ObjectId
import steamengine

DBG = 0

# use safe eval function
from ast import literal_eval as eval
# pycrypto
from Crypto.Cipher import AES


N_TYP_VER = 0
B_TYP_VER = 0

REQUIRED_NAMES = ['MQTT_SERVER', 'MQTT_PORT', 'MQTT_USERNAME', \
	'MQTT_PASSWORD', 'MQTT_CLIENTID', 'RULES', 'XLATE', \
	'pollinterval', \
	'my_client_id', 'services', 'default_clients', \
	'MONGO_URL', 'MONGO_DB', 'PIDFILE']

class SEngine(steamengine.SteamEngine):

	def __init__(self, conf, logger):
		super(SEngine, self).__init__(conf, logger)

		# cached DB
		self.meshes = Meshes(self)
		self.swarms = Swarms(self)
		self.nodes = Nodes(self)

	def get_available_services(self):
		# find available channels by searching all classes for a 'type' class variable
		# with a contect of server or channel
		avail_services = {}
		for name, obj in inspect.getmembers(sys.modules[__name__]):
			if inspect.isclass(obj) and getattr(obj, "type", None) in ['server', 'channel']:
				avail_services[name] = obj
		return avail_services


	def write_stats_data(self, what, ddata=None):
		#N.B. no log or dbprint in this function, for now!!
		data = []
		if what == 'mesh':
			for mname in self.meshes.all():
				data.append(self.meshes.by_name(mname).reportstatus())
		elif what == 'node':
			for slid in self.nodes.all():
				d = self.nodes.by_sl_id(slid).reportstatus()
				if self.conf.get('brief_status', False):
					d['payload'] = "***"
				data.append(d)
		elif what == 'log':
			data = ddata

		topic = "SL/%s/stats" % what
		self.publish(topic, data, retain=True)

		# TODO:  remove when eve reads mqtt stats channel
		sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
		stats_socket = '/tmp/eve_stats_socket'
		if DBG >= 3: logger.debug('write_stats_data: connecting to %s' % stats_socket)
		try:
			sock.connect(stats_socket)
		except socket.error as msg:
			logger.warn('stats data not sent, cause: %s' % msg)
			return

		message = bytes("%s|%s" % (what, json.dumps(data)), 'UTF-8')
		if DBG >= 3: logger.debug('write_stats_data: writing %s' % message)
		sock.sendall(message)


#
# PingService
#
class PingService(steamengine.Service):
	type = "server"

	def process(self, pkt):
		""" we are echoing the msg"""
		pkt['ping'] = 'pong'
		return pkt

#
# DBService
#
class DBService(steamengine.Service):
	type = "server"

	def __init__(self, service_id, engine, logger, sv_config):
		super(DBService, self).__init__(service_id, engine, logger, sv_config)
		self.mongourl = sv_config['mongourl']
		self.mongodatabase = sv_config['mongodb']
		self.mongodb = None
		if not sv_config.get('lazyopen', True):
			self.startmongo()
		self.collections = {}

	functions = ['insert', 'find', 'find_one' ]


	def process(self, pkt):
		""" we are echoing the msg"""
		res = {}
		if not pkt['func'] in self.functions:
			res['res'] = Err('NOTFOUND', 'function %s not defined' % pkt['func']).d()
		elif pkt['func'] == 'insert':
			res['res'] = self.insert(pkt['collection'], pkt['data'])
		elif pkt['func'] == 'find':
			res['res'] = self.find(pkt['collection'], pkt['what'])
		elif pkt['func'] == 'find_one':
			if DBG > 2: self.logger.debug("self.find_one('%s', %s)", pkt['collection'], pkt['what'])
			res['res'] = self.find_one(pkt['collection'], pkt['what'])
		return res


	def startmongo(self):
		if not self.mongodb:
			self.mongodb = MongoClient(self.mongourl)[self.mongodatabase]
			self.logger.info("mongodb '%s' opened", self.mongodatabase)


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
		if '_id' in what and type(what['_id']) == type({}):
			what['_id'] = ObjectId(what['_id']['$oid'])
		res = tb.find_one(what)
		return res


#
# RepubChannel
#
class RepubChannel(steamengine.Service):
	type = "channel"

	def __init__(self, service_id, engine, logger, sv_config=None):
		super(RepubChannel, self).__init__(service_id, engine, logger, sv_config)
		self.repub = repubmqtt.Republish(conf['RULES'], conf['XLATE'], '')
		self.repub.setdebuglevel(DBG)
		self.repub.register_publish_protocol('mongo', publish_mongo)
		self.repub.register_publish_protocol('mqtt', publish_mqtt)


	# Process SteamLink/+/+ messages
	def process(self, msg):
		topic_parts = msg.topic.split('/')
		if topic_parts[2] == "data":
			pkt = loads(msg.payload.decode('utf-8'))
			pkt['_topic'] = msg.topic
			self.logger.info("on_m: process_native data %s %s", msg.topic, str(pkt)[:70]+"...")
			try:
				self.repub.process_message(pkt)
			except Exception as e:
				self.logger.error("repub message error %s: %s", e, pkt, exc_info=True)
			return
		elif topic_parts[2] != "control":
			logger.error( "bogus msg, %s: %s", msg.topic, msg.payload)
			return
		try:
			pkt = json.loads(msg.payload.decode('utf-8'))
		except:
			self.logger.error("control msg to '%s' not json: '%s'", msg.topic, msg.payload)
			return
		self.logger.info("native control msg %s %s", msg.topic, str(pkt)[:70]+"...")
		try:
			opkt = B_typ_0_new(self.engine, int(topic_parts[1]), pkt['payload'], None)
		except Exception as e:
			self.logger.error( "could not build binary pkt from '%s' '%s', cause '%s'", msg.topic, pkt, e)
			return
		try:
			otopic = "%s/%s/control" % (self.sv_config['repubprefix'], opkt.mesh)
			bkey = self.engine.swarms.by_sl_id(opkt.sl_id).swarm_bkey
			pac = opkt.pack(bkey)
		except Exception as e:
			self.logger.error( "could not build packet, cause %s" % e)
			return
		if DBG >= 1: self.logger.debug("publish: %s %s", otopic, ":".join("{:02x}".format(c) for c in pac))
		self.engine.publish(otopic, bytearray(pac), as_json=False)


#
# TransportChannel
#
class TransportChannel(steamengine.Service):
	type = "channel"

	def __init__(self, service_id, engine, logger, sv_config=None):
		super(TransportChannel, self).__init__(service_id, engine, logger, sv_config)


	def poll(self):
		for mname in self.engine.meshes.all():
			topic = "%s/%s/state" % (self.sv_config['prefix'], mname)
			self.engine.publish(topic,"state" )


	def process_transport_data(self, pkg, timestamp):
		""" process a pkt received on the data topic, pubish on the 'native' topic """

		if DBG >= 1: logger.debug("process_transport_data  %s", pkt.json())
		# "native" publish
		otopic = "%s/%s/data" % (self.sv_config['native'], pkt.sl_id)
		print(pkg.dict())
		self.engine.publish(otopic, pkt.dict(), as_json=False)


	# Process SL/+/+ messages
	def process(self, msg):
		topic_parts = msg.topic.split('/')
		timestamp = time.time()		# TODO should come from msg?
		if len(topic_parts) != 3:
			self.logger.error("transport topic invalid: %s", topic_parts)
			return

		if topic_parts[2] == "data":
			try:
				pkt = B_typ_0(self.engine, msg.payload, timestamp)
			except SLException as e:
				self.logger.error("transport packet format error: %s" % e)
				return
			except Exception as e:
				self.logger.exception("transport payload format or decrypt error: ")
				return
			self.logger.info("transport data %s %s", msg.topic, str(pkt.dict())[:70]+"...")

			pkt.setmesh(topic_parts[1])
			self.engine.nodes.by_sl_id(pkt.sl_id).updatestatus(pkt)
			otopic = "%s/%s/data" % (self.sv_config['repubprefix'], pkt.sl_id)
			self.engine.publish(otopic,  pkt.dict())
			self.engine.write_stats_data('node')
			if DBG >= 3: self.logger.debug("on_message: pkt is %s", str(pkt.dict()))

		elif topic_parts[2] == "status":
			self.logger.info("transport status %s %s", msg.topic, msg.payload)
			status = self.engine.meshes.by_name(topic_parts[1]).updatestatus(msg.payload)
			self.engine.write_stats_data('mesh')
			if DBG >= 3:
				self.logger.debug("mesh status %s", self.engine.meshes.by_name(topic_parts[1]).reportstatus())
		elif topic_parts[2] in ["state", "stats", "control" ]:
			pass
		else:
			self.logger.error( "UNKNOWN %s payload %s" % (msg.topic, msg.payload))



NOMESH = "nomesh"	 # placeholder

#
# Mesh
class Mesh:
	sfields = ['status', 'slsent', 'slreceived', \
			'mqttsent', 'mqttreceived', 'mqttqcount', 'loraqcount']

	def __init__(self, oid, mname, location, rtype, rparams):
		self.mesh_oid = oid
		self.mesh_name = mname
		self.location = location
		self.radio_type = rtype
		self.radio_params = rparams

		self.lastcontact = None
		self.status = {}
		self.status["_ts"] = ""
		self.status["loraqcount"] = 0
		self.status["slreceived"] = 0
		self.status["_sl_id"] = 1
		self.status["status"] = "Offline"
		self.status["slsent"] = 0
		self.status["mqttreceived"] = 0
		self.status["mqttsent"] = 0
		self.status["mqttqcount"] = 0

	def __str__(self):
		return "Mesh name=%s type=%s" % (self.mesh_name, self.radio_type)

	def reportstatus(self):
		v = {}
		v['mesh'] = self.mesh_name
		for i in self.status:
			v[i] = self.status[i]
		return v


	def updatestatus(self, payload):
		self.lastcontact = time.time()
		status =  payload.decode("utf-8").strip('\x00').split('/')
		if len(status) != len(Mesh.sfields):
			return dumps({'status': payload, 'from_mesh': mesh})
		else:
			self.status["_ts"] = time.strftime("%Y-%m-%d %H:%M:%S: ",time.localtime())
			for i in range(len(status)):
				self.status[Mesh.sfields[i]] = status[i]


class Meshes:
	mesh_table = {}
	mesh_name_table = {}
	def __init__(self, sl):
		self.sl = sl

	def all(self):
		return Meshes.mesh_name_table.keys()


	def by_name(self, name):
		if name in Meshes.mesh_name_table:
			return Meshes.mesh_name_table[name]
		return self.getmesh({'mesh_name': name})

	def by_oid(self, oid):
		if oid in Meshes.mesh_table:
			return Meshes.mesh_table[oid]

		return self.getmesh({'_id': ObjectId(oid)})


	def getmesh(self, what):
		res = self.sl.ask_question("SteamDB", None, {'func': "find_one", 'what': what, 'collection': 'meshes'})
		if res == None or '_err' in res:
			raise SLException("no mesh %s" % what)
		try:		# mega-kludge to get mongodb ObjectIDs across json channels
			oid = res['_id']['$oid']
		except:
			oid = str(res['_id'])
		mesh = Mesh(oid, res['mesh_name'], \
					res['physical_location']['location_params'], \
					res['radio']['radio_type'], \
					res['radio']['radio_params'])
		Meshes.mesh_table[oid] = mesh
		Meshes.mesh_name_table[res['mesh_name']] = mesh
		return mesh

#
# Swarm
class Swarm:
	def __init__(self, oid, sname, skey, stype):
		self.oid = oid
		self.swarm_name = sname
		self.swarm_crypto_key = skey
		self.swarm_crypto_type = stype

		key = hashlib.sha224(self.swarm_crypto_key.encode("utf-8")).hexdigest()[:32]
		self.swarm_bkey =  bytes.fromhex(key)

	def __str__(self):
		return 'Swarm name=%s, crypto_key=%s' % (self.swarm_name, self.swarm_crypto_key)


class Swarms:
	swarm_table = {}
	swarm_sl_id_table = {}
	def __init__(self, sl):
		self.sl = sl

	def by_sl_id(self, sl_id):
		try:
			return Swarms.swarm_sl_id_table[sl_id]
		except KeyError:
			raise SLException("no swaram for sl_id %s" % sl_id)

	def by_oid(self, oid):
		if oid in Swarms.swarm_table:
			return Swarms.swarm_table[oid]
		res = self.sl.ask_question("SteamDB", None, {'func': "find_one", 'what': {'_id': ObjectId(oid)}, 'collection': 'swarms'})
		if not res or  '_err' in res:
			raise SLException("no swarm %s" % oid)
		swarm = Swarm(oid, res['swarm_name'], \
					res['swarm_crypto']['crypto_key'], \
					res['swarm_crypto']['crypto_type'])
		Swarms.swarm_table[oid] = swarm
		return swarm


#
# Node
class Node:
	def __init__(self, sid, nid, nname, mesh, swarm):
		self.sl_id = sid
		self.node_id = nid
		self.node_name = nname
		self.mesh = mesh
		self.swarm = swarm
		self.lastpkt = None

	def __str__(self):
		return "Node name=%s sl_id=%s node_id=%s" % (self.node_name,
				 self.sl_id, self.node_id)

	def updatestatus(self, pkt):
		self.lastpkt = pkt

	def reportstatus(self):
		if not self.lastpkt:
			return {}
		return self.lastpkt.dict()


class Nodes:
	node_table = {}
	def __init__(self, sl):
		self.sl = sl

	def all(self):
		return Nodes.node_table.keys()


	def by_sl_id(self, sl_id):
		if sl_id in Nodes.node_table:
			return Nodes.node_table[sl_id]

		res = self.sl.ask_question("SteamDB", None, {'func': "find_one", 'what': {'sl_id': sl_id}, 'collection': 'nodes'})
		if '_err' in res:
			raise SLException("no node %s: %s" % (sl_id, res['_err']))

		try:		# mega-kludge to get mongodb ObjectIDs across json channels
			msh = self.sl.meshes.by_oid(str(res['mesh']))
			swm = self.sl.swarms.by_oid(str(res['swarm']))
		except:
			msh = self.sl.meshes.by_oid(res['mesh']['$oid'])
			swm = self.sl.swarms.by_oid(res['swarm']['$oid'])

		node = Node(sl_id, res['node_id'], res['node_name'], msh, swm )
		Nodes.node_table[sl_id] = node
		Swarms.swarm_sl_id_table[sl_id] = swm
		return node


class SLException(BaseException):
	pass


class B_typ_0:
	""" store to bridge packet format """

	sfmt = '<BBBL'
	def __init__(self, sl, pkt=None, timestamp=None):
		self.mesh = NOMESH
		self.sl = sl
		self.logger = self.sl.logger
		if not timestamp:
			timestamp = time.time()
		self._ts = time.strftime("%Y-%m-%d %H:%M:%S",time.localtime(timestamp))
		if pkt:
			dlen = len(pkt) - struct.calcsize(B_typ_0.sfmt)
			sfmt = "%s%is" % (B_typ_0.sfmt, dlen)
			nb_ver, self.node_id, self.rssi, self.sl_id, epayload =  struct.unpack(sfmt, pkt)
			self.n_ver = (nb_ver & 0xF0) >> 4
			self.b_ver = nb_ver & 0x0F
			if self.n_ver != N_TYP_VER or self.b_ver != B_TYP_VER:
				raise SLException("B/N type version mismatch: %s %s" % (self.n_ver, self.b_ver))
			self.rssi = self.rssi - 256
			if DBG >= 3: self.logger.debug("pkt self.n_ver %s self.b_ver %s self.rssi %s self.node_id %s self.sl_id %s", \
				self.n_ver, self.b_ver, self.rssi, self.node_id, self.sl_id)
		else:
			pkt = ''
			self.n_ver = N_TYP_VER
			self.b_ver = B_TYP_VER
			self.rssi = -10
			self.node_id = 0
			self.sl_id = 1
			epayload = b''

		if len(epayload) > 0:
			if not self.sl.nodes.by_sl_id(self.sl_id):
				raise SLException("init: node %s not in table" % self.sl_id)
			bkey = self.sl.swarms.by_sl_id(self.sl_id).swarm_bkey
			dcrypted = self.AES128_decrypt(epayload, bkey)
			try:
				self.payload = dcrypted.decode().rstrip('\0')
			except:
				raise SLException("B_typ_0: wrong crypto key??")
		elif len(pkt) > 8:
			raise SLException("B_typ_0: impossible payload length")
		else:
			self.payload = ''

	def setmesh(self, mesh):
		self.mesh = mesh


	def setfields(self):
#		if not self.sl.nodes.by_sl_id(self.sl_id):
#			raise SLException("pack: node %s not in table" % self.sl_id)
		self.node_id = self.sl.nodes.by_sl_id(self.sl_id).node_id
		self.mesh = self.sl.nodes.by_sl_id(self.sl_id).mesh.mesh_name


	def pack(self, bkey):
		""" return a binary on-wire packet """

		self.setfields()
		header = struct.pack(B_typ_0.sfmt, (self.n_ver << 4) | self.b_ver, self.node_id, self.rssi + 256, self.sl_id)
		payload = self.AES128_encrypt(self.payload, bkey)
		return header + payload


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


	def json(self):
		return  dumps(self.dict())


	# alternate initializer for B_typ
class B_typ_0_new(B_typ_0):
	def __init__(self, sl, sl_id, pkt, timestamp):
		B_typ_0.__init__(self, sl, timestamp=timestamp)
		self.sl = sl
		self.logger = sl.logger
		self.sl_id = sl_id
		self.payload = pkt
		self.setfields()


# MongoDB class for publisher
class MongoDB:
	mongoclient = None
	def __init__(self, mongourl, db):
		self.mongourl = mongourl
		self.startmongo()
		self.mongodb = MongoDB.mongoclient[db]
		self.collections = {}


	def startmongo(self):
		if not MongoDB.mongoclient:
			logger.info("mongodb %s connected", self.mongourl)
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


#
# Testing
def time_a_question():
	startts = time.time()
	cnt = 2
	for i in range(cnt):
		res = Steam.ask_question("ping", None, {"ping": ""})
		if res is None:
			print("timeout!!")
	dur = ((time.time() - startts) / cnt) * 1000
	print("%0.2f per call" % dur)


#
# repubmqtt publishers
#
mongop = None
def publish_mongo(publish, output_data, record):
	global mongop

	if publish['_testmode']:
		logger.info("test: publish_mongo %s %s" % (url, output_data))
		return
	if not mongop:
		mongop = MongoDB(publish['url'], publish['db'])
	#N.B. we want to pass a dict to ..insert(..)
	logger.info( "publish_mongo %s %s" % (publish['collection'], json.loads(output_data)))
	_id = mongop.insert(publish['collection'], json.loads(output_data))
	if not _id:
		logger.error( "publish_mongo error: %s %s %s" % (_id, publish['collection'], json.loads(output_data)))
	return _id


mqttp = None	#N.B.
def publish_mqtt(publish, output_data, data):
	try:
		topic = publish['topic'] % data
	except KeyError as e:
		logger.error( "publish topic '%s' missing key '%s' in data '%s'" % (publish['topic'], e, data))
		return
	retain = publish.get('retain',False)
	if publish['_testmode']:
		logger.info("test: publish_mqtt %s %s" % (topic, output_data))
		return
	logger.info( "publish_mqtt %s %s" % (topic, output_data))
	rc, mid = mqttp.publish(topic, output_data, retain=retain)
	if rc:
		logger.error( "publish_mqtt failed %s, %s %s %s" % (rc, topic, output_data, retain))


def change_runstate(newstate):
	global run_action
	run_action = newstate
	signalevent.set()

def handler(signum, frame):
	logger.warn('received signal %s' % signum)
	if signum == signal.SIGHUP:
		change_runstate("restart")
	elif signum == signal.SIGTERM:
		change_runstate("terminate")


#
# StreamEnging wrapper
def engine(config, logger):
	global run_action, signalevent, mqttp
	signalevent = Event()
	signal.signal(signal.SIGHUP, handler)
	signal.signal(signal.SIGTERM, handler)

	err = False
	for name in REQUIRED_NAMES:
		if not name in config:
			logger.error("required entry '%s' missing in config", name)
			err = True
	if err:
		logger.error("correct config and try again")
		sys.exit(1)

	mdb = MongoDB(config['MONGO_URL'], config['MONGO_DB'])
	mongo_rules = loadmongoconfig(mdb)
	if mongo_rules:
		logger.debug("adding rule %s", pprint.pformat(mongo_rules))
		config['RULES'] += mongo_rules


	sl = SEngine(config, logger)
	mqttp = sl.mqtt_con		#N.B.
	interval = conf.get('pollinterval', 30)
	polltime = time.time() - interval	# force a get status
	change_runstate("run")
	while run_action == "run":
		waitt = max(polltime + interval - time.time(), 0)
		if waitt > 0:
			signalevent.wait(waitt)
			signalevent.clear()
		else:
			polltime = time.time()
			sl.poll_for_status()

	logger.info('now: %s', run_action)
	sl.shutdown()
	return run_action


def loadmongoconfig(mdb):

	mdb_transforms =  mdb.collection('transforms')
	mdb_filters =  mdb.collection('filters')
	mdb_selectors =  mdb.collection('selectors')
	mdb_publishers =  mdb.collection('publishers')

	rules = []
	for tr in mdb_transforms.find():
		if DBG >= 1: logger.debug("transform: %s", tr)
		if tr['active'] != 'on':
			logger.info( "transform '%s' not active" % tr['transform_name'])
			continue
		te = {'name': tr['transform_name']}
		fil = mdb_filters.find_one({'_id': tr['filter']})
		if fil == None:
			raise SLException("no filter obj %s for transform %s" % (tr['filter'], tr['transform_name']))
		te[fil['filter_name']] = eval(fil['filter_string'])
		sel = mdb_selectors.find_one({'_id': tr['selector']})
		if sel == None:
			raise SLException("no selector obj %s for transform %s" % (tr['selector'], tr['transform_name']))
		nsel = eval(sel['selector_string'])
		if len(nsel) != 0:
			te[sel['selector_name']] = eval(sel['selector_string'])
			msel = sel['selector_name']
		else:
			msel = ''

		pub = mdb_publishers.find_one({'_id': tr['publisher']})
		if pub == None:
			raise SLException("no publisher obj %s for transform %s" % (tr['publisher'], tr['transform_name']))
		te[pub['publisher_name']] = eval(pub['publisher_string'])
		te['rules'] = [[fil['filter_name'],msel, pub['publisher_name']]]
		rules.append(te)

#	mdb_nodes = mdb.collection(':

	return rules




def loadconf(conffile):

	global_env = {'__builtin__': None }
	try:
		exec(open(conffile).read(), global_env, conf )
	except Exception as e:
		print("Load of config %s failed: %s" % (conffile, e), file=sys.stderr)
		traceback.print_exc(file=sys.stderr)
		return False
	return True


def getargs():
	parser = argparse.ArgumentParser()
	parser.add_argument("-l", "--log", help="set loglevel, default is info")
	parser.add_argument("-X", "--debug", help="increase debug level",
					default=0, action="count")
	parser.add_argument("conf", help="config file to use",  default=None)
	return parser.parse_args()
#
# main
#

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

DBG = cl_args.debug if cl_args.debug > 0 else DBG


#flog_format='%(asctime)s %(threadName)-12s %(levelname)-4s: %(message)s'
#clog_format='%(threadName)-12s %(levelname)-4s: %(message)s'
flog_format='%(asctime)s %(threadName)-12s: %(message)s'
clog_format='%(threadName)-12s: %(message)s'
log_datefmt='%Y-%m-%d %H:%M:%S'
log_name = 'steamlink'

logging.basicConfig(level=loglevel, format=clog_format, datefmt=log_datefmt)
logger = logging.getLogger()

run_action = "run"
while run_action is "run":
	startts = time.time()
	conf = {}
	if os.path.exists(os.path.expanduser("~/.steamlinkrc")):
		if not loadconf(os.path.expanduser("~/.steamlinkrc")):
			sys.exit(1)

	if not loadconf(os.path.expanduser(cl_args.conf)):
		sys.exit(1)
	DBG = cl_args.debug if cl_args.debug > 0 else conf.get('DBG',0)
	if conf.get('PIDFILE', None):
		pid = os.getpid()
		open(conf['PIDFILE'],"w").write("%s" % pid)

	if conf.get('LOGFILE', None):
		filelog = logging.handlers.RotatingFileHandler(conf['LOGFILE'], maxBytes=10*1024*1024, backupCount=3)
		fileformatter = logging.Formatter(flog_format, datefmt=log_datefmt)
		filelog.setFormatter(fileformatter)
		console = logger.handlers[0]
		logger.addHandler(filelog)
		logger.removeHandler(console)
	rc = 0
	logger.info('steamlink starting')
	try:
		run_action = engine(conf, logger)
		if run_action is "restart":
			run_action = "run"
		else:
			break
	except KeyboardInterrupt:
		change_runstate("terminate")
		break
	except Exception as e:
		logger.exception( 'main exit with error %s: ' % e)
		if DBG > 0 or time.time() > (startts + 1):
			run_action = "terminate"
			logger.error( 'exit')
			rc = 4
			break
		logger.info( 'attempting restart in 5 seconds')
		time.sleep(5)

logger.info('steamlink exit, code=%s' % rc)

sys.exit(rc)
