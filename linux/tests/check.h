// SPDX-License-Identifier: GPL-3.0-or-later
// A minimal check harness for the Linux layer's CTest suite.
#pragma once

#include <stdlib.h>
#include <unistd.h>

#include <cstdio>
#include <string>

inline int checkFailures = 0;

#define CHECK(cond)                                                              \
  do {                                                                           \
    if (!(cond)) {                                                               \
      std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
      ++checkFailures;                                                           \
    }                                                                            \
  } while (0)

#define CHECK_EQ(a, b)                                                                     \
  do {                                                                                     \
    if (!((a) == (b))) {                                                                   \
      std::fprintf(stderr, "%s:%d: CHECK_EQ(%s, %s) failed\n", __FILE__, __LINE__, #a, #b); \
      ++checkFailures;                                                                     \
    }                                                                                      \
  } while (0)

inline int checkResult() {
  if (checkFailures) std::fprintf(stderr, "%d check(s) failed\n", checkFailures);
  return checkFailures ? 1 : 0;
}

// A fresh directory under $TMPDIR (or /tmp) for one test run.
inline std::string tempDir() {
  const char* t = std::getenv("TMPDIR");
  std::string tmpl = std::string(t && *t ? t : "/tmp") + "/mc-test-XXXXXX";
  if (!mkdtemp(&tmpl[0])) {
    std::perror("mkdtemp");
    std::exit(2);
  }
  return tmpl;
}
