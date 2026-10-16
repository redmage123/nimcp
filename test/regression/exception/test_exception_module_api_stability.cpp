/**
 * @file test_exception_module_api_stability.cpp
 * @brief Comprehensive regression tests for exception API stability across modules
 * @date 2026-01-21
 *
 * Tests exception API stability for recently converted modules:
 * 1. Swarm module exception API consistency
 * 2. Security module exception API stability
 * 3. Training module exception interface
 * 4. Perception module exception contracts
 * 5. Memory module exception behavior
 * 6. Bio-async exception routing API
 *
 * These tests ensure:
 * - Error code values remain stable (same codes as before macro conversion)
 * - Exception message format consistency
 * - Recovery action type stability
 * - Severity level mapping accuracy
 * - API signature compatibility
 * - Backward compatibility with pre-macro error handling
 *
 * IMPORTANT: If any test fails, it indicates a breaking change in the
 * exception API that may affect existing code.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

extern "C" {
#include "nimcp.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_macros.h"

// Module headers for API validation
#include "security/nimcp_security.h"
#include "async/nimcp_bio_async.h"
}

//=============================================================================
// Test Fixture Base
//=============================================================================

class ModuleExceptionAPIStabilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize NIMCP
        ASSERT_EQ(nimcp_init(), NIMCP_OK);

        // Initialize exception system
        int result = nimcp_exception_system_init();
        ASSERT_EQ(result, 0) << "Failed to initialize exception system";
    }

    void TearDown() override {
        // Clear any pending exceptions
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
        nimcp_shutdown();
    }
};

//=============================================================================
// Section 1: Swarm Module Exception API Consistency Tests
//=============================================================================

class SwarmExceptionAPITest : public ModuleExceptionAPIStabilityTest {
protected:
    // Swarm error code base (in brain region range)
    static const nimcp_error_t SWARM_ERROR_BASE = 11000;  // Swarm module error base
};

TEST_F(SwarmExceptionAPITest, ErrorCodeValues_SwarmGenericErrors_Stable) {
    // Swarm module generic errors should be in a consistent range
    // These tests verify the error code values haven't changed

    // Success code should always be 0
    EXPECT_EQ(NIMCP_SUCCESS, 0);

    // Generic errors that swarm module uses
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER, 1002);
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, 1003);
    EXPECT_EQ(NIMCP_ERROR_OPERATION_FAILED, 1006);
    EXPECT_EQ(NIMCP_ERROR_NOT_INITIALIZED, 1007);
    EXPECT_EQ(NIMCP_ERROR_TIMEOUT, 1010);
}

TEST_F(SwarmExceptionAPITest, ErrorCodeValues_ConcurrencyErrors_Stable) {
    // Swarm uses threading primitives - verify error codes
    EXPECT_EQ(NIMCP_ERROR_THREAD_CREATE, 6000);
    EXPECT_EQ(NIMCP_ERROR_THREAD_JOIN, 6001);
    EXPECT_EQ(NIMCP_ERROR_MUTEX_LOCK, 6002);
    EXPECT_EQ(NIMCP_ERROR_MUTEX_UNLOCK, 6003);
    EXPECT_EQ(NIMCP_ERROR_DEADLOCK, 6005);
    EXPECT_EQ(NIMCP_ERROR_THREAD_SYNC, 6007);
}

TEST_F(SwarmExceptionAPITest, ExceptionSeverity_ForSwarmErrors_Consistent) {
    // Swarm errors should map to appropriate severity levels

    // Timeout is typically WARNING or ERROR
    nimcp_exception_severity_t timeout_sev =
        nimcp_exception_get_severity_from_code(NIMCP_ERROR_TIMEOUT);
    EXPECT_GE(timeout_sev, EXCEPTION_SEVERITY_WARNING);
    EXPECT_LE(timeout_sev, EXCEPTION_SEVERITY_ERROR);

    // Deadlock should be SEVERE or CRITICAL
    nimcp_exception_severity_t deadlock_sev =
        nimcp_exception_get_severity_from_code(NIMCP_ERROR_DEADLOCK);
    EXPECT_GE(deadlock_sev, EXCEPTION_SEVERITY_SEVERE);
}

TEST_F(SwarmExceptionAPITest, RecoveryAction_ForSwarmErrors_Consistent) {
    // Verify recovery actions are consistent for swarm-related errors

    // Timeout typically suggests retry
    EXPECT_EQ(EXCEPTION_RECOVERY_RETRY, 1);

    // Deadlock may require thread restart
    EXPECT_EQ(EXCEPTION_RECOVERY_RESTART_THREAD, 5);
}

TEST_F(SwarmExceptionAPITest, ExceptionCreate_ForSwarmContext_Works) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__,
        __LINE__,
        __func__,
        "Swarm consensus timeout after %dms",
        100
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_TIMEOUT);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);
    EXPECT_TRUE(strstr(ex->message, "100") != nullptr);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Section 2: Security Module Exception API Stability Tests
//=============================================================================

class SecurityExceptionAPITest : public ModuleExceptionAPIStabilityTest {};

TEST_F(SecurityExceptionAPITest, ErrorCodeValues_SecurityErrors_Stable) {
    // Security error codes (9000-9099)
    EXPECT_EQ(NIMCP_ERROR_SECURITY_BASE, 9000);
    EXPECT_EQ(NIMCP_ERROR_BBB_REJECTED, 9001);
    EXPECT_EQ(NIMCP_ERROR_BBB_VALIDATION, 9002);
    EXPECT_EQ(NIMCP_ERROR_SECURITY_THREAT, 9003);
    EXPECT_EQ(NIMCP_ERROR_ACCESS_DENIED, 9004);
    EXPECT_EQ(NIMCP_ERROR_SIGNATURE_INVALID, 9005);
    EXPECT_EQ(NIMCP_ERROR_ENCRYPTION_FAILED, 9006);
    EXPECT_EQ(NIMCP_ERROR_DECRYPTION_FAILED, 9007);
    EXPECT_EQ(NIMCP_ERROR_CERTIFICATE_INVALID, 9008);
    EXPECT_EQ(NIMCP_ERROR_POLICY_VIOLATION, 9009);
    EXPECT_EQ(NIMCP_ERROR_QUARANTINE_REQUIRED, 9010);
}

TEST_F(SecurityExceptionAPITest, ErrorCodeValues_SecurityRange_Correct) {
    // All security errors must be in range [9000, 10000)
    EXPECT_GE(NIMCP_ERROR_SECURITY_BASE, 9000);
    EXPECT_LT(NIMCP_ERROR_SECURITY_BASE, 10000);
    EXPECT_GE(NIMCP_ERROR_QUARANTINE_REQUIRED, 9000);
    EXPECT_LT(NIMCP_ERROR_QUARANTINE_REQUIRED, 10000);
}

TEST_F(SecurityExceptionAPITest, ExceptionSeverity_SecurityErrors_CorrectMapping) {
    // Security threats should be CRITICAL or SEVERE
    nimcp_exception_severity_t threat_sev =
        nimcp_exception_get_severity_from_code(NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_GE(threat_sev, EXCEPTION_SEVERITY_SEVERE);

    // BBB rejection should be at least ERROR
    nimcp_exception_severity_t bbb_sev =
        nimcp_exception_get_severity_from_code(NIMCP_ERROR_BBB_REJECTED);
    EXPECT_GE(bbb_sev, EXCEPTION_SEVERITY_ERROR);

    // Access denied should be at least WARNING
    nimcp_exception_severity_t access_sev =
        nimcp_exception_get_severity_from_code(NIMCP_ERROR_ACCESS_DENIED);
    EXPECT_GE(access_sev, EXCEPTION_SEVERITY_WARNING);
}

TEST_F(SecurityExceptionAPITest, ExceptionCategory_Security_CorrectMapping) {
    // Security errors should map to security category
    int category = nimcp_error_get_category(NIMCP_ERROR_SECURITY_BASE);
    EXPECT_EQ(category, 9);  // Security category

    category = nimcp_error_get_category(NIMCP_ERROR_BBB_REJECTED);
    EXPECT_EQ(category, 9);

    category = nimcp_error_get_category(NIMCP_ERROR_QUARANTINE_REQUIRED);
    EXPECT_EQ(category, 9);
}

TEST_F(SecurityExceptionAPITest, RecoveryAction_Quarantine_Available) {
    // Quarantine recovery action must be available for security errors
    EXPECT_EQ(EXCEPTION_RECOVERY_QUARANTINE, 7);
}

TEST_F(SecurityExceptionAPITest, SecurityException_Create_WithContext_Works) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_SECURITY_THREAT,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__,
        __LINE__,
        __func__,
        "Security threat detected: %s at level %d",
        "injection_attempt",
        3
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_CRITICAL);
    EXPECT_TRUE(strstr(ex->message, "injection_attempt") != nullptr);

    // Add context
    int result = nimcp_exception_set_context(ex, "threat_type", "prompt_injection");
    EXPECT_EQ(result, 0);

    const char* ctx = nimcp_exception_get_context(ex, "threat_type");
    EXPECT_STREQ(ctx, "prompt_injection");

    nimcp_exception_unref(ex);
}

TEST_F(SecurityExceptionAPITest, ThreatLevelEnum_Stable) {
    // Verify threat level enum values haven't changed
    EXPECT_EQ(NIMCP_THREAT_NONE, 0);
    EXPECT_EQ(NIMCP_THREAT_LOW, 1);
    EXPECT_EQ(NIMCP_THREAT_MEDIUM, 2);
    EXPECT_EQ(NIMCP_THREAT_HIGH, 3);
    EXPECT_EQ(NIMCP_THREAT_CRITICAL, 4);
}

TEST_F(SecurityExceptionAPITest, ThreatLevelName_ReturnsValidStrings) {
    EXPECT_NE(nimcp_threat_level_name(NIMCP_THREAT_NONE), nullptr);
    EXPECT_NE(nimcp_threat_level_name(NIMCP_THREAT_LOW), nullptr);
    EXPECT_NE(nimcp_threat_level_name(NIMCP_THREAT_MEDIUM), nullptr);
    EXPECT_NE(nimcp_threat_level_name(NIMCP_THREAT_HIGH), nullptr);
    EXPECT_NE(nimcp_threat_level_name(NIMCP_THREAT_CRITICAL), nullptr);

    // Verify strings are non-empty
    EXPECT_GT(strlen(nimcp_threat_level_name(NIMCP_THREAT_CRITICAL)), 0UL);
}

//=============================================================================
// Section 3: Training Module Exception Interface Tests
//=============================================================================

class TrainingExceptionAPITest : public ModuleExceptionAPIStabilityTest {};

TEST_F(TrainingExceptionAPITest, ErrorCodeValues_BrainTrainingErrors_Stable) {
    // Brain/Network errors used by training (3000-3999)
    EXPECT_EQ(NIMCP_ERROR_BRAIN_CREATION, 3000);
    EXPECT_EQ(NIMCP_ERROR_BRAIN_INVALID, 3001);
    EXPECT_EQ(NIMCP_ERROR_NETWORK_CREATION, 3002);
    EXPECT_EQ(NIMCP_ERROR_NETWORK_INVALID, 3003);
    EXPECT_EQ(NIMCP_ERROR_DIMENSION_MISMATCH, 3004);
    EXPECT_EQ(NIMCP_ERROR_WEIGHT_INIT, 3005);
    EXPECT_EQ(NIMCP_ERROR_FORWARD_PASS, 3006);
    EXPECT_EQ(NIMCP_ERROR_BACKWARD_PASS, 3007);
    EXPECT_EQ(NIMCP_ERROR_LEARNING_FAILED, 3008);
    EXPECT_EQ(NIMCP_ERROR_INFERENCE_FAILED, 3009);
}

TEST_F(TrainingExceptionAPITest, ErrorCodeValues_BrainErrors_InRange) {
    // Brain errors must be in range [3000, 4000)
    EXPECT_GE(NIMCP_ERROR_BRAIN_CREATION, 3000);
    EXPECT_LT(NIMCP_ERROR_BRAIN_CREATION, 4000);
    EXPECT_GE(NIMCP_ERROR_INFERENCE_FAILED, 3000);
    EXPECT_LT(NIMCP_ERROR_INFERENCE_FAILED, 4000);
}

TEST_F(TrainingExceptionAPITest, ExceptionSeverity_TrainingErrors_Consistent) {
    // Forward/backward pass errors should be ERROR or SEVERE
    nimcp_exception_severity_t fwd_sev =
        nimcp_exception_get_severity_from_code(NIMCP_ERROR_FORWARD_PASS);
    EXPECT_GE(fwd_sev, EXCEPTION_SEVERITY_ERROR);

    nimcp_exception_severity_t bwd_sev =
        nimcp_exception_get_severity_from_code(NIMCP_ERROR_BACKWARD_PASS);
    EXPECT_GE(bwd_sev, EXCEPTION_SEVERITY_ERROR);

    // Learning failed should be at least WARNING
    nimcp_exception_severity_t learn_sev =
        nimcp_exception_get_severity_from_code(NIMCP_ERROR_LEARNING_FAILED);
    EXPECT_GE(learn_sev, EXCEPTION_SEVERITY_WARNING);
}

TEST_F(TrainingExceptionAPITest, ExceptionCategory_Training_CorrectMapping) {
    // Training errors should map to BRAIN category
    nimcp_exception_category_t cat =
        nimcp_exception_get_category_from_code(NIMCP_ERROR_FORWARD_PASS);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN);

    cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_BACKWARD_PASS);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN);
}

TEST_F(TrainingExceptionAPITest, RecoveryAction_Training_RollbackAvailable) {
    // Rollback should be available for training errors
    EXPECT_EQ(EXCEPTION_RECOVERY_ROLLBACK, 4);
    EXPECT_EQ(EXCEPTION_RECOVERY_RETRY, 1);
}

TEST_F(TrainingExceptionAPITest, TrainingException_WithGradientContext_Works) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Gradient explosion in layer %d: gradient_norm=%f",
        5,
        1e10f
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_BACKWARD_PASS);

    // Add training context
    nimcp_exception_set_context(ex, "layer", "5");
    nimcp_exception_set_context(ex, "gradient_norm", "1e10");
    nimcp_exception_set_context(ex, "epoch", "42");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "layer"), "5");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "epoch"), "42");

    nimcp_exception_unref(ex);
}

TEST_F(TrainingExceptionAPITest, DistributedTraining_ErrorCodes_Consistent) {
    // Distributed training uses threading and network errors
    EXPECT_EQ(NIMCP_ERROR_THREAD_CREATE, 6000);
    EXPECT_EQ(NIMCP_ERROR_THREAD_SYNC, 6007);
    EXPECT_EQ(NIMCP_ERROR_NETWORK_IO, 4008);
    EXPECT_EQ(NIMCP_ERROR_TIMEOUT, 1010);
}

//=============================================================================
// Section 4: Perception Module Exception Contracts Tests
//=============================================================================

class PerceptionExceptionAPITest : public ModuleExceptionAPIStabilityTest {};

TEST_F(PerceptionExceptionAPITest, ErrorCodeValues_GenericErrors_UsedByPerception) {
    // Perception uses generic errors for common cases
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER, 1002);
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, 1003);
    EXPECT_EQ(NIMCP_ERROR_OPERATION_FAILED, 1006);
    EXPECT_EQ(NIMCP_ERROR_NOT_INITIALIZED, 1007);
}

TEST_F(PerceptionExceptionAPITest, ErrorCodeValues_MemoryErrors_UsedByPerception) {
    // Perception modules allocate buffers
    EXPECT_EQ(NIMCP_ERROR_NO_MEMORY, 2000);
    EXPECT_EQ(NIMCP_ERROR_BUFFER_TOO_SMALL, 2001);
    EXPECT_EQ(NIMCP_ERROR_BUFFER_OVERFLOW, 2002);
}

TEST_F(PerceptionExceptionAPITest, ExceptionSeverity_BufferErrors_Correct) {
    // Buffer overflow should be SEVERE or CRITICAL
    nimcp_exception_severity_t sev =
        nimcp_exception_get_severity_from_code(NIMCP_ERROR_BUFFER_OVERFLOW);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_SEVERE);

    // No memory should be SEVERE
    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_NO_MEMORY);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_SEVERE);
}

TEST_F(PerceptionExceptionAPITest, ExceptionCategory_Memory_CorrectMapping) {
    nimcp_exception_category_t cat =
        nimcp_exception_get_category_from_code(NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_MEMORY);

    cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_BUFFER_OVERFLOW);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_MEMORY);
}

TEST_F(PerceptionExceptionAPITest, PerceptionException_WithVisualContext_Works) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BUFFER_TOO_SMALL,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Image buffer too small: need %d bytes, have %d bytes",
        640 * 480 * 3,
        640 * 480
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_BUFFER_TOO_SMALL);

    // Add visual context
    nimcp_exception_set_context(ex, "width", "640");
    nimcp_exception_set_context(ex, "height", "480");
    nimcp_exception_set_context(ex, "channels", "3");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "width"), "640");

    nimcp_exception_unref(ex);
}

TEST_F(PerceptionExceptionAPITest, MemoryException_Create_HasCorrectType) {
    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        640 * 480 * 3,  // requested size
        "Failed to allocate visual buffer: %zu bytes",
        (size_t)(640 * 480 * 3)
    );

    ASSERT_NE(mex, nullptr);
    EXPECT_EQ(mex->base.type, EXCEPTION_TYPE_MEMORY);
    EXPECT_EQ(mex->base.category, EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(mex->requested_size, (size_t)(640 * 480 * 3));

    nimcp_exception_unref((nimcp_exception_t*)mex);
}

//=============================================================================
// Section 5: Memory Module Exception Behavior Tests
//=============================================================================

class MemoryModuleExceptionAPITest : public ModuleExceptionAPIStabilityTest {};

TEST_F(MemoryModuleExceptionAPITest, ErrorCodeValues_CognitiveMemoryErrors_Stable) {
    // Working memory errors (8000-8999)
    EXPECT_EQ(NIMCP_ERROR_WORKING_MEMORY, 8000);

    // These are cognitive errors related to memory
    EXPECT_GE(NIMCP_ERROR_WORKING_MEMORY, 8000);
    EXPECT_LT(NIMCP_ERROR_WORKING_MEMORY, 9000);
}

TEST_F(MemoryModuleExceptionAPITest, ErrorCodeValues_HippocampusErrors_Stable) {
    // Hippocampus memory errors (10100-10199)
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_BASE, 10100);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_ENCODING, 10102);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_RETRIEVAL, 10103);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_MEMORY_FULL, 10105);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_PATTERN_SEP, 10106);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_PATTERN_COMP, 10107);
}

TEST_F(MemoryModuleExceptionAPITest, ExceptionSeverity_MemoryFull_Appropriate) {
    // Memory full should be at least WARNING
    nimcp_exception_severity_t sev =
        nimcp_exception_get_severity_from_code(NIMCP_ERROR_HIPPOCAMPUS_MEMORY_FULL);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_WARNING);
}

TEST_F(MemoryModuleExceptionAPITest, ExceptionCategory_Cognitive_CorrectMapping) {
    // Working memory should map to cognitive category
    nimcp_exception_category_t cat =
        nimcp_exception_get_category_from_code(NIMCP_ERROR_WORKING_MEMORY);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_COGNITIVE);
}

TEST_F(MemoryModuleExceptionAPITest, RecoveryAction_GC_AvailableForMemory) {
    // GC and compact should be available for memory errors
    EXPECT_EQ(EXCEPTION_RECOVERY_GC, 2);
    EXPECT_EQ(EXCEPTION_RECOVERY_COMPACT, 3);
}

TEST_F(MemoryModuleExceptionAPITest, MemoryException_WithCapacityContext_Works) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_HIPPOCAMPUS_MEMORY_FULL,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__,
        __LINE__,
        __func__,
        "Hippocampus memory full: capacity=%d, current=%d",
        1000000,
        1000000
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_HIPPOCAMPUS_MEMORY_FULL);

    // Add memory context
    nimcp_exception_set_context(ex, "capacity", "1000000");
    nimcp_exception_set_context(ex, "oldest_item_age_ms", "86400000");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "capacity"), "1000000");

    nimcp_exception_unref(ex);
}

TEST_F(MemoryModuleExceptionAPITest, BrainRegionError_Detection_Works) {
    // Hippocampus errors should be detected as brain region errors
    EXPECT_TRUE(nimcp_error_is_brain_region(NIMCP_ERROR_HIPPOCAMPUS_BASE));
    EXPECT_TRUE(nimcp_error_is_brain_region(NIMCP_ERROR_HIPPOCAMPUS_ENCODING));
    EXPECT_TRUE(nimcp_error_is_brain_region(NIMCP_ERROR_HIPPOCAMPUS_MEMORY_FULL));

    // Non-brain-region errors should not be detected
    EXPECT_FALSE(nimcp_error_is_brain_region(NIMCP_ERROR_WORKING_MEMORY));
}

//=============================================================================
// Section 6: Bio-Async Exception Routing API Tests
//=============================================================================

class BioAsyncExceptionAPITest : public ModuleExceptionAPIStabilityTest {};

TEST_F(BioAsyncExceptionAPITest, ErrorCodeValues_BioAsyncErrors_Stable) {
    // Bio-async error codes (10001-10009)
    EXPECT_EQ(NIMCP_BIO_ERROR_BASE, 10000);
    EXPECT_EQ(NIMCP_BIO_ERROR_NOT_INITIALIZED, 10001);
    EXPECT_EQ(NIMCP_BIO_ERROR_INVALID_CHANNEL, 10002);
    EXPECT_EQ(NIMCP_BIO_ERROR_CHANNEL_SATURATED, 10003);
    EXPECT_EQ(NIMCP_BIO_ERROR_PHASE_INCOHERENT, 10004);
    EXPECT_EQ(NIMCP_BIO_ERROR_WAVE_EXTINCT, 10005);
    EXPECT_EQ(NIMCP_BIO_ERROR_PREDICTION_STALE, 10006);
    EXPECT_EQ(NIMCP_BIO_ERROR_REFRACTORY, 10007);
    EXPECT_EQ(NIMCP_BIO_ERROR_DECAY_COMPLETE, 10008);
}

TEST_F(BioAsyncExceptionAPITest, BioChannelType_EnumValues_Stable) {
    // Channel types must be stable for promise/future creation
    EXPECT_EQ(BIO_CHANNEL_DOPAMINE, 0);
    EXPECT_EQ(BIO_CHANNEL_SEROTONIN, 1);
    EXPECT_EQ(BIO_CHANNEL_NOREPINEPHRINE, 2);
    EXPECT_EQ(BIO_CHANNEL_ACETYLCHOLINE, 3);
    EXPECT_EQ(BIO_CHANNEL_COUNT, 4);
}

TEST_F(BioAsyncExceptionAPITest, OscillationBand_EnumValues_Stable) {
    // Oscillation bands must be stable for phase sync
    EXPECT_EQ(BIO_OSC_DELTA, 0);
    EXPECT_EQ(BIO_OSC_THETA, 1);
    EXPECT_EQ(BIO_OSC_ALPHA, 2);
    EXPECT_EQ(BIO_OSC_BETA, 3);
    EXPECT_EQ(BIO_OSC_GAMMA, 4);
    EXPECT_EQ(BIO_OSC_BAND_COUNT, 5);
}

TEST_F(BioAsyncExceptionAPITest, BioFutureState_EnumValues_Stable) {
    // Future states must be stable for state checking
    EXPECT_EQ(BIO_FUTURE_PENDING, 0);
    EXPECT_EQ(BIO_FUTURE_COMPLETED, 1);
    EXPECT_EQ(BIO_FUTURE_FAILED, 2);
    EXPECT_EQ(BIO_FUTURE_CANCELLED, 3);
    EXPECT_EQ(BIO_FUTURE_DECAYED, 4);
    EXPECT_EQ(BIO_FUTURE_REFRACTORY, 5);
}

TEST_F(BioAsyncExceptionAPITest, BioChannelName_ReturnsValidStrings) {
    // Verify string conversion functions work
    EXPECT_STREQ(nimcp_bio_channel_name(BIO_CHANNEL_DOPAMINE), "dopamine");
    EXPECT_STREQ(nimcp_bio_channel_name(BIO_CHANNEL_SEROTONIN), "serotonin");
    EXPECT_STREQ(nimcp_bio_channel_name(BIO_CHANNEL_NOREPINEPHRINE), "norepinephrine");
    EXPECT_STREQ(nimcp_bio_channel_name(BIO_CHANNEL_ACETYLCHOLINE), "acetylcholine");
}

TEST_F(BioAsyncExceptionAPITest, OscillationBandName_ReturnsValidStrings) {
    EXPECT_STREQ(nimcp_oscillation_band_name(BIO_OSC_DELTA), "delta");
    EXPECT_STREQ(nimcp_oscillation_band_name(BIO_OSC_THETA), "theta");
    EXPECT_STREQ(nimcp_oscillation_band_name(BIO_OSC_ALPHA), "alpha");
    EXPECT_STREQ(nimcp_oscillation_band_name(BIO_OSC_BETA), "beta");
    EXPECT_STREQ(nimcp_oscillation_band_name(BIO_OSC_GAMMA), "gamma");
}

TEST_F(BioAsyncExceptionAPITest, BioFutureStateName_ReturnsValidStrings) {
    EXPECT_STREQ(nimcp_bio_future_state_name(BIO_FUTURE_PENDING), "pending");
    EXPECT_STREQ(nimcp_bio_future_state_name(BIO_FUTURE_COMPLETED), "completed");
    EXPECT_STREQ(nimcp_bio_future_state_name(BIO_FUTURE_FAILED), "failed");
    EXPECT_STREQ(nimcp_bio_future_state_name(BIO_FUTURE_CANCELLED), "cancelled");
    EXPECT_STREQ(nimcp_bio_future_state_name(BIO_FUTURE_DECAYED), "decayed");
    EXPECT_STREQ(nimcp_bio_future_state_name(BIO_FUTURE_REFRACTORY), "refractory");
}

TEST_F(BioAsyncExceptionAPITest, BioAsyncException_WithChannelContext_Works) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_BIO_ERROR_CHANNEL_SATURATED,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__,
        __LINE__,
        __func__,
        "Channel %s saturated at concentration %f",
        "dopamine",
        1.0f
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_BIO_ERROR_CHANNEL_SATURATED);

    // Add bio-async context
    nimcp_exception_set_context(ex, "channel", "dopamine");
    nimcp_exception_set_context(ex, "concentration", "1.0");
    nimcp_exception_set_context(ex, "decay_tau_ms", "200");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "channel"), "dopamine");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Section 7: Cross-Module Exception Interoperability Tests
//=============================================================================

class CrossModuleExceptionTest : public ModuleExceptionAPIStabilityTest {};

TEST_F(CrossModuleExceptionTest, ExceptionChaining_AcrossModules_Works) {
    // Create security exception as root cause
    nimcp_exception_t* security_ex = nimcp_exception_create(
        NIMCP_ERROR_BBB_REJECTED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "BBB rejected training input"
    );

    // Create training exception that wraps security exception
    nimcp_exception_t* training_ex = nimcp_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Learning failed due to security rejection"
    );

    // Chain exceptions
    nimcp_exception_ref(security_ex);  // Keep our reference
    nimcp_exception_set_cause(training_ex, security_ex);

    // Verify chain
    nimcp_exception_t* cause = nimcp_exception_get_cause(training_ex);
    ASSERT_EQ(cause, security_ex);
    EXPECT_EQ(cause->code, NIMCP_ERROR_BBB_REJECTED);

    nimcp_exception_unref(security_ex);
    nimcp_exception_unref(training_ex);
}

TEST_F(CrossModuleExceptionTest, AggregateException_MultipleModules_Works) {
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Multiple module failures during training"
    );

    // Add security error
    nimcp_exception_t* sec_ex = nimcp_exception_create(
        NIMCP_ERROR_BBB_VALIDATION,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "BBB validation warning"
    );
    nimcp_aggregate_exception_add(agg, sec_ex);

    // Add memory error
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1024 * 1024,
        "Buffer allocation failed"
    );
    nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)mem_ex);

    // Add bio-async error
    nimcp_exception_t* bio_ex = nimcp_exception_create(
        NIMCP_BIO_ERROR_CHANNEL_SATURATED,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Dopamine channel saturated"
    );
    nimcp_aggregate_exception_add(agg, bio_ex);

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 3UL);

    // Verify we can retrieve each
    EXPECT_EQ(nimcp_aggregate_exception_get(agg, 0)->code, NIMCP_ERROR_BBB_VALIDATION);
    EXPECT_EQ(nimcp_aggregate_exception_get(agg, 1)->code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(nimcp_aggregate_exception_get(agg, 2)->code, NIMCP_BIO_ERROR_CHANNEL_SATURATED);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

TEST_F(CrossModuleExceptionTest, ErrorCategoryMapping_AllModules_Correct) {
    // Test category mapping for each module type

    // Security
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_SECURITY_BASE), 9);

    // Training/Brain
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_FORWARD_PASS), 3);

    // Memory
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_NO_MEMORY), 2);

    // Threading (swarm)
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_DEADLOCK), 6);

    // Cognitive (working memory)
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_WORKING_MEMORY), 8);

    // Generic
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_OPERATION_FAILED), 1);
}

//=============================================================================
// Section 8: Handler Registration API Stability Tests
//=============================================================================

class HandlerAPIStabilityTest : public ModuleExceptionAPIStabilityTest {
protected:
    static std::atomic<int> handler_call_count;

    static bool test_module_handler(nimcp_exception_t* ex, void* user_data) {
        (void)ex;
        (void)user_data;
        handler_call_count++;
        return false;  // Don't consume
    }
};

std::atomic<int> HandlerAPIStabilityTest::handler_call_count{0};

TEST_F(HandlerAPIStabilityTest, HandlerPriorities_Stable) {
    EXPECT_EQ(NIMCP_HANDLER_PRIORITY_HIGH, 100);
    EXPECT_EQ(NIMCP_HANDLER_PRIORITY_NORMAL, 50);
    EXPECT_EQ(NIMCP_HANDLER_PRIORITY_LOW, 10);
}

TEST_F(HandlerAPIStabilityTest, HandlerRegister_ForSecurityModule_Works) {
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "security_module_handler";
    opts.handler = test_module_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.min_severity = EXCEPTION_SEVERITY_WARNING;
    opts.category_filter = EXCEPTION_CATEGORY_GENERIC;  // Accept all

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);
    EXPECT_STREQ(reg->options.name, "security_module_handler");
    EXPECT_TRUE(reg->active);

    nimcp_handler_unregister(reg);
}

TEST_F(HandlerAPIStabilityTest, HandlerRegister_ForTrainingModule_Works) {
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "training_module_handler";
    opts.handler = test_module_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    nimcp_handler_unregister(reg);
}

TEST_F(HandlerAPIStabilityTest, HandlerDispatch_CrossModule_Works) {
    handler_call_count = 0;

    // Register handlers for different modules
    nimcp_handler_options_t sec_opts;
    nimcp_handler_default_options(&sec_opts);
    sec_opts.name = "security_handler";
    sec_opts.handler = test_module_handler;
    sec_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    nimcp_handler_registration_t* sec_reg = nimcp_handler_register(&sec_opts);

    nimcp_handler_options_t train_opts;
    nimcp_handler_default_options(&train_opts);
    train_opts.name = "training_handler";
    train_opts.handler = test_module_handler;
    train_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    nimcp_handler_registration_t* train_reg = nimcp_handler_register(&train_opts);

    // Create and dispatch exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception"
    );

    nimcp_exception_dispatch(ex);

    // Both handlers should be called (neither consumed)
    EXPECT_EQ(handler_call_count.load(), 2);

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(sec_reg);
    nimcp_handler_unregister(train_reg);
}

//=============================================================================
// Section 9: Error String Conversion Stability Tests
//=============================================================================

class ErrorStringStabilityTest : public ModuleExceptionAPIStabilityTest {};

TEST_F(ErrorStringStabilityTest, ErrorToString_AllModuleCodes_ReturnNonEmpty) {
    // Test representative error codes from each module
    const nimcp_error_t module_codes[] = {
        // Success
        NIMCP_SUCCESS,
        // Generic
        NIMCP_ERROR_UNKNOWN,
        NIMCP_ERROR_INVALID_PARAMETER,
        NIMCP_ERROR_NULL_POINTER,
        // Memory
        NIMCP_ERROR_NO_MEMORY,
        NIMCP_ERROR_BUFFER_OVERFLOW,
        // Brain/Training
        NIMCP_ERROR_BRAIN_CREATION,
        NIMCP_ERROR_FORWARD_PASS,
        NIMCP_ERROR_LEARNING_FAILED,
        // IO
        NIMCP_ERROR_FILE_NOT_FOUND,
        // Threading/Swarm
        NIMCP_ERROR_THREAD_CREATE,
        NIMCP_ERROR_DEADLOCK,
        // Signal
        NIMCP_ERROR_SIGSEGV,
        // Cognitive/Memory
        NIMCP_ERROR_WORKING_MEMORY,
        // Security
        NIMCP_ERROR_SECURITY_BASE,
        NIMCP_ERROR_BBB_REJECTED,
        // Brain regions
        NIMCP_ERROR_HIPPOCAMPUS_BASE,
        NIMCP_ERROR_MOTOR_BASE,
    };

    for (nimcp_error_t code : module_codes) {
        const char* msg = nimcp_error_to_string(code);
        ASSERT_NE(msg, nullptr) << "Error code " << code << " returned NULL string";
        EXPECT_GT(strlen(msg), 0UL) << "Error code " << code << " returned empty string";
    }
}

TEST_F(ErrorStringStabilityTest, SeverityToString_AllValues_ReturnExpected) {
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_DEBUG), "DEBUG");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_INFO), "INFO");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_WARNING), "WARNING");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_ERROR), "ERROR");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_SEVERE), "SEVERE");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_CRITICAL), "CRITICAL");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_FATAL), "FATAL");
}

TEST_F(ErrorStringStabilityTest, CategoryToString_AllValues_ReturnExpected) {
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_GENERIC), "GENERIC");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_MEMORY), "MEMORY");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_BRAIN), "BRAIN");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_IO), "IO");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_CONFIG), "CONFIG");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_THREADING), "THREADING");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_SIGNAL), "SIGNAL");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_COGNITIVE), "COGNITIVE");
}

TEST_F(ErrorStringStabilityTest, TypeToString_AllValues_ReturnExpected) {
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_BASE), "BASE");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_MEMORY), "MEMORY");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_BRAIN), "BRAIN");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_IO), "IO");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_THREADING), "THREADING");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_SECURITY), "SECURITY");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_COGNITIVE), "COGNITIVE");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_GPU), "GPU");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_AGGREGATE), "AGGREGATE");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_SIGNAL), "SIGNAL");
}

TEST_F(ErrorStringStabilityTest, RecoveryActionToString_AllValues_ReturnExpected) {
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_NONE), "NONE");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RETRY), "RETRY");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GC), "GC");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_COMPACT), "COMPACT");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_ROLLBACK), "ROLLBACK");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RESTART_THREAD), "RESTART_THREAD");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RESTART_COMPONENT), "RESTART_COMPONENT");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_QUARANTINE), "QUARANTINE");
}

//=============================================================================
// Section 10: Backward Compatibility Tests
//=============================================================================

class BackwardCompatibilityTest : public ModuleExceptionAPIStabilityTest {};

TEST_F(BackwardCompatibilityTest, LegacyErrorHelpers_StillWork) {
    // These legacy helpers must continue to work for backward compatibility
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS));
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS_WITH_WARNINGS));
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS_PARTIAL));

    EXPECT_FALSE(nimcp_error_is_success(NIMCP_ERROR_OPERATION_FAILED));
    EXPECT_FALSE(nimcp_error_is_success(NIMCP_ERROR_NULL_POINTER));
}

TEST_F(BackwardCompatibilityTest, LegacyFEPBridgeHelpers_StillWork) {
    // FEP bridges return 0/-1 convention must still work
    EXPECT_TRUE(nimcp_is_ok(NIMCP_SUCCESS));
    EXPECT_FALSE(nimcp_is_ok(NIMCP_ERROR_OPERATION_FAILED));

    EXPECT_FALSE(nimcp_is_error(NIMCP_SUCCESS));
    EXPECT_TRUE(nimcp_is_error(NIMCP_ERROR_OPERATION_FAILED));

    // Conversion functions
    EXPECT_EQ(nimcp_from_fep_result(0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_from_fep_result(-1), NIMCP_ERROR_OPERATION_FAILED);

    EXPECT_EQ(nimcp_to_fep_result(NIMCP_SUCCESS), 0);
    EXPECT_EQ(nimcp_to_fep_result(NIMCP_ERROR_OPERATION_FAILED), -1);
}

TEST_F(BackwardCompatibilityTest, OldStatusEnum_Compatible) {
    // NIMCP_OK and NIMCP_ERROR must work for backward compatibility
    EXPECT_EQ(NIMCP_OK, 0);
    EXPECT_EQ(NIMCP_SUCCESS, NIMCP_OK);

    // Common error aliases
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(BackwardCompatibilityTest, RefCountingAPI_Unchanged) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->ref_count, 1);

    // ref() returns same pointer
    nimcp_exception_t* ex2 = nimcp_exception_ref(ex);
    EXPECT_EQ(ex, ex2);
    EXPECT_EQ(ex->ref_count, 2);

    // unref() decrements
    nimcp_exception_unref(ex);
    EXPECT_EQ(ex->ref_count, 1);

    // Final unref() frees
    nimcp_exception_unref(ex);

    // NULL-safety
    EXPECT_EQ(nimcp_exception_ref(nullptr), nullptr);
    nimcp_exception_unref(nullptr);  // Should not crash
}

TEST_F(BackwardCompatibilityTest, ContextAPI_Unchanged) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test"
    );

    // Context API still works as documented
    int result = nimcp_exception_set_context(ex, "key1", "value1");
    EXPECT_EQ(result, 0);

    result = nimcp_exception_set_context(ex, "key2", "value2");
    EXPECT_EQ(result, 0);

    EXPECT_EQ(nimcp_exception_context_count(ex), 2UL);
    EXPECT_STREQ(nimcp_exception_get_context(ex, "key1"), "value1");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "key2"), "value2");
    EXPECT_EQ(nimcp_exception_get_context(ex, "nonexistent"), nullptr);

    result = nimcp_exception_remove_context(ex, "key1");
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_exception_context_count(ex), 1UL);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
