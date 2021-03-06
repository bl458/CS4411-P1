#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <egos/syscall.h>
#include <egos/malloc.h>
#include <egos/block.h>

bool block_read(gpid_t svr, unsigned int ino, unsigned int offset, void *addr){
    /* Prepare request.
     */
    struct block_request req;
    memset(&req, 0, sizeof(req));
    req.type = BLOCK_READ;
    req.ino = ino;

    /* Allocate reply. psize in BLOCKs, not bytes
     */
    struct block_reply *reply = (struct block_reply *) malloc(sizeof(*reply) + BLOCK_SIZE);
    unsigned int reply_size = sizeof(*reply) + BLOCK_SIZE;

    /* Do the RPC.
     */
	req.offset_nblock = offset;

	int n = sys_rpc(svr, &req, sizeof(req), reply, reply_size);
	if (n < (int) reply_size) {
		free(reply);
		return false;
	}
	if (reply->status != BLOCK_OK) {
		free(reply);
		return false;
	}

	n -= sizeof(*reply);
	memcpy(addr, &reply[1], BLOCK_SIZE);

    free(reply);
    return true;
}

bool block_write(gpid_t svr, unsigned int ino, unsigned int offset, const void *addr){
    /* Prepare request.
     */
    struct block_request *req =
                (struct block_request *) malloc(sizeof(*req) + BLOCK_SIZE);
    memset(req, 0, sizeof(*req));
    req->type = BLOCK_WRITE;
    req->ino = ino;

	req->offset_nblock = offset + 0;
	memcpy(&req[1], addr, BLOCK_SIZE);

	/* Do the RPC.
	 */
	struct block_reply reply;
	// printf("BLK_WR %u %u\n", sizeof(*req) + size * BLOCK_SIZE, size);
	int result = sys_rpc(svr, req, sizeof(*req) + BLOCK_SIZE, &reply, sizeof(reply));
	if (result < (int) sizeof(reply) || reply.status != BLOCK_OK) {
		free(req);
		return false;
	}

    free(req);
    return true;
}

bool block_sync(gpid_t svr, unsigned int ino){
    /* Prepare request.
     */
    struct block_request req;
    memset(&req, 0, sizeof(req));
    req.type = BLOCK_SYNC;
    req.ino = ino;

    /* Do the RPC.
     */
    struct block_reply reply;
    // printf("BLK_WR %u %u\n", sizeof(*req) + size * BLOCK_SIZE, size);
    int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
    if (result < (int) sizeof(reply) || reply.status != BLOCK_OK) {
        return false;
    }
    return true;
}

bool block_getsize(gpid_t svr, unsigned int ino, unsigned int *psize){
    /* Prepare request.
     */
    struct block_request req;
    memset(&req, 0, sizeof(req));
    req.type = BLOCK_GETSIZE;
    req.ino = ino;

    /* Do the RPC.
     */
    struct block_reply reply;
    int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
    if (result < (int) sizeof(reply)) {
        return false;
    }
    *psize = reply.size_nblock;
    return reply.status == BLOCK_OK;
}

bool block_setsize(gpid_t svr, unsigned int ino, unsigned int size){
    /* Prepare request.
     */
    struct block_request req;
    memset(&req, 0, sizeof(req));
    req.type = BLOCK_SETSIZE;
    req.ino = ino;
    req.offset_nblock = size;

    /* Do the RPC.
     */
    struct block_reply reply;
    int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
    if (result < (int) sizeof(reply)) {
        return false;
    }
    return reply.status == BLOCK_OK;
}

bool block_getninodes(gpid_t svr, unsigned int *ninodes){
    /* Prepare request.
     */
    struct block_request req;
    memset(&req, 0, sizeof(req));
    req.type = BLOCK_GETNINODES;

    /* Do the RPC.
     */
    struct block_reply reply;
    int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
    if (result < (int) sizeof(reply)) {
        return false;
    }
    *ninodes = reply.br_ninodes;
    return reply.status == BLOCK_OK;
}
