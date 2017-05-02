#ifndef STEAMLINK_H
#define STEAMLINK_H

#include <Arduino.h>

// DEBUG INSTRUCTIONS:
//     Comment out below line to disable debugging
//     Make sure Serial is defined if DEBUG is enabled
//     Debugger uses Serial.println() to debug
#define SL_DEBUG_ENABLED // ENABLE OR DISABLE DEBUGGING

typedef void (*on_receive_handler_function)(uint8_t* buffer, uint8_t size); // user
typedef void (*on_receive_bridge_handler_function)(uint8_t* buffer, uint8_t size, uint32_t slid, uint8_t flags, uint8_t rssi); // bridge

#define  MIN(a,b) (((a)<(b))?(a):(b))
#define  MAX(a,b) (((a)>(b))?(a):(b))


#ifdef SL_DEBUG_ENABLED
  #define INFO(text)  Serial.print("SL_INFO   : ");    Serial.println(text)
  #define WARN(text)  Serial.print("SL_WARNING: ");    Serial.println(text)
  #define ERR(text)   Serial.print("SL_ERROR  : ");    Serial.println(text)
  #define FATAL(text) Serial.print("SL_FATAL  : ");    Serial.println(text)
#else
  #define INFO(text)  ((void)0)
  #define WARN(text)  ((void)0)
  #define ERR(text)   ((void)0)
  #define FATAL(text) ((void)0)
#endif

#endif
