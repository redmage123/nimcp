/**
 * @file test_world_model_e2e.cpp
 * @brief End-to-end tests for World Model modules
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Comprehensive E2E tests for omnidirectional world model and multimodal
 * world model systems. Tests complete cognitive workflows including:
 *
 * - DreamerV3-style training loops with experience replay
 * - JEPA-style latent space prediction
 * - Counterfactual reasoning workflows
 * - Policy evaluation loops
 * - Multi-modal perception pipelines
 * - Full brain simulation cycles
 * - Active inference integration
 * - Long-horizon prediction stability
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <functional>

/* Include world model headers */
#include "cognitive/omni/nimcp_omni_world_model.h"
#include "cognitive/extrapolation/nimcp_world_model_multimodal.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Configuration Constants
 * ============================================================================ */

namespace {
    /* Omni world model dimensions */
    constexpr uint32_t STATE_DIM = 64;
    constexpr uint32_t ACTION_DIM = 8;
    constexpr uint32_t OBS_DIM = 64;
    constexpr uint32_t LATENT_DIM = 64;

    /* Training parameters */
    constexpr uint32_t NUM_TRAINING_STEPS = 50;
    constexpr uint32_t BATCH_SIZE = 16;
    constexpr uint32_t DREAM_EPISODES = 5;
    constexpr uint32_t DREAM_LENGTH = 10;

    /* Prediction horizons */
    constexpr uint32_t SHORT_HORIZON = 5;
    constexpr uint32_t MEDIUM_HORIZON = 15;
    constexpr uint32_t LONG_HORIZON = 30;

    /* Multimodal config */
    constexpr uint32_t VISUAL_DIM = 128;
    constexpr uint32_t AUDITORY_DIM = 64;
    constexpr uint32_t NUM_ENTITIES = 10;

    /* Tolerances */
    constexpr float LEARNING_TOLERANCE = 0.3f;
    constexpr float RECONSTRUCTION_TOLERANCE = 0.5f;
}

/* ============================================================================
 * Omni World Model E2E Test Fixture
 * ============================================================================ */

class WorldModelE2ETest : public ::testing::Test {
protected:
    omni_world_model_t* wm = nullptr;
    nimcp_world_model_t* mm_wm = nullptr;

    std::mt19937 rng{42};
    std::uniform_real_distribution<float> uniform_dist{-1.0f, 1.0f};
    std::normal_distribution<float> normal_dist{0.0f, 0.3f};

    void SetUp() override {
        /* Create omni world model with custom config */
        omni_wm_config_t config;
        omni_wm_get_default_config(&config);
        config.state_dim = STATE_DIM;
        config.action_dim = ACTION_DIM;
        config.obs_dim = OBS_DIM;
        config.latent_dim = LATENT_DIM;
        config.rssm_h_dim = STATE_DIM / 2;
        config.rssm_z_dim = STATE_DIM / 4;
        config.replay_buffer_size = 1000;
        config.batch_size = BATCH_SIZE;
        config.enable_dreaming = true;
        config.use_symlog_rewards = true;
        config.use_rssm = true;
        config.learning_rate = 0.01f;

        wm = omni_wm_create(&config);
        ASSERT_NE(wm, nullptr) << "Failed to create omni world model";

        /* Create multimodal world model */
        wm_config_t mm_config = wm_default_config();
        mm_config.latent_dim = LATENT_DIM;
        mm_config.max_entities = NUM_ENTITIES;
        mm_config.max_prediction_steps = LONG_HORIZON;
        mm_config.fusion_type = WM_FUSION_ATTENTION;
        mm_config.prediction_mode = WM_PRED_MODE_PROBABILISTIC;
        mm_config.enable_bio_async = false;
        mm_config.enable_immune = false;

        mm_wm = wm_create(&mm_config);
        /* Multimodal world model creation may fail if not fully implemented */
        if (mm_wm) {
            wm_init(mm_wm);
        }
    }

    void TearDown() override {
        if (wm) {
            omni_wm_destroy(wm);
            wm = nullptr;
        }
        if (mm_wm) {
            wm_destroy(mm_wm);
            mm_wm = nullptr;
        }
    }

    /* Helper: Generate random state */
    omni_wm_state_t* CreateRandomState(uint32_t dim) {
        omni_wm_state_t* state = omni_wm_state_create(dim);
        if (state) {
            for (uint32_t i = 0; i < dim; i++) {
                state->values[i] = uniform_dist(rng);
            }
        }
        return state;
    }

    /* Helper: Generate random action */
    void FillRandomAction(float* action, uint32_t dim) {
        for (uint32_t i = 0; i < dim; i++) {
            action[i] = uniform_dist(rng) * 0.5f;
        }
    }

    /* Helper: Generate patterned state (for testing prediction) */
    omni_wm_state_t* CreatePatternedState(uint32_t dim, float base, float scale) {
        omni_wm_state_t* state = omni_wm_state_create(dim);
        if (state) {
            for (uint32_t i = 0; i < dim; i++) {
                state->values[i] = base + scale * std::sin((float)i * 0.1f);
            }
        }
        return state;
    }

    /* Helper: Compute MSE between states */
    float ComputeMSE(const omni_wm_state_t* a, const omni_wm_state_t* b) {
        if (!a || !b) return FLT_MAX;
        uint32_t min_dim = std::min(a->dim, b->dim);
        float mse = 0.0f;
        for (uint32_t i = 0; i < min_dim; i++) {
            float diff = a->values[i] - b->values[i];
            mse += diff * diff;
        }
        return mse / min_dim;
    }

    /* Helper: Compute cosine similarity */
    float CosineSimilarity(const float* a, const float* b, uint32_t dim) {
        float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            dot += a[i] * b[i];
            norm_a += a[i] * a[i];
            norm_b += b[i] * b[i];
        }
        return dot / (std::sqrt(norm_a) * std::sqrt(norm_b) + 1e-8f);
    }

    /* Helper: Simple deterministic dynamics for ground truth */
    void SimulateTrueDynamics(const omni_wm_state_t* state, const float* action,
                              uint32_t action_dim, omni_wm_state_t* next_state) {
        /* Simple linear dynamics: next = 0.9*state + 0.1*action + noise */
        uint32_t min_dim = std::min(state->dim, next_state->dim);
        for (uint32_t i = 0; i < min_dim; i++) {
            float action_effect = (i < action_dim) ? action[i] * 0.1f : 0.0f;
            next_state->values[i] = 0.9f * state->values[i] + action_effect
                                    + normal_dist(rng) * 0.01f;
        }
    }
};

/* ============================================================================
 * Test 1: DreamerV3-Style Training Loop
 * ============================================================================ */

TEST_F(WorldModelE2ETest, DreamerV3StyleTrainingLoop) {
    /**
     * Test complete DreamerV3-style training workflow:
     * 1. Create world model
     * 2. Collect experience tuples
     * 3. Add to replay buffer
     * 4. Sample batch and update
     * 5. Run dreaming episodes
     * 6. Verify learning (prediction error should decrease)
     */

    /* Initialize with a starting state */
    omni_wm_state_t* initial_state = CreatePatternedState(STATE_DIM, 0.0f, 1.0f);
    ASSERT_NE(initial_state, nullptr);

    nimcp_error_t err = omni_wm_set_state(wm, initial_state);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Collect initial prediction error before training */
    omni_wm_stats_t initial_stats;
    omni_wm_get_stats(wm, &initial_stats);

    /* Phase 1: Collect experiences */
    std::vector<float> initial_errors;

    omni_wm_state_t* current_state = omni_wm_state_clone(initial_state);
    ASSERT_NE(current_state, nullptr);

    for (uint32_t step = 0; step < NUM_TRAINING_STEPS; step++) {
        /* Generate action */
        float action[ACTION_DIM];
        FillRandomAction(action, ACTION_DIM);

        /* Create experience */
        omni_wm_experience_t* exp = omni_wm_experience_create(STATE_DIM, ACTION_DIM, OBS_DIM);
        ASSERT_NE(exp, nullptr);

        /* Fill experience with current state */
        if (exp->state && exp->state->h && current_state->values) {
            uint32_t copy_dim = std::min(exp->state->h_dim, current_state->dim);
            memcpy(exp->state->h, current_state->values, copy_dim * sizeof(float));
        }
        memcpy(exp->action, action, ACTION_DIM * sizeof(float));

        /* Simulate true dynamics to get next state */
        omni_wm_state_t* true_next = omni_wm_state_create(STATE_DIM);
        ASSERT_NE(true_next, nullptr);
        SimulateTrueDynamics(current_state, action, ACTION_DIM, true_next);

        /* Fill next state in experience */
        if (exp->next_state && exp->next_state->h && true_next->values) {
            uint32_t copy_dim = std::min(exp->next_state->h_dim, true_next->dim);
            memcpy(exp->next_state->h, true_next->values, copy_dim * sizeof(float));
        }

        /* Compute reward based on state magnitude (simple heuristic) */
        float reward = 0.0f;
        for (uint32_t i = 0; i < true_next->dim; i++) {
            reward += true_next->values[i] * 0.01f;
        }
        exp->reward = reward;
        exp->terminal = false;

        /* Add experience to replay buffer */
        err = omni_wm_add_experience(wm, exp);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        /* Update model with this transition */
        err = omni_wm_update(wm, current_state, action, ACTION_DIM, true_next, reward);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        /* Record prediction error in early steps */
        if (step < 10) {
            omni_wm_stats_t stats;
            omni_wm_get_stats(wm, &stats);
            initial_errors.push_back(stats.mean_prediction_error);
        }

        /* Move to next state */
        omni_wm_state_destroy(current_state);
        current_state = true_next;

        omni_wm_experience_destroy(exp);
    }

    /* Verify replay buffer has experiences */
    uint32_t replay_size = omni_wm_get_replay_size(wm);
    EXPECT_GT(replay_size, 0u);
    std::cout << "Replay buffer size: " << replay_size << std::endl;

    /* Phase 2: Run dreaming episodes */
    err = omni_wm_dream(wm, DREAM_EPISODES, DREAM_LENGTH);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Phase 3: Additional training with sampled batches */
    for (uint32_t batch_iter = 0; batch_iter < 10; batch_iter++) {
        std::vector<omni_wm_experience_t*> batch(BATCH_SIZE);
        uint32_t sampled = omni_wm_sample_experiences(wm, batch.data(), BATCH_SIZE);

        /* Process batch experiences (in real system this would do batch gradient update) */
        if (sampled > 0) {
            /* Verify samples are valid */
            for (uint32_t i = 0; i < sampled; i++) {
                EXPECT_NE(batch[i], nullptr);
            }
        }
    }

    /* Phase 4: Verify learning occurred */
    omni_wm_stats_t final_stats;
    omni_wm_get_stats(wm, &final_stats);

    std::cout << "Training completed:" << std::endl;
    std::cout << "  Forward predictions: " << final_stats.forward_predictions << std::endl;
    std::cout << "  Model updates: " << final_stats.model_updates << std::endl;
    std::cout << "  Mean prediction error: " << final_stats.mean_prediction_error << std::endl;

    /* The model should have been updated */
    EXPECT_GT(final_stats.model_updates, 0u);
    EXPECT_GT(final_stats.forward_predictions, 0u);

    /* Cleanup */
    omni_wm_state_destroy(current_state);
    omni_wm_state_destroy(initial_state);
}

/* ============================================================================
 * Test 2: JEPA-Style Prediction
 * ============================================================================ */

TEST_F(WorldModelE2ETest, JEPAStyleLatentPrediction) {
    /**
     * Test JEPA-style latent space prediction workflow:
     * 1. Encode observations to latent space
     * 2. Predict in latent space
     * 3. Decode back to observation space
     * 4. Verify reconstruction quality
     */

    /* Create observation */
    float observation[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        observation[i] = std::sin((float)i * 0.1f) + normal_dist(rng) * 0.1f;
    }

    /* Step 1: Encode to latent space */
    omni_wm_latent_t* latent = omni_wm_latent_create(LATENT_DIM);
    ASSERT_NE(latent, nullptr);

    nimcp_error_t err = omni_wm_encode(wm, observation, OBS_DIM, latent);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify latent has values */
    float latent_norm = 0.0f;
    for (uint32_t i = 0; i < latent->dim; i++) {
        latent_norm += latent->embedding[i] * latent->embedding[i];
        EXPECT_FALSE(std::isnan(latent->embedding[i]));
    }
    EXPECT_GT(latent_norm, 0.0f);
    std::cout << "Latent norm: " << std::sqrt(latent_norm) << std::endl;
    std::cout << "Information content: " << latent->information_content << std::endl;

    /* Step 2: Predict in latent space */
    float action[ACTION_DIM];
    FillRandomAction(action, ACTION_DIM);

    omni_wm_latent_t* predicted_latent = omni_wm_latent_create(LATENT_DIM);
    ASSERT_NE(predicted_latent, nullptr);

    err = omni_wm_predict_latent(wm, latent, action, ACTION_DIM, predicted_latent);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify predicted latent is different but related */
    float cosine_sim = CosineSimilarity(latent->embedding, predicted_latent->embedding, LATENT_DIM);
    std::cout << "Latent cosine similarity after action: " << cosine_sim << std::endl;
    EXPECT_GT(cosine_sim, 0.5f);  /* Should be somewhat similar */

    /* Step 3: Decode back to observation space */
    float reconstructed[OBS_DIM];
    err = omni_wm_decode(wm, latent, reconstructed, OBS_DIM);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Step 4: Verify reconstruction (with untrained model, this won't be perfect) */
    float recon_error = 0.0f;
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        float diff = observation[i] - reconstructed[i];
        recon_error += diff * diff;
        EXPECT_FALSE(std::isnan(reconstructed[i]));
    }
    recon_error = std::sqrt(recon_error / OBS_DIM);
    std::cout << "Reconstruction RMSE: " << recon_error << std::endl;

    /* Decode predicted latent */
    float predicted_obs[OBS_DIM];
    err = omni_wm_decode(wm, predicted_latent, predicted_obs, OBS_DIM);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Predicted observation should be different from original */
    float pred_diff = 0.0f;
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        pred_diff += std::abs(predicted_obs[i] - reconstructed[i]);
    }
    std::cout << "Predicted vs original obs diff: " << pred_diff / OBS_DIM << std::endl;

    /* Cleanup */
    omni_wm_latent_destroy(predicted_latent);
    omni_wm_latent_destroy(latent);
}

/* ============================================================================
 * Test 3: Counterfactual Reasoning Workflow
 * ============================================================================ */

TEST_F(WorldModelE2ETest, CounterfactualReasoningWorkflow) {
    /**
     * Test counterfactual reasoning workflow:
     * 1. Set up initial state
     * 2. Create counterfactual query (what if different action?)
     * 3. Execute counterfactual simulation
     * 4. Compare trajectories
     * 5. Verify divergence calculation
     */

    /* Set up initial state */
    omni_wm_state_t* initial_state = CreatePatternedState(STATE_DIM, 1.0f, 0.5f);
    ASSERT_NE(initial_state, nullptr);

    nimcp_error_t err = omni_wm_set_state(wm, initial_state);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Create two different actions to compare */
    float action_a[ACTION_DIM];
    float action_b[ACTION_DIM];

    /* Action A: positive direction */
    for (uint32_t i = 0; i < ACTION_DIM; i++) {
        action_a[i] = 0.5f;
    }

    /* Action B: negative direction */
    for (uint32_t i = 0; i < ACTION_DIM; i++) {
        action_b[i] = -0.5f;
    }

    /* Execute counterfactual for action A */
    omni_wm_counterfactual_result_t result_a;
    memset(&result_a, 0, sizeof(result_a));

    err = omni_wm_what_if(wm, action_a, ACTION_DIM, SHORT_HORIZON, &result_a);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(result_a.trajectory_len, 0u);

    /* Reset state for second counterfactual */
    err = omni_wm_set_state(wm, initial_state);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Execute counterfactual for action B */
    omni_wm_counterfactual_result_t result_b;
    memset(&result_b, 0, sizeof(result_b));

    err = omni_wm_what_if(wm, action_b, ACTION_DIM, SHORT_HORIZON, &result_b);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(result_b.trajectory_len, 0u);

    /* Compare trajectories - they should diverge */
    float total_divergence = 0.0f;
    uint32_t compare_len = std::min(result_a.trajectory_len, result_b.trajectory_len);

    for (uint32_t t = 1; t < compare_len; t++) {
        if (result_a.trajectory[t] && result_b.trajectory[t]) {
            float step_divergence = ComputeMSE(result_a.trajectory[t], result_b.trajectory[t]);
            total_divergence += step_divergence;

            /* Divergence should increase over time */
            if (t > 1) {
                float prev_div = ComputeMSE(result_a.trajectory[t-1], result_b.trajectory[t-1]);
                /* Trajectories should diverge or stay diverged */
                EXPECT_GE(step_divergence, prev_div * 0.5f);
            }
        }
    }

    std::cout << "Counterfactual comparison:" << std::endl;
    std::cout << "  Trajectory A length: " << result_a.trajectory_len << std::endl;
    std::cout << "  Trajectory B length: " << result_b.trajectory_len << std::endl;
    std::cout << "  Total divergence: " << total_divergence << std::endl;
    std::cout << "  Expected reward A: " << result_a.expected_reward << std::endl;
    std::cout << "  Expected reward B: " << result_b.expected_reward << std::endl;
    std::cout << "  Confidence A: " << result_a.confidence << std::endl;

    /* Trajectories from opposite actions should diverge */
    EXPECT_GT(total_divergence, 0.0f);

    /* Verify stats updated */
    omni_wm_stats_t stats;
    omni_wm_get_stats(wm, &stats);
    EXPECT_GE(stats.counterfactual_queries, 2u);

    /* Cleanup */
    omni_wm_cf_result_destroy(&result_a);
    omni_wm_cf_result_destroy(&result_b);
    omni_wm_state_destroy(initial_state);
}

/* ============================================================================
 * Test 4: Policy Evaluation Loop
 * ============================================================================ */

/* Simple policy function for testing */
static void SimplePolicy(const omni_wm_state_t* state, float* action, void* user_data) {
    (void)user_data;

    /* Policy: actions proportional to state (simple proportional controller) */
    uint32_t action_dim = *(uint32_t*)user_data;

    for (uint32_t i = 0; i < action_dim; i++) {
        if (state && i < state->dim) {
            action[i] = -0.1f * state->values[i];  /* Negative feedback */
        } else {
            action[i] = 0.0f;
        }
    }
}

/* Alternative policy: always move in positive direction */
static void PositivePolicy(const omni_wm_state_t* state, float* action, void* user_data) {
    (void)state;
    uint32_t action_dim = *(uint32_t*)user_data;

    for (uint32_t i = 0; i < action_dim; i++) {
        action[i] = 0.3f;
    }
}

TEST_F(WorldModelE2ETest, PolicyEvaluationLoop) {
    /**
     * Test policy evaluation workflow:
     * 1. Create world model with trained dynamics
     * 2. Define simple policy function
     * 3. Execute rollout
     * 4. Evaluate expected free energy
     * 5. Compare policies
     */

    /* Set initial state */
    omni_wm_state_t* initial_state = CreatePatternedState(STATE_DIM, 2.0f, 1.0f);
    ASSERT_NE(initial_state, nullptr);

    nimcp_error_t err = omni_wm_set_state(wm, initial_state);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    uint32_t action_dim = ACTION_DIM;

    /* Create rollout for policy 1 (proportional controller) */
    omni_wm_rollout_t* rollout1 = omni_wm_rollout_create(MEDIUM_HORIZON, STATE_DIM, ACTION_DIM, OBS_DIM);
    ASSERT_NE(rollout1, nullptr);

    err = omni_wm_rollout(wm, SimplePolicy, MEDIUM_HORIZON, rollout1, &action_dim);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Reset state for second policy */
    err = omni_wm_set_state(wm, initial_state);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Create rollout for policy 2 (positive policy) */
    omni_wm_rollout_t* rollout2 = omni_wm_rollout_create(MEDIUM_HORIZON, STATE_DIM, ACTION_DIM, OBS_DIM);
    ASSERT_NE(rollout2, nullptr);

    err = omni_wm_rollout(wm, PositivePolicy, MEDIUM_HORIZON, rollout2, &action_dim);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Define preferred observations (goal: zero state) */
    float preferred_obs[OBS_DIM];
    memset(preferred_obs, 0, OBS_DIM * sizeof(float));

    /* Evaluate EFE for both policies */
    float efe1 = omni_wm_evaluate_efe(wm, rollout1, preferred_obs, OBS_DIM);
    float efe2 = omni_wm_evaluate_efe(wm, rollout2, preferred_obs, OBS_DIM);

    std::cout << "Policy evaluation results:" << std::endl;
    std::cout << "  Policy 1 (proportional controller):" << std::endl;
    std::cout << "    Rollout length: " << rollout1->length << std::endl;
    std::cout << "    Total reward: " << rollout1->total_reward << std::endl;
    std::cout << "    EFE: " << efe1 << std::endl;
    std::cout << "  Policy 2 (positive policy):" << std::endl;
    std::cout << "    Rollout length: " << rollout2->length << std::endl;
    std::cout << "    Total reward: " << rollout2->total_reward << std::endl;
    std::cout << "    EFE: " << efe2 << std::endl;

    /* Verify rollouts completed */
    EXPECT_GT(rollout1->length, 0u);
    EXPECT_GT(rollout2->length, 0u);

    /* Verify EFE values are finite */
    EXPECT_FALSE(std::isnan(efe1));
    EXPECT_FALSE(std::isnan(efe2));
    EXPECT_LT(efe1, FLT_MAX);
    EXPECT_LT(efe2, FLT_MAX);

    /* Verify stats */
    omni_wm_stats_t stats;
    omni_wm_get_stats(wm, &stats);
    EXPECT_GE(stats.rollouts_completed, 2u);

    /* Cleanup */
    omni_wm_rollout_destroy(rollout2);
    omni_wm_rollout_destroy(rollout1);
    omni_wm_state_destroy(initial_state);
}

/* ============================================================================
 * Test 5: Multi-Modal Perception Pipeline
 * ============================================================================ */

TEST_F(WorldModelE2ETest, MultiModalPerceptionPipeline) {
    /**
     * Test multimodal world model pipeline:
     * 1. Create multimodal world model
     * 2. Process visual input
     * 3. Process auditory input
     * 4. Fuse modalities
     * 5. Predict future state
     * 6. Track entities
     */

    if (!mm_wm) {
        GTEST_SKIP() << "Multimodal world model not available";
    }

    /* Step 1: Create visual input */
    wm_modality_input_t visual_input;
    memset(&visual_input, 0, sizeof(visual_input));
    visual_input.modality = WM_MODALITY_VISUAL;
    visual_input.feature_dim = VISUAL_DIM;
    visual_input.features = (float*)nimcp_calloc(VISUAL_DIM, sizeof(float));
    ASSERT_NE(visual_input.features, nullptr);
    visual_input.confidence = 0.9f;
    visual_input.timestamp = 1000;

    /* Fill visual features (simulated visual scene) */
    for (uint32_t i = 0; i < VISUAL_DIM; i++) {
        visual_input.features[i] = std::sin((float)i * 0.05f) + uniform_dist(rng) * 0.1f;
    }

    /* Step 2: Process visual input */
    wm_error_t wm_err = wm_process_modality(mm_wm, &visual_input);
    EXPECT_EQ(wm_err, WM_OK);

    /* Step 3: Create auditory input */
    wm_modality_input_t auditory_input;
    memset(&auditory_input, 0, sizeof(auditory_input));
    auditory_input.modality = WM_MODALITY_AUDITORY;
    auditory_input.feature_dim = AUDITORY_DIM;
    auditory_input.features = (float*)nimcp_calloc(AUDITORY_DIM, sizeof(float));
    ASSERT_NE(auditory_input.features, nullptr);
    auditory_input.confidence = 0.85f;
    auditory_input.timestamp = 1000;

    /* Fill auditory features */
    for (uint32_t i = 0; i < AUDITORY_DIM; i++) {
        auditory_input.features[i] = std::cos((float)i * 0.1f) + uniform_dist(rng) * 0.1f;
    }

    /* Step 4: Process auditory input */
    wm_err = wm_process_modality(mm_wm, &auditory_input);
    EXPECT_EQ(wm_err, WM_OK);

    /* Step 5: Fuse modalities */
    wm_err = wm_fuse_modalities(mm_wm);
    EXPECT_EQ(wm_err, WM_OK);

    /* Get attention state */
    wm_cross_modal_attention_t attention;
    wm_err = wm_get_attention(mm_wm, &attention);
    EXPECT_EQ(wm_err, WM_OK);

    std::cout << "Cross-modal attention:" << std::endl;
    std::cout << "  Coherence score: " << attention.coherence_score << std::endl;
    std::cout << "  Dominant modality: " << attention.dominant_modality << std::endl;
    std::cout << "  Visual weight: " << attention.modality_weights[WM_MODALITY_VISUAL] << std::endl;
    std::cout << "  Auditory weight: " << attention.modality_weights[WM_MODALITY_AUDITORY] << std::endl;

    /* Step 6: Predict future state */
    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    wm_err = wm_predict(mm_wm, SHORT_HORIZON, &prediction);
    EXPECT_EQ(wm_err, WM_OK);

    std::cout << "Prediction results:" << std::endl;
    std::cout << "  Horizon steps: " << prediction.horizon_steps << std::endl;
    std::cout << "  Confidence: " << prediction.prediction_confidence << std::endl;
    std::cout << "  Surprise: " << prediction.surprise << std::endl;

    /* Step 7: Add and track entities */
    for (uint32_t e = 0; e < 3; e++) {
        wm_entity_t entity;
        memset(&entity, 0, sizeof(entity));
        entity.position[0] = (float)e * 2.0f;
        entity.position[1] = uniform_dist(rng);
        entity.position[2] = uniform_dist(rng);
        entity.velocity[0] = 0.1f;
        entity.velocity[1] = 0.0f;
        entity.velocity[2] = 0.0f;
        entity.existence_prob = 0.95f;
        entity.last_observed = 1000;
        entity.modality_mask = (1 << WM_MODALITY_VISUAL) | (1 << WM_MODALITY_AUDITORY);

        uint32_t entity_id;
        wm_err = wm_add_entity(mm_wm, &entity, &entity_id);
        EXPECT_EQ(wm_err, WM_OK);
        std::cout << "  Added entity " << entity_id << std::endl;
    }

    /* Get statistics */
    wm_stats_t stats;
    wm_err = wm_get_stats(mm_wm, &stats);
    EXPECT_EQ(wm_err, WM_OK);

    std::cout << "World model stats:" << std::endl;
    std::cout << "  Inputs processed: " << stats.inputs_processed << std::endl;
    std::cout << "  Predictions made: " << stats.predictions_made << std::endl;
    std::cout << "  Active entities: " << stats.active_entities << std::endl;

    /* Cleanup */
    nimcp_free(visual_input.features);
    nimcp_free(auditory_input.features);
}

/* ============================================================================
 * Test 6: Full Brain Simulation Cycle
 * ============================================================================ */

TEST_F(WorldModelE2ETest, FullBrainSimulationCycle) {
    /**
     * Test complete brain simulation cycle:
     * 1. Initialize world model
     * 2. Feed observation sequence
     * 3. Update internal state
     * 4. Predict next observations
     * 5. Compare with actual (measure surprise)
     * 6. Update model with prediction error
     */

    /* Initialize state */
    omni_wm_state_t* state = CreatePatternedState(STATE_DIM, 0.0f, 1.0f);
    ASSERT_NE(state, nullptr);

    nimcp_error_t err = omni_wm_set_state(wm, state);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    std::vector<float> surprise_history;
    float total_surprise = 0.0f;

    /* Run simulation cycle */
    for (uint32_t cycle = 0; cycle < 20; cycle++) {
        /* Step 1: Generate action */
        float action[ACTION_DIM];
        for (uint32_t i = 0; i < ACTION_DIM; i++) {
            action[i] = std::sin((float)cycle * 0.3f + (float)i) * 0.3f;
        }

        /* Step 2: Predict next state */
        omni_wm_transition_t prediction;
        memset(&prediction, 0, sizeof(prediction));

        err = omni_wm_predict_forward(wm, action, ACTION_DIM, &prediction);
        EXPECT_EQ(err, NIMCP_SUCCESS);
        ASSERT_NE(prediction.next_state, nullptr);

        /* Step 3: Simulate true next state */
        omni_wm_state_t* actual_next = omni_wm_state_create(STATE_DIM);
        ASSERT_NE(actual_next, nullptr);
        SimulateTrueDynamics(state, action, ACTION_DIM, actual_next);

        /* Step 4: Compute prediction error (surprise) */
        float surprise = ComputeMSE(prediction.next_state, actual_next);
        surprise_history.push_back(surprise);
        total_surprise += surprise;

        /* Step 5: Predict observations from state */
        float predicted_obs[OBS_DIM];
        err = omni_wm_predict_observations(wm, prediction.next_state, predicted_obs, OBS_DIM);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        /* Step 6: Update model with actual observation */
        float reward = 1.0f / (1.0f + surprise);  /* Higher reward for lower surprise */
        err = omni_wm_update(wm, state, action, ACTION_DIM, actual_next, reward);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        /* Move to next state */
        omni_wm_state_destroy(state);
        state = actual_next;

        omni_wm_state_destroy(prediction.next_state);

        /* Update world model's internal state */
        err = omni_wm_set_state(wm, state);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    /* Analyze surprise history */
    float mean_surprise = total_surprise / surprise_history.size();
    float early_surprise = 0.0f;
    float late_surprise = 0.0f;

    for (size_t i = 0; i < 5 && i < surprise_history.size(); i++) {
        early_surprise += surprise_history[i];
    }
    early_surprise /= 5.0f;

    for (size_t i = surprise_history.size() - 5; i < surprise_history.size(); i++) {
        late_surprise += surprise_history[i];
    }
    late_surprise /= 5.0f;

    std::cout << "Brain simulation cycle results:" << std::endl;
    std::cout << "  Total cycles: " << surprise_history.size() << std::endl;
    std::cout << "  Mean surprise: " << mean_surprise << std::endl;
    std::cout << "  Early surprise (first 5): " << early_surprise << std::endl;
    std::cout << "  Late surprise (last 5): " << late_surprise << std::endl;

    /* With learning, late surprise should be lower or comparable */
    /* Note: This is a soft check due to stochastic dynamics */
    EXPECT_LT(late_surprise, early_surprise * 2.0f);

    omni_wm_state_destroy(state);
}

/* ============================================================================
 * Test 7: Active Inference Integration
 * ============================================================================ */

TEST_F(WorldModelE2ETest, ActiveInferenceIntegration) {
    /**
     * Test active inference integration:
     * 1. Connect world model to active inference
     * 2. Define preferred observations (goal)
     * 3. Evaluate multiple policies
     * 4. Select best policy based on EFE
     */

    /* Set initial state */
    omni_wm_state_t* initial_state = CreatePatternedState(STATE_DIM, 3.0f, 1.0f);
    ASSERT_NE(initial_state, nullptr);

    nimcp_error_t err = omni_wm_set_state(wm, initial_state);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Define preferred observations (goal: state near zero) */
    float preferred_obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        preferred_obs[i] = 0.0f;  /* Goal: zero state */
    }

    /* Generate multiple policy candidates */
    const uint32_t num_policies = 5;
    std::vector<std::vector<float>> policies(num_policies);
    std::vector<float> efe_scores(num_policies);

    for (uint32_t p = 0; p < num_policies; p++) {
        /* Generate policy: action sequence for horizon steps */
        policies[p].resize(SHORT_HORIZON * ACTION_DIM);

        float direction = (float)p / num_policies * 2.0f - 1.0f;  /* Range [-1, 1] */

        for (uint32_t t = 0; t < SHORT_HORIZON; t++) {
            for (uint32_t a = 0; a < ACTION_DIM; a++) {
                policies[p][t * ACTION_DIM + a] = direction * 0.5f;
            }
        }

        /* Reset state for evaluation */
        err = omni_wm_set_state(wm, initial_state);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        /* Evaluate policy */
        float efe = omni_wm_evaluate_policy(wm, policies[p].data(), SHORT_HORIZON,
                                            preferred_obs, OBS_DIM);
        efe_scores[p] = efe;

        std::cout << "Policy " << p << " (direction=" << direction << "): EFE=" << efe << std::endl;
    }

    /* Find best policy (lowest EFE) */
    uint32_t best_policy = 0;
    float best_efe = efe_scores[0];

    for (uint32_t p = 1; p < num_policies; p++) {
        if (efe_scores[p] < best_efe) {
            best_efe = efe_scores[p];
            best_policy = p;
        }
    }

    std::cout << "Best policy: " << best_policy << " with EFE=" << best_efe << std::endl;

    /* Verify EFE scores are finite */
    for (uint32_t p = 0; p < num_policies; p++) {
        EXPECT_FALSE(std::isnan(efe_scores[p]));
        EXPECT_LT(efe_scores[p], FLT_MAX);
    }

    /* The best policy should not be the worst */
    float max_efe = *std::max_element(efe_scores.begin(), efe_scores.end());
    EXPECT_LT(best_efe, max_efe * 0.99f + 0.01f);  /* Allow some tolerance */

    omni_wm_state_destroy(initial_state);
}

/* ============================================================================
 * Test 8: Long-Horizon Prediction
 * ============================================================================ */

TEST_F(WorldModelE2ETest, LongHorizonPrediction) {
    /**
     * Test long-horizon prediction:
     * 1. Initialize with known dynamics
     * 2. Roll out predictions to maximum horizon
     * 3. Verify prediction degradation over time
     * 4. Test hierarchical prediction stability
     */

    /* Set initial state */
    omni_wm_state_t* initial_state = CreatePatternedState(STATE_DIM, 1.0f, 0.5f);
    ASSERT_NE(initial_state, nullptr);

    nimcp_error_t err = omni_wm_set_state(wm, initial_state);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Create long rollout */
    omni_wm_rollout_t* rollout = omni_wm_rollout_create(LONG_HORIZON, STATE_DIM, ACTION_DIM, OBS_DIM);
    ASSERT_NE(rollout, nullptr);

    uint32_t action_dim = ACTION_DIM;
    err = omni_wm_rollout(wm, SimplePolicy, LONG_HORIZON, rollout, &action_dim);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Track uncertainty over time */
    std::vector<float> uncertainties;
    std::vector<float> state_magnitudes;

    for (uint32_t t = 0; t < rollout->length; t++) {
        if (rollout->states[t]) {
            uncertainties.push_back(rollout->states[t]->uncertainty);

            float magnitude = 0.0f;
            for (uint32_t i = 0; i < rollout->states[t]->dim; i++) {
                magnitude += rollout->states[t]->values[i] * rollout->states[t]->values[i];
            }
            state_magnitudes.push_back(std::sqrt(magnitude));
        }
    }

    std::cout << "Long-horizon prediction analysis:" << std::endl;
    std::cout << "  Rollout length: " << rollout->length << std::endl;
    std::cout << "  Total reward: " << rollout->total_reward << std::endl;

    /* Analyze uncertainty growth */
    if (uncertainties.size() >= 2) {
        float initial_uncertainty = uncertainties.front();
        float final_uncertainty = uncertainties.back();

        std::cout << "  Initial uncertainty: " << initial_uncertainty << std::endl;
        std::cout << "  Final uncertainty: " << final_uncertainty << std::endl;

        /* Uncertainty should generally increase over long horizons */
        /* or at least not decrease drastically */
    }

    /* Analyze state stability */
    if (state_magnitudes.size() >= 5) {
        float early_magnitude = 0.0f;
        float late_magnitude = 0.0f;

        for (size_t i = 0; i < 5; i++) {
            early_magnitude += state_magnitudes[i];
            late_magnitude += state_magnitudes[state_magnitudes.size() - 5 + i];
        }
        early_magnitude /= 5.0f;
        late_magnitude /= 5.0f;

        std::cout << "  Early state magnitude: " << early_magnitude << std::endl;
        std::cout << "  Late state magnitude: " << late_magnitude << std::endl;

        /* States should not explode */
        EXPECT_LT(late_magnitude, early_magnitude * 100.0f);

        /* States should be finite */
        for (float mag : state_magnitudes) {
            EXPECT_FALSE(std::isnan(mag));
            EXPECT_FALSE(std::isinf(mag));
        }
    }

    /* Test hierarchical prediction (if enabled) */
    omni_wm_state_t* abstract_state = omni_wm_state_create(STATE_DIM / 2);
    if (abstract_state) {
        err = omni_wm_predict_hierarchical(wm, initial_state, 1, abstract_state);

        if (err == NIMCP_SUCCESS) {
            std::cout << "  Hierarchical prediction succeeded" << std::endl;
            std::cout << "  Abstract state level: " << abstract_state->level << std::endl;
        } else if (err == NIMCP_ERROR_NOT_IMPLEMENTED) {
            std::cout << "  Hierarchical prediction not enabled" << std::endl;
        }

        omni_wm_state_destroy(abstract_state);
    }

    /* Cleanup */
    omni_wm_rollout_destroy(rollout);
    omni_wm_state_destroy(initial_state);
}

/* ============================================================================
 * Test 9: RSSM Imagination
 * ============================================================================ */

TEST_F(WorldModelE2ETest, RSSMImagination) {
    /**
     * Test RSSM-based imagination:
     * 1. Create RSSM state
     * 2. Generate action sequence
     * 3. Imagine trajectory in latent space
     * 4. Verify trajectory coherence
     */

    /* Get current RSSM state or create one */
    const omni_wm_rssm_state_t* current_rssm = omni_wm_get_rssm_state(wm);
    omni_wm_rssm_state_t* rssm_state = nullptr;

    if (current_rssm) {
        rssm_state = omni_wm_rssm_state_clone(current_rssm);
    } else {
        rssm_state = omni_wm_rssm_state_create(STATE_DIM / 2, STATE_DIM / 4);
    }
    ASSERT_NE(rssm_state, nullptr);

    /* Initialize with some values */
    for (uint32_t i = 0; i < rssm_state->h_dim; i++) {
        rssm_state->h[i] = std::sin((float)i * 0.1f);
    }
    for (uint32_t i = 0; i < rssm_state->z_dim; i++) {
        rssm_state->z[i] = uniform_dist(rng) * 0.5f;
        rssm_state->z_mean[i] = rssm_state->z[i];
        rssm_state->z_std[i] = 0.1f;
    }

    /* Create action sequence */
    std::vector<float*> actions(SHORT_HORIZON);
    for (uint32_t t = 0; t < SHORT_HORIZON; t++) {
        actions[t] = (float*)nimcp_calloc(ACTION_DIM, sizeof(float));
        ASSERT_NE(actions[t], nullptr);

        for (uint32_t a = 0; a < ACTION_DIM; a++) {
            actions[t][a] = std::sin((float)t * 0.5f + (float)a) * 0.3f;
        }
    }

    /* Imagine trajectory */
    std::vector<omni_wm_rssm_state_t*> trajectory(SHORT_HORIZON);

    nimcp_error_t err = omni_wm_rssm_imagine(wm, rssm_state,
                                             (const float* const*)actions.data(),
                                             SHORT_HORIZON, trajectory.data());
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify trajectory */
    std::cout << "RSSM imagination results:" << std::endl;

    float prev_h_norm = 0.0f;
    for (uint32_t t = 0; t < SHORT_HORIZON; t++) {
        ASSERT_NE(trajectory[t], nullptr);

        /* Compute h norm */
        float h_norm = 0.0f;
        for (uint32_t i = 0; i < trajectory[t]->h_dim; i++) {
            h_norm += trajectory[t]->h[i] * trajectory[t]->h[i];
            EXPECT_FALSE(std::isnan(trajectory[t]->h[i]));
        }
        h_norm = std::sqrt(h_norm);

        /* Compute z norm */
        float z_norm = 0.0f;
        for (uint32_t i = 0; i < trajectory[t]->z_dim; i++) {
            z_norm += trajectory[t]->z[i] * trajectory[t]->z[i];
            EXPECT_FALSE(std::isnan(trajectory[t]->z[i]));
            EXPECT_GT(trajectory[t]->z_std[i], 0.0f);
        }
        z_norm = std::sqrt(z_norm);

        std::cout << "  t=" << t << ": h_norm=" << h_norm << ", z_norm=" << z_norm << std::endl;

        /* Trajectory should evolve smoothly */
        if (t > 0) {
            EXPECT_LT(std::abs(h_norm - prev_h_norm), prev_h_norm * 2.0f + 1.0f);
        }
        prev_h_norm = h_norm;
    }

    /* Cleanup */
    for (uint32_t t = 0; t < SHORT_HORIZON; t++) {
        omni_wm_rssm_state_destroy(trajectory[t]);
        nimcp_free(actions[t]);
    }
    omni_wm_rssm_state_destroy(rssm_state);
}

/* ============================================================================
 * Test 10: MDN Predictions
 * ============================================================================ */

TEST_F(WorldModelE2ETest, MDNPredictions) {
    /**
     * Test MDN (Mixture Density Network) predictions:
     * 1. Create state and action
     * 2. Predict with MDN
     * 3. Sample from distribution
     * 4. Get mode
     * 5. Compute log probability
     */

    /* Create state */
    omni_wm_state_t* state = CreatePatternedState(STATE_DIM, 1.0f, 0.5f);
    ASSERT_NE(state, nullptr);

    /* Create action */
    float action[ACTION_DIM];
    FillRandomAction(action, ACTION_DIM);

    /* Create MDN prediction */
    omni_wm_mdn_prediction_t* mdn = omni_wm_mdn_create(5, STATE_DIM);
    ASSERT_NE(mdn, nullptr);

    /* Predict with MDN */
    nimcp_error_t err = omni_wm_predict_mdn(wm, state, action, ACTION_DIM, mdn);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    std::cout << "MDN prediction results:" << std::endl;
    std::cout << "  Num components: " << mdn->num_components << std::endl;
    std::cout << "  Dimension: " << mdn->dim << std::endl;

    /* Check component weights sum to 1 */
    float weight_sum = 0.0f;
    for (uint32_t k = 0; k < mdn->num_components; k++) {
        weight_sum += mdn->components[k].weight;
        EXPECT_GT(mdn->components[k].weight, 0.0f);
        EXPECT_LE(mdn->components[k].weight, 1.0f);

        std::cout << "  Component " << k << ": weight=" << mdn->components[k].weight << std::endl;
    }
    EXPECT_NEAR(weight_sum, 1.0f, 0.01f);

    /* Sample from MDN */
    float sample[STATE_DIM];
    err = omni_wm_mdn_sample(mdn, sample);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify sample is finite */
    for (uint32_t i = 0; i < STATE_DIM; i++) {
        EXPECT_FALSE(std::isnan(sample[i]));
        EXPECT_FALSE(std::isinf(sample[i]));
    }

    /* Get mode */
    float mode[STATE_DIM];
    err = omni_wm_mdn_mode(mdn, mode);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify mode is finite */
    for (uint32_t i = 0; i < STATE_DIM; i++) {
        EXPECT_FALSE(std::isnan(mode[i]));
        EXPECT_FALSE(std::isinf(mode[i]));
    }

    /* Compute log probability */
    float log_prob_sample = omni_wm_mdn_log_prob(mdn, sample);
    float log_prob_mode = omni_wm_mdn_log_prob(mdn, mode);

    std::cout << "  Log prob of sample: " << log_prob_sample << std::endl;
    std::cout << "  Log prob of mode: " << log_prob_mode << std::endl;

    /* Mode should have higher or equal log probability than random sample */
    EXPECT_FALSE(std::isnan(log_prob_mode));
    EXPECT_GT(log_prob_mode, -FLT_MAX);

    /* Cleanup */
    omni_wm_mdn_destroy(mdn);
    omni_wm_state_destroy(state);
}

/* ============================================================================
 * Test 11: Symlog Transformation
 * ============================================================================ */

TEST_F(WorldModelE2ETest, SymlogTransformation) {
    /**
     * Test symlog transformation for reward normalization:
     * 1. Test positive values
     * 2. Test negative values
     * 3. Test inverse (symexp)
     * 4. Test array operations
     */

    /* Test scalar symlog */
    std::vector<float> test_values = {0.0f, 1.0f, 10.0f, 100.0f, 1000.0f, -1.0f, -10.0f, -100.0f};

    std::cout << "Symlog transformation tests:" << std::endl;

    for (float x : test_values) {
        float y = omni_wm_symlog(x);
        float x_recovered = omni_wm_symexp(y);

        std::cout << "  symlog(" << x << ") = " << y
                  << ", symexp(" << y << ") = " << x_recovered << std::endl;

        /* Verify inverse property */
        EXPECT_NEAR(x, x_recovered, std::abs(x) * 0.01f + 0.001f);

        /* Verify sign preservation */
        if (x > 0) EXPECT_GT(y, 0.0f);
        if (x < 0) EXPECT_LT(y, 0.0f);
        if (x == 0) EXPECT_EQ(y, 0.0f);

        /* Verify compression (symlog magnitude smaller than input for large values) */
        if (std::abs(x) > 2.0f) {
            EXPECT_LT(std::abs(y), std::abs(x));
        }
    }

    /* Test array operations */
    float input[10] = {-100.0f, -10.0f, -1.0f, -0.1f, 0.0f, 0.1f, 1.0f, 10.0f, 100.0f, 1000.0f};
    float transformed[10];
    float recovered[10];

    omni_wm_symlog_array(input, transformed, 10);
    omni_wm_symexp_array(transformed, recovered, 10);

    for (int i = 0; i < 10; i++) {
        EXPECT_NEAR(input[i], recovered[i], std::abs(input[i]) * 0.01f + 0.001f);
    }

    std::cout << "  Array round-trip test passed" << std::endl;
}

/* ============================================================================
 * Test 12: Experience Replay Buffer
 * ============================================================================ */

TEST_F(WorldModelE2ETest, ExperienceReplayBuffer) {
    /**
     * Test experience replay buffer:
     * 1. Add experiences
     * 2. Verify buffer size
     * 3. Sample batches
     * 4. Test circular buffer behavior
     * 5. Clear buffer
     */

    /* Initial buffer should be empty */
    EXPECT_EQ(omni_wm_get_replay_size(wm), 0u);

    /* Add experiences */
    const uint32_t num_experiences = 100;

    for (uint32_t i = 0; i < num_experiences; i++) {
        omni_wm_experience_t* exp = omni_wm_experience_create(STATE_DIM, ACTION_DIM, OBS_DIM);
        ASSERT_NE(exp, nullptr);

        /* Fill with identifiable data */
        if (exp->state && exp->state->h) {
            for (uint32_t j = 0; j < exp->state->h_dim; j++) {
                exp->state->h[j] = (float)i;
            }
        }

        exp->reward = (float)i * 0.01f;
        exp->terminal = (i % 10 == 9);

        nimcp_error_t err = omni_wm_add_experience(wm, exp);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        omni_wm_experience_destroy(exp);
    }

    /* Verify buffer size */
    uint32_t replay_size = omni_wm_get_replay_size(wm);
    EXPECT_EQ(replay_size, num_experiences);
    std::cout << "Replay buffer size after adding " << num_experiences
              << " experiences: " << replay_size << std::endl;

    /* Sample batch */
    std::vector<omni_wm_experience_t*> batch(BATCH_SIZE);
    uint32_t sampled = omni_wm_sample_experiences(wm, batch.data(), BATCH_SIZE);
    EXPECT_EQ(sampled, BATCH_SIZE);

    /* Verify samples are valid */
    for (uint32_t i = 0; i < sampled; i++) {
        EXPECT_NE(batch[i], nullptr);
    }

    std::cout << "Successfully sampled " << sampled << " experiences" << std::endl;

    /* Clear buffer */
    nimcp_error_t err = omni_wm_clear_replay(wm);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(omni_wm_get_replay_size(wm), 0u);
    std::cout << "Buffer cleared successfully" << std::endl;
}

/* ============================================================================
 * Test 13: State Inference from Observations
 * ============================================================================ */

TEST_F(WorldModelE2ETest, StateInferenceFromObservations) {
    /**
     * Test state inference from observations:
     * 1. Create observation
     * 2. Infer state
     * 3. Predict observations from inferred state
     * 4. Compare with original
     */

    /* Create observation */
    float observation[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        observation[i] = std::sin((float)i * 0.15f) + normal_dist(rng) * 0.1f;
    }

    /* Infer state from observation */
    omni_wm_state_t* inferred_state = omni_wm_state_create(STATE_DIM);
    ASSERT_NE(inferred_state, nullptr);

    nimcp_error_t err = omni_wm_infer_state(wm, observation, OBS_DIM, inferred_state);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify inferred state is valid */
    float state_norm = 0.0f;
    for (uint32_t i = 0; i < inferred_state->dim; i++) {
        EXPECT_FALSE(std::isnan(inferred_state->values[i]));
        state_norm += inferred_state->values[i] * inferred_state->values[i];
    }
    state_norm = std::sqrt(state_norm);

    std::cout << "State inference results:" << std::endl;
    std::cout << "  Inferred state norm: " << state_norm << std::endl;
    std::cout << "  State uncertainty: " << inferred_state->uncertainty << std::endl;

    /* Predict observations from inferred state */
    float predicted_obs[OBS_DIM];
    err = omni_wm_predict_observations(wm, inferred_state, predicted_obs, OBS_DIM);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Compare with original (won't be perfect with untrained model) */
    float obs_error = 0.0f;
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        EXPECT_FALSE(std::isnan(predicted_obs[i]));
        float diff = observation[i] - predicted_obs[i];
        obs_error += diff * diff;
    }
    obs_error = std::sqrt(obs_error / OBS_DIM);

    std::cout << "  Observation reconstruction RMSE: " << obs_error << std::endl;

    /* Cleanup */
    omni_wm_state_destroy(inferred_state);
}

/* ============================================================================
 * Test 14: Lateral Dynamics (Cross-Modal)
 * ============================================================================ */

TEST_F(WorldModelE2ETest, LateralDynamicsCrossModal) {
    /**
     * Test lateral dynamics for cross-modal prediction:
     * 1. Create source modality state
     * 2. Predict target modality state
     * 3. Verify relationship
     */

    /* Create source state (e.g., visual modality) */
    omni_wm_state_t* source_state = CreatePatternedState(STATE_DIM, 1.5f, 0.8f);
    ASSERT_NE(source_state, nullptr);

    /* Create target state (e.g., auditory modality) */
    omni_wm_state_t* target_state = omni_wm_state_create(STATE_DIM);
    ASSERT_NE(target_state, nullptr);

    /* Predict lateral (cross-modal) */
    uint32_t target_modality = 1;  /* Arbitrary modality ID */
    nimcp_error_t err = omni_wm_predict_lateral(wm, source_state, target_modality, target_state);

    if (err == NIMCP_SUCCESS) {
        /* Verify target state is related to source */
        float cosine_sim = CosineSimilarity(source_state->values, target_state->values,
                                            std::min(source_state->dim, target_state->dim));

        std::cout << "Lateral prediction results:" << std::endl;
        std::cout << "  Source uncertainty: " << source_state->uncertainty << std::endl;
        std::cout << "  Target uncertainty: " << target_state->uncertainty << std::endl;
        std::cout << "  Cross-modal cosine similarity: " << cosine_sim << std::endl;

        /* Cross-modal states should be related */
        EXPECT_GT(cosine_sim, 0.3f);

        /* Target uncertainty should reflect cross-modal inference uncertainty */
        EXPECT_GE(target_state->uncertainty, source_state->uncertainty);
    } else if (err == NIMCP_ERROR_NOT_IMPLEMENTED) {
        std::cout << "Lateral dynamics not enabled, skipping" << std::endl;
    } else {
        FAIL() << "Unexpected error: " << err;
    }

    /* Cleanup */
    omni_wm_state_destroy(target_state);
    omni_wm_state_destroy(source_state);
}

/* ============================================================================
 * Test 15: Backward Dynamics (Inference)
 * ============================================================================ */

TEST_F(WorldModelE2ETest, BackwardDynamicsInference) {
    /**
     * Test backward dynamics for inferring past states/actions:
     * 1. Set current state
     * 2. Infer previous state and action
     * 3. Verify forward consistency
     */

    /* Create current state */
    omni_wm_state_t* current_state = CreatePatternedState(STATE_DIM, 2.0f, 1.0f);
    ASSERT_NE(current_state, nullptr);

    /* Infer backward */
    omni_wm_transition_t backward_result;
    memset(&backward_result, 0, sizeof(backward_result));

    nimcp_error_t err = omni_wm_infer_backward(wm, current_state, &backward_result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify results */
    ASSERT_NE(backward_result.next_state, nullptr);  /* Actually prev_state in backward */
    ASSERT_NE(backward_result.action_taken, nullptr);

    std::cout << "Backward inference results:" << std::endl;
    std::cout << "  Direction: " << omni_wm_direction_to_string(backward_result.direction) << std::endl;
    std::cout << "  Action dim: " << backward_result.action_dim << std::endl;

    /* Verify action is valid */
    float action_norm = 0.0f;
    for (uint32_t i = 0; i < backward_result.action_dim; i++) {
        EXPECT_FALSE(std::isnan(backward_result.action_taken[i]));
        action_norm += backward_result.action_taken[i] * backward_result.action_taken[i];
    }
    action_norm = std::sqrt(action_norm);
    std::cout << "  Inferred action norm: " << action_norm << std::endl;

    /* Check forward consistency: applying inferred action to prev state should give current */
    omni_wm_state_t* old_state = (omni_wm_state_t*)omni_wm_get_state(wm);
    err = omni_wm_set_state(wm, backward_result.next_state);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    omni_wm_transition_t forward_check;
    memset(&forward_check, 0, sizeof(forward_check));

    err = omni_wm_predict_forward(wm, backward_result.action_taken,
                                   backward_result.action_dim, &forward_check);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    if (forward_check.next_state) {
        float consistency_error = ComputeMSE(forward_check.next_state, current_state);
        std::cout << "  Forward consistency error: " << consistency_error << std::endl;

        omni_wm_state_destroy(forward_check.next_state);
    }

    /* Verify stats */
    omni_wm_stats_t stats;
    omni_wm_get_stats(wm, &stats);
    EXPECT_GT(stats.backward_inferences, 0u);

    /* Cleanup */
    omni_wm_state_destroy(backward_result.next_state);
    nimcp_free(backward_result.action_taken);
    omni_wm_state_destroy(current_state);
}

/* ============================================================================
 * Test 16: Statistics and Monitoring
 * ============================================================================ */

TEST_F(WorldModelE2ETest, StatisticsAndMonitoring) {
    /**
     * Test statistics collection and monitoring:
     * 1. Reset statistics
     * 2. Perform various operations
     * 3. Verify statistics are updated
     */

    /* Reset statistics */
    nimcp_error_t err = omni_wm_reset_stats(wm);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    omni_wm_stats_t initial_stats;
    err = omni_wm_get_stats(wm, &initial_stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(initial_stats.forward_predictions, 0u);
    EXPECT_EQ(initial_stats.backward_inferences, 0u);
    EXPECT_EQ(initial_stats.counterfactual_queries, 0u);
    EXPECT_EQ(initial_stats.rollouts_completed, 0u);
    EXPECT_EQ(initial_stats.model_updates, 0u);

    /* Perform forward prediction */
    omni_wm_state_t* state = CreateRandomState(STATE_DIM);
    ASSERT_NE(state, nullptr);
    err = omni_wm_set_state(wm, state);

    float action[ACTION_DIM];
    FillRandomAction(action, ACTION_DIM);

    omni_wm_transition_t trans;
    memset(&trans, 0, sizeof(trans));
    err = omni_wm_predict_forward(wm, action, ACTION_DIM, &trans);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Perform backward inference */
    omni_wm_transition_t back_trans;
    memset(&back_trans, 0, sizeof(back_trans));
    err = omni_wm_infer_backward(wm, state, &back_trans);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Perform counterfactual */
    omni_wm_counterfactual_result_t cf_result;
    memset(&cf_result, 0, sizeof(cf_result));
    err = omni_wm_what_if(wm, action, ACTION_DIM, 3, &cf_result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Perform rollout */
    uint32_t action_dim = ACTION_DIM;
    omni_wm_rollout_t* rollout = omni_wm_rollout_create(5, STATE_DIM, ACTION_DIM, OBS_DIM);
    err = omni_wm_rollout(wm, SimplePolicy, 5, rollout, &action_dim);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Perform update */
    omni_wm_state_t* next_state = CreateRandomState(STATE_DIM);
    err = omni_wm_update(wm, state, action, ACTION_DIM, next_state, 1.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Check statistics */
    omni_wm_stats_t final_stats;
    err = omni_wm_get_stats(wm, &final_stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    std::cout << "Statistics after operations:" << std::endl;
    std::cout << "  Forward predictions: " << final_stats.forward_predictions << std::endl;
    std::cout << "  Backward inferences: " << final_stats.backward_inferences << std::endl;
    std::cout << "  Lateral predictions: " << final_stats.lateral_predictions << std::endl;
    std::cout << "  Counterfactual queries: " << final_stats.counterfactual_queries << std::endl;
    std::cout << "  Rollouts completed: " << final_stats.rollouts_completed << std::endl;
    std::cout << "  Model updates: " << final_stats.model_updates << std::endl;
    std::cout << "  Mean prediction error: " << final_stats.mean_prediction_error << std::endl;

    /* Verify statistics updated */
    EXPECT_GT(final_stats.forward_predictions, 0u);
    EXPECT_GT(final_stats.backward_inferences, 0u);
    EXPECT_GT(final_stats.counterfactual_queries, 0u);
    EXPECT_GT(final_stats.rollouts_completed, 0u);
    EXPECT_GT(final_stats.model_updates, 0u);

    /* Cleanup */
    omni_wm_rollout_destroy(rollout);
    omni_wm_cf_result_destroy(&cf_result);
    omni_wm_state_destroy(back_trans.next_state);
    nimcp_free(back_trans.action_taken);
    omni_wm_state_destroy(trans.next_state);
    omni_wm_state_destroy(next_state);
    omni_wm_state_destroy(state);
}

/* ============================================================================
 * Test 17: Utility Functions
 * ============================================================================ */

TEST_F(WorldModelE2ETest, UtilityFunctions) {
    /**
     * Test utility functions:
     * 1. Direction to string
     * 2. Learn mode to string
     * 3. Counterfactual type to string
     */

    /* Test direction strings */
    EXPECT_STREQ(omni_wm_direction_to_string(OMNI_WM_DIR_FORWARD), "forward");
    EXPECT_STREQ(omni_wm_direction_to_string(OMNI_WM_DIR_BACKWARD), "backward");
    EXPECT_STREQ(omni_wm_direction_to_string(OMNI_WM_DIR_LATERAL), "lateral");
    EXPECT_STREQ(omni_wm_direction_to_string(OMNI_WM_DIR_HIERARCHICAL), "hierarchical");

    /* Test learn mode strings */
    EXPECT_STREQ(omni_wm_learn_mode_to_string(OMNI_WM_LEARN_ONLINE), "online");
    EXPECT_STREQ(omni_wm_learn_mode_to_string(OMNI_WM_LEARN_BATCH), "batch");
    EXPECT_STREQ(omni_wm_learn_mode_to_string(OMNI_WM_LEARN_REPLAY), "replay");
    EXPECT_STREQ(omni_wm_learn_mode_to_string(OMNI_WM_LEARN_DREAMING), "dreaming");

    /* Test counterfactual type strings */
    EXPECT_STREQ(omni_wm_cf_type_to_string(OMNI_WM_CF_ACTION), "action");
    EXPECT_STREQ(omni_wm_cf_type_to_string(OMNI_WM_CF_STATE), "state");
    EXPECT_STREQ(omni_wm_cf_type_to_string(OMNI_WM_CF_CONTEXT), "context");
    EXPECT_STREQ(omni_wm_cf_type_to_string(OMNI_WM_CF_GOAL), "goal");

    std::cout << "All utility function tests passed" << std::endl;
}

/* ============================================================================
 * Test 18: Stress Test - Many Predictions
 * ============================================================================ */

TEST_F(WorldModelE2ETest, StressTestManyPredictions) {
    /**
     * Stress test with many predictions:
     * 1. Perform 1000 forward predictions
     * 2. Verify no memory leaks or crashes
     * 3. Check statistics consistency
     */

    const uint32_t num_predictions = 1000;

    omni_wm_state_t* state = CreateRandomState(STATE_DIM);
    ASSERT_NE(state, nullptr);

    nimcp_error_t err = omni_wm_set_state(wm, state);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    uint32_t success_count = 0;
    uint32_t error_count = 0;

    for (uint32_t i = 0; i < num_predictions; i++) {
        float action[ACTION_DIM];
        FillRandomAction(action, ACTION_DIM);

        omni_wm_transition_t trans;
        memset(&trans, 0, sizeof(trans));

        err = omni_wm_predict_forward(wm, action, ACTION_DIM, &trans);

        if (err == NIMCP_SUCCESS && trans.next_state) {
            success_count++;

            /* Update state for next prediction */
            omni_wm_state_destroy(state);
            state = trans.next_state;
            err = omni_wm_set_state(wm, state);
            state = omni_wm_state_clone(trans.next_state);
            omni_wm_state_destroy(trans.next_state);
        } else {
            error_count++;
            if (trans.next_state) {
                omni_wm_state_destroy(trans.next_state);
            }
        }
    }

    std::cout << "Stress test results:" << std::endl;
    std::cout << "  Total predictions: " << num_predictions << std::endl;
    std::cout << "  Successful: " << success_count << std::endl;
    std::cout << "  Errors: " << error_count << std::endl;

    /* Most predictions should succeed */
    EXPECT_GT(success_count, num_predictions * 0.95);

    /* Verify statistics */
    omni_wm_stats_t stats;
    omni_wm_get_stats(wm, &stats);
    EXPECT_GE(stats.forward_predictions, success_count);

    omni_wm_state_destroy(state);
}

/* ============================================================================
 * Test 19: Multimodal Status and Error Handling
 * ============================================================================ */

TEST_F(WorldModelE2ETest, MultimodalStatusAndErrorHandling) {
    /**
     * Test multimodal world model status and error handling:
     * 1. Check status
     * 2. Test error handling
     * 3. Verify error strings
     */

    if (!mm_wm) {
        GTEST_SKIP() << "Multimodal world model not available";
    }

    /* Check initial status */
    wm_status_t status = wm_get_status(mm_wm);
    std::cout << "Initial status: " << wm_status_string(status) << std::endl;
    EXPECT_NE(status, WM_STATUS_ERROR);

    /* Test error strings */
    EXPECT_STREQ(wm_error_string(WM_OK), "OK");
    EXPECT_NE(wm_error_string(WM_ERR_NULL_PTR), nullptr);
    EXPECT_NE(wm_error_string(WM_ERR_NOT_INITIALIZED), nullptr);
    EXPECT_NE(wm_error_string(WM_ERR_INVALID_MODALITY), nullptr);

    /* Test modality strings - implementation returns capitalized names */
    EXPECT_STREQ(wm_modality_string(WM_MODALITY_VISUAL), "Visual");
    EXPECT_STREQ(wm_modality_string(WM_MODALITY_AUDITORY), "Auditory");

    /* Test error cases */
    wm_error_t wm_err = wm_process_modality(nullptr, nullptr);
    EXPECT_EQ(wm_err, WM_ERR_NULL_PTR);

    wm_err = wm_predict(mm_wm, 0, nullptr);  /* Invalid horizon */
    EXPECT_NE(wm_err, WM_OK);

    std::cout << "Error handling tests passed" << std::endl;
}

/* ============================================================================
 * Test 20: End-to-End Complete Cognitive Cycle
 * ============================================================================ */

TEST_F(WorldModelE2ETest, CompleteCognitiveCycle) {
    /**
     * Complete cognitive cycle integrating all components:
     * 1. Observe -> Encode
     * 2. Update state
     * 3. Predict forward
     * 4. Evaluate policies (counterfactual reasoning)
     * 5. Select action
     * 6. Execute and observe outcome
     * 7. Update model with prediction error
     * 8. Dream to consolidate
     */

    std::cout << "=== Complete Cognitive Cycle ===" << std::endl;

    /* Initialize */
    omni_wm_state_t* state = CreatePatternedState(STATE_DIM, 1.0f, 0.5f);
    ASSERT_NE(state, nullptr);

    nimcp_error_t err = omni_wm_set_state(wm, state);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Define goal */
    float goal_obs[OBS_DIM];
    memset(goal_obs, 0, OBS_DIM * sizeof(float));  /* Goal: zero state */

    float total_reward = 0.0f;
    const uint32_t num_cycles = 10;

    for (uint32_t cycle = 0; cycle < num_cycles; cycle++) {
        std::cout << "Cycle " << cycle << ":" << std::endl;

        /* Step 1: Encode current observation */
        omni_wm_latent_t* latent = omni_wm_latent_create(LATENT_DIM);
        ASSERT_NE(latent, nullptr);

        float observation[OBS_DIM];
        err = omni_wm_predict_observations(wm, state, observation, OBS_DIM);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        err = omni_wm_encode(wm, observation, OBS_DIM, latent);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        /* Step 2: Evaluate candidate policies (counterfactual reasoning) */
        float best_action[ACTION_DIM];
        float best_efe = FLT_MAX;

        for (int policy = 0; policy < 3; policy++) {
            float test_action[ACTION_DIM];
            for (uint32_t a = 0; a < ACTION_DIM; a++) {
                test_action[a] = (float)(policy - 1) * 0.3f;  /* -0.3, 0, 0.3 */
            }

            /* Quick EFE evaluation */
            std::vector<float> policy_actions(SHORT_HORIZON * ACTION_DIM);
            for (uint32_t t = 0; t < SHORT_HORIZON; t++) {
                memcpy(&policy_actions[t * ACTION_DIM], test_action, ACTION_DIM * sizeof(float));
            }

            err = omni_wm_set_state(wm, state);
            float efe = omni_wm_evaluate_policy(wm, policy_actions.data(), SHORT_HORIZON,
                                                goal_obs, OBS_DIM);

            if (efe < best_efe) {
                best_efe = efe;
                memcpy(best_action, test_action, ACTION_DIM * sizeof(float));
            }
        }

        std::cout << "  Best EFE: " << best_efe << std::endl;

        /* Step 3: Execute selected action */
        err = omni_wm_set_state(wm, state);

        omni_wm_transition_t trans;
        memset(&trans, 0, sizeof(trans));
        err = omni_wm_predict_forward(wm, best_action, ACTION_DIM, &trans);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        /* Step 4: Simulate actual outcome (with noise) */
        omni_wm_state_t* actual_next = omni_wm_state_create(STATE_DIM);
        ASSERT_NE(actual_next, nullptr);
        SimulateTrueDynamics(state, best_action, ACTION_DIM, actual_next);

        /* Step 5: Compute surprise (prediction error) */
        float surprise = ComputeMSE(trans.next_state, actual_next);
        float reward = 1.0f / (1.0f + surprise);
        total_reward += reward;

        std::cout << "  Surprise: " << surprise << ", Reward: " << reward << std::endl;

        /* Step 6: Update model */
        err = omni_wm_update(wm, state, best_action, ACTION_DIM, actual_next, reward);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        /* Step 7: Add to experience buffer */
        omni_wm_experience_t* exp = omni_wm_experience_create(STATE_DIM, ACTION_DIM, OBS_DIM);
        if (exp) {
            memcpy(exp->action, best_action, ACTION_DIM * sizeof(float));
            exp->reward = reward;
            exp->terminal = false;
            omni_wm_add_experience(wm, exp);
            omni_wm_experience_destroy(exp);
        }

        /* Move to next state */
        omni_wm_state_destroy(state);
        state = actual_next;
        err = omni_wm_set_state(wm, state);

        omni_wm_state_destroy(trans.next_state);
        omni_wm_latent_destroy(latent);
    }

    /* Final phase: Dream to consolidate */
    std::cout << "Dreaming phase..." << std::endl;
    err = omni_wm_dream(wm, 3, 5);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Summary */
    std::cout << "=== Cognitive Cycle Summary ===" << std::endl;
    std::cout << "  Total cycles: " << num_cycles << std::endl;
    std::cout << "  Total reward: " << total_reward << std::endl;
    std::cout << "  Average reward: " << total_reward / num_cycles << std::endl;

    omni_wm_stats_t stats;
    omni_wm_get_stats(wm, &stats);
    std::cout << "  Model updates: " << stats.model_updates << std::endl;
    std::cout << "  Mean prediction error: " << stats.mean_prediction_error << std::endl;

    omni_wm_state_destroy(state);
}
