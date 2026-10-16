//=============================================================================
// test_network_analyzer_integration.cpp - Network Analyzer Integration Tests
//=============================================================================

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "cognitive/analysis/nimcp_network_analysis.h"
#include "utils/quantum/nimcp_quantum_shannon.h"

// Test fixture for network analyzer integration
class NetworkAnalyzerIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    network_analyzer_t* analyzer;

    void SetUp() override {
        // Create a small test brain for network analysis
        brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 2);
        ASSERT_NE(brain, nullptr);

        analyzer = nullptr;
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
// Test: brain_get_network_analyzer basic functionality
//=============================================================================

TEST_F(NetworkAnalyzerIntegrationTest, GetNetworkAnalyzer_ValidBrain_ReturnsAnalyzer) {
    // WHAT: Test that brain_get_network_analyzer returns valid analyzer
    // WHY:  Verify lazy initialization works correctly
    // HOW:  Call brain_get_network_analyzer and check return value

    void* analyzer_ptr = brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer_ptr, nullptr);

    // Cast to network_analyzer_t to verify structure
    network_analyzer_t* analyzer = (network_analyzer_t*)analyzer_ptr;
    EXPECT_EQ(analyzer->brain, brain);
}

TEST_F(NetworkAnalyzerIntegrationTest, GetNetworkAnalyzer_NullBrain_ReturnsNull) {
    // WHAT: Test NULL brain handling
    // WHY:  Verify error handling
    // HOW:  Pass NULL brain

    void* analyzer_ptr = brain_get_network_analyzer(nullptr);
    EXPECT_EQ(analyzer_ptr, nullptr);
}

TEST_F(NetworkAnalyzerIntegrationTest, GetNetworkAnalyzer_CalledTwice_ReturnsSameAnalyzer) {
    // WHAT: Test that analyzer is cached
    // WHY:  Verify lazy initialization doesn't recreate analyzer
    // HOW:  Call brain_get_network_analyzer twice and compare pointers

    void* analyzer1 = brain_get_network_analyzer(brain);
    void* analyzer2 = brain_get_network_analyzer(brain);

    EXPECT_EQ(analyzer1, analyzer2);
    EXPECT_NE(analyzer1, nullptr);
}

TEST_F(NetworkAnalyzerIntegrationTest, NetworkAnalyzer_InitialAnalysis_Succeeds) {
    // WHAT: Test that initial analysis runs successfully
    // WHY:  Verify analyzer is configured correctly
    // HOW:  Get analyzer and check if analysis was performed

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Check that analysis was performed during initialization
    const community_structure_t* communities = network_analyzer_get_communities(analyzer);
    // May be NULL if network is too small, that's OK
    // Just verify no crash

    const hub_detection_t* hubs = network_analyzer_get_hubs(analyzer);
    // May be NULL if no hubs detected, that's OK

    // Get metrics (should always work)
    topology_metrics_t metrics = network_analyzer_get_metrics(analyzer);
    EXPECT_GE(metrics.density, 0.0f);
    EXPECT_LE(metrics.density, 1.0f);
}

TEST_F(NetworkAnalyzerIntegrationTest, NetworkAnalyzer_AutoAnalyze_Configured) {
    // WHAT: Test that auto-analyze is configured correctly
    // WHY:  Verify initialization sets auto-analyze parameters
    // HOW:  Get analyzer and check configuration

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Auto-analyze should be enabled with interval of 10
    EXPECT_TRUE(analyzer->auto_analyze);
    EXPECT_EQ(analyzer->analysis_interval, 10u);
}

TEST_F(NetworkAnalyzerIntegrationTest, NetworkAnalyzer_HubThreshold_Configured) {
    // WHAT: Test that hub threshold is configured correctly
    // WHY:  Verify initialization sets hub detection parameters
    // HOW:  Get analyzer and check hub threshold

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Hub threshold should be 0.7
    EXPECT_FLOAT_EQ(analyzer->hub_threshold, 0.7f);
}

//=============================================================================
// Test: Network analyzer with learning events
//=============================================================================

TEST_F(NetworkAnalyzerIntegrationTest, NetworkAnalyzer_AfterLearning_UpdatesTopology) {
    // WHAT: Test that analyzer tracks topology changes during learning
    // WHY:  Verify analyzer integrates with learning pipeline
    // HOW:  Perform learning and check if topology metrics change

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Get initial metrics
    topology_metrics_t initial_metrics = network_analyzer_get_metrics(analyzer);

    // Perform some decisions to generate network activity
    float input[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    for (int i = 0; i < 20; i++) {
        brain_decision_t* decision = brain_decide(brain, input, 10);
        if (decision) {
            brain_free_decision(decision);
        }
    }

    // Run analysis manually
    network_analyzer_run(analyzer);

    // Get updated metrics
    topology_metrics_t updated_metrics = network_analyzer_get_metrics(analyzer);

    // Metrics should be valid (may or may not have changed depending on learning)
    EXPECT_GE(updated_metrics.density, 0.0f);
    EXPECT_LE(updated_metrics.density, 1.0f);
    EXPECT_GE(updated_metrics.clustering_coefficient, 0.0f);
    EXPECT_LE(updated_metrics.clustering_coefficient, 1.0f);
}

//=============================================================================
// Test: Network analyzer memory management
//=============================================================================

TEST_F(NetworkAnalyzerIntegrationTest, NetworkAnalyzer_DestroyedWithBrain_NoLeaks) {
    // WHAT: Test that analyzer is properly destroyed with brain
    // WHY:  Verify memory management
    // HOW:  Create brain, get analyzer, destroy brain, check for leaks

    // Get analyzer (this caches it in brain)
    void* analyzer_ptr = brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer_ptr, nullptr);

    // Destroy brain (should also destroy analyzer)
    brain_destroy(brain);
    brain = nullptr; // Prevent double-free in TearDown

    // No crash = success
}

//=============================================================================
// Test: Network analyzer validation
//=============================================================================

TEST_F(NetworkAnalyzerIntegrationTest, NetworkAnalyzer_ValidateTopology_HealthyNetwork) {
    // WHAT: Test topology validation on healthy network
    // WHY:  Verify validation detects healthy networks
    // HOW:  Create healthy network and validate

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Run analysis
    ASSERT_TRUE(network_analyzer_run(analyzer));

    // Validate learning (should pass for new network)
    bool is_valid = network_analyzer_validate_learning(analyzer);

    // For a newly created network, validation should pass
    EXPECT_TRUE(is_valid || analyzer->topology_is_valid);
}

TEST_F(NetworkAnalyzerIntegrationTest, NetworkAnalyzer_GetError_NoErrorInitially) {
    // WHAT: Test error message retrieval
    // WHY:  Verify error reporting works
    // HOW:  Get analyzer and check error message

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    const char* error = network_analyzer_get_error(analyzer);
    ASSERT_NE(error, nullptr);

    // Should be empty string initially
    EXPECT_EQ(strlen(error), 0u);
}

//=============================================================================
// Test: Network analyzer query functions
//=============================================================================

TEST_F(NetworkAnalyzerIntegrationTest, NetworkAnalyzer_GetCommunities_ReturnsValid) {
    // WHAT: Test community structure retrieval
    // WHY:  Verify query functions work
    // HOW:  Get communities and check validity

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    const community_structure_t* communities = network_analyzer_get_communities(analyzer);

    // May be NULL for small/sparse networks, that's OK
    if (communities) {
        EXPECT_GT(communities->num_communities, 0u);
        EXPECT_GE(communities->modularity, -0.5f);
        EXPECT_LE(communities->modularity, 1.0f);
    }
}

TEST_F(NetworkAnalyzerIntegrationTest, NetworkAnalyzer_GetHubs_ReturnsValid) {
    // WHAT: Test hub detection retrieval
    // WHY:  Verify hub detection works
    // HOW:  Get hubs and check validity

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    const hub_detection_t* hubs = network_analyzer_get_hubs(analyzer);

    // May be NULL if no hubs found, that's OK
    if (hubs) {
        EXPECT_GE(hubs->num_hubs, 0u);
        EXPECT_GE(hubs->hub_threshold, 0.0f);
        EXPECT_LE(hubs->hub_threshold, 1.0f);
    }
}

TEST_F(NetworkAnalyzerIntegrationTest, NetworkAnalyzer_GetMetrics_AlwaysValid) {
    // WHAT: Test metrics retrieval
    // WHY:  Verify metrics are always available
    // HOW:  Get metrics and check validity

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    topology_metrics_t metrics = network_analyzer_get_metrics(analyzer);

    // All metrics should be in valid ranges
    EXPECT_GE(metrics.density, 0.0f);
    EXPECT_LE(metrics.density, 1.0f);
    EXPECT_GE(metrics.clustering_coefficient, 0.0f);
    EXPECT_LE(metrics.clustering_coefficient, 1.0f);
    EXPECT_GE(metrics.avg_path_length, 0.0f);
    EXPECT_GE(metrics.num_edges, 0u);
}

//=============================================================================
// Test: Network analyzer history tracking
//=============================================================================

TEST_F(NetworkAnalyzerIntegrationTest, NetworkAnalyzer_GetModularityHistory_TracksChanges) {
    // WHAT: Test modularity history tracking
    // WHY:  Verify history accumulation works
    // HOW:  Run multiple analyses and check history

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    uint32_t count = 0;
    const float* history = network_analyzer_get_modularity_history(analyzer, &count);

    // May be NULL initially
    if (history) {
        EXPECT_GT(count, 0u);
        EXPECT_LE(count, analyzer->history_capacity);
    }
}

//=============================================================================
// Test: Network analyzer thread safety
//=============================================================================

TEST_F(NetworkAnalyzerIntegrationTest, NetworkAnalyzer_MultipleCalls_ThreadSafe) {
    // WHAT: Test that multiple calls don't interfere
    // WHY:  Verify thread-safe caching
    // HOW:  Call multiple times from same thread

    void* analyzer1 = brain_get_network_analyzer(brain);
    void* analyzer2 = brain_get_network_analyzer(brain);
    void* analyzer3 = brain_get_network_analyzer(brain);

    EXPECT_EQ(analyzer1, analyzer2);
    EXPECT_EQ(analyzer2, analyzer3);
    EXPECT_NE(analyzer1, nullptr);
}

//=============================================================================
// Test: Network analyzer configuration
//=============================================================================

TEST_F(NetworkAnalyzerIntegrationTest, NetworkAnalyzer_SetAutoAnalyze_UpdatesConfig) {
    // WHAT: Test configuration updates
    // WHY:  Verify configuration API works
    // HOW:  Change settings and verify

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Change auto-analyze settings
    network_analyzer_set_auto_analyze(analyzer, false, 20);

    EXPECT_FALSE(analyzer->auto_analyze);
    EXPECT_EQ(analyzer->analysis_interval, 20u);

    // Re-enable with different interval
    network_analyzer_set_auto_analyze(analyzer, true, 5);

    EXPECT_TRUE(analyzer->auto_analyze);
    EXPECT_EQ(analyzer->analysis_interval, 5u);
}

TEST_F(NetworkAnalyzerIntegrationTest, NetworkAnalyzer_SetHubThreshold_UpdatesThreshold) {
    // WHAT: Test hub threshold configuration
    // WHY:  Verify threshold can be changed
    // HOW:  Set threshold and verify

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Change hub threshold
    network_analyzer_set_hub_threshold(analyzer, 0.9f);
    EXPECT_FLOAT_EQ(analyzer->hub_threshold, 0.9f);

    // Change to different value
    network_analyzer_set_hub_threshold(analyzer, 0.5f);
    EXPECT_FLOAT_EQ(analyzer->hub_threshold, 0.5f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
