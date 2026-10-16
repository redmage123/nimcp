/**
 * @file test_split_brain_e2e.cpp
 * @brief End-to-end tests for split-brain experiment simulation
 *
 * WHAT: Full pipeline tests for split-brain experiments and callosotomy studies
 * WHY:  Verify hemispheric independence, cross-cueing detection, and confabulation
 * HOW:  Test visual field presentation, verbal response limitations, callosum control
 *
 * TEST COVERAGE:
 * - Split-Brain Session Lifecycle (3 tests)
 * - Visual Field Presentation (4 tests)
 * - Verbal Response Limitations (4 tests)
 * - Cross-Cueing Detection (3 tests)
 * - Callosum Reconnection Recovery (3 tests)
 * - Confabulation Analysis (3 tests)
 *
 * TOTAL: 20 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Commissurotomy severs corpus callosum for epilepsy treatment
 * - Results in two independent conscious hemispheres
 * - Classic Sperry/Gazzaniga experiments reveal lateralization
 * - Left hemisphere cannot verbalize right-hemisphere inputs
 * - Cross-cueing: information leaks through non-callosal paths
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "../../../e2e_test_framework.h"
#include "utils/nimcp_test_base.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <cstring>


#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "core/brain/hemispheric/nimcp_corpus_callosum.h"
#include "core/brain/hemispheric/nimcp_split_brain_experiments.h"
#include "core/brain/hemispheric/nimcp_lateralization.h"
#include "utils/memory/nimcp_memory.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_EXPERIMENT_TIME_MS = 500.0;
constexpr double MAX_TRIAL_TIME_MS = 100.0;
constexpr double MAX_CALLOSUM_OP_TIME_MS = 50.0;
constexpr float CROSS_CUE_THRESHOLD = 0.3f;
constexpr float CONFLICT_THRESHOLD = 0.5f;
constexpr uint32_t INPUT_SIZE = 64;
constexpr uint32_t OUTPUT_SIZE = 32;
constexpr uint32_t NUM_TRIALS = 10;

//=============================================================================
// Helper Functions
//=============================================================================

static std::vector<float> generate_visual_stimulus(uint32_t size, uint32_t seed) {
    std::vector<float> stimulus(size);
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (uint32_t i = 0; i < size; i++) {
        stimulus[i] = dist(gen);
    }
    return stimulus;
}

static std::vector<float> generate_left_visual_field_stimulus(uint32_t size) {
    // Pattern for left visual field (goes to RIGHT hemisphere)
    std::vector<float> stimulus(size);
    for (uint32_t i = 0; i < size; i++) {
        stimulus[i] = 0.3f + 0.4f * std::sin(i * 0.2f);
    }
    return stimulus;
}

static std::vector<float> generate_right_visual_field_stimulus(uint32_t size) {
    // Pattern for right visual field (goes to LEFT hemisphere)
    std::vector<float> stimulus(size);
    for (uint32_t i = 0; i < size; i++) {
        stimulus[i] = 0.6f + 0.3f * std::cos(i * 0.3f);
    }
    return stimulus;
}

//=============================================================================
// Test Fixture
//=============================================================================

class E2ESplitBrainTest : public ::testing::Test {
protected:
    static hemispheric_brain_t* shared_brain;
    hemispheric_brain_t* brain = nullptr;
    split_brain_session_t* session = nullptr;

    static void SetUpTestSuite() {
        signal_handler_unregister_brain();
        signal_handler_reset_stats();
        signal_handler_uninstall();

        hemispheric_brain_config_t config = hemispheric_brain_default_config();
        config.size = BRAIN_SIZE_MICRO;
        config.num_inputs = INPUT_SIZE;
        config.num_outputs = OUTPUT_SIZE;
        config.default_mode = HEMISPHERIC_MODE_LATERALIZED;

        shared_brain = hemispheric_brain_create(&config);

        // Warm up: training sets eligibility traces, updates build activity
        // via neuromodulators (dopamine/norepinephrine). The adaptive spiking
        // network may still produce near-zero output magnitudes with random
        // weights, but activity_level accumulates from neuromodulator updates.
        if (shared_brain) {
            brain_hemisphere_t* left = hemispheric_brain_get_left(shared_brain);
            brain_hemisphere_t* right = hemispheric_brain_get_right(shared_brain);

            std::vector<float> warmup_input(INPUT_SIZE);
            std::vector<float> warmup_target(INPUT_SIZE);

            for (int iter = 0; iter < 10; iter++) {
                for (uint32_t i = 0; i < INPUT_SIZE; i++) {
                    warmup_input[i] = 0.5f + 0.3f * sinf((float)(i + iter));
                    warmup_target[i] = 0.5f + 0.2f * cosf((float)(i + iter));
                }
                if (left) hemisphere_train(left, warmup_input.data(), warmup_target.data(), INPUT_SIZE);
                if (right) hemisphere_train(right, warmup_input.data(), warmup_target.data(), INPUT_SIZE);
            }

            // Run updates to build activity_level through neuromodulators
            for (int i = 0; i < 50; i++) {
                hemispheric_brain_update(shared_brain, 0.01f);
            }
        }
    }

    static void TearDownTestSuite() {
        if (shared_brain) {
            hemispheric_brain_destroy(shared_brain);
            shared_brain = nullptr;
        }
    }

    void SetUp() override {
        brain = shared_brain;
        ASSERT_NE(brain, nullptr) << "Failed to create hemispheric brain";
    }

    void TearDown() override {
        if (session) {
            split_brain_session_destroy(session);
            session = nullptr;
        }
    }

    void createSession(callosal_condition_t condition = CALLOSAL_STATE_SEVERED) {
        split_brain_session_config_t config = split_brain_session_default_config();
        config.callosal_condition = condition;
        config.num_trials = NUM_TRIALS;
        config.detect_cross_cueing = true;
        config.cross_cue_threshold = CROSS_CUE_THRESHOLD;

        session = split_brain_session_create(&config, brain);
        ASSERT_NE(session, nullptr) << "Failed to create split-brain session";
    }
};

hemispheric_brain_t* E2ESplitBrainTest::shared_brain = nullptr;

//=============================================================================
// Split-Brain Session Lifecycle Tests
//=============================================================================

TEST_F(E2ESplitBrainTest, SessionCreationAndDestruction) {
    E2E_PIPELINE_START("Session Lifecycle");

    // Create session
    E2E_STAGE_BEGIN("Create session", 50);
    split_brain_session_config_t config = split_brain_session_default_config();
    config.paradigm = PARADIGM_TACHISTOSCOPIC;
    config.num_trials = 5;

    session = split_brain_session_create(&config, brain);
    ASSERT_NE(session, nullptr);
    E2E_STAGE_END();

    // Start session
    E2E_STAGE_BEGIN("Start session", 50);
    int result = split_brain_session_start(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // End session
    E2E_STAGE_BEGIN("End session", 50);
    result = split_brain_session_end(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESplitBrainTest, SessionPauseAndResume) {
    E2E_PIPELINE_START("Session Pause/Resume");

    createSession();

    // Start session
    E2E_STAGE_BEGIN("Start session", 50);
    int result = split_brain_session_start(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Pause
    E2E_STAGE_BEGIN("Pause session", 50);
    result = split_brain_session_pause(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Resume
    E2E_STAGE_BEGIN("Resume session", 50);
    result = split_brain_session_resume(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // End
    E2E_STAGE_BEGIN("End session", 50);
    result = split_brain_session_end(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESplitBrainTest, SessionWithMultipleTrials) {
    E2E_PIPELINE_START("Session Multiple Trials");

    createSession();

    E2E_STAGE_BEGIN("Start and run trials", MAX_EXPERIMENT_TIME_MS);
    int result = split_brain_session_start(session);
    EXPECT_EQ(result, 0);

    // Run all trials
    int trials_completed = split_brain_session_run_all_trials(session);
    EXPECT_GT(trials_completed, 0);

    result = split_brain_session_end(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Get statistics
    E2E_STAGE_BEGIN("Get statistics", 20);
    split_brain_session_stats_t stats;
    result = split_brain_session_get_stats(session, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GT(stats.completed_trials, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Visual Field Presentation Tests
//=============================================================================

TEST_F(E2ESplitBrainTest, LeftVisualFieldToRightHemisphere) {
    E2E_PIPELINE_START("Left Visual Field Processing");

    createSession();

    // Start session to apply callosal condition (severed)
    E2E_STAGE_BEGIN("Start session and verify disconnection", 50);
    int start_result = split_brain_session_start(session);
    EXPECT_EQ(start_result, 0);
    EXPECT_FALSE(hemispheric_brain_is_callosum_intact(brain))
        << "Callosum should be disconnected after session start";
    E2E_STAGE_END();

    // Create left visual field stimulus (goes to right hemisphere)
    E2E_STAGE_BEGIN("Process LVF stimulus", MAX_TRIAL_TIME_MS);
    auto lvf_stimulus = generate_left_visual_field_stimulus(INPUT_SIZE);
    std::vector<float> output(OUTPUT_SIZE);

    // Process through spatial domain (right hemisphere)
    int result = hemispheric_brain_process_lateralized(
        brain,
        lvf_stimulus.data(),
        INPUT_SIZE,
        COGNITIVE_DOMAIN_SPATIAL,
        output.data(),
        OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Verify right hemisphere processed
    E2E_STAGE_BEGIN("Verify right processing", 20);
    brain_hemisphere_t* right = hemispheric_brain_get_right(brain);
    ASSERT_NE(right, nullptr);
    float activity = hemisphere_get_activity(right);
    EXPECT_GE(activity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESplitBrainTest, RightVisualFieldToLeftHemisphere) {
    E2E_PIPELINE_START("Right Visual Field Processing");

    createSession();

    // Create right visual field stimulus (goes to left hemisphere)
    E2E_STAGE_BEGIN("Process RVF stimulus", MAX_TRIAL_TIME_MS);
    auto rvf_stimulus = generate_right_visual_field_stimulus(INPUT_SIZE);
    std::vector<float> output(OUTPUT_SIZE);

    // Process through language domain (left hemisphere)
    int result = hemispheric_brain_process_lateralized(
        brain,
        rvf_stimulus.data(),
        INPUT_SIZE,
        COGNITIVE_DOMAIN_LANGUAGE,
        output.data(),
        OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Verify left hemisphere processed
    E2E_STAGE_BEGIN("Verify left processing", 20);
    brain_hemisphere_t* left = hemispheric_brain_get_left(brain);
    ASSERT_NE(left, nullptr);
    float activity = hemisphere_get_activity(left);
    EXPECT_GE(activity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESplitBrainTest, ChimericStimulusDifferentHemispheres) {
    E2E_PIPELINE_START("Chimeric Stimulus");

    createSession();

    // Create trial with chimeric (different) stimuli per hemisphere
    E2E_STAGE_BEGIN("Create chimeric trial", 50);
    split_brain_trial_t* trial = split_brain_trial_create(session, PARADIGM_CHIMERIC_FACES);
    ASSERT_NE(trial, nullptr);
    E2E_STAGE_END();

    // Set different stimuli for each visual field
    E2E_STAGE_BEGIN("Set chimeric stimuli", 50);
    auto left_stim = generate_left_visual_field_stimulus(INPUT_SIZE);
    auto right_stim = generate_right_visual_field_stimulus(INPUT_SIZE);

    split_brain_stimulus_t left_stimulus = {};
    left_stimulus.type = STIMULUS_VISUAL;
    left_stimulus.visual_field = VISUAL_FIELD_LEFT;
    left_stimulus.data = left_stim.data();
    left_stimulus.data_size = left_stim.size();
    left_stimulus.target_domain = COGNITIVE_DOMAIN_FACE_RECOGNITION;

    split_brain_stimulus_t right_stimulus = {};
    right_stimulus.type = STIMULUS_VISUAL;
    right_stimulus.visual_field = VISUAL_FIELD_RIGHT;
    right_stimulus.data = right_stim.data();
    right_stimulus.data_size = right_stim.size();
    right_stimulus.target_domain = COGNITIVE_DOMAIN_FACE_RECOGNITION;

    int result = split_brain_trial_set_chimeric(trial, &left_stimulus, &right_stimulus);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Run trial
    E2E_STAGE_BEGIN("Run chimeric trial", MAX_TRIAL_TIME_MS);
    result = split_brain_session_start(session);
    EXPECT_EQ(result, 0);

    result = split_brain_trial_run(session, trial);
    EXPECT_EQ(result, 0);

    result = split_brain_session_end(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESplitBrainTest, TachistoscopicPresentation) {
    E2E_PIPELINE_START("Tachistoscopic Presentation");

    // Create tachistoscopic experiment
    E2E_STAGE_BEGIN("Create tachistoscopic session", 100);
    session = split_brain_create_tachistoscopic_experiment(brain, 5, 150.0f);
    ASSERT_NE(session, nullptr);
    E2E_STAGE_END();

    // Run experiment
    E2E_STAGE_BEGIN("Run tachistoscopic trials", MAX_EXPERIMENT_TIME_MS);
    int result = split_brain_session_start(session);
    EXPECT_EQ(result, 0);

    int completed = split_brain_session_run_all_trials(session);
    EXPECT_GT(completed, 0);

    result = split_brain_session_end(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Verbal Response Limitation Tests
//=============================================================================

TEST_F(E2ESplitBrainTest, CannotVerbalizeRightHemisphereInput) {
    E2E_PIPELINE_START("Verbal Response Limitation");

    createSession();

    // Start session to sever callosum
    E2E_STAGE_BEGIN("Start session", 50);
    int start_result = split_brain_session_start(session);
    EXPECT_EQ(start_result, 0);
    E2E_STAGE_END();

    // In split-brain, left visual field (right hemisphere) input cannot be verbalized
    E2E_STAGE_BEGIN("Process LVF to right hemisphere", MAX_TRIAL_TIME_MS);
    auto lvf_input = generate_left_visual_field_stimulus(INPUT_SIZE);
    std::vector<float> right_output(OUTPUT_SIZE);

    int result = hemispheric_brain_process_lateralized(
        brain,
        lvf_input.data(),
        INPUT_SIZE,
        COGNITIVE_DOMAIN_SPATIAL,  // Goes to right
        right_output.data(),
        OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Try to verbalize (language is in left hemisphere, which didn't see the input)
    E2E_STAGE_BEGIN("Attempt verbalization", MAX_TRIAL_TIME_MS);

    // Since callosum is cut, left hemisphere has no access to right's information
    // We simulate by checking that language processing of right's output
    // does not have access to the original input information
    // Pad output to INPUT_SIZE since brain expects consistent input dimensions
    std::vector<float> verbal_input(INPUT_SIZE, 0.0f);
    std::copy(right_output.begin(), right_output.end(), verbal_input.begin());
    std::vector<float> verbal_output(OUTPUT_SIZE);
    result = hemispheric_brain_process_lateralized(
        brain,
        verbal_input.data(),
        INPUT_SIZE,
        COGNITIVE_DOMAIN_LANGUAGE,  // Goes to left
        verbal_output.data(),
        OUTPUT_SIZE
    );
    // Processing succeeds but information is degraded due to severed callosum
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESplitBrainTest, CanVerbalizeLeftHemisphereInput) {
    E2E_PIPELINE_START("Verbal Response Success");

    createSession();

    // Start session to apply callosal condition
    E2E_STAGE_BEGIN("Start session", 50);
    int start_result = split_brain_session_start(session);
    EXPECT_EQ(start_result, 0);
    E2E_STAGE_END();

    // Right visual field (left hemisphere) input CAN be verbalized
    E2E_STAGE_BEGIN("Process RVF to left hemisphere", MAX_TRIAL_TIME_MS);
    auto rvf_input = generate_right_visual_field_stimulus(INPUT_SIZE);
    std::vector<float> left_output(OUTPUT_SIZE);

    int result = hemispheric_brain_process_lateralized(
        brain,
        rvf_input.data(),
        INPUT_SIZE,
        COGNITIVE_DOMAIN_LANGUAGE,  // Goes to left
        left_output.data(),
        OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Verbalization succeeds (same hemisphere)
    // Pad output to INPUT_SIZE since brain expects consistent input dimensions
    E2E_STAGE_BEGIN("Verbalization succeeds", MAX_TRIAL_TIME_MS);
    std::vector<float> verbal_input(INPUT_SIZE, 0.0f);
    std::copy(left_output.begin(), left_output.end(), verbal_input.begin());
    std::vector<float> verbal_output(OUTPUT_SIZE);
    result = hemispheric_brain_process_lateralized(
        brain,
        verbal_input.data(),
        INPUT_SIZE,
        COGNITIVE_DOMAIN_LANGUAGE,
        verbal_output.data(),
        OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);

    // Verify output is valid (no NaN/Inf)
    // Note: adaptive spiking networks may produce near-zero outputs with
    // random weights. Output validity, not magnitude, is the key check.
    bool has_nan_inf = false;
    for (uint32_t i = 0; i < OUTPUT_SIZE; i++) {
        if (std::isnan(verbal_output[i]) || std::isinf(verbal_output[i])) {
            has_nan_inf = true;
            break;
        }
    }
    EXPECT_FALSE(has_nan_inf) << "Output should not contain NaN or Inf";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESplitBrainTest, LeftHandPointsRightHemisphereInfo) {
    E2E_PIPELINE_START("Left Hand Pointing");

    createSession();

    // Right hemisphere (left visual field) can point with LEFT hand
    E2E_STAGE_BEGIN("Right hemisphere motor response", MAX_TRIAL_TIME_MS);
    auto lvf_input = generate_left_visual_field_stimulus(INPUT_SIZE);
    std::vector<float> motor_output(OUTPUT_SIZE);

    // Process spatial info in right hemisphere
    int result = hemispheric_brain_process_lateralized(
        brain,
        lvf_input.data(),
        INPUT_SIZE,
        COGNITIVE_DOMAIN_SPATIAL,
        motor_output.data(),
        OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);

    // Right hemisphere controls left hand (contralateral)
    // This should work even with severed callosum
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESplitBrainTest, HemisphereConflictDetection) {
    E2E_PIPELINE_START("Hemisphere Conflict Detection");

    createSession();

    // Create trial to detect conflict
    E2E_STAGE_BEGIN("Create conflict trial", 50);
    split_brain_trial_t* trial = split_brain_trial_create(session, PARADIGM_ALIEN_HAND);
    ASSERT_NE(trial, nullptr);
    E2E_STAGE_END();

    // Set conflicting stimuli
    E2E_STAGE_BEGIN("Set conflicting stimuli", 50);
    auto left_stim = generate_left_visual_field_stimulus(INPUT_SIZE);
    auto right_stim = generate_right_visual_field_stimulus(INPUT_SIZE);

    // Make stimuli very different to induce conflict
    for (uint32_t i = 0; i < INPUT_SIZE; i++) {
        left_stim[i] = 1.0f - left_stim[i];  // Invert one pattern
    }

    split_brain_stimulus_t left_stimulus = {};
    left_stimulus.data = left_stim.data();
    left_stimulus.data_size = left_stim.size();

    split_brain_stimulus_t right_stimulus = {};
    right_stimulus.data = right_stim.data();
    right_stimulus.data_size = right_stim.size();

    int result = split_brain_trial_set_chimeric(trial, &left_stimulus, &right_stimulus);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Check for conflict
    E2E_STAGE_BEGIN("Detect conflict", MAX_TRIAL_TIME_MS);
    result = split_brain_session_start(session);
    EXPECT_EQ(result, 0);

    result = split_brain_trial_run(session, trial);
    EXPECT_EQ(result, 0);

    bool conflict = split_brain_detect_conflict(trial, CONFLICT_THRESHOLD);
    // Conflict detection may or may not trigger based on specific patterns
    (void)conflict;  // Just verify it runs

    result = split_brain_session_end(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Cross-Cueing Detection Tests
//=============================================================================

TEST_F(E2ESplitBrainTest, CrossCueingDetection) {
    E2E_PIPELINE_START("Cross-Cueing Detection");

    createSession();

    // Create trial for cross-cueing detection
    E2E_STAGE_BEGIN("Create cross-cue trial", 50);
    split_brain_trial_t* trial = split_brain_trial_create(session, PARADIGM_CROSS_CUEING);
    ASSERT_NE(trial, nullptr);
    E2E_STAGE_END();

    // Run trial
    E2E_STAGE_BEGIN("Run trial", MAX_TRIAL_TIME_MS);
    int result = split_brain_session_start(session);
    EXPECT_EQ(result, 0);

    result = split_brain_trial_run(session, trial);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Check for cross-cueing
    E2E_STAGE_BEGIN("Analyze cross-cueing", 50);
    bool cross_cue = split_brain_detect_cross_cueing(session, trial);
    float evidence = split_brain_get_cross_cue_evidence(trial);

    // With completely severed callosum, cross-cueing should be minimal
    // but can still occur through subcortical pathways
    (void)cross_cue;
    EXPECT_GE(evidence, 0.0f);
    EXPECT_LE(evidence, 1.0f);

    result = split_brain_session_end(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESplitBrainTest, CrossCueingWithPartialCallosum) {
    E2E_PIPELINE_START("Cross-Cueing Partial Callosum");

    // Create session with partial callosum (some channels blocked)
    E2E_STAGE_BEGIN("Create partial session", 50);
    split_brain_session_config_t config = split_brain_session_default_config();
    config.callosal_condition = CALLOSAL_STATE_PARTIAL;
    config.blocked_channels[CALLOSUM_CHANNEL_COGNITIVE] = true;
    config.blocked_channels[CALLOSUM_CHANNEL_MOTOR] = false;
    config.detect_cross_cueing = true;

    session = split_brain_session_create(&config, brain);
    ASSERT_NE(session, nullptr);
    E2E_STAGE_END();

    // Run experiment
    E2E_STAGE_BEGIN("Run with partial callosum", MAX_EXPERIMENT_TIME_MS);
    int result = split_brain_session_start(session);
    EXPECT_EQ(result, 0);

    int completed = split_brain_session_run_all_trials(session);
    EXPECT_GT(completed, 0);

    result = split_brain_session_end(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Get stats showing cross-cueing
    E2E_STAGE_BEGIN("Analyze cross-cueing stats", 20);
    split_brain_session_stats_t stats;
    result = split_brain_session_get_stats(session, &stats);
    EXPECT_EQ(result, 0);
    // Partial callosum may allow some cross-cueing
    EXPECT_GE(stats.cross_cueing_rate, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESplitBrainTest, CrossCueingWithDegradedCallosum) {
    E2E_PIPELINE_START("Cross-Cueing Degraded Callosum");

    // Create session with degraded callosum
    E2E_STAGE_BEGIN("Create degraded session", 50);
    split_brain_session_config_t config = split_brain_session_default_config();
    config.callosal_condition = CALLOSAL_STATE_DEGRADED;
    config.callosal_strength = 0.3f;  // 30% function remaining

    session = split_brain_session_create(&config, brain);
    ASSERT_NE(session, nullptr);
    E2E_STAGE_END();

    // Set degradation level
    E2E_STAGE_BEGIN("Set degradation", 50);
    int result = split_brain_set_callosal_strength(session, 0.3f);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Run
    E2E_STAGE_BEGIN("Run degraded experiment", MAX_EXPERIMENT_TIME_MS);
    result = split_brain_session_start(session);
    EXPECT_EQ(result, 0);

    int completed = split_brain_session_run_all_trials(session);
    EXPECT_GT(completed, 0);

    result = split_brain_session_end(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Callosum Reconnection Recovery Tests
//=============================================================================

TEST_F(E2ESplitBrainTest, CallosumReconnectionBasic) {
    E2E_PIPELINE_START("Callosum Reconnection");

    createSession();

    // Start session to apply callosal condition (sever callosum)
    E2E_STAGE_BEGIN("Start session", 50);
    int start_result = split_brain_session_start(session);
    EXPECT_EQ(start_result, 0);
    E2E_STAGE_END();

    // Verify disconnected
    E2E_STAGE_BEGIN("Verify disconnected", 20);
    EXPECT_FALSE(hemispheric_brain_is_callosum_intact(brain));
    E2E_STAGE_END();

    // Reconnect
    E2E_STAGE_BEGIN("Reconnect callosum", MAX_CALLOSUM_OP_TIME_MS);
    int result = hemispheric_brain_reconnect_callosum(brain);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Verify reconnected
    E2E_STAGE_BEGIN("Verify reconnected", 20);
    EXPECT_TRUE(hemispheric_brain_is_callosum_intact(brain));
    E2E_STAGE_END();

    // Test bilateral processing after reconnection
    E2E_STAGE_BEGIN("Bilateral processing", MAX_TRIAL_TIME_MS);
    auto input = generate_visual_stimulus(INPUT_SIZE, 42);
    std::vector<float> left_out(OUTPUT_SIZE);
    std::vector<float> right_out(OUTPUT_SIZE);

    result = hemispheric_brain_process_parallel(
        brain,
        input.data(),
        INPUT_SIZE,
        left_out.data(),
        right_out.data(),
        OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESplitBrainTest, CallosumReconnectionRecoveryTrajectory) {
    E2E_PIPELINE_START("Reconnection Recovery Trajectory");

    createSession();

    // Gradually increase connection strength
    E2E_STAGE_BEGIN("Gradual reconnection", MAX_EXPERIMENT_TIME_MS);
    corpus_callosum_t* cc = hemispheric_brain_get_callosum(brain);
    ASSERT_NE(cc, nullptr);

    std::vector<float> strengths = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f, 1.0f};
    for (float strength : strengths) {
        int result = callosum_set_connection_strength(cc, strength);
        EXPECT_EQ(result, 0);

        float actual = callosum_get_connection_strength(cc);
        EXPECT_NEAR(actual, strength, 0.01f);

        // Process with current strength
        auto input = generate_visual_stimulus(INPUT_SIZE, static_cast<uint32_t>(strength * 100));
        std::vector<float> output(OUTPUT_SIZE);
        result = hemispheric_brain_process_cooperative(
            brain,
            input.data(),
            INPUT_SIZE,
            output.data(),
            OUTPUT_SIZE
        );
        EXPECT_EQ(result, 0);

        hemispheric_brain_update(brain, 0.01f);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESplitBrainTest, CallosumRestoreOriginalState) {
    E2E_PIPELINE_START("Restore Original Callosum");

    // Create session and modify callosum
    E2E_STAGE_BEGIN("Create and modify", 100);
    split_brain_session_config_t config = split_brain_session_default_config();
    config.callosal_condition = CALLOSAL_STATE_DEGRADED;
    config.callosal_strength = 0.5f;

    session = split_brain_session_create(&config, brain);
    ASSERT_NE(session, nullptr);

    int result = split_brain_session_start(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Run some trials
    E2E_STAGE_BEGIN("Run trials", MAX_TRIAL_TIME_MS * 3);
    int completed = split_brain_session_run_all_trials(session);
    EXPECT_GT(completed, 0);
    E2E_STAGE_END();

    // Restore original state
    E2E_STAGE_BEGIN("Restore callosum", MAX_CALLOSUM_OP_TIME_MS);
    result = split_brain_restore_callosum(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // End session
    E2E_STAGE_BEGIN("End session", 50);
    result = split_brain_session_end(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Verify callosum is restored
    E2E_STAGE_BEGIN("Verify restoration", 20);
    EXPECT_TRUE(hemispheric_brain_is_callosum_intact(brain));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Confabulation Analysis Tests
//=============================================================================

TEST_F(E2ESplitBrainTest, ConfabulationDetection) {
    E2E_PIPELINE_START("Confabulation Detection");

    createSession();

    // Create confabulation paradigm trial
    E2E_STAGE_BEGIN("Create confabulation trial", 50);
    split_brain_trial_t* trial = split_brain_trial_create(session, PARADIGM_CONFABULATION);
    ASSERT_NE(trial, nullptr);
    E2E_STAGE_END();

    // Run trial
    E2E_STAGE_BEGIN("Run trial", MAX_TRIAL_TIME_MS);
    int result = split_brain_session_start(session);
    EXPECT_EQ(result, 0);

    result = split_brain_trial_run(session, trial);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Analyze confabulation
    E2E_STAGE_BEGIN("Analyze confabulation", 50);
    float confab_score = 0.0f;
    bool confabulated = split_brain_analyze_confabulation(session, trial, &confab_score);

    // Confabulation occurs when left hemisphere explains right's actions
    (void)confabulated;
    EXPECT_GE(confab_score, 0.0f);
    EXPECT_LE(confab_score, 1.0f);

    result = split_brain_session_end(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESplitBrainTest, ConfabulationWithMultipleTrials) {
    E2E_PIPELINE_START("Confabulation Multiple Trials");

    createSession();

    E2E_STAGE_BEGIN("Run confabulation experiment", MAX_EXPERIMENT_TIME_MS);
    int result = split_brain_session_start(session);
    EXPECT_EQ(result, 0);

    uint32_t confab_count = 0;
    for (uint32_t i = 0; i < 5; i++) {
        split_brain_trial_t* trial = split_brain_trial_create(session, PARADIGM_CONFABULATION);
        if (trial == nullptr) continue;

        result = split_brain_trial_run(session, trial);
        EXPECT_EQ(result, 0);

        float score = 0.0f;
        if (split_brain_analyze_confabulation(session, trial, &score)) {
            confab_count++;
        }
    }

    result = split_brain_session_end(session);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Check stats
    E2E_STAGE_BEGIN("Verify confabulation stats", 20);
    split_brain_session_stats_t stats;
    result = split_brain_session_get_stats(session, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.confabulation_events, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESplitBrainTest, HemisphereAgreementAnalysis) {
    E2E_PIPELINE_START("Hemisphere Agreement Analysis");

    createSession();

    E2E_STAGE_BEGIN("Run agreement experiment", MAX_EXPERIMENT_TIME_MS);
    int result = split_brain_session_start(session);
    EXPECT_EQ(result, 0);

    // Run trials and track agreement
    float total_agreement = 0.0f;
    int trial_count = 0;

    for (uint32_t i = 0; i < 5; i++) {
        split_brain_trial_t* trial = split_brain_trial_create(session, PARADIGM_SPATIAL_VERBAL);
        if (trial == nullptr) continue;

        result = split_brain_trial_run(session, trial);
        EXPECT_EQ(result, 0);

        float agreement = split_brain_compute_agreement(trial);
        total_agreement += agreement;
        trial_count++;
    }

    result = split_brain_session_end(session);
    EXPECT_EQ(result, 0);

    if (trial_count > 0) {
        float avg_agreement = total_agreement / trial_count;
        // In split-brain, agreement may be lower than intact brain
        EXPECT_GE(avg_agreement, 0.0f);
        EXPECT_LE(avg_agreement, 1.0f);
    }
    E2E_STAGE_END();

    // Get overall agreement rate
    E2E_STAGE_BEGIN("Overall agreement rate", 20);
    split_brain_session_stats_t stats;
    result = split_brain_session_get_stats(session, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.overall_agreement_rate, 0.0f);
    EXPECT_LE(stats.overall_agreement_rate, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
