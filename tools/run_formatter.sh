#!/bin/bash -eu

# Reformat the C++ and Rust sources.

# Assume this is under tools/.
cd "$(dirname "$(dirname "$0")")"

if [ "${1:-}" = "--diff" ]; then
  clang_format_opts=(--dry-run -Werror)
  fmt_opts=(--check)
else
  clang_format_opts=(-i)
  fmt_opts=()
fi

if [ -z "${CLANG_FORMAT:-}" ]; then
  if command -v cipd > /dev/null; then
    ensure_file=$(mktemp)
    # https://chrome-infra-packages.appspot.com/p/fuchsia/third_party/clang
    # shellcheck disable=SC2016
    echo 'fuchsia/third_party/clang/${platform} integration' > "$ensure_file"
    trap 'rm "$ensure_file"' EXIT
    cipd ensure -ensure-file "$ensure_file" -root clang
    CLANG_FORMAT="./clang/bin/clang-format"
  elif command -v clang-format > /dev/null; then
    # cipd ships with depot_tools, which a plain checkout does not have. Fall
    # back to the clang-format on PATH; it is likely a different version than
    # the pinned package above, so it can disagree on a few lines. Set
    # CLANG_FORMAT, or put depot_tools on PATH, to use a specific one.
    CLANG_FORMAT=$(command -v clang-format)
    echo "run_formatter.sh: cipd not found, using $CLANG_FORMAT" >&2
  else
    echo "run_formatter.sh: no clang-format found. Install one and set" \
         "CLANG_FORMAT=<path>, or put depot_tools (for cipd) on PATH." >&2
    exit 1
  fi
fi

git ls-files | grep -E '\.(h|cc)$' | xargs "$CLANG_FORMAT" "${clang_format_opts[@]}"
