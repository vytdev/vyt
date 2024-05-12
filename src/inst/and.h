#ifndef _VYT_INST_AND_H
#define _VYT_INST_AND_H
#include "../vyt.h"
#include "../exec.h"
#include "../utils.h"

static inline int VINST_and(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {
  if (WQWORD != wsz || DREG != mop1 || (DIMMED != mop2 && DREG != mop2))
    return VEINST;

  vqword val = thr->reg[*op1];

  // second operand
  if (DIMMED == mop2) val &= v__urq(op2);
  else                val &= thr->reg[*op2];

  // set flags
  vfset(thr, RFL_CF, 0);
  vfset(thr, RFL_ZF, 0 == val);
  vfset(thr, RFL_SF, val >> 63);
  vfset(thr, RFL_OF, 0);

  // set the result
  thr->reg[*op1] = val;

  return VOK;
}

#endif // _VYT_INST_AND_H
