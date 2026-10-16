/**
 * @file test_struct_consistency.cpp
 * @brief Struct consistency tests for neural_network_struct and neuron_t
 *
 * WHAT: Verify that struct sizes are non-zero and log them for documentation
 * WHY:  After consolidating 7+ duplicate neural_network_struct definitions into
 *       a single header (nimcp_neuralnet_internal.h), these tests serve as a
 *       canary: if struct layouts change, developers are reminded to do a full
 *       rebuild (make -j4) to avoid stale object file mismatches.
 * HOW:  Google Test with sizeof checks and field offset validation
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstddef>

/* nimcp_neuralnet_internal.h provides the single authoritative definition
 * of struct neural_network_struct, activation_strategy_table_t, and
 * activation_fn_t. It transitively includes nimcp_neuralnet.h for neuron_t. */
extern "C" {
#include "core/neuralnet/nimcp_neuralnet_internal.h"
}

//=============================================================================
// Size stability tests
//=============================================================================

TEST(StructConsistency, NeuralNetworkStructSizeNonZero) {
    struct neural_network_struct nn;
    (void)nn;  /* suppress unused warning */
    EXPECT_GT(sizeof(struct neural_network_struct), 0u);
    printf("sizeof(neural_network_struct) = %zu\n", sizeof(struct neural_network_struct));
}

TEST(StructConsistency, NeuronTSizeNonZero) {
    EXPECT_GT(sizeof(neuron_t), 0u);
    printf("sizeof(neuron_t) = %zu\n", sizeof(neuron_t));
}

TEST(StructConsistency, ActivationStrategyTableSizeIs8Slots) {
    /* The canonical activation strategy table has exactly 8 slots.
     * If this changes, all files using the struct must be rebuilt. */
    activation_strategy_table_t table;
    (void)table;
    EXPECT_EQ(sizeof(table.functions) / sizeof(table.functions[0]), 8u);
    printf("sizeof(activation_strategy_table_t) = %zu\n", sizeof(activation_strategy_table_t));
}

//=============================================================================
// Field offset validation — catch stale struct copies at compile time
//=============================================================================

TEST(StructConsistency, FieldOrderMatchesCanonical) {
    /* Verify that key fields are at expected offsets relative to each other.
     * If field order changes, these will fail and remind developers to rebuild. */
    EXPECT_LT(offsetof(struct neural_network_struct, neurons),
              offsetof(struct neural_network_struct, num_neurons));
    EXPECT_LT(offsetof(struct neural_network_struct, num_neurons),
              offsetof(struct neural_network_struct, capacity));
    EXPECT_LT(offsetof(struct neural_network_struct, capacity),
              offsetof(struct neural_network_struct, current_time));
    EXPECT_LT(offsetof(struct neural_network_struct, current_time),
              offsetof(struct neural_network_struct, config));
    EXPECT_LT(offsetof(struct neural_network_struct, config),
              offsetof(struct neural_network_struct, network_time));
    EXPECT_LT(offsetof(struct neural_network_struct, network_time),
              offsetof(struct neural_network_struct, global_activity));

    /* Bio-async fields come after activation_strategies */
    EXPECT_LT(offsetof(struct neural_network_struct, activation_strategies),
              offsetof(struct neural_network_struct, bio_ctx));

    /* Sparse synapse pools come after bio_async_enabled */
    EXPECT_LT(offsetof(struct neural_network_struct, bio_async_enabled),
              offsetof(struct neural_network_struct, synapse_handle_pool));

    /* Bulk allocation fields are at the end */
    EXPECT_LT(offsetof(struct neural_network_struct, synapse_metadata_pool),
              offsetof(struct neural_network_struct, spike_history_bulk));
    EXPECT_LT(offsetof(struct neural_network_struct, spike_history_bulk),
              offsetof(struct neural_network_struct, activity_history_bulk));
    EXPECT_LT(offsetof(struct neural_network_struct, activity_history_bulk),
              offsetof(struct neural_network_struct, bulk_neuron_count));
}

TEST(StructConsistency, AllExpectedFieldsExist) {
    /* Compile-time verification that all expected fields exist.
     * If any field is removed, this test will fail to compile. */
    struct neural_network_struct nn;
    (void)nn.neurons;
    (void)nn.num_neurons;
    (void)nn.capacity;
    (void)nn.current_time;
    (void)nn.config;
    (void)nn.network_time;
    (void)nn.global_activity;
    (void)nn.network_stability;
    (void)nn.learning_momentum;
    (void)nn.last_avg_weight;
    (void)nn.last_maintenance;
    (void)nn.activation_strategies;
    (void)nn.neuromodulator_system;
    (void)nn.global_state;
    (void)nn.global_state_size;
    (void)nn.glial_integration;
    (void)nn.axon_network;
    (void)nn.bio_ctx;
    (void)nn.bio_async_enabled;
    (void)nn.synapse_handle_pool;
    (void)nn.synapse_metadata_pool;
    (void)nn.spike_history_bulk;
    (void)nn.activity_history_bulk;
    (void)nn.bulk_neuron_count;

    SUCCEED();  /* If we got here, all fields exist */
}
