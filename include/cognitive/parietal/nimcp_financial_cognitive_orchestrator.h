/**
 * @file nimcp_financial_cognitive_orchestrator.h
 * @brief Financial Cognitive Orchestrator - Master Integration (Phase 10)
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Master orchestrator that ties all financial bridges together into a
 *       unified cognitive financial processing system. Coordinates memory,
 *       emotion, attention, decision-making, ethics, learning, and metacognition.
 *
 * WHY:  Financial decision-making requires holistic integration of multiple
 *       cognitive systems working in concert:
 *       - Memory systems (working memory, episodic, autobiographical)
 *       - Emotional processing (Plutchik, motivation, mental health)
 *       - Attention (salience, emotional attention)
 *       - Decision-making (basal ganglia, predictive, reasoning, JEPA)
 *       - Ethics and explainability
 *       - Learning (consolidation, STDP, temporal credit)
 *       - Metacognition (uncertainty, curiosity, regret)
 *       The orchestrator ensures these systems communicate and coordinate.
 *
 * HOW:  Pipeline-based processing where market data flows through:
 *       1. Perception -> encoding and working memory
 *       2. Emotion -> appraisal and state update
 *       3. Attention -> salience filtering
 *       4. Cognition -> world model, ToM, reasoning
 *       5. Decision -> basal ganglia action selection
 *       6. Ethics -> validation and explanation
 *       7. Learning -> consolidation and credit assignment
 *       8. Metacognition -> bias detection and confidence calibration
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                 Financial Cognitive Orchestrator                          |
 * +===========================================================================+
 * |                                                                           |
 * |  +-----------------------------------------------------------------------+
 * |  |                        Core Financial Modules                         |
 * |  +-----------------------------------------------------------------------+
 * |  | Investment | Market | Bridge | Neural | Archetype                     |
 * |  +-----------------------------------------------------------------------+
 * |                                                                           |
 * |  +------------------------+  +------------------------+                  |
 * |  |    Memory Bridges      |  |   Cognitive Bridges    |                  |
 * |  +------------------------+  +------------------------+                  |
 * |  | Working Memory         |  | World Model            |                  |
 * |  | Mammillary             |  | Theory of Mind         |                  |
 * |  | Resonance              |  +------------------------+                  |
 * |  | Autobiographical       |                                               |
 * |  +------------------------+  +------------------------+                  |
 * |                              |   Emotion Bridges      |                  |
 * |  +------------------------+  +------------------------+                  |
 * |  |   Attention Bridges    |  | Emotion                |                  |
 * |  +------------------------+  | Motivation             |                  |
 * |  | Salience               |  | Neuromodulation        |                  |
 * |  | Emotional Attention    |  | Mental Health          |                  |
 * |  +------------------------+  +------------------------+                  |
 * |                                                                           |
 * |  +------------------------+  +------------------------+                  |
 * |  |   Decision Bridges     |  |   Ethics Bridges       |                  |
 * |  +------------------------+  +------------------------+                  |
 * |  | Basal Ganglia          |  | Ethics                 |                  |
 * |  | Predictive             |  | Explanations           |                  |
 * |  | Reasoning              |  +------------------------+                  |
 * |  | JEPA                   |                                               |
 * |  +------------------------+  +------------------------+                  |
 * |                              |   Learning Bridges     |                  |
 * |  +------------------------+  +------------------------+                  |
 * |  | Metacognition Bridges  |  | Consolidation          |                  |
 * |  +------------------------+  | STDP                   |                  |
 * |  | Metacognition          |  | Temporal Credit        |                  |
 * |  | Uncertainty            |  +------------------------+                  |
 * |  | Curiosity              |                                               |
 * |  | Regret                 |  +------------------------+                  |
 * |  +------------------------+  | Fuzzy Logic            |                  |
 * |                              +------------------------+                  |
 * +===========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_COGNITIVE_ORCHESTRATOR_H
#define NIMCP_FINANCIAL_COGNITIVE_ORCHESTRATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_COGNITIVE_ORCHESTRATOR_VERSION    "1.0.0"
#define FINANCIAL_COGNITIVE_ORCHESTRATOR_MAGIC      0x46434F52  /* 'FCOR' */

/** Bio-async module ID for financial cognitive orchestrator */
#define BIO_MODULE_FINANCIAL_COGNITIVE_ORCHESTRATOR 0x03A0

/** Maximum assets in market data */
#define FIN_ORCH_MAX_ASSETS                         512

/** Maximum pending decisions */
#define FIN_ORCH_MAX_PENDING_DECISIONS              64

/** Maximum explanation length */
#define FIN_ORCH_EXPLANATION_LEN                    2048

/** Maximum summary length */
#define FIN_ORCH_SUMMARY_LEN                        1024

/** Maximum reasoning length */
#define FIN_ORCH_REASONING_LEN                      2048

/** Maximum asset symbol length */
#define FIN_ORCH_SYMBOL_LEN                         32

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_ORCH_ERROR_BASE                         34500
#define FIN_ORCH_ERR_OK                             0
#define FIN_ORCH_ERR_NULL                           (FIN_ORCH_ERROR_BASE + 1)
#define FIN_ORCH_ERR_INVALID_PARAM                  (FIN_ORCH_ERROR_BASE + 2)
#define FIN_ORCH_ERR_NO_MEMORY                      (FIN_ORCH_ERROR_BASE + 3)
#define FIN_ORCH_ERR_STATE                          (FIN_ORCH_ERROR_BASE + 4)
#define FIN_ORCH_ERR_IMMUNE                         (FIN_ORCH_ERROR_BASE + 5)
#define FIN_ORCH_ERR_BBB                            (FIN_ORCH_ERROR_BASE + 6)
#define FIN_ORCH_ERR_SUBSYSTEM                      (FIN_ORCH_ERROR_BASE + 7)
#define FIN_ORCH_ERR_PIPELINE                       (FIN_ORCH_ERROR_BASE + 8)
#define FIN_ORCH_ERR_DECISION                       (FIN_ORCH_ERROR_BASE + 9)
#define FIN_ORCH_ERR_LEARNING                       (FIN_ORCH_ERROR_BASE + 10)
#define FIN_ORCH_ERR_CONSOLIDATION                  (FIN_ORCH_ERROR_BASE + 11)
#define FIN_ORCH_ERR_ETHICS                         (FIN_ORCH_ERROR_BASE + 12)
#define FIN_ORCH_ERR_NOT_READY                      (FIN_ORCH_ERROR_BASE + 13)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Orchestrator operational state
 */
typedef enum {
    FIN_ORCH_STATE_UNINITIALIZED = 0,
    FIN_ORCH_STATE_INITIALIZED,
    FIN_ORCH_STATE_READY,
    FIN_ORCH_STATE_PROCESSING,
    FIN_ORCH_STATE_DECIDING,
    FIN_ORCH_STATE_LEARNING,
    FIN_ORCH_STATE_CONSOLIDATING,
    FIN_ORCH_STATE_DEGRADED,
    FIN_ORCH_STATE_ERROR
} fin_orchestrator_state_t;

/**
 * @brief Decision types (guarded to avoid redefinition)
 */
#ifndef FIN_DECISION_TYPE_DEFINED
#define FIN_DECISION_TYPE_DEFINED
typedef enum {
    FIN_DECISION_BUY = 0,
    FIN_DECISION_SELL,
    FIN_DECISION_HOLD,
    FIN_DECISION_SHORT,
    FIN_DECISION_COVER,
    FIN_DECISION_REBALANCE,
    FIN_DECISION_HEDGE,
    FIN_DECISION_EXIT,
    FIN_DECISION_WAIT,
    FIN_DECISION_GATHER_INFO,
    FIN_DECISION_COUNT
} fin_decision_type_t;
#endif /* FIN_DECISION_TYPE_DEFINED */

/**
 * @brief Pipeline stages for tracking
 */
typedef enum {
    FIN_PIPELINE_PERCEPTION = 0,
    FIN_PIPELINE_WORKING_MEMORY,
    FIN_PIPELINE_EMOTION,
    FIN_PIPELINE_ATTENTION,
    FIN_PIPELINE_COGNITION,
    FIN_PIPELINE_DECISION,
    FIN_PIPELINE_ETHICS,
    FIN_PIPELINE_LEARNING,
    FIN_PIPELINE_METACOGNITION,
    FIN_PIPELINE_STAGE_COUNT
} fin_pipeline_stage_t;

/**
 * @brief Trade outcome for learning
 */
typedef enum {
    FIN_OUTCOME_PROFIT = 1,
    FIN_OUTCOME_LOSS = -1,
    FIN_OUTCOME_BREAKEVEN = 0,
    FIN_OUTCOME_PENDING = 2
} fin_trade_outcome_t;

/* ============================================================================
 * Core Data Structures (as specified)
 * ============================================================================ */

/**
 * @brief Financial cognitive orchestrator structure
 *
 * Master structure holding all financial bridge pointers for
 * coordinated cognitive processing.
 */
typedef struct {
    /* Core financial modules (existing) */
    void* investment;           /**< financial_investment_eng_t* */
    void* market;               /**< financial_market_eng_t* */
    void* bridge;               /**< financial_bridge_t* */
    void* neural;               /**< financial_neural_bridge_t* */
    void* archetype;            /**< financial_investor_archetype_t* */

    /* Memory bridges */
    void* working_memory;       /**< financial_wm_bridge_t* */
    void* mammillary;           /**< financial_mammillary_bridge_t* */
    void* resonance;            /**< financial_resonance_bridge_t* */
    void* autobio;              /**< financial_autobio_bridge_t* */

    /* Cognitive bridges */
    void* world_model;          /**< financial_world_model_bridge_t* */
    void* tom;                  /**< financial_tom_bridge_t* */

    /* Emotion bridges */
    void* emotion;              /**< financial_emotion_bridge_t* */
    void* motivation;           /**< financial_motivation_bridge_t* */
    void* neuromod;             /**< financial_neuromod_bridge_t* */
    void* mental_health;        /**< financial_mental_health_bridge_t* */

    /* Attention bridges */
    void* salience;             /**< financial_salience_bridge_t* */
    void* emo_attention;        /**< financial_emo_attention_bridge_t* */

    /* Decision bridges */
    void* basal_ganglia;        /**< financial_bg_bridge_t* */
    void* predictive;           /**< financial_predictive_bridge_t* */
    void* reasoning;            /**< financial_reasoning_bridge_t* */
    void* jepa;                 /**< financial_jepa_bridge_t* */

    /* Ethics bridges */
    void* ethics;               /**< financial_ethics_bridge_t* */
    void* explanations;         /**< financial_explanations_bridge_t* */

    /* Learning bridges */
    void* consolidation;        /**< financial_consolidation_bridge_t* */
    void* stdp;                 /**< financial_stdp_bridge_t* */
    void* temporal_credit;      /**< financial_temporal_credit_bridge_t* */

    /* Metacognition bridges */
    void* metacognition;        /**< financial_metacognition_bridge_t* */
    void* uncertainty;          /**< financial_uncertainty_bridge_t* */
    void* curiosity;            /**< financial_curiosity_bridge_t* */
    void* regret;               /**< financial_regret_bridge_t* */

    /* Fuzzy logic */
    void* fuzzy;                /**< fuzzy_bridge_t* */
} financial_cognitive_orchestrator_t;

/**
 * @brief Market data input structure
 */
typedef struct {
    float* prices;              /**< Array of asset prices */
    float* volumes;             /**< Array of asset volumes */
    uint32_t num_assets;        /**< Number of assets */
    uint64_t timestamp_ms;      /**< Timestamp in milliseconds */
} fin_market_data_t;

/**
 * @brief Cognitive decision output structure
 */
typedef struct {
    int decision_type;          /**< fin_decision_type_t value */
    float magnitude;            /**< Decision magnitude (0-1 for position sizing) */
    char asset[32];             /**< Asset symbol */
    float confidence;           /**< Decision confidence [0-1] */
} fin_cognitive_decision_t;

/**
 * @brief Cognitive explanation structure
 */
typedef struct {
    char summary[1024];         /**< Human-readable summary */
    char reasoning[2048];       /**< Detailed reasoning chain */
    float confidence;           /**< Explanation confidence [0-1] */
} fin_cognitive_explanation_t;

/**
 * @brief Orchestrator statistics
 */
typedef struct {
    uint64_t market_data_processed;   /**< Total market data batches processed */
    uint64_t decisions_made;          /**< Total decisions made */
    uint64_t learning_cycles;         /**< Total learning cycles */
    uint64_t consolidations;          /**< Memory consolidation cycles */
    uint64_t immune_checks;           /**< Immune system validation calls */
    uint64_t bbb_validations;         /**< BBB validation calls */
    uint64_t kg_messages_sent;        /**< KG messages published */
    uint64_t health_heartbeats;       /**< Health heartbeats sent */
} fin_orchestrator_stats_t;

/* ============================================================================
 * Extended Data Structures
 * ============================================================================ */

/**
 * @brief Trade outcome for learning
 */
typedef struct {
    char asset[FIN_ORCH_SYMBOL_LEN];  /**< Asset traded */
    fin_decision_type_t decision;      /**< Original decision */
    float entry_price;                 /**< Entry price */
    float exit_price;                  /**< Exit price */
    float quantity;                    /**< Position size */
    float pnl;                         /**< Profit/loss */
    float return_pct;                  /**< Return percentage */
    fin_trade_outcome_t outcome;       /**< Outcome category */
    uint64_t entry_time_ms;            /**< Entry timestamp */
    uint64_t exit_time_ms;             /**< Exit timestamp */
    float original_confidence;         /**< Confidence at decision time */
} fin_trade_outcome_record_t;

/**
 * @brief Pipeline processing result
 */
typedef struct {
    bool stage_completed[FIN_PIPELINE_STAGE_COUNT]; /**< Stage completion flags */
    float stage_times_us[FIN_PIPELINE_STAGE_COUNT]; /**< Stage processing times */
    float total_time_us;                             /**< Total processing time */
    uint32_t working_memory_items;                   /**< Items in working memory */
    float emotional_state_magnitude;                 /**< Emotional intensity */
    float attention_focus;                           /**< Attention focus level */
    float metacognitive_confidence;                  /**< Metacognitive confidence */
    bool ethics_approved;                            /**< Ethics validation passed */
    char stage_notes[FIN_PIPELINE_STAGE_COUNT][256]; /**< Stage notes */
} fin_pipeline_result_t;

/**
 * @brief Detailed decision result
 */
typedef struct {
    fin_cognitive_decision_t decision;   /**< Core decision */
    fin_cognitive_explanation_t explanation; /**< Explanation */
    fin_pipeline_result_t pipeline;      /**< Pipeline details */

    /* Contributing factors */
    float emotion_influence;             /**< Emotion contribution [0-1] */
    float reasoning_influence;           /**< Reasoning contribution [0-1] */
    float intuition_influence;           /**< Intuition contribution [0-1] */
    float world_model_confidence;        /**< World model prediction confidence */
    float tom_prediction_confidence;     /**< ToM prediction confidence */

    /* Risk assessment */
    float estimated_risk;                /**< Estimated risk [0-1] */
    float uncertainty_epistemic;         /**< Epistemic uncertainty [0-1] */
    float uncertainty_aleatoric;         /**< Aleatoric uncertainty [0-1] */

    /* Metacognitive assessment */
    uint32_t biases_detected;            /**< Number of biases detected */
    bool reconsideration_suggested;      /**< Should reconsider? */
    float calibration_score;             /**< Confidence calibration score */
} fin_detailed_decision_t;

/**
 * @brief Consolidation session result
 */
typedef struct {
    uint32_t patterns_replayed;          /**< Patterns replayed */
    uint32_t patterns_strengthened;      /**< Patterns strengthened */
    uint32_t patterns_pruned;            /**< Patterns pruned */
    float total_strengthening;           /**< Total strength increase */
    float total_weakening;               /**< Total strength decrease */
    uint64_t duration_ms;                /**< Consolidation duration */
} fin_consolidation_session_result_t;

/**
 * @brief Learning result from trade outcome
 */
typedef struct {
    float reward_signal;                 /**< STDP reward signal */
    float temporal_credit;               /**< Temporal credit assigned */
    float pattern_strength_delta;        /**< Pattern strength change */
    float regret_magnitude;              /**< Regret if loss */
    char lesson_learned[512];            /**< Extracted lesson */
    bool pattern_updated;                /**< Was a pattern updated? */
} fin_learning_result_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Orchestrator configuration
 */
typedef struct {
    /* Pipeline settings */
    bool enable_working_memory;          /**< Use working memory stage */
    bool enable_emotion_processing;      /**< Use emotion processing */
    bool enable_attention_filtering;     /**< Use attention filtering */
    bool enable_world_model;             /**< Use world model prediction */
    bool enable_tom;                     /**< Use theory of mind */
    bool enable_ethics_validation;       /**< Use ethics validation */
    bool enable_metacognition;           /**< Use metacognitive monitoring */
    bool enable_learning;                /**< Enable learning from outcomes */
    bool enable_consolidation;           /**< Enable periodic consolidation */

    /* Integration settings */
    bool enable_immune_integration;      /**< Enable immune system checks */
    bool enable_bbb_validation;          /**< Enable BBB validation */
    bool enable_kg_messaging;            /**< Enable KG messaging */
    bool enable_health_monitoring;       /**< Enable health heartbeats */
    bool enable_fuzzy_logic;             /**< Enable fuzzy logic integration */

    /* Decision settings */
    float min_confidence_threshold;      /**< Minimum confidence for decision */
    float ethics_veto_threshold;         /**< Ethics score below which to veto */
    float metacog_reconsider_threshold;  /**< Bias strength to trigger reconsider */
    uint32_t max_decisions_per_cycle;    /**< Max decisions per processing cycle */

    /* Learning settings */
    float learning_rate;                 /**< Base learning rate */
    float temporal_discount;             /**< Temporal credit discount factor */
    uint64_t consolidation_interval_ms;  /**< Consolidation interval */

    /* Memory settings */
    uint32_t working_memory_capacity;    /**< Working memory slots */
    float working_memory_decay_rate;     /**< WM decay rate per second */

    /* Logging */
    bool verbose_logging;                /**< Verbose debug output */
} fin_orchestrator_config_t;

/* ============================================================================
 * Forward Declarations for Subsystems
 * ============================================================================ */

#ifndef BBB_SYSTEM_T_DEFINED
#define BBB_SYSTEM_T_DEFINED
typedef struct bbb_system_struct* bbb_system_t;
#endif

#ifndef ETHICS_ENGINE_T_DEFINED
#define ETHICS_ENGINE_T_DEFINED
typedef struct ethics_engine_struct* ethics_engine_t;
#endif

#ifndef BRAIN_CYCLE_COORDINATOR_T_DEFINED
#define BRAIN_CYCLE_COORDINATOR_T_DEFINED
typedef struct brain_cycle_coordinator brain_cycle_coordinator_t;
#endif

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/**
 * @brief Opaque orchestrator handle
 *
 * Internal structure holds the financial_cognitive_orchestrator_t and
 * additional state for coordinated processing.
 */
typedef struct financial_cognitive_orchestrator_internal financial_cognitive_orchestrator_handle_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_default_config(fin_orchestrator_config_t* config);

/**
 * @brief Create financial cognitive orchestrator
 *
 * @param config Configuration (NULL for defaults)
 * @return Orchestrator handle or NULL on failure
 */
financial_cognitive_orchestrator_handle_t* financial_cognitive_orchestrator_create(
    const fin_orchestrator_config_t* config
);

/**
 * @brief Destroy financial cognitive orchestrator
 *
 * @param orch Orchestrator handle (NULL safe)
 */
void financial_cognitive_orchestrator_destroy(
    financial_cognitive_orchestrator_handle_t* orch
);

/**
 * @brief Reset orchestrator state
 *
 * @param orch Orchestrator handle
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_reset(
    financial_cognitive_orchestrator_handle_t* orch
);

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

/**
 * @brief Get internal orchestrator structure for module registration
 *
 * Returns pointer to the internal financial_cognitive_orchestrator_t
 * for setting module pointers.
 *
 * @param orch Orchestrator handle
 * @return Pointer to internal structure, or NULL if orch is NULL
 */
financial_cognitive_orchestrator_t* financial_cognitive_orchestrator_get_modules(
    financial_cognitive_orchestrator_handle_t* orch
);

/**
 * @brief Register all modules at once
 *
 * Convenience function to register all modules from an existing
 * financial_cognitive_orchestrator_t structure.
 *
 * @param orch Orchestrator handle
 * @param modules Source module structure
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_register_all(
    financial_cognitive_orchestrator_handle_t* orch,
    const financial_cognitive_orchestrator_t* modules
);

/**
 * @brief Validate all required modules are registered
 *
 * @param orch Orchestrator handle
 * @return 0 if all required modules present, error code otherwise
 */
int financial_cognitive_orchestrator_validate_modules(
    const financial_cognitive_orchestrator_handle_t* orch
);

/* ============================================================================
 * Subsystem Setters (Integration Infrastructure)
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_cognitive_orchestrator_set_immune(
    financial_cognitive_orchestrator_handle_t* orch,
    void* immune
);

/**
 * @brief Set BBB system handle
 */
int financial_cognitive_orchestrator_set_bbb(
    financial_cognitive_orchestrator_handle_t* orch,
    bbb_system_t bbb
);

/**
 * @brief Set health agent handle
 */
int financial_cognitive_orchestrator_set_health_agent(
    financial_cognitive_orchestrator_handle_t* orch,
    void* health_agent
);

/**
 * @brief Set KG wiring handle
 */
int financial_cognitive_orchestrator_set_kg_wiring(
    financial_cognitive_orchestrator_handle_t* orch,
    void* kg_wiring
);

/**
 * @brief Set logger handle
 */
int financial_cognitive_orchestrator_set_logger(
    financial_cognitive_orchestrator_handle_t* orch,
    void* logger
);

/**
 * @brief Set security handle
 */
int financial_cognitive_orchestrator_set_security(
    financial_cognitive_orchestrator_handle_t* orch,
    void* security
);

/**
 * @brief Set ethics engine handle
 */
int financial_cognitive_orchestrator_set_ethics_engine(
    financial_cognitive_orchestrator_handle_t* orch,
    ethics_engine_t ethics
);

/**
 * @brief Set LGSS handle
 */
int financial_cognitive_orchestrator_set_lgss(
    financial_cognitive_orchestrator_handle_t* orch,
    const void* lgss
);

/**
 * @brief Set cycle coordinator handle
 */
int financial_cognitive_orchestrator_set_coordinator(
    financial_cognitive_orchestrator_handle_t* orch,
    brain_cycle_coordinator_t* coordinator
);

/**
 * @brief Set bio router handle
 */
int financial_cognitive_orchestrator_set_bio_router(
    financial_cognitive_orchestrator_handle_t* orch,
    void* bio_router
);

/* ============================================================================
 * Core Pipeline API
 * ============================================================================ */

/**
 * @brief Process market data through full cognitive pipeline
 *
 * Runs market data through all enabled pipeline stages:
 * perception, working memory, emotion, attention, cognition,
 * decision, ethics, learning, metacognition.
 *
 * @param orch Orchestrator handle
 * @param data Market data to process
 * @param result Output pipeline result (optional, can be NULL)
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_process_market_data(
    financial_cognitive_orchestrator_handle_t* orch,
    const fin_market_data_t* data,
    fin_pipeline_result_t* result
);

/**
 * @brief Make cognitively-informed financial decision
 *
 * Uses all registered cognitive systems to make a decision
 * about a specific asset. Returns detailed decision with
 * explanation and contributing factors.
 *
 * @param orch Orchestrator handle
 * @param asset Asset symbol to decide on
 * @param result Output detailed decision
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_make_decision(
    financial_cognitive_orchestrator_handle_t* orch,
    const char* asset,
    fin_detailed_decision_t* result
);

/**
 * @brief Learn from trade outcome
 *
 * Updates all learning systems based on trade outcome:
 * - STDP reward signal
 * - Temporal credit assignment
 * - Pattern strength adjustment
 * - Regret analysis
 * - Lesson extraction
 *
 * @param orch Orchestrator handle
 * @param outcome Trade outcome record
 * @param result Output learning result (optional, can be NULL)
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_learn_from_outcome(
    financial_cognitive_orchestrator_handle_t* orch,
    const fin_trade_outcome_record_t* outcome,
    fin_learning_result_t* result
);

/**
 * @brief Consolidate learning
 *
 * Triggers memory consolidation cycle:
 * - Replay profitable patterns
 * - Strengthen winning patterns
 * - Prune losing patterns
 * - Update pattern strengths
 *
 * @param orch Orchestrator handle
 * @param result Output consolidation result (optional, can be NULL)
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_consolidate(
    financial_cognitive_orchestrator_handle_t* orch,
    fin_consolidation_session_result_t* result
);

/* ============================================================================
 * Extended API
 * ============================================================================ */

/**
 * @brief Get current emotional state
 *
 * @param orch Orchestrator handle
 * @param state Output emotional state (void* for flexibility)
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_get_emotional_state(
    const financial_cognitive_orchestrator_handle_t* orch,
    void* state
);

/**
 * @brief Get metacognitive assessment
 *
 * @param orch Orchestrator handle
 * @param assessment Output assessment (void* for flexibility)
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_get_metacognitive_assessment(
    const financial_cognitive_orchestrator_handle_t* orch,
    void* assessment
);

/**
 * @brief Get world model prediction
 *
 * @param orch Orchestrator handle
 * @param asset Asset to predict
 * @param horizon_steps Prediction horizon
 * @param prediction Output prediction (void* for flexibility)
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_predict(
    financial_cognitive_orchestrator_handle_t* orch,
    const char* asset,
    uint32_t horizon_steps,
    void* prediction
);

/**
 * @brief Run ethical validation on action
 *
 * @param orch Orchestrator handle
 * @param decision Decision to validate
 * @param approved Output: whether approved
 * @param explanation Output explanation
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_validate_ethics(
    financial_cognitive_orchestrator_handle_t* orch,
    const fin_cognitive_decision_t* decision,
    bool* approved,
    fin_cognitive_explanation_t* explanation
);

/**
 * @brief Trigger curiosity-driven exploration
 *
 * @param orch Orchestrator handle
 * @param hypothesis Output hypothesis to explore
 * @param exploration_value Output exploration value
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_explore(
    financial_cognitive_orchestrator_handle_t* orch,
    char* hypothesis,
    float* exploration_value
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get orchestrator operational state
 *
 * @param orch Orchestrator handle
 * @return Current operational state
 */
fin_orchestrator_state_t financial_cognitive_orchestrator_get_state(
    const financial_cognitive_orchestrator_handle_t* orch
);

/**
 * @brief Get statistics
 *
 * @param orch Orchestrator handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_get_stats(
    const financial_cognitive_orchestrator_handle_t* orch,
    fin_orchestrator_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param orch Orchestrator handle
 */
void financial_cognitive_orchestrator_reset_stats(
    financial_cognitive_orchestrator_handle_t* orch
);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_cognitive_orchestrator_get_last_error(void);

/* ============================================================================
 * Health Integration
 * ============================================================================ */

/**
 * @brief Send heartbeat
 *
 * @param orch Orchestrator handle
 * @param operation Current operation
 * @param progress Progress [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_heartbeat(
    financial_cognitive_orchestrator_handle_t* orch,
    const char* operation,
    float progress
);

/**
 * @brief Set global health agent for module-level heartbeats
 *
 * @param agent Health agent (can be NULL to disable)
 */
void financial_cognitive_orchestrator_set_health_agent_global(void* agent);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get state name
 *
 * @param state Orchestrator state
 * @return String name (static)
 */
const char* fin_orchestrator_state_name(fin_orchestrator_state_t state);

/**
 * @brief Get decision type name
 *
 * @param decision Decision type
 * @return String name (static)
 */
const char* fin_orchestrator_decision_name(fin_decision_type_t decision);

/**
 * @brief Get pipeline stage name
 *
 * @param stage Pipeline stage
 * @return String name (static)
 */
const char* fin_orchestrator_stage_name(fin_pipeline_stage_t stage);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_cognitive_orchestrator_version(void);

/* ============================================================================
 * Training Integration
 * ============================================================================ */

/**
 * @brief Begin training session
 *
 * @param orch Orchestrator handle
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_training_begin(
    financial_cognitive_orchestrator_handle_t* orch
);

/**
 * @brief End training session
 *
 * @param orch Orchestrator handle
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_training_end(
    financial_cognitive_orchestrator_handle_t* orch
);

/**
 * @brief Training step
 *
 * @param orch Orchestrator handle
 * @param progress Training progress [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int financial_cognitive_orchestrator_training_step(
    financial_cognitive_orchestrator_handle_t* orch,
    float progress
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_COGNITIVE_ORCHESTRATOR_H */
