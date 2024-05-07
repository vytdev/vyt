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

// a test to verify the functionality of memory protection mechanisms
TEST(mem_prot) {
  int stat = VOK;
  vmem mem;

  // initialize the page table
  stat = vminit(&mem);
  if (!TEST_ASSERT(VOK == stat, "vminit failed")) {
    return 0;
  }

  // map the page 0 with READ and EXEC permissions
  stat = vmmap(&mem, 0, VPREAD | VPEXEC);
  if (!TEST_ASSERT(VOK == stat, "vmmap failed")) {
    vmdestroy(&mem);
    return 0;
  }

  vbyte src[] = { 0xff, 0xfe };

  // attempt to do set-data operation requiring WRITE access
  stat = vmsetd(&mem, src, 0x1, 2, VPWRITE);

  // vmsetd should return VEACCES to indicate that the requested WRITE
  // permission is denied, as the page only allows READ and EXEC access, as
  // specified when it was mapped
  if (!TEST_EXPECT_EQ(stat, VEACCES)) {
    vmdestroy(&mem);
    return 0;
  }

  vmdestroy(&mem);
  return 1;
}

int test(const char *suite_name) {
  TEST_RUN(storing_data);
  TEST_RUN(mem_prot);

  // exit code
  return 0;
}
