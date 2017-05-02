#ifndef STEAMLINK_H
#define STEAMLINK_H

#include <Arduino.h>

typedef void (*on_receive_handler_function)(uint8_t* buffer, uint8_t size); // user
typedef void (*on_receive_bridge_handler_function)(uint8_t* buffer, uint8_t size, uint32_t slid, uint8_t flags, uint8_t rssi); // bridge

#define  MIN(a,b) (((a)<(b))?(a):(b))
#define  MAX(a,b) (((a)>(b))?(a):(b))

// DEBUG INSTRUCTIONS:
//     Change below line or comment out to change the level of debugging
//     Make sure Serial is defined if DEBUG is enabled
//     Debugger uses Serial.println() to debug
#define DEBUG_ENABLED DEBUG_LEVEL_INFO

#define DEBUG_LEVEL_INFO   4
#define DEBUG_LEVEL_WARN   3
#define DEBUG_LEVEL_ERR    2
#define DEBUG_LEVEL_FATAL  1
#define DEBUG_LEVEL_NONE   0

#if DEBUG_ENABLED >= DEBUG_LEVEL_INFO
 #define INFO(text) Serial.print("SL_INFO   : ");Serial.println(text)
#else 
 #define INFO(text) ((void)0)
#endif

#if DEBUG_ENABLED >= DEBUG_LEVEL_WARN
 #define WARN(text) Serial.print("SL_WARNING   : ");Serial.println(text)
#else 
 #define WARN(text) ((void)0)
#endif

#if DEBUG_ENABLED >= DEBUG_LEVEL_ERR
 #define ERR(text) Serial.print("SL_ERROR   : ");Serial.println(text)
#else 
 #define ERR(text) ((void)0)
#endif

#if DEBUG_ENABLED >= DEBUG_LEVEL_FATAL
 #define FATAL(text) Serial.print("SL_FATAL   : ");Serial.println(text)
#else 
 #define FATAL(text) ((void)0)
#endif

#endif
