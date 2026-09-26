#!/usr/bin/env bash
# Launcher for the user-local Ubuntu 22.04 ARM64 cross toolchain.
set -euo pipefail
toolchain_dir="$(cd -- "$(dirname -- "$0")/.." && pwd)"
root="${toolchain_dir}/root"
export GCC_EXEC_PREFIX="${root}/usr/lib/gcc-cross/"
export LD_LIBRARY_PATH="${root}/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

case "$(basename -- "$0")" in
  aarch64-linux-gnu-gcc)
    exec "${root}/usr/bin/aarch64-linux-gnu-gcc-11" --sysroot="${root}" -B"${root}/usr/bin/" "$@" ;;
  aarch64-linux-gnu-g++)
    exec "${root}/usr/bin/aarch64-linux-gnu-g++-11" --sysroot="${root}" -B"${root}/usr/bin/" "$@" ;;
  aarch64-linux-gnu-ar)
    exec "${root}/usr/bin/aarch64-linux-gnu-ar" "$@" ;;
  aarch64-linux-gnu-strip)
    exec "${root}/usr/bin/aarch64-linux-gnu-strip" "$@" ;;
  *)
    printf 'Unknown cross-tool invocation: %s\n' "$0" >&2
    exit 2 ;;
esac
