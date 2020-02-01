#include "dual_radio.h"
#define DEBUG_DUAL	0

#include "net/rpl/rpl-icmp6.h"
#include "contiki.h"

#if DEBUG_DUAL
#include <stdio.h>
#define RADIO(...) printf(__VA_ARGS__)
#else
#define RADIO(...)
#endif
PROCESS(dual_dio_broadcast, "process_stop");
PROCESS(dual_dis_broadcast, "dis_broadcast");
#if RPL_LIFETIME_MAX_MODE_DIO_ACK
PROCESS(dual_dio_ack_broadcast, "dio_ack_broadcast");
#endif


int long_range_radio = 0;
int radio_received = SHORT_RADIO;
static rpl_instance_t *temp_instance;


int dual_radio_switch(int radio)
{
	if (radio == LONG_RADIO)
	{
		long_range_radio = 1;
		NETSTACK_CONF_RADIO = cc1200_driver;
		NETSTACK_RADIO = cc1200_driver;
	}
	else if (radio == SHORT_RADIO)
	{
		long_range_radio = 0;
		NETSTACK_CONF_RADIO = cc2538_rf_driver;
		NETSTACK_RADIO = cc2538_rf_driver;
	}
	return 1;
}

int dual_radio_change(void)
{
	if (long_range_radio == 1)
		dual_radio_switch(SHORT_RADIO);
	else if (long_range_radio == 0)
		dual_radio_switch(LONG_RADIO);
		return 1;
}

/* Copying from dual_conf.c cooja mote */

int sending_in_LR(void)
{
	if (long_range_radio == 0){
		return SHORT_RADIO;
	}	else if (long_range_radio == 1){
		return LONG_RADIO;
	}
	return 0;
}

int dual_radio_received(int radio)
{
	if (radio == LONG_RADIO)
	{
		radio_received = LONG_RADIO;
		// RADIO("$$$$$$$$$$$$$$$$$$  INTERRUPT!! long-range radio\n");
		return 1;
	}
	else if(radio == SHORT_RADIO)
	{
		radio_received = SHORT_RADIO;
		// RADIO("$$$$$$$$$$$$$$$$$$  INTERRUPT!! short-range radio\n");
		return 1;
	}
	return 0;
}

int radio_received_is_longrange(void)
{
	if (radio_received == LONG_RADIO)
	{
		// RADIO("$$$$$$$$$$$$$$$$$$  LONG_RADIO_RECEIVED\n");
		return LONG_RADIO;
	}
	else
	{
		// RADIO("$$$$$$$$$$$$$$$$$$  SHORT_RADIO_RECEIVED\n");
		return SHORT_RADIO;
	}
}

PROCESS_THREAD(dual_dio_broadcast, ev, data)
{
	static struct etimer et;
	PROCESS_BEGIN();
	dual_radio_switch(LONG_RADIO);
	dio_output(temp_instance, NULL);
	etimer_set(&et, 1);

	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	RADIO("############################################### DIO_BROADCAST: Process stopped for a while ####################\n");
	dual_radio_switch(SHORT_RADIO);
	dio_output(temp_instance, NULL);
	PROCESS_END();
}

PROCESS_THREAD(dual_dis_broadcast, ev, data)
{
	static struct etimer et;
	PROCESS_BEGIN();
	dual_radio_switch(LONG_RADIO);
	dis_output(NULL);
	etimer_set(&et, 1);

	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	RADIO("##############################################  DIS_BROADCAST: Process stopped for a while ####################\n");
	dual_radio_switch(SHORT_RADIO);
	dis_output(NULL);

	PROCESS_END();
}
	
#if RPL_LIFETIME_MAX_MODE_DIO_ACK
PROCESS_THREAD(dual_dio_ack_broadcast, ev, data)
{
	static struct etimer et;
	PROCESS_BEGIN();
	dual_radio_switch(LONG_RADIO);
	dio_ack_output(temp_instance, NULL);
	etimer_set(&et, 1);

	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	RADIO("############################################### DIO_ACK_BROADCAST: Process stopped for a while ####################\n");
	dual_radio_switch(SHORT_RADIO);
	dio_ack_output(temp_instance, NULL);
	PROCESS_END();
}
#endif


int dio_broadcast(rpl_instance_t * instance)
{
	temp_instance = instance;
	process_start(&dual_dio_broadcast, NULL);
	return 1;
}

int dis_broadcast(void)
{
	process_start(&dual_dis_broadcast, NULL);
	return 1;
}

#if RPL_LIFETIME_MAX_MODE_DIO_ACK
int dio_ack_broadcast(rpl_instance_t * instance)
{
	temp_instance = instance;
	process_start(&dual_dio_ack_broadcast, NULL);
	return 1;
}
#endif
