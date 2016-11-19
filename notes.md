#
* Bridge is unique to a mesh, running at address 1
* Backup bridge addresses are 2 and 3, so other addresses can range between [4, 255]
* Bridge sends the data to the topic: `SL/mesh_{x}/data`
  + The value pubished includes `node_addr` as the first byte
  + The values published includes the RSSI from the last hop on the LoRa mesh as the second byte
  + This is because of a problem with the Adafruit library where we're limited to only 5 topics
* The `receive_handler` includes an MQTT client that decrypts the data and stores in MongoDB and publishes the value to MQTT topic that looks like: `{user_name}/{swarm_name}/{node_addr}/data`
* The user has to make the following associations:
  + `user_name` owns multiple `swarm_name`
  + each `swarm_name` has an AES `key`
  + `swarm_name` owns mutiple `node_addr`
* The notion of a `swarm` is multiple nodes with identical binary images. The `node_addr` will need to be configured dynamically

## Error codes

## Node
* `send()`
  + `encrypt()`
  + `RH_MESH::send()`
* `register_handlers(on_receive, on_send_error)`
* `update()`
  + `RH_MESH::receive()`
  + `decrypt()`
  + `on_receive()`

## Bridge
* `send()`
* `receive()`
* bridge registers `SL_RingBuff::enqueue` `on_receive()`
* `mqtt_publish()`
* `mqtt_listen()`

## Store
* `store`  loop
  + `mqtt_on_message()` from node via bridge
  + `rekey()`
  + `decrypt()`
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

