#!/usr/bin/env bash
# Verifies that the backend serves the built SPA and that API routes win.
set -u
cd "$(dirname "$0")/.."

rm -f data/caiji.db data/caiji.db-wal data/caiji.db-shm
./build/backend/wealth-trace config.json >/tmp/wt-static.log 2>&1 &
PID=$!
sleep 1.2

echo "--- / (SPA) ---"
curl -s -m 5 -o /tmp/wt-root.html -w "status=%{http_code} type=%{content_type}\n" http://127.0.0.1:8080/
head -c 60 /tmp/wt-root.html; echo
echo "--- /dashboard (SPA fallback) ---"
curl -s -m 5 -o /dev/null -w "status=%{http_code} type=%{content_type}\n" http://127.0.0.1:8080/dashboard
echo "--- /api/health (API wins) ---"
curl -s -m 5 -w "\nstatus=%{http_code} type=%{content_type}\n" http://127.0.0.1:8080/api/health
echo "--- traversal blocked ---"
curl -s -m 5 -o /dev/null -w "status=%{http_code}\n" "http://127.0.0.1:8080/../config.json"

kill "$PID" 2>/dev/null
wait "$PID" 2>/dev/null
