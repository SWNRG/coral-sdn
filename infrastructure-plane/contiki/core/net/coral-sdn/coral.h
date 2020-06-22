/**
 * \file
 *         coral.h Main CORAL-SDN header file source code
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
#include "chameleon-bitopt.c" /* ??? maybe I can remove */
#include "dual_conf.h"

#include "mesh.h"
#include "mesh.c"
#include "route.h"

#include "net/linkaddr.h"  

#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "random.h"
#include "lib/memb.h"


//#define METRICS 1  //Enable to print statistics 

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
#elif defined CORAL_COOJA  // Serial for Cooja
   #include "dev/rs232.h"
   #include "dev/serial-line.h"
	#include "node-id.h"
#elif defined ZOUL
	#include "dev/uart1.h"  // uart1 for sky
	#include "dev/dht22.h"  // Temperature and Humidity Sensor
	int node_id=2;
#elif defined RM090
	#include "node-id.h"
	#include "dev/serial-line.h"
#endif


#define UART_BUFFER_SIZE      95 
#define MAX_RETRANSMISSIONS   6  

#define LR_U_SEND_EVENT     51 // Long Range
#define SR_B_SEND_EVENT     52 // Short Range

#define DT_CHANNEL 99      // Data Unicast Channel 
#define BC_CHANNEL 100     // Broadcast Channel
#define UC_CHANNEL 101     // Unicast Channel 
#define RUC_CHANNEL 85     // Reliable Channel

 #endif
