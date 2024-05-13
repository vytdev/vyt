#ifndef _VYT_INST_JMP_H
#define _VYT_INST_JMP_H
#include "../vyt.h"
#include "../exec.h"

static inline int VINST_jmp(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {
  if (WQWORD != wsz || (DREG != mop1 && DRELADDR != mop1 && DABSADDR != mop1 &&
      DDYNADDR != mop1) || DNONE != mop2)
    return VEINST;

  // jump to the given address
  if (DREG == mop1) {
    thr->reg[RIP] = thr->reg[*op1];
  } else {
    thr->reg[RIP] = v__maddr(mop1, op1, thr);
  }

  return VOK;
}

#endif // _VYT_INST_JMP_H
