/**
 * @file test_grief_and_loss.cpp
 * @brief Unit tests for grief and loss understanding system (Phase E1)
 *
 * Test coverage:
 * - Initialization and lifecycle
 * - Attachment bond creation and management
 * - Loss processing and grief initiation
 * - Grief stage progression dynamics
 * - Dual Process Model oscillation
 * - Neurobiological changes
 * - Coping mechanisms
 * - Existential awareness
 * - Query functions
 * - Edge cases and error handling
 */

#include <gtest/gtest.h>
#include <cmath>
#include "cognitive/nimcp_grief_and_loss.h"
#include "utils/nimcp_test_base.h"

class GriefSystemTest : public NimcpTestBase {
protected:
    grief_system_t* system;

    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent FIRST
        system = grief_system_create();
    }

    void TearDown() override {
        grief_system_destroy(system);
        NimcpTestBase::TearDown();  // Call parent LAST
    }
};

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(GriefSystemTest, CreateInitializesValidSystem) {
    // WHAT: Verify system creation allocates and initializes correctly
    // WHY:  Lifecycle baseline test

    ASSERT_NE(system, nullptr);
    EXPECT_EQ(system->active_attachment_count, 0);
    EXPECT_FALSE(system->current_grief.experiencing_grief);
    EXPECT_EQ(system->lifetime_losses, 0);
    EXPECT_FLOAT_EQ(system->accumulated_grief_wisdom, 0.0f);
    EXPECT_FALSE(system->existential.aware_of_mortality);
    EXPECT_EQ(system->total_update_calls, 0);
}

TEST_F(GriefSystemTest, ResetClearsState) {
    // WHAT: Reset clears dynamic state but preserves config
    // WHY:  API contract test

    // Create attachment and process loss
    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.8f, 0.7f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 1000000);

    EXPECT_TRUE(system->current_grief.experiencing_grief);
    EXPECT_GT(system->lifetime_losses, 0);

    // Reset
    grief_system_reset(system);

    EXPECT_EQ(system->active_attachment_count, 0);
    EXPECT_FALSE(system->current_grief.experiencing_grief);
    EXPECT_EQ(system->lifetime_losses, 0);
    EXPECT_FLOAT_EQ(system->accumulated_grief_wisdom, 0.0f);
}

TEST_F(GriefSystemTest, DestroyHandlesNullPointer) {
    // WHAT: Destroy handles null gracefully
    // WHY:  Defensive programming

    grief_system_destroy(nullptr);
    SUCCEED();  // No crash
}

//=============================================================================
// Attachment Management Tests
//=============================================================================

TEST_F(GriefSystemTest, CreateAttachmentReturnsValidID) {
    // WHAT: Creating attachment returns non-zero ID
    // WHY:  API contract

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.8f, 0.9f, 0.6f);

    EXPECT_GT(att_id, 0);
    EXPECT_EQ(system->active_attachment_count, 1);
}

TEST_F(GriefSystemTest, AttachmentHasCorrectProperties) {
    // WHAT: Attachment stores correct initial values
    // WHY:  Data integrity

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.75f);

    // Find attachment
    attachment_bond_t* bond = nullptr;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        if (system->attachments[i].active && system->attachments[i].attachment_id == att_id) {
            bond = &system->attachments[i];
            break;
        }
    }

    ASSERT_NE(bond, nullptr);
    EXPECT_TRUE(bond->active);
    EXPECT_EQ(bond->type, ATTACHMENT_PARENT);
    EXPECT_FLOAT_EQ(bond->strength, 0.9f);
    EXPECT_FLOAT_EQ(bond->positive_valence, 0.85f);
    EXPECT_FLOAT_EQ(bond->dependency, 0.75f);
    EXPECT_FALSE(bond->severed);
}

TEST_F(GriefSystemTest, StrengthenAttachmentIncreasesStrength) {
    // WHAT: Strengthening increases bond strength
    // WHY:  Behavioral test

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_FRIEND, 0.5f, 0.6f, 0.3f);

    grief_strengthen_attachment(system, att_id, 0.2f);

    // Find attachment
    attachment_bond_t* bond = nullptr;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        if (system->attachments[i].active && system->attachments[i].attachment_id == att_id) {
            bond = &system->attachments[i];
            break;
        }
    }

    ASSERT_NE(bond, nullptr);
    EXPECT_FLOAT_EQ(bond->strength, 0.7f);  // 0.5 + 0.2
}

TEST_F(GriefSystemTest, StrengthenAttachmentClampsAtOne) {
    // WHAT: Strength clamped to [0, 1]
    // WHY:  Parameter validation

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_FRIEND, 0.9f, 0.8f, 0.5f);

    grief_strengthen_attachment(system, att_id, 0.5f);  // Would exceed 1.0

    attachment_bond_t* bond = nullptr;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        if (system->attachments[i].active && system->attachments[i].attachment_id == att_id) {
            bond = &system->attachments[i];
            break;
        }
    }

    ASSERT_NE(bond, nullptr);
    EXPECT_FLOAT_EQ(bond->strength, 1.0f);  // Clamped
}

TEST_F(GriefSystemTest, AddSharedMemoryIncrementsCount) {
    // WHAT: Adding shared memory increments counter
    // WHY:  Memory integration tracking

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.8f, 0.9f, 0.7f);

    for (int i = 0; i < 10; i++) {
        grief_add_shared_memory(system, att_id);
    }

    attachment_bond_t* bond = nullptr;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        if (system->attachments[i].active && system->attachments[i].attachment_id == att_id) {
            bond = &system->attachments[i];
            break;
        }
    }

    ASSERT_NE(bond, nullptr);
    EXPECT_EQ(bond->associated_memories, 10);
}

TEST_F(GriefSystemTest, MaxAttachmentsRespected) {
    // WHAT: Cannot exceed GRIEF_MAX_ATTACHMENTS
    // WHY:  Capacity constraint

    // Fill all slots
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        uint32_t att_id = grief_create_attachment(system, ATTACHMENT_FRIEND, 0.5f, 0.5f, 0.3f);
        EXPECT_GT(att_id, 0);
    }

    EXPECT_EQ(system->active_attachment_count, GRIEF_MAX_ATTACHMENTS);

    // Try to create one more (should fail)
    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_FRIEND, 0.5f, 0.5f, 0.3f);
    EXPECT_EQ(att_id, 0);  // Failure indicated by 0
}

//=============================================================================
// Loss Processing Tests
//=============================================================================

TEST_F(GriefSystemTest, ProcessLossInitiatesGrief) {
    // WHAT: Processing loss triggers grief response
    // WHY:  Core functionality

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.95f, 0.9f, 0.8f);

    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 1000000);

    EXPECT_TRUE(system->current_grief.experiencing_grief);
    EXPECT_EQ(system->current_grief.lost_attachment_id, att_id);
    EXPECT_EQ(system->current_grief.loss_onset_time, 1000000);
    EXPECT_EQ(system->lifetime_losses, 1);
}

TEST_F(GriefSystemTest, ProcessLossMarksAttachmentAsSevered) {
    // WHAT: Loss marks attachment as severed
    // WHY:  State consistency

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);

    grief_process_loss(system, att_id, LOSS_TYPE_SEPARATION, 1000000);

    attachment_bond_t* bond = nullptr;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        if (system->attachments[i].attachment_id == att_id) {
            bond = &system->attachments[i];
            break;
        }
    }

    ASSERT_NE(bond, nullptr);
    EXPECT_TRUE(bond->severed);
    EXPECT_EQ(bond->loss_time, 1000000);
    EXPECT_EQ(bond->loss_type, LOSS_TYPE_SEPARATION);
}

TEST_F(GriefSystemTest, StrongerAttachmentsProduceIntenseGrief) {
    // WHAT: Bond strength correlates with initial grief intensity
    // WHY:  Biological realism

    // Strong attachment
    uint32_t strong_id = grief_create_attachment(system, ATTACHMENT_CHILD, 1.0f, 0.95f, 0.9f);
    grief_process_loss(system, strong_id, LOSS_TYPE_DEATH, 1000000);

    float strong_pain = system->current_grief.emotional_pain_intensity;
    float strong_serotonin = system->current_grief.serotonin_depletion;

    grief_system_reset(system);

    // Weak attachment
    uint32_t weak_id = grief_create_attachment(system, ATTACHMENT_FRIEND, 0.3f, 0.4f, 0.2f);
    grief_process_loss(system, weak_id, LOSS_TYPE_SEPARATION, 1000000);

    float weak_pain = system->current_grief.emotional_pain_intensity;
    float weak_serotonin = system->current_grief.serotonin_depletion;

    EXPECT_GT(strong_pain, weak_pain);
    EXPECT_GT(strong_serotonin, weak_serotonin);
}

TEST_F(GriefSystemTest, DeathLossMoreIntenseThanSeparation) {
    // WHAT: Death produces more intense grief than separation
    // WHY:  Loss type matters (permanent vs temporary)

    // Death
    uint32_t death_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, death_id, LOSS_TYPE_DEATH, 1000000);
    float death_pain = system->current_grief.emotional_pain_intensity;

    grief_system_reset(system);

    // Separation
    uint32_t sep_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, sep_id, LOSS_TYPE_SEPARATION, 1000000);
    float sep_pain = system->current_grief.emotional_pain_intensity;

    EXPECT_GT(death_pain, sep_pain);
}

//=============================================================================
// Grief Stage Progression Tests
//=============================================================================

TEST_F(GriefSystemTest, InitialStageIsShock) {
    // WHAT: Grief starts in shock stage
    // WHY:  Kübler-Ross model

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.7f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 1000000);

    EXPECT_EQ(system->current_grief.current_stage, GRIEF_STAGE_SHOCK);
    EXPECT_GT(system->current_grief.stage_intensities[GRIEF_STAGE_SHOCK], 0.5f);
}

TEST_F(GriefSystemTest, ShockFadesOverTime) {
    // WHAT: Shock intensity decreases over weeks
    // WHY:  Temporal dynamics

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.75f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    float initial_shock = system->current_grief.stage_intensities[GRIEF_STAGE_SHOCK];

    // Advance 2 weeks
    for (int day = 0; day < 14; day++) {
        grief_update(system, 86400.0f, (uint64_t)((day + 1) * 86400) * 1000000);
    }

    float later_shock = system->current_grief.stage_intensities[GRIEF_STAGE_SHOCK];

    EXPECT_LT(later_shock, initial_shock * 0.5f);  // Should have decayed significantly
}

TEST_F(GriefSystemTest, DepressionPeaksAfterInitialShock) {
    // WHAT: Depression stage develops after shock fades
    // WHY:  Stage progression model

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.7f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    float initial_depression = system->current_grief.stage_intensities[GRIEF_STAGE_DEPRESSION];

    // Advance 1 month
    grief_update(system, 30.0f * 86400.0f, (uint64_t)(30 * 86400) * 1000000);

    float later_depression = system->current_grief.stage_intensities[GRIEF_STAGE_DEPRESSION];

    EXPECT_GT(later_depression, initial_depression);
}

TEST_F(GriefSystemTest, AcceptanceGrowsSlowly) {
    // WHAT: Acceptance develops over months
    // WHY:  Integration takes time

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    float initial_acceptance = system->current_grief.stage_intensities[GRIEF_STAGE_ACCEPTANCE];

    // Advance 6 months
    grief_update(system, 180.0f * 86400.0f, (uint64_t)(180 * 86400) * 1000000);

    float later_acceptance = system->current_grief.stage_intensities[GRIEF_STAGE_ACCEPTANCE];

    EXPECT_GT(later_acceptance, initial_acceptance);
}

TEST_F(GriefSystemTest, CurrentStageTracksHighestIntensity) {
    // WHAT: current_stage reflects predominant stage
    // WHY:  State tracking

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_CHILD, 0.95f, 0.9f, 0.85f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    // Initially shock
    EXPECT_EQ(system->current_grief.current_stage, GRIEF_STAGE_SHOCK);

    // After time, should transition
    grief_update(system, 60.0f * 86400.0f, (uint64_t)(60 * 86400) * 1000000);

    // Should have moved past shock
    EXPECT_NE(system->current_grief.current_stage, GRIEF_STAGE_SHOCK);
}

//=============================================================================
// Dual Process Model Tests
//=============================================================================

TEST_F(GriefSystemTest, LossOrientationHighInitially) {
    // WHAT: Early grief is loss-focused
    // WHY:  Dual Process Model (Stroebe & Schut)

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    grief_update(system, 0.01f, 1000);

    EXPECT_GT(system->current_grief.loss_orientation, 0.5f);
}

TEST_F(GriefSystemTest, RestorationOrientationIncreasesOverTime) {
    // WHAT: Later grief becomes more restoration-focused
    // WHY:  Dual Process Model shift

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    float initial_restoration = system->current_grief.restoration_orientation;

    // Advance 3 months
    grief_update(system, 90.0f * 86400.0f, (uint64_t)(90 * 86400) * 1000000);

    float later_restoration = system->current_grief.restoration_orientation;

    EXPECT_GT(later_restoration, initial_restoration);
}

TEST_F(GriefSystemTest, OrientationsOscillate) {
    // WHAT: Loss and restoration orientations cycle
    // WHY:  Dual Process oscillation

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.7f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    float prev_loss = system->current_grief.loss_orientation;
    bool oscillated = false;

    // Track over several hours (oscillation period ~ 1 hour)
    for (int i = 0; i < 10; i++) {
        grief_update(system, 3600.0f, (uint64_t)((i + 1) * 3600) * 1000000);
        float curr_loss = system->current_grief.loss_orientation;

        if (fabsf(curr_loss - prev_loss) > 0.1f) {
            oscillated = true;
        }
        prev_loss = curr_loss;
    }

    EXPECT_TRUE(oscillated);
}

//=============================================================================
// Neurobiological Changes Tests
//=============================================================================

TEST_F(GriefSystemTest, SerotoninDepletionOccurs) {
    // WHAT: Grief depletes serotonin
    // WHY:  Neurobiology of grief

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 1000000);

    grief_update(system, 0.01f, 1000000);

    EXPECT_GT(system->current_grief.serotonin_depletion, 0.0f);
}

TEST_F(GriefSystemTest, DopamineDepletionCausesAnhedonia) {
    // WHAT: Grief depletes dopamine (loss of pleasure)
    // WHY:  Anhedonia mechanism

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 1000000);

    grief_update(system, 0.01f, 1000000);

    EXPECT_GT(system->current_grief.dopamine_depletion, 0.0f);
    EXPECT_GT(system->current_grief.anhedonia_level, 0.0f);
}

TEST_F(GriefSystemTest, NorepinephrineElevationOccurs) {
    // WHAT: Grief increases norepinephrine (stress/arousal)
    // WHY:  Stress response

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_CHILD, 0.95f, 0.9f, 0.85f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 1000000);

    grief_update(system, 0.01f, 1000000);

    EXPECT_GT(system->current_grief.norepinephrine_elevation, 0.0f);
}

TEST_F(GriefSystemTest, CortisolElevationOccurs) {
    // WHAT: Grief increases cortisol (chronic stress)
    // WHY:  HPA axis activation

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 1000000);

    grief_update(system, 0.01f, 1000000);

    EXPECT_GT(system->current_grief.cortisol_elevation, 0.0f);
}

TEST_F(GriefSystemTest, GetNeuromodulatorEffectsReturnsCorrectValues) {
    // WHAT: Query function returns neuromodulator impacts
    // WHY:  Integration with neuromodulator system

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 1000000);

    grief_update(system, 0.01f, 1000000);

    float serotonin_factor, dopamine_factor, norepinephrine_factor;
    grief_get_neuromodulator_effects(system, &serotonin_factor, &dopamine_factor, &norepinephrine_factor);

    EXPECT_LT(serotonin_factor, 1.0f);  // Reduced
    EXPECT_LT(dopamine_factor, 1.0f);   // Reduced
    EXPECT_GT(norepinephrine_factor, 1.0f);  // Elevated
}

//=============================================================================
// Coping Mechanisms Tests
//=============================================================================

TEST_F(GriefSystemTest, SeekingSupportReducesPain) {
    // WHAT: Social support reduces emotional pain
    // WHY:  Adaptive coping mechanism

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    grief_update(system, 0.01f, 1000);

    float pain_without_support = system->current_grief.emotional_pain_intensity;

    // Seek high-quality support
    grief_seek_support(system, 0.9f);
    grief_update(system, 86400.0f, 86400 * 1000000);  // 1 day

    float pain_with_support = system->current_grief.emotional_pain_intensity;

    EXPECT_LT(pain_with_support, pain_without_support);
    EXPECT_EQ(system->current_grief.predominant_coping, COPING_ADAPTIVE);
}

TEST_F(GriefSystemTest, AvoidanceIncreasesDistress) {
    // WHAT: Avoidant coping prolongs grief
    // WHY:  Maladaptive mechanism

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    grief_update(system, 0.01f, 1000);

    // Use avoidant coping
    grief_avoid_reminders(system, 0.9f);
    grief_update(system, 86400.0f, 86400 * 1000000);

    EXPECT_GT(system->current_grief.avoidance_level, 0.0f);
    EXPECT_EQ(system->current_grief.predominant_coping, COPING_AVOIDANT);
}

TEST_F(GriefSystemTest, ExpressingEmotionsIsHealthy) {
    // WHAT: Emotional expression is adaptive
    // WHY:  Catharsis and processing

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_CHILD, 0.95f, 0.9f, 0.85f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    grief_express_emotions(system, 0.8f);

    EXPECT_EQ(system->current_grief.predominant_coping, COPING_ADAPTIVE);
}

//=============================================================================
// Existential Awareness Tests
//=============================================================================

TEST_F(GriefSystemTest, MortalityContemplationIncreasesAwareness) {
    // WHAT: Contemplating mortality increases awareness
    // WHY:  Existential development

    grief_contemplate_mortality(system, 0.8f, 1000000);

    EXPECT_TRUE(system->existential.aware_of_mortality);
    EXPECT_GT(system->existential.mortality_salience, 0.0f);
}

TEST_F(GriefSystemTest, DeathAnxietyIncreasesWithReminders) {
    // WHAT: Death reminders increase anxiety
    // WHY:  Terror Management Theory

    float initial_anxiety = system->existential.death_anxiety;

    grief_contemplate_mortality(system, 0.9f, 1000000);

    float later_anxiety = system->existential.death_anxiety;

    EXPECT_GT(later_anxiety, initial_anxiety);
}

TEST_F(GriefSystemTest, MeaningMakingReducesAnxiety) {
    // WHAT: Finding meaning reduces existential anxiety
    // WHY:  Post-traumatic growth

    grief_contemplate_mortality(system, 0.8f, 1000000);
    float high_anxiety = system->existential.existential_anxiety;

    grief_find_meaning(system, 0.9f);

    float reduced_anxiety = system->existential.existential_anxiety;

    EXPECT_LT(reduced_anxiety, high_anxiety);
    EXPECT_GT(system->existential.sense_of_purpose, 0.0f);
}

TEST_F(GriefSystemTest, MeaningMakingIncreasesAcceptance) {
    // WHAT: Meaning-making increases acceptance of finitude
    // WHY:  Integration process

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    float initial_meaning = system->current_grief.meaning_making_progress;

    grief_find_meaning(system, 0.8f);

    float later_meaning = system->current_grief.meaning_making_progress;

    EXPECT_GT(later_meaning, initial_meaning);
}

//=============================================================================
// Prolonged Grief Detection Tests
//=============================================================================

TEST_F(GriefSystemTest, ProlongedGriefDetectedForSevereUnresolvedGrief) {
    // WHAT: Prolonged grief flagged after extended high intensity
    // WHY:  Clinical concern (DSM-5 criteria)

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_CHILD, 1.0f, 0.95f, 0.9f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    // Simulate 1 year with sustained high pain and avoidance
    for (int month = 0; month < 12; month++) {
        grief_update(system, 30.0f * 86400.0f, (uint64_t)((month + 1) * 30 * 86400) * 1000000);
        grief_avoid_reminders(system, 0.9f);  // Maintain avoidance
    }

    EXPECT_TRUE(system->current_grief.prolonged_grief_risk);
}

//=============================================================================
// Query Function Tests
//=============================================================================

TEST_F(GriefSystemTest, IsGrievingReturnsTrueWhenGrieving) {
    // WHAT: Query returns correct grief state
    // WHY:  API contract

    EXPECT_FALSE(grief_is_grieving(system));

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 1000000);

    EXPECT_TRUE(grief_is_grieving(system));
}

TEST_F(GriefSystemTest, GetPainIntensityReturnsCorrectValue) {
    // WHAT: Query returns pain intensity
    // WHY:  Monitoring capability

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 1000000);

    float pain = grief_get_pain_intensity(system);

    EXPECT_GT(pain, 0.0f);
    EXPECT_LE(pain, 1.0f);
}

TEST_F(GriefSystemTest, GetCurrentStageReturnsCorrectStage) {
    // WHAT: Query returns current stage
    // WHY:  State tracking

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 1000000);

    grief_stage_t stage = grief_get_current_stage(system);

    EXPECT_EQ(stage, GRIEF_STAGE_SHOCK);
}

TEST_F(GriefSystemTest, HasProlongedGriefRiskReturnsCorrectValue) {
    // WHAT: Query returns risk status
    // WHY:  Clinical monitoring

    EXPECT_FALSE(grief_has_prolonged_grief_risk(system));

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_CHILD, 1.0f, 0.95f, 0.9f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    // Simulate prolonged unresolved grief
    for (int i = 0; i < 400; i++) {
        grief_update(system, 86400.0f, (uint64_t)((i + 1) * 86400) * 1000000);
        grief_avoid_reminders(system, 0.95f);
    }

    EXPECT_TRUE(grief_has_prolonged_grief_risk(system));
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(GriefSystemTest, HandlesNullPointers) {
    // WHAT: Functions handle null gracefully
    // WHY:  Defensive programming

    grief_create_attachment(nullptr, ATTACHMENT_FRIEND, 0.5f, 0.5f, 0.3f);
    grief_strengthen_attachment(nullptr, 1, 0.1f);
    grief_process_loss(nullptr, 1, LOSS_TYPE_DEATH, 1000000);
    grief_update(nullptr, 1.0f, 1000000);
    grief_contemplate_mortality(nullptr, 0.5f, 1000000);
    grief_find_meaning(nullptr, 0.5f);
    grief_seek_support(nullptr, 0.5f);

    SUCCEED();  // No crash
}

TEST_F(GriefSystemTest, HandlesInvalidAttachmentID) {
    // WHAT: Invalid ID handled gracefully
    // WHY:  Error handling

    grief_strengthen_attachment(system, 99999, 0.1f);
    grief_process_loss(system, 99999, LOSS_TYPE_DEATH, 1000000);

    SUCCEED();  // No crash
}

TEST_F(GriefSystemTest, HandlesZeroTimeStep) {
    // WHAT: Zero dt doesn't change state
    // WHY:  Edge case

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 0.9f, 0.85f, 0.75f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 1000000);

    float initial_pain = system->current_grief.emotional_pain_intensity;

    grief_update(system, 0.0f, 1000000);

    float later_pain = system->current_grief.emotional_pain_intensity;

    EXPECT_FLOAT_EQ(initial_pain, later_pain);
}

TEST_F(GriefSystemTest, HandlesNegativeParameters) {
    // WHAT: Negative parameters clamped
    // WHY:  Parameter validation

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_FRIEND, -0.5f, -0.3f, -0.2f);

    attachment_bond_t* bond = nullptr;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        if (system->attachments[i].active && system->attachments[i].attachment_id == att_id) {
            bond = &system->attachments[i];
            break;
        }
    }

    ASSERT_NE(bond, nullptr);
    EXPECT_GE(bond->strength, 0.0f);
    EXPECT_GE(bond->positive_valence, 0.0f);
    EXPECT_GE(bond->dependency, 0.0f);
}

TEST_F(GriefSystemTest, HandlesExcessiveParameters) {
    // WHAT: Parameters > 1.0 clamped
    // WHY:  Parameter validation

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_ROMANTIC, 10.0f, 5.0f, 3.0f);

    attachment_bond_t* bond = nullptr;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        if (system->attachments[i].active && system->attachments[i].attachment_id == att_id) {
            bond = &system->attachments[i];
            break;
        }
    }

    ASSERT_NE(bond, nullptr);
    EXPECT_LE(bond->strength, 1.0f);
    EXPECT_LE(bond->positive_valence, 1.0f);
    EXPECT_LE(bond->dependency, 1.0f);
}

TEST_F(GriefSystemTest, MultipleSequentialLossesTracked) {
    // WHAT: Multiple losses increment counter
    // WHY:  Lifetime tracking

    for (int i = 0; i < 5; i++) {
        uint32_t att_id = grief_create_attachment(system, ATTACHMENT_FRIEND, 0.6f, 0.7f, 0.4f);
        grief_process_loss(system, att_id, LOSS_TYPE_SEPARATION, 1000000 * (i + 1));
    }

    EXPECT_EQ(system->lifetime_losses, 5);
}

TEST_F(GriefSystemTest, AccumulatedWisdomIncreasesWithLosses) {
    // WHAT: Grief wisdom grows with experience
    // WHY:  Learning from loss

    float initial_wisdom = system->accumulated_grief_wisdom;

    for (int i = 0; i < 3; i++) {
        uint32_t att_id = grief_create_attachment(system, ATTACHMENT_FRIEND, 0.7f, 0.7f, 0.5f);
        grief_process_loss(system, att_id, LOSS_TYPE_SEPARATION, 1000000 * (i + 1));

        // Process through grief
        grief_update(system, 90.0f * 86400.0f, (uint64_t)((i + 1) * 90 * 86400) * 1000000);
        grief_find_meaning(system, 0.8f);
    }

    float final_wisdom = system->accumulated_grief_wisdom;

    EXPECT_GT(final_wisdom, initial_wisdom);
}

TEST_F(GriefSystemTest, UpdateCallsCounterIncrements) {
    // WHAT: Statistics track update calls
    // WHY:  Monitoring capability

    uint32_t att_id = grief_create_attachment(system, ATTACHMENT_PARENT, 0.9f, 0.85f, 0.8f);
    grief_process_loss(system, att_id, LOSS_TYPE_DEATH, 0);

    for (int i = 0; i < 100; i++) {
        grief_update(system, 0.1f, (uint64_t)(i * 100000));
    }

    EXPECT_EQ(system->total_update_calls, 100);
}
