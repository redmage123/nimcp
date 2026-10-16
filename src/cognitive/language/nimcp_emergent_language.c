/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * This module has no production callers. nimcp_emergent_language_create
 * is referenced only inside this file; brain-struct has no
 * `emergent_language` field. The intended consumer was likely the
 * brain-native language path, which now lives in inner_speech /
 * brain_native_language. Either wire a consumer or delete the file
 * before the next major version. Do not extend.
 */

/**
 * @file nimcp_emergent_language.c
 * @brief Emergent language — vocabulary discovered from neural activation patterns
 *
 * WHAT: Tokens emerge from brain output clustering, no human seed words
 * WHY:  The brain develops its OWN symbols that optimally encode its thought space
 * HOW:  Online k-means on brain output vectors + matching pursuit for expression
 *
 * Key algorithms:
 *   observe()  — online clustering: reinforce or discover tokens
 *   express()  — matching pursuit: decompose brain state into token sequence
 *   comprehend() — reconstruct: average centroids back to brain embedding
 */

#include "cognitive/language/nimcp_emergent_language.h"
#include "cognitive/language/nimcp_w12_language_kg_events.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "EMERGENT_LANGUAGE"
#define BRAIN_DIM 4096
#define MAX_LAST_EXPRESSED 32
#define CO_OCCUR_WINDOW 8
#define ACTIVE_WINDOW 1000

/* ======================================================================== */
/* Internal struct                                                            */
/* ======================================================================== */

struct nimcp_emergent_language {
    nimcp_emergent_config_t config;

    nimcp_emergent_token_t* tokens;
    uint32_t vocab_size;

    /* Translation table */
    nimcp_emergent_translation_t* translations;

    /* Co-occurrence tracking */
    uint32_t last_expressed[MAX_LAST_EXPRESSED];
    uint32_t last_expressed_count;

    /* Statistics */
    uint32_t total_observations;
    uint32_t merges_performed;
    uint32_t tokens_pruned;

    /* Symbol generation */
    uint32_t next_symbol_id;

    nimcp_mutex_t* lock;
    uint32_t rng_state;
};

/* ======================================================================== */
/* Helpers                                                                    */
/* ======================================================================== */

static uint32_t _xorshift(uint32_t* s) {
    uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *s = x; return x;
}

static float _randf(uint32_t* s) {
    return (_xorshift(s) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

static float _vec_norm(const float* v, uint32_t n) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) sum += v[i] * v[i];
    return sqrtf(sum + 1e-12f);
}

static float _cosine_similarity(const float* a, const float* b, uint32_t n) {
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    float denom = sqrtf(na + 1e-12f) * sqrtf(nb + 1e-12f);
    return dot / denom;
}

static void _generate_symbol(char* symbol, uint32_t id, bool use_unicode) {
    if (use_unicode) {
        /* Greek letters + geometric symbols with subscript numbers */
        const char* bases[] = {
            "\xce\xb1",  /* alpha */
            "\xce\xb2",  /* beta */
            "\xce\xb3",  /* gamma */
            "\xce\xb4",  /* delta */
            "\xce\xb5",  /* epsilon */
            "\xce\xb6",  /* zeta */
            "\xce\xb7",  /* eta */
            "\xce\xb8",  /* theta */
            "\xce\xb9",  /* iota */
            "\xce\xba",  /* kappa */
            "\xce\xbb",  /* lambda */
            "\xce\xbc",  /* mu */
            "\xce\xbd",  /* nu */
            "\xce\xbe",  /* xi */
            "\xcf\x80",  /* pi */
            "\xcf\x81",  /* rho */
            "\xcf\x83",  /* sigma */
            "\xcf\x84",  /* tau */
            "\xcf\x86",  /* phi */
            "\xcf\x87",  /* chi */
            "\xcf\x88",  /* psi */
            "\xcf\x89",  /* omega */
            "\xe2\x97\x8a",  /* diamond */
            "\xe2\x96\xb3",  /* triangle up */
            "\xe2\x96\xa1",  /* square */
            "\xe2\x97\x8b",  /* circle */
            "\xe2\x97\x86",  /* filled diamond */
            "\xe2\x96\xbd",  /* triangle down */
            "\xe2\x98\x86",  /* star */
            "\xe2\x97\x8e",  /* bullseye */
            "\xe2\x88\x9e",  /* infinity */
            "\xe2\x88\x87",  /* nabla */
        };
        uint32_t base_idx = id % 32;
        uint32_t sub_num = id / 32;
        snprintf(symbol, 16, "%s%u", bases[base_idx], sub_num);
    } else {
        snprintf(symbol, 16, "T%04u", id);
    }
}

/* Initialize a single token with a given centroid vector */
static int _init_token(nimcp_emergent_token_t* tok, uint32_t id,
                        const float* centroid, uint32_t centroid_dim,
                        uint32_t embed_dim, bool use_unicode, uint32_t* rng) {
    memset(tok, 0, sizeof(*tok));
    tok->id = id;
    tok->centroid_dim = centroid_dim;
    tok->embed_dim = embed_dim;

    tok->centroid = nimcp_calloc(centroid_dim, sizeof(float));
    tok->embedding = nimcp_calloc(embed_dim, sizeof(float));
    if (!tok->centroid || !tok->embedding) {
        nimcp_free(tok->centroid);
        nimcp_free(tok->embedding);
        tok->centroid = NULL;
        tok->embedding = NULL;
        return -1;
    }

    memcpy(tok->centroid, centroid, centroid_dim * sizeof(float));

    /* Initialize embedding by projecting centroid to embed_dim.
     * Simple: take first embed_dim components, scaled and with noise. */
    float norm = _vec_norm(centroid, centroid_dim);
    float scale = (norm > 1e-8f) ? (1.0f / norm) : 1.0f;
    for (uint32_t i = 0; i < embed_dim; i++) {
        float base = (i < centroid_dim) ? centroid[i] * scale : 0.0f;
        tok->embedding[i] = base + (_randf(rng) * 2.0f - 1.0f) * 0.01f;
    }

    _generate_symbol(tok->symbol, id, use_unicode);

    tok->usage_count = 1;
    tok->activation_threshold = 0.7f;
    tok->specificity = 1.0f;

    return 0;
}

static void _free_token_data(nimcp_emergent_token_t* tok) {
    if (!tok) return;
    nimcp_free(tok->centroid);
    nimcp_free(tok->embedding);
    tok->centroid = NULL;
    tok->embedding = NULL;
}

/* ======================================================================== */
/* Lifecycle                                                                  */
/* ======================================================================== */

nimcp_emergent_config_t nimcp_emergent_config_default(void) {
    nimcp_emergent_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_tokens = 1024;
    cfg.centroid_dim = BRAIN_DIM;
    cfg.embed_dim = 256;
    cfg.discovery_threshold = 0.3f;
    cfg.merge_threshold = 0.1f;
    cfg.min_observations = 5;
    cfg.temperature = 0.5f;
    cfg.decay_rate = 0.999f;
    cfg.use_unicode_symbols = true;
    return cfg;
}

nimcp_emergent_language_t* nimcp_emergent_language_create(const nimcp_emergent_config_t* config) {
    nimcp_emergent_language_t* el = nimcp_calloc(1, sizeof(nimcp_emergent_language_t));
    if (!el) return NULL;

    el->config = config ? *config : nimcp_emergent_config_default();

    /* Validate dimensions */
    if (el->config.max_tokens == 0) el->config.max_tokens = 1024;
    if (el->config.centroid_dim == 0) el->config.centroid_dim = BRAIN_DIM;
    if (el->config.embed_dim == 0) el->config.embed_dim = 256;

    el->tokens = nimcp_calloc(el->config.max_tokens, sizeof(nimcp_emergent_token_t));
    el->translations = nimcp_calloc(el->config.max_tokens, sizeof(nimcp_emergent_translation_t));
    if (!el->tokens || !el->translations) {
        nimcp_free(el->tokens);
        nimcp_free(el->translations);
        nimcp_free(el);
        return NULL;
    }

    el->vocab_size = 0;
    el->total_observations = 0;
    el->merges_performed = 0;
    el->tokens_pruned = 0;
    el->next_symbol_id = 0;
    el->last_expressed_count = 0;
    el->rng_state = 0xDEADBEEF;

    el->lock = nimcp_mutex_create(NULL);

    LOG_INFO("[%s] Created (max_tokens=%u, centroid_dim=%u, embed_dim=%u, "
             "discovery=%.2f, merge=%.2f)",
             LOG_MODULE, el->config.max_tokens, el->config.centroid_dim,
             el->config.embed_dim, el->config.discovery_threshold,
             el->config.merge_threshold);
    return el;
}

void nimcp_emergent_language_destroy(nimcp_emergent_language_t* el) {
    if (!el) return;

    for (uint32_t i = 0; i < el->vocab_size; i++) {
        _free_token_data(&el->tokens[i]);
    }
    nimcp_free(el->tokens);
    nimcp_free(el->translations);
    if (el->lock) nimcp_mutex_free(el->lock);
    nimcp_free(el);
}

/* ======================================================================== */
/* Core: Observe brain outputs and discover/reinforce tokens                  */
/* ======================================================================== */

int nimcp_emergent_observe(nimcp_emergent_language_t* el,
    const float* brain_output, uint32_t output_dim)
{
    if (!el || !brain_output || output_dim == 0) return -1;

    if (el->lock) nimcp_mutex_lock(el->lock);

    uint32_t cd = el->config.centroid_dim;
    uint32_t dim = (output_dim < cd) ? output_dim : cd;

    /* Normalize input for cosine comparison */
    float input_norm = _vec_norm(brain_output, dim);
    if (input_norm < 1e-10f) {
        if (el->lock) nimcp_mutex_unlock(el->lock);
        return 0; /* Skip near-zero outputs */
    }

    /* Find best matching existing token */
    float best_sim = -2.0f;
    uint32_t best_idx = UINT32_MAX;

    for (uint32_t i = 0; i < el->vocab_size; i++) {
        if (!el->tokens[i].centroid) continue;
        float sim = _cosine_similarity(brain_output, el->tokens[i].centroid, dim);
        if (sim > best_sim) {
            best_sim = sim;
            best_idx = i;
        }
    }

    float reinforce_threshold = 1.0f - el->config.discovery_threshold;

    if (best_idx != UINT32_MAX && best_sim > reinforce_threshold) {
        /* REINFORCE existing token: EMA update centroid toward brain_output */
        nimcp_emergent_token_t* tok = &el->tokens[best_idx];
        float alpha = 0.05f; /* EMA smoothing factor */

        for (uint32_t i = 0; i < dim; i++) {
            tok->centroid[i] = (1.0f - alpha) * tok->centroid[i] +
                                alpha * brain_output[i];
        }
        tok->usage_count++;

        /* Update specificity: running estimate of inverse variance.
         * Higher specificity = tighter cluster. */
        float dist_sq = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            float d = brain_output[i] - tok->centroid[i];
            dist_sq += d * d;
        }
        float variance = dist_sq / (float)dim;
        /* EMA update of inverse variance */
        float new_spec = (variance > 1e-8f) ? (1.0f / variance) : 100.0f;
        tok->specificity = 0.95f * tok->specificity + 0.05f * new_spec;

        /* Also EMA update the embedding (first embed_dim components) */
        float norm = _vec_norm(brain_output, dim);
        float scale = (norm > 1e-8f) ? (1.0f / norm) : 1.0f;
        for (uint32_t i = 0; i < tok->embed_dim && i < dim; i++) {
            tok->embedding[i] = (1.0f - alpha) * tok->embedding[i] +
                                 alpha * brain_output[i] * scale;
        }
    } else {
        /* DISCOVER new token */
        if (el->vocab_size >= el->config.max_tokens) {
            /* Vocab full — try to make room by merging similar tokens */
            if (el->lock) nimcp_mutex_unlock(el->lock);
            nimcp_emergent_merge_similar(el);
            if (el->lock) nimcp_mutex_lock(el->lock);

            /* If still full after merge, try pruning */
            if (el->vocab_size >= el->config.max_tokens) {
                if (el->lock) nimcp_mutex_unlock(el->lock);
                nimcp_emergent_prune(el);
                if (el->lock) nimcp_mutex_lock(el->lock);
            }

            /* If STILL full, cannot add */
            if (el->vocab_size >= el->config.max_tokens) {
                if (el->lock) nimcp_mutex_unlock(el->lock);
                return 0;
            }
        }

        uint32_t new_id = el->next_symbol_id++;
        uint32_t slot = el->vocab_size;

        if (_init_token(&el->tokens[slot], new_id, brain_output, dim,
                        el->config.embed_dim, el->config.use_unicode_symbols,
                        &el->rng_state) == 0) {
            el->vocab_size++;
            LOG_DEBUG("[%s] Discovered token '%s' (id=%u, vocab=%u, best_sim=%.3f)",
                      LOG_MODULE, el->tokens[slot].symbol, new_id,
                      el->vocab_size, best_sim);
            /* W12 KG emit: brain-native vocabulary growth is a rare +
             * semantically interesting event — always emit one node. */
            w12_emit_emergent_vocab_auto("discovered", new_id,
                                         el->tokens[slot].specificity);
        }
    }

    el->total_observations++;

    /* Apply usage decay to all tokens */
    if (el->total_observations % 100 == 0) {
        for (uint32_t i = 0; i < el->vocab_size; i++) {
            /* Decay is applied periodically, not every observation */
            /* usage_count is integer so we don't decay it, but we could
             * track a float "activity" score if needed */
        }
    }

    if (el->lock) nimcp_mutex_unlock(el->lock);
    return 0;
}

/* ======================================================================== */
/* Express: Convert brain state to emergent token sequence (matching pursuit) */
/* ======================================================================== */

int nimcp_emergent_express(nimcp_emergent_language_t* el,
    const float* brain_output, uint32_t output_dim,
    uint32_t* token_ids, uint32_t max_tokens)
{
    if (!el || !brain_output || !token_ids || output_dim == 0 || max_tokens == 0) return -1;

    if (el->lock) nimcp_mutex_lock(el->lock);

    uint32_t cd = el->config.centroid_dim;
    uint32_t dim = (output_dim < cd) ? output_dim : cd;

    if (el->vocab_size == 0) {
        if (el->lock) nimcp_mutex_unlock(el->lock);
        return 0;
    }

    /* Allocate residual vector */
    float* residual = nimcp_calloc(dim, sizeof(float));
    if (!residual) {
        if (el->lock) nimcp_mutex_unlock(el->lock);
        return -1;
    }
    memcpy(residual, brain_output, dim * sizeof(float));

    /* Track which tokens have already been emitted this expression */
    bool* used = nimcp_calloc(el->vocab_size, sizeof(bool));
    if (!used) {
        nimcp_free(residual);
        if (el->lock) nimcp_mutex_unlock(el->lock);
        return -1;
    }

    uint32_t num_emitted = 0;
    float residual_norm = _vec_norm(residual, dim);
    float initial_norm = residual_norm;

    /* Matching pursuit: greedily subtract best-matching centroids */
    while (num_emitted < max_tokens && residual_norm > initial_norm * 0.1f) {
        float best_sim = -2.0f;
        uint32_t best_idx = UINT32_MAX;

        for (uint32_t i = 0; i < el->vocab_size; i++) {
            if (used[i] || !el->tokens[i].centroid) continue;
            float sim = _cosine_similarity(residual, el->tokens[i].centroid, dim);
            if (sim > best_sim) {
                best_sim = sim;
                best_idx = i;
            }
        }

        /* Stop if no good match found */
        if (best_idx == UINT32_MAX || best_sim < el->tokens[best_idx].activation_threshold) {
            break;
        }

        /* Emit this token */
        token_ids[num_emitted++] = el->tokens[best_idx].id;
        used[best_idx] = true;

        /* Subtract centroid contribution from residual.
         * Scale = projection of residual onto centroid direction */
        float cent_norm = _vec_norm(el->tokens[best_idx].centroid, dim);
        float dot = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            dot += residual[i] * el->tokens[best_idx].centroid[i];
        }
        float scale = dot / (cent_norm * cent_norm + 1e-12f);

        for (uint32_t i = 0; i < dim; i++) {
            residual[i] -= scale * el->tokens[best_idx].centroid[i];
        }
        residual_norm = _vec_norm(residual, dim);
    }

    /* Update co-occurrence tracking */
    el->last_expressed_count = (num_emitted < MAX_LAST_EXPRESSED)
        ? num_emitted : MAX_LAST_EXPRESSED;
    for (uint32_t i = 0; i < el->last_expressed_count; i++) {
        el->last_expressed[i] = token_ids[i];
    }

    /* Update co-occurrence data in translations */
    for (uint32_t i = 0; i < num_emitted; i++) {
        /* Find the slot for this token in the translations array */
        for (uint32_t s = 0; s < el->vocab_size; s++) {
            if (el->tokens[s].id != token_ids[i]) continue;
            nimcp_emergent_translation_t* tr = &el->translations[s];
            tr->emergent_id = token_ids[i];
            /* Record co-occurring tokens */
            tr->num_co_occurring = 0;
            for (uint32_t j = 0; j < num_emitted && tr->num_co_occurring < 8; j++) {
                if (j != i) {
                    tr->co_occurring_ids[tr->num_co_occurring++] = token_ids[j];
                }
            }
            break;
        }
    }

    nimcp_free(residual);
    nimcp_free(used);

    if (el->lock) nimcp_mutex_unlock(el->lock);
    return (int)num_emitted;
}

/* ======================================================================== */
/* Express as symbols: Human-readable output                                  */
/* ======================================================================== */

int nimcp_emergent_express_symbols(nimcp_emergent_language_t* el,
    const float* brain_output, uint32_t output_dim,
    char* symbol_text, uint32_t max_text_len)
{
    if (!el || !brain_output || !symbol_text || max_text_len == 0) return -1;

    symbol_text[0] = '\0';

    /* First express as token IDs */
    uint32_t ids[64];
    int num_tokens = nimcp_emergent_express(el, brain_output, output_dim, ids, 64);
    if (num_tokens <= 0) return num_tokens;

    if (el->lock) nimcp_mutex_lock(el->lock);

    /* Convert token IDs to symbol strings */
    uint32_t pos = 0;
    for (int t = 0; t < num_tokens; t++) {
        /* Find the token by ID */
        const char* sym = NULL;
        for (uint32_t s = 0; s < el->vocab_size; s++) {
            if (el->tokens[s].id == ids[t]) {
                sym = el->tokens[s].symbol;
                break;
            }
        }
        if (!sym) continue;

        uint32_t sym_len = (uint32_t)strlen(sym);
        if (pos + sym_len + 2 >= max_text_len) break;

        if (pos > 0) {
            symbol_text[pos++] = ' ';
        }
        memcpy(symbol_text + pos, sym, sym_len);
        pos += sym_len;
    }
    symbol_text[pos] = '\0';

    if (el->lock) nimcp_mutex_unlock(el->lock);
    return (int)pos;
}

/* ======================================================================== */
/* Comprehend: Convert token sequence back to brain embedding                 */
/* ======================================================================== */

int nimcp_emergent_comprehend(nimcp_emergent_language_t* el,
    const uint32_t* token_ids, uint32_t num_tokens,
    float* brain_embedding, uint32_t max_dim)
{
    if (!el || !token_ids || !brain_embedding || num_tokens == 0 || max_dim == 0) return -1;

    if (el->lock) nimcp_mutex_lock(el->lock);

    uint32_t cd = el->config.centroid_dim;
    uint32_t dim = (max_dim < cd) ? max_dim : cd;

    memset(brain_embedding, 0, dim * sizeof(float));

    uint32_t matched = 0;
    for (uint32_t t = 0; t < num_tokens; t++) {
        /* Find token by ID */
        for (uint32_t s = 0; s < el->vocab_size; s++) {
            if (el->tokens[s].id == token_ids[t] && el->tokens[s].centroid) {
                uint32_t tok_dim = el->tokens[s].centroid_dim;
                uint32_t d = (dim < tok_dim) ? dim : tok_dim;
                for (uint32_t i = 0; i < d; i++) {
                    brain_embedding[i] += el->tokens[s].centroid[i];
                }
                matched++;
                break;
            }
        }
    }

    /* Average and normalize */
    if (matched > 0) {
        float inv = 1.0f / (float)matched;
        for (uint32_t i = 0; i < dim; i++) {
            brain_embedding[i] *= inv;
        }
        /* Normalize to unit length */
        float norm = _vec_norm(brain_embedding, dim);
        if (norm > 1e-8f) {
            float inv_norm = 1.0f / norm;
            for (uint32_t i = 0; i < dim; i++) {
                brain_embedding[i] *= inv_norm;
            }
        }
    }

    if (el->lock) nimcp_mutex_unlock(el->lock);
    return (int)matched;
}

/* ======================================================================== */
/* Translation: Map emergent tokens to/from human language                    */
/* ======================================================================== */

int nimcp_emergent_associate_human(nimcp_emergent_language_t* el,
    uint32_t token_id, const char* human_text, float confidence)
{
    if (!el || !human_text) return -1;

    if (el->lock) nimcp_mutex_lock(el->lock);

    /* Find token slot by ID */
    uint32_t slot = UINT32_MAX;
    for (uint32_t i = 0; i < el->vocab_size; i++) {
        if (el->tokens[i].id == token_id) {
            slot = i;
            break;
        }
    }

    if (slot == UINT32_MAX) {
        if (el->lock) nimcp_mutex_unlock(el->lock);
        return -1;
    }

    nimcp_emergent_translation_t* tr = &el->translations[slot];
    tr->emergent_id = token_id;

    /* Update translation with EMA if existing, otherwise overwrite */
    if (tr->translation_confidence > 0.0f && confidence > tr->translation_confidence) {
        /* New translation is more confident, replace */
        snprintf(tr->human_gloss, sizeof(tr->human_gloss), "%s", human_text);
        tr->translation_confidence = confidence;
    } else if (tr->translation_confidence <= 0.0f) {
        /* First translation */
        snprintf(tr->human_gloss, sizeof(tr->human_gloss), "%s", human_text);
        tr->translation_confidence = confidence;
    }
    /* else: keep existing higher-confidence translation */

    if (el->lock) nimcp_mutex_unlock(el->lock);
    return 0;
}

int nimcp_emergent_get_translation(const nimcp_emergent_language_t* el,
    uint32_t token_id, nimcp_emergent_translation_t* translation)
{
    if (!el || !translation) return -1;

    /* Find token slot by ID */
    for (uint32_t i = 0; i < el->vocab_size; i++) {
        if (el->tokens[i].id == token_id) {
            *translation = el->translations[i];
            return 0;
        }
    }
    return -1;
}

/* ======================================================================== */
/* Vocabulary management                                                      */
/* ======================================================================== */

uint32_t nimcp_emergent_get_vocab_size(const nimcp_emergent_language_t* el) {
    return el ? el->vocab_size : 0;
}

int nimcp_emergent_get_token(const nimcp_emergent_language_t* el,
    uint32_t token_id, nimcp_emergent_token_t* token_out)
{
    if (!el || !token_out) return -1;

    for (uint32_t i = 0; i < el->vocab_size; i++) {
        if (el->tokens[i].id == token_id) {
            /* Copy token data (shallow — centroid/embedding are pointers) */
            *token_out = el->tokens[i];
            return 0;
        }
    }
    return -1;
}

int nimcp_emergent_prune(nimcp_emergent_language_t* el) {
    if (!el) return -1;

    if (el->lock) nimcp_mutex_lock(el->lock);

    uint32_t min_obs = el->config.min_observations;
    uint32_t pruned = 0;

    /* Compact: walk from end, swap-remove dead tokens */
    uint32_t i = 0;
    while (i < el->vocab_size) {
        nimcp_emergent_token_t* tok = &el->tokens[i];

        /* Prune if: low usage AND has been around long enough to prove itself */
        bool should_prune = (tok->usage_count < 2) &&
                            (el->total_observations > min_obs * 10);

        if (should_prune) {
            _free_token_data(tok);

            /* Swap with last token */
            if (i < el->vocab_size - 1) {
                el->tokens[i] = el->tokens[el->vocab_size - 1];
                el->translations[i] = el->translations[el->vocab_size - 1];
            }
            el->vocab_size--;
            memset(&el->tokens[el->vocab_size], 0, sizeof(nimcp_emergent_token_t));
            memset(&el->translations[el->vocab_size], 0, sizeof(nimcp_emergent_translation_t));
            pruned++;
            /* Don't increment i — re-check the swapped-in token */
        } else {
            i++;
        }
    }

    if (pruned > 0) {
        el->tokens_pruned += pruned;
        LOG_INFO("[%s] Pruned %u tokens (vocab=%u)", LOG_MODULE, pruned, el->vocab_size);
    }

    if (el->lock) nimcp_mutex_unlock(el->lock);
    return (int)pruned;
}

int nimcp_emergent_merge_similar(nimcp_emergent_language_t* el) {
    if (!el) return -1;

    if (el->lock) nimcp_mutex_lock(el->lock);

    float merge_sim = 1.0f - el->config.merge_threshold; /* e.g., 0.9 */
    uint32_t merged = 0;

    /* O(n^2) scan for close centroids — acceptable for vocab sizes up to ~1024 */
    for (uint32_t i = 0; i < el->vocab_size && el->vocab_size > 1; i++) {
        if (!el->tokens[i].centroid) continue;

        for (uint32_t j = i + 1; j < el->vocab_size; j++) {
            if (!el->tokens[j].centroid) continue;

            uint32_t dim_i = el->tokens[i].centroid_dim;
            uint32_t dim_j = el->tokens[j].centroid_dim;
            uint32_t dim = (dim_i < dim_j) ? dim_i : dim_j;

            float sim = _cosine_similarity(el->tokens[i].centroid,
                                            el->tokens[j].centroid, dim);

            if (sim > merge_sim) {
                /* Merge j into i: weighted average by usage count */
                float wi = (float)el->tokens[i].usage_count;
                float wj = (float)el->tokens[j].usage_count;
                float total = wi + wj;
                if (total < 1.0f) total = 1.0f;

                for (uint32_t d = 0; d < dim; d++) {
                    el->tokens[i].centroid[d] =
                        (wi * el->tokens[i].centroid[d] +
                         wj * el->tokens[j].centroid[d]) / total;
                }

                /* Merge embeddings similarly */
                uint32_t ed = (el->tokens[i].embed_dim < el->tokens[j].embed_dim)
                    ? el->tokens[i].embed_dim : el->tokens[j].embed_dim;
                for (uint32_t d = 0; d < ed; d++) {
                    el->tokens[i].embedding[d] =
                        (wi * el->tokens[i].embedding[d] +
                         wj * el->tokens[j].embedding[d]) / total;
                }

                el->tokens[i].usage_count += el->tokens[j].usage_count;

                /* Keep the more specific symbol (higher specificity) */
                if (el->tokens[j].specificity > el->tokens[i].specificity) {
                    memcpy(el->tokens[i].symbol, el->tokens[j].symbol,
                           sizeof(el->tokens[i].symbol));
                    el->tokens[i].specificity = el->tokens[j].specificity;
                }

                /* Remove j: swap with last */
                _free_token_data(&el->tokens[j]);
                if (j < el->vocab_size - 1) {
                    el->tokens[j] = el->tokens[el->vocab_size - 1];
                    el->translations[j] = el->translations[el->vocab_size - 1];
                }
                el->vocab_size--;
                memset(&el->tokens[el->vocab_size], 0, sizeof(nimcp_emergent_token_t));
                memset(&el->translations[el->vocab_size], 0, sizeof(nimcp_emergent_translation_t));
                merged++;
                j--; /* Re-check this slot (now has a different token) */
            }
        }
    }

    if (merged > 0) {
        el->merges_performed += merged;
        LOG_INFO("[%s] Merged %u similar tokens (vocab=%u)",
                 LOG_MODULE, merged, el->vocab_size);
    }

    if (el->lock) nimcp_mutex_unlock(el->lock);
    return (int)merged;
}

/* ======================================================================== */
/* Persistence                                                                */
/* ======================================================================== */

int nimcp_emergent_save(const nimcp_emergent_language_t* el, const char* filepath) {
    if (!el || !filepath) return -1;

    FILE* f = fopen(filepath, "wb");
    if (!f) {
        LOG_ERROR("[%s] Failed to open '%s' for writing", LOG_MODULE, filepath);
        return -1;
    }

    /* Header */
    fprintf(f, "EMRG %u %u %u %u\n",
            el->vocab_size, el->config.centroid_dim,
            el->config.embed_dim, el->next_symbol_id);

    /* Config */
    fprintf(f, "CFG %u %.6f %.6f %u %.6f %.6f %d\n",
            el->config.max_tokens, el->config.discovery_threshold,
            el->config.merge_threshold, el->config.min_observations,
            el->config.temperature, el->config.decay_rate,
            el->config.use_unicode_symbols ? 1 : 0);

    /* Stats */
    fprintf(f, "STAT %u %u %u\n",
            el->total_observations, el->merges_performed, el->tokens_pruned);

    /* Tokens */
    for (uint32_t i = 0; i < el->vocab_size; i++) {
        const nimcp_emergent_token_t* tok = &el->tokens[i];
        fprintf(f, "TOK %u %s %u %.6f %.6f\n",
                tok->id, tok->symbol, tok->usage_count,
                tok->activation_threshold, tok->specificity);

        /* Centroid */
        fprintf(f, "CEN %u\n", tok->centroid_dim);
        if (tok->centroid) {
            for (uint32_t j = 0; j < tok->centroid_dim; j++) {
                fprintf(f, "%.6f ", tok->centroid[j]);
                if ((j + 1) % 16 == 0) fprintf(f, "\n");
            }
            if (tok->centroid_dim % 16 != 0) fprintf(f, "\n");
        }

        /* Embedding */
        fprintf(f, "EMB %u\n", tok->embed_dim);
        if (tok->embedding) {
            for (uint32_t j = 0; j < tok->embed_dim; j++) {
                fprintf(f, "%.6f ", tok->embedding[j]);
                if ((j + 1) % 16 == 0) fprintf(f, "\n");
            }
            if (tok->embed_dim % 16 != 0) fprintf(f, "\n");
        }

        /* Translation */
        const nimcp_emergent_translation_t* tr = &el->translations[i];
        fprintf(f, "TRL %.6f %u %s\n",
                tr->translation_confidence, tr->num_co_occurring,
                (tr->human_gloss[0] != '\0') ? tr->human_gloss : "<none>");
    }

    fclose(f);
    LOG_INFO("[%s] Saved %u tokens to '%s'", LOG_MODULE, el->vocab_size, filepath);
    return 0;
}

int nimcp_emergent_load(nimcp_emergent_language_t* el, const char* filepath) {
    if (!el || !filepath) return -1;

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        LOG_ERROR("[%s] Failed to open '%s' for reading", LOG_MODULE, filepath);
        return -1;
    }

    /* Clear existing vocab */
    for (uint32_t i = 0; i < el->vocab_size; i++) {
        _free_token_data(&el->tokens[i]);
    }
    el->vocab_size = 0;

    /* Header */
    uint32_t vs, cd, ed, next_id;
    if (fscanf(f, "EMRG %u %u %u %u\n", &vs, &cd, &ed, &next_id) != 4) {
        fclose(f);
        return -1;
    }
    el->next_symbol_id = next_id;

    /* Config */
    uint32_t max_tok, min_obs;
    float disc, merge, temp, decay;
    int use_uni;
    if (fscanf(f, "CFG %u %f %f %u %f %f %d\n",
               &max_tok, &disc, &merge, &min_obs, &temp, &decay, &use_uni) == 7) {
        /* Only update non-structural config (keep max_tokens from creation) */
        el->config.discovery_threshold = disc;
        el->config.merge_threshold = merge;
        el->config.min_observations = min_obs;
        el->config.temperature = temp;
        el->config.decay_rate = decay;
        el->config.use_unicode_symbols = (use_uni != 0);
    }

    /* Stats */
    uint32_t obs, merges, pruned;
    if (fscanf(f, "STAT %u %u %u\n", &obs, &merges, &pruned) == 3) {
        el->total_observations = obs;
        el->merges_performed = merges;
        el->tokens_pruned = pruned;
    }

    /* Tokens */
    for (uint32_t i = 0; i < vs && el->vocab_size < el->config.max_tokens; i++) {
        uint32_t id, usage;
        char symbol[16];
        float thresh, spec;
        if (fscanf(f, "TOK %u %15s %u %f %f\n",
                   &id, symbol, &usage, &thresh, &spec) != 5) break;

        /* Centroid */
        uint32_t c_dim;
        if (fscanf(f, "CEN %u\n", &c_dim) != 1) break;
        uint32_t actual_cd = (c_dim < el->config.centroid_dim) ? c_dim : el->config.centroid_dim;
        float* centroid = nimcp_calloc(actual_cd, sizeof(float));
        if (!centroid) break;
        for (uint32_t j = 0; j < c_dim; j++) {
            float v;
            if (fscanf(f, "%f", &v) != 1) break;
            if (j < actual_cd) centroid[j] = v;
        }

        /* Embedding */
        uint32_t e_dim;
        if (fscanf(f, " EMB %u\n", &e_dim) != 1) {
            nimcp_free(centroid);
            break;
        }
        uint32_t actual_ed = (e_dim < el->config.embed_dim) ? e_dim : el->config.embed_dim;
        float* embedding = nimcp_calloc(actual_ed, sizeof(float));
        if (!embedding) {
            nimcp_free(centroid);
            break;
        }
        for (uint32_t j = 0; j < e_dim; j++) {
            float v;
            if (fscanf(f, "%f", &v) != 1) break;
            if (j < actual_ed) embedding[j] = v;
        }

        /* Translation */
        float trl_conf;
        uint32_t trl_nco;
        char trl_gloss[128];
        if (fscanf(f, " TRL %f %u %127s\n", &trl_conf, &trl_nco, trl_gloss) == 3) {
            el->translations[el->vocab_size].emergent_id = id;
            el->translations[el->vocab_size].translation_confidence = trl_conf;
            el->translations[el->vocab_size].num_co_occurring = 0;
            if (strcmp(trl_gloss, "<none>") != 0) {
                snprintf(el->translations[el->vocab_size].human_gloss,
                         sizeof(el->translations[el->vocab_size].human_gloss),
                         "%s", trl_gloss);
            }
        }

        /* Populate token slot */
        uint32_t slot = el->vocab_size;
        nimcp_emergent_token_t* tok = &el->tokens[slot];
        tok->id = id;
        tok->centroid = centroid;
        tok->centroid_dim = actual_cd;
        tok->embedding = embedding;
        tok->embed_dim = actual_ed;
        memcpy(tok->symbol, symbol, sizeof(tok->symbol));
        tok->usage_count = usage;
        tok->activation_threshold = thresh;
        tok->specificity = spec;
        el->vocab_size++;
    }

    fclose(f);
    LOG_INFO("[%s] Loaded %u tokens from '%s'", LOG_MODULE, el->vocab_size, filepath);
    return 0;
}

/* ======================================================================== */
/* Statistics                                                                 */
/* ======================================================================== */

int nimcp_emergent_get_stats(const nimcp_emergent_language_t* el,
    nimcp_emergent_stats_t* stats)
{
    if (!el || !stats) return -1;

    memset(stats, 0, sizeof(*stats));
    stats->vocab_size = el->vocab_size;
    stats->total_observations = el->total_observations;
    stats->merges_performed = el->merges_performed;
    stats->tokens_pruned = el->tokens_pruned;

    /* Count active tokens (used at least once in last ACTIVE_WINDOW observations) */
    uint32_t active = 0;
    float total_specificity = 0.0f;

    for (uint32_t i = 0; i < el->vocab_size; i++) {
        if (el->tokens[i].usage_count > 0) {
            active++;
        }
        total_specificity += el->tokens[i].specificity;
    }

    stats->active_tokens = active;
    stats->mean_specificity = (el->vocab_size > 0)
        ? total_specificity / (float)el->vocab_size : 0.0f;

    /* Coverage: estimate as fraction of max_tokens used */
    stats->coverage = (el->config.max_tokens > 0)
        ? (float)el->vocab_size / (float)el->config.max_tokens : 0.0f;

    return 0;
}
