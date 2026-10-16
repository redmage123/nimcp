/**
 * @file test_dendrite.cpp
 * @brief Unit tests for dendrite module
 *
 * WHAT: Comprehensive testing of dendrite creation, segments, spines, and plasticity
 * WHY:  Ensure dendrite module works correctly with biological accuracy
 * HOW:  GTest-based tests covering all API functions
 *
 * TEST COVERAGE:
 * - Dendrite creation/destruction
 * - Segment management
 * - Spine management (add/remove)
 * - Cable theory (attenuation)
 * - Input integration
 * - Time evolution
 * - LTP/LTD plasticity
 * - Structural plasticity
 * - Network management
 *
 * @version Phase: Dendrite Integration
 * @date 2025-11-24
 */

#include <gtest/gtest.h>
#include "core/dendrite/nimcp_dendrite.h"
#include <cmath>

// ============================================================================
// Test Fixture
// ============================================================================

class DendriteTest : public ::testing::Test {
protected:
    dendrite_t* dendrite;
    dendrite_config_t config;

    void SetUp() override {
        // Create default dendrite configuration
        config = {};
        config.id = 1;
        config.type = DENDRITE_TYPE_BASAL;
        config.target_neuron_id = 100;
        config.total_length = 200.0f;
        config.mean_diameter = 2.0f;
        config.start_pos[0] = 0.0f;
        config.start_pos[1] = 0.0f;
        config.start_pos[2] = 0.0f;
        config.integration_window_ms = 20.0f;
        config.structural_plasticity = 0.01f;
        config.ltp_threshold = 0.8f;
        config.ltd_threshold = 0.3f;

        dendrite = dendrite_create(&config);
        ASSERT_NE(dendrite, nullptr);
    }

    void TearDown() override {
        if (dendrite) {
            dendrite_destroy(dendrite);
            dendrite = nullptr;
        }
    }
};

// ============================================================================
// Dendrite Creation Tests
// ============================================================================

/**
 * WHAT: Test dendrite creation with valid config
 * WHY:  Verify dendrite structure is properly initialized
 * HOW:  Check all fields are set correctly
 */
TEST_F(DendriteTest, CreateDendrite) {
    EXPECT_EQ(dendrite->id, 1u);
    EXPECT_EQ(dendrite->type, DENDRITE_TYPE_BASAL);
    EXPECT_EQ(dendrite->state, DENDRITE_STATE_NORMAL);
    EXPECT_EQ(dendrite->target_neuron_id, 100u);
    EXPECT_FLOAT_EQ(dendrite->total_length, 200.0f);
    EXPECT_FLOAT_EQ(dendrite->mean_diameter, 2.0f);
    EXPECT_FLOAT_EQ(dendrite->integration_window_ms, 20.0f);
    EXPECT_FLOAT_EQ(dendrite->ltp_threshold, 0.8f);
    EXPECT_FLOAT_EQ(dendrite->ltd_threshold, 0.3f);
    EXPECT_EQ(dendrite->num_segments, 0u);
    EXPECT_EQ(dendrite->num_spines, 0u);
    EXPECT_TRUE(dendrite->is_functional);
}

/**
 * WHAT: Test dendrite creation with NULL config
 * WHY:  Verify proper error handling
 * HOW:  Pass NULL config, expect NULL return
 */
TEST_F(DendriteTest, CreateDendriteNullConfig) {
    dendrite_t* null_dendrite = dendrite_create(nullptr);
    EXPECT_EQ(null_dendrite, nullptr);
}

/**
 * WHAT: Test dendrite destruction
 * WHY:  Verify resources are freed correctly
 * HOW:  Create and destroy, check no crashes
 */
TEST_F(DendriteTest, DestroyDendrite) {
    dendrite_t* temp = dendrite_create(&config);
    ASSERT_NE(temp, nullptr);

    dendrite_destroy(temp);
    // If we get here without crash, destruction succeeded
    SUCCEED();
}

/**
 * WHAT: Test destroying NULL dendrite
 * WHY:  Verify graceful handling of NULL
 * HOW:  Call destroy with NULL, expect no crash
 */
TEST_F(DendriteTest, DestroyNullDendrite) {
    dendrite_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

// ============================================================================
// Segment Management Tests
// ============================================================================

/**
 * WHAT: Test creating segments
 * WHY:  Verify segment array is properly initialized
 * HOW:  Create segments, check count and properties
 */
TEST_F(DendriteTest, CreateSegments) {
    segment_config_t seg_configs[3];

    // Root segment
    seg_configs[0] = {};
    seg_configs[0].type = DENDRITE_SEGMENT_PROXIMAL;
    seg_configs[0].parent_segment = UINT32_MAX;
    seg_configs[0].position[0] = 0.0f;
    seg_configs[0].position[1] = 0.0f;
    seg_configs[0].position[2] = 0.0f;
    seg_configs[0].length = 50.0f;
    seg_configs[0].diameter = 2.0f;
    seg_configs[0].path_distance = 0.0f;
    seg_configs[0].has_active_properties = false;

    // Middle segment
    seg_configs[1] = {};
    seg_configs[1].type = DENDRITE_SEGMENT_DISTAL;
    seg_configs[1].parent_segment = 0;
    seg_configs[1].position[0] = 50.0f;
    seg_configs[1].position[1] = 0.0f;
    seg_configs[1].position[2] = 0.0f;
    seg_configs[1].length = 50.0f;
    seg_configs[1].diameter = 1.5f;
    seg_configs[1].path_distance = 50.0f;
    seg_configs[1].has_active_properties = false;

    // Terminal segment
    seg_configs[2] = {};
    seg_configs[2].type = DENDRITE_SEGMENT_TERMINAL;
    seg_configs[2].parent_segment = 1;
    seg_configs[2].position[0] = 100.0f;
    seg_configs[2].position[1] = 0.0f;
    seg_configs[2].position[2] = 0.0f;
    seg_configs[2].length = 50.0f;
    seg_configs[2].diameter = 1.0f;
    seg_configs[2].path_distance = 100.0f;
    seg_configs[2].has_active_properties = false;

    bool success = dendrite_create_segments(dendrite, 3, seg_configs);
    ASSERT_TRUE(success);

    EXPECT_EQ(dendrite->num_segments, 3u);
    EXPECT_NE(dendrite->segments, nullptr);

    // Check root segment
    EXPECT_EQ(dendrite->segments[0].id, 0u);
    EXPECT_EQ(dendrite->segments[0].type, DENDRITE_SEGMENT_PROXIMAL);
    EXPECT_EQ(dendrite->segments[0].parent_segment, UINT32_MAX);
    EXPECT_FLOAT_EQ(dendrite->segments[0].length, 50.0f);
    EXPECT_FLOAT_EQ(dendrite->segments[0].diameter, 2.0f);

    // Check parent-child relationships
    EXPECT_EQ(dendrite->segments[0].num_children, 1u);
    EXPECT_EQ(dendrite->segments[0].child_segments[0], 1u);
}

/**
 * WHAT: Test adding branch to dendrite
 * WHY:  Verify branching morphology works
 * HOW:  Add branch, check segment count and structure
 */
TEST_F(DendriteTest, AddBranch) {
    // First create initial segments
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.position[0] = 0.0f;
    seg_config.position[1] = 0.0f;
    seg_config.position[2] = 0.0f;
    seg_config.length = 50.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    // Add branch
    uint32_t branch_id = dendrite_add_branch(dendrite, 0, 30.0f, 1.5f, M_PI / 4.0f);
    ASSERT_NE(branch_id, UINT32_MAX);

    EXPECT_EQ(dendrite->num_segments, 2u);
    EXPECT_EQ(dendrite->segments[1].id, branch_id);
    EXPECT_EQ(dendrite->segments[1].type, DENDRITE_SEGMENT_OBLIQUE);
    EXPECT_EQ(dendrite->segments[1].parent_segment, 0u);
    EXPECT_FLOAT_EQ(dendrite->segments[1].length, 30.0f);
    EXPECT_FLOAT_EQ(dendrite->segments[1].diameter, 1.5f);

    // Check parent has child
    EXPECT_EQ(dendrite->segments[0].num_children, 1u);
    EXPECT_EQ(dendrite->segments[0].child_segments[0], branch_id);
}

/**
 * WHAT: Test cable properties calculation
 * WHY:  Verify electrical properties are correct
 * HOW:  Create segment, check R_m, C_m, R_a
 */
TEST_F(DendriteTest, CableProperties) {
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.position[0] = 0.0f;
    seg_config.position[1] = 0.0f;
    seg_config.position[2] = 0.0f;
    seg_config.length = 100.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    dendritic_segment_t* seg = &dendrite->segments[0];

    // Check cable properties are non-zero
    EXPECT_GT(seg->R_m, 0.0f);
    EXPECT_GT(seg->C_m, 0.0f);
    EXPECT_GT(seg->R_a, 0.0f);
}

// ============================================================================
// Spine Management Tests
// ============================================================================

/**
 * WHAT: Test adding spine to segment
 * WHY:  Verify spine creation and initialization
 * HOW:  Add spine, check properties
 */
TEST_F(DendriteTest, AddSpine) {
    // Create segment first
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.position[0] = 0.0f;
    seg_config.position[1] = 0.0f;
    seg_config.position[2] = 0.0f;
    seg_config.length = 50.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    // Add spine
    uint32_t spine_id = dendrite_add_spine(dendrite, 0, SPINE_TYPE_MUSHROOM, 200);
    ASSERT_NE(spine_id, UINT32_MAX);

    EXPECT_EQ(dendrite->num_spines, 1u);
    EXPECT_NE(dendrite->spines, nullptr);

    dendritic_spine_t* spine = &dendrite->spines[0];
    EXPECT_EQ(spine->id, spine_id);
    EXPECT_EQ(spine->type, SPINE_TYPE_MUSHROOM);
    EXPECT_EQ(spine->state, SPINE_STATE_STABLE);
    EXPECT_EQ(spine->dendrite_id, dendrite->id);
    EXPECT_EQ(spine->segment_id, 0u);
    EXPECT_EQ(spine->synapse_id, 200u);

    // Check morphology
    EXPECT_GT(spine->neck_length, 0.0f);
    EXPECT_GT(spine->neck_diameter, 0.0f);
    EXPECT_GT(spine->head_diameter, 0.0f);
    EXPECT_GT(spine->head_volume, 0.0f);

    // Check electrical properties
    EXPECT_GT(spine->neck_resistance, 0.0f);
    EXPECT_GT(spine->head_capacitance, 0.0f);

    // Check segment has spine
    EXPECT_EQ(dendrite->segments[0].num_spines, 1u);
    EXPECT_EQ(dendrite->segments[0].spine_ids[0], spine_id);
}

/**
 * WHAT: Test adding multiple spine types
 * WHY:  Verify different spine morphologies
 * HOW:  Add thin, stubby, mushroom, filopodia spines
 */
TEST_F(DendriteTest, MultipleSpineTypes) {
    // Create segment
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.position[0] = 0.0f;
    seg_config.position[1] = 0.0f;
    seg_config.position[2] = 0.0f;
    seg_config.length = 50.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    // Add different spine types
    uint32_t thin_id = dendrite_add_spine(dendrite, 0, SPINE_TYPE_THIN, 201);
    uint32_t stubby_id = dendrite_add_spine(dendrite, 0, SPINE_TYPE_STUBBY, 202);
    uint32_t mushroom_id = dendrite_add_spine(dendrite, 0, SPINE_TYPE_MUSHROOM, 203);
    uint32_t filopodia_id = dendrite_add_spine(dendrite, 0, SPINE_TYPE_FILOPODIA, 204);

    EXPECT_EQ(dendrite->num_spines, 4u);

    // Check that different types have different morphologies
    dendritic_spine_t* thin = &dendrite->spines[thin_id];
    dendritic_spine_t* stubby = &dendrite->spines[stubby_id];
    dendritic_spine_t* mushroom = &dendrite->spines[mushroom_id];
    dendritic_spine_t* filopodia = &dendrite->spines[filopodia_id];

    // Thin spines have longer necks than stubby
    EXPECT_GT(thin->neck_length, stubby->neck_length);

    // Mushroom spines have larger heads than thin
    EXPECT_GT(mushroom->head_diameter, thin->head_diameter);

    // Filopodia have longest necks
    EXPECT_GT(filopodia->neck_length, thin->neck_length);
}

/**
 * WHAT: Test removing spine
 * WHY:  Verify structural plasticity spine elimination
 * HOW:  Add spine, remove it, check state
 */
TEST_F(DendriteTest, RemoveSpine) {
    // Create segment
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.position[0] = 0.0f;
    seg_config.position[1] = 0.0f;
    seg_config.position[2] = 0.0f;
    seg_config.length = 50.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    // Add spine
    uint32_t spine_id = dendrite_add_spine(dendrite, 0, SPINE_TYPE_MUSHROOM, 200);
    ASSERT_NE(spine_id, UINT32_MAX);

    // Remove spine
    bool success = dendrite_remove_spine(dendrite, spine_id);
    ASSERT_TRUE(success);

    // Check spine is marked eliminated
    EXPECT_EQ(dendrite->spines[spine_id].state, SPINE_STATE_ELIMINATED);
    EXPECT_FLOAT_EQ(dendrite->spines[spine_id].stability, 0.0f);

    // Check segment no longer has spine
    EXPECT_EQ(dendrite->segments[0].num_spines, 0u);
}

/**
 * WHAT: Test getting spine by synapse ID
 * WHY:  Verify spine lookup works
 * HOW:  Add spines, lookup by synapse ID
 */
TEST_F(DendriteTest, GetSpineBySynapse) {
    // Create segment
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.position[0] = 0.0f;
    seg_config.position[1] = 0.0f;
    seg_config.position[2] = 0.0f;
    seg_config.length = 50.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    // Add spines with different synapse IDs
    dendrite_add_spine(dendrite, 0, SPINE_TYPE_THIN, 100);
    dendrite_add_spine(dendrite, 0, SPINE_TYPE_MUSHROOM, 200);
    dendrite_add_spine(dendrite, 0, SPINE_TYPE_STUBBY, 300);

    // Lookup spine by synapse ID
    dendritic_spine_t* spine = dendrite_get_spine_by_synapse(dendrite, 200);
    ASSERT_NE(spine, nullptr);
    EXPECT_EQ(spine->synapse_id, 200u);
    EXPECT_EQ(spine->type, SPINE_TYPE_MUSHROOM);

    // Lookup non-existent synapse
    dendritic_spine_t* not_found = dendrite_get_spine_by_synapse(dendrite, 999);
    EXPECT_EQ(not_found, nullptr);
}

// ============================================================================
// Input Integration Tests
// ============================================================================

/**
 * WHAT: Test receiving input at segment
 * WHY:  Verify synaptic current injection works
 * HOW:  Send input, check voltage increases
 */
TEST_F(DendriteTest, ReceiveInput) {
    // Create segment
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.position[0] = 0.0f;
    seg_config.position[1] = 0.0f;
    seg_config.position[2] = 0.0f;
    seg_config.length = 50.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    // Initial voltage should be zero
    EXPECT_FLOAT_EQ(dendrite->segments[0].voltage, 0.0f);

    // Receive input
    bool success = dendrite_receive_input(dendrite, 0, 0.001f, 1000);
    ASSERT_TRUE(success);

    // Voltage should increase
    EXPECT_GT(dendrite->segments[0].voltage, 0.0f);

    // Calcium should increase
    EXPECT_GT(dendrite->segments[0].calcium, 0.0f);
}

/**
 * WHAT: Test computing somatic current
 * WHY:  Verify integration to soma works
 * HOW:  Add inputs, compute somatic current
 */
TEST_F(DendriteTest, ComputeSomaticCurrent) {
    // Create segments
    segment_config_t seg_configs[2];
    seg_configs[0] = {};
    seg_configs[0].type = DENDRITE_SEGMENT_PROXIMAL;
    seg_configs[0].parent_segment = UINT32_MAX;
    seg_configs[0].position[0] = 0.0f;
    seg_configs[0].position[1] = 0.0f;
    seg_configs[0].position[2] = 0.0f;
    seg_configs[0].length = 50.0f;
    seg_configs[0].diameter = 2.0f;
    seg_configs[0].path_distance = 10.0f;
    seg_configs[0].has_active_properties = false;

    seg_configs[1] = {};
    seg_configs[1].type = DENDRITE_SEGMENT_DISTAL;
    seg_configs[1].parent_segment = 0;
    seg_configs[1].position[0] = 50.0f;
    seg_configs[1].position[1] = 0.0f;
    seg_configs[1].position[2] = 0.0f;
    seg_configs[1].length = 50.0f;
    seg_configs[1].diameter = 1.5f;
    seg_configs[1].path_distance = 60.0f;
    seg_configs[1].has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 2, seg_configs));

    // Add inputs to both segments
    dendrite_receive_input(dendrite, 0, 0.002f, 1000);
    dendrite_receive_input(dendrite, 1, 0.001f, 1000);

    // Compute somatic current
    float somatic_current = dendrite_compute_somatic_current(dendrite);

    // Somatic current should be positive
    EXPECT_GT(somatic_current, 0.0f);

    // Somatic voltage should be set
    EXPECT_GT(dendrite->somatic_voltage, 0.0f);
}

/**
 * WHAT: Test signal attenuation
 * WHY:  Verify cable theory attenuation is correct
 * HOW:  Create long dendrite, check distal attenuation
 */
TEST_F(DendriteTest, SignalAttenuation) {
    // Create proximal and distal segments
    segment_config_t seg_configs[2];
    seg_configs[0] = {};
    seg_configs[0].type = DENDRITE_SEGMENT_PROXIMAL;
    seg_configs[0].parent_segment = UINT32_MAX;
    seg_configs[0].position[0] = 0.0f;
    seg_configs[0].position[1] = 0.0f;
    seg_configs[0].position[2] = 0.0f;
    seg_configs[0].length = 50.0f;
    seg_configs[0].diameter = 2.0f;
    seg_configs[0].path_distance = 10.0f;
    seg_configs[0].has_active_properties = false;

    seg_configs[1] = {};
    seg_configs[1].type = DENDRITE_SEGMENT_DISTAL;
    seg_configs[1].parent_segment = 0;
    seg_configs[1].position[0] = 200.0f;
    seg_configs[1].position[1] = 0.0f;
    seg_configs[1].position[2] = 0.0f;
    seg_configs[1].length = 50.0f;
    seg_configs[1].diameter = 1.0f;
    seg_configs[1].path_distance = 210.0f;  // Far from soma
    seg_configs[1].has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 2, seg_configs));

    // Get attenuation factors
    float proximal_atten = dendrite_get_attenuation(dendrite, 0);
    float distal_atten = dendrite_get_attenuation(dendrite, 1);

    // Proximal should have less attenuation than distal
    EXPECT_GT(proximal_atten, distal_atten);

    // Both should be in range [0, 1]
    EXPECT_GE(proximal_atten, 0.0f);
    EXPECT_LE(proximal_atten, 1.0f);
    EXPECT_GE(distal_atten, 0.0f);
    EXPECT_LE(distal_atten, 1.0f);
}

// ============================================================================
// Time Evolution Tests
// ============================================================================

/**
 * WHAT: Test dendrite timestep
 * WHY:  Verify voltage and calcium dynamics evolve correctly
 * HOW:  Add input, run timestep, check state updates
 */
TEST_F(DendriteTest, DendriteStep) {
    // Create segment
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.position[0] = 0.0f;
    seg_config.position[1] = 0.0f;
    seg_config.position[2] = 0.0f;
    seg_config.length = 50.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    // Add input
    dendrite_receive_input(dendrite, 0, 0.005f, 1000);

    float initial_voltage = dendrite->segments[0].voltage;
    float initial_calcium = dendrite->segments[0].calcium;

    // Run timestep
    dendrite_step(dendrite, 1.0f, 1000);

    // Voltage should decay (or change)
    // With leak, voltage should approach zero
    // Note: May increase if input is strong, but should evolve
    EXPECT_NE(dendrite->segments[0].voltage, initial_voltage);

    // Calcium should decay
    EXPECT_LT(dendrite->segments[0].calcium, initial_calcium);

    // Mean voltage should be updated
    EXPECT_FLOAT_EQ(dendrite->mean_voltage, dendrite->segments[0].voltage);

    // Calcium level should be updated
    EXPECT_FLOAT_EQ(dendrite->calcium_level, dendrite->segments[0].calcium);
}

/**
 * WHAT: Test voltage decay over time
 * WHY:  Verify passive membrane properties
 * HOW:  Set voltage, run timesteps, check exponential decay
 */
TEST_F(DendriteTest, VoltageDecay) {
    // Create segment
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.position[0] = 0.0f;
    seg_config.position[1] = 0.0f;
    seg_config.position[2] = 0.0f;
    seg_config.length = 50.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    // Set initial voltage
    dendrite->segments[0].voltage = 0.020f;  // 20 mV

    // Run multiple timesteps
    for (int i = 0; i < 100; i++) {
        dendrite_step(dendrite, 1.0f, 1000 + i);
    }

    // Voltage should decay toward zero
    EXPECT_LT(dendrite->segments[0].voltage, 0.020f);
}

// ============================================================================
// Plasticity Tests
// ============================================================================

/**
 * WHAT: Test LTP induction
 * WHY:  Verify long-term potentiation strengthens synapses
 * HOW:  Raise calcium, induce LTP, check weight increase
 */
TEST_F(DendriteTest, InduceLTP) {
    // Create segment and spine
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.position[0] = 0.0f;
    seg_config.position[1] = 0.0f;
    seg_config.position[2] = 0.0f;
    seg_config.length = 50.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    uint32_t spine_id = dendrite_add_spine(dendrite, 0, SPINE_TYPE_MUSHROOM, 200);
    ASSERT_NE(spine_id, UINT32_MAX);

    // Set calcium above LTP threshold
    dendrite->spines[spine_id].calcium = 1.0f;

    float initial_weight = dendrite->spines[spine_id].synaptic_weight;
    float initial_ampa = dendrite->spines[spine_id].ampa_receptors;

    // Induce LTP
    dendrite_induce_ltp(dendrite, spine_id, 1.0f);

    // Weight should increase
    EXPECT_GT(dendrite->spines[spine_id].synaptic_weight, initial_weight);

    // AMPA receptors should increase
    EXPECT_GT(dendrite->spines[spine_id].ampa_receptors, initial_ampa);

    // Stability should increase
    EXPECT_GT(dendrite->spines[spine_id].stability, 0.5f);

    // LTP event should be recorded
    EXPECT_EQ(dendrite->activity.ltp_events, 1u);
}

/**
 * WHAT: Test LTD induction
 * WHY:  Verify long-term depression weakens synapses
 * HOW:  Set calcium in LTD range, induce LTD, check weight decrease
 */
TEST_F(DendriteTest, InduceLTD) {
    // Create segment and spine
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.position[0] = 0.0f;
    seg_config.position[1] = 0.0f;
    seg_config.position[2] = 0.0f;
    seg_config.length = 50.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    uint32_t spine_id = dendrite_add_spine(dendrite, 0, SPINE_TYPE_MUSHROOM, 200);
    ASSERT_NE(spine_id, UINT32_MAX);

    // Set calcium in LTD range (between LTD and LTP thresholds)
    dendrite->spines[spine_id].calcium = 0.5f;

    float initial_weight = dendrite->spines[spine_id].synaptic_weight;
    float initial_ampa = dendrite->spines[spine_id].ampa_receptors;

    // Induce LTD
    dendrite_induce_ltd(dendrite, spine_id, 1.0f);

    // Weight should decrease
    EXPECT_LT(dendrite->spines[spine_id].synaptic_weight, initial_weight);

    // AMPA receptors should decrease
    EXPECT_LT(dendrite->spines[spine_id].ampa_receptors, initial_ampa);

    // LTD event should be recorded
    EXPECT_EQ(dendrite->activity.ltd_events, 1u);
}

/**
 * WHAT: Test structural plasticity
 * WHY:  Verify spine elimination based on stability
 * HOW:  Create unstable spine, run structural plasticity, check elimination
 */
TEST_F(DendriteTest, StructuralPlasticity) {
    // Create segment and spine
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.position[0] = 0.0f;
    seg_config.position[1] = 0.0f;
    seg_config.position[2] = 0.0f;
    seg_config.length = 50.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    uint32_t spine_id = dendrite_add_spine(dendrite, 0, SPINE_TYPE_THIN, 200);
    ASSERT_NE(spine_id, UINT32_MAX);

    // Make spine very unstable (below 0.2 threshold)
    dendrite->spines[spine_id].stability = 0.15f;

    // Run structural plasticity multiple times
    // With structural_plasticity=0.01 and stability=0.15, need 15+ iterations to reach 0.0
    for (int i = 0; i < 20; i++) {
        dendrite_update_structural_plasticity(dendrite, 1000 + i);
    }

    // Spine should be eliminated (state set after stability <= 0.0)
    EXPECT_EQ(dendrite->spines[spine_id].state, SPINE_STATE_ELIMINATED);
}

// ============================================================================
// Network Management Tests
// ============================================================================

/**
 * WHAT: Test dendrite network creation
 * WHY:  Verify network manager works
 * HOW:  Create network, check initialization
 */
TEST(DendriteNetworkTest, CreateNetwork) {
    dendrite_network_t* network = dendrite_network_create(10);
    ASSERT_NE(network, nullptr);

    EXPECT_EQ(network->num_dendrites, 0u);
    EXPECT_EQ(network->max_dendrites, 10u);
    EXPECT_NE(network->dendrites, nullptr);

    dendrite_network_destroy(network);
}

/**
 * WHAT: Test network timestep
 * WHY:  Verify all dendrites are updated
 * HOW:  Add dendrites to network, run step
 */
TEST(DendriteNetworkTest, NetworkStep) {
    dendrite_network_t* network = dendrite_network_create(10);
    ASSERT_NE(network, nullptr);

    // Create and add dendrites
    dendrite_config_t config = {};
    config.id = 1;
    config.type = DENDRITE_TYPE_BASAL;
    config.target_neuron_id = 100;
    config.total_length = 100.0f;
    config.mean_diameter = 2.0f;
    config.start_pos[0] = 0.0f;
    config.start_pos[1] = 0.0f;
    config.start_pos[2] = 0.0f;
    config.integration_window_ms = 20.0f;
    config.structural_plasticity = 0.01f;
    config.ltp_threshold = 0.8f;
    config.ltd_threshold = 0.3f;

    dendrite_t* d1 = dendrite_create(&config);
    ASSERT_NE(d1, nullptr);

    network->dendrites[0] = d1;
    network->num_dendrites = 1;

    // Run network step (should not crash)
    dendrite_network_step(network, 1.0f, 1000);

    SUCCEED();

    dendrite_network_destroy(network);
}

/**
 * WHAT: Test network statistics
 * WHY:  Verify stats aggregation works
 * HOW:  Create network with dendrites, get stats
 */
TEST(DendriteNetworkTest, NetworkStats) {
    dendrite_network_t* network = dendrite_network_create(10);
    ASSERT_NE(network, nullptr);

    // Get stats (should be zero for empty network)
    dendrite_network_stats_t stats = dendrite_network_get_stats(network);
    EXPECT_EQ(stats.total_dendrites, 0u);
    EXPECT_EQ(stats.total_segments, 0u);
    EXPECT_EQ(stats.total_spines, 0u);

    dendrite_network_destroy(network);
}

// ============================================================================
// Utility Tests
// ============================================================================

/**
 * WHAT: Test calculate surface area
 * WHY:  Verify morphometric calculation
 * HOW:  Create segments, calculate surface area
 */
TEST_F(DendriteTest, CalculateSurfaceArea) {
    // Create segment
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.position[0] = 0.0f;
    seg_config.position[1] = 0.0f;
    seg_config.position[2] = 0.0f;
    seg_config.length = 100.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    // Calculate surface area
    float area = dendrite_calculate_surface_area(dendrite);

    // Expected: π * diameter * length = π * 2 * 100 = ~628.3 μm²
    float expected = M_PI * 2.0f * 100.0f;
    EXPECT_NEAR(area, expected, 1.0f);

    // Surface area should be stored in dendrite
    EXPECT_FLOAT_EQ(dendrite->surface_area, area);
}

/**
 * WHAT: Test plateau detection
 * WHY:  Verify dendritic spike detection
 * HOW:  Set high voltage, check plateau state
 */
TEST_F(DendriteTest, PlateauDetection) {
    // Create segment
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.position[0] = 0.0f;
    seg_config.position[1] = 0.0f;
    seg_config.position[2] = 0.0f;
    seg_config.length = 50.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    // Initially not in plateau
    EXPECT_FALSE(dendrite_is_in_plateau(dendrite));

    // Set high voltage (plateau potential)
    dendrite->mean_voltage = 0.035f;

    // Should be in plateau
    EXPECT_TRUE(dendrite_is_in_plateau(dendrite));
}

/**
 * WHAT: Test activity statistics
 * WHY:  Verify activity tracking works
 * HOW:  Perform operations, check stats
 */
TEST_F(DendriteTest, ActivityStats) {
    // Create segment and spine
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.position[0] = 0.0f;
    seg_config.position[1] = 0.0f;
    seg_config.position[2] = 0.0f;
    seg_config.length = 50.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    uint32_t spine_id = dendrite_add_spine(dendrite, 0, SPINE_TYPE_MUSHROOM, 200);

    // Receive inputs
    dendrite_receive_input(dendrite, 0, 0.001f, 1000);
    dendrite_receive_input(dendrite, 0, 0.001f, 1001);

    // Induce LTP
    dendrite->spines[spine_id].calcium = 1.0f;
    dendrite_induce_ltp(dendrite, spine_id, 1.0f);

    // Get stats
    dendrite_activity_stats_t stats = dendrite_get_activity_stats(dendrite);

    EXPECT_EQ(stats.total_inputs, 2u);
    EXPECT_EQ(stats.ltp_events, 1u);
    EXPECT_GT(stats.mean_input_rate, 0.0f);
}
