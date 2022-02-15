/* Primitive block-level encryption using AES128 in CTR mode. It encrypts and
 * decrypts blocks live on each read/write by using a hash of the salt and
 * offset.
 *
 * Uses SHA256 with multiple iterations for the master password.
 *
 *
 * Copyright 2018 Jason Liu
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef CIPHERDISK

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <egos/block_store.h>
#include "../../shared/aes.h"
#include <egos/sha256.h>
#include <egos/file.h>

#define CIPHER_ITERATIONS (1 << 20)
#define CIPHER_MAGIC 0x4411AE5D

/* The sole unencrypted block, which contains metadata. */
struct cipherdisk_superblock {
	uint32_t magic;
	uint8_t salt[32];
	uint8_t hashed[32];
};

/* State of a cipherdisk. */
struct cipherdisk_state {
	block_store_t *below;
	uint8_t key[16];
};

union cipherdisk_block {
	struct cipherdisk_superblock superblock;
	block_t datablock;
};

static int readline(char *, size_t);
static int cipherdisk_unlock(const uint8_t *, const uint8_t *, uint8_t *, size_t);
static int cipherdisk_open(block_store_t *, struct cipherdisk_state *);
static int cipherdisk_open(block_store_t *, struct cipherdisk_state *);

static int cipherdisk_write(block_store_t *, block_no, block_t *);
static int cipherdisk_read(block_store_t *, block_no, block_t *);
static int cipherdisk_nblocks(block_store_t *);
static int cipherdisk_setsize(block_store_t *, block_no);
static void cipherdisk_destroy(block_store_t *);

/* Read up to (len - 1) characters into s from stdin and return the number of
 * read chars. s will be null-terminated, and the trailing newline will not be
 * included. s should be at least as large as len. This will allow input
 * infinitely, but only the first len characters are actually stored.
 * Return -1 on error, and -2 if too many characters are read.
 *
 * This uses some hardcoded results for Linux, and may not be portable. */
static int readline(char *s, size_t len) {
	--len;
	size_t i = 0;
	while (i < len) {
		/* Using stdio and bypassing egos's stdio is not ideal, but not sure how
		 * to use egos's stdio this early in init */
		char c = fgetc(stdin);
		if (c == EOF)
			return -1;
		if (c == '\r') {
			s[i] = '\0';
			return i;
		} else if (c == 127) { /* delete keycode (backspace) */
			i = i > 0 ? i - 1 : 0;
		} else {
			s[i++] = c;
		}
	}
	s[len] = '\0';
	/* grab characters, but they won't actually be kept */
	for (;;) {
		char c = fgetc(stdin);
		if (c == EOF || c == '\r')
			return -2;
	}
}

/* Prompt the user for the passphrase to unlock this cipherdisk. keylen must
 * be less than or equal to 32. */
static int cipherdisk_unlock(const uint8_t *salt, const uint8_t *hashed,
		uint8_t *key, size_t keylen) {
	char pwd[256];
	for (unsigned int i = 0; i < 5; ++i) {
		printf("\n\rEnter cipherdisk passphrase: ");
		int n = readline(pwd, sizeof(pwd)/sizeof(pwd[0]));
		printf("\n\r");
		switch (n) {
			case 0:
				fprintf(stderr, "Giving up...\n\r");
				goto fail;

			case -1:
				fprintf(stderr, "cipherdisk_unlock: failed to read from stdin\n\r");
				return -1;

			case -2:
				goto retry;
		}

		/* calculate and check the hashed passphrase */
		sha256_context sc;
		uint8_t digest[32];

		sha256_starts(&sc);
		sha256_update(&sc, salt, 32);
		sha256_update(&sc, (uint8_t *) pwd, n);
		sha256_finish(&sc, digest);

		for (unsigned int j = 0; j < 32; ++j) {
			if (digest[j] != hashed[j]) {
				goto retry;
			}
		}

		/* calculate the key */
		sha256_starts(&sc);
		sha256_update(&sc, hashed, 32);
		sha256_update(&sc, (uint8_t *) pwd, n);
		sha256_finish(&sc, digest);

		memcpy(key, &digest, keylen);

		printf("\n\r");
		return 0;

retry:
		sleep(2);
		fprintf(stderr, "Wrong passphrase.\n\r");
	}

fail:
	fprintf(stderr, "\n\rToo many failed attempts.\n\n\r");
	return -1;
}

static int cipherdisk_setup(struct cipherdisk_state *cs) {
	struct cipherdisk_superblock sb;
	char pwd[2][256];
	sb.magic = CIPHER_MAGIC;
	for (unsigned int i = 0; i < 5; ++i) {
		printf("\n\rEnter a passphrase: ");
		int n1 = readline(pwd[0], sizeof(pwd[0])/sizeof(pwd[0][0]));
		printf("\n\r");
		switch (n1) {
			case 0:
				fprintf(stderr, "Empty passphrases are not allowed.\n\r");
				goto fail;

			case -1:
				fprintf(stderr, "cipherdisk_setup: failed to read from stdin\n\r");
				return -1;

			case -2:
				fprintf(stderr, "Passphrase too long; try again.\n\r");
				continue;
		}

		printf("Retype passphrase: ");
		int n2 = readline(pwd[1], sizeof(pwd[1])/sizeof(pwd[1][0]));
		printf("\n\r");
		switch (n2) {
			case 0:
				goto retry;

			case -1:
				fprintf(stderr, "cipherdisk_setup: failed to read from stdin\n\r");
				return -1;

			case -2:
				goto retry;
		}

		if (n1 != n2) {
			goto retry;
		}

		if (strncmp(pwd[0], pwd[1], sizeof(pwd[0])/sizeof(pwd[0][0])) == 0) {
			/* generate a random salt */
			srandom(clock_now());
			for (unsigned int i = 0; i < 32; ++i) {
				sb.salt[i] = (uint8_t) random();
			}

			/* calculate and set the hashed passphrase */
			sha256_context sc;
			uint8_t digest[32];

			sha256_starts(&sc);
			sha256_update(&sc, sb.salt, 32);
			sha256_update(&sc, (uint8_t *) pwd[0], n1);
			sha256_finish(&sc, digest);

			memcpy(sb.hashed, digest, sizeof(sb.hashed));

			/* calculate the key */
			sha256_starts(&sc);
			sha256_update(&sc, sb.hashed, 32);
			sha256_update(&sc, (uint8_t *) pwd[0], n1);
			sha256_finish(&sc, digest);

			memcpy(cs->key, digest, sizeof(cs->key)/sizeof(cs->key[0]));

			/* encrypt everything underneath */
			block_store_t tmp =
				{ .state = cs
				, .nblocks = cipherdisk_nblocks
				, .setsize = cipherdisk_setsize
				, .read = cipherdisk_read
				, .write = cipherdisk_write
				, .destroy = cipherdisk_destroy
				};
			printf("Encrypting blocks...");
			int nblocks = (*cs->below->nblocks)(cs->below);
			for (int i = 0; i < nblocks - 1; ++i) {
				block_t block;
				(*cs->below->read)(cs->below, i, &block);
				cipherdisk_write(&tmp, i, &block);
			}
			printf(" Done.\n\r");

			/* write the superblock */
			if ((*cs->below->write)(cs->below, 0, (block_t *) &sb) != 0) {
				fprintf(stderr, "cipherdisk_setup: failed to write superblock\n\r");
				return -1;
			}

			printf("\n\r");
			return 0;
		}

retry:
		fprintf(stderr, "Passphrases don't match; try again.\n\r");
	}

fail:
	fprintf(stderr, "Too many failed attempts.\n\n\r");
	return -1;
}

/* Try to open this cipherdisk, or prompt for a new password to create a
 * cipherdisk otherwise, and initialize cs. */
static int cipherdisk_open(block_store_t *below, struct cipherdisk_state *cs) {
	cs->below = below;
	union cipherdisk_block sb;
	if ((*below->read)(below, 0, &sb.datablock) != 0) {
		fprintf(stderr, "cipherdisk_open: failed to read superblock\n\r");
		return -1;
	}

	if (sb.superblock.magic == CIPHER_MAGIC) {
		return cipherdisk_unlock(sb.superblock.salt, sb.superblock.hashed,
				cs->key, sizeof(cs->key)/sizeof(cs->key[0]));
	} else {
		return cipherdisk_setup(cs);
	}
}

static int cipherdisk_nblocks(block_store_t *this_bs) {
	struct cipherdisk_state *cs = this_bs->state;
	return (*cs->below->nblocks)(cs->below) - 1;
}

static int cipherdisk_setsize(block_store_t *this_bs, block_no newsize) {
	/* not implemented */
	return -1;
}

/* Encrypt/decrypt the given block. */
static void block_xcrypt(struct cipherdisk_state *cs, block_no offset, block_t *block) {
	/* calculate the initialization vector */
	sha256_context sc;
	uint8_t iv[32];

	sha256_starts(&sc);
	sha256_update(&sc, cs->key, 32);
	sha256_update(&sc, (uint8_t *) &offset, sizeof(offset));
	sha256_finish(&sc, iv);

	/* this only uses 16 of the 32 bytes for both the iv and key */
	struct AES_ctx aes;
	AES_init_ctx_iv(&aes, cs->key, iv);
	AES_CTR_xcrypt_buffer(&aes, (uint8_t *) block, sizeof(block_t));
}

static int cipherdisk_read(block_store_t *this_bs, block_no offset, block_t *block) {
	struct cipherdisk_state *cs = this_bs->state;
	/* don't need to pad the blocks, because BLOCK_SIZE is already a multiple of 16 */
	assert(sizeof(block_t) % AES_BLOCKLEN == 0);

	if ((*cs->below->read)(cs->below, offset + 1, block) != 0) {
		fprintf(stderr, "cipherdisk_read: failed to read block\n\r");
		return -1;
	}

	/* decrypt the block */
	block_xcrypt(cs, offset, block);
	return 0;
}

static int cipherdisk_write(block_store_t *this_bs, block_no offset, block_t *block) {
	struct cipherdisk_state *cs = this_bs->state;
	/* don't need to pad the blocks, because BLOCK_SIZE is already a multiple of 16 */
	assert(BLOCK_SIZE % AES_BLOCKLEN == 0);

	/* copy the block into a buffer */
	block_t buffer;
	memcpy(&buffer, block, sizeof(buffer));

	/* encrypt the buffer */
	block_xcrypt(cs, offset, &buffer);

	if ((*cs->below->write)(cs->below, offset + 1, &buffer) != 0) {
		fprintf(stderr, "cipherdisk_write: failed to write block\n\r");
		return -1;
	}

	return 0;
}

static void cipherdisk_destroy(block_store_t *this_bs) {
	free(this_bs->state);
	free(this_bs);
}

block_store_t *cipherdisk_init(block_store_t *below) {
	struct cipherdisk_state *cs = new_alloc(struct cipherdisk_state);
	if (cipherdisk_open(below, cs) != 0) {
		fprintf(stderr, "cipherdisk_init: failed to open cipherdisk\n\r");
		sys_exit(1);
	}

	block_store_t *bi = new_alloc(block_store_t);
	bi->state = cs;
	bi->nblocks = cipherdisk_nblocks;
	bi->setsize = cipherdisk_setsize;
	bi->read = cipherdisk_read;
	bi->write = cipherdisk_write;
	bi->destroy = cipherdisk_destroy;
	return bi;
}

#endif // CIPHERDISK
