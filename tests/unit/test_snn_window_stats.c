/**
 * @file test_snn_window_stats.c
 * @brief SNN per-neuron rate statistics are per run window (2026-11-01).
 *
 * spike_trains[n].total_spikes accumulates over the population's lifetime;
 * snn_network_update_stats divided it by the LAST run's duration, so the
 * per-neuron rate, max rate and hyperactive count grew with every run
 * (675k neurons flagged ">200 Hz" on the production pod). With window
 * deltas, repeated identical runs must give stable statistics.
 *
 * Signatures (include/snn/nimcp_snn_network.h, nimcp_snn_config.h):
 *   int snn_config_default(snn_config_t* config);
 *   snn_network_t* snn_network_create(const snn_config_t* config);
 *   int snn_network_add_population(snn_network_t*, uint32_t n_neurons,
 *                                  neuron_type_t neuron_type, const char* name);
 *   int snn_network_set_inputs(snn_network_t*, const float* inputs, uint32_t n_inputs);
 *   int snn_network_run(snn_network_t*, float duration_ms);
 *   int snn_network_get_stats(snn_network_t*, snn_stats_t* stats);
 *   void snn_network_destroy(snn_network_t*);
 */
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

#include <stdio.h>
#include <string.h>

static int g_failures = 0;
#define EXPECT(cond, ...) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); g_failures++; } } while (0)

int main(void) {
    snn_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    EXPECT(snn_config_default(&cfg) == 0, "default config");
    cfg.n_inputs = 64; cfg.n_outputs = 64; cfg.n_hidden = 0;
    snn_network_t* net = snn_network_create(&cfg);
    EXPECT(net != NULL, "create");
    if (!net) return 1;
    int pid = snn_network_add_population(net, 64, NEURON_GENERIC_LIF, "test_pop");
    EXPECT(pid >= 0, "add population (%d)", pid);

    float in[256];
    for (int i = 0; i < 256; i++) in[i] = 5.0f;   /* strong drive -> steady firing */

    snn_stats_t st;
    float max_rate_first = -1.0f, max_rate_last = -1.0f;
    uint32_t hyper_first = 0, hyper_last = 0;
    long total = 0;
    for (int run = 0; run < 40; run++) {
        snn_network_set_inputs(net, in, 64);
        int s = snn_network_run(net, 20.0f);
        if (s > 0) total += s;
        memset(&st, 0, sizeof(st));
        snn_network_get_stats(net, &st);
        if (run == 2)  { max_rate_first = st.max_firing_rate; hyper_first = st.hyperactive_neurons; }
        if (run == 39) { max_rate_last = st.max_firing_rate;  hyper_last = st.hyperactive_neurons; }
    }
    printf("  spikes over 40 runs: %ld | max rate run3 %.1f Hz, run40 %.1f Hz | hyperactive %u -> %u\n",
           total, max_rate_first, max_rate_last, hyper_first, hyper_last);
    EXPECT(total > 0, "network fired (otherwise the test proves nothing)");
    /* Identical runs: the per-window max rate must not grow ~linearly with
     * the number of runs (old behaviour: run40 ~ 13x run3). */
    EXPECT(max_rate_last <= max_rate_first * 2.0f + 1.0f,
           "max rate stable across runs (%.1f -> %.1f)", max_rate_first, max_rate_last);
    EXPECT(hyper_last <= hyper_first + 64 / 4,
           "hyperactive count does not accumulate (%u -> %u)", hyper_first, hyper_last);
    /* A 64-neuron population can never report more than 64 hyperactive. */
    EXPECT(hyper_last <= 64, "hyperactive bounded by population (%u)", hyper_last);

    snn_network_destroy(net);
    if (g_failures) { fprintf(stderr, "test_snn_window_stats: %d failure(s)\n", g_failures); return 1; }
    printf("test_snn_window_stats: all passed\n");
    return 0;
}
