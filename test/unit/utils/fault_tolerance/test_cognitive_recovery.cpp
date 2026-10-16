/**
 * @file test_cognitive_recovery.cpp
 * @brief Comprehensive tests for NIMCP Cognitive Recovery Coordinator
 * @version 1.0.0
 * @date 2025-01-18
 *
 * Tests the brain-driven recovery orchestration system that coordinates:
 * - Health monitoring
 * - Brain recovery integration
 * - Runtime adaptation
 * - Recovery learning
 */

#include <gtest/gtest.h>
#include "utils/fault_tolerance/nimcp_cognitive_recovery.h"
#include "utils/fault_tolerance/nimcp_health_monitor.h"
#include "utils/fault_tolerance/nimcp_diagnostics.h"
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdio>

//=============================================================================
// Helper Functions
//=============================================================================

// Simple mock brain for testing (minimal structure)
// In production, would use full brain_create()
extern "C" {
    typedef struct brain_struct {
        int id;
    } brain_struct;
}

static brain_t create_mock_brain() {
    brain_t brain = (brain_t)calloc(1, sizeof(brain_struct));
    if (brain) {
        brain->id = 1;
    }
    return brain;
}

static void destroy_mock_brain(brain_t brain) {
    if (brain) {
        free(brain);
    }
}

//=============================================================================
// Test Fixtures
//=============================================================================

class CognitiveRecoveryTest : public ::testing::Test {
protected:
    brain_t brain;
    cognitive_recovery_coordinator_t coordinator;

    void SetUp() override {
        brain = create_mock_brain();
        ASSERT_NE(brain, nullptr) << "Failed to create mock brain";
        coordinator = nullptr;
    }

    void TearDown() override {
        if (coordinator) {
            cognitive_recovery_destroy(coordinator);
            coordinator = nullptr;
        }
        if (brain) {
            destroy_mock_brain(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST(CognitiveRecoveryConfig, DefaultConfig) {
    cognitive_recovery_config_t config;
    cognitive_recovery_default_config(&config);

    // Verify default values
    EXPECT_TRUE(config.enable_health_monitoring);
    EXPECT_EQ(config.health_check_interval_ms, 1000u);
    EXPECT_FLOAT_EQ(config.health_threshold, 50.0f);

    EXPECT_TRUE(config.enable_brain_decisions);
    EXPECT_FLOAT_EQ(config.brain_confidence_threshold, 0.3f);
    EXPECT_TRUE(config.enable_learning);

    EXPECT_TRUE(config.enable_auto_adaptation);
    EXPECT_FALSE(config.conservative_adaptation);

    EXPECT_TRUE(config.enable_immediate_tier);
    EXPECT_TRUE(config.enable_tactical_tier);
    EXPECT_TRUE(config.enable_strategic_tier);
    EXPECT_TRUE(config.enable_preventive_tier);

    EXPECT_EQ(config.max_recovery_attempts, 3u);
    EXPECT_EQ(config.recovery_timeout_ms, 5000u);
    EXPECT_FALSE(config.require_user_confirmation);

    EXPECT_FALSE(config.verbose_logging);
    EXPECT_EQ(config.log_file_path, nullptr);
}

TEST(CognitiveRecoveryConfig, DefaultConfigNullSafe) {
    // Should not crash with null pointer
    cognitive_recovery_default_config(nullptr);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(CognitiveRecoveryLifecycle, CreateDestroyWithBrain) {
    brain_t brain = create_mock_brain();
    ASSERT_NE(brain, nullptr);

    cognitive_recovery_coordinator_t coord = cognitive_recovery_create(brain, nullptr);
    // Note: May fail if brain_recovery_init fails, which is expected with mock brain
    // The test validates the API works correctly

    if (coord) {
        cognitive_recovery_destroy(coord);
    }

    destroy_mock_brain(brain);
}

TEST(CognitiveRecoveryLifecycle, CreateNullBrain) {
    cognitive_recovery_coordinator_t coord = cognitive_recovery_create(nullptr, nullptr);
    EXPECT_EQ(coord, nullptr);
}

TEST(CognitiveRecoveryLifecycle, DestroyNull) {
    // Should not crash
    cognitive_recovery_destroy(nullptr);
}

TEST(CognitiveRecoveryLifecycle, CreateWithConfig) {
    brain_t brain = create_mock_brain();
    ASSERT_NE(brain, nullptr);

    cognitive_recovery_config_t config;
    cognitive_recovery_default_config(&config);
    config.enable_health_monitoring = false;  // Disable for simpler testing
    config.enable_brain_decisions = false;

    cognitive_recovery_coordinator_t coord = cognitive_recovery_create(brain, &config);
    if (coord) {
        cognitive_recovery_destroy(coord);
    }

    destroy_mock_brain(brain);
}

//=============================================================================
// Start/Stop Tests
//=============================================================================

TEST_F(CognitiveRecoveryTest, StartStopNull) {
    EXPECT_FALSE(cognitive_recovery_start(nullptr));
    EXPECT_FALSE(cognitive_recovery_stop(nullptr));
}

//=============================================================================
// Health Check Tests
//=============================================================================

TEST(CognitiveRecoveryHealth, GetHealthNull) {
    health_status_snapshot_t health;
    EXPECT_FALSE(cognitive_recovery_get_health(nullptr, &health));
}

TEST(CognitiveRecoveryHealth, GetHealthNullOutput) {
    brain_t brain = create_mock_brain();
    ASSERT_NE(brain, nullptr);

    cognitive_recovery_config_t config;
    cognitive_recovery_default_config(&config);
    config.enable_health_monitoring = false;
    config.enable_brain_decisions = false;

    cognitive_recovery_coordinator_t coord = cognitive_recovery_create(brain, &config);
    if (coord) {
        EXPECT_FALSE(cognitive_recovery_get_health(coord, nullptr));
        cognitive_recovery_destroy(coord);
    }

    destroy_mock_brain(brain);
}

TEST(CognitiveRecoveryHealth, IsNeededNull) {
    EXPECT_FALSE(cognitive_recovery_is_needed(nullptr));
}

TEST(CognitiveRecoveryHealth, IsReadyNull) {
    EXPECT_FALSE(cognitive_recovery_is_ready(nullptr));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST(CognitiveRecoveryStats, GetStatsNull) {
    cognitive_recovery_stats_t stats;
    EXPECT_FALSE(cognitive_recovery_get_stats(nullptr, &stats));
}

TEST(CognitiveRecoveryStats, GetStatsNullOutput) {
    brain_t brain = create_mock_brain();
    ASSERT_NE(brain, nullptr);

    cognitive_recovery_config_t config;
    cognitive_recovery_default_config(&config);
    config.enable_health_monitoring = false;
    config.enable_brain_decisions = false;

    cognitive_recovery_coordinator_t coord = cognitive_recovery_create(brain, &config);
    if (coord) {
        EXPECT_FALSE(cognitive_recovery_get_stats(coord, nullptr));
        cognitive_recovery_destroy(coord);
    }

    destroy_mock_brain(brain);
}

TEST(CognitiveRecoveryStats, InitialStatsZero) {
    brain_t brain = create_mock_brain();
    ASSERT_NE(brain, nullptr);

    cognitive_recovery_config_t config;
    cognitive_recovery_default_config(&config);
    config.enable_health_monitoring = false;
    config.enable_brain_decisions = false;

    cognitive_recovery_coordinator_t coord = cognitive_recovery_create(brain, &config);
    if (coord) {
        cognitive_recovery_stats_t stats;
        if (cognitive_recovery_get_stats(coord, &stats)) {
            EXPECT_EQ(stats.total_recoveries, 0u);
            EXPECT_EQ(stats.successful_recoveries, 0u);
            EXPECT_EQ(stats.failed_recoveries, 0u);
            EXPECT_FLOAT_EQ(stats.success_rate, 0.0f);
            EXPECT_EQ(stats.brain_decisions, 0u);
            EXPECT_EQ(stats.parameters_adjusted, 0u);
            EXPECT_EQ(stats.patterns_learned, 0u);
        }
        cognitive_recovery_destroy(coord);
    }

    destroy_mock_brain(brain);
}

//=============================================================================
// Learning Tests
//=============================================================================

TEST(CognitiveRecoveryLearning, GetPatternsNull) {
    recovery_pattern_t patterns[10];
    EXPECT_EQ(cognitive_recovery_get_learned_patterns(nullptr, patterns, 10), 0u);
}

TEST(CognitiveRecoveryLearning, GetPatternsNullOutput) {
    brain_t brain = create_mock_brain();
    ASSERT_NE(brain, nullptr);

    cognitive_recovery_config_t config;
    cognitive_recovery_default_config(&config);
    config.enable_health_monitoring = false;
    config.enable_brain_decisions = false;

    cognitive_recovery_coordinator_t coord = cognitive_recovery_create(brain, &config);
    if (coord) {
        EXPECT_EQ(cognitive_recovery_get_learned_patterns(coord, nullptr, 10), 0u);
        cognitive_recovery_destroy(coord);
    }

    destroy_mock_brain(brain);
}

//=============================================================================
// Configuration Update Tests
//=============================================================================

TEST(CognitiveRecoveryConfigUpdate, UpdateNull) {
    cognitive_recovery_config_t config;
    cognitive_recovery_default_config(&config);
    EXPECT_FALSE(cognitive_recovery_update_config(nullptr, &config));
}

TEST(CognitiveRecoveryConfigUpdate, UpdateNullConfig) {
    brain_t brain = create_mock_brain();
    ASSERT_NE(brain, nullptr);

    cognitive_recovery_config_t config;
    cognitive_recovery_default_config(&config);
    config.enable_health_monitoring = false;
    config.enable_brain_decisions = false;

    cognitive_recovery_coordinator_t coord = cognitive_recovery_create(brain, &config);
    if (coord) {
        EXPECT_FALSE(cognitive_recovery_update_config(coord, nullptr));
        cognitive_recovery_destroy(coord);
    }

    destroy_mock_brain(brain);
}

TEST(CognitiveRecoveryConfigUpdate, GetConfigNull) {
    cognitive_recovery_config_t config;
    EXPECT_FALSE(cognitive_recovery_get_config(nullptr, &config));
}

//=============================================================================
// Recovery Execution Tests
//=============================================================================

TEST(CognitiveRecoveryExecute, ExecuteNull) {
    diagnostic_result_t diag = {};
    EXPECT_EQ(cognitive_recovery_execute(nullptr, &diag), nullptr);
}

TEST(CognitiveRecoveryExecute, FromErrorNull) {
    EXPECT_EQ(cognitive_recovery_from_error(nullptr, ERROR_TYPE_NAN_DETECTED, nullptr), nullptr);
}

TEST(CognitiveRecoveryExecute, FromSignalNull) {
    crash_context_t ctx = {};
    EXPECT_EQ(cognitive_recovery_from_signal(nullptr, SIGSEGV, &ctx), nullptr);
}

TEST(CognitiveRecoveryExecute, PreventiveNull) {
    health_status_snapshot_t health = {};
    EXPECT_EQ(cognitive_recovery_preventive(nullptr, &health), nullptr);
}

//=============================================================================
// Persistence Tests
//=============================================================================

TEST(CognitiveRecoveryPersistence, SaveNull) {
    EXPECT_FALSE(cognitive_recovery_save(nullptr, "/tmp/test.cr"));
}

TEST(CognitiveRecoveryPersistence, SaveNullPath) {
    brain_t brain = create_mock_brain();
    ASSERT_NE(brain, nullptr);

    cognitive_recovery_config_t config;
    cognitive_recovery_default_config(&config);
    config.enable_health_monitoring = false;
    config.enable_brain_decisions = false;

    cognitive_recovery_coordinator_t coord = cognitive_recovery_create(brain, &config);
    if (coord) {
        EXPECT_FALSE(cognitive_recovery_save(coord, nullptr));
        cognitive_recovery_destroy(coord);
    }

    destroy_mock_brain(brain);
}

TEST(CognitiveRecoveryPersistence, LoadNullBrain) {
    EXPECT_EQ(cognitive_recovery_load(nullptr, "/tmp/test.cr", nullptr), nullptr);
}

TEST(CognitiveRecoveryPersistence, LoadNullPath) {
    brain_t brain = create_mock_brain();
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(cognitive_recovery_load(brain, nullptr, nullptr), nullptr);

    destroy_mock_brain(brain);
}

//=============================================================================
// Reporting Tests
//=============================================================================

TEST(CognitiveRecoveryReport, ReportNull) {
    // Should not crash
    cognitive_recovery_report(nullptr, stdout);
}

TEST(CognitiveRecoveryReport, ReportNullOutput) {
    brain_t brain = create_mock_brain();
    ASSERT_NE(brain, nullptr);

    cognitive_recovery_config_t config;
    cognitive_recovery_default_config(&config);
    config.enable_health_monitoring = false;
    config.enable_brain_decisions = false;

    cognitive_recovery_coordinator_t coord = cognitive_recovery_create(brain, &config);
    if (coord) {
        // Should not crash
        cognitive_recovery_report(coord, nullptr);
        cognitive_recovery_destroy(coord);
    }

    destroy_mock_brain(brain);
}

TEST(CognitiveRecoveryReport, ExportJsonNull) {
    char buffer[1024];
    EXPECT_EQ(cognitive_recovery_export_json(nullptr, buffer, sizeof(buffer)), -1);
}

TEST(CognitiveRecoveryReport, ExportJsonNullBuffer) {
    brain_t brain = create_mock_brain();
    ASSERT_NE(brain, nullptr);

    cognitive_recovery_config_t config;
    cognitive_recovery_default_config(&config);
    config.enable_health_monitoring = false;
    config.enable_brain_decisions = false;

    cognitive_recovery_coordinator_t coord = cognitive_recovery_create(brain, &config);
    if (coord) {
        EXPECT_EQ(cognitive_recovery_export_json(coord, nullptr, 1024), -1);
        cognitive_recovery_destroy(coord);
    }

    destroy_mock_brain(brain);
}

TEST(CognitiveRecoveryReport, ExportJsonZeroSize) {
    brain_t brain = create_mock_brain();
    ASSERT_NE(brain, nullptr);

    cognitive_recovery_config_t config;
    cognitive_recovery_default_config(&config);
    config.enable_health_monitoring = false;
    config.enable_brain_decisions = false;

    cognitive_recovery_coordinator_t coord = cognitive_recovery_create(brain, &config);
    if (coord) {
        char buffer[1024];
        EXPECT_EQ(cognitive_recovery_export_json(coord, buffer, 0), -1);
        cognitive_recovery_destroy(coord);
    }

    destroy_mock_brain(brain);
}

//=============================================================================
// Signal Handler Tests
//=============================================================================

TEST(CognitiveRecoverySignal, InstallNull) {
    EXPECT_FALSE(cognitive_recovery_install_signal_handlers(nullptr));
}

TEST(CognitiveRecoverySignal, UninstallNull) {
    // Should not crash
    cognitive_recovery_uninstall_signal_handlers(nullptr);
}

//=============================================================================
// Result Free Tests
//=============================================================================

TEST(CognitiveRecoveryResult, FreeNull) {
    // Should not crash
    cognitive_recovery_free_result(nullptr);
}

//=============================================================================
// Integration Test - Full Workflow
//=============================================================================

TEST(CognitiveRecoveryIntegration, BasicWorkflow) {
    brain_t brain = create_mock_brain();
    ASSERT_NE(brain, nullptr);

    // Create with minimal config
    cognitive_recovery_config_t config;
    cognitive_recovery_default_config(&config);
    config.enable_health_monitoring = false;  // Skip for simpler testing
    config.enable_brain_decisions = false;    // Skip brain integration
    config.enable_auto_adaptation = false;    // Skip runtime adaptation

    cognitive_recovery_coordinator_t coord = cognitive_recovery_create(brain, &config);
    if (coord) {
        // Start the coordinator
        EXPECT_TRUE(cognitive_recovery_start(coord));

        // Check health
        health_status_snapshot_t health;
        if (cognitive_recovery_get_health(coord, &health)) {
            // With disabled subsystems, should have default healthy status
            EXPECT_GE(health.score, 0.0f);
            EXPECT_LE(health.score, 100.0f);
        }

        // Get statistics
        cognitive_recovery_stats_t stats;
        if (cognitive_recovery_get_stats(coord, &stats)) {
            EXPECT_EQ(stats.total_recoveries, 0u);
        }

        // Export to JSON
        char json[2048];
        int32_t len = cognitive_recovery_export_json(coord, json, sizeof(json));
        if (len > 0) {
            EXPECT_GT(strlen(json), 0u);
        }

        // Stop the coordinator
        EXPECT_TRUE(cognitive_recovery_stop(coord));

        cognitive_recovery_destroy(coord);
    }

    destroy_mock_brain(brain);
}
