/**
 * @file test_exception_error_code_stability.cpp
 * @brief Error code stability regression tests for NIMCP
 *
 * Tests error code stability to prevent breaking changes:
 * - Verify error code numeric values haven't changed
 * - Verify error code names match expected values
 * - Test all NIMCP_ERROR_* codes are properly handled
 * - Test error code to string conversion
 *
 * IMPORTANT: These tests ensure backward compatibility.
 * If any test fails, the error code has changed and
 * existing code depending on these values will break.
 *
 * Estimated tests: 60
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>

extern "C" {
#include "nimcp.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ErrorCodeStabilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(nimcp_init(), NIMCP_OK);
    }

    void TearDown() override {
        nimcp_shutdown();
    }
};

//=============================================================================
// Success Codes (0-999)
//=============================================================================

TEST_F(ErrorCodeStabilityTest, SuccessCodes_HaveExpectedValues) {
    // These values MUST NOT CHANGE - existing code depends on them
    EXPECT_EQ(NIMCP_SUCCESS, 0);
    EXPECT_EQ(NIMCP_SUCCESS_WITH_WARNINGS, 1);
    EXPECT_EQ(NIMCP_SUCCESS_PARTIAL, 2);
}

TEST_F(ErrorCodeStabilityTest, SuccessCodes_InSuccessRange) {
    // All success codes must be in range [0, 999]
    EXPECT_GE(NIMCP_SUCCESS, 0);
    EXPECT_LT(NIMCP_SUCCESS, 1000);
    EXPECT_GE(NIMCP_SUCCESS_WITH_WARNINGS, 0);
    EXPECT_LT(NIMCP_SUCCESS_WITH_WARNINGS, 1000);
    EXPECT_GE(NIMCP_SUCCESS_PARTIAL, 0);
    EXPECT_LT(NIMCP_SUCCESS_PARTIAL, 1000);
}

//=============================================================================
// Generic Errors (1000-1999)
//=============================================================================

TEST_F(ErrorCodeStabilityTest, GenericErrors_HaveExpectedValues) {
    // These values MUST NOT CHANGE - existing code depends on them
    EXPECT_EQ(NIMCP_ERROR_UNKNOWN, 1000);
    EXPECT_EQ(NIMCP_ERROR_NOT_IMPLEMENTED, 1001);
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER, 1002);
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, 1002);  // Alias
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, 1003);
    EXPECT_EQ(NIMCP_ERROR_OUT_OF_RANGE, 1004);
    EXPECT_EQ(NIMCP_ERROR_INVALID_STATE, 1005);
    EXPECT_EQ(NIMCP_ERROR_OPERATION_FAILED, 1006);
    EXPECT_EQ(NIMCP_ERROR_NOT_INITIALIZED, 1007);
    EXPECT_EQ(NIMCP_ERROR_ALREADY_EXISTS, 1008);
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND, 1009);
    EXPECT_EQ(NIMCP_ERROR_TIMEOUT, 1010);
    EXPECT_EQ(NIMCP_ERROR_CANCELLED, 1011);
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED, 1012);
}

TEST_F(ErrorCodeStabilityTest, GenericErrors_InGenericRange) {
    // All generic errors must be in range [1000, 1999]
    EXPECT_GE(NIMCP_ERROR_UNKNOWN, 1000);
    EXPECT_LT(NIMCP_ERROR_UNKNOWN, 2000);
    EXPECT_GE(NIMCP_ERROR_PERMISSION_DENIED, 1000);
    EXPECT_LT(NIMCP_ERROR_PERMISSION_DENIED, 2000);
}

//=============================================================================
// GPU/Hardware Errors (1100-1199)
//=============================================================================

TEST_F(ErrorCodeStabilityTest, GPUErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_GPU, 1100);
    EXPECT_EQ(NIMCP_ERROR_GPU_NOT_AVAILABLE, 1101);
    EXPECT_EQ(NIMCP_ERROR_GPU_MEMORY, 1102);
    EXPECT_EQ(NIMCP_ERROR_CUDA, 1103);
    EXPECT_EQ(NIMCP_ERROR_KERNEL_LAUNCH, 1104);
    EXPECT_EQ(NIMCP_ERROR_GPU_SYNC, 1105);
}

TEST_F(ErrorCodeStabilityTest, GPUErrors_InGPURange) {
    EXPECT_GE(NIMCP_ERROR_GPU, 1100);
    EXPECT_LT(NIMCP_ERROR_GPU, 1200);
    EXPECT_GE(NIMCP_ERROR_GPU_SYNC, 1100);
    EXPECT_LT(NIMCP_ERROR_GPU_SYNC, 1200);
}

//=============================================================================
// Memory Errors (2000-2999)
//=============================================================================

TEST_F(ErrorCodeStabilityTest, MemoryErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_NO_MEMORY, 2000);
    EXPECT_EQ(NIMCP_ERROR_BUFFER_TOO_SMALL, 2001);
    EXPECT_EQ(NIMCP_ERROR_BUFFER_OVERFLOW, 2002);
    EXPECT_EQ(NIMCP_ERROR_MEMORY_CORRUPTION, 2003);
    EXPECT_EQ(NIMCP_ERROR_INVALID_ADDRESS, 2004);
    EXPECT_EQ(NIMCP_ERROR_MEMORY_LEAK, 2005);
    EXPECT_EQ(NIMCP_ERROR_DOUBLE_FREE, 2006);
}

TEST_F(ErrorCodeStabilityTest, MemoryErrors_InMemoryRange) {
    EXPECT_GE(NIMCP_ERROR_NO_MEMORY, 2000);
    EXPECT_LT(NIMCP_ERROR_NO_MEMORY, 3000);
    EXPECT_GE(NIMCP_ERROR_DOUBLE_FREE, 2000);
    EXPECT_LT(NIMCP_ERROR_DOUBLE_FREE, 3000);
}

//=============================================================================
// Brain/Network Errors (3000-3999)
//=============================================================================

TEST_F(ErrorCodeStabilityTest, BrainErrors_HaveExpectedValues) {
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
    EXPECT_EQ(NIMCP_ERROR_COW_FAILED, 3010);
    EXPECT_EQ(NIMCP_ERROR_CLONE_FAILED, 3011);
}

TEST_F(ErrorCodeStabilityTest, KGWiringErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_KG_WIRING_BASE, 3050);
    EXPECT_EQ(NIMCP_ERROR_KG_WIRING_CREATE, 3050);
    EXPECT_EQ(NIMCP_ERROR_KG_WIRING_NULL, 3051);
    EXPECT_EQ(NIMCP_ERROR_KG_WIRING_INPUTS_FULL, 3052);
    EXPECT_EQ(NIMCP_ERROR_KG_WIRING_OUTPUTS_FULL, 3053);
    EXPECT_EQ(NIMCP_ERROR_KG_WIRING_HANDLERS_FULL, 3054);
    EXPECT_EQ(NIMCP_ERROR_KG_WIRING_METADATA_FULL, 3055);
    EXPECT_EQ(NIMCP_ERROR_KG_WIRING_STRING_TOO_LONG, 3056);
    EXPECT_EQ(NIMCP_ERROR_KG_WIRING_INVALID_NAME, 3057);
    EXPECT_EQ(NIMCP_ERROR_KG_WIRING_INVALID_TYPE, 3058);
    EXPECT_EQ(NIMCP_ERROR_KG_WIRING_WEIGHT_ALLOC, 3059);
    EXPECT_EQ(NIMCP_ERROR_KG_WIRING_WEIGHT_INVALID, 3060);
    EXPECT_EQ(NIMCP_ERROR_KG_WIRING_VALIDATION, 3061);
    EXPECT_EQ(NIMCP_ERROR_KG_WIRING_DUPLICATE, 3062);
}

TEST_F(ErrorCodeStabilityTest, BrainErrors_InBrainRange) {
    EXPECT_GE(NIMCP_ERROR_BRAIN_CREATION, 3000);
    EXPECT_LT(NIMCP_ERROR_BRAIN_CREATION, 4000);
    EXPECT_GE(NIMCP_ERROR_KG_WIRING_DUPLICATE, 3000);
    EXPECT_LT(NIMCP_ERROR_KG_WIRING_DUPLICATE, 4000);
}

//=============================================================================
// I/O Errors (4000-4999)
//=============================================================================

TEST_F(ErrorCodeStabilityTest, IOErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_FILE_NOT_FOUND, 4000);
    EXPECT_EQ(NIMCP_ERROR_FILE_READ, 4001);
    EXPECT_EQ(NIMCP_ERROR_FILE_WRITE, 4002);
    EXPECT_EQ(NIMCP_ERROR_FILE_OPEN, 4003);
    EXPECT_EQ(NIMCP_ERROR_FILE_CLOSE, 4004);
    EXPECT_EQ(NIMCP_ERROR_FILE_CORRUPT, 4005);
    EXPECT_EQ(NIMCP_ERROR_SERIALIZATION, 4006);
    EXPECT_EQ(NIMCP_ERROR_DESERIALIZATION, 4007);
    EXPECT_EQ(NIMCP_ERROR_NETWORK_IO, 4008);
    EXPECT_EQ(NIMCP_ERROR_SOCKET_ERROR, 4009);
    EXPECT_EQ(NIMCP_ERROR_IO, 4010);
}

TEST_F(ErrorCodeStabilityTest, IOErrors_InIORange) {
    EXPECT_GE(NIMCP_ERROR_FILE_NOT_FOUND, 4000);
    EXPECT_LT(NIMCP_ERROR_FILE_NOT_FOUND, 5000);
    EXPECT_GE(NIMCP_ERROR_IO, 4000);
    EXPECT_LT(NIMCP_ERROR_IO, 5000);
}

//=============================================================================
// Configuration Errors (5000-5999)
//=============================================================================

TEST_F(ErrorCodeStabilityTest, ConfigErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_CONFIG_INVALID, 5000);
    EXPECT_EQ(NIMCP_ERROR_CONFIG_PARSE, 5001);
    EXPECT_EQ(NIMCP_ERROR_CONFIG_MISSING, 5002);
    EXPECT_EQ(NIMCP_ERROR_CONFIG_TYPE, 5003);
    EXPECT_EQ(NIMCP_ERROR_CONFIG_RANGE, 5004);
    EXPECT_EQ(NIMCP_ERROR_CONFIG_RELOAD, 5005);
}

TEST_F(ErrorCodeStabilityTest, ConfigErrors_InConfigRange) {
    EXPECT_GE(NIMCP_ERROR_CONFIG_INVALID, 5000);
    EXPECT_LT(NIMCP_ERROR_CONFIG_INVALID, 6000);
    EXPECT_GE(NIMCP_ERROR_CONFIG_RELOAD, 5000);
    EXPECT_LT(NIMCP_ERROR_CONFIG_RELOAD, 6000);
}

//=============================================================================
// Threading/Concurrency Errors (6000-6999)
//=============================================================================

TEST_F(ErrorCodeStabilityTest, ThreadingErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_THREAD_CREATE, 6000);
    EXPECT_EQ(NIMCP_ERROR_THREAD_JOIN, 6001);
    EXPECT_EQ(NIMCP_ERROR_MUTEX_LOCK, 6002);
    EXPECT_EQ(NIMCP_ERROR_MUTEX_UNLOCK, 6003);
    EXPECT_EQ(NIMCP_ERROR_MUTEX_INIT, 6004);
    EXPECT_EQ(NIMCP_ERROR_DEADLOCK, 6005);
    EXPECT_EQ(NIMCP_ERROR_RACE_CONDITION, 6006);
    EXPECT_EQ(NIMCP_ERROR_THREAD_SYNC, 6007);
}

TEST_F(ErrorCodeStabilityTest, ThreadingErrors_InThreadingRange) {
    EXPECT_GE(NIMCP_ERROR_THREAD_CREATE, 6000);
    EXPECT_LT(NIMCP_ERROR_THREAD_CREATE, 7000);
    EXPECT_GE(NIMCP_ERROR_THREAD_SYNC, 6000);
    EXPECT_LT(NIMCP_ERROR_THREAD_SYNC, 7000);
}

//=============================================================================
// Signal/Crash Errors (7000-7999)
//=============================================================================

TEST_F(ErrorCodeStabilityTest, SignalErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_SIGNAL_RECEIVED, 7000);
    EXPECT_EQ(NIMCP_ERROR_SIGSEGV, 7001);
    EXPECT_EQ(NIMCP_ERROR_SIGABRT, 7002);
    EXPECT_EQ(NIMCP_ERROR_SIGFPE, 7003);
    EXPECT_EQ(NIMCP_ERROR_SIGBUS, 7004);
    EXPECT_EQ(NIMCP_ERROR_SIGILL, 7005);
    EXPECT_EQ(NIMCP_ERROR_CRASH_RECOVERY, 7006);
    EXPECT_EQ(NIMCP_ERROR_CHECKPOINT_SAVE, 7007);
    EXPECT_EQ(NIMCP_ERROR_CHECKPOINT_LOAD, 7008);
}

TEST_F(ErrorCodeStabilityTest, SignalErrors_InSignalRange) {
    EXPECT_GE(NIMCP_ERROR_SIGNAL_RECEIVED, 7000);
    EXPECT_LT(NIMCP_ERROR_SIGNAL_RECEIVED, 8000);
    EXPECT_GE(NIMCP_ERROR_CHECKPOINT_LOAD, 7000);
    EXPECT_LT(NIMCP_ERROR_CHECKPOINT_LOAD, 8000);
}

//=============================================================================
// Phase 10 Cognitive Errors (8000-8999)
//=============================================================================

TEST_F(ErrorCodeStabilityTest, CognitiveErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_WORKING_MEMORY, 8000);
    EXPECT_EQ(NIMCP_ERROR_EMOTIONAL_TAGGING, 8001);
    EXPECT_EQ(NIMCP_ERROR_EXECUTIVE_CONTROL, 8002);
    EXPECT_EQ(NIMCP_ERROR_SLEEP_WAKE, 8003);
    EXPECT_EQ(NIMCP_ERROR_MENTAL_HEALTH, 8004);
    EXPECT_EQ(NIMCP_ERROR_THEORY_OF_MIND, 8005);
    EXPECT_EQ(NIMCP_ERROR_EXPLANATIONS, 8006);
    EXPECT_EQ(NIMCP_ERROR_META_LEARNING, 8007);
    EXPECT_EQ(NIMCP_ERROR_PREDICTIVE, 8008);
}

TEST_F(ErrorCodeStabilityTest, CognitiveErrors_InCognitiveRange) {
    EXPECT_GE(NIMCP_ERROR_WORKING_MEMORY, 8000);
    EXPECT_LT(NIMCP_ERROR_WORKING_MEMORY, 9000);
    EXPECT_GE(NIMCP_ERROR_PREDICTIVE, 8000);
    EXPECT_LT(NIMCP_ERROR_PREDICTIVE, 9000);
}

//=============================================================================
// Security Errors (9000-9099)
//=============================================================================

TEST_F(ErrorCodeStabilityTest, SecurityErrors_HaveExpectedValues) {
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

TEST_F(ErrorCodeStabilityTest, SecurityErrors_InSecurityRange) {
    EXPECT_GE(NIMCP_ERROR_SECURITY_BASE, 9000);
    EXPECT_LT(NIMCP_ERROR_SECURITY_BASE, 10000);
    EXPECT_GE(NIMCP_ERROR_QUARANTINE_REQUIRED, 9000);
    EXPECT_LT(NIMCP_ERROR_QUARANTINE_REQUIRED, 10000);
}

//=============================================================================
// Brain Region Errors (10000-19999)
//=============================================================================

TEST_F(ErrorCodeStabilityTest, MotorErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_MOTOR_BASE, 10000);
    EXPECT_EQ(NIMCP_ERROR_MOTOR_NONE, 10000);
    EXPECT_EQ(NIMCP_ERROR_MOTOR_INVALID_INPUT, 10001);
    EXPECT_EQ(NIMCP_ERROR_MOTOR_PLANNING, 10002);
    EXPECT_EQ(NIMCP_ERROR_MOTOR_EXECUTION, 10003);
    EXPECT_EQ(NIMCP_ERROR_MOTOR_TRAJECTORY, 10004);
    EXPECT_EQ(NIMCP_ERROR_MOTOR_EFFECTOR, 10005);
    EXPECT_EQ(NIMCP_ERROR_MOTOR_TIMING, 10006);
    EXPECT_EQ(NIMCP_ERROR_MOTOR_BUFFER, 10007);
    EXPECT_EQ(NIMCP_ERROR_MOTOR_INTERNAL, 10008);
}

TEST_F(ErrorCodeStabilityTest, HippocampusErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_BASE, 10100);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_NONE, 10100);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_INVALID_INPUT, 10101);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_ENCODING, 10102);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_RETRIEVAL, 10103);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_NAVIGATION, 10104);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_MEMORY_FULL, 10105);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_PATTERN_SEP, 10106);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_PATTERN_COMP, 10107);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_BUFFER, 10108);
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_INTERNAL, 10109);
}

TEST_F(ErrorCodeStabilityTest, EntorhinalErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_ENTORHINAL_BASE, 10200);
    EXPECT_EQ(NIMCP_ERROR_ENTORHINAL_NONE, 10200);
    EXPECT_EQ(NIMCP_ERROR_ENTORHINAL_INVALID_INPUT, 10201);
    EXPECT_EQ(NIMCP_ERROR_ENTORHINAL_GRID_DRIFT, 10202);
    EXPECT_EQ(NIMCP_ERROR_ENTORHINAL_PATH_INTEGRATION, 10203);
}

TEST_F(ErrorCodeStabilityTest, PrefrontalErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_PREFRONTAL_BASE, 10300);
    EXPECT_EQ(NIMCP_ERROR_PREFRONTAL_NONE, 10300);
    EXPECT_EQ(NIMCP_ERROR_PREFRONTAL_INVALID_INPUT, 10301);
    EXPECT_EQ(NIMCP_ERROR_PREFRONTAL_PLANNING, 10302);
    EXPECT_EQ(NIMCP_ERROR_PREFRONTAL_WORKING_MEMORY, 10303);
    EXPECT_EQ(NIMCP_ERROR_PREFRONTAL_INHIBITION, 10304);
    EXPECT_EQ(NIMCP_ERROR_PREFRONTAL_INTERNAL, 10305);
}

TEST_F(ErrorCodeStabilityTest, CerebellumErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_CEREBELLUM_BASE, 10400);
    EXPECT_EQ(NIMCP_ERROR_CEREBELLUM_NONE, 10400);
    EXPECT_EQ(NIMCP_ERROR_CEREBELLUM_INVALID_INPUT, 10401);
    EXPECT_EQ(NIMCP_ERROR_CEREBELLUM_TIMING, 10402);
    EXPECT_EQ(NIMCP_ERROR_CEREBELLUM_PREDICTION, 10403);
    EXPECT_EQ(NIMCP_ERROR_CEREBELLUM_INTERNAL, 10404);
}

TEST_F(ErrorCodeStabilityTest, ThalamusErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_THALAMUS_BASE, 10500);
    EXPECT_EQ(NIMCP_ERROR_THALAMUS_NONE, 10500);
    EXPECT_EQ(NIMCP_ERROR_THALAMUS_INVALID_INPUT, 10501);
    EXPECT_EQ(NIMCP_ERROR_THALAMUS_RELAY, 10502);
    EXPECT_EQ(NIMCP_ERROR_THALAMUS_GATING, 10503);
    EXPECT_EQ(NIMCP_ERROR_THALAMUS_INTERNAL, 10504);
}

TEST_F(ErrorCodeStabilityTest, AmygdalaErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_AMYGDALA_BASE, 10700);
    EXPECT_EQ(NIMCP_ERROR_AMYGDALA_NONE, 10700);
    EXPECT_EQ(NIMCP_ERROR_AMYGDALA_INVALID_INPUT, 10701);
    EXPECT_EQ(NIMCP_ERROR_AMYGDALA_FEAR_PROCESSING, 10702);
    EXPECT_EQ(NIMCP_ERROR_AMYGDALA_EMOTIONAL_TAG, 10703);
    EXPECT_EQ(NIMCP_ERROR_AMYGDALA_INTERNAL, 10704);
}

TEST_F(ErrorCodeStabilityTest, VTAErrors_HaveExpectedValues) {
    EXPECT_EQ(NIMCP_ERROR_VTA_BASE, 11900);
    EXPECT_EQ(NIMCP_ERROR_VTA_NONE, 11900);
    EXPECT_EQ(NIMCP_ERROR_VTA_INVALID_INPUT, 11901);
    EXPECT_EQ(NIMCP_ERROR_VTA_REWARD, 11902);
    EXPECT_EQ(NIMCP_ERROR_VTA_DOPAMINE, 11903);
    EXPECT_EQ(NIMCP_ERROR_VTA_INTERNAL, 11904);
}

//=============================================================================
// Error Code to String Conversion
//=============================================================================

TEST_F(ErrorCodeStabilityTest, ErrorToString_Success_ReturnsNonEmpty) {
    const char* msg = nimcp_error_to_string(NIMCP_SUCCESS);
    ASSERT_NE(msg, nullptr);
    EXPECT_GT(strlen(msg), 0);
}

TEST_F(ErrorCodeStabilityTest, ErrorToString_AllCategories_ReturnNonEmpty) {
    // Test one code from each major category
    const nimcp_error_t codes[] = {
        NIMCP_SUCCESS,                     // Success (0)
        NIMCP_ERROR_UNKNOWN,               // Generic (1000)
        NIMCP_ERROR_GPU,                   // GPU (1100)
        NIMCP_ERROR_NO_MEMORY,             // Memory (2000)
        NIMCP_ERROR_BRAIN_CREATION,        // Brain (3000)
        NIMCP_ERROR_FILE_NOT_FOUND,        // IO (4000)
        NIMCP_ERROR_CONFIG_INVALID,        // Config (5000)
        NIMCP_ERROR_THREAD_CREATE,         // Threading (6000)
        NIMCP_ERROR_SIGNAL_RECEIVED,       // Signal (7000)
        NIMCP_ERROR_WORKING_MEMORY,        // Cognitive (8000)
        NIMCP_ERROR_SECURITY_BASE,         // Security (9000)
        NIMCP_ERROR_MOTOR_BASE,            // Brain Region (10000)
    };

    for (nimcp_error_t code : codes) {
        const char* msg = nimcp_error_to_string(code);
        ASSERT_NE(msg, nullptr) << "Error code " << code << " returned NULL string";
        EXPECT_GT(strlen(msg), 0) << "Error code " << code << " returned empty string";
    }
}

TEST_F(ErrorCodeStabilityTest, ErrorToString_UnknownCode_ReturnsNonNull) {
    // Unknown codes should still return something useful
    const char* msg = nimcp_error_to_string(999999);
    ASSERT_NE(msg, nullptr);
}

//=============================================================================
// Error Category Name Conversion
//=============================================================================

TEST_F(ErrorCodeStabilityTest, ErrorGetCategoryName_AllCategories_ReturnNonEmpty) {
    const nimcp_error_t codes[] = {
        NIMCP_SUCCESS,
        NIMCP_ERROR_UNKNOWN,
        NIMCP_ERROR_NO_MEMORY,
        NIMCP_ERROR_BRAIN_CREATION,
        NIMCP_ERROR_FILE_NOT_FOUND,
        NIMCP_ERROR_CONFIG_INVALID,
        NIMCP_ERROR_THREAD_CREATE,
        NIMCP_ERROR_SIGNAL_RECEIVED,
        NIMCP_ERROR_WORKING_MEMORY,
        NIMCP_ERROR_SECURITY_BASE,
        NIMCP_ERROR_MOTOR_BASE,
    };

    for (nimcp_error_t code : codes) {
        const char* name = nimcp_error_get_category_name(code);
        ASSERT_NE(name, nullptr) << "Error code " << code << " returned NULL category name";
        EXPECT_GT(strlen(name), 0) << "Error code " << code << " returned empty category name";
    }
}

//=============================================================================
// Error Code Conversion Functions
//=============================================================================

TEST_F(ErrorCodeStabilityTest, MotorErrorConversion_RoundTrip) {
    // Test that conversion to/from nimcp_error_t preserves information
    for (int local = 0; local <= 8; local++) {
        nimcp_error_t nimcp_err = motor_error_to_nimcp(local);
        int back = nimcp_to_motor_error(nimcp_err);
        EXPECT_EQ(back, local) << "Round-trip conversion failed for motor error " << local;
    }
}

TEST_F(ErrorCodeStabilityTest, HippocampusErrorConversion_RoundTrip) {
    for (int local = 0; local <= 9; local++) {
        nimcp_error_t nimcp_err = hippocampus_error_to_nimcp(local);
        int back = nimcp_to_hippocampus_error(nimcp_err);
        EXPECT_EQ(back, local) << "Round-trip conversion failed for hippocampus error " << local;
    }
}

TEST_F(ErrorCodeStabilityTest, EntorhinalErrorConversion_RoundTrip) {
    for (int local = 0; local <= 10; local++) {
        nimcp_error_t nimcp_err = entorhinal_error_to_nimcp(local);
        int back = nimcp_to_entorhinal_error(nimcp_err);
        EXPECT_EQ(back, local) << "Round-trip conversion failed for entorhinal error " << local;
    }
}

//=============================================================================
// nimcp.h Status Codes Stability (Public API)
//=============================================================================

TEST_F(ErrorCodeStabilityTest, NimcpStatus_HaveExpectedValues) {
    // These are the public API status codes from nimcp.h
    // They MUST NOT CHANGE as they are part of the stable public API
    EXPECT_EQ(NIMCP_OK, 0);
    EXPECT_EQ(NIMCP_ERROR, 1000);
    EXPECT_EQ(NIMCP_ERROR_NULL_ARG, 1003);
    EXPECT_EQ(NIMCP_ERROR_INVALID, 1004);
    EXPECT_EQ(NIMCP_ERROR_MEMORY, 2000);
    /* Note: NIMCP_ERROR_IO is 4010 in nimcp_error_codes.h (macro takes precedence over enum) */
    EXPECT_EQ(NIMCP_ERROR_IO, 4010);
}

TEST_F(ErrorCodeStabilityTest, NimcpStatus_SuccessAlias) {
    // NIMCP_SUCCESS should be an alias for NIMCP_OK
    EXPECT_EQ(NIMCP_SUCCESS, NIMCP_OK);
}

//=============================================================================
// Brain Region Error Detection
//=============================================================================

TEST_F(ErrorCodeStabilityTest, ErrorIsBrainRegion_DetectsCorrectly) {
    // Brain region errors should be detected
    EXPECT_TRUE(nimcp_error_is_brain_region(NIMCP_ERROR_MOTOR_BASE));
    EXPECT_TRUE(nimcp_error_is_brain_region(NIMCP_ERROR_HIPPOCAMPUS_BASE));
    EXPECT_TRUE(nimcp_error_is_brain_region(NIMCP_ERROR_VTA_BASE));
    EXPECT_TRUE(nimcp_error_is_brain_region(10500));  // Arbitrary code in range

    // Non-brain-region errors should not be detected
    EXPECT_FALSE(nimcp_error_is_brain_region(NIMCP_SUCCESS));
    EXPECT_FALSE(nimcp_error_is_brain_region(NIMCP_ERROR_NULL_POINTER));
    EXPECT_FALSE(nimcp_error_is_brain_region(NIMCP_ERROR_NO_MEMORY));
    EXPECT_FALSE(nimcp_error_is_brain_region(NIMCP_ERROR_SECURITY_BASE));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
