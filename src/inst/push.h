#ifndef _VYT_INST_PUSH_H
#define _VYT_INST_PUSH_H
#include <string.h>
#include "../vyt.h"
#include "../exec.h"
#include "../utils.h"

static inline int VINST_push(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {
  if ((DIMMED != mop1 && DREG != mop1 && DRELADDR != mop1 && DABSADDR != mop1 &&
      DDYNADDR != mop1) || DNONE != mop2)
    return VEINST;
  int stat = VOK;

  // read buffer
  vbyte buf[8];
  memset(buf, 0, 8);

  // read source to buffer
  if (DIMMED == mop1)
    memcpy(buf, op1, op1sz);
  else if (DREG == mop1)
    v__uwq(buf, thr->reg[*op1]);
  else {
    int stat = vmgetd(&proc->mem, buf, v__maddr(mop1, op1, thr),
                      v__wsz(wsz), VPREAD);
    if (VOK != stat) return stat;
  }

  // push data to stack
  stat = vstpush(proc, thr, buf, v__wsz(wsz));
  if (VOK != stat) return stat;

  return VOK;
}

#endif // _VYT_INST_PUSH_H
