#!/usr/bin/env bash
# Start isolated backends and databases for the API smoke tests:
#   1) anonymous mode (auth disabled)  — full money-flow assertions
#   2) auth mode     (auth required)   — setup/login/401/logout assertions
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


def wait_ready(base, server):
    for _ in range(50):
        if server.poll() is not None:
            raise RuntimeError(f'backend exited before becoming ready ({server.returncode})')
        try:
            with urllib.request.urlopen(base + '/api/health', timeout=0.3) as response:
                if json.load(response).get('data', {}).get('status') == 'ok':
                    return
        except Exception:
            time.sleep(0.1)
    raise RuntimeError('backend did not become ready')


def run_instance(label, auth_mode, auth_env):
    with tempfile.TemporaryDirectory(prefix=f'wt-api-smoke-{label}-') as directory:
        root = Path(directory)
        with socket.socket() as candidate:
            candidate.bind(('127.0.0.1', 0))
            port = candidate.getsockname()[1]

        config = json.loads(Path('config.json').read_text())
        config['database']['path'] = str(root / 'smoke.db')
        config['server']['host'] = '127.0.0.1'
        config['server']['port'] = port
        config['frontend']['enabled'] = False
        config['auth'] = {'mode': auth_mode}
        config_path = root / 'config.json'
        config_path.write_text(json.dumps(config))
        log_path = root / 'backend.log'
        base = f'http://127.0.0.1:{port}'

        with log_path.open('w+') as log:
            server = subprocess.Popen(['./build/backend/wealth-trace', str(config_path)],
                                      stdout=log, stderr=subprocess.STDOUT)
            try:
                wait_ready(base, server)
                if server.poll() is not None:
                    raise RuntimeError('backend exited after health check')
                environment = os.environ.copy()
                environment['WT_SMOKE_BASE_URL'] = base
                if auth_env:
                    environment.update(auth_env)
                result = subprocess.run([sys.executable, 'scripts/api_smoke.py'], env=environment)
                if result.returncode:
                    raise RuntimeError(f'API smoke test failed ({result.returncode})')
            finally:
                if server.poll() is None:
                    server.terminate()
                server.wait(timeout=5)
                log.flush()
                log.seek(0)
                print(f'--- backend log [{label}] (tail) ---')
                print(''.join(log.readlines()[-15:]), end='')


run_instance('anonymous', 'disabled', {})
run_instance('auth', 'required', {'WT_SMOKE_AUTH': '1'})
print('All API smoke tests passed')
PY
