/**
 * @file test_snn_forward_carry.c
 * @brief Brain decisions carry SNN state across calls (2026-11-02).
 *
 * snn_network_forward resets the network before every call, so a decision
 * could not depend on anything seen before it (the video probe measured zero
 * sensitivity to frame order). snn_network_forward_carry is the same forward
 * without the reset; brain decisions use it unless decide_carry_state is 0.
 *
 * Signatures (include/snn/nimcp_snn_network.h, include/snn/nimcp_snn.h):
 *   void           snn_config_default(snn_config_t* config);
 *   snn_network_t* snn_network_create(const snn_config_t* config);
 *   void           snn_network_destroy(snn_network_t* network);
 *   int snn_network_forward(snn_network_t*, const float* inputs, uint32_t n_inputs,
 *                           float* outputs, uint32_t n_outputs, float duration_ms);
 *   int snn_network_forward_carry(snn_network_t*, const float* inputs, uint32_t n_inputs,
 *                                 float* outputs, uint32_t n_outputs, float duration_ms);
 *   void  snn_tune_set_decide_carry_state(float v);  float snn_tune_get_decide_carry_state(void);
 *   void  snn_tune_set_decide_window_ms(float v);    float snn_tune_get_decide_window_ms(void);
 */
#include "nimcp.h"
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_training.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d " #cond " : ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); g_failures++; } } while (0)

#define NIN 16u
#define NOUT 16u

static float max_dev_from_rest(const snn_network_t* net) {
    const snn_population_t* pop = net->input_pop;
    if (!pop || !pop->membrane_v) return 0.0f;
    const float* v = (const float*)nimcp_tensor_data(pop->membrane_v);
    float m = 0.0f;
    for (uint32_t i = 0; i < pop->n_neurons; i++) {
        float d = fabsf(v[i] - net->config.v_rest);
        if (d > m) m = d;
    }
    return m;
}

int main(void) {
    if (!getenv("NIMCP_TEST_ALLOW_GPU")) setenv("CUDA_VISIBLE_DEVICES", "", 1);
    nimcp_init();
    snn_tune_set_noise_rate_hz(0.0f);

    /* Tunables: defaults and range guards */
    CHECK(snn_tune_get_decide_carry_state() == 1.0f, "carry on by default (%f)",
          snn_tune_get_decide_carry_state());
    CHECK(snn_tune_get_decide_window_ms() == 100.0f, "window 100 ms by default (%f)",
          snn_tune_get_decide_window_ms());
    snn_tune_set_decide_window_ms(0.5f);
    CHECK(snn_tune_get_decide_window_ms() == 100.0f, "window below 1 ms ignored");
    snn_tune_set_decide_window_ms(5000.0f);
    CHECK(snn_tune_get_decide_window_ms() == 100.0f, "window above 1000 ms ignored");
    snn_tune_set_decide_window_ms(25.0f);
    CHECK(snn_tune_get_decide_window_ms() == 25.0f, "window settable");
    snn_tune_set_decide_window_ms(100.0f);
    snn_tune_set_decide_carry_state(0.0f);
    CHECK(snn_tune_get_decide_carry_state() == 0.0f, "carry can be turned off");
    snn_tune_set_decide_carry_state(7.0f);
    CHECK(snn_tune_get_decide_carry_state() == 1.0f, "any nonzero turns carry on");

    snn_config_t config;
    snn_config_default(&config);
    config.n_inputs = NIN;
    config.n_outputs = NOUT;
    config.n_hidden = 0;
    snn_network_t* net = snn_network_create(&config);
    CHECK(net != NULL, "create network");
    if (!net) return 1;

    float in[NIN], zero[NIN] = {0}, out[NOUT];
    for (uint32_t i = 0; i < NIN; i++) in[i] = 1.0f;

    /* Carry: the simulation clock keeps running across calls */
    CHECK(snn_network_forward(net, in, NIN, out, NOUT, 10.0f) == SNN_SUCCESS, "forward (reset)");
    uint64_t t_reset = net->sim->current_time_us;
    CHECK(snn_network_forward_carry(net, in, NIN, out, NOUT, 10.0f) == SNN_SUCCESS, "forward_carry 1");
    uint64_t t_carry = net->sim->current_time_us;
    CHECK(t_carry > t_reset, "carried forward continues the clock (%llu -> %llu)",
          (unsigned long long)t_reset, (unsigned long long)t_carry);
    CHECK(snn_network_forward(net, in, NIN, out, NOUT, 10.0f) == SNN_SUCCESS, "forward (reset) again");
    CHECK(net->sim->current_time_us == t_reset, "plain forward restarts the clock (%llu vs %llu)",
          (unsigned long long)net->sim->current_time_us, (unsigned long long)t_reset);

    /* Carry: membrane state driven by the previous input survives into the
     * next call; a plain forward with no input starts from rest. */
    snn_network_forward_carry(net, in, NIN, out, NOUT, 10.0f);
    snn_network_forward_carry(net, zero, NIN, out, NOUT, 1.0f);
    float dev_carry = max_dev_from_rest(net);
    snn_network_forward(net, zero, NIN, out, NOUT, 1.0f);
    float dev_reset = max_dev_from_rest(net);
    CHECK(dev_carry > dev_reset, "previous input still felt after carry (%f) but not after reset (%f)",
          dev_carry, dev_reset);

    /* Argument guards */
    CHECK(snn_network_forward_carry(NULL, in, NIN, out, NOUT, 1.0f) != SNN_SUCCESS, "NULL network rejected");
    CHECK(snn_network_forward_carry(net, NULL, NIN, out, NOUT, 1.0f) != SNN_SUCCESS, "NULL inputs rejected");
    CHECK(snn_network_forward_carry(net, in, NIN, NULL, NOUT, 1.0f) != SNN_SUCCESS, "NULL outputs rejected");
    CHECK(snn_network_forward_carry(net, in, NIN - 1, out, NOUT, 1.0f) != SNN_SUCCESS, "input size mismatch rejected");

    snn_network_destroy(net);
    if (g_failures) { fprintf(stderr, "test_snn_forward_carry: %d failure(s)\n", g_failures); return 1; }
    printf("test_snn_forward_carry: all passed\n");
    return 0;
}
