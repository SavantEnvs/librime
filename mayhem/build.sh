#!/usr/bin/env bash
# mayhem/build.sh — builds librime's fuzz harnesses (sanitized static library), their -standalone
# reproducers, and the project's own gtest suite (normal flags) for mayhem/test.sh.
set -euo pipefail

# clang rejects SOURCE_DATE_EPOCH='' (empty) — it must be unset or a valid integer.
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
OUT=${MAYHEM_OUT:-/mayhem}
TARGETS="fuzz_config fuzz_algebra fuzz_dict_compile"

# CMake takes the compilers from the exported CC/CXX; passing -DCMAKE_*_COMPILER=<name> makes a
# re-run see a "changed" compiler (cache holds the resolved path) and wipe the cache.

# 1) The project's own test suite with its NORMAL flags (shared library + rime_test), kept apart
#    from the sanitized build so test.sh stays an honest oracle.
cmake -S . -B build/oracle -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TEST=ON -DENABLE_LOGGING=ON \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS" -DCMAKE_SHARED_LINKER_FLAGS="$COVERAGE_FLAGS"
cmake --build build/oracle -j"$MAYHEM_JOBS" --target rime_test

# 2) The library itself, static, instrumented: the fuzzed code carries ASan/UBSan, DWARF 3 and
#    SanCov (fuzzer-no-link) so libFuzzer sees coverage inside librime, not just in the harness.
FUZZ_CXXFLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link"
cmake -S . -B build/fuzz -G Ninja \
  -DBUILD_SHARED_LIBS=OFF -DBUILD_TEST=OFF -DENABLE_LOGGING=ON \
  -DCMAKE_BUILD_TYPE= \
  -DCMAKE_C_FLAGS="$FUZZ_CXXFLAGS" -DCMAKE_CXX_FLAGS="$FUZZ_CXXFLAGS"
cmake --build build/fuzz -j"$MAYHEM_JOBS" --target rime-static

LIBRIME=$(find build/fuzz -name 'librime*.a' | head -n1)
LIBS="-lyaml-cpp -lglog -lgflags -lleveldb -lmarisa -lopencc -lboost_regex -lpthread -lm"
INCS="-I$SRC/include -I$SRC/src -I$SRC/build/fuzz/src -DBOOST_DLL_USE_STD_FS -DBOOST_REGEX_NO_LIB"

# 3) Harnesses: libFuzzer binary + non-fuzzer -standalone reproducer, each linked with lsan_off.
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o build/fuzz/lsan_off.o
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o build/fuzz/standalone_main.o

for t in $TARGETS; do
  $CXX -std=c++17 $SANITIZER_FLAGS $DEBUG_FLAGS -O1 $LIB_FUZZING_ENGINE $INCS \
    "mayhem/$t.cc" build/fuzz/lsan_off.o "$LIBRIME" $LIBS -o "$OUT/$t"
  $CXX -std=c++17 $SANITIZER_FLAGS $DEBUG_FLAGS -O1 $INCS \
    "mayhem/$t.cc" build/fuzz/standalone_main.o build/fuzz/lsan_off.o "$LIBRIME" $LIBS \
    -o "$OUT/$t-standalone"
done
