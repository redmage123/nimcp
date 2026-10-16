/**
 * @file test_sensorimotor.cpp
 * @brief Unit tests for NIMCP sensorimotor loop controller.
 *
 * WHAT: Test sensorimotor config defaults, NULL safety, enum values,
 *       and stub lifecycle. Real create requires brain+sim+motor,
 *       so tests requiring a live instance use GTEST_SKIP.
 * WHY:  Sensorimotor loop is the closed-loop control path for embodied
 *       learning; regressions here silently break RL training.
 * HOW:  Google Test, stub mode (no real brain/sim/motor).
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "edge/nimcp_sensorimotor.h"
}

// ============================================================================
// Config Defaults
// ============================================================================

TEST(Sensorimotor, ConfigDefaultValues) {
    nimcp_sm_config_t cfg = nimcp_sensorimotor_config_default();
    EXPECT_FLOAT_EQ(cfg.loop_hz, 30.0f);
    EXPECT_FLOAT_EQ(cfg.learning_rate, 0.001f);
    EXPECT_FLOAT_EQ(cfg.reward_discount, 0.99f);
    EXPECT_FLOAT_EQ(cfg.exploration_noise, 0.1f);
    EXPECT_EQ(cfg.max_episode_steps, 1000u);
    EXPECT_EQ(cfg.num_episodes, 100u);
    EXPECT_FLOAT_EQ(cfg.curiosity_weight, 0.1f);
    EXPECT_TRUE(cfg.enable_domain_randomization);
    EXPECT_EQ(cfg.mode, NIMCP_SM_MODE_REINFORCEMENT);
}

TEST(Sensorimotor, ConfigDefaultsAreReasonable) {
    nimcp_sm_config_t cfg = nimcp_sensorimotor_config_default();
    EXPECT_GT(cfg.loop_hz, 0.0f);
    EXPECT_LE(cfg.loop_hz, 1000.0f);
    EXPECT_GT(cfg.learning_rate, 0.0f);
    EXPECT_LT(cfg.learning_rate, 1.0f);
    EXPECT_GE(cfg.reward_discount, 0.0f);
    EXPECT_LE(cfg.reward_discount, 1.0f);
    EXPECT_GE(cfg.exploration_noise, 0.0f);
    EXPECT_LE(cfg.exploration_noise, 1.0f);
    EXPECT_GT(cfg.max_episode_steps, 0u);
    EXPECT_GT(cfg.num_episodes, 0u);
}

TEST(Sensorimotor, ConfigCustomValuesPreserved) {
    nimcp_sm_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = NIMCP_SM_MODE_LEARNING;
    cfg.loop_hz = 60.0f;
    cfg.learning_rate = 0.01f;
    cfg.reward_discount = 0.95f;
    cfg.exploration_noise = 0.5f;
    cfg.max_episode_steps = 500;
    cfg.num_episodes = 50;
    cfg.curiosity_weight = 0.2f;
    cfg.enable_domain_randomization = false;

    EXPECT_EQ(cfg.mode, NIMCP_SM_MODE_LEARNING);
    EXPECT_FLOAT_EQ(cfg.loop_hz, 60.0f);
    EXPECT_FLOAT_EQ(cfg.learning_rate, 0.01f);
    EXPECT_FLOAT_EQ(cfg.reward_discount, 0.95f);
    EXPECT_FLOAT_EQ(cfg.exploration_noise, 0.5f);
    EXPECT_EQ(cfg.max_episode_steps, 500u);
    EXPECT_EQ(cfg.num_episodes, 50u);
    EXPECT_FLOAT_EQ(cfg.curiosity_weight, 0.2f);
    EXPECT_FALSE(cfg.enable_domain_randomization);
}

// ============================================================================
// Mode Enum Values
// ============================================================================

TEST(Sensorimotor, ModeEnumValues) {
    EXPECT_EQ(NIMCP_SM_MODE_INFERENCE_ONLY, 0);
    EXPECT_EQ(NIMCP_SM_MODE_LEARNING, 1);
    EXPECT_EQ(NIMCP_SM_MODE_REINFORCEMENT, 2);
}

// ============================================================================
// NULL Safety — Create requires brain+sim+motor (all non-NULL)
// ============================================================================

TEST(Sensorimotor, CreateAllNullReturnsNull) {
    nimcp_sm_config_t cfg = nimcp_sensorimotor_config_default();
    nimcp_sensorimotor_t* sm = nimcp_sensorimotor_create(NULL, NULL, NULL, NULL, NULL, &cfg);
    EXPECT_EQ(sm, nullptr) << "All-NULL create should return NULL";
}

TEST(Sensorimotor, CreateNullBrainReturnsNull) {
    int dummy_sim = 1, dummy_motor = 2;
    nimcp_sm_config_t cfg = nimcp_sensorimotor_config_default();
    nimcp_sensorimotor_t* sm = nimcp_sensorimotor_create(
        NULL, &dummy_sim, &dummy_motor, NULL, NULL, &cfg);
    EXPECT_EQ(sm, nullptr) << "NULL brain should return NULL";
}

TEST(Sensorimotor, CreateNullSimReturnsNull) {
    int dummy_brain = 1, dummy_motor = 2;
    nimcp_sm_config_t cfg = nimcp_sensorimotor_config_default();
    nimcp_sensorimotor_t* sm = nimcp_sensorimotor_create(
        &dummy_brain, NULL, &dummy_motor, NULL, NULL, &cfg);
    EXPECT_EQ(sm, nullptr) << "NULL sim should return NULL";
}

TEST(Sensorimotor, CreateNullMotorReturnsNull) {
    int dummy_brain = 1, dummy_sim = 2;
    nimcp_sm_config_t cfg = nimcp_sensorimotor_config_default();
    nimcp_sensorimotor_t* sm = nimcp_sensorimotor_create(
        &dummy_brain, &dummy_sim, NULL, NULL, NULL, &cfg);
    EXPECT_EQ(sm, nullptr) << "NULL motor should return NULL";
}

TEST(Sensorimotor, DestroyNull) {
    nimcp_sensorimotor_destroy(NULL);
    SUCCEED() << "nimcp_sensorimotor_destroy(NULL) did not crash";
}

// ============================================================================
// NULL Safety — Operations on NULL handle
// ============================================================================

TEST(Sensorimotor, ResetNullReturnsError) {
    int rc = nimcp_sensorimotor_reset(NULL);
    EXPECT_EQ(rc, -1);
}

TEST(Sensorimotor, StepNullReturnsError) {
    float reward;
    bool done;
    int rc = nimcp_sensorimotor_step(NULL, &reward, &done);
    EXPECT_EQ(rc, -1);
}

TEST(Sensorimotor, GetObservationNullReturnsError) {
    float features[32];
    int rc = nimcp_sensorimotor_get_observation(NULL, features, 32);
    EXPECT_EQ(rc, -1);
}

TEST(Sensorimotor, GetStatsNullReturnsError) {
    nimcp_sm_stats_t stats;
    int rc = nimcp_sensorimotor_get_stats(NULL, &stats);
    EXPECT_EQ(rc, -1);
}

TEST(Sensorimotor, SetExplorationNullReturnsError) {
    int rc = nimcp_sensorimotor_set_exploration(NULL, 0.5f);
    EXPECT_EQ(rc, -1);
}

TEST(Sensorimotor, RunEpisodeNullReturnsError) {
    int rc = nimcp_sensorimotor_run_episode(NULL, NULL);
    EXPECT_EQ(rc, -1);
}

TEST(Sensorimotor, TrainNullReturnsError) {
    int rc = nimcp_sensorimotor_train(NULL, 10, NULL);
    EXPECT_EQ(rc, -1);
}

// ============================================================================
// Stats struct zero-initialization
// ============================================================================

TEST(Sensorimotor, StatsStructZeroInit) {
    nimcp_sm_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    EXPECT_EQ(stats.total_episodes, 0u);
    EXPECT_EQ(stats.total_steps, 0u);
    EXPECT_FLOAT_EQ(stats.mean_episode_reward, 0.0f);
    EXPECT_FLOAT_EQ(stats.best_episode_reward, 0.0f);
    EXPECT_FLOAT_EQ(stats.mean_episode_length, 0.0f);
    EXPECT_FLOAT_EQ(stats.current_exploration, 0.0f);
    EXPECT_EQ(stats.collisions, 0u);
}

TEST(Sensorimotor, EpisodeStatsStructZeroInit) {
    nimcp_episode_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    EXPECT_EQ(stats.episode_id, 0u);
    EXPECT_EQ(stats.num_steps, 0u);
    EXPECT_FLOAT_EQ(stats.total_reward, 0.0f);
    EXPECT_FLOAT_EQ(stats.avg_reward, 0.0f);
    EXPECT_FLOAT_EQ(stats.max_reward, 0.0f);
    EXPECT_FALSE(stats.terminated);
    EXPECT_FLOAT_EQ(stats.episode_time_sec, 0.0f);
}
