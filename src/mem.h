#ifndef _VYT_MEM_H
#define _VYT_MEM_H
#include <stddef.h>
#include "vyt.h"
#include "locks.h"

typedef struct {
  vqword            ndx;
  vbyte             flags;
  vbyte             *frame;
} vmpage;

typedef struct _cache_entry_s {
  int               ndx;
  uintptr_t         offst; /* vmem->page + ent->offst */
  struct _cache_entry_s *prev;
  struct _cache_entry_s *next;
} _vmem_cache;

typedef struct {
  vmpage            *page;
  vqword            _used;
  vqword            _alloc;
  rw_t              _lock;

  /* used in caching */
  _vmem_cache       *cache_pool;
  vword             _cache_size;
  vword             _cache_used;
  _vmem_cache       *_cache_head;
  _vmem_cache       *_cache_tail;
  fmtx_t            _cache_lock;
} vmem;

/* some constants */
#define VPAGESZ     16384
#define VPAGEMX     0x3ffffffffffff

/* page flags */
#define VPREAD      (1)
#define VPWRITE     (1<<1)
#define VPEXEC      (1<<2)
#define VPOWNED     (1<<3)

/**
 * initialize memory page table and the cache by 'cachesz' entries. set
 * 'cachesz' to 0 to disable caching
 */
int vminit(vmem *mem, vword cachesz);

/**
 * destroy memory page table
 */
int vmdestroy(vmem *mem);

/**
 * map memory at given index
 */
int vmmap(vmem *mem, vqword ndx, vbyte flags);

/**
 * un-map memory at given index
 */
int vmunmap(vmem *mem, vqword ndx);

/**
 * find page from memory, at given the index
 */
int vmgetp(vmem *mem, vqword ndx, vmpage **out);

/**
 * get 'sz' bytes from memory at 'addr' and write it onto 'out'
 */
int vmgetd(vmem *mem, vbyte *out, vqword addr, vqword sz, vbyte perm);

/**
 * set 'sz' bytes to memory at 'addr' from the buffer 'in'
 */
int vmsetd(vmem *mem, vbyte *in, vqword addr, vqword sz, vbyte perm);

/**
 * fill 'sz' bytes of memory at 'addr' with a constant byte 'c'
 */
int vmfilld(vmem *mem, vqword addr, vqword sz, vbyte c);

#endif // _VYT_MEM_H
