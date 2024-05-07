#ifndef _VYT_UTILS_H
#define _VYT_UTILS_H
#include "vyt.h"

/* util read byte */
static inline vbyte v__urb(vbyte *st) {
  return st[0];
}

/* util read word */
static inline vword v__urw(vbyte *st) {
  return st[0]
    | (st[1] << 8);
}

/* util read dword */
static inline vdword v__urd(vbyte *st) {
  return st[0]
    | (st[1] << 8)
    | (st[2] << 16)
    | (st[3] << 24);
}

/* util read qword */
static inline vqword v__urq(vbyte *st) {
  return (vqword)st[0]
    | ((vqword)st[1] << 8)
    | ((vqword)st[2] << 16)
    | ((vqword)st[3] << 24)
    | ((vqword)st[4] << 32)
    | ((vqword)st[5] << 40)
    | ((vqword)st[6] << 48)
    | ((vqword)st[7] << 56);
}

/* util write byte */
static inline void v__uwb(vbyte *out, vbyte v) {
  out[0] = v;
}

/* util write word */
static inline void v__uww(vbyte *out, vword v) {
  out[0] = v & 0xff;
  out[1] = v >> 8;
}

/* util write dword */
static inline void v__uwd(vbyte *out, vdword v) {
  out[0] = v & 0xff;
  out[1] = (v >> 8) & 0xff;
  out[2] = (v >> 16) & 0xff;
  out[3] = v >> 24;
}

/* util write qword */
static inline void v__uwq(vbyte *out, vqword v) {
  out[0] = v & 0xff;
  out[1] = (v >> 8) & 0xff;
  out[2] = (v >> 16) & 0xff;
  out[3] = (v >> 24) & 0xff;
  out[4] = (v >> 32) & 0xff;
  out[5] = (v >> 40) & 0xff;
  out[6] = (v >> 48) & 0xff;
  out[7] = v >> 56;
}

#endif // _VYT_UTILS_H
