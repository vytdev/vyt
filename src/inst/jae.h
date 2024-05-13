#ifndef _VYT_INST_JAE_H
#define _VYT_INST_JAE_H
#include "../vyt.h"
#include "../exec.h"

static inline int VINST_jae(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {
  if (WQWORD != wsz || (DREG != mop1 && DRELADDR != mop1 && DABSADDR != mop1 &&
      DDYNADDR != mop1) || DNONE != mop2)
    return VEINST;

  // check flags
  if (vfget(thr, RFL_CF))
    return VOK;

  // jump to the given address
  if (DREG == mop1) {
    thr->reg[RIP] = thr->reg[*op1];
  } else {
    thr->reg[RIP] = v__maddr(mop1, op1, thr);
  }

  return VOK;
}

#endif // _VYT_INST_JAE_H
