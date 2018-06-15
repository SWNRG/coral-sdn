/**
 * \file
 *         coral.c Main CORAL mote source code
 * \author
 *         Tryfon Theodorou <tryfonthe@gmail.com>
 */
 
#include "coral.h"

#ifdef RM090
	#include "dev/serial-line.h"
#endif

#define RF_U_SEND_EVENT     51
#define RF_B_SEND_EVENT     52

#define MESSAGE "Hello"

#define SENDAUTO 1  // Sending

#ifndef PERIOD
#define PERIOD 600
#endif

#define SEND_INTERVAL		(PERIOD * CLOCK_SECOND)
#define SEND_TIME		  		(random_rand() % (SEND_INTERVAL))
#define MAX_PAYLOAD_LEN	    128

/*---------------------------------------------------------------------------*/
PROCESS(main_coral_process, "CORAL Main Process");
PROCESS(rf_u_send_proc, "RF Unicast Send Process");
PROCESS(rf_b_send_proc, "RF Broacast Send Process");
PROCESS(print_metrics_process, "Printing metrics process");
AUTOSTART_PROCESSES(&main_coral_process, &rf_u_send_proc, &rf_b_send_proc, &print_metrics_process);
/*---------------------------------------------------------------------------*/

static uint8_t uart_buffer[UART_BUFFER_SIZE];
static uint8_t uart_buffer_index = 0;

// Connection structs
static struct broadcast_conn bc;
static struct unicast_conn uc;
static struct runicast_conn runicast;
static struct mesh_conn mc;
//Statistics
static int msgCount=0;
static int d_send=0;  // Data packets send
static int d_recv=0;	 // Data packets received
static int c_send=0;  // Control packets send
static int c_recv=0;	 // Control packets received

typedef struct __attribute__((__packed__)) packet_struct {
	linkaddr_t to;
	uint8_t rssi;
	char tcParam[3]; // TC Parameters TCT,ACK,RET three char
} packet_t;
MEMB(packets_memb, packet_t, 16);
//TCT:1= TC Neighbors’ Request  0= TC Advertisment to Neighbors 
//ACK:1= With aknowledgement    0=without aknowledgement
//RET:Retransmission Delay 

/*============================================================================*/
/* UTILITY FUNCTIONS */
uint8_t getfromjson(uint8_t json[], char tag[], char val[]){
//   PRINTF("JSON=%s, TAG=%s, VAL=%s\n",json,tag,val);
   int i=0;
   int t=0;
   while(json[i]!='\0'){
      if(!strncmp((char *)(json+i),(char *)tag,3)){
         i = i+3;
         while(json[i]==':'||json[i]=='\"') {
            i++; //jump symbols
         }
         while(json[i]!=',' && json[i]!='\"' && json[i]!='}'){   //copy val
            if(t < 6) val[t]=json[i];
            t++;
            i++;
         }        
         val[t]='\0';
         return 1;
      }
      i++;
   }         
   return 0;
}

packet_t *create_packet()
 {
   packet_t *p;
 
   p = memb_alloc(&packets_memb);
   if(p == NULL) {
     return NULL;
   }
   memset(&(p->to), 0, sizeof(p->to));
   memset(&(p->rssi), 0, sizeof(p->rssi));
   memset(&(p->tcParam), 0, sizeof(p->tcParam));
   return p;
 }
 
 void destroy_packet(packet_t *p)
 {
   memb_free(&packets_memb, p);
 }

/*============================================================================*/
/* UART CALLBACK */
int uart_rx_callback(unsigned char c){
   if(c != '\n' && uart_buffer_index < UART_BUFFER_SIZE){
      uart_buffer[uart_buffer_index++] = c;
   }
   else{
      uart_buffer[uart_buffer_index] = '\0';
      uart_buffer_index = 0;
      PRINTF("Received from UART: %s\n",uart_buffer);  
      
      char buf[3];   
      char val[6]="     "; 
      linkaddr_t dest;
      linkaddr_t nexthop;
          
      // READ PACKET TYPE
      getfromjson(uart_buffer, "PTY", val);
      PRINTF("RECIVED PACKET=%s\n",val);
		
		c_recv++; // STAT In any case from uart is a control message

      if(!strncmp((char *)val,"NN",2)){       // NEW NODE
         printf("{\"PTY\":\"NN\",\"NID\":\"%02d.00\"}\n", node_id);
      }                 
      else if(!strncmp((char *)val,"ND",2)){  // NETWORK DISCOVERY
			getfromjson(uart_buffer, "TCT", val);  // Discovery Type
			packet_t *p = create_packet();	
			p->tcParam[0] = val[0];			
			getfromjson(uart_buffer, "ACK", val);  // Acknowledgement
			p->tcParam[1] = val[0];
			getfromjson(uart_buffer, "RET", val);  // Retransmission delay
			p->tcParam[2] = val[0];

 			//memcpy(&(p->tcParam), tcParam, sizeof(p->tcParam));
			process_post(&rf_b_send_proc, RF_B_SEND_EVENT, (process_data_t)p); // CALL SEND BROADCAST PROCESS
      } 
      else if(!strncmp((char *)val,"DT",2)){  // DATA SEND DATA PACKET
		   msgCount++;
         if(getfromjson(uart_buffer, "DID", val)){  // Get destination addr
            strncpy(buf,(char *)val,2);
            dest.u8[0] =  atoi(buf); // Destination net addr
            strncpy(buf,(char *)uart_buffer+3,2 );
            dest.u8[1] =  atoi(buf); // Destination host addr              
         }
		   /* Send a message to node number 1. */
			getfromjson(uart_buffer, "PLD", val);
         char message[10];
		   sprintf(message,"%s %d",val,msgCount);
		   packetbuf_copyfrom(message, strlen(message));
		   PRINTF("Sending Data Message [%s]\n",message);
		   mesh_send(&mc, &dest);	// Sending Mesh Data Message ...	
			d_send++; // STAT send data packet 
			c_recv--; // STAT do not count control message when you recv a send data packet 
		}
      else if(!strncmp((char *)val,"AD",2)||!strncmp((char *)val,"AR",2)){    // ADD ROUTE  
         uint8_t status = 0;
			uint8_t ar = 0; // Boolean if AR 
			if(!strncmp((char *)val,"AR",2)) // If ADD REPLAY save value
				ar = 1;
         if(getfromjson(uart_buffer, "DID", val)){  // Get destination addr
            strncpy(buf,(char *)val,2);
            dest.u8[0] =  atoi(buf); // Destination net addr
            strncpy(buf,(char *)uart_buffer+3,2 );
            dest.u8[1] =  atoi(buf); // Destination host addr              
         }
         else status = 1;
         
         if(getfromjson(uart_buffer, "NXH", val)){  // Get next hop addr
            strncpy(buf,(char *)val,2 );
            nexthop.u8[0] =  atoi(buf);  // Next Hop net addr
            strncpy(buf,(char *)val+3,2 );
            nexthop.u8[1] =  atoi(buf);  // Next Hop host addr        
         }
         else status = 1;
         
         uint8_t cost=0;
         if(getfromjson(uart_buffer, "CST", val)){  // Get Cost
            cost = atoi(val);
         }
         else status = 1;

         uint8_t seqno = 0;
         if(getfromjson(uart_buffer, "SEQ", val)){  // Get SEQ
            seqno = atoi(val);
         }
         else status = 1;
        
         if(!status) {
            PRINTF("ADD route with destination: %u.%u via %u.%u with cost %u and sequence %u\n",
            dest.u8[0], dest.u8[1], nexthop.u8[0],nexthop.u8[1],cost,seqno);
            route_add(&dest, &nexthop, cost, seqno);  //ADddddhhhhccss  dddd=destination, hhhh=nethop hop, cc=cost, ss=sequenceno
         }
         if(ar == 1){  // If "AR" route packet received stop the wating timer and resend the message
            data_packet_resend(&mc);  //mesh.c
         }
      }
      else if(!strncmp((char *)val,"RM",2)){   // REMOVE ROUTE
         if(getfromjson(uart_buffer, "DID", val)){  // Get destination addr
            if(!strncmp((char *)val,"00.00",5)){  //RM0000 REMOVE ALL routing table records
               route_flush_all();
               PRINTF("REMOVING all routing records...\n"); 
            }               
            else{   // REMOVE ONE 
               struct route_entry *rt;
               strncpy(buf,(char *)val,2 );
               dest.u8[0] =  atoi(buf); // Destination net addr
               strncpy(buf,(char *)val+3,2 );
               dest.u8[1] =  atoi(buf); // Destination host addr              
               rt = route_lookup(&dest);   // SEARCH in routing table
               if(rt != NULL) {
                  PRINTF("REMOVING route to %u.%u \n", dest.u8[0], dest.u8[1]); 
                  route_remove(rt);     // REMOVE from routing table   RMddddd  dddd=destination,
               } 
               else {
                  PRINTF("Cannot remove route to %u.%u does not exists\n", dest.u8[0], dest.u8[1]); 
	            } 
	         }   
	      } // end if get DID        
      }
      else {
         PRINTF("Unknown command from cotroller %s\n",uart_buffer);
      }
   }
   return 0;
}

// SEND NB JSON to controller
static void sendNBjson(uint8_t nodeid, linkaddr_t *from, uint8_t rss, uint8_t sss, uint8_t lqi, uint8_t eng  ){
   printf("{\"NID\":\"%02d.00\",\"PTY\":\"NB\",\"NBR\":\"%02d.%02d\",\"RSS\":%u,\"SSS\":%u,\"LQI\":%u,\"ENG\":%u}\n",
          nodeid, from->u8[0], from->u8[1],rss,sss,lqi,eng); // PRINT TO UART (Send message to controler) 
	c_send++; // STAT
}

/*============================================================================*/
/* BROADCAST CALLBACK */
static void recv_bc_callback(struct broadcast_conn *c, const linkaddr_t *from){
   PRINTF("Received from %d.%d broadcast packet: '%s' with RSSI:%u\n",
         from->u8[0], from->u8[1], (char *)packetbuf_dataptr(), 
         (uint8_t) (- packetbuf_attr(PACKETBUF_ATTR_RSSI)));

	packet_t *p = create_packet();	
	p->to.u8[0]=from->u8[0];
	p->to.u8[1]=from->u8[1];
 	memcpy(&(p->tcParam), packetbuf_dataptr(), sizeof(p->tcParam));
	p->rssi = (uint8_t) (- packetbuf_attr(PACKETBUF_ATTR_RSSI));

	if(p->tcParam[0]=='1'){  //TCT: 1= TC Neighbors' Request      
		PRINTF("Executing Node Neighbors' Request Topology Control\n");
		process_post(&rf_u_send_proc, RF_U_SEND_EVENT, (process_data_t)p); // CALL PROCESS
	}
	else if(p->tcParam[0]=='0'){  //TCT: 0= TC Advertisment to Neighbors
		PRINTF("Executing Advertisment to Neighbors Topology Control\n");	
      sendNBjson(node_id, from, // NodeID, FROM addresss
               (uint8_t) (- packetbuf_attr(PACKETBUF_ATTR_RSSI)),   //RSS
               0,                   //SSS
               (uint8_t) (- packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY)),  //LQI
               0);  // ENG 
	}
	else{
		PRINTF("!!!ERROR: Unknown topology control type\n");
	}
   c_recv++; //STAT increase received control packet
}
/*----------------------------------------------------------------------------*/
static const struct broadcast_callbacks broadcast_callbacks = { recv_bc_callback };
/*----------------------------------------------------------------------------*/

/*============================================================================*/
/* UNICAST CALLBACK */
static void recv_uc_callback(struct unicast_conn *c, const linkaddr_t *from){
   PRINTF("Unicast Received Neighbour Data\n");
   sendNBjson(node_id, from, // NodeID, FROM addresss
               (uint8_t) (- packetbuf_attr(PACKETBUF_ATTR_RSSI)),   //RSS
               atoi((char *)packetbuf_dataptr()),                   //SSS
               (uint8_t) (- packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY)),  //LQI
               0);  // ENG   
   c_recv++; //STAT increase received control packet  
}
/*----------------------------------------------------------------------------*/
static const struct unicast_callbacks unicast_callbacks = { recv_uc_callback };
/*----------------------------------------------------------------------------*/

/*============================================================================*/
/* RELIABLE UNICAST CALLBACK */
static void recv_ruc_callback (struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
   PRINTF("Reliable Unicast Received Neighbour Data\n");
   sendNBjson(node_id, from, // NodeID, FROM addresss
               (uint8_t) (- packetbuf_attr(PACKETBUF_ATTR_RSSI)),   //RSS
               atoi((char *)packetbuf_dataptr()),                   //SSS
               (uint8_t) (- packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY)),  //LQI
               0);  // ENG 
   c_recv++; //STAT increase received control packet
}
/*----------------------------------------------------------------------------*/
static const struct runicast_callbacks runicast_callbacks = { recv_ruc_callback };
/*----------------------------------------------------------------------------*/

/*============================================================================*/
/* MESH CALLBACK */
static void sent(struct mesh_conn *c){
  PRINTF("packet sent\n");
}

static void timedout(struct mesh_conn *c){
  PRINTF("packet timedout\n");
}

static void recv(struct mesh_conn *c, const linkaddr_t *from, uint8_t hops){
   printf("Msg from Node: %d\n",from->u8[0]+from->u8[1]);
	PRINTF("Data received from %d.%d: %.*s (%d)\n",
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
   PRINTF("Initializing route table...\n");
   route_init();
   PRINTF("Starting MESH listener...\n"); 
   mesh_open(&mc, 132, &callbacks);
   PRINTF("Starting UART listener...\n"); 
#ifdef Z1  // For Z1-> uart0 
   uart0_init(BAUD2UBR(115200));       /* set the baud rate as necessary */
   uart0_set_input(uart_rx_callback);  /* set the callback function */
#elif defined SKY // For sky->uart1
   uart_init(BAUD2UBR(115200));        /* set the baud rate as necessary 1111 */
   uart_set_input(0,uart_rx_callback); /* set the callback function */
#elif defined ZOUL // For ZOUL->uart1
   uart_init(BAUD2UBR(115200));        /* set the baud rate as necessary 1111 */
   uart_set_input(0,uart_rx_callback); /* set the callback function */
#elif defined RM090 // For RM090->uart1
  serial_line_init();
  uart1_set_input(uart_rx_callback);
#endif
   
   PRINTF("\nCORAL Main process event...\n");    

   SENSORS_ACTIVATE(button_sensor);

   static int count=0;
   char message[MAX_PAYLOAD_LEN];
	int16_t temperature, humidity;
   static struct etimer periodic_timer;
  	static struct etimer send_timer;

  	etimer_set(&periodic_timer, SEND_INTERVAL);
   linkaddr_t addr;
   while(1) {
#ifdef SENDAUTO
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
		etimer_reset(&periodic_timer);
		etimer_set(&send_timer, SEND_TIME);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&send_timer));
		PRINTF("Sending Timer off\n");

//   Na to do gia timers 
//   if(etimer_expired(&periodic)) {
//      etimer_reset(&periodic);
//      ctimer_set(&backoff_timer, SEND_TIME, send_packet, NULL);

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
      /* Send a message to node number 1. */
		if(node_id != 1){  //If not sink
		   count++;
		   sprintf(message,"%d,%s",count,MESSAGE);
		   PRINTF("Sending message: %s\n",message);
		   packetbuf_copyfrom(message, strlen(message));
		   addr.u8[0] = 1;
		   addr.u8[1] = 0;
		   mesh_send(&mc, &addr);
			d_send++; // STAT send data packet 
		}
   }

   PROCESS_END();
}
/*----------------------------------------------------------------------------*/
/* BROADCAST SEND PROCESS */
PROCESS_THREAD(rf_b_send_proc, ev, data) {
	static struct etimer et;
	static char tcType;
	static char tcAck;
	static char tcRet[2];
   static char tcParam[3] = {'1','1','5'};

	PROCESS_EXITHANDLER(broadcast_close(&bc);)

	PROCESS_BEGIN();
   PRINTF("Starting Broadcast listener...\n");
   broadcast_open(&bc, BC_CHANNEL, &broadcast_callbacks);   
 
	while(1) {
		PROCESS_WAIT_EVENT_UNTIL(ev == RF_B_SEND_EVENT );
   	packet_t* p = (packet_t*)data;	

      if (p != NULL){
 			memcpy(&tcParam, p->tcParam, sizeof(tcParam));
			tcType = p->tcParam[0];
			tcAck = p->tcParam[1];   			
			tcRet[0] = p->tcParam[2];
			tcRet[1] = '\0';
			PRINTF("TC broadcast PARAM TCT:%c, ACK:%c, RET:%s\n",tcType,tcAck,tcRet);
			
			if(tcType == '0'){ // if TC with Advertisment implement broadcast delay			
				//Delay 1 to tcRet sec
	 			if(atoi(tcRet)>1){
					etimer_set(&et, CLOCK_SECOND + random_rand() % (CLOCK_SECOND * (atoi(tcRet)-1)));  
				}
				else
					etimer_set(&et, CLOCK_SECOND); 
				PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
			}

			packetbuf_copyfrom(tcParam, sizeof(tcParam)); // Topology parameters
       	PRINTF("Sending Topology Discovey Broadcast message TCT:%c,ACK:%c,RET:%c\n",
					  p->tcParam[0],p->tcParam[1],p->tcParam[2]);
      	broadcast_send(&bc);	 // Sending Broadcast message ...	

			destroy_packet(p);
		} // end if not null
	   else {
			PRINTF("!!! ERROR: data packet empty [rf_b_send_proc PROCESS]  \n");
		}
	} // end of while
	PROCESS_END();
}
/*----------------------------------------------------------------------------*/
/* UNICAST SEND PROCESS */
PROCESS_THREAD(rf_u_send_proc, ev, data) {
	static linkaddr_t addr;
	static struct etimer et;
	static char rssi[5];
	static char tcType;
	static char tcAck;
	static char tcRet[2];

	PROCESS_EXITHANDLER(unicast_close(&uc);runicast_close(&runicast);)

	PROCESS_BEGIN();
	PRINTF("Starting Unicast listener...\n");
	unicast_open(&uc, UC_CHANNEL, &unicast_callbacks); 
	PRINTF("Starting Reliable Unicast listener...\n");
	runicast_open(&runicast, RUC_CHANNEL, &runicast_callbacks); 
//   static packet_t* p; 
	while(1) {
		PROCESS_WAIT_EVENT_UNTIL(ev == RF_U_SEND_EVENT );
   	packet_t* p = (packet_t*)data;

      if (p != NULL){	
			addr.u8[0] = p->to.u8[0];
			addr.u8[1] = p->to.u8[1];	   			
			sprintf(rssi,"%u",(uint8_t) p->rssi);	
			tcType = p->tcParam[0];
			tcAck = p->tcParam[1];
			tcRet[0] = p->tcParam[2];
			tcRet[1] = '\0';
			PRINTF("TC PARAM TCT:%c, ACK:%c, RET:%s\n",tcType,tcAck,tcRet);
			//Delay 1 to 4 sec
 			if(atoi(tcRet)>1)
				etimer_set(&et, CLOCK_SECOND + random_rand() % (CLOCK_SECOND * (atoi(tcRet)-1)));  
			else
				etimer_set(&et, CLOCK_SECOND);  
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

			packetbuf_copyfrom(rssi, sizeof(rssi)); // add rssi to payload

			if(tcAck  == '0'){   // 0 = Topology Discovery no ACK 
				packetbuf_copyfrom(rssi, sizeof(rssi)); // add rssi to payload
				unicast_send(&uc,&addr);   // SEND UNICAST Topology discovery RESPONCE 
				PRINTF("Sending unicast to %d.%d responding to broadcast msg[RSSI:%s]\n",
						addr.u8[0], addr.u8[1],rssi);   
			} else if(tcAck  == '1'){   // 1 = Topology Discovery with ACK
				packetbuf_copyfrom(rssi, sizeof(rssi)); // add rssi to payload
				runicast_send(&runicast, &addr, MAX_RETRANSMISSIONS);  // SEND Reliable UNICAST TopDisc RESPONCE   
				PRINTF("Sending runicast to %d.%d responding to broadcast msg[RSSI:%s]\n",
						addr.u8[0], addr.u8[1],rssi);  
			} else {
				PRINTF("!!!ERROR Topology Discovery value is not 0 or 1 [rf_u_send_proc PROCESS]\n"); 
			}
   	destroy_packet(p);
		} // end if not null
	} // end of while
	PROCESS_END();
}
/*----------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
// PRINT METRICS
PROCESS_THREAD(print_metrics_process, ev, data){
  static struct etimer periodic_timer;
  static int counter=0;
 
  PROCESS_BEGIN();
  PRINTF("Printing Client Metrics...\n");

  // 60*CLOCKS_SECOND for rm090 should print every one (1) min
  etimer_set(&periodic_timer, 60*CLOCK_SECOND);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    etimer_reset(&periodic_timer);

    printf("R:%d, DAG-VERSION:%d\n",counter, 240); 
    printf("R:%d, Imin:%d, Idoubling:%d\n",counter, 8, 8);

    printf("R:%d, udp_sent:%d\n",counter,d_send);
    printf("R:%d, udp_recv:%d\n",counter,d_recv);	
    
    printf("R:%d, icmp_sent:%d\n",counter,c_send);
    printf("R:%d, icmp_recv:%d\n",counter,c_recv);
    
    printf("R:%d, Leaf MODE: %d\n",counter,8);
    
    counter++; //new round of stats
  }
   PROCESS_END();
}

/*---------------------------------------------------------------------------*/


