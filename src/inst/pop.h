#ifndef _VYT_INST_POP_H
#define _VYT_INST_POP_H
#include <string.h>
#include "../vyt.h"
#include "../exec.h"
#include "../utils.h"

static inline int VINST_pop(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {
  if ((DREG != mop1 && DRELADDR != mop1 && DABSADDR != mop1 && DDYNADDR != mop1)
      || DNONE != mop2)
    return VEINST;
  int stat = VOK;

  // read buffer
  vbyte buf[8];
  memset(buf, 0, 8);

  // pop data from stack
  stat = vstpop(proc, thr, buf, v__wsz(wsz));
  if (VOK != stat) return stat;

  // write data
  if (DREG == mop1)
    thr->reg[*op1] = v__urq(buf);
  else {
    int stat = vmsetd(&proc->mem, buf, v__maddr(mop1, op1, thr),
                      v__wsz(wsz), VPWRITE);
    if (VOK != stat) return stat;
  }

  return VOK;
}

#endif // _VYT_INST_POP_H
