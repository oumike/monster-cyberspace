#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace rc {

// Canned replies for boards that can't type (board::canType), edited in web
// config.
constexpr size_t kMaxPhrases = 10;
constexpr size_t kMaxPhraseBytes = 80;

// Trims each phrase, turns tabs, newlines and other control characters into
// spaces, cuts it to kMaxPhraseBytes on a UTF-8 boundary, drops empty ones
// and keeps the first kMaxPhrases.
std::vector<std::string> cleanPhrases(const std::vector<std::string>& in);
// One phrase per line (the NVS and web form); split cleans.
std::string joinPhrases(const std::vector<std::string>& p);
std::vector<std::string> splitPhrases(std::string_view text);
std::vector<std::string> defaultPhrases();

}  // namespace rc
