#include <gtest/gtest.h>
#include "apm/apm_core.hpp"

class APMCoreTest : public ::testing::Test {
protected:
    apm::APMCore core;
};

TEST_F(APMCoreTest, GetVersion) {
    std::string version = core.get_version();
    EXPECT_EQ(version, "1.0.0");
    EXPECT_FALSE(version.empty());
}

TEST_F(APMCoreTest, InitializeValid) {
    bool result = core.initialize(48000, 4);
    EXPECT_TRUE(result);
    EXPECT_TRUE(core.is_initialized());
    EXPECT_EQ(core.get_sample_rate(), 48000);
    EXPECT_EQ(core.get_num_channels(), 4);
}

TEST_F(APMCoreTest, InitializeInvalidSampleRate) {
    bool result = core.initialize(0, 4);
    EXPECT_FALSE(result);
    EXPECT_FALSE(core.is_initialized());
}

TEST_F(APMCoreTest, InitializeInvalidChannels) {
    bool result = core.initialize(48000, 0);
    EXPECT_FALSE(result);
    EXPECT_FALSE(core.is_initialized());
}

TEST_F(APMCoreTest, InitializeNegativeValues) {
    bool result = core.initialize(-48000, -4);
    EXPECT_FALSE(result);
    EXPECT_FALSE(core.is_initialized());
}

TEST_F(APMCoreTest, ProcessWithoutInitialize) {
    std::vector<float> input = {1.0f, 2.0f, 3.0f};
    auto output = core.process(input);
    EXPECT_TRUE(output.empty());
}

TEST_F(APMCoreTest, ProcessWithInitialize) {
    core.initialize(48000, 2);
    
    std::vector<float> input = {1.0f, 2.0f, 3.0f, 4.0f};
    auto output = core.process(input);
    
    EXPECT_EQ(output.size(), input.size());
    EXPECT_FALSE(output.empty());
    
    // Check that processing modifies the signal
    for (size_t i = 0; i < input.size(); ++i) {
        EXPECT_NEAR(output[i], input[i] * 0.95f, 0.001f);
    }
}

TEST_F(APMCoreTest, ProcessEmptyInput) {
    core.initialize(48000, 2);
    
    std::vector<float> input;
    auto output = core.process(input);
    
    EXPECT_TRUE(output.empty());
}

TEST_F(APMCoreTest, MultipleInitializations) {
    EXPECT_TRUE(core.initialize(48000, 4));
    EXPECT_EQ(core.get_sample_rate(), 48000);
    
    // Re-initialize with different parameters
    EXPECT_TRUE(core.initialize(44100, 2));
    EXPECT_EQ(core.get_sample_rate(), 44100);
    EXPECT_EQ(core.get_num_channels(), 2);
}
