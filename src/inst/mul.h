#ifndef _VYT_INST_MUL_H
#define _VYT_INST_MUL_H
#include "../vyt.h"
#include "../exec.h"

static inline int VINST_mul(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {
  if (DREG != mop1 || (DIMMED != mop2 && DREG != mop2))
    return VEINST;

  vqword a = thr->reg[*op1];
  vqword b = 0;

  // second operand
  if (DIMMED == mop2) b = v__rnum(wsz, op2);
  else                b = thr->reg[*op2];

  vqword val = a * b;

  // set flags
  // TODO: CF flag here
  vfset(thr, RFL_ZF, 0 == val);
  vfset(thr, RFL_SF, val >> 63);
  vfset(thr, RFL_OF, ((a>>63) ^ (b>>63)) & ((a>>63) ^ (val>>63)));

  // set the result
  thr->reg[*op1] = val;

  return VOK;
}

#endif // _VYT_INST_MUL_H

