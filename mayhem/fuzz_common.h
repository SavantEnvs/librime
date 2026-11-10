// Shared setup for the librime harnesses: one scratch directory per process, handed to rime as
// its shared/user/staging directory so every harness-owned file lives under $TMPDIR.
#ifndef MAYHEM_FUZZ_COMMON_H_
#define MAYHEM_FUZZ_COMMON_H_

#include <stdint.h>
#include <stdlib.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include <rime_api.h>

namespace fz {

namespace fs = std::filesystem;

inline std::string g_dir;

inline void RemoveScratch() {
  std::error_code ec;
  fs::remove_all(g_dir, ec);
}

// Creates <TMPDIR>/librime-fuzz-XXXXXX and initializes librime on it (quiet: stderr, FATAL only).
inline void InitRime(const char* app_name) {
  const char* tmp = getenv("TMPDIR");
  std::string tmpl = std::string(tmp && *tmp ? tmp : "/tmp") + "/librime-fuzz-XXXXXX";
  if (!mkdtemp(tmpl.data())) {
    perror("mkdtemp");
    abort();
  }
  g_dir = tmpl;
  fs::create_directories(fs::path(g_dir) / "build");
  atexit(RemoveScratch);

  RIME_STRUCT(RimeTraits, traits);
  traits.shared_data_dir = traits.user_data_dir = g_dir.c_str();
  traits.prebuilt_data_dir = traits.staging_dir = g_dir.c_str();
  traits.app_name = app_name;
  traits.min_log_level = 3;
  traits.log_dir = "";
  rime_get_api()->setup(&traits);
  rime_get_api()->initialize(&traits);
}

inline bool WriteFile(const fs::path& path, const void* data, size_t size) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
  return bool(out);
}

}  // namespace fz

#endif  // MAYHEM_FUZZ_COMMON_H_
