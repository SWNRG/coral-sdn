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
 *         Rime route table
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

/**
 * \addtogroup rimeroute
 * @{
 */

#include <stdio.h>

#include "lib/list.h"
#include "lib/memb.h"
#include "sys/ctimer.h"
#include "route.h"
#include "contiki-conf.h"

#ifdef ROUTE_CONF_ENTRIES
#define NUM_RT_ENTRIES ROUTE_CONF_ENTRIES
#else /* ROUTE_CONF_ENTRIES */
#define NUM_RT_ENTRIES 8
#endif /* ROUTE_CONF_ENTRIES */

#ifdef ROUTE_CONF_DECAY_THRESHOLD
#define DECAY_THRESHOLD ROUTE_CONF_DECAY_THRESHOLD
#else /* ROUTE_CONF_DECAY_THRESHOLD */
#define DECAY_THRESHOLD 8
#endif /* ROUTE_CONF_DECAY_THRESHOLD */

#ifdef ROUTE_CONF_DEFAULT_LIFETIME
#define DEFAULT_LIFETIME ROUTE_CONF_DEFAULT_LIFETIME
#else /* ROUTE_CONF_DEFAULT_LIFETIME */
#define DEFAULT_LIFETIME 60
#endif /* ROUTE_CONF_DEFAULT_LIFETIME */

/*
 * List of route entries.
 */
LIST(route_table);
MEMB(route_mem, struct route_entry, NUM_RT_ENTRIES);

static struct ctimer t;

static int max_time = DEFAULT_LIFETIME;

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif


/*---------------------------------------------------------------------------*/
// INCREASE time by one for all ROUTE TABLE entries - REMOVE if time above MAX_TIME
static void
periodic(void *ptr)
{
  struct route_entry *e;

  for(e = list_head(route_table); e != NULL; e = list_item_next(e)) {
    e->time++;
    if(e->time >= max_time) {
      PRINTF("route periodic: removing entry to %d.%d with nexthop %d.%d and cost %d\n",
	     e->dest.u8[0], e->dest.u8[1],
	     e->nexthop.u8[0], e->nexthop.u8[1],
	     e->cost);
      list_remove(route_table, e);
      memb_free(&route_mem, e);
    }
  }

  ctimer_set(&t, CLOCK_SECOND, periodic, NULL);
}
/*---------------------------------------------------------------------------*/
// INITIALIZE
void
route_init(void)
{
  list_init(route_table);
  memb_init(&route_mem);

 // ctimer_set(&t, CLOCK_SECOND, periodic, NULL);  // Tryf: Comment out periodic increase of time
}
/*---------------------------------------------------------------------------*/
// ADD TO ROUTE TABLE
int
route_add(const linkaddr_t *dest, const linkaddr_t *nexthop,
	  uint8_t cost, uint8_t seqno)
{
  struct route_entry *e, *oldest = NULL;

  /* Avoid inserting duplicate entries. */
  e = route_lookup(dest);
  if(e != NULL) {
    // list_remove(route_table, e); 
    route_remove(e);  // Tryf correcting the above was wrong
    PRINTF("REMOVE FROM route_table dest:%u.%u nexthop\n",dest->u8[0],dest->u8[1],nexthop->u8[0],nexthop->u8[1]);
  } 

/* Tryf eavy change if problem check a backup version
    if(e == NULL) {
      for(e = list_head(route_table); e != NULL; e = list_item_next(e)) {
        if(oldest == NULL || e->time >= oldest->time) {
          oldest = e;
        }
      }
      e = oldest;
      list_remove(route_table, e);
      printf("route_add: removing entry to %d.%d with nexthop %d.%d and cost %d\n",
	     e->dest.u8[0], e->dest.u8[1],
	     e->nexthop.u8[0], e->nexthop.u8[1],
	     e->cost);
    }
  }*/

  /* Allocate a new entry  */
  e = memb_alloc(&route_mem);

  linkaddr_copy(&e->dest, dest);
  linkaddr_copy(&e->nexthop, nexthop);
  e->cost = cost;
  e->seqno = seqno;
  e->time = 0;
  e->decay = 0;
  e->time_last_decay=0;

  /* New entry goes first. */
  list_push(route_table, e);

  PRINTF("route_add: new entry to %d.%d with nexthop %d.%d and cost %d Route Table Size:%d\n",
	 e->dest.u8[0], e->dest.u8[1],
	 e->nexthop.u8[0], e->nexthop.u8[1],
	 e->cost,route_num());
  
  return 0;
}
/*---------------------------------------------------------------------------*/
// SEARCH ROUTE to find the best route with the (lowest cost --this was removed by Tryf)
struct route_entry *
route_lookup(const linkaddr_t *dest)
{
  struct route_entry *e;
  uint8_t lowest_cost;
  struct route_entry *best_entry;

  lowest_cost = -1;
  best_entry = NULL;

  /* Find the route with the lowest cost. */
  for(e = list_head(route_table); e != NULL; e = list_item_next(e)) {
      /*  printf("route_lookup: comparing %d.%d.%d.%d with %d.%d.%d.%d\n",
	   uip_ipaddr_to_quad(dest), uip_ipaddr_to_quad(&e->dest));*/

    //printf("LOOKUP CHECK %d: dest:%d.%d  e->dest:%d.%d\n",i++, dest->u8[0], dest->u8[1], e->dest.u8[0],e->dest.u8[1]);

    if(linkaddr_cmp(dest, &e->dest)) {
     // if(e->cost < lowest_cost) {   Tryf removed 25/7/2019
	      best_entry = e;
	      lowest_cost = e->cost;
     // }
    }
  }
  return best_entry;
}
/*---------------------------------------------------------------------------*/
// RESET OLD TIMER and DECAY FOR GOOD ROUTES
void
route_refresh(struct route_entry *e)
{
  if(e != NULL) {
    /* Refresh age of route so that used routes do not get thrown
       out. */
    e->time = 0;
    e->decay = 0;
    
    PRINTF("route_refresh: time %d last %d decay %d for entry to %d.%d with nexthop %d.%d and cost %d\n",
           e->time, e->time_last_decay, e->decay,
           e->dest.u8[0], e->dest.u8[1],
           e->nexthop.u8[0], e->nexthop.u8[1],
           e->cost);

  }
}
/*---------------------------------------------------------------------------*/
void
route_decay(struct route_entry *e)
{
  /* If routes are not refreshed, they decay over time. This function
     is called to decay a route. The route can only be decayed once
     per second. */
  PRINTF("route_decay: time %d last %d decay %d for entry to %d.%d with nexthop %d.%d and cost %d\n",
	 e->time, e->time_last_decay, e->decay,
	 e->dest.u8[0], e->dest.u8[1],
	 e->nexthop.u8[0], e->nexthop.u8[1],
	 e->cost);
  
  if(e->time != e->time_last_decay) {
    /* Do not decay a route too often - not more than once per second. */
    e->time_last_decay = e->time;
    e->decay++;

    if(e->decay >= DECAY_THRESHOLD) {
      PRINTF("route_decay: removing entry to %d.%d with nexthop %d.%d and cost %d\n",
	     e->dest.u8[0], e->dest.u8[1],
	     e->nexthop.u8[0], e->nexthop.u8[1],
	     e->cost);
      route_remove(e);
    }
  }
}
/*---------------------------------------------------------------------------*/
// REMOVE ROUTE
void
route_remove(struct route_entry *e)   
{
  list_remove(route_table, e);
  memb_free(&route_mem, e);
}
/*---------------------------------------------------------------------------*/
// REMOVE ALL ROUTES
void
route_flush_all(void)
{
  struct route_entry *e;
//int i=0;
  while(1) {
    e = list_pop(route_table);
    if(e != NULL) {
     // printf("FLUSHING %d: %u.%u\n",i++, e->dest.u8[0],e->dest.u8[1]);
      memb_free(&route_mem, e);
    } else {
      break;
    }
  }
}
/*---------------------------------------------------------------------------*/
void
route_set_lifetime(int seconds)
{
  max_time = seconds;
}
/*---------------------------------------------------------------------------*/
// RETURN TOTAL NUMBER of ROUTES
int
route_num(void)
{
  struct route_entry *e;
  int i = 0;

  for(e = list_head(route_table); e != NULL; e = list_item_next(e)) {
    i++;
  }
  return i;
}
/*---------------------------------------------------------------------------*/
// PRINT ALL ROUTE TABLE ENTRIES
void
route_print(void)
{
  struct route_entry *e;
  int i = 0;

  for(e = list_head(route_table); e != NULL; e = list_item_next(e)) {
    printf("%d: %d.%d through %d.%d\n",++i, e->dest.u8[0],e->dest.u8[1],e->nexthop.u8[0], e->nexthop.u8[1]);
  }
  if(i==0) printf("Routing Table is empty\n");
}
/*---------------------------------------------------------------------------*/
// RETURN ROUTE IN POSITION num
struct route_entry *
route_get(int num)
{
  struct route_entry *e;
  int i = 0;

  for(e = list_head(route_table); e != NULL; e = list_item_next(e)) {
    if(i == num) {
      return e;
    }
    i++;
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
/** @} */
