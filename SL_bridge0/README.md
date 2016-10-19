SL\_bridge
============

## Bridge between Lora and MQTT


## Examples
 #  pkt transmission from mqtt to a node:

mosquitto\_pub  -h mqtt.steamlink.net -i control0 -u UUUUUU -P PPPPPP  -t UUUUUU/bridge0/config/node -m "1,10,test1234"

	- UUUU is the username
	- PPPP is the password
	- msg has 3 fields toaddr, datalength, data
