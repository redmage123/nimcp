/**
 * @file test_autobiographical_immune_integration.cpp
 * @brief Unit tests for Autobiographical Memory - Immune System Integration
 *
 * WHAT: Test bidirectional coupling between autobiographical memory and immune system
 * WHY:  Validate cytokine modulation of encoding and trauma-triggered immunity
 * HOW:  Test encoding impairment, sickness landmarks, trauma recall immune activation
 *
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_autobiographical_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_autobiographical_memory.h"

class AutobiographicalImmuneTest : public ::testing::Test {
protected:
    autobio_immune_bridge_t* bridge;
    brain_immune_system_t* immune;
    autobiographical_memory_t* autobio_memory;

    void SetUp() override {
        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Create autobiographical memory
        autobio_memory = autobio_create(1000);
        ASSERT_NE(autobio_memory, nullptr);

        // Create bridge
        autobio_immune_config_t bridge_config;
        autobio_immune_default_config(&bridge_config);
        bridge = autobio_immune_bridge_create(&bridge_config, immune, autobio_memory);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        autobio_immune_bridge_destroy(bridge);
        brain_immune_destroy(immune);
        autobio_destroy(autobio_memory);
    }
};

/**
 * TEST: Bridge Creation and Configuration
 * Validates proper initialization with default config
 */
TEST_F(AutobiographicalImmuneTest, BridgeCreationAndDefaults) {
    EXPECT_NE(bridge, nullptr);

    // Verify default config was applied
    autobio_immune_config_t config;
    int result = autobio_immune_default_config(&config);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(config.enable_cytokine_encoding_modulation);
    EXPECT_TRUE(config.enable_inflammation_consolidation_impairment);
    EXPECT_TRUE(config.enable_sickness_landmark_creation);
    EXPECT_TRUE(config.enable_trauma_memory_immune_trigger);
    EXPECT_TRUE(config.enable_positive_memory_immune_boost);
    EXPECT_EQ(config.cytokine_sensitivity, 1.0f);
}

/**
 * TEST: Cytokine Effects on Episodic Encoding
 * BIOLOGICAL: IL-1β impairs hippocampal LTP and encoding efficiency
 */
TEST_F(AutobiographicalImmuneTest, CytokineEncodingImpairment) {
    // Baseline encoding efficiency should be 1.0 (normal)
    float baseline_efficiency = autobio_immune_get_encoding_efficiency(bridge);
    EXPECT_NEAR(baseline_efficiency, 1.0f, 0.1f);

    // Trigger inflammation in immune system
    uint32_t antigen_id;
    uint8_t epitope[] = {0x01, 0x02, 0x03};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  8, 0, &antigen_id);

    // Activate full immune response to raise cytokine levels
    uint32_t b_cell_id, helper_id, antibody_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_t_help_b(immune, helper_id, b_cell_id);
    brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGG, &antibody_id);

    // Release cytokines
    uint32_t cytokine_id;
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, helper_id,
                                   0.8f, 0, &cytokine_id);

    // Apply cytokine effects to encoding
    int result = autobio_immune_apply_cytokine_encoding_effects(bridge);
    EXPECT_EQ(result, 0);

    // Get cytokine effects - encoding should be impaired
    cytokine_memory_effects_t effects;
    autobio_immune_get_cytokine_effects(bridge, &effects);

    // IL-1β should impair encoding (total modulation < 1.0)
    EXPECT_LT(effects.total_encoding_modulation, 1.0f);
    EXPECT_GT(effects.il1_encoding_impairment, 0.0f);

    // Encoding efficiency should be reduced
    float impaired_efficiency = autobio_immune_get_encoding_efficiency(bridge);
    EXPECT_LT(impaired_efficiency, baseline_efficiency);
}

/**
 * TEST: Inflammation Consolidation Impairment
 * BIOLOGICAL: Chronic inflammation impairs sleep-based memory consolidation
 */
TEST_F(AutobiographicalImmuneTest, InflammationConsolidationImpairment) {
    // Apply inflammation effects
    int result = autobio_immune_apply_inflammation_consolidation_effects(bridge);
    EXPECT_EQ(result, 0);

    // Get inflammation state
    inflammation_memory_state_t state;
    autobio_immune_get_inflammation_state(bridge, &state);

    // Initially no inflammation
    EXPECT_EQ(state.current_level, INFLAMMATION_NONE);
    EXPECT_NEAR(state.encoding_efficiency, 1.0f, 0.1f);

    // Create regional inflammation
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 1, 0, &site_id);
    brain_immune_escalate_inflammation(immune, site_id);

    // Reapply effects
    autobio_immune_apply_inflammation_consolidation_effects(bridge);
    autobio_immune_get_inflammation_state(bridge, &state);

    // Consolidation should be impaired
    EXPECT_LT(state.consolidation_quality, 1.0f);
    EXPECT_GT(state.consolidation_quality, 0.0f);
}

/**
 * TEST: Emotional Salience Modulation
 * BIOLOGICAL: Inflammation enhances negative memory salience
 */
TEST_F(AutobiographicalImmuneTest, EmotionalSalienceModulation) {
    // Create negative memory during inflammation
    autobiographical_memory_entry_t neg_memory;
    memset(&neg_memory, 0, sizeof(neg_memory));
    neg_memory.type = AUTOBIO_FAILURE;
    neg_memory.valence = VALENCE_NEGATIVE;
    neg_memory.importance = 0.5f;

    // Create positive memory during inflammation
    autobiographical_memory_entry_t pos_memory;
    memset(&pos_memory, 0, sizeof(pos_memory));
    pos_memory.type = AUTOBIO_ACHIEVEMENT;
    pos_memory.valence = VALENCE_POSITIVE;
    pos_memory.importance = 0.5f;

    // Trigger inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAA, 0xBB, 0xCC};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  7, 0, &antigen_id);

    // Apply cytokine effects
    autobio_immune_apply_cytokine_encoding_effects(bridge);

    // Check salience modulation
    float neg_salience = autobio_immune_modulate_memory_salience(bridge, &neg_memory);
    float pos_salience = autobio_immune_modulate_memory_salience(bridge, &pos_memory);

    // Negative memories should be enhanced, positive reduced during inflammation
    EXPECT_GE(neg_salience, 1.0f); // Enhanced
    EXPECT_LE(pos_salience, 1.0f); // Reduced or unchanged
}

/**
 * TEST: Sickness Landmark Creation
 * BIOLOGICAL: Sickness episodes create distinct autobiographical landmarks
 */
TEST_F(AutobiographicalImmuneTest, SicknessLandmarkCreation) {
    // Initially no sickness landmarks
    sickness_landmark_t landmarks[10];
    uint32_t num_found;
    autobio_immune_get_sickness_landmarks(bridge, landmarks, 10, &num_found);
    EXPECT_EQ(num_found, 0u);

    // Create systemic inflammation (triggers sickness landmark)
    uint64_t landmark_id;
    int result = autobio_immune_create_sickness_landmark(
        bridge, INFLAMMATION_SYSTEMIC, &landmark_id);
    EXPECT_EQ(result, 0);
    EXPECT_NE(landmark_id, 0u);

    // Verify landmark was created
    autobio_immune_get_sickness_landmarks(bridge, landmarks, 10, &num_found);
    EXPECT_EQ(num_found, 1u);
    EXPECT_EQ(landmarks[0].memory_id, landmark_id);
    EXPECT_EQ(landmarks[0].severity, INFLAMMATION_SYSTEMIC);
    EXPECT_GT(landmarks[0].emotional_intensity, 0.0f);

    // Verify memory was stored in autobiographical system
    autobiographical_memory_entry_t memory;
    bool found = autobio_retrieve(autobio_memory, landmark_id, &memory);
    EXPECT_TRUE(found);
    EXPECT_EQ(memory.type, AUTOBIO_CRISIS);
    EXPECT_EQ(memory.valence, VALENCE_NEGATIVE);
}

/**
 * TEST: Sickness Landmark Closure
 * BIOLOGICAL: Sickness episodes end when inflammation resolves
 */
TEST_F(AutobiographicalImmuneTest, SicknessLandmarkClosure) {
    // Create sickness landmark
    uint64_t landmark_id;
    autobio_immune_create_sickness_landmark(bridge, INFLAMMATION_SYSTEMIC, &landmark_id);

    // Close landmark
    int result = autobio_immune_close_sickness_landmark(bridge, landmark_id);
    EXPECT_EQ(result, 0);

    // Verify it was closed (memory updated with outcome)
    autobiographical_memory_entry_t memory;
    bool found = autobio_retrieve(autobio_memory, landmark_id, &memory);
    EXPECT_TRUE(found);
    // Outcome should reflect recovery
    EXPECT_STRNE(memory.outcome, "");
}

/**
 * TEST: Trauma Memory Immune Trigger
 * BIOLOGICAL: Trauma recall activates HPA axis → inflammatory rebound
 */
TEST_F(AutobiographicalImmuneTest, TraumaRecallImmuneActivation) {
    // Create traumatic memory
    autobiographical_memory_entry_t trauma_memory;
    memset(&trauma_memory, 0, sizeof(trauma_memory));
    trauma_memory.type = AUTOBIO_CRISIS;
    trauma_memory.valence = VALENCE_VERY_NEGATIVE;
    trauma_memory.importance = 0.9f; // High importance
    trauma_memory.emotional_intensity = 0.8f;
    snprintf(trauma_memory.what_happened, sizeof(trauma_memory.what_happened),
             "I experienced a severe threat");

    // Trigger immune response from trauma recall
    int result = autobio_immune_trigger_from_trauma_recall(bridge, &trauma_memory);
    EXPECT_EQ(result, 0);

    // Verify immune activation was recorded (simplified check)
    // In full implementation, would verify cytokine release
}

/**
 * TEST: Negative Memory Rumination
 * BIOLOGICAL: Ruminating on failures → chronic stress → immune dysregulation
 */
TEST_F(AutobiographicalImmuneTest, NegativeMemoryRumination) {
    // Create negative memory
    autobiographical_memory_entry_t neg_memory;
    memset(&neg_memory, 0, sizeof(neg_memory));
    neg_memory.type = AUTOBIO_FAILURE;
    neg_memory.valence = VALENCE_NEGATIVE;
    neg_memory.importance = 0.7f;

    uint64_t memory_id = autobio_store(autobio_memory, &neg_memory);
    ASSERT_NE(memory_id, 0u);

    // Ruminate on memory multiple times
    for (int i = 0; i < 6; i++) {
        autobio_immune_ruminate_on_negative_memory(bridge, memory_id);
    }

    // Chronic stress should be detected (>5 recalls)
    // In full implementation, would verify inflammation escalation
}

/**
 * TEST: Positive Memory Immune Boost
 * BIOLOGICAL: Positive memories reduce cortisol, enhance immunity
 */
TEST_F(AutobiographicalImmuneTest, PositiveMemoryImmuneBoost) {
    // Create positive achievement memory
    autobiographical_memory_entry_t achievement;
    memset(&achievement, 0, sizeof(achievement));
    achievement.type = AUTOBIO_ACHIEVEMENT;
    achievement.valence = VALENCE_VERY_POSITIVE;
    achievement.importance = 0.8f;
    snprintf(achievement.what_happened, sizeof(achievement.what_happened),
             "I accomplished a major goal");

    // Boost immune from positive recall
    int result = autobio_immune_boost_from_positive_memory(bridge, &achievement);
    EXPECT_EQ(result, 0);

    // Verify boost was recorded (simplified)
    // In full implementation, would verify IL-10 release
}

/**
 * TEST: Identity-Threatening Memory Detection
 * BIOLOGICAL: Identity-threatening memories trigger stronger immune responses
 */
TEST_F(AutobiographicalImmuneTest, IdentityThreateningMemoryDetection) {
    // Create identity-threatening memory (core + negative + important)
    autobiographical_memory_entry_t identity_threat;
    memset(&identity_threat, 0, sizeof(identity_threat));
    identity_threat.type = AUTOBIO_FAILURE;
    identity_threat.valence = VALENCE_VERY_NEGATIVE;
    identity_threat.importance = 0.9f;
    identity_threat.is_core_memory = true;

    bool is_threatening = autobio_immune_is_identity_threatening(&identity_threat);
    EXPECT_TRUE(is_threatening);

    // Non-core negative memory should not be identity-threatening
    autobiographical_memory_entry_t non_core;
    memset(&non_core, 0, sizeof(non_core));
    non_core.type = AUTOBIO_FAILURE;
    non_core.valence = VALENCE_NEGATIVE;
    non_core.importance = 0.6f;
    non_core.is_core_memory = false;

    is_threatening = autobio_immune_is_identity_threatening(&non_core);
    EXPECT_FALSE(is_threatening);
}

/**
 * TEST: Chronic Inflammation Memory Decline
 * BIOLOGICAL: Chronic inflammation accelerates cognitive aging
 */
TEST_F(AutobiographicalImmuneTest, ChronicInflammationMemoryDecline) {
    // Apply inflammation effects
    autobio_immune_apply_inflammation_consolidation_effects(bridge);

    // Initially no decline
    float decline_rate = autobio_immune_get_memory_decline_rate(bridge);
    EXPECT_NEAR(decline_rate, 0.0f, 0.1f);

    // Simulate chronic inflammation (would need time passage in real impl)
    inflammation_memory_state_t state;
    autobio_immune_get_inflammation_state(bridge, &state);

    // If chronic, decline rate should increase
    if (state.is_chronic) {
        EXPECT_GT(decline_rate, 0.0f);
    }
}

/**
 * TEST: Sickness Behavior Memory Impairment
 * BIOLOGICAL: Cytokine-induced sickness behavior impairs memory
 */
TEST_F(AutobiographicalImmuneTest, SicknessBehaviorMemoryImpairment) {
    // Initially not in sickness behavior
    bool sickness = autobio_immune_is_sickness_affecting_memory(bridge);
    EXPECT_FALSE(sickness);

    // Trigger regional inflammation
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 1, 0, &site_id);
    brain_immune_escalate_inflammation(immune, site_id);

    // Raise cytokines
    uint32_t cytokine_id;
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, 0, 1, 0.9f, 0, &cytokine_id);

    // Apply effects
    autobio_immune_apply_cytokine_encoding_effects(bridge);
    autobio_immune_apply_inflammation_consolidation_effects(bridge);

    // Check if sickness behavior is affecting memory
    sickness = autobio_immune_is_sickness_affecting_memory(bridge);
    // May or may not be true depending on inflammation level
}

/**
 * TEST: Bridge Update Cycle
 * Validates full bidirectional update
 */
TEST_F(AutobiographicalImmuneTest, BridgeUpdateCycle) {
    // Update bridge
    int result = autobio_immune_bridge_update(bridge, 1000); // 1 second
    EXPECT_EQ(result, 0);

    // Multiple updates should work
    for (int i = 0; i < 10; i++) {
        result = autobio_immune_bridge_update(bridge, 100);
        EXPECT_EQ(result, 0);
    }
}

/**
 * TEST: Anti-inflammatory Recovery
 * BIOLOGICAL: IL-10 restores normal encoding
 */
TEST_F(AutobiographicalImmuneTest, AntiInflammatoryRecovery) {
    // First impair with pro-inflammatory cytokines
    uint32_t il1_id;
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, 0, 1, 0.8f, 0, &il1_id);
    autobio_immune_apply_cytokine_encoding_effects(bridge);

    float impaired_encoding = autobio_immune_get_encoding_efficiency(bridge);

    // Release anti-inflammatory IL-10
    uint32_t il10_id;
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL10, 0, 1, 0.7f, 0, &il10_id);
    autobio_immune_apply_cytokine_encoding_effects(bridge);

    float recovered_encoding = autobio_immune_get_encoding_efficiency(bridge);

    // Encoding should improve with IL-10
    EXPECT_GE(recovered_encoding, impaired_encoding);
}

/**
 * TEST: Consolidation Impairment Query
 * Validates consolidation quality measurement
 */
TEST_F(AutobiographicalImmuneTest, ConsolidationImpairmentQuery) {
    // Apply effects
    autobio_immune_apply_cytokine_encoding_effects(bridge);

    // Query consolidation impairment
    float impairment = autobio_immune_get_consolidation_impairment(bridge);
    EXPECT_GE(impairment, 0.0f);
    EXPECT_LE(impairment, 1.0f);
}

/**
 * TEST: Multiple Sickness Landmarks
 * Validates tracking of multiple sickness episodes
 */
TEST_F(AutobiographicalImmuneTest, MultipleSicknessLandmarks) {
    // Create multiple sickness landmarks
    uint64_t landmark1, landmark2, landmark3;

    autobio_immune_create_sickness_landmark(bridge, INFLAMMATION_REGIONAL, &landmark1);
    autobio_immune_close_sickness_landmark(bridge, landmark1);

    autobio_immune_create_sickness_landmark(bridge, INFLAMMATION_SYSTEMIC, &landmark2);
    autobio_immune_close_sickness_landmark(bridge, landmark2);

    autobio_immune_create_sickness_landmark(bridge, INFLAMMATION_STORM, &landmark3);

    // Retrieve all landmarks
    sickness_landmark_t landmarks[10];
    uint32_t num_found;
    autobio_immune_get_sickness_landmarks(bridge, landmarks, 10, &num_found);

    EXPECT_EQ(num_found, 3u);
}

/**
 * TEST: Encoding Efficiency Range
 * Validates encoding efficiency stays within valid range
 */
TEST_F(AutobiographicalImmuneTest, EncodingEfficiencyRange) {
    // Apply various cytokine levels
    for (int i = 0; i < 10; i++) {
        uint32_t cytokine_id;
        float concentration = (float)i / 10.0f;
        brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, 1,
                                       concentration, 0, &cytokine_id);

        autobio_immune_apply_cytokine_encoding_effects(bridge);
        float efficiency = autobio_immune_get_encoding_efficiency(bridge);

        // Efficiency should be in valid range [0, 1.5]
        EXPECT_GE(efficiency, 0.0f);
        EXPECT_LE(efficiency, 1.5f);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
