#!/usr/bin/env bash
# One-command build and launch for both development and production.
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BACKEND_BIN="${BUILD_DIR}/backend/wealth-trace"
VITE_PORT=5173
DEFAULT_BACKEND_PORT=8080

usage() {
  cat <<'EOF'
Usage: bash scripts/run.sh <dev|prod> [config-file]

Modes:
  dev   Build the backend, start it on :8080, then start Vite on :5173.
        Vite proxies /api requests to the backend and provides hot reload.
  prod  Build the frontend and backend, then start the backend on :8080.
        The backend serves frontend/dist and provides SPA route fallback.

The optional config file defaults to config.json in the project root.
The required backend/Vite ports must be free before starting. Use Ctrl-C to
stop the server(s) started by this script.
EOF
}

die() {
  printf 'Error: %s\n' "$*" >&2
  exit 1
}

require_command() {
  command -v "$1" >/dev/null 2>&1 || die "missing required command: $1"
}

# 从配置文件中提取 server.port，缺失时回退到默认端口。
config_port() {
  local port
  port="$(sed -n 's/.*"port"[[:space:]]*:[[:space:]]*\([0-9]\{1,\}\).*/\1/p' "$1" | head -n 1)"
  printf '%s' "${port:-${DEFAULT_BACKEND_PORT}}"
}

# 列出监听指定端口的进程 PID（优先 ss，其次 lsof；都不可用时返回空）。
port_pids() {
  local port="$1"
  if command -v ss >/dev/null 2>&1; then
    ss -ltnpH "sport = :${port}" 2>/dev/null |
      sed -n 's/.*pid=\([0-9]\{1,\}\).*/\1/p' | sort -u
  elif command -v lsof >/dev/null 2>&1; then
    lsof -tiTCP:"${port}" -sTCP:LISTEN 2>/dev/null | sort -u
  fi
}

# 启动前确认端口可用。不能仅凭端口判断进程是否由本脚本启动，
# 因此绝不终止监听进程，避免误杀其它项目或服务。
require_free_port() {
  local port="$1"
  local -a pid_list
  mapfile -t pid_list < <(port_pids "${port}")

  if [[ ${#pid_list[@]} -eq 0 ]]; then
    return 0
  fi

  die "port ${port} is already in use by PID(s): ${pid_list[*]}; stop the owning process or choose another port"
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

BACKEND_PORT="$(config_port "${CONFIG_FILE}")"

if [[ "${mode}" == 'prod' ]]; then
  printf '%s\n' 'Building frontend for backend static hosting...'
  (
    cd "${PROJECT_DIR}/frontend"
    npm run build
  )

  require_free_port "${BACKEND_PORT}"
  printf 'Starting production server at http://127.0.0.1:%s (config: %s)\n' \
    "${BACKEND_PORT}" "${CONFIG_FILE}"
  exec "${BACKEND_BIN}" "${CONFIG_FILE}"
fi

require_free_port "${BACKEND_PORT}"
require_free_port "${VITE_PORT}"
printf 'Starting development backend at http://127.0.0.1:%s (config: %s)\n' \
  "${BACKEND_PORT}" "${CONFIG_FILE}"
"${BACKEND_BIN}" "${CONFIG_FILE}" &
backend_pid=$!
trap cleanup EXIT INT TERM

# Fail early instead of leaving Vite running when the backend cannot bind or start.
sleep 1
if ! kill -0 "${backend_pid}" 2>/dev/null; then
  wait "${backend_pid}"
fi

printf 'Starting Vite development server at http://127.0.0.1:%s ...\n' "${VITE_PORT}"
(
  cd "${PROJECT_DIR}/frontend"
  exec npm run dev -- --host 127.0.0.1 --port "${VITE_PORT}"
) &
vite_pid=$!
wait "${vite_pid}"
