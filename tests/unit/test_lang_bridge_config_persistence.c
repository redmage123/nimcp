/**
 * @file test_lang_bridge_config_persistence.c
 * @brief Tier 2 #8 — verify PA + MQ knobs round-trip through save/load
 *        and that the V3 reader stays compatible with V2 on-disk files.
 *
 * Pattern: standalone smoke test, no GTest dep. Compile:
 *   gcc -I include tests/unit/test_lang_bridge_config_persistence.c \
 *       -L build/lib -lnimcp -lm \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_bridge_config_persistence
 *
 * Coverage:
 *   1. test_v3_round_trip_all_knobs:
 *      Set all 11 PA/MQ knobs to non-default values on a freshly created
 *      bridge, save to /tmp file, destroy, load, verify every knob matches.
 *
 *   2. test_v2_forward_compat:
 *      Hand-write a V2-shaped file (no version sentinel, no ext block) using
 *      a freshly-created bridge's struct blob. Load it through the V3 reader
 *      and verify the persisted PA/MQ knobs all reset to library defaults
 *      (because V2 didn't carry them authoritatively).
 *
 *   3. test_v3_format_prefix_is_distinct_from_v2:
 *      Save a bridge, peek at the on-disk bytes immediately after the magic.
 *      The next u32 must be the V3 sentinel (0xFFFFFFFE) — not a small
 *      max_concept_pops value the way V2 had it. This pins the wire format
 *      so a future V2-only reader, given a V3 file, can refuse cleanly
 *      instead of misparsing.
 */

#include "snn/bridges/nimcp_snn_language_bridge.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

static int g_failures = 0;

#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

#define EXPECT_FLOAT_EQ(a, b, tol) do { \
    float _a = (a), _b = (b); \
    if (!(fabsf(_a - _b) <= (tol))) { \
        fprintf(stderr, "FAIL %s:%d %s ≈ %s : %.6f vs %.6f (tol %.6g)\n", \
                __func__, __LINE__, #a, #b, _a, _b, (double)(tol)); \
        g_failures++; \
    } \
} while (0)

/* Path that's per-pid to avoid collisions with parallel test runs. */
static void tmp_path(char* buf, size_t len, const char* tag)
{
    snprintf(buf, len, "/tmp/test_lang_bridge_persist_%d_%s.bin",
             (int)getpid(), tag);
}

/* Build a bridge with a small concept/word capacity; pop count stays 0
 * (we're not testing population persistence here, only config). */
static snn_language_bridge_t* small_bridge(void)
{
    snn_lang_config_t cfg = snn_lang_config_default();
    cfg.max_concept_pops = 8;
    cfg.max_word_pops    = 16;
    return snn_language_bridge_create(&cfg);
}

/* === Test 1: round-trip all 11 PA/MQ knobs through V3 save/load. === */
static void test_v3_round_trip_all_knobs(void)
{
    snn_language_bridge_t* b = small_bridge();
    EXPECT(b != NULL, "bridge create");
    if (!b) return;

    /* Set all 11 knobs to non-default values via their public setter APIs.
     * Defaults to beat: temperature=0, top_p=1.0, produce_topk=8,
     * glove_blend=0, intent_persistence=0, word_feedback=0.3,
     * enable_snn_spike_routing=false, activation_tau_ms=200,
     * use_hyperbolic_embeddings=false, sampling_mode=0. */
    EXPECT(snn_language_bridge_set_sampling(b, 0.7f, 0.85f) == 0,
            "set_sampling(T=0.7, top_p=0.85)");
    EXPECT(snn_language_bridge_set_glove_blend(b, 0.42f) == 0,
            "set_glove_blend(0.42)");
    EXPECT(snn_language_bridge_set_autoregressive(b, 0.55f, 0.66f) == 0,
            "set_autoregressive(0.55, 0.66)");
    EXPECT(snn_language_bridge_set_snn_spike_routing(b, true, 137.5f) == 0,
            "set_snn_spike_routing(true, 137.5)");
    EXPECT(snn_language_bridge_set_hyperbolic_embeddings(b, true) == 0,
            "set_hyperbolic_embeddings(true)");
    EXPECT(snn_language_bridge_set_sampling_mode(b, 2) == 0,
            "set_sampling_mode(2)");

    /* produce_topk has no public setter — poke it through the config struct
     * via a fresh bridge created from a custom config. We retire `b` and
     * rebuild with the additional override. */
    snn_lang_config_t custom = snn_lang_config_default();
    custom.max_concept_pops = 8;
    custom.max_word_pops    = 16;
    custom.temperature              = 0.7f;
    custom.top_p                    = 0.85f;
    custom.produce_topk             = 13; /* non-default */
    custom.glove_blend              = 0.42f;
    custom.intent_persistence       = 0.55f;
    custom.word_feedback            = 0.66f;
    custom.enable_snn_spike_routing = true;
    custom.activation_tau_ms        = 137.5f;
    custom.use_hyperbolic_embeddings = true;
    custom.sampling_mode            = 2;
    snn_language_bridge_destroy(b);

    b = snn_language_bridge_create(&custom);
    EXPECT(b != NULL, "rebuild bridge with custom cfg");
    if (!b) return;

    char path[256];
    tmp_path(path, sizeof(path), "v3_roundtrip");

    EXPECT(snn_language_bridge_save(b, path) == 0, "save returns 0");
    snn_language_bridge_destroy(b);

    snn_language_bridge_t* loaded = snn_language_bridge_load(path);
    EXPECT(loaded != NULL, "load returns non-NULL");
    if (!loaded) { unlink(path); return; }

    /* Verify all 11 knobs survived. We poke the inner config via the public
     * API by checking observable behavior — but that's fragile. Instead,
     * save the loaded bridge again and re-load to compare; or simpler,
     * read the config off the bridge's blend getter (only spike_blend is
     * exposed). The cleanest probe: call the setters' validation logic to
     * confirm the values are what we expect by re-running a no-op set
     * pattern. We rely on the config blob inside the bridge being correct
     * by saving & re-loading and asserting wire-stable state.
     *
     * Since the public API doesn't expose all knob getters, the safest
     * cross-check is: save → load → save → byte-compare both saves. If the
     * round-trip lost any knob, the second save would differ. */
    char path2[256];
    tmp_path(path2, sizeof(path2), "v3_roundtrip2");
    EXPECT(snn_language_bridge_save(loaded, path2) == 0, "second save returns 0");

    /* Read the post-magic+sentinel+version+config_blob+ext_block region
     * from both files and compare those bytes. They MUST be identical. */
    FILE* f1 = fopen(path, "rb");
    FILE* f2 = fopen(path2, "rb");
    EXPECT(f1 && f2, "open both files");
    if (f1 && f2) {
        /* Skip magic + sentinel + version. */
        fseek(f1, 3 * sizeof(uint32_t), SEEK_SET);
        fseek(f2, 3 * sizeof(uint32_t), SEEK_SET);

        /* Read config blob + ext block size + ext block from each. */
        size_t cfg_size = sizeof(snn_lang_config_t);
        unsigned char *buf1 = malloc(cfg_size);
        unsigned char *buf2 = malloc(cfg_size);
        EXPECT(fread(buf1, 1, cfg_size, f1) == cfg_size, "read cfg blob 1");
        EXPECT(fread(buf2, 1, cfg_size, f2) == cfg_size, "read cfg blob 2");
        EXPECT(memcmp(buf1, buf2, cfg_size) == 0,
                "config blobs identical across save→load→save");

        uint32_t bs1, bs2;
        EXPECT(fread(&bs1, sizeof(uint32_t), 1, f1) == 1, "read ext size 1");
        EXPECT(fread(&bs2, sizeof(uint32_t), 1, f2) == 1, "read ext size 2");
        EXPECT(bs1 == bs2, "ext block sizes match: %u vs %u", bs1, bs2);

        unsigned char *eb1 = malloc(bs1);
        unsigned char *eb2 = malloc(bs2);
        EXPECT(fread(eb1, 1, bs1, f1) == bs1, "read ext block 1");
        EXPECT(fread(eb2, 1, bs2, f2) == bs2, "read ext block 2");
        EXPECT(memcmp(eb1, eb2, bs1) == 0,
                "ext blocks identical across save→load→save");

        /* Decode the explicit ext block from f1 and verify each field. */
        const unsigned char *p = eb1;
        float t, tp, gb, ip, wf, atau;
        uint32_t ptopk;
        uint8_t sr, hyp;
        int32_t smode;
        memcpy(&t,     p, sizeof(float)); p += sizeof(float);
        memcpy(&tp,    p, sizeof(float)); p += sizeof(float);
        memcpy(&ptopk, p, sizeof(uint32_t)); p += sizeof(uint32_t);
        memcpy(&gb,    p, sizeof(float)); p += sizeof(float);
        memcpy(&ip,    p, sizeof(float)); p += sizeof(float);
        memcpy(&wf,    p, sizeof(float)); p += sizeof(float);
        memcpy(&sr,    p, sizeof(uint8_t)); p += sizeof(uint8_t);
        memcpy(&atau,  p, sizeof(float)); p += sizeof(float);
        memcpy(&hyp,   p, sizeof(uint8_t)); p += sizeof(uint8_t);
        memcpy(&smode, p, sizeof(int32_t)); p += sizeof(int32_t);

        EXPECT_FLOAT_EQ(t,    0.7f, 1e-6f);
        EXPECT_FLOAT_EQ(tp,   0.85f, 1e-6f);
        EXPECT(ptopk == 13u, "produce_topk=13 got %u", ptopk);
        EXPECT_FLOAT_EQ(gb,   0.42f, 1e-6f);
        EXPECT_FLOAT_EQ(ip,   0.55f, 1e-6f);
        EXPECT_FLOAT_EQ(wf,   0.66f, 1e-6f);
        EXPECT(sr == 1, "spike_routing=true got %u", sr);
        EXPECT_FLOAT_EQ(atau, 137.5f, 1e-6f);
        EXPECT(hyp == 1, "hyperbolic=true got %u", hyp);
        EXPECT(smode == 2, "sampling_mode=2 got %d", smode);

        free(buf1); free(buf2); free(eb1); free(eb2);
    }
    if (f1) fclose(f1);
    if (f2) fclose(f2);

    snn_language_bridge_destroy(loaded);
    unlink(path);
    unlink(path2);
}

/* === Test 2: V2 forward compat — V3 reader handles a legacy file. === */
static void test_v2_forward_compat(void)
{
    char path[256];
    tmp_path(path, sizeof(path), "v2_legacy");

    /* Hand-craft a V2 file: [magic][config blob][num_concepts=0][num_words=0]
     *                        [num_bindings=0]. No sentinel, no ext block. */
    snn_lang_config_t cfg = snn_lang_config_default();
    cfg.max_concept_pops = 8;
    cfg.max_word_pops    = 16;
    /* Stuff non-default values into the persisted-knob fields of the legacy
     * blob. After load through the V3 reader, these MUST be reset to library
     * defaults (V2 reader path doesn't trust the blob's knob slots). */
    cfg.temperature              = 0.99f;
    cfg.top_p                    = 0.11f;
    cfg.produce_topk             = 99;
    cfg.glove_blend              = 0.77f;
    cfg.intent_persistence       = 0.88f;
    cfg.word_feedback            = 0.99f;
    cfg.enable_snn_spike_routing = true;
    cfg.activation_tau_ms        = 9999.0f;
    cfg.use_hyperbolic_embeddings = true;
    cfg.sampling_mode            = 1;

    FILE* f = fopen(path, "wb");
    EXPECT(f != NULL, "open V2 tmp file");
    if (!f) return;

    uint32_t magic = SNN_LANG_MAGIC;
    fwrite(&magic, sizeof(uint32_t), 1, f);
    fwrite(&cfg, sizeof(snn_lang_config_t), 1, f);
    uint32_t zero = 0;
    fwrite(&zero, sizeof(uint32_t), 1, f); /* num_concept_pops = 0 */
    fwrite(&zero, sizeof(uint32_t), 1, f); /* num_word_pops = 0 */
    fwrite(&zero, sizeof(uint32_t), 1, f); /* num_bindings = 0 */
    fclose(f);

    /* Load through V3 reader. */
    snn_language_bridge_t* b = snn_language_bridge_load(path);
    EXPECT(b != NULL, "V3 reader loads V2 file");
    if (!b) { unlink(path); return; }

    /* The persisted knobs should now be at library defaults, not the
     * non-default values we stuffed into the V2 blob. Probe by saving
     * `b` again (V3 format) and decoding the explicit ext block. */
    char outpath[256];
    tmp_path(outpath, sizeof(outpath), "v2_resaved_as_v3");
    EXPECT(snn_language_bridge_save(b, outpath) == 0, "resave as V3");

    FILE* g = fopen(outpath, "rb");
    EXPECT(g != NULL, "open resaved file");
    if (g) {
        /* magic + sentinel + version + config blob + ext_block_size + block */
        fseek(g, 3 * sizeof(uint32_t) + sizeof(snn_lang_config_t), SEEK_SET);
        uint32_t bs;
        EXPECT(fread(&bs, sizeof(uint32_t), 1, g) == 1, "read ext size");

        float t, tp, gb, ip, wf, atau;
        uint32_t ptopk;
        uint8_t sr, hyp;
        int32_t smode;
        EXPECT(fread(&t, sizeof(float), 1, g) == 1, "read t");
        EXPECT(fread(&tp, sizeof(float), 1, g) == 1, "read tp");
        EXPECT(fread(&ptopk, sizeof(uint32_t), 1, g) == 1, "read ptopk");
        EXPECT(fread(&gb, sizeof(float), 1, g) == 1, "read gb");
        EXPECT(fread(&ip, sizeof(float), 1, g) == 1, "read ip");
        EXPECT(fread(&wf, sizeof(float), 1, g) == 1, "read wf");
        EXPECT(fread(&sr, sizeof(uint8_t), 1, g) == 1, "read sr");
        EXPECT(fread(&atau, sizeof(float), 1, g) == 1, "read atau");
        EXPECT(fread(&hyp, sizeof(uint8_t), 1, g) == 1, "read hyp");
        EXPECT(fread(&smode, sizeof(int32_t), 1, g) == 1, "read smode");

        snn_lang_config_t defaults = snn_lang_config_default();
        EXPECT_FLOAT_EQ(t,    defaults.temperature,        1e-6f);
        EXPECT_FLOAT_EQ(tp,   defaults.top_p,              1e-6f);
        EXPECT(ptopk == defaults.produce_topk,
               "produce_topk reset to default %u; got %u",
               defaults.produce_topk, ptopk);
        EXPECT_FLOAT_EQ(gb,   defaults.glove_blend,        1e-6f);
        EXPECT_FLOAT_EQ(ip,   defaults.intent_persistence, 1e-6f);
        EXPECT_FLOAT_EQ(wf,   defaults.word_feedback,      1e-6f);
        EXPECT(sr == (defaults.enable_snn_spike_routing ? 1 : 0),
               "spike_routing reset; got %u", sr);
        EXPECT_FLOAT_EQ(atau, defaults.activation_tau_ms,  1e-6f);
        EXPECT(hyp == (defaults.use_hyperbolic_embeddings ? 1 : 0),
               "hyperbolic reset; got %u", hyp);
        EXPECT(smode == defaults.sampling_mode,
               "sampling_mode reset; got %d", smode);
        fclose(g);
    }
    snn_language_bridge_destroy(b);
    unlink(path);
    unlink(outpath);
}

/* === Test 3: V3 wire format prefix is structurally distinct from V2. === */
static void test_v3_format_prefix_is_distinct_from_v2(void)
{
    snn_language_bridge_t* b = small_bridge();
    EXPECT(b != NULL, "bridge create");
    if (!b) return;

    char path[256];
    tmp_path(path, sizeof(path), "v3_prefix");
    EXPECT(snn_language_bridge_save(b, path) == 0, "save returns 0");

    FILE* f = fopen(path, "rb");
    EXPECT(f != NULL, "open saved file");
    if (f) {
        uint32_t magic = 0, sentinel = 0, version = 0;
        EXPECT(fread(&magic, sizeof(uint32_t), 1, f) == 1, "read magic");
        EXPECT(fread(&sentinel, sizeof(uint32_t), 1, f) == 1, "read sentinel");
        EXPECT(fread(&version, sizeof(uint32_t), 1, f) == 1, "read version");

        EXPECT(magic == SNN_LANG_MAGIC,
                "magic must be SLBG; got 0x%08x", magic);
        EXPECT(sentinel == SNN_LANG_BRIDGE_FILE_V3_SENTINEL,
                "sentinel 0x%08x != V3 sentinel 0xFFFFFFFE",
                sentinel);
        /* Save always writes the newest format. Accept V3 .. V5 here —
         * all share the V3 wire prefix; the test's intent is to pin the
         * layout, not the literal version number. */
        EXPECT(version == SNN_LANG_BRIDGE_FILE_VERSION_V3 ||
               version == SNN_LANG_BRIDGE_FILE_VERSION_V4 ||
               version == SNN_LANG_BRIDGE_FILE_VERSION_V5,
                "version %u not in {V3, V4, V5}", version);

        /* Distinct from any plausible V2 first-field (max_concept_pops),
         * which is bounded by SNN_LANG_MAX_CONCEPT_POPS. */
        EXPECT(sentinel > SNN_LANG_MAX_CONCEPT_POPS,
                "sentinel must not collide with valid V2 max_concept_pops");

        fclose(f);
    }

    snn_language_bridge_destroy(b);
    unlink(path);
}

/* === Test 4: V4 round-trip — EOS knobs survive save/load through ext block. */
static void test_v4_eos_round_trip(void)
{
    snn_lang_config_t custom = snn_lang_config_default();
    custom.max_concept_pops = 8;
    custom.max_word_pops    = 16;
    /* Set EOS knobs to non-default values via the public config struct. */
    custom.enable_eos_stopping = true;
    custom.eos_min_activation  = 0.123f;
    custom.eos_min_confidence  = 0.045f;

    snn_language_bridge_t* b = snn_language_bridge_create(&custom);
    EXPECT(b != NULL, "rebuild bridge with EOS-enabled cfg");
    if (!b) return;

    char path[256];
    tmp_path(path, sizeof(path), "v4_eos");
    EXPECT(snn_language_bridge_save(b, path) == 0, "save returns 0");
    snn_language_bridge_destroy(b);

    /* Verify the file declares V4. The wire layout is
     * [magic][sentinel][version][config blob][ext_size][ext...] */
    FILE* g = fopen(path, "rb");
    EXPECT(g != NULL, "open V4 file");
    if (g) {
        uint32_t magic = 0, sentinel = 0, version = 0;
        EXPECT(fread(&magic, sizeof(uint32_t), 1, g) == 1, "read magic");
        EXPECT(fread(&sentinel, sizeof(uint32_t), 1, g) == 1, "read sentinel");
        EXPECT(fread(&version, sizeof(uint32_t), 1, g) == 1, "read version");
        EXPECT(magic == SNN_LANG_MAGIC, "magic must be SLBG");
        EXPECT(sentinel == SNN_LANG_BRIDGE_FILE_V3_SENTINEL,
                "sentinel must be V3+ marker");
        EXPECT(version == SNN_LANG_BRIDGE_FILE_VERSION_V5,
                "version must be V5 = 5; got %u", version);

        /* Skip config blob, read ext_block_size, decode tail. */
        fseek(g, sizeof(snn_lang_config_t), SEEK_CUR);
        uint32_t bs;
        EXPECT(fread(&bs, sizeof(uint32_t), 1, g) == 1, "read ext size");
        /* Compute expected V5 ext block size — V4 tail (9 bytes) + V5
         * tail (1 + 4 + 4 = 9 bytes) on top of V3 prefix (43 bytes). */
        size_t v3_size = 4 + 4 + 4 + 4 + 4 + 4 + 1 + 4 + 1 + 4;
        size_t v4_size = v3_size + 1 + 4 + 4;
        size_t v5_size = v4_size + 1 + 4 + 4;
        EXPECT(bs == v5_size,
               "ext block size %u != V5 expected %zu", bs, v5_size);
        fclose(g);
    }

    /* Now reload and confirm EOS knobs round-tripped. */
    snn_language_bridge_t* loaded = snn_language_bridge_load(path);
    EXPECT(loaded != NULL, "V4 reload");
    if (loaded) {
        snn_lang_config_t got;
        EXPECT(snn_language_bridge_get_config(loaded, &got) == 0,
                "get_config after V4 load");
        EXPECT(got.enable_eos_stopping == true,
                "enable_eos_stopping survived V4 round trip");
        EXPECT_FLOAT_EQ(got.eos_min_activation, 0.123f, 1e-6f);
        EXPECT_FLOAT_EQ(got.eos_min_confidence, 0.045f, 1e-6f);
        snn_language_bridge_destroy(loaded);
    }

    unlink(path);
}

/* === Test 5: V3-format file (no EOS tail) loads under V4 reader — EOS knobs
 *             come back at library defaults rather than garbage. === */
static void test_v3_file_loads_with_eos_defaults(void)
{
    char path[256];
    tmp_path(path, sizeof(path), "v3_no_eos_tail");

    /* Hand-craft a V3-format file: magic + sentinel + version=V3 + full
     * config blob + V3-sized ext block (10 fields, no EOS tail) +
     * num_concept_pops=0 + num_word_pops=0 + num_bindings=0.
     *
     * The config struct itself has the EOS fields (this build's layout),
     * but we deliberately set them to non-default values to confirm that
     * the V4 reader's "raw blob is not authoritative for EOS" guard
     * actually resets them when the on-disk ext block stops at V3. */
    snn_lang_config_t cfg = snn_lang_config_default();
    cfg.max_concept_pops = 8;
    cfg.max_word_pops    = 16;
    cfg.enable_eos_stopping = true;     /* stale value in the raw blob */
    cfg.eos_min_activation  = 9.99f;    /* garbage */
    cfg.eos_min_confidence  = 9.99f;    /* garbage */

    FILE* f = fopen(path, "wb");
    EXPECT(f != NULL, "open V3 tmp file");
    if (!f) return;

    uint32_t magic    = SNN_LANG_MAGIC;
    uint32_t sentinel = SNN_LANG_BRIDGE_FILE_V3_SENTINEL;
    uint32_t version  = SNN_LANG_BRIDGE_FILE_VERSION_V3;
    fwrite(&magic,    sizeof(uint32_t), 1, f);
    fwrite(&sentinel, sizeof(uint32_t), 1, f);
    fwrite(&version,  sizeof(uint32_t), 1, f);
    fwrite(&cfg, sizeof(snn_lang_config_t), 1, f);

    /* V3-sized ext block — 10 fields, no EOS tail. */
    uint32_t ext_size = 4 + 4 + 4 + 4 + 4 + 4 + 1 + 4 + 1 + 4;  /* 34 bytes */
    fwrite(&ext_size, sizeof(uint32_t), 1, f);
    float fv = 0.0f; uint32_t uv = 0; uint8_t bv = 0; int32_t iv = 0;
    fv = cfg.temperature;        fwrite(&fv, sizeof(float),    1, f);
    fv = cfg.top_p;              fwrite(&fv, sizeof(float),    1, f);
    uv = cfg.produce_topk;       fwrite(&uv, sizeof(uint32_t), 1, f);
    fv = cfg.glove_blend;        fwrite(&fv, sizeof(float),    1, f);
    fv = cfg.intent_persistence; fwrite(&fv, sizeof(float),    1, f);
    fv = cfg.word_feedback;      fwrite(&fv, sizeof(float),    1, f);
    bv = cfg.enable_snn_spike_routing ? 1 : 0;
    fwrite(&bv, sizeof(uint8_t), 1, f);
    fv = cfg.activation_tau_ms;  fwrite(&fv, sizeof(float),    1, f);
    bv = cfg.use_hyperbolic_embeddings ? 1 : 0;
    fwrite(&bv, sizeof(uint8_t), 1, f);
    iv = cfg.sampling_mode;      fwrite(&iv, sizeof(int32_t),  1, f);

    uint32_t zero = 0;
    fwrite(&zero, sizeof(uint32_t), 1, f); /* num_concept_pops = 0 */
    fwrite(&zero, sizeof(uint32_t), 1, f); /* num_word_pops = 0 */
    fwrite(&zero, sizeof(uint32_t), 1, f); /* num_bindings = 0 */
    fclose(f);

    /* Load through current (V4-capable) reader. */
    snn_language_bridge_t* b = snn_language_bridge_load(path);
    EXPECT(b != NULL, "V4 reader loads V3 file");
    if (!b) { unlink(path); return; }

    snn_lang_config_t got;
    EXPECT(snn_language_bridge_get_config(b, &got) == 0,
            "get_config after V3 load");
    snn_lang_config_t defaults = snn_lang_config_default();
    /* EOS knobs MUST be at library defaults, NOT the garbage we
     * planted in the raw struct blob. */
    EXPECT(got.enable_eos_stopping == defaults.enable_eos_stopping,
            "V3 file: enable_eos_stopping reset to default; got %d",
            (int)got.enable_eos_stopping);
    EXPECT_FLOAT_EQ(got.eos_min_activation, defaults.eos_min_activation, 1e-6f);
    EXPECT_FLOAT_EQ(got.eos_min_confidence, defaults.eos_min_confidence, 1e-6f);

    snn_language_bridge_destroy(b);
    unlink(path);
}

int main(void)
{
    fprintf(stderr, "[Tier 2 #8] test_lang_bridge_config_persistence\n");
    test_v3_round_trip_all_knobs();
    test_v2_forward_compat();
    test_v3_format_prefix_is_distinct_from_v2();
    test_v4_eos_round_trip();
    test_v3_file_loads_with_eos_defaults();

    if (g_failures == 0) {
        fprintf(stderr, "OK — all 5 tests passed\n");
        return 0;
    } else {
        fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
        return 1;
    }
}
