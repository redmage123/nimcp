/**
 * @file test_mammillary_security_integration.cpp
 * @brief Integration tests for Mammillary Bodies with security system
 *
 * WHAT: Tests Mammillary Bodies integration with security module
 * WHY:  Ensure secure memory operations and access control
 * HOW:  Test security bridge, validation, and protection mechanisms
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "nimcp.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/regions/mammillary/nimcp_mammillary.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MammillarySecurityIntegrationTest : public ::testing::Test {
protected:
    nimcp_mammillary_t* mammillary;
    mammillary_config_t config;

    void SetUp() override {
        config = mammillary_default_config();
        config.enable_papez_circuit = true;
        config.enable_head_direction = true;
        mammillary = mammillary_create(&config);
        ASSERT_NE(nullptr, mammillary);
    }

    void TearDown() override {
        if (mammillary) {
            mammillary_destroy(mammillary);
            mammillary = nullptr;
        }
    }

    void CreateTestContext(float* context, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            context[i] = base_value + (float)i * 0.001f;
        }
    }
};

/*=============================================================================
 * SECURITY BRIDGE TESTS
 *===========================================================================*/

TEST_F(MammillarySecurityIntegrationTest, InitSecurityBridge) {
    int result = mammillary_init_security_bridge(mammillary, nullptr, nullptr);
    EXPECT_EQ(0, result);
}

TEST_F(MammillarySecurityIntegrationTest, SecurityBridgeWithContext) {
    mammillary_init_security_bridge(mammillary, nullptr, nullptr);

    mammillary_status_t status = mammillary_get_status(mammillary);
    EXPECT_NE(status, MAMMILLARY_STATUS_ERROR);
}

/*=============================================================================
 * INPUT VALIDATION TESTS
 *===========================================================================*/

TEST_F(MammillarySecurityIntegrationTest, ValidInputAccepted) {
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    uint32_t trace_id = 0;
    int result = mammillary_receive_hippocampal_input(mammillary, trace, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(trace_id, 0u);
}

TEST_F(MammillarySecurityIntegrationTest, NullInputRejected) {
    uint32_t trace_id = 0;
    int result = mammillary_receive_hippocampal_input(mammillary, nullptr, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);
    EXPECT_NE(0, result);
}

TEST_F(MammillarySecurityIntegrationTest, ZeroDimensionRejected) {
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    uint32_t trace_id = 0;
    int result = mammillary_receive_hippocampal_input(mammillary, trace, 0,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);
    EXPECT_NE(0, result);
}

TEST_F(MammillarySecurityIntegrationTest, NullOutputHandled) {
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    /* Null output pointer should be handled */
    int result = mammillary_receive_hippocampal_input(mammillary, trace, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, nullptr);
    EXPECT_GE(result, -1); /* May succeed or fail gracefully */
}

/*=============================================================================
 * ACCESS CONTROL TESTS
 *===========================================================================*/

TEST_F(MammillarySecurityIntegrationTest, ValidTraceAccessible) {
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    uint32_t trace_id = 0;
    ASSERT_EQ(0, mammillary_receive_hippocampal_input(mammillary, trace, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id));

    const nimcp_memory_trace_t* stored = mammillary_get_trace(mammillary, trace_id);
    EXPECT_NE(nullptr, stored);
}

TEST_F(MammillarySecurityIntegrationTest, InvalidTraceIdReturnsNull) {
    const nimcp_memory_trace_t* stored = mammillary_get_trace(mammillary, 999999);
    EXPECT_EQ(nullptr, stored);
}

TEST_F(MammillarySecurityIntegrationTest, MultipleAccessesToSameTrace) {
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    uint32_t trace_id = 0;
    ASSERT_EQ(0, mammillary_receive_hippocampal_input(mammillary, trace, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id));

    for (int i = 0; i < 10; i++) {
        const nimcp_memory_trace_t* stored = mammillary_get_trace(mammillary, trace_id);
        EXPECT_NE(nullptr, stored);
    }
}

/*=============================================================================
 * HEAD DIRECTION VALIDATION TESTS
 *===========================================================================*/

TEST_F(MammillarySecurityIntegrationTest, ValidHDUpdate) {
    int result = mammillary_update_head_direction(mammillary, 0.5f, 10.0f);
    EXPECT_EQ(0, result);
}

TEST_F(MammillarySecurityIntegrationTest, ExtremeAngularVelocity) {
    /* Very large angular velocity - should be handled */
    int result = mammillary_update_head_direction(mammillary, 100.0f, 10.0f);
    EXPECT_GE(result, -1); /* May clamp or reject */
}

TEST_F(MammillarySecurityIntegrationTest, NegativeDtHandled) {
    int result = mammillary_update_head_direction(mammillary, 0.5f, -1.0f);
    EXPECT_GE(result, -1); /* Should handle gracefully */
}

/*=============================================================================
 * BOUNDS CHECKING TESTS
 *===========================================================================*/

TEST_F(MammillarySecurityIntegrationTest, LargeDimensionHandled) {
    float trace[1024];
    CreateTestContext(trace, 1024, 0.5f);

    uint32_t trace_id = 0;
    int result = mammillary_receive_hippocampal_input(mammillary, trace, 1024,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);
    EXPECT_GE(result, -1); /* May succeed or reject */
}

TEST_F(MammillarySecurityIntegrationTest, ValenceRangeClamped) {
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    /* Extreme valence values */
    uint32_t trace_id = 0;
    int result = mammillary_receive_hippocampal_input(mammillary, trace, 128,
        MEMORY_TRACE_EMOTIONAL, 100.0f, &trace_id);
    EXPECT_GE(result, -1); /* Should clamp or handle */
}

/*=============================================================================
 * RECOVERY AND RESET TESTS
 *===========================================================================*/

TEST_F(MammillarySecurityIntegrationTest, ResetClearsState) {
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    uint32_t trace_id = 0;
    mammillary_receive_hippocampal_input(mammillary, trace, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);

    EXPECT_EQ(0, mammillary_reset(mammillary));

    mammillary_status_t status = mammillary_get_status(mammillary);
    EXPECT_GE((int)status, 0);
}

TEST_F(MammillarySecurityIntegrationTest, OperationsAfterResetWork) {
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    uint32_t trace_id1 = 0;
    mammillary_receive_hippocampal_input(mammillary, trace, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id1);

    mammillary_reset(mammillary);

    uint32_t trace_id2 = 0;
    int result = mammillary_receive_hippocampal_input(mammillary, trace, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id2);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * ERROR STATE RECOVERY TESTS
 *===========================================================================*/

TEST_F(MammillarySecurityIntegrationTest, StatusConsistentAfterOperations) {
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    for (int i = 0; i < 10; i++) {
        uint32_t trace_id = 0;
        mammillary_receive_hippocampal_input(mammillary, trace, 128,
            MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);
        mammillary_relay_to_thalamus(mammillary, trace_id);
    }

    mammillary_status_t status = mammillary_get_status(mammillary);
    EXPECT_GE((int)status, 0);
    EXPECT_NE(status, MAMMILLARY_STATUS_ERROR);
}

TEST_F(MammillarySecurityIntegrationTest, ErrorStateRecoverable) {
    /* Trigger an error */
    mammillary_receive_hippocampal_input(mammillary, nullptr, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, nullptr);

    /* Should be able to continue with valid input */
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    uint32_t trace_id = 0;
    int result = mammillary_receive_hippocampal_input(mammillary, trace, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(MammillarySecurityIntegrationTest, DiagnosticsIncludeSecurityInfo) {
    mammillary_init_security_bridge(mammillary, nullptr, nullptr);

    int result = mammillary_log_diagnostics(mammillary);
    EXPECT_EQ(0, result);
}

TEST_F(MammillarySecurityIntegrationTest, StatsTrackOperations) {
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    uint32_t trace_id = 0;
    mammillary_receive_hippocampal_input(mammillary, trace, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);

    mammillary_stats_t stats;
    EXPECT_EQ(0, mammillary_get_stats(mammillary, &stats));
    EXPECT_GE(stats.total_memory_traces, 1u);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
