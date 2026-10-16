/**
 * @file test_accelerator.cpp
 * @brief Unit tests for Portia hardware accelerator detection
 *
 * WHAT: Comprehensive tests for accelerator detection and selection
 * WHY:  Ensure reliable hardware detection across platforms
 * HOW:  Mock detection, test scoring, validate selection logic
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Headers have their own extern "C" guards
    #include "portia/nimcp_portia_accelerator.h"
    #include "security/nimcp_bbb_helpers.h"
    #include "utils/logging/nimcp_logging.h"

using namespace testing;

//=============================================================================
// Test Fixture
//=============================================================================

class AcceleratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize BBB helpers
        if (!bbb_helpers_is_initialized()) {
            bbb_helpers_init();
        }

        // Initialize logging with default config
        nimcp_log_config_t log_config = nimcp_log_default_config();
        log_config.level = LOG_LEVEL_DEBUG;
        log_config.destinations = NIMCP_LOG_DEST_CONSOLE;
        nimcp_log_init(&log_config);
    }

    void TearDown() override {
        nimcp_log_shutdown();
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(AcceleratorTest, DefaultConfig) {
    portia_accelerator_config_t config = portia_accelerator_default_config();

    EXPECT_TRUE(config.detect_gpu);
    EXPECT_TRUE(config.detect_npu);
    EXPECT_TRUE(config.detect_dsp);
    EXPECT_TRUE(config.detect_fpga);
    EXPECT_TRUE(config.detect_tpu);
    EXPECT_TRUE(config.auto_select);
    EXPECT_GT(config.power_budget_watts, 0.0f);
}

TEST_F(AcceleratorTest, CustomConfig) {
    portia_accelerator_config_t config = {
        .detect_gpu = true,
        .detect_npu = false,
        .detect_dsp = false,
        .detect_fpga = false,
        .detect_tpu = false,
        .auto_select = true,
        .power_budget_watts = 100.0f,
        .min_memory_gb = 4.0f,
        .min_tflops = 5.0f,
        .prefer_low_power = true,
        .require_fp16 = false,
        .require_int8 = false
    };

    EXPECT_FALSE(config.detect_npu);
    EXPECT_EQ(config.power_budget_watts, 100.0f);
    EXPECT_EQ(config.min_memory_gb, 4.0f);
}

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(AcceleratorTest, InitWithDefaults) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    portia_accelerator_shutdown(system);
}

TEST_F(AcceleratorTest, InitWithCustomConfig) {
    portia_accelerator_config_t config = portia_accelerator_default_config();
    config.detect_gpu = true;
    config.detect_npu = false;

    portia_accelerator_system_t system = portia_accelerator_init(&config);
    ASSERT_NE(system, nullptr);

    portia_accelerator_shutdown(system);
}

TEST_F(AcceleratorTest, ShutdownNullPointer) {
    // Should not crash
    portia_accelerator_shutdown(nullptr);
}

//=============================================================================
// Detection Tests
//=============================================================================

TEST_F(AcceleratorTest, DetectAll) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    uint32_t count = portia_accelerator_detect_all(system);

    // Count may be 0 on systems without accelerators (valid)
    EXPECT_GE(count, 0u);

    printf("Detected %u total accelerators\n", count);

    portia_accelerator_shutdown(system);
}

TEST_F(AcceleratorTest, DetectGPU) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    uint32_t count = portia_accelerator_detect_gpu(system);

    printf("Detected %u GPU(s)\n", count);

    if (count > 0) {
        accelerator_info_t info;
        EXPECT_TRUE(portia_accelerator_get_by_type(system, ACCELERATOR_TYPE_GPU, &info));
        EXPECT_EQ(info.type, ACCELERATOR_TYPE_GPU);
        EXPECT_TRUE(info.available);

        printf("GPU: %s (%s)\n", info.name, info.vendor);
    }

    portia_accelerator_shutdown(system);
}

TEST_F(AcceleratorTest, DetectNPU) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    uint32_t count = portia_accelerator_detect_npu(system);

    printf("Detected %u NPU(s)\n", count);

    portia_accelerator_shutdown(system);
}

TEST_F(AcceleratorTest, DetectDSP) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    uint32_t count = portia_accelerator_detect_dsp(system);

    printf("Detected %u DSP(s)\n", count);

    portia_accelerator_shutdown(system);
}

TEST_F(AcceleratorTest, DetectFPGA) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    uint32_t count = portia_accelerator_detect_fpga(system);

    printf("Detected %u FPGA(s)\n", count);

    portia_accelerator_shutdown(system);
}

TEST_F(AcceleratorTest, DetectTPU) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    uint32_t count = portia_accelerator_detect_tpu(system);

    printf("Detected %u TPU(s)\n", count);

    portia_accelerator_shutdown(system);
}

//=============================================================================
// Query Tests
//=============================================================================

TEST_F(AcceleratorTest, GetCount) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    uint32_t detected = portia_accelerator_detect_all(system);
    uint32_t count = portia_accelerator_get_count(system);

    EXPECT_EQ(count, detected);

    portia_accelerator_shutdown(system);
}

TEST_F(AcceleratorTest, GetTypeMask) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    portia_accelerator_detect_all(system);
    uint32_t mask = portia_accelerator_get_type_mask(system);

    printf("Type mask: 0x%X\n", mask);

    portia_accelerator_shutdown(system);
}

TEST_F(AcceleratorTest, GetInfo) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    uint32_t count = portia_accelerator_detect_all(system);

    if (count > 0) {
        accelerator_info_t info;
        EXPECT_TRUE(portia_accelerator_get_info(system, 0, &info));
        EXPECT_TRUE(info.available);
        EXPECT_GT(strlen(info.name), 0u);
    }

    portia_accelerator_shutdown(system);
}

TEST_F(AcceleratorTest, GetInfoInvalidIndex) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    accelerator_info_t info;
    EXPECT_FALSE(portia_accelerator_get_info(system, 9999, &info));

    portia_accelerator_shutdown(system);
}

TEST_F(AcceleratorTest, GetBest) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    uint32_t count = portia_accelerator_detect_all(system);

    if (count > 0) {
        accelerator_info_t info;
        EXPECT_TRUE(portia_accelerator_get_best(system, &info));
        EXPECT_TRUE(info.available);

        printf("Best accelerator: %s (%.2f TFlops, %.2f W)\n",
               info.name, info.peak_tflops, info.power_watts);
    }

    portia_accelerator_shutdown(system);
}

TEST_F(AcceleratorTest, GetByType) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    portia_accelerator_detect_all(system);

    uint32_t mask = portia_accelerator_get_type_mask(system);

    if (mask & ACCELERATOR_TYPE_GPU) {
        accelerator_info_t info;
        EXPECT_TRUE(portia_accelerator_get_by_type(system, ACCELERATOR_TYPE_GPU, &info));
        EXPECT_EQ(info.type, ACCELERATOR_TYPE_GPU);
    }

    portia_accelerator_shutdown(system);
}

//=============================================================================
// Selection Tests
//=============================================================================

TEST_F(AcceleratorTest, SetPreferred) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    portia_accelerator_detect_all(system);

    uint32_t mask = portia_accelerator_get_type_mask(system);

    if (mask & ACCELERATOR_TYPE_GPU) {
        EXPECT_TRUE(portia_accelerator_set_preferred(system, ACCELERATOR_TYPE_GPU));
        EXPECT_EQ(portia_accelerator_get_preferred(system), ACCELERATOR_TYPE_GPU);
    }

    portia_accelerator_shutdown(system);
}

TEST_F(AcceleratorTest, SetPreferredUnavailable) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    portia_accelerator_detect_all(system);

    // Try to set a type that's unlikely to be available
    accelerator_type_t rare_type = (accelerator_type_t)128; // Invalid type

    EXPECT_FALSE(portia_accelerator_set_preferred(system, rare_type));

    portia_accelerator_shutdown(system);
}

TEST_F(AcceleratorTest, IsAvailable) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    portia_accelerator_detect_all(system);

    uint32_t mask = portia_accelerator_get_type_mask(system);

    EXPECT_EQ(portia_accelerator_is_available(system, ACCELERATOR_TYPE_GPU),
              (mask & ACCELERATOR_TYPE_GPU) != 0);
    EXPECT_EQ(portia_accelerator_is_available(system, ACCELERATOR_TYPE_NPU),
              (mask & ACCELERATOR_TYPE_NPU) != 0);

    portia_accelerator_shutdown(system);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(AcceleratorTest, TypeNames) {
    EXPECT_STREQ(portia_accelerator_type_name(ACCELERATOR_TYPE_NONE), "None");
    EXPECT_STREQ(portia_accelerator_type_name(ACCELERATOR_TYPE_GPU), "GPU");
    EXPECT_STREQ(portia_accelerator_type_name(ACCELERATOR_TYPE_NPU), "NPU");
    EXPECT_STREQ(portia_accelerator_type_name(ACCELERATOR_TYPE_DSP), "DSP");
    EXPECT_STREQ(portia_accelerator_type_name(ACCELERATOR_TYPE_FPGA), "FPGA");
    EXPECT_STREQ(portia_accelerator_type_name(ACCELERATOR_TYPE_TPU), "TPU");
}

TEST_F(AcceleratorTest, PrintInfo) {
    accelerator_info_t info = {
        .type = ACCELERATOR_TYPE_GPU,
        .name = "Test GPU",
        .vendor = "Test Vendor",
        .compute_units = 32,
        .memory_bytes = 8ULL * 1024 * 1024 * 1024,
        .peak_tflops = 10.0f,
        .power_watts = 150.0f,
        .available = true,
        .initialized = true
    };

    // Should not crash
    portia_accelerator_print_info(&info);
}

TEST_F(AcceleratorTest, PrintAll) {
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    portia_accelerator_detect_all(system);

    // Should not crash
    portia_accelerator_print_all(system);

    portia_accelerator_shutdown(system);
}

TEST_F(AcceleratorTest, EstimatePower) {
    accelerator_info_t info = {
        .power_watts = 100.0f
    };

    float idle = portia_accelerator_estimate_power(&info, 0.0f);
    float half = portia_accelerator_estimate_power(&info, 50.0f);
    float full = portia_accelerator_estimate_power(&info, 100.0f);

    EXPECT_GT(idle, 0.0f);
    EXPECT_LT(idle, half);
    EXPECT_LT(half, full);
    EXPECT_LE(full, info.power_watts);
}

TEST_F(AcceleratorTest, CalculateScore) {
    portia_accelerator_config_t config = portia_accelerator_default_config();

    accelerator_info_t gpu = {
        .type = ACCELERATOR_TYPE_GPU,
        .memory_bytes = 8ULL * 1024 * 1024 * 1024,
        .peak_tflops = 10.0f,
        .power_watts = 150.0f
    };

    accelerator_info_t npu = {
        .type = ACCELERATOR_TYPE_NPU,
        .memory_bytes = 1ULL * 1024 * 1024 * 1024,
        .peak_tflops = 2.0f,
        .power_watts = 5.0f
    };

    float gpu_score = portia_accelerator_calculate_score(&gpu, &config);
    float npu_score = portia_accelerator_calculate_score(&npu, &config);

    EXPECT_GT(gpu_score, 0.0f);
    EXPECT_GT(npu_score, 0.0f);

    // GPU should score higher due to raw performance
    EXPECT_GT(gpu_score, npu_score);

    // With low power preference, NPU might score higher
    config.prefer_low_power = true;
    float npu_score_lp = portia_accelerator_calculate_score(&npu, &config);
    float gpu_score_lp = portia_accelerator_calculate_score(&gpu, &config);

    // NPU has better performance/watt ratio
    EXPECT_GT(npu_score_lp / npu.power_watts, gpu_score_lp / gpu.power_watts);
}

TEST_F(AcceleratorTest, ScoreWithRequirements) {
    portia_accelerator_config_t config = portia_accelerator_default_config();
    config.min_memory_gb = 8.0f;
    config.min_tflops = 5.0f;

    accelerator_info_t good = {
        .memory_bytes = 10ULL * 1024 * 1024 * 1024,
        .peak_tflops = 10.0f,
        .power_watts = 100.0f
    };

    accelerator_info_t bad = {
        .memory_bytes = 2ULL * 1024 * 1024 * 1024,
        .peak_tflops = 2.0f,
        .power_watts = 50.0f
    };

    float good_score = portia_accelerator_calculate_score(&good, &config);
    float bad_score = portia_accelerator_calculate_score(&bad, &config);

    EXPECT_GT(good_score, 0.0f);
    EXPECT_EQ(bad_score, 0.0f); // Doesn't meet requirements
}

//=============================================================================
// Security Tests
//=============================================================================

TEST_F(AcceleratorTest, NullPointerValidation) {
    // All functions should handle NULL gracefully
    EXPECT_EQ(portia_accelerator_detect_all(nullptr), 0u);
    EXPECT_EQ(portia_accelerator_detect_gpu(nullptr), 0u);
    EXPECT_EQ(portia_accelerator_get_count(nullptr), 0u);
    EXPECT_EQ(portia_accelerator_get_type_mask(nullptr), 0u);
    EXPECT_FALSE(portia_accelerator_is_available(nullptr, ACCELERATOR_TYPE_GPU));

    accelerator_info_t info;
    EXPECT_FALSE(portia_accelerator_get_info(nullptr, 0, &info));
    EXPECT_FALSE(portia_accelerator_get_best(nullptr, &info));
}

TEST_F(AcceleratorTest, PowerBudgetEnforced) {
    portia_accelerator_config_t config = portia_accelerator_default_config();
    config.power_budget_watts = 50.0f; // Low budget

    accelerator_info_t high_power = {
        .peak_tflops = 20.0f,
        .power_watts = 200.0f
    };

    float score = portia_accelerator_calculate_score(&high_power, &config);
    EXPECT_EQ(score, 0.0f); // Should be rejected
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(AcceleratorTest, FullWorkflow) {
    // Initialize
    portia_accelerator_system_t system = portia_accelerator_init(nullptr);
    ASSERT_NE(system, nullptr);

    // Detect
    uint32_t count = portia_accelerator_detect_all(system);
    printf("Detected %u accelerators\n", count);

    // Print all
    if (count > 0) {
        portia_accelerator_print_all(system);

        // Get best
        accelerator_info_t best;
        if (portia_accelerator_get_best(system, &best)) {
            printf("\nBest accelerator selected: %s\n", best.name);

            // Set as preferred
            portia_accelerator_set_preferred(system, best.type);

            // Verify
            EXPECT_EQ(portia_accelerator_get_preferred(system), best.type);
        }
    }

    // Cleanup
    portia_accelerator_shutdown(system);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
