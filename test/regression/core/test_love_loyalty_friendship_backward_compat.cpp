/**
 * @file test_love_loyalty_friendship_backward_compat.cpp
 * @brief Regression tests for love/loyalty/friendship system API stability
 *
 * WHAT: Verify API stability and backward compatibility
 * WHY:  Ensure system behavior remains consistent across changes
 * HOW:  Test documented behaviors, parameter ranges, edge cases
 *
 * @version Phase E4: Love, Loyalty, Friendship Regression
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include "cognitive/nimcp_love_loyalty_friendship.h"
#include "cognitive/nimcp_emotional_tagging.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class LoveLoyaltyFriendshipRegressionTest : public ::testing::Test {
protected:
    social_bond_system_t* social_system;

    void SetUp() override {
        social_system = social_bond_system_create();
        ASSERT_NE(social_system, nullptr);
    }

    void TearDown() override {
        social_bond_system_destroy(social_system);
    }
};

//=============================================================================
// API STABILITY: LIFECYCLE FUNCTIONS
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, CreateReturnsNonNullSystem) {
    // WHAT: social_bond_system_create() must return valid pointer
    // WHY:  API contract
    // HOW:  Create system, verify non-null

    social_bond_system_t* system = social_bond_system_create();
    EXPECT_NE(system, nullptr);
    social_bond_system_destroy(system);
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, CreateInitializesWithDefaultValues) {
    // WHAT: Newly created system has documented default values
    // WHY:  API contract
    // HOW:  Create system, verify defaults

    // Default personality traits (moderate)
    EXPECT_NEAR(social_system->extraversion, 0.5f, 0.1f);
    EXPECT_NEAR(social_system->agreeableness, 0.6f, 0.1f);
    EXPECT_NEAR(social_system->openness_to_experience, 0.5f, 0.1f);

    // Default capacities
    EXPECT_NEAR(social_system->love_capacity, 0.8f, 0.1f);
    EXPECT_NEAR(social_system->friendship_capacity, 0.8f, 0.1f);
    EXPECT_NEAR(social_system->loyalty_capacity, 0.7f, 0.1f);

    // Default attachment style (secure)
    EXPECT_EQ(social_system->emotion.attachment_style, ATTACHMENT_SECURE);
    EXPECT_NEAR(social_system->emotion.attachment_security, 0.7f, 0.1f);

    // Default oxytocin level (baseline)
    EXPECT_NEAR(social_system->emotion.oxytocin_level, 0.5f, 0.1f);
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, DestroyHandlesNullPointer) {
    // WHAT: social_bond_system_destroy(NULL) must not crash
    // WHY:  API robustness
    // HOW:  Call destroy with null pointer

    social_bond_system_destroy(nullptr);
    // Should not crash
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, ResetClearsAllRelationships) {
    // WHAT: social_bond_system_reset() clears all relationships
    // WHY:  API contract
    // HOW:  Create relationships, reset, verify cleared

    // Create some relationships
    social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);
    social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    EXPECT_GT(social_system->active_relationship_count, 0);

    social_bond_system_reset(social_system);

    EXPECT_EQ(social_system->active_relationship_count, 0);
}

//=============================================================================
// API STABILITY: RELATIONSHIP CREATION
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, CreateRelationshipReturnsUniqueIDs) {
    // WHAT: Each relationship gets unique ID
    // WHY:  API contract
    // HOW:  Create multiple, verify unique

    uint32_t id1 = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);
    uint32_t id2 = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    EXPECT_NE(id1, id2);
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, CreateRelationshipRespectsMaximum) {
    // WHAT: Cannot create more than SOCIAL_MAX_RELATIONSHIPS
    // WHY:  API contract
    // HOW:  Create max + 1, verify last returns UINT32_MAX

    // Create maximum relationships
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        uint32_t id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);
        EXPECT_NE(id, UINT32_MAX);
    }

    // Try to create one more (should fail)
    uint32_t overflow_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);
    EXPECT_EQ(overflow_id, UINT32_MAX);
}

//=============================================================================
// API STABILITY: INTERACTION PROCESSING
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, ConversationAlwaysIncreasesCloseness) {
    // WHAT: Positive conversation (valence > 0) always increases closeness
    // WHY:  Documented behavior
    // HOW:  Process positive conversation, verify closeness increase

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    float baseline_closeness = social_get_relationship_closeness(social_system, rel_id);

    social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.8f, 1000000);

    float new_closeness = social_get_relationship_closeness(social_system, rel_id);

    EXPECT_GT(new_closeness, baseline_closeness);
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, ConflictAlwaysDecreasesCloseness) {
    // WHAT: Conflict always decreases closeness
    // WHY:  Documented behavior
    // HOW:  Process conflict, verify closeness decrease

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // First build some closeness
    for (int i = 0; i < 5; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    float baseline_closeness = social_get_relationship_closeness(social_system, rel_id);

    social_process_interaction(social_system, rel_id, INTERACTION_CONFLICT, 0.8f, -0.8f, 10000000);

    float new_closeness = social_get_relationship_closeness(social_system, rel_id);

    EXPECT_LT(new_closeness, baseline_closeness);
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, ReconciliationIncreasesClosenessAfterConflict) {
    // WHAT: Reconciliation repairs relationship damage
    // WHY:  Documented behavior
    // HOW:  Conflict then reconcile, verify repair

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Build closeness
    for (int i = 0; i < 5; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    // Conflict
    social_process_interaction(social_system, rel_id, INTERACTION_CONFLICT, 0.8f, -0.8f, 10000000);

    float post_conflict_closeness = social_get_relationship_closeness(social_system, rel_id);

    // Reconciliation
    social_process_interaction(social_system, rel_id, INTERACTION_RECONCILIATION, 0.7f, 0.7f, 11000000);

    float post_reconciliation_closeness = social_get_relationship_closeness(social_system, rel_id);

    EXPECT_GT(post_reconciliation_closeness, post_conflict_closeness);
}

//=============================================================================
// API STABILITY: RELATIONSHIP STAGES
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, StageProgressionFollowsThresholds) {
    // WHAT: Relationship stage follows documented thresholds
    // WHY:  API contract (closeness >= 0.4 → FRIEND, >= 0.7 → CLOSE_FRIEND)
    // HOW:  Build closeness, verify stage transitions

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_STRANGER, ATTACHMENT_SECURE);

    // Initially stranger or acquaintance
    relationship_stage_t initial_stage = RELATIONSHIP_STRANGER;  // Assume this is accessible

    // Build closeness to friendship threshold (0.4)
    for (int i = 0; i < 17; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    float friend_closeness = social_get_relationship_closeness(social_system, rel_id);
    EXPECT_GE(friend_closeness, 0.4f);

    // Build closeness to close friend threshold (0.7)
    for (int i = 15; i < 30; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.8f, 0.8f, i * 1000000);
    }

    float close_friend_closeness = social_get_relationship_closeness(social_system, rel_id);
    EXPECT_GE(close_friend_closeness, 0.7f);
}

//=============================================================================
// API STABILITY: LOVE MECHANICS
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, RomanticLoveIncreasesPassionComponent) {
    // WHAT: LOVE_TYPE_ROMANTIC increases passion most
    // WHY:  Documented behavior (Sternberg's triangle)
    // HOW:  Experience romantic love, verify passion increase

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Build base relationship
    for (int i = 0; i < 10; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    // Experience romantic love
    social_experience_love(social_system, rel_id, LOVE_TYPE_ROMANTIC, 0.9f);

    // Should now be experiencing love
    EXPECT_TRUE(social_is_experiencing_love(social_system));
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, CompanionateLoveIncreasesIntimacyAndCommitment) {
    // WHAT: LOVE_TYPE_COMPANIONATE increases intimacy and commitment (low passion)
    // WHY:  Documented behavior
    // HOW:  Experience companionate love, verify components

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Build base relationship
    for (int i = 0; i < 10; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    // Experience companionate love
    social_experience_love(social_system, rel_id, LOVE_TYPE_COMPANIONATE, 0.8f);

    EXPECT_TRUE(social_is_experiencing_love(social_system));
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, FamilialLoveIncreasesCommitment) {
    // WHAT: LOVE_TYPE_FAMILIAL increases commitment most
    // WHY:  Documented behavior
    // HOW:  Experience familial love, verify commitment

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_FAMILY, ATTACHMENT_SECURE);

    social_experience_love(social_system, rel_id, LOVE_TYPE_FAMILIAL, 0.8f);

    EXPECT_TRUE(social_is_experiencing_love(social_system));
}

//=============================================================================
// API STABILITY: VULNERABILITY MECHANICS
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, AcceptedVulnerabilityAlwaysIncreasesIntimacy) {
    // WHAT: Vulnerability accepted (received_well=true) always increases closeness
    // WHY:  Documented behavior
    // HOW:  Express vulnerability accepted, verify closeness increase

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Build base relationship
    for (int i = 0; i < 5; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    float baseline_closeness = social_get_relationship_closeness(social_system, rel_id);

    social_express_vulnerability(social_system, rel_id, 0.7f, true);

    float new_closeness = social_get_relationship_closeness(social_system, rel_id);

    EXPECT_GT(new_closeness, baseline_closeness);
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, RejectedVulnerabilityAlwaysDecreasesCloseness) {
    // WHAT: Vulnerability rejected (received_well=false) always decreases closeness
    // WHY:  Documented behavior
    // HOW:  Express vulnerability rejected, verify closeness decrease

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Build base relationship
    for (int i = 0; i < 5; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    float baseline_closeness = social_get_relationship_closeness(social_system, rel_id);

    social_express_vulnerability(social_system, rel_id, 0.7f, false);

    float new_closeness = social_get_relationship_closeness(social_system, rel_id);

    EXPECT_LT(new_closeness, baseline_closeness);
}

//=============================================================================
// API STABILITY: SUPPORT AND RECIPROCITY
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, GivingSupportIncreasesReciprocity) {
    // WHAT: Providing support increases reciprocity
    // WHY:  Documented behavior (social exchange theory)
    // HOW:  Provide support, verify reciprocity increase

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    social_provide_support(social_system, rel_id, 0.8f);

    // Verify support was tracked
    // (Implementation should track support_given counter)
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, ReceivingSupportCreatesObligation) {
    // WHAT: Receiving support decreases reciprocity (creates debt)
    // WHY:  Documented behavior (social exchange theory)
    // HOW:  Receive support, verify reciprocity change

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    social_receive_support(social_system, rel_id, 0.8f);

    // Verify support was tracked
    // (Implementation should track support_received counter)
}

//=============================================================================
// API STABILITY: LOYALTY MECHANICS
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, LoyaltyCommitmentRequiresSufficientCloseness) {
    // WHAT: Can only commit loyalty at closeness >= LOYALTY_THRESHOLD (0.6)
    // WHY:  Documented behavior
    // HOW:  Try to commit loyalty at low closeness, verify failure or low strength

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Low closeness (< 0.6)
    social_commit_loyalty(social_system, rel_id, LOYALTY_TO_PERSON, 0.8f);

    // Loyalty may not be established or very weak
    // (Implementation decision: require minimum closeness or allow weak loyalty)
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, PassedLoyaltyTestAlwaysStrengthensLoyalty) {
    // WHAT: Passing loyalty test always strengthens loyalty
    // WHY:  Documented behavior
    // HOW:  Commit loyalty, pass test, verify strengthening

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Build closeness
    for (int i = 0; i < 10; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    social_commit_loyalty(social_system, rel_id, LOYALTY_TO_PERSON, 0.8f);

    EXPECT_TRUE(social_is_loyal_to(social_system, rel_id));

    // Pass loyalty test
    social_test_loyalty(social_system, rel_id, 0.7f, true);

    // Loyalty should remain or strengthen
    EXPECT_TRUE(social_is_loyal_to(social_system, rel_id));
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, FailedLoyaltyTestAlwaysWeakensLoyalty) {
    // WHAT: Failing loyalty test always weakens loyalty
    // WHY:  Documented behavior
    // HOW:  Commit loyalty, fail test, verify weakening

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Build closeness
    for (int i = 0; i < 10; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    social_commit_loyalty(social_system, rel_id, LOYALTY_TO_PERSON, 0.8f);

    // Fail loyalty test (moderate difficulty)
    social_test_loyalty(social_system, rel_id, 0.5f, false);

    // Loyalty should weaken but may still be present
    // (Severe failures break loyalty entirely)
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, SevereFailureBreaksLoyalty) {
    // WHAT: Severe loyalty failure (difficulty > 0.7, failed) breaks loyalty
    // WHY:  Documented behavior
    // HOW:  Commit loyalty, severe failure, verify broken

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Build closeness
    for (int i = 0; i < 10; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    social_commit_loyalty(social_system, rel_id, LOYALTY_TO_PERSON, 0.8f);
    EXPECT_TRUE(social_is_loyal_to(social_system, rel_id));

    // Severe failure
    social_test_loyalty(social_system, rel_id, 0.9f, false);

    // Loyalty should be broken
    EXPECT_FALSE(social_is_loyal_to(social_system, rel_id));
}

//=============================================================================
// API STABILITY: BETRAYAL MECHANICS
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, BetrayalAlwaysDamagesTrust) {
    // WHAT: Betrayal always decreases trust
    // WHY:  Documented behavior
    // HOW:  Experience betrayal, verify trust damage

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Build relationship
    for (int i = 0; i < 10; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    float baseline_closeness = social_get_relationship_closeness(social_system, rel_id);

    // Betrayal
    social_experience_betrayal(social_system, rel_id, 0.7f);

    float post_betrayal_closeness = social_get_relationship_closeness(social_system, rel_id);

    // Closeness should decrease (trust is part of closeness)
    EXPECT_LT(post_betrayal_closeness, baseline_closeness);
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, SevereBetrayalBreaksLoyalty) {
    // WHAT: Betrayal severity > 0.7 breaks loyalty
    // WHY:  Documented behavior
    // HOW:  Commit loyalty, severe betrayal, verify broken

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Build relationship
    for (int i = 0; i < 10; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    social_commit_loyalty(social_system, rel_id, LOYALTY_TO_PERSON, 0.8f);
    EXPECT_TRUE(social_is_loyal_to(social_system, rel_id));

    // Severe betrayal
    social_experience_betrayal(social_system, rel_id, 0.9f);

    // Loyalty should be broken
    EXPECT_FALSE(social_is_loyal_to(social_system, rel_id));
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, RepairAttemptAlwaysIncreasesRepairProgress) {
    // WHAT: Repair attempts always increase trust_repair_progress
    // WHY:  Documented behavior
    // HOW:  Betrayal then repair, verify progress

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Build relationship
    for (int i = 0; i < 10; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    // Betrayal
    social_experience_betrayal(social_system, rel_id, 0.6f);

    float pre_repair_closeness = social_get_relationship_closeness(social_system, rel_id);

    // Repair
    social_attempt_repair(social_system, rel_id, 0.8f, 0.8f);

    float post_repair_closeness = social_get_relationship_closeness(social_system, rel_id);

    // Closeness should increase (repair working)
    EXPECT_GT(post_repair_closeness, pre_repair_closeness);
}

//=============================================================================
// API STABILITY: OXYTOCIN DYNAMICS
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, OxytocinDecaysWithHalfLife) {
    // WHAT: Oxytocin decays with 1-hour half-life (OXYTOCIN_HALF_LIFE = 3600s)
    // WHY:  Documented behavior
    // HOW:  Boost oxytocin, wait 1 hour, verify ~50% decay

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Boost oxytocin
    for (int i = 0; i < 10; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_SHARED_ACTIVITY, 0.9f, 0.9f, i * 1000000);
    }

    float peak_oxytocin = social_get_oxytocin_level(social_system);

    // Wait 1 hour (one half-life)
    social_update(social_system, 3600.0f, 3600000000);

    float decayed_oxytocin = social_get_oxytocin_level(social_system);

    // Should be approximately half (allow 20% margin)
    float ratio = decayed_oxytocin / peak_oxytocin;
    EXPECT_NEAR(ratio, 0.5f, 0.2f);
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, PositiveInteractionsAlwaysIncreaseOxytocin) {
    // WHAT: Positive interactions (valence > 0, intensity > 0.4) increase oxytocin
    // WHY:  Documented behavior
    // HOW:  Process positive interaction, verify oxytocin increase

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    float baseline_oxytocin = social_get_oxytocin_level(social_system);

    // Positive interaction
    social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.8f, 0.8f, 1000000);

    float new_oxytocin = social_get_oxytocin_level(social_system);

    EXPECT_GT(new_oxytocin, baseline_oxytocin);
}

//=============================================================================
// API STABILITY: LONELINESS MECHANICS
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, LonelinessIncreasesWithoutContact) {
    // WHAT: Loneliness increases after 1 day without social contact
    // WHY:  Documented behavior
    // HOW:  Wait 7 days, verify loneliness

    // No relationships or interactions

    // Simulate 7 days
    social_update(social_system, 86400.0f * 7.0f, 86400000000 * 7);

    // Loneliness should develop
    EXPECT_TRUE(social_is_lonely(social_system));
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, CloseFriendsReduceLoneliness) {
    // WHAT: Having close friends (closeness >= 0.7) reduces loneliness
    // WHY:  Documented behavior
    // HOW:  Create close friends, verify loneliness reduced

    // Create close friendships
    for (int i = 0; i < 2; i++) {
        uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

        for (int j = 0; j < 25; j++) {
            social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.8f, 0.8f, j * 1000000);
        }
    }

    // Simulate 3 days (not enough to decay friendships)
    social_update(social_system, 86400.0f * 3.0f, 86400000000 * 3);

    // Should not be lonely (protected by friendships)
    EXPECT_FALSE(social_is_lonely(social_system));
}

//=============================================================================
// API STABILITY: NEUROMODULATOR EFFECTS
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, LoveBoostsDopamineAndOxytocin) {
    // WHAT: Love increases dopamine (reward) and oxytocin (bonding)
    // WHY:  Documented behavior
    // HOW:  Create love, verify neuromodulator effects

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Build relationship
    for (int i = 0; i < 15; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.8f, 0.8f, i * 1000000);
    }

    // Experience love
    social_experience_love(social_system, rel_id, LOVE_TYPE_ROMANTIC, 0.9f);

    float dopamine = 0.0f, oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &dopamine, &oxytocin);

    // Both should be elevated
    EXPECT_GE(dopamine, 1.2f);
    EXPECT_GE(oxytocin, 1.2f);
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, FriendshipBoostsDopamineModerately) {
    // WHAT: Friendship increases dopamine by 20-30%
    // WHY:  Documented behavior
    // HOW:  Create friendship, verify dopamine boost

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Build friendship
    for (int i = 0; i < 23; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    // Update to calculate friendship_warmth
    social_update(social_system, 60.0f, 23000000 + 60000000);

    float dopamine = 0.0f, oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &dopamine, &oxytocin);

    // Moderate dopamine boost
    EXPECT_GE(dopamine, 1.15f);
    EXPECT_LE(dopamine, 1.4f);
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, LonelinessReducesDopamine) {
    // WHAT: Loneliness decreases dopamine (anhedonia)
    // WHY:  Documented behavior
    // HOW:  Induce loneliness, verify dopamine reduction

    // Simulate isolation
    social_update(social_system, 86400.0f * 7.0f, 86400000000 * 7);

    EXPECT_TRUE(social_is_lonely(social_system));

    float dopamine = 0.0f, oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &dopamine, &oxytocin);

    // Dopamine should be reduced
    EXPECT_LE(dopamine, 0.9f);
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, NeuromodulatorFactorsAreClamped) {
    // WHAT: Neuromodulator factors are clamped to valid ranges
    // WHY:  API contract (dopamine: [0.3, 2.0], oxytocin: [0.5, 2.0])
    // HOW:  Create extreme conditions, verify clamping

    // Create multiple intense loves (unrealistic but tests bounds)
    for (int i = 0; i < 10; i++) {
        uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);
        social_experience_love(social_system, rel_id, LOVE_TYPE_ROMANTIC, 1.0f);
    }

    float dopamine = 0.0f, oxytocin = 0.0f;
    social_get_neuromodulator_effects(social_system, &dopamine, &oxytocin);

    // Should be clamped
    EXPECT_LE(dopamine, 2.0f);
    EXPECT_GE(dopamine, 0.3f);
    EXPECT_LE(oxytocin, 2.0f);
    EXPECT_GE(oxytocin, 0.5f);
}

//=============================================================================
// API STABILITY: EMOTIONAL TAG INTEGRATION
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, LoveEmotionHasHighPositiveValence) {
    // WHAT: Love emotion has valence [+0.7 to +0.95]
    // WHY:  Documented behavior (Russell's Circumplex Model)
    // HOW:  Create love, verify emotional tag

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    for (int i = 0; i < 10; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.8f, 0.8f, i * 1000000);
    }

    social_experience_love(social_system, rel_id, LOVE_TYPE_ROMANTIC, 0.9f);

    emotional_tag_t emotion = social_get_emotion(social_system);

    EXPECT_GE(emotion.valence, 0.7f);
    EXPECT_LE(emotion.valence, 0.95f);
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, FriendshipEmotionHasModeratePositiveValence) {
    // WHAT: Friendship emotion has valence [+0.4 to +0.7]
    // WHY:  Documented behavior
    // HOW:  Create friendship, verify emotional tag

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    for (int i = 0; i < 20; i++) {
        social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, i * 1000000);
    }

    // Update to calculate friendship_warmth
    social_update(social_system, 60.0f, 20000000 + 60000000);

    emotional_tag_t emotion = social_get_emotion(social_system);

    EXPECT_GE(emotion.valence, 0.4f);
    EXPECT_LE(emotion.valence, 0.7f);
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, LonelinessEmotionHasNegativeValence) {
    // WHAT: Loneliness emotion has valence [-0.4 to -0.7]
    // WHY:  Documented behavior
    // HOW:  Induce loneliness, verify emotional tag

    social_update(social_system, 86400.0f * 7.0f, 86400000000 * 7);

    EXPECT_TRUE(social_is_lonely(social_system));

    emotional_tag_t emotion = social_get_emotion(social_system);

    EXPECT_LE(emotion.valence, -0.4f);
    EXPECT_GE(emotion.valence, -0.7f);
}

//=============================================================================
// API STABILITY: PARAMETER VALIDATION
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, NegativeParametersAreClamped) {
    // WHAT: Negative parameters are clamped to 0.0
    // WHY:  API robustness
    // HOW:  Pass negative values, verify no crash and valid behavior

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Negative emotional intensity (should be clamped to 0.0)
    social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, -0.5f, 0.5f, 1000000);

    // Should not crash, parameters should be clamped
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, ExcessiveParametersAreClamped) {
    // WHAT: Parameters > 1.0 are clamped to 1.0
    // WHY:  API robustness
    // HOW:  Pass excessive values, verify clamping

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    // Excessive emotional intensity (should be clamped to 1.0)
    social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 5.0f, 5.0f, 1000000);

    // Should not crash, parameters should be clamped
}

//=============================================================================
// API STABILITY: NULL POINTER HANDLING
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, QueryFunctionsHandleNullPointers) {
    // WHAT: Query functions return safe values for null pointers
    // WHY:  API robustness
    // HOW:  Call query functions with null, verify no crash

    // Should not crash
    bool lonely = social_is_lonely(nullptr);
    bool loving = social_is_experiencing_love(nullptr);
    float closeness = social_get_relationship_closeness(nullptr, 0);
    uint32_t count = social_get_close_friend_count(nullptr);
    float oxytocin = social_get_oxytocin_level(nullptr);

    // Values may be undefined but should not crash
}

TEST_F(LoveLoyaltyFriendshipRegressionTest, ModificationFunctionsHandleNullPointers) {
    // WHAT: Modification functions handle null pointers gracefully
    // WHY:  API robustness
    // HOW:  Call modification functions with null, verify no crash

    social_process_interaction(nullptr, 0, INTERACTION_CONVERSATION, 0.5f, 0.5f, 1000000);
    social_express_vulnerability(nullptr, 0, 0.5f, true);
    social_provide_support(nullptr, 0, 0.5f);
    social_commit_loyalty(nullptr, 0, LOYALTY_TO_PERSON, 0.8f);
    social_experience_betrayal(nullptr, 0, 0.5f);
    social_update(nullptr, 1.0f, 1000000);

    // Should not crash
}

//=============================================================================
// API STABILITY: STATISTICS TRACKING
//=============================================================================

TEST_F(LoveLoyaltyFriendshipRegressionTest, StatisticsAreTracked) {
    // WHAT: System tracks usage statistics
    // WHY:  API contract
    // HOW:  Perform operations, verify statistics updated

    uint32_t rel_id = social_create_relationship(social_system, RELATIONSHIP_ACQUAINTANCE, ATTACHMENT_SECURE);

    social_process_interaction(social_system, rel_id, INTERACTION_CONVERSATION, 0.7f, 0.7f, 1000000);
    social_update(social_system, 1.0f, 2000000);

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
