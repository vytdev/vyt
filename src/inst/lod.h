#ifndef _VYT_INST_LOD_H
#define _VYT_INST_LOD_H
#include <string.h>
#include "../vyt.h"
#include "../exec.h"
#include "../mem.h"
#include "../utils.h"

static inline int VINST_lod(vproc *proc, vthrd *thr, vbyte wsz, vbyte mop1, vbyte op1sz, vbyte *op1, vbyte mop2, vbyte op2sz, vbyte *op2) {
  if (DREG != mop1 || (DIMMED != mop2 && DRELADDR != mop2 && DABSADDR != mop2 &&
      DDYNADDR != mop2))
    return VEINST;

  // reading buffer
  vbyte buf[8];
  memset(buf, 0, 8);

  // read bytes
  if (DIMMED == mop2)
    memcpy(buf, op2, v__wsz(wsz));
  else {
    int stat = vmgetd(&proc->mem, buf, v__maddr(mop2, op2, thr),
                      v__wsz(wsz), VPREAD);
    if (VOK != stat) return stat;
  }

  // decode the integer
  thr->reg[*op1] = v__urq(buf);

  return VOK;
}

#endif // _VYT_INST_LOD_H
