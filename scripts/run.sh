#!/usr/bin/env bash
# One-command build and launch for both development and production.
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BACKEND_BIN="${BUILD_DIR}/backend/wealth-trace"

usage() {
  cat <<'EOF'
Usage: bash scripts/run.sh <dev|prod> [config-file]

Modes:
  dev   Build the backend, start it on :8080, then start Vite on :5173.
        Vite proxies /api requests to the backend and provides hot reload.
  prod  Build the frontend and backend, then start the backend on :8080.
        The backend serves frontend/dist and provides SPA route fallback.

The optional config file defaults to config.json in the project root.
Use Ctrl-C to stop the active server(s).
EOF
}

die() {
  printf 'Error: %s\n' "$*" >&2
  exit 1
}

require_command() {
  command -v "$1" >/dev/null 2>&1 || die "missing required command: $1"
}

prepare_frontend() {
  require_command npm

  if [[ ! -d "${PROJECT_DIR}/frontend/node_modules" ]]; then
    printf '%s\n' 'Installing frontend dependencies...'
    (
      cd "${PROJECT_DIR}/frontend"
      if [[ -f package-lock.json ]]; then
        npm ci
      else
        npm install
      fi
    )
  fi
}

build_backend() {
  require_command meson

  if [[ -f "${BUILD_DIR}/build.ninja" ]]; then
    meson setup --reconfigure "${BUILD_DIR}"
  else
    meson setup "${BUILD_DIR}"
  fi
  meson compile -C "${BUILD_DIR}"
}

backend_pid=''
vite_pid=''

cleanup() {
  local exit_code=$?
  trap - EXIT INT TERM

  if [[ -n "${vite_pid}" ]] && kill -0 "${vite_pid}" 2>/dev/null; then
    kill "${vite_pid}" 2>/dev/null || true
  fi
  if [[ -n "${backend_pid}" ]] && kill -0 "${backend_pid}" 2>/dev/null; then
    kill "${backend_pid}" 2>/dev/null || true
  fi
  wait "${vite_pid:-}" 2>/dev/null || true
  wait "${backend_pid:-}" 2>/dev/null || true
  exit "${exit_code}"
}

mode="${1:-}"
case "${mode}" in
  dev|prod)
    shift
    ;;
  -h|--help|'')
    usage
    exit 0
    ;;
  *)
    usage >&2
    die "unknown mode: ${mode}"
    ;;
esac

[[ $# -le 1 ]] || die 'only one optional config-file argument is supported'
CONFIG_FILE="${1:-${PROJECT_DIR}/config.json}"
[[ -f "${CONFIG_FILE}" ]] || die "configuration file not found: ${CONFIG_FILE}"

cd "${PROJECT_DIR}"
prepare_frontend
build_backend

if [[ "${mode}" == 'prod' ]]; then
  printf '%s\n' 'Building frontend for backend static hosting...'
  (
    cd "${PROJECT_DIR}/frontend"
    npm run build
  )

  printf 'Starting production server at http://127.0.0.1:8080 (config: %s)\n' "${CONFIG_FILE}"
  exec "${BACKEND_BIN}" "${CONFIG_FILE}"
fi

printf 'Starting development backend at http://127.0.0.1:8080 (config: %s)\n' "${CONFIG_FILE}"
"${BACKEND_BIN}" "${CONFIG_FILE}" &
backend_pid=$!
trap cleanup EXIT INT TERM

# Fail early instead of leaving Vite running when the backend cannot bind or start.
sleep 1
if ! kill -0 "${backend_pid}" 2>/dev/null; then
  wait "${backend_pid}"
fi

printf '%s\n' 'Starting Vite development server at http://127.0.0.1:5173 ...'
(
  cd "${PROJECT_DIR}/frontend"
  exec npm run dev -- --host 127.0.0.1 --port 5173
) &
vite_pid=$!
wait "${vite_pid}"
