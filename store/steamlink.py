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

from steamclient import B_typ_0, SLException

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
	'MONGO_URL', 'MONGO_DB']


class MsgLogHandler(logging.Handler):

	def __init__(self, queue):
		logging.Handler.__init__(self)
		self.queue = queue

	def enqueue(self, record):
		self.queue.put_nowait(record)

	def prepare(self, record):
		self.format(record)
		record.msg = record.message
		record.args = None
		record.exc_info = None
		return record

	def emit(self, record):
		msg = self.format(record)
		ts = time.strftime("%Y-%d-%m %H:%M:%S", time.localtime(record.created))
		omsg = {'lvl': record.levelno, '_ts': ts, 'line': msg }
		try:
			self.enqueue(omsg)
		except Exception:
			self.handleError(record)


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

	def __init__(self, service, engine, logger, sv_config):
		super(DBService, self).__init__(service, engine, logger, sv_config)

		# default database paramaters
		self.mongourl = sv_config['mongourl']
		self.mongodatabase = sv_config['mongodb']
		self.db = {'/':  MongoDB(self.mongourl, self.mongodatabase, self.logger) }


	def shutdown(self):
		self.running = False
		for db in self.db:
			self.db[db].stopmongo()
		self.engine.logger.info("Service %s shutdown" % self.service)


	functions = ['insert', 'find', 'find_one' ]


	def process(self, pkt):
		""" we are serving network requests """
		res = {}
		dbk = "%s/%s" % (pkt.get('url',''), pkt.get('db',''))
		if not dbk in self.db:
				self.db[dbk] = MongoDB(pkt['url'], pkt['db'], self.logger)
		if not pkt['func'] in self.functions:
			res['res'] = Err('NOTFOUND', 'function %s not defined' % pkt['func']).d()
		elif pkt['func'] == 'insert':
			res['res'] = self.db[dbk].insert(pkt['collection'], pkt['data'])
		elif pkt['func'] == 'find':
			res['res'] = self.db[dbk].find(pkt['collection'], pkt['what'])
		elif pkt['func'] == 'find_one':
			if DBG > 2: self.logger.debug("self.find_one('%s', %s)", pkt['collection'], pkt['what'])
			res['res'] = self.db[dbk].find_one(pkt['collection'], pkt['what'])
		return res


# MongoDB wrapper class
class MongoDB:
	def __init__(self, mongourl, db, logger):
		self.mongourl = mongourl
		self.logger = logger
		self.mongoclient = MongoClient(self.mongourl)
		self.db = db
		self.mongodb = None
		self.running = True
		self.startmongo()
		self.collections = {}

	def stopmongo(self):
		self.running = False
		self.mongoclient.close()
		self.mongoclient = None
		self.mongodb = None

	def startmongo(self):
		if not self.running:
			return
		if not self.mongodb:
			self.logger.info("mongodb %s connected", self.mongourl)
			self.mongodb = self.mongoclient[self.db]


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
		if what and '_id' in what and type(what['_id']) == type({}):
			what['_id'] = ObjectId(what['_id']['$oid'])
		res = tb.find(what)
		return res


	def find_one(self, collection, what):
		if not collection in self.collections:
			self.collection(collection)
		tb = self.collections[collection]
		if what and '_id' in what and type(what['_id']) == type({}):
			what['_id'] = ObjectId(what['_id']['$oid'])
		res = tb.find_one(what)
		return res



#
# RepubChannel
#
class RepubChannel(steamengine.Service):
	type = "channel"

	def __init__(self, service, engine, logger, sv_config=None):
		super(RepubChannel, self).__init__(service, engine, logger, sv_config)
		self.repub_rules = []


	def defered_setup(self):
		self.repub_rules = self.loadrules()
		if self.repub_rules:
			self.logger.debug("adding rules\s %s", pprint.pformat(self.repub_rules))
		else:
			self.logger.error("RepubChanned: no rules!!")
			sys.exit(1)

		self.connect_repub()


	def connect_repub(self):
		self.repub = repubmqtt.Republish(self.repub_rules, conf['XLATE'], '')
		self.repub.setdebuglevel(DBG)
		self.repub.register_publish_protocol('mongo', self.publish_mongo)
		self.repub.register_publish_protocol('mqtt', self.publish_mqtt)


	def loadrules(self):
		def calldb(collection, func, what=None):
			q = {'func': func, 'collection': collection}
			q['what'] = what
			return self.engine.ask_question("SteamDB", None, q)

		rules = []
		for tr in calldb('transforms', 'find'):
			if tr['active'] != 'on':
				self.logger.warn("transform '%s' not active", tr['transform_name'])
				continue
			te = {'name': tr['transform_name']}
			fil = calldb('filters', 'find_one', {'_id': tr['filter']})
			if fil == None:
				raise SLException("no filter obj %s for transform %s" % (tr['filter'], tr['transform_name']))
			te[fil['filter_name']] = eval(fil['filter_string'])
			sel = calldb('selectors','find_one',{'_id': tr['selector']})
			if sel == None:
				raise SLException("no selector obj %s for transform %s" % (tr['selector'], tr['transform_name']))
			nsel = eval(sel['selector_string'])
			if len(nsel) != 0:
				te[sel['selector_name']] = eval(sel['selector_string'])
				msel = sel['selector_name']
			else:
				msel = ''

			pub = calldb('publishers','find_one', {'_id': tr['publisher']})
			if pub == None:
				raise SLException("no publisher obj %s for transform %s" % (tr['publisher'], tr['transform_name']))
			te[pub['publisher_name']] = eval(pub['publisher_string'])
			te['rules'] = [[fil['filter_name'],msel, pub['publisher_name']]]
			rules.append(te)

		return rules

	# Process SteamLink/+/+ messages
	def process(self, msg):
		topic_parts = msg.topic.split('/')
		if topic_parts[2] == "data":
			pkt = loads(msg.payload.decode('utf-8'))
			pkt['_topic'] = msg.topic
			self.logger.debug("on_m: process_native data %s %s", msg.topic, str(pkt)[:90]+"...")
			try:
				self.repub.process_message(pkt)
			except Exception as e:
				self.logger.error("repub message error %s: %s", e, pkt, exc_info=True)
			return
		elif topic_parts[2] != "control":
			self.logger.error( "bogus msg, %s: %s", msg.topic, msg.payload)
			return
		try:
			pkt = json.loads(msg.payload.decode('utf-8'))
		except:
			self.logger.error("control msg to '%s' not json: '%s'", msg.topic, msg.payload)
			return
		self.logger.debug("native control msg %s %s", msg.topic, str(pkt)[:90]+"...")

		sl_id = int(topic_parts[1])
		try:
			opkt = B_typ_0(self.logger, sl_id, pkt['payload'])
		except Exception as e:
			self.logger.error("could not build binary pkt from '%s' '%s', cause '%s'", msg.topic, pkt, e)
			return

		node = self.engine.nodes.by_sl_id(opkt.sl_id)
		if not node:
			raise SLException("repub: node %s not in table" % self.sl_id)
		opkt.setfields(node.node_id, node.mesh.mesh_name, node.swarm.swarm_bkey)
		try:
			otopic = "%s/%s/control" % (self.sv_config['repubprefix'], opkt.mesh)
			pac = opkt.pack()
		except Exception as e:
			self.logger.error( "could not build packet, cause %s" % e)
			return
		if DBG >= 1: self.logger.debug("publish: %s %s", otopic, ":".join("{:02x}".format(c) for c in pac))
		self.engine.publish(otopic, bytearray(pac), as_json=False)

	#
	# repubmqtt publishers
	#
	def publish_mongo(self, publish, output_data, record):
		if publish['_testmode']:
			self.logger.info("test: publish_mongo %s %s" % (url, output_data))
			return
		#N.B. we want to pass a dict to ..insert(..)
		self.logger.debug("publish_mongo %s %s" % (publish['collection'], str(output_data)))

		q = {'func': 'insert', 'collection': publish['collection'], 'what': None,
				'url': publish['url'], 'db': publish['db'], 'data': output_data }
		_id = self.engine.ask_question("SteamDB", None, q)
		if not _id:
			self.logger.error( "publish_mongo error: %s %s %s" % (_id, publish['collection'], str(output_data)))
		return _id


	def publish_mqtt(self, publish, output_data, data):
		try:
			topic = publish['topic'] % data
		except KeyError as e:
			self.logger.error( "publish topic '%s' missing key '%s' in data '%s'" % (publish['topic'], e, data))
			return
		retain = publish.get('retain',False)
		if publish['_testmode']:
			self.logger.info("test: publish_mqtt %s %s" % (topic, output_data))
			return
		print( "publish_mqtt %s %s" % (topic, output_data))
		self.logger.critical( "publish_mqtt %s %s" % (topic, output_data))
		rc, mid = self.engine.mqtt_con.publish(topic, output_data, retain=retain)
		if rc:
			self.logger.error( "publish_mqtt failed %s, %s %s %s" % (rc, topic, output_data, retain))



#
# TransportChannel
#
class TransportChannel(steamengine.Service):
	type = "channel"

	def __init__(self, service, engine, logger, sv_config=None):
		super(TransportChannel, self).__init__(service, engine, logger, sv_config)

		# create thread to read msg 
		self.msglog_task = Thread(target=self.run_msglog)
		self.msglog_task.name = "msglog_th"
		self.msglog_task.daemon = True

		self.msglog_task.start()

	def shutdown(self):
		self.running = False
		self.engine.logger.info("Service %s shutdown" % self.service)


	def poll(self):
		for mname in self.engine.meshes.all():
			topic = "%s/%s/state" % (self.sv_config['prefix'], mname)
			self.engine.publish(topic,"state" )

	def run_msglog(self):
		while self.running:
			r = msglogQ.get()
			self.write_stats_data('log', r)

	def process_transport_data(self, pkg, timestamp):
		""" process a pkt received on the data topic, pubish on the 'native' topic """

		if DBG >= 1: self.logger.debug("process_transport_data  %s", str(pkt))
		# "native" publish
		otopic = "%s/%s/data" % (self.sv_config['native'], pkt.sl_id)
		print(pkg)
		self.engine.publish(otopic, pkt.dict(), as_json=False)


	def write_stats_data(self, what, ddata=None):
		data = []
		if what == 'mesh':
			for mname in self.engine.meshes.all():
				data.append(self.engine.meshes.by_name(mname).reportstatus())
		elif what == 'node':
			for slid in self.engine.nodes.all():
				d = self.engine.nodes.by_sl_id(slid).reportstatus()
				if self.sv_config.get('brief_status', False):
					d['payload'] = "***"
				data.append(d)
		elif what == 'log':
			data = ddata

		if self.sv_config.get('stats_topic', None):
			topic = self.sv_config['stats_topic'] % what
			self.engine.publish(topic, data, retain=True)

		# TODO:  remove when eve reads mqtt stats channel

		if self.sv_config.get('stats_socket', None):
			stats_socket = self.sv_config['stats_socket']
			sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
#			if DBG >= 3: self.logger.debug('write_stats_data: connecting to %s' % stats_socket)
			try:
				sock.connect(stats_socket)
			except socket.error as msg:
#				self.logger.warn('stats data not sent, cause: %s' % msg)
				return

			message = bytes("%s|%s" % (what, json.dumps(data)), 'UTF-8')
#			if DBG >= 3: self.logger.debug('write_stats_data: writing %s' % message)
			sock.sendall(message)



	# Process SL/+/+ messages
	def process(self, msg):
		topic_parts = msg.topic.split('/')
		if len(topic_parts) != 3:
			self.logger.error("transport topic invalid: %s", topic_parts)
			return

		if topic_parts[2] == "data":
			mesh = topic_parts[1]
			pkt = B_typ_0(self.logger)
			try:
				pkt.unpack(msg.payload)
			except SLException as e:
				self.logger.error("transport packet error: %s", e)
				return
			except Exception as e:
				self.logger.exception("transport payload or decrypt error: ")
				return

			node = self.engine.nodes.by_sl_id(pkt.sl_id)
			if not node:
				raise SLException("transport: node %s not in table" % self.sl_id)
			if mesh != node.mesh.mesh_name:
				logger.error("transport: node %s msg from %s should be in %s", node.sl_id, mesh, node.mesh.mesh_name)
				return
			pkt.setfields(node.node_id, node.mesh.mesh_name, node.swarm.swarm_bkey)
			self.logger.debug("transport data %s %s", msg.topic,str(pkt)[:90]+"...")

			self.engine.nodes.by_sl_id(pkt.sl_id).updatestatus(pkt)
			otopic = "%s/%s/data" % (self.sv_config['repubprefix'], pkt.sl_id)
			self.engine.publish(otopic,  pkt.dict())
			self.write_stats_data('node')
			if DBG >= 3: self.logger.debug("on_message: pkt is %s", str(pkt))

		elif topic_parts[2] == "status":
			self.logger.debug("transport status %s %s", msg.topic, msg.payload)
			status = self.engine.meshes.by_name(topic_parts[1]).updatestatus(msg.payload)
			self.write_stats_data('mesh')
			if DBG >= 3:
				self.logger.debug("mesh status %s", self.engine.meshes.by_name(topic_parts[1]).reportstatus())
		elif topic_parts[2] in ["state", "stats", "control" ]:
			pass
		else:
			self.logger.error( "UNKNOWN %s payload %s", msg.topic, msg.payload)


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
		if res is None:
			raise SLException("no mesh %s" % what)
		if '_err' in res:
			raise SLException("cannot get mesh %s" % what)
		try:		# hack to get mongodb ObjectIDs across json channels
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
		if not res:
			raise SLException("no swarm %s" % oid)
		if '_err' in res:
			raise SLException("cannot get swarm %s" % oid)
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
		if res is None:
			raise SLException("no node with sl_id %s" % (sl_id))
		if '_err' in res:
			raise SLException("cannot get node with sl_id %s: %s" % (sl_id, res['_err']))
		try:		# hack to get mongodb ObjectIDs across json channels
			msh = self.sl.meshes.by_oid(str(res['mesh']))
			swm = self.sl.swarms.by_oid(str(res['swarm']))
		except:
			msh = self.sl.meshes.by_oid(res['mesh']['$oid'])
			swm = self.sl.swarms.by_oid(res['swarm']['$oid'])

		node = Node(sl_id, res['node_id'], res['node_name'], msh, swm )
		Nodes.node_table[sl_id] = node
		Swarms.swarm_sl_id_table[sl_id] = swm
		return node



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


def change_runstate(newstate):
	global run_state
	run_state = newstate
	runstate_event.set()

def handler(signum, frame):
	logger.warn('received signal %s' % signum)
	if signum == signal.SIGHUP:
		change_runstate("restart")
	elif signum == signal.SIGTERM:
		change_runstate("terminate")


def check_required_names(conf):
	err = False
	for name in REQUIRED_NAMES:
		if not name in conf:
			logger.error("required entry '%s' missing in config", name)
			err = True
	if err:
		logger.error("correct config and try again")
		sys.exit(1)
	return



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


def loadallconfig():
	if os.path.exists(os.path.expanduser("~/.steamlinkrc")):
		if not loadconf(os.path.expanduser("~/.steamlinkrc")):
			sys.exit(1)

	if not loadconf(os.path.expanduser(cl_args.conf)):
		sys.exit(1)


def set_logging(logfile):
	llog_format='%(threadName)-12s %(levelname)-4s: %(message)s'
	flog_format='%(asctime)s %(threadName)-12s %(levelname)-4s: %(message)s'
	clog_format='%(threadName)-12s: %(message)s'

	log_datefmt='%Y-%m-%d %H:%M:%S'
	logging.basicConfig(format=clog_format, datefmt=log_datefmt)
	logger = logging.getLogger()
	msglog = MsgLogHandler(msglogQ)
	msglogformatter = logging.Formatter(llog_format, datefmt=log_datefmt)
	msglog.setFormatter(msglogformatter)
	msglog.setLevel(logging.INFO)
	logger.addHandler(msglog)
	logger.setLevel(loglevel)
	if logfile:
		console = logger.handlers[0]
		logger.removeHandler(console)

		filelog = logging.handlers.RotatingFileHandler(logfile, maxBytes=10*1024*1024, backupCount=3)
		fileformatter = logging.Formatter(flog_format, datefmt=log_datefmt)
		filelog.setFormatter(fileformatter)
		logger.addHandler(filelog)
	return logger
#
# main
#

msglogQ = queue.Queue()

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

logger = set_logging(None)

runstate_event = Event()
run_state = "run"

signal.signal(signal.SIGHUP, handler)
signal.signal(signal.SIGTERM, handler)

while run_state is "run":
	startts = time.time()
	conf = {}

	loadallconfig()
	check_required_names(conf)	# sys.exit on error!

	DBG = cl_args.debug if cl_args.debug > 0 else conf.get('DBG',0)

	if conf.get('PIDFILE', None):
		open(conf['PIDFILE'],"w").write("%s" % os.getpid())

	if conf.get('LOGFILE', None):
		set_logging(conf['LOGFILE'])

	rc = 0
	logger.info('steamlink starting')
	runstate_event.clear()
	try:
		sl = SEngine(conf, logger)
		runstate_event.wait()
	except KeyboardInterrupt:
		change_runstate("terminate")
		rc = 2
	except Exception as e:
		if DBG > 0 or time.time() > (startts + 1):
			run_state = "terminate"
			rc = 4
		else:
			run_state = "restart"
			time.sleep(5)
		logger.exception( 'steamlink runtime error, will now %s: %s: ' % (run_state, e))
	finally:
		sl.shutdown()
		if run_state is "restart":
			run_state = "run"
		if run_state != "run":
			break

logger.info('steamlink exit, code=%s' % rc)
logging.shutdown()
sys.exit(rc)
