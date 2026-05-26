#!/usr/bin/env bash
# build.sh — configure, build, and test the TCM project
# Usage: ./build.sh [--release] [--asan] [--static]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

BUILD_TYPE="Debug"
ASAN=OFF
STATIC=OFF

for arg in "$@"; do
    case "$arg" in
        --release) BUILD_TYPE="Release" ;;
        --asan)    ASAN=ON ;;
        --static)  STATIC=ON ;;
        -asan)     ASAN=ON ;;  # backward compat
        *) echo "Unknown option: $arg"; exit 1 ;;
    esac
done

echo "=============================="
echo " TCM Build Script"
echo " Project     : ${PROJECT_ROOT}"
echo " Build dir   : ${BUILD_DIR}"
echo " Build type  : ${BUILD_TYPE}"
echo " ASan        : ${ASAN}"
echo " Static      : ${STATIC}"
echo "=============================="

# ---------- Configure ----------
cmake \
    -B "${BUILD_DIR}" \
    -S "${PROJECT_ROOT}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DTCM_ENABLE_ASAN="${ASAN}" \
    -DTCM_STATIC="${STATIC}"

# ---------- Build ----------
cmake --build "${BUILD_DIR}" -j"$(nproc)"

# ---------- Test ----------
echo ""
echo "Running tests..."
cd "${BUILD_DIR}"
ctest --output-on-failure --parallel "$(nproc)"
TEST_RESULT=$?

if [ "${TEST_RESULT}" -ne 0 ]; then
    echo "ERROR: One or more tests failed (exit code ${TEST_RESULT})"
    exit "${TEST_RESULT}"
fi

echo ""
echo "All tests passed."
echo "Build complete: ${BUILD_DIR}/tcm"
