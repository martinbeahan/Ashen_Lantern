#!/usr/bin/env bash
# v2.37 #90: test firebase/database.rules.json on the local Realtime Database emulator (needs Node 18+
# and Java 11+; first run downloads firebase-tools and the emulator). Never touches a real project.
set -euo pipefail
cd "$(dirname "$0")"
[[ -d node_modules ]] || npm install --no-audit --no-fund --silent
npx firebase --project demo-ashen-lantern --config ../../firebase/firebase.json emulators:exec --only database "node rules.test.mjs"
