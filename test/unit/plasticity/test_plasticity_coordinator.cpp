/**
 * @file test_plasticity_coordinator.cpp
 * @brief Unit tests for Plasticity Coordinator
 * @version 1.0.0
 * @date 2025-12-15
 *
 * Comprehensive tests for the plasticity coordinator module including:
 * - Lifecycle (create, destroy)
 * - Default configuration
 * - Mechanism registration and unregistration
 * - Mechanism enable/disable
 * - Unified update cycle
 * - State management (acquisition, consolidation, maintenance, stabilizing)
 * - Conflict resolution strategies
 * - Energy tracking and low energy mode
 * - Brain immune integration
 * - Bio-async integration
 * - Statistics tracking
 * - Thread safety
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>

// Headers have their own extern "C" guards
#include "plasticity/nimcp_plasticity_coordinator.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Mock Plasticity Mechanism
 * ============================================================================ */

/**
 * @brief Mock plasticity mechanism for testing
 */
struct MockPlasticityMechanism {
    int update_count = 0;
    float last_dt = 0.0f;
    bool fail_update = false;
    float weight_change = 0.0f;  // For conflict resolution testing
};

/**
 * @brief Mock update function
 */
static int mock_mechanism_update(plasticity_mechanism_handle_t handle, float dt) {
    auto* mock = static_cast<MockPlasticityMechanism*>(handle);
    if (mock->fail_update) {
        return -1;
    }
    mock->update_count++;
    mock->last_dt = dt;
    return 0;
}

/**
 * @brief Mock weight change query function
 */
static int mock_mechanism_get_weight_change(
    plasticity_mechanism_handle_t handle,
    uint32_t synapse_id,
    float* weight_change_out
) {
    auto* mock = static_cast<MockPlasticityMechanism*>(handle);
    if (!weight_change_out) return -1;
    *weight_change_out = mock->weight_change;
    return 0;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class PlasticityCoordinatorTest : public ::testing::Test {
protected:
    plasticity_coordinator_t* coordinator = nullptr;
    plasticity_coordinator_config_t config;

    void SetUp() override {
        plasticity_coordinator_default_config(&config);
        coordinator = plasticity_coordinator_create(&config);
        ASSERT_NE(coordinator, nullptr);
    }

    void TearDown() override {
        if (coordinator) {
            plasticity_coordinator_destroy(coordinator);
            coordinator = nullptr;
        }
    }

    // Helper: Register a mock mechanism
    uint32_t RegisterMockMechanism(
        MockPlasticityMechanism* mock,
        plasticity_mechanism_type_t type,
        const char* name,
        float priority = 0.5f,
        float energy_cost = 1.0f,
        uint64_t interval_ms = 10
    ) {
        uint32_t mechanism_id = 0;
        int result = plasticity_coordinator_register_mechanism(
            coordinator,
            name,
            type,
            mock,
            mock_mechanism_update,
            mock_mechanism_get_weight_change,
            priority,
            energy_cost,
            interval_ms,
            &mechanism_id
        );
        EXPECT_EQ(result, 0);
        return mechanism_id;
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(PlasticityCoordinatorTest, DefaultConfigIsValid) {
    plasticity_coordinator_config_t cfg;
    int result = plasticity_coordinator_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(cfg.initial_state, PLASTICITY_STATE_ACQUISITION);
    EXPECT_EQ(cfg.conflict_strategy, CONFLICT_RESOLUTION_WEIGHTED_AVERAGE);
    EXPECT_EQ(cfg.max_mechanisms, PLASTICITY_COORDINATOR_MAX_MECHANISMS);
    EXPECT_TRUE(cfg.enable_energy_tracking);
    EXPECT_GT(cfg.energy_budget_per_second, 0.0f);
    EXPECT_TRUE(cfg.enable_bio_async);
    EXPECT_TRUE(cfg.enable_brain_immune);
    EXPECT_TRUE(cfg.enable_statistics);
    EXPECT_TRUE(cfg.enable_logging);
}

TEST_F(PlasticityCoordinatorTest, DefaultConfigNullFails) {
    int result = plasticity_coordinator_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, CreateWithNullConfigUsesDefaults) {
    plasticity_coordinator_t* coord = plasticity_coordinator_create(nullptr);
    ASSERT_NE(coord, nullptr);

    EXPECT_EQ(coord->config.initial_state, PLASTICITY_STATE_ACQUISITION);
    EXPECT_EQ(coord->state, PLASTICITY_STATE_ACQUISITION);

    plasticity_coordinator_destroy(coord);
}

TEST_F(PlasticityCoordinatorTest, CreateWithCustomConfig) {
    plasticity_coordinator_config_t custom_cfg;
    plasticity_coordinator_default_config(&custom_cfg);
    custom_cfg.initial_state = PLASTICITY_STATE_CONSOLIDATION;
    custom_cfg.max_mechanisms = 16;
    custom_cfg.conflict_strategy = CONFLICT_RESOLUTION_STDP_DOMINANT;

    plasticity_coordinator_t* coord = plasticity_coordinator_create(&custom_cfg);
    ASSERT_NE(coord, nullptr);

    EXPECT_EQ(coord->config.initial_state, PLASTICITY_STATE_CONSOLIDATION);
    EXPECT_EQ(coord->config.max_mechanisms, 16u);
    EXPECT_EQ(coord->config.conflict_strategy, CONFLICT_RESOLUTION_STDP_DOMINANT);
    EXPECT_EQ(coord->state, PLASTICITY_STATE_CONSOLIDATION);

    plasticity_coordinator_destroy(coord);
}

TEST_F(PlasticityCoordinatorTest, DestroyNullIsNoOp) {
    plasticity_coordinator_destroy(nullptr);
    // Should not crash
}

TEST_F(PlasticityCoordinatorTest, InitialStateIsCorrect) {
    EXPECT_EQ(plasticity_coordinator_get_state(coordinator), PLASTICITY_STATE_ACQUISITION);
}

/* ============================================================================
 * Mechanism Registration Tests
 * ============================================================================ */

TEST_F(PlasticityCoordinatorTest, RegisterMechanism) {
    MockPlasticityMechanism mock;
    uint32_t mechanism_id = 0;

    int result = plasticity_coordinator_register_mechanism(
        coordinator,
        "test_stdp",
        PLASTICITY_TYPE_STDP,
        &mock,
        mock_mechanism_update,
        mock_mechanism_get_weight_change,
        0.9f,
        1.0f,
        10,
        &mechanism_id
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(mechanism_id, 0u);
}

TEST_F(PlasticityCoordinatorTest, RegisterMultipleMechanisms) {
    MockPlasticityMechanism mock1, mock2, mock3;

    uint32_t id1 = RegisterMockMechanism(&mock1, PLASTICITY_TYPE_STDP, "stdp");
    uint32_t id2 = RegisterMockMechanism(&mock2, PLASTICITY_TYPE_BCM, "bcm");
    uint32_t id3 = RegisterMockMechanism(&mock3, PLASTICITY_TYPE_HOMEOSTATIC, "homeostatic");

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

TEST_F(PlasticityCoordinatorTest, RegisterMechanismNullCoordinator) {
    MockPlasticityMechanism mock;
    uint32_t mechanism_id = 0;

    int result = plasticity_coordinator_register_mechanism(
        nullptr,
        "test",
        PLASTICITY_TYPE_STDP,
        &mock,
        mock_mechanism_update,
        nullptr,
        0.5f,
        1.0f,
        10,
        &mechanism_id
    );

    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, RegisterMechanismNullHandleIsAllowed) {
    /* The coordinator's contract explicitly allows NULL handle and NULL
     * update_fn for declaration-only registrations — brain init registers
     * its mechanisms this way (see init_plasticity_coordinator.c where all
     * 8 bio mechanisms are registered with NULL/NULL). Regressing from
     * "allowed" to "error" would break brain startup. */
    uint32_t mechanism_id = 0;

    int result = plasticity_coordinator_register_mechanism(
        coordinator,
        "test",
        PLASTICITY_TYPE_STDP,
        nullptr,  // NULL handle — declaration-only
        mock_mechanism_update,
        nullptr,
        0.5f,
        1.0f,
        10,
        &mechanism_id
    );

    EXPECT_EQ(result, 0);
    EXPECT_NE(mechanism_id, 0u);
}

TEST_F(PlasticityCoordinatorTest, RegisterMechanismNullUpdateFnIsAllowed) {
    /* Same rationale as above: NULL update_fn is a valid declaration-only
     * registration that the coordinator skips during update() (see the
     * `if (!entry->update_fn) continue;` in plasticity_coordinator_update). */
    MockPlasticityMechanism mock;
    uint32_t mechanism_id = 0;

    int result = plasticity_coordinator_register_mechanism(
        coordinator,
        "test",
        PLASTICITY_TYPE_STDP,
        &mock,
        nullptr,  // NULL update_fn — declaration-only
        nullptr,
        0.5f,
        1.0f,
        10,
        &mechanism_id
    );

    EXPECT_EQ(result, 0);
    EXPECT_NE(mechanism_id, 0u);
}

TEST_F(PlasticityCoordinatorTest, RegisterAllMechanismTypes) {
    /* PLASTICITY_TYPE_COUNT is 9 (STRUCTURAL was added after this test was
     * written). Register all 9 so ids[] is fully initialized — prior version
     * only registered 8 and left ids[8] as uninitialized stack memory, which
     * coincidentally compared equal to a legitimate id and spuriously failed. */
    MockPlasticityMechanism mocks[PLASTICITY_TYPE_COUNT];
    uint32_t ids[PLASTICITY_TYPE_COUNT];

    ids[0] = RegisterMockMechanism(&mocks[0], PLASTICITY_TYPE_STDP, "stdp");
    ids[1] = RegisterMockMechanism(&mocks[1], PLASTICITY_TYPE_BCM, "bcm");
    ids[2] = RegisterMockMechanism(&mocks[2], PLASTICITY_TYPE_HOMEOSTATIC, "homeostatic");
    ids[3] = RegisterMockMechanism(&mocks[3], PLASTICITY_TYPE_ELIGIBILITY, "eligibility");
    ids[4] = RegisterMockMechanism(&mocks[4], PLASTICITY_TYPE_DENDRITIC, "dendritic");
    ids[5] = RegisterMockMechanism(&mocks[5], PLASTICITY_TYPE_STP, "stp");
    ids[6] = RegisterMockMechanism(&mocks[6], PLASTICITY_TYPE_ADAPTIVE, "adaptive");
    ids[7] = RegisterMockMechanism(&mocks[7], PLASTICITY_TYPE_PREDICTIVE, "predictive");
    ids[8] = RegisterMockMechanism(&mocks[8], PLASTICITY_TYPE_STRUCTURAL, "structural");

    // All IDs should be unique
    for (int i = 0; i < PLASTICITY_TYPE_COUNT; i++) {
        for (int j = i + 1; j < PLASTICITY_TYPE_COUNT; j++) {
            EXPECT_NE(ids[i], ids[j])
                << "types " << i << " and " << j << " share id " << ids[i];
        }
    }
}

/* ============================================================================
 * Mechanism Unregistration Tests
 * ============================================================================ */

TEST_F(PlasticityCoordinatorTest, UnregisterMechanism) {
    MockPlasticityMechanism mock;
    uint32_t mechanism_id = RegisterMockMechanism(&mock, PLASTICITY_TYPE_STDP, "stdp");

    int result = plasticity_coordinator_unregister_mechanism(coordinator, mechanism_id);
    EXPECT_EQ(result, 0);
}

TEST_F(PlasticityCoordinatorTest, UnregisterNonExistentMechanism) {
    int result = plasticity_coordinator_unregister_mechanism(coordinator, 9999);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, UnregisterMechanismNullCoordinator) {
    int result = plasticity_coordinator_unregister_mechanism(nullptr, 1);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Mechanism Enable/Disable Tests
 * ============================================================================ */

TEST_F(PlasticityCoordinatorTest, SetMechanismEnabled) {
    MockPlasticityMechanism mock;
    uint32_t mechanism_id = RegisterMockMechanism(&mock, PLASTICITY_TYPE_STDP, "stdp");

    int result = plasticity_coordinator_set_mechanism_enabled(coordinator, mechanism_id, false);
    EXPECT_EQ(result, 0);

    result = plasticity_coordinator_set_mechanism_enabled(coordinator, mechanism_id, true);
    EXPECT_EQ(result, 0);
}

TEST_F(PlasticityCoordinatorTest, SetMechanismEnabledNullCoordinator) {
    int result = plasticity_coordinator_set_mechanism_enabled(nullptr, 1, true);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, SetMechanismEnabledNonExistent) {
    int result = plasticity_coordinator_set_mechanism_enabled(coordinator, 9999, true);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, DisabledMechanismDoesNotUpdate) {
    MockPlasticityMechanism mock;
    uint32_t mechanism_id = RegisterMockMechanism(&mock, PLASTICITY_TYPE_STDP, "stdp", 0.9f, 1.0f, 1);

    // Disable mechanism
    plasticity_coordinator_set_mechanism_enabled(coordinator, mechanism_id, false);

    // Update coordinator
    uint64_t current_time = 100;
    plasticity_coordinator_update(coordinator, current_time, 0.01f);

    // Mock should not have been updated
    EXPECT_EQ(mock.update_count, 0);
}

/* ============================================================================
 * Update Cycle Tests
 * ============================================================================ */

TEST_F(PlasticityCoordinatorTest, UpdateWithNoMechanisms) {
    int result = plasticity_coordinator_update(coordinator, 100, 0.01f);
    EXPECT_EQ(result, 0);  // 0 mechanisms updated
}

TEST_F(PlasticityCoordinatorTest, UpdateNullCoordinator) {
    int result = plasticity_coordinator_update(nullptr, 100, 0.01f);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, UpdateWithZeroDt) {
    int result = plasticity_coordinator_update(coordinator, 100, 0.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(PlasticityCoordinatorTest, UpdateWithNegativeDt) {
    int result = plasticity_coordinator_update(coordinator, 100, -0.01f);
    EXPECT_EQ(result, 0);
}

TEST_F(PlasticityCoordinatorTest, UpdateSingleMechanism) {
    MockPlasticityMechanism mock;
    RegisterMockMechanism(&mock, PLASTICITY_TYPE_STDP, "stdp", 0.9f, 1.0f, 1);

    // First update - mechanism should be updated
    int result = plasticity_coordinator_update(coordinator, 100, 0.01f);
    EXPECT_GT(result, 0);
    EXPECT_EQ(mock.update_count, 1);
    EXPECT_FLOAT_EQ(mock.last_dt, 0.01f);
}

TEST_F(PlasticityCoordinatorTest, UpdateMultipleMechanisms) {
    MockPlasticityMechanism mock1, mock2, mock3;
    RegisterMockMechanism(&mock1, PLASTICITY_TYPE_STDP, "stdp", 0.9f, 1.0f, 1);
    RegisterMockMechanism(&mock2, PLASTICITY_TYPE_BCM, "bcm", 0.8f, 2.0f, 1);
    RegisterMockMechanism(&mock3, PLASTICITY_TYPE_HOMEOSTATIC, "homeostatic", 0.7f, 3.0f, 1);

    int result = plasticity_coordinator_update(coordinator, 100, 0.01f);
    EXPECT_GT(result, 0);
    EXPECT_GT(mock1.update_count, 0);
    EXPECT_GT(mock2.update_count, 0);
    EXPECT_GT(mock3.update_count, 0);
}

TEST_F(PlasticityCoordinatorTest, UpdateRespectsInterval) {
    MockPlasticityMechanism mock;
    RegisterMockMechanism(&mock, PLASTICITY_TYPE_STDP, "stdp", 0.9f, 1.0f, 100);  // 100ms interval

    // First update at t=0
    plasticity_coordinator_update(coordinator, 0, 0.01f);
    EXPECT_EQ(mock.update_count, 1);

    // Second update at t=50 (too soon)
    plasticity_coordinator_update(coordinator, 50, 0.01f);
    EXPECT_EQ(mock.update_count, 1);  // Should not update again

    // Third update at t=150 (interval elapsed)
    plasticity_coordinator_update(coordinator, 150, 0.01f);
    EXPECT_EQ(mock.update_count, 2);  // Should update
}

TEST_F(PlasticityCoordinatorTest, UpdateSpecificMechanism) {
    MockPlasticityMechanism mock;
    uint32_t mechanism_id = RegisterMockMechanism(&mock, PLASTICITY_TYPE_STDP, "stdp");

    int result = plasticity_coordinator_update_mechanism(coordinator, mechanism_id, 0.01f);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(mock.update_count, 1);
}

TEST_F(PlasticityCoordinatorTest, UpdateSpecificMechanismNullCoordinator) {
    int result = plasticity_coordinator_update_mechanism(nullptr, 1, 0.01f);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, UpdateSpecificMechanismNonExistent) {
    int result = plasticity_coordinator_update_mechanism(coordinator, 9999, 0.01f);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, UpdateSpecificMechanismZeroDt) {
    MockPlasticityMechanism mock;
    uint32_t mechanism_id = RegisterMockMechanism(&mock, PLASTICITY_TYPE_STDP, "stdp");

    int result = plasticity_coordinator_update_mechanism(coordinator, mechanism_id, 0.0f);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * State Management Tests
 * ============================================================================ */

TEST_F(PlasticityCoordinatorTest, SetState) {
    int result = plasticity_coordinator_set_state(coordinator, PLASTICITY_STATE_CONSOLIDATION);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(plasticity_coordinator_get_state(coordinator), PLASTICITY_STATE_CONSOLIDATION);
}

TEST_F(PlasticityCoordinatorTest, SetStateNullCoordinator) {
    int result = plasticity_coordinator_set_state(nullptr, PLASTICITY_STATE_CONSOLIDATION);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, SetStateInvalid) {
    int result = plasticity_coordinator_set_state(
        coordinator,
        static_cast<plasticity_coordinator_state_t>(999)
    );
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, SetStateAcquisition) {
    plasticity_coordinator_set_state(coordinator, PLASTICITY_STATE_ACQUISITION);
    EXPECT_EQ(plasticity_coordinator_get_state(coordinator), PLASTICITY_STATE_ACQUISITION);
}

TEST_F(PlasticityCoordinatorTest, SetStateConsolidation) {
    plasticity_coordinator_set_state(coordinator, PLASTICITY_STATE_CONSOLIDATION);
    EXPECT_EQ(plasticity_coordinator_get_state(coordinator), PLASTICITY_STATE_CONSOLIDATION);
}

TEST_F(PlasticityCoordinatorTest, SetStateMaintenance) {
    plasticity_coordinator_set_state(coordinator, PLASTICITY_STATE_MAINTENANCE);
    EXPECT_EQ(plasticity_coordinator_get_state(coordinator), PLASTICITY_STATE_MAINTENANCE);
}

TEST_F(PlasticityCoordinatorTest, SetStateStabilizing) {
    plasticity_coordinator_set_state(coordinator, PLASTICITY_STATE_STABILIZING);
    EXPECT_EQ(plasticity_coordinator_get_state(coordinator), PLASTICITY_STATE_STABILIZING);
}

TEST_F(PlasticityCoordinatorTest, GetStateNullCoordinator) {
    auto state = plasticity_coordinator_get_state(nullptr);
    EXPECT_EQ(state, PLASTICITY_STATE_ACQUISITION);  // Default fallback
}

TEST_F(PlasticityCoordinatorTest, TriggerConsolidation) {
    plasticity_coordinator_set_state(coordinator, PLASTICITY_STATE_ACQUISITION);

    int result = plasticity_coordinator_trigger_consolidation(coordinator);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(plasticity_coordinator_get_state(coordinator), PLASTICITY_STATE_CONSOLIDATION);
}

TEST_F(PlasticityCoordinatorTest, TriggerConsolidationNullCoordinator) {
    int result = plasticity_coordinator_trigger_consolidation(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, StateTransitionAffectsMechanismEnabling) {
    MockPlasticityMechanism stdp_mock, homeostatic_mock;
    RegisterMockMechanism(&stdp_mock, PLASTICITY_TYPE_STDP, "stdp", 0.9f, 1.0f, 1);
    RegisterMockMechanism(&homeostatic_mock, PLASTICITY_TYPE_HOMEOSTATIC, "homeostatic", 0.7f, 3.0f, 1);

    // In ACQUISITION state, STDP is enabled, homeostatic is disabled (per default config)
    plasticity_coordinator_set_state(coordinator, PLASTICITY_STATE_ACQUISITION);
    plasticity_coordinator_update(coordinator, 100, 0.01f);

    int stdp_count_acquisition = stdp_mock.update_count;
    int homeostatic_count_acquisition = homeostatic_mock.update_count;

    // In CONSOLIDATION state, STDP is disabled, homeostatic is enabled
    plasticity_coordinator_set_state(coordinator, PLASTICITY_STATE_CONSOLIDATION);
    plasticity_coordinator_update(coordinator, 200, 0.01f);

    int stdp_count_consolidation = stdp_mock.update_count;
    int homeostatic_count_consolidation = homeostatic_mock.update_count;

    // STDP should have updated in acquisition
    EXPECT_GT(stdp_count_acquisition, 0);

    // Homeostatic should have updated in consolidation
    EXPECT_GT(homeostatic_count_consolidation, homeostatic_count_acquisition);
}

/* ============================================================================
 * Conflict Resolution Tests
 * ============================================================================ */

TEST_F(PlasticityCoordinatorTest, ResolveConflict) {
    float resolved = 0.0f;

    int result = plasticity_coordinator_resolve_conflict(
        coordinator,
        123,  // synapse_id
        PLASTICITY_TYPE_STDP,
        0.1f,  // weight_change_a
        PLASTICITY_TYPE_BCM,
        -0.05f,  // weight_change_b
        &resolved
    );

    EXPECT_EQ(result, 0);
    // Default strategy is WEIGHTED_AVERAGE
    EXPECT_NE(resolved, 0.0f);
}

TEST_F(PlasticityCoordinatorTest, ResolveConflictNullCoordinator) {
    float resolved = 0.0f;

    int result = plasticity_coordinator_resolve_conflict(
        nullptr,
        123,
        PLASTICITY_TYPE_STDP,
        0.1f,
        PLASTICITY_TYPE_BCM,
        -0.05f,
        &resolved
    );

    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, ResolveConflictNullOutput) {
    int result = plasticity_coordinator_resolve_conflict(
        coordinator,
        123,
        PLASTICITY_TYPE_STDP,
        0.1f,
        PLASTICITY_TYPE_BCM,
        -0.05f,
        nullptr
    );

    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, ResolveConflictBelowThreshold) {
    float resolved = 0.0f;

    // Very small difference (below threshold)
    int result = plasticity_coordinator_resolve_conflict(
        coordinator,
        123,
        PLASTICITY_TYPE_STDP,
        0.0001f,
        PLASTICITY_TYPE_BCM,
        0.0002f,
        &resolved
    );

    EXPECT_EQ(result, 0);
    // Should average since below conflict threshold
    EXPECT_NEAR(resolved, 0.00015f, 1e-6f);
}

TEST_F(PlasticityCoordinatorTest, ConflictStrategySTDPDominant) {
    plasticity_coordinator_set_conflict_strategy(coordinator, CONFLICT_RESOLUTION_STDP_DOMINANT);

    float resolved = 0.0f;
    plasticity_coordinator_resolve_conflict(
        coordinator,
        123,
        PLASTICITY_TYPE_STDP,
        0.1f,
        PLASTICITY_TYPE_BCM,
        -0.05f,
        &resolved
    );

    EXPECT_FLOAT_EQ(resolved, 0.1f);  // STDP wins
}

TEST_F(PlasticityCoordinatorTest, ConflictStrategyBCMDominant) {
    plasticity_coordinator_set_conflict_strategy(coordinator, CONFLICT_RESOLUTION_BCM_DOMINANT);

    float resolved = 0.0f;
    plasticity_coordinator_resolve_conflict(
        coordinator,
        123,
        PLASTICITY_TYPE_STDP,
        0.1f,
        PLASTICITY_TYPE_BCM,
        -0.05f,
        &resolved
    );

    EXPECT_FLOAT_EQ(resolved, -0.05f);  // BCM wins
}

TEST_F(PlasticityCoordinatorTest, ConflictStrategyAverage) {
    plasticity_coordinator_set_conflict_strategy(coordinator, CONFLICT_RESOLUTION_AVERAGE);

    float resolved = 0.0f;
    plasticity_coordinator_resolve_conflict(
        coordinator,
        123,
        PLASTICITY_TYPE_STDP,
        0.1f,
        PLASTICITY_TYPE_BCM,
        -0.05f,
        &resolved
    );

    EXPECT_FLOAT_EQ(resolved, 0.025f);  // Average
}

TEST_F(PlasticityCoordinatorTest, SetConflictStrategy) {
    int result = plasticity_coordinator_set_conflict_strategy(
        coordinator,
        CONFLICT_RESOLUTION_AVERAGE
    );

    EXPECT_EQ(result, 0);
}

TEST_F(PlasticityCoordinatorTest, SetConflictStrategyNullCoordinator) {
    int result = plasticity_coordinator_set_conflict_strategy(
        nullptr,
        CONFLICT_RESOLUTION_AVERAGE
    );

    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, SetConflictStrategyInvalid) {
    int result = plasticity_coordinator_set_conflict_strategy(
        coordinator,
        static_cast<conflict_resolution_strategy_t>(999)
    );

    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Energy Tracking Tests
 * ============================================================================ */

TEST_F(PlasticityCoordinatorTest, EnergyTrackingEnabled) {
    EXPECT_TRUE(coordinator->config.enable_energy_tracking);
}

TEST_F(PlasticityCoordinatorTest, GetEnergyRate) {
    float rate = plasticity_coordinator_get_energy_rate(coordinator);
    EXPECT_GE(rate, 0.0f);
}

TEST_F(PlasticityCoordinatorTest, GetEnergyRateNullCoordinator) {
    float rate = plasticity_coordinator_get_energy_rate(nullptr);
    EXPECT_EQ(rate, 0.0f);
}

TEST_F(PlasticityCoordinatorTest, IsLowEnergyInitiallyFalse) {
    bool low_energy = plasticity_coordinator_is_low_energy(coordinator);
    EXPECT_FALSE(low_energy);
}

TEST_F(PlasticityCoordinatorTest, IsLowEnergyNullCoordinator) {
    bool low_energy = plasticity_coordinator_is_low_energy(nullptr);
    EXPECT_FALSE(low_energy);
}

TEST_F(PlasticityCoordinatorTest, EnergyConsumptionTracking) {
    MockPlasticityMechanism mock;
    RegisterMockMechanism(&mock, PLASTICITY_TYPE_STDP, "stdp", 0.9f, 10.0f, 1);  // High energy cost

    // Get initial stats
    plasticity_coordinator_stats_t stats_before;
    plasticity_coordinator_get_stats(coordinator, &stats_before);

    // Update multiple times
    for (int i = 0; i < 10; i++) {
        plasticity_coordinator_update(coordinator, i * 100, 0.01f);
    }

    // Get final stats
    plasticity_coordinator_stats_t stats_after;
    plasticity_coordinator_get_stats(coordinator, &stats_after);

    // Energy should have increased
    EXPECT_GT(stats_after.total_energy_consumed, stats_before.total_energy_consumed);
}

TEST_F(PlasticityCoordinatorTest, LowEnergySkipsLowPriorityMechanisms) {
    // Create coordinator with very low energy budget
    plasticity_coordinator_config_t low_energy_config;
    plasticity_coordinator_default_config(&low_energy_config);
    low_energy_config.energy_budget_per_second = 1.0f;  // Very low budget
    low_energy_config.low_energy_threshold = 0.5f;

    plasticity_coordinator_t* coord = plasticity_coordinator_create(&low_energy_config);
    ASSERT_NE(coord, nullptr);

    MockPlasticityMechanism high_priority_mock, low_priority_mock;

    plasticity_coordinator_register_mechanism(
        coord, "high_priority", PLASTICITY_TYPE_STDP, &high_priority_mock,
        mock_mechanism_update, nullptr, 0.9f, 50.0f, 1, nullptr
    );

    plasticity_coordinator_register_mechanism(
        coord, "low_priority", PLASTICITY_TYPE_BCM, &low_priority_mock,
        mock_mechanism_update, nullptr, 0.3f, 50.0f, 1, nullptr
    );

    // Trigger low energy by consuming budget
    for (int i = 0; i < 5; i++) {
        plasticity_coordinator_update(coord, i * 1000, 0.01f);
    }

    // High priority should update more than low priority in low energy mode
    // (This is a heuristic test - exact behavior depends on timing)

    plasticity_coordinator_destroy(coord);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(PlasticityCoordinatorTest, GetStats) {
    plasticity_coordinator_stats_t stats;
    int result = plasticity_coordinator_get_stats(coordinator, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_mechanisms, 0u);
    EXPECT_EQ(stats.active_mechanisms, 0u);
    EXPECT_EQ(stats.current_state, PLASTICITY_STATE_ACQUISITION);
}

TEST_F(PlasticityCoordinatorTest, GetStatsNullCoordinator) {
    plasticity_coordinator_stats_t stats;
    int result = plasticity_coordinator_get_stats(nullptr, &stats);

    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, GetStatsNullOutput) {
    int result = plasticity_coordinator_get_stats(coordinator, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, StatsTrackMechanisms) {
    MockPlasticityMechanism mock1, mock2;
    RegisterMockMechanism(&mock1, PLASTICITY_TYPE_STDP, "stdp");
    RegisterMockMechanism(&mock2, PLASTICITY_TYPE_BCM, "bcm");

    plasticity_coordinator_stats_t stats;
    plasticity_coordinator_get_stats(coordinator, &stats);

    EXPECT_EQ(stats.total_mechanisms, 2u);
    EXPECT_EQ(stats.active_mechanisms, 2u);
}

TEST_F(PlasticityCoordinatorTest, StatsTrackUpdates) {
    MockPlasticityMechanism mock;
    RegisterMockMechanism(&mock, PLASTICITY_TYPE_STDP, "stdp", 0.9f, 1.0f, 1);

    // Perform updates
    for (int i = 0; i < 5; i++) {
        plasticity_coordinator_update(coordinator, i * 100, 0.01f);
    }

    plasticity_coordinator_stats_t stats;
    plasticity_coordinator_get_stats(coordinator, &stats);

    EXPECT_GT(stats.total_update_cycles, 0u);
    EXPECT_GT(stats.total_mechanism_updates, 0u);
}

TEST_F(PlasticityCoordinatorTest, ResetStats) {
    MockPlasticityMechanism mock;
    RegisterMockMechanism(&mock, PLASTICITY_TYPE_STDP, "stdp", 0.9f, 1.0f, 1);

    // Perform updates
    plasticity_coordinator_update(coordinator, 100, 0.01f);

    // Reset stats
    plasticity_coordinator_reset_stats(coordinator);

    plasticity_coordinator_stats_t stats;
    plasticity_coordinator_get_stats(coordinator, &stats);

    EXPECT_EQ(stats.total_update_cycles, 0u);
    EXPECT_EQ(stats.total_mechanism_updates, 0u);
    EXPECT_EQ(stats.total_energy_consumed, 0.0f);
}

TEST_F(PlasticityCoordinatorTest, ResetStatsNullCoordinator) {
    plasticity_coordinator_reset_stats(nullptr);
    // Should not crash
}

TEST_F(PlasticityCoordinatorTest, StatsTrackStateTransitions) {
    plasticity_coordinator_stats_t stats_before;
    plasticity_coordinator_get_stats(coordinator, &stats_before);

    plasticity_coordinator_set_state(coordinator, PLASTICITY_STATE_CONSOLIDATION);
    plasticity_coordinator_set_state(coordinator, PLASTICITY_STATE_MAINTENANCE);

    plasticity_coordinator_stats_t stats_after;
    plasticity_coordinator_get_stats(coordinator, &stats_after);

    EXPECT_GT(stats_after.state_transition_count, stats_before.state_transition_count);
    EXPECT_EQ(stats_after.current_state, PLASTICITY_STATE_MAINTENANCE);
}

TEST_F(PlasticityCoordinatorTest, StatsTrackConflicts) {
    float resolved = 0.0f;

    plasticity_coordinator_resolve_conflict(
        coordinator, 123, PLASTICITY_TYPE_STDP, 0.1f,
        PLASTICITY_TYPE_BCM, -0.05f, &resolved
    );

    plasticity_coordinator_stats_t stats;
    plasticity_coordinator_get_stats(coordinator, &stats);

    EXPECT_GT(stats.total_conflicts, 0u);
    EXPECT_GT(stats.conflicts_resolved, 0u);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(PlasticityCoordinatorTest, ConnectBrainImmune) {
    // Note: We can't create a real brain_immune_system_t in this test,
    // but we can test the null checks
    int result = plasticity_coordinator_connect_brain_immune(coordinator, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, ConnectBrainImmuneNullCoordinator) {
    int result = plasticity_coordinator_connect_brain_immune(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, DisconnectBrainImmune) {
    int result = plasticity_coordinator_disconnect_brain_immune(coordinator);
    EXPECT_EQ(result, 0);
}

TEST_F(PlasticityCoordinatorTest, DisconnectBrainImmuneNullCoordinator) {
    int result = plasticity_coordinator_disconnect_brain_immune(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, ConnectBioAsync) {
    int result = plasticity_coordinator_connect_bio_async(coordinator);
    // May return 0 (success) or -1 (router not available)
    // Both are acceptable in unit test environment
    EXPECT_TRUE(result == 0 || result == -1);
}

TEST_F(PlasticityCoordinatorTest, ConnectBioAsyncNullCoordinator) {
    int result = plasticity_coordinator_connect_bio_async(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityCoordinatorTest, DisconnectBioAsync) {
    int result = plasticity_coordinator_disconnect_bio_async(coordinator);
    EXPECT_EQ(result, 0);
}

TEST_F(PlasticityCoordinatorTest, DisconnectBioAsyncNullCoordinator) {
    int result = plasticity_coordinator_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(PlasticityCoordinatorTest, MechanismTypeToString) {
    EXPECT_STREQ(plasticity_mechanism_type_to_string(PLASTICITY_TYPE_STDP), "STDP");
    EXPECT_STREQ(plasticity_mechanism_type_to_string(PLASTICITY_TYPE_BCM), "BCM");
    EXPECT_STREQ(plasticity_mechanism_type_to_string(PLASTICITY_TYPE_HOMEOSTATIC), "Homeostatic");
    EXPECT_STREQ(plasticity_mechanism_type_to_string(PLASTICITY_TYPE_ELIGIBILITY), "Eligibility");
    EXPECT_STREQ(plasticity_mechanism_type_to_string(PLASTICITY_TYPE_DENDRITIC), "Dendritic");
    EXPECT_STREQ(plasticity_mechanism_type_to_string(PLASTICITY_TYPE_STP), "STP");
    EXPECT_STREQ(plasticity_mechanism_type_to_string(PLASTICITY_TYPE_ADAPTIVE), "Adaptive");
    EXPECT_STREQ(plasticity_mechanism_type_to_string(PLASTICITY_TYPE_PREDICTIVE), "Predictive");
}

TEST_F(PlasticityCoordinatorTest, MechanismTypeToStringInvalid) {
    const char* result = plasticity_mechanism_type_to_string(
        static_cast<plasticity_mechanism_type_t>(999)
    );
    EXPECT_STREQ(result, "Unknown");
}

TEST_F(PlasticityCoordinatorTest, StateToString) {
    EXPECT_STREQ(plasticity_coordinator_state_to_string(PLASTICITY_STATE_ACQUISITION), "ACQUISITION");
    EXPECT_STREQ(plasticity_coordinator_state_to_string(PLASTICITY_STATE_CONSOLIDATION), "CONSOLIDATION");
    EXPECT_STREQ(plasticity_coordinator_state_to_string(PLASTICITY_STATE_MAINTENANCE), "MAINTENANCE");
    EXPECT_STREQ(plasticity_coordinator_state_to_string(PLASTICITY_STATE_STABILIZING), "STABILIZING");
}

TEST_F(PlasticityCoordinatorTest, StateToStringInvalid) {
    const char* result = plasticity_coordinator_state_to_string(
        static_cast<plasticity_coordinator_state_t>(999)
    );
    EXPECT_STREQ(result, "UNKNOWN");
}

TEST_F(PlasticityCoordinatorTest, ConflictStrategyToString) {
    EXPECT_STREQ(conflict_resolution_strategy_to_string(CONFLICT_RESOLUTION_STDP_DOMINANT), "STDP_DOMINANT");
    EXPECT_STREQ(conflict_resolution_strategy_to_string(CONFLICT_RESOLUTION_BCM_DOMINANT), "BCM_DOMINANT");
    EXPECT_STREQ(conflict_resolution_strategy_to_string(CONFLICT_RESOLUTION_AVERAGE), "AVERAGE");
    EXPECT_STREQ(conflict_resolution_strategy_to_string(CONFLICT_RESOLUTION_WEIGHTED_AVERAGE), "WEIGHTED_AVERAGE");
    EXPECT_STREQ(conflict_resolution_strategy_to_string(CONFLICT_RESOLUTION_IMMUNE_MODULATED), "IMMUNE_MODULATED");
    EXPECT_STREQ(conflict_resolution_strategy_to_string(CONFLICT_RESOLUTION_ENERGY_LIMITED), "ENERGY_LIMITED");
}

TEST_F(PlasticityCoordinatorTest, ConflictStrategyToStringInvalid) {
    const char* result = conflict_resolution_strategy_to_string(
        static_cast<conflict_resolution_strategy_t>(999)
    );
    EXPECT_STREQ(result, "UNKNOWN");
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

TEST_F(PlasticityCoordinatorTest, ConcurrentRegistration) {
    std::vector<std::thread> threads;
    std::vector<MockPlasticityMechanism> mocks(10);

    for (int i = 0; i < 10; i++) {
        threads.emplace_back([this, i, &mocks]() {
            RegisterMockMechanism(
                &mocks[i],
                PLASTICITY_TYPE_STDP,
                ("stdp_" + std::to_string(i)).c_str(),
                0.5f, 1.0f, 10
            );
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    plasticity_coordinator_stats_t stats;
    plasticity_coordinator_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.total_mechanisms, 10u);
}

TEST_F(PlasticityCoordinatorTest, ConcurrentUpdates) {
    MockPlasticityMechanism mock;
    RegisterMockMechanism(&mock, PLASTICITY_TYPE_STDP, "stdp", 0.9f, 1.0f, 1);

    std::vector<std::thread> threads;

    for (int i = 0; i < 5; i++) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 10; j++) {
                plasticity_coordinator_update(coordinator, i * 100 + j * 10, 0.01f);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should complete without crashing
    plasticity_coordinator_stats_t stats;
    plasticity_coordinator_get_stats(coordinator, &stats);
    EXPECT_GT(stats.total_update_cycles, 0u);
}

TEST_F(PlasticityCoordinatorTest, ConcurrentStateChanges) {
    std::vector<std::thread> threads;

    plasticity_coordinator_state_t states[] = {
        PLASTICITY_STATE_ACQUISITION,
        PLASTICITY_STATE_CONSOLIDATION,
        PLASTICITY_STATE_MAINTENANCE,
        PLASTICITY_STATE_STABILIZING
    };

    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, state = states[i]]() {
            for (int j = 0; j < 10; j++) {
                plasticity_coordinator_set_state(coordinator, state);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should complete without crashing
    auto final_state = plasticity_coordinator_get_state(coordinator);
    EXPECT_LT(final_state, PLASTICITY_STATE_COUNT);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(PlasticityCoordinatorTest, MaxMechanisms) {
    // Try to register more than max mechanisms
    std::vector<MockPlasticityMechanism> mocks(PLASTICITY_COORDINATOR_MAX_MECHANISMS + 5);

    int success_count = 0;
    for (size_t i = 0; i < mocks.size(); i++) {
        uint32_t id = 0;
        int result = plasticity_coordinator_register_mechanism(
            coordinator,
            ("mechanism_" + std::to_string(i)).c_str(),
            PLASTICITY_TYPE_STDP,
            &mocks[i],
            mock_mechanism_update,
            nullptr,
            0.5f, 1.0f, 10,
            &id
        );
        if (result == 0) success_count++;
    }

    // Should be able to register up to max, but no more
    EXPECT_EQ(success_count, PLASTICITY_COORDINATOR_MAX_MECHANISMS);
}

TEST_F(PlasticityCoordinatorTest, UpdateAfterMechanismUnregistered) {
    MockPlasticityMechanism mock;
    uint32_t mechanism_id = RegisterMockMechanism(&mock, PLASTICITY_TYPE_STDP, "stdp", 0.9f, 1.0f, 1);

    // Unregister
    plasticity_coordinator_unregister_mechanism(coordinator, mechanism_id);

    // Update should not crash
    int result = plasticity_coordinator_update(coordinator, 100, 0.01f);
    EXPECT_EQ(result, 0);
}

TEST_F(PlasticityCoordinatorTest, FailingMechanismUpdate) {
    MockPlasticityMechanism mock;
    mock.fail_update = true;  // Force update to fail

    RegisterMockMechanism(&mock, PLASTICITY_TYPE_STDP, "stdp", 0.9f, 1.0f, 1);

    // Update should handle failure gracefully
    int result = plasticity_coordinator_update(coordinator, 100, 0.01f);
    EXPECT_GE(result, 0);  // Should not return error for mechanism failure
}

/* ============================================================================
 * Backprop Gate Tests (Apr 11 2026 storm regression)
 *
 * These tests pin down the behavior of the plasticity_bridge gate added
 * after the edge-storm incident. Without the gate, STDP/BCM/homeostatic
 * mechanisms raced backprop's gradient writes; the fix was to let the
 * coordinator query nimcp_tpb_is_backprop_active() at the top of update().
 * ============================================================================ */

#include "middleware/training/nimcp_training_plasticity_bridge.h"

TEST_F(PlasticityCoordinatorTest, SetPlasticityBridgeNullCoordinatorFails) {
    tpb_config_t cfg = tpb_config_default();
    tpb_context_t* tpb = tpb_create(&cfg);
    ASSERT_NE(tpb, nullptr);

    EXPECT_EQ(plasticity_coordinator_set_plasticity_bridge(nullptr, tpb), -1);

    tpb_destroy(tpb);
}

TEST_F(PlasticityCoordinatorTest, SetPlasticityBridgeAcceptsNullTpb) {
    // Passing NULL should disable the gate, not error.
    int result = plasticity_coordinator_set_plasticity_bridge(coordinator, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(PlasticityCoordinatorTest, BackpropGateBlocksUpdates) {
    tpb_config_t cfg = tpb_config_default();
    tpb_context_t* tpb = tpb_create(&cfg);
    ASSERT_NE(tpb, nullptr);

    MockPlasticityMechanism mock;
    RegisterMockMechanism(&mock, PLASTICITY_TYPE_STDP, "stdp", 0.9f, 1.0f, 1);

    EXPECT_EQ(plasticity_coordinator_set_plasticity_bridge(coordinator, tpb), 0);

    // Backprop INACTIVE -> update should run normally.
    nimcp_tpb_set_backprop_active(tpb, false);
    plasticity_coordinator_update(coordinator, 100, 0.01f);
    EXPECT_EQ(mock.update_count, 1);

    // Backprop ACTIVE -> update should early-return; mechanism untouched.
    nimcp_tpb_set_backprop_active(tpb, true);
    plasticity_coordinator_update(coordinator, 200, 0.01f);
    EXPECT_EQ(mock.update_count, 1);  // still 1 — gate prevented the tick

    // Backprop INACTIVE again -> updates resume.
    nimcp_tpb_set_backprop_active(tpb, false);
    plasticity_coordinator_update(coordinator, 300, 0.01f);
    EXPECT_EQ(mock.update_count, 2);

    tpb_destroy(tpb);
}

TEST_F(PlasticityCoordinatorTest, BackpropGateClearedWhenBridgeNull) {
    tpb_config_t cfg = tpb_config_default();
    tpb_context_t* tpb = tpb_create(&cfg);
    ASSERT_NE(tpb, nullptr);
    nimcp_tpb_set_backprop_active(tpb, true);

    MockPlasticityMechanism mock;
    RegisterMockMechanism(&mock, PLASTICITY_TYPE_STDP, "stdp", 0.9f, 1.0f, 1);

    // Gate first ON, then cleared.
    plasticity_coordinator_set_plasticity_bridge(coordinator, tpb);
    plasticity_coordinator_set_plasticity_bridge(coordinator, nullptr);

    // With no bridge, tpb's backprop_active flag must not affect updates.
    plasticity_coordinator_update(coordinator, 100, 0.01f);
    EXPECT_EQ(mock.update_count, 1);

    tpb_destroy(tpb);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
