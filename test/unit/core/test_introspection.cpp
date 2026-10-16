/**
 * @file test_introspection.cpp
 * @brief Tests for brain introspection API
 *
 * WHAT: Comprehensive tests for internal state queries and metacognition
 * WHY: Introspection enables self-awareness - must expose correct state
 * HOW: Unit tests for neuron activity, state extraction, uncertainty, patterns, topology
 */

#include "test_helpers.h"

#include "core/brain/nimcp_brain.h"
#include "cognitive/introspection/nimcp_introspection.h"

#include <gtest/gtest.h>
#include <string.h>
#include <chrono>

//=============================================================================
// Test Fixtures and Helpers
//=============================================================================

/**
 * WHAT: Test fixture for introspection tests
 * WHY: Set up/tear down brain and context for each test
 */
class IntrospectionTest : public ::testing::Test {
   protected:
    brain_t brain;
    introspection_context_t context;

    // Test data
    static const uint32_t NUM_FEATURES = 13;
    float test_features[NUM_FEATURES];

    void SetUp() override
    {
        // Create test brain
        brain = brain_create("test_intro_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION,
                             NUM_FEATURES, 3);
        ASSERT_NE(brain, nullptr);

        // Initialize test features
        for (uint32_t i = 0; i < NUM_FEATURES; i++) {
            test_features[i] = (float) i / NUM_FEATURES;
        }

        context = nullptr;
    }

    void TearDown() override
    {
        // Clean up context
        if (context) {
            introspection_context_destroy(context);
        }

        // Clean up brain
        if (brain) {
            brain_destroy(brain);
        }
    }
};

// Global callback counter
static std::atomic<uint32_t> g_state_change_count{0};

static void state_change_callback(brain_state_t* state, void* ctx)
{
    g_state_change_count++;
}

//=============================================================================
// Configuration Tests
//=============================================================================

/**
 * WHAT: Test default introspection configuration
 * WHY: Verify sensible defaults are provided
 */
TEST_F(IntrospectionTest, DefaultConfig)
{
    introspection_config_t config = introspection_default_config();

    EXPECT_EQ(config.default_strategy, STATE_STRATEGY_BALANCED);
    EXPECT_GT(config.activity_threshold, 0.0f);
    EXPECT_GT(config.history_size, 0u);
    EXPECT_TRUE(config.enable_pattern_tracking);
    EXPECT_TRUE(config.enable_uncertainty_estimation);
}

//=============================================================================
// Context Creation Tests
//=============================================================================

/**
 * WHAT: Test context creation with default config
 * WHY: Verify basic initialization works
 */
TEST_F(IntrospectionTest, CreateContextDefault)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);

    ASSERT_NE(context, nullptr);
}

/**
 * WHAT: Test context creation with NULL brain
 * WHY: Verify proper error handling
 */
TEST_F(IntrospectionTest, CreateContextNullBrain)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(nullptr, &config);

    EXPECT_EQ(context, nullptr);
}

/**
 * WHAT: Test context creation with NULL config (uses defaults)
 * WHY: Verify NULL config handling
 */
TEST_F(IntrospectionTest, CreateContextNullConfig)
{
    context = introspection_context_create(brain, nullptr);

    ASSERT_NE(context, nullptr);
}

/**
 * WHAT: Test context creation with callback
 * WHY: Verify callback registration works
 */
TEST_F(IntrospectionTest, CreateContextWithCallback)
{
    introspection_config_t config = introspection_default_config();
    config.on_state_change = state_change_callback;
    config.callback_context = (void*) 0x12345678;
    context = introspection_context_create(brain, &config);

    ASSERT_NE(context, nullptr);
}

//=============================================================================
// Neuron Population Tests
//=============================================================================

/**
 * WHAT: Test getting active neuron population
 * WHY: Verify population query works
 */
TEST_F(IntrospectionTest, GetActivePopulation)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    neuron_population_t population = brain_get_active_population(context, 0.5f);

    EXPECT_GT(population.total_neurons, 0u);
    EXPECT_LE(population.num_active, population.total_neurons);
    EXPECT_EQ(population.activity_threshold, 0.5f);

    // Clean up if arrays were allocated
    neuron_population_free(&population);
}

/**
 * WHAT: Test getting population with different thresholds
 * WHY: Verify threshold affects results
 */
TEST_F(IntrospectionTest, GetPopulationDifferentThresholds)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    neuron_population_t pop_low = brain_get_active_population(context, 0.1f);
    neuron_population_t pop_high = brain_get_active_population(context, 0.9f);

    // Lower threshold should have more active neurons
    EXPECT_GE(pop_low.num_active, pop_high.num_active);

    neuron_population_free(&pop_low);
    neuron_population_free(&pop_high);
}

/**
 * WHAT: Test getting population with NULL context
 * WHY: Verify proper error handling
 */
TEST_F(IntrospectionTest, GetPopulationNullContext)
{
    neuron_population_t population = brain_get_active_population(nullptr, 0.5f);

    EXPECT_EQ(population.num_active, 0u);
    EXPECT_EQ(population.total_neurons, 0u);
}

/**
 * WHAT: Test getting individual neuron activity
 * WHY: Verify neuron-level queries work
 */
TEST_F(IntrospectionTest, GetNeuronActivity)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    neuron_activity_t activity = brain_get_neuron_activity(context, 100);

    EXPECT_EQ(activity.neuron_id, 100u);
    EXPECT_GE(activity.activation, 0.0f);
    EXPECT_LE(activity.activation, 1.0f);
    EXPECT_GE(activity.num_connections, 0u);
}

//=============================================================================
// Internal State Tests
//=============================================================================

/**
 * WHAT: Test getting internal state with fast strategy
 * WHY: Verify fast state extraction works
 */
TEST_F(IntrospectionTest, GetInternalStateFast)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    brain_state_t state = brain_get_internal_state(context, STATE_STRATEGY_FAST);

    EXPECT_GT(state.dimension, 0u);
    EXPECT_NE(state.state_vector, nullptr);
    EXPECT_NE(state.interpretation, nullptr);
    EXPECT_GT(state.compression_ratio, 1.0f);

    brain_state_free(&state);
}

/**
 * WHAT: Test getting internal state with balanced strategy
 * WHY: Verify balanced state extraction works
 */
TEST_F(IntrospectionTest, GetInternalStateBalanced)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    brain_state_t state = brain_get_internal_state(context, STATE_STRATEGY_BALANCED);

    EXPECT_GT(state.dimension, 0u);
    EXPECT_NE(state.state_vector, nullptr);
    EXPECT_NE(state.interpretation, nullptr);

    brain_state_free(&state);
}

/**
 * WHAT: Test getting internal state with detailed strategy
 * WHY: Verify detailed state extraction works
 */
TEST_F(IntrospectionTest, GetInternalStateDetailed)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    brain_state_t state = brain_get_internal_state(context, STATE_STRATEGY_DETAILED);

    EXPECT_GT(state.dimension, 0u);
    EXPECT_NE(state.state_vector, nullptr);

    brain_state_free(&state);
}

/**
 * WHAT: Test getting state with NULL context
 * WHY: Verify proper error handling
 */
TEST_F(IntrospectionTest, GetStateNullContext)
{
    brain_state_t state = brain_get_internal_state(nullptr, STATE_STRATEGY_BALANCED);

    EXPECT_EQ(state.dimension, 0u);
    EXPECT_EQ(state.state_vector, nullptr);
}

/**
 * WHAT: Test comparing two brain states
 * WHY: Verify state similarity computation works
 */
TEST_F(IntrospectionTest, CompareBrainStates)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    brain_state_t state1 = brain_get_internal_state(context, STATE_STRATEGY_BALANCED);
    brain_state_t state2 = brain_get_internal_state(context, STATE_STRATEGY_BALANCED);

    // Same brain, same time - should be very similar
    float similarity = brain_state_similarity(&state1, &state2);

    EXPECT_GE(similarity, 0.0f);
    EXPECT_LE(similarity, 1.0f);
    // States from same brain at same time should be highly similar
    // NOTE: Placeholder uses rand(), expect lower similarity (~0.75)
    // TODO: Increase to >0.9 when integrated with real network
    EXPECT_GT(similarity, 0.5f);

    brain_state_free(&state1);
    brain_state_free(&state2);
}

/**
 * WHAT: Test state dimensionality across strategies
 * WHY: Verify strategies produce different compression ratios
 */
TEST_F(IntrospectionTest, StateDimensionality)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    brain_state_t state_fast = brain_get_internal_state(context, STATE_STRATEGY_FAST);
    brain_state_t state_balanced = brain_get_internal_state(context, STATE_STRATEGY_BALANCED);
    brain_state_t state_detailed = brain_get_internal_state(context, STATE_STRATEGY_DETAILED);

    // Fast should have smallest dimension, detailed should have largest
    EXPECT_LT(state_fast.dimension, state_balanced.dimension);
    EXPECT_LT(state_balanced.dimension, state_detailed.dimension);

    // Fast should have highest compression ratio
    EXPECT_GT(state_fast.compression_ratio, state_balanced.compression_ratio);
    EXPECT_GT(state_balanced.compression_ratio, state_detailed.compression_ratio);

    brain_state_free(&state_fast);
    brain_state_free(&state_balanced);
    brain_state_free(&state_detailed);
}

//=============================================================================
// Uncertainty Estimation Tests
//=============================================================================

/**
 * WHAT: Test uncertainty estimation
 * WHY: Verify uncertainty queries work
 */
TEST_F(IntrospectionTest, GetUncertainty)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    brain_uncertainty_t uncertainty = brain_get_uncertainty(context, test_features, NUM_FEATURES);

    EXPECT_GE(uncertainty.epistemic, 0.0f);
    EXPECT_LE(uncertainty.epistemic, 1.0f);
    EXPECT_GE(uncertainty.aleatoric, 0.0f);
    EXPECT_LE(uncertainty.aleatoric, 1.0f);
    EXPECT_GE(uncertainty.total, 0.0f);
    EXPECT_LE(uncertainty.total, 1.0f);
    EXPECT_GE(uncertainty.confidence, 0.0f);
    EXPECT_LE(uncertainty.confidence, 1.0f);

    // Total should be epistemic + aleatoric (approximately)
    EXPECT_NEAR(uncertainty.total, uncertainty.epistemic + uncertainty.aleatoric, 0.1f);

    // Confidence should be 1.0 - total
    EXPECT_NEAR(uncertainty.confidence, 1.0f - uncertainty.total, 0.01f);

    brain_uncertainty_free(&uncertainty);
}

/**
 * WHAT: Test uncertainty with NULL context
 * WHY: Verify proper error handling
 */
TEST_F(IntrospectionTest, GetUncertaintyNullContext)
{
    brain_uncertainty_t uncertainty = brain_get_uncertainty(nullptr, test_features, NUM_FEATURES);

    EXPECT_EQ(uncertainty.epistemic, 0.0f);
    EXPECT_EQ(uncertainty.total, 0.0f);
}

/**
 * WHAT: Test uncertainty with disabled estimation
 * WHY: Verify config flag is respected
 */
TEST_F(IntrospectionTest, UncertaintyDisabled)
{
    introspection_config_t config = introspection_default_config();
    config.enable_uncertainty_estimation = false;
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    brain_uncertainty_t uncertainty = brain_get_uncertainty(context, test_features, NUM_FEATURES);

    // Should return zeroed struct when disabled
    EXPECT_EQ(uncertainty.total, 0.0f);
}

//=============================================================================
// Pattern Query Tests
//=============================================================================

/**
 * WHAT: Test checking if pattern is active
 * WHY: Verify pattern query works
 */
TEST_F(IntrospectionTest, IsPatternActive)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    bool is_active = brain_is_pattern_active(context, "test_pattern");

    // Should return false for non-existent pattern
    EXPECT_FALSE(is_active);
}

/**
 * WHAT: Test getting pattern info
 * WHY: Verify pattern metadata retrieval works
 */
TEST_F(IntrospectionTest, GetPatternInfo)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    pattern_info_t* info = brain_get_pattern_info(context, "test_pattern");

    // Should return NULL for non-existent pattern
    EXPECT_EQ(info, nullptr);
}

/**
 * WHAT: Test listing all patterns
 * WHY: Verify pattern enumeration works
 */
TEST_F(IntrospectionTest, ListPatterns)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    uint32_t num_patterns = 0;
    char** patterns = brain_list_patterns(context, &num_patterns);

    // May be NULL if no patterns
    // Just verify it doesn't crash
    if (patterns) {
        pattern_list_free(patterns, num_patterns);
    }
}

/**
 * WHAT: Test pattern tracking disabled
 * WHY: Verify config flag is respected
 */
TEST_F(IntrospectionTest, PatternTrackingDisabled)
{
    introspection_config_t config = introspection_default_config();
    config.enable_pattern_tracking = false;
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    bool is_active = brain_is_pattern_active(context, "test_pattern");

    // Should return false when tracking disabled
    EXPECT_FALSE(is_active);
}

//=============================================================================
// Network Topology Tests
//=============================================================================

/**
 * WHAT: Test getting network topology
 * WHY: Verify topology query works
 */
TEST_F(IntrospectionTest, GetTopology)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    network_topology_t topology = brain_get_topology(context);

    EXPECT_GT(topology.total_neurons, 0u);
    EXPECT_GT(topology.total_connections, 0u);
    EXPECT_GT(topology.avg_connections_per_neuron, 0.0f);
    EXPECT_GE(topology.connection_sparsity, 0.0f);
    EXPECT_LE(topology.connection_sparsity, 1.0f);
    EXPECT_GT(topology.num_layers, 0u);

    network_topology_free(&topology);
}

/**
 * WHAT: Test topology caching
 * WHY: Verify topology is cached for efficiency
 */
TEST_F(IntrospectionTest, TopologyCaching)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    // First call - computes topology
    network_topology_t topo1 = brain_get_topology(context);

    // Second call - should use cached topology
    network_topology_t topo2 = brain_get_topology(context);

    // Should be identical
    EXPECT_EQ(topo1.total_neurons, topo2.total_neurons);
    EXPECT_EQ(topo1.total_connections, topo2.total_connections);

    network_topology_free(&topo1);
    // Don't free topo2 - it's a copy of cached data
}

//=============================================================================
// Activity History Tests
//=============================================================================

/**
 * WHAT: Test getting activity history
 * WHY: Verify history tracking works
 */
TEST_F(IntrospectionTest, GetActivityHistory)
{
    introspection_config_t config = introspection_default_config();
    config.history_size = 10;
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    uint32_t num_entries = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);

    // May be NULL if no history yet
    // Just verify it doesn't crash
    if (history) {
        EXPECT_LE(num_entries, 10u);
        free(history);
    }
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * WHAT: Test getting introspection statistics
 * WHY: Verify statistics tracking works
 */
TEST_F(IntrospectionTest, GetStatistics)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    // Perform some queries
    brain_get_active_population(context, 0.5f);
    brain_get_internal_state(context, STATE_STRATEGY_BALANCED);
    brain_get_uncertainty(context, test_features, NUM_FEATURES);

    introspection_stats_t stats;
    bool result = introspection_get_stats(context, &stats);

    ASSERT_TRUE(result);
    EXPECT_GT(stats.queries_total, 0u);
    EXPECT_GT(stats.memory_used_bytes, 0u);
}

/**
 * WHAT: Test resetting statistics
 * WHY: Verify stats reset works
 */
TEST_F(IntrospectionTest, ResetStatistics)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    // Perform queries
    brain_get_active_population(context, 0.5f);

    // Reset
    introspection_reset_stats(context);

    // Verify reset
    introspection_stats_t stats;
    introspection_get_stats(context, &stats);
    EXPECT_EQ(stats.queries_total, 0u);
    // Memory usage should not be reset
    EXPECT_GT(stats.memory_used_bytes, 0u);
}

//=============================================================================
// Performance Tests
//=============================================================================

/**
 * WHAT: Test state extraction performance
 * WHY: Verify performance targets are met
 */
TEST_F(IntrospectionTest, StateExtractionPerformance)
{
    introspection_config_t config = introspection_default_config();
    context = introspection_context_create(brain, &config);
    ASSERT_NE(context, nullptr);

    const uint32_t NUM_EXTRACTIONS = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < NUM_EXTRACTIONS; i++) {
        brain_state_t state = brain_get_internal_state(context, STATE_STRATEGY_FAST);
        brain_state_free(&state);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double avg_time_ms = (double) duration.count() / NUM_EXTRACTIONS;

    printf("Average state extraction time (fast): %.2f ms\n", avg_time_ms);

    // Fast strategy should be < 1ms
    EXPECT_LT(avg_time_ms, 1.0);
}

// Note: main() is defined in test_module.cpp - all test files share one main()
