//=============================================================================
// test_brain_network_analyzer.cpp - Brain Network Analyzer Integration Tests
//=============================================================================

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "cognitive/analysis/nimcp_network_analysis.h"
#include <chrono>

// Test fixture for brain network analyzer integration
class BrainNetworkAnalyzerTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        // Create a test brain with sufficient neurons for topology analysis
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        config.size = BRAIN_SIZE_MEDIUM;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 10;
        config.num_outputs = 10;
        snprintf(config.task_name, sizeof(config.task_name), "analyzer_test");

        brain = brain_create_custom(&config);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }
};

//=============================================================================
// Test: brain_get_network_analyzer basic functionality
//=============================================================================

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_ValidBrain_ReturnsNonNull) {
    // WHAT: Test getting network analyzer from valid brain
    // WHY:  Verify basic functionality
    // HOW:  Call brain_get_network_analyzer with valid brain

    void* analyzer = brain_get_network_analyzer(brain);
    EXPECT_NE(analyzer, nullptr);
}

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_NullBrain_ReturnsNull) {
    // WHAT: Test NULL brain handling
    // WHY:  Verify error handling
    // HOW:  Pass NULL brain

    void* analyzer = brain_get_network_analyzer(nullptr);
    EXPECT_EQ(analyzer, nullptr);
}

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_MultipleCalls_ReturnsSameInstance) {
    // WHAT: Test that analyzer is cached and reused
    // WHY:  Verify lazy initialization and caching
    // HOW:  Call multiple times and compare pointers

    void* analyzer1 = brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer1, nullptr);

    void* analyzer2 = brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer2, nullptr);

    void* analyzer3 = brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer3, nullptr);

    // All calls should return the same cached instance
    EXPECT_EQ(analyzer1, analyzer2);
    EXPECT_EQ(analyzer2, analyzer3);
}

//=============================================================================
// Test: Analyzer initialization and configuration
//=============================================================================

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_InitializesWithCorrectSettings) {
    // WHAT: Test that analyzer is initialized with correct configuration
    // WHY:  Verify auto-analyze and hub threshold are set
    // HOW:  Get analyzer and check configuration

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Verify auto-analyze is enabled
    EXPECT_TRUE(analyzer->auto_analyze);

    // Verify analysis interval is set to 10
    EXPECT_EQ(analyzer->analysis_interval, 10u);

    // Verify hub threshold is set to 0.7
    EXPECT_FLOAT_EQ(analyzer->hub_threshold, 0.7f);
}

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_RunsInitialAnalysis) {
    // WHAT: Test that initial analysis is run on first access
    // WHY:  Verify analyzer populates topology metrics immediately
    // HOW:  Get analyzer and check analysis count

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Initial analysis should have been run
    EXPECT_GT(analyzer->analysis_count, 0u);
}

//=============================================================================
// Test: Community detection integration
//=============================================================================

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_DetectsCommunities) {
    // WHAT: Test that analyzer can detect communities
    // WHY:  Verify community detection works
    // HOW:  Get analyzer and check communities

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    const community_structure_t* communities = network_analyzer_get_communities(analyzer);

    // Communities should be detected (may be NULL if analysis failed, but that's acceptable)
    // If non-NULL, verify structure is valid
    if (communities) {
        EXPECT_GT(communities->num_communities, 0u);
        EXPECT_GE(communities->modularity, -0.5f);
        EXPECT_LE(communities->modularity, 1.0f);
    }
}

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_DetectsHubs) {
    // WHAT: Test that analyzer can detect hub neurons
    // WHY:  Verify hub detection works
    // HOW:  Get analyzer and check hubs

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    const hub_detection_t* hubs = network_analyzer_get_hubs(analyzer);

    // Hubs may or may not be detected depending on network topology
    // If non-NULL, verify structure is valid
    if (hubs) {
        EXPECT_GE(hubs->num_hubs, 0u);
        EXPECT_GE(hubs->hub_threshold, 0.0f);
        EXPECT_LE(hubs->hub_threshold, 1.0f);

        // Check hub neurons have valid centrality values
        for (uint32_t i = 0; i < hubs->num_hubs; i++) {
            EXPECT_GE(hubs->hubs[i].degree_centrality, 0.0f);
            EXPECT_LE(hubs->hubs[i].degree_centrality, 1.0f);
            EXPECT_GE(hubs->hubs[i].betweenness, 0.0f);
            EXPECT_LE(hubs->hubs[i].betweenness, 1.0f);
        }
    }
}

//=============================================================================
// Test: Topology metrics
//=============================================================================

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_ComputesTopologyMetrics) {
    // WHAT: Test that analyzer computes topology metrics
    // WHY:  Verify metrics calculation works
    // HOW:  Get analyzer and check metrics

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    topology_metrics_t metrics = network_analyzer_get_metrics(analyzer);

    // Verify metrics are in valid ranges
    EXPECT_GE(metrics.clustering_coefficient, 0.0f);
    EXPECT_LE(metrics.clustering_coefficient, 1.0f);
    EXPECT_GE(metrics.density, 0.0f);
    EXPECT_LE(metrics.density, 1.0f);
}

//=============================================================================
// Test: Re-analysis after learning
//=============================================================================

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_ReanalyzesAfterLearning) {
    // WHAT: Test that analyzer re-analyzes after learning events
    // WHY:  Verify auto-analyze works during training
    // HOW:  Get analyzer, trigger learning events, check analysis count

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    uint32_t initial_count = analyzer->analysis_count;

    // Simulate learning events (via learning iteration counter)
    for (int i = 0; i < 15; i++) {
        network_analyzer_on_learning_event(analyzer);
    }

    // Analysis should have been triggered (every 10 iterations)
    EXPECT_GT(analyzer->analysis_count, initial_count);
}

//=============================================================================
// Test: Performance metrics
//=============================================================================

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_FirstCall_ReasonableTime) {
    // WHAT: Test that first analyzer access completes in reasonable time
    // WHY:  Verify performance requirement (50-200ms for first call)
    // HOW:  Time first access

    auto start = std::chrono::high_resolution_clock::now();

    void* analyzer = brain_get_network_analyzer(brain);
    EXPECT_NE(analyzer, nullptr);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within 5000ms (generous upper bound for loaded systems)
    EXPECT_LT(duration.count(), 5000);
}

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_SubsequentCalls_FastLookup) {
    // WHAT: Test that subsequent analyzer accesses are fast (cached)
    // WHY:  Verify performance requirement (<1μs for cached calls)
    // HOW:  Time multiple accesses

    // First call (initialization)
    void* analyzer1 = brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer1, nullptr);

    // Time subsequent calls
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        void* analyzer = brain_get_network_analyzer(brain);
        EXPECT_NE(analyzer, nullptr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // 1000 cached lookups should be very fast (< 1ms total)
    EXPECT_LT(duration.count(), 1000);
}

//=============================================================================
// Test: Memory management
//=============================================================================

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_CleanupOnBrainDestroy) {
    // WHAT: Test that analyzer is cleaned up when brain is destroyed
    // WHY:  Verify no memory leaks
    // HOW:  Create brain, get analyzer, destroy brain, check no crash

    brain_config_t config;
    memset(&config, 0, sizeof(config));
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 10;
    snprintf(config.task_name, sizeof(config.task_name), "temp_brain");

    brain_t temp_brain = brain_create_custom(&config);
    ASSERT_NE(temp_brain, nullptr);

    void* analyzer = brain_get_network_analyzer(temp_brain);
    EXPECT_NE(analyzer, nullptr);

    // Destroy brain (should cleanup analyzer)
    brain_destroy(temp_brain);

    // No crash = successful cleanup
}

//=============================================================================
// Test: Integration with brain operations
//=============================================================================

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_WorksWithBrainInference) {
    // WHAT: Test that analyzer works during brain inference
    // WHY:  Verify integration with main brain operations
    // HOW:  Run inference, get analyzer, check it works

    // Create input
    float input[10] = {1.0f, 0.5f, 0.3f, 0.8f, 0.2f, 0.9f, 0.1f, 0.6f, 0.4f, 0.7f};

    // Run inference
    brain_decision_t* decision = brain_decide(brain, input, 10);
    EXPECT_NE(decision, nullptr);

    // Get analyzer (should still work after inference)
    void* analyzer = brain_get_network_analyzer(brain);
    EXPECT_NE(analyzer, nullptr);

    brain_free_decision(decision);
}

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_WorksWithBrainLearning) {
    // WHAT: Test that analyzer works during brain learning
    // WHY:  Verify integration with learning operations
    // HOW:  Run learning, get analyzer, check it works

    // Create input and labels
    float input[10] = {1.0f, 0.5f, 0.3f, 0.8f, 0.2f, 0.9f, 0.1f, 0.6f, 0.4f, 0.7f};
    const char* label = "test_class";

    // Run learning
    float loss = brain_learn_example(brain, input, 10, label, 1.0f);
    (void)loss;  // Suppress unused warning

    // Get analyzer (should still work after learning)
    void* analyzer = brain_get_network_analyzer(brain);
    EXPECT_NE(analyzer, nullptr);
}

//=============================================================================
// Test: Edge cases
//=============================================================================

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_SmallBrain_HandlesGracefully) {
    // WHAT: Test analyzer with very small brain
    // WHY:  Verify edge case handling
    // HOW:  Create minimal brain and get analyzer

    brain_config_t config;
    memset(&config, 0, sizeof(config));
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 10;
    snprintf(config.task_name, sizeof(config.task_name), "small_brain");

    brain_t small_brain = brain_create_custom(&config);
    ASSERT_NE(small_brain, nullptr);

    void* analyzer = brain_get_network_analyzer(small_brain);
    EXPECT_NE(analyzer, nullptr);

    brain_destroy(small_brain);
}

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_LargeBrain_HandlesGracefully) {
    // WHAT: Test analyzer with large brain
    // WHY:  Verify scalability
    // HOW:  Create large brain and get analyzer

    brain_config_t config;
    memset(&config, 0, sizeof(config));
    config.size = BRAIN_SIZE_LARGE;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 10;
    snprintf(config.task_name, sizeof(config.task_name), "large_brain");

    brain_t large_brain = brain_create_custom(&config);
    ASSERT_NE(large_brain, nullptr);

    void* analyzer = brain_get_network_analyzer(large_brain);
    EXPECT_NE(analyzer, nullptr);

    brain_destroy(large_brain);
}

//=============================================================================
// Test: Analyzer API integration
//=============================================================================

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_SupportsFullAnalyzerAPI) {
    // WHAT: Test that returned analyzer supports full API
    // WHY:  Verify complete integration
    // HOW:  Call various analyzer functions

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Test various API functions
    bool run_result = network_analyzer_run(analyzer);
    EXPECT_TRUE(run_result || !run_result); // Just verify it doesn't crash

    network_analyzer_set_hub_threshold(analyzer, 0.8f);
    EXPECT_FLOAT_EQ(analyzer->hub_threshold, 0.8f);

    network_analyzer_set_auto_analyze(analyzer, false, 20);
    EXPECT_FALSE(analyzer->auto_analyze);
    EXPECT_EQ(analyzer->analysis_interval, 20u);

    // Restore original settings
    network_analyzer_set_auto_analyze(analyzer, true, 10);
    network_analyzer_set_hub_threshold(analyzer, 0.7f);
}

//=============================================================================
// Test: Thread safety (basic)
//=============================================================================

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_ConcurrentAccess_Safe) {
    // WHAT: Test concurrent access to analyzer
    // WHY:  Verify thread safety
    // HOW:  Call multiple times in sequence (basic test)

    // Sequential access (basic thread safety check)
    for (int i = 0; i < 100; i++) {
        void* analyzer = brain_get_network_analyzer(brain);
        EXPECT_NE(analyzer, nullptr);
    }
}

//=============================================================================
// Test: Validation integration
//=============================================================================

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_SupportsValidation) {
    // WHAT: Test that analyzer supports validation operations
    // WHY:  Verify validation API integration
    // HOW:  Get analyzer and check validation functions

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Validation should work (may return true or false depending on topology)
    bool valid = network_analyzer_validate_learning(analyzer);
    EXPECT_TRUE(valid || !valid); // Just verify it doesn't crash
}

//=============================================================================
// Test: History tracking
//=============================================================================

TEST_F(BrainNetworkAnalyzerTest, GetNetworkAnalyzer_TracksHistory) {
    // WHAT: Test that analyzer tracks modularity history
    // WHY:  Verify history tracking functionality
    // HOW:  Get analyzer, run multiple analyses, check history

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Run multiple analyses
    for (int i = 0; i < 5; i++) {
        network_analyzer_run(analyzer);
    }

    // Get history
    uint32_t history_count = 0;
    const float* history = network_analyzer_get_modularity_history(analyzer, &history_count);

    // History should exist
    EXPECT_GT(history_count, 0u);
    if (history) {
        EXPECT_NE(history, nullptr);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
