#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <egos/malloc.h>
#include <egos/file.h>
#include <egos/dir.h>

/* This is the client code for looking up an inode number.
 */
bool dir_lookup(gpid_t svr, fid_t dir, const char *name, fid_t *pfid){
	/* Prepare request.
	 */
	int n = strlen(name);
	struct dir_request *req = malloc(sizeof(*req) + n);
	memset(req, 0, sizeof(*req));
	req->type = DIR_LOOKUP;
	req->dir = dir;
	req->size = n;
	memcpy(req + 1, name, n);

	/* Do the RPC.
	 */
	struct dir_reply reply;
	int r = sys_rpc(svr, req, sizeof(*req) + n, &reply, sizeof(reply));
	free(req);
	if (r != sizeof(reply) || reply.status != DIR_OK) {
		return false;
	}
	*pfid = reply.fid;
	return true;
}

bool dir_insert(gpid_t svr, fid_t dir, const char *name, fid_t fid){
	/* Prepare request.
	 */
	int n = strlen(name);
	struct dir_request *req = malloc(sizeof(*req) + n);
	memset(req, 0, sizeof(*req));
	req->type = DIR_INSERT;
	req->dir = dir;
	req->fid = fid;
	req->size = n;
	memcpy(req + 1, name, n);

	/* Do the RPC.
	 */
	struct dir_reply reply;
	int r = sys_rpc(svr, req, sizeof(*req) + n, &reply, sizeof(reply));
	free(req);
	if (r != sizeof(reply) || reply.status != DIR_OK) {
		return false;
	}
	return true;
}

bool dir_remove(gpid_t svr, fid_t dir, const char *name, fid_t fid){
	/* Prepare request.
	 */
	int n = strlen(name);
	struct dir_request *req = malloc(sizeof(*req) + n);
	memset(req, 0, sizeof(*req));
	req->type = DIR_REMOVE;
	req->dir = dir;
	req->fid = fid;
	req->size = n;
	memcpy(req + 1, name, n);

	/* Do the RPC.
	 */
	struct dir_reply reply;
	int r = sys_rpc(svr, req, sizeof(*req) + n, &reply, sizeof(reply));
	free(req);
	if (r != sizeof(reply) || reply.status != DIR_OK) {
		return false;
	}
	return true;
}

/* Create a directory.  'svr' is the directory server.  'filesvr' is the
 * server where the new directory is to be stored.  'mode' are the access
 * control bits to the new directory.  'dir' is the parent directory.
 * 'name' is the name of the entry to be used in the parent directory.
 * The resulting directory is returned in *p_fid.
 */
bool dir_create2(gpid_t dirsvr, gpid_t filesvr, gmode_t mode, fid_t dir, const char *name, fid_t *p_fid){
	fid_t fid;

	/* First create a new file to hold the directory contents.
	 */
	bool r = file_create(filesvr, mode, &fid.file_no);
	if (!r) {
		return false;
	}
	fid.server = filesvr;

	/* Add the "." and ".." entries.
	 */
	struct dir_entry de[2];
	de[0].fid = fid;
	strcpy(de[0].name, "..dir");
	de[1].fid = dir;
	strcpy(de[1].name, "...dir");
	r = file_write(fid.server, fid.file_no, 0, de, sizeof(de));
	assert(r);

	/* Insert the new file in the directory.
	 */
	r = dir_insert(dirsvr, dir, name, fid);
	if (!r) {
		fprintf(stderr, "dir_insert %s failed\n", name);
	}
	else if (p_fid != 0) {
		*p_fid = fid;
	}
	return r;
}

/* Create a directory.  'svr' is the directory server.  'dir' is the
 * parent directory.  'name' is the name of the entry to be used in
 * the parent directory.  The resulting directory is returned in *p_fid.
 * The directory is stored at the same file server as the parent.
 */
bool dir_create(gpid_t svr, fid_t dir, const char *name, fid_t *p_fid){
	return dir_create2(svr, dir.server, P_FILE_DEFAULT, dir, name, p_fid);
}
