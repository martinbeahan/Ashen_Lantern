#!/usr/bin/env bash
# Combat simulators, built from the shipped native code:
#  1) combat_sim — the v2.29 curve primitives (CombatCurve.h + GameLogic.h), basic attacks only.
#  2) ai_sim     — v2.31: full Game fights (Game.cpp) with the party on auto and the enemy AI on,
#                  v2.30 stat rules vs v2.31 universal stats. Optional arg: fights per cell (default 1000).
# Prints the markdown tables used in the PRs. Usage: tools/combat_sim/run.sh [fights]
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
src="$here/../../app/src/main/cpp"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
cp "$src"/{Game.cpp,GameSave.cpp,Game.h,GameLogic.h,QuestScript.h,EncounterAuthorship.h,Bestiary.h,SaveCodec.h,MemoryCatalog.h,CombatCurve.h,EnemyAi.h,StatRules.h} "$work"/
cp "$here/../migration_test/AndroidOut.h" "$here/combat_sim.cpp" "$here/ai_sim.cpp" "$work"/
ndk_bin="$(ls -d "${ANDROID_HOME:-$HOME/Android/Sdk}"/ndk/*/toolchains/llvm/prebuilt/linux-x86_64/bin 2>/dev/null | sort | tail -1 || true)"
if [[ -n "$ndk_bin" && -x "$ndk_bin/clang++" ]]; then
  cxx=("$ndk_bin/clang++" --target=x86_64-linux-android30 -static)
else
  cxx=("${CXX:-c++}")
fi
"${cxx[@]}" -std=c++17 -O2 -o "$work/combat_sim" "$work/combat_sim.cpp"
"$work/combat_sim"
echo
"${cxx[@]}" -std=c++17 -O2 -o "$work/ai_sim" "$work/ai_sim.cpp" "$work/Game.cpp" "$work/GameSave.cpp"
"$work/ai_sim" "${1:-1000}"
