/**
 * @file test_world_imagination_health_e2e.cpp
 * @brief End-to-end tests for Phase 5.14 World Model & Imagination Health Integration
 * @date 2026-01-19
 *
 * Full system tests for world model and imagination health monitoring.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <mutex>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
}

// Mock components for testing
static jepa_predictor_t* create_mock_jepa(int id) {
    return reinterpret_cast<jepa_predictor_t*>(static_cast<uintptr_t>(0x1000 + id * 0x100));
}

static omni_world_model_t* create_mock_world_model(int id) {
    return reinterpret_cast<omni_world_model_t*>(static_cast<uintptr_t>(0x2000 + id * 0x100));
}

static imagination_engine_t* create_mock_imagination(int id) {
    return reinterpret_cast<imagination_engine_t*>(static_cast<uintptr_t>(0x3000 + id * 0x100));
}

class WorldImaginationHealthE2ETest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent;

    void SetUp() override {
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);
    }

    void TearDown() override {
        if (agent) {
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

/* ============================================================================
 * Full System Lifecycle E2E Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthE2ETest, FullLifecycleWithAllComponents) {
    printf("=== Full Lifecycle E2E Test ===\n");

    // Phase 1: Component registration
    printf("Phase 1: Connecting all components...\n");
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);
    imagination_engine_t* imag = create_mock_imagination(1);

    EXPECT_EQ(nimcp_health_agent_connect_jepa(agent, jepa, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_world_model(agent, wm, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_imagination(agent, imag, nullptr), 0);

    // Phase 2: Initial health check
    printf("Phase 2: Initial health check...\n");
    EXPECT_FALSE(nimcp_health_agent_world_model_needs_attention(agent));
    EXPECT_FALSE(nimcp_health_agent_imagination_needs_attention(agent));
    float initial_wm_score = nimcp_health_agent_get_world_model_health_score(agent);
    float initial_imag_score = nimcp_health_agent_get_imagination_health_score(agent);
    EXPECT_GE(initial_wm_score, 0.0f);
    EXPECT_GE(initial_imag_score, 0.0f);
    printf("  Initial WM score: %.2f, Imag score: %.2f\n", initial_wm_score, initial_imag_score);

    // Phase 3: Health check cycles
    printf("Phase 3: Running 50 health check cycles...\n");
    for (int cycle = 0; cycle < 50; cycle++) {
        EXPECT_EQ(nimcp_health_agent_check_world_model_now(agent), 0);
        EXPECT_EQ(nimcp_health_agent_check_imagination_now(agent), 0);
    }

    // Phase 4: Verify metrics collected
    printf("Phase 4: Verifying metrics...\n");
    world_imagination_health_t health = {};
    EXPECT_EQ(nimcp_health_agent_get_world_imagination_health(agent, &health), 0);
    EXPECT_GE(health.check_count, 50u);
    printf("  Check count: %u\n", health.check_count);
    printf("  WM state: %d, Imag state: %d\n", health.world_model.health_state, health.imagination.health_state);
    printf("  Alignment: %.2f\n", health.wm_imagination_alignment);

    // Phase 5: JEPA metrics
    printf("Phase 5: Verifying JEPA metrics...\n");
    printf("  Mean prediction error: %.4f\n", health.jepa.mean_prediction_error);
    printf("  Embedding variance: %.4f\n", health.jepa.embedding_variance);
    printf("  Embedding orthogonality: %.4f\n", health.jepa.embedding_orthogonality);

    // Phase 6: World model metrics
    printf("Phase 6: Verifying world model metrics...\n");
    printf("  Forward accuracy: %.4f\n", health.world_model.forward_accuracy);
    printf("  Counterfactual validity: %.4f\n", health.world_model.counterfactual_validity);
    printf("  Health score: %.4f\n", health.world_model.health_score);

    // Phase 7: Imagination metrics
    printf("Phase 7: Verifying imagination metrics...\n");
    printf("  Scene vividness: %.4f\n", health.imagination.scene_vividness);
    printf("  Scene coherence: %.4f\n", health.imagination.scene_coherence);
    printf("  Reality check pass rate: %.4f\n", health.imagination.reality_check_pass_rate);

    // Phase 8: Health score aggregation
    printf("Phase 8: Checking aggregate health...\n");
    float final_wm_score = nimcp_health_agent_get_world_model_health_score(agent);
    float final_imag_score = nimcp_health_agent_get_imagination_health_score(agent);
    EXPECT_GE(final_wm_score, 0.0f);
    EXPECT_LE(final_wm_score, 1.0f);
    EXPECT_GE(final_imag_score, 0.0f);
    EXPECT_LE(final_imag_score, 1.0f);
    printf("  Final WM score: %.2f, Imag score: %.2f\n", final_wm_score, final_imag_score);

    // Phase 9: Recovery actions
    printf("Phase 9: Executing recovery actions...\n");
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_RESET_PREDICTOR, "e2e test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_CLEAR_WORKSPACE, "e2e test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(
        agent, WM_RECOVERY_BOOST_GROUNDING, "e2e test"), 0);

    // Phase 10: Cleanup
    printf("Phase 10: Disconnecting all components...\n");
    EXPECT_EQ(nimcp_health_agent_disconnect_imagination(agent), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_world_model(agent), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_jepa(agent), 0);

    printf("Full lifecycle test completed successfully.\n");
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthE2ETest, StressTestHighFrequencyChecks) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);
    imagination_engine_t* imag = create_mock_imagination(1);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);
    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    printf("Running 1000 high-frequency health check cycles...\n");
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(nimcp_health_agent_check_world_model_now(agent), 0);
        EXPECT_EQ(nimcp_health_agent_check_imagination_now(agent), 0);

        if (i % 100 == 0) {
            float wm_score = nimcp_health_agent_get_world_model_health_score(agent);
            float imag_score = nimcp_health_agent_get_imagination_health_score(agent);
            EXPECT_GE(wm_score, 0.0f);
            EXPECT_LE(wm_score, 1.0f);
            EXPECT_GE(imag_score, 0.0f);
            EXPECT_LE(imag_score, 1.0f);
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    printf("Completed 1000 cycles in %ld ms\n", duration.count());

    // Verify final state
    world_imagination_health_t health = {};
    EXPECT_EQ(nimcp_health_agent_get_world_imagination_health(agent, &health), 0);
    EXPECT_GE(health.check_count, 1000u);
}

TEST_F(WorldImaginationHealthE2ETest, StressTestRapidConnectDisconnect) {
    printf("Running rapid connect/disconnect stress test...\n");
    auto start = std::chrono::steady_clock::now();

    for (int cycle = 0; cycle < 500; cycle++) {
        jepa_predictor_t* jepa = create_mock_jepa(cycle);
        omni_world_model_t* wm = create_mock_world_model(cycle);
        imagination_engine_t* imag = create_mock_imagination(cycle);

        nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
        nimcp_health_agent_connect_world_model(agent, wm, nullptr);
        nimcp_health_agent_connect_imagination(agent, imag, nullptr);

        // Quick health check
        nimcp_health_agent_check_world_model_now(agent);
        nimcp_health_agent_check_imagination_now(agent);

        nimcp_health_agent_disconnect_imagination(agent);
        nimcp_health_agent_disconnect_world_model(agent);
        nimcp_health_agent_disconnect_jepa(agent);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    printf("Completed 500 cycles in %ld ms\n", duration.count());
}

/* ============================================================================
 * Concurrent Operation Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthE2ETest, ConcurrentMultiThreadedOperations) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);
    imagination_engine_t* imag = create_mock_imagination(1);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);
    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    std::atomic<bool> stop{false};
    std::atomic<int> total_wm_checks{0};
    std::atomic<int> total_imag_checks{0};
    std::atomic<int> total_metrics_reads{0};
    std::atomic<int> total_recoveries{0};

    // Thread 1: Continuous world model checking
    std::thread wm_thread([&]() {
        while (!stop.load()) {
            nimcp_health_agent_check_world_model_now(agent);
            total_wm_checks.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Thread 2: Continuous imagination checking
    std::thread imag_thread([&]() {
        while (!stop.load()) {
            nimcp_health_agent_check_imagination_now(agent);
            total_imag_checks.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Thread 3: Continuous metrics reading
    std::thread metrics_thread([&]() {
        while (!stop.load()) {
            world_imagination_health_t health = {};
            nimcp_health_agent_get_world_imagination_health(agent, &health);
            nimcp_health_agent_get_world_model_health_score(agent);
            nimcp_health_agent_get_imagination_health_score(agent);
            total_metrics_reads.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    // Thread 4: Occasional recovery actions
    std::thread recovery_thread([&]() {
        while (!stop.load()) {
            nimcp_health_agent_world_model_recovery(
                agent, WM_RECOVERY_NONE, "concurrent test");
            total_recoveries.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Run for 500ms
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop.store(true);

    wm_thread.join();
    imag_thread.join();
    metrics_thread.join();
    recovery_thread.join();

    printf("Concurrent test results:\n");
    printf("  Total WM checks: %d\n", total_wm_checks.load());
    printf("  Total imag checks: %d\n", total_imag_checks.load());
    printf("  Total metrics reads: %d\n", total_metrics_reads.load());
    printf("  Total recoveries: %d\n", total_recoveries.load());

    EXPECT_GT(total_wm_checks.load(), 0);
    EXPECT_GT(total_imag_checks.load(), 0);
    EXPECT_GT(total_metrics_reads.load(), 0);
    EXPECT_GT(total_recoveries.load(), 0);

    // Verify agent is still in valid state
    float wm_score = nimcp_health_agent_get_world_model_health_score(agent);
    float imag_score = nimcp_health_agent_get_imagination_health_score(agent);
    EXPECT_GE(wm_score, 0.0f);
    EXPECT_LE(wm_score, 1.0f);
    EXPECT_GE(imag_score, 0.0f);
    EXPECT_LE(imag_score, 1.0f);
}

/* ============================================================================
 * Recovery Scenario Tests
 * ============================================================================ */

TEST_F(WorldImaginationHealthE2ETest, RecoverySequenceE2E) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);
    imagination_engine_t* imag = create_mock_imagination(1);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);
    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    // Build up some history
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_check_world_model_now(agent);
        nimcp_health_agent_check_imagination_now(agent);
    }

    world_imagination_health_t health_before = {};
    nimcp_health_agent_get_world_imagination_health(agent, &health_before);
    printf("Health before recovery: check_count=%u\n", health_before.check_count);

    // Execute all recovery actions in sequence
    printf("Executing all recovery actions...\n");
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(agent, WM_RECOVERY_NONE, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(agent, WM_RECOVERY_RESET_PREDICTOR, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(agent, WM_RECOVERY_PRUNE_LATENT, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(agent, WM_RECOVERY_RETRAIN_DYNAMICS, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(agent, WM_RECOVERY_CLEAR_WORKSPACE, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(agent, WM_RECOVERY_REDUCE_HORIZON, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(agent, WM_RECOVERY_INCREASE_REALITY_CHECK, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(agent, WM_RECOVERY_THROTTLE_IMAGINATION, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(agent, WM_RECOVERY_BOOST_GROUNDING, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_world_model_recovery(agent, WM_RECOVERY_CHECKPOINT_RESTORE, "test"), 0);

    // System should still be functional after all recoveries
    EXPECT_EQ(nimcp_health_agent_check_world_model_now(agent), 0);
    EXPECT_EQ(nimcp_health_agent_check_imagination_now(agent), 0);

    world_imagination_health_t health_after = {};
    nimcp_health_agent_get_world_imagination_health(agent, &health_after);
    printf("Health after recovery: check_count=%u\n", health_after.check_count);
}

TEST_F(WorldImaginationHealthE2ETest, RecoveryFromDegradedState) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);
    imagination_engine_t* imag = create_mock_imagination(1);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);
    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    // Run checks to establish baseline
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_check_world_model_now(agent);
        nimcp_health_agent_check_imagination_now(agent);
    }

    // Get current state
    world_imagination_health_t health = {};
    nimcp_health_agent_get_world_imagination_health(agent, &health);
    printf("Current WM state: %d\n", health.world_model.health_state);
    printf("Current Imag state: %d\n", health.imagination.health_state);

    // Apply recovery based on state
    if (health.world_model.health_state > WM_HEALTH_OPTIMAL) {
        printf("Applying WM recovery...\n");
        EXPECT_EQ(nimcp_health_agent_world_model_recovery(
            agent, WM_RECOVERY_RESET_PREDICTOR, "recovery test"), 0);
    }

    if (health.imagination.health_state > IMAG_HEALTH_VIVID) {
        printf("Applying imagination recovery...\n");
        EXPECT_EQ(nimcp_health_agent_world_model_recovery(
            agent, WM_RECOVERY_CLEAR_WORKSPACE, "recovery test"), 0);
    }

    // Verify system still functional
    float wm_score = nimcp_health_agent_get_world_model_health_score(agent);
    float imag_score = nimcp_health_agent_get_imagination_health_score(agent);
    EXPECT_GE(wm_score, 0.0f);
    EXPECT_LE(wm_score, 1.0f);
    EXPECT_GE(imag_score, 0.0f);
    EXPECT_LE(imag_score, 1.0f);
}

/* ============================================================================
 * Configuration Change Scenarios
 * ============================================================================ */

TEST_F(WorldImaginationHealthE2ETest, ConfigChangeDuringOperation) {
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);

    printf("Testing config changes during operation...\n");

    // Check with default config
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_check_world_model_now(agent);
    }

    float score1 = nimcp_health_agent_get_world_model_health_score(agent);
    printf("Score with default config: %.4f\n", score1);

    // Change to strict config
    health_agent_wm_imagination_config_t strict_config = {};
    nimcp_health_agent_wm_imagination_config_default(&strict_config);
    strict_config.jepa_error_warning = 0.01f;  // Very strict
    strict_config.jepa_error_critical = 0.05f;
    strict_config.coherence_warning = 0.95f;  // Very high requirement
    strict_config.embedding_variance_min = 0.5f;
    nimcp_health_agent_update_wm_imagination_config(agent, &strict_config);

    // Check with strict config
    nimcp_health_agent_check_world_model_now(agent);
    float score2 = nimcp_health_agent_get_world_model_health_score(agent);
    printf("Score with strict config: %.4f\n", score2);

    // Change to lenient config
    health_agent_wm_imagination_config_t lenient_config = {};
    nimcp_health_agent_wm_imagination_config_default(&lenient_config);
    lenient_config.jepa_error_warning = 0.9f;  // Very lenient
    lenient_config.jepa_error_critical = 0.99f;
    lenient_config.coherence_warning = 0.1f;
    lenient_config.embedding_variance_min = 0.001f;
    nimcp_health_agent_update_wm_imagination_config(agent, &lenient_config);

    // Check with lenient config
    nimcp_health_agent_check_world_model_now(agent);
    float score3 = nimcp_health_agent_get_world_model_health_score(agent);
    printf("Score with lenient config: %.4f\n", score3);

    // All scores should be valid
    EXPECT_GE(score1, 0.0f);
    EXPECT_LE(score1, 1.0f);
    EXPECT_GE(score2, 0.0f);
    EXPECT_LE(score2, 1.0f);
    EXPECT_GE(score3, 0.0f);
    EXPECT_LE(score3, 1.0f);
}

/* ============================================================================
 * Metrics Validation E2E Test
 * ============================================================================ */

TEST_F(WorldImaginationHealthE2ETest, MetricsValidation) {
    jepa_predictor_t* jepa = create_mock_jepa(42);
    omni_world_model_t* wm = create_mock_world_model(42);
    imagination_engine_t* imag = create_mock_imagination(42);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);
    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    // Run checks to build history
    const int num_checks = 20;
    for (int i = 0; i < num_checks; i++) {
        nimcp_health_agent_check_world_model_now(agent);
        nimcp_health_agent_check_imagination_now(agent);
    }

    world_imagination_health_t health = {};
    EXPECT_EQ(nimcp_health_agent_get_world_imagination_health(agent, &health), 0);

    printf("=== Metrics Validation ===\n");
    printf("Check count: %lu\n", (unsigned long)health.check_count);
    printf("Last check timestamp: %lu\n", (unsigned long)health.last_check_timestamp_us);

    printf("\n--- JEPA Health ---\n");
    printf("  Mean prediction error: %.4f\n", health.jepa.mean_prediction_error);
    printf("  Embedding variance: %.4f\n", health.jepa.embedding_variance);
    printf("  Embedding orthogonality: %.4f\n", health.jepa.embedding_orthogonality);
    printf("  Embedding utilization: %.4f\n", health.jepa.embedding_utilization);

    printf("\n--- World Model Health ---\n");
    printf("  Forward accuracy: %.4f\n", health.world_model.forward_accuracy);
    printf("  Forward consistency: %.4f\n", health.world_model.forward_consistency);
    printf("  Counterfactual validity: %.4f\n", health.world_model.counterfactual_validity);
    printf("  Health state: %d\n", health.world_model.health_state);
    printf("  Health score: %.4f\n", health.world_model.health_score);

    printf("\n--- Imagination Health ---\n");
    printf("  Scene vividness: %.4f\n", health.imagination.scene_vividness);
    printf("  Scene coherence: %.4f\n", health.imagination.scene_coherence);
    printf("  Reality check pass rate: %.4f\n", health.imagination.reality_check_pass_rate);
    printf("  Health state: %d\n", health.imagination.health_state);
    printf("  Health score: %.4f\n", health.imagination.health_score);

    printf("\n--- Combined Metrics ---\n");
    printf("  WM state: %d\n", health.world_model.health_state);
    printf("  Imag state: %d\n", health.imagination.health_state);
    printf("  Alignment: %.4f\n", health.wm_imagination_alignment);
    printf("  Active anomalies: %u\n", health.active_anomalies);
    printf("  Recommended action: %d\n", health.recommended_action);

    // Validate metrics are in expected ranges
    EXPECT_GE(health.check_count, static_cast<uint64_t>(num_checks));

    // JEPA metrics
    EXPECT_GE(health.jepa.mean_prediction_error, 0.0f);
    EXPECT_GE(health.jepa.embedding_variance, 0.0f);
    EXPECT_GE(health.jepa.embedding_orthogonality, 0.0f);
    EXPECT_LE(health.jepa.embedding_orthogonality, 1.0f);
    EXPECT_GE(health.jepa.embedding_utilization, 0.0f);
    EXPECT_LE(health.jepa.embedding_utilization, 1.0f);

    // World model metrics
    EXPECT_GE(health.world_model.forward_accuracy, 0.0f);
    EXPECT_LE(health.world_model.forward_accuracy, 1.0f);
    EXPECT_GE(health.world_model.forward_consistency, 0.0f);
    EXPECT_LE(health.world_model.forward_consistency, 1.0f);
    EXPECT_GE(health.world_model.counterfactual_validity, 0.0f);
    EXPECT_LE(health.world_model.counterfactual_validity, 1.0f);
    EXPECT_GE(health.world_model.health_score, 0.0f);
    EXPECT_LE(health.world_model.health_score, 1.0f);

    // Imagination metrics
    EXPECT_GE(health.imagination.scene_vividness, 0.0f);
    EXPECT_LE(health.imagination.scene_vividness, 1.0f);
    EXPECT_GE(health.imagination.scene_coherence, 0.0f);
    EXPECT_LE(health.imagination.scene_coherence, 1.0f);
    EXPECT_GE(health.imagination.reality_check_pass_rate, 0.0f);
    EXPECT_LE(health.imagination.reality_check_pass_rate, 1.0f);
    EXPECT_GE(health.imagination.health_score, 0.0f);
    EXPECT_LE(health.imagination.health_score, 1.0f);

    // Combined metrics
    EXPECT_GE(health.wm_imagination_alignment, 0.0f);
    EXPECT_LE(health.wm_imagination_alignment, 1.0f);
}

/* ============================================================================
 * Partial Connection Scenarios
 * ============================================================================ */

TEST_F(WorldImaginationHealthE2ETest, JepaOnlyOperation) {
    printf("Testing JEPA-only operation...\n");
    jepa_predictor_t* jepa = create_mock_jepa(1);
    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);

    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_check_world_model_now(agent);
    }

    float score = nimcp_health_agent_get_world_model_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);

    world_imagination_health_t health = {};
    EXPECT_EQ(nimcp_health_agent_get_world_imagination_health(agent, &health), 0);
    printf("  JEPA embedding collapse: %d\n", health.jepa.embedding_collapse_detected);
}

TEST_F(WorldImaginationHealthE2ETest, WorldModelOnlyOperation) {
    printf("Testing World Model-only operation...\n");
    omni_world_model_t* wm = create_mock_world_model(1);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);

    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_check_world_model_now(agent);
    }

    float score = nimcp_health_agent_get_world_model_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);

    world_imagination_health_t health = {};
    EXPECT_EQ(nimcp_health_agent_get_world_imagination_health(agent, &health), 0);
    printf("  WM state: %d\n", health.world_model.health_state);
}

TEST_F(WorldImaginationHealthE2ETest, ImaginationOnlyOperation) {
    printf("Testing Imagination-only operation...\n");
    imagination_engine_t* imag = create_mock_imagination(1);
    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_check_imagination_now(agent);
    }

    float score = nimcp_health_agent_get_imagination_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);

    world_imagination_health_t health = {};
    EXPECT_EQ(nimcp_health_agent_get_world_imagination_health(agent, &health), 0);
    printf("  Imagination state: %d\n", health.imagination.health_state);
}

/* ============================================================================
 * State Transition Monitoring
 * ============================================================================ */

TEST_F(WorldImaginationHealthE2ETest, StateTransitionMonitoring) {
    printf("Testing state transition monitoring...\n");
    jepa_predictor_t* jepa = create_mock_jepa(1);
    omni_world_model_t* wm = create_mock_world_model(1);
    imagination_engine_t* imag = create_mock_imagination(1);

    nimcp_health_agent_connect_jepa(agent, jepa, nullptr);
    nimcp_health_agent_connect_world_model(agent, wm, nullptr);
    nimcp_health_agent_connect_imagination(agent, imag, nullptr);

    world_model_health_state_t prev_wm_state = WM_HEALTH_OPTIMAL;
    imagination_health_state_t prev_imag_state = IMAG_HEALTH_VIVID;

    for (int i = 0; i < 100; i++) {
        nimcp_health_agent_check_world_model_now(agent);
        nimcp_health_agent_check_imagination_now(agent);

        world_imagination_health_t health = {};
        nimcp_health_agent_get_world_imagination_health(agent, &health);

        if (health.world_model.health_state != prev_wm_state) {
            printf("  WM state transition: %d -> %d (check %d)\n",
                   prev_wm_state, health.world_model.health_state, i);
            prev_wm_state = health.world_model.health_state;
        }

        if (health.imagination.health_state != prev_imag_state) {
            printf("  Imag state transition: %d -> %d (check %d)\n",
                   prev_imag_state, health.imagination.health_state, i);
            prev_imag_state = health.imagination.health_state;
        }

        // Verify states are always valid
        EXPECT_GE(static_cast<int>(health.world_model.health_state), static_cast<int>(WM_HEALTH_OPTIMAL));
        EXPECT_LE(static_cast<int>(health.world_model.health_state), static_cast<int>(WM_HEALTH_CRITICAL));
        EXPECT_GE(static_cast<int>(health.imagination.health_state), static_cast<int>(IMAG_HEALTH_VIVID));
        EXPECT_LE(static_cast<int>(health.imagination.health_state), static_cast<int>(IMAG_HEALTH_IMPAIRED));
    }
}
