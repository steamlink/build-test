# SteamLink

This document includes a high-level summary of project goals, protocol, and implementation.

## Introduction

SteamLink is a low-power netwoking platform. The SteamLink vision is to give makers the ability to rapidly and easily deploy fully self-hosted, robust, and secure networks of low-powered devices. Potential applications include data-driven citizen science projects and automation systems.

Currently the SteamLink project plans to support LoRa and WiFi devices.

**WARNING:** SteamLink is in active development and the protocol is subject to change. The current codebase has limited security features implemented and some portions are untested. Use at your own risk.

### Ecosystem Layout

```
                                                      +-----------+
                                        +-----------> | WiFi Node |
            +----------+                |             +-----------+
     +----> | Database |                |
     |      +----------+                |
     v                                  v
+---------------+              +-------- ---------+
|               |              |                  |
|               |              |                  |       +-------+-------+       +-----------+
|               |              |                  | <---> |  WiFi | LoRa  | <---> | LoRa Node |
|     Store     | <----------> |    MQTT Broker   |       +-------+-------+       +-----------+
|               |              |                  |             BRIDGE
|               |              |                  |
|               |              |                  |
+---------------+              +------------------+
      ^
      |         +-----------+
      |         |  Web      |
      +-------> |  Console  |
                +-----------+
```
_Made using [this ascii editor](http://asciiflow.com/)_

### Hardware Supported

The SteamLink client arduino library have currently been tested on:

1. Adafruit LoRa featherboards
2. ESP8266
3. ESP32

The store is implemented in Python and most unix like systems should be able to support it.

## Components of the SteamLink Network

A SteamLink network comprises:

### 1. SteamLink Nodes

SteamLink Nodes can be thought of as "clients" in a SteamLink network. There are two types of nodes:
- Standard nodes: These nodes have a single physical interface to send and receive packets. As of the current SteamLink version, traffic from the store terminates at a standard node.
- Bridge node: These nodes have two physical interfaces and can pass packets from one physical interface to another. 

### 2. SteamLink Store

The SteamLink store can be thought of as the "backend" of a SteamLink network. The SteamLink store provides the database, web-console, and a default message brokering facility for the network.

**NB:** All nodes in a SteamLink network have a unique SteamLink ID or `slid` for short. Bridge nodes have two `slid`  

## Packet Flow

The SteamLink packet-flow is in two directions: "node -> store" and "store -> node". All packets include a header followed by a payload.

### 1. `data`: node -> store 

`data` packets as they usually contain sensor data and status messages.

#### Data MQTT Channel
Any node that can communicate directly to MQTT can send its data messages to: `SteamLink/<slid>/data`

#### Data Header format

| Field         | Bits   |  Description                                                |
| ------------- |-------:| :----------------------------------------------------------:|
| `op`          |       8| Data packet op code (see below)                             |
| `slid`        |      32| SteamLink ID of the node creating the packet                |
| `pkg_num`     |      16| Packet number/count set by the node creating the packet     |
| `rssi`        |       8| Describes the signal strength of the previous hop           |
| `qos`         |       8| Quality of Service for the packet                           |

Data packets look like:
```
+----+------+---------+------+-----+-----------+
| op | slid | pkg_num | rssi | qos | payload...|
+----+------+---------+------+-----+-----------+
```
#### Data Packet OP codes:

**NB** Data message op codes have an odd bottom bit

| OP code         | Hex  | Type   |  Description                                                |
| ------------- |-------:|:------:| :----------------------------------------------------------:|
| `DS`          |    0x31| USER   | User packet to store                                        |
| `BS`          |    0x33| ADMIN  | Bridge data to store                                        |
| `ON`          |    0x35| ADMIN  | Node sign on packet                                         |
| `AK`          |    0x37| ADMIN  | Node acknowledgement packet                                 |
| `NK`          |    0x39| ADMIN  | Node negative acknowldegement packet                        |
| `TR`          |    0x3B| ADMIN  | Test packet received, payload = received test data          |
| `SS`          |    0x3D| ADMIN  | Sending status packet, payload = status message             |

### 2. `control`: store -> node

`control` packets usually contain messages to configure and command the nodes.

#### Control MQTT Channel
Any node that can communicate directly to MQTT receives its control messages on: `SteamLink/<slid>/control`

#### Control Header format

| Field         | Bits   |  Description                                                |
| ------------- |-------:| :----------------------------------------------------------:|
| `op`          |       8| Control packet op code (see below)                          |
| `slid`        |      32| SteamLink ID of the destination node of the packet          |
| `pkg_num`     |      16| Packet number/count set by the node creating the packet     |
| `qos`         |       8| Quality of Service for the packet                           |

Control packets look like:
```
+----+------+---------+-----+-----------+
| op | slid | pkg_num | qos | payload...|
+----+------+---------+-----+-----------+
```
#### Control Packet OP codes:

**NB** Control message op codes have an even bottom bit

| OP code         | Hex  | Type   |  Description                                                |
| ------------- |-------:|:------:| :----------------------------------------------------------:|
| `DN`          |    0x30| USER   | User packet to node                                         |
| `BN`          |    0x32| ADMIN  | Bridge paclet to node                                       |
| `GS`          |    0x34| ADMIN  | Get status from node                                        |
| `TD`          |    0x36| ADMIN  | Transmit a test packet via radio                            |
| `SR`          |    0x38| ADMIN  | Set radio parameters from payload and send AK               |
| `BC`          |    0x3A| ADMIN  | Boot cold: Restart node, no AK expected                     |
| `BR`          |    0x3C| ADMIN  | Reset radio: acknowledge with AK status message             |


### Packet Decision Tree

A node has two possible receive interfaces: 

1. PHY interface: ALWAYS USED.
2. Bridge interface: USED ONLY IF THE NODE BEHAVES AS ONE SIDE OF A BRIDGE.



