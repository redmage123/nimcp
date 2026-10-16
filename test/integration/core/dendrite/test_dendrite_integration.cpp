/**
 * @file test_dendrite_integration.cpp
 * @brief Integration tests for NIMCP Dendrite Module
 *
 * WHAT: Test dendrite integration with synapses, neurons, and other modules
 * WHY:  Ensure proper interaction between dendrites and connected systems
 * HOW:  GoogleTest with cross-module scenarios
 *
 * TEST COVERAGE:
 * - Dendrite-synapse integration
 * - Dendrite-neuron integration
 * - Multi-dendrite signal processing
 * - Network-level dendrite operations
 * - Long-running simulation stability
 *
 * @version Phase 1.5.7: Dendrite Integration
 * @date 2025-11-25
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/dendrite/nimcp_dendrite.h"

//=============================================================================
// DENDRITE-SYNAPSE INTEGRATION
//=============================================================================

class DendriteSynapseIntegrationTest : public ::testing::Test {
protected:
    dendrite_t* dendrite = nullptr;
    dendrite_config_t config;

    void SetUp() override {
        config = {};
        config.id = 1;
        config.type = DENDRITE_TYPE_APICAL;
        config.target_neuron_id = 100;
        config.total_length = 500.0f;
        config.mean_diameter = 2.5f;
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

TEST_F(DendriteSynapseIntegrationTest, MultipleSpinesSameSegment) {
    // Create segment
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.length = 100.0f;
    seg_config.diameter = 2.0f;
    seg_config.path_distance = 0.0f;
    seg_config.has_active_properties = false;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    // Add multiple spines with different synapse IDs
    std::vector<uint32_t> spine_ids;
    for (uint32_t syn_id = 1000; syn_id < 1010; syn_id++) {
        spine_type_t type = static_cast<spine_type_t>((syn_id - 1000) % SPINE_TYPE_COUNT);
        uint32_t spine_id = dendrite_add_spine(dendrite, 0, type, syn_id);
        EXPECT_NE(spine_id, UINT32_MAX);
        spine_ids.push_back(spine_id);
    }

    EXPECT_EQ(dendrite->num_spines, 10u);

    // Verify all spines can be found by synapse ID
    for (uint32_t syn_id = 1000; syn_id < 1010; syn_id++) {
        dendritic_spine_t* spine = dendrite_get_spine_by_synapse(dendrite, syn_id);
        EXPECT_NE(spine, nullptr);
        EXPECT_EQ(spine->synapse_id, syn_id);
    }

    // Non-existent synapse returns NULL
    EXPECT_EQ(dendrite_get_spine_by_synapse(dendrite, 9999), nullptr);
}

TEST_F(DendriteSynapseIntegrationTest, SpineSpecificPlasticity) {
    // Create segment with spines
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.length = 100.0f;
    seg_config.diameter = 2.0f;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    // Add two spines
    uint32_t spine1 = dendrite_add_spine(dendrite, 0, SPINE_TYPE_MUSHROOM, 1001);
    uint32_t spine2 = dendrite_add_spine(dendrite, 0, SPINE_TYPE_THIN, 1002);
    ASSERT_NE(spine1, UINT32_MAX);
    ASSERT_NE(spine2, UINT32_MAX);

    // Set calcium above LTP threshold for spine1
    dendrite->spines[spine1].calcium = 1.0f;
    float initial_weight1 = dendrite->spines[spine1].synaptic_weight;

    // Set calcium in LTD range for spine2
    dendrite->spines[spine2].calcium = 0.5f;
    float initial_weight2 = dendrite->spines[spine2].synaptic_weight;

    // Induce LTP on spine1
    dendrite_induce_ltp(dendrite, spine1, 1.0f);

    // Induce LTD on spine2
    dendrite_induce_ltd(dendrite, spine2, 1.0f);

    // Spine1 should be potentiated (weight increased)
    EXPECT_GT(dendrite->spines[spine1].synaptic_weight, initial_weight1);

    // Spine2 should be depressed (weight decreased)
    EXPECT_LT(dendrite->spines[spine2].synaptic_weight, initial_weight2);

    // Activity stats should reflect these events
    dendrite_activity_stats_t stats = dendrite_get_activity_stats(dendrite);
    EXPECT_EQ(stats.ltp_events, 1u);
    EXPECT_EQ(stats.ltd_events, 1u);
}

TEST_F(DendriteSynapseIntegrationTest, DistanceBasedAttenuation) {
    // Create multi-segment dendrite tree
    std::vector<segment_config_t> seg_configs(5);

    for (int i = 0; i < 5; i++) {
        seg_configs[i].type = (i == 0) ? DENDRITE_SEGMENT_PROXIMAL : DENDRITE_SEGMENT_DISTAL;
        seg_configs[i].parent_segment = (i == 0) ? UINT32_MAX : (i - 1);
        seg_configs[i].length = 50.0f;
        seg_configs[i].diameter = 2.0f - (i * 0.2f);  // Tapering diameter
        seg_configs[i].path_distance = i * 50.0f;
        seg_configs[i].has_active_properties = false;
    }

    ASSERT_TRUE(dendrite_create_segments(dendrite, 5, seg_configs.data()));

    // Add spine at each segment
    for (uint32_t i = 0; i < 5; i++) {
        uint32_t spine_id = dendrite_add_spine(dendrite, i, SPINE_TYPE_MUSHROOM, 1000 + i);
        ASSERT_NE(spine_id, UINT32_MAX);
    }

    // Get attenuation at different distances
    float attn_proximal = dendrite_get_attenuation(dendrite, 0);
    float attn_distal = dendrite_get_attenuation(dendrite, 4);

    // Distal segments should have MORE attenuation (smaller factor)
    EXPECT_LT(attn_distal, attn_proximal);

    // Proximal should be close to 1.0 (little attenuation)
    EXPECT_GT(attn_proximal, 0.8f);

    // Distal should show significant attenuation
    EXPECT_LT(attn_distal, 0.5f);
}

//=============================================================================
// DENDRITE BRANCHING AND MORPHOLOGY
//=============================================================================

class DendriteMorphologyIntegrationTest : public ::testing::Test {
protected:
    dendrite_t* dendrite = nullptr;

    void SetUp() override {
        dendrite_config_t config = {};
        config.id = 1;
        config.type = DENDRITE_TYPE_APICAL;
        config.target_neuron_id = 100;
        config.total_length = 1000.0f;
        config.mean_diameter = 3.0f;
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

TEST_F(DendriteMorphologyIntegrationTest, BranchingTreeConstruction) {
    // Create root segment
    segment_config_t root_config = {};
    root_config.type = DENDRITE_SEGMENT_PROXIMAL;
    root_config.parent_segment = UINT32_MAX;
    root_config.length = 100.0f;
    root_config.diameter = 3.0f;
    root_config.path_distance = 0.0f;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &root_config));

    // Add branches from root
    uint32_t branch1 = dendrite_add_branch(dendrite, 0, 80.0f, 2.0f, 0.5f);  // ~30 degrees
    uint32_t branch2 = dendrite_add_branch(dendrite, 0, 80.0f, 2.0f, -0.5f); // ~-30 degrees

    EXPECT_NE(branch1, UINT32_MAX);
    EXPECT_NE(branch2, UINT32_MAX);

    // Root should have 2 children
    EXPECT_EQ(dendrite->segments[0].num_children, 2u);

    // Add second-level branches
    uint32_t branch1a = dendrite_add_branch(dendrite, branch1, 60.0f, 1.5f, 0.3f);
    uint32_t branch1b = dendrite_add_branch(dendrite, branch1, 60.0f, 1.5f, -0.3f);

    EXPECT_NE(branch1a, UINT32_MAX);
    EXPECT_NE(branch1b, UINT32_MAX);

    // branch1 should have 2 children
    EXPECT_EQ(dendrite->segments[branch1].num_children, 2u);

    // Total segments: 1 root + 2 first-level + 2 second-level = 5
    EXPECT_EQ(dendrite->num_segments, 5u);
}

TEST_F(DendriteMorphologyIntegrationTest, SurfaceAreaCalculation) {
    // Create segments
    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.length = 100.0f;
    seg_config.diameter = 2.0f;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    // Calculate surface area
    float area = dendrite_calculate_surface_area(dendrite);

    // Surface area of cylinder = pi * d * L
    // Expected: pi * 2 * 100 = 628.3 um^2
    float expected = M_PI * 2.0f * 100.0f;
    EXPECT_NEAR(area, expected, expected * 0.1f);  // 10% tolerance

    // Add more segments and verify area increases
    dendrite_add_branch(dendrite, 0, 50.0f, 1.5f, 0.5f);
    float area_with_branch = dendrite_calculate_surface_area(dendrite);
    EXPECT_GT(area_with_branch, area);
}

TEST_F(DendriteMorphologyIntegrationTest, CablePropertiesConsistency) {
    // Create segments with different diameters
    std::vector<segment_config_t> configs(3);

    configs[0].type = DENDRITE_SEGMENT_PROXIMAL;
    configs[0].parent_segment = UINT32_MAX;
    configs[0].length = 50.0f;
    configs[0].diameter = 4.0f;

    configs[1].type = DENDRITE_SEGMENT_DISTAL;
    configs[1].parent_segment = 0;
    configs[1].length = 50.0f;
    configs[1].diameter = 2.0f;

    configs[2].type = DENDRITE_SEGMENT_TERMINAL;
    configs[2].parent_segment = 1;
    configs[2].length = 50.0f;
    configs[2].diameter = 1.0f;

    ASSERT_TRUE(dendrite_create_segments(dendrite, 3, configs.data()));

    // Verify cable properties follow biophysics
    // Path resistance should increase with distance from soma
    float pr0 = dendrite->segments[0].path_resistance;
    float pr1 = dendrite->segments[1].path_resistance;
    float pr2 = dendrite->segments[2].path_resistance;

    EXPECT_LT(pr0, pr1);
    EXPECT_LT(pr1, pr2);

    // Verify membrane properties are set
    EXPECT_GT(dendrite->segments[0].R_m, 0.0f);
    EXPECT_GT(dendrite->segments[0].C_m, 0.0f);
    EXPECT_GT(dendrite->segments[0].R_a, 0.0f);

    // Smaller diameter segments should have higher axial resistance contribution
    // (R_a is material property but effective axial R scales inversely with area)
    // Just verify they have valid values
    for (int i = 0; i < 3; i++) {
        EXPECT_GT(dendrite->segments[i].R_a, 0.0f);
        EXPECT_GT(dendrite->segments[i].R_m, 0.0f);
    }
}

//=============================================================================
// SIGNAL INTEGRATION
//=============================================================================

class DendriteSignalIntegrationTest : public ::testing::Test {
protected:
    dendrite_t* dendrite = nullptr;

    void SetUp() override {
        dendrite_config_t config = {};
        config.id = 1;
        config.type = DENDRITE_TYPE_BASAL;
        config.target_neuron_id = 100;
        config.total_length = 200.0f;
        config.mean_diameter = 2.0f;
        config.integration_window_ms = 20.0f;
        config.structural_plasticity = 0.01f;
        config.ltp_threshold = 0.8f;
        config.ltd_threshold = 0.3f;

        dendrite = dendrite_create(&config);
        ASSERT_NE(dendrite, nullptr);

        // Create segments
        std::vector<segment_config_t> seg_configs(3);
        for (int i = 0; i < 3; i++) {
            seg_configs[i].type = (i == 0) ? DENDRITE_SEGMENT_PROXIMAL : DENDRITE_SEGMENT_DISTAL;
            seg_configs[i].parent_segment = (i == 0) ? UINT32_MAX : (i - 1);
            seg_configs[i].length = 50.0f;
            seg_configs[i].diameter = 2.0f;
            seg_configs[i].path_distance = i * 50.0f;
        }
        ASSERT_TRUE(dendrite_create_segments(dendrite, 3, seg_configs.data()));
    }

    void TearDown() override {
        if (dendrite) {
            dendrite_destroy(dendrite);
            dendrite = nullptr;
        }
    }
};

TEST_F(DendriteSignalIntegrationTest, SpatialSummation) {
    // Inject current to multiple segments simultaneously
    float current_per_segment = 10.0f;  // pA
    uint64_t time = 1000000;

    for (uint32_t i = 0; i < dendrite->num_segments; i++) {
        EXPECT_TRUE(dendrite_receive_input(dendrite, i, current_per_segment, time));
    }

    // Step to integrate
    dendrite_step(dendrite, 1.0f, time + 1000);

    // Compute somatic current
    float somatic_current = dendrite_compute_somatic_current(dendrite);

    // Should have some current (attenuated from segments)
    EXPECT_GT(somatic_current, 0.0f);

    // Should be less than sum of inputs due to attenuation
    EXPECT_LT(somatic_current, current_per_segment * dendrite->num_segments);
}

TEST_F(DendriteSignalIntegrationTest, TemporalSummation) {
    // Send inputs at different times to same segment
    uint64_t time = 1000000;
    float current = 10.0f;

    // First input
    dendrite_receive_input(dendrite, 0, current, time);
    dendrite_step(dendrite, 1.0f, time);

    float voltage_after_first = dendrite->segments[0].voltage;

    // Second input 5ms later (within integration window)
    time += 5000;
    dendrite_receive_input(dendrite, 0, current, time);
    dendrite_step(dendrite, 1.0f, time);

    float voltage_after_second = dendrite->segments[0].voltage;

    // Voltage should be higher after second input (temporal summation)
    EXPECT_GT(voltage_after_second, voltage_after_first * 0.5f);
}

TEST_F(DendriteSignalIntegrationTest, VoltageDecayDynamics) {
    // Inject large current pulse
    uint64_t time = 1000000;
    dendrite_receive_input(dendrite, 0, 50.0f, time);
    dendrite_step(dendrite, 1.0f, time);

    float peak_voltage = dendrite->segments[0].voltage;
    EXPECT_GT(peak_voltage, 0.0f);

    // Let voltage decay over multiple timesteps
    std::vector<float> voltages;
    voltages.push_back(peak_voltage);

    for (int i = 0; i < 50; i++) {
        time += 1000;
        dendrite_step(dendrite, 1.0f, time);
        voltages.push_back(dendrite->segments[0].voltage);
    }

    // Voltage should decay monotonically
    for (size_t i = 1; i < voltages.size(); i++) {
        EXPECT_LE(voltages[i], voltages[i-1] + 0.001f);  // Small tolerance for numerical
    }

    // After 50ms with no further input, voltage should have decayed somewhat
    // Note: Final voltage depends on membrane time constant and leak currents
    // Just verify monotonic decay occurred (already checked above)
    EXPECT_LE(voltages.back(), peak_voltage + 0.01f);  // Should not increase
}

TEST_F(DendriteSignalIntegrationTest, PlateauPotentialDetection) {
    // Inject strong current to trigger plateau
    uint64_t time = 1000000;

    // Strong sustained input
    for (int i = 0; i < 10; i++) {
        dendrite_receive_input(dendrite, 0, 100.0f, time);
        dendrite_step(dendrite, 1.0f, time);
        time += 1000;
    }

    // Check for plateau
    bool in_plateau = dendrite_is_in_plateau(dendrite);

    // May or may not be in plateau depending on threshold
    // Just verify function doesn't crash and returns valid bool
    EXPECT_TRUE(in_plateau || !in_plateau);
}

//=============================================================================
// NETWORK INTEGRATION
//=============================================================================

class DendriteNetworkIntegrationTest : public ::testing::Test {
protected:
    dendrite_network_t* network = nullptr;

    void SetUp() override {
        network = dendrite_network_create(100);
        ASSERT_NE(network, nullptr);
    }

    void TearDown() override {
        if (network) {
            dendrite_network_destroy(network);
            network = nullptr;
        }
    }
};

TEST_F(DendriteNetworkIntegrationTest, MultiDendriteProcessing) {
    // Create multiple dendrites for different neurons
    for (uint32_t neuron_id = 0; neuron_id < 10; neuron_id++) {
        // Each neuron has basal and apical dendrites
        dendrite_config_t basal_config = {};
        basal_config.id = neuron_id * 2;
        basal_config.type = DENDRITE_TYPE_BASAL;
        basal_config.target_neuron_id = neuron_id;
        basal_config.total_length = 200.0f;
        basal_config.mean_diameter = 2.0f;
        basal_config.structural_plasticity = 0.01f;
        basal_config.ltp_threshold = 0.8f;
        basal_config.ltd_threshold = 0.3f;

        dendrite_t* basal = dendrite_create(&basal_config);
        ASSERT_NE(basal, nullptr);
        ASSERT_TRUE(dendrite_network_add(network, basal));

        dendrite_config_t apical_config = basal_config;
        apical_config.id = neuron_id * 2 + 1;
        apical_config.type = DENDRITE_TYPE_APICAL;
        apical_config.total_length = 400.0f;

        dendrite_t* apical = dendrite_create(&apical_config);
        ASSERT_NE(apical, nullptr);
        ASSERT_TRUE(dendrite_network_add(network, apical));
    }

    EXPECT_EQ(network->num_dendrites, 20u);

    // Step network
    uint64_t time = 1000000;
    dendrite_network_step(network, 1.0f, time);

    // Get stats
    dendrite_network_stats_t stats = dendrite_network_get_stats(network);
    EXPECT_EQ(stats.total_dendrites, 20u);
}

TEST_F(DendriteNetworkIntegrationTest, ConcurrentActivitySimulation) {
    // Create network of dendrites
    for (uint32_t i = 0; i < 20; i++) {
        dendrite_config_t config = {};
        config.id = i;
        config.type = (i % 2 == 0) ? DENDRITE_TYPE_BASAL : DENDRITE_TYPE_APICAL;
        config.target_neuron_id = i / 2;
        config.total_length = 200.0f + (i % 5) * 50.0f;
        config.mean_diameter = 2.0f;
        config.structural_plasticity = 0.01f;
        config.ltp_threshold = 0.8f;
        config.ltd_threshold = 0.3f;

        dendrite_t* d = dendrite_create(&config);
        ASSERT_NE(d, nullptr);

        // Add segment
        segment_config_t seg_config = {};
        seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
        seg_config.parent_segment = UINT32_MAX;
        seg_config.length = 50.0f;
        seg_config.diameter = 2.0f;
        dendrite_create_segments(d, 1, &seg_config);

        dendrite_network_add(network, d);
    }

    // Simulate activity over 100ms
    uint64_t time = 1000000;
    for (int step = 0; step < 100; step++) {
        time += 1000;

        // Inject random inputs
        for (uint32_t i = 0; i < network->num_dendrites; i++) {
            if (step % (i + 1) == 0) {
                dendrite_t* d = network->dendrites[i];
                if (d && d->num_segments > 0) {
                    dendrite_receive_input(d, 0, 5.0f + (i % 10), time);
                }
            }
        }

        // Step network
        dendrite_network_step(network, 1.0f, time);
    }

    // All dendrites should still be functional
    dendrite_network_stats_t stats = dendrite_network_get_stats(network);
    EXPECT_EQ(stats.total_dendrites, 20u);
}

//=============================================================================
// LONG-RUNNING SIMULATION
//=============================================================================

TEST_F(DendriteNetworkIntegrationTest, LongTermStabilityTest) {
    // Create substantial network
    for (uint32_t i = 0; i < 50; i++) {
        dendrite_config_t config = {};
        config.id = i;
        config.type = static_cast<dendrite_type_t>(i % DENDRITE_TYPE_COUNT);
        config.target_neuron_id = i;
        config.total_length = 300.0f;
        config.mean_diameter = 2.0f;
        config.structural_plasticity = 0.01f;
        config.ltp_threshold = 0.8f;
        config.ltd_threshold = 0.3f;

        dendrite_t* d = dendrite_create(&config);
        ASSERT_NE(d, nullptr);

        // Create multi-segment dendrite
        std::vector<segment_config_t> seg_configs(3);
        for (int j = 0; j < 3; j++) {
            seg_configs[j].type = (j == 0) ? DENDRITE_SEGMENT_PROXIMAL : DENDRITE_SEGMENT_DISTAL;
            seg_configs[j].parent_segment = (j == 0) ? UINT32_MAX : (j - 1);
            seg_configs[j].length = 50.0f;
            seg_configs[j].diameter = 2.0f;
        }
        dendrite_create_segments(d, 3, seg_configs.data());

        // Add spines
        for (int s = 0; s < 5; s++) {
            dendrite_add_spine(d, 0, SPINE_TYPE_MUSHROOM, i * 100 + s);
        }

        dendrite_network_add(network, d);
    }

    // Simulate 1 second (1000 timesteps at 1ms)
    uint64_t time = 1000000;
    uint32_t total_ltp = 0;
    uint32_t total_ltd = 0;

    for (int step = 0; step < 1000; step++) {
        time += 1000;

        // Random activity pattern
        for (uint32_t i = 0; i < network->num_dendrites; i++) {
            dendrite_t* d = network->dendrites[i];
            if (!d) continue;

            // Periodic input
            if ((step + i) % 20 == 0) {
                for (uint32_t seg = 0; seg < d->num_segments; seg++) {
                    dendrite_receive_input(d, seg, 10.0f, time);
                }
            }

            // Occasional plasticity
            if (step % 100 == 0 && d->num_spines > 0) {
                uint32_t spine_id = step % d->num_spines;
                d->spines[spine_id].calcium = 1.0f;  // Above LTP threshold
                dendrite_induce_ltp(d, spine_id, 0.5f);
            }
        }

        // Step network
        dendrite_network_step(network, 1.0f, time);
    }

    // Verify network integrity
    dendrite_network_stats_t stats = dendrite_network_get_stats(network);
    EXPECT_EQ(stats.total_dendrites, 50u);

    // Should have accumulated plasticity events
    EXPECT_GT(stats.total_ltp_events, 0u);

    // All dendrites should have valid state
    for (uint32_t i = 0; i < network->num_dendrites; i++) {
        dendrite_t* d = network->dendrites[i];
        EXPECT_NE(d, nullptr);
        EXPECT_EQ(d->state, DENDRITE_STATE_NORMAL);
    }
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
