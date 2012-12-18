#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <math.h>

#include "spindle.h"

/* make all threads to run on the 2nd CPU */
void apply_func(void *thread, int thread_num, void *arg)
{
	cpu_set_t mask;
	pthread_t *pth = (pthread_t *)thread;

	CPU_ZERO(&mask);
	CPU_SET(1, &mask);
	pthread_setaffinity_np(*pth, sizeof(cpu_set_t), &mask);
}

void worker_func(void *arg)
{
	long n= (long) arg;
	int i, a = 1, b;

	/* just keep'em busy.. */
	for (i = 0; i < 10000000; i++) {
		a = a*i;
		b = pow(a,2);
	}
}

int main() {
	spindle_t *pool;
	spindle_barrier_t *b;
	int i;

	pool = spindle_create(10);
	spindle_apply(pool, apply_func, NULL);

	b = spindle_barrier_create();

	printf("now run `top` and press '1'\n");

	spindle_barrier_start(b);
	for (i = 0; i < 10000; i++) {
		long n = i;
		spindle_dispatch(pool, b, worker_func, (void *)n);
	}
	spindle_barrier_end(b);

	spindle_destroy(pool);
	return 0;
}
