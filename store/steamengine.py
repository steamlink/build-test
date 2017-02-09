
from gevent import monkey
monkey.patch_all()

import logging
from threading import Thread, Event
import paho.mqtt.client as mqtt
import json
import queue
import sys
import time
import inspect

from bson import Binary, Code
from bson.json_util import dumps
from bson.json_util import loads
from pymongo import MongoClient

# Defaults
allow_short_circuit = True
network_timeout = 10
topic_encode = "SE/%(service)s/%(client_id)s/%(task_id)s/%(direction)s"

DBG = 1

class Err:
	def __init__(self, code, msg):
		self.code = code
		self.msg = msg
	def d(self):
		return {'_err': { 'code': self.code, 'message': self.msg }}


class Srv:
	def __init__(self, queue, typ):
		self.queue = queue
		self.typ = typ
		if not typ in ['channel', 'server']:
			logger.error('unkown service type %s', typ)
			sys.exit(1)


class Topic:
	encode = None
	def __init__(self, service, client_id=None, task_id=None, direction=None):
		if direction != None:		# we have all strings --> descret
			if not client_id or not task_id:
				raise ValueError("Topic missing argument")
			self.service = service
			self.client_id = client_id
			self.task_id = task_id
			self.direction = direction
		else:						# we have one string --> decode
			topic = service
			t = topic.split('/')
			e = Topic.encode.split('/')
			if len(t) != len(e):
				raise ValueError("topic malformed: %s" % topic)
			for i in range(len(t)):
				if e[i][:2] == "%(" and e[i][-2:] == ')s':
					n = e[i][2:-2]
					self.__dict__[n] = t[i]
				elif e[i] != t[i]:
					raise ValueError("topic malformed2: %s" % topic)
			return

	def __repr__(self):
		return Topic.encode % self.__dict__


class SteamEngine:
	""" implement routing of mqtt msgs to servers """

	def __init__(self, conf, logger):
		self.conf = conf
		self.logger = logger

		self.my_clientid = conf['my_client_id']
		self.short_circuit = conf.get('allow_short_circuit', allow_short_circuit)
		self.task_timeout = conf.get('network_timeout', network_timeout)
		Topic.encode = conf.get('topic_encode', topic_encode)

		# service_routing is keyed with subscription topic (mqtt flavoured re), values is the queue to send message to
		self.running = True
		self.service_routing = {}
		self.task_id = 0
		self.tasks = {}

		# create thread to handle backflow of answers
		self.answer_task = Thread(target=self.run_answers)
		self.answer_task.name = "answer_th"
		self.answer_task.daemon = True

		# create thread to polling
		self.poll_interval = int(self.conf.get('pollinterval', 3600))
		self.poll_task = Thread(target=self.run_poll)
		self.poll_task.name = "poll_th"
		self.poll_task.daemon = True

		# queue to receive service answers
		self.service_answers = queue.Queue()
		self.service_answer_topic = Topic("+", self.my_clientid, "+", "a")

		# start runners
		self.answer_task.start()
		self.poll_task.start()

		# connect to broker
		self.mqtt_ready = Event()
		self.mqtt_connect()

		# start service
		self.load_and_run_services()


	def shutdown(self):
		self.running = False
		self.mqtt_con.loop_stop()
		self.mqtt_con.disconnect()
		for service in self.conf['services']:
			self.services[service].shutdown()


	def poll_for_status(self):
		for service in self.services:
			if getattr(self.services[service], 'poll', None) \
					and inspect.ismethod(self.services[service].poll):
				self.logger.debug("polling %s", service)
				self.services[service].poll()


	def run_poll(self):
		self.logger.info("Steamlink run_poll start")
		polltime = time.time() - self.poll_interval	+ 3 # force poll 3 sec after boot
		while self.running:
			waitt = max(polltime + self.poll_interval - time.time(), 0)
			if waitt > 0:
				time.sleep(waitt)
			else:
				polltime = time.time()
				self.poll_for_status()


	def check_sv(self, sv, avail_services):
		err = False
		if not sv['type'] in ['server', 'channel']:
			self.logger.error("unknown service type '%s'", sv['type'])
			err = True
		if not sv['class'] in avail_services:
			self.logger.error("unknown service class '%s'", sv['class'])
			err = True
		if err:
			self.logger.error("correct config and re-run")
			sys.exit(1)


	def get_available_services(self):
		# find available channels by searching all classes for a 'type' class variable
		# with a contect of server or channel
		avail_services = {}
		for name, obj in inspect.getmembers(sys.modules[__name__]):
			if inspect.isclass(obj) and getattr(obj, "type", None) in ['server', 'channel']:
				avail_services[name] = obj
		return avail_services

	def load_and_run_services(self):
		self.services = {}
		avail_services = self.get_available_services()

		for service in self.conf['services']:
			sv = self.conf['services'][service]
			self.check_sv(sv, avail_services)
			self.services[service] = avail_services[sv['class']](service, self, self.logger, sv)

		for service in self.conf['services']:
			self.services[service].start()


	def run_answers(self):
		self.logger.info("Steamlink run_answers start")
		while self.running:
			answer = self.service_answers.get()
			topic = Topic(answer['_topic'])
			topic.direction = 'a'
			topic.client_id = answer['_origin']
			self.logger.debug('run_answer publish %s %s', topic, str(answer)[:70]+"...")
			self.mqtt_con.publish(str(topic), dumps(answer))
			self.service_answers.task_done()


	def return_answer(self, pkt):
		tp = Topic(pkt['_topic'])
		if self.short_circuit \
				and tp.client_id == self.my_clientid \
				and tp.task_id in self.tasks:
			self.tasks[task_id].answer = pkt
			self.tasks[task_id].set()
		else:
			self.service_answers.put(pkt)


	def mqtt_connect(self, reconnect=False):
		self.mqtt_ready.clear()
		if reconnect:
			self.mqtt_con.loop_stop()
			self.mqtt_con.disconnect()
			self.mqtt_con.reinitialise(client_id=self.conf['MQTT_CLIENTID'])
		else:
			self.mqtt_con = mqtt.Client(client_id=self.conf['MQTT_CLIENTID'])
		# TODO: figure out cert in conf
		self.mqtt_con.tls_set(self.conf['MQTT_CERT']),
		self.mqtt_con.username_pw_set(self.conf['MQTT_USERNAME'], self.conf['MQTT_PASSWORD'])
		self.mqtt_con.tls_insecure_set(False)
		self.mqtt_con.on_connect = self.mqtt_on_connect
		self.mqtt_con.on_disconnect = self.mqtt_on_disconnect
		self.mqtt_con.on_message = self.mqtt_on_message

		self.logger.info("connecting to MQTT")
		self.mqtt_con.connect(self.conf['MQTT_SERVER'], self.conf['MQTT_PORT'], 60)
		self.mqtt_con.loop_start()
		self.mqtt_con._thread.name = "mqtt_th"	# N.B.


	def mqtt_on_disconnect(self, client, userdata, flags):
		self.mqtt_ready.clear()
		self.logger.warn("disconnected from MQTT boker")


	def mqtt_on_connect(self, client, userdata, flags, rc):
		if self.mqtt_con is None:
			self.mqtt_connect(reconnect=True)
			return
		self.logger.warn("connected to MQTT boker,code "+str(rc))
		# subscribe to all answers for my_client_id
		self.subscribe(self.service_answer_topic)
		self.mqtt_ready.set()
		for service_topic in self.service_routing:
			self.subscribe(service_topic)


	def subscribe(self, service_topic):
		self.mqtt_con.subscribe(str(service_topic))
		self.logger.debug( "subscribe %s" % service_topic)


	def mqtt_on_message(self, client, userdata, msg):
		srv_type = None
		if mqtt.topic_matches_sub(str(self.service_answer_topic), msg.topic):
			srv_type = "answer"
		else:
			for service_sub in self.service_routing:
				if mqtt.topic_matches_sub(service_sub, msg.topic):
					srv_type = self.service_routing[service_sub].typ
					break
		if not srv_type:
			self.logger.error("no registered service for msg topic %s", msg.topic)
			return

		if srv_type == 'channel':
			self.service_routing[service_sub].queue.put(msg)

		elif srv_type in ['server', 'answer']:
			tp = Topic(msg.topic)
			assert tp.client_id == self.my_clientid
			pkt = json.loads(msg.payload.decode('utf-8'))
			pkt['_topic'] = msg.topic
			self.logger.debug("mqtt_on_message %s %s", msg.topic, str(pkt)[:70]+"...")

			if  srv_type is 'answer':
				self.receive_answer(pkt, int(tp.task_id))
			elif srv_type is 'server':
				self.dispatch_service(tp.service, pkt)


	def receive_answer(self, pkt, task_id):
		if  not task_id in self.tasks:
			self.logger.error("received answer for unknown task id %s", task_id)
		else:
			self.tasks[task_id].answer = pkt
			self.tasks[task_id].set()


	def dispatch_service(self, msg_service, pkt):
		msg_service_topic = str(Topic(msg_service, self.my_clientid, "+", "q"))
		if not msg_service_topic in self.service_routing:
			self.logger.error("no service '%s' defined", msg_service)
			return
		self.service_routing[msg_service_topic].queue.put(pkt)
		return


	def publish(self, topic, pkt, as_json=True, retain=False):
		self.mqtt_ready.wait()
		self.logger.debug("publish: %s %s", topic, pkt)
		if as_json:
			opkt = dumps(pkt)
		else:
			opkt = pkt
		self.mqtt_con.publish(str(topic), opkt, retain=retain)


	def register_service(self, service_topic, service_queue, service_type):
		if  service_topic in self.service_routing:
			logger.error("service %s already defined", service_topic)
		else:
			self.logger.debug("register service %s", service_topic)
			self.service_routing[service_topic] = Srv(service_queue, service_type)
			if self.mqtt_ready.is_set():
				self.subscribe(service_topic)


	def ask_question(self, service, client_id,  pkt):
		if client_id == None:
			client_id = self.conf['default_clients'].get(service, self.my_clientid)
		task_id = self.new_task_id()
		self.tasks[task_id] = Event()
		topic = Topic(service, client_id, task_id, "q")
		pkt['_origin'] = self.my_clientid
		if self.short_circuit and client_id == self.my_clientid:
			pkt['_topic'] = str(topic)
			self.dispatch_service(service, pkt)
		else:
			self.publish(topic, pkt)
		if self.tasks[task_id].wait(self.task_timeout):
			answer = self.tasks[task_id].answer
			del self.tasks[task_id]
			del answer['_topic']
		else:
			return Err('TIMEOUT', 'network request  timed out').d()
		return answer['res']


	def new_task_id(self):
		self.task_id +=1
		return self.task_id

#
# Service
class Service(Thread):
	type = "server"	# or "channel"
	def __init__(self, service, engine, logger, sv_config):
		super(Service, self).__init__()
		self.request_q = queue.Queue()
		self.sv_config = sv_config
		self.daemon = True

		self.logger = logger
		self.service = service
		self.engine = engine
		self.name = "%s_th" % service

		if 'topic' in sv_config:
			self.service_topic = sv_config['topic']
		else:
			self.service_topic = str(Topic(service, engine.my_clientid, "+", "q"))
			sv_config['topic'] = self.service_topic

		if 'type' in sv_config:
			self.service_type = sv_config['type']
		else:
			self.service_type = 'channel'
			sv_config['type'] = self.service_type

		self.engine.register_service(self.service_topic, self.request_q, self.service_type)
		self.running = True


	def defered_setup(self):
		pass


	def shutdown(self):
		self.engine.logger.info("Service %s shutdown" % self.service)
		self.running = False


	def run(self):
		self.defered_setup()
		self.engine.logger.info("Service %s start" % self.service)
		while self.running:
			pkt = self.request_q.get()
			if self.service_type == 'channel':
				self.process(pkt)
			else:
				save_topic = pkt['_topic']
				save_origin = pkt['_origin']
				del pkt['_topic']
				answer = self.process(pkt)
				answer['_topic'] = save_topic
				answer['_origin'] = save_origin
				self.engine.return_answer(answer)
				self.request_q.task_done()
		self.engine.logger.info("Service %s finished" % self.service)


	def process(self, pkt):
		# override
		return pkt
