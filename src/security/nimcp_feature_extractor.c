/**
 * @file nimcp_feature_extractor.c
 * @brief Feature extraction for anomaly detection
 *
 * WHAT: Extract behavioral and statistical features from input data
 * WHY:  Convert raw inputs to feature vectors for ML analysis
 * HOW:  Analyze length, entropy, character classes, n-grams, structure, timing
 *
 * FEATURES EXTRACTED:
 * 1. Input length (normalized)
 * 2. Shannon entropy
 * 3. Character class ratios (alpha, numeric, special, control)
 * 4. N-gram entropy (bigram, trigram)
 * 5. Structural features (nesting depth)
 * 6. Timing features (request rate, burst detection)
 * 7. Pattern features (repeated substrings)
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 */

#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_feature_extractor, MESH_ADAPTER_CATEGORY_SECURITY)


/*=============================================================================
 * INTERNAL CONSTANTS
 *============================================================================*/

#define MAX_NGRAM_TABLE_SIZE 65536
#define MIN_REPEAT_LENGTH 4
#define MAX_REPEAT_ANALYSIS_LENGTH 512  /* Limit O(n^2) repeat analysis */
#define TIMING_WINDOW_DEFAULT 10.0f  /* seconds */

/*=============================================================================
 * TIMING CONTEXT STRUCTURE
 *============================================================================*/

typedef struct {
    uint64_t timestamps_us[100];  /**< Circular buffer of timestamps */
    uint32_t count;               /**< Number of stored timestamps */
    uint32_t write_idx;           /**< Write index in circular buffer */
    float window_sec;             /**< Time window for rate calculation */
} timing_context_t;

/*=============================================================================
 * ENTROPY CALCULATION
 *============================================================================*/

float nimcp_calculate_entropy(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return 0.0F;
    }

    /* Count byte frequencies */
    uint32_t freq[256] = {0};
    for (size_t i = 0; i < len; i++) {
        freq[data[i]]++;
    }

    /* Calculate entropy: H = -Σ p(x) log2 p(x) */
    float entropy = 0.0F;
    for (uint32_t i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            float p = (float)freq[i] / (float)len;
            entropy -= p * log2f(p);
        }
    }

    return entropy;
}

/*=============================================================================
 * N-GRAM ANALYSIS
 *============================================================================*/

/**
 * @brief Hash function for n-grams
 */
static uint32_t ngram_hash(const uint8_t* ngram, uint32_t n) {
    uint32_t hash = 0;
    for (uint32_t i = 0; i < n; i++) {
        hash = hash * 31 + ngram[i];
    }
    return hash % MAX_NGRAM_TABLE_SIZE;
}

float nimcp_calculate_ngram_entropy(const uint8_t* data, size_t len, uint32_t n) {
    if (!data || len < n || n < 2 || n > 5) {
        return 0.0F;
    }

    /* Simple hash table for n-gram counts */
    uint32_t* freq_table = (uint32_t*)nimcp_calloc(MAX_NGRAM_TABLE_SIZE, sizeof(uint32_t));
    if (!freq_table) {
        return 0.0F;
    }

    /* Count n-grams */
    uint64_t total_ngrams = 0;
    for (size_t i = 0; i <= len - n; i++) {
        uint32_t hash = ngram_hash(&data[i], n);
        freq_table[hash]++;
        total_ngrams++;
    }

    /* Calculate entropy */
    float entropy = 0.0F;
    for (uint32_t i = 0; i < MAX_NGRAM_TABLE_SIZE; i++) {
        if (freq_table[i] > 0) {
            float p = (float)freq_table[i] / (float)total_ngrams;
            entropy -= p * log2f(p);
        }
    }

    nimcp_free(freq_table);
    return entropy;
}

/*=============================================================================
 * CHARACTER CLASS ANALYSIS
 *============================================================================*/

/**
 * @brief Analyze character classes in input
 */
static void analyze_char_classes(const uint8_t* data, size_t len,
                                  float* alpha_ratio, float* numeric_ratio,
                                  float* special_ratio, float* control_ratio) {
    if (!data || len == 0) {
        *alpha_ratio = 0.0F;
        *numeric_ratio = 0.0F;
        *special_ratio = 0.0F;
        *control_ratio = 0.0F;
        return;
    }

    uint32_t alpha_count = 0;
    uint32_t numeric_count = 0;
    uint32_t special_count = 0;
    uint32_t control_count = 0;

    for (size_t i = 0; i < len; i++) {
        uint8_t c = data[i];
        if (isalpha(c)) {
            alpha_count++;
        } else if (isdigit(c)) {
            numeric_count++;
        } else if (iscntrl(c)) {
            control_count++;
        } else {
            special_count++;
        }
    }

    *alpha_ratio = (float)alpha_count / (float)len;
    *numeric_ratio = (float)numeric_count / (float)len;
    *special_ratio = (float)special_count / (float)len;
    *control_ratio = (float)control_count / (float)len;
}

/*=============================================================================
 * STRUCTURAL ANALYSIS
 *============================================================================*/

uint32_t nimcp_detect_nesting_depth(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_detect_nesting_depth: invalid parameters");

            return 0;
    }

    uint32_t max_depth = 0;
    uint32_t current_depth = 0;

    /* Track multiple bracket types */
    uint32_t paren_depth = 0;
    uint32_t bracket_depth = 0;
    uint32_t brace_depth = 0;
    uint32_t angle_depth = 0;

    for (size_t i = 0; i < len; i++) {
        uint8_t c = data[i];

        switch (c) {
            case '(': paren_depth++; break;
            case ')': if (paren_depth > 0) paren_depth--; break;
            case '[': bracket_depth++; break;
            case ']': if (bracket_depth > 0) bracket_depth--; break;
            case '{': brace_depth++; break;
            case '}': if (brace_depth > 0) brace_depth--; break;
            case '<': angle_depth++; break;
            case '>': if (angle_depth > 0) angle_depth--; break;
        }

        current_depth = paren_depth + bracket_depth + brace_depth + angle_depth;
        if (current_depth > max_depth) {
            max_depth = current_depth;
        }
    }

    return max_depth;
}

/*=============================================================================
 * PATTERN ANALYSIS
 *============================================================================*/

float nimcp_calculate_repeat_ratio(const uint8_t* data, size_t len) {
    if (!data || len < MIN_REPEAT_LENGTH * 2) {
        return 0.0F;
    }

    uint32_t max_repeat_len = 0;
    size_t max_repeat_start = 0;

    /* Limit search length for O(n^2) algorithm performance */
    size_t search_len = len;
    if (search_len > MAX_REPEAT_ANALYSIS_LENGTH) {
        search_len = MAX_REPEAT_ANALYSIS_LENGTH;
    }

    /* Simple O(n^2) algorithm for finding longest repeated substring */
    /* For production, use suffix array or other advanced algorithm */
    for (size_t i = 0; i + MIN_REPEAT_LENGTH <= search_len; i++) {
        for (size_t j = i + 1; j + MIN_REPEAT_LENGTH <= search_len; j++) {
            /* Find match length */
            uint32_t match_len = 0;
            while (i + match_len < search_len && j + match_len < search_len &&
                   data[i + match_len] == data[j + match_len]) {
                match_len++;
            }

            if (match_len > max_repeat_len) {
                max_repeat_len = match_len;
                max_repeat_start = i;
            }
        }
    }

    /* Calculate ratio: how much of analyzed portion is repeated */
    if (max_repeat_len >= MIN_REPEAT_LENGTH) {
        /* Count total occurrences within the analyzed portion */
        uint32_t total_repeat_bytes = 0;
        for (size_t i = 0; i + max_repeat_len <= search_len; i++) {
            if (memcmp(&data[i], &data[max_repeat_start], max_repeat_len) == 0) {
                total_repeat_bytes += max_repeat_len;
                i += max_repeat_len - 1;  /* Skip past this occurrence */
            }
        }
        return (float)total_repeat_bytes / (float)search_len;
    }

    return 0.0F;
}

/*=============================================================================
 * TIMING ANALYSIS
 *============================================================================*/

/**
 * @brief Update timing context and calculate rate features
 */
static void analyze_timing(timing_context_t* ctx, float* request_rate, float* burst_score) {
    if (!ctx) {
        *request_rate = 0.0F;
        *burst_score = 0.0F;
        return;
    }

    /* Get current timestamp */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now_us = (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;

    /* Store timestamp in circular buffer */
    ctx->timestamps_us[ctx->write_idx] = now_us;
    ctx->write_idx = (ctx->write_idx + 1) % 100;
    if (ctx->count < 100) {
        ctx->count++;
    }

    if (ctx->count < 2) {
        *request_rate = 0.0F;
        *burst_score = 0.0F;
        return;
    }

    /* Calculate request rate within time window */
    uint64_t window_us = (uint64_t)(ctx->window_sec * 1000000.0F);
    uint32_t requests_in_window = 0;

    for (uint32_t i = 0; i < ctx->count; i++) {
        if (now_us - ctx->timestamps_us[i] <= window_us) {
            requests_in_window++;
        }
    }

    *request_rate = (float)requests_in_window / ctx->window_sec;

    /* Calculate burst score: variance in inter-request times */
    if (ctx->count >= 3) {
        float mean_interval = 0.0F;
        uint32_t interval_count = 0;

        for (uint32_t i = 1; i < ctx->count; i++) {
            uint32_t prev_idx = (ctx->write_idx + 100 - ctx->count + i - 1) % 100;
            uint32_t curr_idx = (ctx->write_idx + 100 - ctx->count + i) % 100;

            if (ctx->timestamps_us[curr_idx] > ctx->timestamps_us[prev_idx]) {
                uint64_t interval = ctx->timestamps_us[curr_idx] - ctx->timestamps_us[prev_idx];
                mean_interval += (float)interval;
                interval_count++;
            }
        }

        if (interval_count > 0) {
            mean_interval /= (float)interval_count;

            /* Calculate variance */
            float variance = 0.0F;
            for (uint32_t i = 1; i < ctx->count; i++) {
                uint32_t prev_idx = (ctx->write_idx + 100 - ctx->count + i - 1) % 100;
                uint32_t curr_idx = (ctx->write_idx + 100 - ctx->count + i) % 100;

                if (ctx->timestamps_us[curr_idx] > ctx->timestamps_us[prev_idx]) {
                    uint64_t interval = ctx->timestamps_us[curr_idx] - ctx->timestamps_us[prev_idx];
                    float diff = (float)interval - mean_interval;
                    variance += diff * diff;
                }
            }
            variance /= (float)interval_count;

            /* Burst score: coefficient of variation */
            if (mean_interval > 0.0F) {
                *burst_score = sqrtf(variance) / mean_interval;
            } else {
                *burst_score = 0.0F;
            }
        } else {
            *burst_score = 0.0F;
        }
    } else {
        *burst_score = 0.0F;
    }
}

/*=============================================================================
 * PUBLIC API
 *============================================================================*/

nimcp_error_t nimcp_extract_features(const void* input, size_t input_len,
                                      float* features, void* timing_context) {
    if (!input || !features) {
        return NIMCP_INVALID_PARAM;
    }

    const uint8_t* data = (const uint8_t*)input;

    /* Initialize all features to 0 */
    memset(features, 0, NIMCP_FEATURE_COUNT * sizeof(float));

    if (input_len == 0) {
        return NIMCP_SUCCESS;
    }

    /* FEATURE 0: Length (normalized to [0, 1], max 10KB) */
    float normalized_len = (float)input_len / 10240.0F;
    if (normalized_len > 1.0F) normalized_len = 1.0F;
    features[NIMCP_FEATURE_LENGTH] = normalized_len;

    /* FEATURE 1: Shannon entropy [0, 8 bits] normalized to [0, 1] */
    float entropy = nimcp_calculate_entropy(data, input_len);
    features[NIMCP_FEATURE_ENTROPY] = entropy / 8.0F;

    /* FEATURES 2-5: Character class ratios */
    analyze_char_classes(data, input_len,
                         &features[NIMCP_FEATURE_ALPHA_RATIO],
                         &features[NIMCP_FEATURE_NUMERIC_RATIO],
                         &features[NIMCP_FEATURE_SPECIAL_RATIO],
                         &features[NIMCP_FEATURE_CONTROL_RATIO]);

    /* FEATURES 6-7: N-gram entropy */
    if (input_len >= 2) {
        float bigram_entropy = nimcp_calculate_ngram_entropy(data, input_len, 2);
        /* Bigram entropy max is ~16 bits (2 bytes) */
        features[NIMCP_FEATURE_BIGRAM_ENTROPY] = bigram_entropy / 16.0F;
    }

    if (input_len >= 3) {
        float trigram_entropy = nimcp_calculate_ngram_entropy(data, input_len, 3);
        /* Trigram entropy max is ~24 bits (3 bytes) */
        features[NIMCP_FEATURE_TRIGRAM_ENTROPY] = trigram_entropy / 24.0F;
    }

    /* FEATURE 8: Nesting depth (normalized, max depth 20) */
    uint32_t nesting = nimcp_detect_nesting_depth(data, input_len);
    features[NIMCP_FEATURE_NESTING_DEPTH] = (float)nesting / 20.0F;
    if (features[NIMCP_FEATURE_NESTING_DEPTH] > 1.0F) {
        features[NIMCP_FEATURE_NESTING_DEPTH] = 1.0F;
    }

    /* FEATURES 9-10: Timing features */
    if (timing_context) {
        float request_rate, burst_score;
        analyze_timing((timing_context_t*)timing_context, &request_rate, &burst_score);

        /* Normalize request rate (max 100 req/sec) */
        features[NIMCP_FEATURE_REQUEST_RATE] = request_rate / 100.0F;
        if (features[NIMCP_FEATURE_REQUEST_RATE] > 1.0F) {
            features[NIMCP_FEATURE_REQUEST_RATE] = 1.0F;
        }

        /* Burst score is already a ratio (CoV) */
        features[NIMCP_FEATURE_BURST_SCORE] = burst_score;
        if (features[NIMCP_FEATURE_BURST_SCORE] > 1.0F) {
            features[NIMCP_FEATURE_BURST_SCORE] = 1.0F;
        }
    }

    /* FEATURE 11: Repeated substring ratio */
    features[NIMCP_FEATURE_REPEAT_RATIO] = nimcp_calculate_repeat_ratio(data, input_len);

    return NIMCP_SUCCESS;
}
