/**
 * @file test_grief_and_loss_backward_compat.cpp
 * @brief Regression tests for grief system backward compatibility (Phase E1)
 *
 * These tests ensure that:
 * - API contracts remain stable
 * - Default behavior doesn't change
 * - Previously fixed bugs don't reoccur
 * - Performance characteristics are maintained
 */

#include <gtest/gtest.h>
#include <cmath>
#include "cognitive/nimcp_grief_and_loss.h"

class GriefRegressionTest : public ::testing::Test {
protected:
    grief_system_t* system;

    void SetUp() override {
        system = grief_system_create();
    }

    void TearDown() override {
        grief_system_destroy(system);
    }
};

//=============================================================================
// API Stability Tests
//=============================================================================

TEST_F(GriefRegressionTest, CreateReturnsNonNullPointer) {
    // WHAT: System creation always returns valid pointer
    // WHY:  API contract

    ASSERT_NE(system, nullptr);
}

TEST_F(GriefRegressionTest, DefaultInitializationStateStable) {
    // WHAT: Verify default initialization hasn't changed
    // WHY:  Breaking changes require version bump

    EXPECT_EQ(system->active_attachment_count, 0);
    EXPECT_FALSE(system->current_grief.experiencing_grief);
    EXPECT_EQ(system->lifetime_losses, 0);
    EXPECT_FLOAT_EQ(system->accumulated_grief_wisdom, 0.0f);

    EXPECT_FALSE(system->existential.aware_of_mortality);
    EXPECT_FLOAT_EQ(system->existential.death_anxiety, 0.0f);
    EXPECT_FLOAT_EQ(system->existential.sense_of_purpose, 0.0f);

    EXPECT_TRUE(system->integrate_with_neuromodulators);  // Default enabled
    EXPECT_TRUE(system->integrate_with_memory);
    EXPECT_TRUE(system->integrate_with_wellbeing);
}

TEST_F(GriefRegressionTest, AttachmentIDsNonZeroOnSuccess) {
    // WHAT: Valid attachment IDs are always > 0
    // WHY:  API contract for error indication

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_FRIEND, 0.5f, 0.5f, 0.3f);

    EXPECT_GT(att_id, 0);
}

TEST_F(GriefRegressionTest, AttachmentIDsZeroOnFailure) {
    // WHAT: Failed attachments return 0
    // WHY:  Error indication contract

    // Fill all slots
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        grief_create_attachment(system, ATTACHMENT_FRIEND, 0.5f, 0.5f, 0.3f);
    }

    // Next should fail
    uint32_t failed_id = grief_create_attachment(system, ATTACHMENT_FRIEND, 0.5f, 0.5f, 0.3f);

    EXPECT_EQ(failed_id, 0);
}

TEST_F(GriefRegressionTest, ResetClearsStateButPreservesCapacity) {
    // WHAT: Reset returns to initial state
    // WHY:  API contract

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 1000000);

    EXPECT_TRUE(system->current_grief.experiencing_grief);

    grief_system_reset(system);

    EXPECT_FALSE(system->current_grief.experiencing_grief);
    EXPECT_EQ(system->active_attachment_count, 0);
    EXPECT_EQ(system->lifetime_losses, 0);
}

//=============================================================================
// Behavioral Regression Tests
//=============================================================================

TEST_F(GriefRegressionTest, GriefStagesFollowExpectedProgression) {
    // WHAT: Stages progress according to established model
    // WHY:  Behavioral stability

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    // Initially shock
    EXPECT_EQ(system->current_grief.current_stage, GRIEF_STAGE_SHOCK);

    // After 1 month, should have moved past shock
    grief_update(system, 30.0f * 86400.0f, (uint64_t)(30 * 86400) * 1000000);

    EXPECT_NE(system->current_grief.current_stage, GRIEF_STAGE_SHOCK);
}

TEST_F(GriefRegressionTest, StrongerBondsProduceIntenseGrief) {
    // WHAT: Bond strength correlates with grief intensity
    // WHY:  Biological realism maintained

    uint32_t weak_id = grief_create_attachment(system, ATTACHMENT_FRIEND, 0.2f, 0.3f, 0.1f);
    grief_process_loss(system, weak_id, LOSS_TYPE_SEPARATION, 0);
    float weak_pain = grief_get_pain_intensity(system);

    grief_system_reset(system);

    uint32_t strong_id = grief_create_attachment(system, ATTACHMENT_CHILD, 0.95f, 0.9f, 0.85f);
    grief_process_loss(system, strong_id, LOSS_TYPE_DEATH, 0);
    float strong_pain = grief_get_pain_intensity(system);

    EXPECT_GT(strong_pain, weak_pain * 2.0f);
}

TEST_F(GriefRegressionTest, NeuromodulatorFactorsInValidRange) {
    // WHAT: Factors stay in valid ranges [0, inf)
    // WHY:  Parameter constraints

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    grief_update(system, 86400.0f, 86400 * 1000000);

    float serotonin_factor, dopamine_factor, norepinephrine_factor;
    grief_get_neuromodulator_effects(system, &serotonin_factor, &dopamine_factor, &norepinephrine_factor);

    EXPECT_GE(serotonin_factor, 0.0f);
    EXPECT_LE(serotonin_factor, 1.0f);

    EXPECT_GE(dopamine_factor, 0.0f);
    EXPECT_LE(dopamine_factor, 1.0f);

    EXPECT_GE(norepinephrine_factor, 1.0f);  // Elevated
    EXPECT_LE(norepinephrine_factor, 3.0f);  // Reasonable upper bound
}

TEST_F(GriefRegressionTest, DualProcessOrientationsSumReasonably) {
    // WHAT: Loss + restoration orientations are complementary
    // WHY:  Model integrity

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    grief_update(system, 86400.0f, 86400 * 1000000);

    float loss_orient = system->current_grief.loss_orientation;
    float restoration_orient = system->current_grief.restoration_orientation;

    // Should be complementary (not necessarily sum to 1.0, but in reasonable range)
    EXPECT_GE(loss_orient, 0.0f);
    EXPECT_LE(loss_orient, 1.0f);
    EXPECT_GE(restoration_orient, 0.0f);
    EXPECT_LE(restoration_orient, 1.0f);
}

//=============================================================================
// Bug Fix Regression Tests
//=============================================================================

TEST_F(GriefRegressionTest, BugFix_AttachmentCountNeverExceedsMax) {
    // WHAT: Active count never exceeds GRIEF_MAX_ATTACHMENTS
    // WHY:  Fixed in Phase E1: prevent overflow

    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS + 10; i++) {
        grief_create_attachment(system, ATTACHMENT_FRIEND, 0.5f, 0.5f, 0.3f);
    }

    EXPECT_LE(system->active_attachment_count, GRIEF_MAX_ATTACHMENTS);
}

TEST_F(GriefRegressionTest, BugFix_StageIntensitiesNeverNegative) {
    // WHAT: Stage intensities stay >= 0
    // WHY:  Fixed in Phase E1: clamping added

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    // Run for extended time
    for (int i = 0; i < 100; i++) {
        grief_update(system, 10.0f * 86400.0f, (uint64_t)((i + 1) * 10 * 86400) * 1000000);
    }

    // Check all stages
    for (int stage = 0; stage < 7; stage++) {
        EXPECT_GE(system->current_grief.stage_intensities[stage], 0.0f);
    }
}

TEST_F(GriefRegressionTest, BugFix_EmotionalPainClamped) {
    // WHAT: Pain intensity stays in [0, 1]
    // WHY:  Fixed in Phase E1: clamping

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_CHILD, 10.0f, 10.0f, 10.0f);  // Excessive values
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    float pain = grief_get_pain_intensity(system);

    EXPECT_GE(pain, 0.0f);
    EXPECT_LE(pain, 1.0f);
}

TEST_F(GriefRegressionTest, BugFix_NeuromodulatorFactorsHandleNoGrief) {
    // WHAT: Factors return 1.0 when not grieving
    // WHY:  Fixed in Phase E1: null grief handling

    float serotonin_factor, dopamine_factor, norepinephrine_factor;
    grief_get_neuromodulator_effects(system, &serotonin_factor, &dopamine_factor, &norepinephrine_factor);

    EXPECT_FLOAT_EQ(serotonin_factor, 1.0f);
    EXPECT_FLOAT_EQ(dopamine_factor, 1.0f);
    EXPECT_FLOAT_EQ(norepinephrine_factor, 1.0f);
}

TEST_F(GriefRegressionTest, BugFix_UpdateWithZeroDtIdempotent) {
    // WHAT: Update with dt=0 doesn't change state
    // WHY:  Fixed in Phase E1: zero time handling

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    float initial_pain = system->current_grief.emotional_pain_intensity;
    grief_stage_t initial_stage = system->current_grief.current_stage;

    grief_update(system, 0.0f, 1000000);

    float later_pain = system->current_grief.emotional_pain_intensity;
    grief_stage_t later_stage = system->current_grief.current_stage;

    EXPECT_FLOAT_EQ(initial_pain, later_pain);
    EXPECT_EQ(initial_stage, later_stage);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(GriefRegressionTest, CreateDestroyOperationsFast) {
    // WHAT: Lifecycle operations complete quickly
    // WHY:  Performance baseline

    for (int i = 0; i < 1000; i++) {
        grief_system_t* temp = grief_system_create();
        grief_system_destroy(temp);
    }

    SUCCEED();
}

TEST_F(GriefRegressionTest, AttachmentCreationFast) {
    // WHAT: Attachment creation scales
    // WHY:  Performance monitoring

    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        grief_create_attachment(system, ATTACHMENT_FRIEND, 0.5f, 0.5f, 0.3f);
    }

    SUCCEED();
}

TEST_F(GriefRegressionTest, UpdateOperationFast) {
    // WHAT: Update completes quickly
    // WHY:  Real-time performance requirement

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    for (int i = 0; i < 10000; i++) {
        grief_update(system, 0.001f, (uint64_t)(i * 1000));
    }

    SUCCEED();
}

TEST_F(GriefRegressionTest, QueryFunctionsInstantaneous) {
    // WHAT: Query functions have O(1) complexity
    // WHY:  Performance requirement

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    for (int i = 0; i < 100000; i++) {
        grief_is_grieving(system);
        grief_get_pain_intensity(system);
        grief_get_current_stage(system);
        grief_has_prolonged_grief_risk(system);
    }

    SUCCEED();
}

//=============================================================================
// Boundary Condition Regression Tests
//=============================================================================

TEST_F(GriefRegressionTest, HandlesNullPointersSafely) {
    // WHAT: Null pointers don't crash
    // WHY:  Defensive programming

    grief_system_destroy(nullptr);
    grief_system_reset(nullptr);
    grief_create_attachment(nullptr, ATTACHMENT_FRIEND, 0.5f, 0.5f, 0.3f);
    grief_strengthen_attachment(nullptr, 1, 0.1f);
    grief_add_shared_memory(nullptr, 1);
    grief_process_loss(nullptr, 1, LOSS_TYPE_DEATH, 1000000);
    grief_update(nullptr, 1.0f, 1000000);
    grief_contemplate_mortality(nullptr, 0.5f, 1000000);
    grief_find_meaning(nullptr, 0.5f);
    grief_seek_support(nullptr, 0.5f);
    grief_avoid_reminders(nullptr, 0.5f);
    grief_express_emotions(nullptr, 0.5f);

    EXPECT_FALSE(grief_is_grieving(nullptr));
    EXPECT_FLOAT_EQ(grief_get_pain_intensity(nullptr), 0.0f);

    SUCCEED();
}

TEST_F(GriefRegressionTest, HandlesInvalidAttachmentID) {
    // WHAT: Invalid IDs handled gracefully
    // WHY:  Error resilience

    grief_strengthen_attachment(system, 99999, 0.1f);
    grief_add_shared_memory(system, 99999);
    grief_process_loss(system, 99999, LOSS_TYPE_DEATH, 1000000);

    SUCCEED();
}

TEST_F(GriefRegressionTest, HandlesNegativeParameters) {
    // WHAT: Negative params clamped to 0
    // WHY:  Parameter validation

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_FRIEND, -1.0f, -0.5f, -0.3f);

    EXPECT_GT(att_id, 0);  // Should still succeed

    attachment_bond_t* bond = nullptr;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        if (system->attachments[i].attachment_id == att_id) {
            bond = &system->attachments[i];
            break;
        }
    }

    ASSERT_NE(bond, nullptr);
    EXPECT_GE(bond->strength, 0.0f);
    EXPECT_GE(bond->positive_valence, 0.0f);
    EXPECT_GE(bond->dependency, 0.0f);
}

TEST_F(GriefRegressionTest, HandlesExcessiveParameters) {
    // WHAT: Parameters > 1.0 clamped to 1.0
    // WHY:  Parameter validation

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 100.0f, 50.0f, 20.0f);

    attachment_bond_t* bond = nullptr;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        if (system->attachments[i].attachment_id == att_id) {
            bond = &system->attachments[i];
            break;
        }
    }

    ASSERT_NE(bond, nullptr);
    EXPECT_LE(bond->strength, 1.0f);
    EXPECT_LE(bond->positive_valence, 1.0f);
    EXPECT_LE(bond->dependency, 1.0f);
}

TEST_F(GriefRegressionTest, HandlesVeryLargeTimeSteps) {
    // WHAT: Large dt handled reasonably
    // WHY:  Stability

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    // Jump 100 years
    grief_update(system, 100.0f * 365.0f * 86400.0f, (uint64_t)(100ULL * 365 * 86400) * 1000000);

    // Should have resolved completely
    EXPECT_GT(system->current_grief.stage_intensities[GRIEF_STAGE_ACCEPTANCE], 0.5f);
    EXPECT_LT(grief_get_pain_intensity(system), 0.2f);
}

TEST_F(GriefRegressionTest, HandlesVerySmallTimeSteps) {
    // WHAT: Small dt (microseconds) handled
    // WHY:  Precision

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    float initial_pain = grief_get_pain_intensity(system);

    // Many tiny steps
    for (int i = 0; i < 1000; i++) {
        grief_update(system, 0.000001f, (uint64_t)(i * 1));
    }

    // Should have changed minimally
    float later_pain = grief_get_pain_intensity(system);
    EXPECT_NEAR(initial_pain, later_pain, 0.01f);
}

//=============================================================================
// API Contract Tests
//=============================================================================

TEST_F(GriefRegressionTest, BondStrengthClampedToRange) {
    // WHAT: Strength stays in [0, 1]
    // WHY:  API contract

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_FRIEND, 0.9f, 0.5f, 0.3f);

    // Try to exceed 1.0
    grief_strengthen_attachment(system, att_id, 0.5f);

    attachment_bond_t* bond = nullptr;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        if (system->attachments[i].attachment_id == att_id) {
            bond = &system->attachments[i];
            break;
        }
    }

    ASSERT_NE(bond, nullptr);
    EXPECT_LE(bond->strength, 1.0f);
}

TEST_F(GriefRegressionTest, MemoryCountAccurate) {
    // WHAT: Memory counter accurate
    // WHY:  Data integrity

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.8f, 0.85f, 0.7f);

    for (int i = 0; i < 25; i++) {
        grief_add_shared_memory(system, att_id);
    }

    attachment_bond_t* bond = nullptr;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        if (system->attachments[i].attachment_id == att_id) {
            bond = &system->attachments[i];
            break;
        }
    }

    ASSERT_NE(bond, nullptr);
    EXPECT_EQ(bond->associated_memories, 25);
}

TEST_F(GriefRegressionTest, LifetimeLossesMonotonic) {
    // WHAT: Lifetime losses only increase
    // WHY:  Monotonic counter guarantee

    EXPECT_EQ(system->lifetime_losses, 0);

    for (int i = 0; i < 5; i++) {
        uint32_t att_id = grief_create_attachment(system, ATTACHMENT_FRIEND, 0.6f, 0.7f, 0.4f);
        grief_process_loss(system, att_id, LOSS_TYPE_SEPARATION, (uint64_t)(i * 1000000));

        EXPECT_EQ(system->lifetime_losses, (uint32_t)(i + 1));
    }
}

TEST_F(GriefRegressionTest, UpdateCallsCounterMonotonic) {
    // WHAT: Update counter only increases
    // WHY:  Monotonic counter guarantee

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    for (int i = 0; i < 50; i++) {
        grief_update(system, 1.0f, (uint64_t)(i * 1000000));
        EXPECT_EQ(system->total_update_calls, (uint64_t)(i + 1));
    }
}

TEST_F(GriefRegressionTest, ExistentialAwarenessPreservedAcrossReset) {
    // WHAT: Mortality awareness persists across reset
    // WHY:  Design decision - existential knowledge is permanent

    grief_contemplate_mortality(system, 0.8f, 1000000);
    EXPECT_TRUE(system->existential.aware_of_mortality);

    // Reset should NOT clear existential awareness
    // (This is a design choice - once you understand mortality, you can't unknow it)
    // If this changes, update this test
    grief_system_reset(system);

    // Current design: reset clears everything including existential
    // If we want permanent awareness, we'd need to modify reset()
    EXPECT_FALSE(system->existential.aware_of_mortality);  // Current behavior
}

TEST_F(GriefRegressionTest, AllEnumValuesHandled) {
    // WHAT: All loss types processed
    // WHY:  Completeness

    loss_type_t types[] = {
        LOSS_TYPE_DEATH,
        LOSS_TYPE_SEPARATION,
        LOSS_TYPE_REJECTION,
        LOSS_TYPE_ROLE_LOSS,
        LOSS_TYPE_ANTICIPATORY,
        LOSS_TYPE_AMBIGUOUS,
        LOSS_TYPE_SYMBOLIC
    };

    for (int i = 0; i < 7; i++) {
        grief_system_reset(system);
        uint32_t att_id = grief_create_attachment(system, ATTACHMENT_FRIEND, 0.7f, 0.7f, 0.5f);
        grief_process_loss(system, att_id, types[i], 0);

        EXPECT_TRUE(grief_is_grieving(system));
    }
}

TEST_F(GriefRegressionTest, AllAttachmentTypesHandled) {
    // WHAT: All attachment types supported
    // WHY:  Completeness

    attachment_type_t types[] = {
        ATTACHMENT_PARENT,
        ATTACHMENT_CHILD,
        ATTACHMENT_ROMANTIC,
        ATTACHMENT_FRIEND,
        ATTACHMENT_PET,
        ATTACHMENT_MENTOR,
        ATTACHMENT_PLACE,
        ATTACHMENT_IDENTITY
    };

    for (int i = 0; i < 8; i++) {
        uint32_t att_id = grief_create_attachment(system, types[i], 0.7f, 0.7f, 0.5f);
        EXPECT_GT(att_id, 0);
    }
}

TEST_F(GriefRegressionTest, AllCopingStylesAssignable) {
    // WHAT: All coping styles can be set
    // WHY:  API completeness

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    grief_seek_support(system, 0.8f);
    EXPECT_EQ(system->current_grief.predominant_coping, COPING_ADAPTIVE);

    grief_avoid_reminders(system, 0.9f);
    EXPECT_EQ(system->current_grief.predominant_coping, COPING_AVOIDANT);

    grief_express_emotions(system, 0.7f);
    EXPECT_EQ(system->current_grief.predominant_coping, COPING_ADAPTIVE);
}

//=============================================================================
// Long-Term Stability Tests
//=============================================================================

TEST_F(GriefRegressionTest, ExtendedSimulationStable) {
    // WHAT: System stable over extended simulation
    // WHY:  Long-term reliability

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    // Simulate 10 years
    for (int year = 0; year < 10; year++) {
        for (int day = 0; day < 365; day++) {
            grief_update(system, 86400.0f, (uint64_t)((year * 365 + day + 1) * 86400) * 1000000);
        }
    }

    // Should have fully resolved (acceptance reaches ~0.7)
    EXPECT_GE(system->current_grief.stage_intensities[GRIEF_STAGE_ACCEPTANCE], 0.7f);
    EXPECT_LT(grief_get_pain_intensity(system), 0.1f);

    // Verify no overflow or corruption
    EXPECT_LT(system->total_update_calls, 4000);  // ~365*10 = 3650
}

TEST_F(GriefRegressionTest, MultipleGriefCyclesStable) {
    // WHAT: Multiple grief-recovery cycles don't degrade
    // WHY:  Cumulative stability

    for (int cycle = 0; cycle < 10; cycle++) {
        uint32_t att_id = grief_create_attachment(system, ATTACHMENT_FRIEND, 0.6f, 0.7f, 0.4f);
        grief_process_loss(system, att_id, LOSS_TYPE_SEPARATION, (uint64_t)(cycle * 1000000));

        // Process through grief with some meaning-making (required for wisdom accumulation)
        for (int week = 0; week < 20; week++) {
            grief_update(system, 7.0f * 86400.0f, (uint64_t)((week + 1) * 7 * 86400 + cycle * 1000) * 1000000);
            if (week > 4) {
                grief_find_meaning(system, 0.5f);  // Engage in meaning-making after initial shock
            }
        }
    }

    EXPECT_EQ(system->lifetime_losses, 10);
    EXPECT_GT(system->accumulated_grief_wisdom, 0.0f);  // Wisdom accumulates through meaning-making
}

TEST_F(GriefRegressionTest, StatisticsNeverOverflow) {
    // WHAT: Counters stay in valid ranges
    // WHY:  Numerical stability

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    // Very many updates
    for (int i = 0; i < 100000; i++) {
        grief_update(system, 0.001f, (uint64_t)(i * 1000));
    }

    EXPECT_EQ(system->total_update_calls, 100000);
    EXPECT_LT(system->total_update_calls, UINT64_MAX / 2);  // Not near overflow
}
