#!/bin/bash

# Helper script to rebuild GN, supports using Fuchsia prebuilt toolchain + sysroot
# as well as linking against RPMalloc for better performance. See --help.

set -e

die () {
  echo >&2 "ERROR: $@"
  exit 1
}


CLANG_BINPREFIX=
HELP=
TARGET_FLAGS=
TARGET=
SYSROOT_FLAGS=
DEBUG=
LTO=
CCACHE=$(which ccache 2>/dev/null)
BUILD_RPMALLOC=
LINK_RPMALLOC=
NINJA=ninja
INSTALL_TO=

for OPT; do
  case "${OPT}" in
    --help)
      HELP=true
      ;;
    --clang-binprefix=*)
      CLANG_BINPREFIX="${OPT#--clang-binprefix=}"
      ;;
    --sysroot=*)
      SYSROOT_FLAGS="--sysroot=${OPT#--sysroot=}"
      ;;
    --ninja-*)
      NINJA=${OPT#--ninja=}
      ;;
    --fuchsia-dir=*)
      FUCHSIA_DIR="${OPT#--fuchsia-dir=}"
      CLANG_BINPREFIX="${FUCHSIA_DIR}/prebuilt/third_party/clang/linux-x64/bin"
      SYSROOT_FLAGS="--sysroot=${FUCHSIA_DIR}/prebuilt/third_party/sysroot/linux"
      NINJA="${FUCHSIA_DIR}/prebuilt/third_party/ninja/linux-x64/ninja"
      ;;
    --target=*)
      TARGET_FLAGS="${OPT}"
      TARGET="{OPT#--target=}"
      ;;
    --no-ccache)
      CCACHE=
      ;;
    --build-rpmalloc)
      BUILD_RPMALLOC=true
      LINK_RPMALLOC=true
      ;;
    --link-rpmalloc)
      LINK_RPMALLOC=true
      ;;
    --debug)
      DEBUG=true
      ;;
    --lto)
      LTO=true
      ;;
    --install-to=*)
      INSTALL_TO="${OPT#--install-to=}"
      ;;
    -*)
      die "Unknown option $OPT, see --help."
      ;;
    *)
      die "This script does not take parameters [$OPT]. See --help."
      ;;
  esac
done

if [[ -n "$HELP" ]]; then
  PROGNAME="$(basename $0)"
  cat <<EOF
Usage: ${PROGNAME} [options]

Rebuild the GN binary, with ccache and rpmalloc when available.
See tools/build-rpmalloc.sh to rebuild the rpmalloc library from
source.

This script must be invoked from the GN source directory, but
can be installed anywhere. If you have a Fuchsia workspace, using
the --fuchsia-dir=DIR option is recommended.

Valid options:

  --help                 Print this message.
  --clang-binprefix=DIR  Specify directory with Clang toolchain binaries.
  --sysroot=DIR          Specify sysroot directory.
  --ninja=BINARY         Specify ninja binary (default is 'ninja').
  --fuchsia-dir=DIR      Specify Fuchsia directory where to find Clang, ninja and sysroot prebuilts.
  --target=ARCH          Specify clang target triple.
  --debug                Build debug version of the binary.
  --lto                  Use LTO and ICF for smaller binary.
  --no-ccache            Disable ccache usage.
  --build-rpmalloc       Build and link rpmalloc library.
  --link-rpmalloc        Link pre-built rpmalloc library (requires a previous
                         --build-rpmalloc invocation to build it).
  --install-to=PATH      Copy generated binary to PATH on success.
EOF
  exit 0
fi

if [[ -n "${DEBUG}" && -n "${LTO}" ]]; then
  die "Only use one of --debug or --lto!"
fi

GN_BUILD_GEN_ARGS="--no-strip"
if [[ -n "$DEBUG" ]]; then
  echo "Generating debug version of the binary"
  GN_BUILD_GEN_ARGS="${GN_BUILD_GEN_ARGS} --debug"
elif [[ -n "$LTO" ]]; then
  echo "Using link-time optimization and identical-code-folding"
  GN_BUILD_GEN_ARGS="${GN_BUILD_GEN_ARGS} --use-icf --use-lto"
fi

if [[ -n "${CLANG_BINPREFIX}" ]]; then
  CLANG_BINPREFIX="${CLANG_BINPREFIX}/"
fi

CC="${CLANG_BINPRPEFIX}clang"
CXX="${CLANG_BINPREFIX}clang++"
AR="${CLANG_BINPREFIX}llvm-ar"
if [[ ! -f "${AR}" ]]; then
  unset AR
fi
CFLAGS="${TARGET_FLAGS} ${SYSROOT_FLAGS}"
LDFLAGS="${TARGET_FLAGS} ${SYSROOT_FLAGS} -static-libstdc++"

if [[ -n "${CCACHE}" ]]; then
  echo "Using ccache program: ${CCACHE}"
  CC="${CCACHE} $CC"
  CXX="${CCACHE} $CXX"
fi

export CC CXX AR CFLAGS CXXFLAGS

if [[ -z "${XDG_CACHE_HOME}" ]]; then
  XDG_CACHE_HOME="${HOME}/.cache"
fi

if [[ -n "$LINK_RPMALLOC" ]]; then
  RPMALLOC_LIB=${XDG_CACHE_HOME}/gn-build/librpmallocwrap.a

  if [[ -n "$BUILD_RPMALLOC" ]]; then
    echo "Rebuilding rpmalloc library from scratch"
    RPMALLOC_GIT_URL="https://fuchsia.googlesource.com/third_party/github.com/mjansson/rpmalloc"
    RPMALLOC_REVISION="6bb6ca97a8d6a72d626153fd8431ef8477a21145"
    RPMALLOC_ARCH=x86-64
    RPMALLOC_OS=linux
    RPMALLOC_LIBPATH=lib/$RPMALLOC_OS/release/$RPMALLOC_ARCH/librpmallocwrap.a
    TMPDIR=$(mktemp -d /tmp/build-rpmalloc.XXXXX)
    (
      cd $TMPDIR
      git clone $RPMALLOC_GIT_URL .
      git checkout $RPMALLOC_REVISION
      ./configure.py -c release -a $RPMALLOC_ARCH --lto
      "${NINJA}" "${RPMALLOC_LIBPATH}"
    )

    mkdir -p "$(dirname "${RPMALLOC_LIB}")"
    cp -f "${TMPDIR}/${RPMALLOC_LIBPATH}" "${RPMALLOC_LIB}"
  else
    if [[ ! -f "${RPMALLOC_LIB}" ]]; then
      die "rpmalloc library is missing, use --build-rpmalloc instead: ${RPMALLOC_LIB}"
    fi
  fi

  echo "Using rpmalloc library: $RPMALLOC_LIB"
  GN_BUILD_GEN_ARGS="$GN_BUILD_GEN_ARGS --link-lib=\"$RPMALLOC_LIB\""
fi

build/gen.py $GN_BUILD_GEN_ARGS
"${NINJA}" -C out
out/gn_unittests
if [[ -n "${INSTALL_TO}" ]]; then
  mkdir -p "$(dirname "${INSTALL_TO}")"
  cp out/gn "${INSTALL_TO}"
fi

