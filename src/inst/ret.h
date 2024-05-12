#ifndef _VYT_INST_RET_H
#define _VYT_INST_RET_H
#include <string.h>
#include "../vyt.h"
#include "../exec.h"
#include "../utils.h"

static inline int VINST_ret(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {
  if (DNONE != mop1 || DNONE != mop2)
    return VEINST;
  int stat = VOK;

  // pop the return address and set the prog counter there
  vbyte buf[8];
  stat = vstpop(proc, thr, buf, 8);
  if (VOK != stat) return stat;
  thr->reg[RIP] = v__urq(buf);

  return VOK;
}

#endif // _VYT_INST_RET_H
