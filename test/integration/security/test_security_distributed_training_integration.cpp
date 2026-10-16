/**
 * @file test_security_distributed_training_integration.cpp
 * @brief Integration tests for security-distributed training bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Integration tests for security-distributed training:
 * 1. Multi-worker scenarios
 * 2. Byzantine fault tolerance testing
 * 3. Gradient validation workflows
 * 4. End-to-end training round simulation
 * 5. Checkpoint and recovery scenarios
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <random>
#include <thread>
#include <chrono>

extern "C" {
#include "security/distributed/nimcp_security_distributed_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

/* =============================================================================
 * Test Fixtures
 * ============================================================================= */

class SecurityDistributedIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        EXPECT_EQ(0, security_distributed_training_default_config(&config));
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            security_distributed_training_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    void SetUpWorkers(int count) {
        for (int i = 0; i < count; i++) {
            std::string worker_id = "worker_" + std::to_string(i);
            EXPECT_EQ(0, security_distributed_training_register_worker(
                bridge, worker_id.c_str(), nullptr));
            EXPECT_EQ(0, security_distributed_training_set_worker_trust(
                bridge, worker_id.c_str(), SECURITY_WORKER_TRUST_VERIFIED));
        }
    }

    std::vector<float> GenerateNormalGradients(size_t size, float scale = 0.1f) {
        std::vector<float> gradients(size);
        std::mt19937 gen(42);
        std::normal_distribution<float> dist(0.0f, scale);
        for (size_t i = 0; i < size; i++) {
            gradients[i] = dist(gen);
        }
        return gradients;
    }

    std::vector<float> GenerateMaliciousGradients(size_t size, float magnitude = 100.0f) {
        std::vector<float> gradients(size);
        std::mt19937 gen(123);
        std::normal_distribution<float> dist(0.0f, magnitude);
        for (size_t i = 0; i < size; i++) {
            gradients[i] = dist(gen);
        }
        return gradients;
    }

    security_distributed_training_config_t config;
    security_distributed_training_bridge_t* bridge;
};

/* =============================================================================
 * Multi-Worker Scenario Tests
 * ============================================================================= */

TEST_F(SecurityDistributedIntegrationTest, MultiWorkerRegistration) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const int num_workers = 50;
    SetUpWorkers(num_workers);

    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_EQ(50u, stats.total_workers_registered);
    EXPECT_EQ(50u, stats.current_active_workers);
}

TEST_F(SecurityDistributedIntegrationTest, MultiWorkerGradientSubmission) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const int num_workers = 10;
    SetUpWorkers(num_workers);

    const size_t gradient_size = 100;

    /* All workers submit gradients */
    for (int w = 0; w < num_workers; w++) {
        std::string worker_id = "worker_" + std::to_string(w);
        auto gradients = GenerateNormalGradients(gradient_size);

        security_gradient_validation_result_t result;
        EXPECT_EQ(0, security_distributed_training_validate_gradients(
            bridge, worker_id.c_str(), gradients.data(), gradients.size(), &result));
        EXPECT_TRUE(result.is_valid);
    }

    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_EQ(10u, stats.gradients_validated);
    EXPECT_EQ(0u, stats.gradients_rejected);
}

TEST_F(SecurityDistributedIntegrationTest, WorkerTrustProgression) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));

    /* Initial trust is UNTRUSTED */
    EXPECT_EQ(SECURITY_WORKER_TRUST_UNTRUSTED,
              security_distributed_training_get_worker_trust(bridge, "worker_0"));

    /* Progress through trust levels */
    EXPECT_EQ(0, security_distributed_training_set_worker_trust(
        bridge, "worker_0", SECURITY_WORKER_TRUST_PROBATION));
    EXPECT_EQ(SECURITY_WORKER_TRUST_PROBATION,
              security_distributed_training_get_worker_trust(bridge, "worker_0"));

    EXPECT_EQ(0, security_distributed_training_set_worker_trust(
        bridge, "worker_0", SECURITY_WORKER_TRUST_VERIFIED));
    EXPECT_EQ(SECURITY_WORKER_TRUST_VERIFIED,
              security_distributed_training_get_worker_trust(bridge, "worker_0"));

    EXPECT_EQ(0, security_distributed_training_set_worker_trust(
        bridge, "worker_0", SECURITY_WORKER_TRUST_TRUSTED));
    EXPECT_EQ(SECURITY_WORKER_TRUST_TRUSTED,
              security_distributed_training_get_worker_trust(bridge, "worker_0"));
}

/* =============================================================================
 * Byzantine Fault Tolerance Tests
 * ============================================================================= */

TEST_F(SecurityDistributedIntegrationTest, ByzantineWorkerDetection) {
    config.enable_byzantine_detection = true;
    config.auto_quarantine_byzantine = true;
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const int num_workers = 10;
    SetUpWorkers(num_workers);

    /* Simulate Byzantine behavior from worker_0 */
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(0, security_distributed_training_report_worker_anomaly(
            bridge, "worker_0", 0.95f, "Byzantine gradient attack"));
    }

    /* Check if Byzantine was detected and quarantined */
    security_worker_info_t info;
    EXPECT_EQ(0, security_distributed_training_get_worker_info(bridge, "worker_0", &info));
    EXPECT_TRUE(info.is_quarantined);
    EXPECT_GE(info.byzantine_detections, 3u);

    security_byzantine_result_t result;
    EXPECT_EQ(0, security_distributed_training_detect_byzantine(bridge, &result));

    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_GE(stats.workers_quarantined, 1u);
}

TEST_F(SecurityDistributedIntegrationTest, MultipleByzantineWorkers) {
    config.enable_byzantine_detection = true;
    config.auto_quarantine_byzantine = true;
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const int num_workers = 10;
    const int num_byzantine = 3;
    SetUpWorkers(num_workers);

    /* Simulate Byzantine behavior from multiple workers */
    for (int b = 0; b < num_byzantine; b++) {
        std::string worker_id = "worker_" + std::to_string(b);
        for (int i = 0; i < 5; i++) {
            EXPECT_EQ(0, security_distributed_training_report_worker_anomaly(
                bridge, worker_id.c_str(), 0.95f, "Coordinated attack"));
        }
    }

    /* Run Byzantine detection */
    security_byzantine_result_t result;
    EXPECT_EQ(0, security_distributed_training_detect_byzantine(bridge, &result));

    /* Update security effects */
    EXPECT_EQ(0, security_distributed_training_update_security_effects(bridge));

    /* Check Byzantine ratio */
    float byzantine_ratio = security_distributed_training_get_byzantine_ratio(bridge);
    EXPECT_GT(byzantine_ratio, 0.0f);

    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_GE(stats.workers_quarantined, (uint64_t)num_byzantine);
}

TEST_F(SecurityDistributedIntegrationTest, ByzantineFaultToleranceThreshold) {
    config.enable_byzantine_detection = true;
    config.byzantine_threshold = 0.8f;
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    /* Need 3f+1 workers for f Byzantine tolerance */
    const int num_workers = 10;  /* Can tolerate up to 3 Byzantine */
    SetUpWorkers(num_workers);

    /* Simulate exactly 3 Byzantine workers */
    for (int b = 0; b < 3; b++) {
        std::string worker_id = "worker_" + std::to_string(b);
        for (int i = 0; i < 10; i++) {
            security_distributed_training_report_worker_anomaly(
                bridge, worker_id.c_str(), 0.9f, "Byzantine behavior");
        }
    }

    /* System should still function with 7 honest workers */
    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));

    /* At least 3 should be quarantined */
    EXPECT_GE(stats.current_quarantined_workers, 3u);

    /* Should still have honest workers active */
    EXPECT_GE(stats.current_active_workers, 7u);
}

/* =============================================================================
 * Gradient Validation Workflow Tests
 * ============================================================================= */

TEST_F(SecurityDistributedIntegrationTest, GradientValidationWithOutlier) {
    config.enable_gradient_validation = true;
    config.gradient_norm_threshold = 10.0f;
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const int num_workers = 5;
    SetUpWorkers(num_workers);

    const size_t gradient_size = 100;

    /* Submit normal gradients from most workers */
    for (int w = 0; w < num_workers - 1; w++) {
        std::string worker_id = "worker_" + std::to_string(w);
        auto gradients = GenerateNormalGradients(gradient_size, 0.1f);

        security_gradient_validation_result_t result;
        EXPECT_EQ(0, security_distributed_training_validate_gradients(
            bridge, worker_id.c_str(), gradients.data(), gradients.size(), &result));
        EXPECT_TRUE(result.is_valid);
    }

    /* Submit malicious gradient from last worker */
    std::string malicious_worker = "worker_" + std::to_string(num_workers - 1);
    auto malicious_gradients = GenerateMaliciousGradients(gradient_size, 50.0f);

    security_gradient_validation_result_t result;
    EXPECT_EQ(0, security_distributed_training_validate_gradients(
        bridge, malicious_worker.c_str(), malicious_gradients.data(),
        malicious_gradients.size(), &result));

    /* Should be flagged as suspicious or rejected */
    EXPECT_TRUE(result.is_suspicious || !result.is_valid);
    EXPECT_TRUE(result.norm_exceeded);
}

TEST_F(SecurityDistributedIntegrationTest, AggregatedGradientValidation) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    /* Create aggregated gradient from multiple workers */
    const size_t gradient_size = 100;
    std::vector<float> aggregated(gradient_size, 0.0f);

    const int num_workers = 5;
    for (int w = 0; w < num_workers; w++) {
        auto worker_grad = GenerateNormalGradients(gradient_size, 0.1f);
        for (size_t i = 0; i < gradient_size; i++) {
            aggregated[i] += worker_grad[i] / num_workers;
        }
    }

    float anomaly_score = 0.0f;
    bool anomaly = security_distributed_training_validate_aggregated(
        bridge, aggregated.data(), aggregated.size(), &anomaly_score);

    EXPECT_FALSE(anomaly);
    EXPECT_LT(anomaly_score, 0.5f);
}

TEST_F(SecurityDistributedIntegrationTest, AggregatedGradientWithPoisoning) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    /* Create aggregated gradient with a poisoned worker */
    const size_t gradient_size = 100;
    std::vector<float> aggregated(gradient_size, 0.0f);

    const int num_workers = 5;
    for (int w = 0; w < num_workers - 1; w++) {
        auto worker_grad = GenerateNormalGradients(gradient_size, 0.1f);
        for (size_t i = 0; i < gradient_size; i++) {
            aggregated[i] += worker_grad[i] / num_workers;
        }
    }

    /* Add poisoned gradient (extremely large) */
    auto poisoned_grad = GenerateMaliciousGradients(gradient_size, 1000.0f);
    for (size_t i = 0; i < gradient_size; i++) {
        aggregated[i] += poisoned_grad[i] / num_workers;
    }

    float anomaly_score = 0.0f;
    bool anomaly = security_distributed_training_validate_aggregated(
        bridge, aggregated.data(), aggregated.size(), &anomaly_score);

    /* Should detect anomaly due to poisoning */
    EXPECT_TRUE(anomaly);
    EXPECT_GT(anomaly_score, 0.5f);
}

/* =============================================================================
 * End-to-End Training Round Simulation
 * ============================================================================= */

TEST_F(SecurityDistributedIntegrationTest, SimulateTrainingRound) {
    config.enable_byzantine_detection = true;
    config.enable_gradient_validation = true;
    config.enable_secure_checkpointing = true;
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const int num_workers = 10;
    const size_t gradient_size = 100;
    const int num_rounds = 5;

    SetUpWorkers(num_workers);

    std::vector<float> model_weights(gradient_size, 0.5f);

    for (int round = 0; round < num_rounds; round++) {
        /* Update training effects */
        EXPECT_EQ(0, security_distributed_training_update_training_effects(
            bridge, round, 0.5f - 0.05f * round, num_workers));

        /* Collect and validate gradients from all workers */
        std::vector<float> aggregated(gradient_size, 0.0f);
        int valid_count = 0;

        for (int w = 0; w < num_workers; w++) {
            std::string worker_id = "worker_" + std::to_string(w);
            auto gradients = GenerateNormalGradients(gradient_size, 0.1f);

            security_gradient_validation_result_t result;
            EXPECT_EQ(0, security_distributed_training_validate_gradients(
                bridge, worker_id.c_str(), gradients.data(), gradients.size(), &result));

            if (result.is_valid) {
                for (size_t i = 0; i < gradient_size; i++) {
                    aggregated[i] += gradients[i];
                }
                valid_count++;
            }
        }

        /* Average gradients */
        if (valid_count > 0) {
            for (size_t i = 0; i < gradient_size; i++) {
                aggregated[i] /= valid_count;
            }
        }

        /* Validate aggregated gradient */
        float anomaly_score;
        bool anomaly = security_distributed_training_validate_aggregated(
            bridge, aggregated.data(), gradient_size, &anomaly_score);
        EXPECT_FALSE(anomaly);

        /* Apply gradients to model (simple SGD) */
        for (size_t i = 0; i < gradient_size; i++) {
            model_weights[i] -= 0.01f * aggregated[i];
        }

        /* Run Byzantine detection */
        security_byzantine_result_t byz_result;
        EXPECT_EQ(0, security_distributed_training_detect_byzantine(bridge, &byz_result));
        EXPECT_FALSE(byz_result.byzantine_detected);

        /* Update security effects */
        EXPECT_EQ(0, security_distributed_training_update_security_effects(bridge));

        /* Checkpoint every 2 rounds */
        if (round % 2 == 0) {
            std::string checkpoint_name = "round_" + std::to_string(round);
            EXPECT_EQ(0, security_distributed_training_secure_checkpoint(
                bridge, checkpoint_name.c_str(), model_weights.data(),
                model_weights.size(), round));
        }
    }

    /* Verify final state */
    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_EQ(num_rounds * num_workers, (int)stats.gradients_validated);
    EXPECT_EQ(num_rounds, (int)stats.byzantine_checks);
    EXPECT_GE(stats.checkpoints_created, 2u);
}

TEST_F(SecurityDistributedIntegrationTest, SimulateTrainingWithByzantine) {
    config.enable_byzantine_detection = true;
    config.enable_gradient_validation = true;
    config.auto_quarantine_byzantine = true;
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const int num_workers = 10;
    const int num_byzantine = 2;
    const size_t gradient_size = 100;
    const int num_rounds = 3;

    SetUpWorkers(num_workers);

    for (int round = 0; round < num_rounds; round++) {
        /* Submit gradients */
        for (int w = 0; w < num_workers; w++) {
            std::string worker_id = "worker_" + std::to_string(w);

            std::vector<float> gradients;
            if (w < num_byzantine) {
                /* Byzantine workers submit malicious gradients */
                gradients = GenerateMaliciousGradients(gradient_size, 50.0f);
            } else {
                gradients = GenerateNormalGradients(gradient_size, 0.1f);
            }

            security_gradient_validation_result_t result;
            security_distributed_training_validate_gradients(
                bridge, worker_id.c_str(), gradients.data(), gradients.size(), &result);

            /* Report anomalies for Byzantine workers */
            if (w < num_byzantine) {
                security_distributed_training_report_worker_anomaly(
                    bridge, worker_id.c_str(), 0.9f, "Gradient manipulation");
            }
        }

        /* Run Byzantine detection */
        security_byzantine_result_t byz_result;
        security_distributed_training_detect_byzantine(bridge, &byz_result);
    }

    /* Byzantine workers should be detected/quarantined */
    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_GE(stats.gradients_suspicious + stats.gradients_rejected, (uint64_t)num_byzantine);
}

/* =============================================================================
 * Checkpoint and Recovery Tests
 * ============================================================================= */

TEST_F(SecurityDistributedIntegrationTest, CheckpointAndVerify) {
    config.enable_secure_checkpointing = true;
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const size_t model_size = 1000;
    std::vector<float> model(model_size);
    for (size_t i = 0; i < model_size; i++) {
        model[i] = (float)i / model_size;
    }

    /* Create checkpoint */
    EXPECT_EQ(0, security_distributed_training_secure_checkpoint(
        bridge, "checkpoint_100", model.data(), model.size(), 100));

    /* Verify checkpoint */
    security_checkpoint_result_t result = security_distributed_training_verify_checkpoint(
        bridge, "checkpoint_100", model.data(), model.size());
    EXPECT_EQ(SECURITY_CHECKPOINT_OK, result);

    /* Modify model and verify fails */
    model[0] = 999.9f;
    result = security_distributed_training_verify_checkpoint(
        bridge, "checkpoint_100", model.data(), model.size());
    EXPECT_EQ(SECURITY_CHECKPOINT_HASH_MISMATCH, result);
}

TEST_F(SecurityDistributedIntegrationTest, MultipleCheckpoints) {
    config.enable_secure_checkpointing = true;
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const size_t model_size = 100;
    std::vector<float> model(model_size, 0.5f);

    /* Create multiple checkpoints */
    for (int i = 0; i < 5; i++) {
        std::string cp_name = "checkpoint_" + std::to_string(i * 100);

        /* Modify model between checkpoints */
        for (size_t j = 0; j < model_size; j++) {
            model[j] += 0.1f;
        }

        EXPECT_EQ(0, security_distributed_training_secure_checkpoint(
            bridge, cp_name.c_str(), model.data(), model.size(), i * 100));
    }

    /* List checkpoints */
    std::vector<security_distributed_checkpoint_t> checkpoints(10);
    int count = security_distributed_training_list_checkpoints(
        bridge, checkpoints.data(), 10);
    EXPECT_EQ(5, count);

    /* Each checkpoint should have different hash */
    for (int i = 0; i < count - 1; i++) {
        bool same = true;
        for (size_t j = 0; j < SECURITY_DISTRIBUTED_HASH_SIZE; j++) {
            if (checkpoints[i].model_hash[j] != checkpoints[i+1].model_hash[j]) {
                same = false;
                break;
            }
        }
        EXPECT_FALSE(same) << "Checkpoints " << i << " and " << i+1 << " have same hash";
    }
}

/* =============================================================================
 * Threat Level and Response Tests
 * ============================================================================= */

TEST_F(SecurityDistributedIntegrationTest, ThreatLevelEscalation) {
    config.enable_byzantine_detection = true;
    config.auto_quarantine_byzantine = true;
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const int num_workers = 10;
    SetUpWorkers(num_workers);

    /* Initial threat level should be low */
    EXPECT_EQ(0, security_distributed_training_update_security_effects(bridge));
    float initial_threat = security_distributed_training_get_threat_level(bridge);
    EXPECT_LT(initial_threat, 0.3f);

    /* Quarantine multiple workers to increase threat level */
    for (int i = 0; i < 5; i++) {
        std::string worker_id = "worker_" + std::to_string(i);
        security_distributed_training_quarantine_worker(bridge, worker_id.c_str(), "test");
    }

    EXPECT_EQ(0, security_distributed_training_update_security_effects(bridge));
    float elevated_threat = security_distributed_training_get_threat_level(bridge);
    EXPECT_GT(elevated_threat, initial_threat);

    /* Check phase transition */
    security_distributed_phase_t phase = security_distributed_training_get_phase(bridge);
    EXPECT_NE(SECURITY_DISTRIBUTED_PHASE_INACTIVE, phase);
}

TEST_F(SecurityDistributedIntegrationTest, AggregationMethodAdaptation) {
    config.aggregation_method = SECURITY_GRAD_AGG_SIMPLE_AVERAGE;
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    /* Initial aggregation method */
    EXPECT_EQ(SECURITY_GRAD_AGG_SIMPLE_AVERAGE,
              security_distributed_training_get_aggregation_method(bridge));

    /* Increase Byzantine ratio */
    const int num_workers = 10;
    SetUpWorkers(num_workers);

    for (int i = 0; i < 5; i++) {
        std::string worker_id = "worker_" + std::to_string(i);
        for (int j = 0; j < 5; j++) {
            security_distributed_training_report_worker_anomaly(
                bridge, worker_id.c_str(), 0.95f, "Byzantine");
        }
    }

    security_byzantine_result_t byz_result;
    security_distributed_training_detect_byzantine(bridge, &byz_result);
    security_distributed_training_update_security_effects(bridge);

    /* Aggregation method should adapt to more Byzantine-resistant method */
    security_grad_aggregation_t adapted_method =
        security_distributed_training_get_aggregation_method(bridge);

    /* Should recommend stronger protection method */
    EXPECT_NE(SECURITY_GRAD_AGG_SIMPLE_AVERAGE, adapted_method);
}

/* =============================================================================
 * Concurrent Access Tests (Basic Thread Safety)
 * ============================================================================= */

TEST_F(SecurityDistributedIntegrationTest, ConcurrentWorkerRegistration) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const int num_threads = 4;
    const int workers_per_thread = 10;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, workers_per_thread]() {
            for (int w = 0; w < workers_per_thread; w++) {
                std::string worker_id = "worker_" + std::to_string(t) + "_" + std::to_string(w);
                security_distributed_training_register_worker(
                    bridge, worker_id.c_str(), nullptr);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_EQ(num_threads * workers_per_thread, (int)stats.total_workers_registered);
}

TEST_F(SecurityDistributedIntegrationTest, ConcurrentGradientValidation) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const int num_workers = 8;
    SetUpWorkers(num_workers);

    const size_t gradient_size = 100;
    const int validations_per_worker = 5;

    std::vector<std::thread> threads;

    for (int w = 0; w < num_workers; w++) {
        threads.emplace_back([this, w, gradient_size, validations_per_worker]() {
            std::string worker_id = "worker_" + std::to_string(w);
            for (int v = 0; v < validations_per_worker; v++) {
                auto gradients = GenerateNormalGradients(gradient_size, 0.1f);
                security_gradient_validation_result_t result;
                security_distributed_training_validate_gradients(
                    bridge, worker_id.c_str(), gradients.data(), gradients.size(), &result);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_EQ(num_workers * validations_per_worker, (int)stats.gradients_validated);
}

/* =============================================================================
 * Bio-Async Integration Tests
 * ============================================================================= */

TEST_F(SecurityDistributedIntegrationTest, BioAsyncLifecycle) {
#ifndef NIMCP_BIO_ASYNC_ENABLED
    GTEST_SKIP() << "Bio-async router not available";
#endif

    config.enable_bio_async = true;
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    /* Disconnect and reconnect */
    EXPECT_EQ(0, security_distributed_training_disconnect_bio_async(bridge));
    EXPECT_FALSE(security_distributed_training_is_bio_async_connected(bridge));

    EXPECT_EQ(0, security_distributed_training_connect_bio_async(bridge));
    /* May or may not be connected depending on router availability */
}

/* =============================================================================
 * Edge Case Tests
 * ============================================================================= */

TEST_F(SecurityDistributedIntegrationTest, EmptyWorkerSet) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    /* Byzantine detection with no workers */
    security_byzantine_result_t result;
    EXPECT_EQ(0, security_distributed_training_detect_byzantine(bridge, &result));
    EXPECT_FALSE(result.byzantine_detected);
}

TEST_F(SecurityDistributedIntegrationTest, SingleWorker) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "sole_worker", nullptr));
    EXPECT_EQ(0, security_distributed_training_set_worker_trust(
        bridge, "sole_worker", SECURITY_WORKER_TRUST_VERIFIED));

    /* Single worker can't be compared for Byzantine behavior */
    security_byzantine_result_t result;
    EXPECT_EQ(0, security_distributed_training_detect_byzantine(bridge, &result));
    EXPECT_FALSE(result.byzantine_detected);
}

TEST_F(SecurityDistributedIntegrationTest, AllWorkersQuarantined) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const int num_workers = 5;
    SetUpWorkers(num_workers);

    /* Quarantine all workers */
    for (int i = 0; i < num_workers; i++) {
        std::string worker_id = "worker_" + std::to_string(i);
        EXPECT_EQ(0, security_distributed_training_quarantine_worker(
            bridge, worker_id.c_str(), "test"));
    }

    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_EQ(0u, stats.current_active_workers);
    EXPECT_EQ(num_workers, (int)stats.current_quarantined_workers);

    /* Update effects - should have high threat level */
    EXPECT_EQ(0, security_distributed_training_update_security_effects(bridge));
}

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
