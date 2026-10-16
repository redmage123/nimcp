/**
 * @file test_broca_bridges.cpp
 * @brief Unit tests for Broca's region integration bridges
 *
 * Tests:
 * - Broca substrate bridge (metabolic modulation)
 * - Broca thalamic bridge (signal routing)
 * - Broca quantum bridge (quantum-accelerated processing)
 *
 * @version Phase B3: Broca Full Brain Integration
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/broca/nimcp_broca_substrate_bridge.h"
#include "core/brain/regions/broca/nimcp_broca_thalamic_bridge.h"
#include "core/brain/regions/broca/nimcp_broca_quantum_bridge.h"

//=============================================================================
// Broca Substrate Bridge Tests
//=============================================================================

class BrocaSubstrateBridgeTest : public ::testing::Test {
protected:
    broca_substrate_bridge_t* bridge = nullptr;

    void SetUp() override {
        broca_substrate_config_t config = broca_substrate_default_config();
        bridge = broca_substrate_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            broca_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(BrocaSubstrateBridgeTest, CreateWithNullConfig) {
    broca_substrate_bridge_t* b = broca_substrate_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    broca_substrate_bridge_destroy(b);
}

TEST_F(BrocaSubstrateBridgeTest, CreateWithConfig) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(BrocaSubstrateBridgeTest, DefaultConfigValues) {
    broca_substrate_config_t config = broca_substrate_default_config();
    EXPECT_TRUE(config.enable_atp_modulation);
    EXPECT_TRUE(config.enable_fatigue_modulation);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_FLOAT_EQ(config.atp_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.fatigue_sensitivity, 1.0f);
    EXPECT_GT(config.min_capacity, 0.0f);
}

TEST_F(BrocaSubstrateBridgeTest, UpdateWithoutSubstrate) {
    int result = broca_substrate_bridge_update(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(BrocaSubstrateBridgeTest, GetEffects) {
    broca_substrate_effects_t effects;
    int result = broca_substrate_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(result, 0);

    /* Initial effects should be at full capacity */
    EXPECT_FLOAT_EQ(effects.overall_capacity, 1.0f);
    EXPECT_GT(effects.speech_fluency, 0.0f);
    EXPECT_GT(effects.word_retrieval, 0.0f);
    EXPECT_GT(effects.syntax_complexity, 0.0f);
}

TEST_F(BrocaSubstrateBridgeTest, GetEffectsNullBridge) {
    broca_substrate_effects_t effects;
    int result = broca_substrate_bridge_get_effects(nullptr, &effects);
    EXPECT_EQ(result, -1);
}

TEST_F(BrocaSubstrateBridgeTest, GetEffectsNullOutput) {
    int result = broca_substrate_bridge_get_effects(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(BrocaSubstrateBridgeTest, ApplyEffects) {
    int result = broca_substrate_bridge_apply_effects(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(BrocaSubstrateBridgeTest, GetStats) {
    broca_substrate_stats_t stats;
    int result = broca_substrate_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.updates_processed, 0);
}

TEST_F(BrocaSubstrateBridgeTest, ResetStats) {
    /* Update a few times */
    broca_substrate_bridge_update(bridge);
    broca_substrate_bridge_update(bridge);

    broca_substrate_bridge_reset_stats(bridge);

    broca_substrate_stats_t stats;
    broca_substrate_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.updates_processed, 0);
}

//=============================================================================
// Broca Thalamic Bridge Tests
//=============================================================================

class BrocaThalamicBridgeTest : public ::testing::Test {
protected:
    broca_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        broca_thalamic_config_t config = broca_thalamic_default_config();
        bridge = broca_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            broca_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(BrocaThalamicBridgeTest, CreateWithNullConfig) {
    broca_thalamic_bridge_t* b = broca_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    broca_thalamic_bridge_destroy(b);
}

TEST_F(BrocaThalamicBridgeTest, CreateWithConfig) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(BrocaThalamicBridgeTest, DefaultConfigValues) {
    broca_thalamic_config_t config = broca_thalamic_default_config();
    EXPECT_TRUE(config.enable_attention_gating);
    EXPECT_TRUE(config.enable_motor_priority);
    EXPECT_TRUE(config.enable_syntax_routing);
    EXPECT_GT(config.motor_boost, 1.0f);
    EXPECT_GT(config.min_urgency_threshold, 0.0f);
}

TEST_F(BrocaThalamicBridgeTest, Reset) {
    int result = broca_thalamic_bridge_reset(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(BrocaThalamicBridgeTest, RouteMotorCommand) {
    uint8_t command[] = {1, 2, 3, 4};
    int result = broca_thalamic_route_motor_command(bridge, command, sizeof(command), 0.8f);
    EXPECT_EQ(result, 0);
}

TEST_F(BrocaThalamicBridgeTest, RoutePhonemes) {
    uint8_t phonemes[] = {10, 20, 30, 40, 50};
    int result = broca_thalamic_route_phonemes(bridge, phonemes, 5);
    EXPECT_EQ(result, 0);
}

TEST_F(BrocaThalamicBridgeTest, SignalUtteranceStart) {
    int result = broca_thalamic_signal_utterance_start(bridge, 1, 5);
    EXPECT_EQ(result, 0);
}

TEST_F(BrocaThalamicBridgeTest, SignalUtteranceEnd) {
    broca_thalamic_signal_utterance_start(bridge, 1, 5);
    int result = broca_thalamic_signal_utterance_end(bridge, 1);
    EXPECT_EQ(result, 0);
}

TEST_F(BrocaThalamicBridgeTest, SetAttention) {
    int result = broca_thalamic_set_attention(bridge, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(BrocaThalamicBridgeTest, GetAttention) {
    broca_thalamic_set_attention(bridge, 0.75f);

    float attention;
    int result = broca_thalamic_get_attention(bridge, &attention);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(attention, 0.75f);
}

TEST_F(BrocaThalamicBridgeTest, AttentionClamping) {
    broca_thalamic_set_attention(bridge, 2.0f);

    float attention;
    broca_thalamic_get_attention(bridge, &attention);
    EXPECT_LE(attention, 1.0f);

    broca_thalamic_set_attention(bridge, -1.0f);
    broca_thalamic_get_attention(bridge, &attention);
    EXPECT_GE(attention, 0.0f);
}

TEST_F(BrocaThalamicBridgeTest, GetStats) {
    broca_thalamic_stats_t stats;
    int result = broca_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.motor_commands_routed, 0);
}

TEST_F(BrocaThalamicBridgeTest, StatsTrackCommands) {
    uint8_t command[] = {1, 2, 3};
    broca_thalamic_route_motor_command(bridge, command, sizeof(command), 0.9f);
    broca_thalamic_route_motor_command(bridge, command, sizeof(command), 0.8f);

    broca_thalamic_stats_t stats;
    broca_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.motor_commands_routed, 2);
}

TEST_F(BrocaThalamicBridgeTest, ResetStats) {
    uint8_t command[] = {1};
    broca_thalamic_route_motor_command(bridge, command, 1, 0.9f);

    broca_thalamic_bridge_reset_stats(bridge);

    broca_thalamic_stats_t stats;
    broca_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.motor_commands_routed, 0);
}

TEST_F(BrocaThalamicBridgeTest, LowUrgencyGated) {
    broca_thalamic_set_attention(bridge, 0.1f);

    broca_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = BROCA_SIGNAL_MOTOR_COMMAND;
    signal.speech_urgency = 0.1f;  /* Low urgency */

    int result = broca_thalamic_route_signal(bridge, &signal);
    EXPECT_EQ(result, 0);  /* Should be gated, but returns 0 */

    broca_thalamic_stats_t stats;
    broca_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.signals_gated, 0);
}

//=============================================================================
// Broca Quantum Bridge Tests
//=============================================================================

class BrocaQuantumBridgeTest : public ::testing::Test {
protected:
    broca_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        broca_quantum_config_t config = broca_quantum_default_config();
        bridge = broca_quantum_bridge_create(nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            broca_quantum_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(BrocaQuantumBridgeTest, CreateWithNullConfig) {
    broca_quantum_bridge_t* b = broca_quantum_bridge_create(nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    broca_quantum_bridge_destroy(b);
}

TEST_F(BrocaQuantumBridgeTest, CreateWithConfig) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(BrocaQuantumBridgeTest, DefaultConfigValues) {
    broca_quantum_config_t config = broca_quantum_default_config();
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.lexicon_search_depth, 0);
    EXPECT_GT(config.syntax_alternatives, 0);
    EXPECT_GT(config.max_grover_iterations, 0);
    EXPECT_GT(config.min_expression_confidence, 0.0f);
    EXPECT_TRUE(config.enable_interference);
    EXPECT_TRUE(config.use_superposition);
}

TEST_F(BrocaQuantumBridgeTest, IsEnabled) {
    EXPECT_TRUE(broca_quantum_bridge_is_enabled(bridge));
}

TEST_F(BrocaQuantumBridgeTest, SetEnabled) {
    broca_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(broca_quantum_bridge_is_enabled(bridge));

    broca_quantum_bridge_set_enabled(bridge, true);
    EXPECT_TRUE(broca_quantum_bridge_is_enabled(bridge));
}

TEST_F(BrocaQuantumBridgeTest, SearchLexicon) {
    float semantic_target[] = {0.1f, 0.2f, 0.3f, 0.4f};
    quantum_lexical_result_t result;

    int ret = broca_quantum_search_lexicon(bridge, semantic_target, 4, 100, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_candidate, nullptr);
    EXPECT_GT(result.candidates_evaluated, 0);
    EXPECT_GT(result.search_speedup, 0.0f);
}

TEST_F(BrocaQuantumBridgeTest, SearchLexiconDisabled) {
    broca_quantum_bridge_set_enabled(bridge, false);

    float semantic_target[] = {0.1f, 0.2f};
    quantum_lexical_result_t result;

    int ret = broca_quantum_search_lexicon(bridge, semantic_target, 2, 100, &result);
    EXPECT_EQ(ret, -1);  /* Should fail when disabled */
}

TEST_F(BrocaQuantumBridgeTest, OptimizeSyntax) {
    quantum_syntax_result_t result;

    int ret = broca_quantum_optimize_syntax(bridge, "test content", 5, 0.8f, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_structure, nullptr);
    EXPECT_GT(result.structures_evaluated, 0);
}

TEST_F(BrocaQuantumBridgeTest, OptimizePhonemes) {
    uint8_t phonemes[] = {1, 2, 3, 4, 5, 6, 7, 8};
    quantum_phoneme_result_t result;

    int ret = broca_quantum_optimize_phonemes(bridge, phonemes, 8, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_sequence, nullptr);
    EXPECT_GT(result.sequences_evaluated, 0);
}

TEST_F(BrocaQuantumBridgeTest, GetStats) {
    broca_quantum_stats_t stats;
    int ret = broca_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.lexical_searches, 0);
}

TEST_F(BrocaQuantumBridgeTest, StatsTrackSearches) {
    float semantic[] = {0.5f, 0.5f};
    quantum_lexical_result_t result;

    broca_quantum_search_lexicon(bridge, semantic, 2, 50, &result);
    broca_quantum_search_lexicon(bridge, semantic, 2, 50, &result);

    broca_quantum_stats_t stats;
    broca_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.lexical_searches, 2);
}

TEST_F(BrocaQuantumBridgeTest, ResetStats) {
    float semantic[] = {0.5f};
    quantum_lexical_result_t result;
    broca_quantum_search_lexicon(bridge, semantic, 1, 20, &result);

    broca_quantum_reset_stats(bridge);

    broca_quantum_stats_t stats;
    broca_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.lexical_searches, 0);
}

TEST_F(BrocaQuantumBridgeTest, GetConfig) {
    broca_quantum_config_t config;
    int ret = broca_quantum_get_config(bridge, &config);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enabled);
}

TEST_F(BrocaQuantumBridgeTest, GroverSpeedup) {
    float semantic[] = {0.1f, 0.2f, 0.3f};
    quantum_lexical_result_t result;

    /* Search in lexicon of 1000 words */
    broca_quantum_search_lexicon(bridge, semantic, 3, 1000, &result);

    /* Grover provides sqrt(N) speedup */
    /* For N=1000, speedup should be approximately sqrt(1000) ≈ 31.6 */
    EXPECT_GT(result.search_speedup, 20.0f);
}

//=============================================================================
// Null Pointer Safety Tests
//=============================================================================

class BrocaBridgesNullSafetyTest : public ::testing::Test {};

TEST_F(BrocaBridgesNullSafetyTest, SubstrateUpdateNull) {
    EXPECT_EQ(broca_substrate_bridge_update(nullptr), -1);
}

TEST_F(BrocaBridgesNullSafetyTest, SubstrateApplyNull) {
    EXPECT_EQ(broca_substrate_bridge_apply_effects(nullptr), -1);
}

TEST_F(BrocaBridgesNullSafetyTest, SubstrateDestroyNull) {
    broca_substrate_bridge_destroy(nullptr);  /* Should not crash */
}

TEST_F(BrocaBridgesNullSafetyTest, ThalamicResetNull) {
    EXPECT_EQ(broca_thalamic_bridge_reset(nullptr), -1);
}

TEST_F(BrocaBridgesNullSafetyTest, ThalamicDestroyNull) {
    broca_thalamic_bridge_destroy(nullptr);  /* Should not crash */
}

TEST_F(BrocaBridgesNullSafetyTest, QuantumDestroyNull) {
    broca_quantum_bridge_destroy(nullptr);  /* Should not crash */
}

TEST_F(BrocaBridgesNullSafetyTest, QuantumIsEnabledNull) {
    EXPECT_FALSE(broca_quantum_bridge_is_enabled(nullptr));
}

//=============================================================================
// Integration Tests
//=============================================================================

class BrocaBridgesIntegrationTest : public ::testing::Test {
protected:
    broca_substrate_bridge_t* substrate_bridge = nullptr;
    broca_thalamic_bridge_t* thalamic_bridge = nullptr;
    broca_quantum_bridge_t* quantum_bridge = nullptr;

    void SetUp() override {
        substrate_bridge = broca_substrate_bridge_create(nullptr, nullptr, nullptr);
        thalamic_bridge = broca_thalamic_bridge_create(nullptr, nullptr, nullptr);
        quantum_bridge = broca_quantum_bridge_create(nullptr, nullptr);
    }

    void TearDown() override {
        broca_substrate_bridge_destroy(substrate_bridge);
        broca_thalamic_bridge_destroy(thalamic_bridge);
        broca_quantum_bridge_destroy(quantum_bridge);
    }
};

TEST_F(BrocaBridgesIntegrationTest, AllBridgesCreated) {
    ASSERT_NE(substrate_bridge, nullptr);
    ASSERT_NE(thalamic_bridge, nullptr);
    ASSERT_NE(quantum_bridge, nullptr);
}

TEST_F(BrocaBridgesIntegrationTest, SimulateLanguageProduction) {
    /* Step 1: Quantum lexical search for word selection */
    float semantic_target[] = {0.3f, 0.5f, 0.2f};
    quantum_lexical_result_t lex_result;
    int ret = broca_quantum_search_lexicon(quantum_bridge, semantic_target, 3, 500, &lex_result);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(lex_result.best_candidate, nullptr);

    /* Step 2: Update metabolic state */
    ret = broca_substrate_bridge_update(substrate_bridge);
    ASSERT_EQ(ret, 0);

    broca_substrate_effects_t effects;
    broca_substrate_bridge_get_effects(substrate_bridge, &effects);
    EXPECT_GT(effects.speech_fluency, 0.0f);

    /* Step 3: Quantum syntax optimization */
    quantum_syntax_result_t syn_result;
    ret = broca_quantum_optimize_syntax(quantum_bridge, "test", 3, 0.7f, &syn_result);
    ASSERT_EQ(ret, 0);

    /* Step 4: Quantum phoneme optimization */
    uint8_t phonemes[] = {5, 10, 15, 20};
    quantum_phoneme_result_t phon_result;
    ret = broca_quantum_optimize_phonemes(quantum_bridge, phonemes, 4, &phon_result);
    ASSERT_EQ(ret, 0);

    /* Step 5: Signal utterance start through thalamus */
    ret = broca_thalamic_signal_utterance_start(thalamic_bridge, 1, 3);
    ASSERT_EQ(ret, 0);

    /* Step 6: Route motor commands */
    uint8_t motor_cmd[] = {1, 2, 3};
    ret = broca_thalamic_route_motor_command(thalamic_bridge, motor_cmd, 3, 0.9f);
    ASSERT_EQ(ret, 0);

    /* Step 7: Route phonemes */
    ret = broca_thalamic_route_phonemes(thalamic_bridge, phonemes, 4);
    ASSERT_EQ(ret, 0);

    /* Step 8: Signal utterance end */
    ret = broca_thalamic_signal_utterance_end(thalamic_bridge, 1);
    ASSERT_EQ(ret, 0);

    /* Verify statistics */
    broca_thalamic_stats_t thal_stats;
    broca_thalamic_bridge_get_stats(thalamic_bridge, &thal_stats);
    EXPECT_EQ(thal_stats.motor_commands_routed, 1);
    EXPECT_EQ(thal_stats.phoneme_sequences, 1);
    EXPECT_EQ(thal_stats.utterances_started, 1);
    EXPECT_EQ(thal_stats.utterances_completed, 1);

    broca_quantum_stats_t quant_stats;
    broca_quantum_get_stats(quantum_bridge, &quant_stats);
    EXPECT_EQ(quant_stats.lexical_searches, 1);
    EXPECT_EQ(quant_stats.syntax_optimizations, 1);
    EXPECT_EQ(quant_stats.phoneme_optimizations, 1);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
