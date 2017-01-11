# -*- coding: utf-8 -*-
from gevent import monkey
monkey.patch_all()

import paho.mqtt.client as mqtt
from threading import Thread, Event

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
        self.client.on_message = self.on_message

        self.client.tls_insecure_set(True)
        self.client.connect("mqtt.steamlink.net", 1883, 60)

    def runmqtt(self):
        #infinite loop of polling mqtt
        self.client.loop_forever(timeout=1.0, max_packets=1, retry_first_connection=False)

    def on_connect(self, client, userdata, flags, rc):
        print("Connected with result code "+str(rc))

        # Subscribing in on_connect() means that if we lose the connection and
        # reconnect then subscriptions will be renewed.

        for topic in ["SL/+/status"]:
            client.subscribe(topic)
            print("Subscribing %s" % topic)

    def on_message(self, client, userdata, msg):
        msg = "%s: %s" % (msg.topic, msg.payload.decode().strip('\0').replace('/',' |  '))
        socketio.emit('msg', {'count': msg}, namespace='/sl')


    def run(self):
        self.runmqtt()


app = Eve()
app.template_folder = tmpl_dir
app.static_folder = static_dir
socketio = SocketIO(app, async_mode=async_mode)


def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.

    for topic in ["SL/+/status"]:
        client.subscribe(topic)
        print("Subscribing %s" % topic)

def on_message(client, userdata, msg):
    socketio.emit('msg', {'count': msg.payload}, namespace='/sl')


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

@app.route('/status')
def pymeetups():
    return render_template('status.html', async_mode=socketio.async_mode)

@socketio.on('connect', namespace='/sl')
def ws_conn():
    global concount, thread
    concount += 1
    c = "Waiting"
    socketio.emit('msg', {'count': c}, namespace='/sl')
    if not thread.isAlive():
        print("Starting Thread")
        thread = MqttThread()
        thread.start()



@socketio.on('disconnect', namespace='/sl')
def ws_disconn():
    global concount
    concount -= 1
    c = concount
#    socketio.emit('msg', {'count': c}, namespace='/sl')



if __name__ == '__main__':
#    app.run(host=host, port=port)
    socketio.run(app, host=host, port=port, debug=True)
