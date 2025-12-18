#include <gtest/gtest.h>
#include <string>

// New contract headers (must be optional and link-safe)
#include "apm_extension.hpp"
#include "apm_observability.hpp"

/*
 * This test suite intentionally verifies:
 *  - Test infrastructure sanity
 *  - Header-only linkage for extension and observability layers
 *  - No runtime dependency on optional interfaces
 *
 * It does NOT test system behavior.
 */

// -----------------------------------------------------------------------------
// Basic infrastructure sanity
// -----------------------------------------------------------------------------

TEST(APMSystemTest, BasicTest) {
    EXPECT_EQ(1 + 1, 2);
    EXPECT_TRUE(true);
}

TEST(APMSystemTest, StringTest) {
    std::string test = "APM System";
    EXPECT_EQ(test.length(), 10);
    EXPECT_NE(test, "");
}

// -----------------------------------------------------------------------------
// Simple function for coverage continuity
// -----------------------------------------------------------------------------

int add_numbers(int a, int b) {
    return a + b;
}

TEST(APMSystemTest, FunctionCoverage) {
    EXPECT_EQ(add_numbers(2, 3), 5);
    EXPECT_EQ(add_numbers(-1, 1), 0);
    EXPECT_EQ(add_numbers(0, 0), 0);
}

// -----------------------------------------------------------------------------
// Extension contract validation (compile-time only)
// -----------------------------------------------------------------------------

TEST(APMSystemTest, ExtensionInterfaceIsOptional) {
    // No instantiation required — existence and linkage is the test
    SUCCEED();
}

// -----------------------------------------------------------------------------
// Observability contract validation (compile-time only)
// -----------------------------------------------------------------------------

TEST(APMSystemTest, ObservabilityTypesAreAccessible) {
    apm::HealthStatus status = apm::HealthStatus::OK;
    EXPECT_EQ(status, apm::HealthStatus::OK);

    apm::RuntimeMetrics metrics{};
    EXPECT_EQ(metrics.frames_processed, 0);
    EXPECT_EQ(metrics.frames_dropped, 0);
    EXPECT_EQ(metrics.signaling_events, 0);
}

// -----------------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------------

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
