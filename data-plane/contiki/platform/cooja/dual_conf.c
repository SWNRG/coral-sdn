#include "dev/cooja-radio.h"


#include "dual_conf.h"
#include "../lanada/param.h"
#define DEBUG_DUAL	0
#if DEBUG_DUAL
#include <stdio.h>
#include "net/rpl/rpl-icmp6.h"
#include "contiki.h"

#define RADIO(...) printf(__VA_ARGS__)
#else
#define RADIO(...) 
#endif
PROCESS(dual_dio_broadcast, "dio_broadcast");
PROCESS(dual_dis_broadcast, "dis_broadcast");
#if RPL_LIFETIME_MAX_MODE_DIO_ACK
PROCESS(dual_dio_ack_broadcast, "dio_ack_broadcast");
#endif
#if LSA_R
PROCESS(dual_LSA_converge_broadcast, "LSA_converge_broadcast");
#endif

int long_range_radio = 0;
int radio_received = SHORT_RADIO;
static rpl_instance_t *temp_instance;
static uint8_t temp_lr_child;

int dual_radio_switch(int radio)
{
	if (radio == LONG_RADIO)
	{
		long_range_radio = 1;
		LongRangeTransmit = 1;
		// RADIO("LongRangeTransmit = %d\n",LongRangeTransmit);
		// RADIO("$$$$$$$$$$$$$$$$$$  Using long-range radio\n");
	}
	else if (radio == SHORT_RADIO)
	{
		long_range_radio = 0;
		LongRangeTransmit = 0; 
		// RADIO("LongRangeTransmit = %d\n",LongRangeTransmit);
		// RADIO("$$$$$$$$$$$$$$$$$$  Using short-range radio\n");
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
		RADIO("$$$$$$$$$$$$$$$$$$  INTERRUPT!! long-range radio\n");
//		RADIO("HwOnLR: %d, HwOn: %d\n", simRadioHWOnLR, simRadioHWOn);
		return 1;
	}
	else if(radio == SHORT_RADIO)
	{
		radio_received = SHORT_RADIO;	
		RADIO("$$$$$$$$$$$$$$$$$$  INTERRUPT!! short-range radio\n");
//		RADIO("HwOnLR: %d, HwOn: %d\n", simRadioHWOnLR, simRadioHWOn);
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

int dual_radio_turn_on(char targetRadio)
{
	simRadioTarget = targetRadio;
	NETSTACK_RADIO.on();
	return 1;
}

int dual_radio_turn_off(char targetRadio)
{
	simRadioTarget = targetRadio;
	NETSTACK_RADIO.off();
	return 1;
}

PROCESS_THREAD(dual_dio_broadcast, ev, data)
{
	static struct etimer et;
	static uint8_t long_duty_on_local = 1;
	static uint8_t short_duty_on_local = 1;

#if DUAL_ROUTING_CONVERGE
	long_duty_on_local = long_duty_on;
	short_duty_on_local = short_duty_on;
#endif

	PROCESS_BEGIN();
//	dual_radio_switch(SHORT_RADIO);
	dual_radio_switch(LONG_RADIO);
	if (long_duty_on_local == 1) {
		dio_output(temp_instance, NULL);
	}
	etimer_set(&et, 1);

	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	RADIO("############################################### DIO_BROADCAST: Process stopped for a while ####################\n");
//	dual_radio_switch(LONG_RADIO);
		dual_radio_switch(SHORT_RADIO);
	if (short_duty_on_local == 1) {
		dio_output(temp_instance, NULL);
	}
	PROCESS_END();
}

PROCESS_THREAD(dual_dis_broadcast, ev, data)
{
	static struct etimer et;

	static uint8_t long_duty_on_local = 1;
	static uint8_t short_duty_on_local = 1;

#if DUAL_ROUTING_CONVERGE
	long_duty_on_local = long_duty_on;
	short_duty_on_local = short_duty_on;
#endif

	PROCESS_BEGIN();
//	dual_radio_switch(SHORT_RADIO);
	dual_radio_switch(LONG_RADIO);
	
	if (long_duty_on_local == 1) {
		dis_output(NULL);
	}
	etimer_set(&et, 1);
	
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	RADIO("##############################################  DIS_BROADCAST: Process stopped for a while ####################\n");
//	dual_radio_switch(LONG_RADIO);
	dual_radio_switch(SHORT_RADIO);
	
	if (short_duty_on_local == 1) {
		dis_output(NULL);
	}
	
	PROCESS_END();
}
	
#if RPL_LIFETIME_MAX_MODE_DIO_ACK
PROCESS_THREAD(dual_dio_ack_broadcast, ev, data)
{
	static struct etimer et;
	static uint8_t long_duty_on_local = 1;
	static uint8_t short_duty_on_local = 1;

#if DUAL_ROUTING_CONVERGE
	long_duty_on_local = long_duty_on;
	short_duty_on_local = short_duty_on;
#endif

	PROCESS_BEGIN();
	dual_radio_switch(LONG_RADIO);

	if (long_duty_on_local == 1) {
		dio_ack_output(temp_instance, NULL);
	}
	etimer_set(&et, 1);

	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	RADIO("############################################### DIO_ACK_BROADCAST: Process stopped for a while ####################\n");
	dual_radio_switch(SHORT_RADIO);

	if (short_duty_on_local == 1) {
		dio_ack_output(temp_instance, NULL);
	}
	PROCESS_END();
}
#endif

#if LSA_R
PROCESS_THREAD(dual_LSA_converge_broadcast, ev, data)
{
	static struct etimer et;
	static uint8_t long_duty_on_local = 1;
	static uint8_t short_duty_on_local = 1;
	static uint8_t temp_LSA_SR_preamble;

#if DUAL_ROUTING_CONVERGE
	long_duty_on_local = long_duty_on;
	short_duty_on_local = short_duty_on;
#endif

	PROCESS_BEGIN();
	/* Always use LR preamble for LSA converge output */	
	temp_LSA_SR_preamble = LSA_SR_preamble;
	LSA_SR_preamble = 0;

	dual_radio_switch(LONG_RADIO);

	if (long_duty_on_local == 1) {
		LSA_converge_output(temp_lr_child);
	}
	etimer_set(&et, 1);

	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	RADIO("############################################### LSA_converge_BROADCAST: Process stopped for a while ####################\n");
	dual_radio_switch(SHORT_RADIO);

	if (short_duty_on_local == 1) {
		LSA_converge_output(temp_lr_child);
	}

	LSA_SR_preamble = temp_LSA_SR_preamble;
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


#if LSA_R
int LSA_converge_broadcast(uint8_t lr_child)
{
	temp_lr_child = lr_child;
	process_start(&dual_LSA_converge_broadcast, NULL);
	return 1;
}
#endif
