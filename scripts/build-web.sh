#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [ -f .tools/emsdk/emsdk_env.sh ]; then
  source .tools/emsdk/emsdk_env.sh
fi
if ! command -v emcmake >/dev/null; then
  printf 'Falta Emscripten. Ejecutá bash scripts/setup-web.sh primero.\n' >&2
  exit 1
fi
if [ ! -d .tools/raylib ]; then
  printf 'Falta raylib. Ejecutá bash scripts/setup-web.sh primero.\n' >&2
  exit 1
fi
emcmake cmake -S . -B build/web -DBUILD_WEB=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/web --parallel
