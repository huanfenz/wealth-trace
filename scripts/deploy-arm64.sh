#!/usr/bin/env bash
# Build locally, upload one release archive, and activate it over SSH.
set -Eeuo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
host='192.168.50.142'
user='root'
port='8081'
bind_host=''
archive="${project_dir}/dist/wealth-trace-linux-arm64.tar.gz"
skip_build=false

usage() {
  cat <<'EOF'
Usage: bash scripts/deploy-arm64.sh [--host ADDRESS] [--user USER] [--port PORT] [--bind IPV4] [--skip-build]

Builds the ARM64 backend and frontend locally, then deploys by SSH.
Defaults: root@192.168.50.142, application port 8081.
--skip-build uploads the existing dist/wealth-trace-linux-arm64.tar.gz.
--bind sets the interface address for the application; it defaults to --host when --host is IPv4.
SSH key authentication and sudo/root access on the target are required.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host|--user|--port|--bind)
      [[ $# -ge 2 ]] || { usage >&2; exit 2; }
      case "$1" in
        --host) host="$2" ;;
        --user) user="$2" ;;
        --port) port="$2" ;;
        --bind) bind_host="$2" ;;
      esac
      shift 2 ;;
    --skip-build) skip_build=true; shift ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
  esac
done

[[ "${host}" =~ ^[a-zA-Z0-9.-]+$ ]] || { printf 'Invalid host\n' >&2; exit 2; }
[[ "${user}" =~ ^[a-zA-Z_][a-zA-Z0-9_-]*$ ]] || { printf 'Invalid user\n' >&2; exit 2; }
[[ "${port}" =~ ^[0-9]{1,5}$ ]] || { printf 'Invalid port\n' >&2; exit 2; }
port="$((10#${port}))"
(( port >= 1 && port <= 65535 )) || {
  printf 'Invalid port\n' >&2; exit 2;
}
if [[ -z "${bind_host}" ]]; then bind_host="${host}"; fi
python3 -c 'import ipaddress, sys; ipaddress.IPv4Address(sys.argv[1])' "${bind_host}" 2>/dev/null || {
  printf 'Bind address must be IPv4; pass --bind IPV4 when --host is a DNS name.\n' >&2
  exit 2
}

if [[ "${skip_build}" == false ]]; then
  bash "${project_dir}/scripts/build-arm64.sh"
fi
[[ -f "${archive}" ]] || { printf 'Release archive not found: %s\n' "${archive}" >&2; exit 1; }

target="${user}@${host}"
ssh_options=(-o BatchMode=yes -o StrictHostKeyChecking=accept-new)
remote_archive="$(ssh "${ssh_options[@]}" "${target}" 'mktemp /tmp/wealth-trace-arm64.XXXXXXXX.tar.gz')"
trap 'ssh "${ssh_options[@]}" "${target}" "rm -f -- ${remote_archive}" >/dev/null 2>&1 || true' EXIT
rsync -a --chmod=F600 -- "${archive}" "${target}:${remote_archive}"
if [[ "${user}" == root ]]; then
  remote_shell='bash -s --'
else
  remote_shell='sudo -n bash -s --'
fi
ssh "${ssh_options[@]}" "${target}" "${remote_shell} ${remote_archive} ${bind_host} ${port}" \
  < "${project_dir}/scripts/remote-install-arm64.sh"
printf 'Deployed: http://%s:%s/\n' "${host}" "${port}"
