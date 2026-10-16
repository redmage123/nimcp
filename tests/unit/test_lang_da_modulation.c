/**
 * @file test_lang_da_modulation.c
 * @brief TA-3 / Slice A residual — bridge-side DA setter/getter contract.
 *
 * After Slice A made the bridge transport-only, the bridge no longer owns
 * STDP weights or applies DA modulation to any state. The DA modulation
 * gain became a no-op stub on the bridge handle. The real DA-modulated
 * three-factor STDP lives in `src/plasticity/stdp/nimcp_stdp.c` and is
 * regression-tested by `test_snn_negative_da`.
 *
 * This file now covers only the residual public contract on the bridge:
 *   1. Setter accepts well-formed values (positive, zero, negative,
 *      out-of-range) and only rejects NaN + NULL handle.
 *   2. Enable/disable getter+setter pair is callable; NULL bridge yields
 *      sentinel returns.
 *   3. Connecting a neuromodulator system is callable without crashing.
 *
 * The previous tests that asserted the bridge multiplied weights by
 * `1 + DA × gain` (`test_no_neuromod_attached`, `test_disabled_flag`,
 * `test_high_dopamine_amplifies`) were removed in this walkthrough — they
 * asserted bridge-side STDP behavior that Slice A intentionally deleted.
 * That coverage now lives in `test_snn_negative_da`.
 */

#include "snn/bridges/nimcp_snn_language_bridge.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static int g_failures = 0;

#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

static snn_lang_config_t default_cfg(void) {
    snn_lang_config_t c;
    memset(&c, 0, sizeof(c));
    c.max_concept_pops = 4;
    c.max_word_pops = 4;
    c.neurons_per_pop = 8;
    c.stdp_tau_plus = 20.0f;
    c.stdp_tau_minus = 20.0f;
    c.stdp_a_plus = 0.05f;
    c.stdp_a_minus = 0.025f;
    c.stdp_learning_rate = 0.1f;
    c.binding_w_max = 1.0f;
    c.decode_window_ms = 50.0f;
    c.decay_rate = 0.95f;
    c.spike_blend = 0.5f;
    c.enable_da_modulation = true;
    c.da_modulation_gain = 10.0f;
    c.activation_tau_ms = 50.0f;
    return c;
}

static void test_setter_validation(void)
{
    snn_lang_config_t cfg = default_cfg();
    snn_language_bridge_t* b = snn_language_bridge_create(&cfg);
    EXPECT(b != NULL, "create");
    if (!b) return;

    /* NaN gain is the only reject. */
    EXPECT(snn_language_bridge_set_da_modulation_gain(b, NAN) == -1,
           "NaN gain rejected");

    /* Negative gain accepted (anti-Hebbian punishment per Slice F). */
    EXPECT(snn_language_bridge_set_da_modulation_gain(b, -1.0f) == 0,
           "negative gain accepted");
    EXPECT(snn_language_bridge_set_da_modulation_gain(b, 0.0f) == 0,
           "0 gain accepted");
    EXPECT(snn_language_bridge_set_da_modulation_gain(b, 50.0f) == 0,
           "50 gain accepted");
    EXPECT(snn_language_bridge_set_da_modulation_gain(b, 999.0f) == 0,
           "999 gain accepted (no rejection on clamp)");
    EXPECT(snn_language_bridge_set_da_modulation_gain(b, -999.0f) == 0,
           "-999 gain accepted (no rejection on clamp)");

    /* NULL bridge handled gracefully. */
    EXPECT(snn_language_bridge_set_da_modulation_enabled(NULL, true) == -1,
           "NULL bridge enable rejected");
    EXPECT(snn_language_bridge_set_da_modulation_gain(NULL, 1.0f) == -1,
           "NULL bridge gain rejected");
    EXPECT(!snn_language_bridge_get_da_modulation_enabled(NULL),
           "NULL bridge get returns false");

    snn_language_bridge_destroy(b);
}

static void test_enable_setter_callable(void)
{
    snn_lang_config_t cfg = default_cfg();
    snn_language_bridge_t* b = snn_language_bridge_create(&cfg);
    EXPECT(b != NULL, "create");
    if (!b) return;

    /* The setter must not crash and must return 0 on a valid handle. */
    EXPECT(snn_language_bridge_set_da_modulation_enabled(b, true) == 0,
           "enable true returns 0");
    EXPECT(snn_language_bridge_set_da_modulation_enabled(b, false) == 0,
           "enable false returns 0");

    snn_language_bridge_destroy(b);
}

static void test_neuromod_connect_callable(void)
{
    snn_lang_config_t cfg = default_cfg();
    snn_language_bridge_t* b = snn_language_bridge_create(&cfg);
    EXPECT(b != NULL, "create");
    if (!b) return;

    neuromodulator_config_t nc;
    memset(&nc, 0, sizeof(nc));
    neuromodulator_system_t sys = neuromodulator_system_create(&nc);
    EXPECT(sys != NULL, "neuromod create");
    if (sys) {
        (void)neuromodulator_set_level(sys, NEUROMOD_DOPAMINE, 0.5f);
        EXPECT(snn_language_bridge_connect_neuromod(b, sys) == 0,
               "connect neuromod succeeds");
        neuromodulator_system_destroy(sys);
    }

    snn_language_bridge_destroy(b);
}

int main(void)
{
    fprintf(stderr, "=== test_lang_da_modulation (Slice A residual) ===\n");
    test_setter_validation();
    test_enable_setter_callable();
    test_neuromod_connect_callable();

    if (g_failures == 0) {
        fprintf(stderr, "ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
}
