#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [ ! -x build/native/maze_export ]; then bash scripts/build-native.sh; fi
uv run --cache-dir .cache/uv uvicorn server.app:app --host 0.0.0.0 --port 8000
