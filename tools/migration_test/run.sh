#!/usr/bin/env bash
# Save-migration test for the native game core (Bestiary IDs + legacy/renamed names).
# Builds Game.cpp + migration_test.cpp for the host with the NDK clang (x86_64, static)
# or a system clang++/g++, using a stub AndroidOut.h. Usage: tools/migration_test/run.sh
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
src="$here/../../app/src/main/cpp"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
cp "$src"/{Game.cpp,Game.h,GameLogic.h,QuestScript.h,EncounterAuthorship.h,Bestiary.h} "$work"/
cp "$here/AndroidOut.h" "$here/migration_test.cpp" "$work"/
ndk_bin="$(ls -d "${ANDROID_HOME:-$HOME/Android/Sdk}"/ndk/*/toolchains/llvm/prebuilt/linux-x86_64/bin 2>/dev/null | sort | tail -1 || true)"
if [[ -n "$ndk_bin" && -x "$ndk_bin/clang++" ]]; then
  cxx=("$ndk_bin/clang++" --target=x86_64-linux-android30 -static)
else
  cxx=("${CXX:-c++}")
fi
"${cxx[@]}" -std=c++17 -O0 -o "$work/migration_test" "$work/migration_test.cpp" "$work/Game.cpp"
"$work/migration_test"
