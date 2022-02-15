#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <assert.h>
// #include <sys/mman.h>
#include <elf.h>
#include "../h/earth/earth.h"
#include "../h/egos/exec.h"

static void usage(char *name){
	fprintf(stderr, "Usage: %s [-b base] elf-executable egos-executable\n", name);
	exit(1);
}

/* Parse an ELF format file.
 */
int main(int argc, char **argv){
	char c;
	unsigned long base = VIRT_BASE;		// default
    while ((c = getopt(argc, argv, "b:")) != -1) {
		if (c != 'b') {
			usage(argv[0]);
		}
		base = strtoul(optarg, 0, 16);
	}
	if (argc - optind != 2) {
		usage(argv[0]);
	}

	/* Open the two files.
	 */
	FILE *input, *output;
	if ((input = fopen(argv[optind], "r")) == 0) {
		fprintf(stderr, "%s: can't open macho executable %s\n",
									argv[0], argv[optind]);
		return 1;
	}
	if ((output = fopen(argv[optind + 1], "w")) == 0) {
		fprintf(stderr, "%s: can't create grass executable %s\n",
									argv[0], argv[optind + 1]);
		return 1;
	}

#ifdef x86_32
	Elf32_Ehdr ehdr;
#endif
#ifdef x86_64
	Elf64_Ehdr ehdr;
#endif

	size_t n = fread((char *) &ehdr, sizeof(ehdr), 1, input);
	if (n != 1) {
		fprintf(stderr, "%s: can't read header in %s (%d)\n", argv[0], argv[optind], (int) n);
		return 1;
	}
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
			ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
			ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
			ehdr.e_ident[EI_MAG3] != ELFMAG3 ||
#ifdef x86_32
			ehdr.e_ident[EI_CLASS] != ELFCLASS32 ||
#endif
#ifdef x86_64
			ehdr.e_ident[EI_CLASS] != ELFCLASS64 ||
#endif
			ehdr.e_type != ET_EXEC) {
		fprintf(stderr, "%s: %s not an executable\n", argv[0], argv[optind]);
		return 1;
	}

	/* Output header.
	 */
	struct exec_header eh;
	eh.eh_nsegments = 0;
	eh.eh_base = base / PAGESIZE;
	eh.eh_offset = 1;
	eh.eh_size = 0;
	eh.eh_start = ehdr.e_entry;

	/* Read the section table.
	 */
#ifdef x86_32
	Elf32_Shdr *shdr = calloc(sizeof(*shdr), ehdr.e_shnum);
#endif
#ifdef x86_64
	Elf64_Shdr *shdr = calloc(sizeof(*shdr), ehdr.e_shnum);
#endif
	fseek(input, ehdr.e_shoff, SEEK_SET);
	n = fread((char *) shdr, sizeof(*shdr), ehdr.e_shnum, input);
	if (n != ehdr.e_shnum) {
		fprintf(stderr, "%s: can't read section headers in %s (%d %d)\n",
						argv[0], argv[optind], (int) n, (int) ehdr.e_shnum);
		return 1;
	}
	int rela_index = -1, dynsym_index = -1;
	for (int i = 0; i < ehdr.e_shnum; i++) {
		switch (shdr[i].sh_type) {
		case SHT_RELA:
			rela_index = i;
			break;
		case SHT_DYNSYM:
			dynsym_index = i;
			break;
		}
	}
#ifdef VERBOSE
	if (rela_index > 0) {
		printf("rela %d %lx %x\n", shdr[rela_index].sh_type, shdr[rela_index].sh_addr, shdr[rela_index].sh_offset);
	}
	if (dynsym_index > 0) {
		printf("dynsym %d %lx %x\n", shdr[dynsym_index].sh_type, shdr[dynsym_index].sh_addr, shdr[dynsym_index].sh_offset);
	}
#endif

	/* Read the dynsym table.
	 */
#ifdef x86_32
	Elf32 *stab = 0;
#endif
#ifdef x86_64
	Elf64_Sym *stab = 0;
#endif
	if (dynsym_index > 0) {
		stab = malloc(shdr[dynsym_index].sh_size);
		fseek(input, shdr[dynsym_index].sh_offset, SEEK_SET);
		n = fread((char *) stab, 1, shdr[dynsym_index].sh_size, input);
		if (n != shdr[dynsym_index].sh_size) {
			fprintf(stderr, "can't read dynsym table\n");
			return 1;
		}
#ifdef VERBOSE
		unsigned int dynsym_cnt = shdr[dynsym_index].sh_size / shdr[dynsym_index].sh_entsize;
		printf("#entries in dynsym: %u\n", dynsym_cnt);
#endif
	}

	/* Find the program header table.
	 */
	unsigned int outpos = sizeof(eh), max_size = 0;
#ifdef x86_32
	Elf32_Phdr phdr;
#endif
#ifdef x86_64
	Elf64_Phdr phdr;
#endif
	unsigned int inpos = ehdr.e_phoff;
	for (int i = 0; i < ehdr.e_phnum; i++) {
		fseek(input, inpos, SEEK_SET);
		n = fread((char *) &phdr, sizeof(phdr), 1, input);
		if (n != 1) {
			fprintf(stderr, "%s: can't read program header in %s\n",
												argv[0], argv[optind]);
			return 1;
		}
		inpos += sizeof(phdr);

		if (phdr.p_memsz == 0 || phdr.p_type == PT_INTERP || phdr.p_type == PT_DYNAMIC) {
			continue;
		}
		if (phdr.p_vaddr < base) {
			continue;
		}

		printf("%s: %d type=%d vaddr=%lx-%lx memsz=%ld offset=%lx-%lx filesz=%ld prot=%x\n", argv[optind], i, phdr.p_type, phdr.p_vaddr, phdr.p_vaddr + phdr.p_memsz, phdr.p_memsz, phdr.p_offset, phdr.p_offset + phdr.p_filesz, phdr.p_filesz, phdr.p_flags);

		/* Some sanity checks.
		 */
		if (phdr.p_vaddr % PAGESIZE != 0) {
#ifdef VERBOSE
			fprintf(stderr, "%s: segment offset not page aligned\n", argv[0]);
#endif
			phdr.p_memsz += phdr.p_vaddr & (PAGESIZE - 1);
			phdr.p_vaddr &= ~(PAGESIZE - 1);
		}
		if (phdr.p_offset % PAGESIZE != 0) {
#ifdef VERBOSE
			fprintf(stderr, "%s: file offset not page aligned\n", argv[0]);
#endif
			phdr.p_filesz += phdr.p_offset & (PAGESIZE - 1);
			phdr.p_offset &= ~(PAGESIZE - 1);
		}
		if (phdr.p_filesz != 0 && phdr.p_filesz > phdr.p_memsz) {
			fprintf(stderr, "%s: bad size %u %u\n", argv[0],
						(unsigned int) phdr.p_filesz,
						(unsigned int) phdr.p_memsz);
			return 1;
		}

		/* Create a segment descriptor.
		 */
		struct exec_segment es;
		es.es_first = phdr.p_vaddr / PAGESIZE;
		es.es_npages = (phdr.p_memsz + PAGESIZE - 1) / PAGESIZE,
		es.es_prot = 0;
		if (phdr.p_flags & PF_R) {
			es.es_prot |= P_READ;
		}
		if (phdr.p_flags & PF_W) {
			es.es_prot |= P_WRITE;
		}
		if (phdr.p_flags & PF_X) {
			es.es_prot |= P_EXEC;
		}

		/* Write the segment descriptor to the output file.
		 */
		fseek(output, outpos, SEEK_SET);
		fwrite((char *) &es, sizeof(es), 1, output);
		outpos += sizeof(es);
		eh.eh_nsegments++;

		/* Copy the segment into the output file.
		 */
		fseek(input, phdr.p_offset, SEEK_SET);
		fseek(output, PAGESIZE + (phdr.p_vaddr - base), SEEK_SET);
		char buf[PAGESIZE];
		unsigned int total = phdr.p_filesz, size;
#ifdef VERBOSE
		printf(">>> %lx-%lx\n", phdr.p_vaddr - base, phdr.p_vaddr - base + phdr.p_filesz);
#endif
		if ((phdr.p_vaddr - base) + total > max_size) {
			max_size = (phdr.p_vaddr - base) + total;
		}
		while (total > 0) {
			size = total > PAGESIZE ? PAGESIZE : total;
			if ((n = fread(buf, 1, size, input)) <= 0) {
				fprintf(stderr, "%s: unexpexted EOF in %s\n", argv[0], argv[optind]);
				return 1;
			}
			fwrite(buf, 1, n, output);
			total -= n;
		}
	}

	/* Read the GOT.
	 */
	if (rela_index > 0 && stab != 0) {
		fseek(input, shdr[rela_index].sh_offset, SEEK_SET);
		for (size_t i = 0; i < shdr[rela_index].sh_size / shdr[rela_index].sh_entsize; i++) {
#ifdef x86_64
			Elf64_Rela er;
			if (fread(&er, 1, shdr[rela_index].sh_entsize, input) != shdr[rela_index].sh_entsize) {
				fprintf(stderr, "%s: can't read rela in %s\n", argv[0], argv[optind]);
				return 1;
			}
			switch (ELF64_R_TYPE(er.r_info)) {
			case R_X86_64_GLOB_DAT:
#ifdef VERBOSE
				printf("glob offset=%lx addr=%lx size=%d\n", er.r_offset, stab[ELF64_R_SYM(er.r_info)].st_value, (int) sizeof(stab[ELF64_R_SYM(er.r_info)].st_value));
#endif
				fseek(output, PAGESIZE + (er.r_offset - base), SEEK_SET);
				fwrite(&stab[ELF64_R_SYM(er.r_info)].st_value, 1, sizeof(stab[ELF64_R_SYM(er.r_info)].st_value), output);
				break;
			case R_X86_64_RELATIVE:
#ifdef VERBOSE
				printf("rela offset=%lx addend=%lx size=%d\n", er.r_offset, er.r_addend, (int) sizeof(er.r_addend));
#endif
				fseek(output, PAGESIZE + (er.r_offset - base), SEEK_SET);
				fwrite(&er.r_addend, 1, sizeof(er.r_addend), output);
				break;
			}
#endif
		}
	}

	/* Write the output header.
	 */
	fseek(output, 0, SEEK_SET);
	eh.eh_size = (max_size + PAGESIZE - 1) / PAGESIZE;
	fwrite((char *) &eh, sizeof(eh), 1, output);
	fclose(output);
	fclose(input);

	return 0;
}
