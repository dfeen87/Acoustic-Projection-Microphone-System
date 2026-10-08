#!/usr/bin/env bash
set -euo pipefail
compiler=${CXX:-c++}
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
cat > "$scratch/standard-library.cpp" <<'CPP'
#include <chrono>
#include <span>
// Exercise the duration and span operations used by APMS. Calendar formatting
// and range adaptors are outside its supported-feature contract.
constexpr auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::milliseconds{1234});
static_assert(stamp.count() == 1234000);
int main() {
    int values[]{1, 2, 3};
    std::span<int> frame{values};
    auto portion = frame.subspan(0, 2);
    int sum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int value : portion) sum += value;
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start);
    return sum == 3 && elapsed.count() >= 0 ? 0 : 1;
}
CPP
"$compiler" --version
"$compiler" -std=c++20 "$scratch/standard-library.cpp" -o "$scratch/standard-library"
"$scratch/standard-library"
