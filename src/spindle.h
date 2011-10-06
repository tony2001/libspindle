#ifndef SPINDLE_H
# define SPINDLE_H

#include "spindle_version.h"

typedef void* spindle_t;
typedef void* spindle_barrier_t;
typedef void (*spindle_job_func_t)(void *);

/**
 * Creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL.
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
 * 
 * The dispatched thread calls into the function
 * "dispatch_to_here" with argument "arg".
 */
#define spindle_dispatch(from, barrier, to, arg) spindle_dispatch_with_cleanup((from), (barrier), (to), (arg), NULL, NULL)

/**
 * Kills the threadpool, causing
 * all threads in it to commit suicide, and then
 * frees all the memory associated with the threadpool.
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


void spindle_barrier_init(spindle_barrier_t *b);
int spindle_barrier_start(spindle_barrier_t *b);
void spindle_barrier_wait(spindle_barrier_t *b);
void spindle_barrier_end(spindle_barrier_t *b);
void spindle_barrier_destroy(spindle_barrier_t *b);

#endif /* ifndef SPINDLE_H */
