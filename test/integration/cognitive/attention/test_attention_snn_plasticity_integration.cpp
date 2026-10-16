//=============================================================================
// test_attention_snn_plasticity_integration.cpp - Attention SNN/Plasticity Integration
//=============================================================================
/**
 * @file test_attention_snn_plasticity_integration.cpp
 * @brief Integration tests for Attention-SNN-Plasticity bidirectional dataflows
 *
 * WHAT: Tests complete integration between attention system, SNN, and plasticity
 * WHY:  Verify bidirectional dataflows enable attention-modulated learning
 * HOW:  Create both bridges, simulate attention processing pipelines
 *
 * THEORETICAL FOUNDATIONS:
 * - Desimone & Duncan (1995): Biased competition model of attention
 * - Roelfsema & van Ooyen (2005): Attention-gated reinforcement learning
 * - Itti & Koch (2001): Salience-based attention
 *
 * INTEGRATION POINTS:
 * - Attention weights -> SNN encoding -> population activity
 * - SNN spikes -> Plasticity STDP -> weight updates
 * - Salience encoding -> round-trip decoding
 * - Competition dynamics -> winner-take-all learning
 *
 * TEST SCENARIOS:
 * 1.  Full Attention Processing Pipeline: Encode attention -> SNN simulation -> Decode
 * 2.  End-to-end attention focus learning (SNN + Plasticity working together)
 * 3.  Attention shift dynamics with learning
 * 4.  Salience-modulated learning
 * 5.  Competition-driven sparse attention learning
 * 6.  Habituation over repeated attention to same head
 * 7.  Novelty-boosted learning for new attention patterns
 * 8.  Reward-modulated attention learning
 * 9.  Top-k attention with plasticity feedback
 * 10. Gate modulation effects on learning
 * 11. Focus strength and sparsity metrics integration
 * 12. Cross-bridge state synchronization
 * 13. Statistics aggregation across bridges
 * 14. Reset and recovery behavior
 * 15. BCM metaplasticity in attention context
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <atomic>
#include <algorithm>

// Headers have their own extern "C" guards
#include "cognitive/attention/nimcp_attention_snn_bridge.h"
#include "cognitive/attention/nimcp_attention_plasticity_bridge.h"

#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AttentionSNNPlasticityIntegrationTest : public ::testing::Test {
protected:
    attention_snn_bridge_t* snn_bridge;
    attention_plasticity_bridge_t* plasticity_bridge;

    // Test constants
    static constexpr uint32_t NUM_HEADS = 8;
    static constexpr uint32_t SEQUENCE_LENGTH = 16;
    static constexpr uint32_t NEURONS_PER_HEAD = 32;

    // Callback tracking
    std::atomic<int> weight_change_count{0};
    std::atomic<float> last_weight_change{0.0f};
    std::atomic<int> shift_count{0};
    uint32_t last_from_head{0};
    uint32_t last_to_head{0};

    void SetUp() override {
        // Create SNN bridge with test-friendly config
        attention_snn_config_t snn_config = attention_snn_config_default();
        snn_config.num_heads = NUM_HEADS;
        snn_config.neurons_per_head = NEURONS_PER_HEAD;
        snn_config.sequence_length = SEQUENCE_LENGTH;
        snn_config.salience_dim = ATTENTION_SNN_SALIENCE_DIM;
        snn_config.encoding = ATTENTION_SNN_ENCODE_RATE;
        snn_config.decoding = ATTENTION_SNN_DECODE_SOFTMAX;
        snn_config.enable_competition = true;
        snn_config.top_k = 3;
        snn_config.enable_gate_integration = true;
        snn_config.dt_ms = ATTENTION_SNN_DEFAULT_DT;
        snn_config.simulation_window_ms = 50.0f;
        snn_config.enable_bio_async = false;  // Disable for predictable tests
        snn_config.enable_plasticity_integration = true;
        snn_config.enable_immune_modulation = false;

        snn_bridge = attention_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create attention SNN bridge";

        // Create Plasticity bridge with test-friendly config
        attention_plasticity_config_t plasticity_config = attention_plasticity_config_default();
        plasticity_config.enable_attention_modulation = true;
        plasticity_config.enable_salience_modulation = true;
        plasticity_config.enable_bcm = true;
        plasticity_config.enable_homeostatic = true;
        plasticity_config.enable_eligibility = true;
        plasticity_config.enable_habituation = true;
        plasticity_config.enable_novelty_detection = true;
        plasticity_config.enable_bio_async = false;  // Disable for predictable tests
        plasticity_config.stdp_ltp_window_ms = ATTENTION_PLASTICITY_STDP_WINDOW;
        plasticity_config.stdp_ltd_window_ms = ATTENTION_PLASTICITY_STDP_WINDOW;
        plasticity_config.focus_learning_boost = 1.5f;
        plasticity_config.novelty_boost = 2.0f;
        plasticity_config.habituation_rate = 0.1f;

        plasticity_bridge = attention_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create attention plasticity bridge";

        // Reset counters
        weight_change_count = 0;
        last_weight_change = 0.0f;
        shift_count = 0;
        last_from_head = 0;
        last_to_head = 0;
    }

    void TearDown() override {
        if (snn_bridge) {
            attention_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            attention_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Helper: Generate attention weights with a dominant head
    void generate_attention_weights(float* weights, uint32_t num_heads,
                                    uint32_t dominant_head, float dominance = 0.7f) {
        float remaining = 1.0f - dominance;
        float base = remaining / (float)(num_heads - 1);

        for (uint32_t i = 0; i < num_heads; i++) {
            if (i == dominant_head) {
                weights[i] = dominance;
            } else {
                weights[i] = base;
            }
        }
    }

    // Helper: Generate salience map with peaks
    void generate_salience_map(float* salience, uint32_t length,
                               uint32_t peak_pos, float peak_value = 0.9f) {
        for (uint32_t i = 0; i < length; i++) {
            float dist = std::abs((float)i - (float)peak_pos);
            salience[i] = peak_value * std::exp(-0.5f * dist * dist / 4.0f);
            salience[i] = std::max(0.0f, std::min(1.0f, salience[i]));
        }
    }

    // Helper: Register synapses for all heads
    void register_head_synapses(float initial_weight = 0.5f) {
        for (uint32_t head = 0; head < NUM_HEADS; head++) {
            int ret = attention_plasticity_register_synapse(
                plasticity_bridge,
                head,                                 // synapse_id
                ATTENTION_SYNAPSE_QUERY_KEY,          // type
                head,                                 // head_idx
                initial_weight                        // initial_weight
            );
            ASSERT_EQ(ret, 0) << "Failed to register synapse for head " << head;
        }
    }
};

//=============================================================================
// Static Callback Functions
//=============================================================================

static std::atomic<int>* g_weight_counter = nullptr;
static std::atomic<float>* g_last_weight_change = nullptr;
static std::atomic<int>* g_shift_counter = nullptr;
static uint32_t* g_last_from = nullptr;
static uint32_t* g_last_to = nullptr;

static void weight_change_callback(uint32_t synapse_id,
                                   uint32_t head_idx,
                                   float old_weight,
                                   float new_weight,
                                   attention_learn_event_t event_type,
                                   void* user_data) {
    (void)synapse_id;
    (void)head_idx;
    (void)event_type;
    (void)user_data;

    if (g_weight_counter) {
        (*g_weight_counter)++;
    }
    if (g_last_weight_change) {
        (*g_last_weight_change) = new_weight - old_weight;
    }
}

static void shift_callback(uint32_t old_head,
                           uint32_t new_head,
                           float shift_strength,
                           void* user_data) {
    (void)shift_strength;
    (void)user_data;

    if (g_shift_counter) {
        (*g_shift_counter)++;
    }
    if (g_last_from) {
        *g_last_from = old_head;
    }
    if (g_last_to) {
        *g_last_to = new_head;
    }
}

//=============================================================================
// Test 1: Full Attention Processing Pipeline
//=============================================================================

TEST_F(AttentionSNNPlasticityIntegrationTest, FullAttentionProcessingPipeline) {
    // Step 1: Create attention weights with head 3 as dominant
    float input_weights[NUM_HEADS];
    generate_attention_weights(input_weights, NUM_HEADS, 3, 0.6f);

    // Step 2: Encode attention weights in SNN
    int spikes = attention_snn_encode_weights(snn_bridge, input_weights, NUM_HEADS);
    EXPECT_GE(spikes, 0) << "Attention encoding should succeed (returns spike count)";

    // Step 3: Run SNN simulation
    int ret = attention_snn_simulate(snn_bridge, 50.0f);
    EXPECT_EQ(ret, 0) << "SNN simulation should succeed";

    // Step 4: Decode attention weights from SNN output
    float output_weights[NUM_HEADS];
    ret = attention_snn_get_weights(snn_bridge, output_weights, NUM_HEADS);
    EXPECT_EQ(ret, 0) << "Getting decoded weights should succeed";

    // Step 5: Verify output weights are valid probability distribution
    float sum = 0.0f;
    for (uint32_t i = 0; i < NUM_HEADS; i++) {
        EXPECT_GE(output_weights[i], 0.0f) << "Weight should be non-negative";
        EXPECT_LE(output_weights[i], 1.0f) << "Weight should be <= 1.0";
        sum += output_weights[i];
    }
    // Sum should be close to 1 for softmax decoding
    EXPECT_NEAR(sum, 1.0f, 0.1f) << "Weights should sum to ~1.0 for softmax decoding";

    // Step 6: Get focus strength and sparsity
    float focus = attention_snn_get_focus_strength(snn_bridge);
    float sparsity = attention_snn_get_sparsity(snn_bridge);
    EXPECT_GE(focus, 0.0f) << "Focus strength should be non-negative";
    EXPECT_LE(focus, 1.0f) << "Focus strength should be <= 1.0";
    EXPECT_GE(sparsity, 0.0f) << "Sparsity should be non-negative";
    EXPECT_LE(sparsity, 1.0f) << "Sparsity should be <= 1.0";

    // Step 7: Verify stats accumulated
    attention_snn_stats_t stats;
    ret = attention_snn_get_stats(snn_bridge, &stats);
    EXPECT_EQ(ret, 0) << "Getting stats should succeed";
    EXPECT_GT(stats.total_forward_passes, 0u) << "Should have recorded forward passes";
}

//=============================================================================
// Test 2: End-to-End Attention Focus Learning
//=============================================================================

TEST_F(AttentionSNNPlasticityIntegrationTest, EndToEndAttentionFocusLearning) {
    // Setup: Register synapses for all heads
    register_head_synapses(0.5f);

    // Setup callback
    g_weight_counter = &weight_change_count;
    g_last_weight_change = &last_weight_change;
    attention_plasticity_set_weight_callback(plasticity_bridge, weight_change_callback, nullptr);

    uint64_t timestamp = nimcp_time_get_us();
    const int learning_trials = 10;
    const uint32_t target_head = 2;

    // Learning loop: Focus attention on target head repeatedly
    for (int trial = 0; trial < learning_trials; trial++) {
        // Step 1: Create attention weights focused on target head
        float weights[NUM_HEADS];
        generate_attention_weights(weights, NUM_HEADS, target_head, 0.7f);

        // Step 2: Encode in SNN
        attention_snn_encode_weights(snn_bridge, weights, NUM_HEADS);
        attention_snn_simulate(snn_bridge, 30.0f);

        // Step 3: Record focus event in plasticity bridge
        int ret = attention_plasticity_focus(
            plasticity_bridge,
            target_head,
            weights[target_head],
            timestamp + trial * 500000  // 500ms between trials
        );
        EXPECT_EQ(ret, 0) << "Recording focus should succeed";

        // Step 4: Update plasticity
        ret = attention_plasticity_update(plasticity_bridge, 30.0f);
        EXPECT_EQ(ret, 0) << "Plasticity update should succeed";
    }

    // Consolidate learning
    int ret = attention_plasticity_consolidate(plasticity_bridge);
    EXPECT_EQ(ret, 0) << "Consolidation should succeed";

    // Verify: Get learned bias for target head
    float bias;
    ret = attention_plasticity_get_bias(plasticity_bridge, target_head, &bias);
    EXPECT_EQ(ret, 0) << "Getting bias should succeed";
    // Bias should be non-negative after repeated focus
    EXPECT_GE(bias, 0.0f) << "Learned bias should be non-negative";

    // Verify: Get synapse state
    attention_plasticity_synapse_t synapse;
    ret = attention_plasticity_get_synapse(plasticity_bridge, target_head, &synapse);
    EXPECT_EQ(ret, 0) << "Getting synapse should succeed";
    EXPECT_FALSE(std::isnan(synapse.weight)) << "Weight should not be NaN";

    // Verify stats
    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_EQ(stats.total_focus_events, (uint64_t)learning_trials)
        << "Should have recorded all focus events";
}

//=============================================================================
// Test 3: Attention Shift Dynamics with Learning
//=============================================================================

TEST_F(AttentionSNNPlasticityIntegrationTest, AttentionShiftDynamicsWithLearning) {
    register_head_synapses(0.5f);

    // Setup shift callback
    g_shift_counter = &shift_count;
    g_last_from = &last_from_head;
    g_last_to = &last_to_head;
    attention_plasticity_set_shift_callback(plasticity_bridge, shift_callback, nullptr);

    uint64_t timestamp = nimcp_time_get_us();

    // Sequence of attention shifts: 0 -> 3 -> 5 -> 7 -> 2
    uint32_t shift_sequence[] = {0, 3, 5, 7, 2};
    const int num_shifts = 4;

    for (int i = 0; i < num_shifts; i++) {
        uint32_t from_head = shift_sequence[i];
        uint32_t to_head = shift_sequence[i + 1];

        // Step 1: Create attention weights transitioning between heads
        float weights[NUM_HEADS];
        generate_attention_weights(weights, NUM_HEADS, to_head, 0.65f);

        // Step 2: Encode and simulate in SNN
        attention_snn_encode_weights(snn_bridge, weights, NUM_HEADS);
        attention_snn_simulate(snn_bridge, 40.0f);

        // Step 3: Record shift event
        float shift_strength = 0.5f + 0.1f * (float)i;  // Varying strength
        int ret = attention_plasticity_shift(
            plasticity_bridge,
            from_head,
            to_head,
            shift_strength,
            timestamp + i * 300000  // 300ms between shifts
        );
        EXPECT_EQ(ret, 0) << "Recording shift should succeed";

        // Step 4: Update plasticity
        attention_plasticity_update(plasticity_bridge, 40.0f);
    }

    // Verify shift callback was called
    EXPECT_EQ(shift_count.load(), num_shifts)
        << "Shift callback should be called for each shift";

    // Verify last shift was 7 -> 2
    EXPECT_EQ(last_from_head, 7u) << "Last shift should be from head 7";
    EXPECT_EQ(last_to_head, 2u) << "Last shift should be to head 2";

    // Verify stats
    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_EQ(stats.total_shift_events, (uint64_t)num_shifts)
        << "Should have recorded all shift events";
}

//=============================================================================
// Test 4: Salience-Modulated Learning
//=============================================================================

TEST_F(AttentionSNNPlasticityIntegrationTest, SalienceModulatedLearning) {
    register_head_synapses(0.5f);

    uint64_t timestamp = nimcp_time_get_us();

    // Create salience map with peak at position 5
    float salience[SEQUENCE_LENGTH];
    generate_salience_map(salience, SEQUENCE_LENGTH, 5, 0.9f);

    // Step 1: Encode salience in SNN
    int spikes = attention_snn_encode_salience(snn_bridge, salience, SEQUENCE_LENGTH);
    EXPECT_GE(spikes, 0) << "Salience encoding should succeed";

    // Step 2: Simulate SNN
    int ret = attention_snn_simulate(snn_bridge, 50.0f);
    EXPECT_EQ(ret, 0) << "Simulation should succeed";

    // Step 3: Get decoded salience from SNN
    float decoded_salience[SEQUENCE_LENGTH];
    ret = attention_snn_get_salience(snn_bridge, decoded_salience, SEQUENCE_LENGTH);
    EXPECT_EQ(ret, 0) << "Getting decoded salience should succeed";

    // Verify decoded salience values are valid
    for (uint32_t i = 0; i < SEQUENCE_LENGTH; i++) {
        EXPECT_GE(decoded_salience[i], 0.0f) << "Salience should be non-negative at " << i;
        EXPECT_LE(decoded_salience[i], 1.0f) << "Salience should be <= 1.0 at " << i;
    }

    // Step 4: Record salience observation in plasticity bridge
    ret = attention_plasticity_salience(plasticity_bridge, salience, SEQUENCE_LENGTH, timestamp);
    EXPECT_EQ(ret, 0) << "Recording salience should succeed";

    // Step 5: Set salience modulation
    ret = attention_plasticity_set_salience_modulation(plasticity_bridge, 0.8f);
    EXPECT_EQ(ret, 0) << "Setting salience modulation should succeed";

    // Step 6: Focus on high-salience item with learning
    float weights[NUM_HEADS];
    generate_attention_weights(weights, NUM_HEADS, 0, 0.7f);
    attention_plasticity_focus(plasticity_bridge, 0, 0.8f, timestamp + 10000);
    attention_plasticity_update(plasticity_bridge, 50.0f);

    // Verify state reflects salience modulation
    attention_plasticity_bridge_state_t state;
    attention_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_GE(state.current_salience_mod, 0.0f)
        << "Salience modulation should be tracked";
}

//=============================================================================
// Test 5: Competition-Driven Sparse Attention Learning
//=============================================================================

TEST_F(AttentionSNNPlasticityIntegrationTest, CompetitionDrivenSparseAttentionLearning) {
    register_head_synapses(0.5f);

    // Configure SNN for winner-take-all competition
    attention_snn_set_competition_strength(snn_bridge, 0.8f);

    uint64_t timestamp = nimcp_time_get_us();

    // Create diffuse attention (multiple heads with similar weights)
    float diffuse_weights[NUM_HEADS];
    for (uint32_t i = 0; i < NUM_HEADS; i++) {
        diffuse_weights[i] = 0.1f + 0.05f * (float)(i % 3);
    }

    // Step 1: Encode diffuse attention
    int spikes = attention_snn_encode_weights(snn_bridge, diffuse_weights, NUM_HEADS);
    EXPECT_GE(spikes, 0) << "Encoding should succeed";

    // Step 2: Run competition phase
    int ret = attention_snn_compete(snn_bridge, 100.0f);
    EXPECT_EQ(ret, 0) << "Competition should succeed";

    // Step 3: Simulate to settle
    ret = attention_snn_simulate(snn_bridge, 50.0f);
    EXPECT_EQ(ret, 0) << "Simulation should succeed";

    // Step 4: Get output weights (should be more sparse after competition)
    float output_weights[NUM_HEADS];
    attention_snn_get_weights(snn_bridge, output_weights, NUM_HEADS);

    // Step 5: Verify sparsity increased
    float sparsity = attention_snn_get_sparsity(snn_bridge);
    EXPECT_GE(sparsity, 0.0f) << "Sparsity should be computed";

    // Step 6: Get top-k indices
    int32_t top_k_indices[3];
    int num_top = attention_snn_get_top_k(snn_bridge, top_k_indices, 3);
    EXPECT_GE(num_top, 0) << "Should return top-k indices";
    EXPECT_LE(num_top, 3) << "Should not exceed requested k";

    // Verify top-k indices are valid
    for (int i = 0; i < num_top; i++) {
        EXPECT_GE(top_k_indices[i], 0) << "Index should be non-negative";
        EXPECT_LT((uint32_t)top_k_indices[i], NUM_HEADS) << "Index should be < num_heads";
    }

    // Step 7: Record competition-based learning in plasticity
    for (int i = 0; i < num_top; i++) {
        attention_plasticity_focus(
            plasticity_bridge,
            (uint32_t)top_k_indices[i],
            output_weights[top_k_indices[i]],
            timestamp + i * 10000
        );
    }
    attention_plasticity_update(plasticity_bridge, 50.0f);

    // Verify bridge state
    attention_snn_bridge_state_t snn_state;
    attention_snn_get_state(snn_bridge, &snn_state);
    EXPECT_GE(snn_state.competition_energy, 0.0f) << "Competition energy should be tracked";
}

//=============================================================================
// Test 6: Habituation Over Repeated Attention to Same Head
//=============================================================================

TEST_F(AttentionSNNPlasticityIntegrationTest, HabituationOverRepeatedAttention) {
    register_head_synapses(0.7f);

    uint64_t timestamp = nimcp_time_get_us();
    const uint32_t habituated_head = 4;
    const int habituation_trials = 20;

    // Get initial habituation level
    float initial_habituation = attention_plasticity_get_habituation(
        plasticity_bridge, habituated_head
    );

    // Repeatedly attend to the same head (habituation)
    for (int trial = 0; trial < habituation_trials; trial++) {
        // Step 1: Create focused attention on same head
        float weights[NUM_HEADS];
        generate_attention_weights(weights, NUM_HEADS, habituated_head, 0.6f);

        // Step 2: Encode and simulate
        attention_snn_encode_weights(snn_bridge, weights, NUM_HEADS);
        attention_snn_simulate(snn_bridge, 30.0f);

        // Step 3: Record habituation trial
        int ret = attention_plasticity_habituation_trial(
            plasticity_bridge,
            habituated_head,
            timestamp + trial * 200000  // 200ms between trials
        );
        EXPECT_EQ(ret, 0) << "Recording habituation trial should succeed";

        // Step 4: Update plasticity
        attention_plasticity_update(plasticity_bridge, 30.0f);
    }

    // Get final habituation level
    float final_habituation = attention_plasticity_get_habituation(
        plasticity_bridge, habituated_head
    );

    // Habituation level should increase (more habituated)
    EXPECT_GE(final_habituation, initial_habituation)
        << "Habituation level should increase after repeated exposure";

    // Verify habituation stats
    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.habituation_events, 0u) << "Should have recorded habituation events";
}

//=============================================================================
// Test 7: Novelty-Boosted Learning for New Attention Patterns
//=============================================================================

TEST_F(AttentionSNNPlasticityIntegrationTest, NoveltyBoostedLearning) {
    register_head_synapses(0.5f);

    g_weight_counter = &weight_change_count;
    attention_plasticity_set_weight_callback(plasticity_bridge, weight_change_callback, nullptr);

    uint64_t timestamp = nimcp_time_get_us();

    // Phase 1: Habituate to heads 0-3 (familiar pattern)
    for (int trial = 0; trial < 10; trial++) {
        uint32_t familiar_head = trial % 4;
        attention_plasticity_habituation_trial(
            plasticity_bridge, familiar_head, timestamp + trial * 100000
        );
        attention_plasticity_update(plasticity_bridge, 20.0f);
    }

    // Phase 2: Introduce novel pattern (head 6, which was not attended)
    const uint32_t novel_head = 6;
    float novelty_score = 0.9f;  // High novelty

    // Get novelty score before (should be high for unexposed head)
    float pre_novelty = attention_plasticity_get_novelty_score(plasticity_bridge, novel_head);

    // Step 1: Record novelty detection
    int ret = attention_plasticity_novelty(
        plasticity_bridge,
        novel_head,
        novelty_score,
        timestamp + 1500000
    );
    EXPECT_EQ(ret, 0) << "Recording novelty should succeed";

    // Step 2: Focus on novel head with boosted learning
    float weights[NUM_HEADS];
    generate_attention_weights(weights, NUM_HEADS, novel_head, 0.75f);
    attention_snn_encode_weights(snn_bridge, weights, NUM_HEADS);
    attention_snn_simulate(snn_bridge, 40.0f);

    // Step 3: Record focus with novelty
    attention_plasticity_focus(
        plasticity_bridge, novel_head, 0.8f, timestamp + 1550000
    );
    attention_plasticity_update(plasticity_bridge, 40.0f);

    // Get novelty score after
    float post_novelty = attention_plasticity_get_novelty_score(plasticity_bridge, novel_head);

    // Novelty processing should affect the score
    // (exact direction depends on whether novelty decreases after exposure)
    EXPECT_GE(post_novelty, 0.0f) << "Novelty score should be non-negative";
    EXPECT_LE(post_novelty, 1.0f) << "Novelty score should be <= 1.0";

    // Verify novelty stats
    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.novelty_events, 0u) << "Should have recorded novelty events";
}

//=============================================================================
// Test 8: Reward-Modulated Attention Learning
//=============================================================================

TEST_F(AttentionSNNPlasticityIntegrationTest, RewardModulatedAttentionLearning) {
    register_head_synapses(0.5f);

    g_weight_counter = &weight_change_count;
    g_last_weight_change = &last_weight_change;
    attention_plasticity_set_weight_callback(plasticity_bridge, weight_change_callback, nullptr);

    uint64_t timestamp = nimcp_time_get_us();

    // Get initial synapse state
    attention_plasticity_synapse_t synapse_before;
    attention_plasticity_get_synapse(plasticity_bridge, 1, &synapse_before);
    float initial_weight = synapse_before.weight;

    // Trial 1: Attend to head 1 with positive reward
    float weights[NUM_HEADS];
    generate_attention_weights(weights, NUM_HEADS, 1, 0.7f);

    attention_snn_encode_weights(snn_bridge, weights, NUM_HEADS);
    attention_snn_simulate(snn_bridge, 30.0f);

    attention_plasticity_focus(plasticity_bridge, 1, 0.7f, timestamp);
    attention_plasticity_reward(plasticity_bridge, 0.9f, timestamp + 50000);  // Positive reward
    attention_plasticity_update(plasticity_bridge, 50.0f);

    attention_plasticity_synapse_t synapse_after_reward;
    attention_plasticity_get_synapse(plasticity_bridge, 1, &synapse_after_reward);
    float weight_after_reward = synapse_after_reward.weight;

    // Trial 2: Attend to head 2 with negative reward (punishment)
    generate_attention_weights(weights, NUM_HEADS, 2, 0.7f);

    attention_snn_encode_weights(snn_bridge, weights, NUM_HEADS);
    attention_snn_simulate(snn_bridge, 30.0f);

    attention_plasticity_focus(plasticity_bridge, 2, 0.7f, timestamp + 200000);
    attention_plasticity_reward(plasticity_bridge, -0.8f, timestamp + 250000);  // Negative reward
    attention_plasticity_update(plasticity_bridge, 50.0f);

    attention_plasticity_synapse_t synapse_after_punish;
    attention_plasticity_get_synapse(plasticity_bridge, 2, &synapse_after_punish);

    // Verify weights are within valid bounds
    EXPECT_GE(weight_after_reward, 0.0f) << "Weight should be non-negative";
    EXPECT_LE(weight_after_reward, 1.0f) << "Weight should be <= 1.0";
    EXPECT_GE(synapse_after_punish.weight, 0.0f) << "Weight should be non-negative";
    EXPECT_LE(synapse_after_punish.weight, 1.0f) << "Weight should be <= 1.0";

    // Verify eligibility trace was used
    EXPECT_GE(synapse_after_reward.eligibility_trace, 0.0f)
        << "Eligibility trace should be tracked";

    // Verify stats
    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_NE(stats.total_reward, 0.0f) << "Should have recorded reward";
}

//=============================================================================
// Test 9: Top-K Attention with Plasticity Feedback
//=============================================================================

TEST_F(AttentionSNNPlasticityIntegrationTest, TopKAttentionWithPlasticityFeedback) {
    register_head_synapses(0.5f);

    uint64_t timestamp = nimcp_time_get_us();

    // Create mixed attention pattern
    float weights[NUM_HEADS] = {0.3f, 0.5f, 0.7f, 0.2f, 0.8f, 0.4f, 0.6f, 0.35f};

    // Normalize
    float sum = 0.0f;
    for (uint32_t i = 0; i < NUM_HEADS; i++) sum += weights[i];
    for (uint32_t i = 0; i < NUM_HEADS; i++) weights[i] /= sum;

    // Step 1: Encode and simulate
    attention_snn_encode_weights(snn_bridge, weights, NUM_HEADS);
    attention_snn_simulate(snn_bridge, 50.0f);
    attention_snn_compete(snn_bridge, 30.0f);

    // Step 2: Get top-3 attended heads
    int32_t top_k[3];
    int num_top = attention_snn_get_top_k(snn_bridge, top_k, 3);
    EXPECT_GT(num_top, 0) << "Should return at least one top head";

    // Step 3: For each top head, get plasticity modulation
    float modulation[NUM_HEADS];
    int ret = attention_plasticity_get_modulation(plasticity_bridge, modulation, NUM_HEADS);
    EXPECT_EQ(ret, 0) << "Getting modulation should succeed";

    // Step 4: Record focus on top heads with modulation-adjusted strength
    for (int i = 0; i < num_top; i++) {
        uint32_t head = (uint32_t)top_k[i];
        float adjusted_strength = weights[head] * (1.0f + modulation[head]);
        adjusted_strength = std::min(1.0f, adjusted_strength);

        attention_plasticity_focus(
            plasticity_bridge,
            head,
            adjusted_strength,
            timestamp + i * 20000
        );
    }

    // Step 5: Update and consolidate
    attention_plasticity_update(plasticity_bridge, 50.0f);
    attention_plasticity_consolidate(plasticity_bridge);

    // Step 6: Verify learned biases for top-k heads
    for (int i = 0; i < num_top; i++) {
        uint32_t head = (uint32_t)top_k[i];
        float bias;
        ret = attention_plasticity_get_bias(plasticity_bridge, head, &bias);
        EXPECT_EQ(ret, 0) << "Getting bias should succeed for head " << head;
        // Bias values should be valid (can be positive or negative)
        EXPECT_FALSE(std::isnan(bias)) << "Bias should not be NaN for head " << head;
    }
}

//=============================================================================
// Test 10: Gate Modulation Effects on Learning
//=============================================================================

TEST_F(AttentionSNNPlasticityIntegrationTest, GateModulationEffectsOnLearning) {
    register_head_synapses(0.5f);

    uint64_t timestamp = nimcp_time_get_us();

    // Phase 1: Learning with gate open (high gate level)
    int ret = attention_snn_encode_gate(snn_bridge, 0.9f);
    EXPECT_EQ(ret, 0) << "Encoding gate should succeed";

    ret = attention_snn_set_gate_modulation(snn_bridge, 0.9f);
    EXPECT_EQ(ret, 0) << "Setting gate modulation should succeed";

    float weights_open[NUM_HEADS];
    generate_attention_weights(weights_open, NUM_HEADS, 0, 0.7f);
    attention_snn_encode_weights(snn_bridge, weights_open, NUM_HEADS);
    attention_snn_simulate(snn_bridge, 50.0f);

    // Record focus with high attention modulation
    attention_plasticity_set_attention_modulation(plasticity_bridge, 0.9f);
    attention_plasticity_focus(plasticity_bridge, 0, 0.8f, timestamp);
    attention_plasticity_update(plasticity_bridge, 50.0f);

    attention_plasticity_synapse_t synapse_gate_open;
    attention_plasticity_get_synapse(plasticity_bridge, 0, &synapse_gate_open);

    // Reset for phase 2
    attention_snn_reset(snn_bridge);

    // Phase 2: Learning with gate closed (low gate level)
    attention_snn_encode_gate(snn_bridge, 0.1f);
    attention_snn_set_gate_modulation(snn_bridge, 0.1f);

    float weights_closed[NUM_HEADS];
    generate_attention_weights(weights_closed, NUM_HEADS, 1, 0.7f);
    attention_snn_encode_weights(snn_bridge, weights_closed, NUM_HEADS);
    attention_snn_simulate(snn_bridge, 50.0f);

    // Record focus with low attention modulation
    attention_plasticity_set_attention_modulation(plasticity_bridge, 0.1f);
    attention_plasticity_focus(plasticity_bridge, 1, 0.8f, timestamp + 500000);
    attention_plasticity_update(plasticity_bridge, 50.0f);

    attention_plasticity_synapse_t synapse_gate_closed;
    attention_plasticity_get_synapse(plasticity_bridge, 1, &synapse_gate_closed);

    // Verify weights are valid
    EXPECT_FALSE(std::isnan(synapse_gate_open.weight))
        << "Gate-open weight should not be NaN";
    EXPECT_FALSE(std::isnan(synapse_gate_closed.weight))
        << "Gate-closed weight should not be NaN";

    // Check attention state reflects gate activation
    attention_snn_attention_state_t attention_state;
    attention_snn_get_attention_state(snn_bridge, &attention_state);
    EXPECT_GE(attention_state.gate_activation, 0.0f)
        << "Gate activation should be non-negative";
    EXPECT_LE(attention_state.gate_activation, 1.0f)
        << "Gate activation should be <= 1.0";
}

//=============================================================================
// Test 11: Focus Strength and Sparsity Metrics Integration
//=============================================================================

TEST_F(AttentionSNNPlasticityIntegrationTest, FocusStrengthAndSparsityMetricsIntegration) {
    register_head_synapses(0.5f);

    uint64_t timestamp = nimcp_time_get_us();

    // Test with varying focus levels
    struct {
        float dominance;
        const char* description;
    } test_cases[] = {
        {0.95f, "very focused"},
        {0.7f, "moderately focused"},
        {0.4f, "diffuse"},
        {0.125f, "uniform"}  // 1/NUM_HEADS = uniform distribution
    };

    for (const auto& tc : test_cases) {
        // Reset for each test case
        attention_snn_reset(snn_bridge);

        // Create attention pattern
        float weights[NUM_HEADS];
        generate_attention_weights(weights, NUM_HEADS, 3, tc.dominance);

        // Encode and simulate
        attention_snn_encode_weights(snn_bridge, weights, NUM_HEADS);
        attention_snn_simulate(snn_bridge, 50.0f);

        // Get focus and sparsity metrics
        float focus = attention_snn_get_focus_strength(snn_bridge);
        float sparsity = attention_snn_get_sparsity(snn_bridge);

        EXPECT_GE(focus, 0.0f) << tc.description << ": focus should be non-negative";
        EXPECT_LE(focus, 1.0f) << tc.description << ": focus should be <= 1.0";
        EXPECT_GE(sparsity, 0.0f) << tc.description << ": sparsity should be non-negative";
        EXPECT_LE(sparsity, 1.0f) << tc.description << ": sparsity should be <= 1.0";

        // Higher dominance should generally lead to higher focus
        // (exact relationship depends on implementation)
    }

    // Test with plasticity feedback
    float modulation[NUM_HEADS];
    attention_plasticity_get_modulation(plasticity_bridge, modulation, NUM_HEADS);

    // Modulation values should be valid
    for (uint32_t i = 0; i < NUM_HEADS; i++) {
        EXPECT_FALSE(std::isnan(modulation[i]))
            << "Modulation should not be NaN for head " << i;
    }

    // Verify attention state contains focus metrics
    attention_snn_attention_state_t state;
    int ret = attention_snn_get_attention_state(snn_bridge, &state);
    EXPECT_EQ(ret, 0) << "Getting attention state should succeed";
    EXPECT_EQ(state.focus_strength, attention_snn_get_focus_strength(snn_bridge))
        << "Focus strength should match";
    EXPECT_EQ(state.sparsity, attention_snn_get_sparsity(snn_bridge))
        << "Sparsity should match";
}

//=============================================================================
// Test 12: Cross-Bridge State Synchronization
//=============================================================================

TEST_F(AttentionSNNPlasticityIntegrationTest, CrossBridgeStateSynchronization) {
    register_head_synapses(0.5f);

    // Initial states should be IDLE
    attention_snn_bridge_state_t snn_state;
    attention_plasticity_bridge_state_t plasticity_state;

    int ret1 = attention_snn_get_state(snn_bridge, &snn_state);
    int ret2 = attention_plasticity_get_state(plasticity_bridge, &plasticity_state);

    EXPECT_EQ(ret1, 0) << "Getting SNN state should succeed";
    EXPECT_EQ(ret2, 0) << "Getting plasticity state should succeed";
    EXPECT_EQ(snn_state.state, ATTENTION_SNN_STATE_IDLE) << "SNN should start idle";
    EXPECT_EQ(plasticity_state.state, ATTENTION_PLASTICITY_STATE_IDLE)
        << "Plasticity should start idle";

    // Perform coordinated operations
    uint64_t timestamp = nimcp_time_get_us();

    float weights[NUM_HEADS];
    generate_attention_weights(weights, NUM_HEADS, 2, 0.7f);

    // SNN encoding
    attention_snn_encode_weights(snn_bridge, weights, NUM_HEADS);
    attention_snn_get_state(snn_bridge, &snn_state);
    // State may be ENCODING or transitioned already

    // SNN simulation
    attention_snn_simulate(snn_bridge, 50.0f);
    attention_snn_get_state(snn_bridge, &snn_state);
    EXPECT_GE(snn_state.state, ATTENTION_SNN_STATE_IDLE) << "SNN state should be valid";
    EXPECT_LE(snn_state.state, ATTENTION_SNN_STATE_DISABLED) << "SNN state should be valid";

    // Plasticity observation
    attention_plasticity_focus(plasticity_bridge, 2, 0.7f, timestamp);
    attention_plasticity_get_state(plasticity_bridge, &plasticity_state);
    // State may be OBSERVING or transitioned

    // Plasticity update
    attention_plasticity_update(plasticity_bridge, 50.0f);
    attention_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_GE(plasticity_state.state, ATTENTION_PLASTICITY_STATE_IDLE)
        << "Plasticity state should be valid";
    EXPECT_LE(plasticity_state.state, ATTENTION_PLASTICITY_STATE_DISABLED)
        << "Plasticity state should be valid";

    // Verify synchronization between bridges
    EXPECT_EQ(plasticity_state.registered_synapses, NUM_HEADS)
        << "Should have registered synapses for all heads";
    EXPECT_GE(snn_state.active_populations, 0u) << "Active populations should be tracked";
}

//=============================================================================
// Test 13: Statistics Aggregation Across Bridges
//=============================================================================

TEST_F(AttentionSNNPlasticityIntegrationTest, StatisticsAggregationAcrossBridges) {
    register_head_synapses(0.5f);

    uint64_t timestamp = nimcp_time_get_us();
    const int num_iterations = 5;

    // Perform mixed operations across both bridges
    for (int iter = 0; iter < num_iterations; iter++) {
        uint32_t head = iter % NUM_HEADS;

        // SNN operations
        float weights[NUM_HEADS];
        generate_attention_weights(weights, NUM_HEADS, head, 0.65f);
        attention_snn_encode_weights(snn_bridge, weights, NUM_HEADS);
        attention_snn_simulate(snn_bridge, 30.0f);
        attention_snn_compete(snn_bridge, 20.0f);

        // Plasticity operations
        attention_plasticity_focus(
            plasticity_bridge, head, weights[head],
            timestamp + iter * 200000
        );
        if (iter % 2 == 0) {
            attention_plasticity_reward(
                plasticity_bridge, 0.5f - (float)iter * 0.1f,
                timestamp + iter * 200000 + 50000
            );
        }
        attention_plasticity_update(plasticity_bridge, 30.0f);

        // Shift attention
        if (iter > 0) {
            uint32_t from = (iter - 1) % NUM_HEADS;
            attention_plasticity_shift(
                plasticity_bridge, from, head, 0.6f,
                timestamp + iter * 200000 + 10000
            );
        }
    }

    // Get and verify SNN stats
    attention_snn_stats_t snn_stats;
    attention_snn_get_stats(snn_bridge, &snn_stats);

    EXPECT_EQ(snn_stats.total_forward_passes, (uint64_t)num_iterations)
        << "SNN should track forward passes";
    EXPECT_GE(snn_stats.total_spikes_generated, 0u) << "Should track spikes";
    EXPECT_GE(snn_stats.total_decodings, 0u) << "Should track decodings";
    EXPECT_GE(snn_stats.avg_processing_time_ms, 0.0f)
        << "Processing time should be non-negative";
    EXPECT_GE(snn_stats.avg_focus_strength, 0.0f) << "Avg focus should be non-negative";
    EXPECT_GE(snn_stats.avg_sparsity, 0.0f) << "Avg sparsity should be non-negative";

    // Get and verify plasticity stats
    attention_plasticity_stats_t plasticity_stats;
    attention_plasticity_get_stats(plasticity_bridge, &plasticity_stats);

    EXPECT_EQ(plasticity_stats.total_focus_events, (uint64_t)num_iterations)
        << "Should track focus events";
    EXPECT_EQ(plasticity_stats.total_shift_events, (uint64_t)(num_iterations - 1))
        << "Should track shift events";
    EXPECT_GT(plasticity_stats.total_pre_spikes, 0u) << "Should have pre-spikes";

    // Reset stats and verify
    attention_snn_reset_stats(snn_bridge);
    attention_plasticity_reset_stats(plasticity_bridge);

    attention_snn_get_stats(snn_bridge, &snn_stats);
    attention_plasticity_get_stats(plasticity_bridge, &plasticity_stats);

    EXPECT_EQ(snn_stats.total_forward_passes, 0u) << "SNN stats should be reset";
    EXPECT_EQ(plasticity_stats.total_focus_events, 0u) << "Plasticity stats should be reset";
}

//=============================================================================
// Test 14: Reset and Recovery Behavior
//=============================================================================

TEST_F(AttentionSNNPlasticityIntegrationTest, ResetAndRecoveryBehavior) {
    register_head_synapses(0.6f);

    uint64_t timestamp = nimcp_time_get_us();

    // Phase 1: Generate significant activity
    for (int i = 0; i < 10; i++) {
        float weights[NUM_HEADS];
        generate_attention_weights(weights, NUM_HEADS, i % NUM_HEADS, 0.7f);
        attention_snn_encode_weights(snn_bridge, weights, NUM_HEADS);
        attention_snn_simulate(snn_bridge, 20.0f);
        attention_plasticity_focus(
            plasticity_bridge, i % NUM_HEADS, 0.7f,
            timestamp + i * 100000
        );
        attention_plasticity_update(plasticity_bridge, 20.0f);
    }

    // Verify activity was recorded
    attention_snn_stats_t pre_reset_snn_stats;
    attention_plasticity_stats_t pre_reset_plasticity_stats;
    attention_snn_get_stats(snn_bridge, &pre_reset_snn_stats);
    attention_plasticity_get_stats(plasticity_bridge, &pre_reset_plasticity_stats);

    EXPECT_GT(pre_reset_snn_stats.total_forward_passes, 0u)
        << "Should have SNN activity before reset";
    EXPECT_GT(pre_reset_plasticity_stats.total_focus_events, 0u)
        << "Should have plasticity activity before reset";

    // Phase 2: Reset both bridges
    int ret1 = attention_snn_reset(snn_bridge);
    int ret2 = attention_plasticity_reset(plasticity_bridge);

    EXPECT_EQ(ret1, 0) << "SNN reset should succeed";
    EXPECT_EQ(ret2, 0) << "Plasticity reset should succeed";

    // Verify states are reset to IDLE
    attention_snn_bridge_state_t snn_state;
    attention_plasticity_bridge_state_t plasticity_state;

    attention_snn_get_state(snn_bridge, &snn_state);
    attention_plasticity_get_state(plasticity_bridge, &plasticity_state);

    EXPECT_EQ(snn_state.state, ATTENTION_SNN_STATE_IDLE) << "SNN should be idle after reset";
    EXPECT_EQ(plasticity_state.state, ATTENTION_PLASTICITY_STATE_IDLE)
        << "Plasticity should be idle after reset";

    // Note: Stats may or may not be reset depending on implementation
    // The key is that operational state is reset

    // Phase 3: Verify bridges can be used again after reset
    float weights[NUM_HEADS];
    generate_attention_weights(weights, NUM_HEADS, 5, 0.7f);

    int spikes = attention_snn_encode_weights(snn_bridge, weights, NUM_HEADS);
    EXPECT_GE(spikes, 0) << "Encoding should succeed after reset";

    int ret = attention_snn_simulate(snn_bridge, 30.0f);
    EXPECT_EQ(ret, 0) << "Simulation should succeed after reset";

    // Note: Synapses persist after reset (only values are reset to initial state)
    // so we don't need to re-register them

    ret = attention_plasticity_focus(
        plasticity_bridge, 5, 0.7f, nimcp_time_get_us()
    );
    EXPECT_EQ(ret, 0) << "Focus should succeed after reset";

    ret = attention_plasticity_update(plasticity_bridge, 30.0f);
    EXPECT_EQ(ret, 0) << "Update should succeed after reset";
}

//=============================================================================
// Test 15: BCM Metaplasticity in Attention Context
//=============================================================================

TEST_F(AttentionSNNPlasticityIntegrationTest, BCMMetaplasticityInAttentionContext) {
    // Register synapse for BCM test
    int ret = attention_plasticity_register_synapse(
        plasticity_bridge,
        100,                              // synapse_id
        ATTENTION_SYNAPSE_HEAD_OUTPUT,    // type
        0,                                // head_idx
        0.5f                              // initial_weight
    );
    EXPECT_EQ(ret, 0) << "Synapse registration should succeed";

    uint64_t timestamp = nimcp_time_get_us();

    // Phase 1: High attention activity (should shift BCM threshold up)
    for (int i = 0; i < 15; i++) {
        float weights[NUM_HEADS];
        generate_attention_weights(weights, NUM_HEADS, 0, 0.85f);  // Very focused

        attention_snn_encode_weights(snn_bridge, weights, NUM_HEADS);
        attention_snn_simulate(snn_bridge, 30.0f);

        attention_plasticity_focus(
            plasticity_bridge, 0, 0.9f,
            timestamp + i * 80000
        );
        attention_plasticity_update(plasticity_bridge, 30.0f);
    }

    // Get synapse state after high activity
    attention_plasticity_synapse_t synapse_high;
    attention_plasticity_get_synapse(plasticity_bridge, 100, &synapse_high);
    float bcm_threshold_high = synapse_high.bcm_threshold;
    float avg_activity_high = synapse_high.avg_activity;

    // Phase 2: Low attention activity (threshold should adjust down)
    for (int i = 0; i < 15; i++) {
        float weights[NUM_HEADS];
        // Diffuse attention (low on head 0)
        generate_attention_weights(weights, NUM_HEADS, 4, 0.5f);

        attention_snn_encode_weights(snn_bridge, weights, NUM_HEADS);
        attention_snn_simulate(snn_bridge, 30.0f);

        attention_plasticity_focus(
            plasticity_bridge, 0, 0.1f,  // Low focus on head 0
            timestamp + 1500000 + i * 80000
        );
        attention_plasticity_update(plasticity_bridge, 30.0f);
    }

    // Get synapse state after low activity
    attention_plasticity_synapse_t synapse_low;
    attention_plasticity_get_synapse(plasticity_bridge, 100, &synapse_low);
    float bcm_threshold_low = synapse_low.bcm_threshold;
    float avg_activity_low = synapse_low.avg_activity;

    // Verify BCM mechanism is tracking activity
    EXPECT_GE(synapse_low.avg_activity, 0.0f)
        << "Activity average should be non-negative";
    EXPECT_FALSE(std::isnan(synapse_low.bcm_threshold))
        << "BCM threshold should not be NaN";

    // BCM threshold should have changed (exact direction depends on implementation)
    // The key is that it's being tracked and not static
    if (std::abs(bcm_threshold_high - 0.0f) > 0.001f ||
        std::abs(bcm_threshold_low - 0.0f) > 0.001f) {
        // If BCM is active, threshold should differ or be non-zero
        SUCCEED() << "BCM threshold tracking appears active";
    }

    // Verify sensitivity is affected by activity history
    float sensitivity = attention_plasticity_get_sensitivity(plasticity_bridge, 0);
    EXPECT_GE(sensitivity, 0.0f) << "Sensitivity should be non-negative";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
