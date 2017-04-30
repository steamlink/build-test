//
#ifndef STEAMLINK_H
#define STEAMLINK_H

#include <Arduino.h>

typedef void (*on_receive_handler_function)(uint8_t* buffer, uint8_t size); // user
typedef void (*on_receive_bridge_handler_function)(uint8_t* buffer, uint8_t size, uint32_t slid, uint8_t flags, uint8_t rssi); // bridge

#endif
