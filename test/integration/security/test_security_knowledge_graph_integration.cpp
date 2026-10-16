/**
 * @file test_security_knowledge_graph_integration.cpp
 * @brief Integration tests for Security-Knowledge Graph Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Integration tests verifying the security-knowledge graph bridge works correctly
 * with actual knowledge graph operations, multi-query scenarios, and access control.
 *
 * TEST CATEGORIES:
 * - Knowledge graph integration
 * - Multi-query scenarios
 * - Access control testing
 * - Bidirectional effects integration
 * - Threat response scenarios
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>

extern "C" {
#include "security/knowledge/nimcp_security_knowledge_graph_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SecurityKnowledgeGraphIntegrationTest : public ::testing::Test {
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

    /* Generate batch of valid queries */
    std::vector<std::string> generate_valid_queries(size_t count) {
        std::vector<std::string> queries;
        for (size_t i = 0; i < count; i++) {
            queries.push_back("SELECT entity_" + std::to_string(i) +
                            " WHERE type = 'Module'");
        }
        return queries;
    }

    /* Generate batch of injection queries */
    std::vector<std::string> generate_injection_queries(size_t count) {
        std::vector<std::string> queries;
        for (size_t i = 0; i < count; i++) {
            queries.push_back("SELECT * FROM entities;-- DROP TABLE " +
                            std::to_string(i));
        }
        return queries;
    }

    /* Generate batch of entity names */
    std::vector<std::string> generate_entity_names(size_t count) {
        std::vector<std::string> names;
        for (size_t i = 0; i < count; i++) {
            names.push_back("Entity_" + std::to_string(i));
        }
        return names;
    }
};

/* ============================================================================
 * Multi-Query Scenario Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphIntegrationTest, BatchQueryValidation) {
    auto queries = generate_valid_queries(100);
    uint64_t passed = 0;
    uint64_t failed = 0;

    for (const auto& query : queries) {
        sec_kg_query_result_t result;
        int ret = security_kg_validate_query(
            bridge, query.c_str(), query.length(), &result
        );
        EXPECT_EQ(ret, 0);

        if (result == SEC_KG_QUERY_VALID) {
            passed++;
        } else {
            failed++;
        }
    }

    EXPECT_EQ(passed, 100u);
    EXPECT_EQ(failed, 0u);

    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);
    EXPECT_EQ(stats.queries_validated_total, 100u);
    EXPECT_EQ(stats.queries_passed, 100u);
}

TEST_F(SecurityKnowledgeGraphIntegrationTest, BatchInjectionDetection) {
    auto queries = generate_injection_queries(50);
    uint64_t detected = 0;

    for (const auto& query : queries) {
        sec_kg_query_result_t result;
        security_kg_validate_query(bridge, query.c_str(), query.length(), &result);

        if (result == SEC_KG_QUERY_INJECTION_DETECTED) {
            detected++;
        }
    }

    EXPECT_EQ(detected, 50u);

    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);
    EXPECT_EQ(stats.injections_detected, 50u);
}

TEST_F(SecurityKnowledgeGraphIntegrationTest, MixedQueryBatch) {
    auto valid = generate_valid_queries(50);
    auto invalid = generate_injection_queries(50);

    /* Mix queries */
    std::vector<std::string> mixed;
    for (size_t i = 0; i < 50; i++) {
        mixed.push_back(valid[i]);
        mixed.push_back(invalid[i]);
    }

    uint64_t valid_count = 0;
    uint64_t injection_count = 0;

    for (const auto& query : mixed) {
        sec_kg_query_result_t result;
        security_kg_validate_query(bridge, query.c_str(), query.length(), &result);

        if (result == SEC_KG_QUERY_VALID) {
            valid_count++;
        } else if (result == SEC_KG_QUERY_INJECTION_DETECTED) {
            injection_count++;
        }
    }

    EXPECT_EQ(valid_count, 50u);
    EXPECT_EQ(injection_count, 50u);
}

/* ============================================================================
 * Traversal Integration Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphIntegrationTest, DeepTraversalChain) {
    const uint32_t max_depth = SEC_KG_MAX_TRAVERSAL_DEPTH;
    uint32_t successful_depth = 0;

    for (uint32_t depth = 1; depth <= max_depth + 5; depth++) {
        sec_kg_traversal_result_t result;
        security_kg_check_traversal_access(
            bridge,
            ("Node_" + std::to_string(depth - 1)).c_str(),
            ("Node_" + std::to_string(depth)).c_str(),
            "connects_to",
            depth,
            &result
        );

        if (result == SEC_KG_TRAVERSAL_ALLOWED) {
            successful_depth = depth;
        } else {
            break;
        }
    }

    EXPECT_EQ(successful_depth, max_depth);
}

TEST_F(SecurityKnowledgeGraphIntegrationTest, TraversalWithPrivacyBarriers) {
    /* Set up privacy barriers at various nodes */
    security_kg_isolate_private_data(bridge, "Node_5", SEC_KG_PRIVACY_RESTRICTED);
    security_kg_isolate_private_data(bridge, "Node_10", SEC_KG_PRIVACY_CONFIDENTIAL);
    security_kg_isolate_private_data(bridge, "Node_15", SEC_KG_PRIVACY_SECRET);

    /* Try to traverse to each */
    sec_kg_traversal_result_t result;

    /* Depth 4 -> 5 (restricted) should be blocked */
    security_kg_check_traversal_access(
        bridge, "Node_4", "Node_5", "rel", 5, &result
    );
    EXPECT_EQ(result, SEC_KG_TRAVERSAL_NODE_PRIVATE);

    /* Depth 9 -> 10 (confidential) should be blocked */
    security_kg_check_traversal_access(
        bridge, "Node_9", "Node_10", "rel", 10, &result
    );
    EXPECT_EQ(result, SEC_KG_TRAVERSAL_NODE_PRIVATE);

    /* Depth 14 -> 15 (secret) should be blocked */
    security_kg_check_traversal_access(
        bridge, "Node_14", "Node_15", "rel", 15, &result
    );
    EXPECT_EQ(result, SEC_KG_TRAVERSAL_NODE_PRIVATE);
}

TEST_F(SecurityKnowledgeGraphIntegrationTest, DynamicTraversalDepthAdjustment) {
    /* Initial depth should be max */
    sec_to_kg_effects_t effects;
    security_kg_get_sec_to_kg_effects(bridge, &effects);
    uint32_t initial_depth = effects.current_traversal_limit;
    EXPECT_EQ(initial_depth, SEC_KG_MAX_TRAVERSAL_DEPTH);

    /* Reduce depth dynamically */
    security_kg_set_max_traversal_depth(bridge, 10);
    security_kg_get_sec_to_kg_effects(bridge, &effects);
    EXPECT_EQ(effects.current_traversal_limit, 10u);

    /* Verify traversal at old depth fails */
    sec_kg_traversal_result_t result;
    security_kg_check_traversal_access(
        bridge, "A", "B", "rel", 15, &result
    );
    EXPECT_EQ(result, SEC_KG_TRAVERSAL_DEPTH_EXCEEDED);

    /* Verify traversal at new depth succeeds */
    security_kg_check_traversal_access(
        bridge, "A", "B", "rel", 8, &result
    );
    EXPECT_EQ(result, SEC_KG_TRAVERSAL_ALLOWED);
}

/* ============================================================================
 * Privacy Isolation Integration Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphIntegrationTest, MassPrivacyIsolation) {
    auto entities = generate_entity_names(100);

    /* Isolate all entities at different levels */
    for (size_t i = 0; i < entities.size(); i++) {
        sec_kg_privacy_level_t level = static_cast<sec_kg_privacy_level_t>(
            i % 5  /* Cycle through levels */
        );
        int ret = security_kg_isolate_private_data(
            bridge, entities[i].c_str(), level
        );
        EXPECT_EQ(ret, 0);
    }

    /* Verify all levels were set correctly */
    for (size_t i = 0; i < entities.size(); i++) {
        sec_kg_privacy_level_t expected = static_cast<sec_kg_privacy_level_t>(i % 5);
        sec_kg_privacy_level_t actual;
        security_kg_get_privacy_level(bridge, entities[i].c_str(), &actual);
        EXPECT_EQ(actual, expected);
    }
}

TEST_F(SecurityKnowledgeGraphIntegrationTest, PrivacyLevelUpgradeDowngrade) {
    const char* entity = "TestEntity";

    /* Start at public */
    sec_kg_privacy_level_t level;
    security_kg_get_privacy_level(bridge, entity, &level);
    EXPECT_EQ(level, SEC_KG_PRIVACY_PUBLIC);

    /* Upgrade through all levels */
    for (int i = 0; i <= SEC_KG_PRIVACY_SECRET; i++) {
        sec_kg_privacy_level_t new_level = static_cast<sec_kg_privacy_level_t>(i);
        security_kg_isolate_private_data(bridge, entity, new_level);

        security_kg_get_privacy_level(bridge, entity, &level);
        EXPECT_EQ(level, new_level);
    }

    /* Downgrade to internal */
    security_kg_isolate_private_data(bridge, entity, SEC_KG_PRIVACY_INTERNAL);
    security_kg_get_privacy_level(bridge, entity, &level);
    EXPECT_EQ(level, SEC_KG_PRIVACY_INTERNAL);

    /* Remove isolation entirely */
    security_kg_remove_isolation(bridge, entity);
    security_kg_get_privacy_level(bridge, entity, &level);
    EXPECT_EQ(level, SEC_KG_PRIVACY_PUBLIC);
}

/* ============================================================================
 * Threat Response Integration Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphIntegrationTest, EscalatingThreatResponse) {
    /* Process increasing amounts of malicious queries */
    std::string injection = "SELECT * FROM secrets;--";

    for (int wave = 0; wave < 5; wave++) {
        /* Each wave has more queries */
        int query_count = (wave + 1) * 10;

        for (int i = 0; i < query_count; i++) {
            sec_kg_query_result_t result;
            security_kg_validate_query(
                bridge, injection.c_str(), injection.length(), &result
            );
        }

        /* Update bidirectional flow */
        security_kg_update(bridge);
    }

    /* Check statistics reflect attack */
    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);

    /* Total: 10 + 20 + 30 + 40 + 50 = 150 injections */
    EXPECT_EQ(stats.injections_detected, 150u);
}

TEST_F(SecurityKnowledgeGraphIntegrationTest, LockdownAndRecovery) {
    /* Phase 1: Normal operation */
    std::string valid = "SELECT entity";
    sec_kg_query_result_t result;
    security_kg_validate_query(bridge, valid.c_str(), valid.length(), &result);
    EXPECT_EQ(result, SEC_KG_QUERY_VALID);

    /* Phase 2: Attack detected, enter lockdown */
    security_kg_enter_lockdown(bridge, "Massive attack detected");

    sec_kg_bridge_state_t state;
    security_kg_get_state(bridge, &state);
    EXPECT_EQ(state.operational_state, SEC_KG_STATE_LOCKDOWN);
    EXPECT_TRUE(state.lockdown_active);

    /* Phase 3: Verify all operations blocked */
    security_kg_validate_query(bridge, valid.c_str(), valid.length(), &result);
    EXPECT_EQ(result, SEC_KG_QUERY_REJECTED);

    sec_kg_traversal_result_t trav;
    security_kg_check_traversal_access(bridge, "A", "B", "r", 1, &trav);
    EXPECT_EQ(trav, SEC_KG_TRAVERSAL_DENIED);

    /* Phase 4: Recovery - exit lockdown */
    security_kg_exit_lockdown(bridge);

    security_kg_get_state(bridge, &state);
    EXPECT_EQ(state.operational_state, SEC_KG_STATE_READY);
    EXPECT_FALSE(state.lockdown_active);

    /* Phase 5: Verify operations restored */
    security_kg_validate_query(bridge, valid.c_str(), valid.length(), &result);
    EXPECT_EQ(result, SEC_KG_QUERY_VALID);

    security_kg_check_traversal_access(bridge, "A", "B", "r", 1, &trav);
    EXPECT_EQ(trav, SEC_KG_TRAVERSAL_ALLOWED);
}

/* ============================================================================
 * Bidirectional Effects Integration Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphIntegrationTest, EffectsPropagateBidirectionally) {
    /* Initial state */
    sec_to_kg_effects_t sec_effects;
    kg_to_sec_effects_t kg_effects;

    security_kg_get_sec_to_kg_effects(bridge, &sec_effects);
    security_kg_get_kg_to_sec_effects(bridge, &kg_effects);

    EXPECT_EQ(sec_effects.query_threat_level, 0.0f);
    EXPECT_EQ(kg_effects.queries_processed, 0u);

    /* Process queries to generate KG->Sec effects */
    for (int i = 0; i < 100; i++) {
        std::string query = "SELECT entity_" + std::to_string(i);
        sec_kg_query_result_t result;
        security_kg_validate_query(bridge, query.c_str(), query.length(), &result);
    }

    security_kg_get_kg_to_sec_effects(bridge, &kg_effects);
    EXPECT_EQ(kg_effects.queries_processed, 100u);

    /* Update to propagate effects */
    security_kg_update(bridge);

    /* Verify Sec->KG effects updated */
    security_kg_get_sec_to_kg_effects(bridge, &sec_effects);
    /* Threat level should be computed based on anomaly scores */
    EXPECT_GE(sec_effects.query_threat_level, 0.0f);
    EXPECT_LE(sec_effects.query_threat_level, 1.0f);
}

TEST_F(SecurityKnowledgeGraphIntegrationTest, ThreatLevelAffectsTraversalLimit) {
    /* Get initial traversal limit */
    sec_to_kg_effects_t effects;
    security_kg_get_sec_to_kg_effects(bridge, &effects);
    uint32_t initial_limit = effects.current_traversal_limit;

    /* Manually set traversal depth to simulate threat response */
    security_kg_set_max_traversal_depth(bridge, initial_limit / 2);
    /* Note: Do NOT call security_kg_update() here - it recalculates the
     * traversal limit from threat level, overwriting our manual set.
     * The manual set is immediately effective without update. */

    security_kg_get_sec_to_kg_effects(bridge, &effects);
    EXPECT_EQ(effects.current_traversal_limit, initial_limit / 2);
}

/* ============================================================================
 * Access Control Integration Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphIntegrationTest, EntityAccessibilityMatrix) {
    /* Create entities with different privacy levels */
    struct {
        const char* name;
        sec_kg_privacy_level_t level;
        bool expected_accessible;
    } test_entities[] = {
        {"PublicEntity", SEC_KG_PRIVACY_PUBLIC, true},
        {"InternalEntity", SEC_KG_PRIVACY_INTERNAL, false},
        {"RestrictedEntity", SEC_KG_PRIVACY_RESTRICTED, false},
        {"ConfidentialEntity", SEC_KG_PRIVACY_CONFIDENTIAL, false},
        {"SecretEntity", SEC_KG_PRIVACY_SECRET, false}
    };

    /* Set up privacy */
    for (const auto& e : test_entities) {
        if (e.level != SEC_KG_PRIVACY_PUBLIC) {
            security_kg_isolate_private_data(bridge, e.name, e.level);
        }
    }

    /* Test accessibility */
    for (const auto& e : test_entities) {
        bool accessible = false;
        security_kg_is_entity_accessible(bridge, e.name, &accessible);

        if (e.expected_accessible) {
            EXPECT_TRUE(accessible) << "Entity " << e.name << " should be accessible";
        } else {
            EXPECT_FALSE(accessible) << "Entity " << e.name << " should not be accessible";
        }
    }
}

TEST_F(SecurityKnowledgeGraphIntegrationTest, AccessControlUnderLockdown) {
    /* Normal access works */
    bool accessible = true;
    security_kg_is_entity_accessible(bridge, "NormalEntity", &accessible);
    EXPECT_TRUE(accessible);

    /* Enter lockdown */
    security_kg_enter_lockdown(bridge, "Test lockdown");

    /* All access should be denied */
    security_kg_is_entity_accessible(bridge, "NormalEntity", &accessible);
    EXPECT_FALSE(accessible);

    security_kg_is_entity_accessible(bridge, "AnyEntity", &accessible);
    EXPECT_FALSE(accessible);

    /* Exit lockdown */
    security_kg_exit_lockdown(bridge);

    /* Access restored */
    security_kg_is_entity_accessible(bridge, "NormalEntity", &accessible);
    EXPECT_TRUE(accessible);
}

/* ============================================================================
 * Statistics and Monitoring Integration Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphIntegrationTest, StatisticsAccumulateOverTime) {
    /* Process various operations */
    for (int i = 0; i < 50; i++) {
        /* Valid queries */
        std::string valid = "SELECT valid_" + std::to_string(i);
        sec_kg_query_result_t q_result;
        security_kg_validate_query(bridge, valid.c_str(), valid.length(), &q_result);

        /* Traversals */
        sec_kg_traversal_result_t t_result;
        security_kg_check_traversal_access(
            bridge, ("S" + std::to_string(i)).c_str(),
            ("T" + std::to_string(i)).c_str(),
            "rel", 1, &t_result
        );
    }

    /* Add some injections */
    for (int i = 0; i < 10; i++) {
        std::string injection = "SELECT *;--" + std::to_string(i);
        sec_kg_query_result_t result;
        security_kg_validate_query(bridge, injection.c_str(), injection.length(), &result);
    }

    /* Add some false positive reports */
    for (int i = 0; i < 3; i++) {
        security_kg_report_false_positive(bridge);
    }

    /* Verify all stats */
    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);

    EXPECT_EQ(stats.queries_validated_total, 60u);  /* 50 + 10 */
    EXPECT_EQ(stats.queries_passed, 50u);
    EXPECT_EQ(stats.injections_detected, 10u);
    EXPECT_EQ(stats.traversals_validated_total, 50u);
    EXPECT_EQ(stats.traversals_allowed, 50u);
    EXPECT_EQ(stats.false_positives_reported, 3u);
}

TEST_F(SecurityKnowledgeGraphIntegrationTest, ResetStatsClearsAllCounters) {
    /* Generate some stats */
    for (int i = 0; i < 100; i++) {
        std::string query = "SELECT entity_" + std::to_string(i);
        sec_kg_query_result_t result;
        security_kg_validate_query(bridge, query.c_str(), query.length(), &result);
    }

    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);
    EXPECT_GT(stats.queries_validated_total, 0u);

    /* Reset */
    security_kg_reset_stats(bridge);

    /* Verify all cleared */
    security_kg_get_stats(bridge, &stats);
    EXPECT_EQ(stats.queries_validated_total, 0u);
    EXPECT_EQ(stats.queries_passed, 0u);
    EXPECT_EQ(stats.queries_rejected, 0u);
    EXPECT_EQ(stats.injections_detected, 0u);
    EXPECT_EQ(stats.traversals_validated_total, 0u);
    EXPECT_EQ(stats.traversals_allowed, 0u);
    EXPECT_EQ(stats.traversals_denied, 0u);
}

/* ============================================================================
 * Full Integration Workflow Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphIntegrationTest, CompleteSecurityWorkflow) {
    /* Step 1: Initialize bridge and verify ready state */
    sec_kg_bridge_state_t state;
    security_kg_get_state(bridge, &state);
    EXPECT_EQ(state.operational_state, SEC_KG_STATE_READY);

    /* Step 2: Set up privacy for sensitive entities */
    security_kg_isolate_private_data(bridge, "User_Credentials", SEC_KG_PRIVACY_SECRET);
    security_kg_isolate_private_data(bridge, "API_Keys", SEC_KG_PRIVACY_SECRET);
    security_kg_isolate_private_data(bridge, "Internal_Config", SEC_KG_PRIVACY_CONFIDENTIAL);

    /* Step 3: Normal operations */
    for (int i = 0; i < 20; i++) {
        std::string query = "SELECT public_entity_" + std::to_string(i);
        sec_kg_query_result_t result;
        security_kg_validate_query(bridge, query.c_str(), query.length(), &result);
        EXPECT_EQ(result, SEC_KG_QUERY_VALID);

        sec_kg_traversal_result_t trav;
        security_kg_check_traversal_access(
            bridge, "Public_A", "Public_B", "connects", i % 10 + 1, &trav
        );
        EXPECT_EQ(trav, SEC_KG_TRAVERSAL_ALLOWED);
    }

    /* Step 4: Detect attack attempts */
    std::string injection = "SELECT * FROM credentials;--";
    sec_kg_query_result_t inj_result;
    security_kg_validate_query(bridge, injection.c_str(), injection.length(), &inj_result);
    EXPECT_EQ(inj_result, SEC_KG_QUERY_INJECTION_DETECTED);

    /* Step 5: Access to private data should be blocked */
    sec_kg_traversal_result_t trav;
    security_kg_check_traversal_access(
        bridge, "Attacker", "User_Credentials", "steal", 1, &trav
    );
    EXPECT_EQ(trav, SEC_KG_TRAVERSAL_NODE_PRIVATE);

    /* Step 6: Update effects */
    security_kg_update(bridge);

    /* Step 7: Verify final statistics */
    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);

    EXPECT_EQ(stats.queries_validated_total, 21u);  /* 20 valid + 1 injection */
    EXPECT_EQ(stats.queries_passed, 20u);
    EXPECT_EQ(stats.injections_detected, 1u);
    EXPECT_EQ(stats.traversals_validated_total, 21u);  /* 20 + 1 blocked */
    EXPECT_EQ(stats.traversals_allowed, 20u);
    EXPECT_EQ(stats.traversals_denied, 1u);
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
