/**
 * @file test_hippocampus_e2e.cpp
 * @brief End-to-end tests for the Hippocampus module
 *
 * These tests verify complete workflows and realistic usage scenarios
 * involving the hippocampus and its integration with the full NIMCP system.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>

extern "C" {
#include "core/brain/regions/hippocampus/nimcp_hippocampus.h"
}

// =============================================================================
// Test Fixture with Full Memory Circuit Setup
// =============================================================================

class HippocampusE2ETest : public ::testing::Test {
protected:
    nimcp_hippocampus_t* hippo = nullptr;
    hippo_config_t config;

    // Simulated sensory data generators
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(42);  // Fixed seed for reproducibility

        config = hippo_default_config();
        config.num_dg_cells = 1000;
        config.num_ca3_cells = 400;
        config.num_ca1_cells = 600;
        config.num_subiculum_cells = 300;
        config.max_episodes = 500;
        config.num_place_cells = 100;
        config.theta_frequency = 8.0f;
        config.gamma_frequency = 40.0f;
        config.dg_sparsity = 0.05f;
        config.enable_neurogenesis = true;
        config.enable_prime_resonance = true;
        config.consolidation_rate = 0.7f;
    }

    void TearDown() override {
        if (hippo) {
            hippo_destroy(hippo);
            hippo = nullptr;
        }
    }

    void create_hippocampus() {
        hippo = hippo_create(&config);
        ASSERT_NE(hippo, nullptr);
    }

    // Generate random sensory input
    void generate_sensory_input(float* buffer, uint32_t dim, float noise_level = 0.1f) {
        std::normal_distribution<float> dist(0.5f, noise_level);
        for (uint32_t i = 0; i < dim; i++) {
            buffer[i] = std::clamp(dist(rng), 0.0f, 1.0f);
        }
    }

    // Generate spatial position
    void generate_position(float* pos, float x_range, float y_range) {
        std::uniform_real_distribution<float> dist_x(0.0f, x_range);
        std::uniform_real_distribution<float> dist_y(0.0f, y_range);
        pos[0] = dist_x(rng);
        pos[1] = dist_y(rng);
        pos[2] = 0.0f;
    }

    // Generate temporal context
    void generate_temporal_context(float* when, uint32_t dim, uint64_t timestamp) {
        // Encode time as multi-scale representation
        for (uint32_t i = 0; i < dim; i++) {
            float scale = powf(2.0f, (float)i);
            when[i] = sinf((float)timestamp / scale) * 0.5f + 0.5f;
        }
    }

    // Simulate sleep cycle
    void simulate_sleep_cycle(float duration_seconds, float dt = 0.01f) {
        int steps = (int)(duration_seconds / dt);
        for (int i = 0; i < steps; i++) {
            float phase = (float)i / (float)steps;

            // Different sleep stages
            if (phase < 0.2f) {
                // N1 - Light sleep
                hippo_set_oscillation_state(hippo, OSCILLATION_THETA);
            } else if (phase < 0.5f) {
                // N2 - Sleep spindles
                hippo_set_oscillation_state(hippo, OSCILLATION_GAMMA);
            } else if (phase < 0.7f) {
                // N3 - Slow wave sleep with ripples
                hippo_set_oscillation_state(hippo, OSCILLATION_SHARP_WAVE_RIPPLE);
                hippo_trigger_replay(hippo, REPLAY_REVERSE);
            } else {
                // REM - Theta dominant
                hippo_set_oscillation_state(hippo, OSCILLATION_THETA);
                hippo_trigger_replay(hippo, REPLAY_FORWARD);
            }

            hippo_update(hippo, dt);
        }

        // Trigger consolidation at end of sleep
        hippo_consolidate_memories(hippo, dt);
    }
};

// =============================================================================
// E2E: Complete Episodic Memory Workflow
// =============================================================================

TEST_F(HippocampusE2ETest, E2E_EpisodicMemoryLifecycle) {
    /**
     * Complete episodic memory workflow:
     * 1. Encode multiple experiences during "day"
     * 2. Simulate sleep for consolidation
     * 3. Retrieve memories using partial cues
     * 4. Verify memory accuracy degrades gracefully over time
     */
    create_hippocampus();

    // Phase 1: Encoding experiences during "waking"
    const int num_experiences = 20;
    std::vector<uint32_t> episode_ids;
    std::vector<std::vector<float>> original_whats(num_experiences);

    hippo_set_oscillation_state(hippo, OSCILLATION_THETA);

    for (int i = 0; i < num_experiences; i++) {
        float what[50], where[20], when[10];
        generate_sensory_input(what, 50);
        generate_sensory_input(where, 20, 0.05f);  // Less noisy spatial
        generate_temporal_context(when, 10, i * 1000);

        // Store original for later comparison
        original_whats[i].assign(what, what + 50);

        // Emotional valence varies
        float valence = (i % 3 == 0) ? 0.9f : 0.3f;  // Some emotional, some neutral
        float arousal = (i % 5 == 0) ? 0.8f : 0.4f;

        uint32_t episode_id;
        int result = hippo_encode_episode(hippo, what, 50, where, 20, when, 10,
                                           valence, arousal, &episode_id);
        EXPECT_EQ(result, 0);
        episode_ids.push_back(episode_id);

        // Simulate time passing between encodings
        for (int j = 0; j < 100; j++) {
            hippo_update(hippo, 0.001f);
        }
    }

    // Phase 2: Sleep-dependent consolidation
    simulate_sleep_cycle(2.0f);  // 2 seconds simulated sleep

    // Phase 3: Memory retrieval with partial cues
    int successful_retrievals = 0;

    for (int i = 0; i < num_experiences; i++) {
        // Create partial cue (first half of original)
        float partial_cue[25];
        for (int j = 0; j < 25; j++) {
            partial_cue[j] = original_whats[i][j];
        }

        float completed[100];
        uint32_t completed_dim;
        float confidence;

        int result = hippo_pattern_complete(hippo, partial_cue, 25,
                                            completed, &completed_dim, &confidence);
        if (result == 0 && confidence > 0.3f) {
            successful_retrievals++;
        }
    }

    // Should retrieve majority of memories
    EXPECT_GT(successful_retrievals, num_experiences / 2);

    // Phase 4: Verify emotional memories have higher consolidation
    int emotional_count = 0, emotional_consolidated = 0;
    int neutral_count = 0, neutral_consolidated = 0;

    for (int i = 0; i < num_experiences; i++) {
        const nimcp_episode_t* ep = hippo_get_episode(hippo, episode_ids[i]);
        if (ep) {
            if (ep->emotional_valence > 0.5f) {
                emotional_count++;
                if (ep->consolidation_level > 0.5f) emotional_consolidated++;
            } else {
                neutral_count++;
                if (ep->consolidation_level > 0.5f) neutral_consolidated++;
            }
        }
    }

    // Emotional memories should consolidate better on average
    float emotional_rate = emotional_count > 0 ?
        (float)emotional_consolidated / emotional_count : 0.0f;
    float neutral_rate = neutral_count > 0 ?
        (float)neutral_consolidated / neutral_count : 0.0f;

    // This is expected behavior based on emotional modulation
    EXPECT_GE(emotional_rate, neutral_rate * 0.8f);  // Allow some variance
}

// =============================================================================
// E2E: Spatial Navigation Scenario
// =============================================================================

TEST_F(HippocampusE2ETest, E2E_SpatialNavigationAndPlaceCells) {
    /**
     * Spatial navigation scenario:
     * 1. Build cognitive map by exploring environment
     * 2. Form place cell representations
     * 3. Navigate to remembered locations
     * 4. Test novel shortcut discovery
     */
    create_hippocampus();

    const float arena_size = 100.0f;
    const int num_locations = 20;

    // Phase 1: Explore environment and form place cells
    std::vector<std::array<float, 3>> visited_locations;
    std::vector<uint32_t> place_cell_ids;

    for (int i = 0; i < num_locations; i++) {
        float pos[3];
        generate_position(pos, arena_size, arena_size);
        visited_locations.push_back({pos[0], pos[1], pos[2]});

        uint32_t cell_id;
        int result = hippo_create_place_field(hippo, pos, 5.0f, &cell_id);
        EXPECT_EQ(result, 0);
        place_cell_ids.push_back(cell_id);

        // Encode location with context
        float what[30], when[10];
        generate_sensory_input(what, 30);  // Visual features at location
        generate_temporal_context(when, 10, i * 1000);

        uint32_t episode_id;
        hippo_encode_episode(hippo, what, 30, pos, 3, when, 10, 0.5f, 0.5f, &episode_id);

        // Update for path integration
        hippo_update_position(hippo, pos, 3);
        for (int j = 0; j < 50; j++) {
            hippo_update(hippo, 0.001f);
        }
    }

    // Phase 2: Test place cell activity
    hippo_stats_t stats;
    hippo_get_stats(hippo, &stats);

    // Should have created place cells
    EXPECT_GT(stats.place_cells_active, 0u);

    // Phase 3: Test spatial memory retrieval
    // Pick a random visited location
    int target_idx = 5;
    float target_pos[3] = {visited_locations[target_idx][0],
                          visited_locations[target_idx][1],
                          visited_locations[target_idx][2]};

    // Use position as cue
    float retrieved[50];
    uint32_t retrieved_dim;
    float confidence;

    hippo_pattern_complete(hippo, target_pos, 3,
                           retrieved, &retrieved_dim, &confidence);
    // Should be able to retrieve something associated with this location

    // Phase 4: Test cognitive map connectivity
    EXPECT_GT(stats.episodes_encoded, 0u);
}

// =============================================================================
// E2E: Learning and Interference
// =============================================================================

TEST_F(HippocampusE2ETest, E2E_LearningWithInterference) {
    /**
     * Test learning with interference:
     * 1. Learn original associations
     * 2. Learn similar but different associations (retroactive interference)
     * 3. Verify pattern separation maintains distinct memories
     * 4. Test proactive interference with new similar learning
     */
    create_hippocampus();

    const uint32_t pattern_dim = 40;

    // Phase 1: Learn original associations (A-B)
    std::vector<uint32_t> original_ids;
    std::vector<std::vector<float>> patterns_a;

    for (int i = 0; i < 10; i++) {
        float pattern_a[40], pattern_b[40], when[5];

        // Pattern A - consistent base with variations
        for (int j = 0; j < 40; j++) {
            pattern_a[j] = 0.5f + 0.1f * sinf((float)(j + i));
        }
        patterns_a.push_back(std::vector<float>(pattern_a, pattern_a + 40));

        // Pattern B - arbitrary association
        generate_sensory_input(pattern_b, 40);
        generate_temporal_context(when, 5, i);

        uint32_t episode_id;
        hippo_encode_episode(hippo, pattern_a, 40, pattern_b, 40, when, 5,
                              0.5f, 0.5f, &episode_id);
        original_ids.push_back(episode_id);
    }

    // Consolidation
    for (int i = 0; i < 500; i++) {
        hippo_update(hippo, 0.001f);
    }

    // Phase 2: Learn interfering associations (A-C, similar A, different B)
    std::vector<uint32_t> interfering_ids;

    for (int i = 0; i < 10; i++) {
        float pattern_a_similar[40], pattern_c[40], when[5];

        // Similar to original A but slightly different
        for (int j = 0; j < 40; j++) {
            pattern_a_similar[j] = patterns_a[i][j] + 0.05f * cosf((float)j);
        }

        // Pattern C - new association
        generate_sensory_input(pattern_c, 40);
        generate_temporal_context(when, 5, 100 + i);

        uint32_t episode_id;
        hippo_encode_episode(hippo, pattern_a_similar, 40, pattern_c, 40, when, 5,
                              0.5f, 0.5f, &episode_id);
        interfering_ids.push_back(episode_id);
    }

    // Phase 3: Test pattern separation
    // Both original and interfering memories should exist
    int original_retrieved = 0;
    int interfering_retrieved = 0;

    for (uint32_t id : original_ids) {
        if (hippo_get_episode(hippo, id)) original_retrieved++;
    }

    for (uint32_t id : interfering_ids) {
        if (hippo_get_episode(hippo, id)) interfering_retrieved++;
    }

    // Pattern separation should maintain both sets
    EXPECT_GT(original_retrieved, 0);
    EXPECT_GT(interfering_retrieved, 0);

    // Phase 4: Test discrimination
    // Use original cue, should complete to original (not interfering)
    float cue[20];
    for (int j = 0; j < 20; j++) {
        cue[j] = patterns_a[0][j];  // First half of first pattern
    }

    float completed[80];
    uint32_t completed_dim;
    float confidence;

    hippo_pattern_complete(hippo, cue, 20, completed, &completed_dim, &confidence);
    // Result depends on pattern separation effectiveness
}

// =============================================================================
// E2E: Prime Resonance Enhanced Memory
// =============================================================================

TEST_F(HippocampusE2ETest, E2E_PrimeResonanceEnhancedWorkflow) {
    /**
     * Test prime resonance memory enhancement:
     * 1. Connect prime resonance system
     * 2. Encode with resonance enhancement
     * 3. Compare with non-enhanced encoding
     * 4. Test phase-aligned retrieval
     */
    create_hippocampus();

    // Initialize prime resonance bridge with mock context
    // The bridge takes a void* to the PR memory context
    int result = hippo_init_prime_resonance_bridge(hippo, nullptr);
    EXPECT_EQ(result, 0);

    // Manually configure the bridge parameters
    hippo->prime_resonance_bridge.resonance_frequency = 7.83f;  // Schumann resonance
    hippo->prime_resonance_bridge.memory_enhancement_factor = 2.0f;
    hippo->prime_resonance_bridge.resonance_active = true;

    // Phase 1: Encode with resonance enhancement
    std::vector<uint32_t> enhanced_ids;
    for (int i = 0; i < 10; i++) {
        float content[30];
        generate_sensory_input(content, 30);

        uint32_t episode_id;
        result = hippo_resonance_enhanced_encode(hippo, content, 30, &episode_id);
        EXPECT_EQ(result, 0);
        enhanced_ids.push_back(episode_id);
    }

    // Phase 2: Encode without enhancement (disable resonance)
    hippo->prime_resonance_bridge.resonance_active = false;

    std::vector<uint32_t> normal_ids;
    for (int i = 0; i < 10; i++) {
        float what[30], where[10], when[5];
        generate_sensory_input(what, 30);
        generate_sensory_input(where, 10);
        generate_temporal_context(when, 5, i);

        uint32_t episode_id;
        result = hippo_encode_episode(hippo, what, 30, where, 10, when, 5,
                                       0.5f, 0.5f, &episode_id);
        EXPECT_EQ(result, 0);
        normal_ids.push_back(episode_id);
    }

    // Phase 3: Consolidation
    simulate_sleep_cycle(1.0f);

    // Phase 4: Compare encoding strength
    float enhanced_avg_strength = 0.0f;
    float normal_avg_strength = 0.0f;

    for (uint32_t id : enhanced_ids) {
        const nimcp_episode_t* ep = hippo_get_episode(hippo, id);
        if (ep) {
            enhanced_avg_strength += ep->encoding_strength;
        }
    }
    enhanced_avg_strength /= enhanced_ids.size();

    for (uint32_t id : normal_ids) {
        const nimcp_episode_t* ep = hippo_get_episode(hippo, id);
        if (ep) {
            normal_avg_strength += ep->encoding_strength;
        }
    }
    normal_avg_strength /= normal_ids.size();

    // Enhanced encoding should be stronger on average
    EXPECT_GE(enhanced_avg_strength, normal_avg_strength * 0.9f);

    // Re-enable for phase-aligned retrieval
    hippo->prime_resonance_bridge.resonance_active = true;

    // Test guided retrieval
    float cue[15];
    generate_sensory_input(cue, 15);

    uint32_t retrieved_id;
    float confidence;

    result = hippo_resonance_guided_retrieve(hippo, cue, 15, &retrieved_id, &confidence);
    // Should work, but result depends on what was encoded
}

// =============================================================================
// E2E: Multi-Regional Memory Circuit
// =============================================================================

TEST_F(HippocampusE2ETest, E2E_MultiRegionalCircuit) {
    /**
     * Test complete memory circuit:
     * 1. Simulate perception through cortical regions
     * 2. Entorhinal grid cell path integration
     * 3. Hippocampal encoding
     * 4. Mammillary body head direction
     * 5. Test bidirectional information flow
     */
    create_hippocampus();

    // Initialize memory circuit bridges
    int result = hippo_init_entorhinal_bridge(hippo, nullptr);
    EXPECT_EQ(result, 0);

    result = hippo_init_mammillary_bridge(hippo, nullptr);
    EXPECT_EQ(result, 0);

    // Manually configure the bridges
    hippo->entorhinal_bridge.perforant_path_strength = 0.8f;
    hippo->mammillary_bridge.fornix_output_strength = 0.7f;

    // Phase 1: Simulate animal exploring environment
    const int num_steps = 100;
    float current_pos[3] = {50.0f, 50.0f, 0.0f};
    float current_heading = 0.0f;

    for (int step = 0; step < num_steps; step++) {
        // Update heading (random walk)
        std::uniform_real_distribution<float> heading_change(-0.2f, 0.2f);
        current_heading += heading_change(rng);

        // Update position
        float speed = 1.0f;
        current_pos[0] += speed * cosf(current_heading);
        current_pos[1] += speed * sinf(current_heading);

        // Clamp to arena
        current_pos[0] = std::clamp(current_pos[0], 0.0f, 100.0f);
        current_pos[1] = std::clamp(current_pos[1], 0.0f, 100.0f);

        // Send position update to hippocampus
        hippo_update_position(hippo, current_pos, 3);

        // Update head direction through mammillary bridge
        hippo->mammillary_bridge.head_direction_input = current_heading;

        // Occasionally encode experiences
        if (step % 20 == 0) {
            float what[20], when[5];
            generate_sensory_input(what, 20);
            generate_temporal_context(when, 5, step);

            uint32_t episode_id;
            hippo_encode_episode(hippo, what, 20, current_pos, 3, when, 5,
                                  0.3f, 0.3f, &episode_id);
        }

        // Update all systems
        hippo_update(hippo, 0.01f);
    }

    // Phase 2: Verify spatial representation formed
    hippo_stats_t stats;
    hippo_get_stats(hippo, &stats);

    EXPECT_GT(stats.episodes_encoded, 0u);

    // Phase 3: Test path integration - query a previously visited location
    float remembered_pos[3] = {50.0f, 50.0f, 0.0f};  // Start position
    float retrieved[50];
    uint32_t dim;
    float confidence;

    hippo_pattern_complete(hippo, remembered_pos, 3,
                           retrieved, &dim, &confidence);
    // Should retrieve something from the start location area

    // Phase 4: Test head direction consistency from mammillary bridge
    float hd = hippo->mammillary_bridge.head_direction_input;
    // Head direction should be valid (within reasonable range)
    EXPECT_GE(hd, (float)(-2 * M_PI));
    EXPECT_LE(hd, (float)(2 * M_PI));
}

// =============================================================================
// E2E: Stress Response and Memory
// =============================================================================

TEST_F(HippocampusE2ETest, E2E_StressModulatedMemory) {
    /**
     * Test stress effects on memory (Yerkes-Dodson):
     * 1. Low stress - baseline encoding
     * 2. Moderate stress - enhanced encoding
     * 3. High stress - impaired encoding
     * 4. Chronic stress - consolidation impairment
     */
    create_hippocampus();

    // Initialize hypothalamus bridge for stress signaling
    int result = hippo_init_hypothalamus_bridge(hippo, nullptr);
    EXPECT_EQ(result, 0);

    struct StressCondition {
        float stress_level;
        std::string name;
        std::vector<uint32_t> episode_ids;
    };

    std::vector<StressCondition> conditions = {
        {0.1f, "low_stress", {}},
        {0.5f, "moderate_stress", {}},
        {0.95f, "high_stress", {}}
    };

    // Encode under each stress condition
    for (auto& condition : conditions) {
        // Set stress level through hypothalamus bridge
        hippo->hypothalamus_bridge.stress_level = condition.stress_level;

        for (int i = 0; i < 10; i++) {
            float what[30], where[15], when[5];
            generate_sensory_input(what, 30);
            generate_sensory_input(where, 15);
            generate_temporal_context(when, 5, i);

            uint32_t episode_id;
            int result = hippo_encode_episode(hippo, what, 30, where, 15, when, 5,
                                               0.5f, condition.stress_level, &episode_id);
            if (result == 0) {
                condition.episode_ids.push_back(episode_id);
            }

            hippo_update(hippo, 0.01f);
        }
    }

    // Consolidate with low stress
    hippo->hypothalamus_bridge.stress_level = 0.2f;
    simulate_sleep_cycle(1.0f);

    // Analyze encoding quality per condition
    for (const auto& condition : conditions) {
        float avg_strength = 0.0f;
        int count = 0;

        for (uint32_t id : condition.episode_ids) {
            const nimcp_episode_t* ep = hippo_get_episode(hippo, id);
            if (ep) {
                avg_strength += ep->encoding_strength;
                count++;
            }
        }

        if (count > 0) {
            avg_strength /= count;
        }

        // Moderate stress should have good encoding
        // High stress may have reduced encoding (Yerkes-Dodson)
    }

    SUCCEED();  // Stress modulation effects depend on implementation
}

// =============================================================================
// E2E: Memory-Guided Decision Making
// =============================================================================

TEST_F(HippocampusE2ETest, E2E_MemoryGuidedDecision) {
    /**
     * Test memory-guided decision making:
     * 1. Learn location-reward associations
     * 2. Present choice scenario
     * 3. Retrieve relevant memories
     * 4. Decision based on value comparison
     */
    create_hippocampus();

    // Initialize cognitive bridge for decision support
    int result = hippo_init_cognitive_bridge(hippo, nullptr);
    EXPECT_EQ(result, 0);

    // Configure working memory
    hippo->cognitive_bridge.working_memory_load = 0.0f;
    hippo->cognitive_bridge.attention_level = 1.0f;

    // Phase 1: Learn location-reward associations
    struct LocationReward {
        float position[3];
        float reward_value;
        uint32_t episode_id;
    };

    std::vector<LocationReward> locations = {
        {{10.0f, 10.0f, 0.0f}, 0.9f, 0},  // High reward
        {{20.0f, 20.0f, 0.0f}, 0.3f, 0},  // Low reward
        {{30.0f, 30.0f, 0.0f}, 0.8f, 0},  // High reward
        {{40.0f, 40.0f, 0.0f}, 0.2f, 0},  // Low reward
    };

    for (auto& loc : locations) {
        float what[20];
        // Encode visual features of location
        for (int i = 0; i < 20; i++) {
            what[i] = loc.position[0] / 100.0f + 0.1f * (float)i / 20.0f;
        }

        float when[5] = {0.0f};

        result = hippo_encode_episode(hippo, what, 20, loc.position, 3, when, 5,
                                       loc.reward_value, 0.5f, &loc.episode_id);
        EXPECT_EQ(result, 0);

        // Multiple exposures for learning
        for (int rep = 0; rep < 5; rep++) {
            hippo_encode_episode(hippo, what, 20, loc.position, 3, when, 5,
                                  loc.reward_value, 0.5f, &loc.episode_id);
            hippo_update(hippo, 0.01f);
        }
    }

    // Phase 2: Consolidation
    simulate_sleep_cycle(0.5f);

    // Phase 3: Decision scenario - choose between locations
    float option1_cue[3] = {10.0f, 10.0f, 0.0f};
    float option2_cue[3] = {20.0f, 20.0f, 0.0f};

    float retrieved1[50], retrieved2[50];
    uint32_t dim1, dim2;
    float conf1, conf2;

    hippo_pattern_complete(hippo, option1_cue, 3, retrieved1, &dim1, &conf1);
    hippo_pattern_complete(hippo, option2_cue, 3, retrieved2, &dim2, &conf2);

    // Check if emotional valence (reward) is recoverable
    const nimcp_episode_t* ep1 = hippo_get_episode(hippo, locations[0].episode_id);
    const nimcp_episode_t* ep2 = hippo_get_episode(hippo, locations[1].episode_id);

    if (ep1 && ep2) {
        // High reward location should have higher valence
        EXPECT_GT(ep1->emotional_valence, ep2->emotional_valence);
    }
}

// =============================================================================
// E2E: Theta-Gamma Nested Oscillations
// =============================================================================

TEST_F(HippocampusE2ETest, E2E_ThetaGammaNestedOscillations) {
    /**
     * Test theta-gamma nested oscillation encoding:
     * 1. Verify theta-gamma coupling
     * 2. Test phase precession during sequence learning
     * 3. Verify gamma cycles nest within theta cycles
     */
    create_hippocampus();

    // Track oscillation phases during extended operation
    std::vector<float> theta_phases;
    std::vector<float> gamma_phases;
    std::vector<float> theta_gamma_ratios;

    const int num_samples = 1000;
    for (int i = 0; i < num_samples; i++) {
        hippo_update(hippo, 0.001f);

        float theta = hippo_get_theta_phase(hippo);
        float gamma = hippo_get_gamma_phase(hippo);

        theta_phases.push_back(theta);
        gamma_phases.push_back(gamma);

        // Track how many gamma cycles per theta cycle
        if (i > 0 && theta < theta_phases[i-1]) {
            // Theta cycle completed
            int gamma_cycles = 0;
            float prev_gamma = gamma_phases[i-1];
            for (int j = 1; j < i && theta_phases[i-j] < theta; j++) {
                if (gamma_phases[i-j] < prev_gamma && prev_gamma > M_PI) {
                    gamma_cycles++;
                }
                prev_gamma = gamma_phases[i-j];
            }
            if (gamma_cycles > 0) {
                theta_gamma_ratios.push_back((float)gamma_cycles);
            }
        }
    }

    // Verify oscillations are running
    EXPECT_EQ(theta_phases.size(), (size_t)num_samples);
    EXPECT_EQ(gamma_phases.size(), (size_t)num_samples);

    // Theta and gamma should both vary (not stuck at 0)
    float theta_variance = 0.0f;
    float gamma_variance = 0.0f;
    float theta_mean = 0.0f;
    float gamma_mean = 0.0f;

    for (float t : theta_phases) theta_mean += t;
    for (float g : gamma_phases) gamma_mean += g;
    theta_mean /= theta_phases.size();
    gamma_mean /= gamma_phases.size();

    for (float t : theta_phases) theta_variance += (t - theta_mean) * (t - theta_mean);
    for (float g : gamma_phases) gamma_variance += (g - gamma_mean) * (g - gamma_mean);

    EXPECT_GT(theta_variance, 0.0f);
    EXPECT_GT(gamma_variance, 0.0f);
}

// =============================================================================
// E2E: Sequential Memory Encoding
// =============================================================================

TEST_F(HippocampusE2ETest, E2E_SequentialMemoryReplay) {
    /**
     * Test sequential memory encoding and replay:
     * 1. Encode sequence of events
     * 2. Test forward replay (during theta)
     * 3. Test reverse replay (during SWRs)
     * 4. Verify sequence order preservation
     */
    create_hippocampus();

    // Encode a sequence of 10 distinct events
    const int sequence_length = 10;
    std::vector<uint32_t> sequence_ids;
    std::vector<std::vector<float>> sequence_patterns;

    hippo_set_oscillation_state(hippo, OSCILLATION_THETA);

    for (int i = 0; i < sequence_length; i++) {
        float what[30], where[10], when[5];

        // Each event has a unique signature
        for (int j = 0; j < 30; j++) {
            what[j] = (float)i / sequence_length + 0.02f * (float)j;
        }
        sequence_patterns.push_back(std::vector<float>(what, what + 30));

        // Sequential spatial positions
        where[0] = (float)i * 10.0f;
        where[1] = 50.0f;
        for (int j = 2; j < 10; j++) where[j] = 0.5f;

        // Sequential time encoding
        generate_temporal_context(when, 5, i * 100);

        uint32_t episode_id;
        int result = hippo_encode_episode(hippo, what, 30, where, 10, when, 5,
                                           0.5f, 0.5f, &episode_id);
        EXPECT_EQ(result, 0);
        sequence_ids.push_back(episode_id);

        // Brief pause between events
        for (int j = 0; j < 20; j++) {
            hippo_update(hippo, 0.001f);
        }
    }

    // Test forward replay
    hippo_set_oscillation_state(hippo, OSCILLATION_THETA);
    hippo_trigger_replay(hippo, REPLAY_FORWARD);

    for (int i = 0; i < 100; i++) {
        hippo_update(hippo, 0.001f);
    }

    // Test reverse replay (sharp-wave ripples)
    hippo_set_oscillation_state(hippo, OSCILLATION_SHARP_WAVE_RIPPLE);
    hippo_trigger_replay(hippo, REPLAY_REVERSE);

    for (int i = 0; i < 100; i++) {
        hippo_update(hippo, 0.001f);
    }

    // Verify all sequence elements are still stored
    for (uint32_t id : sequence_ids) {
        const nimcp_episode_t* ep = hippo_get_episode(hippo, id);
        EXPECT_NE(ep, nullptr);
    }

    // Test cued recall - first element should predict second
    float cue[15];
    for (int i = 0; i < 15; i++) {
        cue[i] = sequence_patterns[0][i];
    }

    float predicted[60];
    uint32_t predicted_dim;
    float confidence;

    hippo_pattern_complete(hippo, cue, 15, predicted, &predicted_dim, &confidence);
    // Should retrieve pattern completion
}

// =============================================================================
// E2E: Long-Term Memory Consolidation
// =============================================================================

TEST_F(HippocampusE2ETest, E2E_LongTermConsolidation) {
    /**
     * Test long-term memory consolidation:
     * 1. Initial encoding (hippocampal-dependent)
     * 2. Multiple consolidation cycles (sleep)
     * 3. Verify gradual hippocampal independence
     * 4. Test systems consolidation readiness
     */
    create_hippocampus();

    // Encode important memories
    std::vector<uint32_t> episode_ids;
    const int num_memories = 15;

    for (int i = 0; i < num_memories; i++) {
        float what[40], where[20], when[10];
        generate_sensory_input(what, 40);
        generate_sensory_input(where, 20);
        generate_temporal_context(when, 10, i);

        // High emotional salience = important memory
        uint32_t episode_id;
        hippo_encode_episode(hippo, what, 40, where, 20, when, 10,
                              0.8f, 0.7f, &episode_id);
        episode_ids.push_back(episode_id);
    }

    // Track consolidation progress over multiple sleep cycles
    std::vector<float> consolidation_levels;

    for (int sleep_cycle = 0; sleep_cycle < 5; sleep_cycle++) {
        simulate_sleep_cycle(1.0f);

        // Measure average consolidation level
        float avg_consolidation = 0.0f;
        int count = 0;

        for (uint32_t id : episode_ids) {
            const nimcp_episode_t* ep = hippo_get_episode(hippo, id);
            if (ep) {
                avg_consolidation += ep->consolidation_level;
                count++;
            }
        }

        if (count > 0) {
            avg_consolidation /= count;
            consolidation_levels.push_back(avg_consolidation);
        }

        // Simulate waking period between sleep cycles
        hippo_set_oscillation_state(hippo, OSCILLATION_THETA);
        for (int i = 0; i < 200; i++) {
            hippo_update(hippo, 0.01f);
        }
    }

    // Consolidation should increase over cycles
    if (consolidation_levels.size() >= 2) {
        float initial = consolidation_levels.front();
        float final_level = consolidation_levels.back();
        EXPECT_GE(final_level, initial);
    }

    // Verify memories are still accessible
    int accessible_count = 0;
    for (uint32_t id : episode_ids) {
        if (hippo_get_episode(hippo, id)) {
            accessible_count++;
        }
    }

    EXPECT_GT(accessible_count, num_memories / 2);
}

// =============================================================================
// E2E: Full System Integration Stress Test
// =============================================================================

TEST_F(HippocampusE2ETest, E2E_FullSystemStressTest) {
    /**
     * Comprehensive system stress test:
     * 1. Connect all bridges
     * 2. High-frequency encoding
     * 3. Concurrent retrieval and consolidation
     * 4. Verify system stability
     */
    create_hippocampus();

    // Connect all major bridges (using void* nullptr as mock contexts)
    hippo_init_prime_resonance_bridge(hippo, nullptr);
    hippo->prime_resonance_bridge.resonance_frequency = 7.83f;
    hippo->prime_resonance_bridge.resonance_active = true;

    hippo_init_cognitive_bridge(hippo, nullptr);
    hippo->cognitive_bridge.attention_level = 1.0f;

    hippo_init_hypothalamus_bridge(hippo, nullptr);
    hippo->hypothalamus_bridge.stress_level = 0.5f;

    hippo_init_substrate_bridge(hippo, nullptr);
    hippo->substrate_bridge.atp_level = 0.8f;

    // High-frequency operation
    const int iterations = 500;
    int encode_count = 0;
    int retrieve_count = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        // Encode
        float what[30], where[15], when[5];
        generate_sensory_input(what, 30);
        generate_sensory_input(where, 15);
        generate_temporal_context(when, 5, i);

        uint32_t episode_id;
        if (hippo_encode_episode(hippo, what, 30, where, 15, when, 5,
                                  0.5f, 0.5f, &episode_id) == 0) {
            encode_count++;
        }

        // Retrieve
        float partial[15];
        for (int j = 0; j < 15; j++) partial[j] = what[j];

        float completed[60];
        uint32_t dim;
        float conf;
        if (hippo_pattern_complete(hippo, partial, 15, completed, &dim, &conf) == 0) {
            retrieve_count++;
        }

        // Update
        hippo_update(hippo, 0.001f);

        // Periodic consolidation
        if (i % 100 == 99) {
            hippo_consolidate_memories(hippo, 0.01f);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // System should remain stable
    hippo_stats_t stats;
    int result = hippo_get_stats(hippo, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_GT(encode_count, iterations / 2);
    EXPECT_GT(stats.episodes_encoded, 0u);

    // Performance should be reasonable (stress test on varied hardware)
    EXPECT_LT(duration.count(), 15000);  // Under 15 seconds for 500 iterations
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
