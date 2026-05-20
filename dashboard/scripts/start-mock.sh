#!/usr/bin/env bash
# Start bridge (mock telemetry) + Vite dev server. Run from repo root or dashboard/.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BRIDGE="$ROOT/bridge"

echo "Starting WebSocket bridge (mock) on ws://127.0.0.1:8765 ..."
python3 "$BRIDGE/server.py" --mock &
BRIDGE_PID=$!
trap 'kill $BRIDGE_PID 2>/dev/null' EXIT

sleep 1
cd "$ROOT"
if ! command -v npm >/dev/null 2>&1; then
  echo "npm not found. Install Node.js, then: cd dashboard && npm install && npm run dev"
  wait $BRIDGE_PID
  exit 1
fi
[[ -d node_modules ]] || npm install
npm run dev
