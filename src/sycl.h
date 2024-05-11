#ifndef _VYT_SYCL_H
#define _VYT_SYCL_H
#include <stdatomic.h>
#include "vyt.h"
#include "exec.h"

static inline int VSYCL_exit(vproc *proc, vthrd *thr) {
  // stop the current thread's execution
  thr->flags = 0;
  vqword extc = thr->reg[R1];

  // set the higher exit code
  if (atomic_load(&proc->exitcode) < extc)
    atomic_store(&proc->exitcode, extc);

  return VOK;
}

#endif // _VYT_SYCL_H
