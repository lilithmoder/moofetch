#!/usr/bin/env sh

set -e

mkdir -p build/
cd build/

cmake ..

kernel_name="$(uname -s)"

case "${kernel_name}" in
    "Linux" | "MINGW"*)
        cmake_build_args="-j$(nproc)"
        ;;
    "Darwin" | *"BSD" | "DragonFly")
        cmake_build_args="-j$(sysctl -n hw.ncpu)"
        ;;
    *)
        cmake_build_args=""
        ;;
esac

cmake --build . --target moofetch "${cmake_build_args}"

./moofetch "$@"
