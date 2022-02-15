#include <egos/syscall.h>
#include <egos/spawn.h>
#include <stdio.h>

void _start(void){
	extern int main(int argc, char **argv, char **envp);

	stdio_finit();
	
	/* Deal with bug in tcc by copying pointer to GRASS_ENV in local var.
	 */
	struct grass_env *ge = GRASS_ENV;
	char *name = ge->argc > 0 ? ge->argv[0] : "<no args>";
	spawn_set_descr(GRASS_ENV->servers[GPID_SPAWN], sys_getpid(), name);
	int status = main(GRASS_ENV->argc, GRASS_ENV->argv, GRASS_ENV->envp);
	sys_exit(status);
}

int _print_output(const char *buf, unsigned int size){
	return fwrite(buf, 1, size, stdout);
}
