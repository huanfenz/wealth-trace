#!/usr/bin/env bash
# Starts the backend on a clean database, runs the API smoke test, then stops it.
set -u
cd "$(dirname "$0")/.."

rm -f data/caiji.db data/caiji.db-wal data/caiji.db-shm
./build/backend/wealth-trace config.json >/tmp/wt-backend.log 2>&1 &
PID=$!
sleep 1.2

python3 scripts/api_smoke.py
RC=$?

kill "$PID" 2>/dev/null
wait "$PID" 2>/dev/null
echo "--- backend log (tail) ---"
tail -n 15 /tmp/wt-backend.log
exit $RC
