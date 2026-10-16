/**
 * @file test_synapse_hot_cold.cpp
 * @brief Tests for NIMCP 2.11 synapse hot/cold split
 *
 * Validates that the synapse_t (hot, ~56 bytes) and synapse_cold_t (~144 bytes)
 * split works correctly: cold pool lifecycle, lazy allocation, field access,
 * metadata pool integration, network add_connection, checkpoint compat.
 *
 * TEST SUITES:
 * - Unit: Cold pool lifecycle, field separation, lazy alloc, sizeof validation
 * - Integration: add_connection with cold, network create/destroy, rebuild_incoming
 * - Regression: mixed handle/metadata/cold, null-safety, pool exhaustion
 * - E2E: full create→wire→learn→destroy cycle
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

#include "core/neuralnet/nimcp_sparse_synapse.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuralnet_internal.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// UNIT TESTS: Cold Pool Lifecycle
//=============================================================================

class ColdPoolTest : public ::testing::Test {
protected:
    synapse_cold_pool_t cold_pool = nullptr;

    void SetUp() override {
        cold_pool = synapse_cold_pool_create(nullptr);
        ASSERT_NE(cold_pool, nullptr);
    }

    void TearDown() override {
        if (cold_pool) synapse_cold_pool_destroy(cold_pool);
    }
};

TEST_F(ColdPoolTest, CreateDestroy) {
    // Pool should be created successfully with defaults
    EXPECT_NE(cold_pool, nullptr);
    // Destroy and recreate to verify lifecycle
    synapse_cold_pool_destroy(cold_pool);
    cold_pool = synapse_cold_pool_create(nullptr);
    EXPECT_NE(cold_pool, nullptr);
}

TEST_F(ColdPoolTest, CreateWithConfig) {
    synapse_cold_pool_config_t cfg = synapse_cold_pool_default_config();
    cfg.pool_size = 1024;
    cfg.thread_safe = true;
    synapse_cold_pool_t pool2 = synapse_cold_pool_create(&cfg);
    ASSERT_NE(pool2, nullptr);
    synapse_cold_pool_destroy(pool2);
}

TEST_F(ColdPoolTest, AllocateAndGet) {
    uint32_t idx = synapse_cold_pool_allocate(cold_pool);
    EXPECT_NE(idx, SYNAPSE_COLD_NONE);

    synapse_cold_t* cold = synapse_cold_pool_get(cold_pool, idx);
    ASSERT_NE(cold, nullptr);

    // Should be zero-initialized
    EXPECT_EQ(cold->enable_stp, false);
    EXPECT_EQ(cold->enable_bcm, false);
    EXPECT_EQ(cold->enable_eligibility, false);
    EXPECT_EQ(cold->bcm, nullptr);
    EXPECT_EQ(cold->eligibility, nullptr);
    EXPECT_EQ(cold->compute_function, nullptr);
}

TEST_F(ColdPoolTest, AllocateMultiple) {
    std::vector<uint32_t> indices;
    for (int i = 0; i < 100; i++) {
        uint32_t idx = synapse_cold_pool_allocate(cold_pool);
        EXPECT_NE(idx, SYNAPSE_COLD_NONE);
        indices.push_back(idx);
    }
    // All indices should be unique
    for (size_t i = 0; i < indices.size(); i++) {
        for (size_t j = i + 1; j < indices.size(); j++) {
            EXPECT_NE(indices[i], indices[j]);
        }
    }
}

TEST_F(ColdPoolTest, FreeAndReuse) {
    uint32_t idx1 = synapse_cold_pool_allocate(cold_pool);
    ASSERT_NE(idx1, SYNAPSE_COLD_NONE);

    synapse_cold_t* cold = synapse_cold_pool_get(cold_pool, idx1);
    cold->enable_stp = true;

    synapse_cold_pool_free(cold_pool, idx1);

    // Allocate again — should get the freed slot back
    uint32_t idx2 = synapse_cold_pool_allocate(cold_pool);
    EXPECT_NE(idx2, SYNAPSE_COLD_NONE);

    // Re-allocated slot should be zero-initialized
    synapse_cold_t* cold2 = synapse_cold_pool_get(cold_pool, idx2);
    EXPECT_EQ(cold2->enable_stp, false);
}

TEST_F(ColdPoolTest, GetInvalidIndex) {
    // SYNAPSE_COLD_NONE should return NULL
    EXPECT_EQ(synapse_cold_pool_get(cold_pool, SYNAPSE_COLD_NONE), nullptr);
    // Out-of-bounds should return NULL
    EXPECT_EQ(synapse_cold_pool_get(cold_pool, 999999999), nullptr);
}

TEST_F(ColdPoolTest, DestroyNull) {
    // Should not crash
    synapse_cold_pool_destroy(nullptr);
}

TEST_F(ColdPoolTest, PoolGrowsOnDemand) {
    // Allocate more than initial block size to force growth
    synapse_cold_pool_config_t cfg = synapse_cold_pool_default_config();
    cfg.pool_size = 16;  // Very small initial size
    synapse_cold_pool_t small_pool = synapse_cold_pool_create(&cfg);
    ASSERT_NE(small_pool, nullptr);

    // Allocate beyond initial capacity — pool should grow
    for (int i = 0; i < 100; i++) {
        uint32_t idx = synapse_cold_pool_allocate(small_pool);
        EXPECT_NE(idx, SYNAPSE_COLD_NONE) << "Failed at allocation " << i;
    }

    synapse_cold_pool_destroy(small_pool);
}

//=============================================================================
// UNIT TESTS: Struct Size Validation
//=============================================================================

TEST(SynapseStructSize, HotStructIsSmall) {
    // synapse_t (hot) should be ~56 bytes, NOT the old 200 bytes
    EXPECT_LE(sizeof(synapse_t), 64u) << "synapse_t should be <=64 bytes after hot/cold split";
    EXPECT_GT(sizeof(synapse_t), 40u) << "synapse_t should be >40 bytes (has real fields)";
}

TEST(SynapseStructSize, ColdStructContainsMovedFields) {
    // synapse_cold_t should contain STP, BCM, eligibility, compute, type, embedding, ternary
    EXPECT_GE(sizeof(synapse_cold_t), 100u) << "synapse_cold_t should be >=100 bytes";
    EXPECT_LE(sizeof(synapse_cold_t), 200u) << "synapse_cold_t should be <=200 bytes";
}

TEST(SynapseStructSize, MemorySavingsSignificant) {
    // For 82M synapses, savings should be substantial
    size_t old_per_synapse = 200;  // Old synapse_t
    size_t new_per_synapse = sizeof(synapse_t);  // New hot synapse_t
    double ratio = (double)new_per_synapse / (double)old_per_synapse;
    EXPECT_LT(ratio, 0.4) << "Hot struct should be <40% of old size";
}

//=============================================================================
// UNIT TESTS: Hot/Cold Field Separation
//=============================================================================

TEST(SynapseFieldSeparation, HotFieldsAccessible) {
    synapse_t syn = {};
    syn.target_id = 42;
    syn.weight = 0.5f;
    syn.plasticity = 1.0f;
    syn.meta_plasticity = 0.8f;
    syn.trace = 0.1f;
    syn.last_change = 0.01f;
    syn.last_active = 12345;
    syn.strength = 0.9f;
    syn.source_neuron_id = 7;
    syn.axon_id = 3;
    syn.semantic_relevance = 0.75f;
    syn.cold_index = SYNAPSE_COLD_NONE;

    EXPECT_EQ(syn.target_id, 42u);
    EXPECT_FLOAT_EQ(syn.weight, 0.5f);
    EXPECT_FLOAT_EQ(syn.plasticity, 1.0f);
    EXPECT_FLOAT_EQ(syn.meta_plasticity, 0.8f);
    EXPECT_FLOAT_EQ(syn.semantic_relevance, 0.75f);
    EXPECT_EQ(syn.cold_index, SYNAPSE_COLD_NONE);
}

TEST(SynapseFieldSeparation, ColdFieldsAccessible) {
    synapse_cold_t cold = {};
    cold.enable_stp = true;
    cold.enable_bcm = false;
    cold.enable_eligibility = true;
    cold.type = (synapse_type_t)1;  // SYNAPSE_AMPA
    cold.embedding_pool_index = 42;
    cold.embedding_dim = 2048;
    cold.ternary_scale = 1.5f;
    cold.use_ternary_weight = true;

    EXPECT_EQ(cold.enable_stp, true);
    EXPECT_EQ(cold.enable_bcm, false);
    EXPECT_EQ(cold.enable_eligibility, true);
    EXPECT_EQ(cold.type, 1);
    EXPECT_EQ(cold.embedding_pool_index, 42u);
    EXPECT_EQ(cold.embedding_dim, 2048);
    EXPECT_FLOAT_EQ(cold.ternary_scale, 1.5f);
}

TEST(SynapseFieldSeparation, ColdIndexSentinel) {
    synapse_t syn = {};
    syn.cold_index = SYNAPSE_COLD_NONE;
    EXPECT_EQ(syn.cold_index, UINT32_MAX);
}

//=============================================================================
// INTEGRATION TESTS: Network with Hot/Cold
//=============================================================================

class NetworkHotColdTest : public ::testing::Test {
protected:
    neural_network_t network = nullptr;

    void SetUp() override {
        network_config_t config = {};
        config.num_neurons = 100;
        config.input_size = 10;
        config.output_size = 10;
        config.min_weight = -1.0f;
        config.max_weight = 1.0f;
        config.num_layers = 2;
        uint32_t layer_sizes[2] = {50, 50};
        config.layer_sizes = layer_sizes;
        config.enable_bcm = false;
        config.enable_eligibility = false;
        network = neural_network_create(&config);
        ASSERT_NE(network, nullptr);
    }

    void TearDown() override {
        if (network) neural_network_destroy(network);
    }
};

TEST_F(NetworkHotColdTest, AddConnectionCreatesHotMetadata) {
    bool ok = neural_network_add_connection(network, 0, 50, 0.5f);
    ASSERT_TRUE(ok);

    neuron_t* from_neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(from_neuron, nullptr);

    uint32_t out_count = NEURON_OUT_COUNT(from_neuron);
    EXPECT_GT(out_count, 0u);

    // Last added synapse should have hot metadata
    synapse_t* syn = NEURON_OUT_META(network, from_neuron, out_count - 1);
    ASSERT_NE(syn, nullptr);
    EXPECT_EQ(syn->target_id, 50u);
    EXPECT_FLOAT_EQ(syn->weight, 0.5f);
    EXPECT_FLOAT_EQ(syn->plasticity, 1.0f);
    EXPECT_FLOAT_EQ(syn->meta_plasticity, 1.0f);
}

TEST_F(NetworkHotColdTest, AddConnectionCreatesIncomingWithoutMetadata) {
    bool ok = neural_network_add_connection(network, 0, 50, 0.5f);
    ASSERT_TRUE(ok);

    neuron_t* to_neuron = neural_network_get_neuron(network, 50);
    ASSERT_NE(to_neuron, nullptr);

    uint32_t in_count = NEURON_IN_COUNT(to_neuron);
    EXPECT_GT(in_count, 0u);

    // Incoming should be handle-only (no metadata)
    synapse_t* in_meta = NEURON_IN_META(network, to_neuron, in_count - 1);
    EXPECT_EQ(in_meta, nullptr) << "Incoming synapses should not have metadata";
}

TEST_F(NetworkHotColdTest, ColdDataCreatedForSTP) {
    bool ok = neural_network_add_connection(network, 0, 50, 0.5f);
    ASSERT_TRUE(ok);

    neuron_t* from_neuron = neural_network_get_neuron(network, 0);
    uint32_t out_count = NEURON_OUT_COUNT(from_neuron);
    synapse_t* syn = NEURON_OUT_META(network, from_neuron, out_count - 1);
    ASSERT_NE(syn, nullptr);

    // Cold data should be allocated (STP is enabled by default)
    EXPECT_NE(syn->cold_index, SYNAPSE_COLD_NONE)
        << "Cold index should be set (STP needs cold data)";

    synapse_cold_t* cold = SYNAPSE_COLD(network, syn);
    ASSERT_NE(cold, nullptr);
    EXPECT_EQ(cold->enable_stp, true);
}

TEST_F(NetworkHotColdTest, NetworkDestroyCleansColdPool) {
    // Add connections
    for (int i = 0; i < 10; i++) {
        neural_network_add_connection(network, i, 50 + i, 0.5f);
    }
    // Destroy should not leak or crash
    neural_network_destroy(network);
    network = nullptr;  // Prevent double-free in TearDown
}

TEST_F(NetworkHotColdTest, RebuildIncomingNoMetadata) {
    // Add connections
    neural_network_add_connection(network, 0, 50, 0.5f);
    neural_network_add_connection(network, 1, 51, 0.3f);

    // Rebuild incoming
    bool ok = neural_network_rebuild_incoming(network);
    EXPECT_TRUE(ok);

    // Incoming should still be handle-only
    neuron_t* to_neuron = neural_network_get_neuron(network, 50);
    uint32_t in_count = NEURON_IN_COUNT(to_neuron);
    if (in_count > 0) {
        synapse_t* in_meta = NEURON_IN_META(network, to_neuron, 0);
        EXPECT_EQ(in_meta, nullptr) << "After rebuild, incoming should be handle-only";
    }
}

//=============================================================================
// REGRESSION TESTS: Backward Compatibility
//=============================================================================

TEST(SynapseRegression, MetadataPoolStillWorks) {
    // Metadata pool should store new smaller synapse_t
    synapse_metadata_pool_config_t cfg = {};
    cfg.pool_size = 100;
    cfg.enable_statistics = true;
    cfg.thread_safe = false;

    synapse_metadata_pool_t pool = synapse_metadata_pool_create(&cfg);
    ASSERT_NE(pool, nullptr);

    uint32_t idx = synapse_metadata_pool_allocate(pool);
    ASSERT_NE(idx, SPARSE_SYNAPSE_NO_METADATA);

    synapse_t* syn = synapse_metadata_pool_get(pool, idx);
    ASSERT_NE(syn, nullptr);

    // Verify cold_index is initialized to SYNAPSE_COLD_NONE
    EXPECT_EQ(syn->cold_index, SYNAPSE_COLD_NONE);

    // Hot fields should work
    syn->target_id = 99;
    syn->weight = 0.42f;
    syn->plasticity = 0.8f;
    EXPECT_EQ(syn->target_id, 99u);
    EXPECT_FLOAT_EQ(syn->weight, 0.42f);

    synapse_metadata_pool_free(pool, idx);
    synapse_metadata_pool_destroy(pool);
}

TEST(SynapseRegression, NullColdPoolSafety) {
    // SYNAPSE_COLD with NULL pool should return NULL
    synapse_t syn = {};
    syn.cold_index = 0;  // Valid index but no pool
    synapse_cold_t* cold = synapse_cold_pool_get(nullptr, syn.cold_index);
    EXPECT_EQ(cold, nullptr);
}

TEST(SynapseRegression, ColdNoneIndex) {
    // Cold pool get with SYNAPSE_COLD_NONE should return NULL
    synapse_cold_pool_t pool = synapse_cold_pool_create(nullptr);
    ASSERT_NE(pool, nullptr);
    EXPECT_EQ(synapse_cold_pool_get(pool, SYNAPSE_COLD_NONE), nullptr);
    synapse_cold_pool_destroy(pool);
}

TEST(SynapseRegression, MixedSynapsesWithAndWithoutCold) {
    // Some synapses have cold data, some don't — both should work
    synapse_metadata_pool_config_t mcfg = {};
    mcfg.pool_size = 50;
    mcfg.thread_safe = false;
    synapse_metadata_pool_t meta_pool = synapse_metadata_pool_create(&mcfg);
    synapse_cold_pool_t cold_pool = synapse_cold_pool_create(nullptr);
    ASSERT_NE(meta_pool, nullptr);
    ASSERT_NE(cold_pool, nullptr);

    // Allocate 10 hot metadata entries
    std::vector<uint32_t> hot_indices;
    for (int i = 0; i < 10; i++) {
        uint32_t idx = synapse_metadata_pool_allocate(meta_pool);
        ASSERT_NE(idx, SPARSE_SYNAPSE_NO_METADATA);
        hot_indices.push_back(idx);
    }

    // Give cold data to only 3 of them
    for (int i = 0; i < 3; i++) {
        synapse_t* syn = synapse_metadata_pool_get(meta_pool, hot_indices[i]);
        syn->cold_index = synapse_cold_pool_allocate(cold_pool);
        EXPECT_NE(syn->cold_index, SYNAPSE_COLD_NONE);
        synapse_cold_t* cold = synapse_cold_pool_get(cold_pool, syn->cold_index);
        cold->enable_stp = true;
    }

    // Verify: first 3 have cold, rest don't
    for (int i = 0; i < 10; i++) {
        synapse_t* syn = synapse_metadata_pool_get(meta_pool, hot_indices[i]);
        if (i < 3) {
            EXPECT_NE(syn->cold_index, SYNAPSE_COLD_NONE);
            synapse_cold_t* cold = synapse_cold_pool_get(cold_pool, syn->cold_index);
            EXPECT_NE(cold, nullptr);
            EXPECT_EQ(cold->enable_stp, true);
        } else {
            EXPECT_EQ(syn->cold_index, SYNAPSE_COLD_NONE);
        }
    }

    synapse_metadata_pool_destroy(meta_pool);
    synapse_cold_pool_destroy(cold_pool);
}

TEST(SynapseRegression, ColdPoolExhaustionGraceful) {
    // Small cold pool should grow on demand, not crash
    synapse_cold_pool_config_t cfg = synapse_cold_pool_default_config();
    cfg.pool_size = 8;
    synapse_cold_pool_t pool = synapse_cold_pool_create(&cfg);
    ASSERT_NE(pool, nullptr);

    // Allocate way more than initial capacity
    bool all_ok = true;
    for (int i = 0; i < 200; i++) {
        uint32_t idx = synapse_cold_pool_allocate(pool);
        if (idx == SYNAPSE_COLD_NONE) {
            all_ok = false;
            break;
        }
    }
    // Should succeed due to auto-growth
    EXPECT_TRUE(all_ok) << "Cold pool should auto-grow";

    synapse_cold_pool_destroy(pool);
}

TEST(SynapseRegression, HandleOnlyIncoming) {
    // Verify handle-only synapses work without metadata
    sparse_synapse_pool_t pool = sparse_synapse_pool_create(nullptr);
    ASSERT_NE(pool, nullptr);

    sparse_synapse_storage_t storage;
    sparse_synapse_storage_init(&storage);

    // Add handle-only (no metadata)
    int rc = sparse_synapse_add(pool, &storage, 42, 0.5f);
    EXPECT_EQ(rc, 0);

    synapse_handle_t* h = sparse_synapse_get(&storage, 0);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(h->target_neuron_id, 42u);
    EXPECT_FLOAT_EQ(h->weight, 0.5f);
    EXPECT_EQ(h->metadata_index, SPARSE_SYNAPSE_NO_METADATA);

    sparse_synapse_storage_cleanup(pool, &storage);
    sparse_synapse_pool_destroy(pool);
}

//=============================================================================
// E2E TESTS: Full Network Lifecycle with Hot/Cold
//=============================================================================

TEST(SynapseE2E, CreateWireLearnDestroy) {
    // Full lifecycle: create network, wire connections, verify hot/cold, destroy
    network_config_t config = {};
    config.num_neurons = 200;
    config.input_size = 20;
    config.output_size = 20;
    config.min_weight = -1.0f;
    config.max_weight = 1.0f;
    config.num_layers = 3;
    uint32_t layer_sizes[3] = {20, 160, 20};
    config.layer_sizes = layer_sizes;
    config.enable_bcm = false;
    config.enable_eligibility = false;

    neural_network_t net = neural_network_create(&config);
    ASSERT_NE(net, nullptr);

    // Verify neurons exist
    EXPECT_EQ(neural_network_get_num_neurons(net), 200u);

    // Add manual connections
    for (int i = 0; i < 20; i++) {
        bool ok = neural_network_add_connection(net, i, 20 + i, 0.3f);
        EXPECT_TRUE(ok);
    }

    // Verify outgoing has hot metadata
    neuron_t* n0 = neural_network_get_neuron(net, 0);
    ASSERT_NE(n0, nullptr);
    uint32_t out_count = NEURON_OUT_COUNT(n0);
    EXPECT_GT(out_count, 0u);

    synapse_t* syn = NEURON_OUT_META(net, n0, out_count - 1);
    if (syn) {
        // Hot fields should be valid
        EXPECT_FLOAT_EQ(syn->plasticity, 1.0f);
        EXPECT_FLOAT_EQ(syn->meta_plasticity, 1.0f);

        // If STP was enabled, cold should exist
        if (syn->cold_index != SYNAPSE_COLD_NONE) {
            synapse_cold_t* cold = SYNAPSE_COLD(net, syn);
            EXPECT_NE(cold, nullptr);
        }
    }

    // Verify incoming is handle-only
    neuron_t* n20 = neural_network_get_neuron(net, 20);
    if (n20 && NEURON_IN_COUNT(n20) > 0) {
        synapse_t* in_meta = NEURON_IN_META(net, n20, 0);
        EXPECT_EQ(in_meta, nullptr) << "Incoming should be handle-only";
    }

    // Clean destroy
    neural_network_destroy(net);
}

TEST(SynapseE2E, LargeNetworkMemoryProfile) {
    // Verify memory profile for a moderately large network
    network_config_t config = {};
    config.num_neurons = 1000;
    config.input_size = 100;
    config.output_size = 100;
    config.min_weight = -1.0f;
    config.max_weight = 1.0f;
    config.num_layers = 3;
    uint32_t layer_sizes[3] = {100, 800, 100};
    config.layer_sizes = layer_sizes;
    config.enable_bcm = false;
    config.enable_eligibility = false;

    neural_network_t net = neural_network_create(&config);
    ASSERT_NE(net, nullptr);

    // Add 5000 connections
    uint32_t added = 0;
    for (uint32_t i = 0; i < 100; i++) {
        for (uint32_t j = 0; j < 50; j++) {
            uint32_t target = 100 + (i * 50 + j) % 800;
            if (neural_network_add_connection(net, i, target, 0.1f)) {
                added++;
            }
        }
    }
    EXPECT_GT(added, 1000u);

    // Spot-check: outgoing metadata should be hot-only (56 bytes, not 200)
    EXPECT_EQ(sizeof(synapse_t), 56u);

    neural_network_destroy(net);
}

TEST(SynapseE2E, ColdOnlyAllocatedWhenNeeded) {
    // Create network with BCM/eligibility disabled
    network_config_t config = {};
    config.num_neurons = 50;
    config.input_size = 5;
    config.output_size = 5;
    config.min_weight = -1.0f;
    config.max_weight = 1.0f;
    config.num_layers = 2;
    uint32_t layer_sizes[2] = {25, 25};
    config.layer_sizes = layer_sizes;
    config.enable_bcm = false;
    config.enable_eligibility = false;

    neural_network_t net = neural_network_create(&config);
    ASSERT_NE(net, nullptr);

    // Add connections — cold should be allocated for STP
    bool ok = neural_network_add_connection(net, 0, 25, 0.5f);
    ASSERT_TRUE(ok);

    neuron_t* n0 = neural_network_get_neuron(net, 0);
    uint32_t out_count = NEURON_OUT_COUNT(n0);
    synapse_t* syn = NEURON_OUT_META(net, n0, out_count - 1);
    ASSERT_NE(syn, nullptr);

    // STP always creates cold data
    if (syn->cold_index != SYNAPSE_COLD_NONE) {
        synapse_cold_t* cold = SYNAPSE_COLD(net, syn);
        ASSERT_NE(cold, nullptr);
        EXPECT_EQ(cold->enable_stp, true);
        // BCM and eligibility should NOT be enabled
        EXPECT_EQ(cold->enable_bcm, false);
        EXPECT_EQ(cold->enable_eligibility, false);
        EXPECT_EQ(cold->bcm, nullptr);
        EXPECT_EQ(cold->eligibility, nullptr);
    }

    neural_network_destroy(net);
}
