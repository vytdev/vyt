#include <stdlib.h>
#include <stdatomic.h>
#include "exec.h"
#include "locks.h"
#include "utils.h"

// a data structure for passing parameters into threads
struct __vyt_thrdarg {
  int tid;
  vproc *proc;
};

int vpinit(vproc *proc) {
  if (NULL == proc) return VERROR;
  int stat = VOK;

  // setup _varlock
  if (0 != rw_init(&proc->_varlock))
    return VENOMEM;

  // try to initialize page table
  stat = vminit(&proc->mem);
  if (VOK != stat) {
    rw_destroy(&proc->_varlock);
    return stat;
  }

  // initialize the thread list
  proc->thrd = (vthrd*)malloc(sizeof(vthrd));
  if (NULL == proc->thrd) {
    rw_destroy(&proc->_varlock);
    vmdestroy(&proc->mem);
    return VENOMEM;
  }

  // setup the thrd list lock
  if (0 != rw_init(&proc->_thrd_lock)) {
    rw_destroy(&proc->_varlock);
    vmdestroy(&proc->mem);
    free(proc->thrd);
    return VENOMEM;
  }

  // set some variables
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

  // destroy _varlock
  rw_destroy(&proc->_varlock);

  // destroy the page table
  vmdestroy(&proc->mem);

  // de-allocate the thread list
  if (NULL != proc->thrd) {
    free(proc->thrd);
    proc->thrd = NULL;
  }
  // set some variables
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
  proc->thrd[0].reg[RIP] = v__urq(d);
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
  if (NULL == arg) return VENOMEM;
  arg->proc = proc;

  // acquire the thread list lock
  rw_wlock(&proc->_thrd_lock);

  // ptr into a thread list slot
  vthrd *avail = NULL;

  // check if there's still some free slots in the thread list
  // +1 for the main thread (slot 0 is reserved for main)
  if (proc->_thrd_alloc > proc->_thrd_used + 1) {
    for (vqword i = 1; i < proc->_thrd_alloc; i++) {
      if (!(proc->thrd[i].flags & VTUSED)) {
        // this slot is unused! let's use it

        // the thread might still be running in system
        rw_wunlock(&proc->_thrd_lock);
        thrd_join(proc->thrd[i].handle, NULL);
        rw_wlock(&proc->_thrd_lock);

        avail = &proc->thrd[i];
        arg->tid = i;
        if (NULL != tid) *tid = i;
        break;
      }
    }
  }

  // the thread list is full, try re-allocate
  else {
    vthrd *tmp = (vthrd*)realloc(proc->thrd,
                                 sizeof(vthrd) * proc->_thrd_alloc * 2);

    // failed to re-allocate the thread list
    if (NULL == tmp) {
      free(arg);
      rw_wunlock(&proc->_thrd_lock);
      return VENOMEM;
    }

    // initialize new structures
    for (vqword i = 0; i < proc->_thrd_alloc; i++) {
      tmp[proc->_thrd_alloc + i].flags = 0;
      // initialize the 16 registers to 0
      memset(tmp[proc->_thrd_alloc + i].reg, 0, sizeof(vqword) * 16);
    }

    // the end of the last allocation is now available! use it
    avail = &tmp[proc->_thrd_alloc];
    arg->tid = proc->_thrd_alloc;
    if (NULL != tid) *tid = proc->_thrd_alloc;

    proc->_thrd_alloc *= 2;
    proc->thrd = tmp;
  }

  // set some variables on the thread
  avail->flags = VTUSED | VTALIVE;
  avail->reg[RIP] = instptr;
  avail->reg[RSP] = staddr;
  avail->reg[RBP] = staddr;
  proc->_thrd_used++;

  // try start new thread
  if (thrd_create(&avail->handle, v__execunit, arg) != 0) {
    // failed to start new thread, do cleanup and return
    free(arg);
    avail->flags = 0;
    avail->reg[RIP] = 0;
    avail->reg[RSP] = 0;
    avail->reg[RBP] = 0;
    proc->_thrd_used--;

    rw_wunlock(&proc->_thrd_lock);

    // set the vm state to crashed
    atomic_store(&proc->state, VSCRASH);
    return VETHRD;
  }

  rw_wunlock(&proc->_thrd_lock);
  atomic_store(&proc->state, VSACTIVE);

  return VOK;
}

int v__execunit(void *arg) {
  // get the tid first
  int tid     = ((struct __vyt_thrdarg*)arg)->tid;
  vproc *proc = ((struct __vyt_thrdarg*)arg)->proc;
  free(arg);

  // increment number of alive threads
  atomic_fetch_add(&proc->alive, 1);

  // this is the execution loop, it relies on
  while (1) {
    rw_rlock(&proc->_thrd_lock);

    // check if the runtime is still active, and this thread is still alive
    if (
      atomic_load(&proc->state) != VSACTIVE ||
      !(proc->thrd[tid].flags & VTALIVE)
    ) {
      rw_runlock(&proc->_thrd_lock);
      break;
    }

    // increment active threads count
    atomic_fetch_add(&proc->active, 1);

    // TODO: decode and execute instructions here

    // decrement active threads count
    atomic_fetch_sub(&proc->active, 1);
    rw_runlock(&proc->_thrd_lock);

break; // tmp code

  }

  // do some clean-ups
  rw_wlock(&proc->_thrd_lock);
  proc->thrd[tid].flags = 0;
  memset(&proc->thrd[tid].reg, 0, sizeof(vqword) * 16);
  proc->_thrd_used--;
  if (0 == proc->_thrd_used) atomic_store(&proc->state, VSDONE);
  rw_wunlock(&proc->_thrd_lock);

  // decrement number of alive threads
  atomic_fetch_sub(&proc->alive, 1);
  return VOK;
}
