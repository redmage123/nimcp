/**
 * @file test_multi_objective.cpp
 * @brief Unit tests for Phase C4.6 multi-objective adaptation
 *
 * WHAT: Unit tests for Pareto-optimal source selection
 * WHY:  Ensure 100% code coverage and correct behavior
 * HOW:  Test scoring, dominance, Pareto front, selection
 *
 * TEST COVERAGE:
 * - Multi-objective scoring
 * - Pareto dominance checking
 * - Pareto front computation
 * - Source selection from front
 * - Weighted scalarization
 * - Edge cases and error handling
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

class MultiObjectiveTest : public ::testing::Test {
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
        config.enable_quantum_walk = true;
        config.enable_adaptive_routing = true;
        config.enable_multi_objective = true;  // Enable Phase C4.6
        config.num_objectives = 2;
        config.objective_weights[0] = 0.5f;
        config.objective_weights[1] = 0.5f;
        config.pareto_epsilon = 0.01f;
        config.prefer_diversity = true;
        config.num_adaptive_sources = 5;

        // Create field
        field = spatial_neuromod_create(num_neurons, &config);
        ASSERT_NE(field, nullptr);

        // Enable quantum-Shannon (required)
        field->use_quantum_shannon = true;
        quantum_shannon_config_t qs_config = quantum_shannon_default_config();
        field->quantum_shannon_diffusion = quantum_shannon_create(
            network, num_neurons / 2, 10.0f, &qs_config);
        ASSERT_NE(field->quantum_shannon_diffusion, nullptr);

        // Set Shannon metrics
        field->last_propagation_efficiency = 0.8f;
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

TEST_F(MultiObjectiveTest, DefaultConfig_DisabledByDefault) {
    // WHAT: Test that multi-objective is disabled by default
    // WHY:  Opt-in behavior ensures backward compatibility
    // HOW:  Check default config

    spatial_neuromod_config_t default_config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    EXPECT_FALSE(default_config.enable_multi_objective);
}

TEST_F(MultiObjectiveTest, DefaultConfig_ValidDefaults) {
    // WHAT: Test that Phase C4.6 defaults are valid
    // WHY:  Ensure good defaults when user enables multi-objective
    // HOW:  Check all config fields

    spatial_neuromod_config_t default_config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

    EXPECT_EQ(default_config.num_objectives, 2u);
    EXPECT_FLOAT_EQ(default_config.objective_weights[0], 0.5f);
    EXPECT_FLOAT_EQ(default_config.objective_weights[1], 0.5f);
    EXPECT_GT(default_config.pareto_epsilon, 0.0f);
    EXPECT_TRUE(default_config.prefer_diversity);
}

//=============================================================================
// Test: Multi-Objective Scoring
//=============================================================================

TEST_F(MultiObjectiveTest, ScoreNeuron_ComputesAllObjectives) {
    // WHAT: Test that all objectives are computed correctly
    // WHY:  Each objective should reflect a Shannon metric
    // HOW:  Call scoring function, verify all scores valid

    float scores[4];
    bool success = spatial_neuromod_score_neuron_multi_objective(
        field, 50, network, &config, scores);

    ASSERT_TRUE(success);

    // All scores should be in [0, 1]
    for (uint32_t i = 0; i < config.num_objectives; i++) {
        EXPECT_GE(scores[i], 0.0f) << "Objective " << i;
        EXPECT_LE(scores[i], 1.0f) << "Objective " << i;
    }
}

TEST_F(MultiObjectiveTest, ScoreNeuron_ReflectsShannonMetrics) {
    // WHAT: Test that scores reflect Shannon metrics
    // WHY:  Scores should map to efficiency, speedup, etc.
    // HOW:  Set known metrics, verify scores

    field->last_propagation_efficiency = 0.9f;
    field->last_speedup_vs_classical = 25.0f;
    field->last_num_bottlenecks = 0;
    field->last_information_rate = 5.0f;

    float scores[4];
    spatial_neuromod_score_neuron_multi_objective(field, 50, network, &config, scores);

    // Objective 0: Efficiency (should be 0.9)
    EXPECT_NEAR(scores[0], 0.9f, 0.01f);

    // Objective 1: Speedup (normalized, should be 25/50 = 0.5)
    EXPECT_NEAR(scores[1], 0.5f, 0.01f);

    // Objective 2: Bottleneck avoidance (no bottlenecks = 1.0)
    EXPECT_NEAR(scores[2], 1.0f, 0.01f);

    // Objective 3: Info rate (normalized, 5/10 = 0.5)
    EXPECT_NEAR(scores[3], 0.5f, 0.01f);
}

TEST_F(MultiObjectiveTest, ScoreNeuron_DisabledFeature_ReturnsFalse) {
    // WHAT: Test that scoring fails when multi-objective disabled
    // WHY:  Feature is opt-in
    // HOW:  Disable, call, expect false

    config.enable_multi_objective = false;
    float scores[4];
    bool success = spatial_neuromod_score_neuron_multi_objective(
        field, 50, network, &config, scores);

    EXPECT_FALSE(success);
}

TEST_F(MultiObjectiveTest, ScoreNeuron_NoQuantumShannon_ReturnsFalse) {
    // WHAT: Test that scoring fails without quantum-Shannon
    // WHY:  Requires Shannon metrics
    // HOW:  Disable quantum-Shannon, expect failure

    field->use_quantum_shannon = false;
    float scores[4];
    bool success = spatial_neuromod_score_neuron_multi_objective(
        field, 50, network, &config, scores);

    EXPECT_FALSE(success);
}

TEST_F(MultiObjectiveTest, ScoreNeuron_InvalidParameters_ReturnsFalse) {
    // WHAT: Test that scoring handles NULL parameters
    // WHY:  Defensive programming
    // HOW:  Pass NULL, expect false

    float scores[4];

    EXPECT_FALSE(spatial_neuromod_score_neuron_multi_objective(
        nullptr, 50, network, &config, scores));
    EXPECT_FALSE(spatial_neuromod_score_neuron_multi_objective(
        field, 50, network, nullptr, scores));
    EXPECT_FALSE(spatial_neuromod_score_neuron_multi_objective(
        field, 50, network, &config, nullptr));
}

//=============================================================================
// Test: Pareto Dominance
//=============================================================================

TEST_F(MultiObjectiveTest, ParetoDominates_BetterOnAll_ReturnsTrue) {
    // WHAT: Test that A dominates B when better on all objectives
    // WHY:  Core dominance check
    // HOW:  A better on all, expect true

    float scores_a[4] = {0.9f, 0.8f, 0.0f, 0.0f};
    float scores_b[4] = {0.7f, 0.6f, 0.0f, 0.0f};

    bool dominates = spatial_neuromod_pareto_dominates(
        scores_a, scores_b, 2, 0.01f);

    EXPECT_TRUE(dominates);
}

TEST_F(MultiObjectiveTest, ParetoDominates_BetterOnSome_ReturnsTrue) {
    // WHAT: Test that A dominates B when better on some, equal on rest
    // WHY:  Dominance allows equality
    // HOW:  A better on one, equal on another

    float scores_a[4] = {0.9f, 0.7f, 0.0f, 0.0f};
    float scores_b[4] = {0.9f, 0.6f, 0.0f, 0.0f};

    bool dominates = spatial_neuromod_pareto_dominates(
        scores_a, scores_b, 2, 0.01f);

    EXPECT_TRUE(dominates);
}

TEST_F(MultiObjectiveTest, ParetoDominates_WorseOnOne_ReturnsFalse) {
    // WHAT: Test that A doesn't dominate if worse on any objective
    // WHY:  Dominance requires >= on all
    // HOW:  A better on one, worse on another

    float scores_a[4] = {0.9f, 0.5f, 0.0f, 0.0f};
    float scores_b[4] = {0.7f, 0.8f, 0.0f, 0.0f};

    bool dominates = spatial_neuromod_pareto_dominates(
        scores_a, scores_b, 2, 0.01f);

    EXPECT_FALSE(dominates);  // Trade-off: A not dominating
}

TEST_F(MultiObjectiveTest, ParetoDominates_EqualOnAll_ReturnsFalse) {
    // WHAT: Test that equal solutions don't dominate each other
    // WHY:  Dominance requires strict improvement on at least one
    // HOW:  A equal to B on all objectives

    float scores_a[4] = {0.8f, 0.7f, 0.0f, 0.0f};
    float scores_b[4] = {0.8f, 0.7f, 0.0f, 0.0f};

    bool dominates = spatial_neuromod_pareto_dominates(
        scores_a, scores_b, 2, 0.01f);

    EXPECT_FALSE(dominates);
}

TEST_F(MultiObjectiveTest, ParetoDominates_EpsilonTolerance_Works) {
    // WHAT: Test that epsilon tolerance works
    // WHY:  Floating-point comparisons need epsilon
    // HOW:  Values within epsilon should be considered equal

    float scores_a[4] = {0.800f, 0.700f, 0.0f, 0.0f};
    float scores_b[4] = {0.799f, 0.701f, 0.0f, 0.0f};

    // With epsilon=0.01, these should be considered equal
    bool dominates = spatial_neuromod_pareto_dominates(
        scores_a, scores_b, 2, 0.01f);

    EXPECT_FALSE(dominates);
}

//=============================================================================
// Test: Pareto Optimal Selection
//=============================================================================

TEST_F(MultiObjectiveTest, SelectParetoOptimal_FindsFront) {
    // WHAT: Test that Pareto front is found
    // WHY:  Core selection functionality
    // HOW:  Call selection, verify some neurons selected

    uint32_t selected_ids[100];
    uint32_t num_selected;

    bool success = spatial_neuromod_select_pareto_optimal(
        field, network, &config, selected_ids, nullptr, &num_selected);

    ASSERT_TRUE(success);
    EXPECT_GT(num_selected, 0u);
    EXPECT_LE(num_selected, field->current_adaptive_sources);
}

TEST_F(MultiObjectiveTest, SelectParetoOptimal_RespectsK) {
    // WHAT: Test that selection respects K limit
    // WHY:  Should select at most K neurons
    // HOW:  Set small K, verify count

    field->current_adaptive_sources = 3;

    uint32_t selected_ids[100];
    uint32_t num_selected;

    bool success = spatial_neuromod_select_pareto_optimal(
        field, network, &config, selected_ids, nullptr, &num_selected);

    ASSERT_TRUE(success);
    EXPECT_LE(num_selected, 3u);
}

TEST_F(MultiObjectiveTest, SelectParetoOptimal_DisabledFeature_ReturnsFalse) {
    // WHAT: Test that selection fails when disabled
    // WHY:  Feature is opt-in
    // HOW:  Disable, expect failure

    config.enable_multi_objective = false;

    uint32_t selected_ids[100];
    uint32_t num_selected;

    bool success = spatial_neuromod_select_pareto_optimal(
        field, network, &config, selected_ids, nullptr, &num_selected);

    EXPECT_FALSE(success);
}

TEST_F(MultiObjectiveTest, SelectParetoOptimal_InvalidParameters_ReturnsFalse) {
    // WHAT: Test NULL parameter handling
    // WHY:  Defensive programming
    // HOW:  Pass NULL, expect false

    uint32_t selected_ids[100];
    uint32_t num_selected;

    EXPECT_FALSE(spatial_neuromod_select_pareto_optimal(
        nullptr, network, &config, selected_ids, nullptr, &num_selected));
    EXPECT_FALSE(spatial_neuromod_select_pareto_optimal(
        field, network, nullptr, selected_ids, nullptr, &num_selected));
    EXPECT_FALSE(spatial_neuromod_select_pareto_optimal(
        field, network, &config, nullptr, nullptr, &num_selected));
    EXPECT_FALSE(spatial_neuromod_select_pareto_optimal(
        field, network, &config, selected_ids, nullptr, nullptr));
}

//=============================================================================
// Test: Multi-Objective Release
//=============================================================================

TEST_F(MultiObjectiveTest, ReleaseMultiObjective_DistributesNeurotransmitter) {
    // WHAT: Test that release distributes neuromodulator
    // WHY:  Core functionality
    // HOW:  Release, verify source_rate updated

    float initial_total = field->total_released;

    bool success = spatial_neuromod_release_multi_objective(
        field, network, &config, 10.0f);

    ASSERT_TRUE(success);
    EXPECT_GT(field->total_released, initial_total);
}

TEST_F(MultiObjectiveTest, ReleaseMultiObjective_DisabledFeature_ReturnsFalse) {
    // WHAT: Test that release fails when disabled
    // WHY:  Feature is opt-in
    // HOW:  Disable, expect failure

    config.enable_multi_objective = false;

    bool success = spatial_neuromod_release_multi_objective(
        field, network, &config, 10.0f);

    EXPECT_FALSE(success);
}

TEST_F(MultiObjectiveTest, ReleaseMultiObjective_InvalidParameters_ReturnsFalse) {
    // WHAT: Test NULL parameter handling
    // WHY:  Defensive programming
    // HOW:  Pass NULL, expect false

    EXPECT_FALSE(spatial_neuromod_release_multi_objective(
        nullptr, network, &config, 10.0f));
    EXPECT_FALSE(spatial_neuromod_release_multi_objective(
        field, nullptr, &config, 10.0f));
    EXPECT_FALSE(spatial_neuromod_release_multi_objective(
        field, network, nullptr, 10.0f));
}

//=============================================================================
// Test: Integration with Phase C4.4
//=============================================================================

TEST_F(MultiObjectiveTest, Integration_WorksWithAdaptiveRouting) {
    // WHAT: Test that multi-objective works alongside Phase C4.4
    // WHY:  Phases should coexist
    // HOW:  Enable both, verify both work

    config.enable_adaptive_routing = true;
    config.enable_multi_objective = true;

    // Phase C4.4: Regular adaptive selection
    uint32_t selected_c44[100];
    uint32_t num_selected_c44;
    bool success_c44 = spatial_neuromod_select_optimal_sources(
        field, network, &config, selected_c44, nullptr, &num_selected_c44);

    // Phase C4.6: Multi-objective selection
    uint32_t selected_c46[100];
    uint32_t num_selected_c46;
    bool success_c46 = spatial_neuromod_select_pareto_optimal(
        field, network, &config, selected_c46, nullptr, &num_selected_c46);

    EXPECT_TRUE(success_c44);
    EXPECT_TRUE(success_c46);
    EXPECT_GT(num_selected_c44, 0u);
    EXPECT_GT(num_selected_c46, 0u);
}

//=============================================================================
// Test: Multiple Objectives
//=============================================================================

TEST_F(MultiObjectiveTest, MultipleObjectives_2Objectives_Works) {
    // WHAT: Test with 2 objectives
    // WHY:  Verify correct behavior
    // HOW:  Set 2 objectives, verify selection

    config.num_objectives = 2;

    uint32_t selected_ids[100];
    uint32_t num_selected;

    bool success = spatial_neuromod_select_pareto_optimal(
        field, network, &config, selected_ids, nullptr, &num_selected);

    EXPECT_TRUE(success);
}

TEST_F(MultiObjectiveTest, MultipleObjectives_3Objectives_Works) {
    // WHAT: Test with 3 objectives
    // WHY:  Verify scalability
    // HOW:  Set 3 objectives, verify selection

    config.num_objectives = 3;
    config.objective_weights[2] = 0.33f;

    uint32_t selected_ids[100];
    uint32_t num_selected;

    bool success = spatial_neuromod_select_pareto_optimal(
        field, network, &config, selected_ids, nullptr, &num_selected);

    EXPECT_TRUE(success);
}

TEST_F(MultiObjectiveTest, MultipleObjectives_4Objectives_Works) {
    // WHAT: Test with 4 objectives (maximum)
    // WHY:  Verify upper limit
    // HOW:  Set 4 objectives, verify selection

    config.num_objectives = 4;
    config.objective_weights[2] = 0.25f;
    config.objective_weights[3] = 0.25f;

    uint32_t selected_ids[100];
    uint32_t num_selected;

    bool success = spatial_neuromod_select_pareto_optimal(
        field, network, &config, selected_ids, nullptr, &num_selected);

    EXPECT_TRUE(success);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
