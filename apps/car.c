/* "C archiver" (much like shar, but then for C instead o shell).
 * "car file1 ..." produces a C file as output.  If you compile and
 * run this output, it recreates those input files.
 */

#include <stdio.h>
#include <stdlib.h>

#define CHUNK_SIZE	4096

int fid;			// file identifier

void car(char *file){
	FILE *fp = fopen(file, "r");
	if (fp == 0) {
		fprintf(stderr, "Can't open '%s'.  Skipping...\n", file);
		return;
	}

	printf("char *f%d[] = {		// %s\n", fid++, file);

	static char buf[CHUNK_SIZE];
	for (;;) {
		/* ANSI C must support up to 509 characters in a string.  Divide by 2
		 * as we represent each by by two characters.  Then round down to a
		 * multiple of 4 for no good reason.
		 */
		size_t n = fread(buf, 1, 252, fp);
		if (n == 0) {
			if (ferror(fp)) {
				fprintf(stderr, "Read error reading '%s'\n", file);
			}
			fclose(fp);
			break;
		}
		printf("\t\"");
		for (size_t i = 0; i < n; i++) {
			printf("%c%c", 'a' + ((buf[i] >> 4) & 0xF), 'a' + (buf[i] & 0xF));
		}
		printf("\",\n");
	}
	printf("\t0\n};\n");
}

void preamble(){
	printf("#include <stdio.h>\n");
	printf("\n");
	printf("struct car {\n");
	printf("	char *name;\n");
	printf("	char **contents;\n");
	printf("} cars[] = {\n");
}

void postamble(){
	printf("void uncar(struct car *car){\n");
	printf("	FILE *fp = fopen(car->name, \"w\");\n");
	printf("	if (fp == 0) {\n");
	printf("		fprintf(stderr, \"Can't create '%%s'.  Skipping ...\\n\", car->name);\n");
	printf("		return;\n");
	printf("	}\n");
	printf("	char *p;\n");
	printf("	for (int i = 0; (p = car->contents[i]) != 0; i++) {\n");
	printf("		while (*p != 0) {\n");
	printf("			char c = ((p[0] - 'a') << 4) | (p[1] - 'a');\n");
	printf("			fputc(c, fp);\n");
	printf("			p += 2;\n");
	printf("		}\n");
	printf("	}\n");
	printf("	fclose(fp);\n");
	printf("}\n");
	printf("\n");
	printf("int main(){\n");
	printf("	for (int i = 0; cars[i].name != 0; i++)\n");
	printf("		uncar(&cars[i]);\n");
	printf("	return 0;\n");
	printf("}\n");
}

int main(int argc, char **argv){
	for (int i = 1; i < argc; i++) {
		car(argv[i]);
	}
	printf("\n");
	preamble();
	for (int i = 0; i < argc - 1; i++) {
		printf("\t{ \"%s\", f%d },\n", argv[i + 1], i);
	}
	printf("\t{ 0, 0 }\n");
	printf("};\n\n");
	postamble();
	return 0;
}
