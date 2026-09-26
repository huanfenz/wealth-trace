#!/usr/bin/env bash
# Download Ubuntu 22.04 cross-compiler packages into the workspace without sudo.
set -Eeuo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
toolchain_dir="${project_dir}/.toolchains/arm64"

source /etc/os-release
[[ "${ID}" == ubuntu && "${VERSION_ID}" == 22.04 && "$(uname -m)" == x86_64 ]] || {
  printf 'Automatic toolchain setup supports x86_64 Ubuntu 22.04 only. Install aarch64-linux-gnu-g++ using your OS package manager.\n' >&2
  exit 1
}
for command_name in apt dpkg-deb; do
  command -v "${command_name}" >/dev/null || { printf 'Missing %s\n' "${command_name}" >&2; exit 1; }
done

install -d "${toolchain_dir}/bin"
if [[ ! -x "${toolchain_dir}/root/usr/lib/gcc-cross/aarch64-linux-gnu/11/cc1plus" ]]; then
  work_dir="$(mktemp -d "${toolchain_dir}/.install.XXXXXXXX")"
  trap 'rm -rf -- "${work_dir}"' EXIT
  install -d "${work_dir}/packages" "${work_dir}/root"
  packages=(
    cpp-11-aarch64-linux-gnu gcc-11-aarch64-linux-gnu gcc-11-aarch64-linux-gnu-base
    g++-11-aarch64-linux-gnu binutils-aarch64-linux-gnu
    libgcc-11-dev-arm64-cross libstdc++-11-dev-arm64-cross
    libc6-dev-arm64-cross libc6-arm64-cross linux-libc-dev-arm64-cross
    libstdc++6-arm64-cross libgcc-s1-arm64-cross libgomp1-arm64-cross
    libitm1-arm64-cross libatomic1-arm64-cross libasan6-arm64-cross
    liblsan0-arm64-cross libtsan0-arm64-cross libubsan1-arm64-cross
    libhwasan0-arm64-cross gcc-11-cross-base gcc-12-cross-base
  )
  (cd "${work_dir}/packages" && apt download "${packages[@]}")
  for package_file in "${work_dir}"/packages/*.deb; do
    dpkg-deb -x "${package_file}" "${work_dir}/root"
  done
  mv -- "${work_dir}/root" "${toolchain_dir}/root"
fi

install -m 755 "${project_dir}/scripts/cross/toolchain-wrapper.sh" "${toolchain_dir}/bin/toolchain-wrapper.sh"
for tool in gcc g++ ar strip; do
  ln -sfn toolchain-wrapper.sh "${toolchain_dir}/bin/aarch64-linux-gnu-${tool}"
done
printf 'ARM64 toolchain: %s\n' "${toolchain_dir}/bin"
