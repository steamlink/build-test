# SteamLink Admin and Control 

A Stemlink network includes an out-of-band admin/control channel, to facilitate the opeation of bridges and routers, and to allow status and test traffic without interfering with the regular data traffic. The admin and control channel is implemented as part of the bridge code. It is accessible at the MQTT broker, typically through the topics `SL/<slid>/admin_control` and `SL/<slid>/admin_data`.

The `control` topic is used for messages to leaf- or bridge- nodes, the `data` topic is used by those nodes to return messages back to the originator of the `control` messages. The `<slid>` is the unique id of the node a control messages is indended for. Note that the node returns (publishes) it's `data` response with it's own `<slid>`. For example, node 257 will listen for messages on `SL/257/admin_control` and publish it's reposnses on `SL/257/admin_data`.

The payload format of admin and control messages consists of a fixed length header, optionally followed by a opcode dependent variable length data.

The header has the following fields:

- 32 bit slid (network byte order)
- 8 bit opcode 
- 8 bit signal strength

#### Routing and Bridging

Bridging (and later routing) is accomplished via encapsulation. A node (or store) wanting to deliver a message to a distant node encapsulates the final message in a bridge packet and tansmits it using a `BN` (see below) opcode to the first hop.

#### Node

Radio node (SteamLinkLora, etc) transmit messages received via the `admin_send` function to the designated destination in a manner that enables the receiving node to identify the messages as an admin messages. For example, SteamLora uses one of the flag fields in the Lora packet header to indicate `admin` message to the receiver. Any messages received such is delivered up via a admin_receive callback.


#### Control Opcodes

The bridge listens for an acts on the following opcodes:
 
- ##### Bridge
``BN``	bridge data to a node

- ##### Status
``GS`` get status, reply with `SS` message

- ##### Transmit Test Data
``TDdata_to_send`` transmit a test message via radio, acknowlegde with AK or NK (if failed)

- ##### Set Radio mode
``SRx`` set radio paramter to x, acknowlegde with AK or NK

- ##### Restart
``BC`` restart node, no reply

- ##### Reset Radio
``BR`` reset the radio, acknowlegde with AK or NK


#### Data Opcodes

Data messages are sent by the bridge, usually in reply to a control message.

- ##### Bridge
``BS`` Bdige data to the store

- ##### Boot
``ON`` send on startup

- ##### Acknowlegde
``AK``  acknowlegde the last control message of type ``TD``, ``SR``, ``BC``, or ``BR``.

- ##### Negative Acknowlegde
``NK``  negative acknowledge the last control message of type ``TD``, ``SR``, ``BC``, or ``BR``.

- ##### Received Test Data
 ``TRrssi,data`` rssi and test message received via radio
 
- #### State
``SSnewstatus,slsent,slreceived,mqttsent,mqttreceived,mqttQ,LoraQ
``
    - newstatus is one of Online, Resetting, OFFLINE
    - slsent, slreceived are the counts of radio packets sent and received
    - mqttsend, mqttreceived are the counts of mqtt packets send and received
    - mqttQ, loraQ are the number of messages waiting in the respective queues
 
  