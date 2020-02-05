/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Best-effort single-hop unicast example
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"
#include "net/rime/rime.h"

#include "dev/button-sensor.h"

#include "dev/leds.h"

#include <stdio.h>
#include <string.h>
#include "platform-conf.h"
#include "cc1200.h"
#include "cc2420.h"

static char total_buff[2][20];
static int index[2];

/*---------------------------------------------------------------------------*/
PROCESS(example_unicast_process, "Sensor-master");
AUTOSTART_PROCESSES(&example_unicast_process);
/*---------------------------------------------------------------------------*/
static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
	char temp_buff[20];
	int i;
	sprintf(temp_buff,"%d:%s",from->u8[0],(char *)packetbuf_dataptr());
  
	printf("unicast message received from %s\n",temp_buff);
	for (i=0; i<2; i++){
		if (index[i] == 0){
			index[i] = from->u8[0];
			strncpy(total_buff[i], temp_buff,sizeof(temp_buff)-1);
			total_buff[i][sizeof(temp_buff)-1]='\0';
			break;
		}	else if(index[i] == from->u8[0]){
			printf("SIZE OF TEMP_BUFF %d\n",sizeof(temp_buff));
			strncpy(total_buff[i], temp_buff,sizeof(temp_buff)-1);
			total_buff[i][sizeof(temp_buff)-1]='\0';
			break;
		}
	}
}
static const struct unicast_callbacks unicast_callbacks = {recv_uc};
static struct unicast_conn uc;

static void
broadcast_recv(void)
{
	printf("JOONKI\n");
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_unicast_process, ev, data)
{
  PROCESS_EXITHANDLER(unicast_close(&uc);broadcast_close(&broadcast); )
    
  PROCESS_BEGIN();

	// dual_radio_switch(LONG_RADIO);
	NETSTACK_CONF_RADIO = cc1200_driver;
	NETSTACK_RADIO = cc1200_driver;

  unicast_open(&uc, 146, &unicast_callbacks);
	broadcast_open (&broadcast, 129, &broadcast_call);

	char databuff[60];
  while(1) {
    static struct etimer et;
   	
    etimer_set(&et, 5 * CLOCK_SECOND);
    
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		sprintf(databuff,"%s;%s",total_buff[0], total_buff[1]);
		// sprintf(databuff,"abcdefghij abcdefghif abcdefghij abcdefghiji");

		printf("____________TEST___________\n");
		printf("DATABUFF: \n%s\n",databuff);
		printf("---------------------------------------------------------\n");
		
    packetbuf_copyfrom(databuff, 60);
		
		broadcast_send(&broadcast);
		printf("broadcast message sent \n%s\n",databuff);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
