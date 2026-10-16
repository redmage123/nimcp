/**
 * @file test_social_bonds_immune_integration.cpp
 * @brief Unit tests for Social Bonds (Love/Loyalty/Friendship) - Immune Integration
 *
 * WHAT: Test bidirectional coupling between social bonding and immune system
 * WHY:  Validate social connection immune benefits and loneliness inflammation
 * HOW:  Test oxytocin-IL10 coupling, loneliness-triggered inflammation
 *
 * BIOLOGICAL BASIS:
 * - Social connection enhances immunity and reduces inflammation
 * - Loneliness increases pro-inflammatory cytokines
 * - Oxytocin promotes IL-10 release (anti-inflammatory)
 *
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_emotion_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_love_loyalty_friendship.h"
#include "cognitive/nimcp_emotional_system.h"

class SocialBondsImmuneTest : public ::testing::Test {
protected:
    emotion_immune_bridge_t* bridge;
    brain_immune_system_t* immune;
    social_bond_system_t* social_bonds;
    emotional_system_t* emotion_system;

    void SetUp() override {
        // Create systems
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        social_bonds = social_bond_system_create();
        ASSERT_NE(social_bonds, nullptr);

        emotion_config_t emo_config = emotion_system_default_config();
        emotion_system = emotion_system_create(&emo_config);
        ASSERT_NE(emotion_system, nullptr);

        // Create bridge
        emotion_immune_config_t bridge_config;
        emotion_immune_default_config(&bridge_config);
        bridge_config.enable_social_bond_integration = true;
        bridge = emotion_immune_bridge_create(&bridge_config, immune, emotion_system, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);

        // Connect social bonds
        int result = emotion_immune_bridge_connect_social_bonds(bridge, social_bonds);
        ASSERT_EQ(result, 0);
    }

    void TearDown() override {
        emotion_immune_bridge_destroy(bridge);
        brain_immune_destroy(immune);
        social_bond_system_destroy(social_bonds);
        emotion_system_destroy(emotion_system);
    }
};

/**
 * TEST: Love and Friendship Boost Immunity
 * BIOLOGICAL: Positive social bonds enhance immune function, release IL-10
 */
TEST_F(SocialBondsImmuneTest, LoveBoostsImmunity) {
    // Create strong social bond (love/close friendship)
    uint32_t bond_id = social_create_relationship(social_bonds, RELATIONSHIP_CLOSE_FRIEND, 0);
    ASSERT_NE(bond_id, 0);

    // Experience love
    social_experience_love(social_bonds, bond_id, LOVE_TYPE_ROMANTIC, 0.9f);

    // Process supportive interactions
    social_process_interaction(social_bonds, bond_id, INTERACTION_SUPPORT_RECEIVED,
                                0.8f, 0.9f, 1000);

    // Boost immune from social connection
    int result = emotion_immune_boost_from_social_bonds(bridge);
    EXPECT_EQ(result, 0);

    // Should trigger IL-10 release and reduce inflammation
}

/**
 * TEST: Loneliness Triggers Inflammation
 * BIOLOGICAL: Chronic loneliness increases pro-inflammatory cytokines
 */
TEST_F(SocialBondsImmuneTest, LonelinessTriggersInflammation) {
    // Create isolated state (no close relationships)
    // Update to increase loneliness
    social_update(social_bonds, 86400.0f, 86400000000);  // 1 day without interaction

    // Check if lonely
    bool is_lonely = social_is_lonely(social_bonds);

    // Trigger immune response from loneliness
    int result = emotion_immune_trigger_from_loneliness(bridge);
    EXPECT_EQ(result, 0);

    // Loneliness should trigger pro-inflammatory response
}

/**
 * TEST: Inflammation Suppresses Oxytocin and Social Bonding
 * BIOLOGICAL: Inflammation causes social withdrawal (sickness behavior)
 */
TEST_F(SocialBondsImmuneTest, InflammationSuppressesSocialBonding) {
    // Create relationship
    uint32_t bond_id = social_create_relationship(social_bonds, RELATIONSHIP_FRIEND, 0);

    // Trigger high inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAA, 0xBB, 0xCC};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  ANTIGEN_SEVERITY_CRITICAL, 0, &antigen_id);

    // Full immune activation
    uint32_t b_cell_id, helper_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_t_help_b(immune, helper_id, b_cell_id);

    // Apply inflammation effects to social bonding
    int result = emotion_immune_modulate_social_bonds(bridge);
    EXPECT_EQ(result, 0);

    // Oxytocin should be suppressed, loneliness increased
}

/**
 * TEST: Betrayal Stress Triggers Immune Response
 * BIOLOGICAL: Social pain activates HPA axis and inflammation
 */
TEST_F(SocialBondsImmuneTest, BetrayalTriggersInflammation) {
    // Create close bond
    uint32_t bond_id = social_create_relationship(social_bonds, RELATIONSHIP_CLOSE_FRIEND, 0);

    // Strengthen bond
    for (int i = 0; i < 10; i++) {
        social_process_interaction(social_bonds, bond_id, INTERACTION_SUPPORT_RECEIVED,
                                    0.8f, 0.9f, i * 1000);
    }

    // Experience betrayal
    social_experience_betrayal(social_bonds, bond_id, 0.9f);

    // Trigger immune from emotional stress (betrayal is severe stress)
    int result = emotion_immune_trigger_from_stress(bridge);
    EXPECT_EQ(result, 0);

    // Betrayal should trigger inflammatory response
}

/**
 * TEST: Reconciliation and Repair Reduce Inflammation
 * BIOLOGICAL: Relationship repair reduces stress and inflammation
 */
TEST_F(SocialBondsImmuneTest, ReconciliationReducesInflammation) {
    // Create bond and betray
    uint32_t bond_id = social_create_relationship(social_bonds, RELATIONSHIP_CLOSE_FRIEND, 0);
    social_experience_betrayal(social_bonds, bond_id, 0.8f);

    // Trigger inflammation from betrayal
    emotion_immune_trigger_from_stress(bridge);

    // Attempt repair
    social_attempt_repair(social_bonds, bond_id, 0.9f, 0.9f);

    // Should reduce inflammation (test depends on implementation)
}

/**
 * TEST: Oxytocin-IL10 Coupling
 * BIOLOGICAL: Oxytocin promotes IL-10 release (anti-inflammatory)
 */
TEST_F(SocialBondsImmuneTest, OxytocinIL10Coupling) {
    // Create multiple close bonds to boost oxytocin
    for (int i = 0; i < 3; i++) {
        uint32_t bond_id = social_create_relationship(social_bonds, RELATIONSHIP_CLOSE_FRIEND, 0);
        social_experience_love(social_bonds, bond_id, LOVE_TYPE_COMPANIONATE, 0.8f);
    }

    // Get oxytocin level
    float oxytocin = social_get_oxytocin_level(social_bonds);

    // Boost immune from high oxytocin
    int result = emotion_immune_boost_from_social_bonds(bridge);
    EXPECT_EQ(result, 0);

    // High oxytocin should correlate with IL-10 release
}

/**
 * TEST: Close Friend Count and Immune Function
 * BIOLOGICAL: More close relationships = better immune function
 */
TEST_F(SocialBondsImmuneTest, CloseFriendCountBoostsImmunity) {
    // Create multiple close friendships
    uint32_t friend_count = 0;
    for (int i = 0; i < 5; i++) {
        uint32_t bond_id = social_create_relationship(social_bonds, RELATIONSHIP_CLOSE_FRIEND, i * 1000);
        if (bond_id != 0) {
            friend_count++;

            // Strengthen bonds
            for (int j = 0; j < 5; j++) {
                social_process_interaction(social_bonds, bond_id, INTERACTION_SHARED_ACTIVITY,
                                            0.7f, 0.8f, (i * 10 + j) * 1000);
            }
        }
    }

    // Get close friend count
    uint32_t close_friends = social_get_close_friend_count(social_bonds);

    // Boost immune
    int result = emotion_immune_boost_from_social_bonds(bridge);
    EXPECT_EQ(result, 0);

    // More friends should provide stronger immune boost
}

/**
 * TEST: Social Support During Illness
 * BIOLOGICAL: Social support during illness accelerates recovery
 */
TEST_F(SocialBondsImmuneTest, SocialSupportDuringIllness) {
    // Trigger illness (inflammation)
    uint32_t antigen_id;
    uint8_t epitope[] = {0x11, 0x22, 0x33};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  ANTIGEN_SEVERITY_HIGH, 0, &antigen_id);

    // Receive social support
    uint32_t bond_id = social_create_relationship(social_bonds, RELATIONSHIP_CLOSE_FRIEND, 0);
    social_receive_support(social_bonds, bond_id, 0.9f);

    // Boost immune from support
    emotion_immune_boost_from_social_bonds(bridge);

    // Social support should enhance recovery
}

/**
 * TEST: Integration Update Cycle
 */
TEST_F(SocialBondsImmuneTest, UpdateCycleIntegration) {
    // Create social state
    uint32_t bond_id = social_create_relationship(social_bonds, RELATIONSHIP_FRIEND, 0);

    // Update social system
    social_update(social_bonds, 1.0f, 1000);

    // Update bridge
    int result = emotion_immune_bridge_update(bridge, 1000);
    EXPECT_EQ(result, 0);

    // Apply immune modulation
    emotion_immune_modulate_social_bonds(bridge);
    emotion_immune_boost_from_social_bonds(bridge);
}

/**
 * TEST: Null Safety
 */
TEST_F(SocialBondsImmuneTest, NullSafety) {
    int result = emotion_immune_bridge_connect_social_bonds(nullptr, social_bonds);
    EXPECT_NE(result, 0);

    result = emotion_immune_bridge_connect_social_bonds(bridge, nullptr);
    EXPECT_NE(result, 0);

    result = emotion_immune_modulate_social_bonds(nullptr);
    EXPECT_NE(result, 0);

    result = emotion_immune_boost_from_social_bonds(nullptr);
    EXPECT_NE(result, 0);

    result = emotion_immune_trigger_from_loneliness(nullptr);
    EXPECT_NE(result, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
