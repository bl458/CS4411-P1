#include <stdio.h>
#include <string.h>
#include <egos/spawn.h>

int main(int argc, char **argv){
	printf("\n\rFlushing cache\n\r");
	fflushsync(NULL);
	spawn_shutdown(GRASS_ENV->servers[GPID_SPAWN]);
	return 0;
}
