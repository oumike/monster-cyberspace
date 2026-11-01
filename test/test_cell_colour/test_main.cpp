// The Cardputer ADV's flags: low memory, but a colour panel. Cell here is the
// same 12-byte struct the default native build compiles lib/core with.
#define RC_LOW_MEM 1
#define RC_MONO_CELLS 0

#include <unity.h>

#include "ui/grid.h"

void setUp() {}
void tearDown() {}

void test_low_mem_colour_cell_keeps_rgb565() {
  rc::Cell c(U'x', 0xF81F, 0x07E0);
  c.setEdgeColor(0x001F);
  TEST_ASSERT_EQUAL_UINT16(0xF81F, rc::cellColor(c.fg));
  TEST_ASSERT_EQUAL_UINT16(0x07E0, rc::cellColor(c.bg));
  TEST_ASSERT_EQUAL_UINT16(0x001F, rc::cellColor(c.edgeColor));
  TEST_ASSERT_EQUAL(12, (int)sizeof(rc::Cell));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_low_mem_colour_cell_keeps_rgb565);
  return UNITY_END();
}
