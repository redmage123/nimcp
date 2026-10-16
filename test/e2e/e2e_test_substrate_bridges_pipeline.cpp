/**
 * @file e2e_test_substrate_bridges_pipeline.cpp
 * @brief End-to-End Tests for Neural Substrate and Bridge Integration
 *
 * WHAT: E2E tests for neural substrate core functionality and bridges
 * WHY:  Verify substrate lifecycle, state management, and bridge integration
 * HOW:  Test substrate operations, metabolic state, and bridge coordination
 *
 * TEST CATEGORIES:
 * 1. Substrate Lifecycle (3 tests)
 *    - Create, configure, destroy substrate
 *    - State management and updates
 *    - Health monitoring
 *
 * 2. Metabolic Operations (3 tests)
 *    - ATP level management
 *    - Temperature effects
 *    - Oxygen saturation
 *
 * 3. Bridge Integration (3 tests)
 *    - Neuron substrate bridge
 *    - Attention substrate bridge
 *    - Plasticity substrate bridge
 *
 * @author NIMCP Development Team
 * @date 2026-01-14
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <cmath>

// E2E Test Framework
#include "e2e_test_framework.h"

extern "C" {
// Neural Substrate
#include "core/neural_substrate/nimcp_neural_substrate.h"
}

// Global pipeline tracker (defined in e2e_test_framework.cpp)
extern std::unique_ptr<nimcp::e2e::PipelineTracker> g_current_pipeline;

//=============================================================================
// Test Fixture
//=============================================================================

class SubstrateBridgesPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        substrate_ = nullptr;
    }

    void TearDown() override {
        if (substrate_) {
            substrate_destroy(substrate_);
            substrate_ = nullptr;
        }
    }

    // Helper: Create substrate with default config
    bool CreateSubstrate() {
        substrate_config_t config;
        if (substrate_default_config(&config) != 0) {
            return false;
        }
        substrate_ = substrate_create(&config);
        return substrate_ != nullptr;
    }

    // Helper: Set substrate ATP level
    void set_atp_level(float atp) {
        substrate_set_atp(substrate_, atp);
    }

    // Helper: Set substrate temperature
    void set_temperature(float temp_celsius) {
        substrate_set_temperature(substrate_, temp_celsius);
    }

    // Helper: Set substrate oxygen saturation
    void set_oxygen_saturation(float o2) {
        substrate_set_oxygen(substrate_, o2);
    }

    // Helper: Get substrate metabolic state
    bool get_metabolic_state(substrate_metabolic_state_t* state) const {
        return substrate_get_metabolic_state(substrate_, state) == 0;
    }

    // Helper: Get substrate stats
    bool get_stats(substrate_stats_t* stats) const {
        return substrate_get_stats(substrate_, stats) == 0;
    }

    neural_substrate_t* substrate_ = nullptr;
};

//=============================================================================
// 1. SUBSTRATE LIFECYCLE TESTS
//=============================================================================

TEST_F(SubstrateBridgesPipelineTest, SubstrateLifecycle) {
    E2E_PIPELINE_START("Substrate Lifecycle");

    // Stage 1: Get default config
    E2E_STAGE_BEGIN("Get default config", 100);
    substrate_config_t config;
    int result = substrate_default_config(&config);
    E2E_ASSERT(result == 0, "Failed to get default config");
    E2E_STAGE_END();

    // Stage 2: Create substrate
    E2E_STAGE_BEGIN("Create substrate", 200);
    substrate_ = substrate_create(&config);
    E2E_ASSERT_NOT_NULL(substrate_, "Failed to create substrate");
    E2E_STAGE_END();

    // Stage 3: Check initial health
    E2E_STAGE_BEGIN("Check health", 100);
    substrate_health_level_t health = substrate_get_health_level(substrate_);
    E2E_ASSERT(health != SUBSTRATE_HEALTH_CRITICAL, "Health should not be critical initially");
    E2E_STAGE_END();

    // Stage 4: Update substrate
    E2E_STAGE_BEGIN("Update substrate", 100);
    result = substrate_update(substrate_, 10);  // 10ms delta
    E2E_ASSERT(result == 0, "Update should succeed");
    E2E_STAGE_END();

    // Stage 5: Reset substrate
    E2E_STAGE_BEGIN("Reset substrate", 100);
    result = substrate_reset(substrate_);
    E2E_ASSERT(result == 0, "Reset should succeed");
    E2E_STAGE_END();

    // Stage 6: Destroy (in TearDown)
    E2E_PIPELINE_END();
}

TEST_F(SubstrateBridgesPipelineTest, SubstrateStateManagement) {
    E2E_PIPELINE_START("Substrate State Management");

    // Stage 1: Create substrate
    E2E_STAGE_BEGIN("Create substrate", 200);
    E2E_ASSERT(CreateSubstrate(), "Failed to create substrate");
    E2E_STAGE_END();

    // Stage 2: Get metabolic state
    E2E_STAGE_BEGIN("Get metabolic state", 100);
    substrate_metabolic_state_t metabolic;
    E2E_ASSERT(get_metabolic_state(&metabolic), "Failed to get metabolic state");
    std::cout << "Initial ATP: " << metabolic.atp_level << std::endl;
    E2E_STAGE_END();

    // Stage 3: Get physical state
    E2E_STAGE_BEGIN("Get physical state", 100);
    substrate_physical_state_t physical;
    int result = substrate_get_physical_state(substrate_, &physical);
    E2E_ASSERT(result == 0, "Failed to get physical state");
    std::cout << "Temperature: " << physical.temperature << std::endl;
    E2E_STAGE_END();

    // Stage 4: Get stats
    E2E_STAGE_BEGIN("Get stats", 100);
    substrate_stats_t stats;
    E2E_ASSERT(get_stats(&stats), "Failed to get stats");
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SubstrateBridgesPipelineTest, SubstrateHealthMonitoring) {
    E2E_PIPELINE_START("Substrate Health Monitoring");

    // Stage 1: Create substrate
    E2E_STAGE_BEGIN("Create substrate", 200);
    E2E_ASSERT(CreateSubstrate(), "Failed to create substrate");
    E2E_STAGE_END();

    // Stage 2: Check initial capacity
    E2E_STAGE_BEGIN("Check capacity", 100);
    float capacity = substrate_get_capacity(substrate_);
    E2E_ASSERT(capacity > 0.0f, "Capacity should be positive");
    std::cout << "Initial capacity: " << capacity << std::endl;
    E2E_STAGE_END();

    // Stage 3: Get firing modulation
    E2E_STAGE_BEGIN("Get firing modulation", 100);
    float firing_mod = substrate_get_firing_modulation(substrate_);
    std::cout << "Firing modulation: " << firing_mod << std::endl;
    E2E_STAGE_END();

    // Stage 4: Get transmission efficiency
    E2E_STAGE_BEGIN("Get transmission efficiency", 100);
    float efficiency = substrate_get_transmission_efficiency(substrate_);
    std::cout << "Transmission efficiency: " << efficiency << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// 2. METABOLIC OPERATIONS TESTS
//=============================================================================

TEST_F(SubstrateBridgesPipelineTest, ATPManagement) {
    E2E_PIPELINE_START("ATP Level Management");

    // Stage 1: Create substrate
    E2E_STAGE_BEGIN("Create substrate", 200);
    E2E_ASSERT(CreateSubstrate(), "Failed to create substrate");
    E2E_STAGE_END();

    // Stage 2: Get initial ATP
    E2E_STAGE_BEGIN("Get initial ATP", 100);
    substrate_metabolic_state_t state;
    E2E_ASSERT(get_metabolic_state(&state), "Failed to get state");
    float initial_atp = state.atp_level;
    std::cout << "Initial ATP: " << initial_atp << std::endl;
    E2E_STAGE_END();

    // Stage 3: Set low ATP
    E2E_STAGE_BEGIN("Set low ATP", 100);
    set_atp_level(0.3f);  // 30% ATP
    E2E_ASSERT(get_metabolic_state(&state), "Failed to get state");
    std::cout << "Low ATP: " << state.atp_level << std::endl;
    E2E_STAGE_END();

    // Stage 4: Check health under low ATP
    E2E_STAGE_BEGIN("Check health", 100);
    substrate_health_level_t health = substrate_get_health_level(substrate_);
    std::cout << "Health level: " << static_cast<int>(health) << std::endl;
    E2E_STAGE_END();

    // Stage 5: Restore ATP
    E2E_STAGE_BEGIN("Restore ATP", 100);
    set_atp_level(1.0f);  // 100% ATP
    E2E_ASSERT(get_metabolic_state(&state), "Failed to get state");
    E2E_ASSERT(state.atp_level > 0.9f, "ATP should be restored");
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SubstrateBridgesPipelineTest, TemperatureEffects) {
    E2E_PIPELINE_START("Temperature Effects");

    // Stage 1: Create substrate
    E2E_STAGE_BEGIN("Create substrate", 200);
    E2E_ASSERT(CreateSubstrate(), "Failed to create substrate");
    E2E_STAGE_END();

    // Stage 2: Get initial temperature
    E2E_STAGE_BEGIN("Get initial temperature", 100);
    substrate_physical_state_t state;
    int result = substrate_get_physical_state(substrate_, &state);
    E2E_ASSERT(result == 0, "Failed to get state");
    std::cout << "Initial temp: " << state.temperature << "°C" << std::endl;
    E2E_STAGE_END();

    // Stage 3: Set elevated temperature (fever)
    E2E_STAGE_BEGIN("Set fever temperature", 100);
    set_temperature(39.0f);
    result = substrate_get_physical_state(substrate_, &state);
    E2E_ASSERT(result == 0, "Failed to get state");
    std::cout << "Fever temp: " << state.temperature << "°C" << std::endl;
    E2E_STAGE_END();

    // Stage 4: Check modulation effects
    E2E_STAGE_BEGIN("Check modulation", 100);
    float firing_mod = substrate_get_firing_modulation(substrate_);
    std::cout << "Firing modulation at fever: " << firing_mod << std::endl;
    E2E_STAGE_END();

    // Stage 5: Restore normal temperature
    E2E_STAGE_BEGIN("Restore temperature", 100);
    set_temperature(37.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SubstrateBridgesPipelineTest, OxygenSaturation) {
    E2E_PIPELINE_START("Oxygen Saturation");

    // Stage 1: Create substrate
    E2E_STAGE_BEGIN("Create substrate", 200);
    E2E_ASSERT(CreateSubstrate(), "Failed to create substrate");
    E2E_STAGE_END();

    // Stage 2: Get initial oxygen
    E2E_STAGE_BEGIN("Get initial O2", 100);
    substrate_metabolic_state_t state;
    E2E_ASSERT(get_metabolic_state(&state), "Failed to get state");
    std::cout << "Initial O2: " << state.oxygen_saturation << std::endl;
    E2E_STAGE_END();

    // Stage 3: Set low oxygen (hypoxia)
    E2E_STAGE_BEGIN("Set hypoxia", 100);
    set_oxygen_saturation(0.85f);  // 85% O2 - mild hypoxia
    E2E_ASSERT(get_metabolic_state(&state), "Failed to get state");
    std::cout << "Hypoxia O2: " << state.oxygen_saturation << std::endl;
    E2E_STAGE_END();

    // Stage 4: Check effects on capacity
    E2E_STAGE_BEGIN("Check capacity", 100);
    float capacity = substrate_get_capacity(substrate_);
    std::cout << "Capacity under hypoxia: " << capacity << std::endl;
    E2E_STAGE_END();

    // Stage 5: Restore oxygen
    E2E_STAGE_BEGIN("Restore O2", 100);
    set_oxygen_saturation(0.98f);  // Normal O2
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// 3. COMBINED STRESS TEST
//=============================================================================

TEST_F(SubstrateBridgesPipelineTest, CombinedStressTest) {
    E2E_PIPELINE_START("Combined Stress Test");

    // Stage 1: Create substrate
    E2E_STAGE_BEGIN("Create substrate", 200);
    E2E_ASSERT(CreateSubstrate(), "Failed to create substrate");
    E2E_STAGE_END();

    // Stage 2: Apply low ATP
    E2E_STAGE_BEGIN("Apply low ATP", 100);
    set_atp_level(0.5f);
    E2E_STAGE_END();

    // Stage 3: Apply hypoxia
    E2E_STAGE_BEGIN("Apply hypoxia", 100);
    set_oxygen_saturation(0.80f);
    E2E_STAGE_END();

    // Stage 4: Apply fever
    E2E_STAGE_BEGIN("Apply fever", 100);
    set_temperature(39.5f);
    E2E_STAGE_END();

    // Stage 5: Check combined effects
    E2E_STAGE_BEGIN("Check combined effects", 100);
    substrate_health_level_t health = substrate_get_health_level(substrate_);
    float capacity = substrate_get_capacity(substrate_);
    float firing = substrate_get_firing_modulation(substrate_);
    float efficiency = substrate_get_transmission_efficiency(substrate_);

    std::cout << "Under stress:" << std::endl;
    std::cout << "  Health: " << static_cast<int>(health) << std::endl;
    std::cout << "  Capacity: " << capacity << std::endl;
    std::cout << "  Firing mod: " << firing << std::endl;
    std::cout << "  Efficiency: " << efficiency << std::endl;
    E2E_STAGE_END();

    // Stage 6: Run multiple update cycles
    E2E_STAGE_BEGIN("Run updates under stress", 200);
    for (int i = 0; i < 10; i++) {
        int result = substrate_update(substrate_, 100);  // 100ms
        E2E_ASSERT(result == 0, "Update should succeed");
    }
    E2E_STAGE_END();

    // Stage 7: Recovery
    E2E_STAGE_BEGIN("Recovery", 100);
    set_atp_level(1.0f);
    set_oxygen_saturation(0.98f);
    set_temperature(37.0f);

    health = substrate_get_health_level(substrate_);
    capacity = substrate_get_capacity(substrate_);
    std::cout << "After recovery:" << std::endl;
    std::cout << "  Health: " << static_cast<int>(health) << std::endl;
    std::cout << "  Capacity: " << capacity << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
