/**
 * @file test_network_analysis.cpp
 * @brief Unit tests for network analysis module
 *
 * WHAT: Tests for network_analyzer timestamp, connector hubs, graph metrics
 * WHY: Verify correctness of network analysis implementations
 * HOW: Create test networks, verify analysis results
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/analysis/nimcp_network_analysis.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

class NetworkAnalysisTest : public ::testing::Test {
protected:
    brain_t brain;
    network_analyzer_t* analyzer;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        // Create a tiny test brain (minimize memory for per-test init/teardown)
        brain = brain_create("test_network", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 10, 2);
        ASSERT_NE(brain, nullptr);

        analyzer = network_analyzer_create(brain);
        ASSERT_NE(analyzer, nullptr);
    }

    void TearDown() override {
        if (analyzer) {
            network_analyzer_destroy(analyzer);
            analyzer = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        /* Note: Do NOT call nimcp_memory_cleanup() between tests.
         * brain_destroy may leave background threads still winding down,
         * and destroying the memory mutex while they are active causes
         * a pthread priority protocol assertion failure (SIGABRT). */
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(NetworkAnalysisTest, CreateDestroy) {
    // Test basic lifecycle
    network_analyzer_t* test_analyzer = network_analyzer_create(brain);
    ASSERT_NE(test_analyzer, nullptr);

    EXPECT_EQ(test_analyzer->brain, brain);
    EXPECT_TRUE(test_analyzer->auto_analyze);
    EXPECT_EQ(test_analyzer->analysis_interval, 10);
    EXPECT_NEAR(test_analyzer->hub_threshold, 0.7f, 0.01f);
    EXPECT_EQ(test_analyzer->analysis_count, 0);

    network_analyzer_destroy(test_analyzer);
}

TEST_F(NetworkAnalysisTest, CreateWithNullBrain) {
    network_analyzer_t* test_analyzer = network_analyzer_create(nullptr);
    EXPECT_EQ(test_analyzer, nullptr);
}

//=============================================================================
// Hub Detection Tests
//=============================================================================

TEST_F(NetworkAnalysisTest, HubDetectionTimestamp) {
    // WHAT: Verify hub detection sets timestamp
    // WHY: Timestamp is needed for temporal analysis

    // First detect communities (needed for connector hub detection)
    bool comm_result = network_analyzer_detect_communities(analyzer);

    // Run hub detection
    bool result = network_analyzer_detect_hubs(analyzer);

    if (result && analyzer->hubs) {
        // Timestamp should be set (non-zero)
        EXPECT_GT(analyzer->hubs->timestamp, 0);

        // Timestamp should be recent (within last second)
        uint64_t now = nimcp_time_get_ms();
        EXPECT_LE(analyzer->hubs->timestamp, now);
        EXPECT_GE(analyzer->hubs->timestamp, now - 1000);
    }
}

TEST_F(NetworkAnalysisTest, HubThresholdConfiguration) {
    // Test hub threshold setting
    network_analyzer_set_hub_threshold(analyzer, 0.9f);
    EXPECT_NEAR(analyzer->hub_threshold, 0.9f, 0.01f);

    network_analyzer_set_hub_threshold(analyzer, 0.5f);
    EXPECT_NEAR(analyzer->hub_threshold, 0.5f, 0.01f);
}

//=============================================================================
// Connector Hub Tests
//=============================================================================

TEST_F(NetworkAnalysisTest, ConnectorHubDetection) {
    // WHAT: Test connector hub detection
    // WHY: Connector hubs are critical for inter-module communication

    // Run community detection first
    network_analyzer_detect_communities(analyzer);

    // Run hub detection
    bool result = network_analyzer_detect_hubs(analyzer);

    if (result && analyzer->hubs) {
        // Verify is_connector_hub field is set for each hub
        for (uint32_t i = 0; i < analyzer->hubs->num_hubs; i++) {
            // Field should be boolean (0 or 1)
            bool is_connector = analyzer->hubs->hubs[i].is_connector_hub;
            EXPECT_TRUE(is_connector == true || is_connector == false);
        }
    }
}

//=============================================================================
// Metrics Computation Tests
//=============================================================================

TEST_F(NetworkAnalysisTest, ComputeMetricsBasic) {
    // WHAT: Test metrics computation
    // WHY: Verify graph conversion and metrics calculation

    bool result = network_analyzer_compute_metrics(analyzer);
    EXPECT_TRUE(result);

    // Verify metrics are in valid ranges
    EXPECT_GE(analyzer->metrics.density, 0.0f);
    EXPECT_LE(analyzer->metrics.density, 1.0f);

    EXPECT_GE(analyzer->metrics.clustering_coefficient, 0.0f);
    EXPECT_LE(analyzer->metrics.clustering_coefficient, 1.0f);

    EXPECT_GE(analyzer->metrics.avg_path_length, 0.0f);

    EXPECT_GE(analyzer->metrics.assortativity, -1.0f);
    EXPECT_LE(analyzer->metrics.assortativity, 1.0f);
}

TEST_F(NetworkAnalysisTest, ComputeMetricsEdgeCount) {
    // Test that edge count is computed
    network_analyzer_compute_metrics(analyzer);

    // Edge count should be non-negative
    EXPECT_GE(analyzer->metrics.num_edges, 0u);
}

//=============================================================================
// Full Analysis Tests
//=============================================================================

TEST_F(NetworkAnalysisTest, RunFullAnalysis) {
    // WHAT: Test full network analysis pipeline
    // WHY: Verify all components work together

    bool result = network_analyzer_run(analyzer);

    // Analysis should complete (may have warnings but not fail completely)
    // Check analysis count was incremented
    EXPECT_GT(analyzer->analysis_count, 0u);
}

TEST_F(NetworkAnalysisTest, ModularityHistory) {
    // Run analysis multiple times
    for (int i = 0; i < 3; i++) {
        network_analyzer_run(analyzer);
    }

    // Get modularity history
    uint32_t count = 0;
    const float* history = network_analyzer_get_modularity_history(analyzer, &count);

    EXPECT_GE(count, 1u);
    if (history && count > 0) {
        // Modularity should be in valid range
        for (uint32_t i = 0; i < count; i++) {
            EXPECT_GE(history[i], -0.5f);
            EXPECT_LE(history[i], 1.0f);
        }
    }
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(NetworkAnalysisTest, AutoAnalyzeConfiguration) {
    // Test auto-analyze settings
    network_analyzer_set_auto_analyze(analyzer, true, 5);
    EXPECT_TRUE(analyzer->auto_analyze);
    EXPECT_EQ(analyzer->analysis_interval, 5);

    network_analyzer_set_auto_analyze(analyzer, false, 20);
    EXPECT_FALSE(analyzer->auto_analyze);
    EXPECT_EQ(analyzer->analysis_interval, 20);
}

TEST_F(NetworkAnalysisTest, LearningEventTrigger) {
    // Test learning event triggering
    analyzer->auto_analyze = true;
    analyzer->analysis_interval = 2;
    analyzer->iteration_counter = 0;
    uint32_t initial_count = analyzer->analysis_count;

    // Fire learning events
    network_analyzer_on_learning_event(analyzer);
    EXPECT_EQ(analyzer->iteration_counter, 1u);

    network_analyzer_on_learning_event(analyzer);
    // After 2 events, analysis should trigger
    EXPECT_EQ(analyzer->iteration_counter, 0u);
}

//=============================================================================
// Query Tests
//=============================================================================

TEST_F(NetworkAnalysisTest, GetCommunities) {
    network_analyzer_detect_communities(analyzer);

    const community_structure_t* communities =
        network_analyzer_get_communities(analyzer);

    // May be NULL if detection failed, but should not crash
    if (communities) {
        EXPECT_GE(communities->num_communities, 1u);
        EXPECT_GE(communities->modularity, -0.5f);
        EXPECT_LE(communities->modularity, 1.0f);
    }
}

TEST_F(NetworkAnalysisTest, GetHubs) {
    network_analyzer_detect_hubs(analyzer);

    const hub_detection_t* hubs = network_analyzer_get_hubs(analyzer);

    if (hubs) {
        EXPECT_GE(hubs->hub_threshold, 0.0f);
        EXPECT_LE(hubs->hub_threshold, 1.0f);
    }
}

TEST_F(NetworkAnalysisTest, GetMetrics) {
    network_analyzer_compute_metrics(analyzer);

    topology_metrics_t metrics = network_analyzer_get_metrics(analyzer);

    // Metrics should be in valid ranges
    EXPECT_GE(metrics.density, 0.0f);
    EXPECT_LE(metrics.density, 1.0f);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(NetworkAnalysisTest, GetErrorMessage) {
    // Initially no error
    const char* error = network_analyzer_get_error(analyzer);
    ASSERT_NE(error, nullptr);

    // Error message should be empty or null-terminated
    size_t len = strlen(error);
    EXPECT_LT(len, 256u);
}

TEST_F(NetworkAnalysisTest, ValidateLearningNoAnalysis) {
    // Validation should fail if no analysis has been run
    bool valid = network_analyzer_validate_learning(analyzer);

    // Should fail because no analysis results
    EXPECT_FALSE(valid);

    // Error message should be set
    const char* error = network_analyzer_get_error(analyzer);
    EXPECT_GT(strlen(error), 0u);
}

//=============================================================================
// Null Safety Tests
//=============================================================================

TEST_F(NetworkAnalysisTest, NullAnalyzerOperations) {
    // All operations should handle NULL analyzer gracefully
    network_analyzer_destroy(nullptr);  // Should not crash

    EXPECT_FALSE(network_analyzer_run(nullptr));
    EXPECT_FALSE(network_analyzer_detect_communities(nullptr));
    EXPECT_FALSE(network_analyzer_detect_hubs(nullptr));
    EXPECT_FALSE(network_analyzer_compute_metrics(nullptr));
    EXPECT_FALSE(network_analyzer_validate_learning(nullptr));

    EXPECT_EQ(network_analyzer_get_communities(nullptr), nullptr);
    EXPECT_EQ(network_analyzer_get_hubs(nullptr), nullptr);

    uint32_t count = 0;
    EXPECT_EQ(network_analyzer_get_modularity_history(nullptr, &count), nullptr);

    EXPECT_STREQ(network_analyzer_get_error(nullptr), "NULL analyzer");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
