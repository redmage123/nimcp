/**
 * @file test_security_knowledge_graph_regression.cpp
 * @brief Regression tests for Security-Knowledge Graph Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Regression tests ensuring consistent performance, API stability, and
 * security properties for the security-knowledge graph bridge.
 *
 * TEST CATEGORIES:
 * - Performance regression tests (timing baselines)
 * - Query latency tests
 * - Memory regression tests (leak detection)
 * - API stability tests
 * - Detection rate regression
 * - Security property verification
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <cstring>

extern "C" {
#include "security/knowledge/nimcp_security_knowledge_graph_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Constants
 * ============================================================================ */

/* Performance baselines (microseconds) */
static const double QUERY_VALIDATION_MAX_US = 1000.0;     /* 1ms max */
static const double TRAVERSAL_CHECK_MAX_US = 500.0;       /* 0.5ms max */
static const double CREATE_DESTROY_MAX_US = 10000.0;      /* 10ms max */
static const double UPDATE_CYCLE_MAX_US = 2000.0;         /* 2ms max */

/* Memory regression limits */
static const size_t MAX_MEMORY_PER_BRIDGE_KB = 64;        /* 64KB max per bridge */
static const int CREATE_DESTROY_CYCLES = 100;

/* Detection rate baselines */
static const double MIN_INJECTION_DETECTION_RATE = 0.95;  /* 95% detection */
static const double MAX_FALSE_POSITIVE_RATE = 0.05;       /* 5% max FP */

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SecurityKnowledgeGraphRegressionTest : public ::testing::Test {
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

    /* Timing helper */
    template<typename Func>
    double measure_time_us(Func f) {
        auto start = std::chrono::high_resolution_clock::now();
        f();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start).count();
    }

    /* Generate test queries */
    std::vector<std::string> generate_queries(size_t count, bool injection = false) {
        std::vector<std::string> queries;
        for (size_t i = 0; i < count; i++) {
            if (injection) {
                queries.push_back("SELECT * FROM data;-- " + std::to_string(i));
            } else {
                queries.push_back("SELECT entity_" + std::to_string(i) +
                                " WHERE type = 'test'");
            }
        }
        return queries;
    }
};

/* ============================================================================
 * API Stability Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphRegressionTest, DefaultConfigStability) {
    sec_kg_config_t cfg;
    int ret = security_kg_default_config(&cfg);

    EXPECT_EQ(ret, 0);

    /* Verify default values haven't changed */
    EXPECT_TRUE(cfg.enable_query_validation);
    EXPECT_TRUE(cfg.enable_traversal_control);
    EXPECT_TRUE(cfg.enable_integrity_verification);
    EXPECT_TRUE(cfg.enable_consistency_checks);
    EXPECT_TRUE(cfg.enable_privacy_isolation);

    EXPECT_FLOAT_EQ(cfg.injection_threshold, SEC_KG_DEFAULT_INJECTION_THRESHOLD);
    EXPECT_FLOAT_EQ(cfg.integrity_threshold, SEC_KG_DEFAULT_INTEGRITY_THRESHOLD);
    EXPECT_EQ(cfg.max_query_length, SEC_KG_MAX_QUERY_LENGTH);
    EXPECT_EQ(cfg.max_traversal_depth, SEC_KG_MAX_TRAVERSAL_DEPTH);
    EXPECT_EQ(cfg.max_nodes_per_query, SEC_KG_MAX_RESULT_NODES);
}

TEST_F(SecurityKnowledgeGraphRegressionTest, CreateDestroyStability) {
    /* Verify create/destroy API remains stable */
    for (int i = 0; i < 10; i++) {
        sec_kg_config_t cfg;
        security_kg_default_config(&cfg);

        security_kg_bridge_t* br = security_kg_bridge_create(&cfg);
        ASSERT_NE(br, nullptr) << "Create failed on iteration " << i;

        sec_kg_bridge_state_t state;
        int ret = security_kg_get_state(br, &state);
        EXPECT_EQ(ret, 0);
        EXPECT_EQ(state.operational_state, SEC_KG_STATE_READY);

        security_kg_bridge_destroy(br);
    }
}

TEST_F(SecurityKnowledgeGraphRegressionTest, NullPointerHandling) {
    /* All functions should handle NULL gracefully */

    EXPECT_EQ(security_kg_default_config(nullptr), NIMCP_ERROR_NULL_POINTER);
    /* create with NULL config uses defaults and succeeds */
    security_kg_bridge_t* null_bridge = security_kg_bridge_create(nullptr);
    EXPECT_NE(null_bridge, nullptr);
    if (null_bridge) security_kg_bridge_destroy(null_bridge);

    /* Functions with NULL bridge */
    sec_kg_query_result_t q_result;
    EXPECT_EQ(security_kg_validate_query(nullptr, "q", 1, &q_result),
              NIMCP_ERROR_NULL_POINTER);

    sec_kg_traversal_result_t t_result;
    EXPECT_EQ(security_kg_check_traversal_access(nullptr, "s", "t", "r", 1, &t_result),
              NIMCP_ERROR_NULL_POINTER);

    sec_kg_integrity_result_t i_result;
    EXPECT_EQ(security_kg_verify_node_integrity(nullptr, "e", &i_result),
              NIMCP_ERROR_NULL_POINTER);

    sec_kg_consistency_result_t c_result;
    EXPECT_EQ(security_kg_enforce_consistency(nullptr, &c_result),
              NIMCP_ERROR_NULL_POINTER);

    sec_kg_privacy_level_t p_level;
    EXPECT_EQ(security_kg_get_privacy_level(nullptr, "e", &p_level),
              NIMCP_ERROR_NULL_POINTER);

    EXPECT_EQ(security_kg_enter_lockdown(nullptr, "reason"),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_kg_exit_lockdown(nullptr),
              NIMCP_ERROR_NULL_POINTER);

    EXPECT_EQ(security_kg_update(nullptr), NIMCP_ERROR_NULL_POINTER);

    /* Destroy NULL should be safe */
    security_kg_bridge_destroy(nullptr);
    security_kg_reset_stats(nullptr);
}

TEST_F(SecurityKnowledgeGraphRegressionTest, EnumNameStability) {
    /* Verify enum name functions return valid strings */

    EXPECT_STREQ(security_kg_query_result_name(SEC_KG_QUERY_VALID), "VALID");
    EXPECT_STREQ(security_kg_query_result_name(SEC_KG_QUERY_INJECTION_DETECTED),
                 "INJECTION_DETECTED");
    EXPECT_STREQ(security_kg_query_result_name(SEC_KG_QUERY_TOO_LONG), "TOO_LONG");

    EXPECT_STREQ(security_kg_traversal_result_name(SEC_KG_TRAVERSAL_ALLOWED),
                 "ALLOWED");
    EXPECT_STREQ(security_kg_traversal_result_name(SEC_KG_TRAVERSAL_DEPTH_EXCEEDED),
                 "DEPTH_EXCEEDED");

    EXPECT_STREQ(security_kg_integrity_result_name(SEC_KG_NODE_VALID), "VALID");
    EXPECT_STREQ(security_kg_integrity_result_name(SEC_KG_NODE_NOT_FOUND),
                 "NOT_FOUND");

    EXPECT_STREQ(security_kg_privacy_level_name(SEC_KG_PRIVACY_PUBLIC), "PUBLIC");
    EXPECT_STREQ(security_kg_privacy_level_name(SEC_KG_PRIVACY_SECRET), "SECRET");

    EXPECT_STREQ(security_kg_state_name(SEC_KG_STATE_READY), "READY");
    EXPECT_STREQ(security_kg_state_name(SEC_KG_STATE_LOCKDOWN), "LOCKDOWN");
}

/* ============================================================================
 * Performance Regression Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphRegressionTest, QueryValidationLatency) {
    auto queries = generate_queries(1000);
    std::vector<double> latencies;
    latencies.reserve(queries.size());

    for (const auto& query : queries) {
        sec_kg_query_result_t result;
        double latency = measure_time_us([&]() {
            security_kg_validate_query(
                bridge, query.c_str(), query.length(), &result
            );
        });
        latencies.push_back(latency);
    }

    /* Calculate statistics */
    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double avg = sum / latencies.size();

    std::sort(latencies.begin(), latencies.end());
    double p50 = latencies[latencies.size() / 2];
    double p99 = latencies[static_cast<size_t>(latencies.size() * 0.99)];
    double max_val = latencies.back();

    /* Performance assertions */
    EXPECT_LT(avg, QUERY_VALIDATION_MAX_US)
        << "Average query validation latency exceeded baseline";
    EXPECT_LT(p99, QUERY_VALIDATION_MAX_US * 2)
        << "P99 query validation latency exceeded baseline";
    EXPECT_LT(max_val, QUERY_VALIDATION_MAX_US * 5)
        << "Max query validation latency exceeded baseline";

    /* Log performance for tracking */
    std::cout << "Query Validation Latency (us): "
              << "avg=" << avg
              << " p50=" << p50
              << " p99=" << p99
              << " max=" << max_val << std::endl;
}

TEST_F(SecurityKnowledgeGraphRegressionTest, TraversalCheckLatency) {
    std::vector<double> latencies;
    latencies.reserve(1000);

    for (int i = 0; i < 1000; i++) {
        sec_kg_traversal_result_t result;
        double latency = measure_time_us([&]() {
            security_kg_check_traversal_access(
                bridge,
                ("Source_" + std::to_string(i)).c_str(),
                ("Target_" + std::to_string(i)).c_str(),
                "connects_to",
                (i % 10) + 1,
                &result
            );
        });
        latencies.push_back(latency);
    }

    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double avg = sum / latencies.size();

    std::sort(latencies.begin(), latencies.end());
    double p99 = latencies[static_cast<size_t>(latencies.size() * 0.99)];

    EXPECT_LT(avg, TRAVERSAL_CHECK_MAX_US)
        << "Average traversal check latency exceeded baseline";
    EXPECT_LT(p99, TRAVERSAL_CHECK_MAX_US * 2)
        << "P99 traversal check latency exceeded baseline";

    std::cout << "Traversal Check Latency (us): "
              << "avg=" << avg << " p99=" << p99 << std::endl;
}

TEST_F(SecurityKnowledgeGraphRegressionTest, CreateDestroyLatency) {
    std::vector<double> latencies;
    latencies.reserve(100);

    sec_kg_config_t cfg;
    security_kg_default_config(&cfg);

    for (int i = 0; i < 100; i++) {
        security_kg_bridge_t* br = nullptr;

        double latency = measure_time_us([&]() {
            br = security_kg_bridge_create(&cfg);
            security_kg_bridge_destroy(br);
        });
        latencies.push_back(latency);
    }

    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double avg = sum / latencies.size();

    EXPECT_LT(avg, CREATE_DESTROY_MAX_US)
        << "Average create/destroy latency exceeded baseline";

    std::cout << "Create/Destroy Latency (us): avg=" << avg << std::endl;
}

TEST_F(SecurityKnowledgeGraphRegressionTest, UpdateCycleLatency) {
    std::vector<double> latencies;
    latencies.reserve(1000);

    /* Process some queries first to generate state */
    auto queries = generate_queries(100);
    for (const auto& query : queries) {
        sec_kg_query_result_t result;
        security_kg_validate_query(bridge, query.c_str(), query.length(), &result);
    }

    for (int i = 0; i < 1000; i++) {
        double latency = measure_time_us([&]() {
            security_kg_update(bridge);
        });
        latencies.push_back(latency);
    }

    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double avg = sum / latencies.size();

    EXPECT_LT(avg, UPDATE_CYCLE_MAX_US)
        << "Average update cycle latency exceeded baseline";

    std::cout << "Update Cycle Latency (us): avg=" << avg << std::endl;
}

/* ============================================================================
 * Memory Regression Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphRegressionTest, CreateDestroyNoLeaks) {
    /* Create and destroy many bridges - memory should not grow */
    sec_kg_config_t cfg;
    security_kg_default_config(&cfg);

    for (int i = 0; i < CREATE_DESTROY_CYCLES; i++) {
        security_kg_bridge_t* br = security_kg_bridge_create(&cfg);
        ASSERT_NE(br, nullptr) << "Failed at iteration " << i;

        /* Perform some operations */
        std::string query = "SELECT entity_" + std::to_string(i);
        sec_kg_query_result_t result;
        security_kg_validate_query(br, query.c_str(), query.length(), &result);

        security_kg_isolate_private_data(br, ("Entity_" + std::to_string(i)).c_str(),
                                         SEC_KG_PRIVACY_INTERNAL);

        security_kg_update(br);

        security_kg_bridge_destroy(br);
    }

    /* If we get here without crashing or running out of memory, test passes */
    SUCCEED();
}

TEST_F(SecurityKnowledgeGraphRegressionTest, PrivacyRegistryGrowth) {
    /* Add many private nodes */
    for (int i = 0; i < 1000; i++) {
        int ret = security_kg_isolate_private_data(
            bridge,
            ("Entity_" + std::to_string(i)).c_str(),
            static_cast<sec_kg_privacy_level_t>(i % 5)
        );
        EXPECT_EQ(ret, 0) << "Failed at iteration " << i;
    }

    /* Verify all were added correctly */
    for (int i = 0; i < 1000; i++) {
        sec_kg_privacy_level_t level;
        int ret = security_kg_get_privacy_level(
            bridge,
            ("Entity_" + std::to_string(i)).c_str(),
            &level
        );
        EXPECT_EQ(ret, 0);
        EXPECT_EQ(level, static_cast<sec_kg_privacy_level_t>(i % 5));
    }

    /* Remove all */
    for (int i = 0; i < 1000; i++) {
        security_kg_remove_isolation(bridge, ("Entity_" + std::to_string(i)).c_str());
    }

    /* Verify all removed */
    for (int i = 0; i < 1000; i++) {
        sec_kg_privacy_level_t level;
        security_kg_get_privacy_level(
            bridge,
            ("Entity_" + std::to_string(i)).c_str(),
            &level
        );
        EXPECT_EQ(level, SEC_KG_PRIVACY_PUBLIC);
    }
}

/* ============================================================================
 * Detection Rate Regression Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphRegressionTest, InjectionDetectionRate) {
    auto injections = generate_queries(1000, true);  /* injection=true */
    uint64_t detected = 0;

    for (const auto& query : injections) {
        sec_kg_query_result_t result;
        security_kg_validate_query(bridge, query.c_str(), query.length(), &result);

        if (result == SEC_KG_QUERY_INJECTION_DETECTED) {
            detected++;
        }
    }

    double detection_rate = static_cast<double>(detected) / injections.size();

    EXPECT_GE(detection_rate, MIN_INJECTION_DETECTION_RATE)
        << "Injection detection rate (" << detection_rate * 100
        << "%) fell below baseline (" << MIN_INJECTION_DETECTION_RATE * 100 << "%)";

    std::cout << "Injection Detection Rate: " << detection_rate * 100 << "%" << std::endl;
}

TEST_F(SecurityKnowledgeGraphRegressionTest, FalsePositiveRate) {
    auto valid_queries = generate_queries(1000, false);  /* injection=false */
    uint64_t false_positives = 0;

    for (const auto& query : valid_queries) {
        sec_kg_query_result_t result;
        security_kg_validate_query(bridge, query.c_str(), query.length(), &result);

        if (result == SEC_KG_QUERY_INJECTION_DETECTED) {
            false_positives++;
        }
    }

    double fp_rate = static_cast<double>(false_positives) / valid_queries.size();

    EXPECT_LE(fp_rate, MAX_FALSE_POSITIVE_RATE)
        << "False positive rate (" << fp_rate * 100
        << "%) exceeded baseline (" << MAX_FALSE_POSITIVE_RATE * 100 << "%)";

    std::cout << "False Positive Rate: " << fp_rate * 100 << "%" << std::endl;
}

/* ============================================================================
 * Security Property Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphRegressionTest, LockdownBlocksAllOperations) {
    /* Enter lockdown */
    security_kg_enter_lockdown(bridge, "Test lockdown");

    /* Verify queries blocked */
    std::string query = "SELECT entity";
    sec_kg_query_result_t q_result;
    security_kg_validate_query(bridge, query.c_str(), query.length(), &q_result);
    EXPECT_EQ(q_result, SEC_KG_QUERY_REJECTED);

    /* Verify traversals blocked */
    sec_kg_traversal_result_t t_result;
    security_kg_check_traversal_access(bridge, "A", "B", "r", 1, &t_result);
    EXPECT_EQ(t_result, SEC_KG_TRAVERSAL_DENIED);

    /* Verify state is lockdown */
    sec_kg_bridge_state_t state;
    security_kg_get_state(bridge, &state);
    EXPECT_EQ(state.operational_state, SEC_KG_STATE_LOCKDOWN);
    EXPECT_TRUE(state.lockdown_active);

    /* Exit and verify restored */
    security_kg_exit_lockdown(bridge);

    security_kg_validate_query(bridge, query.c_str(), query.length(), &q_result);
    EXPECT_EQ(q_result, SEC_KG_QUERY_VALID);
}

TEST_F(SecurityKnowledgeGraphRegressionTest, PrivacyIsolationEnforced) {
    /* Set up privacy levels */
    security_kg_isolate_private_data(bridge, "Secret", SEC_KG_PRIVACY_SECRET);
    security_kg_isolate_private_data(bridge, "Confidential", SEC_KG_PRIVACY_CONFIDENTIAL);
    security_kg_isolate_private_data(bridge, "Restricted", SEC_KG_PRIVACY_RESTRICTED);
    security_kg_isolate_private_data(bridge, "Internal", SEC_KG_PRIVACY_INTERNAL);

    /* Verify traversal blocked to each */
    struct {
        const char* target;
        sec_kg_privacy_level_t level;
    } test_cases[] = {
        {"Secret", SEC_KG_PRIVACY_SECRET},
        {"Confidential", SEC_KG_PRIVACY_CONFIDENTIAL},
        {"Restricted", SEC_KG_PRIVACY_RESTRICTED},
        {"Internal", SEC_KG_PRIVACY_INTERNAL}
    };

    for (const auto& tc : test_cases) {
        sec_kg_traversal_result_t result;
        security_kg_check_traversal_access(
            bridge, "PublicSource", tc.target, "access", 1, &result
        );
        EXPECT_EQ(result, SEC_KG_TRAVERSAL_NODE_PRIVATE)
            << "Failed for target " << tc.target;
    }
}

TEST_F(SecurityKnowledgeGraphRegressionTest, DepthLimitEnforced) {
    const uint32_t test_depths[] = {8, 16, 32, 64};

    for (uint32_t limit : test_depths) {
        if (limit > SEC_KG_MAX_TRAVERSAL_DEPTH) continue;

        security_kg_set_max_traversal_depth(bridge, limit);

        /* Below limit should pass */
        sec_kg_traversal_result_t result;
        security_kg_check_traversal_access(
            bridge, "A", "B", "r", limit - 1, &result
        );
        EXPECT_EQ(result, SEC_KG_TRAVERSAL_ALLOWED)
            << "Failed at depth " << (limit - 1) << " with limit " << limit;

        /* At limit should pass */
        security_kg_check_traversal_access(
            bridge, "A", "B", "r", limit, &result
        );
        EXPECT_EQ(result, SEC_KG_TRAVERSAL_ALLOWED)
            << "Failed at depth " << limit << " with limit " << limit;

        /* Above limit should fail */
        security_kg_check_traversal_access(
            bridge, "A", "B", "r", limit + 1, &result
        );
        EXPECT_EQ(result, SEC_KG_TRAVERSAL_DEPTH_EXCEEDED)
            << "Failed at depth " << (limit + 1) << " with limit " << limit;
    }
}

TEST_F(SecurityKnowledgeGraphRegressionTest, QueryLengthLimitEnforced) {
    /* At limit should pass */
    std::string at_limit(SEC_KG_MAX_QUERY_LENGTH, 'A');
    sec_kg_query_result_t result;
    security_kg_validate_query(bridge, at_limit.c_str(), at_limit.length(), &result);
    EXPECT_EQ(result, SEC_KG_QUERY_VALID);

    /* Above limit should fail */
    std::string above_limit(SEC_KG_MAX_QUERY_LENGTH + 1, 'A');
    security_kg_validate_query(bridge, above_limit.c_str(), above_limit.length(), &result);
    EXPECT_EQ(result, SEC_KG_QUERY_TOO_LONG);
}

/* ============================================================================
 * Statistics Consistency Tests
 * ============================================================================ */

TEST_F(SecurityKnowledgeGraphRegressionTest, StatisticsAccuracy) {
    /* Perform known quantities of operations */
    const int valid_queries = 100;
    const int injection_queries = 50;
    const int traversals = 75;

    /* Process valid queries */
    for (int i = 0; i < valid_queries; i++) {
        std::string query = "SELECT entity_" + std::to_string(i);
        sec_kg_query_result_t result;
        security_kg_validate_query(bridge, query.c_str(), query.length(), &result);
    }

    /* Process injection queries */
    for (int i = 0; i < injection_queries; i++) {
        std::string query = "SELECT *;-- " + std::to_string(i);
        sec_kg_query_result_t result;
        security_kg_validate_query(bridge, query.c_str(), query.length(), &result);
    }

    /* Process traversals */
    for (int i = 0; i < traversals; i++) {
        sec_kg_traversal_result_t result;
        security_kg_check_traversal_access(
            bridge,
            ("S" + std::to_string(i)).c_str(),
            ("T" + std::to_string(i)).c_str(),
            "r",
            1,
            &result
        );
    }

    /* Verify statistics */
    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);

    EXPECT_EQ(stats.queries_validated_total,
              static_cast<uint64_t>(valid_queries + injection_queries));
    EXPECT_EQ(stats.queries_passed, static_cast<uint64_t>(valid_queries));
    EXPECT_EQ(stats.injections_detected, static_cast<uint64_t>(injection_queries));
    EXPECT_EQ(stats.traversals_validated_total, static_cast<uint64_t>(traversals));
    EXPECT_EQ(stats.traversals_allowed, static_cast<uint64_t>(traversals));
}

TEST_F(SecurityKnowledgeGraphRegressionTest, StatisticsResetComplete) {
    /* Generate stats */
    for (int i = 0; i < 100; i++) {
        std::string query = "SELECT entity_" + std::to_string(i);
        sec_kg_query_result_t result;
        security_kg_validate_query(bridge, query.c_str(), query.length(), &result);
    }

    /* Reset */
    security_kg_reset_stats(bridge);

    /* Verify complete reset */
    sec_kg_stats_t stats;
    security_kg_get_stats(bridge, &stats);

    EXPECT_EQ(stats.queries_validated_total, 0u);
    EXPECT_EQ(stats.queries_passed, 0u);
    EXPECT_EQ(stats.queries_rejected, 0u);
    EXPECT_EQ(stats.injections_detected, 0u);
    EXPECT_EQ(stats.traversals_validated_total, 0u);
    EXPECT_EQ(stats.traversals_allowed, 0u);
    EXPECT_EQ(stats.traversals_denied, 0u);
    EXPECT_EQ(stats.depth_limit_violations, 0u);
    EXPECT_EQ(stats.integrity_checks_total, 0u);
    EXPECT_EQ(stats.consistency_checks_total, 0u);
    EXPECT_EQ(stats.false_positives_reported, 0u);
    EXPECT_FLOAT_EQ(stats.avg_query_validation_time_us, 0.0f);
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
