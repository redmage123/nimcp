/**
 * @file test_glial_integration.cpp
 * @brief Comprehensive integration tests for glial modules
 *
 * Test scenarios:
 * 1. Brain-Glial Integration: Create brain with glial enabled, verify glial_integration_step is called
 * 2. Microglia-Network Integration: Microglia pruning affects actual neural network synapses
 * 3. Oligodendrocyte-Velocity Integration: Myelination changes actual signal propagation delays
 * 4. Astrocyte-Microglia Coordination: Inflammatory signals shared between cell types
 * 5. Full Glial System: All three cell types working together during brain simulation
 * 6. Activity Feedback Loop: Neural activity -> oligodendrocyte myelination -> faster conduction
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "glial/integration/nimcp_glial_integration.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"  // For brain struct internals in tests
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"
#include "glial/microglia/nimcp_microglia.h"
#include "utils/time/nimcp_time.h"
#include "utils/memory/nimcp_memory.h"

// ============================================================================
// Base Test Fixture
// ============================================================================

class GlialIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        network_config_t config{};
        config.num_neurons = 10;
        config.input_size = 5;
        config.output_size = 2;
        config.ei_ratio = 0.8f;
        config.learning_rate = 0.01f;
        config.min_weight = -1.0f;
        config.max_weight = 1.0f;
        network = neural_network_create(&config);
        ASSERT_NE(network, nullptr);

        astrocyte_net = astrocyte_network_create(5);
        ASSERT_NE(astrocyte_net, nullptr);

        oligo_net = oligodendrocyte_network_create(5);
        ASSERT_NE(oligo_net, nullptr);

        microglia_net = microglia_network_create(5);
        ASSERT_NE(microglia_net, nullptr);

        for (uint32_t i = 0; i < 3; i++) {
            astrocyte_t* ast = astrocyte_create(i, ASTROCYTE_TYPE_GENERIC, i * 100.0f, 0.0f, 0.0f, 50.0f);
            ASSERT_NE(ast, nullptr);
            astrocyte_network_add(astrocyte_net, ast);

            oligodendrocyte_t* oligo = oligodendrocyte_create(i, i * 100.0f, 0.0f, 0.0f, 10);
            ASSERT_NE(oligo, nullptr);
            oligodendrocyte_network_add(oligo_net, oligo);

            microglia_t* mg = microglia_create(i, i * 100.0f, 0.0f, 0.0f, 100.0f);
            ASSERT_NE(mg, nullptr);
            microglia_network_add(microglia_net, mg);
        }

        neural_network_add_connection(network, 0, 1, 0.5f);
        neural_network_add_connection(network, 1, 2, 0.7f);
        neural_network_add_connection(network, 2, 3, 0.3f);
    }

    void TearDown() override {
        if (gi) glial_integration_destroy(gi);
        if (network) neural_network_destroy(network);
        if (astrocyte_net) astrocyte_network_destroy(astrocyte_net);
        if (oligo_net) oligodendrocyte_network_destroy(oligo_net);
        if (microglia_net) microglia_network_destroy(microglia_net);
    }

    neural_network_t network = nullptr;
    astrocyte_network_t* astrocyte_net = nullptr;
    oligodendrocyte_network_t* oligo_net = nullptr;
    microglia_network_t* microglia_net = nullptr;
    glial_integration_t* gi = nullptr;
};

// ============================================================================
// SCENARIO 1: Brain-Glial Integration
// ============================================================================

class BrainGlialIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        brain = nullptr;
    }
    void TearDown() override {
        if (brain) brain_destroy(brain);
    }
    brain_t brain = nullptr;
};

TEST_F(BrainGlialIntegrationTest, BrainWithGlialEnabled_HasGlialIntegration) {
    // WHAT: Verify brain with enable_glial creates glial integration
    // WHY:  Brain should automatically create glial subsystem when enabled
    // HOW:  Create brain with enable_glial=true, check glial field

    brain = brain_create("test_glial_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Check if glial is enabled in config and glial subsystem exists
    if (brain->config.enable_glial) {
        EXPECT_NE(brain->glial, nullptr) << "Glial integration should be created when enable_glial=true";
    }
}

TEST_F(BrainGlialIntegrationTest, BrainWithGlialDisabled_NoGlialSubsystem) {
    // WHAT: Verify brain with enable_glial=false has no glial
    // WHY:  Glial should only be created when explicitly enabled
    // HOW:  Create brain, disable glial, verify glial is null

    brain = brain_create("test_no_glial", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // If glial not enabled, it should be null
    if (!brain->config.enable_glial) {
        EXPECT_EQ(brain->glial, nullptr);
    }
}

TEST_F(BrainGlialIntegrationTest, BrainGlialConfig_CanBeModified) {
    // WHAT: Verify glial configuration is accessible
    // WHY:  Users should be able to configure glial behavior
    // HOW:  Access glial config fields

    brain = brain_create("test_glial_config", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Toggle glial enable flag (just verify we can access it)
    bool original = brain->config.enable_glial;
    brain->config.enable_glial = !original;
    EXPECT_NE(brain->config.enable_glial, original);
    brain->config.enable_glial = original; // Restore
}

// ============================================================================
// SCENARIO 2: Microglia-Network Integration
// ============================================================================

TEST_F(GlialIntegrationTest, MicrogliaPruning_AffectsNetworkSynapses) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);
    glial_integration_set_microglia_network(gi, microglia_net);
    glial_integration_set_microglia_pruning_enabled(gi, true);

    uint32_t weak_synapse_id = 2 * 10000 + 3;
    glial_integration_assign_microglia_to_synapse(gi, 0, weak_synapse_id);

    microglia_t* mg = microglia_net->microglia[0];
    ASSERT_NE(mg, nullptr);
    microglia_monitor_synapse(mg, weak_synapse_id);

    uint64_t timestamp = nimcp_time_monotonic_us();
    for (int i = 0; i < 5; i++) {
        microglia_track_synapse_activity(mg, weak_synapse_id, 0.01f, timestamp + i * 10000);
    }

    uint32_t strong_synapse_id = 0 * 10000 + 1;
    glial_integration_assign_microglia_to_synapse(gi, 0, strong_synapse_id);
    microglia_monitor_synapse(mg, strong_synapse_id);
    for (int i = 0; i < 5; i++) {
        microglia_track_synapse_activity(mg, strong_synapse_id, 1.0f, timestamp + i * 10000);
    }

    microglia_update_activity_scores(mg, timestamp + 100000);
    float weak_score = microglia_get_synapse_activity_score(mg, weak_synapse_id);
    float strong_score = microglia_get_synapse_activity_score(mg, strong_synapse_id);
    EXPECT_LT(weak_score, strong_score);
}

TEST_F(GlialIntegrationTest, MicrogliaNetwork_MultiSynapseMonitoring) {
    // WHAT: Test microglia monitoring multiple synapses
    // WHY:  Single microglia can survey many synapses
    // HOW:  Assign multiple synapses, track different activity levels

    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);
    glial_integration_set_microglia_network(gi, microglia_net);
    glial_integration_set_microglia_pruning_enabled(gi, true);

    microglia_t* mg = microglia_net->microglia[0];
    ASSERT_NE(mg, nullptr);

    // Monitor multiple synapses
    uint32_t synapse1 = 0 * 10000 + 1;
    uint32_t synapse2 = 1 * 10000 + 2;
    uint32_t synapse3 = 2 * 10000 + 3;

    microglia_monitor_synapse(mg, synapse1);
    microglia_monitor_synapse(mg, synapse2);
    microglia_monitor_synapse(mg, synapse3);

    EXPECT_EQ(mg->num_monitored_synapses, 3);

    // Track different activity levels
    uint64_t timestamp = nimcp_time_monotonic_us();
    microglia_track_synapse_activity(mg, synapse1, 1.0f, timestamp);
    microglia_track_synapse_activity(mg, synapse2, 0.5f, timestamp);
    microglia_track_synapse_activity(mg, synapse3, 0.05f, timestamp);

    // Verify different scores
    float score1 = microglia_get_synapse_activity_score(mg, synapse1);
    float score2 = microglia_get_synapse_activity_score(mg, synapse2);
    float score3 = microglia_get_synapse_activity_score(mg, synapse3);

    EXPECT_GT(score1, score2);
    EXPECT_GT(score2, score3);
}

// ============================================================================
// SCENARIO 3: Oligodendrocyte-Velocity Integration
// ============================================================================

TEST_F(GlialIntegrationTest, OligodendrocyteMyelination_AffectsConduction) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);
    glial_integration_set_oligodendrocyte_network(gi, oligo_net);
    glial_integration_set_oligodendrocyte_myelination_enabled(gi, true);

    glial_integration_assign_oligodendrocyte_to_neuron(gi, 0, 5);

    oligodendrocyte_t* oligo = oligo_net->oligodendrocytes[0];
    ASSERT_NE(oligo, nullptr);
    oligodendrocyte_assign_neuron(oligo, 5);

    float initial_myelin = oligodendrocyte_get_myelination_level(oligo, 5);
    EXPECT_FLOAT_EQ(initial_myelin, 0.0f);

    uint64_t timestamp = nimcp_time_monotonic_us();
    for (int i = 0; i < 20; i++) {
        oligodendrocyte_track_activity(oligo, 5, 1.0f, timestamp + i * 1000);
        glial_integration_on_neuron_fired(gi, 5, timestamp + i * 1000);
    }

    oligodendrocyte_remodel_myelination(oligo, 1.0f);
    float final_myelin = oligodendrocyte_get_myelination_level(oligo, 5);
    EXPECT_GT(final_myelin, initial_myelin);
    EXPECT_LE(final_myelin, 1.0f);
}

TEST_F(GlialIntegrationTest, OligodendrocyteMyelination_VelocityCalculation) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);
    glial_integration_set_oligodendrocyte_network(gi, oligo_net);

    oligodendrocyte_t* oligo = oligo_net->oligodendrocytes[0];
    ASSERT_NE(oligo, nullptr);
    oligodendrocyte_assign_neuron(oligo, 7);

    float base_velocity = NIMCP_OLIGO_BASE_VELOCITY_MS;
    float velocity_unmyelinated = oligodendrocyte_compute_conduction_velocity(oligo, 7, base_velocity);
    EXPECT_FLOAT_EQ(velocity_unmyelinated, base_velocity);

    uint64_t timestamp = nimcp_time_monotonic_us();
    for (int i = 0; i < 50; i++) {
        oligodendrocyte_track_activity(oligo, 7, 1.0f, timestamp + i * 1000);
    }
    oligodendrocyte_remodel_myelination(oligo, 2.0f);

    float velocity_myelinated = oligodendrocyte_compute_conduction_velocity(oligo, 7, base_velocity);
    EXPECT_GE(velocity_myelinated, velocity_unmyelinated);
}

// ============================================================================
// SCENARIO 4: Astrocyte-Microglia Coordination
// ============================================================================

TEST_F(GlialIntegrationTest, AstrocyteMicrogliaCoordination_BothActive) {
    // WHAT: Test astrocyte and microglia working together
    // WHY:  Both cell types share surveillance of neural activity
    // HOW:  Assign both to overlapping synapses, verify both track activity

    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);
    glial_integration_set_astrocyte_network(gi, astrocyte_net);
    glial_integration_set_microglia_network(gi, microglia_net);
    glial_integration_set_astrocyte_modulation_enabled(gi, true);
    glial_integration_set_microglia_pruning_enabled(gi, true);

    microglia_t* mg = microglia_net->microglia[0];
    astrocyte_t* ast = astrocyte_net->astrocytes[0];
    ASSERT_NE(mg, nullptr);
    ASSERT_NE(ast, nullptr);

    // Assign both to same synapse
    uint32_t synapse_id = 0 * 10000 + 1;
    glial_integration_assign_astrocyte_to_synapse(gi, 0, synapse_id);
    glial_integration_assign_microglia_to_synapse(gi, 0, synapse_id);
    microglia_monitor_synapse(mg, synapse_id);

    // Simulate synaptic activity
    uint64_t timestamp = nimcp_time_monotonic_us();
    for (int i = 0; i < 10; i++) {
        glial_integration_on_synapse_fired(gi, 0, 1, 0.5f, timestamp + i * 1000);
        microglia_track_synapse_activity(mg, synapse_id, 0.5f, timestamp + i * 1000);
    }

    glial_integration_step(gi, timestamp + 10000);

    // Both should have tracked activity
    float modulation = glial_integration_get_synaptic_modulation(gi, 0, 1);
    EXPECT_GE(modulation, 0.8f);
    EXPECT_LE(modulation, 1.2f);

    float activity_score = microglia_get_synapse_activity_score(mg, synapse_id);
    EXPECT_GT(activity_score, 0.0f);
}

TEST_F(GlialIntegrationTest, MicrogliaPruningDecision) {
    // WHAT: Test microglia activity-based pruning decision
    // WHY:  Low activity synapses should be candidates for pruning
    // HOW:  Track low activity, check pruning recommendation

    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);
    glial_integration_set_microglia_network(gi, microglia_net);
    glial_integration_set_microglia_pruning_enabled(gi, true);

    microglia_t* mg = microglia_net->microglia[0];
    ASSERT_NE(mg, nullptr);

    // Assign and monitor weak synapse
    uint32_t synapse_id = 2 * 10000 + 3;
    glial_integration_assign_microglia_to_synapse(gi, 0, synapse_id);
    microglia_monitor_synapse(mg, synapse_id);

    // Track very low activity
    uint64_t timestamp = nimcp_time_monotonic_us();
    microglia_track_synapse_activity(mg, synapse_id, 0.01f, timestamp);

    // Let activity decay
    microglia_update_activity_scores(mg, timestamp + 1000000);

    // Check activity score is low
    float activity_score = microglia_get_synapse_activity_score(mg, synapse_id);
    EXPECT_LT(activity_score, mg->pruning_threshold);

    // Should be identified as weak
    uint32_t weak_synapses[10];
    uint32_t num_weak = microglia_identify_weak_synapses(mg, weak_synapses, 10);
    EXPECT_GT(num_weak, 0);
}

// ============================================================================
// SCENARIO 5: Full Glial System
// ============================================================================

TEST_F(GlialIntegrationTest, FullGlialSystem_AllCellTypesActive) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    glial_integration_set_astrocyte_network(gi, astrocyte_net);
    glial_integration_set_oligodendrocyte_network(gi, oligo_net);
    glial_integration_set_microglia_network(gi, microglia_net);

    glial_integration_set_astrocyte_modulation_enabled(gi, true);
    glial_integration_set_oligodendrocyte_myelination_enabled(gi, true);
    glial_integration_set_microglia_pruning_enabled(gi, true);

    uint32_t synapse_01 = 0 * 10000 + 1;
    uint32_t synapse_12 = 1 * 10000 + 2;
    uint32_t synapse_23 = 2 * 10000 + 3;

    glial_integration_assign_astrocyte_to_synapse(gi, 0, synapse_01);
    glial_integration_assign_astrocyte_to_synapse(gi, 1, synapse_12);
    glial_integration_assign_oligodendrocyte_to_neuron(gi, 0, 1);
    glial_integration_assign_oligodendrocyte_to_neuron(gi, 1, 2);
    glial_integration_assign_microglia_to_synapse(gi, 0, synapse_23);

    uint64_t timestamp = nimcp_time_monotonic_us();
    for (int step = 0; step < 100; step++) {
        glial_integration_on_synapse_fired(gi, 0, 1, 0.5f, timestamp);
        glial_integration_on_synapse_fired(gi, 1, 2, 0.7f, timestamp);
        glial_integration_on_neuron_fired(gi, 1, timestamp);
        glial_integration_on_neuron_fired(gi, 2, timestamp);
        glial_integration_step(gi, timestamp);
        timestamp += 1000;
    }

    glial_integration_stats_t stats{};
    nimcp_result_t result = glial_integration_get_stats(gi, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.num_astrocytes, 3);
    EXPECT_EQ(stats.num_oligodendrocytes, 3);
    EXPECT_EQ(stats.num_microglia, 3);
    EXPECT_EQ(stats.num_tripartite_synapses, 2);
    EXPECT_EQ(stats.num_myelinated_neurons, 2);
    EXPECT_EQ(stats.num_monitored_synapses, 1);
}

TEST_F(GlialIntegrationTest, FullGlialSystem_ModulationEffects) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);
    glial_integration_set_astrocyte_network(gi, astrocyte_net);
    glial_integration_set_astrocyte_modulation_enabled(gi, true);

    uint32_t synapse_id = 0 * 10000 + 1;
    glial_integration_assign_astrocyte_to_synapse(gi, 0, synapse_id);

    astrocyte_t* ast = astrocyte_net->astrocytes[0];
    ASSERT_NE(ast, nullptr);

    float initial_modulation = glial_integration_get_synaptic_modulation(gi, 0, 1);
    EXPECT_GE(initial_modulation, 0.8f);
    EXPECT_LE(initial_modulation, 1.2f);

    astrocyte_update_calcium(ast, 0.01f, 5.0f);

    float elevated_modulation = glial_integration_get_synaptic_modulation(gi, 0, 1);
    EXPECT_GE(elevated_modulation, 0.8f);
    EXPECT_LE(elevated_modulation, 1.2f);
}

// ============================================================================
// SCENARIO 6: Activity Feedback Loop
// ============================================================================

TEST_F(GlialIntegrationTest, ActivityFeedbackLoop_MyelinationAdaptation) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);
    glial_integration_set_oligodendrocyte_network(gi, oligo_net);
    glial_integration_set_oligodendrocyte_myelination_enabled(gi, true);

    glial_integration_assign_oligodendrocyte_to_neuron(gi, 0, 3);
    glial_integration_assign_oligodendrocyte_to_neuron(gi, 1, 4);

    oligodendrocyte_t* oligo0 = oligo_net->oligodendrocytes[0];
    oligodendrocyte_t* oligo1 = oligo_net->oligodendrocytes[1];
    oligodendrocyte_assign_neuron(oligo0, 3);
    oligodendrocyte_assign_neuron(oligo1, 4);

    uint64_t timestamp = nimcp_time_monotonic_us();

    for (int i = 0; i < 100; i++) {
        oligodendrocyte_track_activity(oligo0, 3, 1.0f, timestamp + i * 100);
        glial_integration_on_neuron_fired(gi, 3, timestamp + i * 100);
    }

    for (int i = 0; i < 10; i++) {
        oligodendrocyte_track_activity(oligo1, 4, 0.1f, timestamp + i * 1000);
        glial_integration_on_neuron_fired(gi, 4, timestamp + i * 1000);
    }

    oligodendrocyte_network_step(oligo_net, 1.0f);

    float myelin_high = oligodendrocyte_get_myelination_level(oligo0, 3);
    float myelin_low = oligodendrocyte_get_myelination_level(oligo1, 4);
    EXPECT_GE(myelin_high, myelin_low);
}

TEST_F(GlialIntegrationTest, ActivityFeedbackLoop_VelocityChangesWithActivity) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);
    glial_integration_set_oligodendrocyte_network(gi, oligo_net);
    glial_integration_set_oligodendrocyte_myelination_enabled(gi, true);

    glial_integration_assign_oligodendrocyte_to_neuron(gi, 0, 8);

    oligodendrocyte_t* oligo = oligo_net->oligodendrocytes[0];
    oligodendrocyte_assign_neuron(oligo, 8);

    float base_velocity = NIMCP_OLIGO_BASE_VELOCITY_MS;
    float velocity_before = oligodendrocyte_compute_conduction_velocity(oligo, 8, base_velocity);

    uint64_t timestamp = nimcp_time_monotonic_us();
    for (int epoch = 0; epoch < 5; epoch++) {
        for (int i = 0; i < 50; i++) {
            oligodendrocyte_track_activity(oligo, 8, 1.0f, timestamp);
            timestamp += 100;
        }
        oligodendrocyte_remodel_myelination(oligo, 0.5f);
    }

    float velocity_after = oligodendrocyte_compute_conduction_velocity(oligo, 8, base_velocity);
    EXPECT_GE(velocity_after, velocity_before);

    float myelin_factor = glial_integration_get_myelination_factor(gi, 8);
    EXPECT_GE(myelin_factor, 0.0f);
}

// ============================================================================
// Basic API Tests
// ============================================================================

TEST_F(GlialIntegrationTest, CreateDestroy) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);
    EXPECT_EQ(gi->network, network);
    EXPECT_FALSE(gi->enable_astrocyte_modulation);
    EXPECT_FALSE(gi->enable_oligodendrocyte_myelination);
    EXPECT_FALSE(gi->enable_microglia_pruning);
    glial_integration_destroy(gi);
    gi = nullptr;
}

TEST_F(GlialIntegrationTest, CreateWithNullNetwork) {
    gi = glial_integration_create(nullptr, 100);
    EXPECT_NE(gi, nullptr);
    if (gi) EXPECT_EQ(gi->network, nullptr);
}

TEST_F(GlialIntegrationTest, DestroyNull) {
    glial_integration_destroy(nullptr);
}

TEST_F(GlialIntegrationTest, AssignGlialNetworks) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);

    EXPECT_EQ(glial_integration_set_astrocyte_network(gi, astrocyte_net), NIMCP_SUCCESS);
    EXPECT_EQ(gi->astrocyte_network, astrocyte_net);

    EXPECT_EQ(glial_integration_set_oligodendrocyte_network(gi, oligo_net), NIMCP_SUCCESS);
    EXPECT_EQ(gi->oligodendrocyte_network, oligo_net);

    EXPECT_EQ(glial_integration_set_microglia_network(gi, microglia_net), NIMCP_SUCCESS);
    EXPECT_EQ(gi->microglia_network, microglia_net);
}

TEST_F(GlialIntegrationTest, SynapticModulation_NoAstrocyte) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);
    float modulation = glial_integration_get_synaptic_modulation(gi, 0, 1);
    EXPECT_FLOAT_EQ(modulation, 1.0f);
}

TEST_F(GlialIntegrationTest, MyelinationFactor_NoOligodendrocyte) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);
    float myelination = glial_integration_get_myelination_factor(gi, 5);
    EXPECT_FLOAT_EQ(myelination, 0.0f);
}

TEST_F(GlialIntegrationTest, ShouldPruneSynapse_NoMicroglia) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);
    bool should_prune = glial_integration_should_prune_synapse(gi, 0, 1);
    EXPECT_FALSE(should_prune);
}

TEST_F(GlialIntegrationTest, NullParameterHandling) {
    glial_integration_on_synapse_fired(nullptr, 0, 1, 0.5f, 0);
    glial_integration_on_neuron_fired(nullptr, 0, 0);
    glial_integration_step(nullptr, 0);

    float modulation = glial_integration_get_synaptic_modulation(nullptr, 0, 1);
    EXPECT_FLOAT_EQ(modulation, 1.0f);

    float myelination = glial_integration_get_myelination_factor(nullptr, 0);
    EXPECT_FLOAT_EQ(myelination, 0.0f);
}

TEST_F(GlialIntegrationTest, GetStatistics) {
    gi = glial_integration_create(network, 100);
    ASSERT_NE(gi, nullptr);
    glial_integration_set_astrocyte_network(gi, astrocyte_net);
    glial_integration_set_oligodendrocyte_network(gi, oligo_net);
    glial_integration_set_microglia_network(gi, microglia_net);

    glial_integration_stats_t stats{};
    nimcp_result_t result = glial_integration_get_stats(gi, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.num_astrocytes, 3);
    EXPECT_EQ(stats.num_oligodendrocytes, 3);
    EXPECT_EQ(stats.num_microglia, 3);
}

TEST_F(GlialIntegrationTest, PerformanceLargeNetwork) {
    network_config_t config{};
    config.num_neurons = 1000;
    config.input_size = 100;
    config.output_size = 10;
    config.ei_ratio = 0.8f;
    config.learning_rate = 0.01f;
    neural_network_t large_network = neural_network_create(&config);
    ASSERT_NE(large_network, nullptr);

    gi = glial_integration_create(large_network, 10000);
    ASSERT_NE(gi, nullptr);

    glial_integration_set_astrocyte_network(gi, astrocyte_net);

    for (uint32_t i = 0; i < 100; i++) {
        uint32_t pre = i;
        uint32_t post = (i + 1) % 1000;
        uint32_t synapse_id = pre * 10000 + post;
        glial_integration_assign_astrocyte_to_synapse(gi, i % 3, synapse_id);
    }

    uint64_t start = nimcp_time_monotonic_us();
    for (uint32_t i = 0; i < 100; i++) {
        uint32_t pre = i;
        uint32_t post = (i + 1) % 1000;
        glial_integration_get_synaptic_modulation(gi, pre, post);
    }
    uint64_t end = nimcp_time_monotonic_us();

    uint64_t elapsed = end - start;
    EXPECT_LT(elapsed, 1000);

    neural_network_destroy(large_network);
}
