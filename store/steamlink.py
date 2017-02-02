from gevent import monkey
monkey.patch_all()
import logging
from threading import Thread, Event
import paho.mqtt.client as mqtt
import json
from threading import Event, Thread
import queue


services = {}

sample_configuration = {
    'my_client_id' : 'store1'
    # TODO: copy from existing
    'mqtt_conf' : {
        # TODO
    }
}



class SteamLink:

        service_answers = None
        mqtt_con = None
        sl_logger = None

        def register_service(id, name):
            services[name] = id


        def find_service(name):
            return services.get(name, None)

        def __init__(self, mqtt_conf, my_clientid, my_service_id, logger):
                SteamLink.mqtt_conf = mqtt_conf
                SteamLink.my_clientid = my_clientid
                self.my_service_id = 'SteamLink'
                self.logger = logger
                SteamLink.sl_logger = logger
                # topic_routing is keyed with subscription topic (mqtt flavoured re), values are functions to call on message
                SteamLink.topic_routing = {}
                SteamLink.tasks = {}
                SteamLink.task_id = 0
                # queues
                SteamLink.service_answers = queue.Queue()
                self.service_queue = queue.Queue()
                # connect to broker
                SteamLink.connect()

        def connect():
                if SteamLink.mqtt_con == None:
                    SteamLink.mqtt_con = mqtt.Client(client_id=self.mqtt_conf['MQTT_CLIENTID'])
                    # TODO: figure out cert in conf
                    SteamLink.mqtt_con.tls_set("/home/steamlink/auth/mqtt/ca.crt")
                    SteamLink.mqtt_con.username_pw_set(self.mqtt_conf['MQTT_USERNAME'], self.mqtt_conf['MQTT_PASSWORD'])
                    SteamLink.mqtt_con.tls_insecure_set(False)
                    SteamLink.mqtt_con.on_connect = SteamLink.on_connect
                    SteamLink.mqtt_con.on_message = SteamLink.on_message

        def on_connect(client, userdata, flags, rc):
                SteamLink.sl_logger.info("connected to MQTT boker,code "+str(rc))
                for topic in self.topic_routing:
                        SteamLink.mqtt_con.subscribe(topic)
                        SteamLink.sl_logger.info( "subscribe %s" % topic)

        def on_message(client, userdata, msg):
                pkt = json.loads(msg.payload.decode('utf-8'))
                pkt['topic'] = msg.topic
                topic_parts = msg.topic.split('/')
                assert len(topic_parts) is 4
                for topic in SteamLink.topic_routing:
                        if mqtt.topic_matches_sub(topic, msg.topic):
                                for service_queue in topic_routing[topic]:
                                        # respond to a question
                                        if topic_parts[3] is 'q':
                                                # add it to queue
                                                service_queue.put(pkt)
                                        # process an answer to an asked question
                                        elif topic_parts[3] is 'a':
                                                taskid = topic_parts[2]
                                                self.receive_answer(func, pkt, taskid)
                                        else:
                                                # TODO call func directly?
                                                SteamLink.sl_logger.error("not question or answer")

        def publish(self, topic, pkt):
                SteamLink.mqtt_con.publish(topic, json.dumps(pkt))

        def register_subscription(self, subscription, service_queue):
                if not subscription in self.topic_routing:
                        self.topic_routing[subscription] = []
                self.topic_routing[subscription] += service_queue

        def respond_to_question(self, func, pkt):
                tid = self.new_task_id()
                self.tasks[tid] = Service(tid, func, pkt, self)
                self.tasks[tid].start()

        def ask_question(serviceid, pkt):
                eid = self.new_task_id()
                SteamLink.events[eid] = Event()
                topic = "%s/%s/%s/q" % (serviceid, SteamLink.my_clientid, eid)
                SteamLink.publish(topic, pkt)
                SteamLink.events[eid].wait()
                return SteamLink.events[eid].answer

        def receive_answer(func, pkt, taskid):
                SteamLink.events[taskid].answer = pkt
                SteamLink.events[taskid].set()

        def new_task_id(self):
                self.task_id +=1
                return self.task_id

        def ask_many_question(self, questions):
                pass


class Question:
        def __init__(self, serviceid, pkt):
                self.serviceid = serviceid
                self.my_clientid = my_clientid
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
                topic = self.pkt['topic'][:-1]+'a'
                steamlink.publish(topic, resp)
                # TODO: clean up tasks table
                self.done = True


class SteamDB(Steamlink):
        mongoclient = None

        def __init__(self, db_conf, my_clientid, my_service_id, logger):
                self.db_conf = db_conf
                mqtt_conf = db_conf['mqtt']	# XXX

                super(SteamLink, self).__init__(mqtt_conf, my_clientid, my_service_id, logger)


        def startmongo(self):
                if not SteamDB.mongoclient:
                        SteamDB.mongoclient = MongoClient(self.mongourl)

