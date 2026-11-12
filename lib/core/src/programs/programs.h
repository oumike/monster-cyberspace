// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rc {

// The most source text a Program screen keeps; the rest is cut off.
constexpr size_t kSourceShowMax = 32 * 1024;

// A row of GET /v1/programs, or the head of a source response.
struct ProgramInfo {
  std::string id, name, owner, description;
  std::string runtime;  // "web" when the API leaves it out
  int release = 0;
  int64_t publishedMs = 0;  // 0 unknown
};

// GET /v1/programs/:id/source.
struct ProgramSource {
  ProgramInfo info;
  bool binary = false;  // encoding base64 (wasm): text stays empty
  size_t bytes = 0;     // the decoded size for binary, else the source's length
  std::string text;     // the first kSourceShowMax bytes, cut on a UTF-8 boundary
  bool cut = false;     // text was cut
};

// `cursor` "" when null. Rows without an id are skipped.
bool parseProgramPage(std::string_view body, std::vector<ProgramInfo>& out, std::string& cursor);
// False without an id.
bool parseProgramSource(std::string_view body, ProgramSource& out);
// The decoded size of base64 text (whitespace ignored), without decoding it.
size_t base64DecodedSize(std::string_view b64);

// Source as a Program screen shows it: tabs become two spaces, \r and other
// control characters go, and so does a final newline.
std::u32string cleanSource(std::u32string_view text);

}  // namespace rc
