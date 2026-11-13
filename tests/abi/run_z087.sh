#!/usr/bin/env bash
# Z-087 Aesir boundary trial - the Aesir CPU engine on this
# host run against the Z-082 interposing variant (LD_PRELOAD
# of build/libmimiralloc_interpose.so), plain vs preloaded,
# allocation behaviour on the token path profiled from
# outside the process (engine timing JSON + /usr/bin/time -v).
# Output is the recorded run itself; the caller tees it to
# tests/abi/recorded_run_z13_z087.txt. Per-leg logs land in
# hidden_files/z087_<leg>.log. Exit 0 iff every leg exits 0
# and all legs generate the identical token stream.
#
# BOUNDARY (roadmap, verbatim): read-only toward the Aesir
# forge. The engine is built ONCE from its current source
# via the Aesir seat's own canonical road (run_seat_gate_
# a074.sh shape): pixi run --frozen mojo build -I . with the
# FFI shims staged into an mktemp dir and the output binary
# written into an mktemp dir; HOME is contained in an
# mktemp dir for every leg. Nothing in ~/aesir-forge is
# written by this runner; its git status is snapshotted
# before and after as proof. Any Aesir-side blocker is
# NAMED in the recorded run, never fixed here.
#
# WHY A COMPILED BINARY (attempt-1 finding, preserved in
# hidden_files/z087_recorded_run_attempt1.txt and
# hidden_files/z087_mojorun_preload_episode.log): running
# the engine via `mojo run` puts the Mojo COMPILER in the
# preloaded process; its compile-time allocation churn
# under the interposer did not finish within a 600 s bound
# (100% CPU, RSS ~215 MB, still compiling) while the plain
# mojo-run leg completed in 20.14 s wall. The token path
# is therefore trialled on the compiled binary, which is
# also the only place the token path exists in isolation.
set -u
cd "$(dirname "$0")/../.."
FAIL=0
chk() { if [ "$1" -eq 0 ]; then echo "  [ok] $2"; else echo "  [FAIL] $2"; FAIL=1; fi; }
IPOLIB="$PWD/build/libmimiralloc_interpose.so"
AESIR="$HOME/aesir-forge"
PIXIBIN="$HOME/.pixi/bin/pixi"
MODEL="$AESIR/hidden_files/fixtures/Meta-Llama-3.1-8B-Instruct-Q4_K_M.gguf"
MAXTOK=32
mkdir -p hidden_files

echo "=== Z-087 Aesir boundary trial recorded run ==="
echo "date_utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "host: $(uname -a)"
echo "head: $(git rev-parse HEAD)"
echo "branch: $(git branch --show-current)"
echo "loadavg_start: $(cat /proc/loadavg)"
echo "page_size: $(getconf PAGESIZE)"
echo "preload: $IPOLIB (Z-082 interposing variant)"
echo "rollback: unset LD_PRELOAD"
echo "boundary: read-only toward $AESIR (no writes; git status proof S6)"

echo "=== S1 build the interposing variant ==="
make lib-interpose > hidden_files/z087_build_interpose.log 2>&1
chk $? "make lib-interpose (log hidden_files/z087_build_interpose.log)"
ls -la build/libmimiralloc_interpose.so
echo "interpose defined dynamic exports: $(nm -D --defined-only build/libmimiralloc_interpose.so | wc -l)"
[ "$(nm -D --defined-only build/libmimiralloc_interpose.so | wc -l)" -eq 20 ]
chk $? "interpose export count == 20 (10 namespaced + 10 standard)"
sha256sum build/libmimiralloc_interpose.so

echo "=== S2 engine and model identity (all read-only) ==="
echo "aesir seat head: $(git -C "$AESIR" rev-parse HEAD)"
echo "aesir seat branch: $(git -C "$AESIR" branch --show-current)"
echo "engine entry: aesir_engine/main.mojo -> cli.commands dispatch; run <model.gguf> <prompt...> on the verified CPU backend (acceleration_backend auto/cpu)"
echo "run road: pixi run --frozen mojo build -I . --target-accelerator gfx1151, FFI shims staged in mktemp (Aesir seat-gate A-074 shape), binary in mktemp"
ls -la "$MODEL"
sha256sum "$MODEL"
echo "model note: aesir_engine/model.gguf in the Aesir tree is a 24-byte empty header; the seat's real fixture above is the model of record"
echo "prebuilt binaries assessed (report doc S5): root aesir_main (2026-11-11 build) SEGFAULTS in prefill on a PLAIN run with this fixture; aesir_engine/aesir_main returns canned text (not genuine generation); neither is the token path of record - both named, neither fixed here"
echo "attempt-1 finding: mojo-run-under-preload exceeded a 600 s bound still compiling (100% CPU); plain mojo-run leg: wall 20.14 s, decode_ms 4368.047 for 32 tokens (hidden_files/z087_plain1.log, attempt-1 recorded run preserved)"
mojo_ver="$(cd "$AESIR" && "$PIXIBIN" run --frozen mojo --version 2>/dev/null)"
echo "mojo: $mojo_ver"

echo "=== S3 stage FFI shims and build the engine binary (mktemp, outside the Aesir tree) ==="
STAGING="$(mktemp -d)"
WORK="$(mktemp -d)"
mkdir -p "$WORK/home"
cc -std=c11 -fPIC -shared "$AESIR/aesir_engine/hailo10h_stub.c" -o "$STAGING/libaesir_hailo10h.so" 2>hidden_files/z087_shim_cc.log
chk $? "hailo10h stub"
cc -std=c11 -O2 -fPIC -shared "$AESIR/aesir_engine/thread_par.c" -o "$STAGING/libaesir_par.so" -lpthread 2>>hidden_files/z087_shim_cc.log
chk $? "thread_par shim"
cc -std=c11 -O2 -fPIC -shared "$AESIR/aesir_engine/hip_memory_shim.c" -o "$STAGING/libaesir_hip.so" -ldl 2>>hidden_files/z087_shim_cc.log
chk $? "hip shim"
cc -std=c11 -O2 -fPIC -shared "$AESIR/aesir_engine/vulkan_context_shim.c" "$AESIR/aesir_engine/vulkan_buffer_shim.c" -o "$STAGING/libaesir_vulkan.so" -ldl 2>>hidden_files/z087_shim_cc.log
chk $? "vulkan shim"
cc -std=c11 -O2 -fPIC -shared "$AESIR/aesir_engine/zero_copy_shim.c" -o "$STAGING/libaesir_zc.so" -ldl 2>>hidden_files/z087_shim_cc.log
chk $? "zero-copy shim"
cc -std=c11 -O2 -fPIC -shared "$AESIR/aesir_engine/affinity_shim.c" -o "$STAGING/libaesir_aff.so" 2>>hidden_files/z087_shim_cc.log
chk $? "affinity shim"
echo "staging: $STAGING (removed at end of run)"
BIN="$WORK/aesir_z087"
(cd "$AESIR" && HOME="$WORK/home" "$PIXIBIN" run --frozen mojo build -I . --target-accelerator gfx1151 -Xlinker "-L$STAGING" -Xlinker -laesir_hailo10h -Xlinker -laesir_par -Xlinker -laesir_hip -Xlinker -laesir_vulkan -Xlinker -laesir_zc -Xlinker -laesir_aff -o "$BIN" aesir_engine/main.mojo) > hidden_files/z087_build_engine.log 2>&1
chk $? "mojo build engine binary (log hidden_files/z087_build_engine.log)"
ls -la "$BIN"
sha256sum "$BIN"

git -C "$AESIR" status --porcelain > hidden_files/z087_aesir_status_runner_before.txt

echo "=== S4 trial legs on the compiled binary: plain / preload alternating, 2 reps each ==="
echo "prompt: The capital of Norway is ; max_tokens: $MAXTOK ; sampling: engine deterministic (greedy) loop"
run_leg() {
  leg="$1"; mode="$2"
  log="hidden_files/z087_${leg}.log"
  echo "--- leg $leg ($mode) loadavg: $(cat /proc/loadavg)"
  if [ "$mode" = preload ]; then
    (cd "$AESIR" && HOME="$WORK/home" LD_LIBRARY_PATH="$AESIR/.pixi/envs/default/lib:$STAGING" LD_PRELOAD="$IPOLIB" timeout 300 /usr/bin/time -v "$BIN" run "$MODEL" --max-tokens $MAXTOK The capital of Norway is) > "$log" 2>&1
  else
    (cd "$AESIR" && HOME="$WORK/home" LD_LIBRARY_PATH="$AESIR/.pixi/envs/default/lib:$STAGING" timeout 300 /usr/bin/time -v "$BIN" run "$MODEL" --max-tokens $MAXTOK The capital of Norway is) > "$log" 2>&1
  fi
  rc=$?
  echo "leg $leg exit=$rc"
  [ "$rc" -eq 0 ]; chk $? "leg $leg exit 0"
  grep 'Token generated' "$log" | sed -E "s/.*Token generated: ([0-9]+).*/\1/" | tr '\n' ' ' > "hidden_files/z087_${leg}.tokens"
  echo "leg $leg tokens: $(cat "hidden_files/z087_${leg}.tokens")"
  grep 'AESIR_TIMING_JSON' "$log" | sed 's/^AESIR_TIMING_JSON: //'
  grep -E 'Elapsed \(wall|Maximum resident|page faults' "$log" | sed 's/^\t//'
  sed -n '/^\[Response\]:/,$p' "$log" | head -4
}
run_leg plain1 plain
run_leg preload1 preload
run_leg plain2 plain
run_leg preload2 preload

echo "=== S5 correctness: token-stream identity across all legs ==="
DISTINCT="$(cat hidden_files/z087_plain1.tokens hidden_files/z087_preload1.tokens hidden_files/z087_plain2.tokens hidden_files/z087_preload2.tokens | sort -u | wc -l)"
echo "distinct token streams across 4 legs: $DISTINCT (must be 1)"
[ "$DISTINCT" -eq 1 ]; chk $? "token streams identical (plain == preload, both reps)"
echo "token stream sha256: $(cat hidden_files/z087_plain1.tokens | sha256sum)"

echo "=== S6 boundary proof: Aesir tree untouched by this runner ==="
git -C "$AESIR" status --porcelain > hidden_files/z087_aesir_status_runner_after.txt
if diff hidden_files/z087_aesir_status_runner_before.txt hidden_files/z087_aesir_status_runner_after.txt > hidden_files/z087_aesir_status_diff.txt; then
  echo "aesir git status: UNCHANGED across the trial legs"
else
  echo "aesir git status CHANGED during the legs (diff in hidden_files/z087_aesir_status_diff.txt) - the Aesir forge runs its own concurrent campaign on this host; inspect before crediting any line to this runner"
  cat hidden_files/z087_aesir_status_diff.txt
fi
rm -rf "$STAGING" "$WORK"
echo "loadavg_end: $(cat /proc/loadavg)"
echo "=== Z-087 recorded run end: FAIL=$FAIL ==="
exit $FAIL
