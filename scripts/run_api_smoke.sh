#!/usr/bin/env bash
# Start an isolated backend and database for the API smoke test.
set -euo pipefail
cd "$(dirname "$0")/.."

python3 - <<'PY'
import json
import os
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request
from pathlib import Path

with tempfile.TemporaryDirectory(prefix='wt-api-smoke-') as directory:
    root = Path(directory)
    with socket.socket() as candidate:
        candidate.bind(('127.0.0.1', 0))
        port = candidate.getsockname()[1]

    config = json.loads(Path('config.json').read_text())
    config['database']['path'] = str(root / 'smoke.db')
    config['server']['host'] = '127.0.0.1'
    config['server']['port'] = port
    config['frontend']['enabled'] = False
    config_path = root / 'config.json'
    config_path.write_text(json.dumps(config))
    log_path = root / 'backend.log'
    base = f'http://127.0.0.1:{port}'

    with log_path.open('w+') as log:
        server = subprocess.Popen(['./build/backend/wealth-trace', str(config_path)],
                                  stdout=log, stderr=subprocess.STDOUT)
        try:
            for _ in range(50):
                if server.poll() is not None:
                    raise RuntimeError(f'backend exited before becoming ready ({server.returncode})')
                try:
                    with urllib.request.urlopen(base + '/api/health', timeout=0.3) as response:
                        if json.load(response).get('data', {}).get('status') == 'ok':
                            break
                except Exception:
                    time.sleep(0.1)
            else:
                raise RuntimeError('backend did not become ready')
            if server.poll() is not None:
                raise RuntimeError('backend exited after health check')
            environment = os.environ.copy()
            environment['WT_SMOKE_BASE_URL'] = base
            result = subprocess.run([sys.executable, 'scripts/api_smoke.py'], env=environment)
            if result.returncode:
                raise RuntimeError(f'API smoke test failed ({result.returncode})')
        finally:
            if server.poll() is None:
                server.terminate()
            server.wait(timeout=5)
            log.flush()
            log.seek(0)
            print('--- backend log (tail) ---')
            print(''.join(log.readlines()[-15:]), end='')
PY
