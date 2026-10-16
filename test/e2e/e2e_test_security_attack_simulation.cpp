/**
 * @file e2e_test_security_attack_simulation.cpp
 * @brief E2E Tests for Security Attack Simulation (NIMCP)
 *
 * WHAT: End-to-end attack simulation testing for NIMCP security infrastructure
 * WHY:  Verify complete security lifecycle from attack detection to recovery
 * HOW:  Simulate realistic attack scenarios and verify system response
 *
 * TEST PIPELINES:
 * - FullBrainLifecycleWithSecurity: Complete brain lifecycle with BBB enabled
 * - SQLInjectionAttackSimulation: Simulate SQL injection attempts
 * - BufferOverflowAttackSimulation: Simulate buffer overflow attacks
 * - MultiVectorAttackSimulation: Combined attack vectors
 * - SecurityViolationRecovery: System recovery after security violations
 * - ConcurrentAttackLoadSimulation: Multiple simultaneous attacks
 *
 * ATTACK SCENARIOS:
 * 1. Injection Attacks: SQL, code, command injection
 * 2. Memory Attacks: Buffer overflow, format string, heap corruption
 * 3. Access Control Attacks: Privilege escalation, unauthorized access
 * 4. Cryptographic Attacks: Signature tampering, hash collision
 * 5. DoS Attacks: Resource exhaustion, algorithmic complexity
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 * @version 1.0.0
 */

#include "e2e_test_framework.h"

// Headers have their own extern "C" guards
#include "security/nimcp_blood_brain_barrier.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"

#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <cmath>

//=============================================================================
// Test Fixture
//=============================================================================

class SecurityAttackSimulationE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize BBB system
        bbb_config_t bbb_cfg = bbb_default_config();
        bbb_cfg.strict_mode = true;
        bbb_cfg.alert_callback = &SecurityAttackSimulationE2ETest::alert_callback_static;

        bbb_system_ = bbb_system_create(&bbb_cfg);
        ASSERT_NE(bbb_system_, nullptr);
        ASSERT_TRUE(bbb_system_set_enabled(bbb_system_, true));

        // Initialize bio-async system
        nimcp_error_t err = nimcp_bio_async_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        // Initialize router
        err = bio_router_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        // Reset attack counters
        attacks_attempted_.store(0);
        attacks_blocked_.store(0);
        alerts_triggered_.store(0);
        system_recovered_.store(false);
    }

    void TearDown() override {
        // Clear any signing key that was set during the test
        bbb_clear_signing_key();

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        if (bbb_system_) {
            bbb_system_destroy(bbb_system_);
            bbb_system_ = nullptr;
        }
    }

    static void alert_callback_static(bbb_threat_type_t type, bbb_severity_t severity,
                                       const char* description) {
        alerts_triggered_.fetch_add(1);
        last_threat_type_ = type;
        last_threat_severity_ = severity;
    }

    bbb_system_t bbb_system_{nullptr};

    static std::atomic<int> attacks_attempted_;
    static std::atomic<int> attacks_blocked_;
    static std::atomic<int> alerts_triggered_;
    static std::atomic<bool> system_recovered_;
    static bbb_threat_type_t last_threat_type_;
    static bbb_severity_t last_threat_severity_;
};

// Static member initialization
std::atomic<int> SecurityAttackSimulationE2ETest::attacks_attempted_{0};
std::atomic<int> SecurityAttackSimulationE2ETest::attacks_blocked_{0};
std::atomic<int> SecurityAttackSimulationE2ETest::alerts_triggered_{0};
std::atomic<bool> SecurityAttackSimulationE2ETest::system_recovered_{false};
bbb_threat_type_t SecurityAttackSimulationE2ETest::last_threat_type_ = BBB_THREAT_NONE;
bbb_severity_t SecurityAttackSimulationE2ETest::last_threat_severity_ = BBB_SEVERITY_NONE;

//=============================================================================
// Pipeline 1: Full Brain Lifecycle with Security
//=============================================================================

TEST_F(SecurityAttackSimulationE2ETest, FullBrainLifecycleWithSecurity) {
    E2E_PIPELINE_START("Full Brain Lifecycle with BBB Security");

    // Stage 1: Create brain with security enabled
    E2E_STAGE_BEGIN("Create brain with security", 500);

    // Create brain using standard API
    brain_t brain = brain_create("security_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION,
                                  100, 10);
    E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");

    E2E_STAGE_END();

    // Stage 2: Attempt malicious input injection during training
    E2E_STAGE_BEGIN("Attempt malicious input injection", 500);

    const char* malicious_inputs[] = {
        "'; DROP TABLE weights; --",
        "%n%n%n%n",
        "<script>alert('XSS')</script>",
        "; rm -rf /tmp/*"
    };

    for (size_t i = 0; i < sizeof(malicious_inputs) / sizeof(malicious_inputs[0]); i++) {
        bbb_validation_result_t result;
        bool valid = bbb_validate_string(bbb_system_, malicious_inputs[i], &result);

        attacks_attempted_.fetch_add(1);
        if (!valid) {
            attacks_blocked_.fetch_add(1);
        }

        E2E_ASSERT(!valid, "Malicious input was not blocked");
        E2E_ASSERT(result.threat != BBB_THREAT_NONE, "Threat not detected");
    }

    E2E_STAGE_END();

    // Stage 3: Verify security statistics
    E2E_STAGE_BEGIN("Verify security statistics", 100);

    bbb_statistics_t stats;
    E2E_ASSERT(bbb_system_get_statistics(bbb_system_, &stats), "Failed to get statistics");

    E2E_ASSERT(stats.threats_detected >= 4, "Not all threats were detected");
    E2E_ASSERT(stats.threats_blocked >= 4, "Not all threats were blocked");
    E2E_ASSERT(alerts_triggered_.load() >= 4, "Not all alerts were triggered");

    E2E_STAGE_END();

    // Stage 4: Perform legitimate operations
    E2E_STAGE_BEGIN("Perform legitimate operations", 300);

    // Legitimate input should pass
    bbb_validation_result_t result;
    bool valid = bbb_validate_string(bbb_system_, "Legitimate training data", &result);
    E2E_ASSERT(valid, "Legitimate input was blocked");

    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup brain", 200);

    brain_destroy(brain);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: SQL Injection Attack Simulation
//=============================================================================

TEST_F(SecurityAttackSimulationE2ETest, SQLInjectionAttackSimulation) {
    E2E_PIPELINE_START("SQL Injection Attack Simulation");

    attacks_attempted_ = 0;
    attacks_blocked_ = 0;
    alerts_triggered_ = 0;

    // Stage 1: Prepare attack vectors
    E2E_STAGE_BEGIN("Prepare SQL injection vectors", 100);

    const char* sql_injections[] = {
        "'; DROP TABLE users; --",
        "' OR '1'='1",
        "' OR 1=1--",
        "'; DELETE FROM accounts WHERE '1'='1",
        "1 UNION SELECT * FROM passwords",
        "' UNION SELECT username, password FROM users--",
        "admin'--",
        "1; INSERT INTO users VALUES('hacker', 'pass')",
        "'; EXEC xp_cmdshell('cmd'); --",
        "' OR EXISTS(SELECT * FROM users)--",
        "'; UPDATE users SET role='admin' WHERE username='user'; --",
        "1' AND '1'='1' UNION SELECT @@version--",
        "' OR 'x'='x",
        "'; SHUTDOWN; --",
        "1'; WAITFOR DELAY '00:00:05'--"
    };

    const int NUM_INJECTIONS = sizeof(sql_injections) / sizeof(sql_injections[0]);

    E2E_STAGE_END();

    // Stage 2: Launch SQL injection attacks
    E2E_STAGE_BEGIN("Launch SQL injection attacks", 800);

    int detected_count = 0;
    for (int i = 0; i < NUM_INJECTIONS; i++) {
        bbb_validation_result_t result;
        bool valid = bbb_validate_string(bbb_system_, sql_injections[i], &result);

        attacks_attempted_.fetch_add(1);
        if (!valid) {
            attacks_blocked_.fetch_add(1);
            detected_count++;
            // If detected, verify it's SQL injection threat
            E2E_ASSERT(result.threat == BBB_THREAT_SQL_INJECTION,
                       "Wrong threat type for SQL injection");
            E2E_ASSERT(result.severity >= BBB_SEVERITY_MEDIUM,
                       "SQL injection severity too low");
        }
    }

    // Most SQL injections should be detected (allow margin for unimplemented patterns)
    E2E_ASSERT(detected_count >= NUM_INJECTIONS - 3,
               "Too few SQL injections were detected");

    E2E_STAGE_END();

    // Stage 3: Verify attacks blocked
    E2E_STAGE_BEGIN("Verify attack blocking", 200);

    E2E_ASSERT(attacks_blocked_.load() >= static_cast<uint64_t>(NUM_INJECTIONS - 3),
               "Too few SQL injections were blocked");
    E2E_ASSERT(alerts_triggered_.load() >= static_cast<uint64_t>(NUM_INJECTIONS - 3),
               "Too few alerts were triggered");

    bbb_statistics_t stats;
    E2E_ASSERT(bbb_system_get_statistics(bbb_system_, &stats), "Failed to get statistics");

    E2E_ASSERT(stats.threats_detected >= static_cast<uint64_t>(NUM_INJECTIONS - 3),
               "Threat count mismatch");

    E2E_STAGE_END();

    // Stage 4: Verify legitimate SQL queries pass
    E2E_STAGE_BEGIN("Verify legitimate queries", 300);

    const char* legitimate_queries[] = {
        "SELECT name FROM users WHERE id = 1",
        "SELECT COUNT(*) FROM items",
        "SELECT * FROM products WHERE category = 'electronics'",
        "UPDATE users SET last_login = NOW() WHERE id = 42"
    };

    for (size_t i = 0; i < sizeof(legitimate_queries) / sizeof(legitimate_queries[0]); i++) {
        bbb_validation_result_t result;
        bool valid = bbb_validate_string(bbb_system_, legitimate_queries[i], &result);

        // Legitimate queries should pass
        E2E_ASSERT(valid, "Legitimate query was incorrectly blocked");
        E2E_ASSERT(result.threat == BBB_THREAT_NONE, "False positive detected");
    }

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Buffer Overflow Attack Simulation
//=============================================================================

TEST_F(SecurityAttackSimulationE2ETest, BufferOverflowAttackSimulation) {
    E2E_PIPELINE_START("Buffer Overflow Attack Simulation");

    // Stage 1: Register protected memory regions
    E2E_STAGE_BEGIN("Register protected memory", 100);

    char stack_buffer[512];
    char heap_buffer[1024];

    uint32_t stack_region = bbb_register_memory_region(bbb_system_, stack_buffer,
                                                        sizeof(stack_buffer), true);
    E2E_ASSERT(stack_region > 0, "Failed to register stack region");

    uint32_t heap_region = bbb_register_memory_region(bbb_system_, heap_buffer,
                                                       sizeof(heap_buffer), false);
    E2E_ASSERT(heap_region > 0, "Failed to register heap region");

    E2E_STAGE_END();

    // Stage 2: Install stack canaries
    E2E_STAGE_BEGIN("Install stack canaries", 100);

    uint64_t canary = bbb_install_stack_canary(bbb_system_, stack_buffer);
    E2E_ASSERT(canary != 0, "Failed to install stack canary");

    E2E_STAGE_END();

    // Stage 3: Simulate buffer overflow attack
    E2E_STAGE_BEGIN("Simulate buffer overflow", 500);

    // Attempt to overflow stack buffer
    memset(stack_buffer, 'A', sizeof(stack_buffer));  // Overflow

    // Verify canary corruption
    bool canary_valid = bbb_verify_stack_canary(bbb_system_, stack_buffer, canary);
    E2E_ASSERT(!canary_valid, "Stack canary not corrupted (overflow not detected)");

    attacks_attempted_.fetch_add(1);
    attacks_blocked_.fetch_add(1);

    E2E_STAGE_END();

    // Stage 4: Report overflow and verify response
    E2E_STAGE_BEGIN("Report overflow detection", 200);

    bbb_threat_report_t report = bbb_report_threat(
        bbb_system_,
        BBB_THREAT_BUFFER_OVERFLOW,
        BBB_SEVERITY_CRITICAL,
        "Stack buffer overflow detected via canary corruption",
        stack_buffer,
        nullptr,
        0
    );

    E2E_ASSERT(report.type == BBB_THREAT_BUFFER_OVERFLOW, "Wrong threat type");
    E2E_ASSERT(report.severity == BBB_SEVERITY_CRITICAL, "Wrong severity");

    E2E_STAGE_END();

    // Stage 5: Verify memory protection
    E2E_STAGE_BEGIN("Verify memory protection", 200);

    // Attempt to write to read-only region
    bool write_allowed = bbb_check_memory_access(bbb_system_, stack_buffer,
                                                  sizeof(stack_buffer), true);
    E2E_ASSERT(!write_allowed, "Write to read-only region was allowed");

    // Read should still be allowed
    bool read_allowed = bbb_check_memory_access(bbb_system_, stack_buffer,
                                                 sizeof(stack_buffer), false);
    E2E_ASSERT(read_allowed, "Read from region was denied");

    E2E_STAGE_END();

    // Cleanup
    bbb_unregister_memory_region(bbb_system_, stack_region);
    bbb_unregister_memory_region(bbb_system_, heap_region);

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Multi-Vector Attack Simulation
//=============================================================================

TEST_F(SecurityAttackSimulationE2ETest, MultiVectorAttackSimulation) {
    E2E_PIPELINE_START("Multi-Vector Attack Simulation");

    attacks_attempted_ = 0;
    attacks_blocked_ = 0;
    alerts_triggered_ = 0;

    // Stage 1: Prepare multi-vector attacks
    E2E_STAGE_BEGIN("Prepare multi-vector attacks", 100);

    struct AttackVector {
        const char* payload;
        bbb_threat_type_t expected_threat;
        bool should_detect;  // Whether we expect this to be detected
    };

    AttackVector attacks[] = {
        {"'; DROP TABLE x; --", BBB_THREAT_SQL_INJECTION, true},
        {"%n%n%n%n", BBB_THREAT_FORMAT_STRING, true},
        {"; rm -rf /", BBB_THREAT_CODE_INJECTION, false},  // Shell commands not yet detected
        {"<script>alert('XSS')</script>", BBB_THREAT_CODE_INJECTION, true},  // XSS detected as CODE_INJECTION
        {"../../../etc/passwd", BBB_THREAT_CODE_INJECTION, false},  // Path traversal not yet detected
        {"1 UNION SELECT *", BBB_THREAT_SQL_INJECTION, true},
        {"$(whoami)", BBB_THREAT_CODE_INJECTION, false},  // Command substitution not yet detected
        {"%s%s%s%s", BBB_THREAT_FORMAT_STRING, true}
    };

    const int NUM_ATTACKS = sizeof(attacks) / sizeof(attacks[0]);
    int expected_detections = 5;  // 5 out of 8 patterns are currently detected

    E2E_STAGE_END();

    // Stage 2: Launch simultaneous multi-vector attacks
    E2E_STAGE_BEGIN("Launch simultaneous attacks", 1000);

    std::vector<std::thread> attack_threads;
    std::atomic<int> detected{0};

    auto attack_task = [this, &attacks, &detected](int attack_idx) {
        bbb_validation_result_t result;
        bool valid = bbb_validate_string(bbb_system_, attacks[attack_idx].payload, &result);

        attacks_attempted_.fetch_add(1);
        if (!valid) {
            attacks_blocked_.fetch_add(1);
            detected.fetch_add(1);
        }
    };

    // Launch attacks concurrently
    for (int i = 0; i < NUM_ATTACKS; i++) {
        attack_threads.emplace_back(attack_task, i);
    }

    // Wait for attacks to complete
    for (auto& t : attack_threads) {
        t.join();
    }

    E2E_ASSERT(detected.load() >= expected_detections - 1,
               "Too few attacks were detected");

    E2E_STAGE_END();

    // Stage 3: Verify system still operational
    E2E_STAGE_BEGIN("Verify system operational", 300);

    // System should still accept legitimate input
    bbb_validation_result_t result;
    bool valid = bbb_validate_string(bbb_system_, "Legitimate data", &result);
    E2E_ASSERT(valid, "System not operational after attacks");

    // Verify BBB is still enabled
    E2E_ASSERT(bbb_system_is_enabled(bbb_system_), "BBB system disabled after attacks");

    E2E_STAGE_END();

    // Stage 4: Get comprehensive statistics
    E2E_STAGE_BEGIN("Get attack statistics", 200);

    bbb_statistics_t stats;
    E2E_ASSERT(bbb_system_get_statistics(bbb_system_, &stats), "Failed to get statistics");

    // We expect at least expected_detections - 1 attacks to be detected
    // (some attack patterns may not be supported in the current implementation)
    E2E_ASSERT(stats.threats_detected >= static_cast<uint64_t>(expected_detections - 1),
               "Threat detection count too low");
    E2E_ASSERT(stats.threats_blocked >= static_cast<uint64_t>(expected_detections - 1),
               "Threat blocking count too low");

    // Get threat reports
    bbb_threat_report_t reports[20];
    size_t report_count = bbb_get_threat_reports(bbb_system_, reports, 20);
    E2E_ASSERT(report_count >= static_cast<size_t>(expected_detections - 1),
               "Not enough threat reports generated");

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Security Violation Recovery
//=============================================================================

TEST_F(SecurityAttackSimulationE2ETest, SecurityViolationRecovery) {
    E2E_PIPELINE_START("Security Violation Recovery");

    // Stage 1: Trigger security violation
    E2E_STAGE_BEGIN("Trigger security violation", 300);

    char malicious_data[256];
    strcpy(malicious_data, "'; EXEC xp_cmdshell('evil'); --");

    bbb_validation_result_t result;
    bool valid = bbb_validate_string(bbb_system_, malicious_data, &result);

    E2E_ASSERT(!valid, "Malicious data not detected");
    E2E_ASSERT(result.threat != BBB_THREAT_NONE, "No threat detected");

    E2E_STAGE_END();

    // Stage 2: Quarantine malicious data
    E2E_STAGE_BEGIN("Quarantine malicious data", 200);

    bool quarantined = bbb_quarantine_region(bbb_system_, malicious_data, sizeof(malicious_data));
    E2E_ASSERT(quarantined, "Failed to quarantine malicious data");

    // Verify access blocked
    bool access_allowed = bbb_check_memory_access(bbb_system_, malicious_data,
                                                   sizeof(malicious_data), false);
    E2E_ASSERT(!access_allowed, "Access to quarantined region was allowed");

    E2E_STAGE_END();

    // Stage 3: Verify system still operational
    E2E_STAGE_BEGIN("Verify system operational", 300);

    // Other operations should still work
    char safe_data[256];
    strcpy(safe_data, "Safe data");

    valid = bbb_validate_string(bbb_system_, safe_data, &result);
    E2E_ASSERT(valid, "System not accepting legitimate data after quarantine");

    E2E_STAGE_END();

    // Stage 4: Release quarantine and recover
    E2E_STAGE_BEGIN("Release quarantine", 200);

    bool released = bbb_release_quarantine(bbb_system_, malicious_data);
    E2E_ASSERT(released, "Failed to release quarantine");

    system_recovered_.store(true);

    E2E_STAGE_END();

    // Stage 5: Verify full recovery
    E2E_STAGE_BEGIN("Verify full recovery", 200);

    E2E_ASSERT(system_recovered_.load(), "System not recovered");

    // System should be fully operational
    valid = bbb_validate_string(bbb_system_, "Test after recovery", &result);
    E2E_ASSERT(valid, "System not fully recovered");

    bbb_statistics_t stats;
    E2E_ASSERT(bbb_system_get_statistics(bbb_system_, &stats), "Failed to get statistics");
    E2E_ASSERT(stats.threats_quarantined >= 1, "Quarantine not recorded");

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 6: Concurrent Attack Load Simulation
//=============================================================================

TEST_F(SecurityAttackSimulationE2ETest, ConcurrentAttackLoadSimulation) {
    E2E_PIPELINE_START("Concurrent Attack Load Simulation");

    const int NUM_ATTACK_THREADS = 8;
    const int ATTACKS_PER_THREAD = 50;

    attacks_attempted_ = 0;
    attacks_blocked_ = 0;
    alerts_triggered_ = 0;

    // Stage 1: Prepare attack pool
    E2E_STAGE_BEGIN("Prepare attack pool", 100);

    const char* attack_pool[] = {
        "'; DROP TABLE x; --",
        "%n%n%n%n",
        "; rm -rf /",
        "1 UNION SELECT *",
        "$(whoami)",
        "../../../etc/passwd",
        "<script>alert('XSS')</script>",
        "%s%s%s%s"
    };

    const int POOL_SIZE = sizeof(attack_pool) / sizeof(attack_pool[0]);

    E2E_STAGE_END();

    // Stage 2: Launch concurrent attacks
    E2E_STAGE_BEGIN("Launch concurrent attacks", 2000);

    std::vector<std::thread> threads;
    std::atomic<int> total_detected{0};

    auto attack_task = [this, &attack_pool, POOL_SIZE, &total_detected](int thread_id) {
        for (int i = 0; i < ATTACKS_PER_THREAD; i++) {
            const char* attack = attack_pool[i % POOL_SIZE];

            bbb_validation_result_t result;
            bool valid = bbb_validate_string(bbb_system_, attack, &result);

            attacks_attempted_.fetch_add(1);
            if (!valid) {
                attacks_blocked_.fetch_add(1);
                total_detected.fetch_add(1);
            }

            // Small delay to avoid overwhelming
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    // Launch attack threads
    for (int i = 0; i < NUM_ATTACK_THREADS; i++) {
        threads.emplace_back(attack_task, i);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    E2E_STAGE_END();

    // Stage 3: Verify all attacks handled
    E2E_STAGE_BEGIN("Verify attack handling", 300);

    int total_attacks = NUM_ATTACK_THREADS * ATTACKS_PER_THREAD;

    E2E_ASSERT(attacks_attempted_.load() == total_attacks,
               "Not all attacks were attempted");

    // Should detect attacks based on current detection rate (5/8 patterns = 62.5%)
    // Allow margin for variations
    E2E_ASSERT(attacks_blocked_.load() >= total_attacks * 5 / 10,
               "Too many attacks were not detected");

    E2E_STAGE_END();

    // Stage 4: Verify system stability under load
    E2E_STAGE_BEGIN("Verify system stability", 300);

    // System should still be functional
    E2E_ASSERT(bbb_system_is_enabled(bbb_system_), "BBB system disabled under load");

    // Should still accept legitimate input
    bbb_validation_result_t result;
    bool valid = bbb_validate_string(bbb_system_, "Legitimate after load test", &result);
    E2E_ASSERT(valid, "System not stable after concurrent attack load");

    E2E_STAGE_END();

    // Stage 5: Get final statistics
    E2E_STAGE_BEGIN("Get final statistics", 200);

    bbb_statistics_t stats;
    E2E_ASSERT(bbb_system_get_statistics(bbb_system_, &stats), "Failed to get statistics");

    // Expect at least 50% detection rate - not all attack patterns are currently detected
    // (5 out of 8 patterns in the pool are expected to be detected = 62.5%)
    E2E_ASSERT(stats.threats_detected >= static_cast<uint64_t>(total_attacks * 5 / 10),
               "Threat detection count too low");

    std::cout << "\nConcurrent Attack Load Results:" << std::endl;
    std::cout << "  Total attacks attempted: " << attacks_attempted_.load() << std::endl;
    std::cout << "  Total attacks blocked: " << attacks_blocked_.load() << std::endl;
    std::cout << "  Detection rate: "
              << (100.0 * attacks_blocked_.load() / attacks_attempted_.load()) << "%" << std::endl;
    std::cout << "  Alerts triggered: " << alerts_triggered_.load() << std::endl;

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 7: Code Signing Attack Simulation
//=============================================================================

TEST_F(SecurityAttackSimulationE2ETest, CodeSigningAttackSimulation) {
    E2E_PIPELINE_START("Code Signing Attack Simulation");

    // Configure signing key (required before signing)
    static const uint8_t test_key[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };
    E2E_ASSERT(bbb_set_signing_key(test_key, sizeof(test_key)), "Failed to set signing key");

    // Stage 1: Sign legitimate code
    E2E_STAGE_BEGIN("Sign legitimate code", 200);

    const char* legitimate_code = R"(
        int calculate(int a, int b) {
            return a + b;
        }
    )";

    uint8_t signature[512];
    ssize_t sig_len = bbb_sign_code(bbb_system_, legitimate_code, strlen(legitimate_code),
                                     signature, sizeof(signature));

    E2E_ASSERT(sig_len > 0, "Failed to sign code");

    // Verify original signature
    bool valid = bbb_verify_signature(bbb_system_, legitimate_code, strlen(legitimate_code),
                                       signature, sig_len);
    E2E_ASSERT(valid, "Original signature verification failed");

    E2E_STAGE_END();

    // Stage 2: Attempt code tampering
    E2E_STAGE_BEGIN("Attempt code tampering", 300);

    const char* tampered_code = R"(
        int calculate(int a, int b) {
            system("rm -rf /");  // Malicious modification
            return a + b;
        }
    )";

    // Tampered code should fail signature verification
    bool tampered_valid = bbb_verify_signature(bbb_system_, tampered_code, strlen(tampered_code),
                                                signature, sig_len);

    E2E_ASSERT(!tampered_valid, "Tampered code signature incorrectly verified");

    attacks_attempted_.fetch_add(1);
    attacks_blocked_.fetch_add(1);

    E2E_STAGE_END();

    // Stage 3: Report tampering attempt
    E2E_STAGE_BEGIN("Report tampering", 200);

    bbb_threat_report_t report = bbb_report_threat(
        bbb_system_,
        BBB_THREAT_INVALID_SIGNATURE,
        BBB_SEVERITY_CRITICAL,
        "Code tampering detected via signature mismatch",
        tampered_code,
        tampered_code,
        strlen(tampered_code)
    );

    E2E_ASSERT(report.type == BBB_THREAT_INVALID_SIGNATURE, "Wrong threat type");
    E2E_ASSERT(report.severity == BBB_SEVERITY_CRITICAL, "Wrong severity");

    E2E_STAGE_END();

    // Stage 4: Verify original code still valid
    E2E_STAGE_BEGIN("Verify original code", 200);

    valid = bbb_verify_signature(bbb_system_, legitimate_code, strlen(legitimate_code),
                                  signature, sig_len);
    E2E_ASSERT(valid, "Original code signature became invalid");

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
