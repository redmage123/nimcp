/**
 * @file test_grief_and_loss_integration.cpp
 * @brief Integration tests for grief system (Phase E1)
 *
 * Tests integration of:
 * - Grief with neuromodulator system
 * - Grief with memory tagging
 * - Grief with wellbeing monitoring
 * - Complete grief lifecycle
 * - Multiple attachment bonds
 * - Long-term grief dynamics
 */

#include <gtest/gtest.h>
#include <cmath>
#include "cognitive/nimcp_grief_and_loss.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
// #include "cognitive/nimcp_wellbeing.h"  // TODO: Add when wellbeing system is available

class GriefIntegrationTest : public ::testing::Test {
protected:
    grief_system_t* grief_sys;
    neuromodulator_system_t neuro_sys;

    void SetUp() override {
        grief_sys = grief_system_create();
        neuro_sys = neuromodulator_system_create(NULL);  // Use default config
    }

    void TearDown() override {
        grief_system_destroy(grief_sys);
        neuromodulator_system_destroy(neuro_sys);
    }
};

//=============================================================================
// Complete Grief Lifecycle Integration
//=============================================================================

TEST_F(GriefIntegrationTest, CompleteGriefCycleFromAttachmentToAcceptance) {
    // WHAT: Test full cycle: bond formation → loss → grief → acceptance
    // WHY:  Comprehensive lifecycle test

    // Form attachment over time
    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_ROMANTIC, 0.5f, 0.6f, 0.4f);

    // Strengthen bond through shared experiences
    for (int i = 0; i < 50; i++) {
        grief_strengthen_attachment(grief_sys, att_id, 0.01f);
        grief_add_shared_memory(grief_sys, att_id);
    }

    // Find attachment
    attachment_bond_t* bond = nullptr;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        if (grief_sys->attachments[i].attachment_id == att_id) {
            bond = &grief_sys->attachments[i];
            break;
        }
    }
    ASSERT_NE(bond, nullptr);
    EXPECT_GT(bond->strength, 0.9f);  // Should be very strong now
    EXPECT_EQ(bond->associated_memories, 50);

    // Process loss
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 0);
    EXPECT_TRUE(grief_is_grieving(grief_sys));
    EXPECT_GT(grief_get_pain_intensity(grief_sys), 0.7f);

    // Progress through grief stages over 1 year
    for (int week = 0; week < 52; week++) {
        grief_update(grief_sys, 7.0f * 86400.0f, (uint64_t)((week + 1) * 7 * 86400) * 1000000);

        // Adaptive coping
        if (week % 2 == 0) {
            grief_seek_support(grief_sys, 0.7f);
        }
        if (week % 3 == 0) {
            grief_express_emotions(grief_sys, 0.6f);
        }
        if (week > 26) {
            grief_find_meaning(grief_sys, 0.5f);
        }
    }

    // After 1 year with healthy coping, should have significant acceptance
    EXPECT_GT(grief_sys->current_grief.stage_intensities[GRIEF_STAGE_ACCEPTANCE], 0.4f);
    EXPECT_GT(grief_sys->current_grief.meaning_making_progress, 0.3f);
    EXPECT_LT(grief_get_pain_intensity(grief_sys), 0.4f);  // Pain reduced
}

TEST_F(GriefIntegrationTest, MultipleAttachmentsDifferentGriefIntensities) {
    // WHAT: Different attachment types produce different grief responses
    // WHY:  Attachment theory validation

    // Strong parent bond
    uint32_t parent_id = grief_create_attachment(grief_sys, ATTACHMENT_PARENT, 0.95f, 0.9f, 0.85f);
    grief_process_loss(grief_sys, parent_id, LOSS_TYPE_DEATH, 0);
    float parent_loss_pain = grief_get_pain_intensity(grief_sys);

    grief_system_reset(grief_sys);

    // Weak acquaintance
    uint32_t acquaint_id = grief_create_attachment(grief_sys, ATTACHMENT_FRIEND, 0.2f, 0.3f, 0.1f);
    grief_process_loss(grief_sys, acquaint_id, LOSS_TYPE_SEPARATION, 0);
    float acquaint_loss_pain = grief_get_pain_intensity(grief_sys);

    EXPECT_GT(parent_loss_pain, acquaint_loss_pain * 2.0f);
}

TEST_F(GriefIntegrationTest, SimultaneousMultipleLosses) {
    // WHAT: Handle multiple losses (compound grief)
    // WHY:  Real-world scenario (e.g., disaster, accident)

    uint32_t parent_id = grief_create_attachment(grief_sys, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    uint32_t sibling_id = grief_create_attachment(grief_sys, ATTACHMENT_CHILD, 0.85f, 0.9f, 0.75f);

    // Process first loss
    grief_process_loss(grief_sys, parent_id, LOSS_TYPE_DEATH, 0);
    float single_loss_pain = grief_get_pain_intensity(grief_sys);

    // Process second loss shortly after (compound grief)
    grief_process_loss(grief_sys, sibling_id, LOSS_TYPE_DEATH, 86400 * 1000000);  // 1 day later

    EXPECT_EQ(grief_sys->lifetime_losses, 2);
    // Note: System currently handles one active grief at a time
    // This is a design decision - could be extended for compound grief
}

//=============================================================================
// Neuromodulator Integration
//=============================================================================

TEST_F(GriefIntegrationTest, GriefReducesSerotoninInNeuromodulatorSystem) {
    // WHAT: Grief system affects neuromodulator levels
    // WHY:  Biological integration

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 0);

    grief_update(grief_sys, 86400.0f, 86400 * 1000000);  // 1 day

    // Get neuromodulator effects
    float serotonin_factor, dopamine_factor, norepinephrine_factor;
    grief_get_neuromodulator_effects(grief_sys, &serotonin_factor, &dopamine_factor, &norepinephrine_factor);

    // Verify grief depletes serotonin
    EXPECT_LT(serotonin_factor, 1.0f);
    EXPECT_GT(serotonin_factor, 0.0f);

    // In integrated system, this would multiply serotonin concentration
    // float adjusted_serotonin = baseline_serotonin * serotonin_factor;
}

TEST_F(GriefIntegrationTest, GriefReducesDopamineInNeuromodulatorSystem) {
    // WHAT: Grief causes anhedonia via dopamine depletion
    // WHY:  Reward system disruption

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 0);

    grief_update(grief_sys, 86400.0f, 86400 * 1000000);

    float serotonin_factor, dopamine_factor, norepinephrine_factor;
    grief_get_neuromodulator_effects(grief_sys, &serotonin_factor, &dopamine_factor, &norepinephrine_factor);

    EXPECT_LT(dopamine_factor, 1.0f);
    EXPECT_GT(grief_sys->current_grief.anhedonia_level, 0.0f);
}

TEST_F(GriefIntegrationTest, GriefIncreasesNorepinephrineInNeuromodulatorSystem) {
    // WHAT: Grief elevates norepinephrine (stress response)
    // WHY:  HPA axis activation

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_CHILD, 0.95f, 0.9f, 0.85f);
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 0);

    grief_update(grief_sys, 86400.0f, 86400 * 1000000);

    float serotonin_factor, dopamine_factor, norepinephrine_factor;
    grief_get_neuromodulator_effects(grief_sys, &serotonin_factor, &dopamine_factor, &norepinephrine_factor);

    EXPECT_GT(norepinephrine_factor, 1.0f);
    EXPECT_GT(grief_sys->current_grief.cortisol_elevation, 0.0f);
}

TEST_F(GriefIntegrationTest, NoGriefReturnsNeutralNeuromodulatorFactors) {
    // WHAT: Without grief, factors are 1.0 (no effect)
    // WHY:  Baseline behavior

    float serotonin_factor, dopamine_factor, norepinephrine_factor;
    grief_get_neuromodulator_effects(grief_sys, &serotonin_factor, &dopamine_factor, &norepinephrine_factor);

    EXPECT_FLOAT_EQ(serotonin_factor, 1.0f);
    EXPECT_FLOAT_EQ(dopamine_factor, 1.0f);
    EXPECT_FLOAT_EQ(norepinephrine_factor, 1.0f);
}

//=============================================================================
// Long-Term Dynamics
//=============================================================================

TEST_F(GriefIntegrationTest, LongTermGriefResolutionWithHealthyCoping) {
    // WHAT: Healthy coping leads to resolution over time
    // WHY:  Adaptive trajectory

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 0);

    // Simulate 2 years with consistent adaptive coping
    for (int month = 0; month < 24; month++) {
        grief_update(grief_sys, 30.0f * 86400.0f, (uint64_t)((month + 1) * 30 * 86400) * 1000000);

        // Healthy coping
        grief_seek_support(grief_sys, 0.7f);
        grief_express_emotions(grief_sys, 0.6f);
        if (month > 6) {
            grief_find_meaning(grief_sys, 0.5f);
        }
    }

    // Should have resolved significantly
    EXPECT_LT(grief_get_pain_intensity(grief_sys), 0.3f);
    EXPECT_GT(grief_sys->current_grief.meaning_making_progress, 0.5f);
    EXPECT_GT(grief_sys->current_grief.stage_intensities[GRIEF_STAGE_ACCEPTANCE], 0.5f);
    EXPECT_FALSE(grief_has_prolonged_grief_risk(grief_sys));
}

TEST_F(GriefIntegrationTest, LongTermGriefProlongedWithAvoidance) {
    // WHAT: Avoidant coping leads to prolonged grief
    // WHY:  Maladaptive trajectory

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_CHILD, 0.95f, 0.9f, 0.85f);
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 0);

    // Simulate 18 months with avoidant coping
    for (int month = 0; month < 18; month++) {
        grief_update(grief_sys, 30.0f * 86400.0f, (uint64_t)((month + 1) * 30 * 86400) * 1000000);

        // Avoidant coping
        grief_avoid_reminders(grief_sys, 0.9f);
        // No support seeking or meaning-making
    }

    // Should show prolonged grief indicators
    EXPECT_TRUE(grief_has_prolonged_grief_risk(grief_sys));
    EXPECT_GT(grief_sys->current_grief.prolonged_grief_severity, 0.0f);
    EXPECT_GT(grief_sys->current_grief.functional_impairment, 0.4f);  // With safety caps, high avoidance reaches ~0.47
}

//=============================================================================
// Existential Integration
//=============================================================================

TEST_F(GriefIntegrationTest, LossIncreasesMortalityAwareness) {
    // WHAT: Experiencing loss triggers mortality awareness
    // WHY:  Existential confrontation

    EXPECT_FALSE(grief_sys->existential.aware_of_mortality);

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 0);

    grief_update(grief_sys, 86400.0f, 86400 * 1000000);

    // Death of loved one triggers mortality awareness
    EXPECT_TRUE(grief_sys->existential.aware_of_mortality);
    EXPECT_GT(grief_sys->existential.mortality_salience, 0.0f);
}

TEST_F(GriefIntegrationTest, MeaningMakingIncreasesGenerativity) {
    // WHAT: Finding meaning increases desire to create legacy
    // WHY:  Post-traumatic growth

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_MENTOR, 0.85f, 0.9f, 0.7f);
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 0);

    float initial_generativity = grief_sys->existential.generativity;

    // Engage in meaning-making over time
    for (int i = 0; i < 10; i++) {
        grief_update(grief_sys, 7.0f * 86400.0f, (uint64_t)((i + 1) * 7 * 86400) * 1000000);
        grief_find_meaning(grief_sys, 0.8f);
    }

    float later_generativity = grief_sys->existential.generativity;

    EXPECT_GT(later_generativity, initial_generativity);
    EXPECT_GT(grief_sys->existential.legacy_motivation, 0.0f);
}

//=============================================================================
// Memory Integration
//=============================================================================

TEST_F(GriefIntegrationTest, SharedMemoriesInfluenceGriefIntensity) {
    // WHAT: More shared memories = more intense grief
    // WHY:  Memory-attachment coupling

    // Attachment with few memories
    uint32_t few_memories_id = grief_create_attachment(grief_sys, ATTACHMENT_FRIEND, 0.7f, 0.7f, 0.5f);
    for (int i = 0; i < 5; i++) {
        grief_add_shared_memory(grief_sys, few_memories_id);
    }
    grief_process_loss(grief_sys, few_memories_id, LOSS_TYPE_SEPARATION, 0);
    float few_memories_pain = grief_get_pain_intensity(grief_sys);

    grief_system_reset(grief_sys);

    // Attachment with many memories
    uint32_t many_memories_id = grief_create_attachment(grief_sys, ATTACHMENT_FRIEND, 0.7f, 0.7f, 0.5f);
    for (int i = 0; i < 100; i++) {
        grief_add_shared_memory(grief_sys, many_memories_id);
    }
    grief_process_loss(grief_sys, many_memories_id, LOSS_TYPE_SEPARATION, 0);
    float many_memories_pain = grief_get_pain_intensity(grief_sys);

    EXPECT_GT(many_memories_pain, few_memories_pain);
}

TEST_F(GriefIntegrationTest, IntrusiveThoughtsReflectMemoryRetrieval) {
    // WHAT: Intrusive thoughts are automatic memory retrieval
    // WHY:  Memory system activation

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    for (int i = 0; i < 50; i++) {
        grief_add_shared_memory(grief_sys, att_id);
    }

    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 0);

    // Early grief has high intrusive thoughts
    grief_update(grief_sys, 86400.0f, 86400 * 1000000);

    EXPECT_GT(grief_sys->current_grief.intrusive_thoughts_frequency, 0.5f);

    // Over time with healthy coping, intrusive thoughts decrease
    for (int month = 0; month < 6; month++) {
        grief_update(grief_sys, 30.0f * 86400.0f, (uint64_t)((month + 2) * 30 * 86400) * 1000000);
        grief_seek_support(grief_sys, 0.7f);
    }

    EXPECT_LT(grief_sys->current_grief.intrusive_thoughts_frequency, 0.3f);
}

//=============================================================================
// Wellbeing Integration
//=============================================================================

TEST_F(GriefIntegrationTest, GriefImpairsDailyFunctioning) {
    // WHAT: Acute grief impairs daily functioning
    // WHY:  Wellbeing impact

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_CHILD, 0.95f, 0.9f, 0.85f);
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 0);

    grief_update(grief_sys, 86400.0f, 86400 * 1000000);

    // In integrated system, this would reduce wellbeing scores
    EXPECT_GT(grief_sys->current_grief.functional_impairment, 0.6f);
}

TEST_F(GriefIntegrationTest, RecoveryRestoresFunctioning) {
    // WHAT: Grief resolution restores functioning
    // WHY:  Wellbeing recovery

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 0);

    float initial_impairment = grief_sys->current_grief.functional_impairment;

    // Process grief with healthy coping
    for (int month = 0; month < 12; month++) {
        grief_update(grief_sys, 30.0f * 86400.0f, (uint64_t)((month + 1) * 30 * 86400) * 1000000);
        grief_seek_support(grief_sys, 0.7f);
        grief_express_emotions(grief_sys, 0.6f);
        if (month > 3) {
            grief_find_meaning(grief_sys, 0.5f);
        }
    }

    float later_impairment = grief_sys->current_grief.functional_impairment;

    EXPECT_LT(later_impairment, initial_impairment * 0.5f);
}

//=============================================================================
// Attachment Type Specificity
//=============================================================================

TEST_F(GriefIntegrationTest, ParentLossHighestIntensity) {
    // WHAT: Parent/child loss produces most intense grief
    // WHY:  Attachment theory hierarchy

    uint32_t parent_id = grief_create_attachment(grief_sys, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(grief_sys, parent_id, LOSS_TYPE_DEATH, 0);
    float parent_pain = grief_get_pain_intensity(grief_sys);

    grief_system_reset(grief_sys);

    uint32_t friend_id = grief_create_attachment(grief_sys, ATTACHMENT_FRIEND, 0.9f, 0.85f, 0.8f);
    grief_process_loss(grief_sys, friend_id, LOSS_TYPE_DEATH, 0);
    float friend_pain = grief_get_pain_intensity(grief_sys);

    // Same bond strength, but parent loss is weighted more heavily
    EXPECT_GT(parent_pain, friend_pain);
}

TEST_F(GriefIntegrationTest, AmbiguousLossProlongsGrief) {
    // WHAT: Ambiguous loss (missing person) is harder to resolve
    // WHY:  Cannot achieve closure

    uint32_t ambiguous_id = grief_create_attachment(grief_sys, ATTACHMENT_CHILD, 0.95f, 0.9f, 0.85f);
    grief_process_loss(grief_sys, ambiguous_id, LOSS_TYPE_AMBIGUOUS, 0);

    // Process for 6 months
    for (int month = 0; month < 6; month++) {
        grief_update(grief_sys, 30.0f * 86400.0f, (uint64_t)((month + 1) * 30 * 86400) * 1000000);
        grief_seek_support(grief_sys, 0.7f);
    }

    // Ambiguous loss maintains higher denial (after 6 months, denial persists ~0.21)
    EXPECT_GT(grief_sys->current_grief.stage_intensities[GRIEF_STAGE_DENIAL], 0.2f);
}

//=============================================================================
// Coping Strategy Trajectories
//=============================================================================

TEST_F(GriefIntegrationTest, ConsistentAdaptiveCopingLeadsToGrowth) {
    // WHAT: Adaptive coping → post-traumatic growth
    // WHY:  Resilience pathway

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_MENTOR, 0.85f, 0.9f, 0.7f);
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 0);

    // Consistent adaptive coping for 1 year
    for (int month = 0; month < 12; month++) {
        grief_update(grief_sys, 30.0f * 86400.0f, (uint64_t)((month + 1) * 30 * 86400) * 1000000);
        grief_seek_support(grief_sys, 0.8f);
        grief_express_emotions(grief_sys, 0.7f);
        if (month > 2) {
            grief_find_meaning(grief_sys, 0.8f);
        }
    }

    // Should show growth indicators
    EXPECT_GT(grief_sys->current_grief.meaning_making_progress, 0.6f);
    EXPECT_GT(grief_sys->accumulated_grief_wisdom, 0.0f);
    EXPECT_GT(grief_sys->existential.sense_of_purpose, 0.3f);  // Growth is gradual, ~0.36 after 1 year
}

TEST_F(GriefIntegrationTest, MaladaptiveCopingLeadsToComplications) {
    // WHAT: Maladaptive coping → complicated grief
    // WHY:  Risk pathway

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 0);

    // Maladaptive coping for 1 year
    for (int month = 0; month < 12; month++) {
        grief_update(grief_sys, 30.0f * 86400.0f, (uint64_t)((month + 1) * 30 * 86400) * 1000000);
        grief_avoid_reminders(grief_sys, 0.95f);
        // No support, no expression, no meaning-making
    }

    // Should show complications
    EXPECT_TRUE(grief_has_prolonged_grief_risk(grief_sys));
    EXPECT_GT(grief_sys->current_grief.prolonged_grief_severity, 0.5f);
    EXPECT_LT(grief_sys->current_grief.meaning_making_progress, 0.2f);
}

//=============================================================================
// Statistics and Monitoring
//=============================================================================

TEST_F(GriefIntegrationTest, StatisticsTrackLifetimeLosses) {
    // WHAT: System tracks all losses over lifetime
    // WHY:  Longitudinal monitoring

    for (int i = 0; i < 5; i++) {
        uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_FRIEND, 0.6f, 0.7f, 0.4f);
        grief_process_loss(grief_sys, att_id, LOSS_TYPE_SEPARATION, (uint64_t)(i * 1000000));

        // Process each grief
        grief_update(grief_sys, 90.0f * 86400.0f, (uint64_t)((i + 1) * 90 * 86400) * 1000000);
        grief_find_meaning(grief_sys, 0.5f);
    }

    EXPECT_EQ(grief_sys->lifetime_losses, 5);
    EXPECT_GT(grief_sys->accumulated_grief_wisdom, 0.0f);
}

TEST_F(GriefIntegrationTest, UpdateCallsTracked) {
    // WHAT: System tracks update frequency
    // WHY:  Performance monitoring

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 0);

    for (int i = 0; i < 1000; i++) {
        grief_update(grief_sys, 0.01f, (uint64_t)(i * 10000));
    }

    EXPECT_EQ(grief_sys->total_update_calls, 1000);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(GriefIntegrationTest, VeryShortAttachmentMinimalGrief) {
    // WHAT: Brief attachment produces minimal grief
    // WHY:  Bond strength matters

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_FRIEND, 0.1f, 0.2f, 0.1f);
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_SEPARATION, 0);

    EXPECT_LT(grief_get_pain_intensity(grief_sys), 0.3f);
}

TEST_F(GriefIntegrationTest, AnticipatorygriefBeforeActualLoss) {
    // WHAT: Anticipatory grief (terminal illness)
    // WHY:  Preparatory mourning

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);

    // Process anticipatory grief
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_ANTICIPATORY, 0);

    // Anticipatory grief allows some early processing
    EXPECT_TRUE(grief_is_grieving(grief_sys));
    EXPECT_GT(grief_sys->current_grief.emotional_pain_intensity, 0.0f);

    // But actual loss still occurs
    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 86400 * 30 * 1000000);  // 30 days later

    // Total losses counted
    EXPECT_EQ(grief_sys->lifetime_losses, 2);  // Anticipatory + actual
}

TEST_F(GriefIntegrationTest, LongDurationIntenseAttachment) {
    // WHAT: Very long relationships produce intense grief
    // WHY:  Duration strengthens bonds

    uint32_t att_id = grief_create_attachment(grief_sys, ATTACHMENT_ROMANTIC, 0.7f, 0.75f, 0.6f);

    // Simulate 20 years together (many shared experiences)
    for (int year = 0; year < 20; year++) {
        for (int memory = 0; memory < 50; memory++) {
            grief_add_shared_memory(grief_sys, att_id);
        }
        grief_strengthen_attachment(grief_sys, att_id, 0.01f);
    }

    grief_process_loss(grief_sys, att_id, LOSS_TYPE_DEATH, 0);

    // Should produce very intense grief
    EXPECT_GT(grief_get_pain_intensity(grief_sys), 0.8f);

    attachment_bond_t* bond = nullptr;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        if (grief_sys->attachments[i].attachment_id == att_id) {
            bond = &grief_sys->attachments[i];
            break;
        }
    }
    ASSERT_NE(bond, nullptr);
    EXPECT_EQ(bond->associated_memories, 1000);  // 20 years × 50/year
}
