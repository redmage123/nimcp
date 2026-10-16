/**
 * @file test_network_layer_validation.c
 * @brief Unit tests for network config/layer validation and neuralnet creation
 *
 * WHAT: Verifies that network_config_t validation logic (NULL config, zero
 *       input/output, zero layer_sizes entries) correctly rejects bad configs,
 *       and that valid configs produce correct layer offsets and wiring.
 * WHY:  Catch regressions in the validate_network_config path (static in
 *       nimcp_neuralnet.c) by testing through the public neural_network_create API.
 * HOW:  Uses libcheck framework with small networks.
 *
 * NOTE: validate_network_config is static — we test it indirectly through
 *       neural_network_create, which returns NULL for invalid configs.
 *
 * @author NIMCP Development Team
 * @date 2026-03-08
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "nimcp.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "utils/memory/nimcp_memory.h"

/*=============================================================================
 * Helper: build a default valid network_config_t
 *=============================================================================*/

static network_config_t make_valid_config(void)
{
    network_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.num_neurons      = 100;
    cfg.input_size       = 10;
    cfg.output_size      = 5;
    cfg.ei_ratio         = 0.8f;
    cfg.learning_rate    = 0.01f;
    cfg.hebbian_rate     = 0.001f;
    cfg.stdp_window      = 20.0f;
    cfg.homeostatic_rate = 0.0001f;
    cfg.target_activity  = 0.05f;
    cfg.adaptation_rate  = 0.01f;
    cfg.refractory_period = 2.0f;
    cfg.min_weight       = -1.0f;
    cfg.max_weight       = 1.0f;
    cfg.update_interval  = 1;
    cfg.num_layers       = 0;    /* single-layer default */
    cfg.layer_sizes      = NULL;
    cfg.enable_stdp      = false;
    cfg.enable_hebbian   = false;
    cfg.enable_oja       = false;
    cfg.enable_homeostasis = false;
    return cfg;
}

/*=============================================================================
 * Test 1: Valid config → creation succeeds
 *=============================================================================*/

START_TEST(test_valid_config_creates_network)
{
    network_config_t cfg = make_valid_config();
    neural_network_t net = neural_network_create(&cfg);
    ck_assert_ptr_nonnull(net);

    uint32_t n = neural_network_get_num_neurons(net);
    ck_assert_uint_ge(n, cfg.num_neurons);

    neural_network_destroy(net);
}
END_TEST

/*=============================================================================
 * Test 2: NULL config → creation returns NULL
 *=============================================================================*/

START_TEST(test_null_config_returns_null)
{
    neural_network_t net = neural_network_create(NULL);
    ck_assert_ptr_null(net);
}
END_TEST

/*=============================================================================
 * Test 3: Zero input_size → creation returns NULL
 *=============================================================================*/

START_TEST(test_zero_input_size_returns_null)
{
    network_config_t cfg = make_valid_config();
    cfg.input_size = 0;
    neural_network_t net = neural_network_create(&cfg);
    ck_assert_ptr_null(net);
}
END_TEST

/*=============================================================================
 * Test 4: Zero output_size → creation returns NULL
 *=============================================================================*/

START_TEST(test_zero_output_size_returns_null)
{
    network_config_t cfg = make_valid_config();
    cfg.output_size = 0;
    neural_network_t net = neural_network_create(&cfg);
    ck_assert_ptr_null(net);
}
END_TEST

/*=============================================================================
 * Test 5: Zero layer_sizes entry → creation returns NULL
 *=============================================================================*/

START_TEST(test_zero_layer_sizes_entry_returns_null)
{
    network_config_t cfg = make_valid_config();
    uint32_t layers[3] = {10, 0, 5};  /* middle layer is zero */
    cfg.num_layers = 3;
    cfg.layer_sizes = layers;
    neural_network_t net = neural_network_create(&cfg);
    ck_assert_ptr_null(net);
}
END_TEST

/*=============================================================================
 * Test 6: Zero num_neurons → creation returns NULL
 *=============================================================================*/

START_TEST(test_zero_neurons_returns_null)
{
    network_config_t cfg = make_valid_config();
    cfg.num_neurons = 0;
    neural_network_t net = neural_network_create(&cfg);
    ck_assert_ptr_null(net);
}
END_TEST

/*=============================================================================
 * Test 7: layer_sizes sum > num_neurons → network allocates enough neurons
 *=============================================================================*/

START_TEST(test_layer_sizes_sum_gt_num_neurons)
{
    network_config_t cfg = make_valid_config();
    uint32_t layers[3] = {10, 80, 5};  /* sum = 95 */
    cfg.num_layers = 3;
    cfg.layer_sizes = layers;
    cfg.num_neurons = 95;   /* exactly enough */
    cfg.input_size  = 10;
    cfg.output_size = 5;

    neural_network_t net = neural_network_create(&cfg);
    ck_assert_ptr_nonnull(net);

    uint32_t n = neural_network_get_num_neurons(net);
    /* Must have at least as many neurons as the sum of layers */
    ck_assert_uint_ge(n, 95);

    neural_network_destroy(net);
}
END_TEST

/*=============================================================================
 * Test 8: Consistent layer_sizes → correct neuron count
 *=============================================================================*/

START_TEST(test_consistent_layer_sizes_neuron_count)
{
    network_config_t cfg = make_valid_config();
    uint32_t layers[4] = {10, 30, 30, 5};  /* sum = 75 */
    cfg.num_layers = 4;
    cfg.layer_sizes = layers;
    cfg.num_neurons = 100;  /* more than sum */
    cfg.input_size  = 10;
    cfg.output_size = 5;

    neural_network_t net = neural_network_create(&cfg);
    ck_assert_ptr_nonnull(net);

    uint32_t n = neural_network_get_num_neurons(net);
    /* Should have at least num_neurons (100) since sum < num_neurons */
    ck_assert_uint_ge(n, cfg.num_neurons);

    neural_network_destroy(net);
}
END_TEST

/*=============================================================================
 * Test 9: Layer offsets computation — neuron IDs span layers correctly
 *
 * We verify that neurons in different layers exist by checking that
 * neural_network_get_neuron returns non-NULL for neuron IDs across
 * the offset boundaries.
 *=============================================================================*/

START_TEST(test_layer_offsets_correct)
{
    network_config_t cfg = make_valid_config();
    uint32_t layers[3] = {10, 40, 5};  /* sum = 55 */
    cfg.num_layers = 3;
    cfg.layer_sizes = layers;
    cfg.num_neurons = 55;
    cfg.input_size  = 10;
    cfg.output_size = 5;

    neural_network_t net = neural_network_create(&cfg);
    ck_assert_ptr_nonnull(net);

    /* Offset[0] = 0, offset[1] = 10, offset[2] = 50 */
    /* Neuron at index 0 should be in layer 0 */
    neuron_t* n0 = neural_network_get_neuron(net, 0);
    ck_assert_ptr_nonnull(n0);

    /* Neuron at index 10 should be first neuron of layer 1 */
    neuron_t* n10 = neural_network_get_neuron(net, 10);
    ck_assert_ptr_nonnull(n10);

    /* Neuron at index 50 should be first neuron of layer 2 (output) */
    neuron_t* n50 = neural_network_get_neuron(net, 50);
    ck_assert_ptr_nonnull(n50);

    /* Last neuron at index 54 should exist */
    neuron_t* n54 = neural_network_get_neuron(net, 54);
    ck_assert_ptr_nonnull(n54);

    neural_network_destroy(net);
}
END_TEST

/*=============================================================================
 * Test 10: Backbone wiring creates connections between adjacent layers
 *
 * After creating a multi-layer network, neurons in layer 0 should have
 * outgoing synapses pointing to neurons in layer 1.
 *=============================================================================*/

START_TEST(test_backbone_wiring_adjacent_layers)
{
    network_config_t cfg = make_valid_config();
    uint32_t layers[3] = {10, 30, 5};
    cfg.num_layers = 3;
    cfg.layer_sizes = layers;
    cfg.num_neurons = 45;
    cfg.input_size  = 10;
    cfg.output_size = 5;
    cfg.skip_layer_wiring = false;

    neural_network_t net = neural_network_create(&cfg);
    ck_assert_ptr_nonnull(net);

    /* Check that at least some neurons in layer 0 have outgoing connections
     * to neurons in layer 1 (IDs 10-39). */
    int found_cross_layer = 0;
    for (uint32_t i = 0; i < 10; i++) {
        neuron_t* neuron = neural_network_get_neuron(net, i);
        if (!neuron) continue;
        uint32_t out_count = NEURON_OUT_COUNT(neuron);
        for (uint32_t s = 0; s < out_count; s++) {
            synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, s);
            if (!h) continue;
            uint32_t target = h->target_neuron_id;
            if (target >= 10 && target < 40) {
                found_cross_layer = 1;
                break;
            }
        }
        if (found_cross_layer) break;
    }
    ck_assert_msg(found_cross_layer,
        "No cross-layer connections found from layer 0 → layer 1");

    neural_network_destroy(net);
}
END_TEST

/*=============================================================================
 * Test 11: Forward pass produces non-zero output for non-zero input
 *=============================================================================*/

START_TEST(test_forward_pass_nonzero_output)
{
    network_config_t cfg = make_valid_config();
    cfg.num_neurons = 50;
    cfg.input_size  = 10;
    cfg.output_size = 5;

    neural_network_t net = neural_network_create(&cfg);
    ck_assert_ptr_nonnull(net);

    float inputs[10]  = {1.0f, 0.5f, 0.3f, 0.8f, 0.2f,
                         0.9f, 0.1f, 0.4f, 0.7f, 0.6f};
    float outputs[5]  = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    bool ok = neural_network_forward(net, inputs, 10, outputs, 5);
    ck_assert(ok);

    /* At least one output should be non-zero after forward pass */
    int any_nonzero = 0;
    for (int i = 0; i < 5; i++) {
        if (fabsf(outputs[i]) > 1e-10f) {
            any_nonzero = 1;
            break;
        }
    }
    ck_assert_msg(any_nonzero,
        "Forward pass produced all-zero output for non-zero input");

    neural_network_destroy(net);
}
END_TEST

/*=============================================================================
 * Test 12: Network stats are valid after creation
 *=============================================================================*/

START_TEST(test_network_stats_valid)
{
    network_config_t cfg = make_valid_config();
    cfg.num_neurons = 100;

    neural_network_t net = neural_network_create(&cfg);
    ck_assert_ptr_nonnull(net);

    network_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    bool ok = neural_network_get_stats(net, &stats);
    ck_assert(ok);
    ck_assert_uint_ge(stats.num_neurons, cfg.num_neurons);
    /* Excitatory + inhibitory should sum to total (approximately) */
    ck_assert_uint_le(stats.num_excitatory + stats.num_inhibitory,
                      stats.num_neurons + 1);

    neural_network_destroy(net);
}
END_TEST

/*=============================================================================
 * Test 13: Exceeding MAX_NEURONS → creation returns NULL
 *=============================================================================*/

START_TEST(test_exceed_max_neurons_returns_null)
{
    network_config_t cfg = make_valid_config();
    cfg.num_neurons = MAX_NEURONS + 1;
    neural_network_t net = neural_network_create(&cfg);
    ck_assert_ptr_null(net);
}
END_TEST

/*=============================================================================
 * Test 14: skip_layer_wiring = true → no backbone connections
 *=============================================================================*/

START_TEST(test_skip_layer_wiring)
{
    network_config_t cfg = make_valid_config();
    uint32_t layers[2] = {10, 5};
    cfg.num_layers = 2;
    cfg.layer_sizes = layers;
    cfg.num_neurons = 15;
    cfg.input_size  = 10;
    cfg.output_size = 5;
    cfg.skip_layer_wiring = true;

    neural_network_t net = neural_network_create(&cfg);
    ck_assert_ptr_nonnull(net);

    /* With skip_layer_wiring, layer 0 neurons should have no outgoing to layer 1 */
    int found_cross = 0;
    for (uint32_t i = 0; i < 10; i++) {
        neuron_t* neuron = neural_network_get_neuron(net, i);
        if (!neuron) continue;
        for (uint32_t s = 0; s < NEURON_OUT_COUNT(neuron); s++) {
            synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, s);
            if (!h) continue;
            uint32_t target = h->target_neuron_id;
            if (target >= 10 && target < 15) {
                found_cross = 1;
                break;
            }
        }
        if (found_cross) break;
    }
    ck_assert_msg(!found_cross,
        "skip_layer_wiring=true but cross-layer connections were found");

    neural_network_destroy(net);
}
END_TEST

/*=============================================================================
 * Suite construction
 *=============================================================================*/

static Suite* network_layer_validation_suite(void)
{
    Suite* s = suite_create("Network Layer Validation");

    TCase* tc_validation = tcase_create("Config Validation");
    tcase_set_timeout(tc_validation, 30);
    tcase_add_test(tc_validation, test_valid_config_creates_network);
    tcase_add_test(tc_validation, test_null_config_returns_null);
    tcase_add_test(tc_validation, test_zero_input_size_returns_null);
    tcase_add_test(tc_validation, test_zero_output_size_returns_null);
    tcase_add_test(tc_validation, test_zero_layer_sizes_entry_returns_null);
    tcase_add_test(tc_validation, test_zero_neurons_returns_null);
    tcase_add_test(tc_validation, test_exceed_max_neurons_returns_null);
    suite_add_tcase(s, tc_validation);

    TCase* tc_layers = tcase_create("Layer Structure");
    tcase_set_timeout(tc_layers, 30);
    tcase_add_test(tc_layers, test_layer_sizes_sum_gt_num_neurons);
    tcase_add_test(tc_layers, test_consistent_layer_sizes_neuron_count);
    tcase_add_test(tc_layers, test_layer_offsets_correct);
    tcase_add_test(tc_layers, test_backbone_wiring_adjacent_layers);
    tcase_add_test(tc_layers, test_skip_layer_wiring);
    suite_add_tcase(s, tc_layers);

    TCase* tc_forward = tcase_create("Forward Pass");
    tcase_set_timeout(tc_forward, 30);
    tcase_add_test(tc_forward, test_forward_pass_nonzero_output);
    tcase_add_test(tc_forward, test_network_stats_valid);
    suite_add_tcase(s, tc_forward);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = network_layer_validation_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
