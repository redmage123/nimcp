/**
 * @file test_stdp_refactored.cpp
 * @brief Unit tests for refactored STDP module
 *
 * Tests the new logging, config, security, and async features added during refactoring.
 */

#include <gtest/gtest.h>
#include "plasticity/stdp/nimcp_stdp.h"
#include "security/nimcp_security_integration.h"
#include "utils/config/nimcp_dynamic_config.h"
#include "utils/logging/nimcp_logging.h"

class STDPRefactoredTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logging system
        log_init();
        log_set_level(LOG_LEVEL_DEBUG);

        // Initialize config system
        config_init(nullptr);

        // Ensure STDP module is not initialized
        stdp_module_shutdown();
    }

    void TearDown() override {
        // Cleanup STDP module
        stdp_module_shutdown();

        // Cleanup config
        config_shutdown();

        // Cleanup logging
        log_shutdown();
    }
};

/**
 * Test basic module initialization
 */
TEST_F(STDPRefactoredTest, ModuleInit) {
    // Test init without security context
    EXPECT_TRUE(stdp_module_init(nullptr));

    // Verify multiple inits are safe
    EXPECT_TRUE(stdp_module_init(nullptr));

    // Cleanup
    stdp_module_shutdown();
}

/**
 * Test security integration
 */
TEST_F(STDPRefactoredTest, SecurityRegistration) {
    // Create security context
    nimcp_sec_integration_t* sec_ctx = nimcp_sec_integration_create();
    ASSERT_NE(sec_ctx, nullptr);

    nimcp_sec_integration_config_t sec_cfg = nimcp_sec_integration_default_config();
    ASSERT_EQ(nimcp_sec_integration_init(sec_ctx, &sec_cfg), NIMCP_SUCCESS);

    // Initialize STDP with security context
    EXPECT_TRUE(stdp_module_init(sec_ctx));

    // Verify module statistics work
    uint64_t ltp_count = 0, ltd_count = 0, da_queries = 0;
    stdp_module_get_stats(&ltp_count, &ltd_count, &da_queries);
    EXPECT_EQ(ltp_count, 0);
    EXPECT_EQ(ltd_count, 0);
    EXPECT_EQ(da_queries, 0);

    // Cleanup
    stdp_module_shutdown();
    nimcp_sec_integration_destroy(sec_ctx);
}

/**
 * Test configuration integration
 */
TEST_F(STDPRefactoredTest, ConfigurationIntegration) {
    // Set custom config values
    config_set_float("stdp.learning_rate", 0.02f);
    config_set_float("stdp.a_plus", 0.006f);
    config_set_float("stdp.a_minus", 0.007f);
    config_set_bool("stdp.enable_da_modulation", false);

    // Get default config (should read from config system)
    stdp_config_t config = stdp_config_default();

    // Verify config values were read
    EXPECT_FLOAT_EQ(config.learning_rate, 0.02f);
    EXPECT_FLOAT_EQ(config.a_plus, 0.006f);
    EXPECT_FLOAT_EQ(config.a_minus, 0.007f);
    EXPECT_FALSE(config.enable_da_modulation);

    // Verify fallback to defaults for unset values
    EXPECT_FLOAT_EQ(config.w_max, 1.0f);  // Should be default
}

/**
 * Test logging functionality
 */
TEST_F(STDPRefactoredTest, LoggingFunctionality) {
    // Initialize module (should generate INFO logs)
    EXPECT_TRUE(stdp_module_init(nullptr));

    // Create synapse (should generate DEBUG logs)
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    // Process spikes (should generate DEBUG logs)
    float dw1 = stdp_pre_spike(&synapse, 1.0f);
    float dw2 = stdp_post_spike(&synapse, 2.0f);

    EXPECT_LE(dw1, 0.0f);  // LTD should be negative or zero
    EXPECT_GE(dw2, 0.0f);  // LTP should be positive or zero

    // Print stats (should generate INFO logs)
    stdp_synapse_print_stats(&synapse);

    // Shutdown (should generate INFO log with stats)
    stdp_module_shutdown();

    // Note: In a real test, you'd capture and verify log output
    // For now, we just ensure no crashes
}

/**
 * Test async dopamine query mechanism
 */
TEST_F(STDPRefactoredTest, AsyncDopamineQuery) {
    EXPECT_TRUE(stdp_module_init(nullptr));

    // Set baseline dopamine level via config
    config_set_float("stdp.da_baseline", 0.1f);

    // Create synapse with DA modulation enabled
    stdp_config_t config = stdp_config_default();
    config.enable_da_modulation = true;

    stdp_synapse_t synapse;
    stdp_synapse_init_with_config(&synapse, &config);

    // Process modulated spike (should trigger async DA query)
    float dw = stdp_post_spike_modulated(&synapse, 1.0f, nullptr);

    // Verify DA query was tracked
    uint64_t ltp_count = 0, ltd_count = 0, da_queries = 0;
    stdp_module_get_stats(&ltp_count, &ltd_count, &da_queries);
    EXPECT_GT(da_queries, 0);  // At least one DA query should have occurred

    stdp_module_shutdown();
}

/**
 * Test module statistics tracking
 */
TEST_F(STDPRefactoredTest, ModuleStatistics) {
    EXPECT_TRUE(stdp_module_init(nullptr));

    // Create synapse
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    // Generate some LTP events
    synapse.pre_trace = 1.0f;
    for (int i = 0; i < 5; i++) {
        stdp_post_spike(&synapse, (float)i);
    }

    // Generate some LTD events
    synapse.post_trace = 1.0f;
    for (int i = 0; i < 3; i++) {
        stdp_pre_spike(&synapse, (float)i);
    }

    // Verify global stats
    uint64_t ltp_count = 0, ltd_count = 0, da_queries = 0;
    stdp_module_get_stats(&ltp_count, &ltd_count, &da_queries);

    EXPECT_EQ(ltp_count, 5);
    EXPECT_EQ(ltd_count, 3);

    stdp_module_shutdown();
}

/**
 * Test synapse initialization with custom config
 */
TEST_F(STDPRefactoredTest, SynapseInitWithConfig) {
    stdp_config_t config;
    config.w_max = 2.0f;
    config.learning_rate = 0.05f;
    config.a_plus = 0.01f;
    config.a_minus = 0.012f;
    config.tau_plus = 0.015f;
    config.tau_minus = 0.025f;
    config.enable_da_modulation = true;
    config.da_modulation_gain = 50.0f;
    config.burst_amplification = 2.0f;

    stdp_synapse_t synapse;
    stdp_synapse_init_with_config(&synapse, &config);

    EXPECT_FLOAT_EQ(synapse.w_max, 2.0f);
    EXPECT_FLOAT_EQ(synapse.learning_rate, 0.05f);
    EXPECT_FLOAT_EQ(synapse.a_plus, 0.01f);
    EXPECT_FLOAT_EQ(synapse.a_minus, 0.012f);
    EXPECT_FLOAT_EQ(synapse.tau_plus, 0.015f);
    EXPECT_FLOAT_EQ(synapse.tau_minus, 0.025f);
    EXPECT_TRUE(synapse.enable_da_modulation);
    EXPECT_FLOAT_EQ(synapse.da_modulation_gain, 50.0f);
    EXPECT_FLOAT_EQ(synapse.burst_amplification, 2.0f);
}

/**
 * Test error handling with NULL pointers
 */
TEST_F(STDPRefactoredTest, ErrorHandling) {
    // Should not crash with NULL synapse
    stdp_synapse_init(nullptr);
    stdp_synapse_init_with_config(nullptr, nullptr);
    stdp_update_traces(nullptr, 1.0f);
    EXPECT_FLOAT_EQ(stdp_pre_spike(nullptr, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(stdp_post_spike(nullptr, 0.0f), 0.0f);
    stdp_synapse_reset(nullptr);
    stdp_synapse_print_stats(nullptr);

    // Should handle NULL modulated function parameters
    EXPECT_FLOAT_EQ(stdp_pre_spike_modulated(nullptr, 0.0f, nullptr), 0.0f);
    EXPECT_FLOAT_EQ(stdp_post_spike_modulated(nullptr, 0.0f, nullptr), 0.0f);
    EXPECT_FLOAT_EQ(stdp_get_da_modulation_factor(nullptr, nullptr), 1.0f);
    EXPECT_FLOAT_EQ(stdp_apply_modulated_weight_change(nullptr, 0.0f, nullptr), 0.0f);
}

/**
 * Test trace updates with logging
 */
TEST_F(STDPRefactoredTest, TraceUpdates) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    // Set initial traces
    synapse.pre_trace = 1.0f;
    synapse.post_trace = 1.0f;

    // Update traces (should decay)
    stdp_update_traces(&synapse, 0.01f);  // 10ms

    EXPECT_LT(synapse.pre_trace, 1.0f);
    EXPECT_LT(synapse.post_trace, 1.0f);
    EXPECT_GT(synapse.pre_trace, 0.0f);
    EXPECT_GT(synapse.post_trace, 0.0f);

    // Test invalid dt (should log warning)
    stdp_update_traces(&synapse, -1.0f);
    stdp_update_traces(&synapse, 0.0f);
}

/**
 * Test burst detection via config
 */
TEST_F(STDPRefactoredTest, BurstDetection) {
    EXPECT_TRUE(stdp_module_init(nullptr));

    // Set burst threshold
    config_set_float("stdp.burst_threshold", 0.2f);
    config_set_float("stdp.da_baseline", 0.1f);

    stdp_config_t config = stdp_config_default();
    config.enable_da_modulation = true;
    config.burst_amplification = 3.0f;

    stdp_synapse_t synapse;
    stdp_synapse_init_with_config(&synapse, &config);

    // Get modulation factor (baseline DA = 0.1 < threshold = 0.2, no burst)
    float mod = stdp_get_da_modulation_factor(&synapse, nullptr);
    EXPECT_GT(mod, 1.0f);  // Should have some modulation
    EXPECT_LT(mod, config.burst_amplification);  // But not burst amplification

    stdp_module_shutdown();
}

/**
 * Test config hot-reload capability
 */
TEST_F(STDPRefactoredTest, ConfigHotReload) {
    // Set initial config
    config_set_float("stdp.learning_rate", 0.01f);

    stdp_config_t config1 = stdp_config_default();
    EXPECT_FLOAT_EQ(config1.learning_rate, 0.01f);

    // Change config (simulating hot-reload)
    config_set_float("stdp.learning_rate", 0.02f);

    stdp_config_t config2 = stdp_config_default();
    EXPECT_FLOAT_EQ(config2.learning_rate, 0.02f);

    // Existing synapses would need to be updated manually
    // This test just verifies config changes are reflected
}

/**
 * Test module shutdown cleanup
 */
TEST_F(STDPRefactoredTest, ShutdownCleanup) {
    // Multiple init/shutdown cycles
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(stdp_module_init(nullptr));

        // Do some work
        stdp_synapse_t synapse;
        stdp_synapse_init(&synapse);
        stdp_post_spike(&synapse, 1.0f);

        stdp_module_shutdown();
    }

    // Verify clean state after shutdown
    stdp_module_shutdown();  // Should be safe to call when not initialized
}

/**
 * Integration test: Complete STDP workflow
 */
TEST_F(STDPRefactoredTest, CompleteWorkflow) {
    // Initialize all systems
    EXPECT_TRUE(stdp_module_init(nullptr));

    // Configure via config system
    config_set_float("stdp.learning_rate", 0.01f);
    config_set_bool("stdp.enable_da_modulation", true);

    // Create synapse
    stdp_config_t config = stdp_config_default();
    stdp_synapse_t synapse;
    stdp_synapse_init_with_config(&synapse, &config);

    // Simulate spike sequence: pre -> post (LTP)
    synapse.pre_trace = 0.0f;
    synapse.post_trace = 0.0f;

    stdp_pre_spike(&synapse, 0.0f);  // Pre fires, sets pre_trace
    EXPECT_GT(synapse.pre_trace, 0.0f);

    float ltp = stdp_post_spike(&synapse, 1.0f);  // Post fires with existing pre_trace
    EXPECT_GT(ltp, 0.0f);  // Should have potentiation
    EXPECT_GT(synapse.weight, 0.5f);  // Weight should increase from initial 0.5

    // Reset and test reverse: post -> pre (LTD)
    stdp_synapse_reset(&synapse, 0);
    synapse.weight = 0.5f;

    stdp_post_spike(&synapse, 0.0f);  // Post fires, sets post_trace
    EXPECT_GT(synapse.post_trace, 0.0f);

    float ltd = stdp_pre_spike(&synapse, 1.0f);  // Pre fires with existing post_trace
    EXPECT_LT(ltd, 0.0f);  // Should have depression
    EXPECT_LT(synapse.weight, 0.5f);  // Weight should decrease

    // Verify statistics were tracked
    uint64_t ltp_count = 0, ltd_count = 0, da_queries = 0;
    stdp_module_get_stats(&ltp_count, &ltd_count, &da_queries);
    EXPECT_GT(ltp_count, 0);
    EXPECT_GT(ltd_count, 0);

    // Print final stats
    stdp_synapse_print_stats(&synapse);

    // Cleanup
    stdp_module_shutdown();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
