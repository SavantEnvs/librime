// Fuzzes the dictionary compiler: the input is a whole <name>.dict.yaml source (YAML header,
// then tab-separated "text<TAB>code<TAB>weight" rows). It goes through DictSettings header
// parsing, EntryCollector, Table/Prism building, and then lookups against the built files.
#include <string>

#include <rime/common.h>
#include <rime/algo/encoder.h>
#include <rime/algo/syllabifier.h>
#include <rime/dict/dict_compiler.h>
#include <rime/dict/dictionary.h>
#include <rime/dict/prism.h>
#include <rime/dict/table.h>

#include "fuzz_common.h"

namespace {

constexpr char kName[] = "fuzzdict";

}  // namespace

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  fz::InitRime("rime.fuzz_dict_compile");
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const fz::fs::path dir(fz::g_dir);
  const fz::fs::path build = dir / "build";
  std::error_code ec;
  for (const char* ext : {".table.bin", ".prism.bin", ".reverse.bin"}) {
    fz::fs::remove(build / (std::string(kName) + ext), ec);
  }
  if (!fz::WriteFile(dir / (std::string(kName) + ".dict.yaml"), data, size)) {
    return 0;
  }

  rime::Dictionary dict(
      kName, {},
      {rime::New<rime::Table>(rime::path{(build / "fuzzdict.table.bin").string()})},
      rime::New<rime::Prism>(rime::path{(build / "fuzzdict.prism.bin").string()}));
  rime::DictCompiler compiler(&dict);
  compiler.set_options(rime::DictCompiler::kRebuild);
  if (!compiler.Compile(rime::path())) {
    return -1;
  }
  if (!dict.Load()) {
    return 0;
  }

  for (const char* key : {"a", "ba", "zhong", "ni", "z"}) {
    rime::DictEntryIterator it;
    dict.LookupWords(&it, key, true);
    for (int n = 0; n < 8 && !it.exhausted(); ++n, it.Next()) {
      rime::RawCode raw;
      dict.Decode(it.Peek()->code, &raw);
    }
  }
  rime::SyllableGraph graph;
  rime::Syllabifier syllabifier;
  syllabifier.BuildSyllableGraph("nihaoshijie", *dict.prism(), &graph);
  dict.Lookup(graph, 0);
  return 0;
}
