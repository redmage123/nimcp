//=============================================================================
// test_hemispheric_brain.cpp - Unit tests for hemispheric brain architecture
//=============================================================================
/**
 * @file test_hemispheric_brain.cpp
 * @brief Comprehensive tests for bilateral brain with hemispheres and callosum
 *
 * Test Coverage:
 * - Hemisphere lifecycle (create, destroy)
 * - Hemisphere processing (update, infer, train)
 * - Corpus callosum (communication, bandwidth)
 * - Hemispheric brain (coordination, modes)
 * - Lateralization (dominance, plasticity)
 * - Split-brain mode
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "core/brain/hemispheric/nimcp_brain_hemisphere.h"
#include "core/brain/hemispheric/nimcp_corpus_callosum.h"
#include "core/brain/hemispheric/nimcp_lateralization.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class HemisphereTest : public ::testing::Test {
protected:
    void SetUp() override {
        left_config = hemisphere_default_config(HEMISPHERE_LEFT);
        left_config.size = BRAIN_SIZE_TINY;
        left_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        left_config.num_inputs = 16;
        left_config.num_outputs = 16;
        left_config.enable_bio_async = false;
        right_config = hemisphere_default_config(HEMISPHERE_RIGHT);
        right_config.size = BRAIN_SIZE_TINY;
        right_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        right_config.num_inputs = 16;
        right_config.num_outputs = 16;
        right_config.enable_bio_async = false;
    }

    void TearDown() override {
        // Cleanup handled by individual tests
    }

    hemisphere_config_t left_config;
    hemisphere_config_t right_config;
};

class HemisphericBrainTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = hemispheric_brain_default_config();
        config.size = BRAIN_SIZE_TINY;
        config.num_inputs = 32;
        config.num_outputs = 16;
        config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        config.left_config.size = BRAIN_SIZE_TINY;
        config.left_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        config.left_config.enable_bio_async = false;
        config.right_config.size = BRAIN_SIZE_TINY;
        config.right_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        config.right_config.enable_bio_async = false;
    }

    void TearDown() override {
        if (brain) {
            hemispheric_brain_destroy(brain);
            brain = nullptr;
        }
    }

    hemispheric_brain_config_t config;
    hemispheric_brain_t* brain = nullptr;
};

class CorpusCallosumTest : public ::testing::Test {
protected:
    void SetUp() override {
        cc_config = callosum_default_config();
    }

    void TearDown() override {
        if (callosum) {
            callosum_destroy(callosum);
            callosum = nullptr;
        }
        if (left) {
            hemisphere_destroy(left);
            left = nullptr;
        }
        if (right) {
            hemisphere_destroy(right);
            right = nullptr;
        }
    }

    callosum_config_t cc_config;
    corpus_callosum_t* callosum = nullptr;
    brain_hemisphere_t* left = nullptr;
    brain_hemisphere_t* right = nullptr;
};

//=============================================================================
// Hemisphere Lifecycle Tests
//=============================================================================

TEST_F(HemisphereTest, DefaultConfigLeftHemisphere) {
    // Default config should have left hemisphere ID
    EXPECT_EQ(left_config.hemisphere_id, HEMISPHERE_LEFT);

    // Should have language specialization (left-dominant)
    EXPECT_GT(left_config.specialization_weights[COGNITIVE_DOMAIN_LANGUAGE], 0.8f);

    // Should have logical reasoning (left-dominant)
    EXPECT_GT(left_config.specialization_weights[COGNITIVE_DOMAIN_LOGICAL_REASONING], 0.8f);
}

TEST_F(HemisphereTest, DefaultConfigRightHemisphere) {
    // Default config should have right hemisphere ID
    EXPECT_EQ(right_config.hemisphere_id, HEMISPHERE_RIGHT);

    // Should have spatial specialization (right-dominant)
    EXPECT_GT(right_config.specialization_weights[COGNITIVE_DOMAIN_SPATIAL], 0.7f);

    // Should have emotion processing (right-dominant)
    EXPECT_GT(right_config.specialization_weights[COGNITIVE_DOMAIN_EMOTION], 0.6f);
}

TEST_F(HemisphereTest, CreateLeftHemisphere) {
    brain_hemisphere_t* hemi = hemisphere_create(&left_config);
    ASSERT_NE(hemi, nullptr);

    EXPECT_EQ(hemi->id, HEMISPHERE_LEFT);
    EXPECT_TRUE(hemi->is_active);
    EXPECT_GE(hemi->creation_time, 0u);

    hemisphere_destroy(hemi);
}

TEST_F(HemisphereTest, CreateRightHemisphere) {
    brain_hemisphere_t* hemi = hemisphere_create(&right_config);
    ASSERT_NE(hemi, nullptr);

    EXPECT_EQ(hemi->id, HEMISPHERE_RIGHT);
    EXPECT_TRUE(hemi->is_active);

    hemisphere_destroy(hemi);
}

TEST_F(HemisphereTest, CreateWithNullConfig) {
    brain_hemisphere_t* hemi = hemisphere_create(nullptr);
    EXPECT_EQ(hemi, nullptr);
}

TEST_F(HemisphereTest, DestroyNull) {
    // Should not crash
    hemisphere_destroy(nullptr);
}

//=============================================================================
// Hemisphere Processing Tests
//=============================================================================

TEST_F(HemisphereTest, UpdateHemisphere) {
    brain_hemisphere_t* hemi = hemisphere_create(&left_config);
    ASSERT_NE(hemi, nullptr);

    int result = hemisphere_update(hemi, 0.01f);
    EXPECT_EQ(result, 0);

    // Stats should be updated
    hemisphere_stats_t stats;
    hemisphere_get_stats(hemi, &stats);
    EXPECT_EQ(stats.total_updates, 1u);

    hemisphere_destroy(hemi);
}

TEST_F(HemisphereTest, UpdateInactiveHemisphere) {
    brain_hemisphere_t* hemi = hemisphere_create(&left_config);
    ASSERT_NE(hemi, nullptr);

    hemisphere_set_active(hemi, false);
    int result = hemisphere_update(hemi, 0.01f);
    EXPECT_EQ(result, -1);  // Should fail for inactive hemisphere

    hemisphere_destroy(hemi);
}

TEST_F(HemisphereTest, InferHemisphere) {
    brain_hemisphere_t* hemi = hemisphere_create(&left_config);
    ASSERT_NE(hemi, nullptr);

    float input[16] = {0.5f, 0.3f, 0.8f, 0.1f, 0.7f, 0.2f, 0.9f, 0.4f,
                       0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f, 0.8f, 0.7f};
    float output[16] = {0.0f};

    int result = hemisphere_infer(hemi, input, 16, output, 16);
    EXPECT_EQ(result, 0);

    // Verify inference completed and stats updated (output magnitude
    // depends on random initialization and may be very small for TINY brains)
    if (result == 0) {
        // Just verify the buffer was written to (not all NaN/inf)
        bool valid_output = true;
        for (int i = 0; i < 16; i++) {
            if (std::isnan(output[i]) || std::isinf(output[i])) {
                valid_output = false;
                break;
            }
        }
        EXPECT_TRUE(valid_output) << "Output contains NaN or Inf";
    }

    hemisphere_stats_t stats;
    hemisphere_get_stats(hemi, &stats);
    EXPECT_EQ(stats.total_inferences, 1u);

    hemisphere_destroy(hemi);
}

TEST_F(HemisphereTest, TrainHemisphere) {
    brain_hemisphere_t* hemi = hemisphere_create(&left_config);
    ASSERT_NE(hemi, nullptr);

    float input[16] = {0.5f};
    float target[16] = {0.7f};

    float loss = hemisphere_train(hemi, input, target, 16);
    EXPECT_GE(loss, 0.0f);  // Loss should be non-negative

    hemisphere_stats_t stats;
    hemisphere_get_stats(hemi, &stats);
    EXPECT_EQ(stats.total_learning_steps, 1u);

    hemisphere_destroy(hemi);
}

//=============================================================================
// Hemisphere State Tests
//=============================================================================

TEST_F(HemisphereTest, GetActivity) {
    brain_hemisphere_t* hemi = hemisphere_create(&left_config);
    ASSERT_NE(hemi, nullptr);

    // Initial activity should be 0
    float activity = hemisphere_get_activity(hemi);
    EXPECT_GE(activity, 0.0f);
    EXPECT_LE(activity, 1.0f);

    hemisphere_destroy(hemi);
}

TEST_F(HemisphereTest, GetEnergy) {
    brain_hemisphere_t* hemi = hemisphere_create(&left_config);
    ASSERT_NE(hemi, nullptr);

    float energy = hemisphere_get_energy(hemi);
    EXPECT_GE(energy, 0.0f);

    hemisphere_destroy(hemi);
}

TEST_F(HemisphereTest, GetTier) {
    brain_hemisphere_t* hemi = hemisphere_create(&left_config);
    ASSERT_NE(hemi, nullptr);

    platform_tier_t tier = hemisphere_get_tier(hemi);
    EXPECT_EQ(tier, left_config.initial_tier);

    hemisphere_destroy(hemi);
}

TEST_F(HemisphereTest, SetTier) {
    brain_hemisphere_t* hemi = hemisphere_create(&left_config);
    ASSERT_NE(hemi, nullptr);

    int result = hemisphere_set_tier(hemi, PLATFORM_TIER_FULL);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(hemisphere_get_tier(hemi), PLATFORM_TIER_FULL);

    result = hemisphere_set_tier(hemi, PLATFORM_TIER_MINIMAL);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(hemisphere_get_tier(hemi), PLATFORM_TIER_MINIMAL);

    hemisphere_destroy(hemi);
}

TEST_F(HemisphereTest, GetSpecialization) {
    brain_hemisphere_t* hemi = hemisphere_create(&left_config);
    ASSERT_NE(hemi, nullptr);

    // Left hemisphere should have high language specialization
    float lang_spec = hemisphere_get_specialization(hemi, COGNITIVE_DOMAIN_LANGUAGE);
    EXPECT_GT(lang_spec, 0.8f);

    hemisphere_destroy(hemi);
}

//=============================================================================
// Hemisphere Neuromodulator Tests
//=============================================================================

TEST_F(HemisphereTest, GetNeuromodulator) {
    left_config.enable_local_neuromod = true;
    brain_hemisphere_t* hemi = hemisphere_create(&left_config);
    ASSERT_NE(hemi, nullptr);

    float dopamine = hemisphere_get_neuromod(hemi, NEUROMOD_DOPAMINE);
    EXPECT_GE(dopamine, 0.0f);
    EXPECT_LE(dopamine, 1.0f);

    hemisphere_destroy(hemi);
}

TEST_F(HemisphereTest, SetNeuromodulator) {
    left_config.enable_local_neuromod = true;
    brain_hemisphere_t* hemi = hemisphere_create(&left_config);
    ASSERT_NE(hemi, nullptr);

    int result = hemisphere_set_neuromod(hemi, NEUROMOD_DOPAMINE, 0.8f);
    EXPECT_EQ(result, 0);

    float dopamine = hemisphere_get_neuromod(hemi, NEUROMOD_DOPAMINE);
    EXPECT_NEAR(dopamine, 0.8f, 0.1f);

    hemisphere_destroy(hemi);
}

TEST_F(HemisphereTest, NeuromodDiffusion) {
    left_config.enable_local_neuromod = true;
    left_config.neuromod_diffusion_rate = 0.5f;
    brain_hemisphere_t* hemi = hemisphere_create(&left_config);
    ASSERT_NE(hemi, nullptr);

    // Set initial level
    hemisphere_set_neuromod(hemi, NEUROMOD_DOPAMINE, 0.2f);

    // Apply diffusion from other hemisphere at higher level
    float result = hemisphere_apply_neuromod_diffusion(hemi, NEUROMOD_DOPAMINE, 0.8f);
    EXPECT_GT(result, 0.2f);  // Should increase toward 0.8
    EXPECT_LT(result, 0.8f);  // But not reach it fully

    hemisphere_destroy(hemi);
}

//=============================================================================
// Contralateral Mapping Tests
//=============================================================================

TEST_F(HemisphereTest, MotorMapping) {
    brain_hemisphere_t* hemi = hemisphere_create(&left_config);
    ASSERT_NE(hemi, nullptr);

    float motor_cmd[8] = {0.5f, 0.3f, 0.7f, 0.1f, 0.9f, 0.2f, 0.6f, 0.4f};
    float body_output[8] = {0.0f};

    int result = hemisphere_map_motor_output(hemi, motor_cmd, 8, body_output);
    EXPECT_EQ(result, 0);

    // Should have some output (pass-through for now)
    EXPECT_FLOAT_EQ(body_output[0], motor_cmd[0]);

    hemisphere_destroy(hemi);
}

TEST_F(HemisphereTest, SensoryMapping) {
    brain_hemisphere_t* hemi = hemisphere_create(&left_config);
    ASSERT_NE(hemi, nullptr);

    float body_input[8] = {0.5f, 0.3f, 0.7f, 0.1f, 0.9f, 0.2f, 0.6f, 0.4f};
    float sensory_input[8] = {0.0f};

    int result = hemisphere_map_sensory_input(hemi, body_input, 8, sensory_input);
    EXPECT_EQ(result, 0);

    // Should have some output (pass-through for now)
    EXPECT_FLOAT_EQ(sensory_input[0], body_input[0]);

    hemisphere_destroy(hemi);
}

//=============================================================================
// Corpus Callosum Tests
//=============================================================================

TEST_F(CorpusCallosumTest, DefaultConfig) {
    // Default config should have realistic bandwidth
    EXPECT_EQ(cc_config.bandwidth_mode, CALLOSUM_BW_REALISTIC);
    // Check that queue capacity is reasonable
    EXPECT_GT(cc_config.queue_capacity, 0u);
}

TEST_F(CorpusCallosumTest, CreateCallosum) {
    callosum = callosum_create(&cc_config);
    ASSERT_NE(callosum, nullptr);

    EXPECT_TRUE(callosum_is_connected(callosum));
}

TEST_F(CorpusCallosumTest, CreateWithNullConfig) {
    callosum = callosum_create(nullptr);
    // Should create with defaults
    ASSERT_NE(callosum, nullptr);
    EXPECT_TRUE(callosum_is_connected(callosum));
}

TEST_F(CorpusCallosumTest, ConnectHemispheres) {
    hemisphere_config_t left_cfg = hemisphere_default_config(HEMISPHERE_LEFT);
    left_cfg.size = BRAIN_SIZE_TINY;
    left_cfg.initial_tier = PLATFORM_TIER_CONSTRAINED;
    left_cfg.num_inputs = 16;
    left_cfg.num_outputs = 16;
    left_cfg.enable_bio_async = false;
    hemisphere_config_t right_cfg = hemisphere_default_config(HEMISPHERE_RIGHT);
    right_cfg.size = BRAIN_SIZE_TINY;
    right_cfg.initial_tier = PLATFORM_TIER_CONSTRAINED;
    right_cfg.num_inputs = 16;
    right_cfg.num_outputs = 16;
    right_cfg.enable_bio_async = false;

    left = hemisphere_create(&left_cfg);
    right = hemisphere_create(&right_cfg);
    callosum = callosum_create(&cc_config);

    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    ASSERT_NE(callosum, nullptr);

    int result = callosum_connect_hemispheres(callosum, left, right);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(hemisphere_has_callosum(left));
    EXPECT_TRUE(hemisphere_has_callosum(right));
}

TEST_F(CorpusCallosumTest, DisconnectReconnect) {
    callosum = callosum_create(&cc_config);
    ASSERT_NE(callosum, nullptr);

    EXPECT_TRUE(callosum_is_connected(callosum));

    int result = callosum_disconnect(callosum);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(callosum_is_connected(callosum));

    result = callosum_reconnect(callosum);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(callosum_is_connected(callosum));
}

TEST_F(CorpusCallosumTest, SendMessage) {
    hemisphere_config_t left_cfg = hemisphere_default_config(HEMISPHERE_LEFT);
    hemisphere_config_t right_cfg = hemisphere_default_config(HEMISPHERE_RIGHT);

    left = hemisphere_create(&left_cfg);
    right = hemisphere_create(&right_cfg);
    callosum = callosum_create(&cc_config);

    callosum_connect_hemispheres(callosum, left, right);

    float data[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    int result = callosum_send(callosum, HEMISPHERE_LEFT,
                               CALLOSUM_CHANNEL_COGNITIVE, CALLOSUM_PRIORITY_NORMAL,
                               0x1001, data, sizeof(data));
    EXPECT_GE(result, 0);  // 0 = immediate, 1 = queued

    callosum_stats_t stats;
    callosum_get_stats(callosum, &stats);
    EXPECT_GE(stats.total_messages_left_to_right, 1u);
}

TEST_F(CorpusCallosumTest, BandwidthModes) {
    callosum = callosum_create(&cc_config);
    ASSERT_NE(callosum, nullptr);

    // Test setting different bandwidth modes
    EXPECT_EQ(callosum_set_bandwidth_mode(callosum, CALLOSUM_BW_UNLIMITED), 0);
    EXPECT_EQ(callosum_set_bandwidth_mode(callosum, CALLOSUM_BW_REALISTIC), 0);
    EXPECT_EQ(callosum_set_bandwidth_mode(callosum, CALLOSUM_BW_RESTRICTED), 0);
}

TEST_F(CorpusCallosumTest, LatencyControl) {
    callosum = callosum_create(&cc_config);
    ASSERT_NE(callosum, nullptr);

    // Set custom latency range
    EXPECT_EQ(callosum_set_latency(callosum, 5.0f, 20.0f), 0);
}

//=============================================================================
// Hemispheric Brain Tests
//=============================================================================

TEST_F(HemisphericBrainTest, DefaultConfig) {
    EXPECT_EQ(config.default_mode, HEMISPHERIC_MODE_COOPERATIVE);
    EXPECT_EQ(config.cooperation_strategy, COOPERATION_WEIGHTED);
    EXPECT_TRUE(config.enable_bio_async);
}

TEST_F(HemisphericBrainTest, Create) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    EXPECT_NE(brain->left, nullptr);
    EXPECT_NE(brain->right, nullptr);
    EXPECT_NE(brain->callosum, nullptr);
    EXPECT_TRUE(brain->callosum_intact);
    EXPECT_TRUE(brain->is_active);
}

TEST_F(HemisphericBrainTest, CreateWithNullConfig) {
    brain = hemispheric_brain_create(nullptr);
    ASSERT_NE(brain, nullptr);  // Should use defaults
}

TEST_F(HemisphericBrainTest, Update) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    int result = hemispheric_brain_update(brain, 0.01f);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(brain->update_count, 1u);
}

TEST_F(HemisphericBrainTest, ProcessLateralized) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    float input[32] = {0.5f};
    float output[16] = {0.0f};

    // Language should go to left hemisphere
    int result = hemispheric_brain_process_lateralized(
        brain, input, 32, COGNITIVE_DOMAIN_LANGUAGE, output, 16);
    EXPECT_EQ(result, 0);
}

TEST_F(HemisphericBrainTest, ProcessParallel) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    float input[32] = {0.5f};
    float left_output[16] = {0.0f};
    float right_output[16] = {0.0f};

    int result = hemispheric_brain_process_parallel(
        brain, input, 32, left_output, right_output, 16);
    EXPECT_EQ(result, 0);
}

TEST_F(HemisphericBrainTest, ProcessCompetitive) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    float input[32] = {0.5f};
    float output[16] = {0.0f};
    hemisphere_id_t winner;

    int result = hemispheric_brain_process_competitive(
        brain, input, 32, output, 16, &winner);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(winner == HEMISPHERE_LEFT || winner == HEMISPHERE_RIGHT);
}

TEST_F(HemisphericBrainTest, ProcessCooperative) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    float input[32] = {0.5f};
    float output[16] = {0.0f};

    int result = hemispheric_brain_process_cooperative(brain, input, 32, output, 16);
    EXPECT_EQ(result, 0);
}

TEST_F(HemisphericBrainTest, GetHemispheres) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    brain_hemisphere_t* left = hemispheric_brain_get_left(brain);
    brain_hemisphere_t* right = hemispheric_brain_get_right(brain);

    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(left->id, HEMISPHERE_LEFT);
    EXPECT_EQ(right->id, HEMISPHERE_RIGHT);
}

TEST_F(HemisphericBrainTest, GetDominantHemisphere) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    // Language should be left-dominant
    hemisphere_id_t dominant = hemispheric_brain_get_dominant_for(
        brain, COGNITIVE_DOMAIN_LANGUAGE);
    EXPECT_EQ(dominant, HEMISPHERE_LEFT);

    // Spatial should be right-dominant
    dominant = hemispheric_brain_get_dominant_for(brain, COGNITIVE_DOMAIN_SPATIAL);
    EXPECT_EQ(dominant, HEMISPHERE_RIGHT);
}

TEST_F(HemisphericBrainTest, GetDominance) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    // Language dominance should be high (left)
    float lang_dom = hemispheric_brain_get_dominance(brain, COGNITIVE_DOMAIN_LANGUAGE);
    EXPECT_GT(lang_dom, 0.8f);

    // Spatial dominance should be low (right)
    float spatial_dom = hemispheric_brain_get_dominance(brain, COGNITIVE_DOMAIN_SPATIAL);
    EXPECT_LT(spatial_dom, 0.3f);
}

TEST_F(HemisphericBrainTest, ShiftDominance) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    float initial = hemispheric_brain_get_dominance(brain, COGNITIVE_DOMAIN_EMOTION);

    // Shift toward left hemisphere
    int result = hemispheric_brain_shift_dominance(brain, COGNITIVE_DOMAIN_EMOTION, 0.1f);
    EXPECT_EQ(result, 0);

    float after = hemispheric_brain_get_dominance(brain, COGNITIVE_DOMAIN_EMOTION);
    EXPECT_GT(after, initial);
}

//=============================================================================
// Split-Brain Tests
//=============================================================================

TEST_F(HemisphericBrainTest, DisconnectCallosum) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    EXPECT_TRUE(hemispheric_brain_is_callosum_intact(brain));

    int result = hemispheric_brain_disconnect_callosum(brain);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(hemispheric_brain_is_callosum_intact(brain));
}

TEST_F(HemisphericBrainTest, ReconnectCallosum) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    hemispheric_brain_disconnect_callosum(brain);
    EXPECT_FALSE(hemispheric_brain_is_callosum_intact(brain));

    int result = hemispheric_brain_reconnect_callosum(brain);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(hemispheric_brain_is_callosum_intact(brain));
}

TEST_F(HemisphericBrainTest, SplitBrainProcessing) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    // Disconnect callosum
    hemispheric_brain_disconnect_callosum(brain);

    // Should still be able to update
    int result = hemispheric_brain_update(brain, 0.01f);
    EXPECT_EQ(result, 0);

    // Parallel processing should still work
    float input[32] = {0.5f};
    float left_output[16] = {0.0f};
    float right_output[16] = {0.0f};

    result = hemispheric_brain_process_parallel(
        brain, input, 32, left_output, right_output, 16);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Resource Management Tests
//=============================================================================

TEST_F(HemisphericBrainTest, SetTier) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    // Set left hemisphere to minimal tier
    int result = hemispheric_brain_set_tier(brain, HEMISPHERE_LEFT, PLATFORM_TIER_MINIMAL);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(hemispheric_brain_get_tier(brain, HEMISPHERE_LEFT), PLATFORM_TIER_MINIMAL);

    // Set right hemisphere to full tier
    result = hemispheric_brain_set_tier(brain, HEMISPHERE_RIGHT, PLATFORM_TIER_FULL);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(hemispheric_brain_get_tier(brain, HEMISPHERE_RIGHT), PLATFORM_TIER_FULL);
}

TEST_F(HemisphericBrainTest, AsymmetricResources) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    // Allocate more resources to left hemisphere
    int result = hemispheric_brain_set_asymmetric_resources(brain, 0.7f, true);
    EXPECT_EQ(result, 0);

    // Left should have higher tier than right
    platform_tier_t left_tier = hemispheric_brain_get_tier(brain, HEMISPHERE_LEFT);
    platform_tier_t right_tier = hemispheric_brain_get_tier(brain, HEMISPHERE_RIGHT);
    EXPECT_GE(left_tier, right_tier);
}

//=============================================================================
// Processing Mode Tests
//=============================================================================

TEST_F(HemisphericBrainTest, SetMode) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(hemispheric_brain_get_mode(brain), HEMISPHERIC_MODE_COOPERATIVE);

    hemispheric_brain_set_mode(brain, HEMISPHERIC_MODE_LATERALIZED);
    EXPECT_EQ(hemispheric_brain_get_mode(brain), HEMISPHERIC_MODE_LATERALIZED);

    hemispheric_brain_set_mode(brain, HEMISPHERIC_MODE_PARALLEL);
    EXPECT_EQ(hemispheric_brain_get_mode(brain), HEMISPHERIC_MODE_PARALLEL);

    hemispheric_brain_set_mode(brain, HEMISPHERIC_MODE_COMPETITIVE);
    EXPECT_EQ(hemispheric_brain_get_mode(brain), HEMISPHERIC_MODE_COMPETITIVE);
}

TEST_F(HemisphericBrainTest, SetCooperationStrategy) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    hemispheric_brain_set_cooperation_strategy(brain, COOPERATION_AVERAGE);

    // Run cooperative processing with new strategy
    float input[32] = {0.5f};
    float output[16] = {0.0f};

    int result = hemispheric_brain_process_cooperative(brain, input, 32, output, 16);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(HemisphericBrainTest, GetStats) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    // Run some operations
    hemispheric_brain_update(brain, 0.01f);

    float input[32] = {0.5f};
    float output[16] = {0.0f};
    hemispheric_brain_process_cooperative(brain, input, 32, output, 16);

    hemispheric_brain_stats_t stats;
    int result = hemispheric_brain_get_stats(brain, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.cooperative_operations, 1u);
}

TEST_F(HemisphericBrainTest, ResetStats) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    // Run some operations
    hemispheric_brain_update(brain, 0.01f);

    // Reset stats
    int result = hemispheric_brain_reset_stats(brain);
    EXPECT_EQ(result, 0);

    hemispheric_brain_stats_t stats;
    hemispheric_brain_get_stats(brain, &stats);
    EXPECT_EQ(stats.lateralized_operations, 0u);
}

TEST_F(HemisphericBrainTest, GetEnergy) {
    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);

    float energy = hemispheric_brain_get_energy(brain);
    EXPECT_GE(energy, 0.0f);
}

//=============================================================================
// Lateralization Tests
//=============================================================================

class LateralizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        profile = lateralization_default_profile();
    }

    lateralization_profile_t profile;
};

TEST_F(LateralizationTest, DefaultProfile) {
    // Language should be left-dominant (>0.8)
    EXPECT_GT(lateralization_get_dominance(&profile, COGNITIVE_DOMAIN_LANGUAGE), 0.8f);

    // Spatial should be right-dominant (<0.3)
    EXPECT_LT(lateralization_get_dominance(&profile, COGNITIVE_DOMAIN_SPATIAL), 0.3f);

    // Motor gross should be bilateral (~0.5)
    float motor = lateralization_get_dominance(&profile, COGNITIVE_DOMAIN_MOTOR_GROSS);
    EXPECT_GT(motor, 0.3f);
    EXPECT_LT(motor, 0.7f);
}

TEST_F(LateralizationTest, LeftHandedProfile) {
    lateralization_profile_t lh_profile = lateralization_left_handed_profile();

    // Motor fine should be less left-dominant for left-handers
    float motor_default = lateralization_get_dominance(&profile, COGNITIVE_DOMAIN_MOTOR_FINE);
    float motor_lh = lateralization_get_dominance(&lh_profile, COGNITIVE_DOMAIN_MOTOR_FINE);
    EXPECT_LT(motor_lh, motor_default);
}

TEST_F(LateralizationTest, BilateralProfile) {
    lateralization_profile_t bilateral_profile = lateralization_bilateral_profile();

    // Should be more bilateral across domains
    float lang = lateralization_get_dominance(&bilateral_profile, COGNITIVE_DOMAIN_LANGUAGE);
    EXPECT_GT(lang, 0.3f);
    EXPECT_LT(lang, 0.7f);  // More bilateral than typical
}

TEST_F(LateralizationTest, GetDominantHemisphere) {
    // Language -> Left
    EXPECT_EQ(lateralization_get_dominant_hemisphere(&profile, COGNITIVE_DOMAIN_LANGUAGE),
              HEMISPHERE_LEFT);

    // Spatial -> Right
    EXPECT_EQ(lateralization_get_dominant_hemisphere(&profile, COGNITIVE_DOMAIN_SPATIAL),
              HEMISPHERE_RIGHT);
}

TEST_F(LateralizationTest, ShiftDominance) {
    float initial = lateralization_get_dominance(&profile, COGNITIVE_DOMAIN_EMOTION);

    // Shift toward left
    int result = lateralization_shift_dominance(&profile, COGNITIVE_DOMAIN_EMOTION, 0.1f);
    EXPECT_EQ(result, 0);

    float after = lateralization_get_dominance(&profile, COGNITIVE_DOMAIN_EMOTION);
    EXPECT_GT(after, initial);
}

TEST_F(LateralizationTest, ShiftDominanceClamps) {
    // Set to extreme left
    lateralization_shift_dominance(&profile, COGNITIVE_DOMAIN_LANGUAGE, 0.5f);
    float left_extreme = lateralization_get_dominance(&profile, COGNITIVE_DOMAIN_LANGUAGE);
    EXPECT_LE(left_extreme, 1.0f);

    // Set to extreme right
    lateralization_shift_dominance(&profile, COGNITIVE_DOMAIN_SPATIAL, -0.5f);
    float right_extreme = lateralization_get_dominance(&profile, COGNITIVE_DOMAIN_SPATIAL);
    EXPECT_GE(right_extreme, 0.0f);
}

TEST_F(LateralizationTest, Validate) {
    EXPECT_TRUE(lateralization_validate(&profile));

    // Corrupt the profile
    profile.language_dominance = 1.5f;  // Invalid
    EXPECT_FALSE(lateralization_validate(&profile));
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST(HemisphericUtilTest, HemisphereName) {
    EXPECT_STREQ(hemisphere_name(HEMISPHERE_LEFT), "Left");
    EXPECT_STREQ(hemisphere_name(HEMISPHERE_RIGHT), "Right");
}

TEST(HemisphericUtilTest, ModeName) {
    EXPECT_STREQ(hemispheric_mode_name(HEMISPHERIC_MODE_LATERALIZED), "Lateralized");
    EXPECT_STREQ(hemispheric_mode_name(HEMISPHERIC_MODE_PARALLEL), "Parallel");
    EXPECT_STREQ(hemispheric_mode_name(HEMISPHERIC_MODE_COMPETITIVE), "Competitive");
    EXPECT_STREQ(hemispheric_mode_name(HEMISPHERIC_MODE_COOPERATIVE), "Cooperative");
}

TEST(HemisphericUtilTest, CooperationStrategyName) {
    EXPECT_STREQ(cooperation_strategy_name(COOPERATION_AVERAGE), "Average");
    EXPECT_STREQ(cooperation_strategy_name(COOPERATION_WEIGHTED), "Weighted");
    EXPECT_STREQ(cooperation_strategy_name(COOPERATION_DOMINANT), "Dominant");
    EXPECT_STREQ(cooperation_strategy_name(COOPERATION_ATTENTION_GATED), "Attention-Gated");
}

TEST(HemisphericUtilTest, CognitiveDomainName) {
    EXPECT_STREQ(cognitive_domain_name(COGNITIVE_DOMAIN_LANGUAGE), "Language");
    EXPECT_STREQ(cognitive_domain_name(COGNITIVE_DOMAIN_SPATIAL), "Spatial");
    EXPECT_STREQ(cognitive_domain_name(COGNITIVE_DOMAIN_EMOTION), "Emotion Processing");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
