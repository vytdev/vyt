#ifndef _VYT_INST_LEA_H
#define _VYT_INST_LEA_H
#include "../vyt.h"
#include "../exec.h"

static inline int VINST_lea(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {
  if (WQWORD != wsz || DREG != mop1 || (DRELADDR != mop2 && DABSADDR != mop2 &&
      DDYNADDR != mop2))
    return VEINST;

  // get the memory address
  thr->reg[*op1] = v__maddr(mop2, op2, thr);

  return VOK;
}

#endif // _VYT_INST_LEA_H
