#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .tools
if [ ! -d .tools/raylib ]; then
  git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git .tools/raylib
fi
if ! command -v emcmake >/dev/null; then
  if [ ! -d .tools/emsdk ]; then
    git clone --depth 1 https://github.com/emscripten-core/emsdk.git .tools/emsdk
  fi
  .tools/emsdk/emsdk install 4.0.23
  .tools/emsdk/emsdk activate 4.0.23
fi
bash scripts/build-web.sh
