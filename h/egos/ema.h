#ifndef _EGOS_EMA_H
#define _EGOS_EMA_H

#include <stdbool.h>

#ifdef EMA

struct ema_state {
	bool first;
	double alpha, ema, last_sample;
	unsigned long last_time;
};

void ema_init(struct ema_state *es, double alpha);
void ema_update(struct ema_state *es, double sample);
double ema_avg(struct ema_state *es);

#endif // EMA

#endif // _EGOS_EMA_H
