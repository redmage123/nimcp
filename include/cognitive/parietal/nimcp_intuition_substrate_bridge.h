/**
 * @file nimcp_intuition_substrate_bridge.h
 * @brief Bridge between intuition system and neural substrate
 *
 * WHAT: Bidirectional integration linking intuitive reasoning to metabolic/energy state
 * WHY: Intuition depends on widespread cortical networks with high ATP demands
 * HOW: Monitors ATP, fatigue, metabolic stress; modulates insight depth, confidence, speed
 *
 * BIOLOGICAL BASIS:
 * - Intuition involves distributed prefrontal-parietal-temporal networks
 * - Pattern recognition and insight require sustained neural activity
 * - ATP depletion reduces insight quality (shallow pattern matching)
 * - Metabolic stress impairs analogical reasoning and creative leaps
 * - Fatigue slows intuitive processing and reduces "aha!" moments
 * - Counterfactual reasoning degrades when substrate resources depleted
 *
 * SUBSTRATE DEPENDENCIES:
 * - ATP: Depletion below 50% reduces insight depth, below 30% causes impairment
 * - Fatigue: High fatigue (>0.7) slows intuitive processing by up to 50%
 * - Metabolic stress: Stress >0.6 impairs creative leaps and analogies
 * - Neural integrity: Damaged networks reduce intuition reliability
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_INTUITION_SUBSTRATE_BRIDGE_H
#define NIMCP_INTUITION_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "cognitive/parietal/nimcp_intuition_integrations.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ========================================================================== */

/**
 * Bio-async module ID for intuition substrate bridge
 * Range: 0x1200-0x12FF (Substrate bridges)
 */
#define BIO_MODULE_SUBSTRATE_INTUITION 0x1210

/**
 * ATP thresholds for intuition operations
 * WHAT: Critical ATP levels that trigger intuition modulation
 * WHY: Intuitive processing is ATP-intensive, requiring sustained cortical activity
 * HOW: Different thresholds trigger different levels of impairment
 */
#define INTUITION_ATP_OPTIMAL_THRESHOLD    0.7f    /* Full intuition capacity */
#define INTUITION_ATP_REDUCED_THRESHOLD    0.5f    /* Reduced insight depth */
#define INTUITION_ATP_IMPAIRED_THRESHOLD   0.3f    /* Severe impairment */
#define INTUITION_ATP_CRITICAL_THRESHOLD   0.2f    /* Emergency state */

/**
 * Fatigue thresholds for intuition speed
 * WHAT: Fatigue levels that slow intuitive processing
 * WHY: Mental fatigue reduces pattern recognition efficiency
 */
#define INTUITION_FATIGUE_MILD_THRESHOLD     0.4f   /* Slight slowdown */
#define INTUITION_FATIGUE_MODERATE_THRESHOLD 0.6f   /* Moderate slowdown */
#define INTUITION_FATIGUE_SEVERE_THRESHOLD   0.8f   /* Severe slowdown */

/**
 * Metabolic stress thresholds
 * WHAT: Stress levels affecting creative and analogical reasoning
 * WHY: Metabolic stress impairs higher-order cognitive functions
 */
#define INTUITION_STRESS_MILD_THRESHOLD     0.4f    /* Minor impairment */
#define INTUITION_STRESS_MODERATE_THRESHOLD 0.6f    /* Moderate impairment */
#define INTUITION_STRESS_SEVERE_THRESHOLD   0.8f    /* Severe impairment */

/* ============================================================================
 * Structures
 * ========================================================================== */

/**
 * @struct intuition_substrate_effects_t
 * @brief Computed effects of substrate state on intuition
 *
 * WHAT: Modulation factors for intuitive processes based on metabolic state
 * WHY: Substrate state directly impacts intuition capability
 * HOW: Values in [0-1] range, 1.0 = optimal, 0.0 = complete failure
 */
typedef struct {
    /** Insight depth: Quality of pattern recognition [0-1]
     *  1.0 = deep insights, 0.5 = shallow, 0.0 = no insight */
    float insight_depth;

    /** Intuition accuracy: Reliability of hunches [0-1]
     *  1.0 = highly accurate, 0.5 = unreliable, 0.0 = invalid */
    float intuition_accuracy;

    /** Processing speed: How fast intuition operates [0-1]
     *  1.0 = fastest, 0.5 = slowed, 0.0 = stalled */
    float processing_speed;

    /** Abstraction capacity: Ability to form analogies [0-1]
     *  1.0 = full capacity, 0.5 = limited, 0.0 = concrete only */
    float abstraction_capacity;

    /** Creative leap potential: Ability to make novel connections [0-1]
     *  1.0 = high creativity, 0.5 = limited, 0.0 = rigid */
    float creative_leap_potential;

    /** Counterfactual capacity: What-if reasoning ability [0-1]
     *  1.0 = full capacity, 0.5 = limited, 0.0 = impaired */
    float counterfactual_capacity;

    /** Meta-reasoning depth: Reasoning about reasoning [0-1]
     *  1.0 = full depth, 0.5 = shallow, 0.0 = none */
    float meta_reasoning_depth;

    /** Overall intuition capacity: Combined modulation factor [0-1] */
    float overall_capacity;
} intuition_substrate_effects_t;

/**
 * @struct intuition_substrate_config_t
 * @brief Configuration for intuition-substrate bridge
 */
typedef struct {
    /** Enable ATP-based modulation */
    bool enable_atp_modulation;

    /** Enable fatigue-based modulation */
    bool enable_fatigue_modulation;

    /** Enable metabolic stress modulation */
    bool enable_stress_modulation;

    /** Enable bio-async messaging */
    bool enable_bio_async;

    /** ATP sensitivity (how much ATP affects intuition) [0-1] */
    float atp_sensitivity;

    /** Fatigue sensitivity [0-1] */
    float fatigue_sensitivity;

    /** Stress sensitivity [0-1] */
    float stress_sensitivity;

    /** Minimum capacity (floor for modulation) [0-1] */
    float min_capacity;
} intuition_substrate_config_t;

/**
 * @struct intuition_substrate_bridge_t
 * @brief Opaque handle for intuition-substrate bridge
 */
typedef struct intuition_substrate_bridge intuition_substrate_bridge_t;

/* ============================================================================
 * Lifecycle Functions
 * ========================================================================== */

/**
 * @brief Get default configuration for intuition-substrate bridge
 * @return Default configuration
 */
intuition_substrate_config_t intuition_substrate_default_config(void);

/**
 * @brief Create intuition-substrate bridge
 *
 * @param intuition Intuition system to connect
 * @param substrate Neural substrate to monitor
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
intuition_substrate_bridge_t* intuition_substrate_bridge_create(
    intuition_system_t* intuition,
    neural_substrate_t* substrate,
    const intuition_substrate_config_t* config
);

/**
 * @brief Destroy intuition-substrate bridge
 * @param bridge Bridge to destroy
 */
void intuition_substrate_bridge_destroy(intuition_substrate_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int intuition_substrate_bridge_reset(intuition_substrate_bridge_t* bridge);

/* ============================================================================
 * Update Functions
 * ========================================================================== */

/**
 * @brief Update bridge with current substrate state
 *
 * Reads current metabolic state from substrate and computes
 * modulation effects for intuition system.
 *
 * @param bridge Bridge to update
 * @return 0 on success, -1 on error
 */
int intuition_substrate_bridge_update(intuition_substrate_bridge_t* bridge);

/**
 * @brief Get current substrate effects on intuition
 *
 * @param bridge Bridge to query
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int intuition_substrate_bridge_get_effects(
    const intuition_substrate_bridge_t* bridge,
    intuition_substrate_effects_t* effects
);

/**
 * @brief Apply substrate effects to intuition system
 *
 * Updates the intuition system's inflammation and fatigue
 * based on current substrate state.
 *
 * @param bridge Bridge to use
 * @return 0 on success, -1 on error
 */
int intuition_substrate_bridge_apply_effects(intuition_substrate_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration
 * ========================================================================== */

/**
 * @brief Register bridge with bio-async router
 *
 * @param bridge Bridge to register
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
int intuition_substrate_bridge_register_bio_async(
    intuition_substrate_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Handle incoming bio-async message
 *
 * @param bridge Bridge to handle message
 * @param msg Bio message
 * @return 0 on success, -1 on error
 */
int intuition_substrate_bridge_handle_message(
    intuition_substrate_bridge_t* bridge,
    const bio_message_header_t* msg
);

/* ============================================================================
 * Statistics
 * ========================================================================== */

/**
 * @struct intuition_substrate_stats_t
 * @brief Statistics for intuition-substrate bridge
 */
typedef struct {
    uint64_t updates;                    /* Number of updates */
    uint64_t effects_applied;            /* Number of effect applications */
    uint64_t bio_messages_sent;          /* Bio-async messages sent */
    uint64_t bio_messages_received;      /* Bio-async messages received */
    float avg_atp_level;                 /* Average ATP level observed */
    float avg_fatigue_level;             /* Average fatigue level */
    float avg_capacity;                  /* Average overall capacity */
    uint64_t low_atp_events;             /* Times ATP dropped below threshold */
    uint64_t high_fatigue_events;        /* Times fatigue exceeded threshold */
} intuition_substrate_stats_t;

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int intuition_substrate_bridge_get_stats(
    const intuition_substrate_bridge_t* bridge,
    intuition_substrate_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 * @param bridge Bridge to reset
 */
void intuition_substrate_bridge_reset_stats(intuition_substrate_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTUITION_SUBSTRATE_BRIDGE_H */
