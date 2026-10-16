//=============================================================================
// nimcp_financial_mammillary_bridge.h - Financial Mammillary Memory Bridge
//=============================================================================
/**
 * @file nimcp_financial_mammillary_bridge.h
 * @brief Memory consolidation bridge for financial trade experiences
 *
 * WHAT: Bridges financial trading experiences to mammillary body memory
 *       circuits for episodic memory formation and pattern recognition.
 *
 * WHY:  The mammillary bodies are critical for memory consolidation,
 *       particularly episodic/autobiographical memory. This bridge enables:
 *       - Storage of trade experiences as memory traces
 *       - Consolidation of short-term trade memories to long-term patterns
 *       - Retrieval of similar past trading situations for decision support
 *
 * HOW:  Trade events are encoded as memory traces containing price, quantity,
 *       direction, outcome (P&L), market context, and emotional state.
 *       Memory consolidation strengthens important traces based on outcome
 *       magnitude and retrieval frequency. Query interface enables finding
 *       similar past situations using fuzzy matching on market conditions.
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#ifndef NIMCP_FINANCIAL_MAMMILLARY_BRIDGE_H
#define NIMCP_FINANCIAL_MAMMILLARY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_FINANCIAL_MAMMILLARY     0x0397
#define FIN_MAMMILLARY_MAX_TRACES           4096
#define FIN_MAMMILLARY_MAX_QUERY_RESULTS    64
#define FIN_MAMMILLARY_MAX_CONTEXT_DIM      16

//=============================================================================
// Error Codes
//=============================================================================

#define FIN_MAMMILLARY_ERROR_BASE           35000
#define FIN_MAMMILLARY_ERR_OK               0
#define FIN_MAMMILLARY_ERR_NULL             (FIN_MAMMILLARY_ERROR_BASE + 1)
#define FIN_MAMMILLARY_ERR_INVALID_PARAM    (FIN_MAMMILLARY_ERROR_BASE + 2)
#define FIN_MAMMILLARY_ERR_NO_MEMORY        (FIN_MAMMILLARY_ERROR_BASE + 3)
#define FIN_MAMMILLARY_ERR_CAPACITY         (FIN_MAMMILLARY_ERROR_BASE + 4)
#define FIN_MAMMILLARY_ERR_NOT_FOUND        (FIN_MAMMILLARY_ERROR_BASE + 5)
#define FIN_MAMMILLARY_ERR_SUBSYSTEM        (FIN_MAMMILLARY_ERROR_BASE + 6)
#define FIN_MAMMILLARY_ERR_VALIDATION       (FIN_MAMMILLARY_ERROR_BASE + 7)
#define FIN_MAMMILLARY_ERR_IMMUNE           (FIN_MAMMILLARY_ERROR_BASE + 8)
#define FIN_MAMMILLARY_ERR_BBB              (FIN_MAMMILLARY_ERROR_BASE + 9)
#define FIN_MAMMILLARY_ERR_CONSOLIDATION    (FIN_MAMMILLARY_ERROR_BASE + 10)

//=============================================================================
// Enumerations
//=============================================================================

/** Bridge operational state */
typedef enum {
    FIN_MAMMILLARY_STATE_UNINITIALIZED = 0,
    FIN_MAMMILLARY_STATE_IDLE,
    FIN_MAMMILLARY_STATE_STORING,
    FIN_MAMMILLARY_STATE_CONSOLIDATING,
    FIN_MAMMILLARY_STATE_QUERYING,
    FIN_MAMMILLARY_STATE_ERROR
} fin_mammillary_state_t;

/** Memory trace importance level */
typedef enum {
    FIN_TRACE_IMPORTANCE_LOW = 0,
    FIN_TRACE_IMPORTANCE_NORMAL,
    FIN_TRACE_IMPORTANCE_HIGH,
    FIN_TRACE_IMPORTANCE_CRITICAL
} fin_trace_importance_t;

/** Consolidation phase */
typedef enum {
    FIN_CONSOLIDATION_ENCODING = 0,
    FIN_CONSOLIDATION_STABILIZATION,
    FIN_CONSOLIDATION_INTEGRATION,
    FIN_CONSOLIDATION_COMPLETE
} fin_consolidation_phase_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Memory trace for a single trade experience
 *
 * Encapsulates all relevant information about a trade for memory storage.
 */
typedef struct {
    float trade_price;           /**< Entry/exit price */
    float trade_quantity;        /**< Position size */
    int trade_direction;         /**< 1=buy/long, -1=sell/short */
    float outcome;               /**< P&L result */
    float market_volatility;     /**< Volatility at time of trade */
    float market_trend;          /**< Trend direction/strength [-1,1] */
    float emotional_intensity;   /**< Emotional state during trade [0,1] */
    uint64_t timestamp_ms;       /**< Unix timestamp in milliseconds */
} fin_memory_trace_t;

/**
 * @brief Extended memory trace with consolidation metadata
 */
typedef struct {
    fin_memory_trace_t trace;    /**< Base trace data */
    float importance;            /**< Computed importance [0,1] */
    float consolidation_strength;/**< Memory strength after consolidation */
    uint32_t retrieval_count;    /**< Times this trace was retrieved */
    uint64_t last_retrieval_ms;  /**< Last retrieval timestamp */
    fin_consolidation_phase_t phase; /**< Current consolidation phase */
    float context[FIN_MAMMILLARY_MAX_CONTEXT_DIM]; /**< Extended context */
    uint32_t context_dim;        /**< Valid context dimensions */
} fin_stored_trace_t;

/**
 * @brief Query result with similarity score
 */
typedef struct {
    fin_stored_trace_t trace;    /**< The matched trace */
    float similarity;            /**< Similarity score [0,1] */
    float relevance;             /**< Contextual relevance [0,1] */
    uint32_t index;              /**< Index in storage */
} fin_query_result_t;

/**
 * @brief Query parameters for finding similar traces
 */
typedef struct {
    float target_price;          /**< Target price to match */
    float target_volatility;     /**< Target volatility to match */
    float target_trend;          /**< Target trend to match */
    float price_weight;          /**< Weight for price matching [0,1] */
    float volatility_weight;     /**< Weight for volatility matching [0,1] */
    float trend_weight;          /**< Weight for trend matching [0,1] */
    float outcome_weight;        /**< Weight for outcome matching [0,1] */
    float min_similarity;        /**< Minimum similarity threshold */
    uint32_t max_results;        /**< Maximum results to return */
    bool prefer_profitable;      /**< Prefer profitable outcomes */
    bool prefer_recent;          /**< Prefer recent traces */
} fin_query_params_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t traces_stored;      /**< Total traces stored */
    uint64_t consolidations;     /**< Total consolidation cycles */
    uint64_t queries;            /**< Total queries performed */
    uint64_t matches_found;      /**< Total matches returned */
    uint64_t immune_checks;      /**< Immune validation calls */
    uint64_t bbb_validations;    /**< BBB validation calls */
    uint64_t kg_messages_sent;   /**< KG messages published */
    uint64_t health_heartbeats;  /**< Health heartbeats sent */
} fin_mammillary_bridge_stats_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Bridge configuration
 */
typedef struct {
    uint32_t max_traces;         /**< Maximum stored traces */
    float consolidation_threshold; /**< Min importance for retention */
    float decay_rate;            /**< Memory decay rate per day */
    float retrieval_boost;       /**< Boost per retrieval */
    bool enable_emotional_weighting; /**< Weight by emotional intensity */
    bool enable_outcome_weighting;   /**< Weight by P&L magnitude */
    float inflammation_sensitivity;  /**< Health modulation factor */
    float fatigue_sensitivity;       /**< Health modulation factor */
} fin_mammillary_config_t;

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct financial_mammillary_bridge financial_mammillary_bridge_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create a new financial mammillary bridge
 * @param config Configuration (NULL for defaults)
 * @return New bridge instance or NULL on error
 */
financial_mammillary_bridge_t* financial_mammillary_bridge_create(
    const fin_mammillary_config_t* config);

/**
 * @brief Destroy a financial mammillary bridge
 * @param bridge Bridge to destroy
 */
void financial_mammillary_bridge_destroy(financial_mammillary_bridge_t* bridge);

/**
 * @brief Get default configuration
 * @return Default configuration values
 */
fin_mammillary_config_t financial_mammillary_bridge_default_config(void);

/**
 * @brief Get current bridge state
 * @param bridge Bridge instance
 * @return Current state
 */
fin_mammillary_state_t financial_mammillary_bridge_get_state(
    const financial_mammillary_bridge_t* bridge);

/**
 * @brief Reset the bridge to initial state
 * @param bridge Bridge instance
 * @return 0 on success
 */
int financial_mammillary_bridge_reset(financial_mammillary_bridge_t* bridge);

//=============================================================================
// Subsystem Setters
//=============================================================================

/** Set immune system for validation */
int financial_mammillary_bridge_set_immune(financial_mammillary_bridge_t* bridge,
                                           void* immune);

/** Set BBB for data validation */
int financial_mammillary_bridge_set_bbb(financial_mammillary_bridge_t* bridge,
                                         void* bbb);

/** Set health agent for heartbeats */
int financial_mammillary_bridge_set_health_agent(financial_mammillary_bridge_t* bridge,
                                                  void* health_agent);

/** Set logger for diagnostics */
int financial_mammillary_bridge_set_logger(financial_mammillary_bridge_t* bridge,
                                            void* logger);

/** Enable/disable BBB validation */
int financial_mammillary_bridge_enable_bbb_validation(financial_mammillary_bridge_t* bridge,
                                                       bool enable);

/** Enable/disable immune validation */
int financial_mammillary_bridge_enable_immune_validation(financial_mammillary_bridge_t* bridge,
                                                          bool enable);

//=============================================================================
// KG Wiring Integration
//=============================================================================

/* Forward declaration for KG wiring */
struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/** Set KG wiring for inter-module communication */
int financial_mammillary_bridge_set_kg_wiring(financial_mammillary_bridge_t* bridge,
                                               kg_wiring_t* kg);

//=============================================================================
// Core Operations
//=============================================================================

/**
 * @brief Store a trade as a memory trace (relay to mammillary)
 *
 * @param bridge Bridge instance
 * @param trace Trade memory trace to store
 * @return 0 on success, error code on failure
 */
int financial_mammillary_bridge_relay_trade(financial_mammillary_bridge_t* bridge,
                                            const fin_memory_trace_t* trace);

/**
 * @brief Store a trade with extended context
 *
 * @param bridge Bridge instance
 * @param trace Trade memory trace
 * @param context Extended context array
 * @param context_dim Number of context dimensions
 * @return 0 on success, error code on failure
 */
int financial_mammillary_bridge_relay_trade_with_context(
    financial_mammillary_bridge_t* bridge,
    const fin_memory_trace_t* trace,
    const float* context,
    uint32_t context_dim);

/**
 * @brief Consolidate stored memories
 *
 * Performs memory consolidation cycle:
 * - Decays old, unretrieved memories
 * - Strengthens frequently-retrieved memories
 * - Removes low-importance traces below threshold
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int financial_mammillary_bridge_consolidate(financial_mammillary_bridge_t* bridge);

/**
 * @brief Query for similar past situations
 *
 * @param bridge Bridge instance
 * @param params Query parameters
 * @param results Output array for results
 * @param max_results Maximum results to return
 * @param out_count Actual number of results returned
 * @return 0 on success, error code on failure
 */
int financial_mammillary_bridge_query_similar(
    financial_mammillary_bridge_t* bridge,
    const fin_query_params_t* params,
    fin_query_result_t* results,
    uint32_t max_results,
    uint32_t* out_count);

/**
 * @brief Get default query parameters
 * @return Default query parameters
 */
fin_query_params_t financial_mammillary_bridge_default_query_params(void);

//=============================================================================
// Memory Access
//=============================================================================

/**
 * @brief Get number of stored traces
 * @param bridge Bridge instance
 * @return Number of traces or 0 on error
 */
uint32_t financial_mammillary_bridge_get_trace_count(
    const financial_mammillary_bridge_t* bridge);

/**
 * @brief Get a specific trace by index
 * @param bridge Bridge instance
 * @param index Trace index
 * @param out_trace Output trace
 * @return 0 on success, error code on failure
 */
int financial_mammillary_bridge_get_trace(
    const financial_mammillary_bridge_t* bridge,
    uint32_t index,
    fin_stored_trace_t* out_trace);

//=============================================================================
// Health & Modulation
//=============================================================================

/** Set inflammation level (affects consolidation) */
int financial_mammillary_bridge_set_inflammation(financial_mammillary_bridge_t* bridge,
                                                  float level);

/** Set fatigue level (affects memory encoding) */
int financial_mammillary_bridge_set_fatigue(financial_mammillary_bridge_t* bridge,
                                             float level);

/** Get bridge statistics */
int financial_mammillary_bridge_get_stats(const financial_mammillary_bridge_t* bridge,
                                           fin_mammillary_bridge_stats_t* stats);

/** Reset statistics */
void financial_mammillary_bridge_reset_stats(financial_mammillary_bridge_t* bridge);

/** Get last error message (TLS) */
const char* financial_mammillary_bridge_get_last_error(void);

//=============================================================================
// Global Subsystem Setters (for backward compatibility)
//=============================================================================

/** Set global immune system */
void financial_mammillary_bridge_set_immune_system(void* immune);

/** Set global BBB system */
void financial_mammillary_bridge_set_bbb_system(void* bbb);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_MAMMILLARY_BRIDGE_H */
