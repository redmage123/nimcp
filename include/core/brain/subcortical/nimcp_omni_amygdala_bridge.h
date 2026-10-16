/**
 * @file nimcp_omni_amygdala_bridge.h
 * @brief Omnidirectional Inference to Amygdala Bridge
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: Bridge integrating omnidirectional inference with amygdala
 * WHY:  Enable emotional modulation of inference and fear-based learning
 * HOW:  Prediction errors trigger fear responses, threat detection modulates inference
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * EMOTION-COGNITION INTERACTION:
 * ------------------------------
 * The amygdala modulates cognitive processes bidirectionally:
 *
 *   1. PREDICTION ERROR → EMOTIONAL TAGGING:
 *      - Large unexpected PE → Fear/surprise activation
 *      - Persistent PE → Anxiety state
 *      - PE in threat-relevant domains → Enhanced fear conditioning
 *
 *   2. EMOTIONAL STATE → INFERENCE MODULATION:
 *      - High fear → Focus on threat-relevant predictions
 *      - Anxiety → Reduced backward inference (present-focused)
 *      - Low threat → Enhanced exploratory inference
 *
 *   3. FEAR CONDITIONING + PREDICTION:
 *      - CS-US association via predictive learning
 *      - Backward inference identifies threat causes
 *      - Forward inference predicts threat outcomes
 *
 * THREAT-INFERENCE MAPPING:
 * -------------------------
 *   Threat Level       →  Inference Strategy
 *   ─────────────────────────────────────────────
 *   None              →  Full bidirectional inference
 *   Low               →  Standard inference, slight forward bias
 *   Moderate          →  Forward-focused, threat monitoring
 *   High              →  Immediate forward only, freeze response
 *   Extreme           →  Override to escape/avoidance
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * - Lateral amygdala: Receives sensory predictions, computes threat PE
 * - Basal amygdala: Contextual modulation of predictions
 * - Central amygdala: Outputs that modulate inference strategy
 * - ITC cells: Extinction learning for safe predictions
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OMNI_AMYGDALA_BRIDGE_H
#define NIMCP_OMNI_AMYGDALA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct omni_amygdala_bridge omni_amygdala_bridge_t;
typedef struct jepa_bidirectional jepa_bidirectional_t;
typedef struct predictive_hierarchy predictive_hierarchy_t;
typedef struct hopfield_memory hopfield_memory_t;
typedef struct amygdala amygdala_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for omni-amygdala bridge */
#define BIO_MODULE_OMNI_AMYGDALA_BRIDGE        0x0E54

/** @brief Default PE threshold for fear activation */
#define OMNI_AMYG_FEAR_PE_THRESHOLD            3.0f

/** @brief Default threat detection sensitivity */
#define OMNI_AMYG_DEFAULT_SENSITIVITY          0.5f

/** @brief Anxiety threshold for inference suppression */
#define OMNI_AMYG_ANXIETY_SUPPRESS_THRESHOLD   0.7f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Threat level from omni inference
 */
typedef enum {
    OMNI_THREAT_NONE = 0,            /**< No threat detected */
    OMNI_THREAT_LOW,                 /**< Minor anomaly */
    OMNI_THREAT_MODERATE,            /**< Significant threat signal */
    OMNI_THREAT_HIGH,                /**< Immediate threat */
    OMNI_THREAT_EXTREME              /**< Overwhelming threat */
} omni_threat_level_t;

/**
 * @brief Emotional modulation mode
 */
typedef enum {
    OMNI_EMO_NEUTRAL = 0,            /**< No emotional modulation */
    OMNI_EMO_VIGILANT,               /**< Heightened threat monitoring */
    OMNI_EMO_ANXIOUS,                /**< Anxiety-driven inference */
    OMNI_EMO_FEARFUL,                /**< Fear-driven forward focus */
    OMNI_EMO_SAFE                    /**< Safety signal, exploratory */
} omni_emotional_mode_t;

/**
 * @brief Fear memory type
 */
typedef enum {
    OMNI_FEAR_NONE = 0,              /**< No fear association */
    OMNI_FEAR_CONDITIONED,           /**< Learned fear (CS-US) */
    OMNI_FEAR_CONTEXTUAL,            /**< Context-dependent fear */
    OMNI_FEAR_GENERALIZED            /**< Generalized fear response */
} omni_fear_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Omni effects on amygdala
 */
typedef struct {
    float pe_magnitude;              /**< Prediction error magnitude */
    omni_threat_level_t threat_level; /**< Detected threat level */
    float threat_confidence;         /**< Confidence in threat detection */
    float* threat_pattern;           /**< Pattern triggering threat */
    uint32_t pattern_dim;            /**< Pattern dimension */
    bool is_novel;                   /**< Is this a novel threat? */
    omni_fear_type_t fear_type;      /**< Type of fear response */
} omni_to_amygdala_effects_t;

/**
 * @brief Amygdala effects on omni inference
 */
typedef struct {
    omni_emotional_mode_t mode;      /**< Current emotional mode */
    float fear_level;                /**< Current fear level [0-1] */
    float anxiety_level;             /**< Current anxiety level [0-1] */
    float forward_bias;              /**< Bias toward forward inference */
    float precision_boost;           /**< Precision boost for threats */
    bool suppress_backward;          /**< Suppress backward inference */
    bool suppress_lateral;           /**< Suppress lateral inference */
    float* threat_priors;            /**< Prior probabilities for threats */
    uint32_t num_priors;             /**< Number of threat priors */
} amygdala_to_omni_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Threat detection */
    float fear_pe_threshold;         /**< PE threshold for fear */
    float threat_sensitivity;        /**< Threat detection sensitivity */
    float anxiety_threshold;         /**< Anxiety threshold */

    /* Modulation parameters */
    float max_forward_bias;          /**< Maximum forward inference bias */
    float max_precision_boost;       /**< Maximum precision boost */
    bool allow_backward_suppression; /**< Allow backward suppression */

    /* Fear conditioning */
    bool enable_fear_conditioning;   /**< Enable fear learning */
    float conditioning_rate;         /**< Fear conditioning rate */
    float extinction_rate;           /**< Fear extinction rate */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_logging;             /**< Enable logging */
} omni_amygdala_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;          /**< Total bridge updates */
    uint64_t threat_detections;      /**< Total threat detections */
    uint64_t fear_activations;       /**< Fear activation count */
    uint64_t anxiety_episodes;       /**< Anxiety episode count */
    uint64_t backward_suppressions;  /**< Backward suppression count */
    float avg_fear_level;            /**< Average fear level */
    float avg_anxiety_level;         /**< Average anxiety level */
    float max_threat_level;          /**< Maximum threat level seen */
} omni_amygdala_stats_t;

/**
 * @brief Omni-amygdala bridge structure
 */
struct omni_amygdala_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge */

    omni_amygdala_config_t config;   /**< Configuration */

    /* Connected systems */
    jepa_bidirectional_t* jepa;      /**< Bidirectional JEPA */
    predictive_hierarchy_t* pred_hier; /**< Predictive hierarchy */
    hopfield_memory_t* hopfield;     /**< Hopfield for fear memories */
    amygdala_t* amygdala;            /**< Amygdala system */

    /* Computed effects */
    omni_to_amygdala_effects_t omni_effects;     /**< Omni → amygdala */
    amygdala_to_omni_effects_t amygdala_effects; /**< Amygdala → omni */

    /* Statistics */
    omni_amygdala_stats_t stats;

    /* Bio-async integration */
    void* bio_context;               /**< Bio-async module context */
    bool bio_async_connected;        /**< Bio-async connection state */

    /* Thread safety */
    void* mutex;
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int omni_amygdala_default_config(omni_amygdala_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create omni-amygdala bridge
 */
omni_amygdala_bridge_t* omni_amygdala_bridge_create(
    const omni_amygdala_config_t* config);

/**
 * @brief Destroy bridge
 */
void omni_amygdala_bridge_destroy(omni_amygdala_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int omni_amygdala_connect_jepa(omni_amygdala_bridge_t* bridge,
                                jepa_bidirectional_t* jepa);

int omni_amygdala_connect_pred_hier(omni_amygdala_bridge_t* bridge,
                                     predictive_hierarchy_t* pred_hier);

int omni_amygdala_connect_hopfield(omni_amygdala_bridge_t* bridge,
                                    hopfield_memory_t* hopfield);

int omni_amygdala_connect_amygdala(omni_amygdala_bridge_t* bridge,
                                    amygdala_t* amygdala);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update bridge
 */
int omni_amygdala_update(omni_amygdala_bridge_t* bridge);

/**
 * @brief Apply omni effects to amygdala
 */
int omni_amygdala_apply_to_amygdala(omni_amygdala_bridge_t* bridge);

/**
 * @brief Apply amygdala effects to omni
 */
int omni_amygdala_apply_to_omni(omni_amygdala_bridge_t* bridge);

/* ============================================================================
 * Threat Detection API
 * ============================================================================ */

/**
 * @brief Get current threat level
 */
omni_threat_level_t omni_amygdala_get_threat_level(
    const omni_amygdala_bridge_t* bridge);

/**
 * @brief Get current emotional mode
 */
omni_emotional_mode_t omni_amygdala_get_emotional_mode(
    const omni_amygdala_bridge_t* bridge);

/**
 * @brief Check if backward inference should be suppressed
 */
bool omni_amygdala_should_suppress_backward(
    const omni_amygdala_bridge_t* bridge);

/**
 * @brief Get forward inference bias
 */
float omni_amygdala_get_forward_bias(const omni_amygdala_bridge_t* bridge);

/* ============================================================================
 * Fear Conditioning API
 * ============================================================================ */

/**
 * @brief Condition fear to a pattern
 *
 * @param bridge Bridge
 * @param pattern Pattern to condition [dim]
 * @param dim Pattern dimension
 * @param strength Fear strength [0-1]
 * @return NIMCP_SUCCESS on success
 */
int omni_amygdala_condition_fear(omni_amygdala_bridge_t* bridge,
                                  const float* pattern,
                                  uint32_t dim,
                                  float strength);

/**
 * @brief Extinguish fear to a pattern
 *
 * @param bridge Bridge
 * @param pattern Pattern to extinguish [dim]
 * @param dim Pattern dimension
 * @return NIMCP_SUCCESS on success
 */
int omni_amygdala_extinguish_fear(omni_amygdala_bridge_t* bridge,
                                   const float* pattern,
                                   uint32_t dim);

/* ============================================================================
 * Query API
 * ============================================================================ */

int omni_amygdala_get_omni_effects(const omni_amygdala_bridge_t* bridge,
                                    omni_to_amygdala_effects_t* effects);

int omni_amygdala_get_amygdala_effects(const omni_amygdala_bridge_t* bridge,
                                        amygdala_to_omni_effects_t* effects);

int omni_amygdala_get_stats(const omni_amygdala_bridge_t* bridge,
                             omni_amygdala_stats_t* stats);

int omni_amygdala_reset_stats(omni_amygdala_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int omni_amygdala_connect_bio_async(omni_amygdala_bridge_t* bridge);
int omni_amygdala_disconnect_bio_async(omni_amygdala_bridge_t* bridge);
bool omni_amygdala_is_bio_async_connected(const omni_amygdala_bridge_t* bridge);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_amygdala_threat_to_string(omni_threat_level_t level);
const char* omni_amygdala_mode_to_string(omni_emotional_mode_t mode);
const char* omni_amygdala_fear_type_to_string(omni_fear_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_AMYGDALA_BRIDGE_H */
