# -*- coding: utf-8 -*-

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

app = Eve()
app.template_folder = tmpl_dir
app.static_folder = static_dir

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/add-transform')
def add_transform():
    return render_template('add_transform.html')

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

if __name__ == '__main__':
    app.run(host=host, port=port)
