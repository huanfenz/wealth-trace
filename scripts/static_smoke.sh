#!/usr/bin/env bash
# Verifies that the backend serves the built SPA and that API routes win.
# 后端运行在临时目录的独立配置与数据库上,绝不触碰 data/ 下的真实数据。
set -u
cd "$(dirname "$0")/.."

SMOKE_ROOT="$(mktemp -d /tmp/wt-static-smoke.XXXXXX)"
trap 'rm -rf "$SMOKE_ROOT"' EXIT

python3 - "$SMOKE_ROOT" <<'PY'
import json, sys
from pathlib import Path

root = Path(sys.argv[1])
config = json.loads(Path('config.json').read_text())
config['database']['path'] = str(root / 'smoke.db')
config['server']['host'] = '127.0.0.1'
(root / 'config.json').write_text(json.dumps(config))
PY

./build/backend/wealth-trace "$SMOKE_ROOT/config.json" >/tmp/wt-static.log 2>&1 &
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
