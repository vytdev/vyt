#ifndef _VYT_INST_SYS_H
#define _VYT_INST_SYS_H
#include "../vyt.h"
#include "../exec.h"
#include "../sycl.h"
#include "../utils.h"

static inline int VINST_sys(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {

  if (WWORD != wsz || DIMMED != mop1 || DNONE != mop2)
    return VEINST;

  switch (v__urw(op1)) {
    case 0x0001: return VSYCL_exit(proc, thr);
  }

  return VESYCL;
}

#endif // _VYT_INST_SYS_H
