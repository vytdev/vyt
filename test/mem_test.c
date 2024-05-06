#include <stdio.h>
#include <string.h>
#include "__test.h"
#include "../src/vyt.h"
#include "../src/mem.h"

// a simple test to validate whether our memory paging implementation works
// perfectly fine
TEST(storing_data) {
  int stat = VOK;
  vmem mem;

  // initialize the page table
  stat = vminit(&mem);
  if (!TEST_ASSERT(VOK == stat, "vminit failed")) {
    return 0;
  }

  // map the page 0
  stat = vmmap(&mem, 0, VPREAD | VPWRITE);
  if (!TEST_ASSERT(VOK == stat, "vmmap failed")) {
    vmdestroy(&mem);
    return 0;
  }

  // our test buffers
  vbyte src[] = { 0xde, 0xad, 0xbe, 0xef };
  vbyte buf[4];
  memset(buf, 0, 4);

  // store these 4 bytes into the memory at addr 1
  stat = vmsetd(&mem, src, 0x1, 4, VPWRITE);
  if (!TEST_ASSERT(VOK == stat, "vmsetd failed")) {
    vmdestroy(&mem);
    return 0;
  }

  // get those 4 bytes and store it onto buffer
  stat = vmgetd(&mem, buf, 0x1, 4, VPREAD);
  if (!TEST_ASSERT(VOK == stat, "vmgetd failed")) {
    vmdestroy(&mem);
    return 0;
  }

  // check if src and buf are match
  if (!TEST_ASSERT(memcmp(src, buf, 4) == 0,
                   "data unmatched from what we stored"))
  {
    vmdestroy(&mem);
    return 0;
  }

  vmdestroy(&mem);
  return 1;
}

int test(const char *suite_name) {
  TEST_RUN(storing_data);

  // exit code
  return 0;
}
