/* JOONKI */

#include "net/ip/uip.h"
#include "net/rpl/rpl-private.h"

#if DUAL_RADIO
#if ADDR_MAP

#include "net/nbr-table.h"

extern uip_ds6_lr_addrmap_t ds6_lr_addrmap[NBR_TABLE_MAX_NEIGHBORS];

#endif	/* ADDR_MAP */	
#endif	/* DUAL_RADIO */

void dis_output(uip_ipaddr_t *addr);
void dio_output(rpl_instance_t *instance, uip_ipaddr_t *uc_addr);
