#!/usr/bin/env bash
# Random-input runs under AddressSanitizer + UBSan (Linux, g++): the player and the home with
# odd values (NaN, infinite or huge times, broken UTF-8, tiny windows, the mini player, every
# interface size), the scenes and the welcome at any size, and everything the network can send
# the phone remote (HTTP, WebSocket frames, JSON, the signed header, file names).
# Each seed is reproducible: tools/fuzz.sh [seeds] [first-seed]. A seed that does not finish in
# its time is reported as a hang (that is how the frozen "up next" in a short window was found).
set -u
cd "$(dirname "$0")/.."
seeds=${1:-6}; first=${2:-1}
out=build/fuzz; mkdir -p $out/obj
F="-std=c++20 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined"
objs=()
# (a header or a generated table newer than an object rebuilds it too: the translations live in
# i18n_catalog.inc, which i18n.cpp includes)
newest=$(ls -t src/*/*.h src/*/*.inc | head -1)
for src in src/core/*.cpp src/ui/*.cpp; do
  o=$out/obj/$(basename "${src%.cpp}").o
  [[ $o -nt $src && $o -nt $newest ]] || g++ $F -c "$src" -o "$o" || { echo "FAILED compile $src"; exit 1; }
  objs+=("$o")
done
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} UBSAN_OPTIONS=${UBSAN_OPTIONS:-print_stacktrace=1}
bad=0
run() {  # name, rounds/iterations, time limit
  g++ $F tools/fuzz_$1.cpp "${objs[@]}" -o $out/fuzz_$1 || { echo "FAILED compile fuzz_$1"; bad=1; return; }
  for ((s = first; s < first + seeds; ++s)); do
    timeout $3 $out/fuzz_$1 $s $2 > $out/$1-$s.log 2>&1; st=$?
    if [[ $st == 124 ]]; then echo "HANG fuzz_$1 seed $s"; bad=1
    elif [[ $st != 0 ]] || grep -q "Sanitizer\|runtime error" $out/$1-$s.log; then
      echo "FAILED fuzz_$1 seed $s: $(grep -m2 -E 'ERROR|runtime error' $out/$1-$s.log | tr '\n' ' ')"; bad=1
    fi
  done
}
run play 25 300
run views 10 300
run proto 100000 600
[[ $bad == 0 ]] && echo "fuzz: $seeds seeds of each, clean" || exit 1
