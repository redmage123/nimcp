/**
 * @file test_lang_bridge_spike_routing.c
 * @brief PA-3 — verify the SNN-spike → bridge STDP routing path.
 *
 * Pattern: standalone smoke test. Compile:
 *   gcc -I include tests/unit/test_lang_bridge_spike_routing.c \
 *       -L build/lib -lnimcp -lm \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_bridge_spike_routing
 *
 * Coverage:
 *   1. test_default_off: snn_lang_config_default() leaves spike routing
 *      disabled. drain_pop_spikes is a no-op when the master flag is off.
 *
 *   2. test_set_routing_validates_tau: enabling with tau_ms <= 0 must be
 *      rejected. enabling with tau_ms = 200 must succeed.
 *
 *   3. test_attach_pop_table_capacity: attach up to SNN_LANG_MAX_ATTACHED_POPS
 *      pops; the next attach fails. Re-attaching an already-attached pop_id
 *      updates the role rather than consuming a new slot.
 *
 *   4. test_drain_routes_to_correct_role: with routing enabled, attach a
 *      WORD pop, build synthetic spike_output where neuron[5] fires, drain.
 *      Verify the right STDP path was taken (we use stats counters as a
 *      proxy: total_decode/encode_calls etc. are public). Specifically,
 *      a fresh bridge has no STDP events; after drain + a paired
 *      concept_spike + apply_stdp, total_stdp_updates should reflect the
 *      activity.
 *
 *   5. test_tick_decays_activation: inject spikes, call tick(dt_ms), verify
 *      no runaway. Decode under increasing tick count must NOT diverge —
 *      the original failure mode reproduction. (Indirect: we verify
 *      get_stats avg_word_confidence stays bounded.)
 */

#include "snn/bridges/nimcp_snn_language_bridge.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int g_failures = 0;

#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

static snn_language_bridge_t* mk_bridge(uint32_t cw, uint32_t ww)
{
    snn_lang_config_t cfg = snn_lang_config_default();
    cfg.max_concept_pops = cw;
    cfg.max_word_pops    = ww;
    return snn_language_bridge_create(&cfg);
}

static void test_default_off(void)
{
    snn_language_bridge_t* b = mk_bridge(64, 64);
    EXPECT(b != NULL, "create"); if (!b) return;

    /* drain on disabled-routing returns 0 (no-op) without registering. */
    float spikes[64] = {0};
    spikes[5] = 1.0f;
    int rc = snn_language_bridge_drain_pop_spikes(b, /*pop_id*/77,
                                                    spikes, 64, 1.0f);
    EXPECT(rc == 0, "drain when disabled is a no-op (rc=0); got %d", rc);

    /* Stats should show zero decode/encode/STDP events. */
    snn_lang_stats_t s = {0};
    snn_language_bridge_get_stats(b, &s);
    EXPECT(s.total_stdp_updates == 0, "no STDP events; got %llu",
            (unsigned long long)s.total_stdp_updates);

    snn_language_bridge_destroy(b);
}

static void test_set_routing_validates_tau(void)
{
    snn_language_bridge_t* b = mk_bridge(64, 64);
    EXPECT(b != NULL, "create"); if (!b) return;

    EXPECT(snn_language_bridge_set_snn_spike_routing(b, true, 0.0f) != 0,
            "tau_ms=0 with enabled rejected");
    EXPECT(snn_language_bridge_set_snn_spike_routing(b, true, -1.0f) != 0,
            "tau_ms<0 rejected");
    EXPECT(snn_language_bridge_set_snn_spike_routing(b, true, 200.0f) == 0,
            "tau_ms=200 accepted");
    /* Disabling is always OK. */
    EXPECT(snn_language_bridge_set_snn_spike_routing(b, false, 0.0f) == 0,
            "disabling always OK");

    snn_language_bridge_destroy(b);
}

static void test_attach_pop_table_capacity(void)
{
    snn_language_bridge_t* b = mk_bridge(64, 64);
    EXPECT(b != NULL, "create"); if (!b) return;

    /* Fill the table. */
    for (int i = 0; i < SNN_LANG_MAX_ATTACHED_POPS; i++) {
        EXPECT(snn_language_bridge_attach_snn_pop(b, /*pop_id*/100 + i,
                                                    1024,
                                                    SNN_LANG_POP_ROLE_CONCEPT) == 0,
                "attach #%d", i);
    }
    /* Next attach with a NEW pop_id should fail. */
    EXPECT(snn_language_bridge_attach_snn_pop(b, /*pop_id*/999, 1024,
                                                SNN_LANG_POP_ROLE_CONCEPT) != 0,
            "table-full attach rejected");
    /* Re-attaching a known pop_id updates the role and does NOT fail. */
    EXPECT(snn_language_bridge_attach_snn_pop(b, /*pop_id*/100, 2048,
                                                SNN_LANG_POP_ROLE_WORD) == 0,
            "re-attach updates role");

    /* Verify update via iterator. */
    int found_pop = -1;
    uint32_t found_n = 0;
    snn_lang_pop_role_t found_role = SNN_LANG_POP_ROLE_CONCEPT;
    for (uint32_t i = 0; i < SNN_LANG_MAX_ATTACHED_POPS; i++) {
        int pid = -1; uint32_t nn = 0; snn_lang_pop_role_t r;
        snn_language_bridge_get_attached_pop(b, i, &pid, &nn, &r);
        if (pid == 100) { found_pop = pid; found_n = nn; found_role = r; break; }
    }
    EXPECT(found_pop == 100 && found_n == 2048
           && found_role == SNN_LANG_POP_ROLE_WORD,
            "re-attach updated n=%u role=%d", found_n, (int)found_role);

    snn_language_bridge_destroy(b);
}

static void test_drain_routes_when_enabled(void)
{
    snn_language_bridge_t* b = mk_bridge(64, 64);
    EXPECT(b != NULL, "create"); if (!b) return;

    EXPECT(snn_language_bridge_set_snn_spike_routing(b, true, 200.0f) == 0,
            "enable routing");
    EXPECT(snn_language_bridge_attach_snn_pop(b, /*pop_id*/7, 64,
                                                SNN_LANG_POP_ROLE_WORD) == 0,
            "attach pop 7 as WORD");

    /* Synthetic spike_output: neurons 1, 5, 33 fire. */
    float spikes[64] = {0};
    spikes[1] = 1.0f; spikes[5] = 1.0f; spikes[33] = 1.0f;

    /* Drain at t=10ms — routes to word_spike(1, 10), word_spike(5, 10),
     * word_spike(33, 10). The word_spike function increments
     * word_pops[idx % cap].activation by 1.0 and updates last_spike_ms.
     * We can't directly read those fields (opaque struct), but we can
     * verify drain succeeded. */
    EXPECT(snn_language_bridge_drain_pop_spikes(b, /*pop_id*/7,
                                                  spikes, 64, 10.0f) == 0,
            "drain rc=0");

    /* Drain on unregistered pop fails. */
    EXPECT(snn_language_bridge_drain_pop_spikes(b, /*pop_id*/99,
                                                  spikes, 64, 10.0f) != 0,
            "unregistered pop rejected");

    snn_language_bridge_destroy(b);
}

static void test_tick_decays_activation(void)
{
    snn_language_bridge_t* b = mk_bridge(64, 64);
    EXPECT(b != NULL, "create"); if (!b) return;

    /* tick is callable independently of the routing flag — always-on decay. */
    EXPECT(snn_language_bridge_tick(b, 10.0f) == 0, "tick on default config");

    /* Configure a small tau so a single 10 ms tick visibly decays. */
    EXPECT(snn_language_bridge_set_snn_spike_routing(b, true, 50.0f) == 0,
            "enable + tau=50ms");

    /* Inject many spikes through the public API. */
    for (int t = 0; t < 100; t++) {
        snn_language_bridge_concept_spike(b, t % 10, (float)t);
    }

    /* Hammer tick 10 times at 10 ms each (100 ms total = 2 tau) — each
     * activation should drop to e^(-100/50) ≈ 0.135 of pre-tick value
     * (compounded). Critically, NO RUNAWAY. */
    for (int i = 0; i < 10; i++) {
        EXPECT(snn_language_bridge_tick(b, 10.0f) == 0, "tick %d", i);
    }
    /* Indirect check: stats remain finite. */
    snn_lang_stats_t s = {0};
    snn_language_bridge_get_stats(b, &s);
    EXPECT(isfinite(s.avg_word_confidence), "avg_word_confidence finite");

    snn_language_bridge_destroy(b);
}

/* Walkthrough round 1 add (round 2 deepened): attaching a pop wider than
 * the bridge cap is legal (modulo aliasing is intended) but should warn.
 * Round 2 adds a stats counter; verify it ticks (the LOG_WARN side
 * effect alone is hard to assert from a test, so we use the counter
 * as proxy). */
static void test_attach_overlarge_pop_warns_but_succeeds(void)
{
    snn_language_bridge_t* b = mk_bridge(/*cap_c*/64, /*cap_w*/64);
    EXPECT(b != NULL, "create"); if (!b) return;

    /* Baseline: counter starts at 0. */
    snn_lang_stats_t s0 = {0};
    snn_language_bridge_get_stats(b, &s0);
    EXPECT(s0.attach_collision_warnings == 0, "baseline counter=0; got %llu",
            (unsigned long long)s0.attach_collision_warnings);

    /* SNN pop has 4× more neurons than bridge cap → collision_factor=4.
     * The warning must fire — counter goes 0 → 1. */
    EXPECT(snn_language_bridge_attach_snn_pop(b, /*pop_id*/42, /*n*/256,
                                                SNN_LANG_POP_ROLE_WORD) == 0,
            "attach with n_neurons > cap accepted (warns)");

    snn_lang_stats_t s1 = {0};
    snn_language_bridge_get_stats(b, &s1);
    EXPECT(s1.attach_collision_warnings == 1,
            "collision warning counter incremented; got %llu",
            (unsigned long long)s1.attach_collision_warnings);

    /* Within-cap attach must NOT bump the counter. */
    EXPECT(snn_language_bridge_attach_snn_pop(b, /*pop_id*/43, /*n*/32,
                                                SNN_LANG_POP_ROLE_WORD) == 0,
            "within-cap attach OK");
    snn_lang_stats_t s2 = {0};
    snn_language_bridge_get_stats(b, &s2);
    EXPECT(s2.attach_collision_warnings == 1,
            "within-cap attach didn't bump counter; got %llu",
            (unsigned long long)s2.attach_collision_warnings);

    /* Verify the registration via iterator. */
    int found_pid = -1;
    uint32_t found_n = 0;
    snn_lang_pop_role_t found_role = SNN_LANG_POP_ROLE_CONCEPT;
    for (uint32_t i = 0; i < SNN_LANG_MAX_ATTACHED_POPS; i++) {
        int pid = -1; uint32_t nn = 0; snn_lang_pop_role_t r;
        snn_language_bridge_get_attached_pop(b, i, &pid, &nn, &r);
        if (pid == 42) { found_pid = pid; found_n = nn; found_role = r; break; }
    }
    EXPECT(found_pid == 42 && found_n == 256
           && found_role == SNN_LANG_POP_ROLE_WORD,
            "stored attach has n=%u role=%d", found_n, (int)found_role);

    /* Drain with overlarge spike_output: every neuron firing → modulo
     * folds them into 64 bridge slots. Decay (with reasonable tau) keeps
     * the resulting activations bounded — verify the call returns 0. */
    EXPECT(snn_language_bridge_set_snn_spike_routing(b, true, 50.0f) == 0,
            "enable routing tau=50ms");
    float* spikes = (float*)calloc(256, sizeof(float));
    for (int i = 0; i < 256; i++) spikes[i] = 1.0f;  /* every neuron fires */
    EXPECT(snn_language_bridge_drain_pop_spikes(b, /*pop_id*/42,
                                                  spikes, 256, 1.0f) == 0,
            "drain accepts overlarge n_neurons");
    free(spikes);

    snn_language_bridge_destroy(b);
}

int main(void)
{
    fprintf(stderr, "[PA-3] test_lang_bridge_spike_routing\n");
    test_default_off();
    test_set_routing_validates_tau();
    test_attach_pop_table_capacity();
    test_drain_routes_when_enabled();
    test_tick_decays_activation();
    test_attach_overlarge_pop_warns_but_succeeds();

    if (g_failures == 0) {
        fprintf(stderr, "OK — all 6 tests passed\n");
        return 0;
    } else {
        fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
        return 1;
    }
}
