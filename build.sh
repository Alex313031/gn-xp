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
SCRIPTVER="2.1.6"

export HERE=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# gen.py and the out/ paths are relative, so run from the repo root regardless
# of where the script was invoked from.
cd "$HERE" || die "Failed to cd into $HERE"

JOB_COUNT=$(getconf _NPROCESSORS_ONLN)

WANT_DEBUG=0
WANT_I386=0
WANT_ALL=0
WANT_TARGET=""
VFLAG=""

show_help() {
  cat <<EOF
Usage:
  $SCRIPTNAME [options]

A script to build GN on Linux for Linux or Windows.

Options:
  -h, --help    Show this help
  --version     Show script version
  -c, --clean   Remove build artifacts
  --deps        Install build dependencies
  --i386        Make a 32 bit build (i386 on Linux, win32 on Windows)
  -l, --linux   Build GN for Linux
  -w, --win     Build GN for Windows
  --all         Build all 4 combos: Linux x64/x86 and Windows x64/x86
  -d, --debug   Make a debug build
  -v, --verbose Verbose build output

EOF

  exit 0
}

show_version() {
  printf "\n ${bold} %s Version %s \n\n" "$SCRIPTNAME" "$SCRIPTVER"
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
  # build-essential: gcc, g++, make for the Linux build; g++-multilib adds the
  # 32-bit libs needed for --i386 Linux builds. mingw-w64 provides the
  # *-w64-mingw32-* cross toolchains used by the Windows builds.
  $sudo apt-get update || die "apt-get update failed"
  $sudo apt-get install build-essential g++-multilib ninja-build python3 zip \
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
  # 32- vs 64-bit Linux: gen.py reads CFLAGS/CXXFLAGS (compile) and LDFLAGS
  # (link), so the arch flag covers the whole gn build. gn is only built here,
  # never run, so a 64-bit host just needs g++-multilib's 32-bit libs.
  local arch="" mflag="-m64" archlabel="x64"
  if [ "$WANT_I386" == "1" ]; then
    arch="_i386"
    mflag="-m32"
    archlabel="x86"
  fi
  export CFLAGS="$mflag" CXXFLAGS="$mflag" LDFLAGS="$mflag"
  local zipname="gn_linux${arch}" outdir="out/linux${arch}"
  if [ "$WANT_DEBUG" == "1" ]; then
    zipname="gn_linux${arch}_debug"
    outdir="out/linux${arch}_debug"
    printf "${GRE}Building GN for Linux ${archlabel} using GCC (Debug)...${c0}\n"
    try python3 build/gen.py --out-path "$outdir" --host=linux --platform=linux --debug
    try ninja -C "$outdir" $VFLAG -j"$JOB_COUNT"
    try cd "$outdir"
    try mv -fv gn gn_debug
    printf "${GRE}Zipping up gn_debug...${c0}\n"
    try zip "$zipname.zip" gn_debug
    try mv -fv "$zipname.zip" ../../
  else
    printf "${GRE}Building GN for Linux ${archlabel} using GCC...${c0}\n"
    try python3 build/gen.py --out-path "$outdir" --host=linux --platform=linux
    try ninja -C "$outdir" $VFLAG -j"$JOB_COUNT"
    try cd "$outdir"
    printf "${GRE}Zipping up gn...${c0}\n"
    try zip "$zipname.zip" gn
    try mv -fv "$zipname.zip" ../../
  fi
  printf "${GRE}Done! Zip at ${CYA}${zipname}.zip ${c0}\n"
}

build_windows() {
  local arch="" archlabel="" zipname="" outdir=""
  # Cross-compile for Windows using the mingw-w64 toolchain (installed via --deps).
  # gen.py defaults the mingw toolchain to plain g++/ar, which is the host
  # compiler, so we must point it at the cross compiler explicitly.
  if [ "$WANT_I386" == "1" ]; then
    export CC=i686-w64-mingw32-gcc
    export CXX=i686-w64-mingw32-g++
    export AR=i686-w64-mingw32-ar
    export LD=i686-w64-mingw32-g++
    export WINDRES=i686-w64-mingw32-windres
    arch="win32"
    archlabel="x86"
  else
    export CC=x86_64-w64-mingw32-gcc
    export CXX=x86_64-w64-mingw32-g++
    export AR=x86_64-w64-mingw32-ar
    export LD=x86_64-w64-mingw32-g++
    export WINDRES=x86_64-w64-mingw32-windres
    arch="win64"
    archlabel="x64"
  fi
  if [ "$WANT_DEBUG" == "1" ]; then
    zipname="gn_${arch}_debug"
    outdir="out/${arch}_debug"
    printf "${GRE}Building GN for Windows ${archlabel} using MinGW (Debug)...${c0}\n"
    try python3 build/gen.py --out-path "$outdir" --host=linux --platform=mingw --debug
    try ninja -C "$outdir" $VFLAG -j"$JOB_COUNT"
    try cd "$outdir"
    try mv -fv gn.exe gn_debug.exe
    printf "${GRE}Zipping up gn_debug.exe...${c0}\n"
    try zip "$zipname.zip" gn_debug.exe
    try mv -fv "$zipname.zip" ../../
  else
    zipname="gn_${arch}"
    outdir="out/${arch}"
    printf "${GRE}Building GN for Windows ${archlabel} using MinGW...${c0}\n"
    # --use-lto --use-icf can only be used with MSVC/Clang
    try python3 build/gen.py --out-path "$outdir" --host=linux --platform=mingw
    try ninja -C "$outdir" $VFLAG -j"$JOB_COUNT"
    try cd "$outdir"
    printf "${GRE}Zipping up gn.exe...${c0}\n"
    try zip "$zipname.zip" gn.exe
    try mv -fv "$zipname.zip" ../../
  fi
  printf "${GRE}Done! Zip at ${CYA}${zipname}.zip ${c0}\n"
}

# Build every OS/arch combo in one shot: Linux x64/x86 and Windows x64/x86.
# Each build runs in a subshell so its per-build env exports (CFLAGS/-m*, the
# cross CC/CXX, ...) and the functions' cd into out/ cannot leak into the next
# build. WANT_DEBUG/VFLAG are inherited, so --all --debug makes 4 debug zips.
build_all() {
  ( WANT_I386=0; build_linux )   || die "Linux x64 build failed"
  ( WANT_I386=1; build_linux )   || die "Linux x86 build failed"
  ( WANT_I386=0; build_windows ) || die "Windows x64 build failed"
  ( WANT_I386=1; build_windows ) || die "Windows x86 build failed"
  printf "${GRE}All 4 builds complete!${c0}\n"
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
    --i386)
        WANT_I386=1
        ;;
    --all)
        WANT_ALL=1
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

# --all overrides individual target/arch selection and builds every combo.
if [ "$WANT_ALL" == "1" ]; then
  build_all
  exit 0
fi

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
