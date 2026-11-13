#!/usr/bin/env bash
# Builds and runs the v2.29 combat-curve Monte Carlo against the shipped headers
# (app/src/main/cpp/CombatCurve.h + GameLogic.h). Prints the markdown tables used in the PR.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
src="$here/../../app/src/main/cpp"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
cp "$src"/{GameLogic.h,CombatCurve.h,Bestiary.h,SaveCodec.h} "$work"/
cp "$here/../migration_test/AndroidOut.h" "$here/combat_sim.cpp" "$work"/
ndk_bin="$(ls -d "${ANDROID_HOME:-$HOME/Android/Sdk}"/ndk/*/toolchains/llvm/prebuilt/linux-x86_64/bin 2>/dev/null | sort | tail -1 || true)"
if [[ -n "$ndk_bin" && -x "$ndk_bin/clang++" ]]; then
  cxx=("$ndk_bin/clang++" --target=x86_64-linux-android30 -static)
else
  cxx=("${CXX:-c++}")
fi
"${cxx[@]}" -std=c++17 -O2 -o "$work/combat_sim" "$work/combat_sim.cpp"
"$work/combat_sim"
