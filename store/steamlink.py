from gevent import monkey
monkey.patch_all()
import logging
from threading import Thread, Event
import paho.mqtt.client as mqtt
import json
from threading import Event, Thread

class SteamLink:

    def __init__(self, mqtt_conf, logger):
        self.mqtt_conf = mqtt_conf
        self.client = None
        self.logger = logger
        self.connect()
        # topic_routing is keyed with subscription topic (mqtt flavoured re), values are functions to call on message
        self.topic_routing = {}
        self.tasks = {}
        self.task_id = 0

    def connect(self):
        self.client = mqtt.Client(client_id=self.mqtt_conf['MQTT_CLIENTID'])
        # TODO: figure out cert in conf
        self.client.tls_set("/home/steamlink/auth/mqtt/ca.crt")
        self.client.username_pw_set(self.mqtt_conf['MQTT_USERNAME'], self.mqtt_conf['MQTT_PASSWORD'])
        self.client.tls_insecure_set(False)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message

    def on_connect(self, client, userdata, flags, rc):
        self.logger.info("connected to MQTT boker,code "+str(rc))
        for topic in self.topic_routing:
            self.client.subscribe(topic)
            self.logger.info( "subscribe %s" % topic)

    def on_message(self, client, userdata, msg):
        pkt = json.loads(msg.payload.decode('utf-8'))
        pkt['topic'] = msg.topic
        topic_parts = msg.topic.split('/')
        assert len(topic_parts) is 4
        for topic in self.topic_routing:
            if mqtt.topic_matches_sub(topic, msg.topic):
                for func in topic_routing[topic]:
                    # respond to a question
                    if topic_parts[3] is 'q':
                        self.respond_to_question(func, pkt)
                    # process an answer to an asked question
                    elif topic_parts[3] is 'a':
                        taskid = topic_parts[2]
                        self.receive_answer(func, pkt, taskid)
                    else:
                        # TODO call func directly?
                        self.logger.error("not question or answer")

    def publish(self, topic, pkt):
        self.client.publish(topic, json.dumps(pkt))

    def register_subscription(self, subscription, func):
        if not subscription in self.topic_routing:
            self.topic_routing[subscription] = []
        self.topic_routing[subscription] += func

    def respond_to_question(self, func, pkt):
        tid = self.new_task_id()
        self.tasks[tid] = Service(tid, func, pkt, self)
        self.tasks[tid].start()

    def ask_question(self, serviceid, clientid, pkt):
        eid = self.new_task_id()
        self.events[eid] = Event()
        topic = "%s/%s/%s/q" % (serviceid, clientid, eid)
        self.publish(topic, pkt)
        self.events[eid].wait()
        return self.events[eid].answer

    def receive_answer(self, func, pkt, taskid):
        self.events.[taskid].answer = pkt
        self.events.[taskid].set()

    def new_task_id(self):
        self.task_id +=1
        return self.task_id

    def ask_many_question(self, questions)

class Question:
    def __init__(self, serviceid, clientid, pkt):
        self.serviceid = serviceid
        self.clientid = clientid
        self.pkt = pkt


class Service(Thread):
    def __init__(self, tid, func, pkt, steamlink):
        super(Thread, self).__init__()
        self.tid = tid
        self.func = func
        self.pkt = pkt
        self.done = False

    def run(self):
        resp = self.func(self.pkt)
        topic = self.pkt.['topic'][:-1]+'a'
        steamlink.publish(topic, resp)
        # TODO: clean up tasks table
        self.done = True

