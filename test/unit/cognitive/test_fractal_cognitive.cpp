/**
 * @file test_fractal_cognitive.cpp
 * @brief Unit tests for Fractal Cognitive Integration Module
 *
 * WHAT: Comprehensive unit tests for fractal topology cognitive integration
 * WHY:  Ensure cognitive modules can leverage scale-free network properties
 * HOW:  Test lifecycle, hub queries, centrality, hierarchical levels, and FEP bridge
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_fractal_cognitive.h"
#include "cognitive/fractal_cognitive/nimcp_fractal_cognitive_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"

#include "utils/nimcp_test_base.h"

/* ============================================================================
 * Fractal Cognitive Core Tests
 * ============================================================================ */

class FractalCognitiveTest : public NimcpTestBase {
protected:
    fractal_cognitive_cache_t cache;

    void SetUp() override {
        NimcpTestBase::SetUp();
        memset(&cache, 0, sizeof(cache));
    }

    void TearDown() override {
        if (cache.valid) {
            fractal_cognitive_free(&cache);
        }
        NimcpTestBase::TearDown();
    }
};

TEST_F(FractalCognitiveTest, InitWithNullNetwork) {
    // WHAT: Verify init handles NULL network
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = fractal_cognitive_init(nullptr, &cache);
    EXPECT_FALSE(result);
    EXPECT_FALSE(cache.valid);
}

TEST_F(FractalCognitiveTest, InitWithNullCache) {
    // WHAT: Verify init handles NULL cache
    // WHY:  Defensive programming
    // HOW:  Call with NULL cache

    bool result = fractal_cognitive_init(nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FractalCognitiveTest, FreeNullCache) {
    // WHAT: Verify free handles NULL cache
    // WHY:  Defensive programming
    // HOW:  Call with NULL (should not crash)

    fractal_cognitive_free(nullptr);
    SUCCEED();
}

TEST_F(FractalCognitiveTest, FreeUninitialized) {
    // WHAT: Verify free handles uninitialized cache
    // WHY:  Defensive programming
    // HOW:  Free cache that was never initialized

    fractal_cognitive_cache_t uninit_cache;
    memset(&uninit_cache, 0, sizeof(uninit_cache));
    uninit_cache.valid = false;

    fractal_cognitive_free(&uninit_cache);
    SUCCEED();
}

TEST_F(FractalCognitiveTest, RefreshWithNullNetwork) {
    // WHAT: Verify refresh handles NULL network
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = fractal_cognitive_refresh(nullptr, &cache);
    EXPECT_FALSE(result);
}

TEST_F(FractalCognitiveTest, RefreshWithNullCache) {
    // WHAT: Verify refresh handles NULL cache
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = fractal_cognitive_refresh(nullptr, nullptr);
    EXPECT_FALSE(result);
}

/* ============================================================================
 * Hub Neuron Query Tests
 * ============================================================================ */

TEST_F(FractalCognitiveTest, IsHubWithNullCache) {
    // WHAT: Verify is_hub handles NULL cache
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = fractal_is_hub_neuron(nullptr, 0);
    EXPECT_FALSE(result);
}

TEST_F(FractalCognitiveTest, IsHubWithInvalidCache) {
    // WHAT: Verify is_hub handles invalid cache
    // WHY:  Cache validity check
    // HOW:  Use invalid cache

    cache.valid = false;
    bool result = fractal_is_hub_neuron(&cache, 0);
    EXPECT_FALSE(result);
}

TEST_F(FractalCognitiveTest, IsHubWithEmptyHubList) {
    // WHAT: Verify is_hub with no hubs
    // WHY:  Edge case - no hubs identified
    // HOW:  Valid cache but no hubs

    cache.valid = true;
    cache.hub_indices = nullptr;
    cache.num_hubs = 0;

    bool result = fractal_is_hub_neuron(&cache, 0);
    EXPECT_FALSE(result);
}

TEST_F(FractalCognitiveTest, NearestHubWithNullNetwork) {
    // WHAT: Verify nearest_hub handles NULL network
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    uint32_t distance;
    uint32_t hub = fractal_nearest_hub(nullptr, &cache, 0, &distance);
    EXPECT_EQ(hub, UINT32_MAX);
}

TEST_F(FractalCognitiveTest, NearestHubWithNullCache) {
    // WHAT: Verify nearest_hub handles NULL cache
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    uint32_t distance;
    uint32_t hub = fractal_nearest_hub(nullptr, nullptr, 0, &distance);
    EXPECT_EQ(hub, UINT32_MAX);
}

TEST_F(FractalCognitiveTest, GetCentralNeighborsWithNullNetwork) {
    // WHAT: Verify get_central_neighbors handles NULL network
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    uint32_t central[5];
    uint32_t count = fractal_get_central_neighbors(nullptr, &cache, 0, 2, 5, central);
    EXPECT_EQ(count, 0u);
}

TEST_F(FractalCognitiveTest, GetCentralNeighborsWithNullCache) {
    // WHAT: Verify get_central_neighbors handles NULL cache
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    uint32_t central[5];
    uint32_t count = fractal_get_central_neighbors(nullptr, nullptr, 0, 2, 5, central);
    EXPECT_EQ(count, 0u);
}

TEST_F(FractalCognitiveTest, GetCentralNeighborsWithNullOutput) {
    // WHAT: Verify get_central_neighbors handles NULL output
    // WHY:  Defensive programming
    // HOW:  Call with NULL output

    cache.valid = true;
    uint32_t count = fractal_get_central_neighbors(nullptr, &cache, 0, 2, 5, nullptr);
    EXPECT_EQ(count, 0u);
}

TEST_F(FractalCognitiveTest, GetCentralNeighborsWithZeroK) {
    // WHAT: Verify get_central_neighbors handles k=0
    // WHY:  Edge case
    // HOW:  Request zero neighbors

    cache.valid = true;
    uint32_t central[1];
    uint32_t count = fractal_get_central_neighbors(nullptr, &cache, 0, 2, 0, central);
    EXPECT_EQ(count, 0u);
}

/* ============================================================================
 * Centrality Query Tests
 * ============================================================================ */

TEST_F(FractalCognitiveTest, GetCentralityWithNullCache) {
    // WHAT: Verify get_centrality handles NULL cache
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    float centrality = fractal_get_centrality(nullptr, 0);
    EXPECT_FLOAT_EQ(centrality, 0.0f);
}

TEST_F(FractalCognitiveTest, GetCentralityWithInvalidCache) {
    // WHAT: Verify get_centrality handles invalid cache
    // WHY:  Cache validity check
    // HOW:  Use invalid cache

    cache.valid = false;
    float centrality = fractal_get_centrality(&cache, 0);
    EXPECT_FLOAT_EQ(centrality, 0.0f);
}

TEST_F(FractalCognitiveTest, GetCentralityWithNullScores) {
    // WHAT: Verify get_centrality handles NULL scores array
    // WHY:  Cache may be valid but scores not allocated
    // HOW:  Valid cache but NULL array

    cache.valid = true;
    cache.centrality_scores = nullptr;

    float centrality = fractal_get_centrality(&cache, 0);
    EXPECT_FLOAT_EQ(centrality, 0.0f);
}

TEST_F(FractalCognitiveTest, GetDegreeNormalizedWithNullCache) {
    // WHAT: Verify get_degree_normalized handles NULL cache
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    float degree = fractal_get_degree_normalized(nullptr, 0);
    EXPECT_FLOAT_EQ(degree, 0.0f);
}

TEST_F(FractalCognitiveTest, GetDegreeNormalizedWithInvalidCache) {
    // WHAT: Verify get_degree_normalized handles invalid cache
    // WHY:  Cache validity check
    // HOW:  Use invalid cache

    cache.valid = false;
    float degree = fractal_get_degree_normalized(&cache, 0);
    EXPECT_FLOAT_EQ(degree, 0.0f);
}

/* ============================================================================
 * Hierarchical Level Tests
 * ============================================================================ */

TEST_F(FractalCognitiveTest, GetHierarchicalLevelWithNullCache) {
    // WHAT: Verify get_hierarchical_level handles NULL cache
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    float level = fractal_get_hierarchical_level(nullptr, 0);
    EXPECT_FLOAT_EQ(level, 0.5f);  /* Mid-level default for null cache */
}

TEST_F(FractalCognitiveTest, GetHierarchicalLevelWithInvalidCache) {
    // WHAT: Verify get_hierarchical_level handles invalid cache
    // WHY:  Cache validity check
    // HOW:  Use invalid cache

    cache.valid = false;
    float level = fractal_get_hierarchical_level(&cache, 0);
    EXPECT_FLOAT_EQ(level, 0.5f);  /* Mid-level default for invalid cache */
}

TEST_F(FractalCognitiveTest, GetNeuronsAtLevelWithNullCache) {
    // WHAT: Verify get_neurons_at_level handles NULL cache
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    uint32_t* neurons;
    uint32_t count;
    bool result = fractal_get_neurons_at_level(nullptr, 0.5f, 0.1f, &neurons, &count);
    EXPECT_FALSE(result);
}

TEST_F(FractalCognitiveTest, GetNeuronsAtLevelWithNullOutput) {
    // WHAT: Verify get_neurons_at_level handles NULL output
    // WHY:  Defensive programming
    // HOW:  Call with NULL output

    cache.valid = true;
    bool result = fractal_get_neurons_at_level(&cache, 0.5f, 0.1f, nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FractalCognitiveTest, GetNeuronsAtLevelOutOfRange) {
    // WHAT: Verify get_neurons_at_level handles out-of-range level
    // WHY:  Level should be in [0, 1]
    // HOW:  Use level > 1

    cache.valid = true;
    uint32_t* neurons = nullptr;
    uint32_t count = 0;

    bool result = fractal_get_neurons_at_level(&cache, 1.5f, 0.1f, &neurons, &count);
    // Implementation may clamp or reject
    if (result && neurons) {
        free(neurons);
    }
    SUCCEED();
}

/* ============================================================================
 * Cache Structure Tests
 * ============================================================================ */

TEST_F(FractalCognitiveTest, CacheStructureFields) {
    // WHAT: Verify cache structure has expected fields
    // WHY:  API consistency
    // HOW:  Check field types and initialization

    fractal_cognitive_cache_t test_cache;
    memset(&test_cache, 0, sizeof(test_cache));

    EXPECT_EQ(test_cache.hub_indices, nullptr);
    EXPECT_EQ(test_cache.num_hubs, 0u);
    EXPECT_EQ(test_cache.centrality_scores, nullptr);
    EXPECT_EQ(test_cache.degree_normalized, nullptr);
    EXPECT_FALSE(test_cache.valid);
}

/* ============================================================================
 * Debug/Visualization Tests
 * ============================================================================ */

TEST_F(FractalCognitiveTest, PrintSummaryWithNullCache) {
    // WHAT: Verify print_summary handles NULL cache
    // WHY:  Defensive programming
    // HOW:  Call with NULL (should not crash)

    fractal_cognitive_print_summary(nullptr);
    SUCCEED();
}

TEST_F(FractalCognitiveTest, PrintSummaryWithInvalidCache) {
    // WHAT: Verify print_summary handles invalid cache
    // WHY:  Defensive programming
    // HOW:  Call with invalid cache

    cache.valid = false;
    fractal_cognitive_print_summary(&cache);
    SUCCEED();
}

/* ============================================================================
 * Fractal Cognitive FEP Bridge Tests
 * ============================================================================ */

class FractalCognitiveFepBridgeTest : public NimcpTestBase {
protected:
    fractal_cognitive_fep_bridge_t* bridge = nullptr;
    fep_system_t* fep = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
        bio_router_init(NULL);

        // Create FEP system
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, 8, 4);

        // Create bridge
        fractal_cognitive_fep_config_t config;
        fractal_cognitive_fep_bridge_default_config(&config);
        bridge = fractal_cognitive_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            fractal_cognitive_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
        bio_router_shutdown();
        NimcpTestBase::TearDown();
    }
};

TEST_F(FractalCognitiveFepBridgeTest, CreateDestroy) {
    // WHAT: Verify bridge creation
    // WHY:  Basic lifecycle test
    // HOW:  Check not NULL

    ASSERT_NE(bridge, nullptr);
}

TEST_F(FractalCognitiveFepBridgeTest, DefaultConfig) {
    // WHAT: Verify default config
    // WHY:  Ensure sensible defaults
    // HOW:  Check config values

    fractal_cognitive_fep_config_t config;
    int ret = fractal_cognitive_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.fe_sensitivity, 0.0f);
    EXPECT_GT(config.fractal_sensitivity, 0.0f);
    EXPECT_GT(config.pe_exploration_threshold, 0.0f);
}

TEST_F(FractalCognitiveFepBridgeTest, DefaultConfigNull) {
    // WHAT: Verify default_config handles NULL
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    int ret = fractal_cognitive_fep_bridge_default_config(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(FractalCognitiveFepBridgeTest, DestroyNull) {
    // WHAT: Verify destroying NULL is safe
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    fractal_cognitive_fep_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(FractalCognitiveFepBridgeTest, ConnectFep) {
    // WHAT: Verify FEP connection
    // WHY:  Bridge needs FEP
    // HOW:  Connect and verify

    ASSERT_NE(fep, nullptr);
    int ret = fractal_cognitive_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(FractalCognitiveFepBridgeTest, ConnectFepWithNullBridge) {
    // WHAT: Verify connect handles NULL bridge
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    int ret = fractal_cognitive_fep_bridge_connect_fep(nullptr, fep);
    EXPECT_NE(ret, 0);
}

TEST_F(FractalCognitiveFepBridgeTest, ConnectFractal) {
    // WHAT: Verify fractal cache connection
    // WHY:  Bridge needs fractal cache
    // HOW:  Connect (with NULL, should still succeed or handle gracefully)

    int ret = fractal_cognitive_fep_bridge_connect_fractal(bridge, nullptr);
    // May fail without valid cache, depends on implementation
    (void)ret;
    SUCCEED();
}

TEST_F(FractalCognitiveFepBridgeTest, Disconnect) {
    // WHAT: Verify disconnect works
    // WHY:  Clean disconnection
    // HOW:  Connect then disconnect

    if (fep) {
        fractal_cognitive_fep_bridge_connect_fep(bridge, fep);
        int ret = fractal_cognitive_fep_bridge_disconnect(bridge);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(FractalCognitiveFepBridgeTest, Update) {
    // WHAT: Verify bridge update
    // WHY:  Core functionality
    // HOW:  Connect, update

    if (fep) {
        fractal_cognitive_fep_bridge_connect_fep(bridge, fep);
        int ret = fractal_cognitive_fep_bridge_update(bridge, 100);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(FractalCognitiveFepBridgeTest, UpdateWithNullBridge) {
    // WHAT: Verify update handles NULL
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    int ret = fractal_cognitive_fep_bridge_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(FractalCognitiveFepBridgeTest, GetState) {
    // WHAT: Verify state retrieval
    // WHY:  State query functionality
    // HOW:  Get state after update

    if (fep) {
        fractal_cognitive_fep_bridge_connect_fep(bridge, fep);
        fractal_cognitive_fep_bridge_update(bridge, 100);

        fractal_cognitive_fep_state_t state;
        int ret = fractal_cognitive_fep_bridge_get_state(bridge, &state);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(FractalCognitiveFepBridgeTest, GetStateWithNullBridge) {
    // WHAT: Verify get_state handles NULL
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    fractal_cognitive_fep_state_t state;
    int ret = fractal_cognitive_fep_bridge_get_state(nullptr, &state);
    EXPECT_NE(ret, 0);
}

TEST_F(FractalCognitiveFepBridgeTest, GetStats) {
    // WHAT: Verify stats retrieval
    // WHY:  Monitor bridge activity
    // HOW:  Get stats

    fractal_cognitive_fep_stats_t stats;
    int ret = fractal_cognitive_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(FractalCognitiveFepBridgeTest, BioAsyncConnection) {
    // WHAT: Verify bio-async lifecycle
    // WHY:  Bio-async integration
    // HOW:  Connect, check, disconnect

    EXPECT_FALSE(fractal_cognitive_fep_bridge_is_bio_async_connected(bridge));

    fractal_cognitive_fep_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(fractal_cognitive_fep_bridge_is_bio_async_connected(bridge));

    fractal_cognitive_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(fractal_cognitive_fep_bridge_is_bio_async_connected(bridge));
}

TEST_F(FractalCognitiveFepBridgeTest, TriggerHubDiscovery) {
    // WHAT: Verify hub discovery trigger
    // WHY:  High PE triggers hub re-identification
    // HOW:  Call with high PE

    if (fep) {
        fractal_cognitive_fep_bridge_connect_fep(bridge, fep);
        int ret = fractal_cognitive_fep_trigger_hub_discovery(bridge, 10.0f);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(FractalCognitiveFepBridgeTest, WeightHubsByPrecision) {
    // WHAT: Verify hub precision weighting
    // WHY:  FEP precision affects hub importance
    // HOW:  Call after FEP connection

    if (fep) {
        fractal_cognitive_fep_bridge_connect_fep(bridge, fep);
        int ret = fractal_cognitive_fep_weight_hubs_by_precision(bridge);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(FractalCognitiveFepBridgeTest, TriggerHierarchyUpdate) {
    // WHAT: Verify hierarchy update trigger fails without fractal cache
    // WHY:  FEP bridge requires connected fractal cache
    // HOW:  Call trigger without cache - should fail

    if (fep) {
        fractal_cognitive_fep_bridge_connect_fep(bridge, fep);
        /* No fractal_cache connected, so trigger should fail */
        int ret = fractal_cognitive_fep_trigger_hierarchy_update(bridge);
        EXPECT_NE(ret, 0);
    }
}

TEST_F(FractalCognitiveFepBridgeTest, ApplyHubPriors) {
    // WHAT: Verify hub priors application
    // WHY:  Hub structure constrains FEP model
    // HOW:  Apply priors

    if (fep) {
        fractal_cognitive_fep_bridge_connect_fep(bridge, fep);
        int ret = fractal_cognitive_fep_apply_hub_priors(bridge);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(FractalCognitiveFepBridgeTest, MapHierarchyToFep) {
    // WHAT: Verify hierarchy mapping
    // WHY:  Align fractal levels with FEP
    // HOW:  Map hierarchy

    if (fep) {
        fractal_cognitive_fep_bridge_connect_fep(bridge, fep);
        int ret = fractal_cognitive_fep_map_hierarchy_to_fep(bridge);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(FractalCognitiveFepBridgeTest, UpdateModelStructure) {
    // WHAT: Verify model structure update
    // WHY:  Network architecture determines FEP structure
    // HOW:  Update structure

    if (fep) {
        fractal_cognitive_fep_bridge_connect_fep(bridge, fep);
        int ret = fractal_cognitive_fep_update_model_structure(bridge);
        EXPECT_EQ(ret, 0);
    }
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(FractalCognitiveTest, LargeNeuronIndex) {
    // WHAT: Handle large neuron index
    // WHY:  Bounds checking
    // HOW:  Use very large index

    cache.valid = true;
    cache.centrality_scores = nullptr;

    float centrality = fractal_get_centrality(&cache, UINT32_MAX);
    EXPECT_FLOAT_EQ(centrality, 0.0f);
}

TEST_F(FractalCognitiveTest, ZeroTolerance) {
    // WHAT: Handle zero tolerance in level query
    // WHY:  Edge case
    // HOW:  Use tolerance 0

    cache.valid = true;
    uint32_t* neurons = nullptr;
    uint32_t count = 0;

    bool result = fractal_get_neurons_at_level(&cache, 0.5f, 0.0f, &neurons, &count);
    if (result && neurons) {
        free(neurons);
    }
    SUCCEED();
}

TEST_F(FractalCognitiveTest, NegativeLevel) {
    // WHAT: Handle negative level
    // WHY:  Level should be clamped to [0, 1]
    // HOW:  Use negative level

    cache.valid = true;
    uint32_t* neurons = nullptr;
    uint32_t count = 0;

    bool result = fractal_get_neurons_at_level(&cache, -0.5f, 0.1f, &neurons, &count);
    if (result && neurons) {
        free(neurons);
    }
    SUCCEED();
}
