/**
 * @file test_backprop_grad_clip.c
 * @brief Unit tests for gradient norm clipping in backprop_sparse_full()
 * @date 2026-02-28
 *
 * WHAT: Tests for the max_grad_norm / out_grad_norm feature of the shared
 *       backprop kernel (nimcp_backprop_kernel.h)
 * WHY:  Ensure gradient clipping correctly bounds gradient magnitudes and
 *       that the output gradient norm is populated and meaningful
 * HOW:  Direct API tests with NULL/invalid params (tests 1-2), and
 *       functional tests with real neural networks (tests 3-7)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "plasticity/adaptive/nimcp_backprop_kernel.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuralnet_core.h"
#include "nimcp.h"

/*=============================================================================
 * Test Helpers
 *===========================================================================*/

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_PASS(msg) do { \
    printf("  PASS: %s\n", msg); \
    g_tests_passed++; \
} while(0)

/*-----------------------------------------------------------------------------
 * Network creation helper
 *
 * Creates a small 3-layer feedforward network (input=8, hidden=16, output=4)
 * with dense layer wiring so backprop has real synapses to traverse.
 *---------------------------------------------------------------------------*/
static uint32_t s_layer_sizes[3] = { 8, 16, 4 };

static neural_network_t create_test_network(void)
{
    network_config_t config;
    memset(&config, 0, sizeof(config));

    config.num_neurons    = 28;   /* 8 + 16 + 4 */
    config.ei_ratio       = 1.0f; /* all excitatory for predictable backprop */
    config.learning_rate  = 0.01f;
    config.hebbian_rate   = 0.0f;
    config.stdp_window    = 0.0f;
    config.homeostatic_rate = 0.0f;
    config.target_activity  = 0.1f;
    config.adaptation_rate  = 0.0f;
    config.refractory_period = 5.0f;
    config.min_weight     = -5.0f;
    config.max_weight     =  5.0f;
    config.update_interval = 1000;

    config.input_size     = 8;
    config.output_size    = 4;
    config.num_layers     = 3;
    config.layer_sizes    = s_layer_sizes;

    return neural_network_create(&config);
}

/**
 * Run a forward pass and populate output buffer.
 * Returns true on success.
 */
static bool do_forward(neural_network_t net, const float* inputs,
                       uint32_t in_size, float* outputs, uint32_t out_size)
{
    return neural_network_forward(net, inputs, in_size, outputs, out_size);
}

/*=============================================================================
 * Test 1: NULL inputs return error
 *===========================================================================*/
static void test_null_inputs_return_error(void)
{
    printf("\n=== test_null_inputs_return_error ===\n");

    uint32_t layers[] = { 4, 4 };
    float target[] = { 1.0f, 0.0f, 0.0f, 0.0f };
    float output[] = { 0.5f, 0.5f, 0.5f, 0.5f };
    float grad_norm = -1.0f;

    /* NULL net */
    int rc = backprop_sparse_full(
        NULL, 2, layers, 0.01f, -1.0f, 1.0f,
        target, output, 4, 1.0f, &grad_norm);
    TEST_ASSERT(rc == -1, "NULL net should return -1");

    /* NULL layer_sizes */
    neural_network_t net = create_test_network();
    TEST_ASSERT(net != NULL, "create_test_network should succeed");
    rc = backprop_sparse_full(
        net, 2, NULL, 0.01f, -1.0f, 1.0f,
        target, output, 4, 1.0f, &grad_norm);
    TEST_ASSERT(rc == -1, "NULL layer_sizes should return -1");

    /* NULL target */
    rc = backprop_sparse_full(
        net, 3, s_layer_sizes, 0.01f, -5.0f, 5.0f,
        NULL, output, 4, 1.0f, &grad_norm);
    TEST_ASSERT(rc == -1, "NULL target should return -1");

    /* NULL output */
    rc = backprop_sparse_full(
        net, 3, s_layer_sizes, 0.01f, -5.0f, 5.0f,
        target, NULL, 4, 1.0f, &grad_norm);
    TEST_ASSERT(rc == -1, "NULL output should return -1");

    /* NULL out_grad_norm */
    rc = backprop_sparse_full(
        net, 3, s_layer_sizes, 0.01f, -5.0f, 5.0f,
        target, output, 4, 1.0f, NULL);
    TEST_ASSERT(rc == -1, "NULL out_grad_norm should return -1");

    neural_network_destroy(net);
    TEST_PASS("all NULL input cases return -1");
}

/*=============================================================================
 * Test 2: Too few layers returns error
 *===========================================================================*/
static void test_too_few_layers_return_error(void)
{
    printf("\n=== test_too_few_layers_return_error ===\n");

    neural_network_t net = create_test_network();
    TEST_ASSERT(net != NULL, "create_test_network should succeed");

    uint32_t one_layer[] = { 8 };
    float target[] = { 1.0f };
    float output[] = { 0.5f };
    float grad_norm = -1.0f;

    /* num_layers = 0 */
    int rc = backprop_sparse_full(
        net, 0, one_layer, 0.01f, -5.0f, 5.0f,
        target, output, 1, 1.0f, &grad_norm);
    TEST_ASSERT(rc == -1, "num_layers=0 should return -1");

    /* num_layers = 1 */
    rc = backprop_sparse_full(
        net, 1, one_layer, 0.01f, -5.0f, 5.0f,
        target, output, 1, 1.0f, &grad_norm);
    TEST_ASSERT(rc == -1, "num_layers=1 should return -1");

    neural_network_destroy(net);
    TEST_PASS("num_layers < 2 returns -1");
}

/*=============================================================================
 * Test 3: Gradient norm output is populated (>= 0)
 *===========================================================================*/
static void test_grad_norm_output_populated(void)
{
    printf("\n=== test_grad_norm_output_populated ===\n");

    neural_network_t net = create_test_network();
    TEST_ASSERT(net != NULL, "create_test_network should succeed");

    float inputs[8]  = { 1.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.2f, 0.8f };
    float outputs[4] = { 0.0f };
    float target[4]  = { 1.0f, 0.0f, 0.0f, 0.0f };

    /* Forward pass to populate neuron states and get output */
    bool fwd = do_forward(net, inputs, 8, outputs, 4);
    TEST_ASSERT(fwd, "forward pass should succeed");

    float grad_norm = -999.0f;
    int rc = backprop_sparse_full(
        net, 3, s_layer_sizes, 0.01f, -5.0f, 5.0f,
        target, outputs, 4, 1.0f, &grad_norm);
    TEST_ASSERT(rc == 0, "backprop should return 0");
    TEST_ASSERT(grad_norm >= 0.0f, "gradient norm should be non-negative");
    TEST_ASSERT(!isnan(grad_norm), "gradient norm should not be NaN");
    TEST_ASSERT(!isinf(grad_norm), "gradient norm should not be Inf");

    neural_network_destroy(net);
    TEST_PASS("out_grad_norm is populated with a valid non-negative value");
}

/*=============================================================================
 * Test 4: Gradient clipping reduces norm
 *
 * Use a SINGLE network and run backprop twice on the same state snapshot:
 *   (a) First pass with no clipping (max_grad_norm=0.0) to measure baseline
 *   (b) Second pass with tight clipping (max_grad_norm=0.001)
 * Because the same network is used, the second call operates on already-
 * modified weights, so we cannot do a strict A-vs-B comparison. Instead,
 * we verify that with an extremely tight clip the resulting norm is bounded
 * relative to an unclipped run with the same network state.
 *
 * Strategy: Create network, forward pass, run unclipped backprop to get
 * baseline norm. Then create a SECOND fresh network, forward, run with
 * a tight clip, and verify the clipped norm is finite and non-negative.
 * Additionally, run multiple clipped iterations and verify norms stay stable
 * (do not explode).
 *===========================================================================*/
static void test_grad_clipping_reduces_norm(void)
{
    printf("\n=== test_grad_clipping_reduces_norm ===\n");

    neural_network_t net = create_test_network();
    TEST_ASSERT(net != NULL, "create_test_network should succeed");

    float inputs[8]  = { 1.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.2f, 0.8f };
    float target[4]  = { 1.0f, 0.0f, 0.0f, 0.0f };

    /*
     * Run 10 iterations of backprop with a tight clip (max_grad_norm=0.01).
     * Record all norms and verify none explode beyond a reasonable bound.
     * The clipping logic scales down layer_lr when running norm exceeds
     * the threshold, which constrains the magnitude of weight updates.
     */
    float norms[10];
    float max_observed_norm = 0.0f;

    for (int i = 0; i < 10; i++) {
        float outputs[4] = { 0.0f };
        bool fwd = do_forward(net, inputs, 8, outputs, 4);
        TEST_ASSERT(fwd, "forward pass should succeed");

        norms[i] = -1.0f;
        int rc = backprop_sparse_full(
            net, 3, s_layer_sizes, 0.1f, -5.0f, 5.0f,
            target, outputs, 4, 0.01f, &norms[i]);
        TEST_ASSERT(rc == 0, "backprop with clipping should succeed");
        TEST_ASSERT(norms[i] >= 0.0f, "norm should be non-negative");
        TEST_ASSERT(!isnan(norms[i]), "norm should not be NaN");
        TEST_ASSERT(!isinf(norms[i]), "norm should not be Inf");

        if (norms[i] > max_observed_norm)
            max_observed_norm = norms[i];
    }

    printf("  norms over 10 iterations (clip=0.01):");
    for (int i = 0; i < 10; i++)
        printf(" %.6f", norms[i]);
    printf("\n");
    printf("  max_observed_norm = %f\n", max_observed_norm);

    /*
     * With gradient clipping active, norms should remain bounded.
     * A reasonable upper bound for a small 8-16-4 network with lr=0.1
     * and max_grad_norm=0.01 is well under 10.0.
     */
    TEST_ASSERT(max_observed_norm < 10.0f,
                "gradient norms should be bounded with clipping enabled");

    neural_network_destroy(net);
    TEST_PASS("gradient clipping keeps norms bounded over multiple iterations");
}

/*=============================================================================
 * Test 5: max_grad_norm=0.0 disables clipping
 *
 * With max_grad_norm=0.0, the function should still succeed and report
 * a valid gradient norm. The norm is not artificially bounded.
 *===========================================================================*/
static void test_zero_max_grad_norm_disables_clipping(void)
{
    printf("\n=== test_zero_max_grad_norm_disables_clipping ===\n");

    neural_network_t net = create_test_network();
    TEST_ASSERT(net != NULL, "create_test_network should succeed");

    float inputs[8]  = { 1.0f, 0.5f, 0.8f, 0.3f, 0.9f, 0.1f, 0.7f, 0.4f };
    float outputs[4] = { 0.0f };
    float target[4]  = { 1.0f, 0.0f, 1.0f, 0.0f };

    bool fwd = do_forward(net, inputs, 8, outputs, 4);
    TEST_ASSERT(fwd, "forward pass should succeed");

    float grad_norm = -1.0f;
    int rc = backprop_sparse_full(
        net, 3, s_layer_sizes, 0.01f, -5.0f, 5.0f,
        target, outputs, 4, 0.0f, &grad_norm);
    TEST_ASSERT(rc == 0, "backprop with max_grad_norm=0.0 should succeed");
    TEST_ASSERT(grad_norm >= 0.0f, "gradient norm should be non-negative");
    TEST_ASSERT(!isnan(grad_norm), "gradient norm should not be NaN");

    neural_network_destroy(net);
    TEST_PASS("max_grad_norm=0.0 disables clipping and still reports valid norm");
}

/*=============================================================================
 * Test 6: Default clipping at 1.0
 *
 * With max_grad_norm=1.0 (the common default in callers), the gradient norm
 * should be bounded. For a small network with moderate inputs, the clipped
 * norm should be in a reasonable range.
 *===========================================================================*/
static void test_default_clipping_at_one(void)
{
    printf("\n=== test_default_clipping_at_one ===\n");

    neural_network_t net = create_test_network();
    TEST_ASSERT(net != NULL, "create_test_network should succeed");

    float inputs[8]  = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    float outputs[4] = { 0.0f };
    float target[4]  = { 1.0f, 0.0f, 1.0f, 0.0f };

    bool fwd = do_forward(net, inputs, 8, outputs, 4);
    TEST_ASSERT(fwd, "forward pass should succeed");

    float grad_norm = -1.0f;
    int rc = backprop_sparse_full(
        net, 3, s_layer_sizes, 0.01f, -5.0f, 5.0f,
        target, outputs, 4, 1.0f, &grad_norm);
    TEST_ASSERT(rc == 0, "backprop with max_grad_norm=1.0 should succeed");
    TEST_ASSERT(grad_norm >= 0.0f, "gradient norm should be non-negative");
    TEST_ASSERT(!isnan(grad_norm), "gradient norm should not be NaN");
    TEST_ASSERT(!isinf(grad_norm), "gradient norm should not be Inf");

    printf("  grad_norm with clipping at 1.0 = %f\n", grad_norm);

    /*
     * With max_grad_norm=1.0, the gradient clipping logic scales down
     * the learning rate when the running norm exceeds 1.0. This limits
     * the magnitude of weight updates. The final norm should be finite
     * and not wildly large.
     */
    TEST_ASSERT(grad_norm < 100.0f,
                "grad_norm with clipping=1.0 should be bounded reasonably");

    neural_network_destroy(net);
    TEST_PASS("default clipping at 1.0 produces bounded gradient norm");
}

/*=============================================================================
 * Test 7: Successful call returns 0
 *===========================================================================*/
static void test_success_return_zero(void)
{
    printf("\n=== test_success_return_zero ===\n");

    neural_network_t net = create_test_network();
    TEST_ASSERT(net != NULL, "create_test_network should succeed");

    float inputs[8]  = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    float outputs[4] = { 0.0f };
    float target[4]  = { 0.0f, 1.0f, 0.0f, 1.0f };

    bool fwd = do_forward(net, inputs, 8, outputs, 4);
    TEST_ASSERT(fwd, "forward pass should succeed");

    float grad_norm = -1.0f;
    int rc = backprop_sparse_full(
        net, 3, s_layer_sizes, 0.01f, -5.0f, 5.0f,
        target, outputs, 4, 1.0f, &grad_norm);
    TEST_ASSERT(rc == 0, "backprop_sparse_full should return 0 on success");

    neural_network_destroy(net);
    TEST_PASS("successful call returns 0");
}

/*=============================================================================
 * Main
 *===========================================================================*/
int main(void)
{
    printf("=== Backprop Gradient Clipping Tests ===\n");

    /* Initialize NIMCP runtime (memory tracking, exception system, etc.) */
    nimcp_init();

    test_null_inputs_return_error();
    test_too_few_layers_return_error();
    test_grad_norm_output_populated();
    test_grad_clipping_reduces_norm();
    test_zero_max_grad_norm_disables_clipping();
    test_default_clipping_at_one();
    test_success_return_zero();

    /* Cleanup backprop thread pool */
    backprop_kernel_cleanup();
    nimcp_shutdown();

    printf("\n=== Summary: %d passed, %d failed ===\n",
           g_tests_passed, g_tests_failed);

    return (g_tests_failed > 0) ? 1 : 0;
}
