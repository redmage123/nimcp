#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_parietal_bridge.c - Language-Parietal Spatial Bridge
//=============================================================================

#include "utils/bridge/nimcp_bridge_base.h"
#include "language/bridges/nimcp_language_parietal_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_parietal_bridge)

#define LOG_MODULE "LANGUAGE_PARIETAL_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct language_parietal_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    language_orchestrator_t* language;
    parietal_adapter_t* parietal;
    language_parietal_config_t config;

    /* State */
    lp_parietal_state_t state;
    uint64_t last_update_ms;

    /* Spatial word mappings */
    struct {
        char word[32];
        float vector[3];  /* x, y, z spatial direction */
    } spatial_words[64];
    uint32_t spatial_word_count;

    /* Number word mappings */
    struct {
        char word[32];
        float value;
    } number_words[100];
    uint32_t number_word_count;

    /* Statistics */
    language_parietal_stats_t stats;

    /* Attention state */
    float attention_x;
    float attention_y;
};

//=============================================================================
// Static Helpers
//=============================================================================

static void init_spatial_words(language_parietal_bridge_t* bridge) {
    struct {
        const char* word;
        float x, y, z;
    } defaults[] = {
        {"left", -1.0f, 0.0f, 0.0f},
        {"right", 1.0f, 0.0f, 0.0f},
        {"up", 0.0f, 1.0f, 0.0f},
        {"down", 0.0f, -1.0f, 0.0f},
        {"forward", 0.0f, 0.0f, 1.0f},
        {"back", 0.0f, 0.0f, -1.0f},
        {"above", 0.0f, 0.8f, 0.2f},
        {"below", 0.0f, -0.8f, 0.2f},
        {"near", 0.0f, 0.0f, 0.3f},
        {"far", 0.0f, 0.0f, 1.0f},
        {"here", 0.0f, 0.0f, 0.0f},
        {"there", 0.5f, 0.0f, 0.5f},
        {"inside", 0.0f, 0.0f, -0.5f},
        {"outside", 0.0f, 0.0f, 0.5f},
        {"beside", 0.7f, 0.0f, 0.0f},
        {"between", 0.0f, 0.0f, 0.0f},
        {"through", 0.0f, 0.0f, 1.0f},
        {"across", 1.0f, 0.0f, 0.0f},
        {"around", 0.0f, 0.0f, 0.0f},
        {"over", 0.0f, 0.5f, 0.5f}
    };

    size_t count = sizeof(defaults) / sizeof(defaults[0]);
    for (size_t i = 0; i < count && bridge->spatial_word_count < 64; i++) {
        strncpy(bridge->spatial_words[bridge->spatial_word_count].word,
                defaults[i].word, 31);
        bridge->spatial_words[bridge->spatial_word_count].vector[0] = defaults[i].x;
        bridge->spatial_words[bridge->spatial_word_count].vector[1] = defaults[i].y;
        bridge->spatial_words[bridge->spatial_word_count].vector[2] = defaults[i].z;
        bridge->spatial_word_count++;
    }
}

static void init_number_words(language_parietal_bridge_t* bridge) {
    const char* ones[] = {"zero", "one", "two", "three", "four",
                          "five", "six", "seven", "eight", "nine"};
    const char* teens[] = {"ten", "eleven", "twelve", "thirteen", "fourteen",
                           "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
    const char* tens[] = {"", "", "twenty", "thirty", "forty",
                          "fifty", "sixty", "seventy", "eighty", "ninety"};

    /* 0-9 */
    for (int i = 0; i < 10 && bridge->number_word_count < 100; i++) {
        strncpy(bridge->number_words[bridge->number_word_count].word, ones[i], 31);
        bridge->number_words[bridge->number_word_count].value = (float)i;
        bridge->number_word_count++;
    }

    /* 10-19 */
    for (int i = 0; i < 10 && bridge->number_word_count < 100; i++) {
        strncpy(bridge->number_words[bridge->number_word_count].word, teens[i], 31);
        bridge->number_words[bridge->number_word_count].value = (float)(10 + i);
        bridge->number_word_count++;
    }

    /* 20, 30, ..., 90 */
    for (int i = 2; i < 10 && bridge->number_word_count < 100; i++) {
        strncpy(bridge->number_words[bridge->number_word_count].word, tens[i], 31);
        bridge->number_words[bridge->number_word_count].value = (float)(i * 10);
        bridge->number_word_count++;
    }

    /* Special large numbers */
    struct { const char* word; float value; } large[] = {
        {"hundred", 100.0f},
        {"thousand", 1000.0f},
        {"million", 1000000.0f},
        {"billion", 1000000000.0f}
    };

    for (size_t i = 0; i < 4 && bridge->number_word_count < 100; i++) {
        strncpy(bridge->number_words[bridge->number_word_count].word, large[i].word, 31);
        bridge->number_words[bridge->number_word_count].value = large[i].value;
        bridge->number_word_count++;
    }
}

//=============================================================================
// Public API
//=============================================================================

language_parietal_config_t language_parietal_default_config(void) {
    language_parietal_config_t config = {
        .update_interval_ms = 50,
        .enable_spatial_language = true,
        .enable_number_processing = true,
        .enable_attention_direction = true,
        .enable_bio_async = false
    };
    return config;
}

language_parietal_bridge_t* language_parietal_bridge_create(
    language_orchestrator_t* language,
    parietal_adapter_t* parietal,
    const language_parietal_config_t* config)
{
    if (!language || !parietal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_parietal_bridge_create: required parameter is NULL (language, parietal)");
        return NULL;
    }

    language_parietal_bridge_t* bridge = nimcp_calloc(1, sizeof(language_parietal_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->language = language;
    bridge->parietal = parietal;
    bridge->config = config ? *config : language_parietal_default_config();
    bridge->state = LP_STATE_IDLE;
    bridge->last_update_ms = nimcp_time_now_us() / 1000;

    /* Initialize word mappings */
    if (bridge->config.enable_spatial_language) {
        init_spatial_words(bridge);
    }
    if (bridge->config.enable_number_processing) {
        init_number_words(bridge);
    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->attention_x = 0.0f;
    bridge->attention_y = 0.0f;

    return bridge;
}

void language_parietal_bridge_destroy(language_parietal_bridge_t* bridge) {
    if (bridge) {
        nimcp_free(bridge);
    }
}

int language_parietal_bridge_update(language_parietal_bridge_t* bridge, uint64_t timestamp_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    uint64_t elapsed = timestamp_ms - bridge->last_update_ms;
    if (elapsed < bridge->config.update_interval_ms) {
        return 0;
    }

    bridge->last_update_ms = timestamp_ms;
    bridge->stats.state = bridge->state;

    return 0;
}

int language_parietal_process_spatial_word(
    language_parietal_bridge_t* bridge,
    const char* word,
    float* spatial_vector,
    uint32_t vec_size)
{
    if (!bridge || !word || !spatial_vector || vec_size < 3) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_parietal_process_spatial_word: required parameter is NULL (bridge, word, spatial_vector)");
        return -1;
    }

    if (!bridge->config.enable_spatial_language) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_parietal_process_spatial_word: bridge->config is NULL");
        return -1;
    }

    bridge->state = LP_STATE_SPATIAL_PROCESSING;

    /* Search for word in mappings */
    for (uint32_t i = 0; i < bridge->spatial_word_count; i++) {
        if (strcasecmp(bridge->spatial_words[i].word, word) == 0) {
            spatial_vector[0] = bridge->spatial_words[i].vector[0];
            spatial_vector[1] = bridge->spatial_words[i].vector[1];
            spatial_vector[2] = bridge->spatial_words[i].vector[2];
            bridge->stats.spatial_queries++;
            bridge->state = LP_STATE_IDLE;
            return 0;
        }
    }

    /* Word not found - return zero vector */
    spatial_vector[0] = 0.0f;
    spatial_vector[1] = 0.0f;
    spatial_vector[2] = 0.0f;
    bridge->state = LP_STATE_IDLE;

    return -1;  /* Word not in spatial vocabulary */
}

int language_parietal_number_to_word(
    language_parietal_bridge_t* bridge,
    float number,
    char* word,
    uint32_t max_len)
{
    if (!bridge || !word || max_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_parietal_number_to_word: required parameter is NULL (bridge, word)");
        return -1;
    }

    if (!bridge->config.enable_number_processing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_parietal_number_to_word: bridge->config is NULL");
        return -1;
    }

    bridge->state = LP_STATE_NUMBER_PROCESSING;

    /* Handle simple cases */
    int int_num = (int)roundf(number);

    if (int_num >= 0 && int_num < 20) {
        for (uint32_t i = 0; i < bridge->number_word_count; i++) {
            if ((int)bridge->number_words[i].value == int_num) {
                strncpy(word, bridge->number_words[i].word, max_len - 1);
                word[max_len - 1] = '\0';
                bridge->stats.number_conversions++;
                bridge->state = LP_STATE_IDLE;
                return 0;
            }
        }
    }

    /* For larger numbers, construct compound representation */
    if (int_num >= 20 && int_num < 100) {
        int tens_digit = (int_num / 10) * 10;
        int ones_digit = int_num % 10;

        const char* tens_word = NULL;
        const char* ones_word = NULL;

        for (uint32_t i = 0; i < bridge->number_word_count; i++) {
            if ((int)bridge->number_words[i].value == tens_digit) {
                tens_word = bridge->number_words[i].word;
            }
            if ((int)bridge->number_words[i].value == ones_digit) {
                ones_word = bridge->number_words[i].word;
            }
        }

        if (tens_word) {
            if (ones_digit == 0) {
                strncpy(word, tens_word, max_len - 1);
            } else if (ones_word) {
                snprintf(word, max_len, "%s-%s", tens_word, ones_word);
            }
            word[max_len - 1] = '\0';
            bridge->stats.number_conversions++;
            bridge->state = LP_STATE_IDLE;
            return 0;
        }
    }

    /* Fallback: return numeric string */
    snprintf(word, max_len, "%d", int_num);
    bridge->stats.number_conversions++;
    bridge->state = LP_STATE_IDLE;

    return 0;
}

int language_parietal_word_to_number(
    language_parietal_bridge_t* bridge,
    const char* word,
    float* number)
{
    if (!bridge || !word || !number) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_parietal_word_to_number: required parameter is NULL (bridge, word, number)");
        return -1;
    }

    if (!bridge->config.enable_number_processing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_parietal_word_to_number: bridge->config is NULL");
        return -1;
    }

    bridge->state = LP_STATE_NUMBER_PROCESSING;

    /* Search for exact match */
    for (uint32_t i = 0; i < bridge->number_word_count; i++) {
        if (strcasecmp(bridge->number_words[i].word, word) == 0) {
            *number = bridge->number_words[i].value;
            bridge->stats.number_conversions++;
            bridge->state = LP_STATE_IDLE;
            return 0;
        }
    }

    /* Try parsing compound number (e.g., "twenty-five") */
    char* hyphen = strchr(word, '-');
    if (hyphen) {
        char tens_part[32];
        char ones_part[32];
        size_t tens_len = hyphen - word;

        if (tens_len < 32) {
            strncpy(tens_part, word, tens_len);
            tens_part[tens_len] = '\0';
            strncpy(ones_part, hyphen + 1, 31);
            ones_part[31] = '\0';

            float tens_val = 0, ones_val = 0;
            bool found_tens = false, found_ones = false;

            for (uint32_t i = 0; i < bridge->number_word_count; i++) {
                if (strcasecmp(bridge->number_words[i].word, tens_part) == 0) {
                    tens_val = bridge->number_words[i].value;
                    found_tens = true;
                }
                if (strcasecmp(bridge->number_words[i].word, ones_part) == 0) {
                    ones_val = bridge->number_words[i].value;
                    found_ones = true;
                }
            }

            if (found_tens && found_ones) {
                *number = tens_val + ones_val;
                bridge->stats.number_conversions++;
                bridge->state = LP_STATE_IDLE;
                return 0;
            }
        }
    }

    bridge->state = LP_STATE_IDLE;
    return -1;  /* Word not recognized as number */
}

int language_parietal_direct_attention(
    language_parietal_bridge_t* bridge,
    float x,
    float y)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->config.enable_attention_direction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_parietal_direct_attention: bridge->config is NULL");
        return -1;
    }

    bridge->state = LP_STATE_ATTENTION_ACTIVE;
    bridge->attention_x = x;
    bridge->attention_y = y;
    bridge->stats.attention_shifts++;
    bridge->state = LP_STATE_IDLE;

    return 0;
}

int language_parietal_get_stats(
    const language_parietal_bridge_t* bridge,
    language_parietal_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_parietal_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}
