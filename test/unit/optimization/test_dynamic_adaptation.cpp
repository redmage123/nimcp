/**
 * @file test_dynamic_adaptation.cpp
 * @brief Unit tests for Phase C4.5 dynamic source adaptation
 *
 * WHAT: Unit tests for dynamic K adaptation based on efficiency EMA
 * WHY:  Ensure 100% code coverage and correct behavior
 * HOW:  Test EMA updates, K adaptation logic, cooldown, edge cases
 *
 * TEST COVERAGE:
 * - EMA calculation (exponential moving average)
 * - K increase when efficiency low
 * - K decrease when efficiency high
 * - K clamping to [min_K, max_K]
 * - Cooldown mechanism
 * - Graceful fallback when requirements not met
 * - Query API for current K
 *
 * @author NIMCP Development Team
 * @date 2025-11-14
 */

#include <gtest/gtest.h>
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/quantum/nimcp_quantum_shannon.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DynamicAdaptationTest : public ::testing::Test {
protected:
    neural_network_t network;
    spatial_neuromod_field_t* field;
    spatial_neuromod_config_t config;
    uint32_t num_neurons;

    void SetUp() override {
        num_neurons = 100;

        // Create network
        network_config_t net_config = {};
        net_config.num_neurons = num_neurons;
        net_config.ei_ratio = 0.8f;
        net_config.learning_rate = 0.01f;
        net_config.stdp_window = 20.0f;
        net_config.refractory_period = 2.0f;
        net_config.min_weight = 0.0f;
        net_config.max_weight = 1.0f;
        net_config.input_size = num_neurons;
        net_config.output_size = num_neurons;

        network = neural_network_create(&net_config);
        ASSERT_NE(network, nullptr);

        // Create config with all Phase C4.x features enabled
        config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
        config.enable_quantum_walk = true;          // Required for quantum-Shannon
        config.enable_adaptive_routing = true;      // Required for Phase C4.4
        config.enable_dynamic_adaptation = true;    // Enable Phase C4.5
        config.num_adaptive_sources = 5;            // Initial K
        config.min_adaptive_sources = 1;            // Min K
        config.max_adaptive_sources = 10;           // Max K
        config.adaptation_rate = 0.2f;              // 20% new, 80% old for faster response in tests
        config.target_efficiency = 0.75f;           // Target
        config.efficiency_tolerance = 0.1f;         // Tolerance ±10%
        config.adaptation_cooldown_steps = 10;      // Short cooldown for testing

        // Create field
        field = spatial_neuromod_create(num_neurons, &config);
        ASSERT_NE(field, nullptr);

        // Enable quantum-Shannon (required for Phase C4.5)
        field->use_quantum_shannon = true;
        quantum_shannon_config_t qs_config = quantum_shannon_default_config();
        field->quantum_shannon_diffusion = quantum_shannon_create(
            network, num_neurons / 2, 10.0f, &qs_config);
        ASSERT_NE(field->quantum_shannon_diffusion, nullptr);

        // Set initial Shannon metrics (mid-range efficiency)
        field->last_propagation_efficiency = 0.75f;  // At target
        field->last_speedup_vs_classical = 15.0f;
        field->last_num_bottlenecks = 0;
        field->last_information_rate = 2.0f;
    }

    void TearDown() override {
        if (field) {
            spatial_neuromod_destroy(field);
        }
        if (network) {
            neural_network_destroy(network);
        }
    }
};

//=============================================================================
// Test: Configuration Defaults
//=============================================================================

TEST_F(DynamicAdaptationTest, DefaultConfig_DisabledByDefault) {
    // WHAT: Test that dynamic adaptation is disabled by default
    // WHY:  Opt-in behavior ensures backward compatibility
    // HOW:  Check default config

    spatial_neuromod_config_t default_config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    EXPECT_FALSE(default_config.enable_dynamic_adaptation);
}

TEST_F(DynamicAdaptationTest, DefaultConfig_ValidDefaults) {
    // WHAT: Test that Phase C4.5 defaults are valid
    // WHY:  Ensure good defaults when user enables dynamic adaptation
    // HOW:  Check all config fields

    spatial_neuromod_config_t default_config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

    EXPECT_GE(default_config.min_adaptive_sources, 1u);
    EXPECT_LE(default_config.max_adaptive_sources, 100u);
    EXPECT_GT(default_config.min_adaptive_sources, 0u);
    EXPECT_GE(default_config.adaptation_rate, 0.0f);
    EXPECT_LE(default_config.adaptation_rate, 1.0f);
    EXPECT_GE(default_config.target_efficiency, 0.0f);
    EXPECT_LE(default_config.target_efficiency, 1.0f);
    EXPECT_GE(default_config.efficiency_tolerance, 0.0f);
    EXPECT_GT(default_config.adaptation_cooldown_steps, 0u);
}

//=============================================================================
// Test: Initialization
//=============================================================================

TEST_F(DynamicAdaptationTest, Create_InitializesStateCorrectly) {
    // WHAT: Test that field initialization is correct
    // WHY:  State must start in valid condition
    // HOW:  Check initial values

    EXPECT_FLOAT_EQ(field->efficiency_ema, 0.0f);  // No history yet
    EXPECT_EQ(field->current_adaptive_sources, config.num_adaptive_sources);  // Starts at config value
    EXPECT_EQ(field->adaptation_cooldown, 0u);  // No cooldown initially
}

//=============================================================================
// Test: EMA Updates
//=============================================================================

TEST_F(DynamicAdaptationTest, UpdateDynamicAdaptation_UpdatesEMA) {
    // WHAT: Test that efficiency EMA is updated correctly
    // WHY:  EMA tracks efficiency over time
    // HOW:  Call update, check EMA calculation

    field->last_propagation_efficiency = 0.8f;
    bool success = spatial_neuromod_update_dynamic_adaptation(field, &config);
    ASSERT_TRUE(success);

    // EMA = alpha * current + (1-alpha) * previous
    // EMA = 0.2 * 0.8 + 0.8 * 0.0 = 0.16
    EXPECT_FLOAT_EQ(field->efficiency_ema, 0.16f);

    // Second update
    field->last_propagation_efficiency = 0.9f;
    success = spatial_neuromod_update_dynamic_adaptation(field, &config);
    ASSERT_TRUE(success);

    // EMA = 0.2 * 0.9 + 0.8 * 0.16 = 0.18 + 0.128 = 0.308
    EXPECT_NEAR(field->efficiency_ema, 0.308f, 0.001f);
}

//=============================================================================
// Test: K Adaptation - Increase
//=============================================================================

TEST_F(DynamicAdaptationTest, UpdateDynamicAdaptation_LowEfficiency_IncreasesK) {
    // WHAT: Test that K increases when efficiency below target
    // WHY:  Low efficiency needs more source diversity
    // HOW:  Set low efficiency, update multiple times to exceed tolerance

    uint32_t initial_K = field->current_adaptive_sources;

    // Set efficiency below target - tolerance (0.75 - 0.1 = 0.65)
    field->last_propagation_efficiency = 0.6f;

    // Update multiple times to build up EMA below threshold
    for (int i = 0; i < 20; i++) {
        spatial_neuromod_update_dynamic_adaptation(field, &config);
    }

    // K should have increased
    EXPECT_GT(field->current_adaptive_sources, initial_K);
}

TEST_F(DynamicAdaptationTest, UpdateDynamicAdaptation_IncreasesK_ClampsToMax) {
    // WHAT: Test that K doesn't exceed max_adaptive_sources
    // WHY:  Prevent excessive source selection
    // HOW:  Start near max, set low efficiency, verify clamping

    field->current_adaptive_sources = config.max_adaptive_sources - 1;
    field->adaptation_cooldown = 0;  // No cooldown

    // Set very low efficiency
    field->last_propagation_efficiency = 0.4f;
    field->efficiency_ema = 0.4f;  // Set EMA directly to skip buildup

    // Update (should increase K by 1)
    bool success = spatial_neuromod_update_dynamic_adaptation(field, &config);
    ASSERT_TRUE(success);
    EXPECT_EQ(field->current_adaptive_sources, config.max_adaptive_sources);

    // Update again (should stay at max)
    field->adaptation_cooldown = 0;  // Reset cooldown
    success = spatial_neuromod_update_dynamic_adaptation(field, &config);
    ASSERT_TRUE(success);
    EXPECT_EQ(field->current_adaptive_sources, config.max_adaptive_sources);
}

//=============================================================================
// Test: K Adaptation - Decrease
//=============================================================================

TEST_F(DynamicAdaptationTest, UpdateDynamicAdaptation_HighEfficiency_DecreasesK) {
    // WHAT: Test that K decreases when efficiency above target
    // WHY:  High efficiency means fewer sources needed
    // HOW:  Set high efficiency, update multiple times

    // Start with K at 8 so we have room to observe decrease
    field->current_adaptive_sources = 8;
    field->adaptation_cooldown = 0;

    // Set efficiency above target + tolerance (0.75 + 0.1 = 0.85)
    field->last_propagation_efficiency = 0.9f;

    // Update multiple times to build up EMA above threshold
    for (int i = 0; i < 20; i++) {
        field->adaptation_cooldown = 0;  // Skip cooldown for faster testing
        field->last_propagation_efficiency = 0.9f;  // Keep high
        spatial_neuromod_update_dynamic_adaptation(field, &config);
    }

    // K should have decreased from initial 8
    EXPECT_LT(field->current_adaptive_sources, 8u);
}

TEST_F(DynamicAdaptationTest, UpdateDynamicAdaptation_DecreasesK_ClampsToMin) {
    // WHAT: Test that K doesn't go below min_adaptive_sources
    // WHY:  Need at least min sources
    // HOW:  Start near min, set high efficiency, verify clamping

    field->current_adaptive_sources = config.min_adaptive_sources + 1;
    field->adaptation_cooldown = 0;  // No cooldown

    // Set very high efficiency
    field->last_propagation_efficiency = 0.95f;
    field->efficiency_ema = 0.95f;  // Set EMA directly

    // Update (should decrease K by 1)
    bool success = spatial_neuromod_update_dynamic_adaptation(field, &config);
    ASSERT_TRUE(success);
    EXPECT_EQ(field->current_adaptive_sources, config.min_adaptive_sources);

    // Update again (should stay at min)
    field->adaptation_cooldown = 0;  // Reset cooldown
    success = spatial_neuromod_update_dynamic_adaptation(field, &config);
    ASSERT_TRUE(success);
    EXPECT_EQ(field->current_adaptive_sources, config.min_adaptive_sources);
}

//=============================================================================
// Test: K Adaptation - Within Tolerance
//=============================================================================

TEST_F(DynamicAdaptationTest, UpdateDynamicAdaptation_WithinTolerance_NoChange) {
    // WHAT: Test that K doesn't change when efficiency within tolerance band
    // WHY:  Avoid oscillations around target
    // HOW:  Set efficiency near target, update, check K unchanged

    uint32_t initial_K = field->current_adaptive_sources;
    field->adaptation_cooldown = 0;  // No cooldown

    // Set efficiency within tolerance (0.75 ± 0.1)
    field->last_propagation_efficiency = 0.76f;
    field->efficiency_ema = 0.76f;

    bool success = spatial_neuromod_update_dynamic_adaptation(field, &config);
    ASSERT_TRUE(success);

    // K should not change
    EXPECT_EQ(field->current_adaptive_sources, initial_K);
}

//=============================================================================
// Test: Cooldown Mechanism
//=============================================================================

TEST_F(DynamicAdaptationTest, UpdateDynamicAdaptation_Cooldown_PreventsAdaptation) {
    // WHAT: Test that cooldown prevents rapid adaptations
    // WHY:  Rate limiting for stability
    // HOW:  Adapt, check cooldown set, verify no adaptation during cooldown

    field->adaptation_cooldown = 0;
    field->efficiency_ema = 0.6f;  // Below target
    field->last_propagation_efficiency = 0.6f;

    uint32_t initial_K = field->current_adaptive_sources;

    // First update should adapt (cooldown=0)
    bool success = spatial_neuromod_update_dynamic_adaptation(field, &config);
    ASSERT_TRUE(success);
    EXPECT_GT(field->current_adaptive_sources, initial_K);  // K increased
    EXPECT_EQ(field->adaptation_cooldown, config.adaptation_cooldown_steps);  // Cooldown set

    // Second update should not adapt (cooldown>0)
    uint32_t K_after_adapt = field->current_adaptive_sources;
    success = spatial_neuromod_update_dynamic_adaptation(field, &config);
    ASSERT_TRUE(success);
    EXPECT_EQ(field->current_adaptive_sources, K_after_adapt);  // K unchanged
    EXPECT_EQ(field->adaptation_cooldown, config.adaptation_cooldown_steps - 1);  // Cooldown decremented
}

TEST_F(DynamicAdaptationTest, UpdateDynamicAdaptation_Cooldown_Decrements) {
    // WHAT: Test that cooldown decrements each update
    // WHY:  Eventually allows adaptation again
    // HOW:  Set cooldown, update multiple times, check decrement

    field->adaptation_cooldown = 5;

    for (uint32_t i = 5; i > 0; i--) {
        EXPECT_EQ(field->adaptation_cooldown, i);
        spatial_neuromod_update_dynamic_adaptation(field, &config);
    }

    EXPECT_EQ(field->adaptation_cooldown, 0u);
}

//=============================================================================
// Test: Requirements Checking
//=============================================================================

TEST_F(DynamicAdaptationTest, UpdateDynamicAdaptation_Disabled_ReturnsFalse) {
    // WHAT: Test that update fails when dynamic adaptation disabled
    // WHY:  Feature is opt-in
    // HOW:  Disable in config, call update

    config.enable_dynamic_adaptation = false;
    bool success = spatial_neuromod_update_dynamic_adaptation(field, &config);
    EXPECT_FALSE(success);
}

TEST_F(DynamicAdaptationTest, UpdateDynamicAdaptation_NoQuantumShannon_ReturnsFalse) {
    // WHAT: Test that update fails without quantum-Shannon
    // WHY:  Requires efficiency metrics from quantum-Shannon
    // HOW:  Disable quantum-Shannon, call update

    field->use_quantum_shannon = false;
    bool success = spatial_neuromod_update_dynamic_adaptation(field, &config);
    EXPECT_FALSE(success);
}

TEST_F(DynamicAdaptationTest, UpdateDynamicAdaptation_NoAdaptiveRouting_ReturnsFalse) {
    // WHAT: Test that update fails without adaptive routing enabled
    // WHY:  Dynamic adaptation builds on Phase C4.4
    // HOW:  Disable adaptive routing, call update

    config.enable_adaptive_routing = false;
    bool success = spatial_neuromod_update_dynamic_adaptation(field, &config);
    EXPECT_FALSE(success);
}

TEST_F(DynamicAdaptationTest, UpdateDynamicAdaptation_NullPointers_ReturnsFalse) {
    // WHAT: Test that update handles NULL pointers gracefully
    // WHY:  Defensive programming
    // HOW:  Pass NULL, check return

    EXPECT_FALSE(spatial_neuromod_update_dynamic_adaptation(nullptr, &config));
    EXPECT_FALSE(spatial_neuromod_update_dynamic_adaptation(field, nullptr));
}

//=============================================================================
// Test: Query API
//=============================================================================

TEST_F(DynamicAdaptationTest, GetCurrentAdaptiveSources_ReturnsCorrectValue) {
    // WHAT: Test that query API returns current K
    // WHY:  Users need to know current K value
    // HOW:  Get K, compare to field value

    uint32_t K = spatial_neuromod_get_current_adaptive_sources(field);
    EXPECT_EQ(K, field->current_adaptive_sources);
}

TEST_F(DynamicAdaptationTest, GetCurrentAdaptiveSources_AfterAdaptation_ReturnsNewValue) {
    // WHAT: Test that query reflects K changes
    // WHY:  K changes over time with dynamic adaptation
    // HOW:  Adapt K, query, check new value

    uint32_t initial_K = spatial_neuromod_get_current_adaptive_sources(field);

    // Force adaptation (increase K)
    field->adaptation_cooldown = 0;
    field->efficiency_ema = 0.6f;  // Below target
    field->last_propagation_efficiency = 0.6f;
    spatial_neuromod_update_dynamic_adaptation(field, &config);

    uint32_t new_K = spatial_neuromod_get_current_adaptive_sources(field);
    EXPECT_GT(new_K, initial_K);
}

TEST_F(DynamicAdaptationTest, GetCurrentAdaptiveSources_NullPointer_ReturnsZero) {
    // WHAT: Test that query handles NULL gracefully
    // WHY:  Defensive programming
    // HOW:  Pass NULL, check return

    uint32_t K = spatial_neuromod_get_current_adaptive_sources(nullptr);
    EXPECT_EQ(K, 0u);
}

//=============================================================================
// Test: Integration with Phase C4.4
//=============================================================================

TEST_F(DynamicAdaptationTest, SelectOptimalSources_UsesDynamicK) {
    // WHAT: Test that source selection uses current_adaptive_sources
    // WHY:  Integration between Phase C4.4 and C4.5
    // HOW:  Change K dynamically, verify selection uses new K

    // Initially K=5
    EXPECT_EQ(field->current_adaptive_sources, 5u);

    // Select sources (should select 5)
    uint32_t selected_ids[100];
    float selected_scores[100];
    uint32_t num_selected;

    bool success = spatial_neuromod_select_optimal_sources(
        field, network, &config, selected_ids, selected_scores, &num_selected);
    ASSERT_TRUE(success);
    EXPECT_EQ(num_selected, 5u);

    // Adapt K upward
    field->adaptation_cooldown = 0;
    field->efficiency_ema = 0.6f;  // Low efficiency
    field->last_propagation_efficiency = 0.6f;
    spatial_neuromod_update_dynamic_adaptation(field, &config);

    EXPECT_EQ(field->current_adaptive_sources, 6u);  // K increased

    // Select sources again (should select 6)
    success = spatial_neuromod_select_optimal_sources(
        field, network, &config, selected_ids, selected_scores, &num_selected);
    ASSERT_TRUE(success);
    EXPECT_EQ(num_selected, 6u);  // Uses new K
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
