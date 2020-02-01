#include "contiki.h"
#include "cfs/cfs.h"
#include <stdio.h>
#include <string.h>

#define MAX_BLOCKSIZE 40

PROCESS(read_log_process, "read_log");
AUTOSTART_PROCESSES(&read_log_process);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(read_log_process, ev, data)
{
	static int fd = 0;
	static int block_size = MAX_BLOCKSIZE;
	char *filename = "sent_log";
	int len;
	int offset = 0;
	char buf[MAX_BLOCKSIZE];

	PROCESS_EXITHANDLER(cfs_close(fd));
	PROCESS_BEGIN();

	fd = cfs_open(filename, CFS_READ);
	cfs_seek(fd, offset, CFS_SEEK_SET);
			
	if(fd < 0) {
		printf("Can't open the log file.\n");
	} 
	else {
		while(1) {
			len = cfs_read(fd, buf, block_size);
			if(len <= 0) {
				cfs_close(fd);
				PROCESS_EXIT();
			}
			printf("%s", buf);
		}
	}
	
	PROCESS_END();
}
