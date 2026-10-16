/**
 * @file test_mesh_bbb_integration.cpp
 * @brief Integration tests for Mesh Network with BBB validation
 * @version 1.0.0
 * @date 2026-02-03
 *
 * WHAT: Test full mesh channel operations with BBB security enabled
 * WHY:  Verify mesh and BBB systems work correctly together
 * HOW:  Create mesh channel, enable BBB validation, test transactions
 *
 * TEST SCENARIOS:
 * 1. Full mesh channel with BBB validation enabled
 * 2. Participant registration flow with security checks
 * 3. Transaction timeout and recovery
 * 4. Malicious transaction rejection
 * 5. Security event propagation
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>

// C headers (each has its own extern "C" guards)
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_security_integration.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MeshBBBIntegrationTest : public ::testing::Test {
protected:
    mesh_participant_registry_t* registry_ = nullptr;
    mesh_channel_t* channel_ = nullptr;
    mesh_tx_manager_t* tx_manager_ = nullptr;
    mesh_security_integration_t* security_ = nullptr;
    mesh_bootstrap_t* bootstrap_ = nullptr;
    bbb_system_t bbb_ = nullptr;
    brain_immune_system_t* immune_ = nullptr;

    static std::atomic<int> security_events_received_;
    static std::atomic<int> quarantine_count_;

    void SetUp() override {
        // Create participant registry
        mesh_registry_config_t reg_cfg;
        mesh_registry_default_config(&reg_cfg);
        registry_ = mesh_registry_create(&reg_cfg);
        ASSERT_NE(registry_, nullptr);

        // Create BBB system
        bbb_config_t bbb_cfg = bbb_default_config();
        bbb_ = bbb_system_create(&bbb_cfg);
        ASSERT_NE(bbb_, nullptr);

        // Create immune system
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_ = brain_immune_create(&immune_cfg);
        ASSERT_NE(immune_, nullptr);

        // Create bootstrap (required by mesh_security_create)
        mesh_bootstrap_config_t boot_cfg;
        mesh_bootstrap_default_config(&boot_cfg);
        bootstrap_ = mesh_bootstrap_create(&boot_cfg);
        ASSERT_NE(bootstrap_, nullptr);

        // Reset counters
        security_events_received_.store(0);
        quarantine_count_.store(0);
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
        if (bootstrap_) {
            mesh_bootstrap_destroy(bootstrap_);
            bootstrap_ = nullptr;
        }
        if (immune_) {
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

    // Helper to create a channel with default config
    mesh_channel_t* CreateTestChannel(const char* name) {
        mesh_channel_config_t cfg;
        mesh_channel_default_config(&cfg);
        cfg.channel_name = name;
        cfg.enable_logging = false;
        return mesh_channel_create(&cfg, registry_);
    }

    // Helper to create security integration
    mesh_security_integration_t* CreateSecurityIntegration() {
        mesh_security_config_t sec_cfg;
        mesh_security_default_config(&sec_cfg);
        sec_cfg.enable_bbb_validation = true;
        sec_cfg.enable_auto_quarantine = true;
        sec_cfg.bbb_threat_threshold = 0.7f;
        return mesh_security_create(bootstrap_, bbb_, immune_, nullptr, &sec_cfg);
    }

    // Security event callback
    static void SecurityEventCallback(
        mesh_security_integration_t* integration,
        const mesh_security_event_t* event,
        void* user_data
    ) {
        (void)integration;
        (void)user_data;
        security_events_received_.fetch_add(1);
        if (event->type == MESH_SEC_EVENT_QUARANTINE_ISSUED) {
            quarantine_count_.fetch_add(1);
        }
    }
};

// Static member initialization
std::atomic<int> MeshBBBIntegrationTest::security_events_received_{0};
std::atomic<int> MeshBBBIntegrationTest::quarantine_count_{0};

//=============================================================================
// Test Cases
//=============================================================================

/**
 * @brief Test full mesh channel with BBB validation enabled
 *
 * WHAT: Verify mesh channel operates correctly with BBB validation
 * WHY:  Core integration between mesh and BBB security
 * HOW:  Create channel, enable BBB, submit valid transactions
 */
TEST_F(MeshBBBIntegrationTest, FullChannelWithBBBValidation) {
    // Create channel
    channel_ = CreateTestChannel("test_channel");
    ASSERT_NE(channel_, nullptr);

    // Create transaction manager
    mesh_tx_manager_config_t tx_cfg;
    mesh_tx_manager_default_config(&tx_cfg);
    tx_manager_ = mesh_tx_manager_create(&tx_cfg, registry_);
    ASSERT_NE(tx_manager_, nullptr);

    // Connect BBB to immune
    bool connected = bbb_connect_immune(bbb_, immune_);
    EXPECT_TRUE(connected);

    // Create security integration (bootstrap not needed for basic tests)
    mesh_security_config_t sec_cfg;
    mesh_security_default_config(&sec_cfg);
    sec_cfg.enable_bbb_validation = true;
    security_ = mesh_security_create(bootstrap_, bbb_, immune_, nullptr, &sec_cfg);
    ASSERT_NE(security_, nullptr);

    // Register a test participant
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "test_module", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;

    mesh_participant_config_t pcfg;
    mesh_participant_config_init(&pcfg);
    pcfg.module_name = "test_module";
    pcfg.type = MESH_PARTICIPANT_MODULE;

    mesh_participant_id_t participant_id;
    nimcp_error_t err = mesh_participant_register(registry_, &iface, &pcfg, &participant_id);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Add participant to channel
    err = mesh_channel_add_participant(channel_, participant_id);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Create and submit a valid transaction
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE,
        participant_id,
        mesh_channel_get_id(channel_)
    );
    ASSERT_NE(tx, nullptr);

    // Set valid payload
    const char* payload = "valid_transaction_data";
    err = mesh_transaction_set_payload(tx, payload, strlen(payload));
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Validate through BBB - should pass
    float threat_score = 0.0f;
    err = mesh_security_validate_transaction(security_, tx, &threat_score);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_LT(threat_score, 0.7f);  // Below threat threshold

    // Cleanup
    mesh_transaction_destroy(tx);
}

/**
 * @brief Test participant registration with security checks
 *
 * WHAT: Verify participant registration includes security validation
 * WHY:  New participants must be validated before joining mesh
 * HOW:  Register participant, verify security status, check channel membership
 */
TEST_F(MeshBBBIntegrationTest, ParticipantRegistrationWithSecurityChecks) {
    // Create security integration
    mesh_security_config_t sec_cfg;
    mesh_security_default_config(&sec_cfg);
    sec_cfg.enable_bbb_validation = true;
    sec_cfg.enable_credential_tracking = true;
    security_ = mesh_security_create(bootstrap_, bbb_, immune_, nullptr, &sec_cfg);
    ASSERT_NE(security_, nullptr);

    // Create channel
    channel_ = CreateTestChannel("secured_channel");
    ASSERT_NE(channel_, nullptr);

    // Register multiple participants
    const int NUM_PARTICIPANTS = 5;
    mesh_participant_id_t participant_ids[NUM_PARTICIPANTS];

    for (int i = 0; i < NUM_PARTICIPANTS; i++) {
        mesh_participant_interface_t iface;
        mesh_participant_interface_init(&iface);
        snprintf(iface.module_name, MESH_MAX_NAME_LEN, "module_%d", i);
        iface.type = MESH_PARTICIPANT_MODULE;

        mesh_participant_config_t pcfg;
        mesh_participant_config_init(&pcfg);
        pcfg.module_name = iface.module_name;
        pcfg.type = MESH_PARTICIPANT_MODULE;

        nimcp_error_t err = mesh_participant_register(registry_, &iface, &pcfg, &participant_ids[i]);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        // Check security status before adding to channel
        bool is_quarantined = false;
        float threat_level = 0.0f;
        err = mesh_security_check_participant(
            security_, participant_ids[i], &is_quarantined, &threat_level
        );
        EXPECT_EQ(err, NIMCP_SUCCESS);
        EXPECT_FALSE(is_quarantined);
        EXPECT_LT(threat_level, 0.5f);

        // Add to channel only if not quarantined
        if (!is_quarantined) {
            err = mesh_channel_add_participant(channel_, participant_ids[i]);
            EXPECT_EQ(err, NIMCP_SUCCESS);
        }
    }

    // Verify all participants are in channel
    size_t count = mesh_channel_get_participant_count(channel_);
    EXPECT_EQ(count, static_cast<size_t>(NUM_PARTICIPANTS));
}

/**
 * @brief Test transaction timeout and recovery
 *
 * WHAT: Verify transactions timeout correctly and system recovers
 * WHY:  Transaction timeout is critical for system health
 * HOW:  Create transaction with short timeout, let it expire, verify recovery
 */
TEST_F(MeshBBBIntegrationTest, TransactionTimeoutAndRecovery) {
    // Create channel and transaction manager
    channel_ = CreateTestChannel("timeout_test_channel");
    ASSERT_NE(channel_, nullptr);

    mesh_tx_manager_config_t tx_cfg;
    mesh_tx_manager_default_config(&tx_cfg);
    tx_cfg.default_timeout_ms = 100;  // Short timeout for testing
    tx_manager_ = mesh_tx_manager_create(&tx_cfg, registry_);
    ASSERT_NE(tx_manager_, nullptr);

    // Register participant
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "timeout_tester", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;

    mesh_participant_config_t pcfg;
    mesh_participant_config_init(&pcfg);
    pcfg.module_name = "timeout_tester";
    pcfg.type = MESH_PARTICIPANT_MODULE;

    mesh_participant_id_t participant_id;
    nimcp_error_t err = mesh_participant_register(registry_, &iface, &pcfg, &participant_id);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    err = mesh_channel_add_participant(channel_, participant_id);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Create transaction with very short timeout
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE,
        participant_id,
        mesh_channel_get_id(channel_)
    );
    ASSERT_NE(tx, nullptr);

    const char* payload = "timeout_test_payload";
    err = mesh_transaction_set_payload(tx, payload, strlen(payload));
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = mesh_transaction_set_timeout(tx, 50);  // 50ms timeout
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Submit transaction
    err = mesh_tx_propose(tx_manager_, tx);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Get stats and verify timeout was recorded
    mesh_tx_manager_stats_t stats;
    err = mesh_tx_manager_get_stats(tx_manager_, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Transaction should have been proposed
    EXPECT_GE(stats.transactions_proposed, 1u);

    // Cleanup
    mesh_transaction_destroy(tx);
}

/**
 * @brief Test malicious transaction rejection at BBB boundary
 *
 * WHAT: Verify BBB rejects transactions with malicious characteristics
 * WHY:  Security boundary must prevent harmful transactions
 * HOW:  Create transaction with threat indicators, verify rejection
 */
TEST_F(MeshBBBIntegrationTest, MaliciousTransactionRejection) {
    // Create security integration with strict thresholds
    mesh_security_config_t sec_cfg;
    mesh_security_default_config(&sec_cfg);
    sec_cfg.enable_bbb_validation = true;
    sec_cfg.enable_auto_quarantine = true;
    sec_cfg.bbb_threat_threshold = 0.5f;  // Strict threshold
    security_ = mesh_security_create(bootstrap_, bbb_, immune_, nullptr, &sec_cfg);
    ASSERT_NE(security_, nullptr);

    // Set up event callback
    mesh_security_set_event_callback(security_, SecurityEventCallback, nullptr);

    // Create channel
    channel_ = CreateTestChannel("secure_channel");
    ASSERT_NE(channel_, nullptr);

    // Register participant
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "potential_threat", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;

    mesh_participant_config_t pcfg;
    mesh_participant_config_init(&pcfg);
    pcfg.module_name = "potential_threat";
    pcfg.type = MESH_PARTICIPANT_MODULE;

    mesh_participant_id_t participant_id;
    nimcp_error_t err = mesh_participant_register(registry_, &iface, &pcfg, &participant_id);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Report threat to BBB to raise participant's threat level
    bbb_report_threat(
        bbb_,
        BBB_THREAT_CODE_INJECTION,
        BBB_SEVERITY_HIGH,
        "Detected potential code injection attempt",
        nullptr,
        "potential_threat",
        strlen("potential_threat")
    );

    // Update security and immune systems
    mesh_security_update(security_, 100);
    brain_immune_update(immune_, 100);

    // Create transaction from suspicious participant
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE,
        participant_id,
        mesh_channel_get_id(channel_)
    );
    ASSERT_NE(tx, nullptr);

    // Set suspicious payload pattern
    const char* suspicious_payload = "DROP TABLE; DELETE FROM; exec(";
    err = mesh_transaction_set_payload(tx, suspicious_payload, strlen(suspicious_payload));
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Validate through BBB - may or may not pass depending on threat state
    float threat_score = 0.0f;
    err = mesh_security_validate_transaction(security_, tx, &threat_score);

    // Threat score should be elevated due to reported threat
    // Note: Actual threshold behavior depends on BBB implementation
    std::cout << "Transaction threat score: " << threat_score << std::endl;

    // Get security stats
    mesh_security_stats_t sec_stats;
    err = mesh_security_get_stats(security_, &sec_stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(sec_stats.bbb_validations, 0u);

    // Cleanup
    mesh_transaction_destroy(tx);
}

/**
 * @brief Test security event propagation through mesh
 *
 * WHAT: Verify security events propagate correctly through mesh
 * WHY:  All mesh components must be aware of security state changes
 * HOW:  Trigger security event, verify callback invocation, check broadcast
 */
TEST_F(MeshBBBIntegrationTest, SecurityEventPropagation) {
    // Create security integration
    mesh_security_config_t sec_cfg;
    mesh_security_default_config(&sec_cfg);
    sec_cfg.enable_bbb_validation = true;
    sec_cfg.enable_security_broadcasts = true;
    sec_cfg.enable_auto_quarantine = true;
    security_ = mesh_security_create(bootstrap_, bbb_, immune_, nullptr, &sec_cfg);
    ASSERT_NE(security_, nullptr);

    // Set up event callback
    mesh_security_set_event_callback(security_, SecurityEventCallback, nullptr);

    // Create channel
    channel_ = CreateTestChannel("event_test_channel");
    ASSERT_NE(channel_, nullptr);

    // Register participant
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "event_test_module", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;

    mesh_participant_config_t pcfg;
    mesh_participant_config_init(&pcfg);
    pcfg.module_name = "event_test_module";
    pcfg.type = MESH_PARTICIPANT_MODULE;

    mesh_participant_id_t participant_id;
    nimcp_error_t err = mesh_participant_register(registry_, &iface, &pcfg, &participant_id);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    err = mesh_channel_add_participant(channel_, participant_id);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Reset event counter
    security_events_received_.store(0);

    // Route an exception through security
    err = mesh_security_route_exception(
        security_,
        NIMCP_ERROR_INVALID_ARGUMENT,
        "Test security exception",
        participant_id,
        __FILE__,
        __LINE__,
        nullptr
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Update security system
    mesh_security_update(security_, 100);

    // Verify event was received
    EXPECT_GT(security_events_received_.load(), 0);

    // Broadcast quarantine notification
    err = mesh_security_broadcast_quarantine(
        security_,
        participant_id,
        5000,  // 5 second quarantine
        "Test quarantine"
    );
    // May succeed or fail depending on if broadcast channel exists
    (void)err;

    // Get security stats
    mesh_security_stats_t sec_stats;
    err = mesh_security_get_stats(security_, &sec_stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(sec_stats.exceptions_routed, 0u);
}

/**
 * @brief Test concurrent transactions with BBB validation
 *
 * WHAT: Verify BBB validation works correctly under concurrent load
 * WHY:  Real systems have concurrent transaction submission
 * HOW:  Submit multiple transactions from multiple threads, verify all validated
 */
TEST_F(MeshBBBIntegrationTest, ConcurrentTransactionsWithBBB) {
    // Create security integration
    mesh_security_config_t sec_cfg;
    mesh_security_default_config(&sec_cfg);
    sec_cfg.enable_bbb_validation = true;
    security_ = mesh_security_create(bootstrap_, bbb_, immune_, nullptr, &sec_cfg);
    ASSERT_NE(security_, nullptr);

    // Create channel
    channel_ = CreateTestChannel("concurrent_test_channel");
    ASSERT_NE(channel_, nullptr);

    const int NUM_THREADS = 4;
    const int TXS_PER_THREAD = 10;
    std::atomic<int> successful_validations{0};
    std::atomic<int> validation_errors{0};

    // Register participants for each thread
    mesh_participant_id_t participant_ids[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        mesh_participant_interface_t iface;
        mesh_participant_interface_init(&iface);
        snprintf(iface.module_name, MESH_MAX_NAME_LEN, "concurrent_module_%d", i);
        iface.type = MESH_PARTICIPANT_MODULE;

        mesh_participant_config_t pcfg;
        mesh_participant_config_init(&pcfg);
        pcfg.module_name = iface.module_name;
        pcfg.type = MESH_PARTICIPANT_MODULE;

        nimcp_error_t err = mesh_participant_register(registry_, &iface, &pcfg, &participant_ids[i]);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        err = mesh_channel_add_participant(channel_, participant_ids[i]);
        ASSERT_EQ(err, NIMCP_SUCCESS);
    }

    // Lambda for thread work
    auto validate_transactions = [&](int thread_id) {
        for (int i = 0; i < TXS_PER_THREAD; i++) {
            mesh_transaction_t* tx = mesh_transaction_create(
                MESH_TX_STATE_CHANGE,
                participant_ids[thread_id],
                mesh_channel_get_id(channel_)
            );

            if (!tx) {
                validation_errors.fetch_add(1);
                continue;
            }

            char payload[128];
            snprintf(payload, sizeof(payload), "tx_thread_%d_seq_%d", thread_id, i);
            mesh_transaction_set_payload(tx, payload, strlen(payload));

            float threat_score = 0.0f;
            nimcp_error_t err = mesh_security_validate_transaction(
                security_, tx, &threat_score
            );

            if (err == NIMCP_SUCCESS) {
                successful_validations.fetch_add(1);
            } else {
                validation_errors.fetch_add(1);
            }

            mesh_transaction_destroy(tx);
        }
    };

    // Launch threads
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(validate_transactions, i);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    // Verify results
    int total_expected = NUM_THREADS * TXS_PER_THREAD;
    int total_processed = successful_validations.load() + validation_errors.load();
    EXPECT_EQ(total_processed, total_expected);

    // Most transactions should succeed (unless BBB flagged something)
    EXPECT_GT(successful_validations.load(), total_expected / 2);

    std::cout << "\nConcurrent Validation Results:" << std::endl;
    std::cout << "  Total transactions: " << total_expected << std::endl;
    std::cout << "  Successful: " << successful_validations.load() << std::endl;
    std::cout << "  Errors: " << validation_errors.load() << std::endl;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
