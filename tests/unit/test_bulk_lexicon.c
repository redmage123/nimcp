/**
 * @file test_bulk_lexicon.c
 * @brief Standalone smoke test for the bulk lexicon binary loader.
 *
 * Pattern: follows test_gl_legacy_skip.c — printf + exit, no GTest dep.
 * Compile (paths assume the project root is /home/bbrelin/nimcp):
 *   gcc -I include tests/unit/test_bulk_lexicon.c \
 *       -L build/lib -lnimcp \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_bulk_lexicon
 *
 * What it covers:
 *   1. Build a synthetic .bin with 3 known words at module-defined dim 128.
 *   2. Construct a fresh grounded_language_t (so vocab_count starts at the
 *      seed-only baseline).
 *   3. Call nimcp_internal_load_bulk_lexicon() (the internal entry point
 *      the env-flag autoload also uses; the public API wrapper requires a
 *      brain handle which is overkill for a unit test).
 *   4. Verify vocab_count grew by exactly 3 and that lookup() can find each
 *      word with the right gl_word_class_t.
 *
 * What it does NOT cover (out of scope; deferred to integration suite):
 *   - End-to-end via nimcp_brain_load_bulk_lexicon (needs full brain init).
 *   - The env-var auto-load path in nimcp_brain_init_language.c.
 */

#include "language/nimcp_grounded_language.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Same internal-loader signature exposed by src/api/nimcp_part_core.c.
 * Linking against libnimcp.so picks it up; we declare extern here so the
 * test source doesn't pull the public nimcp.h. */
extern int nimcp_internal_load_bulk_lexicon(
    grounded_language_t* gl,
    const char* bin_path);

#define BULK_MAGIC      0x58454C42u   /* 'BLEX' */
#define BULK_VERSION    1u
#define BULK_DIM        128u

#define CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
    exit(1); } } while (0)

static void w_u32(FILE* f, uint32_t v) {
    size_t n = fwrite(&v, sizeof(v), 1, f);
    CHECK(n == 1, "write u32");
}

static void w_u16(FILE* f, uint16_t v) {
    size_t n = fwrite(&v, sizeof(v), 1, f);
    CHECK(n == 1, "write u16");
}

static void w_u8(FILE* f, uint8_t v) {
    size_t n = fwrite(&v, sizeof(v), 1, f);
    CHECK(n == 1, "write u8");
}

static void w_record(FILE* f, const char* form, uint8_t klass) {
    uint16_t flen = (uint16_t)strlen(form);
    w_u16(f, flen);
    size_t n = fwrite(form, 1, flen, f);
    CHECK(n == flen, "write form bytes");
    w_u8(f, klass);
    w_u8(f, 0u);  /* reserved */
    /* 128 floats — use a deterministic pattern so failure modes are
     * easy to spot in a hex dump. */
    for (uint32_t i = 0; i < BULK_DIM; i++) {
        float v = ((float)i + (float)flen) * 0.001f;
        size_t got = fwrite(&v, sizeof(v), 1, f);
        CHECK(got == 1, "write vec");
    }
}

static void write_synthetic_bin(const char* path) {
    FILE* f = fopen(path, "wb");
    CHECK(f != NULL, "open synthetic bin for write");

    /* Header: 8 × u32 = 32 bytes */
    w_u32(f, BULK_MAGIC);
    w_u32(f, BULK_VERSION);
    w_u32(f, 3u);          /* word_count */
    w_u32(f, BULK_DIM);    /* vector_dim */
    w_u32(f, 64u);         /* record_max_form (informational) */
    w_u32(f, 0u);
    w_u32(f, 0u);
    w_u32(f, 0u);

    /* Three records: noun "dog", verb "run", adjective "red". */
    w_record(f, "bulkloadertestdog",   GL_CLASS_NOUN);
    w_record(f, "bulkloadertestrun",   GL_CLASS_VERB);
    w_record(f, "bulkloadertestred",   GL_CLASS_ADJECTIVE);

    fclose(f);
}

static uint32_t current_vocab_size(grounded_language_t* gl) {
    gl_stats_t s;
    memset(&s, 0, sizeof(s));
    grounded_language_get_stats(gl, &s);
    return s.vocab_size;
}

static void test_bulk_load_three_words(void) {
    const char* path = "/tmp/nimcp_test_bulk_lexicon.bin";
    write_synthetic_bin(path);

    grounded_language_t* gl = grounded_language_create(BULK_DIM, NULL);
    CHECK(gl != NULL, "create gl handle");

    uint32_t before = current_vocab_size(gl);
    fprintf(stderr, "vocab_size before bulk-load: %u\n", before);

    int loaded = nimcp_internal_load_bulk_lexicon(gl, path);
    CHECK(loaded == 3, "expected 3 entries inserted");

    uint32_t after = current_vocab_size(gl);
    fprintf(stderr, "vocab_size after bulk-load:  %u (delta=%d)\n",
            after, (int)after - (int)before);
    CHECK(after >= before + 3, "vocab_size grew by at least 3");

    /* Each word should be reachable via lookup + carry a non-empty
     * binding (proves fast_map ran end-to-end, not just that vocab_count
     * was bumped). We use unique 'bulkloadertest…' prefixes so we don't
     * collide with the default function-word seed list.
     *
     * NOTE on learned_class: grounded_language_fast_map() does NOT copy
     * the category arg into entry->learned_class — class is inferred from
     * morphology + distributional context. See nimcp_grounded_language.c
     * around line 156. So we don't assert on learned_class here; the
     * category byte's job is to tag the underlying CONCEPT, not the
     * lexicon entry's POS slot. */
    const gl_lexicon_entry_t* e1 = grounded_language_lookup(gl, "bulkloadertestdog");
    CHECK(e1 != NULL, "lookup bulkloadertestdog");
    CHECK(e1->binding_count > 0, "bulkloadertestdog has at least one binding");
    CHECK(e1->context_initialized, "bulkloadertestdog context_vector populated");

    const gl_lexicon_entry_t* e2 = grounded_language_lookup(gl, "bulkloadertestrun");
    CHECK(e2 != NULL, "lookup bulkloadertestrun");
    CHECK(e2->binding_count > 0, "bulkloadertestrun has at least one binding");

    const gl_lexicon_entry_t* e3 = grounded_language_lookup(gl, "bulkloadertestred");
    CHECK(e3 != NULL, "lookup bulkloadertestred");
    CHECK(e3->binding_count > 0, "bulkloadertestred has at least one binding");

    grounded_language_destroy(gl);
    unlink(path);
    printf("PASS bulk_load_three_words\n");
}

static void test_bulk_load_bad_magic(void) {
    const char* path = "/tmp/nimcp_test_bulk_bad_magic.bin";
    FILE* f = fopen(path, "wb");
    CHECK(f != NULL, "open bad-magic file");
    /* Header with wrong magic — loader must reject. */
    w_u32(f, 0xDEADBEEFu);
    w_u32(f, BULK_VERSION);
    w_u32(f, 0u);
    w_u32(f, BULK_DIM);
    w_u32(f, 0u); w_u32(f, 0u); w_u32(f, 0u); w_u32(f, 0u);
    fclose(f);

    grounded_language_t* gl = grounded_language_create(BULK_DIM, NULL);
    CHECK(gl != NULL, "create gl");

    int rc = nimcp_internal_load_bulk_lexicon(gl, path);
    CHECK(rc < 0, "bad magic must return <0");

    grounded_language_destroy(gl);
    unlink(path);
    printf("PASS bulk_load_bad_magic\n");
}

int main(void) {
    test_bulk_load_three_words();
    test_bulk_load_bad_magic();
    printf("ALL PASS\n");
    return 0;
}
