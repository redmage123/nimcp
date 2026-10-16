/**
 * @file nimcp_grounded_language_persistence.c
 * @brief Sidecar persistence for the grounded_language module.
 * @date 2026-05-04
 *
 * File format (little-endian, packed):
 *
 *   magic[8]     = "NIMCP_GL"
 *   version      = uint32_t (currently 2; v1 still readable)
 *   reserved     = uint32_t (zero — pad / future flags)
 *   semantic_dim = uint32_t   (must match destination handle's dim on load)
 *   vocab_count  = uint32_t
 *   [v1 only] template_count = uint32_t
 *
 *   gl_stats_t (field-by-field):
 *     stats.vocab_size                   : uint32_t
 *     stats.total_bindings               : uint32_t
 *     stats.total_groundings             : uint32_t
 *     stats.total_comprehensions         : uint32_t
 *     stats.total_productions            : uint32_t
 *     [v1 only] stats.templates_learned  : uint32_t
 *     stats.avg_binding_strength         : float
 *     stats.avg_comprehension_confidence : float
 *     stats.vocabulary_growth_rate       : float
 *
 *   rng_state    : uint64_t
 *
 *   For each lexicon entry:
 *     form_len           : uint16_t
 *     form               : char[form_len]      (no NUL, lowercase)
 *     form_hash          : uint32_t
 *     binding_count      : uint32_t
 *     For each binding:
 *       concept_id           : uint64_t
 *       strength             : float
 *       modality_strength[GL_MODALITY_COUNT] : float[]
 *       exposure_count       : uint32_t
 *       last_activation_ms   : uint64_t
 *       confidence           : float
 *     frequency          : uint32_t
 *     learned_class      : uint32_t  (gl_word_class_t cast)
 *     class_confidence   : float
 *     context_initialized: uint8_t
 *     If context_initialized:
 *       context_vector   : float[semantic_dim]
 *     valence            : float
 *     arousal            : float
 *
 *   [v1 only] For each template:
 *     type        : uint32_t
 *     slot_count  : uint32_t
 *     slots       : uint32_t[slot_count]
 *     frequency   : float
 *     confidence  : float
 *
 * v2 dropped the template machinery; grammar is now learned emergently
 * via SNN bridge plasticity. v2 readers skip the legacy template block
 * when loading v1 files.
 *
 * Defensive load: any short read, oversized record, or bad enum value
 * causes the load to abort with -1. The handle is left in whatever state
 * was reached so far — caller can choose to destroy + recreate.
 *
 * NOT serialized: semantic_memory pointer (managed elsewhere),
 * visual_ctx/auditory_ctx/speech_ctx/column_pool/emotional_ctx (runtime
 * wiring redone at brain init), snn_bridge (runtime).
 */

#include "language/nimcp_grounded_language_persistence.h"
#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_internal.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define LOG_MODULE "GL_PERSIST"

static const char GL_SIDECAR_MAGIC[8] = {
    'N', 'I', 'M', 'C', 'P', '_', 'G', 'L'
};
/* v1: original layout with templates_learned in stats + per-template records.
 * v2: templates removed — grammar is now learned emergently via SNN bridge.
 * v3: appended compositional phrase table (bigram/trigram surface forms +
 *     frequency) so the produce-path bigram reranker survives restarts.
 *     v1/v2 files are still readable — phrase_count stays 0 and phrases get
 *     re-learned from text. */
#define GL_SIDECAR_VERSION 3u

/* D5 — hash-fn version byte. Bump this whenever gl_hash_form / hash_word
 * changes. Saved in the low byte of the `reserved` u32 in the v2 header.
 * v1 files have reserved=0 which we read as legacy hash version 0.
 * Live runtime version starts at 1 (current FNV-1a mix). */
#define GL_HASH_FN_VERSION 1u

/* Sanity caps to defend against malformed / hostile input. The handle
 * already enforces GL_MAX_VOCAB at insert time, but we want to bail
 * before allocating gigabytes from a corrupt count field. */
#define GL_PERSIST_MAX_BINDINGS_PER_WORD  4096u
/* Legacy v1 template-record cap. Kept only for the v1 read path's
 * defensive bounds check while skipping the legacy block. */
#define GL_PERSIST_LEGACY_MAX_SLOTS       8u

/* ============================================================ I/O helpers */

static int write_u8(FILE* f, uint8_t v) {
    return fwrite(&v, 1, 1, f) == 1 ? 0 : -1;
}
static int write_u16(FILE* f, uint16_t v) {
    return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}
static int write_u32(FILE* f, uint32_t v) {
    return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}
static int write_u64(FILE* f, uint64_t v) {
    return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}
static int write_f32(FILE* f, float v) {
    return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}
static int write_bytes(FILE* f, const void* buf, size_t n) {
    if (n == 0) return 0;
    return fwrite(buf, 1, n, f) == n ? 0 : -1;
}
/* Length-prefixed string (uint16 + bytes, no NUL). Caps at maxlen. */
static int write_str(FILE* f, const char* s, size_t maxlen) {
    size_t n = s ? strnlen(s, maxlen) : 0;
    if (n > UINT16_MAX) n = UINT16_MAX;
    if (write_u16(f, (uint16_t)n) != 0) return -1;
    return write_bytes(f, s, n);
}
/* Float array. */
static int write_floats(FILE* f, const float* arr, size_t n) {
    if (n == 0) return 0;
    return fwrite(arr, sizeof(float), n, f) == n ? 0 : -1;
}

static int read_u8(FILE* f, uint8_t* v) {
    return fread(v, 1, 1, f) == 1 ? 0 : -1;
}
static int read_u16(FILE* f, uint16_t* v) {
    return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}
static int read_u32(FILE* f, uint32_t* v) {
    return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}
static int read_u64(FILE* f, uint64_t* v) {
    return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}
static int read_f32(FILE* f, float* v) {
    return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}
/* Read length-prefixed string. Truncates gracefully if the on-disk length
 * exceeds out_size; remaining bytes are skipped via fseek. */
static int read_str(FILE* f, char* out, size_t out_size) {
    uint16_t n = 0;
    if (read_u16(f, &n) != 0) return -1;
    if ((size_t)n >= out_size) {
        size_t to_keep = out_size - 1;
        if (fread(out, 1, to_keep, f) != to_keep) return -1;
        if (fseek(f, (long)(n - to_keep), SEEK_CUR) != 0) return -1;
        out[to_keep] = '\0';
        return 0;
    }
    if (n > 0 && fread(out, 1, n, f) != n) return -1;
    out[n] = '\0';
    return 0;
}
static int read_floats(FILE* f, float* arr, size_t n) {
    if (n == 0) return 0;
    return fread(arr, sizeof(float), n, f) == n ? 0 : -1;
}
/* Fixed-length raw read — symmetric with write_bytes(). Unlike read_str
 * there is no length prefix; the caller knows the exact byte count. */
static int read_bytes(FILE* f, void* buf, size_t n) {
    if (n == 0) return 0;
    return fread(buf, 1, n, f) == n ? 0 : -1;
}

/* ============================================================ stats helpers */

/* v2 stats layout — templates_learned slot removed. */
static int write_stats(FILE* f, const gl_stats_t* s) {
    if (write_u32(f, s->vocab_size) != 0) return -1;
    if (write_u32(f, s->total_bindings) != 0) return -1;
    if (write_u32(f, s->total_groundings) != 0) return -1;
    if (write_u32(f, s->total_comprehensions) != 0) return -1;
    if (write_u32(f, s->total_productions) != 0) return -1;
    if (write_f32(f, s->avg_binding_strength) != 0) return -1;
    if (write_f32(f, s->avg_comprehension_confidence) != 0) return -1;
    if (write_f32(f, s->vocabulary_growth_rate) != 0) return -1;
    return 0;
}

static int read_stats_v2(FILE* f, gl_stats_t* s) {
    if (read_u32(f, &s->vocab_size) != 0) return -1;
    if (read_u32(f, &s->total_bindings) != 0) return -1;
    if (read_u32(f, &s->total_groundings) != 0) return -1;
    if (read_u32(f, &s->total_comprehensions) != 0) return -1;
    if (read_u32(f, &s->total_productions) != 0) return -1;
    if (read_f32(f, &s->avg_binding_strength) != 0) return -1;
    if (read_f32(f, &s->avg_comprehension_confidence) != 0) return -1;
    if (read_f32(f, &s->vocabulary_growth_rate) != 0) return -1;
    return 0;
}

/* v1 stats: same fields plus templates_learned at slot 6. Read-and-drop
 * the legacy slot so subsequent records align. */
static int read_stats_v1(FILE* f, gl_stats_t* s) {
    uint32_t legacy_templates_learned = 0;
    if (read_u32(f, &s->vocab_size) != 0) return -1;
    if (read_u32(f, &s->total_bindings) != 0) return -1;
    if (read_u32(f, &s->total_groundings) != 0) return -1;
    if (read_u32(f, &s->total_comprehensions) != 0) return -1;
    if (read_u32(f, &s->total_productions) != 0) return -1;
    if (read_u32(f, &legacy_templates_learned) != 0) return -1;
    (void)legacy_templates_learned;
    if (read_f32(f, &s->avg_binding_strength) != 0) return -1;
    if (read_f32(f, &s->avg_comprehension_confidence) != 0) return -1;
    if (read_f32(f, &s->vocabulary_growth_rate) != 0) return -1;
    return 0;
}

/* ============================================================ binding helpers */

static int write_binding(FILE* f, const gl_word_binding_t* b) {
    if (write_u64(f, b->concept_id) != 0) return -1;
    if (write_f32(f, b->strength) != 0) return -1;
    if (write_floats(f, b->modality_strength, GL_MODALITY_COUNT) != 0) return -1;
    if (write_u32(f, b->exposure_count) != 0) return -1;
    if (write_u64(f, b->last_activation_ms) != 0) return -1;
    if (write_f32(f, b->confidence) != 0) return -1;
    return 0;
}

static int read_binding(FILE* f, gl_word_binding_t* b) {
    if (read_u64(f, &b->concept_id) != 0) return -1;
    if (read_f32(f, &b->strength) != 0) return -1;
    if (read_floats(f, b->modality_strength, GL_MODALITY_COUNT) != 0) return -1;
    if (read_u32(f, &b->exposure_count) != 0) return -1;
    if (read_u64(f, &b->last_activation_ms) != 0) return -1;
    if (read_f32(f, &b->confidence) != 0) return -1;
    return 0;
}

/* ============================================================ legacy template skip
 *
 * v1 templates were variable-length records: type(u32) + slot_count(u32) +
 * slots(slot_count × u32) + frequency(f32) + confidence(f32). We discard
 * each one to advance the file cursor. */
static int skip_legacy_template_v1(FILE* f) {
    uint32_t type_u = 0, slot_count = 0;
    if (read_u32(f, &type_u) != 0) return -1;
    if (read_u32(f, &slot_count) != 0) return -1;
    if (slot_count > GL_PERSIST_LEGACY_MAX_SLOTS) {
        LOG_WARN(LOG_MODULE, "legacy template slot_count=%u exceeds cap %u — file corrupt",
                 slot_count, GL_PERSIST_LEGACY_MAX_SLOTS);
        return -1;
    }
    long bytes_to_skip = (long)slot_count * (long)sizeof(uint32_t)
                         + 2L * (long)sizeof(float);
    if (fseek(f, bytes_to_skip, SEEK_CUR) != 0) return -1;
    return 0;
}

/* ============================================================ save */

int gl_persistence_save(const grounded_language_t* gl, const char* path) {
    if (!gl || !path) {
        LOG_WARN(LOG_MODULE, "gl_persistence_save: NULL parameter");
        return -1;
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        LOG_WARN(LOG_MODULE, "gl_persistence_save: cannot open %s for writing", path);
        return -1;
    }

    /* ---- Header (v2: dropped legacy template_count slot).
     * D5: pack hash_fn_version into the low byte of `reserved`.
     * High 24 bits stay zero for future use. */
    if (write_bytes(f, GL_SIDECAR_MAGIC, sizeof(GL_SIDECAR_MAGIC)) != 0) goto fail;
    if (write_u32(f, GL_SIDECAR_VERSION) != 0) goto fail;
    if (write_u32(f, (uint32_t)GL_HASH_FN_VERSION) != 0) goto fail;  /* reserved + hash_fn ver */
    if (write_u32(f, gl->semantic_dim) != 0) goto fail;
    if (write_u32(f, gl->vocab_count) != 0) goto fail;

    if (write_stats(f, &gl->stats) != 0) goto fail;
    if (write_u64(f, gl->rng_state) != 0) goto fail;

    /* ---- Lexicon entries. */
    for (uint32_t w = 0; w < gl->vocab_count; w++) {
        const gl_lexicon_entry_t* e = gl->vocab_list[w];
        if (!e) {
            /* Hole in vocab_list — write an empty form so the index lines up
             * on load. Can't legitimately happen unless the lexicon was
             * corrupted before save, but we keep the offset stable. */
            if (write_u16(f, 0) != 0) goto fail;
            if (write_u32(f, 0u) != 0) goto fail;     /* form_hash */
            if (write_u32(f, 0u) != 0) goto fail;     /* binding_count */
            if (write_u32(f, 0u) != 0) goto fail;     /* frequency */
            if (write_u32(f, GL_CLASS_UNKNOWN) != 0) goto fail;
            if (write_f32(f, 0.0f) != 0) goto fail;   /* class_confidence */
            if (write_u8(f, 0u) != 0) goto fail;      /* context_initialized */
            if (write_f32(f, 0.0f) != 0) goto fail;   /* valence */
            if (write_f32(f, 0.0f) != 0) goto fail;   /* arousal */
            continue;
        }

        if (write_str(f, e->form, GL_MAX_WORD_LEN) != 0) goto fail;
        if (write_u32(f, e->form_hash) != 0) goto fail;

        /* Bindings — bound by GL_PERSIST_MAX_BINDINGS_PER_WORD on load. */
        if (write_u32(f, e->binding_count) != 0) goto fail;
        for (uint32_t b = 0; b < e->binding_count; b++) {
            if (write_binding(f, &e->bindings[b]) != 0) goto fail;
        }

        if (write_u32(f, e->frequency) != 0) goto fail;
        if (write_u32(f, (uint32_t)e->learned_class) != 0) goto fail;
        if (write_f32(f, e->class_confidence) != 0) goto fail;

        uint8_t ctx_init = e->context_initialized ? 1u : 0u;
        if (write_u8(f, ctx_init) != 0) goto fail;
        if (ctx_init && e->context_vector) {
            if (write_floats(f, e->context_vector, gl->semantic_dim) != 0) goto fail;
        }

        if (write_f32(f, e->valence) != 0) goto fail;
        if (write_f32(f, e->arousal) != 0) goto fail;
    }

    /* v2: no template block — emergent grammar via SNN bridge. */

    /* v3: compositional phrase table (bigrams/trigrams). This is the
     * substrate the produce-path bigram reranker scores against
     * (cos + ALPHA*log(1+bigram_freq)); pre-v3 it was wiped on every
     * daemon restart, degenerating the rerank to cosine-only. Per-phrase
     * fixed record matches the standalone grounded_language_save layout:
     *   form[GL_MAX_PHRASE_LEN] | component_words(u8) | frequency(u32)
     * semantic_vec is NOT persisted — it's a lazy cache rebuilt from
     * constituent word vectors on first lookup post-load. */
    if (write_u32(f, gl->phrase_count) != 0) goto fail;
    for (uint32_t pi = 0; pi < gl->phrase_count; pi++) {
        const gl_phrase_t* p = &gl->phrases[pi];
        if (write_bytes(f, p->form, GL_MAX_PHRASE_LEN) != 0) goto fail;
        if (write_u8(f, p->component_words) != 0) goto fail;
        if (write_u32(f, p->frequency) != 0) goto fail;
    }

    fclose(f);
    LOG_INFO(LOG_MODULE,
             "Saved grounded language sidecar to %s (%u words, %u phrases, dim=%u)",
             path, gl->vocab_count, gl->phrase_count, gl->semantic_dim);
    return 0;

fail:
    LOG_WARN(LOG_MODULE, "gl_persistence_save: write failure at %s", path);
    fclose(f);
    return -1;
}

/* ============================================================ load */

int gl_persistence_load(grounded_language_t* gl, const char* path) {
    if (!gl || !path) {
        LOG_WARN(LOG_MODULE, "gl_persistence_load: NULL parameter");
        return -1;
    }

    FILE* f = fopen(path, "rb");
    if (!f) {
        /* Missing file is normal on first run / pre-sidecar checkpoints. */
        return -1;
    }

    /* D7: suppress NEW_WORD events + synthesized SNN-bridge spikes for the
     * duration of this rehydrate. ~30K replay events across ~20 subscribers
     * = ~600K wasted callback invocations on cold boot, plus the synthesized
     * spike pairs would walk STDP weights for bindings whose weights we just
     * loaded. Cleared on every exit path below (success + fail). */
    bool prev_loading = gl->is_loading;
    gl->is_loading = true;

    /* ---- Header validation. All early-exit paths goto fail so the
     * D7 is_loading flag gets restored. */
    char magic[8];
    if (fread(magic, 1, sizeof(magic), f) != sizeof(magic)
        || memcmp(magic, GL_SIDECAR_MAGIC, sizeof(magic)) != 0) {
        LOG_WARN(LOG_MODULE, "gl_persistence_load: bad magic in %s", path);
        goto fail;
    }

    uint32_t version = 0, reserved = 0;
    if (read_u32(f, &version) != 0 || read_u32(f, &reserved) != 0) {
        goto fail;
    }
    if (version != 1u && version != 2u && version != 3u) {
        LOG_WARN(LOG_MODULE,
                 "gl_persistence_load: unsupported version=%u (runtime=%u) for %s",
                 version, GL_SIDECAR_VERSION, path);
        goto fail;
    }

    /* D5: hash-fn version byte lives in the low byte of `reserved`. v1
     * files have reserved=0 (legacy hash). If the saved hash version
     * doesn't match GL_HASH_FN_VERSION the saved form_hash values are
     * stale relative to live hash_word(); we still load the lexicon
     * (form strings + bindings remain meaningful) but force-rehash each
     * entry's form_hash field on insert via lexicon_find_or_create —
     * which is what already happens (see comment near entry->form_hash
     * assignment below). The version mismatch is logged as a
     * not-the-end-of-the-world warning. */
    uint8_t saved_hash_fn_version = (uint8_t)(reserved & 0xFFu);
    if (saved_hash_fn_version != GL_HASH_FN_VERSION) {
        LOG_WARN(LOG_MODULE,
                 "gl_persistence_load: hash_fn_version mismatch "
                 "(file=%u runtime=%u) — form_hash fields will be "
                 "recomputed on rehydrate (lexicon still loadable)",
                 (unsigned)saved_hash_fn_version,
                 (unsigned)GL_HASH_FN_VERSION);
    }

    uint32_t saved_dim = 0, vocab_count = 0, legacy_template_count = 0;
    if (read_u32(f, &saved_dim) != 0
        || read_u32(f, &vocab_count) != 0) {
        goto fail;
    }
    if (version == 1u) {
        /* v1 had a template_count slot in the header; v2 dropped it. */
        if (read_u32(f, &legacy_template_count) != 0) {
            goto fail;
        }
    }

    if (saved_dim != gl->semantic_dim) {
        LOG_WARN(LOG_MODULE,
                 "gl_persistence_load: semantic_dim mismatch (file=%u handle=%u)",
                 saved_dim, gl->semantic_dim);
        goto fail;
    }
    if (vocab_count > GL_MAX_VOCAB) {
        LOG_WARN(LOG_MODULE,
                 "gl_persistence_load: vocab_count %u > GL_MAX_VOCAB %u — corrupt",
                 vocab_count, GL_MAX_VOCAB);
        goto fail;
    }

    /* ---- Stats + RNG. v1 reads include the legacy templates_learned slot. */
    int stats_rc = (version == 1u)
        ? read_stats_v1(f, &gl->stats)
        : read_stats_v2(f, &gl->stats);
    if (stats_rc != 0) goto fail;
    if (read_u64(f, &gl->rng_state) != 0) goto fail;

    /* ---- Lexicon entries.
     *
     * gl already exists with seeded function/conceptual words. We rehydrate
     * each saved word via gl_internal_lexicon_find_or_create() — for words
     * that already exist (seeded duplicates) we overwrite their fields with
     * the saved values; for new words we insert. The hash table is kept
     * coherent because find_or_create is the same primitive used at runtime. */
    char form_buf[GL_MAX_WORD_LEN];
    for (uint32_t w = 0; w < vocab_count; w++) {
        if (read_str(f, form_buf, sizeof(form_buf)) != 0) goto fail;

        uint32_t form_hash = 0, binding_count = 0;
        if (read_u32(f, &form_hash) != 0) goto fail;
        if (read_u32(f, &binding_count) != 0) goto fail;
        if (binding_count > GL_PERSIST_MAX_BINDINGS_PER_WORD) {
            LOG_WARN(LOG_MODULE,
                     "gl_persistence_load: binding_count %u > cap %u — corrupt",
                     binding_count, GL_PERSIST_MAX_BINDINGS_PER_WORD);
            goto fail;
        }

        gl_lexicon_entry_t* entry = NULL;
        if (form_buf[0] != '\0') {
            entry = gl_internal_lexicon_find_or_create(gl, form_buf);
            /* If the lexicon is full we still need to consume the bytes so
             * subsequent records aren't skewed. entry stays NULL and we
             * read-and-discard below. */
        }

        /* Read bindings into entry->bindings, growing as needed. */
        for (uint32_t b = 0; b < binding_count; b++) {
            gl_word_binding_t binding;
            memset(&binding, 0, sizeof(binding));
            if (read_binding(f, &binding) != 0) goto fail;
            if (!entry) continue;

            if (entry->binding_count >= entry->binding_capacity) {
                uint32_t new_cap = entry->binding_capacity > 0
                    ? entry->binding_capacity * 2 : 4;
                gl_word_binding_t* new_arr = (gl_word_binding_t*)nimcp_realloc(
                    entry->bindings, new_cap * sizeof(gl_word_binding_t));
                if (!new_arr) {
                    /* OOM growing — drop this binding but keep going. */
                    continue;
                }
                entry->bindings = new_arr;
                entry->binding_capacity = new_cap;
            }
            entry->bindings[entry->binding_count++] = binding;
        }

        uint32_t frequency = 0, learned_class_u = 0;
        float class_confidence = 0.0f;
        if (read_u32(f, &frequency) != 0) goto fail;
        if (read_u32(f, &learned_class_u) != 0) goto fail;
        if (read_f32(f, &class_confidence) != 0) goto fail;

        uint8_t ctx_init = 0;
        if (read_u8(f, &ctx_init) != 0) goto fail;
        /* Read context_vector into a scratch buffer first if entry is NULL,
         * so we still consume the bytes in lockstep with the file. */
        if (ctx_init) {
            if (entry && entry->context_vector) {
                if (read_floats(f, entry->context_vector, gl->semantic_dim) != 0) goto fail;
            } else {
                /* Skip semantic_dim floats. */
                if (fseek(f, (long)(gl->semantic_dim * sizeof(float)),
                          SEEK_CUR) != 0) goto fail;
            }
        }

        float valence = 0.0f, arousal = 0.0f;
        if (read_f32(f, &valence) != 0) goto fail;
        if (read_f32(f, &arousal) != 0) goto fail;

        if (entry) {
            /* Do NOT overwrite entry->form_hash with the saved value:
             * gl_internal_lexicon_find_or_create has already set it via
             * the live hash function, and the entry is sitting in the
             * hash table at slot (live_hash % size). Restoring a stale
             * disk hash would break lexicon_find — slot lookups recompute
             * the live hash and compare against entry->form_hash. The
             * saved form_hash field is informational only. */
            (void)form_hash;
            entry->frequency = frequency;
            entry->learned_class = (gl_word_class_t)learned_class_u;
            entry->class_confidence = class_confidence;
            entry->context_initialized = ctx_init ? true : false;
            entry->valence = valence;
            entry->arousal = arousal;
        }
    }

    /* ---- Legacy templates (v1 only). v2 omits this block entirely. */
    if (version == 1u) {
        for (uint32_t t = 0; t < legacy_template_count; t++) {
            if (skip_legacy_template_v1(f) != 0) goto fail;
        }
    }

    /* ---- v3: compositional phrase table. v1/v2 files end before this
     * block; phrase_count stays 0 and phrases re-learn from text. We
     * rehydrate via gl_internal_phrase_find_or_create (same primitive
     * _gl_track_phrases uses at runtime) so the hash + eviction
     * invariants hold; then stamp the saved frequency. semantic_vec is
     * NOT restored — it's a lazy cache rebuilt on first lookup. */
    if (version >= 3u) {
        uint32_t saved_phrase_count = 0;
        if (read_u32(f, &saved_phrase_count) != 0) goto fail;
        if (saved_phrase_count > GL_MAX_PHRASES) {
            LOG_WARN(LOG_MODULE,
                     "gl_persistence_load: phrase_count %u > GL_MAX_PHRASES %u — corrupt",
                     saved_phrase_count, (unsigned)GL_MAX_PHRASES);
            goto fail;
        }
        for (uint32_t pi = 0; pi < saved_phrase_count; pi++) {
            char phrase_form[GL_MAX_PHRASE_LEN];
            uint8_t  comp_n = 0;
            uint32_t freq   = 0;
            if (read_bytes(f, phrase_form, GL_MAX_PHRASE_LEN) != 0) goto fail;
            if (read_u8(f, &comp_n) != 0) goto fail;
            if (read_u32(f, &freq)  != 0) goto fail;
            phrase_form[GL_MAX_PHRASE_LEN - 1] = '\0';
            if (phrase_form[0] == '\0') continue;  /* skip empty slot */
            gl_phrase_t* p = gl_internal_phrase_find_or_create(gl, phrase_form, comp_n);
            if (p) p->frequency = freq;
        }
    }

    fclose(f);
    gl->is_loading = prev_loading;  /* D7: end of rehydrate */

    /* read_stats_v{1,2} above reads s->vocab_size directly from the
     * sidecar, which represents the vocab count AT SAVE TIME. If a
     * bulk-lexicon preload (NIMCP_BULK_LEXICON env, fired earlier in
     * brain init) added words BEFORE this load runs, those entries
     * are still in gl->lexicon (find_or_create doesn't remove anything)
     * but gl->stats.vocab_size now reports the saved-checkpoint count
     * instead of the live count. Pre-fix: pod with 29,424-word
     * WordNet+GloVe bulk preload + 4,669-word saved checkpoint
     * reported vocab_size=4669 — under-reported by ~25K.
     *
     * Reset stats.vocab_size to the live vocab_count so consumers see
     * the actual lexicon size. The other persisted stats counters
     * (total_bindings, total_comprehensions, etc.) are cumulative
     * across the brain's lifetime and stay as loaded — they're
     * intentionally history. vocab_size is current state. */
    gl->stats.vocab_size = gl->vocab_count;

    LOG_INFO(LOG_MODULE,
             "Loaded grounded language sidecar from %s (%u words live, "
             "dim=%u, fmt=v%u)",
             path, gl->vocab_count, gl->semantic_dim, version);
    return 0;

fail:
    LOG_WARN(LOG_MODULE, "gl_persistence_load: malformed sidecar at %s", path);
    fclose(f);
    gl->is_loading = prev_loading;  /* D7: end of rehydrate (fail path) */
    return -1;
}
