#ifndef _VYT_INST_NOT_H
#define _VYT_INST_NOT_H
#include "../vyt.h"
#include "../exec.h"
#include "../utils.h"

static inline int VINST_not(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {
  if (WQWORD != wsz || DREG != mop1 || DNONE != mop2)
    return VEINST;

  vqword val = thr->reg[*op1];
  val = ~val;

  // set flags
  vfset(thr, RFL_ZF, 0 == val);
  vfset(thr, RFL_SF, val >> 63);

  // set the result
  thr->reg[*op1] = val;

  return VOK;
}

#endif // _VYT_INST_NOT_H
