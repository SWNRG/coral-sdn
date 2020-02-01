#include "contiki-conf.h"

#if DUAL_RADIO

#include "cc1200.h"
#include "cc2420.h"
#include "net/rpl/rpl-private.h"

#ifndef SHORT_RADIO
#define SHORT_RADIO 2
#endif

#ifndef LONG_RADIO
#define LONG_RADIO 1
#endif

#ifndef BOTH_RADIO
#define BOTH_RADIO 0
#endif

#ifndef BOTH_RADIO
#define BOTH_RADIO 0
#endif

#ifndef DUAL_DUTY_RATIO
#define DUAL_DUTY_RATIO 2
#endif


/* Radio drivers */
extern struct radio_driver NETSTACK_CONF_RADIO;
extern struct radio_driver NETSTACK_RADIO;

/* Functions */
int dual_radio_switch(int radio);
int dual_radio_change(void);
int dual_radio_received(int radio);
int radio_received_is_longrange(void);
int sending_in_LR(void);

int dual_radio_turn_on(char targetRadio);
int dual_radio_turn_off(char targetRadio);

/* Process */
int dio_broadcast(rpl_instance_t* instance);
int dis_broadcast(void);
#if RPL_LIFETIME_MAX_MODE
int dio_ack_broadcast(rpl_instance_t * instance);
#endif

/* Global variable for Dual RPL */
int long_range_radio;
int radio_received;
char dual_duty_cycle_count;
int transmission_count;

uint8_t packet_forwarding;
#endif
