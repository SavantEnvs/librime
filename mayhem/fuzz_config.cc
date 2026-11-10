// Fuzzes librime's YAML configuration stack: the raw yaml-cpp -> ConfigItem conversion
// (Config::LoadFromStream) and the config compiler that resolves __include / __patch /
// __append / __merge directives, which is what every *.yaml schema, dictionary header and
// user customization goes through.
#include <sstream>
#include <string>

#include <rime/component.h>
#include <rime/config.h>

#include "fuzz_common.h"

namespace {

rime::the<rime::Config::Component> g_component;

// Exercise every accessor on every node of a loaded tree (depth-limited traversal).
void Walk(const rime::an<rime::ConfigItem>& item, int depth) {
  if (!item || depth > 48) {
    return;
  }
  switch (item->type()) {
    case rime::ConfigItem::kScalar: {
      auto value = std::dynamic_pointer_cast<rime::ConfigValue>(item);
      bool b;
      int i;
      double d;
      std::string s;
      value->GetBool(&b);
      value->GetInt(&i);
      value->GetDouble(&d);
      value->GetString(&s);
      break;
    }
    case rime::ConfigItem::kList: {
      auto list = std::dynamic_pointer_cast<rime::ConfigList>(item);
      for (size_t n = 0; n < list->size(); ++n) {
        Walk(list->GetAt(n), depth + 1);
      }
      break;
    }
    case rime::ConfigItem::kMap: {
      auto map = std::dynamic_pointer_cast<rime::ConfigMap>(item);
      for (auto it = map->begin(); it != map->end(); ++it) {
        Walk(it->second, depth + 1);
      }
      break;
    }
    default:
      break;
  }
}

void Inspect(rime::Config& config) {
  Walk(config.GetItem(""), 0);
  std::ostringstream out;
  config.SaveToStream(out);
}

}  // namespace

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  fz::InitRime("rime.fuzz_config");
  g_component.reset(new rime::ConfigComponent<rime::ConfigBuilder>);
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  {
    rime::Config config;
    std::istringstream in(std::string(reinterpret_cast<const char*>(data), size));
    if (config.LoadFromStream(in)) {
      Inspect(config);
    }
  }
  {
    const auto path = fz::fs::path(fz::g_dir) / "fuzz.yaml";
    if (!fz::WriteFile(path, data, size)) {
      return 0;
    }
    rime::the<rime::Config> config(g_component->Create("fuzz"));
    if (config) {
      Inspect(*config);
    }
  }
  return 0;
}
