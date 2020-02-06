/* Log levels */
#include "log_message.h"
#include "lanada/param.h"
#include "sys/residual.h"

#ifdef COOJA
#include "net/linkaddr.h"
FILE *log_fp;
#else	/* COOJA */
int log_file;
#endif	/* COOJA */


void log_initialization(void){
	cxmac_collision_count = 0;
	cxmac_transmission_count = 0;
	csma_transmission_count = 0;
	control_message_count = 0;
	data_message_count = 0;
	data_fwd_count = 0;
	dio_count = 0;
	dis_count = 0;
	dao_count = 0;
	dao_fwd_count = 0;
	dao_ack_count = 0;
	dao_ack_fwd_count = 0;
	LSA_count = 0;
	dio_ack_count = 0;
	icmp_count = 0;
	tcp_output_count = 0;

#if SIMULATION_SETTING
	printf("\nDUAL_RADIO: %d ADDR_MAP: %d RPL_LIFETIME_MAX_MODE: %d LONG_WEIGHT_RATIO: %d STROBE_CNT_MODE: %d DUAL_ROUTING_CONVERGE: %d LSA_MAC: %d\n", \
			DUAL_RADIO, ADDR_MAP, RPL_LIFETIME_MAX_MODE, \
			LONG_WEIGHT_RATIO, STROBE_CNT_MODE, DUAL_ROUTING_CONVERGE, LSA_MAC);
#if LSA_R
// Tryf	printf("LSA_R: %d\nMAX_LSA_RETRANSMISSION: %d\nLSA_CONVERGE_TIME: %d\nLSA_MESSAGE_TIME: %d\nLSA_BROADCAST_TIME: %d\n", \
			LSA_R, MAX_LSA_RETRANSMISSION, LSA_CONVERGE_TIME, LSA_MESSAGE_TIME, LSA_BROADCAST_TIME);
#endif 
// Tryf	printf("DETERMINED_ROUTING_TREE: %d\nRESIDUAL_ENERGY_MAX: %d\n", \
			DETERMINED_ROUTING_TREE, RESIDUAL_ENERGY_MAX);
#ifdef COOJA
// Tryf	printf("DISSIPATION_RATE: %d, %d, %d, %d, %d, %d, %d\n",DISSIPATION_RATE[0],DISSIPATION_RATE[1],\
			DISSIPATION_RATE[2],DISSIPATION_RATE[3],DISSIPATION_RATE[4],DISSIPATION_RATE[5],DISSIPATION_RATE[6]);
#endif
#endif
	

#ifdef COOJA
	char filename[100];
	sprintf(filename, "/home/user/Desktop/Debug_log/log_message%d.txt",linkaddr_node_addr.u8[1]);
	// Tryf printf("\nOpening the file for cooja\n\n");
	log_fp = fopen(filename, "w");
#else	/* COOJA */
	printf("\nOpening the file for z1/firefly\n\n");
	log_file = cfs_open("log_message", CFS_WRITE);
#endif /* COOJA */
}

void log_finisher(void){
#ifdef COOJA
	printf("\nClosing the file for cooja\n\n");
	fclose(log_fp);
#else
	printf("\nClosing the file for z1/firefly\n\n");
	cfs_close(log_file);
#endif
}
