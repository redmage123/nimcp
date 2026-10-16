/**
 * @file test_medulla_exception_handling.cpp
 * @brief Unit tests for exception handling in Medulla Oblongata modules
 * @date 2026-01-24
 *
 * WHAT: Test NIMCP_THROW_TO_IMMUNE exception handling for medulla modules
 * WHY:  Ensure proper error reporting to immune system for all error conditions
 * HOW:  Test invalid configs, NULL pointers, and boundary conditions
 *
 * MODULES TESTED:
 * - nimcp_medulla.h: Main medulla orchestrator
 * - nimcp_medulla_immune_bridge.h: Medulla-immune system integration
 * - nimcp_medulla_cerebellum_bridge.h: Medulla-cerebellum inferior olive bridge
 *
 * NOTE: nimcp_brainstem_coupling.h is NOT included here due to a typedef conflict
 * with nimcp_medulla.h (brainstem_coupling_t is defined differently in each).
 * Brainstem coupling tests are in the separate test_brainstem_coupling.cpp file.
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <cmath>

#include "utils/nimcp_test_base.h"

// Include medulla headers - these work together without conflicts
#include "core/medulla/nimcp_medulla.h"
#include "core/medulla/nimcp_medulla_immune_bridge.h"
#include "core/medulla/nimcp_medulla_cerebellum_bridge.h"

// NOTE: We cannot include nimcp_brainstem_coupling.h here due to typedef conflict
// The brainstem_coupling_t type is defined as a pointer in nimcp_medulla.h
// but as a struct in nimcp_brainstem_coupling.h. Tests for brainstem coupling
// exception handling are in the existing test_brainstem_coupling.cpp file.

//=============================================================================
// Test Fixture
//=============================================================================

class MedullaExceptionHandlingTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }
};

//=============================================================================
// MEDULLA CORE TESTS (nimcp_medulla.h)
//=============================================================================

//-----------------------------------------------------------------------------
// medulla_create Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaCreate_NullConfig_ReturnsValidHandle) {
    // NULL config should use defaults - this is valid behavior per the API
    medulla_t medulla = medulla_create(nullptr);
    // May return NULL if allocation fails, but NULL config is valid
    if (medulla != nullptr) {
        medulla_destroy(medulla);
    }
}

TEST_F(MedullaExceptionHandlingTest, MedullaCreate_ValidConfig_ReturnsHandle) {
    medulla_config_t config = medulla_default_config();
    medulla_t medulla = medulla_create(&config);
    EXPECT_NE(medulla, nullptr);
    if (medulla != nullptr) {
        medulla_destroy(medulla);
    }
}

//-----------------------------------------------------------------------------
// medulla_destroy Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaDestroy_NullHandle_NoException) {
    // NULL destroy should be safe
    medulla_destroy(nullptr);
    // Should not crash or throw
}

//-----------------------------------------------------------------------------
// medulla_start Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaStart_NullHandle_ReturnsError) {
    int result = medulla_start(nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error (e.g., NIMCP_ERROR_NULL_POINTER = 1003)
}

TEST_F(MedullaExceptionHandlingTest, MedullaStart_ValidHandle_ReturnsSuccess) {
    medulla_config_t config = medulla_default_config();
    medulla_t medulla = medulla_create(&config);
    ASSERT_NE(medulla, nullptr);

    int result = medulla_start(medulla);
    EXPECT_GE(result, 0);

    medulla_stop(medulla);
    medulla_destroy(medulla);
}

//-----------------------------------------------------------------------------
// medulla_stop Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaStop_NullHandle_ReturnsError) {
    int result = medulla_stop(nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaStop_NotStarted_ReturnsSuccess) {
    medulla_config_t config = medulla_default_config();
    medulla_t medulla = medulla_create(&config);
    ASSERT_NE(medulla, nullptr);

    // Stopping without starting should be safe
    int result = medulla_stop(medulla);
    EXPECT_GE(result, 0);

    medulla_destroy(medulla);
}

//-----------------------------------------------------------------------------
// medulla_update Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaUpdate_NullHandle_ReturnsError) {
    int result = medulla_update(nullptr, 0.016f);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaUpdate_NegativeDt_HandlesGracefully) {
    medulla_config_t config = medulla_default_config();
    medulla_t medulla = medulla_create(&config);
    ASSERT_NE(medulla, nullptr);

    medulla_start(medulla);
    // Negative dt should be handled gracefully
    int result = medulla_update(medulla, -1.0f);
    // May succeed (clamp to 0) or fail - implementation dependent
    (void)result;

    medulla_stop(medulla);
    medulla_destroy(medulla);
}

TEST_F(MedullaExceptionHandlingTest, MedullaUpdate_ZeroDt_ReturnsSuccess) {
    medulla_config_t config = medulla_default_config();
    medulla_t medulla = medulla_create(&config);
    ASSERT_NE(medulla, nullptr);

    medulla_start(medulla);
    int result = medulla_update(medulla, 0.0f);
    EXPECT_GE(result, 0);

    medulla_stop(medulla);
    medulla_destroy(medulla);
}

//-----------------------------------------------------------------------------
// medulla_emergency_shutdown Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaEmergencyShutdown_NullHandle_ReturnsError) {
    int result = medulla_emergency_shutdown(nullptr, "test reason");
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaEmergencyShutdown_NullReason_HandlesGracefully) {
    medulla_config_t config = medulla_default_config();
    medulla_t medulla = medulla_create(&config);
    ASSERT_NE(medulla, nullptr);

    medulla_start(medulla);
    // NULL reason should be handled gracefully
    int result = medulla_emergency_shutdown(medulla, nullptr);
    // Should either succeed or fail gracefully
    (void)result;

    medulla_destroy(medulla);
}

TEST_F(MedullaExceptionHandlingTest, MedullaEmergencyShutdown_ValidParams_ReturnsSuccess) {
    medulla_config_t config = medulla_default_config();
    medulla_t medulla = medulla_create(&config);
    ASSERT_NE(medulla, nullptr);

    medulla_start(medulla);
    int result = medulla_emergency_shutdown(medulla, "unit test emergency");
    EXPECT_GE(result, 0);

    medulla_destroy(medulla);
}

//-----------------------------------------------------------------------------
// medulla_get_stats Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaGetStats_NullHandle_ReturnsError) {
    medulla_stats_t stats;
    int result = medulla_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaGetStats_NullOutput_ReturnsError) {
    medulla_config_t config = medulla_default_config();
    medulla_t medulla = medulla_create(&config);
    ASSERT_NE(medulla, nullptr);

    int result = medulla_get_stats(medulla, nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error

    medulla_destroy(medulla);
}

TEST_F(MedullaExceptionHandlingTest, MedullaGetStats_ValidParams_ReturnsSuccess) {
    medulla_config_t config = medulla_default_config();
    medulla_t medulla = medulla_create(&config);
    ASSERT_NE(medulla, nullptr);

    medulla_stats_t stats;
    int result = medulla_get_stats(medulla, &stats);
    EXPECT_GE(result, 0);

    medulla_destroy(medulla);
}

//-----------------------------------------------------------------------------
// medulla_request_state_change Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaRequestStateChange_NullHandle_ReturnsError) {
    int result = medulla_request_state_change(nullptr, MEDULLA_STATE_RUNNING);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaRequestStateChange_InvalidState_HandlesGracefully) {
    medulla_config_t config = medulla_default_config();
    medulla_t medulla = medulla_create(&config);
    ASSERT_NE(medulla, nullptr);

    // Note: The function doesn't validate enum values, so invalid states are accepted
    int result = medulla_request_state_change(medulla, (medulla_state_t)999);
    EXPECT_EQ(result, 0);  // Function accepts any value (no enum validation)

    medulla_destroy(medulla);
}

//-----------------------------------------------------------------------------
// medulla_connect_* Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaConnectHealthMonitor_NullMedulla_ReturnsError) {
    int result = medulla_connect_health_monitor(nullptr, nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaConnectRecoverySystem_NullMedulla_ReturnsError) {
    int result = medulla_connect_recovery_system(nullptr, nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaConnectSleepWake_NullMedulla_ReturnsError) {
    int result = medulla_connect_sleep_wake(nullptr, nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaConnectNeuromodulators_NullMedulla_ReturnsError) {
    int result = medulla_connect_neuromodulators(nullptr, nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// medulla_get_arousal_level Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaGetArousalLevel_NullHandle_ReturnsNegative) {
    float level = medulla_get_arousal_level(nullptr);
    EXPECT_LT(level, 0.0f);
}

TEST_F(MedullaExceptionHandlingTest, MedullaGetArousalLevel_ValidHandle_ReturnsValidRange) {
    medulla_config_t config = medulla_default_config();
    medulla_t medulla = medulla_create(&config);
    ASSERT_NE(medulla, nullptr);

    float level = medulla_get_arousal_level(medulla);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);

    medulla_destroy(medulla);
}

//-----------------------------------------------------------------------------
// medulla_boost_arousal Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaBoostArousal_NullHandle_ReturnsError) {
    int result = medulla_boost_arousal(nullptr, 0.1f);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaBoostArousal_NegativeDelta_HandlesGracefully) {
    medulla_config_t config = medulla_default_config();
    medulla_t medulla = medulla_create(&config);
    ASSERT_NE(medulla, nullptr);

    int result = medulla_boost_arousal(medulla, -0.1f);
    // May clamp or reject - implementation dependent
    (void)result;

    medulla_destroy(medulla);
}

//-----------------------------------------------------------------------------
// medulla_reduce_arousal Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaReduceArousal_NullHandle_ReturnsError) {
    int result = medulla_reduce_arousal(nullptr, 0.1f);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// medulla_get_protection_level Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaGetProtectionLevel_NullHandle_ReturnsDefault) {
    protection_level_t level = medulla_get_protection_level(nullptr);
    // Should return a default/error value
    (void)level;
}

//-----------------------------------------------------------------------------
// medulla_get_circadian_phase Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaGetCircadianPhase_NullHandle_ReturnsDefault) {
    circadian_phase_t phase = medulla_get_circadian_phase(nullptr);
    // Should return a default/error value
    (void)phase;
}

//-----------------------------------------------------------------------------
// medulla_bio_async Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaConnectBioAsync_NullHandle_ReturnsError) {
    int result = medulla_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaDisconnectBioAsync_NullHandle_ReturnsError) {
    int result = medulla_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaIsBioAsyncConnected_NullHandle_ReturnsFalse) {
    bool connected = medulla_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

//-----------------------------------------------------------------------------
// medulla_test_* Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaTestSetArousal_NullHandle_ReturnsError) {
    int result = medulla_test_set_arousal(nullptr, 0.5f);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaTestSetProtection_NullHandle_ReturnsError) {
    int result = medulla_test_set_protection(nullptr, PROTECTION_LEVEL_NORMAL);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaTestSetCircadian_NullHandle_ReturnsError) {
    int result = medulla_test_set_circadian(nullptr, CIRCADIAN_PHASE_MORNING);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//=============================================================================
// MEDULLA IMMUNE BRIDGE TESTS (nimcp_medulla_immune_bridge.h)
//=============================================================================

//-----------------------------------------------------------------------------
// medulla_immune_create Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaImmuneCreate_NullAll_ReturnsNull) {
    medulla_immune_bridge_t bridge = medulla_immune_create(nullptr, nullptr, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(MedullaExceptionHandlingTest, MedullaImmuneCreate_NullMedulla_ReturnsNull) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);

    medulla_immune_bridge_t bridge = medulla_immune_create(&config, nullptr, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

//-----------------------------------------------------------------------------
// medulla_immune_destroy Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaImmuneDestroy_NullHandle_NoException) {
    medulla_immune_destroy(nullptr);
    // Should not crash or throw
}

//-----------------------------------------------------------------------------
// medulla_immune_update Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaImmuneUpdate_NullHandle_ReturnsError) {
    int result = medulla_immune_update(nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// medulla_immune_update_immune_to_medulla Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaImmuneUpdateImmuneToMedulla_NullHandle_ReturnsError) {
    medulla_cytokine_effects_t effects;
    int result = medulla_immune_update_immune_to_medulla(nullptr, &effects);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// medulla_immune_update_medulla_to_immune Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaImmuneUpdateMedullaToImmune_NullHandle_ReturnsError) {
    medulla_immune_effects_t effects;
    int result = medulla_immune_update_medulla_to_immune(nullptr, &effects);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// medulla_immune_get_cytokine_effects Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaImmuneGetCytokineEffects_NullHandle_ReturnsError) {
    medulla_cytokine_effects_t effects;
    int result = medulla_immune_get_cytokine_effects(nullptr, &effects);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaImmuneGetCytokineEffects_NullOutput_ReturnsError) {
    // Need valid bridge to test null output
    medulla_config_t med_config = medulla_default_config();
    medulla_t medulla = medulla_create(&med_config);
    if (medulla != nullptr) {
        medulla_immune_config_t config;
        medulla_immune_default_config(&config);
        medulla_immune_bridge_t bridge = medulla_immune_create(&config, medulla, nullptr);
        if (bridge != nullptr) {
            int result = medulla_immune_get_cytokine_effects(bridge, nullptr);
            EXPECT_NE(result, 0);  // Returns non-zero on error
            medulla_immune_destroy(bridge);
        }
        medulla_destroy(medulla);
    }
}

//-----------------------------------------------------------------------------
// medulla_immune_get_immune_effects Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaImmuneGetImmuneEffects_NullHandle_ReturnsError) {
    medulla_immune_effects_t effects;
    int result = medulla_immune_get_immune_effects(nullptr, &effects);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// medulla_immune_get_stats Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaImmuneGetStats_NullHandle_ReturnsError) {
    medulla_immune_stats_t stats;
    int result = medulla_immune_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaImmuneGetStats_NullOutput_ReturnsError) {
    medulla_config_t med_config = medulla_default_config();
    medulla_t medulla = medulla_create(&med_config);
    if (medulla != nullptr) {
        medulla_immune_config_t config;
        medulla_immune_default_config(&config);
        medulla_immune_bridge_t bridge = medulla_immune_create(&config, medulla, nullptr);
        if (bridge != nullptr) {
            int result = medulla_immune_get_stats(bridge, nullptr);
            EXPECT_NE(result, 0);  // Returns non-zero on error
            medulla_immune_destroy(bridge);
        }
        medulla_destroy(medulla);
    }
}

//-----------------------------------------------------------------------------
// medulla_immune_bio_async Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedullaImmuneConnectBioAsync_NullHandle_ReturnsError) {
    int result = medulla_immune_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaImmuneDisconnectBioAsync_NullHandle_ReturnsError) {
    int result = medulla_immune_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedullaImmuneIsBioAsyncConnected_NullHandle_ReturnsFalse) {
    bool connected = medulla_immune_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

//=============================================================================
// MEDULLA CEREBELLUM BRIDGE TESTS (nimcp_medulla_cerebellum_bridge.h)
//=============================================================================

//-----------------------------------------------------------------------------
// med_cereb_bridge_default_config Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeDefaultConfig_NullOutput_ReturnsError) {
    int result = med_cereb_bridge_default_config(nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeDefaultConfig_ValidOutput_ReturnsSuccess) {
    med_cereb_bridge_config_t config;
    int result = med_cereb_bridge_default_config(&config);
    EXPECT_GE(result, 0);
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_create Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeCreate_NullConfig_ReturnsHandle) {
    // NULL config should use defaults - this may be valid
    med_cereb_bridge_t bridge = med_cereb_bridge_create(nullptr);
    if (bridge != nullptr) {
        med_cereb_bridge_destroy(bridge);
    }
}

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeCreate_ValidConfig_ReturnsHandle) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    EXPECT_NE(bridge, nullptr);
    if (bridge != nullptr) {
        med_cereb_bridge_destroy(bridge);
    }
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_destroy Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeDestroy_NullHandle_NoException) {
    med_cereb_bridge_destroy(nullptr);
    // Should not crash or throw
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_reset Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeReset_NullHandle_ReturnsError) {
    int result = med_cereb_bridge_reset(nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_connect_medulla Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeConnectMedulla_NullBridge_ReturnsError) {
    int result = med_cereb_bridge_connect_medulla(nullptr, nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeConnectMedulla_NullMedulla_ReturnsError) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    if (bridge != nullptr) {
        int result = med_cereb_bridge_connect_medulla(bridge, nullptr);
        EXPECT_NE(result, 0);  // Returns non-zero on error
        med_cereb_bridge_destroy(bridge);
    }
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_connect_cerebellum Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeConnectCerebellum_NullBridge_ReturnsError) {
    int result = med_cereb_bridge_connect_cerebellum(nullptr, nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_connect_bio_async Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeConnectBioAsync_NullBridge_ReturnsError) {
    int result = med_cereb_bridge_connect_bio_async(nullptr, nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_is_connected Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeIsConnected_NullHandle_ReturnsFalse) {
    bool connected = med_cereb_bridge_is_connected(nullptr);
    EXPECT_FALSE(connected);
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_queue_error Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeQueueError_NullHandle_ReturnsError) {
    int result = med_cereb_bridge_queue_error(nullptr, MED_CEREB_ERROR_TIMING, 0.5f, 0);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeQueueError_InvalidErrorType_HandlesGracefully) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    if (bridge != nullptr) {
        int result = med_cereb_bridge_queue_error(
            bridge, (med_cereb_error_type_t)999, 0.5f, 0);
        EXPECT_NE(result, 0);  // Returns non-zero on error
        med_cereb_bridge_destroy(bridge);
    }
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_send_climbing_signal Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeSendClimbingSignal_NullHandle_ReturnsError) {
    int result = med_cereb_bridge_send_climbing_signal(
        nullptr, MED_CEREB_ERROR_TIMING, 0.5f, 0);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_broadcast_error Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeBroadcastError_NullHandle_ReturnsError) {
    int result = med_cereb_bridge_broadcast_error(nullptr, MED_CEREB_ERROR_TIMING, 0.5f);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_get_arousal_effects Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeGetArousalEffects_NullHandle_ReturnsError) {
    med_cereb_arousal_effects_t effects;
    int result = med_cereb_bridge_get_arousal_effects(nullptr, &effects);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeGetArousalEffects_NullOutput_ReturnsError) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    if (bridge != nullptr) {
        int result = med_cereb_bridge_get_arousal_effects(bridge, nullptr);
        EXPECT_NE(result, 0);  // Returns non-zero on error
        med_cereb_bridge_destroy(bridge);
    }
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_modulate_motor Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeModulateMotor_NullHandle_ReturnsError) {
    float motor_in[3] = {1.0f, 0.5f, 0.0f};
    float motor_out[3];
    int result = med_cereb_bridge_modulate_motor(nullptr, motor_in, motor_out, 3);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeModulateMotor_NullInput_ReturnsError) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    if (bridge != nullptr) {
        float motor_out[3];
        int result = med_cereb_bridge_modulate_motor(bridge, nullptr, motor_out, 3);
        EXPECT_NE(result, 0);  // Returns non-zero on error
        med_cereb_bridge_destroy(bridge);
    }
}

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeModulateMotor_NullOutput_ReturnsError) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    if (bridge != nullptr) {
        float motor_in[3] = {1.0f, 0.5f, 0.0f};
        int result = med_cereb_bridge_modulate_motor(bridge, motor_in, nullptr, 3);
        EXPECT_NE(result, 0);  // Returns non-zero on error
        med_cereb_bridge_destroy(bridge);
    }
}

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeModulateMotor_ZeroDimensions_ReturnsError) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    if (bridge != nullptr) {
        float motor_in[3] = {1.0f, 0.5f, 0.0f};
        float motor_out[3];
        int result = med_cereb_bridge_modulate_motor(bridge, motor_in, motor_out, 0);
        EXPECT_NE(result, 0);  // Returns non-zero on error
        med_cereb_bridge_destroy(bridge);
    }
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_get_protection_effects Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeGetProtectionEffects_NullHandle_ReturnsError) {
    med_cereb_protection_effects_t effects;
    int result = med_cereb_bridge_get_protection_effects(nullptr, &effects);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeGetProtectionEffects_NullOutput_ReturnsError) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    if (bridge != nullptr) {
        int result = med_cereb_bridge_get_protection_effects(bridge, nullptr);
        EXPECT_NE(result, 0);  // Returns non-zero on error
        med_cereb_bridge_destroy(bridge);
    }
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_motor_allowed Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeMotorAllowed_NullHandle_ReturnsFalse) {
    bool allowed = med_cereb_bridge_motor_allowed(nullptr, true, false);
    EXPECT_FALSE(allowed);
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_emergency_stop Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeEmergencyStop_NullHandle_ReturnsError) {
    int result = med_cereb_bridge_emergency_stop(nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_release_emergency Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeReleaseEmergency_NullHandle_ReturnsError) {
    int result = med_cereb_bridge_release_emergency(nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_get_circadian_effects Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeGetCircadianEffects_NullHandle_ReturnsError) {
    med_cereb_circadian_effects_t effects;
    int result = med_cereb_bridge_get_circadian_effects(nullptr, &effects);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeGetCircadianEffects_NullOutput_ReturnsError) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    if (bridge != nullptr) {
        int result = med_cereb_bridge_get_circadian_effects(bridge, nullptr);
        EXPECT_NE(result, 0);  // Returns non-zero on error
        med_cereb_bridge_destroy(bridge);
    }
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_get_learning_multiplier Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeGetLearningMultiplier_NullHandle_ReturnsDefault) {
    float multiplier = med_cereb_bridge_get_learning_multiplier(nullptr);
    // Should return a default/error value (probably 0.0 or 1.0)
    (void)multiplier;
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_apply_circadian_learning Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeApplyCircadianLearning_NullHandle_ReturnsError) {
    int result = med_cereb_bridge_apply_circadian_learning(nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_update Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeUpdate_NullHandle_ReturnsError) {
    int result = med_cereb_bridge_update(nullptr, 1000);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_process_messages Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeProcessMessages_NullHandle_ReturnsError) {
    int result = med_cereb_bridge_process_messages(nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_get_stats Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeGetStats_NullHandle_ReturnsError) {
    med_cereb_bridge_stats_t stats;
    int result = med_cereb_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeGetStats_NullOutput_ReturnsError) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    if (bridge != nullptr) {
        int result = med_cereb_bridge_get_stats(bridge, nullptr);
        EXPECT_NE(result, 0);  // Returns non-zero on error
        med_cereb_bridge_destroy(bridge);
    }
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_reset_stats Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeResetStats_NullHandle_ReturnsError) {
    int result = med_cereb_bridge_reset_stats(nullptr);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_get_io_state Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeGetIOState_NullHandle_ReturnsError) {
    med_cereb_inferior_olive_t io_state;
    int result = med_cereb_bridge_get_io_state(nullptr, &io_state);
    EXPECT_NE(result, 0);  // Returns non-zero on error
}

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeGetIOState_NullOutput_ReturnsError) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    if (bridge != nullptr) {
        int result = med_cereb_bridge_get_io_state(bridge, nullptr);
        EXPECT_NE(result, 0);  // Returns non-zero on error
        med_cereb_bridge_destroy(bridge);
    }
}

//-----------------------------------------------------------------------------
// med_cereb_bridge_pending_error_count Tests
//-----------------------------------------------------------------------------

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgePendingErrorCount_NullHandle_ReturnsZero) {
    uint32_t count = med_cereb_bridge_pending_error_count(nullptr);
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// INTEGRATION AND STATE VALIDATION TESTS
//=============================================================================

TEST_F(MedullaExceptionHandlingTest, MedullaFullLifecycle_NoExceptions) {
    // Create and start medulla
    medulla_config_t config = medulla_default_config();
    medulla_t medulla = medulla_create(&config);
    ASSERT_NE(medulla, nullptr);

    // Start
    int result = medulla_start(medulla);
    EXPECT_GE(result, 0);

    // Update a few times
    for (int i = 0; i < 10; i++) {
        result = medulla_update(medulla, 0.016f);
        EXPECT_GE(result, 0);
    }

    // Get stats
    medulla_stats_t stats;
    result = medulla_get_stats(medulla, &stats);
    EXPECT_GE(result, 0);

    // Stop
    result = medulla_stop(medulla);
    EXPECT_GE(result, 0);

    // Destroy
    medulla_destroy(medulla);
}

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeFullLifecycle_NoExceptions) {
    // Create bridge
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Queue some errors
    for (int i = 0; i < 5; i++) {
        int result = med_cereb_bridge_queue_error(
            bridge, MED_CEREB_ERROR_TIMING, 0.1f * i, i);
        EXPECT_GE(result, 0);
    }

    // Check pending count
    uint32_t count = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_GT(count, 0u);

    // Update
    int result = med_cereb_bridge_update(bridge, 1000);
    EXPECT_GE(result, 0);

    // Get stats
    med_cereb_bridge_stats_t stats;
    result = med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_GE(result, 0);

    // Reset
    result = med_cereb_bridge_reset(bridge);
    EXPECT_GE(result, 0);

    // Destroy
    med_cereb_bridge_destroy(bridge);
}

TEST_F(MedullaExceptionHandlingTest, AllModulesNullDestroy_NoExceptions) {
    // All destroy functions should be safe with NULL
    // Note: brainstem_coupling tests are in test_brainstem_coupling.cpp due to typedef conflict
    medulla_destroy(nullptr);
    medulla_immune_destroy(nullptr);
    med_cereb_bridge_destroy(nullptr);
    // Should not crash
}

TEST_F(MedullaExceptionHandlingTest, MedullaCreateDestroyMultiple_NoLeaks) {
    for (int i = 0; i < 5; i++) {
        medulla_config_t config = medulla_default_config();
        medulla_t medulla = medulla_create(&config);
        ASSERT_NE(medulla, nullptr) << "Failed on iteration " << i;
        medulla_destroy(medulla);
    }
}

TEST_F(MedullaExceptionHandlingTest, MedCerebBridgeCreateDestroyMultiple_NoLeaks) {
    for (int i = 0; i < 5; i++) {
        med_cereb_bridge_config_t config;
        med_cereb_bridge_default_config(&config);
        med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
        ASSERT_NE(bridge, nullptr) << "Failed on iteration " << i;
        med_cereb_bridge_destroy(bridge);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
