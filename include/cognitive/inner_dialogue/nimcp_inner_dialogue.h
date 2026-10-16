/**
 * @file nimcp_inner_dialogue.h
 * @brief Inner Dialogue Engine — Structured Internal Conversation System
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Orchestrates structured multi-perspective internal conversations within
 *        the NIMCP brain, enabling deliberation before action
 * WHY:  Human cognition benefits from internal debate; different brain regions
 *        contribute complementary viewpoints that improve decision quality
 * HOW:  State machine drives turn-taking among registered cognitive perspectives;
 *        convergence detector determines when consensus or termination is reached;
 *        full exception handling, immune integration, BBB validation, and health
 *        monitoring throughout
 *
 * ARCHITECTURE:
 * ```
 *  ┌───────────────────────────────────────────────────────────────────────┐
 *  │                     INNER DIALOGUE ENGINE                             │
 *  │                                                                       │
 *  │  State Machine: IDLE → INITIATED → DELIBERATING → CONVERGING/DONE   │
 *  │                                                                       │
 *  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐            │
 *  │  │Analytical│  │Emotional │  │ Critical │  │ Creative │ ...        │
 *  │  │  (PFC)   │  │(vPFC/Amy)│  │  (ACC)   │  │  (DMN)   │            │
 *  │  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘            │
 *  │       │              │              │              │                  │
 *  │  ┌────▼──────────────▼──────────────▼──────────────▼────┐            │
 *  │  │              Perspective Scheduler                    │            │
 *  │  │         (priority + relevance + fairness)             │            │
 *  │  └────┬─────────────────────────────────────────────────┘            │
 *  │       │                                                              │
 *  │  ┌────▼────────────────────────────────────────────────┐             │
 *  │  │           Turn History (Circular Buffer)             │             │
 *  │  └────┬────────────────────────────────────────────────┘             │
 *  │       │                                                              │
 *  │  ┌────▼────────────────────────────────────────────────┐             │
 *  │  │          Convergence / Deadlock Detector             │             │
 *  │  │    (agreement trend, rumination, emotional spiral)   │             │
 *  │  └──────────────────────────────────────────────────────┘             │
 *  │                                                                       │
 *  │  Integration: Health Heartbeat │ Bio-Async │ Immune │ BBB │ Logging  │
 *  └───────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * BIOLOGICAL BASIS:
 * Internal speech and deliberation engage Broca's area, dorsolateral PFC,
 * anterior cingulate, and medial temporal lobes in a coordinated "inner
 * dialogue" that precedes conscious decision-making.  This module formalises
 * that process as a structured conversation protocol.
 *
 * ERROR CODE RANGE: 29300-29499 (Inner Dialogue Engine module)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_INNER_DIALOGUE_H
#define NIMCP_INNER_DIALOGUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/inner_dialogue/nimcp_inner_dialogue_turn.h"
#include "cognitive/inner_dialogue/nimcp_inner_dialogue_perspective.h"
#include "cognitive/inner_dialogue/nimcp_inner_dialogue_convergence.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;

struct bbb_system_struct;
typedef struct bbb_system_struct* bbb_system_t;

struct brain_cycle_coordinator;
typedef struct brain_cycle_coordinator brain_cycle_coordinator_t;

struct ethics_engine_struct;
typedef struct ethics_engine_struct* ethics_engine_t;

/* ============================================================================
 * Error Codes (Range: 29300-29499)
 * ============================================================================ */

#define NIMCP_INNER_DIALOGUE_ERROR_BASE             29300
#define NIMCP_INNER_DIALOGUE_ERROR_NULL              (NIMCP_INNER_DIALOGUE_ERROR_BASE + 1)
#define NIMCP_INNER_DIALOGUE_ERROR_NO_MEMORY         (NIMCP_INNER_DIALOGUE_ERROR_BASE + 2)
#define NIMCP_INNER_DIALOGUE_ERROR_INVALID_CONFIG     (NIMCP_INNER_DIALOGUE_ERROR_BASE + 3)
#define NIMCP_INNER_DIALOGUE_ERROR_INVALID_STATE      (NIMCP_INNER_DIALOGUE_ERROR_BASE + 4)
#define NIMCP_INNER_DIALOGUE_ERROR_ALREADY_RUNNING    (NIMCP_INNER_DIALOGUE_ERROR_BASE + 5)
#define NIMCP_INNER_DIALOGUE_ERROR_NOT_RUNNING        (NIMCP_INNER_DIALOGUE_ERROR_BASE + 6)
#define NIMCP_INNER_DIALOGUE_ERROR_NO_PERSPECTIVES    (NIMCP_INNER_DIALOGUE_ERROR_BASE + 7)
#define NIMCP_INNER_DIALOGUE_ERROR_TURN_FAILED        (NIMCP_INNER_DIALOGUE_ERROR_BASE + 8)
#define NIMCP_INNER_DIALOGUE_ERROR_CONVERGENCE_CHECK  (NIMCP_INNER_DIALOGUE_ERROR_BASE + 9)
#define NIMCP_INNER_DIALOGUE_ERROR_MUTEX              (NIMCP_INNER_DIALOGUE_ERROR_BASE + 10)
#define NIMCP_INNER_DIALOGUE_ERROR_BBB_REJECTED       (NIMCP_INNER_DIALOGUE_ERROR_BASE + 11)
#define NIMCP_INNER_DIALOGUE_ERROR_IMMUNE_SUPPRESSED  (NIMCP_INNER_DIALOGUE_ERROR_BASE + 12)
#define NIMCP_INNER_DIALOGUE_ERROR_MAX_TURNS          (NIMCP_INNER_DIALOGUE_ERROR_BASE + 13)
#define NIMCP_INNER_DIALOGUE_ERROR_CANCELLED          (NIMCP_INNER_DIALOGUE_ERROR_BASE + 14)
#define NIMCP_INNER_DIALOGUE_ERROR_ETHICS_REJECTED    (NIMCP_INNER_DIALOGUE_ERROR_BASE + 15)
#define NIMCP_INNER_DIALOGUE_ERROR_LGSS_DENIED        (NIMCP_INNER_DIALOGUE_ERROR_BASE + 16)

/* ============================================================================
 * Constants
 * ============================================================================ */

#define INNER_DIALOGUE_VERSION               "1.0.0"
#define INNER_DIALOGUE_MAGIC                 0x494E444C  /**< 'INDL' */
#define INNER_DIALOGUE_MAX_TOPIC_LEN         256
#define INNER_DIALOGUE_DEFAULT_MAX_TURNS     32
#define INNER_DIALOGUE_DEFAULT_URGENCY       0.5f
#define INNER_DIALOGUE_MIN_RELEVANCE_THRESHOLD 0.1f

/* ============================================================================
 * Engine State Machine
 * ============================================================================ */

/**
 * @brief Engine conversation state
 *
 * WHAT: Tracks the lifecycle phase of a conversation
 * WHY:  Guards state transitions, prevents invalid operations
 * HOW:  Linear progression with failure/cancel exit paths
 *
 * State transitions:
 *   IDLE → INITIATED → DELIBERATING → CONVERGING → CONCLUDED
 *                  └→ CANCELLED
 *          DELIBERATING → DEADLOCKED → ESCALATED → CONCLUDED
 *          DELIBERATING → RUMINATING → CONCLUDED
 */
typedef enum {
    DIALOGUE_STATE_IDLE = 0,       /**< No active conversation */
    DIALOGUE_STATE_INITIATED,      /**< Topic set, ready to deliberate */
    DIALOGUE_STATE_DELIBERATING,   /**< Perspectives actively producing turns */
    DIALOGUE_STATE_CONVERGING,     /**< Agreement trending upward */
    DIALOGUE_STATE_DEADLOCKED,     /**< Irreconcilable disagreement detected */
    DIALOGUE_STATE_RUMINATING,     /**< Repetitive pattern detected */
    DIALOGUE_STATE_ESCALATED,      /**< Handed off to executive controller */
    DIALOGUE_STATE_CONCLUDED,      /**< Final conclusion reached */
    DIALOGUE_STATE_CANCELLED,      /**< Externally cancelled */
    DIALOGUE_STATE_COUNT           /**< Sentinel */
} inner_dialogue_state_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Engine configuration
 *
 * WHAT: All tuneable parameters for the inner dialogue engine
 * WHY:  Different cognitive tasks need different dialogue parameters
 */
typedef struct {
    /* Turn limits */
    uint32_t max_turns;                 /**< Maximum turns before forced termination */
    float urgency;                      /**< Time pressure [0-1]; higher = fewer turns */

    /* Relevance gating */
    float min_relevance_threshold;      /**< Perspectives below this are skipped */

    /* Convergence detection */
    convergence_config_t convergence;   /**< Convergence/deadlock thresholds */

    /* Integration flags */
    bool enable_bio_async;              /**< Enable bio-async messaging */
    bool enable_immune_integration;     /**< Report to immune system */
    bool enable_bbb_validation;         /**< Validate turns through BBB */
    bool enable_health_heartbeat;       /**< Register with health monitor */
    bool enable_ethics_evaluation;      /**< Evaluate turns through ethics engine */
    bool enable_lgss_evaluation;        /**< Evaluate turns through LGSS safety KB */
    bool verbose_logging;               /**< Enable debug/trace logging */
} inner_dialogue_config_t;

/**
 * @brief Get default engine configuration
 *
 * @return Configuration with sensible defaults
 */
inner_dialogue_config_t inner_dialogue_default_config(void);

/* ============================================================================
 * Conversation Result
 * ============================================================================ */

/**
 * @brief Outcome of a completed or terminated conversation
 *
 * WHAT: Full summary of how the dialogue went
 * WHY:  Learning subsystems need outcome data; executive needs conclusion
 */
typedef struct {
    /* Conclusion */
    inner_dialogue_turn_t conclusion;           /**< Final synthesised turn */
    bool has_conclusion;                        /**< True if a conclude act was produced */

    /* Termination */
    termination_reason_t termination_reason;    /**< Why conversation ended */
    inner_dialogue_state_t final_state;         /**< State at termination */

    /* Metrics */
    uint32_t total_turns;                       /**< Turns actually taken */
    uint32_t perspectives_participated;         /**< How many perspectives contributed */
    float final_agreement;                      /**< Agreement score at end */
    float avg_confidence;                       /**< Mean confidence across turns */
    float avg_novelty;                          /**< Mean novelty across turns */
    float total_formulation_time_ms;            /**< Sum of formulation times */

    /* Convergence */
    convergence_analysis_t final_analysis;      /**< Last convergence analysis */
} inner_dialogue_result_t;

/* ============================================================================
 * Engine Statistics
 * ============================================================================ */

/**
 * @brief Cumulative engine statistics across all conversations
 */
typedef struct {
    uint64_t conversations_started;
    uint64_t conversations_completed;
    uint64_t conversations_deadlocked;
    uint64_t conversations_ruminated;
    uint64_t conversations_escalated;
    uint64_t conversations_cancelled;
    uint64_t total_turns_produced;
    uint64_t immune_reports_sent;
    uint64_t bbb_rejections;
    uint64_t ethics_rejections;             /**< Turns rejected by ethics engine */
    uint64_t lgss_denials;                  /**< Turns denied by LGSS safety KB */
    uint64_t bio_messages_sent;
    float avg_turns_to_convergence;
} inner_dialogue_engine_stats_t;

/* ============================================================================
 * Engine Opaque Type
 * ============================================================================ */

/**
 * @brief Inner dialogue engine instance
 *
 * WHAT: Opaque engine handle
 * WHY:  Encapsulate internal state; single engine per brain
 */
typedef struct inner_dialogue_engine inner_dialogue_engine_t;

/* ============================================================================
 * Engine Lifecycle API
 * ============================================================================ */

/**
 * @brief Create inner dialogue engine
 *
 * WHAT: Allocate and initialise the engine with given configuration
 * WHY:  Brain needs one engine to orchestrate internal conversations
 * HOW:  Allocates engine, perspective registry, turn history; registers health heartbeat
 *
 * EXCEPTION: Reports NIMCP_INNER_DIALOGUE_ERROR_NO_MEMORY to immune system on failure
 *
 * @param config Engine configuration (NULL = use defaults)
 * @return New engine, or NULL on failure
 */
inner_dialogue_engine_t* inner_dialogue_engine_create(
    const inner_dialogue_config_t* config);

/**
 * @brief Destroy engine and free all resources
 *
 * WHAT: Clean shutdown of engine
 * WHY:  Prevent memory/resource leaks
 * HOW:  Cancel active conversation, cleanup sub-structures, free memory
 *
 * @param engine Engine to destroy (NULL-safe)
 */
void inner_dialogue_engine_destroy(inner_dialogue_engine_t* engine);

/**
 * @brief Reset engine to initial state
 *
 * @param engine Engine to reset
 * @return 0 on success, error code on failure
 */
int inner_dialogue_engine_reset(inner_dialogue_engine_t* engine);

/* ============================================================================
 * Integration API — External System Connections
 * ============================================================================ */

/**
 * @brief Connect engine to health monitoring agent
 *
 * @param engine Engine
 * @param agent  Health agent for heartbeat reporting
 * @return 0 on success, error code on failure
 */
int inner_dialogue_engine_set_health_agent(inner_dialogue_engine_t* engine,
                                            nimcp_health_agent_t* agent);

/**
 * @brief Connect engine to brain immune system
 *
 * @param engine Engine
 * @param immune Immune system for exception reporting
 * @return 0 on success, error code on failure
 */
int inner_dialogue_engine_set_immune(inner_dialogue_engine_t* engine,
                                      brain_immune_system_t* immune);

/**
 * @brief Connect engine to blood-brain barrier
 *
 * @param engine Engine
 * @param bbb    BBB system for content validation
 * @return 0 on success, error code on failure
 */
int inner_dialogue_engine_set_bbb(inner_dialogue_engine_t* engine,
                                    bbb_system_t bbb);

/**
 * @brief Connect engine to bio-async router
 *
 * @param engine Engine
 * @param router Bio-async router handle
 * @return 0 on success, error code on failure
 */
int inner_dialogue_engine_set_bio_router(inner_dialogue_engine_t* engine,
                                          void* router);

/**
 * @brief Connect engine to brain cycle coordinator
 *
 * WHAT: Integrate with the brain cycle coordinator for health-aware deliberation
 * WHY:  Engine modulates urgency and can suspend when brain cycles are degraded
 * HOW:  Registers a health callback; queries coordinator health during step()
 *
 * @param engine Engine
 * @param coordinator Brain cycle coordinator handle
 * @return 0 on success, error code on failure
 */
int inner_dialogue_engine_set_cycle_coordinator(inner_dialogue_engine_t* engine,
                                                 brain_cycle_coordinator_t* coordinator);

/**
 * @brief Connect engine to ethics evaluation engine
 *
 * WHAT: Integrate with the ethics engine for moral evaluation of dialogue turns
 * WHY:  Deliberation conclusions must align with ethical standards; harmful or
 *        deceptive formulations are rejected before being recorded
 * HOW:  Each turn's content is submitted as an action_context_t; turns with
 *        ethics_action != ALLOW are discarded and the perspective is notified
 *
 * @param engine Engine
 * @param ethics Ethics engine handle (NULL to disconnect)
 * @return 0 on success, error code on failure
 */
int inner_dialogue_engine_set_ethics(inner_dialogue_engine_t* engine,
                                      ethics_engine_t ethics);

/**
 * @brief Connect engine to LGSS safety knowledge base
 *
 * WHAT: Integrate with the Logical Guardrails Safety Schema for L0 safety checks
 * WHY:  LGSS provides the highest-priority safety layer (above ethics); dialogue
 *        content that triggers DENY rules is unconditionally blocked
 * HOW:  Turn content is submitted as a safety_action_context_t; LGSS is evaluated
 *        BEFORE ethics (defense in depth: L0 LGSS → L3 ethics → BBB → record)
 *
 * @param engine Engine
 * @param lgss_kb Const pointer to locked safety KB (const safety_kb_t*), or NULL to disconnect
 * @return 0 on success, error code on failure
 */
int inner_dialogue_engine_set_lgss(inner_dialogue_engine_t* engine,
                                     const void* lgss_kb);

/**
 * @brief Set global health agent for static helper functions
 *
 * @param agent Health agent (NULL to disable)
 */
void inner_dialogue_set_health_agent_global(nimcp_health_agent_t* agent);

/* ============================================================================
 * Perspective Management API
 * ============================================================================ */

/**
 * @brief Get mutable perspective registry
 *
 * WHAT: Access the engine's perspective registry for registration
 * WHY:  External code registers perspectives before starting conversations
 *
 * @param engine Engine
 * @return Pointer to registry, or NULL on error
 */
inner_dialogue_perspective_registry_t* inner_dialogue_engine_get_registry(
    inner_dialogue_engine_t* engine);

/**
 * @brief Connect brain to inner dialogue for perspective module access
 */
int inner_dialogue_engine_connect_brain(inner_dialogue_engine_t* engine, void* brain);

/**
 * @brief Register built-in perspectives with default stub callbacks
 *
 * @param engine Engine
 * @return 0 on success, error code on failure
 */
int inner_dialogue_engine_register_builtins(inner_dialogue_engine_t* engine);

/* ============================================================================
 * Conversation API
 * ============================================================================ */

/**
 * @brief Start a new conversation on the given topic
 *
 * WHAT: Transition engine from IDLE to INITIATED, prepare for deliberation
 * WHY:  Every deliberation begins with a topic
 * HOW:  Validate state, set topic, reset turn history, send bio-async start msg
 *
 * @param engine Engine
 * @param topic  Conversation topic string (copied internally)
 * @return 0 on success, error code on failure
 */
int inner_dialogue_engine_start(inner_dialogue_engine_t* engine,
                                 const char* topic);

/**
 * @brief Execute one deliberation turn
 *
 * WHAT: Select next perspective, formulate turn, validate, record, check convergence
 * WHY:  Fine-grained control — caller drives the turn loop
 * HOW:
 *   1. Guard: must be in DELIBERATING or INITIATED state
 *   2. Query cycle coordinator health (modulate urgency / suppress if degraded)
 *   3. Select highest-priority perspective
 *   4. Call perspective's formulate()
 *   5. LGSS safety check (L0 — highest priority guardrail)
 *   6. Ethics evaluation (moral alignment — Golden Rule + violation checks)
 *   7. BBB validate turn content (content safety)
 *   8. Record turn in history
 *   9. Notify all perspectives via observe()
 *  10. Send bio-async turn event
 *  11. Run convergence analysis
 *  12. Update state machine based on analysis
 *  10. Health heartbeat
 *
 * @param engine Engine
 * @param turn_out Optional: output the turn that was produced (can be NULL)
 * @return 0 if turn completed (more turns possible),
 *         positive value if conversation should end (see termination_reason_t),
 *         negative on error
 */
int inner_dialogue_engine_step(inner_dialogue_engine_t* engine,
                                inner_dialogue_turn_t* turn_out);

/**
 * @brief Run full conversation to completion
 *
 * WHAT: Call step() in a loop until convergence, max turns, or termination
 * WHY:  Convenience API for synchronous deliberation
 * HOW:  Loop calling step() with health heartbeat on each iteration
 *
 * @param engine Engine
 * @param result Output: conversation result (must be pre-allocated)
 * @return 0 on success (converged), positive = termination reason, negative = error
 */
int inner_dialogue_engine_run(inner_dialogue_engine_t* engine,
                               inner_dialogue_result_t* result);

/**
 * @brief Cancel the current conversation
 *
 * @param engine Engine
 * @return 0 on success, error code if no active conversation
 */
int inner_dialogue_engine_cancel(inner_dialogue_engine_t* engine);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current engine state
 *
 * @param engine Engine
 * @return Current state, or DIALOGUE_STATE_IDLE if engine is NULL
 */
inner_dialogue_state_t inner_dialogue_engine_get_state(
    const inner_dialogue_engine_t* engine);

/**
 * @brief Get the current conversation's turn history
 *
 * @param engine Engine
 * @return Pointer to history (read-only), or NULL
 */
const inner_dialogue_turn_history_t* inner_dialogue_engine_get_history(
    const inner_dialogue_engine_t* engine);

/**
 * @brief Get current topic
 *
 * @param engine Engine
 * @return Topic string, or NULL if no active conversation
 */
const char* inner_dialogue_engine_get_topic(
    const inner_dialogue_engine_t* engine);

/**
 * @brief Get current turn number
 *
 * @param engine Engine
 * @return Current turn number, or 0 if no active conversation
 */
uint32_t inner_dialogue_engine_get_turn_number(
    const inner_dialogue_engine_t* engine);

/**
 * @brief Get last convergence analysis
 *
 * @param engine Engine
 * @param analysis Output analysis
 * @return 0 on success, error code on failure
 */
int inner_dialogue_engine_get_convergence(
    const inner_dialogue_engine_t* engine,
    convergence_analysis_t* analysis);

/**
 * @brief Get engine statistics
 *
 * @param engine Engine
 * @param stats  Output statistics
 * @return 0 on success, error code on failure
 */
int inner_dialogue_engine_get_stats(const inner_dialogue_engine_t* engine,
                                     inner_dialogue_engine_stats_t* stats);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert engine state to string
 *
 * @param state Engine state
 * @return Static string, or "UNKNOWN"
 */
const char* inner_dialogue_state_to_string(inner_dialogue_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INNER_DIALOGUE_H */
