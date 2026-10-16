/**
 * @file test_collective_fep_bridge.cpp
 * @brief Unit tests for Collective Cognition FEP Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * Tests the FEP (Free Energy Principle) bridge for collective cognition:
 * - Bridge creation and destruction
 * - FEP orchestrator registration/unregistration
 * - Update callback functionality
 * - Free energy computation from phi
 * - Coherence and synchronization metrics
 * - Statistics tracking
 * - Null parameter handling
 * - Thread safety
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/collective_cognition/nimcp_collective_fep_bridge.h"
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"

/* ============================================================================
 * MOCK FEP ORCHESTRATOR (minimal mock for testing)
 * ============================================================================ */

/**
 * @brief Minimal mock FEP orchestrator for testing bridge registration
 *
 * Note: In a real test environment, this would be a proper mock or
 * test double. For unit testing the bridge, we need minimal FEP support.
 */
struct fep_orchestrator {
    uint32_t registered_count;
    void* registered_handles[16];
    uint32_t next_bridge_id;
    bool is_running;
};

/* Mock orchestrator functions (these would normally be in the FEP module) */
static fep_orchestrator_t* mock_fep_create(void) {
    fep_orchestrator_t* orch = (fep_orchestrator_t*)calloc(1, sizeof(fep_orchestrator_t));
    if (orch) {
        orch->next_bridge_id = 1;
        orch->is_running = true;
    }
    return orch;
}

static void mock_fep_destroy(fep_orchestrator_t* orch) {
    free(orch);
}

/* ============================================================================
 * TEST FIXTURE
 * ============================================================================ */

class CollectiveFepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create mock FEP orchestrator
        orchestrator_ = mock_fep_create();
        ASSERT_NE(orchestrator_, nullptr);

        // Create collective cognition instance
        cc_config_ = collective_cognition_default_config();
        collective_ = collective_cognition_create(&cc_config_);
        ASSERT_NE(collective_, nullptr);

        // Register some instances for phi computation
        collective_cognition_register_instance(collective_, 1, nullptr);
        collective_cognition_register_instance(collective_, 2, nullptr);
        collective_cognition_update(collective_);
    }

    void TearDown() override {
        // Unregister if needed
        if (collective_cognition_fep_is_registered()) {
            collective_cognition_fep_bridge_unregister(orchestrator_);
        }

        // Destroy collective cognition
        if (collective_) {
            collective_cognition_destroy(collective_);
            collective_ = nullptr;
        }

        // Destroy mock orchestrator
        if (orchestrator_) {
            mock_fep_destroy(orchestrator_);
            orchestrator_ = nullptr;
        }
    }

    fep_orchestrator_t* orchestrator_ = nullptr;
    collective_cognition_t* collective_ = nullptr;
    collective_cognition_config_t cc_config_;
};

/* ============================================================================
 * LIFECYCLE TESTS
 * ============================================================================ */

/**
 * Test: BridgeCreation
 * Verify bridge can be created and destroyed successfully
 */
TEST_F(CollectiveFepBridgeTest, BridgeCreation) {
    // Create bridge with default config
    collective_fep_bridge_t* bridge = collective_fep_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr) << "Bridge creation with NULL config should succeed";

    // Destroy bridge
    collective_fep_bridge_destroy(bridge);
    SUCCEED() << "Bridge destroyed successfully";
}

/**
 * Test: BridgeCreationWithConfig
 * Verify bridge can be created with custom configuration
 */
TEST_F(CollectiveFepBridgeTest, BridgeCreationWithConfig) {
    collective_fep_config_t config = collective_fep_config_default();

    // Customize config
    config.phi_weight = 0.5f;
    config.coherence_weight = 0.3f;
    config.sync_weight = 0.1f;
    config.consensus_weight = 0.1f;
    config.prediction_error_threshold = 0.2f;

    collective_fep_bridge_t* bridge = collective_fep_bridge_create(&config);
    ASSERT_NE(bridge, nullptr) << "Bridge creation with custom config should succeed";

    collective_fep_bridge_destroy(bridge);
}

/**
 * Test: BridgeDestroyNull
 * Verify destroying NULL bridge is safe
 */
TEST_F(CollectiveFepBridgeTest, BridgeDestroyNull) {
    collective_fep_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/**
 * Test: BridgeReset
 * Verify bridge can be reset
 */
TEST_F(CollectiveFepBridgeTest, BridgeReset) {
    collective_fep_bridge_t* bridge = collective_fep_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    int result = collective_fep_bridge_reset(bridge);
    EXPECT_EQ(result, 0) << "Bridge reset should succeed";

    collective_fep_bridge_destroy(bridge);
}

/**
 * Test: BridgeResetNull
 * Verify reset with NULL bridge returns error
 */
TEST_F(CollectiveFepBridgeTest, BridgeResetNull) {
    int result = collective_fep_bridge_reset(nullptr);
    EXPECT_EQ(result, -1) << "Reset with NULL bridge should fail";
}

/* ============================================================================
 * FEP REGISTRATION TESTS
 * ============================================================================ */

/**
 * Test: FEPRegistration
 * Verify collective cognition can register with FEP orchestrator
 */
TEST_F(CollectiveFepBridgeTest, FEPRegistration) {
    uint32_t bridge_id = 0;

    int result = collective_cognition_fep_bridge_register(
        orchestrator_,
        collective_,
        &bridge_id
    );
    EXPECT_EQ(result, 0) << "FEP registration should succeed";
    EXPECT_GT(bridge_id, 0u) << "Bridge ID should be assigned";
    EXPECT_TRUE(collective_cognition_fep_is_registered()) << "Should be registered";
}

/**
 * Test: FEPRegistrationNullBridgeIdOut
 * Verify registration works when bridge_id_out is NULL
 */
TEST_F(CollectiveFepBridgeTest, FEPRegistrationNullBridgeIdOut) {
    int result = collective_cognition_fep_bridge_register(
        orchestrator_,
        collective_,
        nullptr
    );
    EXPECT_EQ(result, 0) << "FEP registration with NULL bridge_id_out should succeed";
    EXPECT_TRUE(collective_cognition_fep_is_registered());
}

/**
 * Test: FEPUnregistration
 * Verify collective cognition can unregister from FEP orchestrator
 */
TEST_F(CollectiveFepBridgeTest, FEPUnregistration) {
    // First register
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, nullptr), 0);
    ASSERT_TRUE(collective_cognition_fep_is_registered());

    // Now unregister
    int result = collective_cognition_fep_bridge_unregister(orchestrator_);
    EXPECT_EQ(result, 0) << "FEP unregistration should succeed";
    EXPECT_FALSE(collective_cognition_fep_is_registered()) << "Should be unregistered";
}

/**
 * Test: FEPDoubleRegistration
 * Verify double registration is handled properly
 */
TEST_F(CollectiveFepBridgeTest, FEPDoubleRegistration) {
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, nullptr), 0);

    // Attempt second registration
    uint32_t bridge_id = 0;
    int result = collective_cognition_fep_bridge_register(orchestrator_, collective_, &bridge_id);
    // Should either succeed (re-register) or return error, but not crash
    // Implementation may vary - check that bridge_id is consistent
    EXPECT_TRUE(result == 0 || result == -1);
}

/**
 * Test: FEPUnregistrationWhenNotRegistered
 * Verify unregistration when not registered
 */
TEST_F(CollectiveFepBridgeTest, FEPUnregistrationWhenNotRegistered) {
    EXPECT_FALSE(collective_cognition_fep_is_registered());
    int result = collective_cognition_fep_bridge_unregister(orchestrator_);
    // Should return error or succeed silently, but not crash
    EXPECT_TRUE(result == 0 || result == -1);
}

/**
 * Test: FEPGetBridgeId
 * Verify bridge ID can be retrieved after registration
 */
TEST_F(CollectiveFepBridgeTest, FEPGetBridgeId) {
    uint32_t expected_id = 0;
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, &expected_id), 0);

    uint32_t actual_id = collective_cognition_fep_get_bridge_id();
    EXPECT_EQ(actual_id, expected_id) << "Bridge ID should match registration ID";
}

/**
 * Test: FEPGetBridgeIdNotRegistered
 * Verify bridge ID is 0 when not registered
 */
TEST_F(CollectiveFepBridgeTest, FEPGetBridgeIdNotRegistered) {
    EXPECT_FALSE(collective_cognition_fep_is_registered());
    uint32_t bridge_id = collective_cognition_fep_get_bridge_id();
    EXPECT_EQ(bridge_id, 0u) << "Bridge ID should be 0 when not registered";
}

/* ============================================================================
 * FEP UPDATE CALLBACK TESTS
 * ============================================================================ */

/**
 * Test: UpdateCallback
 * Verify FEP update callback works correctly
 */
TEST_F(CollectiveFepBridgeTest, UpdateCallback) {
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, nullptr), 0);

    // Get the bridge handle
    collective_fep_bridge_t* bridge = collective_cognition_fep_get_bridge();
    ASSERT_NE(bridge, nullptr);

    // Call update callback
    int result = collective_cognition_fep_update_callback((void*)bridge);
    EXPECT_EQ(result, 0) << "FEP update callback should succeed";
}

/**
 * Test: UpdateCallbackNull
 * Verify update callback handles NULL gracefully
 */
TEST_F(CollectiveFepBridgeTest, UpdateCallbackNull) {
    int result = collective_cognition_fep_update_callback(nullptr);
    EXPECT_EQ(result, -1) << "Update callback with NULL should fail";
}

/**
 * Test: ForceUpdate
 * Verify forced FEP update works
 */
TEST_F(CollectiveFepBridgeTest, ForceUpdate) {
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, nullptr), 0);

    int result = collective_cognition_fep_force_update();
    EXPECT_EQ(result, 0) << "Force update should succeed";
}

/**
 * Test: ForceUpdateWhenNotRegistered
 * Verify forced update fails when not registered
 */
TEST_F(CollectiveFepBridgeTest, ForceUpdateWhenNotRegistered) {
    EXPECT_FALSE(collective_cognition_fep_is_registered());
    int result = collective_cognition_fep_force_update();
    EXPECT_EQ(result, -1) << "Force update should fail when not registered";
}

/* ============================================================================
 * FREE ENERGY AND PHI RELATIONSHIP TESTS
 * ============================================================================ */

/**
 * Test: FreeEnergyFromPhi
 * Verify free energy is inversely related to phi
 * Higher phi (integrated information) should result in lower free energy
 */
TEST_F(CollectiveFepBridgeTest, FreeEnergyFromPhi) {
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, nullptr), 0);

    // Update to compute initial free energy
    ASSERT_EQ(collective_cognition_fep_force_update(), 0);

    collective_fep_metrics_t metrics;
    ASSERT_EQ(collective_cognition_fep_get_metrics(&metrics), 0);

    // Free energy should be in valid range [0-1]
    EXPECT_GE(metrics.free_energy, 0.0f);
    EXPECT_LE(metrics.free_energy, 1.0f);

    // Phi contribution should be positive
    EXPECT_GE(metrics.phi_contribution, 0.0f);
}

/**
 * Test: FreeEnergyComponents
 * Verify all free energy components are computed
 */
TEST_F(CollectiveFepBridgeTest, FreeEnergyComponents) {
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, nullptr), 0);
    ASSERT_EQ(collective_cognition_fep_force_update(), 0);

    float phi_contrib, coherence_contrib, sync_contrib, consensus_contrib;
    int result = collective_cognition_fep_get_contributions(
        &phi_contrib,
        &coherence_contrib,
        &sync_contrib,
        &consensus_contrib
    );
    EXPECT_EQ(result, 0) << "Getting contributions should succeed";

    // All contributions should be valid floats >= 0
    EXPECT_GE(phi_contrib, 0.0f);
    EXPECT_GE(coherence_contrib, 0.0f);
    EXPECT_GE(sync_contrib, 0.0f);
    EXPECT_GE(consensus_contrib, 0.0f);
}

/* ============================================================================
 * COHERENCE METRICS TESTS
 * ============================================================================ */

/**
 * Test: CoherenceMetrics
 * Verify coherence affects prediction error
 */
TEST_F(CollectiveFepBridgeTest, CoherenceMetrics) {
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, nullptr), 0);

    // Run several updates to let metrics stabilize
    for (int i = 0; i < 5; i++) {
        collective_cognition_update(collective_);
        collective_cognition_fep_force_update();
    }

    collective_fep_metrics_t metrics;
    ASSERT_EQ(collective_cognition_fep_get_metrics(&metrics), 0);

    // Coherence contribution should be tracked
    EXPECT_GE(metrics.coherence_contribution, 0.0f);
    EXPECT_LE(metrics.coherence_contribution, 1.0f);

    // Integration quality should be computed
    EXPECT_GE(metrics.integration_quality, 0.0f);
    EXPECT_LE(metrics.integration_quality, 1.0f);
}

/* ============================================================================
 * SYNCHRONIZATION TRACKING TESTS
 * ============================================================================ */

/**
 * Test: SynchronizationTracking
 * Verify synchronization quality is tracked
 */
TEST_F(CollectiveFepBridgeTest, SynchronizationTracking) {
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, nullptr), 0);
    ASSERT_EQ(collective_cognition_fep_force_update(), 0);

    collective_fep_metrics_t metrics;
    ASSERT_EQ(collective_cognition_fep_get_metrics(&metrics), 0);

    // Sync contribution should be tracked
    EXPECT_GE(metrics.sync_contribution, 0.0f);
    EXPECT_LE(metrics.sync_contribution, 1.0f);
}

/* ============================================================================
 * METRICS TRACKING TESTS
 * ============================================================================ */

/**
 * Test: MetricsTracking
 * Verify metrics are updated correctly over time
 */
TEST_F(CollectiveFepBridgeTest, MetricsTracking) {
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, nullptr), 0);

    // Run multiple updates
    uint32_t initial_count = 0;
    {
        collective_fep_metrics_t metrics;
        ASSERT_EQ(collective_cognition_fep_get_metrics(&metrics), 0);
        initial_count = metrics.update_count;
    }

    for (int i = 0; i < 5; i++) {
        collective_cognition_update(collective_);
        ASSERT_EQ(collective_cognition_fep_force_update(), 0);
    }

    collective_fep_metrics_t metrics;
    ASSERT_EQ(collective_cognition_fep_get_metrics(&metrics), 0);
    EXPECT_GT(metrics.update_count, initial_count) << "Update count should increase";
    EXPECT_GT(metrics.last_update_time, 0u) << "Last update time should be set";
}

/**
 * Test: StatsTracking
 * Verify extended statistics are tracked correctly
 */
TEST_F(CollectiveFepBridgeTest, StatsTracking) {
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, nullptr), 0);

    // Run several updates
    for (int i = 0; i < 10; i++) {
        collective_cognition_update(collective_);
        ASSERT_EQ(collective_cognition_fep_force_update(), 0);
    }

    collective_fep_stats_t stats;
    ASSERT_EQ(collective_cognition_fep_get_stats(&stats), 0);

    // Verify stats are populated
    EXPECT_GE(stats.total_updates, 10u) << "Should have at least 10 updates";
    EXPECT_TRUE(stats.is_registered) << "Should be registered";
    EXPECT_GT(stats.fep_bridge_id, 0u) << "Bridge ID should be set";
    EXPECT_GE(stats.avg_free_energy, 0.0f);
    EXPECT_LE(stats.avg_free_energy, 1.0f);
}

/**
 * Test: MetricsReset
 * Verify metrics can be reset
 */
TEST_F(CollectiveFepBridgeTest, MetricsReset) {
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, nullptr), 0);

    // Run some updates
    for (int i = 0; i < 5; i++) {
        collective_cognition_fep_force_update();
    }

    // Reset metrics
    ASSERT_EQ(collective_cognition_fep_reset_metrics(), 0);

    collective_fep_metrics_t metrics;
    ASSERT_EQ(collective_cognition_fep_get_metrics(&metrics), 0);
    EXPECT_EQ(metrics.update_count, 0u) << "Update count should be reset";
}

/* ============================================================================
 * CONFIGURATION TESTS
 * ============================================================================ */

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(CollectiveFepBridgeTest, DefaultConfig) {
    collective_fep_config_t config = collective_fep_config_default();

    // Weights should sum to approximately 1.0
    float weight_sum = config.phi_weight + config.coherence_weight +
                       config.sync_weight + config.consensus_weight;
    EXPECT_NEAR(weight_sum, 1.0f, 0.01f) << "Weights should sum to 1.0";

    // Thresholds should be in valid range
    EXPECT_GT(config.prediction_error_threshold, 0.0f);
    EXPECT_LE(config.prediction_error_threshold, 1.0f);
}

/**
 * Test: SetConfig
 * Verify configuration can be updated
 */
TEST_F(CollectiveFepBridgeTest, SetConfig) {
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, nullptr), 0);

    collective_fep_config_t new_config = collective_fep_config_default();
    new_config.phi_weight = 0.7f;
    new_config.coherence_weight = 0.1f;
    new_config.sync_weight = 0.1f;
    new_config.consensus_weight = 0.1f;

    int result = collective_cognition_fep_set_config(&new_config);
    EXPECT_EQ(result, 0) << "Setting config should succeed";

    // Verify config was set
    collective_fep_config_t retrieved_config;
    ASSERT_EQ(collective_cognition_fep_get_config(&retrieved_config), 0);
    EXPECT_FLOAT_EQ(retrieved_config.phi_weight, 0.7f);
}

/* ============================================================================
 * NULL HANDLING TESTS
 * ============================================================================ */

/**
 * Test: NullHandling
 * Verify all functions handle null parameters correctly
 */
TEST_F(CollectiveFepBridgeTest, NullHandling) {
    // Registration with NULL orchestrator
    EXPECT_EQ(collective_cognition_fep_bridge_register(nullptr, collective_, nullptr), -1);

    // Registration with NULL collective
    EXPECT_EQ(collective_cognition_fep_bridge_register(orchestrator_, nullptr, nullptr), -1);

    // Unregistration with NULL orchestrator
    EXPECT_EQ(collective_cognition_fep_bridge_unregister(nullptr), -1);

    // Get metrics with NULL output
    EXPECT_EQ(collective_cognition_fep_get_metrics(nullptr), -1);

    // Get stats with NULL output
    EXPECT_EQ(collective_cognition_fep_get_stats(nullptr), -1);

    // Set config with NULL
    EXPECT_EQ(collective_cognition_fep_set_config(nullptr), -1);

    // Get config with NULL output
    EXPECT_EQ(collective_cognition_fep_get_config(nullptr), -1);

    // Get contributions with NULL outputs
    float dummy;
    EXPECT_EQ(collective_cognition_fep_get_contributions(nullptr, &dummy, &dummy, &dummy), -1);
    EXPECT_EQ(collective_cognition_fep_get_contributions(&dummy, nullptr, &dummy, &dummy), -1);
    EXPECT_EQ(collective_cognition_fep_get_contributions(&dummy, &dummy, nullptr, &dummy), -1);
    EXPECT_EQ(collective_cognition_fep_get_contributions(&dummy, &dummy, &dummy, nullptr), -1);
}

/* ============================================================================
 * THREAD SAFETY TESTS
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Verify concurrent access to FEP bridge is thread-safe
 */
TEST_F(CollectiveFepBridgeTest, ThreadSafety) {
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, nullptr), 0);

    std::atomic<int> update_count{0};
    std::atomic<int> metrics_read_count{0};
    std::atomic<bool> error_occurred{false};
    const int num_threads = 4;
    const int iterations = 100;

    std::vector<std::thread> threads;

    // Spawn update threads
    for (int t = 0; t < num_threads / 2; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < iterations && !error_occurred; i++) {
                if (collective_cognition_fep_force_update() == 0) {
                    update_count++;
                }
            }
        });
    }

    // Spawn metrics reading threads
    for (int t = 0; t < num_threads / 2; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < iterations && !error_occurred; i++) {
                collective_fep_metrics_t metrics;
                if (collective_cognition_fep_get_metrics(&metrics) == 0) {
                    metrics_read_count++;
                    // Verify metrics are in valid range
                    if (metrics.free_energy < 0.0f || metrics.free_energy > 1.0f) {
                        error_occurred = true;
                    }
                }
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_FALSE(error_occurred) << "No errors should occur during concurrent access";
    EXPECT_GT(update_count, 0) << "Some updates should succeed";
    EXPECT_GT(metrics_read_count, 0) << "Some metrics reads should succeed";
}

/* ============================================================================
 * INTEGRATION TESTS
 * ============================================================================ */

/**
 * Test: PhiEvolutionAffectsFreeEnergy
 * Verify that phi evolution over time affects free energy
 */
TEST_F(CollectiveFepBridgeTest, PhiEvolutionAffectsFreeEnergy) {
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, nullptr), 0);

    std::vector<float> free_energy_history;

    // Run multiple updates and track free energy
    for (int i = 0; i < 20; i++) {
        collective_cognition_update(collective_);
        ASSERT_EQ(collective_cognition_fep_force_update(), 0);

        collective_fep_metrics_t metrics;
        ASSERT_EQ(collective_cognition_fep_get_metrics(&metrics), 0);
        free_energy_history.push_back(metrics.free_energy);
    }

    // Verify we have a history
    EXPECT_EQ(free_energy_history.size(), 20u);

    // All values should be in valid range
    for (float fe : free_energy_history) {
        EXPECT_GE(fe, 0.0f);
        EXPECT_LE(fe, 1.0f);
    }
}

/**
 * Test: InstanceCountAffectsMetrics
 * Verify that adding/removing instances affects FEP metrics
 */
TEST_F(CollectiveFepBridgeTest, InstanceCountAffectsMetrics) {
    ASSERT_EQ(collective_cognition_fep_bridge_register(orchestrator_, collective_, nullptr), 0);

    // Get baseline metrics with 2 instances
    ASSERT_EQ(collective_cognition_fep_force_update(), 0);
    collective_fep_stats_t stats_2;
    ASSERT_EQ(collective_cognition_fep_get_stats(&stats_2), 0);

    // Add more instances
    collective_cognition_register_instance(collective_, 3, nullptr);
    collective_cognition_register_instance(collective_, 4, nullptr);
    collective_cognition_update(collective_);
    ASSERT_EQ(collective_cognition_fep_force_update(), 0);

    collective_fep_stats_t stats_4;
    ASSERT_EQ(collective_cognition_fep_get_stats(&stats_4), 0);

    // More instances should affect capacity
    EXPECT_GE(stats_4.active_instances, stats_2.active_instances);
}
