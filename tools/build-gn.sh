#!/bin/bash

# Helper script to rebuild GN, supports using Fuchsia prebuilt toolchain + sysroot
# as well as linking against RPMalloc for better performance. See --help.

set -e

die () {
  echo >&2 "ERROR: $@"
  exit 1
}


BINPREFIX=
HELP=
TARGET_FLAGS=
SYSROOT_FLAGS=
DEBUG=
GCC=
LTO=
ASAN=
CCACHE=$(which ccache 2>/dev/null)
BUILD_RPMALLOC=
LINK_RPMALLOC=
NINJA=ninja
INSTALL_TO=
WINDOWS=
# Set the BINPREFIX variable depending on the value of
# --binprefix and --gcc.
#
# $1: binprefix value, can be a directory, or a file prefix.
# $2: true if GCC must be used.
set_binprefix () {
  local prefix="${1}"
  local is_gcc="${2}"
  if [[ ! -d "${prefix}" ]]; then
    # Not a directory, assume it is a toolchain prefix
    # e.g. `/path/to/toolchain/bin/x64_64-gnu-`.
    BINPREFIX="${prefix}"
  else
    # If a directory, append trailing slash to binprefix.
    BINPREFIX="${prefix%/}/"
  fi
}

for OPT; do
  case "${OPT}" in
    --help)
      HELP=true
      ;;
    --binprefix=*)
      set_binprefix "${OPT#--binprefix=}" "${GCC}"
      ;;
    --sysroot=*)
      SYSROOT_FLAGS="--sysroot=${OPT#--sysroot=}"
      ;;
    --ninja-*)
      NINJA="${OPT#--ninja=}"
      ;;
    --windows-env=*)
      WINDOWS_ENV="${OPT#--windows-env=}"
      ;;
    --gcc)
      if [[ -n "$BINPREFIX" ]]; then
        die "Option --gcc must appear before --binprefix or --fuchsia-dir!"
      fi
      GCC=true
      ;;
    --fuchsia-dir=*)
      FUCHSIA_DIR="${OPT#--fuchsia-dir=}"
      if [[ -n "${GCC}" ]]; then
        set_binprefix "${FUCHSIA_DIR}/prebuilt/third_party/gcc/linux-x64/bin/x86_64-elf-"
      else
        set_binprefix "${FUCHSIA_DIR}/prebuilt/third_party/clang/linux-x64/bin"
      fi
      SYSROOT_FLAGS="--sysroot=${FUCHSIA_DIR}/prebuilt/third_party/sysroot/linux"
      NINJA="${FUCHSIA_DIR}/prebuilt/third_party/ninja/linux-x64/ninja"
      ;;
    --ninja=*)
      NINJA="${OPT#--ninja=}"
      ;;
    --target=*)
      TARGET_FLAGS="${OPT}"
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
    --asan)
      ASAN=true
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
  --binprefix=DIR        Specify directory with Clang (or GCC) toolchain binaries.
  --gcc                  Use GCC instead of Clang
  --sysroot=DIR          Specify sysroot directory.
  --ninja=BINARY         Specify ninja binary (default is 'ninja').
  --fuchsia-dir=DIR      Specify Fuchsia directory where to find Clang, ninja and sysroot prebuilts.
  --target=ARCH          Specify clang target triple.
  --debug                Build debug version of the binary.
  --lto                  Use LTO and ICF for smaller binary.
  --asan                 Use Address Sanitizer.
  --no-ccache            Disable ccache usage.
  --build-rpmalloc       Build and link rpmalloc library.
  --link-rpmalloc        Link pre-built rpmalloc library (requires a previous
                         --build-rpmalloc invocation to build it).
  --install-to=PATH      Copy generated binary to PATH on success.
  --windows-env=FILE     Path to an envsetup.sh script that sets up the environment
                         for a Windows cross-build with clang-cl. This shall
                         set CC, CXX, AR, LINK and LIBS appropriately. This will
                         be sourced directly!
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

if [[ -n "${WINDOWS_ENV}" ]]; then
  if [[ ! -f "${WINDOWS_ENV}" ]]; then
    die "Missing file: ${WINDOWS_ENV}"
  fi
  source "${WINDOWS_ENV}"
  GN_BUILD_GEN_ARGS="${GN_BUILD_GEN_ARGS} --platform msvc --no-last-commit-position"
elif [[ -n "${GCC}" ]]; then
  CC="${BINPRPEFIX}gcc"
  CXX="${BINPREFIX}g++"
  AR="${BINPREFIX}ar"
else
  CC="${BINPRPEFIX}clang"
  CXX="${BINPREFIX}clang++"
  AR="${BINPREFIX}llvm-ar"
fi
if [[ ! -f "${AR}" ]]; then
  unset AR
fi
CFLAGS="${TARGET_FLAGS} ${SYSROOT_FLAGS}"
LDFLAGS="${TARGET_FLAGS} ${SYSROOT_FLAGS} -static-libstdc++"

if [[ -n "$ASAN" ]]; then
  CFLAGS="${CFLAGS} -fsanitize=address"
  LDFLAGS="${LDFLAGS} -fsanitize=address"
fi

if [[ -n "${CCACHE}" ]]; then
  echo "Using ccache program: ${CCACHE}"
  CC="${CCACHE} $CC"
  CXX="${CCACHE} $CXX"
fi

export CC CXX AR CFLAGS CXXFLAGS LDFLAGS

if [[ -z "${XDG_CACHE_HOME}" ]]; then
  XDG_CACHE_HOME="${HOME}/.cache"
fi

if [[ -n "$LINK_RPMALLOC" ]]; then
  RPMALLOC_LIB=${XDG_CACHE_HOME}/gn-build/librpmallocwrap.a

  if [[ -n "$BUILD_RPMALLOC" ]]; then
    echo "Rebuilding rpmalloc library from scratch"
    RPMALLOC_GIT_URL="https://fuchsia.googlesource.com/third_party/github.com/mjansson/rpmalloc"
    RPMALLOC_REVISION="4c107231abe627886a2429ea29f45a2a767a9e2d"
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
if [[ -n "${WINDOWS_ENV}" ]]; then
  wine out/gn_unittests.exe
else
  out/gn_unittests
fi

if [[ -n "${INSTALL_TO}" ]]; then
  mkdir -p "$(dirname "${INSTALL_TO}")"
  cp out/gn "${INSTALL_TO}"
fi

