#ifndef _VYT_INST_IMOD_H
#define _VYT_INST_IMOD_H
#include <stdint.h>
#include "../vyt.h"
#include "../exec.h"

static inline int VINST_imod(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {
  if (DREG != mop1 || (DIMMED != mop2 && DREG != mop2))
    return VEINST;

  vqword a = thr->reg[*op1];
  vqword b = 0;

  // second operand
  if (DIMMED == mop2) b = v__rnum(wsz, op2);
  else                b = thr->reg[*op2];

  vqword val = (int64_t)a % (int64_t)b;

  // set flags
  // TODO: CF flag here
  vfset(thr, RFL_ZF, 0 == val);
  vfset(thr, RFL_SF, val >> 63);
  vfset(thr, RFL_OF, ((a>>63) ^ (b>>63)) ^ (val>>63));

  // set the result
  thr->reg[*op1] = val;

  return VOK;
}

#endif // _VYT_INST_IMOD_H

