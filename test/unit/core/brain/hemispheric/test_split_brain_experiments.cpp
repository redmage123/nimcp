/**
 * @file test_split_brain_experiments.cpp
 * @brief Unit tests for split-brain experimental framework
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/brain/hemispheric/nimcp_split_brain_experiments.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"

//=============================================================================
// Configuration Tests
//=============================================================================

TEST(SplitBrainConfigTest, DefaultConfigHasValidValues) {
    split_brain_session_config_t config = split_brain_session_default_config();

    EXPECT_EQ(config.paradigm, PARADIGM_CHIMERIC_FACES);
    EXPECT_GT(config.num_trials, 0u);
    EXPECT_GT(config.stimulus_duration_ms, 0.0f);
    EXPECT_GT(config.response_timeout_ms, 0.0f);
    EXPECT_TRUE(config.detect_cross_cueing);
}

TEST(SplitBrainConfigTest, ValidateConfigRejectsNull) {
    EXPECT_FALSE(split_brain_validate_config(nullptr));
}

TEST(SplitBrainConfigTest, ValidateConfigAcceptsDefaults) {
    split_brain_session_config_t config = split_brain_session_default_config();
    EXPECT_TRUE(split_brain_validate_config(&config));
}

TEST(SplitBrainConfigTest, ValidateConfigRejectsZeroTrials) {
    split_brain_session_config_t config = split_brain_session_default_config();
    config.num_trials = 0;
    EXPECT_FALSE(split_brain_validate_config(&config));
}

TEST(SplitBrainConfigTest, ValidateConfigRejectsExcessiveTrials) {
    split_brain_session_config_t config = split_brain_session_default_config();
    config.num_trials = SPLIT_BRAIN_MAX_TRIALS + 1;
    EXPECT_FALSE(split_brain_validate_config(&config));
}

TEST(SplitBrainConfigTest, ValidateConfigRejectsInvalidStrength) {
    split_brain_session_config_t config = split_brain_session_default_config();
    config.callosal_strength = 1.5f;  // Invalid > 1.0
    EXPECT_FALSE(split_brain_validate_config(&config));
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(SplitBrainLifecycleTest, CreateWithNullBrainFails) {
    split_brain_session_config_t config = split_brain_session_default_config();
    split_brain_session_t* session = split_brain_session_create(&config, nullptr);
    EXPECT_EQ(session, nullptr);
}

TEST(SplitBrainLifecycleTest, CreateWithDefaultConfigSucceeds) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_t* session = split_brain_session_create(nullptr, brain);
    EXPECT_NE(session, nullptr);

    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

TEST(SplitBrainLifecycleTest, DestroyNullSessionIsSafe) {
    split_brain_session_destroy(nullptr);
    SUCCEED();
}

TEST(SplitBrainLifecycleTest, StartSessionConfiguresCallosum) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_config_t config = split_brain_session_default_config();
    config.callosal_condition = CALLOSAL_STATE_SEVERED;

    split_brain_session_t* session = split_brain_session_create(&config, brain);
    ASSERT_NE(session, nullptr);

    int result = split_brain_session_start(session);
    EXPECT_EQ(result, 0);

    // Callosum should be disconnected
    EXPECT_FALSE(hemispheric_brain_is_callosum_intact(brain));

    split_brain_session_end(session);

    // Callosum should be restored
    EXPECT_TRUE(hemispheric_brain_is_callosum_intact(brain));

    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Paradigm Tests
//=============================================================================

TEST(SplitBrainParadigmTest, ChimericFacesExperimentCreates) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_t* session = split_brain_create_chimeric_faces_experiment(brain, 10);
    EXPECT_NE(session, nullptr);

    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

TEST(SplitBrainParadigmTest, DichoticExperimentCreates) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_t* session = split_brain_create_dichotic_experiment(brain, 10);
    EXPECT_NE(session, nullptr);

    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

TEST(SplitBrainParadigmTest, TachistoscopicExperimentCreates) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_t* session = split_brain_create_tachistoscopic_experiment(brain, 10, 150.0f);
    EXPECT_NE(session, nullptr);

    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

TEST(SplitBrainParadigmTest, DegradationStudyCreates) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_t* session = split_brain_create_degradation_study(brain, 5, 10);
    EXPECT_NE(session, nullptr);

    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Trial Tests
//=============================================================================

TEST(SplitBrainTrialTest, CreateTrialSucceeds) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_t* session = split_brain_session_create(nullptr, brain);
    ASSERT_NE(session, nullptr);

    split_brain_session_start(session);

    split_brain_trial_t* trial = split_brain_trial_create(session, PARADIGM_CHIMERIC_FACES);
    EXPECT_NE(trial, nullptr);
    EXPECT_EQ(trial->paradigm, PARADIGM_CHIMERIC_FACES);
    EXPECT_EQ(trial->trial_number, 0u);

    split_brain_session_end(session);
    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

TEST(SplitBrainTrialTest, SetChimericStimuliSucceeds) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_t* session = split_brain_session_create(nullptr, brain);
    ASSERT_NE(session, nullptr);

    split_brain_session_start(session);

    split_brain_trial_t* trial = split_brain_trial_create(session, PARADIGM_CHIMERIC_FACES);
    ASSERT_NE(trial, nullptr);

    float left_data[] = {1.0f, 2.0f, 3.0f};
    float right_data[] = {4.0f, 5.0f, 6.0f};

    split_brain_stimulus_t left_stim = {
        .type = STIMULUS_VISUAL,
        .visual_field = VISUAL_FIELD_LEFT,
        .data = left_data,
        .data_size = 3
    };

    split_brain_stimulus_t right_stim = {
        .type = STIMULUS_VISUAL,
        .visual_field = VISUAL_FIELD_RIGHT,
        .data = right_data,
        .data_size = 3
    };

    int result = split_brain_trial_set_chimeric(trial, &left_stim, &right_stim);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(trial->use_chimeric);

    split_brain_session_end(session);
    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

TEST(SplitBrainTrialTest, RunTrialUpdatesStats) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_t* session = split_brain_session_create(nullptr, brain);
    ASSERT_NE(session, nullptr);

    split_brain_session_start(session);

    split_brain_trial_t* trial = split_brain_trial_create(session, PARADIGM_SPATIAL_VERBAL);
    ASSERT_NE(trial, nullptr);

    int result = split_brain_trial_run(session, trial);
    EXPECT_EQ(result, 0);

    split_brain_session_stats_t stats;
    split_brain_session_get_stats(session, &stats);
    EXPECT_EQ(stats.completed_trials, 1u);

    split_brain_session_end(session);
    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Callosal Condition Tests
//=============================================================================

TEST(SplitBrainCallosalTest, ApplySeveredCondition) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_t* session = split_brain_session_create(nullptr, brain);
    ASSERT_NE(session, nullptr);

    int result = split_brain_apply_callosal_condition(session, CALLOSAL_STATE_SEVERED);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(hemispheric_brain_is_callosum_intact(brain));

    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

TEST(SplitBrainCallosalTest, ApplyIntactCondition) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_t* session = split_brain_session_create(nullptr, brain);
    ASSERT_NE(session, nullptr);

    // First sever
    split_brain_apply_callosal_condition(session, CALLOSAL_STATE_SEVERED);
    EXPECT_FALSE(hemispheric_brain_is_callosum_intact(brain));

    // Then restore
    int result = split_brain_apply_callosal_condition(session, CALLOSAL_STATE_INTACT);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(hemispheric_brain_is_callosum_intact(brain));

    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

TEST(SplitBrainCallosalTest, RestoreCallosumWorks) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_config_t config = split_brain_session_default_config();
    config.callosal_condition = CALLOSAL_STATE_SEVERED;

    split_brain_session_t* session = split_brain_session_create(&config, brain);
    ASSERT_NE(session, nullptr);

    split_brain_session_start(session);
    EXPECT_FALSE(hemispheric_brain_is_callosum_intact(brain));

    split_brain_restore_callosum(session);
    EXPECT_TRUE(hemispheric_brain_is_callosum_intact(brain));

    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Agreement Analysis Tests
//=============================================================================

TEST(SplitBrainAgreementTest, ComputeAgreementNullTrialReturnsZero) {
    float agreement = split_brain_compute_agreement(nullptr);
    EXPECT_FLOAT_EQ(agreement, 0.0f);
}

TEST(SplitBrainAgreementTest, DetectConflictNullTrialReturnsFalse) {
    bool conflict = split_brain_detect_conflict(nullptr, 0.5f);
    EXPECT_FALSE(conflict);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST(SplitBrainStatsTest, GetStatsNullReturnsError) {
    split_brain_session_stats_t stats;
    int result = split_brain_session_get_stats(nullptr, &stats);
    EXPECT_LT(result, 0);
}

TEST(SplitBrainStatsTest, InitialStatsAreZero) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_t* session = split_brain_session_create(nullptr, brain);
    ASSERT_NE(session, nullptr);

    split_brain_session_stats_t stats;
    int result = split_brain_session_get_stats(session, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_trials, 0u);
    EXPECT_EQ(stats.completed_trials, 0u);

    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

TEST(SplitBrainStatsTest, ComputeLateralizationIndex) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_t* session = split_brain_session_create(nullptr, brain);
    ASSERT_NE(session, nullptr);

    // Without trials, LI should be 0
    float li = split_brain_compute_lateralization_index(session, COGNITIVE_DOMAIN_LANGUAGE);
    EXPECT_FLOAT_EQ(li, 0.0f);

    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

TEST(SplitBrainStatsTest, AnalyzeReactionTimes) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_t* session = split_brain_session_create(nullptr, brain);
    ASSERT_NE(session, nullptr);

    float left_rt, right_rt, diff;
    int result = split_brain_analyze_reaction_times(session, &left_rt, &right_rt, &diff);
    EXPECT_EQ(result, 0);

    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Report Generation Tests
//=============================================================================

TEST(SplitBrainReportTest, GenerateReportNullReturnsError) {
    char buffer[1024];
    int result = split_brain_generate_report(nullptr, buffer, sizeof(buffer));
    EXPECT_LT(result, 0);
}

TEST(SplitBrainReportTest, GenerateReportSucceeds) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_t* session = split_brain_session_create(nullptr, brain);
    ASSERT_NE(session, nullptr);

    char buffer[2048];
    int len = split_brain_generate_report(session, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_TRUE(strstr(buffer, "Split-Brain") != nullptr);

    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST(SplitBrainUtilTest, ParadigmNameReturnsValidString) {
    EXPECT_STREQ(split_brain_paradigm_name(PARADIGM_CHIMERIC_FACES), "Chimeric Faces");
    EXPECT_STREQ(split_brain_paradigm_name(PARADIGM_DICHOTIC_LISTENING), "Dichotic Listening");
    EXPECT_STREQ(split_brain_paradigm_name(PARADIGM_TACHISTOSCOPIC), "Tachistoscopic");
}

TEST(SplitBrainUtilTest, CallosalConditionNameReturnsValidString) {
    EXPECT_STREQ(split_brain_callosal_condition_name(CALLOSAL_STATE_INTACT), "Intact");
    EXPECT_STREQ(split_brain_callosal_condition_name(CALLOSAL_STATE_SEVERED), "Severed (Split-Brain)");
    EXPECT_STREQ(split_brain_callosal_condition_name(CALLOSAL_STATE_DEGRADED), "Degraded");
}

TEST(SplitBrainUtilTest, OutcomeNameReturnsValidString) {
    EXPECT_STREQ(split_brain_outcome_name(OUTCOME_CORRECT), "Correct");
    EXPECT_STREQ(split_brain_outcome_name(OUTCOME_CONFLICT), "Conflict");
    EXPECT_STREQ(split_brain_outcome_name(OUTCOME_CROSS_CUE_DETECTED), "Cross-Cue Detected");
}

//=============================================================================
// Callback Tests
//=============================================================================

static int callback_count = 0;
static void test_trial_callback(split_brain_trial_t* trial, void* user_data) {
    (void)trial;
    (void)user_data;
    callback_count++;
}

TEST(SplitBrainCallbackTest, TrialCallbackInvoked) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    split_brain_session_t* session = split_brain_session_create(nullptr, brain);
    ASSERT_NE(session, nullptr);

    callback_count = 0;
    split_brain_set_trial_callback(session, test_trial_callback, nullptr);

    split_brain_session_start(session);

    split_brain_trial_t* trial = split_brain_trial_create(session, PARADIGM_SPATIAL_VERBAL);
    ASSERT_NE(trial, nullptr);

    split_brain_trial_run(session, trial);
    EXPECT_EQ(callback_count, 1);

    split_brain_session_end(session);
    split_brain_session_destroy(session);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
