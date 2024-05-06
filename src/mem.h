#ifndef _VYT_MEM_H
#define _VYT_MEM_H
#include "vyt.h"
#include "locks.h"

typedef struct {
  vqword            ndx;
  vbyte             flags;
  vbyte             *frame;
} vmpage;

typedef struct {
  vmpage            *page;
  vqword            _used;
  vqword            _alloc;
  rw_t              _lock;
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
 * initialize memory page table
 */
int vminit(vmem *mem);

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
