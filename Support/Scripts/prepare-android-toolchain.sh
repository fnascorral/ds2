#!/usr/bin/env bash
##
## Copyright (c) 2014-present, Facebook, Inc.
## All rights reserved.
##
## This source code is licensed under the University of Illinois/NCSA Open
## Source License found in the LICENSE file in the root directory of this
## source tree. An additional grant of patent rights can be found in the
## PATENTS file in the same directory.
##

source "$(dirname "$0")/common.sh"

[ $# -le 2 ] || die "usage: $0 [ARCH] [PLATFORM]"

case "$(uname)" in
  "Linux")  host="linux-x86";;
  "Darwin") host="darwin-x86";;
  *)        die "This script works only on Linux and Mac OS X.";;
esac

if [ $# -eq 0 ]; then
  for arch in aarch64 arm x86; do
    "$0" "$arch"
  done
  exit 0
fi

case "${1}" in
  "aarch64")  toolchain_arch="aarch64"; toolchain_triple="aarch64-linux-android";;
  "arm")      toolchain_arch="arm";     toolchain_triple="arm-linux-androideabi";;
  "x86")      toolchain_arch="x86_64";  toolchain_triple="x86_64-linux-android";;
  *)          die "Unknown architecture '${1}'." ;;
esac

if [ $# -lt 2 ]; then
  aosp_platform="android-21"
else
  if [ "${2}" -lt 16 ]; then
    die "Minimum supported android platform is 16."
  fi
  if [ "${2}" -lt 21 ] && [ "${1}" != "arm" ]; then
    die "Minimum supported android platform for \`${1}' target is 21."
  fi
  aosp_platform="android-${2}"
fi

toolchain_version="4.9"
toolchain_path="/tmp/aosp-toolchain/${toolchain_triple}-${toolchain_version}"
aosp_ndk_path="/tmp/aosp-toolchain/aosp-ndk"

aosp_prebuilt_clone() {
  git_clone "https://android.googlesource.com/platform/prebuilts/$1" "$2"
}

clobber_dir() {
  if [ -e "$2" ]; then
    rm -rf "$2"
  fi

  mkdir -p "$(dirname "$2")"
  cp -R "$1" "$2"
}

mkdir -p "$(dirname "${toolchain_path}")"

aosp_prebuilt_clone "gcc/${host}/$1/${toolchain_triple}-${toolchain_version}" "${toolchain_path}"
aosp_prebuilt_clone "ndk" "${aosp_ndk_path}"

# Copy sysroot.
case "$1" in
  "aarch64")
    # For aarch64, the sysroot is in `arch-arm64`, not `arch-aarch64`.
    clobber_dir "${aosp_ndk_path}/current/platforms/${aosp_platform}/arch-arm64" "${toolchain_path}/sysroot"
    ;;
  "arm")
    clobber_dir "${aosp_ndk_path}/current/platforms/${aosp_platform}/arch-${toolchain_arch}" "${toolchain_path}/sysroot"
    ;;
  "x86")
    # For x86_64, libraries are in `usr/lib64/`; we need to copy 32-bit versions of these libraries to `usr/lib/`.
    clobber_dir "${aosp_ndk_path}/current/platforms/${aosp_platform}/arch-${toolchain_arch}" "${toolchain_path}/sysroot"
    clobber_dir "${aosp_ndk_path}/current/platforms/${aosp_platform}/arch-x86/usr/lib" "${toolchain_path}/sysroot/usr/lib"
    ;;
esac

# gcc version isn't guaranteed to match toolchain version - for example, 4.9 vs 4.9.x
gcc_version="$( "${toolchain_path}/bin/${toolchain_triple}-gcc" --version | awk 'NR==1{print $3}' )"

# Copy C++ stuff
clobber_dir "${aosp_ndk_path}/current/sources/cxx-stl/gnu-libstdc++/${toolchain_version}/include" "${toolchain_path}/include/c++/${gcc_version}"
copy_cpp_binaries() {
  clobber_dir "${aosp_ndk_path}/current/sources/cxx-stl/gnu-libstdc++/${toolchain_version}/libs/$1/libgnustl_static.a" "${toolchain_path}/${toolchain_triple}/$2/libstdc++.a"
  clobber_dir "${aosp_ndk_path}/current/sources/cxx-stl/gnu-libstdc++/${toolchain_version}/libs/$1/libgnustl_shared.so" "${toolchain_path}/${toolchain_triple}/$2/libgnustl_shared.so"
  clobber_dir "${aosp_ndk_path}/current/sources/cxx-stl/gnu-libstdc++/${toolchain_version}/libs/$1/libsupc++.a" "${toolchain_path}/${toolchain_triple}/$2/libsupc++.a"
}
copy_cpp_bits() {
  clobber_dir "${aosp_ndk_path}/current/sources/cxx-stl/gnu-libstdc++/${toolchain_version}/libs/$1/include/bits" "${toolchain_path}/include/c++/${gcc_version}/${toolchain_triple}/$2"
}
case "$1" in
  "aarch64")
    copy_cpp_binaries "arm64-v8a" "lib"
    copy_cpp_bits "arm64-v8a" "bits"
    ;;
  "arm")
    copy_cpp_binaries "armeabi" "lib"
    copy_cpp_bits "armeabi" "bits"
    ;;
  "x86")
    copy_cpp_binaries "x86_64" "lib64"; copy_cpp_binaries "x86" "lib"
    copy_cpp_bits "x86_64" "bits";      copy_cpp_bits "x86" "32/bits"
    ;;
esac
