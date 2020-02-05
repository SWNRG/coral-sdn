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
#include <string.h>
#include <stdlib.h>
#include "dev/button-sensor.h"
//#include "dev/rs232.h"
#include "dev/serial-line.h"

#include "dev/leds.h"
//#include "dual_conf.h" // COOJAMOTE
#include "dual_radio.h"  //Z1
#include <stdio.h>
int transmission_count=1;
/*---------------------------------------------------------------------------*/
PROCESS(example_unicast_process, "Example unicast");
AUTOSTART_PROCESSES(&example_unicast_process);
/*---------------------------------------------------------------------------*/
//extern int LongRangeTransmit;
//extern int LongRangeReceiving;
int LongRangeTransmit=0;
int LongRangeReceiving=0;

static int serial_input_byte(unsigned char c){
   printf("RECIEVED FROM SERIAL:%d (%c)\n",c,c);
}

static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
  char *ls_ind = NULL;
  if(LongRangeReceiving > 0){
    ls_ind = "LONG";
  } else{
    ls_ind = "SHORT";
  }
  printf("[%s]unicast message received from %d.%d: '%s' \n",
	 ls_ind, from->u8[0], from->u8[1],(char *)packetbuf_dataptr());
  memset((char *)packetbuf_dataptr(), 0, strlen((char *)packetbuf_dataptr()));

  printf("Long Range Received from %d.%d broadcast packet: '%s' with RSSI:%u\n",
            from->u8[0], from->u8[1], (char *)packetbuf_dataptr(), 
            (uint8_t) (- packetbuf_attr(PACKETBUF_ATTR_RSSI)));

}
static const struct unicast_callbacks unicast_callbacks = {recv_uc};
static struct unicast_conn uc;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_unicast_process, ev, data)
{
  PROCESS_EXITHANDLER(unicast_close(&uc);)
    
  PROCESS_BEGIN();

  unicast_open(&uc, 146, &unicast_callbacks);
	static char str[15];
	static int count=0;
//   rs232_set_input(serial_input_byte);
 
  while(1) {
    static struct etimer et;
    linkaddr_t addr;
   	count ++;
		
    etimer_set(&et, CLOCK_SECOND/4);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    if(LongRangeTransmit){
			sprintf(str, "Short %d", count);
      packetbuf_copyfrom(str, strlen(str));
			dual_radio_switch(SHORT_RADIO);
    } else {
			sprintf(str, "Long %d", count);
      packetbuf_copyfrom(str, strlen(str));
			dual_radio_switch(LONG_RADIO);
    }
   addr.u8[0] = 1;
    addr.u8[1] = 0;
    if(!linkaddr_cmp(&addr, &linkaddr_node_addr)) {
      unicast_send(&uc, &addr);
    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
