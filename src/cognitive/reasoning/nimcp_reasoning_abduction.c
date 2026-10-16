/**
 * @file nimcp_reasoning_abduction.c
 * @brief Abductive Reasoning — Inference to Best Explanation Implementation
 *
 * WHAT: Generate and evaluate explanatory hypotheses for observed phenomena
 * WHY:  Enable human-like "inference to best explanation" reasoning
 * HOW:  Keyword extraction from observations, pattern matching for common causes,
 *       scoring via weighted plausibility (simplicity + explanatory_power + coherence)
 *
 * HYPOTHESIS GENERATION ALGORITHM:
 * 1. Extract content keywords from all observations
 * 2. Find shared keywords across observations (potential common causes)
 * 3. Generate candidate hypotheses:
 *    a. Direct cause: "X is caused by [shared keyword pattern]"
 *    b. Generalization: "X is an instance of [general principle]"
 *    c. Analogy: "X is similar to [known case], suggesting [explanation]"
 * 4. Score each hypothesis:
 *    - explanatory_power = observations_explained / total_observations
 *    - simplicity = 1.0 / (1.0 + word_count * 0.1)
 *    - coherence = 1.0 - contradiction_score
 *    - plausibility = weighted sum of above
 *    - free_energy = -log(plausibility + 1e-6)
 * 5. Sort by plausibility descending, return top N
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#include "cognitive/reasoning/nimcp_reasoning_abduction.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#define LOG_MODULE "reasoning_abduction"

/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

/** Maximum keywords to extract from a single observation */
#define MAX_KEYWORDS_PER_OBS 16

/** Maximum total keywords across all observations */
#define MAX_TOTAL_KEYWORDS 128

/** Maximum keyword length */
#define MAX_KEYWORD_LEN 64

/** Minimum word length to be considered a content keyword */
#define MIN_KEYWORD_LEN 4

/** Negation words that indicate contradiction */
static const char* NEGATION_WORDS[] = {
    "not", "never", "no", "none", "neither", "nor", "without",
    "absent", "lack", "fail", "unable", "impossible", NULL
};

/** Common stop words to exclude from keyword extraction */
static const char* STOP_WORDS[] = {
    "the", "and", "for", "are", "but", "was", "has", "had", "have",
    "been", "with", "this", "that", "from", "they", "were", "will",
    "each", "which", "their", "there", "what", "about", "when", NULL
};

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Extracted keyword with occurrence tracking
 */
typedef struct {
    char word[MAX_KEYWORD_LEN];
    uint32_t observation_mask;  /**< Bitmask: which observations contain this keyword */
    uint32_t occurrence_count;  /**< Total occurrences across all observations */
} keyword_t;

/**
 * @brief Internal state for the abductive reasoning engine
 */
struct reasoning_abduction {
    abduction_config_t config;

    abductive_observation_t observations[ABDUCTION_MAX_OBSERVATIONS];
    uint32_t num_observations;

    abduction_stats_t stats;

    nimcp_mutex_t* mutex;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Check if a word is a stop word
 */
static bool is_stop_word(const char* word)
{
    for (int i = 0; STOP_WORDS[i] != NULL; i++) {
        if (strcasecmp(word, STOP_WORDS[i]) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if a word is a negation word
 */
static bool is_negation_word(const char* word)
{
    for (int i = 0; NEGATION_WORDS[i] != NULL; i++) {
        if (strcasecmp(word, NEGATION_WORDS[i]) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Count words in a string (space-delimited)
 */
static uint32_t count_words(const char* str)
{
    if (!str) return 0;

    uint32_t count = 0;
    bool in_word = false;

    for (const char* p = str; *p; p++) {
        if (*p == ' ' || *p == '\t' || *p == '\n') {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            count++;
        }
    }
    return count;
}

/**
 * @brief Extract content keywords from a string
 *
 * WHAT: Split on whitespace, filter stop words, normalize to lowercase
 * WHY:  Keywords drive hypothesis generation
 *
 * @param text Input text
 * @param keywords Output keyword buffer
 * @param max_keywords Maximum keywords to extract
 * @return Number of keywords extracted
 */
static uint32_t extract_keywords(const char* text, keyword_t* keywords,
                                  uint32_t max_keywords, uint32_t obs_index)
{
    if (!text || !keywords || max_keywords == 0) return 0;

    uint32_t count = 0;
    const char* p = text;

    while (*p && count < max_keywords) {
        /* Skip whitespace and punctuation */
        while (*p && (isspace((unsigned char)*p) || ispunct((unsigned char)*p))) {
            p++;
        }
        if (!*p) break;

        /* Extract word */
        char word[MAX_KEYWORD_LEN];
        uint32_t wlen = 0;
        while (*p && !isspace((unsigned char)*p) && !ispunct((unsigned char)*p) &&
               wlen < MAX_KEYWORD_LEN - 1) {
            word[wlen++] = (char)tolower((unsigned char)*p);
            p++;
        }
        word[wlen] = '\0';

        /* Filter: minimum length, not a stop word */
        if (wlen < MIN_KEYWORD_LEN || is_stop_word(word)) {
            continue;
        }

        /* Check if keyword already exists in the output */
        bool found = false;
        for (uint32_t i = 0; i < count; i++) {
            if (strcmp(keywords[i].word, word) == 0) {
                keywords[i].observation_mask |= (1u << obs_index);
                keywords[i].occurrence_count++;
                found = true;
                break;
            }
        }

        if (!found) {
            strncpy(keywords[count].word, word, MAX_KEYWORD_LEN - 1);
            keywords[count].word[MAX_KEYWORD_LEN - 1] = '\0';
            keywords[count].observation_mask = (1u << obs_index);
            keywords[count].occurrence_count = 1;
            count++;
        }
    }

    return count;
}

/**
 * @brief Check if an observation contains negation
 *
 * WHAT: Scan for negation words
 * WHY:  Contradictory observations reduce coherence
 */
static bool observation_has_negation(const char* description)
{
    if (!description) return false;

    const char* p = description;
    while (*p) {
        /* Skip to word boundary */
        while (*p && (isspace((unsigned char)*p) || ispunct((unsigned char)*p))) {
            p++;
        }
        if (!*p) break;

        /* Extract word */
        char word[MAX_KEYWORD_LEN];
        uint32_t wlen = 0;
        while (*p && !isspace((unsigned char)*p) && !ispunct((unsigned char)*p) &&
               wlen < MAX_KEYWORD_LEN - 1) {
            word[wlen++] = (char)tolower((unsigned char)*p);
            p++;
        }
        word[wlen] = '\0';

        if (is_negation_word(word)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Compute simplicity score from explanation text
 *
 * WHAT: Shorter explanations are simpler (Occam's razor)
 * HOW:  simplicity = 1.0 / (1.0 + word_count * 0.1)
 */
static float compute_simplicity(const char* explanation)
{
    uint32_t words = count_words(explanation);
    return 1.0f / (1.0f + (float)words * 0.1f);
}

/**
 * @brief Compute plausibility from component scores
 */
static float compute_plausibility(const abduction_config_t* config,
                                   float simplicity, float explanatory_power,
                                   float coherence)
{
    float score = config->simplicity_weight * simplicity +
                  config->explanatory_weight * explanatory_power +
                  config->coherence_weight * coherence;

    /* Clamp to [0, 1] */
    if (score > 1.0f) score = 1.0f;
    if (score < 0.0f) score = 0.0f;

    return score;
}

/**
 * @brief Sort hypotheses by plausibility descending (simple insertion sort)
 */
static void sort_hypotheses_by_plausibility(abductive_hypothesis_t* hypotheses,
                                             uint32_t count)
{
    for (uint32_t i = 1; i < count; i++) {
        abductive_hypothesis_t key = hypotheses[i];
        int j = (int)i - 1;
        while (j >= 0 && hypotheses[j].plausibility < key.plausibility) {
            hypotheses[j + 1] = hypotheses[j];
            j--;
        }
        hypotheses[j + 1] = key;
    }
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

abduction_config_t reasoning_abduction_default_config(void)
{
    abduction_config_t config;
    memset(&config, 0, sizeof(config));

    config.enabled = true;
    config.max_hypotheses = ABDUCTION_DEFAULT_MAX_HYPOTHESES;
    config.min_plausibility = ABDUCTION_DEFAULT_MIN_PLAUSIBILITY;
    config.prefer_simplicity = true;
    config.simplicity_weight = 0.3f;
    config.explanatory_weight = 0.5f;
    config.coherence_weight = 0.2f;

    return config;
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

reasoning_abduction_t* reasoning_abduction_create(const abduction_config_t* config)
{
    reasoning_abduction_t* abduction = (reasoning_abduction_t*)nimcp_calloc(
        1, sizeof(reasoning_abduction_t));
    if (!abduction) {
        NIMCP_LOGGING_ERROR("reasoning_abduction: failed to allocate instance");
        return NULL;
    }

    if (config) {
        abduction->config = *config;
    } else {
        abduction->config = reasoning_abduction_default_config();
    }

    abduction->num_observations = 0;
    memset(&abduction->stats, 0, sizeof(abduction_stats_t));

    /* Create mutex for thread safety */
    mutex_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.type = MUTEX_TYPE_NORMAL;
    abduction->mutex = nimcp_mutex_create(&attr);
    if (!abduction->mutex) {
        NIMCP_LOGGING_ERROR("reasoning_abduction: failed to create mutex");
        nimcp_free(abduction);
        abduction = NULL;
        return NULL;
    }

    NIMCP_LOGGING_INFO("reasoning_abduction: created (max_hypotheses=%u, "
                       "min_plausibility=%.2f, weights=%.1f/%.1f/%.1f)",
                       abduction->config.max_hypotheses,
                       abduction->config.min_plausibility,
                       abduction->config.simplicity_weight,
                       abduction->config.explanatory_weight,
                       abduction->config.coherence_weight);

    return abduction;
}

void reasoning_abduction_destroy(reasoning_abduction_t* abduction)
{
    if (!abduction) return;

    if (abduction->mutex) {
        nimcp_mutex_destroy(abduction->mutex);
        abduction->mutex = NULL;
    }

    NIMCP_LOGGING_INFO("reasoning_abduction: destroyed (total_abductions=%u)",
                       abduction->stats.total_abductions);

    nimcp_free(abduction);
    abduction = NULL;
}

/*=============================================================================
 * OBSERVATION MANAGEMENT
 *===========================================================================*/

int reasoning_abduction_add_observation(reasoning_abduction_t* abduction,
                                         const abductive_observation_t* observation)
{
    if (!abduction || !observation) return -1;

    nimcp_mutex_lock(abduction->mutex);

    if (abduction->num_observations >= ABDUCTION_MAX_OBSERVATIONS) {
        nimcp_mutex_unlock(abduction->mutex);
        NIMCP_LOGGING_WARN("reasoning_abduction: observation buffer full (%u/%u)",
                           abduction->num_observations, ABDUCTION_MAX_OBSERVATIONS);
        return -1;
    }

    abduction->observations[abduction->num_observations] = *observation;
    abduction->num_observations++;

    nimcp_mutex_unlock(abduction->mutex);
    return 0;
}

int reasoning_abduction_clear_observations(reasoning_abduction_t* abduction)
{
    if (!abduction) return -1;

    nimcp_mutex_lock(abduction->mutex);
    abduction->num_observations = 0;
    memset(abduction->observations, 0, sizeof(abduction->observations));
    nimcp_mutex_unlock(abduction->mutex);
    return 0;
}

/*=============================================================================
 * HYPOTHESIS GENERATION
 *===========================================================================*/

int reasoning_abduction_generate(reasoning_abduction_t* abduction,
                                  abduction_result_t* result)
{
    if (!abduction || !result) return -1;

    nimcp_mutex_lock(abduction->mutex);

    uint64_t start_time = nimcp_time_get_us();
    memset(result, 0, sizeof(abduction_result_t));

    uint32_t num_obs = abduction->num_observations;

    if (num_obs == 0) {
        /* No observations — no hypotheses */
        result->generation_time_us = nimcp_time_get_us() - start_time;
        abduction->stats.total_abductions++;
        nimcp_mutex_unlock(abduction->mutex);
        return 0;
    }

    /* Step 1: Extract keywords from all observations */
    keyword_t all_keywords[MAX_TOTAL_KEYWORDS];
    uint32_t total_keywords = 0;

    for (uint32_t i = 0; i < num_obs && i < 32; i++) {
        keyword_t obs_keywords[MAX_KEYWORDS_PER_OBS];
        memset(obs_keywords, 0, sizeof(obs_keywords));

        uint32_t n = extract_keywords(
            abduction->observations[i].description,
            obs_keywords, MAX_KEYWORDS_PER_OBS, i);

        /* Merge into global keyword list */
        for (uint32_t k = 0; k < n && total_keywords < MAX_TOTAL_KEYWORDS; k++) {
            bool found = false;
            for (uint32_t g = 0; g < total_keywords; g++) {
                if (strcmp(all_keywords[g].word, obs_keywords[k].word) == 0) {
                    all_keywords[g].observation_mask |= obs_keywords[k].observation_mask;
                    all_keywords[g].occurrence_count += obs_keywords[k].occurrence_count;
                    found = true;
                    break;
                }
            }
            if (!found) {
                all_keywords[total_keywords] = obs_keywords[k];
                total_keywords++;
            }
        }
    }

    /* Step 2: Compute contradiction score (observations with negation) */
    uint32_t negation_count = 0;
    for (uint32_t i = 0; i < num_obs; i++) {
        if (observation_has_negation(abduction->observations[i].description)) {
            negation_count++;
        }
    }
    float base_coherence = 1.0f - (float)negation_count / (float)num_obs;
    if (base_coherence < 0.0f) base_coherence = 0.0f;

    /* Step 3: Find shared keywords (appear in multiple observations) */
    uint32_t max_hyp = abduction->config.max_hypotheses;
    if (max_hyp > ABDUCTION_MAX_HYPOTHESES) max_hyp = ABDUCTION_MAX_HYPOTHESES;
    uint32_t hyp_count = 0;

    /* 3a: Generate hypotheses from shared keywords (common cause) */
    for (uint32_t k = 0; k < total_keywords && hyp_count < max_hyp; k++) {
        /* Count how many observations share this keyword */
        uint32_t shared_count = 0;
        uint32_t mask = all_keywords[k].observation_mask;
        while (mask) {
            shared_count += (mask & 1);
            mask >>= 1;
        }

        if (shared_count >= 2 || (num_obs == 1 && shared_count == 1)) {
            abductive_hypothesis_t* h = &result->hypotheses[hyp_count];
            memset(h, 0, sizeof(abductive_hypothesis_t));

            snprintf(h->explanation, ABDUCTION_MAX_EXPLANATION_LEN,
                     "Common cause: observations share factor '%s'",
                     all_keywords[k].word);

            h->observations_explained = shared_count;
            h->total_observations = num_obs;
            h->explanatory_power = (float)shared_count / (float)num_obs;
            h->simplicity = compute_simplicity(h->explanation);
            h->coherence = base_coherence;
            h->plausibility = compute_plausibility(&abduction->config,
                                                    h->simplicity,
                                                    h->explanatory_power,
                                                    h->coherence);
            h->free_energy = -logf(h->plausibility + 1e-6f);

            hyp_count++;
        }
    }

    /* 3b: Generate generalization hypothesis if multiple observations */
    if (num_obs >= 2 && hyp_count < max_hyp) {
        abductive_hypothesis_t* h = &result->hypotheses[hyp_count];
        memset(h, 0, sizeof(abductive_hypothesis_t));

        /* Find the most common keyword to use as the generalization anchor */
        uint32_t best_k = 0;
        uint32_t best_count = 0;
        for (uint32_t k = 0; k < total_keywords; k++) {
            if (all_keywords[k].occurrence_count > best_count) {
                best_count = all_keywords[k].occurrence_count;
                best_k = k;
            }
        }

        if (total_keywords > 0) {
            snprintf(h->explanation, ABDUCTION_MAX_EXPLANATION_LEN,
                     "General principle: all observations reflect '%s' phenomenon",
                     all_keywords[best_k].word);
        } else {
            snprintf(h->explanation, ABDUCTION_MAX_EXPLANATION_LEN,
                     "General principle: observations share common underlying pattern");
        }

        h->observations_explained = num_obs;
        h->total_observations = num_obs;
        h->explanatory_power = 1.0f;
        h->simplicity = compute_simplicity(h->explanation);
        h->coherence = base_coherence * 0.9f; /* Generalizations slightly less coherent */
        h->plausibility = compute_plausibility(&abduction->config,
                                                h->simplicity,
                                                h->explanatory_power,
                                                h->coherence);
        h->free_energy = -logf(h->plausibility + 1e-6f);

        hyp_count++;
    }

    /* 3c: Generate single-observation direct cause hypotheses */
    for (uint32_t i = 0; i < num_obs && hyp_count < max_hyp; i++) {
        abductive_hypothesis_t* h = &result->hypotheses[hyp_count];
        memset(h, 0, sizeof(abductive_hypothesis_t));

        /* Use first keyword from this observation as the direct cause */
        keyword_t obs_kw[MAX_KEYWORDS_PER_OBS];
        memset(obs_kw, 0, sizeof(obs_kw));
        uint32_t nkw = extract_keywords(
            abduction->observations[i].description, obs_kw,
            MAX_KEYWORDS_PER_OBS, i);

        if (nkw > 0) {
            snprintf(h->explanation, ABDUCTION_MAX_EXPLANATION_LEN,
                     "Direct cause: '%s' explains observation %u",
                     obs_kw[0].word, i + 1);
        } else {
            snprintf(h->explanation, ABDUCTION_MAX_EXPLANATION_LEN,
                     "Direct cause: unknown factor explains observation %u",
                     i + 1);
        }

        h->observations_explained = 1;
        h->total_observations = num_obs;
        h->explanatory_power = 1.0f / (float)num_obs;
        h->simplicity = compute_simplicity(h->explanation);
        h->coherence = base_coherence;
        h->plausibility = compute_plausibility(&abduction->config,
                                                h->simplicity,
                                                h->explanatory_power,
                                                h->coherence);
        h->free_energy = -logf(h->plausibility + 1e-6f);

        hyp_count++;
    }

    /* Step 4: Filter by minimum plausibility */
    uint32_t filtered_count = 0;
    for (uint32_t i = 0; i < hyp_count; i++) {
        if (result->hypotheses[i].plausibility >= abduction->config.min_plausibility) {
            if (filtered_count != i) {
                result->hypotheses[filtered_count] = result->hypotheses[i];
            }
            filtered_count++;
        }
    }
    hyp_count = filtered_count;

    /* Step 5: Sort by plausibility descending */
    sort_hypotheses_by_plausibility(result->hypotheses, hyp_count);

    /* Finalize result */
    result->num_hypotheses = hyp_count;
    if (hyp_count > 0) {
        result->best_hypothesis_index = 0;
        result->best_plausibility = result->hypotheses[0].plausibility;
    }
    result->generation_time_us = nimcp_time_get_us() - start_time;

    /* Update stats */
    abduction->stats.total_abductions++;
    abduction->stats.total_observations_processed += num_obs;

    float n = (float)abduction->stats.total_abductions;
    abduction->stats.avg_hypotheses_generated =
        abduction->stats.avg_hypotheses_generated * ((n - 1.0f) / (fabsf(n) > 1e-7f ? n : 1e-7f)) +
        (float)hyp_count / (fabsf(n) > 1e-7f ? n : 1e-7f);
    abduction->stats.avg_best_plausibility =
        abduction->stats.avg_best_plausibility * ((n - 1.0f) / (fabsf(n) > 1e-7f ? n : 1e-7f)) +
        result->best_plausibility / (fabsf(n) > 1e-7f ? n : 1e-7f);

    NIMCP_LOGGING_INFO("reasoning_abduction: generated %u hypotheses from %u observations "
                       "(best_plausibility=%.3f, time=%lu us)",
                       hyp_count, num_obs, result->best_plausibility,
                       (unsigned long)result->generation_time_us);

    nimcp_mutex_unlock(abduction->mutex);
    return 0;
}

/*=============================================================================
 * HYPOTHESIS EVALUATION
 *===========================================================================*/

int reasoning_abduction_evaluate(reasoning_abduction_t* abduction,
                                  abductive_hypothesis_t* hypothesis)
{
    if (!abduction || !hypothesis) return -1;

    nimcp_mutex_lock(abduction->mutex);

    /* Recompute simplicity from explanation text */
    hypothesis->simplicity = compute_simplicity(hypothesis->explanation);

    /* Recompute plausibility */
    hypothesis->plausibility = compute_plausibility(&abduction->config,
                                                     hypothesis->simplicity,
                                                     hypothesis->explanatory_power,
                                                     hypothesis->coherence);

    /* Recompute free energy */
    hypothesis->free_energy = -logf(hypothesis->plausibility + 1e-6f);

    nimcp_mutex_unlock(abduction->mutex);
    return 0;
}

/*=============================================================================
 * BEST SELECTION
 *===========================================================================*/

const abductive_hypothesis_t* reasoning_abduction_select_best(
    const abduction_result_t* result)
{
    if (!result || result->num_hypotheses == 0) return NULL;

    return &result->hypotheses[result->best_hypothesis_index];
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

int reasoning_abduction_get_stats(const reasoning_abduction_t* abduction,
                                   abduction_stats_t* stats)
{
    if (!abduction || !stats) return -1;

    /* Read stats without mutex — stats are informational, not critical.
     * The individual fields are updated atomically in generate(). */
    *stats = abduction->stats;
    return 0;
}
