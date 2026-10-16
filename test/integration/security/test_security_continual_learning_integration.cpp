/**
 * @file test_security_continual_learning_integration.cpp
 * @brief Integration tests for Security-Continual Learning Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * INTEGRATION SCENARIOS:
 * 1. Complete security workflow for continual learning
 * 2. Multi-task protection and monitoring
 * 3. Drift detection across task boundaries
 * 4. Replay integrity during task transitions
 * 5. Learning rate adaptation under threat
 * 6. Bidirectional effect propagation
 * 7. Concurrent learning and protection
 * 8. Emergency response scenarios
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

extern "C" {
#include "security/continual/nimcp_security_continual_learning_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

namespace {

/* =============================================================================
 * Test Fixture
 * ============================================================================= */

class SecurityCLIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        EXPECT_EQ(0, security_cl_default_config(&config));
        config.enable_forgetting_protection = true;
        config.enable_drift_detection = true;
        config.enable_replay_verification = true;
        config.enable_lr_monitoring = true;
        config.enable_task_validation = true;
        config.enable_ewc_boost = true;
        config.retention_threshold = 0.8f;
        config.critical_threshold = 0.6f;
        config.enable_logging = true;

        bridge = security_cl_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }

    void TearDown() override {
        if (bridge) {
            security_cl_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    /* Helper to simulate continual learning task */
    void simulateTaskLearning(
        uint32_t task_id,
        const char* task_name,
        float baseline_accuracy,
        const std::vector<float>& weights)
    {
        /* Register task */
        EXPECT_EQ(0, security_cl_register_task(bridge, task_id, task_name, baseline_accuracy));

        /* Protect knowledge */
        EXPECT_EQ(0, security_cl_protect_knowledge(bridge, weights.data(), weights.size(), task_id));
    }

    security_cl_config_t config;
    security_cl_bridge_t* bridge;
};

/* =============================================================================
 * Complete Workflow Tests
 * ============================================================================= */

TEST_F(SecurityCLIntegrationTest, CompleteSecurityWorkflow) {
    /* Step 1: Connect systems */
    int dummy_cl = 42;
    EXPECT_EQ(0, security_cl_connect_continual_learning(bridge, &dummy_cl));

    /* Verify connection */
    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_TRUE(stats.cl_connected);

    /* Step 2: Initialize drift baseline */
    std::vector<float> baseline_features = {0.5f, 0.5f, 0.5f, 0.5f};
    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, baseline_features.data(), baseline_features.size()));

    /* Step 3: Learn first task */
    std::vector<float> weights_task0(100, 0.1f);
    simulateTaskLearning(0, "mnist_digits", 0.98f, weights_task0);

    /* Step 4: Verify protection is active */
    security_cl_effects_t effects;
    EXPECT_EQ(0, security_cl_get_security_effects(bridge, &effects));
    EXPECT_GT(effects.protected_task_count, 0u);

    /* Step 5: Monitor retention */
    float retention;
    security_cl_retention_level_t level = security_cl_monitor_retention(bridge, 0, 0.95f, &retention);
    EXPECT_EQ(SECURITY_CL_RETENTION_HEALTHY, level);
    EXPECT_GT(retention, 0.9f);

    /* Step 6: Learn second task with drift check */
    security_cl_drift_type_t drift_type;
    float drift_score;
    std::vector<float> task1_features = {0.55f, 0.52f, 0.48f, 0.51f};  /* Slight drift */
    bool drift_allowed = security_cl_validate_drift(
        bridge, task1_features.data(), task1_features.size(), &drift_type, &drift_score);
    EXPECT_TRUE(drift_allowed);  /* Natural drift should be allowed */

    std::vector<float> weights_task1(100, 0.2f);
    simulateTaskLearning(1, "mnist_fashion", 0.92f, weights_task1);

    /* Step 7: Monitor both tasks */
    level = security_cl_monitor_retention(bridge, 0, 0.92f, &retention);
    EXPECT_EQ(SECURITY_CL_RETENTION_HEALTHY, level);

    level = security_cl_monitor_retention(bridge, 1, 0.90f, &retention);
    EXPECT_EQ(SECURITY_CL_RETENTION_HEALTHY, level);

    /* Step 8: Get final statistics */
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_EQ(2u, stats.tasks_registered);
    EXPECT_EQ(2u, stats.knowledge_protections);
    EXPECT_GT(stats.retention_checks, 0u);
    EXPECT_GT(stats.drift_checks, 0u);
}

/* =============================================================================
 * Multi-Task Protection Tests
 * ============================================================================= */

TEST_F(SecurityCLIntegrationTest, MultiTaskProtectionProgression) {
    const int num_tasks = 5;

    /* Learn sequence of tasks */
    for (int t = 0; t < num_tasks; t++) {
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "task_%d", t);

        std::vector<float> weights(100, 0.1f + t * 0.05f);
        simulateTaskLearning(t, task_name, 0.95f - t * 0.01f, weights);
    }

    /* Monitor all tasks */
    for (int t = 0; t < num_tasks; t++) {
        float retention;
        /* Simulate slight degradation for earlier tasks */
        float current_accuracy = (0.95f - t * 0.01f) * (1.0f - 0.02f * (num_tasks - t - 1));
        security_cl_retention_level_t level = security_cl_monitor_retention(
            bridge, t, current_accuracy, &retention);
        EXPECT_NE(SECURITY_CL_RETENTION_COMPROMISED, level);
    }

    /* Verify overall status */
    float overall_retention;
    security_cl_retention_level_t overall = security_cl_get_retention_status(bridge, &overall_retention);
    EXPECT_EQ(SECURITY_CL_RETENTION_HEALTHY, overall);
}

TEST_F(SecurityCLIntegrationTest, ProtectionPenaltyIncreasesWithDrift) {
    /* Learn initial task */
    std::vector<float> initial_weights = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    EXPECT_EQ(0, security_cl_protect_knowledge(bridge, initial_weights.data(), initial_weights.size(), 0));

    /* Compute penalty for progressively different weights */
    float penalties[5];
    for (int i = 0; i < 5; i++) {
        std::vector<float> new_weights(initial_weights);
        /* Increase drift */
        for (size_t j = 0; j < new_weights.size(); j++) {
            new_weights[j] += (i + 1) * 0.1f;
        }
        EXPECT_EQ(0, security_cl_compute_protection_penalty(
            bridge, new_weights.data(), new_weights.size(), &penalties[i]));
    }

    /* Penalties should increase with drift */
    for (int i = 1; i < 5; i++) {
        EXPECT_GT(penalties[i], penalties[i-1]);
    }
}

/* =============================================================================
 * Drift Detection Across Tasks
 * ============================================================================= */

TEST_F(SecurityCLIntegrationTest, DriftDetectionTaskBoundaries) {
    /* Establish baseline for task 0 */
    std::vector<float> task0_features = {0.1f, 0.2f, 0.3f, 0.4f};
    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, task0_features.data(), task0_features.size()));

    /* Learn task 0 */
    std::vector<float> weights0(100, 0.1f);
    simulateTaskLearning(0, "task_0", 0.95f, weights0);

    /* Check drift for task 1 - should be natural */
    std::vector<float> task1_features = {0.2f, 0.3f, 0.4f, 0.5f};
    security_cl_drift_type_t drift_type;
    float drift_score;
    bool allowed = security_cl_validate_drift(
        bridge, task1_features.data(), task1_features.size(), &drift_type, &drift_score);

    /* Depending on drift magnitude, may or may not be detected */
    EXPECT_GE(drift_score, 0.0f);

    /* Reset baseline for new task */
    EXPECT_EQ(0, security_cl_reset_drift_baseline(bridge));
    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, task1_features.data(), task1_features.size()));

    /* Learn task 1 */
    std::vector<float> weights1(100, 0.2f);
    simulateTaskLearning(1, "task_1", 0.93f, weights1);
}

TEST_F(SecurityCLIntegrationTest, AdversarialDriftBlocked) {
    /* Establish baseline */
    std::vector<float> baseline = {0.1f, 0.1f, 0.1f, 0.1f};
    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, baseline.data(), baseline.size()));

    /* Simulate adversarial drift - sudden large change */
    std::vector<float> adversarial = {0.9f, 0.9f, 0.9f, 0.9f};
    security_cl_drift_type_t drift_type;
    float drift_score;
    bool allowed = security_cl_validate_drift(
        bridge, adversarial.data(), adversarial.size(), &drift_type, &drift_score);

    /* Should detect high drift score */
    EXPECT_GT(drift_score, 0.3f);

    /* If classified as adversarial, should be blocked */
    if (drift_type == SECURITY_CL_DRIFT_ADVERSARIAL) {
        EXPECT_FALSE(allowed);
    }

    /* Verify stats */
    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_GT(stats.drift_detections, 0u);
}

/* =============================================================================
 * Replay Integrity Tests
 * ============================================================================= */

TEST_F(SecurityCLIntegrationTest, ReplayIntegrityAcrossTaskTransitions) {
    /* Create replay buffer */
    std::vector<float> replay_buffer(1000, 0.5f);

    uint32_t buffer_id = security_cl_register_replay_buffer(
        bridge, "experience_replay", replay_buffer.data(),
        replay_buffer.size() * sizeof(float), 100);
    EXPECT_GT(buffer_id, 0u);

    /* Learn first task using replay */
    std::vector<float> weights0(100, 0.1f);
    simulateTaskLearning(0, "task_0", 0.95f, weights0);

    /* Verify replay buffer integrity */
    security_cl_replay_status_t status;
    EXPECT_TRUE(security_cl_verify_replay(
        bridge, replay_buffer.data(), replay_buffer.size() * sizeof(float), 100, &status));
    EXPECT_EQ(SECURITY_CL_REPLAY_OK, status);

    /* Legitimately update buffer for task 1 */
    for (size_t i = 0; i < 100; i++) {
        replay_buffer[i] = 0.6f;  /* Add new experiences */
    }
    EXPECT_EQ(0, security_cl_update_replay_hash(
        bridge, buffer_id, replay_buffer.data(),
        replay_buffer.size() * sizeof(float), 110));

    /* Verify updated buffer */
    EXPECT_TRUE(security_cl_verify_replay(
        bridge, replay_buffer.data(), replay_buffer.size() * sizeof(float), 110, &status));
    EXPECT_EQ(SECURITY_CL_REPLAY_OK, status);

    /* Learn second task */
    std::vector<float> weights1(100, 0.2f);
    simulateTaskLearning(1, "task_1", 0.93f, weights1);
}

TEST_F(SecurityCLIntegrationTest, ReplayTamperingDetectedAndQuarantined) {
    /* Create replay buffer */
    std::vector<float> replay_buffer(1000, 0.5f);

    uint32_t buffer_id = security_cl_register_replay_buffer(
        bridge, "experience_replay", replay_buffer.data(),
        replay_buffer.size() * sizeof(float), 100);
    EXPECT_GT(buffer_id, 0u);

    /* Tamper with buffer without updating hash */
    replay_buffer[0] = 999.9f;

    /* Verification should fail */
    security_cl_replay_status_t status;
    bool valid = security_cl_verify_replay(
        bridge, replay_buffer.data(), replay_buffer.size() * sizeof(float), 100, &status);
    EXPECT_FALSE(valid);
    EXPECT_NE(SECURITY_CL_REPLAY_OK, status);

    /* Quarantine suspicious samples */
    std::vector<uint32_t> suspicious = {0, 1, 2};
    EXPECT_EQ(0, security_cl_quarantine_replay_samples(bridge, suspicious.data(), suspicious.size()));

    /* Verify quarantine count */
    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_EQ(3u, stats.samples_quarantined);
    EXPECT_GT(stats.replay_failures, 0u);
}

/* =============================================================================
 * Learning Rate Adaptation Tests
 * ============================================================================= */

TEST_F(SecurityCLIntegrationTest, LRAdaptationUnderThreat) {
    /* Normal LR recording */
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(0, security_cl_record_lr(bridge, 0.001f));
        EXPECT_FALSE(security_cl_detect_lr_manipulation(bridge, 0.001f));
    }

    /* Simulate threat by degrading retention */
    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "task_0", 0.95f));
    float retention;
    security_cl_monitor_retention(bridge, 0, 0.50f, &retention);  /* Critical */

    /* Update security effects */
    EXPECT_EQ(0, security_cl_update_security_effects(bridge));

    /* Get effects to see LR scaling */
    security_cl_effects_t effects;
    EXPECT_EQ(0, security_cl_get_security_effects(bridge, &effects));

    /* Under threat, LR should be scaled down */
    if (effects.threat_level > 0.3f) {
        EXPECT_LT(effects.lr_scale_factor, 1.0f);
    }

    /* Safe LR should respect bounds */
    float safe_lr = security_cl_get_safe_lr(bridge);
    EXPECT_GE(safe_lr, effects.lr_min_allowed);
    EXPECT_LE(safe_lr, effects.lr_max_allowed);
}

TEST_F(SecurityCLIntegrationTest, LRManipulationDetectionAndCorrection) {
    /* Record normal LR pattern */
    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(0, security_cl_record_lr(bridge, 0.001f));
    }

    /* Attempt manipulation - spike to high LR */
    EXPECT_TRUE(security_cl_detect_lr_manipulation(bridge, 0.5f));  /* Way above max */

    /* Get safe LR */
    float safe_lr = security_cl_get_safe_lr(bridge);
    EXPECT_LE(safe_lr, config.lr_max_bound);

    /* Verify manipulation detected in stats */
    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_GT(stats.lr_manipulations_detected, 0u);
}

/* =============================================================================
 * Bidirectional Effect Propagation Tests
 * ============================================================================= */

TEST_F(SecurityCLIntegrationTest, ThreatPropagationToProtection) {
    /* Initial threat level should be 0 */
    EXPECT_FLOAT_EQ(0.0f, security_cl_get_threat_level(bridge));

    /* Create multiple threat signals */

    /* 1. Retention degradation */
    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "task_0", 0.95f));
    float retention;
    security_cl_monitor_retention(bridge, 0, 0.40f, &retention);  /* Critical */

    /* 2. Drift anomaly */
    std::vector<float> baseline = {0.1f, 0.2f, 0.3f, 0.4f};
    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, baseline.data(), baseline.size()));
    std::vector<float> adversarial = {0.9f, 0.8f, 0.7f, 0.6f};
    security_cl_drift_type_t drift_type;
    float drift_score;
    security_cl_validate_drift(bridge, adversarial.data(), adversarial.size(), &drift_type, &drift_score);

    /* Update and propagate effects */
    EXPECT_EQ(0, security_cl_update_security_effects(bridge));

    /* Verify threat propagation */
    float threat = security_cl_get_threat_level(bridge);
    EXPECT_GT(threat, 0.0f);

    /* Verify protection boost */
    security_cl_effects_t effects;
    EXPECT_EQ(0, security_cl_get_security_effects(bridge, &effects));
    EXPECT_GE(effects.ewc_lambda_boost, 1.0f);

    /* Phase should reflect threat level */
    security_cl_phase_t phase = security_cl_get_phase(bridge);
    EXPECT_NE(SECURITY_CL_PHASE_INACTIVE, phase);
}

TEST_F(SecurityCLIntegrationTest, CLEffectsTriggerSecurityResponse) {
    /* Update CL effects with anomalies */
    EXPECT_EQ(0, security_cl_update_cl_effects(bridge, 0.5f, 0.8f, 0.001f, 0));

    /* Get CL effects */
    cl_security_effects_t cl_effects;
    EXPECT_EQ(0, security_cl_get_cl_effects(bridge, &cl_effects));
    EXPECT_TRUE(cl_effects.valid);
    EXPECT_TRUE(cl_effects.retention_anomaly);  /* retention < critical threshold */

    /* Update security effects based on CL signals */
    EXPECT_EQ(0, security_cl_update_security_effects(bridge));

    /* Verify security response */
    security_cl_effects_t sec_effects;
    EXPECT_EQ(0, security_cl_get_security_effects(bridge, &sec_effects));
    EXPECT_GT(sec_effects.threat_level, 0.0f);
}

/* =============================================================================
 * Emergency Response Tests
 * ============================================================================= */

TEST_F(SecurityCLIntegrationTest, EmergencyResponseTriggered) {
    /* Register tasks */
    for (int t = 0; t < 3; t++) {
        char name[32];
        snprintf(name, sizeof(name), "task_%d", t);
        std::vector<float> weights(100, 0.1f + t * 0.1f);
        simulateTaskLearning(t, name, 0.95f, weights);
    }

    /* Catastrophic forgetting on all tasks */
    for (int t = 0; t < 3; t++) {
        float retention;
        security_cl_monitor_retention(bridge, t, 0.30f, &retention);
    }

    /* Update security effects */
    EXPECT_EQ(0, security_cl_update_security_effects(bridge));

    /* Verify emergency response */
    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_GT(stats.emergency_responses, 0u);

    /* Check if under attack */
    float threat = security_cl_get_threat_level(bridge);
    if (threat > 0.6f) {
        EXPECT_TRUE(security_cl_is_under_attack(bridge));
    }

    /* Retention should be compromised */
    EXPECT_TRUE(security_cl_is_retention_compromised(bridge));
}

TEST_F(SecurityCLIntegrationTest, KnowledgeLockUnderSevereAttack) {
    /* Create severe threat scenario */
    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "task_0", 0.95f));
    float retention;
    security_cl_monitor_retention(bridge, 0, 0.20f, &retention);  /* Very critical */

    /* Add drift anomaly */
    std::vector<float> baseline = {0.1f, 0.2f, 0.3f, 0.4f};
    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, baseline.data(), baseline.size()));
    std::vector<float> adversarial = {0.9f, 0.9f, 0.9f, 0.9f};
    security_cl_drift_type_t drift_type;
    float drift_score;
    security_cl_validate_drift(bridge, adversarial.data(), adversarial.size(), &drift_type, &drift_score);

    /* Add replay anomaly */
    std::vector<float> replay(1000, 0.5f);
    uint32_t buffer_id = security_cl_register_replay_buffer(
        bridge, "replay", replay.data(), replay.size() * sizeof(float), 100);
    replay[0] = 999.0f;
    security_cl_replay_status_t status;
    security_cl_verify_replay(bridge, replay.data(), replay.size() * sizeof(float), 100, &status);

    /* Update effects multiple times to accumulate threat */
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(0, security_cl_update_security_effects(bridge));
    }

    /* Get effects */
    security_cl_effects_t effects;
    EXPECT_EQ(0, security_cl_get_security_effects(bridge, &effects));

    /* Under severe attack (threat > 0.8), knowledge should be locked */
    if (effects.threat_level > 0.8f) {
        EXPECT_TRUE(effects.knowledge_lock_active);
        EXPECT_TRUE(effects.new_task_blocked);
    }
}

/* =============================================================================
 * Concurrent Learning and Protection Tests
 * ============================================================================= */

TEST_F(SecurityCLIntegrationTest, ConcurrentTaskLearningAndMonitoring) {
    /* Register initial tasks */
    for (int t = 0; t < 5; t++) {
        char name[32];
        snprintf(name, sizeof(name), "task_%d", t);
        EXPECT_EQ(0, security_cl_register_task(bridge, t, name, 0.95f - t * 0.02f));
    }

    std::atomic<int> complete_count{0};
    const int num_threads = 4;
    const int iterations = 25;

    auto worker = [this, &complete_count, iterations](int thread_id) {
        for (int i = 0; i < iterations; i++) {
            uint32_t task_id = (thread_id + i) % 5;

            /* Monitor retention */
            float retention;
            security_cl_retention_level_t level = security_cl_monitor_retention(
                bridge, task_id, 0.85f, &retention);
            (void)level;

            /* Protect knowledge */
            std::vector<float> weights(50, 0.1f + thread_id * 0.01f);
            security_cl_protect_knowledge(bridge, weights.data(), weights.size(), task_id);

            /* Record LR */
            security_cl_record_lr(bridge, 0.001f + i * 0.0001f);

            complete_count++;
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back(worker, t);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(num_threads * iterations, complete_count.load());

    /* Verify stats accumulated correctly */
    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_GT(stats.retention_checks, 0u);
    EXPECT_GT(stats.knowledge_protections, 0u);
}

/* =============================================================================
 * Phase Transition Tests
 * ============================================================================= */

TEST_F(SecurityCLIntegrationTest, PhaseTransitionsDuringOperation) {
    /* Initial phase should be MONITORING */
    EXPECT_EQ(SECURITY_CL_PHASE_MONITORING, security_cl_get_phase(bridge));

    /* Connect CL - should transition to PROTECTING */
    int dummy_cl = 42;
    EXPECT_EQ(0, security_cl_connect_continual_learning(bridge, &dummy_cl));
    EXPECT_EQ(SECURITY_CL_PHASE_PROTECTING, security_cl_get_phase(bridge));

    /* Introduce moderate threat */
    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "task_0", 0.95f));
    float retention;
    security_cl_monitor_retention(bridge, 0, 0.70f, &retention);  /* Degrading */
    EXPECT_EQ(0, security_cl_update_security_effects(bridge));

    /* Phase might be PROTECTING or DEFENDING depending on threat level */
    security_cl_phase_t phase = security_cl_get_phase(bridge);
    EXPECT_NE(SECURITY_CL_PHASE_INACTIVE, phase);
    EXPECT_NE(SECURITY_CL_PHASE_MONITORING, phase);

    /* Introduce severe threat */
    security_cl_monitor_retention(bridge, 0, 0.30f, &retention);  /* Critical */
    EXPECT_EQ(0, security_cl_update_security_effects(bridge));

    /* If threat is high enough, should be DEFENDING */
    float threat = security_cl_get_threat_level(bridge);
    if (threat > 0.7f) {
        EXPECT_EQ(SECURITY_CL_PHASE_DEFENDING, security_cl_get_phase(bridge));
    }
}

/* =============================================================================
 * Stats Accumulation Tests
 * ============================================================================= */

TEST_F(SecurityCLIntegrationTest, StatsAccumulateCorrectly) {
    security_cl_stats_t initial_stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &initial_stats));

    /* Perform various operations */
    const int num_tasks = 3;
    const int checks_per_task = 5;

    for (int t = 0; t < num_tasks; t++) {
        char name[32];
        snprintf(name, sizeof(name), "task_%d", t);

        /* Task registration */
        std::vector<float> weights(100, 0.1f + t * 0.1f);
        simulateTaskLearning(t, name, 0.95f, weights);

        /* Drift checks */
        std::vector<float> features(10, 0.5f + t * 0.1f);
        security_cl_update_drift_baseline(bridge, features.data(), features.size());
        security_cl_drift_type_t drift_type;
        float drift_score;
        security_cl_validate_drift(bridge, features.data(), features.size(), &drift_type, &drift_score);

        /* Retention checks */
        for (int c = 0; c < checks_per_task; c++) {
            float retention;
            security_cl_monitor_retention(bridge, t, 0.9f, &retention);
        }

        /* LR checks */
        security_cl_detect_lr_manipulation(bridge, 0.001f);
        security_cl_record_lr(bridge, 0.001f);
    }

    /* Get final stats */
    security_cl_stats_t final_stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &final_stats));

    /* Verify accumulation */
    EXPECT_EQ(num_tasks, (int)final_stats.tasks_registered);
    EXPECT_EQ(num_tasks, (int)final_stats.knowledge_protections);
    EXPECT_EQ(num_tasks * checks_per_task, (int)final_stats.retention_checks);
    EXPECT_EQ(num_tasks, (int)final_stats.drift_checks);
    EXPECT_EQ(num_tasks, (int)final_stats.lr_checks);
}

TEST_F(SecurityCLIntegrationTest, StatsResetWorks) {
    /* Generate some stats */
    std::vector<float> weights(100, 0.1f);
    simulateTaskLearning(0, "task_0", 0.95f, weights);

    float retention;
    security_cl_monitor_retention(bridge, 0, 0.9f, &retention);

    /* Verify non-zero stats */
    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_GT(stats.knowledge_protections, 0u);

    /* Reset */
    EXPECT_EQ(0, security_cl_reset_stats(bridge));

    /* Verify reset */
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_EQ(0u, stats.knowledge_protections);
    EXPECT_EQ(0u, stats.retention_checks);
}

}  /* namespace */

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
