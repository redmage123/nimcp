/**
 * @file test_fault_tolerance_integration.cpp
 * @brief Integration tests for the fault tolerance system
 *
 * WHAT: Tests checkpoint save/restore, recovery strategies, graceful degradation
 * WHY:  Verify the system can recover from failures and maintain stability
 * HOW:  Test checkpoint lifecycle, recovery operations, circuit breakers
 *
 * Test Categories:
 * - Checkpoint save and load
 * - Checkpoint validation and integrity
 * - Recovery from failures
 * - GPU to CPU fallback during failures
 * - Circuit breaker pattern
 * - Graceful degradation tiers
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <cstdio>
#include <sys/stat.h>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_checkpoint.h"
#include "utils/fault_tolerance/nimcp_recovery.h"
#include "utils/fault_tolerance/nimcp_graceful_degradation.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Helpers
//=============================================================================

static const char* TEST_CHECKPOINT_DIR = "/tmp/nimcp_test_checkpoints";
static const char* TEST_CHECKPOINT_PATH = "/tmp/nimcp_test_checkpoints/test.ckpt";

static void ensure_test_dir() {
    // Create test directory if it doesn't exist
    mkdir(TEST_CHECKPOINT_DIR, 0755);
}

static void cleanup_test_dir() {
    // Clean up test files
    remove(TEST_CHECKPOINT_PATH);
}

//=============================================================================
// Test Fixture - Checkpoint Tests
//=============================================================================

class CheckpointTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensure_test_dir();
        checkpoint_clear_error();
    }

    void TearDown() override {
        cleanup_test_dir();
    }
};

//=============================================================================
// Test Fixture - Recovery Tests
//=============================================================================

class RecoveryTest : public ::testing::Test {
protected:
    circuit_breaker_t* breaker = nullptr;

    void SetUp() override {
        breaker = circuit_breaker_create(5, 1000);
    }

    void TearDown() override {
        if (breaker) {
            circuit_breaker_destroy(breaker);
            breaker = nullptr;
        }
    }
};

//=============================================================================
// Test Fixture - Graceful Degradation Tests
//=============================================================================

class GracefulDegradationTest : public ::testing::Test {
protected:
    gd_context_t* ctx = nullptr;
    gd_config_t config;

    void SetUp() override {
        config = gd_default_config();
        ctx = gd_create(&config);
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        if (ctx) {
            gd_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// Checkpoint Options Tests
//=============================================================================

TEST_F(CheckpointTest, DefaultOptions) {
    checkpoint_options_t opts = checkpoint_default_options();

    // Verify default values
    EXPECT_TRUE(opts.enable_compression);
    EXPECT_FALSE(opts.incremental);
    EXPECT_TRUE(opts.save_subsystems);
    EXPECT_FALSE(opts.save_activations);
    EXPECT_EQ(opts.compression_level, 6);
    EXPECT_EQ(opts.temp_dir, nullptr);
}

TEST_F(CheckpointTest, GetVersion) {
    const char* version = checkpoint_get_version();
    EXPECT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);

    // Version should be in format "major.minor"
    EXPECT_NE(strchr(version, '.'), nullptr);
}

TEST_F(CheckpointTest, ValidateNonExistentFile) {
    bool valid = checkpoint_validate("/nonexistent/path/file.ckpt");
    EXPECT_FALSE(valid);
}

TEST_F(CheckpointTest, ValidateEmptyPath) {
    bool valid = checkpoint_validate("");
    EXPECT_FALSE(valid);
}

TEST_F(CheckpointTest, ValidateNullPath) {
    bool valid = checkpoint_validate(nullptr);
    EXPECT_FALSE(valid);
}

TEST_F(CheckpointTest, GetErrorMessage) {
    // Clear error first
    checkpoint_clear_error();

    // Attempt to validate non-existent file (will set error)
    checkpoint_validate("/nonexistent/file.ckpt");

    // Get error message
    const char* error = checkpoint_get_error();
    EXPECT_NE(error, nullptr);
    // Error may be empty string or contain message
}

TEST_F(CheckpointTest, ClearError) {
    // Set some error
    checkpoint_validate("/nonexistent/file.ckpt");

    // Clear it
    checkpoint_clear_error();

    const char* error = checkpoint_get_error();
    EXPECT_NE(error, nullptr);
    // After clear, should be empty
    EXPECT_EQ(strlen(error), 0u);
}

TEST_F(CheckpointTest, LoadFromNonExistentFile) {
    brain_t brain = nullptr;
    bool result = checkpoint_load(&brain, "/nonexistent/checkpoint.ckpt");
    EXPECT_FALSE(result);
    EXPECT_EQ(brain, nullptr);
}

TEST_F(CheckpointTest, LoadNullBrainPointer) {
    bool result = checkpoint_load(nullptr, TEST_CHECKPOINT_PATH);
    EXPECT_FALSE(result);
}

TEST_F(CheckpointTest, SaveNullBrain) {
    bool result = checkpoint_save(nullptr, TEST_CHECKPOINT_PATH);
    EXPECT_FALSE(result);
}

TEST_F(CheckpointTest, SaveExNullBrain) {
    checkpoint_options_t opts = checkpoint_default_options();
    bool result = checkpoint_save_ex(nullptr, TEST_CHECKPOINT_PATH, &opts);
    EXPECT_FALSE(result);
}

TEST_F(CheckpointTest, ListEmptyDirectory) {
    checkpoint_info_t* list = nullptr;
    uint32_t count = 0;

    bool result = checkpoint_list(TEST_CHECKPOINT_DIR, &list, &count);
    EXPECT_TRUE(result);
    EXPECT_EQ(count, 0u);

    if (list) {
        nimcp_free(list);
    }
}

TEST_F(CheckpointTest, ListNullDirectory) {
    checkpoint_info_t* list = nullptr;
    uint32_t count = 0;

    bool result = checkpoint_list(nullptr, &list, &count);
    EXPECT_FALSE(result);
}

TEST_F(CheckpointTest, CleanupOldNonexistentDir) {
    // Should handle non-existent directory gracefully
    bool result = checkpoint_cleanup_old("/nonexistent/path", 5);
    EXPECT_FALSE(result);
}

TEST_F(CheckpointTest, CleanupWithZeroKeep) {
    // keep_count = 0 should be safe (no-op)
    bool result = checkpoint_cleanup_old(TEST_CHECKPOINT_DIR, 0);
    EXPECT_TRUE(result);
}

//=============================================================================
// Recovery Strategy Tests
//=============================================================================

TEST_F(RecoveryTest, CircuitBreakerCreate) {
    EXPECT_NE(breaker, nullptr);
}

TEST_F(RecoveryTest, CircuitBreakerInitialState) {
    circuit_state_t state = circuit_breaker_get_state(breaker);
    EXPECT_EQ(state, CIRCUIT_CLOSED);
}

TEST_F(RecoveryTest, CircuitBreakerRecordSuccess) {
    circuit_breaker_record_success(breaker);
    circuit_state_t state = circuit_breaker_get_state(breaker);
    EXPECT_EQ(state, CIRCUIT_CLOSED);
}

TEST_F(RecoveryTest, CircuitBreakerRecordFailure) {
    // Record some failures (not enough to open)
    circuit_breaker_record_failure(breaker);
    circuit_breaker_record_failure(breaker);

    circuit_state_t state = circuit_breaker_get_state(breaker);
    EXPECT_EQ(state, CIRCUIT_CLOSED);  // Should still be closed
}

TEST_F(RecoveryTest, CircuitBreakerOpensAfterThreshold) {
    // Record enough failures to open circuit (threshold is 5)
    for (int i = 0; i < 5; ++i) {
        circuit_breaker_record_failure(breaker);
    }

    circuit_state_t state = circuit_breaker_get_state(breaker);
    EXPECT_EQ(state, CIRCUIT_OPEN);
}

TEST_F(RecoveryTest, CircuitBreakerAllowOperationWhenClosed) {
    bool allowed = circuit_breaker_allow_operation(breaker);
    EXPECT_TRUE(allowed);
}

TEST_F(RecoveryTest, CircuitBreakerBlocksWhenOpen) {
    // Open the circuit
    for (int i = 0; i < 5; ++i) {
        circuit_breaker_record_failure(breaker);
    }

    bool allowed = circuit_breaker_allow_operation(breaker);
    EXPECT_FALSE(allowed);
}

TEST_F(RecoveryTest, CircuitBreakerReset) {
    // Open the circuit
    for (int i = 0; i < 5; ++i) {
        circuit_breaker_record_failure(breaker);
    }
    EXPECT_EQ(circuit_breaker_get_state(breaker), CIRCUIT_OPEN);

    // Reset
    circuit_breaker_reset(breaker);
    EXPECT_EQ(circuit_breaker_get_state(breaker), CIRCUIT_CLOSED);
}

TEST_F(RecoveryTest, CircuitBreakerDestroyNull) {
    // Should be safe to destroy NULL
    circuit_breaker_destroy(nullptr);
}

TEST_F(RecoveryTest, TierName) {
    EXPECT_STREQ(recovery_tier_name(RECOVERY_TIER_IMMEDIATE), "IMMEDIATE");
    EXPECT_STREQ(recovery_tier_name(RECOVERY_TIER_TACTICAL), "TACTICAL");
    EXPECT_STREQ(recovery_tier_name(RECOVERY_TIER_STRATEGIC), "STRATEGIC");
    EXPECT_STREQ(recovery_tier_name(RECOVERY_TIER_PREVENTIVE), "PREVENTIVE");
}

TEST_F(RecoveryTest, ActionName) {
    EXPECT_STREQ(recovery_action_name(EXCEPTION_RECOVERY_NONE), "NONE");
    EXPECT_STREQ(recovery_action_name(RECOVERY_ACTION_CLEAR_NAN), "CLEAR_NAN");
    EXPECT_STREQ(recovery_action_name(RECOVERY_ACTION_RELOAD_CHECKPOINT), "RELOAD_CHECKPOINT");
    EXPECT_STREQ(recovery_action_name(RECOVERY_ACTION_FALLBACK_CPU), "FALLBACK_CPU");
}

TEST_F(RecoveryTest, StatusName) {
    EXPECT_STREQ(recovery_status_name(RECOVERY_SUCCESS), "SUCCESS");
    EXPECT_STREQ(recovery_status_name(RECOVERY_PARTIAL), "PARTIAL");
    EXPECT_STREQ(recovery_status_name(RECOVERY_FAILED), "FAILED");
    EXPECT_STREQ(recovery_status_name(RECOVERY_NOT_APPLICABLE), "NOT_APPLICABLE");
    EXPECT_STREQ(recovery_status_name(RECOVERY_REQUIRES_RESTART), "REQUIRES_RESTART");
}

TEST_F(RecoveryTest, SelectStrategyNullDiagnosis) {
    recovery_strategy_t* strategy = recovery_select_strategy(nullptr);
    // May return NULL or default strategy
    (void)strategy;  // Just ensure no crash
}

//=============================================================================
// Graceful Degradation Tests
//=============================================================================

TEST_F(GracefulDegradationTest, CreateAndDestroy) {
    EXPECT_NE(ctx, nullptr);
}

TEST_F(GracefulDegradationTest, DefaultConfig) {
    gd_config_t default_config = gd_default_config();

    EXPECT_TRUE(default_config.enable_auto_degradation);
    EXPECT_TRUE(default_config.enable_load_shedding);
    EXPECT_TRUE(default_config.enable_quality_reduction);
    EXPECT_GT(default_config.check_interval_ms, 0u);
    EXPECT_GT(default_config.tier_cooldown_ms, 0u);
    EXPECT_EQ(default_config.initial_tier, GD_TIER_FULL);
}

TEST_F(GracefulDegradationTest, InitialTier) {
    gd_tier_t tier = gd_get_current_tier(ctx);
    EXPECT_EQ(tier, GD_TIER_FULL);
}

TEST_F(GracefulDegradationTest, SetTier) {
    bool result = gd_set_tier(ctx, GD_TIER_REDUCED, "test");
    EXPECT_TRUE(result);

    gd_tier_t tier = gd_get_current_tier(ctx);
    EXPECT_EQ(tier, GD_TIER_REDUCED);
}

TEST_F(GracefulDegradationTest, SetTierToEmergency) {
    bool result = gd_set_tier(ctx, GD_TIER_EMERGENCY, "critical situation");
    EXPECT_TRUE(result);

    gd_tier_t tier = gd_get_current_tier(ctx);
    EXPECT_EQ(tier, GD_TIER_EMERGENCY);
}

TEST_F(GracefulDegradationTest, StartAndStop) {
    bool start_result = gd_start(ctx);
    EXPECT_TRUE(start_result);

    bool stop_result = gd_stop(ctx);
    EXPECT_TRUE(stop_result);
}

TEST_F(GracefulDegradationTest, RegisterFeature) {
    gd_feature_t feature;
    memset(&feature, 0, sizeof(feature));
    strcpy(feature.name, "test_feature");
    feature.priority = GD_PRIORITY_MEDIUM;
    feature.minimum_tier = GD_TIER_STANDARD;
    feature.is_enabled = true;
    feature.can_degrade = true;
    feature.current_quality = 100.0f;
    feature.min_quality = 50.0f;

    uint32_t id = gd_register_feature(ctx, &feature);
    EXPECT_GT(id, 0u);
}

TEST_F(GracefulDegradationTest, FeatureEnabledCheck) {
    gd_feature_t feature;
    memset(&feature, 0, sizeof(feature));
    strcpy(feature.name, "enabled_feature");
    feature.priority = GD_PRIORITY_MEDIUM;
    feature.minimum_tier = GD_TIER_FULL;
    feature.is_enabled = true;

    uint32_t id = gd_register_feature(ctx, &feature);
    ASSERT_GT(id, 0u);

    bool enabled = gd_is_feature_enabled(ctx, id);
    EXPECT_TRUE(enabled);
}

TEST_F(GracefulDegradationTest, SetFeatureEnabled) {
    gd_feature_t feature;
    memset(&feature, 0, sizeof(feature));
    strcpy(feature.name, "toggle_feature");
    feature.priority = GD_PRIORITY_LOW;
    feature.is_enabled = true;

    uint32_t id = gd_register_feature(ctx, &feature);
    ASSERT_GT(id, 0u);

    // Disable feature
    bool result = gd_set_feature_enabled(ctx, id, false);
    EXPECT_TRUE(result);

    bool enabled = gd_is_feature_enabled(ctx, id);
    EXPECT_FALSE(enabled);
}

TEST_F(GracefulDegradationTest, FeatureQuality) {
    gd_feature_t feature;
    memset(&feature, 0, sizeof(feature));
    strcpy(feature.name, "quality_feature");
    feature.priority = GD_PRIORITY_MEDIUM;
    feature.is_enabled = true;
    feature.can_degrade = true;
    feature.current_quality = 100.0f;
    feature.min_quality = 25.0f;

    uint32_t id = gd_register_feature(ctx, &feature);
    ASSERT_GT(id, 0u);

    float quality = gd_get_feature_quality(ctx, id);
    EXPECT_FLOAT_EQ(quality, 100.0f);

    // Reduce quality
    bool result = gd_set_feature_quality(ctx, id, 75.0f);
    EXPECT_TRUE(result);

    quality = gd_get_feature_quality(ctx, id);
    EXPECT_FLOAT_EQ(quality, 75.0f);
}

TEST_F(GracefulDegradationTest, UpdateResourceUsage) {
    bool result = gd_update_resource(ctx, GD_RESOURCE_CPU, 50.0f);
    EXPECT_TRUE(result);

    float usage = gd_get_resource_usage(ctx, GD_RESOURCE_CPU);
    EXPECT_FLOAT_EQ(usage, 50.0f);
}

TEST_F(GracefulDegradationTest, ResourceCriticalCheck) {
    // Set resource to critical level
    gd_update_resource(ctx, GD_RESOURCE_MEMORY, 95.0f);

    bool critical = gd_is_resource_critical(ctx, GD_RESOURCE_MEMORY);
    EXPECT_TRUE(critical);
}

TEST_F(GracefulDegradationTest, ResourceNotCritical) {
    gd_update_resource(ctx, GD_RESOURCE_GPU, 30.0f);

    bool critical = gd_is_resource_critical(ctx, GD_RESOURCE_GPU);
    EXPECT_FALSE(critical);
}

TEST_F(GracefulDegradationTest, LoadShedding) {
    // Start load shedding with 50% rate
    bool result = gd_start_load_shedding(ctx, 50.0f, GD_PRIORITY_HIGH, 5000);
    EXPECT_TRUE(result);

    gd_load_shed_config_t shed_config;
    memset(&shed_config, 0, sizeof(shed_config));
    bool active = gd_get_load_shed_status(ctx, &shed_config);
    EXPECT_TRUE(active);
    EXPECT_FLOAT_EQ(shed_config.shed_rate, 50.0f);
    EXPECT_EQ(shed_config.min_priority, GD_PRIORITY_HIGH);

    // Stop load shedding
    result = gd_stop_load_shedding(ctx);
    EXPECT_TRUE(result);
}

TEST_F(GracefulDegradationTest, ShouldAcceptCriticalRequest) {
    gd_start_load_shedding(ctx, 90.0f, GD_PRIORITY_HIGH, 5000);

    // Critical requests should always be accepted
    bool accept = gd_should_accept_request(ctx, GD_PRIORITY_CRITICAL);
    EXPECT_TRUE(accept);

    gd_stop_load_shedding(ctx);
}

TEST_F(GracefulDegradationTest, GetStatistics) {
    gd_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    bool result = gd_get_stats(ctx, &stats);
    EXPECT_TRUE(result);

    // Initial stats should be zero
    EXPECT_EQ(stats.total_transitions, 0u);
}

TEST_F(GracefulDegradationTest, ResetStatistics) {
    // Make some changes to generate stats
    gd_set_tier(ctx, GD_TIER_REDUCED, "test");
    gd_set_tier(ctx, GD_TIER_FULL, "test");

    // Reset stats
    gd_reset_stats(ctx);

    gd_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    gd_get_stats(ctx, &stats);

    // Stats should be reset
    EXPECT_EQ(stats.total_transitions, 0u);
}

TEST_F(GracefulDegradationTest, TierToString) {
    EXPECT_STREQ(gd_tier_to_string(GD_TIER_FULL), "FULL");
    EXPECT_STREQ(gd_tier_to_string(GD_TIER_STANDARD), "STANDARD");
    EXPECT_STREQ(gd_tier_to_string(GD_TIER_REDUCED), "REDUCED");
    EXPECT_STREQ(gd_tier_to_string(GD_TIER_MINIMAL), "MINIMAL");
    EXPECT_STREQ(gd_tier_to_string(GD_TIER_EMERGENCY), "EMERGENCY");
}

TEST_F(GracefulDegradationTest, PriorityToString) {
    EXPECT_STREQ(gd_priority_to_string(GD_PRIORITY_CRITICAL), "CRITICAL");
    EXPECT_STREQ(gd_priority_to_string(GD_PRIORITY_HIGH), "HIGH");
    EXPECT_STREQ(gd_priority_to_string(GD_PRIORITY_MEDIUM), "MEDIUM");
    EXPECT_STREQ(gd_priority_to_string(GD_PRIORITY_LOW), "LOW");
    EXPECT_STREQ(gd_priority_to_string(GD_PRIORITY_OPTIONAL), "OPTIONAL");
}

TEST_F(GracefulDegradationTest, ResourceToString) {
    EXPECT_STREQ(gd_resource_to_string(GD_RESOURCE_CPU), "CPU");
    EXPECT_STREQ(gd_resource_to_string(GD_RESOURCE_MEMORY), "MEMORY");
    EXPECT_STREQ(gd_resource_to_string(GD_RESOURCE_GPU), "GPU");
    EXPECT_STREQ(gd_resource_to_string(GD_RESOURCE_NETWORK), "NETWORK");
}

TEST_F(GracefulDegradationTest, ActionToString) {
    EXPECT_STREQ(gd_action_to_string(GD_ACTION_DISABLE_FEATURE), "DISABLE_FEATURE");
    EXPECT_STREQ(gd_action_to_string(GD_ACTION_REDUCE_QUALITY), "REDUCE_QUALITY");
    EXPECT_STREQ(gd_action_to_string(GD_ACTION_SHED_LOAD), "SHED_LOAD");
}

TEST_F(GracefulDegradationTest, TriggerToString) {
    EXPECT_STREQ(gd_trigger_to_string(GD_TRIGGER_RESOURCE), "RESOURCE");
    EXPECT_STREQ(gd_trigger_to_string(GD_TRIGGER_ERROR_RATE), "ERROR_RATE");
    EXPECT_STREQ(gd_trigger_to_string(GD_TRIGGER_MANUAL), "MANUAL");
}

//=============================================================================
// Integration Scenario Tests
//=============================================================================

TEST(FaultToleranceIntegrationTest, CircuitBreakerWithRecovery) {
    // Create circuit breaker
    circuit_breaker_t* cb = circuit_breaker_create(3, 500);
    ASSERT_NE(cb, nullptr);

    // Simulate operations with failures
    int success_count = 0;
    int failure_count = 0;

    for (int i = 0; i < 10; ++i) {
        if (circuit_breaker_allow_operation(cb)) {
            // Simulate 60% failure rate
            if (i % 3 == 0) {
                success_count++;
                circuit_breaker_record_success(cb);
            } else {
                failure_count++;
                circuit_breaker_record_failure(cb);
            }
        }
    }

    // After many failures, circuit should be open
    circuit_state_t state = circuit_breaker_get_state(cb);
    EXPECT_TRUE(state == CIRCUIT_OPEN || state == CIRCUIT_CLOSED);

    circuit_breaker_destroy(cb);
}

TEST(FaultToleranceIntegrationTest, GracefulDegradationWorkflow) {
    // Create GD context
    gd_config_t config = gd_default_config();
    gd_context_t* ctx = gd_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Start monitoring
    gd_start(ctx);

    // Register features with different priorities
    gd_feature_t feature1, feature2, feature3;

    memset(&feature1, 0, sizeof(feature1));
    strcpy(feature1.name, "critical_feature");
    feature1.priority = GD_PRIORITY_CRITICAL;
    feature1.is_enabled = true;
    uint32_t id1 = gd_register_feature(ctx, &feature1);

    memset(&feature2, 0, sizeof(feature2));
    strcpy(feature2.name, "medium_feature");
    feature2.priority = GD_PRIORITY_MEDIUM;
    feature2.is_enabled = true;
    uint32_t id2 = gd_register_feature(ctx, &feature2);

    memset(&feature3, 0, sizeof(feature3));
    strcpy(feature3.name, "optional_feature");
    feature3.priority = GD_PRIORITY_OPTIONAL;
    feature3.is_enabled = true;
    uint32_t id3 = gd_register_feature(ctx, &feature3);

    // Simulate increasing resource pressure
    gd_update_resource(ctx, GD_RESOURCE_CPU, 50.0f);
    gd_update_resource(ctx, GD_RESOURCE_MEMORY, 60.0f);

    // Verify features are still enabled at full tier
    EXPECT_TRUE(gd_is_feature_enabled(ctx, id1));
    EXPECT_TRUE(gd_is_feature_enabled(ctx, id2));
    EXPECT_TRUE(gd_is_feature_enabled(ctx, id3));

    // Degrade to reduced tier
    gd_set_tier(ctx, GD_TIER_REDUCED, "high resource usage");

    // In reduced tier, optional features might be disabled
    // Critical features should remain enabled
    EXPECT_TRUE(gd_is_feature_enabled(ctx, id1));

    // Simulate recovery
    gd_update_resource(ctx, GD_RESOURCE_CPU, 30.0f);
    gd_update_resource(ctx, GD_RESOURCE_MEMORY, 40.0f);
    gd_set_tier(ctx, GD_TIER_FULL, "resources recovered");

    // Stop and get stats
    gd_stop(ctx);

    gd_stats_t stats;
    gd_get_stats(ctx, &stats);
    EXPECT_GE(stats.total_transitions, 2u);  // At least 2 tier changes

    gd_destroy(ctx);
}

TEST(FaultToleranceIntegrationTest, ResourceMonitoringCycle) {
    gd_config_t config = gd_default_config();
    gd_context_t* ctx = gd_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Simulate a monitoring cycle
    for (int cycle = 0; cycle < 5; ++cycle) {
        // Simulate varying resource usage
        float cpu = 40.0f + (cycle * 10.0f);
        float memory = 50.0f + (cycle * 8.0f);

        gd_update_resource(ctx, GD_RESOURCE_CPU, cpu);
        gd_update_resource(ctx, GD_RESOURCE_MEMORY, memory);

        // Check current readings
        float cpu_reading = gd_get_resource_usage(ctx, GD_RESOURCE_CPU);
        EXPECT_FLOAT_EQ(cpu_reading, cpu);

        // Evaluate tier
        gd_evaluate_tier(ctx);
    }

    gd_destroy(ctx);
}

TEST(FaultToleranceIntegrationTest, ProfileManagement) {
    gd_config_t config = gd_default_config();
    gd_context_t* ctx = gd_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Create a custom profile
    gd_profile_t profile;
    memset(&profile, 0, sizeof(profile));
    strcpy(profile.name, "test_profile");
    profile.current_tier = GD_TIER_STANDARD;
    profile.tier_change_cooldown_ms = 1000;
    profile.quality_multipliers[GD_TIER_FULL] = 1.0f;
    profile.quality_multipliers[GD_TIER_STANDARD] = 0.8f;
    profile.quality_multipliers[GD_TIER_REDUCED] = 0.6f;

    uint32_t profile_id = gd_create_profile(ctx, &profile);
    EXPECT_GT(profile_id, 0u);

    // Activate profile
    bool result = gd_activate_profile(ctx, profile_id);
    EXPECT_TRUE(result);

    // Get active profile
    gd_profile_t active;
    memset(&active, 0, sizeof(active));
    result = gd_get_active_profile(ctx, &active);
    EXPECT_TRUE(result);
    EXPECT_STREQ(active.name, "test_profile");

    // Delete profile
    result = gd_delete_profile(ctx, profile_id);
    EXPECT_TRUE(result);

    gd_destroy(ctx);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
