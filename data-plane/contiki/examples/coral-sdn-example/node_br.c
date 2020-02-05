/**
 * \file
 *         BR.c Main CORAL mote source code
 * \author
 *         Tryfon Theodorou <tryfonthe@gmail.com>
 */
 
#include "net/coral-sdn/coral.h"
#include "net/coral-sdn/coral_br.c"

#include "sys/ctimer.h"
#include "lib/list.h"
#include "lib/memb.h"


#define MESSAGE "Hello"

//#define SENDAUTO 1  // Sending
#define SENDTOSINK 1 // For mobile experiments
//#define METRICS 1
//#define PP 1 // For Peer to Peer 
#define NET_NODES 30   // ????????????????????/ SOS change per simulation
static int toNode =  1;

#ifndef PERIOD
#define PERIOD 40    //my experiments with 120  (60=1min, 600=10min, 120=2min)
#endif

#define START_INTERVAL     (60 * CLOCK_SECOND)  // Wait a minute befor you start
#define SEND_INTERVAL		(PERIOD * CLOCK_SECOND)
#define SEND_TIME		  		(random_rand() % (SEND_INTERVAL))
#define MAX_PAYLOAD_LEN	    80

/*---------------------------------------------------------------------------*/
PROCESS(main_coral_process, "CORAL Border Router Main Process");
AUTOSTART_PROCESSES(&main_coral_process);
/*---------------------------------------------------------------------------*/


// Connection structs
struct mesh_conn mc;


/*============================================================================*/
/* MESH CALLBACK */
static void sent(struct mesh_conn *c){
  PRINTF("packet sent\n");
}

static void timedout(struct mesh_conn *c){
  PRINTF("packet timedout\n");
}

static void recv(struct mesh_conn *c, const linkaddr_t *from, uint8_t hops){
  if(LongRangeReceiving > 0){
    PRINTF("LONG====\n");
  } else{
    PRINTF("SHORT====\n");
  }
   printf("#RECV %d from %d hops %d\n",node_id,from->u8[0]+from->u8[1], hops);
   //printf("Msg from Node: %d\n",from->u8[0]+from->u8[1]);
	printf("Data received from %d.%d: %.*s (%d)\n",
         from->u8[0], from->u8[1],
         packetbuf_datalen(), (char *)packetbuf_dataptr(), packetbuf_datalen());
   d_recv++; //STAT increase received data packet

   printf("{\"NID\":\"%02d.00\",\"PTY\":\"DT\",\"SID\":\"%02d.%02d\",\"PLD\":\"%s\"}\n",
          node_id, from->u8[0], from->u8[1],  // Sender Address
          (char *)packetbuf_dataptr()         // Send Message Payload
          );  // PRINT TO UART (Send message to controler)

  //packetbuf_copyfrom(MESSAGE, strlen(MESSAGE));
  //mesh_send(&mc, from);   // send ack
}
/*----------------------------------------------------------------------------*/
const static struct mesh_callbacks callbacks = {recv, sent, timedout};
/*----------------------------------------------------------------------------*/

/*============================================================================*/
/* MAIN PROCESS */
PROCESS_THREAD(main_coral_process, ev, data){
   PROCESS_EXITHANDLER(mesh_close(&mc);)

   PROCESS_BEGIN();
   PRINTF("Initializing CORAL-SDN Border Router...\n");
	coral_init();

   PRINTF("Starting MESH listener...\n"); 
   mesh_open(&mc, DT_CHANNEL, &callbacks);

  
   PRINTF("\nBorder Router Main process event...\n");    
   SENSORS_ACTIVATE(button_sensor);

   static int count=0;
   char message[MAX_PAYLOAD_LEN];
   static struct etimer start_timer;
   static struct etimer periodic_timer;
  	static struct etimer send_timer;
#ifdef ZOUL
	int16_t temperature, humidity;
#endif

  	etimer_set(&start_timer, START_INTERVAL);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&start_timer));

  	etimer_set(&periodic_timer, SEND_INTERVAL);
   linkaddr_t addr;

   while(1) {
#ifdef SENDAUTO
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
		etimer_reset(&periodic_timer);
		etimer_set(&send_timer, SEND_TIME);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&send_timer));
		PRINTF("Sending Timer off\n");
#else
      /* Wait for button click before sending the first message. */
      PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event && data == &button_sensor);
      PRINTF("Button clicked sending message\n");
#endif

#ifdef ZOUL
    	if(dht22_read_all(&temperature, &humidity) != DHT22_ERROR) {
		   sprintf(message,"ID:%03d|Temp:%02d.%02dºC|Humid:%02d.%02dRH",count, 
				temperature / 10, temperature % 10, 
				humidity / 10, humidity % 10);
    	}
      sprintf(message,"%s %d",MESSAGE,count);
#endif

#ifdef PP
      if(toNode>NET_NODES){
         toNode = 1;
         count++;
      }
      if(toNode != node_id && count < 25){ // Avoid self messages
         sprintf(message, "Hello %d from the client", count);
	      addr.u8[0] = toNode;
	      addr.u8[1] = 0;
	      packetbuf_copyfrom(message, strlen(message));
         mesh_send(&mc, &addr);
         d_send++; // STAT send data packet 
         printf("#SEND %d to %d\n",node_id,toNode);
      }
      toNode++;
#endif

#ifdef SENDTOSINK
      /* Send a message to node number 1. */
		if(node_id != 1){  //If not sink
		   count++;
		   sprintf(message,"%d,%s",count,MESSAGE);
		   PRINTF("Sending message: %s\n",message);
		   packetbuf_copyfrom(message, strlen(message));
			//dual_radio_switch(SHORT_RADIO); //Dual set short
		   //addr.u8[0] = 1;
		   //addr.u8[1] = 0;
		   // mesh_send(&mc, &addr);
			d_send++; // STAT send data packet 
		}
#endif
   } // End while

   PROCESS_END();
}


