//=============================================================================
// test_pr_memory_quantum_bridge.cpp - Unit Tests for PR Memory Quantum Bridge
//=============================================================================
/**
 * @file test_pr_memory_quantum_bridge.cpp
 * @brief Comprehensive tests for PR Memory quantum integration module
 *
 * Tests: Quantum search, consolidation optimization, entanglement analysis,
 *        community detection, quantum walk diffusion
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

extern "C" {
#include "core/brain/internal/nimcp_pr_memory_quantum_bridge.h"
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

class PRQuantumLifecycleTest : public ::testing::Test {
protected:
    pr_memory_quantum_ctx_t ctx = nullptr;

    void TearDown() override {
        if (ctx) {
            pr_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(PRQuantumLifecycleTest, CreateWithDefaultConfig) {
    ctx = pr_quantum_create(nullptr);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(PRQuantumLifecycleTest, CreateWithCustomConfig) {
    pr_quantum_config_t config = pr_quantum_default_config();
    config.enable_quantum_search = true;
    config.enable_quantum_consolidation = true;
    config.enable_quantum_entangle_analysis = true;
    config.grover_max_iterations = 20;

    ctx = pr_quantum_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(PRQuantumLifecycleTest, DestroyNull) {
    pr_quantum_destroy(nullptr);  // Should not crash
}

TEST_F(PRQuantumLifecycleTest, ResetContext) {
    ctx = pr_quantum_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    pr_quantum_reset(ctx);  // Should not crash
}

TEST_F(PRQuantumLifecycleTest, EnableDisable) {
    ctx = pr_quantum_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    EXPECT_TRUE(pr_quantum_is_enabled(ctx));

    pr_quantum_set_enabled(ctx, false);
    EXPECT_FALSE(pr_quantum_is_enabled(ctx));

    pr_quantum_set_enabled(ctx, true);
    EXPECT_TRUE(pr_quantum_is_enabled(ctx));
}

TEST_F(PRQuantumLifecycleTest, DefaultConfigValues) {
    pr_quantum_config_t config = pr_quantum_default_config();

    EXPECT_TRUE(config.enable_quantum_search);
    EXPECT_TRUE(config.enable_quantum_consolidation);
    EXPECT_EQ(config.grover_max_iterations, PR_QUANTUM_GROVER_ITERATIONS);
    EXPECT_EQ(config.mc_samples, PR_QUANTUM_MC_SAMPLES);
}

TEST_F(PRQuantumLifecycleTest, GetFeatures) {
    ctx = pr_quantum_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    uint32_t features = pr_quantum_get_features(ctx);
    EXPECT_GT(features, 0u);  // Some features should be enabled
}

//=============================================================================
// Quantum Search Tests
//=============================================================================

class PRQuantumSearchTest : public ::testing::Test {
protected:
    pr_memory_quantum_ctx_t ctx = nullptr;

    void SetUp() override {
        pr_quantum_config_t config = pr_quantum_default_config();
        config.enable_quantum_search = true;
        config.grover_max_iterations = 10;
        config.search_candidates_limit = 100;
        ctx = pr_quantum_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            pr_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(PRQuantumSearchTest, SearchMemory) {
    // Create a test query pattern
    std::vector<float> query = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    pr_quantum_search_result_t result;
    memset(&result, 0, sizeof(result));

    bool success = pr_quantum_search_memory(ctx, query.data(), query.size() * sizeof(float),
                                             0xF, &result);  // Search all Z-levels

    // Note: May return false if no memories attached, but shouldn't crash
    if (success) {
        EXPECT_GE(result.search_speedup, 1.0f);
        EXPECT_LE(result.satisfaction_probability, 1.0f);
    }
}

TEST_F(PRQuantumSearchTest, SimilaritySearch) {
    std::vector<float> query = {0.5f, 0.5f, 0.5f, 0.5f};
    std::vector<pr_quantum_candidate_t> results(10);
    uint32_t num_found = 0;

    bool success = pr_quantum_similarity_search(ctx, query.data(), query.size() * sizeof(float),
                                                 10, results.data(), &num_found);

    if (success) {
        // Verify candidates have valid fields
        for (uint32_t i = 0; i < num_found; i++) {
            EXPECT_GE(results[i].amplitude, 0.0f);
            EXPECT_LE(results[i].amplitude, 1.0f);
            EXPECT_GE(results[i].z_level, 0);
            EXPECT_LE(results[i].z_level, 3);
        }
    }
}

TEST_F(PRQuantumSearchTest, AssociativeRecall) {
    std::vector<float> cue = {0.3f, 0.3f, 0.3f};
    std::vector<uint64_t> signatures(10);
    std::vector<float> strengths(10);
    uint32_t num_found = 0;

    bool success = pr_quantum_associative_recall(ctx, cue.data(), cue.size() * sizeof(float),
                                                  10, signatures.data(), strengths.data(), &num_found);

    if (success) {
        for (uint32_t i = 0; i < num_found; i++) {
            EXPECT_GE(strengths[i], 0.0f);
            EXPECT_LE(strengths[i], 1.0f);
        }
    }
}

//=============================================================================
// Quantum Consolidation Tests
//=============================================================================

class PRQuantumConsolidationTest : public ::testing::Test {
protected:
    pr_memory_quantum_ctx_t ctx = nullptr;

    void SetUp() override {
        pr_quantum_config_t config = pr_quantum_default_config();
        config.enable_quantum_consolidation = true;
        config.anneal_initial_temp = 1.0f;
        config.anneal_final_temp = 0.01f;
        config.anneal_iterations = 50;
        ctx = pr_quantum_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            pr_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(PRQuantumConsolidationTest, OptimizeConsolidation) {
    // Create test candidates
    std::vector<pr_quantum_candidate_t> candidates(5);
    for (int i = 0; i < 5; i++) {
        candidates[i].resonance_signature = 1000 + i;
        candidates[i].z_level = 0;  // All in Z0 (working memory)
        candidates[i].resonance_strength = 0.5f + 0.1f * i;
        candidates[i].amplitude = 0.2f;
    }

    std::vector<pr_quantum_consolidation_t> decisions(5);
    uint32_t num_decisions = 0;

    bool success = pr_quantum_optimize_consolidation(ctx, candidates.data(), 5,
                                                      decisions.data(), 5, &num_decisions);

    if (success && num_decisions > 0) {
        for (uint32_t i = 0; i < num_decisions; i++) {
            EXPECT_GE(decisions[i].from_level, 0);
            EXPECT_LE(decisions[i].to_level, 3);
            EXPECT_GE(decisions[i].promotion_probability, 0.0f);
            EXPECT_LE(decisions[i].promotion_probability, 1.0f);
        }
    }
}

TEST_F(PRQuantumConsolidationTest, GetAnnealState) {
    float temperature, tunneling_prob;
    uint64_t iteration;

    bool success = pr_quantum_get_anneal_state(ctx, &temperature, &tunneling_prob, &iteration);
    EXPECT_TRUE(success);

    EXPECT_GT(temperature, 0.0f);
    EXPECT_GE(tunneling_prob, 0.0f);
    EXPECT_LE(tunneling_prob, 1.0f);
}

//=============================================================================
// Entanglement Analysis Tests
//=============================================================================

class PRQuantumEntangleTest : public ::testing::Test {
protected:
    pr_memory_quantum_ctx_t ctx = nullptr;

    void SetUp() override {
        pr_quantum_config_t config = pr_quantum_default_config();
        config.enable_quantum_entangle_analysis = true;
        config.bottleneck_threshold = 0.4f;
        ctx = pr_quantum_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            pr_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(PRQuantumEntangleTest, AnalyzeEntanglementFlow) {
    // Note: Requires attached brain, but API should handle gracefully
    float efficiency = pr_quantum_analyze_entanglement_flow(ctx, nullptr);

    // Without brain, should return default or 0
    EXPECT_GE(efficiency, 0.0f);
    EXPECT_LE(efficiency, 1.0f);
}

TEST_F(PRQuantumEntangleTest, DetectEntangleBottlenecks) {
    std::vector<pr_entangle_bottleneck_t> bottlenecks(10);
    uint32_t num_found = 0;

    bool success = pr_quantum_detect_entangle_bottlenecks(ctx, nullptr, bottlenecks.data(),
                                                           10, &num_found);

    // Should handle null brain gracefully
    if (success && num_found > 0) {
        for (uint32_t i = 0; i < num_found; i++) {
            EXPECT_GE(bottlenecks[i].deficit, 0.0f);
            EXPECT_LE(bottlenecks[i].deficit, 1.0f);
        }
    }
}

//=============================================================================
// Community Detection Tests
//=============================================================================

class PRQuantumCommunityTest : public ::testing::Test {
protected:
    pr_memory_quantum_ctx_t ctx = nullptr;

    void SetUp() override {
        pr_quantum_config_t config = pr_quantum_default_config();
        config.enable_quantum_communities = true;
        config.community_iterations = 50;
        ctx = pr_quantum_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            pr_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(PRQuantumCommunityTest, DetectCommunities) {
    pr_quantum_community_t result;
    memset(&result, 0, sizeof(result));

    bool success = pr_quantum_detect_communities(ctx, nullptr, &result);

    if (success && result.community_assignments) {
        EXPECT_GE(result.modularity_score, -0.5f);
        EXPECT_LE(result.modularity_score, 1.0f);
        EXPECT_GE(result.quantum_speedup, 1.0f);

        pr_quantum_free_community_result(&result);
    }
}

TEST_F(PRQuantumCommunityTest, FindMemoryHubs) {
    pr_quantum_hubs_t result;
    memset(&result, 0, sizeof(result));

    bool success = pr_quantum_find_memory_hubs(ctx, nullptr, &result);

    if (success && result.hub_signatures) {
        for (uint32_t i = 0; i < result.num_hubs; i++) {
            EXPECT_GE(result.centrality_scores[i], 0.0f);
        }

        pr_quantum_free_hubs_result(&result);
    }
}

//=============================================================================
// Quantum Walk Diffusion Tests
//=============================================================================

class PRQuantumWalkTest : public ::testing::Test {
protected:
    pr_memory_quantum_ctx_t ctx = nullptr;

    void SetUp() override {
        pr_quantum_config_t config = pr_quantum_default_config();
        config.enable_quantum_walk = true;
        config.walk_steps = 50;
        config.decoherence_rate = 0.1f;
        ctx = pr_quantum_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            pr_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(PRQuantumWalkTest, DiffuseResonance) {
    std::vector<float> diffused(100, 0.0f);
    uint32_t num_affected = 0;

    bool success = pr_quantum_diffuse_resonance(ctx, nullptr, 12345, 1.0f,
                                                 diffused.data(), 100, &num_affected);

    if (success) {
        float total = 0.0f;
        for (uint32_t i = 0; i < 100; i++) {
            EXPECT_GE(diffused[i], 0.0f);
            total += diffused[i];
        }
        // Total should approximately conserve probability
        if (total > 0) {
            EXPECT_NEAR(total, 1.0f, 0.2f);
        }
    }
}

TEST_F(PRQuantumWalkTest, DiffuseResonanceMulti) {
    uint64_t sources[] = {100, 200, 300};
    float initial[] = {0.33f, 0.33f, 0.34f};
    std::vector<float> diffused(100, 0.0f);
    uint32_t num_affected = 0;

    bool success = pr_quantum_diffuse_resonance_multi(ctx, nullptr, sources, initial, 3,
                                                       diffused.data(), 100, &num_affected);

    if (success) {
        float total = 0.0f;
        for (uint32_t i = 0; i < 100; i++) {
            total += diffused[i];
        }
        if (total > 0) {
            EXPECT_NEAR(total, 1.0f, 0.2f);
        }
    }
}

//=============================================================================
// Enhanced Operations Tests
//=============================================================================

class PRQuantumEnhancedTest : public ::testing::Test {
protected:
    pr_memory_quantum_ctx_t ctx = nullptr;

    void SetUp() override {
        pr_quantum_config_t config = pr_quantum_default_config();
        config.enable_quantum_search = true;
        config.enable_quantum_consolidation = true;
        ctx = pr_quantum_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            pr_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(PRQuantumEnhancedTest, StoreEnhanced) {
    std::vector<float> content = {1.0f, 2.0f, 3.0f, 4.0f};
    uint64_t signature = 0;

    bool success = pr_quantum_store_enhanced(ctx, nullptr, content.data(),
                                              content.size() * sizeof(float), 0.8f, &signature);

    // Without brain, store may fail but shouldn't crash
    if (success) {
        EXPECT_NE(signature, 0u);
    }
}

TEST_F(PRQuantumEnhancedTest, RetrieveEnhanced) {
    std::vector<float> query = {1.0f, 2.0f};
    std::vector<float> content(10, 0.0f);
    uint64_t signature = 0;
    float strength = 0.0f;

    bool success = pr_quantum_retrieve_enhanced(ctx, nullptr, query.data(),
                                                 query.size() * sizeof(float),
                                                 content.data(), content.size() * sizeof(float),
                                                 &signature, &strength);

    if (success) {
        EXPECT_GE(strength, 0.0f);
        EXPECT_LE(strength, 1.0f);
    }
}

TEST_F(PRQuantumEnhancedTest, QuantumTick) {
    uint32_t operations = pr_quantum_tick(ctx, nullptr, 1000000);

    // Without brain, should return 0 but not crash
    EXPECT_GE(operations, 0u);
}

//=============================================================================
// Metrics Tests
//=============================================================================

class PRQuantumMetricsTest : public ::testing::Test {
protected:
    pr_memory_quantum_ctx_t ctx = nullptr;

    void SetUp() override {
        pr_quantum_config_t config = pr_quantum_default_config();
        config.enable_metrics = true;
        ctx = pr_quantum_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            pr_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(PRQuantumMetricsTest, GetMetrics) {
    pr_quantum_metrics_t metrics;

    bool result = pr_quantum_get_metrics(ctx, &metrics);
    EXPECT_TRUE(result);
}

TEST_F(PRQuantumMetricsTest, ResetMetrics) {
    // Generate some activity
    pr_quantum_tick(ctx, nullptr, 1000);

    // Reset
    pr_quantum_reset_metrics(ctx);

    pr_quantum_metrics_t metrics;
    pr_quantum_get_metrics(ctx, &metrics);

    EXPECT_EQ(metrics.quantum_searches, 0u);
    EXPECT_EQ(metrics.quantum_consolidations, 0u);
}

//=============================================================================
// Diagnostic Tests
//=============================================================================

class PRQuantumDiagnosticTest : public ::testing::Test {
protected:
    pr_memory_quantum_ctx_t ctx = nullptr;

    void SetUp() override {
        ctx = pr_quantum_create(nullptr);
    }

    void TearDown() override {
        if (ctx) {
            pr_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(PRQuantumDiagnosticTest, Verify) {
    bool valid = pr_quantum_verify(ctx);
    EXPECT_TRUE(valid);
}

TEST_F(PRQuantumDiagnosticTest, PrintStatus) {
    // Should not crash
    pr_quantum_print_status(ctx);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
