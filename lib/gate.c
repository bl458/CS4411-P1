#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <egos/malloc.h>
#include <egos/syscall.h>
#include <egos/gate.h>

bool gate_pull(gpid_t svr, const char *file, unsigned long pos,
						/* OUT */ char *buf, /* IN/OUT */ unsigned int *size){
	/* Prepare request.
	 */
	unsigned int n = strlen(file);
	struct gate_request *req = malloc(sizeof(*req) + n);
	memset(req, 0, sizeof(*req));
	req->type = GATE_PULL;
	req->u.pull.pos = pos;
	req->u.pull.flen = n;
	req->u.pull.size = *size;
	memcpy(req + 1, file, n);

	/* Do the RPC.
	 */
	struct gate_reply *rep = malloc(sizeof(*rep) + *size);
	int r = sys_rpc(svr, req, sizeof(*req) + n, rep, sizeof(*rep) + *size);
	free(req);
	if (r < (int) sizeof(*rep) || rep->status != GATE_OK) {
		free(rep);
		return false;
	}
	assert(r == (int) (sizeof(*rep) + rep->u.size));
	memcpy(buf, rep + 1, rep->u.size);
	*size = rep->u.size;
	free(rep);
	return true;
}

bool gate_push(gpid_t svr, const char *file, unsigned long pos,
					/* IN */ const char *buf, /* IN */ unsigned int size){
	/* Prepare request.
	 */
	unsigned int n = strlen(file);
	struct gate_request *req = malloc(sizeof(*req) + n + size);
	memset(req, 0, sizeof(*req));
	req->type = GATE_PUSH;
	req->u.push.pos = pos;
	req->u.push.flen = n;
	req->u.push.size = size;
	memcpy(req + 1, file, n);
	memcpy((char *) (req + 1) + n, buf, size);

	/* Do the RPC.
	 */
	struct gate_reply rep;
	int r = sys_rpc(svr, req, sizeof(*req) + n + size, &rep, sizeof(rep));
	if (r < (int) sizeof(rep) || rep.status != GATE_OK) {
		return false;
	}
	assert(r == (int) sizeof(rep));
	return true;
}

bool gate_gettime(gpid_t svr, struct gate_time *time){
	/* Prepare request.
	 */
	struct gate_request req;
	memset(&req, 0, sizeof(req));
	req.type = GATE_GETTIME;

	/* Do the RPC.
	 */
	struct gate_reply rep;
	int r = sys_rpc(svr, &req, sizeof(req), &rep, sizeof(rep));
	if (r < (int) sizeof(rep) || rep.status != GATE_OK) {
		return false;
	}
	*time = rep.u.time;
	return true;
}
