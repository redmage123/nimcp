/**
 * @file nimcp_fep_context.h
 * @brief Free Energy Principle Contextual Model Switching
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Context-dependent generative model switching for Free Energy Principle
 * WHY:  Cognitive efficiency requires task-specific models (driving vs. reading);
 *       rapid context switching enables flexible inference across situations.
 * HOW:  Maintain library of context-specific models, infer context from observations,
 *       switch between models using hard/soft/gated strategies.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PREFRONTAL CORTEX CONTEXT REPRESENTATION:
 * -----------------------------------------
 * - Lateral PFC maintains task sets (context representations)
 * - Rapid switching between behavioral strategies (~100-300ms)
 * - Context signals gate information processing in posterior cortex
 * - Evidence from Miller & Cohen (2001) "An integrative theory of PFC function"
 *
 * HIPPOCAMPAL CONTEXT INDEXING:
 * -----------------------------
 * - Hippocampus indexes memories by context (spatial, temporal, emotional)
 * - Pattern separation creates distinct representations for different contexts
 * - Pattern completion retrieves context-appropriate memories
 * - Reference: Smith & Mizumori (2006) "Hippocampal place cells, context, and
 *   episodic memory"
 *
 * SCHEMA THEORY:
 * --------------
 * - Pre-compiled generative models (schemas) for familiar situations
 * - Rapid inference by activating appropriate schema
 * - Schema switching when prediction errors accumulate
 * - Reference: Ghosh & Gilboa (2014) "What is a memory schema? A historical
 *   perspective on current neuroscience literature"
 *
 * PREDICTIVE PROCESSING VIEW:
 * ---------------------------
 * - Different contexts = different priors and likelihood functions
 * - Context inference as model selection (maximize evidence)
 * - Context switching when current model fails to explain observations
 * - Reference: Friston et al. (2017) "Active inference, curiosity and insight"
 *
 * SWITCHING MECHANISMS:
 * ---------------------
 * 1. HARD SWITCHING:
 *    - Immediate full context change (doorway effect, scene changes)
 *    - Complete replacement of generative model
 *    - Fast but potentially disruptive
 *
 * 2. SOFT SWITCHING:
 *    - Gradual interpolation between contexts (twilight, gradual transitions)
 *    - Weighted blend of multiple models
 *    - Smoother but requires maintaining multiple models
 *
 * 3. GATED SWITCHING:
 *    - Evidence-based threshold crossing (confidence-gated)
 *    - Switch only when evidence is strong enough
 *    - Prevents spurious context changes
 *
 * CONTEXT LEARNING:
 * -----------------
 * - Extract context-specific statistics from experience
 * - Update context priors and transition dynamics
 * - Create new contexts for novel situations
 * - Reference: Collins & Frank (2013) "Cognitive control over learning"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    FEP CONTEXTUAL MODEL SWITCHING                          ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   CONTEXT LIBRARY                                   │  ║
 * ║   │                                                                     │  ║
 * ║   │   Context 1: "Driving"         Activation: 0.85                    │  ║
 * ║   │   ├─ Prior beliefs: [...]                                          │  ║
 * ║   │   ├─ Transition dynamics: [...]                                    │  ║
 * ║   │   └─ Last used: 1000ms ago                                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   Context 2: "Reading"         Activation: 0.12                    │  ║
 * ║   │   ├─ Prior beliefs: [...]                                          │  ║
 * ║   │   ├─ Transition dynamics: [...]                                    │  ║
 * ║   │   └─ Last used: 5000ms ago                                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   Context 3: "Social"          Activation: 0.03                    │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   CONTEXT INFERENCE                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   Observation o → Evidence computation for each context            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ln p(o|context_i) = -F_i (Free energy under context i)          │  ║
 * ║   │                                                                     │  ║
 * ║   │   Context probabilities: softmax over evidence                     │  ║
 * ║   │   p(context_i|o) ∝ exp(-F_i)                                      │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   SWITCHING STRATEGIES                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   HARD:   Model ← Context_new (immediate replacement)              │  ║
 * ║   │                                                                     │  ║
 * ║   │   SOFT:   Model ← α*Context_old + (1-α)*Context_new                │  ║
 * ║   │           α(t) = α(t-1) * interpolation_rate                       │  ║
 * ║   │                                                                     │  ║
 * ║   │   GATED:  if confidence > threshold:                               │  ║
 * ║   │             Model ← Context_new                                     │  ║
 * ║   │           else:                                                     │  ║
 * ║   │             Model ← Context_old                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   CONTEXT LEARNING                                  │  ║
 * ║   │                                                                     │  ║
 * ║   │   Update context from experience:                                  │  ║
 * ║   │   μ_context ← μ_context + η * (μ_current - μ_context)             │  ║
 * ║   │                                                                     │  ║
 * ║   │   Create new context:                                              │  ║
 * ║   │   μ_new ← Extract(current_beliefs)                                 │  ║
 * ║   │   Add to library                                                   │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * USAGE EXAMPLE:
 * ```c
 * // Create context system
 * fep_context_config_t config;
 * fep_context_default_config(&config);
 * config.max_contexts = 10;
 * config.switch_mode = CONTEXT_SWITCH_SOFT;
 * fep_context_system_t* ctx_sys = fep_context_create(&config);
 *
 * // Add contexts
 * float driving_priors[64] = {...};
 * uint32_t driving_id;
 * fep_context_add(ctx_sys, "driving", driving_priors, 64, &driving_id);
 *
 * float reading_priors[64] = {...};
 * uint32_t reading_id;
 * fep_context_add(ctx_sys, "reading", reading_priors, 64, &reading_id);
 *
 * // Connect to FEP system
 * fep_context_connect(ctx_sys, fep);
 *
 * // Infer and switch context from observation
 * uint32_t inferred_id;
 * float confidence;
 * fep_context_infer(ctx_sys, fep, observation, obs_dim, &inferred_id, &confidence);
 * fep_context_auto_switch(ctx_sys, fep, observation, obs_dim);
 *
 * // Manual context switching
 * fep_context_switch(ctx_sys, fep, reading_id);
 *
 * // Learn context from current experience
 * fep_context_learn_from_experience(ctx_sys, fep, driving_id);
 * ```
 *
 * REFERENCES:
 * - Miller & Cohen (2001) "An integrative theory of prefrontal cortex function"
 * - Smith & Mizumori (2006) "Hippocampal place cells, context, and episodic memory"
 * - Ghosh & Gilboa (2014) "What is a memory schema?"
 * - Friston et al. (2017) "Active inference, curiosity and insight"
 * - Collins & Frank (2013) "Cognitive control over learning"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FEP_CONTEXT_H
#define NIMCP_FEP_CONTEXT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
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

#define FEP_CONTEXT_MAX_NAME_LEN       64      /**< Maximum context name length */
#define FEP_CONTEXT_DEFAULT_MAX        16      /**< Default max contexts */
#define FEP_CONTEXT_DEFAULT_THRESHOLD  0.7f    /**< Default switch threshold */
#define FEP_CONTEXT_DEFAULT_INTERP     0.9f    /**< Default interpolation rate */
#define FEP_CONTEXT_DEFAULT_DECAY      0.95f   /**< Default activation decay */
#define FEP_CONTEXT_MIN_CONFIDENCE     0.5f    /**< Minimum confidence for gated switch */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Context switching modes
 *
 * WHAT: Strategies for transitioning between contexts
 * WHY:  Different situations require different switching behaviors
 * HOW:  Hard = immediate, Soft = gradual, Gated = threshold-based
 */
typedef enum {
    CONTEXT_SWITCH_HARD = 0,     /**< Immediate full replacement */
    CONTEXT_SWITCH_SOFT,         /**< Gradual interpolation */
    CONTEXT_SWITCH_GATED         /**< Evidence-threshold based */
} fep_context_switch_mode_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for context system
 */
typedef struct {
    uint32_t max_contexts;                 /**< Maximum number of contexts */
    fep_context_switch_mode_t switch_mode; /**< Switching strategy */
    float switch_threshold;                /**< Evidence threshold for switching */
    float interpolation_rate;              /**< Alpha decay for soft switching */
    float context_decay_rate;              /**< Unused context activation decay */
    bool enable_context_learning;          /**< Allow learning from experience */
    bool enable_context_creation;          /**< Allow creating new contexts */
} fep_context_config_t;

/**
 * @brief Single context (generative model variant)
 *
 * WHAT: Context-specific priors and dynamics
 * WHY:  Different situations have different statistical regularities
 * HOW:  Store beliefs, transitions, and metadata per context
 */
typedef struct {
    uint32_t context_id;                   /**< Unique context identifier */
    char name[FEP_CONTEXT_MAX_NAME_LEN];   /**< Human-readable name */

    /* Context-specific model parameters */
    float* prior_beliefs;                  /**< Context priors μ₀ */
    float* transition_matrix;              /**< Context dynamics A */
    uint32_t belief_dim;                   /**< Belief dimensionality */
    uint32_t transition_dim;               /**< Transition matrix dimension */

    /* Context state */
    float activation;                      /**< Current activation level [0,1] */
    uint64_t last_used;                    /**< Timestamp of last use (ms) */
    uint64_t use_count;                    /**< Number of times used */
} fep_context_t;

/**
 * @brief System state snapshot
 */
typedef struct {
    uint32_t active_context_id;            /**< Currently active context */
    float active_context_confidence;       /**< Confidence in active context */
    uint32_t num_contexts;                 /**< Number of registered contexts */
    bool switching_in_progress;            /**< Currently switching contexts */
} fep_context_state_t;

/**
 * @brief Opaque context system handle
 */
typedef struct fep_context_system fep_context_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Populate config with sensible defaults
 * WHY:  Easy initialization without manual setup
 * HOW:  Set biologically-plausible default values
 *
 * @param config Output configuration structure
 */
void fep_context_default_config(fep_context_config_t* config);

/**
 * @brief Create context system
 *
 * WHAT: Initialize contextual model switching system
 * WHY:  Enable flexible multi-context inference
 * HOW:  Allocate context library, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New context system or NULL on failure
 */
fep_context_system_t* fep_context_create(const fep_context_config_t* config);

/**
 * @brief Destroy context system
 *
 * WHAT: Free all context resources
 * WHY:  Clean memory management
 * HOW:  Free contexts, bio-async, mutex
 *
 * @param sys Context system (NULL safe)
 */
void fep_context_destroy(fep_context_system_t* sys);

/* ============================================================================
 * Context Management API
 * ============================================================================ */

/**
 * @brief Add new context to library
 *
 * WHAT: Register context-specific generative model
 * WHY:  Build repertoire of situation-specific models
 * HOW:  Allocate context, copy parameters, add to library
 *
 * @param sys Context system
 * @param name Human-readable context name
 * @param prior_beliefs Prior belief vector for this context
 * @param belief_dim Dimensionality of beliefs
 * @param context_id Output context ID
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_add(
    fep_context_system_t* sys,
    const char* name,
    const float* prior_beliefs,
    size_t belief_dim,
    uint32_t* context_id
);

/**
 * @brief Remove context from library
 *
 * WHAT: Delete context by ID
 * WHY:  Prune unused or obsolete contexts
 * HOW:  Find context, free resources, compact array
 *
 * @param sys Context system
 * @param context_id Context to remove
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_remove(fep_context_system_t* sys, uint32_t context_id);

/**
 * @brief Get context information
 *
 * WHAT: Retrieve context metadata and parameters
 * WHY:  Inspect context library contents
 * HOW:  Copy context structure to output
 *
 * @param sys Context system
 * @param context_id Context to query
 * @param context Output context structure
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_get(
    const fep_context_system_t* sys,
    uint32_t context_id,
    fep_context_t* context
);

/**
 * @brief Update context parameters
 *
 * WHAT: Modify context priors or dynamics
 * WHY:  Adapt contexts to changing statistics
 * HOW:  Replace belief vector with new values
 *
 * @param sys Context system
 * @param context_id Context to update
 * @param new_beliefs New belief vector
 * @param belief_dim Dimensionality
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_update(
    fep_context_system_t* sys,
    uint32_t context_id,
    const float* new_beliefs,
    size_t belief_dim
);

/* ============================================================================
 * Context Switching API
 * ============================================================================ */

/**
 * @brief Manually switch to target context
 *
 * WHAT: Apply target context model to FEP system
 * WHY:  Explicit context change from external signal
 * HOW:  Replace FEP priors/dynamics with context-specific values
 *
 * @param sys Context system
 * @param fep FEP system to modify
 * @param target_context_id Context to switch to
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_switch(
    fep_context_system_t* sys,
    fep_system_t* fep,
    uint32_t target_context_id
);

/**
 * @brief Infer most likely context from observation
 *
 * WHAT: Compute evidence for each context given observation
 * WHY:  Model selection - which context explains data best
 * HOW:  Compute free energy under each context, softmax over evidence
 *
 * @param sys Context system
 * @param fep FEP system for evidence computation
 * @param observation Observed data
 * @param obs_dim Observation dimensionality
 * @param inferred_context_id Output: most likely context
 * @param confidence Output: confidence in inference [0,1]
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_infer(
    fep_context_system_t* sys,
    fep_system_t* fep,
    const float* observation,
    size_t obs_dim,
    uint32_t* inferred_context_id,
    float* confidence
);

/**
 * @brief Automatic context switching based on observation
 *
 * WHAT: Infer context and switch if appropriate
 * WHY:  Autonomous adaptation to changing situations
 * HOW:  Infer context, apply switching strategy (hard/soft/gated)
 *
 * @param sys Context system
 * @param fep FEP system to modify
 * @param observation Current observation
 * @param obs_dim Observation dimensionality
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_auto_switch(
    fep_context_system_t* sys,
    fep_system_t* fep,
    const float* observation,
    size_t obs_dim
);

/* ============================================================================
 * Context Application API
 * ============================================================================ */

/**
 * @brief Apply context to FEP system
 *
 * WHAT: Load context priors and dynamics into FEP
 * WHY:  Instantiate context-specific generative model
 * HOW:  Copy context parameters to FEP levels
 *
 * @param sys Context system
 * @param fep FEP system to modify
 * @param context_id Context to apply
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_apply(
    fep_context_system_t* sys,
    fep_system_t* fep,
    uint32_t context_id
);

/**
 * @brief Blend two contexts
 *
 * WHAT: Interpolate between two context models
 * WHY:  Smooth transitions for soft switching
 * HOW:  Weighted average: result = α*ctx1 + (1-α)*ctx2
 *
 * @param sys Context system
 * @param fep FEP system to modify
 * @param context1_id First context
 * @param context2_id Second context
 * @param blend_factor Blend weight [0=ctx1, 1=ctx2]
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_blend(
    fep_context_system_t* sys,
    fep_system_t* fep,
    uint32_t context1_id,
    uint32_t context2_id,
    float blend_factor
);

/* ============================================================================
 * Context Learning API
 * ============================================================================ */

/**
 * @brief Learn context from current experience
 *
 * WHAT: Update context parameters from FEP's current beliefs
 * WHY:  Adapt contexts to changing statistics
 * HOW:  Moving average: μ_ctx ← μ_ctx + η*(μ_current - μ_ctx)
 *
 * @param sys Context system
 * @param fep FEP system (source of current beliefs)
 * @param context_id Context to update
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_learn_from_experience(
    fep_context_system_t* sys,
    fep_system_t* fep,
    uint32_t context_id
);

/**
 * @brief Create new context from current FEP state
 *
 * WHAT: Extract current beliefs as new context
 * WHY:  Build new context for novel situations
 * HOW:  Copy FEP beliefs to new context, add to library
 *
 * @param sys Context system
 * @param fep FEP system (source of beliefs)
 * @param name Name for new context
 * @param new_context_id Output: ID of created context
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_create_from_current(
    fep_context_system_t* sys,
    fep_system_t* fep,
    const char* name,
    uint32_t* new_context_id
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current system state
 *
 * WHAT: Snapshot of context system state
 * WHY:  Monitor active context and switching status
 * HOW:  Copy state fields to output structure
 *
 * @param sys Context system
 * @param state Output state structure
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_get_state(
    const fep_context_system_t* sys,
    fep_context_state_t* state
);

/**
 * @brief Get active context ID
 *
 * WHAT: Query which context is currently active
 * WHY:  Track context switches
 * HOW:  Return active_context_id field
 *
 * @param sys Context system
 * @param context_id Output context ID
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_get_active(
    const fep_context_system_t* sys,
    uint32_t* context_id
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect context system to FEP system
 *
 * WHAT: Establish link for context switching
 * WHY:  Enable context manipulation of FEP beliefs
 * HOW:  Store FEP pointer in context system
 *
 * @param context Context system
 * @param fep FEP system to control
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_connect(fep_context_system_t* context, fep_system_t* fep);

/**
 * @brief Connect to bio-async messaging
 *
 * WHAT: Register with bio-async router
 * WHY:  Enable inter-module context notifications
 * HOW:  Register as BIO_MODULE_FEP_CONTEXT
 *
 * @param sys Context system
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_connect_bio_async(fep_context_system_t* sys);

/**
 * @brief Disconnect from bio-async
 *
 * WHAT: Unregister from bio-async router
 * WHY:  Clean shutdown
 * HOW:  Unregister module context
 *
 * @param sys Context system
 * @return NIMCP_SUCCESS or error code
 */
int fep_context_disconnect_bio_async(fep_context_system_t* sys);

/**
 * @brief Check if bio-async is connected
 *
 * @param sys Context system
 * @return true if connected, false otherwise
 */
bool fep_context_is_bio_async_connected(const fep_context_system_t* sys);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEP_CONTEXT_H */
