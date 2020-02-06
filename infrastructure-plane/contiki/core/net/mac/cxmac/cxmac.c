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
 *         A simple power saving MAC protocol based on X-MAC [SenSys 2006]
 * \author
 *         Adam Dunkels <adam@sics.se>
 *         Niclas Finne <nfi@sics.se>
 *         Joakim Eriksson <joakime@sics.se>
 */

#include "dev/leds.h"
#include "dev/radio.h"
#include "dev/watchdog.h"
#include "net/netstack.h"
#include "lib/random.h"
#include "net/mac/cxmac/cxmac.h"
#include "net/rime/rime.h"
#include "net/rime/timesynch.h"
#include "sys/compower.h"
#include "sys/pt.h"
#include "sys/rtimer.h"

#include "contiki-conf.h"
#include "sys/cc.h"

#ifdef EXPERIMENT_SETUP
#include "experiment-setup.h"
#endif

#include <string.h>

#include "../lanada/param.h"

/*
#if CONTIKI_TARGET_COOJA
#include "lib/simEnvChange.h"
#include "sys/cooja_mt.h"
#endif  CONTIKI_TARGET_COOJA
*/

#ifndef WITH_ACK_OPTIMIZATION
#define WITH_ACK_OPTIMIZATION        1
#endif
#ifndef WITH_ENCOUNTER_OPTIMIZATION
#define WITH_ENCOUNTER_OPTIMIZATION  1
#endif
#ifndef WITH_STREAMING
#define WITH_STREAMING               1
#endif
#ifndef WITH_STROBE_BROADCAST
#define WITH_STROBE_BROADCAST        1
#endif

struct announcement_data {
  uint16_t id;
  uint16_t value;
};

/* The maximum number of announcements in a single announcement
   message - may need to be increased in the future. */
#define ANNOUNCEMENT_MAX 10

/* The structure of the announcement messages. */
struct announcement_msg {
  uint16_t num;
  struct announcement_data data[ANNOUNCEMENT_MAX];
};

/* The length of the header of the announcement message, i.e., the
   "num" field in the struct. */
#define ANNOUNCEMENT_MSG_HEADERLEN (sizeof (uint16_t))

#define DISPATCH          0
#define TYPE_STROBE       0x10
/* #define TYPE_DATA         0x11 */
#define TYPE_ANNOUNCEMENT 0x12
#define TYPE_STROBE_ACK   0x13

struct cxmac_hdr {
  uint8_t dispatch;
  uint8_t type;
/*
#if STROBE_CNT_MODE
  uint8_t strobe_cnt; // For strobe cnt mode
#endif
*/
};

#define MAX_STROBE_SIZE 50

#ifdef CXMAC_CONF_ON_TIME
#define DEFAULT_ON_TIME (CXMAC_CONF_ON_TIME)
#else
#define DEFAULT_ON_TIME (RTIMER_ARCH_SECOND / 80)
#endif

#ifdef CXMAC_CONF_OFF_TIME
#define DEFAULT_OFF_TIME (CXMAC_CONF_OFF_TIME)
#else
#define DEFAULT_OFF_TIME (RTIMER_ARCH_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE - DEFAULT_ON_TIME)
#endif

#define DEFAULT_PERIOD (DEFAULT_OFF_TIME + DEFAULT_ON_TIME)

#define WAIT_TIME_BEFORE_STROBE_ACK RTIMER_ARCH_SECOND / 1000

/* On some platforms, we may end up with a DEFAULT_PERIOD that is 0
   which will make compilation fail due to a modulo operation in the
   code. To ensure that DEFAULT_PERIOD is greater than zero, we use
   the construct below. */
#if DEFAULT_PERIOD == 0
#undef DEFAULT_PERIOD
#define DEFAULT_PERIOD 1
#endif

/* The cycle time for announcements. */
#define ANNOUNCEMENT_PERIOD 4 * CLOCK_SECOND

/* The time before sending an announcement within one announcement
   cycle. */
#define ANNOUNCEMENT_TIME (random_rand() % (ANNOUNCEMENT_PERIOD))

#define DEFAULT_STROBE_WAIT_TIME (7 * DEFAULT_ON_TIME / 16)

struct cxmac_config cxmac_config = {
  DEFAULT_ON_TIME,
  DEFAULT_OFF_TIME,
/*  Original setting */
//  4 * DEFAULT_ON_TIME + DEFAULT_OFF_TIME,
/*  Customized setting */
  5 * DEFAULT_ON_TIME + DEFAULT_OFF_TIME,
  DEFAULT_STROBE_WAIT_TIME
};

#include <stdio.h>

static struct pt pt;
#if DUAL_RADIO
#if LSA_MAC
PROCESS(strobe_wait, "strobe wait");
#endif
#endif
static volatile unsigned char strobe_target;

static volatile uint8_t cxmac_is_on = 0;

static volatile unsigned char waiting_for_packet = 0;
static volatile unsigned char someone_is_sending = 0;
static volatile unsigned char we_are_sending = 0;
static volatile unsigned char radio_is_on = 0;

#undef LEDS_ON
#undef LEDS_OFF
#undef LEDS_TOGGLE

#define LEDS_ON(x) leds_on(x)
#define LEDS_OFF(x) leds_off(x)
#define LEDS_TOGGLE(x) leds_toggle(x)
#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINTDEBUG(...) printf(__VA_ARGS__)
#else
/* JOONI
 * Enable LEDS when DEBUG == 0 
#undef LEDS_ON
#undef LEDS_OFF
#undef LEDS_TOGGLE
#define LEDS_ON(x)
#define LEDS_OFF(x)
#define LEDS_TOGGLE(x)
*/ 
#define PRINTF(...)
#define PRINTDEBUG(...)
#endif

#if DUAL_RADIO
#ifdef ZOLERTIA_Z1
#include	"../platform/z1/dual_radio.h"
#elif COOJA /* ZOLERTIA_Z1 */
#include	"../platform/cooja/dual_conf.h"
#else /* ZOLERTIA_Z1 */
#include "../platform/zoul/dual_radio.h"
#endif /* ZOLERTIA_Z1 */
#endif /* DUAL_RADIO */
#include "sys/log_message.h"
// extern int cxmac_collision_count, cxmac_transmission_count;

#if CXMAC_CONF_ANNOUNCEMENTS
/* Timers for keeping track of when to send announcements. */
static struct ctimer announcement_cycle_ctimer, announcement_ctimer;

static int announcement_radio_txpower;
#endif /* CXMAC_CONF_ANNOUNCEMENTS */

/* Flag that is used to keep track of whether or not we are listening
   for announcements from neighbors. */
static uint8_t is_listening;

#if CXMAC_CONF_COMPOWER
static struct compower_activity current_packet;
#endif /* CXMAC_CONF_COMPOWER */

#if WITH_ENCOUNTER_OPTIMIZATION

#include "lib/list.h"
#include "lib/memb.h"

struct encounter {
  struct encounter *next;
  linkaddr_t neighbor;
  rtimer_clock_t time;
};

#define MAX_ENCOUNTERS 4
LIST(encounter_list);
MEMB(encounter_memb, struct encounter, MAX_ENCOUNTERS);
#endif /* WITH_ENCOUNTER_OPTIMIZATION */

static uint8_t is_streaming;
static linkaddr_t is_streaming_to, is_streaming_to_too;
static rtimer_clock_t stream_until;
#define DEFAULT_STREAM_TIME (RTIMER_ARCH_SECOND)

static uint8_t is_short_waiting = 0;

/* remaining energy JJH */
#if RPL_ENERGY_MODE
extern uint8_t remaining_energy;
#endif
#if DUAL_RADIO
static void dual_radio_on(char target);
static void dual_radio_off(char target);
#endif
/*---------------------------------------------------------------------------*/
static void
on(void)
{
  if(cxmac_is_on && radio_is_on == 0) {
    radio_is_on = 1;
#if DUAL_RADIO
		dual_radio_turn_on(BOTH_RADIO);
#else
    NETSTACK_RADIO.on();
    LEDS_ON(LEDS_RED);
#endif
  }
}
/*---------------------------------------------------------------------------*/
static void
off(void)
{
  if(cxmac_is_on && radio_is_on != 0 && is_listening == 0 &&
     is_streaming == 0) {
    radio_is_on = 0;
#if DUAL_RADIO
		dual_radio_turn_off(BOTH_RADIO);
#else
    NETSTACK_RADIO.off();
    LEDS_OFF(LEDS_RED);
#endif
  }
}
/*---------------------------------------------------------------------------*/
#if DUAL_RADIO
static void
powercycle_dual_turn_radio_on(char target)
{
  if(we_are_sending == 0 &&
     waiting_for_packet == 0) {
	  dual_radio_on(target);
  }
#if CXMAC_CONF_COMPOWER
  compower_accumulate(&compower_idle_activity);
#endif /* CXMAC_CONF_COMPOWER */
}
static void
powercycle_dual_turn_radio_off(char target)
{
  if(we_are_sending == 0 &&
     waiting_for_packet == 0) {
	  dual_radio_off(target);
  }
}
#else
static void
powercycle_turn_radio_off(void)
{
  if(we_are_sending == 0 &&
     waiting_for_packet == 0) {
    off();
  }
#if CXMAC_CONF_COMPOWER
  compower_accumulate(&compower_idle_activity);
#endif /* CXMAC_CONF_COMPOWER */
}
static void
powercycle_turn_radio_on(void)
{
  if(we_are_sending == 0 &&
     waiting_for_packet == 0) {
    on();
  }
}
#endif
#if DUAL_RADIO
#if LSA_MAC
PROCESS_THREAD(strobe_wait, ev, data)
{
	static struct etimer et;
	rtimer_clock_t t;
//	printf("before process begin\n");
	PROCESS_BEGIN();
	if(!is_short_waiting)
	{
		uint8_t *cnt = (uint8_t *)data;
		t = (cxmac_config.strobe_time) - (*cnt + 1)*(cxmac_config.on_time + 1);
		t >= cxmac_config.strobe_time ? t = 1 : t;
#if DUAL_RADIO
		dual_radio_off(BOTH_RADIO);
#else
		off();
#endif
	}
	else
	{
		t = SHORT_SLOT_LEN;
	}
//	printf("before timer set\n");
	clock_time_t t_wait = (1ul * CLOCK_SECOND * (t)) / RTIMER_ARCH_SECOND;
	etimer_set(&et, t_wait);
	PROCESS_WAIT_UNTIL(etimer_expired(&et));
//	printf("after time expired\n");
	if(!is_short_waiting)
	{
#if DUAL_RADIO
		dual_radio_on(strobe_target);
#else
		on();
#endif
		waiting_for_packet = 1;
	}
#if DUAL_RADIO
	else
	{
		dual_radio_off(SHORT_RADIO);
		waiting_for_packet = 0;
	}
#endif
	is_short_waiting = 0;
	PROCESS_END();
}
#endif
#endif

/*---------------------------------------------------------------------------*/
#if DUAL_RADIO
static void
dual_radio_on(char target)
{
//	printf("dual_radio_on target %d %d\n",target, radio_is_on);
	if(cxmac_is_on && radio_is_on == 0) {
		radio_is_on = 1;
		dual_radio_turn_on(target);
		if(target == LONG_RADIO)
		{
			LEDS_ON(LEDS_GREEN);
		}
		if(target == SHORT_RADIO)
		{
			LEDS_ON(LEDS_RED);
		}
		if(target == BOTH_RADIO)
		{
			LEDS_ON(LEDS_GREEN);
			LEDS_ON(LEDS_RED);
		}
	}
}
static void
dual_radio_off(char target)
{
//	printf("dual_radio_off target %d %d\n",target, radio_is_on);
	if(cxmac_is_on && radio_is_on != 0 && is_listening == 0 &&
			is_streaming == 0) {
		radio_is_on = 0;
		dual_radio_turn_off(target);
		if(target == LONG_RADIO)
		{
			LEDS_OFF(LEDS_GREEN);
		}
		if(target == SHORT_RADIO)
		{
			LEDS_OFF(LEDS_RED);
		}
		if(target == BOTH_RADIO)
		{
			LEDS_OFF(LEDS_GREEN);
			LEDS_OFF(LEDS_RED);
		}
	}
}
#endif
/*---------------------------------------------------------------------------*/
static struct ctimer cpowercycle_ctimer;
#define CSCHEDULE_POWERCYCLE(rtime) cschedule_powercycle((1ul * CLOCK_SECOND * (rtime)) / RTIMER_ARCH_SECOND)
static char cpowercycle(void *ptr);
static void
cschedule_powercycle(clock_time_t time)
{

  if(cxmac_is_on) {
    if(time == 0) {
      time = 1;
    }
    ctimer_set(&cpowercycle_ctimer, time,
               (void (*)(void *))cpowercycle, NULL);
  }
}
/*---------------------------------------------------------------------------*/
static char
cpowercycle(void *ptr)
{
  if(is_streaming) {
    if(!RTIMER_CLOCK_LT(RTIMER_NOW(), stream_until)) {
      is_streaming = 0;
      linkaddr_copy(&is_streaming_to, &linkaddr_null);
      linkaddr_copy(&is_streaming_to_too, &linkaddr_null);
    }
  }

  PT_BEGIN(&pt);

  while(1) {
    if(someone_is_sending > 0) {
      someone_is_sending--;
    }

    /* If there were a strobe in the air, turn radio on */
#if DUAL_RADIO
#if DUAL_ROUTING_CONVERGE
		// JOOONKI is working on this	
		if(dual_duty_cycle_count <= DUAL_DUTY_RATIO-2)
    {
    	dual_duty_cycle_count++;
			if (short_duty_on == 1){
	      powercycle_dual_turn_radio_on(SHORT_RADIO);
			}
    }
    else
    {
    	dual_duty_cycle_count = 0;
			if (short_duty_on == 1 && long_duty_on == 1) {
	      powercycle_dual_turn_radio_on(BOTH_RADIO);
			}	else if (long_duty_on == 1) {
	      powercycle_dual_turn_radio_on(LONG_RADIO);
			}	else if (short_duty_on == 1 ) {
				powercycle_dual_turn_radio_on(SHORT_RADIO);
			}
    }

#else /* DUAL_ROUTING_CONVERGE */
#if LSA_MAC
#if LSA_R
		if (LSA_converge == 1)
		{
			if (LSA_lr_child == 1) {
				powercycle_dual_turn_radio_on(LONG_RADIO);
			} else {
				powercycle_dual_turn_radio_on(SHORT_RADIO);
			}
		} else {
			powercycle_dual_turn_radio_on(LONG_RADIO);
		}
#else /* LSA_R */ 
		powercycle_dual_turn_radio_on(LONG_RADIO);
#endif /* LSA_R */
#else
    if(dual_duty_cycle_count <= DUAL_DUTY_RATIO-2)
    {
    	dual_duty_cycle_count++;
      powercycle_dual_turn_radio_on(SHORT_RADIO);
    }
    else
    {
    	dual_duty_cycle_count = 0;
      powercycle_dual_turn_radio_on(BOTH_RADIO);
    }
#endif /* LSA_MAC */
#endif /* DUAL_ROUTING_CONVERGE */
#else	/* DUAL_RADIO */
    powercycle_turn_radio_on();
#endif /* DUAL_RADIO */



    // printf("cpowerycle on\n");
    CSCHEDULE_POWERCYCLE(DEFAULT_ON_TIME);
    PT_YIELD(&pt);
    if(cxmac_config.off_time > 0) {
#if DUAL_RADIO
      powercycle_dual_turn_radio_off(BOTH_RADIO);
#else
      powercycle_turn_radio_off();
#endif

      if(waiting_for_packet != 0) {
	waiting_for_packet++;
	if(waiting_for_packet > 2) {
	  /* We should not be awake for more than two consecutive
	     power cycles without having heard a packet, so we turn off
	     the radio. */
	  waiting_for_packet = 0;
#if DUAL_RADIO
	  powercycle_dual_turn_radio_off(BOTH_RADIO);
#else
	  powercycle_turn_radio_off();
#endif
	}
      }
      // printf("cpowerycle off\n");
      CSCHEDULE_POWERCYCLE(DEFAULT_OFF_TIME);
      PT_YIELD(&pt);
    }
  }

  PT_END(&pt);
}
/*---------------------------------------------------------------------------*/
#if CXMAC_CONF_ANNOUNCEMENTS
static int
parse_announcements(const linkaddr_t *from)
{
  /* Parse incoming announcements */
  struct announcement_msg adata;
  int i;

  memcpy(&adata, packetbuf_dataptr(), MIN(packetbuf_datalen(), sizeof(adata)));

  /*  printf("%d.%d: probe from %d.%d with %d announcements\n",
	 linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
	 from->u8[0], from->u8[1], adata->num);*/
  /*  for(i = 0; i < packetbuf_datalen(); ++i) {
    printf("%02x ", ((uint8_t *)packetbuf_dataptr())[i]);
  }
  printf("\n");*/

  for(i = 0; i < adata.num; ++i) {
    /*   printf("%d.%d: announcement %d: %d\n",
	  linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
	  adata->data[i].id,
	  adata->data[i].value);*/

    announcement_heard(from,
		       adata.data[i].id,
		       adata.data[i].value);
  }
  return i;
}
/*---------------------------------------------------------------------------*/
static int
format_announcement(char *hdr)
{
  struct announcement_msg adata;
  struct announcement *a;

  /* Construct the announcements */
  /*  adata = (struct announcement_msg *)hdr;*/

  adata.num = 0;
  for(a = announcement_list();
      a != NULL && adata.num < ANNOUNCEMENT_MAX;
      a = list_item_next(a)) {
    adata.data[adata.num].id = a->id;
    adata.data[adata.num].value = a->value;
    adata.num++;
  }

  memcpy(hdr, &adata, sizeof(struct announcement_msg));

  if(adata.num > 0) {
    return ANNOUNCEMENT_MSG_HEADERLEN +
      sizeof(struct announcement_data) * adata.num;
  } else {
    return 0;
  }
}
#endif /* CXMAC_CONF_ANNOUNCEMENTS */
/*---------------------------------------------------------------------------*/
#if WITH_ENCOUNTER_OPTIMIZATION
static void
register_encounter(const linkaddr_t *neighbor, rtimer_clock_t time)
{
  struct encounter *e;

  /* If we have an entry for this neighbor already, we renew it. */
  for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)) {
    if(linkaddr_cmp(neighbor, &e->neighbor)) {
      e->time = time;
      break;
    }
  }
  /* No matching encounter was found, so we allocate a new one. */
  if(e == NULL) {
    e = memb_alloc(&encounter_memb);
    if(e == NULL) {
      /* We could not allocate memory for this encounter, so we just drop it. */
      return;
    }
    linkaddr_copy(&e->neighbor, neighbor);
    e->time = time;
    list_add(encounter_list, e);
  }
}
#endif /* WITH_ENCOUNTER_OPTIMIZATION */
/*---------------------------------------------------------------------------*/
static int
send_packet(void)
{
  rtimer_clock_t t0;
  rtimer_clock_t t;
  rtimer_clock_t encounter_time = 0;
  int strobes;
  struct cxmac_hdr *hdr;
  int got_strobe_ack = 0;
  uint8_t strobe[MAX_STROBE_SIZE];
  int strobe_len, len;
  int is_broadcast = 0;
  int is_dispatch, is_strobe_ack;
  /*int is_reliable;*/
  struct encounter *e;
  struct queuebuf *packet;
  int is_already_streaming = 0;
  uint8_t collisions;

  cxmac_transmission_count++;
#if DUAL_RADIO
  char target = SHORT_RADIO;
  rtimer_clock_t strobe_time;
#endif
	// JJH
#if RPL_ENERGY_MODE
  int original_datalen;
  uint8_t *original_dataptr;
#endif
#if STROBE_CNT_MODE
  uint8_t strobe_cnt = 1;
  int cnt_pos;
#endif
#if DUAL_RADIO
#if LSA_MAC
	static uint8_t was_short;
#endif
#endif

  /* Create the X-MAC header for the data packet. */
#if !NETSTACK_CONF_BRIDGE_MODE
  /* If NETSTACK_CONF_BRIDGE_MODE is set, assume PACKETBUF_ADDR_SENDER is already set. */
  packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
#endif

	/* JOONKI */
#if DUAL_RADIO
	if(sending_in_LR() == LONG_RADIO){
		target = LONG_RADIO;
		strobe_time = cxmac_config.strobe_time * DUAL_DUTY_RATIO;
	packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &long_linkaddr_node_addr);
	}	else	{
		target = SHORT_RADIO;
		strobe_time = cxmac_config.strobe_time;
	packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
	}
#else
	packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
#endif
  if(packetbuf_holds_broadcast()) {
    is_broadcast = 1;
    PRINTDEBUG("cxmac: send broadcast\n");
  } else {
#if NETSTACK_CONF_WITH_IPV6
    PRINTDEBUG("cxmac: send unicast to %02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1],
           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[2],
           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[3],
           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[4],
           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[5],
           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[6],
           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[7]);
#else
    PRINTDEBUG("cxmac: send unicast to %u.%u\n",
           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1]);
#endif /* NETSTACK_CONF_WITH_IPV6 */
  }
  len = NETSTACK_FRAMER.create();
  strobe_len = len + sizeof(struct cxmac_hdr);
  if(len < 0 || strobe_len > (int)sizeof(strobe)) {
    /* Failed to send */
   PRINTF("cxmac: send failed, too large header\n");
    return MAC_TX_ERR_FATAL;
  }
  memcpy(strobe, packetbuf_hdrptr(), len);
#if STROBE_CNT_MODE
  strobe[len] = DISPATCH | (strobe_cnt << 2); /* dispatch */
  cnt_pos = len;
#else
  strobe[len] = DISPATCH; /* dispatch */
#endif
  strobe[len + 1] = TYPE_STROBE; /* type */
/*
#if STROBE_CNT_MODE
  strobe[len + 2] = strobe_cnt;
  cnt_pos = len + 2;
#endif
*/

  packetbuf_compact();
  packet = queuebuf_new_from_packetbuf();
  if(packet == NULL) {
    /* No buffer available */
    PRINTF("cxmac: send failed, no queue buffer available (of %u)\n",
           QUEUEBUF_CONF_NUM);
    return MAC_TX_ERR;
  }

#if WITH_STREAMING && PACKETBUF_WITH_PACKET_TYPE
  if(is_streaming == 1 &&
     (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
		   &is_streaming_to) ||
      linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
		   &is_streaming_to_too))) {
    is_already_streaming = 1;
  }
  if(packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) ==
     PACKETBUF_ATTR_PACKET_TYPE_STREAM) {
    is_streaming = 1;
    if(linkaddr_cmp(&is_streaming_to, &linkaddr_null)) {
      linkaddr_copy(&is_streaming_to, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
    } else if(!linkaddr_cmp(&is_streaming_to, packetbuf_addr(PACKETBUF_ADDR_RECEIVER))) {
      linkaddr_copy(&is_streaming_to_too, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
    }
    stream_until = RTIMER_NOW() + DEFAULT_STREAM_TIME;
  }
#endif /* WITH_STREAMING */

#if DUAL_RADIO
  dual_radio_off(BOTH_RADIO);
#else
  off();
#endif

#if WITH_ENCOUNTER_OPTIMIZATION
  /* We go through the list of encounters to find if we have recorded
     an encounter with this particular neighbor. If so, we can compute
     the time for the next expected encounter and setup a ctimer to
     switch on the radio just before the encounter. */
  for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)) {
    const linkaddr_t *neighbor = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);

    if(linkaddr_cmp(neighbor, &e->neighbor)) {
      rtimer_clock_t wait, now, expected;

      /* We expect encounters to happen every DEFAULT_PERIOD time
	 units. The next expected encounter is at time e->time +
	 DEFAULT_PERIOD. To compute a relative offset, we subtract
	 with clock_time(). Because we are only interested in turning
	 on the radio within the DEFAULT_PERIOD period, we compute the
	 waiting time with modulo DEFAULT_PERIOD. */

      now = RTIMER_NOW();
      wait = ((rtimer_clock_t)(e->time - now)) % (DEFAULT_PERIOD);
      expected = now + wait - 2 * DEFAULT_ON_TIME;

#if WITH_ACK_OPTIMIZATION && PACKETBUF_WITH_PACKET_TYPE
      /* Wait until the receiver is expected to be awake */
      if(packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) !=
	 PACKETBUF_ATTR_PACKET_TYPE_ACK &&
	 is_streaming == 0) {
	/* Do not wait if we are sending an ACK, because then the
	   receiver will already be awake. */
	while(RTIMER_CLOCK_LT(RTIMER_NOW(), expected));
      }
#else /* WITH_ACK_OPTIMIZATION */
      /* Wait until the receiver is expected to be awake */
      while(RTIMER_CLOCK_LT(RTIMER_NOW(), expected));
#endif /* WITH_ACK_OPTIMIZATION */
    }
  }
#endif /* WITH_ENCOUNTER_OPTIMIZATION */

  /* By setting we_are_sending to one, we ensure that the rtimer
     powercycle interrupt do not interfere with us sending the packet. */
  we_are_sending = 1;
  
  t0 = RTIMER_NOW();
  strobes = 0;

 //  LEDS_ON(LEDS_BLUE);

  /* Send a train of strobes until the receiver answers with an ACK. */
	/* Always use long preamble in LSA_MAC mode */
#if DUAL_RADIO
#if LSA_MAC
#if LSA_R
	if (is_broadcast || LSA_SR_preamble == 0) {  
		if (sending_in_LR() == SHORT_RADIO){
			was_short = 1;
			dual_radio_switch(LONG_RADIO);
			target = LONG_RADIO;
		}	else	{
			was_short = 0;
		}
	}
#else
	if (sending_in_LR() == SHORT_RADIO){
		was_short = 1;
		dual_radio_switch(LONG_RADIO);
		target = LONG_RADIO;
	}	else	{
		was_short = 0;
	}
#endif /* LSA_R */
#endif /* LSA_MAC */
#endif

	  /* Turn on the radio to listen for the strobe ACK. */
#if DUAL_RADIO
  dual_radio_on(target);
#else
  on();
#endif

  collisions = 0;
  if(!is_already_streaming) {
	  watchdog_stop();
	  got_strobe_ack = 0;
	  t = RTIMER_NOW();

	  for(strobes = 0, collisions = 0;
			  got_strobe_ack == 0 && collisions == 0 &&
#if DUAL_RADIO
					  RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + strobe_time);
#else
					  RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + cxmac_config.strobe_time);
#endif
			  strobes++) {
			/* JOONKI
			 * short range broadcast skip sending strobed preambles */
#if DUAL_RADIO
#if LSA_MAC
			if (is_broadcast && was_short == 1){
				break;
			} 
#endif /* LSA_MAC */ 
#endif
			while(got_strobe_ack == 0 &&
					RTIMER_CLOCK_LT(RTIMER_NOW(), t + cxmac_config.strobe_wait_time)) {
				rtimer_clock_t now = RTIMER_NOW();
				/* See if we got an ACK */
				packetbuf_clear();
				len = NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);
				if(len > 0) {
					packetbuf_set_datalen(len);
					if(NETSTACK_FRAMER.parse() >= 0) {
						//					  printf("packet parsed\n");
						hdr = packetbuf_dataptr();
#if STROBE_CNT_MODE
						char dispatch_ext = hdr->dispatch << 6;
						is_dispatch = dispatch_ext == DISPATCH;
#else
						is_dispatch = hdr->dispatch == DISPATCH;
#endif
						is_strobe_ack = hdr->type == TYPE_STROBE_ACK;
						if(is_dispatch && is_strobe_ack) {
							//						  	    	printf("ACK recognized\n");
#if DUAL_RADIO
							if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
									&linkaddr_node_addr) ||
									linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
											&long_linkaddr_node_addr)) 
#else
								if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
										&linkaddr_node_addr)) 
#endif
								{
									/* We got an ACK from the receiver, so we can immediately send
		   the packet. */
									//								  printf("got strobe_ack\n");
									got_strobe_ack = 1;
									encounter_time = now;
								} else {
									PRINTDEBUG("cxmac: strobe ack for someone else\n");
								}
							} else /*if(hdr->dispatch == DISPATCH && hdr->type == TYPE_STROBE)*/ {
								PRINTDEBUG("cxmac: strobe from someone else\n");
								collisions++;
							}
						} else {
							PRINTF("cxmac: send failed to parse %u\n", len);
						}
					}
				}
				t = RTIMER_NOW();
				/* Send the strobe packet. */
				if(got_strobe_ack == 0 && collisions == 0) {
					if(is_broadcast) {
#if WITH_STROBE_BROADCAST
						NETSTACK_RADIO.send(strobe, strobe_len);
#if STROBE_CNT_MODE
						strobe[cnt_pos] += (1 << 2);
						//	  printf("cxmac tx strobe_cnt %d t: %d\n",strobe_cnt,RTIMER_NOW());
#endif	/* STROBE_CNT_MODE */
#else		/* WITH_STROBE_BROADCAST */

						/* restore the packet to send */
						queuebuf_to_packetbuf(packet);
						NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
#endif	/* WITH_STROBE_BROADCAST */

#if DUAL_RADIO
						dual_radio_off(target);
#else
						off();
#endif
					} else {
						NETSTACK_RADIO.send(strobe, strobe_len);
#if 0
						/* Turn off the radio for a while to let the other side
	     respond. We don't need to keep our radio on when we know
	     that the other side needs some time to produce a reply. */
#if DUAL_RADIO
						dual_radio_off(target);
#else
						off();
#endif
						rtimer_clock_t wt = RTIMER_NOW();
						while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + WAIT_TIME_BEFORE_STROBE_ACK));
#endif /* 0 */
#if DUAL_RADIO
						dual_radio_on(target);
#else
						on();
#endif
					}
				}
			}
	  }

#if WITH_ACK_OPTIMIZATION
  /* If we have received the strobe ACK, and we are sending a packet
     that will need an upper layer ACK (as signified by the
     PACKETBUF_ATTR_RELIABLE packet attribute), we keep the radio on. */
  if(got_strobe_ack && (
#if NETSTACK_CONF_WITH_RIME
      packetbuf_attr(PACKETBUF_ATTR_RELIABLE) ||
      packetbuf_attr(PACKETBUF_ATTR_ERELIABLE) ||
#endif /* NETSTACK_CONF_WITH_RIME */
#if PACKETBUF_WITH_PACKET_TYPE
			packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) ==
			PACKETBUF_ATTR_PACKET_TYPE_STREAM ||
#endif
      0)) {
#if DUAL_RADIO
	  dual_radio_on(target);
#else
	  on(); /* Wait for ACK packet */
#endif
    waiting_for_packet = 1;
  } else {
#if DUAL_RADIO
	dual_radio_off(target);
#else
    off();
#endif
  }
#else /* WITH_ACK_OPTIMIZATION */

#if DUAL_RADIO
  dual_radio_off(target);
#else
  off();
#endif

#endif /* WITH_ACK_OPTIMIZATION */

	/* Switch the radio back to the original one */
#if DUAL_RADIO
#if LSA_MAC
#if LSA_R
	if (is_broadcast || LSA_SR_preamble == 0) {  
		if (was_short == 1)	{
 				dual_radio_switch(SHORT_RADIO);
				target = SHORT_RADIO;
			}
	}
#else
			if (was_short == 1)	{
 				dual_radio_switch(SHORT_RADIO);
				target = SHORT_RADIO;
			}
#endif
#endif
#endif

  /* restore the packet to send */
  queuebuf_to_packetbuf(packet);
  queuebuf_free(packet);

  /* Send the data packet. */
  if((is_broadcast || got_strobe_ack || is_streaming) && collisions == 0) {

#if RPL_ENERGY_MODE
//	     Relaying Tx energy consumption for Data packet JJH
	  original_datalen = packetbuf_totlen();
	  original_dataptr = packetbuf_hdrptr();
	  if(original_dataptr[original_datalen-1]=='X')
	  {
//	    	 For each data relay, energy reduction 1 for short 2 for long
	  	if(remaining_energy >1){
#if DUAL_RADIO
	    	if(radio_received_is_longrange()==LONG_RADIO)
	    	{
	    		if(remaining_energy-2 < 1)
	    			remaining_energy=1;
	    		else
	    			remaining_energy-=2;
				}	else {
	    		remaining_energy--;
				}
#else
				remaining_energy--;
#endif
			}
			PRINTF("node %d energy %d\n",linkaddr_node_addr.u8[1],remaining_energy);
	   	if(remaining_energy == 1) { // A node dies first 
	   		PRINTF("ENERGY DEPLETION\n");
			}
		}
#endif
    NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
  }

#if WITH_ENCOUNTER_OPTIMIZATION
  if(got_strobe_ack && !is_streaming) {
    register_encounter(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), encounter_time);
  }
#endif /* WITH_ENCOUNTER_OPTIMIZATION */
  watchdog_start();

  PRINTF("cxmac: send (strobes=%u,len=%u,%s), done\n", strobes,
	 packetbuf_totlen(), got_strobe_ack ? "ack" : "no ack");

#if CXMAC_CONF_COMPOWER
  /* Accumulate the power consumption for the packet transmission. */
  compower_accumulate(&current_packet);

  /* Convert the accumulated power consumption for the transmitted
     packet to packet attributes so that the higher levels can keep
     track of the amount of energy spent on transmitting the
     packet. */
  compower_attrconv(&current_packet);

  /* Clear the accumulated power consumption so that it is ready for
     the next packet. */
  compower_clear(&current_packet);
#endif /* CXMAC_CONF_COMPOWER */

  we_are_sending = 0;

  LEDS_OFF(LEDS_BLUE);

  if(collisions == 0) {
    if(!is_broadcast && !got_strobe_ack) {
      return MAC_TX_NOACK;
    } else {
      return MAC_TX_OK;
    }
  } else {
    someone_is_sending++;
    cxmac_collision_count++;
    return MAC_TX_COLLISION;
  }
}
/*---------------------------------------------------------------------------*/
static void
qsend_packet(mac_callback_t sent, void *ptr)
{
  int ret;
  if(someone_is_sending) {
    PRINTF("cxmac: should queue packet, now just dropping %d %d %d %d.\n",
	   waiting_for_packet, someone_is_sending, we_are_sending, radio_is_on);
    RIMESTATS_ADD(sendingdrop);
		cxmac_collision_count ++;
    ret = MAC_TX_COLLISION;
  } else {
    PRINTF("cxmac: send immediately.\n");
    ret = send_packet();
  }

  mac_call_sent_callback(sent, ptr, ret, 1);
}
/*---------------------------------------------------------------------------*/
static void
qsend_list(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list)
{
  if(buf_list != NULL) {
    queuebuf_to_packetbuf(buf_list->buf);
    qsend_packet(sent, ptr);
		/* Switch the radio back to the original one */
  }
}
/*---------------------------------------------------------------------------*/
static void
input_packet(void)
{
  struct cxmac_hdr *hdr;
	// JJH
#if DUAL_RADIO
  int target = SHORT_RADIO;
#endif
#if RPL_ENERGY_MODE
  int original_datalen;
  uint8_t *original_dataptr;
#endif
#if LSA_MAC
	uint8_t for_short;
#endif 

  if(NETSTACK_FRAMER.parse() >= 0) {
    hdr = packetbuf_dataptr();
#if RPL_ENERGY_MODE
    original_datalen = packetbuf_totlen();
    original_dataptr = packetbuf_hdrptr();
#endif
#if STROBE_CNT_MODE
    char dispatch_ext = hdr->dispatch << 6;
//    printf("packet input dispatch %d\n",dispatch_ext);
    if(dispatch_ext != DISPATCH) 
#else
    if(hdr->dispatch != DISPATCH) 
#endif
		{		// The packet is for data
      someone_is_sending = 0;
#if DUAL_RADIO
      if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                                           &linkaddr_node_addr) ||
      	 linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
      			 	 	 	 	 	 	   &linkaddr_null) ||
		linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
		     					     &long_linkaddr_node_addr)) 
#else
      if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                                     &linkaddr_node_addr) ||
	 linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                      &linkaddr_null)) 
	/* This is a regular packet that is destined to us or to the
	   broadcast address. */

	/* We have received the final packet, so we can go back to being
	   asleep. */
#endif
				{

#if DUAL_RADIO
#if LSA_MAC
				/* JOONKI
				 * waiting for incoming short broadcast */
				if (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &linkaddr_null) && 
						radio_received_is_longrange()==LONG_RADIO){
					dual_radio_off(LONG_RADIO);
					dual_radio_on(SHORT_RADIO);
					waiting_for_packet = 1;
					is_short_waiting = 1;
					process_start(&strobe_wait, NULL);
				}	else if(radio_received_is_longrange()==SHORT_RADIO){
					dual_radio_off(BOTH_RADIO);
					waiting_for_packet = 0;
				}
				else{
					dual_radio_off(BOTH_RADIO);
					waiting_for_packet = 0;
				}
#else		/* LSA_MAC */
				dual_radio_off(BOTH_RADIO);
				waiting_for_packet = 0;
#endif /* LSA_MAC */
#else
    	  off();
    	  waiting_for_packet = 0;
#endif

#if CXMAC_CONF_COMPOWER
	/* Accumulate the power consumption for the packet reception. */
	compower_accumulate(&current_packet);
	/* Convert the accumulated power consumption for the received
	   packet to packet attributes so that the higher levels can
	   keep track of the amount of energy spent on receiving the
	   packet. */
	compower_attrconv(&current_packet);

	/* Clear the accumulated power consumption so that it is ready
	   for the next packet. */
	compower_clear(&current_packet);
#endif /* CXMAC_CONF_COMPOWER */


/*
  uint8_t src_addr1=original_dataptr[original_datalen-4];
	uint8_t src_addr2=original_dataptr[original_datalen-3];
	uint8_t src_addr3=original_dataptr[original_datalen-2];
*/
#if RPL_ENERGY_MODE
	if(original_dataptr[original_datalen-1]=='X')
	{
//		 For each data relay, energy reduction 1 for short 2 for long
		if(linkaddr_node_addr.u8[1]!=1 && remaining_energy >1){
#if DUAL_RADIO
			if(radio_received_is_longrange()==LONG_RADIO)
			{
				if(remaining_energy-2 < 1){
					remaining_energy=1;
				}	else	{
					remaining_energy-=2;
				}
			}	else	{
				remaining_energy--;
			}
#else
	remaining_energy--;
#endif
		}
	PRINTF("node %d energy %d\n",linkaddr_node_addr.u8[1],remaining_energy);
		if(remaining_energy == 1) // A node dies
			PRINTF("ENERGY DEPLETION\n");
#if DUAL_RADIO
		PRINTF("DATA from: %d to: %d %c %d\n",
				packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[1],linkaddr_node_addr.u8[1],radio_received_is_longrange()==LONG_RADIO ? 'L' : 'S',remaining_energy);
#else
	PRINTF("DATA from: %d to: %d %d\n",
				packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[1],linkaddr_node_addr.u8[1],remaining_energy);
#endif
	}
#endif /* RPL_ENERGY_MODE */
        PRINTDEBUG("cxmac: data(%u)\n", packetbuf_datalen());
        NETSTACK_MAC.input();
        return;
      } else {
        PRINTDEBUG("cxmac: data not for us\n");
      }

    } else if(hdr->type == TYPE_STROBE) {
      someone_is_sending = 2;
#if DUAL_RADIO
				if (radio_received_is_longrange()==LONG_RADIO){
					dual_radio_switch(LONG_RADIO);
				}	else if (radio_received_is_longrange() == SHORT_RADIO){
					dual_radio_switch(SHORT_RADIO);
				}
#endif
#if DUAL_RADIO
      if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                      &linkaddr_node_addr) ||
    	 linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
    		  		     &long_linkaddr_node_addr)) 
#else
      if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                      &linkaddr_node_addr)) 
#endif
			{	
#if DUAL_RADIO
#if LSA_MAC
				if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),&linkaddr_node_addr) == 1){
					for_short = 1;
				} else if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),&long_linkaddr_node_addr) == 1) {
					for_short = 0;
				}
#endif
#endif
	/* This is a strobe packet for us. */

	/* If the sender address is someone else, we should
	   acknowledge the strobe and wait for the packet. By using
	   the same address as both sender and receiver, we flag the
	   message is a strobe ack. */
	hdr->type = TYPE_STROBE_ACK;
	packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER,
			   packetbuf_addr(PACKETBUF_ADDR_SENDER));
#if DUAL_RADIO
	if(sending_in_LR() == LONG_RADIO){
		target = LONG_RADIO;
		packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &long_linkaddr_node_addr);
	}	else	{
		target = SHORT_RADIO;
		packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
	}
#else
  	packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
#endif
	packetbuf_compact();
	if(NETSTACK_FRAMER.create() >= 0) {
	  /* We turn on the radio in anticipation of the incoming
	     packet. */
	  someone_is_sending = 1;
	  waiting_for_packet = 1;

#if DUAL_RADIO
#if LSA_MAC
		dual_radio_off(BOTH_RADIO);
		if (for_short == 1) {
			target = SHORT_RADIO;
		} else if (for_short == 0) {
			target = LONG_RADIO;
		}
#endif
#endif

#if DUAL_RADIO
	  dual_radio_on(target);
#else
	  on();
#endif
	  NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
	  PRINTDEBUG("cxmac: send strobe ack %u\n", packetbuf_totlen());
	} else {
	  PRINTF("cxmac: failed to send strobe ack\n");
	}
      } else if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                             &linkaddr_null)) {
	/* If the receiver address is null, the strobe is sent to
	   prepare for an incoming broadcast packet. If this is the
	   case, we turn on the radio and wait for the incoming
	   broadcast packet. */
//    	  printf("cxmac strobe_cnt %d\n",hdr->dispatch >> 2);
    	  waiting_for_packet = 1;
#if STROBE_CNT_MODE
    	  uint8_t cnt = hdr->dispatch >> 2;
#if DUAL_RADIO
    	  strobe_target = radio_received_is_longrange();
    	  dual_radio_off(BOTH_RADIO);
    	  /* Wait based on strobe count, to Rx data */
    	  process_start(&strobe_wait, &cnt);
#else
    	  off();
    	  process_start(&strobe_wait, &cnt);
#endif
#endif	/* STROBE_CNT_MODE */


      } else {
        PRINTDEBUG("cxmac: strobe not for us\n");
      }

      /* We are done processing the strobe and we therefore return
	 to the caller. */
      return;
#if CXMAC_CONF_ANNOUNCEMENTS
    } else if(hdr->type == TYPE_ANNOUNCEMENT) {
      packetbuf_hdrreduce(sizeof(struct cxmac_hdr));
      parse_announcements(packetbuf_addr(PACKETBUF_ADDR_SENDER));
#endif /* CXMAC_CONF_ANNOUNCEMENTS */
    } else if(hdr->type == TYPE_STROBE_ACK) {
      PRINTDEBUG("cxmac: stray strobe ack\n");
    } else {
      PRINTF("cxmac: unknown type %u (%u)\n", hdr->type,
             packetbuf_datalen());
    }
  } else {
    PRINTF("cxmac: failed to parse (%u)\n", packetbuf_totlen());
  }
}
/*---------------------------------------------------------------------------*/
#if CXMAC_CONF_ANNOUNCEMENTS
static void
send_announcement(void *ptr)
{
  struct cxmac_hdr *hdr;
  int announcement_len;

  /* Set up the probe header. */
  packetbuf_clear();
  hdr = packetbuf_dataptr();

  announcement_len = format_announcement((char *)hdr +
					 sizeof(struct cxmac_hdr));

  if(announcement_len > 0) {
    packetbuf_set_datalen(sizeof(struct cxmac_hdr) + announcement_len);
    hdr->dispatch = DISPATCH;
    hdr->type = TYPE_ANNOUNCEMENT;

    packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &linkaddr_null);
    packetbuf_set_attr(PACKETBUF_ATTR_RADIO_TXPOWER, announcement_radio_txpower);
    if(NETSTACK_FRAMER.create() >= 0) {
      NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
cycle_announcement(void *ptr)
{
  ctimer_set(&announcement_ctimer, ANNOUNCEMENT_TIME,
	     send_announcement, NULL);
  ctimer_set(&announcement_cycle_ctimer, ANNOUNCEMENT_PERIOD,
	     cycle_announcement, NULL);
  if(is_listening > 0) {
    is_listening--;
    /*    printf("is_listening %d\n", is_listening);*/
  }
}
/*---------------------------------------------------------------------------*/
static void
listen_callback(int periods)
{
  is_listening = periods + 1;
}
#endif /* CXMAC_CONF_ANNOUNCEMENTS */
/*---------------------------------------------------------------------------*/
void
cxmac_set_announcement_radio_txpower(int txpower)
{
#if CXMAC_CONF_ANNOUNCEMENTS
  announcement_radio_txpower = txpower;
#endif /* CXMAC_CONF_ANNOUNCEMENTS */
}
/*---------------------------------------------------------------------------*/
void
cxmac_init(void)
{
#if DUAL_RADIO
  dual_duty_cycle_count = 0;
#if DUAL_ROUTING_CONVERGE
	long_duty_on = 1;
	short_duty_on = 1;
#endif
#if LSA_R
	LSA_converge = 0;
	LSA_SR_preamble = 0;
	LSA_message_input = 0;
	LSA_broadcast_count = 1;
#endif

#endif
  radio_is_on = 0;
  waiting_for_packet = 0;
  PT_INIT(&pt);
  /*  rtimer_set(&rt, RTIMER_NOW() + cxmac_config.off_time, 1,
      (void (*)(struct rtimer *, void *))powercycle, NULL);*/

  cxmac_is_on = 1;

#if WITH_ENCOUNTER_OPTIMIZATION
  list_init(encounter_list);
  memb_init(&encounter_memb);
#endif /* WITH_ENCOUNTER_OPTIMIZATION */

#if CXMAC_CONF_ANNOUNCEMENTS
  announcement_register_listen_callback(listen_callback);
  ctimer_set(&announcement_cycle_ctimer, ANNOUNCEMENT_TIME,
	     cycle_announcement, NULL);
#endif /* CXMAC_CONF_ANNOUNCEMENTS */

  CSCHEDULE_POWERCYCLE(DEFAULT_OFF_TIME);
}
/*---------------------------------------------------------------------------*/
static int
turn_on(void)
{
  cxmac_is_on = 1;
  /*  rtimer_set(&rt, RTIMER_NOW() + cxmac_config.off_time, 1,
      (void (*)(struct rtimer *, void *))powercycle, NULL);*/
  CSCHEDULE_POWERCYCLE(DEFAULT_OFF_TIME);
  return 1;
}
/*---------------------------------------------------------------------------*/
static int
turn_off(int keep_radio_on)
{
  cxmac_is_on = 0;
#if DUAL_RADIO
  if(keep_radio_on) {
    return dual_radio_turn_on(BOTH_RADIO);
  } else {
    return dual_radio_turn_off(BOTH_RADIO);
  }
#else
  if(keep_radio_on) {
    return NETSTACK_RADIO.on();
  } else {
    return NETSTACK_RADIO.off();
  }
#endif
}
/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
  return (1ul * CLOCK_SECOND * DEFAULT_PERIOD) / RTIMER_ARCH_SECOND;
}
/*---------------------------------------------------------------------------*/
const struct rdc_driver cxmac_driver =
  {
    "CX-MAC",
    cxmac_init,
    qsend_packet,
    qsend_list,
    input_packet,
    turn_on,
    turn_off,
    channel_check_interval,
  };
