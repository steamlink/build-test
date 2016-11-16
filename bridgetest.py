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
	decryptor = AES.new(bkey, AES.MODE_ECB);
	return decryptor.decrypt(pkt)


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
	if msg.topic == "SL/mesh1/data":
		try:
			fromddr = ord(msg.payload[0])
			rssi = ord(msg.payload[1]) - 256
			pl = AES128_decrypt(msg.payload[2:]);
		except Exception as e:
			print "payload format or decrypt error: %s" % e
			return
		print("%s: %s from:%s rssi %s len %s data: %s" %(ts,msg.topic, \
			fromddr, rssi, len(pl), pl))
# like 2016-10-21 16:56:24: SL/mesh1/data   1, -33|Button 1!  pkt: 67	
#		m = pl.split(',',2)
#		if m[2][:6] == "Button":
#			addr = int(m[0])
#			r = m[2][7:].split('!',1)
#			state = int(r[0])
#			client.publish("SL/mesh1/config/node","%s,%s" % (addr, state))
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

