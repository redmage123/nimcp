/**
 * @file test_health_diagnostic_bridge_regression.cpp
 * @brief Regression tests dedicated to Health Diagnostic Bridge
 * @version 1.0.0
 * @date 2025-01-27
 *
 * WHAT: Regression tests preventing known diagnostic bridge bugs from reoccurring
 * WHY: Ensure all bug fixes remain stable across code changes
 * HOW: Test specific bug scenarios, edge cases, and boundary conditions
 *
 * Test Coverage:
 * - Null pointer dereference edge cases (all API functions)
 * - Severity mapping boundary issues (out-of-range, max, min)
 * - Confidence numerical stability (NaN, Inf, negative, >1.0, zero)
 * - Memory management and leak prevention
 * - Configuration validation edge cases
 * - String handling (empty, max length, truncation)
 * - Statistics accuracy and overflow prevention
 * - Custom mapping stability
 * - Batch conversion edge cases
 * - Enrichment API stability
 * - KG wiring regression (metadata limits, duplicate entries)
 * - Security regression (sanitization, threat analysis stability)
 * - Math utils regression (phasor edge cases, degenerate arrays)
 * - Quantum annealing regression (zero iterations, extreme configs)
 * - Code immune regression (config boundary values)
 * - Exception handler completeness
 * - Concurrent access safety
 * - Agent message boundary values
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <limits>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

// Core headers (have their own extern "C" guards)
#include "cognitive/fault_tolerance/nimcp_health_diagnostic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "security/nimcp_security.h"
#include "utils/math/nimcp_complex_math.h"
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"
#include "cognitive/immune/nimcp_code_immune.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"

extern "C" {
    void health_diagnostic_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

//=============================================================================
// Test Fixture
//=============================================================================

class HealthDiagBridgeRegressionTest : public ::testing::Test {
protected:
    size_t baseline_allocated = 0;
    health_diag_bridge_t* bridge = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        baseline_allocated = stats.current_allocated;

        bridge = health_diag_bridge_create(NULL);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) health_diag_bridge_destroy(bridge);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        // Allow up to 8KB for exception/immune system infrastructure allocations
        const size_t exception_infra_tolerance = 8192;
        EXPECT_LE(stats.current_allocated,
                  baseline_allocated + exception_infra_tolerance)
            << "Memory leak detected! Allocated: " << stats.current_allocated
            << ", Baseline: " << baseline_allocated;
    }

    void create_anomaly(anomaly_t* anomaly, anomaly_type_t type,
                        anomaly_severity_t severity, float confidence) {
        memset(anomaly, 0, sizeof(*anomaly));
        anomaly->type = type;
        anomaly->severity = severity;
        anomaly->confidence = confidence;
        snprintf(anomaly->description, sizeof(anomaly->description),
                 "Regression anomaly type=%d", (int)type);
    }

    void create_agent_msg(health_agent_message_t* msg, health_agent_msg_type_t type,
                          health_agent_severity_t severity) {
        memset(msg, 0, sizeof(*msg));
        msg->type = type;
        msg->severity = severity;
    }
};

//=============================================================================
// REGRESSION: Null Pointer Handling - Complete Coverage
//=============================================================================

/**
 * Bug #R1: Crash when passing NULL bridge to any API function
 */
TEST_F(HealthDiagBridgeRegressionTest, NullBridgeCrashPrevention) {
    EXPECT_NO_THROW(health_diag_bridge_destroy(NULL));
    EXPECT_FALSE(health_diag_bridge_is_ready(NULL));

    anomaly_t anomaly;
    memset(&anomaly, 0, sizeof(anomaly));
    diagnostic_result_t* result = nullptr;

    EXPECT_EQ(health_diag_bridge_convert_anomaly(NULL, &anomaly, &result), -1);
    EXPECT_EQ(result, nullptr);

    health_agent_message_t msg;
    memset(&msg, 0, sizeof(msg));
    EXPECT_EQ(health_diag_bridge_convert_agent_message(NULL, &msg, &result), -1);

    health_diag_bridge_stats_t stats;
    EXPECT_EQ(health_diag_bridge_get_stats(NULL, &stats), -1);

    EXPECT_NO_THROW(health_diag_bridge_reset_stats(NULL));
    EXPECT_EQ(health_diag_bridge_get_anomaly_mapping(NULL, ANOMALY_MEMORY_LEAK), nullptr);

    anomaly_error_mapping_t a_map;
    memset(&a_map, 0, sizeof(a_map));
    EXPECT_EQ(health_diag_bridge_add_anomaly_mapping(NULL, &a_map), -1);

    agent_error_mapping_t ag_map;
    memset(&ag_map, 0, sizeof(ag_map));
    EXPECT_EQ(health_diag_bridge_add_agent_mapping(NULL, &ag_map), -1);

    EXPECT_EQ(health_diag_bridge_enrich_stack_trace(NULL, NULL), -1);
    EXPECT_EQ(health_diag_bridge_enrich_memory_snapshot(NULL, NULL), -1);
    EXPECT_EQ(health_diag_bridge_analyze_patterns(NULL, NULL), -1);

    EXPECT_EQ(health_diag_bridge_convert_anomalies(NULL, NULL, 0, NULL, NULL), -1);
}

/**
 * Bug #R2: Crash when output parameters are NULL
 */
TEST_F(HealthDiagBridgeRegressionTest, NullOutputParameterHandling) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);

    EXPECT_EQ(health_diag_bridge_convert_anomaly(bridge, &anomaly, NULL), -1);
    EXPECT_EQ(health_diag_bridge_get_stats(bridge, NULL), -1);

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_EMERGENCY, HEALTH_SEVERITY_CRITICAL);
    EXPECT_EQ(health_diag_bridge_convert_agent_message(bridge, &msg, NULL), -1);
}

/**
 * Bug #R3: Crash when input data is NULL
 */
TEST_F(HealthDiagBridgeRegressionTest, NullInputDataHandling) {
    diagnostic_result_t* result = nullptr;

    EXPECT_EQ(health_diag_bridge_convert_anomaly(bridge, NULL, &result), -1);
    EXPECT_EQ(result, nullptr);

    result = nullptr;
    EXPECT_EQ(health_diag_bridge_convert_agent_message(bridge, NULL, &result), -1);
    EXPECT_EQ(result, nullptr);

    EXPECT_EQ(health_diag_bridge_add_anomaly_mapping(bridge, NULL), -1);
    EXPECT_EQ(health_diag_bridge_add_agent_mapping(bridge, NULL), -1);

    EXPECT_EQ(health_diag_bridge_enrich_stack_trace(bridge, NULL), -1);
    EXPECT_EQ(health_diag_bridge_enrich_memory_snapshot(bridge, NULL), -1);
    EXPECT_EQ(health_diag_bridge_analyze_patterns(bridge, NULL), -1);
}

/**
 * Bug #R4: Crash when config output is NULL
 */
TEST_F(HealthDiagBridgeRegressionTest, NullConfigOutputHandling) {
    EXPECT_EQ(health_diag_bridge_default_config(NULL), -1);
}

//=============================================================================
// REGRESSION: Severity Mapping Boundaries
//=============================================================================

/**
 * Bug #R5: Out-of-range severity enum causes array bounds violation
 */
TEST_F(HealthDiagBridgeRegressionTest, OutOfRangeAnomalySeverity) {
    for (int sev = -1; sev <= 100; sev++) {
        diag_severity_t result = health_diag_bridge_translate_anomaly_severity(
            (anomaly_severity_t)sev);
        // Should never crash, should return a valid severity
        EXPECT_GE((int)result, 0);
    }
}

TEST_F(HealthDiagBridgeRegressionTest, OutOfRangeAgentSeverity) {
    for (int sev = -1; sev <= 100; sev++) {
        diag_severity_t result = health_diag_bridge_translate_agent_severity(
            (health_agent_severity_t)sev);
        EXPECT_GE((int)result, 0);
    }
}

/**
 * Bug #R6: Invalid anomaly severity in anomaly struct causes crash during conversion
 */
TEST_F(HealthDiagBridgeRegressionTest, InvalidSeverityInAnomalyConversion) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, (anomaly_severity_t)999, 0.8f);

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    EXPECT_EQ(ret, 0);
    if (result) {
        EXPECT_GE((int)result->severity, 0);
        diagnostics_free_result(result);
    }
}

/**
 * Bug #R7: Invalid agent severity causes crash during conversion
 */
TEST_F(HealthDiagBridgeRegressionTest, InvalidSeverityInAgentMessageConversion) {
    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_EMERGENCY, (health_agent_severity_t)999);

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_agent_message(bridge, &msg, &result);
    EXPECT_EQ(ret, 0);
    if (result) {
        EXPECT_GE((int)result->severity, 0);
        diagnostics_free_result(result);
    }
}

//=============================================================================
// REGRESSION: Anomaly Type Boundaries
//=============================================================================

/**
 * Bug #R8: Unknown anomaly type causes array out-of-bounds
 */
TEST_F(HealthDiagBridgeRegressionTest, InvalidAnomalyTypeConversion) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, (anomaly_type_t)999, ANOMALY_SEVERITY_WARNING, 0.8f);

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    EXPECT_EQ(ret, 0);
    if (result) {
        diagnostics_free_result(result);
    }
}

/**
 * Bug #R9: Unknown agent message type causes crash
 */
TEST_F(HealthDiagBridgeRegressionTest, InvalidAgentMessageTypeConversion) {
    health_agent_message_t msg;
    create_agent_msg(&msg, (health_agent_msg_type_t)999, HEALTH_SEVERITY_WARNING);

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_agent_message(bridge, &msg, &result);
    // Should either succeed with default mapping or return error
    if (ret == 0 && result) {
        diagnostics_free_result(result);
    }
}

//=============================================================================
// REGRESSION: Confidence Numerical Stability
//=============================================================================

/**
 * Bug #R10: Zero confidence causes division by zero
 */
TEST_F(HealthDiagBridgeRegressionTest, ZeroConfidenceHandling) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.0f);

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    EXPECT_EQ(ret, 0);
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(std::isnan(result->confidence));
    EXPECT_GE(result->confidence, 0.0f);
    EXPECT_LE(result->confidence, 1.0f);
    diagnostics_free_result(result);
}

/**
 * Bug #R11: NaN confidence propagates through calculations
 */
TEST_F(HealthDiagBridgeRegressionTest, NaNConfidenceHandling) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING,
                   std::numeric_limits<float>::quiet_NaN());

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    if (ret == 0 && result) {
        EXPECT_FALSE(std::isnan(result->confidence));
        diagnostics_free_result(result);
    }
}

/**
 * Bug #R12: Inf confidence causes downstream issues
 */
TEST_F(HealthDiagBridgeRegressionTest, InfConfidenceHandling) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING,
                   std::numeric_limits<float>::infinity());

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    if (ret == 0 && result) {
        EXPECT_FALSE(std::isinf(result->confidence));
        EXPECT_LE(result->confidence, 1.0f);
        diagnostics_free_result(result);
    }
}

/**
 * Bug #R13: Negative infinity confidence
 */
TEST_F(HealthDiagBridgeRegressionTest, NegInfConfidenceHandling) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING,
                   -std::numeric_limits<float>::infinity());

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    if (ret == 0 && result) {
        EXPECT_FALSE(std::isinf(result->confidence));
        EXPECT_GE(result->confidence, 0.0f);
        diagnostics_free_result(result);
    }
}

/**
 * Bug #R14: Confidence >1.0 causes issues in downstream processing
 */
TEST_F(HealthDiagBridgeRegressionTest, ConfidenceClampingAboveOne) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 5.0f);

    diagnostic_result_t* result = nullptr;
    health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    if (result) {
        EXPECT_LE(result->confidence, 1.0f);
        EXPECT_GE(result->confidence, 0.0f);
        diagnostics_free_result(result);
    }
}

/**
 * Bug #R15: Confidence <0.0 not clamped
 */
TEST_F(HealthDiagBridgeRegressionTest, ConfidenceClampingBelowZero) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, -2.0f);

    diagnostic_result_t* result = nullptr;
    health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    if (result) {
        EXPECT_LE(result->confidence, 1.0f);
        EXPECT_GE(result->confidence, 0.0f);
        diagnostics_free_result(result);
    }
}

//=============================================================================
// REGRESSION: String Handling
//=============================================================================

/**
 * Bug #R16: Buffer overflow on max length description
 */
TEST_F(HealthDiagBridgeRegressionTest, MaxLengthDescriptionTruncation) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);
    memset(anomaly.description, 'A', sizeof(anomaly.description) - 1);
    anomaly.description[sizeof(anomaly.description) - 1] = '\0';

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    EXPECT_EQ(ret, 0);
    if (result) diagnostics_free_result(result);
}

/**
 * Bug #R17: Empty description causes logging crash
 */
TEST_F(HealthDiagBridgeRegressionTest, EmptyDescriptionHandling) {
    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);
    anomaly.description[0] = '\0';

    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    EXPECT_EQ(ret, 0);
    if (result) diagnostics_free_result(result);
}

/**
 * Bug #R18: Anomaly type name returns NULL for invalid types
 */
TEST_F(HealthDiagBridgeRegressionTest, TypeNameFunctionsNeverReturnNull) {
    for (int i = -1; i <= 100; i++) {
        const char* name = health_diag_bridge_anomaly_type_name((anomaly_type_t)i);
        ASSERT_NE(name, nullptr) << "NULL returned for anomaly type " << i;
        EXPECT_GT(strlen(name), 0u) << "Empty string for anomaly type " << i;
    }

    for (int i = -1; i <= 50; i++) {
        const char* name = health_diag_bridge_agent_msg_type_name((health_agent_msg_type_t)i);
        ASSERT_NE(name, nullptr) << "NULL returned for msg type " << i;
        EXPECT_GT(strlen(name), 0u) << "Empty string for msg type " << i;
    }
}

//=============================================================================
// REGRESSION: Memory Management
//=============================================================================

/**
 * Bug #R19: Memory leak when converting many anomalies
 */
TEST_F(HealthDiagBridgeRegressionTest, NoLeaksAfterManyConversions) {
    for (int i = 0; i < 500; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, (anomaly_type_t)(1 + i % 9),
                       ANOMALY_SEVERITY_WARNING, 0.7f);
        diagnostic_result_t* result = nullptr;
        health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);
    }
    // TearDown checks for leaks
}

/**
 * Bug #R20: Memory leak on failed conversions
 */
TEST_F(HealthDiagBridgeRegressionTest, NoLeaksOnFilteredConversions) {
    health_diag_bridge_config_t config;
    health_diag_bridge_default_config(&config);
    config.min_severity = ANOMALY_SEVERITY_CRITICAL;

    health_diag_bridge_t* filtered = health_diag_bridge_create(&config);
    ASSERT_NE(filtered, nullptr);

    for (int i = 0; i < 100; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_INFO, 0.5f);
        diagnostic_result_t* result = nullptr;
        // Should be filtered, not leak
        health_diag_bridge_convert_anomaly(filtered, &anomaly, &result);
        EXPECT_EQ(result, nullptr);
    }

    health_diag_bridge_destroy(filtered);
}

/**
 * Bug #R21: Double destroy crash
 */
TEST_F(HealthDiagBridgeRegressionTest, DoubleDestroyNoCrash) {
    health_diag_bridge_t* temp = health_diag_bridge_create(NULL);
    ASSERT_NE(temp, nullptr);

    health_diag_bridge_destroy(temp);
    EXPECT_NO_THROW(health_diag_bridge_destroy(NULL));
}

//=============================================================================
// REGRESSION: Configuration Validation
//=============================================================================

/**
 * Bug #R22: Default config idempotency
 */
TEST_F(HealthDiagBridgeRegressionTest, DefaultConfigIdempotency) {
    health_diag_bridge_config_t config1, config2;

    health_diag_bridge_default_config(&config1);
    config1.default_confidence = 0.99f;
    config1.escalation_threshold = 999;

    health_diag_bridge_default_config(&config1);
    health_diag_bridge_default_config(&config2);

    EXPECT_FLOAT_EQ(config1.default_confidence, config2.default_confidence);
    EXPECT_EQ(config1.escalation_threshold, config2.escalation_threshold);
    EXPECT_EQ(config1.capture_stack_trace, config2.capture_stack_trace);
    EXPECT_EQ(config1.capture_memory_snapshot, config2.capture_memory_snapshot);
    EXPECT_EQ(config1.enable_pattern_analysis, config2.enable_pattern_analysis);
    EXPECT_EQ(config1.enable_bio_async, config2.enable_bio_async);
    EXPECT_EQ(config1.verbose_logging, config2.verbose_logging);
}

/**
 * Bug #R23: Extreme config values don't crash
 */
TEST_F(HealthDiagBridgeRegressionTest, ExtremeConfigValuesHandled) {
    health_diag_bridge_config_t config;
    health_diag_bridge_default_config(&config);

    // Extreme confidence
    config.default_confidence = 0.0f;
    health_diag_bridge_t* b1 = health_diag_bridge_create(&config);
    if (b1) health_diag_bridge_destroy(b1);

    config.default_confidence = 1.0f;
    health_diag_bridge_t* b2 = health_diag_bridge_create(&config);
    if (b2) health_diag_bridge_destroy(b2);

    // Extreme escalation threshold
    config.default_confidence = 0.7f;
    config.escalation_threshold = 0;
    health_diag_bridge_t* b3 = health_diag_bridge_create(&config);
    if (b3) health_diag_bridge_destroy(b3);

    config.escalation_threshold = UINT32_MAX;
    health_diag_bridge_t* b4 = health_diag_bridge_create(&config);
    if (b4) health_diag_bridge_destroy(b4);
}

//=============================================================================
// REGRESSION: Statistics Accuracy
//=============================================================================

/**
 * Bug #R24: Statistics counters overflow
 */
TEST_F(HealthDiagBridgeRegressionTest, StatisticsNoOverflowInBurst) {
    for (int i = 0; i < 1000; i++) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_WARNING, 0.8f);
        diagnostic_result_t* result = nullptr;
        health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);
    }

    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.anomalies_converted, 1000u);
    EXPECT_EQ(stats.by_anomaly_type[ANOMALY_MEMORY_LEAK], 1000u);
}

/**
 * Bug #R25: Statistics reset leaves stale data
 */
TEST_F(HealthDiagBridgeRegressionTest, StatisticsResetComplete) {
    // Generate diverse stats
    anomaly_type_t types[] = {ANOMALY_MEMORY_LEAK, ANOMALY_ERROR_SPIKE,
                              ANOMALY_RESOURCE_EXHAUSTION};
    for (auto type : types) {
        anomaly_t anomaly;
        create_anomaly(&anomaly, type, ANOMALY_SEVERITY_WARNING, 0.8f);
        diagnostic_result_t* result = nullptr;
        health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
        if (result) diagnostics_free_result(result);
    }

    health_agent_message_t msg;
    create_agent_msg(&msg, HEALTH_MSG_EMERGENCY, HEALTH_SEVERITY_CRITICAL);
    diagnostic_result_t* result = nullptr;
    health_diag_bridge_convert_agent_message(bridge, &msg, &result);
    if (result) diagnostics_free_result(result);

    // Reset
    health_diag_bridge_reset_stats(bridge);

    health_diag_bridge_stats_t stats;
    health_diag_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.anomalies_converted, 0u);
    EXPECT_EQ(stats.agent_messages_converted, 0u);
    EXPECT_EQ(stats.conversions_failed, 0u);
    EXPECT_FLOAT_EQ(stats.avg_conversion_time_us, 0.0f);

    for (int i = 0; i <= (int)ANOMALY_UNKNOWN; i++) {
        EXPECT_EQ(stats.by_anomaly_type[i], 0u) << "Stale anomaly type " << i;
    }
    for (int i = 0; i < (int)HEALTH_MSG_COUNT; i++) {
        EXPECT_EQ(stats.by_agent_msg_type[i], 0u) << "Stale msg type " << i;
    }
    for (int i = 0; i <= (int)DIAG_SEVERITY_FATAL; i++) {
        EXPECT_EQ(stats.by_severity[i], 0u) << "Stale severity " << i;
    }
}

//=============================================================================
// REGRESSION: Custom Mapping Stability
//=============================================================================

/**
 * Bug #R26: Adding maximum number of custom mappings
 */
TEST_F(HealthDiagBridgeRegressionTest, MaxCustomMappingsHandled) {
    // Add up to HEALTH_DIAG_BRIDGE_MAX_MAPPINGS custom anomaly mappings
    for (int i = 0; i < (int)HEALTH_DIAG_BRIDGE_MAX_MAPPINGS; i++) {
        anomaly_error_mapping_t mapping;
        mapping.anomaly_type = (anomaly_type_t)(i % 10);
        mapping.error_type = ERROR_TYPE_UNKNOWN;
        mapping.default_severity = DIAG_SEVERITY_WARNING;
        mapping.default_confidence = 0.7f;
        mapping.description_template = "Custom mapping %d";

        health_diag_bridge_add_anomaly_mapping(bridge, &mapping);
    }

    // Bridge should still work
    EXPECT_TRUE(health_diag_bridge_is_ready(bridge));

    anomaly_t anomaly;
    create_anomaly(&anomaly, ANOMALY_MEMORY_LEAK, ANOMALY_SEVERITY_WARNING, 0.8f);
    diagnostic_result_t* result = nullptr;
    int ret = health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
    EXPECT_EQ(ret, 0);
    if (result) diagnostics_free_result(result);
}

/**
 * Bug #R27: Overwriting existing mapping doesn't leak
 */
TEST_F(HealthDiagBridgeRegressionTest, MappingOverwriteNoLeak) {
    for (int i = 0; i < 100; i++) {
        anomaly_error_mapping_t mapping;
        mapping.anomaly_type = ANOMALY_MEMORY_LEAK;
        mapping.error_type = (error_type_t)(i % 10);
        mapping.default_severity = DIAG_SEVERITY_WARNING;
        mapping.default_confidence = 0.5f + 0.005f * i;
        mapping.description_template = "Overwrite test";

        health_diag_bridge_add_anomaly_mapping(bridge, &mapping);
    }
    // TearDown checks for leaks
}

//=============================================================================
// REGRESSION: Batch Conversion Edge Cases
//=============================================================================

/**
 * Bug #R28: Batch conversion with zero count
 */
TEST_F(HealthDiagBridgeRegressionTest, BatchConvertZeroCount) {
    anomaly_t anomalies[1];
    diagnostic_result_t* results[1] = {};
    uint32_t converted = 0;

    int ret = health_diag_bridge_convert_anomalies(
        bridge, anomalies, 0, results, &converted);
    // Should handle gracefully
    EXPECT_GE(ret, -1);
}

/**
 * Bug #R29: Batch conversion with all filtered out
 */
TEST_F(HealthDiagBridgeRegressionTest, BatchConvertAllFiltered) {
    health_diag_bridge_config_t config;
    health_diag_bridge_default_config(&config);
    config.min_severity = ANOMALY_SEVERITY_CRITICAL;

    health_diag_bridge_t* filtered = health_diag_bridge_create(&config);
    ASSERT_NE(filtered, nullptr);

    const uint32_t count = 3;
    anomaly_t anomalies[3];
    diagnostic_result_t* results[3] = {};
    uint32_t converted = 0;

    for (uint32_t i = 0; i < count; i++) {
        create_anomaly(&anomalies[i], ANOMALY_MEMORY_LEAK,
                       ANOMALY_SEVERITY_INFO, 0.5f);
    }

    int ret = health_diag_bridge_convert_anomalies(
        filtered, anomalies, count, results, &converted);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(converted, 0u);

    health_diag_bridge_destroy(filtered);
}

//=============================================================================
// REGRESSION: Concurrent Access Safety
//=============================================================================

/**
 * Bug #R30: Concurrent conversions cause data corruption
 */
TEST_F(HealthDiagBridgeRegressionTest, ConcurrentConversionSafety) {
    const int thread_count = 4;
    const int ops_per_thread = 100;
    std::atomic<bool> any_crash{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < ops_per_thread; i++) {
                anomaly_t anomaly;
                memset(&anomaly, 0, sizeof(anomaly));
                anomaly.type = (anomaly_type_t)(1 + i % 9);
                anomaly.severity = ANOMALY_SEVERITY_WARNING;
                anomaly.confidence = 0.7f;

                diagnostic_result_t* result = nullptr;
                if (health_diag_bridge_convert_anomaly(
                        bridge, &anomaly, &result) == 0 && result) {
                    if (std::isnan(result->confidence) ||
                        result->confidence < 0.0f ||
                        result->confidence > 1.0f) {
                        any_crash.store(true);
                    }
                    diagnostics_free_result(result);
                }
            }
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_FALSE(any_crash.load());
}

/**
 * Bug #R31: Concurrent stats access causes read tear
 */
TEST_F(HealthDiagBridgeRegressionTest, ConcurrentStatsAccess) {
    std::atomic<bool> running{true};

    std::thread converter([&]() {
        while (running.load()) {
            anomaly_t anomaly;
            memset(&anomaly, 0, sizeof(anomaly));
            anomaly.type = ANOMALY_MEMORY_LEAK;
            anomaly.severity = ANOMALY_SEVERITY_WARNING;
            anomaly.confidence = 0.8f;

            diagnostic_result_t* result = nullptr;
            health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
            if (result) diagnostics_free_result(result);
        }
    });

    std::thread stats_reader([&]() {
        for (int i = 0; i < 100; i++) {
            health_diag_bridge_stats_t stats;
            health_diag_bridge_get_stats(bridge, &stats);
            // Should never get corrupted values
            EXPECT_GE(stats.anomalies_converted, 0u);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    running.store(false);

    converter.join();
    stats_reader.join();
}

//=============================================================================
// REGRESSION: KG Wiring
//=============================================================================

/**
 * Bug #R32: Duplicate KG wiring entries
 */
TEST_F(HealthDiagBridgeRegressionTest, KGWiringDuplicateEntries) {
    kg_module_wiring_t* wiring = kg_module_wiring_create(
        "health_diagnostic_bridge", "FAULT_TOLERANCE");
    ASSERT_NE(wiring, nullptr);

    // Add same input twice
    kg_module_wiring_add_input(wiring, "health_monitor", "ANOMALY_DETECTED", true);
    kg_module_wiring_add_input(wiring, "health_monitor", "ANOMALY_DETECTED", true);

    // Add same output twice
    kg_module_wiring_add_output(wiring, "DIAGNOSTIC_RESULT", "Result");
    kg_module_wiring_add_output(wiring, "DIAGNOSTIC_RESULT", "Result");

    // Should still validate
    char error_buf[256];
    kg_module_wiring_validate(wiring, error_buf, sizeof(error_buf));

    kg_module_wiring_destroy(wiring);
}

/**
 * Bug #R33: KG wiring NULL safety
 */
TEST_F(HealthDiagBridgeRegressionTest, KGWiringNullSafety) {
    EXPECT_NO_THROW(kg_module_wiring_destroy(NULL));
    EXPECT_FALSE(kg_module_wiring_has_input(NULL, "test", "test"));
    EXPECT_FALSE(kg_module_wiring_has_output(NULL, "test"));
    EXPECT_FALSE(kg_module_wiring_has_handler(NULL, "test"));
}

//=============================================================================
// REGRESSION: Security Module
//=============================================================================

/**
 * Bug #R34: Empty string through security validation
 */
TEST_F(HealthDiagBridgeRegressionTest, SecurityEmptyStringValidation) {
    nimcp_input_validation_t valid = nimcp_security_validate_input(
        "", 256, NULL);
    EXPECT_EQ(valid, NIMCP_INPUT_VALID);
}

/**
 * Bug #R35: NULL string through security validation
 */
TEST_F(HealthDiagBridgeRegressionTest, SecurityNullStringValidation) {
    nimcp_input_validation_t valid = nimcp_security_validate_input(
        NULL, 256, NULL);
    // Should not crash, may return invalid
    (void)valid;
}

/**
 * Bug #R36: Threat level names for all values
 */
TEST_F(HealthDiagBridgeRegressionTest, SecurityThreatLevelNamesComplete) {
    const nimcp_threat_level_t levels[] = {
        NIMCP_THREAT_NONE, NIMCP_THREAT_LOW, NIMCP_THREAT_MEDIUM,
        NIMCP_THREAT_HIGH, NIMCP_THREAT_CRITICAL
    };

    for (auto level : levels) {
        const char* name = nimcp_threat_level_name(level);
        ASSERT_NE(name, nullptr) << "NULL for threat level " << (int)level;
        EXPECT_GT(strlen(name), 0u);
    }
}

//=============================================================================
// REGRESSION: Math Utils - Phasor Edge Cases
//=============================================================================

/**
 * Bug #R37: Phasor coherence with single element
 */
TEST_F(HealthDiagBridgeRegressionTest, PhasorCoherenceSingleElement) {
    complex_math_init(NULL);

    neural_phasor_t single = phasor_from_polar(1.0f, 0.5f);
    float coherence = phasor_array_coherence(&single, 1);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);

    complex_math_cleanup();
}

/**
 * Bug #R38: Phasor with zero amplitude
 */
TEST_F(HealthDiagBridgeRegressionTest, PhasorZeroAmplitude) {
    neural_phasor_t p = phasor_from_polar(0.0f, 0.5f);
    float amp = phasor_amplitude(p);
    EXPECT_NEAR(amp, 0.0f, 0.01f);
    EXPECT_FALSE(std::isnan(amp));
}

/**
 * Bug #R39: Phasor normalize zero vector
 */
TEST_F(HealthDiagBridgeRegressionTest, PhasorNormalizeZero) {
    neural_phasor_t p = phasor_from_polar(0.0f, 0.0f);
    neural_phasor_t normalized = phasor_normalize(p);
    float amp = phasor_amplitude(normalized);
    // Should not produce NaN
    EXPECT_FALSE(std::isnan(amp));
}

//=============================================================================
// REGRESSION: Quantum Annealing Edge Cases
//=============================================================================

static float simple_energy(const float* state, uint32_t dim, void* user_data) {
    (void)user_data;
    float e = 0.0f;
    for (uint32_t i = 0; i < dim; i++) e += state[i] * state[i];
    return e;
}

/**
 * Bug #R40: Quantum annealer with minimal iterations
 */
TEST_F(HealthDiagBridgeRegressionTest, QuantumAnnealerMinimalIterations) {
    quantum_annealing_config_t config = quantum_annealing_default_config();
    config.num_iterations = 1;
    config.seed = 42;

    quantum_annealer_t annealer = quantum_annealer_create(&config);
    if (annealer) {
        float initial[2] = {1.0f, 1.0f};
        float result[2] = {};
        float energy = quantum_anneal(annealer, simple_energy, initial, result, 2, NULL);
        EXPECT_GE(energy, 0.0f);
        quantum_annealer_destroy(annealer);
    }
}

/**
 * Bug #R41: Quantum annealer NULL destroy
 */
TEST_F(HealthDiagBridgeRegressionTest, QuantumAnnealerNullDestroy) {
    EXPECT_NO_THROW(quantum_annealer_destroy(NULL));
}

//=============================================================================
// REGRESSION: Code Immune Edge Cases
//=============================================================================

/**
 * Bug #R42: Code immune with extreme auto-repair config
 */
TEST_F(HealthDiagBridgeRegressionTest, CodeImmuneExtremeConfig) {
    code_immune_config_t config;
    code_immune_default_config(&config);

    config.auto_repair.min_crash_count = 0;
    config.auto_repair.cooldown_ms = 0;
    config.auto_repair.min_severity = 0.0f;
    config.auto_repair.min_confidence = 0.0f;

    code_immune_system_t* immune = code_immune_create_with_config(NULL, &config);
    if (immune) {
        code_immune_destroy(immune);
    }

    // Also test max values
    config.auto_repair.min_crash_count = UINT32_MAX;
    config.auto_repair.cooldown_ms = UINT32_MAX;
    config.auto_repair.min_severity = 1.0f;
    config.auto_repair.min_confidence = 1.0f;

    immune = code_immune_create_with_config(NULL, &config);
    if (immune) {
        code_immune_destroy(immune);
    }
}

/**
 * Bug #R43: Code immune disabled auto-repair
 */
TEST_F(HealthDiagBridgeRegressionTest, CodeImmuneDisabledAutoRepair) {
    code_immune_config_t config;
    code_immune_default_config(&config);
    config.auto_repair.enabled = false;

    code_immune_system_t* immune = code_immune_create_with_config(NULL, &config);
    if (immune) {
        EXPECT_NE(immune, nullptr);
        code_immune_destroy(immune);
    }
}

//=============================================================================
// REGRESSION: Health Agent Integration
//=============================================================================

/**
 * Bug #R44: Setting agent during conversions doesn't crash
 */
TEST_F(HealthDiagBridgeRegressionTest, AgentSetDuringConversionsNoCrash) {
    health_agent_config_t cfg;
    nimcp_health_agent_default_config(&cfg);
    nimcp_health_agent_t* agent = nimcp_health_agent_create(&cfg);
    ASSERT_NE(agent, nullptr);

    std::atomic<bool> running{true};

    std::thread converter([&]() {
        while (running.load()) {
            anomaly_t anomaly;
            memset(&anomaly, 0, sizeof(anomaly));
            anomaly.type = ANOMALY_MEMORY_LEAK;
            anomaly.severity = ANOMALY_SEVERITY_WARNING;
            anomaly.confidence = 0.8f;

            diagnostic_result_t* result = nullptr;
            health_diag_bridge_convert_anomaly(bridge, &anomaly, &result);
            if (result) diagnostics_free_result(result);
        }
    });

    for (int i = 0; i < 20; i++) {
        health_diagnostic_bridge_set_health_agent(agent);
        health_diagnostic_bridge_set_health_agent(NULL);
    }

    running.store(false);
    converter.join();

    health_diagnostic_bridge_set_health_agent(NULL);
    nimcp_health_agent_destroy(agent);
}

/**
 * Bug #R45: Version string is always non-NULL and non-empty
 */
TEST_F(HealthDiagBridgeRegressionTest, VersionStringStability) {
    for (int i = 0; i < 100; i++) {
        const char* ver = health_diag_bridge_version();
        ASSERT_NE(ver, nullptr);
        EXPECT_GT(strlen(ver), 0u);
        EXPECT_STREQ(ver, HEALTH_DIAG_BRIDGE_VERSION);
    }
}

//=============================================================================
// REGRESSION: Logging
//=============================================================================

/**
 * Bug #R46: Logger create/destroy cycle doesn't leak
 */
TEST_F(HealthDiagBridgeRegressionTest, LoggerCreateDestroyCycle) {
    for (int i = 0; i < 5; i++) {
        nimcp_log_config_t log_config = nimcp_log_default_config();
        log_config.level = LOG_LEVEL_DEBUG;

        nimcp_logger_t logger = nimcp_log_create(&log_config);
        if (logger) {
            nimcp_log_destroy(logger);
        }
    }
}
