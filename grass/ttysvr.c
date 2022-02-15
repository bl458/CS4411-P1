#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <earth/earth.h>
#include <earth/intf.h>
#include <egos/malloc.h>
#include <egos/syscall.h>
#include <egos/queue.h>
#include <egos/file.h>
#include "process.h"

/* This is the "tty" (terminal) server.  It support the FILE interface
 * to read from the keyboard or write to the screen.
 */

#define MAX_TTY_SIZE	4096		// maximum size of request to tty driver

/* Used to buffer up to a line of input.
 */
struct input {
	char *data;
	unsigned int size;
};

/* A request.
 */
struct tty_request {
	gpid_t src;
	unsigned int size;
};

/* The global state used by the terminal server.
 */
struct tty_state {
	gpid_t pid;					// process id of tty server
	struct input *buf;			// buffered bytes
	struct queue requests;		// queue of requests
	struct queue inputs;		// queue of buffered input
	unsigned long flags;		// see file.h
};

/* Respond to the client that sent req.  Note that this may be called
 * from the interrupt handler, and so we cannot use sys_send().
 */
static void tty_respond(struct tty_state *ts, gpid_t src,
					enum file_status status, void *data, unsigned int size){
	struct file_reply *rep = new_alloc_ext(struct file_reply, size);
	rep->status = status;
	rep->fcb.st_size = size;
	memcpy(&rep[1], data, size);
	proc_send(ts->pid, 0, src, MSG_REPLY, rep, sizeof(*rep) + size);
	m_free(rep);
}

/* A line of input is ready to be sent to the client that sent req.
 * Note, this may be called from an interrupt handler.
 */
static void tty_handle(struct tty_state *ts,
				struct tty_request *tr, struct input *buf){
	unsigned int n = buf == 0 ? 0 : buf->size;

	/* We may not be able to use all of the buffer.
	 */
	if (n > tr->size) {
		n = tr->size;
	}

	/* Send the reply.
	 */
	struct file_reply *rep = new_alloc_ext(struct file_reply, n);
	rep->status = FILE_OK;
	memcpy(&rep[1], buf->data, n);
	proc_send(ts->pid, 0, tr->src, MSG_REPLY, rep, sizeof(*rep) + n);
	m_free(rep);
	m_free(tr);

	/* If there's left-over data in the buffer, insert it back
	 * into the queue.
	  */
	if (n < buf->size) {
		memmove(buf->data, &buf->data[n], buf->size - n);
		buf->size -= n;
		queue_insert(&ts->inputs, buf);
	}
	else {
		m_free(buf->data);
		m_free(buf);
	}
}

/* This is the I/O interrupt for terminal input.  Add the input to
 * the buffer.  If there's a full line of input and a request for
 * input is queued, return the line.
 *
 * Note that this is an interrupt handler, and so it does not
 * necesssarily run on the stack of the the tty process, but on
 * the stack of whatever process invoked intr_suspend().
 */
static void tty_deliver(void *arg, char *buf, int size){
	struct tty_state *ts = arg;

	for (; size-- > 0; buf++) {
		switch (*buf) {
		/* TODO.  If we want to make it more like Unix, we should also
		 *		  flush the input buffer on <ctrl>C.
		 */
		case 'c' & 0x1F:
			printf("<ctrl>C\n");
			proc_kill(ts->pid, 0, STAT_INTR);
			break;
		case 'd' & 0x1F:
			queue_add(&ts->inputs, ts->buf);
			ts->buf = new_alloc(struct input);
			break;
		case 'h' & 0x1F:
		case 0x7F:
			if (ts->buf->size > 0) {
				ts->buf->size--;
				printf("\b \b");
			}
			break;
		case 'l' & 0x1F:
			proc_dump();
			break;
		case 'q' & 0x1F:
			shut_down();
			break;
		default:
			if (*buf == '\r') {
				*buf = '\n';
			}
			if (ts->flags & FILE_ECHO) {
				printf("%c", *buf);
			}
			if (*buf == '\n') {
				printf("\r");
			}
			ts->buf->data = m_realloc(ts->buf->data, ts->buf->size + 1);
			ts->buf->data[ts->buf->size++] = *buf;
			if (*buf == '\n') {
				queue_add(&ts->inputs, ts->buf);
				ts->buf = new_alloc(struct input);
			}
		}
	}

	struct tty_request *tr;
	if (!queue_empty(&ts->inputs) && (tr = queue_get(&ts->requests)) != 0) {
		tty_handle(ts, tr, queue_get(&ts->inputs));
	}
}

/* Respond to a read request.
 */
static void tty_read(struct tty_state *ts, struct file_request *req, gpid_t src){
	/* If not the 0 inode, return an error.
	 */
	if (req->file_no != 0) {
		tty_respond(ts, src, FILE_ERROR, 0, 0);
		return;
	}

	/* Add the request to the queue of read requests.
	 */
	struct tty_request *tr = new_alloc(struct tty_request);
	tr->src = src;
	tr->size = req->size;
	queue_add(&ts->requests, tr);

	/* If there's already input waiting, respond now.
	 */
	if (!queue_empty(&ts->inputs)) {
		tr = queue_get(&ts->requests);
		tty_handle(ts, tr, queue_get(&ts->inputs));
	}
}

/* Respond to a write request.
 */
static void tty_write(struct tty_state *ts,
				struct file_request *req, gpid_t src, char *data, unsigned int size){
	/* Error checks.
	 */
	if (req->file_no != 1 && req->file_no != 2) {
		printf("tty_write: bad inode number: %u\n", req->file_no);
		tty_respond(ts, src, FILE_ERROR, 0, 0);
		return;
	}
	if (req->size != size) {
		printf("tty_write: size mismatch\n");
		tty_respond(ts, src, FILE_ERROR, 0, 0);
		return;
	}

	unsigned int i;
	for (i = 0; i < size; i++) {
		earth.dev_tty.write(req->file_no, &data[i], 1);
		if (data[i] == '\n') {
			earth.dev_tty.write(req->file_no, "\r", 1);
		}
	}
	tty_respond(ts, src, FILE_OK, 0, 0);
}

/* Respond to a sync request (but ignore it otherwise).
 */
static void tty_sync(struct tty_state *ts,
				struct file_request *req, gpid_t src){
	tty_respond(ts, src, FILE_OK, 0, 0);
}

static void tty_do_stat(struct tty_state *ts, struct file_request *req, gpid_t src){
	struct file_reply rep;
	memset(&rep, 0, sizeof(rep));
	rep.status = FILE_OK;
	rep.op = FILE_STAT;
	rep.fcb.st_mode = P_FILE_ALL;
	sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

static void tty_set_flags(struct tty_state *ts, struct file_request *req, gpid_t src){
	ts->flags = req->offset;
	tty_respond(ts, src, FILE_OK, 0, 0);
}

/* The terminal server.  Acts essentiallly as a file server, but ignoring
 * offsets.  inode 0 = stdin, inode 1 = stdout, inode 2 - stderr.
 */
static void tty_proc(void *arg){
	struct tty_state *ts = new_alloc(struct tty_state);

	printf("TTY SERVER (terminal I/O): pid=%u\n\r", sys_getpid());

	ts->pid = sys_getpid();
	queue_init(&ts->requests);
	queue_init(&ts->inputs);
	ts->buf = new_alloc(struct input);
	earth.dev_tty.create(0, tty_deliver, ts);

	struct file_request *req = new_alloc_ext(struct file_request, PAGESIZE);
	for (;;) {
		/* Receive a request.
		 */
		gpid_t src;
		int req_size = sys_recv(MSG_REQUEST, 0, req, sizeof(req) + PAGESIZE, &src, 0);
		if (req_size < 0) {
			printf("tty server: terminating\n\r");
			m_free(req);
			break;
		}

		assert(req_size >= (int) sizeof(*req));

		switch (req->type) {
		case FILE_READ:
			tty_read(ts, req, src);
			break;
		case FILE_WRITE:
			tty_write(ts, req, src, (char *) &req[1], req_size - sizeof(*req));
			break;
        case FILE_SYNC:
            tty_sync(ts, req, src);
            break;
		case FILE_STAT:
			tty_do_stat(ts, req, src);
			break;
		case FILE_SET_FLAGS:
			tty_set_flags(ts, req, src);
			break;
		default:
			printf("tty_proc: unknown command %d, src=%u\n", req->type, src);
			tty_respond(ts, src, FILE_ERROR, 0, 0);
		}
	}
}

gpid_t tty_init(void){
	return proc_create(1, "tty", tty_proc, 0);
}
