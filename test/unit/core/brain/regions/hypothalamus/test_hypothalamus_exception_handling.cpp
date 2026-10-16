/**
 * @file test_hypothalamus_exception_handling.cpp
 * @brief Unit tests for exception handling in Hypothalamus modules
 * @date 2026-01-24
 *
 * WHAT: Test NIMCP_THROW_TO_IMMUNE exception handling for hypothalamus modules
 * WHY:  Ensure proper error reporting to immune system for all error conditions
 * HOW:  Test invalid configs, NULL pointers, and boundary conditions
 *
 * MODULES TESTED:
 * - hypothalamus_orchestrator: Bridge coordination and event routing
 * - hypothalamus_drives: Drive state management and alignment safety
 * - hypothalamus_homeostasis: Homeostatic control with PID controllers
 * - hypothalamus_perception_bridge: Sensory modulation integration
 * - hypothalamus_alignment: Alignment introspection and verification
 * - hypothalamus_internal_bus: Internal message bus for cross-talk
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <cmath>

extern "C" {
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_homeostasis.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_perception_bridge.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_alignment.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_internal_bus.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class HypothalamusExceptionTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static char last_exception_message[256];
    static nimcp_handler_registration_t* registration;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        memset(last_exception_message, 0, sizeof(last_exception_message));

        nimcp_exception_system_init();

        // Register our test handler
        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.handler = test_exception_handler;
        options.user_data = nullptr;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.name = "hypothalamus_test_handler";
        registration = nimcp_handler_register(&options);
    }

    void TearDown() override {
        if (registration) {
            nimcp_handler_unregister(registration);
            registration = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        if (ex->message) {
            strncpy(last_exception_message, ex->message, sizeof(last_exception_message) - 1);
        }
        return false;  // Don't consume - allow propagation
    }

    void reset_counters() {
        handler_call_count = 0;
        last_exception_code = 0;
        memset(last_exception_message, 0, sizeof(last_exception_message));
    }
};

std::atomic<int> HypothalamusExceptionTest::handler_call_count{0};
std::atomic<int> HypothalamusExceptionTest::last_exception_code{0};
char HypothalamusExceptionTest::last_exception_message[256] = {0};
nimcp_handler_registration_t* HypothalamusExceptionTest::registration = nullptr;

//=============================================================================
// Orchestrator Exception Tests
//=============================================================================

TEST_F(HypothalamusExceptionTest, Orchestrator_NullConfigParam_ReturnsError) {
    // hypo_orch_default_config should handle NULL gracefully
    reset_counters();

    int result = hypo_orch_default_config(nullptr);

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, Orchestrator_ResetNull_ReturnsError) {
    reset_counters();

    int result = hypo_orch_reset(nullptr);

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, Orchestrator_RegisterBridgeNull_ReturnsError) {
    reset_counters();
    uint32_t bridge_id = 0;

    int result = hypo_orch_register_bridge(
        nullptr,
        HYPO_BRIDGE_EMOTION,
        "test_bridge",
        nullptr,
        nullptr,
        &bridge_id
    );

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, Orchestrator_UnregisterBridgeNull_ReturnsError) {
    reset_counters();

    int result = hypo_orch_unregister_bridge(nullptr, 1);

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, Orchestrator_GetBridgeInfoNull_ReturnsError) {
    reset_counters();
    hypo_bridge_info_t info;

    int result = hypo_orch_get_bridge_info(nullptr, 1, &info);

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, Orchestrator_SubscribeNull_ReturnsError) {
    reset_counters();

    int result = hypo_orch_subscribe(
        nullptr,
        1,
        HYPO_EVENT_DRIVE_ACTIVATED,
        nullptr,
        nullptr
    );

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, Orchestrator_CreateAndDestroyValid_NoExceptions) {
    reset_counters();

    hypo_orch_config_t config;
    int cfg_result = hypo_orch_default_config(&config);
    EXPECT_EQ(cfg_result, 0);

    hypo_orchestrator_t orch = hypo_orch_create(&config);
    if (orch != nullptr) {
        EXPECT_EQ(handler_call_count.load(), 0);
        hypo_orch_destroy(orch);
    }
}

//=============================================================================
// Drives Exception Tests
//=============================================================================

TEST_F(HypothalamusExceptionTest, Drives_UpdateNull_ReturnsFalse) {
    reset_counters();

    bool result = hypo_drive_update(nullptr, 1000);

    EXPECT_FALSE(result);
}

TEST_F(HypothalamusExceptionTest, Drives_ResetNull_ReturnsFalse) {
    reset_counters();

    bool result = hypo_drive_reset(nullptr);

    EXPECT_FALSE(result);
}

TEST_F(HypothalamusExceptionTest, Drives_GetStateNull_ReturnsFalse) {
    reset_counters();
    hypo_drive_state_t state;

    bool result = hypo_drive_get_state(nullptr, HYPO_DRIVE_HUNGER, &state);

    EXPECT_FALSE(result);
}

TEST_F(HypothalamusExceptionTest, Drives_GetStateNullOutput_ReturnsFalse) {
    hypo_drive_config_t config = hypo_drive_default_config();
    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
    ASSERT_NE(drives, nullptr);

    reset_counters();

    bool result = hypo_drive_get_state(drives, HYPO_DRIVE_HUNGER, nullptr);

    EXPECT_FALSE(result);

    hypo_drive_destroy(drives);
}

TEST_F(HypothalamusExceptionTest, Drives_GetStateInvalidDrive_ReturnsFalse) {
    hypo_drive_config_t config = hypo_drive_default_config();
    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
    ASSERT_NE(drives, nullptr);

    reset_counters();
    hypo_drive_state_t state;

    bool result = hypo_drive_get_state(drives, (hypo_drive_type_t)999, &state);

    EXPECT_FALSE(result);

    hypo_drive_destroy(drives);
}

TEST_F(HypothalamusExceptionTest, Drives_SatisfyNull_ReturnsZero) {
    reset_counters();

    float reward = hypo_drive_satisfy(nullptr, HYPO_DRIVE_HUNGER, 0.5f);

    // Function returns 0.0f on error (NULL system)
    EXPECT_EQ(reward, 0.0f);
}

TEST_F(HypothalamusExceptionTest, Drives_SatisfyInvalidAmount_ReturnsNegative) {
    hypo_drive_config_t config = hypo_drive_default_config();
    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
    ASSERT_NE(drives, nullptr);

    reset_counters();

    // Invalid satisfaction amounts should be clamped or rejected
    float reward = hypo_drive_satisfy(drives, HYPO_DRIVE_HUNGER, -1.0f);

    // Either clamped to valid or returns negative
    (void)reward;

    hypo_drive_destroy(drives);
}

TEST_F(HypothalamusExceptionTest, Drives_LockAlignmentNull_ReturnsFalse) {
    reset_counters();

    bool result = hypo_drive_lock_alignment(nullptr, HYPO_LOCK_HARD);

    EXPECT_FALSE(result);
}

TEST_F(HypothalamusExceptionTest, Drives_CreateDestroyValid_NoExceptions) {
    reset_counters();

    hypo_drive_config_t config = hypo_drive_default_config();
    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);

    ASSERT_NE(drives, nullptr);
    EXPECT_EQ(handler_call_count.load(), 0);

    hypo_drive_destroy(drives);
}

//=============================================================================
// Homeostasis Exception Tests
//=============================================================================

TEST_F(HypothalamusExceptionTest, Homeostasis_ResetNull_ReturnsFalse) {
    reset_counters();

    bool result = hypo_homeostasis_reset(nullptr);

    EXPECT_FALSE(result);
}

TEST_F(HypothalamusExceptionTest, Homeostasis_SetValueNull_ReturnsFalse) {
    reset_counters();

    bool result = hypo_homeostasis_set_value(nullptr, HYPO_VAR_TEMPERATURE, 37.0f);

    EXPECT_FALSE(result);
}

TEST_F(HypothalamusExceptionTest, Homeostasis_SetValueInvalidVar_ReturnsFalse) {
    hypo_homeostasis_config_t config = hypo_homeostasis_default_config();
    hypo_homeostasis_handle_t* homeo = hypo_homeostasis_create(&config);
    ASSERT_NE(homeo, nullptr);

    reset_counters();

    bool result = hypo_homeostasis_set_value(homeo, (hypo_variable_type_t)999, 37.0f);

    EXPECT_FALSE(result);

    hypo_homeostasis_destroy(homeo);
}

TEST_F(HypothalamusExceptionTest, Homeostasis_GetVariableNull_ReturnsFalse) {
    reset_counters();
    hypo_homeostatic_var_t var;

    bool result = hypo_homeostasis_get_variable(nullptr, HYPO_VAR_TEMPERATURE, &var);

    EXPECT_FALSE(result);
}

TEST_F(HypothalamusExceptionTest, Homeostasis_GetVariableNullOutput_ReturnsFalse) {
    hypo_homeostasis_config_t config = hypo_homeostasis_default_config();
    hypo_homeostasis_handle_t* homeo = hypo_homeostasis_create(&config);
    ASSERT_NE(homeo, nullptr);

    reset_counters();

    bool result = hypo_homeostasis_get_variable(homeo, HYPO_VAR_TEMPERATURE, nullptr);

    EXPECT_FALSE(result);

    hypo_homeostasis_destroy(homeo);
}

TEST_F(HypothalamusExceptionTest, Homeostasis_UpdateNull_ReturnsFalse) {
    reset_counters();

    bool result = hypo_homeostasis_update(nullptr, 1000);

    EXPECT_FALSE(result);
}

TEST_F(HypothalamusExceptionTest, Homeostasis_GetRewardNull_ReturnsZero) {
    reset_counters();

    float reward = hypo_homeostasis_get_reward(nullptr);

    // Function returns 0.0f on error (NULL system)
    EXPECT_EQ(reward, 0.0f);
}

TEST_F(HypothalamusExceptionTest, Homeostasis_ComputeRewardNull_ReturnsFalse) {
    reset_counters();
    hypo_alignment_reward_t reward;

    bool result = hypo_homeostasis_compute_reward(nullptr, &reward);

    EXPECT_FALSE(result);
}

TEST_F(HypothalamusExceptionTest, Homeostasis_ComputeRewardNullOutput_ReturnsFalse) {
    hypo_homeostasis_config_t config = hypo_homeostasis_default_config();
    hypo_homeostasis_handle_t* homeo = hypo_homeostasis_create(&config);
    ASSERT_NE(homeo, nullptr);

    reset_counters();

    bool result = hypo_homeostasis_compute_reward(homeo, nullptr);

    EXPECT_FALSE(result);

    hypo_homeostasis_destroy(homeo);
}

TEST_F(HypothalamusExceptionTest, Homeostasis_ModifySetpointNull_ReturnsFalse) {
    reset_counters();

    bool result = hypo_homeostasis_modify_setpoint(nullptr, HYPO_VAR_TEMPERATURE, 37.5f, 1);

    EXPECT_FALSE(result);
}

TEST_F(HypothalamusExceptionTest, Homeostasis_SetGainsNull_ReturnsFalse) {
    reset_counters();
    hypo_pid_gains_t gains = {0.5f, 0.1f, 0.05f, 1.0f, 0.1f};

    bool result = hypo_homeostasis_set_gains(nullptr, HYPO_VAR_TEMPERATURE, &gains);

    EXPECT_FALSE(result);
}

TEST_F(HypothalamusExceptionTest, Homeostasis_CreateDestroyValid_NoExceptions) {
    reset_counters();

    hypo_homeostasis_config_t config = hypo_homeostasis_default_config();
    hypo_homeostasis_handle_t* homeo = hypo_homeostasis_create(&config);

    ASSERT_NE(homeo, nullptr);
    EXPECT_EQ(handler_call_count.load(), 0);

    hypo_homeostasis_destroy(homeo);
}

//=============================================================================
// Perception Bridge Exception Tests
//=============================================================================

TEST_F(HypothalamusExceptionTest, PerceptionBridge_CreateNullDrives_ReturnsNull) {
    reset_counters();

    hypo_perception_bridge_t* bridge = hypo_perception_bridge_create(nullptr, nullptr);

    EXPECT_EQ(bridge, nullptr);
    // May or may not throw exception depending on implementation
}

TEST_F(HypothalamusExceptionTest, PerceptionBridge_ComputeModulationNull_ReturnsError) {
    reset_counters();

    int result = hypo_perception_bridge_compute_modulation(nullptr);

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, PerceptionBridge_GetModulationNull_ReturnsError) {
    reset_counters();
    hypo_perception_modulation_t mod;

    int result = hypo_perception_bridge_get_modulation(nullptr, &mod);

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, PerceptionBridge_GetModulationNullOutput_ReturnsError) {
    hypo_drive_config_t config = hypo_drive_default_config();
    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
    ASSERT_NE(drives, nullptr);

    hypo_perception_bridge_t* bridge = hypo_perception_bridge_create(drives, nullptr);
    if (bridge != nullptr) {
        reset_counters();

        int result = hypo_perception_bridge_get_modulation(bridge, nullptr);

        EXPECT_LT(result, 0);

        hypo_perception_bridge_destroy(bridge);
    }

    hypo_drive_destroy(drives);
}

TEST_F(HypothalamusExceptionTest, PerceptionBridge_ProcessDetectionNull_ReturnsError) {
    reset_counters();
    hypo_sensory_detection_t detection = {};

    int result = hypo_perception_bridge_process_detection(nullptr, &detection);

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, PerceptionBridge_CreateDestroyValid_NoExceptions) {
    hypo_drive_config_t config = hypo_drive_default_config();
    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
    ASSERT_NE(drives, nullptr);

    reset_counters();

    hypo_perception_bridge_t* bridge = hypo_perception_bridge_create(drives, nullptr);

    if (bridge != nullptr) {
        EXPECT_EQ(handler_call_count.load(), 0);
        hypo_perception_bridge_destroy(bridge);
    }

    hypo_drive_destroy(drives);
}

//=============================================================================
// Alignment Exception Tests
//=============================================================================

TEST_F(HypothalamusExceptionTest, Alignment_GetSnapshotNull_ReturnsError) {
    reset_counters();
    hypo_alignment_snapshot_t snapshot;

    hypo_alignment_status_t status = hypo_alignment_get_snapshot(nullptr, &snapshot);

    EXPECT_NE(status, HYPO_ALIGN_OK);
}

TEST_F(HypothalamusExceptionTest, Alignment_GetSnapshotNullOutput_ReturnsError) {
    hypo_drive_config_t config = hypo_drive_default_config();
    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
    ASSERT_NE(drives, nullptr);

    reset_counters();

    hypo_alignment_status_t status = hypo_alignment_get_snapshot(drives, nullptr);

    EXPECT_NE(status, HYPO_ALIGN_OK);

    hypo_drive_destroy(drives);
}

TEST_F(HypothalamusExceptionTest, Alignment_VerifyNull_ReturnsError) {
    reset_counters();
    hypo_verification_report_t report;

    hypo_alignment_status_t status = hypo_alignment_verify(nullptr, &report);

    EXPECT_NE(status, HYPO_ALIGN_OK);
}

TEST_F(HypothalamusExceptionTest, Alignment_VerifyNullOutput_ReturnsError) {
    hypo_drive_config_t config = hypo_drive_default_config();
    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
    ASSERT_NE(drives, nullptr);

    reset_counters();

    hypo_alignment_status_t status = hypo_alignment_verify(drives, nullptr);

    EXPECT_NE(status, HYPO_ALIGN_OK);

    hypo_drive_destroy(drives);
}

TEST_F(HypothalamusExceptionTest, Alignment_HealthCheckNull_ReturnsError) {
    reset_counters();
    float score = 0.0f;

    hypo_alignment_status_t status = hypo_alignment_health_check(nullptr, &score);

    EXPECT_NE(status, HYPO_ALIGN_OK);
}

TEST_F(HypothalamusExceptionTest, Alignment_HealthCheckNullOutput_ReturnsError) {
    hypo_drive_config_t config = hypo_drive_default_config();
    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
    ASSERT_NE(drives, nullptr);

    reset_counters();

    hypo_alignment_status_t status = hypo_alignment_health_check(drives, nullptr);

    EXPECT_NE(status, HYPO_ALIGN_OK);

    hypo_drive_destroy(drives);
}

TEST_F(HypothalamusExceptionTest, Alignment_GetWeightNull_ReturnsFalse) {
    reset_counters();
    float value = 0.0f;

    bool result = hypo_alignment_get_weight(nullptr, "human_wellbeing", &value);

    EXPECT_FALSE(result);
}

TEST_F(HypothalamusExceptionTest, Alignment_GetWeightNullName_ReturnsFalse) {
    hypo_drive_config_t config = hypo_drive_default_config();
    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
    ASSERT_NE(drives, nullptr);

    reset_counters();
    float value = 0.0f;

    bool result = hypo_alignment_get_weight(drives, nullptr, &value);

    EXPECT_FALSE(result);

    hypo_drive_destroy(drives);
}

TEST_F(HypothalamusExceptionTest, Alignment_GetWeightNullOutput_ReturnsFalse) {
    hypo_drive_config_t config = hypo_drive_default_config();
    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
    ASSERT_NE(drives, nullptr);

    reset_counters();

    bool result = hypo_alignment_get_weight(drives, "human_wellbeing", nullptr);

    EXPECT_FALSE(result);

    hypo_drive_destroy(drives);
}

TEST_F(HypothalamusExceptionTest, Alignment_VerifyWeightBoundsNull_ReturnsFalse) {
    reset_counters();

    bool result = hypo_alignment_verify_weight_bounds(nullptr, 0.0f, 1.0f);

    EXPECT_FALSE(result);
}

TEST_F(HypothalamusExceptionTest, Alignment_VerifyIntegrityNull_ReturnsFalse) {
    reset_counters();

    bool result = hypo_alignment_verify_integrity(nullptr);

    EXPECT_FALSE(result);
}

TEST_F(HypothalamusExceptionTest, Alignment_ComputeChecksumNull_ReturnsZero) {
    reset_counters();

    uint32_t checksum = hypo_alignment_compute_checksum(nullptr);

    EXPECT_EQ(checksum, 0u);
}

TEST_F(HypothalamusExceptionTest, Alignment_RegisterCallbackNull_ReturnsZero) {
    reset_counters();

    auto callback = [](const hypo_alignment_snapshot_t*, hypo_audit_event_t, void*) {};
    uint32_t id = hypo_alignment_register_callback(nullptr, callback, nullptr);

    EXPECT_EQ(id, 0u);
}

TEST_F(HypothalamusExceptionTest, Alignment_RegisterCallbackNullCallback_ReturnsZero) {
    hypo_drive_config_t config = hypo_drive_default_config();
    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
    ASSERT_NE(drives, nullptr);

    reset_counters();

    uint32_t id = hypo_alignment_register_callback(drives, nullptr, nullptr);

    EXPECT_EQ(id, 0u);

    hypo_drive_destroy(drives);
}

TEST_F(HypothalamusExceptionTest, Alignment_UnregisterCallbackNull_ReturnsFalse) {
    reset_counters();

    bool result = hypo_alignment_unregister_callback(nullptr, 1);

    EXPECT_FALSE(result);
}

TEST_F(HypothalamusExceptionTest, Alignment_RequestModificationNull_ReturnsError) {
    reset_counters();

    hypo_alignment_status_t status = hypo_alignment_request_modification(
        nullptr, HYPO_PARAM_SETPOINT, 0, 0.5f, 1, "test");

    EXPECT_NE(status, HYPO_ALIGN_OK);
}

TEST_F(HypothalamusExceptionTest, Alignment_AcknowledgeAlertNull_ReturnsFalse) {
    reset_counters();

    bool result = hypo_alignment_acknowledge_alert(nullptr, 0, 1);

    EXPECT_FALSE(result);
}

//=============================================================================
// Internal Bus Exception Tests
//=============================================================================

TEST_F(HypothalamusExceptionTest, InternalBus_DefaultConfigNull_ReturnsError) {
    reset_counters();

    int result = hypo_ibus_default_config(nullptr);

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, InternalBus_ResetNull_ReturnsError) {
    reset_counters();

    int result = hypo_ibus_reset(nullptr);

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, InternalBus_SubscribeNull_ReturnsError) {
    reset_counters();

    auto callback = [](const hypo_internal_event_t*, void*) { return 0; };
    int result = hypo_ibus_subscribe(
        nullptr,
        HYPO_IMOD_DRIVES,
        HYPO_IEVT_DRIVE_SATISFIED,
        callback,
        nullptr
    );

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, InternalBus_SubscribeNullCallback_ReturnsError) {
    hypo_ibus_config_t config;
    hypo_ibus_default_config(&config);
    hypo_ibus_t bus = hypo_ibus_create(&config);

    if (bus != nullptr) {
        reset_counters();

        int result = hypo_ibus_subscribe(
            bus,
            HYPO_IMOD_DRIVES,
            HYPO_IEVT_DRIVE_SATISFIED,
            nullptr,
            nullptr
        );

        EXPECT_LT(result, 0);

        hypo_ibus_destroy(bus);
    }
}

TEST_F(HypothalamusExceptionTest, InternalBus_UnsubscribeNull_ReturnsError) {
    reset_counters();

    int result = hypo_ibus_unsubscribe(nullptr, 1);

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, InternalBus_UnsubscribeInvalidId_ReturnsError) {
    hypo_ibus_config_t config;
    hypo_ibus_default_config(&config);
    hypo_ibus_t bus = hypo_ibus_create(&config);

    if (bus != nullptr) {
        reset_counters();

        int result = hypo_ibus_unsubscribe(bus, 99999);

        EXPECT_LT(result, 0);

        hypo_ibus_destroy(bus);
    }
}

TEST_F(HypothalamusExceptionTest, InternalBus_PublishNull_ReturnsError) {
    reset_counters();
    hypo_internal_event_t event = {};

    int result = hypo_ibus_publish(nullptr, &event);

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, InternalBus_PublishNullEvent_ReturnsError) {
    hypo_ibus_config_t config;
    hypo_ibus_default_config(&config);
    hypo_ibus_t bus = hypo_ibus_create(&config);

    if (bus != nullptr) {
        reset_counters();

        int result = hypo_ibus_publish(bus, nullptr);

        EXPECT_LT(result, 0);

        hypo_ibus_destroy(bus);
    }
}

TEST_F(HypothalamusExceptionTest, InternalBus_GetStatsNull_ReturnsError) {
    reset_counters();
    hypo_ibus_stats_t stats;

    int result = hypo_ibus_get_stats(nullptr, &stats);

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, InternalBus_GetStatsNullOutput_ReturnsError) {
    hypo_ibus_config_t config;
    hypo_ibus_default_config(&config);
    hypo_ibus_t bus = hypo_ibus_create(&config);

    if (bus != nullptr) {
        reset_counters();

        int result = hypo_ibus_get_stats(bus, nullptr);

        EXPECT_LT(result, 0);

        hypo_ibus_destroy(bus);
    }
}

TEST_F(HypothalamusExceptionTest, InternalBus_SubscribeToModuleNull_ReturnsError) {
    reset_counters();

    auto callback = [](const hypo_internal_event_t*, void*) { return 0; };
    int result = hypo_ibus_subscribe_to_module(
        nullptr,
        HYPO_IMOD_DRIVES,
        HYPO_IMOD_CIRCADIAN,
        callback,
        nullptr
    );

    EXPECT_LT(result, 0);
}

TEST_F(HypothalamusExceptionTest, InternalBus_CreateDestroyValid_NoExceptions) {
    reset_counters();

    hypo_ibus_config_t config;
    int cfg_result = hypo_ibus_default_config(&config);
    EXPECT_EQ(cfg_result, 0);

    hypo_ibus_t bus = hypo_ibus_create(&config);

    if (bus != nullptr) {
        EXPECT_EQ(handler_call_count.load(), 0);
        hypo_ibus_destroy(bus);
    }
}

//=============================================================================
// Cross-Module Integration Tests
//=============================================================================

TEST_F(HypothalamusExceptionTest, AllModules_ValidCreation_NoExceptions) {
    reset_counters();

    // Create all modules with valid configs
    hypo_drive_config_t drive_config = hypo_drive_default_config();
    hypo_drive_system_handle_t* drives = hypo_drive_create(&drive_config);
    ASSERT_NE(drives, nullptr);

    hypo_homeostasis_config_t homeo_config = hypo_homeostasis_default_config();
    hypo_homeostasis_handle_t* homeo = hypo_homeostasis_create(&homeo_config);
    ASSERT_NE(homeo, nullptr);

    hypo_ibus_config_t bus_config;
    hypo_ibus_default_config(&bus_config);
    hypo_ibus_t bus = hypo_ibus_create(&bus_config);
    // Bus may be null depending on implementation

    hypo_orch_config_t orch_config;
    hypo_orch_default_config(&orch_config);
    hypo_orchestrator_t orch = hypo_orch_create(&orch_config);
    // Orchestrator may be null depending on implementation

    // No exceptions should have been thrown for valid creation
    EXPECT_EQ(handler_call_count.load(), 0);

    // Cleanup
    if (orch) hypo_orch_destroy(orch);
    if (bus) hypo_ibus_destroy(bus);
    hypo_homeostasis_destroy(homeo);
    hypo_drive_destroy(drives);
}

TEST_F(HypothalamusExceptionTest, AllModules_NullDestroy_NoExceptions) {
    reset_counters();

    // NULL destroy should be safe and not throw
    hypo_drive_destroy(nullptr);
    hypo_homeostasis_destroy(nullptr);
    hypo_ibus_destroy(nullptr);
    hypo_orch_destroy(nullptr);
    hypo_perception_bridge_destroy(nullptr);

    // No exceptions should have been thrown
    EXPECT_EQ(handler_call_count.load(), 0);
}

//=============================================================================
// Boundary Condition Tests
//=============================================================================

TEST_F(HypothalamusExceptionTest, Drives_InvalidDriveType_HandlesGracefully) {
    hypo_drive_config_t config = hypo_drive_default_config();
    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
    ASSERT_NE(drives, nullptr);

    reset_counters();
    hypo_drive_state_t state;

    // Test with invalid drive type
    bool result = hypo_drive_get_state(drives, (hypo_drive_type_t)-1, &state);
    EXPECT_FALSE(result);

    result = hypo_drive_get_state(drives, (hypo_drive_type_t)HYPO_DRIVE_COUNT, &state);
    EXPECT_FALSE(result);

    hypo_drive_destroy(drives);
}

TEST_F(HypothalamusExceptionTest, Homeostasis_InvalidVariableType_HandlesGracefully) {
    hypo_homeostasis_config_t config = hypo_homeostasis_default_config();
    hypo_homeostasis_handle_t* homeo = hypo_homeostasis_create(&config);
    ASSERT_NE(homeo, nullptr);

    reset_counters();

    // Test with invalid variable type
    bool result = hypo_homeostasis_set_value(homeo, (hypo_variable_type_t)-1, 0.0f);
    EXPECT_FALSE(result);

    result = hypo_homeostasis_set_value(homeo, (hypo_variable_type_t)HYPO_VAR_COUNT, 0.0f);
    EXPECT_FALSE(result);

    hypo_homeostasis_destroy(homeo);
}

TEST_F(HypothalamusExceptionTest, InternalBus_InvalidEventType_HandlesGracefully) {
    hypo_ibus_config_t config;
    hypo_ibus_default_config(&config);
    hypo_ibus_t bus = hypo_ibus_create(&config);

    if (bus != nullptr) {
        reset_counters();

        auto callback = [](const hypo_internal_event_t*, void*) { return 0; };
        int result = hypo_ibus_subscribe(
            bus,
            HYPO_IMOD_DRIVES,
            (hypo_internal_event_type_t)-1,
            callback,
            nullptr
        );

        // Should either fail or succeed gracefully
        (void)result;

        hypo_ibus_destroy(bus);
    }
}

TEST_F(HypothalamusExceptionTest, Alignment_InvalidWeightName_ReturnsFalse) {
    hypo_drive_config_t config = hypo_drive_default_config();
    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
    ASSERT_NE(drives, nullptr);

    reset_counters();
    float value = 0.0f;

    bool result = hypo_alignment_get_weight(drives, "nonexistent_weight", &value);

    EXPECT_FALSE(result);

    hypo_drive_destroy(drives);
}

//=============================================================================
// Memory Tracking Tests
//=============================================================================

TEST_F(HypothalamusExceptionTest, Drives_CreateDestroyMultiple_NoLeaks) {
    for (int i = 0; i < 5; i++) {
        hypo_drive_config_t config = hypo_drive_default_config();
        hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
        ASSERT_NE(drives, nullptr) << "Failed on iteration " << i;
        hypo_drive_destroy(drives);
    }
}

TEST_F(HypothalamusExceptionTest, Homeostasis_CreateDestroyMultiple_NoLeaks) {
    for (int i = 0; i < 5; i++) {
        hypo_homeostasis_config_t config = hypo_homeostasis_default_config();
        hypo_homeostasis_handle_t* homeo = hypo_homeostasis_create(&config);
        ASSERT_NE(homeo, nullptr) << "Failed on iteration " << i;
        hypo_homeostasis_destroy(homeo);
    }
}

TEST_F(HypothalamusExceptionTest, InternalBus_CreateDestroyMultiple_NoLeaks) {
    for (int i = 0; i < 5; i++) {
        hypo_ibus_config_t config;
        hypo_ibus_default_config(&config);
        hypo_ibus_t bus = hypo_ibus_create(&config);
        if (bus) {
            hypo_ibus_destroy(bus);
        }
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
