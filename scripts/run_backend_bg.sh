#!/usr/bin/env bash
# Starts the backend in the background (serving both the API and frontend/dist).
set -u
cd "$(dirname "$0")/.."

pkill -x wealth-trace 2>/dev/null
sleep 0.3

nohup ./build/backend/wealth-trace config.json >/tmp/wt-run.log 2>&1 &
PID=$!
disown "$PID" 2>/dev/null || true

sleep 1.5
echo "backend pid=$PID"
echo "--- health ---"
curl -s -m 5 http://127.0.0.1:8080/api/health || echo "(no response)"
echo
echo "--- log ---"
tail -n 8 /tmp/wt-run.log
