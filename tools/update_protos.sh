#!/bin/bash -eu

ROOT_DIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")/..")"
OUT_DIR="${ROOT_DIR}/src/gn/proto"

cd "${ROOT_DIR}/third_party/protobuf"

# Explicitly allow empty glbs because it glbs for *_test.cc, and we don't bother copying those files.
bazel run //src/google/protobuf/compiler:protoc \
    --noincompatible_disallow_empty_glob -- \
    -I="${OUT_DIR}" --cpp_out="${OUT_DIR}" "${OUT_DIR}"/*.proto