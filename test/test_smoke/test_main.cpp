#include <unity.h>

#include "core_version.h"

void setUp() {}
void tearDown() {}

void test_version() { TEST_ASSERT_EQUAL_STRING("0.1.0", RC_VERSION); }

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_version);
  return UNITY_END();
}
