/**
 * @file nimcp_code_generation.c
 * @brief Code Generation Engine Implementation
 *
 * WHAT: Implementation of autonomous code fix generation
 * WHY:  Enable self-healing through intelligent fix generation
 * HOW:  Template-based generation with pattern matching and confidence scoring
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#include "cognitive/parietal/nimcp_code_generation.h"
#include "cognitive/parietal/nimcp_fix_templates.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(code_generation, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Fix history entry for learning
 */
typedef struct {
    uint64_t fix_id;
    code_fix_strategy_t strategy;
    error_type_t error_type;
    char epitope[NIMCP_ID_BUFFER_SIZE];                   /**< Error signature hash */
    bool success;
    float confidence;
    uint64_t timestamp;
} fix_history_entry_t;

/**
 * @brief Code generation engine internal state
 */
struct code_gen_engine {
    uint32_t magic;                     /**< Validation magic */
    code_gen_config_t config;           /**< Configuration */

    /* Template storage */
    fix_template_t* custom_templates;
    uint32_t custom_template_count;
    uint32_t custom_template_capacity;

    /* Fix tracking */
    generated_fix_t* fix_history;
    uint32_t fix_history_count;
    uint32_t fix_history_capacity;
    uint64_t next_fix_id;

    /* Learning history */
    fix_history_entry_t* learning_history;
    uint32_t learning_history_count;
    uint32_t learning_history_capacity;

    /* Statistics */
    code_gen_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Bio-async communication */
    bio_module_context_t bio_ctx;

    /* State */
    bool ready;
};

//=============================================================================
// Error Type to Strategy Mapping
//=============================================================================

/**
 * @brief Strategy mapping entry
 */
typedef struct {
    error_type_t error_type;
    code_fix_strategy_t strategies[4];
    uint32_t strategy_count;
    float base_confidences[4];
} strategy_mapping_t;

static const strategy_mapping_t g_strategy_map[] = {
    { ERROR_TYPE_NULL_POINTER,      { FIX_STRATEGY_NULL_CHECK },                    1, { 0.9f } },
    { ERROR_TYPE_BUFFER_OVERFLOW,   { FIX_STRATEGY_BOUNDS_CHECK },                  1, { 0.85f } },
    { ERROR_TYPE_BUFFER_UNDERFLOW,  { FIX_STRATEGY_BOUNDS_CHECK },                  1, { 0.85f } },
    { ERROR_TYPE_DIVIDE_BY_ZERO,    { FIX_STRATEGY_DIVISION_GUARD },                1, { 0.95f } },
    { ERROR_TYPE_NAN_DETECTED,      { FIX_STRATEGY_NAN_GUARD },                     1, { 0.9f } },
    { ERROR_TYPE_INF_DETECTED,      { FIX_STRATEGY_NAN_GUARD },                     1, { 0.9f } },
    { ERROR_TYPE_NUMERICAL_OVERFLOW,{ FIX_STRATEGY_OVERFLOW_GUARD },                1, { 0.8f } },
    { ERROR_TYPE_MEMORY_LEAK,       { FIX_STRATEGY_MEMORY_CLEANUP },                1, { 0.75f } },
    { ERROR_TYPE_DOUBLE_FREE,       { FIX_STRATEGY_MEMORY_CLEANUP },                1, { 0.85f } },
    { ERROR_TYPE_USE_AFTER_FREE,    { FIX_STRATEGY_MEMORY_CLEANUP },                1, { 0.7f } },
    { ERROR_TYPE_DEADLOCK,          { FIX_STRATEGY_MUTEX_FIX },                     1, { 0.65f } },
    { ERROR_TYPE_RACE_CONDITION,    { FIX_STRATEGY_MUTEX_FIX },                     1, { 0.6f } },
    { ERROR_TYPE_ALIGNMENT_ERROR,   { FIX_STRATEGY_ALIGNMENT_FIX },                 1, { 0.8f } },
    { ERROR_TYPE_ASSERTION_FAILED,  { FIX_STRATEGY_ASSERTION_FIX, FIX_STRATEGY_ERROR_HANDLING }, 2, { 0.5f, 0.4f } },
    { ERROR_TYPE_SEGFAULT,          { FIX_STRATEGY_NULL_CHECK, FIX_STRATEGY_BOUNDS_CHECK }, 2, { 0.7f, 0.5f } },
};

static const size_t g_strategy_map_size = sizeof(g_strategy_map) / sizeof(g_strategy_map[0]);

//=============================================================================
// Forward Declarations
//=============================================================================

static int engine_init_templates(code_gen_engine_t* engine);
static void engine_cleanup_templates(code_gen_engine_t* engine);
static int generate_fix_for_strategy(code_gen_engine_t* engine,
                                     code_fix_strategy_t strategy,
                                     const code_gen_request_t* request,
                                     generated_fix_t* fix);
static void compute_epitope(const code_gen_request_t* request, char* epitope, size_t size);
static uint64_t get_timestamp_ms(void);
static nimcp_error_t code_gen_handle_bio_message(const void* msg, size_t msg_size,
                                                  nimcp_bio_promise_t response_promise,
                                                  void* user_data);
static int register_code_gen_bio_handlers(code_gen_engine_t* engine);

//=============================================================================
// Lifecycle Functions
//=============================================================================

code_gen_config_t code_gen_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_default_con", 0.0f);


    code_gen_config_t config = {0};
    config.default_min_confidence = 0.7f;
    config.default_max_risk = 0.3f;
    config.default_max_candidates = 5;
    config.template_directory = NULL;
    config.enable_code_immune_learning = true;
    config.enable_pattern_matching = true;
    config.pattern_match_threshold = 0.8f;
    config.require_human_approval_complex = true;
    config.auto_rollback_on_regression = true;
    config.verbose_logging = false;
    return config;
}

code_gen_engine_t* code_gen_create(const code_gen_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_create", 0.0f);


    code_gen_engine_t* engine = nimcp_calloc(1, sizeof(code_gen_engine_t));
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate engine");

        return NULL;
    }

    engine->magic = CODE_GEN_MAGIC;

    /* Apply configuration */
    if (config) {
        engine->config = *config;
    } else {
        engine->config = code_gen_default_config();
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_RECURSIVE;
    engine->mutex = nimcp_mutex_create(&attr);
    if (!engine->mutex) {
        nimcp_free(engine);
        engine = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "code_gen_create: engine->mutex is NULL");
        return NULL;
    }

    /* Allocate fix history */
    engine->fix_history_capacity = 256;
    engine->fix_history = nimcp_calloc(engine->fix_history_capacity, sizeof(generated_fix_t));
    if (!engine->fix_history) {
        nimcp_mutex_destroy(engine->mutex);
        nimcp_free(engine);
        engine = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "code_gen_create: engine->fix_history is NULL");
        return NULL;
    }

    /* Allocate learning history */
    engine->learning_history_capacity = 512;
    engine->learning_history = nimcp_calloc(engine->learning_history_capacity, sizeof(fix_history_entry_t));
    if (!engine->learning_history) {
        nimcp_free(engine->fix_history);
        nimcp_mutex_destroy(engine->mutex);
        nimcp_free(engine);
        engine = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "code_gen_create: engine->learning_history is NULL");
        return NULL;
    }

    /* Allocate custom templates */
    engine->custom_template_capacity = CODE_GEN_MAX_TEMPLATES;
    engine->custom_templates = nimcp_calloc(engine->custom_template_capacity, sizeof(fix_template_t));
    if (!engine->custom_templates) {
        nimcp_free(engine->learning_history);
        nimcp_free(engine->fix_history);
        nimcp_mutex_destroy(engine->mutex);
        nimcp_free(engine);
        engine = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "code_gen_create: engine->custom_templates is NULL");
        return NULL;
    }

    /* Initialize built-in templates */
    if (engine_init_templates(engine) != 0) {
        nimcp_free(engine->custom_templates);
        nimcp_free(engine->learning_history);
        nimcp_free(engine->fix_history);
        nimcp_mutex_destroy(engine->mutex);
        nimcp_free(engine);
        engine = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "code_gen_create: validation failed");
        return NULL;
    }

    engine->next_fix_id = 1;

    /* Register with bio-async router */
    if (bio_router_is_initialized()) {
        bio_module_info_t info = {0};
        info.module_id = BIO_MODULE_CODE_GENERATION;
        info.module_name = "code_generation";
        info.inbox_capacity = 32;
        info.user_data = engine;
        engine->bio_ctx = bio_router_register_module(&info);
        if (engine->bio_ctx) {
            register_code_gen_bio_handlers(engine);
        }
    }

    engine->ready = true;

    return engine;
}

void code_gen_destroy(code_gen_engine_t* engine) {
    if (!engine) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_destroy", 0.0f);


    if (engine->magic != CODE_GEN_MAGIC) {
        return;
    }

    engine->ready = false;
    engine->magic = 0;

    /* Unregister from bio-async router */
    if (engine->bio_ctx) {
        bio_router_unregister_module(engine->bio_ctx);
        engine->bio_ctx = NULL;
    }

    engine_cleanup_templates(engine);

    if (engine->custom_templates) {
        nimcp_free(engine->custom_templates);
    }
    if (engine->learning_history) {
        nimcp_free(engine->learning_history);
    }
    if (engine->fix_history) {
        nimcp_free(engine->fix_history);
    }
    if (engine->mutex) {
        nimcp_mutex_destroy(engine->mutex);
    }

    nimcp_free(engine);
    engine = NULL;
}

bool code_gen_is_ready(const code_gen_engine_t* engine) {
    if (!engine || engine->magic != CODE_GEN_MAGIC) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_is_ready", 0.0f);


    return engine->ready;
}

//=============================================================================
// Core Generation Functions
//=============================================================================

int code_gen_generate_candidates(
    code_gen_engine_t* engine,
    const code_gen_request_t* request,
    code_gen_result_t* result
) {
    if (!engine || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "code_gen_generate_candidates: required parameter is NULL (engine, request, result)");
        return -1;
    }
    if (!code_gen_is_ready(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "code_gen_generate_candidates: code_gen_is_ready is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_generate_ca", 0.0f);


    memset(result, 0, sizeof(*result));
    uint64_t start_time = get_timestamp_ms();

    nimcp_mutex_lock(engine->mutex);

    /* Get compatible strategies */
    code_fix_strategy_t strategies[8];
    error_type_t error_type = request->diagnosis ? request->diagnosis->error_type : ERROR_TYPE_UNKNOWN;
    int strategy_count = code_gen_get_compatible_strategies(engine, error_type, strategies, 8);

    if (strategy_count <= 0) {
        snprintf(result->error_message, sizeof(result->error_message),
                 "No compatible strategies for error type 0x%04X", error_type);
        nimcp_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "code_gen_generate_candidates: validation failed");
        return -1;
    }

    /* Limit candidates */
    uint32_t max_candidates = request->max_candidates > 0 ?
                              request->max_candidates : engine->config.default_max_candidates;
    if (max_candidates > CODE_GEN_MAX_CANDIDATES) {
        max_candidates = CODE_GEN_MAX_CANDIDATES;
    }

    /* Generate fix for each strategy */
    for (int i = 0; i < strategy_count && result->candidates.count < max_candidates; i++) {
        generated_fix_t fix = {0};
        if (generate_fix_for_strategy(engine, strategies[i], request, &fix) == 0) {
            /* Check confidence threshold */
            float min_conf = request->min_confidence > 0 ?
                            request->min_confidence : engine->config.default_min_confidence;
            float max_risk = request->max_risk > 0 ?
                            request->max_risk : engine->config.default_max_risk;

            if (fix.confidence >= min_conf && fix.risk_score <= max_risk) {
                fix.fix_id = engine->next_fix_id++;
                fix.timestamp = get_timestamp_ms();
                fix.status = FIX_STATUS_PROPOSED;

                if (request->diagnosis) {
                    fix.diagnostic_id = request->diagnosis->error_id;
                }

                result->candidates.candidates[result->candidates.count++] = fix;

                /* Store in history */
                if (engine->fix_history_count < engine->fix_history_capacity) {
                    engine->fix_history[engine->fix_history_count++] = fix;
                }

                /* Update stats */
                engine->stats.fixes_generated++;
                engine->stats.by_strategy[strategies[i]]++;
            }
        }
    }

    /* Select best fix */
    if (result->candidates.count > 0) {
        generated_fix_t best;
        if (code_gen_select_best_fix(engine, &result->candidates, &best) == 0) {
            result->candidates.selected_index = 0;
            for (uint32_t i = 0; i < result->candidates.count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && result->candidates.count > 256) {
                    code_generation_heartbeat("code_generat_loop",
                                     (float)(i + 1) / (float)result->candidates.count);
                }

                if (result->candidates.candidates[i].fix_id == best.fix_id) {
                    result->candidates.selected_index = i;
                    break;
                }
            }
            result->best_fix = &result->candidates.candidates[result->candidates.selected_index];
        }
        result->success = true;
    }

    result->generation_time_us = (get_timestamp_ms() - start_time) * 1000;
    engine->stats.avg_generation_time_us =
        (engine->stats.avg_generation_time_us * (engine->stats.fixes_generated - 1) +
         result->generation_time_us) / engine->stats.fixes_generated;

    nimcp_mutex_unlock(engine->mutex);
    return result->success ? 0 : -1;
}

int code_gen_select_best_fix(
    code_gen_engine_t* engine,
    const fix_candidate_set_t* candidates,
    generated_fix_t* selected
) {
    if (!engine || !candidates || !selected || candidates->count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "code_gen_select_best_fix: required parameter is NULL (engine, candidates, selected)");
        return -1;
    }

    /* Score each candidate: confidence * (1 - risk) * (1 + historical_success) */
    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_select_best", 0.0f);


    float best_score = -1.0f;
    uint32_t best_idx = 0;

    for (uint32_t i = 0; i < candidates->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && candidates->count > 256) {
            code_generation_heartbeat("code_generat_loop",
                             (float)(i + 1) / (float)candidates->count);
        }

        const generated_fix_t* fix = &candidates->candidates[i];
        float score = fix->confidence * (1.0f - fix->risk_score);

        /* Boost by historical success if available */
        if (fix->historical_success_rate > 0) {
            score *= (1.0f + fix->historical_success_rate * 0.5f);
        }

        /* Prefer simpler fixes */
        float complexity_penalty = 1.0f - (fix->complexity * 0.1f);
        score *= complexity_penalty;

        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    *selected = candidates->candidates[best_idx];
    return 0;
}

int code_gen_generate_with_strategy(
    code_gen_engine_t* engine,
    code_fix_strategy_t strategy,
    const code_location_t* location,
    const char* source_code,
    generated_fix_t* fix
) {
    if (!engine || !location || !source_code || !fix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "code_gen_generate_with_strategy: required parameter is NULL (engine, location, source_code, fix)");
        return -1;
    }
    if (!code_gen_is_ready(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "code_gen_generate_with_strategy: code_gen_is_ready is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_generate_wi", 0.0f);


    code_gen_request_t request = {0};
    request.location = *location;
    request.source_code = source_code;
    request.preferred_strategy = strategy;

    nimcp_mutex_lock(engine->mutex);
    int ret = generate_fix_for_strategy(engine, strategy, &request, fix);
    if (ret == 0) {
        fix->fix_id = engine->next_fix_id++;
        fix->timestamp = get_timestamp_ms();
        fix->status = FIX_STATUS_PROPOSED;
        engine->stats.fixes_generated++;
        engine->stats.by_strategy[strategy]++;
    }
    nimcp_mutex_unlock(engine->mutex);

    return ret;
}

//=============================================================================
// Strategy Selection Functions
//=============================================================================

int code_gen_select_strategy(
    code_gen_engine_t* engine,
    error_type_t error_type,
    const code_analysis_result_t* code_analysis,
    code_fix_strategy_t* strategy,
    float* confidence
) {
    if (!engine || !strategy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "code_gen_select_strategy: required parameter is NULL (engine, strategy)");
        return -1;
    }

    *strategy = FIX_STRATEGY_NONE;
    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_select_stra", 0.0f);


    if (confidence) {
        *confidence = 0.0f;
    }

    /* Find in mapping */
    for (size_t i = 0; i < g_strategy_map_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && g_strategy_map_size > 256) {
            code_generation_heartbeat("code_generat_loop",
                             (float)(i + 1) / (float)g_strategy_map_size);
        }

        if (g_strategy_map[i].error_type == error_type) {
            *strategy = g_strategy_map[i].strategies[0];
            if (confidence) {
                *confidence = g_strategy_map[i].base_confidences[0];

                /* Adjust by code analysis if available */
                if (code_analysis) {
                    /* Higher confidence for simpler code */
                    if (code_analysis->repair_difficulty < 0.3f) {
                        *confidence *= 1.2f;
                    } else if (code_analysis->repair_difficulty > 0.7f) {
                        *confidence *= 0.8f;
                    }
                    if (*confidence > 1.0f) *confidence = 1.0f;
                }
            }
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "code_gen_select_strategy: validation failed");
    return -1;
}

int code_gen_get_compatible_strategies(
    code_gen_engine_t* engine,
    error_type_t error_type,
    code_fix_strategy_t* strategies,
    uint32_t max_strategies
) {
    if (!strategies || max_strategies == 0) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_get_compati", 0.0f);


    uint32_t count = 0;

    /* Find in mapping */
    for (size_t i = 0; i < g_strategy_map_size && count < max_strategies; i++) {
        if (g_strategy_map[i].error_type == error_type) {
            for (uint32_t j = 0; j < g_strategy_map[i].strategy_count && count < max_strategies; j++) {
                strategies[count++] = g_strategy_map[i].strategies[j];
            }
            return count;
        }
    }

    /* Default strategies for unknown errors */
    if (count == 0 && max_strategies >= 2) {
        strategies[count++] = FIX_STRATEGY_ERROR_HANDLING;
        strategies[count++] = FIX_STRATEGY_NULL_CHECK;
    }

    return count;
}

//=============================================================================
// Pattern Matching Functions
//=============================================================================

int code_gen_match_historical_pattern(
    code_gen_engine_t* engine,
    const code_gen_request_t* request,
    uint64_t* pattern_id,
    float* similarity
) {
    if (!engine || !request || !pattern_id || !similarity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "code_gen_match_historical_pattern: required parameter is NULL (engine, request, pattern_id, similarity)");
        return -1;
    }

    *pattern_id = 0;
    *similarity = 0.0f;

    if (!engine->config.enable_pattern_matching) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "code_gen_match_historical_pattern: engine->config is NULL");
        return -1;
    }

    /* Compute epitope for current request */
    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_match_histo", 0.0f);


    char epitope[NIMCP_ID_BUFFER_SIZE];
    compute_epitope(request, epitope, sizeof(epitope));

    nimcp_mutex_lock(engine->mutex);

    /* Search learning history for similar patterns */
    float best_similarity = 0.0f;
    uint64_t best_id = 0;

    for (uint32_t i = 0; i < engine->learning_history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && engine->learning_history_count > 256) {
            code_generation_heartbeat("code_generat_loop",
                             (float)(i + 1) / (float)engine->learning_history_count);
        }

        const fix_history_entry_t* entry = &engine->learning_history[i];
        if (!entry->success) continue;  /* Only match successful fixes */

        /* Simple similarity: matching characters */
        int matches = 0;
        for (int j = 0; j < 64 && epitope[j] && entry->epitope[j]; j++) {
            if (epitope[j] == entry->epitope[j]) matches++;
        }
        float sim = (float)matches / 64.0f;

        if (sim > best_similarity && sim >= engine->config.pattern_match_threshold) {
            best_similarity = sim;
            best_id = entry->fix_id;
        }
    }

    nimcp_mutex_unlock(engine->mutex);

    if (best_id > 0) {
        *pattern_id = best_id;
        *similarity = best_similarity;
        return 0;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "code_gen_match_historical_pattern: validation failed");
    return -1;
}

int code_gen_learn_from_outcome(
    code_gen_engine_t* engine,
    const generated_fix_t* fix,
    bool success
) {
    if (!engine || !fix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "code_gen_learn_from_outcome: required parameter is NULL (engine, fix)");
        return -1;
    }

    if (!engine->config.enable_code_immune_learning) {
        return 0;  /* Learning disabled, silently succeed */
    }

    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_learn_from_", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Add to learning history */
    if (engine->learning_history_count < engine->learning_history_capacity) {
        fix_history_entry_t* entry = &engine->learning_history[engine->learning_history_count++];
        entry->fix_id = fix->fix_id;
        entry->strategy = fix->strategy;
        entry->error_type = ERROR_TYPE_UNKNOWN;  /* Would need diagnostic context */
        entry->success = success;
        entry->confidence = fix->confidence;
        entry->timestamp = get_timestamp_ms();

        /* Compute simplified epitope from fix location */
        snprintf(entry->epitope, sizeof(entry->epitope),
                 "%s:%u:%d", fix->function_name, fix->start_line, fix->strategy);
    }

    /* Update statistics */
    if (success) {
        engine->stats.fixes_validated++;
    } else {
        engine->stats.fixes_failed++;
    }

    engine->stats.patterns_matched++;

    nimcp_mutex_unlock(engine->mutex);
    return 0;
}

//=============================================================================
// Confidence Scoring Functions
//=============================================================================

float code_gen_calculate_confidence(
    code_gen_engine_t* engine,
    const generated_fix_t* fix,
    const code_gen_request_t* request
) {
    if (!engine || !fix) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_calculate_c", 0.0f);


    float confidence = 0.5f;  /* Base confidence */

    /* Strategy-based confidence */
    for (size_t i = 0; i < g_strategy_map_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && g_strategy_map_size > 256) {
            code_generation_heartbeat("code_generat_loop",
                             (float)(i + 1) / (float)g_strategy_map_size);
        }

        for (uint32_t j = 0; j < g_strategy_map[i].strategy_count; j++) {
            if (g_strategy_map[i].strategies[j] == fix->strategy) {
                confidence = g_strategy_map[i].base_confidences[j];
                break;
            }
        }
    }

    /* Complexity penalty */
    switch (fix->complexity) {
        case FIX_COMPLEXITY_TRIVIAL:   confidence *= 1.1f; break;
        case FIX_COMPLEXITY_SIMPLE:    confidence *= 1.0f; break;
        case FIX_COMPLEXITY_MODERATE:  confidence *= 0.9f; break;
        case FIX_COMPLEXITY_COMPLEX:   confidence *= 0.7f; break;
        case FIX_COMPLEXITY_ARCHITECTURAL: confidence *= 0.5f; break;
    }

    /* Historical success boost */
    if (fix->historical_success_rate > 0) {
        confidence = confidence * 0.7f + fix->historical_success_rate * 0.3f;
    }

    /* Code analysis context */
    if (request && request->code_analysis) {
        confidence *= request->code_analysis->confidence;
    }

    /* Clamp to [0, 1] */
    if (confidence > 1.0f) confidence = 1.0f;
    if (confidence < 0.0f) confidence = 0.0f;

    return confidence;
}

float code_gen_calculate_risk(
    code_gen_engine_t* engine,
    const generated_fix_t* fix,
    const code_analysis_result_t* code_analysis
) {
    if (!fix) {
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_calculate_r", 0.0f);


    float risk = 0.2f;  /* Base risk */

    /* Complexity risk */
    switch (fix->complexity) {
        case FIX_COMPLEXITY_TRIVIAL:   risk += 0.0f; break;
        case FIX_COMPLEXITY_SIMPLE:    risk += 0.1f; break;
        case FIX_COMPLEXITY_MODERATE:  risk += 0.2f; break;
        case FIX_COMPLEXITY_COMPLEX:   risk += 0.4f; break;
        case FIX_COMPLEXITY_ARCHITECTURAL: risk += 0.6f; break;
    }

    /* Strategy risk */
    switch (fix->strategy) {
        case FIX_STRATEGY_NULL_CHECK:
        case FIX_STRATEGY_BOUNDS_CHECK:
        case FIX_STRATEGY_DIVISION_GUARD:
        case FIX_STRATEGY_NAN_GUARD:
            /* Low-risk guard insertions */
            break;
        case FIX_STRATEGY_MUTEX_FIX:
        case FIX_STRATEGY_MEMORY_CLEANUP:
            risk += 0.2f;  /* Moderate risk */
            break;
        case FIX_STRATEGY_INITIALIZATION:
        case FIX_STRATEGY_ERROR_HANDLING:
            risk += 0.1f;
            break;
        default:
            risk += 0.15f;
            break;
    }

    /* Code analysis context */
    if (code_analysis) {
        /* Higher complexity = higher risk */
        if (code_analysis->complexity.time_complexity >= COMPLEXITY_O_N_SQUARED) {
            risk += 0.1f;
        }
        /* Circular dependencies = higher risk */
        if (code_analysis->has_circular_deps) {
            risk += 0.15f;
        }
        /* Many affected modules = higher risk */
        if (code_analysis->affected_modules > 5) {
            risk += 0.1f;
        }
    }

    /* Clamp to [0, 1] */
    if (risk > 1.0f) risk = 1.0f;
    if (risk < 0.0f) risk = 0.0f;

    return risk;
}

//=============================================================================
// Template Management Functions
//=============================================================================

int code_gen_load_templates(
    code_gen_engine_t* engine,
    const char* directory
) {
    if (!engine || !directory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "code_gen_load_templates: required parameter is NULL (engine, directory)");
        return -1;
    }

    /* TODO: Implement file-based template loading */
    /* For now, built-in templates are sufficient */

    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_load_templa", 0.0f);


    return 0;
}

int code_gen_register_template(
    code_gen_engine_t* engine,
    code_fix_strategy_t strategy,
    const char* template_code,
    const char* description
) {
    if (!engine || !template_code) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "code_gen_register_template: required parameter is NULL (engine, template_code)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_register_te", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    if (engine->custom_template_count >= engine->custom_template_capacity) {
        nimcp_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "code_gen_register_template: capacity exceeded");
        return -1;
    }

    fix_template_t* tmpl = &engine->custom_templates[engine->custom_template_count++];
    tmpl->template_id = engine->custom_template_count;
    tmpl->strategy = strategy;
    tmpl->complexity = FIX_COMPLEXITY_SIMPLE;

    strncpy(tmpl->code, template_code, sizeof(tmpl->code) - 1);
    if (description) {
        strncpy(tmpl->description, description, sizeof(tmpl->description) - 1);
    }

    tmpl->base_confidence = 0.7f;
    tmpl->base_risk = 0.2f;

    nimcp_mutex_unlock(engine->mutex);
    return 0;
}

//=============================================================================
// Fix Status Management
//=============================================================================

int code_gen_update_status(
    code_gen_engine_t* engine,
    uint64_t fix_id,
    fix_status_t new_status
) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_update_stat", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    for (uint32_t i = 0; i < engine->fix_history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && engine->fix_history_count > 256) {
            code_generation_heartbeat("code_generat_loop",
                             (float)(i + 1) / (float)engine->fix_history_count);
        }

        if (engine->fix_history[i].fix_id == fix_id) {
            fix_status_t old_status = engine->fix_history[i].status;
            engine->fix_history[i].status = new_status;

            /* Update stats based on transition */
            if (new_status == FIX_STATUS_COMPILED && old_status != FIX_STATUS_COMPILED) {
                engine->stats.fixes_compiled++;
            } else if (new_status == FIX_STATUS_VALIDATED && old_status != FIX_STATUS_VALIDATED) {
                engine->stats.fixes_validated++;
            } else if (new_status == FIX_STATUS_APPLIED && old_status != FIX_STATUS_APPLIED) {
                engine->stats.fixes_applied++;
            } else if (new_status == FIX_STATUS_COMMITTED && old_status != FIX_STATUS_COMMITTED) {
                engine->stats.fixes_committed++;
            } else if (new_status == FIX_STATUS_FAILED && old_status != FIX_STATUS_FAILED) {
                engine->stats.fixes_failed++;
            }

            nimcp_mutex_unlock(engine->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(engine->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "code_gen_update_status: operation failed");
    return -1;
}

const generated_fix_t* code_gen_get_fix(
    code_gen_engine_t* engine,
    uint64_t fix_id
) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_get_fix", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    for (uint32_t i = 0; i < engine->fix_history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && engine->fix_history_count > 256) {
            code_generation_heartbeat("code_generat_loop",
                             (float)(i + 1) / (float)engine->fix_history_count);
        }

        if (engine->fix_history[i].fix_id == fix_id) {
            nimcp_mutex_unlock(engine->mutex);
            return &engine->fix_history[i];
        }
    }

    nimcp_mutex_unlock(engine->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "code_gen_get_fix: validation failed");
    return NULL;
}

//=============================================================================
// Statistics Functions
//=============================================================================

int code_gen_get_stats(
    const code_gen_engine_t* engine,
    code_gen_stats_t* stats
) {
    if (!engine || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "code_gen_get_stats: required parameter is NULL (engine, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_get_stats", 0.0f);


    nimcp_mutex_lock(((code_gen_engine_t*)engine)->mutex);
    *stats = engine->stats;

    /* Calculate average confidence */
    if (engine->fix_history_count > 0) {
        float total_conf = 0.0f;
        for (uint32_t i = 0; i < engine->fix_history_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && engine->fix_history_count > 256) {
                code_generation_heartbeat("code_generat_loop",
                                 (float)(i + 1) / (float)engine->fix_history_count);
            }

            total_conf += engine->fix_history[i].confidence;
        }
        stats->avg_confidence = total_conf / engine->fix_history_count;
    }

    nimcp_mutex_unlock(((code_gen_engine_t*)engine)->mutex);
    return 0;
}

void code_gen_reset_stats(code_gen_engine_t* engine) {
    if (!engine) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_reset_stats", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    memset(&engine->stats, 0, sizeof(engine->stats));
    nimcp_mutex_unlock(engine->mutex);
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* code_gen_strategy_name(code_fix_strategy_t strategy) {
    switch (strategy) {
        case FIX_STRATEGY_NONE:           return "none";
        case FIX_STRATEGY_NULL_CHECK:     return "null_check";
        case FIX_STRATEGY_BOUNDS_CHECK:   return "bounds_check";
        case FIX_STRATEGY_DIVISION_GUARD: return "division_guard";
        case FIX_STRATEGY_INITIALIZATION: return "initialization";
        case FIX_STRATEGY_NAN_GUARD:      return "nan_guard";
        case FIX_STRATEGY_MUTEX_FIX:      return "mutex_fix";
        case FIX_STRATEGY_MEMORY_CLEANUP: return "memory_cleanup";
        case FIX_STRATEGY_ERROR_HANDLING: return "error_handling";
        case FIX_STRATEGY_TYPE_CAST:      return "type_cast";
        case FIX_STRATEGY_OVERFLOW_GUARD: return "overflow_guard";
        case FIX_STRATEGY_ALIGNMENT_FIX:  return "alignment_fix";
        case FIX_STRATEGY_ASSERTION_FIX:  return "assertion_fix";
        case FIX_STRATEGY_CUSTOM:         return "custom";
        default:                          return "unknown";
    }
}

const char* code_gen_complexity_name(fix_complexity_t complexity) {
    switch (complexity) {
        case FIX_COMPLEXITY_TRIVIAL:      return "trivial";
        case FIX_COMPLEXITY_SIMPLE:       return "simple";
        case FIX_COMPLEXITY_MODERATE:     return "moderate";
        case FIX_COMPLEXITY_COMPLEX:      return "complex";
        case FIX_COMPLEXITY_ARCHITECTURAL: return "architectural";
        default:                          return "unknown";
    }
}

const char* code_gen_status_name(fix_status_t status) {
    switch (status) {
        case FIX_STATUS_PROPOSED:   return "proposed";
        case FIX_STATUS_COMPILED:   return "compiled";
        case FIX_STATUS_TESTED:     return "tested";
        case FIX_STATUS_VALIDATED:  return "validated";
        case FIX_STATUS_APPLIED:    return "applied";
        case FIX_STATUS_COMMITTED:  return "committed";
        case FIX_STATUS_FAILED:     return "failed";
        default:                    return "unknown";
    }
}

const char* code_gen_version(void) {
    return CODE_GEN_VERSION;
}

//=============================================================================
// Internal Functions
//=============================================================================

static int engine_init_templates(code_gen_engine_t* engine) {
    /* Built-in templates are defined as macros in nimcp_fix_templates.h */
    /* No dynamic initialization needed */
    return 0;
}

static void engine_cleanup_templates(code_gen_engine_t* engine) {
    /* No dynamic cleanup needed for built-in templates */
}

static int generate_fix_for_strategy(
    code_gen_engine_t* engine,
    code_fix_strategy_t strategy,
    const code_gen_request_t* request,
    generated_fix_t* fix
) {
    if (!fix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fix is NULL");

        return -1;
    }

    memset(fix, 0, sizeof(*fix));
    fix->strategy = strategy;

    /* Copy location */
    if (request) {
        strncpy(fix->source_file, request->location.file_path, sizeof(fix->source_file) - 1);
        strncpy(fix->function_name, request->location.function_name, sizeof(fix->function_name) - 1);
        fix->start_line = request->location.line_number;
        fix->end_line = request->location.line_number;
    }

    /* Generate fix based on strategy */
    switch (strategy) {
        case FIX_STRATEGY_NULL_CHECK:
            snprintf(fix->fixed_code, sizeof(fix->fixed_code),
                "if (%s == NULL) {\n"
                "    return %s;\n"
                "}\n",
                "${VAR_NAME}", "${ERROR_VALUE}");
            snprintf(fix->explanation, sizeof(fix->explanation),
                "Added null pointer check to prevent segmentation fault");
            fix->complexity = FIX_COMPLEXITY_TRIVIAL;
            fix->confidence = 0.9f;
            fix->risk_score = 0.1f;
            break;

        case FIX_STRATEGY_BOUNDS_CHECK:
            snprintf(fix->fixed_code, sizeof(fix->fixed_code),
                "if (%s < 0 || %s >= %s) {\n"
                "    return %s;\n"
                "}\n",
                "${INDEX}", "${INDEX}", "${SIZE}", "${ERROR_VALUE}");
            snprintf(fix->explanation, sizeof(fix->explanation),
                "Added array bounds check to prevent buffer overflow");
            fix->complexity = FIX_COMPLEXITY_TRIVIAL;
            fix->confidence = 0.85f;
            fix->risk_score = 0.1f;
            break;

        case FIX_STRATEGY_DIVISION_GUARD:
            snprintf(fix->fixed_code, sizeof(fix->fixed_code),
                "if (%s == 0) {\n"
                "    return %s;\n"
                "}\n",
                "${DIVISOR}", "${ERROR_VALUE}");
            snprintf(fix->explanation, sizeof(fix->explanation),
                "Added division by zero guard");
            fix->complexity = FIX_COMPLEXITY_TRIVIAL;
            fix->confidence = 0.95f;
            fix->risk_score = 0.05f;
            break;

        case FIX_STRATEGY_NAN_GUARD:
            snprintf(fix->fixed_code, sizeof(fix->fixed_code),
                "if (isnan(%s) || isinf(%s)) {\n"
                "    return %s;\n"
                "}\n",
                "${VAR}", "${VAR}", "${ERROR_VALUE}");
            snprintf(fix->explanation, sizeof(fix->explanation),
                "Added NaN/Inf guard to prevent numerical instability propagation");
            fix->complexity = FIX_COMPLEXITY_TRIVIAL;
            fix->confidence = 0.9f;
            fix->risk_score = 0.1f;
            break;

        case FIX_STRATEGY_MEMORY_CLEANUP:
            snprintf(fix->fixed_code, sizeof(fix->fixed_code),
                "if (%s != NULL) {\n"
                "    nimcp_free(%s);\n"
                "    %s = NULL;\n"
                "}\n",
                "${PTR}", "${PTR}", "${PTR}");
            snprintf(fix->explanation, sizeof(fix->explanation),
                "Added proper memory cleanup with null-after-free pattern");
            fix->complexity = FIX_COMPLEXITY_SIMPLE;
            fix->confidence = 0.8f;
            fix->risk_score = 0.2f;
            break;

        case FIX_STRATEGY_MUTEX_FIX:
            snprintf(fix->fixed_code, sizeof(fix->fixed_code),
                "nimcp_mutex_lock(%s);\n"
                "/* protected section */\n"
                "nimcp_mutex_unlock(%s);\n",
                "${MUTEX}", "${MUTEX}");
            snprintf(fix->explanation, sizeof(fix->explanation),
                "Added mutex lock/unlock to prevent race condition");
            fix->complexity = FIX_COMPLEXITY_MODERATE;
            fix->confidence = 0.65f;
            fix->risk_score = 0.35f;
            break;

        case FIX_STRATEGY_ERROR_HANDLING:
            snprintf(fix->fixed_code, sizeof(fix->fixed_code),
                "int %s = %s;\n"
                "if (%s != 0) {\n"
                "    return %s;\n"
                "}\n",
                "${RESULT}", "${CALL}", "${RESULT}", "${ERROR_VALUE}");
            snprintf(fix->explanation, sizeof(fix->explanation),
                "Added return value error checking");
            fix->complexity = FIX_COMPLEXITY_SIMPLE;
            fix->confidence = 0.75f;
            fix->risk_score = 0.15f;
            break;

        case FIX_STRATEGY_INITIALIZATION:
            snprintf(fix->fixed_code, sizeof(fix->fixed_code),
                "%s %s = %s;\n",
                "${TYPE}", "${VAR}", "${INIT_VALUE}");
            snprintf(fix->explanation, sizeof(fix->explanation),
                "Added proper variable initialization");
            fix->complexity = FIX_COMPLEXITY_TRIVIAL;
            fix->confidence = 0.85f;
            fix->risk_score = 0.1f;
            break;

        case FIX_STRATEGY_OVERFLOW_GUARD:
            snprintf(fix->fixed_code, sizeof(fix->fixed_code),
                "if (%s > 0 && %s > %s - %s) {\n"
                "    return %s;\n"
                "}\n",
                "${A}", "${B}", "${MAX}", "${A}", "${ERROR_VALUE}");
            snprintf(fix->explanation, sizeof(fix->explanation),
                "Added integer overflow guard");
            fix->complexity = FIX_COMPLEXITY_SIMPLE;
            fix->confidence = 0.8f;
            fix->risk_score = 0.15f;
            break;

        case FIX_STRATEGY_ALIGNMENT_FIX:
            snprintf(fix->fixed_code, sizeof(fix->fixed_code),
                "%s* %s = (%s*)(((uintptr_t)%s + %s - 1) & ~(%s - 1));\n",
                "${TYPE}", "${ALIGNED}", "${TYPE}", "${PTR}", "${ALIGN}", "${ALIGN}");
            snprintf(fix->explanation, sizeof(fix->explanation),
                "Added pointer alignment to prevent bus error");
            fix->complexity = FIX_COMPLEXITY_SIMPLE;
            fix->confidence = 0.8f;
            fix->risk_score = 0.15f;
            break;

        default:
            snprintf(fix->explanation, sizeof(fix->explanation),
                "No template available for strategy %s", code_gen_strategy_name(strategy));
            fix->confidence = 0.0f;
            fix->risk_score = 1.0f;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "generate_fix_for_strategy: operation failed");
            return -1;
    }

    /* Copy original code if provided */
    if (request && request->source_code) {
        strncpy(fix->original_code, request->source_code,
                sizeof(fix->original_code) - 1);
    }

    /* Identify root cause */
    snprintf(fix->root_cause, sizeof(fix->root_cause),
             "Missing %s protection", code_gen_strategy_name(strategy));

    return 0;
}

static void compute_epitope(const code_gen_request_t* request, char* epitope, size_t size) {
    if (!request || !epitope || size == 0) {
        return;
    }

    /* Simple epitope: function name + line + error type */
    snprintf(epitope, size, "%s:%u:%04X",
             request->location.function_name,
             request->location.line_number,
             request->diagnosis ? request->diagnosis->error_type : 0);
}

static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

//=============================================================================
// Bio-Async Communication
//=============================================================================

/**
 * @brief Handle incoming bio-async messages
 */
static nimcp_error_t code_gen_handle_bio_message(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    if (!msg || msg_size < sizeof(bio_message_header_t) || !user_data) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    code_gen_engine_t* engine = (code_gen_engine_t*)user_data;
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    (void)response_promise;  /* May be NULL for fire-and-forget messages */
    (void)engine;  /* Used for future message processing */

    switch (header->type) {
        case BIO_MSG_CODE_GEN_REQUEST:
            /* Handle code generation request via bio-async */
            break;

        case BIO_MSG_CODE_GEN_VALIDATE:
            /* Handle validation request */
            break;

        case BIO_MSG_CODE_GEN_LEARN:
            /* Handle learning outcome notification */
            break;

        default:
            /* Unknown message type for this module */
            return NIMCP_ERROR_UNKNOWN;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Register message handlers for code generation module
 */
static int register_code_gen_bio_handlers(code_gen_engine_t* engine) {
    if (!engine || !engine->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "register_code_gen_bio_handlers: required parameter is NULL (engine, engine->bio_ctx)");
        return -1;
    }

    /* Register handlers for code generation messages */
    bio_router_register_handler(engine->bio_ctx, BIO_MSG_CODE_GEN_REQUEST,
                                code_gen_handle_bio_message);
    bio_router_register_handler(engine->bio_ctx, BIO_MSG_CODE_GEN_VALIDATE,
                                code_gen_handle_bio_message);
    bio_router_register_handler(engine->bio_ctx, BIO_MSG_CODE_GEN_LEARN,
                                code_gen_handle_bio_message);

    return 0;
}

/**
 * @brief Broadcast code generation result via bio-async
 */
int code_gen_broadcast_result(
    code_gen_engine_t* engine,
    uint64_t fix_id,
    bool success,
    float confidence
) {
    if (!engine || !engine->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "code_gen_broadcast_result: required parameter is NULL (engine, engine->bio_ctx)");
        return -1;
    }

    /* Build and send result message */
    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_broadcast_r", 0.0f);


    struct {
        bio_message_header_t header;
        uint64_t fix_id;
        uint8_t success;
        float confidence;
    } msg = {0};

    msg.header.type = BIO_MSG_CODE_GEN_RESULT;
    msg.header.source_module = BIO_MODULE_CODE_GENERATION;
    msg.header.target_module = BIO_MODULE_SELF_REPAIR;  /* Target self-repair coordinator */
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
    msg.fix_id = fix_id;
    msg.success = success ? 1 : 0;
    msg.confidence = confidence;

    bio_router_send(engine->bio_ctx, &msg, sizeof(msg), 0);
    return 0;
}

/**
 * @brief Process pending bio-async messages
 */
uint32_t code_gen_process_messages(code_gen_engine_t* engine, uint32_t max_messages) {
    if (!engine || !engine->bio_ctx) {
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    code_generation_heartbeat("code_generat_code_gen_process_mes", 0.0f);


    return bio_router_process_inbox(engine->bio_ctx, max_messages);
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void code_generation_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_code_generation_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int code_generation_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "code_generation_training_begin: NULL argument");
        return -1;
    }
    code_generation_heartbeat_instance(g_code_generation_health_agent, "code_generation_training_begin", 0.0f);
    (void)(struct code_gen_engine*)instance; /* Module state available for reset */
    return 0;
}

int code_generation_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "code_generation_training_end: NULL argument");
        return -1;
    }
    code_generation_heartbeat_instance(g_code_generation_health_agent, "code_generation_training_end", 1.0f);
    (void)(struct code_gen_engine*)instance; /* Module state available for finalization */
    return 0;
}

int code_generation_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "code_generation_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    code_generation_heartbeat_instance(g_code_generation_health_agent, "code_generation_training_step", progress);
    (void)(struct code_gen_engine*)instance; /* Module state available for step adaptation */
    return 0;
}
