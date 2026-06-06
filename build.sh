#!/bin/bash

# Copyright (c) 2026 Alex313031

set -euo pipefail

YEL='\033[1;33m' # Yellow
CYA='\033[1;96m' # Cyan
RED='\033[1;31m' # Red
GRE='\033[1;32m' # Green
c0='\033[0;00m'  # Reset Text
bold='\033[1;37m' # Bold Text
underline='\033[4m' # Underline Text

# Error handling
yell() { echo -e "$0: $*" >&2; }
die()  { yell "${RED}$* ${c0}"; exit 1; }
try() { "$@" || die "${RED}Failed $*"; }

SCRIPTNAME=$(basename "$0")
SCRIPTVER="2.1.2"

export HERE=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# gen.py and the out/ paths are relative, so run from the repo root regardless
# of where the script was invoked from.
cd "$HERE" || die "Failed to cd into $HERE"

JOB_COUNT=$(getconf _NPROCESSORS_ONLN)

WANT_DEBUG=0
WANT_TARGET=""
VFLAG=""

show_help() {
  cat <<EOF
Usage:
  $SCRIPTNAME [options]

A script to build GN on Linux for Linux or Windows.

Options:
  -h, --help    Show this help.
  --version     Show script version.
  -c, --clean   Remove build artifacts
  --deps        Install build dependencies
  -l, --linux   Build GN for Linux
  -w, --win     Build GN for Windows
  -d, --debug   Make a debug build
  -v, --verbose Verbose build output

EOF

  exit 0
}

show_version() {
  printf "\n %s Version %s \n\n" "$SCRIPTNAME" "$SCRIPTVER"
  exit 0
}

install_deps() {
  if ! command -v apt-get >/dev/null; then
    die "--deps only supports apt-based systems (Ubuntu/Debian); install the prerequisites manually"
  fi
  # use sudo only when not already root (e.g. plain CI containers lack sudo)
  local sudo=""
  if [ "$(id -u)" -ne 0 ]; then
    sudo="sudo"
  fi

  printf "${GRE}Installing dependencies for %s...${c0}\n" "$SCRIPTNAME"
  # build-essential: gcc, g++, make for the Linux build. mingw-w64 provides the
  # x86_64-w64-mingw32-* cross toolchain used by the Windows build.
  $sudo apt-get update || die "apt-get update failed"
  $sudo apt-get install build-essential ninja-build python3 zip \
        mingw-w64 mingw-w64-i686-dev mingw-w64-x86-64-dev mingw-w64-tools \
      || die "Failed to install dependencies"
  printf "${GRE}Done installing dependencies!${c0}\n"
}

build_linux() {
  # Build using system GCC, not clang
  export CC=gcc
  export CXX=g++
  export AR=ar
  export LD=g++
  if [ "$WANT_DEBUG" == "1" ]; then
    printf "${GRE}Building GN for Linux using GCC (Debug)...${c0}\n"
    try python3 build/gen.py --out-path out/linux_debug --host=linux --platform=linux --debug
    try ninja -C out/linux_debug $VFLAG -j"$JOB_COUNT"
    try cd out/linux_debug
    try mv -fv gn gn_debug
    printf "${GRE}Zipping up build.${c0}\n"
    try zip "gn_linux_debug.zip" gn_debug
    try mv -fv gn_linux_debug.zip ../../
    printf "${GRE}Done!${c0}\n"
  else
    printf "${GRE}Building GN for Linux using GCC...${c0}\n"
    try python3 build/gen.py --out-path out/linux --host=linux --platform=linux
    try ninja -C out/linux $VFLAG -j"$JOB_COUNT"
    try cd out/linux
    printf "${GRE}Zipping up build.${c0}\n"
    try zip "gn_linux.zip" gn
    try mv -fv gn_linux.zip ../../
    printf "${GRE}Done!${c0}\n"
  fi
}

build_windows() {
  # Cross-compile for Windows using the mingw-w64 toolchain (installed via --deps).
  # gen.py defaults the mingw toolchain to plain g++/ar, which is the host
  # compiler, so we must point it at the cross compiler explicitly.
  export CC=x86_64-w64-mingw32-gcc
  export CXX=x86_64-w64-mingw32-g++
  export AR=x86_64-w64-mingw32-ar
  export LD=x86_64-w64-mingw32-g++
  if [ "$WANT_DEBUG" == "1" ]; then
    printf "${GRE}Building GN for Windows using MinGW (Debug)...${c0}\n"
    try python3 build/gen.py --out-path out/win_debug --host=linux --platform=mingw --debug
    try ninja -C out/win_debug $VFLAG -j"$JOB_COUNT"
    try cd out/win_debug
    try mv -fv gn.exe gn_debug.exe
    printf "${GRE}Zipping up build.${c0}\n"
    try zip "gn_win_debug.zip" gn_debug.exe
    try mv -fv gn_win_debug.zip ../../
    printf "${GRE}Done!${c0}\n"
  else
    printf "${GRE}Building GN for Windows using MinGW...${c0}\n"
    # --use-lto --use-icf can only be used with MSVC/Clang
    try python3 build/gen.py --out-path out/win --host=linux --platform=mingw
    try ninja -C out/win $VFLAG -j"$JOB_COUNT"
    try cd out/win
    printf "${GRE}Zipping up build.${c0}\n"
    try zip "gn_win.zip" gn.exe
    try mv -fv gn_win.zip ../../
    printf "${GRE}Done!${c0}\n"
  fi
}

clean_out() {
  printf "${YEL}Cleaning ${HERE}/out/...${c0}\n"
  rm -rfv "${HERE}"/out/*
  printf "${YEL}Cleaning .zips...${c0}\n"
  rm -rfv "${HERE}"/gn_*.zip
}

while :; do
  case ${1-} in
    -h|--help)
        show_help
        ;;
    --version)
        show_version
        ;;
    --deps)
        install_deps
        exit 0
        ;;
    -c|--clean)
        clean_out
        exit 0
        ;;
    -d|--debug)
        WANT_DEBUG=1
        ;;
    -v|--verbose)
        VFLAG="-v"
        ;;
    -l|--linux)
        [ -n "$WANT_TARGET" ] && [ "$WANT_TARGET" != "linux" ] && die "Cannot specify both linux and win"
        WANT_TARGET="linux"
        ;;
    -w|--win)
        [ -n "$WANT_TARGET" ] && [ "$WANT_TARGET" != "windows" ] && die "Cannot specify both linux and win"
        WANT_TARGET="windows"
        ;;
    --)
        shift
        break
        ;;
    -?*)
        die "Unknown option '$1'"
        ;;
    *)
        break
  esac
  shift
done

case "$WANT_TARGET" in
  linux)
      build_linux || die "Linux build failed"
      ;;
  windows)
      build_windows || die "Windows build failed"
      ;;
  *)
      yell "${YEL}No build target specified (use -l/--linux or -w/--win).${c0}"
      show_help
      ;;
esac

exit 0
