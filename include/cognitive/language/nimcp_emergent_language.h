/**
 * @file nimcp_emergent_language.h
 * @brief Emergent language — vocabulary discovered from neural activation patterns
 *
 * WHAT: A language system where tokens emerge from brain output clustering,
 *       not from human vocabulary seeds. Each "word" is a cluster centroid
 *       in the brain's 4096-dimensional thought space.
 * WHY:  The brain should develop its OWN symbols that optimally encode its
 *       internal representations, producing an alien language that reflects
 *       how the neural architecture thinks, not how humans think.
 * HOW:  Online k-means clustering of brain output vectors. New patterns that
 *       don't match existing centroids become new tokens. Similar patterns
 *       reinforce existing ones. Expression decomposes brain state into a
 *       sparse sequence of concept-tokens via matching pursuit.
 */
#ifndef NIMCP_EMERGENT_LANGUAGE_H
#define NIMCP_EMERGENT_LANGUAGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Emergent token -- discovered from neural patterns, not human words */
typedef struct {
    uint32_t id;
    float* centroid;               /* Cluster centroid in brain output space */
    uint32_t centroid_dim;
    float* embedding;              /* Learned token embedding (for generation) */
    uint32_t embed_dim;
    char symbol[16];               /* Auto-generated symbol (e.g., "xi0", "diamond3") */
    uint32_t usage_count;          /* Times this token was selected */
    float activation_threshold;    /* Min similarity to centroid to activate */
    float specificity;             /* How tight the cluster is (inverse variance) */
} nimcp_emergent_token_t;

/* Emergent language config */
typedef struct {
    uint32_t max_tokens;           /* Max vocabulary size (default 1024) */
    uint32_t centroid_dim;         /* Brain output dimension (default 4096) */
    uint32_t embed_dim;            /* Token embedding dimension (default 256) */
    float discovery_threshold;     /* Min distance from existing centroids to create new token (default 0.3) */
    float merge_threshold;         /* Max distance between centroids to merge (default 0.1) */
    uint32_t min_observations;     /* Min observations before token is "real" (default 5) */
    float temperature;             /* Sampling temperature (default 0.5 -- lower = more precise) */
    float decay_rate;              /* Unused token decay (default 0.999) */
    bool use_unicode_symbols;      /* Generate Unicode symbols for display (default true) */
} nimcp_emergent_config_t;

/* Translation to/from human language (approximate) */
typedef struct {
    uint32_t emergent_id;
    char human_gloss[128];         /* Best human approximation */
    float translation_confidence;  /* How well this translates (0 = untranslatable) */
    uint32_t co_occurring_ids[8];  /* Tokens that frequently appear together */
    uint32_t num_co_occurring;
} nimcp_emergent_translation_t;

typedef struct nimcp_emergent_language nimcp_emergent_language_t;

/* === Lifecycle === */
nimcp_emergent_language_t* nimcp_emergent_language_create(const nimcp_emergent_config_t* config);
void nimcp_emergent_language_destroy(nimcp_emergent_language_t* el);

/* === Core: Discover tokens from brain output patterns === */
/* Feed brain outputs to the language. New patterns that don't match
 * existing tokens become new tokens. Similar patterns reinforce existing ones.
 * This is how the vocabulary GROWS from experience. */
int nimcp_emergent_observe(nimcp_emergent_language_t* el,
    const float* brain_output, uint32_t output_dim);

/* === Express: Convert brain state to emergent token sequence === */
int nimcp_emergent_express(nimcp_emergent_language_t* el,
    const float* brain_output, uint32_t output_dim,
    uint32_t* token_ids, uint32_t max_tokens);

/* === Express as symbols: Human-readable (sort of) output === */
int nimcp_emergent_express_symbols(nimcp_emergent_language_t* el,
    const float* brain_output, uint32_t output_dim,
    char* symbol_text, uint32_t max_text_len);

/* === Comprehend: Convert token sequence back to brain embedding === */
int nimcp_emergent_comprehend(nimcp_emergent_language_t* el,
    const uint32_t* token_ids, uint32_t num_tokens,
    float* brain_embedding, uint32_t max_dim);

/* === Translation: Attempt to map to/from human language === */
/* Associate an emergent token with a human description based on
 * the context in which it was discovered */
int nimcp_emergent_associate_human(nimcp_emergent_language_t* el,
    uint32_t token_id, const char* human_text, float confidence);

/* Get best human translation for an emergent token */
int nimcp_emergent_get_translation(const nimcp_emergent_language_t* el,
    uint32_t token_id, nimcp_emergent_translation_t* translation);

/* === Vocabulary management === */
uint32_t nimcp_emergent_get_vocab_size(const nimcp_emergent_language_t* el);
int nimcp_emergent_get_token(const nimcp_emergent_language_t* el,
    uint32_t token_id, nimcp_emergent_token_t* token_out);
int nimcp_emergent_prune(nimcp_emergent_language_t* el);  /* Remove unused tokens */
int nimcp_emergent_merge_similar(nimcp_emergent_language_t* el); /* Merge close centroids */

/* === Persistence === */
int nimcp_emergent_save(const nimcp_emergent_language_t* el, const char* filepath);
int nimcp_emergent_load(nimcp_emergent_language_t* el, const char* filepath);

/* === Statistics === */
typedef struct {
    uint32_t vocab_size;
    uint32_t active_tokens;        /* Tokens used in last 1000 observations */
    float mean_specificity;        /* Average cluster tightness */
    float coverage;                /* Fraction of brain output space covered */
    uint32_t total_observations;
    uint32_t merges_performed;
    uint32_t tokens_pruned;
} nimcp_emergent_stats_t;

int nimcp_emergent_get_stats(const nimcp_emergent_language_t* el,
    nimcp_emergent_stats_t* stats);

nimcp_emergent_config_t nimcp_emergent_config_default(void);

#ifdef __cplusplus
}
#endif
#endif
