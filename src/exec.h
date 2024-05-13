#ifndef _VYT_EXEC_H
#define _VYT_EXEC_H
#include <threads.h>
#include <stdatomic.h>
#include "vyt.h"
#include "mem.h"
#include "locks.h"
#include "utils.h"

/* constants */
#define MAIN_STACK_START ((vqword)1<<63)

struct vopts {
  vqword            stacksz;          /* main's stack size */
};

typedef struct {
  vdword            tid;
  thrd_t            handle;
  vbyte             flags;
  vqword            reg[16];
} vthrd;

typedef struct {
  struct vopts      *opts;

  _Atomic vqword    nexec;
  _Atomic int       state;
  _Atomic int       exitcode;
  _Atomic vdword    crash_tid;
  _Atomic int       crash_stat;

  vmem              mem;

  vthrd             **thrd;
  _Atomic vdword    alive;
  _Atomic vdword    active;
  vdword            _thrd_used;
  vdword            _thrd_alloc;
  rw_t              _thrd_lock;
} vproc;

/* process states */
#define VSEMPTY     0
#define VSINIT      1
#define VSLOAD      2
#define VSACTIVE    3
#define VSDONE      4
#define VSCRASH     5

/* thread flags */
#define VTALIVE     0x1   /* the thread is alive */

/* load type */
#define VLLOAD      0x1   /* load from payload */
#define VLINIT      0x2   /* zero-initialize */

/* registers */
#define R1          0x1
#define R2          0x2
#define R3          0x3
#define R4          0x4
#define R5          0x5
#define R6          0x6
#define R7          0x7
#define R8          0x8
#define R9          0x9
#define RSI         0xa
#define RDI         0xb
#define RSP         0xc
#define RBP         0xd
#define RIP         0xe
#define RFL         0xf

/* operand wordsize */
#define WBYTE       0x0
#define WWORD       0x1
#define WDWORD      0x2
#define WQWORD      0x3

/* opcode argument modes */
#define DNONE       0x0
#define DIMMED      0x1
#define DREG        0x2
#define DRELADDR    0x3
#define DABSADDR    0x4
#define DDYNADDR    0x5

/* rfl register flags */
#define RFL_CF      (1)
#define RFL_ZF      (1<<1)
#define RFL_SF      (1<<2)
#define RFL_OF      (1<<3)

/**
 * initialize the given process context
 */
int vpinit(vproc *proc, struct vopts *opt);

/**
 * destroy the given process context
 */
int vpdestroy(vproc *proc);

/**
 * load program into given process context
 */
int vload(vproc *proc, vbyte *stream, vqword sz);

/**
 * make a new thread
 */
int vstart(vproc *proc, int *tid, vqword instptr, vqword staddr);

/**
 * start the main thread and the whole virtual machine process
 */
int vrun(vproc *proc);

/**
 * handle vm crash
 */
int v__handle_crash(vproc *proc);

/**
 * internal: thread execution unit
 */
int v__execunit(void *arg);

/* returns the data size in bytes from given wordsize */
static inline int v__wsz(vbyte wsz) {
  switch (wsz) {
    case WBYTE:   return 1;
    case WWORD:   return 2;
    case WDWORD:  return 4;
    case WQWORD:  return 8;
    default:      return 0;
  }
}

/* returns the size of an operand in the instruction */
static inline int v__opsz(vbyte opmode, vbyte wsz) {
  switch (opmode) {
    case DNONE:                     return 0;
    case DIMMED:                    return v__wsz(wsz);
    case DREG:                      return 1;
    case DRELADDR: case DABSADDR:   return 8;
    case DDYNADDR:                  return 10;
    default:                        return 0;
  }
}

/* read a number based wordsize */
static inline vqword v__rnum(vbyte wsz, vbyte *data) {
  switch (wsz) {
    case WBYTE:   return (vqword)v__urb(data);
    case WWORD:   return (vqword)v__urw(data);
    case WDWORD:  return (vqword)v__urd(data);
    case WQWORD:  return v__urq(data);
    default:      return 0;
  }
}

/* resolve memory address */
static inline vqword v__maddr(vbyte opmode, vbyte *op, vthrd *thr) {
  switch (opmode) {
    case DRELADDR:
      return thr->reg[RIP] + v__urq(op);
    case DABSADDR:
      return v__urq(op);
    case DDYNADDR:
      return thr->reg[op[0] & 4] +  // base
             op[1] *                // scale
             thr->reg[op[0] >> 4] + // index
             v__urq(op + 2);        // displacement
    default:
      return 0;
  }
}

/* push bytes to a thread's stack */
static inline int vstpush(vproc *proc, vthrd *thr, vbyte *data, vqword sz) {
  thr->reg[RSP] -= sz;
  return vmsetd(&proc->mem, data, thr->reg[RSP], sz, VPWRITE);
}

/* pop bytes from a thread's stack */
static inline int vstpop(vproc *proc, vthrd *thr, vbyte *data, vqword sz) {
  int stat = vmgetd(&proc->mem, data, thr->reg[RSP], sz, VPREAD);
  thr->reg[RSP] += sz;
  return stat;
}

/* set given flag(s) */
static inline void vfset(vthrd *thr, vqword flag, char val) {
  if (val) thr->reg[RFL] |= flag;
  else     thr->reg[RFL] &= ~flag;
}

/* return whether a flag is set */
static inline int vfget(vthrd *thr, vqword flag) {
  if (thr->reg[RFL] & flag) return 1;
  return 0;
}

#endif // _VYT_EXEC_H
