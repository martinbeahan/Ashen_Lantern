#!/usr/bin/env bash
# Save-migration + save-safety + Rekindling (v2.33, prestige_test) + Kindling (v2.25 story) + Memory Ledger (v2.26) + save slots (v2.28) + combat curve (v2.29) tests for the native game core (Bestiary IDs, legacy/renamed
# names, escaped text fields, transactional load, class validation).
# Builds Game.cpp + GameSave.cpp + the tests for the host with the NDK clang (x86_64, static)
# or a system clang++/g++, using a stub AndroidOut.h. Usage: tools/migration_test/run.sh
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
src="$here/../../app/src/main/cpp"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
cp "$src"/{Game.cpp,GameSave.cpp,Game.h,GameLogic.h,QuestScript.h,EncounterAuthorship.h,Bestiary.h,SaveCodec.h,MemoryCatalog.h,CombatCurve.h,EnemyAi.h,StatRules.h,Perks.h,GamePerks.cpp} "$work"/
cp "$here/AndroidOut.h" "$here/migration_test.cpp" "$here/save_safety_test.cpp" "$here/kindling_test.cpp" "$here/memory_test.cpp" "$here/slots_test.cpp" "$work"/
ndk_bin="$(ls -d "${ANDROID_HOME:-$HOME/Android/Sdk}"/ndk/*/toolchains/llvm/prebuilt/linux-x86_64/bin 2>/dev/null | sort | tail -1 || true)"
if [[ -n "$ndk_bin" && -x "$ndk_bin/clang++" ]]; then
  cxx=("$ndk_bin/clang++" --target=x86_64-linux-android30 -static)
else
  cxx=("${CXX:-c++}")
fi
"${cxx[@]}" -std=c++17 -O0 -o "$work/migration_test" "$work/migration_test.cpp" "$work/Game.cpp" "$work/GameSave.cpp" "$work/GamePerks.cpp"
"$work/migration_test"
# v2.36 #91: save_safety_test traps on any signed int overflow (the INT_MAX / fuzz cases must stay UB-free).
trap_ub=()
if [[ -n "$ndk_bin" && -x "$ndk_bin/clang++" ]]; then trap_ub=(-fsanitize=signed-integer-overflow -fsanitize-trap=signed-integer-overflow); fi
"${cxx[@]}" "${trap_ub[@]}" -std=c++17 -O0 -o "$work/save_safety_test" "$work/save_safety_test.cpp" "$work/Game.cpp" "$work/GameSave.cpp" "$work/GamePerks.cpp"
"$work/save_safety_test"
"${cxx[@]}" -std=c++17 -O0 -o "$work/kindling_test" "$work/kindling_test.cpp" "$work/Game.cpp" "$work/GameSave.cpp" "$work/GamePerks.cpp"
"$work/kindling_test"
"${cxx[@]}" -std=c++17 -O0 -o "$work/memory_test" "$work/memory_test.cpp" "$work/Game.cpp" "$work/GameSave.cpp" "$work/GamePerks.cpp"
"$work/memory_test"
"${cxx[@]}" -std=c++17 -O0 -o "$work/slots_test" "$work/slots_test.cpp" "$work/Game.cpp" "$work/GameSave.cpp" "$work/GamePerks.cpp"
"$work/slots_test"
cp "$here/combat_test.cpp" "$work"/
"${cxx[@]}" -std=c++17 -O0 -o "$work/combat_test" "$work/combat_test.cpp" "$work/Game.cpp" "$work/GameSave.cpp" "$work/GamePerks.cpp"
"$work/combat_test"
cp "$here/ai_test.cpp" "$work"/
"${cxx[@]}" -std=c++17 -O0 -o "$work/ai_test" "$work/ai_test.cpp" "$work/Game.cpp" "$work/GameSave.cpp" "$work/GamePerks.cpp"
"$work/ai_test"
cp "$here/stats_test.cpp" "$work"/
"${cxx[@]}" -std=c++17 -O0 -o "$work/stats_test" "$work/stats_test.cpp" "$work/Game.cpp" "$work/GameSave.cpp" "$work/GamePerks.cpp"
"$work/stats_test"
cp "$here/perks_test.cpp" "$work"/
"${cxx[@]}" -std=c++17 -O0 -o "$work/perks_test" "$work/perks_test.cpp" "$work/Game.cpp" "$work/GameSave.cpp" "$work/GamePerks.cpp"
"$work/perks_test"
cp "$here/prestige_test.cpp" "$work"/
"${cxx[@]}" -std=c++17 -O0 -o "$work/prestige_test" "$work/prestige_test.cpp" "$work/Game.cpp" "$work/GameSave.cpp" "$work/GamePerks.cpp"
"$work/prestige_test"
