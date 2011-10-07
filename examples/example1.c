#include <stdio.h>
#include <unistd.h>
#include <spindle.h>


void worker_func(void *arg)
{
	long n= (long) arg;

	/* yeah, EACH worker sleeps for 1 second, but nevertheless the whole example runs only 1 second */
	sleep(1);

	printf("worker #%ld is done\n", n);
}


int main() {
	spindle_t *pool;
	spindle_barrier_t *b;
	int i;

	pool = spindle_create(10);
	b = spindle_barrier_create();

	spindle_barrier_start(b);
	for (i = 0; i < 10; i++) {
		long n = i;
		spindle_dispatch(pool, b, worker_func, (void *)n);
	}
	spindle_barrier_end(b);

	printf("this message is printed after all the workers have finished their jobs\n");
	spindle_destroy(pool);
	return 0;
}
