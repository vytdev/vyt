#ifndef _VYT_INST_MOV_H
#define _VYT_INST_MOV_H
#include <string.h>
#include "../vyt.h"
#include "../exec.h"
#include "../mem.h"
#include "../utils.h"

static inline int VINST_mov(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {
  if ((DREG != mop1 && DRELADDR != mop1 && DABSADDR != mop1 && DDYNADDR != mop1)
    || (DIMMED != mop2 && DREG != mop2 && DRELADDR != mop2 && DABSADDR != mop2
    && DDYNADDR != mop2))
    return VEINST;

  // reading buffer
  vbyte buf[8];
  memset(buf, 0, 8);

  // read data into buffer
  if (DIMMED == mop2) {
    memcpy(buf, op2, op2sz);
  } else if (DREG == mop2) {
    v__uwq(buf, thr->reg[*op2]);
  } else {
    int stat = vmgetd(&proc->mem, buf, v__maddr(mop2, op2, thr),
                      v__wsz(wsz), VPREAD);
    if (VOK != stat) return stat;
  }

  // write data to the target
  if (DREG == mop1) {
    thr->reg[*op1] = v__urq(buf);
  } else {
    int stat = vmsetd(&proc->mem, buf, v__maddr(mop1, op1, thr),
                      v__wsz(wsz), VPWRITE);
    if (VOK != stat) return stat;
  }

  return VOK;
}

#endif // _VYT_INST_MOV_H
