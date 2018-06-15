/**
 * \file
 *         coral.h Main CORAL mote source code
 * \author
 *         Tryfon Theodorou <tryfonthe@gmail.com>
 */
  
#ifndef CORAL_H_
#define CORAL_H_

#include "contiki.h"
#include <stdio.h>
#include <stdlib.h>
#include "dev/serial-line.h"  /* ??? maybe I can remove */
#include "net/rime/rime.h"
#include "mesh.h"
#include "route.h"

#include "net/linkaddr.h"  

#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "random.h"
#include "lib/memb.h"

#define DEBUG 0
#if DEBUG 
   #define PRINTF(...) printf(__VA_ARGS__)
#else
   #define PRINTF(...)
#endif

#ifdef Z1
	#include "dev/uart0.h"  // uart0 for zolertia
	#include "node-id.h"
#elif defined SKY
	#include "dev/uart1.h"  // uart1 for sky
	#include "node-id.h"
#elif defined ZOUL
	#include "dev/uart1.h"  // uart1 for sky
	#include "dev/dht22.h"  // Temperature and Humidity Sensor
	int node_id=2;
#elif defined RM090
	#include "node-id.h"
#endif

#define UART_BUFFER_SIZE      80 
#define MAX_RETRANSMISSIONS   10  
 
#define BC_CHANNEL 100     // Broadcast Channel
#define UC_CHANNEL 101     // Unicast Channel 
#define RUC_CHANNEL 144    // Reliable Channel

 #endif
