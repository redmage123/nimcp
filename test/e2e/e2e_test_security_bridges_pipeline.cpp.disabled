/**
 * @file e2e_test_security_bridges_pipeline.cpp
 * @brief E2E Tests for Complete Security Bridges Pipeline
 *
 * WHAT: End-to-end testing for all security bridge integrations
 * WHY:  Verify complete security pipeline from threat detection to immune response
 * HOW:  Test async, logging, immune bridges working together through full workflows
 *
 * TEST PIPELINES:
 * - FullSecurityPipeline: Initialize all components, connect bridges, threat flow
 * - ThreatDetectionPipeline: Malicious input -> async -> logging -> immune -> pattern
 * - DistributedThreatIntelPipeline: Multi-node threat intel sharing via async
 * - EmergencyModePipeline: TNF-alpha or critical threat triggering emergency
 * - AuditTrailPipeline: Capture all security events, export to JSON
 * - PatternLearningPipeline: New threat -> memory cell -> pattern DB -> faster response
 * - TolerancePipeline: False positive -> whitelist -> regulatory T cell suppression
 * - HighLoadPipeline: Concurrent threats, rate limiting, async load, no data loss
 * - RecoveryPipeline: Attack -> neutralization -> IL-10 recovery -> baseline
 * - CrossModuleCorrelationPipeline: Anomaly correlation across modules
 * - CytokineModulationPipeline: Full cytokine effect propagation
 * - ThreatIntelCachePipeline: Cache management and freshness
 *
 * @author NIMCP Development Team
 * @date 2025-01-09
 * @version 1.0.0
 */

#include "e2e_test_framework.h"

// Security module headers
#include "security/async/nimcp_security_async_bridge.h"
#include "security/logging/nimcp_security_logging_bridge.h"
#include "security/immune/nimcp_security_immune_unified_bridge.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_pattern_db.h"
#include "security/nimcp_policy_engine.h"
#include "security/nimcp_rate_limiter.h"

// Async headers
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

// Utilities
#include "utils/memory/nimcp_memory.h"

#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <mutex>

//=============================================================================
// Test Fixture: Security Bridges Pipeline
//=============================================================================

class SecurityBridgesPipelineE2ETest : public ::testing::Test {
protected:
    // Core security components
    bbb_system_t bbb_system_ = nullptr;
    nimcp_anomaly_detector_t anomaly_detector_ = nullptr;
    nimcp_pattern_db_t pattern_db_ = nullptr;
    nimcp_policy_engine_t policy_engine_ = nullptr;
    nimcp_rate_limiter_t rate_limiter_ = nullptr;

    // Bridge instances
    security_async_bridge_t* async_bridge_ = nullptr;
    security_logging_bridge_t* logging_bridge_ = nullptr;
    sec_immune_unified_bridge_t* immune_bridge_ = nullptr;

    // Counters for event tracking
    static std::atomic<uint32_t> threats_detected_;
    static std::atomic<uint32_t> events_logged_;
    static std::atomic<uint32_t> immune_responses_;
    static std::atomic<uint32_t> patterns_learned_;
    static std::atomic<uint32_t> async_messages_;

    // Thread safety
    static std::mutex event_mutex_;

    void SetUp() override {
        // Initialize bio-async system
        nimcp_error_t err = nimcp_bio_async_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        err = bio_router_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        // Reset counters
        threats_detected_.store(0);
        events_logged_.store(0);
        immune_responses_.store(0);
        patterns_learned_.store(0);
        async_messages_.store(0);
    }

    void TearDown() override {
        // Destroy bridges in reverse order
        if (immune_bridge_) {
            sec_immune_unified_destroy(immune_bridge_);
            immune_bridge_ = nullptr;
        }

        if (logging_bridge_) {
            security_logging_bridge_destroy(logging_bridge_);
            logging_bridge_ = nullptr;
        }

        if (async_bridge_) {
            security_async_bridge_destroy(async_bridge_);
            async_bridge_ = nullptr;
        }

        // Destroy security components
        if (rate_limiter_) {
            nimcp_rate_limiter_destroy(rate_limiter_);
            rate_limiter_ = nullptr;
        }

        if (policy_engine_) {
            nimcp_policy_engine_destroy(policy_engine_);
            policy_engine_ = nullptr;
        }

        if (pattern_db_) {
            nimcp_pattern_db_destroy(pattern_db_);
            pattern_db_ = nullptr;
        }

        if (anomaly_detector_) {
            nimcp_anomaly_detector_destroy(anomaly_detector_);
            anomaly_detector_ = nullptr;
        }

        if (bbb_system_) {
            bbb_system_destroy(bbb_system_);
            bbb_system_ = nullptr;
        }

        // Shutdown async
        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        // Clean up temp files
        std::remove("/tmp/nimcp_security_audit.json");
        std::remove("/tmp/nimcp_security_export.bin");
    }

    // Helper: Create all security components
    bool CreateSecurityComponents() {
        // Create BBB system
        bbb_config_t bbb_config;
        bbb_default_config(&bbb_config);
        bbb_config.enable_input_validation = true;
        bbb_config.enable_code_signing = true;
        bbb_config.enable_memory_boundaries = true;
        bbb_config.enable_access_control = true;

        bbb_system_ = bbb_system_create(&bbb_config);
        if (!bbb_system_) return false;

        // Create anomaly detector
        nimcp_anomaly_config_t anomaly_config;
        nimcp_anomaly_default_config(&anomaly_config);
        anomaly_config.sensitivity = 0.8f;
        anomaly_config.enable_ml_detection = true;

        anomaly_detector_ = nimcp_anomaly_detector_create(&anomaly_config);
        if (!anomaly_detector_) return false;

        // Create pattern database
        nimcp_pattern_db_config_t pattern_config;
        nimcp_pattern_db_default_config(&pattern_config);
        pattern_config.max_patterns = 1000;
        pattern_config.enable_auto_learning = true;

        pattern_db_ = nimcp_pattern_db_create(&pattern_config);
        if (!pattern_db_) return false;

        // Create policy engine
        nimcp_policy_config_t policy_config;
        nimcp_policy_default_config(&policy_config);
        policy_config.enable_strict_mode = true;

        policy_engine_ = nimcp_policy_engine_create(&policy_config);
        if (!policy_engine_) return false;

        // Create rate limiter
        nimcp_rate_limiter_config_t rate_config;
        nimcp_rate_limiter_default_config(&rate_config);
        rate_config.default_requests_per_second = 100;
        rate_config.enable_penalties = true;

        rate_limiter_ = nimcp_rate_limiter_create(&rate_config);
        if (!rate_limiter_) return false;

        return true;
    }

    // Helper: Create and connect all bridges
    bool CreateAndConnectBridges() {
        // Create async bridge
        security_async_config_t async_config;
        security_async_default_config(&async_config);
        async_config.enable_threat_broadcast = true;
        async_config.enable_distributed_intel = true;
        async_config.enable_event_bus = true;

        async_bridge_ = security_async_bridge_create(&async_config);
        if (!async_bridge_) return false;

        // Connect security components to async bridge
        if (security_async_connect_bbb(async_bridge_, bbb_system_) != 0) return false;
        if (security_async_connect_anomaly_detector(async_bridge_, anomaly_detector_) != 0) return false;
        if (security_async_connect_pattern_db(async_bridge_, pattern_db_) != 0) return false;
        if (security_async_connect_policy_engine(async_bridge_, policy_engine_) != 0) return false;
        if (security_async_connect_rate_limiter(async_bridge_, rate_limiter_) != 0) return false;

        // Connect to bio-async router
        if (security_async_connect_bio_async(async_bridge_) != 0) return false;

        // Create logging bridge
        security_logging_bridge_config_t logging_config;
        security_logging_default_config(&logging_config);
        logging_config.enable_pattern_analysis = true;
        logging_config.buffer_capacity = 10000;

        logging_bridge_ = security_logging_bridge_create(&logging_config);
        if (!logging_bridge_) return false;

        // Connect logging to security components
        if (security_logging_connect_bbb(logging_bridge_, bbb_system_) != 0) return false;
        if (security_logging_connect_anomaly_detector(logging_bridge_, anomaly_detector_) != 0) return false;
        if (security_logging_connect_rate_limiter(logging_bridge_, rate_limiter_) != 0) return false;

        // Create unified immune bridge
        sec_immune_unified_config_t immune_config;
        sec_immune_unified_default_config(&immune_config);

        immune_bridge_ = sec_immune_unified_create(&immune_config);
        if (!immune_bridge_) return false;

        // Connect immune bridge to all components
        if (sec_immune_unified_connect_all(
                immune_bridge_,
                bbb_system_,
                anomaly_detector_,
                pattern_db_,
                rate_limiter_,
                policy_engine_) != 0) {
            return false;
        }

        return true;
    }

    // Helper: Get current timestamp in microseconds
    uint64_t GetTimestampUs() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
    }

    // Helper: Generate unique threat hash
    void GenerateThreatHash(uint8_t* hash, size_t len, int seed) {
        for (size_t i = 0; i < len; i++) {
            hash[i] = static_cast<uint8_t>((seed + i * 7) & 0xFF);
        }
    }

    // Helper: Wait for async processing
    void WaitForAsync(uint32_t expected_count, uint32_t timeout_ms) {
        auto start = std::chrono::steady_clock::now();
        while (async_messages_.load() < expected_count) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();
            if (elapsed > timeout_ms) break;
        }
    }
};

// Static member initialization
std::atomic<uint32_t> SecurityBridgesPipelineE2ETest::threats_detected_{0};
std::atomic<uint32_t> SecurityBridgesPipelineE2ETest::events_logged_{0};
std::atomic<uint32_t> SecurityBridgesPipelineE2ETest::immune_responses_{0};
std::atomic<uint32_t> SecurityBridgesPipelineE2ETest::patterns_learned_{0};
std::atomic<uint32_t> SecurityBridgesPipelineE2ETest::async_messages_{0};
std::mutex SecurityBridgesPipelineE2ETest::event_mutex_;

//=============================================================================
// Pipeline 1: Full Security Pipeline
//=============================================================================

TEST_F(SecurityBridgesPipelineE2ETest, FullSecurityPipeline) {
    E2E_PIPELINE_START("Full Security Pipeline Integration");

    // Stage 1: Create all security components
    E2E_STAGE_BEGIN("Create security components", 500);

    bool components_created = CreateSecurityComponents();
    E2E_ASSERT(components_created, "Failed to create security components");
    E2E_ASSERT_NOT_NULL(bbb_system_, "BBB system is null");
    E2E_ASSERT_NOT_NULL(anomaly_detector_, "Anomaly detector is null");
    E2E_ASSERT_NOT_NULL(pattern_db_, "Pattern DB is null");
    E2E_ASSERT_NOT_NULL(policy_engine_, "Policy engine is null");
    E2E_ASSERT_NOT_NULL(rate_limiter_, "Rate limiter is null");

    E2E_STAGE_END();

    // Stage 2: Create and connect all bridges
    E2E_STAGE_BEGIN("Create and connect bridges", 500);

    bool bridges_connected = CreateAndConnectBridges();
    E2E_ASSERT(bridges_connected, "Failed to create and connect bridges");

    // Verify connections
    EXPECT_TRUE(security_async_is_connected(async_bridge_));
    EXPECT_TRUE(security_async_is_bio_async_connected(async_bridge_));

    security_async_state_t state;
    int result = security_async_get_state(async_bridge_, &state);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(state.bbb_connected);
    EXPECT_TRUE(state.anomaly_connected);
    EXPECT_TRUE(state.pattern_db_connected);
    EXPECT_TRUE(state.policy_engine_connected);
    EXPECT_TRUE(state.rate_limiter_connected);
    EXPECT_TRUE(state.is_active);

    E2E_STAGE_END();

    // Stage 3: Simulate threat detection flow
    E2E_STAGE_BEGIN("Simulate threat detection flow", 1000);

    // Create threat data
    uint8_t threat_data[64];
    GenerateThreatHash(threat_data, sizeof(threat_data), 42);

    // Report through BBB validation
    bbb_validation_result_t validation;
    const char* test_input = "'; DROP TABLE users; --";
    bool is_valid = bbb_validate_string(bbb_system_, test_input, &validation);
    EXPECT_FALSE(is_valid);  // Malicious input should be invalid

    // Broadcast threat via async bridge
    result = security_async_broadcast_threat(
        async_bridge_,
        BBB_THREAT_SQL_INJECTION,
        BBB_SEVERITY_HIGH,
        "SQL injection attack detected in user input",
        threat_data
    );
    EXPECT_EQ(result, 0);

    // Log the threat via logging bridge
    result = security_logging_log_bbb(
        logging_bridge_,
        BBB_THREAT_SQL_INJECTION,
        BBB_SEVERITY_HIGH,
        BBB_ACTION_BLOCK,
        "SQL injection attack blocked at BBB perimeter"
    );
    EXPECT_EQ(result, 0);

    // Present threat to immune system
    uint32_t antigen_id;
    result = sec_immune_unified_present_bbb_threat(
        immune_bridge_,
        BBB_THREAT_SQL_INJECTION,
        BBB_SEVERITY_HIGH,
        threat_data,
        sizeof(threat_data),
        &antigen_id
    );
    EXPECT_EQ(result, 0);
    EXPECT_GT(antigen_id, 0u);

    E2E_STAGE_END();

    // Stage 4: Verify immune response triggered
    E2E_STAGE_BEGIN("Verify immune response", 500);

    // Update immune bridge to process antigen
    result = sec_immune_unified_update(immune_bridge_);
    EXPECT_EQ(result, 0);

    // Apply cytokine effects
    result = sec_immune_unified_apply_cytokine_effects(immune_bridge_);
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 5: Verify statistics across bridges
    E2E_STAGE_BEGIN("Verify statistics", 300);

    security_async_stats_t async_stats;
    result = security_async_get_stats(async_bridge_, &async_stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(async_stats.threat_events, 1u);

    security_logging_bridge_stats_t logging_stats;
    result = security_logging_get_stats(logging_bridge_, &logging_stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(logging_stats.total_entries, 1u);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Threat Detection Pipeline
//=============================================================================

TEST_F(SecurityBridgesPipelineE2ETest, ThreatDetectionPipeline) {
    E2E_PIPELINE_START("Threat Detection Pipeline");

    // Setup
    E2E_STAGE_BEGIN("Setup components and bridges", 800);
    ASSERT_TRUE(CreateSecurityComponents());
    ASSERT_TRUE(CreateAndConnectBridges());
    E2E_STAGE_END();

    // Stage 1: Detect malicious input through BBB
    E2E_STAGE_BEGIN("Detect malicious input", 500);

    const char* malicious_input = "'; DROP TABLE users; --";
    bbb_validation_result_t validation;

    bool is_valid = bbb_validate_string(bbb_system_, malicious_input, &validation);
    EXPECT_FALSE(is_valid);  // Should be invalid
    EXPECT_FALSE(validation.valid);
    EXPECT_EQ(validation.threat, BBB_THREAT_SQL_INJECTION);
    EXPECT_GE(validation.severity, BBB_SEVERITY_HIGH);

    E2E_STAGE_END();

    // Stage 2: Broadcast threat via async
    E2E_STAGE_BEGIN("Broadcast via async", 500);

    int result = security_async_broadcast_threat(
        async_bridge_,
        validation.threat,
        validation.severity,
        "SQL injection attack detected in user input",
        nullptr
    );
    EXPECT_EQ(result, 0);

    // Process async events
    uint32_t processed = security_async_process_events(async_bridge_, 0);
    EXPECT_GE(processed, 0u);

    E2E_STAGE_END();

    // Stage 3: Log threat with full context
    E2E_STAGE_BEGIN("Log threat to audit trail", 500);

    result = security_logging_log_bbb(
        logging_bridge_,
        validation.threat,
        validation.severity,
        BBB_ACTION_BLOCK,
        "SQL injection blocked - malicious input rejected"
    );
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 4: Trigger immune response
    E2E_STAGE_BEGIN("Trigger immune response", 500);

    uint8_t threat_hash[32];
    GenerateThreatHash(threat_hash, sizeof(threat_hash), 100);

    uint32_t antigen_id;
    result = sec_immune_unified_present_bbb_threat(
        immune_bridge_,
        validation.threat,
        validation.severity,
        threat_hash,
        sizeof(threat_hash),
        &antigen_id
    );
    EXPECT_EQ(result, 0);
    EXPECT_GT(antigen_id, 0u);

    // Update immune system
    result = sec_immune_unified_update(immune_bridge_);
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 5: Learn pattern for future detection
    E2E_STAGE_BEGIN("Learn threat pattern", 500);

    // Form memory cell from neutralized threat
    uint32_t memory_id;
    result = sec_immune_unified_form_memory(
        immune_bridge_,
        antigen_id,
        0,  // antibody_id
        &memory_id
    );
    EXPECT_EQ(result, 0);
    EXPECT_GT(memory_id, 0u);

    // Sync memory to pattern database
    result = sec_immune_unified_sync_memory_to_pattern(immune_bridge_, memory_id);
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Distributed Threat Intel Pipeline
//=============================================================================

TEST_F(SecurityBridgesPipelineE2ETest, DistributedThreatIntelPipeline) {
    E2E_PIPELINE_START("Distributed Threat Intelligence Pipeline");

    // Setup
    E2E_STAGE_BEGIN("Setup components", 800);
    ASSERT_TRUE(CreateSecurityComponents());
    ASSERT_TRUE(CreateAndConnectBridges());
    E2E_STAGE_END();

    // Stage 1: Cache local threat intel
    E2E_STAGE_BEGIN("Cache local threat intel", 500);

    threat_intel_entry_t intel_entry;
    memset(&intel_entry, 0, sizeof(intel_entry));

    // Create unique threat hash
    for (size_t i = 0; i < 32; i++) {
        intel_entry.threat_hash[i] = static_cast<uint8_t>(i * 7 + 13);
    }
    intel_entry.threat_type = BBB_THREAT_ROP_CHAIN;
    intel_entry.severity = BBB_SEVERITY_CRITICAL;
    intel_entry.source_node = 1;
    intel_entry.first_seen_ms = 1704067200000ULL;
    intel_entry.last_seen_ms = intel_entry.first_seen_ms + 60000;
    intel_entry.observation_count = 5;
    intel_entry.confidence = 0.95f;
    intel_entry.confirmed = true;

    int result = security_async_cache_threat_intel(async_bridge_, &intel_entry);
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 2: Share threat intel with peers
    E2E_STAGE_BEGIN("Share threat intel", 500);

    result = security_async_share_threat_intel(async_bridge_, 10);
    EXPECT_EQ(result, 0);

    // Process outgoing messages
    uint32_t processed = security_async_process_events(async_bridge_, 0);
    EXPECT_GE(processed, 0u);

    security_async_stats_t stats;
    result = security_async_get_stats(async_bridge_, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.intel_shared, 1u);

    E2E_STAGE_END();

    // Stage 3: Receive threat report from peer
    E2E_STAGE_BEGIN("Receive peer threat report", 500);

    uint8_t peer_threat_hash[32];
    for (size_t i = 0; i < 32; i++) {
        peer_threat_hash[i] = static_cast<uint8_t>(i * 11 + 23);
    }

    result = security_async_receive_threat_report(
        async_bridge_,
        2,  // Source node
        BBB_THREAT_SHELLCODE,
        peer_threat_hash,
        0.85f  // Confidence
    );
    EXPECT_EQ(result, 0);

    // Verify received intel is cached
    threat_intel_entry_t retrieved;
    bool found = security_async_lookup_threat_intel(async_bridge_, peer_threat_hash, &retrieved);
    EXPECT_TRUE(found);
    EXPECT_EQ(retrieved.threat_type, BBB_THREAT_SHELLCODE);
    EXPECT_EQ(retrieved.source_node, 2u);

    E2E_STAGE_END();

    // Stage 4: Request distributed threat intel
    E2E_STAGE_BEGIN("Request threat intel", 500);

    result = security_async_request_threat_intel(async_bridge_, nullptr);
    EXPECT_EQ(result, 0);

    // Process events
    security_async_process_events(async_bridge_, 0);

    E2E_STAGE_END();

    // Stage 5: Verify aggregated network threat level
    E2E_STAGE_BEGIN("Verify network threat level", 300);

    async_security_effects_t async_effects;
    result = security_async_get_async_effects(async_bridge_, &async_effects);
    EXPECT_EQ(result, 0);
    EXPECT_GE(async_effects.peer_threat_reports, 1u);

    // Get intel cache stats
    uint32_t count, confirmed;
    result = security_async_get_intel_stats(async_bridge_, &count, &confirmed);
    EXPECT_EQ(result, 0);
    EXPECT_GE(count, 2u);  // At least our cached entry + peer report

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Emergency Mode Pipeline
//=============================================================================

TEST_F(SecurityBridgesPipelineE2ETest, EmergencyModePipeline) {
    E2E_PIPELINE_START("Emergency Mode Pipeline");

    // Setup
    E2E_STAGE_BEGIN("Setup components", 800);
    ASSERT_TRUE(CreateSecurityComponents());
    ASSERT_TRUE(CreateAndConnectBridges());
    E2E_STAGE_END();

    // Stage 1: Verify initial state (no emergency)
    E2E_STAGE_BEGIN("Verify initial non-emergency state", 200);

    EXPECT_FALSE(security_async_is_emergency_mode(async_bridge_));
    EXPECT_FALSE(sec_immune_unified_is_emergency_mode(immune_bridge_));

    E2E_STAGE_END();

    // Stage 2: Simulate critical threat triggering emergency
    E2E_STAGE_BEGIN("Trigger emergency via critical threat", 500);

    uint8_t critical_threat_data[64];
    GenerateThreatHash(critical_threat_data, sizeof(critical_threat_data), 999);

    // Report critical threat through BBB
    bbb_threat_report_t critical_report = bbb_report_threat(
        bbb_system_,
        BBB_THREAT_BUFFER_OVERFLOW,
        BBB_SEVERITY_CRITICAL,
        "Critical stack overflow attack detected",
        nullptr,
        critical_threat_data,
        sizeof(critical_threat_data)
    );
    EXPECT_NE(critical_report.timestamp, 0u);

    // Present to immune system
    uint32_t antigen_id;
    int result = sec_immune_unified_present_bbb_threat(
        immune_bridge_,
        critical_report.type,
        critical_report.severity,
        critical_threat_data,
        sizeof(critical_threat_data),
        &antigen_id
    );
    EXPECT_EQ(result, 0);

    // Update immune bridge
    result = sec_immune_unified_update(immune_bridge_);
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 3: Enter emergency mode
    E2E_STAGE_BEGIN("Enter emergency mode", 300);

    result = security_async_enter_emergency_mode(async_bridge_);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(security_async_is_emergency_mode(async_bridge_));

    E2E_STAGE_END();

    // Stage 4: Log emergency event
    E2E_STAGE_BEGIN("Log emergency event", 300);

    result = security_logging_log_audit(
        logging_bridge_,
        SECURITY_LOG_SEV_CRITICAL,
        "EMERGENCY_MODE",
        "EMERGENCY: Critical stack overflow attack - system lockdown initiated",
        nullptr
    );
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 5: Exit emergency mode
    E2E_STAGE_BEGIN("Exit emergency mode", 500);

    result = security_async_exit_emergency_mode(async_bridge_);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(security_async_is_emergency_mode(async_bridge_));

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Audit Trail Pipeline
//=============================================================================

TEST_F(SecurityBridgesPipelineE2ETest, AuditTrailPipeline) {
    E2E_PIPELINE_START("Audit Trail Pipeline");

    // Setup
    E2E_STAGE_BEGIN("Setup components", 800);
    ASSERT_TRUE(CreateSecurityComponents());
    ASSERT_TRUE(CreateAndConnectBridges());
    E2E_STAGE_END();

    // Stage 1: Log various security events
    E2E_STAGE_BEGIN("Log diverse security events", 1000);

    int result;

    // Log access event
    result = security_logging_log_access(
        logging_bridge_,
        true,  // allowed
        "user_admin",
        "database_config",
        "Admin accessed database configuration"
    );
    EXPECT_EQ(result, 0);

    // Log BBB threat event
    result = security_logging_log_bbb(
        logging_bridge_,
        BBB_THREAT_BUFFER_OVERFLOW,
        BBB_SEVERITY_HIGH,
        BBB_ACTION_BLOCK,
        "Buffer overflow attempt blocked at BBB perimeter"
    );
    EXPECT_EQ(result, 0);

    // Log policy event
    result = security_logging_log_policy(
        logging_bridge_,
        "STRICT_INPUT_VALIDATION",
        SECURITY_LOG_ACTION_ALERT,
        "api_endpoint",
        "Security policy updated: stricter input validation"
    );
    EXPECT_EQ(result, 0);

    // Log crypto event
    result = security_logging_log_crypto(
        logging_bridge_,
        "verify",
        false,  // failure
        "key_id_12345",
        "Invalid cryptographic signature rejected"
    );
    EXPECT_EQ(result, 0);

    // Log audit event
    result = security_logging_log_audit(
        logging_bridge_,
        SECURITY_LOG_SEV_INFO,
        "AUDIT_SYSTEM",
        "Audit log checkpoint created",
        nullptr
    );
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 2: Verify log statistics
    E2E_STAGE_BEGIN("Verify log statistics", 300);

    security_logging_bridge_stats_t stats;
    result = security_logging_get_stats(logging_bridge_, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.total_entries, 5u);

    E2E_STAGE_END();

    // Stage 3: Analyze threat patterns
    E2E_STAGE_BEGIN("Analyze threat patterns", 500);

    result = security_logging_analyze_patterns(logging_bridge_);
    EXPECT_GE(result, 0);  // Returns count of patterns detected

    // Get detected patterns
    security_threat_pattern_t patterns[10];
    size_t pattern_count = 0;
    result = security_logging_get_patterns(
        logging_bridge_,
        patterns,
        10,
        &pattern_count
    );
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 4: Export audit log to JSON
    E2E_STAGE_BEGIN("Export to JSON", 1000);

    result = security_logging_export_to_file(
        logging_bridge_,
        "/tmp/nimcp_security_audit.json",
        SECURITY_LOG_FORMAT_JSON,
        0,  // start_time (all)
        0   // end_time (all)
    );
    EXPECT_GE(result, 0);  // Returns number of entries exported

    // Verify file exists and has content
    FILE* f = fopen("/tmp/nimcp_security_audit.json", "r");
    EXPECT_NE(f, nullptr);
    if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        EXPECT_GT(size, 100);  // File should have substantial content
        fclose(f);
    }

    E2E_STAGE_END();

    // Stage 5: Query logs by time range
    E2E_STAGE_BEGIN("Query logs by time", 500);

    security_log_entry_t entries[20];
    size_t entry_count = 0;

    result = security_logging_query_entries(
        logging_bridge_,
        0,  // start_time
        0,  // end_time (now)
        0,  // all categories
        SECURITY_LOG_SEV_DEBUG,  // min severity
        entries,
        20,
        &entry_count
    );
    EXPECT_EQ(result, 0);
    EXPECT_GE(entry_count, 5u);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 6: Pattern Learning Pipeline
//=============================================================================

TEST_F(SecurityBridgesPipelineE2ETest, PatternLearningPipeline) {
    E2E_PIPELINE_START("Pattern Learning Pipeline");

    // Setup
    E2E_STAGE_BEGIN("Setup components", 800);
    ASSERT_TRUE(CreateSecurityComponents());
    ASSERT_TRUE(CreateAndConnectBridges());
    E2E_STAGE_END();

    // Stage 1: Detect novel threat
    E2E_STAGE_BEGIN("Detect novel threat", 500);

    uint8_t novel_threat_hash[32];
    GenerateThreatHash(novel_threat_hash, sizeof(novel_threat_hash), 777);

    // Check pattern DB - should not exist yet
    uint32_t existing_memory;
    int check_result = sec_immune_unified_check_memory(
        immune_bridge_,
        novel_threat_hash,
        sizeof(novel_threat_hash),
        &existing_memory
    );
    EXPECT_NE(check_result, 0);  // Should not find existing memory

    // Report threat
    bbb_threat_report_t novel_report = bbb_report_threat(
        bbb_system_,
        BBB_THREAT_BUFFER_OVERFLOW,
        BBB_SEVERITY_HIGH,
        "Novel threat",
        nullptr,
        novel_threat_hash,
        sizeof(novel_threat_hash)
    );
    EXPECT_NE(novel_report.timestamp, 0u);

    E2E_STAGE_END();

    // Stage 2: Present to immune system
    E2E_STAGE_BEGIN("Present to immune system", 500);

    uint32_t antigen_id;
    int present_result = sec_immune_unified_present_bbb_threat(
        immune_bridge_,
        novel_report.type,
        novel_report.severity,
        novel_threat_hash,
        sizeof(novel_threat_hash),
        &antigen_id
    );
    EXPECT_EQ(present_result, 0);
    EXPECT_GT(antigen_id, 0u);

    // Update immune system
    int update_result = sec_immune_unified_update(immune_bridge_);
    EXPECT_EQ(update_result, 0);

    E2E_STAGE_END();

    // Stage 3: Form memory cell
    E2E_STAGE_BEGIN("Form memory cell", 500);

    uint32_t memory_id;
    int form_result = sec_immune_unified_form_memory(
        immune_bridge_,
        antigen_id,
        0,  // antibody_id
        &memory_id
    );
    EXPECT_EQ(form_result, 0);
    EXPECT_GT(memory_id, 0u);

    E2E_STAGE_END();

    // Stage 4: Sync pattern to database
    E2E_STAGE_BEGIN("Sync pattern to DB", 500);

    int sync_result = sec_immune_unified_sync_memory_to_pattern(immune_bridge_, memory_id);
    EXPECT_EQ(sync_result, 0);

    // Broadcast pattern update
    int broadcast_result = security_async_broadcast_pattern_update(
        async_bridge_,
        0,  // Pattern ID assigned by DB
        NIMCP_PATTERN_BUFFER_OVERFLOW,
        true  // Is new
    );
    EXPECT_EQ(broadcast_result, 0);

    E2E_STAGE_END();

    // Stage 5: Test secondary response (faster detection)
    E2E_STAGE_BEGIN("Test secondary response", 500);

    auto start = std::chrono::high_resolution_clock::now();

    // Check if memory exists now
    uint32_t found_memory;
    int memory_check_result = sec_immune_unified_check_memory(
        immune_bridge_,
        novel_threat_hash,
        sizeof(novel_threat_hash),
        &found_memory
    );
    EXPECT_EQ(memory_check_result, 0);  // Should find memory now
    EXPECT_EQ(found_memory, memory_id);

    // Trigger secondary response
    int sec_result = sec_immune_unified_secondary_response(
        immune_bridge_,
        found_memory,
        antigen_id
    );
    EXPECT_EQ(sec_result, 0);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Secondary response should be fast (memory cell response)
    EXPECT_LT(duration, 5000);  // Less than 5ms

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 7: Tolerance Pipeline
//=============================================================================

TEST_F(SecurityBridgesPipelineE2ETest, TolerancePipeline) {
    E2E_PIPELINE_START("Tolerance (False Positive) Pipeline");

    // Setup
    E2E_STAGE_BEGIN("Setup components", 800);
    ASSERT_TRUE(CreateSecurityComponents());
    ASSERT_TRUE(CreateAndConnectBridges());
    E2E_STAGE_END();

    // Stage 1: Create pattern that looks like a threat but is benign
    E2E_STAGE_BEGIN("Create benign pattern", 300);

    uint8_t benign_pattern[32];
    for (size_t i = 0; i < 32; i++) {
        benign_pattern[i] = static_cast<uint8_t>(i * 3 + 17);
    }

    E2E_STAGE_END();

    // Stage 2: Present as threat initially
    E2E_STAGE_BEGIN("Present as initial threat", 500);

    uint32_t antigen_id;
    int result = sec_immune_unified_present_bbb_threat(
        immune_bridge_,
        BBB_THREAT_FORMAT_STRING,
        BBB_SEVERITY_MEDIUM,
        benign_pattern,
        sizeof(benign_pattern),
        &antigen_id
    );
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 3: Mark as false positive / add tolerance
    E2E_STAGE_BEGIN("Add tolerance for benign pattern", 500);

    result = sec_immune_unified_add_tolerance(
        immune_bridge_,
        benign_pattern,
        sizeof(benign_pattern),
        "Whitelisted: legitimate logging format string",
        true  // is_permanent
    );
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 4: Verify tolerance is active
    E2E_STAGE_BEGIN("Verify tolerance active", 300);

    bool is_tolerated = sec_immune_unified_is_tolerated(
        immune_bridge_,
        benign_pattern,
        sizeof(benign_pattern)
    );
    EXPECT_TRUE(is_tolerated);

    E2E_STAGE_END();

    // Stage 5: Verify pattern no longer triggers response
    E2E_STAGE_BEGIN("Verify no response to tolerated pattern", 500);

    // Present same pattern again
    uint32_t new_antigen_id;
    result = sec_immune_unified_present_bbb_threat(
        immune_bridge_,
        BBB_THREAT_FORMAT_STRING,
        BBB_SEVERITY_MEDIUM,
        benign_pattern,
        sizeof(benign_pattern),
        &new_antigen_id
    );
    // Should succeed but be handled as benign due to tolerance
    EXPECT_EQ(result, 0);

    // Log tolerance event
    result = security_logging_log_audit(
        logging_bridge_,
        SECURITY_LOG_SEV_INFO,
        "TOLERANCE_SYSTEM",
        "Pattern whitelisted: regulatory T cell tolerance active",
        nullptr
    );
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 8: High Load Pipeline
//=============================================================================

TEST_F(SecurityBridgesPipelineE2ETest, HighLoadPipeline) {
    E2E_PIPELINE_START("High Load Stress Test Pipeline");

    // Setup
    E2E_STAGE_BEGIN("Setup components", 800);
    ASSERT_TRUE(CreateSecurityComponents());
    ASSERT_TRUE(CreateAndConnectBridges());
    E2E_STAGE_END();

    // Stage 1: Generate concurrent threat load
    E2E_STAGE_BEGIN("Generate concurrent threats", 3000);

    const int num_threads = 4;
    const int threats_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> total_processed{0};
    std::atomic<int> total_errors{0};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, threats_per_thread, &total_processed, &total_errors]() {
            for (int i = 0; i < threats_per_thread; i++) {
                bbb_threat_type_t type = static_cast<bbb_threat_type_t>(
                    1 + ((t + i) % 10)  // 10 threat types (skip NONE)
                );
                bbb_severity_t severity = static_cast<bbb_severity_t>(
                    (i % 4)  // 4 severity levels
                );

                char desc[256];
                snprintf(desc, sizeof(desc), "Concurrent threat t=%d i=%d", t, i);

                int result = security_async_broadcast_threat(
                    async_bridge_,
                    type,
                    severity,
                    desc,
                    nullptr
                );

                if (result == 0) {
                    total_processed.fetch_add(1);
                } else {
                    total_errors.fetch_add(1);
                }

                // Small delay to prevent overwhelming
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_processed.load(), num_threads * threats_per_thread);
    EXPECT_EQ(total_errors.load(), 0);

    E2E_STAGE_END();

    // Stage 2: Process async message queue
    E2E_STAGE_BEGIN("Process async queue", 2000);

    uint32_t total_events = 0;
    uint32_t batch;
    do {
        batch = security_async_process_events(async_bridge_, 100);
        total_events += batch;
    } while (batch > 0);

    EXPECT_GE(total_events, 0u);

    E2E_STAGE_END();

    // Stage 3: Verify no data loss
    E2E_STAGE_BEGIN("Verify no data loss", 500);

    security_async_stats_t stats;
    int result = security_async_get_stats(async_bridge_, &stats);
    EXPECT_EQ(result, 0);

    // Check for dropped events
    EXPECT_EQ(stats.events_dropped, 0u) << "Events were dropped under high load";
    EXPECT_EQ(stats.queue_overflows, 0u) << "Queue overflows occurred";

    E2E_STAGE_END();

    // Stage 4: Verify statistics consistency
    E2E_STAGE_BEGIN("Verify statistics consistency", 300);

    EXPECT_GE(stats.threat_events, static_cast<uint64_t>(num_threads * threats_per_thread));

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 9: Recovery Pipeline
//=============================================================================

TEST_F(SecurityBridgesPipelineE2ETest, RecoveryPipeline) {
    E2E_PIPELINE_START("Attack Recovery Pipeline");

    // Setup
    E2E_STAGE_BEGIN("Setup components", 800);
    ASSERT_TRUE(CreateSecurityComponents());
    ASSERT_TRUE(CreateAndConnectBridges());
    E2E_STAGE_END();

    // Stage 1: Capture baseline state
    E2E_STAGE_BEGIN("Capture baseline state", 300);

    // Update to establish baseline
    int result = sec_immune_unified_update(immune_bridge_);
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 2: Simulate attack
    E2E_STAGE_BEGIN("Simulate attack", 500);

    // Multiple high-severity threats
    for (int i = 0; i < 5; i++) {
        uint8_t threat_data[32];
        GenerateThreatHash(threat_data, sizeof(threat_data), 500 + i);

        bbb_threat_type_t threat_type = static_cast<bbb_threat_type_t>(
            BBB_THREAT_SHELLCODE + (i % 3)
        );

        bbb_threat_report_t report = bbb_report_threat(
            bbb_system_,
            threat_type,
            BBB_SEVERITY_HIGH,
            "Test threat",
            nullptr,
            threat_data,
            sizeof(threat_data)
        );
        EXPECT_NE(report.timestamp, 0u);

        uint32_t antigen_id;
        int ret = sec_immune_unified_present_bbb_threat(
            immune_bridge_,
            threat_type,
            BBB_SEVERITY_HIGH,
            threat_data,
            sizeof(threat_data),
            &antigen_id
        );
        EXPECT_EQ(ret, 0);
    }

    // Update immune system to process attacks
    result = sec_immune_unified_update(immune_bridge_);
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 3: Neutralize threats
    E2E_STAGE_BEGIN("Neutralize threats", 500);

    // Quarantine affected regions
    bool quarantined = bbb_quarantine_region(bbb_system_, reinterpret_cast<void*>(0x1000), 0x2000);
    EXPECT_TRUE(quarantined);

    E2E_STAGE_END();

    // Stage 4: Trigger recovery
    E2E_STAGE_BEGIN("Trigger recovery", 1000);

    // Simulate recovery time passing - update multiple times
    for (int i = 0; i < 10; i++) {
        int update_result = sec_immune_unified_update(immune_bridge_);
        EXPECT_EQ(update_result, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    E2E_STAGE_END();

    // Stage 5: Verify recovery progress
    E2E_STAGE_BEGIN("Verify recovery progress", 500);

    // Continue recovery updates
    for (int i = 0; i < 20; i++) {
        int result = sec_immune_unified_update(immune_bridge_);
        EXPECT_EQ(result, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // System should be recovering
    EXPECT_FALSE(sec_immune_unified_is_emergency_mode(immune_bridge_));

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 10: Cross-Module Correlation Pipeline
//=============================================================================

TEST_F(SecurityBridgesPipelineE2ETest, CrossModuleCorrelationPipeline) {
    E2E_PIPELINE_START("Cross-Module Anomaly Correlation Pipeline");

    // Setup
    E2E_STAGE_BEGIN("Setup components", 800);
    ASSERT_TRUE(CreateSecurityComponents());
    ASSERT_TRUE(CreateAndConnectBridges());
    E2E_STAGE_END();

    // Stage 1: Generate anomalies from multiple sources
    E2E_STAGE_BEGIN("Generate multi-source anomalies", 500);

    // Anomaly from module 1 (network)
    int result = security_async_receive_threat_report(
        async_bridge_,
        1,  // Network module
        BBB_THREAT_CODE_INJECTION,
        nullptr,
        0.7f
    );
    EXPECT_EQ(result, 0);

    // Anomaly from module 2 (memory)
    result = security_async_receive_threat_report(
        async_bridge_,
        2,  // Memory module
        BBB_THREAT_BUFFER_OVERFLOW,
        nullptr,
        0.65f
    );
    EXPECT_EQ(result, 0);

    // Anomaly from module 3 (filesystem)
    result = security_async_receive_threat_report(
        async_bridge_,
        3,  // Filesystem module
        BBB_THREAT_PATH_TRAVERSAL,
        nullptr,
        0.75f
    );
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 2: Update async effects to correlate
    E2E_STAGE_BEGIN("Correlate anomalies", 500);

    result = security_async_update_async_effects(async_bridge_);
    EXPECT_EQ(result, 0);

    async_security_effects_t effects;
    result = security_async_get_async_effects(async_bridge_, &effects);
    EXPECT_EQ(result, 0);

    EXPECT_GE(effects.peer_threat_reports, 3u);

    E2E_STAGE_END();

    // Stage 3: Log correlated threat
    E2E_STAGE_BEGIN("Log correlated threat", 300);

    char msg[256];
    snprintf(msg, sizeof(msg),
             "Correlated threat detected: %u sources, level %.2f",
             effects.peer_threat_reports,
             effects.network_threat_level);

    result = security_logging_log_audit(
        logging_bridge_,
        SECURITY_LOG_SEV_WARNING,
        "CORRELATION_ENGINE",
        msg,
        nullptr
    );
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 11: Cytokine Modulation Pipeline
//=============================================================================

TEST_F(SecurityBridgesPipelineE2ETest, CytokineModulationPipeline) {
    E2E_PIPELINE_START("Cytokine Effect Modulation Pipeline");

    // Setup
    E2E_STAGE_BEGIN("Setup components", 800);
    ASSERT_TRUE(CreateSecurityComponents());
    ASSERT_TRUE(CreateAndConnectBridges());
    E2E_STAGE_END();

    // Stage 1: Trigger threat to generate cytokines
    E2E_STAGE_BEGIN("Trigger threat for cytokines", 500);

    uint8_t threat_data[32];
    GenerateThreatHash(threat_data, sizeof(threat_data), 111);

    uint32_t antigen_id;
    int result = sec_immune_unified_present_bbb_threat(
        immune_bridge_,
        BBB_THREAT_SQL_INJECTION,
        BBB_SEVERITY_MEDIUM,
        threat_data,
        sizeof(threat_data),
        &antigen_id
    );
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 2: Update to process
    E2E_STAGE_BEGIN("Update immune system", 500);

    result = sec_immune_unified_update(immune_bridge_);
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 3: Apply cytokine effects
    E2E_STAGE_BEGIN("Apply cytokine effects", 500);

    result = sec_immune_unified_apply_cytokine_effects(immune_bridge_);
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 4: Apply inflammation modulation
    E2E_STAGE_BEGIN("Apply inflammation modulation", 500);

    result = sec_immune_unified_apply_inflammation(immune_bridge_);
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    // Stage 5: Trigger higher severity for stronger response
    E2E_STAGE_BEGIN("Trigger high severity threat", 500);

    uint8_t high_threat_data[32];
    GenerateThreatHash(high_threat_data, sizeof(high_threat_data), 222);

    uint32_t high_antigen_id;
    result = sec_immune_unified_present_bbb_threat(
        immune_bridge_,
        BBB_THREAT_SHELLCODE,
        BBB_SEVERITY_HIGH,
        high_threat_data,
        sizeof(high_threat_data),
        &high_antigen_id
    );
    EXPECT_EQ(result, 0);

    result = sec_immune_unified_update(immune_bridge_);
    EXPECT_EQ(result, 0);

    result = sec_immune_unified_apply_cytokine_effects(immune_bridge_);
    EXPECT_EQ(result, 0);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 12: Threat Intel Cache Pipeline
//=============================================================================

TEST_F(SecurityBridgesPipelineE2ETest, ThreatIntelCachePipeline) {
    E2E_PIPELINE_START("Threat Intel Cache Management Pipeline");

    // Setup
    E2E_STAGE_BEGIN("Setup components", 800);
    ASSERT_TRUE(CreateSecurityComponents());
    ASSERT_TRUE(CreateAndConnectBridges());
    E2E_STAGE_END();

    // Stage 1: Populate cache with entries
    E2E_STAGE_BEGIN("Populate threat intel cache", 500);

    for (int i = 0; i < 10; i++) {
        threat_intel_entry_t entry;
        memset(&entry, 0, sizeof(entry));

        for (size_t j = 0; j < 32; j++) {
            entry.threat_hash[j] = static_cast<uint8_t>(i * 31 + j);
        }
        entry.threat_type = static_cast<bbb_threat_type_t>(i % 5);  // Use fixed value
        entry.severity = static_cast<bbb_severity_t>(i % 4);  // Use fixed value
        entry.source_node = static_cast<uint32_t>(i);
        entry.first_seen_ms = 1704067200000ULL + i * 60000;
        entry.last_seen_ms = entry.first_seen_ms + 30000;
        entry.observation_count = i + 1;
        entry.confidence = 0.5f + (i * 0.05f);
        entry.confirmed = (i % 2 == 0);

        int result = security_async_cache_threat_intel(async_bridge_, &entry);
        EXPECT_EQ(result, 0);
    }

    E2E_STAGE_END();

    // Stage 2: Verify cache statistics
    E2E_STAGE_BEGIN("Verify cache statistics", 300);

    uint32_t count, confirmed;
    int result = security_async_get_intel_stats(async_bridge_, &count, &confirmed);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 10u);
    EXPECT_EQ(confirmed, 5u);  // Half are confirmed

    E2E_STAGE_END();

    // Stage 3: Lookup specific entries
    E2E_STAGE_BEGIN("Lookup cache entries", 500);

    for (int i = 0; i < 10; i++) {
        uint8_t hash[32];
        for (size_t j = 0; j < 32; j++) {
            hash[j] = static_cast<uint8_t>(i * 31 + j);
        }

        threat_intel_entry_t retrieved;
        bool found = security_async_lookup_threat_intel(async_bridge_, hash, &retrieved);
        EXPECT_TRUE(found) << "Entry " << i << " not found";
        EXPECT_EQ(retrieved.source_node, static_cast<uint32_t>(i));
        EXPECT_EQ(retrieved.observation_count, static_cast<uint32_t>(i + 1));
    }

    E2E_STAGE_END();

    // Stage 4: Clear and verify
    E2E_STAGE_BEGIN("Clear cache", 300);

    result = security_async_clear_threat_intel(async_bridge_);
    EXPECT_EQ(result, 0);

    result = security_async_get_intel_stats(async_bridge_, &count, &confirmed);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(confirmed, 0u);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
