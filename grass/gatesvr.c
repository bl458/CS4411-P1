#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <earth/earth.h>
#include <earth/intf.h>
#include <egos/malloc.h>
#include <egos/gate.h>
#include "process.h"

static void gate_respond(gpid_t src, enum gate_status status){
	struct gate_reply rep;
	memset(&rep, 0, sizeof(rep));
	rep.status = status;
	sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Pull part of a file from the underlying operating system.
 */
static void gate_do_pull(struct gate_request *req, gpid_t src,
											void *data, unsigned int size){
	if (req->u.pull.flen != size) {
		printf("gate_do_pull: args don't match %u %u %u\n\r",
						req->u.pull.flen, size);
		gate_respond(src, GATE_ERROR);
	}
	char *file = m_alloc(req->u.pull.flen + 1);
	memcpy(file, data, req->u.pull.flen);
	file[req->u.pull.flen] = 0;

	size_t n = req->u.pull.size;
	if (n > PAGESIZE) {
		n = PAGESIZE;
	}
	struct gate_reply *rep = new_alloc_ext(struct gate_reply, n);
	int r = earth.dev_gate.read(file, req->u.pull.pos, rep + 1, n);
	if (r < 0) {
		printf("gate_do_pull: can't read '%s'\n\r", file);
		m_free(file);
		m_free(rep);
		gate_respond(src, GATE_ERROR);
		return;
	}

	rep->status = GATE_OK;
	rep->u.size = r;
	sys_send(src, MSG_REPLY, rep, sizeof(*rep) + r);

	m_free(rep);
	m_free(file);
}

/* Push part of a file to the underlying operating system.
 */
static void gate_do_push(struct gate_request *req, gpid_t src,
											void *data, unsigned int size){
	if (req->u.push.flen + req->u.push.size != size) {
		printf("gate_do_push: args don't match %u %u %u\n\r",
						req->u.push.flen, req->u.push.size, size);
		gate_respond(src, GATE_ERROR);
	}
	char *file = m_alloc(req->u.push.flen + 1);
	memcpy(file, data, req->u.push.flen);
	file[req->u.push.flen] = 0;

	size_t n = req->u.push.size;
	struct gate_reply rep;
	memset(&rep, 0, sizeof(rep));
	int r = earth.dev_gate.write(file, req->u.push.pos,
						(char *) (req + 1) + req->u.push.flen, n);
	if (r < 0) {
		printf("gate_do_push: can't write '%s'\n\r", file);
		m_free(file);
		gate_respond(src, GATE_ERROR);
		return;
	}

	rep.status = GATE_OK;
	rep.u.size = r;
	sys_send(src, MSG_REPLY, &rep, sizeof(rep));

	m_free(file);
}

/* Get timestamp of the physical world outide of EGOS.
 */
static void gate_do_gettime(struct gate_request *req, gpid_t src){
	struct gate_reply rep; 
	earth.dev_gate.gettime(&rep.u.time);
	rep.status = GATE_OK;
	sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* The 'gate' server, providing an interface with the underlying operating
 * system.
 */
static void gate_proc(void *arg){
	printf("GATE SERVER (interface to earth): pid=%u\n\r", sys_getpid());

	struct gate_request *req = new_alloc_ext(struct gate_request, PAGESIZE);
	for (;;) {
		gpid_t src;
		int req_size = sys_recv(MSG_REQUEST, 0, req, sizeof(req) + PAGESIZE, &src, 0);
		if (req_size < 0) {
			printf("gate server terminating\n\r");
			m_free(req);
			break;
		}

		assert(req_size >= (int) sizeof(*req));
		switch (req->type) {
		case GATE_PULL:
			gate_do_pull(req, src, req + 1, req_size - sizeof(*req));
			break;
		case GATE_PUSH:
			gate_do_push(req, src, req + 1, req_size - sizeof(*req));
			break;
		case GATE_GETTIME:
			gate_do_gettime(req, src);
			break;
		default:
			printf("gate server: bad request type\n\r");
			gate_respond(src, GATE_ERROR);
		}
	}
}

gpid_t gate_init(void){
	return proc_create(1, "gate", gate_proc, 0);
}
