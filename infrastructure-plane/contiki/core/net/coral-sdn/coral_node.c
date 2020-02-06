/**
 * \file
 *         coral.c Main CORAL mote source code
 * \author
 *         Tryfon Theodorou <tryfonthe@gmail.com>
 */
 
#include "coral.h"

#define LR_U_SEND_EVENT     51
#define SR_B_SEND_EVENT     52


//#define METRICS 0


/*---------------------------------------------------------------------------*/
PROCESS(LR_u_send_proc, "LR Unicast Send Process");
PROCESS(SR_b_send_proc, "SR Broacast Send Process");
PROCESS(print_metrics_process, "Printing metrics process");
//AUTOSTART_PROCESSES(&LR_u_send_proc, &SR_b_send_proc, &print_metrics_process);
/*---------------------------------------------------------------------------*/
uint8_t mobile_node=0;

extern int LongRangeTransmit;
extern int LongRangeReceiving;
int transmission_count=1;

extern struct mesh_conn mc;  // Unicast to controller retransmission for missroute

linkaddr_t br_id;  // Border Router ID

// Connection structs
static struct broadcast_conn bc;
struct unicast_conn uc;
static struct runicast_conn runicast;
//Statistics
static int msgCount=0;
static int d_send=0;  // Data packets send
static int d_recv=0;	 // Data packets received
       int c_send=0;  // Control packets send
static int c_recv=0;	 // Control packets received
       int s_send=0;	 // Control packets send to the Controller  DUMMY for compatibility with the border router for the mesh.c file 

typedef struct __attribute__((__packed__)) packet_struct {
   char pty[3];   // Packet Type
	linkaddr_t sink; // Border Router
	linkaddr_t to; //NID
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
   memset(&(p->pty), 0, sizeof(p->pty));
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


/* INITIALIZE PROTOCOL */
void coral_init(){
   process_start(&LR_u_send_proc, NULL);
   process_start(&SR_b_send_proc, NULL);
   process_start(&print_metrics_process, NULL);

   PRINTF("Initializing route table...\n");
   route_init();
}

/*============================================================================*/
/* UTILITY FUNCTIONS */
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

/*============================================================================*/
/*                           SR BROADCAST                                     */
/*============================================================================*/
/* BROADCAST CALLBACK */
static void recv_bc_callback(struct broadcast_conn *c, const linkaddr_t *from){
   static char msgbuf[20];
   static char PTY[3];
   static char buf[4]; 

   if(LongRangeReceiving > 0)
      PRINTF("Long Range Received from %d.%d broadcast packet: '%s' with RSSI:%u\n",
            from->u8[0], from->u8[1], (char *)packetbuf_dataptr(), 
            (uint8_t) (- packetbuf_attr(PACKETBUF_ATTR_RSSI)));
   else
      PRINTF("Short Range Received from %d.%d broadcast packet: '%s' with RSSI:%u\n",
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

   getsubstr(data_buf,0,1,PTY);
   strcpy(p->pty,PTY); // Packet Type

   // if recieve PTY=NN BID=0100  New node solicitation request
   if(!strncmp((char *)PTY,"NN",2)){ // Respond to new node request
      if(getsubstr(data_buf,2,3,buf)) p->sink.u8[0] = atoi(buf);  // Get sink ID
      if(getsubstr(data_buf,4,5,buf)) p->sink.u8[1] = atoi(buf);

      process_post(&LR_u_send_proc, LR_U_SEND_EVENT, (process_data_t)p); // Short BROADCAST SEND PROCESS
   }
   // if recieve PTY=ND BID=0100 TCT ACK RET Neighbor Discovery request
   else if(!strncmp((char *)PTY,"ND",2)){
      if(getsubstr(data_buf,2,3,buf)) p->sink.u8[0] = atoi(buf);
      if(getsubstr(data_buf,4,5,buf)) p->sink.u8[1] = atoi(buf);
      if(getsubstr(data_buf,6,6,buf)) p->TCT = atoi(buf);
      if(getsubstr(data_buf,7,7,buf)) p->ACK = atoi(buf);
      if(getsubstr(data_buf,8,8,buf)) p->RET = atoi(buf);

      PRINTF("RECEIVE BC PACKETBUF_DATALEN%d buf=%s TCT=%u ACK=%u RET=%u\n",
               packetbuf_datalen(),data_buf,p->TCT,p->ACK,p->RET);

      if(p->TCT==1){  //TC-NR   
		   PRINTF("Executing LR Unicast respond process call\n");
		   process_post(&LR_u_send_proc, LR_U_SEND_EVENT, (process_data_t)p); // Long UNICAST RESPOND to Border router
      }
      else if(p->TCT==0){  //TCT: 0= TC-NA Advertisment to Neighbors - Flooding      
         if(route_lookup(from)==NULL){
		      PRINTF("Executing LR Unicast respond process call\n");
		      process_post(&LR_u_send_proc, LR_U_SEND_EVENT, (process_data_t)p); // Long UNICAST RESPOND to Border router

            route_add(from, from, p->rssi, 0);  //ADddddhhhhccss  dddd=destination, hhhh=nethop hop, cc=cost, ss=sequenceno
            PRINTF("ADD route with destination: %u.%u via %u.%u with cost %u and sequence %u\n",
                    from->u8[0], from->u8[1], from->u8[0],from->u8[1],p->rssi,0);
            // BROADCAST
            if(route_num()==1){  //If the first time receive a broadcast message resend a broadcast
               process_post(&SR_b_send_proc, SR_B_SEND_EVENT, (process_data_t)p); // Short BROADCAST SEND PROCESS
            }
         }
	   }
      else{
         printf("!!!ERROR [recv_bc_callback]: Unknown topology control type [%d]\n",p->TCT);
      }
   } // END if ND
   else if(!strncmp((char *)PTY,"RM",2)){
      linkaddr_t dest;
      // PAYLOAD: PTY DID 
      // SIZE(6):  2   4   
      // EXAMPLE: RM  0100  
      if(getsubstr(data_buf,2,3,buf)) dest.u8[0] = atoi(buf);
      if(getsubstr(data_buf,4,5,buf)) dest.u8[1] = atoi(buf);

      if(dest.u8[0]==0 && dest.u8[1]==0){  //RM0000 REMOVE ALL routing table records
          route_flush_all();
          PRINTF("REMOVING all routing records... Routing Table size:%d\n",route_num()); 
      }
   } // END if RM   
   else if(!strncmp((char *)PTY,"BR",2)){  
      // PAYLOAD: PTY BID 
      // SIZE(6):  2   4   
      // EXAMPLE: RM  0100  
      if(getsubstr(data_buf,2,3,buf)) br_id.u8[0] = atoi(buf);
      if(getsubstr(data_buf,4,5,buf)) br_id.u8[1] = atoi(buf);
      PRINTF("New Border Router %02u.%02u registering\n",br_id.u8[0],br_id.u8[1]);
   } // END if BR           
   else if(!strncmp((char *)PTY,"DB",2)){ // PRINT DEBUG
      route_print();
   }
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
   static int delay=1;

	PROCESS_EXITHANDLER(broadcast_close(&bc);)

	PROCESS_BEGIN();
   PRINTF("Starting Broadcast listener...\n");
   broadcast_open(&bc, BC_CHANNEL, &broadcast_callbacks);   
 
	while(1) {
		PROCESS_WAIT_EVENT_UNTIL(ev == SR_B_SEND_EVENT );
      p = (packet_t*)data;	

      if (p != NULL){
         sink.u8[0] = p->sink.u8[0];
         sink.u8[1] = p->sink.u8[1];
			tcType = p->TCT;
			tcAck = p->ACK;   			
			tcRet = p->RET;

	      PRINTF("THREAD SR_b_send_proc PRT: ND %02u.%02u %01u %01u %01u\n",p->sink.u8[0],p->sink.u8[1],p->TCT,p->ACK,p->RET); 		
		   destroy_packet(p);
			if(tcType == 0){ // if TC with Advertisment implement broadcast flooding delay	  
            delay = (tcRet!=0)?(random_rand() % (tcRet*100)):0;
            delay = (CLOCK_SECOND / 1000) * ((delay!=0)?delay:5);
            PRINTF("ND TC-NA Broadcast Delay = %d\n",delay);
            etimer_set(&et, delay); // Delay Time based on RET		
				PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
			}
   /*      else if(tcType == 1){ // if TC-NR	  
            delay = (tcRet!=0)?(random_rand() % (tcRet*100)):0;
            delay = (CLOCK_SECOND / 1000) * ((delay!=0)?delay:5);
            PRINTF("ND TC-NR Broadcast Delay = %d\n",delay);
            etimer_set(&et, delay); // Delay Time based on RET		
				PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
			}
*/



	      //PRINTF("PTR: 2. ND %02u.%02u %01u %01u %01u\n",p->sink.u8[0],p->sink.u8[1],p->TCT,p->ACK,p->RET);
         //PRINTF("VAR: 2. ND %02u.%02u %01u %01u %01u\n",sink.u8[0],sink.u8[1],tcType,tcAck,tcRet); 

         // PAYLOAD: PTY SINK TCT ACK RET
         // SIZE(9):  2   4    1   1   1
         // EXAMPLE:  ND 0100  0   0   2 
         sprintf(msgbuf,"ND%02u%02u%01u%01u%01u",sink.u8[0],sink.u8[1],tcType,tcAck,tcRet);
			packetbuf_copyfrom(msgbuf, sizeof(msgbuf)); // Topology parameters
       	PRINTF("Sending Topology Discovey SR Broadcast message [%s] TCT:%u,ACK:%u,RET:%u\n",
					  msgbuf, tcType,tcAck,tcRet);
         
         dual_radio_switch(SHORT_RADIO);
      	broadcast_send(&bc);	 // Sending Broadcast message ...	
			c_send++; // STAT send control packet 
		} // end if not null
	   else {
			printf("!!!ERROR: data packet empty [SR_b_send_proc PROCESS] \n");
		}

	} // end of while
	PROCESS_END();
}
/*----------------------------------------------------------------------------*/

/*============================================================================*/
/*                           LR UNICAST                                       */
/*============================================================================*/
/* UNICAST UDP CALLBACK */
static void recv_uc_callback(struct unicast_conn *c, const linkaddr_t *from){
   static char buf[4]; 
   linkaddr_t dest;

   PRINTF("Unicast Received Neighbour Data: ");
   c_recv++; //STAT increase received control packet 
   char data_buf[packetbuf_datalen()];
   packetbuf_copyto(data_buf);
   PRINTF("%s",data_buf);

   if(LongRangeReceiving > 0){  //LONG RANGE
      PRINTF("From LONG Range\n");

      char PTY[3];
      if(getsubstr(data_buf,0,1,PTY)){
         if(!strncmp((char *)PTY,"ND",2)){
	         packet_t *p = create_packet();	
            if(getsubstr(data_buf,2,3,buf)) p->sink.u8[0] = atoi(buf);
            if(getsubstr(data_buf,4,5,buf)) p->sink.u8[1] = atoi(buf);
            if(getsubstr(data_buf,6,6,buf)) p->TCT = atoi(buf);
            if(getsubstr(data_buf,7,7,buf)) p->ACK = atoi(buf);
            if(getsubstr(data_buf,8,8,buf)) p->RET = atoi(buf);         

            process_post(&SR_b_send_proc, SR_B_SEND_EVENT, (process_data_t)p); // CALL SEND BROADCAST PROCESS
	         destroy_packet(p);
         } // ND end
         else if(!strncmp((char *)PTY,"AD",2)||!strncmp((char *)PTY,"AR",2)){
            // PAYLOAD: PTY DID  NXH  CST SEQ 
            // SIZE(15): 2   4    4    3   2
            // EXAMPLE: AD  0700 0300 102 00 
            if(getsubstr(data_buf,2,3,buf)) dest.u8[0] = atoi(buf);
            if(getsubstr(data_buf,4,5,buf)) dest.u8[1] = atoi(buf);
            linkaddr_t nexthop;
            if(getsubstr(data_buf,6,7,buf)) nexthop.u8[0] = atoi(buf);
            if(getsubstr(data_buf,8,9,buf)) nexthop.u8[1] = atoi(buf);
            int cost=0;
            if(getsubstr(data_buf,10,12,buf)) cost = atoi(buf);
            int seqno=0;
            if(getsubstr(data_buf,13,14,buf)) seqno = atoi(buf);
            PRINTF("ADD route with destination: %u.%u via %u.%u with cost %u and sequence %u\n",
                    dest.u8[0], dest.u8[1], nexthop.u8[0],nexthop.u8[1],cost,seqno);
            route_add(&dest, &nexthop, cost, seqno);  //ADddddhhhhccss  dddd=destination, hhhh=nethop hop, cc=cost, ss=sequenceno
            if(!strncmp((char *)PTY,"AR",2)){  // If "AR" route packet received stop the wating timer and resend the message
               dual_radio_switch(SHORT_RADIO);
               data_packet_resend(&mc);  //mesh.c
            }
         } // AD or AR end
         else if(!strncmp((char *)PTY,"RM",2)){
            // PAYLOAD: PTY DID 
            // SIZE(6):  2   4   
            // EXAMPLE: RM  0100  
            if(getsubstr(data_buf,2,3,buf)) dest.u8[0] = atoi(buf);
            if(getsubstr(data_buf,4,5,buf)) dest.u8[1] = atoi(buf);
            if(dest.u8[0]==0 && dest.u8[1]==0){  //RM0000 REMOVE ALL routing table records
               route_flush_all();
               PRINTF("REMOVING all routing records... Routing Table size:%d\n",route_num()); 
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
         } // RM end
      } // end if getsubstr for PTY
   } else{  // SHORT RANGE
      PRINTF("From SHORT Range\n");
   }

}
/*----------------------------------------------------------------------------*/
static const struct unicast_callbacks unicast_callbacks = { recv_uc_callback };
/*----------------------------------------------------------------------------*/

/*============================================================================*/
/* RELIABLE TCP UNICAST CALLBACK */
static void recv_ruc_callback (struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
   PRINTF("Reliable Unicast Received Neighbour Data\n");
   c_recv++; //STAT increase received control packet
   if(LongRangeReceiving > 0){
      PRINTF("From LONG Range");
// COPY THE ABOVE

   } else{
      PRINTF("From SHORT Range");
   }
}
/*----------------------------------------------------------------------------*/
static const struct runicast_callbacks runicast_callbacks = { recv_ruc_callback };
/*----------------------------------------------------------------------------*/

/* UNICAST SEND PROCESS */
PROCESS_THREAD(LR_u_send_proc, ev, data) {
	static linkaddr_t addr;
	static linkaddr_t from;
	static struct etimer et;
	static char rssi[5];
	static char lqi[5];
	static char eng[6];
	static uint8_t tcType;
	static uint8_t tcAck;
	static uint8_t tcRet;
   static char msgbuf[20];
   static int delay=1; 

	PROCESS_EXITHANDLER(unicast_close(&uc);runicast_close(&runicast);)

	PROCESS_BEGIN();
	PRINTF("Starting Unicast listener...\n");
	unicast_open(&uc, UC_CHANNEL, &unicast_callbacks); 
	PRINTF("Starting Reliable Unicast listener...\n");
	runicast_open(&runicast, RUC_CHANNEL, &runicast_callbacks); 

	while(1) {
		PROCESS_WAIT_EVENT_UNTIL(ev == LR_U_SEND_EVENT );
   	packet_t* p = (packet_t*)data;

      if (p != NULL){
         addr.u8[0] = p->sink.u8[0]; // Border Router
         addr.u8[1] = p->sink.u8[1]; 			

         if(!strncmp((char *)p->pty,"NN",2)){
            delay = (CLOCK_SECOND / 1000) * (node_id*5+50);
            etimer_set(&et, delay); 		
            PRINTF("NN Delay:%d\n",delay);
				PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));	

            //PAYLOAD:  PTY NID  ENG
            //SIZE(11):  2   4    5
            //EXAMPLE:  NN  0200 12345
            sprintf(msgbuf,"NN%02d00%05d", node_id, 0);  //ADD ENG fix tryfon ?????????????????????
            packetbuf_copyfrom(msgbuf, sizeof(msgbuf));
           
            dual_radio_switch(LONG_RADIO);
            PRINTF("Sending LR unicast to %d.%d responding to broadcast msg[%s]\n",addr.u8[0],addr.u8[1],msgbuf); 
			//   runicast_send(&runicast, &addr, MAX_RETRANSMISSIONS);  // SEND Reliable UNICAST TopDisc RESPONCE  
            unicast_send(&uc,&addr);   // SEND LR UNICAST New node RESPONCE   
            c_send++; // STAT send control packet */ 
   	      destroy_packet(p);
         }

         else if(!strncmp((char *)p->pty,"ND",2)){
	  		   from.u8[0] = p->to.u8[0]; //From 
   		   from.u8[1] = p->to.u8[1];	
	   	   sprintf(rssi,"%04u",(uint8_t) p->rssi);	
		      sprintf(lqi,"%04u",(uint8_t) p->lqi);	
		      sprintf(eng,"%05d",(uint16_t) p->eng);	
		      tcType = p->TCT;
		      tcAck = p->ACK;
		      tcRet = p->RET;
			   PRINTF("TC PARAM TCT:%u, ACK:%u, RET:%u\n",tcType,tcAck,tcRet);
   	      destroy_packet(p);

///????????? new delay  
// Edo den prepei na exo delay giati otan erxete deytero gia na steilei ksexnaei to proto an den to exei steilei 
//giati perimeni. Tha prepei na ylopoiiso oura minimaton
            //delay = (CLOCK_SECOND / 1000) + 10;
         /*   delay = (tcRet!=0)?(random_rand() % (tcRet*10)):0;
            delay = (CLOCK_SECOND / 1000) * ((delay!=0)?delay:5);
            etimer_set(&et, delay); 		
            PRINTF("ND Delay:%d\n",delay);
				PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));	*/

            //PAYLOAD:  PTY NBR  RSS  LQI  ENG
            //SIZE(19):  2   4    4    4    5
            //EXAMPLE:  NB  0200 0000 0000 00000    
            sprintf(msgbuf,"NB%02d%02d%s%s%s", from.u8[0], from.u8[1],rssi,lqi,eng);
			   packetbuf_copyfrom(msgbuf, sizeof(msgbuf)); // Neighbor parameters information
         
            dual_radio_switch(LONG_RADIO);
	         if(tcAck  == 0){   // 0 = Topology Discovery no ACK 
				   PRINTF("Sending LR unicast to %d.%d responding to broadcast msg[RSSI:%s]\n",
					   	  addr.u8[0], addr.u8[1],msgbuf); 
				   unicast_send(&uc,&addr);   // SEND LR UNICAST Topology discovery RESPONCE  
			      c_send++; // STAT send control packet 
			   } else if(tcAck  == 1){   // 1 = Topology Discovery with ACK
				   PRINTF("Sending LR runicast to %d.%d responding to broadcast msg[RSSI:%s]\n",
					   	  addr.u8[0], addr.u8[1],msgbuf); 
				   runicast_send(&runicast, &addr, MAX_RETRANSMISSIONS);  // SEND Reliable UNICAST TopDisc RESPONCE    
			      c_send++; // STAT send control packet 
			   } else {
				   printf("!!!ERROR Topology Discovery value is not 0 or 1 [LR_u_send_proc PROCESS]\n"); 
			   } 
         } 
         else{
				printf("!!!ERROR Unknown packet code: %s [LR_u_send_proc PROCESS]\n",(char *)p->pty); 
         }
   // destroy_packet(p);
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
    
//    printf("R:%d, Leaf MODE: %d\n",counter,8);
    
    counter++; //new round of stats
  } 
#endif
   PROCESS_END();
}

/*---------------------------------------------------------------------------*/


