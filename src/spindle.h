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

#include "spindle_version.h"

/* main data struct of the thread pool, returned by spindle_create() */
typedef void* spindle_t;

/* thread barrier struct, initialized by spindle_barrier_init() and destroyed by spindle_barrier_destroy() */
typedef void* spindle_barrier_t;

/* general purpose job function */
typedef void (*spindle_job_func_t)(void *);

/* thread apply function, first argument is a pointer to pthread_t */
typedef void (*spindle_apply_func_t)(void *thread, int thread_num, void *arg);

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


#endif /* ifndef SPINDLE_H */
