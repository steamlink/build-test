#!/usr/local/bin/python3

import time
from steamclient import SteamLink

def callback(buf):
	global din
	din = buf
	print("we got %s" % buf)

SL_TOKEN = "3ca5de6fa904467e4e66f1fc8e6f54bd1400000000c064440005"

din = "run"
sl = SteamLink(SL_TOKEN, "mesh_py")
sl.register_handler(callback)
sl.send({'key': 'python test'})
while din != "quit":
	time.sleep(1)
	sl.send({'time': time.asctime()})
sl.shutdown()

