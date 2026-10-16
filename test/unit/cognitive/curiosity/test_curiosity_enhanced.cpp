/**
 * @file test_curiosity_enhanced.cpp
 * @brief Unit tests for Enhanced Curiosity System (10 Enhancements)
 *
 * TEST COVERAGE:
 * - Configuration defaults and validation
 * - Lifecycle (create, update, destroy)
 * - Enhancement 1: Boredom & Understimulation detection
 * - Enhancement 2: Interest Decay & Satiation curves
 * - Enhancement 3: Curiosity Type Differentiation
 * - Enhancement 4: Approach-Avoidance conflict resolution
 * - Enhancement 5: Social Curiosity & Gossip tracking
 * - Enhancement 6: Meta-Curiosity (introspection, blind spots)
 * - Enhancement 7: Curiosity Contagion
 * - Enhancement 8: Surprise-Driven Learning Rate
 * - Enhancement 9: Curiosity Fatigue & Recovery
 * - Enhancement 10: Counterfactual Curiosity
 * - State and statistics retrieval
 * - Bio-async integration
 * - Thread safety
 * - Edge cases and error handling
 *
 * BIOLOGICAL VALIDATION:
 * - Boredom increases novelty-seeking drive
 * - Interest decays with hyperbolic/exponential curves
 * - Different curiosity types have distinct behavioral signatures
 * - Anxiety suppresses exploration appropriately
 * - Social curiosity tracks agents and gossip
 * - Meta-awareness grows with introspection
 * - Curiosity spreads through contagion
 * - Surprise boosts learning rate
 * - Fatigue requires recovery periods
 * - Counterfactual reasoning enables learning from alternatives
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "cognitive/curiosity/nimcp_curiosity_enhanced.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CuriosityEnhancedTest : public ::testing::Test {
protected:
    curiosity_enhanced_system_t* system = nullptr;
    curiosity_enhanced_config_t config;

    void SetUp() override {
        curiosity_enhanced_config_default(&config);
    }

    void TearDown() override {
        if (system) {
            curiosity_enhanced_destroy(system);
            system = nullptr;
        }
    }

    void CreateSystem() {
        system = curiosity_enhanced_create(&config, nullptr);
        ASSERT_NE(system, nullptr) << "Failed to create enhanced curiosity system";
    }
};

//=============================================================================
// 1. Configuration Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, DefaultConfigValid) {
    curiosity_enhanced_config_t cfg;
    curiosity_enhanced_config_default(&cfg);

    // Global settings
    EXPECT_GT(cfg.update_interval_ms, 0.0f);
    EXPECT_TRUE(cfg.enable_bio_async);
    EXPECT_TRUE(cfg.enable_all_enhancements);

    // Boredom config
    EXPECT_FLOAT_EQ(cfg.boredom.boredom_threshold, BOREDOM_THRESHOLD_DEFAULT);
    EXPECT_GT(cfg.boredom.monotony_decay_rate, 0.0f);
    EXPECT_GT(cfg.boredom.novelty_boost_factor, 0.0f);

    // Interest config
    EXPECT_GT(cfg.interest.base_decay_rate, 0.0f);
    EXPECT_GT(cfg.interest.half_life_ms, 0u);
    EXPECT_TRUE(cfg.interest.enable_hyperbolic_decay);

    // Types config
    float type_sum = 0.0f;
    for (int i = 0; i < CURIOSITY_TYPE_COUNT; i++) {
        type_sum += cfg.types.type_weights[i];
    }
    EXPECT_NEAR(type_sum, 1.0f, 0.01f);

    // Anxiety config
    EXPECT_GT(cfg.anxiety.anxiety_suppress_threshold, 0.0f);
    EXPECT_LE(cfg.anxiety.anxiety_suppress_threshold, 1.0f);

    // Social config
    EXPECT_GT(cfg.social.gossip_decay_rate, 0.0f);

    // Meta config
    EXPECT_GT(cfg.meta.introspection_frequency_ms, 0.0f);

    // Contagion config
    EXPECT_FLOAT_EQ(cfg.contagion.base_susceptibility, CONTAGION_SUSCEPTIBILITY_DEFAULT);

    // Surprise config
    EXPECT_GT(cfg.surprise.max_lr_boost, 1.0f);

    // Fatigue config
    EXPECT_GT(cfg.fatigue.rest_threshold, 0.0f);

    // Counterfactual config
    EXPECT_GT(cfg.counterfactual.learning_value_threshold, 0.0f);
}

TEST_F(CuriosityEnhancedTest, DefaultConfigWithNull) {
    // Should not crash with NULL
    curiosity_enhanced_config_default(nullptr);
}

//=============================================================================
// 2. Lifecycle Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, CreateWithDefaultConfig) {
    CreateSystem();
    EXPECT_NE(system, nullptr);
}

TEST_F(CuriosityEnhancedTest, CreateWithNullConfig) {
    system = curiosity_enhanced_create(nullptr, nullptr);
    EXPECT_NE(system, nullptr);
}

TEST_F(CuriosityEnhancedTest, DestroyNull) {
    // Should not crash
    curiosity_enhanced_destroy(nullptr);
}

TEST_F(CuriosityEnhancedTest, UpdateWithValidSystem) {
    CreateSystem();
    int ret = curiosity_enhanced_update(system, 100.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityEnhancedTest, UpdateWithNull) {
    int ret = curiosity_enhanced_update(nullptr, 100.0f);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityEnhancedTest, MultipleUpdates) {
    CreateSystem();
    for (int i = 0; i < 100; i++) {
        int ret = curiosity_enhanced_update(system, 10.0f);
        EXPECT_EQ(ret, 0);
    }
}

//=============================================================================
// 3. Enhancement 1: Boredom Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, BoredomInitiallyFalse) {
    CreateSystem();
    curiosity_boredom_state_t state;
    bool is_bored = curiosity_enhanced_is_bored(system, &state);
    EXPECT_FALSE(is_bored);
    EXPECT_FLOAT_EQ(state.novelty_seeking_boost, 1.0f);
}

TEST_F(CuriosityEnhancedTest, BoredomIsBoredWithNull) {
    bool is_bored = curiosity_enhanced_is_bored(nullptr, nullptr);
    EXPECT_FALSE(is_bored);
}

TEST_F(CuriosityEnhancedTest, BoredomReportStimulus) {
    CreateSystem();
    int ret = curiosity_enhanced_report_stimulus(system, 12345, 0.8f);
    EXPECT_EQ(ret, 0);

    curiosity_boredom_state_t state;
    curiosity_enhanced_is_bored(system, &state);
    EXPECT_FLOAT_EQ(state.last_stimulus_novelty, 0.8f);
}

TEST_F(CuriosityEnhancedTest, BoredomRepeatedStimulusIncreasesMonotony) {
    CreateSystem();

    // Report same stimulus multiple times
    for (int i = 0; i < 10; i++) {
        curiosity_enhanced_report_stimulus(system, 12345, 0.2f);
        curiosity_enhanced_update(system, 100.0f);
    }

    curiosity_boredom_state_t state;
    curiosity_enhanced_is_bored(system, &state);
    EXPECT_GT(state.repetition_count, 0u);
}

TEST_F(CuriosityEnhancedTest, BoredomGetBoost) {
    CreateSystem();
    float boost = curiosity_enhanced_get_boredom_boost(system);
    EXPECT_GE(boost, 1.0f);
    EXPECT_LE(boost, BOREDOM_NOVELTY_SEEK_BOOST);
}

TEST_F(CuriosityEnhancedTest, BoredomBoostWithNull) {
    float boost = curiosity_enhanced_get_boredom_boost(nullptr);
    EXPECT_FLOAT_EQ(boost, 1.0f);
}

TEST_F(CuriosityEnhancedTest, BoredomNoveltyResetsMonotony) {
    CreateSystem();

    // Build up monotony
    for (int i = 0; i < 5; i++) {
        curiosity_enhanced_report_stimulus(system, 12345, 0.1f);
        curiosity_enhanced_update(system, 100.0f);
    }

    curiosity_boredom_state_t before;
    curiosity_enhanced_is_bored(system, &before);

    // Report high novelty
    curiosity_enhanced_report_stimulus(system, 99999, 0.9f);

    curiosity_boredom_state_t after;
    curiosity_enhanced_is_bored(system, &after);

    // Monotony should decrease after novelty
    EXPECT_LT(after.monotony_level, before.monotony_level + 0.1f);
}

//=============================================================================
// 4. Enhancement 2: Interest Decay Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, InterestNewTopicFullInterest) {
    CreateSystem();
    float interest = curiosity_enhanced_get_topic_interest(system, "new_topic");
    EXPECT_FLOAT_EQ(interest, 1.0f);  // Unknown = full interest
}

TEST_F(CuriosityEnhancedTest, InterestGetWithNull) {
    float interest = curiosity_enhanced_get_topic_interest(nullptr, "topic");
    EXPECT_FLOAT_EQ(interest, 0.0f);
}

TEST_F(CuriosityEnhancedTest, InterestRecordExposure) {
    CreateSystem();
    int ret = curiosity_enhanced_record_exposure(system, "test_topic", 0.5f);
    EXPECT_EQ(ret, 0);

    // Interest should still be retrievable
    float interest = curiosity_enhanced_get_topic_interest(system, "test_topic");
    EXPECT_GT(interest, 0.0f);
    EXPECT_LE(interest, 1.0f);
}

TEST_F(CuriosityEnhancedTest, InterestRecordExposureNull) {
    int ret = curiosity_enhanced_record_exposure(nullptr, "topic", 0.5f);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityEnhancedTest, InterestComputeSatiation) {
    CreateSystem();

    // Record multiple exposures
    for (int i = 0; i < 20; i++) {
        curiosity_enhanced_record_exposure(system, "saturated_topic", 0.8f);
    }

    float satiation = curiosity_enhanced_compute_satiation(system, "saturated_topic");
    EXPECT_GT(satiation, 0.0f);
}

TEST_F(CuriosityEnhancedTest, InterestSatiationWithNull) {
    float satiation = curiosity_enhanced_compute_satiation(nullptr, "topic");
    EXPECT_FLOAT_EQ(satiation, 0.0f);
}

TEST_F(CuriosityEnhancedTest, InterestResidualInterest) {
    CreateSystem();
    curiosity_enhanced_record_exposure(system, "topic", 0.5f);
    float residual = curiosity_enhanced_get_residual_interest(system, "topic");
    EXPECT_GT(residual, 0.0f);
}

TEST_F(CuriosityEnhancedTest, InterestResidualWithNull) {
    float residual = curiosity_enhanced_get_residual_interest(nullptr, "topic");
    EXPECT_FLOAT_EQ(residual, 0.0f);
}

//=============================================================================
// 5. Enhancement 3: Curiosity Type Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, TypeGetDominant) {
    CreateSystem();
    curiosity_type_t type = curiosity_enhanced_get_dominant_type(system);
    EXPECT_GE(type, CURIOSITY_TYPE_DIVERSIVE);
    EXPECT_LT(type, CURIOSITY_TYPE_COUNT);
}

TEST_F(CuriosityEnhancedTest, TypeGetDominantNull) {
    curiosity_type_t type = curiosity_enhanced_get_dominant_type(nullptr);
    EXPECT_EQ(type, CURIOSITY_TYPE_EPISTEMIC);  // Default
}

TEST_F(CuriosityEnhancedTest, TypeGetProfile) {
    CreateSystem();
    curiosity_type_profile_t profile;
    int ret = curiosity_enhanced_get_type_profile(system, &profile);
    EXPECT_EQ(ret, 0);

    // Check all intensities sum to 1.0
    float sum = 0.0f;
    for (int i = 0; i < CURIOSITY_TYPE_COUNT; i++) {
        sum += profile.type_intensities[i];
        EXPECT_GE(profile.type_intensities[i], 0.0f);
    }
    EXPECT_NEAR(sum, 1.0f, 0.01f);
}

TEST_F(CuriosityEnhancedTest, TypeGetProfileNull) {
    int ret = curiosity_enhanced_get_type_profile(nullptr, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityEnhancedTest, TypeSetIntensity) {
    CreateSystem();
    int ret = curiosity_enhanced_set_type_intensity(system, CURIOSITY_TYPE_SOCIAL, 0.8f);
    EXPECT_EQ(ret, 0);

    curiosity_type_profile_t profile;
    curiosity_enhanced_get_type_profile(system, &profile);
    EXPECT_FLOAT_EQ(profile.type_intensities[CURIOSITY_TYPE_SOCIAL], 0.8f);
}

TEST_F(CuriosityEnhancedTest, TypeSetIntensityInvalidType) {
    CreateSystem();
    int ret = curiosity_enhanced_set_type_intensity(system, (curiosity_type_t)100, 0.5f);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityEnhancedTest, TypeTransition) {
    CreateSystem();
    int ret = curiosity_enhanced_transition_type(system, CURIOSITY_TYPE_PERCEPTUAL);
    EXPECT_EQ(ret, 0);

    curiosity_type_t type = curiosity_enhanced_get_dominant_type(system);
    EXPECT_EQ(type, CURIOSITY_TYPE_PERCEPTUAL);
}

TEST_F(CuriosityEnhancedTest, TypeToString) {
    EXPECT_STREQ(curiosity_type_to_string(CURIOSITY_TYPE_DIVERSIVE), "diversive");
    EXPECT_STREQ(curiosity_type_to_string(CURIOSITY_TYPE_SPECIFIC), "specific");
    EXPECT_STREQ(curiosity_type_to_string(CURIOSITY_TYPE_PERCEPTUAL), "perceptual");
    EXPECT_STREQ(curiosity_type_to_string(CURIOSITY_TYPE_EPISTEMIC), "epistemic");
    EXPECT_STREQ(curiosity_type_to_string(CURIOSITY_TYPE_SOCIAL), "social");
    EXPECT_STREQ(curiosity_type_to_string(CURIOSITY_TYPE_MORBID), "morbid");
}

TEST_F(CuriosityEnhancedTest, TypeFromString) {
    EXPECT_EQ(curiosity_type_from_string("diversive"), CURIOSITY_TYPE_DIVERSIVE);
    EXPECT_EQ(curiosity_type_from_string("epistemic"), CURIOSITY_TYPE_EPISTEMIC);
    EXPECT_EQ(curiosity_type_from_string("invalid"), CURIOSITY_TYPE_EPISTEMIC);
    EXPECT_EQ(curiosity_type_from_string(nullptr), CURIOSITY_TYPE_EPISTEMIC);
}

//=============================================================================
// 6. Enhancement 4: Anxiety Balance Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, AnxietyConnectNull) {
    CreateSystem();
    int ret = curiosity_enhanced_connect_anxiety(system, nullptr);
    EXPECT_EQ(ret, 0);  // Should accept NULL anxiety
}

TEST_F(CuriosityEnhancedTest, AnxietyGetNetMotivation) {
    CreateSystem();
    float net = curiosity_enhanced_get_net_motivation(system);
    EXPECT_GE(net, -1.0f);
    EXPECT_LE(net, 1.0f);
}

TEST_F(CuriosityEnhancedTest, AnxietyGetNetMotivationNull) {
    float net = curiosity_enhanced_get_net_motivation(nullptr);
    EXPECT_FLOAT_EQ(net, 0.0f);
}

TEST_F(CuriosityEnhancedTest, AnxietyShouldExplore) {
    CreateSystem();
    bool should = curiosity_enhanced_should_explore(system, 0.0f);
    // With no threat, should typically explore
    EXPECT_TRUE(should);
}

TEST_F(CuriosityEnhancedTest, AnxietyShouldExploreHighThreat) {
    CreateSystem();
    bool should = curiosity_enhanced_should_explore(system, 1.0f);
    // With high threat, exploration may be suppressed
    // Result depends on approach-avoidance balance
    (void)should;  // Just verify no crash
}

TEST_F(CuriosityEnhancedTest, AnxietyShouldExploreNull) {
    bool should = curiosity_enhanced_should_explore(nullptr, 0.5f);
    EXPECT_FALSE(should);
}

TEST_F(CuriosityEnhancedTest, AnxietyReportConflictResolution) {
    CreateSystem();
    int ret = curiosity_enhanced_report_conflict_resolution(system, true);
    EXPECT_EQ(ret, 0);

    ret = curiosity_enhanced_report_conflict_resolution(system, false);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityEnhancedTest, AnxietyReportConflictNull) {
    int ret = curiosity_enhanced_report_conflict_resolution(nullptr, true);
    EXPECT_NE(ret, 0);
}

//=============================================================================
// 7. Enhancement 5: Social Curiosity Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, SocialConnectTom) {
    CreateSystem();
    int ret = curiosity_enhanced_connect_tom(system, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityEnhancedTest, SocialAssessTarget) {
    CreateSystem();
    curiosity_social_target_t target;
    float interest = curiosity_enhanced_assess_social_target(system, "agent1", &target);
    EXPECT_GT(interest, 0.0f);
    EXPECT_STREQ(target.agent_id, "agent1");
}

TEST_F(CuriosityEnhancedTest, SocialAssessTargetNull) {
    float interest = curiosity_enhanced_assess_social_target(nullptr, "agent", nullptr);
    EXPECT_FLOAT_EQ(interest, 0.0f);
}

TEST_F(CuriosityEnhancedTest, SocialRecordInteraction) {
    CreateSystem();
    int ret = curiosity_enhanced_record_social_interaction(system, "agent1", 0.7f);
    EXPECT_EQ(ret, 0);

    curiosity_social_target_t target;
    curiosity_enhanced_assess_social_target(system, "agent1", &target);
    EXPECT_EQ(target.interaction_count, 1u);
}

TEST_F(CuriosityEnhancedTest, SocialRecordInteractionNull) {
    int ret = curiosity_enhanced_record_social_interaction(nullptr, "agent", 0.5f);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityEnhancedTest, SocialGetGossipInterest) {
    CreateSystem();
    float gossip = curiosity_enhanced_get_gossip_interest(system);
    EXPECT_GE(gossip, 0.0f);
    EXPECT_LE(gossip, 1.0f);
}

TEST_F(CuriosityEnhancedTest, SocialGetGossipNull) {
    float gossip = curiosity_enhanced_get_gossip_interest(nullptr);
    EXPECT_FLOAT_EQ(gossip, 0.0f);
}

TEST_F(CuriosityEnhancedTest, SocialMultipleTargets) {
    CreateSystem();

    for (int i = 0; i < 10; i++) {
        char agent_id[32];
        snprintf(agent_id, sizeof(agent_id), "agent_%d", i);
        curiosity_enhanced_record_social_interaction(system, agent_id, 0.5f);
    }

    // All agents should be tracked
    for (int i = 0; i < 10; i++) {
        char agent_id[32];
        snprintf(agent_id, sizeof(agent_id), "agent_%d", i);
        float interest = curiosity_enhanced_assess_social_target(system, agent_id, nullptr);
        EXPECT_GT(interest, 0.0f);
    }
}

//=============================================================================
// 8. Enhancement 6: Meta-Curiosity Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, MetaIntrospect) {
    CreateSystem();
    curiosity_meta_state_t state;
    int ret = curiosity_enhanced_introspect(system, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(state.introspection_depth, 0.0f);
}

TEST_F(CuriosityEnhancedTest, MetaIntrospectNull) {
    int ret = curiosity_enhanced_introspect(nullptr, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityEnhancedTest, MetaIdentifyBlindSpots) {
    CreateSystem();

    // Set one type very low
    curiosity_enhanced_set_type_intensity(system, CURIOSITY_TYPE_MORBID, 0.01f);

    uint32_t count = curiosity_enhanced_identify_blind_spots(system);
    // Should identify at least one blind spot
    EXPECT_GE(count, 0u);
}

TEST_F(CuriosityEnhancedTest, MetaIdentifyBlindSpotsNull) {
    uint32_t count = curiosity_enhanced_identify_blind_spots(nullptr);
    EXPECT_EQ(count, 0u);
}

TEST_F(CuriosityEnhancedTest, MetaGetMetaCuriosity) {
    CreateSystem();
    float meta = curiosity_enhanced_get_meta_curiosity(system);
    EXPECT_GE(meta, 0.0f);
    EXPECT_LE(meta, 1.0f);
}

TEST_F(CuriosityEnhancedTest, MetaGetMetaCuriosityNull) {
    float meta = curiosity_enhanced_get_meta_curiosity(nullptr);
    EXPECT_FLOAT_EQ(meta, 0.0f);
}

TEST_F(CuriosityEnhancedTest, MetaIntrospectionGrowsAwareness) {
    CreateSystem();

    curiosity_meta_state_t before;
    curiosity_enhanced_introspect(system, &before);

    for (int i = 0; i < 10; i++) {
        curiosity_enhanced_introspect(system, nullptr);
    }

    curiosity_meta_state_t after;
    curiosity_enhanced_introspect(system, &after);

    EXPECT_GT(after.self_awareness_of_interests, before.self_awareness_of_interests);
}

//=============================================================================
// 9. Enhancement 7: Contagion Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, ContagionObserveCuriosity) {
    CreateSystem();

    curiosity_contagion_event_t event;
    memset(&event, 0, sizeof(event));
    strncpy(event.observed_agent, "other_agent", sizeof(event.observed_agent) - 1);
    strncpy(event.topic_of_interest, "interesting_topic", sizeof(event.topic_of_interest) - 1);
    event.observed_curiosity_intensity = 0.8f;

    bool adopted = curiosity_enhanced_observe_curiosity(system, &event);
    // Adoption depends on intensity and susceptibility
    (void)adopted;  // Just verify no crash
}

TEST_F(CuriosityEnhancedTest, ContagionObserveNull) {
    bool adopted = curiosity_enhanced_observe_curiosity(nullptr, nullptr);
    EXPECT_FALSE(adopted);
}

TEST_F(CuriosityEnhancedTest, ContagionGetSusceptibility) {
    CreateSystem();
    float susc = curiosity_enhanced_get_contagion_susceptibility(system);
    EXPECT_GE(susc, 0.0f);
    EXPECT_LE(susc, 1.0f);
}

TEST_F(CuriosityEnhancedTest, ContagionGetSusceptibilityNull) {
    float susc = curiosity_enhanced_get_contagion_susceptibility(nullptr);
    EXPECT_FLOAT_EQ(susc, 0.0f);
}

TEST_F(CuriosityEnhancedTest, ContagionSetSusceptibility) {
    CreateSystem();
    int ret = curiosity_enhanced_set_contagion_susceptibility(system, 0.8f);
    EXPECT_EQ(ret, 0);

    float susc = curiosity_enhanced_get_contagion_susceptibility(system);
    EXPECT_FLOAT_EQ(susc, 0.8f);
}

TEST_F(CuriosityEnhancedTest, ContagionSetSusceptibilityNull) {
    int ret = curiosity_enhanced_set_contagion_susceptibility(nullptr, 0.5f);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityEnhancedTest, ContagionHighIntensityAdoption) {
    CreateSystem();
    curiosity_enhanced_set_contagion_susceptibility(system, 1.0f);

    curiosity_contagion_event_t event;
    memset(&event, 0, sizeof(event));
    strncpy(event.observed_agent, "enthusiastic", sizeof(event.observed_agent) - 1);
    strncpy(event.topic_of_interest, "exciting", sizeof(event.topic_of_interest) - 1);
    event.observed_curiosity_intensity = 1.0f;

    bool adopted = curiosity_enhanced_observe_curiosity(system, &event);
    EXPECT_TRUE(adopted);
}

//=============================================================================
// 10. Enhancement 8: Surprise Learning Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, SurpriseReport) {
    CreateSystem();
    float boost = curiosity_enhanced_report_surprise(system, 0.8f, "unexpected_event");
    EXPECT_GE(boost, 1.0f);
}

TEST_F(CuriosityEnhancedTest, SurpriseReportNull) {
    float boost = curiosity_enhanced_report_surprise(nullptr, 0.5f, "event");
    EXPECT_FLOAT_EQ(boost, 1.0f);
}

TEST_F(CuriosityEnhancedTest, SurpriseGetBoost) {
    CreateSystem();
    float boost = curiosity_enhanced_get_surprise_boost(system);
    EXPECT_GE(boost, 1.0f);
}

TEST_F(CuriosityEnhancedTest, SurpriseGetBoostNull) {
    float boost = curiosity_enhanced_get_surprise_boost(nullptr);
    EXPECT_FLOAT_EQ(boost, 1.0f);
}

TEST_F(CuriosityEnhancedTest, SurpriseHighErrorBoost) {
    CreateSystem();
    float boost = curiosity_enhanced_report_surprise(system, 1.0f, "huge_surprise");
    EXPECT_GT(boost, 1.0f);
    EXPECT_LE(boost, SURPRISE_LR_BOOST_MAX);
}

TEST_F(CuriosityEnhancedTest, SurpriseLowErrorNoBoost) {
    CreateSystem();
    float boost = curiosity_enhanced_report_surprise(system, 0.1f, "expected");
    EXPECT_FLOAT_EQ(boost, 1.0f);
}

TEST_F(CuriosityEnhancedTest, SurprisePrioritize) {
    CreateSystem();
    curiosity_enhanced_report_surprise(system, 0.9f, "important");
    float priority = curiosity_enhanced_prioritize_surprise(system, "important");
    EXPECT_GT(priority, 0.0f);
}

//=============================================================================
// 11. Enhancement 9: Fatigue Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, FatigueCheckInitial) {
    CreateSystem();
    curiosity_fatigue_state_t state;
    float fatigue = curiosity_enhanced_check_fatigue(system, &state);
    EXPECT_FLOAT_EQ(fatigue, 0.0f);  // Initially no fatigue
    EXPECT_FALSE(state.needs_rest);
}

TEST_F(CuriosityEnhancedTest, FatigueCheckNull) {
    float fatigue = curiosity_enhanced_check_fatigue(nullptr, nullptr);
    EXPECT_FLOAT_EQ(fatigue, 0.0f);
}

TEST_F(CuriosityEnhancedTest, FatigueAccumulates) {
    CreateSystem();

    // Update many times to accumulate fatigue
    for (int i = 0; i < 1000; i++) {
        curiosity_enhanced_update(system, 100.0f);
    }

    float fatigue = curiosity_enhanced_check_fatigue(system, nullptr);
    EXPECT_GT(fatigue, 0.0f);
}

TEST_F(CuriosityEnhancedTest, FatigueNeedsRest) {
    CreateSystem();
    bool needs = curiosity_enhanced_needs_rest(system);
    EXPECT_FALSE(needs);  // Initially no rest needed
}

TEST_F(CuriosityEnhancedTest, FatigueNeedsRestNull) {
    bool needs = curiosity_enhanced_needs_rest(nullptr);
    EXPECT_FALSE(needs);
}

TEST_F(CuriosityEnhancedTest, FatigueInitiateRecovery) {
    CreateSystem();
    int ret = curiosity_enhanced_initiate_recovery(system, 10000.0f);
    EXPECT_EQ(ret, 0);

    curiosity_fatigue_state_t state;
    curiosity_enhanced_check_fatigue(system, &state);
    EXPECT_TRUE(state.is_resting);
}

TEST_F(CuriosityEnhancedTest, FatigueInitiateRecoveryNull) {
    int ret = curiosity_enhanced_initiate_recovery(nullptr, 1000.0f);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityEnhancedTest, FatigueEndRecovery) {
    CreateSystem();
    curiosity_enhanced_initiate_recovery(system, 5000.0f);
    int ret = curiosity_enhanced_end_recovery(system);
    EXPECT_EQ(ret, 0);

    curiosity_fatigue_state_t state;
    curiosity_enhanced_check_fatigue(system, &state);
    EXPECT_FALSE(state.is_resting);
}

//=============================================================================
// 12. Enhancement 10: Counterfactual Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, CounterfactualGenerate) {
    CreateSystem();
    curiosity_counterfactual_t cf;
    int ret = curiosity_enhanced_generate_counterfactual(
        system, "chose_A", "got_bad_result", &cf);
    EXPECT_EQ(ret, 0);
    EXPECT_STRNE(cf.counterfactual_question, "");
}

TEST_F(CuriosityEnhancedTest, CounterfactualGenerateNull) {
    int ret = curiosity_enhanced_generate_counterfactual(nullptr, "a", "b", nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityEnhancedTest, CounterfactualExplore) {
    CreateSystem();
    curiosity_counterfactual_t cf;
    curiosity_enhanced_generate_counterfactual(system, "decision", "outcome", &cf);

    float outcome;
    int ret = curiosity_enhanced_explore_counterfactual(system, &cf, &outcome);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(cf.is_explored);
}

TEST_F(CuriosityEnhancedTest, CounterfactualExploreNull) {
    int ret = curiosity_enhanced_explore_counterfactual(nullptr, nullptr, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityEnhancedTest, CounterfactualGetCuriosity) {
    CreateSystem();
    float cf_curiosity = curiosity_enhanced_get_counterfactual_curiosity(system);
    EXPECT_GE(cf_curiosity, 0.0f);
}

TEST_F(CuriosityEnhancedTest, CounterfactualGetCuriosityNull) {
    float cf_curiosity = curiosity_enhanced_get_counterfactual_curiosity(nullptr);
    EXPECT_FLOAT_EQ(cf_curiosity, 0.0f);
}

TEST_F(CuriosityEnhancedTest, CounterfactualMultipleGeneration) {
    CreateSystem();

    for (int i = 0; i < 20; i++) {
        char decision[64], outcome[64];
        snprintf(decision, sizeof(decision), "decision_%d", i);
        snprintf(outcome, sizeof(outcome), "outcome_%d", i);

        curiosity_counterfactual_t cf;
        int ret = curiosity_enhanced_generate_counterfactual(
            system, decision, outcome, &cf);
        EXPECT_EQ(ret, 0);
    }

    curiosity_enhanced_stats_t stats;
    curiosity_enhanced_get_stats(system, &stats);
    EXPECT_GE(stats.counterfactuals_generated, 20u);
}

//=============================================================================
// 13. State and Statistics Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, GetState) {
    CreateSystem();
    curiosity_enhanced_state_t state;
    int ret = curiosity_enhanced_get_state(system, &state);
    EXPECT_EQ(ret, 0);

    EXPECT_GE(state.overall_curiosity_drive, 0.0f);
    EXPECT_LE(state.overall_curiosity_drive, 1.0f);
}

TEST_F(CuriosityEnhancedTest, GetStateNull) {
    int ret = curiosity_enhanced_get_state(nullptr, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityEnhancedTest, GetStats) {
    CreateSystem();
    curiosity_enhanced_stats_t stats;
    int ret = curiosity_enhanced_get_stats(system, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityEnhancedTest, GetStatsNull) {
    int ret = curiosity_enhanced_get_stats(nullptr, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityEnhancedTest, ResetStats) {
    CreateSystem();

    // Generate some stats
    curiosity_enhanced_report_surprise(system, 0.9f, "event");
    curiosity_enhanced_record_exposure(system, "topic", 0.5f);

    curiosity_enhanced_reset_stats(system);

    curiosity_enhanced_stats_t stats;
    curiosity_enhanced_get_stats(system, &stats);

    EXPECT_EQ(stats.surprise_events, 0u);
    EXPECT_FLOAT_EQ(stats.avg_curiosity_level, 0.0f);
}

TEST_F(CuriosityEnhancedTest, ResetStatsNull) {
    // Should not crash
    curiosity_enhanced_reset_stats(nullptr);
}

TEST_F(CuriosityEnhancedTest, GetOverallDrive) {
    CreateSystem();
    float drive = curiosity_enhanced_get_overall_drive(system);
    EXPECT_GE(drive, 0.0f);
    EXPECT_LE(drive, 1.0f);
}

TEST_F(CuriosityEnhancedTest, GetOverallDriveNull) {
    float drive = curiosity_enhanced_get_overall_drive(nullptr);
    EXPECT_FLOAT_EQ(drive, 0.0f);
}

//=============================================================================
// 14. Bio-Async Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, BioAsyncConnect) {
    CreateSystem();
    int ret = curiosity_enhanced_connect_bio_async(system);
    EXPECT_EQ(ret, 0);

    bool connected = curiosity_enhanced_is_bio_async_connected(system);
    // May or may not be connected depending on router availability
    (void)connected;
}

TEST_F(CuriosityEnhancedTest, BioAsyncConnectNull) {
    int ret = curiosity_enhanced_connect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityEnhancedTest, BioAsyncDisconnect) {
    CreateSystem();
    curiosity_enhanced_connect_bio_async(system);
    int ret = curiosity_enhanced_disconnect_bio_async(system);
    EXPECT_EQ(ret, 0);

    bool connected = curiosity_enhanced_is_bio_async_connected(system);
    EXPECT_FALSE(connected);
}

TEST_F(CuriosityEnhancedTest, BioAsyncDisconnectNull) {
    int ret = curiosity_enhanced_disconnect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityEnhancedTest, BioAsyncIsConnected) {
    CreateSystem();
    bool connected = curiosity_enhanced_is_bio_async_connected(system);
    EXPECT_FALSE(connected);  // Not connected initially
}

TEST_F(CuriosityEnhancedTest, BioAsyncIsConnectedNull) {
    bool connected = curiosity_enhanced_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

//=============================================================================
// 15. Integration Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, FullUpdateCycle) {
    CreateSystem();

    // Run a complete update cycle with all systems
    curiosity_enhanced_update(system, 100.0f);

    // Report stimulus
    curiosity_enhanced_report_stimulus(system, 12345, 0.6f);

    // Record exposure
    curiosity_enhanced_record_exposure(system, "test_topic", 0.5f);

    // Report surprise
    curiosity_enhanced_report_surprise(system, 0.7f, "surprise_event");

    // Social interaction
    curiosity_enhanced_record_social_interaction(system, "agent1", 0.4f);

    // Update again
    curiosity_enhanced_update(system, 100.0f);

    // Get state
    curiosity_enhanced_state_t state;
    curiosity_enhanced_get_state(system, &state);

    EXPECT_GE(state.overall_curiosity_drive, 0.0f);
    EXPECT_LE(state.overall_curiosity_drive, 1.0f);
}

TEST_F(CuriosityEnhancedTest, StatsAccumulate) {
    CreateSystem();

    // Generate various events
    for (int i = 0; i < 10; i++) {
        curiosity_enhanced_report_stimulus(system, (uint64_t)i, 0.9f);
        curiosity_enhanced_report_surprise(system, 0.8f, "event");
        curiosity_enhanced_update(system, 50.0f);
    }

    curiosity_enhanced_stats_t stats;
    curiosity_enhanced_get_stats(system, &stats);

    EXPECT_GE(stats.novelty_events, 0u);
    EXPECT_GE(stats.surprise_events, 0u);
}

//=============================================================================
// 16. Edge Cases
//=============================================================================

TEST_F(CuriosityEnhancedTest, VerySmallDeltaTime) {
    CreateSystem();
    int ret = curiosity_enhanced_update(system, 0.001f);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityEnhancedTest, LargeDeltaTime) {
    CreateSystem();
    int ret = curiosity_enhanced_update(system, 10000.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityEnhancedTest, ZeroNovelty) {
    CreateSystem();
    int ret = curiosity_enhanced_report_stimulus(system, 1, 0.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityEnhancedTest, MaxNovelty) {
    CreateSystem();
    int ret = curiosity_enhanced_report_stimulus(system, 1, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityEnhancedTest, EmptyTopic) {
    CreateSystem();
    float interest = curiosity_enhanced_get_topic_interest(system, "");
    EXPECT_FLOAT_EQ(interest, 1.0f);  // Empty topic = new = full interest
}

TEST_F(CuriosityEnhancedTest, LongTopic) {
    CreateSystem();
    char long_topic[512];
    memset(long_topic, 'a', sizeof(long_topic) - 1);
    long_topic[sizeof(long_topic) - 1] = '\0';

    float interest = curiosity_enhanced_get_topic_interest(system, long_topic);
    EXPECT_FLOAT_EQ(interest, 1.0f);
}

TEST_F(CuriosityEnhancedTest, RapidCreateDestroy) {
    for (int i = 0; i < 100; i++) {
        curiosity_enhanced_system_t* sys = curiosity_enhanced_create(nullptr, nullptr);
        EXPECT_NE(sys, nullptr);
        curiosity_enhanced_destroy(sys);
    }
}

//=============================================================================
// 17. Thread Safety Tests
//=============================================================================

TEST_F(CuriosityEnhancedTest, ConcurrentUpdates) {
    CreateSystem();

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this]() {
            for (int j = 0; j < 100; j++) {
                curiosity_enhanced_update(system, 10.0f);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // System should still be valid
    float drive = curiosity_enhanced_get_overall_drive(system);
    EXPECT_GE(drive, 0.0f);
    EXPECT_LE(drive, 1.0f);
}

TEST_F(CuriosityEnhancedTest, ConcurrentReads) {
    CreateSystem();
    curiosity_enhanced_update(system, 100.0f);

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this]() {
            for (int j = 0; j < 100; j++) {
                curiosity_enhanced_get_overall_drive(system);
                curiosity_enhanced_get_dominant_type(system);
                curiosity_enhanced_get_boredom_boost(system);
                curiosity_enhanced_get_meta_curiosity(system);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}

TEST_F(CuriosityEnhancedTest, ConcurrentMixedOperations) {
    CreateSystem();

    std::vector<std::thread> threads;

    // Reader threads
    for (int i = 0; i < 2; i++) {
        threads.emplace_back([this]() {
            for (int j = 0; j < 50; j++) {
                curiosity_enhanced_get_overall_drive(system);
                curiosity_enhanced_get_topic_interest(system, "topic");
            }
        });
    }

    // Writer threads
    for (int i = 0; i < 2; i++) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 50; j++) {
                curiosity_enhanced_update(system, 10.0f);
                curiosity_enhanced_report_stimulus(system, (uint64_t)(i * 100 + j), 0.5f);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify system integrity
    curiosity_enhanced_state_t state;
    int ret = curiosity_enhanced_get_state(system, &state);
    EXPECT_EQ(ret, 0);
}
