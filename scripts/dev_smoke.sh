#!/usr/bin/env bash
# Starts backend + Vite dev server, checks the SPA and the proxied API, then stops both.
set -u
cd "$(dirname "$0")/.."
# Prefer an nvm-managed node when present, otherwise rely on PATH.
NODE_BIN="$(ls -d "$HOME"/.nvm/versions/node/*/bin 2>/dev/null | sort -V | tail -1)"
if [ -n "$NODE_BIN" ]; then
  export PATH="$NODE_BIN:$PATH"
fi

rm -f data/caiji.db data/caiji.db-wal data/caiji.db-shm
./build/backend/wealth-trace config.json >/tmp/wt-dev-backend.log 2>&1 &
BACKEND=$!

(cd frontend && npm run dev -- --host 127.0.0.1 --port 5173 >/tmp/wt-vite.log 2>&1) &
VITE=$!

sleep 4
echo "--- SPA root ---"
curl -s -m 5 http://127.0.0.1:5173/ | head -c 200
echo
echo "--- proxied API ---"
curl -s -m 5 http://127.0.0.1:5173/api/health
echo

kill "$VITE" 2>/dev/null
pkill -P "$VITE" 2>/dev/null
kill "$BACKEND" 2>/dev/null
wait "$BACKEND" 2>/dev/null
sleep 0.3
echo "--- vite log tail ---"
tail -n 8 /tmp/wt-vite.log
