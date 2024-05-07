#ifndef _VYT_LOCKS_H
#define _VYT_LOCKS_H
#include <threads.h>

/**
 * a writer-preferring rw lock implementation, based on:
 * https://en.m.wikipedia.org/wiki/Readers%E2%80%93writer_lock#Using_a_condition_variable_and_a_mutex
 * 
 * TODO: improve performance efficiency
 */

typedef struct {
  int num_readers_active;
  int num_writers_waiting;
  char writer_active;
  mtx_t rw_mutex;
  cnd_t rw_cond;
} rw_t;

/**
 * initialize a rw lock
 */
static inline int rw_init(rw_t *rw) {
  if (NULL == rw) return thrd_error;

  rw->num_readers_active = 0;
  rw->num_writers_waiting = 0;
  rw->writer_active = 0;

  int s = mtx_init(&rw->rw_mutex, mtx_plain);
  if (thrd_success != s) return s;

  s = cnd_init(&rw->rw_cond);
  if (thrd_success != s) {
    mtx_destroy(&rw->rw_mutex);
    return s;
  }

  return thrd_success;
}

/**
 * destroy a rw lock
 */
static inline void rw_destroy(rw_t *rw) {
  if (NULL == rw) return;

  mtx_destroy(&rw->rw_mutex);
  cnd_destroy(&rw->rw_cond);

  rw->num_readers_active = 0;
  rw->num_writers_waiting = 0;
  rw->writer_active = 0;
}

/**
 * acquire read lock
 */
static inline int rw_rlock(rw_t *rw) {
  if (NULL == rw) return thrd_error;

  mtx_lock(&rw->rw_mutex);
  while (0 < rw->num_writers_waiting || rw->writer_active)
    cnd_wait(&rw->rw_cond, &rw->rw_mutex);
  rw->num_readers_active++;
  mtx_unlock(&rw->rw_mutex);

  return thrd_success;
}

/**
 * release read lock
 */
static inline int rw_runlock(rw_t *rw) {
  if (NULL == rw) return thrd_error;

  mtx_lock(&rw->rw_mutex);
  rw->num_readers_active--;
  if (0 == rw->num_readers_active)
    cnd_signal(&rw->rw_cond);
  mtx_unlock(&rw->rw_mutex);

  return thrd_success;
}

/**
 * acquire write lock
 */
static inline int rw_wlock(rw_t *rw) {
  if (NULL == rw) return thrd_error;

  mtx_lock(&rw->rw_mutex);
  rw->num_writers_waiting++;
  while (0 < rw->num_readers_active || rw->writer_active)
    cnd_wait(&rw->rw_cond, &rw->rw_mutex);
  rw->num_writers_waiting--;
  rw->writer_active = 1;
  mtx_unlock(&rw->rw_mutex);

  return thrd_success;
}

/**
 * release write lock
 */
static inline int rw_wunlock(rw_t *rw) {
  if (NULL == rw) return thrd_error;

  mtx_lock(&rw->rw_mutex);
  rw->writer_active = 0;
  cnd_broadcast(&rw->rw_cond);
  mtx_unlock(&rw->rw_mutex);

  return thrd_success;
}

#endif // _VYT_LOCKS_H
