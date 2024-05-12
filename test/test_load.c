#include <stdio.h>
#include "__test.h"
#include "../src/vyt.h"
#include "../src/exec.h"
#include "../src/mem.h"

// a test to verify that our program loader works as expected
TEST(program_loader) {
  int stat = VOK;
  vproc p;

  // startup options
  struct vopts opt = {
    .stacksz = 0,           // no need to allocate stack
  };

  // initialize the process
  stat = vpinit(&p, &opt);
  if (!TEST_ASSERT(VOK == stat, "vpinit failed")) {
    return 0;
  }

  // our program sample
  vbyte prog[] = {
    0x00, 0x56, 0x59, 0x54,                         // the header
    0x01,                                           // abi version
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // entry point

    // first entry on the load table
    VLLOAD,                                         // load type (from payload)
    VPREAD | VPWRITE,                               // flags
    0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // file offset
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // memory address
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // size to load

    // indicates the end of load table
    0x00,

    // some data to load
    /* 0x28: */ 0xde, 0xad, 0xbe, 0xef,
  };

  // try load the sample program
  stat = vload(&p, prog, sizeof(prog));
  if (!TEST_ASSERT(VOK == stat, "vload failed")) {
    vperr(stat);
    vpdestroy(&p);
    return 0;
  }

  // check if the bytes DE AD BE EF is in the memory at address 0x1
  vbyte buf[4];
  stat = vmgetd(&p.mem, buf, 0x1, 4, VPREAD);
  if (!TEST_ASSERT(VOK == stat, "vmgetd failed")) {
    vperr(stat);
    vpdestroy(&p);
    return 0;
  }

  // now, check it~
  if (!TEST_ASSERT(memcmp(&prog[0x28], buf, 4) == 0,
                   "data unmatched from what we stored"))
  {
    vpdestroy(&p);
    return 0;
  }

  // test succeded!
  vpdestroy(&p);
  return 1;
}

int test(const char *suite_name) {
  TEST_RUN(program_loader);
  return 0;
}
