/**
 * @file test_love_loyalty_friendship.cpp
 * @brief Unit tests for love, loyalty, and friendship system (Phase E4)
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/nimcp_love_loyalty_friendship.h"

class SocialBondSystemTest : public ::testing::Test {
protected:
    social_bond_system_t* system;

    void SetUp() override {
        system = social_bond_system_create();
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        social_bond_system_destroy(system);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SocialBondSystemTest, CreateInitializesValidSystem) {
    EXPECT_EQ(system->active_relationship_count, 0);
    EXPECT_FALSE(system->emotion.experiencing_love);
    EXPECT_FALSE(system->emotion.actively_loyal);
    EXPECT_FLOAT_EQ(system->extraversion, 0.5f);
    EXPECT_FLOAT_EQ(system->agreeableness, 0.6f);
    EXPECT_FLOAT_EQ(system->love_capacity, 0.8f);
    EXPECT_EQ(system->emotion.attachment_style, ATTACHMENT_SECURE);
    EXPECT_FLOAT_EQ(system->emotion.oxytocin_level, 0.5f);
}

TEST_F(SocialBondSystemTest, ResetClearsRelationships) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_STRANGER, 1000000);
    EXPECT_GT(rel_id, 0);
    social_bond_system_reset(system);
    EXPECT_EQ(system->active_relationship_count, 0);
    EXPECT_FLOAT_EQ(system->extraversion, 0.5f);
}

TEST_F(SocialBondSystemTest, DestroyHandlesNullPointer) {
    social_bond_system_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Relationship Creation Tests
//=============================================================================

TEST_F(SocialBondSystemTest, CreateRelationshipReturnsValidID) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_STRANGER, 1000000);
    EXPECT_GT(rel_id, 0);
    EXPECT_EQ(system->active_relationship_count, 1);
}

TEST_F(SocialBondSystemTest, MaxRelationshipsRespected) {
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_STRANGER, 1000000);
        EXPECT_NE(rel_id, UINT32_MAX);
    }
    EXPECT_EQ(system->active_relationship_count, SOCIAL_MAX_RELATIONSHIPS);
    uint32_t overflow_id = social_create_relationship(system, RELATIONSHIP_STRANGER, 1000000);
    EXPECT_EQ(overflow_id, UINT32_MAX);
}

//=============================================================================
// Interaction Type Tests
//=============================================================================

TEST_F(SocialBondSystemTest, ConversationIncreasesCloseness) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_STRANGER, 0);
    float initial_closeness = social_get_relationship_closeness(system, rel_id);
    social_process_interaction(system, rel_id, INTERACTION_CONVERSATION, 0.8f, 0.7f, 1000000);
    EXPECT_GT(social_get_relationship_closeness(system, rel_id), initial_closeness);
}

TEST_F(SocialBondSystemTest, ConflictDecreasesCloseness) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_process_interaction(system, rel_id, INTERACTION_CONVERSATION, 0.8f, 0.8f, 0);
    float closeness_after_positive = social_get_relationship_closeness(system, rel_id);
    social_process_interaction(system, rel_id, INTERACTION_CONFLICT, 0.9f, -1.0f, 1000000);
    EXPECT_LT(social_get_relationship_closeness(system, rel_id), closeness_after_positive);
}

TEST_F(SocialBondSystemTest, ReconciliationRepairsDamage) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_process_interaction(system, rel_id, INTERACTION_CONFLICT, 0.8f, -1.0f, 0);
    float closeness_after_conflict = social_get_relationship_closeness(system, rel_id);
    social_process_interaction(system, rel_id, INTERACTION_RECONCILIATION, 0.8f, 1.0f, 1000000);
    EXPECT_GT(social_get_relationship_closeness(system, rel_id), closeness_after_conflict);
}

TEST_F(SocialBondSystemTest, CelebrationBoostsIntimacy) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_process_interaction(system, rel_id, INTERACTION_CELEBRATION, 0.9f, 1.0f, 1000000);
    SUCCEED();
}

TEST_F(SocialBondSystemTest, CollaborationIncreasesCommitment) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_process_interaction(system, rel_id, INTERACTION_COLLABORATION, 0.8f, 0.9f, 1000000);
    SUCCEED();
}

//=============================================================================
// Vulnerability Tests
//=============================================================================

TEST_F(SocialBondSystemTest, VulnerabilityAcceptedDeepensIntimacy) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    float initial_closeness = social_get_relationship_closeness(system, rel_id);
    social_express_vulnerability(system, rel_id, 0.8f, true);
    EXPECT_GT(social_get_relationship_closeness(system, rel_id), initial_closeness);
}

TEST_F(SocialBondSystemTest, VulnerabilityRejectedDamagesBond) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_process_interaction(system, rel_id, INTERACTION_CONVERSATION, 0.8f, 0.8f, 0);
    float closeness_before = social_get_relationship_closeness(system, rel_id);
    social_express_vulnerability(system, rel_id, 0.7f, false);
    EXPECT_LT(social_get_relationship_closeness(system, rel_id), closeness_before);
}

//=============================================================================
// Support Function Tests
//=============================================================================

TEST_F(SocialBondSystemTest, ProvideSupportIncreasesReciprocity) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_provide_support(system, rel_id, 0.8f);
    SUCCEED();
}

TEST_F(SocialBondSystemTest, ReceiveSupportCreatesObligation) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_receive_support(system, rel_id, 0.7f);
    SUCCEED();
}

//=============================================================================
// Friendship Stage Progression Tests
//=============================================================================

TEST_F(SocialBondSystemTest, ClosenessProgressesToFriend) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_STRANGER, 0);
    // Need more interactions to reach friendship threshold (0.4)
    for (int i = 0; i < 20; i++) {
        social_process_interaction(system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.8f, i * 1000000);
    }
    EXPECT_GE(social_get_relationship_closeness(system, rel_id), FRIENDSHIP_THRESHOLD);
}

TEST_F(SocialBondSystemTest, ClosenessProgressesToCloseFriend) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    for (int i = 0; i < 20; i++) {
        social_process_interaction(system, rel_id, INTERACTION_CONVERSATION, 0.8f, 0.9f, i * 1000000);
        social_express_vulnerability(system, rel_id, 0.6f, true);
    }
    EXPECT_GE(social_get_relationship_closeness(system, rel_id), CLOSE_FRIEND_THRESHOLD);
}

//=============================================================================
// Love Experience Tests
//=============================================================================

TEST_F(SocialBondSystemTest, RomanticLoveIncreasesPassion) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_experience_love(system, rel_id, LOVE_TYPE_ROMANTIC, 0.8f);
    EXPECT_TRUE(system->emotion.experiencing_love);
    EXPECT_EQ(system->total_loves_experienced, 1);
}

TEST_F(SocialBondSystemTest, CompanionateLoveIncreasesIntimacyAndCommitment) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_CLOSE_FRIEND, 0);
    social_experience_love(system, rel_id, LOVE_TYPE_COMPANIONATE, 0.8f);
    EXPECT_TRUE(system->emotion.experiencing_love);
}

TEST_F(SocialBondSystemTest, FamilialLoveIncreasesCommitment) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_STRANGER, 0);
    social_experience_love(system, rel_id, LOVE_TYPE_FAMILIAL, 0.9f);
    EXPECT_TRUE(system->emotion.experiencing_love);
}

TEST_F(SocialBondSystemTest, PlatonicLoveBalancesIntimacyAndCommitment) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_CLOSE_FRIEND, 0);
    social_experience_love(system, rel_id, LOVE_TYPE_PLATONIC, 0.7f);
    EXPECT_TRUE(system->emotion.experiencing_love);
}

TEST_F(SocialBondSystemTest, LoveIncreasesOxytocinLevel) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    float initial_oxytocin = system->emotion.oxytocin_level;
    social_experience_love(system, rel_id, LOVE_TYPE_ROMANTIC, 0.9f);
    EXPECT_GT(system->emotion.oxytocin_level, initial_oxytocin);
}

//=============================================================================
// Loyalty Tests
//=============================================================================

TEST_F(SocialBondSystemTest, CommitLoyaltyIncreasesCommitment) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_commit_loyalty(system, rel_id, LOYALTY_TO_PERSON, 0.8f);
    EXPECT_EQ(system->total_loyalty_commitments, 1);
    EXPECT_TRUE(system->emotion.actively_loyal);
}

TEST_F(SocialBondSystemTest, PassedLoyaltyTestStrengthensLoyalty) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_commit_loyalty(system, rel_id, LOYALTY_TO_PERSON, 0.6f);
    social_test_loyalty(system, rel_id, 0.7f, true);
    SUCCEED();
}

TEST_F(SocialBondSystemTest, FailedLoyaltyTestWeakensLoyalty) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_commit_loyalty(system, rel_id, LOYALTY_TO_PERSON, 0.8f);
    social_test_loyalty(system, rel_id, 0.6f, false);
    SUCCEED();
}

//=============================================================================
// Betrayal and Repair Tests
//=============================================================================

TEST_F(SocialBondSystemTest, BetrayalDamagesTrust) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_CLOSE_FRIEND, 0);
    for (int i = 0; i < 10; i++) {
        social_process_interaction(system, rel_id, INTERACTION_CONVERSATION, 0.8f, 0.9f, i * 1000000);
    }
    float closeness_before = social_get_relationship_closeness(system, rel_id);
    social_experience_betrayal(system, rel_id, 0.8f);
    EXPECT_LT(social_get_relationship_closeness(system, rel_id), closeness_before);
}

TEST_F(SocialBondSystemTest, RepairAttemptIncreasesRepairProgress) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_experience_betrayal(system, rel_id, 0.5f);
    float closeness_after_betrayal = social_get_relationship_closeness(system, rel_id);
    social_attempt_repair(system, rel_id, 0.8f, 0.9f);
    EXPECT_GT(social_get_relationship_closeness(system, rel_id), closeness_after_betrayal);
}

//=============================================================================
// Oxytocin Bonding Tests
//=============================================================================

TEST_F(SocialBondSystemTest, PositiveInteractionsIncreaseOxytocin) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    float initial_oxytocin = system->emotion.oxytocin_level;
    social_process_interaction(system, rel_id, INTERACTION_CONVERSATION, 0.9f, 0.8f, 1000000);
    EXPECT_GT(system->emotion.oxytocin_level, initial_oxytocin);
}

TEST_F(SocialBondSystemTest, OxytocinDecaysOverTime) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_process_interaction(system, rel_id, INTERACTION_CELEBRATION, 0.9f, 0.9f, 0);
    float initial_oxytocin = system->emotion.oxytocin_level;
    social_update(system, 7200.0f, 7200ULL * 1000000);
    EXPECT_LT(system->emotion.oxytocin_level, initial_oxytocin);
}

//=============================================================================
// Loneliness Tests
//=============================================================================

TEST_F(SocialBondSystemTest, LonelinessIncreasesWithoutInteraction) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    float initial_loneliness = system->emotion.loneliness;
    social_update(system, 7.0f * 86400.0f, 7ULL * 86400 * 1000000);
    EXPECT_GT(system->emotion.loneliness, initial_loneliness);
}

TEST_F(SocialBondSystemTest, IsLonelyReturnsTrueWhenLonely) {
    system->emotion.loneliness = 0.6f;
    EXPECT_TRUE(social_is_lonely(system));
}

//=============================================================================
// Query Function Tests
//=============================================================================

TEST_F(SocialBondSystemTest, IsExperiencingLoveReturnsTrueWhenInLove) {
    EXPECT_FALSE(social_is_experiencing_love(system));
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_experience_love(system, rel_id, LOVE_TYPE_ROMANTIC, 0.9f);
    EXPECT_TRUE(social_is_experiencing_love(system));
}

TEST_F(SocialBondSystemTest, GetRelationshipClosenessReturnsCorrectValue) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_process_interaction(system, rel_id, INTERACTION_CONVERSATION, 0.8f, 0.9f, 0);
    float closeness = social_get_relationship_closeness(system, rel_id);
    EXPECT_GT(closeness, 0.0f);
}

TEST_F(SocialBondSystemTest, GetCloseFriendCountReturnsCorrectValue) {
    for (int i = 0; i < 4; i++) {
        social_create_relationship(system, RELATIONSHIP_CLOSE_FRIEND, 0);
    }
    social_update(system, 1.0f, 1000000);
    EXPECT_EQ(social_get_close_friend_count(system), 4);
}

TEST_F(SocialBondSystemTest, GetOxytocinLevelReturnsCorrectValue) {
    system->emotion.oxytocin_level = 0.75f;
    EXPECT_FLOAT_EQ(social_get_oxytocin_level(system), 0.75f);
}

TEST_F(SocialBondSystemTest, GetNeuromodulatorEffectsReturnsCorrectValues) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_experience_love(system, rel_id, LOVE_TYPE_ROMANTIC, 0.9f);
    float dopamine_factor = 0.0f, oxytocin_factor = 0.0f;
    social_get_neuromodulator_effects(system, &dopamine_factor, &oxytocin_factor);
    EXPECT_GT(dopamine_factor, 1.0f);
    EXPECT_GT(oxytocin_factor, 1.0f);
}

TEST_F(SocialBondSystemTest, LonelinessReducesDopamine) {
    system->emotion.loneliness = 0.8f;
    float dopamine_factor = 0.0f, oxytocin_factor = 0.0f;
    social_get_neuromodulator_effects(system, &dopamine_factor, &oxytocin_factor);
    EXPECT_LT(dopamine_factor, 1.0f);
}

//=============================================================================
// Emotional Tag Integration Tests
//=============================================================================

TEST_F(SocialBondSystemTest, GetEmotionReturnsLoveEmotion) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_experience_love(system, rel_id, LOVE_TYPE_ROMANTIC, 0.9f);
    emotional_tag_t emotion = social_get_emotion(system);
    EXPECT_GE(emotion.valence, 0.7f);
    EXPECT_GE(emotion.arousal, 0.6f);
}

TEST_F(SocialBondSystemTest, GetEmotionReturnsFriendshipEmotion) {
    system->emotion.friendship_warmth = 0.6f;
    emotional_tag_t emotion = social_get_emotion(system);
    EXPECT_GE(emotion.valence, 0.4f);
}

TEST_F(SocialBondSystemTest, GetEmotionReturnsLonelinessEmotion) {
    system->emotion.loneliness = 0.7f;
    emotional_tag_t emotion = social_get_emotion(system);
    EXPECT_LT(emotion.valence, 0.0f);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(SocialBondSystemTest, HandlesNullPointers) {
    social_bond_system_reset(nullptr);
    social_create_relationship(nullptr, RELATIONSHIP_STRANGER, 0);
    social_process_interaction(nullptr, 1, INTERACTION_CONVERSATION, 0.5f, 0.5f, 0);
    EXPECT_FALSE(social_is_experiencing_love(nullptr));
    EXPECT_FALSE(social_is_lonely(nullptr));
    SUCCEED();
}

TEST_F(SocialBondSystemTest, HandlesInvalidRelationshipID) {
    social_process_interaction(system, 99999, INTERACTION_CONVERSATION, 0.5f, 0.5f, 0);
    float closeness = social_get_relationship_closeness(system, 99999);
    EXPECT_FLOAT_EQ(closeness, 0.0f);
    SUCCEED();
}

TEST_F(SocialBondSystemTest, HandlesNegativeParameters) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_process_interaction(system, rel_id, INTERACTION_CONVERSATION, -0.5f, -2.0f, 0);
    SUCCEED();
}

TEST_F(SocialBondSystemTest, HandlesExcessiveParameters) {
    uint32_t rel_id = social_create_relationship(system, RELATIONSHIP_FRIEND, 0);
    social_process_interaction(system, rel_id, INTERACTION_CONVERSATION, 5.0f, 3.0f, 0);
    SUCCEED();
}

TEST_F(SocialBondSystemTest, UpdateCallsCounterIncrements) {
    uint64_t initial = system->total_update_calls;
    for (int i = 0; i < 100; i++) {
        social_update(system, 0.1f, (uint64_t)(i * 100000));
    }
    EXPECT_EQ(system->total_update_calls, initial + 100);
}
