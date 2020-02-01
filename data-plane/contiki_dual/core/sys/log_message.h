/* Log levels */
#include <stdio.h>
#include "cfs/cfs.h"

#ifdef COOJA
extern FILE *log_fp;
#else	/* COOJA */
extern int log_file;
#endif	/* COOJA */

#define SIMULATION_SETTING	1
#if SIMULAITON_SETTING
extern const energy_t DISSIPATION_RATE;
#endif

/* This status log takes large size of memory, maybe we should remove this part for z1/firefly */
int cxmac_collision_count, cxmac_transmission_count, csma_transmission_count;
int control_message_count, data_message_count, data_fwd_count;
int dio_count, dis_count, dao_count, dao_fwd_count, dao_ack_count, dao_ack_fwd_count, dio_ack_count, LSA_count;
int icmp_count, tcp_output_count;


void log_initialization(void);
void log_finisher(void);

#define LOG_LEVEL		0

#if LOG_LEVEL == 0
#define LOG_MESSAGE(...) printf(__VA_ARGS__)

#elif	LOG_LEVEL == 1
#ifdef COOJA
#define LOG_MESSAGE(...) do{\
	fprintf(log_fp, __VA_ARGS__);\
	fflush(log_fp);\
}while(0)
#else	/* COOJA */
#define LOG_MESSAGE(...) cfs_write(log_file, __VA_ARGS__, 30)
#endif	/* COOJA */

#elif LOG_LEVEL == 2
#ifdef COOJA
#define LOG_MESSAGE(...) do{\
		printf(__VA_ARGS__);\
		fprintf(log_fp, __VA_ARGS__);\
		fflush(log_fp);\
	}while(0)

#else /* COOJA */
#define LOG_MESSAGE(...) do{\
		printf(__VA_ARGS__);\
		cfs_write(log_file, __VA_ARGS__,30);\
	}while(0)
#endif	/* COOJA */

#elif LOG_LEVEL == 3
#define LOG_MESSAGE(...)
#endif
