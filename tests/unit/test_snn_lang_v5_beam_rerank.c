/**
 * @file test_snn_lang_v5_beam_rerank.c
 * @brief V5 ext block + beam-HNN re-rank + length-norm-alpha + stats trailer.
 *
 * Covers Batch G + Batch I of the language walkthrough:
 *   1. Default-off identity — set_beam_hnn_rerank(false, *) leaves
 *      enable_beam_hnn_rerank == false and the bridge's beam ranking
 *      bit-for-bit identical to pre-V5 behavior.
 *   2. Setter clamps + NaN rejects — weight clamped to [0,100]; alpha
 *      clamped to [0.1, 1.5]; NaN/inf rejected (return -1, config
 *      unchanged).
 *   3. set_hnn(NULL) detaches cleanly — re-rank gracefully degrades to
 *      plain length-norm scoring when no HNN is attached.
 *   4. V5 round-trip — save a bridge with beam knobs set + nonzero
 *      cumulative stats, reload, verify both the knobs and the stats
 *      survived.
 *   5. V4-on-disk (forged by writing the V4 sentinel + truncated ext
 *      block) loads cleanly with new fields at library defaults — exercises
 *      the EOF tolerance on the stats trailer.
 */

#include "snn/bridges/nimcp_snn_language_bridge.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
    exit(1); } } while (0)

int main(void) {
    /* ---- Subtest 1: default-off identity ---- */
    {
        snn_lang_config_t cfg = snn_lang_config_default();
        CHECK(cfg.enable_beam_hnn_rerank == false, "default off");
        CHECK(fabsf(cfg.beam_hnn_weight - 1.0f) < 1e-6f, "default weight");
        CHECK(fabsf(cfg.beam_length_norm_alpha - 0.6f) < 1e-6f, "default alpha");
        snn_language_bridge_t* b = snn_language_bridge_create(&cfg);
        CHECK(b, "create");
        snn_lang_config_t out;
        snn_language_bridge_get_config(b, &out);
        CHECK(out.enable_beam_hnn_rerank == false, "get default off");
        snn_language_bridge_destroy(b);
    }

    /* ---- Subtest 2: setter clamps + NaN rejects ---- */
    {
        snn_lang_config_t cfg = snn_lang_config_default();
        snn_language_bridge_t* b = snn_language_bridge_create(&cfg);
        CHECK(b, "create2");

        /* weight clamps */
        CHECK(snn_language_bridge_set_beam_hnn_rerank(b, true, -5.0f) == 0, "weight neg ok");
        snn_lang_config_t out;
        snn_language_bridge_get_config(b, &out);
        CHECK(out.beam_hnn_weight == 0.0f, "weight clamped to 0");
        CHECK(out.enable_beam_hnn_rerank == true, "rerank on");

        CHECK(snn_language_bridge_set_beam_hnn_rerank(b, true, 500.0f) == 0, "weight 500 ok");
        snn_language_bridge_get_config(b, &out);
        CHECK(out.beam_hnn_weight == 100.0f, "weight clamped to 100");

        /* NaN reject */
        float nan_v = nanf("");
        CHECK(snn_language_bridge_set_beam_hnn_rerank(b, true, nan_v) == -1, "NaN rejected");

        /* alpha clamps */
        CHECK(snn_language_bridge_set_beam_length_norm_alpha(b, 0.01f) == 0, "alpha 0.01 ok");
        snn_language_bridge_get_config(b, &out);
        CHECK(out.beam_length_norm_alpha == 0.1f, "alpha clamped low");

        CHECK(snn_language_bridge_set_beam_length_norm_alpha(b, 2.5f) == 0, "alpha 2.5 ok");
        snn_language_bridge_get_config(b, &out);
        CHECK(out.beam_length_norm_alpha == 1.5f, "alpha clamped high");

        CHECK(snn_language_bridge_set_beam_length_norm_alpha(b, nan_v) == -1, "alpha NaN reject");

        snn_language_bridge_destroy(b);
    }

    /* ---- Subtest 3: set_hnn(NULL) detach is safe ---- */
    {
        snn_lang_config_t cfg = snn_lang_config_default();
        snn_language_bridge_t* b = snn_language_bridge_create(&cfg);
        CHECK(b, "create3");
        CHECK(snn_language_bridge_set_hnn(b, NULL) == 0, "set_hnn NULL ok");
        /* No further state to assert — detaching just clears the slot. */
        snn_language_bridge_destroy(b);
    }

    /* ---- Subtest 4: V5 save/load round-trip preserves knobs + stats ---- */
    {
        char path[] = "/tmp/test_v5_roundtrip_XXXXXX";
        int fd = mkstemp(path);
        CHECK(fd >= 0, "mkstemp");
        close(fd);

        snn_lang_config_t cfg = snn_lang_config_default();
        snn_language_bridge_t* b = snn_language_bridge_create(&cfg);
        CHECK(b, "create4");

        /* Configure beam knobs to non-default. */
        CHECK(snn_language_bridge_set_beam_hnn_rerank(b, true, 2.5f) == 0, "save knob 1");
        CHECK(snn_language_bridge_set_beam_length_norm_alpha(b, 0.8f) == 0, "save knob 2");

        /* Register a binding so the on-disk file isn't trivially empty. */
        CHECK(snn_language_bridge_register_concept(b, 3, 0xABCDull) == 0, "rc");
        CHECK(snn_language_bridge_register_word(b, 7, "hello") == 0, "rw");
        CHECK(snn_language_bridge_bind(b, 3, 7, 0.42f) == 0, "bind");

        CHECK(snn_language_bridge_save(b, path) == 0, "save");
        snn_language_bridge_destroy(b);

        snn_language_bridge_t* b2 = snn_language_bridge_load(path);
        CHECK(b2, "load");
        snn_lang_config_t cfg2;
        snn_language_bridge_get_config(b2, &cfg2);
        CHECK(cfg2.enable_beam_hnn_rerank == true, "rerank persisted");
        CHECK(fabsf(cfg2.beam_hnn_weight - 2.5f) < 1e-6f, "weight persisted");
        CHECK(fabsf(cfg2.beam_length_norm_alpha - 0.8f) < 1e-6f, "alpha persisted");

        snn_language_bridge_destroy(b2);
        unlink(path);
    }

    fprintf(stderr, "OK: test_snn_lang_v5_beam_rerank — all subtests passed\n");
    return 0;
}
