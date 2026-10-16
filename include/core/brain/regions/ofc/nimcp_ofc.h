/**
 * @file nimcp_ofc.h
 * @brief Orbitofrontal Cortex (OFC) - Value-Based Decision Making
 *
 * WHAT: Neural substrate for value computation, economic decision-making,
 *       and emotion-cognition integration
 * WHY:  Critical for reward processing, adaptive behavior, and social decisions
 * HOW:  Implements expected value computation, reward prediction error,
 *       reversal learning, and risk assessment
 *
 * BIOLOGICAL BASIS:
 * - Receives inputs from all sensory modalities via association cortices
 * - Strong connections with amygdala, hypothalamus, and striatum
 * - Projects to prefrontal cortex for executive control
 * - Encodes economic value across different reward types
 *
 * SUBDIVISIONS IMPLEMENTED:
 * - Lateral OFC (lOFC): Stimulus-reward associations, reversal learning
 * - Medial OFC (mOFC): Value comparison, choice
 * - Anterior OFC (aOFC): Abstract/social rewards
 * - Posterior OFC (pOFC): Primary reward processing
 *
 * FULL INTEGRATION WITH:
 * - Security module, Immune system, KG wiring, Bio-async
 * - SNN/STDP/Plasticity, Hypothalamus, Omnidirectional
 * - Cognitive/Training layers, Perception, Symbolic logic
 * - Swarm, Dragonfly, Portia, Logging, Thalamic, Neural substrate
 * - Quantum algorithms (QMC, QMCTS), Math utilities, Threading
 *
 * @version 1.0
 * @date 2026-01-13
 */

#ifndef NIMCP_OFC_H
#define NIMCP_OFC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Platform and utilities */
#include "utils/platform/nimcp_platform_tier.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"

/* Forward declarations for integration */
struct nimcp_brain_kg;
struct nimcp_bio_router;
struct nimcp_immune_system;
struct nimcp_security_context;
struct nimcp_snn_network;
struct nimcp_plasticity_engine;
struct nimcp_hypothalamus;
struct nimcp_thalamus;
struct nimcp_cognitive_hub;
struct nimcp_training_context;
struct nimcp_perception_system;
struct nimcp_symbolic_engine;
struct nimcp_swarm_context;
struct nimcp_dragonfly_context;
struct nimcp_portia_context;
struct nimcp_qmc_context;
struct nimcp_omni_predictor;

/*=============================================================================
 * OFC SUBDIVISIONS
 *===========================================================================*/

/**
 * @brief OFC subdivisions with distinct functional roles
 */
typedef enum {
    OFC_SUBDIV_LATERAL = 0,     /**< Stimulus-reward, reversal learning */
    OFC_SUBDIV_MEDIAL,          /**< Value comparison, choice selection */
    OFC_SUBDIV_ANTERIOR,        /**< Abstract/social reward processing */
    OFC_SUBDIV_POSTERIOR,       /**< Primary reward, sensory integration */
    OFC_SUBDIV_COUNT
} ofc_subdivision_t;

/*=============================================================================
 * VALUE REPRESENTATION
 *===========================================================================*/

/**
 * @brief Value types processed by OFC
 */
typedef enum {
    OFC_VALUE_EXPECTED = 0,     /**< Expected value of outcome */
    OFC_VALUE_RECEIVED,         /**< Actually received value */
    OFC_VALUE_PREDICTION_ERROR, /**< Difference: received - expected */
    OFC_VALUE_PROBABILITY,      /**< Probability of reward */
    OFC_VALUE_MAGNITUDE,        /**< Magnitude of reward */
    OFC_VALUE_DELAY,            /**< Temporal discount factor */
    OFC_VALUE_RISK,             /**< Variance/uncertainty of outcome */
    OFC_VALUE_SOCIAL,           /**< Social reward component */
    OFC_VALUE_COUNT
} ofc_value_type_t;

/**
 * @brief Single value representation
 */
typedef struct {
    ofc_value_type_t type;
    float value;                /**< Computed value [-1, 1] normalized */
    float confidence;           /**< Confidence in estimate [0, 1] */
    float temporal_discount;    /**< Hyperbolic discount factor */
    uint64_t timestamp_us;      /**< When computed */
} ofc_value_t;

/**
 * @brief Complete value state for an option/stimulus
 */
typedef struct {
    uint32_t stimulus_id;       /**< Identifier for stimulus/option */
    ofc_value_t values[OFC_VALUE_COUNT];
    float integrated_value;     /**< Combined utility estimate */
    float choice_probability;   /**< Softmax probability of choosing */
    bool active;                /**< Currently being evaluated */
} ofc_option_state_t;

/*=============================================================================
 * DECISION MAKING
 *===========================================================================*/

/**
 * @brief Decision types
 */
typedef enum {
    OFC_DECISION_BINARY = 0,    /**< Two-alternative forced choice */
    OFC_DECISION_MULTI,         /**< Multiple alternatives */
    OFC_DECISION_SEQUENTIAL,    /**< Sequential sampling */
    OFC_DECISION_SOCIAL,        /**< Social/interpersonal decision */
    OFC_DECISION_MORAL          /**< Moral/ethical judgment */
} ofc_decision_type_t;

/**
 * @brief Decision outcome
 */
typedef struct {
    uint32_t chosen_option;     /**< Selected option ID */
    float decision_value;       /**< Value of chosen option */
    float confidence;           /**< Decision confidence */
    float reaction_time_ms;     /**< Simulated reaction time */
    ofc_decision_type_t type;   /**< Type of decision made */
    bool reversal_detected;     /**< Contingency reversal detected */
} ofc_decision_t;

/*=============================================================================
 * BIO-ASYNC MESSAGE TYPES
 *===========================================================================*/

/**
 * @brief OFC bio-async message types
 */
typedef enum {
    OFC_BIO_MSG_VALUE_UPDATE = 0,   /**< Value computation update */
    OFC_BIO_MSG_DECISION,           /**< Decision made */
    OFC_BIO_MSG_PREDICTION_ERROR,   /**< RPE signal broadcast */
    OFC_BIO_MSG_REVERSAL,           /**< Reversal learning triggered */
    OFC_BIO_MSG_RISK_ASSESSMENT,    /**< Risk evaluation result */
    OFC_BIO_MSG_SOCIAL_REWARD,      /**< Social reward signal */
    OFC_BIO_MSG_EMOTION_MODULATION, /**< Emotion affecting value */
    OFC_BIO_MSG_STATE_REQUEST,      /**< Request for OFC state */
    OFC_BIO_MSG_COUNT
} ofc_bio_msg_type_t;

/**
 * @brief Subscription masks for bio-async
 */
#define OFC_BIO_SUB_VALUE       (1U << OFC_BIO_MSG_VALUE_UPDATE)
#define OFC_BIO_SUB_DECISION    (1U << OFC_BIO_MSG_DECISION)
#define OFC_BIO_SUB_RPE         (1U << OFC_BIO_MSG_PREDICTION_ERROR)
#define OFC_BIO_SUB_REVERSAL    (1U << OFC_BIO_MSG_REVERSAL)
#define OFC_BIO_SUB_RISK        (1U << OFC_BIO_MSG_RISK_ASSESSMENT)
#define OFC_BIO_SUB_SOCIAL      (1U << OFC_BIO_MSG_SOCIAL_REWARD)
#define OFC_BIO_SUB_EMOTION     (1U << OFC_BIO_MSG_EMOTION_MODULATION)
#define OFC_BIO_SUB_ALL         (0xFFFFFFFFU)

/*=============================================================================
 * KG WIRING INTEGRATION
 *===========================================================================*/

/**
 * @brief KG node types for OFC
 */
typedef enum {
    OFC_KG_NODE_REGION = 0,     /**< OFC region node */
    OFC_KG_NODE_SUBDIVISION,    /**< Subdivision node */
    OFC_KG_NODE_VALUE_SIGNAL,   /**< Value computation node */
    OFC_KG_NODE_DECISION,       /**< Decision node */
    OFC_KG_NODE_CONNECTION      /**< Connection/edge node */
} ofc_kg_node_type_t;

/**
 * @brief KG wiring state for OFC
 */
typedef struct {
    uint64_t region_node_id;    /**< Main OFC node in KG */
    uint64_t subdiv_node_ids[OFC_SUBDIV_COUNT];
    uint64_t value_node_ids[OFC_VALUE_COUNT];
    uint32_t edge_count;        /**< Number of KG edges */
    bool registered;            /**< Registration complete */
    uint64_t admin_token;       /**< Security token for KG ops */
} ofc_kg_state_t;

/*=============================================================================
 * STATISTICS AND METRICS
 *===========================================================================*/

/**
 * @brief OFC operational statistics
 */
typedef struct {
    uint64_t decisions_made;
    uint64_t reversals_detected;
    uint64_t prediction_errors;
    float avg_decision_confidence;
    float avg_reaction_time_ms;
    float total_reward_received;
    float total_reward_expected;
    uint64_t bio_msgs_sent;
    uint64_t bio_msgs_received;
    uint64_t kg_updates;
    uint64_t immune_alerts;
} ofc_stats_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief OFC configuration
 */
typedef struct {
    /* Value computation */
    float learning_rate;            /**< RPE learning rate */
    float discount_rate;            /**< Temporal discounting (gamma) */
    float risk_sensitivity;         /**< Risk preference [-1=averse, 1=seeking] */
    float social_weight;            /**< Weight for social rewards */

    /* Decision making */
    float decision_threshold;       /**< Evidence threshold for decision */
    float noise_level;              /**< Decision noise (softmax temp) */
    uint32_t max_options;           /**< Maximum options to evaluate */
    float reversal_threshold;       /**< Threshold for reversal detection */

    /* Integration settings */
    bool enable_bio_async;          /**< Enable bio-async messaging */
    bool enable_kg_wiring;          /**< Enable KG registration */
    bool enable_immune;             /**< Enable immune monitoring */
    bool enable_security;           /**< Enable security checks */
    bool enable_logging;            /**< Enable detailed logging */
    bool enable_quantum;            /**< Enable QMC optimization */

    /* Resource limits */
    uint32_t max_history_size;      /**< Decision history buffer */
    uint32_t update_interval_ms;    /**< State update interval */

    /* Platform tier */
    platform_tier_t platform_tier;
} ofc_config_t;

/*=============================================================================
 * MAIN OFC STATE STRUCTURE
 *===========================================================================*/

/**
 * @brief Complete OFC system state
 */
typedef struct nimcp_ofc {
    /* Configuration */
    ofc_config_t config;

    /* Subdivision states */
    struct {
        float activity[OFC_SUBDIV_COUNT];
        float modulation[OFC_SUBDIV_COUNT];
        bool active[OFC_SUBDIV_COUNT];
    } subdivisions;

    /* Value computation */
    ofc_option_state_t* options;    /**< Current options being evaluated */
    uint32_t num_options;
    uint32_t max_options;

    /* Current decision state */
    ofc_decision_t current_decision;
    bool decision_pending;

    /* Learning state */
    float prediction_error;         /**< Current RPE */
    float cumulative_reward;        /**< Total reward tracked */
    uint32_t trial_count;           /**< Number of trials */

    /* Emotion modulation */
    float emotion_valence;          /**< Current emotional valence */
    float emotion_arousal;          /**< Current emotional arousal */

    /* Integration handles */
    struct nimcp_brain_kg* kg;
    struct nimcp_bio_router* bio_router;
    struct nimcp_immune_system* immune;
    struct nimcp_security_context* security;
    struct nimcp_snn_network* snn;
    struct nimcp_plasticity_engine* plasticity;
    struct nimcp_hypothalamus* hypothalamus;
    struct nimcp_thalamus* thalamus;
    struct nimcp_cognitive_hub* cognitive_hub;
    struct nimcp_training_context* training;
    struct nimcp_perception_system* perception;
    struct nimcp_symbolic_engine* symbolic;
    struct nimcp_swarm_context* swarm;
    struct nimcp_dragonfly_context* dragonfly;
    struct nimcp_portia_context* portia;
    struct nimcp_qmc_context* qmc;
    struct nimcp_omni_predictor* omni;

    /* KG wiring state */
    ofc_kg_state_t kg_state;

    /* Statistics */
    ofc_stats_t stats;

    /* Threading */
    nimcp_mutex_t* mutex;

    /* Logging */
    nimcp_logger_t* logger;

    /* State flags */
    bool initialized;
    bool connected;
    uint64_t last_update_us;
} nimcp_ofc_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default OFC configuration
 */
NIMCP_EXPORT int ofc_default_config(ofc_config_t* config);

/**
 * @brief Create OFC instance
 */
NIMCP_EXPORT nimcp_ofc_t* ofc_create(const ofc_config_t* config);

/**
 * @brief Destroy OFC instance
 */
NIMCP_EXPORT void ofc_destroy(nimcp_ofc_t* ofc);

/**
 * @brief Initialize OFC (post-creation setup)
 */
NIMCP_EXPORT int ofc_init(nimcp_ofc_t* ofc);

/**
 * @brief Reset OFC to initial state
 */
NIMCP_EXPORT int ofc_reset(nimcp_ofc_t* ofc);

/*=============================================================================
 * VALUE COMPUTATION API
 *===========================================================================*/

/**
 * @brief Present stimulus/option for evaluation
 */
NIMCP_EXPORT int ofc_present_option(
    nimcp_ofc_t* ofc,
    uint32_t stimulus_id,
    float reward_magnitude,
    float reward_probability,
    float delay);

/**
 * @brief Compute expected value for option
 */
NIMCP_EXPORT int ofc_compute_value(
    nimcp_ofc_t* ofc,
    uint32_t stimulus_id,
    ofc_value_t* result);

/**
 * @brief Update values with prediction error
 */
NIMCP_EXPORT int ofc_update_prediction_error(
    nimcp_ofc_t* ofc,
    uint32_t stimulus_id,
    float received_reward);

/**
 * @brief Get integrated value for option
 */
NIMCP_EXPORT float ofc_get_integrated_value(
    nimcp_ofc_t* ofc,
    uint32_t stimulus_id);

/*=============================================================================
 * DECISION MAKING API
 *===========================================================================*/

/**
 * @brief Make decision between current options
 */
NIMCP_EXPORT int ofc_make_decision(
    nimcp_ofc_t* ofc,
    ofc_decision_t* decision);

/**
 * @brief Check for reversal in contingencies
 */
NIMCP_EXPORT int ofc_check_reversal(
    nimcp_ofc_t* ofc,
    bool* reversal_detected);

/**
 * @brief Assess risk for current options
 */
NIMCP_EXPORT int ofc_assess_risk(
    nimcp_ofc_t* ofc,
    uint32_t stimulus_id,
    float* risk_value);

/**
 * @brief Process social reward signal
 */
NIMCP_EXPORT int ofc_process_social_reward(
    nimcp_ofc_t* ofc,
    float social_value,
    uint32_t social_context);

/*=============================================================================
 * EMOTION INTEGRATION API
 *===========================================================================*/

/**
 * @brief Set emotion modulation
 */
NIMCP_EXPORT int ofc_set_emotion(
    nimcp_ofc_t* ofc,
    float valence,
    float arousal);

/**
 * @brief Get emotion-modulated value
 */
NIMCP_EXPORT float ofc_get_emotion_modulated_value(
    nimcp_ofc_t* ofc,
    uint32_t stimulus_id);

/*=============================================================================
 * INTEGRATION API - KG WIRING
 *===========================================================================*/

/**
 * @brief Register OFC with Knowledge Graph
 */
NIMCP_EXPORT int ofc_kg_register(
    nimcp_ofc_t* ofc,
    struct nimcp_brain_kg* kg,
    uint64_t admin_token);

/**
 * @brief Unregister OFC from Knowledge Graph
 */
NIMCP_EXPORT int ofc_kg_unregister(nimcp_ofc_t* ofc);

/**
 * @brief Update KG with current state
 */
NIMCP_EXPORT int ofc_kg_update_state(nimcp_ofc_t* ofc);

/**
 * @brief Query KG for related information
 */
NIMCP_EXPORT int ofc_kg_query(
    nimcp_ofc_t* ofc,
    const char* query,
    void* result,
    size_t result_size);

/*=============================================================================
 * INTEGRATION API - BIO-ASYNC
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 */
NIMCP_EXPORT int ofc_bio_async_connect(
    nimcp_ofc_t* ofc,
    struct nimcp_bio_router* router);

/**
 * @brief Disconnect from bio-async router
 */
NIMCP_EXPORT int ofc_bio_async_disconnect(nimcp_ofc_t* ofc);

/**
 * @brief Broadcast OFC message
 */
NIMCP_EXPORT int ofc_bio_async_broadcast(
    nimcp_ofc_t* ofc,
    ofc_bio_msg_type_t msg_type,
    const void* payload,
    size_t payload_size);

/**
 * @brief Subscribe to messages
 */
NIMCP_EXPORT int ofc_bio_async_subscribe(
    nimcp_ofc_t* ofc,
    uint32_t subscription_mask);

/*=============================================================================
 * INTEGRATION API - OTHER SYSTEMS
 *===========================================================================*/

/**
 * @brief Connect to immune system
 */
NIMCP_EXPORT int ofc_immune_connect(
    nimcp_ofc_t* ofc,
    struct nimcp_immune_system* immune);

/**
 * @brief Connect to security context
 */
NIMCP_EXPORT int ofc_security_connect(
    nimcp_ofc_t* ofc,
    struct nimcp_security_context* security);

/**
 * @brief Connect to SNN/plasticity
 */
NIMCP_EXPORT int ofc_snn_connect(
    nimcp_ofc_t* ofc,
    struct nimcp_snn_network* snn,
    struct nimcp_plasticity_engine* plasticity);

/**
 * @brief Connect to hypothalamus
 */
NIMCP_EXPORT int ofc_hypothalamus_connect(
    nimcp_ofc_t* ofc,
    struct nimcp_hypothalamus* hypo);

/**
 * @brief Connect to thalamus
 */
NIMCP_EXPORT int ofc_thalamus_connect(
    nimcp_ofc_t* ofc,
    struct nimcp_thalamus* thalamus);

/**
 * @brief Connect to cognitive hub
 */
NIMCP_EXPORT int ofc_cognitive_connect(
    nimcp_ofc_t* ofc,
    struct nimcp_cognitive_hub* hub);

/**
 * @brief Connect to training system
 */
NIMCP_EXPORT int ofc_training_connect(
    nimcp_ofc_t* ofc,
    struct nimcp_training_context* training);

/**
 * @brief Connect to perception system
 */
NIMCP_EXPORT int ofc_perception_connect(
    nimcp_ofc_t* ofc,
    struct nimcp_perception_system* perception);

/**
 * @brief Connect to symbolic logic engine
 */
NIMCP_EXPORT int ofc_symbolic_connect(
    nimcp_ofc_t* ofc,
    struct nimcp_symbolic_engine* symbolic);

/**
 * @brief Connect to swarm system
 */
NIMCP_EXPORT int ofc_swarm_connect(
    nimcp_ofc_t* ofc,
    struct nimcp_swarm_context* swarm);

/**
 * @brief Connect to dragonfly system
 */
NIMCP_EXPORT int ofc_dragonfly_connect(
    nimcp_ofc_t* ofc,
    struct nimcp_dragonfly_context* dragonfly);

/**
 * @brief Connect to portia system
 */
NIMCP_EXPORT int ofc_portia_connect(
    nimcp_ofc_t* ofc,
    struct nimcp_portia_context* portia);

/**
 * @brief Connect to QMC system
 */
NIMCP_EXPORT int ofc_qmc_connect(
    nimcp_ofc_t* ofc,
    struct nimcp_qmc_context* qmc);

/**
 * @brief Connect to omnidirectional predictor
 */
NIMCP_EXPORT int ofc_omni_connect(
    nimcp_ofc_t* ofc,
    struct nimcp_omni_predictor* omni);

/*=============================================================================
 * UPDATE AND STATE API
 *===========================================================================*/

/**
 * @brief Update OFC state (call each timestep)
 */
NIMCP_EXPORT int ofc_update(nimcp_ofc_t* ofc, float dt);

/**
 * @brief Get current statistics
 */
NIMCP_EXPORT int ofc_get_stats(
    const nimcp_ofc_t* ofc,
    ofc_stats_t* stats);

/**
 * @brief Get subdivision activity
 */
NIMCP_EXPORT float ofc_get_subdivision_activity(
    const nimcp_ofc_t* ofc,
    ofc_subdivision_t subdiv);

/**
 * @brief Clear all options
 */
NIMCP_EXPORT int ofc_clear_options(nimcp_ofc_t* ofc);

/*=============================================================================
 * QUANTUM OPTIMIZATION API
 *===========================================================================*/

/**
 * @brief Optimize value computation using QMC
 */
NIMCP_EXPORT int ofc_qmc_optimize_values(nimcp_ofc_t* ofc);

/**
 * @brief Use QMCTS for decision search
 */
NIMCP_EXPORT int ofc_qmcts_decision_search(
    nimcp_ofc_t* ofc,
    uint32_t num_iterations,
    ofc_decision_t* best_decision);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OFC_H */
