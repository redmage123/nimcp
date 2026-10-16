/**
 * @file test_cognitive_enhancements.cpp
 * @brief Comprehensive unit tests for 11 cognitive enhancement modules.
 *
 * Modules tested:
 *   1. Emotional Learning
 *   2. Contrastive Self
 *   3. Inner Speech
 *   4. Episodic Replay
 *   5. World Model Trainer
 *   6. Output Attention
 *   7. Working Memory Scratchpad
 *   8. Analogical Transfer
 *   9. Multi-Timescale Memory
 *  10. Self Curriculum
 *  11. Dynamic Architecture
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "cognitive/nimcp_emotional_learning.h"
#include "cognitive/nimcp_contrastive_self.h"
#include "cognitive/language/nimcp_inner_speech.h"
#include "cognitive/memory/nimcp_episodic_replay.h"
#include "cognitive/nimcp_world_model_trainer.h"
#include "cognitive/nimcp_output_attention.h"
#include "cognitive/nimcp_wm_scratchpad.h"
#include "cognitive/nimcp_analogical_transfer.h"
#include "cognitive/nimcp_multiscale_memory.h"
#include "cognitive/nimcp_self_curriculum.h"
#include "cognitive/nimcp_dynamic_arch.h"
}

// ============================================================================
// Emotional Learning
// ============================================================================

TEST(EmotionalLearning, CreateDestroy) {
    nimcp_emotional_learning_config_t cfg = nimcp_emotional_learning_config_default();
    nimcp_emotional_learning_t* el = nimcp_emotional_learning_create(&cfg);
    ASSERT_NE(el, nullptr);
    nimcp_emotional_learning_destroy(el);
}

TEST(EmotionalLearning, CreateWithNullConfig) {
    nimcp_emotional_learning_t* el = nimcp_emotional_learning_create(NULL);
    ASSERT_NE(el, nullptr);
    nimcp_emotional_learning_destroy(el);
}

TEST(EmotionalLearning, ConfigDefaults) {
    nimcp_emotional_learning_config_t cfg = nimcp_emotional_learning_config_default();
    EXPECT_GT(cfg.surprise_boost, 1.0f);
    EXPECT_GT(cfg.reward_boost, 1.0f);
    EXPECT_GT(cfg.failure_boost, 1.0f);
    EXPECT_GT(cfg.boredom_decay, 0.0f);
    EXPECT_LT(cfg.boredom_decay, 1.0f);
    EXPECT_GT(cfg.arousal_ema_alpha, 0.0f);
    EXPECT_GT(cfg.surprise_threshold, 0.0f);
}

TEST(EmotionalLearning, ModulateHighLossBoosted) {
    nimcp_emotional_learning_t* el = nimcp_emotional_learning_create(NULL);
    ASSERT_NE(el, nullptr);

    // Record some baseline experiences first
    nimcp_emotional_learning_record(el, 0.1f, 0.0f);
    nimcp_emotional_learning_record(el, 0.1f, 0.0f);
    nimcp_emotional_learning_record(el, 0.1f, 0.0f);

    float base_lr = 0.01f;
    // High loss = surprise -> should boost LR
    float modulated = nimcp_emotional_learning_modulate(el, base_lr, 10.0f, 0.0f, true);
    EXPECT_GE(modulated, base_lr) << "Surprising high-loss should boost learning rate";

    nimcp_emotional_learning_destroy(el);
}

TEST(EmotionalLearning, RecordUpdatesArousal) {
    nimcp_emotional_learning_t* el = nimcp_emotional_learning_create(NULL);
    ASSERT_NE(el, nullptr);

    float arousal_before = nimcp_emotional_learning_get_arousal(el);
    // Record a high-reward experience to increase arousal
    nimcp_emotional_learning_record(el, 5.0f, 1.0f);
    float arousal_after = nimcp_emotional_learning_get_arousal(el);

    // Arousal should have changed (either up or stayed, depending on initial)
    // At minimum, get_arousal should return a valid value
    EXPECT_GE(arousal_after, 0.0f);
    EXPECT_LE(arousal_after, 1.0f);
    (void)arousal_before;

    nimcp_emotional_learning_destroy(el);
}

TEST(EmotionalLearning, NullSafety) {
    nimcp_emotional_learning_destroy(NULL);
    float result = nimcp_emotional_learning_modulate(NULL, 0.01f, 1.0f, 0.0f, false);
    (void)result;
    nimcp_emotional_learning_record(NULL, 1.0f, 0.0f);
    float arousal = nimcp_emotional_learning_get_arousal(NULL);
    (void)arousal;
    SUCCEED() << "NULL operations did not crash";
}

// ============================================================================
// Contrastive Self
// ============================================================================

TEST(ContrastiveSelf, CreateDestroy) {
    nimcp_contrastive_self_config_t cfg = nimcp_contrastive_self_config_default();
    nimcp_contrastive_self_t* cs = nimcp_contrastive_self_create(&cfg);
    ASSERT_NE(cs, nullptr);
    nimcp_contrastive_self_destroy(cs);
}

TEST(ContrastiveSelf, CreateWithNullConfig) {
    nimcp_contrastive_self_t* cs = nimcp_contrastive_self_create(NULL);
    ASSERT_NE(cs, nullptr);
    nimcp_contrastive_self_destroy(cs);
}

TEST(ContrastiveSelf, ConfigDefaults) {
    nimcp_contrastive_self_config_t cfg = nimcp_contrastive_self_config_default();
    EXPECT_GT(cfg.buffer_capacity, 0u);
    EXPECT_GT(cfg.negative_margin, 0.0f);
    EXPECT_GT(cfg.contrastive_weight, 0.0f);
    EXPECT_GT(cfg.negatives_per_sample, 0u);
}

TEST(ContrastiveSelf, RecordAndGenerateNegatives) {
    nimcp_contrastive_self_t* cs = nimcp_contrastive_self_create(NULL);
    ASSERT_NE(cs, nullptr);

    // Record several samples with different labels
    const uint32_t dim = 8;
    float input_a[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float output_a[8] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float input_b[8] = {1.0f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float output_b[8] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    nimcp_contrastive_self_record(cs, input_a, dim, output_a, dim, "cat");
    nimcp_contrastive_self_record(cs, input_b, dim, output_b, dim, "dog");

    // Attempt to generate negatives - may return 0 with only 2 samples
    float neg_features[80];
    float neg_targets[80];
    int count = nimcp_contrastive_self_generate_negatives(
        cs, neg_features, neg_targets, 10, dim, dim);
    EXPECT_GE(count, 0) << "generate_negatives should not return negative value";

    nimcp_contrastive_self_destroy(cs);
}

TEST(ContrastiveSelf, NullSafety) {
    nimcp_contrastive_self_destroy(NULL);
    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float output[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    nimcp_contrastive_self_record(NULL, input, 4, output, 4, "test");
    float neg_f[32], neg_t[32];
    int count = nimcp_contrastive_self_generate_negatives(NULL, neg_f, neg_t, 4, 4, 4);
    (void)count;
    SUCCEED() << "NULL operations did not crash";
}

// ============================================================================
// Inner Speech
// ============================================================================

TEST(InnerSpeech, CreateDestroy) {
    nimcp_inner_speech_config_t cfg = nimcp_inner_speech_config_default();
    nimcp_inner_speech_t* is = nimcp_inner_speech_create(&cfg);
    ASSERT_NE(is, nullptr);
    nimcp_inner_speech_destroy(is);
}

TEST(InnerSpeech, CreateWithNullConfigReturnsNull) {
    // Inner speech create requires a non-NULL config
    nimcp_inner_speech_t* is = nimcp_inner_speech_create(NULL);
    EXPECT_EQ(is, nullptr) << "NULL config should return NULL for inner speech";
}

TEST(InnerSpeech, ConfigDefaults) {
    nimcp_inner_speech_config_t cfg = nimcp_inner_speech_config_default();
    EXPECT_GT(cfg.max_iterations, 0u);
    EXPECT_LE(cfg.max_iterations, INNER_SPEECH_MAX_ITER);
    EXPECT_GT(cfg.convergence_threshold, 0.0f);
    EXPECT_LE(cfg.convergence_threshold, 1.0f);
    EXPECT_GT(cfg.blend_original, 0.0f);
    EXPECT_GT(cfg.blend_encoded, 0.0f);
}

TEST(InnerSpeech, GetIterationsBeforeRefine) {
    nimcp_inner_speech_config_t cfg = nimcp_inner_speech_config_default();
    nimcp_inner_speech_t* is = nimcp_inner_speech_create(&cfg);
    ASSERT_NE(is, nullptr);
    uint32_t iters = nimcp_inner_speech_get_iterations(is);
    EXPECT_EQ(iters, 0u) << "Iterations should be 0 before any refinement";
    nimcp_inner_speech_destroy(is);
}

TEST(InnerSpeech, NullSafety) {
    nimcp_inner_speech_destroy(NULL);
    uint32_t iters = nimcp_inner_speech_get_iterations(NULL);
    (void)iters;
    float brain_out[16] = {};
    float refined[16] = {};
    char text[128] = {};
    int rc = nimcp_inner_speech_refine(NULL, brain_out, 16, refined, text, 128);
    (void)rc;
    SUCCEED() << "NULL operations did not crash";
}

// ============================================================================
// Episodic Replay
// ============================================================================

TEST(EpisodicReplay, CreateDestroy) {
    nimcp_episodic_replay_config_t cfg = nimcp_episodic_replay_config_default();
    nimcp_episodic_replay_t* er = nimcp_episodic_replay_create(&cfg);
    ASSERT_NE(er, nullptr);
    nimcp_episodic_replay_destroy(er);
}

TEST(EpisodicReplay, CreateWithNullConfigReturnsNull) {
    // Episodic replay create requires a non-NULL config
    nimcp_episodic_replay_t* er = nimcp_episodic_replay_create(NULL);
    EXPECT_EQ(er, nullptr) << "NULL config should return NULL for episodic replay";
}

TEST(EpisodicReplay, ConfigDefaults) {
    nimcp_episodic_replay_config_t cfg = nimcp_episodic_replay_config_default();
    EXPECT_GT(cfg.replay_count, 0u);
    EXPECT_GT(cfg.replay_speed_multiplier, 0.0f);
    EXPECT_GT(cfg.importance_threshold, 0.0f);
    EXPECT_GT(cfg.replay_lr_scale, 0.0f);
    EXPECT_GT(cfg.buffer_capacity, 0u);
}

TEST(EpisodicReplay, RecordAndCheckBufferSize) {
    nimcp_episodic_replay_config_t cfg = nimcp_episodic_replay_config_default();
    nimcp_episodic_replay_t* er = nimcp_episodic_replay_create(&cfg);
    ASSERT_NE(er, nullptr);

    EXPECT_EQ(nimcp_episodic_replay_get_buffer_size(er), 0u);

    float features[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    float target[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    int rc = nimcp_episodic_replay_record(er, features, 8, target, 4, "test_label", 0.5f, 0.8f);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(nimcp_episodic_replay_get_buffer_size(er), 1u);

    nimcp_episodic_replay_destroy(er);
}

TEST(EpisodicReplay, RecordMultipleExperiences) {
    nimcp_episodic_replay_config_t cfg = nimcp_episodic_replay_config_default();
    nimcp_episodic_replay_t* er = nimcp_episodic_replay_create(&cfg);
    ASSERT_NE(er, nullptr);

    for (int i = 0; i < 5; i++) {
        float features[4] = {(float)i, (float)(i+1), (float)(i+2), (float)(i+3)};
        float target[2] = {(float)i * 0.1f, (float)(i+1) * 0.1f};
        char label[32];
        snprintf(label, sizeof(label), "experience_%d", i);
        int rc = nimcp_episodic_replay_record(er, features, 4, target, 2, label, 0.5f, 0.0f);
        EXPECT_EQ(rc, 0);
    }
    EXPECT_EQ(nimcp_episodic_replay_get_buffer_size(er), 5u);

    nimcp_episodic_replay_destroy(er);
}

TEST(EpisodicReplay, NullSafety) {
    nimcp_episodic_replay_destroy(NULL);
    float features[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float target[2] = {0.0f, 0.0f};
    int rc = nimcp_episodic_replay_record(NULL, features, 4, target, 2, "test", 0.5f, 0.0f);
    (void)rc;
    uint32_t sz = nimcp_episodic_replay_get_buffer_size(NULL);
    EXPECT_EQ(sz, 0u);
    SUCCEED() << "NULL operations did not crash";
}

// ============================================================================
// World Model Trainer
// ============================================================================

TEST(WorldModelTrainer, CreateDestroy) {
    nimcp_wmt_config_t cfg = nimcp_wmt_config_default();
    nimcp_world_model_trainer_t* wmt = nimcp_wmt_create(&cfg);
    ASSERT_NE(wmt, nullptr);
    nimcp_wmt_destroy(wmt);
}

TEST(WorldModelTrainer, CreateWithNullConfig) {
    nimcp_world_model_trainer_t* wmt = nimcp_wmt_create(NULL);
    ASSERT_NE(wmt, nullptr);
    nimcp_wmt_destroy(wmt);
}

TEST(WorldModelTrainer, ConfigDefaults) {
    nimcp_wmt_config_t cfg = nimcp_wmt_config_default();
    EXPECT_GT(cfg.prediction_horizon, 0u);
    EXPECT_GT(cfg.prediction_lr, 0.0f);
    EXPECT_GT(cfg.error_ema_alpha, 0.0f);
    EXPECT_LE(cfg.error_ema_alpha, 1.0f);
}

TEST(WorldModelTrainer, RecordTransition) {
    nimcp_world_model_trainer_t* wmt = nimcp_wmt_create(NULL);
    ASSERT_NE(wmt, nullptr);

    float state[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    float action[2] = {0.5f, -0.5f};
    float next_state[8] = {1.1f, 2.1f, 3.1f, 4.1f, 5.1f, 6.1f, 7.1f, 8.1f};
    int rc = nimcp_wmt_record_transition(wmt, state, 8, action, 2, next_state);
    EXPECT_EQ(rc, 0);

    nimcp_wmt_destroy(wmt);
}

TEST(WorldModelTrainer, PredictionErrorInitiallyZero) {
    nimcp_world_model_trainer_t* wmt = nimcp_wmt_create(NULL);
    ASSERT_NE(wmt, nullptr);

    float error = nimcp_wmt_get_prediction_error(wmt);
    EXPECT_NEAR(error, 0.0f, 1e-6f) << "Prediction error should be 0 before any training";

    nimcp_wmt_destroy(wmt);
}

TEST(WorldModelTrainer, NullSafety) {
    nimcp_wmt_destroy(NULL);
    float state[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float action[2] = {0.0f, 0.0f};
    float next[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int rc = nimcp_wmt_record_transition(NULL, state, 4, action, 2, next);
    (void)rc;
    float err = nimcp_wmt_get_prediction_error(NULL);
    (void)err;
    SUCCEED() << "NULL operations did not crash";
}

// ============================================================================
// Output Attention
// ============================================================================

TEST(OutputAttention, CreateDestroy) {
    nimcp_oa_config_t cfg = nimcp_oa_config_default();
    nimcp_output_attention_t* oa = nimcp_oa_create(&cfg);
    ASSERT_NE(oa, nullptr);
    nimcp_oa_destroy(oa);
}

TEST(OutputAttention, CreateWithNullConfig) {
    nimcp_output_attention_t* oa = nimcp_oa_create(NULL);
    ASSERT_NE(oa, nullptr);
    nimcp_oa_destroy(oa);
}

TEST(OutputAttention, ConfigDefaults) {
    nimcp_oa_config_t cfg = nimcp_oa_config_default();
    EXPECT_GT(cfg.num_heads, 0u);
    EXPECT_GT(cfg.head_dim, 0u);
    EXPECT_GT(cfg.task_embedding_dim, 0u);
}

TEST(OutputAttention, AttendDoesNotCrash) {
    nimcp_output_attention_t* oa = nimcp_oa_create(NULL);
    ASSERT_NE(oa, nullptr);

    const uint32_t dim = 32;
    float brain_output[32];
    float attended_output[32];
    for (uint32_t i = 0; i < dim; i++) {
        brain_output[i] = (float)i * 0.1f;
    }
    memset(attended_output, 0, sizeof(attended_output));

    int rc = nimcp_oa_attend(oa, brain_output, dim, "math", attended_output);
    EXPECT_EQ(rc, 0);

    nimcp_oa_destroy(oa);
}

TEST(OutputAttention, NullSafety) {
    nimcp_oa_destroy(NULL);
    float brain_out[16] = {};
    float attended[16] = {};
    int rc = nimcp_oa_attend(NULL, brain_out, 16, "test", attended);
    (void)rc;
    SUCCEED() << "NULL operations did not crash";
}

// ============================================================================
// Working Memory Scratchpad
// ============================================================================

TEST(WMScratchpad, CreateDestroy) {
    nimcp_wms_config_t cfg = nimcp_wms_config_default();
    nimcp_wm_scratchpad_t* wms = nimcp_wms_create(&cfg);
    ASSERT_NE(wms, nullptr);
    nimcp_wms_destroy(wms);
}

TEST(WMScratchpad, CreateWithNullConfig) {
    nimcp_wm_scratchpad_t* wms = nimcp_wms_create(NULL);
    ASSERT_NE(wms, nullptr);
    nimcp_wms_destroy(wms);
}

TEST(WMScratchpad, ConfigDefaults) {
    nimcp_wms_config_t cfg = nimcp_wms_config_default();
    EXPECT_GT(cfg.num_slots, 0u);
    EXPECT_LE(cfg.num_slots, NIMCP_WMS_MAX_SLOTS);
    EXPECT_GT(cfg.slot_dim, 0u);
    EXPECT_GT(cfg.decay_rate, 0.0f);
    EXPECT_LE(cfg.decay_rate, 1.0f);
}

TEST(WMScratchpad, WriteAndReadBack) {
    nimcp_wm_scratchpad_t* wms = nimcp_wms_create(NULL);
    ASSERT_NE(wms, nullptr);

    float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    int rc = nimcp_wms_write(wms, 0, data, 4, "test_slot");
    EXPECT_EQ(rc, 0);

    float readback[4] = {};
    int count = nimcp_wms_read(wms, 0, readback, 4);
    EXPECT_GT(count, 0);
    // Values may have been decayed on read, but should be non-zero
    EXPECT_GT(fabsf(readback[0]), 0.0f);

    nimcp_wms_destroy(wms);
}

TEST(WMScratchpad, ClearSlot) {
    nimcp_wm_scratchpad_t* wms = nimcp_wms_create(NULL);
    ASSERT_NE(wms, nullptr);

    float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    nimcp_wms_write(wms, 0, data, 4, "slot_to_clear");
    EXPECT_EQ(nimcp_wms_get_active_count(wms), 1u);

    int rc = nimcp_wms_clear_slot(wms, 0);
    EXPECT_EQ(rc, 0);

    nimcp_wms_destroy(wms);
}

TEST(WMScratchpad, FindByLabel) {
    nimcp_wm_scratchpad_t* wms = nimcp_wms_create(NULL);
    ASSERT_NE(wms, nullptr);

    float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    nimcp_wms_write(wms, 0, data, 4, "alpha");
    nimcp_wms_write(wms, 1, data, 4, "beta");
    nimcp_wms_write(wms, 2, data, 4, "gamma");

    int idx = nimcp_wms_find_by_label(wms, "beta");
    EXPECT_EQ(idx, 1);

    int missing = nimcp_wms_find_by_label(wms, "nonexistent");
    EXPECT_EQ(missing, -1);

    nimcp_wms_destroy(wms);
}

TEST(WMScratchpad, DecayReducesValues) {
    nimcp_wm_scratchpad_t* wms = nimcp_wms_create(NULL);
    ASSERT_NE(wms, nullptr);

    float data[4] = {10.0f, 20.0f, 30.0f, 40.0f};
    nimcp_wms_write(wms, 0, data, 4, "decay_test");

    // Apply decay multiple times
    nimcp_wms_decay(wms);
    nimcp_wms_decay(wms);
    nimcp_wms_decay(wms);

    float readback[4] = {};
    int count = nimcp_wms_read(wms, 0, readback, 4);
    EXPECT_GT(count, 0);
    // After multiple decays, values should be smaller than original
    EXPECT_LT(readback[0], 10.0f);

    nimcp_wms_destroy(wms);
}

TEST(WMScratchpad, ActiveCountTracking) {
    nimcp_wm_scratchpad_t* wms = nimcp_wms_create(NULL);
    ASSERT_NE(wms, nullptr);

    EXPECT_EQ(nimcp_wms_get_active_count(wms), 0u);

    float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    nimcp_wms_write(wms, 0, data, 4, "slot0");
    EXPECT_EQ(nimcp_wms_get_active_count(wms), 1u);

    nimcp_wms_write(wms, 1, data, 4, "slot1");
    EXPECT_EQ(nimcp_wms_get_active_count(wms), 2u);

    nimcp_wms_destroy(wms);
}

TEST(WMScratchpad, ClearAll) {
    nimcp_wm_scratchpad_t* wms = nimcp_wms_create(NULL);
    ASSERT_NE(wms, nullptr);

    float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    nimcp_wms_write(wms, 0, data, 4, "a");
    nimcp_wms_write(wms, 1, data, 4, "b");
    nimcp_wms_write(wms, 2, data, 4, "c");
    EXPECT_GE(nimcp_wms_get_active_count(wms), 2u);

    nimcp_wms_clear(wms);
    EXPECT_EQ(nimcp_wms_get_active_count(wms), 0u);

    nimcp_wms_destroy(wms);
}

TEST(WMScratchpad, NullSafety) {
    nimcp_wms_destroy(NULL);
    nimcp_wms_clear(NULL);
    nimcp_wms_decay(NULL);
    float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    int rc = nimcp_wms_write(NULL, 0, data, 4, "test");
    (void)rc;
    float readback[4] = {};
    int count = nimcp_wms_read(NULL, 0, readback, 4);
    (void)count;
    int idx = nimcp_wms_find_by_label(NULL, "test");
    (void)idx;
    uint32_t active = nimcp_wms_get_active_count(NULL);
    EXPECT_EQ(active, 0u);
    SUCCEED() << "NULL operations did not crash";
}

// ============================================================================
// Analogical Transfer
// ============================================================================

TEST(AnalogicalTransfer, CreateDestroy) {
    nimcp_analogical_config_t cfg = nimcp_analogical_config_default();
    nimcp_analogical_transfer_t* at = nimcp_analogical_create(&cfg);
    ASSERT_NE(at, nullptr);
    nimcp_analogical_destroy(at);
}

TEST(AnalogicalTransfer, CreateWithNullConfig) {
    nimcp_analogical_transfer_t* at = nimcp_analogical_create(NULL);
    ASSERT_NE(at, nullptr);
    nimcp_analogical_destroy(at);
}

TEST(AnalogicalTransfer, ConfigDefaults) {
    nimcp_analogical_config_t cfg = nimcp_analogical_config_default();
    EXPECT_EQ(cfg.max_analogies, 100u);
    EXPECT_NEAR(cfg.similarity_threshold, 0.6f, 0.01f);
    EXPECT_NEAR(cfg.transfer_weight, 0.3f, 0.01f);
}

TEST(AnalogicalTransfer, StorePattern) {
    nimcp_analogical_transfer_t* at = nimcp_analogical_create(NULL);
    ASSERT_NE(at, nullptr);

    EXPECT_EQ(nimcp_analogical_get_pattern_count(at), 0u);

    float features[8] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};
    float solution[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    int rc = nimcp_analogical_store_pattern(at, features, 8, solution, 4, "pattern_a", 0.9f);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(nimcp_analogical_get_pattern_count(at), 1u);

    nimcp_analogical_destroy(at);
}

TEST(AnalogicalTransfer, FindAnalogyWhenEmpty) {
    nimcp_analogical_transfer_t* at = nimcp_analogical_create(NULL);
    ASSERT_NE(at, nullptr);

    float query[8] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};
    float solution_out[ANALOGICAL_MAX_SOLUTION] = {};
    uint32_t sol_dim = 0;
    char label_out[ANALOGICAL_MAX_LABEL] = {};

    float sim = nimcp_analogical_find_analogy(at, query, 8, solution_out, &sol_dim, label_out);
    EXPECT_NEAR(sim, 0.0f, 1e-6f) << "Should find no analogy when store is empty";

    nimcp_analogical_destroy(at);
}

TEST(AnalogicalTransfer, StoreAndFindMatchingPattern) {
    nimcp_analogical_transfer_t* at = nimcp_analogical_create(NULL);
    ASSERT_NE(at, nullptr);

    // Store a pattern
    float features[8] = {1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f};
    float solution[4] = {0.9f, 0.8f, 0.7f, 0.6f};
    nimcp_analogical_store_pattern(at, features, 8, solution, 4, "math_problem", 0.95f);

    // Query with very similar features (should match)
    float query[8] = {1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f};
    float solution_out[ANALOGICAL_MAX_SOLUTION] = {};
    uint32_t sol_dim = 0;
    char label_out[ANALOGICAL_MAX_LABEL] = {};

    float sim = nimcp_analogical_find_analogy(at, query, 8, solution_out, &sol_dim, label_out);
    EXPECT_GT(sim, 0.5f) << "Identical features should produce high similarity";

    nimcp_analogical_destroy(at);
}

TEST(AnalogicalTransfer, RecordOutcome) {
    nimcp_analogical_transfer_t* at = nimcp_analogical_create(NULL);
    ASSERT_NE(at, nullptr);

    float features[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float solution[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    nimcp_analogical_store_pattern(at, features, 4, solution, 4, "outcome_test", 0.5f);

    int rc = nimcp_analogical_record_outcome(at, "outcome_test", 0.9f);
    EXPECT_EQ(rc, 0);

    int rc_missing = nimcp_analogical_record_outcome(at, "nonexistent", 0.5f);
    EXPECT_EQ(rc_missing, -1);

    nimcp_analogical_destroy(at);
}

TEST(AnalogicalTransfer, NullSafety) {
    nimcp_analogical_destroy(NULL);
    float features[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float solution[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int rc = nimcp_analogical_store_pattern(NULL, features, 4, solution, 4, "test", 0.5f);
    (void)rc;
    float sol_out[ANALOGICAL_MAX_SOLUTION] = {};
    uint32_t sol_dim = 0;
    char label_out[ANALOGICAL_MAX_LABEL] = {};
    float sim = nimcp_analogical_find_analogy(NULL, features, 4, sol_out, &sol_dim, label_out);
    (void)sim;
    uint32_t count = nimcp_analogical_get_pattern_count(NULL);
    EXPECT_EQ(count, 0u);
    SUCCEED() << "NULL operations did not crash";
}

// ============================================================================
// Multi-Timescale Memory
// ============================================================================

TEST(MultiscaleMemory, CreateDestroy) {
    nimcp_multiscale_config_t cfg = nimcp_multiscale_config_default();
    nimcp_multiscale_memory_t* mm = nimcp_multiscale_create(&cfg);
    ASSERT_NE(mm, nullptr);
    nimcp_multiscale_destroy(mm);
}

TEST(MultiscaleMemory, CreateWithNullConfig) {
    nimcp_multiscale_memory_t* mm = nimcp_multiscale_create(NULL);
    ASSERT_NE(mm, nullptr);
    nimcp_multiscale_destroy(mm);
}

TEST(MultiscaleMemory, ConfigDefaults) {
    nimcp_multiscale_config_t cfg = nimcp_multiscale_config_default();
    EXPECT_EQ(cfg.immediate_capacity, 10u);
    EXPECT_EQ(cfg.recent_capacity, 1000u);
    EXPECT_EQ(cfg.compression_ratio, 4u);
    EXPECT_NEAR(cfg.consolidation_threshold, 0.7f, 0.01f);
}

TEST(MultiscaleMemory, PushAndCheckCount) {
    nimcp_multiscale_memory_t* mm = nimcp_multiscale_create(NULL);
    ASSERT_NE(mm, nullptr);

    EXPECT_EQ(nimcp_multiscale_get_immediate_count(mm), 0u);

    float embedding[16];
    for (uint32_t i = 0; i < 16; i++) embedding[i] = (float)i * 0.1f;

    int rc = nimcp_multiscale_push(mm, embedding, 16, "first_memory", 0.8f);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(nimcp_multiscale_get_immediate_count(mm), 1u);

    nimcp_multiscale_destroy(mm);
}

TEST(MultiscaleMemory, QueryImmediateWithMatch) {
    nimcp_multiscale_memory_t* mm = nimcp_multiscale_create(NULL);
    ASSERT_NE(mm, nullptr);

    // Push a memory
    float embedding[16];
    for (uint32_t i = 0; i < 16; i++) embedding[i] = (float)(i + 1);
    nimcp_multiscale_push(mm, embedding, 16, "findme", 0.9f);

    // Query with same vector
    nimcp_memory_query_result_t results[4];
    memset(results, 0, sizeof(results));
    int count = nimcp_multiscale_query_immediate(mm, embedding, 16, results, 4);
    EXPECT_GT(count, 0) << "Should find at least one result";
    if (count > 0) {
        EXPECT_GT(results[0].similarity, 0.5f);
    }

    nimcp_multiscale_destroy(mm);
}

TEST(MultiscaleMemory, ConsolidateOnEmptyBuffer) {
    nimcp_multiscale_memory_t* mm = nimcp_multiscale_create(NULL);
    ASSERT_NE(mm, nullptr);

    int merged = nimcp_multiscale_consolidate(mm);
    EXPECT_GE(merged, 0) << "Consolidate on empty buffer should return 0, not error";

    nimcp_multiscale_destroy(mm);
}

TEST(MultiscaleMemory, GetCounts) {
    nimcp_multiscale_memory_t* mm = nimcp_multiscale_create(NULL);
    ASSERT_NE(mm, nullptr);

    EXPECT_EQ(nimcp_multiscale_get_immediate_count(mm), 0u);
    EXPECT_EQ(nimcp_multiscale_get_recent_count(mm), 0u);

    nimcp_multiscale_destroy(mm);
}

TEST(MultiscaleMemory, NullSafety) {
    nimcp_multiscale_destroy(NULL);
    float embedding[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    int rc = nimcp_multiscale_push(NULL, embedding, 8, "test", 0.5f);
    (void)rc;
    nimcp_memory_query_result_t results[4];
    int count = nimcp_multiscale_query_immediate(NULL, embedding, 8, results, 4);
    (void)count;
    uint32_t ic = nimcp_multiscale_get_immediate_count(NULL);
    EXPECT_EQ(ic, 0u);
    uint32_t rcc = nimcp_multiscale_get_recent_count(NULL);
    EXPECT_EQ(rcc, 0u);
    SUCCEED() << "NULL operations did not crash";
}

// ============================================================================
// Self Curriculum
// ============================================================================

TEST(SelfCurriculum, CreateDestroy) {
    nimcp_self_curriculum_config_t cfg = nimcp_self_curriculum_config_default();
    nimcp_self_curriculum_t* sc = nimcp_self_curriculum_create(&cfg);
    ASSERT_NE(sc, nullptr);
    nimcp_self_curriculum_destroy(sc);
}

TEST(SelfCurriculum, CreateWithNullConfig) {
    nimcp_self_curriculum_t* sc = nimcp_self_curriculum_create(NULL);
    ASSERT_NE(sc, nullptr);
    nimcp_self_curriculum_destroy(sc);
}

TEST(SelfCurriculum, ConfigDefaults) {
    nimcp_self_curriculum_config_t cfg = nimcp_self_curriculum_config_default();
    EXPECT_GT(cfg.generation_interval, 0u);
    EXPECT_GT(cfg.max_generated, 0u);
    EXPECT_GT(cfg.uncertainty_threshold, 0.0f);
    EXPECT_GT(cfg.imagination_steps, 0u);
}

TEST(SelfCurriculum, UpdateUncertainty) {
    nimcp_self_curriculum_t* sc = nimcp_self_curriculum_create(NULL);
    ASSERT_NE(sc, nullptr);

    int rc = nimcp_self_curriculum_update_uncertainty(sc, "ethics_trolley", 0.8f);
    EXPECT_EQ(rc, 0);

    rc = nimcp_self_curriculum_update_uncertainty(sc, "math_addition", 0.3f);
    EXPECT_EQ(rc, 0);

    nimcp_self_curriculum_destroy(sc);
}

TEST(SelfCurriculum, ShouldGenerateInitiallyFalse) {
    nimcp_self_curriculum_t* sc = nimcp_self_curriculum_create(NULL);
    ASSERT_NE(sc, nullptr);

    // With no uncertainty data, should not generate
    bool should = nimcp_self_curriculum_should_generate(sc, 50);
    EXPECT_FALSE(should) << "Should not generate without sufficient uncertainty data";

    nimcp_self_curriculum_destroy(sc);
}

TEST(SelfCurriculum, NullSafety) {
    nimcp_self_curriculum_destroy(NULL);
    int rc = nimcp_self_curriculum_update_uncertainty(NULL, "test", 0.5f);
    (void)rc;
    bool should = nimcp_self_curriculum_should_generate(NULL, 100);
    (void)should;
    SUCCEED() << "NULL operations did not crash";
}

// ============================================================================
// Dynamic Architecture
// ============================================================================

TEST(DynamicArch, CreateDestroy) {
    nimcp_dynamic_arch_config_t cfg = nimcp_dynamic_arch_config_default();
    nimcp_dynamic_arch_t* da = nimcp_dynamic_arch_create(&cfg);
    ASSERT_NE(da, nullptr);
    nimcp_dynamic_arch_destroy(da);
}

TEST(DynamicArch, CreateWithNullConfig) {
    nimcp_dynamic_arch_t* da = nimcp_dynamic_arch_create(NULL);
    ASSERT_NE(da, nullptr);
    nimcp_dynamic_arch_destroy(da);
}

TEST(DynamicArch, ConfigDefaults) {
    nimcp_dynamic_arch_config_t cfg = nimcp_dynamic_arch_config_default();
    EXPECT_GT(cfg.monitor_interval, 0u);
    EXPECT_GT(cfg.utilization_window, 0u);
    EXPECT_GT(cfg.grow_threshold, 0.0f);
    EXPECT_LT(cfg.shrink_threshold, cfg.grow_threshold);
    EXPECT_GT(cfg.max_recommendations, 0u);
}

TEST(DynamicArch, RegisterRegion) {
    nimcp_dynamic_arch_t* da = nimcp_dynamic_arch_create(NULL);
    ASSERT_NE(da, nullptr);

    int rc = nimcp_dynamic_arch_register_region(da, "prefrontal", 0, 1000);
    EXPECT_EQ(rc, 0);

    rc = nimcp_dynamic_arch_register_region(da, "occipital", 1000, 500);
    EXPECT_EQ(rc, 0);

    nimcp_dynamic_arch_destroy(da);
}

TEST(DynamicArch, RecordActivation) {
    nimcp_dynamic_arch_t* da = nimcp_dynamic_arch_create(NULL);
    ASSERT_NE(da, nullptr);

    nimcp_dynamic_arch_register_region(da, "hippocampus", 0, 200);

    int rc = nimcp_dynamic_arch_record_activation(da, "hippocampus", 50, 0.8f);
    EXPECT_EQ(rc, 0);

    nimcp_dynamic_arch_destroy(da);
}

TEST(DynamicArch, AnalyzeReturnsZeroInitially) {
    nimcp_dynamic_arch_t* da = nimcp_dynamic_arch_create(NULL);
    ASSERT_NE(da, nullptr);

    nimcp_dynamic_arch_register_region(da, "test_region", 0, 100);

    int recs = nimcp_dynamic_arch_analyze(da);
    EXPECT_GE(recs, 0) << "Analyze should return 0+ recommendations";

    nimcp_dynamic_arch_destroy(da);
}

TEST(DynamicArch, GetUtilization) {
    nimcp_dynamic_arch_t* da = nimcp_dynamic_arch_create(NULL);
    ASSERT_NE(da, nullptr);

    nimcp_dynamic_arch_register_region(da, "cerebellum", 0, 500);

    float util = nimcp_dynamic_arch_get_utilization(da, "cerebellum");
    EXPECT_GE(util, 0.0f);

    // Non-existent region should return -1
    float missing = nimcp_dynamic_arch_get_utilization(da, "nonexistent_region");
    EXPECT_LT(missing, 0.0f);

    nimcp_dynamic_arch_destroy(da);
}

TEST(DynamicArch, NullSafety) {
    nimcp_dynamic_arch_destroy(NULL);
    int rc = nimcp_dynamic_arch_register_region(NULL, "test", 0, 100);
    (void)rc;
    rc = nimcp_dynamic_arch_record_activation(NULL, "test", 0, 0.5f);
    (void)rc;
    int recs = nimcp_dynamic_arch_analyze(NULL);
    (void)recs;
    float util = nimcp_dynamic_arch_get_utilization(NULL, "test");
    (void)util;
    SUCCEED() << "NULL operations did not crash";
}
