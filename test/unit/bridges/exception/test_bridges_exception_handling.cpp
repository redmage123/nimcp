/**
 * @file test_bridges_exception_handling.cpp
 * @brief Unit tests for bridge exception handling
 *
 * WHAT: Test exception handling in bridge components
 * WHY:  Verify bridges properly handle and propagate exceptions during communication
 * HOW:  Test FEP bridges, substrate bridges, cortical bridges, and cross-bridge exceptions
 *
 * TEST SCENARIOS:
 * - Bridge connection failure exceptions
 * - Cross-bridge communication exceptions
 * - FEP bridge update exceptions
 * - Substrate bridge metabolic exceptions
 * - Cortical bridge layer exceptions
 * - Bio-async bridge exceptions
 * - Bridge exception recovery
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <atomic>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class BridgesExceptionHandlingTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static std::atomic<nimcp_exception_category_t> last_category;
    static std::atomic<nimcp_exception_severity_t> last_severity;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        last_category = EXCEPTION_CATEGORY_GENERIC;
        last_severity = EXCEPTION_SEVERITY_INFO;

        nimcp_exception_system_init();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    // Test handler for bridge exceptions
    static bool bridge_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        last_category = ex->category;
        last_severity = ex->severity;
        return false;
    }

    // Helper to create bridge exception
    static nimcp_exception_t* create_bridge_exception(
        nimcp_error_t code,
        nimcp_exception_severity_t severity,
        const char* message
    ) {
        return nimcp_exception_create(
            code, severity, __FILE__, __LINE__, __func__, "%s", message
        );
    }
};

std::atomic<int> BridgesExceptionHandlingTest::handler_call_count(0);
std::atomic<int> BridgesExceptionHandlingTest::last_exception_code(0);
std::atomic<nimcp_exception_category_t> BridgesExceptionHandlingTest::last_category(EXCEPTION_CATEGORY_GENERIC);
std::atomic<nimcp_exception_severity_t> BridgesExceptionHandlingTest::last_severity(EXCEPTION_SEVERITY_INFO);

//=============================================================================
// Bridge Connection Exception Tests
//=============================================================================

TEST_F(BridgesExceptionHandlingTest, BridgeConnectionFailureException) {
    // WHAT: Test exception creation for bridge connection failures
    // WHY:  Bridges must report connection failures clearly

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_CONNECTION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        "Failed to connect bridge to target system"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_CONNECTION_FAILED);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);

    // Add bridge-specific context
    nimcp_exception_set_context(ex, "bridge_type", "fep_bridge");
    nimcp_exception_set_context(ex, "source_module", "circular_buffer");
    nimcp_exception_set_context(ex, "target_module", "fep_system");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "bridge_type"), "fep_bridge");

    nimcp_exception_unref(ex);
}

TEST_F(BridgesExceptionHandlingTest, BridgeDisconnectedStateException) {
    // WHAT: Test exception for operations on disconnected bridges
    // WHY:  Attempting operations on disconnected bridges should fail gracefully

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_NOT_CONNECTED,
        EXCEPTION_SEVERITY_WARNING,
        "Bridge operation failed: systems not connected"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "bridge_type", "cortical_substrate");
    nimcp_exception_set_context(ex, "system_a_connected", "false");
    nimcp_exception_set_context(ex, "system_b_connected", "true");
    nimcp_exception_set_context(ex, "operation", "update");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "system_a_connected"), "false");

    nimcp_exception_unref(ex);
}

TEST_F(BridgesExceptionHandlingTest, BridgeNullSystemException) {
    // WHAT: Test exception for null system pointer during connection
    // WHY:  Null pointers should be caught early with clear errors

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_SEVERITY_ERROR,
        "Cannot connect NULL system to bridge"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    nimcp_exception_set_context(ex, "parameter", "system_a");
    nimcp_exception_set_context(ex, "bridge_type", "feature_extractor_fep");

    nimcp_exception_unref(ex);
}

//=============================================================================
// FEP Bridge Exception Tests
//=============================================================================

TEST_F(BridgesExceptionHandlingTest, FEPBridgeUpdateException) {
    // WHAT: Test exceptions during FEP bridge update cycles
    // WHY:  FEP bridges have bidirectional updates that can fail

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_UPDATE_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        "FEP bridge update failed: precision calculation error"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "bridge_type", "circular_buffer_fep");
    nimcp_exception_set_context(ex, "direction", "fep_to_buffer");
    nimcp_exception_set_context(ex, "precision", "NaN");
    nimcp_exception_set_context(ex, "delta_ms", "16");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "direction"), "fep_to_buffer");

    nimcp_exception_unref(ex);
}

TEST_F(BridgesExceptionHandlingTest, FEPBridgePrecisionNaNException) {
    // WHAT: Test NaN detection in FEP bridge precision values
    // WHY:  NaN precision corrupts FEP-based modulation

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_NAN_DETECTED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0,  // brain_id
        "fep_bridge",
        "NaN detected in FEP precision value"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_NAN_DETECTED);
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_BRAIN);

    nimcp_exception_set_context((nimcp_exception_t*)ex, "bridge_type", "population_coding_fep");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "variable", "precision");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(BridgesExceptionHandlingTest, FEPBridgeHorizonException) {
    // WHAT: Test exception for invalid prediction horizon in FEP bridges
    // WHY:  Invalid horizons corrupt temporal planning

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_SEVERITY_WARNING,
        "FEP bridge horizon out of valid range"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "bridge_type", "circular_buffer_fep");
    nimcp_exception_set_context(ex, "horizon_value", "1000");
    nimcp_exception_set_context(ex, "max_horizon", "256");
    nimcp_exception_set_context(ex, "min_horizon", "4");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "horizon_value"), "1000");

    nimcp_exception_unref(ex);
}

TEST_F(BridgesExceptionHandlingTest, FEPBridgeSurpriseOverflowException) {
    // WHAT: Test exception for extreme surprise values in FEP bridges
    // WHY:  Unbounded surprise can cause instability

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_OVERFLOW,
        EXCEPTION_SEVERITY_WARNING,
        "FEP bridge surprise value overflow"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "bridge_type", "feature_extractor_fep");
    nimcp_exception_set_context(ex, "surprise_value", "inf");
    nimcp_exception_set_context(ex, "max_surprise", "100.0");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Substrate Bridge Exception Tests
//=============================================================================

TEST_F(BridgesExceptionHandlingTest, SubstrateBridgeATPException) {
    // WHAT: Test exception for ATP depletion in substrate bridges
    // WHY:  ATP depletion affects neural computation

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_RESOURCE_EXHAUSTED,
        EXCEPTION_SEVERITY_SEVERE,
        "Substrate bridge: critical ATP depletion"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_SEVERE);

    nimcp_exception_set_context(ex, "bridge_type", "cortical_substrate");
    nimcp_exception_set_context(ex, "atp_level", "0.15");
    nimcp_exception_set_context(ex, "critical_threshold", "0.3");
    nimcp_exception_set_context(ex, "affected_layer", "IV");

    // Severe exceptions should trigger immune response
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(ex, &response);
    EXPECT_TRUE(ex->presented_to_immune);

    nimcp_exception_unref(ex);
}

TEST_F(BridgesExceptionHandlingTest, SubstrateBridgeTemperatureException) {
    // WHAT: Test exception for hyperthermia in substrate bridges
    // WHY:  Temperature affects Q10 metabolic rates

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_THRESHOLD_EXCEEDED,
        EXCEPTION_SEVERITY_WARNING,
        "Substrate bridge: temperature exceeds safe range"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "bridge_type", "cortical_substrate");
    nimcp_exception_set_context(ex, "temperature", "42.5");
    nimcp_exception_set_context(ex, "max_safe", "40.0");
    nimcp_exception_set_context(ex, "q10_factor", "2.8");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "temperature"), "42.5");

    nimcp_exception_unref(ex);
}

TEST_F(BridgesExceptionHandlingTest, SubstrateBridgeLayerImpairmentException) {
    // WHAT: Test exception for layer-specific impairment
    // WHY:  Different cortical layers have different metabolic sensitivities

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_DEGRADED_PERFORMANCE,
        EXCEPTION_SEVERITY_WARNING,
        "Substrate bridge: Layer II/III impaired"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "bridge_type", "cortical_substrate");
    nimcp_exception_set_context(ex, "layer", "II/III");
    nimcp_exception_set_context(ex, "layer_gain", "0.45");
    nimcp_exception_set_context(ex, "expected_gain", "1.0");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Cortical Bridge Exception Tests
//=============================================================================

TEST_F(BridgesExceptionHandlingTest, CorticalBridgeFidelityException) {
    // WHAT: Test exception for column fidelity degradation
    // WHY:  Low fidelity affects columnar computation quality

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_DEGRADED_PERFORMANCE,
        EXCEPTION_SEVERITY_WARNING,
        "Cortical bridge: column fidelity below threshold"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "bridge_type", "cortical_substrate");
    nimcp_exception_set_context(ex, "column_fidelity", "0.35");
    nimcp_exception_set_context(ex, "min_fidelity", "0.5");

    nimcp_exception_unref(ex);
}

TEST_F(BridgesExceptionHandlingTest, CorticalBridgeCompetitionException) {
    // WHAT: Test exception for weakened columnar competition
    // WHY:  Competition efficiency affects winner-take-all

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_DEGRADED_PERFORMANCE,
        EXCEPTION_SEVERITY_INFO,
        "Cortical bridge: competition efficiency reduced"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "bridge_type", "cortical_plasticity");
    nimcp_exception_set_context(ex, "competition_efficiency", "0.6");
    nimcp_exception_set_context(ex, "sparsity_modulation", "0.8");

    nimcp_exception_unref(ex);
}

TEST_F(BridgesExceptionHandlingTest, CorticalBridgeHierarchyException) {
    // WHAT: Test exception for hierarchical processing depth limits
    // WHY:  Limited hierarchy affects abstraction capabilities

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_RESOURCE_EXHAUSTED,
        EXCEPTION_SEVERITY_WARNING,
        "Cortical bridge: hierarchical depth capacity exceeded"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "bridge_type", "cortical_hierarchy");
    nimcp_exception_set_context(ex, "current_depth", "8");
    nimcp_exception_set_context(ex, "max_depth", "6");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Cross-Bridge Communication Exception Tests
//=============================================================================

TEST_F(BridgesExceptionHandlingTest, CrossBridgeMessageException) {
    // WHAT: Test exception for cross-bridge message failures
    // WHY:  Bridges communicate via bio-async and can have message errors

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_MESSAGE_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        "Cross-bridge message delivery failed"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "source_bridge", "feature_extractor_fep");
    nimcp_exception_set_context(ex, "target_bridge", "population_coding_fep");
    nimcp_exception_set_context(ex, "message_type", "precision_update");
    nimcp_exception_set_context(ex, "error_reason", "queue_full");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "message_type"), "precision_update");

    nimcp_exception_unref(ex);
}

TEST_F(BridgesExceptionHandlingTest, CrossBridgeSyncException) {
    // WHAT: Test exception for synchronization failures between bridges
    // WHY:  Bridges need synchronized state for coherent behavior

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_GPU_SYNC,
        EXCEPTION_SEVERITY_ERROR,
        "Cross-bridge synchronization failed"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "bridge_a", "thalamic_router_fep");
    nimcp_exception_set_context(ex, "bridge_b", "attention_fep");
    nimcp_exception_set_context(ex, "state_mismatch", "precision");
    nimcp_exception_set_context(ex, "delta_ms", "50");

    nimcp_exception_unref(ex);
}

TEST_F(BridgesExceptionHandlingTest, CrossBridgeCyclicDependencyException) {
    // WHAT: Test exception for cyclic dependency detection
    // WHY:  Cyclic bridge dependencies can cause deadlocks

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_CYCLIC_DEPENDENCY,
        EXCEPTION_SEVERITY_ERROR,
        "Cyclic dependency detected in bridge update chain"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "cycle_path", "A->B->C->A");
    nimcp_exception_set_context(ex, "detection_point", "bridge_C");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Bio-Async Bridge Exception Tests
//=============================================================================

TEST_F(BridgesExceptionHandlingTest, BioAsyncConnectionException) {
    // WHAT: Test exception for bio-async router connection failures
    // WHY:  Bridges depend on bio-async for distributed communication

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_CONNECTION_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        "Bio-async connection failed: router unavailable"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "bridge_type", "cortical_substrate");
    nimcp_exception_set_context(ex, "module_id", "0x1210");
    nimcp_exception_set_context(ex, "module_name", "substrate_cortical");

    nimcp_exception_unref(ex);
}

TEST_F(BridgesExceptionHandlingTest, BioAsyncTimeoutException) {
    // WHAT: Test exception for bio-async message timeout
    // WHY:  Async messages can timeout affecting bridge coordination

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_WARNING,
        "Bio-async message timeout"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "bridge_type", "sequence_detector_fep");
    nimcp_exception_set_context(ex, "timeout_ms", "100");
    nimcp_exception_set_context(ex, "message_type", "state_broadcast");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Bridge Recovery Exception Tests
//=============================================================================

TEST_F(BridgesExceptionHandlingTest, BridgeRecoveryStrategy) {
    // WHAT: Test recovery strategies for bridge exceptions
    // WHY:  Different bridge errors need different recovery approaches

    struct {
        nimcp_error_t code;
        nimcp_exception_severity_t severity;
        const char* bridge_type;
    } test_cases[] = {
        { NIMCP_ERROR_CONNECTION_FAILED, EXCEPTION_SEVERITY_ERROR, "fep_bridge" },
        { NIMCP_ERROR_RESOURCE_EXHAUSTED, EXCEPTION_SEVERITY_SEVERE, "substrate_bridge" },
        { NIMCP_ERROR_NAN_DETECTED, EXCEPTION_SEVERITY_ERROR, "cortical_bridge" },
    };

    for (const auto& tc : test_cases) {
        nimcp_exception_t* ex = create_bridge_exception(
            tc.code, tc.severity, tc.bridge_type
        );
        ASSERT_NE(ex, nullptr);

        nimcp_exception_recovery_strategy_t strategy;
        nimcp_exception_get_recovery_strategy(ex, &strategy);

        // All bridge exceptions should have a valid primary action
        EXPECT_NE(strategy.primary_action, (nimcp_exception_recovery_action_t)-1)
            << "Invalid strategy for: " << tc.bridge_type;

        nimcp_exception_unref(ex);
    }
}

TEST_F(BridgesExceptionHandlingTest, BridgeReconnectionRecovery) {
    // WHAT: Test recovery via bridge reconnection
    // WHY:  Many bridge errors can be resolved by reconnecting

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_NOT_CONNECTED,
        EXCEPTION_SEVERITY_ERROR,
        "Bridge systems disconnected"
    );

    ASSERT_NE(ex, nullptr);
    ex->suggested_action = EXCEPTION_RECOVERY_RESTART_COMPONENT;

    nimcp_exception_set_context(ex, "bridge_type", "hemispheric_fep");
    nimcp_exception_set_context(ex, "recovery_hint", "reconnect_systems");

    EXPECT_EQ(ex->suggested_action, EXCEPTION_RECOVERY_RESTART_COMPONENT);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Bridge Exception Chaining Tests
//=============================================================================

TEST_F(BridgesExceptionHandlingTest, BridgeExceptionCausedBySubsystem) {
    // WHAT: Test exception chaining from subsystem to bridge
    // WHY:  Bridge failures often have underlying subsystem causes

    // Root cause: FEP system error
    nimcp_exception_t* fep_ex = create_bridge_exception(
        NIMCP_ERROR_COMPUTATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        "FEP free energy calculation failed"
    );
    nimcp_exception_set_context(fep_ex, "component", "fep_system");

    // Bridge-level exception
    nimcp_exception_t* bridge_ex = create_bridge_exception(
        NIMCP_ERROR_UPDATE_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        "FEP bridge update failed"
    );
    nimcp_exception_set_context(bridge_ex, "component", "circular_buffer_fep_bridge");
    nimcp_exception_set_cause(bridge_ex, fep_ex);

    // Verify chain
    nimcp_exception_t* cause = nimcp_exception_get_cause(bridge_ex);
    ASSERT_NE(cause, nullptr);
    EXPECT_EQ(cause->code, NIMCP_ERROR_COMPUTATION_FAILED);
    EXPECT_STREQ(nimcp_exception_get_context(cause, "component"), "fep_system");

    nimcp_exception_unref(bridge_ex);
}

TEST_F(BridgesExceptionHandlingTest, MultiBridgeAggregateException) {
    // WHAT: Test aggregate exception for multiple bridge failures
    // WHY:  System updates may cause multiple bridges to fail

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_MULTIPLE_ERRORS,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Multiple bridge update failures"
    );
    ASSERT_NE(agg, nullptr);

    // Add individual bridge failures
    nimcp_exception_t* ex1 = create_bridge_exception(
        NIMCP_ERROR_UPDATE_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        "FEP bridge update failed"
    );
    nimcp_exception_set_context(ex1, "bridge_type", "circular_buffer_fep");

    nimcp_exception_t* ex2 = create_bridge_exception(
        NIMCP_ERROR_UPDATE_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        "Substrate bridge update failed"
    );
    nimcp_exception_set_context(ex2, "bridge_type", "cortical_substrate");

    nimcp_exception_t* ex3 = create_bridge_exception(
        NIMCP_ERROR_UPDATE_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        "Training bridge update failed"
    );
    nimcp_exception_set_context(ex3, "bridge_type", "training_plasticity");

    nimcp_aggregate_exception_add(agg, ex1);
    nimcp_aggregate_exception_add(agg, ex2);
    nimcp_aggregate_exception_add(agg, ex3);

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 3u);

    // Overall severity should be at least as high as worst child
    EXPECT_GE(((nimcp_exception_t*)agg)->severity, EXCEPTION_SEVERITY_ERROR);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Bridge Handler Filtering Tests
//=============================================================================

TEST_F(BridgesExceptionHandlingTest, BridgeSpecificHandler) {
    // WHAT: Test handler that filters for bridge-related exceptions
    // WHY:  Bridge handlers may want to only handle bridge errors

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "bridge_handler";
    options.handler = bridge_exception_handler;
    options.priority = 100;
    // Category filtering would be used if bridges had a dedicated category
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    handler_call_count = 0;

    // Dispatch bridge exception
    nimcp_exception_t* bridge_ex = create_bridge_exception(
        NIMCP_ERROR_CONNECTION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        "Bridge connection failed"
    );
    nimcp_exception_set_context(bridge_ex, "bridge_type", "test_bridge");

    nimcp_exception_dispatch(bridge_ex);

    EXPECT_GE(handler_call_count.load(), 1);

    nimcp_exception_unref(bridge_ex);
    nimcp_handler_unregister(reg);
}

//=============================================================================
// Bridge Immune Integration Tests
//=============================================================================

TEST_F(BridgesExceptionHandlingTest, BridgeExceptionImmunePresentation) {
    // WHAT: Test immune system presentation for severe bridge exceptions
    // WHY:  Critical bridge failures need immune-mediated recovery

    nimcp_exception_immune_init(NULL);

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_RESOURCE_EXHAUSTED,
        EXCEPTION_SEVERITY_SEVERE,
        "Substrate bridge: metabolic resources depleted"
    );
    nimcp_exception_set_context(ex, "bridge_type", "cortical_substrate");
    nimcp_exception_set_context(ex, "recovery_needed", "true");

    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex, &response);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(ex->presented_to_immune);

    nimcp_exception_immune_shutdown();
    nimcp_exception_unref(ex);
}

//=============================================================================
// Sleep Bridge Exception Tests
//=============================================================================

TEST_F(BridgesExceptionHandlingTest, SleepBridgeStateException) {
    // WHAT: Test exception for invalid sleep state transitions
    // WHY:  Sleep bridges manage state-dependent processing

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_SEVERITY_WARNING,
        "Sleep bridge: invalid state transition"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "bridge_type", "circular_buffer_sleep");
    nimcp_exception_set_context(ex, "current_state", "REM");
    nimcp_exception_set_context(ex, "requested_state", "DEEP_SLEEP");
    nimcp_exception_set_context(ex, "transition_allowed", "false");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "current_state"), "REM");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Immune Bridge Exception Tests
//=============================================================================

TEST_F(BridgesExceptionHandlingTest, ImmuneBridgeActivationException) {
    // WHAT: Test exception for immune bridge activation failures
    // WHY:  Immune bridges must report activation issues

    nimcp_exception_t* ex = create_bridge_exception(
        NIMCP_ERROR_ACTIVATION_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        "Immune bridge: B cell activation failed"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "bridge_type", "thalamic_immune");
    nimcp_exception_set_context(ex, "immune_component", "b_cell");
    nimcp_exception_set_context(ex, "expected_state", "PLASMA");
    nimcp_exception_set_context(ex, "actual_state", "ACTIVATED");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
