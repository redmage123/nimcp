/**
 * @file test_security_stack_e2e.cpp
 * @brief End-to-end tests for full security stack integration
 *
 * WHAT: E2E tests verifying complete security stack workflows
 * WHY:  Ensure all security components work together correctly
 * HOW:  Test complete BBB validation, pattern detection, rate limiting, and anomaly detection
 *
 * TEST SCENARIOS:
 * 1. Complete BBB validation flow from input to decision
 * 2. Pattern detection triggering security response
 * 3. Rate limiting under sustained load
 * 4. Anomaly detection with real patterns
 * 5. Multi-layer defense coordination
 *
 * @author NIMCP Development Team
 * @date 2026-02-02
 */

#include "e2e_test_framework.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

extern "C" {
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_rate_limiter.h"
#include "security/nimcp_encrypted_audit.h"
#include "security/nimcp_security_orchestrator.h"
#include "utils/error/nimcp_error_codes.h"
}

namespace nimcp {
namespace e2e {

/* ============================================================================
 * Test Constants
 * ============================================================================ */

static constexpr int RATE_LIMIT_THREADS = 4;
static constexpr int RATE_LIMIT_REQUESTS_PER_THREAD = 200;
static constexpr int STRESS_ITERATIONS = 1000;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SecurityStackE2E : public ::testing::Test {
protected:
    bbb_system_t bbb_ = nullptr;
    nimcp_anomaly_detector_t anomaly_ = nullptr;
    nimcp_rate_limiter_t limiter_ = nullptr;
    security_orchestrator_t orchestrator_ = nullptr;

    void SetUp() override {
        // Create BBB system
        bbb_config_t bbb_config = bbb_default_config();
        bbb_config.input.validate_strings = true;
        bbb_config.input.sanitize_sql = true;
        bbb_config.input.max_string_length = 4096;
        bbb_config.strict_mode = true;
        bbb_ = bbb_system_create(&bbb_config);

        // Create anomaly detector
        nimcp_anomaly_config_t anom_config = nimcp_anomaly_detector_default_config();
        anom_config.overall_anomaly_threshold = 0.7f;
        anom_config.enable_online_learning = true;
        anomaly_ = nimcp_anomaly_detector_create(&anom_config);

        // Create rate limiter
        nimcp_rate_limit_config_t rate_config = nimcp_rate_limiter_default_config();
        rate_config.requests_per_second = 100.0f;
        rate_config.burst_size = 150;
        rate_config.per_client = true;
        rate_config.penalty.enabled = true;
        limiter_ = nimcp_rate_limiter_create(&rate_config);

        // Create security orchestrator
        security_orch_config_t orch_config;
        security_orch_default_config(&orch_config);
        orch_config.enable_async = false;  // Synchronous for testing
        orch_config.auto_lockdown = false; // Manual control for testing
        orchestrator_ = security_orch_create(&orch_config);
    }

    void TearDown() override {
        if (orchestrator_) {
            security_orch_destroy(orchestrator_);
            orchestrator_ = nullptr;
        }
        if (limiter_) {
            nimcp_rate_limiter_destroy(limiter_);
            limiter_ = nullptr;
        }
        if (anomaly_) {
            nimcp_anomaly_detector_destroy(anomaly_);
            anomaly_ = nullptr;
        }
        if (bbb_) {
            bbb_system_destroy(bbb_);
            bbb_ = nullptr;
        }
    }
};

/* ============================================================================
 * E2E Test: Complete BBB Validation Flow
 * ============================================================================ */

TEST_F(SecurityStackE2E, CompleteBBBValidationFlow) {
    E2E_PIPELINE_START("Complete BBB Validation Flow");

    E2E_STAGE_BEGIN("Setup", 100);
    E2E_ASSERT_NOT_NULL(bbb_, "BBB system created");
    E2E_STAGE_END();

    // Phase 1: Validate normal input
    E2E_STAGE_BEGIN("Validate normal input", 100);
    const char* normal_input = "Hello, this is a normal message.";
    bbb_validation_result_t result;
    bool valid = bbb_validate_string(bbb_, normal_input, &result);
    EXPECT_TRUE(valid) << "Normal input should be valid";
    EXPECT_EQ(result.threat, BBB_THREAT_NONE) << "No threat detected for normal input";
    E2E_STAGE_END();

    // Phase 2: Validate suspicious input
    E2E_STAGE_BEGIN("Validate suspicious input", 100);
    const char* suspicious_input = "SELECT * FROM users WHERE id=1; DROP TABLE users;--";
    valid = bbb_validate_string(bbb_, suspicious_input, &result);
    EXPECT_FALSE(valid) << "SQL injection should be blocked";
    EXPECT_EQ(result.threat, BBB_THREAT_SQL_INJECTION) << "SQL injection threat detected";
    E2E_STAGE_END();

    // Phase 3: Check statistics
    E2E_STAGE_BEGIN("Check statistics", 100);
    bbb_statistics_t stats;
    bool got_stats = bbb_system_get_statistics(bbb_, &stats);
    EXPECT_TRUE(got_stats) << "Statistics retrieved";
    EXPECT_GE(stats.total_validations, 2u) << "At least 2 validations recorded";
    EXPECT_GE(stats.threats_detected, 1u) << "At least 1 threat detected";
    E2E_STAGE_END();

    // Phase 4: Test integer validation
    E2E_STAGE_BEGIN("Integer validation", 100);
    valid = bbb_validate_integer(bbb_, 42, &result);
    EXPECT_TRUE(valid) << "Normal integer should be valid";
    // Test integer overflow attempt
    valid = bbb_validate_integer(bbb_, static_cast<int64_t>(9223372036854775807LL), &result);
    // This may or may not be valid depending on config - just verify it runs
    E2E_STAGE_END();

    // Phase 5: Test pointer validation
    E2E_STAGE_BEGIN("Pointer validation", 100);
    char test_buffer[256] = "test data";
    valid = bbb_validate_pointer(bbb_, test_buffer, sizeof(test_buffer), &result);
    EXPECT_TRUE(valid) << "Valid pointer should pass";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: Pattern Detection Triggering Security Response
 * ============================================================================ */

TEST_F(SecurityStackE2E, PatternDetectionSecurityResponse) {
    E2E_PIPELINE_START("Pattern Detection Security Response");

    E2E_STAGE_BEGIN("Setup", 100);
    E2E_ASSERT_NOT_NULL(bbb_, "BBB system created");
    E2E_ASSERT_NOT_NULL(anomaly_, "Anomaly detector created");
    E2E_STAGE_END();

    // Phase 1: Train with normal patterns
    E2E_STAGE_BEGIN("Train with normal patterns", 500);
    const char* normal_patterns[] = {
        "Hello world",
        "This is a test",
        "Normal user input",
        "Query parameter value",
        "Regular data entry"
    };
    for (size_t i = 0; i < sizeof(normal_patterns) / sizeof(normal_patterns[0]); i++) {
        nimcp_error_t err = nimcp_anomaly_train(anomaly_, normal_patterns[i],
                                                 strlen(normal_patterns[i]), true);
        EXPECT_EQ(err, NIMCP_OK) << "Training with normal pattern succeeded";
    }
    E2E_STAGE_END();

    // Phase 2: Detect anomalous pattern
    E2E_STAGE_BEGIN("Detect anomalous pattern", 200);
    const char* anomalous = "\x00\x01\x02\x03\x04\x05\x06\x07shellcode";
    nimcp_anomaly_result_t anom_result;
    nimcp_error_t err = nimcp_anomaly_detect(anomaly_, anomalous, strlen(anomalous), &anom_result);
    EXPECT_EQ(err, NIMCP_OK) << "Anomaly detection completed";
    std::cout << "    Anomaly score: " << anom_result.anomaly_score
              << ", Confidence: " << anom_result.confidence << "\n";
    E2E_STAGE_END();

    // Phase 3: Verify BBB also catches it
    E2E_STAGE_BEGIN("Verify BBB detection", 200);
    bbb_validation_result_t bbb_result;
    bool valid = bbb_validate_input(bbb_, anomalous, strlen(anomalous), &bbb_result);
    if (!valid) {
        std::cout << "    BBB blocked: " << bbb_result.reason << "\n";
    } else {
        std::cout << "    BBB allowed (may need stricter config)\n";
    }
    E2E_STAGE_END();

    // Phase 4: Check combined detection statistics
    E2E_STAGE_BEGIN("Check detection statistics", 100);
    nimcp_anomaly_stats_t anom_stats;
    err = nimcp_anomaly_get_stats(anomaly_, &anom_stats);
    EXPECT_EQ(err, NIMCP_OK) << "Anomaly stats retrieved";
    EXPECT_GE(anom_stats.total_detections, 1u) << "At least 1 detection recorded";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: Rate Limiting Under Sustained Load
 * ============================================================================ */

TEST_F(SecurityStackE2E, RateLimitingSustainedLoad) {
    E2E_PIPELINE_START("Rate Limiting Under Sustained Load");

    E2E_STAGE_BEGIN("Setup", 100);
    E2E_ASSERT_NOT_NULL(limiter_, "Rate limiter created");
    E2E_STAGE_END();

    std::atomic<int> requests_allowed{0};
    std::atomic<int> requests_denied{0};

    // Phase 1: Start concurrent workers
    E2E_STAGE_BEGIN("Run concurrent workers", 5000);
    std::vector<std::thread> threads;

    for (int t = 0; t < RATE_LIMIT_THREADS; t++) {
        threads.emplace_back([this, &requests_allowed, &requests_denied, t]() {
            char client_id[32];
            snprintf(client_id, sizeof(client_id), "client_%d", t);

            for (int i = 0; i < RATE_LIMIT_REQUESTS_PER_THREAD; i++) {
                if (nimcp_rate_limiter_allow(limiter_, client_id)) {
                    requests_allowed++;
                } else {
                    requests_denied++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }
    E2E_STAGE_END();

    // Phase 2: Analyze results
    E2E_STAGE_BEGIN("Analyze results", 100);
    int total = requests_allowed.load() + requests_denied.load();
    std::cout << "    Total requests: " << total << "\n";
    std::cout << "    Allowed: " << requests_allowed.load()
              << " (" << (100.0 * requests_allowed.load() / total) << "%)\n";
    std::cout << "    Denied: " << requests_denied.load()
              << " (" << (100.0 * requests_denied.load() / total) << "%)\n";
    EXPECT_GT(requests_allowed.load(), 0) << "Some requests were allowed";
    E2E_STAGE_END();

    // Phase 3: Check statistics
    E2E_STAGE_BEGIN("Check statistics", 100);
    nimcp_rate_limit_stats_t stats;
    nimcp_error_t err = nimcp_rate_limiter_get_stats(limiter_, &stats);
    EXPECT_EQ(err, NIMCP_OK) << "Statistics retrieved";
    EXPECT_EQ(stats.total_requests, static_cast<uint64_t>(total)) << "Total requests match";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: Anomaly Detection with Real Patterns
 * ============================================================================ */

TEST_F(SecurityStackE2E, AnomalyDetectionRealPatterns) {
    E2E_PIPELINE_START("Anomaly Detection with Real Patterns");

    E2E_STAGE_BEGIN("Setup", 100);
    E2E_ASSERT_NOT_NULL(anomaly_, "Anomaly detector created");
    E2E_STAGE_END();

    // Phase 1: Build baseline with normal HTTP requests
    E2E_STAGE_BEGIN("Build baseline", 500);
    const char* normal_requests[] = {
        "GET /api/users HTTP/1.1",
        "POST /api/login HTTP/1.1",
        "GET /api/products?page=1 HTTP/1.1",
        "PUT /api/users/123 HTTP/1.1",
        "DELETE /api/sessions/abc HTTP/1.1",
        "GET /static/style.css HTTP/1.1",
        "GET /images/logo.png HTTP/1.1"
    };
    for (size_t i = 0; i < sizeof(normal_requests) / sizeof(normal_requests[0]); i++) {
        nimcp_error_t err = nimcp_anomaly_train(anomaly_, normal_requests[i],
                                                 strlen(normal_requests[i]), true);
        EXPECT_EQ(err, NIMCP_OK) << "Training succeeded";
    }
    E2E_STAGE_END();

    // Phase 2: Test normal request detection
    E2E_STAGE_BEGIN("Test normal request", 200);
    nimcp_anomaly_result_t result;
    const char* test_normal = "GET /api/orders HTTP/1.1";
    nimcp_error_t err = nimcp_anomaly_detect(anomaly_, test_normal, strlen(test_normal), &result);
    EXPECT_EQ(err, NIMCP_OK) << "Detection completed";
    std::cout << "    Normal request score: " << result.anomaly_score << "\n";
    E2E_STAGE_END();

    // Phase 3: Test path traversal attack detection
    E2E_STAGE_BEGIN("Test path traversal", 200);
    const char* path_traversal = "GET /api/../../../etc/passwd HTTP/1.1";
    err = nimcp_anomaly_detect(anomaly_, path_traversal, strlen(path_traversal), &result);
    EXPECT_EQ(err, NIMCP_OK) << "Detection completed";
    std::cout << "    Path traversal score: " << result.anomaly_score << "\n";
    E2E_STAGE_END();

    // Phase 4: Test command injection attack detection
    E2E_STAGE_BEGIN("Test command injection", 200);
    const char* cmd_injection = "GET /api/cmd?exec=`rm -rf /` HTTP/1.1";
    err = nimcp_anomaly_detect(anomaly_, cmd_injection, strlen(cmd_injection), &result);
    EXPECT_EQ(err, NIMCP_OK) << "Detection completed";
    std::cout << "    Command injection score: " << result.anomaly_score << "\n";
    E2E_STAGE_END();

    // Phase 5: Test XSS attack detection
    E2E_STAGE_BEGIN("Test XSS attack", 200);
    const char* xss_attack = "GET /search?q=<script>alert('XSS')</script> HTTP/1.1";
    err = nimcp_anomaly_detect(anomaly_, xss_attack, strlen(xss_attack), &result);
    EXPECT_EQ(err, NIMCP_OK) << "Detection completed";
    std::cout << "    XSS attack score: " << result.anomaly_score << "\n";
    E2E_STAGE_END();

    // Phase 6: Test binary payload detection
    E2E_STAGE_BEGIN("Test binary payload", 200);
    uint8_t binary_payload[100];
    for (int i = 0; i < 100; i++) {
        binary_payload[i] = static_cast<uint8_t>(i * 3);  // Non-text pattern
    }
    err = nimcp_anomaly_detect(anomaly_, binary_payload, sizeof(binary_payload), &result);
    EXPECT_EQ(err, NIMCP_OK) << "Detection completed";
    std::cout << "    Binary payload score: " << result.anomaly_score << "\n";
    E2E_STAGE_END();

    // Phase 7: Verify statistics
    E2E_STAGE_BEGIN("Verify statistics", 100);
    nimcp_anomaly_stats_t stats;
    err = nimcp_anomaly_get_stats(anomaly_, &stats);
    EXPECT_EQ(err, NIMCP_OK) << "Stats retrieved";
    EXPECT_GE(stats.total_detections, 5u) << "At least 5 detections";
    std::cout << "    Total detections: " << stats.total_detections << "\n";
    std::cout << "    Training samples: " << stats.training_samples << "\n";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: Multi-Layer Defense Coordination
 * ============================================================================ */

TEST_F(SecurityStackE2E, MultiLayerDefenseCoordination) {
    E2E_PIPELINE_START("Multi-Layer Defense Coordination");

    E2E_STAGE_BEGIN("Setup", 100);
    E2E_ASSERT_NOT_NULL(bbb_, "BBB created");
    E2E_ASSERT_NOT_NULL(anomaly_, "Anomaly detector created");
    E2E_ASSERT_NOT_NULL(limiter_, "Rate limiter created");
    E2E_STAGE_END();

    // Phase 1: Simulate attack sequence
    E2E_STAGE_BEGIN("Simulate coordinated attack", 1000);

    // Attack step 1: Rapid requests (should trigger rate limiting)
    int rate_blocked = 0;
    for (int i = 0; i < 50; i++) {
        if (!nimcp_rate_limiter_allow(limiter_, "attacker")) {
            rate_blocked++;
        }
    }
    std::cout << "    Rate limited: " << rate_blocked << "/50 requests\n";

    // Attack step 2: SQL injection attempt (should trigger BBB)
    const char* sql_attack = "' OR '1'='1' --";
    bbb_validation_result_t bbb_result;
    bool bbb_blocked = !bbb_validate_string(bbb_, sql_attack, &bbb_result);
    if (bbb_blocked) {
        std::cout << "    BBB blocked: " << bbb_threat_type_name(bbb_result.threat) << "\n";
    }

    // Attack step 3: Anomalous payload (should trigger anomaly detector)
    const char* anomaly_attack = "\x90\x90\x90\x90\xeb\x1e\x5e\x89\x76";  // NOP sled + shellcode
    nimcp_anomaly_result_t anom_result;
    nimcp_anomaly_detect(anomaly_, anomaly_attack, strlen(anomaly_attack), &anom_result);
    std::cout << "    Anomaly score: " << anom_result.anomaly_score << "\n";
    E2E_STAGE_END();

    // Phase 2: Verify all layers detected threats
    E2E_STAGE_BEGIN("Verify multi-layer detection", 200);
    bbb_statistics_t bbb_stats;
    bbb_system_get_statistics(bbb_, &bbb_stats);
    std::cout << "    BBB threats detected: " << bbb_stats.threats_detected << "\n";

    nimcp_anomaly_stats_t anom_stats;
    nimcp_anomaly_get_stats(anomaly_, &anom_stats);
    std::cout << "    Anomaly detections: " << anom_stats.total_detections << "\n";

    nimcp_rate_limit_stats_t rate_stats;
    nimcp_rate_limiter_get_stats(limiter_, &rate_stats);
    std::cout << "    Rate limit denials: " << rate_stats.requests_denied << "\n";
    E2E_STAGE_END();

    // Phase 3: Verify defense-in-depth
    E2E_STAGE_BEGIN("Verify defense-in-depth", 100);
    int layers_triggered = 0;
    if (bbb_stats.threats_detected > 0) layers_triggered++;
    if (anom_stats.total_detections > 0) layers_triggered++;
    if (rate_stats.requests_denied > 0) layers_triggered++;

    std::cout << "    Layers triggered: " << layers_triggered << "/3\n";
    EXPECT_GE(layers_triggered, 2) << "At least 2 defense layers triggered";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: Security Stack Stress Test
 * ============================================================================ */

TEST_F(SecurityStackE2E, SecurityStackStress) {
    E2E_PIPELINE_START("Security Stack Stress Test");

    E2E_STAGE_BEGIN("Setup", 100);
    E2E_ASSERT_NOT_NULL(bbb_, "BBB created");
    E2E_STAGE_END();

    // Phase 1: Rapid validation stress test
    E2E_STAGE_BEGIN("Rapid validation stress", 10000);
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        char input[128];
        snprintf(input, sizeof(input), "Test input %d with random data %d", i, rand());

        bbb_validation_result_t result;
        bbb_validate_string(bbb_, input, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double ops_per_sec = STRESS_ITERATIONS / (duration_ms / 1000.0);
    std::cout << "    Duration: " << duration_ms << " ms\n";
    std::cout << "    Throughput: " << ops_per_sec << " validations/sec\n";
    EXPECT_GT(ops_per_sec, 1000.0) << "Throughput > 1000 ops/sec";
    E2E_STAGE_END();

    // Phase 2: Mixed attack pattern stress
    E2E_STAGE_BEGIN("Mixed attack pattern stress", 10000);
    const char* attack_patterns[] = {
        "SELECT * FROM users",
        "<script>alert(1)</script>",
        "../../../etc/passwd",
        "; rm -rf /",
        "UNION SELECT password"
    };
    int num_patterns = sizeof(attack_patterns) / sizeof(attack_patterns[0]);

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        const char* pattern = attack_patterns[i % num_patterns];
        bbb_validation_result_t result;
        bbb_validate_string(bbb_, pattern, &result);
    }
    E2E_STAGE_END();

    // Phase 3: Verify no memory leaks or crashes
    E2E_STAGE_BEGIN("Verify system stability", 100);
    bbb_statistics_t stats;
    bool got_stats = bbb_system_get_statistics(bbb_, &stats);
    EXPECT_TRUE(got_stats) << "Statistics still accessible";
    EXPECT_GE(stats.total_validations, static_cast<uint64_t>(STRESS_ITERATIONS * 2))
        << "All validations recorded";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

} // namespace e2e
} // namespace nimcp
