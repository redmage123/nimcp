//=============================================================================
// nimcp_financial_resonance_bridge.h - Financial Resonance Pattern Bridge
//=============================================================================
/**
 * @file nimcp_financial_resonance_bridge.h
 * @brief Resonance-based pattern matching and similarity detection for
 *        financial market data using oscillator-based encoding
 *
 * WHAT: Bridges financial market states to resonance-based pattern representation
 *       using Jaccard similarity, phase synchronization, quaternion distance,
 *       and Kuramoto coupling for cross-asset coherence detection.
 *
 * WHY:  Traditional financial pattern matching uses static templates.
 *       Resonance encoding enables:
 *       - Dynamic pattern representation through oscillator phases
 *       - Cross-asset synchronization detection via Kuramoto coupling
 *       - Multi-scale similarity through composite scoring
 *       - Robust pattern retrieval under market regime changes
 *
 * HOW:  Market states are encoded as resonance queries with signature hashes
 *       and oscillator phases. Similarity search uses weighted combination of:
 *       - Jaccard similarity (set-based overlap)
 *       - Phase coherence (oscillator alignment)
 *       - Quaternion distance (rotation-invariant similarity)
 *       - Kuramoto order parameter (cross-asset sync)
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#ifndef NIMCP_FINANCIAL_RESONANCE_BRIDGE_H
#define NIMCP_FINANCIAL_RESONANCE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_FINANCIAL_RESONANCE      0x0397
#define FIN_RESONANCE_MAX_OSCILLATORS       8
#define FIN_RESONANCE_MAX_PATTERNS          256
#define FIN_RESONANCE_MAX_ASSETS            64
#define FIN_RESONANCE_SIGNATURE_BITS        64

//=============================================================================
// Error Codes
//=============================================================================

#define FIN_RESONANCE_ERROR_BASE            35000
#define FIN_RESONANCE_ERR_OK                0
#define FIN_RESONANCE_ERR_NULL              (FIN_RESONANCE_ERROR_BASE + 1)
#define FIN_RESONANCE_ERR_INVALID_PARAM     (FIN_RESONANCE_ERROR_BASE + 2)
#define FIN_RESONANCE_ERR_ENCODING          (FIN_RESONANCE_ERROR_BASE + 3)
#define FIN_RESONANCE_ERR_SIMILARITY        (FIN_RESONANCE_ERROR_BASE + 4)
#define FIN_RESONANCE_ERR_COHERENCE         (FIN_RESONANCE_ERROR_BASE + 5)
#define FIN_RESONANCE_ERR_SUBSYSTEM         (FIN_RESONANCE_ERROR_BASE + 6)
#define FIN_RESONANCE_ERR_VALIDATION        (FIN_RESONANCE_ERROR_BASE + 7)
#define FIN_RESONANCE_ERR_NOT_FOUND         (FIN_RESONANCE_ERROR_BASE + 8)
#define FIN_RESONANCE_ERR_CAPACITY          (FIN_RESONANCE_ERROR_BASE + 9)
#define FIN_RESONANCE_ERR_STATE             (FIN_RESONANCE_ERROR_BASE + 10)

//=============================================================================
// Enumerations
//=============================================================================

/** Bridge operational state */
typedef enum {
    FIN_RESONANCE_STATE_UNINITIALIZED = 0,
    FIN_RESONANCE_STATE_IDLE,
    FIN_RESONANCE_STATE_ENCODING,
    FIN_RESONANCE_STATE_SEARCHING,
    FIN_RESONANCE_STATE_COMPUTING,
    FIN_RESONANCE_STATE_DEGRADED,
    FIN_RESONANCE_STATE_ERROR
} fin_resonance_state_t;

/** Encoding schemes for market state to resonance */
typedef enum {
    FIN_RESONANCE_ENCODING_HASH,           /**< Simple hash-based encoding */
    FIN_RESONANCE_ENCODING_PHASE,          /**< Phase-angle encoding */
    FIN_RESONANCE_ENCODING_QUATERNION,     /**< Quaternion rotation encoding */
    FIN_RESONANCE_ENCODING_HYBRID,         /**< Combined multi-method encoding */
    FIN_RESONANCE_ENCODING_COUNT
} fin_resonance_encoding_t;

/** Market condition regime for context */
typedef enum {
    FIN_REGIME_UNKNOWN = 0,
    FIN_REGIME_BULL,
    FIN_REGIME_BEAR,
    FIN_REGIME_SIDEWAYS,
    FIN_REGIME_HIGH_VOLATILITY,
    FIN_REGIME_CRISIS,
    FIN_REGIME_RECOVERY,
    FIN_REGIME_COUNT
} fin_market_regime_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Resonance query for pattern matching
 *
 * Encodes a market state as a combination of:
 * - Signature hash for fast pre-filtering
 * - Global phase for temporal alignment
 * - Oscillator phases for multi-scale representation
 */
typedef struct {
    uint64_t signature_hash;                        /**< LSH-style signature */
    float phase;                                    /**< Global phase [0, 2*pi) */
    float oscillator_phases[FIN_RESONANCE_MAX_OSCILLATORS]; /**< Per-oscillator phases */
    uint32_t num_oscillators;                       /**< Active oscillators */
} fin_resonance_query_t;

/**
 * @brief Result of similarity computation
 *
 * Provides multi-faceted similarity scores for pattern comparison.
 */
typedef struct {
    float jaccard_score;                            /**< Set overlap similarity [0,1] */
    float phase_score;                              /**< Phase alignment score [0,1] */
    float quaternion_score;                         /**< Rotation distance score [0,1] */
    float kuramoto_score;                           /**< Kuramoto coupling score [0,1] */
    float combined_score;                           /**< Weighted composite score [0,1] */
    uint64_t trace_id;                              /**< Debug trace identifier */
} fin_resonance_result_t;

/**
 * @brief Market state input for encoding
 */
typedef struct {
    char symbol[32];                                /**< Asset symbol */
    float price;                                    /**< Current price */
    float volume;                                   /**< Trading volume */
    float volatility;                               /**< Realized volatility */
    float momentum;                                 /**< Price momentum */
    float rsi;                                      /**< RSI indicator [0,100] */
    float macd;                                     /**< MACD value */
    uint64_t timestamp_us;                          /**< Observation timestamp */
    fin_market_regime_t regime;                     /**< Current market regime */
} fin_market_state_t;

/**
 * @brief Stored pattern with metadata
 */
typedef struct {
    fin_resonance_query_t query;                    /**< Encoded resonance query */
    float outcome;                                  /**< Historical outcome */
    float importance;                               /**< Pattern importance weight */
    uint64_t creation_time_us;                      /**< Creation timestamp */
    uint32_t retrieval_count;                       /**< Times retrieved */
    fin_market_regime_t regime;                     /**< Regime when created */
    char label[64];                                 /**< Optional pattern label */
} fin_resonance_pattern_t;

/**
 * @brief Cross-asset coherence data for Kuramoto computation
 */
typedef struct {
    char symbols[FIN_RESONANCE_MAX_ASSETS][32];     /**< Asset symbols */
    float phases[FIN_RESONANCE_MAX_ASSETS];         /**< Phase per asset */
    float natural_freqs[FIN_RESONANCE_MAX_ASSETS];  /**< Natural frequencies */
    float coupling_matrix[FIN_RESONANCE_MAX_ASSETS][FIN_RESONANCE_MAX_ASSETS]; /**< Coupling strengths */
    uint32_t num_assets;                            /**< Number of assets */
} fin_kuramoto_input_t;

/**
 * @brief Kuramoto coherence output
 */
typedef struct {
    float order_parameter;                          /**< Global sync [0,1] */
    float mean_phase;                               /**< Collective phase */
    float phase_variance;                           /**< Phase dispersion */
    float* asset_contributions;                     /**< Per-asset sync contribution (optional) */
    uint32_t num_synced;                            /**< Fully synchronized count */
    uint32_t num_desynced;                          /**< Desynchronized count */
    bool critical_sync;                             /**< Above critical threshold? */
} fin_kuramoto_output_t;

//=============================================================================
// Bridge Statistics
//=============================================================================

/**
 * @brief Operational statistics for the resonance bridge
 */
typedef struct {
    uint64_t encodings;                             /**< Total encoding operations */
    uint64_t similarity_queries;                    /**< Total similarity searches */
    uint64_t coherence_calcs;                       /**< Kuramoto coherence calculations */
    uint64_t immune_checks;                         /**< Immune validation calls */
    uint64_t bbb_validations;                       /**< BBB validation calls */
    uint64_t kg_messages_sent;                      /**< KG wiring messages sent */
    uint64_t health_heartbeats;                     /**< Heartbeat signals sent */
    uint64_t patterns_stored;                       /**< Patterns in storage */
    uint64_t patterns_retrieved;                    /**< Pattern retrievals */
    float avg_encoding_time_us;                     /**< Average encoding time */
    float avg_search_time_us;                       /**< Average search time */
    float avg_combined_score;                       /**< Average similarity score */
} fin_resonance_bridge_stats_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for the resonance bridge
 */
typedef struct {
    /* Encoding parameters */
    fin_resonance_encoding_t default_encoding;      /**< Default encoding method */
    uint32_t num_oscillators;                       /**< Oscillators per encoding */
    float phase_resolution;                         /**< Phase quantization */
    float hash_similarity_threshold;                /**< Pre-filter threshold */

    /* Similarity weights */
    float jaccard_weight;                           /**< Weight for Jaccard score */
    float phase_weight;                             /**< Weight for phase score */
    float quaternion_weight;                        /**< Weight for quaternion score */
    float kuramoto_weight;                          /**< Weight for Kuramoto score */

    /* Kuramoto parameters */
    float kuramoto_coupling_strength;               /**< Global coupling K */
    float critical_sync_threshold;                  /**< Threshold for critical sync */

    /* Storage limits */
    uint32_t max_patterns;                          /**< Maximum stored patterns */
    float pattern_consolidation_threshold;          /**< Threshold for pruning */

    /* Modulation */
    float inflammation_sensitivity;                 /**< Response to inflammation */
    float fatigue_sensitivity;                      /**< Response to fatigue */

    /* Validation flags */
    bool enable_immune_validation;                  /**< Enable immune checks */
    bool enable_bbb_validation;                     /**< Enable BBB validation */

    /* Logging */
    bool enable_trace_logging;                      /**< Enable trace IDs */
} fin_resonance_config_t;

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct financial_resonance_bridge financial_resonance_bridge_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create a new financial resonance bridge
 * @param config Configuration (NULL for defaults)
 * @return New bridge instance or NULL on error
 */
financial_resonance_bridge_t* financial_resonance_bridge_create(
    const fin_resonance_config_t* config);

/**
 * @brief Destroy a financial resonance bridge
 * @param bridge Bridge to destroy
 */
void financial_resonance_bridge_destroy(financial_resonance_bridge_t* bridge);

/**
 * @brief Get default configuration
 * @return Default configuration structure
 */
fin_resonance_config_t financial_resonance_bridge_default_config(void);

/**
 * @brief Get current bridge state
 * @param bridge Bridge instance
 * @return Current operational state
 */
fin_resonance_state_t financial_resonance_bridge_get_state(
    const financial_resonance_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 * @param bridge Bridge instance
 * @return FIN_RESONANCE_ERR_OK on success
 */
int financial_resonance_bridge_reset(financial_resonance_bridge_t* bridge);

//=============================================================================
// Subsystem Setters
//=============================================================================

/** Set immune system for validation */
int financial_resonance_bridge_set_immune(financial_resonance_bridge_t* bridge, void* immune);

/** Set BBB for data validation */
int financial_resonance_bridge_set_bbb(financial_resonance_bridge_t* bridge, void* bbb);

/** Set health agent for heartbeats */
int financial_resonance_bridge_set_health_agent(financial_resonance_bridge_t* bridge, void* health_agent);

/** Set KG wiring for message passing */
int financial_resonance_bridge_set_kg_wiring(financial_resonance_bridge_t* bridge, void* kg_wiring);

/** Set logger for output */
int financial_resonance_bridge_set_logger(financial_resonance_bridge_t* bridge, void* logger);

/** Set security module */
int financial_resonance_bridge_set_security(financial_resonance_bridge_t* bridge, void* security);

/** Set bio router for metabolic integration */
int financial_resonance_bridge_set_bio_router(financial_resonance_bridge_t* bridge, void* bio_router);

/** Set cycle for temporal coordination */
int financial_resonance_bridge_set_cycle(financial_resonance_bridge_t* bridge, void* cycle);

/** Enable/disable immune validation */
int financial_resonance_bridge_enable_immune_validation(financial_resonance_bridge_t* bridge, bool enable);

/** Enable/disable BBB validation */
int financial_resonance_bridge_enable_bbb_validation(financial_resonance_bridge_t* bridge, bool enable);

//=============================================================================
// Core API: Encoding
//=============================================================================

/**
 * @brief Encode market state to resonance query
 *
 * Converts a market state observation into a resonance query suitable
 * for pattern matching and similarity search.
 *
 * @param bridge Bridge instance
 * @param state Market state to encode
 * @param out_query Output resonance query
 * @return FIN_RESONANCE_ERR_OK on success
 */
int financial_resonance_bridge_encode_market(
    financial_resonance_bridge_t* bridge,
    const fin_market_state_t* state,
    fin_resonance_query_t* out_query);

/**
 * @brief Encode multiple market states (batch encoding)
 *
 * @param bridge Bridge instance
 * @param states Array of market states
 * @param count Number of states
 * @param out_queries Output array of queries
 * @return FIN_RESONANCE_ERR_OK on success
 */
int financial_resonance_bridge_encode_batch(
    financial_resonance_bridge_t* bridge,
    const fin_market_state_t* states,
    uint32_t count,
    fin_resonance_query_t* out_queries);

//=============================================================================
// Core API: Similarity Search
//=============================================================================

/**
 * @brief Find similar past patterns
 *
 * Searches stored patterns for those most similar to the query,
 * using the configured similarity metric weights.
 *
 * @param bridge Bridge instance
 * @param query Query to match
 * @param out_results Output array of results
 * @param max_results Maximum results to return
 * @param out_count Actual number of results
 * @return FIN_RESONANCE_ERR_OK on success
 */
int financial_resonance_bridge_find_similar(
    financial_resonance_bridge_t* bridge,
    const fin_resonance_query_t* query,
    fin_resonance_pattern_t* out_results,
    uint32_t max_results,
    uint32_t* out_count);

/**
 * @brief Compute similarity between two queries
 *
 * @param bridge Bridge instance
 * @param query1 First query
 * @param query2 Second query
 * @param out_result Similarity result
 * @return FIN_RESONANCE_ERR_OK on success
 */
int financial_resonance_bridge_compute_similarity(
    financial_resonance_bridge_t* bridge,
    const fin_resonance_query_t* query1,
    const fin_resonance_query_t* query2,
    fin_resonance_result_t* out_result);

//=============================================================================
// Core API: Kuramoto Coherence
//=============================================================================

/**
 * @brief Compute cross-asset synchronization using Kuramoto model
 *
 * Analyzes the phase coherence across multiple assets to detect
 * market-wide synchronization phenomena (e.g., correlated selloffs).
 *
 * @param bridge Bridge instance
 * @param input Kuramoto input with asset phases and coupling
 * @param out_coherence Output coherence metrics
 * @return FIN_RESONANCE_ERR_OK on success
 */
int financial_resonance_bridge_kuramoto_coherence(
    financial_resonance_bridge_t* bridge,
    const fin_kuramoto_input_t* input,
    fin_kuramoto_output_t* out_coherence);

/**
 * @brief Quick coherence check (order parameter only)
 *
 * @param bridge Bridge instance
 * @param phases Array of phases
 * @param count Number of phases
 * @param out_order_param Output order parameter
 * @return FIN_RESONANCE_ERR_OK on success
 */
int financial_resonance_bridge_quick_coherence(
    financial_resonance_bridge_t* bridge,
    const float* phases,
    uint32_t count,
    float* out_order_param);

//=============================================================================
// Pattern Storage
//=============================================================================

/**
 * @brief Store a pattern with outcome
 *
 * @param bridge Bridge instance
 * @param query Pattern query
 * @param outcome Historical outcome
 * @param importance Pattern importance
 * @param label Optional label (can be NULL)
 * @return FIN_RESONANCE_ERR_OK on success
 */
int financial_resonance_bridge_store_pattern(
    financial_resonance_bridge_t* bridge,
    const fin_resonance_query_t* query,
    float outcome,
    float importance,
    const char* label);

/**
 * @brief Consolidate pattern storage (prune low-importance patterns)
 *
 * @param bridge Bridge instance
 * @return FIN_RESONANCE_ERR_OK on success
 */
int financial_resonance_bridge_consolidate(financial_resonance_bridge_t* bridge);

/**
 * @brief Get number of stored patterns
 *
 * @param bridge Bridge instance
 * @return Pattern count
 */
uint32_t financial_resonance_bridge_get_pattern_count(
    const financial_resonance_bridge_t* bridge);

//=============================================================================
// Health & Modulation
//=============================================================================

/**
 * @brief Check bridge health status
 * @param bridge Bridge instance
 * @return FIN_RESONANCE_ERR_OK if healthy
 */
int financial_resonance_bridge_check_health(financial_resonance_bridge_t* bridge);

/**
 * @brief Send heartbeat signal
 * @param bridge Bridge instance
 * @param operation Current operation name
 * @param progress Progress [0,1]
 * @return FIN_RESONANCE_ERR_OK on success
 */
int financial_resonance_bridge_heartbeat(
    financial_resonance_bridge_t* bridge,
    const char* operation,
    float progress);

/** Set inflammation level [0,1] */
int financial_resonance_bridge_set_inflammation(financial_resonance_bridge_t* bridge, float level);

/** Set fatigue level [0,1] */
int financial_resonance_bridge_set_fatigue(financial_resonance_bridge_t* bridge, float level);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge instance
 * @param out_stats Output statistics
 * @return FIN_RESONANCE_ERR_OK on success
 */
int financial_resonance_bridge_get_stats(
    const financial_resonance_bridge_t* bridge,
    fin_resonance_bridge_stats_t* out_stats);

/**
 * @brief Reset statistics counters
 * @param bridge Bridge instance
 */
void financial_resonance_bridge_reset_stats(financial_resonance_bridge_t* bridge);

/**
 * @brief Get last error message (thread-local)
 * @return Error message string
 */
const char* financial_resonance_bridge_get_last_error(void);

//=============================================================================
// Global Health Agent (for module-level heartbeats)
//=============================================================================

/**
 * @brief Set global health agent for module-level heartbeats
 * @param agent Health agent (can be NULL to disable)
 */
void financial_resonance_bridge_set_health_agent_global(void* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_RESONANCE_BRIDGE_H */
