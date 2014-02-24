#ifndef SPINDLE_H
# define SPINDLE_H

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

#include <pthread.h>
#include "spindle_version.h"

/* general purpose job function */
typedef void (*spindle_job_func_t)(void *);

/* thread apply function, first argument is a pointer to pthread_t */
typedef void (*spindle_apply_func_t)(void *thread, int thread_num, void *arg);

typedef struct _spindle_queue_node_t spindle_queue_node_t;

typedef struct _spindle_queue_head_t
{
	spindle_queue_node_t *head;
	spindle_queue_node_t *tail;
	spindle_queue_node_t *free_head;
	spindle_queue_node_t *free_tail;
	int capacity;
	int max_capacity;
	int posted;
} spindle_queue_head_t;

typedef struct _spindle_barrier_t {
	pthread_mutex_t mutex;
	pthread_cond_t var;
	volatile int posted_count;
	volatile int done_count;
} spindle_barrier_t;

typedef struct _spindle_t {
#ifdef SPINDLE_DEBUG
	struct timeval  created;    /* When the threadpool was created.*/
#endif
	pthread_t      *threads;       /* The threads themselves.*/
	pthread_mutex_t mutex;      /* protects all vars declared below.*/
	int             size;       /* Number of threads in the pool*/
	int             live;       /* Number of live threads in pool (when pool is being destroyed, live<=size) */

	pthread_cond_t  job_posted; /* dispatcher: "Hey guys, there's a job!"*/
	pthread_cond_t  job_taken;  /* a worker: "Got it!"*/

	spindle_queue_head_t      *job_queue;      /* queue of work orders*/
} spindle_t;

struct _spindle_queue_node_t
{
  spindle_job_func_t func_to_dispatch;
  void *func_arg;
  spindle_job_func_t cleanup_func;
  void *cleanup_arg;
  spindle_barrier_t *barrier;
  spindle_queue_node_t *next;
  spindle_queue_node_t *prev;
};

/**
 * Creates a fixed-sized thread pool.
 * If the function succeeds, it returns a non-NULL pointer to the pool struct, else it returns NULL.
 */
spindle_t *spindle_create(int num_threads_in_pool);

/**
 * Sends a thread off to do some work.  If all threads in the pool are busy, dispatch will
 * block until a thread becomes free and is dispatched.
 *
 * Once a thread is dispatched, this function returns immediately.
 *
 * Also enables the user to define cleanup handlers in
 * cases of immediate cancel.  The cleanup handler function (cleaner_func) is
 * executed automatically with cleaner_arg as the argument after the
 * dispatch_to_here function is done; in the case that the thread is destroyed,
 * cleaner_func is also called before the thread exits.
 */
void spindle_dispatch_with_cleanup(spindle_t *from_me, spindle_barrier_t *barrier, spindle_job_func_t dispatch_to_here, void * arg, spindle_job_func_t cleaner_func, void* cleaner_arg);

/**
 * The dispatched thread calls into the function
 * "dispatch_to_here" with argument "arg".
 */
#define spindle_dispatch(from, barrier, to, arg) spindle_dispatch_with_cleanup((from), (barrier), (to), (arg), NULL, NULL)

/**
 * Apply a function to all threads in the pool.
 * */
void spindle_apply(spindle_t *p, spindle_apply_func_t func, void *arg);

/**
 * Kills the threadpool, causing all threads in it to commit suicide,
 * and then frees all the memory associated with the threadpool.
 */
void spindle_destroy(spindle_t *destroyme);

/**
 * Cancels all threads in the threadpool immediately.
 * It is potentially dangerous to use with libraries that are not specifically
 * asynchronous cancel thread safe. Also, without cleanup handlers, any dynamic
 * memory or system resource in use could potentially be left without a reference
 * to it.
 */
void spindle_destroy_immediately(spindle_t *destroymenow);

/**
 * Creates and initializes barrier struct.
 */
spindle_barrier_t *spindle_barrier_create();

/**
 * Starts the barrier, after that point all calls to spindle_dispatch*() have to use this barrier.
 */
int spindle_barrier_start(spindle_barrier_t *b);

/**
 * Waits for the threads to finish their jobs and continues after all of the workers have finished.
 */
void spindle_barrier_wait(spindle_barrier_t *b);

/**
 * Destroys and frees the barrier internal struct.
 */
void spindle_barrier_destroy(spindle_barrier_t *b);

/**
 * A shortcut for _wait() + _destroy()
 */
void spindle_barrier_end(spindle_barrier_t *b);

int spindle_queue_get_posted(spindle_t *p);

#endif /* ifndef SPINDLE_H */
