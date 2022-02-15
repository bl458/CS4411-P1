#ifndef _EGOS_GATE_H
#define _EGOS_GATE_H

#include <stdbool.h>
#include <egos/syscall.h>

struct gate_request {
	enum {
		GATE_UNUSED,				// simplifies finding bugs
		GATE_PULL,					// pull data from local file
		GATE_PUSH,					// push data to local file
		GATE_GETTIME,				// get absolute time info
	} type;							// type of request

	union {
		struct {
			unsigned long pos;		// position in file
			unsigned int flen;		// length of file name
			unsigned int size;		// size of buffer
		} pull;
		struct {
			unsigned long pos;		// position in file
			unsigned int flen;		// length of file name
			unsigned int size;		// size of buffer
		} push;
	} u;
};

struct gate_reply {
	enum gate_status { GATE_OK, GATE_ERROR } status;
	union {
		unsigned int size;			// size of response
		struct gate_time time;
	} u;
};

bool gate_pull(gpid_t svr, const char *file, unsigned long pos,
						/* OUT */ char *buf, /* IN/OUT */ unsigned int *size);
bool gate_push(gpid_t svr, const char *file, unsigned long pos,
						/* IN */ const char *buf, /* IN */ unsigned int size);
bool gate_gettime(gpid_t svr, /* OUT */ struct gate_time *time);

#endif // _EGOS_GATE_H
