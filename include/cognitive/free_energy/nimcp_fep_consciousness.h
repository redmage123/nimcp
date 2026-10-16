/**
 * @file nimcp_fep_consciousness.h
 * @brief Consciousness-Gated Action Selection for Free Energy Principle
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Integrates consciousness metrics (IIT Φ) with FEP active inference,
 *       gating action selection and modulating precision based on awareness level.
 * WHY:  Consciousness influences action selection in biological systems - high Φ
 *       enables deliberate action selection, low Φ defaults to habitual responses.
 *       Metacognitive monitoring assesses generative model quality.
 * HOW:  Bridge between introspection (consciousness metrics) and FEP system,
 *       applying Global Workspace Theory and IIT to gate actions and boost precision.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * GLOBAL WORKSPACE THEORY (Baars, 1988; Dehaene & Changeux, 2011):
 * ------------------------------------------------------------------
 * - Conscious access gates action selection
 * - Global workspace broadcasts information to action systems
 * - Unconscious processing defaults to habitual responses
 * - Consciousness enables flexible, novel action combinations
 *
 * INTEGRATED INFORMATION THEORY (Tononi, 2004-2024):
 * ---------------------------------------------------
 * - Φ (phi) quantifies consciousness level
 * - High Φ = integrated system, conscious processing
 * - Low Φ = modular system, unconscious processing
 * - Φ modulates action coherence and precision
 *
 * ATTENTION AS PRECISION (Friston, 2010):
 * ----------------------------------------
 * - Attention = precision optimization in predictive coding
 * - Conscious attention increases gain on relevant predictions
 * - Φ correlates with attention and precision weighting
 * - High Φ → high precision → strong belief updates
 *
 * METACOGNITIVE MONITORING (Fleming & Dolan, 2012):
 * --------------------------------------------------
 * - Consciousness assesses generative model quality
 * - Type 1 processing: Direct inference
 * - Type 2 processing: Metacognitive assessment of Type 1
 * - Metacognitive confidence gates action execution
 *
 * CONSCIOUSNESS-ACTION GATING:
 * ----------------------------
 * When Φ < threshold:
 *   - Default to habitual/cached actions
 *   - Reduced precision (less learning from errors)
 *   - Fast, automatic responses
 *
 * When Φ ≥ threshold:
 *   - Full policy evaluation
 *   - Increased precision (more learning)
 *   - Deliberate, flexible action selection
 *   - Metacognitive intervention when model quality low
 *
 * REFERENCES:
 * - Baars, B. J. (1988) "A Cognitive Theory of Consciousness"
 * - Dehaene, S., & Changeux, J. P. (2011) "Experimental and theoretical approaches to conscious processing"
 * - Tononi, G. (2004) "An information integration theory of consciousness"
 * - Friston, K. (2010) "The free-energy principle: a unified brain theory?"
 * - Fleming, S. M., & Dolan, R. J. (2012) "The neural basis of metacognitive ability"
 * - Seth, A. K., & Bayne, T. (2022) "Theories of consciousness"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FEP_CONSCIOUSNESS_H
#define NIMCP_FEP_CONSCIOUSNESS_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/introspection/nimcp_consciousness_metrics.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_async.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default minimum Φ threshold for conscious action selection */
#define FEP_CONSCIOUSNESS_DEFAULT_PHI_THRESHOLD 0.3f

/** Default attention gain boost from consciousness */
#define FEP_CONSCIOUSNESS_DEFAULT_ATTENTION_GAIN 2.0f

/** Default coherence weight in action selection */
#define FEP_CONSCIOUSNESS_DEFAULT_COHERENCE_WEIGHT 0.5f

/** Default metacognitive intervention threshold */
#define FEP_CONSCIOUSNESS_DEFAULT_METACOGNITIVE_THRESHOLD 0.7f

/** Maximum precision boost factor */
#define FEP_CONSCIOUSNESS_MAX_PRECISION_BOOST 5.0f

/** Minimum precision factor */
#define FEP_CONSCIOUSNESS_MIN_PRECISION_FACTOR 0.1f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief FEP consciousness configuration
 *
 * WHAT: Configuration for consciousness-gated action selection
 * WHY:  Control how consciousness influences FEP active inference
 * HOW:  Thresholds and gains for gating and modulation
 */
typedef struct {
    float phi_threshold;              /**< Minimum Φ for conscious action selection */
    float attention_gain;             /**< How much consciousness boosts precision */
    float coherence_weight;           /**< Weight of action coherence in selection */
    float metacognitive_threshold;    /**< Threshold for metacognitive intervention */
    bool enable_global_workspace;     /**< Enable global workspace broadcasting */
    bool enable_metacognitive_control; /**< Enable metacognitive monitoring */
    bool enable_habitual_cache;       /**< Cache habitual responses for low Φ */
    uint32_t update_interval_ms;      /**< How often to update Φ (0 = every call) */
} fep_consciousness_config_t;

/**
 * @brief FEP consciousness state
 *
 * WHAT: Current consciousness state affecting FEP
 * WHY:  Track consciousness influence on action selection
 * HOW:  Φ, attention, coherence, metacognitive confidence
 */
typedef struct {
    float current_phi;                /**< Current Φ (integrated information) */
    float attention_level;            /**< Current attention level (0-1) */
    float action_coherence;           /**< Coherence of selected actions (0-1) */
    float metacognitive_confidence;   /**< Confidence in generative model (0-1) */
    bool conscious_access;            /**< Is current processing conscious? */
    consciousness_state_t consciousness_state; /**< IIT consciousness state */
    uint64_t last_phi_update_ms;      /**< Last Φ computation timestamp */
    uint32_t unconscious_actions;     /**< Count of unconscious/habitual actions */
    uint32_t conscious_actions;       /**< Count of conscious/deliberate actions */
    uint32_t metacognitive_interventions; /**< Count of metacognitive overrides */
} fep_consciousness_state_t;

/**
 * @brief Opaque FEP consciousness bridge handle
 *
 * WHAT: Bridge between consciousness metrics and FEP system
 * WHY:  Encapsulation - hide implementation details
 * HOW:  Pimpl idiom
 */
typedef struct fep_consciousness_bridge fep_consciousness_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP consciousness configuration
 *
 * WHAT: Provide sensible defaults for consciousness-FEP integration
 * WHY:  Easy initialization with biologically-plausible parameters
 * HOW:  Set defaults based on IIT and GWT research
 *
 * DEFAULTS:
 * - phi_threshold: 0.3 (empirical threshold for conscious processing)
 * - attention_gain: 2.0 (conscious attention doubles precision)
 * - coherence_weight: 0.5 (balance risk and coherence)
 * - metacognitive_threshold: 0.7 (intervene when confidence low)
 * - enable_global_workspace: true
 * - enable_metacognitive_control: true
 * - enable_habitual_cache: true (fast unconscious responses)
 * - update_interval_ms: 100 (10 Hz, similar to conscious perception rate)
 *
 * @param config Output configuration
 */
void fep_consciousness_default_config(fep_consciousness_config_t* config);

/**
 * @brief Create FEP consciousness bridge
 *
 * WHAT: Initialize consciousness-gated action selection system
 * WHY:  Enable consciousness to influence FEP active inference
 * HOW:  Allocate bridge, setup connections, initialize state
 *
 * BIOLOGICAL MEANING:
 * Creates the interface between conscious awareness (Φ) and action
 * selection, implementing Global Workspace Theory's gating mechanism.
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 *
 * ERRORS:
 * - Returns NULL if allocation fails
 * - Returns NULL if config is invalid
 *
 * MEMORY: Caller must call fep_consciousness_destroy() when done
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
fep_consciousness_bridge_t* fep_consciousness_create(
    const fep_consciousness_config_t* config
);

/**
 * @brief Destroy FEP consciousness bridge
 *
 * WHAT: Free all resources associated with bridge
 * WHY:  Prevent memory leaks
 * HOW:  Disconnect systems, free allocations
 *
 * @param bridge Bridge to destroy (NULL-safe)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (caller must ensure no concurrent access)
 */
void fep_consciousness_destroy(fep_consciousness_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect FEP system to consciousness bridge
 *
 * WHAT: Link consciousness bridge to FEP system
 * WHY:  Enable consciousness to gate and modulate FEP actions
 * HOW:  Store FEP reference, validate compatibility
 *
 * @param bridge Consciousness bridge
 * @param fep FEP system to connect
 * @return 0 on success, NIMCP_ERROR_* on failure
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if bridge or fep is NULL
 * - NIMCP_ERROR_INVALID_STATE if already connected
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int fep_consciousness_connect_fep(
    fep_consciousness_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect introspection system to consciousness bridge
 *
 * WHAT: Link introspection (consciousness metrics) to bridge
 * WHY:  Enable Φ computation and consciousness state access
 * HOW:  Store introspection reference, setup Φ computation
 *
 * BIOLOGICAL MEANING:
 * Connects the consciousness measurement system (IIT Φ) to the
 * action selection gating mechanism (Global Workspace).
 *
 * @param bridge Consciousness bridge
 * @param introspection Introspection context
 * @return 0 on success, NIMCP_ERROR_* on failure
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if bridge or introspection is NULL
 * - NIMCP_ERROR_INVALID_STATE if already connected
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int fep_consciousness_connect_introspection(
    fep_consciousness_bridge_t* bridge,
    introspection_context_t introspection
);

/**
 * @brief Disconnect all systems from bridge
 *
 * WHAT: Remove all connections from bridge
 * WHY:  Safe shutdown or reconfiguration
 * HOW:  Clear all system references
 *
 * @param bridge Consciousness bridge
 * @return 0 on success, NIMCP_ERROR_* on failure
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if bridge is NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int fep_consciousness_disconnect(fep_consciousness_bridge_t* bridge);

/* ============================================================================
 * Consciousness-Gated Operations API
 * ============================================================================ */

/**
 * @brief Gate action selection based on consciousness level
 *
 * WHAT: Apply consciousness-based gating to proposed action
 * WHY:  Implement Global Workspace Theory - consciousness gates actions
 * HOW:  If Φ < threshold, return habitual/cached action; else return proposed
 *
 * BIOLOGICAL MECHANISM:
 * - High Φ (conscious): Full deliberation, novel actions allowed
 * - Low Φ (unconscious): Fast habitual responses, cached actions
 * - Implements dual-process theory (System 1 vs System 2)
 *
 * ALGORITHM:
 * 1. Check current Φ level
 * 2. If Φ < threshold:
 *    - Check habitual cache for similar state
 *    - Return cached action (fast, unconscious)
 * 3. If Φ ≥ threshold:
 *    - Return proposed action (deliberate, conscious)
 *    - Cache for future habitual use
 *
 * @param bridge Consciousness bridge
 * @param proposed_action Action ID proposed by FEP
 * @param gated_action Output: action after consciousness gating
 * @return 0 on success, NIMCP_ERROR_* on failure
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if bridge or gated_action is NULL
 * - NIMCP_ERROR_INVALID_STATE if not connected to FEP/introspection
 *
 * COMPLEXITY: O(1) for cache hit, O(log n) for cache miss
 * THREAD-SAFE: Yes
 */
int fep_consciousness_gate_action(
    fep_consciousness_bridge_t* bridge,
    uint32_t proposed_action,
    uint32_t* gated_action
);

/**
 * @brief Modulate precision based on consciousness level
 *
 * WHAT: Adjust precision (inverse variance) based on Φ
 * WHY:  Attention as precision - consciousness boosts gain on predictions
 * HOW:  Scale precision by Φ-dependent factor
 *
 * BIOLOGICAL MECHANISM:
 * - Conscious attention increases precision (gain) on attended features
 * - High Φ → high precision → stronger belief updates
 * - Low Φ → low precision → weaker belief updates, less learning
 * - Implements Friston's "attention as precision" framework
 *
 * FORMULA:
 *   precision_out = precision_in × (1 + attention_gain × Φ)
 *
 * Where:
 *   attention_gain = configured boost factor (default 2.0)
 *   Φ = current integrated information (0-1)
 *
 * @param bridge Consciousness bridge
 * @param precision Input/output: precision to modulate
 * @return 0 on success, NIMCP_ERROR_* on failure
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if bridge or precision is NULL
 * - NIMCP_ERROR_INVALID_STATE if not connected
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int fep_consciousness_modulate_precision(
    fep_consciousness_bridge_t* bridge,
    float* precision
);

/**
 * @brief Evaluate action coherence for policy
 *
 * WHAT: Compute how coherent/integrated a policy's actions are
 * WHY:  High Φ should select coherent, integrated action sequences
 * HOW:  Measure consistency and integration across policy actions
 *
 * BIOLOGICAL MECHANISM:
 * - Conscious processing produces coherent, unified actions
 * - High Φ correlates with action integration
 * - Incoherent actions indicate low consciousness or conflict
 * - Coherence used to bias policy selection
 *
 * COHERENCE METRICS:
 * - Temporal consistency: Actions follow logical sequence
 * - Goal alignment: Actions serve unified goal
 * - Resource efficiency: Actions don't conflict/waste resources
 *
 * @param bridge Consciousness bridge
 * @param policy Policy to evaluate
 * @param coherence Output: coherence score (0-1)
 * @return 0 on success, NIMCP_ERROR_* on failure
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if bridge, policy, or coherence is NULL
 *
 * COMPLEXITY: O(n) where n = number of actions in policy
 * THREAD-SAFE: Yes
 */
int fep_consciousness_evaluate_coherence(
    fep_consciousness_bridge_t* bridge,
    const fep_policy_t* policy,
    float* coherence
);

/* ============================================================================
 * Metacognitive Functions API
 * ============================================================================ */

/**
 * @brief Assess generative model quality
 *
 * WHAT: Metacognitive evaluation of how good the FEP model is
 * WHY:  Type 2 metacognition - knowing how much we know
 * HOW:  Analyze prediction errors, free energy, model consistency
 *
 * BIOLOGICAL MECHANISM:
 * - Metacognition = "thinking about thinking"
 * - Type 2 processing assesses Type 1 (direct inference)
 * - High prediction errors → low model quality
 * - Low free energy convergence → poor model fit
 * - Triggers model revision or cautious action selection
 *
 * QUALITY METRICS:
 * - Prediction error magnitude (lower = better)
 * - Free energy trend (decreasing = good, increasing = bad)
 * - Belief convergence (converged = confident)
 * - Surprise level (low = expected, high = surprising)
 *
 * @param bridge Consciousness bridge
 * @param quality Output: model quality score (0-1, higher = better)
 * @return 0 on success, NIMCP_ERROR_* on failure
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if bridge or quality is NULL
 * - NIMCP_ERROR_INVALID_STATE if not connected to FEP
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int fep_consciousness_assess_model_quality(
    fep_consciousness_bridge_t* bridge,
    float* quality
);

/**
 * @brief Request conscious attention to specific target
 *
 * WHAT: Broadcast attention request via Global Workspace
 * WHY:  Attention prioritizes processing of salient information
 * HOW:  Signal to boost precision on target observations/beliefs
 *
 * BIOLOGICAL MECHANISM:
 * - Global Workspace broadcasts attention signals
 * - Attended information receives processing priority
 * - Precision boost on attended features
 * - Implements Dehaene's Global Workspace architecture
 *
 * @param bridge Consciousness bridge
 * @param target Target ID to attend to (observation/belief index)
 * @param priority Attention priority (0-1)
 * @return 0 on success, NIMCP_ERROR_* on failure
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if bridge is NULL
 * - NIMCP_ERROR_OUT_OF_RANGE if priority not in [0,1]
 * - NIMCP_ERROR_INVALID_STATE if global workspace not enabled
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int fep_consciousness_request_attention(
    fep_consciousness_bridge_t* bridge,
    uint32_t target,
    float priority
);

/* ============================================================================
 * Update and State API
 * ============================================================================ */

/**
 * @brief Update consciousness state
 *
 * WHAT: Recompute Φ and update consciousness-related state
 * WHY:  Consciousness level changes over time, needs periodic update
 * HOW:  Call introspection_compute_phi(), update attention, coherence
 *
 * WHEN TO CALL:
 * - Periodically based on update_interval_ms
 * - After significant brain state changes
 * - Before critical action selections
 * - When metacognitive assessment needed
 *
 * @param bridge Consciousness bridge
 * @param delta_ms Milliseconds since last update
 * @return 0 on success, NIMCP_ERROR_* on failure
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if bridge is NULL
 * - NIMCP_ERROR_INVALID_STATE if not connected to introspection
 *
 * COMPLEXITY: O(n^2) where n = brain size (Φ computation)
 * TIME: ~10-200ms depending on network size and Φ method
 * THREAD-SAFE: Yes
 */
int fep_consciousness_update(
    fep_consciousness_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Get current consciousness state
 *
 * WHAT: Retrieve current Φ, attention, coherence, metacognitive state
 * WHY:  Query consciousness influence on FEP
 * HOW:  Copy internal state to output structure
 *
 * @param bridge Consciousness bridge
 * @param state Output: current consciousness state
 * @return 0 on success, NIMCP_ERROR_* on failure
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if bridge or state is NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int fep_consciousness_get_state(
    const fep_consciousness_bridge_t* bridge,
    fep_consciousness_state_t* state
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with bio-async communication system
 * WHY:  Enable inter-module messaging for consciousness events
 * HOW:  Register as BIO_MODULE_FEP_CONSCIOUSNESS, setup inbox
 *
 * MESSAGES SENT:
 * - Consciousness state changes (Φ updates)
 * - Metacognitive interventions
 * - Attention requests (Global Workspace broadcasts)
 * - Action gating decisions
 *
 * MESSAGES RECEIVED:
 * - Attention shift requests from other modules
 * - Model quality queries
 * - Consciousness state queries
 *
 * @param bridge Consciousness bridge
 * @return 0 on success, NIMCP_ERROR_* on failure
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if bridge is NULL
 * - NIMCP_ERROR_OPERATION_FAILED if bio-async registration fails
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int fep_consciousness_connect_bio_async(fep_consciousness_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async system
 * WHY:  Clean shutdown
 * HOW:  Deregister module, clear bio context
 *
 * @param bridge Consciousness bridge
 * @return 0 on success, NIMCP_ERROR_* on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int fep_consciousness_disconnect_bio_async(fep_consciousness_bridge_t* bridge);

/**
 * @brief Check if connected to bio-async
 *
 * @param bridge Consciousness bridge
 * @return true if connected, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool fep_consciousness_is_bio_async_connected(
    const fep_consciousness_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEP_CONSCIOUSNESS_H */
