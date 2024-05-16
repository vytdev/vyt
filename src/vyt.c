#include <stdio.h>
#include "vyt.h"

int vperr(int ret) {
  char *msg;

  // find the appropriate message
  switch (ret) {
    case VOK:     msg = "operation completed successfully"; break;
    case VERROR:  msg = "an internal error occured"; break;
    case VENULL:  msg = "null pointer dereference"; break;
    case VENOMEM: msg = "out of memory"; break;
    case VESEGV:  msg = "segmentation fault"; break;
    case VEACCES: msg = "permission denied"; break;
    case VEHDR:   msg = "failed to read header"; break;
    case VEMAGIC: msg = "unsupported format"; break;
    case VEREV:   msg = "version mismatch"; break;
    case VEMALF:  msg = "malformed format"; break;
    case VETHRD:  msg = "cannot create new thread"; break;
    case VEINST:  msg = "illegal instruction"; break;
    case VESYCL:  msg = "unknown system call"; break;
    default:
      // unknown error code
      fprintf(stderr, "error %d\n", ret);
      return ret;
  }

  // print the message
  fprintf(stderr, "%s\n", msg);
  return ret;
}
