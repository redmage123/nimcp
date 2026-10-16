/**
 * @file nimcp_imagination_callbacks.h
 * @brief Standard Callback Types for Imagination Engine Integration
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Defines standard callback types for modules integrating with imagination engine
 * WHY:  Enables consistent bidirectional communication between imagination and brain modules
 * HOW:  Callback function typedefs with standardized signatures
 *
 * BIOLOGICAL BASIS:
 * Neural communication in the brain is bidirectional - imagination (default mode network)
 * both receives input from and sends output to multiple brain regions. These callbacks
 * model the afferent (incoming) and efferent (outgoing) neural projections.
 *
 * USAGE:
 * ```c
 * // Register callback in connected module
 * void my_imagination_handler(const imagination_scenario_t* scenario,
 *                             imagination_mode_t mode,
 *                             void* user_data) {
 *     my_module_t* mod = (my_module_t*)user_data;
 *     // Process imagination result
 * }
 *
 * hippocampus_set_imagination_callback(hipp, my_imagination_handler, my_module);
 * ```
 */

#ifndef NIMCP_IMAGINATION_CALLBACKS_H
#define NIMCP_IMAGINATION_CALLBACKS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations to avoid circular dependencies */
struct imagination_scenario;
struct imagination_goal;
struct nimcp_tensor;
struct counterfactual_query;

/* ============================================================================
 * Imagination Mode Enumeration (duplicated for standalone use)
 * ============================================================================ */

/**
 * @brief Imagination operating modes
 */
typedef enum imagination_callback_mode {
    IMAG_CB_MODE_PASSIVE = 0,        /**< Background daydreaming */
    IMAG_CB_MODE_DIRECTED,           /**< Goal-directed imagination */
    IMAG_CB_MODE_COUNTERFACTUAL,     /**< "What if" reasoning */
    IMAG_CB_MODE_PROSPECTIVE,        /**< Future simulation */
    IMAG_CB_MODE_RETROSPECTIVE,      /**< Memory reconstruction */
    IMAG_CB_MODE_CREATIVE,           /**< Novel combination */
    IMAG_CB_MODE_SOCIAL,             /**< Perspective-taking */
    IMAG_CB_MODE_SPATIAL,            /**< Mental navigation */
    IMAG_CB_MODE_MATHEMATICAL,       /**< Abstract visualization */
    IMAG_CB_MODE_SCIENTIFIC,         /**< Hypothesis testing */
    IMAG_CB_MODE_REM_DREAM,          /**< REM sleep dreaming */
    IMAG_CB_MODE_LUCID,              /**< Conscious dream control */
    IMAG_CB_MODE_COUNT
} imagination_callback_mode_t;

/* ============================================================================
 * Result Callbacks - Receiving Imagination Output
 * ============================================================================ */

/**
 * @brief Callback invoked when imagination generates a scenario result
 *
 * WHAT: Notifies module when imagination completes a scenario
 * WHY:  Allows modules to consume imagination output asynchronously
 * WHEN: Called after imagination_step_scenario() produces stable output
 *
 * @param scenario  The completed imagination scenario with latent state
 * @param mode      The imagination mode that generated this result
 * @param user_data Module-specific context pointer
 */
typedef void (*imagination_result_callback_t)(
    const struct imagination_scenario* scenario,
    imagination_callback_mode_t mode,
    void* user_data
);

/**
 * @brief Callback invoked when visual content is generated
 *
 * WHAT: Notifies visual processing modules of generated imagery
 * WHY:  Enables visual cortex to render imagined content
 * WHEN: Called after imagination_generate_visual()
 *
 * @param visual_content  Tensor containing generated visual features
 * @param vividness       Subjective vividness [0.0-1.0]
 * @param user_data       Module-specific context pointer
 */
typedef void (*imagination_visual_callback_t)(
    const struct nimcp_tensor* visual_content,
    float vividness,
    void* user_data
);

/**
 * @brief Callback invoked when audio content is generated
 *
 * WHAT: Notifies audio processing modules of generated sounds
 * WHY:  Enables audio cortex to render imagined audio
 * WHEN: Called after imagination_generate_audio()
 *
 * @param audio_content   Tensor containing generated audio features
 * @param vividness       Subjective vividness [0.0-1.0]
 * @param user_data       Module-specific context pointer
 */
typedef void (*imagination_audio_callback_t)(
    const struct nimcp_tensor* audio_content,
    float vividness,
    void* user_data
);

/**
 * @brief Callback invoked with counterfactual reasoning results
 *
 * WHAT: Returns results of "what if" reasoning
 * WHY:  Enables modules to evaluate alternative outcomes
 * WHEN: Called after imagination_counterfactual() completes
 *
 * @param scenario     The counterfactual scenario
 * @param original     The original scenario for comparison
 * @param plausibility How plausible the counterfactual is [0.0-1.0]
 * @param user_data    Module-specific context pointer
 */
typedef void (*imagination_counterfactual_callback_t)(
    const struct imagination_scenario* scenario,
    const struct imagination_scenario* original,
    float plausibility,
    void* user_data
);

/**
 * @brief Callback invoked with prospective simulation results
 *
 * WHAT: Returns results of future simulation
 * WHY:  Enables planning and decision-making based on predicted outcomes
 * WHEN: Called after prospective imagination scenario completes
 *
 * @param scenario         The prospective scenario
 * @param predicted_states Array of predicted future states
 * @param num_predictions  Number of predictions
 * @param confidence       Overall prediction confidence [0.0-1.0]
 * @param user_data        Module-specific context pointer
 */
typedef void (*imagination_prospective_callback_t)(
    const struct imagination_scenario* scenario,
    const struct nimcp_tensor* predicted_states,
    size_t num_predictions,
    float confidence,
    void* user_data
);

/**
 * @brief Callback invoked with social simulation results
 *
 * WHAT: Returns results of perspective-taking / ToM simulation
 * WHY:  Enables understanding of other agents' mental states
 * WHEN: Called after social imagination scenario completes
 *
 * @param scenario     The social simulation scenario
 * @param agent_id     ID of the simulated agent
 * @param mental_state Inferred mental state of the agent
 * @param confidence   Confidence in the inference [0.0-1.0]
 * @param user_data    Module-specific context pointer
 */
typedef void (*imagination_social_callback_t)(
    const struct imagination_scenario* scenario,
    uint64_t agent_id,
    const struct nimcp_tensor* mental_state,
    float confidence,
    void* user_data
);

/* ============================================================================
 * Request Callbacks - Triggering Imagination
 * ============================================================================ */

/**
 * @brief Callback to request imagination from another module
 *
 * WHAT: Allows modules to trigger imagination scenarios
 * WHY:  Enables bottom-up imagination requests (e.g., curiosity triggering exploration)
 * WHEN: Called when a module needs imagination capabilities
 *
 * @param goal        Goal specification for the imagination
 * @param mode        Requested imagination mode
 * @param urgency     Request urgency [0.0-1.0]
 * @param user_data   Module-specific context pointer
 * @return Scenario ID or 0 on failure
 */
typedef uint32_t (*imagination_request_callback_t)(
    const struct imagination_goal* goal,
    imagination_callback_mode_t mode,
    float urgency,
    void* user_data
);

/**
 * @brief Callback for memory retrieval requests during imagination
 *
 * WHAT: Requests relevant memories from hippocampus during imagination
 * WHY:  Imagination content is grounded in episodic/semantic memory
 * WHEN: Called when imagination needs memory content
 *
 * @param query_cue     Cue for memory retrieval (latent representation)
 * @param max_memories  Maximum number of memories to retrieve
 * @param user_data     Module-specific context pointer
 * @return Retrieved memory tensor or NULL
 */
typedef struct nimcp_tensor* (*imagination_memory_request_callback_t)(
    const struct nimcp_tensor* query_cue,
    size_t max_memories,
    void* user_data
);

/**
 * @brief Callback for goal update during imagination
 *
 * WHAT: Notifies prefrontal cortex when imagination goals change
 * WHY:  Executive control needs to monitor imagination direction
 * WHEN: Called when imagination scenario goals are updated
 *
 * @param new_goal    The updated goal
 * @param old_goal    The previous goal (may be NULL)
 * @param scenario_id ID of the affected scenario
 * @param user_data   Module-specific context pointer
 */
typedef void (*imagination_goal_update_callback_t)(
    const struct imagination_goal* new_goal,
    const struct imagination_goal* old_goal,
    uint32_t scenario_id,
    void* user_data
);

/* ============================================================================
 * Modulation Callbacks - External Influences on Imagination
 * ============================================================================ */

/**
 * @brief Callback for immune system modulation of imagination
 *
 * WHAT: Reports inflammation effects on imagination vividness/coherence
 * WHY:  Sickness behavior includes reduced imaginative capacity
 * WHEN: Called when immune state changes affect imagination
 *
 * @param vividness_modifier   Multiplier for vividness [0.0-1.0]
 * @param coherence_modifier   Multiplier for coherence [0.0-1.0]
 * @param inflammation_level   Current inflammation level [0.0-1.0]
 * @param user_data            Module-specific context pointer
 */
typedef void (*imagination_immune_modulation_callback_t)(
    float vividness_modifier,
    float coherence_modifier,
    float inflammation_level,
    void* user_data
);

/**
 * @brief Callback for metabolic modulation of imagination
 *
 * WHAT: Reports ATP/energy effects on imagination capacity
 * WHY:  Imagination is metabolically expensive
 * WHEN: Called when energy state changes affect imagination
 *
 * @param capacity_modifier    Multiplier for imagination capacity [0.0-1.0]
 * @param atp_level            Current ATP level [0.0-1.0]
 * @param fatigue_level        Current fatigue level [0.0-1.0]
 * @param user_data            Module-specific context pointer
 */
typedef void (*imagination_metabolic_modulation_callback_t)(
    float capacity_modifier,
    float atp_level,
    float fatigue_level,
    void* user_data
);

/**
 * @brief Callback for attention gating of imagination
 *
 * WHAT: Reports attention state effects on imagination access
 * WHY:  Attention gates what imagination content reaches consciousness
 * WHEN: Called when attention focus changes
 *
 * @param attention_weight     Weight for imagination content [0.0-1.0]
 * @param focus_target         Current attention target (may be NULL)
 * @param user_data            Module-specific context pointer
 */
typedef void (*imagination_attention_gate_callback_t)(
    float attention_weight,
    const struct nimcp_tensor* focus_target,
    void* user_data
);

/* ============================================================================
 * Collective/Swarm Callbacks
 * ============================================================================ */

/**
 * @brief Callback for receiving collective imagination insights
 *
 * WHAT: Receives imagination content from swarm consciousness
 * WHY:  Collective imagination enables distributed creativity
 * WHEN: Called when swarm shares imagination results
 *
 * @param scenario     Shared imagination scenario
 * @param source_node  Node ID that shared the scenario
 * @param relevance    Relevance to local imagination [0.0-1.0]
 * @param user_data    Module-specific context pointer
 */
typedef void (*imagination_collective_receive_callback_t)(
    const struct imagination_scenario* scenario,
    uint64_t source_node,
    float relevance,
    void* user_data
);

/**
 * @brief Callback for sharing local imagination to collective
 *
 * WHAT: Shares local imagination scenario with swarm
 * WHY:  Enables collective creativity and insight sharing
 * WHEN: Called when local imagination produces shareable content
 *
 * @param scenario     Scenario to share
 * @param share_scope  Scope of sharing (local cluster, global, etc.)
 * @param user_data    Module-specific context pointer
 * @return 0 on success, -1 on failure
 */
typedef int (*imagination_collective_share_callback_t)(
    const struct imagination_scenario* scenario,
    uint32_t share_scope,
    void* user_data
);

/* ============================================================================
 * Callback Registration Structure
 * ============================================================================ */

/**
 * @brief Container for all imagination callbacks
 *
 * WHAT: Groups all callbacks for a module's imagination integration
 * WHY:  Simplifies registration and management of callbacks
 * HOW:  Passed to imagination_engine_register_callbacks()
 */
typedef struct imagination_callback_set {
    /* Result callbacks */
    imagination_result_callback_t on_result;
    imagination_visual_callback_t on_visual;
    imagination_audio_callback_t on_audio;
    imagination_counterfactual_callback_t on_counterfactual;
    imagination_prospective_callback_t on_prospective;
    imagination_social_callback_t on_social;

    /* Request callbacks */
    imagination_request_callback_t on_request;
    imagination_memory_request_callback_t on_memory_request;
    imagination_goal_update_callback_t on_goal_update;

    /* Modulation callbacks */
    imagination_immune_modulation_callback_t on_immune_modulation;
    imagination_metabolic_modulation_callback_t on_metabolic_modulation;
    imagination_attention_gate_callback_t on_attention_gate;

    /* Collective callbacks */
    imagination_collective_receive_callback_t on_collective_receive;
    imagination_collective_share_callback_t on_collective_share;

    /* User data for all callbacks */
    void* user_data;
} imagination_callback_set_t;

/**
 * @brief Initialize callback set with NULL values
 *
 * @param set Callback set to initialize
 * @return 0 on success, -1 on error
 */
static inline int imagination_callback_set_init(imagination_callback_set_t* set) {
    if (!set) return -1;
    memset(set, 0, sizeof(imagination_callback_set_t));
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMAGINATION_CALLBACKS_H */
