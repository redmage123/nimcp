/**
 * @file nimcp_financial_stdp_bridge.h
 * @brief Financial STDP Bridge - Spike-Timing Dependent Plasticity for Trading
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for learning timing correlations between market signals and
 *       trade outcomes using STDP (Spike-Timing Dependent Plasticity).
 *
 * WHY:  Financial markets exhibit temporal patterns where certain signals
 *       (technical indicators, sentiment shifts, price movements) precede
 *       profitable/unprofitable outcomes. STDP naturally learns these
 *       signal-to-outcome timing correlations, strengthening connections
 *       for predictive signals and weakening connections for misleading ones.
 *
 * HOW:  When a signal occurs followed by a trade outcome, the temporal
 *       difference determines synaptic weight changes:
 *       - Signal before positive outcome: LTP (Long-Term Potentiation)
 *       - Signal after outcome (or before negative): LTD (Long-Term Depression)
 *       Trade results (P&L) modulate plasticity magnitude (reward-modulated STDP).
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |              Financial STDP Bridge                                        |
 * +===========================================================================+
 * |                                                                           |
 * |  +-----------------------+       +-----------------------+                |
 * |  |  Signal Registry      |       |  Outcome Buffer       |                |
 * |  +-----------------------+       +-----------------------+                |
 * |  | Signal type           |       | Trade P&L             |                |
 * |  | Strength              |       | Timestamp             |                |
 * |  | Timestamp             |       | Context               |                |
 * |  +----------+------------+       +------------+----------+                |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |          STDP Learning Engine                             |            |
 * |  |  dt = t_outcome - t_signal                                |            |
 * |  |  dw = A+ * exp(-dt/tau+) if dt > 0 (predictive)          |            |
 * |  |  dw = -A- * exp(dt/tau-) if dt < 0 (anti-predictive)     |            |
 * |  |  dw *= reward_modulation(P&L)                             |            |
 * |  +----------------------------------------------------------+            |
 * |             |                                                             |
 * |             v                                                             |
 * |  +----------------------------------------------------------+            |
 * |  |          Correlation Weights                              |            |
 * |  |  signal_type -> weight (learned predictive strength)     |            |
 * |  +----------------------------------------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * SIGNAL TYPES:
 * - Technical: RSI/MACD/Bollinger crossovers, volume spikes
 * - Sentiment: News sentiment shifts, social media trends
 * - Price: Gap up/down, breakout, reversal patterns
 * - Fundamental: Earnings surprise, rating changes
 * - Cross-asset: Correlation breaks, sector rotation
 *
 * @see nimcp_financial_bridge.h
 * @see nimcp_financial_neural_bridge.h
 * @see nimcp_parietal_plasticity_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_STDP_BRIDGE_H
#define NIMCP_FINANCIAL_STDP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_STDP_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_STDP_BRIDGE_MAGIC      0x46535450  /* 'FSTP' */

/** Bio-async module ID for financial STDP bridge */
#define BIO_MODULE_FINANCIAL_STDP        0x039B

/** Maximum signal types tracked */
#define FIN_STDP_MAX_SIGNAL_TYPES        64

/** Maximum signals in temporal window */
#define FIN_STDP_MAX_SIGNALS             256

/** Maximum outcomes in buffer */
#define FIN_STDP_MAX_OUTCOMES            128

/** Default STDP time window (ms) */
#define FIN_STDP_DEFAULT_WINDOW_MS       60000  /* 1 minute */

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_STDP_ERROR_BASE              33500
#define FIN_STDP_ERR_OK                  0
#define FIN_STDP_ERR_NULL                (FIN_STDP_ERROR_BASE + 1)
#define FIN_STDP_ERR_INVALID_PARAM       (FIN_STDP_ERROR_BASE + 2)
#define FIN_STDP_ERR_NO_MEMORY           (FIN_STDP_ERROR_BASE + 3)
#define FIN_STDP_ERR_STATE               (FIN_STDP_ERROR_BASE + 4)
#define FIN_STDP_ERR_IMMUNE              (FIN_STDP_ERROR_BASE + 5)
#define FIN_STDP_ERR_BBB                 (FIN_STDP_ERROR_BASE + 6)
#define FIN_STDP_ERR_FULL                (FIN_STDP_ERROR_BASE + 7)
#define FIN_STDP_ERR_NOT_FOUND           (FIN_STDP_ERROR_BASE + 8)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_STDP_OP_STATE_UNINITIALIZED = 0,
    FIN_STDP_OP_STATE_INITIALIZED,
    FIN_STDP_OP_STATE_LEARNING,
    FIN_STDP_OP_STATE_ACTIVE,
    FIN_STDP_OP_STATE_DEGRADED,
    FIN_STDP_OP_STATE_ERROR
} fin_stdp_op_state_t;

/**
 * @brief Signal type categories
 */
typedef enum {
    FIN_STDP_SIGNAL_TECHNICAL = 0,   /**< Technical indicator signal */
    FIN_STDP_SIGNAL_SENTIMENT,        /**< Sentiment/news signal */
    FIN_STDP_SIGNAL_PRICE_ACTION,     /**< Price action pattern */
    FIN_STDP_SIGNAL_FUNDAMENTAL,      /**< Fundamental data signal */
    FIN_STDP_SIGNAL_CROSS_ASSET,      /**< Cross-asset correlation */
    FIN_STDP_SIGNAL_VOLUME,           /**< Volume signal */
    FIN_STDP_SIGNAL_VOLATILITY,       /**< Volatility signal */
    FIN_STDP_SIGNAL_CUSTOM,           /**< User-defined signal */
    FIN_STDP_SIGNAL_TYPE_COUNT
} fin_stdp_signal_category_t;

/**
 * @brief Plasticity mode
 */
typedef enum {
    FIN_STDP_MODE_STANDARD = 0,      /**< Standard STDP */
    FIN_STDP_MODE_REWARD_MODULATED,  /**< Reward-modulated STDP */
    FIN_STDP_MODE_TRIPLET,           /**< Triplet STDP rule */
    FIN_STDP_MODE_BCM                /**< BCM-style sliding threshold */
} fin_stdp_mode_t;

/* ============================================================================
 * Core Data Structures (as specified by user)
 * ============================================================================ */

/**
 * @brief Financial signal event
 */
typedef struct {
    int signal_type;       /**< Signal type identifier */
    float strength;        /**< Signal strength [0-1] */
    uint64_t timestamp_ms; /**< Timestamp in milliseconds */
} fin_signal_t;

/**
 * @brief Financial outcome (trade result)
 */
typedef struct {
    float outcome;         /**< Trade outcome (P&L, normalized) */
    uint64_t timestamp_ms; /**< Timestamp in milliseconds */
} fin_outcome_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t correlations_learned;  /**< Total correlations learned */
    uint64_t updates_from_trades;   /**< Weight updates from trade outcomes */
    uint64_t immune_checks;         /**< Immune system checks performed */
    uint64_t bbb_validations;       /**< BBB validations performed */
    uint64_t kg_messages_sent;      /**< KG messages published */
    uint64_t health_heartbeats;     /**< Health heartbeats sent */
} fin_stdp_bridge_stats_t;

/* ============================================================================
 * Extended Data Structures
 * ============================================================================ */

/**
 * @brief Extended signal with metadata
 */
typedef struct {
    fin_signal_t signal;                /**< Base signal data */
    fin_stdp_signal_category_t category; /**< Signal category */
    char name[64];                       /**< Signal name (e.g., "RSI_oversold") */
    float confidence;                    /**< Signal confidence [0-1] */
    uint32_t context_id;                 /**< Context/session identifier */
} fin_signal_extended_t;

/**
 * @brief Extended outcome with metadata
 */
typedef struct {
    fin_outcome_t outcome;              /**< Base outcome data */
    float confidence;                   /**< Outcome confidence [0-1] */
    uint32_t context_id;                /**< Context/session identifier */
    char symbol[16];                    /**< Trading symbol */
} fin_outcome_extended_t;

/**
 * @brief Learned signal-outcome correlation
 */
typedef struct {
    int signal_type;                    /**< Signal type */
    float weight;                       /**< Current synaptic weight */
    float initial_weight;               /**< Initial weight */
    float eligibility_trace;            /**< Eligibility trace for learning */
    uint64_t update_count;              /**< Number of weight updates */
    uint64_t last_update_ms;            /**< Last update timestamp */
    float avg_dt_ms;                    /**< Average signal-outcome delay */
    float predictive_accuracy;          /**< Computed predictive accuracy */
} fin_stdp_correlation_t;

/**
 * @brief STDP learning result
 */
typedef struct {
    int signal_type;                    /**< Signal type that was updated */
    float weight_before;                /**< Weight before update */
    float weight_after;                 /**< Weight after update */
    float delta_weight;                 /**< Weight change */
    float dt_ms;                        /**< Signal-outcome temporal difference */
    bool is_ltp;                        /**< True if LTP, false if LTD */
} fin_stdp_learn_result_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* STDP parameters */
    float tau_plus_ms;                  /**< LTP time constant (ms) */
    float tau_minus_ms;                 /**< LTD time constant (ms) */
    float a_plus;                       /**< LTP magnitude */
    float a_minus;                      /**< LTD magnitude */
    float learning_rate;                /**< Base learning rate */
    uint64_t learning_window_ms;        /**< Temporal window for correlation (ms) */

    /* Weight bounds */
    float weight_min;                   /**< Minimum weight */
    float weight_max;                   /**< Maximum weight */
    float initial_weight;               /**< Initial weight for new signals */

    /* Reward modulation */
    bool enable_reward_modulation;      /**< Enable reward-modulated STDP */
    float reward_scale;                 /**< Scale factor for reward modulation */
    float punishment_scale;             /**< Scale factor for loss modulation */

    /* BCM parameters (optional) */
    bool enable_bcm;                    /**< Enable BCM sliding threshold */
    float bcm_tau_ms;                   /**< BCM threshold time constant */
    float bcm_target_rate;              /**< BCM target activity */

    /* Homeostatic parameters */
    bool enable_homeostasis;            /**< Enable homeostatic scaling */
    float homeostatic_tau_ms;           /**< Homeostatic time constant */
    float target_mean_weight;           /**< Target mean weight */

    /* Integration settings */
    bool enable_immune_integration;     /**< Enable immune system checks */
    bool enable_bbb_validation;         /**< Enable BBB validation */
    bool enable_kg_messaging;           /**< Enable KG messaging */
    bool enable_health_monitoring;      /**< Enable health heartbeats */

    /* Plasticity mode */
    fin_stdp_mode_t mode;               /**< STDP mode */

    /* Logging */
    bool verbose_logging;               /**< Verbose debug output */
} fin_stdp_config_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Learning event callback
 */
typedef void (*fin_stdp_learn_callback_t)(
    const fin_stdp_learn_result_t* result,
    void* user_data
);

/**
 * @brief Signal registered callback
 */
typedef void (*fin_stdp_signal_callback_t)(
    const fin_signal_extended_t* signal,
    void* user_data
);

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
 * Bridge Handle
 * ============================================================================ */

/**
 * @brief Opaque financial STDP bridge handle
 */
typedef struct financial_stdp_bridge financial_stdp_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_default_config(fin_stdp_config_t* config);

/**
 * @brief Create financial STDP bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_stdp_bridge_t* financial_stdp_bridge_create(
    const fin_stdp_config_t* config
);

/**
 * @brief Destroy financial STDP bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_stdp_bridge_destroy(financial_stdp_bridge_t* bridge);

/**
 * @brief Reset bridge state (clear signals/outcomes, keep learned weights)
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_reset(financial_stdp_bridge_t* bridge);

/**
 * @brief Reset all learned weights to initial values
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_reset_weights(financial_stdp_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_stdp_bridge_set_immune(
    financial_stdp_bridge_t* bridge,
    void* immune
);

/**
 * @brief Set BBB system handle
 */
int financial_stdp_bridge_set_bbb(
    financial_stdp_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Set health agent handle
 */
int financial_stdp_bridge_set_health_agent(
    financial_stdp_bridge_t* bridge,
    void* health_agent
);

/**
 * @brief Set KG wiring handle
 */
int financial_stdp_bridge_set_kg_wiring(
    financial_stdp_bridge_t* bridge,
    void* kg_wiring
);

/**
 * @brief Set logger handle
 */
int financial_stdp_bridge_set_logger(
    financial_stdp_bridge_t* bridge,
    void* logger
);

/**
 * @brief Set security handle
 */
int financial_stdp_bridge_set_security(
    financial_stdp_bridge_t* bridge,
    void* security
);

/**
 * @brief Set ethics engine handle
 */
int financial_stdp_bridge_set_ethics(
    financial_stdp_bridge_t* bridge,
    ethics_engine_t ethics
);

/**
 * @brief Set LGSS handle
 */
int financial_stdp_bridge_set_lgss(
    financial_stdp_bridge_t* bridge,
    const void* lgss
);

/**
 * @brief Set cycle coordinator handle
 */
int financial_stdp_bridge_set_coordinator(
    financial_stdp_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator
);

/**
 * @brief Set bio router handle
 */
int financial_stdp_bridge_set_bio_router(
    financial_stdp_bridge_t* bridge,
    void* bio_router
);

/* ============================================================================
 * Core API - Signal Recording
 * ============================================================================ */

/**
 * @brief Record a signal event
 *
 * @param bridge Bridge handle
 * @param signal Signal to record
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_record_signal(
    financial_stdp_bridge_t* bridge,
    const fin_signal_t* signal
);

/**
 * @brief Record a signal event with extended metadata
 *
 * @param bridge Bridge handle
 * @param signal Extended signal to record
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_record_signal_ex(
    financial_stdp_bridge_t* bridge,
    const fin_signal_extended_t* signal
);

/**
 * @brief Clear all recorded signals
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_clear_signals(financial_stdp_bridge_t* bridge);

/* ============================================================================
 * Core API - Learning
 * ============================================================================ */

/**
 * @brief Learn timing correlation between signal and outcome
 *
 * Applies STDP rule based on temporal difference between recorded signals
 * and the provided outcome. Updates synaptic weights for all signals
 * within the learning window.
 *
 * @param bridge Bridge handle
 * @param signal Signal that occurred
 * @param outcome Outcome that followed
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_learn_correlation(
    financial_stdp_bridge_t* bridge,
    const fin_signal_t* signal,
    const fin_outcome_t* outcome
);

/**
 * @brief Update weights based on trade result
 *
 * Updates weights for all signals that occurred within the learning window
 * prior to the trade outcome. Reward-modulates the plasticity based on P&L.
 *
 * @param bridge Bridge handle
 * @param outcome Trade outcome (P&L)
 * @return Number of signals updated, or negative error code
 */
int financial_stdp_bridge_update_from_trade(
    financial_stdp_bridge_t* bridge,
    const fin_outcome_t* outcome
);

/**
 * @brief Apply batch learning from signal-outcome pairs
 *
 * @param bridge Bridge handle
 * @param signals Array of signals
 * @param outcomes Array of outcomes
 * @param count Number of pairs
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_batch_learn(
    financial_stdp_bridge_t* bridge,
    const fin_signal_t* signals,
    const fin_outcome_t* outcomes,
    uint32_t count
);

/**
 * @brief Apply homeostatic scaling to weights
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_apply_homeostasis(financial_stdp_bridge_t* bridge);

/**
 * @brief Consolidate learning (e.g., during simulated "sleep")
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_consolidate(financial_stdp_bridge_t* bridge);

/* ============================================================================
 * Core API - Weight Access
 * ============================================================================ */

/**
 * @brief Get correlation weight for a signal type
 *
 * @param bridge Bridge handle
 * @param signal_type Signal type identifier
 * @param out_weight Output weight value
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_get_weight(
    financial_stdp_bridge_t* bridge,
    int signal_type,
    float* out_weight
);

/**
 * @brief Get full correlation info for a signal type
 *
 * @param bridge Bridge handle
 * @param signal_type Signal type identifier
 * @param out_correlation Output correlation structure
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_get_correlation(
    financial_stdp_bridge_t* bridge,
    int signal_type,
    fin_stdp_correlation_t* out_correlation
);

/**
 * @brief Get all learned correlations
 *
 * @param bridge Bridge handle
 * @param correlations Output array (caller allocated)
 * @param max_count Maximum correlations to return
 * @return Number of correlations, or negative error code
 */
int financial_stdp_bridge_get_all_correlations(
    financial_stdp_bridge_t* bridge,
    fin_stdp_correlation_t* correlations,
    uint32_t max_count
);

/**
 * @brief Get top N predictive signals
 *
 * @param bridge Bridge handle
 * @param correlations Output array (caller allocated)
 * @param count Number to return
 * @return Number returned, or negative error code
 */
int financial_stdp_bridge_get_top_predictive(
    financial_stdp_bridge_t* bridge,
    fin_stdp_correlation_t* correlations,
    uint32_t count
);

/**
 * @brief Get signal count
 *
 * @param bridge Bridge handle
 * @return Number of tracked signal types
 */
uint32_t financial_stdp_bridge_get_signal_count(
    const financial_stdp_bridge_t* bridge
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set learning event callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_set_learn_callback(
    financial_stdp_bridge_t* bridge,
    fin_stdp_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Set signal registered callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_set_signal_callback(
    financial_stdp_bridge_t* bridge,
    fin_stdp_signal_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge operational state
 *
 * @param bridge Bridge handle
 * @return Current operational state
 */
fin_stdp_op_state_t financial_stdp_bridge_get_op_state(
    const financial_stdp_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_get_stats(
    const financial_stdp_bridge_t* bridge,
    fin_stdp_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_stdp_bridge_reset_stats(financial_stdp_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_stdp_bridge_get_last_error(void);

/* ============================================================================
 * Health Integration
 * ============================================================================ */

/**
 * @brief Send heartbeat
 *
 * @param bridge Bridge handle
 * @param operation Current operation
 * @param progress Progress [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int financial_stdp_bridge_heartbeat(
    financial_stdp_bridge_t* bridge,
    const char* operation,
    float progress
);

/**
 * @brief Set global health agent for module-level heartbeats
 *
 * @param agent Health agent (can be NULL to disable)
 */
void financial_stdp_bridge_set_health_agent_global(void* agent);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get operational state name
 *
 * @param state Operational state
 * @return String name (static)
 */
const char* fin_stdp_op_state_name(fin_stdp_op_state_t state);

/**
 * @brief Get signal category name
 *
 * @param category Signal category
 * @return String name (static)
 */
const char* fin_stdp_signal_category_name(fin_stdp_signal_category_t category);

/**
 * @brief Get plasticity mode name
 *
 * @param mode Plasticity mode
 * @return String name (static)
 */
const char* fin_stdp_mode_name(fin_stdp_mode_t mode);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_stdp_bridge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_STDP_BRIDGE_H */
