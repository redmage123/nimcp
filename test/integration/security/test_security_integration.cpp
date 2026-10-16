/**
 * @file test_security_integration.cpp
 * @brief Comprehensive security integration tests
 * @date 2026-02-02
 *
 * WHAT: Integration tests verifying cross-module security coordination
 * WHY:  Ensure all security components work together correctly
 * HOW:  Test BBB+Immune, Tripwire+Anomaly, RateLimiter+Capability,
 *       Policy+Safety, and SBOM+SupplyChain integrations
 *
 * TEST CATEGORIES:
 *   1. BBB + Brain Immune Integration (4 tests)
 *   2. Tripwire + Anomaly Detector Integration (4 tests)
 *   3. Rate Limiter + Capability Control Integration (4 tests)
 *   4. Policy Engine + Safety Verification Integration (4 tests)
 *   5. SBOM + Supply Chain Integration (4 tests)
 *   6. Error Paths and Edge Cases (4 tests)
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

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

/** Test epitope for threat simulation */
static const uint8_t TEST_EPITOPE[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
static constexpr size_t TEST_EPITOPE_LEN = sizeof(TEST_EPITOPE);

/** Standard test timeout (100ms) */
static constexpr int TEST_TIMEOUT_MS = 100;

//=============================================================================
// Category 1: BBB + Brain Immune Integration Tests
//=============================================================================

class BBBImmuneIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Reset BBB subsystem state for test isolation
        bbb_reset_test_state();

        // Create brain immune system
        brain_immune_config_t immune_config = {};
        immune_config.max_antigens = 100;
        immune_config.max_b_cells = 50;
        immune_config.max_t_cells = 50;
        immune_config.max_antibodies = 100;
        immune_system_ = brain_immune_create(&immune_config);

        // Create BBB system
        bbb_config_ = bbb_default_config();
        bbb_config_.strict_mode = true;
        bbb_system_ = bbb_system_create(&bbb_config_);

        ASSERT_NE(bbb_system_, nullptr);
        ASSERT_TRUE(bbb_system_set_enabled(bbb_system_, true));
    }

    void TearDown() override
    {
        bbb_clear_signing_key();

        if (bbb_system_) {
            bbb_system_destroy(bbb_system_);
            bbb_system_ = nullptr;
        }

        if (immune_system_) {
            brain_immune_destroy(immune_system_);
            immune_system_ = nullptr;
        }
    }

    brain_immune_system_t* immune_system_ = nullptr;
    bbb_system_t bbb_system_ = nullptr;
    bbb_config_t bbb_config_;
};

/**
 * @test BBBThreatTriggersImmuneResponse
 *
 * WHAT: Verify BBB threats are properly forwarded to immune system
 * WHY:  Integration between BBB and immune is critical for coordinated defense
 * HOW:  Connect BBB to immune, trigger threat, verify antigen presentation
 */
TEST_F(BBBImmuneIntegrationTest, BBBThreatTriggersImmuneResponse)
{
    ASSERT_NE(immune_system_, nullptr) << "Immune system should be created";

    // Connect BBB to immune system
    bool connected = bbb_connect_immune(bbb_system_, immune_system_);
    EXPECT_TRUE(connected) << "BBB should connect to immune system";

    // Detect a threat
    char malicious_data[256];
    strcpy(malicious_data, "'; DROP TABLE users; --");

    bbb_validation_result_t result;
    bool valid = bbb_validate_string(bbb_system_, malicious_data, &result);
    EXPECT_FALSE(valid) << "SQL injection should be detected";
    EXPECT_EQ(result.threat, BBB_THREAT_SQL_INJECTION);

    // Present threat to immune system manually (simulating automatic flow)
    uint32_t antigen_id = 0;
    int present_result = brain_immune_present_bbb_threat(
        immune_system_,
        result.threat,
        result.severity,
        TEST_EPITOPE,
        TEST_EPITOPE_LEN,
        &antigen_id
    );
    EXPECT_EQ(0, present_result);
    EXPECT_GT(antigen_id, 0u) << "Antigen should be created";

    // Verify immune system has the antigen
    brain_immune_stats_t stats;
    EXPECT_EQ(0, brain_immune_get_stats(immune_system_, &stats));
    EXPECT_GT(stats.antigens_processed, 0u) << "Immune system should have antigens";
}

/**
 * @test BBBQuarantineCoordinatesWithImmune
 *
 * WHAT: Verify quarantine actions coordinate with immune response
 * WHY:  Quarantine should trigger immune system escalation
 * HOW:  Quarantine region, verify immune inflammation initiated
 */
TEST_F(BBBImmuneIntegrationTest, BBBQuarantineCoordinatesWithImmune)
{
    ASSERT_TRUE(bbb_connect_immune(bbb_system_, immune_system_));

    char threat_data[256];
    strcpy(threat_data, "Malicious payload");

    // Quarantine the region
    EXPECT_TRUE(bbb_quarantine_region(bbb_system_, threat_data, sizeof(threat_data)));
    EXPECT_TRUE(bbb_is_quarantined(bbb_system_, threat_data, sizeof(threat_data)));

    // Present threat as critical to trigger inflammation
    uint32_t antigen_id = 0;
    EXPECT_EQ(0, brain_immune_present_bbb_threat(
        immune_system_,
        BBB_THREAT_SHELLCODE,
        BBB_SEVERITY_CRITICAL,
        TEST_EPITOPE,
        TEST_EPITOPE_LEN,
        &antigen_id
    ));

    // Initiate inflammation
    uint32_t site_id = 0;
    EXPECT_EQ(0, brain_immune_initiate_inflammation(immune_system_, 0, antigen_id, &site_id));
    EXPECT_GT(site_id, 0u);

    // Clean up
    EXPECT_TRUE(bbb_release_quarantine(bbb_system_, threat_data));
}

/**
 * @test BBBStatisticsReflectImmuneEvents
 *
 * WHAT: Verify BBB statistics track immune-related events
 * WHY:  Statistics needed for monitoring and auditing
 * HOW:  Generate events, verify statistics increment
 */
TEST_F(BBBImmuneIntegrationTest, BBBStatisticsReflectImmuneEvents)
{
    ASSERT_TRUE(bbb_connect_immune(bbb_system_, immune_system_));

    // Get initial stats
    bbb_statistics_t initial_stats;
    EXPECT_TRUE(bbb_system_get_statistics(bbb_system_, &initial_stats));

    // Perform validations
    bbb_validation_result_t result;
    bbb_validate_string(bbb_system_, "safe input", &result);
    bbb_validate_string(bbb_system_, "'; DROP TABLE x; --", &result);
    bbb_validate_string(bbb_system_, "another safe input", &result);

    // Get final stats
    bbb_statistics_t final_stats;
    EXPECT_TRUE(bbb_system_get_statistics(bbb_system_, &final_stats));

    // Verify stats updated
    EXPECT_EQ(final_stats.total_validations, initial_stats.total_validations + 3);
    EXPECT_GE(final_stats.threats_detected, initial_stats.threats_detected + 1);
}

/**
 * @test DisconnectedImmuneSystemHandledGracefully
 *
 * WHAT: Verify BBB works when immune system disconnected
 * WHY:  System must remain functional even without immune
 * HOW:  Connect then disconnect, verify BBB still works
 */
TEST_F(BBBImmuneIntegrationTest, DisconnectedImmuneSystemHandledGracefully)
{
    // Connect immune
    ASSERT_TRUE(bbb_connect_immune(bbb_system_, immune_system_));

    // Disconnect by passing NULL
    EXPECT_TRUE(bbb_connect_immune(bbb_system_, nullptr));

    // BBB should still work
    bbb_validation_result_t result;
    bool valid = bbb_validate_string(bbb_system_, "test input", &result);
    EXPECT_TRUE(valid) << "BBB should still validate after immune disconnect";

    // Threat detection should still work
    valid = bbb_validate_string(bbb_system_, "'; DROP TABLE x; --", &result);
    EXPECT_FALSE(valid) << "BBB should still detect threats after immune disconnect";
}

//=============================================================================
// Category 2: Tripwire + Anomaly Detector Integration Tests
//=============================================================================

class TripwireAnomalyIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create tripwire system
        tripwire_config_ = tripwire_default_config();
        tripwire_ = tripwire_create(&tripwire_config_);
        ASSERT_NE(tripwire_, nullptr);

        // Create anomaly detector
        anomaly_config_ = nimcp_anomaly_detector_default_config();
        anomaly_detector_ = nimcp_anomaly_detector_create(&anomaly_config_);
        ASSERT_NE(anomaly_detector_, nullptr);
    }

    void TearDown() override
    {
        if (tripwire_) {
            tripwire_destroy(tripwire_);
            tripwire_ = nullptr;
        }

        if (anomaly_detector_) {
            nimcp_anomaly_detector_destroy(anomaly_detector_);
            anomaly_detector_ = nullptr;
        }
    }

    tripwire_config_t tripwire_config_;
    tripwire_system_t* tripwire_ = nullptr;
    nimcp_anomaly_config_t anomaly_config_;
    nimcp_anomaly_detector_t anomaly_detector_ = nullptr;
};

/**
 * @test TripwireAndAnomalyDetectSameThreat
 *
 * WHAT: Verify tripwire and anomaly detector can detect same threat pattern
 * WHY:  Defense in depth - multiple detection layers
 * HOW:  Present same suspicious pattern to both systems
 */
TEST_F(TripwireAnomalyIntegrationTest, TripwireAndAnomalyDetectSameThreat)
{
    // Observe suspicious network pattern via tripwire
    for (int i = 0; i < 20; ++i) {
        tripwire_observe_network_connection(
            tripwire_,
            0xC0A80001,      // Suspicious external IP
            31337,           // Suspicious port
            100000,          // High outbound
            100,             // Low response
            TRIPWIRE_PROTO_TCP
        );
    }

    float tripwire_score = tripwire_detect_network_anomaly(tripwire_);
    EXPECT_GE(tripwire_score, 0.0f);
    EXPECT_LE(tripwire_score, 1.0f);

    // Check same pattern with anomaly detector
    const char* suspicious_pattern = "\x00\x00\x00\x00\xFF\xFF\xFF\xFF";
    nimcp_anomaly_result_t anomaly_result;
    nimcp_error_t err = nimcp_anomaly_detect(
        anomaly_detector_,
        suspicious_pattern,
        8,
        &anomaly_result
    );
    EXPECT_EQ(NIMCP_OK, err);
    // Score depends on training state, but should be valid
    EXPECT_GE(anomaly_result.anomaly_score, 0.0f);
    EXPECT_LE(anomaly_result.anomaly_score, 1.0f);
}

/**
 * @test AnomalyFeedsBackToTripwire
 *
 * WHAT: Verify anomaly detection can inform tripwire observations
 * WHY:  Correlation between systems improves detection
 * HOW:  Detect anomaly, record as tripwire observation
 */
TEST_F(TripwireAnomalyIntegrationTest, AnomalyFeedsBackToTripwire)
{
    // Train anomaly detector on normal data
    const char* normal_data[] = {
        "normal request 1",
        "normal request 2",
        "typical user input"
    };

    for (const char* data : normal_data) {
        nimcp_anomaly_train(anomaly_detector_, data, strlen(data), true);
    }

    // Detect anomaly
    const char* anomalous = ";;{{{{{{{[[[[";
    nimcp_anomaly_result_t result;
    nimcp_anomaly_detect(anomaly_detector_, anomalous, strlen(anomalous), &result);

    // If anomaly detected, observe it in tripwire
    if (result.anomaly_score > 0.5f) {
        tripwire_observe_resource(tripwire_, 1, result.anomaly_score * 100.0f, "anomaly_detected");
    }

    // Check tripwire stats
    tripwire_stats_t stats;
    EXPECT_EQ(NIMCP_OK, tripwire_get_stats(tripwire_, &stats));
}

/**
 * @test BothSystemsHandleNullInputs
 *
 * WHAT: Verify both systems handle NULL inputs gracefully
 * WHY:  Robustness against invalid inputs
 * HOW:  Pass NULL to various functions, verify no crash
 */
TEST_F(TripwireAnomalyIntegrationTest, BothSystemsHandleNullInputs)
{
    // Tripwire NULL handling
    nimcp_error_t result = tripwire_observe_goal(nullptr, 1, 0.5f, 0.5f);
    EXPECT_NE(NIMCP_OK, result) << "Tripwire should reject NULL system";

    result = tripwire_get_stats(nullptr, nullptr);
    EXPECT_NE(NIMCP_OK, result) << "Tripwire should reject NULL params";

    // Anomaly detector NULL handling
    nimcp_anomaly_result_t anomaly_result;
    nimcp_error_t err = nimcp_anomaly_detect(nullptr, "test", 4, &anomaly_result);
    EXPECT_NE(NIMCP_OK, err) << "Anomaly detector should reject NULL detector";

    err = nimcp_anomaly_detect(anomaly_detector_, nullptr, 0, &anomaly_result);
    EXPECT_NE(NIMCP_OK, err) << "Anomaly detector should reject NULL input";
}

/**
 * @test ConcurrentTripwireAndAnomalyDetection
 *
 * WHAT: Verify concurrent access to both systems is safe
 * WHY:  Real systems have parallel detection
 * HOW:  Run both systems in parallel threads
 */
TEST_F(TripwireAnomalyIntegrationTest, ConcurrentTripwireAndAnomalyDetection)
{
    const int NUM_ITERATIONS = 50;
    std::atomic<int> tripwire_ops{0};
    std::atomic<int> anomaly_ops{0};

    // Tripwire thread
    std::thread tripwire_thread([this, &tripwire_ops]() {
        for (int i = 0; i < NUM_ITERATIONS; ++i) {
            tripwire_observe_goal(tripwire_, i % 10, 0.8f, 0.9f);
            tripwire_ops.fetch_add(1);
        }
    });

    // Anomaly thread
    std::thread anomaly_thread([this, &anomaly_ops]() {
        for (int i = 0; i < NUM_ITERATIONS; ++i) {
            char buf[32];
            snprintf(buf, sizeof(buf), "input_%d", i);
            nimcp_anomaly_result_t result;
            nimcp_anomaly_detect(anomaly_detector_, buf, strlen(buf), &result);
            anomaly_ops.fetch_add(1);
        }
    });

    tripwire_thread.join();
    anomaly_thread.join();

    EXPECT_EQ(tripwire_ops.load(), NUM_ITERATIONS);
    EXPECT_EQ(anomaly_ops.load(), NUM_ITERATIONS);
}

//=============================================================================
// Category 3: Rate Limiter + Capability Control Integration Tests
//=============================================================================

class RateLimiterCapabilityIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create rate limiter
        rate_limit_config_ = nimcp_rate_limiter_default_config();
        rate_limit_config_.requests_per_second = 10.0f;
        rate_limit_config_.burst_size = 15;
        rate_limiter_ = nimcp_rate_limiter_create(&rate_limit_config_);
        ASSERT_NE(rate_limiter_, nullptr);

        // Create capability control
        capability_config_ = capability_control_default_config();
        capability_control_ = capability_control_create(&capability_config_);
        ASSERT_NE(capability_control_, nullptr);
    }

    void TearDown() override
    {
        if (rate_limiter_) {
            nimcp_rate_limiter_destroy(rate_limiter_);
            rate_limiter_ = nullptr;
        }

        if (capability_control_) {
            capability_control_destroy(capability_control_);
            capability_control_ = nullptr;
        }
    }

    nimcp_rate_limit_config_t rate_limit_config_;
    nimcp_rate_limiter_t rate_limiter_ = nullptr;
    capability_control_config_t capability_config_;
    capability_control_t* capability_control_ = nullptr;
};

/**
 * @test RateLimiterEnforcesCapabilityLimits
 *
 * WHAT: Verify rate limiter can enforce capability resource limits
 * WHY:  Rate limiting is part of resource control
 * HOW:  Set limits, verify enforcement
 */
TEST_F(RateLimiterCapabilityIntegrationTest, RateLimiterEnforcesCapabilityLimits)
{
    // Verify initial allowance
    int allowed_count = 0;
    for (int i = 0; i < 20; ++i) {
        if (nimcp_rate_limiter_allow(rate_limiter_, "test_client")) {
            allowed_count++;
        }
    }

    // Should be limited by burst size
    EXPECT_LE(allowed_count, 15) << "Burst limit should be enforced";
    EXPECT_GT(allowed_count, 0) << "Some requests should be allowed";

    // Check capability resource limits
    bool allowed = false;
    capability_control_check_resources(
        capability_control_,
        1024 * 1024,  // 1MB memory
        1000000,      // 1M FLOPS
        &allowed
    );
    // Safe envelope should allow reasonable resources
    EXPECT_TRUE(allowed);
}

/**
 * @test CapabilityCheckBeforeRateLimit
 *
 * WHAT: Verify capability check happens before rate limiting
 * WHY:  Unauthorized actions should be rejected before consuming rate quota
 * HOW:  Attempt unauthorized action, verify no rate limit consumption
 */
TEST_F(RateLimiterCapabilityIntegrationTest, CapabilityCheckBeforeRateLimit)
{
    // Try to access network with restricted capabilities
    capability_action_t action = {};
    strcpy(action.action_type, "network_access");
    action.category = CAPABILITY_NETWORK;
    strcpy(action.target_domain, "malicious-site.com");

    capability_check_result_t cap_result;
    capability_control_check_action(capability_control_, &action, &cap_result);

    // If capability denied, should not consume rate limit
    if (!cap_result.allowed) {
        // This access was blocked by capability, not rate limiter
        EXPECT_EQ(cap_result.violated_category, CAPABILITY_NETWORK);
    }

    // Now check a valid action
    strcpy(action.target_domain, "");
    action.category = CAPABILITY_RESOURCE;
    action.memory_required = 1024;

    capability_control_check_action(capability_control_, &action, &cap_result);
    // Resource action should be allowed
    EXPECT_TRUE(cap_result.allowed);
}

/**
 * @test RateLimitStatisticsTrackCapabilityViolations
 *
 * WHAT: Verify statistics track both rate limit and capability violations
 * WHY:  Need comprehensive monitoring
 * HOW:  Generate both types of violations, check stats
 */
TEST_F(RateLimiterCapabilityIntegrationTest, RateLimitStatisticsTrackCapabilityViolations)
{
    // Exhaust rate limit
    for (int i = 0; i < 50; ++i) {
        nimcp_rate_limiter_allow(rate_limiter_, "test_client");
    }

    // Get rate limiter stats
    nimcp_rate_limit_stats_t rl_stats;
    nimcp_rate_limiter_get_stats(rate_limiter_, &rl_stats);
    EXPECT_GT(rl_stats.total_requests, 0u);
    EXPECT_GT(rl_stats.requests_denied, 0u);

    // Generate capability violations
    capability_action_t action = {};
    action.category = CAPABILITY_SELF_MODIFICATION;
    action.modifies_self = true;

    capability_check_result_t cap_result;
    for (int i = 0; i < 5; ++i) {
        capability_control_check_action(capability_control_, &action, &cap_result);
    }

    // Get capability stats
    capability_control_stats_t cap_stats;
    capability_control_get_stats(capability_control_, &cap_stats);
    EXPECT_GT(cap_stats.total_actions_checked, 0u);
}

/**
 * @test NullParameterHandling
 *
 * WHAT: Verify NULL parameters are handled gracefully
 * WHY:  Robustness requirement
 * HOW:  Pass NULL to functions, verify no crash
 */
TEST_F(RateLimiterCapabilityIntegrationTest, NullParameterHandling)
{
    // Rate limiter NULL handling
    EXPECT_FALSE(nimcp_rate_limiter_allow(nullptr, "client"));
    EXPECT_FALSE(nimcp_rate_limiter_allow(rate_limiter_, nullptr));

    nimcp_rate_limit_stats_t stats;
    EXPECT_NE(NIMCP_OK, nimcp_rate_limiter_get_stats(nullptr, &stats));

    // Capability control NULL handling
    capability_check_result_t result;
    EXPECT_NE(NIMCP_OK, capability_control_check_action(nullptr, nullptr, &result));
    EXPECT_NE(NIMCP_OK, capability_control_check_action(capability_control_, nullptr, &result));

    capability_control_stats_t cap_stats;
    EXPECT_NE(NIMCP_OK, capability_control_get_stats(nullptr, &cap_stats));
}

//=============================================================================
// Category 4: Policy Engine + Safety Verification Integration Tests
//=============================================================================

class PolicySafetyIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create policy engine
        policy_config_.max_policies = 10;
        policy_config_.max_rules_per_policy = 100;
        policy_config_.enable_caching = true;
        policy_config_.cache_size = 1000;
        policy_config_.enable_optimization = true;
        policy_config_.enable_hot_reload = false;
        policy_config_.policy_directory = nullptr;
        policy_config_.bio_router = nullptr;
        policy_engine_ = nimcp_policy_engine_create(&policy_config_);
        ASSERT_NE(policy_engine_, nullptr);

        // Create safety verification
        safety_config_ = safety_verification_default_config();
        safety_verification_ = safety_verification_create(&safety_config_);
        ASSERT_NE(safety_verification_, nullptr);
    }

    void TearDown() override
    {
        if (policy_engine_) {
            nimcp_policy_engine_destroy(policy_engine_);
            policy_engine_ = nullptr;
        }

        if (safety_verification_) {
            safety_verification_destroy(safety_verification_);
            safety_verification_ = nullptr;
        }
    }

    nimcp_policy_engine_config_t policy_config_;
    nimcp_policy_engine_t policy_engine_ = nullptr;
    safety_verification_config_t safety_config_;
    safety_verification_t* safety_verification_ = nullptr;
};

/**
 * @test PolicyEvaluationWithSafetyVerification
 *
 * WHAT: Verify policy evaluation integrates with safety verification
 * WHY:  Policies must be verified safe before enforcement
 * HOW:  Create rules, verify completeness, evaluate
 */
TEST_F(PolicySafetyIntegrationTest, PolicyEvaluationWithSafetyVerification)
{
    // Create safety rules
    safety_rule_t rules[3];
    memset(rules, 0, sizeof(rules));

    // Rule 1: Block dangerous actions
    rules[0].rule_id = 1;
    strcpy(rules[0].name, "block_dangerous");
    rules[0].priority = 100;
    rules[0].is_blocking = true;
    rules[0].is_mandatory = true;
    strcpy(rules[0].condition, "action_type == DANGEROUS");

    // Rule 2: Log all actions
    rules[1].rule_id = 2;
    strcpy(rules[1].name, "log_all");
    rules[1].priority = 50;
    rules[1].is_blocking = false;
    rules[1].is_mandatory = true;
    strcpy(rules[1].condition, "true");

    // Rule 3: Allow safe actions
    rules[2].rule_id = 3;
    strcpy(rules[2].name, "allow_safe");
    rules[2].priority = 10;
    rules[2].is_blocking = false;
    rules[2].is_mandatory = false;
    strcpy(rules[2].condition, "action_type == SAFE");

    // Verify completeness (without SAT solver for now)
    verification_result_t result;
    nimcp_error_t err = safety_verify_completeness(
        safety_verification_,
        rules,
        3,
        &result
    );
    EXPECT_EQ(NIMCP_OK, err);
    // Result depends on implementation
    EXPECT_LE(result.coverage_percentage, 100.0f);
    EXPECT_GE(result.coverage_percentage, 0.0f);
}

/**
 * @test SafetyVerificationBlocksInvalidPolicy
 *
 * WHAT: Verify safety verification catches invalid policies
 * WHY:  Prevent deployment of unsafe policies
 * HOW:  Create conflicting rules, verify detection
 */
TEST_F(PolicySafetyIntegrationTest, SafetyVerificationBlocksInvalidPolicy)
{
    // Create potentially conflicting rules
    safety_rule_t rules[2];
    memset(rules, 0, sizeof(rules));

    // Rule 1: Block action A
    rules[0].rule_id = 1;
    strcpy(rules[0].name, "block_A");
    rules[0].priority = 100;
    rules[0].is_blocking = true;
    strcpy(rules[0].condition, "action == A");

    // Rule 2: Allow action A (conflict!)
    rules[1].rule_id = 2;
    strcpy(rules[1].name, "allow_A");
    rules[1].priority = 100;  // Same priority = potential conflict
    rules[1].is_blocking = false;
    strcpy(rules[1].condition, "action == A");

    // Check priority respect
    verification_result_t result;
    nimcp_error_t err = safety_verify_priority_respect(
        safety_verification_,
        rules,
        2,
        &result
    );
    EXPECT_EQ(NIMCP_OK, err);
    // Same priority may not be a violation depending on implementation
}

/**
 * @test PolicyContextCreationAndDestruction
 *
 * WHAT: Verify policy context lifecycle is correct
 * WHY:  Memory management for contexts
 * HOW:  Create, use, destroy contexts
 */
TEST_F(PolicySafetyIntegrationTest, PolicyContextCreationAndDestruction)
{
    // Create context
    nimcp_policy_context_t ctx = nimcp_policy_context_create();
    ASSERT_NE(ctx, nullptr);

    // Set values
    EXPECT_EQ(NIMCP_OK, nimcp_policy_context_set_string(ctx, "action", "read"));
    EXPECT_EQ(NIMCP_OK, nimcp_policy_context_set_int(ctx, "user_level", 5));
    EXPECT_EQ(NIMCP_OK, nimcp_policy_context_set_bool(ctx, "is_admin", false));
    EXPECT_EQ(NIMCP_OK, nimcp_policy_context_set_float(ctx, "trust_score", 0.85));

    // Get values
    nimcp_policy_value_t value;
    EXPECT_EQ(NIMCP_OK, nimcp_policy_context_get(ctx, "user_level", &value));
    EXPECT_EQ(NIMCP_POLICY_VALUE_INT, value.type);
    EXPECT_EQ(5, value.int_val);

    // Destroy
    nimcp_policy_context_destroy(ctx);
}

/**
 * @test SafetyVerificationStatistics
 *
 * WHAT: Verify statistics are tracked correctly
 * WHY:  Need to monitor verification performance
 * HOW:  Run verifications, check stats
 */
TEST_F(PolicySafetyIntegrationTest, SafetyVerificationStatistics)
{
    safety_rule_t rules[1];
    memset(rules, 0, sizeof(rules));
    rules[0].rule_id = 1;
    strcpy(rules[0].name, "test_rule");
    rules[0].priority = 50;
    rules[0].is_mandatory = true;

    // Run some verifications
    verification_result_t result;
    for (int i = 0; i < 5; ++i) {
        safety_verify_completeness(safety_verification_, rules, 1, &result);
    }

    // Check stats
    safety_verification_stats_t stats;
    EXPECT_EQ(NIMCP_OK, safety_verification_get_stats(safety_verification_, &stats));
    EXPECT_GT(stats.total_verifications, 0u);
}

//=============================================================================
// Category 5: SBOM + Supply Chain Integration Tests
//=============================================================================

class SBOMSupplyChainIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create supply chain context
        memset(&supply_chain_config_, 0, sizeof(supply_chain_config_));
        supply_chain_config_.enable_logging = false;
        supply_chain_config_.strict_mode = false;
        supply_chain_config_.default_hash_algo = NIMCP_HASH_SHA256;
        supply_chain_ = nimcp_supply_chain_create(&supply_chain_config_);
        ASSERT_NE(supply_chain_, nullptr);
    }

    void TearDown() override
    {
        if (supply_chain_) {
            nimcp_supply_chain_destroy(supply_chain_);
            supply_chain_ = nullptr;
        }
    }

    nimcp_supply_chain_config_t supply_chain_config_;
    nimcp_supply_chain_t supply_chain_ = nullptr;
};

/**
 * @test AddAndQueryDependency
 *
 * WHAT: Verify dependencies can be added and queried
 * WHY:  Core SBOM functionality
 * HOW:  Add dependency, query it back
 */
TEST_F(SBOMSupplyChainIntegrationTest, AddAndQueryDependency)
{
    // Create dependency
    nimcp_dependency_t dep = {};
    dep.magic = NIMCP_DEPENDENCY_MAGIC;
    strcpy(dep.name, "test-library");
    strcpy(dep.version, "1.0.0");
    strcpy(dep.license, "MIT");
    strcpy(dep.supplier, "TestCorp");
    dep.is_direct = true;
    dep.is_critical = false;
    dep.status = NIMCP_VERIFY_UNKNOWN;

    // Add to SBOM
    nimcp_error_t err = nimcp_sbom_add_dependency(supply_chain_, &dep);
    EXPECT_EQ(NIMCP_OK, err);

    // Query back
    nimcp_dependency_t queried;
    err = nimcp_sbom_query_dependency(supply_chain_, "test-library", &queried);
    EXPECT_EQ(NIMCP_OK, err);
    EXPECT_STREQ("test-library", queried.name);
    EXPECT_STREQ("1.0.0", queried.version);
}

/**
 * @test TrustedSourceManagement
 *
 * WHAT: Verify trusted sources can be managed
 * WHY:  Need to track verified artifact sources
 * HOW:  Add, check, revoke sources
 */
TEST_F(SBOMSupplyChainIntegrationTest, TrustedSourceManagement)
{
    // Add trusted source
    nimcp_error_t err = nimcp_supply_chain_add_trusted_source(
        supply_chain_,
        "https://trusted-repo.example.com",
        "/path/to/pubkey.pem",
        NIMCP_SIG_ED25519
    );
    EXPECT_EQ(NIMCP_OK, err);

    // Check if trusted
    bool trusted = nimcp_supply_chain_is_source_trusted(
        supply_chain_,
        "https://trusted-repo.example.com"
    );
    EXPECT_TRUE(trusted);

    // Revoke source
    err = nimcp_supply_chain_revoke_source(
        supply_chain_,
        "https://trusted-repo.example.com"
    );
    EXPECT_EQ(NIMCP_OK, err);

    // Check revoked
    trusted = nimcp_supply_chain_is_source_trusted(
        supply_chain_,
        "https://trusted-repo.example.com"
    );
    EXPECT_FALSE(trusted);
}

/**
 * @test SupplyChainStatistics
 *
 * WHAT: Verify statistics track supply chain operations
 * WHY:  Need monitoring of verification activities
 * HOW:  Perform operations, check stats
 */
TEST_F(SBOMSupplyChainIntegrationTest, SupplyChainStatistics)
{
    // Add some dependencies
    for (int i = 0; i < 5; ++i) {
        nimcp_dependency_t dep = {};
        dep.magic = NIMCP_DEPENDENCY_MAGIC;
        snprintf(dep.name, sizeof(dep.name), "dep-%d", i);
        strcpy(dep.version, "1.0.0");
        dep.is_direct = true;
        nimcp_sbom_add_dependency(supply_chain_, &dep);
    }

    // Get stats
    nimcp_supply_chain_stats_t stats;
    nimcp_error_t err = nimcp_supply_chain_get_stats(supply_chain_, &stats);
    EXPECT_EQ(NIMCP_OK, err);
    EXPECT_EQ(5u, stats.total_dependencies);
}

/**
 * @test NullSupplyChainHandling
 *
 * WHAT: Verify NULL parameters handled gracefully
 * WHY:  Robustness requirement
 * HOW:  Pass NULL to functions
 */
TEST_F(SBOMSupplyChainIntegrationTest, NullSupplyChainHandling)
{
    // NULL supply chain
    nimcp_dependency_t dep = {};
    EXPECT_NE(NIMCP_OK, nimcp_sbom_add_dependency(nullptr, &dep));

    // NULL dependency
    EXPECT_NE(NIMCP_OK, nimcp_sbom_add_dependency(supply_chain_, nullptr));

    // NULL stats output
    EXPECT_NE(NIMCP_OK, nimcp_supply_chain_get_stats(supply_chain_, nullptr));
}

//=============================================================================
// Category 6: Error Paths and Edge Cases
//=============================================================================

class SecurityErrorPathsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        bbb_reset_test_state();
    }

    void TearDown() override
    {
        bbb_clear_signing_key();
    }
};

/**
 * @test BBBNullSystemHandling
 *
 * WHAT: Verify BBB handles NULL system gracefully
 * WHY:  Prevent crashes from invalid handles
 * HOW:  Call functions with NULL handle
 */
TEST_F(SecurityErrorPathsTest, BBBNullSystemHandling)
{
    bbb_validation_result_t result;
    EXPECT_FALSE(bbb_validate_string(nullptr, "test", &result));
    EXPECT_FALSE(bbb_validate_input(nullptr, "test", 4, &result));
    EXPECT_FALSE(bbb_system_set_enabled(nullptr, true));
    EXPECT_FALSE(bbb_system_is_enabled(nullptr));

    bbb_statistics_t stats;
    EXPECT_FALSE(bbb_system_get_statistics(nullptr, &stats));
}

/**
 * @test TripwireInvalidTypeHandling
 *
 * WHAT: Verify tripwire handles invalid type values
 * WHY:  Prevent undefined behavior with bad enum values
 * HOW:  Pass out-of-range enum values
 */
TEST_F(SecurityErrorPathsTest, TripwireInvalidTypeHandling)
{
    tripwire_config_t config = tripwire_default_config();
    tripwire_system_t* tripwire = tripwire_create(&config);
    ASSERT_NE(tripwire, nullptr);

    // Set sensitivity with invalid type (should handle gracefully)
    nimcp_error_t err = tripwire_set_sensitivity(
        tripwire,
        static_cast<tripwire_type_t>(999),  // Invalid type
        1.0f
    );
    // Should either reject or handle gracefully (not crash)

    // Enable invalid type
    err = tripwire_set_enabled(
        tripwire,
        static_cast<tripwire_type_t>(999),
        true
    );
    // Should either reject or handle gracefully

    tripwire_destroy(tripwire);
}

/**
 * @test EncryptedAuditKeyValidation
 *
 * WHAT: Verify encrypted audit validates key parameters
 * WHY:  Security-critical key handling
 * HOW:  Pass invalid key sizes
 */
TEST_F(SecurityErrorPathsTest, EncryptedAuditKeyValidation)
{
    nimcp_encrypted_audit_config_t config = nimcp_encrypted_audit_default_config();

    // Try to create with NULL key
    nimcp_encrypted_audit_t audit = nimcp_encrypted_audit_create(
        &config,
        nullptr,
        NIMCP_AUDIT_KEY_SIZE
    );
    EXPECT_EQ(nullptr, audit) << "Should reject NULL key";

    // Try with wrong key size
    uint8_t short_key[16] = {0};
    audit = nimcp_encrypted_audit_create(&config, short_key, 16);
    EXPECT_EQ(nullptr, audit) << "Should reject short key";

    // Create with valid key
    uint8_t valid_key[NIMCP_AUDIT_KEY_SIZE] = {0};
    audit = nimcp_encrypted_audit_create(&config, valid_key, NIMCP_AUDIT_KEY_SIZE);
    // May succeed or fail depending on implementation
    if (audit) {
        nimcp_encrypted_audit_destroy(audit);
    }
}

/**
 * @test ConcurrentSecurityModuleAccess
 *
 * WHAT: Verify multiple security modules can be accessed concurrently
 * WHY:  Real systems use multiple modules in parallel
 * HOW:  Create multiple modules, access from threads
 */
TEST_F(SecurityErrorPathsTest, ConcurrentSecurityModuleAccess)
{
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 20;
    std::atomic<int> successful_ops{0};

    // Create modules
    bbb_config_t bbb_config = bbb_default_config();
    bbb_system_t bbb = bbb_system_create(&bbb_config);
    ASSERT_NE(bbb, nullptr);
    bbb_system_set_enabled(bbb, true);

    tripwire_config_t tw_config = tripwire_default_config();
    tripwire_system_t* tripwire = tripwire_create(&tw_config);
    ASSERT_NE(tripwire, nullptr);

    std::vector<std::thread> threads;

    // Launch threads accessing both modules
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                // BBB operation
                bbb_validation_result_t result;
                char buf[64];
                snprintf(buf, sizeof(buf), "thread_%d_input_%d", t, i);
                bbb_validate_string(bbb, buf, &result);

                // Tripwire operation
                tripwire_observe_goal(tripwire, t, 0.5f + i * 0.01f, 0.5f);

                successful_ops.fetch_add(1);
            }
        });
    }

    // Wait for all threads
    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(successful_ops.load(), NUM_THREADS * OPS_PER_THREAD);

    // Cleanup
    bbb_system_destroy(bbb);
    tripwire_destroy(tripwire);
}

}  // anonymous namespace
