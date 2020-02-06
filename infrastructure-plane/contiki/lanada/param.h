
#define RPL_ENERGY_MODE 0
#define RPL_LIFETIME_MAX_MODE 1	// Child information is saved in each node

/* Distributed weight update problem solutions */
#define MODE_DIO_WEIGHT_UPDATED 0
#define MODE_LAST_PARENT	0 // Tx Last parent info. in dio_ack
#define MODE_PARENT_UPDATE_IN_ROUND 0 // Update parent only when round is synchronized // Not implemented yet

/* OF best parent selection strategy */
#define OF_MWHOF	0 // Minimum Weight Hysteresis Objective Function

/* Metric ratio between weight and rank */
//#define ALPHA 2
/* Weight ratio between long and short*/
#define LONG_WEIGHT_RATIO 2

/* Sink's infinite energy */
#define SINK_INFINITE_ENERGY	1

/* Using strobe cnt, reducing idle listening while Tx preamble */
#define STROBE_CNT_MODE		0

/* Energy log */
#define RPL_ICMP_ENERGY_LOG		0

/* Data aggregation shceme enabled or not */
#define DATA_AGGREGATION 1	// not implemented yet

/* Dual routing converge */
#define DUAL_ROUTING_CONVERGE 	0

/* LSA-MAC, implemeted on cxmac
 * Preamble free short broadcast after long broadcast, dual broadcast is included in LSA-MAC
 * Only long duty cylce, long preamble */
#if DUAL_RADIO
#define LSA_MAC	1
#ifdef ZOLERTIA_Z1
#define LSA_R	0
#else
#define LSA_R	1
#endif
#else	/* DUAL_RADIO */
#define LSA_MAC 0
#define LSA_R 0
#endif /* DUAL_RADIO */

#define SERVER_NODE 1



#if DUAL_ROUTING_CONVERGE
uint8_t long_duty_on;
uint8_t short_duty_on;
#define CONVERGE_TIME	(100ul * CLOCK_SECOND) // Convergence time in second
#endif

#if LSA_R
uint8_t LSA_converge;
uint8_t LSA_SR_preamble;
uint8_t LSA_lr_child;
uint8_t LSA_message_input;
uint8_t LSA_message_flag;
uint8_t LSA_broadcast_count;
#define MAX_LSA_RETRANSMISSION 4
#define LSA_CONVERGE_TIME	(900ul * CLOCK_SECOND) // Convergence time in second
#define LSA_MESSAGE_TIME	(100ul * CLOCK_SECOND) // Convergence time in second
#define LSA_BROADCAST_TIME	(1ul * CLOCK_SECOND) // Convergence time in second

#ifdef RPL_LIFETIME_MAX_MODE
#undef RPL_LIFETIME_MAX_MODE
#endif /* RPL_LIFETIME_MAX_MODE */
#define RPL_LIFETIME_MAX_MODE 1	// Child information is saved in each node

#endif /* LSA_R */

#if RPL_ENERGY_MODE
uint8_t remaining_energy;
uint8_t alpha;
#define LONG_ETX_PENALTY 5

#elif RPL_LIFETIME_MAX_MODE
#define RPL_ETX_WEIGHT 	0
uint8_t my_weight;
uint8_t my_sink_reachability;
uint8_t my_parent_number;
#define DATA_PKT_SIZE 10 // 'B' in theory
#define SHORT_TX_COST 1
#define SHORT_RX_COST 1
#define LONG_TX_COST 9
#define LONG_RX_COST 6
#endif /* RPL_ENERGY_MODE */


#if LSA_MAC
#define SHORT_SLOT_LEN	(RTIMER_ARCH_SECOND / 160 * 2) // Short on time slot length in rtimer
#endif

/*-----------------------------------------------------------------------------------------------*/
#define DETERMINED_ROUTING_TREE	0

#if DETERMINED_ROUTING_TREE
#define MAX_NODE_NUMBER 30

#endif /* ROUTING_TREE */
