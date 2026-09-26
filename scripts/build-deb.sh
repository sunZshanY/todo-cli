#!/bin/sh
set -eu

for tool in cmake ctest cpack dpkg-deb dpkg-shlibdeps file; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Missing build tool: %s. See the Debian dependencies in README.md.\n' "$tool" >&2
        exit 1
    fi
done

source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${1:-"$source_dir/build-debian"}

cmake -S "$source_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DTODO_USE_SYSTEM_SQLITE=ON \
    -DTODO_BUILD_TESTS=ON
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure
cpack --config "$build_dir/CPackConfig.cmake" -G DEB
