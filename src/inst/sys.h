#ifndef _VYT_INST_SYS_H
#define _VYT_INST_SYS_H
#include "../vyt.h"
#include "../exec.h"

static inline int VINST_sys(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {

  // TODO: implement this
  thr->flags = 0; // tmp code, stops the execution unit

  return VOK;
}

#endif // _VYT_INST_SYS_H
