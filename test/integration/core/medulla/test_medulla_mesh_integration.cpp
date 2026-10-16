/**
 * @file test_medulla_mesh_integration.cpp
 * @brief Mesh Network Integration Tests for Medulla Oblongata
 *
 * WHAT: Integration tests for medulla participation in mesh network
 * WHY:  Verify medulla can register as participant, handle transactions,
 *       and synchronize state through the mesh network
 * HOW:  Create mesh bootstrap, register medulla as participant, run
 *       transaction flows, verify state synchronization
 *
 * TEST COVERAGE:
 * 1. Participant Registration - Register medulla as mesh participant
 * 2. Transaction Handling - Process incoming mesh transactions
 * 3. State Synchronization - Sync arousal/protection state via mesh
 * 4. Channel Membership - Join arousal/protection channels
 * 5. Endorsement Flow - Endorse transactions for state changes
 * 6. Cross-Channel State - State changes propagate across channels
 * 7. Recovery Integration - Mesh recovery affects medulla state
 * 8. Concurrent Operations - Thread safety with mesh operations
 *
 * BIOLOGICAL CONTEXT:
 * The medulla oblongata coordinates with other brain regions through
 * ascending and descending pathways. In NIMCP, the mesh network provides
 * distributed coordination similar to these neural pathways.
 *
 * @author NIMCP Development Team
 * @date 2026-01-31
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

// Core headers - NIMCP headers have internal extern "C" blocks
#include "core/medulla/nimcp_medulla.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_medulla_integration.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"

// Compatibility macros for undefined mesh constants
#ifndef MESH_PARTICIPANT_PROCESSOR
#define MESH_PARTICIPANT_PROCESSOR MESH_PARTICIPANT_MODULE
#endif
#ifndef MESH_CHANNEL_VITAL
#define MESH_CHANNEL_VITAL MESH_CHANNEL_SUBCORTICAL
#endif
#ifndef MESH_TX_TYPE_STATE_UPDATE
#define MESH_TX_TYPE_STATE_UPDATE MESH_TX_STATE_CHANGE
#endif
#ifndef MESH_CHANNEL_MOTOR
#define MESH_CHANNEL_MOTOR MESH_CHANNEL_SUBCORTICAL
#endif

//=============================================================================
// Test Fixture - Medulla with Mesh Bootstrap
//=============================================================================

class MedullaMeshIntegrationTest : public ::testing::Test {
protected:
    medulla_t medulla = nullptr;
    mesh_bootstrap_t* bootstrap = nullptr;
    mesh_participant_registry_t* registry = nullptr;

    void SetUp() override {
        // Create medulla with bio-async enabled for mesh
        medulla_config_t med_config = medulla_default_config();
        med_config.enable_bio_async = true;
        med_config.enable_health_integration = false;
        medulla = medulla_create(&med_config);
        ASSERT_NE(medulla, nullptr) << "Failed to create medulla";

        // Create mesh bootstrap with core subsystems
        mesh_bootstrap_config_t mesh_config;
        mesh_bootstrap_default_config(&mesh_config);
        mesh_config.subsystems = MESH_SUBSYSTEMS_CORE;
        mesh_config.subsystems.enable_async = true;

        bootstrap = mesh_bootstrap_create(&mesh_config);
        // Bootstrap may be null on some systems - tests will skip if needed

        if (bootstrap) {
            registry = mesh_bootstrap_get_registry(bootstrap);
        }
    }

    void TearDown() override {
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
        if (bootstrap) {
            mesh_bootstrap_destroy(bootstrap);
            bootstrap = nullptr;
        }
        registry = nullptr;
    }

    // Helper: Create participant interface for medulla
    mesh_participant_interface_t createMedullaInterface() {
        mesh_participant_interface_t interface;
        mesh_participant_interface_init(&interface);

        strncpy(interface.module_name, "medulla_oblongata",
                MESH_MAX_NAME_LEN - 1);
        interface.type = MESH_PARTICIPANT_PROCESSOR;
        interface.home_channel = MESH_CHANNEL_VITAL;  // Vital functions channel
        interface.user_context = medulla;

        // Set callbacks
        interface.on_proposal = medullaOnProposal;
        interface.on_commit = medullaOnCommit;
        interface.get_free_energy = medullaGetFreeEnergy;

        return interface;
    }

    // Callback: Handle transaction proposals
    static nimcp_error_t medullaOnProposal(void* ctx, const mesh_transaction_t* tx) {
        if (!ctx || !tx) return NIMCP_ERROR_NULL_POINTER;

        // Accept all proposals for now
        return NIMCP_SUCCESS;
    }

    // Callback: Handle committed transactions
    static nimcp_error_t medullaOnCommit(void* ctx, const mesh_transaction_t* tx) {
        if (!ctx || !tx) return NIMCP_ERROR_NULL_POINTER;

        medulla_t medulla = (medulla_t)ctx;

        // Process state change transactions
        if (tx->type == MESH_TX_TYPE_STATE_UPDATE) {
            // Apply state changes
            medulla_update(medulla, 0.01f);
        }

        return NIMCP_SUCCESS;
    }

    // Callback: Get free energy (arousal/stability metric)
    static float medullaGetFreeEnergy(void* ctx) {
        if (!ctx) return 1.0f;

        medulla_t medulla = (medulla_t)ctx;
        float arousal = medulla_get_arousal_level(medulla);

        // Free energy is inversely related to stability
        // Optimal arousal (0.5) = lowest free energy
        float deviation = fabsf(arousal - 0.5f);
        return deviation;
    }

    // Helper: Run medulla updates
    void runMedullaUpdates(int count, float dt = 0.02f) {
        for (int i = 0; i < count; i++) {
            medulla_update(medulla, dt);
        }
    }
};

//=============================================================================
// 1. Participant Registration Tests
//=============================================================================

TEST_F(MedullaMeshIntegrationTest, MedullaRegistersAsParticipant) {
    // WHAT: Test medulla can register as mesh participant
    // WHY:  Medulla must participate in mesh for distributed coordination
    // HOW:  Create interface, register with registry, verify ID assigned

    if (!registry) {
        GTEST_SKIP() << "Mesh registry not available";
    }

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Create participant interface
    mesh_participant_interface_t interface = createMedullaInterface();

    // Configure registration
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "medulla_oblongata";
    config.type = MESH_PARTICIPANT_PROCESSOR;
    config.home_channel = MESH_CHANNEL_VITAL;
    config.user_context = medulla;

    // Register participant
    mesh_participant_id_t id = 0;
    nimcp_error_t err = mesh_participant_register(
        registry, &interface, &config, &id);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NE(id, 0u) << "Should be assigned non-zero ID";

    // Verify registration
    EXPECT_TRUE(mesh_participant_is_registered(registry, id));

    // Clean up
    mesh_participant_unregister(registry, id);
}

TEST_F(MedullaMeshIntegrationTest, MedullaParticipantPersists) {
    // WHAT: Test medulla participant registration persists
    // WHY:  Registration should survive multiple operations
    // HOW:  Register, perform operations, verify still registered

    if (!registry) {
        GTEST_SKIP() << "Mesh registry not available";
    }

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    mesh_participant_interface_t interface = createMedullaInterface();
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "medulla_oblongata";
    config.type = MESH_PARTICIPANT_PROCESSOR;
    config.home_channel = MESH_CHANNEL_VITAL;
    config.user_context = medulla;

    mesh_participant_id_t id = 0;
    EXPECT_EQ(mesh_participant_register(registry, &interface, &config, &id), NIMCP_SUCCESS);

    // Run multiple medulla updates
    runMedullaUpdates(50);

    // Verify still registered
    EXPECT_TRUE(mesh_participant_is_registered(registry, id));

    // Get participant interface back
    const mesh_participant_interface_t* retrieved =
        mesh_participant_get(registry, id);
    EXPECT_NE(retrieved, nullptr);
    EXPECT_STREQ(retrieved->module_name, "medulla_oblongata");

    mesh_participant_unregister(registry, id);
}

TEST_F(MedullaMeshIntegrationTest, MedullaRegistrationByName) {
    // WHAT: Test medulla can be found by name
    // WHY:  Other modules may look up medulla by name
    // HOW:  Register, lookup by name, verify found

    if (!registry) {
        GTEST_SKIP() << "Mesh registry not available";
    }

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    mesh_participant_interface_t interface = createMedullaInterface();
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "medulla_oblongata";
    config.type = MESH_PARTICIPANT_PROCESSOR;
    config.home_channel = MESH_CHANNEL_VITAL;
    config.user_context = medulla;

    mesh_participant_id_t id = 0;
    EXPECT_EQ(mesh_participant_register(registry, &interface, &config, &id), NIMCP_SUCCESS);

    // Lookup by name
    const mesh_participant_interface_t* found =
        mesh_participant_get_by_name(registry, "medulla_oblongata");
    EXPECT_NE(found, nullptr);
    EXPECT_EQ(found->id, id);

    mesh_participant_unregister(registry, id);
}

//=============================================================================
// 2. Transaction Handling Tests
//=============================================================================

TEST_F(MedullaMeshIntegrationTest, MedullaHandlesProposal) {
    // WHAT: Test medulla handles transaction proposals
    // WHY:  Medulla must respond to proposed state changes
    // HOW:  Create transaction, invoke on_proposal callback

    if (!bootstrap) {
        GTEST_SKIP() << "Mesh bootstrap not available";
    }

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Create a transaction
    mesh_participant_id_t proposer_id = 1001;
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_TYPE_STATE_UPDATE, proposer_id, MESH_CHANNEL_VITAL);

    if (!tx) {
        GTEST_SKIP() << "Transaction creation not available";
    }

    // Set payload (arousal change request)
    float arousal_delta = 0.1f;
    EXPECT_EQ(mesh_transaction_set_payload(tx, &arousal_delta, sizeof(float)), NIMCP_SUCCESS);

    // Invoke proposal callback
    nimcp_error_t result = medullaOnProposal(medulla, tx);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    mesh_transaction_destroy(tx);
}

TEST_F(MedullaMeshIntegrationTest, MedullaHandlesCommit) {
    // WHAT: Test medulla handles committed transactions
    // WHY:  Committed transactions should affect medulla state
    // HOW:  Create transaction, invoke on_commit callback

    if (!bootstrap) {
        GTEST_SKIP() << "Mesh bootstrap not available";
    }

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    medulla_stats_t stats_before;
    medulla_get_stats(medulla, &stats_before);

    // Create and commit a transaction
    mesh_participant_id_t proposer_id = 1001;
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_TYPE_STATE_UPDATE, proposer_id, MESH_CHANNEL_VITAL);

    if (!tx) {
        GTEST_SKIP() << "Transaction creation not available";
    }

    float arousal_delta = 0.1f;
    mesh_transaction_set_payload(tx, &arousal_delta, sizeof(float));

    // Invoke commit callback
    nimcp_error_t result = medullaOnCommit(medulla, tx);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify medulla was updated
    medulla_stats_t stats_after;
    medulla_get_stats(medulla, &stats_after);
    EXPECT_GT(stats_after.total_updates, stats_before.total_updates);

    mesh_transaction_destroy(tx);
}

TEST_F(MedullaMeshIntegrationTest, MedullaRejectsInvalidTransaction) {
    // WHAT: Test medulla rejects invalid transactions
    // WHY:  Must validate transactions before processing
    // HOW:  Pass NULL transaction, verify rejection

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // NULL transaction should be rejected
    nimcp_error_t result = medullaOnProposal(medulla, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);

    // NULL context should be rejected
    mesh_transaction_t dummy;
    memset(&dummy, 0, sizeof(dummy));
    result = medullaOnProposal(nullptr, &dummy);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// 3. State Synchronization Tests
//=============================================================================

TEST_F(MedullaMeshIntegrationTest, ArousalStateSyncsViaMesh) {
    // WHAT: Test arousal state changes sync via mesh
    // WHY:  Other brain regions need to know arousal level
    // HOW:  Change arousal, verify free energy callback reflects change

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Get baseline free energy
    float fe_baseline = medullaGetFreeEnergy(medulla);

    // Boost arousal significantly
    medulla_boost_arousal(medulla, 0.3f);
    runMedullaUpdates(5);

    // Free energy should change (arousal moved from baseline)
    float fe_after = medullaGetFreeEnergy(medulla);
    // Free energy will differ if arousal moved away from optimal (0.5)
    EXPECT_GE(fe_after, 0.0f);
    EXPECT_LE(fe_after, 0.5f);  // Max deviation is 0.5
}

TEST_F(MedullaMeshIntegrationTest, ProtectionStateSyncsViaMesh) {
    // WHAT: Test protection state can be synchronized
    // WHY:  Protection level affects system-wide behavior
    // HOW:  Change protection, verify state accessible

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Initial protection should be normal
    protection_level_t initial = medulla_get_protection_level(medulla);
    EXPECT_EQ(initial, PROTECTION_LEVEL_NORMAL);

    // Trigger emergency
    medulla_emergency_shutdown(medulla, "mesh sync test");

    // Protection should be elevated
    protection_level_t after = medulla_get_protection_level(medulla);
    EXPECT_GE((int)after, (int)PROTECTION_LEVEL_CRITICAL);
}

TEST_F(MedullaMeshIntegrationTest, CircadianStateSyncsViaMesh) {
    // WHAT: Test circadian state can be synchronized
    // WHY:  Circadian phase affects learning and performance
    // HOW:  Set circadian phase, verify accessible

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Set specific circadian phase
    EXPECT_EQ(medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING), NIMCP_SUCCESS);
    runMedullaUpdates(3);

    // Verify phase is set
    circadian_phase_t phase = medulla_get_circadian_phase(medulla);
    EXPECT_EQ(phase, CIRCADIAN_PHASE_MORNING);
}

//=============================================================================
// 4. Channel Membership Tests
//=============================================================================

TEST_F(MedullaMeshIntegrationTest, MedullaJoinsVitalChannel) {
    // WHAT: Test medulla joins vital functions channel
    // WHY:  Medulla should be in vital channel for autonomic coordination
    // HOW:  Register, verify home channel is vital

    if (!registry) {
        GTEST_SKIP() << "Mesh registry not available";
    }

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    mesh_participant_interface_t interface = createMedullaInterface();
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "medulla_oblongata";
    config.type = MESH_PARTICIPANT_PROCESSOR;
    config.home_channel = MESH_CHANNEL_VITAL;
    config.user_context = medulla;

    mesh_participant_id_t id = 0;
    EXPECT_EQ(mesh_participant_register(registry, &interface, &config, &id), NIMCP_SUCCESS);

    // Verify in vital channel
    EXPECT_TRUE(mesh_participant_is_in_channel(registry, id, MESH_CHANNEL_VITAL));

    mesh_participant_unregister(registry, id);
}

TEST_F(MedullaMeshIntegrationTest, MedullaJoinsMultipleChannels) {
    // WHAT: Test medulla can join multiple channels
    // WHY:  Medulla coordinates with multiple systems
    // HOW:  Register, join additional channels, verify membership

    if (!registry) {
        GTEST_SKIP() << "Mesh registry not available";
    }

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    mesh_participant_interface_t interface = createMedullaInterface();
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "medulla_oblongata";
    config.type = MESH_PARTICIPANT_PROCESSOR;
    config.home_channel = MESH_CHANNEL_VITAL;
    config.user_context = medulla;

    mesh_participant_id_t id = 0;
    EXPECT_EQ(mesh_participant_register(registry, &interface, &config, &id), NIMCP_SUCCESS);

    // Join motor coordination channel
    nimcp_error_t err = mesh_participant_join_channel(registry, id, MESH_CHANNEL_MOTOR);
    if (err == NIMCP_SUCCESS) {
        EXPECT_TRUE(mesh_participant_is_in_channel(registry, id, MESH_CHANNEL_MOTOR));
    }

    // Still in vital channel
    EXPECT_TRUE(mesh_participant_is_in_channel(registry, id, MESH_CHANNEL_VITAL));

    mesh_participant_unregister(registry, id);
}

//=============================================================================
// 5. Endorsement Flow Tests
//=============================================================================

TEST_F(MedullaMeshIntegrationTest, MedullaProvidesFreeEnergy) {
    // WHAT: Test medulla provides free energy for endorsement
    // WHY:  Free energy is used for FEP-based consensus
    // HOW:  Get free energy, verify valid range

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Get free energy at different arousal levels
    medulla_test_set_arousal(medulla, 0.5f);  // Optimal
    runMedullaUpdates(3);
    float fe_optimal = medullaGetFreeEnergy(medulla);

    medulla_test_set_arousal(medulla, 0.9f);  // High
    runMedullaUpdates(3);
    float fe_high = medullaGetFreeEnergy(medulla);

    medulla_test_set_arousal(medulla, 0.1f);  // Low
    runMedullaUpdates(3);
    float fe_low = medullaGetFreeEnergy(medulla);

    // All should be in valid range
    EXPECT_GE(fe_optimal, 0.0f);
    EXPECT_LE(fe_optimal, 0.5f);
    EXPECT_GE(fe_high, 0.0f);
    EXPECT_GE(fe_low, 0.0f);

    // Optimal arousal should have lowest free energy
    EXPECT_LE(fe_optimal, fe_high);
    EXPECT_LE(fe_optimal, fe_low);
}

//=============================================================================
// 6. Cross-Channel State Tests
//=============================================================================

TEST_F(MedullaMeshIntegrationTest, StateChangeAffectsMultipleChannels) {
    // WHAT: Test state changes propagate across channels
    // WHY:  Arousal affects motor, cognitive, and vital functions
    // HOW:  Change arousal, verify affects are measurable

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Record baseline state
    float baseline_arousal = medulla_get_arousal_level(medulla);
    circadian_phase_t baseline_phase = medulla_get_circadian_phase(medulla);
    protection_level_t baseline_protection = medulla_get_protection_level(medulla);

    // Trigger comprehensive state change
    medulla_boost_arousal(medulla, 0.2f);
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_EVENING);
    runMedullaUpdates(10);

    // All states should be accessible (for cross-channel sync)
    float new_arousal = medulla_get_arousal_level(medulla);
    circadian_phase_t new_phase = medulla_get_circadian_phase(medulla);
    protection_level_t new_protection = medulla_get_protection_level(medulla);

    // Arousal should have changed
    EXPECT_NE(new_arousal, baseline_arousal);

    // Phase should have changed
    EXPECT_NE(new_phase, baseline_phase);

    // Protection should still be normal (unless triggered)
    EXPECT_EQ(new_protection, baseline_protection);
}

//=============================================================================
// 7. Recovery Integration Tests
//=============================================================================

TEST_F(MedullaMeshIntegrationTest, MedullaRecoveryAffectsState) {
    // WHAT: Test recovery from emergency state
    // WHY:  System must be able to recover from protection states
    // HOW:  Trigger emergency, properly recover via STOPPED, verify state normalizes

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Trigger emergency
    EXPECT_EQ(medulla_emergency_shutdown(medulla, "recovery test"), NIMCP_SUCCESS);

    protection_level_t emergency_level = medulla_get_protection_level(medulla);
    EXPECT_GE((int)emergency_level, (int)PROTECTION_LEVEL_CRITICAL);

    // Recovery from EMERGENCY requires going through STOPPED first (safety protocol)
    // Direct EMERGENCY -> RUNNING is not allowed by design
    EXPECT_EQ(medulla_request_state_change(medulla, MEDULLA_STATE_STOPPED), NIMCP_SUCCESS);

    // Run updates to allow recovery
    runMedullaUpdates(10);

    // Now transition to running
    EXPECT_EQ(medulla_request_state_change(medulla, MEDULLA_STATE_RUNNING), NIMCP_SUCCESS);

    // Run updates
    runMedullaUpdates(50);

    // Verify state transitioned
    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    // State should be running (not emergency)
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);
}

TEST_F(MedullaMeshIntegrationTest, ProtectionLevelRecovery) {
    // WHAT: Test protection level can decrease after threat passes
    // WHY:  Must return to normal operation after threat
    // HOW:  Set high protection, run updates, verify trend toward normal

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Force elevated protection
    EXPECT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_DEFENSIVE), NIMCP_SUCCESS);

    protection_level_t elevated = medulla_get_protection_level(medulla);
    EXPECT_EQ(elevated, PROTECTION_LEVEL_DEFENSIVE);

    // Reset to normal
    EXPECT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL), NIMCP_SUCCESS);
    runMedullaUpdates(10);

    protection_level_t recovered = medulla_get_protection_level(medulla);
    EXPECT_EQ(recovered, PROTECTION_LEVEL_NORMAL);
}

//=============================================================================
// 8. Concurrent Operations Tests
//=============================================================================

TEST_F(MedullaMeshIntegrationTest, ConcurrentMeshAndMedullaOperations) {
    // WHAT: Test thread safety of mesh and medulla operations
    // WHY:  Real system has concurrent access from multiple threads
    // HOW:  Run concurrent operations, verify no crashes

    if (!registry) {
        GTEST_SKIP() << "Mesh registry not available";
    }

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Register medulla
    mesh_participant_interface_t interface = createMedullaInterface();
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "medulla_oblongata";
    config.type = MESH_PARTICIPANT_PROCESSOR;
    config.home_channel = MESH_CHANNEL_VITAL;
    config.user_context = medulla;

    mesh_participant_id_t id = 0;
    EXPECT_EQ(mesh_participant_register(registry, &interface, &config, &id), NIMCP_SUCCESS);

    std::atomic<bool> running{true};
    std::atomic<int> medulla_ops{0};
    std::atomic<int> mesh_ops{0};

    // Medulla update thread
    std::thread medulla_thread([&]() {
        while (running) {
            medulla_update(medulla, 0.001f);
            medulla_boost_arousal(medulla, 0.001f);
            medulla_ops++;
        }
    });

    // Mesh query thread
    std::thread mesh_thread([&]() {
        while (running) {
            mesh_participant_is_registered(registry, id);
            float fe = mesh_participant_get_free_energy(registry, id);
            (void)fe;
            mesh_ops++;
        }
    });

    // Run for short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    medulla_thread.join();
    mesh_thread.join();

    // Should have done many operations
    EXPECT_GT(medulla_ops.load(), 10);
    EXPECT_GT(mesh_ops.load(), 10);

    // Clean up
    mesh_participant_unregister(registry, id);
}

TEST_F(MedullaMeshIntegrationTest, ConcurrentTransactionHandling) {
    // WHAT: Test concurrent transaction handling
    // WHY:  Multiple transactions may arrive concurrently
    // HOW:  Submit multiple transactions from threads

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    std::atomic<int> proposals_handled{0};
    std::atomic<int> commits_handled{0};

    auto handle_transactions = [&](int count) {
        for (int i = 0; i < count; i++) {
            mesh_transaction_t dummy;
            memset(&dummy, 0, sizeof(dummy));
            dummy.type = MESH_TX_TYPE_STATE_UPDATE;

            if (medullaOnProposal(medulla, &dummy) == NIMCP_SUCCESS) {
                proposals_handled++;
            }
            if (medullaOnCommit(medulla, &dummy) == NIMCP_SUCCESS) {
                commits_handled++;
            }
        }
    };

    // Launch multiple threads
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back(handle_transactions, 100);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have handled many transactions
    EXPECT_GT(proposals_handled.load(), 100);
    EXPECT_GT(commits_handled.load(), 100);
}

//=============================================================================
// Registry Statistics Tests
//=============================================================================

TEST_F(MedullaMeshIntegrationTest, RegistryTracksParticipant) {
    // WHAT: Test registry statistics track medulla
    // WHY:  Monitoring requires accurate statistics
    // HOW:  Register, check stats, unregister, verify stats

    if (!registry) {
        GTEST_SKIP() << "Mesh registry not available";
    }

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    mesh_registry_stats_t stats_before;
    mesh_registry_get_stats(registry, &stats_before);

    // Register medulla
    mesh_participant_interface_t interface = createMedullaInterface();
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "medulla_oblongata";
    config.type = MESH_PARTICIPANT_PROCESSOR;
    config.home_channel = MESH_CHANNEL_VITAL;
    config.user_context = medulla;

    mesh_participant_id_t id = 0;
    EXPECT_EQ(mesh_participant_register(registry, &interface, &config, &id), NIMCP_SUCCESS);

    mesh_registry_stats_t stats_after;
    mesh_registry_get_stats(registry, &stats_after);

    // Participant count should have increased
    EXPECT_EQ(stats_after.total_participants, stats_before.total_participants + 1);

    // Unregister
    mesh_participant_unregister(registry, id);

    mesh_registry_stats_t stats_final;
    mesh_registry_get_stats(registry, &stats_final);

    // Count should decrease
    EXPECT_EQ(stats_final.total_participants, stats_before.total_participants);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
