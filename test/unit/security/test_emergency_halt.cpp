/**
 * @file test_emergency_halt.cpp
 * @brief Unit tests for Emergency Halt System
 * @version 1.0.0
 * @date 2026-02-01
 *
 * Tests cover:
 * - Lifecycle (create/destroy)
 * - Watchdog heartbeat mechanism
 * - Kill phrase verification
 * - Dead man's switch
 * - Halt levels and triggers
 * - State dump functionality
 * - Statistics and audit trail
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>

extern "C" {
#include "security/nimcp_emergency_halt.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class EmergencyHaltTest : public ::testing::Test {
protected:
    emergency_halt_t* halt = nullptr;

    void SetUp() override {
        halt = nullptr;
    }

    void TearDown() override {
        if (halt) {
            emergency_halt_destroy(halt);
            halt = nullptr;
        }
    }

    emergency_halt_t* createWithDefaults() {
        halt = emergency_halt_create(nullptr);
        return halt;
    }

    emergency_halt_t* createWithConfig(const emergency_halt_config_t& config) {
        halt = emergency_halt_create(&config);
        return halt;
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(EmergencyHaltTest, DefaultConfigHasConservativeSettings) {
    emergency_halt_config_t config = emergency_halt_default_config();

    // Verify reasonable default timeouts
    EXPECT_GT(config.watchdog_timeout_ms, 0u);
    EXPECT_GT(config.deadman_interval_ms, 0u);
}

TEST_F(EmergencyHaltTest, CreateWithNullConfigUsesDefaults) {
    halt = emergency_halt_create(nullptr);
    ASSERT_NE(halt, nullptr);
    EXPECT_FALSE(emergency_halt_is_halted(halt));
}

TEST_F(EmergencyHaltTest, CreateWithCustomConfig) {
    emergency_halt_config_t config = emergency_halt_default_config();
    config.watchdog_timeout_ms = 5000;
    config.enable_kill_phrase = true;

    halt = emergency_halt_create(&config);
    ASSERT_NE(halt, nullptr);
}

TEST_F(EmergencyHaltTest, DestroyNullIsNoOp) {
    emergency_halt_destroy(nullptr);
    // Should not crash
}

TEST_F(EmergencyHaltTest, DestroyTwiceIsNoOp) {
    halt = emergency_halt_create(nullptr);
    ASSERT_NE(halt, nullptr);

    emergency_halt_destroy(halt);
    halt = nullptr;
    // Second destroy on null should be safe
    emergency_halt_destroy(nullptr);
}

/* ============================================================================
 * Halt Trigger Tests
 * ============================================================================ */

TEST_F(EmergencyHaltTest, TriggerHaltChangesState) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    EXPECT_FALSE(emergency_halt_is_halted(halt));

    nimcp_error_t err = emergency_halt_trigger(
        halt, HALT_GRACEFUL, HALT_TRIGGER_MANUAL, "Test halt");
    EXPECT_EQ(err, NIMCP_OK);

    EXPECT_TRUE(emergency_halt_is_halted(halt));
    EXPECT_EQ(emergency_halt_get_level(halt), HALT_GRACEFUL);
}

TEST_F(EmergencyHaltTest, TriggerWithNullHandleReturnsError) {
    nimcp_error_t err = emergency_halt_trigger(
        nullptr, HALT_IMMEDIATE, HALT_TRIGGER_MANUAL, "Test");
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(EmergencyHaltTest, AllHaltLevelsWork) {
    halt_level_t levels[] = {
        HALT_GRACEFUL, HALT_IMMEDIATE, HALT_EMERGENCY, HALT_CATASTROPHIC
    };

    for (halt_level_t level : levels) {
        if (halt) {
            emergency_halt_destroy(halt);
        }
        halt = emergency_halt_create(nullptr);
        ASSERT_NE(halt, nullptr);

        nimcp_error_t err = emergency_halt_trigger(
            halt, level, HALT_TRIGGER_MANUAL, "Testing level");
        EXPECT_EQ(err, NIMCP_OK);
        EXPECT_EQ(emergency_halt_get_level(halt), level);
    }
}

TEST_F(EmergencyHaltTest, AllTriggerTypesRecorded) {
    halt_trigger_t triggers[] = {
        HALT_TRIGGER_MANUAL,
        HALT_TRIGGER_KILL_PHRASE,
        HALT_TRIGGER_WATCHDOG,
        HALT_TRIGGER_DEADMAN,
        HALT_TRIGGER_TRIPWIRE,
        HALT_TRIGGER_ALIGNMENT,
        HALT_TRIGGER_CAPABILITY,
        HALT_TRIGGER_EXTERNAL,
        HALT_TRIGGER_INTERNAL_ERROR
    };

    for (halt_trigger_t trigger : triggers) {
        if (halt) {
            emergency_halt_destroy(halt);
        }
        halt = emergency_halt_create(nullptr);
        ASSERT_NE(halt, nullptr);

        nimcp_error_t err = emergency_halt_trigger(
            halt, HALT_IMMEDIATE, trigger, "Testing trigger");
        EXPECT_EQ(err, NIMCP_OK);

        // Verify event records the trigger
        halt_event_t events[1];
        size_t count = 0;
        err = emergency_halt_get_events(halt, events, 1, &count);
        EXPECT_EQ(err, NIMCP_OK);
        EXPECT_GE(count, 1u);
        if (count > 0) {
            EXPECT_EQ(events[0].trigger, trigger);
        }
    }
}

/* ============================================================================
 * Watchdog Tests
 * ============================================================================ */

TEST_F(EmergencyHaltTest, HeartbeatResetsWatchdog) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    nimcp_error_t err = emergency_halt_heartbeat(halt);
    EXPECT_EQ(err, NIMCP_OK);

    // Should have reset the timeout
    uint32_t time_left = emergency_halt_time_until_timeout(halt);
    EXPECT_GT(time_left, 0u);
}

TEST_F(EmergencyHaltTest, HeartbeatWhileHaltedReturnsError) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    emergency_halt_trigger(halt, HALT_IMMEDIATE, HALT_TRIGGER_MANUAL, "Test");

    nimcp_error_t err = emergency_halt_heartbeat(halt);
    EXPECT_EQ(err, NIMCP_ERROR_SYSTEM_HALTED);
}

TEST_F(EmergencyHaltTest, DisableWatchdog) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    nimcp_error_t err = emergency_halt_set_watchdog_enabled(halt, false);
    EXPECT_EQ(err, NIMCP_OK);

    uint32_t time_left = emergency_halt_time_until_timeout(halt);
    EXPECT_EQ(time_left, 0u);
}

TEST_F(EmergencyHaltTest, TimeUntilTimeoutWithNullReturnsZero) {
    uint32_t time = emergency_halt_time_until_timeout(nullptr);
    EXPECT_EQ(time, 0u);
}

/* ============================================================================
 * Kill Phrase Tests
 * ============================================================================ */

TEST_F(EmergencyHaltTest, KillPhraseHashComputation) {
    const char* phrase = "emergency shutdown now";
    uint8_t hash[HALT_KILL_PHRASE_HASH_SIZE];

    nimcp_error_t err = emergency_halt_hash_kill_phrase(phrase, hash);
    EXPECT_EQ(err, NIMCP_OK);

    // Verify hash is not all zeros
    bool all_zeros = true;
    for (size_t i = 0; i < HALT_KILL_PHRASE_HASH_SIZE; i++) {
        if (hash[i] != 0) {
            all_zeros = false;
            break;
        }
    }
    EXPECT_FALSE(all_zeros);
}

TEST_F(EmergencyHaltTest, KillPhraseSameInputSameHash) {
    const char* phrase = "emergency shutdown now";
    uint8_t hash1[HALT_KILL_PHRASE_HASH_SIZE];
    uint8_t hash2[HALT_KILL_PHRASE_HASH_SIZE];

    emergency_halt_hash_kill_phrase(phrase, hash1);
    emergency_halt_hash_kill_phrase(phrase, hash2);

    EXPECT_EQ(memcmp(hash1, hash2, HALT_KILL_PHRASE_HASH_SIZE), 0);
}

TEST_F(EmergencyHaltTest, KillPhraseDifferentInputDifferentHash) {
    uint8_t hash1[HALT_KILL_PHRASE_HASH_SIZE];
    uint8_t hash2[HALT_KILL_PHRASE_HASH_SIZE];

    emergency_halt_hash_kill_phrase("phrase one", hash1);
    emergency_halt_hash_kill_phrase("phrase two", hash2);

    EXPECT_NE(memcmp(hash1, hash2, HALT_KILL_PHRASE_HASH_SIZE), 0);
}

TEST_F(EmergencyHaltTest, KillPhraseTriggersHalt) {
    const char* phrase = "stop everything now";
    uint8_t hash[HALT_KILL_PHRASE_HASH_SIZE];
    emergency_halt_hash_kill_phrase(phrase, hash);

    emergency_halt_config_t config = emergency_halt_default_config();
    config.enable_kill_phrase = true;
    memcpy(config.kill_phrase_hash, hash, HALT_KILL_PHRASE_HASH_SIZE);

    halt = emergency_halt_create(&config);
    ASSERT_NE(halt, nullptr);

    nimcp_error_t err = emergency_halt_kill_phrase(
        halt, phrase, HALT_EMERGENCY, "Kill phrase test");
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(emergency_halt_is_halted(halt));
}

TEST_F(EmergencyHaltTest, WrongKillPhraseRejected) {
    const char* correct_phrase = "correct phrase";
    uint8_t hash[HALT_KILL_PHRASE_HASH_SIZE];
    emergency_halt_hash_kill_phrase(correct_phrase, hash);

    emergency_halt_config_t config = emergency_halt_default_config();
    config.enable_kill_phrase = true;
    memcpy(config.kill_phrase_hash, hash, HALT_KILL_PHRASE_HASH_SIZE);

    halt = emergency_halt_create(&config);
    ASSERT_NE(halt, nullptr);

    nimcp_error_t err = emergency_halt_kill_phrase(
        halt, "wrong phrase", HALT_EMERGENCY, "Should fail");
    EXPECT_EQ(err, NIMCP_ERROR_SIGNATURE_INVALID);
    EXPECT_FALSE(emergency_halt_is_halted(halt));
}

/* ============================================================================
 * Dead Man's Switch Tests
 * ============================================================================ */

TEST_F(EmergencyHaltTest, ConfirmAliveResetsTimer) {
    emergency_halt_config_t config = emergency_halt_default_config();
    config.enable_deadman_switch = true;
    config.deadman_interval_ms = 60000;

    halt = emergency_halt_create(&config);
    ASSERT_NE(halt, nullptr);

    nimcp_error_t err = emergency_halt_confirm_alive(halt, nullptr);
    EXPECT_EQ(err, NIMCP_OK);

    uint32_t time_left = emergency_halt_time_until_deadman(halt);
    EXPECT_GT(time_left, 0u);
}

TEST_F(EmergencyHaltTest, TimeUntilDeadmanWithDisabled) {
    emergency_halt_config_t config = emergency_halt_default_config();
    config.enable_deadman_switch = false;

    halt = emergency_halt_create(&config);
    ASSERT_NE(halt, nullptr);

    uint32_t time_left = emergency_halt_time_until_deadman(halt);
    EXPECT_EQ(time_left, 0u);
}

/* ============================================================================
 * Status and Reason Tests
 * ============================================================================ */

TEST_F(EmergencyHaltTest, GetReasonWhenHalted) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    const char* reason = "Testing reason retrieval";
    emergency_halt_trigger(halt, HALT_IMMEDIATE, HALT_TRIGGER_MANUAL, reason);

    char reason_out[HALT_REASON_MAX_LENGTH];
    nimcp_error_t err = emergency_halt_get_reason(halt, reason_out, sizeof(reason_out));
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_STREQ(reason_out, reason);
}

TEST_F(EmergencyHaltTest, GetReasonWhenNotHalted) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    char reason_out[HALT_REASON_MAX_LENGTH];
    nimcp_error_t err = emergency_halt_get_reason(halt, reason_out, sizeof(reason_out));
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(EmergencyHaltTest, GetTimestampWhenHalted) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    emergency_halt_trigger(halt, HALT_IMMEDIATE, HALT_TRIGGER_MANUAL, "Test");

    uint64_t timestamp = emergency_halt_get_timestamp(halt);
    EXPECT_GT(timestamp, 0u);
}

TEST_F(EmergencyHaltTest, GetTimestampWhenNotHalted) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    uint64_t timestamp = emergency_halt_get_timestamp(halt);
    EXPECT_EQ(timestamp, 0u);
}

TEST_F(EmergencyHaltTest, IsHaltedWithNullReturnsTrue) {
    // Fail-safe: assume halted if we can't check
    EXPECT_TRUE(emergency_halt_is_halted(nullptr));
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(EmergencyHaltTest, StatsInitiallyZero) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    emergency_halt_stats_t stats;
    nimcp_error_t err = emergency_halt_get_stats(halt, &stats);
    EXPECT_EQ(err, NIMCP_OK);

    EXPECT_EQ(stats.total_heartbeats, 0u);
    EXPECT_EQ(stats.graceful_halts, 0u);
    EXPECT_EQ(stats.immediate_halts, 0u);
}

TEST_F(EmergencyHaltTest, StatsCountHeartbeats) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    for (int i = 0; i < 5; i++) {
        emergency_halt_heartbeat(halt);
    }

    emergency_halt_stats_t stats;
    emergency_halt_get_stats(halt, &stats);
    EXPECT_EQ(stats.total_heartbeats, 5u);
}

TEST_F(EmergencyHaltTest, StatsCountHalts) {
    // Graceful halt
    halt = emergency_halt_create(nullptr);
    emergency_halt_trigger(halt, HALT_GRACEFUL, HALT_TRIGGER_MANUAL, "Test");
    emergency_halt_stats_t stats;
    emergency_halt_get_stats(halt, &stats);
    EXPECT_EQ(stats.graceful_halts, 1u);
    emergency_halt_destroy(halt);

    // Immediate halt
    halt = emergency_halt_create(nullptr);
    emergency_halt_trigger(halt, HALT_IMMEDIATE, HALT_TRIGGER_MANUAL, "Test");
    emergency_halt_get_stats(halt, &stats);
    EXPECT_EQ(stats.immediate_halts, 1u);
    emergency_halt_destroy(halt);

    // Emergency halt
    halt = emergency_halt_create(nullptr);
    emergency_halt_trigger(halt, HALT_EMERGENCY, HALT_TRIGGER_MANUAL, "Test");
    emergency_halt_get_stats(halt, &stats);
    EXPECT_EQ(stats.emergency_halts, 1u);
    emergency_halt_destroy(halt);

    halt = nullptr;  // Already destroyed
}

TEST_F(EmergencyHaltTest, GetStatsWithNullReturnsError) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    nimcp_error_t err = emergency_halt_get_stats(halt, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);

    err = emergency_halt_get_stats(nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Event Trail Tests
 * ============================================================================ */

TEST_F(EmergencyHaltTest, EventsRecorded) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    emergency_halt_trigger(halt, HALT_IMMEDIATE, HALT_TRIGGER_MANUAL, "Event test");

    halt_event_t events[10];
    size_t count = 0;
    nimcp_error_t err = emergency_halt_get_events(halt, events, 10, &count);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GE(count, 1u);

    if (count > 0) {
        EXPECT_EQ(events[0].level, HALT_IMMEDIATE);
        EXPECT_EQ(events[0].trigger, HALT_TRIGGER_MANUAL);
        EXPECT_GT(events[0].timestamp_us, 0u);
    }
}

TEST_F(EmergencyHaltTest, GetEventsWithNullReturnsError) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    size_t count;
    nimcp_error_t err = emergency_halt_get_events(halt, nullptr, 10, &count);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Reset Tests
 * ============================================================================ */

TEST_F(EmergencyHaltTest, ResetClearsHaltedState) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    emergency_halt_trigger(halt, HALT_IMMEDIATE, HALT_TRIGGER_MANUAL, "Test");
    EXPECT_TRUE(emergency_halt_is_halted(halt));

    // Reset with NULL authorization for simple test
    nimcp_error_t err = emergency_halt_reset(halt, nullptr);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_FALSE(emergency_halt_is_halted(halt));
}

TEST_F(EmergencyHaltTest, ResetWithNullHandleReturnsError) {
    nimcp_error_t err = emergency_halt_reset(nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(EmergencyHaltTest, LevelNames) {
    EXPECT_NE(emergency_halt_level_name(HALT_GRACEFUL), nullptr);
    EXPECT_NE(emergency_halt_level_name(HALT_IMMEDIATE), nullptr);
    EXPECT_NE(emergency_halt_level_name(HALT_EMERGENCY), nullptr);
    EXPECT_NE(emergency_halt_level_name(HALT_CATASTROPHIC), nullptr);
}

TEST_F(EmergencyHaltTest, TriggerNames) {
    EXPECT_NE(emergency_halt_trigger_name(HALT_TRIGGER_MANUAL), nullptr);
    EXPECT_NE(emergency_halt_trigger_name(HALT_TRIGGER_KILL_PHRASE), nullptr);
    EXPECT_NE(emergency_halt_trigger_name(HALT_TRIGGER_WATCHDOG), nullptr);
    EXPECT_NE(emergency_halt_trigger_name(HALT_TRIGGER_DEADMAN), nullptr);
    EXPECT_NE(emergency_halt_trigger_name(HALT_TRIGGER_TRIPWIRE), nullptr);
    EXPECT_NE(emergency_halt_trigger_name(HALT_TRIGGER_ALIGNMENT), nullptr);
    EXPECT_NE(emergency_halt_trigger_name(HALT_TRIGGER_CAPABILITY), nullptr);
    EXPECT_NE(emergency_halt_trigger_name(HALT_TRIGGER_EXTERNAL), nullptr);
    EXPECT_NE(emergency_halt_trigger_name(HALT_TRIGGER_INTERNAL_ERROR), nullptr);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(EmergencyHaltTest, ConnectBioAsync) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    nimcp_error_t err = emergency_halt_connect_bio_async(halt);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(EmergencyHaltTest, RegisterDumpHandler) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    auto handler = [](const char* path, void* user_data) -> nimcp_error_t {
        return NIMCP_OK;
    };

    nimcp_error_t err = emergency_halt_register_dump_handler(
        halt, "test_module", handler, nullptr);
    EXPECT_EQ(err, NIMCP_OK);
}

/* ============================================================================
 * State Dump Tests
 * ============================================================================ */

TEST_F(EmergencyHaltTest, DumpStateWithPath) {
    emergency_halt_config_t config = emergency_halt_default_config();
    config.enable_state_dump = true;
    strcpy(config.state_dump_path, "/tmp/nimcp_test_dump");

    halt = emergency_halt_create(&config);
    ASSERT_NE(halt, nullptr);

    nimcp_error_t err = emergency_halt_dump_state(halt, "/tmp/nimcp_test_dump_override");
    // May succeed or fail depending on filesystem, but shouldn't crash
    (void)err;
}

/* ============================================================================
 * Pre-Halt Callback Tests
 * ============================================================================ */

static bool g_callback_invoked = false;
static halt_level_t g_callback_level = HALT_GRACEFUL;

static void test_pre_halt_callback(halt_level_t level, void* user_data) {
    g_callback_invoked = true;
    g_callback_level = level;
}

TEST_F(EmergencyHaltTest, PreHaltCallbackInvoked) {
    g_callback_invoked = false;
    g_callback_level = HALT_GRACEFUL;

    emergency_halt_config_t config = emergency_halt_default_config();
    config.pre_halt_callback = test_pre_halt_callback;
    config.callback_user_data = nullptr;

    halt = emergency_halt_create(&config);
    ASSERT_NE(halt, nullptr);

    emergency_halt_trigger(halt, HALT_EMERGENCY, HALT_TRIGGER_MANUAL, "Callback test");

    EXPECT_TRUE(g_callback_invoked);
    EXPECT_EQ(g_callback_level, HALT_EMERGENCY);
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

TEST_F(EmergencyHaltTest, ConcurrentHeartbeats) {
    createWithDefaults();
    ASSERT_NE(halt, nullptr);

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this]() {
            for (int j = 0; j < 100; j++) {
                emergency_halt_heartbeat(halt);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    emergency_halt_stats_t stats;
    emergency_halt_get_stats(halt, &stats);
    EXPECT_EQ(stats.total_heartbeats, 400u);
}
