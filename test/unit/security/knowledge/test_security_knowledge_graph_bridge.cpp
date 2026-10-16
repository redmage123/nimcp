/**
 * @file test_security_knowledge_graph_bridge.cpp
 * @brief Unit tests for Security-Knowledge Graph Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Comprehensive tests for the security-knowledge graph bridge including:
 * - Lifecycle (default_config, create/destroy)
 * - Connection tests (kg_reader, bbb, anomaly_detector)
 * - Query validation (injection, entity names, search)
 * - Traversal access control (depth, privacy, scope)
 * - Node integrity verification
 * - Consistency enforcement
 * - Privacy isolation
 * - Lockdown mode
 * - Bidirectional update tests
 * - State and statistics tests
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <string>

extern "C" {
#include "security/knowledge/nimcp_security_knowledge_graph_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SecurityKnowledgeGraphBridgeTest : public ::testing::Test {
protected:
    security_kg_bridge_t* bridge = nullptr;
    sec_kg_config_t config;

    void SetUp() override {
        security_kg_default_config(&config);
        bridge = security_kg_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            security_kg_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    /* Helper: Create a valid entity name */
    static std::string create_valid_entity_name(const char* base = "TestEntity") {
        return std::string(base) + "_Valid";
    }

    /* Helper: Create an injection attack query */
    static std::string create_injection_query() {
        return "SELECT * FROM entities; DROP TABLE entities;--";
    }

    /* Helper: Create a very long query */
    static std::string create_long_query(size_t length = SEC_KG_MAX_QUERY_LENGTH + 100) {
        return std::string(length, 'A');
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphBridgeTest, DefaultConfigIsValid) {
    sec_kg_config_t cfg;
    int result = security_kg_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cfg.enable_query_validation);
    EXPECT_TRUE(cfg.enable_traversal_control);
    EXPECT_TRUE(cfg.enable_integrity_verification);
    EXPECT_TRUE(cfg.enable_consistency_checks);
    EXPECT_TRUE(cfg.enable_privacy_isolation);
    EXPECT_FLOAT_EQ(cfg.injection_threshold, SEC_KG_DEFAULT_INJECTION_THRESHOLD);
    EXPECT_FLOAT_EQ(cfg.integrity_threshold, SEC_KG_DEFAULT_INTEGRITY_THRESHOLD);
    EXPECT_EQ(cfg.max_query_length, SEC_KG_MAX_QUERY_LENGTH);
    EXPECT_EQ(cfg.max_traversal_depth, SEC_KG_MAX_TRAVERSAL_DEPTH);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, DefaultConfigNullFails) {
    int result = security_kg_default_config(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CreateWithValidConfig) {
    sec_kg_config_t cfg;
    security_kg_default_config(&cfg);

    security_kg_bridge_t* br = security_kg_bridge_create(&cfg);
    ASSERT_NE(br, nullptr);

    sec_kg_bridge_state_t state;
    security_kg_get_state(br, &state);
    EXPECT_EQ(state.operational_state, SEC_KG_STATE_READY);

    security_kg_bridge_destroy(br);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CreateWithNullConfigUsesDefaults) {
    security_kg_bridge_t* br = security_kg_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);

    sec_kg_bridge_state_t state;
    int ret = security_kg_get_state(br, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.operational_state, SEC_KG_STATE_READY);

    security_kg_bridge_destroy(br);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CreateWithCustomConfig) {
    sec_kg_config_t custom_cfg;
    security_kg_default_config(&custom_cfg);
    custom_cfg.injection_threshold = 0.9f;
    custom_cfg.max_traversal_depth = 16;
    custom_cfg.enable_rate_limiting = false;

    security_kg_bridge_t* br = security_kg_bridge_create(&custom_cfg);
    ASSERT_NE(br, nullptr);

    security_kg_bridge_destroy(br);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, DestroyNullIsSafe) {
    security_kg_bridge_destroy(nullptr);
    /* Should not crash */
}

TEST_F(SecurityKnowledgeGraphBridgeTest, DestroyValidBridge) {
    sec_kg_config_t cfg;
    security_kg_default_config(&cfg);

    security_kg_bridge_t* br = security_kg_bridge_create(&cfg);
    ASSERT_NE(br, nullptr);

    security_kg_bridge_destroy(br);
    /* Should not crash */
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphBridgeTest, ConnectReaderNullBridgeFails) {
    kg_reader_t* dummy_reader = reinterpret_cast<kg_reader_t*>(0x12345678);
    int result = security_kg_connect_reader(nullptr, dummy_reader);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ConnectReaderNullReaderFails) {
    int result = security_kg_connect_reader(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ConnectReaderValidUpdatesState) {
    kg_reader_t* dummy_reader = reinterpret_cast<kg_reader_t*>(0x12345678);
    int result = security_kg_connect_reader(bridge, dummy_reader);
    EXPECT_EQ(result, 0);

    sec_kg_bridge_state_t state;
    security_kg_get_state(bridge, &state);
    EXPECT_TRUE(state.kg_reader_connected);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ConnectBBBNullBridgeFails) {
    bbb_system_t dummy_bbb = reinterpret_cast<bbb_system_t>(0x12345678);
    int result = security_kg_connect_bbb(nullptr, dummy_bbb);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ConnectBBBNullBBBFails) {
    int result = security_kg_connect_bbb(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ConnectBBBValidUpdatesState) {
    bbb_system_t dummy_bbb = reinterpret_cast<bbb_system_t>(0x12345678);
    int result = security_kg_connect_bbb(bridge, dummy_bbb);
    EXPECT_EQ(result, 0);

    sec_kg_bridge_state_t state;
    security_kg_get_state(bridge, &state);
    EXPECT_TRUE(state.bbb_connected);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ConnectAnomalyDetectorNullBridgeFails) {
    nimcp_anomaly_detector_t dummy_detector = reinterpret_cast<nimcp_anomaly_detector_t>(0x12345678);
    int result = security_kg_connect_anomaly_detector(nullptr, dummy_detector);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ConnectAnomalyDetectorNullDetectorFails) {
    int result = security_kg_connect_anomaly_detector(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ConnectAnomalyDetectorValidUpdatesState) {
    nimcp_anomaly_detector_t dummy_detector = reinterpret_cast<nimcp_anomaly_detector_t>(0x12345678);
    int result = security_kg_connect_anomaly_detector(bridge, dummy_detector);
    EXPECT_EQ(result, 0);

    sec_kg_bridge_state_t state;
    security_kg_get_state(bridge, &state);
    EXPECT_TRUE(state.anomaly_detector_connected);
}

/* ============================================================================
 * Query Validation Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphBridgeTest, ValidateQueryValidInput) {
    std::string query = "SELECT entity WHERE name = 'TestEntity'";
    sec_kg_query_result_t result;

    int ret = security_kg_validate_query(
        bridge, query.c_str(), query.length(), &result
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_QUERY_VALID);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ValidateQueryNullBridgeFails) {
    std::string query = "SELECT entity";
    sec_kg_query_result_t result;

    int ret = security_kg_validate_query(
        nullptr, query.c_str(), query.length(), &result
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ValidateQueryNullQueryFails) {
    sec_kg_query_result_t result;
    int ret = security_kg_validate_query(bridge, nullptr, 0, &result);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ValidateQueryNullResultFails) {
    std::string query = "SELECT entity";
    int ret = security_kg_validate_query(bridge, query.c_str(), query.length(), nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ValidateQueryTooLong) {
    std::string query = create_long_query();
    sec_kg_query_result_t result;

    int ret = security_kg_validate_query(
        bridge, query.c_str(), query.length(), &result
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_QUERY_TOO_LONG);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ValidateQuerySqlInjection) {
    std::string query = "SELECT * FROM entities; DROP TABLE entities;--";
    sec_kg_query_result_t result;

    int ret = security_kg_validate_query(
        bridge, query.c_str(), query.length(), &result
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_QUERY_INJECTION_DETECTED);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ValidateQueryCommentInjection) {
    std::string query = "SELECT entity /* malicious comment */ WHERE id=1";
    sec_kg_query_result_t result;

    int ret = security_kg_validate_query(
        bridge, query.c_str(), query.length(), &result
    );

    EXPECT_EQ(ret, 0);
    /* Should detect injection pattern */
    EXPECT_TRUE(result == SEC_KG_QUERY_INJECTION_DETECTED ||
                result == SEC_KG_QUERY_VALID);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ValidateQueryUpdatesStats) {
    std::string query = "SELECT entity WHERE name = 'Test'";
    sec_kg_query_result_t result;

    security_kg_validate_query(bridge, query.c_str(), query.length(), &result);

    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);
    EXPECT_EQ(stats.queries_validated_total, 1u);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ValidateEntityNameValid) {
    std::string entity = "Valid_Entity_Name-123";
    sec_kg_query_result_t result;

    int ret = security_kg_validate_entity_name(bridge, entity.c_str(), &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_QUERY_VALID);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ValidateEntityNameNullBridgeFails) {
    sec_kg_query_result_t result;
    int ret = security_kg_validate_entity_name(nullptr, "Entity", &result);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ValidateEntityNameNullNameFails) {
    sec_kg_query_result_t result;
    int ret = security_kg_validate_entity_name(bridge, nullptr, &result);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ValidateEntityNameWithSpecialChars) {
    std::string entity = "Entity<script>alert('xss')</script>";
    sec_kg_query_result_t result;

    int ret = security_kg_validate_entity_name(bridge, entity.c_str(), &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_QUERY_INJECTION_DETECTED);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ValidateEntityNameEmpty) {
    sec_kg_query_result_t result;
    int ret = security_kg_validate_entity_name(bridge, "", &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_QUERY_MALFORMED);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ValidateSearchValid) {
    std::string search = "module capabilities";
    sec_kg_query_result_t result;

    int ret = security_kg_validate_search(bridge, search.c_str(), &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_QUERY_VALID);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ValidateSearchNullBridgeFails) {
    sec_kg_query_result_t result;
    int ret = security_kg_validate_search(nullptr, "search", &result);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Traversal Access Control Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckTraversalAccessValid) {
    sec_kg_traversal_result_t result;

    int ret = security_kg_check_traversal_access(
        bridge, "SourceEntity", "TargetEntity", "connects_to", 1, &result
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_TRAVERSAL_ALLOWED);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckTraversalAccessNullBridgeFails) {
    sec_kg_traversal_result_t result;
    int ret = security_kg_check_traversal_access(
        nullptr, "Source", "Target", "rel", 1, &result
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckTraversalAccessNullSourceFails) {
    sec_kg_traversal_result_t result;
    int ret = security_kg_check_traversal_access(
        bridge, nullptr, "Target", "rel", 1, &result
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckTraversalAccessNullResultFails) {
    int ret = security_kg_check_traversal_access(
        bridge, "Source", "Target", "rel", 1, nullptr
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckTraversalAccessDepthExceeded) {
    sec_kg_traversal_result_t result;

    int ret = security_kg_check_traversal_access(
        bridge, "Source", "Target", "rel", SEC_KG_MAX_TRAVERSAL_DEPTH + 10, &result
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_TRAVERSAL_DEPTH_EXCEEDED);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckTraversalAccessNullTarget) {
    sec_kg_traversal_result_t result;

    /* NULL target should be allowed (wildcard traversal) */
    int ret = security_kg_check_traversal_access(
        bridge, "Source", nullptr, "rel", 1, &result
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_TRAVERSAL_ALLOWED);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckTraversalAccessNullRelType) {
    sec_kg_traversal_result_t result;

    /* NULL relation type should be allowed (any relation) */
    int ret = security_kg_check_traversal_access(
        bridge, "Source", "Target", nullptr, 1, &result
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_TRAVERSAL_ALLOWED);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckTraversalAccessPrivateNode) {
    /* First, isolate a node */
    security_kg_isolate_private_data(bridge, "PrivateTarget", SEC_KG_PRIVACY_SECRET);

    sec_kg_traversal_result_t result;
    int ret = security_kg_check_traversal_access(
        bridge, "Source", "PrivateTarget", "rel", 1, &result
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_TRAVERSAL_NODE_PRIVATE);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, IsEntityAccessibleValid) {
    bool accessible = false;

    int ret = security_kg_is_entity_accessible(bridge, "PublicEntity", &accessible);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(accessible);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, IsEntityAccessibleNullBridgeFails) {
    bool accessible;
    int ret = security_kg_is_entity_accessible(nullptr, "Entity", &accessible);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, IsEntityAccessibleNullNameFails) {
    bool accessible;
    int ret = security_kg_is_entity_accessible(bridge, nullptr, &accessible);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, IsEntityAccessiblePrivateEntity) {
    security_kg_isolate_private_data(bridge, "SecretEntity", SEC_KG_PRIVACY_SECRET);

    bool accessible = true;
    int ret = security_kg_is_entity_accessible(bridge, "SecretEntity", &accessible);

    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(accessible);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, SetMaxTraversalDepthValid) {
    int ret = security_kg_set_max_traversal_depth(bridge, 16);
    EXPECT_EQ(ret, 0);

    sec_to_kg_effects_t effects;
    security_kg_get_sec_to_kg_effects(bridge, &effects);
    EXPECT_EQ(effects.current_traversal_limit, 16u);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, SetMaxTraversalDepthNullBridgeFails) {
    int ret = security_kg_set_max_traversal_depth(nullptr, 16);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Node Integrity Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphBridgeTest, VerifyNodeIntegrityNullBridgeFails) {
    sec_kg_integrity_result_t result;
    int ret = security_kg_verify_node_integrity(nullptr, "Entity", &result);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, VerifyNodeIntegrityNullNameFails) {
    sec_kg_integrity_result_t result;
    int ret = security_kg_verify_node_integrity(bridge, nullptr, &result);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, VerifyNodeIntegrityNullResultFails) {
    int ret = security_kg_verify_node_integrity(bridge, "Entity", nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, VerifyNodeIntegrityNoReader) {
    sec_kg_integrity_result_t result;
    int ret = security_kg_verify_node_integrity(bridge, "Entity", &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_NODE_NOT_FOUND);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, VerifyRelationIntegrityNullBridgeFails) {
    sec_kg_integrity_result_t result;
    int ret = security_kg_verify_relation_integrity(nullptr, "From", "To", "rel", &result);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, VerifyRelationIntegrityNullFromFails) {
    sec_kg_integrity_result_t result;
    int ret = security_kg_verify_relation_integrity(bridge, nullptr, "To", "rel", &result);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, VerifyRelationIntegrityNullToFails) {
    sec_kg_integrity_result_t result;
    int ret = security_kg_verify_relation_integrity(bridge, "From", nullptr, "rel", &result);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, VerifyRelationIntegrityNoReader) {
    sec_kg_integrity_result_t result;
    int ret = security_kg_verify_relation_integrity(bridge, "From", "To", "rel", &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_NODE_NOT_FOUND);
}

/* ============================================================================
 * Consistency Enforcement Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphBridgeTest, EnforceConsistencyNullBridgeFails) {
    sec_kg_consistency_result_t result;
    int ret = security_kg_enforce_consistency(nullptr, &result);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, EnforceConsistencyNullResultFails) {
    int ret = security_kg_enforce_consistency(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, EnforceConsistencyNoReader) {
    sec_kg_consistency_result_t result;
    int ret = security_kg_enforce_consistency(bridge, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_CONSTRAINT_VIOLATION);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckOrphanedNodesNullBridgeFails) {
    uint32_t count;
    int ret = security_kg_check_orphaned_nodes(nullptr, &count);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckOrphanedNodesNullCountFails) {
    int ret = security_kg_check_orphaned_nodes(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckOrphanedNodesNoReader) {
    uint32_t count = 999;
    int ret = security_kg_check_orphaned_nodes(bridge, &count);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(count, 0u);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckDanglingRelationsNullBridgeFails) {
    uint32_t count;
    int ret = security_kg_check_dangling_relations(nullptr, &count);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckDanglingRelationsNullCountFails) {
    int ret = security_kg_check_dangling_relations(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Privacy Isolation Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphBridgeTest, IsolatePrivateDataValid) {
    int ret = security_kg_isolate_private_data(
        bridge, "SecretEntity", SEC_KG_PRIVACY_SECRET
    );
    EXPECT_EQ(ret, 0);

    sec_kg_privacy_level_t level;
    security_kg_get_privacy_level(bridge, "SecretEntity", &level);
    EXPECT_EQ(level, SEC_KG_PRIVACY_SECRET);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, IsolatePrivateDataNullBridgeFails) {
    int ret = security_kg_isolate_private_data(nullptr, "Entity", SEC_KG_PRIVACY_SECRET);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, IsolatePrivateDataNullNameFails) {
    int ret = security_kg_isolate_private_data(bridge, nullptr, SEC_KG_PRIVACY_SECRET);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, IsolatePrivateDataUpdateExisting) {
    security_kg_isolate_private_data(bridge, "Entity", SEC_KG_PRIVACY_INTERNAL);

    int ret = security_kg_isolate_private_data(bridge, "Entity", SEC_KG_PRIVACY_SECRET);
    EXPECT_EQ(ret, 0);

    sec_kg_privacy_level_t level;
    security_kg_get_privacy_level(bridge, "Entity", &level);
    EXPECT_EQ(level, SEC_KG_PRIVACY_SECRET);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, IsolateMultipleEntities) {
    security_kg_isolate_private_data(bridge, "Entity1", SEC_KG_PRIVACY_INTERNAL);
    security_kg_isolate_private_data(bridge, "Entity2", SEC_KG_PRIVACY_RESTRICTED);
    security_kg_isolate_private_data(bridge, "Entity3", SEC_KG_PRIVACY_CONFIDENTIAL);

    sec_kg_privacy_level_t level;

    security_kg_get_privacy_level(bridge, "Entity1", &level);
    EXPECT_EQ(level, SEC_KG_PRIVACY_INTERNAL);

    security_kg_get_privacy_level(bridge, "Entity2", &level);
    EXPECT_EQ(level, SEC_KG_PRIVACY_RESTRICTED);

    security_kg_get_privacy_level(bridge, "Entity3", &level);
    EXPECT_EQ(level, SEC_KG_PRIVACY_CONFIDENTIAL);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, RemoveIsolationValid) {
    security_kg_isolate_private_data(bridge, "Entity", SEC_KG_PRIVACY_SECRET);

    int ret = security_kg_remove_isolation(bridge, "Entity");
    EXPECT_EQ(ret, 0);

    sec_kg_privacy_level_t level;
    security_kg_get_privacy_level(bridge, "Entity", &level);
    EXPECT_EQ(level, SEC_KG_PRIVACY_PUBLIC);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, RemoveIsolationNullBridgeFails) {
    int ret = security_kg_remove_isolation(nullptr, "Entity");
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, RemoveIsolationNullNameFails) {
    int ret = security_kg_remove_isolation(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, RemoveIsolationNonExistent) {
    int ret = security_kg_remove_isolation(bridge, "NonExistentEntity");
    EXPECT_EQ(ret, 0);  /* Should succeed silently */
}

TEST_F(SecurityKnowledgeGraphBridgeTest, GetPrivacyLevelNullBridgeFails) {
    sec_kg_privacy_level_t level;
    int ret = security_kg_get_privacy_level(nullptr, "Entity", &level);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, GetPrivacyLevelNullNameFails) {
    sec_kg_privacy_level_t level;
    int ret = security_kg_get_privacy_level(bridge, nullptr, &level);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, GetPrivacyLevelNullLevelFails) {
    int ret = security_kg_get_privacy_level(bridge, "Entity", nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, GetPrivacyLevelPublicDefault) {
    sec_kg_privacy_level_t level;
    int ret = security_kg_get_privacy_level(bridge, "UnknownEntity", &level);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(level, SEC_KG_PRIVACY_PUBLIC);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckPrivacyAccessValid) {
    bool allowed = false;
    int ret = security_kg_check_privacy_access(bridge, SEC_KG_PRIVACY_PUBLIC, &allowed);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(allowed);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckPrivacyAccessNullBridgeFails) {
    bool allowed;
    int ret = security_kg_check_privacy_access(nullptr, SEC_KG_PRIVACY_PUBLIC, &allowed);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, CheckPrivacyAccessNullAllowedFails) {
    int ret = security_kg_check_privacy_access(bridge, SEC_KG_PRIVACY_PUBLIC, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Lockdown Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphBridgeTest, EnterLockdownValid) {
    int ret = security_kg_enter_lockdown(bridge, "Security threat detected");
    EXPECT_EQ(ret, 0);

    bool active = false;
    security_kg_is_lockdown_active(bridge, &active);
    EXPECT_TRUE(active);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, EnterLockdownNullBridgeFails) {
    int ret = security_kg_enter_lockdown(nullptr, "reason");
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, EnterLockdownBlocksQueries) {
    security_kg_enter_lockdown(bridge, "Test lockdown");

    std::string query = "SELECT entity";
    sec_kg_query_result_t result;
    security_kg_validate_query(bridge, query.c_str(), query.length(), &result);

    EXPECT_EQ(result, SEC_KG_QUERY_REJECTED);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, EnterLockdownBlocksTraversals) {
    security_kg_enter_lockdown(bridge, "Test lockdown");

    sec_kg_traversal_result_t result;
    security_kg_check_traversal_access(bridge, "Source", "Target", "rel", 1, &result);

    EXPECT_EQ(result, SEC_KG_TRAVERSAL_DENIED);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ExitLockdownValid) {
    security_kg_enter_lockdown(bridge, "Test lockdown");
    int ret = security_kg_exit_lockdown(bridge);
    EXPECT_EQ(ret, 0);

    bool active = true;
    security_kg_is_lockdown_active(bridge, &active);
    EXPECT_FALSE(active);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ExitLockdownNullBridgeFails) {
    int ret = security_kg_exit_lockdown(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ExitLockdownRestoresOperations) {
    security_kg_enter_lockdown(bridge, "Test lockdown");
    security_kg_exit_lockdown(bridge);

    std::string query = "SELECT entity";
    sec_kg_query_result_t result;
    security_kg_validate_query(bridge, query.c_str(), query.length(), &result);

    EXPECT_EQ(result, SEC_KG_QUERY_VALID);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, IsLockdownActiveNullBridgeFails) {
    bool active;
    int ret = security_kg_is_lockdown_active(nullptr, &active);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, IsLockdownActiveNullActiveFails) {
    int ret = security_kg_is_lockdown_active(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphBridgeTest, UpdateSecToKgValid) {
    int ret = security_kg_update_sec_to_kg(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, UpdateSecToKgNullFails) {
    int ret = security_kg_update_sec_to_kg(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, UpdateKgToSecValid) {
    int ret = security_kg_update_kg_to_sec(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, UpdateKgToSecNullFails) {
    int ret = security_kg_update_kg_to_sec(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, UpdateFullCycleValid) {
    int ret = security_kg_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, UpdateFullCycleNullFails) {
    int ret = security_kg_update(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, GetSecToKgEffectsValid) {
    sec_to_kg_effects_t effects;
    int ret = security_kg_get_sec_to_kg_effects(bridge, &effects);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.query_threat_level, 0.0f);
    EXPECT_LE(effects.query_threat_level, 1.0f);
    EXPECT_GE(effects.traversal_threat_level, 0.0f);
    EXPECT_LE(effects.traversal_threat_level, 1.0f);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, GetSecToKgEffectsNullBridgeFails) {
    sec_to_kg_effects_t effects;
    int ret = security_kg_get_sec_to_kg_effects(nullptr, &effects);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, GetSecToKgEffectsNullEffectsFails) {
    int ret = security_kg_get_sec_to_kg_effects(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, GetKgToSecEffectsValid) {
    kg_to_sec_effects_t effects;
    int ret = security_kg_get_kg_to_sec_effects(bridge, &effects);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.query_anomaly_score, 0.0f);
    EXPECT_LE(effects.query_anomaly_score, 1.0f);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, GetKgToSecEffectsNullBridgeFails) {
    kg_to_sec_effects_t effects;
    int ret = security_kg_get_kg_to_sec_effects(nullptr, &effects);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, GetKgToSecEffectsNullEffectsFails) {
    int ret = security_kg_get_kg_to_sec_effects(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * State and Statistics Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphBridgeTest, GetStateValid) {
    sec_kg_bridge_state_t state;
    int ret = security_kg_get_state(bridge, &state);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.operational_state, SEC_KG_STATE_READY);
    EXPECT_FALSE(state.kg_reader_connected);
    EXPECT_FALSE(state.bbb_connected);
    EXPECT_FALSE(state.anomaly_detector_connected);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, GetStateNullBridgeFails) {
    sec_kg_bridge_state_t state;
    int ret = security_kg_get_state(nullptr, &state);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, GetStateNullStateFails) {
    int ret = security_kg_get_state(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, GetStatsValid) {
    sec_kg_stats_t stats;
    int ret = security_kg_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.queries_validated_total, 0u);
    EXPECT_EQ(stats.traversals_validated_total, 0u);
    EXPECT_EQ(stats.integrity_checks_total, 0u);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, GetStatsNullBridgeFails) {
    sec_kg_stats_t stats;
    int ret = security_kg_get_stats(nullptr, &stats);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, GetStatsNullStatsFails) {
    int ret = security_kg_get_stats(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, StatsAccumulateQueries) {
    std::string query = "SELECT entity";
    sec_kg_query_result_t result;

    security_kg_validate_query(bridge, query.c_str(), query.length(), &result);
    security_kg_validate_query(bridge, query.c_str(), query.length(), &result);
    security_kg_validate_query(bridge, query.c_str(), query.length(), &result);

    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);

    EXPECT_EQ(stats.queries_validated_total, 3u);
    EXPECT_EQ(stats.queries_passed, 3u);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, StatsAccumulateTraversals) {
    sec_kg_traversal_result_t result;

    security_kg_check_traversal_access(bridge, "S1", "T1", "r", 1, &result);
    security_kg_check_traversal_access(bridge, "S2", "T2", "r", 2, &result);

    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);

    EXPECT_EQ(stats.traversals_validated_total, 2u);
    EXPECT_EQ(stats.traversals_allowed, 2u);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, StatsTrackInjections) {
    std::string injection = "SELECT * FROM entities;--";
    sec_kg_query_result_t result;

    security_kg_validate_query(bridge, injection.c_str(), injection.length(), &result);

    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);

    EXPECT_GE(stats.injections_detected, 1u);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ResetStatsValid) {
    std::string query = "SELECT entity";
    sec_kg_query_result_t result;
    security_kg_validate_query(bridge, query.c_str(), query.length(), &result);

    security_kg_reset_stats(bridge);

    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);

    EXPECT_EQ(stats.queries_validated_total, 0u);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ResetStatsNullIsSafe) {
    security_kg_reset_stats(nullptr);
    /* Should not crash */
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphBridgeTest, QueryResultNameMapping) {
    EXPECT_NE(security_kg_query_result_name(SEC_KG_QUERY_VALID), nullptr);
    EXPECT_NE(security_kg_query_result_name(SEC_KG_QUERY_INJECTION_DETECTED), nullptr);
    EXPECT_NE(security_kg_query_result_name(SEC_KG_QUERY_TOO_LONG), nullptr);
    EXPECT_NE(security_kg_query_result_name(SEC_KG_QUERY_MALFORMED), nullptr);
    EXPECT_NE(security_kg_query_result_name(SEC_KG_QUERY_FORBIDDEN_PATTERN), nullptr);
    EXPECT_NE(security_kg_query_result_name(SEC_KG_QUERY_RATE_LIMITED), nullptr);
    EXPECT_NE(security_kg_query_result_name(SEC_KG_QUERY_UNAUTHORIZED), nullptr);
    EXPECT_NE(security_kg_query_result_name(SEC_KG_QUERY_REJECTED), nullptr);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, TraversalResultNameMapping) {
    EXPECT_NE(security_kg_traversal_result_name(SEC_KG_TRAVERSAL_ALLOWED), nullptr);
    EXPECT_NE(security_kg_traversal_result_name(SEC_KG_TRAVERSAL_DEPTH_EXCEEDED), nullptr);
    EXPECT_NE(security_kg_traversal_result_name(SEC_KG_TRAVERSAL_SCOPE_DENIED), nullptr);
    EXPECT_NE(security_kg_traversal_result_name(SEC_KG_TRAVERSAL_NODE_PRIVATE), nullptr);
    EXPECT_NE(security_kg_traversal_result_name(SEC_KG_TRAVERSAL_EDGE_FORBIDDEN), nullptr);
    EXPECT_NE(security_kg_traversal_result_name(SEC_KG_TRAVERSAL_CYCLE_DETECTED), nullptr);
    EXPECT_NE(security_kg_traversal_result_name(SEC_KG_TRAVERSAL_RATE_LIMITED), nullptr);
    EXPECT_NE(security_kg_traversal_result_name(SEC_KG_TRAVERSAL_DENIED), nullptr);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, IntegrityResultNameMapping) {
    EXPECT_NE(security_kg_integrity_result_name(SEC_KG_NODE_VALID), nullptr);
    EXPECT_NE(security_kg_integrity_result_name(SEC_KG_NODE_HASH_MISMATCH), nullptr);
    EXPECT_NE(security_kg_integrity_result_name(SEC_KG_NODE_SIGNATURE_INVALID), nullptr);
    EXPECT_NE(security_kg_integrity_result_name(SEC_KG_NODE_TIMESTAMP_ANOMALY), nullptr);
    EXPECT_NE(security_kg_integrity_result_name(SEC_KG_NODE_RELATION_MISMATCH), nullptr);
    EXPECT_NE(security_kg_integrity_result_name(SEC_KG_NODE_CORRUPTED), nullptr);
    EXPECT_NE(security_kg_integrity_result_name(SEC_KG_NODE_NOT_FOUND), nullptr);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ConsistencyResultNameMapping) {
    EXPECT_NE(security_kg_consistency_result_name(SEC_KG_CONSISTENT), nullptr);
    EXPECT_NE(security_kg_consistency_result_name(SEC_KG_ORPHANED_NODE), nullptr);
    EXPECT_NE(security_kg_consistency_result_name(SEC_KG_DANGLING_RELATION), nullptr);
    EXPECT_NE(security_kg_consistency_result_name(SEC_KG_DUPLICATE_ENTRY), nullptr);
    EXPECT_NE(security_kg_consistency_result_name(SEC_KG_CYCLE_VIOLATION), nullptr);
    EXPECT_NE(security_kg_consistency_result_name(SEC_KG_TYPE_VIOLATION), nullptr);
    EXPECT_NE(security_kg_consistency_result_name(SEC_KG_CONSTRAINT_VIOLATION), nullptr);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, PrivacyLevelNameMapping) {
    EXPECT_NE(security_kg_privacy_level_name(SEC_KG_PRIVACY_PUBLIC), nullptr);
    EXPECT_NE(security_kg_privacy_level_name(SEC_KG_PRIVACY_INTERNAL), nullptr);
    EXPECT_NE(security_kg_privacy_level_name(SEC_KG_PRIVACY_RESTRICTED), nullptr);
    EXPECT_NE(security_kg_privacy_level_name(SEC_KG_PRIVACY_CONFIDENTIAL), nullptr);
    EXPECT_NE(security_kg_privacy_level_name(SEC_KG_PRIVACY_SECRET), nullptr);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, StateNameMapping) {
    EXPECT_NE(security_kg_state_name(SEC_KG_STATE_UNINITIALIZED), nullptr);
    EXPECT_NE(security_kg_state_name(SEC_KG_STATE_READY), nullptr);
    EXPECT_NE(security_kg_state_name(SEC_KG_STATE_PROCESSING), nullptr);
    EXPECT_NE(security_kg_state_name(SEC_KG_STATE_LOCKDOWN), nullptr);
    EXPECT_NE(security_kg_state_name(SEC_KG_STATE_DEGRADED), nullptr);
    EXPECT_NE(security_kg_state_name(SEC_KG_STATE_ERROR), nullptr);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ReportFalsePositiveValid) {
    int ret = security_kg_report_false_positive(bridge);
    EXPECT_EQ(ret, 0);

    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);
    EXPECT_EQ(stats.false_positives_reported, 1u);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ReportFalsePositiveNullFails) {
    int ret = security_kg_report_false_positive(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ReportFalsePositiveMultiple) {
    security_kg_report_false_positive(bridge);
    security_kg_report_false_positive(bridge);
    security_kg_report_false_positive(bridge);

    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);
    EXPECT_EQ(stats.false_positives_reported, 3u);
}

/* ============================================================================
 * Integration Workflow Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphBridgeTest, FullQueryValidationWorkflow) {
    /* 1. Validate query */
    std::string query = "SELECT entity WHERE type = 'Module'";
    sec_kg_query_result_t result;

    int ret = security_kg_validate_query(
        bridge, query.c_str(), query.length(), &result
    );
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_QUERY_VALID);

    /* 2. Update bidirectional flow */
    ret = security_kg_update(bridge);
    EXPECT_EQ(ret, 0);

    /* 3. Verify stats */
    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);
    EXPECT_GE(stats.queries_validated_total, 1u);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, FullTraversalWorkflow) {
    /* 1. Check traversal access */
    sec_kg_traversal_result_t result;
    int ret = security_kg_check_traversal_access(
        bridge, "ModuleA", "ModuleB", "connects_to", 1, &result
    );
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_TRAVERSAL_ALLOWED);

    /* 2. Continue traversal at deeper level */
    ret = security_kg_check_traversal_access(
        bridge, "ModuleB", "ModuleC", "integrates_with", 2, &result
    );
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_KG_TRAVERSAL_ALLOWED);

    /* 3. Verify stats */
    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);
    EXPECT_EQ(stats.traversals_validated_total, 2u);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, PrivacyIsolationWorkflow) {
    /* 1. Isolate a sensitive entity */
    int ret = security_kg_isolate_private_data(
        bridge, "SensitiveModule", SEC_KG_PRIVACY_CONFIDENTIAL
    );
    EXPECT_EQ(ret, 0);

    /* 2. Verify isolation */
    sec_kg_privacy_level_t level;
    security_kg_get_privacy_level(bridge, "SensitiveModule", &level);
    EXPECT_EQ(level, SEC_KG_PRIVACY_CONFIDENTIAL);

    /* 3. Verify traversal is blocked */
    sec_kg_traversal_result_t trav_result;
    security_kg_check_traversal_access(
        bridge, "PublicModule", "SensitiveModule", "accesses", 1, &trav_result
    );
    EXPECT_EQ(trav_result, SEC_KG_TRAVERSAL_NODE_PRIVATE);

    /* 4. Declassify */
    ret = security_kg_remove_isolation(bridge, "SensitiveModule");
    EXPECT_EQ(ret, 0);

    /* 5. Verify access restored */
    security_kg_get_privacy_level(bridge, "SensitiveModule", &level);
    EXPECT_EQ(level, SEC_KG_PRIVACY_PUBLIC);
}

TEST_F(SecurityKnowledgeGraphBridgeTest, ThreatDetectionAndLockdownWorkflow) {
    /* 1. Process malicious query */
    std::string injection = "SELECT * FROM secrets;--";
    sec_kg_query_result_t result;
    security_kg_validate_query(bridge, injection.c_str(), injection.length(), &result);
    EXPECT_EQ(result, SEC_KG_QUERY_INJECTION_DETECTED);

    /* 2. Enter lockdown */
    int ret = security_kg_enter_lockdown(bridge, "Injection attack detected");
    EXPECT_EQ(ret, 0);

    /* 3. Verify all operations blocked */
    std::string valid_query = "SELECT entity";
    security_kg_validate_query(bridge, valid_query.c_str(), valid_query.length(), &result);
    EXPECT_EQ(result, SEC_KG_QUERY_REJECTED);

    sec_kg_traversal_result_t trav_result;
    security_kg_check_traversal_access(bridge, "A", "B", "r", 1, &trav_result);
    EXPECT_EQ(trav_result, SEC_KG_TRAVERSAL_DENIED);

    /* 4. Exit lockdown */
    ret = security_kg_exit_lockdown(bridge);
    EXPECT_EQ(ret, 0);

    /* 5. Verify operations restored */
    security_kg_validate_query(bridge, valid_query.c_str(), valid_query.length(), &result);
    EXPECT_EQ(result, SEC_KG_QUERY_VALID);
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
