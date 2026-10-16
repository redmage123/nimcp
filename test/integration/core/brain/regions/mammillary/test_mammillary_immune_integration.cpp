/**
 * @file test_mammillary_immune_integration.cpp
 * @brief Integration tests for Mammillary Bodies with immune system
 *
 * WHAT: Tests Mammillary Bodies integration with brain immune system
 * WHY:  Ensure proper health monitoring and anomaly detection
 * HOW:  Test immune bridge initialization and health metrics
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

class MammillaryImmuneIntegrationTest : public ::testing::Test {
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
 * IMMUNE BRIDGE TESTS
 *===========================================================================*/

TEST_F(MammillaryImmuneIntegrationTest, InitImmuneBridge) {
    int result = mammillary_init_immune_bridge(mammillary, nullptr);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryImmuneIntegrationTest, ImmuneBridgeWithContext) {
    /* Initialize with nullptr context - should still work */
    mammillary_init_immune_bridge(mammillary, nullptr);

    mammillary_status_t status = mammillary_get_status(mammillary);
    EXPECT_NE(status, MAMMILLARY_STATUS_ERROR);
}

/*=============================================================================
 * HEALTH STATUS TESTS
 *===========================================================================*/

TEST_F(MammillaryImmuneIntegrationTest, HealthStatusInitial) {
    float health = mammillary_get_health_status(mammillary);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(MammillaryImmuneIntegrationTest, HealthStatusAfterOperations) {
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    for (int i = 0; i < 10; i++) {
        uint32_t trace_id = 0;
        mammillary_receive_hippocampal_input(mammillary, trace, 128,
            MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);
        mammillary_update(mammillary, 10.0f);
    }

    float health = mammillary_get_health_status(mammillary);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(MammillaryImmuneIntegrationTest, CircuitIntegrityTracked) {
    float integrity = mammillary_get_circuit_integrity(mammillary);
    EXPECT_GE(integrity, 0.0f);
    EXPECT_LE(integrity, 1.0f);
}

/*=============================================================================
 * ERROR RECOVERY TESTS
 *===========================================================================*/

TEST_F(MammillaryImmuneIntegrationTest, RecoverFromInvalidInput) {
    /* Try invalid operations */
    mammillary_receive_hippocampal_input(mammillary, nullptr, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, nullptr);

    /* Should still be functional */
    mammillary_status_t status = mammillary_get_status(mammillary);
    EXPECT_GE((int)status, 0);

    /* Should be able to continue normal operations */
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);
    uint32_t trace_id = 0;
    int result = mammillary_receive_hippocampal_input(mammillary, trace, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryImmuneIntegrationTest, HealthAfterReset) {
    /* Perform operations */
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);
    uint32_t trace_id = 0;
    mammillary_receive_hippocampal_input(mammillary, trace, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);

    /* Reset */
    mammillary_reset(mammillary);

    /* Health should still be valid */
    float health = mammillary_get_health_status(mammillary);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

/*=============================================================================
 * STRESS TESTS
 *===========================================================================*/

TEST_F(MammillaryImmuneIntegrationTest, HealthUnderLoad) {
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    for (int i = 0; i < 100; i++) {
        uint32_t trace_id = 0;
        mammillary_receive_hippocampal_input(mammillary, trace, 128,
            MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);
        mammillary_relay_to_thalamus(mammillary, trace_id);
        mammillary_update(mammillary, 1.0f);
    }

    float health = mammillary_get_health_status(mammillary);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(MammillaryImmuneIntegrationTest, IntegrityUnderPapezCycles) {
    for (int i = 0; i < 50; i++) {
        mammillary_process_papez_cycle(mammillary);
    }

    float integrity = mammillary_get_circuit_integrity(mammillary);
    EXPECT_GE(integrity, 0.0f);
    EXPECT_LE(integrity, 1.0f);
}

/*=============================================================================
 * DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(MammillaryImmuneIntegrationTest, DiagnosticsIncludeHealth) {
    int result = mammillary_log_diagnostics(mammillary);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryImmuneIntegrationTest, StatsAfterImmuneOperations) {
    mammillary_init_immune_bridge(mammillary, nullptr);

    for (int i = 0; i < 10; i++) {
        mammillary_update(mammillary, 10.0f);
    }

    mammillary_stats_t stats;
    EXPECT_EQ(0, mammillary_get_stats(mammillary, &stats));
    EXPECT_GE(stats.updates_processed, 10u);
}

/*=============================================================================
 * DECAY AND MAINTENANCE TESTS
 *===========================================================================*/

TEST_F(MammillaryImmuneIntegrationTest, TraceDecay) {
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    for (int i = 0; i < 5; i++) {
        uint32_t trace_id = 0;
        mammillary_receive_hippocampal_input(mammillary, trace, 128,
            MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);
    }

    int result = mammillary_decay_traces(mammillary, 0.1f);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryImmuneIntegrationTest, TraceRemoval) {
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    uint32_t trace_id = 0;
    ASSERT_EQ(0, mammillary_receive_hippocampal_input(mammillary, trace, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id));

    int result = mammillary_remove_trace(mammillary, trace_id);
    EXPECT_EQ(0, result);

    /* Trace should no longer exist */
    const nimcp_memory_trace_t* stored = mammillary_get_trace(mammillary, trace_id);
    EXPECT_EQ(nullptr, stored);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
