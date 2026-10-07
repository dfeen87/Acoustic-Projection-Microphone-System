#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
build_dir=${1:-"$repo/build"}
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
cmake --install "$build_dir" --prefix "$scratch/install"
mkdir "$scratch/consumer"
cat > "$scratch/consumer/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.18)
project(APMConsumer LANGUAGES CXX)
find_package(APM 11 REQUIRED CONFIG)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE APM::apm_core)
CMAKE
cat > "$scratch/consumer/main.cpp" <<'CPP'
#include <apm/apm_core.hpp>
#include <apm/apm_system.h>
#include <apm/config.h>
#include <string>
int main() {
    apm::APMCore core;
    if (core.get_version() != std::string(APM_VERSION)) return 1;
    if (!core.initialize(16000, 1)) return 2;
    if (core.process({0.1f, 0.0f}).size() != 2) return 3;
    apm::AudioFrame frame(2, 16000, 1);
    return frame.frame_count() == 2 ? 0 : 4;
}
CPP
cmake -S "$scratch/consumer" -B "$scratch/consumer-build" -DCMAKE_PREFIX_PATH="$scratch/install"
cmake --build "$scratch/consumer-build" --parallel 2
"$scratch/consumer-build/consumer"
