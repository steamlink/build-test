# -*- coding: utf-8 -*-
from gevent import monkey
monkey.patch_all()

import paho.mqtt.client as mqtt
from threading import Thread, Event
import socket
import json
import os


"""
    Eve Demo
    ~~~~~~~~

    A demostration of a simple API powered by Eve REST API.

    The live demo is available at eve-demo.herokuapp.com. Please keep in mind
    that the it is running on Heroku's free tier using a free MongoHQ
    sandbox, which means that the first request to the service will probably
    be slow. The database gets a reset every now and then.

    :copyright: (c) 2016 by Nicola Iarocci.
    :license: BSD, see LICENSE for more details.
"""

import os
import struct
import hashlib
import pprint
import time
from eve import Eve
from flask import render_template, request, redirect
from flask_socketio import SocketIO

tmpl_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'templates')
static_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'static')

# Heroku support: bind to PORT if defined, otherwise default to 5000.
if 'PORT' in os.environ:
    port = int(os.environ.get('PORT'))
    # use '0.0.0.0' to ensure your REST API is reachable from all your
    # network (and not only your computer).
    host = '0.0.0.0'
else:
    port = 5000
    host = '127.0.0.1'

async_mode = None


thread = Thread()
thread_stop_event = Event()


class MqttThread(Thread):
    def __init__(self):
        super(MqttThread, self).__init__()
        self.client = mqtt.Client(client_id="web99")
        #self.client.tls_set("/home/steamlink/auth/mqtt/ca.crt")
        self.client.username_pw_set("andreas","1foot123")
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_message = self.on_message

        self.client.tls_insecure_set(True)
        self.client.connect("mqtt.steamlink.net", 1883, 60)
        self.loop = gevent.get_hub().loop
        self.watcher_r = self.loop.io(self.client.socket().fileno(), gevent.core.READ)
        self.watcher_r.start(self.handle_read, self.client)
        self.watcher_w = self.loop.io(self.client._sockpairR.fileno(), gevent.core.READ)
        self.watcher_w.start(self.handle_write, self.client)


    def handle_write(self, mqttc):
        mqttc._sockpairR.recv(1)
        status = mqttc.loop_write()
        if status != mqtt.MQTT_ERR_SUCCESS:
            print('error', mqtt.error_string(status))
            mqttc._state = mqtt.mqtt_cs_disconnecting

    def handle_read(self, mqttc):
        status = mqttc.loop_read()
        if status != mqtt.MQTT_ERR_SUCCESS:
            print('error', mqtt.error_string(status))
            mqttc._state = mqtt.mqtt_cs_disconnecting


    def runmqtt(self):
        #infinite loop of polling mqtt
        while self.client._state != mqtt.mqtt_cs_disconnecting:
            status = self.client.loop_misc()
            if status == mqtt.MQTT_ERR_SUCCESS:
                gevent.sleep(1)
            else:
                print('error', mqtt.error_string(status))
                self.client._state = mqtt.mqtt_cs_disconnecting
                break

#        self.client.loop_forever(timeout=1.0, max_packets=1, retry_first_connection=False)

    def on_connect(self, client, userdata, flags, rc):
        print("Connected with result code "+str(rc))

        # Subscribing in on_connect() means that if we lose the connection and
        # reconnect then subscriptions will be renewed.

        for topic in ["SteamLink/mesh/status"]:
            client.subscribe(topic)
            print("Subscribing %s" % topic)

    def on_disconnect(self, client, userdata, rc):
        print("disconnected rc=%s" % rc)

    def on_message(self, client, userdata, msg):
        msg = msg.payload.decode().strip('\0')
#        print("message %s" % (str(msg)))
        socketio.emit('msg', {'count': [json.loads(msg)]}, namespace='/sl')


    def run(self):
        self.runmqtt()


class FileThread(Thread):
    def __init__(self):
        super(FileThread, self).__init__()
        self.stats_socket_address = '/tmp/eve_stats_socket'

        try:
            os.unlink(self.stats_socket_address)
        except OSError:
            if os.path.exists(self.stats_socket_address):
                raise

        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)


    def on_message(self, msg):
        msg = msg.decode().strip('\0')
        print("message %s" % (str(msg)))


    def run(self):
        self.sock.bind(self.stats_socket_address)
        self.sock.listen(1)
        
        while True:
            connection, client_address = self.sock.accept()

            try:
                while True:
                    data = connection.recv(16)
                    print('received "%s"' % data)
                    if data:
                        socketio.emit('msg', {'count': [data]}, namespace='/sl')
                        connection.sendall(data)
                    else:
                        print('no more data from', client_address)
                        break
            except Exception as e:
                print("filesock: trap %s" % e)
            
            finally:
                # Clean up the connection
                connection.close()
        

app = Eve()
app.template_folder = tmpl_dir
app.static_folder = static_dir
socketio = SocketIO(app, async_mode=async_mode)



concount = 0

@app.route('/')
def index():
    return render_template('index.html', async_mode=socketio.async_mode)

@app.route('/add-mesh')
def add_mesh():
    return render_template('add_mesh.html', async_mode=socketio.async_mode)

@app.route('/add-transform')
def add_transform():
    return render_template('add_transform.html', async_mode=socketio.async_mode)

@app.route('/evelog')
def evelog():
    def generate():
        with open('/home/steamlink/log/steamlink_eve.log') as f:
#            while True:
                yield f.read()
#		time.sleep(1)

    return app.response_class(generate(), mimetype='text/plain')


@app.route('/republog')
def republog():
    def generate():
        with open('/home/steamlink/log/su-steamlink-repub.log') as f:
#            while True:
                yield f.read()
#		time.sleep(1)

    return app.response_class(generate(), mimetype='text/plain')


def add_token_to_node(items):
    for item in items:
        mesh = app.data.driver.db['meshes'].find_one({"_id" : item["mesh"]})
        swarm = app.data.driver.db['swarms'].find_one({"_id" : item["swarm"]})
        item['sl_id'] = app.data.driver.db['sl_ids'].find_and_modify(
            query={ },
            update={'$inc': {'id': 1}},
            upsert=True
        ).get('id')
        item['node_id'] = app.data.driver.db['meshes'].find_and_modify(
            query={'_id' : mesh["_id"]},
            update={'$inc': {'last_allocated': 1}},
            upsert=False
        ).get('last_allocated')
        item["token"] = create_token(mesh['radio']['radio_params'], swarm['swarm_crypto']['crypto_key'], item['sl_id'], item['node_id'])

def create_token(radio_params, swarm_key, sl_id, node_id ):
    # TODO: update with an if for normal
    modem_config = "00"
#    freq = struct.pack('<f',915.00).hex()
    x = struct.pack('<f',915.00)
    import codecs
    freq = codecs.encode(x, 'hex').decode()
    print("fre is ", freq)
    key = hashlib.sha224(swarm_key.encode("utf-8")).hexdigest()[:32]
    print("key is ", key)
#    hexed_sl_id = struct.pack('<I',sl_id).hex()
    x = struct.pack('<I',sl_id)
    hexed_sl_id = codecs.encode(x, 'hex').decode()
    print("sid is ", hexed_sl_id)
#    hexed_node_id = struct.pack('<B', node_id).hex()
    x = struct.pack('<B', node_id)
    hexed_node_id = codecs.encode(x, 'hex').decode()
    print("nid is ", hexed_node_id)
    return key + hexed_sl_id + freq + modem_config + hexed_node_id


def send_bridge_token(response):
    for item in response["_items"]:
        item["token"] = create_token(item["radio"]["radio_params"],"secret", 99, 1)


def init_last_allocated(items):
    for item in items:
        item['last_allocated'] = 5

app.on_insert_nodes += add_token_to_node
app.on_insert_meshes += init_last_allocated
app.on_fetched_resource_meshes += send_bridge_token

@app.route('/status')
def pymeetups():
    return render_template('status.html', async_mode=socketio.async_mode)

@socketio.on('connect', namespace='/sl')
def ws_conn():
    global concount, thread
    c = "{}"
#    socketio.emit('msg', {'count': c}, namespace='/sl')
#    if not thread.isAlive():
    if concount == 0:
        print("Starting Thread")
#        thread = MqttThread()
        thread = FileThread()
        thread.start()
    concount += 1



@socketio.on('disconnect', namespace='/sl')
def ws_disconn():
    global concount
    concount -= 1
    c = concount
#    socketio.emit('msg', {'count': c}, namespace='/sl')



if __name__ == '__main__':
#    app.run(host=host, port=port)
    socketio.run(app, host=host, port=port, debug=True)
