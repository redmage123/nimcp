/**
 * @file nimcp_self_heal.c
 * @brief Self-Healing Engine Implementation - Intelligent Crash Recovery with LNN
 * @version 1.0.0
 * @date 2025-12-27
 *
 * WHAT: Implementation of self-healing engine with pattern matching and LNN
 * WHY:  Enable automated code repair through learned crash-to-fix mappings
 * HOW:  Pattern library for known fixes, LNN for learning novel mappings
 *
 * @author NIMCP Development Team
 */

#include "cognitive/immune/nimcp_self_heal.h"
#include "cognitive/immune/nimcp_heal_patterns.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/signal/nimcp_signal_handler.h"
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_training.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE(self_heal, MESH_ADAPTER_CATEGORY_SECURITY)



/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define LOG_TAG "SelfHeal"

/* Feature encoding parameters */
#define CRASH_TYPE_SIGSEGV    0.1f
#define CRASH_TYPE_SIGFPE     0.2f
#define CRASH_TYPE_SIGBUS     0.3f
#define CRASH_TYPE_SIGABRT    0.4f

/* Online learning parameters */
#define ONLINE_DECAY_RATE     0.995f    /**< Weight decay per update */
#define ONLINE_MIN_WEIGHT     0.01f     /**< Minimum sample weight */
#define ONLINE_PRIORITY_BOOST 1.5f      /**< Boost for recent successful fixes */
#define ONLINE_RECENCY_WINDOW 86400000000ULL  /**< 24 hours in microseconds */

/* Feature extraction layout */
#define FEATURE_SIGNAL_OFFSET      0    /**< Signal type features start */
#define FEATURE_SOURCE_OFFSET      4    /**< Source type features start */
#define FEATURE_THREAT_OFFSET      8    /**< Threat type features start */
#define FEATURE_CONTEXT_OFFSET     12   /**< Context features start */
#define FEATURE_BACKTRACE_OFFSET   20   /**< Backtrace hash features start */
#define FEATURE_PATTERN_OFFSET     28   /**< Pattern encoding features start */
#define FEATURE_RESERVED_OFFSET    48   /**< Reserved features start */

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Hash string to float in [0,1] range
 */
static float hash_string_to_float(const char* str, size_t len)
{
    if (str == NULL || len == 0) return 0.0f;

    uint32_t hash = 5381;
    for (size_t i = 0; i < len && str[i] != '\0'; i++) {
        hash = ((hash << 5) + hash) + (uint8_t)str[i];
    }
    return (float)(hash % 10000) / 10000.0f;
}

/**
 * @brief Hash backtrace to multiple features
 *
 * WHAT: Convert backtrace addresses to normalized feature values
 * WHY:  Capture stack trace patterns for similar crash detection
 * HOW:  Hash groups of addresses to create distributed representation
 */
static void hash_backtrace_to_features(
    const void* const* backtrace,
    int depth,
    float* features,
    size_t n_features)
{
    if (backtrace == NULL || features == NULL || n_features == 0) return;
    if (depth <= 0) {
        for (size_t i = 0; i < n_features; i++) features[i] = 0.0f;
        return;
    }

    /* Hash groups of addresses */
    for (size_t i = 0; i < n_features; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n_features > 256) {
                self_heal_heartbeat("self_heal_loop",
                                 (float)(i + 1) / (float)n_features);
            }

        uint32_t hash = 5381;
        int group_size = (depth + (int)n_features - 1) / (int)n_features;
        int start = (int)i * group_size;
        int end = start + group_size;
        if (end > depth) end = depth;

        for (int j = start; j < end; j++) {
            uintptr_t addr = (uintptr_t)backtrace[j];
            hash = ((hash << 5) + hash) + (uint8_t)(addr & 0xFF);
            hash = ((hash << 5) + hash) + (uint8_t)((addr >> 8) & 0xFF);
            hash = ((hash << 5) + hash) + (uint8_t)((addr >> 16) & 0xFF);
            hash = ((hash << 5) + hash) + (uint8_t)((addr >> 24) & 0xFF);
        }
        features[i] = (float)(hash % 10000) / 10000.0f;
    }
}

/**
 * @brief Calculate sample weight based on recency and success
 *
 * WHAT: Compute training weight for online learning
 * WHY:  Prioritize recent successful fixes over old failures
 * HOW:  Exponential decay with recency boost
 */
static float calculate_sample_weight(
    uint64_t sample_time,
    uint64_t current_time,
    float success_score)
{
    uint64_t age = current_time - sample_time;
    float recency = 1.0f;

    /* Apply exponential decay based on age */
    if (age > 0) {
        float decay_steps = (float)age / (float)ONLINE_RECENCY_WINDOW;
        recency = powf(ONLINE_DECAY_RATE, decay_steps);
        if (recency < ONLINE_MIN_WEIGHT) recency = ONLINE_MIN_WEIGHT;
    }

    /* Boost successful fixes */
    if (success_score > 0.5f) {
        recency *= ONLINE_PRIORITY_BOOST;
    }

    return recency;
}

/**
 * @brief Apply weight decay to all training samples
 *
 * WHAT: Reduce weights of old samples
 * WHY:  Favor recent patterns in learning
 * HOW:  Multiply all weights by decay factor
 */
static void decay_sample_weights_unlocked(self_heal_engine_t* engine)
{
    if (engine == NULL || engine->training_samples == NULL) return;

    uint64_t now = get_time_us();
    size_t n_samples = engine->n_training_samples;
    if (n_samples > engine->training_sample_capacity) {
        n_samples = engine->training_sample_capacity;
    }

    for (size_t i = 0; i < n_samples; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n_samples > 256) {
            self_heal_heartbeat("self_heal_loop",
                             (float)(i + 1) / (float)n_samples);
        }

        training_sample_t* sample = &engine->training_samples[i];
        float weight = calculate_sample_weight(
            sample->timestamp, now, sample->success_score);
        /* Blend with original score */
        sample->success_score *= weight;
    }
}

/**
 * @brief Map BBB threat type to fix pattern type
 */
static fix_pattern_type_t map_bbb_threat_to_pattern(bbb_threat_type_t threat)
{
    switch (threat) {
        case BBB_THREAT_BUFFER_OVERFLOW:
            return FIX_PATTERN_BOUNDS_CHECK;
        case BBB_THREAT_MEMORY_VIOLATION:
            return FIX_PATTERN_NULL_CHECK;
        case BBB_THREAT_INTEGER_OVERFLOW:
            return FIX_PATTERN_OVERFLOW_CHECK;
        default:
            return FIX_PATTERN_UNKNOWN;
    }
}

/**
 * @brief Initialize LNN network for fix prediction
 */
static int init_lnn_network(self_heal_engine_t* engine)
{
    if (engine == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "init_lnn_network: invalid parameter");
        return -1;
    }
    if (!engine->config.enable_lnn) return 0;

    /* Create NCP network: features -> hidden -> fix_type */
    uint32_t n_inputs = SELF_HEAL_FEATURE_DIM;
    uint32_t n_inter = engine->config.lnn_hidden_size;
    uint32_t n_command = n_inter / 2;
    uint32_t n_outputs = FIX_PATTERN_COUNT;

    engine->lnn_network = lnn_network_create_ncp(
        n_inputs, n_inter, n_command, n_outputs
    );

    if (engine->lnn_network == NULL) {
        LOG_MODULE_ERROR(LOG_TAG, "Failed to create LNN network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_lnn_network: validation failed");
        return -1;
    }

    /* Initialize weights randomly */
    int ret = lnn_network_init_weights(engine->lnn_network, 0);
    if (ret != 0) {
        LOG_MODULE_WARN(LOG_TAG, "LNN weight init returned %d", ret);
    }

    /* Create training context if learning enabled */
    if (engine->config.enable_learning) {
        lnn_training_config_t train_config;
        lnn_training_config_default(&train_config);
        train_config.learning_rate = engine->config.lnn_learning_rate;
        train_config.optimizer_type = NIMCP_OPTIMIZER_ADAM;

        engine->lnn_training = lnn_training_create(
            engine->lnn_network, &train_config
        );

        if (engine->lnn_training == NULL) {
            LOG_MODULE_WARN(LOG_TAG, "Failed to create LNN training context");
        }
    }

    engine->lnn_initialized = true;
    LOG_MODULE_INFO(LOG_TAG, "LNN network initialized (%u->%u->%u)",
                    n_inputs, n_inter, n_outputs);

    return 0;
}

/**
 * @brief Allocate training sample buffer
 */
static int init_training_buffer(self_heal_engine_t* engine)
{
    if (engine == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "init_training_buffer: invalid parameter");
        return -1;
    }
    if (!engine->config.enable_learning) return 0;

    size_t capacity = engine->config.max_training_samples;
    if (capacity == 0) capacity = SELF_HEAL_MAX_TRAINING_SAMPLES;

    engine->training_samples = nimcp_calloc(capacity, sizeof(training_sample_t));
    if (engine->training_samples == NULL) {
        LOG_MODULE_ERROR(LOG_TAG, "Failed to allocate training buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_buffer: validation failed");
        return -1;
    }

    engine->training_sample_capacity = capacity;
    engine->n_training_samples = 0;

    return 0;
}

/**
 * @brief Apply variable substitution to template
 */
static int apply_template_substitution(
    const char* template_str,
    const pattern_match_result_t* match,
    char* output,
    size_t output_size)
{
    if (template_str == NULL || output == NULL || output_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_template_substitution: output_size is zero");
        return -1;
    }

    size_t out_idx = 0;
    size_t template_len = strlen(template_str);

    for (size_t i = 0; i < template_len && out_idx < output_size - 1; i++) {
        /* Check for variable placeholder ${name} */
        if (template_str[i] == '$' && i + 1 < template_len && template_str[i + 1] == '{') {
            /* Find closing brace */
            size_t var_start = i + 2;
            size_t var_end = var_start;
            while (var_end < template_len && template_str[var_end] != '}') {
                var_end++;
            }

            if (var_end < template_len) {
                char var_name[NIMCP_ID_BUFFER_SIZE] = {0};
                size_t var_len = var_end - var_start;
                if (var_len < sizeof(var_name)) {
                    strncpy(var_name, template_str + var_start, var_len);
                }

                /* Substitute variable */
                const char* value = NULL;
                if (strcmp(var_name, "ptr") == 0) value = match->var_ptr;
                else if (strcmp(var_name, "idx") == 0) value = match->var_idx;
                else if (strcmp(var_name, "size") == 0) value = match->var_size;
                else if (strcmp(var_name, "divisor") == 0) value = match->var_divisor;
                else if (strcmp(var_name, "type") == 0) value = match->var_type;

                if (value != NULL && value[0] != '\0') {
                    size_t val_len = strlen(value);
                    if (out_idx + val_len < output_size) {
                        memcpy(output + out_idx, value, val_len);
                        out_idx += val_len;
                    }
                }

                i = var_end; /* Skip past variable */
                continue;
            }
        }

        output[out_idx++] = template_str[i];
    }

    output[out_idx] = '\0';
    return 0;
}

/**
 * @brief Generate fix from pattern
 */
static int generate_pattern_fix(
    self_heal_engine_t* engine,
    const fix_pattern_t* pattern,
    const char* source_code,
    heal_result_t* result)
{
    if (engine == NULL || pattern == NULL || source_code == NULL || result == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "generate_pattern_fix: validation failed");
        return -1;
    }

    /* Match pattern against source code */
    pattern_match_result_t match = {0};
    int ret = heal_pattern_match(engine->pattern_library, source_code,
                                  strlen(source_code), &match);

    if (ret != 0 || !match.valid) {
        /* Try to extract variable names heuristically */
        /* For NULL check, extract pointer name before '->' */
        const char* arrow = strstr(source_code, "->");
        if (arrow != NULL && pattern->type == FIX_PATTERN_NULL_CHECK) {
            const char* ptr_start = arrow - 1;
            while (ptr_start > source_code &&
                   ((*ptr_start >= 'a' && *ptr_start <= 'z') ||
                    (*ptr_start >= 'A' && *ptr_start <= 'Z') ||
                    (*ptr_start >= '0' && *ptr_start <= '9') ||
                    *ptr_start == '_')) {
                ptr_start--;
            }
            ptr_start++;
            size_t ptr_len = (size_t)(arrow - ptr_start);
            if (ptr_len > 0 && ptr_len < sizeof(match.var_ptr)) {
                strncpy(match.var_ptr, ptr_start, ptr_len);
                match.var_ptr[ptr_len] = '\0';
                match.valid = true;
            }
        }

        /* For bounds check, extract array name and index */
        const char* bracket = strchr(source_code, '[');
        if (bracket != NULL && pattern->type == FIX_PATTERN_BOUNDS_CHECK) {
            const char* arr_start = bracket - 1;
            while (arr_start > source_code &&
                   ((*arr_start >= 'a' && *arr_start <= 'z') ||
                    (*arr_start >= 'A' && *arr_start <= 'Z') ||
                    (*arr_start >= '0' && *arr_start <= '9') ||
                    *arr_start == '_')) {
                arr_start--;
            }
            arr_start++;

            size_t arr_len = (size_t)(bracket - arr_start);
            if (arr_len > 0 && arr_len < sizeof(match.var_ptr)) {
                strncpy(match.var_ptr, arr_start, arr_len);
                match.var_ptr[arr_len] = '\0';
            }

            /* Extract index */
            const char* close_bracket = strchr(bracket, ']');
            if (close_bracket != NULL) {
                size_t idx_len = (size_t)(close_bracket - bracket - 1);
                if (idx_len > 0 && idx_len < sizeof(match.var_idx)) {
                    strncpy(match.var_idx, bracket + 1, idx_len);
                    match.var_idx[idx_len] = '\0';
                }
            }
            strncpy(match.var_size, "size", sizeof(match.var_size) - 1); /* Default size variable */
            match.var_size[sizeof(match.var_size) - 1] = '\0';
            match.valid = true;
        }

        /* For division, extract operands */
        const char* div_op = strstr(source_code, " / ");
        if (div_op != NULL && pattern->type == FIX_PATTERN_ZERO_CHECK) {
            /* Extract divisor after / */
            const char* div_start = div_op + 3;
            while (*div_start == ' ') div_start++;

            size_t div_len = 0;
            while (div_start[div_len] != '\0' && div_start[div_len] != ' ' &&
                   div_start[div_len] != ';' && div_start[div_len] != ')') {
                div_len++;
            }
            if (div_len > 0 && div_len < sizeof(match.var_divisor)) {
                strncpy(match.var_divisor, div_start, div_len);
                match.var_divisor[div_len] = '\0';
                match.valid = true;
            }
        }
    }

    /* Generate fix based on pattern type */
    char fix_prefix[NIMCP_LOG_BUFFER_SIZE] = {0};

    switch (pattern->type) {
        case FIX_PATTERN_NULL_CHECK:
            if (match.var_ptr[0] != '\0') {
                snprintf(fix_prefix, sizeof(fix_prefix),
                    "if (%s == NULL) {\n    return NIMCP_ERROR_NULL_POINTER;\n}\n",
                    match.var_ptr);
            }
            break;

        case FIX_PATTERN_BOUNDS_CHECK:
            if (match.var_idx[0] != '\0') {
                snprintf(fix_prefix, sizeof(fix_prefix),
                    "if (%s >= %s) {\n    return NIMCP_ERROR_OUT_OF_RANGE;\n}\n",
                    match.var_idx, match.var_size[0] ? match.var_size : "size");
            }
            break;

        case FIX_PATTERN_ZERO_CHECK:
            if (match.var_divisor[0] != '\0') {
                snprintf(fix_prefix, sizeof(fix_prefix),
                    "if (%s == 0) {\n    return 0;\n}\n",
                    match.var_divisor);
            }
            break;

        case FIX_PATTERN_UAF_CHECK:
            if (match.var_ptr[0] != '\0') {
                snprintf(fix_prefix, sizeof(fix_prefix),
                    "if (%s != NULL) {\n    /* UAF protected access */\n",
                    match.var_ptr);
            }
            break;

        case FIX_PATTERN_ALIGN_FIX:
            if (match.var_ptr[0] != '\0' && match.var_type[0] != '\0') {
                snprintf(fix_prefix, sizeof(fix_prefix),
                    "%s _aligned_tmp;\nmemcpy(&_aligned_tmp, %s, sizeof(%s));\n",
                    match.var_type, match.var_ptr, match.var_type);
            }
            break;

        case FIX_PATTERN_DOUBLE_FREE:
            if (match.var_ptr[0] != '\0') {
                snprintf(fix_prefix, sizeof(fix_prefix),
                    "if (%s != NULL) {\n    nimcp_free(%s);\n    %s = NULL;\n}\n",
                    match.var_ptr, match.var_ptr, match.var_ptr);
            }
            break;

        case FIX_PATTERN_OVERFLOW_CHECK:
            if (match.var_ptr[0] != '\0' && match.var_idx[0] != '\0') {
                snprintf(fix_prefix, sizeof(fix_prefix),
                    "if (%s > UINT32_MAX - %s) {\n"
                    "    return NIMCP_ERROR_OVERFLOW;\n}\n",
                    match.var_ptr, match.var_idx);
            }
            break;

        case FIX_PATTERN_INIT_CHECK:
            if (match.var_ptr[0] != '\0' && match.var_type[0] != '\0') {
                snprintf(fix_prefix, sizeof(fix_prefix),
                    "%s %s = {0};\n",
                    match.var_type, match.var_ptr);
            }
            break;

        case FIX_PATTERN_RACE_GUARD:
            if (match.var_ptr[0] != '\0') {
                snprintf(fix_prefix, sizeof(fix_prefix),
                    "nimcp_mutex_lock(mutex);\n");
            }
            break;

        case FIX_PATTERN_RESOURCE_LEAK:
            if (match.var_ptr[0] != '\0') {
                snprintf(fix_prefix, sizeof(fix_prefix),
                    "if (%s != NULL) {\n"
                    "    nimcp_free(%s);\n"
                    "    %s = NULL;\n"
                    "}\n",
                    match.var_ptr, match.var_ptr, match.var_ptr);
            }
            break;

        case FIX_PATTERN_FORMAT_STRING:
            if (match.var_ptr[0] != '\0') {
                snprintf(fix_prefix, sizeof(fix_prefix),
                    "/* Fix: use printf(\"%%s\", %s); */\n",
                    match.var_ptr);
            }
            break;

        case FIX_PATTERN_BUFFER_UNDERFLOW:
            if (match.var_idx[0] != '\0') {
                snprintf(fix_prefix, sizeof(fix_prefix),
                    "if ((int)(%s) < 0) {\n"
                    "    return NIMCP_ERROR_OUT_OF_RANGE;\n"
                    "}\n",
                    match.var_idx);
            }
            break;

        case FIX_PATTERN_STACK_OVERFLOW:
            snprintf(fix_prefix, sizeof(fix_prefix),
                "static _Thread_local int _recursion_depth = 0;\n"
                "if (++_recursion_depth > 1000) {\n"
                "    _recursion_depth--;\n"
                "    return -1;\n"
                "}\n");
            break;

        case FIX_PATTERN_LOCK_ORDER:
            if (match.var_ptr[0] != '\0') {
                snprintf(fix_prefix, sizeof(fix_prefix),
                    "if (nimcp_mutex_trylock(%s) != 0) {\n"
                    "    return NIMCP_ERROR_TIMEOUT;\n"
                    "}\n",
                    match.var_ptr);
            }
            break;

        default:
            /* Unknown pattern - just copy original */
            strncpy(result->fixed_code, source_code, SELF_HEAL_MAX_CODE_SIZE - 1);
            result->status = HEAL_STATUS_NO_PATTERN;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: operation failed");
            return -1;
    }

    /* Combine fix prefix with original code */
    if (fix_prefix[0] != '\0') {
        snprintf(result->fixed_code, SELF_HEAL_MAX_CODE_SIZE,
                 "%s%s", fix_prefix, source_code);
        result->status = HEAL_STATUS_SUCCESS;
        result->pattern_used = pattern->type;
        result->pattern_id = pattern->id;
        result->confidence = pattern->confidence;
        result->lnn_generated = false;
    } else {
        /* Failed to generate fix */
        strncpy(result->fixed_code, source_code, SELF_HEAL_MAX_CODE_SIZE - 1);
        result->status = HEAL_STATUS_NO_PATTERN;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: operation failed");
        return -1;
    }

    return 0;
}

/**
 * @brief LNN forward pass to predict fix type
 */
static int lnn_predict_fix_type(
    self_heal_engine_t* engine,
    const crash_features_t* features,
    fix_pattern_type_t* fix_type_out,
    float* confidence_out)
{
    if (engine == NULL || !engine->lnn_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "lnn_predict_fix_type: invalid parameter");
        return -1;
    }
    if (features == NULL || fix_type_out == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "lnn_predict_fix_type: invalid parameter");
        return -1;
    }

    /* Create input tensor from features */
    uint32_t dims[1] = {SELF_HEAL_FEATURE_DIM};
    nimcp_tensor_t* input = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    if (input == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "lnn_predict_fix_type: invalid parameter");
        return -1;
    }

    /* Copy features to input tensor */
    float* input_data = (float*)nimcp_tensor_data(input);
    if (input_data == NULL) {
        nimcp_tensor_destroy(input);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lnn_predict_fix_type: validation failed");
        return -1;
    }
    memcpy(input_data, features->features, SELF_HEAL_FEATURE_DIM * sizeof(float));

    /* Create output tensor */
    uint32_t out_dims[1] = {FIX_PATTERN_COUNT};
    nimcp_tensor_t* output = nimcp_tensor_create(out_dims, 1, NIMCP_DTYPE_F32);
    if (output == NULL) {
        nimcp_tensor_destroy(input);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lnn_predict_fix_type: validation failed");
        return -1;
    }

    /* Forward pass */
    int ret = lnn_forward_step(engine->lnn_network, input, output, SELF_HEAL_LNN_DT);
    if (ret != 0) {
        nimcp_tensor_destroy(input);
        nimcp_tensor_destroy(output);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lnn_predict_fix_type: validation failed");
        return -1;
    }

    /* Find max output (predicted class) */
    float* output_data = (float*)nimcp_tensor_data(output);
    if (output_data == NULL) {
        nimcp_tensor_destroy(input);
        nimcp_tensor_destroy(output);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lnn_predict_fix_type: validation failed");
        return -1;
    }

    float max_val = output_data[0];
    int max_idx = 0;
    float sum_exp = 0.0f;

    /* Softmax normalization for confidence */
    for (int i = 0; i < FIX_PATTERN_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && FIX_PATTERN_COUNT > 256) {
            self_heal_heartbeat("self_heal_loop",
                             (float)(i + 1) / (float)FIX_PATTERN_COUNT);
        }

        if (output_data[i] > max_val) {
            max_val = output_data[i];
            max_idx = i;
        }
        sum_exp += expf(output_data[i]);
    }

    *fix_type_out = (fix_pattern_type_t)max_idx;
    if (confidence_out != NULL && sum_exp > 0.0f) {
        *confidence_out = expf(max_val) / sum_exp;
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(output);

    return 0;
}

/**
 * @brief Add training sample
 */
static int add_training_sample(
    self_heal_engine_t* engine,
    const crash_features_t* features,
    fix_pattern_type_t correct_type,
    float success_score)
{
    if (engine == NULL || !engine->config.enable_learning) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "add_training_sample: invalid parameter");
        return -1;
    }
    if (features == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "add_training_sample: invalid parameter");
        return -1;
    }

    nimcp_mutex_lock(engine->mutex);

    /* Check capacity */
    if (engine->n_training_samples >= engine->training_sample_capacity) {
        /* Circular buffer - overwrite oldest */
        size_t idx = engine->n_training_samples % engine->training_sample_capacity;
        engine->training_samples[idx].features = *features;
        engine->training_samples[idx].correct_fix_type = correct_type;
        engine->training_samples[idx].success_score = success_score;
        engine->training_samples[idx].timestamp = get_time_us();
    } else {
        size_t idx = engine->n_training_samples;
        engine->training_samples[idx].features = *features;
        engine->training_samples[idx].correct_fix_type = correct_type;
        engine->training_samples[idx].success_score = success_score;
        engine->training_samples[idx].timestamp = get_time_us();
    }

    engine->n_training_samples++;
    engine->stats.training_samples++;

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

int self_heal_default_config(self_heal_config_t* config)
{
    if (config == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_default_config: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_default_config", 0.0f);


    memset(config, 0, sizeof(self_heal_config_t));

    config->mode = HEAL_MODE_HYBRID;
    config->confidence_threshold = SELF_HEAL_DEFAULT_CONFIDENCE;
    config->enable_lnn = true;
    config->enable_learning = true;
    config->enable_logging = true;

    config->lnn_hidden_size = SELF_HEAL_LNN_HIDDEN_SIZE;
    config->lnn_learning_rate = SELF_HEAL_LNN_LEARNING_RATE;
    config->max_training_samples = SELF_HEAL_MAX_TRAINING_SAMPLES;

    config->use_custom_patterns = true;
    config->max_custom_patterns = HEAL_PATTERN_MAX_CUSTOM_PATTERNS;

    config->immune_system = NULL;

    return 0;
}

self_heal_engine_t* self_heal_create(const self_heal_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_create", 0.0f);


    self_heal_engine_t* engine = nimcp_calloc(1, sizeof(self_heal_engine_t));
    if (engine == NULL) {
        LOG_MODULE_ERROR(LOG_TAG, "Failed to allocate engine");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_heal_create: validation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config != NULL) {
        engine->config = *config;
    } else {
        self_heal_default_config(&engine->config);
    }

    /* Create mutex */
    engine->mutex = nimcp_mutex_create(NULL);
    if (engine->mutex == NULL) {
        LOG_MODULE_ERROR(LOG_TAG, "Failed to create mutex");
        nimcp_free(engine);
        engine = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_heal_create: validation failed");
        return NULL;
    }

    /* Create pattern library */
    engine->pattern_library = heal_pattern_library_create();
    if (engine->pattern_library == NULL) {
        LOG_MODULE_ERROR(LOG_TAG, "Failed to create pattern library");
        nimcp_mutex_destroy(engine->mutex);
        nimcp_free(engine);
        engine = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_heal_create: validation failed");
        return NULL;
    }

    /* Initialize LNN if enabled */
    if (engine->config.enable_lnn) {
        if (init_lnn_network(engine) != 0) {
            LOG_MODULE_WARN(LOG_TAG, "LNN init failed, using pattern-only mode");
            engine->config.enable_lnn = false;
        }
    }

    /* Initialize training buffer */
    if (engine->config.enable_learning) {
        if (init_training_buffer(engine) != 0) {
            LOG_MODULE_WARN(LOG_TAG, "Training buffer init failed");
            engine->config.enable_learning = false;
        }
    }

    /* Connect to immune system if provided */
    if (engine->config.immune_system != NULL) {
        self_heal_connect_immune(engine, engine->config.immune_system);
    }

    engine->start_time = get_time_us();
    engine->initialized = true;

    LOG_MODULE_INFO(LOG_TAG, "Self-healing engine created (mode=%s, lnn=%s)",
                    self_heal_mode_to_string(engine->config.mode),
                    engine->config.enable_lnn ? "enabled" : "disabled");

    return engine;
}

void self_heal_destroy(self_heal_engine_t* engine)
{
    if (engine == NULL) return;

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_destroy", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Destroy LNN */
    if (engine->lnn_training != NULL) {
        lnn_training_destroy(engine->lnn_training);
        engine->lnn_training = NULL;
    }

    if (engine->lnn_network != NULL) {
        lnn_network_destroy(engine->lnn_network);
        engine->lnn_network = NULL;
    }

    /* Free training samples */
    if (engine->training_samples != NULL) {
        nimcp_free(engine->training_samples);
        engine->training_samples = NULL;
    }

    /* Destroy pattern library */
    if (engine->pattern_library != NULL) {
        heal_pattern_library_destroy(engine->pattern_library);
        engine->pattern_library = NULL;
    }

    engine->initialized = false;

    nimcp_mutex_unlock(engine->mutex);
    nimcp_mutex_destroy(engine->mutex);

    nimcp_free(engine);
    engine = NULL;

    LOG_MODULE_INFO(LOG_TAG, "Self-healing engine destroyed");
}

fix_pattern_type_t self_heal_analyze_crash(
    self_heal_engine_t* engine,
    const brain_antigen_t* antigen)
{
    if (engine == NULL || antigen == NULL) {
        return FIX_PATTERN_UNKNOWN;
    }

    /* First, try to map from antigen source */
    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_analyze_crash", 0.0f);


    if (antigen->source == ANTIGEN_SOURCE_BBB) {
        fix_pattern_type_t pattern = map_bbb_threat_to_pattern(antigen->bbb_threat_type);
        if (pattern != FIX_PATTERN_UNKNOWN) {
            return pattern;
        }
    }

    /* Try pattern matching on epitope */
    if (antigen->epitope_len > 0) {
        pattern_match_result_t match = {0};
        int ret = heal_pattern_match(engine->pattern_library,
                                      (const char*)antigen->epitope,
                                      antigen->epitope_len, &match);
        if (ret == 0 && match.valid) {
            return match.matched_type;
        }
    }

    /* Use LNN prediction if enabled */
    if (engine->config.enable_lnn && engine->lnn_initialized) {
        crash_features_t features = {0};
        if (self_heal_extract_features(engine, antigen, &features) == 0) {
            fix_pattern_type_t predicted;
            float confidence = 0.0f;
            if (lnn_predict_fix_type(engine, &features, &predicted, &confidence) == 0) {
                if (confidence >= engine->config.confidence_threshold) {
                    return predicted;
                }
            }
        }
    }

    return FIX_PATTERN_UNKNOWN;
}

int self_heal_extract_features(
    self_heal_engine_t* engine,
    const brain_antigen_t* antigen,
    crash_features_t* features)
{
    if (engine == NULL || antigen == NULL || features == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_heal_extract_features: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_extract_features", 0.0f);


    memset(features, 0, sizeof(crash_features_t));

    size_t idx = 0;

    /* Feature 0-3: Antigen source encoding (one-hot) */
    features->features[idx++] = (antigen->source == ANTIGEN_SOURCE_BBB) ? 1.0f : 0.0f;
    features->features[idx++] = (antigen->source == ANTIGEN_SOURCE_BFT) ? 1.0f : 0.0f;
    features->features[idx++] = (antigen->source == ANTIGEN_SOURCE_ANOMALY) ? 1.0f : 0.0f;
    features->features[idx++] = (antigen->source == ANTIGEN_SOURCE_SWARM) ? 1.0f : 0.0f;

    /* Feature 4-7: BBB threat type encoding */
    if (antigen->source == ANTIGEN_SOURCE_BBB) {
        features->features[idx++] = (antigen->bbb_threat_type == BBB_THREAT_MEMORY_VIOLATION) ? 1.0f : 0.0f;
        features->features[idx++] = (antigen->bbb_threat_type == BBB_THREAT_BUFFER_OVERFLOW) ? 1.0f : 0.0f;
        features->features[idx++] = (antigen->bbb_threat_type == BBB_THREAT_INTEGER_OVERFLOW) ? 1.0f : 0.0f;
        features->features[idx++] = (antigen->bbb_threat_type == BBB_THREAT_CODE_INJECTION) ? 1.0f : 0.0f;
    } else {
        idx += 4; /* Skip these features */
    }

    /* Feature 8: Severity normalized to [0,1] */
    features->features[idx++] = (float)antigen->severity / 10.0f;

    /* Feature 9: Confidence */
    features->features[idx++] = antigen->confidence;

    /* Feature 10: Danger signal */
    features->features[idx++] = antigen->danger_signal;

    /* Feature 11-14: Epitope hash (divided into parts) */
    if (antigen->epitope_len > 0) {
        float hash = hash_string_to_float((const char*)antigen->epitope, antigen->epitope_len);
        features->features[idx++] = hash;
        features->features[idx++] = fmodf(hash * NIMCP_PI_F, 1.0f);
        features->features[idx++] = fmodf(hash * NIMCP_EULER_F, 1.0f);
        features->features[idx++] = fmodf(hash * 1.61803f, 1.0f);
    } else {
        idx += 4;
    }

    /* Feature 15: Source node ID normalized */
    features->features[idx++] = (float)(antigen->source_node_id % 1000) / 1000.0f;

    /* Feature 16: Response count */
    features->features[idx++] = (float)antigen->response_count / 100.0f;

    /* Feature 17: Processed flag */
    features->features[idx++] = antigen->processed ? 1.0f : 0.0f;

    /* Feature 18: Neutralized flag */
    features->features[idx++] = antigen->neutralized ? 1.0f : 0.0f;

    /* Fill remaining features with zero (reserved) */
    while (idx < SELF_HEAL_FEATURE_DIM) {
        features->features[idx++] = 0.0f;
    }

    features->n_features = SELF_HEAL_FEATURE_DIM;

    /* Pre-analyze suggested type */
    features->suggested_type = map_bbb_threat_to_pattern(antigen->bbb_threat_type);
    features->type_confidence = antigen->confidence;

    return 0;
}

/**
 * @brief Extract extended features from crash context
 *
 * WHAT: Convert crash context (signal, backtrace) to numerical features
 * WHY:  More detailed crash signature for better fix prediction
 * HOW:  Encode signal type, addresses, backtrace patterns
 */
int self_heal_extract_features_from_context(
    self_heal_engine_t* engine,
    const signal_crash_context_t* crash_ctx,
    crash_features_t* features)
{
    if (engine == NULL || crash_ctx == NULL || features == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_heal_extract_features_from_context: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_extract_features_fro", 0.0f);


    memset(features, 0, sizeof(crash_features_t));
    size_t idx = 0;

    /* Feature 0-3: Signal type encoding */
    features->features[idx++] = (crash_ctx->signal == SIGSEGV) ? CRASH_TYPE_SIGSEGV : 0.0f;
    features->features[idx++] = (crash_ctx->signal == SIGFPE) ? CRASH_TYPE_SIGFPE : 0.0f;
    features->features[idx++] = (crash_ctx->signal == SIGBUS) ? CRASH_TYPE_SIGBUS : 0.0f;
    features->features[idx++] = (crash_ctx->signal == SIGABRT) ? CRASH_TYPE_SIGABRT : 0.0f;

    /* Feature 4-7: Fault address characteristics */
    uintptr_t fault_addr = (uintptr_t)crash_ctx->fault_address;
    features->features[idx++] = (fault_addr == 0) ? 1.0f : 0.0f;  /* NULL pointer */
    features->features[idx++] = ((fault_addr & 0x7) != 0) ? 1.0f : 0.0f;  /* Unaligned */
    features->features[idx++] = (fault_addr < 0x10000) ? 1.0f : 0.0f;  /* Low address */
    features->features[idx++] = (float)((fault_addr >> 12) & 0xFFFF) / 65535.0f;  /* Page hash */

    /* Feature 8-11: Instruction pointer characteristics */
    uintptr_t ip = (uintptr_t)crash_ctx->instruction_pointer;
    features->features[idx++] = (float)((ip >> 4) & 0xFFFF) / 65535.0f;
    features->features[idx++] = (float)((ip >> 20) & 0xFFFF) / 65535.0f;
    features->features[idx++] = (float)(ip & 0xF) / 16.0f;  /* Instruction alignment */
    features->features[idx++] = (ip == 0) ? 1.0f : 0.0f;  /* Invalid IP */

    /* Feature 12-15: Stack pointer characteristics */
    uintptr_t sp = (uintptr_t)crash_ctx->stack_pointer;
    features->features[idx++] = ((sp & 0xF) != 0) ? 1.0f : 0.0f;  /* Stack unaligned */
    features->features[idx++] = (float)((sp >> 8) & 0xFF) / 255.0f;
    features->features[idx++] = (sp == 0) ? 1.0f : 0.0f;  /* Stack corruption */
    features->features[idx++] = (float)crash_ctx->backtrace_depth / 32.0f;  /* Depth normalized */

    /* Feature 16-19: Base pointer / frame analysis */
    uintptr_t bp = (uintptr_t)crash_ctx->base_pointer;
    features->features[idx++] = (bp == 0) ? 1.0f : 0.0f;
    features->features[idx++] = (bp > sp) ? 1.0f : 0.0f;  /* Normal stack direction */
    features->features[idx++] = (float)((bp >> 12) & 0xFFFF) / 65535.0f;
    features->features[idx++] = (bp == sp) ? 1.0f : 0.0f;  /* No local vars */

    /* Feature 20-27: Backtrace hash (8 features) */
    hash_backtrace_to_features(
        (const void* const*)crash_ctx->backtrace,
        crash_ctx->backtrace_depth,
        &features->features[idx],
        8
    );
    idx += 8;

    /* Feature 28-35: Memory region hash */
    float region_hash = hash_string_to_float(
        crash_ctx->memory_region, strlen(crash_ctx->memory_region));
    features->features[idx++] = region_hash;
    features->features[idx++] = fmodf(region_hash * NIMCP_EULER_F, 1.0f);
    features->features[idx++] = fmodf(region_hash * NIMCP_PI_F, 1.0f);
    features->features[idx++] = fmodf(region_hash * NIMCP_SQRT2_F, 1.0f);

    /* Memory region indicators */
    features->features[idx++] = (strstr(crash_ctx->memory_region, "[heap]") != NULL) ? 1.0f : 0.0f;
    features->features[idx++] = (strstr(crash_ctx->memory_region, "[stack]") != NULL) ? 1.0f : 0.0f;
    features->features[idx++] = (strstr(crash_ctx->memory_region, "r-x") != NULL) ? 1.0f : 0.0f;  /* Executable */
    features->features[idx++] = (strstr(crash_ctx->memory_region, "rw-") != NULL) ? 1.0f : 0.0f;  /* Writable */

    /* Feature 36-47: Pattern indicators */
    /* Detect common crash patterns from context */
    bool null_deref = (fault_addr == 0 || fault_addr < 0x1000);
    bool stack_overflow = (strstr(crash_ctx->memory_region, "[stack]") != NULL) &&
                          (crash_ctx->signal == SIGSEGV);
    bool heap_corruption = (strstr(crash_ctx->memory_region, "[heap]") != NULL) &&
                           (crash_ctx->signal == SIGABRT);
    bool alignment_fault = (crash_ctx->signal == SIGBUS) &&
                           ((fault_addr & 0x7) != 0);
    bool div_zero = (crash_ctx->signal == SIGFPE);

    features->features[idx++] = null_deref ? 1.0f : 0.0f;
    features->features[idx++] = stack_overflow ? 1.0f : 0.0f;
    features->features[idx++] = heap_corruption ? 1.0f : 0.0f;
    features->features[idx++] = alignment_fault ? 1.0f : 0.0f;
    features->features[idx++] = div_zero ? 1.0f : 0.0f;

    /* Reserved pattern features */
    while (idx < 48) {
        features->features[idx++] = 0.0f;
    }

    /* Fill remaining with derived features */
    while (idx < SELF_HEAL_FEATURE_DIM) {
        features->features[idx++] = 0.0f;
    }

    features->n_features = SELF_HEAL_FEATURE_DIM;

    /* Suggest fix type based on crash pattern */
    if (null_deref) {
        features->suggested_type = FIX_PATTERN_NULL_CHECK;
        features->type_confidence = 0.9f;
    } else if (div_zero) {
        features->suggested_type = FIX_PATTERN_ZERO_CHECK;
        features->type_confidence = 0.95f;
    } else if (alignment_fault) {
        features->suggested_type = FIX_PATTERN_ALIGN_FIX;
        features->type_confidence = 0.85f;
    } else if (stack_overflow) {
        features->suggested_type = FIX_PATTERN_BOUNDS_CHECK;
        features->type_confidence = 0.7f;
    } else if (heap_corruption) {
        features->suggested_type = FIX_PATTERN_UAF_CHECK;
        features->type_confidence = 0.6f;
    } else {
        features->suggested_type = FIX_PATTERN_UNKNOWN;
        features->type_confidence = 0.0f;
    }

    return 0;
}

int self_heal_generate_fix(
    self_heal_engine_t* engine,
    const brain_antigen_t* antigen,
    const char* source_code,
    heal_result_t* result)
{
    if (engine == NULL || antigen == NULL || source_code == NULL || result == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_heal_generate_fix: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_generate_fix", 0.0f);


    uint64_t start_time = get_time_us();

    memset(result, 0, sizeof(heal_result_t));
    strncpy(result->original_code, source_code, SELF_HEAL_MAX_CODE_SIZE - 1);
    result->status = HEAL_STATUS_INTERNAL_ERROR;

    nimcp_mutex_lock(engine->mutex);
    engine->stats.crashes_analyzed++;
    nimcp_mutex_unlock(engine->mutex);

    /* Check code size */
    if (strlen(source_code) >= SELF_HEAL_MAX_CODE_SIZE) {
        result->status = HEAL_STATUS_CODE_TOO_LARGE;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "self_heal_generate_fix: capacity exceeded");
        return -1;
    }

    /* Analyze crash to determine pattern type */
    fix_pattern_type_t pattern_type = self_heal_analyze_crash(engine, antigen);

    /* Try pattern-based fix first */
    bool pattern_success = false;
    const fix_pattern_t* pattern = NULL;

    if (engine->config.mode != HEAL_MODE_LNN_ONLY) {
        pattern = heal_pattern_get_by_type(engine->pattern_library, pattern_type);
        if (pattern != NULL && pattern->enabled) {
            if (generate_pattern_fix(engine, pattern, source_code, result) == 0) {
                pattern_success = true;
                result->lnn_generated = false;
            }
        }
    }

    /* Try LNN-based prediction if pattern failed or mode requires it */
    if (!pattern_success &&
        engine->config.enable_lnn &&
        engine->lnn_initialized &&
        engine->config.mode != HEAL_MODE_PATTERN_ONLY) {

        crash_features_t features = {0};
        if (self_heal_extract_features(engine, antigen, &features) == 0) {
            fix_pattern_type_t predicted_type;
            float confidence = 0.0f;

            if (lnn_predict_fix_type(engine, &features, &predicted_type, &confidence) == 0) {
                if (confidence >= engine->config.confidence_threshold) {
                    pattern = heal_pattern_get_by_type(engine->pattern_library, predicted_type);
                    if (pattern != NULL && pattern->enabled) {
                        if (generate_pattern_fix(engine, pattern, source_code, result) == 0) {
                            result->lnn_generated = true;
                            result->confidence = confidence;
                        }
                    }
                }
            }
        }
    }

    /* Update timing */
    result->generation_time_us = get_time_us() - start_time;

    /* Update statistics */
    nimcp_mutex_lock(engine->mutex);
    if (result->status == HEAL_STATUS_SUCCESS) {
        engine->stats.fixes_generated++;
        if (result->lnn_generated) {
            engine->stats.lnn_fixes++;
        } else if (pattern_success) {
            engine->stats.pattern_fixes++;
        }

        /* Update running average */
        float n = (float)engine->stats.fixes_generated;
        engine->stats.avg_confidence =
            (engine->stats.avg_confidence * (n - 1) + result->confidence) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    } else {
        engine->stats.failed_fixes++;
    }

    engine->stats.avg_generation_time_ms =
        (engine->stats.avg_generation_time_ms * (engine->stats.crashes_analyzed - 1) +
         (double)result->generation_time_us / 1000.0) /
        engine->stats.crashes_analyzed;

    nimcp_mutex_unlock(engine->mutex);

    return (result->status == HEAL_STATUS_SUCCESS) ? 0 : -1;
}

int self_heal_generate_candidates(
    self_heal_engine_t* engine,
    const brain_antigen_t* antigen,
    const char* source_code,
    fix_candidate_t* candidates,
    size_t max_candidates)
{
    if (engine == NULL || antigen == NULL || source_code == NULL ||
        candidates == NULL || max_candidates == 0) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_generate_candidates", 0.0f);

    size_t n_candidates = 0;

    /* Try all applicable patterns */
    for (int type = 0; type < FIX_PATTERN_COUNT && n_candidates < max_candidates; type++) {
        const fix_pattern_t* pattern = heal_pattern_get_by_type(
            engine->pattern_library, (fix_pattern_type_t)type);

        if (pattern == NULL || !pattern->enabled) continue;

        heal_result_t result = {0};
        if (generate_pattern_fix(engine, pattern, source_code, &result) == 0) {
            candidates[n_candidates].result = result;
            candidates[n_candidates].score = pattern->confidence;
            candidates[n_candidates].pattern_based = true;
            candidates[n_candidates].lnn_based = false;
            n_candidates++;
        }
    }

    /* Add LNN prediction if available */
    if (engine->config.enable_lnn && engine->lnn_initialized &&
        n_candidates < max_candidates) {

        crash_features_t features = {0};
        if (self_heal_extract_features(engine, antigen, &features) == 0) {
            fix_pattern_type_t predicted_type;
            float confidence = 0.0f;

            if (lnn_predict_fix_type(engine, &features, &predicted_type, &confidence) == 0 &&
                confidence >= engine->config.confidence_threshold) {

                const fix_pattern_t* pattern = heal_pattern_get_by_type(
                    engine->pattern_library, predicted_type);

                if (pattern != NULL && pattern->enabled) {
                    heal_result_t result = {0};
                    if (generate_pattern_fix(engine, pattern, source_code, &result) == 0) {
                        result.lnn_generated = true;
                        result.confidence = confidence;
                        candidates[n_candidates].result = result;
                        candidates[n_candidates].score = confidence;
                        candidates[n_candidates].pattern_based = false;
                        candidates[n_candidates].lnn_based = true;
                        n_candidates++;
                    }
                }
            }
        }
    }

    /* Sort by score (simple bubble sort for small array) */
    /* Guard: Need at least 2 candidates to sort */
    if (n_candidates > 1) {
        for (size_t i = 0; i < n_candidates - 1; i++) {
            for (size_t j = 0; j < n_candidates - i - 1; j++) {
                if (candidates[j].score < candidates[j + 1].score) {
                    fix_candidate_t tmp = candidates[j];
                    candidates[j] = candidates[j + 1];
                    candidates[j + 1] = tmp;
                }
            }
        }
    }

    return (int)n_candidates;
}

float self_heal_lnn_predict(
    self_heal_engine_t* engine,
    const crash_features_t* features,
    fix_pattern_type_t* fix_type_out)
{
    if (engine == NULL || features == NULL || fix_type_out == NULL) {
        return 0.0f;
    }

    if (!engine->config.enable_lnn || !engine->lnn_initialized) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_lnn_predict", 0.0f);


    float confidence = 0.0f;
    if (lnn_predict_fix_type(engine, features, fix_type_out, &confidence) != 0) {
        return 0.0f;
    }

    return confidence;
}

int self_heal_train_on_success(
    self_heal_engine_t* engine,
    const brain_antigen_t* antigen,
    const heal_result_t* fix)
{
    if (engine == NULL || antigen == NULL || fix == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_train_on_success: invalid parameter");
        return -1;
    }
    if (!engine->config.enable_learning) return 0;

    /* Extract features */
    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_train_on_success", 0.0f);


    crash_features_t features = {0};
    if (self_heal_extract_features(engine, antigen, &features) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_heal_train_on_success: validation failed");
        return -1;
    }

    /* Add positive training sample */
    int ret = add_training_sample(engine, &features, fix->pattern_used, 1.0f);

    /* Update pattern statistics */
    if (engine->pattern_library != NULL && fix->pattern_id > 0) {
        heal_pattern_update_stats(engine->pattern_library, fix->pattern_id, true);
    }

    /* Run training update if batch ready */
    if (engine->n_training_samples >= SELF_HEAL_TRAINING_BATCH_SIZE) {
        self_heal_train_batch(engine);
    }

    return ret;
}

int self_heal_train_on_failure(
    self_heal_engine_t* engine,
    const brain_antigen_t* antigen,
    const heal_result_t* fix)
{
    if (engine == NULL || antigen == NULL || fix == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_train_on_failure: invalid parameter");
        return -1;
    }
    if (!engine->config.enable_learning) return 0;

    /* Extract features */
    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_train_on_failure", 0.0f);


    crash_features_t features = {0};
    if (self_heal_extract_features(engine, antigen, &features) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_heal_train_on_failure: validation failed");
        return -1;
    }

    /* Add negative training sample */
    int ret = add_training_sample(engine, &features, fix->pattern_used, 0.0f);

    /* Update pattern statistics */
    if (engine->pattern_library != NULL && fix->pattern_id > 0) {
        heal_pattern_update_stats(engine->pattern_library, fix->pattern_id, false);
    }

    return ret;
}

/**
 * @brief Perform single-step online learning update
 *
 * WHAT: Update LNN weights immediately after fix attempt
 * WHY:  Faster adaptation to new crash patterns
 * HOW:  Single forward/backward pass with current sample
 */
int self_heal_train_online(
    self_heal_engine_t* engine,
    const crash_features_t* features,
    fix_pattern_type_t correct_type,
    float success_score)
{
    if (engine == NULL || features == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_train_online: invalid parameter");
        return -1;
    }
    if (!engine->config.enable_learning || !engine->lnn_initialized) return 0;
    if (engine->lnn_training == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_train_online: invalid parameter");
        return -1;
    }

    /* Create input tensor from features */
    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_train_online", 0.0f);


    uint32_t input_dims[1] = {SELF_HEAL_FEATURE_DIM};
    nimcp_tensor_t* input = nimcp_tensor_create(input_dims, 1, NIMCP_DTYPE_F32);
    if (input == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_train_online: invalid parameter");
        return -1;
    }

    /* Create target tensor (one-hot) */
    uint32_t target_dims[1] = {FIX_PATTERN_COUNT};
    nimcp_tensor_t* target = nimcp_tensor_create(target_dims, 1, NIMCP_DTYPE_F32);
    if (target == NULL) {
        nimcp_tensor_destroy(input);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_heal_train_online: validation failed");
        return -1;
    }

    /* Fill input */
    float* input_data = (float*)nimcp_tensor_data(input);
    if (input_data == NULL) {
        nimcp_tensor_destroy(input);
        nimcp_tensor_destroy(target);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_heal_train_online: validation failed");
        return -1;
    }
    memcpy(input_data, features->features, SELF_HEAL_FEATURE_DIM * sizeof(float));

    /* Fill target (weighted one-hot) */
    float* target_data = (float*)nimcp_tensor_data(target);
    if (target_data == NULL) {
        nimcp_tensor_destroy(input);
        nimcp_tensor_destroy(target);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_heal_train_online: validation failed");
        return -1;
    }

    for (int i = 0; i < FIX_PATTERN_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && FIX_PATTERN_COUNT > 256) {
            self_heal_heartbeat("self_heal_loop",
                             (float)(i + 1) / (float)FIX_PATTERN_COUNT);
        }

        target_data[i] = (i == (int)correct_type) ? success_score : 0.0f;
    }

    /* Run single training step */
    float loss = 0.0f;
    int ret = lnn_training_step(engine->lnn_training, input, target, &loss);

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(target);

    if (ret == 0) {
        nimcp_mutex_lock(engine->mutex);
        engine->stats.training_updates++;
        nimcp_mutex_unlock(engine->mutex);

        LOG_MODULE_DEBUG(LOG_TAG, "Online update: type=%d score=%.2f loss=%.4f",
                         correct_type, success_score, loss);
    }

    return ret;
}

/**
 * @brief Apply temporal decay to training samples
 *
 * WHAT: Decay old sample weights to prioritize recent data
 * WHY:  Online learning should favor recent patterns
 * HOW:  Apply exponential decay based on sample age
 */
int self_heal_decay_samples(self_heal_engine_t* engine)
{
    if (engine == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_decay_samples: invalid parameter");
        return -1;
    }
    if (!engine->config.enable_learning) return 0;

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_decay_samples", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    decay_sample_weights_unlocked(engine);
    nimcp_mutex_unlock(engine->mutex);

    LOG_MODULE_DEBUG(LOG_TAG, "Applied temporal decay to %zu samples",
                     engine->n_training_samples);

    return 0;
}

int self_heal_train_batch(self_heal_engine_t* engine)
{
    if (engine == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_train_batch: invalid parameter");
        return -1;
    }
    if (!engine->config.enable_learning || !engine->lnn_initialized) return 0;
    if (engine->lnn_training == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_train_batch: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_train_batch", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Apply temporal decay before batch training */
    decay_sample_weights_unlocked(engine);

    size_t batch_size = engine->n_training_samples;
    if (batch_size > SELF_HEAL_TRAINING_BATCH_SIZE) {
        batch_size = SELF_HEAL_TRAINING_BATCH_SIZE;
    }

    if (batch_size == 0) {
        nimcp_mutex_unlock(engine->mutex);
        return 0;
    }

    /* Create input tensor for batch */
    uint32_t input_dims[2] = {(uint32_t)batch_size, SELF_HEAL_FEATURE_DIM};
    nimcp_tensor_t* inputs = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_F32);
    if (inputs == NULL) {
        nimcp_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_heal_train_batch: validation failed");
        return -1;
    }

    /* Create target tensor for batch */
    uint32_t target_dims[2] = {(uint32_t)batch_size, FIX_PATTERN_COUNT};
    nimcp_tensor_t* targets = nimcp_tensor_create(target_dims, 2, NIMCP_DTYPE_F32);
    if (targets == NULL) {
        nimcp_tensor_destroy(inputs);
        nimcp_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_heal_train_batch: validation failed");
        return -1;
    }

    /* Fill tensors with training data */
    float* input_data = (float*)nimcp_tensor_data(inputs);
    float* target_data = (float*)nimcp_tensor_data(targets);

    if (input_data == NULL || target_data == NULL) {
        nimcp_tensor_destroy(inputs);
        nimcp_tensor_destroy(targets);
        nimcp_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_heal_train_batch: validation failed");
        return -1;
    }

    for (size_t i = 0; i < batch_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && batch_size > 256) {
            self_heal_heartbeat("self_heal_loop",
                             (float)(i + 1) / (float)batch_size);
        }

        training_sample_t* sample = &engine->training_samples[i];

        /* Copy features */
        memcpy(input_data + i * SELF_HEAL_FEATURE_DIM,
               sample->features.features,
               SELF_HEAL_FEATURE_DIM * sizeof(float));

        /* Create one-hot target */
        for (int j = 0; j < FIX_PATTERN_COUNT; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && FIX_PATTERN_COUNT > 256) {
                self_heal_heartbeat("self_heal_loop",
                                 (float)(j + 1) / (float)FIX_PATTERN_COUNT);
            }

            target_data[i * FIX_PATTERN_COUNT + j] =
                (j == (int)sample->correct_fix_type) ? sample->success_score : 0.0f;
        }
    }

    nimcp_mutex_unlock(engine->mutex);

    /* Run training step */
    float loss = 0.0f;
    int ret = lnn_training_step(engine->lnn_training, inputs, targets, &loss);

    nimcp_tensor_destroy(inputs);
    nimcp_tensor_destroy(targets);

    if (ret == 0) {
        nimcp_mutex_lock(engine->mutex);
        engine->stats.training_updates++;
        nimcp_mutex_unlock(engine->mutex);

        LOG_MODULE_DEBUG(LOG_TAG, "Training batch complete, loss=%.4f", loss);
    }

    return ret;
}

int self_heal_save_model(self_heal_engine_t* engine, const char* path)
{
    if (engine == NULL || path == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_save_model: invalid parameter");
        return -1;
    }
    if (!engine->lnn_initialized || engine->lnn_network == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_save_model: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_save_model", 0.0f);


    return lnn_save(engine->lnn_network, path);
}

int self_heal_load_model(self_heal_engine_t* engine, const char* path)
{
    if (engine == NULL || path == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_load_model: invalid parameter");
        return -1;
    }

    /* Destroy existing network if any */
    if (engine->lnn_network != NULL) {
        if (engine->lnn_training != NULL) {
            lnn_training_destroy(engine->lnn_training);
            engine->lnn_training = NULL;
        }
        lnn_network_destroy(engine->lnn_network);
        engine->lnn_network = NULL;
    }

    /* Load network from file */
    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_load_model", 0.0f);


    engine->lnn_network = lnn_load(path);
    if (engine->lnn_network == NULL) {
        LOG_MODULE_ERROR(LOG_TAG, "Failed to load LNN from %s", path);
        engine->lnn_initialized = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_heal_load_model: validation failed");
        return -1;
    }

    /* Recreate training context if learning enabled */
    if (engine->config.enable_learning) {
        lnn_training_config_t train_config;
        lnn_training_config_default(&train_config);
        train_config.learning_rate = engine->config.lnn_learning_rate;

        engine->lnn_training = lnn_training_create(
            engine->lnn_network, &train_config);
    }

    engine->lnn_initialized = true;
    LOG_MODULE_INFO(LOG_TAG, "Loaded LNN model from %s", path);

    return 0;
}

const fix_pattern_t* self_heal_get_pattern(
    self_heal_engine_t* engine,
    fix_pattern_type_t type)
{
    if (engine == NULL || engine->pattern_library == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "self_heal_get_pattern: parameter is NULL");
        return NULL;
    }
    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_get_pattern", 0.0f);


    return heal_pattern_get_by_type(engine->pattern_library, type);
}

int self_heal_register_pattern(
    self_heal_engine_t* engine,
    const fix_pattern_t* pattern)
{
    if (engine == NULL || pattern == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_register_pattern: invalid parameter");
        return -1;
    }
    if (engine->pattern_library == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_register_pattern: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_register_pattern", 0.0f);


    uint32_t id = 0;
    return heal_pattern_register(engine->pattern_library, pattern, &id);
}

int self_heal_connect_immune(
    self_heal_engine_t* engine,
    brain_immune_system_t* immune_system)
{
    if (engine == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_connect_immune: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_connect_immune", 0.0f);


    engine->immune_system = immune_system;
    engine->immune_connected = (immune_system != NULL);

    if (engine->immune_connected) {
        LOG_MODULE_INFO(LOG_TAG, "Connected to immune system");
    }

    return 0;
}

int self_heal_handle_antigen(
    self_heal_engine_t* engine,
    const brain_antigen_t* antigen)
{
    if (engine == NULL || antigen == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_handle_antigen: invalid parameter");
        return -1;
    }

    /* Try to generate a fix - source code would need to be provided */
    fix_pattern_type_t pattern_type = self_heal_analyze_crash(engine, antigen);

    LOG_MODULE_DEBUG(LOG_TAG, "Analyzed antigen %u -> pattern type %s",
                     antigen->id, heal_pattern_type_to_string(pattern_type));

    return 0;
}

int self_heal_get_stats(
    self_heal_engine_t* engine,
    self_heal_stats_t* stats)
{
    if (engine == NULL || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_get_stats: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_get_stats", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    *stats = engine->stats;

    /* Calculate success rate */
    if (engine->stats.fixes_generated > 0) {
        stats->success_rate = (float)engine->stats.fixes_generated /
                              (float)(engine->stats.fixes_generated + engine->stats.failed_fixes);
    } else {
        stats->success_rate = 0.0f;
    }

    /* Estimate memory usage */
    stats->memory_usage_bytes = sizeof(self_heal_engine_t);
    if (engine->training_samples != NULL) {
        stats->memory_usage_bytes +=
            engine->training_sample_capacity * sizeof(training_sample_t);
    }

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int self_heal_reset_stats(self_heal_engine_t* engine)
{
    if (engine == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "self_heal_reset_stats: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_reset_stats", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    memset(&engine->stats, 0, sizeof(self_heal_stats_t));
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

const char* self_heal_status_to_string(heal_status_t status)
{
    switch (status) {
        case HEAL_STATUS_SUCCESS:        return "success";
        case HEAL_STATUS_NO_PATTERN:     return "no_pattern";
        case HEAL_STATUS_LNN_FAILURE:    return "lnn_failure";
        case HEAL_STATUS_LOW_CONFIDENCE: return "low_confidence";
        case HEAL_STATUS_CODE_TOO_LARGE: return "code_too_large";
        case HEAL_STATUS_INVALID_INPUT:  return "invalid_input";
        case HEAL_STATUS_INTERNAL_ERROR: return "internal_error";
        default:                         return "unknown";
    }
}

const char* self_heal_mode_to_string(self_heal_mode_t mode)
{
    switch (mode) {
        case HEAL_MODE_PATTERN_ONLY: return "pattern_only";
        case HEAL_MODE_LNN_ONLY:     return "lnn_only";
        case HEAL_MODE_HYBRID:       return "hybrid";
        case HEAL_MODE_LEARNING:     return "learning";
        default:                     return "unknown";
    }
}

/* ============================================================================
 * Pattern Library Implementation
 * ============================================================================ */

pattern_library_t* heal_pattern_library_create(void)
{
    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_heal_pattern_library", 0.0f);


    pattern_library_t* lib = nimcp_calloc(1, sizeof(pattern_library_t));
    if (lib == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "heal_pattern_library_create: parameter is NULL");
        return NULL;
    }

    /* Allocate built-in patterns */
    lib->builtin_count = FIX_PATTERN_COUNT;
    lib->builtin_patterns = nimcp_calloc(lib->builtin_count, sizeof(fix_pattern_t));
    if (lib->builtin_patterns == NULL) {
        nimcp_free(lib);
        lib = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "heal_pattern_library_create: validation failed");
        return NULL;
    }

    /* Initialize built-in patterns */
    for (int i = 0; i < FIX_PATTERN_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && FIX_PATTERN_COUNT > 256) {
            self_heal_heartbeat("self_heal_loop",
                             (float)(i + 1) / (float)FIX_PATTERN_COUNT);
        }

        heal_pattern_init_builtin(&lib->builtin_patterns[i], (fix_pattern_type_t)i);
        lib->builtin_patterns[i].id = i + 1;
    }

    /* Allocate custom patterns */
    lib->custom_capacity = HEAL_PATTERN_MAX_CUSTOM_PATTERNS;
    lib->custom_patterns = nimcp_calloc(lib->custom_capacity, sizeof(fix_pattern_t));
    if (lib->custom_patterns == NULL) {
        nimcp_free(lib->builtin_patterns);
        nimcp_free(lib);
        lib = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "heal_pattern_library_create: validation failed");
        return NULL;
    }
    lib->custom_count = 0;

    /* Create mutex */
    lib->mutex = nimcp_mutex_create(NULL);
    if (lib->mutex == NULL) {
        nimcp_free(lib->custom_patterns);
        nimcp_free(lib->builtin_patterns);
        nimcp_free(lib);
        lib = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "heal_pattern_library_create: validation failed");
        return NULL;
    }

    lib->next_pattern_id = FIX_PATTERN_COUNT + 1;
    lib->initialized = true;

    return lib;
}

void heal_pattern_library_destroy(pattern_library_t* library)
{
    if (library == NULL) return;

    if (library->mutex != NULL) {
        nimcp_mutex_destroy(library->mutex);
    }

    if (library->builtin_patterns != NULL) {
        nimcp_free(library->builtin_patterns);
    }

    if (library->custom_patterns != NULL) {
        nimcp_free(library->custom_patterns);
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_heal_pattern_library", 0.0f);


    nimcp_free(library);
    library = NULL;
}

const fix_pattern_t* heal_pattern_get_by_type(
    pattern_library_t* library,
    fix_pattern_type_t type)
{
    if (library == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "heal_pattern_get_by_type: parameter is NULL");
        return NULL;
    }
    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_heal_pattern_get_by_", 0.0f);


    if (type < 0 || type >= FIX_PATTERN_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "heal_pattern_get_by_type: parameter is NULL");
        return NULL;
    }

    return &library->builtin_patterns[type];
}

const fix_pattern_t* heal_pattern_get_by_id(
    pattern_library_t* library,
    uint32_t id)
{
    if (library == NULL || id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "heal_pattern_get_by_id: parameter is NULL");
        return NULL;
    }

    /* Check built-in patterns */
    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_heal_pattern_get_by_", 0.0f);


    for (size_t i = 0; i < library->builtin_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && library->builtin_count > 256) {
            self_heal_heartbeat("self_heal_loop",
                             (float)(i + 1) / (float)library->builtin_count);
        }

        if (library->builtin_patterns[i].id == id) {
            return &library->builtin_patterns[i];
        }
    }

    /* Check custom patterns */
    nimcp_mutex_lock(library->mutex);
    for (size_t i = 0; i < library->custom_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && library->custom_count > 256) {
            self_heal_heartbeat("self_heal_loop",
                             (float)(i + 1) / (float)library->custom_count);
        }

        if (library->custom_patterns[i].id == id) {
            nimcp_mutex_unlock(library->mutex);
            return &library->custom_patterns[i];
        }
    }
    nimcp_mutex_unlock(library->mutex);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "heal_pattern_get_by_id: validation failed");
    return NULL;
}

int heal_pattern_register(
    pattern_library_t* library,
    const fix_pattern_t* pattern,
    uint32_t* id_out)
{
    if (library == NULL || pattern == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "heal_pattern_register: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_heal_pattern_registe", 0.0f);


    nimcp_mutex_lock(library->mutex);

    if (library->custom_count >= library->custom_capacity) {
        nimcp_mutex_unlock(library->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "heal_pattern_register: capacity exceeded");
        return -1;
    }

    /* Copy pattern */
    fix_pattern_t* new_pattern = &library->custom_patterns[library->custom_count];
    *new_pattern = *pattern;
    new_pattern->id = library->next_pattern_id++;
    new_pattern->user_defined = true;
    new_pattern->created_time = (uint64_t)time(NULL);
    new_pattern->enabled = true;

    library->custom_count++;

    if (id_out != NULL) {
        *id_out = new_pattern->id;
    }

    nimcp_mutex_unlock(library->mutex);

    return 0;
}

int heal_pattern_unregister(pattern_library_t* library, uint32_t id)
{
    if (library == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "heal_pattern_unregister: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_heal_pattern_unregis", 0.0f);


    nimcp_mutex_lock(library->mutex);

    for (size_t i = 0; i < library->custom_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && library->custom_count > 256) {
            self_heal_heartbeat("self_heal_loop",
                             (float)(i + 1) / (float)library->custom_count);
        }

        if (library->custom_patterns[i].id == id) {
            /* Shift remaining patterns */
            for (size_t j = i; j < library->custom_count - 1; j++) {
                library->custom_patterns[j] = library->custom_patterns[j + 1];
            }
            library->custom_count--;
            nimcp_mutex_unlock(library->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(library->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_pattern_unregister: operation failed");
    return -1;
}

int heal_pattern_match(
    pattern_library_t* library,
    const char* code,
    size_t code_len,
    pattern_match_result_t* result)
{
    if (library == NULL || code == NULL || result == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "heal_pattern_match: invalid parameter");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_heal_pattern_match", 0.0f);


    (void)code_len; /* May be used for future bounds checking */

    memset(result, 0, sizeof(pattern_match_result_t));

    /* Check for common crash patterns in code */

    /* NULL dereference pattern: ptr-> */
    if (strstr(code, "->") != NULL) {
        result->matched_type = FIX_PATTERN_NULL_CHECK;
        result->match_score = 0.8f;
        result->valid = true;
        return 0;
    }

    /* Format string vulnerability: printf(var) without format specifier */
    if ((strstr(code, "printf(") != NULL || strstr(code, "sprintf(") != NULL ||
         strstr(code, "fprintf(") != NULL) &&
        strstr(code, "\"%") == NULL) {
        result->matched_type = FIX_PATTERN_FORMAT_STRING;
        result->match_score = 0.85f;
        result->valid = true;
        return 0;
    }

    /* Resource leak: open/fopen/malloc without matching close/free */
    if ((strstr(code, "fopen(") != NULL || strstr(code, "open(") != NULL ||
         strstr(code, "socket(") != NULL) &&
        strstr(code, "fclose") == NULL && strstr(code, "close(") == NULL) {
        result->matched_type = FIX_PATTERN_RESOURCE_LEAK;
        result->match_score = 0.7f;
        result->valid = true;
        return 0;
    }

    /* Recursive function call (potential stack overflow) */
    if (strstr(code, "recursive") != NULL || strstr(code, "self_call") != NULL) {
        result->matched_type = FIX_PATTERN_STACK_OVERFLOW;
        result->match_score = 0.6f;
        result->valid = true;
        return 0;
    }

    /* Mutex lock patterns (race condition or deadlock) */
    if (strstr(code, "mutex_lock") != NULL || strstr(code, "pthread_mutex") != NULL) {
        const char* first_lock = strstr(code, "lock(");
        if (first_lock != NULL && strstr(first_lock + 1, "lock(") != NULL) {
            result->matched_type = FIX_PATTERN_LOCK_ORDER;
            result->match_score = 0.65f;
            result->valid = true;
            return 0;
        }
        result->matched_type = FIX_PATTERN_RACE_GUARD;
        result->match_score = 0.5f;
        result->valid = true;
        return 0;
    }

    /* Integer overflow: arithmetic operations with size/count/len */
    if ((strstr(code, " + ") != NULL || strstr(code, "+=") != NULL) &&
        (strstr(code, "size") != NULL || strstr(code, "count") != NULL ||
         strstr(code, "len") != NULL)) {
        result->matched_type = FIX_PATTERN_OVERFLOW_CHECK;
        result->match_score = 0.7f;
        result->valid = true;
        return 0;
    }

    /* Array access pattern: check for potential underflow vs overflow */
    if (strchr(code, '[') != NULL && strchr(code, ']') != NULL) {
        /* Buffer underflow: negative index (e.g., arr[-1] or arr[i - n]) */
        if (strstr(code, "[-") != NULL ||
            (strchr(code, '[') != NULL && strstr(code, " - ") != NULL)) {
            result->matched_type = FIX_PATTERN_BUFFER_UNDERFLOW;
            result->match_score = 0.65f;
            result->valid = true;
            return 0;
        }
        /* General bounds check for array access */
        result->matched_type = FIX_PATTERN_BOUNDS_CHECK;
        result->match_score = 0.7f;
        result->valid = true;
        return 0;
    }

    /* Uninitialized variable pattern */
    const char* decl = strstr(code, "int ");
    if (decl != NULL) {
        const char* semicolon = strchr(decl, ';');
        const char* equals = strchr(decl, '=');
        if (semicolon != NULL && (equals == NULL || equals > semicolon)) {
            result->matched_type = FIX_PATTERN_INIT_CHECK;
            result->match_score = 0.6f;
            result->valid = true;
            return 0;
        }
    }

    /* Division pattern: / */
    if (strstr(code, " / ") != NULL || strstr(code, "/=") != NULL) {
        result->matched_type = FIX_PATTERN_ZERO_CHECK;
        result->match_score = 0.75f;
        result->valid = true;
        return 0;
    }

    /* Free pattern (use-after-free or double-free) */
    if (strstr(code, "nimcp_free(") != NULL || strstr(code, "nimcp_free(") != NULL) {
        const char* free_call = strstr(code, "nimcp_free(");
        if (free_call != NULL) {
            const char* after_free = free_call + 5;
            if (strstr(after_free, "->") != NULL || strchr(after_free, '[') != NULL) {
                result->matched_type = FIX_PATTERN_UAF_CHECK;
                result->match_score = 0.8f;
                result->valid = true;
                return 0;
            }
        }
        result->matched_type = FIX_PATTERN_DOUBLE_FREE;
        result->match_score = 0.7f;
        result->valid = true;
        return 0;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_pattern_match: operation failed");
    return -1;
}

int heal_pattern_apply(
    const fix_pattern_t* pattern,
    const pattern_match_result_t* match,
    const char* original_code,
    char* fixed_code,
    size_t fixed_code_size)
{
    if (pattern == NULL || original_code == NULL ||
        fixed_code == NULL || fixed_code_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_pattern_apply: operation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_heal_pattern_apply", 0.0f);

    return apply_template_substitution(
        pattern->template_after, match, fixed_code, fixed_code_size);
}

int heal_pattern_update_stats(
    pattern_library_t* library,
    uint32_t id,
    bool success)
{
    if (library == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "heal_pattern_update_stats: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_heal_pattern_update_", 0.0f);


    nimcp_mutex_lock(library->mutex);

    fix_pattern_t* pattern = NULL;

    /* Find pattern */
    for (size_t i = 0; i < library->builtin_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && library->builtin_count > 256) {
            self_heal_heartbeat("self_heal_loop",
                             (float)(i + 1) / (float)library->builtin_count);
        }

        if (library->builtin_patterns[i].id == id) {
            pattern = &library->builtin_patterns[i];
            break;
        }
    }

    if (pattern == NULL) {
        for (size_t i = 0; i < library->custom_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && library->custom_count > 256) {
                self_heal_heartbeat("self_heal_loop",
                                 (float)(i + 1) / (float)library->custom_count);
            }

            if (library->custom_patterns[i].id == id) {
                pattern = &library->custom_patterns[i];
                break;
            }
        }
    }

    if (pattern == NULL) {
        nimcp_mutex_unlock(library->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_pattern_update_stats: validation failed");
        return -1;
    }

    /* Update stats */
    pattern->total_applications++;
    if (success) {
        pattern->success_count++;
        pattern->last_used_time = (uint64_t)time(NULL);
    } else {
        pattern->fail_count++;
    }

    /* Recalculate confidence */
    if (pattern->total_applications > 0) {
        pattern->confidence = (float)pattern->success_count /
                              (float)pattern->total_applications;
    }

    nimcp_mutex_unlock(library->mutex);

    return 0;
}

size_t heal_pattern_get_all_by_type(
    pattern_library_t* library,
    fix_pattern_type_t type,
    const fix_pattern_t** patterns_out,
    size_t max_patterns)
{
    if (library == NULL || patterns_out == NULL || max_patterns == 0) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_heal_pattern_get_all", 0.0f);


    size_t count = 0;

    /* Check built-in */
    for (size_t i = 0; i < library->builtin_count && count < max_patterns; i++) {
        if (library->builtin_patterns[i].type == type) {
            patterns_out[count++] = &library->builtin_patterns[i];
        }
    }

    /* Check custom */
    nimcp_mutex_lock(library->mutex);
    for (size_t i = 0; i < library->custom_count && count < max_patterns; i++) {
        if (library->custom_patterns[i].type == type) {
            patterns_out[count++] = &library->custom_patterns[i];
        }
    }
    nimcp_mutex_unlock(library->mutex);

    return count;
}

const fix_pattern_t* heal_pattern_get_best(
    pattern_library_t* library,
    fix_pattern_type_t type)
{
    if (library == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "heal_pattern_get_best: parameter is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_heal_pattern_get_bes", 0.0f);


    const fix_pattern_t* best = NULL;
    float best_confidence = -1.0f;

    /* Search built-in patterns */
    for (size_t i = 0; i < library->builtin_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && library->builtin_count > 256) {
            self_heal_heartbeat("self_heal_loop",
                             (float)(i + 1) / (float)library->builtin_count);
        }

        if (library->builtin_patterns[i].type == type &&
            library->builtin_patterns[i].enabled &&
            library->builtin_patterns[i].confidence > best_confidence) {
            best = &library->builtin_patterns[i];
            best_confidence = best->confidence;
        }
    }

    /* Search custom patterns */
    nimcp_mutex_lock(library->mutex);
    for (size_t i = 0; i < library->custom_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && library->custom_count > 256) {
            self_heal_heartbeat("self_heal_loop",
                             (float)(i + 1) / (float)library->custom_count);
        }

        if (library->custom_patterns[i].type == type &&
            library->custom_patterns[i].enabled &&
            library->custom_patterns[i].confidence > best_confidence) {
            best = &library->custom_patterns[i];
            best_confidence = best->confidence;
        }
    }
    nimcp_mutex_unlock(library->mutex);

    return best;
}

const char* heal_pattern_type_to_string(fix_pattern_type_t type)
{
    switch (type) {
        case FIX_PATTERN_NULL_CHECK:      return "null_check";
        case FIX_PATTERN_BOUNDS_CHECK:    return "bounds_check";
        case FIX_PATTERN_ZERO_CHECK:      return "zero_check";
        case FIX_PATTERN_UAF_CHECK:       return "uaf_check";
        case FIX_PATTERN_ALIGN_FIX:       return "align_fix";
        case FIX_PATTERN_DOUBLE_FREE:     return "double_free";
        case FIX_PATTERN_OVERFLOW_CHECK:  return "overflow_check";
        case FIX_PATTERN_INIT_CHECK:      return "init_check";
        case FIX_PATTERN_LOCK_ORDER:      return "lock_order";
        case FIX_PATTERN_RACE_GUARD:      return "race_guard";
        case FIX_PATTERN_RESOURCE_LEAK:   return "resource_leak";
        case FIX_PATTERN_FORMAT_STRING:   return "format_string";
        case FIX_PATTERN_BUFFER_UNDERFLOW:return "buffer_underflow";
        case FIX_PATTERN_STACK_OVERFLOW:  return "stack_overflow";
        case FIX_PATTERN_LNN_GENERATED:   return "lnn_generated";
        case FIX_PATTERN_CUSTOM:          return "custom";
        case FIX_PATTERN_UNKNOWN:         return "unknown";
        default:                          return "invalid";
    }
}

fix_pattern_type_t heal_pattern_type_from_string(const char* name)
{
    if (name == NULL) return FIX_PATTERN_UNKNOWN;

    if (strcmp(name, "null_check") == 0) return FIX_PATTERN_NULL_CHECK;
    if (strcmp(name, "bounds_check") == 0) return FIX_PATTERN_BOUNDS_CHECK;
    if (strcmp(name, "zero_check") == 0) return FIX_PATTERN_ZERO_CHECK;
    if (strcmp(name, "uaf_check") == 0) return FIX_PATTERN_UAF_CHECK;
    if (strcmp(name, "align_fix") == 0) return FIX_PATTERN_ALIGN_FIX;
    if (strcmp(name, "double_free") == 0) return FIX_PATTERN_DOUBLE_FREE;
    if (strcmp(name, "overflow_check") == 0) return FIX_PATTERN_OVERFLOW_CHECK;
    if (strcmp(name, "init_check") == 0) return FIX_PATTERN_INIT_CHECK;
    if (strcmp(name, "lock_order") == 0) return FIX_PATTERN_LOCK_ORDER;
    if (strcmp(name, "race_guard") == 0) return FIX_PATTERN_RACE_GUARD;
    if (strcmp(name, "resource_leak") == 0) return FIX_PATTERN_RESOURCE_LEAK;
    if (strcmp(name, "format_string") == 0) return FIX_PATTERN_FORMAT_STRING;
    if (strcmp(name, "buffer_underflow") == 0) return FIX_PATTERN_BUFFER_UNDERFLOW;
    if (strcmp(name, "stack_overflow") == 0) return FIX_PATTERN_STACK_OVERFLOW;
    if (strcmp(name, "lnn_generated") == 0) return FIX_PATTERN_LNN_GENERATED;
    if (strcmp(name, "custom") == 0) return FIX_PATTERN_CUSTOM;

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_heal_pattern_type_fr", 0.0f);


    return FIX_PATTERN_UNKNOWN;
}

int heal_pattern_init_builtin(fix_pattern_t* pattern, fix_pattern_type_t type)
{
    if (pattern == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "heal_pattern_init_builtin: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_heal_pattern_init_bu", 0.0f);


    memset(pattern, 0, sizeof(fix_pattern_t));
    pattern->type = type;
    pattern->match_strategy = PATTERN_MATCH_SUBSTRING;
    pattern->scope = PATTERN_SCOPE_EXPRESSION;
    pattern->confidence = HEAL_PATTERN_DEFAULT_CONFIDENCE;
    pattern->enabled = true;
    pattern->user_defined = false;

    switch (type) {
        case FIX_PATTERN_NULL_CHECK:
            strncpy(pattern->name, "NULL Pointer Check", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description,
                    "Add null pointer validation before dereference",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            strncpy(pattern->template_before, HEAL_PATTERN_NULL_BEFORE,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            strncpy(pattern->template_after, HEAL_PATTERN_NULL_AFTER,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            pattern->confidence = 0.9f;
            break;

        case FIX_PATTERN_BOUNDS_CHECK:
            strncpy(pattern->name, "Array Bounds Check", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description,
                    "Add bounds validation before array access",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            strncpy(pattern->template_before, HEAL_PATTERN_BOUNDS_BEFORE,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            strncpy(pattern->template_after, HEAL_PATTERN_BOUNDS_AFTER,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            pattern->confidence = 0.85f;
            break;

        case FIX_PATTERN_ZERO_CHECK:
            strncpy(pattern->name, "Division by Zero Check", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description,
                    "Add divisor validation before division",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            strncpy(pattern->template_before, HEAL_PATTERN_DIVZERO_BEFORE,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            strncpy(pattern->template_after, HEAL_PATTERN_DIVZERO_AFTER,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            pattern->confidence = 0.9f;
            break;

        case FIX_PATTERN_UAF_CHECK:
            strncpy(pattern->name, "Use-After-Free Check", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description,
                    "Add null check and set NULL after free",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            strncpy(pattern->template_before, HEAL_PATTERN_UAF_USE_BEFORE,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            strncpy(pattern->template_after, HEAL_PATTERN_UAF_USE_AFTER,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            pattern->confidence = 0.8f;
            break;

        case FIX_PATTERN_ALIGN_FIX:
            strncpy(pattern->name, "Alignment Fix", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description,
                    "Replace direct cast with memcpy for alignment safety",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            strncpy(pattern->template_before, HEAL_PATTERN_ALIGN_BEFORE,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            strncpy(pattern->template_after, HEAL_PATTERN_ALIGN_AFTER,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            pattern->confidence = 0.75f;
            break;

        case FIX_PATTERN_DOUBLE_FREE:
            strncpy(pattern->name, "Double-Free Protection", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description,
                    "Add null check and set NULL after free",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            strncpy(pattern->template_before, HEAL_PATTERN_DOUBLE_FREE_BEFORE,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            strncpy(pattern->template_after, HEAL_PATTERN_DOUBLE_FREE_AFTER,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            pattern->confidence = 0.85f;
            break;

        case FIX_PATTERN_OVERFLOW_CHECK:
            strncpy(pattern->name, "Integer Overflow Check", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description,
                    "Add overflow check before arithmetic operation",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            strncpy(pattern->template_before, HEAL_PATTERN_OVERFLOW_BEFORE,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            strncpy(pattern->template_after, HEAL_PATTERN_OVERFLOW_AFTER,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            pattern->confidence = 0.7f;
            break;

        case FIX_PATTERN_INIT_CHECK:
            strncpy(pattern->name, "Uninitialized Variable Check", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description,
                    "Initialize variable before use",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            pattern->confidence = 0.6f;
            pattern->enabled = false; /* Needs more context */
            break;

        case FIX_PATTERN_LOCK_ORDER:
            strncpy(pattern->name, "Lock Ordering", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description,
                    "Enforce consistent lock acquisition order",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            pattern->confidence = 0.5f;
            pattern->enabled = false; /* Complex pattern */
            break;

        case FIX_PATTERN_RACE_GUARD:
            strncpy(pattern->name, "Race Condition Guard", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description,
                    "Add synchronization for shared data access",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            strncpy(pattern->template_before, HEAL_PATTERN_RACE_BEFORE,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            strncpy(pattern->template_after, HEAL_PATTERN_RACE_AFTER,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            pattern->confidence = 0.6f;
            pattern->enabled = true;
            break;

        case FIX_PATTERN_RESOURCE_LEAK:
            strncpy(pattern->name, "Resource Leak Fix", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description,
                    "Add cleanup code for unclosed handles/memory",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            strncpy(pattern->template_before, HEAL_PATTERN_RESOURCE_LEAK_BEFORE,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            strncpy(pattern->template_after, HEAL_PATTERN_RESOURCE_LEAK_AFTER,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            pattern->confidence = 0.7f;
            pattern->enabled = true;
            break;

        case FIX_PATTERN_FORMAT_STRING:
            strncpy(pattern->name, "Format String Fix", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description,
                    "Fix format string vulnerability with explicit format",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            strncpy(pattern->template_before, HEAL_PATTERN_FORMAT_STRING_BEFORE,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            strncpy(pattern->template_after, HEAL_PATTERN_FORMAT_STRING_AFTER,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            pattern->confidence = 0.85f;
            pattern->enabled = true;
            break;

        case FIX_PATTERN_BUFFER_UNDERFLOW:
            strncpy(pattern->name, "Buffer Underflow Check", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description,
                    "Add lower bound check for negative array index",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            strncpy(pattern->template_before, HEAL_PATTERN_UNDERFLOW_BEFORE,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            strncpy(pattern->template_after, HEAL_PATTERN_UNDERFLOW_AFTER,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            pattern->confidence = 0.8f;
            pattern->enabled = true;
            break;

        case FIX_PATTERN_STACK_OVERFLOW:
            strncpy(pattern->name, "Stack Overflow Guard", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description,
                    "Add recursion depth limit to prevent stack overflow",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            strncpy(pattern->template_before, HEAL_PATTERN_STACK_OVERFLOW_BEFORE,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            strncpy(pattern->template_after, HEAL_PATTERN_STACK_OVERFLOW_AFTER,
                    HEAL_PATTERN_MAX_TEMPLATE_SIZE - 1);
            pattern->confidence = 0.65f;
            pattern->enabled = true;
            break;

        case FIX_PATTERN_LNN_GENERATED:
            strncpy(pattern->name, "LNN Generated", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description,
                    "Fix generated by neural network prediction",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            pattern->confidence = 0.5f;
            pattern->enabled = true;
            break;

        case FIX_PATTERN_CUSTOM:
        case FIX_PATTERN_UNKNOWN:
        default:
            strncpy(pattern->name, "Unknown", HEAL_PATTERN_MAX_NAME_SIZE - 1);
            strncpy(pattern->description, "Unknown pattern type",
                    HEAL_PATTERN_MAX_DESCRIPTION_SIZE - 1);
            pattern->confidence = 0.0f;
            pattern->enabled = false;
            break;
    }

    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about self heal
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int self_heal_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    self_heal_heartbeat("self_heal_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Self_Heal");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                self_heal_heartbeat("self_heal_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Self heal self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Self_Heal");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Self_Heal");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void self_heal_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_self_heal_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int self_heal_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_heal_training_begin: NULL argument");
        return -1;
    }
    self_heal_heartbeat_instance(NULL, "self_heal_training_begin", 0.0f);
    return 0;
}

int self_heal_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_heal_training_end: NULL argument");
        return -1;
    }
    self_heal_heartbeat_instance(NULL, "self_heal_training_end", 1.0f);
    return 0;
}

int self_heal_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_heal_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    self_heal_heartbeat_instance(NULL, "self_heal_training_step", progress);
    return 0;
}
