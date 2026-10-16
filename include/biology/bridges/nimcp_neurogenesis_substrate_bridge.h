/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_neurogenesis_substrate_bridge.h - Neurogenesis to Bio-Async Substrate Bridge
//=============================================================================
/**
 * @file nimcp_neurogenesis_substrate_bridge.h
 * @brief Bridge between neurogenesis and bio-async messaging substrate
 *
 * WHAT: Connects neurogenesis with the asynchronous biological messaging system,
 *       enabling growth factor signaling, metabolic coordination, and substrate
 *       availability for new neuron development.
 *
 * WHY:  Bridges the gap between:
 *       - Neurogenesis (stem cells, differentiation, survival)
 *       - Bio-async messaging (growth factors, metabolic signals)
 *       - Substrate availability (ATP, nutrients, oxygen)
 *
 * HOW:  Bidirectional integration:
 *       1. Neurogenesis -> Substrate: Resource demand messages
 *       2. Substrate -> Neurogenesis: Growth factor delivery, metabolic state
 *       3. Neurotrophic factor routing (BDNF, NGF, etc.)
 *       4. Metabolic gating of proliferation and survival
 *
 * BIOLOGICAL BASIS:
 * ```
 * NEUROGENESIS                          BIO-ASYNC SUBSTRATE
 * -----------------------------------------------------------------
 * Stem cell quiescence               <- Low growth factor availability
 * Proliferation activation           <- BDNF/EGF/FGF signaling
 * Differentiation                    <- Specific factor combinations
 * Axon/dendrite growth               <- NGF/BDNF gradient following
 * Activity-dependent survival        <- Neurotrophic factor competition
 * Metabolic demand                   -> ATP/glucose requests
 * Oxygen requirements                -> Neurovascular coupling
 * ```
 *
 * NEUROTROPHIC SIGNALING:
 * - BDNF: Promotes survival, enhances differentiation
 * - NGF: Axon guidance, survival signaling
 * - EGF: Stem cell proliferation
 * - FGF: Multipotent progenitor maintenance
 * - VEGF: Neurovascular coupling, angiogenesis
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NEUROGENESIS_SUBSTRATE_BRIDGE_H
#define NIMCP_NEUROGENESIS_SUBSTRATE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define NEUROGENESIS_SUBSTRATE_MODULE_NAME      "neurogenesis_substrate_bridge"

/** Maximum tracked growth factor types */
#define NEUROGENESIS_SUBSTRATE_MAX_FACTORS      16

/** Maximum pending messages */
#define NEUROGENESIS_SUBSTRATE_MAX_MESSAGES     256

/** Default BDNF threshold for survival */
#define NEUROGENESIS_SUBSTRATE_BDNF_THRESH      0.3f

/** Default ATP threshold for proliferation */
#define NEUROGENESIS_SUBSTRATE_ATP_THRESH       0.6f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Neurotrophic factor types
 */
typedef enum {
    NG_SUBSTRATE_BDNF = 0,           /**< Brain-derived neurotrophic factor */
    NG_SUBSTRATE_NGF,                /**< Nerve growth factor */
    NG_SUBSTRATE_EGF,                /**< Epidermal growth factor */
    NG_SUBSTRATE_FGF,                /**< Fibroblast growth factor */
    NG_SUBSTRATE_VEGF,               /**< Vascular endothelial growth factor */
    NG_SUBSTRATE_IGF,                /**< Insulin-like growth factor */
    NG_SUBSTRATE_CNTF,               /**< Ciliary neurotrophic factor */
    NG_SUBSTRATE_NT3,                /**< Neurotrophin-3 */
    NG_SUBSTRATE_NT4,                /**< Neurotrophin-4 */
    NG_SUBSTRATE_GDNF,               /**< Glial-derived neurotrophic factor */
    NG_SUBSTRATE_FACTOR_COUNT
} ng_substrate_factor_t;

/**
 * @brief Message types for bio-async communication
 */
typedef enum {
    NG_MSG_RESOURCE_REQUEST = 0,     /**< Request ATP/glucose/oxygen */
    NG_MSG_GROWTH_FACTOR_REQUEST,    /**< Request neurotrophic factors */
    NG_MSG_METABOLIC_STATUS,         /**< Report metabolic state */
    NG_MSG_SURVIVAL_SIGNAL,          /**< Survival signaling */
    NG_MSG_DIFFERENTIATION_SIGNAL,   /**< Differentiation trigger */
    NG_MSG_GRADIENT_QUERY,           /**< Query factor gradient */
    NG_MSG_NEUROVASCULAR_COUPLING    /**< Request increased blood flow */
} ng_substrate_msg_type_t;

/**
 * @brief Metabolic state for neurogenesis
 */
typedef enum {
    NG_METABOLIC_OPTIMAL = 0,        /**< Full metabolic support */
    NG_METABOLIC_ADEQUATE,           /**< Sufficient for maintenance */
    NG_METABOLIC_STRESSED,           /**< Reduced, limits proliferation */
    NG_METABOLIC_CRITICAL            /**< Insufficient, triggers apoptosis */
} ng_metabolic_state_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for neurogenesis-substrate bridge
 */
typedef struct {
    /** Growth factor thresholds */
    float bdnf_survival_threshold;       /**< BDNF level for survival */
    float bdnf_proliferation_threshold;  /**< BDNF for proliferation */
    float ngf_guidance_threshold;        /**< NGF for axon guidance */
    float egf_proliferation_threshold;   /**< EGF for stem cell division */

    /** Metabolic thresholds */
    float atp_proliferation_threshold;   /**< ATP for proliferation */
    float atp_survival_threshold;        /**< ATP for survival */
    float glucose_threshold;             /**< Minimum glucose */
    float oxygen_threshold;              /**< Minimum oxygen */

    /** Signaling parameters */
    float signal_diffusion_rate;         /**< Growth factor diffusion */
    float signal_decay_rate;             /**< Factor decay rate */
    float gradient_sensitivity;          /**< Sensitivity to gradients */
    bool enable_competition;             /**< Neurons compete for factors */

    /** Message routing */
    uint32_t message_queue_size;         /**< Max pending messages */
    float message_timeout_ms;            /**< Message timeout */
    bool enable_priority_routing;        /**< Priority-based routing */

    /** Update parameters */
    float update_interval_ms;            /**< Bridge update interval */
    bool enable_logging;
    bool enable_metrics;
} ng_substrate_config_t;

/**
 * @brief Growth factor availability state
 */
typedef struct {
    float levels[NG_SUBSTRATE_FACTOR_COUNT]; /**< Current factor levels */
    float gradients[NG_SUBSTRATE_FACTOR_COUNT][3]; /**< Gradient vectors */
    float production_rates[NG_SUBSTRATE_FACTOR_COUNT]; /**< Production rates */
    float consumption_rates[NG_SUBSTRATE_FACTOR_COUNT]; /**< Consumption */
    uint64_t last_update;                /**< Last update timestamp */
} ng_substrate_factor_state_t;

/**
 * @brief Metabolic availability state
 */
typedef struct {
    float atp_level;                     /**< ATP availability (0-1) */
    float glucose_level;                 /**< Glucose availability (0-1) */
    float oxygen_level;                  /**< Oxygen availability (0-1) */
    float lactate_level;                 /**< Lactate level */
    ng_metabolic_state_t state;          /**< Overall metabolic state */
    float metabolic_demand;              /**< Current demand */
    float metabolic_supply;              /**< Current supply */
} ng_substrate_metabolic_t;

/**
 * @brief Bio-async message for neurogenesis
 */
typedef struct {
    ng_substrate_msg_type_t type;        /**< Message type */
    uint32_t source_id;                  /**< Source (neuron/niche ID) */
    uint32_t target_id;                  /**< Target (0 = broadcast) */
    float payload[8];                    /**< Message payload */
    float priority;                      /**< Message priority */
    uint64_t timestamp;                  /**< Creation timestamp */
    uint64_t expiry;                     /**< Expiration timestamp */
} ng_substrate_message_t;

/**
 * @brief Neuron substrate requirements
 */
typedef struct {
    uint32_t neuron_id;                  /**< Neuron identifier */
    float bdnf_requirement;              /**< BDNF needed */
    float atp_requirement;               /**< ATP needed */
    float glucose_requirement;           /**< Glucose needed */
    float oxygen_requirement;            /**< Oxygen needed */
    float current_satisfaction;          /**< Fraction of needs met */
    bool survival_at_risk;               /**< Insufficient resources */
} ng_substrate_requirements_t;

/**
 * @brief Growth factor gradient information
 */
typedef struct {
    ng_substrate_factor_t factor;        /**< Which factor */
    float concentration;                 /**< Local concentration */
    float gradient_magnitude;            /**< Gradient strength */
    float gradient_direction[3];         /**< Gradient direction vector */
    float source_distance;               /**< Distance to nearest source */
} ng_substrate_gradient_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t messages_sent;              /**< Messages sent */
    uint64_t messages_received;          /**< Messages received */
    uint64_t messages_expired;           /**< Messages expired */
    uint64_t resource_requests;          /**< Resource requests made */
    uint64_t survival_signals;           /**< Survival signals sent */
    float avg_bdnf_level;                /**< Average BDNF availability */
    float avg_atp_level;                 /**< Average ATP availability */
    float avg_satisfaction;              /**< Average requirement satisfaction */
    uint32_t neurons_at_risk;            /**< Neurons with insufficient resources */
    uint64_t update_count;               /**< Total updates */
    float last_update_ms;                /**< Last update timestamp */
} ng_substrate_stats_t;

/** Opaque bridge handle */
typedef struct ng_substrate_bridge_struct ng_substrate_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_substrate_default_config(ng_substrate_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create neurogenesis-substrate bridge
 *
 * WHAT: Creates bridge for growth factor and metabolic signaling
 * WHY:  Enable neurotrophic support for neurogenesis
 * HOW:  Routes messages, tracks factor levels, manages metabolism
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ng_substrate_bridge_t* ng_substrate_bridge_create(
    const ng_substrate_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void ng_substrate_bridge_destroy(ng_substrate_bridge_t* bridge);

//=============================================================================
// Growth Factor API
//=============================================================================

/**
 * @brief Set growth factor level at location
 *
 * WHAT: Updates local growth factor concentration
 * WHY:  Factor availability affects neurogenesis
 * HOW:  Sets level, triggers gradient recalculation
 *
 * @param bridge Bridge handle
 * @param factor Factor type
 * @param level Concentration level (0-1)
 * @param position Spatial position (optional, NULL for global)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_substrate_set_factor(
    ng_substrate_bridge_t* bridge,
    ng_substrate_factor_t factor,
    float level,
    const float* position
);

/**
 * @brief Get growth factor level at location
 *
 * @param bridge Bridge handle
 * @param factor Factor type
 * @param position Spatial position (optional, NULL for global)
 * @return Factor level (0-1)
 */
NIMCP_EXPORT float ng_substrate_get_factor(
    const ng_substrate_bridge_t* bridge,
    ng_substrate_factor_t factor,
    const float* position
);

/**
 * @brief Get growth factor gradient at location
 *
 * WHAT: Computes local gradient of growth factor
 * WHY:  Neurons follow factor gradients for guidance
 * HOW:  Calculates gradient from local concentration field
 *
 * @param bridge Bridge handle
 * @param factor Factor type
 * @param position Spatial position
 * @param gradient Output gradient information
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_substrate_get_gradient(
    const ng_substrate_bridge_t* bridge,
    ng_substrate_factor_t factor,
    const float* position,
    ng_substrate_gradient_t* gradient
);

/**
 * @brief Get complete factor state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_substrate_get_factor_state(
    const ng_substrate_bridge_t* bridge,
    ng_substrate_factor_state_t* state
);

//=============================================================================
// Metabolic API
//=============================================================================

/**
 * @brief Set metabolic state
 *
 * WHAT: Updates metabolic availability from substrate
 * WHY:  Metabolism gates neurogenesis processes
 * HOW:  External metabolic system provides state
 *
 * @param bridge Bridge handle
 * @param metabolic Metabolic state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_substrate_set_metabolic(
    ng_substrate_bridge_t* bridge,
    const ng_substrate_metabolic_t* metabolic
);

/**
 * @brief Get current metabolic state
 *
 * @param bridge Bridge handle
 * @param metabolic Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_substrate_get_metabolic(
    const ng_substrate_bridge_t* bridge,
    ng_substrate_metabolic_t* metabolic
);

/**
 * @brief Request metabolic resources
 *
 * WHAT: Sends resource request message
 * WHY:  New neurons need metabolic support
 * HOW:  Queues message for bio-async routing
 *
 * @param bridge Bridge handle
 * @param neuron_id Requesting neuron
 * @param atp_needed ATP amount needed
 * @param glucose_needed Glucose needed
 * @param oxygen_needed Oxygen needed
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_substrate_request_resources(
    ng_substrate_bridge_t* bridge,
    uint32_t neuron_id,
    float atp_needed,
    float glucose_needed,
    float oxygen_needed
);

/**
 * @brief Check if metabolic state supports proliferation
 *
 * @param bridge Bridge handle
 * @return true if proliferation is metabolically supported
 */
NIMCP_EXPORT bool ng_substrate_can_proliferate(
    const ng_substrate_bridge_t* bridge
);

//=============================================================================
// Messaging API
//=============================================================================

/**
 * @brief Send bio-async message
 *
 * WHAT: Queues message for delivery
 * WHY:  Enable async communication for neurogenesis
 * HOW:  Adds to priority queue for routing
 *
 * @param bridge Bridge handle
 * @param message Message to send
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_substrate_send_message(
    ng_substrate_bridge_t* bridge,
    const ng_substrate_message_t* message
);

/**
 * @brief Receive pending messages for target
 *
 * WHAT: Gets messages for specific target
 * WHY:  Allow neurogenesis to receive substrate signals
 * HOW:  Filters queue by target ID
 *
 * @param bridge Bridge handle
 * @param target_id Target (neuron/niche ID)
 * @param messages Output array
 * @param max_messages Maximum to receive
 * @return Number of messages received
 */
NIMCP_EXPORT int ng_substrate_receive_messages(
    ng_substrate_bridge_t* bridge,
    uint32_t target_id,
    ng_substrate_message_t* messages,
    uint32_t max_messages
);

/**
 * @brief Process message queue
 *
 * WHAT: Routes and delivers pending messages
 * WHY:  Periodic message processing
 * HOW:  Matches messages to targets, expires old messages
 *
 * @param bridge Bridge handle
 * @return Number of messages processed
 */
NIMCP_EXPORT int ng_substrate_process_messages(ng_substrate_bridge_t* bridge);

//=============================================================================
// Neuron Support API
//=============================================================================

/**
 * @brief Register neuron for substrate support
 *
 * WHAT: Registers new neuron for resource tracking
 * WHY:  Track resource requirements and satisfaction
 * HOW:  Creates requirement entry for neuron
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param bdnf_need BDNF requirement
 * @param atp_need ATP requirement
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_substrate_register_neuron(
    ng_substrate_bridge_t* bridge,
    uint32_t neuron_id,
    float bdnf_need,
    float atp_need
);

/**
 * @brief Get neuron substrate requirements
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param requirements Output requirements
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_substrate_get_requirements(
    const ng_substrate_bridge_t* bridge,
    uint32_t neuron_id,
    ng_substrate_requirements_t* requirements
);

/**
 * @brief Evaluate neuron survival based on substrate
 *
 * WHAT: Checks if substrate supports neuron survival
 * WHY:  Insufficient substrate leads to apoptosis
 * HOW:  Compares requirements against availability
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return Survival score (0-1, <threshold = death)
 */
NIMCP_EXPORT float ng_substrate_evaluate_survival(
    const ng_substrate_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Request neurovascular coupling
 *
 * WHAT: Requests increased blood flow for region
 * WHY:  High activity needs metabolic support
 * HOW:  Sends coupling signal to neurovascular bridge
 *
 * @param bridge Bridge handle
 * @param region Region identifier
 * @param demand Metabolic demand level
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_substrate_request_vascular_support(
    ng_substrate_bridge_t* bridge,
    uint32_t region,
    float demand
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Process messages, update factors, check survival
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_substrate_update(
    ng_substrate_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_substrate_reset(ng_substrate_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_substrate_get_stats(
    const ng_substrate_bridge_t* bridge,
    ng_substrate_stats_t* stats
);

/**
 * @brief Get neurons at survival risk
 *
 * @param bridge Bridge handle
 * @param ids Output array
 * @param max_ids Maximum IDs to return
 * @return Number of at-risk neurons
 */
NIMCP_EXPORT int ng_substrate_get_at_risk_neurons(
    const ng_substrate_bridge_t* bridge,
    uint32_t* ids,
    uint32_t max_ids
);

/**
 * @brief Get factor name string
 *
 * @param factor Factor type
 * @return Factor name
 */
NIMCP_EXPORT const char* ng_substrate_factor_name(ng_substrate_factor_t factor);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROGENESIS_SUBSTRATE_BRIDGE_H */