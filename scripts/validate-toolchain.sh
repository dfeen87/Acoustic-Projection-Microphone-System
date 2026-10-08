#!/usr/bin/env bash
set -euo pipefail
compiler=${CXX:-c++}
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
cat > "$scratch/standard-library.cpp" <<'CPP'
#include <chrono>
#include <ranges>
#include <span>
constexpr std::chrono::hh_mm_ss stamp{std::chrono::milliseconds{1234}};
static_assert(stamp.subseconds().count() == 234);
int main() {
    int values[]{1, 2, 3};
    std::span<int> frame{values};
    int sum = 0;
    for (int value : frame | std::views::take(2)) sum += value;
    return sum == 3 ? 0 : 1;
}
CPP
"$compiler" --version
"$compiler" -std=c++20 "$scratch/standard-library.cpp" -o "$scratch/standard-library"
"$scratch/standard-library"
