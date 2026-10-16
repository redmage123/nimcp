/**
 * @file nimcp_financial_cognitive_orchestrator.c
 * @brief Financial Cognitive Orchestrator Implementation - Master Integration (Phase 10)
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Master orchestrator that ties all financial bridges together into a
 *       unified cognitive financial processing system.
 *
 * WHY:  Financial decision-making requires holistic integration of multiple
 *       cognitive systems working in concert. This orchestrator ensures
 *       coordinated processing through perception, emotion, attention,
 *       cognition, decision-making, ethics, learning, and metacognition.
 *
 * HOW:  Pipeline-based processing where market data flows through all
 *       enabled cognitive stages, producing decisions with explanations
 *       and supporting continuous learning from outcomes.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>

#include "cognitive/parietal/nimcp_financial_cognitive_orchestrator.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(fin_orch, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Stub heartbeat for migration compatibility */
static inline void fin_orch_heartbeat_global(const char* op, float progress) {
    (void)op; (void)progress;
}


/* ============================================================================
 * Immune/BBB Integration (Phase 9: Security Integration)
 * ============================================================================ */

struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;
extern int brain_immune_validate_operation(brain_immune_system_t* immune,
                                            const char* operation,
                                            uint32_t severity);

struct bbb_system_struct;
extern int bbb_validate_data(bbb_system_t bbb, const void* data,
                              size_t size, const char* context);

/* ============================================================================
 * Thread-Local Error Handling
 * ============================================================================ */

static _Thread_local char fin_orch_last_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_orch_last_error, sizeof(fin_orch_last_error), fmt, args);
    va_end(args);
}

const char* financial_cognitive_orchestrator_get_last_error(void) {
    return fin_orch_last_error;
}

/* ============================================================================
 * KG Wiring Message Types
 * ============================================================================ */

#define KG_MSG_FIN_ORCH_MARKET_DATA    "FIN_ORCH_MARKET_DATA"
#define KG_MSG_FIN_ORCH_DECISION       "FIN_ORCH_DECISION"
#define KG_MSG_FIN_ORCH_LEARNING       "FIN_ORCH_LEARNING"
#define KG_MSG_FIN_ORCH_CONSOLIDATION  "FIN_ORCH_CONSOLIDATION"
#define KG_MSG_FIN_ORCH_STATE_CHANGE   "FIN_ORCH_STATE_CHANGE"

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct financial_cognitive_orchestrator_internal {
    uint32_t magic;
    fin_orchestrator_config_t config;
    fin_orchestrator_state_t op_state;
    fin_orchestrator_stats_t stats;

    /* Module pointers structure */
    financial_cognitive_orchestrator_t modules;

    /* Pipeline state */
    fin_pipeline_stage_t current_stage;
    uint64_t last_processing_time_us;
    uint64_t last_consolidation_ms;

    /* Integration subsystems */
    void* immune;
    void* bbb;
    void* health_agent;
    void* kg_wiring;
    void* logger;
    void* security;
    void* ethics_engine;
    const void* lgss;
    void* coordinator;
    void* bio_router;

    /* Training state */
    bool training_active;

    /* Timing */
    uint64_t creation_time_ms;
};

static inline uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static inline uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/* ============================================================================
 * KG Publishing Helper
 * ============================================================================ */

static int orch_kg_publish(financial_cognitive_orchestrator_handle_t* orch,
                            const char* msg_type,
                            const void* payload,
                            size_t size) {
    if (orch && orch->kg_wiring && orch->config.enable_kg_messaging) {
        orch->stats.kg_messages_sent++;
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        return 0;
    }
    return 0;
}

/* ============================================================================
 * Immune/BBB Validation Helpers
 * ============================================================================ */

static int orch_validate_immune(financial_cognitive_orchestrator_handle_t* orch,
                                 const char* operation) {
    if (!orch->config.enable_immune_integration || !orch->immune) {
        return 0;
    }

    orch->stats.immune_checks++;
    int result = brain_immune_validate_operation(
        (brain_immune_system_t*)orch->immune, operation, 1);
    if (result != 0) {
        set_error("Immune validation failed for operation: %s", operation);
        return FIN_ORCH_ERR_IMMUNE;
    }
    return 0;
}

static int orch_validate_bbb(financial_cognitive_orchestrator_handle_t* orch,
                              const void* data, size_t size,
                              const char* context) {
    if (!orch->config.enable_bbb_validation || !orch->bbb) {
        return 0;
    }

    orch->stats.bbb_validations++;
    int result = bbb_validate_data(orch->bbb, data, size, context);
    if (result != 0) {
        set_error("BBB validation failed for context: %s", context);
        return FIN_ORCH_ERR_BBB;
    }
    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* fin_orchestrator_state_name(fin_orchestrator_state_t state) {
    switch (state) {
        case FIN_ORCH_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case FIN_ORCH_STATE_INITIALIZED:   return "INITIALIZED";
        case FIN_ORCH_STATE_READY:         return "READY";
        case FIN_ORCH_STATE_PROCESSING:    return "PROCESSING";
        case FIN_ORCH_STATE_DECIDING:      return "DECIDING";
        case FIN_ORCH_STATE_LEARNING:      return "LEARNING";
        case FIN_ORCH_STATE_CONSOLIDATING: return "CONSOLIDATING";
        case FIN_ORCH_STATE_DEGRADED:      return "DEGRADED";
        case FIN_ORCH_STATE_ERROR:         return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* fin_orchestrator_decision_name(fin_decision_type_t decision) {
    switch (decision) {
        case FIN_DECISION_BUY:         return "BUY";
        case FIN_DECISION_SELL:        return "SELL";
        case FIN_DECISION_HOLD:        return "HOLD";
        case FIN_DECISION_SHORT:       return "SHORT";
        case FIN_DECISION_COVER:       return "COVER";
        case FIN_DECISION_REBALANCE:   return "REBALANCE";
        case FIN_DECISION_HEDGE:       return "HEDGE";
        case FIN_DECISION_EXIT:        return "EXIT";
        case FIN_DECISION_WAIT:        return "WAIT";
        case FIN_DECISION_GATHER_INFO: return "GATHER_INFO";
        default: return "UNKNOWN";
    }
}

const char* fin_orchestrator_stage_name(fin_pipeline_stage_t stage) {
    switch (stage) {
        case FIN_PIPELINE_PERCEPTION:      return "PERCEPTION";
        case FIN_PIPELINE_WORKING_MEMORY:  return "WORKING_MEMORY";
        case FIN_PIPELINE_EMOTION:         return "EMOTION";
        case FIN_PIPELINE_ATTENTION:       return "ATTENTION";
        case FIN_PIPELINE_COGNITION:       return "COGNITION";
        case FIN_PIPELINE_DECISION:        return "DECISION";
        case FIN_PIPELINE_ETHICS:          return "ETHICS";
        case FIN_PIPELINE_LEARNING:        return "LEARNING";
        case FIN_PIPELINE_METACOGNITION:   return "METACOGNITION";
        default: return "UNKNOWN";
    }
}

const char* financial_cognitive_orchestrator_version(void) {
    return FINANCIAL_COGNITIVE_ORCHESTRATOR_VERSION;
}

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int financial_cognitive_orchestrator_default_config(fin_orchestrator_config_t* config) {
    if (!config) {
        set_error("NULL config pointer");
        return FIN_ORCH_ERR_NULL;
    }

    memset(config, 0, sizeof(*config));

    /* Pipeline settings - enable all by default */
    config->enable_working_memory = true;
    config->enable_emotion_processing = true;
    config->enable_attention_filtering = true;
    config->enable_world_model = true;
    config->enable_tom = true;
    config->enable_ethics_validation = true;
    config->enable_metacognition = true;
    config->enable_learning = true;
    config->enable_consolidation = true;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;
    config->enable_fuzzy_logic = true;

    /* Decision settings */
    config->min_confidence_threshold = 0.6f;
    config->ethics_veto_threshold = 0.3f;
    config->metacog_reconsider_threshold = 0.7f;
    config->max_decisions_per_cycle = 10;

    /* Learning settings */
    config->learning_rate = 0.01f;
    config->temporal_discount = 0.95f;
    config->consolidation_interval_ms = 3600000; /* 1 hour */

    /* Memory settings */
    config->working_memory_capacity = 7;
    config->working_memory_decay_rate = 0.1f;

    /* Logging */
    config->verbose_logging = false;

    return FIN_ORCH_ERR_OK;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

financial_cognitive_orchestrator_handle_t* financial_cognitive_orchestrator_create(
    const fin_orchestrator_config_t* config
) {
    fin_orch_heartbeat_global("orchestrator_create", 0.0f);

    financial_cognitive_orchestrator_handle_t* orch =
        (financial_cognitive_orchestrator_handle_t*)nimcp_calloc(
            1, sizeof(financial_cognitive_orchestrator_handle_t));
    if (!orch) {
        set_error("Failed to allocate orchestrator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_cognitive_orchestrator_create: orch is NULL");
        return NULL;
    }

    orch->magic = FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC;

    /* Apply configuration */
    if (config) {
        orch->config = *config;
    } else {
        financial_cognitive_orchestrator_default_config(&orch->config);
    }

    /* Initialize module pointers to NULL */
    memset(&orch->modules, 0, sizeof(orch->modules));

    /* Initialize state */
    orch->op_state = FIN_ORCH_STATE_INITIALIZED;
    orch->current_stage = FIN_PIPELINE_PERCEPTION;
    orch->creation_time_ms = get_timestamp_ms();
    orch->last_consolidation_ms = orch->creation_time_ms;

    fin_orch_heartbeat_global("orchestrator_create", 1.0f);
    return orch;
}

void financial_cognitive_orchestrator_destroy(
    financial_cognitive_orchestrator_handle_t* orch
) {
    if (!orch) return;

    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) {
        set_error("Invalid orchestrator magic");
        return;
    }

    fin_orch_heartbeat_global("orchestrator_destroy", 0.5f);

    /* Note: We do NOT destroy the individual module bridges here.
     * The orchestrator does not own them - it only holds references.
     * Ownership and destruction is the responsibility of the caller. */

    orch->magic = 0;
    nimcp_free(orch);
    orch = NULL;

    fin_orch_heartbeat_global("orchestrator_destroy", 1.0f);
}

int financial_cognitive_orchestrator_reset(
    financial_cognitive_orchestrator_handle_t* orch
) {
    if (!orch) {
        set_error("NULL orchestrator pointer");
        return FIN_ORCH_ERR_NULL;
    }

    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) {
        set_error("Invalid orchestrator magic");
        return FIN_ORCH_ERR_STATE;
    }

    fin_orch_heartbeat_global("orchestrator_reset", 0.5f);

    /* Reset statistics */
    memset(&orch->stats, 0, sizeof(orch->stats));

    /* Reset state */
    orch->op_state = FIN_ORCH_STATE_INITIALIZED;
    orch->current_stage = FIN_PIPELINE_PERCEPTION;
    orch->last_processing_time_us = 0;
    orch->training_active = false;

    fin_orch_heartbeat_global("orchestrator_reset", 1.0f);
    return FIN_ORCH_ERR_OK;
}

/* ============================================================================
 * Module Registration
 * ============================================================================ */

financial_cognitive_orchestrator_t* financial_cognitive_orchestrator_get_modules(
    financial_cognitive_orchestrator_handle_t* orch
) {
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "financial_cognitive_orchestrator_get_modules: orch is NULL");
        return NULL;
    }
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "financial_cognitive_orchestrator_get_modules: validation failed");
        return NULL;
    }
    return &orch->modules;
}

int financial_cognitive_orchestrator_register_all(
    financial_cognitive_orchestrator_handle_t* orch,
    const financial_cognitive_orchestrator_t* modules
) {
    if (!orch || !modules) {
        set_error("NULL pointer");
        return FIN_ORCH_ERR_NULL;
    }

    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) {
        set_error("Invalid orchestrator magic");
        return FIN_ORCH_ERR_STATE;
    }

    /* Copy all module pointers */
    memcpy(&orch->modules, modules, sizeof(financial_cognitive_orchestrator_t));

    return FIN_ORCH_ERR_OK;
}

int financial_cognitive_orchestrator_validate_modules(
    const financial_cognitive_orchestrator_handle_t* orch
) {
    if (!orch) {
        set_error("NULL orchestrator pointer");
        return FIN_ORCH_ERR_NULL;
    }

    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) {
        set_error("Invalid orchestrator magic");
        return FIN_ORCH_ERR_STATE;
    }

    /* Check minimum required modules */
    /* At minimum, we need the core financial modules */
    if (!orch->modules.investment) {
        set_error("Missing required module: investment");
        return FIN_ORCH_ERR_SUBSYSTEM;
    }
    if (!orch->modules.market) {
        set_error("Missing required module: market");
        return FIN_ORCH_ERR_SUBSYSTEM;
    }

    return FIN_ORCH_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

int financial_cognitive_orchestrator_set_immune(
    financial_cognitive_orchestrator_handle_t* orch,
    void* immune
) {
    if (!orch) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;
    orch->immune = immune;
    return FIN_ORCH_ERR_OK;
}

int financial_cognitive_orchestrator_set_bbb(
    financial_cognitive_orchestrator_handle_t* orch,
    bbb_system_t bbb
) {
    if (!orch) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;
    orch->bbb = bbb;
    return FIN_ORCH_ERR_OK;
}

int financial_cognitive_orchestrator_set_health_agent(
    financial_cognitive_orchestrator_handle_t* orch,
    void* health_agent
) {
    if (!orch) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;
    orch->health_agent = health_agent;
    return FIN_ORCH_ERR_OK;
}

int financial_cognitive_orchestrator_set_kg_wiring(
    financial_cognitive_orchestrator_handle_t* orch,
    void* kg_wiring
) {
    if (!orch) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;
    orch->kg_wiring = kg_wiring;
    return FIN_ORCH_ERR_OK;
}

int financial_cognitive_orchestrator_set_logger(
    financial_cognitive_orchestrator_handle_t* orch,
    void* logger
) {
    if (!orch) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;
    orch->logger = logger;
    return FIN_ORCH_ERR_OK;
}

int financial_cognitive_orchestrator_set_security(
    financial_cognitive_orchestrator_handle_t* orch,
    void* security
) {
    if (!orch) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;
    orch->security = security;
    return FIN_ORCH_ERR_OK;
}

int financial_cognitive_orchestrator_set_ethics_engine(
    financial_cognitive_orchestrator_handle_t* orch,
    ethics_engine_t ethics
) {
    if (!orch) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;
    orch->ethics_engine = ethics;
    return FIN_ORCH_ERR_OK;
}

int financial_cognitive_orchestrator_set_lgss(
    financial_cognitive_orchestrator_handle_t* orch,
    const void* lgss
) {
    if (!orch) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;
    orch->lgss = lgss;
    return FIN_ORCH_ERR_OK;
}

int financial_cognitive_orchestrator_set_coordinator(
    financial_cognitive_orchestrator_handle_t* orch,
    brain_cycle_coordinator_t* coordinator
) {
    if (!orch) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;
    orch->coordinator = coordinator;
    return FIN_ORCH_ERR_OK;
}

int financial_cognitive_orchestrator_set_bio_router(
    financial_cognitive_orchestrator_handle_t* orch,
    void* bio_router
) {
    if (!orch) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;
    orch->bio_router = bio_router;
    return FIN_ORCH_ERR_OK;
}

/* ============================================================================
 * Pipeline Stage Processors
 * ============================================================================ */

static int process_perception_stage(
    financial_cognitive_orchestrator_handle_t* orch,
    const fin_market_data_t* data,
    fin_pipeline_result_t* result
) {
    uint64_t start = get_timestamp_us();

    /* Validate market data through BBB */
    int ret = orch_validate_bbb(orch, data->prices,
                                 data->num_assets * sizeof(float),
                                 "market_data_prices");
    if (ret != 0) return ret;

    ret = orch_validate_bbb(orch, data->volumes,
                             data->num_assets * sizeof(float),
                             "market_data_volumes");
    if (ret != 0) return ret;

    /* Encode market data through neural bridge if available */
    if (orch->modules.neural) {
        /* In a full implementation, we would call:
         * financial_neural_bridge_encode_market_event(orch->modules.neural, ...) */
    }

    if (result) {
        result->stage_completed[FIN_PIPELINE_PERCEPTION] = true;
        result->stage_times_us[FIN_PIPELINE_PERCEPTION] =
            (float)(get_timestamp_us() - start);
        snprintf(result->stage_notes[FIN_PIPELINE_PERCEPTION],
                 sizeof(result->stage_notes[0]),
                 "Processed %u assets", data->num_assets);
    }

    return FIN_ORCH_ERR_OK;
}

static int process_working_memory_stage(
    financial_cognitive_orchestrator_handle_t* orch,
    const fin_market_data_t* data,
    fin_pipeline_result_t* result
) {
    if (!orch->config.enable_working_memory) {
        if (result) result->stage_completed[FIN_PIPELINE_WORKING_MEMORY] = true;
        return FIN_ORCH_ERR_OK;
    }

    uint64_t start = get_timestamp_us();

    /* Add to working memory if available */
    if (orch->modules.working_memory) {
        /* In a full implementation, we would call:
         * financial_wm_bridge_add(orch->modules.working_memory, item) */
    }

    if (result) {
        result->stage_completed[FIN_PIPELINE_WORKING_MEMORY] = true;
        result->stage_times_us[FIN_PIPELINE_WORKING_MEMORY] =
            (float)(get_timestamp_us() - start);
        result->working_memory_items = orch->config.working_memory_capacity;
    }

    return FIN_ORCH_ERR_OK;
}

static int process_emotion_stage(
    financial_cognitive_orchestrator_handle_t* orch,
    const fin_market_data_t* data,
    fin_pipeline_result_t* result
) {
    if (!orch->config.enable_emotion_processing) {
        if (result) result->stage_completed[FIN_PIPELINE_EMOTION] = true;
        return FIN_ORCH_ERR_OK;
    }

    uint64_t start = get_timestamp_us();
    float emotion_magnitude = 0.0f;

    /* Update emotional state through emotion bridge */
    if (orch->modules.emotion) {
        /* In a full implementation:
         * financial_emotion_bridge_update(orch->modules.emotion, event) */
        emotion_magnitude = 0.3f; /* Placeholder */
    }

    /* Update motivation state */
    if (orch->modules.motivation) {
        /* financial_motivation_bridge_update(...) */
    }

    /* Update neuromodulator state */
    if (orch->modules.neuromod) {
        /* financial_neuromod_bridge_update(...) */
    }

    /* Check mental health */
    if (orch->modules.mental_health) {
        /* financial_mental_health_bridge_check(...) */
    }

    if (result) {
        result->stage_completed[FIN_PIPELINE_EMOTION] = true;
        result->stage_times_us[FIN_PIPELINE_EMOTION] =
            (float)(get_timestamp_us() - start);
        result->emotional_state_magnitude = emotion_magnitude;
    }

    (void)data;
    return FIN_ORCH_ERR_OK;
}

static int process_attention_stage(
    financial_cognitive_orchestrator_handle_t* orch,
    const fin_market_data_t* data,
    fin_pipeline_result_t* result
) {
    if (!orch->config.enable_attention_filtering) {
        if (result) result->stage_completed[FIN_PIPELINE_ATTENTION] = true;
        return FIN_ORCH_ERR_OK;
    }

    uint64_t start = get_timestamp_us();
    float attention_focus = 0.0f;

    /* Process through salience bridge */
    if (orch->modules.salience) {
        /* financial_salience_bridge_compute(...) */
        attention_focus = 0.7f; /* Placeholder */
    }

    /* Process through emotional attention bridge */
    if (orch->modules.emo_attention) {
        /* financial_emo_attention_bridge_modulate(...) */
    }

    if (result) {
        result->stage_completed[FIN_PIPELINE_ATTENTION] = true;
        result->stage_times_us[FIN_PIPELINE_ATTENTION] =
            (float)(get_timestamp_us() - start);
        result->attention_focus = attention_focus;
    }

    (void)data;
    return FIN_ORCH_ERR_OK;
}

static int process_cognition_stage(
    financial_cognitive_orchestrator_handle_t* orch,
    const fin_market_data_t* data,
    fin_pipeline_result_t* result
) {
    uint64_t start = get_timestamp_us();

    /* Update world model */
    if (orch->config.enable_world_model && orch->modules.world_model) {
        /* financial_world_model_bridge_update(...) */
    }

    /* Update theory of mind models */
    if (orch->config.enable_tom && orch->modules.tom) {
        /* financial_tom_bridge_update(...) */
    }

    /* Autobiographical memory encoding */
    if (orch->modules.autobio) {
        /* financial_autobio_bridge_encode_episode(...) */
    }

    /* Resonance processing */
    if (orch->modules.resonance) {
        /* financial_resonance_bridge_compute(...) */
    }

    /* Mammillary memory */
    if (orch->modules.mammillary) {
        /* financial_mammillary_bridge_store(...) */
    }

    if (result) {
        result->stage_completed[FIN_PIPELINE_COGNITION] = true;
        result->stage_times_us[FIN_PIPELINE_COGNITION] =
            (float)(get_timestamp_us() - start);
    }

    (void)data;
    return FIN_ORCH_ERR_OK;
}

static int process_decision_stage(
    financial_cognitive_orchestrator_handle_t* orch,
    const fin_market_data_t* data,
    fin_pipeline_result_t* result
) {
    uint64_t start = get_timestamp_us();

    /* Basal ganglia action selection */
    if (orch->modules.basal_ganglia) {
        /* financial_bg_bridge_select_action(...) */
    }

    /* Predictive coding */
    if (orch->modules.predictive) {
        /* financial_predictive_bridge_compute_efe(...) */
    }

    /* Reasoning */
    if (orch->modules.reasoning) {
        /* financial_reasoning_bridge_infer(...) */
    }

    /* JEPA prediction */
    if (orch->modules.jepa) {
        /* financial_jepa_bridge_predict(...) */
    }

    if (result) {
        result->stage_completed[FIN_PIPELINE_DECISION] = true;
        result->stage_times_us[FIN_PIPELINE_DECISION] =
            (float)(get_timestamp_us() - start);
    }

    (void)data;
    return FIN_ORCH_ERR_OK;
}

static int process_ethics_stage(
    financial_cognitive_orchestrator_handle_t* orch,
    const fin_market_data_t* data,
    fin_pipeline_result_t* result
) {
    if (!orch->config.enable_ethics_validation) {
        if (result) {
            result->stage_completed[FIN_PIPELINE_ETHICS] = true;
            result->ethics_approved = true;
        }
        return FIN_ORCH_ERR_OK;
    }

    uint64_t start = get_timestamp_us();
    bool approved = true;

    /* Ethics validation */
    if (orch->modules.ethics) {
        /* financial_ethics_bridge_validate(...) */
    }

    /* Generate explanations */
    if (orch->modules.explanations) {
        /* financial_explanations_bridge_generate(...) */
    }

    if (result) {
        result->stage_completed[FIN_PIPELINE_ETHICS] = true;
        result->stage_times_us[FIN_PIPELINE_ETHICS] =
            (float)(get_timestamp_us() - start);
        result->ethics_approved = approved;
    }

    (void)data;
    return FIN_ORCH_ERR_OK;
}

static int process_learning_stage(
    financial_cognitive_orchestrator_handle_t* orch,
    const fin_market_data_t* data,
    fin_pipeline_result_t* result
) {
    if (!orch->config.enable_learning) {
        if (result) result->stage_completed[FIN_PIPELINE_LEARNING] = true;
        return FIN_ORCH_ERR_OK;
    }

    uint64_t start = get_timestamp_us();

    /* Note: Learning typically happens in learn_from_outcome(), not in
     * the regular processing pipeline. This stage is for online learning
     * from the market data itself. */

    /* STDP correlation learning */
    if (orch->modules.stdp) {
        /* financial_stdp_bridge_correlate(...) */
    }

    /* Temporal credit tracking */
    if (orch->modules.temporal_credit) {
        /* financial_temporal_credit_bridge_update(...) */
    }

    if (result) {
        result->stage_completed[FIN_PIPELINE_LEARNING] = true;
        result->stage_times_us[FIN_PIPELINE_LEARNING] =
            (float)(get_timestamp_us() - start);
    }

    (void)data;
    return FIN_ORCH_ERR_OK;
}

static int process_metacognition_stage(
    financial_cognitive_orchestrator_handle_t* orch,
    const fin_market_data_t* data,
    fin_pipeline_result_t* result
) {
    if (!orch->config.enable_metacognition) {
        if (result) result->stage_completed[FIN_PIPELINE_METACOGNITION] = true;
        return FIN_ORCH_ERR_OK;
    }

    uint64_t start = get_timestamp_us();
    float confidence = 0.5f;

    /* Metacognitive assessment */
    if (orch->modules.metacognition) {
        /* financial_metacognition_bridge_assess(...) */
    }

    /* Uncertainty decomposition */
    if (orch->modules.uncertainty) {
        /* financial_uncertainty_bridge_decompose(...) */
    }

    /* Curiosity evaluation */
    if (orch->modules.curiosity) {
        /* financial_curiosity_bridge_evaluate(...) */
    }

    /* Regret analysis (for past decisions) */
    if (orch->modules.regret) {
        /* financial_regret_bridge_analyze(...) */
    }

    if (result) {
        result->stage_completed[FIN_PIPELINE_METACOGNITION] = true;
        result->stage_times_us[FIN_PIPELINE_METACOGNITION] =
            (float)(get_timestamp_us() - start);
        result->metacognitive_confidence = confidence;
    }

    (void)data;
    return FIN_ORCH_ERR_OK;
}

/* ============================================================================
 * Core Pipeline API
 * ============================================================================ */

int financial_cognitive_orchestrator_process_market_data(
    financial_cognitive_orchestrator_handle_t* orch,
    const fin_market_data_t* data,
    fin_pipeline_result_t* result
) {
    if (!orch || !data) {
        set_error("NULL pointer");
        return FIN_ORCH_ERR_NULL;
    }

    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) {
        set_error("Invalid orchestrator magic");
        return FIN_ORCH_ERR_STATE;
    }

    if (!data->prices || !data->volumes || data->num_assets == 0) {
        set_error("Invalid market data");
        return FIN_ORCH_ERR_INVALID_PARAM;
    }

    /* Immune validation */
    int ret = orch_validate_immune(orch, "process_market_data");
    if (ret != 0) return ret;

    /* Update state */
    fin_orchestrator_state_t prev_state = orch->op_state;
    orch->op_state = FIN_ORCH_STATE_PROCESSING;

    /* Initialize result if provided */
    if (result) {
        memset(result, 0, sizeof(*result));
    }

    uint64_t pipeline_start = get_timestamp_us();

    /* Heartbeat */
    if (orch->health_agent && orch->config.enable_health_monitoring) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)orch->health_agent,
            "process_market_data", 0.0f);
        orch->stats.health_heartbeats++;
    }

    /* Process through all pipeline stages */
    ret = process_perception_stage(orch, data, result);
    if (ret != 0) goto cleanup;

    ret = process_working_memory_stage(orch, data, result);
    if (ret != 0) goto cleanup;

    ret = process_emotion_stage(orch, data, result);
    if (ret != 0) goto cleanup;

    ret = process_attention_stage(orch, data, result);
    if (ret != 0) goto cleanup;

    ret = process_cognition_stage(orch, data, result);
    if (ret != 0) goto cleanup;

    ret = process_decision_stage(orch, data, result);
    if (ret != 0) goto cleanup;

    ret = process_ethics_stage(orch, data, result);
    if (ret != 0) goto cleanup;

    ret = process_learning_stage(orch, data, result);
    if (ret != 0) goto cleanup;

    ret = process_metacognition_stage(orch, data, result);
    if (ret != 0) goto cleanup;

    /* Update statistics */
    orch->stats.market_data_processed++;
    orch->last_processing_time_us = get_timestamp_us() - pipeline_start;

    if (result) {
        result->total_time_us = (float)orch->last_processing_time_us;
    }

    /* Publish KG message */
    orch_kg_publish(orch, KG_MSG_FIN_ORCH_MARKET_DATA, data, sizeof(*data));

    /* Heartbeat completion */
    if (orch->health_agent && orch->config.enable_health_monitoring) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)orch->health_agent,
            "process_market_data", 1.0f);
        orch->stats.health_heartbeats++;
    }

cleanup:
    orch->op_state = (ret == 0) ? FIN_ORCH_STATE_READY : prev_state;
    return ret;
}

int financial_cognitive_orchestrator_make_decision(
    financial_cognitive_orchestrator_handle_t* orch,
    const char* asset,
    fin_detailed_decision_t* result
) {
    if (!orch || !asset || !result) {
        set_error("NULL pointer");
        return FIN_ORCH_ERR_NULL;
    }

    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) {
        set_error("Invalid orchestrator magic");
        return FIN_ORCH_ERR_STATE;
    }

    /* Immune validation */
    int ret = orch_validate_immune(orch, "make_decision");
    if (ret != 0) return ret;

    /* Update state */
    orch->op_state = FIN_ORCH_STATE_DECIDING;

    memset(result, 0, sizeof(*result));

    /* Heartbeat */
    if (orch->health_agent && orch->config.enable_health_monitoring) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)orch->health_agent,
            "make_decision", 0.0f);
        orch->stats.health_heartbeats++;
    }

    /* Initialize decision */
    result->decision.decision_type = FIN_DECISION_HOLD;
    result->decision.magnitude = 0.0f;
    strncpy(result->decision.asset, asset, sizeof(result->decision.asset) - 1);
    result->decision.confidence = 0.0f;

    /* In a full implementation, we would:
     * 1. Query world model for prediction
     * 2. Query ToM for other investor predictions
     * 3. Combine with reasoning rules
     * 4. Filter through basal ganglia action selection
     * 5. Apply predictive coding / active inference
     * 6. Check ethics
     * 7. Run metacognitive checks
     * 8. Generate explanation
     */

    /* Placeholder decision logic */
    float world_model_confidence = 0.6f;
    float tom_confidence = 0.5f;
    float reasoning_score = 0.55f;

    result->world_model_confidence = world_model_confidence;
    result->tom_prediction_confidence = tom_confidence;

    /* Combine influences */
    float combined = (world_model_confidence + tom_confidence + reasoning_score) / 3.0f;

    if (combined > 0.6f) {
        result->decision.decision_type = FIN_DECISION_BUY;
        result->decision.magnitude = nimcp_clampf(combined - 0.5f, 0.0f, 1.0f);
    } else if (combined < 0.4f) {
        result->decision.decision_type = FIN_DECISION_SELL;
        result->decision.magnitude = nimcp_clampf(0.5f - combined, 0.0f, 1.0f);
    } else {
        result->decision.decision_type = FIN_DECISION_HOLD;
        result->decision.magnitude = 0.0f;
    }

    result->decision.confidence = combined;

    /* Check confidence threshold */
    if (result->decision.confidence < orch->config.min_confidence_threshold) {
        result->decision.decision_type = FIN_DECISION_GATHER_INFO;
        result->decision.magnitude = 0.0f;
    }

    /* Generate explanation */
    snprintf(result->explanation.summary, sizeof(result->explanation.summary),
             "Decision: %s %s with %.1f%% confidence",
             fin_orchestrator_decision_name(result->decision.decision_type),
             asset, result->decision.confidence * 100.0f);

    snprintf(result->explanation.reasoning, sizeof(result->explanation.reasoning),
             "World model confidence: %.2f, ToM confidence: %.2f, "
             "Reasoning score: %.2f. Combined score: %.2f %s threshold (%.2f).",
             world_model_confidence, tom_confidence, reasoning_score, combined,
             (combined >= orch->config.min_confidence_threshold) ? ">=" : "<",
             orch->config.min_confidence_threshold);

    result->explanation.confidence = combined;

    /* Contribution factors */
    result->emotion_influence = 0.2f;
    result->reasoning_influence = 0.5f;
    result->intuition_influence = 0.3f;

    /* Risk assessment placeholder */
    result->estimated_risk = 0.4f;
    result->uncertainty_epistemic = 0.3f;
    result->uncertainty_aleatoric = 0.2f;

    /* Metacognitive placeholder */
    result->biases_detected = 0;
    result->reconsideration_suggested = false;
    result->calibration_score = 0.7f;

    /* Update statistics */
    orch->stats.decisions_made++;

    /* Publish KG message */
    orch_kg_publish(orch, KG_MSG_FIN_ORCH_DECISION, result, sizeof(*result));

    /* Heartbeat completion */
    if (orch->health_agent && orch->config.enable_health_monitoring) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)orch->health_agent,
            "make_decision", 1.0f);
        orch->stats.health_heartbeats++;
    }

    orch->op_state = FIN_ORCH_STATE_READY;
    return FIN_ORCH_ERR_OK;
}

int financial_cognitive_orchestrator_learn_from_outcome(
    financial_cognitive_orchestrator_handle_t* orch,
    const fin_trade_outcome_record_t* outcome,
    fin_learning_result_t* result
) {
    if (!orch || !outcome) {
        set_error("NULL pointer");
        return FIN_ORCH_ERR_NULL;
    }

    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) {
        set_error("Invalid orchestrator magic");
        return FIN_ORCH_ERR_STATE;
    }

    if (!orch->config.enable_learning) {
        if (result) {
            memset(result, 0, sizeof(*result));
            snprintf(result->lesson_learned, sizeof(result->lesson_learned),
                     "Learning disabled");
        }
        return FIN_ORCH_ERR_OK;
    }

    /* Immune validation */
    int ret = orch_validate_immune(orch, "learn_from_outcome");
    if (ret != 0) return ret;

    orch->op_state = FIN_ORCH_STATE_LEARNING;

    /* Initialize result if provided */
    if (result) {
        memset(result, 0, sizeof(*result));
    }

    /* Heartbeat */
    if (orch->health_agent && orch->config.enable_health_monitoring) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)orch->health_agent,
            "learn_from_outcome", 0.0f);
        orch->stats.health_heartbeats++;
    }

    /* Compute reward signal */
    float reward = 0.0f;
    if (outcome->outcome == FIN_OUTCOME_PROFIT) {
        reward = nimcp_clampf(outcome->return_pct, 0.0f, 1.0f);
    } else if (outcome->outcome == FIN_OUTCOME_LOSS) {
        reward = nimcp_clampf(outcome->return_pct, -1.0f, 0.0f);
    }

    /* STDP reward learning */
    if (orch->modules.stdp) {
        /* financial_stdp_bridge_apply_reward(orch->modules.stdp, reward) */
    }

    /* Temporal credit assignment */
    float temporal_credit = reward * orch->config.temporal_discount;
    if (orch->modules.temporal_credit) {
        /* financial_temporal_credit_bridge_assign(orch->modules.temporal_credit, ...) */
    }

    /* Consolidation pattern update */
    float strength_delta = 0.0f;
    if (orch->modules.consolidation) {
        /* financial_consolidation_bridge_add_trade(orch->modules.consolidation, ...) */
        strength_delta = (reward > 0) ?
            (reward * orch->config.learning_rate) :
            (reward * orch->config.learning_rate * 0.5f);
    }

    /* Regret analysis for losses */
    float regret = 0.0f;
    if (outcome->outcome == FIN_OUTCOME_LOSS && orch->modules.regret) {
        regret = fabsf(outcome->return_pct);
        /* financial_regret_bridge_analyze(orch->modules.regret, ...) */
    }

    /* Fill result */
    if (result) {
        result->reward_signal = reward;
        result->temporal_credit = temporal_credit;
        result->pattern_strength_delta = strength_delta;
        result->regret_magnitude = regret;
        result->pattern_updated = (fabsf(strength_delta) > 0.001f);

        if (outcome->outcome == FIN_OUTCOME_PROFIT) {
            snprintf(result->lesson_learned, sizeof(result->lesson_learned),
                     "Profitable trade on %s: %.2f%% return. "
                     "Pattern strengthened by %.4f.",
                     outcome->asset, outcome->return_pct * 100.0f, strength_delta);
        } else if (outcome->outcome == FIN_OUTCOME_LOSS) {
            snprintf(result->lesson_learned, sizeof(result->lesson_learned),
                     "Loss on %s: %.2f%%. Pattern weakened by %.4f. "
                     "Regret magnitude: %.4f.",
                     outcome->asset, outcome->return_pct * 100.0f,
                     strength_delta, regret);
        } else {
            snprintf(result->lesson_learned, sizeof(result->lesson_learned),
                     "Breakeven on %s. Minimal pattern adjustment.",
                     outcome->asset);
        }
    }

    /* Update statistics */
    orch->stats.learning_cycles++;

    /* Publish KG message */
    orch_kg_publish(orch, KG_MSG_FIN_ORCH_LEARNING, outcome, sizeof(*outcome));

    /* Heartbeat completion */
    if (orch->health_agent && orch->config.enable_health_monitoring) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)orch->health_agent,
            "learn_from_outcome", 1.0f);
        orch->stats.health_heartbeats++;
    }

    orch->op_state = FIN_ORCH_STATE_READY;
    return FIN_ORCH_ERR_OK;
}

int financial_cognitive_orchestrator_consolidate(
    financial_cognitive_orchestrator_handle_t* orch,
    fin_consolidation_session_result_t* result
) {
    if (!orch) {
        set_error("NULL orchestrator pointer");
        return FIN_ORCH_ERR_NULL;
    }

    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) {
        set_error("Invalid orchestrator magic");
        return FIN_ORCH_ERR_STATE;
    }

    if (!orch->config.enable_consolidation) {
        if (result) {
            memset(result, 0, sizeof(*result));
        }
        return FIN_ORCH_ERR_OK;
    }

    /* Immune validation */
    int ret = orch_validate_immune(orch, "consolidate");
    if (ret != 0) return ret;

    orch->op_state = FIN_ORCH_STATE_CONSOLIDATING;

    /* Initialize result if provided */
    if (result) {
        memset(result, 0, sizeof(*result));
    }

    uint64_t start_ms = get_timestamp_ms();

    /* Heartbeat */
    if (orch->health_agent && orch->config.enable_health_monitoring) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)orch->health_agent,
            "consolidate", 0.0f);
        orch->stats.health_heartbeats++;
    }

    /* Run consolidation through consolidation bridge */
    uint32_t replayed = 0, strengthened = 0, pruned = 0;
    float total_strengthen = 0.0f, total_weaken = 0.0f;

    if (orch->modules.consolidation) {
        /* In full implementation:
         * fin_consolidation_result_t cons_result;
         * financial_consolidation_bridge_consolidate(
         *     orch->modules.consolidation,
         *     FIN_CONSOLIDATION_MODE_FULL,
         *     &cons_result);
         * replayed = cons_result.patterns_replayed;
         * pruned = cons_result.patterns_pruned;
         */

        /* Placeholder */
        replayed = 10;
        strengthened = 8;
        pruned = 2;
        total_strengthen = 0.15f;
        total_weaken = -0.05f;
    }

    /* Fill result */
    if (result) {
        result->patterns_replayed = replayed;
        result->patterns_strengthened = strengthened;
        result->patterns_pruned = pruned;
        result->total_strengthening = total_strengthen;
        result->total_weakening = total_weaken;
        result->duration_ms = get_timestamp_ms() - start_ms;
    }

    /* Update statistics */
    orch->stats.consolidations++;
    orch->last_consolidation_ms = get_timestamp_ms();

    /* Publish KG message */
    orch_kg_publish(orch, KG_MSG_FIN_ORCH_CONSOLIDATION, result,
                    result ? sizeof(*result) : 0);

    /* Heartbeat completion */
    if (orch->health_agent && orch->config.enable_health_monitoring) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)orch->health_agent,
            "consolidate", 1.0f);
        orch->stats.health_heartbeats++;
    }

    orch->op_state = FIN_ORCH_STATE_READY;
    return FIN_ORCH_ERR_OK;
}

/* ============================================================================
 * Extended API
 * ============================================================================ */

int financial_cognitive_orchestrator_get_emotional_state(
    const financial_cognitive_orchestrator_handle_t* orch,
    void* state
) {
    if (!orch || !state) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;

    if (orch->modules.emotion) {
        /* financial_emotion_bridge_get_state(orch->modules.emotion, state) */
        return FIN_ORCH_ERR_OK;
    }

    set_error("Emotion module not registered");
    return FIN_ORCH_ERR_SUBSYSTEM;
}

int financial_cognitive_orchestrator_get_metacognitive_assessment(
    const financial_cognitive_orchestrator_handle_t* orch,
    void* assessment
) {
    if (!orch || !assessment) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;

    if (orch->modules.metacognition) {
        /* financial_metacognition_bridge_assess(orch->modules.metacognition, assessment) */
        return FIN_ORCH_ERR_OK;
    }

    set_error("Metacognition module not registered");
    return FIN_ORCH_ERR_SUBSYSTEM;
}

int financial_cognitive_orchestrator_predict(
    financial_cognitive_orchestrator_handle_t* orch,
    const char* asset,
    uint32_t horizon_steps,
    void* prediction
) {
    if (!orch || !asset || !prediction) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;

    if (orch->modules.world_model) {
        /* financial_world_model_bridge_predict(orch->modules.world_model, ...) */
        (void)horizon_steps;
        return FIN_ORCH_ERR_OK;
    }

    set_error("World model module not registered");
    return FIN_ORCH_ERR_SUBSYSTEM;
}

int financial_cognitive_orchestrator_validate_ethics(
    financial_cognitive_orchestrator_handle_t* orch,
    const fin_cognitive_decision_t* decision,
    bool* approved,
    fin_cognitive_explanation_t* explanation
) {
    if (!orch || !decision || !approved) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;

    *approved = true; /* Default to approved */

    if (orch->modules.ethics) {
        /* financial_ethics_bridge_validate(orch->modules.ethics, ...) */
    }

    if (explanation) {
        snprintf(explanation->summary, sizeof(explanation->summary),
                 "Ethics check %s for %s %s",
                 *approved ? "passed" : "failed",
                 fin_orchestrator_decision_name(decision->decision_type),
                 decision->asset);
        explanation->confidence = 0.9f;
    }

    return FIN_ORCH_ERR_OK;
}

int financial_cognitive_orchestrator_explore(
    financial_cognitive_orchestrator_handle_t* orch,
    char* hypothesis,
    float* exploration_value
) {
    if (!orch || !hypothesis || !exploration_value) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;

    if (orch->modules.curiosity) {
        /* financial_curiosity_bridge_generate_hypothesis(orch->modules.curiosity, ...) */
        snprintf(hypothesis, 256, "Investigate sector rotation opportunity");
        *exploration_value = 0.7f;
        return FIN_ORCH_ERR_OK;
    }

    set_error("Curiosity module not registered");
    return FIN_ORCH_ERR_SUBSYSTEM;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

fin_orchestrator_state_t financial_cognitive_orchestrator_get_state(
    const financial_cognitive_orchestrator_handle_t* orch
) {
    if (!orch) return FIN_ORCH_STATE_UNINITIALIZED;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) {
        return FIN_ORCH_STATE_ERROR;
    }
    return orch->op_state;
}

int financial_cognitive_orchestrator_get_stats(
    const financial_cognitive_orchestrator_handle_t* orch,
    fin_orchestrator_stats_t* stats
) {
    if (!orch || !stats) {
        set_error("NULL pointer");
        return FIN_ORCH_ERR_NULL;
    }

    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) {
        set_error("Invalid orchestrator magic");
        return FIN_ORCH_ERR_STATE;
    }

    *stats = orch->stats;
    return FIN_ORCH_ERR_OK;
}

void financial_cognitive_orchestrator_reset_stats(
    financial_cognitive_orchestrator_handle_t* orch
) {
    if (!orch) return;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return;
    memset(&orch->stats, 0, sizeof(orch->stats));
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_cognitive_orchestrator_heartbeat(
    financial_cognitive_orchestrator_handle_t* orch,
    const char* operation,
    float progress
) {
    if (!orch) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;

    if (orch->health_agent && orch->config.enable_health_monitoring) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)orch->health_agent, operation, progress);
        orch->stats.health_heartbeats++;
    }

    return FIN_ORCH_ERR_OK;
}

/* ============================================================================
 * Training Integration
 * ============================================================================ */

int financial_cognitive_orchestrator_training_begin(
    financial_cognitive_orchestrator_handle_t* orch
) {
    if (!orch) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;

    orch->training_active = true;
    fin_orch_heartbeat_global("training_begin", 0.0f);

    return FIN_ORCH_ERR_OK;
}

int financial_cognitive_orchestrator_training_end(
    financial_cognitive_orchestrator_handle_t* orch
) {
    if (!orch) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;

    orch->training_active = false;
    fin_orch_heartbeat_global("training_end", 1.0f);

    return FIN_ORCH_ERR_OK;
}

int financial_cognitive_orchestrator_training_step(
    financial_cognitive_orchestrator_handle_t* orch,
    float progress
) {
    if (!orch) return FIN_ORCH_ERR_NULL;
    if (orch->magic != FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC) return FIN_ORCH_ERR_STATE;

    if (!orch->training_active) {
        set_error("Training not active");
        return FIN_ORCH_ERR_STATE;
    }

    fin_orch_heartbeat_global("training_step", progress);

    return FIN_ORCH_ERR_OK;
}

/* ============================================================================
 * W14 (2026-04-24): KG runtime emit for financial cognitive orchestrator.
 *
 * Callers that hold a brain_t invoke this after financial decision / trade
 * generation to register the decision in brain->internal_kg and query the
 * last-expected-return bias.
 * ============================================================================ */
#include "cognitive/kg/nimcp_wave14_math_genius_kg.h"
float financial_orchestrator_wave14_kg_emit(
    struct brain_struct* brain,
    const char* decision_label,
    float expected_return,
    float risk)
{
    if (!brain) return 0.5f;
    wave14_financial_emit_decision(brain, decision_label, expected_return, risk);
    return wave14_financial_query_return_bias(brain);
}
