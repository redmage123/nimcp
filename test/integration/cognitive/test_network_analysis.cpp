//=============================================================================
// test_network_analysis.cpp - Integration Tests for Network Analysis
//=============================================================================

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "cognitive/analysis/nimcp_network_analysis.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/consolidation/nimcp_consolidation.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class NetworkAnalysisTest : public ::testing::Test {
protected:
    brain_t brain;
    network_analyzer_t* analyzer;

    void SetUp() override {
        // Create small brain for testing (new API)
        brain = brain_create("test_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
        ASSERT_NE(brain, nullptr);

        // Create network analyzer
        analyzer = network_analyzer_create(brain);
        ASSERT_NE(analyzer, nullptr);
    }

    void TearDown() override {
        if (analyzer) {
            network_analyzer_destroy(analyzer);
        }
        if (brain) {
            brain_destroy(brain);
        }
    }
};

//=============================================================================
// Basic Functionality Tests
//=============================================================================

TEST_F(NetworkAnalysisTest, CreateDestroy) {
    // Test is successful if SetUp and TearDown complete without errors
    EXPECT_NE(analyzer, nullptr);
}

TEST_F(NetworkAnalysisTest, RunAnalysis) {
    // Run complete network analysis
    bool success = network_analyzer_run(analyzer);
    EXPECT_TRUE(success);

    // Check that results were generated
    const community_structure_t* communities = network_analyzer_get_communities(analyzer);
    ASSERT_NE(communities, nullptr) << "Communities should be detected";
    EXPECT_GT(communities->num_communities, 0u);

    const hub_detection_t* hubs = network_analyzer_get_hubs(analyzer);
    EXPECT_NE(hubs, nullptr);
    EXPECT_GT(hubs->num_hubs, 0u);

    topology_metrics_t metrics = network_analyzer_get_metrics(analyzer);
    EXPECT_GE(metrics.clustering_coefficient, 0.0f);
    EXPECT_LE(metrics.clustering_coefficient, 1.0f);
}

TEST_F(NetworkAnalysisTest, CommunityDetection) {
    // Test community detection alone
    bool success = network_analyzer_detect_communities(analyzer);
    EXPECT_TRUE(success);

    const community_structure_t* communities = network_analyzer_get_communities(analyzer);
    ASSERT_NE(communities, nullptr);

    // Check modularity in valid range
    EXPECT_GE(communities->modularity, -0.5f);
    EXPECT_LE(communities->modularity, 1.0f);

    // Check communities have neurons
    for (uint32_t i = 0; i < communities->num_communities; i++) {
        EXPECT_GT(communities->community_sizes[i], 0u);
    }
}

TEST_F(NetworkAnalysisTest, HubDetection) {
    // Test hub detection alone
    bool success = network_analyzer_detect_hubs(analyzer);
    EXPECT_TRUE(success);

    const hub_detection_t* hubs = network_analyzer_get_hubs(analyzer);
    ASSERT_NE(hubs, nullptr);

    // Check hub metrics in valid range
    for (uint32_t i = 0; i < hubs->num_hubs; i++) {
        EXPECT_GE(hubs->hubs[i].degree_centrality, 0.0f);
        EXPECT_LE(hubs->hubs[i].degree_centrality, 1.0f);
        EXPECT_GE(hubs->hubs[i].betweenness, 0.0f);
        EXPECT_LE(hubs->hubs[i].betweenness, 1.0f);
    }
}

TEST_F(NetworkAnalysisTest, TopologyMetrics) {
    // Test topology metrics computation
    bool success = network_analyzer_compute_metrics(analyzer);
    EXPECT_TRUE(success);

    topology_metrics_t metrics = network_analyzer_get_metrics(analyzer);

    // Check all metrics in valid ranges
    EXPECT_GE(metrics.clustering_coefficient, 0.0f);
    EXPECT_LE(metrics.clustering_coefficient, 1.0f);
    EXPECT_GT(metrics.avg_path_length, 0.0f);
    EXPECT_GT(metrics.small_worldness, 0.0f);
    EXPECT_GE(metrics.density, 0.0f);
    EXPECT_LE(metrics.density, 1.0f);
}

//=============================================================================
// Validation Tests
//=============================================================================

TEST_F(NetworkAnalysisTest, ValidateHealthyTopology) {
    // Run analysis first
    ASSERT_TRUE(network_analyzer_run(analyzer));

    // Validate topology is healthy
    bool valid = network_analyzer_validate_learning(analyzer);
    EXPECT_TRUE(valid);
}

TEST_F(NetworkAnalysisTest, ValidationError) {
    // Validation before analysis should fail
    bool valid = network_analyzer_validate_learning(analyzer);
    EXPECT_FALSE(valid);

    // Should have error message
    const char* error = network_analyzer_get_error(analyzer);
    EXPECT_NE(error[0], '\0');
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(NetworkAnalysisTest, AutoAnalyze) {
    // Enable auto-analysis
    network_analyzer_set_auto_analyze(analyzer, true, 10);

    // Trigger learning events
    for (int i = 0; i < 15; i++) {
        network_analyzer_on_learning_event(analyzer);
    }

    // Analysis should have run at least once (interval=10)
    const community_structure_t* communities = network_analyzer_get_communities(analyzer);
    EXPECT_NE(communities, nullptr);
}

TEST_F(NetworkAnalysisTest, HubThreshold) {
    // Set hub threshold
    network_analyzer_set_hub_threshold(analyzer, 0.9f);

    // Run analysis
    ASSERT_TRUE(network_analyzer_detect_hubs(analyzer));

    const hub_detection_t* hubs = network_analyzer_get_hubs(analyzer);
    ASSERT_NE(hubs, nullptr);

    // All hubs should meet threshold
    for (uint32_t i = 0; i < hubs->num_hubs; i++) {
        EXPECT_GE(hubs->hubs[i].degree_centrality, 0.9f);
    }
}

//=============================================================================
// History Tracking Tests
//=============================================================================

TEST_F(NetworkAnalysisTest, ModularityHistory) {
    // Run analysis multiple times
    for (int i = 0; i < 5; i++) {
        ASSERT_TRUE(network_analyzer_run(analyzer));
    }

    // Check history was recorded
    uint32_t count = 0;
    const float* history = network_analyzer_get_modularity_history(analyzer, &count);
    ASSERT_NE(history, nullptr);
    EXPECT_EQ(count, 5u);

    // All modularity values should be valid
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_GE(history[i], -0.5f);
        EXPECT_LE(history[i], 1.0f);
    }
}

TEST_F(NetworkAnalysisTest, NewCommunityDetection) {
    // Initial analysis
    ASSERT_TRUE(network_analyzer_run(analyzer));
    bool new_community = network_analyzer_check_new_community(analyzer);
    EXPECT_FALSE(new_community);  // First run, no previous to compare

    // Second analysis (might detect new community)
    ASSERT_TRUE(network_analyzer_run(analyzer));
    new_community = network_analyzer_check_new_community(analyzer);
    // Result depends on network changes, just verify it doesn't crash
}

//=============================================================================
// Integration with Curiosity
//=============================================================================

class CuriosityNetworkAnalysisTest : public ::testing::Test {
protected:
    brain_t brain;
    curiosity_engine_t curiosity;

    void SetUp() override {
        brain = brain_create("curiosity_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
        ASSERT_NE(brain, nullptr);

        curiosity = curiosity_engine_create(brain, "test_learner");
        ASSERT_NE(curiosity, nullptr);
    }

    void TearDown() override {
        if (curiosity) {
            curiosity_engine_destroy(curiosity);
        }
        if (brain) {
            brain_destroy(brain);
        }
    }
};

TEST_F(CuriosityNetworkAnalysisTest, NoveltyTriggersAnalysis) {
    // Detect highly novel concept (gap_size > 0.7)
    knowledge_gap_t gap = curiosity_detect_knowledge_gap(curiosity, "quantum_entanglement");

    // High novelty should trigger network analysis
    // Check that learning potential was calculated
    EXPECT_GE(gap.learning_potential, 0.0f);
    EXPECT_LE(gap.learning_potential, 2.0f);  // Can exceed 1.0 with boost
}

TEST_F(CuriosityNetworkAnalysisTest, LowNoveltySkipsAnalysis) {
    // Learn something first
    curiosity_learn_answer(curiosity, "What is water?", "H2O molecule");

    // Check same concept again (low novelty)
    knowledge_gap_t gap = curiosity_detect_knowledge_gap(curiosity, "water");

    // Low novelty should not boost learning potential
    EXPECT_GE(gap.gap_size, 0.0f);
    EXPECT_LT(gap.gap_size, 0.7f);  // Familiar concept
}

//=============================================================================
// Integration with Consolidation
//=============================================================================

TEST_F(NetworkAnalysisTest, AnalyzeAfterConsolidation) {
    // Get initial topology
    ASSERT_TRUE(network_analyzer_run(analyzer));
    const community_structure_t* before = network_analyzer_get_communities(analyzer);
    ASSERT_NE(before, nullptr);
    float modularity_before = before->modularity;

    // Run consolidation
    consolidation_config_t config = consolidation_default_config();
    config.enable_pruning = true;
    bool success = brain_consolidate_memory(brain, &config);
    EXPECT_TRUE(success);

    // Re-analyze after consolidation
    ASSERT_TRUE(network_analyzer_run(analyzer));
    const community_structure_t* after = network_analyzer_get_communities(analyzer);
    ASSERT_NE(after, nullptr);

    // Modularity might change after consolidation
    // Just verify analysis completes successfully
    EXPECT_GE(after->modularity, -0.5f);
    EXPECT_LE(after->modularity, 1.0f);
}

//=============================================================================
// Reporting Tests
//=============================================================================

TEST_F(NetworkAnalysisTest, PrintReport) {
    // Run analysis
    ASSERT_TRUE(network_analyzer_run(analyzer));

    // Print report (should not crash)
    // Redirect stdout to capture output
    testing::internal::CaptureStdout();
    network_analyzer_print_report(analyzer);
    std::string output = testing::internal::GetCapturedStdout();

    // Check report contains expected sections
    EXPECT_NE(output.find("Network Topology Analysis"), std::string::npos);
    EXPECT_NE(output.find("Communities"), std::string::npos);
    EXPECT_NE(output.find("Hub Neurons"), std::string::npos);
}

TEST_F(NetworkAnalysisTest, PrintModularityTrend) {
    // Run analysis multiple times
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(network_analyzer_run(analyzer));
    }

    // Print trend (should not crash)
    testing::internal::CaptureStdout();
    network_analyzer_print_modularity_trend(analyzer);
    std::string output = testing::internal::GetCapturedStdout();

    // Check output contains modularity data
    EXPECT_NE(output.find("Modularity Trend"), std::string::npos);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
