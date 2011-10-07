/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* this code is heavily influenced by this file: 
 * http://people.clarkson.edu/~jmatthew/cs644.archive/cs644.fa2001/proj/locksmith/code/ExampleTest/threadpool.c
 * but also contains a lot of modifications and improvements
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>

#include "spindle_config.h"
#include "spindle.h"
#include "spindle_internal.h"

/* {{{ internal funcs and stuff */

static inline spindle_queue_head_t *queue_create(int initial_cap) /* {{{ */
{
	spindle_queue_head_t *job_queue = (spindle_queue_head_t *) malloc (sizeof(spindle_queue_head_t));
	int max_cap = MAX_QUEUE_MEMORY_SIZE / (sizeof(spindle_queue_node_t));
	int i;
	spindle_queue_node_t *temp;

	if (job_queue == NULL) {
		return NULL;
	}

	if (initial_cap > max_cap) {
		initial_cap = max_cap;
	}

	if (initial_cap == 0) {
		return NULL;
	}

	job_queue->capacity = initial_cap;
	job_queue->posted = 0;
	job_queue->max_capacity = max_cap;
	job_queue->head = NULL;
	job_queue->tail = NULL;
	job_queue->free_head = (spindle_queue_node_t *) malloc (sizeof(spindle_queue_node_t));

	if (job_queue->free_head == NULL) {
		free(job_queue);
		return NULL;
	}
	job_queue->free_tail = job_queue->free_head;

	/* populate the free queue */
	for(i = 0; i< initial_cap; i++) {
		temp = (spindle_queue_node_t *) malloc (sizeof(spindle_queue_node_t));
		if (temp == NULL) {
			return job_queue;
		}
		temp->next = job_queue->free_head;
		temp->prev = NULL;
		job_queue->free_head->prev = temp;
		job_queue->free_head = temp;
	}

	return job_queue;
}
/* }}} */

static inline void queue_destroy(spindle_queue_head_t *queue) /* {{{ */
{
	spindle_queue_node_t *node, *temp;

	temp = queue->head;
	while (temp) {
		node = temp;
		temp = node->next;
		free(node);
	}
	
	temp = queue->free_head;
	while (temp) {
		node = temp;
		temp = node->next;
		free(node);
	}
	free(queue);
}
/* }}} */

static inline void queue_post_job(spindle_queue_head_t * job_queue, spindle_barrier_int_t *barrier, spindle_job_func_t func1, void * arg1, spindle_job_func_t func2, void * arg2) /* {{{ */
{
	spindle_queue_node_t *temp;
		
	if (job_queue->free_tail == NULL) {
	    temp = (spindle_queue_node_t *) malloc (sizeof(spindle_queue_node_t));
		if (temp == NULL) {
			return;
		}
		temp->next = NULL;
		temp->prev = NULL;
		job_queue->free_head = temp;
		job_queue->free_tail = temp;
		job_queue->capacity++;
	}
	
	temp = job_queue->free_tail;
	if (job_queue->free_tail->prev == NULL) {
		job_queue->free_tail = NULL;
		job_queue->free_head = NULL;
	} else {
		job_queue->free_tail->prev->next= NULL;
		job_queue->free_tail = job_queue->free_tail->prev;
		job_queue->free_tail->next = NULL;
	}

	job_queue->posted++;
	temp->func_to_dispatch = func1;
	temp->func_arg = arg1;
	temp->cleanup_func = func2;
	temp->cleanup_arg = arg2;
	temp->barrier = barrier;
	if (barrier) {
		barrier->posted_count++;
	}

	temp->prev = job_queue->tail;
	temp->next = NULL;

	if (job_queue->tail) {
		job_queue->tail->next = temp;
	} else {
		job_queue->head = temp;
	}
	job_queue->tail = temp;
}
/* }}} */

static inline void queue_fetch_job(spindle_queue_head_t * job_queue, spindle_barrier_int_t **barrier, spindle_job_func_t * func1, void ** arg1, spindle_job_func_t * func2, void ** arg2) /* {{{ */
{
	spindle_queue_node_t *temp;
		
	temp = job_queue->tail;
	if (temp == NULL) {
		return;
	}

	if (temp->prev) {
		temp->prev->next = NULL;
	} else {
		job_queue->head = NULL;
	}
	job_queue->tail = temp->prev;

	*func1 = temp->func_to_dispatch;
	*arg1  = temp->func_arg;
	*func2 = temp->cleanup_func;
	*arg2  = temp->cleanup_arg;
	*barrier = temp->barrier;

	temp->next = NULL;
	temp->prev = NULL;
	if (job_queue->free_head == NULL) {
		job_queue->free_tail = temp;
		job_queue->free_head = temp;
	} else {
		temp->next = job_queue->free_head;
		job_queue->free_head->prev = temp;
		job_queue->free_head = temp;
	}
}
/* }}} */

static inline int queue_can_accept_order(spindle_queue_head_t * job_queue) /* {{{ */
{
	return (job_queue->free_tail != NULL || job_queue->capacity <= job_queue->max_capacity);
}
/* }}} */

static inline int queue_is_job_available(spindle_queue_head_t * job_queue) /* {{{ */
{
  return (job_queue->tail != NULL);
}
/* }}} */


#ifdef SPINDLE_DEBUG
static inline void etfprintf(struct timeval then, ...) /* {{{ */
{
	va_list         args;
	struct timeval  now;
	struct timeval  diff;
	FILE           *out;
	char           *fmt;

	va_start(args, then);
	out = va_arg(args, FILE *);
	fmt = va_arg(args, char *);

	gettimeofday(&now, NULL);
	timersub(&now, &then, &diff);
	fprintf(out, "%03d.%06d:", (int)diff.tv_sec, (int)diff.tv_usec);
	vfprintf(out, fmt, args);
	fflush(NULL);
	va_end(args);
}
/* }}} */
#endif

static void spindle_mutex_unlock_wrapper(void *data) /* {{{ */
{
	pthread_mutex_t *mutex = (pthread_mutex_t *)data;
	pthread_mutex_unlock(mutex);
}
/* }}} */

static void spindle_barrier_signal(spindle_barrier_int_t *b) /* {{{ */
{
	pthread_mutex_lock(&b->mutex);
	b->done_count++;
	pthread_cond_signal(&b->var);
	pthread_mutex_unlock(&b->mutex);
}
/* }}} */

static void *th_do_work(void *data) /* {{{ */
{
	spindle_int_t *pool = (spindle_int_t *)data; 
	spindle_barrier_int_t *barrier;
#ifdef SPINDLE_DEBUG
	int myid = pool->live; /* this is wrong, but we need it only for debugging */
#endif
	
	/* When we get a posted job, we copy it into these local vars */
	spindle_job_func_t  myjob;
	void        *myarg;  
	spindle_job_func_t  mycleaner;
	void        *mycleanarg;

	TP_DEBUG(pool, " >>> Thread[%d] starting, grabbing mutex.\n", myid);

	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
	pthread_cleanup_push(spindle_mutex_unlock_wrapper, (void *)&pool->mutex);

	/* Grab mutex so we can begin waiting for a job */
	if (0 != pthread_mutex_lock(&pool->mutex)) {
		return NULL;
	}

	/* Main loop: wait for job posting, do job(s) ... forever */
	for( ; ; ) {

		TP_DEBUG(pool, " <<< Thread[%d] waiting for signal.\n", myid);

		/* only look for jobs if we're not in shutdown */
		while(queue_is_job_available(pool->job_queue) == 0) {
			pthread_cond_wait(&pool->job_posted, &pool->mutex);
		}

		TP_DEBUG(pool, " >>> Thread[%d] received signal.\n", myid);

		/* else execute all the jobs first */
		queue_fetch_job(pool->job_queue, &barrier, &myjob, &myarg, &mycleaner, &mycleanarg);
		if (myjob == (void *)-1) {
			break;
		}
		pthread_cond_signal(&pool->job_taken);

		TP_DEBUG(pool, " <<< Thread[%d] yielding mutex, taking job.\n", myid);

		/* unlock mutex so other jobs can be posted */
		if (0 != pthread_mutex_unlock(&pool->mutex)) {
			return NULL;
		}

		/* Run the job we've taken */
		if(mycleaner != NULL) {
			pthread_cleanup_push(mycleaner,mycleanarg);
			myjob(myarg);
			pthread_cleanup_pop(1);
		} else {
			myjob(myarg);
		}

		TP_DEBUG(pool, " >>> Thread[%d] JOB DONE!\n", myid);
		if (barrier) {
			/* Job done! */
			spindle_barrier_signal(barrier);
		}

		/* Grab mutex so we can grab posted job, or (if no job is posted)
		   begin waiting for next posting. */
		if (0 != pthread_mutex_lock(&pool->mutex)) {
			return NULL;
		}
	}

	/* If we get here, we broke from loop because state is ALL_EXIT */
	--pool->live;

	TP_DEBUG(pool, " <<< Thread[%d] exiting (signalling 'job_taken').\n", myid);

	/* We're not really taking a job ... but this signals the destroyer
	   that one thread has exited, so it can keep on destroying. */
	pthread_cond_signal(&pool->job_taken);

	pthread_cleanup_pop(0);
	pthread_mutex_unlock(&pool->mutex);
	return NULL;
}  
/* }}} */

/* }}} */

spindle_t *spindle_create(int num_threads_in_pool) /* {{{ */
{
	spindle_int_t *pool;
	int i;

	if (num_threads_in_pool <= 0 || num_threads_in_pool > SPINDLE_MAX_IN_POOL) {
		return NULL;
	}

	pool = (spindle_int_t *) malloc(sizeof(spindle_int_t));
	if (pool == NULL) {
		return NULL;
	}

	pthread_mutex_init(&(pool->mutex), NULL);
	pthread_cond_init(&(pool->job_posted), NULL);
	pthread_cond_init(&(pool->job_taken), NULL);
	pool->size = num_threads_in_pool;
	pool->job_queue = queue_create(num_threads_in_pool);
#ifdef SPINDLE_DEBUG
	gettimeofday(&pool->created, NULL);
#endif

	pool->threads = (pthread_t *) malloc(pool->size * sizeof(pthread_t));
	if (NULL == pool->threads) {
		free(pool);
		return NULL;
	}

	pool->live = 0;
	for (i = 0; i < pool->size; i++) {
		if (0 != pthread_create(pool->threads + i, NULL, th_do_work, (void *) pool)) {
			free(pool->threads);
			free(pool);
			return NULL;
		}
		pool->live++;
		pthread_detach(pool->threads[i]);
	}

	TP_DEBUG(pool, " <<< Threadpool created with %d threads.\n", num_threads_in_pool);

	return (spindle_t *)pool;
}
/* }}} */

void spindle_dispatch_with_cleanup(spindle_t *from_me, spindle_barrier_t *barrier, spindle_job_func_t dispatch_to_here, void *arg, spindle_job_func_t cleaner_func, void * cleaner_arg) /* {{{ */
{
	spindle_int_t *pool = (spindle_int_t *) from_me;
	spindle_barrier_int_t *barrier_int = (spindle_barrier_int_t *)barrier;
	
	pthread_cleanup_push(spindle_mutex_unlock_wrapper, (void *) &pool->mutex);
	TP_DEBUG(pool, " >>> Dispatcher: grabbing mutex.\n");

	if (0 != pthread_mutex_lock(&pool->mutex)) {
		TP_DEBUG(pool, " >>> Dispatcher: failed to lock mutex!\n");
		return;
	}

	while(!queue_can_accept_order(pool->job_queue)) {
		TP_DEBUG(pool, " <<< Dispatcher: job queue full, %s\n", "signaling 'posted', waiting on 'taken'.");
		pthread_cond_signal(&pool->job_posted);
		pthread_cond_wait(&pool->job_taken, &pool->mutex);
	}

	/* Finally, there's room to post a job. Do so and signal workers */
	TP_DEBUG(pool, " <<< Dispatcher: posting job, signaling 'posted', yielding mutex\n");
	queue_post_job(pool->job_queue, barrier_int, dispatch_to_here, arg, cleaner_func, cleaner_arg);

	pthread_cond_signal(&pool->job_posted);

	pthread_cleanup_pop(0);
	/* unlock mutex so a worker can pick up the job */
	pthread_mutex_unlock(&pool->mutex);
}
/* }}} */

void spindle_destroy(spindle_t *destroyme) /* {{{ */
{
	spindle_int_t *pool = (spindle_int_t *) destroyme;
	int oldtype;

	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldtype);
	pthread_cleanup_push(spindle_mutex_unlock_wrapper, (void *) &pool->mutex); 

	/* Cause all threads to exit. Because they were detached when created,
	   the underlying memory for each is automatically reclaimed. */
	TP_DEBUG(pool, " >>> Destroyer: grabbing mutex, posting exit job. Live = %d\n", pool->live);

	if (0 != pthread_mutex_lock(&pool->mutex)) {
		return;
	}

	while (pool->live > 0) {
		TP_DEBUG(pool, " <<< Destroyer: signalling 'job_posted', waiting on 'job_taken'.\n");
		queue_post_job(pool->job_queue, NULL, (spindle_job_func_t) -1, NULL, NULL, NULL);
		/* get workers to check in ... */
		pthread_cond_signal(&pool->job_posted);
		/* ... and wake up when they check out */
		pthread_cond_wait(&pool->job_taken, &pool->mutex);
		TP_DEBUG(pool, " >>> Destroyer: received 'job_taken'. Live = %d\n", pool->live);
	}

	memset(pool->threads, 0, pool->size * sizeof(pthread_t));
	free(pool->threads);

	TP_DEBUG(pool, " <<< Destroyer: releasing mutex prior to destroying it.\n");

	pthread_cleanup_pop(0);  
	if (0 != pthread_mutex_unlock(&pool->mutex)) {
		return;
	}

	TP_DEBUG(pool, " --- Destroyer: destroying mutex.\n");

	if (0 != pthread_mutex_destroy(&pool->mutex)) {
		return;
	}

	TP_DEBUG(pool, " --- Destroyer: destroying conditional variables.\n");

	if (0 != pthread_cond_destroy(&pool->job_posted)) {
		return;
	}

	if (0 != pthread_cond_destroy(&pool->job_taken)) {
		return;
	}

	queue_destroy(pool->job_queue);
	memset(pool, 0, sizeof(spindle_int_t));

	free(pool);
	pool = NULL;
	destroyme = NULL;
}
/* }}} */

void spindle_destroy_immediately(spindle_t *destroymenow) /* {{{ */
{
	spindle_int_t *pool = (spindle_int_t *) destroymenow;
	int oldtype,i;

	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldtype);

	pthread_cleanup_push(spindle_mutex_unlock_wrapper, (void *) &pool->mutex);
	pthread_mutex_lock(&pool->mutex);


	for(i = 0; i < pool->size; i++) {
		pthread_cancel(pool->threads[i]);
	}

	TP_DEBUG(pool, " --- Destroyer: destroying mutex.\n");

	pthread_cleanup_pop(0);

	if (0 != pthread_mutex_destroy(&pool->mutex)) {
		TP_DEBUG(pool, " --- failed to destroy mutex: %s (%d)\n", strerror(errno), errno);
	}

	TP_DEBUG(pool, " --- Destroyer: destroying conditional variables.\n");

	pthread_cond_destroy(&pool->job_posted);
	pthread_cond_destroy(&pool->job_taken);
	
	memset(pool, 0, sizeof(spindle_int_t));
	free(pool);
	pool = NULL;
	destroymenow = NULL;
}
/* }}} */

spindle_barrier_t *spindle_barrier_create(void) /* {{{ */
{
	spindle_barrier_int_t *barrier;

	barrier = malloc(sizeof(spindle_barrier_int_t));

	if (!barrier) {
		return NULL;
	}

	pthread_mutex_init(&barrier->mutex, NULL);
	pthread_cond_init(&barrier->var, NULL);
	barrier->posted_count = 0;
	barrier->done_count = 0;
	return barrier;
}
/* }}} */

int spindle_barrier_start(spindle_barrier_t *b) /* {{{ */
{
	spindle_barrier_int_t *barrier = (spindle_barrier_int_t *)b;

	barrier->posted_count = 0;
	barrier->done_count = 0;
	return 0;
}
/* }}} */

void spindle_barrier_wait(spindle_barrier_t *b) /* {{{ */
{
	spindle_barrier_int_t *barrier = (spindle_barrier_int_t *)b;

	pthread_mutex_lock(&barrier->mutex);
	while (barrier->done_count < barrier->posted_count) {
		pthread_cond_wait(&barrier->var, &barrier->mutex);
	}
	pthread_mutex_unlock(&barrier->mutex);
}
/* }}} */

void spindle_barrier_destroy(spindle_barrier_t *b) /* {{{ */
{
	spindle_barrier_int_t *barrier = (spindle_barrier_int_t *)b;

	pthread_mutex_destroy(&barrier->mutex);
	pthread_cond_destroy(&barrier->var);
	free(barrier);
	b = NULL;
}
/* }}} */

void spindle_barrier_end(spindle_barrier_t *b) /* {{{ */
{
	spindle_barrier_wait(b);
	spindle_barrier_destroy(b);
}
/* }}} */

