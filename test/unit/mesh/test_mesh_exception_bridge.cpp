/**
 * @file test_mesh_exception_bridge.cpp
 * @brief Unit tests for mesh exception bridge (Phase 14)
 *
 * Tests exception to immune system routing and antigen conversion.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_exception_bridge.h"
#include "mesh/nimcp_mesh_types.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST(MeshExceptionBridgeConfigTest, DefaultConfig) {
    mesh_exception_bridge_config_t config;
    nimcp_error_t err = mesh_exception_bridge_default_config(&config);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(config.debounce_ms, 0u);
    EXPECT_GT(config.escalation_window_ms, 0u);
    EXPECT_GT(config.max_per_window, 0u);
}

TEST(MeshExceptionBridgeConfigTest, DefaultConfigNullPointer) {
    nimcp_error_t err = mesh_exception_bridge_default_config(nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST(MeshExceptionBridgeConfigTest, DefaultSeverityThresholds) {
    mesh_exception_bridge_config_t config;
    mesh_exception_bridge_default_config(&config);

    /* Min report severity should allow warnings and above */
    EXPECT_LE(config.min_report_severity, MESH_EXC_SEVERITY_WARNING);

    /* Quarantine threshold should be high severity */
    EXPECT_GE(config.quarantine_threshold, MESH_EXC_SEVERITY_SEVERE);
}

/* ============================================================================
 * Exception Classification Tests
 * ============================================================================ */

TEST(MeshExceptionBridgeClassifyTest, MemoryError) {
    mesh_exception_category_t category;
    mesh_exception_severity_t severity;

    nimcp_error_t err = mesh_exception_bridge_classify(
        NIMCP_ERROR_OUT_OF_MEMORY, &category, &severity);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(category, MESH_EXC_CAT_MEMORY);
}

TEST(MeshExceptionBridgeClassifyTest, NullPointerError) {
    mesh_exception_category_t category;
    mesh_exception_severity_t severity;

    nimcp_error_t err = mesh_exception_bridge_classify(
        NIMCP_ERROR_NULL_POINTER, &category, &severity);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    /* Null pointer could be logic or memory category */
    EXPECT_TRUE(category == MESH_EXC_CAT_LOGIC ||
                category == MESH_EXC_CAT_MEMORY);
}

TEST(MeshExceptionBridgeClassifyTest, TimeoutError) {
    mesh_exception_category_t category;
    mesh_exception_severity_t severity;

    nimcp_error_t err = mesh_exception_bridge_classify(
        NIMCP_ERROR_TIMEOUT, &category, &severity);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(category, MESH_EXC_CAT_TIMING);
}

TEST(MeshExceptionBridgeClassifyTest, ClassifyNullOutputs) {
    /* Classify with null outputs should still succeed - outputs are optional */
    nimcp_error_t err = mesh_exception_bridge_classify(
        NIMCP_ERROR_OUT_OF_MEMORY, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST(MeshExceptionBridgeClassifyTest, ClassifyWithOnlyCategoryOutput) {
    mesh_exception_category_t category;
    nimcp_error_t err = mesh_exception_bridge_classify(
        NIMCP_ERROR_TIMEOUT, &category, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(category, MESH_EXC_CAT_TIMING);
}

TEST(MeshExceptionBridgeClassifyTest, ClassifyWithOnlySeverityOutput) {
    mesh_exception_severity_t severity;
    nimcp_error_t err = mesh_exception_bridge_classify(
        NIMCP_ERROR_TIMEOUT, nullptr, &severity);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Severity Level Tests
 * ============================================================================ */

TEST(MeshExceptionBridgeSeverityTest, TraceSeverityIsLowest) {
    EXPECT_LT(MESH_EXC_SEVERITY_TRACE, MESH_EXC_SEVERITY_INFO);
}

TEST(MeshExceptionBridgeSeverityTest, CriticalSeverityIsHighest) {
    EXPECT_GT(MESH_EXC_SEVERITY_CRITICAL, MESH_EXC_SEVERITY_SEVERE);
    EXPECT_GT(MESH_EXC_SEVERITY_CRITICAL, MESH_EXC_SEVERITY_ERROR);
    EXPECT_GT(MESH_EXC_SEVERITY_CRITICAL, MESH_EXC_SEVERITY_WARNING);
}

TEST(MeshExceptionBridgeSeverityTest, SeverityOrdering) {
    EXPECT_LT(MESH_EXC_SEVERITY_TRACE, MESH_EXC_SEVERITY_INFO);
    EXPECT_LT(MESH_EXC_SEVERITY_INFO, MESH_EXC_SEVERITY_WARNING);
    EXPECT_LT(MESH_EXC_SEVERITY_WARNING, MESH_EXC_SEVERITY_ERROR);
    EXPECT_LT(MESH_EXC_SEVERITY_ERROR, MESH_EXC_SEVERITY_SEVERE);
    EXPECT_LT(MESH_EXC_SEVERITY_SEVERE, MESH_EXC_SEVERITY_CRITICAL);
}

/* ============================================================================
 * Category Tests
 * ============================================================================ */

TEST(MeshExceptionBridgeCategoryTest, AllCategoriesDefined) {
    /* Verify all categories have distinct values */
    EXPECT_NE(MESH_EXC_CAT_MEMORY, MESH_EXC_CAT_SECURITY);
    EXPECT_NE(MESH_EXC_CAT_SECURITY, MESH_EXC_CAT_NETWORK);
    EXPECT_NE(MESH_EXC_CAT_NETWORK, MESH_EXC_CAT_RESOURCE);
    EXPECT_NE(MESH_EXC_CAT_RESOURCE, MESH_EXC_CAT_LOGIC);
    EXPECT_NE(MESH_EXC_CAT_LOGIC, MESH_EXC_CAT_TIMING);
    EXPECT_NE(MESH_EXC_CAT_TIMING, MESH_EXC_CAT_DATA);
    EXPECT_NE(MESH_EXC_CAT_DATA, MESH_EXC_CAT_SYSTEM);
    EXPECT_NE(MESH_EXC_CAT_SYSTEM, MESH_EXC_CAT_GPU);
}

/* ============================================================================
 * Immune Action Tests
 * ============================================================================ */

TEST(MeshExceptionBridgeActionTest, ActionOrdering) {
    /* None should be the least severe action */
    EXPECT_LT(MESH_IMMUNE_ACTION_NONE, MESH_IMMUNE_ACTION_LOG);

    /* Shutdown should be the most severe action */
    EXPECT_GT(MESH_IMMUNE_ACTION_SHUTDOWN, MESH_IMMUNE_ACTION_RESTART);
    EXPECT_GT(MESH_IMMUNE_ACTION_SHUTDOWN, MESH_IMMUNE_ACTION_REVOKE);
    EXPECT_GT(MESH_IMMUNE_ACTION_SHUTDOWN, MESH_IMMUNE_ACTION_QUARANTINE);
}

TEST(MeshExceptionBridgeActionTest, AllActionsDistinct) {
    EXPECT_NE(MESH_IMMUNE_ACTION_NONE, MESH_IMMUNE_ACTION_LOG);
    EXPECT_NE(MESH_IMMUNE_ACTION_LOG, MESH_IMMUNE_ACTION_WARN);
    EXPECT_NE(MESH_IMMUNE_ACTION_WARN, MESH_IMMUNE_ACTION_QUARANTINE);
    EXPECT_NE(MESH_IMMUNE_ACTION_QUARANTINE, MESH_IMMUNE_ACTION_REVOKE);
    EXPECT_NE(MESH_IMMUNE_ACTION_REVOKE, MESH_IMMUNE_ACTION_REPAIR);
    EXPECT_NE(MESH_IMMUNE_ACTION_REPAIR, MESH_IMMUNE_ACTION_RESTART);
    EXPECT_NE(MESH_IMMUNE_ACTION_RESTART, MESH_IMMUNE_ACTION_SHUTDOWN);
}

/* ============================================================================
 * Antigen Structure Tests
 * ============================================================================ */

TEST(MeshExceptionBridgeAntigenTest, AntigenStructureSize) {
    /* Antigen structure should have reasonable size */
    EXPECT_GT(sizeof(mesh_exception_antigen_t), 0u);
    EXPECT_LT(sizeof(mesh_exception_antigen_t), 1024u);  /* Not too large */
}

TEST(MeshExceptionBridgeAntigenTest, AntigenPatternSize) {
    mesh_exception_antigen_t antigen;
    /* Pattern should be fixed size for consistent routing */
    EXPECT_EQ(sizeof(antigen.pattern), sizeof(float) * 8);
}

/* ============================================================================
 * Response Structure Tests
 * ============================================================================ */

TEST(MeshExceptionBridgeResponseTest, ResponseStructureSize) {
    EXPECT_GT(sizeof(mesh_exception_response_t), 0u);
}

TEST(MeshExceptionBridgeResponseTest, ResponseExplanationBuffer) {
    mesh_exception_response_t response;
    /* Explanation buffer should be large enough */
    EXPECT_GE(sizeof(response.explanation), 64u);
}
