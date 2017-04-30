# SteamLink Bridge 

SteamLink bridges should support the following message on the `<sl>/../admin_control` and `<sl>/../admin_data` topics,  `<sl>` denotes the topic prefix, ususally `SteamLink`. Each message is a simple record with the first 2 ascii characters denoting the record type. 

## Control Messages

Control messages are sent to the bridge. Most control messages will require the brigde to send a reply message on the `<sl>/../admin_data` topic.

- #### Status
``GS`` get status, reply with `SS` message

- #### Transmit Test Data
``TDdata_to_send`` transmit a test message via radio, acknowlegde with AK or NK (if failed)

- #### Set Radio mode
``SRx`` set radio paramter to x, acknowlegde with AK or NK

- #### Restart
``BC`` restart node, no reply

- #### Reset Radio
``BR`` reset the radio, acknowlegde with AK or NK


## Data Messages

Data messages are sent by the bridge, usually in reply to a control message.

- #### Boot
``ON`` send on startup

- #### Acknowlegde
``AK``  acknowlegde the last control message of type ``TD``, ``SR``, ``BC``, or ``BR``.

- #### Negative Acknowlegde
``NK``  negative acknowledge the last control message of type ``TD``, ``SR``, ``BC``, or ``BR``.

- #### Received Test Data
 ``TRrssi,data`` rssi and test message received via radio
 
- ### State
``SSnewstatus,slsent,slreceived,mqttsent,mqttreceived,mqttQ,LoraQ
``
    - newstatus is one of Online, Resetting, OFFLINE
    - slsent, slreceived are the counts of radio packets sent and received
    - mqttsend, mqttreceived are the counts of mqtt packets send and received
    - mqttQ, loraQ are the number of messages waiting in the respective queues
 
 -
 #
 Note that Test Data messages should be sent without interfereing with regular data messages. For LoRa radios, this is acomplished by setting on of the packet flags as a test message indicator.
 