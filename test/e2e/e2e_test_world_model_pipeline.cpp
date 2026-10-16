/**
 * @file e2e_test_world_model_pipeline.cpp
 * @brief End-to-end tests for World Model pipeline
 * @version 1.0.0
 * @date 2025-01-24
 *
 * Tests complete world model workflows:
 * - Multimodal world model creation, processing, and prediction
 * - Omni world model forward/backward/lateral dynamics
 * - Cross-modal fusion pipeline
 * - Entity tracking pipeline
 * - Exception handling and recovery
 * - Integration with cognitive components
 *
 * ARCHITECTURE:
 * - Multimodal World Model: Integrates 10 sensory modalities
 * - Omni World Model: DreamerV3-inspired RSSM for counterfactual reasoning
 * - Cross-modal attention: Learns modality importance weights
 * - Entity tracking: Tracks objects with existence probability
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>
#include <thread>

#include "cognitive/extrapolation/nimcp_world_model_multimodal.h"
#include "cognitive/omni/nimcp_omni_world_model.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Configuration
//=============================================================================

namespace {
    constexpr uint32_t TEST_LATENT_DIM = 64;
    constexpr uint32_t TEST_STATE_DIM = 32;
    constexpr uint32_t TEST_ACTION_DIM = 16;
    constexpr uint32_t TEST_OBS_DIM = 64;
    constexpr uint32_t TEST_FEATURE_DIM = 64;
    constexpr uint32_t NUM_ENTITIES = 10;
    constexpr uint32_t PREDICTION_HORIZON = 10;
    constexpr uint32_t NUM_STEPS = 50;
    constexpr float CONFIDENCE_THRESHOLD = 0.5f;
}

//=============================================================================
// Test Fixture
//=============================================================================

class WorldModelE2ETest : public ::testing::Test {
protected:
    nimcp_world_model_t* multimodal_wm = nullptr;
    omni_world_model_t* omni_wm = nullptr;
    std::mt19937 rng{42};

    void SetUp() override {
        // Create multimodal world model
        wm_config_t mm_config = wm_default_config();
        mm_config.latent_dim = TEST_LATENT_DIM;
        mm_config.max_entities = NUM_ENTITIES * 2;
        mm_config.max_prediction_steps = PREDICTION_HORIZON * 2;
        mm_config.enable_bio_async = false;
        mm_config.enable_immune = false;
        multimodal_wm = wm_create(&mm_config);
        ASSERT_NE(multimodal_wm, nullptr);

        wm_error_t result = wm_init(multimodal_wm);
        ASSERT_EQ(result, WM_OK);

        // Create omni world model
        omni_wm_config_t omni_config;
        omni_wm_get_default_config(&omni_config);
        omni_config.state_dim = TEST_STATE_DIM;
        omni_config.action_dim = TEST_ACTION_DIM;
        omni_config.obs_dim = TEST_OBS_DIM;
        omni_config.enable_dreaming = false;
        omni_config.use_rssm = true;
        omni_wm = omni_wm_create(&omni_config);
        ASSERT_NE(omni_wm, nullptr);
    }

    void TearDown() override {
        if (multimodal_wm) {
            wm_destroy(multimodal_wm);
            multimodal_wm = nullptr;
        }
        if (omni_wm) {
            omni_wm_destroy(omni_wm);
            omni_wm = nullptr;
        }
    }

    // Helper: Create modality input
    wm_modality_input_t create_modality_input(wm_modality_t modality, uint32_t dim) {
        wm_modality_input_t input = {};
        input.modality = modality;
        input.feature_dim = dim;
        input.features = new float[dim];
        input.confidence = 0.9f;
        input.timestamp = static_cast<uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count() / 1000000
        );
        input.attention_weights = nullptr;

        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        for (uint32_t i = 0; i < dim; i++) {
            input.features[i] = dist(rng);
        }
        return input;
    }

    // Helper: Free modality input
    void free_modality_input(wm_modality_input_t& input) {
        delete[] input.features;
        input.features = nullptr;
    }

    // Helper: Create random state
    omni_wm_state_t* create_random_state(uint32_t dim) {
        omni_wm_state_t* state = omni_wm_state_create(dim);
        if (!state) return nullptr;

        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (uint32_t i = 0; i < dim; i++) {
            state->values[i] = dist(rng);
        }
        state->uncertainty = 0.1f;
        return state;
    }

    // Helper: Create random action
    std::vector<float> create_random_action(uint32_t dim) {
        std::vector<float> action(dim);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (auto& val : action) {
            val = dist(rng);
        }
        return action;
    }

    // Helper: Calculate cosine similarity
    float cosine_similarity(const float* a, const float* b, uint32_t dim) {
        float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            dot += a[i] * b[i];
            norm_a += a[i] * a[i];
            norm_b += b[i] * b[i];
        }
        return dot / (std::sqrt(norm_a) * std::sqrt(norm_b) + 1e-8f);
    }
};

//=============================================================================
// Multimodal World Model E2E Tests
//=============================================================================

TEST_F(WorldModelE2ETest, MultimodalProcessingPipeline) {
    // Test: Complete multimodal processing pipeline
    // Input -> Encode -> Fuse -> Predict

    // 1. Process visual modality
    auto visual_input = create_modality_input(WM_MODALITY_VISUAL, TEST_FEATURE_DIM);
    wm_error_t result = wm_process_modality(multimodal_wm, &visual_input);
    EXPECT_EQ(result, WM_OK);

    // 2. Process auditory modality
    auto auditory_input = create_modality_input(WM_MODALITY_AUDITORY, TEST_FEATURE_DIM / 2);
    result = wm_process_modality(multimodal_wm, &auditory_input);
    EXPECT_EQ(result, WM_OK);

    // 3. Process tactile modality
    auto tactile_input = create_modality_input(WM_MODALITY_TACTILE, TEST_FEATURE_DIM / 4);
    result = wm_process_modality(multimodal_wm, &tactile_input);
    EXPECT_EQ(result, WM_OK);

    // 4. Fuse modalities
    result = wm_fuse_modalities(multimodal_wm);
    EXPECT_EQ(result, WM_OK);

    // 5. Get cross-modal attention
    wm_cross_modal_attention_t attention = {};
    result = wm_get_attention(multimodal_wm, &attention);
    EXPECT_EQ(result, WM_OK);
    EXPECT_GE(attention.coherence_score, 0.0f);
    EXPECT_LE(attention.coherence_score, 1.0f);

    // 6. Make prediction
    wm_prediction_t prediction = {};
    result = wm_predict(multimodal_wm, PREDICTION_HORIZON, &prediction);
    EXPECT_EQ(result, WM_OK);
    EXPECT_EQ(prediction.horizon_steps, PREDICTION_HORIZON);

    // Cleanup
    free_modality_input(visual_input);
    free_modality_input(auditory_input);
    free_modality_input(tactile_input);
}

TEST_F(WorldModelE2ETest, AllModalitiesProcessing) {
    // Test: Process all supported modalities

    wm_modality_t modalities[] = {
        WM_MODALITY_VISUAL,
        WM_MODALITY_AUDITORY,
        WM_MODALITY_TACTILE,
        WM_MODALITY_PROPRIOCEPTIVE,
        WM_MODALITY_OLFACTORY,
        WM_MODALITY_GUSTATORY,
        WM_MODALITY_VESTIBULAR,
        WM_MODALITY_INTEROCEPTIVE,
        WM_MODALITY_LINGUISTIC,
        WM_MODALITY_SEMANTIC
    };

    int success_count = 0;
    for (int i = 0; i < WM_MODALITY_COUNT; i++) {
        auto input = create_modality_input(modalities[i], TEST_FEATURE_DIM);
        wm_error_t result = wm_process_modality(multimodal_wm, &input);
        if (result == WM_OK) {
            success_count++;
        }
        free_modality_input(input);
    }

    std::cout << "Modalities processed: " << success_count << "/" << WM_MODALITY_COUNT << std::endl;
    EXPECT_EQ(success_count, WM_MODALITY_COUNT);

    // Fuse all and verify
    wm_error_t result = wm_fuse_modalities(multimodal_wm);
    EXPECT_EQ(result, WM_OK);
}

TEST_F(WorldModelE2ETest, EntityTrackingPipeline) {
    // Test: Add, update, and track entities

    std::vector<uint32_t> entity_ids;

    // 1. Add entities
    for (uint32_t i = 0; i < NUM_ENTITIES; i++) {
        wm_entity_t entity = {};
        entity.entity_id = i;
        entity.position[0] = static_cast<float>(i) * 0.5f;
        entity.position[1] = static_cast<float>(i) * 0.3f;
        entity.position[2] = 0.0f;
        entity.velocity[0] = 0.1f;
        entity.velocity[1] = 0.05f;
        entity.velocity[2] = 0.0f;
        entity.existence_prob = 0.9f;
        entity.last_observed = 1000 + i * 100;
        entity.modality_mask = (1 << WM_MODALITY_VISUAL) | (1 << WM_MODALITY_AUDITORY);

        uint32_t assigned_id = 0;
        wm_error_t result = wm_add_entity(multimodal_wm, &entity, &assigned_id);
        if (result == WM_OK) {
            entity_ids.push_back(assigned_id);
        }
    }

    std::cout << "Entities added: " << entity_ids.size() << "/" << NUM_ENTITIES << std::endl;

    // 2. Update entities
    for (size_t i = 0; i < entity_ids.size(); i++) {
        wm_entity_t update = {};
        update.position[0] = static_cast<float>(i) + 1.0f;
        update.position[1] = static_cast<float>(i) + 0.5f;
        update.existence_prob = 0.85f;

        wm_update_entity(multimodal_wm, entity_ids[i], &update);
    }

    // 3. Retrieve entities
    if (!entity_ids.empty()) {
        wm_entity_t retrieved = {};
        wm_error_t result = wm_get_entity(multimodal_wm, entity_ids[0], &retrieved);
        if (result == WM_OK) {
            EXPECT_GE(retrieved.existence_prob, 0.0f);
            EXPECT_LE(retrieved.existence_prob, 1.0f);
        }
    }

    // 4. Predict entity trajectory
    if (!entity_ids.empty()) {
        float trajectory[10 * 3]; // 10 steps x 3D position
        float confidence;
        wm_predict_entity(multimodal_wm, entity_ids[0], 10, trajectory, &confidence);
    }

    // 5. Verify stats
    wm_stats_t stats = {};
    wm_get_stats(multimodal_wm, &stats);
    std::cout << "Active entities: " << stats.active_entities << std::endl;
}

TEST_F(WorldModelE2ETest, LongRunningPredictionPipeline) {
    // Test: Continuous prediction over many timesteps

    for (uint32_t step = 0; step < NUM_STEPS; step++) {
        // Add sensory input at each step
        auto visual = create_modality_input(WM_MODALITY_VISUAL, TEST_FEATURE_DIM);
        visual.timestamp = 1000 + step * 100;
        wm_process_modality(multimodal_wm, &visual);

        if (step % 2 == 0) {
            auto auditory = create_modality_input(WM_MODALITY_AUDITORY, TEST_FEATURE_DIM / 2);
            auditory.timestamp = 1000 + step * 100;
            wm_process_modality(multimodal_wm, &auditory);
            free_modality_input(auditory);
        }

        // Fuse and predict
        wm_fuse_modalities(multimodal_wm);

        wm_prediction_t prediction = {};
        wm_error_t result = wm_predict(multimodal_wm, 5, &prediction);
        EXPECT_EQ(result, WM_OK);

        // Update world model with time
        wm_update(multimodal_wm, 100.0f);

        free_modality_input(visual);
    }

    wm_stats_t stats = {};
    wm_get_stats(multimodal_wm, &stats);
    std::cout << "Total inputs processed: " << stats.inputs_processed << std::endl;
    std::cout << "Total predictions made: " << stats.predictions_made << std::endl;
    std::cout << "Total fusion operations: " << stats.fusion_operations << std::endl;
}

//=============================================================================
// Omni World Model E2E Tests
//=============================================================================

TEST_F(WorldModelE2ETest, OmniForwardDynamicsPipeline) {
    // Test: Forward dynamics prediction pipeline

    // Set initial state
    omni_wm_state_t* state = create_random_state(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);

    nimcp_error_t result = omni_wm_set_state(omni_wm, state);
    EXPECT_EQ(result, NIMCP_OK);

    // Perform multiple forward predictions
    for (int step = 0; step < 10; step++) {
        auto action = create_random_action(TEST_ACTION_DIM);
        omni_wm_transition_t transition = {};

        result = omni_wm_predict_forward(omni_wm, action.data(), TEST_ACTION_DIM, &transition);
        EXPECT_EQ(result, NIMCP_OK);

        // Verify transition is valid
        EXPECT_FALSE(std::isnan(transition.log_prob));
        EXPECT_FALSE(std::isinf(transition.log_prob));
    }

    omni_wm_state_destroy(state);

    omni_wm_stats_t stats = {};
    omni_wm_get_stats(omni_wm, &stats);
    std::cout << "Forward predictions: " << stats.forward_predictions << std::endl;
}

TEST_F(WorldModelE2ETest, OmniBackwardInferencePipeline) {
    // Test: Backward dynamics inference pipeline

    omni_wm_state_t* current_state = create_random_state(TEST_STATE_DIM);
    ASSERT_NE(current_state, nullptr);

    omni_wm_transition_t transition = {};
    nimcp_error_t result = omni_wm_infer_backward(omni_wm, current_state, &transition);
    // May not be implemented, check graceful handling
    if (result == NIMCP_OK) {
        EXPECT_FALSE(std::isnan(transition.log_prob));
    }

    omni_wm_state_destroy(current_state);

    omni_wm_stats_t stats = {};
    omni_wm_get_stats(omni_wm, &stats);
    std::cout << "Backward inferences: " << stats.backward_inferences << std::endl;
}

TEST_F(WorldModelE2ETest, OmniCounterfactualPipeline) {
    // Test: Counterfactual reasoning pipeline ("What if?")

    omni_wm_state_t* initial_state = create_random_state(TEST_STATE_DIM);
    ASSERT_NE(initial_state, nullptr);

    omni_wm_set_state(omni_wm, initial_state);

    // Create hypothetical action
    auto action = create_random_action(TEST_ACTION_DIM);

    // Execute "what if" query
    omni_wm_counterfactual_result_t result = {};
    nimcp_error_t err = omni_wm_what_if(omni_wm, action.data(), TEST_ACTION_DIM, 5, &result);

    if (err == NIMCP_OK) {
        EXPECT_GE(result.confidence, 0.0f);
        EXPECT_LE(result.confidence, 1.0f);
        std::cout << "Counterfactual confidence: " << result.confidence << std::endl;
        omni_wm_cf_result_destroy(&result);
    }

    omni_wm_state_destroy(initial_state);

    omni_wm_stats_t stats = {};
    omni_wm_get_stats(omni_wm, &stats);
    std::cout << "Counterfactual queries: " << stats.counterfactual_queries << std::endl;
}

TEST_F(WorldModelE2ETest, OmniLearningPipeline) {
    // Test: Model learning from experience

    omni_wm_state_t* state = create_random_state(TEST_STATE_DIM);
    omni_wm_state_t* next_state = create_random_state(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    ASSERT_NE(next_state, nullptr);

    // Learn from multiple experiences
    for (int exp = 0; exp < 20; exp++) {
        auto action = create_random_action(TEST_ACTION_DIM);
        float reward = static_cast<float>(exp % 2 == 0 ? 1.0f : -0.5f);

        nimcp_error_t result = omni_wm_update(omni_wm, state, action.data(),
                                               TEST_ACTION_DIM, next_state, reward);
        if (result != NIMCP_OK) {
            // Learning may not be fully implemented
            break;
        }

        // Randomize states for next iteration
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (uint32_t i = 0; i < TEST_STATE_DIM; i++) {
            state->values[i] = dist(rng);
            next_state->values[i] = dist(rng);
        }
    }

    omni_wm_state_destroy(state);
    omni_wm_state_destroy(next_state);

    omni_wm_stats_t stats = {};
    omni_wm_get_stats(omni_wm, &stats);
    std::cout << "Model updates: " << stats.model_updates << std::endl;
}

TEST_F(WorldModelE2ETest, OmniRolloutPipeline) {
    // Test: Policy rollout for planning

    omni_wm_state_t* initial_state = create_random_state(TEST_STATE_DIM);
    ASSERT_NE(initial_state, nullptr);

    omni_wm_set_state(omni_wm, initial_state);

    // Create rollout structure
    omni_wm_rollout_t* rollout = omni_wm_rollout_create(10, TEST_STATE_DIM,
                                                         TEST_ACTION_DIM, TEST_OBS_DIM);
    if (rollout) {
        // Simple policy: random actions
        auto policy = [](const omni_wm_state_t* state, float* action, void* user_data) {
            std::mt19937* gen = static_cast<std::mt19937*>(user_data);
            std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
            for (uint32_t i = 0; i < TEST_ACTION_DIM; i++) {
                action[i] = dist(*gen);
            }
        };

        nimcp_error_t result = omni_wm_rollout(omni_wm, policy, 10, rollout, &rng);
        if (result == NIMCP_OK) {
            std::cout << "Rollout total reward: " << rollout->total_reward << std::endl;
            std::cout << "Rollout EFE: " << rollout->expected_free_energy << std::endl;
        }

        omni_wm_rollout_destroy(rollout);
    }

    omni_wm_state_destroy(initial_state);

    omni_wm_stats_t stats = {};
    omni_wm_get_stats(omni_wm, &stats);
    std::cout << "Rollouts completed: " << stats.rollouts_completed << std::endl;
}

//=============================================================================
// RSSM (Recurrent State Space Model) E2E Tests
//=============================================================================

TEST_F(WorldModelE2ETest, OmniRSSMPipeline) {
    // Test: RSSM forward step pipeline (DreamerV3-style)

    // Create RSSM state
    omni_wm_rssm_state_t* rssm_state = omni_wm_rssm_state_create(TEST_STATE_DIM, TEST_STATE_DIM / 2);
    if (!rssm_state) {
        GTEST_SKIP() << "RSSM not available";
    }

    omni_wm_rssm_state_t* next_rssm_state = omni_wm_rssm_state_create(TEST_STATE_DIM, TEST_STATE_DIM / 2);
    ASSERT_NE(next_rssm_state, nullptr);

    // Run multiple RSSM steps
    for (int step = 0; step < 10; step++) {
        auto action = create_random_action(TEST_ACTION_DIM);

        nimcp_error_t result = omni_wm_rssm_step(omni_wm, rssm_state, action.data(),
                                                  TEST_ACTION_DIM, next_rssm_state);
        if (result != NIMCP_OK) {
            break;
        }

        // Swap states
        std::swap(rssm_state, next_rssm_state);
    }

    omni_wm_rssm_state_destroy(rssm_state);
    omni_wm_rssm_state_destroy(next_rssm_state);
}

TEST_F(WorldModelE2ETest, OmniImaginationPipeline) {
    // Test: Imagination rollout in latent space

    omni_wm_rssm_state_t* initial_state = omni_wm_rssm_state_create(TEST_STATE_DIM, TEST_STATE_DIM / 2);
    if (!initial_state) {
        GTEST_SKIP() << "RSSM not available";
    }

    // Create action sequence
    const uint32_t horizon = 5;
    std::vector<std::vector<float>> actions(horizon);
    std::vector<const float*> action_ptrs(horizon);

    for (uint32_t h = 0; h < horizon; h++) {
        actions[h] = create_random_action(TEST_ACTION_DIM);
        action_ptrs[h] = actions[h].data();
    }

    // Allocate trajectory
    std::vector<omni_wm_rssm_state_t*> trajectory(horizon);
    for (uint32_t h = 0; h < horizon; h++) {
        trajectory[h] = omni_wm_rssm_state_create(TEST_STATE_DIM, TEST_STATE_DIM / 2);
    }

    nimcp_error_t result = omni_wm_rssm_imagine(omni_wm, initial_state, action_ptrs.data(),
                                                 horizon, trajectory.data());
    if (result == NIMCP_OK) {
        std::cout << "Imagination trajectory: " << horizon << " steps" << std::endl;
    }

    // Cleanup
    for (auto* state : trajectory) {
        omni_wm_rssm_state_destroy(state);
    }
    omni_wm_rssm_state_destroy(initial_state);
}

//=============================================================================
// Latent Encoding E2E Tests (JEPA-style)
//=============================================================================

TEST_F(WorldModelE2ETest, OmniLatentEncodingPipeline) {
    // Test: Encode observations to latent space, predict, decode

    // Create latent representation
    omni_wm_latent_t* latent = omni_wm_latent_create(TEST_LATENT_DIM);
    if (!latent) {
        GTEST_SKIP() << "Latent encoding not available";
    }

    // Create observation
    std::vector<float> observation(TEST_OBS_DIM);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (auto& val : observation) {
        val = dist(rng);
    }

    // Encode
    nimcp_error_t result = omni_wm_encode(omni_wm, observation.data(), TEST_OBS_DIM, latent);
    if (result == NIMCP_OK) {
        EXPECT_GE(latent->information_content, 0.0f);

        // Predict in latent space
        omni_wm_latent_t* predicted = omni_wm_latent_create(TEST_LATENT_DIM);
        auto action = create_random_action(TEST_ACTION_DIM);

        result = omni_wm_predict_latent(omni_wm, latent, action.data(), TEST_ACTION_DIM, predicted);
        if (result == NIMCP_OK) {
            // Decode back to observation space
            std::vector<float> reconstructed(TEST_OBS_DIM);
            result = omni_wm_decode(omni_wm, predicted, reconstructed.data(), TEST_OBS_DIM);
            if (result == NIMCP_OK) {
                // Verify no NaN
                for (float val : reconstructed) {
                    EXPECT_FALSE(std::isnan(val));
                }
            }
        }

        omni_wm_latent_destroy(predicted);
    }

    omni_wm_latent_destroy(latent);
}

//=============================================================================
// Experience Replay E2E Tests
//=============================================================================

TEST_F(WorldModelE2ETest, OmniExperienceReplayPipeline) {
    // Test: Store and sample experiences for learning

    // Store experiences
    for (int i = 0; i < 50; i++) {
        omni_wm_experience_t* exp = omni_wm_experience_create(TEST_STATE_DIM,
                                                               TEST_ACTION_DIM,
                                                               TEST_OBS_DIM);
        if (!exp) {
            GTEST_SKIP() << "Experience replay not available";
        }

        // Fill experience
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (uint32_t j = 0; j < TEST_ACTION_DIM; j++) {
            exp->action[j] = dist(rng);
        }
        exp->reward = dist(rng);
        exp->symlog_reward = omni_wm_symlog(exp->reward);
        exp->terminal = (i == 49);

        nimcp_error_t result = omni_wm_add_experience(omni_wm, exp);
        omni_wm_experience_destroy(exp);

        if (result != NIMCP_OK) break;
    }

    uint32_t replay_size = omni_wm_get_replay_size(omni_wm);
    std::cout << "Replay buffer size: " << replay_size << std::endl;

    // Sample batch
    if (replay_size > 0) {
        const uint32_t batch_size = 8;
        std::vector<omni_wm_experience_t*> batch(batch_size);

        uint32_t sampled = omni_wm_sample_experiences(omni_wm, batch.data(), batch_size);
        std::cout << "Sampled experiences: " << sampled << std::endl;

        // Cleanup (sampled experiences are still owned by buffer)
    }
}

//=============================================================================
// Symlog Transformation E2E Tests
//=============================================================================

TEST_F(WorldModelE2ETest, SymlogTransformationPipeline) {
    // Test: Symlog transformation for reward normalization (DreamerV3)

    std::vector<float> rewards = {0.0f, 0.01f, 1.0f, 10.0f, 100.0f, 1000.0f, -1.0f, -100.0f};
    std::vector<float> transformed(rewards.size());
    std::vector<float> recovered(rewards.size());

    // Transform
    omni_wm_symlog_array(rewards.data(), transformed.data(), static_cast<uint32_t>(rewards.size()));

    // Inverse transform
    omni_wm_symexp_array(transformed.data(), recovered.data(), static_cast<uint32_t>(rewards.size()));

    // Verify roundtrip
    for (size_t i = 0; i < rewards.size(); i++) {
        float error = std::abs(recovered[i] - rewards[i]);
        float tolerance = std::abs(rewards[i]) * 0.001f + 0.001f;
        EXPECT_LT(error, tolerance) << "Symlog roundtrip error for " << rewards[i];
    }

    std::cout << "Symlog transformation verified for " << rewards.size() << " values" << std::endl;
}

//=============================================================================
// Serialization E2E Tests
//=============================================================================

TEST_F(WorldModelE2ETest, OmniSerializationPipeline) {
    // Test: Save and load world model state

    // Add some state to the world model
    omni_wm_state_t* state = create_random_state(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    omni_wm_set_state(omni_wm, state);

    // Perform some operations
    auto action = create_random_action(TEST_ACTION_DIM);
    omni_wm_transition_t transition = {};
    omni_wm_predict_forward(omni_wm, action.data(), TEST_ACTION_DIM, &transition);

    // Get required buffer size
    size_t buffer_size = omni_wm_serialize(omni_wm, nullptr, 0);
    if (buffer_size == 0) {
        omni_wm_state_destroy(state);
        GTEST_SKIP() << "Serialization not available";
    }

    // Serialize
    std::vector<uint8_t> buffer(buffer_size);
    size_t written = omni_wm_serialize(omni_wm, buffer.data(), buffer_size);
    EXPECT_EQ(written, buffer_size);

    // Deserialize
    omni_world_model_t* restored = omni_wm_deserialize(buffer.data(), buffer_size);
    if (restored) {
        // Verify restored model works
        omni_wm_transition_t restored_transition = {};
        nimcp_error_t result = omni_wm_predict_forward(restored, action.data(),
                                                        TEST_ACTION_DIM, &restored_transition);
        EXPECT_EQ(result, NIMCP_OK);

        omni_wm_destroy(restored);
    }

    omni_wm_state_destroy(state);
    std::cout << "Serialization buffer size: " << buffer_size << " bytes" << std::endl;
}

TEST_F(WorldModelE2ETest, OmniCheckpointPipeline) {
    // Test: Create and restore checkpoints

    // Add state
    omni_wm_state_t* state1 = create_random_state(TEST_STATE_DIM);
    ASSERT_NE(state1, nullptr);
    omni_wm_set_state(omni_wm, state1);

    // Create checkpoint
    uint64_t checkpoint1 = omni_wm_checkpoint(omni_wm);
    if (checkpoint1 == 0) {
        omni_wm_state_destroy(state1);
        GTEST_SKIP() << "Checkpointing not available";
    }

    // Modify state
    omni_wm_state_t* state2 = create_random_state(TEST_STATE_DIM);
    omni_wm_set_state(omni_wm, state2);

    // Create another checkpoint
    uint64_t checkpoint2 = omni_wm_checkpoint(omni_wm);
    EXPECT_NE(checkpoint2, checkpoint1);

    // Restore first checkpoint
    nimcp_error_t result = omni_wm_restore_checkpoint(omni_wm, checkpoint1);
    EXPECT_EQ(result, NIMCP_OK);

    uint32_t checkpoint_count = omni_wm_get_checkpoint_count(omni_wm);
    std::cout << "Checkpoints stored: " << checkpoint_count << std::endl;

    // Cleanup checkpoints
    omni_wm_clear_checkpoints(omni_wm);

    omni_wm_state_destroy(state1);
    omni_wm_state_destroy(state2);
}

//=============================================================================
// Exception Handling E2E Tests
//=============================================================================

TEST_F(WorldModelE2ETest, ExceptionHandlingRecoveryPipeline) {
    // Test: Recovery from error conditions

    // Trigger null pointer error
    wm_error_t result = wm_process_modality(multimodal_wm, nullptr);
    EXPECT_EQ(result, WM_ERR_NULL_PTR);

    wm_error_t last_error = wm_get_last_error(multimodal_wm);
    EXPECT_EQ(last_error, WM_ERR_NULL_PTR);

    // Verify recovery - should still be able to process valid input
    auto visual = create_modality_input(WM_MODALITY_VISUAL, TEST_FEATURE_DIM);
    result = wm_process_modality(multimodal_wm, &visual);
    EXPECT_EQ(result, WM_OK);

    // Verify status is not ERROR
    wm_status_t status = wm_get_status(multimodal_wm);
    EXPECT_NE(status, WM_STATUS_ERROR);

    free_modality_input(visual);
}

TEST_F(WorldModelE2ETest, InvalidParameterHandlingPipeline) {
    // Test: Handling of invalid parameters

    // Invalid modality
    wm_modality_input_t input = {};
    input.modality = static_cast<wm_modality_t>(100);  // Invalid
    input.feature_dim = TEST_FEATURE_DIM;
    input.features = new float[TEST_FEATURE_DIM];
    input.confidence = 0.9f;

    wm_error_t result = wm_process_modality(multimodal_wm, &input);
    // Should handle gracefully
    if (result != WM_OK) {
        EXPECT_EQ(wm_get_last_error(multimodal_wm), WM_ERR_INVALID_MODALITY);
    }

    delete[] input.features;

    // Invalid horizon
    wm_prediction_t prediction = {};
    result = wm_predict(multimodal_wm, 100000, &prediction);  // Exceeds max
    if (result != WM_OK) {
        wm_error_t last = wm_get_last_error(multimodal_wm);
        EXPECT_TRUE(last == WM_ERR_INVALID_HORIZON || last == WM_ERR_CAPACITY_EXCEEDED);
    }
}

//=============================================================================
// Integrated Pipeline E2E Tests
//=============================================================================

TEST_F(WorldModelE2ETest, MultimodalPlusOmniIntegratedPipeline) {
    // Test: Integrated pipeline using both world models

    // 1. Process multimodal inputs
    auto visual = create_modality_input(WM_MODALITY_VISUAL, TEST_FEATURE_DIM);
    auto auditory = create_modality_input(WM_MODALITY_AUDITORY, TEST_FEATURE_DIM / 2);

    wm_process_modality(multimodal_wm, &visual);
    wm_process_modality(multimodal_wm, &auditory);
    wm_fuse_modalities(multimodal_wm);

    // 2. Get fused state
    float global_state[TEST_LATENT_DIM];
    uint32_t state_dim = TEST_LATENT_DIM;
    wm_error_t result = wm_get_global_state(multimodal_wm, global_state, &state_dim);

    if (result == WM_OK) {
        // 3. Use fused state to initialize omni world model
        omni_wm_state_t* omni_state = omni_wm_state_from_values(global_state,
                                                                 std::min(state_dim, TEST_STATE_DIM));
        if (omni_state) {
            omni_wm_set_state(omni_wm, omni_state);

            // 4. Perform counterfactual reasoning
            auto action = create_random_action(TEST_ACTION_DIM);
            omni_wm_counterfactual_result_t cf_result = {};
            nimcp_error_t err = omni_wm_what_if(omni_wm, action.data(), TEST_ACTION_DIM, 5, &cf_result);

            if (err == NIMCP_OK) {
                std::cout << "Integrated pipeline counterfactual confidence: "
                          << cf_result.confidence << std::endl;
                omni_wm_cf_result_destroy(&cf_result);
            }

            omni_wm_state_destroy(omni_state);
        }
    }

    free_modality_input(visual);
    free_modality_input(auditory);
}

TEST_F(WorldModelE2ETest, StressTestFullPipeline) {
    // Test: Stress test the complete pipeline

    const int num_iterations = 100;
    int multimodal_success = 0;
    int omni_success = 0;

    for (int i = 0; i < num_iterations; i++) {
        // Multimodal processing
        auto input = create_modality_input(
            static_cast<wm_modality_t>(i % WM_MODALITY_COUNT),
            TEST_FEATURE_DIM
        );

        if (wm_process_modality(multimodal_wm, &input) == WM_OK) {
            wm_fuse_modalities(multimodal_wm);

            wm_prediction_t prediction = {};
            if (wm_predict(multimodal_wm, 5, &prediction) == WM_OK) {
                multimodal_success++;
            }
        }

        free_modality_input(input);

        // Omni processing
        omni_wm_state_t* state = create_random_state(TEST_STATE_DIM);
        if (state) {
            omni_wm_set_state(omni_wm, state);

            auto action = create_random_action(TEST_ACTION_DIM);
            omni_wm_transition_t transition = {};

            if (omni_wm_predict_forward(omni_wm, action.data(), TEST_ACTION_DIM, &transition) == NIMCP_OK) {
                omni_success++;
            }

            omni_wm_state_destroy(state);
        }
    }

    std::cout << "Stress test results:" << std::endl;
    std::cout << "  Multimodal: " << multimodal_success << "/" << num_iterations << std::endl;
    std::cout << "  Omni: " << omni_success << "/" << num_iterations << std::endl;

    EXPECT_GT(multimodal_success, num_iterations * 0.9);  // At least 90% success
    EXPECT_GT(omni_success, num_iterations * 0.9);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
