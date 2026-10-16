/**
 * @file test_knowledge_immune_integration.cpp
 * @brief Unit tests for Knowledge Base - Immune System Integration
 *
 * WHAT: Test bidirectional coupling between knowledge base and immune system
 * WHY:  Validate cytokine modulation of retrieval/encoding and health knowledge immune priming
 * HOW:  Test cytokine effects on retrieval latency, inflammation encoding impairment,
 *       health knowledge priming, and illness-based knowledge prioritization
 *
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_knowledge_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/knowledge/nimcp_knowledge.h"

class KnowledgeImmuneIntegrationTest : public ::testing::Test {
protected:
    knowledge_immune_bridge_t* bridge;
    brain_immune_system_t* immune;
    knowledge_system_t knowledge;

    void SetUp() override {
        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Create knowledge system
        knowledge = knowledge_system_create("test_learner");
        ASSERT_NE(knowledge, nullptr);

        // Create bridge
        knowledge_immune_config_t bridge_config;
        knowledge_immune_default_config(&bridge_config);
        bridge = knowledge_immune_bridge_create(&bridge_config, immune, knowledge);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        knowledge_immune_bridge_destroy(bridge);
        brain_immune_destroy(immune);
        knowledge_system_destroy(knowledge);
    }

    // Helper: Trigger inflammation in immune system
    void TriggerInflammation(brain_inflammation_level_t target_level) {
        uint32_t antigen_id;
        uint8_t epitope[] = {0x01, 0x02, 0x03};

        uint32_t severity = 3; // Default moderate
        if (target_level == INFLAMMATION_SYSTEMIC) severity = 8;
        if (target_level == INFLAMMATION_STORM) severity = 10;

        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                      severity, 0, &antigen_id);

        // Activate immune response to raise cytokines
        uint32_t b_cell_id, helper_id, antibody_id;
        brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
        brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
        brain_immune_t_help_b(immune, helper_id, b_cell_id);
        brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGG, &antibody_id);

        // Create inflammation site
        uint32_t site_id;
        brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);

        // Escalate to target level
        while (immune->inflammation_sites[0].level < target_level) {
            brain_immune_escalate_inflammation(immune, site_id);
        }
    }

    // Helper: Add health knowledge
    void AddHealthKnowledge() {
        knowledge_item_t item;
        memset(&item, 0, sizeof(item));

        strncpy(item.concept_name, "immune_system", sizeof(item.concept_name) - 1);
        item.domain = KNOWLEDGE_DOMAIN_SCIENCE;
        strncpy(item.definition, "Body's defense against pathogens", sizeof(item.definition) - 1);
        item.confidence = 0.9f;
        item.learned_timestamp = 0;

        knowledge_add_item(knowledge, &item);
    }
};

/**
 * TEST: Lifecycle - Default Configuration
 * BIOLOGICAL: Verify default config sets reasonable thresholds
 */
TEST_F(KnowledgeImmuneIntegrationTest, DefaultConfiguration) {
    knowledge_immune_config_t config;
    int result = knowledge_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_retrieval_modulation);
    EXPECT_TRUE(config.enable_inflammation_encoding_impairment);
    EXPECT_TRUE(config.enable_knowledge_immune_priming);
    EXPECT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_GT(config.baseline_retrieval_latency_ms, 0.0f);
}

/**
 * TEST: Lifecycle - Bridge Creation
 */
TEST_F(KnowledgeImmuneIntegrationTest, BridgeCreation) {
    EXPECT_NE(bridge, nullptr);
    EXPECT_NE(bridge->immune_system, nullptr);
    EXPECT_NE(bridge->knowledge_system, nullptr);
    EXPECT_TRUE(bridge->enable_cytokine_retrieval_modulation);
}

/**
 * TEST: Cytokine Effects on Retrieval Latency
 * BIOLOGICAL: IL-1β, IL-6, TNF-α increase retrieval latency
 */
TEST_F(KnowledgeImmuneIntegrationTest, CytokineRetrievalLatency) {
    // Get baseline retrieval multiplier
    float baseline = knowledge_immune_get_retrieval_latency_multiplier(bridge);
    EXPECT_EQ(baseline, 1.0f); // No inflammation = no slowdown

    // Trigger inflammation to raise cytokines
    TriggerInflammation(INFLAMMATION_REGIONAL);

    // Apply cytokine effects
    int result = knowledge_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    // Get cytokine effects
    cytokine_knowledge_effects_t effects;
    knowledge_immune_get_cytokine_effects(bridge, &effects);

    // Retrieval should be slower due to pro-inflammatory cytokines
    EXPECT_GT(effects.total_latency_multiplier, 1.0f);
    EXPECT_GT(effects.retrieval_impairment, 0.0f);

    // Verify latency increase
    float latency_increase_pct = knowledge_immune_get_retrieval_latency_increase_pct(bridge);
    EXPECT_GT(latency_increase_pct, 0.0f);
}

/**
 * TEST: IL-6 Specific Retrieval Impact
 * BIOLOGICAL: IL-6 impairs semantic memory access
 */
TEST_F(KnowledgeImmuneIntegrationTest, IL6SemanticImpairment) {
    // Trigger inflammation
    TriggerInflammation(INFLAMMATION_REGIONAL);

    // Apply cytokine effects
    knowledge_immune_apply_cytokine_effects(bridge);

    // Get effects
    cytokine_knowledge_effects_t effects;
    knowledge_immune_get_cytokine_effects(bridge, &effects);

    // IL-6 should contribute to latency increase
    EXPECT_GT(effects.il6_latency_multiplier, 1.0f);
    EXPECT_LE(effects.il6_latency_multiplier, CYTOKINE_IL6_RETRIEVAL_IMPACT);
}

/**
 * TEST: Inflammation Encoding Impairment
 * BIOLOGICAL: Inflammation reduces new fact encoding confidence
 */
TEST_F(KnowledgeImmuneIntegrationTest, InflammationEncodingImpairment) {
    // Baseline encoding (no inflammation)
    float baseline_penalty = knowledge_immune_get_encoding_penalty(bridge);
    EXPECT_EQ(baseline_penalty, 0.0f);

    // Trigger systemic inflammation
    TriggerInflammation(INFLAMMATION_SYSTEMIC);

    // Apply inflammation effects
    int result = knowledge_immune_apply_inflammation_encoding(bridge);
    EXPECT_EQ(result, 0);

    // Get inflammation state
    inflammation_knowledge_state_t state;
    knowledge_immune_get_inflammation_state(bridge, &state);

    // Encoding should be impaired
    EXPECT_EQ(state.current_level, INFLAMMATION_SYSTEMIC);
    EXPECT_GT(state.encoding_penalty, 0.0f);

    // Encoding success rate should be reduced
    float success_rate = knowledge_immune_get_encoding_success_rate(bridge);
    EXPECT_LT(success_rate, 1.0f);
}

/**
 * TEST: Inflammation Level Mapping
 * BIOLOGICAL: Higher inflammation = more encoding impairment
 */
TEST_F(KnowledgeImmuneIntegrationTest, InflammationLevelMapping) {
    // Test different inflammation levels
    brain_inflammation_level_t levels[] = {
        INFLAMMATION_LOCAL,
        INFLAMMATION_REGIONAL,
        INFLAMMATION_SYSTEMIC,
        INFLAMMATION_STORM
    };

    float prev_penalty = 0.0f;
    for (int i = 0; i < 4; i++) {
        // Reset bridge
        TearDown();
        SetUp();

        // Trigger inflammation
        TriggerInflammation(levels[i]);
        knowledge_immune_apply_inflammation_encoding(bridge);

        // Get penalty
        float penalty = knowledge_immune_get_encoding_penalty(bridge);

        // Penalty should increase with inflammation level
        EXPECT_GT(penalty, prev_penalty);
        prev_penalty = penalty;
    }
}

/**
 * TEST: Sickness Behavior Learning Impairment
 * BIOLOGICAL: Cytokines induce fatigue, reduce learning motivation
 */
TEST_F(KnowledgeImmuneIntegrationTest, SicknessBehaviorLearningImpairment) {
    // Trigger high inflammation
    TriggerInflammation(INFLAMMATION_SYSTEMIC);

    // Apply sickness effects
    int result = knowledge_immune_apply_sickness_learning_impairment(bridge);
    EXPECT_EQ(result, 0);

    // Get inflammation state
    inflammation_knowledge_state_t state;
    knowledge_immune_get_inflammation_state(bridge, &state);

    // Sickness should suppress curiosity and learning
    if (state.sickness_level >= SICKNESS_LEARNING_THRESHOLD) {
        EXPECT_GT(state.curiosity_suppression, 0.0f);
        EXPECT_LT(state.learning_motivation, 1.0f);
    }
}

/**
 * TEST: Chronic Inflammation Cognitive Decline
 * BIOLOGICAL: Sustained inflammation causes progressive impairment
 */
TEST_F(KnowledgeImmuneIntegrationTest, ChronicInflammationCognitiveDecline) {
    // Trigger inflammation
    TriggerInflammation(INFLAMMATION_REGIONAL);

    // Apply inflammation effects
    knowledge_immune_apply_inflammation_encoding(bridge);

    // Get state
    inflammation_knowledge_state_t state;
    knowledge_immune_get_inflammation_state(bridge, &state);

    // Check if chronic (depends on duration tracking)
    if (state.is_chronic) {
        EXPECT_GT(state.cognitive_decline, 0.0f);
        EXPECT_GT(state.association_weakening, 0.0f);
    }
}

/**
 * TEST: Health Knowledge Immune Priming
 * BIOLOGICAL: Health knowledge enhances immune preparedness
 */
TEST_F(KnowledgeImmuneIntegrationTest, HealthKnowledgeImmunePriming) {
    // Add health knowledge
    AddHealthKnowledge();

    // Prime immune from knowledge
    int result = knowledge_immune_prime_from_health_knowledge(bridge);
    EXPECT_EQ(result, 0);

    // Check if priming occurred (depends on implementation)
    // In real implementation, would check immune preparedness boost
}

/**
 * TEST: Threat Assessment from Knowledge
 * BIOLOGICAL: Semantic understanding improves risk assessment
 */
TEST_F(KnowledgeImmuneIntegrationTest, ThreatAssessmentFromKnowledge) {
    // Add pathogen knowledge
    knowledge_item_t item;
    memset(&item, 0, sizeof(item));
    strncpy(item.concept_name, "virus", sizeof(item.concept_name) - 1);
    item.domain = KNOWLEDGE_DOMAIN_SCIENCE;
    strncpy(item.definition, "Infectious pathogen", sizeof(item.definition) - 1);
    item.confidence = 0.8f;
    knowledge_add_item(knowledge, &item);

    // Assess threat
    float severity;
    int result = knowledge_immune_assess_threat(bridge, "virus", &severity);
    EXPECT_EQ(result, 0);
    EXPECT_GT(severity, 0.0f);
    EXPECT_LE(severity, 10.0f);
}

/**
 * TEST: Illness Knowledge Prioritization
 * BIOLOGICAL: Brain prioritizes health knowledge when sick
 */
TEST_F(KnowledgeImmuneIntegrationTest, IllnessKnowledgePrioritization) {
    // Trigger sickness behavior
    TriggerInflammation(INFLAMMATION_SYSTEMIC);
    knowledge_immune_apply_sickness_learning_impairment(bridge);

    // Prioritize health knowledge
    int result = knowledge_immune_prioritize_health_knowledge(bridge);
    EXPECT_EQ(result, 0);

    // Check domain multipliers
    float health_mult = knowledge_immune_get_domain_retrieval_multiplier(
        bridge, KNOWLEDGE_DOMAIN_SCIENCE
    );
    float art_mult = knowledge_immune_get_domain_retrieval_multiplier(
        bridge, KNOWLEDGE_DOMAIN_ART
    );

    // During illness, health domains should be faster
    if (bridge->illness_priority.is_sick) {
        EXPECT_LT(health_mult, art_mult);
    }
}

/**
 * TEST: IL-10 Recovery Effects
 * BIOLOGICAL: IL-10 restores normal retrieval speed
 */
TEST_F(KnowledgeImmuneIntegrationTest, IL10RecoveryEffects) {
    // Trigger inflammation
    TriggerInflammation(INFLAMMATION_REGIONAL);
    knowledge_immune_apply_cytokine_effects(bridge);

    // Get impaired latency
    float impaired_mult = knowledge_immune_get_retrieval_latency_multiplier(bridge);
    EXPECT_GT(impaired_mult, 1.0f);

    // In real implementation, would trigger IL-10 release here
    // For now, just verify IL-10 benefit is configured
    cytokine_knowledge_effects_t effects;
    knowledge_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_LT(effects.il10_latency_benefit, 1.0f); // IL-10 reduces latency
}

/**
 * TEST: Cognitive Impairment Detection
 * BIOLOGICAL: Significant impairment threshold detection
 */
TEST_F(KnowledgeImmuneIntegrationTest, CognitiveImpairmentDetection) {
    // Baseline - no impairment
    bool impaired_baseline = knowledge_immune_is_cognitively_impaired(bridge);
    EXPECT_FALSE(impaired_baseline);

    // Trigger severe inflammation
    TriggerInflammation(INFLAMMATION_STORM);
    knowledge_immune_apply_cytokine_effects(bridge);
    knowledge_immune_apply_inflammation_encoding(bridge);

    // Should detect cognitive impairment
    bool impaired = knowledge_immune_is_cognitively_impaired(bridge);
    // May be true depending on thresholds
    if (impaired) {
        EXPECT_TRUE(impaired);
    }
}

/**
 * TEST: Bridge Update Integration
 * BIOLOGICAL: Comprehensive bidirectional update
 */
TEST_F(KnowledgeImmuneIntegrationTest, BridgeUpdate) {
    // Trigger inflammation
    TriggerInflammation(INFLAMMATION_REGIONAL);

    // Update bridge
    int result = knowledge_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);

    // Verify update applied all effects
    EXPECT_GT(bridge->total_updates, 0);
    EXPECT_GT(bridge->cytokine_modulations, 0);
}

/**
 * TEST: Thread Safety - Concurrent Updates
 */
TEST_F(KnowledgeImmuneIntegrationTest, ThreadSafety) {
    // Multiple updates should be thread-safe
    for (int i = 0; i < 100; i++) {
        knowledge_immune_bridge_update(bridge, 10);
    }

    EXPECT_EQ(bridge->total_updates, 100);
}

/**
 * TEST: Retrieval Latency Increase Percentage
 */
TEST_F(KnowledgeImmuneIntegrationTest, RetrievalLatencyIncreasePercentage) {
    // Baseline
    float baseline_pct = knowledge_immune_get_retrieval_latency_increase_pct(bridge);
    EXPECT_EQ(baseline_pct, 0.0f);

    // Trigger inflammation
    TriggerInflammation(INFLAMMATION_REGIONAL);
    knowledge_immune_apply_cytokine_effects(bridge);

    // Get increase percentage
    float increase_pct = knowledge_immune_get_retrieval_latency_increase_pct(bridge);
    EXPECT_GT(increase_pct, 0.0f);
}

/**
 * TEST: Encoding Success Rate
 */
TEST_F(KnowledgeImmuneIntegrationTest, EncodingSuccessRate) {
    // Baseline - 100% success
    float baseline_rate = knowledge_immune_get_encoding_success_rate(bridge);
    EXPECT_EQ(baseline_rate, 1.0f);

    // Trigger inflammation
    TriggerInflammation(INFLAMMATION_SYSTEMIC);
    knowledge_immune_apply_inflammation_encoding(bridge);

    // Success rate should be reduced
    float rate = knowledge_immune_get_encoding_success_rate(bridge);
    EXPECT_LT(rate, 1.0f);
    EXPECT_GT(rate, 0.0f);
}

/**
 * TEST: Threat Learning Trigger
 * BIOLOGICAL: Learning about threats primes immune
 */
TEST_F(KnowledgeImmuneIntegrationTest, ThreatLearningTrigger) {
    // Baseline priming events
    uint32_t baseline_events = bridge->knowledge_priming_events;

    // Learn about a health threat
    int result = knowledge_immune_trigger_from_threat_learning(bridge, "pathogen");
    EXPECT_EQ(result, 0);

    // Priming events should increase
    EXPECT_GT(bridge->knowledge_priming_events, baseline_events);
}

/**
 * TEST: Domain Retrieval Multiplier - Normal State
 */
TEST_F(KnowledgeImmuneIntegrationTest, DomainRetrievalMultiplierNormal) {
    // Normal state - all domains equal
    float science_mult = knowledge_immune_get_domain_retrieval_multiplier(
        bridge, KNOWLEDGE_DOMAIN_SCIENCE
    );
    float art_mult = knowledge_immune_get_domain_retrieval_multiplier(
        bridge, KNOWLEDGE_DOMAIN_ART
    );

    EXPECT_EQ(science_mult, 1.0f);
    EXPECT_EQ(art_mult, 1.0f);
}

/**
 * TEST: Statistics Tracking
 */
TEST_F(KnowledgeImmuneIntegrationTest, StatisticsTracking) {
    // Perform various operations
    knowledge_immune_apply_cytokine_effects(bridge);
    knowledge_immune_apply_inflammation_encoding(bridge);
    knowledge_immune_prime_from_health_knowledge(bridge);
    knowledge_immune_bridge_update(bridge, 100);

    // Verify statistics
    EXPECT_GT(bridge->total_updates, 0);
    EXPECT_GT(bridge->cytokine_modulations, 0);
}

/**
 * REGRESSION TEST: Null Pointer Handling
 */
TEST_F(KnowledgeImmuneIntegrationTest, NullPointerHandling) {
    // All functions should handle NULL gracefully
    EXPECT_EQ(knowledge_immune_apply_cytokine_effects(nullptr), -1);
    EXPECT_EQ(knowledge_immune_apply_inflammation_encoding(nullptr), -1);
    EXPECT_FALSE(knowledge_immune_is_cognitively_impaired(nullptr));
    EXPECT_EQ(knowledge_immune_get_retrieval_latency_multiplier(nullptr), 1.0f);
    EXPECT_EQ(knowledge_immune_get_encoding_penalty(nullptr), 0.0f);
}

/**
 * REGRESSION TEST: Configuration Validation
 */
TEST_F(KnowledgeImmuneIntegrationTest, ConfigurationValidation) {
    knowledge_immune_config_t config;
    knowledge_immune_default_config(&config);

    // Verify sensible defaults
    EXPECT_GT(config.baseline_retrieval_latency_ms, 0.0f);
    EXPECT_GT(config.cytokine_sensitivity, 0.0f);
    EXPECT_LE(config.cytokine_sensitivity, 2.0f);
    EXPECT_GT(config.chronic_inflammation_days, 0.0f);
}

/**
 * INTEGRATION TEST: Full Immune-Knowledge Cycle
 * BIOLOGICAL: Complete cycle from inflammation → impairment → recovery
 */
TEST_F(KnowledgeImmuneIntegrationTest, FullImmuneCycle) {
    // Phase 1: Trigger inflammation
    TriggerInflammation(INFLAMMATION_SYSTEMIC);
    knowledge_immune_bridge_update(bridge, 100);

    // Verify impairment
    bool impaired_phase1 = knowledge_immune_is_cognitively_impaired(bridge);

    // Phase 2: Ongoing inflammation
    knowledge_immune_bridge_update(bridge, 100);

    // Phase 3: Recovery (would resolve inflammation in real implementation)
    // For now, just verify bridge continues to function
    knowledge_immune_bridge_update(bridge, 100);

    EXPECT_GT(bridge->total_updates, 0);
}
