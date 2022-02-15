#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <egos/malloc.h>
#include <egos/file.h>

bool file_exist(gpid_t svr, unsigned int file_no) {
	unsigned int psize = 0;
	return file_read(svr, file_no, 0, 0, &psize);
}

/* Creates a file on server svr. Returns the file number of the new 
 * file (on that server) in *p_fileno.
 */
bool file_create(gpid_t svr, gmode_t mode, unsigned int *p_fileno){
	/* Prepare request.
	 */
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_CREATE;
	req.mode = mode;

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (result < (int) sizeof(reply)) {
		return false;
	}
	*p_fileno = reply.file_no;
	return reply.status == FILE_OK;
}

bool file_chown(gpid_t svr, unsigned int file_no, unsigned int uid) {
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_CHOWN;
	req.file_no = file_no;
	req.uid = uid;

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (result < (int) sizeof(reply)) {
		return false;
	}
	return reply.status == FILE_OK;
}

bool file_chmod(gpid_t svr, unsigned int file_no, gmode_t mode) {
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_CHMOD;
	req.file_no = file_no;
	req.mode = mode;

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (result < (int) sizeof(reply)) {
		return false;
	}
	return reply.status == FILE_OK;
}

bool file_read(gpid_t svr, unsigned int file_no, unsigned long offset,
										void *addr, unsigned int *psize){
	/* Prepare request.
	 */
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_READ;
	req.file_no = file_no;
	req.offset = offset;
	req.size = *psize;

	/* Allocate reply.
	 */
	struct file_reply *reply = (struct file_reply *) malloc(sizeof(*reply) + *psize);
	unsigned int reply_size = sizeof(*reply) + *psize;

	/* Do the RPC.
	 */
	int n = sys_rpc(svr, &req, sizeof(req), reply, reply_size);
	if (n < (int) sizeof(*reply)) {
		free(reply);
		return false;
	}
	if (reply->status != FILE_OK) {
		free(reply);
		return false;
	}
	n -= sizeof(*reply);
	memcpy(addr, &reply[1], n);
	*psize = n;
	free(reply);
	return true;
}

bool file_write(gpid_t svr, unsigned int file_no, unsigned long offset,
										const void *addr, unsigned int size){
	/* Prepare request.
	 */
	struct file_request *req =
				(struct file_request *) malloc(sizeof(*req) + size);
	memset(req, 0, sizeof(*req));
	req->type = FILE_WRITE;
	req->file_no = file_no;
	req->offset = offset;
	req->size = size;
	memcpy(&req[1], addr, size);

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, req, sizeof(*req) + size, &reply, sizeof(reply));
	free(req);
	if (result < (int) sizeof(reply)) {
		return false;
	}
	return reply.status == FILE_OK;
}

bool file_sync(gpid_t svr, unsigned int file_no){
	/* Prepare request.
	 */
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_SYNC;
	req.file_no = file_no;

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (result < (int) sizeof(reply)) {
		return false;
	}
	return reply.status == FILE_OK;
}

bool file_stat(gpid_t svr, unsigned int file_no, struct file_control_block *pfcb){
	/* Prepare request.
	 */
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_STAT;
	req.file_no = file_no;

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (result < (int) sizeof(reply)) {
		return false;
	}
	*pfcb = reply.fcb;
	return reply.status == FILE_OK;
}

bool file_setsize(gpid_t svr, unsigned int file_no, unsigned long size){
	/* Prepare request.
	 */
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_SETSIZE;
	req.file_no = file_no;
	req.offset = size;

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (result < (int) sizeof(reply)) {
		return false;
	}
	return reply.status == FILE_OK;
}

bool file_delete(gpid_t svr, unsigned int file_no){
	/* Prepare request.
	 */
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_DELETE;
	req.file_no = file_no;

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (result < (int) sizeof(reply)) {
		return false;
	}
	return reply.status == FILE_OK;
}

bool file_set_flags(gpid_t svr, unsigned int file_no, unsigned long flags){
	/* Prepare request.
	 */
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_SET_FLAGS;
	req.file_no = file_no;
	req.offset = flags;

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (result < (int) sizeof(reply)) {
		return false;
	}
	return reply.status == FILE_OK;
}
