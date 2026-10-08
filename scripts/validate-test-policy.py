#!/usr/bin/env python3
"""Prove CTest keeps wall-clock budgets separate from correctness checks."""
import argparse
import json
import pathlib
import subprocess
import tempfile


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=pathlib.Path)
    args = parser.parse_args()
    cache = {}
    for line in (args.build_dir / "CMakeCache.txt").read_text().splitlines():
        if "=" in line and not line.startswith(("//", "#")):
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    tests = json.loads(subprocess.check_output(
        ["ctest", "--test-dir", str(args.build_dir), "--show-only=json-v1"], text=True
    ))["tests"]
    by_name = {test["name"]: test for test in tests}
    budgets = {
        "smartglasses_apm_test": ("smartglasses_latency_test", "SmartglassesApmTest.RealTimeFrameProcessingLatency"),
        "smartglasses_apm_dsp_pipeline_test": ("smartglasses_dsp_latency_test", "SmartglassesApmDspPipelineTest.Sub10msLatencyBenchmark"),
    }
    enabled = cache.get("BUILD_BENCHMARKS") == "ON"
    for correctness, (performance, case) in budgets.items():
        test = by_name[correctness]
        require(f"--gtest_filter=-{case}" in test["command"], f"{correctness} mixes latency with correctness")
        if enabled:
            measured = by_name[performance]
            props = {item["name"]: item["value"] for item in measured["properties"]}
            require(f"--gtest_filter={case}" in measured["command"], f"{performance} does not select the original budget")
            require("performance" in props["LABELS"] and props["RUN_SERIAL"] is True, f"{performance} must run serially")
            require(cache["CMAKE_BUILD_TYPE"] == "Release", "Performance checks require Release")
        else:
            require(performance not in by_name, f"{performance} requires explicit BUILD_BENCHMARKS=ON")
    # Rejecting inappropriate measurement builds is part of the same contract;
    # these fail before dependency discovery or network access.
    with tempfile.TemporaryDirectory(prefix="apm-test-policy-") as scratch:
        variants = (
            ("debug", "-DCMAKE_BUILD_TYPE=Debug"),
            ("coverage", "-DENABLE_COVERAGE=ON"),
            ("sanitizer", "-DCMAKE_CXX_FLAGS=-fsanitize=undefined"),
        )
        for name, option in variants:
            configured = subprocess.run(
                ["cmake", "-S", cache["CMAKE_HOME_DIRECTORY"], "-B", str(pathlib.Path(scratch) / name),
                 "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TESTING=ON", "-DBUILD_BENCHMARKS=ON", option],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
            )
            require(configured.returncode != 0 and "BUILD_BENCHMARKS requires" in configured.stdout,
                    f"{name} benchmark configuration did not reject instrumentation/debug timing: {configured.stdout}")
    print(f"CTest performance policy verified: {'explicit Release budgets' if enabled else 'correctness only'}")


if __name__ == "__main__":
    main()
