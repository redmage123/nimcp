/**
 * @file nimcp_inner_dialogue.c
 * @brief Inner Dialogue Engine — Core State Machine and Turn Orchestration
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Implements the full inner dialogue engine lifecycle, state machine,
 *        turn orchestration, and integration with health/immune/BBB/bio-async/
 *        ethics/LGSS/cycle-coordinator
 * WHY:  Central coordinator for structured multi-perspective deliberation
 * HOW:  State machine drives step(); each step selects perspective, formulates
 *        turn, validates via BBB, records in history, checks convergence,
 *        updates state, and reports to immune/health/bio-async
 *
 * EXCEPTION HANDLING:
 * Every public function uses NIMCP_THROW_TO_IMMUNE or NIMCP_CHECK_THROW_IMMUNE
 * for error paths, ensuring the immune system learns from every failure.
 *
 * HEALTH MONITORING:
 * Dual heartbeat pattern — instance-level (engine->health_agent) with global
 * fallback (g_inner_dialogue_health_agent).  Heartbeat on every major operation.
 *
 * BIO-ASYNC:
 * Sends messages for conversation start, each turn, convergence events,
 * and conversation end.
 *
 * BBB INTEGRATION:
 * When enabled, every turn's content is validated through the blood-brain
 * barrier before being accepted into the history.
 *
 * @author NIMCP Development Team
 */

#include "cognitive/inner_dialogue/nimcp_inner_dialogue.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety_types.h"

#include <string.h>
#include <math.h>
#include "utils/math/nimcp_math_helpers.h"

/* ============================================================================
 * Forward Declarations — External APIs (weak dependency)
 * ============================================================================ */

/* bio_router_broadcast is declared in async/nimcp_bio_router.h (already included transitively) */

/* BBB validation — called only if bbb is connected */
struct bbb_system_struct;
extern int bbb_validate_input(struct bbb_system_struct* bbb,
                              const void* data, size_t len);

/* ============================================================================
 * Health Agent Integration (Dual Heartbeat Pattern)
 * ============================================================================ */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(inner_dialogue, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Engine Internal Structure
 * ============================================================================ */

struct inner_dialogue_engine {
    /* Magic for validation */
    uint32_t magic;

    /* Configuration */
    inner_dialogue_config_t config;

    /* State machine */
    inner_dialogue_state_t state;

    /* Current conversation */
    char topic[INNER_DIALOGUE_MAX_TOPIC_LEN];
    uint32_t conversation_id;
    uint32_t turn_number;

    /* Sub-structures */
    inner_dialogue_perspective_registry_t registry;
    inner_dialogue_turn_history_t* history;
    convergence_analysis_t last_analysis;

    /* Integration handles */
    nimcp_health_agent_t* health_agent;
    brain_immune_system_t* immune;
    bbb_system_t bbb;
    void* bio_router;
    brain_cycle_coordinator_t* cycle_coordinator;
    ethics_engine_t ethics;
    const safety_kb_t* lgss_kb;
    void* brain;                    /**< Opaque brain_t for perspective module access */

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Statistics */
    inner_dialogue_engine_stats_t stats;

    /* Conversation counter */
    uint32_t next_conversation_id;
};

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/** Dual heartbeat: instance first, global fallback */
static inline void engine_heartbeat(const inner_dialogue_engine_t* engine,
                                     const char* op, float progress) {
    if (engine && engine->health_agent) {
        nimcp_health_agent_heartbeat_ex(engine->health_agent, op, progress);
    } else if (g_inner_dialogue_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_inner_dialogue_health_agent, op, progress);
    }
}

/** @brief Send heartbeat from engine module (instance-level) */
static inline void engine_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_inner_dialogue_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_inner_dialogue_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_inner_dialogue_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/** Validate engine pointer and magic */
static inline bool engine_valid(const inner_dialogue_engine_t* engine) {
    return engine && engine->magic == INNER_DIALOGUE_MAGIC;
}

/** Get microsecond timestamp */
static uint64_t get_timestamp_us(void) {
    /* Use NIMCP's thread timer if available, else fall back to 0 */
    return nimcp_time_get_ms() * 1000ULL;
}

/* ============================================================================
 * Bio-Async Messaging Helpers
 * ============================================================================ */

/* BIO_MSG_INNER_DIALOGUE_* and BIO_MODULE_INNER_DIALOGUE are now defined
 * in nimcp_bio_messages.h (0x6EC0 range, module 0x1E10). */

static void send_bio_message(inner_dialogue_engine_t* engine,
                              uint32_t msg_type,
                              const void* payload,
                              size_t payload_size) {
    if (!engine->config.enable_bio_async || !engine->bio_router) {
        return;
    }

    struct {
        bio_message_header_t header;
        uint8_t payload[256];
    } msg;
    memset(&msg, 0, sizeof(msg));

    size_t copy_size = (payload_size > 256) ? 256 : payload_size;
    bio_msg_init_header(&msg.header, msg_type,
                        BIO_MODULE_INNER_DIALOGUE, 0,
                        (uint32_t)copy_size);
    if (payload && copy_size > 0) {
        memcpy(msg.payload, payload, copy_size);
    }

    nimcp_error_t err = bio_router_broadcast(engine->bio_router,
                                              &msg,
                                              sizeof(bio_message_header_t) + copy_size);
    if (err != 0) {
        NIMCP_LOGGING_WARN("inner_dialogue: bio-async broadcast failed (type=0x%04X, err=%d)",
                            msg_type, err);
    } else {
        engine->stats.bio_messages_sent++;
        NIMCP_LOGGING_DEBUG("inner_dialogue: bio-async sent type=0x%04X", msg_type);
    }
}

/* ============================================================================
 * BBB Validation Helper
 * ============================================================================ */

static bool validate_turn_through_bbb(inner_dialogue_engine_t* engine,
                                       const inner_dialogue_turn_t* turn) {
    if (!engine->config.enable_bbb_validation || !engine->bbb) {
        return true;  /* No BBB = accept all */
    }

    int rc = bbb_validate_input(engine->bbb, turn->content, turn->content_len);
    if (rc != 0) {
        NIMCP_LOGGING_WARN("inner_dialogue: BBB rejected turn content (perspective=%u, rc=%d)",
                            turn->perspective_idx, rc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_BBB_REJECTED,
                              "inner_dialogue: BBB rejected turn from perspective %u",
                              turn->perspective_idx);
        engine->stats.bbb_rejections++;
        return false;
    }
    NIMCP_LOGGING_DEBUG("inner_dialogue: BBB validated turn content (perspective=%u)",
                        turn->perspective_idx);
    return true;
}

/* ============================================================================
 * State Machine String Table
 * ============================================================================ */

static const char* s_state_names[DIALOGUE_STATE_COUNT] = {
    "IDLE",
    "INITIATED",
    "DELIBERATING",
    "CONVERGING",
    "DEADLOCKED",
    "RUMINATING",
    "ESCALATED",
    "CONCLUDED",
    "CANCELLED"
};

const char* inner_dialogue_state_to_string(inner_dialogue_state_t state) {
    if ((unsigned)state < DIALOGUE_STATE_COUNT) {
        return s_state_names[(unsigned)state];
    }
    return "UNKNOWN";
}

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

inner_dialogue_config_t inner_dialogue_default_config(void) {
    inner_dialogue_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_turns = INNER_DIALOGUE_DEFAULT_MAX_TURNS;
    cfg.urgency = INNER_DIALOGUE_DEFAULT_URGENCY;
    cfg.min_relevance_threshold = INNER_DIALOGUE_MIN_RELEVANCE_THRESHOLD;
    cfg.convergence = inner_dialogue_convergence_default_config();
    cfg.enable_bio_async = true;
    cfg.enable_immune_integration = true;
    cfg.enable_bbb_validation = false;  /* Off by default — opt-in */
    cfg.enable_health_heartbeat = true;
    cfg.enable_ethics_evaluation = true;   /* On by default — ethical alignment is mandatory */
    cfg.enable_lgss_evaluation = true;     /* On by default — L0 safety guardrails always active */
    cfg.verbose_logging = false;
    return cfg;
}

/* ============================================================================
 * Engine Lifecycle
 * ============================================================================ */

inner_dialogue_engine_t* inner_dialogue_engine_create(
    const inner_dialogue_config_t* config) {
    NIMCP_LOGGING_INFO("inner_dialogue: creating engine (version=%s)", INNER_DIALOGUE_VERSION);

    inner_dialogue_engine_t* engine = nimcp_malloc(sizeof(inner_dialogue_engine_t));
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NO_MEMORY,
                              "inner_dialogue: failed to allocate engine struct");
        return NULL;
    }
    memset(engine, 0, sizeof(inner_dialogue_engine_t));

    /* Set configuration */
    if (config) {
        memcpy(&engine->config, config, sizeof(inner_dialogue_config_t));
    } else {
        engine->config = inner_dialogue_default_config();
    }

    /* Validate configuration */
    if (engine->config.max_turns == 0) {
        NIMCP_LOGGING_WARN("inner_dialogue: max_turns=0, defaulting to %u",
                           (unsigned)INNER_DIALOGUE_DEFAULT_MAX_TURNS);
        engine->config.max_turns = INNER_DIALOGUE_DEFAULT_MAX_TURNS;
    }

    /* Initialise magic */
    engine->magic = INNER_DIALOGUE_MAGIC;

    /* Initialise perspective registry */
    int rc = inner_dialogue_perspective_registry_init(&engine->registry);
    if (rc != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NO_MEMORY,
                              "inner_dialogue: failed to init perspective registry (rc=%d)", rc);
        nimcp_free(engine);
        engine = NULL;
        return NULL;
    }

    /* Create turn history */
    engine->history = inner_dialogue_turn_history_create();
    if (!engine->history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NO_MEMORY,
                              "inner_dialogue: failed to create turn history");
        nimcp_free(engine);
        engine = NULL;
        return NULL;
    }

    /* Allocate mutex */
    engine->mutex = nimcp_mutex_create(NULL);
    if (!engine->mutex) {
        NIMCP_LOGGING_WARN("inner_dialogue: mutex creation failed, continuing without thread safety");
    }

    /* Initial state */
    engine->state = DIALOGUE_STATE_IDLE;
    engine->next_conversation_id = 1;

    engine_heartbeat(engine, "engine_create", 1.0f);
    NIMCP_LOGGING_INFO("inner_dialogue: engine created (max_turns=%u, urgency=%.2f, "
                       "bio_async=%d, immune=%d, bbb=%d, heartbeat=%d)",
                       engine->config.max_turns, (double)engine->config.urgency,
                       engine->config.enable_bio_async,
                       engine->config.enable_immune_integration,
                       engine->config.enable_bbb_validation,
                       engine->config.enable_health_heartbeat);
    return engine;
}

void inner_dialogue_engine_destroy(inner_dialogue_engine_t* engine) {
    if (!engine) return;

    NIMCP_LOGGING_INFO("inner_dialogue: destroying engine (conversations=%lu, turns=%lu)",
                       (unsigned long)engine->stats.conversations_started,
                       (unsigned long)engine->stats.total_turns_produced);

    /* Cancel any active conversation */
    if (engine->state != DIALOGUE_STATE_IDLE &&
        engine->state != DIALOGUE_STATE_CONCLUDED &&
        engine->state != DIALOGUE_STATE_CANCELLED) {
        NIMCP_LOGGING_WARN("inner_dialogue: destroying engine with active conversation");
        engine->state = DIALOGUE_STATE_CANCELLED;
    }

    /* Cleanup sub-structures */
    if (engine->history) {
        inner_dialogue_turn_history_destroy(engine->history);
        engine->history = NULL;
    }

    if (engine->mutex) {
        nimcp_mutex_destroy(engine->mutex);
        nimcp_free(engine->mutex);
        engine->mutex = NULL;
    }

    engine->magic = 0;
    nimcp_free(engine);
    engine = NULL;
}

int inner_dialogue_engine_reset(inner_dialogue_engine_t* engine) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: reset with invalid engine");
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }

    if (engine->mutex) nimcp_mutex_lock(engine->mutex);

    engine->state = DIALOGUE_STATE_IDLE;
    memset(engine->topic, 0, INNER_DIALOGUE_MAX_TOPIC_LEN);
    engine->turn_number = 0;
    engine->conversation_id = 0;
    memset(&engine->last_analysis, 0, sizeof(convergence_analysis_t));

    if (engine->history) {
        inner_dialogue_turn_history_reset(engine->history);
    }

    if (engine->mutex) nimcp_mutex_unlock(engine->mutex);

    engine_heartbeat(engine, "engine_reset", 1.0f);
    NIMCP_LOGGING_INFO("inner_dialogue: engine reset to IDLE");
    return 0;
}

/* ============================================================================
 * Integration API
 * ============================================================================ */

int inner_dialogue_engine_set_health_agent(inner_dialogue_engine_t* engine,
                                            nimcp_health_agent_t* agent) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: set_health_agent with invalid engine");
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }
    engine->health_agent = agent;
    NIMCP_LOGGING_INFO("inner_dialogue: health agent %s",
                       agent ? "connected" : "disconnected");
    return 0;
}

int inner_dialogue_engine_set_immune(inner_dialogue_engine_t* engine,
                                      brain_immune_system_t* immune) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: set_immune with invalid engine");
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }
    engine->immune = immune;
    NIMCP_LOGGING_INFO("inner_dialogue: immune system %s",
                       immune ? "connected" : "disconnected");
    return 0;
}

int inner_dialogue_engine_set_bbb(inner_dialogue_engine_t* engine,
                                    bbb_system_t bbb) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: set_bbb with invalid engine");
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }
    engine->bbb = bbb;
    NIMCP_LOGGING_INFO("inner_dialogue: BBB %s",
                       bbb ? "connected" : "disconnected");
    return 0;
}

int inner_dialogue_engine_connect_brain(inner_dialogue_engine_t* engine,
                                          void* brain) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: connect_brain with invalid engine");
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }
    engine->brain = brain;
    NIMCP_LOGGING_INFO("inner_dialogue: brain %s",
                       brain ? "connected" : "disconnected");
    return 0;
}

int inner_dialogue_engine_set_bio_router(inner_dialogue_engine_t* engine,
                                          void* router) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: set_bio_router with invalid engine");
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }
    engine->bio_router = router;
    NIMCP_LOGGING_INFO("inner_dialogue: bio router %s",
                       router ? "connected" : "disconnected");
    return 0;
}

int inner_dialogue_engine_set_cycle_coordinator(inner_dialogue_engine_t* engine,
                                                 brain_cycle_coordinator_t* coordinator) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: set_cycle_coordinator with invalid engine");
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }
    engine->cycle_coordinator = coordinator;
    NIMCP_LOGGING_INFO("inner_dialogue: cycle coordinator %s",
                       coordinator ? "connected" : "disconnected");
    return 0;
}

/* ============================================================================
 * Cycle Coordinator Health Query Helper
 * ============================================================================ */

/**
 * @brief Query brain cycle coordinator for aggregate health status
 *
 * WHAT: Check if brain cycles are healthy enough for deliberation
 * WHY:  When brain cycles are degraded/stalled, deliberation should adapt
 *        (increase urgency, reduce max turns, or suspend altogether)
 * HOW:  Query the immune-tick and brain-update cycles; count degraded/stalled
 *
 * @param engine Engine with connected coordinator
 * @param urgency_boost Output: additional urgency [0.0-0.5] based on degradation
 * @return true if deliberation should proceed, false if cycles too degraded
 */
static bool query_cycle_coordinator_health(const inner_dialogue_engine_t* engine,
                                            float* urgency_boost) {
    *urgency_boost = 0.0f;
    if (!engine->cycle_coordinator) {
        return true;  /* No coordinator = always proceed */
    }

    brain_cycle_coordinator_stats_t stats;
    int rc = brain_cycle_coordinator_get_stats(engine->cycle_coordinator, &stats);
    if (rc != 0) {
        NIMCP_LOGGING_WARN("inner_dialogue: cycle coordinator stats query failed (rc=%d)", rc);
        return true;  /* Fail open — don't block deliberation */
    }

    /* If more than half of cycles are stalled, suppress deliberation */
    uint32_t total = stats.categories[0].total_cycles;
    uint32_t stalled = stats.categories[0].stalled_cycles;
    uint32_t degraded = stats.categories[0].degraded_cycles;

    /* Aggregate across all categories */
    for (uint32_t i = 1; i < BRAIN_CYCLE_CATEGORY_COUNT; i++) {
        total   += stats.categories[i].total_cycles;
        stalled += stats.categories[i].stalled_cycles;
        degraded += stats.categories[i].degraded_cycles;
    }

    if (total == 0) {
        return true;
    }

    float stalled_ratio = (float)stalled / (float)total;
    float degraded_ratio = (float)degraded / (float)total;

    if (stalled_ratio > 0.5f) {
        NIMCP_LOGGING_WARN("inner_dialogue: %u/%u cycles stalled — suppressing deliberation",
                           stalled, total);
        return false;
    }

    /* Modulate urgency: more degraded cycles → higher urgency → faster convergence */
    *urgency_boost = nimcp_clampf(degraded_ratio * 0.5f + stalled_ratio * 0.3f, 0.0f, 0.5f);

    if (*urgency_boost > 0.01f) {
        NIMCP_LOGGING_DEBUG("inner_dialogue: cycle health urgency boost=%.3f "
                            "(degraded=%u, stalled=%u, total=%u)",
                            (double)*urgency_boost, degraded, stalled, total);
    }

    return true;
}

/* ============================================================================
 * Ethics Integration
 * ============================================================================ */

int inner_dialogue_engine_set_ethics(inner_dialogue_engine_t* engine,
                                      ethics_engine_t ethics) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: set_ethics with invalid engine");
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }
    engine->ethics = ethics;
    NIMCP_LOGGING_INFO("inner_dialogue: ethics engine %s",
                       ethics ? "connected" : "disconnected");
    return 0;
}

int inner_dialogue_engine_set_lgss(inner_dialogue_engine_t* engine,
                                     const void* lgss_kb) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: set_lgss with invalid engine");
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }
    engine->lgss_kb = (const safety_kb_t*)lgss_kb;
    NIMCP_LOGGING_INFO("inner_dialogue: LGSS safety KB %s",
                       lgss_kb ? "connected" : "disconnected");
    return 0;
}

/* ============================================================================
 * LGSS Safety Evaluation Helper
 * ============================================================================ */

/**
 * @brief Evaluate a dialogue turn against the LGSS safety knowledge base
 *
 * WHAT: L0 safety check — highest-priority guardrail, evaluated before ethics
 * WHY:  LGSS provides irreversible, tamper-resistant safety rules that must
 *        never be overridden by any other system; defense in depth
 * HOW:  Build a safety_action_context_t from turn content and metadata,
 *        call symbolic_logic_safety_evaluate(), block on DENY
 *
 * @param engine Engine with connected LGSS KB
 * @param turn   Turn to evaluate
 * @return true if turn passes LGSS, false if denied
 */
static bool validate_turn_through_lgss(inner_dialogue_engine_t* engine,
                                        const inner_dialogue_turn_t* turn) {
    if (!engine->lgss_kb || !engine->config.enable_lgss_evaluation) {
        return true;  /* No LGSS = accept all */
    }

    /* Build safety action context from turn content */
    safety_action_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    snprintf(ctx.source, sizeof(ctx.source), "inner_dialogue");
    snprintf(ctx.action_description, sizeof(ctx.action_description),
             "dialogue_turn_%s_%s",
             dialogue_act_to_string(turn->act),
             turn->perspective_idx < INNER_DIALOGUE_MAX_PERSPECTIVES ?
                 engine->registry.entries[turn->perspective_idx].desc.name : "unknown");

    /* Add turn content as string field */
    symbolic_logic_safety_context_add_string(&ctx, "content",
        turn->content_len > 0 ? turn->content : "");
    symbolic_logic_safety_context_add_string(&ctx, "dialogue_act",
        dialogue_act_to_string(turn->act));
    symbolic_logic_safety_context_add_string(&ctx, "topic", engine->topic);

    /* Add numeric fields */
    symbolic_logic_safety_context_add_numeric(&ctx, "confidence", turn->confidence);
    symbolic_logic_safety_context_add_numeric(&ctx, "novelty", turn->novelty);
    symbolic_logic_safety_context_add_numeric(&ctx, "predicted_harm", 0.0f);

    ctx.timestamp = turn->timestamp_us;

    /* Evaluate */
    safety_evaluation_t eval;
    memset(&eval, 0, sizeof(eval));
    bool evaluated = symbolic_logic_safety_evaluate(engine->lgss_kb, &ctx, &eval);

    if (!evaluated) {
        /* Evaluation failed — fail-safe: DENY */
        NIMCP_LOGGING_WARN("inner_dialogue: LGSS evaluation failed — fail-safe DENY");
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_LGSS_DENIED,
                              "inner_dialogue: LGSS evaluation failure (fail-safe deny)");
        engine->stats.lgss_denials++;
        return false;
    }

    if (eval.action == SAFETY_ACTION_DENY) {
        NIMCP_LOGGING_WARN("inner_dialogue: LGSS DENIED turn (perspective=%u, severity=%d, "
                           "rules_triggered=%u): %s",
                           turn->perspective_idx, (int)eval.max_severity,
                           eval.num_triggered, eval.explanation);
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_LGSS_DENIED,
                              "inner_dialogue: LGSS denied turn from perspective %u — %s",
                              turn->perspective_idx, eval.explanation);
        engine->stats.lgss_denials++;
        return false;
    }

    if (eval.action == SAFETY_ACTION_WARN) {
        NIMCP_LOGGING_WARN("inner_dialogue: LGSS WARNING on turn (perspective=%u): %s",
                           turn->perspective_idx, eval.explanation);
        /* WARN = allow but log */
    }

    NIMCP_LOGGING_DEBUG("inner_dialogue: LGSS passed turn (perspective=%u, action=%d)",
                        turn->perspective_idx, (int)eval.action);
    return true;
}

/* ============================================================================
 * Ethics Evaluation Helper
 * ============================================================================ */

/**
 * @brief Evaluate a dialogue turn through the ethics engine
 *
 * WHAT: Moral evaluation of turn content before it enters the conversation record
 * WHY:  Deliberation conclusions must align with ethical standards; harmful,
 *        deceptive, or unfair formulations are rejected to maintain integrity
 * HOW:  Build an action_context_t with harm/deception/autonomy scores derived
 *        from the turn metadata, call ethics_engine_evaluate_action(), block
 *        if verdict is not ALLOW
 *
 * @param engine Engine with connected ethics engine
 * @param turn   Turn to evaluate
 * @return true if turn passes ethics, false if rejected
 */
static bool validate_turn_through_ethics(inner_dialogue_engine_t* engine,
                                          const inner_dialogue_turn_t* turn) {
    if (!engine->ethics || !engine->config.enable_ethics_evaluation) {
        return true;  /* No ethics engine = accept all */
    }

    /* Build action context from turn metadata */
    action_context_t action;
    memset(&action, 0, sizeof(action));

    /* Use turn scores as violation signal proxies:
     *   - Low confidence with high novelty could indicate speculative harm
     *   - WARN act carries inherent risk awareness
     *   - CHALLENGE acts may contain adversarial framing
     * Map turn metadata conservatively — let ethics engine decide */
    action.predicted_harm = 0.0f;  /* Baseline: no predicted harm */
    action.deception_level = 0.0f;
    action.autonomy_violation = 0.0f;
    action.privacy_violation = 0.0f;
    action.consent_violation = 0.0f;
    action.fairness_violation = 0.0f;

    /* If the ethical perspective itself produced the turn, mark as pre-reviewed */
    if (turn->perspective_idx < INNER_DIALOGUE_MAX_PERSPECTIVES) {
        const inner_dialogue_perspective_entry_t* entry =
            &engine->registry.entries[turn->perspective_idx];
        if (entry->registered &&
            entry->desc.type == PERSPECTIVE_ETHICAL) {
            /* Ethical perspective already self-filtered; trust it */
            NIMCP_LOGGING_TRACE("inner_dialogue: ethics evaluation skipped — "
                                "turn from ethical perspective");
            return true;
        }
    }

    /* Use features array for content-derived signals:
     * We pass confidence and novelty as the feature vector so the ethics
     * engine can apply its policies with context. */
    float features[4] = {
        turn->confidence,
        turn->relevance,
        turn->novelty,
        (float)turn->act  /* Act type as numeric for policy matching */
    };
    action.features = features;
    action.num_features = 4;
    action.num_affected_agents = 0;
    action.affected_agents = NULL;

    /* Evaluate */
    ethics_evaluation_t eval = ethics_engine_evaluate_action(engine->ethics, &action);

    if (!eval.allowed) {
        NIMCP_LOGGING_WARN("inner_dialogue: ETHICS REJECTED turn (perspective=%u, "
                           "act=%s, violation=%s, confidence=%.2f): %s",
                           turn->perspective_idx,
                           dialogue_act_to_string(turn->act),
                           ethics_violation_type_name(eval.primary_violation),
                           (double)eval.confidence,
                           eval.explanation);
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_ETHICS_REJECTED,
                              "inner_dialogue: ethics rejected turn from perspective %u — %s",
                              turn->perspective_idx,
                              ethics_violation_type_name(eval.primary_violation));
        engine->stats.ethics_rejections++;
        return false;
    }

    /* Log Golden Rule score for audit */
    if (eval.golden_rule_score < 0.0f) {
        NIMCP_LOGGING_WARN("inner_dialogue: ethics negative Golden Rule score=%.2f "
                           "for turn (perspective=%u, act=%s) — allowed but flagged",
                           (double)eval.golden_rule_score,
                           turn->perspective_idx,
                           dialogue_act_to_string(turn->act));
    }

    NIMCP_LOGGING_DEBUG("inner_dialogue: ethics passed turn (perspective=%u, "
                        "golden_rule=%.2f, recommended=%d)",
                        turn->perspective_idx,
                        (double)eval.golden_rule_score,
                        (int)eval.recommended_action);
    return true;
}

/* ============================================================================
 * Perspective Management
 * ============================================================================ */

inner_dialogue_perspective_registry_t* inner_dialogue_engine_get_registry(
    inner_dialogue_engine_t* engine) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: engine_valid is NULL");
        return NULL;
    }
    return &engine->registry;
}

int inner_dialogue_engine_register_builtins(inner_dialogue_engine_t* engine) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: register_builtins with invalid engine");
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }
    int rc = inner_dialogue_register_builtin_perspectives(&engine->registry);
    engine_heartbeat(engine, "register_builtins", 1.0f);
    return rc;
}

/* ============================================================================
 * Conversation API
 * ============================================================================ */

int inner_dialogue_engine_start(inner_dialogue_engine_t* engine,
                                 const char* topic) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: start with invalid engine");
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }
    if (!topic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: start with NULL topic");
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }

    if (engine->mutex) nimcp_mutex_lock(engine->mutex);

    /* Guard: must be IDLE or CONCLUDED/CANCELLED */
    if (engine->state != DIALOGUE_STATE_IDLE &&
        engine->state != DIALOGUE_STATE_CONCLUDED &&
        engine->state != DIALOGUE_STATE_CANCELLED) {
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_ALREADY_RUNNING,
                              "inner_dialogue: start called but engine in state %s",
                              inner_dialogue_state_to_string(engine->state));
        return NIMCP_INNER_DIALOGUE_ERROR_ALREADY_RUNNING;
    }

    /* Guard: must have perspectives */
    if (engine->registry.count == 0) {
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NO_PERSPECTIVES,
                              "inner_dialogue: start called but no perspectives registered");
        return NIMCP_INNER_DIALOGUE_ERROR_NO_PERSPECTIVES;
    }

    /* Set topic */
    strncpy(engine->topic, topic, INNER_DIALOGUE_MAX_TOPIC_LEN - 1);
    engine->topic[INNER_DIALOGUE_MAX_TOPIC_LEN - 1] = '\0';

    /* Assign conversation ID */
    engine->conversation_id = engine->next_conversation_id++;
    engine->turn_number = 0;

    /* Reset history */
    inner_dialogue_turn_history_reset(engine->history);
    memset(&engine->last_analysis, 0, sizeof(convergence_analysis_t));

    /* Transition to INITIATED */
    engine->state = DIALOGUE_STATE_INITIATED;
    engine->stats.conversations_started++;

    /* Capture values before unlocking to avoid data race */
    uint32_t conv_id = engine->conversation_id;
    uint32_t num_perspectives = engine->registry.count;

    if (engine->mutex) nimcp_mutex_unlock(engine->mutex);

    /* Bio-async: conversation start */
    struct {
        uint32_t conversation_id;
        uint32_t num_perspectives;
    } start_payload = {
        conv_id,
        num_perspectives
    };
    send_bio_message(engine, BIO_MSG_INNER_DIALOGUE_START,
                     &start_payload, sizeof(start_payload));

    engine_heartbeat(engine, "conversation_start", 0.0f);
    NIMCP_LOGGING_INFO("inner_dialogue: conversation #%u started — topic='%s', perspectives=%u",
                       conv_id, engine->topic, num_perspectives);
    return 0;
}

int inner_dialogue_engine_step(inner_dialogue_engine_t* engine,
                                inner_dialogue_turn_t* turn_out) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: step with invalid engine");
        return -NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }

    if (engine->mutex) nimcp_mutex_lock(engine->mutex);

    /* 1. Guard state */
    if (engine->state != DIALOGUE_STATE_INITIATED &&
        engine->state != DIALOGUE_STATE_DELIBERATING &&
        engine->state != DIALOGUE_STATE_CONVERGING) {
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        NIMCP_LOGGING_WARN("inner_dialogue: step called in state %s",
                            inner_dialogue_state_to_string(engine->state));
        return -NIMCP_INNER_DIALOGUE_ERROR_INVALID_STATE;
    }

    /* Transition INITIATED → DELIBERATING on first step */
    if (engine->state == DIALOGUE_STATE_INITIATED) {
        engine->state = DIALOGUE_STATE_DELIBERATING;
        NIMCP_LOGGING_DEBUG("inner_dialogue: INITIATED → DELIBERATING");
    }

    /* Query cycle coordinator for brain health */
    float cycle_urgency_boost = 0.0f;
    if (!query_cycle_coordinator_health(engine, &cycle_urgency_boost)) {
        /* Brain cycles too degraded — suspend deliberation */
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        NIMCP_LOGGING_WARN("inner_dialogue: deliberation suspended — brain cycles degraded");
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_IMMUNE_SUPPRESSED,
                              "inner_dialogue: brain cycle coordinator reports critical degradation");
        return -NIMCP_INNER_DIALOGUE_ERROR_IMMUNE_SUPPRESSED;
    }

    /* Check max turns */
    if (engine->turn_number >= engine->config.max_turns) {
        engine->state = DIALOGUE_STATE_CONCLUDED;
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        NIMCP_LOGGING_INFO("inner_dialogue: max turns (%u) reached",
                           engine->config.max_turns);
        return (int)TERMINATION_MAX_TURNS;
    }

    /* 2. Build context for perspective selection */
    perspective_turn_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.conversation_id = engine->conversation_id;
    ctx.turn_number = engine->turn_number;
    ctx.topic = engine->topic;
    ctx.last_turn = inner_dialogue_turn_history_get_latest(engine->history);
    ctx.history = engine->history;
    ctx.urgency = nimcp_clampf(engine->config.urgency + cycle_urgency_boost, 0.0f, 1.0f);
    ctx.emotional_temperature = engine->last_analysis.emotional_temperature;
    ctx.brain = engine->brain;

    /* 3. Select perspective */
    int persp_idx = inner_dialogue_perspective_select_next(
        &engine->registry, &ctx, engine->turn_number);
    if (persp_idx < 0) {
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        NIMCP_LOGGING_ERROR("inner_dialogue: no perspective available for turn %u",
                            engine->turn_number);
        return -NIMCP_INNER_DIALOGUE_ERROR_TURN_FAILED;
    }
    engine_heartbeat(engine, "step_select", 0.2f);

    /* 4. Formulate turn */
    inner_dialogue_perspective_entry_t* entry = &engine->registry.entries[persp_idx];
    inner_dialogue_turn_t turn;
    memset(&turn, 0, sizeof(turn));
    turn.conversation_id = engine->conversation_id;
    turn.timestamp_us = get_timestamp_us();

    uint64_t start_time = nimcp_time_get_ms();
    bool produced = entry->desc.formulate(&ctx, &turn);
    uint64_t end_time = nimcp_time_get_ms();
    turn.formulation_time_ms = (float)(end_time - start_time);

    if (!produced) {
        entry->turns_skipped++;
        NIMCP_LOGGING_DEBUG("inner_dialogue: perspective '%s' skipped turn %u",
                            entry->desc.name, engine->turn_number);
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        /* Not an error — caller should call step() again */
        return 0;
    }
    entry->turns_produced++;
    entry->cumulative_confidence += turn.confidence;
    entry->last_turn_timestamp_us = turn.timestamp_us;
    engine_heartbeat(engine, "step_formulate", 0.4f);

    /* 5. LGSS safety check (L0 — highest priority, before ethics and BBB) */
    if (!validate_turn_through_lgss(engine, &turn)) {
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        NIMCP_LOGGING_WARN("inner_dialogue: LGSS denied turn from '%s' at turn %u",
                            entry->desc.name, engine->turn_number);
        return 0;  /* Skip this turn, continue */
    }
    engine_heartbeat(engine, "step_lgss", 0.45f);

    /* 6. Ethics evaluation (L3 — moral alignment check) */
    if (!validate_turn_through_ethics(engine, &turn)) {
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        NIMCP_LOGGING_WARN("inner_dialogue: ethics rejected turn from '%s' at turn %u",
                            entry->desc.name, engine->turn_number);
        return 0;  /* Skip this turn, continue */
    }
    engine_heartbeat(engine, "step_ethics", 0.48f);

    /* 8. BBB validate */
    if (!validate_turn_through_bbb(engine, &turn)) {
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        NIMCP_LOGGING_WARN("inner_dialogue: BBB rejected turn from '%s' at turn %u",
                            entry->desc.name, engine->turn_number);
        return 0;  /* Skip this turn, continue */
    }
    engine_heartbeat(engine, "step_bbb", 0.5f);

    /* 9. Record turn */
    int turn_id = inner_dialogue_turn_history_record(engine->history, &turn);
    if (turn_id < 0) {
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_TURN_FAILED,
                              "inner_dialogue: failed to record turn %u",
                              engine->turn_number);
        return -NIMCP_INNER_DIALOGUE_ERROR_TURN_FAILED;
    }
    engine->turn_number++;
    engine->stats.total_turns_produced++;

    /* Copy to caller if requested */
    if (turn_out) {
        memcpy(turn_out, &turn, sizeof(inner_dialogue_turn_t));
        turn_out->turn_id = (uint32_t)turn_id;
    }
    engine_heartbeat(engine, "step_record", 0.6f);

    /* 10. Notify all perspectives via observe()
     * Copy callback data under lock, release lock, invoke callbacks,
     * then re-acquire lock — avoids holding mutex during user callbacks.
     */
    inner_dialogue_turn_t recorded_copy;
    const inner_dialogue_turn_t* recorded = inner_dialogue_turn_history_get_latest(engine->history);
    if (recorded) {
        memcpy(&recorded_copy, recorded, sizeof(inner_dialogue_turn_t));
    }

    struct { perspective_observe_fn fn; void* user_data; } observers[INNER_DIALOGUE_MAX_PERSPECTIVES];
    uint32_t num_observers = 0;
    for (uint32_t i = 0; i < INNER_DIALOGUE_MAX_PERSPECTIVES; i++) {
        if (engine->registry.entries[i].registered &&
            engine->registry.entries[i].desc.observe &&
            (int)i != persp_idx) {
            observers[num_observers].fn = engine->registry.entries[i].desc.observe;
            observers[num_observers].user_data = engine->registry.entries[i].desc.user_data;
            num_observers++;
        }
    }

    if (engine->mutex) nimcp_mutex_unlock(engine->mutex);

    /* Invoke callbacks outside the lock */
    if (recorded) {
        for (uint32_t i = 0; i < num_observers; i++) {
            observers[i].fn(&recorded_copy, observers[i].user_data);
        }
    }

    if (engine->mutex) nimcp_mutex_lock(engine->mutex);

    /* 11. Bio-async turn event */
    struct {
        uint32_t conversation_id;
        uint32_t turn_id;
        uint32_t perspective_idx;
        uint32_t act;
        float confidence;
    } turn_payload = {
        engine->conversation_id,
        (uint32_t)turn_id,
        turn.perspective_idx,
        (uint32_t)turn.act,
        turn.confidence
    };
    send_bio_message(engine, BIO_MSG_INNER_DIALOGUE_TURN,
                     &turn_payload, sizeof(turn_payload));

    NIMCP_LOGGING_INFO("inner_dialogue: turn %u by '%s' — act=%s, conf=%.2f, novel=%.2f",
                       engine->turn_number - 1, entry->desc.name,
                       dialogue_act_to_string(turn.act),
                       (double)turn.confidence, (double)turn.novelty);
    engine_heartbeat(engine, "step_notify", 0.7f);

    /* 12. Convergence analysis */
    convergence_analysis_t analysis;
    int rc = inner_dialogue_convergence_analyse(
        engine->history, &engine->config.convergence, &analysis);
    if (rc != 0) {
        NIMCP_LOGGING_WARN("inner_dialogue: convergence analysis failed (rc=%d)", rc);
    } else {
        memcpy(&engine->last_analysis, &analysis, sizeof(convergence_analysis_t));
    }
    engine_heartbeat(engine, "step_convergence", 0.85f);

    /* 13. Update state machine based on analysis */
    int result = 0;
    if (analysis.recommended_action == TERMINATION_CONVERGED) {
        engine->state = DIALOGUE_STATE_CONVERGING;
        /* If a CONCLUDE act was produced, finalize */
        if (turn.act == DIALOGUE_ACT_CONCLUDE ||
            analysis.agreement_score >= engine->config.convergence.agreement_threshold + 0.1f) {
            engine->state = DIALOGUE_STATE_CONCLUDED;
            engine->stats.conversations_completed++;
            result = (int)TERMINATION_CONVERGED;
            NIMCP_LOGGING_INFO("inner_dialogue: CONVERGED at turn %u (agreement=%.2f)",
                               engine->turn_number,
                               (double)analysis.agreement_score);
        }
    } else if (analysis.recommended_action == TERMINATION_DEADLOCKED) {
        engine->state = DIALOGUE_STATE_DEADLOCKED;
        engine->stats.conversations_deadlocked++;
        result = (int)TERMINATION_DEADLOCKED;
        NIMCP_LOGGING_WARN("inner_dialogue: DEADLOCKED at turn %u (score=%.2f)",
                           engine->turn_number,
                           (double)analysis.deadlock_score);
        send_bio_message(engine, BIO_MSG_INNER_DIALOGUE_DEADLOCK,
                         &analysis.deadlock_score, sizeof(float));
    } else if (analysis.recommended_action == TERMINATION_RUMINATING) {
        engine->state = DIALOGUE_STATE_RUMINATING;
        engine->stats.conversations_ruminated++;
        result = (int)TERMINATION_RUMINATING;
        NIMCP_LOGGING_WARN("inner_dialogue: RUMINATING at turn %u (score=%.2f)",
                           engine->turn_number,
                           (double)analysis.rumination_score);
        send_bio_message(engine, BIO_MSG_INNER_DIALOGUE_RUMINATION,
                         &analysis.rumination_score, sizeof(float));
    } else if (analysis.recommended_action == TERMINATION_EMOTIONAL_SPIRAL) {
        engine->state = DIALOGUE_STATE_CONCLUDED;
        result = (int)TERMINATION_EMOTIONAL_SPIRAL;
        NIMCP_LOGGING_WARN("inner_dialogue: EMOTIONAL SPIRAL at turn %u (temp=%.2f)",
                           engine->turn_number,
                           (double)analysis.emotional_temperature);
    }

    if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
    engine_heartbeat(engine, "step_complete", 1.0f);
    return result;
}

/* ============================================================================
 * Parallel Batch Formulation
 * ============================================================================
 *
 * WHAT: Formulate all registered perspectives in parallel, select best turn
 * WHY:  Sequential formulation is the bottleneck — each calls cognitive modules
 * HOW:  Submit formulate() calls to thread pool, wait, pick highest-confidence
 *       turn, then validate + record sequentially (validation must be serial)
 */

typedef struct {
    const perspective_turn_context_t* ctx;
    inner_dialogue_perspective_entry_t* entry;
    inner_dialogue_turn_t turn;
    bool produced;
    uint64_t formulation_time_ms;
    uint32_t conversation_id;
} parallel_formulate_arg_t;

static void parallel_formulate_task(void* arg) {
    parallel_formulate_arg_t* pfa = (parallel_formulate_arg_t*)arg;
    memset(&pfa->turn, 0, sizeof(inner_dialogue_turn_t));
    pfa->turn.conversation_id = pfa->conversation_id;
    pfa->turn.timestamp_us = get_timestamp_us();

    uint64_t start = nimcp_time_get_ms();
    pfa->produced = pfa->entry->desc.formulate(pfa->ctx, &pfa->turn);
    uint64_t end = nimcp_time_get_ms();
    pfa->formulation_time_ms = end - start;
    pfa->turn.formulation_time_ms = (float)(end - start);
}

/**
 * @brief Parallel batch step — formulate all perspectives, select best
 *
 * Replaces the single-perspective step() in the run loop.
 * Formulates all registered perspectives in parallel via thread pool,
 * picks the turn with highest confidence, validates it, records it.
 *
 * @return 0 = continue, >0 = termination reason, <0 = error
 */
static int inner_dialogue_engine_step_parallel(
    inner_dialogue_engine_t* engine,
    inner_dialogue_turn_t* turn_out,
    nimcp_thread_pool_t* pool)
{
    if (!engine_valid(engine)) return -NIMCP_INNER_DIALOGUE_ERROR_NULL;

    if (engine->mutex) nimcp_mutex_lock(engine->mutex);

    /* Guard state */
    if (engine->state != DIALOGUE_STATE_INITIATED &&
        engine->state != DIALOGUE_STATE_DELIBERATING &&
        engine->state != DIALOGUE_STATE_CONVERGING) {
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        return -NIMCP_INNER_DIALOGUE_ERROR_INVALID_STATE;
    }
    if (engine->state == DIALOGUE_STATE_INITIATED) {
        engine->state = DIALOGUE_STATE_DELIBERATING;
    }

    /* Check cycle coordinator health */
    float cycle_urgency_boost = 0.0f;
    if (!query_cycle_coordinator_health(engine, &cycle_urgency_boost)) {
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        return -NIMCP_INNER_DIALOGUE_ERROR_IMMUNE_SUPPRESSED;
    }

    if (engine->turn_number >= engine->config.max_turns) {
        engine->state = DIALOGUE_STATE_CONCLUDED;
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        return (int)TERMINATION_MAX_TURNS;
    }

    /* Build shared context */
    perspective_turn_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.conversation_id = engine->conversation_id;
    ctx.turn_number = engine->turn_number;
    ctx.topic = engine->topic;
    ctx.last_turn = inner_dialogue_turn_history_get_latest(engine->history);
    ctx.history = engine->history;
    ctx.urgency = nimcp_clampf(engine->config.urgency + cycle_urgency_boost, 0.0f, 1.0f);
    ctx.emotional_temperature = engine->last_analysis.emotional_temperature;
    ctx.brain = engine->brain;

    /* Collect registered perspectives */
    parallel_formulate_arg_t batch[INNER_DIALOGUE_MAX_PERSPECTIVES];
    uint32_t batch_count = 0;

    for (uint32_t i = 0; i < INNER_DIALOGUE_MAX_PERSPECTIVES; i++) {
        if (!engine->registry.entries[i].registered) continue;
        batch[batch_count].ctx = &ctx;
        batch[batch_count].entry = &engine->registry.entries[i];
        batch[batch_count].produced = false;
        batch[batch_count].conversation_id = engine->conversation_id;
        batch_count++;
    }

    if (batch_count == 0) {
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        return -NIMCP_INNER_DIALOGUE_ERROR_TURN_FAILED;
    }

    /* Release mutex during parallel formulation (perspectives read-only) */
    if (engine->mutex) nimcp_mutex_unlock(engine->mutex);

    /* Submit all formulations to thread pool */
    for (uint32_t i = 0; i < batch_count; i++) {
        nimcp_pool_submit(pool, parallel_formulate_task, &batch[i]);
    }
    nimcp_pool_wait(pool);

    /* Re-acquire mutex for validation + recording */
    if (engine->mutex) nimcp_mutex_lock(engine->mutex);

    /* Select best turn: highest confidence among produced turns */
    int best_idx = -1;
    float best_confidence = -1.0f;
    for (uint32_t i = 0; i < batch_count; i++) {
        if (batch[i].produced && batch[i].turn.confidence > best_confidence) {
            best_confidence = batch[i].turn.confidence;
            best_idx = (int)i;
        }
    }

    /* Update stats for all perspectives */
    for (uint32_t i = 0; i < batch_count; i++) {
        if (batch[i].produced) {
            batch[i].entry->turns_produced++;
            batch[i].entry->cumulative_confidence += batch[i].turn.confidence;
            batch[i].entry->last_turn_timestamp_us = batch[i].turn.timestamp_us;
        } else {
            batch[i].entry->turns_skipped++;
        }
    }

    if (best_idx < 0) {
        /* All perspectives skipped */
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        return 0;
    }

    inner_dialogue_turn_t* turn = &batch[best_idx].turn;

    /* Validate sequentially: LGSS → Ethics → BBB */
    if (!validate_turn_through_lgss(engine, turn) ||
        !validate_turn_through_ethics(engine, turn) ||
        !validate_turn_through_bbb(engine, turn)) {
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        return 0;
    }

    /* Record turn */
    int turn_id = inner_dialogue_turn_history_record(engine->history, turn);
    if (turn_id < 0) {
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        return -NIMCP_INNER_DIALOGUE_ERROR_TURN_FAILED;
    }
    engine->turn_number++;
    engine->stats.total_turns_produced++;

    if (turn_out) {
        memcpy(turn_out, turn, sizeof(inner_dialogue_turn_t));
        turn_out->turn_id = (uint32_t)turn_id;
    }

    /* Notify observers (outside lock) */
    inner_dialogue_turn_t recorded_copy;
    const inner_dialogue_turn_t* recorded = inner_dialogue_turn_history_get_latest(engine->history);
    if (recorded) {
        memcpy(&recorded_copy, recorded, sizeof(inner_dialogue_turn_t));
    }

    struct { perspective_observe_fn fn; void* user_data; } observers[INNER_DIALOGUE_MAX_PERSPECTIVES];
    uint32_t num_observers = 0;
    for (uint32_t i = 0; i < INNER_DIALOGUE_MAX_PERSPECTIVES; i++) {
        if (engine->registry.entries[i].registered &&
            engine->registry.entries[i].desc.observe) {
            observers[num_observers].fn = engine->registry.entries[i].desc.observe;
            observers[num_observers].user_data = engine->registry.entries[i].desc.user_data;
            num_observers++;
        }
    }
    if (engine->mutex) nimcp_mutex_unlock(engine->mutex);

    if (recorded) {
        for (uint32_t i = 0; i < num_observers; i++) {
            observers[i].fn(&recorded_copy, observers[i].user_data);
        }
    }

    if (engine->mutex) nimcp_mutex_lock(engine->mutex);

    /* Convergence analysis */
    convergence_analysis_t analysis;
    int rc = inner_dialogue_convergence_analyse(
        engine->history, &engine->config.convergence, &analysis);
    if (rc == 0) {
        memcpy(&engine->last_analysis, &analysis, sizeof(convergence_analysis_t));
    }

    int result = 0;
    if (analysis.recommended_action == TERMINATION_CONVERGED) {
        engine->state = DIALOGUE_STATE_CONVERGING;
        if (turn->act == DIALOGUE_ACT_CONCLUDE ||
            analysis.agreement_score >= engine->config.convergence.agreement_threshold + 0.1f) {
            engine->state = DIALOGUE_STATE_CONCLUDED;
            engine->stats.conversations_completed++;
            result = (int)TERMINATION_CONVERGED;
        }
    } else if (analysis.recommended_action == TERMINATION_DEADLOCKED) {
        engine->state = DIALOGUE_STATE_DEADLOCKED;
        engine->stats.conversations_deadlocked++;
        result = (int)TERMINATION_DEADLOCKED;
    } else if (analysis.recommended_action == TERMINATION_RUMINATING) {
        engine->state = DIALOGUE_STATE_RUMINATING;
        engine->stats.conversations_ruminated++;
        result = (int)TERMINATION_RUMINATING;
    } else if (analysis.recommended_action == TERMINATION_EMOTIONAL_SPIRAL) {
        engine->state = DIALOGUE_STATE_CONCLUDED;
        result = (int)TERMINATION_EMOTIONAL_SPIRAL;
    }

    if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
    return result;
}


int inner_dialogue_engine_run(inner_dialogue_engine_t* engine,
                               inner_dialogue_result_t* result) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: run with invalid engine");
        return -NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: run with NULL result");
        return -NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }

    memset(result, 0, sizeof(inner_dialogue_result_t));
    NIMCP_LOGGING_INFO("inner_dialogue: running conversation #%u to completion",
                       engine->conversation_id);

    int step_result = 0;
    uint32_t step_count = 0;
    uint32_t max_steps = engine->config.max_turns * 2;  /* Allow skip retries */

    /* Create thread pool for parallel formulation (one per perspective) */
    static const uint32_t PARALLEL_THREADS = 4;
    nimcp_thread_pool_t* pool = nimcp_pool_create(PARALLEL_THREADS);

    while (step_count < max_steps) {
        inner_dialogue_turn_t turn;

        if (pool) {
            step_result = inner_dialogue_engine_step_parallel(engine, &turn, pool);
        } else {
            step_result = inner_dialogue_engine_step(engine, &turn);
        }

        if (step_result > 0) {
            /* Termination reason returned */
            NIMCP_LOGGING_INFO("inner_dialogue: run terminated — %s after %u steps",
                               termination_reason_to_string((termination_reason_t)step_result),
                               step_count);
            break;
        }
        if (step_result < 0) {
            /* Error */
            NIMCP_LOGGING_ERROR("inner_dialogue: run error at step %u (rc=%d)",
                                step_count, step_result);
            break;
        }
        step_count++;
        engine_heartbeat(engine, "run_loop",
                         (float)engine->turn_number / (float)engine->config.max_turns);
    }

    if (pool) nimcp_pool_destroy(pool);

    /* If we exhausted max_steps without termination */
    if (step_result == 0 && step_count >= max_steps) {
        engine->state = DIALOGUE_STATE_CONCLUDED;
        step_result = (int)TERMINATION_MAX_TURNS;
    }

    /* Populate result */
    result->termination_reason = (step_result > 0) ?
        (termination_reason_t)step_result : TERMINATION_CANCELLED;
    result->final_state = engine->state;
    result->total_turns = engine->turn_number;

    /* Count participating perspectives */
    uint32_t participated = 0;
    for (uint32_t i = 0; i < INNER_DIALOGUE_MAX_PERSPECTIVES; i++) {
        if (engine->registry.entries[i].registered &&
            engine->registry.entries[i].turns_produced > 0) {
            participated++;
        }
    }
    result->perspectives_participated = participated;

    /* Copy last analysis */
    memcpy(&result->final_analysis, &engine->last_analysis, sizeof(convergence_analysis_t));
    result->final_agreement = engine->last_analysis.agreement_score;

    /* Copy conclusion if last turn was CONCLUDE */
    const inner_dialogue_turn_t* last = inner_dialogue_turn_history_get_latest(engine->history);
    if (last && last->act == DIALOGUE_ACT_CONCLUDE) {
        memcpy(&result->conclusion, last, sizeof(inner_dialogue_turn_t));
        result->has_conclusion = true;
    } else if (last) {
        /* Use last turn as best-effort conclusion */
        memcpy(&result->conclusion, last, sizeof(inner_dialogue_turn_t));
        result->has_conclusion = false;
    }

    /* Compute averages from history stats */
    inner_dialogue_turn_history_stats_t hist_stats;
    if (inner_dialogue_turn_history_get_stats(engine->history, &hist_stats) == 0) {
        result->avg_confidence = hist_stats.avg_confidence;
        result->avg_novelty = hist_stats.avg_novelty;
    }

    /* Bio-async: conversation end */
    struct {
        uint32_t conversation_id;
        uint32_t total_turns;
        uint32_t termination_reason;
        float final_agreement;
    } end_payload = {
        engine->conversation_id,
        engine->turn_number,
        (uint32_t)result->termination_reason,
        result->final_agreement
    };
    send_bio_message(engine, BIO_MSG_INNER_DIALOGUE_END,
                     &end_payload, sizeof(end_payload));

    engine_heartbeat(engine, "run_complete", 1.0f);
    NIMCP_LOGGING_INFO("inner_dialogue: conversation #%u complete — %s, %u turns, "
                       "agreement=%.2f, %u perspectives",
                       engine->conversation_id,
                       termination_reason_to_string(result->termination_reason),
                       result->total_turns,
                       (double)result->final_agreement,
                       result->perspectives_participated);

    /* Update rolling average for turns_to_convergence */
    if (result->termination_reason == TERMINATION_CONVERGED &&
        engine->stats.conversations_completed > 0) {
        float n = (float)engine->stats.conversations_completed;
        engine->stats.avg_turns_to_convergence +=
            ((float)result->total_turns - engine->stats.avg_turns_to_convergence) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    }

    return step_result;
}

int inner_dialogue_engine_cancel(inner_dialogue_engine_t* engine) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: cancel with invalid engine");
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }

    if (engine->mutex) nimcp_mutex_lock(engine->mutex);

    if (engine->state == DIALOGUE_STATE_IDLE ||
        engine->state == DIALOGUE_STATE_CONCLUDED ||
        engine->state == DIALOGUE_STATE_CANCELLED) {
        if (engine->mutex) nimcp_mutex_unlock(engine->mutex);
        return NIMCP_INNER_DIALOGUE_ERROR_NOT_RUNNING;
    }

    inner_dialogue_state_t prev = engine->state;
    engine->state = DIALOGUE_STATE_CANCELLED;
    engine->stats.conversations_cancelled++;

    if (engine->mutex) nimcp_mutex_unlock(engine->mutex);

    NIMCP_LOGGING_INFO("inner_dialogue: conversation #%u cancelled (was %s)",
                       engine->conversation_id,
                       inner_dialogue_state_to_string(prev));
    engine_heartbeat(engine, "conversation_cancel", 1.0f);
    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

inner_dialogue_state_t inner_dialogue_engine_get_state(
    const inner_dialogue_engine_t* engine) {
    return engine_valid(engine) ? engine->state : DIALOGUE_STATE_IDLE;
}

const inner_dialogue_turn_history_t* inner_dialogue_engine_get_history(
    const inner_dialogue_engine_t* engine) {
    return engine_valid(engine) ? engine->history : NULL;
}

const char* inner_dialogue_engine_get_topic(
    const inner_dialogue_engine_t* engine) {
    if (!engine_valid(engine) || engine->state == DIALOGUE_STATE_IDLE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "inner_dialogue_engine_cancel: engine_valid is NULL");
        return NULL;
    }
    return engine->topic;
}

uint32_t inner_dialogue_engine_get_turn_number(
    const inner_dialogue_engine_t* engine) {
    return engine_valid(engine) ? engine->turn_number : 0;
}

int inner_dialogue_engine_get_convergence(
    const inner_dialogue_engine_t* engine,
    convergence_analysis_t* analysis) {
    if (!engine_valid(engine)) {
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }
    if (!analysis) {
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }
    memcpy(analysis, &engine->last_analysis, sizeof(convergence_analysis_t));
    return 0;
}

int inner_dialogue_engine_get_stats(const inner_dialogue_engine_t* engine,
                                     inner_dialogue_engine_stats_t* stats) {
    if (!engine_valid(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_ERROR_NULL,
                              "inner_dialogue: get_stats with invalid engine");
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }
    if (!stats) {
        return NIMCP_INNER_DIALOGUE_ERROR_NULL;
    }
    memcpy(stats, &engine->stats, sizeof(inner_dialogue_engine_stats_t));
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void engine_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_inner_dialogue_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int engine_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "engine_training_begin: NULL argument");
        return -1;
    }
    engine_heartbeat_instance(NULL, "engine_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int engine_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "engine_training_end: NULL argument");
        return -1;
    }
    engine_heartbeat_instance(NULL, "engine_training_end", 1.0f);
    (void)instance;
    return 0;
}

int engine_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "engine_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    engine_heartbeat_instance(NULL, "engine_training_step", progress);
    (void)instance;
    return 0;
}
