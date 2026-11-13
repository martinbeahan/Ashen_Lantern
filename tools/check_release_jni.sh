#!/usr/bin/env bash
# Release/R8 sanity check (Code Auditor L9): every `external fun` in MainActivity.kt must
#  1) be exported from libdndbeginnerremote.so as Java_com_fintrack_dndbeginnerremote_MainActivity_<name>
#  2) still be a native method of com.fintrack.dndbeginnerremote.MainActivity in the (minified) dex.
# Usage: tools/check_release_jni.sh [path/to/app-release.apk]
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
apk="${1:-$(ls "$root"/app/build/outputs/apk/release/*.apk | head -1)}"
sdk="${ANDROID_HOME:-$HOME/Android/Sdk}"
bt="$(ls -d "$sdk"/build-tools/* | sort -V | tail -1)"
nm="$(ls "$sdk"/ndk/*/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm | sort -V | tail -1)"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
names="$(grep -oE '^\s*external fun [A-Za-z0-9_]+' "$root/app/src/main/java/com/fintrack/dndbeginnerremote/MainActivity.kt" | awk '{print $3}' | sort -u)"
unzip -q -o "$apk" 'lib/*' 'classes*.dex' -d "$work"
fail=0
for so in "$work"/lib/*/libdndbeginnerremote.so; do
  syms="$("$nm" -D --defined-only "$so" | awk '{print $3}')"
  for n in $names; do
    grep -qx "Java_com_fintrack_dndbeginnerremote_MainActivity_$n" <<<"$syms" || { echo "MISSING symbol $n in $so"; fail=1; }
  done
done
dump="$(for d in "$work"/classes*.dex; do "$bt/dexdump" "$d"; done)"
cls="$(awk '/Class descriptor  : .Lcom\/fintrack\/dndbeginnerremote\/MainActivity;./{f=1;next} /Class descriptor/{f=0} f' <<<"$dump")"
[[ -n "$cls" ]] || { echo "MainActivity class missing or renamed in dex"; exit 1; }
for n in $names; do
  grep -A2 -E "name +: '$n'" <<<"$cls" | grep -q "NATIVE" || { echo "NOT native / renamed in dex: $n"; fail=1; }
done
count="$(wc -w <<<"$names")"
[[ $fail == 0 ]] && echo "release JNI check OK: $count external funs, symbols in every ABI, natives kept in dex" || exit 1
