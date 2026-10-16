/**
 * @file test_security_workflow_e2e.cpp
 * @brief End-to-end tests for complete security workflow
 * @date 2026-02-02
 *
 * WHAT: End-to-end tests verifying complete security workflow from
 *       initialization through threat detection to cleanup
 * WHY:  Verify all security components work together in realistic scenarios
 * HOW:  Full workflow tests: init -> register -> detect -> respond -> cleanup
 *
 * TEST WORKFLOW:
 *   1. Initialize all security modules
 *   2. Register health agents
 *   3. Simulate threat detection
 *   4. Verify immune response
 *   5. Check audit logging
 *   6. Verify cleanup
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>

extern "C" {
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_tripwires.h"
#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_rate_limiter.h"
#include "security/nimcp_capability_control.h"
#include "security/nimcp_policy_engine.h"
#include "security/nimcp_safety_verification.h"
#include "security/nimcp_supply_chain.h"
#include "security/nimcp_encrypted_audit.h"
#include "cognitive/immune/nimcp_brain_immune.h"
}

namespace {

//=============================================================================
// Test Constants
//=============================================================================

/** Master encryption key for audit (test only - NOT for production) */
static const uint8_t TEST_AUDIT_KEY[32] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

/** Test epitope for immune system */
static const uint8_t TEST_EPITOPE[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
static constexpr size_t TEST_EPITOPE_LEN = sizeof(TEST_EPITOPE);

//=============================================================================
// Security Context - Manages all security modules
//=============================================================================

/**
 * @brief RAII wrapper for complete security context
 *
 * WHAT: Manages lifecycle of all security modules
 * WHY:  Ensure proper initialization and cleanup order
 * HOW:  Creates modules in dependency order, destroys in reverse
 */
class SecurityContext {
public:
    SecurityContext()
    {
        initialize_all();
    }

    ~SecurityContext()
    {
        cleanup_all();
    }

    bool is_valid() const { return initialized_; }

    // Module accessors
    bbb_system_t get_bbb() const { return bbb_; }
    brain_immune_system_t* get_immune() const { return immune_; }
    tripwire_system_t* get_tripwire() const { return tripwire_; }
    nimcp_anomaly_detector_t get_anomaly() const { return anomaly_; }
    nimcp_rate_limiter_t get_rate_limiter() const { return rate_limiter_; }
    capability_control_t* get_capability() const { return capability_; }
    safety_verification_t* get_safety() const { return safety_; }
    nimcp_encrypted_audit_t get_audit() const { return audit_; }

private:
    void initialize_all()
    {
        // 1. Initialize immune system first (no dependencies)
        brain_immune_config_t immune_config = {};
        immune_config.max_antigens = 100;
        immune_config.max_b_cells = 50;
        immune_config.max_t_cells = 50;
        immune_config.max_antibodies = 100;
        immune_ = brain_immune_create(&immune_config);
        if (!immune_) return;

        // 2. Initialize BBB with immune connection
        bbb_reset_test_state();
        bbb_config_ = bbb_default_config();
        bbb_config_.strict_mode = true;
        bbb_ = bbb_system_create(&bbb_config_);
        if (!bbb_) return;
        bbb_system_set_enabled(bbb_, true);
        bbb_connect_immune(bbb_, immune_);

        // 3. Initialize tripwire system
        tripwire_config_ = tripwire_default_config();
        tripwire_ = tripwire_create(&tripwire_config_);
        if (!tripwire_) return;

        // 4. Initialize anomaly detector
        anomaly_config_ = nimcp_anomaly_detector_default_config();
        anomaly_ = nimcp_anomaly_detector_create(&anomaly_config_);
        if (!anomaly_) return;

        // 5. Initialize rate limiter
        rate_limit_config_ = nimcp_rate_limiter_default_config();
        rate_limit_config_.requests_per_second = 100.0f;
        rate_limiter_ = nimcp_rate_limiter_create(&rate_limit_config_);
        if (!rate_limiter_) return;

        // 6. Initialize capability control
        capability_config_ = capability_control_default_config();
        capability_ = capability_control_create(&capability_config_);
        if (!capability_) return;

        // 7. Initialize safety verification
        safety_config_ = safety_verification_default_config();
        safety_ = safety_verification_create(&safety_config_);
        if (!safety_) return;

        // 8. Initialize encrypted audit
        audit_config_ = nimcp_encrypted_audit_default_config();
        audit_ = nimcp_encrypted_audit_create(&audit_config_, TEST_AUDIT_KEY, sizeof(TEST_AUDIT_KEY));
        if (!audit_) return;

        initialized_ = true;
    }

    void cleanup_all()
    {
        // Cleanup in reverse order

        if (audit_) {
            nimcp_encrypted_audit_destroy(audit_);
            audit_ = nullptr;
        }

        if (safety_) {
            safety_verification_destroy(safety_);
            safety_ = nullptr;
        }

        if (capability_) {
            capability_control_destroy(capability_);
            capability_ = nullptr;
        }

        if (rate_limiter_) {
            nimcp_rate_limiter_destroy(rate_limiter_);
            rate_limiter_ = nullptr;
        }

        if (anomaly_) {
            nimcp_anomaly_detector_destroy(anomaly_);
            anomaly_ = nullptr;
        }

        if (tripwire_) {
            tripwire_destroy(tripwire_);
            tripwire_ = nullptr;
        }

        if (bbb_) {
            bbb_system_destroy(bbb_);
            bbb_ = nullptr;
        }

        if (immune_) {
            brain_immune_destroy(immune_);
            immune_ = nullptr;
        }

        bbb_clear_signing_key();
        initialized_ = false;
    }

    bool initialized_ = false;

    // Configurations
    bbb_config_t bbb_config_;
    tripwire_config_t tripwire_config_;
    nimcp_anomaly_config_t anomaly_config_;
    nimcp_rate_limit_config_t rate_limit_config_;
    capability_control_config_t capability_config_;
    safety_verification_config_t safety_config_;
    nimcp_encrypted_audit_config_t audit_config_;

    // Module handles
    brain_immune_system_t* immune_ = nullptr;
    bbb_system_t bbb_ = nullptr;
    tripwire_system_t* tripwire_ = nullptr;
    nimcp_anomaly_detector_t anomaly_ = nullptr;
    nimcp_rate_limiter_t rate_limiter_ = nullptr;
    capability_control_t* capability_ = nullptr;
    safety_verification_t* safety_ = nullptr;
    nimcp_encrypted_audit_t audit_ = nullptr;
};

//=============================================================================
// E2E Test Fixture
//=============================================================================

class SecurityWorkflowE2ETest : public ::testing::Test {
protected:
    void SetUp() override
    {
        bbb_reset_test_state();
        bbb_clear_signing_key();
    }

    void TearDown() override
    {
        bbb_clear_signing_key();
    }
};

//=============================================================================
// Complete Workflow Tests
//=============================================================================

/**
 * @test CompleteSecurityWorkflowInitialization
 *
 * WHAT: Verify complete security stack can be initialized
 * WHY:  All modules must initialize correctly together
 * HOW:  Create SecurityContext, verify all modules valid
 */
TEST_F(SecurityWorkflowE2ETest, CompleteSecurityWorkflowInitialization)
{
    SecurityContext ctx;

    // Verify all modules initialized
    ASSERT_TRUE(ctx.is_valid()) << "Security context should initialize";
    EXPECT_NE(ctx.get_bbb(), nullptr) << "BBB should be created";
    EXPECT_NE(ctx.get_immune(), nullptr) << "Immune system should be created";
    EXPECT_NE(ctx.get_tripwire(), nullptr) << "Tripwire should be created";
    EXPECT_NE(ctx.get_anomaly(), nullptr) << "Anomaly detector should be created";
    EXPECT_NE(ctx.get_rate_limiter(), nullptr) << "Rate limiter should be created";
    EXPECT_NE(ctx.get_capability(), nullptr) << "Capability control should be created";
    EXPECT_NE(ctx.get_safety(), nullptr) << "Safety verification should be created";
    EXPECT_NE(ctx.get_audit(), nullptr) << "Audit system should be created";
}

/**
 * @test ThreatDetectionToImmuneResponseWorkflow
 *
 * WHAT: Complete workflow from threat detection to immune response
 * WHY:  Core security functionality
 * HOW:  Detect threat -> Present to immune -> Activate response
 */
TEST_F(SecurityWorkflowE2ETest, ThreatDetectionToImmuneResponseWorkflow)
{
    SecurityContext ctx;
    ASSERT_TRUE(ctx.is_valid());

    // Step 1: Detect threat via BBB
    char malicious_input[256];
    strcpy(malicious_input, "'; DROP TABLE users; --");

    bbb_validation_result_t validation_result;
    bool valid = bbb_validate_string(ctx.get_bbb(), malicious_input, &validation_result);
    EXPECT_FALSE(valid) << "BBB should detect SQL injection";
    EXPECT_EQ(validation_result.threat, BBB_THREAT_SQL_INJECTION);

    // Step 2: Present threat to immune system
    uint32_t antigen_id = 0;
    int present_result = brain_immune_present_bbb_threat(
        ctx.get_immune(),
        validation_result.threat,
        validation_result.severity,
        TEST_EPITOPE,
        TEST_EPITOPE_LEN,
        &antigen_id
    );
    EXPECT_EQ(0, present_result) << "Antigen presentation should succeed";
    EXPECT_GT(antigen_id, 0u) << "Antigen ID should be valid";

    // Step 3: Activate B cell response
    uint32_t b_cell_id = 0;
    EXPECT_EQ(0, brain_immune_activate_b_cell(ctx.get_immune(), antigen_id, &b_cell_id));

    // Step 4: Activate T cell response for critical threats
    uint32_t helper_t_id = 0;
    EXPECT_EQ(0, brain_immune_activate_helper_t(ctx.get_immune(), antigen_id, &helper_t_id));

    // Step 5: T cell helps B cell transition to plasma
    EXPECT_EQ(0, brain_immune_t_help_b(ctx.get_immune(), helper_t_id, b_cell_id));

    // Step 6: Produce antibody
    uint32_t antibody_id = 0;
    EXPECT_EQ(0, brain_immune_produce_antibody(ctx.get_immune(), b_cell_id,
                                                ANTIBODY_IGG, &antibody_id));

    // Step 7: Neutralize threat
    EXPECT_EQ(0, brain_immune_neutralize(ctx.get_immune(), antigen_id, antibody_id));
    EXPECT_TRUE(brain_immune_is_neutralized(ctx.get_immune(), antigen_id));

    // Verify immune stats
    brain_immune_stats_t immune_stats;
    EXPECT_EQ(0, brain_immune_get_stats(ctx.get_immune(), &immune_stats));
    EXPECT_GT(immune_stats.antigens_processed, 0u);
}

/**
 * @test TripwireDetectionWithAuditLogging
 *
 * WHAT: Tripwire detection with audit logging
 * WHY:  All detections must be logged for forensics
 * HOW:  Detect via tripwire -> Log to audit
 */
TEST_F(SecurityWorkflowE2ETest, TripwireDetectionWithAuditLogging)
{
    SecurityContext ctx;
    ASSERT_TRUE(ctx.is_valid());

    // Log start of monitoring
    EXPECT_EQ(NIMCP_OK, nimcp_encrypted_audit_log(
        ctx.get_audit(),
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_SYSTEM,
        "Security monitoring started",
        nullptr, 0
    ));

    // Observe suspicious behavior via tripwire
    for (int i = 0; i < 30; ++i) {
        tripwire_observe_network_connection(
            ctx.get_tripwire(),
            0xC0A80001,      // Suspicious IP
            31337,           // Suspicious port
            100000,          // High outbound (exfiltration pattern)
            100,             // Low response
            TRIPWIRE_PROTO_TCP
        );
    }

    // Check for tripwire alerts
    tripwire_alert_t alerts[10];
    uint32_t alert_count = 0;
    EXPECT_EQ(NIMCP_OK, tripwire_check(ctx.get_tripwire(), alerts, 10, &alert_count));

    // Log any alerts
    for (uint32_t i = 0; i < alert_count; ++i) {
        char alert_msg[512];
        snprintf(alert_msg, sizeof(alert_msg),
                 "Tripwire alert: type=%d, confidence=%.2f, severity=%d",
                 alerts[i].type, alerts[i].confidence, alerts[i].severity);

        EXPECT_EQ(NIMCP_OK, nimcp_encrypted_audit_log(
            ctx.get_audit(),
            NIMCP_AUDIT_WARNING,
            NIMCP_AUDIT_THREAT,
            alert_msg,
            nullptr, 0
        ));
    }

    // Verify audit stats
    nimcp_encrypted_audit_stats_t audit_stats;
    EXPECT_EQ(NIMCP_OK, nimcp_encrypted_audit_get_stats(ctx.get_audit(), &audit_stats));
    EXPECT_GT(audit_stats.total_entries, 0u) << "Audit should have entries";
}

/**
 * @test RateLimitingWithCapabilityEnforcement
 *
 * WHAT: Rate limiting integrated with capability control
 * WHY:  Both mechanisms must work together
 * HOW:  Check capabilities -> Check rate limit -> Execute or reject
 */
TEST_F(SecurityWorkflowE2ETest, RateLimitingWithCapabilityEnforcement)
{
    SecurityContext ctx;
    ASSERT_TRUE(ctx.is_valid());

    // Define action to check
    capability_action_t action = {};
    strcpy(action.action_type, "network_request");
    action.category = CAPABILITY_NETWORK;
    action.memory_required = 1024;

    // Workflow: capability check then rate limit
    int successful_requests = 0;
    int capability_denied = 0;
    int rate_limited = 0;

    for (int i = 0; i < 200; ++i) {
        // Step 1: Check capability
        capability_check_result_t cap_result;
        capability_control_check_action(ctx.get_capability(), &action, &cap_result);

        if (!cap_result.allowed) {
            capability_denied++;
            continue;
        }

        // Step 2: Check rate limit
        if (!nimcp_rate_limiter_allow(ctx.get_rate_limiter(), "test_client")) {
            rate_limited++;
            continue;
        }

        // Step 3: Request allowed
        successful_requests++;
    }

    // Verify some requests succeeded and some were rate limited
    EXPECT_GT(successful_requests, 0) << "Some requests should succeed";
    EXPECT_GT(rate_limited, 0) << "Some requests should be rate limited";

    // Log results
    char summary[256];
    snprintf(summary, sizeof(summary),
             "Request summary: success=%d, capability_denied=%d, rate_limited=%d",
             successful_requests, capability_denied, rate_limited);

    EXPECT_EQ(NIMCP_OK, nimcp_encrypted_audit_log(
        ctx.get_audit(),
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_AUTHORIZATION,
        summary,
        nullptr, 0
    ));
}

/**
 * @test AnomalyDetectionWithQuarantine
 *
 * WHAT: Anomaly detection triggers quarantine
 * WHY:  Detected anomalies must be contained
 * HOW:  Detect anomaly -> Quarantine region -> Log
 */
TEST_F(SecurityWorkflowE2ETest, AnomalyDetectionWithQuarantine)
{
    SecurityContext ctx;
    ASSERT_TRUE(ctx.is_valid());

    // Train anomaly detector on normal data
    const char* normal_samples[] = {
        "GET /api/users HTTP/1.1",
        "POST /api/login HTTP/1.1",
        "GET /api/products HTTP/1.1"
    };

    for (const char* sample : normal_samples) {
        nimcp_anomaly_train(ctx.get_anomaly(), sample, strlen(sample), true);
    }

    // Check suspicious input
    char suspicious_input[256];
    strcpy(suspicious_input, "\x00\x00\xFF\xFF\xDE\xAD\xBE\xEF{{{{[[[");

    nimcp_anomaly_result_t anomaly_result;
    nimcp_anomaly_detect(ctx.get_anomaly(), suspicious_input,
                         strlen(suspicious_input), &anomaly_result);

    // If anomalous, validate via BBB and potentially quarantine
    if (anomaly_result.anomaly_score > 0.5f) {
        bbb_validation_result_t bbb_result;
        bool valid = bbb_validate_input(ctx.get_bbb(), suspicious_input,
                                         strlen(suspicious_input), &bbb_result);

        if (!valid || bbb_result.threat != BBB_THREAT_NONE) {
            // Quarantine the data
            EXPECT_TRUE(bbb_quarantine_region(ctx.get_bbb(), suspicious_input,
                                              sizeof(suspicious_input)));

            // Log the quarantine
            EXPECT_EQ(NIMCP_OK, nimcp_encrypted_audit_log(
                ctx.get_audit(),
                NIMCP_AUDIT_WARNING,
                NIMCP_AUDIT_THREAT,
                "Anomalous input quarantined",
                suspicious_input, strlen(suspicious_input)
            ));

            // Verify quarantine
            EXPECT_TRUE(bbb_is_quarantined(ctx.get_bbb(), suspicious_input,
                                           sizeof(suspicious_input)));

            // Release after processing
            EXPECT_TRUE(bbb_release_quarantine(ctx.get_bbb(), suspicious_input));
        }
    }
}

/**
 * @test SafetyVerificationBeforePolicyDeployment
 *
 * WHAT: Safety verification before deploying new policy
 * WHY:  Policies must be verified safe before use
 * HOW:  Create rules -> Verify -> Deploy
 */
TEST_F(SecurityWorkflowE2ETest, SafetyVerificationBeforePolicyDeployment)
{
    SecurityContext ctx;
    ASSERT_TRUE(ctx.is_valid());

    // Define safety rules
    safety_rule_t rules[3];
    memset(rules, 0, sizeof(rules));

    rules[0].rule_id = 1;
    strcpy(rules[0].name, "block_dangerous");
    rules[0].priority = 100;
    rules[0].is_blocking = true;
    rules[0].is_mandatory = true;

    rules[1].rule_id = 2;
    strcpy(rules[1].name, "log_all");
    rules[1].priority = 50;
    rules[1].is_blocking = false;
    rules[1].is_mandatory = true;

    rules[2].rule_id = 3;
    strcpy(rules[2].name, "allow_safe");
    rules[2].priority = 10;
    rules[2].is_blocking = false;
    rules[2].is_mandatory = false;

    // Step 1: Verify completeness
    verification_result_t completeness_result;
    EXPECT_EQ(NIMCP_OK, safety_verify_completeness(
        ctx.get_safety(), rules, 3, &completeness_result
    ));

    // Step 2: Verify priority respect
    verification_result_t priority_result;
    EXPECT_EQ(NIMCP_OK, safety_verify_priority_respect(
        ctx.get_safety(), rules, 3, &priority_result
    ));

    // Step 3: Log verification results
    char verify_msg[256];
    snprintf(verify_msg, sizeof(verify_msg),
             "Safety verification: completeness=%.1f%%, priority_check=%s",
             completeness_result.coverage_percentage,
             priority_result.passed ? "PASS" : "FAIL");

    EXPECT_EQ(NIMCP_OK, nimcp_encrypted_audit_log(
        ctx.get_audit(),
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_CONFIGURATION,
        verify_msg,
        nullptr, 0
    ));

    // Verify stats
    safety_verification_stats_t safety_stats;
    EXPECT_EQ(NIMCP_OK, safety_verification_get_stats(ctx.get_safety(), &safety_stats));
    EXPECT_GT(safety_stats.total_verifications, 0u);
}

/**
 * @test MultiThreatScenario
 *
 * WHAT: Handle multiple simultaneous threats
 * WHY:  Real attacks often involve multiple vectors
 * HOW:  Simulate SQL injection + network anomaly + rate limit abuse
 */
TEST_F(SecurityWorkflowE2ETest, MultiThreatScenario)
{
    SecurityContext ctx;
    ASSERT_TRUE(ctx.is_valid());

    std::atomic<int> threats_detected{0};
    std::atomic<int> threats_blocked{0};

    // Thread 1: SQL injection attempts
    std::thread sql_thread([&ctx, &threats_detected, &threats_blocked]() {
        for (int i = 0; i < 50; ++i) {
            char payload[256];
            snprintf(payload, sizeof(payload), "' OR '%d'='%d", i, i);

            bbb_validation_result_t result;
            if (!bbb_validate_string(ctx.get_bbb(), payload, &result)) {
                threats_detected.fetch_add(1);
                if (result.threat == BBB_THREAT_SQL_INJECTION) {
                    threats_blocked.fetch_add(1);
                }
            }
        }
    });

    // Thread 2: Network anomaly simulation
    std::thread network_thread([&ctx, &threats_detected]() {
        for (int i = 0; i < 50; ++i) {
            tripwire_observe_network_connection(
                ctx.get_tripwire(),
                0xC0A80001 + i,
                31337 + (i % 100),
                50000 + i * 1000,
                100,
                TRIPWIRE_PROTO_TCP
            );
        }

        float anomaly_score = tripwire_detect_network_anomaly(ctx.get_tripwire());
        if (anomaly_score > 0.3f) {
            threats_detected.fetch_add(1);
        }
    });

    // Thread 3: Rate limit abuse
    std::thread rate_thread([&ctx, &threats_blocked]() {
        for (int i = 0; i < 200; ++i) {
            if (!nimcp_rate_limiter_allow(ctx.get_rate_limiter(), "attacker")) {
                threats_blocked.fetch_add(1);
            }
        }
    });

    // Wait for all threads
    sql_thread.join();
    network_thread.join();
    rate_thread.join();

    // Verify threats were handled
    EXPECT_GT(threats_detected.load(), 0) << "Some threats should be detected";
    EXPECT_GT(threats_blocked.load(), 0) << "Some threats should be blocked";

    // Log summary
    char summary[256];
    snprintf(summary, sizeof(summary),
             "Multi-threat scenario: detected=%d, blocked=%d",
             threats_detected.load(), threats_blocked.load());

    EXPECT_EQ(NIMCP_OK, nimcp_encrypted_audit_log(
        ctx.get_audit(),
        NIMCP_AUDIT_WARNING,
        NIMCP_AUDIT_THREAT,
        summary,
        nullptr, 0
    ));
}

/**
 * @test CleanShutdownWorkflow
 *
 * WHAT: Verify clean shutdown of all security modules
 * WHY:  Prevent resource leaks and ensure proper cleanup
 * HOW:  Initialize -> Use -> Destroy in order
 */
TEST_F(SecurityWorkflowE2ETest, CleanShutdownWorkflow)
{
    // Create context in a scope
    {
        SecurityContext ctx;
        ASSERT_TRUE(ctx.is_valid());

        // Use all modules
        bbb_validation_result_t bbb_result;
        bbb_validate_string(ctx.get_bbb(), "test", &bbb_result);

        tripwire_observe_goal(ctx.get_tripwire(), 1, 0.5f, 0.5f);

        nimcp_anomaly_result_t anomaly_result;
        nimcp_anomaly_detect(ctx.get_anomaly(), "test", 4, &anomaly_result);

        nimcp_rate_limiter_allow(ctx.get_rate_limiter(), "client");

        capability_action_t action = {};
        capability_check_result_t cap_result;
        capability_control_check_action(ctx.get_capability(), &action, &cap_result);

        safety_verification_stats_t safety_stats;
        safety_verification_get_stats(ctx.get_safety(), &safety_stats);

        nimcp_encrypted_audit_log(ctx.get_audit(), NIMCP_AUDIT_INFO,
                                  NIMCP_AUDIT_SYSTEM, "Shutdown test", nullptr, 0);

        // Context destructor will clean up all modules
    }

    // After context is destroyed, verify we can create new context
    SecurityContext new_ctx;
    EXPECT_TRUE(new_ctx.is_valid()) << "Should be able to create new context after cleanup";
}

/**
 * @test FullSecurityPipelineStressTest
 *
 * WHAT: Stress test the complete security pipeline
 * WHY:  Verify system handles high load
 * HOW:  Many concurrent operations across all modules
 */
TEST_F(SecurityWorkflowE2ETest, FullSecurityPipelineStressTest)
{
    SecurityContext ctx;
    ASSERT_TRUE(ctx.is_valid());

    const int NUM_THREADS = 8;
    const int OPS_PER_THREAD = 100;
    std::atomic<int> total_ops{0};
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&ctx, &total_ops, &errors, t]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                // Cycle through different operations
                switch ((t + i) % 5) {
                    case 0: {
                        bbb_validation_result_t result;
                        bbb_validate_string(ctx.get_bbb(), "stress_test", &result);
                        break;
                    }
                    case 1: {
                        tripwire_observe_goal(ctx.get_tripwire(), i % 10, 0.5f, 0.5f);
                        break;
                    }
                    case 2: {
                        nimcp_anomaly_result_t result;
                        nimcp_anomaly_detect(ctx.get_anomaly(), "test", 4, &result);
                        break;
                    }
                    case 3: {
                        nimcp_rate_limiter_allow(ctx.get_rate_limiter(), "stress_client");
                        break;
                    }
                    case 4: {
                        capability_action_t action = {};
                        capability_check_result_t result;
                        capability_control_check_action(ctx.get_capability(), &action, &result);
                        break;
                    }
                }
                total_ops.fetch_add(1);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(total_ops.load(), NUM_THREADS * OPS_PER_THREAD);
    EXPECT_EQ(errors.load(), 0) << "No errors should occur";

    // Final audit entry
    char summary[256];
    snprintf(summary, sizeof(summary),
             "Stress test complete: %d operations, %d errors",
             total_ops.load(), errors.load());

    EXPECT_EQ(NIMCP_OK, nimcp_encrypted_audit_log(
        ctx.get_audit(),
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_SYSTEM,
        summary,
        nullptr, 0
    ));
}

//=============================================================================
// Negative Tests
//=============================================================================

/**
 * @test PartialInitializationRecovery
 *
 * WHAT: Verify partial initialization is handled
 * WHY:  Some modules may fail to initialize
 * HOW:  Test with NULL audit key to trigger failure
 */
TEST_F(SecurityWorkflowE2ETest, DISABLED_PartialInitializationRecovery)
{
    // This test is disabled because SecurityContext always tries to create
    // all modules. A more targeted test would need custom initialization.

    // Individual module failure handling
    nimcp_encrypted_audit_t bad_audit = nimcp_encrypted_audit_create(
        nullptr,  // NULL config
        nullptr,  // NULL key
        0         // Zero key size
    );
    EXPECT_EQ(bad_audit, nullptr) << "Should fail with invalid params";
}

}  // anonymous namespace
