/**
 * \file
 *         coral.c Main CORAL mote source code
 * \author
 *         Tryfon Theodorou <tryfonthe@gmail.com>
 */
 
#include "coral.h"

#include "sys/ctimer.h"
#include "lib/list.h"
#include "lib/memb.h"
#define NUM_PACKET_QUEUE 100  // MAX Size of packet buffer
static struct ctimer pkt_timer;
struct packet_entry {
  struct packet_entry *next;
  linkaddr_t dest;  // Destination Address
  char packet[10];  // Size of and ND request packet
};
LIST(packet_queue);
MEMB(packet_mem, struct packet_entry, NUM_PACKET_QUEUE);


#ifdef RM090
	#include "dev/serial-line.h"
#endif


#define LR_U_SEND_EVENT     51
#define SR_B_SEND_EVENT     52

#define MESSAGE "Hello"

//#define METRICS 1

/*---------------------------------------------------------------------------*/
PROCESS(LR_u_send_proc, "LR Unicast Send Process");
PROCESS(SR_b_send_proc, "SR Broacast Send Process");
PROCESS(print_metrics_process, "Printing metrics process");
/*---------------------------------------------------------------------------*/
extern int LongRangeTransmit;
extern int LongRangeReceiving;
int transmission_count=1;

static uint8_t uart_buffer[UART_BUFFER_SIZE];
static uint8_t uart_buffer_index = 0;

linkaddr_t br_id;  // Border Router ID (do not delete used for extern mesh.c)

// Connection structs
static struct broadcast_conn bc;
struct unicast_conn uc;
static struct runicast_conn runicast;
extern struct mesh_conn mc;
//Statistics
static int msgCount=0;
static int d_send=0;  // Data packets send
static int d_recv=0;	 // Data packets received
       int c_send=0;  // Control packets send
static int c_recv=0;	 // Control packets received
       int s_send=0;	 // Control packets send to the Controller
static int s_recv=0;	 // Control packets received from the Controller

typedef struct __attribute__((__packed__)) packet_struct {
	linkaddr_t sink; //Border Router
	linkaddr_t to; // NID  
	uint8_t rssi;  // RSSI
	uint8_t lqi;   // LQI
	uint8_t eng;   // Energy
	uint8_t TCT;   // Topology Control Type
	uint8_t ACK;   // TCP=1 or UDP=0 repsonse
	uint8_t RET;   // Retransmission Delay
} packet_t;
MEMB(packets_memb, packet_t, 1);
//TCT:1= TC Neighbors’ Request  0= TC Advertisment to Neighbors 
//ACK:1= With aknowledgement    0=without aknowledgement
//RET:Retransmission Delay 

packet_t *create_packet()
 {
   packet_t *p;
 
   p = memb_alloc(&packets_memb);
   if(p == NULL) {
     return NULL;
   }
   memset(&(p->sink), 0, sizeof(p->sink));
   memset(&(p->to), 0, sizeof(p->to));
   memset(&(p->rssi), 0, sizeof(p->rssi));
   memset(&(p->lqi), 0, sizeof(p->lqi));
   memset(&(p->eng), 0, sizeof(p->eng));
   memset(&(p->TCT), 0, sizeof(p->TCT));
   memset(&(p->ACK), 0, sizeof(p->ACK));
   memset(&(p->RET), 0, sizeof(p->RET));
   return p;
 }

 void destroy_packet(packet_t *p)
 {
   memb_free(&packets_memb, p);
 }



/*============================================================================*/
/* UTILITY FUNCTIONS */
uint8_t getfromjson(uint8_t json[], char tag[], char val[]){
   //PRINTF("[getfromjson] JSON=%s, TAG=%s, VAL=%s\n",json,tag,val);
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

uint8_t getsubstr(char srcstr[], int from, int to, char substr[]){
   if(strlen(srcstr)>from && strlen(srcstr)>to && from <= to){
      int i=0, t=0;
      for(i=from;i<=to;i++){
         substr[t++] = srcstr[i];
      }
      substr[t] = '\0';
      return 1;
   }
   printf("!!!ERROR [getsubstr]: String %s size=%d Substring from=%d to=%d not done", 
           srcstr,strlen(srcstr),from,to);
   return 0;
}

// PACKET QUEUE Functions
static void send_packet(void *ptr){  // CALLBACK packet_timer function
   struct packet_entry *pkt;

ctimer_reset(&pkt_timer);

   pkt = list_chop(packet_queue); // Remove the last element
   if(pkt!=NULL){
   //   PRINTF("PACKET_QUEUE: Retrieve %s to %02u.%02u\n", pkt->packet, pkt->dest.u8[0], pkt->dest.u8[1]);
      PRINTF("TRYF1: *** LR Unicast Send to %u.%u -> Topology Discovey message [%s] \n",
				 pkt->dest.u8[0], pkt->dest.u8[1], pkt->packet);  
      packetbuf_copyfrom(pkt->packet, sizeof(pkt->packet));   
      dual_radio_switch(LONG_RADIO);
      unicast_send(&uc,&pkt->dest);	 // Sending Unicast message to node addr...	
		c_send++; // STAT send control packet */
      memb_free(&packet_mem, pkt);
   }
/*   else{
      PRINTF("TRYF2: PACKET_QUEUE: is empty\n");
   }

/*   if(list_head(packet_queue)==NULL){ // No more packets
      PRINTF("TRYF2: STOP Timer\n");
      ctimer_stop(&pkt_timer);   // STOP Timer
   }
/*   else { // More packets to send
      PRINTF("START Timer\n");
      ctimer_reset(&pkt_timer);  // RESET Timer
   }*/
 }

static void add_packet_to_queue(linkaddr_t dest, char data[]){
   struct packet_entry *pkt;
   pkt = memb_alloc(&packet_mem); // Allocate a new entry
   pkt->dest.u8[0] = dest.u8[0]; // Copy destination 
   pkt->dest.u8[1] = dest.u8[1]; // Copy destination 
   strcpy(pkt->packet, data);    // Copy data
PRINTF("TRYF1: ADDING %u.%u message [%s] \n", pkt->dest.u8[0], pkt->dest.u8[1], pkt->packet); 
   list_push(packet_queue, pkt);  // Add to list
 //  PRINTF("TRYF2: RESTART Timer because add\n");
 //  ctimer_restart(&pkt_timer);     // RESET Timer
}

/*============================================================================*/
/* UART CALLBACK */
int uart_rx_callback(unsigned char c){
   static char msgbuf[19];
   static linkaddr_t sink;
   static linkaddr_t addr;
   static char PTY[3];

   if(c != '\n' && uart_buffer_index < UART_BUFFER_SIZE && c != '}' ){
      uart_buffer[uart_buffer_index++] = c;
   }
   else{
      if(c == '}') uart_buffer[uart_buffer_index++] = c;
      uart_buffer[uart_buffer_index] = '\0';
      uart_buffer_index = 0;
      PRINTF("Received from UART: %s\n",uart_buffer);  
      
      char buf[3];   
      char val[6]="     "; 
      linkaddr_t dest;
      linkaddr_t nexthop;
          
      // READ PACKET TYPE
      getfromjson(uart_buffer, "PTY", PTY);
      PRINTF("RECIVED PACKET=%s\n",PTY);
		s_recv++; // STAT In any case from uart is a control message		

      if(!strncmp(PTY,"BR",2)){       // NEW NODE
         printf("{\"PTY\":\"BR\",\"BID\":\"%02d.00\"}\n", node_id); // Send to controller
         // PAYLOAD: PTY BID 
         // SIZE(6):  2   4   
         // EXAMPLE: BR  0100  
         sprintf(msgbuf,"BR%02u00",node_id);
			packetbuf_copyfrom(msgbuf, sizeof(msgbuf)); // Topology parameters
       	PRINTF("LR Broadcast Send -> Border Router info [%s] FROM: %u.00\n",
					  msgbuf, node_id);
         dual_radio_switch(LONG_RADIO);
      	broadcast_send(&bc);	 // Sending Broadcast message Border Router
			c_send++; // STAT send control packet */
         br_id.u8[0] = node_id;
         br_id.u8[1] = 0;
      } // End BR if   
      else if(!strncmp(PTY,"NN",2)){ // BROADCAST FOR NEW NODES
        if(getfromjson(uart_buffer, "BID", val)){  // Get Sink Adapter ID addr
            strncpy(buf,(char *)val,2);
            sink.u8[0] =  atoi(buf); // Destination net addr
            strncpy(buf,(char *)val+3,2 );
            sink.u8[1] =  atoi(buf); // Destination host addr              
         }
         // PAYLOAD: PTY SINK 
         // SIZE(6):  2   4   
         // EXAMPLE: NN  0100  
         sprintf(msgbuf,"NN%02u%02u",sink.u8[0],sink.u8[1]);
			packetbuf_copyfrom(msgbuf, sizeof(msgbuf)); // Topology parameters
       	PRINTF("LR Broadcast Send -> Request of new nodes message [%s] FROM: %u.%u\n",
					  msgbuf, sink.u8[0], sink.u8[1]);
         
         dual_radio_switch(LONG_RADIO);
      	broadcast_send(&bc);	 // Sending Broadcast message ...	
			c_send++; // STAT send control packet */
         //printf("{\"PTY\":\"NN\",\"NID\":\"%02d.00\"}\n", node_id);
      } // End NN if                
      else if(!strncmp(PTY,"ND",2)){  // NETWORK DISCOVERY
			packet_t *p = create_packet();
         if(getfromjson(uart_buffer, "BID", val)){  // Get Sink Adapter ID addr
            strncpy(buf,(char *)val,2);
            p->sink.u8[0] =  atoi(buf); // Destination net addr
            strncpy(buf,(char *)val+3,2 );
            p->sink.u8[1] =  atoi(buf); // Destination host addr              
         }
			getfromjson(uart_buffer, "TCT", val);  // Discovery Type
			p->TCT = atoi(val);			
			getfromjson(uart_buffer, "ACK", val);  // Acknowledgement
			p->ACK = atoi(val);	
			getfromjson(uart_buffer, "RET", val);  // Retransmission delay
			p->RET = atoi(val);	
         PRINTF("ND %02u.%02u %01u %01u %01u\n",p->sink.u8[0],p->sink.u8[1],p->TCT,p->ACK,p->RET);

         // PAYLOAD: PTY SINK TCT ACK RET
         // SIZE(9):  2   4    1   1   1
         // EXAMPLE:  ND 0100  0   0   2 
         sprintf(msgbuf,"ND%02u%02u%01u%01u%01u",p->sink.u8[0],p->sink.u8[1],p->TCT,p->ACK,p->RET);
         packetbuf_copyfrom(msgbuf, sizeof(msgbuf)); // Topology parameters

         if(p->TCT == 0){ // TC-NA
       	   PRINTF("SR Broadcast Send -> Topology Discovey message [%s] FROM: %u.%u TCT:%u,ACK:%u,RET:%u\n",
				        msgbuf, p->sink.u8[0], p->sink.u8[1], p->TCT, p->ACK, p->RET);     
            dual_radio_switch(SHORT_RADIO);
      	   broadcast_send(&bc);	 // Sending Broadcast message ...	
			   c_send++; // STAT send control packet */
         }
         else if(p->TCT==1){ // TC-NR Neighbor Request 
            if(getfromjson(uart_buffer, "NID", val)){  // Get Node Address ID
               strncpy(buf,(char *)val,2);
               addr.u8[0] =  atoi(buf); // Destination net addr
               strncpy(buf,(char *)val+3,2 );
               addr.u8[1] =  atoi(buf); // Destination host addr              
            }
            if(node_id==addr.u8[0]){ // Request the is for the border router neighbors send only 
       	      PRINTF("SR Broadcast Send -> Topology Discovey message [%s] FROM: %u.%u TCT:%u,ACK:%u,RET:%u\n",
				           msgbuf, p->sink.u8[0], p->sink.u8[1], p->TCT, p->ACK, p->RET);     
               dual_radio_switch(SHORT_RADIO);
      	      broadcast_send(&bc);	 // Sending Broadcast message ...	
			      c_send++; // STAT send control packet */
            }
            else { // All other nodes in the network
 /*      	      PRINTF("*** LR Unicast Send to %u.%u -> Topology Discovey message [%s] FROM: %u.%u TCT:%u,ACK:%u,RET:%u\n",
				           addr.u8[0], addr.u8[1], msgbuf, p->sink.u8[0], p->sink.u8[1], p->TCT, p->ACK, p->RET);     
               dual_radio_switch(LONG_RADIO);
      	      unicast_send(&uc,&addr);	 // Sending Unicast message to node addr...	
			      c_send++; // STAT send control packet */
               add_packet_to_queue(addr, msgbuf);
            }
         }
         else {
            printf("!!!ERROR: Unknown Topology Control Type %d [uart_rx_callback] \n",p->TCT);
         }
			destroy_packet(p);  
      } // End ND if
      else if(!strncmp(PTY,"DT",2)){  // DATA SEND DATA PACKET
		   msgCount++;
         if(getfromjson(uart_buffer, "DID", val)){  // Get destination addr
            strncpy(buf,(char *)val,2);
            dest.u8[0] =  atoi(buf); // Destination net addr
            strncpy(buf,(char *)val+3,2 );  // IT WAS LIKE THAT strncpy(buf,(char *)uart_buffer+3,2 );
            dest.u8[1] =  atoi(buf); // Destination host addr              
         }
		   /* Send a message to node number 1. */
			getfromjson(uart_buffer, "PLD", val);
         char message[10];
		   sprintf(message,"%s %d",val,msgCount);
		   packetbuf_copyfrom(message, strlen(message));
		   printf("Sending Data Message [%s] to %2d.%2d\n",message,dest.u8[0],dest.u8[1]);
         dual_radio_switch(SHORT_RADIO); //Dual set short
		   mesh_send(&mc, &dest);	// Sending Mesh Data Message ...	
			d_send++; // STAT send data packet 
			s_recv--; // STAT do not count control message when you recv a send data packet 
		} // End DT if
      else if(!strncmp(PTY,"AD",2)||!strncmp(PTY,"AR",2)){    // ADD ROUTE  
         uint8_t status = 0;
			uint8_t ar = 0; // Boolean if AR 
			if(!strncmp((char *)PTY,"AR",2)) // If ADD REPLAY save value
				ar = 1;

         if(getfromjson(uart_buffer, "NID", val)){  // Get Node Address ID
            strncpy(buf,(char *)val,2);
            addr.u8[0] =  atoi(buf); // Node net addr
            strncpy(buf,(char *)val+3,2 );
            addr.u8[1] =  atoi(buf); // Node host addr              
         }
         else status = 1;

         if(getfromjson(uart_buffer, "DID", val)){  // Get destination addr
            strncpy(buf,(char *)val,2);
            dest.u8[0] =  atoi(buf); // Destination net addr
            strncpy(buf,(char *)val+3,2 );   // IT WAS LIKE THAT strncpy(buf,(char *)uart_buffer+3,2 );
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

         if(addr.u8[0]!=node_id){ // For any node in the network except the adapter
            // PAYLOAD: PTY DID  NXH  CST SEQ 
            // SIZE(15): 2   4    4    3   2
            // EXAMPLE: AD  0700 0300 102  00 
            sprintf(msgbuf,"%s%02u%02u%02u%02u%03d%02d",
                    PTY,dest.u8[0],dest.u8[1],nexthop.u8[0],nexthop.u8[1],cost,seqno);
			   packetbuf_copyfrom(msgbuf, sizeof(msgbuf)); // Topology parameters
         
            dual_radio_switch(LONG_RADIO);
     	      unicast_send(&uc,&addr);	 // Sending Unicast to the NID message ...
     	      PRINTF("LR Unicast Send to %02u.%02u -> Add route message [%s]\n",addr.u8[0],addr.u8[1],msgbuf);
			   c_send++; // STAT send control packet */
         }
         else { // For border router
            if(!status) {
               PRINTF("ADD route with destination: %u.%u via %u.%u with cost %u and sequence %u\n",
               dest.u8[0], dest.u8[1], nexthop.u8[0],nexthop.u8[1],cost,seqno);
               route_add(&dest, &nexthop, cost, seqno);  //ADddddhhhhccss  dddd=destination, hhhh=nethop hop, cc=cost, ss=sequenceno
            }

            if(ar == 1){  // If "AR" route packet received stop the wating timer and resend the message
             // to del  dual_radio_switch(SHORT_RADIO);
               data_packet_resend(&mc);  //mesh.c
            }
         }
      } // End AD or AR
      else if(!strncmp(PTY,"RM",2)){   // REMOVE ROUTE
         if(getfromjson(uart_buffer, "NID", val)){  // Get Node Address ID
            strncpy(buf,(char *)val,2);
            addr.u8[0] =  atoi(buf); // Node net addr
            strncpy(buf,(char *)val+3,2 );
            addr.u8[1] =  atoi(buf); // Node host addr              
         }
         if(getfromjson(uart_buffer, "DID", val)){  // Get destination addr
            strncpy(buf,(char *)val,2 );
            dest.u8[0] =  atoi(buf); // Destination net addr
            strncpy(buf,(char *)val+3,2 );
            dest.u8[1] =  atoi(buf); // Destination host addr              
         }
         if(addr.u8[0]!=node_id){ // For any node in the network except the adapter
            // PAYLOAD: PTY DID 
            // SIZE(6):  2   4   
            // EXAMPLE: RM  0100  
            sprintf(msgbuf,"RM%02u%02u",dest.u8[0],dest.u8[1]);
			   packetbuf_copyfrom(msgbuf, sizeof(msgbuf)); // Topology parameters
         
            dual_radio_switch(LONG_RADIO);
            if(addr.u8[0]==0 && addr.u8[1]==0){ // if NID=0
       	      PRINTF("LR Broadcast Send -> Request for remove route message [%s]\n",msgbuf);
      	      broadcast_send(&bc);	// Sending Broadcast message ...
               route_flush_all(); // flush the adapter routing rules as well
            }
            else {  // if NID not 0 send unicast to one node
      	      unicast_send(&uc,&addr);	 // Sending Unicast to one node message ...
       	      PRINTF("LR Unicast Send -> Request for remove route message [%s]\n",msgbuf);
			      c_send++; // STAT send control packet */
            }	
         }
         else { // Remove route from adapter
            if(dest.u8[0]==0 && dest.u8[1]==0){  //RM0000 REMOVE ALL routing table records
               route_flush_all();
               PRINTF("REMOVING all routing records...Route Table size:%d\n",route_num()); 
            }               
            else{   // REMOVE ONE 
               struct route_entry *rt;
               rt = route_lookup(&dest);   // SEARCH in routing table
               if(rt != NULL) {
                  PRINTF("REMOVING route to %u.%u \n", dest.u8[0], dest.u8[1]); 
                  route_remove(rt);     // REMOVE from routing table   RMddddd  dddd=destination,
               } 
               else {
                  PRINTF("Cannot remove route to %u.%u does not exists\n", dest.u8[0], dest.u8[1]); 
	            } 
	         } // end if get DID
         } // end of else remove from adapter        
      } // End RM if
      else if(!strncmp(PTY,"DB",2)){ // PRINT DEBUG
         route_print();
         // PAYLOAD: PTY 
         // SIZE(2):  2     
         // EXAMPLE: DB  
         sprintf(msgbuf,"DB");
			packetbuf_copyfrom(msgbuf, sizeof(msgbuf)); // Topology parameters
       	PRINTF("LR Broadcast Debug message [%s]\n",msgbuf);
         dual_radio_switch(LONG_RADIO);
 	      broadcast_send(&bc);	 // Sending Broadcast message ...	
      } // End DB if
      else {
         PRINTF("Unknown command from cotroller %s\n",uart_buffer);
      } 
   }
   return 0;
}


/*============================================================================*/
/*                           SR BROADCAST                                     */
/*============================================================================*/
// SEND NB JSON to controller FOR all nodes neigbors to the sink
static void sendNBjson(uint8_t nodeid, linkaddr_t *from, uint8_t rss, uint8_t sss, uint8_t lqi, uint8_t eng  ){
   printf("{\"PTY\":\"NB\",\"NID\":\"%02d.00\",\"NBR\":\"%02d.%02d\",\"BID\":\"%02d.00\",\"RSS\":%u,\"SSS\":%u,\"LQI\":%u,\"ENG\":%u}\n",
          nodeid, from->u8[0], from->u8[1],nodeid,rss,sss,lqi,eng); // PRINT TO UART (Send message to controler) 
	s_send++; // STAT
}
/* BROADCAST CALLBACK */
static void recv_bc_callback(struct broadcast_conn *c, const linkaddr_t *from){
   PRINTF("Received from %d.%d broadcast packet: '%s' with RSSI:%u\n",
         from->u8[0], from->u8[1], (char *)packetbuf_dataptr(), 
         (uint8_t) (- packetbuf_attr(PACKETBUF_ATTR_RSSI)));
   c_recv++; //STAT increase received control packet

	packet_t *p = create_packet();	

	p->to.u8[0]=from->u8[0];
	p->to.u8[1]=from->u8[1];
	p->rssi = (uint8_t) (- packetbuf_attr(PACKETBUF_ATTR_RSSI));
   p->lqi = (uint8_t) (- packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY));
   p->eng = (uint16_t) 0;
   char data_buf[packetbuf_datalen()];
   packetbuf_copyto(data_buf);
   char PTY[3];
   if(getsubstr(data_buf,0,1,PTY) && !strncmp((char *)PTY,"ND",2)){
      char buf[4]; 
      if(getsubstr(data_buf,2,3,buf)) p->sink.u8[0] = atoi(buf);
      if(getsubstr(data_buf,4,5,buf)) p->sink.u8[1] = atoi(buf);
      if(getsubstr(data_buf,6,6,buf)) p->TCT = atoi(buf);
      if(getsubstr(data_buf,7,7,buf)) p->ACK = atoi(buf);
      if(getsubstr(data_buf,8,8,buf)) p->RET = atoi(buf);

      if(route_lookup(from)==NULL){
         route_add(from, from, p->rssi, 0);  //ADddddhhhhccss  dddd=destination, hhhh=nethop hop, cc=cost, ss=sequenceno
         PRINTF("ADD route with destination: %u.%u via %u.%u with cost %u and sequence %u\n",
                 from->u8[0], from->u8[1], from->u8[0],from->u8[1],p->rssi,0);
      }
      // Send to CONTROLLER new NBR: NodeID, FROM addresss, RSS, SSS, LQI, ENG
      sendNBjson(node_id, from, p->rssi, 0, p->lqi, 0); 
   } // END if ND 
   else{
      printf("!!!ERROR [recv_bc_callback]: Unknown Packet type [%s]\n",PTY);
   }
	destroy_packet(p);
}
/*----------------------------------------------------------------------------*/
static const struct broadcast_callbacks broadcast_callbacks = { recv_bc_callback };
/*----------------------------------------------------------------------------*/

/* BROADCAST SEND PROCESS */
PROCESS_THREAD(SR_b_send_proc, ev, data) {
	static struct etimer et;
	static uint8_t tcType=1;
	static uint8_t tcAck=1;
	static uint8_t tcRet=5;
   static linkaddr_t sink;
   static char msgbuf[10];
	static packet_t *p;

	PROCESS_EXITHANDLER(broadcast_close(&bc);)

	PROCESS_BEGIN();
   PRINTF("Starting Broadcast listener...\n");
   broadcast_open(&bc, BC_CHANNEL, &broadcast_callbacks);   
 
	while(1) {
		PROCESS_WAIT_EVENT_UNTIL(ev == SR_B_SEND_EVENT );

//??? probably remore all these ???
		p = (packet_t*)data;	

      if (p != NULL){
         sink.u8[0] = p->sink.u8[0];
         sink.u8[1] = p->sink.u8[1];
			tcType = p->TCT;
			tcAck = p->ACK;   			
			tcRet = p->RET;

	      PRINTF("THREAD SR_b_send_proc PRT: ND %02u.%02u %01u %01u %01u\n",p->sink.u8[0],p->sink.u8[1],p->TCT,p->ACK,p->RET); 		
			if(tcType == 0){ // if TC-NA with Advertisment implement broadcast flooding delay	
     //  No need for delay in the first message
     //       etimer_set(&et, CLOCK_SECOND * (random_rand() % tcRet)); // Delay Time based on RET		
	  //			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
			
	         printf("PTR: 2. ND %02u.%02u %01u %01u %01u\n",p->sink.u8[0],p->sink.u8[1],p->TCT,p->ACK,p->RET);
            printf("VAR: 2. ND %02u.%02u %01u %01u %01u\n",sink.u8[0],sink.u8[1],tcType,tcAck,tcRet); 

            // PAYLOAD: PTY SINK TCT ACK RET
            // SIZE(9):  2   4    1   1   1
            // EXAMPLE:  ND 0100  0   0   2 
            sprintf(msgbuf,"ND%02u%02u%01u%01u%01u",sink.u8[0],sink.u8[1],tcType,tcAck,tcRet);
			   packetbuf_copyfrom(msgbuf, sizeof(msgbuf)); // Topology parameters
       	   PRINTF("SR Broadcast Send -> Topology Discovey message [%s] FROM: %u.%u TCT:%u,ACK:%u,RET:%u\n",
				        msgbuf, sink.u8[0], sink.u8[1], tcType,tcAck,tcRet);
         
            dual_radio_switch(SHORT_RADIO);
      	   broadcast_send(&bc);	 // Sending Broadcast message ...	
			   c_send++; // STAT send control packet */
         }
         else if(tcType==1){ // TC-NR Neighbor Request 

         }
         else {
            printf("!!!ERROR: Unknown Topology Control Type %d [SR_b_send_proc PROCESS] \n",tcType);
         }
		} // end if not null
	   else {
			printf("!!!ERROR: data packet empty [SR_b_send_proc PROCESS] \n");
		}
		destroy_packet(p);
	} // end of while
	PROCESS_END();
}
/*----------------------------------------------------------------------------*/

/*============================================================================*/
/*                           LR UNICAST                                       */
/*============================================================================*/
// SEND NB JSON to Controller
static void sendNBtoControler(linkaddr_t *from, char data_buf[]){ 
   //PAYLOAD:  PTY NBR  RSS  LQI  ENG
   //SIZE(19):  2   4    4    4    5
   //EXAMPLE:  NB  0200 0000 0000 00000  
   char NBRu0[3];
   getsubstr(data_buf,2,3,NBRu0);
   char NBRu1[3];
   getsubstr(data_buf,4,5,NBRu1);
   char RSS[5];
   getsubstr(data_buf,6,9,RSS);
   char LQI[5];
   getsubstr(data_buf,10,13,LQI);
   char ENG[6];
   getsubstr(data_buf,14,18,ENG);

   PRINTF("Sending to Controller:\n");
   printf("{\"PTY\":\"NB\",\"NID\":\"%s.%s\",\"NBR\":\"%02d.%02d\",\"BID\":\"%02d.00\",\"RSS\":%s,\"SSS\":%s,\"LQI\":%s,\"ENG\":%s}\n",
          NBRu0, NBRu1, from->u8[0], from->u8[1], node_id, RSS,"0", LQI, ENG); // PRINT TO UART (Send message to controler) 
   s_send++;
}

/* UNICAST UDP CALLBACK */
static void recv_uc_callback(struct unicast_conn *c, const linkaddr_t *from){
   static char PTY[3];
   static int pkt_ok;
   static char NIDu0[3];
   static char NIDu1[3];
   static char DIDu0[3];
   static char DIDu1[3];

   PRINTF("Unicast Received Neighbour Data:");
   c_recv++; //STAT increase received control packet 
   char data_buf[packetbuf_datalen()];
   packetbuf_copyto(data_buf);
   PRINTF(" [%s] \n",data_buf);

   if(LongRangeReceiving > 0){  //LONG RANGE
      PRINTF("From LONG Range\n");
      pkt_ok = getsubstr(data_buf,0,1,PTY);
      if(pkt_ok && !strncmp((char *)PTY,"NN",2)){ // Response to new node request
         // if recieved PTY=NN New node solicitation response
         //PAYLOAD:  PTY NID  ENG
         //SIZE(11):  2   4 
         //EXAMPLE:  NN  0200 12345
         getsubstr(data_buf,2,3,NIDu0);
         getsubstr(data_buf,4,5,NIDu1);
//??? TRYF TO REMOVE
/*if(atoi(NIDu0)>10){
   ctimer_set(&pkt_timer, (CLOCK_SECOND/1000)*300, send_packet, NULL); // Send Packet ND TIMER
PRINTF("TRYF STARTING TIMER\n");
}*/

         char eng[6];
         getsubstr(data_buf,6,10,eng);
         PRINTF("Sending to Controller:\n");
         printf("{\"PTY\":\"NN\",\"BID\":\"%02d.00\",\"NID\":\"%s.%s\",\"ENG\":\"%s\"}\n", node_id, NIDu0, NIDu1, eng);
         s_send++;
      }
      // if response from Neighbor Node New Neighbor
      else if(pkt_ok && !strncmp((char *)PTY,"NB",2)){
         sendNBtoControler(from, data_buf);
      }
      // if response from Neighbor Node Miss Route
      else if(pkt_ok && !strncmp((char *)PTY,"MR",2)){
         // if recieved PTY=MR Miss Route
         //PAYLOAD:  PTY NID  DID
         //SIZE(10):  2   4    4
         //EXAMPLE:  NN  0200 0400
         getsubstr(data_buf,2,3,NIDu0);
         getsubstr(data_buf,4,5,NIDu1);
         getsubstr(data_buf,6,7,DIDu0);
         getsubstr(data_buf,8,9,DIDu1);
         //SEND MissRoute to the controller  eg MR 01.00 ***********
         printf("{\"PTY\":\"MR\",\"NID\":\"%s.%s\",\"DID\":\"%s.%s\"}\n", NIDu0,NIDu1,DIDu0,DIDu1);  
         s_send++;
      }
      else{
         printf("Error!!! Unknown packet [%s] received from %s.%s\n", PTY,NIDu0,NIDu1);
      }
   } else{  // SHORT RANGE
      printf("From SHORT Range\n");
   }
}
/*----------------------------------------------------------------------------*/
static const struct unicast_callbacks unicast_callbacks = { recv_uc_callback };
/*----------------------------------------------------------------------------*/

/*============================================================================*/
/* RELIABLE TCP UNICAST CALLBACK */
static void recv_ruc_callback (struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
   static char PTY[3];
   c_recv++; //STAT increase received control packet 
   char data_buf[packetbuf_datalen()];
   packetbuf_copyto(data_buf);
   PRINTF("Reliable Unicast seq [%d] Received Neighbour Data:[%s]\n", seqno, data_buf);

   if(LongRangeReceiving > 0){  //LONG RANGE
      PRINTF("From LONG Range\n");

      if(getsubstr(data_buf,0,1,PTY) && !strncmp((char *)PTY,"NN",2)){ // Response to new node request
      // if recieved PTY=NN New node solicitation response
      //PAYLOAD:  PTY NID  ENG
      //SIZE(11):  2   4 
      //EXAMPLE:  NN  0200 12345
         char NIDu0[3];
         getsubstr(data_buf,2,3,NIDu0);
         char NIDu1[3];
         getsubstr(data_buf,4,5,NIDu1);
         char eng[6];
         getsubstr(data_buf,6,10,eng);
         PRINTF("Sending to Controller:\n");
         printf("{\"PTY\":\"NN\",\"BID\":\"%02d.00\",\"NID\":\"%s.%s\",\"ENG\":\"%s\"}\n", node_id, NIDu0, NIDu1, eng);
         s_send++;
      }
      else if(getsubstr(data_buf,0,1,PTY) && !strncmp((char *)PTY,"NB",2)){
      // if response from Neighbor Node
         sendNBtoControler(from, data_buf);
      }
   } else{  // SHORT RANGE
      printf("From SHORT Range\n");
   }
}
/*----------------------------------------------------------------------------*/
static const struct runicast_callbacks runicast_callbacks = { recv_ruc_callback };
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* UNICAST SEND PROCESS */
PROCESS_THREAD(LR_u_send_proc, ev, data) {
	static linkaddr_t addr;
	static struct etimer et;
	static char rssi[5];
	static uint8_t tcType;
	static uint8_t tcAck;
	static uint8_t tcRet;   
   static int delay=1; 

	PROCESS_EXITHANDLER(unicast_close(&uc);runicast_close(&runicast);)

	PROCESS_BEGIN();
	PRINTF("Starting Unicast listener...\n");
	unicast_open(&uc, UC_CHANNEL, &unicast_callbacks); 
	PRINTF("Starting Reliable Unicast listener...\n");
	runicast_open(&runicast, RUC_CHANNEL, &runicast_callbacks); 

	while(1) {
		PROCESS_WAIT_EVENT_UNTIL(ev == LR_U_SEND_EVENT );

//??? probably remore all these ???

   	packet_t* p = (packet_t*)data;

      if (p != NULL){	
			addr.u8[0] = p->to.u8[0];
			addr.u8[1] = p->to.u8[1];	   			
			sprintf(rssi,"%u",(uint8_t) p->rssi);	
			tcType = p->TCT;
			tcAck = p->ACK;
			tcRet = p->RET;
         
			PRINTF("TC PARAM TCT:%u, ACK:%u, RET:%u\n",tcType,tcAck,tcRet);
         delay = (random_rand() % 200);  // Random delay from 50 to 1400 ms
         delay = (CLOCK_SECOND / 1000) * ((delay!=0)?delay:0.5);
         etimer_set(&et, delay); 		
PRINTF("Border Router send TD Delay:%d\n",delay);
         PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));	

			packetbuf_copyfrom(rssi, sizeof(rssi)); // add rssi to payload

			if(tcAck == 0){   // 0 = Topology Discovery no ACK 
				packetbuf_copyfrom(rssi, sizeof(rssi)); // add rssi to payload
				unicast_send(&uc,&addr);   // SEND UNICAST Topology discovery RESPONCE 
				PRINTF("Sending unicast to %d.%d responding to broadcast msg[RSSI:%s]\n",
						addr.u8[0], addr.u8[1],rssi);   
			} else if(tcAck == 1){   // 1 = Topology Discovery with ACK
				packetbuf_copyfrom(rssi, sizeof(rssi)); // add rssi to payload
				runicast_send(&runicast, &addr, MAX_RETRANSMISSIONS);  // SEND Reliable UNICAST TopDisc RESPONCE   
				PRINTF("Sending runicast to %d.%d responding to broadcast msg[RSSI:%s]\n",
						addr.u8[0], addr.u8[1],rssi);  
			} else {
				printf("!!!ERROR Topology Discovery value is not 0 or 1 [rf_u_send_proc PROCESS]\n"); 
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
#ifdef METRICS 

  PRINTF("Printing Client Metrics...\n");

  // 60*CLOCKS_SECOND for rm090 should print every one (1) min
  etimer_set(&periodic_timer, 60*CLOCK_SECOND);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    etimer_reset(&periodic_timer);

//    printf("R:%d, DAG-VERSION:%d\n",counter, 240); 
//    printf("R:%d, Imin:%d, Idoubling:%d\n",counter, 8, 8);

    printf("R:%d, udp_sent:%d\n",counter,d_send);
    printf("R:%d, udp_recv:%d\n",counter,d_recv);	
    
    printf("R:%d, icmp_sent:%d\n",counter,c_send);
    printf("R:%d, icmp_recv:%d\n",counter,c_recv);
    
    printf("R:%d, ctrl_sent:%d\n",counter,s_send); // Send to Controller 
    printf("R:%d, ctrl_recv:%d\n",counter,s_recv); // Receive from Controller
//    printf("R:%d, Leaf MODE: %d\n",counter,8);
    
    counter++; //new round of stats
  } 
#endif
   PROCESS_END();

}

/*---------------------------------------------------------------------------*/



/* INITIALIZE CORAL-SDN BOREDER ROUTER PROTOCOL */
void coral_init(){
   process_start(&LR_u_send_proc, NULL);
   process_start(&SR_b_send_proc, NULL);
   process_start(&print_metrics_process, NULL);

   PRINTF("Initializing route table...\n");
   route_init();

   PRINTF("Initializing packet queue buffer and start timer...\n");
   list_init(packet_queue);
   memb_init(&packet_mem);
   ctimer_set(&pkt_timer, (CLOCK_SECOND/1000)*600, send_packet, NULL); // Send Packet ND TIMER

   PRINTF("Starting UART listener...\n"); 
#ifdef Z1  // For Z1-> uart0 
   uart0_init(BAUD2UBR(115200));       /* set the baud rate as necessary */
   uart0_set_input(uart_rx_callback);  /* set the callback function */
#elif defined SKY // For sky->uart1
   uart_init(BAUD2UBR(115200));        /* set the baud rate as necessary 1111 */
   uart_set_input(0,uart_rx_callback); /* set the callback function */
#elif defined CORAL_COOJA // For COOJA->uart1
   rs232_set_speed(RS232_115200);
   rs232_init();
   rs232_set_input(uart_rx_callback);
#elif defined ZOUL // For ZOUL->uart1
   uart_init(BAUD2UBR(115200));        /* set the baud rate as necessary 1111 */
   uart_set_input(0,uart_rx_callback); /* set the callback function */
#elif defined RM090 // For RM090->uart1
  serial_line_init();
  uart1_set_input(uart_rx_callback);
#endif

}


