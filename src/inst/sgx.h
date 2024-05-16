#ifndef _VYT_INST_SGX_H
#define _VYT_INST_SGX_H
#include "../vyt.h"
#include "../exec.h"

static inline int VINST_sgx(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {
  if (DREG != mop1 || DNONE != mop2)
    return VEINST;

  // sign extend!
  thr->reg[*op1] = vsignx(wsz, thr->reg[*op1]);

  return VOK;
}

#endif // _VYT_INST_SGX_H
