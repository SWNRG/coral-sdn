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
 */

#include <stdio.h>
#include <string.h>

#include "contiki.h"

#include "sys/cooja_mt.h"
#include "lib/simEnvChange.h"

#include "net/packetbuf.h"
#include "net/rime/rimestats.h"
#include "net/netstack.h"

#include "dev/radio.h"
#include "dev/cooja-radio.h"



#include "dev/leds.h" // For debug
/* JOONKI */

#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif
#if DUAL_RADIO
#include "../dual_conf.h"
#endif

#define COOJA_RADIO_BUFSIZE PACKETBUF_SIZE
#define CCA_SS_THRESHOLD -95

#define WITH_TURNAROUND 1
#define WITH_SEND_CCA 1

/* COOJA */
#if DUAL_RADIO
#else
#endif

//#if DUAL_RADIO
char simReceivingLR = 0;
char simInDataBufferLR[COOJA_RADIO_BUFSIZE];
int simInSizeLR = 0;
char simOutDataBufferLR[COOJA_RADIO_BUFSIZE];
int simRadioChannelLR = 27;
int simOutSizeLR = 0;
char simRadioHWOnLR = 1;
int simSignalStrengthLR = -100;
int simLastSignalStrengthLR = -100;
char simPowerLR = 100;
int simLQILR = 105;
//#endif

char simReceiving = 0;
char simInDataBuffer[COOJA_RADIO_BUFSIZE];
int simInSize = 0;
char simOutDataBuffer[COOJA_RADIO_BUFSIZE];
int simRadioChannel = 26;
int simOutSize = 0;
char simRadioHWOn = 1;
int simSignalStrength = -100;

/* Not used variable */
int simLastSignalStrength = -100;
char simPower = 100;
int simLQI = 105;

int LongRangeTransmit = 0;
int LongRangeReceiving = 0;
static const void *pending_data;

PROCESS(cooja_radio_process, "cooja radio process");

/* JOONKI */
//extern mac_callback_t * global_sent;
//extern FILE *debugfp;

/*---------------------------------------------------------------------------*/
void
radio_set_channel(int channel)
{
#if DUAL_RADIO
	  simRadioChannel = channel;
	  simRadioChannelLR = channel;
#else
	  simRadioChannel = channel;
#endif
}
/*---------------------------------------------------------------------------*/
void
radio_set_txpower(unsigned char power)
{
  /* 1 - 100: Number indicating output power */

#if DUAL_RADIO
	simPower = power;
	simPowerLR = power;
#else
	simPower = power;
#endif
}
/*---------------------------------------------------------------------------*/
int
radio_signal_strength_last(void)
{
	/* Not used function */
  return simLastSignalStrength;
}
/*---------------------------------------------------------------------------*/
int
radio_signal_strength_current(void)
{
	/* Not used function */
  return simSignalStrength;
}
/*---------------------------------------------------------------------------*/
int
radio_LQI(void)
{
	/* Not used function */
	return simLQI;
}
/*---------------------------------------------------------------------------*/
static int
radio_on(void)
{
#if DUAL_RADIO
	if(simRadioTarget == SHORT_RADIO)
	{
		// printf("on SHORT\n");
		simRadioHWOn = 1;
	}
	else if(simRadioTarget == LONG_RADIO)
	{
		// printf("on LONG\n");
		simRadioHWOnLR = 1;
	}
	else if(simRadioTarget == BOTH_RADIO)
	{
		// printf("on BOTH\n");
		simRadioHWOn = 1;
		simRadioHWOnLR = 1;
	}
#else
	simRadioHWOn = 1;
#endif
//  cooja_mt_yield();
  return 1;
}
/*---------------------------------------------------------------------------*/
static int
radio_off(void)
{
#if DUAL_RADIO
	if(simRadioTarget == SHORT_RADIO)
	{
		// printf("off SHORT\n");
		simRadioHWOn = 0;
	}
	else if(simRadioTarget == LONG_RADIO)
	{
		// printf("off LONG\n");
		simRadioHWOnLR = 0;
	}
	else if(simRadioTarget == BOTH_RADIO)
	{
		// printf("off BOTH\n");
		simRadioHWOn = 0;
		simRadioHWOnLR = 0;
	}
#else
	simRadioHWOn = 0;
#endif
//  cooja_mt_yield();
  return 1;
}
/*---------------------------------------------------------------------------*/
static void
doInterfaceActionsBeforeTick(void)
{
//  printf("beforetick on %d %d size %d %d\n",simRadioHWOn,simRadioHWOnLR,simInSize,simInSizeLR);
//	printf("HWOnLR: %d, HWOn: %d\n",simRadioHWOnLR, simRadioHWOn);
	if(!simRadioHWOn) {
    simInSize = 0;
  }
#if DUAL_RADIO
  if(!simRadioHWOnLR) {
    simInSizeLR = 0;
  }
  if(!simInSize && !simInSizeLR) {
	  return;
  }
#else
  if(!simInSize) {
	  return;
  }
#endif

  if(simReceiving) {
    simLastSignalStrength = simSignalStrength;
    return;
  }
#if DUAL_RADIO
  if(simReceivingLR) {
    simLastSignalStrengthLR = simSignalStrengthLR;
    return;
  }
#endif
//  printf("simInsize\n");
#if DUAL_RADIO
  if(simInSize > 0 || simInSizeLR > 0) {
	  // printf("In the simInsize\n");
	    process_poll(&cooja_radio_process);
  }
#else
  if(simInSize > 0) {
//	   printf("In the simInsize\n");
	    process_poll(&cooja_radio_process);
  }
#endif
//  printf("simInsize2\n");
}
/*---------------------------------------------------------------------------*/
static void
doInterfaceActionsAfterTick(void)
{
}
/*---------------------------------------------------------------------------*/
static int
// radio_read(void *buf, unsigned short bufsize)
radio_read(void *buf, unsigned short bufsize)
{
  // printf("radio read %d %d\n",simInSize, simInSizeLR);
  int tmp = simInSize;

#if DUAL_RADIO
  if(simInSize == 0 && simInSizeLR == 0) {
    return 0;
  }
  if(bufsize < simInSize || bufsize <simInSizeLR ) {
    simInSize = 0; /* rx flush */
    simInSizeLR = 0;
    RIMESTATS_ADD(toolong);
    return 0;
  }
#else
  if(simInSize == 0) {
    return 0;
  }
  if(bufsize < simInSize ) {
    simInSize = 0; /* rx flush */
    RIMESTATS_ADD(toolong);
    return 0;
  }
#endif

#if DUAL_RADIO
			/*JOONKI*/
			if (LongRangeReceiving > 0)	{
				dual_radio_received(LONG_RADIO);
			}	else {
				dual_radio_received(SHORT_RADIO);
			}

#endif

	/*	if (sent != NULL){
		fprintf(debugfp,"INSIDE READ BEFORE memcpy: cooja-radio_driver/radio_read sent: %x\n\n", *sent);
		fflush(debugfp);
	}	*/
#if DUAL_RADIO
	if(radio_received_is_longrange() == LONG_RADIO)	{
		memcpy(buf, simInDataBufferLR, simInSizeLR);
		tmp = simInSizeLR;
		// fprintf(debugfp, "BUFFER address : %x, simInSizeLR : %d, simInSizeSR : %d\n\n",buf,simInSizeLR,simInSize);
		// fflush(debugfp);
		simInSizeLR = 0;
	}	else {
		memcpy(buf, simInDataBuffer, simInSize);
		// fprintf(debugfp, "BUFFER address : %x, simInSizeLR : %d, simInSizeSR : %d\n\n",buf,simInSizeLR,simInSize);
		// fflush(debugfp);
		simInSize = 0;
	}
#else
		memcpy(buf, simInDataBuffer, simInSize);
		simInSize = 0;
#endif
	/* if (sent != NULL){
		fprintf(debugfp,"INSIDE READ AFTER memcpy: cooja-radio_driver/radio_read sent: %x\n\n", *sent);
		fflush(debugfp);
	}	*/
#if DUAL_RADIO
	if(radio_received_is_longrange() == LONG_RADIO) {
	  packetbuf_set_attr(PACKETBUF_ATTR_RSSI, simSignalStrengthLR);
	  packetbuf_set_attr(PACKETBUF_ATTR_LINK_QUALITY, simLQILR);
	} else {
		packetbuf_set_attr(PACKETBUF_ATTR_RSSI, simSignalStrength);
		packetbuf_set_attr(PACKETBUF_ATTR_LINK_QUALITY, simLQI);
	}
#else
		packetbuf_set_attr(PACKETBUF_ATTR_RSSI, simSignalStrength);
		packetbuf_set_attr(PACKETBUF_ATTR_LINK_QUALITY, simLQI);
#endif
	/* if (sent != NULL){
		fprintf(debugfp,"INSIDE READ AFTER packetbuf: cooja-radio_driver/radio_read sent: %x\n\n", *sent);
		fflush(debugfp);
	}	*/
  return tmp;
}
/*---------------------------------------------------------------------------*/
static int
channel_clear(void)
{
#if DUAL_RADIO
  if(simRadioTarget == SHORT_RADIO && simSignalStrength > CCA_SS_THRESHOLD) {
    return 0;
  } else if(simRadioTarget == LONG_RADIO && simSignalStrengthLR > CCA_SS_THRESHOLD) {
    return 0;
  }
#else
  if(simSignalStrength > CCA_SS_THRESHOLD) {
	  return 0;
  }
#endif
  return 1;
}
/*---------------------------------------------------------------------------*/
static int
radio_send(const void *payload, unsigned short payload_len)
{
  int radiostate = simRadioHWOn;
#if DUAL_RADIO
  int radiostateLR = simRadioHWOnLR;
  char tmp = simRadioTarget;
#endif
  /* Simulate turnaround time of 2ms for packets, 1ms for acks*/
#if WITH_TURNAROUND
  simProcessRunValue = 1;
  cooja_mt_yield();
  if(payload_len > 3) {
    simProcessRunValue = 1;
    cooja_mt_yield();
  }
#endif /* WITH_TURNAROUND */

#if DUAL_RADIO
  if (sending_in_LR() == LONG_RADIO){
		if(!simRadioHWOnLR)
 	 {
		/* Turn on radio temporarily */
		simRadioHWOnLR = 1;
 	 }
	}	else if (sending_in_LR() == SHORT_RADIO) {
		if(!simRadioHWOn) {
    	/* Turn on radio temporarily */
    	simRadioHWOn = 1;
  	}
	}
	leds_on(LEDS_BLUE);	// For debug
#else
 	if(!simRadioHWOn) {
    /* Turn on radio temporarily */
    simRadioHWOn = 1;
  }
#endif
  if(payload_len > COOJA_RADIO_BUFSIZE) {
    return RADIO_TX_ERR;
  }
  if(payload_len == 0) {
    return RADIO_TX_ERR;
  }
	// PRINTF("COOJA RADIO: Sending packet in cooja driver\n");

#if DUAL_RADIO
/* IN CASE OF LONG RADIO */
	if (sending_in_LR() == LONG_RADIO){
	  if(simOutSizeLR > 0) {
	    return RADIO_TX_ERR;
	  }
		PRINTF("$$$$$$$$$$$$$$$ Sending in LR ------------------------>\n");
		// PRINTF("LongRangeTransmit : %d\n",LongRangeTransmit); 
  	/* Transmit on CCA */
#if WITH_SEND_CCA
		simRadioTarget = LONG_RADIO;
	  if(!channel_clear()) {
  	  return RADIO_TX_COLLISION;
	  }	
#endif /* WITH_SEND_CCA */
	  /* Copy packet data to temporary storage */
	  memcpy(simOutDataBufferLR, payload, payload_len);
		simOutSizeLR = payload_len;
		}	
/* IN CASE OF SHORT RADIO */
	else {	
	  simRadioTarget = SHORT_RADIO;
#endif /* DUAL_RADIO */
		PRINTF("$$$$$$$$$$$$$$$$ Sending in SR ------->\n");
		// PRINTF("LongRangeTransmit : %d\n",LongRangeTransmit);
	  if(simOutSize > 0) {
	    return RADIO_TX_ERR;
	  }
	  /* Transmit on CCA */
#if WITH_SEND_CCA
	  if(!channel_clear()) {
	    return RADIO_TX_COLLISION;
	  }
#endif /* WITH_SEND_CCA */
	  /* Copy packet data to temporary storage */
 	 memcpy(simOutDataBuffer, payload, payload_len);
 	 simOutSize = payload_len;

#if DUAL_RADIO
	}
#endif
  /* Transmit */
#if DUAL_RADIO
  while(simOutSize > 0 || simOutSizeLR > 0) {
    cooja_mt_yield();
  }
#else
	while(simOutSize > 0) {
//		printf("in the simOutSize\n");
		cooja_mt_yield();
  }
#endif

  simRadioHWOn = radiostate;
#if DUAL_RADIO
  simRadioHWOnLR = radiostateLR;
  simRadioTarget = tmp;
	
	leds_off(LEDS_BLUE); // For debug
#endif
  return RADIO_TX_OK;
}
/*---------------------------------------------------------------------------*/
static int
prepare_packet(const void *data, unsigned short len)
{
  pending_data = data;
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
transmit_packet(unsigned short len)
{
  int ret = RADIO_TX_ERR;
  if(pending_data != NULL) {
    ret = radio_send(pending_data, len);
  }
  return ret;
}
/*---------------------------------------------------------------------------*/
static int
receiving_packet(void)
{
#if DUAL_RADIO
	return simReceiving|simReceivingLR;
#else
	return simReceiving;
#endif
}
/*---------------------------------------------------------------------------*/
static int
pending_packet(void)
{
#if DUAL_RADIO
  return (!simReceiving && simInSize > 0)|(!simReceivingLR && simInSizeLR > 0);
#else
  return !simReceiving && simInSize >0;
#endif
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(cooja_radio_process, ev, data)
{
  int len;

  PROCESS_BEGIN();

  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);
    packetbuf_clear();
		// if (simInSize >0 && simInSizeLR > 0)
		//	dual_radio_received(SHORT_RADIO);

    len = radio_read(packetbuf_dataptr(), PACKETBUF_SIZE);
		if(len > 0) {
      packetbuf_set_datalen(len);
   	  NETSTACK_RDC.input();
   	}
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
static int
init(void)
{
  process_start(&cooja_radio_process, NULL);
  return 1;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
get_value(radio_param_t param, radio_value_t *value)
{
  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
set_value(radio_param_t param, radio_value_t value)
{
  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
get_object(radio_param_t param, void *dest, size_t size)
{
  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
set_object(radio_param_t param, const void *src, size_t size)
{
  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
const struct radio_driver cooja_radio_driver =
{
    init,
    prepare_packet,
    transmit_packet,
    radio_send,
    radio_read,
    channel_clear,
    receiving_packet,
    pending_packet,
    radio_on,
    radio_off,
    get_value,
    set_value,
    get_object,
    set_object
};
/*---------------------------------------------------------------------------*/
SIM_INTERFACE(radio_interface,
	      doInterfaceActionsBeforeTick,
	      doInterfaceActionsAfterTick);
