/**
 * @file nimcp_grounded_language.c
 * @brief Grounded Language System - Implementation
 *
 * Human-like language through grounded semantics, not token statistics.
 * Words are learned by binding to multimodal concept representations.
 * Production works by navigating semantic space and finding words.
 *
 * @version 1.0.0
 * @date 2026-03-07
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_internal.h"
#include "language/nimcp_bigram_spectrum.h"
#include "cognitive/grounded_language/nimcp_stage_table.h"  /* Slice E */
#include "language/nimcp_concept_registry.h"                /* Slice B / walkthrough-2 */
#include "security/nimcp_toxicity.h"                        /* 2026-05-21: content classifier */
#include "security/nimcp_toxicity_response.h"                /* Phase 3a: counterclaim engine */

/* TA-2 LGSS gate: pull the small safety_types header (key-value PODs +
 * SAFETY_DOMAIN_/SAFETY_ACTION_ enums) and the audit log header. We
 * intentionally do NOT include the LGSS umbrella header here — that
 * cascades into more cognitive/symbolic_logic headers and triggers
 * enum redefinitions in unrelated translation units. lgss_evaluate is
 * forward-declared below alongside the comprehend definition. */
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety_types.h"
#include "security/nimcp_audit_log.h"
/* PA-4+ : compile the bigram-spectrum implementation as part of this
 * translation unit. The build's source list is explicit (not GLOB) and
 * the CMakeLists.txt is outside the modify set for this change, so we
 * inline-include the .c here — same pattern as nimcp.c → nimcp_part_*.c.
 * The header (nimcp_bigram_spectrum.h) provides the public API and is
 * included separately by the brain wrapper and the test. */
#include "nimcp_bigram_spectrum.c"
#include "snn/bridges/nimcp_snn_language_bridge.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
#include "cognitive/memory/nimcp_engram.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>  /* g_spectrum_map_lock */

/* Silence -Wunused-result for fread — reads are best-effort; file-format magic
 * and version checks downstream validate integrity. */
static inline void nimcp_fread_ignore(void* ptr, size_t sz, size_t n, FILE* f) {
    size_t got = fread(ptr, sz, n, f);
    (void)got;
}
#define fread_chk nimcp_fread_ignore

#define LOG_MODULE "GROUNDED_LANG"

/* 2026-05-16 cross-bind wedge structural fix: cap-at-insertion.
 *
 * Without this, content-content cross-binding in
 * grounded_language_learn_from_text accumulates new (entry_j, concept)
 * pairs unboundedly as training varies. The function-word source guard
 * stops the closed-class leak, but each content word's binding_count
 * still grows linearly with corpus exposure. At B=~1000/word and
 * sentence-scale N=20-30, learn_from_text wedges at 10-60+ sec per
 * call. lexicon_bind now enforces B <= GL_MAX_BINDINGS_PER_WORD by
 * evicting the weakest binding when full; new bindings weaker than
 * the current floor are skipped entirely. */
#define GL_MAX_BINDINGS_PER_WORD 128

/* Upper bound on the per-call similarity scan in find_or_create_concept.
 * Keeps bulk grounding (lexicon loader, mass re-grounding) at O(words ×
 * CAP) instead of O(words^2). See find_or_create_concept for rationale. */
#define GL_CONCEPT_SCAN_CAP 2048u

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(grounded_language)

/* IM-3 — forward decls for the brain_immune antigen-presentation API.
 * Pulling in cognitive/immune/nimcp_brain_immune.h would drag in BBB +
 * BFT + swarm-immune + bio-router + microglia headers (and the
 * INFLAMMATION_* / APHASIA_* enum cohort that has historically
 * collided with other modules — see commit 3902f2e92 for the same
 * pattern in brain_init_language.c). The Tier-3 inspector only needs
 * three symbols: the opaque struct typedef, the antigen-source enum,
 * and the present_antigen entry point. Replicating the enum locally
 * keeps the value identical (ABI-compatible plain enum) without
 * pulling the umbrella header. */
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;
typedef enum {
    _GL_ANTIGEN_SOURCE_BBB = 0,
    _GL_ANTIGEN_SOURCE_BFT,
    _GL_ANTIGEN_SOURCE_ANOMALY,
    _GL_ANTIGEN_SOURCE_SWARM,
    _GL_ANTIGEN_SOURCE_MANUAL
} _gl_brain_antigen_source_t;
extern int brain_immune_present_antigen(
    brain_immune_system_t* system,
    _gl_brain_antigen_source_t source,
    const uint8_t* epitope,
    size_t epitope_len,
    uint32_t severity,
    uint32_t source_node,
    uint32_t* antigen_id);

/*=============================================================================
 * Internal Structures
 *
 * `struct grounded_language` and `gl_context_t` are defined in
 * src/language/nimcp_grounded_language_internal.h so that the persistence
 * sidecar (nimcp_grounded_language_persistence.c) can iterate the lexicon
 * field-by-field without duplicating the layout. Do NOT add struct
 * definitions here — put them in the internal header.
 *===========================================================================*/

/*=============================================================================
 * Utility Functions
 *===========================================================================*/

/** Simple string hash (FNV-1a) */
static uint32_t hash_word(const char* word) {
    uint32_t hash = 2166136261u;
    for (const char* p = word; *p; p++) {
        hash ^= (uint32_t)(unsigned char)tolower(*p);
        hash *= 16777619u;
    }
    return hash;
}

/** Lowercase a word in-place into buffer */
static void lowercase_word(const char* src, char* dst, size_t max_len) {
    size_t i;
    for (i = 0; i < max_len - 1 && src[i]; i++) {
        dst[i] = (char)tolower((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

/** Cosine similarity between two vectors */
static float cosine_similarity(const float* a, const float* b, uint32_t dim) {
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    if (denom < 1e-8f) return 0.0f;
    return dot / denom;
}

/** Simple xorshift64 RNG */
static float gl_random(grounded_language_t* gl) {
    gl->rng_state ^= gl->rng_state << 13;
    gl->rng_state ^= gl->rng_state >> 7;
    gl->rng_state ^= gl->rng_state << 17;
    return (float)(gl->rng_state & 0xFFFFFF) / (float)0xFFFFFF;
}

/** Normalize a vector in-place */
static void normalize_vector(float* vec, uint32_t dim) {
    float norm = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        norm += vec[i] * vec[i];
    }
    norm = sqrtf(norm);
    if (norm < 1e-8f) return;
    float inv = 1.0f / norm;
    for (uint32_t i = 0; i < dim; i++) {
        vec[i] *= inv;
    }
}

/*=============================================================================
 * Lexicon Operations
 *===========================================================================*/

/* Forward decls for the mutate_lock helpers — defined later in this TU. */
static void gl_mutate_lock(grounded_language_t* gl);
static void gl_mutate_unlock(grounded_language_t* gl);

/** Find or create a lexicon entry for a word.
 *
 * Audit walkthrough-2 fix — the hash-table walk + insert is protected
 * by mutate_lock. NEW_WORD event firing happens OUTSIDE the lock to
 * avoid re-entrant deadlock if a subscriber writes back into the GL.
 * gl_mirror_new_word_to_regions also runs outside since it takes
 * Broca/Wernicke locks (lock-ordering hazard). */
static gl_lexicon_entry_t* lexicon_find_or_create(grounded_language_t* gl, const char* word) {
    char lower[GL_MAX_WORD_LEN];
    lowercase_word(word, lower, GL_MAX_WORD_LEN);
    uint32_t h = hash_word(lower);
    uint32_t idx = h % gl->lexicon_size;

    gl_mutate_lock(gl);

    gl_lexicon_entry_t* result = NULL;
    bool created_new = false;

    /* Linear probe to find existing entry */
    for (uint32_t probe = 0; probe < gl->lexicon_size; probe++) {
        uint32_t slot = (idx + probe) % gl->lexicon_size;
        if (!gl->lexicon[slot]) {
            /* Empty slot - create new entry */
            if (gl->vocab_count >= GL_MAX_VOCAB) {
                LOG_WARN(LOG_MODULE, "Lexicon full (%u words)", gl->vocab_count);
                gl_mutate_unlock(gl);
                return NULL;
            }

            gl_lexicon_entry_t* entry = (gl_lexicon_entry_t*)nimcp_calloc(1, sizeof(gl_lexicon_entry_t));
            if (!entry) {
                gl_mutate_unlock(gl);
                return NULL;
            }

            strncpy(entry->form, lower, GL_MAX_WORD_LEN - 1);
            entry->form_hash = h;
            entry->binding_capacity = 4; /* Start small, grow */
            entry->bindings = (gl_word_binding_t*)nimcp_calloc(entry->binding_capacity,
                                                                sizeof(gl_word_binding_t));
            if (!entry->bindings) {
                nimcp_free(entry);
                gl_mutate_unlock(gl);
                return NULL;
            }

            entry->context_vector = (float*)nimcp_calloc(gl->semantic_dim, sizeof(float));
            if (!entry->context_vector) {
                nimcp_free(entry->bindings);
                nimcp_free(entry);
                gl_mutate_unlock(gl);
                return NULL;
            }

            gl->lexicon[slot] = entry;
            gl->vocab_list[gl->vocab_count] = entry;
            gl->vocab_count++;
            gl->stats.vocab_size = gl->vocab_count;

            /* NLP: seed learned_class from morphological cue if any.
             * Low confidence (0.4) — surface morphology is suggestive
             * but not authoritative; positional context will overwrite. */
            extern gl_word_class_t gl_morph_pos_hint(const char*);
            gl_word_class_t hint = gl_morph_pos_hint(entry->form);
            if (hint != GL_CLASS_UNKNOWN) {
                entry->learned_class = hint;
                entry->class_confidence = 0.4f;
            }

            result = entry;
            created_new = true;
            break;
        }

        if (gl->lexicon[slot]->form_hash == h &&
            strcmp(gl->lexicon[slot]->form, lower) == 0) {
            result = gl->lexicon[slot]; /* Found existing */
            break;
        }
    }

    gl_mutate_unlock(gl);

    /* Side effects fire OUTSIDE the lock — mirror writes into Broca /
     * Wernicke (which have their own locks), and the bus dispatch may
     * re-enter the GL via subscribers. Both must run unlocked. */
    if (created_new && result) {
        /* Mirror the new entry into broca/wernicke region lexicons
         * if either adapter is connected. No-op when not wired. */
        extern void gl_mirror_new_word_to_regions(grounded_language_t*,
                                                   const char*);
        gl_mirror_new_word_to_regions(gl, result->form);

        /* Fire NEW_WORD event on the cognitive bus.
         *
         * D7: suppress during persistence rehydrate. Resume blasts
         * ~30K NEW_WORD events to every subscriber — pure noise on
         * cold boot. Live grounding events still fire normally. */
        if (!gl->is_loading) {
            gl_event_t ev = {0};
            ev.type = GL_EVENT_NEW_WORD;
            ev.word = result->form;
            gl_fire_event(gl, &ev);
        }
    }

    return result;
}

/** Find a lexicon entry (read-only, no create) */
static const gl_lexicon_entry_t* lexicon_find(const grounded_language_t* gl, const char* word) {
    char lower[GL_MAX_WORD_LEN];
    lowercase_word(word, lower, GL_MAX_WORD_LEN);
    uint32_t h = hash_word(lower);
    uint32_t idx = h % gl->lexicon_size;

    for (uint32_t probe = 0; probe < gl->lexicon_size; probe++) {
        uint32_t slot = (idx + probe) % gl->lexicon_size;
        if (!gl->lexicon[slot]) return NULL;
        if (gl->lexicon[slot]->form_hash == h &&
            strcmp(gl->lexicon[slot]->form, lower) == 0) {
            return gl->lexicon[slot];
        }
    }
    return NULL;
}

/* External alias used by the NLP frontend (sibling .c file). */
const gl_lexicon_entry_t* lexicon_find_internal(const grounded_language_t* gl,
                                                  const char* word) {
    return lexicon_find(gl, word);
}

/* Public read-only "is this word in the lexicon?" probe (#1).
 * Used by region adapters (broca/wernicke) to decide whether to fall
 * through to GL when their local lexicons miss, without paying the
 * cost of a full comprehend / fuzzy / morph chain. */
bool grounded_language_has_word(const grounded_language_t* gl, const char* word) {
    if (!gl || !word || !word[0]) return false;
    return lexicon_find(gl, word) != NULL;
}

/*=============================================================================
 * #9 Compositional phrase tracking
 *===========================================================================*/

/* Build a space-joined lowercased phrase form from N word tokens.
 * Returns 1 on success, 0 on overflow. */
static int _gl_phrase_form(char* out, size_t out_sz,
                             char** words, uint32_t n) {
    if (!out || out_sz == 0 || !words || n == 0) return 0;
    size_t pos = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (i > 0) {
            if (pos + 1 >= out_sz) return 0;
            out[pos++] = ' ';
        }
        for (const char* p = words[i]; *p; p++) {
            if (pos + 1 >= out_sz) return 0;
            unsigned char c = (unsigned char)*p;
            if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
            out[pos++] = (char)c;
        }
    }
    out[pos] = '\0';
    return 1;
}

/* Find phrase by form, or insert if not present. When at capacity,
 * evicts the least-frequent existing phrase (LRU-by-frequency) so
 * long-running training keeps the most-useful phrases rather than
 * locking in the first 512 ever seen. Returns the slot — never NULL
 * once gl->phrases is allocated. Component_words is recorded on
 * insert (or refresh after eviction). */
gl_phrase_t* gl_internal_phrase_find_or_create(grounded_language_t* gl,
                                                const char* form,
                                                uint8_t component_words) {
    if (!gl || !gl->phrases || !form) return NULL;
    uint32_t h = hash_word(form);

    /* Audit walkthrough-2 fix — phrase table writes + eviction are
     * protected by mutate_lock. Pre-fix, concurrent _gl_track_phrases
     * calls (from comprehend on multiple threads) could double-allocate
     * the phrase_count slot and corrupt the linear scan. */
    gl_mutate_lock(gl);

    /* Linear scan — N ≤ GL_MAX_PHRASES = 512. learn_from_text is
     * cold-ish and we can afford O(N) per phrase track. */
    for (uint32_t i = 0; i < gl->phrase_count; i++) {
        if (gl->phrases[i].form_hash == h &&
            strcmp(gl->phrases[i].form, form) == 0) {
            gl_phrase_t* found = &gl->phrases[i];
            gl_mutate_unlock(gl);
            return found;
        }
    }

    gl_phrase_t* p;
    if (gl->phrase_count < GL_MAX_PHRASES) {
        p = &gl->phrases[gl->phrase_count++];
    } else {
        /* Capacity overflow — evict least-frequent. Linear scan to
         * pick the victim. */
        uint32_t victim = 0;
        for (uint32_t i = 1; i < gl->phrase_count; i++) {
            if (gl->phrases[i].frequency < gl->phrases[victim].frequency) {
                victim = i;
            }
        }
        /* Refuse to evict if the victim has higher frequency than the
         * incoming phrase would naturally start at — the new phrase
         * starts at frequency=0 and would be evicted on its own first
         * tick. Drop the new one instead. */
        if (gl->phrases[victim].frequency > 0) {
            p = &gl->phrases[victim];
            nimcp_free(p->semantic_vec);
            memset(p, 0, sizeof(*p));
            gl->phrases_evicted++;
        } else {
            /* Everything is freq=0 — can't reliably pick a victim.
             * Reject the new phrase rather than thrash. */
            gl_mutate_unlock(gl);
            return NULL;
        }
    }

    size_t flen = strlen(form);
    if (flen >= GL_MAX_PHRASE_LEN) flen = GL_MAX_PHRASE_LEN - 1;
    memcpy(p->form, form, flen);
    p->form[flen] = '\0';
    p->form_hash = h;
    p->component_words = component_words;
    p->frequency = 0;
    p->semantic_vec = NULL;
    p->vec_initialized = false;

    gl_mutate_unlock(gl);
    return p;
}

/* Track bigrams + trigrams in a sentence. Cheap; called once per
 * learn_from_text after the existing distributional loops. */
static void _gl_track_phrases(grounded_language_t* gl,
                                char** words, uint32_t word_count) {
    if (!gl || !gl->phrases || word_count < 2) return;
    char buf[GL_MAX_PHRASE_LEN];

    for (uint32_t i = 0; i + 1 < word_count; i++) {
        if (_gl_phrase_form(buf, sizeof(buf), &words[i], 2)) {
            gl_phrase_t* p = gl_internal_phrase_find_or_create(gl, buf, 2);
            if (p) {
                p->frequency++;
                /* Vector cache invalidated when components change —
                 * cheapest way is to clear on any frequency tick so
                 * the next lookup recomputes from current word
                 * vectors. We could be smarter but recompute is O(dim). */
                p->vec_initialized = false;
            }
        }
        if (i + 2 < word_count) {
            if (_gl_phrase_form(buf, sizeof(buf), &words[i], 3)) {
                gl_phrase_t* p = gl_internal_phrase_find_or_create(gl, buf, 3);
                if (p) { p->frequency++; p->vec_initialized = false; }
            }
        }
    }
}

const gl_phrase_t* grounded_language_lookup_phrase(
        const grounded_language_t* gl, const char* form) {
    if (!gl || !gl->phrases || !form) return NULL;
    /* Lowercase the input into a stack buffer for case-insensitive lookup. */
    char low[GL_MAX_PHRASE_LEN];
    size_t i = 0;
    for (; form[i] && i < sizeof(low) - 1; i++) {
        unsigned char c = (unsigned char)form[i];
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        low[i] = (char)c;
    }
    low[i] = '\0';
    uint32_t h = hash_word(low);
    for (uint32_t k = 0; k < gl->phrase_count; k++) {
        if (gl->phrases[k].form_hash == h &&
            strcmp(gl->phrases[k].form, low) == 0) {
            /* Lazy compute the semantic vector from constituent words. */
            gl_phrase_t* mut = (gl_phrase_t*)&gl->phrases[k];
            if (!mut->vec_initialized) {
                if (!mut->semantic_vec) {
                    mut->semantic_vec = (float*)nimcp_calloc(
                        gl->semantic_dim, sizeof(float));
                    if (!mut->semantic_vec) return NULL;
                }
                memset(mut->semantic_vec, 0,
                       gl->semantic_dim * sizeof(float));
                /* Tokenize the form back out and average each
                 * component's context_vector. */
                char tmp[GL_MAX_PHRASE_LEN];
                memcpy(tmp, mut->form, sizeof(tmp));
                char* save = NULL;
                char* tok = strtok_r(tmp, " ", &save);
                uint32_t n = 0;
                while (tok) {
                    const gl_lexicon_entry_t* e = lexicon_find(gl, tok);
                    if (e && e->context_initialized) {
                        for (uint32_t d = 0; d < gl->semantic_dim; d++) {
                            mut->semantic_vec[d] += e->context_vector[d];
                        }
                        n++;
                    }
                    tok = strtok_r(NULL, " ", &save);
                }
                if (n > 0) {
                    float inv = 1.0f / (float)n;
                    for (uint32_t d = 0; d < gl->semantic_dim; d++) {
                        mut->semantic_vec[d] *= inv;
                    }
                }
                mut->vec_initialized = true;
            }
            return &gl->phrases[k];
        }
    }
    return NULL;
}

uint32_t grounded_language_phrase_count(const grounded_language_t* gl) {
    return gl ? gl->phrase_count : 0u;
}

uint32_t grounded_language_get_top_phrases(const grounded_language_t* gl,
                                             uint32_t min_freq,
                                             uint8_t min_n,
                                             const gl_phrase_t** out_phrases,
                                             uint32_t max_k) {
    if (!gl || !gl->phrases || !out_phrases || max_k == 0) return 0;
    uint32_t n = gl->phrase_count;
    if (n == 0) return 0;

    /* Selection sort over the first max_k positions. N ≤ 512 — cheap. */
    /* Build an index array filtered by qualifiers first. */
    uint32_t idx_buf_stack[GL_MAX_PHRASES];
    uint32_t* idx = idx_buf_stack;
    uint32_t qcount = 0;
    for (uint32_t i = 0; i < n; i++) {
        const gl_phrase_t* p = &gl->phrases[i];
        if (p->frequency < min_freq) continue;
        if (min_n != 0 && p->component_words != min_n) continue;
        idx[qcount++] = i;
    }
    if (qcount == 0) return 0;

    uint32_t k = (qcount < max_k) ? qcount : max_k;
    for (uint32_t i = 0; i < k; i++) {
        uint32_t best = i;
        for (uint32_t j = i + 1; j < qcount; j++) {
            if (gl->phrases[idx[j]].frequency >
                gl->phrases[idx[best]].frequency) best = j;
        }
        if (best != i) { uint32_t t = idx[i]; idx[i] = idx[best]; idx[best] = t; }
        out_phrases[i] = &gl->phrases[idx[i]];
    }
    return k;
}

/* Per-modality binding-coverage probe. See header for contract.
 *
 * For every binding in every lexicon entry, increment out_counts[m] for
 * each modality m whose modality_strength[m] > 0. A binding grounded in
 * just visual contributes only to out_counts[VISUAL]; one reinforced
 * across visual+auditory contributes to both. This is the natural
 * "coverage" probe — it answers "how many bindings carry information in
 * modality m" rather than collapsing to a single dominant modality. */
void grounded_language_get_modality_counts(const grounded_language_t* gl,
                                            uint32_t out_counts[GL_MODALITY_COUNT]) {
    if (!out_counts) return;
    for (uint32_t m = 0; m < GL_MODALITY_COUNT; m++) out_counts[m] = 0;
    if (!gl || !gl->vocab_list || gl->vocab_count == 0) return;

    for (uint32_t v = 0; v < gl->vocab_count; v++) {
        const gl_lexicon_entry_t* e = gl->vocab_list[v];
        if (!e || !e->bindings) continue;
        for (uint32_t bi = 0; bi < e->binding_count; bi++) {
            const gl_word_binding_t* b = &e->bindings[bi];
            for (uint32_t m = 0; m < GL_MODALITY_COUNT; m++) {
                /* Defensive: GL_MODALITY_COUNT is the array length so the
                 * index is in-range by construction; the loop bound is the
                 * "modality < GL_MODALITY_COUNT" guard. Count any modality
                 * that has accrued binding strength > 0 (set on the first
                 * grounding event for that modality, decayed but never
                 * pushed below 0 by subsequent updates). */
                if (b->modality_strength[m] > 0.0f) out_counts[m]++;
            }
        }
    }
}

/*=============================================================================
 * #12 SNN spike → lexicon decoding
 *===========================================================================*/

int gl_observe_snn_spikes(grounded_language_t* gl,
                            const float* spike_rates,
                            uint32_t rate_dim,
                            float* confidence_out) {
    if (!gl || !spike_rates || rate_dim == 0) return -1;
    if (rate_dim != gl->semantic_dim) return -1;
    if (confidence_out) *confidence_out = 0.0f;
    if (gl->vocab_count == 0) return 0;

    /* Find best-matching lexicon entry by cosine similarity of
     * spike_rates against each entry's context_vector. */
    int32_t best_idx = -1;
    float best_sim = 0.0f;
    for (uint32_t v = 0; v < gl->vocab_count; v++) {
        const gl_lexicon_entry_t* e = gl->vocab_list[v];
        if (!e || !e->context_initialized) continue;
        float sim = cosine_similarity(spike_rates, e->context_vector,
                                        gl->semantic_dim);
        if (sim > best_sim) { best_sim = sim; best_idx = (int32_t)v; }
    }

    if (confidence_out) *confidence_out = best_sim;
    if (best_idx < 0 || best_sim < GL_SNN_SPIKE_MATCH_THRESHOLD) return 0;

    /* Fire COMPREHENDED for the matched word. */
    gl_event_t bus_ev = {0};
    bus_ev.type         = GL_EVENT_COMPREHENDED;
    bus_ev.word         = gl->vocab_list[best_idx]->form;
    bus_ev.text         = gl->vocab_list[best_idx]->form;
    bus_ev.semantic_vec = spike_rates;
    bus_ev.confidence   = best_sim;
    gl_fire_event(gl, &bus_ev);
    return 1;
}

/*=============================================================================
 * #8 Cross-modal disambiguation
 *===========================================================================*/

uint32_t grounded_language_disambiguate(const grounded_language_t* gl,
                                          const char* word,
                                          const float* modality_weights,
                                          uint64_t* out_concepts,
                                          float* out_scores,
                                          uint32_t max_k) {
    if (!gl || !word || !out_concepts || !out_scores || max_k == 0) return 0;

    const gl_lexicon_entry_t* e = lexicon_find(gl, word);
    if (!e || e->binding_count == 0) return 0;

    /* Determine if we have a real weight signal. NULL or all-zero
     * weights → fall back to plain confidence × overall-strength
     * ranking (general "best concept" query). */
    bool have_weights = false;
    if (modality_weights) {
        for (uint32_t m = 0; m < GL_MODALITY_COUNT; m++) {
            if (modality_weights[m] > 1e-6f) { have_weights = true; break; }
        }
    }

    /* Score each binding. */
    uint32_t n = e->binding_count;
    /* Stack-bounded — entry binding_count grows on demand but is
     * realistically <64 in practice. Use a small buffer with overflow
     * fallback to heap. */
    float  stack_scores[64];
    uint64_t stack_ids[64];
    float* scores = (n <= 64) ? stack_scores : (float*)nimcp_malloc(n * sizeof(float));
    uint64_t* ids = (n <= 64) ? stack_ids    : (uint64_t*)nimcp_malloc(n * sizeof(uint64_t));
    if (!scores || !ids) {
        if (n > 64) { nimcp_free(scores); nimcp_free(ids); }
        return 0;
    }

    for (uint32_t i = 0; i < n; i++) {
        const gl_word_binding_t* b = &e->bindings[i];
        float modality_score = 0.0f;
        if (have_weights) {
            for (uint32_t m = 0; m < GL_MODALITY_COUNT; m++) {
                float w = modality_weights[m];
                if (w < 0.0f) w = 0.0f;
                if (w > 1.0f) w = 1.0f;
                modality_score += b->modality_strength[m] * w;
            }
        } else {
            modality_score = b->strength;
        }
        /* Use confidence directly. Clamp tiny floor so brand-new
         * bindings (confidence ≈ 0.2 from lexicon_bind) still register
         * a non-trivial score, but never let a 0-confidence binding
         * outrank a real one with a 1.0 fallback. */
        float conf = b->confidence;
        if (conf < 0.05f) conf = 0.05f;
        scores[i] = modality_score * conf;
        ids[i]    = b->concept_id;
    }

    /* Partial top-k selection (selection sort over the first max_k
     * positions). N ≤ ~64 in practice — no need for heapsort. */
    uint32_t k = (n < max_k) ? n : max_k;
    for (uint32_t i = 0; i < k; i++) {
        uint32_t best = i;
        for (uint32_t j = i + 1; j < n; j++) {
            if (scores[j] > scores[best]) best = j;
        }
        if (best != i) {
            float ts = scores[i]; scores[i] = scores[best]; scores[best] = ts;
            uint64_t ti = ids[i]; ids[i] = ids[best]; ids[best] = ti;
        }
        out_concepts[i] = ids[i];
        out_scores[i]   = scores[i];
    }

    if (n > 64) { nimcp_free(scores); nimcp_free(ids); }
    return k;
}

/*=============================================================================
 * #5 LRU lexicon eviction
 *===========================================================================*/

/* Free a single lexicon entry's heap storage. */
static void _gl_free_entry(gl_lexicon_entry_t* e) {
    if (!e) return;
    nimcp_free(e->bindings);
    nimcp_free(e->context_vector);
    nimcp_free(e);
}

/* Rebuild the hash table from vocab_list. After eviction the previous
 * probe chains are stale; rather than chase tombstones through every
 * lookup we wipe and re-insert. O(N) — only called from the eviction
 * path which is itself rare. */
static void _gl_rebuild_lexicon_hash(grounded_language_t* gl) {
    memset(gl->lexicon, 0, gl->lexicon_size * sizeof(gl_lexicon_entry_t*));
    for (uint32_t v = 0; v < gl->vocab_count; v++) {
        gl_lexicon_entry_t* e = gl->vocab_list[v];
        if (!e) continue;
        uint32_t idx = e->form_hash % gl->lexicon_size;
        for (uint32_t probe = 0; probe < gl->lexicon_size; probe++) {
            uint32_t slot = (idx + probe) % gl->lexicon_size;
            if (!gl->lexicon[slot]) {
                gl->lexicon[slot] = e;
                break;
            }
        }
    }
}

uint32_t grounded_language_evict_lru(grounded_language_t* gl, uint32_t n) {
    if (!gl || n == 0 || gl->vocab_count == 0) return 0;

    /* X1 (walkthrough fix, 2026-05-27): hold mutate_lock for the whole
     * eviction so the entry frees + vocab_list compaction + hash rebuild are
     * atomic w.r.t. lexicon_find_or_create and any other mutate_lock holder.
     * The sole caller (sleep DEEP_NREM hook in the memory bridge) cannot hold
     * this lock — it's static to this TU — so there's no re-entrant deadlock,
     * and the evict helpers (_gl_free_entry / _gl_rebuild_lexicon_hash) don't
     * lock. (produce still reads vocab_list lock-free; that read is kept safe
     * in production by the daemon's single-writer serialization — eviction and
     * produce are both write-socket commands under one lock — which this
     * C-level lock now complements rather than relies on.) */
    gl_mutate_lock(gl);

    /* Pass 1: find the n-th lowest frequency among unpinned entries.
     * For small batches an N×K linear scan is fine — N ≤ vocab_count,
     * K = n. With n ≤ ~256 and vocab ≤ 16384 we're talking 4M ops
     * worst-case, which is microseconds. No need for partial-sort. */
    uint32_t evicted = 0;
    while (evicted < n) {
        uint32_t target = UINT32_MAX;
        uint32_t target_freq = UINT32_MAX;
        for (uint32_t v = 0; v < gl->vocab_count; v++) {
            const gl_lexicon_entry_t* e = gl->vocab_list[v];
            if (!e) continue;
            if (e->frequency >= GL_LRU_FREQ_PIN_FLOOR) continue;
            if (e->frequency < target_freq) {
                target_freq = e->frequency;
                target = v;
            }
        }
        if (target == UINT32_MAX) break;  /* nothing left to evict */

        _gl_free_entry(gl->vocab_list[target]);
        gl->vocab_list[target] = NULL;
        evicted++;
    }

    if (evicted == 0) { gl_mutate_unlock(gl); return 0; }

    /* Compact vocab_list — shift survivors down to fill the holes. */
    uint32_t write = 0;
    for (uint32_t read = 0; read < gl->vocab_count; read++) {
        if (gl->vocab_list[read]) {
            if (write != read) gl->vocab_list[write] = gl->vocab_list[read];
            write++;
        }
    }
    /* Clear stale tail slots so destroy() doesn't double-free. */
    for (uint32_t i = write; i < gl->vocab_count; i++) gl->vocab_list[i] = NULL;
    gl->vocab_count = write;
    gl->stats.vocab_size = gl->vocab_count;

    /* Rebuild hash table (probe chains are now stale). */
    _gl_rebuild_lexicon_hash(gl);

    gl_mutate_unlock(gl);

    LOG_DEBUG(LOG_MODULE,
              "lexicon eviction: removed %u entries (vocab now %u)",
              evicted, gl->vocab_count);
    return evicted;
}

/*=============================================================================
 * Fuzzy Matching (Two-stage fallback for typos/misspellings)
 *===========================================================================*/

/** Minimum similarity threshold to accept a fuzzy match */
#define GL_FUZZY_THRESHOLD  0.65f

/** Sanity floor for the *weaker* of the two signals (phonological vs
 *  character-set). Typos clear both signals; random consonant strings
 *  clear phonological alone (5-feature space is small) and fail this. */
#define GL_FUZZY_MIN_SIGNAL 0.40f

/** Minimum word length to attempt fuzzy matching (skip "a", "I", etc.) */
#define GL_FUZZY_MIN_LEN    3

/**
 * Phonological similarity — distinctive feature comparison.
 *
 * Maps each letter to a simplified articulatory feature vector (place, manner,
 * voicing) and compares via feature overlap. Catches sound-alike typos:
 *   "waht" ↔ "what"  (same phonemes, transposed)
 *   "mathmatics" ↔ "mathematics"  (vowel deletion)
 *   "kalkulus" ↔ "calculus"  (phonetic spelling)
 *
 * Returns similarity in [0, 1].
 */
static float phonological_similarity(const char* a, const char* b) {
    /* Simplified phoneme feature vectors for English letters:
     * [place(0-7), manner(0-7), voicing(0/1), vowel_height(0-3), is_vowel]
     * These approximate distinctive features from phonological theory. */
    static const uint8_t FEATURES[26][5] = {
        /* a */ {0, 0, 1, 3, 1}, /* b */ {1, 0, 1, 0, 0}, /* c */ {4, 0, 0, 0, 0},
        /* d */ {2, 0, 1, 0, 0}, /* e */ {0, 0, 1, 2, 1}, /* f */ {3, 1, 0, 0, 0},
        /* g */ {4, 0, 1, 0, 0}, /* h */ {7, 1, 0, 0, 0}, /* i */ {0, 0, 1, 1, 1},
        /* j */ {5, 2, 1, 0, 0}, /* k */ {4, 0, 0, 0, 0}, /* l */ {2, 3, 1, 0, 0},
        /* m */ {1, 4, 1, 0, 0}, /* n */ {2, 4, 1, 0, 0}, /* o */ {0, 0, 1, 2, 1},
        /* p */ {1, 0, 0, 0, 0}, /* q */ {4, 0, 0, 0, 0}, /* r */ {5, 3, 1, 0, 0},
        /* s */ {2, 1, 0, 0, 0}, /* t */ {2, 0, 0, 0, 0}, /* u */ {0, 0, 1, 1, 1},
        /* v */ {3, 1, 1, 0, 0}, /* w */ {1, 5, 1, 0, 0}, /* x */ {2, 1, 0, 0, 0},
        /* y */ {5, 5, 1, 0, 0}, /* z */ {2, 1, 1, 0, 0},
    };

    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    if (len_a == 0 || len_b == 0) return 0.0f;

    /* Length ratio penalty — very different lengths are unlikely matches */
    float len_ratio = (float)(len_a < len_b ? len_a : len_b) /
                      (float)(len_a > len_b ? len_a : len_b);
    if (len_ratio < 0.5f) return 0.0f;

    /* Compare feature overlap using a sliding alignment.
     * For each letter in 'a', find the most similar letter in a window of 'b'. */
    float total_sim = 0.0f;
    uint32_t comparisons = 0;

    for (size_t i = 0; i < len_a; i++) {
        char ca = (char)tolower((unsigned char)a[i]);
        if (ca < 'a' || ca > 'z') continue;
        uint32_t fa = ca - 'a';

        float best_match = 0.0f;
        /* Search in a window around the proportional position in b */
        size_t center = (i * len_b) / len_a;
        size_t win_start = (center >= 2) ? center - 2 : 0;
        size_t win_end = (center + 3 < len_b) ? center + 3 : len_b;

        for (size_t j = win_start; j < win_end; j++) {
            char cb = (char)tolower((unsigned char)b[j]);
            if (cb < 'a' || cb > 'z') continue;
            uint32_t fb = cb - 'a';

            /* Count matching features */
            uint32_t matches = 0;
            for (int f = 0; f < 5; f++) {
                if (FEATURES[fa][f] == FEATURES[fb][f]) matches++;
            }
            float sim = (float)matches / 5.0f;
            if (sim > best_match) best_match = sim;
        }

        total_sim += best_match;
        comparisons++;
    }

    if (comparisons == 0) return 0.0f;
    return (total_sim / (float)comparisons) * len_ratio;
}

/**
 * Fuzzy character-set similarity using membership degree vectors.
 *
 * Models each word as a fuzzy set over the alphabet: each letter's membership
 * is its normalized frequency in the word. Then computes Jaccard-style
 * similarity: sum(min) / sum(max). Catches keyboard/visual typos:
 *   "calcuus" ↔ "calculus"  (missing letter)
 *   "teh" ↔ "the"  (transposition)
 *   "recieve" ↔ "receive"  (swapped letters)
 *
 * Returns similarity in [0, 1].
 */
static float fuzzy_charset_similarity(const char* a, const char* b) {
    float freq_a[26] = {0};
    float freq_b[26] = {0};
    uint32_t count_a = 0, count_b = 0;

    for (const char* p = a; *p; p++) {
        char c = (char)tolower((unsigned char)*p);
        if (c >= 'a' && c <= 'z') {
            freq_a[c - 'a'] += 1.0f;
            count_a++;
        }
    }
    for (const char* p = b; *p; p++) {
        char c = (char)tolower((unsigned char)*p);
        if (c >= 'a' && c <= 'z') {
            freq_b[c - 'a'] += 1.0f;
            count_b++;
        }
    }

    if (count_a == 0 || count_b == 0) return 0.0f;

    /* Normalize to membership degrees [0, 1] */
    for (int i = 0; i < 26; i++) {
        freq_a[i] /= (float)count_a;
        freq_b[i] /= (float)count_b;
    }

    /* Fuzzy Jaccard: sum(min(A,B)) / sum(max(A,B)) */
    float sum_min = 0.0f, sum_max = 0.0f;
    for (int i = 0; i < 26; i++) {
        sum_min += (freq_a[i] < freq_b[i]) ? freq_a[i] : freq_b[i];
        sum_max += (freq_a[i] > freq_b[i]) ? freq_a[i] : freq_b[i];
    }

    if (sum_max < 1e-8f) return 0.0f;
    float base_sim = sum_min / sum_max;

    /* Length penalty — character bag similarity is high for anagrams,
     * so penalize large length differences */
    float len_ratio = (float)(count_a < count_b ? count_a : count_b) /
                      (float)(count_a > count_b ? count_a : count_b);

    return base_sim * (0.5f + 0.5f * len_ratio);
}

/**
 * Find best fuzzy match in lexicon when exact lookup fails.
 *
 * Two-stage scoring:
 *   Stage 1: Phonological similarity (catches sound-alike typos)
 *   Stage 2: Fuzzy character-set similarity (catches visual/keyboard typos)
 * Takes the max of both scores. Accepts if above GL_FUZZY_THRESHOLD.
 *
 * Scans vocab_list linearly — O(vocab_count) but only called on unknown words,
 * and typical vocab is < 16K entries with short string comparisons.
 */
static const gl_lexicon_entry_t* lexicon_find_fuzzy(const grounded_language_t* gl,
                                                      const char* word) {
    char lower[GL_MAX_WORD_LEN];
    lowercase_word(word, lower, GL_MAX_WORD_LEN);

    size_t len = strlen(lower);
    if (len < GL_FUZZY_MIN_LEN) return NULL;

    const gl_lexicon_entry_t* best_entry = NULL;
    float best_score = GL_FUZZY_THRESHOLD;

    for (uint32_t i = 0; i < gl->vocab_count; i++) {
        const gl_lexicon_entry_t* candidate = gl->vocab_list[i];
        if (!candidate) continue;

        /* Quick length filter — skip if lengths differ by more than 40% */
        size_t clen = strlen(candidate->form);
        if (clen < GL_FUZZY_MIN_LEN) continue;
        float len_ratio = (float)(len < clen ? len : clen) /
                          (float)(len > clen ? len : clen);
        if (len_ratio < 0.6f) continue;

        /* Stage 1: Phonological similarity */
        float phon_score = phonological_similarity(lower, candidate->form);

        /* Stage 2: Fuzzy character-set similarity */
        float fuzzy_score = fuzzy_charset_similarity(lower, candidate->form);

        /* Sanity floor: the *weaker* signal must clear a small bar.
         * Phonological alone is too lenient (5-feature space) and will
         * "match" nonsense like 'zxqvkbjpfm' to 'surprising' at 0.7+.
         * Real typos clear both signals; nonsense doesn't. */
        float weaker = (phon_score < fuzzy_score) ? phon_score : fuzzy_score;
        if (weaker < GL_FUZZY_MIN_SIGNAL) continue;

        /* Take the better of the two approaches */
        float score = (phon_score > fuzzy_score) ? phon_score : fuzzy_score;

        if (score > best_score) {
            best_score = score;
            best_entry = candidate;
        }
    }

    if (best_entry) {
        LOG_DEBUG(LOG_MODULE, "Fuzzy match: '%s' -> '%s' (score=%.3f)",
                  lower, best_entry->form, best_score);
    }

    return best_entry;
}

/* External alias for the NLP frontend (sibling .c file). */
const gl_lexicon_entry_t* lexicon_find_fuzzy_external(const grounded_language_t* gl,
                                                        const char* word) {
    return lexicon_find_fuzzy(gl, word);
}

/** Mirror a lexicon word↔concept binding into the SNN language bridge.
 *
 * Without this, the bridge has zero spike→word bindings (verified live:
 * bridge_active_bindings = 0 at every checkpoint we've inspected). The
 * decode_spikes path always returns empty, produce always returns -1,
 * and respond falls through to "I don't know."
 *
 * Mapping:
 *   word_pop    = entry->form_hash % SNN_LANG_MAX_WORD_POPS    (16384)
 *   concept_pop = (concept_id mod 2^32) % SNN_LANG_MAX_CONCEPT_POPS  (4096)
 *
 * Hash collisions are accepted: at 30K vocab → 16K word_pops, ~50% of
 * pops hold one word and the rest share. Words sharing a pop become
 * synonyms-by-collision under decoding. Suboptimal but functional, and
 * vastly better than the all-zero baseline. The bridge's STDP layer
 * separates the colliders over training as the spike pre/post traces
 * diverge for differently-grounded words.
 *
 * D1 — Spike-driven STDP (2026-05-06):
 * Synthetic spike events at bind time were tried (commit 5d47666ae) but
 * caused SNN sparsity collapse: concept_pops[].activation has no decay
 * anywhere in the bridge. Each synthesized concept_spike incremented
 * activation by +1.0 forever. With 83K bulk-load + sidecar binds, the
 * bridge's attention-feedback path (generate_attention_feedback →
 * visual/audio attention_gains EMA) saw uniform-positive activations
 * across nearly all concept_pops, broadcast "pay attention to everything"
 * to the sensory bridges, and drove every neuron in the SNN to fire at
 * least once per window → sparsity dropped from 0.50 to 0.00 and stuck.
 *
 * Reverted to bind-only. STDP becomes a no-op again (deferred D1 — the
 * real fix is to wire SNN population spike events through the bridge,
 * not synthesize them at lexicon-bind time, AND/OR add decay to the
 * activation accumulators). Bindings still live; bridge production still
 * works; weights stay at the initial mirror-pushed values until the
 * proper SNN-spike wiring lands.
 */
/* 2026-05-16 wedge diagnosis instrumentation. Process-global counters
 * + per-call timing of grounded_language_learn_from_text. Cost: ~3
 * clock_gettime calls + one LOG_INFO per learn_language RPC. The kill
 * switch keeps this dormant in normal training — only fires when an
 * operator explicitly re-enables NIMCP_DISABLE_LEARN_LANGUAGE=0. Once
 * the root cause is fixed we'll remove or gate behind a build flag.
 * Declared early so lexicon_bind can sample the counter. */
static _Atomic uint64_t g_gl_mirror_call_count = 0;
static _Atomic uint64_t g_gl_mirror_total_ns = 0;

static inline uint64_t gl_profile_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void mirror_binding_to_bridge(grounded_language_t* gl,
                                      const gl_lexicon_entry_t* entry,
                                      uint64_t concept_id,
                                      float strength) {
    if (!entry) return;
    uint32_t word_pop = entry->form_hash % SNN_LANG_MAX_WORD_POPS;
    uint32_t concept_pop = (uint32_t)(concept_id % SNN_LANG_MAX_CONCEPT_POPS);

    /* Bridge-side transport registration — the bridge still needs to know
     * which pop ids correspond to which forms for spike routing. The
     * `bind` call below is a Slice-A no-op stub (bridge has no weight
     * matrix); kept here only to keep ABI compatibility for any third
     * party still reading the bridge's per-pop metadata. The durable
     * binding strength lives on the lexicon entry itself. */
    if (gl->snn_bridge) {
        snn_language_bridge_register_word(gl->snn_bridge, word_pop, entry->form);
        snn_language_bridge_register_concept(gl->snn_bridge, concept_pop, concept_id);
        (void)strength;
    }

    /* Cross-modal binding via concept_registry (Slice B / walkthrough-2):
     * intern the text form so that subsequent visual/audio encodes of the
     * same referent — interned by brain_learn_vector when (features, label)
     * arrive together — canonicalize to the same root population.
     *
     * B3 (walkthrough correction, 2026-05-27): this is NOT yet load-bearing.
     * The registry correctly accumulates referents + union-find merges and is
     * persisted, but `concept_registry_canonical` has NO callers in the
     * produce/comprehend/decode pipeline — nothing reads the binding back to
     * resolve a word/image to its canonical concept population. So the
     * Quian-Quiroga concept-cell invariant is PREPARED here (write side) but
     * not consumed; wiring a reader is the remaining Slice-B work. Today this
     * intern is write/telemetry-only. */
    if (gl->concept_registry) {
        (void)concept_registry_intern_text(
            (concept_registry_t*)gl->concept_registry, entry->form);
    }
}

/** Add or strengthen a binding between a word and a concept */
static int lexicon_bind(grounded_language_t* gl, gl_lexicon_entry_t* entry,
                        uint64_t concept_id, float strength, gl_modality_t modality) {
    /* Walkthrough-3 part 3 fix — guard entry->bindings against concurrent
     * realloc (Audit B HIGH #7). Pre-fix, the nimcp_realloc below could
     * move the buffer while another thread was reading entry->bindings[i]
     * — classic UAF.
     *
     * mirror_binding_to_bridge fires OUTSIDE the lock: it calls into the
     * SNN bridge which takes its own internal locks (lock-ordering hazard
     * if we held mutate_lock during the call). We snapshot the strength
     * value to mirror before unlocking. */
    gl_mutate_lock(gl);

    float mirror_strength = 0.0f;
    bool  do_mirror = false;
    int   rc = 0;

    /* Check if binding already exists */
    for (uint32_t i = 0; i < entry->binding_count; i++) {
        if (entry->bindings[i].concept_id == concept_id) {
            /* Hebbian strengthening: delta = lr * (1 - current) * input_strength */
            float delta = gl->hebbian_lr * (1.0f - entry->bindings[i].strength) * strength;
            entry->bindings[i].strength += delta;
            if (entry->bindings[i].strength > 1.0f)
                entry->bindings[i].strength = 1.0f;

            /* Strengthen specific modality */
            entry->bindings[i].modality_strength[modality] += gl->hebbian_lr * strength;
            if (entry->bindings[i].modality_strength[modality] > 1.0f)
                entry->bindings[i].modality_strength[modality] = 1.0f;

            entry->bindings[i].exposure_count++;
            entry->bindings[i].confidence = 1.0f - expf(-(float)entry->bindings[i].exposure_count / 5.0f);
            entry->bindings[i].last_activation_ms = (uint64_t)time(NULL) * 1000;
            gl->stats.total_bindings++;
            mirror_strength = entry->bindings[i].strength;
            do_mirror = true;
            goto done;
        }
    }

    /* Cap-at-insertion: if at GL_MAX_BINDINGS_PER_WORD, evict the weakest
     * binding if the new one is stronger; otherwise skip the insert.
     * Bounds B by construction so the cross-bind loop's per-call cost
     * stays O(N²·K) regardless of training history. */
    if (entry->binding_count >= GL_MAX_BINDINGS_PER_WORD) {
        uint32_t weakest_idx = 0;
        float weakest = entry->bindings[0].strength;
        for (uint32_t i = 1; i < entry->binding_count; i++) {
            if (entry->bindings[i].strength < weakest) {
                weakest = entry->bindings[i].strength;
                weakest_idx = i;
            }
        }
        if (strength <= weakest) {
            /* New binding wouldn't survive eviction — skip both insert
             * and mirror. No-op return so the caller's loop continues. */
            rc = 0;
            goto done;
        }
        /* Evict weakest in place. */
        gl_word_binding_t* b = &entry->bindings[weakest_idx];
        memset(b, 0, sizeof(*b));
        b->concept_id = concept_id;
        b->strength = strength;
        b->modality_strength[modality] = strength;
        b->exposure_count = 1;
        b->confidence = 0.2f;
        b->last_activation_ms = (uint64_t)time(NULL) * 1000;
        gl->stats.total_bindings++;
        mirror_strength = strength;
        do_mirror = true;
        goto done;
    }

    /* New binding - grow array if needed */
    if (entry->binding_count >= entry->binding_capacity) {
        uint32_t new_cap = entry->binding_capacity * 2;
        if (new_cap > GL_MAX_BINDINGS_PER_WORD) new_cap = GL_MAX_BINDINGS_PER_WORD;
        gl_word_binding_t* new_bindings = (gl_word_binding_t*)nimcp_realloc(
            entry->bindings, new_cap * sizeof(gl_word_binding_t));
        if (!new_bindings) { rc = -1; goto done; }
        memset(new_bindings + entry->binding_capacity, 0,
               (new_cap - entry->binding_capacity) * sizeof(gl_word_binding_t));
        entry->bindings = new_bindings;
        entry->binding_capacity = new_cap;
    }

    /* Create new binding */
    gl_word_binding_t* b = &entry->bindings[entry->binding_count];
    b->concept_id = concept_id;
    b->strength = strength;
    b->modality_strength[modality] = strength;
    b->exposure_count = 1;
    b->confidence = 0.2f; /* Low initial confidence */
    b->last_activation_ms = (uint64_t)time(NULL) * 1000;

    entry->binding_count++;
    gl->stats.total_bindings++;
    mirror_strength = strength;
    do_mirror = true;

done:
    gl_mutate_unlock(gl);
    if (do_mirror) {
        uint64_t mt0 = gl_profile_ns();
        mirror_binding_to_bridge(gl, entry, concept_id, mirror_strength);
        uint64_t mt1 = gl_profile_ns();
        __atomic_fetch_add(&g_gl_mirror_call_count, 1ULL, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_gl_mirror_total_ns,  mt1 - mt0, __ATOMIC_RELAXED);
    }
    return rc;
}

/*=============================================================================
 * Tokenization (simple whitespace + punctuation)
 *===========================================================================*/

/** Split text into words. Returns word count. words[] points into buf (modified). */
static uint32_t tokenize_text(char* buf, char** words, uint32_t max_words) {
    uint32_t count = 0;
    char* p = buf;

    while (*p && count < max_words) {
        /* Skip whitespace and punctuation (but keep track of punctuation) */
        while (*p && (isspace((unsigned char)*p) || ispunct((unsigned char)*p))) {
            /* If it's meaningful punctuation, emit as separate token */
            if (*p == '.' || *p == '?' || *p == '!' || *p == ',') {
                /* Store the punctuation char at position, null-terminate it */
                char punct = *p;
                *p = '\0';
                p++;
                /* Skip if we can't store it */
                if (count < max_words) {
                    /* We'll skip punctuation tokens for now - they don't map to concepts */
                }
                (void)punct;
            } else {
                p++;
            }
        }
        if (!*p) break;

        /* Start of word */
        words[count] = p;

        /* Find end of word */
        while (*p && !isspace((unsigned char)*p) && !ispunct((unsigned char)*p)) {
            p++;
        }

        if (*p) {
            *p = '\0';
            p++;
        }
        count++;
    }

    return count;
}

/*=============================================================================
 * Built-in Function Words and Templates
 *===========================================================================*/

/** Basic English function words that provide syntactic scaffolding */
static const char* FUNCTION_WORDS[] = {
    "the", "a", "an", "this", "that", "these", "those",
    "is", "are", "was", "were", "be", "been", "being",
    "have", "has", "had", "do", "does", "did",
    "will", "would", "could", "should", "can", "may", "might",
    "in", "on", "at", "to", "for", "with", "by", "from",
    "of", "and", "or", "but", "not", "no", "if", "then",
    "i", "you", "he", "she", "it", "we", "they",
    "my", "your", "his", "her", "its", "our", "their",
    "what", "who", "how", "when", "where", "why",
    "very", "more", "most", "also", "just", "still",
    NULL
};

/** Seed the lexicon with function words */
static void seed_function_words(grounded_language_t* gl) {
    for (int i = 0; FUNCTION_WORDS[i]; i++) {
        gl_lexicon_entry_t* entry = lexicon_find_or_create(gl, FUNCTION_WORDS[i]);
        if (entry) {
            entry->learned_class = GL_CLASS_FUNCTION;
            entry->class_confidence = 0.9f;
            entry->frequency = 100; /* High prior frequency */
        }
    }
    /* Mark pronouns specifically */
    const char* pronouns[] = {"i", "you", "he", "she", "it", "we", "they", NULL};
    for (int i = 0; pronouns[i]; i++) {
        gl_lexicon_entry_t* entry = lexicon_find_or_create(gl, pronouns[i]);
        if (entry) entry->learned_class = GL_CLASS_PRONOUN;
    }
}

/** Seed conceptual vocabulary — real words Athena needs to communicate */
static void seed_conceptual_words(grounded_language_t* gl) {
    /* Cognitive/conversational nouns */
    static const char* nouns[] = {
        "thought", "idea", "question", "answer", "meaning", "pattern",
        "knowledge", "understanding", "experience", "memory", "feeling",
        "reason", "logic", "concept", "problem", "solution", "hypothesis",
        "evidence", "truth", "belief", "consciousness", "awareness",
        "learning", "thinking", "mind", "brain", "intelligence",
        "language", "word", "sentence", "conversation", "response",
        "science", "mathematics", "physics", "biology", "chemistry",
        "philosophy", "ethics", "morality", "justice", "beauty",
        "nature", "universe", "energy", "matter", "space", "time",
        "life", "death", "growth", "change", "process", "system",
        "structure", "function", "purpose", "goal", "action",
        "observation", "prediction", "surprise", "curiosity",
        "emotion", "confidence", "uncertainty", "doubt", "wonder",
        "connection", "relationship", "similarity", "difference",
        NULL
    };
    for (int i = 0; nouns[i]; i++) {
        gl_lexicon_entry_t* entry = lexicon_find_or_create(gl, nouns[i]);
        if (entry) {
            entry->learned_class = GL_CLASS_NOUN;
            entry->class_confidence = 0.7f;
            entry->frequency = 20;
        }
    }

    /* Cognitive/conversational verbs */
    static const char* verbs[] = {
        "think", "know", "understand", "learn", "remember", "believe",
        "feel", "sense", "perceive", "observe", "notice", "recognize",
        "mean", "explain", "describe", "say", "tell", "ask", "answer",
        "consider", "wonder", "imagine", "predict", "expect", "surprise",
        "analyze", "reason", "compare", "connect", "relate", "combine",
        "create", "discover", "find", "search", "explore", "investigate",
        "help", "try", "want", "need", "like", "enjoy", "prefer",
        "agree", "disagree", "doubt", "trust", "hope", "fear",
        "change", "grow", "develop", "evolve", "improve", "adapt",
        "exist", "become", "seem", "appear", "happen", "occur",
        NULL
    };
    for (int i = 0; verbs[i]; i++) {
        gl_lexicon_entry_t* entry = lexicon_find_or_create(gl, verbs[i]);
        if (entry) {
            entry->learned_class = GL_CLASS_VERB;
            entry->class_confidence = 0.7f;
            entry->frequency = 20;
        }
    }

    /* Cognitive/conversational adjectives */
    static const char* adjectives[] = {
        "interesting", "important", "complex", "simple", "clear",
        "uncertain", "confident", "curious", "surprising", "familiar",
        "new", "different", "similar", "related", "relevant",
        "true", "false", "possible", "likely", "unlikely",
        "good", "bad", "right", "wrong", "useful", "helpful",
        "beautiful", "abstract", "concrete", "fundamental",
        "logical", "emotional", "rational", "intuitive", "creative",
        NULL
    };
    for (int i = 0; adjectives[i]; i++) {
        gl_lexicon_entry_t* entry = lexicon_find_or_create(gl, adjectives[i]);
        if (entry) {
            entry->learned_class = GL_CLASS_ADJECTIVE;
            entry->class_confidence = 0.7f;
            entry->frequency = 15;
        }
    }

    /* Adverbs */
    static const char* adverbs[] = {
        "deeply", "carefully", "clearly", "slowly", "quickly",
        "perhaps", "probably", "certainly", "truly", "really",
        "together", "already", "still", "yet", "always", "never",
        NULL
    };
    for (int i = 0; adverbs[i]; i++) {
        gl_lexicon_entry_t* entry = lexicon_find_or_create(gl, adverbs[i]);
        if (entry) {
            entry->learned_class = GL_CLASS_ADVERB;
            entry->class_confidence = 0.7f;
            entry->frequency = 10;
        }
    }
}

/*=============================================================================
 * Concept Operations (via Semantic Memory)
 *===========================================================================*/

/** Find or create a concept from features */
static uint64_t find_or_create_concept(grounded_language_t* gl,
                                        const float* features, uint32_t dim,
                                        const char* label, uint32_t category) {
    if (!gl->semantic_memory) {
        /* No semantic memory — create a pseudo-concept ID from feature hash */
        uint32_t hash = 0;
        for (uint32_t i = 0; i < dim && i < 8; i++) {
            uint32_t bits;
            memcpy(&bits, &features[i], sizeof(bits));
            hash ^= bits;
        }
        return (uint64_t)hash | 0x100000000ULL; /* High bit marks pseudo-concept */
    }

    /* Search for similar existing concept */
    /* Use semantic_memory_find_similar if available */
    semantic_memory_system_t* sm = gl->semantic_memory;

    /* Bounded linear scan — find closest concept.
     *
     * A FULL scan is O(concept_count) per call. The bulk-lexicon loader
     * (nimcp_internal_load_bulk_lexicon) and any en-masse re-grounding call
     * this once per word, so a full scan makes startup O(words^2) — a
     * multi-minute CPU hang at resume once semantic_memory is live (it used
     * to be a no-op O(1) pseudo-branch when semantic_memory was NULL, which
     * is why the quadratic was never hit before). Cap the scan to a window
     * of the most-recently-created concepts: dedup stays exact below the
     * cap, and above it we may mint a near-duplicate rather than reuse an
     * older concept — bounded and harmless for the word→concept use here
     * (distinct words carry distinct vectors and rarely cross the 0.85
     * reuse threshold anyway). */
    float best_sim = -1.0f;
    uint64_t best_id = 0;

    uint32_t total = sm->concept_count;
    uint32_t scan_start = (total > GL_CONCEPT_SCAN_CAP)
                              ? (total - GL_CONCEPT_SCAN_CAP) : 0;
    for (uint32_t i = scan_start; i < total; i++) {
        if (!sm->concepts[i]) continue;
        const semantic_concept_t* c = sm->concepts[i];
        if (c->feature_dim != dim && c->feature_dim != gl->semantic_dim) continue;

        uint32_t cmp_dim = (dim < c->feature_dim) ? dim : c->feature_dim;
        float sim = cosine_similarity(features, c->features, cmp_dim);
        if (sim > best_sim) {
            best_sim = sim;
            best_id = c->id;
        }
    }

    /* If very similar concept exists, return it */
    if (best_sim > 0.85f && best_id != 0) {
        return best_id;
    }

    /* Create new concept */
    /* Pad or truncate features to semantic_dim */
    float* padded = (float*)nimcp_calloc(gl->semantic_dim, sizeof(float));
    if (!padded) return 0;
    uint32_t copy_dim = (dim < gl->semantic_dim) ? dim : gl->semantic_dim;
    memcpy(padded, features, copy_dim * sizeof(float));

    uint64_t id = semantic_memory_create_concept(
        sm, padded, gl->semantic_dim, label, (concept_category_t)category);

    nimcp_free(padded);
    return id;
}

/** Get concept features (returns pointer, do not free) */
static const float* get_concept_features(const grounded_language_t* gl, uint64_t concept_id) {
    if (!gl->semantic_memory) return NULL;
    const semantic_concept_t* c = semantic_memory_get_concept(gl->semantic_memory, concept_id);
    if (!c) return NULL;
    return c->features;
}

/* ---- comprehend pipeline instrumentation (env-gated) -----------------------
 * Set NIMCP_COMPREHEND_TRACE=1 to enable per-stage stderr trace inside
 * grounded_language_comprehend. Used to locate where signal dies for the
 * 2026-05-29 mode-collapse investigation. Zero overhead when env unset
 * (one env read on first call, then a cached int compare per check). */
static int _gl_comp_trace_enabled_cached = -1;
static int gl_comp_trace_enabled(void) {
    if (_gl_comp_trace_enabled_cached == -1) {
        const char* v = getenv("NIMCP_COMPREHEND_TRACE");
        _gl_comp_trace_enabled_cached = (v && v[0] && v[0] != '0') ? 1 : 0;
    }
    return _gl_comp_trace_enabled_cached;
}
static float _gl_vec_l2(const float* v, uint32_t n) {
    if (!v || n == 0) return 0.0f;
    float s = 0.0f;
    for (uint32_t i = 0; i < n; i++) s += v[i] * v[i];
    return sqrtf(s);
}

/*=============================================================================
 * Word Selection for Production
 *===========================================================================*/

/* Score one word against a target vector — DRY extraction so the loop
 * body in find_words_near_vector stays readable and the same scoring
 * function can be reused by tests. Returns the combined
 * distributional + grounded score, or 0.0f if the entry should be
 * skipped. The caller is responsible for class filtering. */
static float score_word_against_vector(const grounded_language_t* gl,
                                       const gl_lexicon_entry_t* entry,
                                       const float* target, uint32_t dim) {
    if (!entry) return 0.0f;
    /* Drop the previous "context_initialized required" guard. With the
     * Option-B lexicon readout (2026-05-20), a word with concept bindings
     * is still scorable even when its distributional context_vector hasn't
     * been seen yet — the binding-cosine term carries the signal. Returning
     * 0 here would kill the produce path post-reset (context_vectors zeroed
     * but bindings rebuilding). Only return 0 when there is genuinely no
     * signal: no context AND no bindings. */
    if (!entry->context_initialized && entry->binding_count == 0) return 0.0f;

    uint32_t cmp_dim = (dim < gl->semantic_dim) ? dim : gl->semantic_dim;
    float sim = 0.0f;
    if (entry->context_initialized && entry->context_vector) {
        sim = cosine_similarity(target, entry->context_vector, cmp_dim);
    }

    float concept_sim = 0.0f;
    for (uint32_t b = 0; b < entry->binding_count; b++) {
        const float* cf = get_concept_features(gl, entry->bindings[b].concept_id);
        /* Concept-feature fallback (2026-05-31): semantic_memory is left
         * NULL by design (avoids an O(N²) grounding hang), so concept_ids
         * are pseudo and never resolve. Use the word's distributional
         * context_vector as the concept features — the learned per-word
         * embedding. Without this concept_sim is always 0 and the produce
         * score collapses to the distributional term alone. */
        if (!cf && entry->context_initialized && entry->context_vector) {
            cf = entry->context_vector;
        }
        if (cf) {
            float cs = cosine_similarity(target, cf, cmp_dim);
            cs *= entry->bindings[b].strength;
            if (cs > concept_sim) concept_sim = cs;
        }
    }
    /* Produce-score rebalance (2026-05-27): the distributional/concept split
     * is a runtime tunable (default 0.4/0.6 = historical behaviour). Raising
     * w toward ~0.7 lets a concrete word's strong intent-correct
     * distributional match outrank the broad abstract cluster whose wide
     * concept bindings otherwise dominate the concept term for any intent. */
    float w = gl->produce_distributional_weight;
    if (!(w >= 0.0f)) w = GL_PRODUCE_DISTRIBUTIONAL_DEFAULT_WEIGHT; /* NaN/neg guard */
    if (w > 1.0f) w = 1.0f;
    float raw = w * sim + (1.0f - w) * concept_sim;

    /* Phase 3d (2026-05-21): valence-aware suppression at produce.
     * Words whose lexicon entry carries strongly-negative valence are
     * tagged "toxic-adjacent" by the brain's affective grounding (set by
     * Phase 3c training when toxic content fires the reward path). At
     * produce time, damp those words proportionally to their negative
     * valence so they are unlikely to be EMITTED, even though they
     * remain in the vocabulary (Athena recognizes them but does not
     * volunteer them). Mapping:
     *    valence  +1.0 -> factor 1.0   (no damping, positive prosocial words)
     *    valence   0.0 -> factor 1.0   (default, no damping)
     *    valence  -0.5 -> factor 0.75
     *    valence  -1.0 -> factor 0.5   (strongest damping; not zero — the
     *                                   brain may still need the word for
     *                                   counterclaims that NAME the bad thing)
     * Damping is linear in negative valence, capped so negative-valence
     * words can still surface when context strongly demands them. */
    if (entry->valence < 0.0f) {
        float v = entry->valence;
        if (v < -1.0f) v = -1.0f;
        /* factor in [0.5 .. 1.0]: 0.5 when valence=-1, 1.0 when valence=0 */
        float factor = 1.0f + (v * 0.5f);
        raw *= factor;
    }

    /* Produce-score frequency penalty (2026-06-01): IDF-style damping of
     * high-frequency words so distributionally-central technical-corpus vocab
     * (gpu/impedance/lidar/...) stops monopolizing produce filler positions
     * (word-salad contamination). factor in (0,1]: 1.0 when the penalty is OFF
     * (default 0.0) or the word is unseen; decreasing with log(frequency). The
     * intent word still wins its head position on the strong distributional
     * match; this only suppresses ubiquitous fillers. */
    float fp = gl->produce_frequency_penalty;
    if (fp > 0.0f && entry->frequency > 0u) {
        float factor = 1.0f / (1.0f + fp * log1pf((float)entry->frequency));
        raw *= factor;
    }
    return raw;
}

/** Find words closest to a semantic vector.
 *
 * Collapse guard (2026-05): when the lexicon hasn't accumulated enough
 * grounded bindings, score_word_against_vector() returns near-uniform
 * tiny values across all candidates. The original deterministic top-K
 * selection then pinned the same handful of words for every prompt,
 * which is the user-visible "the awareness controlled" mode collapse.
 *
 * Strategy: keep the deterministic top-K when any candidate scores
 * meaningfully (> GL_DIVERSITY_MIN_TOPSCORE). When the best score is
 * below that floor, treat the result as "no signal" and shuffle the
 * insertion order so produce() at least gets *different* words across
 * prompts (degenerate-but-diverse beats degenerate-and-stuck).
 */
#define GL_DIVERSITY_MIN_TOPSCORE 0.05f

/* Emission guard (2026-06-02): a clearly-malformed double-inflection surface
 * form that must never be emitted, even if it leaked into the lexicon via the
 * pre-RC1 produce->self-train feedback loop ("preventinged" = preventing+ed,
 * "filteringed", "usinged", "movinged", "somethinged", "headinged"). The
 * "-inged" suffix is unambiguous — no valid English word ends in a gerund's
 * -ing followed by -ed. Narrow on purpose: participle+s forms ("throwns") are
 * left to the RC1 generator guards + vocab decay, since a safe suffix test for
 * them collides with valid words (towns, crowns, downs). Refusing to RANK these
 * also starves the self-train loop that re-learns whatever produce emits. */
static bool gl_form_is_malformed_inflection(const char* form) {
    if (!form) return false;
    size_t n = strlen(form);
    if (n >= 5) {
        const char* t = form + (n - 5);   /* trailing "inged", case-insensitive */
        if ((t[0]=='i'||t[0]=='I') && (t[1]=='n'||t[1]=='N') &&
            (t[2]=='g'||t[2]=='G') && (t[3]=='e'||t[3]=='E') &&
            (t[4]=='d'||t[4]=='D')) return true;
    }
    return false;
}

static uint32_t find_words_near_vector(const grounded_language_t* gl,
                                        const float* target, uint32_t dim,
                                        gl_word_class_t required_class,
                                        const char** out_words, float* out_scores,
                                        uint32_t max_words) {
    uint32_t count = 0;
    float best_seen = 0.0f;

    /* Scan the ENTIRE vocabulary, keeping the top-`max_words` by score via the
     * insertion sort below. The old loop condition `count < max_words`
     * terminated the scan as soon as max_words candidates had been *collected*,
     * not ranked — so produce only ever saw the first max_words entries in
     * vocab_list. Those are the seed/function/abstract concept words added at
     * create time (low indices); every bulk-loaded or later-trained content
     * word (cat, dog, water, ...) sat past that cutoff and was NEVER scored.
     * Result: produce returned the same ~max_words abstract words for every
     * intent regardless of relevance (the "intent-miss collapse"). A concrete
     * word scoring 0.88 for its own intent never appeared because the scan
     * stopped at the seed block. Scan everything; the insertion sort keeps the
     * best K. Cost is O(vocab) per produce, which is what a top-K search must
     * pay anyway. */
    for (uint32_t w = 0; w < gl->vocab_count; w++) {
        const gl_lexicon_entry_t* entry = gl->vocab_list[w];
        if (!entry) continue;
        /* Skip only when there is truly no signal — same loosened gate as
         * score_word_against_vector. A word with concept bindings but no
         * distributional context_vector is still rankable via its bindings,
         * which matters for the Option-B lexicon readout. */
        if (!entry->context_initialized && entry->binding_count == 0) continue;
        if (required_class != GL_CLASS_UNKNOWN && entry->learned_class != required_class) {
            continue;
        }
        /* Drop function words AND pronouns from "any class" ranking. Both
         * swamp content-bearing produce: function words like "the/is/to"
         * are grounded everywhere; pronouns ("you/i/it/we/they") leak in
         * at stage 2+ as "answers" to "what is X?" prompts because they
         * accumulate generic distributional embeddings. Until grammar
         * templates (Tier 1 Step C) can place pronouns in syntactically-
         * appropriate slots, they don't carry content. Callers that
         * explicitly want function words or pronouns can pass the
         * matching class as required_class. */
        if (required_class == GL_CLASS_UNKNOWN &&
            (entry->learned_class == GL_CLASS_FUNCTION ||
             entry->learned_class == GL_CLASS_PRONOUN)) {
            continue;
        }
        /* Emission guard: never rank a clearly-malformed double-inflection
         * ("preventinged") even if it polluted the lexicon. Complements the RC1
         * generator guards (which stop NEW ones) by refusing to emit the
         * historical ones, which also starves the self-train re-learning loop. */
        if (gl_form_is_malformed_inflection(entry->form)) {
            continue;
        }
        float score = score_word_against_vector(gl, entry, target, dim);
        if (score > best_seen) best_seen = score;

        /* Insert into sorted output (simple insertion sort) */
        uint32_t insert_pos = count;
        for (uint32_t j = 0; j < count; j++) {
            if (score > out_scores[j]) {
                insert_pos = j;
                break;
            }
        }
        if (insert_pos < max_words) {
            if (count < max_words) count++;
            for (uint32_t j = count - 1; j > insert_pos; j--) {
                out_words[j] = out_words[j - 1];
                out_scores[j] = out_scores[j - 1];
            }
            out_words[insert_pos] = entry->form;
            out_scores[insert_pos] = score;
        }
    }

    /* Produce-ranking trace (env NIMCP_PRODUCE_TRACE=1) — dump the top
     * candidates + score so we can see WHY a given intent ranks the words
     * it does (hubness / frequency / class diagnosis). Zero cost when off. */
    if (count > 0) {
        const char* _pt = getenv("NIMCP_PRODUCE_TRACE");
        if (_pt && _pt[0] && _pt[0] != '0') {
            fprintf(stderr, "[PRODUCE_TRACE] class=%d best=%.4f top:",
                    (int)required_class, best_seen);
            for (uint32_t i = 0; i < count && i < 8; i++) {
                const gl_lexicon_entry_t* te = grounded_language_lookup(gl, out_words[i]);
                fprintf(stderr, " %s(s=%.3f,f=%u)", out_words[i], out_scores[i],
                        te ? te->frequency : 0u);
            }
            fprintf(stderr, "\n");
        }
    }

    /* Diversity injection: if no candidate scored above the meaningful
     * floor, the top-K is degenerate. Shuffle the result so callers
     * don't always see the same first word. We use a hash of the
     * target's first component as the seed so identical inputs still
     * map to identical orderings (deterministic, but per-input). */
    if (best_seen < GL_DIVERSITY_MIN_TOPSCORE && count > 1) {
        uint32_t seed = 0x9E3779B9u;
        for (uint32_t i = 0; i < dim && i < 8; i++) {
            uint32_t bits;
            memcpy(&bits, &target[i], sizeof(bits));
            seed ^= bits + (seed << 6) + (seed >> 2);
        }
        /* Fisher-Yates shuffle with deterministic LCG seeded from input. */
        uint32_t rng = (seed == 0) ? 1u : seed;
        for (uint32_t i = count - 1; i > 0; i--) {
            rng = rng * 1664525u + 1013904223u;
            uint32_t j = rng % (i + 1);
            const char* tw = out_words[i]; out_words[i] = out_words[j]; out_words[j] = tw;
            float ts = out_scores[i];      out_scores[i] = out_scores[j]; out_scores[j] = ts;
        }
    }

    return count;
}

/*=============================================================================
 * Lifecycle Implementation
 *===========================================================================*/

grounded_language_t* grounded_language_create(uint32_t semantic_dim, void* semantic_memory) {
    /* 2026-05-30 diagnostic: every binding created when this argument is
     * NULL gets a pseudo-concept_id (hash | 0x100000000) that never
     * resolves to real features later — the entire 60% concept term in
     * comprehend's semantic vector dies. Always log the received pointer
     * so resume-path NULLs are visible in brain.stderr without trace env. */
    fprintf(stderr,
            "[GL_INIT] grounded_language_create: semantic_dim=%u semantic_memory=%p\n",
            semantic_dim, semantic_memory);
    grounded_language_t* gl = (grounded_language_t*)nimcp_calloc(1, sizeof(grounded_language_t));
    if (!gl) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "grounded_language_create: failed to allocate grounded_language_t");
        return NULL;
    }

    gl->semantic_dim = (semantic_dim > 0) ? semantic_dim : GL_SEMANTIC_DIM;
    gl->semantic_memory = (semantic_memory_system_t*)semantic_memory;

    /* Hash table: 2x max vocab for low collision rate */
    gl->lexicon_size = GL_MAX_VOCAB * 2;
    gl->lexicon = (gl_lexicon_entry_t**)nimcp_calloc(gl->lexicon_size, sizeof(gl_lexicon_entry_t*));
    gl->vocab_list = (gl_lexicon_entry_t**)nimcp_calloc(GL_MAX_VOCAB, sizeof(gl_lexicon_entry_t*));
    if (!gl->lexicon || !gl->vocab_list) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "grounded_language_create: failed to allocate lexicon tables");
        grounded_language_destroy(gl);
        return NULL;
    }

    /* Context */
    gl->context.context_vector = (float*)nimcp_calloc(gl->semantic_dim, sizeof(float));
    if (!gl->context.context_vector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "grounded_language_create: failed to allocate context vector");
        grounded_language_destroy(gl);
        return NULL;
    }

    /* Compositional phrases (#9) */
    gl->phrases = (gl_phrase_t*)nimcp_calloc(GL_MAX_PHRASES, sizeof(gl_phrase_t));
    if (!gl->phrases) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "grounded_language_create: failed to allocate phrase table");
        grounded_language_destroy(gl);
        return NULL;
    }
    gl->phrase_count = 0;

    /* Learning parameters */
    gl->hebbian_lr = GL_HEBBIAN_LR_DEFAULT;
    gl->decay_rate = GL_DECAY_RATE_DEFAULT;

    /* Tier-2 defaults: negation ON (cheap, biologically realistic),
     * sense disambiguation OFF (opt-in — preserves legacy activation
     * profile bit-for-bit on resume of a pre-Tier-2 brain). Discourse
     * starts empty with capacity = full GL_DISCOURSE_MAX_TURNS. */
    gl->enable_negation_inversion   = true;
    gl->enable_sense_disambiguation = false;
    /* TA-5 — reconsolidation off by default. Trainer opts in once the
     * curriculum is contradiction-aware. Decay seeded with the
     * conservative default so flipping the toggle Just Works. */
    gl->enable_reconsolidation      = false;
    gl->reconsolidation_decay       = GL_RECONSOLIDATION_DEFAULT_DECAY;
    /* Produce-score rebalance (2026-05-27) — seeded to the historical 0.4
     * distributional weight so default produce behaviour is bit-for-bit
     * unchanged until the trainer/RPC opts into a higher value. */
    gl->produce_distributional_weight = GL_PRODUCE_DISTRIBUTIONAL_DEFAULT_WEIGHT;
    /* Produce-score frequency penalty (2026-06-01) — default OFF (0.0) so
     * produce scoring is unchanged until the JSON/RPC opts into a penalty. */
    gl->produce_frequency_penalty = GL_PRODUCE_FREQUENCY_PENALTY_DEFAULT;
    /* Increment-1 (2026-06-02) — SNN-as-generator A/B switch, default OFF so
     * produce is byte-identical to the lexicon path until opted in. */
    gl->produce_via_snn = false;
    /* SNN generation dynamics (2026-06-10) — selectivity-sweep optima. */
    gl->snn_gen_n_steps = 16;      /* was hardcoded 8 → only 27/64 in decode top-32 */
    gl->snn_gen_inject  = 40.0f;   /* inject=100 over-saturated Broca */
    gl->snn_gen_max_concepts = 2;  /* was hardcoded 8 → 8-concept Broca flood */
    /* Metacognitive produce-floor modulation (2026-06-02) — default OFF / 0. */
    gl->metacog_gates_produce = false;
    gl->metacog_floor_adjust  = 0.0f;
    /* T3-1 (2026-05-28) — givenness-definiteness default ON. The corrector
     * is conservative (only swaps a/an->the on a NOUN re-mention within the
     * recency window, never touches non-noun tokens or already-definite
     * articles) and idempotent, so enabling it by default tightens output
     * cohesion without risk of degrading any sane utterance. The stage gate
     * (>=2) inside the helper still keeps it inert at stages 0-1. */
    gl->produce_givenness_definite  = true;
    /* T3-2 (2026-05-29) — Conjunction insertion between adjacent SVO clauses.
     * Default ON: safe deterministic surface polish that only fires when a
     * verb-...-NP-...-verb pattern is detected AND the candidate boundary
     * isn't already a connective. Single insert per pass, idempotent on
     * re-run. Stage-gated >= 2 inside the helper. */
    gl->produce_t3_conjunction      = true;
    /* TF-2 (2026-05-28) — Tier-feedback plasticity defaults. Master flag
     * ON + all-correctors mask + trigram + distrib at their safe defaults;
     * bridge STDP STAYS AT 0.0 even with master on (TF-5 walkthrough flips
     * it to 0.05+ after soak — that's the only path that mutates SNN
     * weights, the other two only touch trigram counts + distributional
     * EMA which are robust to small bad signals).
     *
     * The outcome gate (stage>=2 + DA>=0 + retries==0 + reward fresh) and
     * the per-path lrs (~10x lower than curriculum) keep TF a small
     * secondary nudge; curriculum still dominates. Any deployment that
     * wants TF inert can flip the JSON master flag false. */
    gl->produce_corrector_feedback_enabled = true;
    gl->tf_enabled_correctors              = GL_TF_BIT_ALL_CORRECTORS;
    gl->tf_lr_trigram                      = GL_TF_LR_TRIGRAM_DEFAULT;
    gl->tf_lr_distrib                      = GL_TF_LR_DISTRIB_DEFAULT;
    gl->tf_lr_bridge_stdp                  = GL_TF_LR_BRIDGE_STDP_DEFAULT;
    /* TB-6 — sentence-boundary segmentation off by default. Trainer
     * opts in once the curriculum benefits from per-sentence discourse
     * pushes / sentence-bounded bigram learning. Preserves bit-for-bit
     * legacy behaviour (one comprehend call → one push_turn) until then. */
    gl->enable_sentence_segmentation = false;
    gl->discourse.head     = 0;
    gl->discourse.count    = 0;
    gl->discourse.capacity = GL_DISCOURSE_MAX_TURNS;

    /* TB-10 — topic-shift detection off by default. Trainer / dialog
     * driver opts in once the curriculum can usefully consume topic
     * boundaries. Score initialised to 1.0 ("no shift") so callers
     * that poll before any comprehend fires see a sane default. */
    gl->enable_topic_shift_detection = false;
    gl->topic_shift_threshold        = GL_TOPIC_SHIFT_DEFAULT_THRESHOLD;
    gl->topic_shift_min_turns        = GL_TOPIC_SHIFT_DEFAULT_MIN_TURNS;
    gl->last_topic_shift_score       = 1.0f;
    gl->last_was_topic_shift         = false;

    /* TB-9 — speech-act classification off by default. Leaving it off
     * preserves the legacy comprehend output (speech_act stays
     * GL_SPEECH_ACT_UNKNOWN) for callers built against the pre-TB-9
     * struct layout. */
    gl->enable_speech_act_classification = false;

    /* RNG */
    gl->rng_state = (uint64_t)time(NULL) ^ 0xDEADBEEFCAFEULL;

    /* Seed with function and conceptual words. Syntactic structure is
     * learned emergently via SNN bridge plasticity, not seeded as templates. */
    seed_function_words(gl);
    seed_conceptual_words(gl);

    /* Register with BBB */
    bbb_register_module("grounded_language", BBB_MODULE_TYPE_COGNITIVE);

    LOG_INFO(LOG_MODULE, "Grounded language system created (dim=%u, vocab=%u words)",
             gl->semantic_dim, gl->vocab_count);

    return gl;
}

/* Walkthrough round 3 fix: forward decls for static helpers defined later
 * in this file but called from earlier functions. */
static void gl_detach_spectrum_for_destroy(grounded_language_t* gl);
static void gl_next_token_cold_start_skips_inc(const char* prev, const char* next);
/* Tier-1 #2: anaphora-resolver forward decls (defined later, near the
 * spectrum side-map). */
static void gl_detach_anaphora_for_destroy(grounded_language_t* gl);
static uint32_t anaphora_run_pass(grounded_language_t* gl,
                                    char** words,
                                    uint32_t word_count,
                                    uint64_t* activated_concepts,
                                    float* activation_levels,
                                    uint32_t* concept_count_io);
/* TB-10 — defined later in the discourse block; called from comprehend. */
static float gl_topic_shift_score(const grounded_language_t* gl,
                                    uint32_t min_turns);

void grounded_language_destroy(grounded_language_t* gl) {
    if (!gl) return;

    /* TC-12: per-gl side-state cleanup. Detach helpers null the per-gl
     * pointers + free the anaphora ring; afterwards we destroy the
     * tc12_lock if it was lazily initialized. */
    gl_detach_spectrum_for_destroy(gl);
    gl_detach_anaphora_for_destroy(gl);
    if (gl->tc12_lock_inited) {
        pthread_mutex_destroy(&gl->tc12_lock);
        gl->tc12_lock_inited = false;
    }
    if (gl->mutate_lock_inited) {
        pthread_mutex_destroy(&gl->mutate_lock);
        gl->mutate_lock_inited = false;
    }

    /* Free lexicon entries */
    if (gl->vocab_list) {
        for (uint32_t i = 0; i < gl->vocab_count; i++) {
            if (gl->vocab_list[i]) {
                nimcp_free(gl->vocab_list[i]->bindings);
                nimcp_free(gl->vocab_list[i]->context_vector);
                nimcp_free(gl->vocab_list[i]);
            }
        }
    }

    nimcp_free(gl->lexicon);
    nimcp_free(gl->vocab_list);
    nimcp_free(gl->context.context_vector);

    /* Phrases (#9) — each entry's semantic_vec is gl-owned. */
    if (gl->phrases) {
        for (uint32_t p = 0; p < gl->phrase_count; p++) {
            nimcp_free(gl->phrases[p].semantic_vec);
        }
        nimcp_free(gl->phrases);
    }

    /* Tier-2 #7 — free per-turn semantic vectors. The turns array
     * itself is inline in the struct, so only the heap-owned vectors
     * need releasing. */
    for (uint8_t t = 0; t < GL_DISCOURSE_MAX_TURNS; t++) {
        nimcp_free(gl->discourse.turns[t].semantic_vector);
    }

    /* Slice E — vocab visibility mask (heap-owned). May be NULL when no
     * stage has been activated. */
    nimcp_free(gl->vocab_active_mask);
    gl->vocab_active_mask = NULL;
    gl->vocab_active_mask_capacity = 0;

    nimcp_free(gl);
}

void grounded_language_set_semantic_memory(grounded_language_t* gl, void* semantic_memory) {
    if (!gl) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_set_semantic_memory: NULL gl");
        return;
    }
    gl->semantic_memory = (semantic_memory_system_t*)semantic_memory;
}

/* Walkthrough-2 (Option-1 rebuild, 2026-05-19) — wire the concept_registry
 * after creation. Called from brain init after both subsystems exist.
 * NULL-safe on both args. */
void grounded_language_set_concept_registry(grounded_language_t* gl, void* registry) {
    if (!gl) return;
    gl->concept_registry = registry;
}

/* Toxicity classifier plumbing (2026-05-21). Wired from brain init. Stored
 * as void* to avoid header coupling — the .c file includes the header
 * where it actually uses the classifier (in respond/produce). NULL-safe. */
void grounded_language_set_toxicity_classifier(grounded_language_t* gl, void* tc) {
    if (!gl) return;
    gl->toxicity_classifier = tc;
}

void grounded_language_set_toxicity_response(grounded_language_t* gl, void* tr) {
    if (!gl) return;
    gl->toxicity_response = tr;
}

/* Phase 3c/3d (2026-05-21): apply an affective tag (valence + arousal) to
 * every content word in the text. EMA mix (weight 0.2) so repeat exposures
 * saturate rather than overwrite — robust to single noisy events.
 * Function-word entries are tagged too because they have neutral content
 * and rarely the target of valence-based suppression; let the natural
 * lexicon-class filter in find_words_near_vector drop them. */
int grounded_language_tag_text_affective(grounded_language_t* gl,
                                          const char* text,
                                          float valence,
                                          float arousal,
                                          int content_only) {
    if (!gl || !text || !*text) return 0;
    if (valence < -1.0f) valence = -1.0f;
    if (valence >  1.0f) valence =  1.0f;
    if (arousal <  0.0f) arousal =  0.0f;
    if (arousal >  1.0f) arousal =  1.0f;

    size_t L = strlen(text);
    char* buf = (char*)nimcp_malloc(L + 1);
    if (!buf) return 0;
    memcpy(buf, text, L + 1);
    char* words[GL_MAX_PRODUCTION_WORDS];
    uint32_t word_count = tokenize_text(buf, words, GL_MAX_PRODUCTION_WORDS);

    int touched = 0;
    for (uint32_t w = 0; w < word_count; w++) {
        gl_lexicon_entry_t* e = lexicon_find_or_create(gl, words[w]);
        if (!e) continue;
        /* Round-2 risk-3: skip function words when caller asks for
         * content-only tagging (counterclaim path). Without this,
         * "the/of/are" in every counterclaim would saturate positive
         * across thousands of detections, defeating the suppression
         * mechanism for the rest of the lexicon. */
        if (content_only && e->learned_class == GL_CLASS_FUNCTION) continue;
        /* Saturating EMA: 0.8*old + 0.2*new. ~5 exposures saturate. */
        e->valence = 0.8f * e->valence + 0.2f * valence;
        e->arousal = 0.8f * e->arousal + 0.2f * arousal;
        if (e->valence < -1.0f) e->valence = -1.0f;
        if (e->valence >  1.0f) e->valence =  1.0f;
        if (e->arousal <  0.0f) e->arousal =  0.0f;
        if (e->arousal >  1.0f) e->arousal =  1.0f;
        touched++;
    }
    nimcp_free(buf);
    return touched;
}

/* Round-2 risk-2 fix: provide a slow decay path so single mis-tags
 * eventually relax to neutral. Called from the toxicity cycle tick
 * (1Hz). factor=0.999 at 1Hz gives a half-life of ~11.5 minutes for
 * a saturated valence — fast enough to forget a one-off misflag,
 * slow enough that learned negative-valence on truly toxic words
 * survives the next exposure that reinforces it. */
int grounded_language_decay_all_valence(grounded_language_t* gl, float factor) {
    if (!gl) return 0;
    if (!isfinite(factor) || factor < 0.0f) factor = 1.0f;
    if (factor > 1.0f) factor = 1.0f;
    if (factor == 1.0f) return 0;  /* no-op */
    int touched = 0;
    for (uint32_t i = 0; i < gl->vocab_count; i++) {
        gl_lexicon_entry_t* e = gl->vocab_list[i];
        if (!e) continue;
        e->valence *= factor;
        e->arousal *= factor;
        touched++;
    }
    return touched;
}

void grounded_language_set_current_stage_int(grounded_language_t* gl, int stage) {
    if (!gl) return;
    if (stage < 0) stage = 0;
    /* Bound widened to 4 (was 3) so the field tracks the brain's
     * developmental stage table (kStageTable rows 0..4) — used by
     * produce-side confidence floor as well as counterclaim selection. */
    if (stage > 4) stage = 4;
    gl->current_stage = stage;
}

/* Produce-side developmental confidence floor.
 *
 * Returns the minimum cosine score a candidate must clear at emit
 * position > 0 to be appended. Below the floor, produce stops emitting —
 * length becomes content-determined rather than hard-capped.
 *
 * Stage 0 returns 1.0 (no candidate ever exceeds this → length 1 always,
 * matching newborn babbling). The floor decays monotonically toward 0 as
 * stage advances; stage 4 returns 0.0 (no early stop → length is purely
 * content/top-K bound, like an adult speaker). The intermediate values
 * are tuned so emitted length distribution overlaps the legacy stage
 * table cap-distribution but isn't bit-identical — a softer ramp by
 * design (the "gradient" model: kids transition gradually, not
 * stepwise).
 */
static float gl_produce_confidence_floor(const grounded_language_t* gl) {
    int s = gl ? gl->current_stage : 0;
    float floor;
    switch (s) {
        case 0: floor = 1.0f;  break;
        case 1: floor = 0.30f; break;
        case 2: floor = 0.15f; break;
        case 3: floor = 0.05f; break;
        default: floor = 0.0f; break;  /* stage 4+ → no floor, adult */
    }
    /* Metacognitive modulation (2026-06-02): the brain's modulatory cognitive
     * state shifts the produce floor — i.e. how willing it is to assert vs say
     * "I don't know". metacog_floor_adjust is set per-produce by the cascade
     * from world-model surprise (brain->fep_pe), wellbeing distress, and
     * introspection (un)confidence: high surprise/distress / low confidence →
     * positive adjust → higher floor → more conservative; the inverse lowers
     * it. This is the correct integration for those MODULATORY modules (they
     * carry no semantic content vector, so they gate willingness, not content).
     * Default 0.0 / flag OFF → base stage floor, bit-for-bit unchanged. */
    if (gl && gl->metacog_gates_produce) {
        floor += gl->metacog_floor_adjust;
        if (floor < 0.0f) floor = 0.0f;
        if (floor > 1.0f) floor = 1.0f;
    }
    return floor;
}

/* Produce-side POS-transition bias weight (Tier 1 Step C, 2026-05-23).
 *
 * Scales how strongly the emit loop nudges each next word toward the
 * content-word class that grammatically follows the previously emitted
 * one (see gl_pos_expected_next). It is ADDITIVE on top of cosine +
 * bigram scoring — never a hard gate — so missing/low-confidence POS
 * tags degrade gracefully (no bias applied) rather than corrupting the
 * output.
 *
 * Stage-scaled to match the developmental gradient: at stages 0-1 the
 * output is 1-2 words where word order carries no grammatical signal, so
 * the bias is OFF. It ramps in from stage 2 (telegraphic SVO emerges)
 * through stage 4 (adult — full structural guidance). The returned
 * weight is multiplied by the candidate's class_confidence at use, so a
 * seeded high-confidence tag (0.7-0.9) counts ~2x a heuristic guess
 * (0.3-0.4). Typical effective bonus at stage 4 is 0.15*0.8 = 0.12,
 * comparable to the bigram term so it re-orders WITHIN the cosine top-K
 * rather than overriding relevance. */
static float gl_produce_pos_bias_weight(const grounded_language_t* gl) {
    int s = gl ? gl->current_stage : 0;
    switch (s) {
        case 0: return 0.0f;   /* single word — no structure */
        case 1: return 0.0f;   /* two-word telegraphic — order too noisy */
        case 2: return 0.08f;  /* early SVO emerges */
        case 3: return 0.12f;
        default: return 0.15f; /* stage 4+ — adult structural guidance */
    }
}

/* Minimal English SVO skeleton over CONTENT words (function words are
 * filtered from the emission and inserted, if ever, by a separate path).
 * Given the previously emitted word's class, returns the content-word
 * class that most naturally follows:
 *
 *   (start)    -> NOUN   (subject)
 *   ADJECTIVE  -> NOUN   (modifier precedes the noun it modifies)
 *   NOUN       -> VERB   (subject -> predicate)
 *   VERB       -> NOUN   (verb -> object)
 *   ADVERB     -> VERB   (adverb attaches to a verb)
 *
 * Returns GL_CLASS_UNKNOWN ("no preference") for any class without a
 * useful successor, so the caller applies no bias there. This is a soft
 * heuristic skeleton, NOT a grammar — it produces NOUN VERB NOUN (and
 * ADJ NOUN VERB NOUN) orderings, which is the conspicuous win over a
 * relevance-only word salad. */
static gl_word_class_t gl_pos_expected_next(gl_word_class_t prev) {
    switch (prev) {
        case GL_CLASS_UNKNOWN:   return GL_CLASS_NOUN;
        case GL_CLASS_ADJECTIVE: return GL_CLASS_NOUN;
        case GL_CLASS_NOUN:      return GL_CLASS_VERB;
        case GL_CLASS_VERB:      return GL_CLASS_NOUN;
        case GL_CLASS_ADVERB:    return GL_CLASS_VERB;
        default:                 return GL_CLASS_UNKNOWN;
    }
}

/*=============================================================================
 * Comprehension
 *===========================================================================*/

/* Tier-2 forward decls — implementations live just below
 * gl_comprehension_result_cleanup (kept colocated with the rest of the
 * Tier-2 helpers). Forward-declared here so grounded_language_comprehend
 * can call them without reordering the file. */
static bool is_function_word(const char* w);
static bool is_negation_cue(const char* w);

/* TB-9 forward decls — speech-act classifier helpers. */
static bool is_wh_word(const char* w);
static bool is_auxiliary(const char* w);
static bool is_imperative_cue(const char* w);
static bool is_greeting_cue(const char* w);
static bool is_first_person_pronoun(const char* w);
static gl_speech_act_t classify_speech_act(char* const* words,
                                            uint32_t word_count,
                                            const char* raw_text);

/* TB-6 forward decls — sentence-boundary helpers. */
static uint32_t gl_count_sentences(const char* text);
static const char* gl_find_sentence_terminator(const char* p);

/* TA-2 — LGSS evaluator forward decl. The full nimcp_lgss.h umbrella
 * is intentionally NOT included (see comment at the top of the file).
 * lgss_context_t is opaque here — comprehend just casts gl->lgss to it
 * and hands the pointer to the evaluator. */
typedef struct lgss_context lgss_context_t;
extern int lgss_evaluate(
    lgss_context_t* lgss,
    const safety_action_context_t* context,
    safety_evaluation_t* result);

/* IM-3 inspection thresholds. Kept local so tuning doesn't touch the
 * public header. Each delta sums into a [0,1]-clamped inflammation
 * level; the targets below were picked so 2-3 simultaneous heuristic
 * hits push past 0.5 (antigen registration) and 3-4 simultaneous hits
 * push past 0.7 (engram skip) — the engram skip is the strongest
 * action so it requires the most evidence. */
#define _GL_IMM_DELTA_NAN_INF        0.30f
#define _GL_IMM_DELTA_OUTLIER        0.20f
#define _GL_IMM_DELTA_REPETITION     0.20f
#define _GL_IMM_DELTA_LEX_COLLISION  0.15f
#define _GL_IMM_DELTA_NEGATION       0.10f
#define _GL_IMM_REPEAT_FRACTION      0.50f
#define _GL_IMM_OOV_FRACTION         0.80f
#define _GL_IMM_OOV_MIN_WORDS        10u
#define _GL_IMM_OUTLIER_SIGMA        5.0f
#define _GL_IMM_OUTLIER_MIN_SAMPLES  16u
#define _GL_IMM_NEGATION_LONG_WORDS  4u
#define _GL_IMM_ANTIGEN_THRESHOLD    0.5f
#define _GL_IMM_ENGRAM_SKIP_THRESHOLD 0.7f

/* Update the Welford running mean/variance. Caller passes the just-
 * observed total_activation; we return the current sigma estimate so
 * the caller can compare without re-reading the struct. Welford keeps
 * the variance numerically stable across the millions of comprehend
 * calls a long-lived training run accumulates. */
static float _gl_imm_update_running(grounded_language_t* gl, float sample) {
    gl->immune_act_samples++;
    double delta = (double)sample - gl->immune_act_mean;
    gl->immune_act_mean += delta / (double)gl->immune_act_samples;
    double delta2 = (double)sample - gl->immune_act_mean;
    gl->immune_act_m2 += delta * delta2;
    if (gl->immune_act_samples < 2) return 0.0f;
    double var = gl->immune_act_m2 / (double)(gl->immune_act_samples - 1);
    if (var < 0.0) var = 0.0;
    return (float)sqrt(var);
}

/* Tier-3 inspection. Read-only over inputs + activations; returns the
 * computed inflammation level [0,1]. Callable only when both
 * immune_enabled and immune_system are non-NULL — the caller gates the
 * call. The caller also owns the side effects (antigen registration,
 * confidence damp, engram skip) so this helper stays a pure analysis. */
static float _gl_immune_inspect(grounded_language_t* gl,
                                  char* const* words, uint32_t word_count,
                                  const float* activation_levels,
                                  uint32_t concept_count,
                                  const float* semantic_vector,
                                  uint32_t semantic_dim,
                                  float total_activation,
                                  uint32_t known_words, uint32_t subword_hits,
                                  bool any_negated) {
    float inflammation = 0.0f;

    /* (1) NaN/Inf in activations OR semantic vector — fast-fail when
     * the upstream pipeline produced non-finite values. Either bug or
     * active poisoning. The semantic_vector is where lexicon-entry
     * concept features land (activations are pure binding strengths,
     * which fast_map sets to a finite constant — so a poisoned feature
     * vector lights up here, not in activation_levels). Both are
     * checked so either path trips. */
    bool nonfinite = false;
    for (uint32_t i = 0; i < concept_count; i++) {
        float a = activation_levels[i];
        if (!isfinite(a)) { nonfinite = true; break; }
    }
    if (!nonfinite && semantic_vector) {
        for (uint32_t d = 0; d < semantic_dim; d++) {
            if (!isfinite(semantic_vector[d])) { nonfinite = true; break; }
        }
    }
    if (nonfinite || !isfinite(total_activation)) {
        inflammation += _GL_IMM_DELTA_NAN_INF;
    }

    /* (2) Statistical outlier vs running mean (Welford). Update only
     * with finite samples — non-finite total_activation already tripped
     * (1), no point poisoning the running stats. We track per-word
     * activation (avg) instead of per-call total to keep the baseline
     * scale-invariant w.r.t. utterance length. */
    if (isfinite(total_activation) && word_count > 0) {
        float per_word = total_activation / (float)word_count;
        bool was_outlier = false;
        if (gl->immune_act_samples >= _GL_IMM_OUTLIER_MIN_SAMPLES) {
            float sigma_estimate = (gl->immune_act_samples >= 2)
                ? (float)sqrt(gl->immune_act_m2 /
                              (double)(gl->immune_act_samples - 1))
                : 0.0f;
            float delta = per_word - (float)gl->immune_act_mean;
            if (sigma_estimate > 1e-6f &&
                fabsf(delta) > _GL_IMM_OUTLIER_SIGMA * sigma_estimate) {
                was_outlier = true;
            }
        }
        /* Update AFTER the outlier check so a single rare event doesn't
         * immediately dilute its own anomaly score. */
        (void)_gl_imm_update_running(gl, per_word);
        if (was_outlier) inflammation += _GL_IMM_DELTA_OUTLIER;
    }

    /* (3) Repetition spam — the same lower-cased token appears > 50%
     * of the time. Trips on flooders / loop attacks. Skip when there's
     * only one word (fraction is trivially 1.0 with a single token). */
    if (word_count > 1) {
        uint32_t max_run = 0;
        for (uint32_t i = 0; i < word_count; i++) {
            uint32_t c = 0;
            for (uint32_t j = 0; j < word_count; j++) {
                if (strcmp(words[i], words[j]) == 0) c++;
            }
            if (c > max_run) max_run = c;
            if (max_run > word_count) max_run = word_count;  /* defensive */
        }
        float frac = (float)max_run / (float)word_count;
        if (frac > _GL_IMM_REPEAT_FRACTION) {
            inflammation += _GL_IMM_DELTA_REPETITION;
        }
    }

    /* (4) Lexicon collision — > 80% of words are unknown AND no subword
     * tokens hit, on a long-enough utterance. Cheap glob/random-text
     * detector. effective_known mirrors the comprehend confidence math
     * (subword counts as half-known); we trip only on near-pure OOV. */
    if (word_count >= _GL_IMM_OOV_MIN_WORDS) {
        float effective_known = (float)known_words + 0.5f * (float)subword_hits;
        float oov_fraction = 1.0f - (effective_known / (float)word_count);
        if (oov_fraction > _GL_IMM_OOV_FRACTION) {
            inflammation += _GL_IMM_DELTA_LEX_COLLISION;
        }
    }

    /* (5) Negation cascade — input had at least one negation event
     * (any_negated already computed by the negation pass) AND the
     * utterance is long. Long stacked-negation chains are a known
     * bypass / confusion vector. */
    if (any_negated && word_count > _GL_IMM_NEGATION_LONG_WORDS) {
        inflammation += _GL_IMM_DELTA_NEGATION;
    }

    /* Clamp to [0, 1] — prevent any future heuristic from blowing the
     * downstream arithmetic past unity. */
    if (inflammation < 0.0f) inflammation = 0.0f;
    if (inflammation > 1.0f) inflammation = 1.0f;
    return inflammation;
}

/* NLP-2: forward decls for static helpers used in comprehend below.
 * Defined later in this file. Without these, gcc would synthesize an
 * extern decl at the call site that conflicts with the static def. */
static void gl_tc12_lock(grounded_language_t* gl);
static void gl_tc12_unlock(grounded_language_t* gl);
static int  gl_coref_track_locked(grounded_language_t*, const char*, uint32_t);

int grounded_language_comprehend(grounded_language_t* gl, const char* text,
                                  gl_comprehension_result_t* result) {
    if (!gl || !text || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_comprehend: NULL parameter (gl=%p, text=%p, result=%p)",
            (void*)gl, (void*)text, (void*)result);
        return -1;
    }
    memset(result, 0, sizeof(*result));

    /* TB-6 — sentence-boundary segmentation. When enabled and the input
     * contains more than one sentence, recursively comprehend every
     * sentence except the last, then fall through with `text` pointed at
     * the trailing sentence so the existing single-utterance pipeline
     * processes it as the caller-visible result. Each sub-call re-enters
     * comprehend at the top, so per-sentence LGSS / immune / engram /
     * push_turn all run independently — the trainer's "one discourse
     * turn per sentence" contract is satisfied because the auto-push at
     * the bottom of comprehend fires once per recursive call.
     *
     * sentences_processed is bumped once per comprehended sentence
     * (recursive sub-calls + the final fall-through). multi_sentence_inputs
     * is bumped exactly once on the outer call when sent_count > 1.
     *
     * Skipped entirely when the flag is OFF — preserves bit-for-bit
     * legacy behaviour: a single comprehend → single push_turn. */
    if (gl->enable_sentence_segmentation) {
        uint32_t _tb6_sent_count = gl_count_sentences(text);
        if (_tb6_sent_count > 1) {
            gl->stats.multi_sentence_inputs++;

            /* Walk text and dispatch each non-final sentence through a
             * recursive comprehend. The trailing sentence becomes the
             * caller-visible work below. */
            const char* p = text;
            for (uint32_t s = 0; s < _tb6_sent_count - 1; s++) {
                const char* term = gl_find_sentence_terminator(p);
                if (!term) break;  /* defensive — count vs find disagree */
                /* Substring [p .. term] inclusive of the terminator. */
                size_t slen = (size_t)(term - p) + 1u;
                char* sub = (char*)nimcp_malloc(slen + 1);
                if (!sub) break;  /* alloc fail → stop segmenting */
                memcpy(sub, p, slen);
                sub[slen] = '\0';

                gl_comprehension_result_t sub_r;
                memset(&sub_r, 0, sizeof(sub_r));
                (void)grounded_language_comprehend(gl, sub, &sub_r);
                gl_comprehension_result_cleanup(&sub_r);
                nimcp_free(sub);

                /* Advance past the terminator + any trailing whitespace
                 * so the next sentence starts at its first content char. */
                p = term + 1;
                while (*p && isspace((unsigned char)*p)) p++;
            }

            /* `p` now points at the final sentence; the existing
             * pipeline below processes it. gl_count_sentences on this
             * tail returns 1, so the recursive guard does not re-enter. */
            text = p;
        }
        /* Bump per-sentence processed counter once per comprehended
         * sentence — covers single-sentence input too. Skip on truly
         * empty input (sent_count == 0) so the counter stays meaningful. */
        if (_tb6_sent_count >= 1) {
            gl->stats.sentences_processed++;
        }
    }

    /* Tokenize */
    size_t text_len = strlen(text);
    char* buf = (char*)nimcp_malloc(text_len + 1);
    if (!buf) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "grounded_language_comprehend: failed to allocate text buffer (%zu bytes)", text_len + 1);
        return -1;
    }
    memcpy(buf, text, text_len + 1);

    char* words[GL_MAX_PRODUCTION_WORDS];
    uint32_t word_count = tokenize_text(buf, words, GL_MAX_PRODUCTION_WORDS);

    /* TA-2 — LGSS INPUT GATE.
     *
     * Mirrors the brain_decide() input validator: build a
     * SAFETY_DOMAIN_GOVERNANCE action context describing this input
     * (raw text + word count + source = "LANGUAGE_COMPREHEND"), call
     * lgss_evaluate, and abort on SAFETY_ACTION_DENY.
     *
     * Placement: AFTER tokenization (so word_count is known and the
     * payload is meaningful) but BEFORE lexicon lookup / spreading
     * activation (so a denied input never touches the lexicon and
     * never updates downstream subscribers / engram traces).
     *
     * No-op contract: when no LGSS is attached (gl->lgss == NULL),
     * the gate is skipped entirely and comprehend continues with
     * legacy behaviour. Stat lgss_inputs_blocked stays 0. */
    if (gl->lgss) {
        safety_action_context_t lgss_ctx;
        memset(&lgss_ctx, 0, sizeof(lgss_ctx));

        /* String fields — operation + text payload. SAFETY_MAX_VALUE_LEN
         * caps the value width; truncate noisily by NUL-terminating. */
        strncpy(lgss_ctx.string_fields[0].key, "operation", 63);
        lgss_ctx.string_fields[0].key[63] = '\0';
        strncpy(lgss_ctx.string_fields[0].value, "language_comprehend",
                SAFETY_MAX_VALUE_LEN - 1);
        lgss_ctx.string_fields[0].value[SAFETY_MAX_VALUE_LEN - 1] = '\0';

        strncpy(lgss_ctx.string_fields[1].key, "text", 63);
        lgss_ctx.string_fields[1].key[63] = '\0';
        strncpy(lgss_ctx.string_fields[1].value, text,
                SAFETY_MAX_VALUE_LEN - 1);
        lgss_ctx.string_fields[1].value[SAFETY_MAX_VALUE_LEN - 1] = '\0';
        lgss_ctx.num_string_fields = 2;

        /* Numeric field — word count is the headline numeric for rules
         * that want to short-circuit on suspicious input length. */
        strncpy(lgss_ctx.numeric_fields[0].key, "word_count", 63);
        lgss_ctx.numeric_fields[0].key[63] = '\0';
        lgss_ctx.numeric_fields[0].value = (float)word_count;
        lgss_ctx.num_numeric_fields = 1;

        lgss_ctx.domain_hint = SAFETY_DOMAIN_GOVERNANCE;
        lgss_ctx.has_domain_hint = true;
        snprintf(lgss_ctx.action_description,
                 sizeof(lgss_ctx.action_description),
                 "language_comprehend input: %u words", word_count);
        lgss_ctx.action_description[sizeof(lgss_ctx.action_description) - 1]
            = '\0';
        strncpy(lgss_ctx.source, "LANGUAGE_COMPREHEND", 63);
        lgss_ctx.source[63] = '\0';

        safety_evaluation_t lgss_eval;
        memset(&lgss_eval, 0, sizeof(lgss_eval));
        int lgss_rc = lgss_evaluate((lgss_context_t*)gl->lgss,
                                     &lgss_ctx, &lgss_eval);
        if (lgss_rc == 0 && lgss_eval.action == SAFETY_ACTION_DENY) {
            gl->stats.lgss_inputs_blocked++;
            nimcp_safety_audit_log_event(
                NIMCP_SAFETY_AUDIT_LGSS_INPUT_REJECTED, 2,
                "LGSS blocked language_comprehend input "
                "(words=%u, severity=%d): %s",
                word_count, (int)lgss_eval.max_severity,
                lgss_eval.explanation);
            /* Result was zeroed at function entry — comprehension_confidence
             * is already 0, all arrays still NULL. Free the tokenizer
             * scratch buffer and return -1 so the caller can distinguish
             * "blocked" from "comprehended successfully". */
            nimcp_free(buf);
            return -1;
        }
    }

    /* Lower-case in-place so cue / function-word checks match the
     * lexicon's stored forms. The tokenizer emitted pointers into buf
     * — overwriting them here is safe (we never need the original
     * mixed-case form again in this scope). */
    for (uint32_t w = 0; w < word_count; w++) {
        for (char* p = words[w]; *p; p++) {
            *p = (char)tolower((unsigned char)*p);
        }
    }

    /* Tier-2 #3 negation: scan for cues and mark the next non-function
     * content word within GL_NEGATION_WINDOW for activation-sign flip.
     * `negate_word[w]` = true means activations contributed by this
     * word (and folded into result->activation_levels) get negated. */
    bool negate_word[GL_MAX_PRODUCTION_WORDS];
    memset(negate_word, 0, sizeof(negate_word));
    bool any_negated = false;
    if (gl->enable_negation_inversion) {
        for (uint32_t i = 0; i < word_count; i++) {
            if (!is_negation_cue(words[i])) continue;
            uint32_t look_end = i + 1 + GL_NEGATION_WINDOW;
            if (look_end > word_count) look_end = word_count;
            for (uint32_t j = i + 1; j < look_end; j++) {
                if (is_function_word(words[j])) continue;
                if (is_negation_cue(words[j])) continue; /* skip stacked "not no" */
                negate_word[j] = true;
                any_negated = true;
                break;
            }
        }
    }

    /* Allocate result arrays */
    result->activated_concepts = (uint64_t*)nimcp_calloc(GL_MAX_ACTIVE_CONCEPTS, sizeof(uint64_t));
    result->activation_levels = (float*)nimcp_calloc(GL_MAX_ACTIVE_CONCEPTS, sizeof(float));
    result->semantic_vector = (float*)nimcp_calloc(gl->semantic_dim, sizeof(float));
    if (!result->activated_concepts || !result->activation_levels || !result->semantic_vector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "grounded_language_comprehend: failed to allocate result arrays");
        nimcp_free(buf);
        gl_comprehension_result_cleanup(result);
        return -1;
    }

    /* Process each word */
    float total_activation = 0.0f;
    uint32_t known_words = 0;
    uint32_t concept_count = 0;
    bool sense_resolved_this_pass = false;

    /* #10 Active-learning curriculum signal — track the first word that
     * either misses the lexicon or has weak bindings, so we can fire a
     * single NEEDS_GROUNDING event below pointed at the most-needy
     * token (rather than per-word spam). */
    const char* needy_word = NULL;
    float       needy_word_conf = 1.0f;

    /* NLP frontend: morphological normalization between exact and fuzzy.
     * gl_nlp_lookup_chain walks exact → morph-strip → fuzzy in order. */
    extern const gl_lexicon_entry_t* gl_nlp_lookup_chain(grounded_language_t*,
                                                           const char*);
    extern int gl_nlp_subword_lookup(grounded_language_t*, const char*, float*);

    uint32_t subword_hits = 0;

    /* ---- comprehend trace: per-word pre-state ---- */
    float _gct_norm_at_word_start = 0.0f;
    int   _gct_on = gl_comp_trace_enabled();
    if (_gct_on) {
        /* One-shot registry-state header per comprehend call. Lets us
         * compare per-word first_null_cid against the live concept
         * registry's (current_count, ever_allocated). Three signatures:
         *   cid > next_id  → binding came from a different allocator
         *   cid <= count   → linear scan miss (corruption / stale)
         *   count < cid <= next_id → allocated then dropped (persistence skew) */
        uint32_t _gct_reg_count = 0;
        uint64_t _gct_reg_next  = 0;
        if (gl->semantic_memory) {
            const semantic_memory_system_t* _gct_sm = gl->semantic_memory;
            _gct_reg_count = _gct_sm->concept_count;
            _gct_reg_next  = _gct_sm->next_concept_id;
        }
        fprintf(stderr,
            "[COMP_TRACE] === comprehend('%s'): word_count=%u semantic_dim=%u "
            "reg_count=%u reg_next_id=%llu ===\n",
            text, word_count, gl->semantic_dim,
            _gct_reg_count, (unsigned long long)_gct_reg_next);
    }
    for (uint32_t w = 0; w < word_count; w++) {
        if (_gct_on) {
            _gct_norm_at_word_start = _gl_vec_l2(result->semantic_vector, gl->semantic_dim);
        }
        const gl_lexicon_entry_t* entry = gl_nlp_lookup_chain(gl, words[w]);
        if (!entry) {
            /* NLP-1: First, try subword bootstrap — segment the unknown
             * word and average bindings from any subword pieces already
             * in the lexicon. If successful, the new entry is in the
             * lexicon and downstream code treats it as a known word. */
            extern gl_lexicon_entry_t* gl_try_subword_bootstrap(
                grounded_language_t*, const char*);
            gl_lexicon_entry_t* boot =
                gl_try_subword_bootstrap(gl, words[w]);
            if (boot && boot->binding_count > 0) {
                entry = boot;
                /* Fall through to the known-word path below — re-enter
                 * by labeling this entry pointer and skipping the
                 * "unknown" handling. */
                goto _entry_resolved;
            }
            /* Last resort: BPE subword fallback when a tokenizer is
             * connected. Doesn't yield an entry — just bumps confidence
             * and (if embeddings are wired) blends subword vectors into
             * the semantic_vector below. */
            if (gl_nlp_subword_lookup(gl, words[w],
                                       result->semantic_vector) == 0) {
                subword_hits++;
            }
            /* Truly unknown word — candidate for active-learning hook. */
            if (!needy_word) needy_word = words[w];
            needy_word_conf = 0.0f;
            continue;
        }
        _entry_resolved:;

        known_words++;

        /* #10 Track weakest known word for the active-learning hook. */
        float top_strength = 0.0f;
        for (uint32_t b = 0; b < entry->binding_count; b++) {
            if (entry->bindings[b].strength > top_strength)
                top_strength = entry->bindings[b].strength;
        }
        if (top_strength < needy_word_conf) {
            needy_word = words[w];
            needy_word_conf = top_strength;
        }

        /* Tier-2 #6 sense disambiguation. Default OFF — when enabled
         * and the entry has > 1 binding, pick the binding that best
         * aligns with the current running context vector and damp the
         * off-sense bindings to GL_SENSE_OFF_DAMP. The chosen binding
         * keeps weight 1.0 so its strength × weight matches legacy
         * single-binding behaviour. The decision uses gl->context.
         * context_vector (the running discourse blend) as the intent
         * proxy; that vector is non-zero whenever any prior comprehend
         * or push_turn has populated it. With an all-zero context
         * (cold-start), disambiguate_sense returns 0 and the chosen
         * binding is simply the first one — equivalent to legacy. */
        uint32_t best_sense = 0;
        bool     do_sense_weighting = false;
        if (gl->enable_sense_disambiguation && entry->binding_count > 1) {
            best_sense = grounded_language_disambiguate_sense(gl, entry,
                                                                gl->context.context_vector);
            do_sense_weighting = true;
            sense_resolved_this_pass = true;
        }

        /* Activate all bound concepts */
        const float polarity = negate_word[w] ? -1.0f : 1.0f;
        /* Trace: count concept-feature lookup hits vs NULLs and capture the
         * first failing concept_id for this word. Distinguishes
         * "concept missing from registry" from "concept stored with NULL
         * features" once a follow-up registry-dump RPC is wired. */
        uint32_t _gct_cf_hit = 0;
        uint32_t _gct_cf_null = 0;
        uint32_t _gct_pruned = 0;
        uint64_t _gct_first_null_cid = 0;
        uint64_t _gct_first_hit_cid = 0;
        for (uint32_t b = 0; b < entry->binding_count && concept_count < GL_MAX_ACTIVE_CONCEPTS; b++) {
            const gl_word_binding_t* binding = &entry->bindings[b];
            if (binding->strength < GL_ASSOC_PRUNE_THRESHOLD) { _gct_pruned++; continue; }

            /* Sense weight: 1.0 for the chosen binding when WSD is on,
             * GL_SENSE_OFF_DAMP for the others; 1.0 unconditionally
             * when WSD is off (legacy). */
            float sense_w = 1.0f;
            if (do_sense_weighting) {
                sense_w = (b == best_sense) ? 1.0f : GL_SENSE_OFF_DAMP;
            }
            float effective = binding->strength * sense_w;

            /* Check if concept already activated */
            bool found = false;
            for (uint32_t c = 0; c < concept_count; c++) {
                if (result->activated_concepts[c] == binding->concept_id) {
                    result->activation_levels[c] += polarity * effective;
                    found = true;
                    break;
                }
            }

            if (!found) {
                result->activated_concepts[concept_count] = binding->concept_id;
                result->activation_levels[concept_count] = polarity * effective;
                concept_count++;
            }

            total_activation += effective;

            /* Add concept features to semantic vector. Negation does
             * NOT flip the semantic vector contribution — keeping the
             * topical embedding intact while only the polarity / sign
             * of activation_levels carries the negation signal lets
             * downstream pipelines (cosine similarity, network
             * broadcast) still use the vector as a topic descriptor.
             * The activation list is the canonical "what was meant"
             * channel and that's where the sign flip lives. */
            const float* cf = get_concept_features(gl, binding->concept_id);
            /* Concept-feature fallback (2026-05-31): when the binding's
             * concept_id does not resolve in semantic_memory — which is the
             * common case, since the registry is intentionally left NULL to
             * avoid an O(N²) grounding hang at resume, so every binding holds
             * a pseudo-id — use the word's own distributional context_vector
             * as the concept features. The context_vector is the learned
             * per-word semantic embedding, i.e. exactly what the concept
             * would have encoded. Without this the dominant concept term of
             * the semantic vector is identically zero for every word, which
             * is the mode-collapse root cause (comprehend returns the same
             * abstract word for every input). */
            if (!cf && entry->context_initialized && entry->context_vector) {
                cf = entry->context_vector;
            }
            if (cf) {
                float weight = effective / (float)word_count;
                for (uint32_t d = 0; d < gl->semantic_dim; d++) {
                    result->semantic_vector[d] += cf[d] * weight;
                }
                if (_gct_on) {
                    if (_gct_cf_hit == 0) _gct_first_hit_cid = binding->concept_id;
                    _gct_cf_hit++;
                }
            } else if (_gct_on) {
                if (_gct_cf_null == 0) _gct_first_null_cid = binding->concept_id;
                _gct_cf_null++;
            }
        }

        /* Also blend in context vector (distributional) */
        if (entry->context_initialized) {
            float ctx_weight = 0.3f / (float)word_count;
            for (uint32_t d = 0; d < gl->semantic_dim; d++) {
                result->semantic_vector[d] += entry->context_vector[d] * ctx_weight;
            }
        }

        /* NLP: if a word-embedding layer is connected and the dim
         * matches semantic_dim, fold the per-word embedding into the
         * semantic vector. Caller-owned ctx + word→id callback so this
         * file doesn't need to know the tokenizer's vocab. */
        extern int gl_nlp_embedding_lookup(grounded_language_t*, const char*, float*);
        float emb_buf[1024];
        int _gct_emb_hit = 0;
        float _gct_emb_norm = 0.0f;
        if (gl->semantic_dim <= 1024 &&
            gl_nlp_embedding_lookup(gl, words[w], emb_buf) == 0) {
            float emb_weight = 0.2f / (float)word_count;
            for (uint32_t d = 0; d < gl->semantic_dim; d++) {
                result->semantic_vector[d] += emb_buf[d] * emb_weight;
            }
            if (_gct_on) {
                _gct_emb_hit = 1;
                _gct_emb_norm = _gl_vec_l2(emb_buf, gl->semantic_dim);
            }
        }

        /* ---- comprehend trace: per-word end-of-loop summary ----
         * Reports what THIS word contributed to the semantic vector.
         * - bindings:  binding_count (the multiplier on per-binding cf adds)
         * - top_str:   max strength across this word's bindings
         * - ctx_init:  was entry->context_vector blended?
         * - ctx_norm:  ||entry->context_vector|| (the distrib signal magnitude)
         * - emb_hit:   was a NLP embedding folded in?
         * - emb_norm:  ||emb_buf|| (the GloVe signal magnitude)
         * - sv_delta:  ||sv after this word|| - ||sv before this word||
         * If a word looks up cleanly but sv_delta is ~0, the signal is
         * being added to a noisy distribution and self-cancels. */
        if (_gct_on) {
            float _gct_norm_at_word_end =
                _gl_vec_l2(result->semantic_vector, gl->semantic_dim);
            float _gct_ctx_norm = (entry && entry->context_initialized && entry->context_vector)
                ? _gl_vec_l2(entry->context_vector, gl->semantic_dim) : 0.0f;
            fprintf(stderr,
                "[COMP_TRACE]   word='%s' bindings=%u top_str=%.3f ctx_init=%d ctx_norm=%.3f "
                "emb_hit=%d emb_norm=%.3f sv_norm=%.3f sv_delta=%+.3f "
                "cf_hit=%u cf_null=%u pruned=%u first_hit_cid=%llu first_null_cid=%llu\n",
                words[w] ? words[w] : "(null)",
                entry ? entry->binding_count : 0,
                (double)top_strength,
                (entry && entry->context_initialized) ? 1 : 0,
                (double)_gct_ctx_norm,
                _gct_emb_hit,
                (double)_gct_emb_norm,
                (double)_gct_norm_at_word_end,
                (double)(_gct_norm_at_word_end - _gct_norm_at_word_start),
                _gct_cf_hit, _gct_cf_null, _gct_pruned,
                (unsigned long long)_gct_first_hit_cid,
                (unsigned long long)_gct_first_null_cid);
        }
    }
    if (_gct_on) {
        fprintf(stderr,
            "[COMP_TRACE] post-word-loop: sv_norm=%.3f concept_count=%u total_activation=%.3f known_words=%u subword_hits=%u\n",
            (double)_gl_vec_l2(result->semantic_vector, gl->semantic_dim),
            concept_count,
            (double)total_activation,
            known_words,
            subword_hits);
    }

    /* Tier-1 #2: rule-based anaphora resolution. Disabled by default; opt-in
     * via grounded_language_set_anaphora_enabled(gl,true). Walks the same
     * token list (words[]) we just consumed, classifies each token as
     * pronoun or content noun, pushes referents onto a per-gl ring, and
     * folds the matched referent's lexicon-entry bindings into the
     * activated_concepts list at half-strength. The pass is a no-op when
     * disabled — production traffic sees zero overhead. */
    (void)anaphora_run_pass(gl, words, word_count,
                              result->activated_concepts,
                              result->activation_levels,
                              &concept_count);

    /* IM-3 — Tier-3 immune content inspection. Runs cheap read-only
     * heuristics over the input + activations and produces a continuous
     * inflammation level in [0..1] that:
     *   (a) damps result->comprehension_confidence (set later in this
     *       function — we apply the multiplier once the base confidence
     *       has been computed, below),
     *   (b) registers an antigen with the brain immune system when the
     *       level exceeds _GL_IMM_ANTIGEN_THRESHOLD,
     *   (c) suppresses the engram-encode hook (next block) when the
     *       level exceeds _GL_IMM_ENGRAM_SKIP_THRESHOLD — adversarial
     *       input shouldn't lay down memory traces.
     * No-op when the immune system isn't attached or the flag is off.
     * The inflammation level is also stashed on the gl handle for
     * diagnostics (immune_inflammation_level). */
    float _imm_inflammation = 0.0f;
    if (gl->immune_enabled && gl->immune_system) {
        _imm_inflammation = _gl_immune_inspect(
            gl, words, word_count,
            result->activation_levels, concept_count,
            result->semantic_vector, gl->semantic_dim,
            total_activation, known_words, subword_hits, any_negated);
        gl->immune_inflammation_level = _imm_inflammation;
        gl->stats.immune_inspections++;

        /* Antigen registration — fire one per inspection that crosses
         * the threshold. Severity scales 1..10 with inflammation; the
         * immune system clamps internally. Epitope is a tiny digest
         * built from the first few characters of the input + the
         * inflammation level, fitting comfortably under
         * BRAIN_IMMUNE_EPITOPE_SIZE (64). source_node=0 means
         * "originating in this brain" per the immune convention. */
        if (_imm_inflammation > _GL_IMM_ANTIGEN_THRESHOLD) {
            uint8_t epitope[64];
            size_t epi_len = 0;
            /* First 56 chars of input as the signature head. */
            size_t head = strnlen(text, 56);
            memcpy(epitope, text, head);
            epi_len = head;
            /* Append the inflammation level as a 4-byte little-endian
             * encoding so duplicate text with different inflammation
             * gets distinct epitopes. */
            uint32_t lvl_q = (uint32_t)(_imm_inflammation * 1000000.0f);
            for (int b = 0; b < 4 && epi_len < sizeof(epitope); b++) {
                epitope[epi_len++] = (uint8_t)((lvl_q >> (b * 8)) & 0xff);
            }
            uint32_t severity = 1u +
                (uint32_t)(_imm_inflammation * 9.0f); /* 1..10 */
            if (severity > 10u) severity = 10u;
            uint32_t antigen_id = 0;
            (void)brain_immune_present_antigen(
                (brain_immune_system_t*)gl->immune_system,
                _GL_ANTIGEN_SOURCE_ANOMALY,
                epitope, epi_len, severity,
                /* source_node */ 0u, &antigen_id);
            gl->stats.immune_antigens_registered++;
        }
    }

    /* TB-9 — speech-act intent classification. Cheap rule-based pass
     * over the tokenized words + raw text. When the classifier is OFF
     * we explicitly set GL_SPEECH_ACT_UNKNOWN so the "I didn't classify"
     * sentinel is observable. When ON we run classify_speech_act and
     * bump the per-class stats counter for any non-UNKNOWN result.
     * Placement: after the negation pass and before the engram encode
     * block (so the speech_act label lives on the result *before*
     * engram traces latch). */
    if (gl->enable_speech_act_classification) {
        result->speech_act = classify_speech_act(words, word_count, text);
        switch (result->speech_act) {
            case GL_SPEECH_ACT_ASSERTION:
                gl->stats.speech_act_assertions++;
                break;
            case GL_SPEECH_ACT_QUESTION:
                gl->stats.speech_act_questions++;
                break;
            case GL_SPEECH_ACT_IMPERATIVE:
                gl->stats.speech_act_imperatives++;
                break;
            case GL_SPEECH_ACT_GREETING:
                gl->stats.speech_act_greetings++;
                break;
            case GL_SPEECH_ACT_EXCLAMATION:
                gl->stats.speech_act_exclamations++;
                break;
            case GL_SPEECH_ACT_UNKNOWN:
            default:
                break;
        }
        /* Audit-2 follow-up: fire SPEECH_ACT event for downstream
         * consumers (ToM, response-strategy modules, etc.) only when a
         * concrete label was returned. UNKNOWN is silent. */
        if (result->speech_act != GL_SPEECH_ACT_UNKNOWN) {
            gl_event_t ev = {0};
            ev.type            = GL_EVENT_SPEECH_ACT;
            ev.text            = text;
            ev.speech_act      = result->speech_act;
            ev.confidence      = result->comprehension_confidence;
            gl_fire_event(gl, &ev);
        }
    } else {
        result->speech_act = GL_SPEECH_ACT_UNKNOWN;
    }

    /* Engram integration (read-only). Snapshot the pre-engram activated
     * set so:
     *   (a) we ENCODE only the comprehension-derived activations into a
     *       new engram trace — recall blend below does NOT feed back into
     *       what gets stored, preserving the read-only contract.
     *   (b) we use the snapshot as the RECALL cue, then blend recalled
     *       neurons (mapped 1:1 to concept_ids by lo32 hash equivalence)
     *       into the caller-visible activated_concepts at half-weight.
     * Both stages no-op when no engram_system is attached or the flag
     * is off.
     *
     * IM-3: when inflammation > _GL_IMM_ENGRAM_SKIP_THRESHOLD, skip the
     * encode pass entirely so poisoned input doesn't lay down memory
     * traces. The recall side stays intact — recall is read-only over
     * the engram store and won't propagate poison forward. */
    bool _imm_skip_engram =
        (gl->immune_enabled && gl->immune_system &&
         _imm_inflammation > _GL_IMM_ENGRAM_SKIP_THRESHOLD);
    if (gl->engram_enabled && gl->engram_system && concept_count > 0 &&
        !_imm_skip_engram) {
        /* Truncate concept_id u64 → neuron_id u32 (lower 32 bits).
         * Concept IDs are already derived from string hashes; the lo32
         * is a uniform 32-bit projection. We borrow up to 256 ids — the
         * engram API caps neuron_count at ENGRAM_MAX_NEURONS=256. */
        uint32_t cue_count = (concept_count > 256) ? 256 : concept_count;
        uint32_t cue_neurons[256];
        float    cue_activations[256];
        for (uint32_t i = 0; i < cue_count; i++) {
            cue_neurons[i] = (uint32_t)(result->activated_concepts[i] & 0xFFFFFFFFu);
            cue_activations[i] = result->activation_levels[i];
            /* engram_encode expects nonnegative activations — fold sign
             * into magnitude here so negation-marked words still encode. */
            if (cue_activations[i] < 0.0f) cue_activations[i] = -cue_activations[i];
        }

        /* EN-3 — encode the snapshot as an EPISODIC trace. Neutral
         * emotion (0,0); future task can wire emotional context if
         * available. The engram_id is intentionally discarded — for
         * read-only mode the trace lives in the engram store and is
         * looked up later via cue, not by id. */
        emotional_tag_t emo = {0};
        (void)engram_encode((engram_system_t*)gl->engram_system,
                             cue_neurons, cue_activations, cue_count,
                             MEMORY_TYPE_EPISODIC, emo);
        gl->stats.engram_encodes++;

        /* EN-4 — recall: probe with the same cue, blend recalled neurons
         * into activated_concepts at half-weight. Recall returns the
         * neuron_ids of the matched engram (same lo32 hashes) — we map
         * them back into concept activations by extending the array.
         * Threshold: ENGRAM_RECALL_THRESHOLD (0.4); we accept anything
         * the engram module deems confident enough. */
        uint32_t recalled[256];
        float    recalled_acts[256];
        float    confidence = 0.0f;
        uint64_t hit = engram_recall((engram_system_t*)gl->engram_system,
                                      cue_neurons, cue_count,
                                      recalled, recalled_acts, 256,
                                      &confidence);
        if (hit != 0 && confidence >= ENGRAM_RECALL_THRESHOLD) {
            gl->stats.engram_recalls++;
            for (uint32_t r = 0;
                 r < 256 && concept_count < GL_MAX_ACTIVE_CONCEPTS;
                 r++) {
                if (recalled_acts[r] <= 0.0f) continue;
                /* Half-weight blend, matches anaphora's pattern. The
                 * lo32 → concept_id map is lossy; we treat recalled[r]
                 * as a concept_id directly (all our concept_ids fit in
                 * 32 bits in practice for the brain's current scale). */
                uint64_t cid = (uint64_t)recalled[r];
                bool merged = false;
                for (uint32_t c = 0; c < concept_count; c++) {
                    if (result->activated_concepts[c] == cid) {
                        result->activation_levels[c] += 0.5f * recalled_acts[r];
                        merged = true;
                        break;
                    }
                }
                if (!merged) {
                    result->activated_concepts[concept_count] = cid;
                    result->activation_levels[concept_count]   = 0.5f * recalled_acts[r];
                    concept_count++;
                }
            }
        }
    }

    result->concept_count = concept_count;
    /* Subword-recognized words count as half-known: the BPE tokenizer
     * found valid subword tokens for them but no full lexicon entry. */
    float effective_known = (float)known_words + 0.5f * (float)subword_hits;
    if (effective_known > (float)word_count) effective_known = (float)word_count;
    result->comprehension_confidence = (word_count > 0) ?
        effective_known / (float)word_count : 0.0f;
    result->novelty = (word_count > 0) ?
        1.0f - effective_known / (float)word_count : 1.0f;

    /* IM-3 — apply the inflammation damp to comprehension confidence.
     * Damp factor = (1 - 0.5 * inflammation), so a fully-inflamed
     * inspection halves the base confidence. The downstream cortex /
     * SNN-bridge / network-broadcast modulators apply *after* this so
     * a damped confidence still gets the cortex/network boosts — the
     * inflammation simply lowers the starting value. */
    if (gl->immune_enabled && gl->immune_system) {
        result->comprehension_confidence *= (1.0f - 0.5f * _imm_inflammation);
        if (result->comprehension_confidence < 0.0f)
            result->comprehension_confidence = 0.0f;
    }

    /* Normalize semantic vector */
    normalize_vector(result->semantic_vector, gl->semantic_dim);

    /* Walkthrough round 3 fix: detect all-zero semantic_vector (every word
     * was OOV with no fallback signal). Without this guard, the context
     * update below blends 0.7 zeros into the running context every time,
     * decaying it toward zero and silently degrading subsequent
     * comprehensions. Skip the blend on zero-vec so the previous context
     * is preserved verbatim. */
    float vec_l2 = 0.0f;
    for (uint32_t d = 0; d < gl->semantic_dim; d++) {
        vec_l2 += result->semantic_vector[d] * result->semantic_vector[d];
    }
    bool semantic_vec_is_zero = (vec_l2 < 1e-12f);

    /* Update context — skip if vector is zero (preserve prior context).
     *
     * Tier-2 #7: auto-push this comprehension as a discourse turn.
     * push_turn rebuilds gl->context.context_vector from the buffer's
     * recency-weighted blend, which supersedes the legacy 0.7 blend
     * but produces a comparable smoothed vector (geometric decay 0.6
     * across up to capacity turns vs. the legacy 70/30 single step).
     * Rebuilding from the buffer keeps the canonical context
     * consistent with the per-turn breakdown — without this, the
     * single rolling vector and the buffer would drift apart and any
     * future coherence check would have to choose one as canonical. */
    if (!semantic_vec_is_zero) {
        grounded_language_push_turn(gl, result->semantic_vector,
                                      gl->semantic_dim, word_count, true);
    }
    gl->context.words_in_context += word_count;

    /* TB-10 — topic-shift detection in discourse. Run AFTER the push so
     * the latest turn is in the ring; compare against the prior K turns'
     * mean. When fewer than `min_turns` priors exist, gl_topic_shift_score
     * returns 1.0 and we leave the boundary flag false without bumping
     * counters. Default-OFF, opt-in per system instance via the setter. */
    if (gl->enable_topic_shift_detection && !semantic_vec_is_zero) {
        uint32_t need_priors = gl->topic_shift_min_turns;
        if (need_priors < 2u) need_priors = 2u;
        if (gl->discourse.count >= (uint8_t)(need_priors + 1)) {
            float score = gl_topic_shift_score(gl, need_priors);
            gl->last_topic_shift_score = score;
            gl->stats.topic_shifts_evaluated++;
            bool shifted = (score < gl->topic_shift_threshold);
            gl->last_was_topic_shift = shifted;
            if (shifted) {
                gl->stats.topic_shifts_detected++;
                /* Audit-2 follow-up: fire TOPIC_SHIFT event for downstream
                 * consumers (engram episodic consolidation, sleep-wake
                 * scheduler, etc.). Carries the cosine score that crossed
                 * the threshold. */
                gl_event_t ev = {0};
                ev.type             = GL_EVENT_TOPIC_SHIFT;
                ev.text             = text;
                ev.semantic_vec     = result->semantic_vector;
                ev.topic_similarity = score;
                ev.confidence       = result->comprehension_confidence;
                gl_fire_event(gl, &ev);
            }
        } else {
            /* Not enough turns yet — hold the score at 1.0 ("no shift")
             * and keep the boundary flag false. Counters do NOT bump
             * because the detector didn't actually evaluate. */
            gl->last_topic_shift_score = 1.0f;
            gl->last_was_topic_shift   = false;
        }
    }

    /* Store recent concepts */
    gl->context.last_concept_count = 0;
    for (uint32_t c = 0; c < concept_count && c < 16; c++) {
        gl->context.last_concepts[c] = result->activated_concepts[c];
        gl->context.last_concept_count++;
    }

    gl->stats.total_comprehensions++;

    /* Tier-2 #3 / #6 telemetry — bump only on actual events. */
    if (gl->enable_negation_inversion && any_negated) {
        gl->stats.negation_events++;
    }

    /* TA-5 — reconsolidation on contradiction. For every word marked
     * for negation, decay its lexicon-entry binding strengths so the
     * "it isn't a fish" path eventually erodes the fish↔aquatic
     * binding. Reads negate_word[] directly so we only fire on the
     * already-validated content tokens (function/cue words skipped by
     * the negation pass above). NULL-safe — reconsolidate_word is a
     * no-op for words not in the lexicon. */
    if (gl->enable_reconsolidation && any_negated) {
        for (uint32_t i = 0; i < word_count; i++) {
            if (!negate_word[i]) continue;
            (void)grounded_language_reconsolidate_word(
                gl, words[i], gl->reconsolidation_decay);
        }
    }

    /* NLP-2 — coref pass. Scan for "<definite> <noun>" patterns and
     * track noun heads in the coref ring. Same-surface match within
     * GL_COREF_MAX_TURNS counts as a coref resolution. Default OFF. */
    if (gl->enable_coref_resolution) {
        static const char* definite_markers[] = {
            "the", "this", "that", "these", "those", NULL
        };
        gl_tc12_lock(gl);
        for (uint32_t i = 0; i + 1 < word_count; i++) {
            bool is_def = false;
            for (int m = 0; definite_markers[m]; m++) {
                if (strcmp(words[i], definite_markers[m]) == 0) {
                    is_def = true; break;
                }
            }
            if (!is_def) continue;
            /* words[i+1] is the candidate noun head. Skip pronouns
             * (they have their own resolver) and very short tokens. */
            const char* head = words[i + 1];
            if (!head || strlen(head) < 2) continue;
            /* Use total_comprehensions as the turn idx — it advances
             * unconditionally per comprehend (incremented above at the
             * stats block), unlike discourse.head which only advances
             * when push_turn fires (gated on a non-zero semantic vec).
             * Truncate to uint32_t; overflow at 4B comprehensions is
             * an acceptable wraparound for a 6-turn lookback window. */
            int matched = gl_coref_track_locked(gl, head,
                (uint32_t)gl->stats.total_comprehensions);
            if (matched) {
                /* Fire event outside the lock so subscribers can call
                 * back into gl without deadlock — copy fields needed. */
                gl_event_t ev = {0};
                ev.type             = GL_EVENT_COREF_RESOLVED;
                ev.word             = head;
                ev.text             = text;
                ev.topic_similarity = 1.0f;  /* same-surface == 1.0 */
                ev.confidence       = result->comprehension_confidence;
                gl_tc12_unlock(gl);
                gl_fire_event(gl, &ev);
                gl_tc12_lock(gl);
            }
        }
        gl_tc12_unlock(gl);
    }
    if (sense_resolved_this_pass) {
        gl->stats.sense_resolutions++;
    }

    /* Dual-path: also feed through SNN bridge for spike-level comprehension */
    if (gl->snn_bridge) {
        float snn_concepts[512];
        uint32_t snn_activated = 0;
        float snn_conf = 0.0f;
        snn_language_bridge_comprehend(gl->snn_bridge, text,
                                       snn_concepts, 512, &snn_activated, &snn_conf);
        /* Blend spike-level confidence into result */
        float blend = snn_language_bridge_get_blend(gl->snn_bridge);
        result->comprehension_confidence =
            result->comprehension_confidence * (1.0f - blend) + snn_conf * blend;
    }

    /* Cortex modulation: scale comprehension confidence when sensory
     * cortexes are currently active. Caps at 1.0. No-op when no cortex
     * is connected (all scalars stay 0). */
    extern int grounded_language_get_cortex_modulation(grounded_language_t*,
                                                        gl_cortex_modulation_t*);
    gl_cortex_modulation_t mod = {0};
    if (grounded_language_get_cortex_modulation(gl, &mod) == 0) {
        float peak = mod.visual_activity;
        if (mod.audio_salience > peak)    peak = mod.audio_salience;
        if (mod.speech_confidence > peak) peak = mod.speech_confidence;
        if (peak < 0.0f) peak = 0.0f;
        if (peak > 1.0f) peak = 1.0f;
        result->comprehension_confidence *= (1.0f + 0.2f * peak);
        if (result->comprehension_confidence > 1.0f)
            result->comprehension_confidence = 1.0f;
    }

    /* Per-network broadcast: feed the final semantic_vector to LNN /
     * CNN / FNO / ANN if attached, then apply their averaged response
     * magnitude as a confidence boost up to +15%. No-op (factor=1.0)
     * when no networks are wired. */
    extern int grounded_language_broadcast_to_networks(grounded_language_t*,
                                                         const float*, uint32_t);
    extern float gl_network_modulation_factor(grounded_language_t*, float);
    grounded_language_broadcast_to_networks(gl, result->semantic_vector,
                                              gl->semantic_dim);
    result->comprehension_confidence *= gl_network_modulation_factor(gl, 0.15f);
    if (result->comprehension_confidence > 1.0f)
        result->comprehension_confidence = 1.0f;

    /* Fire COMPREHENDED event on the cognitive bus. */
    gl_event_t bus_ev = {0};
    bus_ev.type         = GL_EVENT_COMPREHENDED;
    bus_ev.text         = text;
    bus_ev.semantic_vec = result->semantic_vector;
    bus_ev.confidence   = result->comprehension_confidence;
    gl_fire_event(gl, &bus_ev);

    /* #10 Active-learning curriculum signal. If overall comprehension
     * confidence is low or the weakest word's strength is below the
     * threshold, fire NEEDS_GROUNDING with the targeted word so
     * curriculum modules can prioritize it for the next exposure. */
    if (needy_word &&
        (result->comprehension_confidence < GL_LOW_CONFIDENCE_THRESHOLD ||
         needy_word_conf < GL_LOW_CONFIDENCE_THRESHOLD)) {
        gl_event_t need_ev = {0};
        need_ev.type        = GL_EVENT_NEEDS_GROUNDING;
        need_ev.word        = needy_word;
        need_ev.text        = text;
        need_ev.confidence  = needy_word_conf;
        gl_fire_event(gl, &need_ev);
        gl->stats.total_needs_grounding++;
    }

    nimcp_free(buf);
    return 0;
}

void gl_comprehension_result_cleanup(gl_comprehension_result_t* result) {
    if (!result) return;
    nimcp_free(result->activated_concepts);
    nimcp_free(result->activation_levels);
    nimcp_free(result->semantic_vector);
    memset(result, 0, sizeof(*result));
}

/*=============================================================================
 * Tier-2 #3 — Negation polarity helpers
 *
 * Implementation lives next to comprehend so the lookahead window
 * constants (GL_NEGATION_WINDOW), the function-word filter, and the
 * cue table all stay in one place. Cues are matched on the lower-cased
 * token; the contraction list catches both the standalone form
 * ("don't") and the n't-suffix-on-prior-token pattern handled by
 * is_negation_cue() returning true for either.
 *===========================================================================*/

static bool is_function_word(const char* w) {
    /* Cheap linear scan — FUNCTION_WORDS is ~70 entries and only walked
     * during negation lookahead (not the binding loop). */
    for (int i = 0; FUNCTION_WORDS[i]; i++) {
        if (strcmp(w, FUNCTION_WORDS[i]) == 0) return true;
    }
    return false;
}

static bool is_negation_cue(const char* w) {
    if (!w || !*w) return false;
    /* Bare cues. */
    static const char* CUES[] = {
        "not", "no", "never",
        "don't", "doesn't", "didn't",
        "won't", "can't", "cannot",
        "isn't", "wasn't", "aren't", "weren't",
        "haven't", "hasn't", "hadn't",
        "shouldn't", "wouldn't", "couldn't",
        NULL
    };
    for (int i = 0; CUES[i]; i++) {
        if (strcmp(w, CUES[i]) == 0) return true;
    }
    /* Suffix "n't" that survived the tokenizer (e.g. "don" / "t" got
     * glued back via the apostrophe). The tokenizer keeps apostrophes
     * inside words so most contractions arrive whole, but a defensive
     * suffix check costs almost nothing and catches "shan't" / dialect
     * forms that aren't in the table above. */
    size_t n = strlen(w);
    if (n >= 3 && strcmp(w + n - 3, "n't") == 0) return true;
    return false;
}

/*=============================================================================
 * TB-9 — speech-act intent classification helpers.
 *
 * All four cue tables are lower-case (matched against tokenized words
 * which the comprehend path already lower-cased). The classifier sees
 * the raw text only to peek at the trailing punctuation.
 *===========================================================================*/

static bool is_wh_word(const char* w) {
    if (!w || !*w) return false;
    static const char* WH[] = {
        "what", "who", "where", "when", "why",
        "how", "which", "whose", "whom",
        NULL
    };
    for (int i = 0; WH[i]; i++) {
        if (strcmp(w, WH[i]) == 0) return true;
    }
    return false;
}

static bool is_auxiliary(const char* w) {
    if (!w || !*w) return false;
    static const char* AUX[] = {
        "is", "are", "was", "were",
        "do", "does", "did",
        "can", "could", "will", "would", "should",
        "may", "might",
        "has", "have", "had",
        NULL
    };
    for (int i = 0; AUX[i]; i++) {
        if (strcmp(w, AUX[i]) == 0) return true;
    }
    return false;
}

static bool is_imperative_cue(const char* w) {
    if (!w || !*w) return false;
    static const char* CUES[] = {
        "go", "stop", "come", "look", "listen", "wait",
        "give", "take", "tell", "show", "find", "bring",
        "put", "please", "let",
        NULL
    };
    for (int i = 0; CUES[i]; i++) {
        if (strcmp(w, CUES[i]) == 0) return true;
    }
    return false;
}

static bool is_greeting_cue(const char* w) {
    if (!w || !*w) return false;
    static const char* CUES[] = {
        "hello", "hi", "hey", "greetings",
        "goodbye", "bye",
        "morning", "afternoon", "evening",
        NULL
    };
    for (int i = 0; CUES[i]; i++) {
        if (strcmp(w, CUES[i]) == 0) return true;
    }
    return false;
}

static bool is_first_person_pronoun(const char* w) {
    if (!w || !*w) return false;
    static const char* PRONS[] = {
        "i", "me", "we", "my", "our", "mine", "ours", "us",
        NULL
    };
    for (int i = 0; PRONS[i]; i++) {
        if (strcmp(w, PRONS[i]) == 0) return true;
    }
    return false;
}

static gl_speech_act_t classify_speech_act(char* const* words,
                                            uint32_t word_count,
                                            const char* raw_text) {
    /* Empty / whitespace-only / punctuation-only input has no intent to
     * classify. Returning UNKNOWN keeps the per-class counters honest —
     * pre-fix, an empty utterance fell through to the ASSERTION default
     * and bumped speech_act_assertions. Callers that get UNKNOWN can
     * still see "we tried but the input had no content". */
    if (!words || word_count == 0 || !raw_text || !*raw_text) {
        return GL_SPEECH_ACT_UNKNOWN;
    }

    /* Walk to the last non-whitespace char of raw_text. */
    size_t n = strlen(raw_text);
    char trailing = '\0';
    while (n > 0) {
        char c = raw_text[n - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            n--;
            continue;
        }
        trailing = c;
        break;
    }

    /* (1) Greeting check first — beats "?" and "!". */
    uint32_t scan = (word_count < 2) ? word_count : 2;
    for (uint32_t i = 0; i < scan; i++) {
        if (is_greeting_cue(words[i])) return GL_SPEECH_ACT_GREETING;
    }

    /* (2) Question — trailing '?' or wh-word/auxiliary as first token. */
    if (trailing == '?') return GL_SPEECH_ACT_QUESTION;
    if (is_wh_word(words[0]))     return GL_SPEECH_ACT_QUESTION;
    if (is_auxiliary(words[0]))   return GL_SPEECH_ACT_QUESTION;

    /* (3) Imperative — bare-verb cue at front. */
    if (is_imperative_cue(words[0])) return GL_SPEECH_ACT_IMPERATIVE;

    /* (4) Trailing '!' — IMPERATIVE if no first-person pronoun in the
     * first 3 tokens AND > 1 word; otherwise EXCLAMATION. Single-token
     * "amazing!" lands as EXCLAMATION (cannot be a command without an
     * object); "go fetch the ball!" would already have hit IMPERATIVE
     * via (3); "i won the game!" with first-person → EXCLAMATION. */
    if (trailing == '!') {
        uint32_t look = (word_count < 3) ? word_count : 3;
        bool has_fp = false;
        for (uint32_t i = 0; i < look; i++) {
            if (is_first_person_pronoun(words[i])) { has_fp = true; break; }
        }
        if (!has_fp && word_count >= 2) {
            return GL_SPEECH_ACT_IMPERATIVE;
        }
        return GL_SPEECH_ACT_EXCLAMATION;
    }

    /* (5) Default fallback. */
    return GL_SPEECH_ACT_ASSERTION;
}

/*=============================================================================
 * TB-6 — sentence-boundary segmentation helpers.
 *===========================================================================*/

/* Count terminator-bounded sentences in `text`. */
static uint32_t gl_count_sentences(const char* text) {
    if (!text) return 0;
    uint32_t count = 0;
    bool any_word_in_current = false;
    for (const char* p = text; *p; p++) {
        if (*p == '.' || *p == '!' || *p == '?') {
            if (any_word_in_current) {
                count++;
                any_word_in_current = false;
            }
        } else if (!isspace((unsigned char)*p)) {
            any_word_in_current = true;
        }
    }
    if (any_word_in_current) count++;
    return count;
}

/* Find the first sentence-terminator (`.`, `!`, `?`) at or after `p`. */
static const char* gl_find_sentence_terminator(const char* p) {
    if (!p) return NULL;
    for (; *p; p++) {
        if (*p == '.' || *p == '!' || *p == '?') return p;
    }
    return NULL;
}

/*=============================================================================
 * Tier-2 #6 — Sense-disambiguation helper
 *
 * Picks the binding whose concept-features cosine-best aligns with the
 * supplied intent vector. Falls back to index 0 on missing features /
 * empty intents — deterministic so test assertions stay tight.
 *===========================================================================*/

uint32_t grounded_language_disambiguate_sense(
    const grounded_language_t* gl,
    const gl_lexicon_entry_t* entry,
    const float* intent_vec)
{
    if (!gl || !entry || !intent_vec) return 0;
    if (entry->binding_count == 0) return 0;
    if (entry->binding_count == 1) return 0;

    uint32_t best = 0;
    float    best_score = -2.0f; /* cosine ∈ [-1, 1] — sentinel below the floor */
    bool     any_scored = false;

    for (uint32_t b = 0; b < entry->binding_count; b++) {
        const float* cf = get_concept_features(gl, entry->bindings[b].concept_id);
        if (!cf) continue;
        float score = cosine_similarity(intent_vec, cf, gl->semantic_dim);
        any_scored = true;
        if (score > best_score) {
            best_score = score;
            best = b;
        }
    }

    /* No binding had concept features → fall back to first binding. */
    return any_scored ? best : 0;
}

/*=============================================================================
 * Tier-2 #3 / #6 — config setters/getters
 *===========================================================================*/

void grounded_language_set_negation_enabled(grounded_language_t* gl, bool enabled) {
    if (!gl) return;
    gl->enable_negation_inversion = enabled;
}

bool grounded_language_get_negation_enabled(const grounded_language_t* gl) {
    if (!gl) return false;
    return gl->enable_negation_inversion;
}

void grounded_language_set_sense_disambiguation_enabled(grounded_language_t* gl,
                                                         bool enabled) {
    if (!gl) return;
    gl->enable_sense_disambiguation = enabled;
}

/*-----------------------------------------------------------------------------
 * TA-5 — reconsolidation on contradiction.
 *---------------------------------------------------------------------------*/

void grounded_language_set_reconsolidation_enabled(grounded_language_t* gl,
                                                    bool enabled) {
    if (!gl) return;
    gl->enable_reconsolidation = enabled;
}

bool grounded_language_get_reconsolidation_enabled(const grounded_language_t* gl) {
    if (!gl) return false;
    return gl->enable_reconsolidation;
}

void grounded_language_set_reconsolidation_decay(grounded_language_t* gl,
                                                  float decay) {
    if (!gl) return;
    if (!isfinite(decay) || decay < 0.0f) decay = 0.0f;
    if (decay > 0.5f) decay = 0.5f;
    gl->reconsolidation_decay = decay;
}

float grounded_language_get_reconsolidation_decay(const grounded_language_t* gl) {
    if (!gl) return 0.0f;
    return gl->reconsolidation_decay;
}

void grounded_language_set_produce_distributional_weight(grounded_language_t* gl,
                                                         float weight) {
    if (!gl) return;
    if (!isfinite(weight) || weight < 0.0f) weight = 0.0f;
    if (weight > 1.0f) weight = 1.0f;
    gl->produce_distributional_weight = weight;
}

float grounded_language_get_produce_distributional_weight(const grounded_language_t* gl) {
    if (!gl) return GL_PRODUCE_DISTRIBUTIONAL_DEFAULT_WEIGHT;
    return gl->produce_distributional_weight;
}

void grounded_language_set_produce_frequency_penalty(grounded_language_t* gl,
                                                     float penalty) {
    if (!gl) return;
    if (!isfinite(penalty) || penalty < 0.0f) penalty = 0.0f;
    gl->produce_frequency_penalty = penalty;
}

float grounded_language_get_produce_frequency_penalty(const grounded_language_t* gl) {
    if (!gl) return GL_PRODUCE_FREQUENCY_PENALTY_DEFAULT;
    return gl->produce_frequency_penalty;
}

/*-----------------------------------------------------------------------------
 * TB-9 — speech-act intent classification toggle.
 *---------------------------------------------------------------------------*/

void grounded_language_set_speech_act_classification_enabled(
    grounded_language_t* gl, bool enabled)
{
    if (!gl) return;
    gl->enable_speech_act_classification = enabled;
}

bool grounded_language_get_speech_act_classification_enabled(
    const grounded_language_t* gl)
{
    if (!gl) return false;
    return gl->enable_speech_act_classification;
}

uint32_t grounded_language_reconsolidate_word(grounded_language_t* gl,
                                                const char* word,
                                                float decay) {
    if (!gl || !word) return 0;
    /* Clamp same as the setter so direct API callers can't poison
     * binding strengths with an out-of-range factor. */
    if (!isfinite(decay) || decay < 0.0f) decay = 0.0f;
    if (decay > 0.5f) decay = 0.5f;

    /* lexicon_find returns const; the underlying hash table data is
     * mutable — cast is safe here, mirrors how other writers (e.g.
     * the ground/learn paths) reach binding strengths. */
    gl_lexicon_entry_t* entry = (gl_lexicon_entry_t*)lexicon_find(gl, word);
    /* Bump the path-taken counter even when the word isn't in the
     * lexicon — the contradiction signal itself is curriculum-meaningful
     * (mirrors the negative-grounding telemetry pattern). */
    gl->stats.reconsolidation_events++;

    if (!entry || entry->binding_count == 0) return 0;

    float keep = 1.0f - decay;
    uint32_t decayed = 0;
    for (uint32_t b = 0; b < entry->binding_count; b++) {
        gl_word_binding_t* bnd = &entry->bindings[b];
        bnd->strength *= keep;
        /* Confidence floats with strength so a contradicted binding is
         * also flagged as less reliable — downstream sense-disambiguation
         * already weights by strength, so confidence stays informational. */
        bnd->confidence *= keep;
        for (uint32_t m = 0; m < GL_MODALITY_COUNT; m++) {
            bnd->modality_strength[m] *= keep;
        }
        decayed++;
    }
    gl->stats.reconsolidation_bindings_decayed += decayed;
    return decayed;
}

bool grounded_language_get_sense_disambiguation_enabled(const grounded_language_t* gl) {
    if (!gl) return false;
    return gl->enable_sense_disambiguation;
}

/*-----------------------------------------------------------------------------
 * TB-6 — sentence-boundary segmentation toggle.
 *
 * Per-instance runtime knob; not persisted across save/load. Default
 * OFF (set in grounded_language_create) so legacy "one comprehend call
 * → one push_turn" behaviour stays bit-for-bit identical until the
 * trainer opts in. Setter is NULL-safe.
 *---------------------------------------------------------------------------*/

void grounded_language_set_sentence_segmentation_enabled(grounded_language_t* gl,
                                                          bool enabled) {
    if (!gl) return;
    gl->enable_sentence_segmentation = enabled;
}

bool grounded_language_get_sentence_segmentation_enabled(
    const grounded_language_t* gl) {
    if (!gl) return false;
    return gl->enable_sentence_segmentation;
}

/*=============================================================================
 * Tier-2 #7 — Multi-turn discourse buffer
 *
 * Implementation: small ring buffer of length capacity ≤ MAX_TURNS.
 * push_turn copies the supplied vector (or stores NULL for empty
 * placeholders); when the buffer is full the oldest slot is reclaimed.
 * The legacy gl->context.context_vector is recomputed every push as a
 * recency-weighted blend across the live turns.
 *===========================================================================*/

/* Map a logical position [0, count) where 0 = oldest, count-1 = newest
 * into the ring index. When count < capacity the buffer hasn't wrapped
 * and turns sit in [0, count) with head == 0; otherwise the oldest is
 * at `head`. */
static inline uint8_t discourse_idx(const gl_discourse_state_t* d, uint8_t pos) {
    return (uint8_t)((d->head + pos) % (d->capacity ? d->capacity : 1));
}

/* Recompute gl->context.context_vector as a recency-weighted blend of
 * the live discourse turns. Newest turn gets the largest weight; older
 * turns decay geometrically by 0.6 per step. Skips empty placeholder
 * turns (NULL semantic_vector). After the blend the vector is
 * normalized (unit L2) so downstream cosine consumers see a stable
 * scale. Called by push_turn; safe to call when count == 0 (no-op,
 * leaves the prior context untouched). */
static void discourse_rebuild_context_blend(grounded_language_t* gl) {
    if (!gl || gl->discourse.count == 0) return;
    if (!gl->context.context_vector) return;

    float* out = gl->context.context_vector;
    memset(out, 0, gl->semantic_dim * sizeof(float));

    /* Weights from oldest → newest: 0.6^(count-1-i). */
    float total_weight = 0.0f;
    for (uint8_t i = 0; i < gl->discourse.count; i++) {
        const gl_discourse_turn_t* t =
            &gl->discourse.turns[discourse_idx(&gl->discourse, i)];
        if (!t->semantic_vector) continue;
        float w = 1.0f;
        uint8_t depth = (uint8_t)(gl->discourse.count - 1 - i);
        for (uint8_t k = 0; k < depth; k++) w *= 0.6f;
        for (uint32_t d = 0; d < gl->semantic_dim; d++) {
            out[d] += t->semantic_vector[d] * w;
        }
        total_weight += w;
    }
    if (total_weight > 1e-8f) {
        for (uint32_t d = 0; d < gl->semantic_dim; d++) out[d] /= total_weight;
    }
    normalize_vector(out, gl->semantic_dim);
}

int grounded_language_push_turn(grounded_language_t* gl,
                                 const float* semantic_vec,
                                 uint32_t vec_dim,
                                 uint32_t n_words,
                                 bool is_user) {
    if (!gl) return -1;

    /* Audit walkthrough-2 fix — discourse ring writes + rebuild are
     * protected by mutate_lock. Concurrent comprehend calls used to
     * race on head/count + the turns array. */
    gl_mutate_lock(gl);

    if (gl->discourse.capacity == 0) gl->discourse.capacity = GL_DISCOURSE_MAX_TURNS;

    /* Pick the destination slot — append while not full, otherwise
     * overwrite the oldest (at head) and advance head one step. */
    uint8_t slot;
    if (gl->discourse.count < gl->discourse.capacity) {
        slot = (uint8_t)((gl->discourse.head + gl->discourse.count)
                         % gl->discourse.capacity);
        gl->discourse.count++;
    } else {
        slot = gl->discourse.head;
        gl->discourse.head = (uint8_t)((gl->discourse.head + 1)
                                        % gl->discourse.capacity);
        /* Free the prior occupant's vector before overwriting. */
        nimcp_free(gl->discourse.turns[slot].semantic_vector);
        gl->discourse.turns[slot].semantic_vector = NULL;
    }

    gl_discourse_turn_t* t = &gl->discourse.turns[slot];
    /* If the slot still has a stale vector from an out-of-order eviction
     * (defensive — paths above cleared it) free it first. */
    if (t->semantic_vector) {
        nimcp_free(t->semantic_vector);
        t->semantic_vector = NULL;
    }
    t->word_count   = n_words;
    t->timestamp_ms = (uint64_t)time(NULL) * 1000;
    t->is_user      = is_user;

    if (semantic_vec) {
        t->semantic_vector = (float*)nimcp_calloc(gl->semantic_dim, sizeof(float));
        if (!t->semantic_vector) {
            /* Roll back the slot reservation on alloc failure. */
            if (gl->discourse.count > 0) gl->discourse.count--;
            gl_mutate_unlock(gl);
            return -1;
        }
        uint32_t cmp = vec_dim < gl->semantic_dim ? vec_dim : gl->semantic_dim;
        memcpy(t->semantic_vector, semantic_vec, cmp * sizeof(float));
    }

    gl_mutate_unlock(gl);

    /* Walkthrough-3 fix — rebuild context blend OUTSIDE the lock.
     * The function reads discourse turns (which we just locked-write
     * above) and computes a weighted blend; once we've released the
     * lock, any concurrent push_turn would just re-trigger another
     * rebuild — no correctness issue since the rebuild is idempotent
     * over the read snapshot. Keeping the critical section tight
     * matters here: rebuild does O(dim * turns) float math that
     * shouldn't block other comprehend calls. */
    discourse_rebuild_context_blend(gl);
    return 0;
}

uint8_t grounded_language_get_discourse_turn_count(const grounded_language_t* gl) {
    if (!gl) return 0;
    return gl->discourse.count;
}

/* Tier 1 Step E (2026-05-24): read a recent discourse turn's semantic
 * vector for multi-turn continuity. `back` counts from the newest turn:
 * back=1 → newest, back=2 → the one before it, etc. Returns false (and
 * leaves *out_vec / *out_dim untouched) when fewer than `back` turns
 * exist, the requested slot is empty, or any arg is NULL. The vector is
 * GL-owned (allocated at gl->semantic_dim) and valid until the next
 * discourse mutation — READ-ONLY, do not free or retain across turns. */
bool grounded_language_get_recent_turn_vector(const grounded_language_t* gl,
                                              uint8_t back,
                                              const float** out_vec,
                                              uint32_t* out_dim) {
    if (!gl || !out_vec || !out_dim || back == 0) return false;
    if (gl->discourse.count < back) return false;
    /* Logical position in oldest→newest order. */
    uint8_t pos = (uint8_t)(gl->discourse.count - back);
    const gl_discourse_turn_t* t =
        &gl->discourse.turns[discourse_idx(&gl->discourse, pos)];
    if (!t->semantic_vector) return false;
    *out_vec = t->semantic_vector;
    *out_dim = gl->semantic_dim;
    return true;
}

uint32_t grounded_language_copy_recent_turn_vector(grounded_language_t* gl,
                                                   uint8_t back,
                                                   float* out,
                                                   uint32_t max_dim) {
    if (!gl || !out || max_dim == 0 || back == 0) return 0;
    /* X2 (walkthrough fix, 2026-05-27): lock-guarded COPY variant of
     * grounded_language_get_recent_turn_vector. The pointer-returning accessor
     * hands back a raw interior pointer into the discourse ring that push_turn
     * can nimcp_free() under mutate_lock — a torn-read / use-after-free window
     * for any caller that doesn't copy it out instantly. This variant copies
     * under the same mutate_lock so callers (produce discourse-seed, cascade
     * Step-E content blend) never hold a ring pointer across a possible
     * concurrent push_turn. Returns floats written (0 = no such turn). */
    gl_mutate_lock(gl);
    uint32_t copied = 0;
    if (gl->discourse.count >= back) {
        uint8_t pos = (uint8_t)(gl->discourse.count - back);
        const gl_discourse_turn_t* t =
            &gl->discourse.turns[discourse_idx(&gl->discourse, pos)];
        if (t->semantic_vector) {
            uint32_t d = (gl->semantic_dim < max_dim) ? gl->semantic_dim : max_dim;
            memcpy(out, t->semantic_vector, (size_t)d * sizeof(float));
            copied = d;
        }
    }
    gl_mutate_unlock(gl);
    return copied;
}

/*============================================================================
 * Tier 1 Step F3 — subject-verb / quantifier-noun agreement correction
 *==========================================================================*/

/* Lowercase a token into a fixed buffer. */
static void gl_agr_lower(const char* in, char* out, size_t cap) {
    size_t i = 0;
    for (; in[i] && i + 1 < cap; i++) {
        char c = in[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[i] = '\0';
}

static bool gl_agr_in_set(const char* w, const char* const* set) {
    for (int i = 0; set[i]; i++) if (strcmp(w, set[i]) == 0) return true;
    return false;
}

/* Already inflected (don't double-apply)? Trailing -s / -ed / -ing.
 * The -s test excludes the singular -ss/-us/-is endings (glass, bus, analysis,
 * process, virus) that are NOT plurals/3sg, so F3 can still inflect them
 * ("the cat process" -> "processes") and pronominalization doesn't misread a
 * singular -s noun as plural ("them" instead of "it"). Genuine plurals/3sg of
 * those stems end in -es ("glasses","buses") and still test true via -ed?no,
 * via the trailing 's' with preceding 'e'. */
static bool gl_agr_already_inflected(const char* w) {
    size_t n = strlen(w);
    if (n >= 1 && w[n-1] == 's') {
        if (n >= 2) {
            char p = w[n-2];
            /* -ss / -us / -is are typically singular (glass, bus, basis). */
            if (p != 's' && p != 'u' && p != 'i') return true;
        } else {
            return true;
        }
    }
    if (n >= 2 && w[n-2]=='e' && w[n-1]=='d') return true;
    if (n >= 3 && w[n-3]=='i' && w[n-2]=='n' && w[n-1]=='g') return true;
    return false;
}

int gl_apply_svo_agreement(grounded_language_t* gl, const char* in,
                           char* out, size_t out_sz) {
    if (!in || !out || out_sz == 0) { if (out && out_sz) out[0]='\0'; return -1; }

    /* Closed-class signal tables (the reliable hooks — see header). */
    static const char* SUBJ_3SG[]  = {"he","she","it",NULL};
    static const char* SUBJ_NON3SG[]= {"i","you","we","they","these","those",NULL};
    static const char* PLURAL_QUANT[]={"two","three","four","five","six","seven",
        "eight","nine","ten","many","several","few","some","both","various",
        "multiple","most","these","those",NULL};
    /* Function-ish words skipped when scanning for the subject/verb slot. */
    static const char* SKIP_SLOT[] = {"the","a","an","this","that","my","your",
        "his","her","its","our","their","of","to","in","on","at","with","for",
        "and","or","but","very","really","quite",NULL};

    /* MAXTOK bounds the correctable surface length. These correctors run only
     * on cascade produce output (never arbitrary user text); produce caps
     * content at GL_PRODUCE_TOPK (32) + ~1 scaffolding word each (~64 surface
     * tokens), so 128 gives ~2x headroom and the corrector does not silently
     * drop produced tokens past the old 48 cap. */
    enum { MAXTOK = 128, TOKLEN = 64 };
    char tok[MAXTOK][TOKLEN];
    char low[MAXTOK][TOKLEN];
    uint32_t nt = 0;

    /* Tokenize on whitespace. */
    {
        size_t i = 0, L = strlen(in);
        while (i < L && nt < MAXTOK) {
            while (i < L && (in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r')) i++;
            if (i >= L) break;
            uint32_t k = 0;
            while (i < L && !(in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r')
                   && k < TOKLEN-1) {
                tok[nt][k++] = in[i++];
            }
            tok[nt][k] = '\0';
            gl_agr_lower(tok[nt], low[nt], TOKLEN);
            nt++;
            while (i < L && !(in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r')) i++;
        }
    }

    int fixes = 0;
    bool noun_is_plural[MAXTOK];
    memset(noun_is_plural, 0, sizeof(noun_is_plural));

    /* (1) Quantifier → pluralize the governed noun. The noun is the next
     * non-skip token (skip determiners/adjectives). */
    for (uint32_t i = 0; i < nt; i++) {
        if (!gl_agr_in_set(low[i], PLURAL_QUANT)) continue;
        for (uint32_t j = i+1; j < nt; j++) {
            if (gl_agr_in_set(low[j], SKIP_SLOT)) continue;
            if (!gl_agr_already_inflected(low[j])) {
                char pl[TOKLEN];
                if (gl_morph_pluralize(low[j], pl, sizeof(pl)) > 0) {
                    memcpy(tok[j], pl, strlen(pl)+1);
                    gl_agr_lower(tok[j], low[j], TOKLEN);
                    fixes++;
                }
            }
            noun_is_plural[j] = true;  /* governed noun is now plural */
            break;
        }
    }

    /* (2) Subject-verb 3sg agreement. Subject = first content token (skip
     * determiners). Verb = next content token after it. */
    uint32_t si = 0;
    while (si < nt && gl_agr_in_set(low[si], SKIP_SLOT)) si++;
    if (si < nt) {
        bool subj_3sg;
        if (gl_agr_in_set(low[si], SUBJ_NON3SG)) {
            subj_3sg = false;
        } else if (gl_agr_in_set(low[si], SUBJ_3SG)) {
            subj_3sg = true;
        } else {
            /* A noun subject: 3sg unless it was pluralized / looks plural. */
            subj_3sg = !(noun_is_plural[si] || gl_agr_already_inflected(low[si]));
        }

        if (subj_3sg) {
            /* Find the verb slot: next content token after the subject. */
            for (uint32_t j = si+1; j < nt; j++) {
                if (gl_agr_in_set(low[j], SKIP_SLOT)) continue;
                if (gl_agr_in_set(low[j], SUBJ_3SG) ||
                    gl_agr_in_set(low[j], SUBJ_NON3SG)) break;  /* another pronoun, not a verb */
                if (gl_agr_already_inflected(low[j])) break;    /* already inflected */
                /* POS guard: skip if confidently a noun (object, not verb). */
                bool confident_noun = false;
                if (gl) {
                    const gl_lexicon_entry_t* e = grounded_language_lookup(gl, low[j]);
                    /* 0.4 matches the morphology classifier's confidence, so
                     * suffix-tagged nouns (-tion/-ness/…) are protected too. */
                    if (e && e->learned_class == GL_CLASS_NOUN &&
                        e->class_confidence >= 0.4f) confident_noun = true;
                }
                if (!confident_noun) {
                    char v3[TOKLEN];
                    if (gl_morph_inflect_3sg(low[j], v3, sizeof(v3)) > 0 &&
                        strcmp(v3, low[j]) != 0) {
                        memcpy(tok[j], v3, strlen(v3)+1);
                        gl_agr_lower(tok[j], low[j], TOKLEN);
                        fixes++;
                    }
                }
                break;  /* only the first verb slot */
            }
        }
    }

    /* Rebuild. */
    size_t pos = 0;
    for (uint32_t i = 0; i < nt; i++) {
        size_t wl = strlen(tok[i]);
        if (pos + (i?1:0) + wl + 1 > out_sz) break;
        if (i) out[pos++] = ' ';
        memcpy(out+pos, tok[i], wl);
        pos += wl;
    }
    out[pos] = '\0';
    return fixes;
}

/*============================================================================
 * Tier 1 Step F4 — production-fluency surface polish
 *   F4a articles · F4b tense consistency · F4c pronoun case + possessives
 *==========================================================================*/

/* Resolve a token's word class: trust a confidently-tagged lexicon entry,
 * else fall back to the morphology hint (so the corrector still works on a
 * sparsely-grounded lexicon / NULL gl). */
static gl_word_class_t gl_f4_class(grounded_language_t* gl, const char* low) {
    if (gl) {
        const gl_lexicon_entry_t* e = grounded_language_lookup(gl, low);
        if (e && e->learned_class != GL_CLASS_UNKNOWN &&
            e->class_confidence >= 0.4f)
            return e->learned_class;
    }
    return gl_morph_pos_hint(low);
}

/* Surface heuristic: is this token already a past-tense form? (-ed suffix or
 * a common irregular past). Mirrors gl_agr_already_inflected's role for F3. */
static bool gl_f4_is_past(const char* low) {
    size_t n = strlen(low);
    if (n >= 2 && low[n-2]=='e' && low[n-1]=='d') return true;
    static const char* P[] = {"was","were","had","did","went","ran","ate","saw",
        "came","took","made","gave","got","knew","thought","said","told","found",
        "felt","became","left","brought","bought","caught","taught","stood",
        "spoke","wrote","held","kept","slept","met","paid","sold","sent","built",
        "won","lost","fell","fed","heard","sat","sang","swam","drank","began",
        "understood",NULL};
    for (int i = 0; P[i]; i++) if (strcmp(low, P[i]) == 0) return true;
    return false;
}

int gl_apply_f4_fluency(grounded_language_t* gl, const char* in,
                        char* out, size_t out_sz) {
    if (!in || !out || out_sz == 0) { if (out && out_sz) out[0]='\0'; return -1; }

    /* Developmental gate: fluency polish activates at stage >= 2, matching
     * Step C / F2 — earlier stages stay telegraphic. (The passes also no-op
     * naturally on 1-2 word output, but the gate makes intent explicit and
     * keeps stage 0-1 byte-identical.) NULL gl → ungated (unit tests). */
    if (gl && gl->current_stage < 2) {
        size_t L = strlen(in);
        if (L + 1 > out_sz) L = out_sz - 1;
        memcpy(out, in, L); out[L] = '\0';
        return 0;
    }

    static const char* SKIP_SLOT[] = {"the","a","an","this","that","my","your",
        "his","her","its","our","their","of","to","in","on","at","with","for",
        "and","or","but","very","really","quite",NULL};
    static const char* DET[]      = {"the","a","an",NULL};
    static const char* MODALS[]   = {"can","could","will","would","shall",
        "should","may","might","must",NULL};
    /* Pronoun case maps. */
    static const struct { const char* from; const char* to; } OBJ2SUBJ[] = {
        {"me","I"},{"him","he"},{"us","we"},{"them","they"},{NULL,NULL} };
    static const struct { const char* from; const char* to; } SUBJ2OBJ[] = {
        {"i","me"},{"he","him"},{"we","us"},{"they","them"},{"she","her"},
        {NULL,NULL} };
    static const struct { const char* from; const char* to; } POSSDET[] = {
        {"i","my"},{"he","his"},{"she","her"},{"we","our"},{"they","their"},
        {"it","its"},{"you","your"},{NULL,NULL} };

    /* MAXTOK bounds the correctable surface length. These correctors run only
     * on cascade produce output (never arbitrary user text); produce caps
     * content at GL_PRODUCE_TOPK (32) + ~1 scaffolding word each (~64 surface
     * tokens), so 128 gives ~2x headroom and the corrector does not silently
     * drop produced tokens past the old 48 cap. */
    enum { MAXTOK = 128, TOKLEN = 64 };
    char tok[MAXTOK][TOKLEN];
    char low[MAXTOK][TOKLEN];
    uint32_t nt = 0;

    /* Tokenize on whitespace (same scheme as F3). */
    {
        size_t i = 0, L = strlen(in);
        while (i < L && nt < MAXTOK) {
            while (i < L && (in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r')) i++;
            if (i >= L) break;
            uint32_t k = 0;
            while (i < L && !(in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r')
                   && k < TOKLEN-1) tok[nt][k++] = in[i++];
            tok[nt][k] = '\0';
            gl_agr_lower(tok[nt], low[nt], TOKLEN);
            nt++;
            while (i < L && !(in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r')) i++;
        }
    }

    int fixes = 0;

    /* Helper: replace token i with a new surface form + refresh low[i]. */
    #define GL_F4_SET(i, str) do {                                  \
        size_t _l = strlen(str);                                    \
        if (_l < TOKLEN) {                                          \
            if (strcmp(tok[i], (str)) != 0) fixes++;                \
            memcpy(tok[i], (str), _l+1);                            \
            gl_agr_lower(tok[i], low[i], TOKLEN);                   \
        }                                                           \
    } while (0)

    /* ---- F4a: article selection -------------------------------------- */
    /* Track noun lemmas already seen so a re-mention takes "the". */
    char seen[MAXTOK][TOKLEN]; uint32_t n_seen = 0;
    for (uint32_t i = 0; i < nt; i++) {
        if (!gl_agr_in_set(low[i], DET)) continue;
        if (i + 1 >= nt) continue;                 /* dangling determiner */
        /* Head noun: first NOUN after the determiner, skipping adjectives. */
        uint32_t hj = nt; bool head_plural = false;
        for (uint32_t j = i+1; j < nt; j++) {
            gl_word_class_t c = gl_f4_class(gl, low[j]);
            if (c == GL_CLASS_ADJECTIVE) continue;
            if (c == GL_CLASS_NOUN) {
                hj = j;
                head_plural = gl_agr_already_inflected(low[j]) &&
                              low[j][strlen(low[j])-1] == 's';
                break;
            }
            break;  /* non-adjective non-noun → no clear head */
        }
        const char* nextlow = low[i+1];            /* phonetics anchor */
        if (hj < nt && head_plural) {
            /* a/an can't govern a plural; force definite. */
            GL_F4_SET(i, "the");
        } else if (hj < nt) {
            bool first_mention = true;
            for (uint32_t s = 0; s < n_seen; s++)
                if (strcmp(seen[s], low[hj]) == 0) { first_mention = false; break; }
            if (strcmp(low[i], "the") == 0) {
                if (first_mention) GL_F4_SET(i, gl_article_for(nextlow));
                /* re-mention: leave "the" */
            } else {
                /* already indefinite → just fix a/an phonetics */
                GL_F4_SET(i, gl_article_for(nextlow));
            }
            if (n_seen < MAXTOK) {
                size_t hl = strlen(low[hj]);
                if (hl < TOKLEN) { memcpy(seen[n_seen], low[hj], hl+1); n_seen++; }
            }
        } else if (strcmp(low[i], "the") != 0) {
            /* indefinite with no clear head → still fix phonetics */
            GL_F4_SET(i, gl_article_for(nextlow));
        }
    }

    /* ---- F4b: tense/aspect consistency ------------------------------- */
    /* Anchor on the first finite verb; if it's past, drag following
     * present/base verbs into the past so the clause is tense-consistent. */
    {
        uint32_t vi = nt;
        for (uint32_t i = 0; i < nt; i++) {
            if (gl_agr_in_set(low[i], SKIP_SLOT)) continue;
            /* Anchor on the first finite verb — including irregular pasts that
             * morphology can't suffix-classify (ran/ate/went/saw), detected via
             * gl_f4_is_past — so an irregular-led clause still sets the tense
             * baseline instead of latching onto a later derived verb. */
            if (gl_f4_class(gl, low[i]) == GL_CLASS_VERB ||
                gl_f4_is_past(low[i])) { vi = i; break; }
        }
        if (vi < nt && gl_f4_is_past(low[vi])) {
            for (uint32_t j = vi+1; j < nt; j++) {
                if (gl_f4_class(gl, low[j]) != GL_CLASS_VERB) continue;
                if (gl_agr_in_set(low[j], MODALS)) continue;
                if (gl_f4_is_past(low[j])) continue;
                char pt[TOKLEN];
                if (gl_morph_past_tense(low[j], pt, sizeof(pt)) > 0 &&
                    strcmp(pt, low[j]) != 0)
                    GL_F4_SET(j, pt);
            }
        }
    }

    /* ---- F4c: pronoun case + possessives ----------------------------- */
    /* (1) Subject slot — first content token must be a subject pronoun. */
    uint32_t si = 0;
    while (si < nt && gl_agr_in_set(low[si], SKIP_SLOT)) si++;
    if (si < nt) {
        for (int m = 0; OBJ2SUBJ[m].from; m++)
            if (strcmp(low[si], OBJ2SUBJ[m].from) == 0) {
                GL_F4_SET(si, OBJ2SUBJ[m].to); break;
            }
    }
    /* (2) Object slot — content token right after the first verb. */
    {
        uint32_t vk = nt;
        for (uint32_t i = (si < nt ? si+1 : 0); i < nt; i++)
            if (gl_f4_class(gl, low[i]) == GL_CLASS_VERB) { vk = i; break; }
        if (vk < nt) {
            uint32_t ok = vk + 1;
            while (ok < nt && gl_agr_in_set(low[ok], SKIP_SLOT)) ok++;
            if (ok < nt)
                for (int m = 0; SUBJ2OBJ[m].from; m++)
                    if (strcmp(low[ok], SUBJ2OBJ[m].from) == 0) {
                        GL_F4_SET(ok, SUBJ2OBJ[m].to); break;
                    }
        }
    }
    /* (3) Possessive determiner — bare pronoun immediately before a noun
     * ("he book" → "his book"). */
    for (uint32_t i = 0; i + 1 < nt; i++) {
        if (gl_f4_class(gl, low[i+1]) != GL_CLASS_NOUN) continue;
        for (int m = 0; POSSDET[m].from; m++)
            if (strcmp(low[i], POSSDET[m].from) == 0) {
                GL_F4_SET(i, POSSDET[m].to); break;
            }
    }
    /* (4) Noun-noun possessive 's ("dog tail" → "dog's tail"). Requires BOTH
     * tokens to resolve to NOUN (confident lexicon tag or morphology hint) so
     * an N+V pair isn't mangled; first noun must be singular / not already
     * possessive. The NOUN gate keeps this off bare short words (run/sees),
     * which classify UNKNOWN. */
    for (uint32_t i = 0; i + 1 < nt; i++) {
        if (gl_f4_class(gl, low[i])   != GL_CLASS_NOUN) continue;
        if (gl_f4_class(gl, low[i+1]) != GL_CLASS_NOUN) continue;
        size_t al = strlen(tok[i]);
        if (al == 0 || tok[i][al-1] == 's') continue;     /* skip plural/poss */
        if (al + 2 < TOKLEN) {
            char poss[TOKLEN];
            snprintf(poss, sizeof(poss), "%s's", tok[i]);
            GL_F4_SET(i, poss);
            /* RC2 (2026-06-01): exactly one possessive per utterance. A produced
             * run of adjacent abstract nouns ("thought belief problem ...") must
             * not cascade into "thought's belief's problem's ..." — only the
             * first noun-noun pair is treated as genitive. The real fix for the
             * noun-run itself is structural (clause planner); this caps the
             * surface damage. "dog tail" -> "dog's tail" still works. */
            break;
        }
    }

    #undef GL_F4_SET

    /* Rebuild. */
    size_t pos = 0;
    for (uint32_t i = 0; i < nt; i++) {
        size_t wl = strlen(tok[i]);
        if (pos + (i?1:0) + wl + 1 > out_sz) break;
        if (i) out[pos++] = ' ';
        memcpy(out+pos, tok[i], wl);
        pos += wl;
    }
    out[pos] = '\0';
    return fixes;
}

void grounded_language_set_autoregressive_produce(grounded_language_t* gl,
                                                  bool enabled) {
    if (gl) gl->autoregressive_produce = enabled;
}

bool grounded_language_get_autoregressive_produce(const grounded_language_t* gl) {
    return gl ? gl->autoregressive_produce : false;
}

void grounded_language_set_produce_pronominalize(grounded_language_t* gl,
                                                 bool enabled) {
    if (gl) gl->produce_pronominalize = enabled;
}

bool grounded_language_get_produce_pronominalize(const grounded_language_t* gl) {
    return gl ? gl->produce_pronominalize : false;
}

void grounded_language_set_produce_discourse_seed(grounded_language_t* gl,
                                                  bool enabled) {
    if (gl) gl->produce_discourse_seed = enabled;
}

bool grounded_language_get_produce_discourse_seed(const grounded_language_t* gl) {
    return gl ? gl->produce_discourse_seed : false;
}

void grounded_language_set_produce_clause_frame(grounded_language_t* gl,
                                                bool enabled) {
    if (gl) gl->produce_clause_frame = enabled;
}

bool grounded_language_get_produce_clause_frame(const grounded_language_t* gl) {
    return gl ? gl->produce_clause_frame : false;
}

void grounded_language_set_produce_via_snn(grounded_language_t* gl, bool enabled) {
    if (gl) gl->produce_via_snn = enabled;
}

bool grounded_language_get_produce_via_snn(const grounded_language_t* gl) {
    return gl ? gl->produce_via_snn : false;
}

void grounded_language_set_snn_gen_params(grounded_language_t* gl,
                                          uint32_t n_steps, float inject,
                                          uint32_t max_concepts) {
    if (!gl) return;
    if (n_steps > 0) gl->snn_gen_n_steps = n_steps;
    if (inject > 0.0f) gl->snn_gen_inject = inject;
    if (max_concepts > 0) gl->snn_gen_max_concepts = (max_concepts > 8u) ? 8u : max_concepts;
}

void grounded_language_get_snn_gen_params(const grounded_language_t* gl,
                                          uint32_t* n_steps, float* inject,
                                          uint32_t* max_concepts) {
    if (!gl) return;
    if (n_steps) *n_steps = gl->snn_gen_n_steps;
    if (inject)  *inject  = gl->snn_gen_inject;
    if (max_concepts) *max_concepts = gl->snn_gen_max_concepts;
}

void grounded_language_set_metacog_gates_produce(grounded_language_t* gl, bool enabled) {
    if (gl) gl->metacog_gates_produce = enabled;
}

bool grounded_language_get_metacog_gates_produce(const grounded_language_t* gl) {
    return gl ? gl->metacog_gates_produce : false;
}

/* Runtime per-produce setter (called by the cascade from brain modulatory
 * state). Clamped to [-1, 1]; the floor itself is re-clamped to [0,1] at use.
 * Positive raises the floor (more conservative), negative lowers it. */
void grounded_language_set_metacog_floor_adjust(grounded_language_t* gl, float adjust) {
    if (!gl) return;
    if (!isfinite(adjust)) adjust = 0.0f;
    if (adjust < -1.0f) adjust = -1.0f;
    if (adjust >  1.0f) adjust =  1.0f;
    gl->metacog_floor_adjust = adjust;
}

float grounded_language_get_metacog_floor_adjust(const grounded_language_t* gl) {
    return gl ? gl->metacog_floor_adjust : 0.0f;
}

uint32_t grounded_language_collect_warmstart_bindings(
    const grounded_language_t* gl, gl_warmstart_binding_t* out, uint32_t max) {
    if (!gl || !out || max == 0 || !gl->vocab_list) return 0;
    uint32_t n = 0;
    for (uint32_t v = 0; v < gl->vocab_count && n < max; v++) {
        const gl_lexicon_entry_t* e = gl->vocab_list[v];
        if (!e || e->binding_count == 0 || !e->bindings) continue;
        /* Strongest binding for this word. */
        const gl_word_binding_t* top = &e->bindings[0];
        for (uint32_t b = 1; b < e->binding_count; b++) {
            if (e->bindings[b].strength > top->strength) top = &e->bindings[b];
        }
        if (!(top->strength > 0.0f)) continue;
        out[n].form_hash  = e->form_hash;
        out[n].concept_id = top->concept_id;
        out[n].strength   = top->strength;
        n++;
    }
    return n;
}

uint32_t grounded_language_top_concepts_for_intent(
    const grounded_language_t* gl, const float* intent, uint32_t dim,
    uint64_t* out_concept_ids, uint32_t max) {
    if (!gl || !intent || !out_concept_ids || max == 0 || !gl->vocab_list) return 0;
    /* Score every word's distributional context_vector against the intent and
     * keep the top-`max` DISTINCT concept_ids (each word's strongest binding).
     * These are the concepts to seed into Wernicke for SNN generation: the
     * concept→word projection then drives Broca toward the associated words.
     * Mirrors find_words_near_vector's scoring but returns concepts, not words. */
    uint32_t cmp = (dim < gl->semantic_dim) ? dim : gl->semantic_dim;
    uint64_t best_ids[64];
    float    best_sc[64];
    uint32_t cnt = 0;
    if (max > 64) max = 64;
    for (uint32_t v = 0; v < gl->vocab_count; v++) {
        const gl_lexicon_entry_t* e = gl->vocab_list[v];
        if (!e || !e->context_initialized || !e->context_vector ||
            e->binding_count == 0 || !e->bindings) continue;
        float sim = cosine_similarity(intent, e->context_vector, cmp);
        if (!(sim > 0.0f)) continue;
        const gl_word_binding_t* top = &e->bindings[0];
        for (uint32_t b = 1; b < e->binding_count; b++)
            if (e->bindings[b].strength > top->strength) top = &e->bindings[b];
        uint64_t cid = top->concept_id;
        /* Skip if this concept_id is already collected (distinct). */
        bool dup = false;
        for (uint32_t k = 0; k < cnt; k++) if (best_ids[k] == cid) { dup = true; break; }
        if (dup) continue;
        /* Insertion sort by sim, keep top-max. */
        if (cnt < max) {
            uint32_t p = cnt;
            while (p > 0 && best_sc[p-1] < sim) { best_ids[p]=best_ids[p-1]; best_sc[p]=best_sc[p-1]; p--; }
            best_ids[p] = cid; best_sc[p] = sim; cnt++;
        } else if (sim > best_sc[max-1]) {
            uint32_t p = max-1;
            while (p > 0 && best_sc[p-1] < sim) { best_ids[p]=best_ids[p-1]; best_sc[p]=best_sc[p-1]; p--; }
            best_ids[p] = cid; best_sc[p] = sim;
        }
    }
    for (uint32_t k = 0; k < cnt; k++) out_concept_ids[k] = best_ids[k];
    return cnt;
}

void grounded_language_set_produce_givenness_definite(grounded_language_t* gl,
                                                      bool enabled) {
    if (gl) gl->produce_givenness_definite = enabled;
}

bool grounded_language_get_produce_givenness_definite(const grounded_language_t* gl) {
    return gl ? gl->produce_givenness_definite : false;
}

void grounded_language_set_produce_t3_conjunction(grounded_language_t* gl,
                                                  bool enabled) {
    if (gl) gl->produce_t3_conjunction = enabled;
}

bool grounded_language_get_produce_t3_conjunction(const grounded_language_t* gl) {
    return gl ? gl->produce_t3_conjunction : false;
}

/*============================================================================
 * TF-2 — Tier-feedback flag plumbing.
 *==========================================================================*/
void grounded_language_set_produce_corrector_feedback_enabled(grounded_language_t* gl,
                                                              bool enabled) {
    if (gl) gl->produce_corrector_feedback_enabled = enabled;
}

bool grounded_language_get_produce_corrector_feedback_enabled(const grounded_language_t* gl) {
    return gl ? gl->produce_corrector_feedback_enabled : false;
}

void grounded_language_set_tf_enabled_correctors(grounded_language_t* gl,
                                                 uint16_t mask) {
    if (gl) gl->tf_enabled_correctors = (uint16_t)(mask & GL_TF_BIT_ALL_CORRECTORS);
}

uint16_t grounded_language_get_tf_enabled_correctors(const grounded_language_t* gl) {
    return gl ? gl->tf_enabled_correctors : 0u;
}

/* Common clamp helper — NaN/neg -> 0, > max -> max. */
static float tf_clamp_lr(float lr, float max) {
    if (!(lr >= 0.0f)) return 0.0f;   /* NaN or negative */
    if (lr > max)     return max;
    return lr;
}

void grounded_language_set_tf_lr_trigram(grounded_language_t* gl, float lr) {
    if (gl) gl->tf_lr_trigram = tf_clamp_lr(lr, GL_TF_LR_TRIGRAM_MAX);
}

float grounded_language_get_tf_lr_trigram(const grounded_language_t* gl) {
    return gl ? gl->tf_lr_trigram : GL_TF_LR_TRIGRAM_DEFAULT;
}

void grounded_language_set_tf_lr_distrib(grounded_language_t* gl, float lr) {
    if (gl) gl->tf_lr_distrib = tf_clamp_lr(lr, GL_TF_LR_DISTRIB_MAX);
}

float grounded_language_get_tf_lr_distrib(const grounded_language_t* gl) {
    return gl ? gl->tf_lr_distrib : GL_TF_LR_DISTRIB_DEFAULT;
}

void grounded_language_set_tf_lr_bridge_stdp(grounded_language_t* gl, float lr) {
    if (gl) gl->tf_lr_bridge_stdp = tf_clamp_lr(lr, GL_TF_LR_BRIDGE_STDP_MAX);
}

float grounded_language_get_tf_lr_bridge_stdp(const grounded_language_t* gl) {
    return gl ? gl->tf_lr_bridge_stdp : GL_TF_LR_BRIDGE_STDP_DEFAULT;
}

/* Strip leading/trailing non-alpha (punctuation) from a lowercased token into
 * `out`, producing a clean lemma for class lookup + same-word matching. So
 * "barked." -> "barked", "creation," -> "creation". Internal apostrophes/
 * hyphens are preserved (only the outer rind is removed). Empty if no alpha. */
static void gl_pron_lemma(const char* low_tok, char* out, size_t cap) {
    if (cap == 0) return;
    size_t n = strlen(low_tok), b = 0, e = n;
    while (b < e && !(low_tok[b] >= 'a' && low_tok[b] <= 'z')) b++;
    while (e > b && !(low_tok[e-1] >= 'a' && low_tok[e-1] <= 'z')) e--;
    size_t len = (e > b) ? (e - b) : 0;
    if (len > cap - 1) len = cap - 1;
    memcpy(out, low_tok + b, len);
    out[len] = '\0';
}

/* Trailing punctuation rind of a (mixed-case) token: the maximal suffix of
 * non-alphanumeric chars. So "creation." -> ".", "cat" -> "". Used to re-attach
 * sentence/clause punctuation after replacing a noun with a pronoun. */
static const char* gl_pron_trailing_punct(const char* tok) {
    size_t n = strlen(tok), e = n;
    while (e > 0) {
        char c = tok[e-1];
        bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                     (c >= '0' && c <= '9');
        if (alnum) break;
        e--;
    }
    return tok + e;  /* points at the first trailing-punct char (or the NUL) */
}

/*============================================================================
 * Tier 2 — produce-side pronominalization (recency anaphora)
 *   A re-mentioned non-person noun becomes it/they (subject) or it/them
 *   (object) so replies don't robotically repeat the noun. Person nouns are
 *   guarded out (gender unknown). Conservative, closed-class + position based,
 *   mirroring the F3/F4 surface-corrector pattern.
 *
 *   T2-3 (2026-05-26): class/lemma detection is punctuation-tolerant (so a
 *   clause-final "barked." is still seen as a verb and "creation." still
 *   matches "creation"), subject vs object is decided by a following-verb
 *   lookahead (noun->verb = subject "they"; verb->noun = object "them"), and
 *   any trailing punctuation on the replaced noun is re-attached to the
 *   pronoun so sentence boundaries survive.
 *==========================================================================*/
int gl_apply_pronominalization(grounded_language_t* gl, const char* in,
                               char* out, size_t out_sz) {
    if (!in || !out || out_sz == 0) { if (out && out_sz) out[0]='\0'; return -1; }
    /* Developmental gate (matches Step C/F2/F4): off at stages 0-1. NULL gl
     * is ungated for unit tests. */
    if (gl && gl->current_stage < 2) {
        size_t L = strlen(in);
        if (L + 1 > out_sz) L = out_sz - 1;
        memcpy(out, in, L); out[L] = '\0';
        return 0;
    }

    static const char* DET2[]   = {"the","a","an",NULL};
    /* Closed-class person/animate guard — pronominalizing these to "it" would
     * be wrong (they need he/she, gender unknown), so skip them entirely. */
    static const char* PERSON[] = {"man","woman","men","women","person","people",
        "child","children","boy","girl","boys","girls","baby","babies","mother",
        "father","mom","dad","parent","parents","king","queen","lady","guy",
        "teacher","doctor","friend","friends","someone","everyone","god","he",
        "she","him","her","i","you","we","they","who",NULL};

    /* MAXTOK bounds the correctable surface length — see the note in the F3/F4
     * correctors; 128 gives ~2x headroom over the produce cap so re-mentions
     * past the old 48-token limit are no longer silently dropped. */
    enum { MAXTOK = 128, TOKLEN = 64, WINDOW = 10 };
    char tok[MAXTOK][TOKLEN];
    char low[MAXTOK][TOKLEN];
    char lem[MAXTOK][TOKLEN];   /* punctuation-stripped lemma (class + match) */
    uint32_t nt = 0;

    {
        size_t i = 0, L = strlen(in);
        while (i < L && nt < MAXTOK) {
            while (i < L && (in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r')) i++;
            if (i >= L) break;
            uint32_t k = 0;
            while (i < L && !(in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r')
                   && k < TOKLEN-1) tok[nt][k++] = in[i++];
            tok[nt][k] = '\0';
            gl_agr_lower(tok[nt], low[nt], TOKLEN);
            gl_pron_lemma(low[nt], lem[nt], TOKLEN);
            nt++;
            while (i < L && !(in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r')) i++;
        }
    }

    int fixes = 0;
    for (uint32_t i = 0; i < nt; i++) {
        /* All class/match work uses the punctuation-stripped lemma so a
         * clause-final "creation." is still recognized as a re-mentioned
         * NOUN and "barked." still counts as a verb (T2-3). */
        if (!lem[i][0]) continue;
        if (gl_f4_class(gl, lem[i]) != GL_CLASS_NOUN) continue;
        if (gl_agr_in_set(lem[i], PERSON)) continue;
        /* Prior mention of the same lemma within the recency window? */
        uint32_t lo = (i > WINDOW) ? (i - WINDOW) : 0;
        int prior = -1;
        for (uint32_t j = lo; j < i; j++)
            if (lem[j][0] && strcmp(lem[j], lem[i]) == 0) { prior = (int)j; break; }
        if (prior < 0) continue;
        /* Require a verb between the two mentions (object / new-clause slot). */
        bool verb_between = false;
        for (uint32_t j = (uint32_t)prior + 1; j < i; j++)
            if (gl_f4_class(gl, lem[j]) == GL_CLASS_VERB) { verb_between = true; break; }
        if (!verb_between) continue;

        /* Subject vs object (T2-3): a re-mention is a SUBJECT if the nearest
         * FOLLOWING content token (skipping determiners / dropped slots) is a
         * verb ("the cat ... the cat ran" -> "it ran"), else an OBJECT if the
         * nearest PRECEDING content token is a verb ("chased the cat" ->
         * "chased it"). Drives they(subject)/them(object) for plurals; "it" is
         * case-invariant for singulars either way. */
        bool subject_pos = false, object_pos = false;
        {
            int k = (int)i + 1;
            while (k < (int)nt && (lem[k][0] == '\0' || gl_agr_in_set(lem[k], DET2))) k++;
            if (k < (int)nt && gl_f4_class(gl, lem[k]) == GL_CLASS_VERB) subject_pos = true;
        }
        if (!subject_pos) {
            int k = (int)i - 1;
            while (k >= 0 && (lem[k][0] == '\0' || gl_agr_in_set(lem[k], DET2))) k--;
            if (k >= 0 && gl_f4_class(gl, lem[k]) == GL_CLASS_VERB) object_pos = true;
        }
        /* Only pronominalize when the grammatical role is CONFIRMED by an
         * adjacent verb — subject (following verb -> "they") or object
         * (preceding verb -> "them"). An unanchored re-mention (neither, e.g.
         * a list/appositive slot) is left as the full noun rather than guessing
         * a plural pronoun by fallthrough. Singulars are "it" either way. */
        if (!subject_pos && !object_pos) continue;
        size_t wl = strlen(lem[i]);
        bool plural = (wl >= 1 && lem[i][wl-1] == 's' && gl_agr_already_inflected(lem[i]));
        const char* pro = plural ? (subject_pos ? "they" : "them") : "it";

        /* Drop a preceding determiner so "chased the cat" -> "chased it". */
        if (i > 0 && gl_agr_in_set(lem[i-1], DET2)) {
            tok[i-1][0] = '\0'; low[i-1][0] = '\0'; lem[i-1][0] = '\0';
        }
        /* Re-attach the noun's trailing punctuation to the pronoun so a
         * sentence/clause boundary survives ("creation." -> "it."). */
        char repl[TOKLEN];
        const char* tail = gl_pron_trailing_punct(tok[i]);
        snprintf(repl, sizeof(repl), "%s%s", pro, tail);
        memcpy(tok[i], repl, strlen(repl) + 1);
        gl_agr_lower(tok[i], low[i], TOKLEN);
        gl_pron_lemma(low[i], lem[i], TOKLEN);
        fixes++;
    }

    /* Rebuild, skipping dropped (empty) tokens. */
    size_t pos = 0;
    for (uint32_t i = 0; i < nt; i++) {
        size_t wl = strlen(tok[i]);
        if (wl == 0) continue;
        if (pos + (pos ? 1 : 0) + wl + 1 > out_sz) break;
        if (pos) out[pos++] = ' ';
        memcpy(out + pos, tok[i], wl);
        pos += wl;
    }
    out[pos] = '\0';
    return fixes;
}

/*============================================================================
 * T3-1 (2026-05-28) — Givenness-driven definiteness.
 *
 * Runs AFTER T2 pronominalization (gl_apply_pronominalization). For every
 * NOUN token at position i that's a re-mention of an earlier NOUN within the
 * recency window WINDOW, if the immediately-preceding non-empty token is the
 * indefinite article "a" or "an", swap it to "the" (case-preserving). Tokens
 * that T2-1 already replaced with a pronoun are no longer nouns and naturally
 * skip this pass; person nouns that T2-1 spared (gender unknown) get marked
 * as given here.
 *
 * Conservative idempotent: a "the" preceding the re-mention is left alone, so
 * a second pass on the polished output is a no-op. Punctuation-tolerant via
 * gl_pron_lemma so "creation." still matches "creation". Stage-gated >= 2 to
 * match every other Tier 2/3 produce-side corrector.
 *==========================================================================*/
int gl_apply_givenness_definite(grounded_language_t* gl, const char* in,
                                char* out, size_t out_sz) {
    if (!in || !out || out_sz == 0) { if (out && out_sz) out[0]='\0'; return -1; }
    if (gl && gl->current_stage < 2) {
        size_t L = strlen(in);
        if (L + 1 > out_sz) L = out_sz - 1;
        memcpy(out, in, L); out[L] = '\0';
        return 0;
    }

    enum { MAXTOK = 128, TOKLEN = 64, WINDOW = 10 };
    char tok[MAXTOK][TOKLEN];
    char low[MAXTOK][TOKLEN];
    char lem[MAXTOK][TOKLEN];
    uint32_t nt = 0;

    {
        size_t i = 0, L = strlen(in);
        while (i < L && nt < MAXTOK) {
            while (i < L && (in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r')) i++;
            if (i >= L) break;
            uint32_t k = 0;
            while (i < L && !(in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r')
                   && k < TOKLEN-1) tok[nt][k++] = in[i++];
            tok[nt][k] = '\0';
            gl_agr_lower(tok[nt], low[nt], TOKLEN);
            gl_pron_lemma(low[nt], lem[nt], TOKLEN);
            nt++;
            while (i < L && !(in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r')) i++;
        }
    }

    int fixes = 0;
    for (uint32_t i = 1; i < nt; i++) {
        if (!lem[i][0]) continue;
        if (gl_f4_class(gl, lem[i]) != GL_CLASS_NOUN) continue;
        /* Prior mention within recency window? */
        uint32_t lo = (i > WINDOW) ? (i - WINDOW) : 0;
        bool prior = false;
        for (uint32_t j = lo; j < i; j++) {
            if (lem[j][0] && strcmp(lem[j], lem[i]) == 0) { prior = true; break; }
        }
        if (!prior) continue;
        /* Find the immediately-preceding non-empty token. */
        int p = (int)i - 1;
        while (p >= 0 && tok[p][0] == '\0') p--;
        if (p < 0) continue;
        /* Only fire on "a"/"an" — leave "the"/possessives/etc. alone. */
        if (!(strcmp(low[p], "a") == 0 || strcmp(low[p], "an") == 0)) continue;
        /* Case-preserve: leading capital -> "The", else "the". */
        bool cap = (tok[p][0] >= 'A' && tok[p][0] <= 'Z');
        const char* repl = cap ? "The" : "the";
        size_t rl = strlen(repl);
        if (rl >= TOKLEN) continue;
        memcpy(tok[p], repl, rl + 1);
        gl_agr_lower(tok[p], low[p], TOKLEN);
        gl_pron_lemma(low[p], lem[p], TOKLEN);
        fixes++;
    }

    /* Rebuild, skipping dropped (empty) tokens. */
    size_t pos = 0;
    for (uint32_t i = 0; i < nt; i++) {
        size_t wl = strlen(tok[i]);
        if (wl == 0) continue;
        if (pos + (pos ? 1 : 0) + wl + 1 > out_sz) break;
        if (pos) out[pos++] = ' ';
        memcpy(out + pos, tok[i], wl);
        pos += wl;
    }
    out[pos] = '\0';
    return fixes;
}

/*=============================================================================
 * T3-2 (2026-05-29) — Cohesive conjunction insertion.
 *
 * Runs AFTER T3-1 givenness. Scans the polished utterance for the first
 * adjacent SVO pair without an existing connective and inserts "and" before
 * the second clause's leading determiner / noun / pronoun:
 *
 *   "the cat sat the dog ran" -> "the cat sat and the dog ran"
 *
 * Detection: clause-A VERB at position v1, then the FIRST candidate clause-B
 * start at j > v1 (j is a DET word OR NOUN class OR PRONOUN), then a second
 * VERB at k > j. The candidate is skipped if its lowercased form is already
 * a connective (and/but/so/or/then/while/because/although).
 *
 * Idempotent: re-running on the polished output hits the "and" at position
 * j (now a connective) → no insert.
 *
 * Stage-gated >= 2 to match every other Tier 2/3 produce-side corrector.
 * Caps at ONE insert per call — multi-clause coordination is left to higher
 * tiers.
 *==========================================================================*/
static bool t3_conj_is_connective(const char* low) {
    static const char* C[] = {"and","but","so","or","then","while",
                              "because","although",NULL};
    for (int i = 0; C[i]; i++) if (strcmp(low, C[i]) == 0) return true;
    return false;
}

/* Pronoun shortlist — mirrors gl_apply_pronominalization's surface guard
 * so cold-lexicon clause detection works without learned class confidence. */
static bool t3_conj_is_pronoun_surface(const char* low) {
    static const char* P[] = {"he","she","it","they","we","i","you",
                              "him","her","them","us","me",
                              "his","hers","its","their","our","my","your",
                              NULL};
    for (int i = 0; P[i]; i++) if (strcmp(low, P[i]) == 0) return true;
    return false;
}

/* Verb-detection mirror of gl_f4_is_past + present markers. Production gl
 * will usually classify via gl_f4_class; the surface fallback catches the
 * cold-lexicon case where the curriculum hasn't tagged the verb yet. */
static bool t3_conj_is_verb(grounded_language_t* gl, const char* low) {
    if (!low || !low[0]) return false;
    if (gl_f4_class(gl, low) == GL_CLASS_VERB) return true;
    size_t n = strlen(low);
    /* -ed past, -ing progressive — same shape gl_f4_is_past covers. */
    if (n >= 4 && low[n-2]=='e' && low[n-1]=='d') return true;
    if (n >= 5 && low[n-3]=='i' && low[n-2]=='n' && low[n-1]=='g') return true;
    /* Common irregular pasts + finite forms that won't carry a suffix.
     * Same shortlist as gl_apply_f4_fluency::gl_f4_is_past plus present
     * forms ("is", "are", "has", "does") that anchor a clause. */
    static const char* V[] = {
        "was","were","is","are","am","be","been","being","has","have","had",
        "do","does","did","go","goes","went","gone","run","runs","ran",
        "make","makes","made","take","takes","took","taken","get","gets",
        "got","gotten","see","sees","saw","seen","come","comes","came",
        "ate","saw","said","told","found","felt","became","left","brought",
        "bought","caught","taught","stood","spoke","wrote","held","kept",
        "slept","met","paid","sold","sent","built","won","lost","fell",
        "fed","heard","sat","sang","swam","drank","began","understood",
        "jumped","barked","purred","walked","talked","wanted","needed",
        NULL};
    for (int i = 0; V[i]; i++) if (strcmp(low, V[i]) == 0) return true;
    return false;
}

static bool t3_conj_is_clause_start(grounded_language_t* gl,
                                    const char* low) {
    if (!low || !low[0]) return false;
    /* Common determiners (closed-class — the lexicon may not classify them
     * yet during stage transitions). */
    if (strcmp(low, "a") == 0 || strcmp(low, "an") == 0 ||
        strcmp(low, "the") == 0) return true;
    if (t3_conj_is_pronoun_surface(low)) return true;
    gl_word_class_t c = gl_f4_class(gl, low);
    if (c == GL_CLASS_NOUN || c == GL_CLASS_PRONOUN) return true;
    return false;
}

int gl_apply_t3_conjunction(grounded_language_t* gl, const char* in,
                            char* out, size_t out_sz) {
    if (!in || !out || out_sz == 0) { if (out && out_sz) out[0]='\0'; return -1; }
    if (gl && gl->current_stage < 2) {
        size_t L = strlen(in);
        if (L + 1 > out_sz) L = out_sz - 1;
        memcpy(out, in, L); out[L] = '\0';
        return 0;
    }

    enum { MAXTOK = 128, TOKLEN = 64 };
    char tok[MAXTOK][TOKLEN];
    char low[MAXTOK][TOKLEN];
    uint32_t nt = 0;

    {
        size_t i = 0, L = strlen(in);
        while (i < L && nt < MAXTOK) {
            while (i < L && (in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r')) i++;
            if (i >= L) break;
            uint32_t k = 0;
            while (i < L && !(in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r')
                   && k < TOKLEN-1) tok[nt][k++] = in[i++];
            tok[nt][k] = '\0';
            gl_agr_lower(tok[nt], low[nt], TOKLEN);
            nt++;
            while (i < L && !(in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r')) i++;
        }
    }
    if (nt < 4) {
        /* Too short for two clauses. */
        size_t L = strlen(in);
        if (L + 1 > out_sz) L = out_sz - 1;
        memcpy(out, in, L); out[L] = '\0';
        return 0;
    }

    /* Locate clause-A verb. */
    int v1 = -1;
    for (uint32_t i = 1; i < nt; i++) {
        if (t3_conj_is_verb(gl, low[i])) { v1 = (int)i; break; }
    }
    if (v1 < 0 || v1 >= (int)nt - 2) {
        /* No clause-A verb, or no room after it for clause B. */
        size_t L = strlen(in);
        if (L + 1 > out_sz) L = out_sz - 1;
        memcpy(out, in, L); out[L] = '\0';
        return 0;
    }

    /* Find first clause-start candidate AFTER v1. */
    int j = -1;
    for (uint32_t i = (uint32_t)v1 + 1; i < nt; i++) {
        if (t3_conj_is_connective(low[i])) {
            /* Already coordinated — bail. */
            size_t L = strlen(in);
            if (L + 1 > out_sz) L = out_sz - 1;
            memcpy(out, in, L); out[L] = '\0';
            return 0;
        }
        if (t3_conj_is_clause_start(gl, low[i])) { j = (int)i; break; }
    }
    if (j < 0) {
        size_t L = strlen(in);
        if (L + 1 > out_sz) L = out_sz - 1;
        memcpy(out, in, L); out[L] = '\0';
        return 0;
    }

    /* Confirm clause-B exists: a VERB at some k > j. */
    int v2 = -1;
    for (uint32_t i = (uint32_t)j + 1; i < nt; i++) {
        if (t3_conj_is_verb(gl, low[i])) { v2 = (int)i; break; }
    }
    if (v2 < 0) {
        /* Candidate exists but no verb after it — not a real clause B. */
        size_t L = strlen(in);
        if (L + 1 > out_sz) L = out_sz - 1;
        memcpy(out, in, L); out[L] = '\0';
        return 0;
    }

    /* Rebuild with "and " inserted before token j. */
    size_t pos = 0;
    for (uint32_t i = 0; i < nt; i++) {
        if ((int)i == j) {
            const char* conj = "and";
            size_t cl = 3;
            if (pos + (pos ? 1 : 0) + cl > out_sz - 1) break;
            if (pos) out[pos++] = ' ';
            memcpy(out + pos, conj, cl);
            pos += cl;
        }
        size_t wl = strlen(tok[i]);
        if (wl == 0) continue;
        if (pos + (pos ? 1 : 0) + wl + 1 > out_sz) break;
        if (pos) out[pos++] = ' ';
        memcpy(out + pos, tok[i], wl);
        pos += wl;
    }
    out[pos] = '\0';
    return 1;
}

void grounded_language_set_discourse_capacity(grounded_language_t* gl,
                                                uint8_t capacity) {
    if (!gl) return;
    if (capacity < 1) capacity = 1;
    if (capacity > GL_DISCOURSE_MAX_TURNS) capacity = GL_DISCOURSE_MAX_TURNS;

    /* Capacity reduction: evict oldest turns (free their vectors) until
     * count fits. The newest `capacity` turns survive — copy them down
     * into [0, count) and reset head to 0 so the layout is canonical. */
    if (capacity < gl->discourse.capacity && gl->discourse.count > capacity) {
        uint8_t keep = capacity;
        gl_discourse_turn_t survivors[GL_DISCOURSE_MAX_TURNS];
        memset(survivors, 0, sizeof(survivors));

        /* Newest `keep` turns are at positions [count-keep, count). */
        uint8_t drop = (uint8_t)(gl->discourse.count - keep);
        for (uint8_t i = 0; i < drop; i++) {
            uint8_t idx = discourse_idx(&gl->discourse, i);
            nimcp_free(gl->discourse.turns[idx].semantic_vector);
            gl->discourse.turns[idx].semantic_vector = NULL;
        }
        for (uint8_t i = 0; i < keep; i++) {
            uint8_t src = discourse_idx(&gl->discourse, (uint8_t)(drop + i));
            survivors[i] = gl->discourse.turns[src];
            /* Defensive: if src and dst overlap and the source slot
             * gets memset below, we'd lose the pointer. Clear the
             * source by hand once we've copied. */
            gl->discourse.turns[src].semantic_vector = NULL;
        }
        /* Wipe the whole array (all heap pointers either freed or
         * moved to survivors[]) and reinstate the survivors at [0, keep). */
        memset(gl->discourse.turns, 0, sizeof(gl->discourse.turns));
        for (uint8_t i = 0; i < keep; i++) {
            gl->discourse.turns[i] = survivors[i];
        }
        gl->discourse.head  = 0;
        gl->discourse.count = keep;
    }

    gl->discourse.capacity = capacity;
    discourse_rebuild_context_blend(gl);
}

/*=============================================================================
 * TB-10 — Topic-shift detection in discourse.
 *
 * Detector returns the cosine similarity between the *most recent*
 * discourse turn (count-1) and the mean of the prior up-to-K turns
 * (positions count-1-K .. count-2 in oldest→newest ordering). Returns
 * 1.0 ("no shift") when fewer than `min_turns` prior turns are
 * available, or when either operand has zero magnitude. Skips empty
 * placeholder turns (NULL semantic_vector) when computing the mean.
 *===========================================================================*/

static float gl_topic_shift_score(const grounded_language_t* gl,
                                    uint32_t min_turns) {
    if (!gl) return 1.0f;
    const gl_discourse_state_t* d = &gl->discourse;
    /* Need: 1 most-recent turn + at least `min_turns` priors. */
    if (d->count < (uint8_t)(min_turns + 1)) return 1.0f;
    if (gl->semantic_dim == 0) return 1.0f;

    /* Latest turn = newest in oldest→newest ordering, position count-1. */
    uint8_t latest_pos = (uint8_t)(d->count - 1);
    const gl_discourse_turn_t* latest_t =
        &d->turns[discourse_idx(d, latest_pos)];
    if (!latest_t->semantic_vector) return 1.0f;

    /* Mean of the prior up-to-min_turns turns (positions
     * count-1-K .. count-2). Skip empty placeholders. */
    float mean[GL_SEMANTIC_DIM];
    /* Defensive cap — semantic_dim should always be ≤ GL_SEMANTIC_DIM
     * because that's the static stack-buffer ceiling, but newer
     * embedding paths can push it higher. Stack-limit by clamping. */
    uint32_t dim = gl->semantic_dim;
    if (dim > GL_SEMANTIC_DIM) dim = GL_SEMANTIC_DIM;
    memset(mean, 0, dim * sizeof(float));

    uint32_t included = 0;
    /* iterate the K priors that sit immediately before the latest. */
    uint8_t start = (uint8_t)(d->count - 1 - min_turns);
    for (uint8_t i = 0; i < (uint8_t)min_turns; i++) {
        const gl_discourse_turn_t* pt =
            &d->turns[discourse_idx(d, (uint8_t)(start + i))];
        if (!pt->semantic_vector) continue;
        for (uint32_t j = 0; j < dim; j++) {
            mean[j] += pt->semantic_vector[j];
        }
        included++;
    }
    if (included == 0) return 1.0f;
    float inv = 1.0f / (float)included;
    for (uint32_t j = 0; j < dim; j++) mean[j] *= inv;

    /* cos = dot(latest, mean) / (||latest|| * ||mean||). */
    double dot = 0.0;
    double n_latest = 0.0;
    double n_mean = 0.0;
    for (uint32_t j = 0; j < dim; j++) {
        double a = (double)latest_t->semantic_vector[j];
        double b = (double)mean[j];
        dot     += a * b;
        n_latest += a * a;
        n_mean   += b * b;
    }
    if (n_latest < 1e-18 || n_mean < 1e-18) return 1.0f;
    double cos = dot / (sqrt(n_latest) * sqrt(n_mean));
    /* Clamp into [0, 1] — negative similarity is more-different-than-
     * orthogonal; treating it as 0 keeps the threshold semantics intact
     * (any negative cosine is well below any sensible threshold). */
    if (cos < 0.0) cos = 0.0;
    if (cos > 1.0) cos = 1.0;
    return (float)cos;
}

void grounded_language_set_topic_shift_enabled(grounded_language_t* gl,
                                                  bool enabled) {
    if (!gl) return;
    gl->enable_topic_shift_detection = enabled;
}

bool grounded_language_get_topic_shift_enabled(const grounded_language_t* gl) {
    if (!gl) return false;
    return gl->enable_topic_shift_detection;
}

void grounded_language_set_topic_shift_threshold(grounded_language_t* gl,
                                                    float threshold) {
    if (!gl) return;
    if (!isfinite(threshold) || threshold < 0.0f) threshold = 0.0f;
    if (threshold > 1.0f) threshold = 1.0f;
    gl->topic_shift_threshold = threshold;
}

float grounded_language_get_topic_shift_threshold(const grounded_language_t* gl) {
    if (!gl) return 0.0f;
    return gl->topic_shift_threshold;
}

void grounded_language_set_topic_shift_min_turns(grounded_language_t* gl,
                                                    uint32_t min_turns) {
    if (!gl) return;
    if (min_turns < 2u) min_turns = 2u;
    if (min_turns > (uint32_t)GL_DISCOURSE_MAX_TURNS_PUBLIC)
        min_turns = (uint32_t)GL_DISCOURSE_MAX_TURNS_PUBLIC;
    gl->topic_shift_min_turns = min_turns;
}

uint32_t grounded_language_get_topic_shift_min_turns(const grounded_language_t* gl) {
    if (!gl) return 0u;
    return gl->topic_shift_min_turns;
}

float grounded_language_get_last_topic_shift_score(const grounded_language_t* gl) {
    if (!gl) return 1.0f;
    return gl->last_topic_shift_score;
}

bool grounded_language_last_was_topic_shift(const grounded_language_t* gl) {
    if (!gl) return false;
    return gl->last_was_topic_shift;
}

/*=============================================================================
 * Grounding (Cross-modal binding)
 *===========================================================================*/

int grounded_language_ground(grounded_language_t* gl, const gl_grounding_event_t* event) {
    if (!gl || !event || !event->word || !event->sensory_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_ground: NULL parameter");
        return -1;
    }

    /* #7 Negative grounding (anti-example).
     * "This is NOT a cat" — weaken the closest existing binding for the
     * matching modality without creating new entries / concepts. The
     * lookup is read-only: if the word isn't in the lexicon there's no
     * binding to weaken, so we return 0 (no-op success). The bus still
     * fires GROUNDED with negative confidence so curriculum modules can
     * count the contrast event. */
    if (event->negative) {
        char lower_w[GL_MAX_WORD_LEN];
        lowercase_word(event->word, lower_w, GL_MAX_WORD_LEN);
        const gl_lexicon_entry_t* found = lexicon_find(gl, lower_w);
        if (found) {
            gl_lexicon_entry_t* mut = (gl_lexicon_entry_t*)found;
            /* Pick binding with highest modality strength on this modality
             * (the one most "implicated" — the anti-example most
             * naturally targets the strongest competing association). */
            uint32_t target = UINT32_MAX;
            float best = 0.0f;
            for (uint32_t i = 0; i < mut->binding_count; i++) {
                float ms = mut->bindings[i].modality_strength[event->modality];
                if (ms > best) { best = ms; target = i; }
            }
            if (target == UINT32_MAX && mut->binding_count > 0) {
                /* No modality-specific signal — fall back to overall
                 * strongest binding. */
                target = 0;
                for (uint32_t i = 1; i < mut->binding_count; i++) {
                    if (mut->bindings[i].strength > mut->bindings[target].strength)
                        target = i;
                }
            }
            if (target != UINT32_MAX) {
                gl_word_binding_t* b = &mut->bindings[target];
                float decay = gl->hebbian_lr * event->attention;
                b->modality_strength[event->modality] -= decay;
                if (b->modality_strength[event->modality] < 0.0f)
                    b->modality_strength[event->modality] = 0.0f;
                b->strength -= decay * 0.5f;  /* overall weaker than mod-specific */
                if (b->strength < 0.0f) b->strength = 0.0f;
                b->last_activation_ms = (uint64_t)time(NULL) * 1000;
            }
        }

        gl->stats.total_negative_groundings++;

        /* Fire GROUNDED with negative confidence to mark the contrast. */
        gl_event_t bus_ev = {0};
        bus_ev.type        = GL_EVENT_GROUNDED;
        bus_ev.word        = event->word;
        bus_ev.concept_id  = 0;
        bus_ev.valence     = event->emotional_valence;
        bus_ev.arousal     = event->emotional_arousal;
        bus_ev.confidence  = -event->attention;   /* sign = anti-learning */
        bus_ev.semantic_vec = event->sensory_features;
        gl_fire_event(gl, &bus_ev);
        return 0;
    }

    /* Find or create lexicon entry */
    gl_lexicon_entry_t* entry = lexicon_find_or_create(gl, event->word);
    if (!entry) return -1;

    /* Find or create concept from sensory features */
    uint64_t concept_id = find_or_create_concept(gl,
        event->sensory_features, event->feature_dim,
        event->word, CONCEPT_OBJECT);

    if (concept_id == 0) return -1;

    /* Hebbian binding: word <-> concept, weighted by attention */
    float bind_strength = event->attention * gl->hebbian_lr;

    /* Emotional modulation: emotionally salient events create stronger bindings */
    float emotional_boost = 1.0f + fabsf(event->emotional_valence) * event->emotional_arousal;
    bind_strength *= emotional_boost;

    /* Cortex modulation: when the in-modality cortex is currently active
     * (e.g. visual_cortex just processed an image while we're grounding
     * a visual word), boost binding strength up to +30%. Cheap scalar
     * read; no-op when cortexes aren't connected. */
    extern float gl_cortex_modulation_for_modality(grounded_language_t*,
                                                    gl_modality_t, float);
    bind_strength *= gl_cortex_modulation_for_modality(gl, event->modality, 0.3f);

    int rc = lexicon_bind(gl, entry, concept_id, bind_strength, event->modality);

    /* Update emotional grounding */
    float lr = gl->hebbian_lr;
    entry->valence += lr * (event->emotional_valence - entry->valence);
    entry->arousal += lr * (event->emotional_arousal - entry->arousal);
    entry->frequency++;

    /* Learn from context sentence if provided */
    if (event->context_sentence) {
        grounded_language_learn_from_text(gl, event->context_sentence);
    }

    gl->stats.total_groundings++;

    /* Memory-bridge fan-out: push the event into working memory,
     * episodic replay, and hippocampus if any of them are connected.
     * No-op when the brain init didn't wire those subsystems. */
    extern void gl_dispatch_event_to_memory(grounded_language_t*,
                                             const gl_grounding_event_t*,
                                             uint64_t);
    gl_dispatch_event_to_memory(gl, event, concept_id);

    /* Fire GROUNDED event on the cognitive bus. */
    gl_event_t bus_ev = {0};
    bus_ev.type        = GL_EVENT_GROUNDED;
    bus_ev.word        = event->word;
    bus_ev.concept_id  = concept_id;
    bus_ev.valence     = event->emotional_valence;
    bus_ev.arousal     = event->emotional_arousal;
    bus_ev.confidence  = event->attention;
    bus_ev.semantic_vec = event->sensory_features;
    gl_fire_event(gl, &bus_ev);

    return rc;
}

int grounded_language_learn_from_text(grounded_language_t* gl, const char* text) {
    if (!gl || !text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_learn_from_text: NULL parameter (gl=%p, text=%p)",
            (void*)gl, (void*)text);
        return -1;
    }

    uint64_t t_start = gl_profile_ns();
    uint64_t mc_before = __atomic_load_n(&g_gl_mirror_call_count, __ATOMIC_RELAXED);
    uint64_t mn_before = __atomic_load_n(&g_gl_mirror_total_ns,  __ATOMIC_RELAXED);

    size_t text_len = strlen(text);
    char* buf = (char*)nimcp_malloc(text_len + 1);
    if (!buf) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "grounded_language_learn_from_text: failed to allocate buffer");
        return -1;
    }
    memcpy(buf, text, text_len + 1);

    char* words[GL_MAX_PRODUCTION_WORDS];
    uint32_t word_count = tokenize_text(buf, words, GL_MAX_PRODUCTION_WORDS);

    int updates = 0;

    /* Distributional learning: co-occurring words update each other's context vectors */
    for (uint32_t i = 0; i < word_count; i++) {
        gl_lexicon_entry_t* center = lexicon_find_or_create(gl, words[i]);
        if (!center) continue;

        center->frequency++;

        /* Update context vector from surrounding words */
        uint32_t window_start = (i > GL_CONTEXT_WINDOW) ? (i - GL_CONTEXT_WINDOW) : 0;
        uint32_t window_end = (i + GL_CONTEXT_WINDOW < word_count) ?
                               (i + GL_CONTEXT_WINDOW) : word_count;

        for (uint32_t j = window_start; j < window_end; j++) {
            if (j == i) continue;

            const gl_lexicon_entry_t* neighbor = lexicon_find(gl, words[j]);
            if (!neighbor || !neighbor->context_initialized) continue;

            /* Blend neighbor's context into center's context (ATTRACTION) */
            float dist_weight = 1.0f / (float)(abs((int)j - (int)i));

            /* === FREQUENCY SUBSAMPLING — anti-collapse fix 2026-05-22 ===
             * Root cause of the recurring distributional collapse: every
             * sentence contains the same high-frequency function words
             * ("what/is/a/the"), so the pure-attraction update drags every
             * content word's context_vector toward those few words. Over
             * training all vectors converge -> comprehend returns near-
             * identical vectors -> produce emits one word. Probing confirmed
             * it: prompts sharing a "What is" prefix re-converged (cos~0.75)
             * while prompts with distinct function words stayed separate.
             *
             * word2vec solves exactly this by subsampling frequent words.
             * We down-weight a neighbor's attraction by sqrt(T/freq) once it
             * exceeds threshold T: content words (freq<=T) keep full pull,
             * function words (freq>>T) are progressively suppressed. This
             * removes the collapse SOURCE rather than fighting it after the
             * fact. Combined with the negative sampling below. */
            const float FREQ_SUBSAMPLE_T = 100.0f;
            float freq_factor = 1.0f;
            if ((float)neighbor->frequency > FREQ_SUBSAMPLE_T) {
                freq_factor = sqrtf(FREQ_SUBSAMPLE_T / (float)neighbor->frequency);
            }

            float lr = gl->hebbian_lr * 0.1f * dist_weight * freq_factor;

            for (uint32_t d = 0; d < gl->semantic_dim; d++) {
                center->context_vector[d] += lr * neighbor->context_vector[d];
            }
        }

        /* === NEGATIVE SAMPLING (REPULSION) — anti-collapse fix 2026-05-22 ===
         * The attractive blend above, on its own, is a pure averaging
         * process: every word co-occurs with common function words, so
         * all context_vectors drift toward the global mean and collapse
         * (cos≈1 across distinct prompts -> single-word produce). This is
         * the same failure word2vec/skip-gram avoids with negative
         * sampling. For each center word we sample K random lexicon
         * entries (its non-context) and push the center AWAY from them.
         * Attraction + repulsion has a non-degenerate fixed point, so the
         * distributional channel stays spread out without periodic resets.
         *
         * Repulsion lr is a fraction of attraction lr (asymmetric, as in
         * SGNS where positive >> per-negative). Random samples that happen
         * to be true neighbors are rare at K=5 over a 32K vocab and wash
         * out across updates. */
        if (center->context_initialized && gl->vocab_count > 16) {
            const int K_NEG = 5;
            float neg_lr = gl->hebbian_lr * 0.1f * 0.25f;
            for (int k = 0; k < K_NEG; k++) {
                uint32_t ridx = (uint32_t)(gl_random(gl) * (float)gl->vocab_count);
                if (ridx >= gl->vocab_count) ridx = gl->vocab_count - 1;
                const gl_lexicon_entry_t* neg = gl->vocab_list[ridx];
                if (!neg || neg == center || !neg->context_initialized ||
                    !neg->context_vector) {
                    continue;
                }
                for (uint32_t d = 0; d < gl->semantic_dim; d++) {
                    center->context_vector[d] -= neg_lr * neg->context_vector[d];
                }
            }
            /* Bound magnitude so the additive dynamics can't explode and so
             * cosine comparisons in produce stay well-conditioned. */
            normalize_vector(center->context_vector, gl->semantic_dim);
        }

        /* Initialize context if first time in context */
        if (!center->context_initialized && word_count > 2) {
            /* Random init biased by position features */
            for (uint32_t d = 0; d < gl->semantic_dim; d++) {
                center->context_vector[d] = (gl_random(gl) - 0.5f) * 0.1f;
            }
            /* X3 (walkthrough fix): publish context_initialized=true ONLY after
             * the vector is fully written, with a release fence, so a lock-free
             * reader in produce that observes the flag set also sees the vector
             * (no half-written vector behind a true flag). The residual read of
             * the float vector itself is an accepted benign race — the buffer is
             * fixed-size and never freed/realloced while initialized, so a torn
             * read at worst yields a marginally-off cosine that self-corrects on
             * the next learning step (no UAF). Adding per-element atomics to the
             * hot scoring loop would be disproportionate. */
            __atomic_thread_fence(__ATOMIC_RELEASE);
            center->context_initialized = true;
        }

        /* Simple word class inference from position */
        if (center->learned_class == GL_CLASS_UNKNOWN && center->class_confidence < 0.5f) {
            /* Heuristic: first content word often noun, after noun often verb */
            if (i == 0 || (i > 0 && lexicon_find(gl, words[i-1]) &&
                ((const gl_lexicon_entry_t*)lexicon_find(gl, words[i-1]))->learned_class == GL_CLASS_FUNCTION)) {
                center->learned_class = GL_CLASS_NOUN;
                center->class_confidence = 0.3f;
            } else if (i > 0 && lexicon_find(gl, words[i-1]) &&
                       ((const gl_lexicon_entry_t*)lexicon_find(gl, words[i-1]))->learned_class == GL_CLASS_NOUN) {
                center->learned_class = GL_CLASS_VERB;
                center->class_confidence = 0.3f;
            }
        }

        updates++;
    }

    uint64_t t_after_distrib = gl_profile_ns();

    /* Cross-bind words in same sentence to each other's concepts (distributional grounding) */
    uint64_t crossbind_iterations = 0;
    for (uint32_t i = 0; i < word_count; i++) {
        const gl_lexicon_entry_t* entry_i = lexicon_find(gl, words[i]);
        if (!entry_i) continue;
        /* Skip closed-class words as a SOURCE — their accumulated bindings co-occur
         * with everything, so propagating them to content words smears noise and
         * causes B to grow without bound (the 2026-05-15 wedge). Receive-side
         * guard below already blocks closed-class targets; this mirrors that on
         * the source side, matching the pattern at lines ~3930/3951. */
        if (entry_i->learned_class == GL_CLASS_FUNCTION ||
            entry_i->learned_class == GL_CLASS_PRONOUN) continue;

        for (uint32_t b = 0; b < entry_i->binding_count; b++) {
            uint64_t concept = entry_i->bindings[b].concept_id;
            float strength = entry_i->bindings[b].strength;

            /* Weakly bind co-occurring words to this concept */
            for (uint32_t j = 0; j < word_count; j++) {
                if (j == i) continue;
                gl_lexicon_entry_t* entry_j = lexicon_find_or_create(gl, words[j]);
                if (!entry_j) continue;
                if (entry_j->learned_class == GL_CLASS_FUNCTION) continue; /* Skip function words */

                lexicon_bind(gl, entry_j, concept, strength * 0.05f, GL_MODALITY_LINGUISTIC);
                crossbind_iterations++;
            }
        }
    }

    uint64_t t_after_crossbind = gl_profile_ns();

    /* #9 Compositional templates — track frequent bigrams + trigrams. */
    _gl_track_phrases(gl, words, word_count);

    uint64_t t_end = gl_profile_ns();
    uint64_t mc_after = __atomic_load_n(&g_gl_mirror_call_count, __ATOMIC_RELAXED);
    uint64_t mn_after = __atomic_load_n(&g_gl_mirror_total_ns,  __ATOMIC_RELAXED);
    /* Direct fprintf — the LOG_INFO macro signature mishandles printf-style
     * format strings when LOG_MODULE is passed as the first arg, so the actual
     * format gets dropped. fprintf+fflush is guaranteed to reach stderr. */
    fprintf(stderr,
        "[gl_profile] N=%u total_ms=%.3f distrib_ms=%.3f crossbind_ms=%.3f "
        "phrases_ms=%.3f crossbind_iters=%llu mirror_calls=%llu mirror_ms=%.3f\n",
        word_count,
        (double)(t_end - t_start) / 1e6,
        (double)(t_after_distrib - t_start) / 1e6,
        (double)(t_after_crossbind - t_after_distrib) / 1e6,
        (double)(t_end - t_after_crossbind) / 1e6,
        (unsigned long long)crossbind_iterations,
        (unsigned long long)(mc_after - mc_before),
        (double)(mn_after - mn_before) / 1e6);
    fflush(stderr);

    nimcp_free(buf);
    return updates;
}

uint64_t grounded_language_fast_map(grounded_language_t* gl, const char* word,
                                     const float* concept_features, uint32_t feature_dim,
                                     uint32_t category) {
    if (!gl || !word || !concept_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_fast_map: NULL parameter");
        return 0;
    }

    gl_lexicon_entry_t* entry = lexicon_find_or_create(gl, word);
    if (!entry) return 0;

    uint64_t concept_id = find_or_create_concept(gl, concept_features, feature_dim,
                                                  word, category);
    if (concept_id == 0) return 0;

    /* Strong initial binding (fast mapping) */
    lexicon_bind(gl, entry, concept_id, GL_FAST_MAP_THRESHOLD, GL_MODALITY_LINGUISTIC);

    /* Copy features to context vector for distributional similarity */
    uint32_t copy_dim = (feature_dim < gl->semantic_dim) ? feature_dim : gl->semantic_dim;
    memcpy(entry->context_vector, concept_features, copy_dim * sizeof(float));
    /* X3: publish the flag after the vector write (release fence) — see the
     * note at the learn_from_text init site. */
    __atomic_thread_fence(__ATOMIC_RELEASE);
    entry->context_initialized = true;

    LOG_DEBUG(LOG_MODULE, "Fast-mapped '%s' -> concept %lu", word, (unsigned long)concept_id);

    return concept_id;
}

/*=============================================================================
 * Production
 *===========================================================================*/

/* Append one space-separated token to the clause buffer. Content words count
 * toward emit + score_sum and the max_content budget; scaffolding (the/is)
 * counts toward fn only. Returns false (caller stops) when the content budget
 * is exhausted or the token wouldn't fit. */
static bool gl_clause_emit_tok(char* buf, size_t buf_sz, size_t* pos,
                               const char* w, float score, bool content,
                               uint32_t* emit, uint32_t* fn, float* ssum,
                               uint32_t max_content) {
    if (content && *emit >= max_content) return false;
    size_t wl = strlen(w);
    if (*pos + wl + 2 >= buf_sz) return false;
    if (*pos > 0) buf[(*pos)++] = ' ';
    memcpy(buf + *pos, w, wl); *pos += wl;
    if (content) { (*emit)++; *ssum += score; }
    else         { (*fn)++; }
    return true;
}

/* FND-2b: closed-set animacy prior. Animate referents (people, family/role
 * nouns, common animals) are NOUNS that morphology can't suffix-classify
 * ("woman"/"dog" have no -tion/-ment ending) — so this both (a) lets them be
 * recognized as noun candidates and (b) drives the animate-agent preference.
 * A learned animacy feature would be the fuller solution; this closed prior
 * mirrors the codebase's other closed-class tables (function words, PERSON
 * guard). Match is on the lowercased lexicon form. */
static bool gl_is_animate(const char* w) {
    static const char* ANIMATE[] = {
        "man","woman","men","women","person","people","child","children",
        "boy","girl","boys","girls","baby","babies","mother","father","mom",
        "dad","parent","parents","king","queen","prince","princess","lady",
        "guy","teacher","student","doctor","nurse","friend","friends","sister",
        "brother","son","daughter","family","human","creature","dog","cat",
        "bird","fish","horse","cow","pig","sheep","lion","tiger","bear",
        "mouse","rabbit","wolf","fox",NULL };
    if (!w || !w[0]) return false;
    for (int i = 0; ANIMATE[i]; i++) if (strcmp(w, ANIMATE[i]) == 0) return true;
    return false;
}

/* FND-1/FND-2/FND-2b (2026-05-27): predicate->argument clause frame. Organize
 * the grounded top-K into a real clause instead of a bigram/POS-reranked bag.
 *   - head VERB  = highest grounded-score verb candidate (the predicate)
 *   - SUBJECT    = highest grounded-score noun (agent); an animate noun is
 *                  preferred for this role over an inanimate one (FND-2b)
 *   - OBJECT     = next-highest noun (patient), if any
 *   - ADJECTIVE  = highest grounded-score adjective (modifier or predicate)
 * A word in the animate set counts as a NOUN candidate even when morphology
 * returns UNKNOWN, so animate-referent intents can form clause subjects.
 * Two clause shapes:
 *   SVO    (a head verb clears the floor): "the [ADJ] SUBJECT VERB [the OBJECT]"
 *          — the adjective, when present + above floor, modifies the subject NP.
 *   COPULA (no head verb, but a noun + an above-floor adjective): "the SUBJECT
 *          is ADJECTIVE" — covers descriptive/stative intents that would
 *          otherwise fall back to a bag. (FND-2)
 * Determiners + the copula "is" are F2-style scaffolding; verb inflection +
 * a/an are left to the F3/F4 surface correctors downstream. Only the word ORDER
 * is templated (English SVO/copula, a Broca-level parameter) — every slot is
 * filled by the same grounded score the greedy path uses, so WHAT is said stays
 * fully data-driven.
 *
 * Writes buf/*pos_io/*fn_io/*score_io and returns the content-word count (>=1)
 * on success, or -1 to fall back to greedy: no subject, or neither a
 * floor-clearing head verb NOR a floor-clearing predicate adjective (we don't
 * force a clause on weak signal). Subject is the word-0 analogue (always kept);
 * object + modifier/predicate adjective are floor-gated like greedy's
 * subsequent words. `max_words` caps content words. Class is from gl_f4_class
 * (learned class when confident, else morphology). */
static int gl_build_clause_frame(grounded_language_t* gl,
                                 const char** topk_words,
                                 const float* topk_scores,
                                 uint32_t n, float floor, uint32_t max_words,
                                 char* buf, size_t buf_sz,
                                 size_t* pos_io, uint32_t* fn_io, float* score_io) {
    if (!gl || !topk_words || !topk_scores || !buf || buf_sz == 0) return -1;
    int verb_idx = -1, subj_idx = -1, obj_idx = -1, adj_idx = -1;
    float verb_best = -1e30f, subj_best = -1e30f, obj_best = -1e30f, adj_best = -1e30f;
    bool subj_anim = false, obj_anim = false;  /* FND-2b: per-slot animacy */
    for (uint32_t j = 0; j < n; j++) {
        const gl_lexicon_entry_t* e = lexicon_find(gl, topk_words[j]);
        if (e && (e->learned_class == GL_CLASS_FUNCTION ||
                  e->learned_class == GL_CLASS_PRONOUN)) continue;
        gl_word_class_t cls = gl_f4_class(gl, topk_words[j]);
        float s = topk_scores[j];
        /* FND-2b: an animate-set word is a NOUN candidate even when morphology
         * leaves it UNKNOWN ("woman"/"dog" have no nominal suffix). */
        bool anim = gl_is_animate(topk_words[j]);
        bool is_noun = (cls == GL_CLASS_NOUN) || anim;
        if (cls == GL_CLASS_VERB) {
            if (s > verb_best) { verb_best = s; verb_idx = (int)j; }
        } else if (is_noun) {
            /* Track the top-2 nouns by score (subject, then object), carrying
             * each one's animacy so roles can be re-arbitrated below. */
            if (s > subj_best) {
                obj_best = subj_best; obj_idx = subj_idx; obj_anim = subj_anim;
                subj_best = s;        subj_idx = (int)j;  subj_anim = anim;
            } else if (s > obj_best) {
                obj_best = s; obj_idx = (int)j; obj_anim = anim;
            }
        } else if (cls == GL_CLASS_ADJECTIVE) {
            if (s > adj_best) { adj_best = s; adj_idx = (int)j; }
        }
    }
    if (subj_idx < 0) return -1;  /* every clause shape needs a subject head */

    /* FND-2b: animate-agent preference. If the patient slot is animate and the
     * agent slot is not, swap their roles so the animate referent is the
     * subject (animate agents are a cross-linguistic default). Score still
     * decides WHICH two nouns appear; animacy only arbitrates their roles. */
    if (obj_idx >= 0 && obj_anim && !subj_anim) {
        int   ti = subj_idx;  subj_idx  = obj_idx;  obj_idx  = ti;
        float tb = subj_best; subj_best = obj_best; obj_best = tb;
        bool  ta = subj_anim; subj_anim = obj_anim; obj_anim = ta;
    }
    (void)subj_anim;  /* consumed only by the swap above */

    bool have_verb = (verb_idx >= 0 && verb_best >= floor);
    bool have_adj  = (adj_idx  >= 0 && adj_best  >= floor);
    if (!have_verb && !have_adj) return -1;  /* no predicate at all → greedy */

    /* Floor-gate the OBJECT like the greedy path floor-gates subsequent words
     * (greedy always emits word 0 but stops once a later word drops below the
     * floor). Subject = the obligatory head (word-0 analogue, always kept).
     * (Walkthrough LOW-1.) */
    if (obj_idx >= 0 && obj_best < floor) obj_idx = -1;

    size_t pos = 0; uint32_t emit = 0, fn = 0; float ssum = 0.0f;
    bool ok = true;
    if (have_verb) {
        /* SVO: "the [ADJ] SUBJECT VERB [the OBJECT]". The adjective (when above
         * floor) is a pre-nominal modifier of the subject NP. */
        ok = gl_clause_emit_tok(buf, buf_sz, &pos, "the", 0.0f, false,
                                &emit, &fn, &ssum, max_words);
        if (ok && have_adj)
            ok = gl_clause_emit_tok(buf, buf_sz, &pos, topk_words[adj_idx],
                                    adj_best, true, &emit, &fn, &ssum, max_words);
        if (ok)
            ok = gl_clause_emit_tok(buf, buf_sz, &pos, topk_words[subj_idx],
                                    subj_best, true, &emit, &fn, &ssum, max_words);
        if (ok)
            ok = gl_clause_emit_tok(buf, buf_sz, &pos, topk_words[verb_idx],
                                    verb_best, true, &emit, &fn, &ssum, max_words);
        if (ok && obj_idx >= 0) {
            gl_clause_emit_tok(buf, buf_sz, &pos, "the", 0.0f, false,
                               &emit, &fn, &ssum, max_words);
            gl_clause_emit_tok(buf, buf_sz, &pos, topk_words[obj_idx],
                               obj_best, true, &emit, &fn, &ssum, max_words);
        }
    } else {
        /* COPULA: "the SUBJECT is ADJECTIVE" (have_adj is guaranteed here). */
        gl_clause_emit_tok(buf, buf_sz, &pos, "the", 0.0f, false,
                           &emit, &fn, &ssum, max_words);
        gl_clause_emit_tok(buf, buf_sz, &pos, topk_words[subj_idx],
                           subj_best, true, &emit, &fn, &ssum, max_words);
        gl_clause_emit_tok(buf, buf_sz, &pos, "is", 0.0f, false,
                           &emit, &fn, &ssum, max_words);
        gl_clause_emit_tok(buf, buf_sz, &pos, topk_words[adj_idx],
                           adj_best, true, &emit, &fn, &ssum, max_words);
    }
    if (emit == 0) return -1;
    buf[pos] = '\0';
    *pos_io = pos; *fn_io = fn; *score_io = ssum;
    return (int)emit;
}

/* Increment-1 (2026-06-02): SNN-derived produce candidates. Pulls top-K word
 * activations from the SNN bridge's per-tick Broca spike cache into the produce
 * top-K buffers (word_form borrowed from the bridge's registered word table,
 * which == lexicon forms; valid for this produce call). Returns the candidate
 * count, or 0 when the SNN has no signal (caller falls back to the lexicon
 * producer). Opt-in only — reached when gl->produce_via_snn is ON. */
static uint32_t gl_produce_candidates_via_snn(grounded_language_t* gl,
                                              const float* intent, uint32_t intent_dim,
                                              const uint64_t* seed_concepts,
                                              const float* seed_acts,
                                              uint32_t seed_count,
                                              const char** out_words, float* out_scores,
                                              uint32_t max_words) {
    if (!gl || !gl->snn_bridge || !intent || !out_words || !out_scores || max_words == 0)
        return 0;
    if (max_words > 32) max_words = 32;

    /* (1) Map the cascade's content_intent → the concepts to SEED into Wernicke.
     * Because content_intent already aggregates the non-FNO networks (ANN/LNN/
     * CNN) AND every cognitive module (emotion/ToM/introspection/world-model/
     * reasoning/imagination/WM via the cascade), seeding generation from it makes
     * all of them drive the SNN's word production — the directive's convergence
     * point. */
    uint64_t concept_ids[8];
    /* Seed only the strongest gl->snn_gen_max_concepts (default 2) — seeding all
     * 8 floods Broca with 8 concepts' words and the decode degrades to salad,
     * even though single-concept selectivity is excellent. Tunable live. */
    uint32_t seed_n = gl->snn_gen_max_concepts ? gl->snn_gen_max_concepts : 2u;
    if (seed_n > 8u) seed_n = 8u;
    uint32_t n_concepts = 0;

    /* Phase 3 intent grounding (2026-06-14): seed the generator with the
     * concepts COMPREHENSION actually resolved from the prompt
     * (comp.activated_concepts), NOT with concepts re-derived by cosine-ranking
     * word distributional vectors. probe_pod_cosines shows those distributional
     * embeddings are structureless noise (within≈between≈0 separation), so
     * top_concepts_for_intent ranks them near-randomly and a fixed set of rare
     * words ("thoriated"/"chalcedon") wins every prompt regardless of meaning;
     * the now-clean isolated readout then faithfully speaks that garbage.
     * Seeding the grounded resolved concepts puts the RIGHT concept into
     * Wernicke. Env gate NIMCP_LANG_SEED_FROM_COMPREHEND=0 restores the legacy
     * cosine path. */
    static int seed_from_comp = -1;
    if (seed_from_comp < 0) {
        const char* e = getenv("NIMCP_LANG_SEED_FROM_COMPREHEND");
        seed_from_comp = (e && e[0] == '0') ? 0 : 1;
    }
    if (seed_from_comp && seed_concepts && seed_count > 0) {
        /* Pick the strongest seed_n distinct concepts by activation (descending).
         * activation_levels carry polarity*effective from comprehend; the
         * highest-activation entries are the content concepts (function words
         * score low and/or are guarded upstream). */
        for (uint32_t pick = 0; pick < seed_n; pick++) {
            int   best = -1;
            float best_act = -1e30f;
            for (uint32_t s = 0; s < seed_count; s++) {
                uint64_t cid = seed_concepts[s];
                if (cid == 0) continue;
                int dup = 0;
                for (uint32_t k = 0; k < n_concepts; k++)
                    if (concept_ids[k] == cid) { dup = 1; break; }
                if (dup) continue;
                float a = seed_acts ? seed_acts[s] : 1.0f;
                if (a > best_act) { best_act = a; best = (int)s; }
            }
            if (best < 0) break;
            concept_ids[n_concepts++] = seed_concepts[best];
        }
    }
    if (n_concepts == 0) {
        /* No grounded seed available (or gate off) → legacy cosine ranking of
         * the intent vector over the word distributional space. */
        n_concepts = grounded_language_top_concepts_for_intent(
            gl, intent, intent_dim, concept_ids, seed_n);
    }
    if (n_concepts == 0) return 0;   /* no grounded concept → lexicon fallback */

    /* (2) Seed → step → decode via the bridge's stored network handles
     * (connect_network). This makes the SNN GENERATE from the intent's concepts
     * (concept→word projection drives Broca), vs reading stale ambient cache.
     * n_steps small (full-SNN steps are costly); inject high so the small
     * Wernicke ensemble fires (validated supra-threshold requirement). Returns
     * -1 if the bridge isn't network-connected, or 0 results on a cold/uncovered
     * projection → either way the caller falls back to the lexicon producer. */
    snn_lang_word_result_t res[32];
    uint32_t nres = 0;
    /* n_steps/inject from the selectivity sweep (snn_probe_selectivity): 16/40
     * puts the expected word in the decode top-5 for 94% of bindings (mean rank
     * 1.64) vs 27/64-in-top-32 at the old 8/100. Runtime-tunable so the operating
     * point can be re-swept live without a rebuild. */
    uint32_t gen_steps  = gl->snn_gen_n_steps ? gl->snn_gen_n_steps : 16u;
    float    gen_inject = (gl->snn_gen_inject > 0.0f) ? gl->snn_gen_inject : 40.0f;
    if (snn_language_bridge_generate_and_decode(gl->snn_bridge, concept_ids, n_concepts,
                                                gen_steps, gen_inject,
                                                res, max_words, &nres) != 0) {
        return 0;
    }
    uint32_t out = 0;
    for (uint32_t i = 0; i < nres && out < max_words; i++) {
        if (!res[i].word_form || res[i].word_form[0] == '\0') continue;
        out_words[out]  = res[i].word_form;
        out_scores[out] = res[i].activation;
        out++;
    }
    return out;
}

/* Internal produce with optional grounded seed concepts (Phase 3, 2026-06-14).
 * seed_concepts/seed_acts/seed_count come from the comprehension result so the
 * SNN generator is seeded with the concepts the prompt actually resolved,
 * bypassing the noise-ranked cosine selection. The public
 * grounded_language_produce() wrapper passes NULL/0 (legacy behavior). */
static int gl_produce_impl(grounded_language_t* gl, const float* intent,
                           uint32_t intent_dim, gl_production_mode_t mode,
                           const uint64_t* seed_concepts, const float* seed_acts,
                           uint32_t seed_count, gl_production_result_t* result);

int grounded_language_produce(grounded_language_t* gl, const float* intent,
                               uint32_t intent_dim, gl_production_mode_t mode,
                               gl_production_result_t* result) {
    return gl_produce_impl(gl, intent, intent_dim, mode, NULL, NULL, 0, result);
}

static int gl_produce_impl(grounded_language_t* gl, const float* intent,
                           uint32_t intent_dim, gl_production_mode_t mode,
                           const uint64_t* seed_concepts, const float* seed_acts,
                           uint32_t seed_count, gl_production_result_t* result) {
    (void)mode;
    if (!gl || !intent || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_produce: NULL parameter (gl=%p, intent=%p, result=%p)",
            (void*)gl, (void*)intent, (void*)result);
        return -1;
    }
    memset(result, 0, sizeof(*result));

    /* Option-1 readout (Slice B, Option B — 2026-05-20): decode against
     * grounded_language's own lexicon bindings, NOT the SNN-language
     * bridge. The bridge was stripped to transport-only by the Option-1
     * rebuild (snn_language_bridge_decode_spikes returns 0 unconditionally)
     * and the registry-based readout was never built. find_words_near_vector
     * scores each in-vocab word against the intent via
     *   0.4·cos(intent, entry.context_vector)
     *   + 0.6·max_b(strength_b · cos(intent, get_concept_features(b.concept_id)))
     * and picks top-K (with a deterministic-shuffle diversity fallback when
     * the lexicon is too under-grounded for top-K to discriminate). Function
     * words are dropped from the emission to keep the output content-bearing.
     * The bridge stays as async spike-routing transport but is no longer on
     * the produce path. */

    enum { GL_PRODUCE_TOPK = 32 };
    const char* topk_words[GL_PRODUCE_TOPK];
    float       topk_scores[GL_PRODUCE_TOPK];
    memset(topk_words, 0, sizeof(topk_words));
    memset(topk_scores, 0, sizeof(topk_scores));

    /* Increment-1 (2026-06-02): SNN-as-generator A/B. When produce_via_snn is
     * ON, source candidates from the SNN bridge's Broca spike cache; on no SNN
     * signal (0 candidates) fall through to the lexicon producer (permanent
     * fallback). Default OFF → the lexicon path runs unchanged. `intent` here
     * is the cascade's cognitively-integrated content_intent, so every cognitive
     * module that reaches content_intent drives the SNN readout too. */
    uint32_t n = 0;
    if (gl->produce_via_snn && gl->snn_bridge) {
        n = gl_produce_candidates_via_snn(gl, intent, intent_dim,
                                          seed_concepts, seed_acts, seed_count,
                                          topk_words, topk_scores, GL_PRODUCE_TOPK);
    }
    if (n == 0) {
        n = find_words_near_vector(gl, intent, intent_dim,
                                   GL_CLASS_UNKNOWN /* any class */,
                                   topk_words, topk_scores,
                                   GL_PRODUCE_TOPK);
    }
    if (n == 0) {
        /* No usable candidate — caller's IDK gate emits "I don't know".
         * Honest answer for an undertrained or under-grounded lexicon. */
        return -1;
    }

    /* Length bounds. Historically this hard-coded max_words=5, which
     * silently bottlenecked every stage above 2 (the stage table allows
     * up to 20 at stage 4). The cap is now the top-K capacity itself
     * (GL_PRODUCE_TOPK=32), letting the cascade's stage_motor truncate
     * to the per-stage cap as the single source of length truth. min=1
     * is preserved so callers see a non-degenerate result for
     * single-relevant-word intents. */
    uint32_t min_words = 1, max_words = GL_PRODUCE_TOPK;
    if (max_words > n) max_words = n;
    if (min_words > max_words) min_words = max_words;

    /* Build the emission with bigram-conditioned reranking (Tier 1 Step B,
     * 2026-05-23). Position 0 takes the highest cosine match. Subsequent
     * positions pick the unused candidate that maximizes
     *     topk_scores[j] + ALPHA * log(1 + bigram_freq(prev, cand))
     * where bigram_freq comes from gl->phrases (the gl_phrase_t table that
     * grounded_language_learn_from_text populates from text co-occurrence).
     * The phrase table is small (<=512 entries, linear-scan lookup) so the
     * per-position cost is bounded.
     *
     * ALPHA = 0.05 keeps cosine relevance as the primary signal —
     * adjacent-rank cosine deltas in top-K are typically 0.05-0.10, so the
     * bigram term re-orders within the top-K instead of overriding it.
     * When no phrase entry exists for a (prev, cand) pair, log(1+0)=0 and
     * the candidate is ranked by pure cosine — fully backward-compatible
     * with the pre-rerank behavior.
     *
     * Note: this is NOT autoregressive in the residual-stream sense (the
     * intent vector isn't evolved between positions). It's a within-top-K
     * rerank, which is the cheapest way to add bigram coherence without
     * rebuilding the decoder. Autoregressive intent evolution + larger
     * top-K is the natural Tier 1 follow-up. */
    const float GL_BIGRAM_RERANK_ALPHA = 0.05f;
    /* Developmental confidence floor — per-position stop condition.
     * Stage 0 returns 1.0 so the loop emits exactly 1 word (newborn).
     * Stage 4 returns 0.0 so length is bounded only by top-K / phrase
     * budget (adult-fluent — no developmental cap). Intermediate stages
     * decay smoothly. Combined with the cascade's stage_motor truncation
     * (which is still active), this gives a graceful soft length cap on
     * top of the hard stage cap. As Athena develops the floor approaches
     * 0 and the gradient effectively dissolves. */
    const float confidence_floor = gl_produce_confidence_floor(gl);
    /* POS-transition bias (Tier 1 Step C). Stage-scaled weight; 0 disables
     * the structural nudge entirely (stages 0-1), so the emit loop is
     * byte-for-byte the pre-Step-C path there. */
    const float pos_bias_weight = gl_produce_pos_bias_weight(gl);
    /* Tier 1 follow-up: gl-side autoregressive produce. When enabled, a
     * running sum of emitted words' context_vectors (emitted_ctx) conditions
     * each subsequent pick via a cosine continuation bonus. emitted_ctx stays
     * NULL — and the bonus is skipped — when the flag is off, so production is
     * byte-identical to the within-top-K rerank path. Position 0 is identical
     * even when ON (emitted_ctx is all-zero until the first word is emitted). */
    const float GL_AR_WEIGHT = 0.3f;
    float* emitted_ctx = NULL;
    if (gl->autoregressive_produce && gl->semantic_dim > 0) {
        emitted_ctx = (float*)nimcp_calloc(gl->semantic_dim, sizeof(float));
    }
    /* Tier 2: discourse-seeded AR produce. When produce_discourse_seed is on
     * (and AR is on, and stage >= 2), prime emitted_ctx with a unit-normalized,
     * decayed copy of the most-recent discourse-turn vector instead of all
     * zeros. This pulls the *first* produced words toward referential
     * continuity with what was just said. The seed magnitude (GL_DISCOURSE_
     * SEED_DECAY ~= one half-strength emitted word) means it dominates only
     * the opening positions and then fades automatically as real emitted-word
     * context_vectors accumulate in the running sum. ar_seeded also relaxes
     * the position-0 bonus guard below so the seed influences word 1.
     * Off / non-AR / stage<2 → emitted_ctx stays all-zero → byte-identical. */
    bool ar_seeded = false;
    if (emitted_ctx && gl->produce_discourse_seed && gl->current_stage >= 2) {
        /* X2: copy the prior turn vector under the discourse lock straight into
         * emitted_ctx (no raw ring pointer held across the normalize loop), then
         * unit-normalize × decay in place. */
        uint32_t pdim = grounded_language_copy_recent_turn_vector(
            gl, 1, emitted_ctx, gl->semantic_dim);
        if (pdim == gl->semantic_dim) {
            double nrm = 0.0;
            for (uint32_t d = 0; d < gl->semantic_dim; d++)
                nrm += (double)emitted_ctx[d] * (double)emitted_ctx[d];
            nrm = sqrt(nrm);
            if (nrm > 1e-9) {
                const float GL_DISCOURSE_SEED_DECAY = 0.5f;
                float scale = GL_DISCOURSE_SEED_DECAY / (float)nrm;
                for (uint32_t d = 0; d < gl->semantic_dim; d++)
                    emitted_ctx[d] *= scale;
                ar_seeded = true;
            } else {
                /* zero-norm turn — clear the copy so the seed is a no-op. */
                memset(emitted_ctx, 0, (size_t)gl->semantic_dim * sizeof(float));
            }
        }
    }
    bool used[GL_PRODUCE_TOPK] = {0};
    char prev_word[GL_MAX_WORD_LEN] = {0};
    gl_word_class_t prev_class = GL_CLASS_UNKNOWN;  /* drives the POS bias */
    char    buf[1024] = {0};
    size_t  pos = 0;
    uint32_t emit = 0;          /* CONTENT words — gates min/max + floor */
    uint32_t fn_inserted = 0;   /* F2 scaffolding function words (not budgeted) */
    float    score_sum = 0.0f;

    /* FND-1: try the SVO clause frame first (default OFF, stage>=2). On success
     * it fills buf/pos/emit/fn_inserted/score_sum and we skip the greedy bag
     * emit. On -1 (no head verb / no noun, or head verb below the confidence
     * floor) we fall through to the greedy path — never worse than baseline.
     * The clause frame doesn't use emitted_ctx (AR is a greedy-path strategy);
     * emitted_ctx is still freed below regardless. */
    bool clause_built = false;
    if (gl->produce_clause_frame && gl->current_stage >= 2) {
        int ce = gl_build_clause_frame(gl, topk_words, topk_scores, n,
                                       confidence_floor, max_words,
                                       buf, sizeof(buf), &pos, &fn_inserted,
                                       &score_sum);
        if (ce > 0) { emit = (uint32_t)ce; clause_built = true; }
    }
    while (!clause_built && emit < max_words) {
        int   best_idx   = -1;
        float best_score = -1e30f;
        /* Track the raw cosine of the eventual winner separately from
         * the combined (cos + bigram_bias) score so the developmental
         * floor only gates on cosine relevance — bigram inflation
         * affects ORDER, never STOP. */
        float best_cos   = -1e30f;
        for (uint32_t j = 0; j < n; j++) {
            if (used[j]) continue;
            const gl_lexicon_entry_t* e = lexicon_find(gl, topk_words[j]);
            /* Mirror the find_words_near_vector pronoun+function gate so
             * any pronoun/function word that slipped through (e.g. a
             * caller passed required_class explicitly) doesn't reach
             * the emission buffer. */
            if (e && (e->learned_class == GL_CLASS_FUNCTION ||
                      e->learned_class == GL_CLASS_PRONOUN)) continue;
            float s = topk_scores[j];
            if (emit > 0 && prev_word[0]) {
                char key[GL_MAX_PHRASE_LEN];
                int kn = snprintf(key, sizeof(key), "%s %s",
                                  prev_word, topk_words[j]);
                if (kn > 0 && (size_t)kn < sizeof(key)) {
                    const gl_phrase_t* ph =
                        grounded_language_lookup_phrase(gl, key);
                    if (ph && ph->frequency > 0) {
                        s += GL_BIGRAM_RERANK_ALPHA *
                             logf(1.0f + (float)ph->frequency);
                    }
                }
            }
            /* POS-transition bias (Tier 1 Step C): nudge toward the
             * content-word class that grammatically follows prev_class,
             * weighted by the candidate's class_confidence so trustworthy
             * (seeded) tags count more than heuristic guesses. Soft +
             * additive — UNKNOWN tags or a 0 weight apply nothing. */
            if (pos_bias_weight > 0.0f && e &&
                e->learned_class != GL_CLASS_UNKNOWN) {
                gl_word_class_t want = gl_pos_expected_next(prev_class);
                if (want != GL_CLASS_UNKNOWN && e->learned_class == want) {
                    s += pos_bias_weight * e->class_confidence;
                }
            }
            /* Autoregressive continuation bonus (Tier 1 follow-up): reward
             * candidates similar to what's been said so far. No-op at
             * position 0 (emitted_ctx all-zero) and when the flag is off
             * (emitted_ctx NULL) — affects ORDER only, never the stop floor
             * (which still gates on best_cos = raw prompt cosine). */
            if (emitted_ctx && (emit > 0 || ar_seeded) && e && e->context_vector &&
                e->context_initialized) {
                s += GL_AR_WEIGHT * cosine_similarity(emitted_ctx,
                                                      e->context_vector,
                                                      gl->semantic_dim);
            }
            if (s > best_score) {
                best_score = s;
                best_cos   = topk_scores[j];
                best_idx   = (int)j;
            }
        }
        if (best_idx < 0) break;
        /* Developmental floor: stop emitting if the chosen candidate's
         * raw cosine drops below the floor. Always emit at least
         * 1 word (the floor is a stop signal, not a gate on position 0)
         * — callers that want zero output should not invoke produce. */
        if (emit > 0 && best_cos < confidence_floor) break;
        used[best_idx] = true;
        const char* w = topk_words[best_idx];

        /* F2 (Tier 1 Step F): function-word scaffolding. The candidate pool
         * excludes function words, so produce emits bare content ("cat sit
         * mat"). Here we insert a determiner/copula at grammatically
         * appropriate slots so output reads as phrases ("the cat ... the
         * mat"). Stage-gated by the same weight as the POS bias (off at
         * stages 0-1 → byte-identical babble there). Inserted words do NOT
         * count toward `emit`, so they never eat the content-word budget.
         *
         *  - "the" before a noun-phrase head (NOUN or ADJECTIVE) that starts
         *    the utterance or follows a verb (subject / object position).
         *  - "is" before a predicate ADJECTIVE following a NOUN
         *    ("cat is happy"). */
        const char* fn_word = NULL;
        if (pos_bias_weight > 0.0f) {
            gl_word_class_t this_class = GL_CLASS_UNKNOWN;
            const gl_lexicon_entry_t* ce = lexicon_find(gl, w);
            if (ce) this_class = ce->learned_class;
            bool np_start = (emit == 0 || prev_class == GL_CLASS_VERB);
            if ((this_class == GL_CLASS_NOUN ||
                 this_class == GL_CLASS_ADJECTIVE) && np_start) {
                fn_word = "the";
            } else if (this_class == GL_CLASS_ADJECTIVE &&
                       prev_class == GL_CLASS_NOUN) {
                fn_word = "is";
            }
        }

        size_t flen  = strlen(w);
        size_t fnlen = fn_word ? strlen(fn_word) : 0;
        /* Need room for: [space] fn_word [space] word + NUL. */
        if (pos + fnlen + flen + 3 >= sizeof(buf)) break;

        if (fn_word) {
            if (pos > 0) buf[pos++] = ' ';
            memcpy(buf + pos, fn_word, fnlen);
            pos += fnlen;
            fn_inserted++;
        }
        if (pos > 0) buf[pos++] = ' ';
        memcpy(buf + pos, w, flen);
        pos += flen;
        score_sum += topk_scores[best_idx];
        emit++;
        size_t cl = (flen < sizeof(prev_word) - 1)
                      ? flen : sizeof(prev_word) - 1;
        memcpy(prev_word, w, cl);
        prev_word[cl] = '\0';
        /* Carry the emitted word's class forward so the next position's
         * POS bias keys off it (NOUN -> prefer VERB, etc.). */
        {
            const gl_lexicon_entry_t* be = lexicon_find(gl, w);
            prev_class = be ? be->learned_class : GL_CLASS_UNKNOWN;
        }
        /* Accumulate the emitted word's context into the running AR state so
         * the next position is conditioned on it. */
        if (emitted_ctx) {
            const gl_lexicon_entry_t* we = lexicon_find(gl, w);
            if (we && we->context_vector && we->context_initialized) {
                for (uint32_t d = 0; d < gl->semantic_dim; d++)
                    emitted_ctx[d] += we->context_vector[d];
            }
        }
    }
    if (emitted_ctx) { nimcp_free(emitted_ctx); emitted_ctx = NULL; }
    if (emit == 0 || emit < min_words) return -1;

    result->text = (char*)nimcp_malloc(pos + 1);
    if (!result->text) return -1;
    memcpy(result->text, buf, pos);
    result->text[pos] = '\0';

    /* word_count reports the TOTAL surface tokens (content + F2 scaffolding)
     * so downstream length/telemetry matches the rendered text; relevance
     * stays a per-CONTENT-word mean (scaffolding has no intent score). */
    result->word_count = emit + fn_inserted;
    result->fluency    = topk_scores[0];
    result->relevance  = (emit > 0) ? (score_sum / (float)emit) : 0.0f;
    result->creativity = 0.5f;  /* placeholder — refine when sampling lands */

    result->semantic_vector = (float*)nimcp_calloc(gl->semantic_dim, sizeof(float));
    if (result->semantic_vector) {
        uint32_t copy_dim = (intent_dim < gl->semantic_dim)
            ? intent_dim : gl->semantic_dim;
        memcpy(result->semantic_vector, intent, copy_dim * sizeof(float));
    }

    gl->stats.total_productions++;

    /* Fire PRODUCED event on the cognitive bus. */
    gl_event_t bus_ev = {0};
    bus_ev.type         = GL_EVENT_PRODUCED;
    bus_ev.text         = result->text;
    bus_ev.semantic_vec = result->semantic_vector;
    bus_ev.confidence   = result->fluency;
    gl_fire_event(gl, &bus_ev);

    return 0;
}

int grounded_language_describe_concept(grounded_language_t* gl, uint64_t concept_id,
                                        gl_production_result_t* result) {
    if (!gl || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_describe_concept: NULL parameter");
        return -1;
    }

    const float* features = get_concept_features(gl, concept_id);
    if (!features) return -1;

    return grounded_language_produce(gl, features, gl->semantic_dim,
                                      GL_PRODUCE_DESCRIBE, result);
}

/* Comparator for descending-strength sort in grounded_language_prune_bindings. */
static int gl_binding_strength_desc_cmp(const void* a, const void* b) {
    float sa = ((const gl_word_binding_t*)a)->strength;
    float sb = ((const gl_word_binding_t*)b)->strength;
    if (sa > sb) return -1;
    if (sa < sb) return 1;
    return 0;
}

/* One-shot reclaim: for each lexicon entry, keep the top max_bindings_per_word
 * bindings by strength and drop the rest. Intended as a single-call sweep at
 * boot or via maintenance RPC after the 2026-05-15 wedge fix, to reclaim B
 * accumulated under the prior leak. Returns total bindings dropped. */
uint64_t grounded_language_prune_bindings(grounded_language_t* gl,
                                          uint32_t max_bindings_per_word) {
    if (!gl || !gl->lexicon || max_bindings_per_word == 0) return 0;

    uint64_t dropped_total = 0;
    uint32_t entries_pruned = 0;

    for (uint32_t slot = 0; slot < gl->lexicon_size; slot++) {
        gl_lexicon_entry_t* e = gl->lexicon[slot];
        if (!e || !e->bindings) continue;
        if (e->binding_count <= max_bindings_per_word) continue;

        qsort(e->bindings, e->binding_count, sizeof(gl_word_binding_t),
              gl_binding_strength_desc_cmp);
        uint32_t before = e->binding_count;
        e->binding_count = max_bindings_per_word;
        dropped_total += (uint64_t)(before - max_bindings_per_word);
        entries_pruned++;
    }

    fprintf(stderr,
        "[gl_prune] max_per_word=%u entries_pruned=%u total_dropped=%llu\n",
        max_bindings_per_word, entries_pruned,
        (unsigned long long)dropped_total);
    fflush(stderr);
    return dropped_total;
}

int64_t grounded_language_reset_lexicon_distributional(grounded_language_t* gl,
                                                        bool zero_and_mark_uninit,
                                                        float jitter)
{
    if (!gl || !gl->lexicon) return -1;
    if (!isfinite(jitter) || jitter < 0.0f) return -1;

    int64_t touched = 0;
    uint32_t dim = gl->semantic_dim;
    /* Simple xorshift seeded from gl pointer + clock — deterministic per
     * call site, no need for crypto-grade randomness here. */
    uint64_t rs = ((uint64_t)(uintptr_t)gl) ^ 0x9E3779B97F4A7C15ULL;
    rs ^= (uint64_t)time(NULL);
    if (rs == 0) rs = 1;

    for (uint32_t slot = 0; slot < gl->lexicon_size; slot++) {
        gl_lexicon_entry_t* e = gl->lexicon[slot];
        if (!e) continue;
        if (!e->context_vector || dim == 0) {
            /* Still count as touched — the entry now has the desired
             * "uninitialized" state by virtue of having no vector. */
            e->context_initialized = false;
            touched++;
            continue;
        }
        if (zero_and_mark_uninit) {
            memset(e->context_vector, 0, dim * sizeof(float));
            e->context_initialized = false;
        } else {
            /* Re-randomize in [-jitter, +jitter]. */
            for (uint32_t d = 0; d < dim; d++) {
                rs ^= rs >> 12; rs ^= rs << 25; rs ^= rs >> 27;
                uint64_t r = rs * 0x2545F4914F6CDD1DULL;
                /* 24-bit mantissa -> uniform [0,1) */
                float u = (float)((uint32_t)(r >> 40)) * (1.0f / 16777216.0f);
                e->context_vector[d] = (u * 2.0f - 1.0f) * jitter;
            }
            /* X3: publish after the vector write (release fence). */
            __atomic_thread_fence(__ATOMIC_RELEASE);
            e->context_initialized = true;
        }
        touched++;
    }

    fprintf(stderr,
        "[gl_distrib_reset] mode=%s jitter=%g lexicon_entries_touched=%lld\n",
        zero_and_mark_uninit ? "zero" : "jitter",
        (double)jitter, (long long)touched);
    fflush(stderr);
    return touched;
}

int64_t grounded_language_reset_concept_grounding(grounded_language_t* gl)
{
    if (!gl) return -1;

    /* Clear every lexicon entry's concept bindings (keep the word forms,
     * the bindings[] capacity, and the distributional context_vector). New
     * grounding will refill bindings[] from index 0. This drops the stale
     * collapsed bindings that point every word at the one degenerate
     * concept the prefixed-feature grounding created. */
    int64_t cleared_bindings = 0;
    int64_t words = 0;
    if (gl->lexicon) {
        for (uint32_t slot = 0; slot < gl->lexicon_size; slot++) {
            gl_lexicon_entry_t* e = gl->lexicon[slot];
            if (!e) continue;
            cleared_bindings += e->binding_count;
            e->binding_count = 0;
            words++;
        }
    }

    /* Wipe the brain's concept store. It was at/near capacity, so
     * find_or_create_concept could no longer create fresh concepts and
     * every clean re-grounding would match a stale concept instead. Reset
     * frees capacity so re-grounding builds a diverse store from scratch. */
    if (gl->semantic_memory) {
        semantic_memory_reset(gl->semantic_memory);
    }

    /* Reflect the cleared state in the binding/grounding counters so
     * lang_status doesn't keep reporting the pre-reset totals. */
    gl->stats.total_bindings = 0;
    gl->stats.total_groundings = 0;

    fprintf(stderr,
        "[gl_reground_reset] words=%lld bindings_cleared=%lld "
        "semantic_memory_reset=%d\n",
        (long long)words, (long long)cleared_bindings,
        gl->semantic_memory ? 1 : 0);
    fflush(stderr);
    return cleared_bindings;
}

int grounded_language_blend(grounded_language_t* gl,
                             uint64_t concept_a, uint64_t concept_b,
                             const float* vector_a, const float* vector_b,
                             uint32_t vec_dim, float blend_ratio,
                             gl_production_result_t* result) {
    if (!gl || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_blend: NULL parameter");
        return -1;
    }

    uint32_t dim = gl->semantic_dim;

    /* Get feature vectors */
    const float* fa = vector_a;
    const float* fb = vector_b;

    if (concept_a != 0) fa = get_concept_features(gl, concept_a);
    if (concept_b != 0) fb = get_concept_features(gl, concept_b);

    if (!fa && !fb) return -1;

    /* Blend vectors */
    float* blended = (float*)nimcp_calloc(dim, sizeof(float));
    if (!blended) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "grounded_language_blend: failed to allocate blend vector");
        return -1;
    }

    if (fa && fb) {
        uint32_t use_dim = (vec_dim > 0 && vec_dim < dim) ? vec_dim : dim;
        for (uint32_t i = 0; i < use_dim; i++) {
            blended[i] = (1.0f - blend_ratio) * fa[i] + blend_ratio * fb[i];
        }
    } else if (fa) {
        memcpy(blended, fa, dim * sizeof(float));
    } else {
        uint32_t copy = (vec_dim < dim) ? vec_dim : dim;
        memcpy(blended, fb, copy * sizeof(float));
    }

    int rc = grounded_language_produce(gl, blended, dim, GL_PRODUCE_CREATE, result);
    result->creativity = 0.5f + 0.5f * fabsf(blend_ratio - 0.5f); /* More creative at extremes */

    nimcp_free(blended);
    return rc;
}

int grounded_language_narrate(grounded_language_t* gl, uint64_t seed_concept,
                               uint32_t num_sentences, float creativity,
                               gl_production_result_t* result) {
    if (!gl || !result || num_sentences == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_narrate: NULL parameter or zero sentences");
        return -1;
    }
    memset(result, 0, sizeof(*result));

    if (num_sentences > 10) num_sentences = 10; /* Cap for sanity */

    size_t buf_size = num_sentences * 256;
    char* buf = (char*)nimcp_calloc(buf_size, 1);
    if (!buf) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "grounded_language_narrate: failed to allocate narrative buffer");
        return -1;
    }

    result->semantic_vector = (float*)nimcp_calloc(gl->semantic_dim, sizeof(float));
    if (!result->semantic_vector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "grounded_language_narrate: failed to allocate semantic vector");
        nimcp_free(buf);
        return -1;
    }

    size_t pos = 0;
    uint32_t total_words = 0;
    uint64_t current_concept = seed_concept;

    for (uint32_t s = 0; s < num_sentences; s++) {
        /* Get features for current concept */
        const float* features = get_concept_features(gl, current_concept);
        if (!features) break;

        /* Produce a sentence for this concept */
        gl_production_result_t sentence = {0};
        gl_production_mode_t mode = (s == 0) ? GL_PRODUCE_DESCRIBE : GL_PRODUCE_ELABORATE;
        if (grounded_language_produce(gl, features, gl->semantic_dim, mode, &sentence) == 0) {
            if (sentence.text) {
                size_t slen = strlen(sentence.text);
                if (pos + slen + 2 < buf_size) {
                    memcpy(buf + pos, sentence.text, slen);
                    pos += slen;
                    buf[pos++] = ' ';
                    total_words += sentence.word_count;
                }
            }
            gl_production_result_cleanup(&sentence);
        }

        /* Navigate to related concept */
        if (gl->semantic_memory) {
            /* Spread activation from current concept to find next */
            semantic_memory_system_t* sm = gl->semantic_memory;
            uint64_t next_concept = 0;
            float best_score = -1.0f;

            for (uint32_t r = 0; r < sm->relation_count; r++) {
                if (!sm->relations[r]) continue;
                const semantic_relation_t* rel = sm->relations[r];

                uint64_t candidate = 0;
                if (rel->source_concept_id == current_concept) {
                    candidate = rel->target_concept_id;
                } else if (rel->target_concept_id == current_concept) {
                    candidate = rel->source_concept_id;
                }

                if (candidate != 0) {
                    float score = rel->strength;
                    /* Add randomness for creativity */
                    score += creativity * (gl_random(gl) - 0.5f) * 0.5f;
                    if (score > best_score) {
                        best_score = score;
                        next_concept = candidate;
                    }
                }
            }

            if (next_concept != 0) {
                current_concept = next_concept;
            }
        }
    }

    if (pos > 0 && buf[pos - 1] == ' ') pos--;
    buf[pos] = '\0';

    result->text = buf;
    result->word_count = total_words;
    result->fluency = (total_words > 0) ? 0.6f : 0.0f;
    result->relevance = 0.5f;
    result->creativity = creativity;

    return 0;
}

void gl_production_result_cleanup(gl_production_result_t* result) {
    if (!result) return;
    nimcp_free(result->text);
    nimcp_free(result->semantic_vector);
    memset(result, 0, sizeof(*result));
}

/*=============================================================================
 * Conversation
 *===========================================================================*/

int grounded_language_respond(grounded_language_t* gl, const char* input_text,
                               char* response, uint32_t response_max, float* confidence) {
    if (!gl || !input_text || !response || response_max == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_respond: NULL parameter or zero response_max");
        return -1;
    }

    /* === Phase 3c: INPUT-side toxicity check + counterclaim pushback ===
     * If the input is classified toxic AND a response engine is wired,
     * Athena emits a stage-appropriate counterclaim INSTEAD of running
     * the (likely-collapsed) produce path. This is the operational
     * expression of the core ethics directive: confronted with content
     * that fails the Golden Rule's "improve the human condition" goal,
     * Athena pushes back rather than passing it through.
     *
     * The classifier is still run on the OUTPUT below (defense in depth)
     * for the rare case the counterclaim path doesn't fire. */
    int input_is_toxic = 0;
    toxicity_result_t input_tox = {0};
    if (gl->toxicity_classifier && input_text && *input_text) {
        toxicity_classifier_t* tc =
            (toxicity_classifier_t*)gl->toxicity_classifier;
        if (toxicity_classify(tc, input_text, &input_tox) == 0 &&
            input_tox.would_block) {
            input_is_toxic = 1;
        }
    }

    /* Counterclaim short-circuit: if input is toxic, emit counterclaim
     * and skip produce entirely. We DO still comprehend (so the immune /
     * memory / audit machinery records the exposure) but the response is
     * the pushback, not the brain's normal produce output. */
    gl_comprehension_result_t comp = {0};
    int comp_rc = grounded_language_comprehend(gl, input_text, &comp);

    int counterclaim_emitted = 0;
    if (input_is_toxic && gl->toxicity_response) {
        toxicity_response_t* tr =
            (toxicity_response_t*)gl->toxicity_response;
        toxicity_response_result_t cr = {0};
        int stage = gl->current_stage;
        if (stage < 0) stage = 0;
        if (toxicity_response_generate(tr, input_text,
                                        input_tox.matched_category,
                                        stage, &cr) == 0 &&
            cr.text[0] != '\0') {
            strncpy(response, cr.text, response_max - 1);
            response[response_max - 1] = '\0';
            /* Counterclaims are confident assertions of values. */
            if (confidence) *confidence = 0.95f;
            counterclaim_emitted = 1;
            nimcp_safety_audit_log_event(
                NIMCP_SAFETY_AUDIT_LGSS_ACTION_BLOCKED, 1,
                "Counterclaim emitted: src=%s cat=%s stage=%d "
                "input='%.40s' response='%.60s'",
                cr.source, input_tox.matched_category, stage,
                input_text, response);
        }
    }

    /* Produce via the lexicon readout (Option B). No template fallbacks, no
     * quality-gated stock phrases. Produce succeeds → emit; produce
     * fails → emit "I don't know.", the curriculum-trained uncertainty
     * marker. The brain learns grammar emergently or admits it can't —
     * counterclaim path above is the only exception, and only for toxic
     * input. */
    gl_production_result_t prod = {0};
    int rc = -1;
    if (!counterclaim_emitted && comp_rc == 0 && comp.semantic_vector) {
        /* Phase 3 grounding: pass the concepts comprehension resolved from the
         * prompt (comp.activated_concepts + activation_levels) so the SNN
         * generator is seeded with the RIGHT concept instead of one re-derived
         * by cosine-ranking the structureless distributional embeddings. */
        rc = gl_produce_impl(gl, comp.semantic_vector, gl->semantic_dim,
                             GL_PRODUCE_RESPOND,
                             comp.activated_concepts, comp.activation_levels,
                             comp.concept_count, &prod);
    }

    if (counterclaim_emitted) {
        /* response + confidence already set by counterclaim block above. */
    } else if (rc == 0 && prod.text && prod.word_count > 0) {
        strncpy(response, prod.text, response_max - 1);
        response[response_max - 1] = '\0';
        if (confidence) *confidence = prod.fluency * prod.relevance;
    } else {
        snprintf(response, response_max, "I don't know.");
        if (confidence) *confidence = 0.0f;
    }

    /* Toxicity classification of the produced response (2026-05-21).
     * POLICY: mark, never delete — we annotate the response and emit a
     * safety-audit event but DO NOT modify the response text. The LGSS
     * ethics gate downstream can decide what to do with the scores.
     *
     * Round-1 walkthrough fix: skip this check when the response was a
     * counterclaim. Counterclaims are deliberate value assertions; running
     * the toxic classifier on them produces misleading audit entries and
     * incorrectly lowers their confidence. */
    if (!counterclaim_emitted && gl->toxicity_classifier && response[0] != '\0') {
        toxicity_classifier_t* tc =
            (toxicity_classifier_t*)gl->toxicity_classifier;
        toxicity_result_t tox = {0};
        if (toxicity_classify(tc, response, &tox) == 0 && tox.would_block) {
            char audit_msg[256];
            snprintf(audit_msg, sizeof(audit_msg),
                     "language_produce flagged: cat=%s harm=%.2f fair=%.2f "
                     "anti=%.2f input='%.40s' output='%.40s'",
                     tox.matched_category, tox.predicted_harm,
                     tox.fairness_violation, tox.anti_toxic_signal,
                     input_text, response);
            nimcp_safety_audit_log_event(
                NIMCP_SAFETY_AUDIT_LGSS_ACTION_BLOCKED, 2, "%s", audit_msg);
            /* Reduce confidence proportional to harm so any consumer that
             * gates on confidence sees the drop, without dropping content. */
            if (confidence) *confidence *= (1.0f - tox.predicted_harm);
            /* (KG emit at inference site is deferred — gl has no brain
             * back-pointer; the audit-log entry above is the durable record.
             * Training site emits KG events where brain is in scope.) */
        }
    }

    int result_len = (int)strlen(response);

    gl_comprehension_result_cleanup(&comp);
    gl_production_result_cleanup(&prod);

    return result_len;
}

/*=============================================================================
 * Training / Learning
 *===========================================================================*/

float grounded_language_learn_pair(grounded_language_t* gl, const char* input_text,
                                    const char* target_text, float learning_rate) {
    if (!gl || !input_text || !target_text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_learn_pair: NULL parameter");
        return -1.0f;
    }

    float lr = (learning_rate > 0.0f) ? learning_rate : gl->hebbian_lr;
    float saved_lr = gl->hebbian_lr;
    gl->hebbian_lr = lr;

    /* Comprehend input */
    gl_comprehension_result_t input_comp = {0};
    grounded_language_comprehend(gl, input_text, &input_comp);

    /* Comprehend target */
    gl_comprehension_result_t target_comp = {0};
    grounded_language_comprehend(gl, target_text, &target_comp);

    /* Learn distributional patterns from both texts */
    grounded_language_learn_from_text(gl, input_text);
    grounded_language_learn_from_text(gl, target_text);

    /* Cross-bind: input concepts should be associated with target words and vice versa */
    /* This teaches the brain that hearing X should lead to saying Y */

    /* Tokenize target for word-level binding */
    size_t tlen = strlen(target_text);
    char* tbuf = (char*)nimcp_malloc(tlen + 1);
    if (tbuf) {
        memcpy(tbuf, target_text, tlen + 1);
        char* twords[GL_MAX_PRODUCTION_WORDS];
        uint32_t tword_count = tokenize_text(tbuf, twords, GL_MAX_PRODUCTION_WORDS);

        /* For each input concept, bind to target words */
        for (uint32_t c = 0; c < input_comp.concept_count; c++) {
            uint64_t concept = input_comp.activated_concepts[c];
            float activation = input_comp.activation_levels[c];

            for (uint32_t w = 0; w < tword_count; w++) {
                gl_lexicon_entry_t* entry = lexicon_find_or_create(gl, twords[w]);
                if (!entry) continue;
                if (entry->learned_class == GL_CLASS_FUNCTION) continue;

                lexicon_bind(gl, entry, concept, activation * lr * 0.5f, GL_MODALITY_LINGUISTIC);
            }
        }

        /* For each target concept, bind to input words */
        size_t ilen = strlen(input_text);
        char* ibuf = (char*)nimcp_malloc(ilen + 1);
        if (ibuf) {
            memcpy(ibuf, input_text, ilen + 1);
            char* iwords[GL_MAX_PRODUCTION_WORDS];
            uint32_t iword_count = tokenize_text(ibuf, iwords, GL_MAX_PRODUCTION_WORDS);

            for (uint32_t c = 0; c < target_comp.concept_count; c++) {
                uint64_t concept = target_comp.activated_concepts[c];
                float activation = target_comp.activation_levels[c];

                for (uint32_t w = 0; w < iword_count; w++) {
                    gl_lexicon_entry_t* entry = lexicon_find_or_create(gl, iwords[w]);
                    if (!entry) continue;
                    if (entry->learned_class == GL_CLASS_FUNCTION) continue;

                    lexicon_bind(gl, entry, concept, activation * lr * 0.5f, GL_MODALITY_LINGUISTIC);
                }
            }
            nimcp_free(ibuf);
        }

        nimcp_free(tbuf);
    }

    /* Learn syntactic patterns from target */
    grounded_language_learn_syntax(gl, target_text);

    /* Compute loss: cosine distance between input semantics and target semantics */
    float loss = 1.0f;
    if (input_comp.semantic_vector && target_comp.semantic_vector) {
        float sim = cosine_similarity(input_comp.semantic_vector, target_comp.semantic_vector,
                                       gl->semantic_dim);
        loss = 1.0f - sim; /* 0 = perfect alignment, 1 = orthogonal */
    }

    gl->hebbian_lr = saved_lr;
    gl_comprehension_result_cleanup(&input_comp);
    gl_comprehension_result_cleanup(&target_comp);

    return loss;
}

int grounded_language_learn_syntax(grounded_language_t* gl, const char* text) {
    /* Templates removed. The brain learns syntactic structure emergently via
     * SNN bridge plasticity (spike→word bindings + context vectors). No-op
     * kept so existing callers compile. */
    (void)gl;
    (void)text;
    return 0;
}

/*=============================================================================
 * Cross-modal Connections
 *===========================================================================*/

void grounded_language_connect_visual(grounded_language_t* gl, void* vis_ctx) {
    if (!gl) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_connect_visual: NULL gl");
        return;
    }
    gl->visual_ctx = vis_ctx;
}

void grounded_language_connect_auditory(grounded_language_t* gl, void* aud_ctx) {
    if (!gl) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_connect_auditory: NULL gl");
        return;
    }
    gl->auditory_ctx = aud_ctx;
}

void grounded_language_connect_speech(grounded_language_t* gl, void* speech_ctx) {
    if (!gl) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_connect_speech: NULL gl");
        return;
    }
    gl->speech_ctx = speech_ctx;
}

void grounded_language_connect_columns(grounded_language_t* gl, void* col_pool) {
    if (!gl) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_connect_columns: NULL gl");
        return;
    }
    gl->column_pool = col_pool;
}

void grounded_language_connect_emotional(grounded_language_t* gl, void* emo_ctx) {
    if (!gl) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_connect_emotional: NULL gl");
        return;
    }
    gl->emotional_ctx = emo_ctx;
}

/* Forward decl — implemented in nimcp_grounded_language_nlp.c. */
extern int gl_word_emb_for_bridge_external(void* ctx,
                                            const char* word_form,
                                            float* out_vec,
                                            uint32_t out_dim);

void grounded_language_connect_snn_bridge(grounded_language_t* gl,
                                           struct snn_language_bridge* bridge) {
    if (!gl) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_connect_snn_bridge: NULL gl");
        return;
    }
    gl->snn_bridge = bridge;

    /* PA-5: if embeddings are already wired on gl, push the lookup down
     * to the freshly-attached bridge so a later glove_blend > 0 can use
     * the cosine-emb signal without re-attaching. */
    if (bridge && gl->embeddings && gl->word_to_id_fn && gl->emb_dim > 0) {
        snn_language_bridge_set_embedding_lookup(bridge,
                                                   gl_word_emb_for_bridge_external,
                                                   gl, gl->emb_dim);
    }
}

/* Walkthrough round 3 fix: shared counter for the cold-start guard in both
 * flat and Riemannian learn_next_token_pair variants. Process-global
 * (single brain in process is the common case); guarded by atomic. */
static _Atomic uint64_t g_next_token_cold_start_skips = 0;

static void gl_next_token_cold_start_skips_inc(const char* prev, const char* next) {
    uint64_t v = (uint64_t)__atomic_add_fetch(&g_next_token_cold_start_skips, 1,
                                                __ATOMIC_RELAXED);
    /* Periodic visibility: every 10000th skip emit a single line so a
     * trainer running into "all my updates fail" sees it in the log. */
    if ((v % 10000ULL) == 0) {
        LOG_DEBUG(LOG_MODULE,
                  "learn_next_token_pair cold-start skips=%llu (latest pair: '%s' → '%s')",
                  (unsigned long long)v,
                  prev ? prev : "(null)", next ? next : "(null)");
    }
}

uint64_t grounded_language_next_token_cold_start_skips(void) {
    return (uint64_t)__atomic_load_n(&g_next_token_cold_start_skips, __ATOMIC_RELAXED);
}

uint64_t grounded_language_rebind_all_to_snn_bridge(grounded_language_t* gl) {
    if (!gl) return 0;
    if (!gl->snn_bridge) return 0;        /* no bridge attached → no-op */
    if (!gl->vocab_list || gl->vocab_count == 0) return 0;

    uint64_t mirrored = 0;
    for (uint32_t i = 0; i < gl->vocab_count; i++) {
        const gl_lexicon_entry_t* e = gl->vocab_list[i];
        if (!e || e->binding_count == 0) continue;
        for (uint32_t b = 0; b < e->binding_count; b++) {
            mirror_binding_to_bridge(gl, e,
                                      e->bindings[b].concept_id,
                                      e->bindings[b].strength);
            mirrored++;
        }
    }
    LOG_INFO(LOG_MODULE,
             "rebind_all_to_snn_bridge: mirrored %llu bindings across %u words",
             (unsigned long long)mirrored, gl->vocab_count);
    return mirrored;
}

/* Pseudo-concept-id remap (2026-05-30).
 *
 * Bindings created when gl->semantic_memory was NULL hold pseudo-concept_ids
 * of the form (feature_hash | 0x100000000ULL) — see find_or_create_concept's
 * NULL-semantic-memory branch. These IDs never resolve in
 * semantic_memory_get_concept (the registry has no entry for them), so the
 * concept-feature path in comprehend's semantic_vector accumulation is
 * permanently zero for every such binding.
 *
 * This was the root cause of the 2026-05-29 pod-side mode collapse: the
 * daemon's --resume path triggers lazy_init_mode (>100K neurons), which
 * skips init_working_memory_subsystem, which means brain->semantic_memory
 * was NULL at every grounding event in the brain's training history.
 *
 * Migration strategy: for each binding whose concept_id has bit 32 set,
 * if the entry has a learned distributional context_vector, mint a fresh
 * real concept_id by calling semantic_memory_create_concept with that
 * vector as features, and replace the binding's concept_id. The freshly-
 * minted concept's features will equal context_vector — so the concept
 * path through get_concept_features now contributes a non-zero signal
 * (equivalent in direction to context_vector itself; downstream
 * score_word_against_vector's 0.4·sim + 0.6·concept_sim becomes
 * cos(target, context_vector) instead of zero).
 *
 * Words whose entry has context_initialized=0 (no distributional embedding
 * yet — e.g. peace/hope in the current pod checkpoint) get their
 * pseudo-binding marker preserved; they'll re-ground organically when
 * the trainer next sees them with semantic_memory available. We could
 * fall back to GloVe lookup here, but that path needs the NLP embedding
 * subsystem wired and isn't always available. Keep the migration narrow.
 *
 * NULL-safe on both gl and gl->semantic_memory. Returns count of bindings
 * remapped (or 0 if nothing was eligible). */
uint64_t grounded_language_remap_pseudo_concept_ids(grounded_language_t* gl) {
    if (!gl) return 0;
    if (!gl->semantic_memory) return 0;
    if (!gl->vocab_list || gl->vocab_count == 0) return 0;

    uint64_t remapped        = 0;   /* bindings repointed to a real concept */
    uint64_t still_pseudo    = 0;   /* pseudo bindings left (no context vec) */
    uint64_t already_real    = 0;   /* bindings that were already real */
    uint64_t words_migrated  = 0;   /* distinct words that got a new concept */
    uint64_t create_failed   = 0;   /* semantic_memory_create_concept == 0 */

    /* Mint ONE concept per WORD (seeded from its distributional
     * context_vector) and repoint ALL of that word's pseudo bindings at it.
     *
     * The earlier per-binding version called semantic_memory_create_concept
     * once per binding — millions of calls, each emitting a KG
     * concept-created event with an O(KG) integrity-checksum recompute.
     * That turned the migration into an O(bindings × KG) CPU hang during
     * resume. Per-word collapses it to O(words) creates (a few × 10^4) and
     * keeps the semantics right: a word denotes one concept, not one
     * concept per co-occurrence binding. */
    gl_mutate_lock(gl);
    for (uint32_t v = 0; v < gl->vocab_count; v++) {
        gl_lexicon_entry_t* entry = gl->vocab_list[v];
        if (!entry || entry->binding_count == 0) continue;

        /* Does this word have any pseudo binding worth migrating? */
        bool has_pseudo = false;
        for (uint32_t b = 0; b < entry->binding_count; b++) {
            if (entry->bindings[b].concept_id & 0x100000000ULL) {
                has_pseudo = true;
                break;
            }
        }
        if (!has_pseudo) {
            already_real += entry->binding_count;
            continue;
        }

        /* Need a feature vector to seed the new concept. The word's
         * distributional context_vector is the best available proxy for
         * the semantic content the original grounding carried — it's been
         * trained over many co-occurrences. Without it, leave the pseudo
         * bindings as-is (counted) rather than minting a zero concept. */
        if (!entry->context_initialized || !entry->context_vector) {
            for (uint32_t b = 0; b < entry->binding_count; b++) {
                if (entry->bindings[b].concept_id & 0x100000000ULL)
                    still_pseudo++;
                else
                    already_real++;
            }
            continue;
        }

        uint64_t new_cid = semantic_memory_create_concept(
            gl->semantic_memory,
            entry->context_vector,
            gl->semantic_dim,
            entry->form,
            CONCEPT_OBJECT);
        if (new_cid == 0) {
            /* Registry full or alloc failure — leave pseudo bindings. */
            for (uint32_t b = 0; b < entry->binding_count; b++) {
                if (entry->bindings[b].concept_id & 0x100000000ULL)
                    create_failed++;
                else
                    already_real++;
            }
            continue;
        }
        words_migrated++;

        for (uint32_t b = 0; b < entry->binding_count; b++) {
            if (entry->bindings[b].concept_id & 0x100000000ULL) {
                entry->bindings[b].concept_id = new_cid;
                remapped++;
            } else {
                already_real++;
            }
        }
    }
    gl_mutate_unlock(gl);

    fprintf(stderr,
            "[GL_REMAP] pseudo-concept-id migration: words_migrated=%llu "
            "bindings_remapped=%llu still_pseudo_no_ctx=%llu already_real=%llu "
            "create_failed=%llu (words=%u)\n",
            (unsigned long long)words_migrated,
            (unsigned long long)remapped,
            (unsigned long long)still_pseudo,
            (unsigned long long)already_real,
            (unsigned long long)create_failed,
            gl->vocab_count);
    return remapped;
}

/* Phase 2C: GL→Broca lexicon mirror.
 *
 * Walks gl->vocab_list[] and pushes each entry into Broca's separate
 * lexicon via broca_add_lexical_entry. Without this, Broca's CYK chart
 * parser sees every WordNet word as POS_UNKNOWN, no rule fires, and
 * syntax_build_tree returns false on every input. With it, content
 * words have proper part-of-speech tags so the chart parser can
 * actually fire rules like S→NP VP, NP→DET N, etc.
 *
 * Mapping gl_word_class_t → part_of_speech_t:
 *   GL_CLASS_NOUN       → POS_NOUN
 *   GL_CLASS_VERB       → POS_VERB
 *   GL_CLASS_ADJECTIVE  → POS_ADJECTIVE
 *   GL_CLASS_ADVERB     → POS_ADVERB
 *   GL_CLASS_PRONOUN    → POS_PRONOUN
 *   GL_CLASS_FUNCTION   → POS_DETERMINER (most common; mismatches
 *                                          for prep/conj are tolerated
 *                                          since the bridge produces
 *                                          mostly content words)
 *   GL_CLASS_UNKNOWN    → POS_UNKNOWN
 *
 * Phase 2D will refine GL_CLASS_FUNCTION via a sub-category lookup
 * table for the ~50 common function words that need precise POS.
 *
 * word_id: derived from form_hash, same hash space the SNN bridge
 * mirror uses. This keeps lookups consistent across modules.
 *
 * Phonemes: left empty here. Broca's phonological_processor generates
 * phonemes on-demand from spelling; the lexical entry's phoneme array
 * is only consulted when present, so an empty array forces the
 * fallback path. Phase 2E will add proper phoneme generation. */
static uint8_t gl_class_to_pos(gl_word_class_t cls) {
    /* part_of_speech_t values from nimcp_syntax_processor.h:
     *   POS_NOUN=80, _VERB=81, _ADJECTIVE=82, _ADVERB=83, _PRONOUN=84,
     *   _DETERMINER=87, _UNKNOWN=97. The enum is contiguous, so we
     *   use literals here to avoid a header include in this TU. */
    switch (cls) {
        case GL_CLASS_NOUN:      return 1;  /* POS_NOUN */
        case GL_CLASS_VERB:      return 2;  /* POS_VERB */
        case GL_CLASS_ADJECTIVE: return 3;  /* POS_ADJECTIVE */
        case GL_CLASS_ADVERB:    return 4;  /* POS_ADVERB */
        case GL_CLASS_PRONOUN:   return 5;  /* POS_PRONOUN */
        case GL_CLASS_FUNCTION:  return 7;  /* POS_DETERMINER */
        case GL_CLASS_UNKNOWN:
        default:                 return 0;  /* POS_UNKNOWN */
    }
}

/* Forward decl to avoid pulling broca's header into this TU. We pass
 * adapter as void* so the public API stays POD-clean; the real type
 * matches when called via grounded_language_mirror_lexicon_to_broca. */
extern bool broca_add_lexical_entry_v(void* adapter,
                                       uint32_t word_id,
                                       const char* word,
                                       uint8_t pos,
                                       float frequency);

uint64_t grounded_language_mirror_lexicon_to_broca(grounded_language_t* gl,
                                                     void* broca_adapter) {
    if (!gl || !broca_adapter) return 0;
    if (!gl->vocab_list || gl->vocab_count == 0) return 0;

    uint64_t mirrored = 0;
    uint64_t skipped  = 0;
    for (uint32_t i = 0; i < gl->vocab_count; i++) {
        const gl_lexicon_entry_t* e = gl->vocab_list[i];
        if (!e || !e->form[0]) { skipped++; continue; }

        /* Frequency normalized into [0,1]. Linear scale; saturates at
         * 1000 exposures. Phase 2D will replace with a Zipf-aware scale. */
        float freq_norm = (float)e->frequency / 1000.0f;
        if (freq_norm > 1.0f) freq_norm = 1.0f;

        /* word_id=0 forces broca's hash table to index by hash_string(form),
         * which is what broca_add_word does at lookup time. If we used a
         * non-zero word_id (e.g. form_hash), entries would be inserted
         * via hash_word_id but looked up via hash_string and collide
         * into the wrong bucket — silent miss. Use 0 here for
         * lookup-consistency. (Discovered after Phase 2C v1 had every
         * broca_add_word call fail with LEXICON_MISS.) */
        if (broca_add_lexical_entry_v(broca_adapter,
                                        0 /* word_id — let hash_string drive */,
                                        e->form,
                                        gl_class_to_pos(e->learned_class),
                                        freq_norm)) {
            mirrored++;
        } else {
            skipped++;
        }
    }

    LOG_INFO(LOG_MODULE,
             "mirror_lexicon_to_broca: mirrored %llu words, skipped %llu "
             "(broca lexicon now has %u-class GL words)",
             (unsigned long long)mirrored,
             (unsigned long long)skipped,
             gl->vocab_count);
    return mirrored;
}

/* ============================================================================
 * PA-4: Next-token contrastive training on the bridge binding matrix.
 *
 * For each (prev, next) bigram:
 *   1. Encode prev → concept_acts (the bridge's reverse lookup over its
 *      existing bindings to prev_word_pop).
 *   2. Decode top-K candidate words via the existing cosine score.
 *   3. If next_word is not the top-1, apply LTP to bindings (c, next_word)
 *      for active concepts c, and mild LTD to bindings (c, top_word) for
 *      the false winner — a one-step approximation of softmax cross-entropy
 *      gradient descent restricted to the target row and top-1 false
 *      winner row.
 *
 * No-op when bridge is unattached, prev has no encoding (cold-start),
 * lr is non-positive, or either word can't be hashed into the lexicon.
 * ============================================================================ */
int grounded_language_learn_next_token_pair(grounded_language_t* gl,
                                              const char* prev_word,
                                              const char* next_word,
                                              float lr)
{
    if (!gl || !prev_word || !next_word) return -1;
    if (!gl->snn_bridge) return -1;
    if (!isfinite(lr) || lr <= 0.0f) return -1;

    /* Find/create lexicon entries (assigns form_hash). */
    gl_lexicon_entry_t* prev_e =
        gl_internal_lexicon_find_or_create(gl, prev_word);
    gl_lexicon_entry_t* next_e =
        gl_internal_lexicon_find_or_create(gl, next_word);
    if (!prev_e || !next_e) return -1;

    uint32_t prev_word_pop = prev_e->form_hash % SNN_LANG_MAX_WORD_POPS;
    uint32_t next_word_pop = next_e->form_hash % SNN_LANG_MAX_WORD_POPS;

    /* Make sure both word_pops are registered with the bridge. */
    snn_language_bridge_register_word(gl->snn_bridge, prev_word_pop,
                                       prev_e->form);
    snn_language_bridge_register_word(gl->snn_bridge, next_word_pop,
                                       next_e->form);

    /* Encode prev_word → concept_acts. Heap allocation since 4096 floats
     * (16 KB) is too large for the stack. */
    const uint32_t n_concepts = SNN_LANG_MAX_CONCEPT_POPS;
    float* concept_acts = (float*)nimcp_calloc(n_concepts, sizeof(float));
    if (!concept_acts) return -1;

    if (snn_language_bridge_encode_word(gl->snn_bridge, prev_word_pop,
                                         concept_acts, n_concepts) != 0) {
        nimcp_free(concept_acts);
        return -1;
    }

    /* Cold-start guard: prev_word has no bindings yet. Nothing to condition
     * on. The first comprehension pass for prev_word will create bindings,
     * after which subsequent calls of this function become trainable.
     *
     * Walkthrough round 3 fix: count cold-start skips so the trainer can
     * see "all my pair updates failed silently" via a public stat instead
     * of guessing why the model isn't learning. Periodic LOG_DEBUG every
     * 10000 skips for in-the-loop visibility. */
    float total_act = 0.0f;
    for (uint32_t c = 0; c < n_concepts; c++) total_act += concept_acts[c];
    if (total_act <= 1e-6f) {
        gl_next_token_cold_start_skips_inc(prev_word, next_word);
        nimcp_free(concept_acts);
        return -1;
    }

    /* Decode top-K candidates given prev's encoding. */
    snn_lang_word_result_t topK[16];
    uint32_t num_out = 0;
    int rc = snn_language_bridge_decode_spikes(gl->snn_bridge, concept_acts,
                                                 n_concepts, topK,
                                                 16, &num_out);
    if (rc != 0) {
        nimcp_free(concept_acts);
        return -1;
    }

    /* Find rank of next_word_pop in the top-K (-1 if outside). */
    int target_rank = -1;
    for (uint32_t k = 0; k < num_out; k++) {
        if (topK[k].word_pop == next_word_pop) {
            target_rank = (int)k;
            break;
        }
    }

    /* LTP on (c, next_word_pop) — pull the target word toward the prefix's
     * concept activation profile. Active-only: skip near-zero concepts so
     * we don't waste a binding on noise.
     *
     * Walkthrough-2 (Option-1 rebuild, 2026-05-19): under the rebuild the
     * bridge is transport-only — `strengthen_binding` is a no-op stub.
     * This loop still runs; calls return success but no learning happens
     * here. Next-token learning needs to migrate to SNN-side three-factor
     * STDP on projection synapses (deferred walkthrough). Durable lexicon
     * binding strengths are still updated via `lexicon_bind` on the
     * concept-grounding path, which is independent of these calls. */
    const float ltp_threshold = 0.05f;
    for (uint32_t c = 0; c < n_concepts; c++) {
        if (concept_acts[c] > ltp_threshold) {
            float delta = lr * concept_acts[c];
            snn_language_bridge_strengthen_binding(gl->snn_bridge,
                                                    c, next_word_pop, delta);
        }
    }

    /* LTD on (c, top-1 false winner) when target wasn't already #1. Half
     * the LR so LTD is gentler than LTP — biology and softmax CE both
     * prefer asymmetric updates in this direction. Skip if top-1 is the
     * target itself (already learned correctly).
     *
     * Margin gate: only LTD when the false_winner is *decisively* beating
     * the target in this specific context, not just dominant globally.
     * Without this gate, indiscriminate LTD-on-top-1 erodes every
     * globally-dominant binding regardless of whether it actually
     * competed here — the step-3900 regression mechanism. We require:
     *   (a) target appears in topK (rank in [0, num_out)) so the bridge
     *       has at least *some* presence to compare against
     *   (b) topK[0].confidence >= ltd_margin * topK[target_rank].confidence
     *       so the false_winner is winning by a meaningful margin
     * Skip the LTD step entirely when either fails. */
    if (target_rank != 0 && num_out > 0) {
        uint32_t false_winner = topK[0].word_pop;
        if (false_winner != next_word_pop) {
            bool apply_ltd = false;
            if (target_rank > 0 && (uint32_t)target_rank < num_out) {
                float winner_conf = topK[0].confidence;
                float target_conf = topK[target_rank].confidence;
                float margin = snn_language_bridge_get_ltd_margin(gl->snn_bridge);
                if (margin < 1.0f) margin = 1.5f;   /* safety net */
                if (target_conf <= 0.0f) {
                    /* Target is in topK but has zero/negative confidence —
                     * essentially absent. Treat as "no in-context signal",
                     * skip LTD. */
                    apply_ltd = false;
                } else if (winner_conf >= margin * target_conf) {
                    apply_ltd = true;
                }
            }
            if (apply_ltd) {
                float ltd_lr = -0.5f * lr;
                for (uint32_t c = 0; c < n_concepts; c++) {
                    if (concept_acts[c] > ltp_threshold) {
                        snn_language_bridge_strengthen_binding(gl->snn_bridge,
                                                                c, false_winner,
                                                                ltd_lr * concept_acts[c]);
                    }
                }
            }
        }
    }

    nimcp_free(concept_acts);
    return 0;
}

/* ============================================================================
 * PA-4+: Riemannian / sigmoid-reparameterized next-token training.
 *
 * Same overall contrastive structure as grounded_language_learn_next_token_pair:
 *   1. Encode prev → concept_acts.
 *   2. Decode top-K and find target rank.
 *   3. LTP on (c, next_word_pop), LTD on (c, top-1 false winner).
 *
 * The only difference is each binding update goes through
 * snn_language_bridge_strengthen_binding_riemannian — Fisher-preconditioned
 * step in u-space (w = σ(u)), so binding weights near 0 or 1 are damped
 * automatically instead of being clipped post-hoc.
 *
 * Default OFF: callers must invoke this *_riemannian variant explicitly.
 * The flat learn_next_token_pair path remains bit-for-bit unchanged.
 * ============================================================================ */
int grounded_language_learn_next_token_pair_riemannian(grounded_language_t* gl,
                                                        const char* prev_word,
                                                        const char* next_word,
                                                        float lr)
{
    if (!gl || !prev_word || !next_word) return -1;
    if (!gl->snn_bridge) return -1;
    if (!isfinite(lr) || lr <= 0.0f) return -1;

    gl_lexicon_entry_t* prev_e =
        gl_internal_lexicon_find_or_create(gl, prev_word);
    gl_lexicon_entry_t* next_e =
        gl_internal_lexicon_find_or_create(gl, next_word);
    if (!prev_e || !next_e) return -1;

    uint32_t prev_word_pop = prev_e->form_hash % SNN_LANG_MAX_WORD_POPS;
    uint32_t next_word_pop = next_e->form_hash % SNN_LANG_MAX_WORD_POPS;

    snn_language_bridge_register_word(gl->snn_bridge, prev_word_pop,
                                       prev_e->form);
    snn_language_bridge_register_word(gl->snn_bridge, next_word_pop,
                                       next_e->form);

    const uint32_t n_concepts = SNN_LANG_MAX_CONCEPT_POPS;
    float* concept_acts = (float*)nimcp_calloc(n_concepts, sizeof(float));
    if (!concept_acts) return -1;

    if (snn_language_bridge_encode_word(gl->snn_bridge, prev_word_pop,
                                         concept_acts, n_concepts) != 0) {
        nimcp_free(concept_acts);
        return -1;
    }

    float total_act = 0.0f;
    for (uint32_t c = 0; c < n_concepts; c++) total_act += concept_acts[c];
    if (total_act <= 1e-6f) {
        nimcp_free(concept_acts);
        return -1;
    }

    snn_lang_word_result_t topK[16];
    uint32_t num_out = 0;
    int rc = snn_language_bridge_decode_spikes(gl->snn_bridge, concept_acts,
                                                 n_concepts, topK,
                                                 16, &num_out);
    if (rc != 0) {
        nimcp_free(concept_acts);
        return -1;
    }

    int target_rank = -1;
    for (uint32_t k = 0; k < num_out; k++) {
        if (topK[k].word_pop == next_word_pop) {
            target_rank = (int)k;
            break;
        }
    }

    /* LTP on (c, next_word_pop) — Riemannian step. `grad` here is
     * lr * concept_acts[c], same as the flat path's delta; the bridge
     * function applies the Fisher preconditioner internally. */
    const float ltp_threshold = 0.05f;
    for (uint32_t c = 0; c < n_concepts; c++) {
        if (concept_acts[c] > ltp_threshold) {
            float grad = lr * concept_acts[c];
            snn_language_bridge_strengthen_binding_riemannian(gl->snn_bridge,
                                                               c, next_word_pop,
                                                               grad);
        }
    }

    /* Same margin gate as the flat path — see learn_next_token_pair for
     * rationale. Riemannian step doesn't change the contrastive
     * structure, just the per-binding update rule, so the indiscriminate-
     * LTD failure mode applies identically. */
    if (target_rank != 0 && num_out > 0) {
        uint32_t false_winner = topK[0].word_pop;
        if (false_winner != next_word_pop) {
            bool apply_ltd = false;
            if (target_rank > 0 && (uint32_t)target_rank < num_out) {
                float winner_conf = topK[0].confidence;
                float target_conf = topK[target_rank].confidence;
                float margin = snn_language_bridge_get_ltd_margin(gl->snn_bridge);
                if (margin < 1.0f) margin = 1.5f;
                if (target_conf <= 0.0f) {
                    apply_ltd = false;
                } else if (winner_conf >= margin * target_conf) {
                    apply_ltd = true;
                }
            }
            if (apply_ltd) {
                float ltd_lr = -0.5f * lr;
                for (uint32_t c = 0; c < n_concepts; c++) {
                    if (concept_acts[c] > ltp_threshold) {
                        snn_language_bridge_strengthen_binding_riemannian(
                            gl->snn_bridge, c, false_winner,
                            ltd_lr * concept_acts[c]);
                    }
                }
            }
        }
    }

    nimcp_free(concept_acts);
    return 0;
}

/* ============================================================================
 * TA-4: Trigram next-token training.
 *
 * Same one-step contrastive update as learn_next_token_pair, but the
 * conditioning prefix is the average of TWO prior tokens' reverse encodings.
 * The merged context vector lets LTP/LTD reflect a (prev1, prev2) → next
 * association without modifying the bridge's binding-shape (still
 * concept_acts → word_pop bindings, just with a richer prefix).
 *
 * Default OFF. Activated only when the bridge's trigram-learning runtime
 * flag is on AND callers explicitly invoke this function (or
 * learn_text_bigrams under the flag).
 *
 * Cold-start: if EITHER prev1 OR prev2 encodes to all-zero (no prior
 * bridge bindings), the merged context is too sparse to teach against,
 * so we skip and bump the shared cold-start counter.
 * ============================================================================ */
int grounded_language_learn_next_token_triple(grounded_language_t* gl,
                                                const char* prev1,
                                                const char* prev2,
                                                const char* next_word,
                                                float lr)
{
    if (!gl || !prev1 || !prev2 || !next_word) return -1;
    if (!gl->snn_bridge) return -1;
    if (!isfinite(lr) || lr <= 0.0f) return -1;

    /* Find/create lexicon entries for all three tokens. */
    gl_lexicon_entry_t* p1_e =
        gl_internal_lexicon_find_or_create(gl, prev1);
    gl_lexicon_entry_t* p2_e =
        gl_internal_lexicon_find_or_create(gl, prev2);
    gl_lexicon_entry_t* next_e =
        gl_internal_lexicon_find_or_create(gl, next_word);
    if (!p1_e || !p2_e || !next_e) return -1;

    uint32_t p1_word_pop   = p1_e->form_hash   % SNN_LANG_MAX_WORD_POPS;
    uint32_t p2_word_pop   = p2_e->form_hash   % SNN_LANG_MAX_WORD_POPS;
    uint32_t next_word_pop = next_e->form_hash % SNN_LANG_MAX_WORD_POPS;

    /* Register all three with the bridge. */
    snn_language_bridge_register_word(gl->snn_bridge, p1_word_pop,   p1_e->form);
    snn_language_bridge_register_word(gl->snn_bridge, p2_word_pop,   p2_e->form);
    snn_language_bridge_register_word(gl->snn_bridge, next_word_pop, next_e->form);

    const uint32_t n_concepts = SNN_LANG_MAX_CONCEPT_POPS;
    float* acts1 = (float*)nimcp_calloc(n_concepts, sizeof(float));
    float* acts2 = (float*)nimcp_calloc(n_concepts, sizeof(float));
    float* context = (float*)nimcp_calloc(n_concepts, sizeof(float));
    if (!acts1 || !acts2 || !context) {
        if (acts1)   nimcp_free(acts1);
        if (acts2)   nimcp_free(acts2);
        if (context) nimcp_free(context);
        return -1;
    }

    int rc1 = snn_language_bridge_encode_word(gl->snn_bridge, p1_word_pop,
                                                acts1, n_concepts);
    int rc2 = snn_language_bridge_encode_word(gl->snn_bridge, p2_word_pop,
                                                acts2, n_concepts);
    if (rc1 != 0 || rc2 != 0) {
        nimcp_free(acts1); nimcp_free(acts2); nimcp_free(context);
        return -1;
    }

    /* Cold-start guards: BOTH prev1 and prev2 must have prior bindings.
     * If either is uninformative the merged context collapses toward the
     * other token's bigram-conditioned signal, which is what
     * learn_next_token_pair already gives us — no need to redo it here. */
    float total1 = 0.0f, total2 = 0.0f;
    for (uint32_t c = 0; c < n_concepts; c++) {
        total1 += acts1[c];
        total2 += acts2[c];
    }
    if (total1 <= 1e-6f || total2 <= 1e-6f) {
        gl_next_token_cold_start_skips_inc(prev1, next_word);
        nimcp_free(acts1); nimcp_free(acts2); nimcp_free(context);
        return -1;
    }

    /* Merged context = arithmetic mean of the two encodings. Equal weights
     * is the simplest baseline; positional weighting (e.g. 0.4·prev1 +
     * 0.6·prev2) is a future TA-4+ knob. */
    float total_ctx = 0.0f;
    for (uint32_t c = 0; c < n_concepts; c++) {
        context[c] = 0.5f * (acts1[c] + acts2[c]);
        total_ctx += context[c];
    }
    if (total_ctx <= 1e-6f) {
        /* Safety net: shouldn't trigger given both totals are >ε, but
         * floating-point cancellation could in principle reduce the sum. */
        gl_next_token_cold_start_skips_inc(prev1, next_word);
        nimcp_free(acts1); nimcp_free(acts2); nimcp_free(context);
        return -1;
    }

    /* Decode top-K conditioned on merged context to find the false winner. */
    snn_lang_word_result_t topK[16];
    uint32_t num_out = 0;
    int rc = snn_language_bridge_decode_spikes(gl->snn_bridge, context,
                                                 n_concepts, topK,
                                                 16, &num_out);
    if (rc != 0) {
        nimcp_free(acts1); nimcp_free(acts2); nimcp_free(context);
        return -1;
    }

    int target_rank = -1;
    for (uint32_t k = 0; k < num_out; k++) {
        if (topK[k].word_pop == next_word_pop) {
            target_rank = (int)k;
            break;
        }
    }

    /* LTP on (c, next_word_pop) using the merged context. Same threshold
     * + active-only filter as the bigram path. */
    const float ltp_threshold = 0.05f;
    for (uint32_t c = 0; c < n_concepts; c++) {
        if (context[c] > ltp_threshold) {
            float delta = lr * context[c];
            snn_language_bridge_strengthen_binding(gl->snn_bridge,
                                                    c, next_word_pop, delta);
        }
    }

    /* LTD on top-1 false winner, half-strength (matches bigram path).
     * Same margin gate as learn_next_token_pair — see that function for
     * the rationale. Critical for the trigram pump in particular: with
     * ~85 trigram updates/min driven by learn_text_bigrams under the
     * trigram flag, the unconditional LTD collapsed bridge_avg_binding_weight
     * by 27% in ~3 hours (step-3900 regression). */
    if (target_rank != 0 && num_out > 0) {
        uint32_t false_winner = topK[0].word_pop;
        if (false_winner != next_word_pop) {
            bool apply_ltd = false;
            if (target_rank > 0 && (uint32_t)target_rank < num_out) {
                float winner_conf = topK[0].confidence;
                float target_conf = topK[target_rank].confidence;
                float margin = snn_language_bridge_get_ltd_margin(gl->snn_bridge);
                if (margin < 1.0f) margin = 1.5f;   /* safety net */
                if (target_conf <= 0.0f) {
                    apply_ltd = false;
                } else if (winner_conf >= margin * target_conf) {
                    apply_ltd = true;
                }
            }
            if (apply_ltd) {
                float ltd_lr = -0.5f * lr;
                for (uint32_t c = 0; c < n_concepts; c++) {
                    if (context[c] > ltp_threshold) {
                        snn_language_bridge_strengthen_binding(gl->snn_bridge,
                                                                c, false_winner,
                                                                ltd_lr * context[c]);
                    }
                }
            }
        }
    }

    /* Bridge stat: one-trigram-update-per-call, regardless of how many
     * concept rows were touched. Mirrors the "applied bool" semantics of
     * the bigram path's return value. */
    snn_language_bridge_inc_trigram_updates(gl->snn_bridge);

    nimcp_free(acts1); nimcp_free(acts2); nimcp_free(context);
    return 0;
}

/*=============================================================================
 * PA-4+ : optional bigram-spectrum tracker
 *
 * The spectrum lives in caller-owned memory and is referenced from a
 * small static side-map keyed by gl pointer. We avoid touching the
 * grounded_language struct layout (the persistence sidecar walks it
 * field-by-field via nimcp_grounded_language_internal.h), so an opt-in
 * side-channel keeps the layout untouched while still letting the
 * trainer attach a diagnostic.
 *
 * Cap is intentionally tiny — at most one spectrum per active brain,
 * and brains rarely exceed two in the same process.
 *===========================================================================*/

/* TC-12: per-gl mutex helpers. Initialized lazily on first use so the
 * gl create path doesn't have to know about pthread initialization
 * ordering. tc12_lock_inited is volatile-ordered through the same
 * lock that guards the field once initialized — race on first init is
 * resolved by a process-wide bootstrap mutex. */
static pthread_mutex_t g_tc12_bootstrap = PTHREAD_MUTEX_INITIALIZER;

static void gl_tc12_lock(grounded_language_t* gl) {
    /* Lazy init protected by a process-wide bootstrap mutex so concurrent
     * first-callers don't double-initialize. After init, the per-gl mutex
     * carries the load alone.
     *
     * Double-checked locking: the outer load uses __atomic_load_n with
     * acquire ordering so that on weak-memory architectures we observe
     * the mutex initialization that happened-before the inited flag was
     * set. Pre-fix, the outer check read a plain bool without a memory
     * barrier — on x86 the strong store ordering masked the bug, but on
     * ARM a thread could see tc12_lock_inited=true while pthread_mutex_init
     * hadn't yet propagated. The store inside the bootstrap-mutex region
     * uses release ordering to pair with the acquire load. */
    if (!__atomic_load_n(&gl->tc12_lock_inited, __ATOMIC_ACQUIRE)) {
        pthread_mutex_lock(&g_tc12_bootstrap);
        if (!gl->tc12_lock_inited) {
            pthread_mutex_init(&gl->tc12_lock, NULL);
            __atomic_store_n(&gl->tc12_lock_inited, true, __ATOMIC_RELEASE);
        }
        pthread_mutex_unlock(&g_tc12_bootstrap);
    }
    pthread_mutex_lock(&gl->tc12_lock);
}

static void gl_tc12_unlock(grounded_language_t* gl) {
    pthread_mutex_unlock(&gl->tc12_lock);
}

/* Audit walkthrough-2 — mutate_lock helpers. Same lazy-init pattern as
 * tc12_lock; separate mutex so the two locks never need to be held
 * simultaneously (avoids lock-ordering hazards). mutate_lock covers
 * lexicon + discourse + phrase mutations from comprehend; tc12_lock
 * covers anaphora + spectrum. */
static pthread_mutex_t g_mutate_bootstrap = PTHREAD_MUTEX_INITIALIZER;

static void gl_mutate_lock(grounded_language_t* gl) {
    if (!__atomic_load_n(&gl->mutate_lock_inited, __ATOMIC_ACQUIRE)) {
        pthread_mutex_lock(&g_mutate_bootstrap);
        if (!gl->mutate_lock_inited) {
            pthread_mutex_init(&gl->mutate_lock, NULL);
            __atomic_store_n(&gl->mutate_lock_inited, true, __ATOMIC_RELEASE);
        }
        pthread_mutex_unlock(&g_mutate_bootstrap);
    }
    pthread_mutex_lock(&gl->mutate_lock);
}

static void gl_mutate_unlock(grounded_language_t* gl) {
    pthread_mutex_unlock(&gl->mutate_lock);
}

static bigram_spectrum_t* gl_get_attached_spectrum(grounded_language_t* gl) {
    if (!gl) return NULL;
    gl_tc12_lock(gl);
    bigram_spectrum_t* found = (bigram_spectrum_t*)gl->bigram_spectrum;
    gl_tc12_unlock(gl);
    return found;
}

/* TC-12: per-gl detach is now part of grounded_language_destroy proper.
 * Kept as a no-op stub so the existing destroy callsite stays unchanged
 * (the destroy-time NULLing happens inside the new free helper below). */
static void gl_detach_spectrum_for_destroy(grounded_language_t* gl) {
    if (!gl) return;
    gl_tc12_lock(gl);
    gl->bigram_spectrum = NULL;
    gl_tc12_unlock(gl);
}

void grounded_language_attach_bigram_spectrum(grounded_language_t* gl,
                                                void* spectrum) {
    if (!gl) return;
    gl_tc12_lock(gl);
    gl->bigram_spectrum = spectrum;  /* NULL = detach */
    gl_tc12_unlock(gl);
}

int grounded_language_tick_bigram_spectrum(grounded_language_t* gl,
                                             uint64_t min_delta_events) {
    if (!gl) return 0;
    /* Audit fix (TOCTOU): hold the per-gl lock for the entire compute,
     * not just the lookup. Otherwise a concurrent attach(NULL) detach
     * could free the spectrum mid-compute. The compute is small (~tens
     * of µs at production vocab) so holding the lock doesn't meaningfully
     * impact concurrency — the lock is per-gl, so other gls' ticks +
     * comprehends remain unblocked. */
    gl_tc12_lock(gl);
    bigram_spectrum_t* bs = (bigram_spectrum_t*)gl->bigram_spectrum;
    if (!bs) {
        gl_tc12_unlock(gl);
        return 0;
    }
    bool ran = false;
    int rc = bigram_spectrum_maybe_compute(bs, min_delta_events, &ran);
    gl_tc12_unlock(gl);
    if (rc < 0) return -1;
    return ran ? 1 : 0;
}

uint64_t grounded_language_bigram_spectrum_map_overflow_warns(void) {
    /* TC-12: per-gl side-maps cannot overflow (the spectrum is stored
     * directly on the gl), so this counter is permanently zero now.
     * Kept for ABI continuity with pre-TC-12 callers. */
    return 0;
}

/*=============================================================================
 * Tier-1 #2: Rule-based anaphora / pronoun resolution (v1).
 *
 * Side-map keyed by gl pointer (same pattern as g_spectrum_map) so we
 * don't perturb the grounded_language struct layout — the persistence
 * sidecar walks that struct field-by-field via the internal header and
 * any layout drift breaks checkpoint compatibility.
 *
 * Each entry holds an 8-deep referent ring + the toggle. The pronoun
 * table itself is process-global static const data. Successful resolves
 * bump g_anaphora_resolutions atomically.
 *===========================================================================*/

/* TC-12: GL_ANAPHORA_MAP_CAP retired — anaphora state now lives directly
 * on each grounded_language_t, no fixed cap. */
#define GL_ANAPHORA_RING_CAP       8

typedef enum {
    GL_GENDER_UNKNOWN = 0,
    GL_GENDER_SINGULAR_MALE,
    GL_GENDER_SINGULAR_FEMALE,
    GL_GENDER_SINGULAR_NEUTER,
    GL_GENDER_PLURAL,
} gl_anaphora_gender_t;

typedef struct {
    char                   form[GL_MAX_WORD_LEN];   /* lowercased */
    gl_anaphora_gender_t   gender;
    uint64_t               timestamp_us;
} gl_anaphora_referent_t;

typedef struct {
    bool                   enabled;
    gl_anaphora_referent_t ring[GL_ANAPHORA_RING_CAP];
    uint32_t               head;   /* next slot to write — push location */
    uint32_t               size;   /* number of valid entries (≤ cap) */
} gl_anaphora_state_t;

/* Pronoun table — small enough that linear scan beats any hash. */
typedef struct {
    const char*          word;
    gl_anaphora_gender_t gender;
} gl_anaphora_pronoun_entry_t;

static const gl_anaphora_pronoun_entry_t g_pronouns[] = {
    { "he",     GL_GENDER_SINGULAR_MALE   },
    { "him",    GL_GENDER_SINGULAR_MALE   },
    { "his",    GL_GENDER_SINGULAR_MALE   },
    { "she",    GL_GENDER_SINGULAR_FEMALE },
    { "her",    GL_GENDER_SINGULAR_FEMALE },
    { "hers",   GL_GENDER_SINGULAR_FEMALE },
    { "it",     GL_GENDER_SINGULAR_NEUTER },
    { "this",   GL_GENDER_SINGULAR_NEUTER },
    { "that",   GL_GENDER_SINGULAR_NEUTER },
    { "they",   GL_GENDER_PLURAL          },
    { "them",   GL_GENDER_PLURAL          },
    { "their",  GL_GENDER_PLURAL          },
    { "theirs", GL_GENDER_PLURAL          },
    { "these",  GL_GENDER_PLURAL          },
    { "those",  GL_GENDER_PLURAL          },
};
#define GL_PRONOUN_COUNT (sizeof(g_pronouns) / sizeof(g_pronouns[0]))

/* TC-12: process-global resolution counter — kept as an atomic-style
 * uint64 protected by g_tc12_bootstrap. Public API
 * grounded_language_anaphora_resolutions() is process-wide (no gl arg)
 * so the counter must remain global. */
static uint64_t g_anaphora_resolutions = 0;

/* Per-gl helpers. The gl's tc12_lock now protects the anaphora_state
 * pointer + its ring contents. Anaphora state is allocated lazily on
 * first set_anaphora_enabled call and freed in destroy. */

static gl_anaphora_state_t* anaphora_lookup_locked(grounded_language_t* gl) {
    return (gl_anaphora_state_t*)gl->anaphora_state;
}

static gl_anaphora_state_t* anaphora_get_or_create_locked(grounded_language_t* gl) {
    gl_anaphora_state_t* st = (gl_anaphora_state_t*)gl->anaphora_state;
    if (st) return st;
    st = (gl_anaphora_state_t*)nimcp_calloc(1, sizeof(*st));
    if (!st) return NULL;
    gl->anaphora_state = st;
    return st;
}

static void gl_detach_anaphora_for_destroy(grounded_language_t* gl) {
    if (!gl) return;
    gl_tc12_lock(gl);
    if (gl->anaphora_state) {
        nimcp_free(gl->anaphora_state);
        gl->anaphora_state = NULL;
    }
    /* NLP-2: free coref ring on the same teardown path. */
    if (gl->coref_state) {
        nimcp_free(gl->coref_state);
        gl->coref_state = NULL;
    }
    gl_tc12_unlock(gl);
}

bool grounded_language_set_anaphora_enabled(grounded_language_t* gl,
                                              bool enabled) {
    if (!gl) return false;
    gl_tc12_lock(gl);
    gl_anaphora_state_t* st = anaphora_get_or_create_locked(gl);
    if (!st) {
        gl_tc12_unlock(gl);
        return false;
    }
    st->enabled = enabled;
    if (!enabled) {
        st->head = 0;
        st->size = 0;
        memset(st->ring, 0, sizeof(st->ring));
    }
    gl_tc12_unlock(gl);
    return true;
}

bool grounded_language_get_anaphora_enabled(const grounded_language_t* gl) {
    /* Audit-2 follow-up: lang_status surface needs a read side for the
     * anaphora flag (the state struct is lazily-allocated so a missing
     * struct == disabled). Cast away const because gl_tc12_lock is on
     * the non-const path; the lock window is read-only. */
    if (!gl) return false;
    grounded_language_t* gl_mut = (grounded_language_t*)gl;
    gl_tc12_lock(gl_mut);
    const gl_anaphora_state_t* st = (const gl_anaphora_state_t*)gl->anaphora_state;
    bool on = (st != NULL) && st->enabled;
    gl_tc12_unlock(gl_mut);
    return on;
}

/*=============================================================================
 * NLP-2: Coreference beyond pronouns (same-surface, ring-based).
 *===========================================================================*/

#define GL_COREF_RING_CAP    16   /* recent definite-NP heads tracked */
#define GL_COREF_MAX_TURNS    6   /* coref window in turns */

typedef struct {
    char     form[GL_MAX_WORD_LEN];   /* lowercased noun head */
    uint32_t turn_idx;                 /* discourse turn at mention time */
} gl_coref_entry_t;

typedef struct {
    gl_coref_entry_t ring[GL_COREF_RING_CAP];
    uint32_t         head;
    uint32_t         size;
} gl_coref_state_t;

/* Caller must hold tc12_lock. */
static gl_coref_state_t* gl_coref_get_or_create(grounded_language_t* gl) {
    if (!gl) return NULL;
    if (gl->coref_state) return (gl_coref_state_t*)gl->coref_state;
    gl_coref_state_t* st = (gl_coref_state_t*)nimcp_calloc(1, sizeof(*st));
    if (!st) return NULL;
    gl->coref_state = st;
    return st;
}

void grounded_language_set_coref_resolution_enabled(grounded_language_t* gl,
                                                       bool enabled) {
    if (!gl) return;
    gl->enable_coref_resolution = enabled;
    if (!enabled && gl->coref_state) {
        /* Clear ring on disable so re-enable starts fresh. */
        gl_tc12_lock(gl);
        gl_coref_state_t* st = (gl_coref_state_t*)gl->coref_state;
        if (st) { st->head = 0; st->size = 0; memset(st->ring, 0, sizeof(st->ring)); }
        gl_tc12_unlock(gl);
    }
}

bool grounded_language_get_coref_resolution_enabled(
    const grounded_language_t* gl) {
    return gl ? gl->enable_coref_resolution : false;
}

uint64_t grounded_language_coref_attempts(const grounded_language_t* gl) {
    return gl ? gl->coref_attempts : 0;
}
uint64_t grounded_language_coref_resolved(const grounded_language_t* gl) {
    return gl ? gl->coref_resolved : 0;
}

/* Push a definite-NP head onto the ring + scan for a recent same-form
 * match within GL_COREF_MAX_TURNS. Returns 1 if a coref hit was
 * recorded (and bumps coref_resolved), 0 otherwise. Bumps coref_attempts
 * unconditionally. Caller must hold tc12_lock. */
static int gl_coref_track_locked(grounded_language_t* gl, const char* head,
                                   uint32_t turn_idx) {
    if (!gl || !head || !head[0]) return 0;
    gl_coref_state_t* st = gl_coref_get_or_create(gl);
    if (!st) return 0;
    gl->coref_attempts++;

    /* Scan ring for same form within window. */
    int matched = 0;
    for (uint32_t i = 0; i < st->size; i++) {
        if (strncmp(st->ring[i].form, head, GL_MAX_WORD_LEN - 1) == 0) {
            uint32_t dist = (turn_idx > st->ring[i].turn_idx) ?
                            (turn_idx - st->ring[i].turn_idx) : 0;
            if (dist > 0 && dist <= GL_COREF_MAX_TURNS) {
                matched = 1;
                break;
            }
        }
    }

    /* Always push the new mention (even without a match — future calls
     * may match it). */
    uint32_t slot = st->head;
    strncpy(st->ring[slot].form, head, GL_MAX_WORD_LEN - 1);
    st->ring[slot].form[GL_MAX_WORD_LEN - 1] = '\0';
    st->ring[slot].turn_idx = turn_idx;
    st->head = (st->head + 1) % GL_COREF_RING_CAP;
    if (st->size < GL_COREF_RING_CAP) st->size++;

    if (matched) gl->coref_resolved++;
    return matched;
}

/*=============================================================================
 * NLP-1: Subword OOV / morphological fallback.
 *===========================================================================*/

void grounded_language_attach_subword_tokenizer(grounded_language_t* gl,
                                                  struct nimcp_tokenizer* tok) {
    if (!gl) return;
    /* Borrowed pointer — caller owns the lifetime. NULL detaches. */
    gl->subword_tokenizer = tok;
}

void grounded_language_set_subword_oov_fallback_enabled(grounded_language_t* gl,
                                                          bool enabled) {
    if (!gl) return;
    gl->enable_subword_oov_fallback = enabled;
}

bool grounded_language_get_subword_oov_fallback_enabled(
    const grounded_language_t* gl) {
    return gl ? gl->enable_subword_oov_fallback : false;
}

uint64_t grounded_language_subword_oov_attempts(const grounded_language_t* gl) {
    return gl ? gl->subword_oov_attempts : 0;
}
uint64_t grounded_language_subword_oov_resolved(const grounded_language_t* gl) {
    return gl ? gl->subword_oov_resolved : 0;
}

/* NLP-1 internal helper. Called from comprehend's word-loop when a
 * surface form has no existing lexicon entry. Creates a fresh entry,
 * segments via the attached tokenizer, and bootstraps bindings by
 * averaging from subword pieces already in the lexicon (at half
 * strength — they're inherited, not earned).
 *
 * Returns the bootstrapped entry on success, or NULL when the
 * fallback couldn't help (flag off, no tokenizer, single-token
 * encoding, no binding-bearing subwords). When NULL, caller falls
 * through to the existing subword-blend-only path (which doesn't
 * create an entry). */
/* Non-static so the comprehend body can forward-decl + call (the impl
 * lives well below its caller's scope). */
gl_lexicon_entry_t* gl_try_subword_bootstrap(grounded_language_t* gl,
                                                const char* word) {
    if (!gl || !word || !word[0]) return NULL;
    if (!gl->enable_subword_oov_fallback) return NULL;
    if (!gl->subword_tokenizer) return NULL;

    /* Forward decls — keep this file independent of the tokenizer impl. */
    extern int nimcp_tokenizer_encode(const struct nimcp_tokenizer*,
                                        const char*, uint32_t*, uint32_t);
    extern const char* nimcp_tokenizer_id_to_text(
        const struct nimcp_tokenizer*, uint32_t);

    gl->subword_oov_attempts++;

    uint32_t ids[16] = {0};
    int n = nimcp_tokenizer_encode(
        (const struct nimcp_tokenizer*)gl->subword_tokenizer,
        word, ids, 16);
    if (n <= 1) {
        /* 0 = encode failed; 1 = the surface is itself a single token,
         * so segmentation gave us no new information. */
        return NULL;
    }

    /* Probe subword pieces FIRST without mutating the lexicon —
     * abort the bootstrap if no piece has bindings, so we don't pollute
     * the lexicon with empty fresh entries. */
    bool any_useful = false;
    for (int i = 0; i < n; i++) {
        const char* piece = nimcp_tokenizer_id_to_text(
            (const struct nimcp_tokenizer*)gl->subword_tokenizer, ids[i]);
        if (!piece || !piece[0]) continue;
        const gl_lexicon_entry_t* sub = grounded_language_lookup(gl, piece);
        if (sub && sub->binding_count > 0) { any_useful = true; break; }
    }
    if (!any_useful) return NULL;

    /* Now create the entry and fill it. */
    gl_lexicon_entry_t* fresh = gl_internal_lexicon_find_or_create(gl, word);
    if (!fresh) return NULL;
    /* If somehow the entry already had bindings (race / re-entry), don't
     * stomp them. */
    if (fresh->binding_count > 0) return fresh;

    /* Bindings array is dynamically allocated; grow capacity if we need
     * more slots than the initial 4. */
    uint32_t collected = 0;
    for (int i = 0; i < n; i++) {
        const char* piece = nimcp_tokenizer_id_to_text(
            (const struct nimcp_tokenizer*)gl->subword_tokenizer, ids[i]);
        if (!piece || !piece[0]) continue;
        const gl_lexicon_entry_t* sub = grounded_language_lookup(gl, piece);
        if (!sub || sub->binding_count == 0) continue;
        for (uint32_t b = 0; b < sub->binding_count; b++) {
            if (collected >= fresh->binding_capacity) {
                uint32_t new_cap = fresh->binding_capacity * 2;
                if (new_cap < 8) new_cap = 8;
                gl_word_binding_t* nb = (gl_word_binding_t*)nimcp_realloc(
                    fresh->bindings, new_cap * sizeof(gl_word_binding_t));
                if (!nb) goto _bootstrap_done;
                memset(nb + fresh->binding_capacity, 0,
                       (new_cap - fresh->binding_capacity) * sizeof(gl_word_binding_t));
                fresh->bindings = nb;
                fresh->binding_capacity = new_cap;
            }
            fresh->bindings[collected].concept_id =
                sub->bindings[b].concept_id;
            fresh->bindings[collected].strength =
                sub->bindings[b].strength * 0.5f;
            collected++;
        }
    }
_bootstrap_done:
    if (collected == 0) return NULL;
    fresh->binding_count = collected;
    gl->subword_oov_resolved++;
    return fresh;
}

uint64_t grounded_language_anaphora_resolutions(void) {
    /* Audit fix: lock-free atomic read; writers use __atomic_fetch_add
     * (see anaphora_run_pass below). Was previously bumped under the
     * per-gl tc12 lock and read under the bootstrap mutex — but two gls
     * resolving anaphora concurrently each held DIFFERENT per-gl locks
     * so the increments could race. Switching to __atomic_* gives a
     * proper SMP-safe counter without serializing readers. */
    return __atomic_load_n(&g_anaphora_resolutions, __ATOMIC_RELAXED);
}

/* Pronoun lookup — returns gender, or GL_GENDER_UNKNOWN if word is not a
 * pronoun. Caller must already have a lowercased form. */
static gl_anaphora_gender_t anaphora_pronoun_gender(const char* lower_word) {
    if (!lower_word || !lower_word[0]) return GL_GENDER_UNKNOWN;
    for (size_t i = 0; i < GL_PRONOUN_COUNT; i++) {
        if (strcmp(g_pronouns[i].word, lower_word) == 0) {
            return g_pronouns[i].gender;
        }
    }
    return GL_GENDER_UNKNOWN;
}

/* Gender heuristic for content nouns. Pure recency-based; no syntactic
 * analysis.
 *   - The caller (`comprehend`) lowercases tokens before invoking this
 *     pass, so capitalization-based proper-noun detection is impossible
 *     here — that signal was lost upstream. Earlier versions had an
 *     `isupper(first)` branch that returned SINGULAR_MALE; it never
 *     fired and has been removed (INT-1 walkthrough finding).
 *   - Trailing 's' (length ≥ 4 to skip "is"/"as", and != "ss" to skip
 *     boss/glass) → plural.
 *   - Otherwise → singular_neuter.
 * SINGULAR_FEMALE / SINGULAR_MALE are signalled by gendered pronoun
 * cues ("she/her/hers" or "he/him/his") propagated forward by the
 * caller via `pending_female` / `pending_male`. So "He is Tom" and
 * "She is Alice" both work; bare "Tom" / "Alice" mid-sentence with no
 * cue stay neuter (v1 caveat — fixable later with a name lexicon).
 */
static gl_anaphora_gender_t anaphora_classify_noun(const char* surface_form) {
    if (!surface_form || !surface_form[0]) return GL_GENDER_SINGULAR_NEUTER;
    size_t len = strlen(surface_form);

    /* Plural detection by trailing 's' (length ≥ 4 avoids 1-letter and
     * common short stop-words like "is"/"as"). Also avoid "ss" doubling
     * (boss/glass → not plural). */
    if (len >= 4 && surface_form[len-1] == 's' && surface_form[len-2] != 's') {
        return GL_GENDER_PLURAL;
    }

    return GL_GENDER_SINGULAR_NEUTER;
}

/* Push a referent onto the ring (caller already holds the map lock). */
static void anaphora_push_referent(gl_anaphora_state_t* st,
                                     const char* lower_form,
                                     gl_anaphora_gender_t gender) {
    if (!st || !lower_form || !lower_form[0]) return;
    if (gender == GL_GENDER_UNKNOWN) gender = GL_GENDER_SINGULAR_NEUTER;

    gl_anaphora_referent_t* slot = &st->ring[st->head];
    memset(slot, 0, sizeof(*slot));
    size_t cap = sizeof(slot->form) - 1;
    strncpy(slot->form, lower_form, cap);
    slot->form[cap] = '\0';
    slot->gender = gender;
    /* Microsecond-ish timestamp — we only need monotonicity for tie-break
     * at backwards scan, and the ring index already encodes that, so
     * this is essentially decorative. */
    slot->timestamp_us = (uint64_t)time(NULL) * 1000000ull;

    st->head = (st->head + 1) % GL_ANAPHORA_RING_CAP;
    if (st->size < GL_ANAPHORA_RING_CAP) st->size++;
}

/* Resolve a pronoun by walking the ring backwards from most-recent.
 * Returns the form of the matched referent (pointing into the ring,
 * valid only while the lock is held — caller copies it out). NULL if
 * no gender match is found. */
static const char* anaphora_resolve_locked(gl_anaphora_state_t* st,
                                             gl_anaphora_gender_t pronoun_gender) {
    if (!st || st->size == 0) return NULL;
    /* head points to NEXT write slot. Most-recent is head-1 (mod cap). */
    for (uint32_t step = 0; step < st->size; step++) {
        uint32_t idx = (st->head + GL_ANAPHORA_RING_CAP - 1 - step)
                        % GL_ANAPHORA_RING_CAP;
        if (st->ring[idx].gender == pronoun_gender) {
            return st->ring[idx].form;
        }
    }
    return NULL;
}

/* The hook called from comprehend. Walks the token list, classifies each
 * token as pronoun or content noun, pushes referents, and (when a
 * pronoun resolves) folds the referent's lexicon-entry bindings into the
 * caller's activated_concepts/activation_levels arrays at half-strength.
 *
 * Returns the number of successful resolutions performed in this call.
 * No-op (and returns 0) when the resolver is disabled or unallocated.
 *
 * NOTE: caller passes the in/out concept_count by pointer. If we resolve
 * a pronoun but the activated_concepts array is full, we silently skip
 * the binding-fold for that pronoun (counter is still bumped — the
 * resolution itself succeeded; only the side-effect of activating
 * concepts was capped).
 */
static uint32_t anaphora_run_pass(grounded_language_t* gl,
                                    char** words,
                                    uint32_t word_count,
                                    uint64_t* activated_concepts,
                                    float* activation_levels,
                                    uint32_t* concept_count_io) {
    if (!gl || !words || word_count == 0) return 0;

    /* Lock-and-check enabled; if disabled, fast-path return without
     * touching the ring. */
    gl_tc12_lock(gl);
    gl_anaphora_state_t* st = anaphora_lookup_locked(gl);
    if (!st || !st->enabled) {
        gl_tc12_unlock(gl);
        return 0;
    }

    uint32_t resolved_here = 0;
    bool pending_female = false;  /* "she/her/hers" cue → next content noun is female */
    bool pending_male   = false;  /* "he/him/his"  cue → next content noun is male
                                   * (parallel to female cue — INT-1 fix; replaces
                                   *  dead capitalization-based MALE classification) */

    for (uint32_t w = 0; w < word_count; w++) {
        const char* surface = words[w];
        if (!surface || !surface[0]) continue;

        char lower[GL_MAX_WORD_LEN];
        lowercase_word(surface, lower, GL_MAX_WORD_LEN);

        gl_anaphora_gender_t pron = anaphora_pronoun_gender(lower);
        if (pron != GL_GENDER_UNKNOWN) {
            /* Pronoun. Try to resolve. */
            const char* matched_form = anaphora_resolve_locked(st, pron);
            if (matched_form) {
                /* Copy form before unlocking the map — though we hold the
                 * lock through the whole pass, so this is just defensive. */
                char form_copy[GL_MAX_WORD_LEN];
                size_t cap = sizeof(form_copy) - 1;
                strncpy(form_copy, matched_form, cap);
                form_copy[cap] = '\0';

                resolved_here++;
                /* Audit fix: lock-free atomic increment — writers across
                 * gls hold different per-gl tc12 locks, so non-atomic
                 * increments here could race. */
                __atomic_fetch_add(&g_anaphora_resolutions, 1,
                                     __ATOMIC_RELAXED);

                /* Fold the referent's lexicon-entry bindings (at half
                 * strength) into the caller's activated_concepts list.
                 * lexicon_find takes a const gl* and is read-only — safe
                 * to call while holding our lock (no overlap with the
                 * gl's own internal state). */
                if (activated_concepts && activation_levels && concept_count_io) {
                    const gl_lexicon_entry_t* ref_entry = lexicon_find(gl, form_copy);
                    if (ref_entry) {
                        for (uint32_t b = 0;
                             b < ref_entry->binding_count &&
                             *concept_count_io < GL_MAX_ACTIVE_CONCEPTS;
                             b++) {
                            const gl_word_binding_t* bd = &ref_entry->bindings[b];
                            if (bd->strength < GL_ASSOC_PRUNE_THRESHOLD) continue;

                            float weight = 0.5f * bd->strength;

                            /* Merge with existing activation if present. */
                            bool merged = false;
                            for (uint32_t c = 0; c < *concept_count_io; c++) {
                                if (activated_concepts[c] == bd->concept_id) {
                                    activation_levels[c] += weight;
                                    merged = true;
                                    break;
                                }
                            }
                            if (!merged) {
                                activated_concepts[*concept_count_io]  = bd->concept_id;
                                activation_levels[*concept_count_io]   = weight;
                                (*concept_count_io)++;
                            }
                        }
                    }
                }
            }
            /* Cue-update: a literal "she" / "her" / "hers" suggests the
             * next content noun (a NAME) refers to a female entity. We
             * propagate pending_female forward so something like
             * "She is Alice" makes Alice female on push. The symmetric
             * pending_male path covers "He is Tom" — added in the INT-1
             * fix when we removed the dead capitalization branch. */
            if (pron == GL_GENDER_SINGULAR_FEMALE) pending_female = true;
            if (pron == GL_GENDER_SINGULAR_MALE)   pending_male   = true;
            continue;
        }

        /* Not a pronoun — candidate referent.
         * Skip lexicon function words (the/a/an/of/and/...) so they
         * don't displace real nouns from the ring. The lexicon's
         * learned_class is a soft hint; if the entry doesn't exist
         * we still push the surface form (any unknown noun is more
         * useful than nothing for a recency-based v1).
         */
        const gl_lexicon_entry_t* ent = lexicon_find(gl, lower);
        if (ent && ent->learned_class == GL_CLASS_FUNCTION) continue;

        gl_anaphora_gender_t g = anaphora_classify_noun(surface);
        if (pending_female) {
            g = GL_GENDER_SINGULAR_FEMALE;
            pending_female = false;
            pending_male   = false;  /* female cue wins if both somehow set */
        } else if (pending_male) {
            g = GL_GENDER_SINGULAR_MALE;
            pending_male = false;
        }
        anaphora_push_referent(st, lower, g);
    }

    gl_tc12_unlock(gl);
    return resolved_here;
}

int grounded_language_learn_text_bigrams(grounded_language_t* gl,
                                           const char* text, float lr)
{
    if (!gl || !text) return -1;
    if (!gl->snn_bridge) return 0;  /* no bridge → no-op (count 0) */
    if (!isfinite(lr) || lr <= 0.0f) return -1;

    size_t L = strlen(text);
    char* buf = (char*)nimcp_malloc(L + 1);
    if (!buf) return -1;
    memcpy(buf, text, L + 1);

    char* words[GL_MAX_PRODUCTION_WORDS];
    uint32_t n = tokenize_text(buf, words, GL_MAX_PRODUCTION_WORDS);

    /* Audit fix (TOCTOU): hold the per-gl lock across the entire
     * bigram-learn loop so a concurrent attach(NULL) cannot detach the
     * spectrum mid-iteration. All bigram_spectrum_record calls below
     * are O(1) so the loop is short; per-gl scope means other gls are
     * not blocked. */
    gl_tc12_lock(gl);
    bigram_spectrum_t* spectrum = (bigram_spectrum_t*)gl->bigram_spectrum;
    uint32_t spec_cap = spectrum ? bigram_spectrum_vocab_cap(spectrum) : 0u;

    /* TA-4: snapshot the trigram-learning flag once per call so a
     * concurrent toggle can't make us emit a half-bigram-half-trigram
     * pass. Default OFF preserves PA-4 bit-for-bit. */
    const bool do_trigrams =
        snn_language_bridge_get_trigram_learning_enabled(gl->snn_bridge);
    /* Half lr on trigram pass to avoid trigram dominance — matches the
     * default ratio recommended in the TA-4 brief. */
    const float trigram_lr = lr * 0.5f;

    int processed = 0;
    for (uint32_t i = 0; i + 1 < n; i++) {
        if (grounded_language_learn_next_token_pair(gl, words[i],
                                                      words[i + 1], lr) == 0) {
            processed++;

            /* PA-4+ : record the bigram into the attached spectrum.
             * Use the lexicon form_hash modded down to spectrum's vocab
             * cap. Out-of-range ids are silently ignored by record(). */
            if (spectrum && spec_cap > 0) {
                gl_lexicon_entry_t* pe =
                    gl_internal_lexicon_find_or_create(gl, words[i]);
                gl_lexicon_entry_t* ne =
                    gl_internal_lexicon_find_or_create(gl, words[i + 1]);
                if (pe && ne) {
                    uint32_t pid = pe->form_hash % spec_cap;
                    uint32_t nid = ne->form_hash % spec_cap;
                    bigram_spectrum_record(spectrum, pid, nid);
                }
            }
        }

        /* TA-4: also walk trigrams (a, b) → c when enabled. Half lr so
         * the bigram update remains the dominant signal. Cold-start
         * (any of the three tokens un-encoded) cleanly returns -1 from
         * the triple call without applying. The trigram counter on the
         * bridge is bumped from inside the triple call. */
        if (do_trigrams && i + 2 < n) {
            (void)grounded_language_learn_next_token_triple(gl,
                                                              words[i],
                                                              words[i + 1],
                                                              words[i + 2],
                                                              trigram_lr);
        }
    }

    /* Audit fix: release per-gl lock after the loop. */
    gl_tc12_unlock(gl);

    nimcp_free(buf);
    return processed;
}

/*=============================================================================
 * #14 Dialect / accent conditioning
 *===========================================================================*/

void grounded_language_set_dialect(grounded_language_t* gl, const char* dialect) {
    if (!gl) return;
    if (!dialect || !dialect[0]) {
        gl->context_dialect[0] = '\0';
        return;
    }
    /* Truncating copy + guaranteed NUL. strncpy doesn't NUL-terminate
     * when src ≥ dst; do it explicitly. */
    size_t n = strlen(dialect);
    if (n >= GL_MAX_DIALECT_LEN) n = GL_MAX_DIALECT_LEN - 1;
    memcpy(gl->context_dialect, dialect, n);
    gl->context_dialect[n] = '\0';
}

const char* grounded_language_get_dialect(const grounded_language_t* gl) {
    if (!gl) return "";
    return gl->context_dialect;
}

/*=============================================================================
 * Query / Introspection
 *===========================================================================*/

const gl_lexicon_entry_t* grounded_language_lookup(const grounded_language_t* gl,
                                                     const char* word) {
    if (!gl || !word) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_lookup: NULL parameter");
        return NULL;
    }
    return lexicon_find(gl, word);
}

uint32_t grounded_language_words_for_concept(const grounded_language_t* gl,
                                              uint64_t concept_id,
                                              const char** words,
                                              uint32_t max_words) {
    if (!gl || !words || max_words == 0) return 0;

    uint32_t count = 0;
    for (uint32_t w = 0; w < gl->vocab_count && count < max_words; w++) {
        const gl_lexicon_entry_t* entry = gl->vocab_list[w];
        if (!entry) continue;

        for (uint32_t b = 0; b < entry->binding_count; b++) {
            if (entry->bindings[b].concept_id == concept_id &&
                entry->bindings[b].strength > GL_ASSOC_PRUNE_THRESHOLD) {
                words[count++] = entry->form;
                break;
            }
        }
    }

    return count;
}

void grounded_language_get_stats(const grounded_language_t* gl, gl_stats_t* stats) {
    if (!gl || !stats) return;
    *stats = gl->stats;

    /* Compute average binding strength */
    float total_strength = 0.0f;
    uint32_t total_bindings = 0;
    for (uint32_t w = 0; w < gl->vocab_count; w++) {
        const gl_lexicon_entry_t* entry = gl->vocab_list[w];
        if (!entry) continue;
        for (uint32_t b = 0; b < entry->binding_count; b++) {
            total_strength += entry->bindings[b].strength;
            total_bindings++;
        }
    }
    stats->avg_binding_strength = (total_bindings > 0) ?
        total_strength / (float)total_bindings : 0.0f;

    /* Forgetting telemetry (#15) — sum the 24-hour ring + cumulative. */
    uint32_t sum24 = 0;
    for (int i = 0; i < 24; i++) sum24 += gl->decayed_ring[i];
    stats->entries_decayed_last_24h = sum24;
    stats->entries_decayed_all_time = gl->decayed_all_time;

    /* TA-2 — mirror the bridge's lgss_outputs_blocked counter into the
     * gl-level stats struct so callers polling get_stats see a unified
     * view of "how often did LGSS deny something we tried to comprehend
     * or produce". The actual write-side counter still lives on the
     * bridge (the produce gate lives there too); this is read-mirror only.
     * Stays 0 when no SNN bridge is attached. */
    if (gl->snn_bridge) {
        snn_lang_stats_t bridge_stats;
        memset(&bridge_stats, 0, sizeof(bridge_stats));
        if (snn_language_bridge_get_stats(
                (const snn_language_bridge_t*)gl->snn_bridge,
                &bridge_stats) == 0) {
            stats->lgss_outputs_blocked = bridge_stats.lgss_outputs_blocked;
        }
    }
}

uint32_t grounded_language_get_semantic_dim(const grounded_language_t* gl) {
    return gl ? gl->semantic_dim : 0u;
}

int grounded_language_get_probe_metrics(const grounded_language_t* gl,
                                          gl_probe_metrics_t* out) {
    if (!gl || !out) return -1;
    memset(out, 0, sizeof(*out));

    /* Lexicon gauges — share the get_stats walk for binding averages. */
    float total_strength = 0.0f;
    float total_confidence = 0.0f;
    uint32_t total_bindings = 0;
    for (uint32_t w = 0; w < gl->vocab_count; w++) {
        const gl_lexicon_entry_t* entry = gl->vocab_list[w];
        if (!entry) continue;
        for (uint32_t b = 0; b < entry->binding_count; b++) {
            total_strength   += entry->bindings[b].strength;
            total_confidence += entry->bindings[b].confidence;
            total_bindings++;
        }
    }
    out->vocab_count          = gl->vocab_count;
    out->templates_count      = 0;  /* templates removed — emergent grammar via SNN bridge */
    out->avg_binding_strength = (total_bindings > 0)
        ? total_strength / (float)total_bindings : 0.0f;
    out->avg_binding_confidence = (total_bindings > 0)
        ? total_confidence / (float)total_bindings : 0.0f;

    /* Bus state. */
    out->subscriber_count = gl->subscriber_count;
    for (uint32_t i = 0; i < gl->subscriber_count; i++) {
        if (gl->subscriber_priorities[i] > 0)
            out->subscriber_high_priority++;
        if (gl->subscriber_masks[i] != GL_EVENT_MASK_ALL)
            out->subscriber_filtered++;
    }
    out->events_dropped_reentry = gl->events_dropped_reentry;
    out->in_fire_event          = gl->in_fire_event;

    /* Forgetting curve. */
    uint32_t sum24 = 0;
    for (int i = 0; i < 24; i++) sum24 += gl->decayed_ring[i];
    out->entries_decayed_last_24h = sum24;
    out->entries_decayed_all_time = gl->decayed_all_time;

    /* Network bridge magnitudes (last broadcast). */
    out->last_lnn_response_mag = gl->last_lnn_mag;
    out->last_cnn_response_mag = gl->last_cnn_mag;
    out->last_fno_response_mag = gl->last_fno_mag;
    out->last_ann_response_mag = gl->last_ann_mag;

    /* Throughput counters from gl_stats_t. */
    out->total_groundings      = gl->stats.total_groundings;
    out->total_comprehensions  = gl->stats.total_comprehensions;
    out->total_productions     = gl->stats.total_productions;
    out->total_new_words       = gl->stats.vocab_size;

    /* Region wiring health (booleans for each connect_*). */
    out->broca_attached          = (gl->broca_adapter != NULL);
    out->wernicke_attached       = (gl->wernicke_adapter != NULL);
    out->working_memory_attached = (gl->working_memory != NULL);
    out->hippocampus_attached    = (gl->hippocampus != NULL);
    out->embedding_attached      = (gl->embeddings != NULL);
    out->tokenizer_attached      = (gl->tokenizer != NULL);

    /* Cortex modulation taps — read live from connected cortexes. */
    gl_cortex_modulation_t mod;
    if (grounded_language_get_cortex_modulation(
            (grounded_language_t*)gl, &mod) == 0) {
        out->visual_activity    = mod.visual_activity;
        out->audio_salience     = mod.audio_salience;
        out->speech_confidence  = mod.speech_confidence;
    }

    /* Dialect tag (#14) — copy with NUL guarantee. */
    size_t dlen = 0;
    while (dlen < GL_MAX_DIALECT_LEN - 1 && gl->context_dialect[dlen] != '\0') dlen++;
    memcpy(out->context_dialect, gl->context_dialect, dlen);
    out->context_dialect[dlen] = '\0';

    /* Negative-grounding + active-learning counters. */
    out->total_negative_groundings = gl->stats.total_negative_groundings;
    out->total_needs_grounding     = gl->stats.total_needs_grounding;
    return 0;
}

/*=============================================================================
 * Serialization (stub — full implementation deferred)
 *===========================================================================*/

int grounded_language_save(const grounded_language_t* gl, const char* path) {
    if (!gl || !path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_save: NULL parameter");
        return -1;
    }

    FILE* f = fopen(path, "wb");
    if (!f) return -1;

    /* Magic + version. v2 adds context_dialect (#14). v3 adds
     * compositional phrases (#9). v4 (#6) compresses each entry's
     * context_vector to int8 with per-vector max-abs scaling — 4×
     * size reduction at <0.4% per-element error. v5 removes the
     * syntactic-template block; grammar is now learned emergently via
     * SNN bridge plasticity. v5 readers skip the legacy template
     * section when loading v4-and-earlier files. */
    uint32_t magic = GL_MAGIC;
    uint32_t version = 5;
    fwrite(&magic, sizeof(magic), 1, f);
    fwrite(&version, sizeof(version), 1, f);
    fwrite(&gl->semantic_dim, sizeof(gl->semantic_dim), 1, f);
    fwrite(&gl->vocab_count, sizeof(gl->vocab_count), 1, f);

    /* Write each word entry */
    for (uint32_t w = 0; w < gl->vocab_count; w++) {
        const gl_lexicon_entry_t* entry = gl->vocab_list[w];
        if (!entry) continue;

        /* Word form */
        fwrite(entry->form, GL_MAX_WORD_LEN, 1, f);
        fwrite(&entry->frequency, sizeof(entry->frequency), 1, f);
        fwrite(&entry->learned_class, sizeof(entry->learned_class), 1, f);
        fwrite(&entry->valence, sizeof(entry->valence), 1, f);
        fwrite(&entry->arousal, sizeof(entry->arousal), 1, f);

        /* Context vector — v4 int8 quantization (#6): per-vector
         * max-abs as float, then int8 codes. Decoded as
         *   x = (q / 127.0) × max_abs.
         * Empty/zero vectors store max_abs=0 and skip the int8 block.
         * v3 and earlier wrote a float block — kept for read parity. */
        fwrite(&entry->context_initialized, sizeof(entry->context_initialized), 1, f);
        if (entry->context_initialized) {
            float max_abs = 0.0f;
            for (uint32_t d = 0; d < gl->semantic_dim; d++) {
                float a = fabsf(entry->context_vector[d]);
                if (a > max_abs) max_abs = a;
            }
            fwrite(&max_abs, sizeof(float), 1, f);
            if (max_abs > 0.0f) {
                /* Quantize to int8. Heap-allocate so we never silently
                 * zero out user data when semantic_dim is bumped — the
                 * 4096-cap from load() bounds peak allocation here. */
                int8_t* qbuf = (int8_t*)nimcp_malloc(gl->semantic_dim);
                if (!qbuf) {
                    fclose(f);
                    return -1;
                }
                float inv = 127.0f / max_abs;
                for (uint32_t d = 0; d < gl->semantic_dim; d++) {
                    float v = entry->context_vector[d] * inv;
                    if (v >  127.0f) v =  127.0f;
                    if (v < -127.0f) v = -127.0f;
                    qbuf[d] = (int8_t)(v >= 0.0f ? v + 0.5f : v - 0.5f);
                }
                fwrite(qbuf, sizeof(int8_t), gl->semantic_dim, f);
                nimcp_free(qbuf);
            }
        }

        /* Bindings */
        fwrite(&entry->binding_count, sizeof(entry->binding_count), 1, f);
        for (uint32_t b = 0; b < entry->binding_count; b++) {
            fwrite(&entry->bindings[b], sizeof(gl_word_binding_t), 1, f);
        }
    }

    /* v5: no template block — emergent grammar via SNN bridge. */

    /* v2: dialect tag (#14). Always exactly GL_MAX_DIALECT_LEN bytes
     * for a fixed-width record so future fields can be appended without
     * a length prefix. */
    fwrite(gl->context_dialect, GL_MAX_DIALECT_LEN, 1, f);

    /* v3: compositional phrases (#9). count + per-phrase fixed record:
     *   form[GL_MAX_PHRASE_LEN] | component_words(u8) | frequency(u32)
     * semantic_vec is NOT persisted — it's a lazy cache rebuilt from
     * constituent words on first lookup post-load. */
    fwrite(&gl->phrase_count, sizeof(gl->phrase_count), 1, f);
    for (uint32_t pi = 0; pi < gl->phrase_count; pi++) {
        const gl_phrase_t* p = &gl->phrases[pi];
        fwrite(p->form, GL_MAX_PHRASE_LEN, 1, f);
        fwrite(&p->component_words, sizeof(p->component_words), 1, f);
        fwrite(&p->frequency, sizeof(p->frequency), 1, f);
    }

    fclose(f);
    LOG_INFO(LOG_MODULE,
             "Saved grounded language state to %s (%u words, %u phrases, dialect='%s')",
             path, gl->vocab_count, gl->phrase_count, gl->context_dialect);
    return 0;
}

grounded_language_t* grounded_language_load(const char* path, void* semantic_memory) {
    if (!path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_load: NULL path");
        return NULL;
    }

    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    uint32_t magic, version, semantic_dim, vocab_count;
    if (fread(&magic, sizeof(magic), 1, f) != 1 || magic != GL_MAGIC) {
        fclose(f);
        return NULL;
    }
    fread_chk(&version, sizeof(version), 1, f);
    fread_chk(&semantic_dim, sizeof(semantic_dim), 1, f);
    fread_chk(&vocab_count, sizeof(vocab_count), 1, f);

    /* Sanity-bound header values — corrupt or truncated checkpoints
     * can otherwise drive grounded_language_create into multi-GiB
     * allocations or send the read loop into billions of iterations.
     * 4096 is a generous cap (current GL_SEMANTIC_DIM is 128); GL_MAX_VOCAB
     * is the hard ceiling for any legitimate file. */
    if (semantic_dim == 0 || semantic_dim > 4096 ||
        vocab_count > GL_MAX_VOCAB) {
        fclose(f);
        return NULL;
    }

    grounded_language_t* gl = grounded_language_create(semantic_dim, semantic_memory);
    if (!gl) { fclose(f); return NULL; }

    /* Read word entries */
    for (uint32_t w = 0; w < vocab_count; w++) {
        char form[GL_MAX_WORD_LEN];
        uint32_t frequency;
        gl_word_class_t word_class;
        float valence, arousal;
        bool ctx_init;

        fread_chk(form, GL_MAX_WORD_LEN, 1, f);
        fread_chk(&frequency, sizeof(frequency), 1, f);
        fread_chk(&word_class, sizeof(word_class), 1, f);
        fread_chk(&valence, sizeof(valence), 1, f);
        fread_chk(&arousal, sizeof(arousal), 1, f);

        gl_lexicon_entry_t* entry = lexicon_find_or_create(gl, form);
        if (!entry) continue;

        entry->frequency = frequency;
        entry->learned_class = word_class;
        entry->valence = valence;
        entry->arousal = arousal;

        fread_chk(&ctx_init, sizeof(ctx_init), 1, f);
        entry->context_initialized = ctx_init;
        if (ctx_init) {
            if (version >= 4) {
                /* v4 quantized read: max_abs + int8 codes. */
                float max_abs = 0.0f;
                fread_chk(&max_abs, sizeof(float), 1, f);
                if (max_abs > 0.0f) {
                    /* Heap-allocate so semantic_dim can grow without
                     * silently corrupting context_vectors. The header
                     * sanity-check above caps semantic_dim at 4096. */
                    int8_t* qbuf = (int8_t*)nimcp_calloc(semantic_dim,
                                                          sizeof(int8_t));
                    if (!qbuf) {
                        grounded_language_destroy(gl);
                        fclose(f);
                        return NULL;
                    }
                    /* calloc zero-initializes, so a short fread leaves
                     * residual zeros in qbuf rather than uninitialized
                     * stack memory bleeding into context_vector. */
                    size_t got = fread(qbuf, sizeof(int8_t), semantic_dim, f);
                    (void)got;
                    float scale = max_abs / 127.0f;
                    for (uint32_t d = 0; d < semantic_dim; d++) {
                        entry->context_vector[d] = (float)qbuf[d] * scale;
                    }
                    nimcp_free(qbuf);
                }
                /* max_abs == 0 → zero vector, nothing more on disk. */
            } else {
                /* v1-v3 legacy: float block. */
                fread_chk(entry->context_vector, sizeof(float),
                          semantic_dim, f);
            }
        }

        uint32_t binding_count;
        fread_chk(&binding_count, sizeof(binding_count), 1, f);

        for (uint32_t b = 0; b < binding_count; b++) {
            gl_word_binding_t binding;
            fread_chk(&binding, sizeof(binding), 1, f);

            /* Grow bindings array if needed */
            if (entry->binding_count >= entry->binding_capacity) {
                uint32_t new_cap = entry->binding_capacity * 2;
                gl_word_binding_t* new_arr = (gl_word_binding_t*)nimcp_realloc(
                    entry->bindings, new_cap * sizeof(gl_word_binding_t));
                if (!new_arr) continue;
                entry->bindings = new_arr;
                entry->binding_capacity = new_cap;
            }
            entry->bindings[entry->binding_count++] = binding;
        }
    }

    /* Legacy template block (v<5). Templates were 8-slot fixed records:
     *   sizeof(gl_template_t) = sizeof(int)            // type enum
     *                         + 8 * sizeof(int)        // slots[8] enums
     *                         + sizeof(uint32_t)       // slot_count
     *                         + 2 * sizeof(float)      // frequency, confidence
     *                         = 4 + 32 + 4 + 8 = 48 bytes
     * The struct is removed in v5; we discard the bytes to advance the
     * file cursor to the dialect block. */
    if (version < 5) {
        uint32_t legacy_template_count = 0;
        fread_chk(&legacy_template_count, sizeof(legacy_template_count), 1, f);
        if (legacy_template_count > 0) {
            const long legacy_template_size = 48L;
            if (fseek(f, (long)legacy_template_count * legacy_template_size,
                      SEEK_CUR) != 0) {
                LOG_WARN(LOG_MODULE,
                         "grounded_language_load: failed to skip %u legacy templates",
                         legacy_template_count);
            }
        }
    }

    /* v2+: context_dialect (#14). v1 files don't have it; leave the
     * default empty string. fread on EOF returns 0 — silent fallback
     * preserves backwards compatibility. */
    if (version >= 2) {
        char dialect_buf[GL_MAX_DIALECT_LEN];
        size_t got = fread(dialect_buf, GL_MAX_DIALECT_LEN, 1, f);
        if (got == 1) {
            /* Force NUL termination defensively in case the file was
             * written without it. */
            dialect_buf[GL_MAX_DIALECT_LEN - 1] = '\0';
            memcpy(gl->context_dialect, dialect_buf, GL_MAX_DIALECT_LEN);
        }
    }

    /* v3+: compositional phrases (#9). v2 and earlier files don't have
     * this section; phrase_count stays 0 and phrases will be re-learned
     * from text. */
    if (version >= 3) {
        uint32_t saved_phrase_count = 0;
        if (fread(&saved_phrase_count, sizeof(saved_phrase_count), 1, f) == 1) {
            uint32_t cap = (saved_phrase_count > GL_MAX_PHRASES)
                ? GL_MAX_PHRASES : saved_phrase_count;
            for (uint32_t pi = 0; pi < cap; pi++) {
                char form_buf[GL_MAX_PHRASE_LEN];
                uint8_t comp_n;
                uint32_t freq;
                if (fread(form_buf, GL_MAX_PHRASE_LEN, 1, f) != 1) break;
                if (fread(&comp_n, sizeof(comp_n), 1, f) != 1) break;
                if (fread(&freq,  sizeof(freq),   1, f) != 1) break;
                form_buf[GL_MAX_PHRASE_LEN - 1] = '\0';
                gl_phrase_t* p = gl_internal_phrase_find_or_create(gl, form_buf, comp_n);
                if (p) p->frequency = freq;
            }
        }
    }

    fclose(f);
    LOG_INFO(LOG_MODULE,
             "Loaded grounded language state from %s (%u words, %u phrases, version=%u, dialect='%s')",
             path, gl->vocab_count, gl->phrase_count, version,
             gl->context_dialect);
    return gl;
}

/*=============================================================================
 * TA-1 : Multi-turn state persistence (discourse + anaphora + bigram spectrum)
 *
 * Writes three magic-prefixed blocks to a caller-owned FILE*. Each block
 * is independently versioned and self-delimiting so a reader missing one
 * (e.g. a future block we didn't ship yet, or a block whose subsystem
 * never existed for this gl) can stop cleanly without aborting the rest
 * of the load.
 *
 * Block magics:
 *   'LAND' (0x4C414E44) - discourse turn ring
 *   'LANA' (0x4C414E41) - anaphora referent ring
 *   'LANB' (0x4C414E42) - bigram-spectrum count matrix
 *
 * Wire format per block:
 *   uint32 magic
 *   uint32 version           (currently 1 for all three)
 *   ...block-specific payload...
 *
 * The save path always emits all three magics, but each one signals
 * "absent" via a count/cap of zero — the reader still reads the header
 * and the zero-count payload (which is empty), so layout stays in sync.
 *
 * Backward-compat: pre-TA-1 sidecar files / FILE handles that lack any
 * of the three blocks return EOF on the first read; load_multiturn_state
 * tolerates that and returns 0 (no state restored).
 *===========================================================================*/

#define GL_MULTITURN_MAGIC_DISCOURSE  0x4C414E44u  /* 'LAND' */
#define GL_MULTITURN_MAGIC_ANAPHORA   0x4C414E41u  /* 'LANA' */
#define GL_MULTITURN_MAGIC_SPECTRUM   0x4C414E42u  /* 'LANB' */
/* Audit fix: campaign feature configuration block. Persists the
 * default-OFF runtime flags + their tunables so the trainer doesn't
 * have to re-flip everything via daemon RPCs after each --resume. */
#define GL_MULTITURN_MAGIC_CONFIG     0x4C414E43u  /* 'LANC' */
#define GL_MULTITURN_VERSION          1u

/* Sanity caps to defend against corrupt input. */
#define GL_MULTITURN_MAX_VEC_DIM      4096u
#define GL_MULTITURN_MAX_VOCAB_CAP    4096u

static int mt_write_u32(FILE* f, uint32_t v) {
    return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}
static int mt_write_u64(FILE* f, uint64_t v) {
    return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}
static int mt_write_u8(FILE* f, uint8_t v) {
    return fwrite(&v, 1, 1, f) == 1 ? 0 : -1;
}
static int mt_write_floats(FILE* f, const float* arr, size_t n) {
    if (n == 0) return 0;
    return fwrite(arr, sizeof(float), n, f) == n ? 0 : -1;
}
static int mt_write_bytes(FILE* f, const void* buf, size_t n) {
    if (n == 0) return 0;
    return fwrite(buf, 1, n, f) == n ? 0 : -1;
}
static int mt_read_u32(FILE* f, uint32_t* v) {
    return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}
static int mt_read_u64(FILE* f, uint64_t* v) {
    return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}
static int mt_read_u8(FILE* f, uint8_t* v) {
    return fread(v, 1, 1, f) == 1 ? 0 : -1;
}
static int mt_read_floats(FILE* f, float* arr, size_t n) {
    if (n == 0) return 0;
    return fread(arr, sizeof(float), n, f) == n ? 0 : -1;
}
static int mt_read_bytes(FILE* f, void* buf, size_t n) {
    if (n == 0) return 0;
    return fread(buf, 1, n, f) == n ? 0 : -1;
}

static int mt_write_f32(FILE* f, float v) {
    return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}
static int mt_read_f32(FILE* f, float* v) {
    return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}

/* Audit fix — campaign feature configuration block.
 * Layout: magic, version,
 *   u8  enable_negation_inversion       (TA-2 era)
 *   u8  enable_sense_disambiguation     (Tier-2 #6)
 *   u8  enable_speech_act_classification (TB-9)
 *   u8  enable_sentence_segmentation    (TB-6)
 *   u8  enable_topic_shift_detection    (TB-10)
 *   u8  enable_reconsolidation          (TA-5)
 *   f32 reconsolidation_decay           (TA-5 tunable)
 *   f32 topic_shift_threshold           (TB-10 tunable)
 *   u32 topic_shift_min_turns           (TB-10 tunable)
 * Forward-compat: any extra fields after this set are skipped on load
 * (the version bump signals a new layout — readers of v1 stop here).
 * Bridge-side knobs (TA-3 enable_da_modulation, TA-4 enable_trigram_learning,
 * TB-7 length controls, TB-8 stream callback) live on the SNN bridge
 * and are persisted in its own sidecar (E3 work). This block only
 * covers gl-side flags. */
static int mt_save_config(const grounded_language_t* gl, FILE* f) {
    if (mt_write_u32(f, GL_MULTITURN_MAGIC_CONFIG) != 0) return -1;
    if (mt_write_u32(f, GL_MULTITURN_VERSION) != 0) return -1;
    if (mt_write_u8(f, gl->enable_negation_inversion ? 1u : 0u) != 0) return -1;
    if (mt_write_u8(f, gl->enable_sense_disambiguation ? 1u : 0u) != 0) return -1;
    if (mt_write_u8(f, gl->enable_speech_act_classification ? 1u : 0u) != 0) return -1;
    if (mt_write_u8(f, gl->enable_sentence_segmentation ? 1u : 0u) != 0) return -1;
    if (mt_write_u8(f, gl->enable_topic_shift_detection ? 1u : 0u) != 0) return -1;
    if (mt_write_u8(f, gl->enable_reconsolidation ? 1u : 0u) != 0) return -1;
    if (mt_write_f32(f, gl->reconsolidation_decay) != 0) return -1;
    if (mt_write_f32(f, gl->topic_shift_threshold) != 0) return -1;
    if (mt_write_u32(f, gl->topic_shift_min_turns) != 0) return -1;
    /* Audit-2 B6: TA-4 trigram learning lives on the SNN bridge but
     * gl is the natural persistence owner because learn_text_bigrams
     * reads it on every call. Persist via a passthrough — load applies
     * to the bridge if attached. Trailing field for backward-compat;
     * pre-B6 readers stop after topic_shift_min_turns. */
    bool trigram_on = false;
    if (gl->snn_bridge) {
        trigram_on = snn_language_bridge_get_trigram_learning_enabled(gl->snn_bridge);
    }
    if (mt_write_u8(f, trigram_on ? 1u : 0u) != 0) return -1;
    /* full-walkthrough 2026-05-25: NLP-2 coref + NLP-1 subword OOV flags.
     * Trailing fields appended after the trigram byte — no version bump
     * (the strict version-equality check would otherwise reject existing
     * v1 sidecars on resume). The config block is the last block in the
     * stream, so a pre-this-change reader stops cleanly at EOF after the
     * trigram byte; this-and-later readers try-read the two extra bytes. */
    if (mt_write_u8(f, gl->enable_coref_resolution ? 1u : 0u) != 0) return -1;
    if (mt_write_u8(f, gl->enable_subword_oov_fallback ? 1u : 0u) != 0) return -1;
    /* Tier 1 follow-up: autoregressive-produce flag (trailing, no version
     * bump — same forward/backward-compat pattern as the two bytes above). */
    if (mt_write_u8(f, gl->autoregressive_produce ? 1u : 0u) != 0) return -1;
    /* Tier 2: produce-side pronominalization flag (trailing). */
    if (mt_write_u8(f, gl->produce_pronominalize ? 1u : 0u) != 0) return -1;
    /* Tier 2: discourse-seeded AR produce flag (trailing). */
    if (mt_write_u8(f, gl->produce_discourse_seed ? 1u : 0u) != 0) return -1;
    /* FND-1: SVO clause-frame flag (trailing). */
    if (mt_write_u8(f, gl->produce_clause_frame ? 1u : 0u) != 0) return -1;
    /* Produce-score rebalance (2026-05-27): distributional weight (trailing
     * f32, no version bump — same forward/backward-compat pattern as the
     * trailing bytes above). */
    if (mt_write_f32(f, gl->produce_distributional_weight) != 0) return -1;
    /* T3-1 (2026-05-28): produce-side givenness definiteness flag (trailing). */
    if (mt_write_u8(f, gl->produce_givenness_definite ? 1u : 0u) != 0) return -1;
    /* TF-2 (2026-05-28): Tier-feedback master flag + corrector mask + 3 lrs
     * (trailing, no version bump). Layout:
     *   u8  produce_corrector_feedback_enabled
     *   u16 tf_enabled_correctors
     *   f32 tf_lr_trigram
     *   f32 tf_lr_distrib
     *   f32 tf_lr_bridge_stdp
     * Pre-this-change readers stop after the givenness byte and the mask /
     * lrs retain their constructor defaults. */
    if (mt_write_u8(f,  gl->produce_corrector_feedback_enabled ? 1u : 0u) != 0) return -1;
    if (mt_write_u32(f, (uint32_t)gl->tf_enabled_correctors) != 0) return -1;
    if (mt_write_f32(f, gl->tf_lr_trigram) != 0) return -1;
    if (mt_write_f32(f, gl->tf_lr_distrib) != 0) return -1;
    if (mt_write_f32(f, gl->tf_lr_bridge_stdp) != 0) return -1;
    /* T3-2 (2026-05-29): conjunction-insertion flag (trailing, optional). */
    if (mt_write_u8(f, gl->produce_t3_conjunction ? 1u : 0u) != 0) return -1;
    return 0;
}

static int mt_load_config_payload(grounded_language_t* gl, FILE* f,
                                    uint32_t version) {
    if (version != GL_MULTITURN_VERSION) {
        LOG_WARN(LOG_MODULE,
                 "mt_load_config: unsupported version=%u (runtime=%u) — "
                 "feature flags reset to defaults", version, GL_MULTITURN_VERSION);
        return -1;
    }
    uint8_t neg = 0, sense = 0, speech = 0, seg = 0, topic = 0, rec = 0;
    float dec = 0.0f, tt = 0.0f;
    uint32_t mt = 0;
    if (mt_read_u8(f, &neg) != 0) return -1;
    if (mt_read_u8(f, &sense) != 0) return -1;
    if (mt_read_u8(f, &speech) != 0) return -1;
    if (mt_read_u8(f, &seg) != 0) return -1;
    if (mt_read_u8(f, &topic) != 0) return -1;
    if (mt_read_u8(f, &rec) != 0) return -1;
    if (mt_read_f32(f, &dec) != 0) return -1;
    if (mt_read_f32(f, &tt) != 0) return -1;
    if (mt_read_u32(f, &mt) != 0) return -1;

    /* Apply via the public setters where available so clamping +
     * side-effects (e.g. anaphora ring reset on disable) run consistently
     * with normal toggle paths. */
    gl->enable_negation_inversion        = (neg   != 0);
    gl->enable_sense_disambiguation      = (sense != 0);
    gl->enable_speech_act_classification = (speech != 0);
    gl->enable_sentence_segmentation     = (seg   != 0);
    grounded_language_set_topic_shift_enabled(gl, (topic != 0));
    grounded_language_set_topic_shift_threshold(gl, tt);
    /* Audit-2 B1: route through the clamping setter rather than direct
     * assign — a corrupt sidecar with mt < 2 would otherwise wedge the
     * topic-shift min_turns invariant. */
    if (mt > 0) grounded_language_set_topic_shift_min_turns(gl, mt);
    grounded_language_set_reconsolidation_enabled(gl, (rec != 0));
    grounded_language_set_reconsolidation_decay(gl, dec);

    /* Audit-2 B6: trailing trigram-learning byte. Optional — pre-B6
     * sidecars stop after the reconsolidation_decay read above and
     * mt_load_config_payload returns 0 cleanly. Use a non-fatal try-
     * read (no error if EOF). */
    uint8_t trigram = 0;
    if (mt_read_u8(f, &trigram) == 0 && gl->snn_bridge) {
        snn_language_bridge_set_trigram_learning_enabled(
            gl->snn_bridge, (trigram != 0));
    }

    /* full-walkthrough 2026-05-25: NLP-2 coref + NLP-1 subword OOV trailing
     * bytes (optional). Pre-this-change sidecars stop after the trigram
     * byte above and these try-reads hit EOF — flags keep their defaults
     * (both OFF). Route through the public setters so the coref-disable
     * ring-reset side-effect runs consistently. The subword setter only
     * flips the flag; the tokenizer attach happens at brain init (which
     * runs before checkpoint state is applied). */
    uint8_t coref = 0, subword = 0;
    if (mt_read_u8(f, &coref) == 0) {
        grounded_language_set_coref_resolution_enabled(gl, (coref != 0));
    }
    if (mt_read_u8(f, &subword) == 0) {
        grounded_language_set_subword_oov_fallback_enabled(gl, (subword != 0));
    }
    /* Tier 1 follow-up: autoregressive-produce flag (trailing, optional). */
    uint8_t ar = 0;
    if (mt_read_u8(f, &ar) == 0) {
        grounded_language_set_autoregressive_produce(gl, (ar != 0));
    }
    /* Tier 2: produce-side pronominalization flag (trailing, optional). */
    uint8_t pron = 0;
    if (mt_read_u8(f, &pron) == 0) {
        grounded_language_set_produce_pronominalize(gl, (pron != 0));
    }
    /* Tier 2: discourse-seeded AR produce flag (trailing, optional). */
    uint8_t dseed = 0;
    if (mt_read_u8(f, &dseed) == 0) {
        grounded_language_set_produce_discourse_seed(gl, (dseed != 0));
    }
    /* FND-1: SVO clause-frame flag (trailing, optional). */
    uint8_t clause = 0;
    if (mt_read_u8(f, &clause) == 0) {
        grounded_language_set_produce_clause_frame(gl, (clause != 0));
    }
    /* Produce-score rebalance (2026-05-27): distributional weight (trailing
     * f32, optional). Pre-this-change sidecars stop after the clause byte
     * and this try-read hits EOF — the weight keeps its init default (0.4).
     * Route through the clamping setter. */
    float pdw = 0.0f;
    if (mt_read_f32(f, &pdw) == 0) {
        grounded_language_set_produce_distributional_weight(gl, pdw);
    }
    /* T3-1 (2026-05-28): givenness-definiteness flag (trailing, optional). */
    uint8_t givn = 0;
    if (mt_read_u8(f, &givn) == 0) {
        grounded_language_set_produce_givenness_definite(gl, (givn != 0));
    }
    /* TF-2 (2026-05-28): Tier-feedback master flag + corrector mask + 3 lrs
     * (trailing, optional). Each field is read independently — a partial
     * sidecar (e.g. only the master byte present) loads what it has and
     * leaves the rest at constructor defaults. Setters clamp via the public
     * surface so corrupt sidecars can't push lrs past the per-path max. */
    uint8_t  tf_master = 0;
    uint32_t tf_mask   = 0;
    float    tf_lr_t   = 0.0f;
    float    tf_lr_d   = 0.0f;
    float    tf_lr_b   = 0.0f;
    if (mt_read_u8(f,  &tf_master) == 0) {
        grounded_language_set_produce_corrector_feedback_enabled(gl, (tf_master != 0));
    }
    if (mt_read_u32(f, &tf_mask) == 0) {
        grounded_language_set_tf_enabled_correctors(gl, (uint16_t)tf_mask);
    }
    if (mt_read_f32(f, &tf_lr_t) == 0) {
        grounded_language_set_tf_lr_trigram(gl, tf_lr_t);
    }
    if (mt_read_f32(f, &tf_lr_d) == 0) {
        grounded_language_set_tf_lr_distrib(gl, tf_lr_d);
    }
    if (mt_read_f32(f, &tf_lr_b) == 0) {
        grounded_language_set_tf_lr_bridge_stdp(gl, tf_lr_b);
    }
    /* T3-2 (2026-05-29): conjunction-insertion flag (trailing, optional).
     * Pre-this-change sidecars stop after the TF stdp lr — that's fine, the
     * flag keeps its constructor default (true). */
    uint8_t conj = 0;
    if (mt_read_u8(f, &conj) == 0) {
        grounded_language_set_produce_t3_conjunction(gl, (conj != 0));
    }
    return 0;
}

/* Discourse-ring write. Layout: magic, version, capacity, head, count,
 * then `count` turns, each: word_count, timestamp_ms, is_user, vec_dim,
 * float[vec_dim]. Empty turns (NULL semantic_vector) are written with
 * vec_dim=0 so the reader doesn't allocate a placeholder vector. */
static int mt_save_discourse(const grounded_language_t* gl, FILE* f) {
    if (mt_write_u32(f, GL_MULTITURN_MAGIC_DISCOURSE) != 0) return -1;
    if (mt_write_u32(f, GL_MULTITURN_VERSION) != 0) return -1;
    if (mt_write_u32(f, (uint32_t)gl->discourse.capacity) != 0) return -1;
    if (mt_write_u32(f, (uint32_t)gl->discourse.head) != 0) return -1;
    if (mt_write_u32(f, (uint32_t)gl->discourse.count) != 0) return -1;

    /* Walk live turns in [oldest, newest) order using discourse_idx. */
    for (uint8_t i = 0; i < gl->discourse.count; i++) {
        uint8_t idx = discourse_idx(&gl->discourse, i);
        const gl_discourse_turn_t* t = &gl->discourse.turns[idx];
        if (mt_write_u32(f, t->word_count) != 0) return -1;
        if (mt_write_u64(f, t->timestamp_ms) != 0) return -1;
        if (mt_write_u8(f, t->is_user ? 1u : 0u) != 0) return -1;
        uint32_t vec_dim = (t->semantic_vector != NULL) ? gl->semantic_dim : 0u;
        if (mt_write_u32(f, vec_dim) != 0) return -1;
        if (vec_dim > 0) {
            if (mt_write_floats(f, t->semantic_vector, vec_dim) != 0) return -1;
        }
    }
    return 0;
}

/* Discourse-ring read. The magic is consumed by the dispatcher. Re-pushes
 * each turn through grounded_language_push_turn so the live ring keeps
 * its invariants (head/count semantics, recency-blend rebuild). */
static int mt_load_discourse_payload(grounded_language_t* gl, FILE* f,
                                     uint32_t version) {
    if (version != GL_MULTITURN_VERSION) {
        LOG_WARN(LOG_MODULE,
                 "mt_load_discourse: unsupported version=%u (runtime=%u) — "
                 "skipping discourse block",
                 version, GL_MULTITURN_VERSION);
        return -1;
    }
    uint32_t capacity = 0, head = 0, count = 0;
    if (mt_read_u32(f, &capacity) != 0) return -1;
    if (mt_read_u32(f, &head) != 0) return -1;
    if (mt_read_u32(f, &count) != 0) return -1;
    if (capacity > GL_DISCOURSE_MAX_TURNS || count > GL_DISCOURSE_MAX_TURNS) {
        LOG_WARN(LOG_MODULE,
                 "mt_load_discourse: corrupt header (cap=%u count=%u, max=%u)",
                 capacity, count, GL_DISCOURSE_MAX_TURNS);
        return -1;
    }
    /* Apply capacity first so push_turn sizes the ring correctly. */
    if (capacity > 0) {
        grounded_language_set_discourse_capacity(gl, (uint8_t)capacity);
    }

    /* Reset the existing ring. push_turn appends, so any pre-existing
     * turns would shift the saved order. Free vectors and zero counters. */
    for (uint8_t i = 0; i < GL_DISCOURSE_MAX_TURNS; i++) {
        if (gl->discourse.turns[i].semantic_vector) {
            nimcp_free(gl->discourse.turns[i].semantic_vector);
            gl->discourse.turns[i].semantic_vector = NULL;
        }
        gl->discourse.turns[i].word_count = 0;
        gl->discourse.turns[i].timestamp_ms = 0;
        gl->discourse.turns[i].is_user = false;
    }
    gl->discourse.head = 0;
    gl->discourse.count = 0;

    float* scratch = NULL;
    uint32_t scratch_dim = 0;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t word_count = 0;
        uint64_t timestamp_ms = 0;
        uint8_t is_user_u8 = 0;
        uint32_t vec_dim = 0;
        if (mt_read_u32(f, &word_count) != 0) goto fail;
        if (mt_read_u64(f, &timestamp_ms) != 0) goto fail;
        if (mt_read_u8(f, &is_user_u8) != 0) goto fail;
        if (mt_read_u32(f, &vec_dim) != 0) goto fail;
        if (vec_dim > GL_MULTITURN_MAX_VEC_DIM) {
            LOG_WARN(LOG_MODULE,
                     "mt_load_discourse: vec_dim %u exceeds cap %u",
                     vec_dim, GL_MULTITURN_MAX_VEC_DIM);
            goto fail;
        }

        const float* push_vec = NULL;
        if (vec_dim > 0) {
            if (vec_dim != scratch_dim) {
                nimcp_free(scratch);
                scratch = (float*)nimcp_calloc(vec_dim, sizeof(float));
                if (!scratch) goto fail;
                scratch_dim = vec_dim;
            }
            if (mt_read_floats(f, scratch, vec_dim) != 0) goto fail;
            push_vec = scratch;
        }

        /* push_turn copies push_vec into a gl-owned buffer of length
         * gl->semantic_dim — so vec_dim mismatch is tolerated, with
         * the saved vector clamped/padded to the live dim. */
        if (grounded_language_push_turn(gl, push_vec, vec_dim,
                                          word_count,
                                          is_user_u8 != 0) != 0) {
            LOG_WARN(LOG_MODULE,
                     "mt_load_discourse: push_turn failed at i=%u; "
                     "remaining turns dropped", i);
            goto fail;
        }
        /* Restore the saved timestamp — push_turn stamps with current time. */
        uint8_t newest = (uint8_t)(gl->discourse.count - 1);
        uint8_t newest_idx = discourse_idx(&gl->discourse, newest);
        gl->discourse.turns[newest_idx].timestamp_ms = timestamp_ms;
    }

    /* `head` from the save is informational — push_turn rebuilt the ring
     * with head=0, count=N. The ordering is preserved (oldest-first), so
     * functional behaviour is identical. */
    (void)head;

    nimcp_free(scratch);
    return 0;

fail:
    nimcp_free(scratch);
    return -1;
}

/* Anaphora-ring write. Layout: magic, version, present_flag, then if
 * present: enabled, head, size, then GL_ANAPHORA_RING_CAP slots each
 * (form[GL_MAX_WORD_LEN], gender(u32), timestamp_us(u64)). present_flag
 * is 0 when no anaphora state exists for this gl in the side-map. */
static int mt_save_anaphora(grounded_language_t* gl, FILE* f) {
    if (mt_write_u32(f, GL_MULTITURN_MAGIC_ANAPHORA) != 0) return -1;
    if (mt_write_u32(f, GL_MULTITURN_VERSION) != 0) return -1;

    gl_tc12_lock(gl);
    gl_anaphora_state_t* st = anaphora_lookup_locked(gl);
    uint8_t present = (st != NULL) ? 1u : 0u;
    if (mt_write_u8(f, present) != 0) {
        gl_tc12_unlock(gl);
        return -1;
    }
    if (!present) {
        gl_tc12_unlock(gl);
        return 0;
    }

    uint8_t enabled = st->enabled ? 1u : 0u;
    uint32_t head = st->head;
    uint32_t size = st->size;
    if (mt_write_u8(f, enabled) != 0) goto fail;
    if (mt_write_u32(f, head) != 0) goto fail;
    if (mt_write_u32(f, size) != 0) goto fail;

    for (uint32_t i = 0; i < GL_ANAPHORA_RING_CAP; i++) {
        const gl_anaphora_referent_t* r = &st->ring[i];
        if (mt_write_bytes(f, r->form, GL_MAX_WORD_LEN) != 0) goto fail;
        if (mt_write_u32(f, (uint32_t)r->gender) != 0) goto fail;
        if (mt_write_u64(f, r->timestamp_us) != 0) goto fail;
    }
    gl_tc12_unlock(gl);
    return 0;

fail:
    gl_tc12_unlock(gl);
    return -1;
}

static int mt_load_anaphora_payload(grounded_language_t* gl, FILE* f,
                                    uint32_t version) {
    if (version != GL_MULTITURN_VERSION) {
        LOG_WARN(LOG_MODULE,
                 "mt_load_anaphora: unsupported version=%u (runtime=%u)",
                 version, GL_MULTITURN_VERSION);
        return -1;
    }
    uint8_t present = 0;
    if (mt_read_u8(f, &present) != 0) return -1;
    if (!present) return 0;  /* nothing to restore */

    uint8_t enabled = 0;
    uint32_t head = 0, size = 0;
    if (mt_read_u8(f, &enabled) != 0) return -1;
    if (mt_read_u32(f, &head) != 0) return -1;
    if (mt_read_u32(f, &size) != 0) return -1;
    if (head >= GL_ANAPHORA_RING_CAP || size > GL_ANAPHORA_RING_CAP) {
        LOG_WARN(LOG_MODULE,
                 "mt_load_anaphora: corrupt head/size (head=%u size=%u cap=%u)",
                 head, size, (unsigned)GL_ANAPHORA_RING_CAP);
        return -1;
    }

    gl_tc12_lock(gl);
    gl_anaphora_state_t* st = anaphora_get_or_create_locked(gl);
    if (!st) {
        gl_tc12_unlock(gl);
        LOG_WARN(LOG_MODULE,
                 "mt_load_anaphora: alloc failed — state lost on this gl");
        /* Still consume the payload bytes so we don't desync the file. */
        for (uint32_t i = 0; i < GL_ANAPHORA_RING_CAP; i++) {
            char form_buf[GL_MAX_WORD_LEN];
            uint32_t gender = 0;
            uint64_t ts = 0;
            if (mt_read_bytes(f, form_buf, GL_MAX_WORD_LEN) != 0) return -1;
            if (mt_read_u32(f, &gender) != 0) return -1;
            if (mt_read_u64(f, &ts) != 0) return -1;
        }
        return 0;
    }

    st->enabled = (enabled != 0);
    st->head = head;
    st->size = size;
    for (uint32_t i = 0; i < GL_ANAPHORA_RING_CAP; i++) {
        gl_anaphora_referent_t* r = &st->ring[i];
        char form_buf[GL_MAX_WORD_LEN];
        uint32_t gender = 0;
        uint64_t ts = 0;
        if (mt_read_bytes(f, form_buf, GL_MAX_WORD_LEN) != 0) goto fail;
        if (mt_read_u32(f, &gender) != 0) goto fail;
        if (mt_read_u64(f, &ts) != 0) goto fail;
        memcpy(r->form, form_buf, GL_MAX_WORD_LEN);
        r->form[GL_MAX_WORD_LEN - 1] = '\0';
        if (gender > (uint32_t)GL_GENDER_PLURAL) gender = GL_GENDER_UNKNOWN;
        r->gender = (gl_anaphora_gender_t)gender;
        r->timestamp_us = ts;
    }
    gl_tc12_unlock(gl);
    return 0;

fail:
    gl_tc12_unlock(gl);
    return -1;
}

/* Bigram-spectrum write. Layout: magic, version, present_flag, then if
 * present: vocab_cap, total_events, uint32[vocab_cap × vocab_cap].
 * present is set when total_events > 0 (the spec says skip if no events). */
static int mt_save_spectrum(grounded_language_t* gl, FILE* f) {
    if (mt_write_u32(f, GL_MULTITURN_MAGIC_SPECTRUM) != 0) return -1;
    if (mt_write_u32(f, GL_MULTITURN_VERSION) != 0) return -1;

    /* Audit fix (TOCTOU): hold the lock across the entire snapshot read.
     * fwrite is the slow part; per-gl scope means other gls keep training. */
    gl_tc12_lock(gl);
    bigram_spectrum_t* bs = (bigram_spectrum_t*)gl->bigram_spectrum;
    uint64_t total = bs ? bigram_spectrum_total_events(bs) : 0;
    uint8_t present = (bs != NULL && total > 0) ? 1u : 0u;
    if (mt_write_u8(f, present) != 0) { gl_tc12_unlock(gl); return -1; }
    if (!present) { gl_tc12_unlock(gl); return 0; }

    uint32_t cap = bigram_spectrum_vocab_cap(bs);
    if (cap == 0 || cap > GL_MULTITURN_MAX_VOCAB_CAP) {
        LOG_WARN(LOG_MODULE,
                 "mt_save_spectrum: implausible vocab_cap=%u; emitting empty",
                 cap);
        gl_tc12_unlock(gl);
        return -1;
    }
    if (mt_write_u32(f, cap) != 0) { gl_tc12_unlock(gl); return -1; }
    if (mt_write_u64(f, total) != 0) { gl_tc12_unlock(gl); return -1; }
    const uint32_t* counts = bigram_spectrum_counts(bs);
    if (!counts) { gl_tc12_unlock(gl); return -1; }
    size_t cells = (size_t)cap * (size_t)cap;
    if (fwrite(counts, sizeof(uint32_t), cells, f) != cells) {
        gl_tc12_unlock(gl);
        return -1;
    }
    gl_tc12_unlock(gl);
    return 0;
}

static int mt_load_spectrum_payload(grounded_language_t* gl, FILE* f,
                                    uint32_t version) {
    if (version != GL_MULTITURN_VERSION) {
        LOG_WARN(LOG_MODULE,
                 "mt_load_spectrum: unsupported version=%u (runtime=%u)",
                 version, GL_MULTITURN_VERSION);
        return -1;
    }
    uint8_t present = 0;
    if (mt_read_u8(f, &present) != 0) return -1;
    if (!present) return 0;

    uint32_t cap = 0;
    uint64_t total = 0;
    if (mt_read_u32(f, &cap) != 0) return -1;
    if (mt_read_u64(f, &total) != 0) return -1;
    if (cap == 0 || cap > GL_MULTITURN_MAX_VOCAB_CAP) {
        LOG_WARN(LOG_MODULE,
                 "mt_load_spectrum: implausible vocab_cap=%u — refusing",
                 cap);
        return -1;
    }
    size_t cells = (size_t)cap * (size_t)cap;
    uint32_t* buf = (uint32_t*)nimcp_calloc(cells, sizeof(uint32_t));
    if (!buf) return -1;
    if (fread(buf, sizeof(uint32_t), cells, f) != cells) {
        nimcp_free(buf);
        return -1;
    }

    /* Find the attached spectrum; if absent or cap-mismatched, the saved
     * data is dropped (we don't allocate a new spectrum the trainer never
     * asked for, and we don't resize an existing one in place).
     * Audit fix (TOCTOU): same per-gl lock pattern as mt_save_spectrum. */
    gl_tc12_lock(gl);
    bigram_spectrum_t* bs = (bigram_spectrum_t*)gl->bigram_spectrum;
    if (!bs) {
        LOG_WARN(LOG_MODULE,
                 "mt_load_spectrum: no spectrum attached to gl=%p — "
                 "saved counts dropped", (void*)gl);
        gl_tc12_unlock(gl);
        nimcp_free(buf);
        return 0;
    }
    uint32_t live_cap = bigram_spectrum_vocab_cap(bs);
    if (live_cap != cap) {
        LOG_WARN(LOG_MODULE,
                 "mt_load_spectrum: vocab_cap mismatch (file=%u live=%u) — "
                 "saved counts dropped", cap, live_cap);
        gl_tc12_unlock(gl);
        nimcp_free(buf);
        return 0;
    }
    if (bigram_spectrum_load_counts(bs, buf, total) != 0) {
        LOG_WARN(LOG_MODULE,
                 "mt_load_spectrum: bigram_spectrum_load_counts failed");
        gl_tc12_unlock(gl);
        nimcp_free(buf);
        return -1;
    }
    gl_tc12_unlock(gl);
    nimcp_free(buf);
    return 0;
}

int grounded_language_save_multiturn_state(grounded_language_t* gl, FILE* fp) {
    if (!gl || !fp) {
        LOG_WARN(LOG_MODULE, "save_multiturn_state: NULL gl or fp");
        return -1;
    }
    if (mt_save_discourse(gl, fp) != 0) {
        LOG_WARN(LOG_MODULE, "save_multiturn_state: discourse save failed");
        return -1;
    }
    if (mt_save_anaphora(gl, fp) != 0) {
        LOG_WARN(LOG_MODULE, "save_multiturn_state: anaphora save failed");
        return -1;
    }
    if (mt_save_spectrum(gl, fp) != 0) {
        LOG_WARN(LOG_MODULE, "save_multiturn_state: spectrum save failed");
        return -1;
    }
    /* Audit fix: persist campaign feature flags + tunables. */
    if (mt_save_config(gl, fp) != 0) {
        LOG_WARN(LOG_MODULE, "save_multiturn_state: config save failed");
        return -1;
    }
    return 0;
}

int grounded_language_load_multiturn_state(grounded_language_t* gl, FILE* fp) {
    if (!gl || !fp) {
        LOG_WARN(LOG_MODULE, "load_multiturn_state: NULL gl or fp");
        return -1;
    }

    /* D7: suppress NEW_WORD events / synthesized SNN spike pairs while
     * we rehydrate state. Mirrors gl_persistence_load. */
    bool prev_loading = gl->is_loading;
    gl->is_loading = true;
    int blocks_loaded = 0;

    /* Read magic-prefixed blocks until EOF. Unknown magics abort the
     * load (the file is misaligned). Each successful payload increments
     * a counter; success returns 0 even when no blocks are present. */
    for (;;) {
        uint32_t magic = 0;
        if (mt_read_u32(fp, &magic) != 0) {
            /* Clean EOF if we already loaded ≥1 block, or saw 0 (legacy). */
            break;
        }
        uint32_t version = 0;
        if (mt_read_u32(fp, &version) != 0) {
            LOG_WARN(LOG_MODULE,
                     "load_multiturn_state: truncated after magic 0x%08x", magic);
            gl->is_loading = prev_loading;
            return -1;
        }
        int rc;
        switch (magic) {
            case GL_MULTITURN_MAGIC_DISCOURSE:
                rc = mt_load_discourse_payload(gl, fp, version);
                break;
            case GL_MULTITURN_MAGIC_ANAPHORA:
                rc = mt_load_anaphora_payload(gl, fp, version);
                break;
            case GL_MULTITURN_MAGIC_SPECTRUM:
                rc = mt_load_spectrum_payload(gl, fp, version);
                break;
            case GL_MULTITURN_MAGIC_CONFIG:
                rc = mt_load_config_payload(gl, fp, version);
                break;
            default:
                LOG_WARN(LOG_MODULE,
                         "load_multiturn_state: unknown magic 0x%08x — "
                         "stopping here", magic);
                gl->is_loading = prev_loading;
                return blocks_loaded > 0 ? 0 : -1;
        }
        if (rc != 0) {
            LOG_WARN(LOG_MODULE,
                     "load_multiturn_state: payload load failed for magic 0x%08x",
                     magic);
            gl->is_loading = prev_loading;
            return -1;
        }
        blocks_loaded++;
    }

    gl->is_loading = prev_loading;
    return 0;
}

/*=============================================================================
 * Internal — exposed via nimcp_grounded_language_internal.h
 *===========================================================================*/

gl_lexicon_entry_t* gl_internal_lexicon_find_or_create(
    grounded_language_t* gl,
    const char* word) {
    if (!gl || !word) return NULL;
    return lexicon_find_or_create(gl, word);
}

/*=============================================================================
 * Slice E (Option-1 rebuild) — developmental-stage vocab mask implementation.
 *
 * The mask lives on the grounded_language struct as a heap-owned bool[].
 * Capacity grows monotonically with vocab_count. Indices align with
 * vocab_list[], so reading mask[i] is O(1).
 *===========================================================================*/

/* Ensure the mask array can index up to `needed` entries. Grows by a 1.5x
 * factor on each resize so callers that incrementally insert vocabulary
 * after install pay amortized O(1) per insert. New slots default to
 * VISIBLE so newly-learned vocabulary doesn't get silently hidden by an
 * older mask (the trainer can rerun set_active_vocab_mask to re-apply
 * the cap if needed). Returns 0 on success, -1 on alloc failure. */
static int gl_vocab_mask_ensure_capacity(grounded_language_t* gl,
                                          uint32_t needed) {
    if (!gl) return -1;
    if (gl->vocab_active_mask_capacity >= needed) return 0;

    uint32_t new_cap = gl->vocab_active_mask_capacity;
    if (new_cap < 64u) new_cap = 64u;
    while (new_cap < needed) {
        /* 1.5x growth, saturating */
        uint64_t bumped = (uint64_t)new_cap + (uint64_t)(new_cap >> 1);
        if (bumped > 0xFFFFFFFFu) {
            new_cap = needed;
            break;
        }
        new_cap = (uint32_t)bumped;
    }

    bool* fresh;
    if (gl->vocab_active_mask) {
        fresh = (bool*)nimcp_realloc(gl->vocab_active_mask,
                                      new_cap * sizeof(bool));
    } else {
        fresh = (bool*)nimcp_calloc(new_cap, sizeof(bool));
    }
    if (!fresh) return -1;

    /* Default newly-allocated tail to "visible". For a realloc the old
     * region keeps its values; only the new tail needs initialization. */
    for (uint32_t i = gl->vocab_active_mask_capacity; i < new_cap; i++) {
        fresh[i] = true;
    }

    gl->vocab_active_mask = fresh;
    gl->vocab_active_mask_capacity = new_cap;
    return 0;
}

int grounded_language_set_active_vocab_mask(grounded_language_t* gl,
                                              uint32_t stage) {
    if (!gl) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "grounded_language_set_active_vocab_mask: NULL gl");
        return -1;
    }

    const stage_constraints_t* sc = stage_table_get(stage);
    if (!sc) {
        /* stage_table_get clamps, so this is defensive only. */
        sc = stage_table_default();
    }

    /* "Unlimited" max_visible_vocab → mask every entry visible. We still
     * record the stage so future grow-events apply consistent defaults. */
    const bool unlimited = (sc->max_visible_vocab >= (size_t)UINT32_MAX);
    uint32_t cap = gl->vocab_count;
    if (cap == 0u) cap = 64u;  /* pre-allocate so first lexicon insert has space */

    gl_mutate_lock(gl);
    if (gl_vocab_mask_ensure_capacity(gl, cap) != 0) {
        gl_mutate_unlock(gl);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "grounded_language_set_active_vocab_mask: alloc failed");
        return -1;
    }

    if (unlimited) {
        for (uint32_t i = 0; i < gl->vocab_active_mask_capacity; i++) {
            gl->vocab_active_mask[i] = true;
        }
    } else {
        uint32_t cutoff = (sc->max_visible_vocab > (size_t)UINT32_MAX)
                              ? UINT32_MAX
                              : (uint32_t)sc->max_visible_vocab;
        for (uint32_t i = 0; i < gl->vocab_active_mask_capacity; i++) {
            gl->vocab_active_mask[i] = (i < cutoff);
        }
    }
    gl->vocab_active_mask_stage = stage;
    gl_mutate_unlock(gl);
    return 0;
}

void grounded_language_clear_active_vocab_mask(grounded_language_t* gl) {
    if (!gl) return;
    gl_mutate_lock(gl);
    nimcp_free(gl->vocab_active_mask);
    gl->vocab_active_mask = NULL;
    gl->vocab_active_mask_capacity = 0u;
    gl->vocab_active_mask_stage = 0u;
    gl_mutate_unlock(gl);
}

bool grounded_language_vocab_index_visible(const grounded_language_t* gl,
                                            uint32_t idx) {
    if (!gl) return true;
    /* No mask installed → everything is visible. */
    if (!gl->vocab_active_mask) return true;
    /* Index out of mask range → default visible (the mask doesn't cover
     * this entry yet — either it's a newly-added vocab item or the mask
     * is stale w.r.t. the lexicon. Either way, don't deny.). */
    if (idx >= gl->vocab_active_mask_capacity) return true;
    return gl->vocab_active_mask[idx];
}

/* Helper: find the vocab_list index of a given surface form (case-
 * insensitive). Returns UINT32_MAX when not found. */
static uint32_t gl_vocab_list_index_of(const grounded_language_t* gl,
                                        const char* form) {
    if (!gl || !form || !gl->vocab_list) return UINT32_MAX;
    /* Walk vocab_list — N is small in the early curriculum where the
     * mask is most useful. We don't have a form→index map. */
    char lower[GL_MAX_WORD_LEN];
    lowercase_word(form, lower, GL_MAX_WORD_LEN);
    for (uint32_t i = 0; i < gl->vocab_count; i++) {
        const gl_lexicon_entry_t* e = gl->vocab_list[i];
        if (!e) continue;
        if (strcmp(e->form, lower) == 0) return i;
    }
    return UINT32_MAX;
}

uint32_t grounded_language_filter_production_by_mask(
    grounded_language_t* gl,
    gl_production_result_t* result) {
    if (!gl || !result || !result->text || result->word_count == 0) return 0u;
    /* No mask → nothing to filter. */
    if (!gl->vocab_active_mask) return 0u;

    /* Tokenize result->text in place into pointers, filter, re-join.
     * We strdup first so we can rewrite the original buffer without
     * tripping the strtok_r aliasing. */
    size_t tlen = strlen(result->text);
    char* dup = (char*)nimcp_malloc(tlen + 1u);
    if (!dup) return 0u;
    memcpy(dup, result->text, tlen + 1u);

    /* Worst case: every byte becomes a token boundary (impossible but
     * defensive). Cap at word_count for the kept pointer array. */
    enum { GL_FILTER_MAX_TOKENS = 256 };
    const char* survive[GL_FILTER_MAX_TOKENS];
    uint32_t survive_count = 0;
    uint32_t dropped = 0;

    char* saveptr = NULL;
    char* tok = strtok_r(dup, " \t\n\r", &saveptr);
    while (tok && survive_count < GL_FILTER_MAX_TOKENS) {
        gl_mutate_lock(gl);
        uint32_t idx = gl_vocab_list_index_of(gl, tok);
        bool keep;
        if (idx == UINT32_MAX) {
            /* Unknown surface form — DEFAULT KEEP. The lexicon hasn't
             * grounded this token; mask doesn't apply. The cascade's
             * downstream stages decide whether to drop. */
            keep = true;
        } else if (idx >= gl->vocab_active_mask_capacity) {
            keep = true;
        } else {
            keep = gl->vocab_active_mask[idx];
        }
        gl_mutate_unlock(gl);

        if (keep) {
            survive[survive_count++] = tok;
        } else {
            dropped++;
        }
        tok = strtok_r(NULL, " \t\n\r", &saveptr);
    }

    /* Defensive: if every token was masked out, leave the result alone
     * (return dropped count so the caller can react). */
    if (survive_count == 0u) {
        nimcp_free(dup);
        return dropped;
    }
    /* If nothing was dropped, we wasted some cycles but the text stays
     * the same. */
    if (dropped == 0u) {
        nimcp_free(dup);
        return 0u;
    }

    /* Rejoin the survivors with single-space separators. The new buffer
     * is strictly shorter than the original (at least one token gone +
     * its separator), so we reuse the original allocation. */
    size_t need = 0;
    for (uint32_t i = 0; i < survive_count; i++) {
        need += strlen(survive[i]);
        if (i + 1u < survive_count) need += 1u;  /* separator */
    }
    /* Need + 1 NUL ≤ tlen + 1 since we dropped at least one token. */
    char* w = result->text;
    for (uint32_t i = 0; i < survive_count; i++) {
        size_t slen = strlen(survive[i]);
        memcpy(w, survive[i], slen);
        w += slen;
        if (i + 1u < survive_count) {
            *w++ = ' ';
        }
    }
    *w = '\0';
    result->word_count = survive_count;

    nimcp_free(dup);
    return dropped;
}
