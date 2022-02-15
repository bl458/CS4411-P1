#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <egos/spawn.h>

#define MAX_ARGS	256

bool path_exec(char *exec, int argc, char **argv){
	char *argb;
	unsigned int total;
	gpid_t pid;

	/* Load the arguments and start executable.
	 */
	spawn_load_args(GRASS_ENV, argc, argv, &argb, &total);
	bool r = spawn_fexec(exec, argb, total, true, 0, &pid);
	free(argb);
	if (!r) {
		return false;
	}

	/* Wait for process to finish.
	 */
	for (;;) {
		struct msg_event mev;
		int size = sys_recv(MSG_EVENT, 0, &mev, sizeof(mev), 0, 0);
		if (size != sizeof(mev)) {
			fprintf(stderr, "path_exec: bad event size\n");
			continue;
		}
		if (mev.pid == pid) {
			return mev.status == 0;
		}
		fprintf(stderr, "path_exec: unexpected event\n");
	}
}

// Usage: cc [-c] [x.[co] ...] [-o outfile]
int main(int argc, char **argv){
	bool compile_only = false, convert = false, kernel = false;
	char *output = 0;

	if (argc == 1) {
		printf("Usage: cc [-c] [x.[co] ...] [-o outfile]\n");
		return 0;
	}

	/* See what we need.
	 */
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-c") == 0) {
			compile_only = true;
		}
		else if (strcmp(argv[i], "-k") == 0) {
			kernel = true;
		}
		else if (strcmp(argv[i], "-o") == 0) {
			output = argv[++i];
		}
	}

	int out_argc = 0;
	char **out_argv = calloc(argc + MAX_ARGS, sizeof(char *));
	out_argv[out_argc++] = "tcc";
	out_argv[out_argc++] = "-g";
	out_argv[out_argc++] = "-DTCC";
	out_argv[out_argc++] = "-DGRASS";
	out_argv[out_argc++] = "-Dx86_64";
	out_argv[out_argc++] = "-DTLSF";
	out_argv[out_argc++] = "-I/tcc/include";
	out_argv[out_argc++] = "-I/src/include";
	out_argv[out_argc++] = "-I/src/h";

	if (compile_only) {
		if (output) {
			out_argv[out_argc++] = "-o";
			out_argv[out_argc++] = output;
		}
	}
	else {
		out_argv[out_argc++] = "-o";
		out_argv[out_argc++] = "/tmp/cc.tmp";
		convert = true;
	}

	if (compile_only) {
		out_argv[out_argc++] = "-c";
	}
	else {
		out_argv[out_argc++] = "-Wl,-nostdlib";
		if (kernel) {
			out_argv[out_argc++] = "-Wl,-Ttext=2000000000";
			out_argv[out_argc++] = "-Wl,-section-alignment=1000";
		}
		else {
			out_argv[out_argc++] = "-Wl,-Ttext=1000000000";
			out_argv[out_argc++] = "-Wl,-section-alignment=1000";
			out_argv[out_argc++] = "/tcc/crt0.o";
		}
	}

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-c") == 0) {
			compile_only = true;
		}
		else if (strcmp(argv[i], "-k") == 0) {
			kernel = true;
		}
		else if (strcmp(argv[i], "-o") == 0) {
			output = argv[++i];
		}
		else {
			out_argv[out_argc++] = argv[i];
		}
	}

	if (!compile_only) {
		out_argv[out_argc++] = "/tcc/libgrass.a";
		out_argv[out_argc++] = "/tcc/libtcc1.a";
		out_argv[out_argc++] = "/tcc/libgrass.a";
		out_argv[out_argc++] = "/tcc/end.o";
	}

	bool r = path_exec("/tcc/tcc.exe", out_argc, out_argv);

	if (r && convert) {
		out_argc = 0;
		out_argv[out_argc++] = "elf_cvt";
		if (kernel) {
			out_argv[out_argc++] = "-b";
			out_argv[out_argc++] = "2000000000";
		}
		out_argv[out_argc++] = "/tmp/cc.tmp";
		out_argv[out_argc++] = output == 0 ? "a.out.exe" : output;
		path_exec("/bin/elf_cvt.exe", out_argc, out_argv);
	}

	return 0;
}
