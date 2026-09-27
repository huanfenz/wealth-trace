#!/usr/bin/env bash
# Build both applications locally and produce a self-contained ARM64 release archive.
set -Eeuo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${project_dir}/build-arm64"
archive="${project_dir}/dist/wealth-trace-linux-arm64.tar.gz"

if ! command -v aarch64-linux-gnu-g++ >/dev/null 2>&1; then
  bash "${project_dir}/scripts/install-arm64-toolchain.sh"
  export PATH="${project_dir}/.toolchains/arm64/bin:${PATH}"
fi

for command_name in meson ninja npm node tar aarch64-linux-gnu-gcc aarch64-linux-gnu-g++ aarch64-linux-gnu-ar aarch64-linux-gnu-strip; do
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    printf 'Missing %s. See the ARM64 deployment section in README.md.\n' "${command_name}" >&2
    exit 1
  fi
done
if ! node -e 'process.exit(Number(process.versions.node.split(".")[0]) >= 20 ? 0 : 1)'; then
  printf 'Building the MCP server requires Node.js 20 or newer.\n' >&2
  exit 1
fi

if [[ ! -x "${project_dir}/frontend/node_modules/.bin/vite" ||
      ! -x "${project_dir}/frontend/node_modules/.bin/vue-tsc" ]]; then
  (cd "${project_dir}/frontend" && npm ci)
fi

if [[ -f "${build_dir}/build.ninja" ]]; then
  meson setup --reconfigure "${build_dir}" \
    --cross-file "${project_dir}/scripts/cross/aarch64-linux-gnu.ini" \
    -Denable_tests=false --buildtype=release
else
  meson setup "${build_dir}" \
    --cross-file "${project_dir}/scripts/cross/aarch64-linux-gnu.ini" \
    -Denable_tests=false --buildtype=release
fi
meson compile -C "${build_dir}" -j "${ARM64_JOBS:-2}"
(cd "${project_dir}/frontend" && npm run build)
(cd "${project_dir}/mcp" && npm ci --no-audit --no-fund && npm run build)

stage_dir="$(mktemp -d)"
trap 'rm -rf -- "${stage_dir}"' EXIT
install -d "${stage_dir}/bin" "${stage_dir}/frontend" "${stage_dir}/mcp" "${project_dir}/dist"
install -m 755 "${build_dir}/backend/wealth-trace" "${stage_dir}/bin/wealth-trace"
cp -a "${project_dir}/frontend/dist" "${stage_dir}/frontend/dist"
cp -a "${project_dir}/migrations" "${stage_dir}/migrations"
cp -a "${project_dir}/docs" "${stage_dir}/docs"
cp -a "${project_dir}/mcp/dist" "${stage_dir}/mcp/dist"
cp "${project_dir}/mcp/package.json" "${project_dir}/mcp/package-lock.json" "${stage_dir}/mcp/"
cp "${project_dir}/README.md" "${stage_dir}/README.md"
cp "${project_dir}/config.json" "${stage_dir}/config.json"
# Install only runtime dependencies into the archive while the build machine
# still has npm access. The target needs Node.js, but no npm or registry access.
npm ci --prefix "${stage_dir}/mcp" --omit=dev --offline --no-audit --no-fund
tar -C "${stage_dir}" -czf "${archive}.tmp" .
mv -f "${archive}.tmp" "${archive}"
printf 'ARM64 release: %s\n' "${archive}"
sha256sum "${archive}"
