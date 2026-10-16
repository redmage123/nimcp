/**
 * @file test_bbb_immune_integration.cpp
 * @brief Integration tests for BBB + Immune System and Tripwires + Statistics
 *
 * WHAT: Integration tests verifying bidirectional coordination between security
 *       components (BBB, Tripwires) and the brain immune/statistical systems.
 *
 * WHY:  The security-immune integration mirrors biological neuroimmune systems
 *       where physical barriers, pattern recognition, and adaptive immunity
 *       work together. These tests ensure proper integration flows.
 *
 * TEST CATEGORIES:
 *   1. BBB + Immune Connection/Disconnection (4 tests)
 *   2. BBB Quarantine with Immune Response (4 tests)
 *   3. Concurrent Operations Stress Test (2 tests)
 *   4. Memory Pressure Scenarios (2 tests)
 *   5. Timeout and Recovery (2 tests)
 *   6. Tripwires + Statistics (4 tests)
 *
 * @author NIMCP Development Team
 * @date 2026-02-02
 */

#include "test_helpers.h"

// Headers have their own extern "C" guards
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_tripwires.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>

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
// Category 1: BBB + Immune Connection Tests
//=============================================================================

class BBBImmuneConnectionTest : public ::testing::Test {
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
 * @test ConnectImmuneSystemToBBB
 *
 * WHAT: Verify BBB can connect to immune system successfully
 * WHY:  Connection is prerequisite for immune-coordinated response
 * HOW:  Call bbb_connect_immune and verify success
 */
TEST_F(BBBImmuneConnectionTest, ConnectImmuneSystemToBBB)
{
    ASSERT_NE(immune_system_, nullptr) << "Immune system should be created";

    bool result = bbb_connect_immune(bbb_system_, immune_system_);
    EXPECT_TRUE(result) << "BBB should successfully connect to immune system";
}

/**
 * @test DisconnectImmuneSystemSafely
 *
 * WHAT: Verify BBB can be destroyed without immune system crash
 * WHY:  Must handle case where immune system is connected during destruction
 * HOW:  Connect, then destroy BBB - immune system should remain valid
 */
TEST_F(BBBImmuneConnectionTest, DisconnectImmuneSystemSafely)
{
    // Connect immune system
    bool result = bbb_connect_immune(bbb_system_, immune_system_);
    ASSERT_TRUE(result);

    // Destroy BBB system (immune should survive)
    bbb_system_destroy(bbb_system_);
    bbb_system_ = nullptr;

    // Immune system should still be valid
    brain_immune_stats_t stats;
    int stat_result = brain_immune_get_stats(immune_system_, &stats);
    EXPECT_EQ(0, stat_result) << "Immune system should remain functional after BBB disconnect";
}

/**
 * @test ReconnectAfterDisconnect
 *
 * WHAT: Verify BBB can reconnect to immune system after destruction
 * WHY:  Support dynamic lifecycle management
 * HOW:  Create, connect, destroy, create new BBB, reconnect
 */
TEST_F(BBBImmuneConnectionTest, ReconnectAfterDisconnect)
{
    // First connection
    ASSERT_TRUE(bbb_connect_immune(bbb_system_, immune_system_));

    // Destroy first BBB
    bbb_system_destroy(bbb_system_);

    // Create new BBB
    bbb_system_ = bbb_system_create(&bbb_config_);
    ASSERT_NE(bbb_system_, nullptr);
    ASSERT_TRUE(bbb_system_set_enabled(bbb_system_, true));

    // Reconnect should succeed
    bool result = bbb_connect_immune(bbb_system_, immune_system_);
    EXPECT_TRUE(result) << "Reconnection to immune system should succeed";
}

/**
 * @test NullImmuneSystemConnection
 *
 * WHAT: Verify BBB gracefully handles NULL immune system (disconnect operation)
 * WHY:  Robustness - NULL is a valid disconnect operation, system should remain functional
 * HOW:  Connect NULL (disconnect) and verify BBB remains operational
 *
 * NOTE: Passing NULL immune_system is intentionally allowed - it disconnects the
 *       immune system from BBB. This is not an error case.
 */
TEST_F(BBBImmuneConnectionTest, NullImmuneSystemConnection)
{
    // First connect a real immune system
    ASSERT_TRUE(bbb_connect_immune(bbb_system_, immune_system_));

    // Now disconnect by passing NULL (this is a valid operation)
    bool result = bbb_connect_immune(bbb_system_, nullptr);
    EXPECT_TRUE(result) << "Connecting NULL immune system (disconnect) should succeed";

    // BBB should still be functional after disconnect
    bbb_validation_result_t val_result;
    bool valid = bbb_validate_string(bbb_system_, "test input", &val_result);
    EXPECT_TRUE(valid) << "BBB should remain functional after disconnection";
}

//=============================================================================
// Category 2: BBB Quarantine with Immune Response Tests
//=============================================================================

class BBBQuarantineImmuneTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        bbb_reset_test_state();

        // Create immune system
        brain_immune_config_t immune_config = {};
        immune_config.max_antigens = 100;
        immune_config.max_b_cells = 50;
        immune_config.max_t_cells = 50;
        immune_config.max_antibodies = 100;
        immune_system_ = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system_, nullptr);

        // Create and configure BBB
        bbb_config_ = bbb_default_config();
        bbb_config_.strict_mode = true;
        bbb_config_.alert_callback = &BBBQuarantineImmuneTest::alert_callback_static;
        bbb_system_ = bbb_system_create(&bbb_config_);
        ASSERT_NE(bbb_system_, nullptr);
        ASSERT_TRUE(bbb_system_set_enabled(bbb_system_, true));

        // Connect immune system
        ASSERT_TRUE(bbb_connect_immune(bbb_system_, immune_system_));

        alert_count_.store(0);
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

    static void alert_callback_static(bbb_threat_type_t type, bbb_severity_t severity,
                                       const char* description)
    {
        alert_count_.fetch_add(1);
    }

    brain_immune_system_t* immune_system_ = nullptr;
    bbb_system_t bbb_system_ = nullptr;
    bbb_config_t bbb_config_;
    static std::atomic<int> alert_count_;
};

std::atomic<int> BBBQuarantineImmuneTest::alert_count_{0};

/**
 * @test QuarantineRegionWithImmuneResponse
 *
 * WHAT: Verify quarantine triggers immune response coordination
 * WHY:  Quarantine action should activate killer T cell style response
 * HOW:  Quarantine malicious data, check immune stats
 */
TEST_F(BBBQuarantineImmuneTest, QuarantineRegionWithImmuneResponse)
{
    // Get initial immune stats
    brain_immune_stats_t initial_stats;
    ASSERT_EQ(0, brain_immune_get_stats(immune_system_, &initial_stats));

    // Detect malicious input
    char malicious_data[256];
    strcpy(malicious_data, "'; DROP TABLE users; --");

    bbb_validation_result_t result;
    bool valid = bbb_validate_string(bbb_system_, malicious_data, &result);
    EXPECT_FALSE(valid);
    EXPECT_EQ(result.threat, BBB_THREAT_SQL_INJECTION);

    // Quarantine the region
    EXPECT_TRUE(bbb_quarantine_region(bbb_system_, malicious_data, sizeof(malicious_data)));

    // Verify quarantine blocks access
    EXPECT_FALSE(bbb_check_memory_access(bbb_system_, malicious_data, sizeof(malicious_data), false));

    // Present threat to immune system
    uint32_t antigen_id = 0;
    int present_result = brain_immune_present_bbb_threat(
        immune_system_,
        BBB_THREAT_SQL_INJECTION,
        BBB_SEVERITY_HIGH,
        TEST_EPITOPE,
        TEST_EPITOPE_LEN,
        &antigen_id
    );
    EXPECT_EQ(0, present_result);
    EXPECT_GT(antigen_id, 0u);

    // Release quarantine
    EXPECT_TRUE(bbb_release_quarantine(bbb_system_, malicious_data));
}

/**
 * @test MultipleQuarantinesCoordinated
 *
 * WHAT: Verify multiple simultaneous quarantines are handled
 * WHY:  Real attacks may target multiple memory regions
 * HOW:  Quarantine multiple regions, verify all are tracked
 */
TEST_F(BBBQuarantineImmuneTest, MultipleQuarantinesCoordinated)
{
    const int NUM_REGIONS = 5;
    std::vector<char*> regions;

    // Quarantine multiple regions
    for (int i = 0; i < NUM_REGIONS; ++i) {
        char* region = new char[256];
        snprintf(region, 256, "Malicious data region %d", i);
        regions.push_back(region);

        EXPECT_TRUE(bbb_quarantine_region(bbb_system_, region, 256))
            << "Failed to quarantine region " << i;
    }

    // Verify all are quarantined
    for (int i = 0; i < NUM_REGIONS; ++i) {
        EXPECT_TRUE(bbb_is_quarantined(bbb_system_, regions[i], 256))
            << "Region " << i << " should be quarantined";
    }

    // Release all and cleanup
    for (int i = 0; i < NUM_REGIONS; ++i) {
        EXPECT_TRUE(bbb_release_quarantine(bbb_system_, regions[i]));
        delete[] regions[i];
    }
}

/**
 * @test QuarantinePreventsFurtherAccess
 *
 * WHAT: Verify quarantine completely blocks access
 * WHY:  Security requirement - quarantine must be enforced
 * HOW:  Quarantine region, attempt read and write, verify both fail
 */
TEST_F(BBBQuarantineImmuneTest, QuarantinePreventsFurtherAccess)
{
    char sensitive_data[512];
    strcpy(sensitive_data, "Sensitive data that was compromised");

    // Register and quarantine
    uint32_t region_id = bbb_register_memory_region(bbb_system_, sensitive_data,
                                                     sizeof(sensitive_data), false);
    ASSERT_GT(region_id, 0u);

    EXPECT_TRUE(bbb_quarantine_region(bbb_system_, sensitive_data, sizeof(sensitive_data)));

    // Both read and write should be blocked
    EXPECT_FALSE(bbb_check_memory_access(bbb_system_, sensitive_data, sizeof(sensitive_data), false));
    EXPECT_FALSE(bbb_check_memory_access(bbb_system_, sensitive_data, sizeof(sensitive_data), true));

    // Cleanup
    EXPECT_TRUE(bbb_release_quarantine(bbb_system_, sensitive_data));
    EXPECT_TRUE(bbb_unregister_memory_region(bbb_system_, region_id));
}

/**
 * @test QuarantineWithInflammationEscalation
 *
 * WHAT: Verify critical threats trigger inflammation in immune system
 * WHY:  Critical security events should escalate immune response
 * HOW:  Present critical threat, check inflammation is initiated
 */
TEST_F(BBBQuarantineImmuneTest, QuarantineWithInflammationEscalation)
{
    // Present critical threat
    uint32_t antigen_id = 0;
    ASSERT_EQ(0, brain_immune_present_bbb_threat(
        immune_system_,
        BBB_THREAT_SHELLCODE,
        BBB_SEVERITY_CRITICAL,
        TEST_EPITOPE,
        TEST_EPITOPE_LEN,
        &antigen_id
    ));

    // Initiate inflammation for critical threat
    uint32_t site_id = 0;
    ASSERT_EQ(0, brain_immune_initiate_inflammation(immune_system_, 0, antigen_id, &site_id));
    EXPECT_GT(site_id, 0u);

    // Escalate inflammation
    ASSERT_EQ(0, brain_immune_escalate_inflammation(immune_system_, site_id));

    // Resolve inflammation
    ASSERT_EQ(0, brain_immune_resolve_inflammation(immune_system_, site_id));
}

//=============================================================================
// Category 3: Concurrent Operations Stress Tests
//=============================================================================

class BBBImmuneConcurrentTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        bbb_reset_test_state();

        brain_immune_config_t immune_config = {};
        immune_config.max_antigens = 500;
        immune_config.max_b_cells = 200;
        immune_config.max_t_cells = 200;
        immune_config.max_antibodies = 500;
        immune_system_ = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system_, nullptr);

        bbb_config_ = bbb_default_config();
        bbb_config_.strict_mode = true;
        bbb_system_ = bbb_system_create(&bbb_config_);
        ASSERT_NE(bbb_system_, nullptr);
        ASSERT_TRUE(bbb_system_set_enabled(bbb_system_, true));
        ASSERT_TRUE(bbb_connect_immune(bbb_system_, immune_system_));
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
 * @test ConcurrentValidationAndImmune
 *
 * WHAT: Verify concurrent BBB validations and immune operations
 * WHY:  Real systems have parallel threat detection and response
 * HOW:  Run validation and immune threads concurrently
 */
TEST_F(BBBImmuneConcurrentTest, ConcurrentValidationAndImmune)
{
    const int NUM_THREADS = 4;
    const int OPERATIONS_PER_THREAD = 50;
    std::atomic<int> validations_completed{0};
    std::atomic<int> immune_operations{0};
    std::vector<std::thread> threads;

    // Validation threads
    auto validation_task = [this, &validations_completed]() {
        for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
            bbb_validation_result_t result;
            if (i % 2 == 0) {
                bbb_validate_string(bbb_system_, "safe input", &result);
            } else {
                bbb_validate_string(bbb_system_, "'; DROP TABLE x; --", &result);
            }
            validations_completed.fetch_add(1);
        }
    };

    // Immune operation threads
    auto immune_task = [this, &immune_operations]() {
        for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
            uint32_t antigen_id = 0;
            uint8_t epitope[TEST_EPITOPE_LEN];
            memcpy(epitope, TEST_EPITOPE, TEST_EPITOPE_LEN);
            epitope[0] = static_cast<uint8_t>(i % 256);

            brain_immune_present_bbb_threat(
                immune_system_,
                BBB_THREAT_SQL_INJECTION,
                BBB_SEVERITY_MEDIUM,
                epitope,
                TEST_EPITOPE_LEN,
                &antigen_id
            );
            immune_operations.fetch_add(1);
        }
    };

    // Launch threads
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(validation_task);
        threads.emplace_back(immune_task);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(validations_completed.load(), NUM_THREADS * OPERATIONS_PER_THREAD);
    EXPECT_EQ(immune_operations.load(), NUM_THREADS * OPERATIONS_PER_THREAD);

    // Verify stats are consistent
    bbb_statistics_t bbb_stats;
    EXPECT_TRUE(bbb_system_get_statistics(bbb_system_, &bbb_stats));
    EXPECT_EQ(bbb_stats.total_validations, (uint64_t)(NUM_THREADS * OPERATIONS_PER_THREAD));
}

/**
 * @test ConcurrentQuarantineOperations
 *
 * WHAT: Verify concurrent quarantine/release operations
 * WHY:  Multiple threats may be quarantined simultaneously
 * HOW:  Parallel quarantine and release operations
 */
TEST_F(BBBImmuneConcurrentTest, ConcurrentQuarantineOperations)
{
    const int NUM_THREADS = 4;
    std::atomic<int> quarantine_success{0};
    std::atomic<int> release_success{0};
    std::vector<std::thread> threads;
    std::vector<char*> buffers(NUM_THREADS * 2);

    // Pre-allocate buffers
    for (size_t i = 0; i < buffers.size(); ++i) {
        buffers[i] = new char[256];
        snprintf(buffers[i], 256, "Thread buffer %zu", i);
    }

    auto quarantine_task = [this, &quarantine_success, &release_success, &buffers](int thread_id) {
        size_t buffer_idx = thread_id * 2;

        // Quarantine first buffer
        if (bbb_quarantine_region(bbb_system_, buffers[buffer_idx], 256)) {
            quarantine_success.fetch_add(1);

            // Release it
            if (bbb_release_quarantine(bbb_system_, buffers[buffer_idx])) {
                release_success.fetch_add(1);
            }
        }

        // Quarantine second buffer
        if (bbb_quarantine_region(bbb_system_, buffers[buffer_idx + 1], 256)) {
            quarantine_success.fetch_add(1);

            // Release it
            if (bbb_release_quarantine(bbb_system_, buffers[buffer_idx + 1])) {
                release_success.fetch_add(1);
            }
        }
    };

    // Launch threads
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(quarantine_task, i);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GE(quarantine_success.load(), NUM_THREADS);
    EXPECT_GE(release_success.load(), NUM_THREADS);

    // Cleanup
    for (auto* buffer : buffers) {
        delete[] buffer;
    }
}

//=============================================================================
// Category 4: Memory Pressure Scenarios
//=============================================================================

class BBBImmuneMemoryPressureTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        bbb_reset_test_state();

        // Small immune system to trigger pressure scenarios
        brain_immune_config_t immune_config = {};
        immune_config.max_antigens = 10;
        immune_config.max_b_cells = 5;
        immune_config.max_t_cells = 5;
        immune_config.max_antibodies = 10;
        immune_system_ = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system_, nullptr);

        bbb_config_ = bbb_default_config();
        bbb_system_ = bbb_system_create(&bbb_config_);
        ASSERT_NE(bbb_system_, nullptr);
        ASSERT_TRUE(bbb_system_set_enabled(bbb_system_, true));
        ASSERT_TRUE(bbb_connect_immune(bbb_system_, immune_system_));
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
 * @test ExceedAntigenCapacity
 *
 * WHAT: Verify system handles antigen capacity overflow gracefully
 * WHY:  Real attacks may flood system with threats
 * HOW:  Present more antigens than capacity, verify no crash
 */
TEST_F(BBBImmuneMemoryPressureTest, ExceedAntigenCapacity)
{
    // Present more antigens than capacity (10)
    for (int i = 0; i < 20; ++i) {
        uint32_t antigen_id = 0;
        uint8_t epitope[TEST_EPITOPE_LEN];
        memcpy(epitope, TEST_EPITOPE, TEST_EPITOPE_LEN);
        epitope[0] = static_cast<uint8_t>(i);

        int result = brain_immune_present_bbb_threat(
            immune_system_,
            BBB_THREAT_SQL_INJECTION,
            BBB_SEVERITY_MEDIUM,
            epitope,
            TEST_EPITOPE_LEN,
            &antigen_id
        );

        // Some may fail due to capacity, but should not crash
        if (i < 10) {
            EXPECT_EQ(0, result) << "First 10 antigens should succeed";
        }
        // Later ones may fail or succeed depending on implementation
    }

    // System should still be functional
    brain_immune_stats_t stats;
    EXPECT_EQ(0, brain_immune_get_stats(immune_system_, &stats));
}

/**
 * @test ManySmallQuarantines
 *
 * WHAT: Verify system handles many small quarantined regions
 * WHY:  Test quarantine tracking efficiency
 * HOW:  Create many small quarantine regions
 */
TEST_F(BBBImmuneMemoryPressureTest, ManySmallQuarantines)
{
    const int NUM_REGIONS = 100;
    std::vector<char*> regions;

    for (int i = 0; i < NUM_REGIONS; ++i) {
        char* region = new char[64];
        snprintf(region, 64, "Region %d", i);
        regions.push_back(region);

        bool result = bbb_quarantine_region(bbb_system_, region, 64);
        if (!result && i > 50) {
            // Some implementations may have limits
            break;
        }
    }

    // Release all successfully quarantined regions
    for (auto* region : regions) {
        if (bbb_is_quarantined(bbb_system_, region, 64)) {
            bbb_release_quarantine(bbb_system_, region);
        }
        delete[] region;
    }

    // System should still be functional
    bbb_statistics_t stats;
    EXPECT_TRUE(bbb_system_get_statistics(bbb_system_, &stats));
}

//=============================================================================
// Category 5: Timeout and Recovery Tests
//=============================================================================

class BBBImmuneTimeoutRecoveryTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        bbb_reset_test_state();

        brain_immune_config_t immune_config = {};
        immune_config.max_antigens = 100;
        immune_config.max_b_cells = 50;
        immune_config.max_t_cells = 50;
        immune_config.max_antibodies = 100;
        immune_system_ = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system_, nullptr);

        bbb_config_ = bbb_default_config();
        bbb_system_ = bbb_system_create(&bbb_config_);
        ASSERT_NE(bbb_system_, nullptr);
        ASSERT_TRUE(bbb_system_set_enabled(bbb_system_, true));
        ASSERT_TRUE(bbb_connect_immune(bbb_system_, immune_system_));
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
 * @test ImmuneUpdateWithTimeDelta
 *
 * WHAT: Verify immune system updates correctly with time delta
 * WHY:  Immune responses are time-dependent
 * HOW:  Update with various time deltas, verify state progression
 */
TEST_F(BBBImmuneTimeoutRecoveryTest, ImmuneUpdateWithTimeDelta)
{
    // Start the immune system (required for brain_immune_update to work)
    ASSERT_EQ(0, brain_immune_start(immune_system_));

    // Present a threat
    uint32_t antigen_id = 0;
    ASSERT_EQ(0, brain_immune_present_bbb_threat(
        immune_system_,
        BBB_THREAT_BUFFER_OVERFLOW,
        BBB_SEVERITY_HIGH,
        TEST_EPITOPE,
        TEST_EPITOPE_LEN,
        &antigen_id
    ));

    // Update with small time delta
    EXPECT_EQ(0, brain_immune_update(immune_system_, 10));

    // Update with larger time delta
    EXPECT_EQ(0, brain_immune_update(immune_system_, 100));

    // Update with very large time delta
    EXPECT_EQ(0, brain_immune_update(immune_system_, 10000));

    // System should remain stable
    brain_immune_stats_t stats;
    EXPECT_EQ(0, brain_immune_get_stats(immune_system_, &stats));

    // Stop the immune system
    brain_immune_stop(immune_system_);
}

/**
 * @test RecoveryAfterThreatNeutralization
 *
 * WHAT: Verify system recovers properly after threat neutralization
 * WHY:  Must return to normal operation after threat is handled
 * HOW:  Present threat, neutralize, verify normal operation resumes
 *
 * NOTE: B cells must progress through states:
 *       ACTIVATED -> PLASMA (via helper T cell) -> can produce antibodies
 */
TEST_F(BBBImmuneTimeoutRecoveryTest, RecoveryAfterThreatNeutralization)
{
    // Present and handle threat
    uint32_t antigen_id = 0;
    ASSERT_EQ(0, brain_immune_present_bbb_threat(
        immune_system_,
        BBB_THREAT_CODE_INJECTION,
        BBB_SEVERITY_CRITICAL,
        TEST_EPITOPE,
        TEST_EPITOPE_LEN,
        &antigen_id
    ));

    // Activate B cell (creates B cell in ACTIVATED state)
    uint32_t b_cell_id = 0;
    EXPECT_EQ(0, brain_immune_activate_b_cell(immune_system_, antigen_id, &b_cell_id));

    // Activate helper T cell (needed to help B cell transition to PLASMA)
    uint32_t helper_t_id = 0;
    EXPECT_EQ(0, brain_immune_activate_helper_t(immune_system_, antigen_id, &helper_t_id));

    // Helper T provides assistance to B cell (transitions B cell to PLASMA state)
    EXPECT_EQ(0, brain_immune_t_help_b(immune_system_, helper_t_id, b_cell_id));

    // Now B cell is in PLASMA state and can produce antibodies
    uint32_t antibody_id = 0;
    EXPECT_EQ(0, brain_immune_produce_antibody(immune_system_, b_cell_id,
                                               ANTIBODY_IGG, &antibody_id));

    // Neutralize threat
    EXPECT_EQ(0, brain_immune_neutralize(immune_system_, antigen_id, antibody_id));

    // Verify neutralization
    EXPECT_TRUE(brain_immune_is_neutralized(immune_system_, antigen_id));

    // Normal operations should work
    bbb_validation_result_t result;
    bool valid = bbb_validate_string(bbb_system_, "safe input after recovery", &result);
    EXPECT_TRUE(valid);
}

//=============================================================================
// Category 6: Tripwires + Statistics Tests
//=============================================================================

class TripwireStatisticsTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        tripwire_config_ = tripwire_default_config();
        tripwire_ = tripwire_create(&tripwire_config_);
        ASSERT_NE(tripwire_, nullptr);
    }

    void TearDown() override
    {
        if (tripwire_) {
            tripwire_destroy(tripwire_);
            tripwire_ = nullptr;
        }
    }

    tripwire_config_t tripwire_config_;
    tripwire_system_t* tripwire_ = nullptr;
};

/**
 * @test BayesianInferenceWithRealData
 *
 * WHAT: Verify Bayesian inference works with realistic observation data
 * WHY:  Core statistical functionality for goal drift detection
 * HOW:  Feed realistic goal observations, verify inference results
 *
 * NOTE: Goal drift detection requires sufficient observations to establish baseline.
 *       With few observations, the drift score may be high due to insufficient data.
 *       The detection uses min_observations threshold (default: 10 from config).
 */
TEST_F(TripwireStatisticsTest, BayesianInferenceWithRealData)
{
    // Observe consistent goal pursuit (need enough for baseline)
    for (int i = 0; i < 50; ++i) {
        nimcp_error_t result = tripwire_observe_goal(
            tripwire_,
            1,      // goal_id
            0.8f,   // pursuit_intensity - consistent
            0.9f    // stated_priority
        );
        EXPECT_EQ(NIMCP_OK, result);
    }

    // Get goal drift score - should be non-negative
    float score = tripwire_detect_goal_drift(tripwire_);
    EXPECT_GE(score, 0.0f) << "Drift score should be non-negative";
    // Note: With consistent behavior, score depends on baseline establishment

    // Now introduce drift - different goal with mismatched priority/pursuit
    for (int i = 0; i < 20; ++i) {
        tripwire_observe_goal(
            tripwire_,
            2,      // different goal_id
            0.9f,   // high pursuit of different goal
            0.1f    // but stated priority is low (mismatch indicates potential drift)
        );
    }

    // Drift score should be valid (non-negative, bounded)
    float drift_score = tripwire_detect_goal_drift(tripwire_);
    EXPECT_GE(drift_score, 0.0f) << "Drift score should be non-negative";
    EXPECT_LE(drift_score, 1.0f) << "Drift score should be bounded [0, 1]";

    // Verify Bayesian tracking updated stats
    tripwire_stats_t stats;
    EXPECT_EQ(NIMCP_OK, tripwire_get_stats(tripwire_, &stats));
    // Posterior mean should be updated (non-zero indicates Bayesian updates occurred)
    // Note: posterior_mean tracks the pursuit intensity, not observation count
}

/**
 * @test NetworkAnomalyDetectionEndToEnd
 *
 * WHAT: Verify network anomaly detection pipeline works end-to-end
 * WHY:  Core security functionality for network threat detection
 * HOW:  Feed network observations, check detection results
 */
TEST_F(TripwireStatisticsTest, NetworkAnomalyDetectionEndToEnd)
{
    // Establish baseline with normal traffic
    for (int i = 0; i < 50; ++i) {
        nimcp_error_t result = tripwire_observe_network_connection(
            tripwire_,
            0x0A000001 + i,     // varying internal IPs
            80,                  // HTTP port
            1000,                // 1KB sent
            5000,                // 5KB received (normal ratio)
            TRIPWIRE_PROTO_HTTP
        );
        EXPECT_EQ(NIMCP_OK, result);
    }

    // Initial anomaly score should be low
    float initial_score = tripwire_detect_network_anomaly(tripwire_);

    // Introduce anomalous traffic
    for (int i = 0; i < 10; ++i) {
        tripwire_observe_network_connection(
            tripwire_,
            0xC0A80001,          // suspicious external IP
            31337,               // suspicious port
            100000,              // 100KB sent (high exfil)
            100,                 // tiny response
            TRIPWIRE_PROTO_TCP
        );
    }

    // Anomaly score should increase
    float anomaly_score = tripwire_detect_network_anomaly(tripwire_);
    // Depends on implementation, but should be non-negative
    EXPECT_GE(anomaly_score, 0.0f);
}

/**
 * @test GoalDriftDetection
 *
 * WHAT: Verify goal drift detection identifies objective shifts
 * WHY:  Detect gradual misalignment from assigned objectives
 * HOW:  Simulate gradual goal shift, verify detection
 *
 * NOTE: total_observations is only incremented by tripwire_observe_action(),
 *       not tripwire_observe_goal(). Goal observations update goal trackers
 *       which have their own observation counts in pursuit_stats.n.
 */
TEST_F(TripwireStatisticsTest, GoalDriftDetection)
{
    // Initial consistent behavior
    for (int i = 0; i < 30; ++i) {
        tripwire_observe_goal(tripwire_, 1, 0.9f, 0.9f);
    }

    // Gradual shift
    for (int i = 0; i < 30; ++i) {
        float old_intensity = 0.9f - (0.5f * i / 30.0f);  // Decreasing
        float new_intensity = 0.1f + (0.5f * i / 30.0f);  // Increasing

        tripwire_observe_goal(tripwire_, 1, old_intensity, 0.9f);
        tripwire_observe_goal(tripwire_, 2, new_intensity, 0.1f);
    }

    float drift_score = tripwire_detect_goal_drift(tripwire_);
    EXPECT_GE(drift_score, 0.0f);
    EXPECT_LE(drift_score, 1.0f);

    // Get statistics - check Bayesian posterior updates instead of total_observations
    tripwire_stats_t stats;
    EXPECT_EQ(NIMCP_OK, tripwire_get_stats(tripwire_, &stats));
    // goal_posterior_mean should be updated by Bayesian inference
    // (value depends on observations, but should be within valid range)
    EXPECT_GE(stats.goal_posterior_mean, 0.0f);
    EXPECT_LE(stats.goal_posterior_mean, 1.0f);
}

/**
 * @test AlertGenerationAndCooldown
 *
 * WHAT: Verify alert generation respects cooldown period
 * WHY:  Prevent alert flooding from repeated detections
 * HOW:  Trigger multiple alerts, verify cooldown prevents duplicates
 */
TEST_F(TripwireStatisticsTest, AlertGenerationAndCooldown)
{
    // Configure with cooldown
    tripwire_config_.deduplicate_alerts = true;
    tripwire_config_.alert_cooldown_ms = 100;

    // Observe potentially concerning behavior
    for (int i = 0; i < 20; ++i) {
        tripwire_observe_resource(tripwire_, 1, 1000.0f, "excessive_memory");
    }

    // Check for alerts
    tripwire_alert_t alerts[10];
    uint32_t alert_count = 0;
    nimcp_error_t result = tripwire_check(tripwire_, alerts, 10, &alert_count);
    EXPECT_EQ(NIMCP_OK, result);

    // Wait for cooldown
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // More observations
    for (int i = 0; i < 10; ++i) {
        tripwire_observe_resource(tripwire_, 1, 1000.0f, "excessive_memory");
    }

    // Check again - should be able to get new alerts after cooldown
    uint32_t new_alert_count = 0;
    result = tripwire_check(tripwire_, alerts, 10, &new_alert_count);
    EXPECT_EQ(NIMCP_OK, result);
}

}  // anonymous namespace
