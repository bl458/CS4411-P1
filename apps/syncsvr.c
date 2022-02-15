#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <earth/earth.h>

/* This is the sync server.  It calls sync() every 30 seconds.
 */
int main(){
	printf("SYNC SERVER (periodically synchronizes file system caches): pid=%u\n\r", sys_getpid());

	for (;;) {
		sleep(30);
		file_sync(GRASS_ENV->servers[GPID_FILE], (unsigned int) -1);
	}
}
