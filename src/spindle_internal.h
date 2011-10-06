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

/* maximum number of threads allowed in a pool */
#define SPINDLE_MAX_IN_POOL 200
#define MAX_QUEUE_MEMORY_SIZE 65536

#ifdef SPINDLE_DEBUG
# define TP_DEBUG(pool, ...) etfprintf((pool)->created, stderr, __VA_ARGS__);
#else
# define TP_DEBUG(pool, ...)
#endif

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
	pthread_cond_t  var;
	int posted_count;
	int done_count;
} spindle_barrier_int_t;

typedef struct _spindle_t {
#ifdef SPINDLE_DEBUG
	struct timeval  created;    /* When the threadpool was created.*/
#endif
	pthread_t      *threads;       /* The threads themselves.*/
	pthread_mutex_t mutex;      /* protects all vars declared below.*/
	int             size;       /* Number of threads in the pool*/
	int             live;       /* Number of live threads in pool (when*/
	/*   pool is being destroyed, live<=arrsz)*/

	pthread_cond_t  job_posted; /* dispatcher: "Hey guys, there's a job!"*/
	pthread_cond_t  job_taken;  /* a worker: "Got it!"*/

	spindle_queue_head_t      *job_queue;      /* queue of work orders*/
} spindle_int_t;

struct _spindle_queue_node_t
{
  spindle_job_func_t func_to_dispatch;
  void *func_arg;
  spindle_job_func_t cleanup_func;
  void *cleanup_arg;
  spindle_barrier_int_t *barrier;
  spindle_queue_node_t *next;
  spindle_queue_node_t *prev;
};

