#ifndef _VYT_INST_CALL_H
#define _VYT_INST_CALL_H
#include "../vyt.h"
#include "../exec.h"
#include "../utils.h"

static inline int VINST_call(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {
  if (WQWORD != wsz || (DREG != mop1 && DRELADDR != mop1 && DABSADDR != mop1 &&
      DDYNADDR != mop1) || DNONE != mop2)
    return VEINST;
  int stat = VOK;

  // push the return address first
  vbyte buf[8];
  v__uwq(buf, thr->reg[RIP]);
  stat = vstpush(proc, thr, buf, 8);
  if (VOK != stat) return stat;

  // jump to the given address
  if (DREG == mop1) {
    thr->reg[RIP] = thr->reg[*op1];
  } else {
    thr->reg[RIP] = v__maddr(mop1, op1, thr);
  }

  return VOK;
}

#endif // _VYT_INST_CALL_H
