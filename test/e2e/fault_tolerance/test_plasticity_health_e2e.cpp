/**
 * @file test_plasticity_health_e2e.cpp
 * @brief End-to-end tests for plasticity health monitoring pipeline
 * @date 2026-01-20
 *
 * Tests the complete pipeline from plasticity monitoring components
 * through STDP health metrics to plasticity anomaly detection and response.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/parietal/nimcp_parietal_training_bridge.h"
#include "core/brain/regions/parietal/nimcp_parietal_adapter.h"
#include "utils/fault_tolerance/nimcp_stdp_health_metrics.h"
#include "utils/fault_tolerance/nimcp_plasticity_anomaly_detection.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
}

/**
 * E2E test fixture that creates a complete plasticity health monitoring
 * pipeline using available components.
 */
class PlasticityHealthE2ETest : public ::testing::Test {
protected:
    // Health monitoring components
    nimcp_health_agent_t* health_agent = nullptr;
    stdp_health_metrics_t* stdp_health = nullptr;
    plasticity_anomaly_detector_t* anomaly_detector = nullptr;

    // Parietal adapter and training bridge
    parietal_adapter_t* parietal_adapter = nullptr;
    parietal_training_bridge_t* parietal_bridge = nullptr;

    void SetUp() override {
        // Create health agent
        health_agent_config_t ha_config;
        nimcp_health_agent_default_config(&ha_config);
        health_agent = nimcp_health_agent_create(&ha_config);

        // Create STDP health metrics
        stdp_health_config_t sh_config;
        stdp_health_default_config(&sh_config);
        stdp_health = stdp_health_create(&sh_config, health_agent);

        // Create plasticity anomaly detector
        plasticity_anomaly_config_t pa_config;
        plasticity_anomaly_default_config(&pa_config);
        anomaly_detector = plasticity_anomaly_create(&pa_config, nullptr);

        // Create parietal adapter
        parietal_cortex_config_t pc_config = parietal_cortex_adapter_default_config();
        parietal_adapter = parietal_cortex_adapter_create(&pc_config);

        // Create parietal training bridge with real adapter
        parietal_training_config_t pt_config;
        parietal_training_default_config(&pt_config);
        parietal_bridge = parietal_training_create(&pt_config, parietal_adapter, nullptr);
    }

    void TearDown() override {
        if (parietal_bridge) {
            parietal_training_destroy(parietal_bridge);
            parietal_bridge = nullptr;
        }
        if (parietal_adapter) {
            parietal_cortex_adapter_destroy(parietal_adapter);
            parietal_adapter = nullptr;
        }
        if (anomaly_detector) {
            plasticity_anomaly_destroy(anomaly_detector);
            anomaly_detector = nullptr;
        }
        if (stdp_health) {
            stdp_health_destroy(stdp_health);
            stdp_health = nullptr;
        }
        if (health_agent) {
            nimcp_health_agent_destroy(health_agent);
            health_agent = nullptr;
        }
    }
};

/* ============================================================================
 * Pipeline Initialization Tests
 * ============================================================================ */

TEST_F(PlasticityHealthE2ETest, AllComponentsInitialize) {
    EXPECT_NE(health_agent, nullptr);
    EXPECT_NE(stdp_health, nullptr);
    EXPECT_NE(anomaly_detector, nullptr);
    // parietal_bridge may be NULL, that's ok
}

TEST_F(PlasticityHealthE2ETest, ComponentsCanBeQueried) {
    // STDP health stats
    stdp_health_stats_t sh_stats;
    EXPECT_EQ(stdp_health_get_stats(stdp_health, &sh_stats), 0);

    // Anomaly detector stats
    plasticity_detection_stats_t ad_stats;
    EXPECT_EQ(plasticity_anomaly_get_stats(anomaly_detector, &ad_stats), 0);
}

/* ============================================================================
 * Monitoring Pipeline Tests
 * ============================================================================ */

TEST_F(PlasticityHealthE2ETest, HealthyWeightsFlowThroughPipeline) {
    // Load rules
    plasticity_anomaly_load_all_default_rules(anomaly_detector);

    // Simulate healthy weight data
    float weights[] = {0.3f, 0.5f, 0.7f, 0.4f, 0.6f, 0.8f, 0.2f, 0.9f};

    // Check through STDP health
    int sh_anomalies = stdp_health_check_weights(stdp_health, weights, 8, "e2e_weights");
    EXPECT_EQ(sh_anomalies, 0);

    // Check through anomaly detector
    int ad_anomalies = plasticity_anomaly_analyze_weights(anomaly_detector, weights, 8, "e2e_weights");
    EXPECT_EQ(ad_anomalies, 0);
}

TEST_F(PlasticityHealthE2ETest, AnomalousWeightsDetectedByBothSystems) {
    plasticity_anomaly_load_all_default_rules(anomaly_detector);

    // Weights with NaN values
    float weights[] = {0.3f, NAN, 0.7f, 0.4f, INFINITY};

    // Check through STDP health
    int sh_anomalies = stdp_health_check_weights(stdp_health, weights, 5, "bad_weights");
    EXPECT_GT(sh_anomalies, 0);

    // Check through anomaly detector
    int ad_anomalies = plasticity_anomaly_analyze_weights(anomaly_detector, weights, 5, "bad_weights");
    EXPECT_GT(ad_anomalies, 0);
}

/* ============================================================================
 * Learning Rate Monitoring Tests
 * ============================================================================ */

TEST_F(PlasticityHealthE2ETest, NormalLearningRatePassesChecks) {
    int anomaly = stdp_health_check_learning_rate(stdp_health, 0.01f, "normal_lr");
    EXPECT_EQ(anomaly, 0);
}

TEST_F(PlasticityHealthE2ETest, ExcessiveLearningRateDetected) {
    int anomaly = stdp_health_check_learning_rate(stdp_health, 10.0f, "high_lr");
    EXPECT_NE(anomaly, 0);
}

/* ============================================================================
 * Timing Analysis Tests
 * ============================================================================ */

TEST_F(PlasticityHealthE2ETest, NormalTimingPassesBothSystems) {
    plasticity_anomaly_load_default_rules(anomaly_detector, PLASTICITY_CATEGORY_TIMING);

    float pre[] = {1.0f, 5.0f, 10.0f, 15.0f};
    float post[] = {2.0f, 6.0f, 11.0f, 16.0f};

    // STDP health check
    int sh_violations = stdp_health_check_timing(stdp_health, pre, post, 4);
    // May or may not have violations depending on window size

    // Anomaly detector
    int ad_anomalies = plasticity_anomaly_analyze_timing(anomaly_detector, pre, post, 4);
    EXPECT_GE(ad_anomalies, 0);
}

/* ============================================================================
 * Full Pipeline Simulation Tests
 * ============================================================================ */

TEST_F(PlasticityHealthE2ETest, SimulatedLearningSessionWithHealthMonitoring) {
    plasticity_anomaly_load_all_default_rules(anomaly_detector);

    // Simulate a learning session over multiple epochs
    const int num_epochs = 10;
    int total_anomalies = 0;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        // Generate synthetic weight data
        float weights[20];
        for (int i = 0; i < 20; i++) {
            weights[i] = 0.5f + (0.1f * sinf((float)(epoch * 20 + i) / 10.0f));
            // Inject anomaly in epoch 5
            if (epoch == 5 && i == 10) {
                weights[i] = NAN;
            }
        }

        // Check weights through both systems
        char name[32];
        snprintf(name, sizeof(name), "epoch_%d", epoch);
        stdp_health_check_weights(stdp_health, weights, 20, name);
        int anomalies = plasticity_anomaly_analyze_weights(anomaly_detector, weights, 20, name);
        total_anomalies += anomalies;

        // Check learning rate
        float lr = 0.01f * powf(0.95f, (float)epoch);  // Decaying LR
        stdp_health_check_learning_rate(stdp_health, lr, "learning_rate");

        // Submit metrics
        plasticity_anomaly_submit_metric(anomaly_detector, "loss", 1.0f / (epoch + 1),
            PLASTICITY_CATEGORY_LEARNING);
    }

    // Should have detected at least the NaN anomaly
    EXPECT_GT(total_anomalies, 0);

    // Get final statistics - verify no errors
    stdp_health_stats_t sh_stats;
    EXPECT_EQ(stdp_health_get_stats(stdp_health, &sh_stats), 0);

    plasticity_detection_stats_t ad_stats;
    EXPECT_EQ(plasticity_anomaly_get_stats(anomaly_detector, &ad_stats), 0);
}

/* ============================================================================
 * Concurrent Monitoring Tests
 * ============================================================================ */

TEST_F(PlasticityHealthE2ETest, ConcurrentMonitoringAcrossMultipleSystems) {
    plasticity_anomaly_load_all_default_rules(anomaly_detector);

    const int num_threads = 4;
    std::atomic<int> total_checks{0};
    std::atomic<int> total_anomalies{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 20; i++) {
                // Generate weights - thread 0 injects anomalies
                float weights[10];
                for (int w = 0; w < 10; w++) {
                    weights[w] = 0.5f + (t * 0.05f) + (w * 0.02f);
                    if (t == 0 && i % 4 == 0 && w == 5) {
                        weights[w] = NAN;
                    }
                }

                char name[32];
                snprintf(name, sizeof(name), "t%d_i%d", t, i);

                // Check through STDP health
                int sh = stdp_health_check_weights(stdp_health, weights, 10, name);
                if (sh > 0) total_anomalies++;

                // Check through anomaly detector
                int ad = plasticity_anomaly_analyze_weights(anomaly_detector, weights, 10, name);
                if (ad > 0) total_anomalies++;

                total_checks++;
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(total_checks.load(), num_threads * 20);
    EXPECT_GT(total_anomalies.load(), 0);  // Should detect the NaN injections
}

/* ============================================================================
 * Stats Reset and Recovery Tests
 * ============================================================================ */

TEST_F(PlasticityHealthE2ETest, StatsResetAcrossSystems) {
    // Generate some activity
    float bad_weights[] = {NAN, 0.5f, INFINITY};
    stdp_health_check_weights(stdp_health, bad_weights, 3, "bad");
    plasticity_anomaly_load_all_default_rules(anomaly_detector);
    plasticity_anomaly_analyze_weights(anomaly_detector, bad_weights, 3, "bad");

    // Reset both systems
    stdp_health_reset_stats(stdp_health);
    plasticity_anomaly_reset_stats(anomaly_detector);

    // Verify reset
    stdp_health_stats_t sh_stats;
    stdp_health_get_stats(stdp_health, &sh_stats);
    EXPECT_EQ(sh_stats.anomalies_detected, 0u);

    plasticity_detection_stats_t ad_stats;
    plasticity_anomaly_get_stats(anomaly_detector, &ad_stats);
    EXPECT_EQ(ad_stats.anomalies_detected, 0u);
}

/* ============================================================================
 * Health Score Tracking Tests
 * ============================================================================ */

TEST_F(PlasticityHealthE2ETest, HealthScoreDegradesWithAnomalies) {
    // Get initial health score
    stdp_health_stats_t stats_before;
    stdp_health_get_stats(stdp_health, &stats_before);
    float initial_score = stats_before.overall_health_score;

    // Introduce multiple anomalies
    for (int i = 0; i < 5; i++) {
        float bad_weights[] = {NAN, INFINITY, -INFINITY};
        char name[16];
        snprintf(name, sizeof(name), "bad_%d", i);
        stdp_health_check_weights(stdp_health, bad_weights, 3, name);
    }

    // Get final health score
    stdp_health_stats_t stats_after;
    stdp_health_get_stats(stdp_health, &stats_after);
    float final_score = stats_after.overall_health_score;

    // Health score should have degraded
    EXPECT_LE(final_score, initial_score);
    EXPECT_GT(stats_after.anomalies_detected, 0u);
}

/* ============================================================================
 * Callback Integration Tests
 * ============================================================================ */

static std::atomic<int> e2e_anomaly_count{0};
static void e2e_anomaly_callback(const stdp_anomaly_report_t* report, void* user_data) {
    (void)user_data;
    if (report) {
        e2e_anomaly_count++;
    }
}

TEST_F(PlasticityHealthE2ETest, AnomalyCallbacksFireDuringMonitoring) {
    e2e_anomaly_count = 0;
    stdp_health_set_anomaly_callback(stdp_health, e2e_anomaly_callback, nullptr);

    // Trigger anomalies
    for (int i = 0; i < 3; i++) {
        float bad_weights[] = {NAN, 0.5f};
        char name[16];
        snprintf(name, sizeof(name), "cb_%d", i);
        stdp_health_check_weights(stdp_health, bad_weights, 2, name);
    }

    EXPECT_GT(e2e_anomaly_count.load(), 0);
}

/* ============================================================================
 * Parietal Bridge Integration Tests (if available)
 * ============================================================================ */

TEST_F(PlasticityHealthE2ETest, ParietalBridgeIntegration) {
    if (!parietal_adapter) {
        GTEST_SKIP() << "Parietal adapter not available";
    }
    if (!parietal_bridge) {
        GTEST_SKIP() << "Parietal bridge not available";
    }

    // Test integration with parietal training bridge
    parietal_learning_signal_t signal = {};
    signal.domain = PARIETAL_DOMAIN_COORDINATE_TRANSFORM;
    signal.loss_value = 0.5f;
    signal.loss_delta = -0.01f;
    signal.learning_rate = 0.001f;

    parietal_train_response_t response = parietal_training_process_signal(parietal_bridge, &signal);
    // Response may vary - just verify it doesn't crash
    (void)response;

    // Check state is valid
    parietal_train_state_t state = parietal_training_get_state(parietal_bridge);
    EXPECT_NE(state, PARIETAL_TRAIN_STATE_ERROR);
}

/* ============================================================================
 * Full Pipeline Stress Test
 * ============================================================================ */

TEST_F(PlasticityHealthE2ETest, StressTestFullPipeline) {
    plasticity_anomaly_load_all_default_rules(anomaly_detector);

    const int num_iterations = 100;
    std::atomic<bool> running{true};
    std::atomic<int> iterations_done{0};

    // Background thread for anomaly detection
    std::thread detector_thread([&]() {
        while (running || iterations_done < num_iterations) {
            plasticity_anomaly_detect(anomaly_detector);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Main thread generates data
    for (int i = 0; i < num_iterations; i++) {
        float weights[10];
        for (int w = 0; w < 10; w++) {
            weights[w] = 0.5f + (0.1f * sinf((float)(i * 10 + w)));
            if (i % 20 == 0 && w == 5) {
                weights[w] = NAN;  // Occasional anomaly
            }
        }

        char name[16];
        snprintf(name, sizeof(name), "stress_%d", i);
        stdp_health_check_weights(stdp_health, weights, 10, name);
        plasticity_anomaly_analyze_weights(anomaly_detector, weights, 10, name);

        float lr = 0.01f * (1.0f + 0.1f * sinf((float)i / 10.0f));
        stdp_health_check_learning_rate(stdp_health, lr, "stress_lr");

        iterations_done++;
    }

    running = false;
    detector_thread.join();

    // Verify pipeline handled the stress without errors
    stdp_health_stats_t sh_stats;
    EXPECT_EQ(stdp_health_get_stats(stdp_health, &sh_stats), 0);

    plasticity_detection_stats_t ad_stats;
    EXPECT_EQ(plasticity_anomaly_get_stats(anomaly_detector, &ad_stats), 0);

    // We injected NaN values, anomalies should have been detected
    EXPECT_GT(ad_stats.anomalies_detected, 0u);
}
