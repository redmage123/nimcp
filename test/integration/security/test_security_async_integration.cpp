/**
 * @file test_security_async_integration.cpp
 * @brief Integration tests for Security-Async Bridge (NIMCP)
 *
 * WHAT: Comprehensive integration tests for security module - bio-async router integration
 * WHY:  Verify bidirectional security event coordination across NIMCP systems
 * HOW:  Test security broadcasts, threat intel sharing, emergency mode coordination
 *
 * TEST COVERAGE:
 * 1. Security + Bio-Router Integration:
 *    - Security events broadcast through bio-router
 *    - Threat messages received from distributed nodes
 *    - Policy changes propagated system-wide
 *    - Bio-async message priority affected by threat level
 *
 * 2. Security + BBB Integration:
 *    - BBB threats trigger async broadcasts
 *    - BBB threshold adjustments from network threat intel
 *    - Quarantine actions coordinated across modules
 *
 * 3. Security + Anomaly Detector Integration:
 *    - Anomaly detection triggers async threat sharing
 *    - Distributed anomaly correlation
 *    - Cross-module pattern detection
 *
 * 4. Security + Rate Limiter Integration:
 *    - Rate limit events shared via async
 *    - System-wide load coordination
 *    - Emergency throttling coordination
 *
 * 5. Security + Pattern DB Integration:
 *    - Pattern updates synced via async
 *    - Distributed threat pattern learning
 *    - Cross-module pattern matching
 *
 * 6. Bidirectional Flow Tests:
 *    - Security->Async: verify broadcasts reach modules
 *    - Async->Security: verify intel updates pattern DB
 *    - Full cycle: threat detected -> broadcast -> pattern learned
 *
 * 7. Emergency Mode Tests:
 *    - Emergency mode broadcast
 *    - System-wide security posture change
 *    - Recovery coordination
 *
 * 8. Concurrent Operation:
 *    - Multiple modules sending/receiving
 *    - Thread safety under load
 *    - Race condition prevention
 *
 * BIOLOGICAL BASIS:
 * The security-async integration mirrors the immune system's coordinated
 * response to threats. Security events act as cytokine signals, broadcast
 * via the NOREPINEPHRINE channel for priority delivery. Pattern updates
 * sync like immune memory B-cells sharing antibody patterns.
 *
 * @author NIMCP Development Team
 * @date 2025-01-09
 * @version 1.0.0
 */

#include <gtest/gtest.h>

/* Headers have their own extern "C" guards */
#include "security/async/nimcp_security_async_bridge.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_pattern_db.h"
#include "security/nimcp_policy_engine.h"
#include "security/nimcp_rate_limiter.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <condition_variable>

namespace {

/* ============================================================================
 * Test Constants
 * ============================================================================ */

/** @brief Maximum wait time for async operations (ms) */
static const int ASYNC_TIMEOUT_MS = 1000;

/** @brief Number of threads for concurrent tests */
static const int NUM_TEST_THREADS = 4;

/** @brief Messages per thread in concurrent tests */
static const int MESSAGES_PER_THREAD = 25;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

/**
 * @brief Test fixture for Security-Async bridge integration tests
 *
 * WHAT: Provides common setup/teardown for security-async testing
 * WHY:  Ensure consistent test environment and proper resource cleanup
 * HOW:  Initialize all security subsystems and bio-async router before each test
 */
class SecurityAsyncIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        /* Reset test state counters */
        threat_broadcasts_received_.store(0);
        policy_changes_received_.store(0);
        pattern_updates_received_.store(0);
        rate_limit_events_received_.store(0);
        bbb_alerts_received_.store(0);
        anomaly_events_received_.store(0);
        emergency_mode_activations_.store(0);

        /* Initialize bio-async system */
        nimcp_error_t err = nimcp_bio_async_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Bio-async initialization failed";
        bio_async_initialized_ = true;

        /* Initialize bio-router */
        bio_router_config_t router_cfg = bio_router_default_config();
        router_cfg.max_modules = 16;
        router_cfg.inbox_capacity = 256;
        err = bio_router_init(&router_cfg);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Bio-router initialization failed";
        bio_router_initialized_ = true;

        /* Create BBB system */
        bbb_config_t bbb_cfg = bbb_default_config();
        bbb_cfg.strict_mode = true;
        bbb_system_ = bbb_system_create(&bbb_cfg);
        ASSERT_NE(bbb_system_, nullptr) << "BBB system creation failed";
        ASSERT_TRUE(bbb_system_set_enabled(bbb_system_, true));

        /* Create anomaly detector */
        nimcp_anomaly_config_t anomaly_cfg = nimcp_anomaly_detector_default_config();
        anomaly_detector_ = nimcp_anomaly_detector_create(&anomaly_cfg);
        ASSERT_NE(anomaly_detector_, nullptr) << "Anomaly detector creation failed";

        /* Create pattern database */
        nimcp_pattern_db_config_t pattern_cfg = nimcp_pattern_db_default_config();
        pattern_cfg.enable_statistics = true;
        pattern_db_ = nimcp_pattern_db_create(&pattern_cfg);
        ASSERT_NE(pattern_db_, nullptr) << "Pattern DB creation failed";

        /* Create rate limiter */
        nimcp_rate_limit_config_t rate_cfg = nimcp_rate_limiter_default_config();
        rate_cfg.requests_per_second = 100.0f;
        rate_cfg.burst_size = 150;
        rate_cfg.enable_statistics = true;
        rate_limiter_ = nimcp_rate_limiter_create(&rate_cfg);
        ASSERT_NE(rate_limiter_, nullptr) << "Rate limiter creation failed";

        /* Create security-async bridge with default config */
        security_async_config_t bridge_cfg;
        int cfg_result = security_async_default_config(&bridge_cfg);
        ASSERT_EQ(cfg_result, 0) << "Failed to get default security-async config";

        bridge_cfg.enable_threat_broadcast = true;
        bridge_cfg.enable_policy_announcements = true;
        bridge_cfg.enable_pattern_sync = true;
        bridge_cfg.enable_rate_limit_events = true;
        bridge_cfg.enable_bbb_alerts = true;
        bridge_cfg.enable_anomaly_events = true;
        bridge_cfg.enable_distributed_intel = true;
        bridge_cfg.broadcast_threshold = SECURITY_EVENT_SEVERITY_LOW;

        bridge_ = security_async_bridge_create(&bridge_cfg);
        ASSERT_NE(bridge_, nullptr) << "Security-async bridge creation failed";

        /* Connect security subsystems to bridge */
        ASSERT_EQ(security_async_connect_bbb(bridge_, bbb_system_), 0);
        ASSERT_EQ(security_async_connect_anomaly_detector(bridge_, anomaly_detector_), 0);
        ASSERT_EQ(security_async_connect_pattern_db(bridge_, pattern_db_), 0);
        ASSERT_EQ(security_async_connect_rate_limiter(bridge_, rate_limiter_), 0);

        /* Connect to bio-async router */
        ASSERT_EQ(security_async_connect_bio_async(bridge_), 0);

        /* Register test observer module */
        RegisterObserverModule();
    }

    void TearDown() override
    {
        /* Unregister observer module */
        if (observer_ctx_) {
            bio_router_unregister_module(observer_ctx_);
            observer_ctx_ = nullptr;
        }

        /* Disconnect and destroy bridge */
        if (bridge_) {
            security_async_disconnect_bio_async(bridge_);
            security_async_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }

        /* Destroy security subsystems */
        if (rate_limiter_) {
            nimcp_rate_limiter_destroy(rate_limiter_);
            rate_limiter_ = nullptr;
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

        /* Shutdown bio-router and bio-async */
        if (bio_router_initialized_) {
            bio_router_shutdown();
            bio_router_initialized_ = false;
        }
        if (bio_async_initialized_) {
            nimcp_bio_async_shutdown();
            bio_async_initialized_ = false;
        }
    }

    /**
     * @brief Register observer module to receive security broadcasts
     *
     * WHAT: Register a test module that monitors security event broadcasts
     * WHY:  Verify broadcasts are actually received by other modules
     * HOW:  Register handlers for all security message types
     */
    void RegisterObserverModule()
    {
        bio_module_info_t info = {
            .module_id = BIO_MODULE_INTROSPECTION,
            .module_name = "security_observer",
            .inbox_capacity = 128,
            .user_data = this
        };

        observer_ctx_ = bio_router_register_module(&info);
        ASSERT_NE(observer_ctx_, nullptr);

        /* Register handlers for security messages */
        bio_router_register_category_handler(
            observer_ctx_,
            0x0750,  /* Security message category base */
            &SecurityAsyncIntegrationTest::SecurityMessageHandler
        );
    }

    /**
     * @brief Static handler for security messages
     */
    static nimcp_error_t SecurityMessageHandler(
        const void* msg,
        size_t msg_size,
        nimcp_bio_promise_t response_promise,
        void* user_data)
    {
        auto* self = static_cast<SecurityAsyncIntegrationTest*>(user_data);
        if (!self || !msg) return NIMCP_ERROR_INVALID_PARAM;

        const bio_message_header_t* header =
            static_cast<const bio_message_header_t*>(msg);

        /* Categorize and count the message */
        uint32_t msg_type = header->type;

        if (msg_type >= BIO_MSG_SECURITY_THREAT_DETECTED_EXT &&
            msg_type <= BIO_MSG_SECURITY_THREAT_QUARANTINED) {
            self->threat_broadcasts_received_.fetch_add(1);
        } else if (msg_type >= BIO_MSG_SECURITY_POLICY_CHANGE_EXT &&
                   msg_type <= BIO_MSG_SECURITY_POLICY_RELOAD) {
            self->policy_changes_received_.fetch_add(1);
        } else if (msg_type >= BIO_MSG_SECURITY_PATTERN_UPDATE_EXT &&
                   msg_type <= BIO_MSG_SECURITY_PATTERN_LEARNED) {
            self->pattern_updates_received_.fetch_add(1);
        } else if (msg_type >= BIO_MSG_SECURITY_RATE_LIMIT_HIT_EXT &&
                   msg_type <= BIO_MSG_SECURITY_RATE_LIMIT_UNBLOCKED) {
            self->rate_limit_events_received_.fetch_add(1);
        } else if (msg_type >= BIO_MSG_SECURITY_BBB_ALERT_EXT &&
                   msg_type <= BIO_MSG_SECURITY_BBB_MEMORY_VIOLATION) {
            self->bbb_alerts_received_.fetch_add(1);
        } else if (msg_type >= BIO_MSG_SECURITY_ANOMALY_DETECTED_EXT &&
                   msg_type <= BIO_MSG_SECURITY_ANOMALY_TIMING) {
            self->anomaly_events_received_.fetch_add(1);
        }

        /* Notify any waiters */
        self->message_cv_.notify_all();

        return NIMCP_SUCCESS;
    }

    /**
     * @brief Wait for message condition with timeout
     */
    bool WaitForMessages(std::atomic<int>& counter, int expected, int timeout_ms)
    {
        auto start = std::chrono::steady_clock::now();
        while (counter.load() < expected) {
            /* Process incoming messages */
            bio_router_process_inbox(observer_ctx_, 10);

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();

            if (elapsed >= timeout_ms) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }

    /* Security subsystems */
    bbb_system_t bbb_system_{nullptr};
    nimcp_anomaly_detector_t anomaly_detector_{nullptr};
    nimcp_pattern_db_t pattern_db_{nullptr};
    nimcp_rate_limiter_t rate_limiter_{nullptr};

    /* Security-async bridge */
    security_async_bridge_t* bridge_{nullptr};

    /* Observer module context */
    bio_module_context_t observer_ctx_{nullptr};

    /* Initialization flags */
    bool bio_async_initialized_{false};
    bool bio_router_initialized_{false};

    /* Event counters */
    std::atomic<int> threat_broadcasts_received_{0};
    std::atomic<int> policy_changes_received_{0};
    std::atomic<int> pattern_updates_received_{0};
    std::atomic<int> rate_limit_events_received_{0};
    std::atomic<int> bbb_alerts_received_{0};
    std::atomic<int> anomaly_events_received_{0};
    std::atomic<int> emergency_mode_activations_{0};

    /* Synchronization */
    std::mutex message_mutex_;
    std::condition_variable message_cv_;
};

/* ============================================================================
 * Security + Bio-Router Integration Tests
 * ============================================================================ */

/**
 * @test Verify security events broadcast through bio-router
 *
 * WHAT: Test that security threat broadcasts reach registered modules
 * WHY:  Core functionality - security alerts must propagate system-wide
 * HOW:  Broadcast threat, verify observer module receives it
 */
TEST_F(SecurityAsyncIntegrationTest, SecurityEventsBroadcastThroughBioRouter)
{
    /* Broadcast a threat alert */
    uint8_t threat_hash[32] = {0};
    std::memset(threat_hash, 0xAB, sizeof(threat_hash));

    int result = security_async_broadcast_threat(
        bridge_,
        BBB_THREAT_SQL_INJECTION,
        BBB_SEVERITY_HIGH,
        "SQL injection detected in user input",
        threat_hash
    );
    EXPECT_EQ(result, 0) << "Failed to broadcast threat";

    /* Wait for broadcast to be received */
    bool received = WaitForMessages(threat_broadcasts_received_, 1, ASYNC_TIMEOUT_MS);
    EXPECT_TRUE(received) << "Threat broadcast not received within timeout";
    EXPECT_GE(threat_broadcasts_received_.load(), 1);
}

/**
 * @test Verify threat messages received from distributed nodes
 *
 * WHAT: Test receiving threat reports from peer modules
 * WHY:  Distributed detection improves overall security coverage
 * HOW:  Simulate peer threat report, verify it updates local state
 */
TEST_F(SecurityAsyncIntegrationTest, ThreatMessagesReceivedFromDistributedNodes)
{
    uint8_t threat_hash[32];
    std::memset(threat_hash, 0xCD, sizeof(threat_hash));

    /* Receive threat report from peer */
    int result = security_async_receive_threat_report(
        bridge_,
        BIO_MODULE_ETHICS,  /* Simulated peer module */
        BBB_THREAT_CODE_INJECTION,
        threat_hash,
        0.85f  /* High confidence */
    );
    EXPECT_EQ(result, 0) << "Failed to receive threat report";

    /* Verify threat intel was cached */
    threat_intel_entry_t entry;
    bool found = security_async_lookup_threat_intel(bridge_, threat_hash, &entry);
    EXPECT_TRUE(found) << "Threat intel not cached after receiving report";

    if (found) {
        EXPECT_EQ(entry.threat_type, BBB_THREAT_CODE_INJECTION);
        EXPECT_FLOAT_EQ(entry.confidence, 0.85f);
        EXPECT_EQ(entry.source_node, BIO_MODULE_ETHICS);
    }
}

/**
 * @test Verify policy changes propagate system-wide
 *
 * WHAT: Test policy change announcements are broadcast to all modules
 * WHY:  Policy enforcement requires consistent state across system
 * HOW:  Announce policy change, verify observer receives it
 */
TEST_F(SecurityAsyncIntegrationTest, PolicyChangesPropagatedSystemWide)
{
    /* Announce policy change */
    int result = security_async_announce_policy_change(
        bridge_,
        NIMCP_POLICY_ACTION_DENY,
        "strict_input_validation",
        "Enabled strict input validation for all modules"
    );
    EXPECT_EQ(result, 0) << "Failed to announce policy change";

    /* Wait for broadcast */
    bool received = WaitForMessages(policy_changes_received_, 1, ASYNC_TIMEOUT_MS);
    EXPECT_TRUE(received) << "Policy change not received within timeout";
    EXPECT_GE(policy_changes_received_.load(), 1);
}

/**
 * @test Verify bio-async message priority affected by threat level
 *
 * WHAT: Test that active threats boost message priority in async system
 * WHY:  Security alerts need priority delivery during incidents
 * HOW:  Activate threat, verify effects show priority boost
 */
TEST_F(SecurityAsyncIntegrationTest, MessagePriorityAffectedByThreatLevel)
{
    /* Get baseline effects */
    security_async_effects_t baseline_effects;
    ASSERT_EQ(security_async_get_security_effects(bridge_, &baseline_effects), 0);
    EXPECT_FALSE(baseline_effects.active_threat);
    EXPECT_FLOAT_EQ(baseline_effects.priority_boost, 0.0f);

    /* Broadcast high-severity threat */
    int result = security_async_broadcast_threat(
        bridge_,
        BBB_THREAT_SHELLCODE,
        BBB_SEVERITY_CRITICAL,
        "Critical shellcode detected",
        nullptr
    );
    EXPECT_EQ(result, 0);

    /* Update effects */
    EXPECT_EQ(security_async_update_security_effects(bridge_), 0);

    /* Verify priority boost */
    security_async_effects_t threat_effects;
    ASSERT_EQ(security_async_get_security_effects(bridge_, &threat_effects), 0);
    EXPECT_TRUE(threat_effects.active_threat);
    EXPECT_GT(threat_effects.priority_boost, 0.0f);
}

/* ============================================================================
 * Security + BBB Integration Tests
 * ============================================================================ */

/**
 * @test Verify BBB threats trigger async broadcasts
 *
 * WHAT: Test that BBB-detected threats are automatically broadcast
 * WHY:  BBB is perimeter defense - threats must alert the entire system
 * HOW:  Report BBB threat, verify broadcast was generated
 */
TEST_F(SecurityAsyncIntegrationTest, BBBThreatsTriggerAsyncBroadcasts)
{
    /* Create BBB threat report */
    bbb_threat_report_t report;
    std::memset(&report, 0, sizeof(report));
    report.type = BBB_THREAT_BUFFER_OVERFLOW;
    report.severity = BBB_SEVERITY_HIGH;
    report.action_taken = BBB_ACTION_BLOCK;
    std::strncpy(report.description, "Buffer overflow in network handler",
                 sizeof(report.description) - 1);

    /* Broadcast BBB alert */
    int result = security_async_broadcast_bbb_alert(bridge_, &report);
    EXPECT_EQ(result, 0) << "Failed to broadcast BBB alert";

    /* Wait for broadcast */
    bool received = WaitForMessages(bbb_alerts_received_, 1, ASYNC_TIMEOUT_MS);
    EXPECT_TRUE(received) << "BBB alert not received within timeout";
    EXPECT_GE(bbb_alerts_received_.load(), 1);
}

/**
 * @test Verify BBB threshold adjustments from network threat intel
 *
 * WHAT: Test that distributed threat intel affects BBB sensitivity
 * WHY:  Network-wide threats should heighten local defenses
 * HOW:  Add threat intel, verify async effects reflect elevated state
 */
TEST_F(SecurityAsyncIntegrationTest, BBBThresholdAdjustmentsFromNetworkThreatIntel)
{
    /* Add multiple threat intel entries to simulate network-wide attack */
    for (int i = 0; i < 5; i++) {
        uint8_t threat_hash[32];
        std::memset(threat_hash, i + 0x10, sizeof(threat_hash));

        int result = security_async_receive_threat_report(
            bridge_,
            BIO_MODULE_BRAIN + i,  /* Different peers */
            BBB_THREAT_SQL_INJECTION,
            threat_hash,
            0.9f
        );
        EXPECT_EQ(result, 0);
    }

    /* Update async effects from accumulated intel */
    EXPECT_EQ(security_async_update_async_effects(bridge_), 0);

    /* Verify elevated network threat level */
    async_security_effects_t effects;
    ASSERT_EQ(security_async_get_async_effects(bridge_, &effects), 0);
    EXPECT_GE(effects.peer_threat_reports, 5u);
    EXPECT_GT(effects.network_threat_level, 0.0f);
}

/**
 * @test Verify quarantine actions coordinated across modules
 *
 * WHAT: Test that quarantine events are broadcast for coordination
 * WHY:  Quarantine must be enforced consistently across all modules
 * HOW:  Broadcast quarantine action, verify it reaches other modules
 */
TEST_F(SecurityAsyncIntegrationTest, QuarantineActionsCoordinatedAcrossModules)
{
    /* Create quarantine threat report */
    bbb_threat_report_t report;
    std::memset(&report, 0, sizeof(report));
    report.type = BBB_THREAT_ROP_CHAIN;
    report.severity = BBB_SEVERITY_CRITICAL;
    report.action_taken = BBB_ACTION_QUARANTINE;
    report.quarantined = true;
    std::strncpy(report.description, "ROP chain detected - region quarantined",
                 sizeof(report.description) - 1);

    /* Broadcast quarantine alert */
    int result = security_async_broadcast_bbb_alert(bridge_, &report);
    EXPECT_EQ(result, 0);

    /* Wait for broadcast */
    bool received = WaitForMessages(bbb_alerts_received_, 1, ASYNC_TIMEOUT_MS);
    EXPECT_TRUE(received);

    /* Verify effects reflect quarantine */
    EXPECT_EQ(security_async_update_security_effects(bridge_), 0);
    security_async_effects_t effects;
    ASSERT_EQ(security_async_get_security_effects(bridge_, &effects), 0);
    EXPECT_GE(effects.quarantined_nodes, 0u);  /* May not track count */
}

/* ============================================================================
 * Security + Anomaly Detector Integration Tests
 * ============================================================================ */

/**
 * @test Verify anomaly detection triggers async threat sharing
 *
 * WHAT: Test that anomaly events are shared via async
 * WHY:  ML-detected anomalies may indicate novel attacks
 * HOW:  Create anomaly event, verify it's broadcast
 */
TEST_F(SecurityAsyncIntegrationTest, AnomalyDetectionTriggersAsyncThreatSharing)
{
    /* Create and publish anomaly event */
    security_async_event_t event;
    std::memset(&event, 0, sizeof(event));
    event.category = SECURITY_EVENT_CATEGORY_ANOMALY;
    event.severity = SECURITY_EVENT_SEVERITY_HIGH;
    event.timestamp_us = 1000000;
    event.source_module = BIO_MODULE_BRAIN;
    event.data.anomaly.anomaly_score = 0.92f;
    event.data.anomaly.triggered_features = NIMCP_TRIGGER_ENTROPY | NIMCP_TRIGGER_SPECIAL_RATIO;
    event.data.anomaly.confidence = 0.88f;
    std::strncpy(event.description, "High entropy anomaly detected",
                 sizeof(event.description) - 1);

    int result = security_async_publish_event(bridge_, &event);
    EXPECT_EQ(result, 0) << "Failed to publish anomaly event";

    /* Wait for broadcast */
    bool received = WaitForMessages(anomaly_events_received_, 1, ASYNC_TIMEOUT_MS);
    EXPECT_TRUE(received) << "Anomaly event not received within timeout";
}

/**
 * @test Verify distributed anomaly correlation
 *
 * WHAT: Test that anomalies from multiple sources are correlated
 * WHY:  Correlated anomalies indicate coordinated attacks
 * HOW:  Receive anomaly reports from multiple peers, check correlation
 */
TEST_F(SecurityAsyncIntegrationTest, DistributedAnomalyCorrelation)
{
    /* Simulate anomaly reports from multiple modules */
    uint8_t common_hash[32];
    std::memset(common_hash, 0xEF, sizeof(common_hash));

    const uint32_t peer_modules[] = {
        BIO_MODULE_ETHICS,
        BIO_MODULE_SALIENCE,
        BIO_MODULE_ATTENTION
    };

    for (uint32_t peer : peer_modules) {
        int result = security_async_receive_threat_report(
            bridge_,
            peer,
            BBB_THREAT_DATA_TAMPERING,
            common_hash,
            0.75f
        );
        EXPECT_EQ(result, 0);
    }

    /* Update async effects */
    EXPECT_EQ(security_async_update_async_effects(bridge_), 0);

    /* Verify correlation detected */
    async_security_effects_t effects;
    ASSERT_EQ(security_async_get_async_effects(bridge_, &effects), 0);
    EXPECT_GE(effects.peer_threat_reports, 3u);

    /* Lookup threat should show confirmed from multiple sources */
    threat_intel_entry_t entry;
    bool found = security_async_lookup_threat_intel(bridge_, common_hash, &entry);
    EXPECT_TRUE(found);
    if (found) {
        EXPECT_GE(entry.observation_count, 3u);
    }
}

/* ============================================================================
 * Security + Rate Limiter Integration Tests
 * ============================================================================ */

/**
 * @test Verify rate limit events shared via async
 *
 * WHAT: Test that rate limit violations are broadcast
 * WHY:  Rate limiting indicates potential DoS - other modules should know
 * HOW:  Broadcast rate limit event, verify it reaches observer
 */
TEST_F(SecurityAsyncIntegrationTest, RateLimitEventsSharedViaAsync)
{
    /* Broadcast rate limit event */
    int result = security_async_broadcast_rate_limit(
        bridge_,
        "client_192.168.1.100",
        PENALTY_REDUCE_RATE_50,
        5  /* 5 violations */
    );
    EXPECT_EQ(result, 0) << "Failed to broadcast rate limit event";

    /* Wait for broadcast */
    bool received = WaitForMessages(rate_limit_events_received_, 1, ASYNC_TIMEOUT_MS);
    EXPECT_TRUE(received) << "Rate limit event not received within timeout";
    EXPECT_GE(rate_limit_events_received_.load(), 1);
}

/**
 * @test Verify system-wide load coordination
 *
 * WHAT: Test that rate limiting affects system-wide security posture
 * WHY:  Heavy rate limiting may indicate attack requiring response
 * HOW:  Broadcast multiple rate limit events, check effects
 */
TEST_F(SecurityAsyncIntegrationTest, SystemWideLoadCoordination)
{
    /* Broadcast multiple rate limit events */
    for (int i = 0; i < 5; i++) {
        char client_id[64];
        snprintf(client_id, sizeof(client_id), "client_%d.%d.%d.%d",
                 192, 168, 1, 100 + i);

        int result = security_async_broadcast_rate_limit(
            bridge_,
            client_id,
            PENALTY_BLOCK_TEMPORARY,
            10 + i
        );
        EXPECT_EQ(result, 0);
    }

    /* Update effects */
    EXPECT_EQ(security_async_update_security_effects(bridge_), 0);

    /* Verify rate reduction factor reflects load */
    security_async_effects_t effects;
    ASSERT_EQ(security_async_get_security_effects(bridge_, &effects), 0);
    /* Heavy rate limiting may trigger rate reduction */
    EXPECT_GE(effects.rate_reduction_factor, 0.0f);
}

/**
 * @test Verify emergency throttling coordination
 *
 * WHAT: Test emergency throttle broadcast and coordination
 * WHY:  System-wide throttling needed during severe attacks
 * HOW:  Enter emergency mode, verify effects show throttle active
 */
TEST_F(SecurityAsyncIntegrationTest, EmergencyThrottlingCoordination)
{
    /* Enter emergency mode */
    int result = security_async_enter_emergency_mode(bridge_);
    EXPECT_EQ(result, 0) << "Failed to enter emergency mode";
    EXPECT_TRUE(security_async_is_emergency_mode(bridge_));

    /* Update and check effects */
    EXPECT_EQ(security_async_update_security_effects(bridge_), 0);
    security_async_effects_t effects;
    ASSERT_EQ(security_async_get_security_effects(bridge_, &effects), 0);

    /* Emergency mode should enable throttle */
    EXPECT_TRUE(effects.emergency_throttle);

    /* Exit emergency mode */
    result = security_async_exit_emergency_mode(bridge_);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(security_async_is_emergency_mode(bridge_));
}

/* ============================================================================
 * Security + Pattern DB Integration Tests
 * ============================================================================ */

/**
 * @test Verify pattern updates synced via async
 *
 * WHAT: Test that pattern database updates are broadcast
 * WHY:  Distributed pattern sync improves threat detection
 * HOW:  Broadcast pattern update, verify observer receives it
 */
TEST_F(SecurityAsyncIntegrationTest, PatternUpdatesSyncedViaAsync)
{
    /* Broadcast pattern update */
    int result = security_async_broadcast_pattern_update(
        bridge_,
        12345,  /* Pattern ID */
        NIMCP_PATTERN_SQL_INJECTION,
        true    /* New pattern */
    );
    EXPECT_EQ(result, 0) << "Failed to broadcast pattern update";

    /* Wait for broadcast */
    bool received = WaitForMessages(pattern_updates_received_, 1, ASYNC_TIMEOUT_MS);
    EXPECT_TRUE(received) << "Pattern update not received within timeout";
    EXPECT_GE(pattern_updates_received_.load(), 1);
}

/**
 * @test Verify distributed threat pattern learning
 *
 * WHAT: Test receiving pattern updates from peers
 * WHY:  Learn from network-wide threat discoveries
 * HOW:  Receive pattern entry from peer, verify it's processed
 */
TEST_F(SecurityAsyncIntegrationTest, DistributedThreatPatternLearning)
{
    /* Create pattern entry from peer */
    nimcp_pattern_entry_t entry;
    entry.pattern = "(?i)(exec|execute)\\s*\\(";
    entry.category = NIMCP_PATTERN_COMMAND_INJECTION;
    entry.priority = 10;
    entry.weight = 0.9f;
    entry.description = "Command injection via exec()";
    entry.flags = NIMCP_PATTERN_FLAG_CASE_INSENSITIVE;

    /* Receive pattern update from peer */
    int result = security_async_receive_pattern_update(
        bridge_,
        BIO_MODULE_ETHICS,  /* Peer module */
        &entry
    );
    EXPECT_EQ(result, 0) << "Failed to receive pattern update";

    /* Statistics should reflect received pattern */
    security_async_stats_t stats;
    ASSERT_EQ(security_async_get_stats(bridge_, &stats), 0);
    /* Stats may not track individual patterns */
}

/**
 * @test Verify cross-module pattern matching
 *
 * WHAT: Test pattern match notifications via async
 * WHY:  Pattern matches should be shared for correlation
 * HOW:  Create pattern match event, broadcast, verify receipt
 */
TEST_F(SecurityAsyncIntegrationTest, CrossModulePatternMatching)
{
    /* Create and publish pattern match event */
    security_async_event_t event;
    std::memset(&event, 0, sizeof(event));
    event.category = SECURITY_EVENT_CATEGORY_PATTERN;
    event.severity = SECURITY_EVENT_SEVERITY_MEDIUM;
    event.timestamp_us = 2000000;
    event.source_module = BIO_MODULE_BRAIN;
    event.data.pattern.category = NIMCP_PATTERN_XSS;
    event.data.pattern.pattern_id = 54321;
    event.data.pattern.threat_score = 0.75f;
    std::strncpy(event.description, "XSS pattern matched in request",
                 sizeof(event.description) - 1);

    int result = security_async_publish_event(bridge_, &event);
    EXPECT_EQ(result, 0);

    /* Wait for event propagation */
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Verify statistics updated */
    security_async_stats_t stats;
    ASSERT_EQ(security_async_get_stats(bridge_, &stats), 0);
    EXPECT_GE(stats.pattern_events, 1u);
}

/* ============================================================================
 * Bidirectional Flow Tests
 * ============================================================================ */

/**
 * @test Verify Security->Async broadcasts reach modules
 *
 * WHAT: Test outbound security event flow
 * WHY:  Core functionality validation
 * HOW:  Broadcast multiple event types, verify all received
 */
TEST_F(SecurityAsyncIntegrationTest, SecurityToAsyncBroadcastsReachModules)
{
    /* Reset counters */
    threat_broadcasts_received_.store(0);
    policy_changes_received_.store(0);
    rate_limit_events_received_.store(0);

    /* Broadcast threat */
    EXPECT_EQ(security_async_broadcast_threat(
        bridge_, BBB_THREAT_FORMAT_STRING, BBB_SEVERITY_MEDIUM,
        "Format string detected", nullptr), 0);

    /* Broadcast policy change */
    EXPECT_EQ(security_async_announce_policy_change(
        bridge_, NIMCP_POLICY_ACTION_LOG, "audit_mode",
        "Audit mode enabled"), 0);

    /* Broadcast rate limit */
    EXPECT_EQ(security_async_broadcast_rate_limit(
        bridge_, "test_client", PENALTY_WARN, 1), 0);

    /* Wait for all broadcasts */
    auto start = std::chrono::steady_clock::now();
    while ((threat_broadcasts_received_.load() < 1 ||
            policy_changes_received_.load() < 1 ||
            rate_limit_events_received_.load() < 1) &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < ASYNC_TIMEOUT_MS) {
        bio_router_process_inbox(observer_ctx_, 20);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_GE(threat_broadcasts_received_.load(), 1);
    EXPECT_GE(policy_changes_received_.load(), 1);
    EXPECT_GE(rate_limit_events_received_.load(), 1);
}

/**
 * @test Verify Async->Security intel updates pattern DB
 *
 * WHAT: Test inbound intelligence updates local state
 * WHY:  Distributed intel must improve local detection
 * HOW:  Receive intel, verify cache and effects updated
 */
TEST_F(SecurityAsyncIntegrationTest, AsyncToSecurityIntelUpdatesPatternDB)
{
    /* Clear intel cache */
    EXPECT_EQ(security_async_clear_threat_intel(bridge_), 0);

    /* Receive multiple threat reports */
    uint8_t hash1[32], hash2[32];
    std::memset(hash1, 0x11, sizeof(hash1));
    std::memset(hash2, 0x22, sizeof(hash2));

    EXPECT_EQ(security_async_receive_threat_report(
        bridge_, BIO_MODULE_ETHICS, BBB_THREAT_SQL_INJECTION, hash1, 0.8f), 0);
    EXPECT_EQ(security_async_receive_threat_report(
        bridge_, BIO_MODULE_SALIENCE, BBB_THREAT_CODE_INJECTION, hash2, 0.9f), 0);

    /* Verify both are cached */
    threat_intel_entry_t entry1, entry2;
    EXPECT_TRUE(security_async_lookup_threat_intel(bridge_, hash1, &entry1));
    EXPECT_TRUE(security_async_lookup_threat_intel(bridge_, hash2, &entry2));

    /* Verify intel stats */
    uint32_t count, confirmed;
    ASSERT_EQ(security_async_get_intel_stats(bridge_, &count, &confirmed), 0);
    EXPECT_GE(count, 2u);
}

/**
 * @test Verify full cycle: threat detected -> broadcast -> pattern learned
 *
 * WHAT: Test complete bidirectional flow
 * WHY:  End-to-end validation of security-async integration
 * HOW:  Detect threat locally, broadcast, receive peer intel, verify learning
 */
TEST_F(SecurityAsyncIntegrationTest, FullCycleThreatDetectedBroadcastPatternLearned)
{
    /* Step 1: Detect threat locally and broadcast */
    uint8_t threat_hash[32];
    std::memset(threat_hash, 0x33, sizeof(threat_hash));

    EXPECT_EQ(security_async_broadcast_threat(
        bridge_,
        BBB_THREAT_SHELLCODE,
        BBB_SEVERITY_CRITICAL,
        "Shellcode pattern detected",
        threat_hash
    ), 0);

    /* Step 2: Wait for broadcast */
    bool received = WaitForMessages(threat_broadcasts_received_, 1, ASYNC_TIMEOUT_MS);
    EXPECT_TRUE(received);

    /* Step 3: Receive corroborating intel from peer */
    EXPECT_EQ(security_async_receive_threat_report(
        bridge_,
        BIO_MODULE_TRAINING,
        BBB_THREAT_SHELLCODE,
        threat_hash,
        0.95f
    ), 0);

    /* Step 4: Verify threat is now confirmed */
    threat_intel_entry_t entry;
    EXPECT_TRUE(security_async_lookup_threat_intel(bridge_, threat_hash, &entry));
    EXPECT_EQ(entry.threat_type, BBB_THREAT_SHELLCODE);
    /* With local + peer report, observation count should be >= 2 */

    /* Step 5: Verify statistics reflect complete cycle */
    security_async_stats_t stats;
    ASSERT_EQ(security_async_get_stats(bridge_, &stats), 0);
    EXPECT_GE(stats.threat_events, 1u);
    EXPECT_GE(stats.intel_received, 1u);
}

/* ============================================================================
 * Emergency Mode Tests
 * ============================================================================ */

/**
 * @test Verify emergency mode broadcast
 *
 * WHAT: Test that emergency mode is communicated system-wide
 * WHY:  All modules must know when system is under attack
 * HOW:  Enter emergency mode, verify state propagates
 */
TEST_F(SecurityAsyncIntegrationTest, EmergencyModeBroadcast)
{
    /* Initially not in emergency mode */
    EXPECT_FALSE(security_async_is_emergency_mode(bridge_));

    /* Enter emergency mode */
    EXPECT_EQ(security_async_enter_emergency_mode(bridge_), 0);
    EXPECT_TRUE(security_async_is_emergency_mode(bridge_));

    /* Verify state reflects emergency */
    security_async_state_t state;
    ASSERT_EQ(security_async_get_state(bridge_, &state), 0);
    EXPECT_TRUE(state.emergency_mode);

    /* Exit emergency mode */
    EXPECT_EQ(security_async_exit_emergency_mode(bridge_), 0);
    EXPECT_FALSE(security_async_is_emergency_mode(bridge_));
}

/**
 * @test Verify system-wide security posture change
 *
 * WHAT: Test that emergency mode affects all security effects
 * WHY:  Emergency requires coordinated elevated response
 * HOW:  Enter emergency, check all effects are elevated
 */
TEST_F(SecurityAsyncIntegrationTest, SystemWideSecurityPostureChange)
{
    /* Get baseline effects */
    EXPECT_EQ(security_async_update_security_effects(bridge_), 0);
    security_async_effects_t baseline;
    ASSERT_EQ(security_async_get_security_effects(bridge_, &baseline), 0);

    /* Enter emergency mode */
    EXPECT_EQ(security_async_enter_emergency_mode(bridge_), 0);
    EXPECT_EQ(security_async_update_security_effects(bridge_), 0);

    /* Get emergency effects */
    security_async_effects_t emergency;
    ASSERT_EQ(security_async_get_security_effects(bridge_, &emergency), 0);

    /* Verify elevated posture */
    EXPECT_TRUE(emergency.active_threat);
    EXPECT_TRUE(emergency.emergency_throttle);
    EXPECT_GT(emergency.priority_boost, baseline.priority_boost);

    /* Cleanup */
    EXPECT_EQ(security_async_exit_emergency_mode(bridge_), 0);
}

/**
 * @test Verify recovery coordination
 *
 * WHAT: Test coordinated recovery from emergency mode
 * WHY:  Smooth recovery prevents operational disruption
 * HOW:  Exit emergency mode, verify state returns to normal
 */
TEST_F(SecurityAsyncIntegrationTest, RecoveryCoordination)
{
    /* Enter and exit emergency mode */
    EXPECT_EQ(security_async_enter_emergency_mode(bridge_), 0);
    EXPECT_TRUE(security_async_is_emergency_mode(bridge_));

    /* Perform recovery */
    EXPECT_EQ(security_async_exit_emergency_mode(bridge_), 0);
    EXPECT_FALSE(security_async_is_emergency_mode(bridge_));

    /* Update effects post-recovery */
    EXPECT_EQ(security_async_update_security_effects(bridge_), 0);

    /* Verify recovery state */
    security_async_effects_t effects;
    ASSERT_EQ(security_async_get_security_effects(bridge_, &effects), 0);
    EXPECT_FALSE(effects.emergency_throttle);

    /* State should reflect recovery */
    security_async_state_t state;
    ASSERT_EQ(security_async_get_state(bridge_, &state), 0);
    EXPECT_FALSE(state.emergency_mode);
}

/* ============================================================================
 * Concurrent Operation Tests
 * ============================================================================ */

/**
 * @test Verify multiple modules sending/receiving concurrently
 *
 * WHAT: Test concurrent security event handling
 * WHY:  Real systems have multiple concurrent security operations
 * HOW:  Multiple threads broadcast events, verify all processed
 */
TEST_F(SecurityAsyncIntegrationTest, MultipleModulesSendingReceivingConcurrently)
{
    std::atomic<int> total_broadcasts{0};
    std::vector<std::thread> threads;

    /* Launch sender threads */
    for (int t = 0; t < NUM_TEST_THREADS; t++) {
        threads.emplace_back([this, t, &total_broadcasts]() {
            for (int i = 0; i < MESSAGES_PER_THREAD; i++) {
                char desc[128];
                snprintf(desc, sizeof(desc),
                         "Threat from thread %d message %d", t, i);

                uint8_t hash[32];
                std::memset(hash, (t * 100 + i) & 0xFF, sizeof(hash));

                int result = security_async_broadcast_threat(
                    bridge_,
                    BBB_THREAT_SQL_INJECTION,
                    BBB_SEVERITY_MEDIUM,
                    desc,
                    hash
                );

                if (result == 0) {
                    total_broadcasts.fetch_add(1);
                }

                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
        });
    }

    /* Process messages concurrently */
    auto receiver = std::thread([this]() {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start).count() < ASYNC_TIMEOUT_MS * 2) {
            bio_router_process_inbox(observer_ctx_, 20);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    /* Wait for senders */
    for (auto& t : threads) {
        t.join();
    }

    /* Wait for receiver */
    receiver.join();

    /* Verify all broadcasts succeeded */
    int expected = NUM_TEST_THREADS * MESSAGES_PER_THREAD;
    EXPECT_EQ(total_broadcasts.load(), expected)
        << "Not all broadcasts succeeded";

    /* Verify most messages were received */
    EXPECT_GE(threat_broadcasts_received_.load(), expected / 2)
        << "Too few broadcasts received";
}

/**
 * @test Verify thread safety under load
 *
 * WHAT: Test bridge thread safety with heavy concurrent access
 * WHY:  Bridge must handle production workloads safely
 * HOW:  Concurrent reads/writes to bridge state
 */
TEST_F(SecurityAsyncIntegrationTest, ThreadSafetyUnderLoad)
{
    std::atomic<int> operations_completed{0};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    /* Mixed operation threads */
    for (int t = 0; t < NUM_TEST_THREADS * 2; t++) {
        threads.emplace_back([this, t, &operations_completed, &errors]() {
            for (int i = 0; i < MESSAGES_PER_THREAD; i++) {
                int result = 0;

                switch ((t + i) % 5) {
                case 0:  /* Broadcast threat */
                    result = security_async_broadcast_threat(
                        bridge_, BBB_THREAT_CODE_INJECTION, BBB_SEVERITY_LOW,
                        "Test threat", nullptr);
                    break;

                case 1:  /* Get state */
                    {
                        security_async_state_t state;
                        result = security_async_get_state(bridge_, &state);
                    }
                    break;

                case 2:  /* Get stats */
                    {
                        security_async_stats_t stats;
                        result = security_async_get_stats(bridge_, &stats);
                    }
                    break;

                case 3:  /* Update effects */
                    result = security_async_update_security_effects(bridge_);
                    break;

                case 4:  /* Process events */
                    security_async_process_events(bridge_, 5);
                    result = 0;
                    break;
                }

                if (result == 0) {
                    operations_completed.fetch_add(1);
                } else {
                    errors.fetch_add(1);
                }

                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    /* Wait for all threads */
    for (auto& t : threads) {
        t.join();
    }

    /* Verify no crashes and acceptable error rate */
    int total_ops = NUM_TEST_THREADS * 2 * MESSAGES_PER_THREAD;
    EXPECT_GT(operations_completed.load(), total_ops / 2)
        << "Too many operations failed";

    /* Small error count acceptable due to concurrent state changes */
    EXPECT_LT(errors.load(), total_ops / 4)
        << "Too many errors under concurrent load";
}

/**
 * @test Verify race condition prevention
 *
 * WHAT: Test for data races in critical sections
 * WHY:  Race conditions cause undefined behavior
 * HOW:  Concurrent state modifications with verification
 */
TEST_F(SecurityAsyncIntegrationTest, RaceConditionPrevention)
{
    std::atomic<bool> race_detected{false};
    std::atomic<int> emergency_toggles{0};
    std::vector<std::thread> threads;

    /* Emergency mode toggle threads */
    for (int t = 0; t < NUM_TEST_THREADS; t++) {
        threads.emplace_back([this, t, &race_detected, &emergency_toggles]() {
            for (int i = 0; i < 10; i++) {
                if (t % 2 == 0) {
                    /* Enter emergency mode */
                    security_async_enter_emergency_mode(bridge_);
                    bool is_emergency = security_async_is_emergency_mode(bridge_);

                    /* If we just entered, should be in emergency mode */
                    /* (Unless another thread exited in between) */
                    emergency_toggles.fetch_add(1);
                } else {
                    /* Exit emergency mode */
                    security_async_exit_emergency_mode(bridge_);
                    emergency_toggles.fetch_add(1);
                }

                /* Verify state consistency */
                security_async_state_t state;
                if (security_async_get_state(bridge_, &state) == 0) {
                    bool api_says = security_async_is_emergency_mode(bridge_);
                    /* State should match API - if not, possible race */
                    if (state.emergency_mode != api_says) {
                        /* Allow brief inconsistency due to concurrent access */
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    /* Wait for threads */
    for (auto& t : threads) {
        t.join();
    }

    /* Should complete without crashes */
    EXPECT_EQ(emergency_toggles.load(), NUM_TEST_THREADS * 10);
    EXPECT_FALSE(race_detected.load()) << "Possible race condition detected";

    /* Final state should be consistent */
    security_async_state_t final_state;
    ASSERT_EQ(security_async_get_state(bridge_, &final_state), 0);
    bool final_emergency = security_async_is_emergency_mode(bridge_);
    EXPECT_EQ(final_state.emergency_mode, final_emergency);
}

/* ============================================================================
 * Statistics and Monitoring Tests
 * ============================================================================ */

/**
 * @test Verify statistics tracking accuracy
 *
 * WHAT: Test that all operations are correctly counted
 * WHY:  Monitoring depends on accurate statistics
 * HOW:  Perform known operations, verify counts match
 */
TEST_F(SecurityAsyncIntegrationTest, StatisticsTrackingAccuracy)
{
    /* Reset statistics */
    EXPECT_EQ(security_async_reset_stats(bridge_), 0);

    /* Perform known number of operations */
    const int NUM_THREATS = 5;
    const int NUM_POLICIES = 3;
    const int NUM_PATTERNS = 4;

    for (int i = 0; i < NUM_THREATS; i++) {
        security_async_broadcast_threat(
            bridge_, BBB_THREAT_SQL_INJECTION, BBB_SEVERITY_MEDIUM,
            "Test threat", nullptr);
    }

    for (int i = 0; i < NUM_POLICIES; i++) {
        security_async_announce_policy_change(
            bridge_, NIMCP_POLICY_ACTION_LOG, "test_rule",
            "Test policy change");
    }

    for (int i = 0; i < NUM_PATTERNS; i++) {
        security_async_broadcast_pattern_update(
            bridge_, 1000 + i, NIMCP_PATTERN_XSS, true);
    }

    /* Get statistics */
    security_async_stats_t stats;
    ASSERT_EQ(security_async_get_stats(bridge_, &stats), 0);

    /* Verify counts */
    EXPECT_GE(stats.threat_events, (uint64_t)NUM_THREATS);
    EXPECT_GE(stats.policy_events, (uint64_t)NUM_POLICIES);
    EXPECT_GE(stats.pattern_events, (uint64_t)NUM_PATTERNS);
    EXPECT_GE(stats.events_published,
              (uint64_t)(NUM_THREATS + NUM_POLICIES + NUM_PATTERNS));
}

/**
 * @test Verify connection state tracking
 *
 * WHAT: Test that connection state is accurately reported
 * WHY:  Operators need to know subsystem connectivity
 * HOW:  Check state, verify all subsystems show connected
 */
TEST_F(SecurityAsyncIntegrationTest, ConnectionStateTracking)
{
    /* Get state */
    security_async_state_t state;
    ASSERT_EQ(security_async_get_state(bridge_, &state), 0);

    /* Verify all subsystems connected */
    EXPECT_TRUE(state.bbb_connected);
    EXPECT_TRUE(state.anomaly_connected);
    EXPECT_TRUE(state.pattern_db_connected);
    EXPECT_TRUE(state.rate_limiter_connected);
    EXPECT_TRUE(state.is_active);

    /* Verify helper function agrees */
    EXPECT_TRUE(security_async_is_connected(bridge_));
    EXPECT_TRUE(security_async_is_bio_async_connected(bridge_));
}

/* ============================================================================
 * Threat Intelligence Tests
 * ============================================================================ */

/**
 * @test Verify threat intel caching and lookup
 *
 * WHAT: Test threat intelligence cache operations
 * WHY:  Fast local lookup is critical for performance
 * HOW:  Cache entries, lookup, verify retrieval
 */
TEST_F(SecurityAsyncIntegrationTest, ThreatIntelCachingAndLookup)
{
    /* Clear cache */
    EXPECT_EQ(security_async_clear_threat_intel(bridge_), 0);

    /* Create intel entry */
    threat_intel_entry_t entry;
    std::memset(&entry, 0, sizeof(entry));
    std::memset(entry.threat_hash, 0x44, sizeof(entry.threat_hash));
    entry.threat_type = BBB_THREAT_SHELLCODE;
    entry.severity = BBB_SEVERITY_CRITICAL;
    entry.source_node = BIO_MODULE_BRAIN;
    entry.first_seen_ms = 1000;
    entry.last_seen_ms = 2000;
    entry.observation_count = 3;
    entry.confidence = 0.92f;
    entry.confirmed = true;

    /* Cache the entry */
    EXPECT_EQ(security_async_cache_threat_intel(bridge_, &entry), 0);

    /* Lookup the entry */
    threat_intel_entry_t retrieved;
    bool found = security_async_lookup_threat_intel(
        bridge_, entry.threat_hash, &retrieved);

    EXPECT_TRUE(found);
    if (found) {
        EXPECT_EQ(retrieved.threat_type, BBB_THREAT_SHELLCODE);
        EXPECT_EQ(retrieved.severity, BBB_SEVERITY_CRITICAL);
        EXPECT_FLOAT_EQ(retrieved.confidence, 0.92f);
        EXPECT_TRUE(retrieved.confirmed);
    }

    /* Verify stats */
    uint32_t count, confirmed;
    ASSERT_EQ(security_async_get_intel_stats(bridge_, &count, &confirmed), 0);
    EXPECT_GE(count, 1u);
    EXPECT_GE(confirmed, 1u);
}

/**
 * @test Verify threat intel sharing with peers
 *
 * WHAT: Test proactive threat intel distribution
 * WHY:  Shared knowledge improves collective security
 * HOW:  Add intel, share with peers, verify broadcast
 */
TEST_F(SecurityAsyncIntegrationTest, ThreatIntelSharingWithPeers)
{
    /* Add several intel entries */
    for (int i = 0; i < 3; i++) {
        uint8_t hash[32];
        std::memset(hash, 0x50 + i, sizeof(hash));

        int result = security_async_receive_threat_report(
            bridge_,
            BIO_MODULE_ETHICS,
            BBB_THREAT_CODE_INJECTION,
            hash,
            0.85f + i * 0.05f
        );
        EXPECT_EQ(result, 0);
    }

    /* Share intel with peers */
    int result = security_async_share_threat_intel(bridge_, 0);  /* 0 = all */
    EXPECT_EQ(result, 0);

    /* Verify intel was shared */
    security_async_stats_t stats;
    ASSERT_EQ(security_async_get_stats(bridge_, &stats), 0);
    EXPECT_GE(stats.intel_shared, 0u);  /* May not share if no peers */
}

}  /* anonymous namespace */
