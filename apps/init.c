#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <egos/spawn.h>
#include <egos/dir.h>
#include <egos/gate.h>
#include <egos/map.h>

static struct map *dirtree;						// directory tree

struct fs_init {
	enum fs_type { FI_END, FI_DIR, FI_FILE } type;
	unsigned int uid;
	char *path, *src;
};

static struct fs_init fs_table[] = {
	{ FI_DIR, 0, "/bin.dir", 0 },
	{ FI_DIR, 0, "/etc.dir", 0 },
	{ FI_DIR, 0, "/usr.dir", 0 },

	{ FI_FILE, 0, "/etc.dir/passwd", "/etc/passwd" },

	{ FI_FILE, 0, "/bin.dir/dirsvr.exe", "bin/dirsvr.exe" },
	{ FI_FILE, 0, "/bin.dir/init.exe", "bin/init.exe" },
	{ FI_FILE, 0, "/bin.dir/login.exe", "bin/login.exe" },
	{ FI_FILE, 0, "/bin.dir/pwdsvr.exe", "bin/pwdsvr.exe" },
	{ FI_FILE, 0, "/bin.dir/syncsvr.exe", "bin/syncsvr.exe" },

	{ FI_FILE, 0, "/bin.dir/bfs.exe", "bin/bfs.exe" },
	{ FI_FILE, 0, "/bin.dir/blocksvr.exe", "bin/blocksvr.exe" },
	{ FI_FILE, 0, "/bin.dir/cat.exe", "bin/cat.exe" },
	{ FI_FILE, 0, "/bin.dir/echo.exe", "bin/echo.exe" },
	{ FI_FILE, 0, "/bin.dir/kill.exe", "bin/kill.exe" },
	{ FI_FILE, 0, "/bin.dir/loop.exe", "bin/loop.exe" },
	{ FI_FILE, 0, "/bin.dir/ls.exe", "bin/ls.exe" },
	{ FI_FILE, 0, "/bin.dir/passwd.exe", "bin/passwd.exe" },
	{ FI_FILE, 0, "/bin.dir/pwd.exe", "bin/pwd.exe" },
	{ FI_FILE, 0, "/bin.dir/shell.exe", "bin/shell.exe" },
	{ FI_FILE, 0, "/bin.dir/shutdown.exe", "bin/shutdown.exe" },
	{ FI_FILE, 0, "/bin.dir/sync.exe", "bin/sync.exe" },

	{ FI_DIR, 666, "/usr.dir/guest.dir", 0 },
	{ FI_DIR, 10, "/usr.dir/rvr.dir", 0 },
	{ FI_DIR, 20, "/usr.dir/yunhao.dir", 0 },

	{ FI_END, 0, 0, 0 }
};

static bool endswith(const char *s, const char *suffix){
    size_t len = strlen(s), suflen = strlen(suffix);
    return suflen > len ? false :
			strcmp(s + len - suflen, suffix) == 0;
}

#define BUF_SIZE    4096

/* Load a "grass" file with contents of a local file.
 */
static void fs_pull(fid_t fid, const char *file){
    static char buf[BUF_SIZE];

    unsigned long pos = 0;
	for (;;) {
		unsigned int size = BUF_SIZE;
		bool r = gate_pull(GRASS_ENV->servers[GPID_GATE], file, pos, buf, &size);
		if (!r) {
			fprintf(stderr, "pull: gate_pull failed\n");
			exit(1);
		}
		if (size == 0) {
			break;
		}
        bool status = file_write(fid.server, fid.file_no, pos, buf, size);
        assert(status);
        pos += size;
    }
}

/* Create and load a file on the given server.
 */
fid_t file_load(gpid_t server, unsigned int uid, const char *src){
	/* Allocate a file.
	 */
	unsigned int file_no;
	bool r = file_create(server, P_FILE_DEFAULT, &file_no);
	assert(r);
	if (uid != 0) {
		r = file_chown(server, file_no, uid);
		assert(r);
	}

	/* Initialize the file.
	 */
	fid_t fid = fid_val(server, file_no);
	fs_pull(fid, src);

	return fid;
}

/* Copy the given file from the underlying operating system into
 * the file system using the given inode number.  'ds' is the
 * process identifier of the directory server.
 */
static void file_install(gpid_t ds, const char *path, const char *src, unsigned int uid){
	/* Break up path.
	 */
	char *div = rindex(path, '/');
	assert(div);

	/* Find the parent directory.
	 */
	fid_t *parent = map_lookup(dirtree, path, (int) (div - path));
	assert(parent);

	/* Create and load the file.
	 */
	fid_t fid = file_load(parent->server, uid, src == 0 ? div + 1 : src);

	/* Insert into the directory.
	 */
	bool r = dir_insert(ds, *parent, div + 1, fid);
	assert(r);
}

/* Create the given directory.
 */
static void dir_install(gpid_t dirsvr, const char *path,
					unsigned int uid, /* OUT */ fid_t *p_fid){
	/* Break up path.
	 */
	char *div = rindex(path, '/');
	assert(div);

	/* Find the parent directory.
	 */
	fid_t *parent = map_lookup(dirtree, path, (int) (div - path));
	assert(parent);
	
	/* Create the new directory.
	 */
	fid_t *fid = malloc(sizeof(*fid));
	bool r = dir_create(dirsvr, *parent, div + 1, fid);
	assert(r);

	if (uid != 0) {
		r = file_chown(fid->server, fid->file_no, uid);
		assert(r);
	}

	/* Insert the new directory in the directory tree.
	 */
	void **ent;
	ent = map_insert(&dirtree, path, strlen(path));
	*ent = fid;

	if (p_fid != 0) {
		*p_fid = *fid;
	}
}

/* Create a .dev entry.
 */
static void dev_install(struct grass_env *ge, fid_t dev,
								char *name, enum grass_servers svr){
	fid_t fid;
	fid.server = ge->servers[svr];
	fid.file_no = 0;
	(void) dir_insert(ge->servers[GPID_DIR], dev, name, fid);
}

static void fs_init(gpid_t ds){
	struct fs_init *fi = fs_table;

	for (int i = 0; fs_table[i].type != FI_END; i++, fi++) {
		enum fs_type type;

		printf("Installing %s                                \r", fi->path);
		fflush(stdout);

		if (endswith(fi->path, ".dir")) {
			type = FI_DIR;
		}
		else {
			type = FI_FILE;
		}

		switch (type) {
		case FI_DIR:
			assert(fi->src == NULL);
			dir_install(ds, fi->path, fi->uid, 0);
			break;
		case FI_FILE:
			file_install(ds, fi->path, fi->src, fi->uid);
			break;
		default:
			assert(0);
		}
	}
	printf("File system installation completed                        \n\r");
}

static void dt_cleanup(void *env,
			const void *key, unsigned int key_size, void *value){
	free(value);
}

/* Set up the initial file system.
 */
void fs_install(struct grass_env *ge) {
	dirtree = map_init();			// directory tree map

	/* Insert root into directory tree map.
	 */
	void **ent;
	ent = map_insert(&dirtree, "", 0);
	*ent = malloc(sizeof(ge->root));
	* (fid_t *) *ent = ge->root;

	gpid_t ds = ge->servers[GPID_DIR];
	fid_t dev;

	/* Create dev directory.
	 */
	dir_install(ds, "/dev.dir", 0, &dev);

	/* Create some "file devices" in the /dev directory.
	 */
	dev_install(ge, dev, "file.dev", GPID_FILE);
	dev_install(ge, dev, "file_block.dev", GPID_FILE_BLOCK);
	dev_install(ge, dev, "file_ram.dev", GPID_FILE_RAM);
	dev_install(ge, dev, "tty.dev", GPID_TTY);

	/* Initialize the rest of the file system.
	 */
	fs_init(ds);

	/* Clean up directory tree.
	 */
	map_iter(0, dirtree, dt_cleanup);
}

/* This is the 'init' process---the very first process that is started
 * by the kernel itself.  Currently, it launches a shell and then it
 * just waits.
 */
int main(int argc, char **argv){
	char *args[2];
	gpid_t pid;

	printf("INIT SERVER (initializes file system, runs login process): pid=%u\n", sys_getpid());

	/* Try to read inode 1 (file for dir server) and see whether it exists
	 */
	if (file_exist(GRASS_ENV->servers[GPID_FILE], 1)) {
		// TODO: with tcc, GRASS_ENV->root.file_no = 1 seems to be optimized out
		//		 or something so replaced it with the following two lines:
		struct grass_env *gp = GRASS_ENV;
		gp->root.file_no = 1;
		assert(GRASS_ENV->root.file_no == 1);
	}
	else {
		/* Create the root directory.
		 */
		bool r = file_create(GRASS_ENV->servers[GPID_FILE], P_FILE_DEFAULT, &GRASS_ENV->root.file_no);
		assert(r);
		assert(GRASS_ENV->root.file_no == 1);
		printf("INIT SERVER: new disk: installing dir file at inode 1\n\r");

		/* Write the initial contents of the root directory.
		 */
		struct dir_entry de[2];
		de[0].fid = GRASS_ENV->root;
		strcpy(de[0].name, "..dir");
		de[1].fid = GRASS_ENV->root;
		strcpy(de[1].name, "...dir");
		(void) file_write(GRASS_ENV->root.server, GRASS_ENV->root.file_no, 0, de, sizeof(de));

		printf("Creating directory hierarchy\n\r");
		fs_install(GRASS_ENV);
		fflushsync(0);
	}
	GRASS_ENV->cwd = GRASS_ENV->root;

	/* Create /tmp.  Remove any existing entry.
	 */
	fid_t tmp_fid;
	bool r = dir_lookup(GRASS_ENV->servers[GPID_DIR],
						GRASS_ENV->root, "tmp.dir", &tmp_fid);
	if (r) {
		r = dir_remove(GRASS_ENV->servers[GPID_DIR],
							GRASS_ENV->root, "tmp.dir", tmp_fid);
	}
	(void) dir_create2(GRASS_ENV->servers[GPID_DIR],
						GRASS_ENV->servers[GPID_FILE_RAM],
						P_FILE_ALL, GRASS_ENV->root, "tmp.dir", &tmp_fid);

	/* Start the sync server.
	 */
	args[0] = "syncsvr";
	args[1] = 0;
	(void) spawn_vexec("/bin/syncsvr.exe", 1, args, false, 0, &GRASS_ENV->servers[GPID_SYNC]);

	/* Start the password server.
	 */
	args[0] = "pwdsvr";
	args[1] = 0;
	(void) spawn_vexec("/bin/pwdsvr.exe", 1, args, false, 0, &GRASS_ENV->servers[GPID_PWD]);

	/* Start the login process.
	 */
	args[0] = "login";
	args[1] = 0;
	(void) spawn_vexec("/bin/login.exe", 1, args, false, 0, &pid);

	for (;;) {
		struct msg_event mev;
		int size = sys_recv(MSG_EVENT, 0, &mev, sizeof(mev), 0, 0);
		if (size < 0) {
			printf("init: terminating\n");
			return 1;
		}
		if (size != sizeof(mev)) {
			continue;
		}
		printf("init: process %u died\n", mev.pid);
	}
}
