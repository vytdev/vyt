#include <stdlib.h>
#include <stdatomic.h>
#include "exec.h"
#include "locks.h"
#include "utils.h"

// a data structure for passing parameters into threads
struct __vyt_thrdarg {
  vthrd *thr;
  vproc *proc;
};

int vpinit(vproc *proc) {
  if (NULL == proc) return VERROR;
  int stat = VOK;

  // try to initialize page table
  stat = vminit(&proc->mem);
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

  // set some variables
  atomic_store(&proc->nexec, 0);
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

int v__execunit(void *arg) {
  // get the thread info
  vthrd* thr  = ((struct __vyt_thrdarg*)arg)->thr;
  vproc *proc = ((struct __vyt_thrdarg*)arg)->proc;
  free(arg);

  // increment number of alive threads
  atomic_fetch_add(&proc->alive, 1);

  while (1) {
    // check if the runtime is still active, and this thread is still alive
    if (atomic_load(&proc->state) != VSACTIVE || !(thr->flags & VTALIVE))
      break;

    // increment active threads count
    atomic_fetch_add(&proc->active, 1);

    // TODO: decode and execute instructions here

    // decrement active threads count
    atomic_fetch_sub(&proc->active, 1);
    atomic_fetch_add(&proc->nexec, 1);

break; // tmp code

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
