import paho.mqtt.client as mqtt
import time
import sys

from Crypto.Cipher import AES
import binascii

key = b'2b7e151628aed2a6abf7158809cf4f3c'
bkey = binascii.unhexlify(key)


def hexprint(pkt):
	for c in pkt:
		print "%02x" % ord(c),
	print

def AES128_decrypt(pkt):

	if len(pkt) % 16 != 0:
		print "pkt len error: ", len(pkt)
		hexprint(pkt)
		return ""
	decryptor = AES.new(bkey, AES.MODE_ECB)
	return decryptor.decrypt(pkt)

def AES128_encrypt(msg):
	if len(msg) % 16 != 0:
		pmsg = msg + "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"[len(msg) % 16:]
	else:
		pmsg = msg
	encryptor = AES.new(bkey, AES.MODE_ECB)
	return bytearray(encryptor.encrypt(pmsg))

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
	print("Connected with result code "+str(rc))

	# Subscribing in on_connect() means that if we lose the connection and
	# reconnect then subscriptions will be renewed.

	if len(sys.argv) > 1:
		for topic in sys.argv[1:]:
			client.subscribe(topic)
			print "Subscribing %s" % topic
	else:
		topic = "#"
		client.subscribe(topic)
		print "Subscribing %s" % topic



# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
#	ts=time.strftime("%Y-%m-%d %H:%M:%S",time.localtime())
	ts=time.strftime("%Y-%m-%d %H:%M:%S",time.localtime(msg.timestamp))
	if msg.topic in ["SL/mesh_1/data", "SL/mesh_1/control"]:
		try:		# rough decode B_TYP_VER 0
			nodever = ord(msg.payload[0])
			fromaddr = ord(msg.payload[1])
			rssi = ord(msg.payload[2]) - 256
			sw_id = ord(msg.payload[3])
			pl = AES128_decrypt(msg.payload[4:]).rstrip('\0')
		except Exception as e:
			print "payload format or decrypt error: %s" % e
			return
		print("%s: %s from:%s rssi %s len %s data: %s" %(ts,msg.topic, \
			fromaddr, rssi, len(pl), pl))
		m = pl.split()
		if m[0] == "Button":
			state = int(m[1])
			pkt = AES128_encrypt("%s" % state)
			print "pkt len %s" % len(pkt)
			spkt = bytearray("%c%c%c%c" % (0,fromaddr,0,0)) + pkt 
			client.publish("SL/mesh_1/control", spkt)
	else:
		print("%s: %s payload %s" % (ts,msg.topic, msg.payload))
# like 2016-10-21 16:56:24: SL/mesh_1/data   1, -33|Button 1!  pkt: 67	
#		m = pl.split(',',2)
#		if m[2][:6] == "Button":
#			addr = int(m[0])
#			r = m[2][7:].split('!',1)
#			state = int(r[0])
#			client.publish("SL/mesh_1/config/node","%s,%s" % (addr, state))
#
# Main
#
fullFlag = True

client = mqtt.Client(client_id="py_look")
# client.tls_set("/usr/local/etc/mosquitto/CA/ca.crt")
client.tls_set("/home/steamlink/auth/mqtt/ca.crt")
client.username_pw_set("andreas","1foot123")
client.on_connect = on_connect
client.on_message = on_message

client.tls_insecure_set(False)
client.connect("mqtt.steamlink.net", 8883, 60)


try:
	client.loop_forever()
except KeyboardInterrupt:
	sys.exit(0)

