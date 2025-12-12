#include <gtest/gtest.h>

// Simple test to verify testing infrastructure works
TEST(APMSystemTest, BasicTest) {
    EXPECT_EQ(1 + 1, 2);
    EXPECT_TRUE(true);
}

TEST(APMSystemTest, StringTest) {
    std::string test = "APM System";
    EXPECT_EQ(test.length(), 10);
    EXPECT_NE(test, "");
}

// Add a simple function to test coverage
int add_numbers(int a, int b) {
    return a + b;
}

TEST(APMSystemTest, FunctionCoverage) {
    EXPECT_EQ(add_numbers(2, 3), 5);
    EXPECT_EQ(add_numbers(-1, 1), 0);
    EXPECT_EQ(add_numbers(0, 0), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
