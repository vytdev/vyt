#include <stdlib.h>
#include <stdatomic.h>
#include <stdio.h>
#include "exec.h"
#include "locks.h"
#include "utils.h"
// include instructions
#include "inst/sys.h"
#include "inst/lod.h"
#include "inst/mov.h"
#include "inst/call.h"
#include "inst/ret.h"
#include "inst/push.h"
#include "inst/pop.h"
#include "inst/and.h"
#include "inst/or.h"
#include "inst/xor.h"
#include "inst/not.h"
#include "inst/shl.h"
#include "inst/shr.h"

// a data structure for passing parameters into threads
struct __vyt_thrdarg {
  vthrd *thr;
  vproc *proc;
};

// TODO: make this more customizable

int vpinit(vproc *proc, struct vopts *opt) {
  if (NULL == proc) return VERROR;
  int stat = VOK;

  // try to initialize page table
  stat = vminit(&proc->mem, 24);
  if (VOK != stat)
    return stat;

  // initialize the thread list
  proc->thrd = (vthrd**)malloc(sizeof(vthrd*));
  if (NULL == proc->thrd) {
    vmdestroy(&proc->mem);
    return VENOMEM;
  }

  // pre-allocate main thread ctx
  proc->thrd[0] = (vthrd*)malloc(sizeof(vthrd));
  if (NULL == proc->thrd) {
    vmdestroy(&proc->mem);
    free(proc->thrd);
    return VENOMEM;
  }

  // setup the thrd list lock
  if (0 != rw_init(&proc->_thrd_lock)) {
    vmdestroy(&proc->mem);
    free(proc->thrd);
    free(proc->thrd[0]);
    return VENOMEM;
  }

  proc->opts = opt;

  // set some variables
  atomic_store(&proc->nexec, 0);
  atomic_store(&proc->exitcode, 0);
  atomic_store(&proc->crash_tid, 0);
  atomic_store(&proc->crash_stat, 0);
  atomic_store(&proc->alive, 0);
  atomic_store(&proc->active, 0);
  proc->_thrd_used = 0;
  proc->_thrd_alloc = 1;

  atomic_store(&proc->state, VSINIT);
  return VOK;
}

int vpdestroy(vproc *proc) {
  if (NULL == proc) return VERROR;

  // check the proc ctx state
  int state = atomic_load(&proc->state);
  if (VSDONE != state && VSCRASH != state) return VERROR;

  proc->opts = NULL;

  // destroy the page table
  vmdestroy(&proc->mem);

  // de-allocate the thread list
  if (NULL != proc->thrd) {
    if (NULL != proc->thrd[0]) free(proc->thrd[0]);
    free(proc->thrd);
    proc->thrd = NULL;
  }
  // set some variables
  atomic_store(&proc->nexec, 0);
  atomic_store(&proc->exitcode, 0);
  atomic_store(&proc->crash_tid, 0);
  atomic_store(&proc->crash_stat, 0);
  atomic_store(&proc->alive, 0);
  atomic_store(&proc->active, 0);
  proc->_thrd_used = 0;
  proc->_thrd_alloc = 0;
  // destroy the thrd lock
  rw_destroy(&proc->_thrd_lock);

  atomic_store(&proc->state, VSEMPTY);
  return VOK;
}

int vload(vproc *proc, vbyte *stream, vqword sz) {
  if (NULL == proc || NULL == stream || VSINIT != atomic_load(&proc->state))
    return VERROR;

  // stream too short to fit the header
  if (13 > sz) return VEHDR;

  vbyte *d = stream;

  // check the magic number
  // 00 56 59 54 (see the spec)
  if (memcmp(d, &(char[]){ 0x00, 0x56, 0x59, 0x54 }, 4) != 0)
    return VEMAGIC;
  d += 4;

  // check the abi version
  if (v__urb(d) != VVERSION)
    return VEREV;
  d++;

  // extract the entry address
  proc->thrd[0]->reg[RIP] = v__urq(d);
  d += 8;

  // parse and process the load table
  int stat = 0;
  while (0 != *d) {
    // the size of each load table entries is 26 bytes
    if (d - stream + 26 >= sz) return VEMALF;

    vbyte type    = v__urb(d); d++;
    vbyte flags   = v__urb(d); d++;
    vqword foffst = v__urq(d); d += 8;
    vqword maddr  = v__urq(d); d += 8;
    vqword size   = v__urq(d); d += 8;

    // nothing to load
    if (0 == size) continue;

    if (foffst >= sz || foffst + size > sz) return VEMALF;

    // map pages
    vqword pgfrom = maddr >> 14;
    vqword pgto = (maddr + size - 1) >> 14;
    for ( ; pgfrom <= pgto; pgfrom++) {
      stat = vmmap(&proc->mem, pgfrom, flags);
      if (VOK != stat) return stat;
    }

    // store segment onto memory
    switch (type) {
      case VLLOAD:
        stat = vmsetd(&proc->mem, stream + foffst, maddr, size, flags & 7);
        break;
      case VLINIT:
        stat = vmfilld(&proc->mem, maddr, size, 0);
        break;
      default:
        stat = VEMALF;
    }

    if (VOK != stat) return stat;
  }

  atomic_store(&proc->state, VSLOAD);
  return VOK;
}

int vstart(vproc *proc, int *tid, vqword instptr, vqword staddr) {
  if (NULL == proc) return VERROR;

  // check the proc ctx state
  int state = atomic_load(&proc->state);
  if (VSLOAD != state && VSACTIVE != state) return VERROR;

  // allocate 'arg', passed to the new thread
  struct __vyt_thrdarg *arg = (struct __vyt_thrdarg*)malloc(
    sizeof(struct __vyt_thrdarg));
  if (NULL == arg)
    return VENOMEM;
  arg->proc = proc;

  // allocate thread ctx structure
  vthrd *thr = (vthrd*)malloc(sizeof(vthrd));
  if (NULL == thr) {
    free(arg);
    return VENOMEM;
  }

  // initialize the thread ctx
  thr->flags = VTALIVE;
  memset(&thr->reg[0], 0, sizeof(vqword) * 16);
  thr->reg[RIP] = instptr;
  thr->reg[RSP] = staddr;
  thr->reg[RBP] = staddr;
  arg->thr = thr;

  // acquire the thread list lock
  rw_wlock(&proc->_thrd_lock);

  // check if there's still some free slots in the thread list
  // +1 for the main thread (slot 0 is reserved for main)
  if (proc->_thrd_alloc > proc->_thrd_used + 1) {
    for (vqword i = 1; i < proc->_thrd_alloc; i++) {
      if (NULL == proc->thrd[i]) {

        // this slot is unused! let's use it
        proc->thrd[i] = thr;
        thr->tid = i;
        if (NULL != tid) *tid = i;
        break;

      }
    }
  }

  // the thread list is full, try re-allocate
  else {
    vthrd **tmp = (vthrd**)realloc(proc->thrd,
                                   sizeof(vthrd*) * proc->_thrd_alloc * 2);

    // failed to re-allocate the thread list
    if (NULL == tmp) {
      free(arg);
      free(thr);
      rw_wunlock(&proc->_thrd_lock);
      return VENOMEM;
    }

    // initialize new structures
    for (vqword i = 0; i < proc->_thrd_alloc; i++)
      tmp[proc->_thrd_alloc + i] = NULL;

    // the end of the last allocation is now available! use it
    tmp[proc->_thrd_alloc] = thr;
    thr->tid = proc->_thrd_alloc;
    if (NULL != tid) *tid = proc->_thrd_alloc;

    proc->_thrd_alloc *= 2;
    proc->thrd = tmp;
  }

  // try start new thread
  if (thrd_create(&thr->handle, v__execunit, arg) != 0) {
    // failed to start new thread, do cleanup and return
    proc->thrd[thr->tid] = NULL;
    free(arg);
    free(thr);

    rw_wunlock(&proc->_thrd_lock);

    // set the vm state to crashed
    atomic_store(&proc->state, VSCRASH);
    return VETHRD;
  }

  // increment the number of thread allocated on the list
  proc->_thrd_used++;

  rw_wunlock(&proc->_thrd_lock);
  atomic_store(&proc->state, VSACTIVE);

  return VOK;
}

int vrun(vproc *proc) {
  if (NULL == proc) return VERROR;

  // the vm should be already loaded
  if (atomic_load(&proc->state) != VSLOAD) return VERROR;

  int stat = VOK;

  // setup 'arg', passed to main's execution unit
  struct __vyt_thrdarg *arg = (struct __vyt_thrdarg*)malloc(
    sizeof(struct __vyt_thrdarg));
  if (NULL == arg)
    return VENOMEM;
  arg->thr = proc->thrd[0];
  arg->proc = proc;
  proc->thrd[0]->flags = VTALIVE;

  // setup main's stack
  proc->thrd[0]->reg[RSP] = MAIN_STACK_START;
  proc->thrd[0]->reg[RBP] = MAIN_STACK_START;

  // map the stack memory
  for (vqword loc = MAIN_STACK_START - 1,
       to = (MAIN_STACK_START - proc->opts->stacksz);
       loc >= to;
       loc -= VPAGESZ)
  {
    stat = vmmap(&proc->mem, loc >> 14, VPREAD | VPWRITE);
    if (VOK != stat) return stat;
  }

  // TODO: setup args

  // increment number of active threads and set the vm state to active
  proc->_thrd_used++;
  atomic_store(&proc->state, VSACTIVE);

  // run main
  v__execunit(arg);
  arg = NULL;

  // main is done, wait for other threads to finish
  while (atomic_load(&proc->state) == VSACTIVE)
    thrd_yield();

  // handle any crashes
  v__handle_crash(proc);

  return atomic_load(&proc->crash_stat);
}

int v__handle_crash(vproc *proc) {
  if (NULL == proc) return VERROR;

  // the vm is not crashed
  if (atomic_load(&proc->state) != VSCRASH) return VOK;

  // get the crashed thread
  vdword tid = atomic_load(&proc->crash_tid);
  vthrd *thr = proc->thrd[tid];
  int stat = atomic_load(&proc->crash_stat);

  // print some useful crash details
  fprintf(stderr, "unhandled critical exception\n");
  // ... cli args, program name, program args
  fprintf(stderr, "abi:       %u\n", VVERSION);
  fprintf(stderr, "tid:       %d\n", tid);
  fprintf(stderr, "mem:       %llu pages\n", proc->mem._used);
  fprintf(stderr, "code:      %d\n", stat);
  fprintf(stderr, "cause:     ");
  vperr(stat);
  fprintf(stderr, "\n");
  fprintf(stderr, "     r1  %016llx %lld\n", thr->reg[R1],  thr->reg[R1]);
  fprintf(stderr, "     r2  %016llx %lld\n", thr->reg[R2],  thr->reg[R2]);
  fprintf(stderr, "     r3  %016llx %lld\n", thr->reg[R3],  thr->reg[R3]);
  fprintf(stderr, "     r4  %016llx %lld\n", thr->reg[R4],  thr->reg[R4]);
  fprintf(stderr, "     r5  %016llx %lld\n", thr->reg[R5],  thr->reg[R5]);
  fprintf(stderr, "     r6  %016llx %lld\n", thr->reg[R6],  thr->reg[R6]);
  fprintf(stderr, "     r7  %016llx %lld\n", thr->reg[R7],  thr->reg[R7]);
  fprintf(stderr, "     r8  %016llx %lld\n", thr->reg[R8],  thr->reg[R8]);
  fprintf(stderr, "     r9  %016llx %lld\n", thr->reg[R9],  thr->reg[R9]);
  fprintf(stderr, "     rsi %016llx %lld\n", thr->reg[RSI], thr->reg[RSI]);
  fprintf(stderr, "     rdi %016llx %lld\n", thr->reg[RDI], thr->reg[RDI]);
  fprintf(stderr, "     rsp %016llx %lld\n", thr->reg[RSP], thr->reg[RSP]);
  fprintf(stderr, "     rbp %016llx %lld\n", thr->reg[RBP], thr->reg[RBP]);
  fprintf(stderr, "     rip %016llx %lld\n", thr->reg[RIP], thr->reg[RIP]);
  fprintf(stderr, "     rfl %016llx %lld\n", thr->reg[RFL], thr->reg[RFL]);
  fprintf(stderr, "\n");

  // free the crashed thread ctx
  free(thr);
  proc->thrd[tid] = NULL;
  return VOK;
}

int v__execunit(void *arg) {
  // get the thread info
  vthrd* thr  = ((struct __vyt_thrdarg*)arg)->thr;
  vproc *proc = ((struct __vyt_thrdarg*)arg)->proc;
  free(arg);

  // increment number of alive threads
  atomic_fetch_add(&proc->alive, 1);
  int stat = VOK;
  vbyte buf[23];

  while (1) {
    // check if there's no error in last execution, the runtime is still active,
    // and this thread is still alive
    if (
      VOK != stat ||
      atomic_load(&proc->state) != VSACTIVE ||
      !(thr->flags & VTALIVE)
    ) break;
    // increment active threads count
    atomic_fetch_add(&proc->active, 1);

    // decode the instruction here
    stat = vmgetd(&proc->mem, buf, thr->reg[RIP], 3, VPREAD | VPEXEC);
    if (VOK != stat) {
      atomic_fetch_sub(&proc->active, 1);
      break;
    }
    // get the opcode
    vword opcode = v__urw(buf);
    // get the modeb, +2 for the opcode
    vbyte modeb = v__urb(buf + 2);
    vbyte wsz   = modeb & 3;
    vbyte mop1  = (modeb >> 2) & 7;
    vbyte mop2  = (modeb >> 5) & 7;
    // get the operands
    vbyte op1sz = v__opsz(mop1, wsz);
    vbyte op2sz = v__opsz(mop2, wsz);
    stat = vmgetd(&proc->mem,
                  buf + 3,
                  thr->reg[RIP] + 3,
                  op1sz + op2sz,
                  VPREAD | VPEXEC);
    if (VOK != stat) {
      atomic_fetch_sub(&proc->active, 1);
      break;
    }
    vbyte *op1 = buf + 3;
    vbyte *op2 = buf + 3 + op1sz;
    // update the program counter
    // - 3     - opcode and wordsize
    // - op1sz - size of the first opcode
    // - op2sz - size of the second opcode
    thr->reg[RIP] += 3 + op1sz + op2sz;

    // switch though opcodes
    switch (opcode) {
#define ICALL(opcode, mnemonic)                             \
  case opcode: stat = VINST_##mnemonic(                     \
    proc, thr, wsz, mop1, op1sz, op1, mop2, op2sz, op2      \
  ); break

      ICALL(0x0001, sys);
      ICALL(0x0002, lod);
      ICALL(0x0003, mov);
      ICALL(0x0004, call);
      ICALL(0x0005, ret);
      ICALL(0x0006, push);
      ICALL(0x0007, pop);
      ICALL(0x0008, and);
      ICALL(0x0009, or);
      ICALL(0x000a, xor);
      ICALL(0x000b, not);
      ICALL(0x000c, shl);
      ICALL(0x000d, shr);

#undef ICALL
      default: stat = VEINST;
    }

    // decrement active threads count
    atomic_fetch_sub(&proc->active, 1);
    atomic_fetch_add(&proc->nexec, 1);
  }

  // error occured, crash the vm!
  if (VOK != stat) {
    atomic_store(&proc->state, VSCRASH);
    atomic_store(&proc->crash_tid, thr->tid);
    atomic_store(&proc->crash_stat, stat);
    return stat;
  }

  // do some clean-ups
  rw_wlock(&proc->_thrd_lock);
  proc->thrd[thr->tid] = NULL;
  proc->_thrd_used--;
  if (0 == proc->_thrd_used) atomic_store(&proc->state, VSDONE);
  rw_wunlock(&proc->_thrd_lock);

  // free thread ctx
  free(thr);

  // decrement number of alive threads
  atomic_fetch_sub(&proc->alive, 1);
  return VOK;
}
