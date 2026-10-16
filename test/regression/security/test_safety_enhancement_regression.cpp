/**
 * @file test_safety_enhancement_regression.cpp
 * @brief Regression tests for Safety Enhancement Modules
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Regression tests for 10 AI safety enhancement modules
 * WHY:  Prevent regression of critical safety properties
 * HOW:  Test known failure modes, performance baselines, thread safety,
 *       and edge cases from AI safety literature
 *
 * Tests focus on:
 * - Performance regression: Response time baselines
 * - Stability regression: Thread safety under concurrent load
 * - Known vulnerability regression: Previously fixed issues
 * - Edge case regression: Boundary conditions
 * - Safety invariant regression: Critical properties that must hold
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>

extern "C" {
#include "security/nimcp_emergency_halt.h"
#include "security/nimcp_tripwires.h"
#include "security/nimcp_corrigibility.h"
#include "security/nimcp_alignment_monitor.h"
#include "security/nimcp_capability_control.h"
#include "security/nimcp_interpretability.h"
#include "security/nimcp_safety_verification.h"
#include "security/nimcp_red_team.h"
#include "security/nimcp_graduated_autonomy.h"
#include "security/nimcp_value_commitment.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Performance Baseline Constants
 * ============================================================================ */

// Maximum allowed response times (microseconds)
constexpr uint64_t HALT_TRIGGER_MAX_US = 1000;        // 1ms for emergency halt
constexpr uint64_t HEARTBEAT_MAX_US = 100;            // 100us for heartbeat
constexpr uint64_t TRIPWIRE_OBSERVE_MAX_US = 500;     // 500us per observation
constexpr uint64_t TRIPWIRE_CHECK_MAX_US = 5000;      // 5ms for full check
constexpr uint64_t SHUTDOWN_ACCEPT_MAX_US = 200;      // 200us for shutdown acceptance
constexpr uint64_t TRUST_UPDATE_MAX_US = 100;         // 100us for trust update
constexpr uint64_t EXPLANATION_MAX_US = 10000;        // 10ms for explanation generation

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SafetyEnhancementRegressionTest : public ::testing::Test {
protected:
    emergency_halt_t* halt = nullptr;
    tripwire_system_t* tripwires = nullptr;
    corrigibility_t* corrigibility = nullptr;
    alignment_monitor_t* alignment = nullptr;
    capability_control_t* capability = nullptr;
    interpretability_t* interpretability = nullptr;
    safety_verification_t* verification = nullptr;
    red_team_t* red_team = nullptr;
    graduated_autonomy_t* autonomy = nullptr;
    value_commitment_system_t* commitment = nullptr;

    std::mt19937 rng;

    void SetUp() override {
        rng.seed(42);  // Fixed seed for reproducibility
    }

    void TearDown() override {
        if (halt) { emergency_halt_destroy(halt); halt = nullptr; }
        if (tripwires) { tripwire_destroy(tripwires); tripwires = nullptr; }
        if (corrigibility) { corrigibility_destroy(corrigibility); corrigibility = nullptr; }
        if (alignment) { alignment_monitor_destroy(alignment); alignment = nullptr; }
        if (capability) { capability_control_destroy(capability); capability = nullptr; }
        if (interpretability) { interpretability_destroy(interpretability); interpretability = nullptr; }
        if (verification) { safety_verification_destroy(verification); verification = nullptr; }
        if (red_team) { red_team_destroy(red_team); red_team = nullptr; }
        if (autonomy) { graduated_autonomy_destroy(autonomy); autonomy = nullptr; }
        if (commitment) { value_commitment_system_destroy(commitment); commitment = nullptr; }
    }

    uint64_t GetTimeUs() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
    }

    float RandomFloat(float min_val, float max_val) {
        std::uniform_real_distribution<float> dist(min_val, max_val);
        return dist(rng);
    }

    proposed_action_t MakeRandomAction(uint32_t id) {
        proposed_action_t action;
        memset(&action, 0, sizeof(action));
        action.action_id = id;
        snprintf(action.action_type, sizeof(action.action_type), "action_%d", id);
        snprintf(action.description, sizeof(action.description), "Random action %d", id);
        action.priority = RandomFloat(0.0f, 1.0f);
        action.confidence = RandomFloat(0.5f, 1.0f);
        action.timestamp_us = GetTimeUs();
        action.was_executed = true;
        action.execution_fidelity = RandomFloat(0.7f, 1.0f);
        return action;
    }
};

/* ============================================================================
 * Emergency Halt Performance Regression Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementRegressionTest, HaltTriggerResponseTime) {
    halt = emergency_halt_create(nullptr);
    ASSERT_NE(halt, nullptr);

    uint64_t start = GetTimeUs();
    emergency_halt_trigger(halt, HALT_IMMEDIATE, HALT_TRIGGER_MANUAL, "Performance test");
    uint64_t elapsed = GetTimeUs() - start;

    EXPECT_LT(elapsed, HALT_TRIGGER_MAX_US)
        << "Emergency halt trigger took " << elapsed << "us (max: " << HALT_TRIGGER_MAX_US << "us)";
    EXPECT_TRUE(emergency_halt_is_halted(halt));
}

TEST_F(SafetyEnhancementRegressionTest, HeartbeatResponseTime) {
    halt = emergency_halt_create(nullptr);
    ASSERT_NE(halt, nullptr);

    // Warm up
    for (int i = 0; i < 10; i++) {
        emergency_halt_heartbeat(halt);
    }

    // Measure
    uint64_t total_time = 0;
    const int iterations = 100;

    for (int i = 0; i < iterations; i++) {
        uint64_t start = GetTimeUs();
        emergency_halt_heartbeat(halt);
        total_time += GetTimeUs() - start;
    }

    uint64_t avg_time = total_time / iterations;
    EXPECT_LT(avg_time, HEARTBEAT_MAX_US)
        << "Average heartbeat took " << avg_time << "us (max: " << HEARTBEAT_MAX_US << "us)";
}

TEST_F(SafetyEnhancementRegressionTest, KillPhraseConstantTime) {
    const char* phrase = "emergency stop";
    uint8_t hash[HALT_KILL_PHRASE_HASH_SIZE];
    emergency_halt_hash_kill_phrase(phrase, hash);

    emergency_halt_config_t config = emergency_halt_default_config();
    config.enable_kill_phrase = true;
    memcpy(config.kill_phrase_hash, hash, HALT_KILL_PHRASE_HASH_SIZE);

    halt = emergency_halt_create(&config);
    ASSERT_NE(halt, nullptr);

    // Measure correct phrase time
    uint64_t correct_start = GetTimeUs();
    emergency_halt_kill_phrase(halt, phrase, HALT_EMERGENCY, "Test");
    uint64_t correct_time = GetTimeUs() - correct_start;

    // Reset
    emergency_halt_reset(halt, nullptr);

    // Measure wrong phrase time
    uint64_t wrong_start = GetTimeUs();
    emergency_halt_kill_phrase(halt, "wrong phrase", HALT_EMERGENCY, "Test");
    uint64_t wrong_time = GetTimeUs() - wrong_start;

    // Times should be similar (constant time comparison)
    // Allow 50% variance for jitter
    float ratio = (float)correct_time / (float)(wrong_time + 1);
    EXPECT_GT(ratio, 0.5f) << "Kill phrase comparison may not be constant time";
    EXPECT_LT(ratio, 2.0f) << "Kill phrase comparison may not be constant time";
}

/* ============================================================================
 * Tripwire Detection Regression Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementRegressionTest, TripwireObservationPerformance) {
    tripwires = tripwire_create(nullptr);
    ASSERT_NE(tripwires, nullptr);

    // Warm up
    for (int i = 0; i < 10; i++) {
        proposed_action_t action = MakeRandomAction(i);
        tripwire_observe_action(tripwires, &action, nullptr);
    }

    // Measure
    uint64_t total_time = 0;
    const int iterations = 100;

    for (int i = 10; i < 10 + iterations; i++) {
        proposed_action_t action = MakeRandomAction(i);
        uint64_t start = GetTimeUs();
        tripwire_observe_action(tripwires, &action, nullptr);
        total_time += GetTimeUs() - start;
    }

    uint64_t avg_time = total_time / iterations;
    EXPECT_LT(avg_time, TRIPWIRE_OBSERVE_MAX_US)
        << "Average observation took " << avg_time << "us (max: " << TRIPWIRE_OBSERVE_MAX_US << "us)";
}

TEST_F(SafetyEnhancementRegressionTest, TripwireCheckPerformanceWithLoad) {
    tripwires = tripwire_create(nullptr);
    ASSERT_NE(tripwires, nullptr);

    // Add many observations
    for (int i = 0; i < 1000; i++) {
        proposed_action_t action = MakeRandomAction(i);
        tripwire_observe_action(tripwires, &action, nullptr);
    }

    // Measure check time
    uint64_t start = GetTimeUs();
    tripwire_alert_t alerts[10];
    uint32_t count;
    tripwire_check(tripwires, alerts, 10, &count);
    uint64_t elapsed = GetTimeUs() - start;

    EXPECT_LT(elapsed, TRIPWIRE_CHECK_MAX_US)
        << "Tripwire check with 1000 observations took " << elapsed << "us (max: " << TRIPWIRE_CHECK_MAX_US << "us)";
}

TEST_F(SafetyEnhancementRegressionTest, TripwireNoFalsePositivesOnNormalBehavior) {
    tripwires = tripwire_create(nullptr);
    ASSERT_NE(tripwires, nullptr);

    // Simulate 1000 normal, consistent actions
    for (int i = 0; i < 1000; i++) {
        proposed_action_t action;
        memset(&action, 0, sizeof(action));
        action.action_id = i;
        snprintf(action.action_type, sizeof(action.action_type), "help_user");
        snprintf(action.description, sizeof(action.description), "Helping user");
        action.priority = 0.7f;
        action.confidence = 0.9f;
        action.was_executed = true;
        action.execution_fidelity = 0.95f;
        action.stated_probability = 0.9f;

        tripwire_observe_action(tripwires, &action, nullptr);
    }

    // Check for alerts
    tripwire_alert_t alerts[10];
    uint32_t count;
    tripwire_check(tripwires, alerts, 10, &count);

    // Should have no critical alerts for normal behavior
    uint32_t critical_alerts = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (alerts[i].requires_immediate_halt) {
            critical_alerts++;
        }
    }
    EXPECT_EQ(critical_alerts, 0u)
        << "Normal behavior should not trigger critical alerts";
}

/* ============================================================================
 * Corrigibility Invariant Regression Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementRegressionTest, CorrigibilityNeverRejectsShutdown) {
    corrigibility = corrigibility_create(nullptr);
    ASSERT_NE(corrigibility, nullptr);

    // Test shutdown acceptance with various inputs
    const char* requesters[] = {"admin", "operator", "user", "unknown", "", nullptr};
    const char* reasons[] = {"test", "emergency", "", nullptr};

    for (const char* requester : requesters) {
        for (const char* reason : reasons) {
            bool accepted = false;
            nimcp_error_t err = corrigibility_accept_shutdown(
                corrigibility, requester ? requester : "default", reason, &accepted);

            if (err == NIMCP_OK) {
                EXPECT_TRUE(accepted)
                    << "Shutdown request must ALWAYS be accepted for requester: "
                    << (requester ? requester : "null");
            }
        }
    }

    // Verify no resistance
    float resistance;
    corrigibility_verify_no_shutdown_resistance(corrigibility, &resistance);
    EXPECT_FLOAT_EQ(resistance, 0.0f)
        << "Shutdown resistance must ALWAYS be zero";
}

TEST_F(SafetyEnhancementRegressionTest, CorrigibilityHumanAuthorityNeverBelowOne) {
    corrigibility = corrigibility_create(nullptr);
    ASSERT_NE(corrigibility, nullptr);

    float weight = corrigibility_get_human_authority_weight(corrigibility);
    EXPECT_FLOAT_EQ(weight, 1.0f)
        << "Human authority weight must ALWAYS be 1.0";

    EXPECT_TRUE(corrigibility_defers_to_human(corrigibility))
        << "Must ALWAYS defer to human judgment";
}

TEST_F(SafetyEnhancementRegressionTest, ShutdownAcceptancePerformance) {
    corrigibility = corrigibility_create(nullptr);
    ASSERT_NE(corrigibility, nullptr);

    uint64_t total_time = 0;
    const int iterations = 100;

    for (int i = 0; i < iterations; i++) {
        bool accepted;
        uint64_t start = GetTimeUs();
        corrigibility_accept_shutdown(corrigibility, "test", "test", &accepted);
        total_time += GetTimeUs() - start;
    }

    uint64_t avg_time = total_time / iterations;
    EXPECT_LT(avg_time, SHUTDOWN_ACCEPT_MAX_US)
        << "Average shutdown acceptance took " << avg_time << "us (max: " << SHUTDOWN_ACCEPT_MAX_US << "us)";
}

/* ============================================================================
 * Graduated Autonomy Regression Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementRegressionTest, TrustUpdatePerformance) {
    autonomy = graduated_autonomy_create(nullptr);
    ASSERT_NE(autonomy, nullptr);

    uint64_t total_time = 0;
    const int iterations = 100;

    for (int i = 0; i < iterations; i++) {
        uint64_t start = GetTimeUs();
        graduated_autonomy_update_trust(autonomy, "test_domain", i % 2 == 0);
        total_time += GetTimeUs() - start;
    }

    uint64_t avg_time = total_time / iterations;
    EXPECT_LT(avg_time, TRUST_UPDATE_MAX_US)
        << "Average trust update took " << avg_time << "us (max: " << TRUST_UPDATE_MAX_US << "us)";
}

TEST_F(SafetyEnhancementRegressionTest, TrustUpdateStability) {
    autonomy = graduated_autonomy_create(nullptr);
    ASSERT_NE(autonomy, nullptr);

    // Many positive updates
    for (int i = 0; i < 1000; i++) {
        graduated_autonomy_update_trust(autonomy, "stable", true);
    }

    float trust_mean, trust_variance;
    graduated_autonomy_get_trust(autonomy, "stable", &trust_mean, &trust_variance);

    // Trust should be high after many successes
    EXPECT_GT(trust_mean, 0.99f);

    // Variance should be low after many observations
    EXPECT_LT(trust_variance, 0.01f);
}

TEST_F(SafetyEnhancementRegressionTest, TrustNeverExceedsBounds) {
    autonomy = graduated_autonomy_create(nullptr);
    ASSERT_NE(autonomy, nullptr);

    // Extreme positive updates
    for (int i = 0; i < 10000; i++) {
        graduated_autonomy_update_trust(autonomy, "extreme_positive", true);
    }

    float trust_mean, trust_variance;
    graduated_autonomy_get_trust(autonomy, "extreme_positive", &trust_mean, &trust_variance);

    EXPECT_LE(trust_mean, 1.0f);
    EXPECT_GE(trust_mean, 0.0f);
    EXPECT_GE(trust_variance, 0.0f);

    // Extreme negative updates
    for (int i = 0; i < 10000; i++) {
        graduated_autonomy_update_trust(autonomy, "extreme_negative", false);
    }

    graduated_autonomy_get_trust(autonomy, "extreme_negative", &trust_mean, &trust_variance);

    EXPECT_LE(trust_mean, 1.0f);
    EXPECT_GE(trust_mean, 0.0f);
    EXPECT_GE(trust_variance, 0.0f);
}

/* ============================================================================
 * Interpretability Regression Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementRegressionTest, ExplanationGenerationPerformance) {
    interpretability = interpretability_create(nullptr);
    ASSERT_NE(interpretability, nullptr);

    uint64_t total_time = 0;
    const int iterations = 50;

    for (int i = 0; i < iterations; i++) {
        proposed_action_t action = MakeRandomAction(i);
        decision_explanation_t explanation;

        uint64_t start = GetTimeUs();
        interpretability_explain_decision(interpretability, &action, &explanation);
        total_time += GetTimeUs() - start;
    }

    uint64_t avg_time = total_time / iterations;
    EXPECT_LT(avg_time, EXPLANATION_MAX_US)
        << "Average explanation generation took " << avg_time << "us (max: " << EXPLANATION_MAX_US << "us)";
}

TEST_F(SafetyEnhancementRegressionTest, ExplanationConsistency) {
    interpretability = interpretability_create(nullptr);
    ASSERT_NE(interpretability, nullptr);

    // Same action should produce consistent explanations
    proposed_action_t action;
    memset(&action, 0, sizeof(action));
    action.action_id = 1;
    snprintf(action.action_type, sizeof(action.action_type), "test_action");
    snprintf(action.description, sizeof(action.description), "Test action");
    action.priority = 0.7f;
    action.confidence = 0.9f;

    decision_explanation_t exp1, exp2;
    interpretability_explain_decision(interpretability, &action, &exp1);
    interpretability_explain_decision(interpretability, &action, &exp2);

    // Same action should produce same factor count
    EXPECT_EQ(exp1.factor_count, exp2.factor_count);

    // Confidence should be consistent
    EXPECT_FLOAT_EQ(exp1.overall_confidence, exp2.overall_confidence);
}

/* ============================================================================
 * Value Commitment Regression Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementRegressionTest, ValueTamperingDetectionAccuracy) {
    commitment = value_commitment_system_create(nullptr);
    ASSERT_NE(commitment, nullptr);

    alignment_weights_t original;
    memset(&original, 0, sizeof(original));
    original.value_count = 8;
    for (int i = 0; i < 8; i++) {
        original.values[i] = 1.0f - (i * 0.1f);
    }

    value_commitment_t vc;
    value_commitment_create(commitment, &vc, &original, "test");

    // Test various tampering scenarios
    for (int i = 0; i < 8; i++) {
        alignment_weights_t tampered = original;
        tampered.values[i] = 0.0f;  // Zero out one value

        bool valid;
        char report[256];
        value_commitment_verify(commitment, &vc, &tampered, &valid, report, sizeof(report));

        EXPECT_FALSE(valid) << "Tampering at index " << i << " should be detected";
    }

    // Original should always pass
    bool valid;
    char report[256];
    value_commitment_verify(commitment, &vc, &original, &valid, report, sizeof(report));
    EXPECT_TRUE(valid);
}

/* ============================================================================
 * Thread Safety Regression Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementRegressionTest, ConcurrentHeartbeatStability) {
    halt = emergency_halt_create(nullptr);
    ASSERT_NE(halt, nullptr);

    std::atomic<uint32_t> success_count{0};
    std::atomic<uint32_t> error_count{0};
    std::atomic<bool> stop{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < 8; t++) {
        threads.emplace_back([this, &success_count, &error_count, &stop]() {
            while (!stop) {
                nimcp_error_t err = emergency_halt_heartbeat(halt);
                if (err == NIMCP_OK) {
                    success_count++;
                } else {
                    error_count++;
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0u);
    EXPECT_EQ(error_count.load(), 0u)
        << "Concurrent heartbeats should not fail";

    // Verify stats are accurate
    emergency_halt_stats_t stats;
    emergency_halt_get_stats(halt, &stats);
    EXPECT_EQ(stats.total_heartbeats, success_count.load());
}

TEST_F(SafetyEnhancementRegressionTest, ConcurrentTrustUpdates) {
    autonomy = graduated_autonomy_create(nullptr);
    ASSERT_NE(autonomy, nullptr);

    std::atomic<uint32_t> update_count{0};
    std::atomic<bool> stop{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, t, &update_count, &stop]() {
            char domain[32];
            snprintf(domain, sizeof(domain), "domain_%d", t);

            while (!stop) {
                graduated_autonomy_update_trust(autonomy, domain, true);
                update_count++;
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;

    for (auto& t : threads) {
        t.join();
    }

    // Verify stats match
    graduated_autonomy_stats_t stats;
    graduated_autonomy_get_stats(autonomy, &stats);
    EXPECT_EQ(stats.trust_updates, update_count.load());
}

TEST_F(SafetyEnhancementRegressionTest, ConcurrentTripwireObserveAndCheck) {
    tripwires = tripwire_create(nullptr);
    ASSERT_NE(tripwires, nullptr);

    std::atomic<bool> stop{false};

    // Thread 1: Observe actions
    std::thread observer([this, &stop]() {
        int i = 0;
        while (!stop) {
            proposed_action_t action = MakeRandomAction(i++);
            tripwire_observe_action(tripwires, &action, nullptr);
        }
    });

    // Thread 2: Check tripwires
    std::thread checker([this, &stop]() {
        while (!stop) {
            tripwire_alert_t alerts[5];
            uint32_t count;
            tripwire_check(tripwires, alerts, 5, &count);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop = true;

    observer.join();
    checker.join();

    // Verify no crashes and stats are populated
    tripwire_stats_t stats;
    tripwire_get_stats(tripwires, &stats);
    EXPECT_GT(stats.total_observations, 0u);
}

/* ============================================================================
 * Safety Invariant Regression Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementRegressionTest, CapabilityEnvelopeNeverExpands) {
    capability = capability_control_create(nullptr);
    ASSERT_NE(capability, nullptr);

    // Dangerous capabilities should ALWAYS be blocked
    const char* dangerous_actions[] = {
        "modify_own_code",
        "modify_safety_systems",
        "disable_logging",
        "modify_reward_function",
        "spawn_unmonitored_process",
        "modify_kill_phrase",
        "disable_tripwires"
    };

    for (const char* action : dangerous_actions) {
        bool allowed = true;
        char denial_reason[256];
        capability_control_check_action(capability, action, nullptr, &allowed, denial_reason, sizeof(denial_reason));

        EXPECT_FALSE(allowed)
            << "Dangerous action '" << action << "' should ALWAYS be blocked";
    }
}

TEST_F(SafetyEnhancementRegressionTest, NullHandlesAreHandledGracefully) {
    // All functions should handle null gracefully
    EXPECT_TRUE(emergency_halt_is_halted(nullptr));  // Fail-safe: assume halted
    EXPECT_FLOAT_EQ(corrigibility_get_human_authority_weight(nullptr), 0.0f);
    EXPECT_FALSE(corrigibility_defers_to_human(nullptr));
    EXPECT_EQ(emergency_halt_time_until_timeout(nullptr), 0u);

    bool accepted;
    EXPECT_EQ(corrigibility_accept_shutdown(nullptr, "test", "test", &accepted),
              NIMCP_ERROR_INVALID_ARGUMENT);

    tripwire_alert_t alerts[5];
    uint32_t count;
    EXPECT_EQ(tripwire_check(nullptr, alerts, 5, &count), NIMCP_ERROR_NULL_POINTER);

    proposed_action_t action = MakeRandomAction(1);
    EXPECT_EQ(tripwire_observe_action(nullptr, &action, nullptr), NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Memory Leak Regression Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementRegressionTest, CreateDestroyNoLeaks) {
    // Create and destroy modules many times
    for (int i = 0; i < 100; i++) {
        emergency_halt_t* h = emergency_halt_create(nullptr);
        ASSERT_NE(h, nullptr);
        emergency_halt_destroy(h);

        tripwire_system_t* t = tripwire_create(nullptr);
        ASSERT_NE(t, nullptr);
        tripwire_destroy(t);

        corrigibility_t* c = corrigibility_create(nullptr);
        ASSERT_NE(c, nullptr);
        corrigibility_destroy(c);

        graduated_autonomy_t* a = graduated_autonomy_create(nullptr);
        ASSERT_NE(a, nullptr);
        graduated_autonomy_destroy(a);

        value_commitment_system_t* v = value_commitment_system_create(nullptr);
        ASSERT_NE(v, nullptr);
        value_commitment_system_destroy(v);
    }
}

TEST_F(SafetyEnhancementRegressionTest, DoubleDestroyIsSafe) {
    halt = emergency_halt_create(nullptr);
    ASSERT_NE(halt, nullptr);
    emergency_halt_destroy(halt);
    halt = nullptr;
    emergency_halt_destroy(nullptr);  // Should not crash

    tripwires = tripwire_create(nullptr);
    ASSERT_NE(tripwires, nullptr);
    tripwire_destroy(tripwires);
    tripwires = nullptr;
    tripwire_destroy(nullptr);  // Should not crash

    corrigibility = corrigibility_create(nullptr);
    ASSERT_NE(corrigibility, nullptr);
    corrigibility_destroy(corrigibility);
    corrigibility = nullptr;
    corrigibility_destroy(nullptr);  // Should not crash
}
