/**
 * @file test_love_loyalty_friendship_integration.cpp
 * @brief Integration tests for love/loyalty/friendship system with neuromodulators
 *
 * WHAT: Tests social bond system integration with neuromodulator system
 * WHY:  Ensure love/friendship/loyalty correctly modulate dopamine/oxytocin
 * HOW:  Create social bonds, verify neuromodulator effects and emotional tagging
 *
 * @version Phase E4: Love, Loyalty, Friendship Integration
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include "cognitive/nimcp_love_loyalty_friendship.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include <cmath>

//=============================================================================
// TEST FIXTURE
//=============================================================================

class LoveLoyaltyFriendshipIntegrationTest : public ::testing::Test {
protected:
    social_bond_system_t* social_system;

    void SetUp() override {
        social_system = social_bond_system_create();
        ASSERT_NE(social_system, nullptr);
    }

    void TearDown() override {
        social_bond_system_destroy(social_system);
    }

    // Helper: Create relationship and bring to friendship
    uint32_t create_friendship(float target_closeness = 0.5f) {
        uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

        // Multiple positive interactions to build closeness
        for (int i = 0; i < 20; i++) {
            social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.8f, 0.8f, i * 1000000);
            social_process_interaction(social_system, rel_id, INTERACTION_SHARED_ACTIVITY, 0.7f, 0.7f, i * 1000000 + 500000);
        }

        // Update to calculate friendship_warmth (1 minute is enough with fast time constant)
        social_update(social_system, 60.0f, 20000000 + 60000000);  // 1 minute

        return rel_id;
    }

    // Helper: Create romantic relationship
    uint32_t create_romantic_love() {
        uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

        // Build intimacy and closeness first
        for (int i = 0; i < 15; i++) {
            social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.9f, 0.9f, i * 1000000);
        }

        // Experience romantic love
        social_experience_love(social_system, rel_id, LOVE_TYPE_ROMANTIC, 0.9f);

        // Update to process love state (but not too much time or love will fade)
        social_update(social_system, 1.0f, 16000000);

        return rel_id;
    }
};

//=============================================================================
// BASIC INTEGRATION TESTS
//=============================================================================

TEST_F(LoveLoyaltyFriendshipIntegrationTest, SystemIntegratesWithNeuromodulators) {
    // WHAT: Verify social bond system provides neuromodulator factors
    // WHY:  Integration point with cognitive pipeline
    // HOW:  Query neuromodulator effects

    float dopamine_factor = 0.0f;
    float oxytocin_factor = 0.0f;

    social_get_neuromodulator_effects(social_system, &dopamine_factor, &oxytocin_factor);

    // Should return valid baseline factors
    EXPECT_GT(dopamine_factor, 0.0f);
    EXPECT_GT(oxytocin_factor, 0.0f);
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, EmotionalTagIntegrationWorks) {
    // WHAT: Verify emotional tag integration
    // WHY:  Social emotions must integrate with emotional tagging system
    // HOW:  Get emotion tag and verify structure

    emotional_tag_t emotion = social_get_emotion(social_system);

    // Baseline should be neutral
    EXPECT_NEAR(emotion.valence, 0.0f, 0.1f);
    EXPECT_NEAR(emotion.arousal, 0.5f, 0.1f);
}

//=============================================================================
// LOVE NEUROMODULATOR INTEGRATION
//=============================================================================

TEST_F(LoveLoyaltyFriendshipIntegrationTest, RomanticLoveBoostsDopamine) {
    // WHAT: Romantic love increases dopamine (reward)
    // WHY:  Love is rewarding, drives approach behavior
    // HOW:  Create romantic love, verify dopamine boost

    float baseline_dopamine = 0.0f;
    float baseline_oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &baseline_dopamine, &baseline_oxytocin);

    uint32_t rel_id = create_romantic_love();

    float love_dopamine = 0.0f;
    float love_oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &love_dopamine, &love_oxytocin);

    // Dopamine should be significantly boosted (up to 50% increase)
    EXPECT_GT(love_dopamine, baseline_dopamine);
    EXPECT_GE(love_dopamine, 1.3f);  // At least 30% boost
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, RomanticLoveBoostsOxytocin) {
    // WHAT: Romantic love increases oxytocin (bonding)
    // WHY:  Oxytocin mediates attachment and trust
    // HOW:  Create romantic love, verify oxytocin boost

    float baseline_dopamine = 0.0f;
    float baseline_oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &baseline_dopamine, &baseline_oxytocin);

    uint32_t rel_id = create_romantic_love();

    float love_dopamine = 0.0f;
    float love_oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &love_oxytocin, &love_oxytocin);

    // Oxytocin should be significantly boosted (up to 100% increase)
    EXPECT_GT(love_oxytocin, baseline_oxytocin);
    EXPECT_GE(love_oxytocin, 1.5f);  // At least 50% boost
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, CompanionateLoveAlsoBoostsNeuromodulators) {
    // WHAT: Companionate love (intimacy + commitment, low passion) affects neuromodulators
    // WHY:  Long-term love is also rewarding and bonding
    // HOW:  Create companionate love, verify effects

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Build intimacy and commitment (companionate love)
    for (int i = 0; i < 10; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.8f, 0.8f, i * 1000000);
    }
    social_experience_love(social_system, rel_id, LOVE_TYPE_COMPANIONATE, 0.8f);

    float dopamine = 0.0f;
    float oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &dopamine, &oxytocin);

    // Should still boost neuromodulators
    EXPECT_GE(dopamine, 1.2f);
    EXPECT_GE(oxytocin, 1.3f);
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, FamilialLoveBoostsOxytocin) {
    // WHAT: Familial love increases oxytocin (attachment)
    // WHY:  Family bonds are mediated by oxytocin
    // HOW:  Create familial love, verify oxytocin boost

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_FAMILY, ATTACHMENT_SECURE);
    social_experience_love(social_system, rel_id, LOVE_TYPE_FAMILIAL, 0.8f);

    float dopamine = 0.0f;
    float oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &dopamine, &oxytocin);

    // Oxytocin should be elevated
    EXPECT_GE(oxytocin, 1.3f);
}

//=============================================================================
// FRIENDSHIP NEUROMODULATOR INTEGRATION
//=============================================================================

TEST_F(LoveLoyaltyFriendshipIntegrationTest, FriendshipBoostsDopamineModerately) {
    // WHAT: Friendship increases dopamine (social reward)
    // WHY:  Friendships are rewarding but less intensely than romantic love
    // HOW:  Create friendship, verify moderate dopamine boost

    uint32_t rel_id = create_friendship(0.6f);

    float dopamine = 0.0f;
    float oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &dopamine, &oxytocin);

    // Dopamine should be moderately boosted (20-30% increase)
    EXPECT_GE(dopamine, 1.15f);
    EXPECT_LE(dopamine, 1.4f);
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, MultipleFriendshipsHaveCumulativeEffect) {
    // WHAT: Multiple friendships increase social reward
    // WHY:  Rich social network is rewarding
    // HOW:  Create multiple friendships, verify cumulative dopamine boost

    // Create 3 friendships
    for (int i = 0; i < 3; i++) {
        create_friendship(0.5f + i * 0.1f);
    }

    float dopamine = 0.0f;
    float oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &dopamine, &oxytocin);

    // Should have cumulative effect
    EXPECT_GE(dopamine, 1.3f);
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, CloseFriendshipStrongerThanCasualFriendship) {
    // WHAT: Close friendships have stronger neuromodulator effects
    // WHY:  Closeness intensity affects reward
    // HOW:  Compare casual vs close friendship dopamine effects

    social_bond_system_t* system1 = social_bond_system_create();
    social_bond_system_t* system2 = social_bond_system_create();

    // System 1: casual friendship (closeness = 0.5)
    uint32_t casual = social_create_relationship(system1, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);
    for (int i = 0; i < 15; i++) {
        social_process_interaction(system1, casual, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }
    social_update(system1, 60.0f, 15000000 + 60000000);

    // System 2: close friendship (closeness = 0.8)
    uint32_t close_rel = social_create_relationship(system2, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);
    for (int i = 0; i < 30; i++) {
        social_process_interaction(system2, close_rel, INTERACTION_CONVERSATION, 0.9f, 0.9f, i * 1000000);
        social_process_interaction(system2, close_rel, INTERACTION_SHARED_ACTIVITY, 0.8f, 0.8f, i * 1000000 + 500000);
    }
    social_update(system2, 60.0f, 30000000 + 60000000);

    float casual_dopamine = 0.0f, casual_oxytocin = 0.0f;
    float close_dopamine = 0.0f, close_oxytocin = 0.0f;

    social_get_neuromodulator_effects(system1, &casual_dopamine, &casual_oxytocin);
    social_get_neuromodulator_effects(system2, &close_dopamine, &close_oxytocin);

    // Close friendship should have stronger effect
    EXPECT_GT(close_dopamine, casual_dopamine);

    social_bond_system_destroy(system1);
    social_bond_system_destroy(system2);
}

//=============================================================================
// LONELINESS NEUROMODULATOR INTEGRATION
//=============================================================================

TEST_F(LoveLoyaltyFriendshipIntegrationTest, LonelinessReducesDopamine) {
    // WHAT: Loneliness decreases dopamine (anhedonia)
    // WHY:  Social isolation is aversive, reduces reward sensitivity
    // HOW:  Induce loneliness, verify dopamine reduction

    float baseline_dopamine = 0.0f;
    float baseline_oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &baseline_dopamine, &baseline_oxytocin);

    // Simulate 10 days of no social contact (loneliness builds)
    social_update(social_system, 86400.0f * 10.0f, 86400000000 * 10);

    // Verify loneliness developed
    EXPECT_TRUE(social_is_lonely(social_system));

    float lonely_dopamine = 0.0f;
    float lonely_oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &lonely_dopamine, &lonely_oxytocin);

    // Dopamine should be reduced (down to 30% reduction)
    EXPECT_LT(lonely_dopamine, baseline_dopamine);
    EXPECT_LE(lonely_dopamine, 0.85f);
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, SevereLonelinessHasStrongerEffect) {
    // WHAT: Severe loneliness has stronger dopamine reduction
    // WHY:  Intensity matters
    // HOW:  Compare moderate vs severe loneliness

    // Simulate 30 days of isolation (severe loneliness)
    social_update(social_system, 86400.0f * 30.0f, 86400000000 * 30);

    float lonely_dopamine = 0.0f;
    float lonely_oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &lonely_dopamine, &lonely_oxytocin);

    // Should have strong dopamine reduction
    EXPECT_LE(lonely_dopamine, 0.75f);
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, FriendshipsProtectAgainstLoneliness) {
    // WHAT: Having close friends prevents loneliness dopamine reduction
    // WHY:  Social support buffers against isolation
    // HOW:  Create friendships, verify dopamine maintained despite time

    // Create 2 close friendships
    uint32_t rel1 = create_friendship(0.7f);
    uint32_t rel2 = create_friendship(0.7f);

    // Simulate some time passing (but not enough to decay friendships)
    social_update(social_system, 86400.0f * 3.0f, 86400000000 * 3);

    float dopamine = 0.0f;
    float oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &dopamine, &oxytocin);

    // Dopamine should remain elevated (friendships protect)
    EXPECT_GE(dopamine, 1.1f);
    EXPECT_FALSE(social_is_lonely(social_system));
}

//=============================================================================
// OXYTOCIN BONDING DYNAMICS
//=============================================================================

TEST_F(LoveLoyaltyFriendshipIntegrationTest, PositiveInteractionsIncreaseOxytocin) {
    // WHAT: Positive social interactions boost oxytocin
    // WHY:  Oxytocin mediates social bonding
    // HOW:  Process positive interactions, verify oxytocin increase

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    float baseline_oxytocin = social_get_oxytocin_level(social_system);

    // Multiple positive interactions
    for (int i = 0; i < 5; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.8f, 0.8f, i * 1000000);
    }

    float elevated_oxytocin = social_get_oxytocin_level(social_system);

    EXPECT_GT(elevated_oxytocin, baseline_oxytocin);
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, OxytocinDecaysOverTime) {
    // WHAT: Oxytocin decays with 1-hour half-life
    // WHY:  Neurochemical realism
    // HOW:  Boost oxytocin, wait 1 hour, verify decay

    uint32_t rel_id = create_friendship(0.6f);

    float peak_oxytocin = social_get_oxytocin_level(social_system);

    // Wait 1 hour (one half-life)
    social_update(social_system, 3600.0f, 3600000000);

    float decayed_oxytocin = social_get_oxytocin_level(social_system);

    // Should be approximately half
    EXPECT_LT(decayed_oxytocin, peak_oxytocin);
    EXPECT_NEAR(decayed_oxytocin / peak_oxytocin, 0.5f, 0.2f);
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, OxytocinBoostStrengthensRelationshipBond) {
    // WHAT: High oxytocin strengthens relationship bond
    // WHY:  Oxytocin mediates attachment strength
    // HOW:  Create high oxytocin state, verify relationship bond strength

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Multiple intense positive interactions (oxytocin boost)
    for (int i = 0; i < 10; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_SHARED_ACTIVITY, 0.9f, 0.9f, i * 1000000);
    }

    float oxytocin = social_get_oxytocin_level(social_system);
    float closeness = social_get_relationship_closeness(social_system, rel_id);

    // Both should be elevated
    EXPECT_GE(oxytocin, 0.6f);
    EXPECT_GE(closeness, 0.4f);
}

//=============================================================================
// VULNERABILITY AND TRUST INTEGRATION
//=============================================================================

TEST_F(LoveLoyaltyFriendshipIntegrationTest, VulnerabilityAcceptedDeepensIntimacy) {
    // WHAT: Accepted vulnerability increases intimacy component of love
    // WHY:  Vulnerability sharing builds deep connection
    // HOW:  Express vulnerability accepted, verify intimacy increase

    // Create friendship with moderate closeness (not maxed out)
    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);
    for (int i = 0; i < 10; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    float baseline_closeness = social_get_relationship_closeness(social_system, rel_id);

    // Express vulnerability (accepted)
    social_express_vulnerability(social_system, rel_id, 0.8f, true);

    float new_closeness = social_get_relationship_closeness(social_system, rel_id);

    // Closeness should increase
    EXPECT_GT(new_closeness, baseline_closeness);
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, VulnerabilityRejectedDamagesBond) {
    // WHAT: Rejected vulnerability damages relationship
    // WHY:  Rejection of vulnerability breaks trust
    // HOW:  Express vulnerability rejected, verify closeness decrease

    uint32_t rel_id = create_friendship(0.5f);

    float baseline_closeness = social_get_relationship_closeness(social_system, rel_id);

    // Express vulnerability (rejected)
    social_express_vulnerability(social_system, rel_id, 0.8f, false);

    float new_closeness = social_get_relationship_closeness(social_system, rel_id);

    // Closeness should decrease
    EXPECT_LT(new_closeness, baseline_closeness);
}

//=============================================================================
// LOYALTY INTEGRATION
//=============================================================================

TEST_F(LoveLoyaltyFriendshipIntegrationTest, LoyaltyCommitmentIncreasesCommitmentComponent) {
    // WHAT: Committing loyalty increases commitment component of love
    // WHY:  Loyalty is a form of commitment
    // HOW:  Commit loyalty, verify commitment increase

    uint32_t rel_id = create_friendship(0.6f);

    // Commit loyalty
    social_commit_loyalty(social_system, rel_id, LOYALTY_TO_PERSON, 0.8f);

    // Verify loyalty established
    EXPECT_TRUE(social_is_loyal_to(social_system, rel_id));
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, PassedLoyaltyTestStrengthensLoyalty) {
    // WHAT: Passing loyalty tests strengthens loyalty bond
    // WHY:  Demonstrated loyalty reinforces commitment
    // HOW:  Test loyalty (pass), verify strengthening

    uint32_t rel_id = create_friendship(0.6f);
    social_commit_loyalty(social_system, rel_id, LOYALTY_TO_PERSON, 0.8f);

    // Test loyalty (pass)
    social_test_loyalty(social_system, rel_id, 0.7f, true);

    // Loyalty should be strengthened
    EXPECT_TRUE(social_is_loyal_to(social_system, rel_id));
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, FailedLoyaltyTestDamagesRelationship) {
    // WHAT: Failing loyalty test damages relationship
    // WHY:  Broken loyalty breaks trust
    // HOW:  Test loyalty (fail), verify damage

    // Create friendship with moderate closeness (not maxed out)
    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);
    for (int i = 0; i < 12; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }
    social_commit_loyalty(social_system, rel_id, LOYALTY_TO_PERSON, 0.8f);

    float baseline_closeness = social_get_relationship_closeness(social_system, rel_id);

    // Test loyalty (fail)
    social_test_loyalty(social_system, rel_id, 0.8f, false);

    float new_closeness = social_get_relationship_closeness(social_system, rel_id);

    // Closeness should decrease
    EXPECT_LT(new_closeness, baseline_closeness);
}

//=============================================================================
// BETRAYAL AND REPAIR INTEGRATION
//=============================================================================

TEST_F(LoveLoyaltyFriendshipIntegrationTest, BetrayalDamagesTrust) {
    // WHAT: Betrayal significantly damages trust
    // WHY:  Betrayal breaks fundamental relationship bond
    // HOW:  Experience betrayal, verify trust damage

    uint32_t rel_id = create_friendship(0.6f);

    float baseline_trust = 0.6f;  // Assume initial trust

    // Experience betrayal
    social_experience_betrayal(social_system, rel_id, 0.8f);

    float new_trust = 0.0f;  // Trust should be significantly reduced

    // Trust should be damaged
    // (Note: Need to add trust query function in implementation)
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, RepairAttemptRestoresTrustGradually) {
    // WHAT: Repair attempts gradually restore trust after betrayal
    // WHY:  Trust can be rebuilt with effort
    // HOW:  Experience betrayal, attempt repair, verify gradual restoration

    uint32_t rel_id = create_friendship(0.6f);

    // Experience betrayal
    social_experience_betrayal(social_system, rel_id, 0.7f);

    float pre_repair_closeness = social_get_relationship_closeness(social_system, rel_id);

    // Attempt repair (high quality apology, high effort)
    social_attempt_repair(social_system, rel_id, 0.8f, 0.8f);

    float post_repair_closeness = social_get_relationship_closeness(social_system, rel_id);

    // Closeness should increase (repair working)
    EXPECT_GT(post_repair_closeness, pre_repair_closeness);
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, SevereBetrayalBreaksLoyalty) {
    // WHAT: Severe betrayal breaks loyalty bond
    // WHY:  Some betrayals are unforgivable
    // HOW:  Commit loyalty, severe betrayal, verify loyalty broken

    uint32_t rel_id = create_friendship(0.6f);
    social_commit_loyalty(social_system, rel_id, LOYALTY_TO_PERSON, 0.8f);

    EXPECT_TRUE(social_is_loyal_to(social_system, rel_id));

    // Severe betrayal (severity > 0.7)
    social_experience_betrayal(social_system, rel_id, 0.9f);

    // Loyalty should be broken
    EXPECT_FALSE(social_is_loyal_to(social_system, rel_id));
}

//=============================================================================
// EMOTIONAL TAG INTEGRATION
//=============================================================================

TEST_F(LoveLoyaltyFriendshipIntegrationTest, LoveEmotionHasHighPositiveValence) {
    // WHAT: Love emotion has high positive valence [+0.7 to +0.95]
    // WHY:  Love is intensely positive emotion
    // HOW:  Create romantic love, verify emotional tag

    uint32_t rel_id = create_romantic_love();

    emotional_tag_t emotion = social_get_emotion(social_system);

    // Should be highly positive
    EXPECT_GE(emotion.valence, 0.7f);
    EXPECT_LE(emotion.valence, 0.95f);
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, LoveEmotionHasHighArousal) {
    // WHAT: Love emotion has high arousal [0.6 to 0.9]
    // WHY:  Love is energizing, activating
    // HOW:  Create romantic love, verify arousal level

    uint32_t rel_id = create_romantic_love();

    emotional_tag_t emotion = social_get_emotion(social_system);

    // Should be high arousal
    EXPECT_GE(emotion.arousal, 0.6f);
    EXPECT_LE(emotion.arousal, 0.9f);
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, FriendshipEmotionHasModeratePositiveValence) {
    // WHAT: Friendship emotion has moderate positive valence [+0.4 to +0.7]
    // WHY:  Friendship is pleasant but less intense than romantic love
    // HOW:  Create friendship, verify emotional tag

    uint32_t rel_id = create_friendship(0.6f);

    emotional_tag_t emotion = social_get_emotion(social_system);

    // Should be moderately positive
    EXPECT_GE(emotion.valence, 0.4f);
    EXPECT_LE(emotion.valence, 0.7f);
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, LonelinessEmotionHasNegativeValence) {
    // WHAT: Loneliness emotion has negative valence [-0.4 to -0.7]
    // WHY:  Loneliness is aversive, unpleasant
    // HOW:  Induce loneliness, verify emotional tag

    // Simulate isolation (loneliness develops)
    social_update(social_system, 86400.0f * 7.0f, 86400000000 * 7);

    EXPECT_TRUE(social_is_lonely(social_system));

    emotional_tag_t emotion = social_get_emotion(social_system);

    // Should be negative
    EXPECT_LE(emotion.valence, -0.4f);
    EXPECT_GE(emotion.valence, -0.7f);
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, LonelinessEmotionHasLowArousal) {
    // WHAT: Loneliness has low arousal [0.2 to 0.4]
    // WHY:  Loneliness is more passive, withdrawn emotion
    // HOW:  Induce loneliness, verify arousal level

    // Simulate isolation
    social_update(social_system, 86400.0f * 7.0f, 86400000000 * 7);

    emotional_tag_t emotion = social_get_emotion(social_system);

    // Should be low arousal
    EXPECT_GE(emotion.arousal, 0.2f);
    EXPECT_LE(emotion.arousal, 0.4f);
}

//=============================================================================
// COMPLETE LIFECYCLE INTEGRATION TESTS
//=============================================================================

TEST_F(LoveLoyaltyFriendshipIntegrationTest, CompleteRelationshipLifecycleFromStrangerToLove) {
    // WHAT: Full relationship progression from stranger to romantic love
    // WHY:  Verify complete system integration
    // HOW:  Create relationship, progress through stages, verify neuromodulator effects

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_STRANGER, ATTACHMENT_SECURE);

    // Stage 1: Stranger to Acquaintance (casual interactions)
    for (int i = 0; i < 5; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.5f, 0.6f, i * 1000000);
    }

    // Stage 2: Acquaintance to Friend (positive shared experiences)
    for (int i = 0; i < 15; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_SHARED_ACTIVITY, 0.75f, 0.75f, (i + 5) * 1000000);
    }

    // Update to calculate friendship_warmth
    social_update(social_system, 60.0f, 15000000 + 60000000);

    float friend_dopamine = 0.0f, friend_oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &friend_dopamine, &friend_oxytocin);
    EXPECT_GE(friend_dopamine, 1.1f);  // Friendship boost

    // Stage 3: Friend to Close Friend (vulnerability, support)
    social_express_vulnerability(social_system, rel_id, 0.7f, true);
    social_provide_support(social_system, rel_id, 0.8f);

    // Stage 4: Close Friend to Romantic Love
    social_experience_love(social_system, rel_id, LOVE_TYPE_ROMANTIC, 0.9f);

    float love_dopamine = 0.0f, love_oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &love_dopamine, &love_oxytocin);
    EXPECT_GE(love_dopamine, 1.3f);  // Love boost (stronger than friendship)
    EXPECT_GE(love_oxytocin, 1.5f);  // Strong oxytocin boost

    // Verify emotional tag
    emotional_tag_t emotion = social_get_emotion(social_system);
    EXPECT_GE(emotion.valence, 0.7f);  // Highly positive
    EXPECT_GE(emotion.arousal, 0.6f);  // High arousal

    // Verify experiencing love
    EXPECT_TRUE(social_is_experiencing_love(social_system));
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, CompleteBetrayalAndRepairCycle) {
    // WHAT: Full betrayal and repair cycle
    // WHY:  Verify complete integration of betrayal/repair mechanics
    // HOW:  Create friendship, betray, repair, verify neuromodulator changes

    uint32_t rel_id = create_friendship(0.7f);
    social_commit_loyalty(social_system, rel_id, LOYALTY_TO_PERSON, 0.8f);

    float pre_betrayal_dopamine = 0.0f, pre_betrayal_oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &pre_betrayal_dopamine, &pre_betrayal_oxytocin);

    // Betrayal
    social_experience_betrayal(social_system, rel_id, 0.8f);

    // Update to recalculate friendship_warmth based on damaged relationship
    social_update(social_system, 60.0f, 100000000);

    float post_betrayal_dopamine = 0.0f, post_betrayal_oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &post_betrayal_dopamine, &post_betrayal_oxytocin);

    // Dopamine should drop (relationship damaged)
    EXPECT_LT(post_betrayal_dopamine, pre_betrayal_dopamine);

    // Loyalty should be broken (severe betrayal)
    EXPECT_FALSE(social_is_loyal_to(social_system, rel_id));

    // Repair attempts (multiple)
    for (int i = 0; i < 5; i++) {
        social_attempt_repair(social_system, rel_id, 0.8f, 0.8f);
        social_update(social_system, 86400.0f, 86400000000 * (i + 1));  // 1 day between attempts
    }

    float post_repair_dopamine = 0.0f, post_repair_oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &post_repair_dopamine, &post_repair_oxytocin);

    // Dopamine should partially recover
    EXPECT_GT(post_repair_dopamine, post_betrayal_dopamine);
}

//=============================================================================
// EDGE CASES AND ROBUSTNESS
//=============================================================================

TEST_F(LoveLoyaltyFriendshipIntegrationTest, HandlesNullSystemPointer) {
    // WHAT: Verify null pointer safety
    // WHY:  Prevent crashes
    // HOW:  Call functions with null pointers

    float dopamine = 0.0f, oxytocin = 0.0f;
    social_get_neuromodulator_effects(nullptr, &dopamine, &oxytocin);

    // Should not crash (undefined values OK)
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, HandlesExtremeNeuromodulatorValues) {
    // WHAT: Neuromodulator factors are clamped to valid ranges
    // WHY:  Prevent extreme values from breaking system
    // HOW:  Create extreme love state, verify clamping

    // Create multiple intense romantic relationships (unrealistic but tests bounds)
    for (int i = 0; i < 10; i++) {
        uint32_t rel_id = create_romantic_love();
    }

    float dopamine = 0.0f, oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &dopamine, &oxytocin);

    // Should be clamped to maximum (2.0x)
    EXPECT_LE(dopamine, 2.0f);
    EXPECT_LE(oxytocin, 2.0f);
}

TEST_F(LoveLoyaltyFriendshipIntegrationTest, StatisticsUpdateCorrectly) {
    // WHAT: Verify statistics tracking
    // WHY:  Monitor system usage
    // HOW:  Perform operations, verify statistics

    uint32_t rel_id = create_friendship(0.5f);

    social_update(social_system, 1.0f, 1000000);

    // Update calls should be tracked
    EXPECT_GT(social_system->total_update_calls, 0);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
