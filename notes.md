
## Node functions
* Can send and receive from upstream (bridge --> store)
* `send()`
  + `encrypt()`
  + `insert_sw_id()` TODO
  + `RH_MESH::send()`
* `register_handlers(on_receive, on_send_error)`
* `update()`
  + `RH_MESH::receive()`
  + `check_sw_id()` TODO
  + `decrypt()`
  + `on_receive()`

## Bridge functions
* Bridge is unique to a mesh, running at address 1
* Backup bridge addresses are 2 and 3, so other addresses can range between [4, 255]
* Bridge sends the data to the topic: `SL/mesh_{x}/data`
  + The value pubished pads `node_addr` as the first byte
  + The values also pads the RSSI from the last hop on the LoRa mesh as the second byte
* `send()`
* `receive()`
* bridge registers `SL_RingBuff::enqueue` `on_receive()`
* `mqtt_publish()`
* `mqtt_listen()`
* TODO: upgrade to standard steamlink library

## Receive Handler functions (what bridgetest.py will evolve to)
* The `receive_handler` includes an MQTT client that decrypts the data and stores in MongoDB and publishes the value to MQTT topic that looks like: `{user_name}/{swarm_name}/{node_addr}/data`
* The user has to make the following associations:
  + `user_name` owns multiple `swarm_id`
  + each `swarm_id` has a human readable `swarm_name` chosen by the user
  + each `swarm_id` has an AES `key`
  + each `swarm_id` owns mutiple `node_addr` in a single mesh (must be the same mesh because the token encodes radio information)
* The notion of a `swarm` is multiple nodes with identical binary images. The `node_addr` will need to be configured dynamically in a `swarm`

### pseudo code
* `store`  loop
  + `mqtt_on_message()` from node via bridge
  + `decrypt()`
  + `rekey()`
  + `add_to_mongo()`
* `control` loop
  + `read_from_mongo()`
  + `encrypt`
  + `key()`
  + `mqtt_bridge_publish()` to node via bridge
* `public_publish` loop
  + `read_from_mongo()`
  + `transform(topic_in, topic_out, operation)`
    + `topic_to_mongo_key`
  + `mqtt_public_publish()` public mqtt interface

# Packet formats
```
node to bridge
n_typ 0 (encoded as flags 0000)
+-----+---------------------+
|sw_id|encrypted_payload....|
+-----+---------------------+

bridge to store on topic "SL/mesh_ID/data"
b_typ 0
+-----+-----+-------+----+-----+--------------------+
|b_typ|n_typ|node_id|rssi|sw_id|encrypted_payload...|
+-----+-----+-------+----+-----+--------------------+
|<-----BRIDGE_WRAP------>|<----NODE_PKT----------->>|

store to bridge like bridge to store, with rssi = 0

bridge to node like node to bridge

------- ----------- ---------
sw_id   swarm id    16bit
b_typ   bridge typ  4 bit
b_typ   node typ    4 bit
node_id RH addr     8 bit
rssi    signal      8 bit

```
# Eve schema
* swarms
  + `swarm_id`
  + `swarm_name`
  + `field_names` optional
  + `swarm_user_id`
  + `node_id_array`
  + `mesh_id`
  + `radio_params`
  + `swarm_aes_key`
* bridges
  + `mesh_id`
  + `node_addr`
  + `nodes_seen_array` TODO
* users
  + `user_id`
  + `user_name` must be unique
  + `password`
  + `email`
  + `swarm_id_array`
  + `web_token`
  + `mqtt_password` - same as above for now
  + `topic_transformations_array`
* topic_transformations
  + `topic_in`
  + `topic_out`

# Alias tranformations
## Simple alias
`user_name/swarm_name/node_addr` --> `toronto/steamlabs/temperature`
## Many to one alias
`toronto/+/temperature` --> `toronto/temperature`


# TODOS
* bridges
  + bridges are nodes too, allocate a node record and make it's address 1
  + fix bridge_token it needs the mesh_id


* packet formats
  + add a pkt counter in each direction so nodes/stores can know if pkts were lost
