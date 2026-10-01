#!/usr/bin/env bash
# Runs on the target as root. Receives an archive, bind address, and port.
set -Eeuo pipefail

archive="${1:?archive required}"
bind_host="${2:?bind address required}"
port="${3:?port required}"
install_root='/opt/wealth-trace'
releases_dir="${install_root}/releases"
current_link="${install_root}/current"
config_file='/etc/wealth-trace/config.json'
unit_file='/etc/systemd/system/wealth-trace.service'

[[ "$(id -u)" == 0 ]] || { printf 'Deployment requires root or passwordless sudo.\n' >&2; exit 1; }
[[ "$(uname -m)" == aarch64 ]] || { printf 'Target is not ARM64.\n' >&2; exit 1; }
for command_name in python3 tar curl systemctl sha256sum; do
  command -v "${command_name}" >/dev/null || { printf 'Missing %s on target.\n' "${command_name}" >&2; exit 1; }
done

install -d -m 755 "${releases_dir}" /etc/wealth-trace
stage_dir="$(mktemp -d "${releases_dir}/.incoming.XXXXXXXX")"
config_next="$(mktemp /etc/wealth-trace/.config.next.XXXXXXXX)"
unit_next="$(mktemp /etc/systemd/system/.wealth-trace.next.XXXXXXXX)"
config_previous="$(mktemp /etc/wealth-trace/.config.previous.XXXXXXXX)"
unit_previous="$(mktemp /etc/systemd/system/.wealth-trace.previous.XXXXXXXX)"
had_config=false
had_unit=false
activated=false
previous_link=''
database_path=''
db_backup=''

cleanup() {
  rm -rf -- "${stage_dir}"
  rm -f -- "${archive}" "${config_next}" "${unit_next}" \
    "${config_previous}" "${unit_previous}" "${current_link}.next.$$"
}
rollback() {
  local exit_code=$?
  trap - ERR
  if [[ "${activated}" == true ]]; then
    printf 'Activation failed; restoring the previous service configuration.\n' >&2
    systemctl stop wealth-trace.service || true
    # 新版本重启时可能已对数据库执行迁移;若不还原,回滚后的旧程序会读到
    # 不兼容的新 schema 而崩溃。还原部署前的在线备份(含清理 WAL/SHM 残留)。
    if [[ -n "${db_backup}" && -f "${db_backup}" ]]; then
      printf 'Restoring pre-deploy database from %s\n' "${db_backup}" >&2
      rm -f -- "${database_path}" "${database_path}-wal" "${database_path}-shm"
      install -m 600 -o wealthtrace -g wealthtrace -- "${db_backup}" "${database_path}"
    fi
    if [[ -n "${previous_link}" ]]; then
      ln -sfn -- "${previous_link}" "${current_link}.next.$$"
      mv -Tf -- "${current_link}.next.$$" "${current_link}"
    else
      rm -f -- "${current_link}"
    fi
    if [[ "${had_config}" == true ]]; then cp -- "${config_previous}" "${config_file}"; else rm -f -- "${config_file}"; fi
    if [[ "${had_unit}" == true ]]; then cp -- "${unit_previous}" "${unit_file}"; else rm -f -- "${unit_file}"; fi
    systemctl daemon-reload || true
    if [[ "${had_unit}" == true ]]; then systemctl restart wealth-trace.service || true; fi
    rm -rf -- "${release_dir}"
  fi
  exit "${exit_code}"
}
trap cleanup EXIT
trap rollback ERR

tar --no-same-owner -xzf "${archive}" -C "${stage_dir}"
chmod 755 "${stage_dir}"
[[ -x "${stage_dir}/bin/wealth-trace" && -f "${stage_dir}/frontend/dist/index.html" &&
   -d "${stage_dir}/migrations" && -f "${stage_dir}/README.md" &&
   -f "${stage_dir}/docs/MCP.md" && -f "${stage_dir}/mcp/dist/index.js" &&
   -f "${stage_dir}/mcp/node_modules/@modelcontextprotocol/server/package.json" &&
   -f "${stage_dir}/mcp/node_modules/zod/package.json" ]] || {
  printf 'Incomplete release archive.\n' >&2
  exit 1
}
[[ "$(od -An -tx1 -N20 "${stage_dir}/bin/wealth-trace" | tr -d ' \n' | cut -c37-40)" == b700 ]] || {
  printf 'Backend binary is not AArch64 ELF.\n' >&2
  exit 1
}

id wealthtrace >/dev/null 2>&1 || useradd --system --home-dir /var/lib/wealth-trace --shell /usr/sbin/nologin wealthtrace
install -d -m 755 -o wealthtrace -g wealthtrace /var/lib/wealth-trace

if [[ -f "${config_file}" ]]; then
  cp -- "${config_file}" "${config_previous}"
  had_config=true
  config_source="${config_file}"
else
  config_source="${stage_dir}/config.json"
fi
if [[ -f "${unit_file}" ]]; then cp -- "${unit_file}" "${unit_previous}"; had_unit=true; fi
if [[ -L "${current_link}" ]]; then previous_link="$(readlink "${current_link}")"; fi

python3 - "${config_source}" "${config_next}" "${bind_host}" "${port}" <<'PY'
import json
import os
import sys

source, destination, bind_host, port = sys.argv[1:]
with open(source, encoding='utf-8') as stream:
    config = json.load(stream)
config.setdefault('server', {}).update(host=bind_host, port=int(port))
# 远程部署一律强制登录鉴权:该服务承载全部家庭财务数据,不允许匿名访问。
config.setdefault('auth', {})['mode'] = 'required'
config['auth'].pop('allow_unauthenticated_lan', None)
database = config.setdefault('database', {})
database.setdefault('path', '/var/lib/wealth-trace/caiji.db')
if not os.path.isabs(database['path']):
    database['path'] = '/var/lib/wealth-trace/caiji.db'
database['migrations_dir'] = '/opt/wealth-trace/current/migrations'
config.setdefault('frontend', {}).update(dir='/opt/wealth-trace/current/frontend/dist', enabled=True)
with open(destination, 'w', encoding='utf-8') as stream:
    json.dump(config, stream, ensure_ascii=False, indent=2)
    stream.write('\n')
PY
chmod 644 "${config_next}"

cat > "${unit_next}" <<'UNIT'
[Unit]
Description=Wealth Trace family asset manager
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=wealthtrace
Group=wealthtrace
WorkingDirectory=/opt/wealth-trace/current
ExecStart=/opt/wealth-trace/current/bin/wealth-trace /etc/wealth-trace/config.json
Restart=on-failure
RestartSec=5
StateDirectory=wealth-trace
ProtectSystem=full
PrivateTmp=true
NoNewPrivileges=true

[Install]
WantedBy=multi-user.target
UNIT
chmod 644 "${unit_next}"

release_id="$(date -u +%Y%m%dT%H%M%SZ)-$(sha256sum "${archive}" | cut -c1-12)"
release_dir="${releases_dir}/${release_id}"
[[ ! -e "${release_dir}" ]] || { printf 'Release already exists: %s\n' "${release_dir}" >&2; exit 1; }

# SQLite backup includes WAL contents while the old service stays online.
database_path="$(python3 - "${config_next}" <<'PY'
import json, sys
with open(sys.argv[1], encoding='utf-8') as stream:
    print(json.load(stream)['database']['path'])
PY
)"
if [[ -f "${database_path}" ]]; then
  backup_dir='/var/lib/wealth-trace/backups'
  # The service imports backups as wealthtrace; keep the directory writable
  # by that account even when this installer runs as root. install -d also
  # repairs ownership on directories created by older releases.
  install -d -m 700 -o wealthtrace -g wealthtrace "${backup_dir}"
  db_backup="${backup_dir}/${release_id}.db"
  python3 - "${database_path}" "${db_backup}" <<'PY'
import sqlite3, sys
source = sqlite3.connect(f'file:{sys.argv[1]}?mode=ro', uri=True)
backup = sqlite3.connect(sys.argv[2])
source.backup(backup)
backup.close()
source.close()
PY
  chmod 600 "${db_backup}"
  printf 'Database backup: %s\n' "${db_backup}"
fi

mv -- "${stage_dir}" "${release_dir}"
stage_dir="${releases_dir}/.not-present.$$"
activated=true
ln -s -- "${release_dir}" "${current_link}.next.$$"
mv -Tf -- "${current_link}.next.$$" "${current_link}"
mv -f -- "${config_next}" "${config_file}"
mv -f -- "${unit_next}" "${unit_file}"
systemctl daemon-reload
systemctl enable wealth-trace.service
systemctl restart wealth-trace.service

healthy=false
for attempt in $(seq 1 20); do
  if curl --noproxy '*' -fsS --max-time 2 -o /dev/null "http://${bind_host}:${port}/api/health" 2>/dev/null; then
    healthy=true
    break
  fi
  sleep 1
done
if [[ "${healthy}" != true ]]; then
  journalctl -u wealth-trace.service -n 30 --no-pager >&2 || true
  false
fi
printf 'Active release: %s\n' "${release_dir}"
systemctl --no-pager --quiet is-active wealth-trace.service
printf '\n'
printf '登录鉴权已启用(auth.mode=required)。\n'
printf '首次访问 http://%s:%s 时,浏览器会引导创建管理员账号;\n' "${bind_host}" "${port}"
printf 'MCP 使用需配置 WEALTH_TRACE_API_USER / WEALTH_TRACE_API_PASSWORD 环境变量。\n'
