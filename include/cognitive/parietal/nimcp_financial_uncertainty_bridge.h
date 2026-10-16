//=============================================================================
// nimcp_financial_uncertainty_bridge.h - Financial Uncertainty Decomposition Bridge
//=============================================================================
/**
 * @file nimcp_financial_uncertainty_bridge.h
 * @brief Uncertainty decomposition for financial decision making
 *
 * WHAT: Implements uncertainty decomposition framework for financial analysis,
 *       separating epistemic (reducible through information) from aleatoric
 *       (irreducible random) uncertainty components.
 *
 * WHY:  Financial decisions require understanding the nature of uncertainty:
 *       - Epistemic uncertainty: Can be reduced by gathering more data/info
 *       - Aleatoric uncertainty: Intrinsic randomness, cannot be reduced
 *       This distinction guides whether to wait for more information or act now.
 *
 * HOW:  The bridge analyzes prediction distributions to decompose total
 *       uncertainty into its components. Uses variance decomposition techniques
 *       and information-theoretic measures to quantify each type.
 *
 * ARCHITECTURE:
 * ```
 * +------------------------------------------------------------------+
 * |              FINANCIAL UNCERTAINTY BRIDGE                          |
 * +------------------------------------------------------------------+
 * |                                                                   |
 * |   +-------------------+    +-------------------+                  |
 * |   |  PREDICTION INPUT |    |  UNCERTAINTY      |                  |
 * |   |                   |    |  DECOMPOSITION    |                  |
 * |   | Values[]          |    |                   |                  |
 * |   | Confidences[]     |    | Epistemic         |                  |
 * |   | Count             |    | Aleatoric         |                  |
 * |   +--------+----------+    | Total             |                  |
 * |            |               +--------+----------+                  |
 * |            v                        |                             |
 * |   +------------------------------------------+                    |
 * |   |     VARIANCE DECOMPOSITION               |                    |
 * |   |   Total = Epistemic + Aleatoric          |                    |
 * |   |                                          |                    |
 * |   | Epistemic: Var(E[Y|X]) - model spread   |                    |
 * |   | Aleatoric: E[Var(Y|X)] - intrinsic noise |                    |
 * |   +------------------------------------------+                    |
 * |            |                        |                             |
 * |            v                        v                             |
 * |   +-------------------+    +-------------------+                  |
 * |   | DECISION SUPPORT  |    | INFO GATHERING    |                  |
 * |   | Should gather     |    | Recommendations   |                  |
 * |   | more info?        |    | What info?        |                  |
 * |   +-------------------+    +-------------------+                  |
 * |                                                                   |
 * +------------------------------------------------------------------+
 * ```
 *
 * THEORETICAL FOUNDATION:
 * =======================
 * Total Variance = E[Var(Y|X)] + Var(E[Y|X])
 *                = Aleatoric    + Epistemic
 *
 * Where:
 * - Y is the quantity of interest (e.g., future price)
 * - X represents available information/features
 * - E[Var(Y|X)] is expected variance within each prediction (noise)
 * - Var(E[Y|X]) is variance of mean predictions (model uncertainty)
 *
 * Information Value:
 * - High epistemic + low aleatoric = gather more info before deciding
 * - Low epistemic + high aleatoric = info won't help, decide now
 * - High both = complex situation, proceed with caution
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#ifndef NIMCP_FINANCIAL_UNCERTAINTY_BRIDGE_H
#define NIMCP_FINANCIAL_UNCERTAINTY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Bio-async module identifier for financial uncertainty bridge */
#define BIO_MODULE_FINANCIAL_UNCERTAINTY     0x03A2

/** Maximum number of predictions for decomposition */
#define FIN_UNCERTAINTY_MAX_PREDICTIONS      1024

/** Maximum ensemble size for decomposition */
#define FIN_UNCERTAINTY_MAX_ENSEMBLE         64

/** Maximum info sources to recommend */
#define FIN_UNCERTAINTY_MAX_INFO_SOURCES     16

//=============================================================================
// Error Codes
//=============================================================================

#define FIN_UNCERTAINTY_ERROR_BASE           36000
#define FIN_UNCERTAINTY_ERR_OK               0
#define FIN_UNCERTAINTY_ERR_NULL             (FIN_UNCERTAINTY_ERROR_BASE + 1)
#define FIN_UNCERTAINTY_ERR_INVALID_PARAM    (FIN_UNCERTAINTY_ERROR_BASE + 2)
#define FIN_UNCERTAINTY_ERR_NO_MEMORY        (FIN_UNCERTAINTY_ERROR_BASE + 3)
#define FIN_UNCERTAINTY_ERR_NOT_INITIALIZED  (FIN_UNCERTAINTY_ERROR_BASE + 4)
#define FIN_UNCERTAINTY_ERR_DECOMPOSITION    (FIN_UNCERTAINTY_ERROR_BASE + 5)
#define FIN_UNCERTAINTY_ERR_SUBSYSTEM        (FIN_UNCERTAINTY_ERROR_BASE + 6)
#define FIN_UNCERTAINTY_ERR_VALIDATION       (FIN_UNCERTAINTY_ERROR_BASE + 7)
#define FIN_UNCERTAINTY_ERR_INSUFFICIENT_DATA (FIN_UNCERTAINTY_ERROR_BASE + 8)

//=============================================================================
// Enumerations
//=============================================================================

/** Bridge operational state */
typedef enum {
    FIN_UNCERTAINTY_STATE_UNINITIALIZED = 0,
    FIN_UNCERTAINTY_STATE_IDLE,
    FIN_UNCERTAINTY_STATE_DECOMPOSING,
    FIN_UNCERTAINTY_STATE_ANALYZING,
    FIN_UNCERTAINTY_STATE_ERROR
} fin_uncertainty_op_state_t;

/** Information source types for recommendations */
typedef enum {
    FIN_INFO_SOURCE_NONE = 0,
    FIN_INFO_SOURCE_FUNDAMENTAL,    /**< Financial statements, earnings */
    FIN_INFO_SOURCE_TECHNICAL,      /**< Price/volume patterns */
    FIN_INFO_SOURCE_SENTIMENT,      /**< News, social media */
    FIN_INFO_SOURCE_MACROECONOMIC,  /**< GDP, inflation, rates */
    FIN_INFO_SOURCE_INDUSTRY,       /**< Sector-specific data */
    FIN_INFO_SOURCE_INSIDER,        /**< Insider trading filings */
    FIN_INFO_SOURCE_ANALYST,        /**< Analyst estimates */
    FIN_INFO_SOURCE_OPTIONS,        /**< Options market implied data */
    FIN_INFO_SOURCE_FLOW,           /**< Order flow, institutional */
    FIN_INFO_SOURCE_COUNT
} fin_info_source_t;

/** Decision guidance based on uncertainty */
typedef enum {
    FIN_DECISION_WAIT = 0,          /**< Wait for more information */
    FIN_DECISION_ACT_CAUTIOUSLY,    /**< Act with small position */
    FIN_DECISION_ACT_CONFIDENTLY,   /**< Act with full conviction */
    FIN_DECISION_HEDGE,             /**< Act but hedge uncertainty */
    FIN_DECISION_PASS               /**< Too uncertain, pass entirely */
} fin_decision_guidance_t;

//=============================================================================
// Data Structures (as specified by user)
//=============================================================================

/**
 * @brief Decomposed uncertainty result
 *
 * Contains the decomposition of total uncertainty into its
 * epistemic (reducible) and aleatoric (irreducible) components.
 */
typedef struct {
    float epistemic;          /**< Reducible uncertainty (knowledge gap) */
    float aleatoric;          /**< Irreducible uncertainty (randomness) */
    float total;              /**< Combined uncertainty */
} fin_uncertainty_t;

/**
 * @brief Prediction input for uncertainty decomposition
 *
 * Contains multiple predictions (e.g., from ensemble models)
 * with associated confidence values for decomposition analysis.
 */
typedef struct {
    float* values;            /**< Predicted values array */
    float* confidences;       /**< Confidence for each prediction [0-1] */
    uint32_t count;           /**< Number of predictions */
} fin_prediction_t;

/**
 * @brief Bridge statistics for monitoring and diagnostics
 */
typedef struct {
    uint64_t decompositions;          /**< Total decomposition operations */
    uint64_t info_gather_recommendations; /**< Info gathering recommendations made */
    uint64_t immune_checks;           /**< Immune system validations */
    uint64_t bbb_validations;         /**< Blood-brain barrier validations */
    uint64_t kg_messages_sent;        /**< Knowledge graph messages sent */
    uint64_t health_heartbeats;       /**< Health agent heartbeats */
} fin_uncertainty_bridge_stats_t;

//=============================================================================
// Extended Data Structures
//=============================================================================

/**
 * @brief Information gathering recommendation
 */
typedef struct {
    fin_info_source_t source;         /**< Recommended information source */
    float expected_uncertainty_reduction; /**< Expected reduction in epistemic [0-1] */
    float cost;                       /**< Cost/effort to obtain [0-1] */
    float value_of_information;       /**< Expected value of getting this info */
    char description[128];            /**< Human-readable description */
} fin_info_recommendation_t;

/**
 * @brief Extended uncertainty analysis result
 */
typedef struct {
    fin_uncertainty_t uncertainty;    /**< Core decomposition */

    /* Derived metrics */
    float epistemic_ratio;            /**< epistemic / total [0-1] */
    float aleatoric_ratio;            /**< aleatoric / total [0-1] */
    float confidence;                 /**< Confidence in decomposition */

    /* Decision guidance */
    fin_decision_guidance_t guidance; /**< Recommended decision approach */
    bool should_gather_info;          /**< Should gather more info? */

    /* Info recommendations */
    fin_info_recommendation_t* recommendations; /**< Info source recommendations */
    uint32_t num_recommendations;     /**< Number of recommendations */

    /* Analysis details */
    float mean_prediction;            /**< Mean of input predictions */
    float std_prediction;             /**< Std dev of input predictions */
    float mean_confidence;            /**< Mean input confidence */
} fin_uncertainty_analysis_t;

/**
 * @brief Ensemble prediction for variance decomposition
 */
typedef struct {
    float** ensemble_predictions;     /**< [ensemble_size][num_samples] */
    float** ensemble_variances;       /**< Per-sample variance from each model */
    uint32_t ensemble_size;           /**< Number of ensemble members */
    uint32_t num_samples;             /**< Number of samples per member */
} fin_ensemble_prediction_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Bridge configuration structure
 */
typedef struct {
    /* Decomposition settings */
    float min_predictions;            /**< Minimum predictions for valid decomp */
    float confidence_threshold;       /**< Minimum confidence to include */
    bool use_weighted_decomposition;  /**< Weight by confidence values */

    /* Info gathering thresholds */
    float info_gathering_threshold;   /**< Epistemic ratio to recommend info */
    float act_threshold;              /**< Below this total, act confidently */
    float pass_threshold;             /**< Above this total, pass entirely */

    /* Modulation sensitivity */
    float inflammation_sensitivity;   /**< Sensitivity to inflammation [0-2] */
    float fatigue_sensitivity;        /**< Sensitivity to fatigue [0-2] */

    /* Security settings */
    bool enable_bbb_validation;       /**< Enable blood-brain barrier checks */
    bool enable_immune_validation;    /**< Enable immune system checks */
} fin_uncertainty_config_t;

//=============================================================================
// Opaque Handle
//=============================================================================

/** Opaque bridge handle */
typedef struct financial_uncertainty_bridge financial_uncertainty_bridge_t;

//=============================================================================
// Forward Declarations for Subsystems
//=============================================================================

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

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default configuration with sensible values
 */
fin_uncertainty_config_t financial_uncertainty_bridge_default_config(void);

/**
 * @brief Create financial uncertainty bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
financial_uncertainty_bridge_t* financial_uncertainty_bridge_create(
    const fin_uncertainty_config_t* config);

/**
 * @brief Destroy bridge and free resources
 * @param bridge Bridge handle
 */
void financial_uncertainty_bridge_destroy(financial_uncertainty_bridge_t* bridge);

/**
 * @brief Get current bridge state
 * @param bridge Bridge handle
 * @return Current operational state
 */
fin_uncertainty_op_state_t financial_uncertainty_bridge_get_state(
    const financial_uncertainty_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_uncertainty_bridge_reset(financial_uncertainty_bridge_t* bridge);

//=============================================================================
// Subsystem Setters
//=============================================================================

/**
 * @brief Set immune system for validation
 * @param bridge Bridge handle
 * @param immune Immune system handle (NULL to disable)
 * @return 0 on success
 */
int financial_uncertainty_bridge_set_immune(financial_uncertainty_bridge_t* bridge,
                                             void* immune);

/**
 * @brief Set blood-brain barrier for data validation
 * @param bridge Bridge handle
 * @param bbb BBB handle (NULL to disable)
 * @return 0 on success
 */
int financial_uncertainty_bridge_set_bbb(financial_uncertainty_bridge_t* bridge,
                                          bbb_system_t bbb);

/**
 * @brief Enable/disable BBB validation
 * @param bridge Bridge handle
 * @param enable True to enable validation
 * @return 0 on success
 */
int financial_uncertainty_bridge_enable_bbb_validation(
    financial_uncertainty_bridge_t* bridge, bool enable);

/**
 * @brief Enable/disable immune validation
 * @param bridge Bridge handle
 * @param enable True to enable validation
 * @return 0 on success
 */
int financial_uncertainty_bridge_enable_immune_validation(
    financial_uncertainty_bridge_t* bridge, bool enable);

/**
 * @brief Set KG wiring for inter-module communication
 * @param bridge Bridge handle
 * @param kg KG wiring handle
 * @return 0 on success
 */
int financial_uncertainty_bridge_set_kg_wiring(financial_uncertainty_bridge_t* bridge,
                                                void* kg);

/**
 * @brief Set health agent for heartbeat monitoring
 * @param bridge Bridge handle
 * @param health_agent Health agent handle
 * @return 0 on success
 */
int financial_uncertainty_bridge_set_health_agent(financial_uncertainty_bridge_t* bridge,
                                                   void* health_agent);

/**
 * @brief Set logger for debug/trace output
 * @param bridge Bridge handle
 * @param logger Logger handle
 * @return 0 on success
 */
int financial_uncertainty_bridge_set_logger(financial_uncertainty_bridge_t* bridge,
                                             void* logger);

/**
 * @brief Set security handle
 * @param bridge Bridge handle
 * @param security Security handle
 * @return 0 on success
 */
int financial_uncertainty_bridge_set_security(financial_uncertainty_bridge_t* bridge,
                                               void* security);

/**
 * @brief Set ethics engine handle
 * @param bridge Bridge handle
 * @param ethics Ethics engine handle
 * @return 0 on success
 */
int financial_uncertainty_bridge_set_ethics(financial_uncertainty_bridge_t* bridge,
                                             ethics_engine_t ethics);

/**
 * @brief Set LGSS handle
 * @param bridge Bridge handle
 * @param lgss LGSS handle
 * @return 0 on success
 */
int financial_uncertainty_bridge_set_lgss(financial_uncertainty_bridge_t* bridge,
                                           const void* lgss);

/**
 * @brief Set cycle coordinator handle
 * @param bridge Bridge handle
 * @param coordinator Cycle coordinator handle
 * @return 0 on success
 */
int financial_uncertainty_bridge_set_coordinator(financial_uncertainty_bridge_t* bridge,
                                                  brain_cycle_coordinator_t* coordinator);

/**
 * @brief Set bio router handle
 * @param bridge Bridge handle
 * @param bio_router Bio router handle
 * @return 0 on success
 */
int financial_uncertainty_bridge_set_bio_router(financial_uncertainty_bridge_t* bridge,
                                                 void* bio_router);

//=============================================================================
// Core Uncertainty Decomposition API
//=============================================================================

/**
 * @brief Decompose uncertainty into epistemic and aleatoric components
 *
 * Given a set of predictions with confidences, decomposes the total
 * uncertainty into:
 * - Epistemic: Variance between predictions (reducible by more data)
 * - Aleatoric: Average variance within predictions (irreducible noise)
 *
 * @param bridge Bridge handle
 * @param prediction Input predictions with confidences
 * @param uncertainty Output decomposed uncertainty
 * @return 0 on success, error code on failure
 */
int financial_uncertainty_bridge_decompose(
    financial_uncertainty_bridge_t* bridge,
    const fin_prediction_t* prediction,
    fin_uncertainty_t* uncertainty);

/**
 * @brief Decompose from ensemble predictions (full variance decomposition)
 *
 * More accurate decomposition using ensemble model outputs where each
 * ensemble member provides both predictions and variance estimates.
 *
 * @param bridge Bridge handle
 * @param ensemble Ensemble predictions
 * @param uncertainty Output decomposed uncertainty
 * @return 0 on success, error code on failure
 */
int financial_uncertainty_bridge_decompose_ensemble(
    financial_uncertainty_bridge_t* bridge,
    const fin_ensemble_prediction_t* ensemble,
    fin_uncertainty_t* uncertainty);

/**
 * @brief Determine if more information should be gathered
 *
 * Analyzes the uncertainty decomposition to recommend whether to
 * gather more information before making a decision.
 *
 * @param bridge Bridge handle
 * @param uncertainty Current uncertainty decomposition
 * @param should_gather Output: true if info gathering recommended
 * @return 0 on success, error code on failure
 */
int financial_uncertainty_bridge_should_gather_info(
    financial_uncertainty_bridge_t* bridge,
    const fin_uncertainty_t* uncertainty,
    bool* should_gather);

/**
 * @brief Full uncertainty analysis with recommendations
 *
 * Performs complete analysis including decomposition, decision guidance,
 * and information gathering recommendations.
 *
 * @param bridge Bridge handle
 * @param prediction Input predictions
 * @param analysis Output full analysis (caller must free recommendations)
 * @return 0 on success, error code on failure
 */
int financial_uncertainty_bridge_analyze(
    financial_uncertainty_bridge_t* bridge,
    const fin_prediction_t* prediction,
    fin_uncertainty_analysis_t* analysis);

/**
 * @brief Get decision guidance based on uncertainty
 *
 * @param bridge Bridge handle
 * @param uncertainty Current uncertainty
 * @param guidance Output decision guidance
 * @return 0 on success, error code on failure
 */
int financial_uncertainty_bridge_get_guidance(
    financial_uncertainty_bridge_t* bridge,
    const fin_uncertainty_t* uncertainty,
    fin_decision_guidance_t* guidance);

/**
 * @brief Get information source recommendations
 *
 * @param bridge Bridge handle
 * @param uncertainty Current uncertainty
 * @param recommendations Output array (caller allocates)
 * @param max_recommendations Max recommendations to return
 * @param num_recommendations Output: actual number returned
 * @return 0 on success, error code on failure
 */
int financial_uncertainty_bridge_recommend_info(
    financial_uncertainty_bridge_t* bridge,
    const fin_uncertainty_t* uncertainty,
    fin_info_recommendation_t* recommendations,
    uint32_t max_recommendations,
    uint32_t* num_recommendations);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Allocate prediction structure
 * @param count Number of predictions
 * @return Allocated structure or NULL on error
 */
fin_prediction_t* financial_uncertainty_prediction_create(uint32_t count);

/**
 * @brief Free prediction structure
 * @param prediction Prediction to free
 */
void financial_uncertainty_prediction_destroy(fin_prediction_t* prediction);

/**
 * @brief Allocate analysis structure
 * @param max_recommendations Max recommendations to allocate
 * @return Allocated structure or NULL on error
 */
fin_uncertainty_analysis_t* financial_uncertainty_analysis_create(
    uint32_t max_recommendations);

/**
 * @brief Free analysis structure
 * @param analysis Analysis to free
 */
void financial_uncertainty_analysis_destroy(fin_uncertainty_analysis_t* analysis);

/**
 * @brief Free recommendations in analysis (but not analysis itself)
 * @param analysis Analysis containing recommendations
 */
void financial_uncertainty_analysis_free_recommendations(
    fin_uncertainty_analysis_t* analysis);

//=============================================================================
// Modulation API
//=============================================================================

/**
 * @brief Set inflammation level (affects analysis conservatism)
 * @param bridge Bridge handle
 * @param level Inflammation level [0-1]
 * @return 0 on success
 */
int financial_uncertainty_bridge_set_inflammation(
    financial_uncertainty_bridge_t* bridge, float level);

/**
 * @brief Set fatigue level (affects analysis precision)
 * @param bridge Bridge handle
 * @param level Fatigue level [0-1]
 * @return 0 on success
 */
int financial_uncertainty_bridge_set_fatigue(
    financial_uncertainty_bridge_t* bridge, float level);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics (caller allocates)
 * @return 0 on success
 */
int financial_uncertainty_bridge_get_stats(
    const financial_uncertainty_bridge_t* bridge,
    fin_uncertainty_bridge_stats_t* stats);

/**
 * @brief Reset statistics counters
 * @param bridge Bridge handle
 */
void financial_uncertainty_bridge_reset_stats(financial_uncertainty_bridge_t* bridge);

/**
 * @brief Get last error message
 * @return Thread-local error message string
 */
const char* financial_uncertainty_bridge_get_last_error(void);

//=============================================================================
// Health Integration
//=============================================================================

/**
 * @brief Set global health agent for module-level heartbeats
 * @param agent Health agent (can be NULL to disable)
 */
void financial_uncertainty_bridge_set_health_agent_global(void* agent);

//=============================================================================
// String Conversion Utilities
//=============================================================================

/**
 * @brief Get string name for operational state
 * @param state Operational state
 * @return Static string name
 */
const char* fin_uncertainty_state_name(fin_uncertainty_op_state_t state);

/**
 * @brief Get string name for info source type
 * @param source Info source type
 * @return Static string name
 */
const char* fin_uncertainty_info_source_name(fin_info_source_t source);

/**
 * @brief Get string name for decision guidance
 * @param guidance Decision guidance
 * @return Static string name
 */
const char* fin_uncertainty_guidance_name(fin_decision_guidance_t guidance);

/**
 * @brief Get bridge version string
 * @return Version string
 */
const char* financial_uncertainty_bridge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_UNCERTAINTY_BRIDGE_H */
