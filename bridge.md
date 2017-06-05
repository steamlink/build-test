# SteamLink Admin and Control 


A SteamLink network implements an admin/control channel, for the operation of bridges and routers, and to handle data traffic. It is accessible at the MQTT broker, typically through the topics `SL/<slid>/admin_control` and `SL/<slid>/admin_data`.

The `admin_control` topic is used for messages to leaf- or bridge- nodes, the `admin_data` topic is used by those nodes to return messages back to the originator of the `admin_control` messages. The `<slid>` is the unique id of the node a control messages is indended for. Note that the node returns (publishes) it's `admin_data` response with it's own `<slid>`. For example, node 257 will listen for messages on `SL/257/admin_control` and publish it's reposnses on `SL/257/admin_data`.

The payload format of admin and control messages consists of a fixed length header, optionally followed by a opcode dependent variable length data.

## Packet Types

All nodes send and receive packets with the following parameters:

1. `slid`, 32 bits: `admin_data` (ie. publish) packets contain the slid of the publishing node. `admin_control` packets contain the slid of the destination node
3. `rssi`, 8 bits: received signal strength indicator or `rssi` describes the signal strength at the previous hop
2. `op`, 8 bits: opcode or `op` is the control instruction for the destination
4. `QoS`, 8 (2 + 6) bits: 2 bits are QoS followed by 6 bit packet count per slid
5. `payload`: payload of the packet

### Packets to and from primary nodes and bridges

Packets to primary nodes and bridges have the following packet format.

```
+------+------+----+-----+----------------+
| slid | rssi | op | qoS |      payload...|
+------+------+----+-----+----------------+
```
In some cases when encryption is required, both the `op` and the `payload` is encrypted.


SteamLink relies on "source-routing". True source routing happens for store-to-node. Node-to-store publishing A node (or store) wanting to deliver a message to a distant node builds a packet by encapsulation of multiple messages. The op code is used by each receiving node to determine its action.

#### Node

Radio node (SteamLinkLora, etc) transmit messages received via the `admin_send` function to the designated destination in a manner that enables the receiving node to identify the messages as an admin messages. For example, SteamLinkLora uses one of the flag fields in the Lora packet header to indicate `admin` message to the receiver. Any messages received such is delivered up via a admin_receive callback.


#### Control Opcodes

The bridge listens for an acts on the following opcodes:
 
 - ##### Data
``DN<packet>``        data to a node

- ##### Bridge
``BN<packet>``        bridge data to a node, can be multi-level encapsulated

- ##### Status
``GS`` get status, reply with `SS` message

- ##### Transmit Test Data
``TDdata_to_send`` transmit a test message via radio, acknowlegde with AK or NK (if failed)

- ##### Set Transmission mode
``SRx`` set transmission paramter to x, acknowlegde with AK or NK

- ##### Restart
``BC`` restart node, no reply

- ##### Reset Transmitter
``BR`` reset the transmitter, acknowlegde with AK or NK


#### Data Opcodes

Data messages are sent by the bridge, usually in reply to a control message.

- ##### Data
``DS<packet>`` data to the store

- ##### Bridge
``BS<packet>`` Bdige data to the store, can be multi-level encapsulated

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
  
  
