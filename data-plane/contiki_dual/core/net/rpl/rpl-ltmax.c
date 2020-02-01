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
 *         The Minimum Rank with Hysteresis Objective Function (MRHOF)
 *
 *         This implementation uses the estimated number of
 *         transmissions (ETX) as the additive routing metric,
 *         and also provides stubs for the energy metric.
 *
 * \author Joakim Eriksson <joakime@sics.se>, Nicolas Tsiftes <nvt@sics.se>
 */

/**
 * \addtogroup uip6
 * @{
 */
#include "net/rpl/rpl-private.h"
#include "net/nbr-table.h"

#include "rpl_debug.h"
#define DEBUG DEBUG_RPL_LTMAX_OF
#include "net/ip/uip-debug.h"

/* JOONKI */
#if DUAL_RADIO
#ifdef ZOLERTIA_Z1
#include	"../platform/z1/dual_radio.h"
#elif COOJA /* ZOLERTIA_Z1 */
#include	"../platform/cooja/dual_conf.h"
#else /* ZOLERTIA_Z1 */
#include "../platform/zoul/dual_radio.h"
#endif /* ZOLERTIA_Z1 */
#endif /* DUAL_RADIO */
/* JJH */
#include "../lanada/param.h"

#if RPL_LIFETIME_MAX_MODE
static void reset(rpl_dag_t *);
static void neighbor_link_callback(rpl_parent_t *, int, int);
#if RPL_WITH_DAO_ACK
static void dao_ack_callback(rpl_parent_t *, int);
#endif
static rpl_parent_t *best_parent(rpl_parent_t *, rpl_parent_t *);
static rpl_dag_t *best_dag(rpl_dag_t *, rpl_dag_t *);
static rpl_rank_t calculate_rank(rpl_parent_t *, rpl_rank_t);
static void update_metric_container(rpl_instance_t *);

rpl_of_t rpl_ltmax_of = {
  reset,
  neighbor_link_callback,
#if RPL_WITH_DAO_ACK
  dao_ack_callback,
#endif
  best_parent,
  best_dag,
  calculate_rank,
  update_metric_container,
  1
};

/* Constants for the ETX moving average */
#define ETX_SCALE   100
#define ETX_ALPHA   90

/* Reject parents that have a higher link metric than the following. */
#define MAX_LINK_METRIC			10

/* Reject parents that have a higher path cost than the following. */
#define MAX_PATH_COST			100

	/*
	 * The rank must differ more than 1/PARENT_SWITCH_THRESHOLD_DIV in order
 * to switch preferred parent.
 */
#define PARENT_SWITCH_THRESHOLD_DIV	2

typedef uint16_t rpl_path_metric_t;

static rpl_path_metric_t
calculate_path_metric(rpl_parent_t *p)
{
  uip_ds6_nbr_t *nbr;
  rpl_path_metric_t ret_metric;
//  rpl_rank_t rank_rate;
  if(p == NULL) {
    return MAX_PATH_COST * RPL_DAG_MC_ETX_DIVISOR;
  }
  nbr = rpl_get_nbr(p);
  if(nbr == NULL) {
    return MAX_PATH_COST * RPL_DAG_MC_ETX_DIVISOR;
  }
#if RPL_DAG_MC == RPL_DAG_MC_NONE
  {
#if RPL_LIFETIME_MAX_MODE
//	  PRINTF("rank, base_rank %d %d\n",p->rank,rpl_get_any_dag()->base_rank);
//	  rank_rate = p->rank - rpl_get_any_dag()->base_rank < 0 ? 0 : p->rank - rpl_get_any_dag()->base_rank;
//	  PRINTF("weight: %d rank_rate: %d\n",p->parent_sum_weight * RPL_DAG_MC_ETX_DIVISOR, rank_rate);
//	  ret_metric = p->parent_sum_weight * RPL_DAG_MC_ETX_DIVISOR + ALPHA * rank_rate;

	  ret_metric = p->rank + (p->parent_sum_weight + p->parent_weight) * RPL_DAG_MC_ETX_DIVISOR;
#else
	  ret_metric = p->rank + (uint16_t)nbr->link_metric;
#endif
	  PRINTF("ip:%d %c weight:%d rank:%d ret_metric:%d %d\n",nbr->ipaddr.u8[15], nbr->ipaddr.u8[8]==0x82 ? 'L' : 'S',
			  p->parent_sum_weight * RPL_DAG_MC_ETX_DIVISOR, p->rank, ret_metric, rpl_get_any_dag()->preferred_parent == p);
	  return ret_metric;
  }
#elif RPL_DAG_MC == RPL_DAG_MC_ETX
  return p->mc.obj.etx + (uint16_t)nbr->link_metric;
#elif RPL_DAG_MC == RPL_DAG_MC_ENERGY
  return p->mc.obj.energy.energy_est + (uint16_t)nbr->link_metric;
#else
#error "Unsupported RPL_DAG_MC configured. See rpl.h."
#endif /* RPL_DAG_MC */
}

static void
reset(rpl_dag_t *dag)
{
  PRINTF("RPL_LTMAX_OF: Reset MRHOF\n");
}

#if RPL_WITH_DAO_ACK
static void
dao_ack_callback(rpl_parent_t *p, int status)
{
  if(status == RPL_DAO_ACK_UNABLE_TO_ADD_ROUTE_AT_ROOT) {
    return;
  }
  /* here we need to handle failed DAO's and other stuff */
  PRINTF("RPL_LTMAX_OF: LTMAX_OF - DAO ACK received with status: %d\n", status);
  if(status >= RPL_DAO_ACK_UNABLE_TO_ACCEPT) {
    /* punish the ETX as if this was 10 packets lost */
    neighbor_link_callback(p, MAC_TX_OK, 10);
  } else if(status == RPL_DAO_ACK_TIMEOUT) { /* timeout = no ack */
    /* punish the total lack of ACK with a similar punishment */
    neighbor_link_callback(p, MAC_TX_OK, 10);
  }
}
#endif /* RPL_WITH_DAO_ACK */

static void
neighbor_link_callback(rpl_parent_t *p, int status, int numtx)
{
  uint16_t recorded_ett = 0;
  uint16_t packet_ett = numtx * RPL_DAG_MC_ETX_DIVISOR;
  uint16_t new_ett;
  uip_ds6_nbr_t *nbr = NULL;
#if DUAL_RADIO
  uint8_t is_longrange;
#endif

  nbr = rpl_get_nbr(p);
  if(nbr == NULL) {
    /* No neighbor for this parent - something bad has occurred */
    return;
  }

  recorded_ett = nbr->link_metric;

  /* Do not penalize the ETX when collisions or transmission errors occur. */
  if(status == MAC_TX_OK || status == MAC_TX_NOACK) {
/*    if(status == MAC_TX_NOACK) {
        packet_ett = MAX_LINK_METRIC * RPL_DAG_MC_ETX_DIVISOR;
    }*/
    if(p->flags & RPL_PARENT_FLAG_LINK_METRIC_VALID) {
      /* We already have a valid link metric, use weighted moving average to update it */
      new_ett = ((uint32_t)recorded_ett * ETX_ALPHA +
                 (uint32_t)packet_ett * (ETX_SCALE - ETX_ALPHA)) / ETX_SCALE;
    } else {
      /* We don't have a valid link metric, set it to the current packet's ETX */
      new_ett = packet_ett;
      /* Set link metric as valid */
      p->flags |= RPL_PARENT_FLAG_LINK_METRIC_VALID;
    }

    PRINTF("RPL_LTMAX_OF: ETT changed from %u to %u (packet ETT = %u)\n",
        (unsigned)(recorded_ett / RPL_DAG_MC_ETX_DIVISOR),
        (unsigned)(new_ett  / RPL_DAG_MC_ETX_DIVISOR),
        (unsigned)(packet_ett / RPL_DAG_MC_ETX_DIVISOR));
    /* update the link metric for this nbr */
    nbr->link_metric = new_ett;
	PRINTF("LTMAX_OF link_metric %d\n",nbr->link_metric);
#if RPL_LIFETIME_MAX_MODE
#if DUAL_RADIO
    if(radio_received_is_longrange() == LONG_RADIO)
    {
    	is_longrange = 1;
    }
    else
    {
    	is_longrange = 0;
    }
#if RPL_ETX_WEIGHT
	if(nbr->link_metric == 0)
	{
		p->parent_weight = 1 * (is_longrange ? LONG_WEIGHT_RATIO : 1);
	}
	else
	{
		p->parent_weight = nbr->link_metric/RPL_DAG_MC_ETX_DIVISOR * (is_longrange ? LONG_WEIGHT_RATIO : 1); // Tx cost using DIO_ACK
	}
#else
	p->parent_weight = 1 * (is_longrange ? LONG_WEIGHT_RATIO : 1);

#endif

	PRINTF("LTMAX_OF %d parent_weight %d\n",is_longrange,p->parent_weight);
#else
#if RPL_ETX_WEIGHT
	if(nbr->link_metric == 0)
	{
		p->parent_weight = 1 * SHORT_RX_COST;
	}
	else
	{
		p->parent_weight = nbr->link_metric/RPL_DAG_MC_ETX_DIVISOR * SHORT_RX_COST; // Tx cost using DIO_ACK
	}
#else
	p->parent_weight = 1 * SHORT_RX_COST;

#endif

#endif
  }
#endif
}

static rpl_rank_t
calculate_rank(rpl_parent_t *p, rpl_rank_t base_rank)
{
  rpl_rank_t new_rank;
  rpl_rank_t rank_increase;
  uip_ds6_nbr_t *nbr;

  if(p == NULL || (nbr = rpl_get_nbr(p)) == NULL) {
    if(base_rank == 0) {
    	PRINTF("null inf rank\n");
      return INFINITE_RANK;
    }
    rank_increase = RPL_INIT_LINK_METRIC * RPL_DAG_MC_ETX_DIVISOR;
  } else {
	  if(p->parent_sum_weight == 0)
	  {
		  rank_increase = (nbr->ipaddr.u8[8] == 0x82 ? LONG_WEIGHT_RATIO : 1) * RPL_DAG_MC_ETX_DIVISOR;
	  }
	  else
	  {
		  rank_increase = p->parent_sum_weight * RPL_DAG_MC_ETX_DIVISOR;
	  }
    if(base_rank == 0) {
      base_rank = p->rank;
    }
  }

  if(INFINITE_RANK - base_rank < rank_increase) {
    /* Reached the maximum rank. */
	  PRINTF("reach max rank\n");
    new_rank = INFINITE_RANK;
  } else {
   /* Calculate the rank based on the new rank information from DIO or
      stored otherwise. */
    //	PRINTF("RPL_JKOF: P's rem_energy %d\n",p->rem_energy); // need to check NULL
    new_rank = base_rank + rank_increase;
  }
 	/* JOONKI */
	PRINTF("RPL_LTMAX_OF: rank calculation with parent");
	PRINTLLADDR(uip_ds6_nbr_get_ll(nbr));
	PRINTF("\n");
	PRINTF("RPL_LTMAX_OF: My new_rank is %d\n",new_rank);
	
  return new_rank;
}

static rpl_dag_t *
best_dag(rpl_dag_t *d1, rpl_dag_t *d2)
{
  if(d1->grounded != d2->grounded) {
    return d1->grounded ? d1 : d2;
  }

  if(d1->preference != d2->preference) {
    return d1->preference > d2->preference ? d1 : d2;
  }

  return d1->rank < d2->rank ? d1 : d2;
}

static rpl_parent_t *
best_parent(rpl_parent_t *p1, rpl_parent_t *p2)
{
  PRINTF("RPL_LTMAX_OF: Best parent\n");
  rpl_dag_t *dag;
  rpl_path_metric_t min_diff;
  rpl_path_metric_t p1_metric;
  rpl_path_metric_t p2_metric;
#if RPL_LIFETIME_MAX_MODE
  uip_ds6_nbr_t *nbr1 = NULL;
  uip_ds6_nbr_t *nbr2 = NULL;
  nbr1 = rpl_get_nbr(p1);
  nbr2 = rpl_get_nbr(p2);
#endif
#if DUAL_RADIO
  uint8_t is_longrange1;
  uint8_t is_longrange2;
#endif

  dag = p1->dag; /* Both parents are in the same DAG. */

  min_diff = RPL_DAG_MC_ETX_DIVISOR /
             PARENT_SWITCH_THRESHOLD_DIV;

  p1_metric = calculate_path_metric(p1);
  p2_metric = calculate_path_metric(p2);
#if RPL_LIFETIME_MAX_MODE
#if DUAL_RADIO
  if(p1 == dag->preferred_parent)
  {
//	  is_longrange1 = nbr1->ipaddr.u8[8]==0x82;
//	  p1_metric += (is_longrange1 ? LONG_WEIGHT_RATIO : 1) * RPL_DAG_MC_ETX_DIVISOR;
	  if(p1->parent_sum_weight != 0 && p1_metric - p1->parent_weight * RPL_DAG_MC_ETX_DIVISOR > 0)
	  {
		  p1_metric -= p1->parent_weight * RPL_DAG_MC_ETX_DIVISOR;
	  }
  }
  else if(p2 == dag->preferred_parent)
  {
//	  is_longrange2 = nbr2->ipaddr.u8[8]==0x82;
//	  p2_metric += (is_longrange2 ? LONG_WEIGHT_RATIO : 1) * RPL_DAG_MC_ETX_DIVISOR;
	  if(p2->parent_sum_weight != 0 && p2_metric - p2->parent_weight * RPL_DAG_MC_ETX_DIVISOR > 0)
	  {
		  p2_metric -= p2->parent_weight * RPL_DAG_MC_ETX_DIVISOR;
	  }
  }
#else	/* DUAL_RADIO */
  if(p1 != dag->preferred_parent)
  {
	  p1_metric += RPL_DAG_MC_ETX_DIVISOR;
  }
  if(p2 != dag->preferred_parent)
  {
	  p2_metric += RPL_DAG_MC_ETX_DIVISOR;
  }
#endif	/* DUAL_RADIO */

#endif	/* RPL_LIFETIME_MAX_MODE */

//  printf("Comparing %d %c p1: %d\n", nbr1->ipaddr.u8[15], nbr1->ipaddr.u8[8]==0x82 ? 'L' : 'S', p1_metric);
//  printf("Comparing %d %c p2: %d\n", nbr2->ipaddr.u8[15], nbr2->ipaddr.u8[8]==0x82 ? 'L' : 'S', p2_metric);
#if OF_MWHOF
  if(p1 == dag->preferred_parent || p2 == dag->preferred_parent)
  {
	  if(p1_metric < p2_metric + min_diff &&
			  p1_metric > p2_metric - min_diff)
	  {
		  return dag->preferred_parent;
	  }
  }
  return p1_metric <= p2_metric ? p1 : p2;

#else
  if(p1_metric == p2_metric)
  {
	  if(p1->rank < p2->rank)
	  {
		  return p1;
	  }
	  else if(p1->rank > p2->rank)
	  {
		  return p2;
	  }
	  if(p1 == dag->preferred_parent || p2 == dag->preferred_parent)
	  {
		  return dag->preferred_parent;
	  }
//	  return p1->rank <= p2->rank ? p1 : p2;
  }
  return p1_metric <= p2_metric ? p1 : p2;
#endif
}

#if RPL_DAG_MC == RPL_DAG_MC_NONE
static void
update_metric_container(rpl_instance_t *instance)
{
  instance->mc.type = RPL_DAG_MC;
}
#else
static void
update_metric_container(rpl_instance_t *instance)
{
  rpl_path_metric_t path_metric;
  rpl_dag_t *dag;
#if RPL_DAG_MC == RPL_DAG_MC_ENERGY
  uint8_t type;
#endif

  instance->mc.type = RPL_DAG_MC;
  instance->mc.flags = RPL_DAG_MC_FLAG_P;
  instance->mc.aggr = RPL_DAG_MC_AGGR_ADDITIVE;
  instance->mc.prec = 0;

  dag = instance->current_dag;

  if (!dag->joined) {
    PRINTF("RPL_LTMAX_OF: Cannot update the metric container when not joined\n");
    return;
  }

  if(dag->rank == ROOT_RANK(instance)) {
    path_metric = 0;
  } else {
    path_metric = calculate_path_metric(dag->preferred_parent);
  }

#if RPL_DAG_MC == RPL_DAG_MC_ETX
  instance->mc.length = sizeof(instance->mc.obj.etx);
  instance->mc.obj.etx = path_metric;

  PRINTF("RPL_LTMAX_OF: My path ETX to the root is %u.%u\n",
	instance->mc.obj.etx / RPL_DAG_MC_ETX_DIVISOR,
	(instance->mc.obj.etx % RPL_DAG_MC_ETX_DIVISOR * 100) /
	 RPL_DAG_MC_ETX_DIVISOR);
#elif RPL_DAG_MC == RPL_DAG_MC_ENERGY
  instance->mc.length = sizeof(instance->mc.obj.energy);

  if(dag->rank == ROOT_RANK(instance)) {
    type = RPL_DAG_MC_ENERGY_TYPE_MAINS;
  } else {
    type = RPL_DAG_MC_ENERGY_TYPE_BATTERY;
  }

  instance->mc.obj.energy.flags = type << RPL_DAG_MC_ENERGY_TYPE;
  instance->mc.obj.energy.energy_est = path_metric;
#endif /* RPL_DAG_MC == RPL_DAG_MC_ETX */
}
#endif /* RPL_DAG_MC == RPL_DAG_MC_NONE */
#endif
/** @}*/
