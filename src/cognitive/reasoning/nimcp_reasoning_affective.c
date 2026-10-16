/**
 * @file nimcp_reasoning_affective.c
 * @brief Affective Modulation for Convergent Reasoning — Implementation
 *
 * WHAT: Emotion-based confidence modulation using keyword analysis
 * WHY:  Biological brains use emotional state to modulate reasoning certainty
 * HOW:  Case-insensitive keyword matching → intensity → weighted delta
 *
 * KEYWORD CATEGORIES:
 *   Grief:   "loss", "death", "grief", "missing", "gone", "farewell"
 *   Joy:     "happy", "success", "achievement", "celebrate", "win", "great"
 *   Remorse: "wrong", "mistake", "regret", "sorry", "fault", "guilt"
 *   Social:  "friend", "family", "team", "together", "trust", "loyalty"
 *   Shadow:  "hidden", "suppress", "deny", "avoid", "fear", "anger"
 *   Bias:    "fair", "equal", "biased", "discriminat", "prejudice", "stereotype"
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#include "cognitive/reasoning/nimcp_reasoning_affective.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Case-insensitive substring search
 *
 * WHAT: Find needle in haystack, ignoring case
 * WHY:  Keywords should match regardless of capitalization
 * HOW:  Convert both to lowercase during comparison
 *
 * @param haystack String to search in
 * @param needle Substring to find
 * @return Pointer to first match, or NULL if not found
 */
static const char* strcasestr_portable(const char* haystack, const char* needle)
{
    if (!haystack || !needle) return NULL;
    if (*needle == '\0') return haystack;

    size_t needle_len = strlen(needle);
    size_t haystack_len = strlen(haystack);

    if (needle_len > haystack_len) return NULL;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        bool match = true;
        for (size_t j = 0; j < needle_len; j++) {
            if (tolower((unsigned char)haystack[i + j]) !=
                tolower((unsigned char)needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return &haystack[i];
    }
    return NULL;
}

/**
 * @brief Count keyword matches in a query string
 *
 * WHAT: Count how many keywords from a list appear in the query
 * WHY:  More keyword matches → higher emotional intensity
 * HOW:  Case-insensitive search for each keyword
 *
 * @param query Query string to search
 * @param keywords NULL-terminated array of keyword strings
 * @return Number of keywords found
 */
static uint32_t count_keyword_matches(const char* query, const char* const* keywords)
{
    if (!query || !keywords) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; keywords[i] != NULL; i++) {
        if (strcasestr_portable(query, keywords[i]) != NULL) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Convert keyword match count to intensity [0, 1]
 *
 * WHAT: Map discrete match count to continuous intensity
 * WHY:  Normalize to standard [0, 1] range for weighting
 * HOW:  0 matches → 0.0, 1 match → 0.3, 2 → 0.5, 3 → 0.7, 4+ → 0.9
 */
static float matches_to_intensity(uint32_t matches)
{
    switch (matches) {
        case 0: return 0.0f;
        case 1: return 0.3f;
        case 2: return 0.5f;
        case 3: return 0.7f;
        default: return 0.9f;  /* 4+ matches → high intensity */
    }
}

/*=============================================================================
 * KEYWORD TABLES
 *===========================================================================*/

static const char* const s_grief_keywords[] = {
    "loss", "death", "grief", "missing", "gone", "farewell", NULL
};

static const char* const s_joy_keywords[] = {
    "happy", "success", "achievement", "celebrate", "win", "great", NULL
};

static const char* const s_remorse_keywords[] = {
    "wrong", "mistake", "regret", "sorry", "fault", "guilt", NULL
};

static const char* const s_social_keywords[] = {
    "friend", "family", "team", "together", "trust", "loyalty", NULL
};

static const char* const s_shadow_keywords[] = {
    "hidden", "suppress", "deny", "avoid", "fear", "anger", NULL
};

static const char* const s_bias_keywords[] = {
    "fair", "equal", "biased", "discriminat", "prejudice", "stereotype", NULL
};

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

affective_config_t reasoning_affective_default_config(void)
{
    affective_config_t config;
    memset(&config, 0, sizeof(config));

    config.enable_affective_modulation = true;
    config.grief_weight = AFFECTIVE_DEFAULT_GRIEF_WEIGHT;
    config.joy_weight = AFFECTIVE_DEFAULT_JOY_WEIGHT;
    config.remorse_weight = AFFECTIVE_DEFAULT_REMORSE_WEIGHT;
    config.social_weight = AFFECTIVE_DEFAULT_SOCIAL_WEIGHT;
    config.shadow_weight = AFFECTIVE_DEFAULT_SHADOW_WEIGHT;
    config.bias_weight = AFFECTIVE_DEFAULT_BIAS_WEIGHT;
    config.intensity_threshold = 0.1f;

    return config;
}

/*=============================================================================
 * EVALUATE FUNCTIONS
 *===========================================================================*/

affective_contribution_t reasoning_affective_evaluate_grief(
    const void* grief_system, const char* query)
{
    affective_contribution_t result;
    memset(&result, 0, sizeof(result));

    if (!grief_system || !query) {
        result.influence_type = AFFECT_NONE;
        return result;
    }

    uint32_t matches = count_keyword_matches(query, s_grief_keywords);
    float intensity = matches_to_intensity(matches);

    result.influence_type = AFFECT_GRIEF;
    result.intensity = intensity;
    result.confidence_delta = AFFECTIVE_DEFAULT_GRIEF_WEIGHT * intensity;
    snprintf(result.description, sizeof(result.description),
             "Grief modulation: %u keywords, intensity=%.2f",
             matches, (double)intensity);

    return result;
}

affective_contribution_t reasoning_affective_evaluate_joy(
    const void* joy_system, const char* query)
{
    affective_contribution_t result;
    memset(&result, 0, sizeof(result));

    if (!joy_system || !query) {
        result.influence_type = AFFECT_NONE;
        return result;
    }

    uint32_t matches = count_keyword_matches(query, s_joy_keywords);
    float intensity = matches_to_intensity(matches);

    result.influence_type = AFFECT_JOY;
    result.intensity = intensity;
    result.confidence_delta = AFFECTIVE_DEFAULT_JOY_WEIGHT * intensity;
    snprintf(result.description, sizeof(result.description),
             "Joy modulation: %u keywords, intensity=%.2f",
             matches, (double)intensity);

    return result;
}

affective_contribution_t reasoning_affective_evaluate_remorse(
    const void* remorse_system, const char* query)
{
    affective_contribution_t result;
    memset(&result, 0, sizeof(result));

    if (!remorse_system || !query) {
        result.influence_type = AFFECT_NONE;
        return result;
    }

    uint32_t matches = count_keyword_matches(query, s_remorse_keywords);
    float intensity = matches_to_intensity(matches);

    result.influence_type = AFFECT_REMORSE;
    result.intensity = intensity;
    result.confidence_delta = AFFECTIVE_DEFAULT_REMORSE_WEIGHT * intensity;
    snprintf(result.description, sizeof(result.description),
             "Remorse modulation: %u keywords, intensity=%.2f",
             matches, (double)intensity);

    return result;
}

affective_contribution_t reasoning_affective_evaluate_social(
    const void* social_system, const char* query)
{
    affective_contribution_t result;
    memset(&result, 0, sizeof(result));

    if (!social_system || !query) {
        result.influence_type = AFFECT_NONE;
        return result;
    }

    uint32_t matches = count_keyword_matches(query, s_social_keywords);
    float intensity = matches_to_intensity(matches);

    result.influence_type = AFFECT_SOCIAL_BOND;
    result.intensity = intensity;
    result.confidence_delta = AFFECTIVE_DEFAULT_SOCIAL_WEIGHT * intensity;
    snprintf(result.description, sizeof(result.description),
             "Social bond modulation: %u keywords, intensity=%.2f",
             matches, (double)intensity);

    return result;
}

affective_contribution_t reasoning_affective_evaluate_shadow(
    const void* shadow_system, const char* query)
{
    affective_contribution_t result;
    memset(&result, 0, sizeof(result));

    if (!shadow_system || !query) {
        result.influence_type = AFFECT_NONE;
        return result;
    }

    uint32_t matches = count_keyword_matches(query, s_shadow_keywords);
    float intensity = matches_to_intensity(matches);

    result.influence_type = AFFECT_SHADOW;
    result.intensity = intensity;
    result.confidence_delta = AFFECTIVE_DEFAULT_SHADOW_WEIGHT * intensity;
    snprintf(result.description, sizeof(result.description),
             "Shadow emotion modulation: %u keywords, intensity=%.2f",
             matches, (double)intensity);

    return result;
}

affective_contribution_t reasoning_affective_evaluate_bias(
    const void* bias_system, const char* query)
{
    affective_contribution_t result;
    memset(&result, 0, sizeof(result));

    if (!bias_system || !query) {
        result.influence_type = AFFECT_NONE;
        return result;
    }

    uint32_t matches = count_keyword_matches(query, s_bias_keywords);
    float intensity = matches_to_intensity(matches);

    result.influence_type = AFFECT_BIAS;
    result.intensity = intensity;
    result.confidence_delta = AFFECTIVE_DEFAULT_BIAS_WEIGHT * intensity;
    snprintf(result.description, sizeof(result.description),
             "Bias detection modulation: %u keywords, intensity=%.2f",
             matches, (double)intensity);

    return result;
}

/*=============================================================================
 * NET MODULATION
 *===========================================================================*/

float reasoning_affective_compute_net_modulation(
    const affective_contribution_t* contributions,
    uint32_t count,
    const affective_config_t* config)
{
    if (!contributions || count == 0) return 0.0f;

    /* Use default config if none provided */
    affective_config_t default_config;
    if (!config) {
        default_config = reasoning_affective_default_config();
        config = &default_config;
    }

    float net = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        const affective_contribution_t* c = &contributions[i];

        /* Skip contributions below intensity threshold */
        if (c->intensity < config->intensity_threshold) {
            continue;
        }

        /* Skip AFFECT_NONE contributions */
        if (c->influence_type == AFFECT_NONE) {
            continue;
        }

        net += c->confidence_delta;
    }

    /* Clamp to [-0.5, +0.5] */
    if (net > 0.5f) net = 0.5f;
    if (net < -0.5f) net = -0.5f;

    return net;
}
