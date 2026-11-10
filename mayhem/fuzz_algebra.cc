// Fuzzes spelling algebra, the regex-based rewrite language schemas use to derive spellings
// (xlit / xform / erase / derive / fuzz / abbrev / reorder). Input layout: the first line holds
// space-separated syllables the rules are applied to; every following line is one rule.
#include <sstream>
#include <string>
#include <vector>

#include <rime/algo/algebra.h>
#include <rime/config.h>

#include "fuzz_common.h"

namespace {

constexpr size_t kMaxSyllables = 16;

}  // namespace

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  fz::InitRime("rime.fuzz_algebra");
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::istringstream in(std::string(reinterpret_cast<const char*>(data), size));
  std::string line;
  std::getline(in, line);

  std::vector<std::string> syllables;
  std::istringstream words(line);
  std::string word;
  while (syllables.size() < kMaxSyllables && words >> word) {
    syllables.push_back(word);
  }

  auto rules = rime::New<rime::ConfigList>();
  while (std::getline(in, line)) {
    rules->Append(rime::New<rime::ConfigValue>(line));
  }

  rime::Projection projection;
  if (!projection.Load(rules)) {
    return -1;
  }

  rime::Script script;
  for (const auto& syllable : syllables) {
    std::string text = syllable;
    projection.Apply(&text);
    script.AddSyllable(syllable);
  }
  projection.Apply(&script);
  return 0;
}
