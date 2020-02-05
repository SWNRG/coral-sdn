/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
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
 *         ICMP6 I/O for RPL control messages.
 *
 * \author Joakim Eriksson <joakime@sics.se>, Nicolas Tsiftes <nvt@sics.se>
 * Contributors: Niclas Finne <nfi@sics.se>, Joel Hoglund <joel@sics.se>,
 *               Mathieu Pouillot <m.pouillot@watteco.com>
 *               George Oikonomou <oikonomou@users.sourceforge.net> (multicast)
 */

/**
 * \addtogroup uip6
 * @{
 */

#include "net/ip/tcpip.h"
// #include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uip-nd6.h"
#include "net/ipv6/uip-icmp6.h"
// #include "net/rpl/rpl-private.h"
#include "net/packetbuf.h"
#include "net/ipv6/multicast/uip-mcast6.h"
#include "random.h"
#include "net/rpl/rpl-icmp6.h"

#include <limits.h>
#include <string.h>

#include "rpl_debug.h"
#define DEBUG DEBUG_RPL_ICMP6

#include "net/ip/uip-debug.h"

#if DUAL_RADIO
#ifdef ZOLERTIA_Z1
#include	"../platform/z1/dual_radio.h"
#elif COOJA /* ZOLERTIA_Z1 */
#include	"../platform/cooja/dual_conf.h"
#else /* ZOLERTIA_Z1 */
#include "../platform/zoul/dual_radio.h"
#endif /* ZOLERTIA_Z1 */
#endif /* DUAL_RADIO */

/* To transfer remaining energy JJH */
#include "../lanada/param.h"
#include "sys/log_message.h"
#include "sys/residual.h"
#include <stdlib.h>
#if RPL_ENERGY_MODE
extern uint8_t remaining_energy;
#endif

/*---------------------------------------------------------------------------*/
#define RPL_DIO_GROUNDED                 0x80
#define RPL_DIO_MOP_SHIFT                3
#define RPL_DIO_MOP_MASK                 0x38
#define RPL_DIO_PREFERENCE_MASK          0x07

#define UIP_IP_BUF       ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_ICMP_BUF     ((struct uip_icmp_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_ICMP_PAYLOAD ((unsigned char *)&uip_buf[uip_l2_l3_icmp_hdr_len])
/*---------------------------------------------------------------------------*/
static void dis_input(void);
static void dio_input(void);
static void dao_input(void);
static void dao_ack_input(void);
#if RPL_LIFETIME_MAX_MODE_DIO_ACK
static void dio_ack_input(void);
#endif
#if LSA_R
static void LSA_converge_input(void);
#endif

static void dao_output_target_seq(rpl_parent_t *parent, uip_ipaddr_t *prefix,
				  uint8_t lifetime, uint8_t seq_no);

/* some debug callbacks useful when debugging RPL networks */
#ifdef RPL_DEBUG_DIO_INPUT
void RPL_DEBUG_DIO_INPUT(uip_ipaddr_t *, rpl_dio_t *);
#endif

#ifdef RPL_DEBUG_DAO_OUTPUT
void RPL_DEBUG_DAO_OUTPUT(rpl_parent_t *);
#endif

static uint8_t dao_sequence = RPL_LOLLIPOP_INIT;

extern rpl_of_t RPL_OF;

#if RPL_CONF_MULTICAST
static uip_mcast6_route_t *mcast_group;
#endif
/*---------------------------------------------------------------------------*/
/* Initialise RPL ICMPv6 message handlers */
UIP_ICMP6_HANDLER(dis_handler, ICMP6_RPL, RPL_CODE_DIS, dis_input);
UIP_ICMP6_HANDLER(dio_handler, ICMP6_RPL, RPL_CODE_DIO, dio_input);
UIP_ICMP6_HANDLER(dao_handler, ICMP6_RPL, RPL_CODE_DAO, dao_input);
UIP_ICMP6_HANDLER(dao_ack_handler, ICMP6_RPL, RPL_CODE_DAO_ACK, dao_ack_input);
#if RPL_LIFETIME_MAX_MODE_DIO_ACK
UIP_ICMP6_HANDLER(dio_ack_handler, ICMP6_RPL, RPL_CODE_DIO_ACK, dio_ack_input);
#endif
#if LSA_R
UIP_ICMP6_HANDLER(LSA_converge_handler, ICMP6_RPL, RPL_CODE_LSA, LSA_converge_input);
#endif
/*---------------------------------------------------------------------------*/

#if RPL_WITH_DAO_ACK
static uip_ds6_route_t *
find_route_entry_by_dao_ack(uint8_t seq)
{
  uip_ds6_route_t *re;
  re = uip_ds6_route_head();
  while(re != NULL) {
    if(re->state.dao_seqno_out == seq && RPL_ROUTE_IS_DAO_PENDING(re)) {
      /* found it! */
      return re;
    }
    re = uip_ds6_route_next(re);
  }
  return NULL;
}
#endif /* RPL_WITH_DAO_ACK */

/* JOONKI
 * maybe wrong place?? */
#if RPL_LIFETIME_MAX_MODE
static int
add_nbr_from_dio(uip_ipaddr_t *from, rpl_dio_t *dio)
{
  /* add this to the neighbor cache if not already there */
  if(rpl_icmp6_update_nbr_table(from, NBR_TABLE_REASON_RPL_DIO, dio) == NULL) {
    PRINTF("RPL: Out of memory, dropping DIO from ");
    PRINT6ADDR(from);
    PRINTF("\n");
    return 0;
  }
  return 1;
}
#endif
#if RPL_LIFETIME_MAX_MODE_DIO_ACK
static int
add_nbr_from_dio_ack(uip_ipaddr_t *from, uint8_t weight)
{
  /* add this to the neighbor cache if not already there */
  if(rpl_icmp6_update_nbr_table(from, NBR_TABLE_REASON_UNDEFINED, &weight) == NULL) {
    PRINTF("RPL: Out of memory, dropping DIO_ACK from ");
    PRINT6ADDR(from);
    PRINTF("\n");
    return 0;
  }
  return 1;
}
#endif
#if RPL_LIFETIME_MAX_MODE
static int
add_nbr_from_dao(uip_ipaddr_t *from, uint8_t weight)
{
  /* add this to the neighbor cache if not already there */
  if(rpl_icmp6_update_nbr_table(from, NBR_TABLE_REASON_RPL_DAO, &weight) == NULL) {
    PRINTF("RPL: Out of memory, dropping DAO from ");
    PRINT6ADDR(from);
    PRINTF("\n");
    return 0;
  }
  return 1;
}
#endif

/* prepare for forwarding of DAO */
static uint8_t
prepare_for_dao_fwd(uint8_t sequence, uip_ds6_route_t *rep)
{
  /* not pending - or pending but not a retransmission */
  RPL_LOLLIPOP_INCREMENT(dao_sequence);

  /* set DAO pending and sequence numbers */
  rep->state.dao_seqno_in = sequence;
  rep->state.dao_seqno_out = dao_sequence;
  RPL_ROUTE_SET_DAO_PENDING(rep);
  return dao_sequence;
}
/*---------------------------------------------------------------------------*/
static int
get_global_addr(uip_ipaddr_t *addr)
{
  int i;
  int state;
	/* JOONKI 
	 * In this part we don't have to mension long range interface */

  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      if(!uip_is_addr_linklocal(&uip_ds6_if.addr_list[i].ipaddr)) {
        memcpy(addr, &uip_ds6_if.addr_list[i].ipaddr, sizeof(uip_ipaddr_t));
        return 1;
      }
    }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static uint32_t
get32(uint8_t *buffer, int pos)
{
  return (uint32_t)buffer[pos] << 24 | (uint32_t)buffer[pos + 1] << 16 |
         (uint32_t)buffer[pos + 2] << 8 | buffer[pos + 3];
}
/*---------------------------------------------------------------------------*/
static void
set32(uint8_t *buffer, int pos, uint32_t value)
{
  buffer[pos++] = value >> 24;
  buffer[pos++] = (value >> 16) & 0xff;
  buffer[pos++] = (value >> 8) & 0xff;
  buffer[pos++] = value & 0xff;
}
/*---------------------------------------------------------------------------*/
static uint16_t
get16(uint8_t *buffer, int pos)
{
  return (uint16_t)buffer[pos] << 8 | buffer[pos + 1];
}
/*---------------------------------------------------------------------------*/
static void
set16(uint8_t *buffer, int pos, uint16_t value)
{
  buffer[pos++] = value >> 8;
  buffer[pos++] = value & 0xff;
}
/*---------------------------------------------------------------------------*/
uip_ds6_nbr_t *
rpl_icmp6_update_nbr_table(uip_ipaddr_t *from, nbr_table_reason_t reason, void *data)
{
  uip_ds6_nbr_t *nbr;
	// linkaddr_t temp_lladdr;

  if((nbr = uip_ds6_nbr_lookup(from)) == NULL) {
		/* JOONKI */
		// temp_lladdr = *packetbuf_addr(PACKETBUF_ADDR_SENDER);

    if((nbr = uip_ds6_nbr_add(from, (uip_lladdr_t *)
															packetbuf_addr(PACKETBUF_ADDR_SENDER),
                              //& temp_lladdr,
                              0, NBR_REACHABLE, reason, data)) != NULL) {
      PRINTF("RPL: Neighbor added to neighbor cache ");
      PRINT6ADDR(from);
      PRINTF(", ");
      PRINTLLADDR((uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_SENDER));
      // PRINTLLADDR((uip_lladdr_t *)&temp_lladdr);
      PRINTF("\n");

    }
  }

  if(nbr != NULL) {
#if UIP_ND6_SEND_NA
    /* set reachable timer if we added or found the nbr entry - and update
       neighbor entry to reachable to avoid sending NS/NA, etc.  */
    stimer_set(&nbr->reachable, UIP_ND6_REACHABLE_TIME / 1000);
    nbr->state = NBR_REACHABLE;
#endif /* UIP_ND6_SEND_NA */
  }
  return nbr;
 }
/*---------------------------------------------------------------------------*/
static void
dis_input(void)
{
  rpl_instance_t *instance;
  rpl_instance_t *end;
	
/* JOONKI
 * Need to add dual rpl sequence when we get unicast DIS message
 * Unicast DIS is not used in this version
 */

  /* DAG Information Solicitation */
  PRINTF("RPL: Received a DIS from ");
  PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
  PRINTF("\n");

  for(instance = &instance_table[0], end = instance + RPL_MAX_INSTANCES;
      instance < end; ++instance) {
    if(instance->used == 1) {
			/* Usually we don't use LEAF_ONLY */
#if RPL_LEAF_ONLY
      if(!uip_is_addr_mcast(&UIP_IP_BUF->destipaddr)) {
	PRINTF("RPL: LEAF ONLY Multicast DIS will NOT reset DIO timer\n");
#else /* !RPL_LEAF_ONLY */
      if(uip_is_addr_mcast(&UIP_IP_BUF->destipaddr)) {
        PRINTF("RPL: Multicast DIS => reset DIO timer\n");
        rpl_reset_dio_timer(instance);
      } else {
#endif /* !RPL_LEAF_ONLY */
	/* Check if this neighbor should be added according to the policy. */
        if(rpl_icmp6_update_nbr_table(&UIP_IP_BUF->srcipaddr,
                                      NBR_TABLE_REASON_RPL_DIS, NULL) == NULL) {
          PRINTF("RPL: Out of Memory, not sending unicast DIO, DIS from ");
          PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
          PRINTF(", ");
          PRINTLLADDR((uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_SENDER));
          PRINTF("\n");
        } else {
          PRINTF("RPL: Unicast DIS, reply to sender\n");
          dio_output(instance, &UIP_IP_BUF->srcipaddr);
        }
	/* } */
      }
    }
  }
  uip_clear_buf();
}
/*---------------------------------------------------------------------------*/
void
dis_output(uip_ipaddr_t *addr)
{
  unsigned char *buffer;
  uip_ipaddr_t tmpaddr;

  /*
   * DAG Information Solicitation  - 2 bytes reserved
   *      0                   1                   2
   *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
   *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *     |     Flags     |   Reserved    |   Option(s)...
   *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   */
	dis_count ++;
#if RPL_ICMP_ENERGY_LOG
	char *log_buf = (char*) malloc(sizeof(char)*100);
	sprintf(log_buf,"DIS_OUTPUT, Energy: %d\n",(int)get_residual_energy()); 
	LOG_MESSAGE(log_buf); 
	free(log_buf);
#endif

  buffer = UIP_ICMP_PAYLOAD;
  buffer[0] = buffer[1] = 0;

/* JOONKI
 * It's better to add some dual rpl unicast for later use
 * Unicast DIS is not used in this version
 */

  if(addr == NULL) {
    uip_create_linklocal_rplnodes_mcast(&tmpaddr);
    addr = &tmpaddr;
  }

  PRINTF("RPL: Sending a DIS to ");
  PRINT6ADDR(addr);
  PRINTF("\n");

  uip_icmp6_send(addr, ICMP6_RPL, RPL_CODE_DIS, 2);
}
/*---------------------------------------------------------------------------*/
static void
dio_input(void)
{
  unsigned char *buffer;
  uint8_t buffer_length;
  rpl_dio_t dio;
  uint8_t subopt_type;
  int i;
  int len;
  uip_ipaddr_t from;


  memset(&dio, 0, sizeof(dio));

  /* Set default values in case the DIO configuration option is missing. */
  dio.dag_intdoubl = RPL_DIO_INTERVAL_DOUBLINGS;
  dio.dag_intmin = RPL_DIO_INTERVAL_MIN;
  dio.dag_redund = RPL_DIO_REDUNDANCY;
  dio.dag_min_hoprankinc = RPL_MIN_HOPRANKINC;
  dio.dag_max_rankinc = RPL_MAX_RANKINC;
  dio.ocp = RPL_OF.ocp;
  dio.default_lifetime = RPL_DEFAULT_LIFETIME;
  dio.lifetime_unit = RPL_DEFAULT_LIFETIME_UNIT;

  uip_ipaddr_copy(&from, &UIP_IP_BUF->srcipaddr);

  /* DAG Information Object */
  PRINTF("RPL: Received a DIO from ");
  PRINT6ADDR(&from);
  PRINTF("\n");

  buffer_length = uip_len - uip_l3_icmp_hdr_len;

  /* Process the DIO base option. */
  i = 0;
  buffer = UIP_ICMP_PAYLOAD;

  dio.instance_id = buffer[i++];
  dio.version = buffer[i++];
  dio.rank = get16(buffer, i);
  i += 2;

  PRINTF("RPL: Incoming DIO (id, ver, rank) = (%u,%u,%u)\n",
         (unsigned)dio.instance_id,
         (unsigned)dio.version,
         (unsigned)dio.rank);

  dio.grounded = buffer[i] & RPL_DIO_GROUNDED;
  dio.mop = (buffer[i]& RPL_DIO_MOP_MASK) >> RPL_DIO_MOP_SHIFT;
  dio.preference = buffer[i++] & RPL_DIO_PREFERENCE_MASK;

  dio.dtsn = buffer[i++];
  /* two reserved bytes */
  /* One byte for remaining energy JJH*/
#if RPL_ENERGY_MODE
  dio.rem_energy = buffer[i++];
  i += 1;
#elif RPL_LIFETIME_MAX_MODE
  dio.parent_weight = buffer[i++];
  PRINTF("recv dio p's weight %d\n",dio.parent_weight);
  dio.dio_weight = buffer[i++];
  PRINTF("recv dio s' weight %d\n",dio.dio_weight);
#else
  i += 2;
#endif

  memcpy(&dio.dag_id, buffer + i, sizeof(dio.dag_id));
  i += sizeof(dio.dag_id);

  PRINTF("RPL: Incoming DIO (dag_id, pref) = (");
  PRINT6ADDR(&dio.dag_id);
  PRINTF(", %u)\n", dio.preference);

  /* Check if there are any DIO suboptions. */
  for(; i < buffer_length; i += len) {
    subopt_type = buffer[i];
    if(subopt_type == RPL_OPTION_PAD1) {
      len = 1;
    } else {
      /* Suboption with a two-byte header + payload */
      len = 2 + buffer[i + 1];
    }

    if(len + i > buffer_length) {
      PRINTF("RPL: Invalid DIO packet\n");
      RPL_STAT(rpl_stats.malformed_msgs++);
      goto discard;
    }

    PRINTF("RPL: DIO option %u, length: %u\n", subopt_type, len - 2);

    switch(subopt_type) {
    case RPL_OPTION_DAG_METRIC_CONTAINER:
      if(len < 6) {
        PRINTF("RPL: Invalid DAG MC, len = %d\n", len);
	RPL_STAT(rpl_stats.malformed_msgs++);
        goto discard;
      }
      dio.mc.type = buffer[i + 2];
      dio.mc.flags = buffer[i + 3] << 1;
      dio.mc.flags |= buffer[i + 4] >> 7;
      dio.mc.aggr = (buffer[i + 4] >> 4) & 0x3;
      dio.mc.prec = buffer[i + 4] & 0xf;
      dio.mc.length = buffer[i + 5];

      if(dio.mc.type == RPL_DAG_MC_NONE) {
        /* No metric container: do nothing */
      } else if(dio.mc.type == RPL_DAG_MC_ETX) {
        dio.mc.obj.etx = get16(buffer, i + 6);

        PRINTF("RPL: DAG MC: type %u, flags %u, aggr %u, prec %u, length %u, ETX %u\n",
	       (unsigned)dio.mc.type,
	       (unsigned)dio.mc.flags,
	       (unsigned)dio.mc.aggr,
	       (unsigned)dio.mc.prec,
	       (unsigned)dio.mc.length,
	       (unsigned)dio.mc.obj.etx);
      } else if(dio.mc.type == RPL_DAG_MC_ENERGY) {
        dio.mc.obj.energy.flags = buffer[i + 6];
        dio.mc.obj.energy.energy_est = buffer[i + 7];
      } else {
       PRINTF("RPL: Unhandled DAG MC type: %u\n", (unsigned)dio.mc.type);
       goto discard;
      }
      break;
    case RPL_OPTION_ROUTE_INFO:
      if(len < 9) {
        PRINTF("RPL: Invalid destination prefix option, len = %d\n", len);
	RPL_STAT(rpl_stats.malformed_msgs++);
        goto discard;
      }

      /* The flags field includes the preference value. */
      dio.destination_prefix.length = buffer[i + 2];
      dio.destination_prefix.flags = buffer[i + 3];
      dio.destination_prefix.lifetime = get32(buffer, i + 4);

      if(((dio.destination_prefix.length + 7) / 8) + 8 <= len &&
         dio.destination_prefix.length <= 128) {
        PRINTF("RPL: Copying destination prefix\n");
        memcpy(&dio.destination_prefix.prefix, &buffer[i + 8],
               (dio.destination_prefix.length + 7) / 8);
      } else {
        PRINTF("RPL: Invalid route info option, len = %d\n", len);
	RPL_STAT(rpl_stats.malformed_msgs++);
        goto discard;
      }

      break;
    case RPL_OPTION_DAG_CONF:
      if(len != 16) {
        PRINTF("RPL: Invalid DAG configuration option, len = %d\n", len);
	RPL_STAT(rpl_stats.malformed_msgs++);
        goto discard;
      }

      /* Path control field not yet implemented - at i + 2 */
      dio.dag_intdoubl = buffer[i + 3];
      dio.dag_intmin = buffer[i + 4];
      dio.dag_redund = buffer[i + 5];
      dio.dag_max_rankinc = get16(buffer, i + 6);
      dio.dag_min_hoprankinc = get16(buffer, i + 8);
      dio.ocp = get16(buffer, i + 10);
      /* buffer + 12 is reserved */
      dio.default_lifetime = buffer[i + 13];
      dio.lifetime_unit = get16(buffer, i + 14);
      PRINTF("RPL: DAG conf:dbl=%d, min=%d red=%d maxinc=%d mininc=%d ocp=%d d_l=%u l_u=%u\n",
             dio.dag_intdoubl, dio.dag_intmin, dio.dag_redund,
             dio.dag_max_rankinc, dio.dag_min_hoprankinc, dio.ocp,
             dio.default_lifetime, dio.lifetime_unit);
      break;
    case RPL_OPTION_PREFIX_INFO:
#if RPL_LIFETIME_MAX_MODE
    	if(len != 48) {
#else
    	if(len != 32) {
#endif
        PRINTF("RPL: Invalid DAG prefix info, len != 48 or 32\n");
	RPL_STAT(rpl_stats.malformed_msgs++);
        goto discard;
      }
      dio.prefix_info.length = buffer[i + 2];
      dio.prefix_info.flags = buffer[i + 3];
      /* valid lifetime is ingnored for now - at i + 4 */
      /* preferred lifetime stored in lifetime */
      dio.prefix_info.lifetime = get32(buffer, i + 8);
      /* 32-bit reserved at i + 12 */
      PRINTF("RPL: Copying prefix information\n");
      memcpy(&dio.prefix_info.prefix, &buffer[i + 16], 16);
#if RPL_LIFETIME_MAX_MODE
      memcpy(&dio.parent_addr,&buffer[i + 32],16);
      PRINTF("recv dio addr: ");
      PRINT6ADDR(&dio.parent_addr);
      PRINTF("\n");
#endif
      break;
    default:
      PRINTF("RPL: Unsupported suboption type in DIO: %u\n",
	(unsigned)subopt_type);
    }
  }

#ifdef RPL_DEBUG_DIO_INPUT
  RPL_DEBUG_DIO_INPUT(&from, &dio);
#endif

  rpl_process_dio(&from, &dio);
/*  PRINTF("before add_nbr\n");
  if(!add_nbr_from_dio(&from, &dio)) {
    PRINTF("RPL: Could not add parent based on DIO in icmp6\n");
    return;
  }
  PRINTF("after add_nbr\n");*/
#if RPL_LIFETIME_MAX_MODE
  // Check 1. sender is my child or not, 2. dio-> parent is me or not
  rpl_child_t *c;
#if DUAL_RADIO
  uint8_t is_longrange = radio_received_is_longrange();
  PRINTF("before child cmp %d\n",is_longrange);
#endif
#if MODE_DIO_WEIGHT_UPDATED
  uint8_t prev_weight = my_weight;
#endif
  uint8_t prev_weight = my_weight;

  PRINTF("DIO_from:");
  PRINT6ADDR(&from);
  c = rpl_find_child(&from);
  PRINTF(" c: %d\n",c == NULL);
  if(c != NULL)
  {
	  PRINTF("after child cmp\n");
	  PRINT6ADDR(&dio.parent_addr);
	  PRINTF("\n");
#if DUAL_RADIO
	  if((is_longrange == LONG_RADIO
			  && uip_ipaddr_cmp(&dio.parent_addr, &uip_ds6_long_get_link_local(-1)->ipaddr))
			  || (is_longrange == SHORT_RADIO
					  && uip_ipaddr_cmp(&dio.parent_addr, &uip_ds6_get_link_local(-1)->ipaddr)))
//	  if(uip_ipaddr_cmp(&dio.parent_addr, &uip_ds6_get_link_local(-1)->ipaddr)
//		   || uip_ipaddr_cmp(&dio.parent_addr, &uip_ds6_long_get_link_local(-1)->ipaddr))
#else
		  if(uip_ipaddr_cmp(&dio.parent_addr, &uip_ds6_get_link_local(-1)->ipaddr))
#endif
	    {
		  PRINTF("it's me\n");
		  // weight update
		  if(c->weight != dio.parent_weight)
		  {
			  my_weight -= c->weight;
			  c->weight = dio.parent_weight;
			  my_weight += c->weight;
			  PRINTF("my_weight update in dio %d\n",my_weight);
		  }
	    }
	  else
	    {
	      my_weight -= c->weight;
	      rpl_remove_child(c);
	      PRINTF("my_weight deduct in dio %d\n",my_weight);
		  PRINTF("child list in dio input\n");
		  rpl_print_child_neighbor_list();
	      // In my child list but I'm not the parent any more, so remove child
	    }
  }
  else
  {
#if DUAL_RADIO
	  if((is_longrange == LONG_RADIO
			  && uip_ipaddr_cmp(&dio.parent_addr, &uip_ds6_long_get_link_local(-1)->ipaddr))
			  || (is_longrange == SHORT_RADIO
					  && uip_ipaddr_cmp(&dio.parent_addr, &uip_ds6_get_link_local(-1)->ipaddr)))
#else
		  if(uip_ipaddr_cmp(&dio.parent_addr, &uip_ds6_get_link_local(-1)->ipaddr))
#endif
	  {

		  c = rpl_add_child(dio.parent_weight, &from);
		  if(c == NULL)
		  {
			  PRINTF("fail to add child\n");
		  }
		  else
		  {
			  my_weight += c->weight;
			  PRINTF("my_weight add child in dio %d\n",my_weight);
		  }
		  PRINTF("child list in dio input\n");
		  rpl_print_child_neighbor_list();
		  // Not included in child list but I'm the parent maybe due to the DIO_ACK loss
	  }
  }
#if MODE_DIO_WEIGHT_UPDATED
  // If my_weight changed,
  if(prev_weight != my_weight)
  {
	  dio_broadcast(rpl_get_default_instance());
  }
#endif
  if(prev_weight != my_weight)
  {
	  PRINTF("DIO Reset in DIO\n");
	  rpl_reset_dio_timer(rpl_get_default_instance());
  }
  PRINTF("DIO INPUT my_weight %d\n",my_weight);
#endif

 discard:
  uip_clear_buf();
}
/*---------------------------------------------------------------------------*/
void
dio_output(rpl_instance_t *instance, uip_ipaddr_t *uc_addr)
{
  unsigned char *buffer;
  int pos;
  rpl_dag_t *dag = instance->current_dag;
#if !RPL_LEAF_ONLY
  uip_ipaddr_t addr;
#endif /* !RPL_LEAF_ONLY */

	dio_count ++;
#if RPL_ICMP_ENERGY_LOG
	char *log_buf = (char*) malloc(sizeof(char)*100);
	sprintf(log_buf,"DIO_OUTPUT, Energy: %d\n",(int) get_residual_energy()); 
	LOG_MESSAGE(log_buf); 
	free(log_buf);
#endif

#if RPL_LEAF_ONLY
  /* In leaf mode, we only send DIO messages as unicasts in response to
     unicast DIS messages. */
  if(uc_addr == NULL) {
    PRINTF("RPL: LEAF ONLY have multicast addr: skip dio_output\n");
    return;
  }
#endif /* RPL_LEAF_ONLY */

  /* DAG Information Object */
  pos = 0;

  buffer = UIP_ICMP_PAYLOAD;
  buffer[pos++] = instance->instance_id;
  buffer[pos++] = dag->version;

#if RPL_LEAF_ONLY
  PRINTF("RPL: LEAF ONLY DIO rank set to INFINITE_RANK\n");
  set16(buffer, pos, INFINITE_RANK);
#else /* RPL_LEAF_ONLY */
  set16(buffer, pos, dag->rank);
#endif /* RPL_LEAF_ONLY */
  pos += 2;

  buffer[pos] = 0;
  if(dag->grounded) {
    buffer[pos] |= RPL_DIO_GROUNDED;
  }

  buffer[pos] |= instance->mop << RPL_DIO_MOP_SHIFT;
  buffer[pos] |= dag->preference & RPL_DIO_PREFERENCE_MASK;
  pos++;

  buffer[pos++] = instance->dtsn_out;

  if(RPL_DIO_REFRESH_DAO_ROUTES && uc_addr == NULL) {
    /* Request new DAO to refresh route. We do not do this for unicast DIO
     * in order to avoid DAO messages after a DIS-DIO update,
     * or upon unicast DIO probing. */
    RPL_LOLLIPOP_INCREMENT(instance->dtsn_out);
  }

#if RPL_ENERGY_MODE
  buffer[pos++] = remaining_energy; /* remaining energy JJH */
  buffer[pos++] = 0; /* reserved */
#elif RPL_LIFETIME_MAX_MODE
  if(dag->preferred_parent != NULL)
  {
	  if(dag->preferred_parent->parent_weight == 0)
	  {
#if DUAL_RADIO
		  PRINTF("send dio weight %d default\n",sending_in_LR()==LONG_RADIO ? LONG_WEIGHT_RATIO : 1);
		  buffer[pos++] = sending_in_LR()==LONG_RADIO ? LONG_WEIGHT_RATIO : 1; /* parent's weight default */
#else	/* DUAL_RADIO */
			PRINTF("send dio weight %d default\n",1);
		  buffer[pos++] = 1; /* parent's weight default */
#endif	/* DUAL_RADIO */
	  }
	  else
	  {
		  PRINTF("send dio weight %d\n",dag->preferred_parent->parent_weight);
		  buffer[pos++] = dag->preferred_parent->parent_weight; /* parent's weight  */
	  }
  }
  else
  {
	  buffer[pos++] = 0;
  }
//  buffer[pos++] = 0; /* reserved */
#if SINK_INFINITE_ENERGY
  /* For sink node, set weight 0 */
  if(uip_ds6_get_link_local(-1)->ipaddr.u8[15]==1)
  {
	  PRINTF("send my weight sink %d but 0\n",my_weight);
	  buffer[pos++] = 0;
  }
  else
  {
	  PRINTF("send my weight %d\n",my_weight);
	  buffer[pos++] = my_weight;
  }
#else
  buffer[pos++] = my_weight;
#endif
#else
  /* reserved 2 bytes */
  buffer[pos++] = 0; /* flags */
  buffer[pos++] = 0; /* reserved */
#endif

  memcpy(buffer + pos, &dag->dag_id, sizeof(dag->dag_id));
  pos += 16;

#if !RPL_LEAF_ONLY
  if(instance->mc.type != RPL_DAG_MC_NONE) {
    instance->of->update_metric_container(instance);

    buffer[pos++] = RPL_OPTION_DAG_METRIC_CONTAINER;
    buffer[pos++] = 6;
    buffer[pos++] = instance->mc.type;
    buffer[pos++] = instance->mc.flags >> 1;
    buffer[pos] = (instance->mc.flags & 1) << 7;
    buffer[pos++] |= (instance->mc.aggr << 4) | instance->mc.prec;
    if(instance->mc.type == RPL_DAG_MC_ETX) {
      buffer[pos++] = 2;
      set16(buffer, pos, instance->mc.obj.etx);
      pos += 2;
    } else if(instance->mc.type == RPL_DAG_MC_ENERGY) {
      buffer[pos++] = 2;
      buffer[pos++] = instance->mc.obj.energy.flags;
      buffer[pos++] = instance->mc.obj.energy.energy_est;
    } else {
      PRINTF("RPL: Unable to send DIO because of unhandled DAG MC type %u\n",
	(unsigned)instance->mc.type);
      return;
    }
  }
#endif /* !RPL_LEAF_ONLY */

  /* Always add a DAG configuration option. */
  buffer[pos++] = RPL_OPTION_DAG_CONF;
  buffer[pos++] = 14;
  buffer[pos++] = 0; /* No Auth, PCS = 0 */
  buffer[pos++] = instance->dio_intdoubl;
  buffer[pos++] = instance->dio_intmin;
  buffer[pos++] = instance->dio_redundancy;
  set16(buffer, pos, instance->max_rankinc);
  pos += 2;
  set16(buffer, pos, instance->min_hoprankinc);
  pos += 2;
  /* OCP is in the DAG_CONF option */
  set16(buffer, pos, instance->of->ocp);
  pos += 2;
  buffer[pos++] = 0; /* reserved */
  buffer[pos++] = instance->default_lifetime;
  set16(buffer, pos, instance->lifetime_unit);
  pos += 2;

  /* Check if we have a prefix to send also. */
  if(dag->prefix_info.length > 0) {
    buffer[pos++] = RPL_OPTION_PREFIX_INFO;
#if RPL_LIFETIME_MAX_MODE
    buffer[pos++] = 46; /* always 30 bytes + 2 long + 16 bytes addr JJH */
#else
    buffer[pos++] = 30; /* always 30 bytes + 2 long */
#endif
    buffer[pos++] = dag->prefix_info.length;
    buffer[pos++] = dag->prefix_info.flags;
    set32(buffer, pos, dag->prefix_info.lifetime);
    pos += 4;
    set32(buffer, pos, dag->prefix_info.lifetime);
    pos += 4;
    memset(&buffer[pos], 0, 4);
    pos += 4;
    memcpy(&buffer[pos], &dag->prefix_info.prefix, 16);
    pos += 16;
    PRINTF("RPL: Sending prefix info in DIO for ");
    	PRINT6ADDR(&dag->prefix_info.prefix);
    PRINTF("\n");
  } else {
    PRINTF("RPL: No prefix to announce (len %d)\n",
           dag->prefix_info.length);
  }
#if RPL_LIFETIME_MAX_MODE
    if(dag->preferred_parent != NULL)
    {
    	PRINTF("send dio addr: ");
    	PRINT6ADDR(rpl_get_parent_ipaddr(dag->preferred_parent));
    	PRINTF("\n");
    	memcpy(&buffer[pos],rpl_get_parent_ipaddr(dag->preferred_parent),16);
    	pos += 16;
    }
    else
    {
    	memset(&buffer[pos],0,16);
    	pos += 16;
    }
#endif
#if RPL_LEAF_ONLY
#if (DEBUG) & DEBUG_PRINT
  if(uc_addr == NULL) {
    PRINTF("RPL: LEAF ONLY sending unicast-DIO from multicast-DIO\n");
  }
#endif /* DEBUG_PRINT */
  PRINTF("RPL: Sending unicast-DIO with rank %u to ",
      (unsigned)dag->rank);
  PRINT6ADDR(uc_addr);
  PRINTF("\n");
  uip_icmp6_send(uc_addr, ICMP6_RPL, RPL_CODE_DIO, pos);
#else /* RPL_LEAF_ONLY */
  /* Unicast requests get unicast replies! */
  if(uc_addr == NULL) {
    PRINTF("RPL: Sending a multicast-DIO with rank %u\n",
        (unsigned)instance->current_dag->rank);
    uip_create_linklocal_rplnodes_mcast(&addr);
    uip_icmp6_send(&addr, ICMP6_RPL, RPL_CODE_DIO, pos);
  } else {
/*JOONKI*/
#if DUAL_RADIO
#if ADDR_MAP
		int i;
		uip_ds6_nbr_t *nbr = NULL;
		nbr = uip_ds6_nbr_lookup(uc_addr);	
		for (i=0; i<NBR_TABLE_MAX_NEIGHBORS; i++){
			/* PRINTLLADDR(uip_ds6_nbr_get_ll(nbr));
			PRINTF("\n");
			PRINTLLADDR(&ds6_lr_addrmap[i].lladdr);
			PRINTF("\n"); */
			if (linkaddr_cmp(&ds6_lr_addrmap[i].lladdr, (const linkaddr_t *)uip_ds6_nbr_get_ll(nbr))){
				if (ds6_lr_addrmap[i].lr == 1){
						dual_radio_switch(LONG_RADIO);
					}	else	{
						dual_radio_switch(SHORT_RADIO);
					}
				// PRINTF("SUCCESS!!!\n");
				break;
			}
		}
#else /* ADDR_MAP */

		if (uc_addr->u8[8] == 0x82)	{
			dual_radio_switch(LONG_RADIO);
		}	else	{
			dual_radio_switch(SHORT_RADIO);
		}
#endif	/* ADDR_MAP */
#endif /* DUAL_RADIO */


    PRINTF("aRPL: Sending unicast-DIO with rank %u to ",
        (unsigned)instance->current_dag->rank);
		
    PRINT6ADDR(uc_addr);
    PRINTF("\n");
    uip_icmp6_send(uc_addr, ICMP6_RPL, RPL_CODE_DIO, pos);
  }
#endif /* RPL_LEAF_ONLY */
}
/*---------------------------------------------------------------------------*/
#if RPL_LIFETIME_MAX_MODE_DIO_ACK
void
dio_ack_input(void)
{
	PRINTF("dio_ack_input!\n");
	unsigned char *buffer;
	uip_ipaddr_t from, parent;
	rpl_child_t *c = NULL;
	uint8_t weight, pos, buffer_length;
#if MODE_LAST_PARENT
	uip_ipaddr_t last_parent;
	uint8_t last_weight;
#endif

	if(rpl_get_default_instance() == NULL)
	{
		PRINTF("rpl-icmp6 before joining instance\n");
		uip_clear_buf();
		return;
	}

	pos = 0;

	buffer = UIP_ICMP_PAYLOAD;
	uip_ipaddr_copy(&from, &UIP_IP_BUF->srcipaddr);
	buffer_length = uip_len - uip_l3_icmp_hdr_len;
	PRINTF("bufferlen %d\n",buffer_length);
	if(buffer_length == 1) // NULL parent
	{
		PRINTF("dio_ack NULL parent\n");
		weight = buffer[pos++];
	}
	else if(buffer_length == 17) // Only preferred parent
	{
		memcpy(&parent,&buffer[pos],16);
		pos += 16;
		PRINTF("recv dio_ack addr: ");
		PRINT6ADDR(&parent);
    	PRINTF("\n");
		weight = buffer[pos++];
	}
#if MODE_LAST_PARENT
	else if(buffer_length == 34) //Last parent case
	{
		memcpy(&last_parent,&buffer[pos],16);
		pos += 16;
		PRINTF("recv dio_ack lp addr: ");
		PRINT6ADDR(&last_parent);
    	PRINTF("\n");
		last_weight = buffer[pos++];
	}
#endif

	if(!add_nbr_from_dio_ack(&from, weight)) // Should I add nbr for all dio_ack sender? JJH
	{
		PRINTF("fail to add nbr with dio_ack\n");
	}
	c = rpl_find_child(&from);
#if DUAL_RADIO
	uint8_t is_longrange = radio_received_is_longrange();
	if((is_longrange == LONG_RADIO
			&& uip_ipaddr_cmp(&parent, &uip_ds6_long_get_link_local(-1)->ipaddr))
			|| (is_longrange == SHORT_RADIO
					&& uip_ipaddr_cmp(&parent, &uip_ds6_get_link_local(-1)->ipaddr)))
#else
		if(uip_ipaddr_cmp(&parent, &uip_ds6_get_link_local(-1)->ipaddr))
#endif
	{
		if (c == NULL)
		{
			c = rpl_add_child(weight,&from);
			if(c == NULL)
			{
				PRINTF("fail to add child\n");
			}
			else
			{
				my_weight += c->weight;
				PRINTF("my_weight in dio_ack %d\n",my_weight);
			}
			PRINTF("child list in dio_ack input\n");
			rpl_print_child_neighbor_list();
		}
		else
		{
			if(c->weight != weight)
			{
				my_weight -= c->weight;
				c->weight = weight;
				my_weight += c->weight;
				PRINTF("my_weight update in dio_ack %d\n",my_weight);
			}
		}
	}
	// If I'm a last parent
#if MODE_LAST_PARENT
#if DUAL_RADIO
	else if((is_longrange == LONG_RADIO
			&& uip_ipaddr_cmp(&last_parent, &uip_ds6_long_get_link_local(-1)->ipaddr))
			|| (is_longrange == SHORT_RADIO
					&& uip_ipaddr_cmp(&last_parent, &uip_ds6_get_link_local(-1)->ipaddr)))
#else
		else if(uip_ipaddr_cmp(&last_parent, &uip_ds6_get_link_local(-1)->ipaddr))
#endif
	{
		if (c != NULL)
		{
			my_weight -= c->weight;
			PRINTF("my_weight update deduct in dio_ack %d\n",my_weight);
			rpl_remove_child(c);
		}
		rpl_parent_t *p = NULL;

		p = rpl_find_parent(rpl_get_default_instance()->current_dag,&parent);
		if(p != NULL )
		{
			p->parent_sum_weight += weight;
		}
	}
#endif
	// Just third party
	else
	{
		if (c != NULL)
		{
			my_weight -= c->weight;
			PRINTF("my_weight update deduct in dio_ack %d\n",my_weight);
			rpl_remove_child(c);
		}
		rpl_parent_t *p = NULL, *lp = NULL;
		if(rpl_get_default_instance() == NULL)
		{
			PRINTF("rpl-icmp6 before joining instance\n");
			return;
		}
//		p = rpl_find_parent(rpl_get_default_instance()->current_dag,&parent);
		if(p != NULL )
		{
			p->parent_sum_weight += weight;
		}
#if MODE_LAST_PARENT
//		lp = rpl_find_parent(rpl_get_default_instance()->current_dag,&last_parent);
		if(lp != NULL )
		{
			lp->parent_sum_weight -= last_weight;
		}
#endif
	}
#if MODE_DIO_WEIGHT_UPDATED
	uint8_t prev_weight = my_weight;
	// If my_weight changed,
	if(prev_weight != my_weight)
	{
		dio_broadcast(rpl_get_default_instance());
	}
#endif
	/* child info list add
	   Compare it with previous info */
	uip_clear_buf();
}
/*---------------------------------------------------------------------------*/
void
dio_ack_output(rpl_instance_t *instance, uip_ipaddr_t *uc_addr)
{
	unsigned char *buffer;
	int pos;
	uip_ipaddr_t addr;
	rpl_dag_t *dag = instance->current_dag;
	rpl_parent_t *p = dag->preferred_parent;

	dio_ack_count ++;
#if RPL_ICMP_ENERGY_LOG
	char *log_buf = (char*) malloc(sizeof(char)*100);
	sprintf(log_buf,"DIO_ACK_OUTPUT, Energy: %d\n",(int) get_residual_energy());
	LOG_MESSAGE(log_buf);
	free(log_buf);
#endif

	pos = 0;

	buffer = UIP_ICMP_PAYLOAD;

	if(p != NULL)
	{
		PRINTF("send dio_ack addr: ");
		PRINT6ADDR(rpl_get_parent_ipaddr(dag->preferred_parent));
		PRINTF("\n");

		memcpy(&buffer[pos],rpl_get_parent_ipaddr(p),16);
		pos += 16;
		PRINTF("dio_ack p weight %d\n",p->parent_weight);
		buffer[pos++] = p->parent_weight;
	}
	else
	{
		PRINTF("send dio_ack parent NULL\n");
		buffer[pos++] = 1;
	}
#if MODE_LAST_PARENT
	if(instance->last_parent != NULL)
	{
		PRINTF("send dio_ack lp addr: ");
		PRINT6ADDR(rpl_get_parent_ipaddr(instance->last_parent));
		PRINTF("\n");

		memcpy(&buffer[pos],rpl_get_parent_ipaddr(instance->last_parent),16);
		pos += 16;
		PRINTF("dio_ack lp weight %d\n",instance->last_parent_weight);
		buffer[pos++] = instance->last_parent_weight;
	}
#endif

	if (uc_addr == NULL)
	{
		PRINTF("RPL: Sending a multicast-DIO_ACK\n");
		uip_create_linklocal_rplnodes_mcast(&addr);
		uip_icmp6_send(&addr, ICMP6_RPL, RPL_CODE_DIO_ACK, pos);
	}
	else
	{
		uip_ds6_nbr_t *nbr = NULL;
		nbr = uip_ds6_nbr_lookup(uc_addr);

		/*JOONKI*/
#if DUAL_RADIO
#if ADDR_MAP
		int i;
		for (i=0; i<NBR_TABLE_MAX_NEIGHBORS; i++){
			/*		 PRINTLLADDR(uip_ds6_nbr_get_ll(nbr));
				PRINTF("\n");
				PRINTLLADDR(&ds6_lr_addrmap[i].lladdr);
				PRINTF("\n");*/
			if (linkaddr_cmp(&ds6_lr_addrmap[i].lladdr, (const linkaddr_t *)uip_ds6_nbr_get_ll(nbr))){
				if (ds6_lr_addrmap[i].lr == 1){
					dual_radio_switch(LONG_RADIO);
				}	else	{
					dual_radio_switch(SHORT_RADIO);
				}
				//			PRINTF("SUCCESS!!!\n");
				break;
			}
		}
#else /* ADDR_MAP */
		if (dest->u8[8] == 0x82)	{
			dual_radio_switch(LONG_RADIO);
		}	else	{
			dual_radio_switch(SHORT_RADIO);
		}
#endif	/* ADDR_MAP */
#endif /* DUAL_RADIO */
	uip_icmp6_send(uc_addr, ICMP6_RPL, RPL_CODE_DIO_ACK, pos);
	}
}
#endif

#if LSA_R
void
LSA_converge_input(void)
{
	uip_ipaddr_t from;
	int pos;
	unsigned char * buffer;
	uint8_t buffer_length;
	uint8_t LSA_lr_child;
	rpl_parent_t * preferred_parent;
	uip_ds6_nbr_t *nbr;

	PRINTF("LSA_converge_input\n");
	uip_ipaddr_copy(&from, &UIP_IP_BUF->srcipaddr);
	PRINTF("LSA: Received a LSA RI from ");
	PRINT6ADDR(&from);
	PRINTF("\n");
	
	buffer_length = uip_len - uip_l3_icmp_hdr_len;

	pos = 0;
	buffer = UIP_ICMP_PAYLOAD;

	LSA_lr_child = buffer[pos++];
	


	rpl_parent_t *p = nbr_table_head(rpl_parents);
	if (p != NULL) {
		preferred_parent = p->dag->preferred_parent;
		if(preferred_parent != NULL)
		{
			nbr = rpl_get_nbr(preferred_parent);
		}
	}

	if (uip_ip6addr_cmp(&from, &nbr->ipaddr)) {
		rpl_LSA_convergence_timer(2);
		LSA_SR_preamble = !LSA_lr_child;
		LSA_message_input = 1;
		printf("LSA: LSA_SR_preamble is %d\n",LSA_SR_preamble);
	}

	uip_clear_buf();
}
/*---------------------------------------------------------------------------*/
void
LSA_converge_output(uint8_t lr_child)
{
  unsigned char *buffer;
  int pos;
	uip_ipaddr_t addr;
	LSA_count ++;
#if RPL_ICMP_ENERGY_LOG
	char *log_buf = (char*) malloc(sizeof(char)*100);
	sprintf(log_buf,"LSA_OUTPUT, Energy: %d\n",(int) get_residual_energy()); 
	LOG_MESSAGE(log_buf); 
	free(log_buf);
#endif

	/* LSA routing information */
  pos = 0;
  buffer = UIP_ICMP_PAYLOAD;
	buffer[pos++] = lr_child;
	
	PRINTF("LSA: Sending a LSA routing information\n");
  uip_create_linklocal_rplnodes_mcast(&addr);
  uip_icmp6_send(&addr, ICMP6_RPL, RPL_CODE_LSA, pos);
}

#endif /* LSA_R */
/*---------------------------------------------------------------------------*/
static void
dao_input(void)
{
	uip_ipaddr_t dao_sender_addr;
	rpl_dag_t *dag;
	rpl_instance_t *instance;
	unsigned char *buffer;
	uint16_t sequence;
	uint8_t instance_id;
	uint8_t lifetime;
	uint8_t prefixlen;
	uint8_t flags;
	uint8_t subopt_type;
	/*
  uint8_t pathcontrol;
  uint8_t pathsequence;
	 */
	uip_ipaddr_t prefix;
	uip_ds6_route_t *rep;
	uint8_t buffer_length;
	int pos;
	int len;
	int i;
	int learned_from;
	rpl_parent_t *parent;
	uip_ds6_nbr_t *nbr;
	int is_root;
	uip_ipaddr_t tmp_addr;
	uip_ipaddr_t tmp_addr_2;
#if RPL_LIFETIME_MAX_MODE
	rpl_child_t *c = NULL;
	uint8_t weight;
#endif

  prefixlen = 0;
  parent = NULL;

  uip_ipaddr_copy(&dao_sender_addr, &UIP_IP_BUF->srcipaddr);
  
	/* DAG Information Object */
  PRINTF("RPL: Received a DAO from ");
  PRINT6ADDR(&dao_sender_addr);
  PRINTF("\n");

  buffer = UIP_ICMP_PAYLOAD;
  buffer_length = uip_len - uip_l3_icmp_hdr_len;

  pos = 0;
  instance_id = buffer[pos++];

  instance = rpl_get_instance(instance_id);
  if(instance == NULL) {
    PRINTF("RPL: Ignoring a DAO for an unknown RPL instance(%u)\n",
           instance_id);
    goto discard;
  }

  lifetime = instance->default_lifetime;

  flags = buffer[pos++];
  /* reserved */
  pos++;
  sequence = buffer[pos++];

  dag = instance->current_dag;
  is_root = (dag->rank == ROOT_RANK(instance));

  /* Is the DAG ID present? */
  if(flags & RPL_DAO_D_FLAG) {
    if(memcmp(&dag->dag_id, &buffer[pos], sizeof(dag->dag_id))) {
      PRINTF("RPL: Ignoring a DAO for a DAG different from ours\n");
      goto discard;
    }
    pos += 16;
  }

  learned_from = uip_is_addr_mcast(&dao_sender_addr) ?
                 RPL_ROUTE_FROM_MULTICAST_DAO : RPL_ROUTE_FROM_UNICAST_DAO;

  /* Destination Advertisement Object */
  PRINTF("RPL: Received a (%s) DAO with sequence number %u from ",
      learned_from == RPL_ROUTE_FROM_UNICAST_DAO? "unicast": "multicast", sequence);
  PRINT6ADDR(&dao_sender_addr);
  PRINTF("\n");

  if(learned_from == RPL_ROUTE_FROM_UNICAST_DAO) {
    /* Check whether this is a DAO forwarding loop. */
    parent = rpl_find_parent(dag, &dao_sender_addr);
    /* check if this is a new DAO registration with an "illegal" rank */
    /* if we already route to this node it is likely */
    if(parent != NULL &&
       DAG_RANK(parent->rank, instance) < DAG_RANK(dag->rank, instance)) {
      PRINTF("RPL: Loop detected when receiving a unicast DAO from a node with a lower rank! (%u < %u)\n",
          DAG_RANK(parent->rank, instance), DAG_RANK(dag->rank, instance));
      parent->rank = INFINITE_RANK;
      parent->flags |= RPL_PARENT_FLAG_UPDATED;
      goto discard;
    }

    /* If we get the DAO from our parent, we also have a loop. */
    if(parent != NULL && parent == dag->preferred_parent) {
      PRINTF("RPL: Loop detected when receiving a unicast DAO from our parent\n");
      parent->rank = INFINITE_RANK;
      parent->flags |= RPL_PARENT_FLAG_UPDATED;
      goto discard;
    }
  }

  /* Check if there are any RPL options present. */
  for(i = pos; i < buffer_length; i += len) {
    subopt_type = buffer[i];
    if(subopt_type == RPL_OPTION_PAD1) {
      len = 1;
    } else {
      /* The option consists of a two-byte header and a payload. */
      len = 2 + buffer[i + 1];
    }

    switch(subopt_type) {
    case RPL_OPTION_TARGET:
      /* Handle the target option. */
      prefixlen = buffer[i + 3];
      memset(&prefix, 0, sizeof(prefix));
      memcpy(&prefix, buffer + i + 4, (prefixlen + 7) / CHAR_BIT);
      break;
    case RPL_OPTION_TRANSIT:
      /* The path sequence and control are ignored. */
      /*      pathcontrol = buffer[i + 3];
              pathsequence = buffer[i + 4];*/
      lifetime = buffer[i + 5];
      /* The parent address is also ignored. */
      break;
    }
  }

  PRINTF("RPL: DAO lifetime: %u, prefix length: %u prefix: ",
          (unsigned)lifetime, (unsigned)prefixlen);
  PRINT6ADDR(&prefix);
  PRINTF("\n");

#if RPL_CONF_MULTICAST
  if(uip_is_addr_mcast_global(&prefix)) {
    mcast_group = uip_mcast6_route_add(&prefix);
    if(mcast_group) {
      mcast_group->dag = dag;
      mcast_group->lifetime = RPL_LIFETIME(instance, lifetime);
    }
    goto fwd_dao;
  }
#endif

  rep = uip_ds6_route_lookup(&prefix);

  if(lifetime == RPL_ZERO_LIFETIME) {
    PRINTF("RPL: No-Path DAO received\n");
    /* No-Path DAO received; invoke the route purging routine. */
    if(rep != NULL &&
       !RPL_ROUTE_IS_NOPATH_RECEIVED(rep) &&
       rep->length == prefixlen &&
       uip_ds6_route_nexthop(rep) != NULL &&
       uip_ipaddr_cmp(uip_ds6_route_nexthop(rep), &dao_sender_addr)) {
      PRINTF("RPL: Setting expiration timer for prefix ");
      PRINT6ADDR(&prefix);
      PRINTF("\n");
      RPL_ROUTE_SET_NOPATH_RECEIVED(rep);
      rep->state.lifetime = RPL_NOPATH_REMOVAL_DELAY;

      /* We forward the incoming No-Path DAO to our parent, if we have
         one. */
      if(dag->preferred_parent != NULL &&
         rpl_get_parent_ipaddr(dag->preferred_parent) != NULL) {
        uint8_t out_seq;
        out_seq = prepare_for_dao_fwd(sequence, rep);

/*JOONKI*/
  			uip_ipaddr_copy(&tmp_addr, rpl_get_parent_ipaddr(dag->preferred_parent));

#if DUAL_RADIO
#if ADDR_MAP
			int i;
			uip_ds6_nbr_t *nbr = NULL;
			nbr = uip_ds6_nbr_lookup(&tmp_addr);	
			for (i=0; i<NBR_TABLE_MAX_NEIGHBORS; i++){
				/* PRINTLLADDR(uip_ds6_nbr_get_ll(nbr));
				PRINTF("\n");
				PRINTLLADDR(&ds6_lr_addrmap[i].lladdr);
				PRINTF("\n");*/
				if (linkaddr_cmp(&ds6_lr_addrmap[i].lladdr, (const linkaddr_t *)uip_ds6_nbr_get_ll(nbr))){
					if (ds6_lr_addrmap[i].lr == 1){
							dual_radio_switch(LONG_RADIO);
						}	else	{
							dual_radio_switch(SHORT_RADIO);
						}
					// PRINTF("SUCCESS!!!\n");
					break;
				}
			}
#else /* ADDR_MAP */
				if (tmp_addr.u8[8] == 0x82)	{
					dual_radio_switch(LONG_RADIO);
				}	else	{
					dual_radio_switch(SHORT_RADIO);
				}

#endif	/* ADDR_MAP */
#endif /* DUAL_RADIO */

        PRINTF("RPL: Forwarding No-path DAO to parent - out_seq:%d",
	       out_seq);
        PRINT6ADDR(&tmp_addr);
        PRINTF("\n");

        buffer = UIP_ICMP_PAYLOAD;
        buffer[3] = out_seq; /* add an outgoing seq no before fwd */
				
				dao_fwd_count ++;
#if RPL_ICMP_ENERGY_LOG
				char *log_buf = (char*) malloc(sizeof(char)*100);
				sprintf(log_buf,"DAO_FWD_OUTPUT NO PATH, Energy: %d\n",(int) get_residual_energy()); 
				LOG_MESSAGE(log_buf); 
				free(log_buf);
#endif

        uip_icmp6_send(rpl_get_parent_ipaddr(dag->preferred_parent),
                       ICMP6_RPL, RPL_CODE_DAO, buffer_length);
      }
    } 
    /* independent if we remove or not - ACK the request */
    if(flags & RPL_DAO_K_FLAG) {
      /* indicate that we accepted the no-path DAO */
      dao_ack_output(instance, &dao_sender_addr, sequence,
                     RPL_DAO_ACK_UNCONDITIONAL_ACCEPT);
    }
    goto discard;
  }

  PRINTF("RPL: Adding DAO route\n");

  /* Update and add neighbor - if no room - fail. */
  if((nbr = rpl_icmp6_update_nbr_table(&dao_sender_addr, NBR_TABLE_REASON_RPL_DAO, instance)) == NULL) {
    PRINTF("RPL: Out of Memory, dropping DAO from ");
    PRINT6ADDR(&dao_sender_addr);
    PRINTF(", ");
    PRINTLLADDR((uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_SENDER));
    PRINTF("\n");
    if(flags & RPL_DAO_K_FLAG) {
      /* signal the failure to add the node */
      dao_ack_output(instance, &dao_sender_addr, sequence,
		     is_root ? RPL_DAO_ACK_UNABLE_TO_ADD_ROUTE_AT_ROOT :
		     RPL_DAO_ACK_UNABLE_TO_ACCEPT);
    }
    goto discard;
  }
  rep = rpl_add_route(dag, &prefix, prefixlen, &dao_sender_addr);
  if(rep == NULL) {
    RPL_STAT(rpl_stats.mem_overflows++);
    PRINTF("RPL: Could not add a route after receiving a DAO\n");
    if(flags & RPL_DAO_K_FLAG) {
      /* signal the failure to add the node */
      dao_ack_output(instance, &dao_sender_addr, sequence,
		     is_root ? RPL_DAO_ACK_UNABLE_TO_ADD_ROUTE_AT_ROOT :
		     RPL_DAO_ACK_UNABLE_TO_ACCEPT);
    }
    goto discard;
  }

  /* set lifetime and clear NOPATH bit */
  rep->state.lifetime = RPL_LIFETIME(instance, lifetime);
  RPL_ROUTE_CLEAR_NOPATH_RECEIVED(rep);

#if RPL_CONF_MULTICAST
fwd_dao:
#endif
#if RPL_LIFETIME_MAX_MODE
	uint8_t prev_weight = my_weight;
  /* Add child as receiving DAO */
/*  if(!add_nbr_from_dao(&dao_sender_addr, weight))
  {
	  PRINTF("fail to add nbr with dao\n");
  }
  PRINTF("DAO_sender:");
  PRINT6ADDR(&dao_sender_addr);*/
  c = rpl_find_child(&dao_sender_addr);

#if DUAL_RADIO
  uint8_t is_longrange = radio_received_is_longrange();
  weight = is_longrange == LONG_RADIO ? LONG_WEIGHT_RATIO : 1;
#else
  weight = 1;
#endif
  if (c == NULL)
  {
	  c = rpl_add_child(weight,&dao_sender_addr);
	  if(c == NULL)
	  {
		  PRINTF("fail to add child\n");
	  }
	  else
	  {
		  my_weight += c->weight;
		  PRINTF("my_weight in dao %d\n",my_weight);
	  }
	  PRINTF("child list in dao input\n");
	  rpl_print_child_neighbor_list();
  }
  else
  {
	  if(c->weight != weight)
	  {
		  my_weight -= c->weight;
		  c->weight = weight;
		  my_weight += c->weight;
		  PRINTF("my_weight update in dao %d\n",my_weight);
	  }
  }
#endif
  if(learned_from == RPL_ROUTE_FROM_UNICAST_DAO) {
    int should_ack = 0;

    if(flags & RPL_DAO_K_FLAG) {
      /*
       * check if this route is already installed and we can ack now!
       * not pending - and same seq-no means that we can ack.
       * (e.g. the route is installed already so it will not take any
       * more room that it already takes - so should be ok!)
       */
      if((!RPL_ROUTE_IS_DAO_PENDING(rep) &&
          rep->state.dao_seqno_in == sequence) ||
          dag->rank == ROOT_RANK(instance)) {
        should_ack = 1;
      }
    }
		
    if(dag->preferred_parent != NULL &&
       rpl_get_parent_ipaddr(dag->preferred_parent) != NULL) {
      uint8_t out_seq = 0;
      /* if this is pending and we get the same seq no it is a retrans */
      if(RPL_ROUTE_IS_DAO_PENDING(rep) &&
         rep->state.dao_seqno_in == sequence) {
        /* keep the same seq-no as before for parent also */
        out_seq = rep->state.dao_seqno_out;
      } else {
        out_seq = prepare_for_dao_fwd(sequence, rep);
      }

/*JOONKI*/
  			uip_ipaddr_copy(&tmp_addr_2, rpl_get_parent_ipaddr(dag->preferred_parent));

#if DUAL_RADIO
#if ADDR_MAP
			int i;
			uip_ds6_nbr_t *nbr = NULL;
			nbr = uip_ds6_nbr_lookup(&tmp_addr_2);	
			for (i=0; i<NBR_TABLE_MAX_NEIGHBORS; i++){
				/* PRINTLLADDR(uip_ds6_nbr_get_ll(nbr));
				PRINTF("\n");
				PRINTLLADDR(&ds6_lr_addrmap[i].lladdr);
				PRINTF("\n");*/
				if (linkaddr_cmp(&ds6_lr_addrmap[i].lladdr, (const linkaddr_t *)uip_ds6_nbr_get_ll(nbr))){
					if (ds6_lr_addrmap[i].lr == 1){
							dual_radio_switch(LONG_RADIO);
						}	else	{
							dual_radio_switch(SHORT_RADIO);
						}
					// PRINTF("SUCCESS!!!\n");
					break;
				}
			}
#else /* ADDR_MAP */
				if (tmp_addr_2.u8[8] == 0x82)	{
					dual_radio_switch(LONG_RADIO);
				}	else	{
					dual_radio_switch(SHORT_RADIO);
				}
#endif	/* ADDR_MAP */
#endif /* DUAL_RADIO */


      PRINTF("RPL: Forwarding DAO to parent ");
      PRINT6ADDR(&tmp_addr_2);
      PRINTF(" in seq: %d out seq: %d\n", sequence, out_seq);

      buffer = UIP_ICMP_PAYLOAD;
      buffer[3] = out_seq; /* add an outgoing seq no before fwd */

			dao_fwd_count ++;
#if RPL_ICMP_ENERGY_LOG
			char *log_buf = (char*) malloc(sizeof(char)*100);
			sprintf(log_buf,"DAO_FWD_OUTPUT, Energy: %d\n",(int) get_residual_energy()); 
			LOG_MESSAGE(log_buf); 
			free(log_buf);
#endif
      uip_icmp6_send(rpl_get_parent_ipaddr(dag->preferred_parent),
                     ICMP6_RPL, RPL_CODE_DAO, buffer_length);
    }
    if(should_ack) {

      PRINTF("RPL: Sending DAO ACK\n");
      dao_ack_output(instance, &dao_sender_addr, sequence,
                     RPL_DAO_ACK_UNCONDITIONAL_ACCEPT);
    }
  }
#if RPL_LIFETIME_MAX_MODE
  if(prev_weight != my_weight)
  {
	  PRINTF("DIO Reset in DAO\n");
	  rpl_reset_dio_timer(instance);
  }
#endif

 discard:
  uip_clear_buf();
}
/*---------------------------------------------------------------------------*/
#if RPL_WITH_DAO_ACK
static void
handle_dao_retransmission(void *ptr)
{
  rpl_parent_t *parent;
  uip_ipaddr_t prefix;
  rpl_instance_t *instance;

  parent = ptr;
  if(parent == NULL || parent->dag == NULL || parent->dag->instance == NULL) {
    return;
  }
  instance = parent->dag->instance;

  if(instance->my_dao_transmissions >= RPL_DAO_MAX_RETRANSMISSIONS) {
    /* No more retransmissions - give up. */
    if(instance->lifetime_unit == 0xffff && instance->default_lifetime == 0xff) {
      /*
       * ContikiRPL was previously using infinite lifetime for routes
       * and no DAO_ACK configured. This probably means that the root
       * and possibly other nodes might be running an old version that
       * does not support DAO ack. Assume that everything is ok for
       * now and let the normal repair mechanisms detect any problems.
       */
      return;
    }

    if(instance->of->dao_ack_callback) {
      /* Inform the objective function about the timeout. */
      instance->of->dao_ack_callback(parent, RPL_DAO_ACK_TIMEOUT);
    }

    /* Perform local repair and hope to find another parent. */
    rpl_local_repair(instance);
    return;
  }

  PRINTF("RPL: will retransmit DAO - seq:%d trans:%d\n", instance->my_dao_seqno,
	 instance->my_dao_transmissions);

  if(get_global_addr(&prefix) == 0) {
    return;
  }

  ctimer_set(&instance->dao_retransmit_timer,
             RPL_DAO_RETRANSMISSION_TIMEOUT / 2 +
             (random_rand() % (RPL_DAO_RETRANSMISSION_TIMEOUT / 2)),
	     handle_dao_retransmission, parent);

  instance->my_dao_transmissions++;
  dao_output_target_seq(parent, &prefix,
			instance->default_lifetime, instance->my_dao_seqno);
}
#endif /* RPL_WITH_DAO_ACK */
/*---------------------------------------------------------------------------*/
void
dao_output(rpl_parent_t *parent, uint8_t lifetime)
{
  /* Destination Advertisement Object */
  uip_ipaddr_t prefix;

	dao_count ++;
#if RPL_ICMP_ENERGY_LOG
	char *log_buf = (char*) malloc(sizeof(char)*100);
	sprintf(log_buf,"DAO_OUTPUT, Energy: %d\n",(int) get_residual_energy()); 
	LOG_MESSAGE(log_buf); 
	free(log_buf);
#endif


  if(get_global_addr(&prefix) == 0) {
    PRINTF("RPL: No global address set for this node - suppressing DAO\n");
    return;
  }

  if(parent == NULL || parent->dag == NULL || parent->dag->instance == NULL) {
    return;
  }

  RPL_LOLLIPOP_INCREMENT(dao_sequence);
#if RPL_WITH_DAO_ACK
  /* set up the state since this will be the first transmission of DAO */
  /* retransmissions will call directly to dao_output_target_seq */
  /* keep track of my own sending of DAO for handling ack and loss of ack */
  if(lifetime != RPL_ZERO_LIFETIME) {
    rpl_instance_t *instance;
    instance = parent->dag->instance;

    instance->my_dao_seqno = dao_sequence;
    instance->my_dao_transmissions = 1;
    ctimer_set(&instance->dao_retransmit_timer, RPL_DAO_RETRANSMISSION_TIMEOUT,
 	       handle_dao_retransmission, parent);
  }
#else
   /* We know that we have tried to register so now we are assuming
      that we have a down-link - unless this is a zero lifetime one */
  parent->dag->instance->has_downward_route = lifetime != RPL_ZERO_LIFETIME;
#endif /* RPL_WITH_DAO_ACK */

  /* Sending a DAO with own prefix as target */
  dao_output_target(parent, &prefix, lifetime);
}
/*---------------------------------------------------------------------------*/
void
dao_output_target(rpl_parent_t *parent, uip_ipaddr_t *prefix, uint8_t lifetime)
{
  dao_output_target_seq(parent, prefix, lifetime, dao_sequence);
}
/*---------------------------------------------------------------------------*/
static void
dao_output_target_seq(rpl_parent_t *parent, uip_ipaddr_t *prefix,
		      uint8_t lifetime, uint8_t seq_no)
{
  rpl_dag_t *dag;
  rpl_instance_t *instance;
  unsigned char *buffer;
  uint8_t prefixlen;
  int pos;
  uip_ipaddr_t dst;


  /* Destination Advertisement Object */

  /* If we are in feather mode, we should not send any DAOs */
  if(rpl_get_mode() == RPL_MODE_FEATHER) {
    return;
  }

  if(parent == NULL) {
    PRINTF("RPL dao_output_target error parent NULL\n");
    return;
  }

  dag = parent->dag;
  if(dag == NULL) {
    PRINTF("RPL dao_output_target error dag NULL\n");
    return;
  }

  instance = dag->instance;

  if(instance == NULL) {
    PRINTF("RPL dao_output_target error instance NULL\n");
    return;
  }
  if(prefix == NULL) {
    PRINTF("RPL dao_output_target error prefix NULL\n");
    return;
  }
#ifdef RPL_DEBUG_DAO_OUTPUT
  RPL_DEBUG_DAO_OUTPUT(parent);
#endif

  buffer = UIP_ICMP_PAYLOAD;
  pos = 0;

  buffer[pos++] = instance->instance_id;
  buffer[pos] = 0;
#if RPL_DAO_SPECIFY_DAG
  buffer[pos] |= RPL_DAO_D_FLAG;
#endif /* RPL_DAO_SPECIFY_DAG */
#if RPL_WITH_DAO_ACK
  if(lifetime != RPL_ZERO_LIFETIME) {
    buffer[pos] |= RPL_DAO_K_FLAG;
  }
#endif /* RPL_WITH_DAO_ACK */
  ++pos;
  buffer[pos++] = 0; /* reserved */
  buffer[pos++] = seq_no;
#if RPL_DAO_SPECIFY_DAG
  memcpy(buffer + pos, &dag->dag_id, sizeof(dag->dag_id));
  pos+=sizeof(dag->dag_id);
#endif /* RPL_DAO_SPECIFY_DAG */

  /* create target subopt */
  prefixlen = sizeof(*prefix) * CHAR_BIT;
  buffer[pos++] = RPL_OPTION_TARGET;
  buffer[pos++] = 2 + ((prefixlen + 7) / CHAR_BIT);
  buffer[pos++] = 0; /* reserved */
  buffer[pos++] = prefixlen;
  memcpy(buffer + pos, prefix, (prefixlen + 7) / CHAR_BIT);
  pos += ((prefixlen + 7) / CHAR_BIT);

  /* Create a transit information sub-option. */
  buffer[pos++] = RPL_OPTION_TRANSIT;
  buffer[pos++] = 4;
  buffer[pos++] = 0; /* flags - ignored */
  buffer[pos++] = 0; /* path control - ignored */
  buffer[pos++] = 0; /* path seq - ignored */
  buffer[pos++] = lifetime;

/* JOONKI */
  uip_ipaddr_copy(&dst, rpl_get_parent_ipaddr(parent));
#if DUAL_RADIO
#if ADDR_MAP
	int i;
	uip_ds6_nbr_t *nbr = NULL;

	nbr = uip_ds6_nbr_lookup(&dst);	
	for (i=0; i<NBR_TABLE_MAX_NEIGHBORS; i++){
		/* PRINTLLADDR(uip_ds6_nbr_get_ll(nbr));
		PRINTF("\n");
		PRINTLLADDR(&ds6_lr_addrmap[i].lladdr);
		PRINTF("\n"); */
		if (linkaddr_cmp(&ds6_lr_addrmap[i].lladdr, (const linkaddr_t *)uip_ds6_nbr_get_ll(nbr))){
			if (ds6_lr_addrmap[i].lr == 1){
					dual_radio_switch(LONG_RADIO);
				}	else	{
					dual_radio_switch(SHORT_RADIO);
				}
			// PRINTF("SUCCESS!!!\n");
			break;
		}
	}
	
#else /* DDR_MAP */
	if (dst.u8[8] == 0x82)	{
		dual_radio_switch(LONG_RADIO);
	}	else	{
		dual_radio_switch(SHORT_RADIO);
	}
#endif	/* ADDR_MAP */
#endif /* DUAL_RADIO */
	PRINTF("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ DAO_OUTPUT @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
  PRINTF("RPL: Sending a %sDAO with sequence number %u, lifetime %u, prefix ",
      lifetime == RPL_ZERO_LIFETIME ? "No-Path " : "", seq_no, lifetime);
  PRINT6ADDR(prefix);
  PRINTF(" to ");
  PRINT6ADDR(&dst);
  PRINTF("\n");

  if(rpl_get_parent_ipaddr(parent)!= NULL) {
    uip_icmp6_send(&dst, ICMP6_RPL, RPL_CODE_DAO, pos);
  }
}
/*---------------------------------------------------------------------------*/
static void
dao_ack_input(void)
{
#if RPL_WITH_DAO_ACK

  uint8_t *buffer;
  uint8_t instance_id;
  uint8_t sequence;
  uint8_t status;
  rpl_instance_t *instance;
  rpl_parent_t *parent;
  uip_ipaddr_t from;

  buffer = UIP_ICMP_PAYLOAD;

  instance_id = buffer[0];
  sequence = buffer[2];
  status = buffer[3];

  instance = rpl_get_instance(instance_id);
  if(instance == NULL) {
    uip_clear_buf();
    return;
  }

  uip_ipaddr_copy(&from, &UIP_IP_BUF->srcipaddr);

	/* DAG Information Object */
  PRINTF("RPL: Received a DAO_ACK from ");
  PRINT6ADDR(&from);
  PRINTF("\n");


	parent = rpl_find_parent(instance->current_dag, &from);
  if(parent == NULL) {
    /* not a known instance - drop the packet and ignore */
    uip_clear_buf();
    return;
  }

  PRINTF("RPL: Received a DAO %s with sequence number %d (%d) and status %d from ",
   status < 128 ? "ACK" : "NACK",
	 sequence, instance->my_dao_seqno, status);
  PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
  PRINTF("\n");

  if(sequence == instance->my_dao_seqno) {
    instance->has_downward_route = status < 128;

    /* always stop the retransmit timer when the ACK arrived */
    ctimer_stop(&instance->dao_retransmit_timer);

    /* Inform objective function on status of the DAO ACK */
    if(instance->of->dao_ack_callback) {
      instance->of->dao_ack_callback(parent, status);
    }

#if RPL_REPAIR_ON_DAO_NACK
    if(status >= RPL_DAO_ACK_UNABLE_TO_ACCEPT) {
      /*
       * Failed the DAO transmission - need to remove the default route.
       * Trigger a local repair since we can not get our DAO in.
       */
      rpl_local_repair(instance);
    }
#endif

  } else {
    /* this DAO ACK should be forwarded to another recently registered route */
    uip_ds6_route_t *re;
    uip_ipaddr_t *nexthop;
    if((re = find_route_entry_by_dao_ack(sequence)) != NULL) {
      /* pick the recorded seq no from that node and forward DAO ACK - and
         clear the pending flag*/
      RPL_ROUTE_CLEAR_DAO_PENDING(re);

      nexthop = uip_ds6_route_nexthop(re);

/*JOONKI*/
#if DUAL_RADIO
#if ADDR_MAP
			int i;
			uip_ds6_nbr_t *nbr = NULL;
			nbr = uip_ds6_nbr_lookup(nexthop);	
			for (i=0; i<NBR_TABLE_MAX_NEIGHBORS; i++){
				/* PRINTLLADDR(uip_ds6_nbr_get_ll(nbr));
				PRINTF("\n");
				PRINTLLADDR(&ds6_lr_addrmap[i].lladdr);
				PRINTF("\n"); */
				if (linkaddr_cmp(&ds6_lr_addrmap[i].lladdr, (const linkaddr_t *)uip_ds6_nbr_get_ll(nbr))){
					if (ds6_lr_addrmap[i].lr == 1){
							dual_radio_switch(LONG_RADIO);
						}	else	{
							dual_radio_switch(SHORT_RADIO);
						}
					// PRINTF("SUCCESS!!!\n");
					break;
				}
			}
#else /* ADDR_MAP */
			if (nexthop->u8[8] == 0x82)	{
				dual_radio_switch(LONG_RADIO);
			}	else	{
				dual_radio_switch(SHORT_RADIO);
			}
#endif	/* ADDR_MAP */
#endif /* DUAL_RADIO */

      if(nexthop == NULL) {
        PRINTF("RPL: No next hop to fwd DAO ACK to\n");
      } else {
        PRINTF("RPL: Fwd DAO ACK to:");
        PRINT6ADDR(nexthop);
        PRINTF("\n");
        buffer[2] = re->state.dao_seqno_in;
				dao_ack_fwd_count ++;
#if RPL_ICMP_ENERGY_LOG
				char *log_buf = (char*) malloc(sizeof(char)*100);
				sprintf(log_buf,"DAO_ACK_FWD_OUTPUT, Energy: %d\n",(int) get_residual_energy()); 
				LOG_MESSAGE(log_buf); 
				free(log_buf);
#endif
        uip_icmp6_send(nexthop, ICMP6_RPL, RPL_CODE_DAO_ACK, 4);
      }

      if(status >= RPL_DAO_ACK_UNABLE_TO_ACCEPT) {
        /* this node did not get in to the routing tables above... - remove */
        uip_ds6_route_rm(re);
      }
    } else {
      PRINTF("RPL: No route entry found to forward DAO ACK (seqno %u)\n", sequence);
    }
  }
#endif /* RPL_WITH_DAO_ACK */
  uip_clear_buf();
}
/*---------------------------------------------------------------------------*/
void
dao_ack_output(rpl_instance_t *instance, uip_ipaddr_t *dest, uint8_t sequence,
	       uint8_t status)
{
#if RPL_WITH_DAO_ACK
  unsigned char *buffer;
	
	dao_ack_count ++;
#if RPL_ICMP_ENERGY_LOG
	char *log_buf = (char*) malloc(sizeof(char)*100);
	sprintf(log_buf,"DAO_ACK_OUTPUT, Energy: %d\n",(int) get_residual_energy()); 
	LOG_MESSAGE(log_buf); 
	free(log_buf);
#endif

/*JOONKI*/

#if DUAL_RADIO
#if ADDR_MAP
		int i;
		uip_ds6_nbr_t *nbr = NULL;
		nbr = uip_ds6_nbr_lookup(dest);	
		for (i=0; i<NBR_TABLE_MAX_NEIGHBORS; i++){
			/* PRINTLLADDR(uip_ds6_nbr_get_ll(nbr));
			PRINTF("\n");
			PRINTLLADDR(&ds6_lr_addrmap[i].lladdr);
			PRINTF("\n"); */
			if (linkaddr_cmp(&ds6_lr_addrmap[i].lladdr, (const linkaddr_t *)uip_ds6_nbr_get_ll(nbr))){
				if (ds6_lr_addrmap[i].lr == 1){
					dual_radio_switch(LONG_RADIO);
				}	else	{
					dual_radio_switch(SHORT_RADIO);
				}
				// PRINTF("SUCCESS!!!\n");
				break;
			}
		}
#else /* ADDR_MAP */
	if (dest->u8[8] == 0x82)	{
		dual_radio_switch(LONG_RADIO);
	}	else	{
		dual_radio_switch(SHORT_RADIO);
	}
#endif	/* ADDR_MAP */
#endif /* DUAL_RADIO */


  PRINTF("RPL: Sending a DAO %s with sequence number %d to ", status < 128 ? "ACK" : "NACK", sequence);
  PRINT6ADDR(dest);
  PRINTF(" with status %d\n", status);

  buffer = UIP_ICMP_PAYLOAD;

  buffer[0] = instance->instance_id;
  buffer[1] = 0;
  buffer[2] = sequence;
  buffer[3] = status;

  uip_icmp6_send(dest, ICMP6_RPL, RPL_CODE_DAO_ACK, 4);
#endif /* RPL_WITH_DAO_ACK */
}
/*---------------------------------------------------------------------------*/
void
rpl_icmp6_register_handlers()
{
  uip_icmp6_register_input_handler(&dis_handler);
  uip_icmp6_register_input_handler(&dio_handler);
  uip_icmp6_register_input_handler(&dao_handler);
  uip_icmp6_register_input_handler(&dao_ack_handler);
#if RPL_LIFETIME_MAX_MODE_DIO_ACK
  uip_icmp6_register_input_handler(&dio_ack_handler);
#endif
#if LSA_R
  uip_icmp6_register_input_handler(&LSA_converge_handler);
#endif
}
/*---------------------------------------------------------------------------*/

/** @}*/
