#ifndef _VYT_INST_CMP_H
#define _VYT_INST_CMP_H
#include "../vyt.h"
#include "../exec.h"

static inline int VINST_cmp(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {
  if ((DIMMED != mop1 && DREG != mop1) || (DIMMED != mop2 && DREG != mop2))
    return VEINST;

  vqword vop1 = 0;
  vqword vop2 = 0;
  vqword result = 0;

  // first operand
  if (DIMMED == mop2) vop1 = v__rnum(wsz, op1);
  else                vop1 = thr->reg[*op1];

  // second operand
  if (DIMMED == mop2) vop2 = v__rnum(wsz, op2);
  else                vop2 = thr->reg[*op2];

  // subtract values
  result = vop1 - vop2;

  // set flags
  vfset(thr, RFL_CF, vop1 < vop2);
  vfset(thr, RFL_ZF, 0 == result);
  vfset(thr, RFL_SF, result >> 63);
  vfset(thr, RFL_OF, ((vop1>>63) ^ (vop2>>63)) & ((vop1>>63) ^ (result>>63)));
  // RFL_OF logic:
  // (vop1>>63) != (vop2>>63) && (vop1>>63) != (result>>63)

  return VOK;
}

#endif // _VYT_INST_CMP_H
