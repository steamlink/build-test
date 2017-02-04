from gevent import monkey
monkey.patch_all()

import logging
from threading import Thread, Event
import paho.mqtt.client as mqtt
import json
import queue
import time
import cProfile

sample_configuration = {
	'my_client_id' : 'store1',
	'allow_short_circuit': False,

	'mqtt_conf' : {
		'MQTT_CLIENTID': 'sl_store1',
		'MQTT_USERNAME': 'andreas',
		'MQTT_PASSWORD': '1foot123',
		'MQTT_SERVER': 'mqtt.steamlink.net',
		'MQTT_PORT': 1883
	}
}


class SteamLink:
	""" implement routing of mqtt msgs to servers """

	def __init__(self, conf, logger):
		self.my_clientid = conf['my_client_id']
		self.mqtt_conf = conf['mqtt_conf']
		self.mqtt_con = None
		self.short_circuit = conf.get('allow_short_circuit', False)

		self.sl_logger = logger
		self.task_timeout = 10	# number of seconds to wait for answers

		# service_routing is keyed with subscription topic (mqtt flavoured re), values are the queues to send message to
		self.running = True
		self.mqtt_ready = Event()
		self.service_routing = {}
		self.task_id = 0
		self.tasks = {}

		# create thread to handle backflow of answers
		self.answer_task = Thread(target=self.run_answers)
		self.answer_task.name = "answer_th"
		self.answer_task.daemon = True

		# queue to receive service answers
		self.service_answers = queue.Queue()

		# start answer runner
		self.answer_task.start()
		# connect to broker
		self.mqtt_con = mqtt.Client(client_id=self.mqtt_conf['MQTT_CLIENTID'])
		self.mqtt_connect()


	def run_answers(self):
		self.sl_logger.info("Steamlink run_answers start")
		while self.running:
			answer = self.service_answers.get()
			topic = answer['_topic'][:-1]+'a'
			self.sl_logger.info('run_answer publish %s %s', topic, str(answer))
			self.mqtt_con.publish(topic, json.dumps(answer))
			self.service_answers.task_done()


	def return_answer(self, pkt):
		tp = pkt['_topic'].split('/')
		client_id = tp[1]
		task_id = int(tp[2])
		if self.short_circuit and client_id == self.my_clientid and task_id in self.tasks:
			self.tasks[task_id].answer = pkt
			self.tasks[task_id].set()
		else:
			self.service_answers.put(pkt)


	def mqtt_connect(self, reconnect=False):
		if reconnect:
			self.mqtt_con.loop_stop()
			self.mqtt_con.disconnect()
#			self.mqtt_con.reinitialise(client_id=self.mqtt_conf['MQTT_CLIENTID'])
		self.mqtt_con = mqtt.Client(client_id=self.mqtt_conf['MQTT_CLIENTID'])
		# TODO: figure out cert in conf
#		self.mqtt_con.tls_set("/home/steamlink/auth/mqtt/ca.crt"),
		self.mqtt_con.username_pw_set(self.mqtt_conf['MQTT_USERNAME'], self.mqtt_conf['MQTT_PASSWORD'])
		self.mqtt_con.tls_insecure_set(False)
		self.mqtt_con.on_connect = self.mqtt_on_connect
		self.mqtt_con.on_disconnect = self.mqtt_on_disconnect
		self.mqtt_con.on_message = self.mqtt_on_message

		self.sl_logger.info("connecting to MQTT")
		self.mqtt_con.connect(self.mqtt_conf['MQTT_SERVER'], self.mqtt_conf['MQTT_PORT'], 60)
		self.mqtt_con.loop_start()


	def mqtt_on_disconnect(self, client, userdata, flags):
		self.mqtt_ready.clear()
		self.mqtt_con = None


	def mqtt_on_connect(self, client, userdata, flags, rc):
		self.sl_logger.info("connected to MQTT boker,code "+str(rc))
		self.mqtt_con.subscribe("+/%s/+/a" % self.my_clientid)
		for service_id in self.service_routing:
			stopic = "%s/%s/+/q" % (service_id, self.my_clientid)
			self.mqtt_con.subscribe(stopic)
			self.sl_logger.info( "subscribe %s" % stopic)

		self.mqtt_ready.set()


	def mqtt_on_message(self, client, userdata, msg):
		pkt = json.loads(msg.payload.decode('utf-8'))

		pkt['_topic'] = msg.topic
		topic_parts = msg.topic.split('/')
		self.sl_logger.info("mqtt_on_message %s %s", topic_parts, str(pkt))
		assert len(topic_parts) is 4
		assert topic_parts[1] == self.my_clientid
		msg_service_id = topic_parts[0]
		msg_task_id = int(topic_parts[2])
		msg_qa = topic_parts[3]

		handled = False

		if  msg_qa is 'a':
			self.receive_answer(pkt, msg_task_id)
			handled = True
		elif msg_qa is 'q':
			handled = self.dispatch_service(msg_service_id, pkt)
		else:
			self.sl_logger.error("on_message: not q or a %s", msg.topic)
			handled = True
		if not handled:
			self.sl_logger.error("unknown service %s", topic_parts[0])


	def dispatch_service(self, msg_service_id, pkt):
		handled = False
		for service_id in self.service_routing:
			if msg_service_id == service_id:
				for service_queue in self.service_routing[service_id]:
					# add it to queue
					service_queue.put(pkt)
					handled = True
		return handled


	def publish(self, topic, pkt):
		self.mqtt_ready.wait()
		self.sl_logger.info("publish: %s %s", topic, pkt)
		self.mqtt_con.publish(topic, json.dumps(pkt))


	def register_service(self, service_id, service_queue):
		if not service_id in self.service_routing:
			self.service_routing[service_id] = []
		self.sl_logger.info("register service %s", service_id)
		self.service_routing[service_id] += [service_queue]
		self.mqtt_connect(True)


	def receive_answer(self, pkt, task_id):
		if  not task_id in self.tasks:
			self.sl_logger.error("received answer for unknown task id %s", task_id)
		else:
			self.tasks[task_id].answer = pkt
			self.tasks[task_id].set()


	def ask_question(self, service_id, client_id,  pkt):
		task_id = self.new_task_id()
		self.tasks[task_id] = Event()
		topic = "%s/%s/%s/q" % (service_id, client_id, task_id)
		if self.short_circuit and client_id == self.my_clientid:
			pkt['_topic'] = topic
			self.dispatch_service(service_id, pkt)
		else:
			self.publish(topic, pkt)
		if self.tasks[task_id].wait(self.task_timeout):
			answer = self.tasks[task_id].answer
			del answer['_topic']
			del self.tasks[task_id]
		else:
			answer = None
		return answer


	def new_task_id(self):
		self.task_id +=1
		return self.task_id



class Service(Thread):
	def __init__(self, service_id, steamlink, logger):
		super(Service, self).__init__()
		self.request_q = queue.Queue()
		self.daemon = True

		self.logger = logger
		self.service_id = service_id
		self.steamlink = steamlink
		self.name = "%s_th" % service_id

		self.steamlink.register_service(self.service_id, self.request_q)
		self.running = True


	def run(self):
		self.steamlink.sl_logger.info("Service run start")
		while self.running:
			pkt = self.request_q.get()
			self.request_q.task_done()
			save_topic = pkt['_topic']
			del pkt['_topic']
			answer = self.process(pkt)
			answer['_topic'] = save_topic
			self.steamlink.return_answer(answer)


	def process(self, pkt):	
		""" we are echoing the msg"""
		pkt['ping'] = 'pong'
		return pkt



class DBService(Service):
	mongoclient = None

	def __init__(self, db_conf, my_clientid, my_service_id, logger):
		self.db_conf = db_conf
		mqtt_conf = db_conf['mqtt']	# XXX

		super(self, self).__init__(mqtt_conf, my_clientid, my_service_id, logger)


	def startmongo(self):
		if not SteamDB.mongoclient:
			SteamDB.mongoclient = MongoClient(self.mongourl)



def main():
	log_format='%(asctime)s %(threadName)s %(levelname)s: %(message)s'
	log_datefmt='%Y-%m-%d %H:%M:%S'
	log_name = 'steam'
	logging.basicConfig(level=logging.DEBUG, format=log_format, datefmt=log_datefmt)
	logger = logging.getLogger(log_name)

	Steam = SteamLink(sample_configuration, logger)

	client_id = 'store1'
	# start a ping service
	ping_srv = Service("ping", Steam, logger)
	ping_srv.start()

	startts = time.time()
	cnt = 20
	for i in range(cnt):
		res = Steam.ask_question("ping", client_id, {"ping": ""})
		if res is None:
			print("timeout!!")
	dur = ((time.time() - startts) / cnt) * 1000
	
	print("%0.2f per call" % dur)
	print(res)

#cProfile.run('main()')
main()
