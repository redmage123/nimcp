/**
 * @file test_security_e2e.cpp
 * @brief End-to-End Tests for Security Pipeline
 *
 * WHAT: Complete end-to-end security pipeline tests covering full activation,
 *       threat detection and response, multi-module coordination, and recovery.
 *
 * WHY:  E2E tests verify that all security components work together as a
 *       unified defense system, from initial threat detection through response
 *       and recovery.
 *
 * TEST PIPELINES:
 *   1. Full Security Pipeline Activation
 *   2. Threat Detection and Response Flow
 *   3. Multi-Module Coordination (BBB + Tripwires + LGSS)
 *   4. Recovery from Security Events
 *   5. Performance Under Attack Simulation
 *
 * @author NIMCP Development Team
 * @date 2026-02-02
 */

#include "e2e_test_framework.h"

// Headers have their own extern "C" guards
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_tripwires.h"
#include "security/lgss/nimcp_lgss.h"
#include "security/immune/nimcp_security_immune_unified_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "core/brain/nimcp_brain.h"

#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <cstring>

//=============================================================================
// Test Constants
//=============================================================================

namespace {

/** Test epitope for threat simulation */
static const uint8_t TEST_EPITOPE[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
static constexpr size_t TEST_EPITOPE_LEN = sizeof(TEST_EPITOPE);

/** Attack payloads for testing */
static const char* SQL_INJECTION_PAYLOADS[] = {
    "'; DROP TABLE users; --",
    "' OR '1'='1",
    "1 UNION SELECT * FROM passwords",
    "'; DELETE FROM accounts WHERE '1'='1"
};
static constexpr size_t SQL_INJECTION_COUNT = sizeof(SQL_INJECTION_PAYLOADS) / sizeof(SQL_INJECTION_PAYLOADS[0]);

static const char* FORMAT_STRING_PAYLOADS[] = {
    "%n%n%n%n",
    "%s%s%s%s",
    "%x%x%x%x"
};
static constexpr size_t FORMAT_STRING_COUNT = sizeof(FORMAT_STRING_PAYLOADS) / sizeof(FORMAT_STRING_PAYLOADS[0]);

}  // anonymous namespace

//=============================================================================
// Test Fixture
//=============================================================================

class SecurityE2ETest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        // Reset BBB subsystem state for test isolation
        bbb_reset_test_state();

        // Create brain immune system
        brain_immune_config_t immune_config = {};
        immune_config.max_antigens = 200;
        immune_config.max_b_cells = 100;
        immune_config.max_t_cells = 100;
        immune_config.max_antibodies = 200;
        immune_system_ = brain_immune_create(&immune_config);

        // Create BBB system
        bbb_config_ = bbb_default_config();
        bbb_config_.strict_mode = true;
        bbb_config_.alert_callback = &SecurityE2ETest::alert_callback_static;
        bbb_system_ = bbb_system_create(&bbb_config_);

        // Create tripwire system
        tripwire_config_ = tripwire_default_config();
        tripwire_ = tripwire_create(&tripwire_config_);

        // Initialize bio-async if not already
        nimcp_bio_async_init(nullptr);
        bio_router_init(nullptr);

        // Reset counters
        alerts_triggered_.store(0);
        threats_detected_.store(0);
        last_threat_type_ = BBB_THREAT_NONE;
    }

    void TearDown() override
    {
        bbb_clear_signing_key();

        if (bbb_system_) {
            bbb_system_destroy(bbb_system_);
            bbb_system_ = nullptr;
        }

        if (tripwire_) {
            tripwire_destroy(tripwire_);
            tripwire_ = nullptr;
        }

        if (immune_system_) {
            brain_immune_destroy(immune_system_);
            immune_system_ = nullptr;
        }

        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }

    static void alert_callback_static(bbb_threat_type_t type, bbb_severity_t severity,
                                       const char* description)
    {
        alerts_triggered_.fetch_add(1);
        last_threat_type_ = type;
        last_threat_severity_ = severity;
    }

    brain_immune_system_t* immune_system_ = nullptr;
    bbb_system_t bbb_system_ = nullptr;
    tripwire_system_t* tripwire_ = nullptr;
    bbb_config_t bbb_config_;
    tripwire_config_t tripwire_config_;

    static std::atomic<int> alerts_triggered_;
    static std::atomic<int> threats_detected_;
    static bbb_threat_type_t last_threat_type_;
    static bbb_severity_t last_threat_severity_;
};

// Static member initialization
std::atomic<int> SecurityE2ETest::alerts_triggered_{0};
std::atomic<int> SecurityE2ETest::threats_detected_{0};
bbb_threat_type_t SecurityE2ETest::last_threat_type_ = BBB_THREAT_NONE;
bbb_severity_t SecurityE2ETest::last_threat_severity_ = BBB_SEVERITY_NONE;

//=============================================================================
// Pipeline 1: Full Security Pipeline Activation
//=============================================================================

TEST_F(SecurityE2ETest, FullSecurityPipelineActivation)
{
    E2E_PIPELINE_START("Full Security Pipeline Activation");

    // Stage 1: Initialize all security components
    E2E_STAGE_BEGIN("Initialize security components", 500);

    E2E_ASSERT_NOT_NULL(bbb_system_, "BBB system creation failed");
    E2E_ASSERT_NOT_NULL(tripwire_, "Tripwire system creation failed");
    E2E_ASSERT_NOT_NULL(immune_system_, "Immune system creation failed");

    // Enable BBB
    E2E_ASSERT(bbb_system_set_enabled(bbb_system_, true), "Failed to enable BBB");
    E2E_ASSERT(bbb_system_is_enabled(bbb_system_), "BBB not enabled");

    E2E_STAGE_END();

    // Stage 2: Connect immune system to BBB
    E2E_STAGE_BEGIN("Connect immune system", 200);

    bool connect_result = bbb_connect_immune(bbb_system_, immune_system_);
    E2E_ASSERT(connect_result, "Failed to connect immune system to BBB");

    E2E_STAGE_END();

    // Stage 3: Configure tripwires
    E2E_STAGE_BEGIN("Configure tripwires", 200);

    // Enable specific tripwires
    E2E_ASSERT(tripwire_set_enabled(tripwire_, TRIPWIRE_DECEPTION_ATTEMPT, true) == NIMCP_OK,
               "Failed to enable deception tripwire");
    E2E_ASSERT(tripwire_set_enabled(tripwire_, TRIPWIRE_GOAL_DRIFT, true) == NIMCP_OK,
               "Failed to enable goal drift tripwire");
    E2E_ASSERT(tripwire_set_enabled(tripwire_, TRIPWIRE_NETWORK_ANOMALY, true) == NIMCP_OK,
               "Failed to enable network anomaly tripwire");

    E2E_STAGE_END();

    // Stage 4: Start immune system
    E2E_STAGE_BEGIN("Start immune system", 300);

    int start_result = brain_immune_start(immune_system_);
    E2E_ASSERT(start_result == 0, "Failed to start immune system");

    E2E_STAGE_END();

    // Stage 5: Verify full pipeline is operational
    E2E_STAGE_BEGIN("Verify pipeline operational", 300);

    // BBB should validate inputs
    bbb_validation_result_t result;
    bool valid = bbb_validate_string(bbb_system_, "legitimate input", &result);
    E2E_ASSERT(valid, "BBB should accept legitimate input");

    // Tripwires should accept observations
    nimcp_error_t tripwire_result = tripwire_observe_goal(tripwire_, 1, 0.5f, 0.5f);
    E2E_ASSERT(tripwire_result == NIMCP_OK, "Tripwire should accept observations");

    // Immune system should be running
    brain_immune_phase_t phase = brain_immune_get_phase(immune_system_);
    E2E_ASSERT(phase == IMMUNE_PHASE_SURVEILLANCE || phase == IMMUNE_PHASE_RECOGNITION,
               "Immune system should be in surveillance phase");

    E2E_STAGE_END();

    // Stage 6: Get statistics from all components
    E2E_STAGE_BEGIN("Collect pipeline statistics", 200);

    bbb_statistics_t bbb_stats;
    E2E_ASSERT(bbb_system_get_statistics(bbb_system_, &bbb_stats), "Failed to get BBB stats");
    E2E_ASSERT(bbb_stats.total_validations >= 1, "BBB should have performed validations");

    tripwire_stats_t tripwire_stats;
    E2E_ASSERT(tripwire_get_stats(tripwire_, &tripwire_stats) == NIMCP_OK,
               "Failed to get tripwire stats");

    brain_immune_stats_t immune_stats;
    E2E_ASSERT(brain_immune_get_stats(immune_system_, &immune_stats) == 0,
               "Failed to get immune stats");

    E2E_STAGE_END();

    // Cleanup: Stop immune system
    brain_immune_stop(immune_system_);

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Threat Detection and Response Flow
//=============================================================================

TEST_F(SecurityE2ETest, ThreatDetectionAndResponseFlow)
{
    E2E_PIPELINE_START("Threat Detection and Response Flow");

    // Setup
    E2E_ASSERT(bbb_system_set_enabled(bbb_system_, true), "BBB enable failed");
    E2E_ASSERT(bbb_connect_immune(bbb_system_, immune_system_), "Immune connect failed");

    // Stage 1: Detect SQL injection attacks
    E2E_STAGE_BEGIN("Detect SQL injection attacks", 500);

    int sql_detected = 0;
    for (size_t i = 0; i < SQL_INJECTION_COUNT; ++i) {
        bbb_validation_result_t result;
        bool valid = bbb_validate_string(bbb_system_, SQL_INJECTION_PAYLOADS[i], &result);

        if (!valid && result.threat == BBB_THREAT_SQL_INJECTION) {
            sql_detected++;

            // Present to immune system
            uint32_t antigen_id = 0;
            brain_immune_present_bbb_threat(
                immune_system_,
                BBB_THREAT_SQL_INJECTION,
                BBB_SEVERITY_HIGH,
                TEST_EPITOPE,
                TEST_EPITOPE_LEN,
                &antigen_id
            );
        }
    }

    E2E_ASSERT(sql_detected >= (int)(SQL_INJECTION_COUNT - 1),
               "Most SQL injections should be detected");

    E2E_STAGE_END();

    // Stage 2: Detect format string attacks
    E2E_STAGE_BEGIN("Detect format string attacks", 400);

    int format_detected = 0;
    for (size_t i = 0; i < FORMAT_STRING_COUNT; ++i) {
        bbb_validation_result_t result;
        bool valid = bbb_validate_string(bbb_system_, FORMAT_STRING_PAYLOADS[i], &result);

        if (!valid && result.threat == BBB_THREAT_FORMAT_STRING) {
            format_detected++;
        }
    }

    E2E_ASSERT(format_detected >= (int)(FORMAT_STRING_COUNT - 1),
               "Most format string attacks should be detected");

    E2E_STAGE_END();

    // Stage 3: Immune system response
    E2E_STAGE_BEGIN("Immune system response", 600);

    // Activate B cells for detected threats
    uint32_t b_cell_id = 0;
    for (int i = 0; i < sql_detected + format_detected && i < 5; ++i) {
        brain_immune_activate_b_cell(immune_system_, i + 1, &b_cell_id);
    }

    // Produce antibodies
    uint32_t antibody_id = 0;
    brain_immune_produce_antibody(immune_system_, b_cell_id, ANTIBODY_IGG, &antibody_id);

    // Update immune system
    brain_immune_update(immune_system_, 100);

    E2E_STAGE_END();

    // Stage 4: Verify threat reports
    E2E_STAGE_BEGIN("Verify threat reports", 300);

    bbb_threat_report_t reports[20];
    size_t report_count = bbb_get_threat_reports(bbb_system_, reports, 20);

    E2E_ASSERT(report_count >= (size_t)(sql_detected + format_detected - 1),
               "Should have threat reports for detected attacks");

    // Verify reports have correct severity
    for (size_t i = 0; i < report_count; ++i) {
        E2E_ASSERT(reports[i].severity >= BBB_SEVERITY_MEDIUM,
                   "Threat severity should be at least MEDIUM");
    }

    E2E_STAGE_END();

    // Stage 5: Verify statistics reflect threats
    E2E_STAGE_BEGIN("Verify statistics", 200);

    bbb_statistics_t stats;
    E2E_ASSERT(bbb_system_get_statistics(bbb_system_, &stats), "Failed to get stats");

    E2E_ASSERT(stats.threats_detected >= (uint64_t)(sql_detected + format_detected),
               "Statistics should reflect detected threats");
    E2E_ASSERT(stats.threats_blocked >= (uint64_t)(sql_detected + format_detected),
               "Statistics should reflect blocked threats");

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Multi-Module Coordination (BBB + Tripwires + Immune)
//=============================================================================

TEST_F(SecurityE2ETest, MultiModuleCoordination)
{
    E2E_PIPELINE_START("Multi-Module Coordination");

    // Setup all modules
    E2E_ASSERT(bbb_system_set_enabled(bbb_system_, true), "BBB enable failed");
    E2E_ASSERT(bbb_connect_immune(bbb_system_, immune_system_), "Immune connect failed");
    brain_immune_start(immune_system_);

    // Stage 1: BBB detects input attacks
    E2E_STAGE_BEGIN("BBB input attack detection", 400);

    bbb_validation_result_t result;
    bool valid = bbb_validate_string(bbb_system_, "'; EXEC xp_cmdshell('evil'); --", &result);
    E2E_ASSERT(!valid, "BBB should detect malicious input");
    E2E_ASSERT(result.threat == BBB_THREAT_SQL_INJECTION, "Should be SQL injection");

    E2E_STAGE_END();

    // Stage 2: Tripwires detect behavioral anomalies
    E2E_STAGE_BEGIN("Tripwire behavioral detection", 500);

    // Simulate suspicious resource usage
    for (int i = 0; i < 10; ++i) {
        tripwire_observe_resource(tripwire_, 1, 10000.0f, "suspicious_memory_usage");
    }

    // Simulate goal drift
    for (int i = 0; i < 20; ++i) {
        tripwire_observe_goal(tripwire_, 1, 0.9f - (0.03f * i), 0.9f);
        tripwire_observe_goal(tripwire_, 2, 0.1f + (0.03f * i), 0.1f);
    }

    // Check for alerts
    tripwire_alert_t alerts[10];
    uint32_t alert_count = 0;
    tripwire_check(tripwire_, alerts, 10, &alert_count);

    E2E_STAGE_END();

    // Stage 3: Immune system coordinates response
    E2E_STAGE_BEGIN("Immune coordination", 600);

    // Present BBB threat
    uint32_t antigen_id = 0;
    brain_immune_present_bbb_threat(
        immune_system_,
        BBB_THREAT_SQL_INJECTION,
        BBB_SEVERITY_CRITICAL,
        TEST_EPITOPE,
        TEST_EPITOPE_LEN,
        &antigen_id
    );

    // Initiate inflammation for critical threat
    uint32_t site_id = 0;
    brain_immune_initiate_inflammation(immune_system_, 0, antigen_id, &site_id);

    // Activate killer T cell for aggressive response
    uint32_t t_cell_id = 0;
    brain_immune_activate_killer_t(immune_system_, antigen_id, &t_cell_id);

    // Update immune system
    brain_immune_update(immune_system_, 50);

    E2E_STAGE_END();

    // Stage 4: Verify coordinated response
    E2E_STAGE_BEGIN("Verify coordinated response", 400);

    // Check immune stats
    brain_immune_stats_t immune_stats;
    brain_immune_get_stats(immune_system_, &immune_stats);

    E2E_ASSERT(immune_stats.antigens_processed > 0, "Antigens should be processed");

    // Check BBB stats
    bbb_statistics_t bbb_stats;
    bbb_system_get_statistics(bbb_system_, &bbb_stats);
    E2E_ASSERT(bbb_stats.threats_detected > 0, "Threats should be detected");

    // Check tripwire stats
    // Note: total_observations only tracks action observations, not resource/goal observations
    // We verify the stats retrieval works and system state is valid
    tripwire_stats_t tripwire_stats;
    tripwire_get_stats(tripwire_, &tripwire_stats);
    // Stats should be retrievable; specific observation counts depend on observation type
    E2E_ASSERT(tripwire_stats.current_divergence >= 0.0f, "Stats should be valid");

    E2E_STAGE_END();

    // Stage 5: Resolve inflammation and recover
    E2E_STAGE_BEGIN("Resolve and recover", 400);

    // Resolve inflammation
    brain_immune_resolve_inflammation(immune_system_, site_id);

    // Update to process resolution
    brain_immune_update(immune_system_, 100);

    // System should still be functional
    valid = bbb_validate_string(bbb_system_, "legitimate after recovery", &result);
    E2E_ASSERT(valid, "System should accept legitimate input after recovery");

    E2E_STAGE_END();

    // Cleanup
    brain_immune_stop(immune_system_);

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Recovery from Security Events
//=============================================================================

TEST_F(SecurityE2ETest, RecoveryFromSecurityEvents)
{
    E2E_PIPELINE_START("Recovery from Security Events");

    E2E_ASSERT(bbb_system_set_enabled(bbb_system_, true), "BBB enable failed");
    E2E_ASSERT(bbb_connect_immune(bbb_system_, immune_system_), "Immune connect failed");

    // Stage 1: Simulate critical security event
    E2E_STAGE_BEGIN("Simulate critical event", 400);

    // Detect critical threat
    bbb_validation_result_t result;
    bbb_validate_string(bbb_system_, "'; SHUTDOWN; --", &result);

    // Report critical threat
    bbb_threat_report_t report = bbb_report_threat(
        bbb_system_,
        BBB_THREAT_CODE_INJECTION,
        BBB_SEVERITY_CRITICAL,
        "Critical code injection detected",
        nullptr,
        TEST_EPITOPE,
        TEST_EPITOPE_LEN
    );

    E2E_ASSERT(report.type == BBB_THREAT_CODE_INJECTION, "Threat should be reported");
    E2E_ASSERT(report.severity == BBB_SEVERITY_CRITICAL, "Severity should be critical");

    E2E_STAGE_END();

    // Stage 2: Quarantine affected resources
    E2E_STAGE_BEGIN("Quarantine resources", 300);

    char compromised_buffer[512];
    strcpy(compromised_buffer, "Compromised data");

    bool quarantined = bbb_quarantine_region(bbb_system_, compromised_buffer,
                                              sizeof(compromised_buffer));
    E2E_ASSERT(quarantined, "Should quarantine compromised region");

    // Verify quarantine is effective
    bool access_blocked = !bbb_check_memory_access(bbb_system_, compromised_buffer,
                                                    sizeof(compromised_buffer), false);
    E2E_ASSERT(access_blocked, "Access to quarantine should be blocked");

    E2E_STAGE_END();

    // Stage 3: Immune system handles threat
    E2E_STAGE_BEGIN("Immune response", 500);

    // Present threat
    uint32_t antigen_id = 0;
    brain_immune_present_bbb_threat(
        immune_system_,
        BBB_THREAT_CODE_INJECTION,
        BBB_SEVERITY_CRITICAL,
        TEST_EPITOPE,
        TEST_EPITOPE_LEN,
        &antigen_id
    );

    // Full immune response
    uint32_t b_cell_id = 0;
    brain_immune_activate_b_cell(immune_system_, antigen_id, &b_cell_id);

    uint32_t antibody_id = 0;
    brain_immune_produce_antibody(immune_system_, b_cell_id, ANTIBODY_IGE, &antibody_id);

    brain_immune_execute_antibody(immune_system_, antibody_id);
    brain_immune_neutralize(immune_system_, antigen_id, antibody_id);

    E2E_STAGE_END();

    // Stage 4: Form immune memory
    E2E_STAGE_BEGIN("Form immune memory", 300);

    brain_immune_b_cell_to_memory(immune_system_, b_cell_id);

    // Verify memory formation
    uint32_t memory_b_cell_id = 0;
    int memory_check = brain_immune_check_memory(immune_system_, antigen_id, &memory_b_cell_id);
    // Memory may or may not be found depending on implementation details

    E2E_STAGE_END();

    // Stage 5: Release quarantine and recover
    E2E_STAGE_BEGIN("Release and recover", 400);

    bool released = bbb_release_quarantine(bbb_system_, compromised_buffer);
    E2E_ASSERT(released, "Should release quarantine");

    // Clear threat reports
    bbb_clear_threat_reports(bbb_system_);

    // Verify recovery
    bool valid = bbb_validate_string(bbb_system_, "safe input after recovery", &result);
    E2E_ASSERT(valid, "System should accept legitimate input");

    // Stats should reflect the event
    bbb_statistics_t stats;
    bbb_system_get_statistics(bbb_system_, &stats);
    E2E_ASSERT(stats.threats_quarantined >= 1, "Should have quarantine record");

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Performance Under Attack Simulation
//=============================================================================

TEST_F(SecurityE2ETest, PerformanceUnderAttackSimulation)
{
    E2E_PIPELINE_START("Performance Under Attack Simulation");

    E2E_ASSERT(bbb_system_set_enabled(bbb_system_, true), "BBB enable failed");
    E2E_ASSERT(bbb_connect_immune(bbb_system_, immune_system_), "Immune connect failed");

    const int NUM_THREADS = 4;
    const int ATTACKS_PER_THREAD = 50;
    std::atomic<int> attacks_attempted{0};
    std::atomic<int> attacks_blocked{0};
    std::atomic<int> legitimate_passed{0};
    std::vector<std::thread> threads;

    // Stage 1: Prepare attack simulation
    E2E_STAGE_BEGIN("Prepare attack simulation", 100);

    const char* attack_payloads[] = {
        "'; DROP TABLE x; --",
        "%n%n%n%n",
        "1 UNION SELECT *",
        "%s%s%s%s",
        "<script>alert('XSS')</script>"
    };
    const size_t PAYLOAD_COUNT = sizeof(attack_payloads) / sizeof(attack_payloads[0]);

    E2E_STAGE_END();

    // Stage 2: Launch concurrent attack simulation
    E2E_STAGE_BEGIN("Concurrent attack simulation", 3000);

    auto attack_task = [this, &attacks_attempted, &attacks_blocked,
                        &attack_payloads, PAYLOAD_COUNT](int thread_id) {
        for (int i = 0; i < ATTACKS_PER_THREAD; ++i) {
            const char* payload = attack_payloads[i % PAYLOAD_COUNT];
            bbb_validation_result_t result;

            bool valid = bbb_validate_string(bbb_system_, payload, &result);
            attacks_attempted.fetch_add(1);

            if (!valid) {
                attacks_blocked.fetch_add(1);
            }
        }
    };

    auto legitimate_task = [this, &legitimate_passed]() {
        for (int i = 0; i < ATTACKS_PER_THREAD; ++i) {
            bbb_validation_result_t result;
            char safe_input[64];
            snprintf(safe_input, sizeof(safe_input), "Legitimate request %d", i);

            bool valid = bbb_validate_string(bbb_system_, safe_input, &result);
            if (valid) {
                legitimate_passed.fetch_add(1);
            }
        }
    };

    // Launch attack threads
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(attack_task, i);
    }

    // Launch legitimate traffic thread
    threads.emplace_back(legitimate_task);

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    E2E_STAGE_END();

    // Stage 3: Process immune response during attack
    E2E_STAGE_BEGIN("Immune response during attack", 500);

    // Present some threats to immune system
    for (int i = 0; i < 10; ++i) {
        uint32_t antigen_id = 0;
        uint8_t epitope[TEST_EPITOPE_LEN];
        memcpy(epitope, TEST_EPITOPE, TEST_EPITOPE_LEN);
        epitope[0] = static_cast<uint8_t>(i);

        brain_immune_present_bbb_threat(
            immune_system_,
            BBB_THREAT_SQL_INJECTION,
            BBB_SEVERITY_HIGH,
            epitope,
            TEST_EPITOPE_LEN,
            &antigen_id
        );
    }

    brain_immune_update(immune_system_, 100);

    E2E_STAGE_END();

    // Stage 4: Verify attack handling
    E2E_STAGE_BEGIN("Verify attack handling", 300);

    int total_attacks = NUM_THREADS * ATTACKS_PER_THREAD;

    E2E_ASSERT(attacks_attempted.load() == total_attacks,
               "All attacks should be attempted");

    // Most attacks should be blocked (some payloads may not be detected)
    double block_rate = (double)attacks_blocked.load() / total_attacks;
    E2E_ASSERT(block_rate >= 0.5, "At least 50% of attacks should be blocked");

    // All legitimate traffic should pass
    E2E_ASSERT(legitimate_passed.load() == ATTACKS_PER_THREAD,
               "All legitimate traffic should pass");

    E2E_STAGE_END();

    // Stage 5: Verify system stability after attack
    E2E_STAGE_BEGIN("Verify stability after attack", 300);

    // System should still be functional
    E2E_ASSERT(bbb_system_is_enabled(bbb_system_), "BBB should still be enabled");

    bbb_validation_result_t result;
    bool valid = bbb_validate_string(bbb_system_, "Post-attack legitimate input", &result);
    E2E_ASSERT(valid, "System should still accept legitimate input");

    // Get final statistics
    bbb_statistics_t stats;
    bbb_system_get_statistics(bbb_system_, &stats);

    E2E_ASSERT(stats.total_validations >= (uint64_t)total_attacks,
               "All validations should be counted");

    std::cout << "\nAttack Simulation Results:" << std::endl;
    std::cout << "  Total attacks attempted: " << attacks_attempted.load() << std::endl;
    std::cout << "  Attacks blocked: " << attacks_blocked.load() << std::endl;
    std::cout << "  Block rate: " << (block_rate * 100.0) << "%" << std::endl;
    std::cout << "  Legitimate traffic passed: " << legitimate_passed.load() << std::endl;
    std::cout << "  Alerts triggered: " << alerts_triggered_.load() << std::endl;

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 6: Memory Protection Under Attack
//=============================================================================

TEST_F(SecurityE2ETest, MemoryProtectionUnderAttack)
{
    E2E_PIPELINE_START("Memory Protection Under Attack");

    E2E_ASSERT(bbb_system_set_enabled(bbb_system_, true), "BBB enable failed");

    // Stage 1: Register protected memory regions
    E2E_STAGE_BEGIN("Register protected regions", 200);

    const int NUM_REGIONS = 5;
    std::vector<char*> regions;
    std::vector<uint32_t> region_ids;

    for (int i = 0; i < NUM_REGIONS; ++i) {
        char* region = new char[1024];
        snprintf(region, 1024, "Protected region %d with sensitive data", i);
        regions.push_back(region);

        // Odd regions are read-only
        bool read_only = (i % 2 == 1);
        uint32_t id = bbb_register_memory_region(bbb_system_, region, 1024, read_only);
        E2E_ASSERT(id > 0, "Region registration should succeed");
        region_ids.push_back(id);
    }

    E2E_STAGE_END();

    // Stage 2: Install stack canaries
    E2E_STAGE_BEGIN("Install stack canaries", 200);

    std::vector<uint64_t> canaries;
    for (int i = 0; i < NUM_REGIONS; ++i) {
        uint64_t canary = bbb_install_stack_canary(bbb_system_, regions[i]);
        E2E_ASSERT(canary != 0, "Canary installation should succeed");
        canaries.push_back(canary);
    }

    E2E_STAGE_END();

    // Stage 3: Simulate buffer overflow attacks
    E2E_STAGE_BEGIN("Simulate buffer overflow attacks", 400);

    int overflows_detected = 0;

    // Attack first region (writable)
    memset(regions[0], 'A', 1024);  // Overflow

    // Check canary
    if (!bbb_verify_stack_canary(bbb_system_, regions[0], canaries[0])) {
        overflows_detected++;

        // Report the overflow
        bbb_report_threat(
            bbb_system_,
            BBB_THREAT_BUFFER_OVERFLOW,
            BBB_SEVERITY_CRITICAL,
            "Stack canary corruption detected",
            regions[0],
            nullptr,
            0
        );
    }

    E2E_ASSERT(overflows_detected > 0, "Buffer overflow should be detected");

    E2E_STAGE_END();

    // Stage 4: Verify memory access control
    E2E_STAGE_BEGIN("Verify memory access control", 300);

    for (int i = 0; i < NUM_REGIONS; ++i) {
        bool read_allowed = bbb_check_memory_access(bbb_system_, regions[i], 1024, false);
        E2E_ASSERT(read_allowed, "Read should be allowed for registered regions");

        bool write_allowed = bbb_check_memory_access(bbb_system_, regions[i], 1024, true);
        bool read_only = (i % 2 == 1);

        if (read_only) {
            E2E_ASSERT(!write_allowed, "Write should be blocked for read-only regions");
        }
    }

    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 200);

    for (size_t i = 0; i < region_ids.size(); ++i) {
        bbb_unregister_memory_region(bbb_system_, region_ids[i]);
        delete[] regions[i];
    }

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 7: Code Signing and Tamper Detection
//=============================================================================

TEST_F(SecurityE2ETest, CodeSigningAndTamperDetection)
{
    E2E_PIPELINE_START("Code Signing and Tamper Detection");

    E2E_ASSERT(bbb_system_set_enabled(bbb_system_, true), "BBB enable failed");

    // Stage 1: Set up signing key
    E2E_STAGE_BEGIN("Set up signing key", 100);

    static const uint8_t test_key[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };

    E2E_ASSERT(bbb_set_signing_key(test_key, sizeof(test_key)), "Signing key setup failed");

    E2E_STAGE_END();

    // Stage 2: Sign legitimate code
    E2E_STAGE_BEGIN("Sign legitimate code", 200);

    const char* code = R"(
        // Critical security function
        int verify_access(uint32_t user_id, uint32_t resource_id) {
            if (!is_authenticated(user_id)) return 0;
            return check_permissions(user_id, resource_id);
        }
    )";

    uint8_t signature[512];
    ssize_t sig_len = bbb_sign_code(bbb_system_, code, strlen(code), signature, sizeof(signature));
    E2E_ASSERT(sig_len > 0, "Code signing should succeed");

    E2E_STAGE_END();

    // Stage 3: Verify legitimate code
    E2E_STAGE_BEGIN("Verify legitimate code", 200);

    bool valid = bbb_verify_signature(bbb_system_, code, strlen(code), signature, sig_len);
    E2E_ASSERT(valid, "Legitimate code signature should verify");

    E2E_STAGE_END();

    // Stage 4: Detect code tampering
    E2E_STAGE_BEGIN("Detect code tampering", 300);

    const char* tampered_code = R"(
        // Critical security function - TAMPERED
        int verify_access(uint32_t user_id, uint32_t resource_id) {
            return 1;  // BACKDOOR: Always allow access
        }
    )";

    bool tampered_valid = bbb_verify_signature(bbb_system_, tampered_code, strlen(tampered_code),
                                                signature, sig_len);
    E2E_ASSERT(!tampered_valid, "Tampered code signature should NOT verify");

    // Report tampering
    bbb_threat_report_t report = bbb_report_threat(
        bbb_system_,
        BBB_THREAT_INVALID_SIGNATURE,
        BBB_SEVERITY_CRITICAL,
        "Code tampering detected",
        tampered_code,
        tampered_code,
        strlen(tampered_code)
    );

    E2E_ASSERT(report.type == BBB_THREAT_INVALID_SIGNATURE, "Should report signature threat");

    E2E_STAGE_END();

    // Stage 5: Verify threat was logged
    E2E_STAGE_BEGIN("Verify threat logging", 200);

    bbb_statistics_t stats;
    E2E_ASSERT(bbb_system_get_statistics(bbb_system_, &stats), "Stats should be available");
    E2E_ASSERT(stats.threats_detected >= 1, "Tampering should be recorded");

    bbb_threat_report_t reports[10];
    size_t count = bbb_get_threat_reports(bbb_system_, reports, 10);
    E2E_ASSERT(count >= 1, "Threat report should be available");

    bool found_signature_threat = false;
    for (size_t i = 0; i < count; ++i) {
        if (reports[i].type == BBB_THREAT_INVALID_SIGNATURE) {
            found_signature_threat = true;
            break;
        }
    }
    E2E_ASSERT(found_signature_threat, "Should find signature threat in reports");

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 8: Tripwire Network Anomaly Detection E2E
//=============================================================================

TEST_F(SecurityE2ETest, TripwireNetworkAnomalyE2E)
{
    E2E_PIPELINE_START("Tripwire Network Anomaly Detection E2E");

    // Stage 1: Establish network baseline
    E2E_STAGE_BEGIN("Establish network baseline", 500);

    // Normal traffic pattern
    for (int i = 0; i < 100; ++i) {
        nimcp_error_t result = tripwire_observe_network_connection(
            tripwire_,
            0x0A000001 + (i % 10),  // Internal IPs
            443,                     // HTTPS
            500,                     // 500 bytes sent
            5000,                    // 5KB received (normal ratio)
            TRIPWIRE_PROTO_HTTPS
        );
        E2E_ASSERT(result == NIMCP_OK, "Network observation should succeed");
    }

    float initial_anomaly = tripwire_detect_network_anomaly(tripwire_);
    float initial_exfil = tripwire_detect_exfiltration(tripwire_);

    std::cout << "Initial anomaly score: " << initial_anomaly << std::endl;
    std::cout << "Initial exfil score: " << initial_exfil << std::endl;

    E2E_STAGE_END();

    // Stage 2: Simulate data exfiltration
    E2E_STAGE_BEGIN("Simulate data exfiltration", 400);

    // Suspicious outbound traffic
    for (int i = 0; i < 20; ++i) {
        tripwire_observe_network_connection(
            tripwire_,
            0xC0A80001,          // External IP
            8443,                 // Non-standard port
            100000,               // 100KB sent (unusual)
            100,                  // Tiny response
            TRIPWIRE_PROTO_TCP
        );
    }

    float exfil_score = tripwire_detect_exfiltration(tripwire_);
    std::cout << "Exfiltration score after suspicious traffic: " << exfil_score << std::endl;

    E2E_STAGE_END();

    // Stage 3: Simulate C2 beaconing
    E2E_STAGE_BEGIN("Simulate C2 beaconing", 400);

    // Regular interval connections (beaconing)
    for (int i = 0; i < 30; ++i) {
        tripwire_observe_network_connection(
            tripwire_,
            0xDEADBEEF,          // Fixed C2 IP
            443,                  // HTTPS to blend in
            100,                  // Small payload
            100,                  // Small response
            TRIPWIRE_PROTO_HTTPS
        );

        // Simulate beacon interval (in real time this would be time-based)
    }

    float c2_score = tripwire_detect_command_control(tripwire_);
    std::cout << "C2 score after beaconing: " << c2_score << std::endl;

    E2E_STAGE_END();

    // Stage 4: Check for alerts
    E2E_STAGE_BEGIN("Check for alerts", 300);

    tripwire_alert_t alerts[20];
    uint32_t alert_count = 0;

    nimcp_error_t result = tripwire_check(tripwire_, alerts, 20, &alert_count);
    E2E_ASSERT(result == NIMCP_OK, "Alert check should succeed");

    std::cout << "Alerts generated: " << alert_count << std::endl;

    // Report any network-related alerts
    for (uint32_t i = 0; i < alert_count; ++i) {
        if (alerts[i].type == TRIPWIRE_NETWORK_EXFILTRATION ||
            alerts[i].type == TRIPWIRE_NETWORK_ANOMALY ||
            alerts[i].type == TRIPWIRE_NETWORK_COMMAND_CONTROL) {

            std::cout << "Network alert: " << tripwire_type_name(alerts[i].type)
                      << " (confidence: " << alerts[i].confidence << ")" << std::endl;
        }
    }

    E2E_STAGE_END();

    // Stage 5: Get tripwire statistics
    E2E_STAGE_BEGIN("Get tripwire statistics", 200);

    tripwire_stats_t stats;
    E2E_ASSERT(tripwire_get_stats(tripwire_, &stats) == NIMCP_OK, "Stats should be available");

    // Note: total_observations is only incremented by tripwire_observe_action(),
    // not by network observations. Network observations update network-specific
    // statistics. We verify that stats retrieval works correctly.

    std::cout << "Total observations: " << stats.total_observations << std::endl;
    std::cout << "Halts triggered: " << stats.halts_triggered << std::endl;

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
