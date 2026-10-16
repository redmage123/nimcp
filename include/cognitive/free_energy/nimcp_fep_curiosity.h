/**
 * @file nimcp_fep_curiosity.h
 * @brief Epistemic Value and Curiosity for Free Energy Principle
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of curiosity-driven exploration via epistemic value
 * WHY:  Organisms seek information to reduce uncertainty about their environment.
 *       Curiosity emerges from minimizing expected free energy via information gain.
 * HOW:  Compute epistemic value (information gain), empowerment (control), and
 *       novelty to modulate action selection toward exploratory behaviors.
 *
 * THEORETICAL FOUNDATION:
 * ==================================================================================
 *
 * EPISTEMIC VALUE (Friston et al., 2015):
 * ---------------------------------------
 * Epistemic value measures expected information gain from observations:
 *
 *   EV = E_q(o|π)[H[p(s)] - H[p(s|o)]]
 *      = E[Complexity reduction]
 *      = I[s, o|π]  (mutual information)
 *
 * Where:
 *   H[p(s)] = Prior entropy (uncertainty before observation)
 *   H[p(s|o)] = Posterior entropy (uncertainty after observation)
 *   I[s, o|π] = Mutual information between states and observations under policy π
 *
 * EXPECTED FREE ENERGY WITH CURIOSITY:
 * ------------------------------------
 * G(π) = E_q(o,s|π)[ln q(s|π) - ln p(o,s|π)]
 *      = Risk + Ambiguity - Epistemic Value
 *
 * Risk = Extrinsic value (goal-directed)
 * Ambiguity = Uncertainty about observations
 * Epistemic Value = Information gain (intrinsic motivation)
 *
 * INFORMATION GAIN COMPUTATION:
 * -----------------------------
 * Information gain = KL[q(s|o)||q(s)] between posterior and prior:
 *
 *   IG = ∫ q(s|o) ln[q(s|o)/q(s)] ds
 *
 * High IG → observations that resolve uncertainty → curiosity-driven exploration
 *
 * EMPOWERMENT (Salge et al., 2014):
 * ---------------------------------
 * Empowerment = mutual information between actions and future states:
 *
 *   Emp = I[a, s'|s] = H[s'|s] - H[s'|a,s]
 *
 * Measures "how much control" the agent has over its future:
 * - High empowerment states → many possible futures (high control)
 * - Low empowerment states → limited options (low control)
 *
 * NOVELTY DETECTION:
 * ------------------
 * Two approaches:
 *
 * 1. Count-based: track state visitation counts
 *    Novelty ∝ 1/√count(s)
 *
 * 2. Prediction-error: error between predicted and actual observations
 *    Novelty ∝ ||o - g(μ)||²
 *
 * CURIOSITY TYPES:
 * ----------------
 * 1. EPISTEMIC: Pure information gain (reduce uncertainty)
 * 2. EMPOWERMENT: Maximize future options (increase control)
 * 3. COMPETENCE: Seek optimal challenge (flow state)
 * 4. NOVELTY: Prefer unfamiliar states (exploration bonus)
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * - Dopaminergic system: Reward prediction errors signal novelty/surprise
 * - Anterior cingulate cortex (ACC): Prediction error monitoring
 * - Locus coeruleus (LC): Noradrenergic novelty detection
 * - Hippocampus: Novelty detection via mismatch detection
 * - Prefrontal cortex: Goal-directed vs. exploratory trade-offs
 *
 * EXPLORATION-EXPLOITATION TRADE-OFF:
 * -----------------------------------
 * Total value = Extrinsic value (exploitation) + Epistemic value (exploration)
 *
 * Balance controlled by:
 * - Precision of preferences (high precision → exploitation)
 * - Epistemic bonus weighting (high weight → exploration)
 * - Environmental uncertainty (high uncertainty → exploration)
 *
 * REFERENCES:
 * - Friston et al. (2015) "Active inference and epistemic value"
 * - Friston et al. (2017) "Active inference: A process theory"
 * - Salge et al. (2014) "Empowerment - An introduction"
 * - Schmidhuber (2010) "Formal theory of creativity, fun, and intrinsic motivation"
 * - Oudeyer & Kaplan (2007) "What is intrinsic motivation?"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    FEP CURIOSITY SYSTEM                                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               EPISTEMIC VALUE COMPUTATION                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   Prior Beliefs q(s)                                                │  ║
 * ║   │         ↓                                                           │  ║
 * ║   │   Observation o → Posterior q(s|o)                                  │  ║
 * ║   │         ↓                                                           │  ║
 * ║   │   Information Gain = KL[q(s|o)||q(s)]                              │  ║
 * ║   │         ↓                                                           │  ║
 * ║   │   Epistemic Value = E[IG]                                           │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               EMPOWERMENT COMPUTATION                               │  ║
 * ║   │                                                                     │  ║
 * ║   │   Current State s                                                   │  ║
 * ║   │         ↓                                                           │  ║
 * ║   │   Future State Distribution p(s'|s)                                 │  ║
 * ║   │         ↓                                                           │  ║
 * ║   │   Controlled Distribution p(s'|a,s)                                 │  ║
 * ║   │         ↓                                                           │  ║
 * ║   │   Empowerment = I[a, s'|s]                                          │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               NOVELTY DETECTION                                     │  ║
 * ║   │                                                                     │  ║
 * ║   │   State Memory Bank                                                 │  ║
 * ║   │   ┌─────────────────────────────┐                                   │  ║
 * ║   │   │ s₁ → count₁                 │                                   │  ║
 * ║   │   │ s₂ → count₂                 │                                   │  ║
 * ║   │   │ s₃ → count₃                 │                                   │  ║
 * ║   │   └─────────────────────────────┘                                   │  ║
 * ║   │         ↓                                                           │  ║
 * ║   │   Novelty = 1/√count(s)                                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               ACTION MODULATION                                     │  ║
 * ║   │                                                                     │  ║
 * ║   │   G(π) = Risk + Ambiguity - α·EpistemicValue                        │  ║
 * ║   │                            - β·Empowerment                          │  ║
 * ║   │                            - γ·Novelty                              │  ║
 * ║   │         ↓                                                           │  ║
 * ║   │   P(π) ∝ exp(-G(π)/temperature)                                     │  ║
 * ║   │         ↓                                                           │  ║
 * ║   │   Select Action → Exploratory Behavior                              │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FEP_CURIOSITY_H
#define NIMCP_FEP_CURIOSITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/tensor/nimcp_tensor.h"
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

/** Default exploration bonus */
#define FEP_CURIOSITY_DEFAULT_EXPLORATION_BONUS    0.1f

/** Default novelty threshold */
#define FEP_CURIOSITY_DEFAULT_NOVELTY_THRESHOLD    0.5f

/** Default information gain weight */
#define FEP_CURIOSITY_DEFAULT_INFO_GAIN_WEIGHT     1.0f

/** Default empowerment weight */
#define FEP_CURIOSITY_DEFAULT_EMPOWERMENT_WEIGHT   0.5f

/** Default memory capacity for novelty detection */
#define FEP_CURIOSITY_DEFAULT_MEMORY_CAPACITY      1000

/** Minimum count for novelty computation */
#define FEP_CURIOSITY_MIN_COUNT                    1

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Curiosity types
 *
 * WHAT: Different modes of curiosity-driven exploration
 * WHY:  Different cognitive goals require different exploration strategies
 * HOW:  Enum-based selection of curiosity computation method
 */
typedef enum {
    CURIOSITY_EPISTEMIC = 0,      /**< Pure information gain (reduce uncertainty) */
    CURIOSITY_EMPOWERMENT,        /**< Maximize future options (increase control) */
    CURIOSITY_COMPETENCE,         /**< Seek optimal challenge (flow state) */
    CURIOSITY_NOVELTY             /**< Prefer unfamiliar states (exploration bonus) */
} fep_curiosity_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Curiosity system configuration
 */
typedef struct {
    fep_curiosity_type_t type;          /**< Primary curiosity mode */
    float exploration_bonus;             /**< Bonus weight for exploration */
    float novelty_threshold;             /**< Threshold for novelty detection */
    float information_gain_weight;       /**< Weight for epistemic value */
    float empowerment_weight;            /**< Weight for empowerment */
    bool enable_intrinsic_motivation;    /**< Enable intrinsic reward signals */
    size_t memory_capacity;              /**< State memory capacity */
} fep_curiosity_config_t;

/**
 * @brief Current curiosity state
 */
typedef struct {
    float epistemic_value;       /**< Current epistemic value */
    float information_gain;      /**< Recent information gain */
    float empowerment;           /**< Current empowerment estimate */
    float novelty_score;         /**< Current novelty score */
    float exploration_drive;     /**< Overall exploration drive [0,1] */
} fep_curiosity_state_t;

/**
 * @brief Curiosity statistics
 */
typedef struct {
    uint64_t observations_processed; /**< Total observations processed */
    uint64_t novel_states_found;     /**< Number of novel states encountered */
    float total_information_gain;    /**< Cumulative information gain */
    float avg_epistemic_value;       /**< Average epistemic value */
} fep_curiosity_stats_t;

/**
 * @brief Opaque curiosity system handle
 */
typedef struct fep_curiosity_system fep_curiosity_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default curiosity configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible parameters
 * HOW:  Set standard exploration/exploitation balance
 *
 * @param config Output configuration structure
 */
void fep_curiosity_default_config(fep_curiosity_config_t* config);

/**
 * @brief Create curiosity system
 *
 * WHAT: Initialize epistemic value computation system
 * WHY:  Enable curiosity-driven exploration in FEP agents
 * HOW:  Allocate memory structures, initialize state tracking
 *
 * @param config Configuration (NULL = defaults)
 * @return New curiosity system or NULL on failure
 */
fep_curiosity_system_t* fep_curiosity_create(const fep_curiosity_config_t* config);

/**
 * @brief Destroy curiosity system
 *
 * WHAT: Free all resources
 * WHY:  Clean shutdown, prevent memory leaks
 * HOW:  Release memory, destroy mutex
 *
 * @param sys Curiosity system (NULL safe)
 */
void fep_curiosity_destroy(fep_curiosity_system_t* sys);

/**
 * @brief Reset curiosity system to initial state
 *
 * WHAT: Clear state memory, reset statistics
 * WHY:  Prepare for new environment or task
 * HOW:  Clear novelty memory, zero counters
 *
 * @param sys Curiosity system
 * @return 0 on success, negative on error
 */
int fep_curiosity_reset(fep_curiosity_system_t* sys);

/* ============================================================================
 * Core Computation API
 * ============================================================================ */

/**
 * @brief Compute epistemic value for a policy
 *
 * WHAT: Calculate expected information gain under policy
 * WHY:  Epistemic value drives information-seeking behavior
 * HOW:  Compute KL[q(s|o,π)||q(s|π)] averaged over predicted observations
 *
 * @param sys Curiosity system
 * @param fep FEP system (for beliefs and generative model)
 * @param policy Policy to evaluate
 * @return Epistemic value (higher = more informative)
 */
float fep_compute_epistemic_value(
    fep_curiosity_system_t* sys,
    fep_system_t* fep,
    const fep_policy_t* policy
);

/**
 * @brief Compute information gain from observation
 *
 * WHAT: Calculate KL divergence between posterior and prior
 * WHY:  Information gain measures uncertainty reduction
 * HOW:  KL[q(s|o)||q(s)] = ∫ q(s|o) ln[q(s|o)/q(s)] ds
 *
 * @param sys Curiosity system
 * @param fep FEP system
 * @param observation Observation vector
 * @param dim Observation dimensionality
 * @return Information gain (nats)
 */
float fep_compute_information_gain(
    fep_curiosity_system_t* sys,
    fep_system_t* fep,
    const float* observation,
    size_t dim
);

/**
 * @brief Compute empowerment for current state
 *
 * WHAT: Calculate mutual information I[a, s'|s]
 * WHY:  Empowerment measures control over future states
 * HOW:  Estimate I[a, s'|s] = H[s'|s] - H[s'|a,s]
 *
 * @param sys Curiosity system
 * @param fep FEP system
 * @param state State vector
 * @param dim State dimensionality
 * @return Empowerment (bits)
 */
float fep_compute_empowerment(
    fep_curiosity_system_t* sys,
    fep_system_t* fep,
    const float* state,
    size_t dim
);

/**
 * @brief Compute novelty score for state
 *
 * WHAT: Measure how unfamiliar the state is
 * WHY:  Novelty drives exploratory behavior
 * HOW:  Count-based: novelty ∝ 1/√(count + 1)
 *
 * @param sys Curiosity system
 * @param state State vector
 * @param dim State dimensionality
 * @return Novelty score [0, 1]
 */
float fep_compute_novelty(
    fep_curiosity_system_t* sys,
    const float* state,
    size_t dim
);

/* ============================================================================
 * Active Inference Integration API
 * ============================================================================ */

/**
 * @brief Modulate expected free energy with epistemic value
 *
 * WHAT: Add curiosity bonus to EFE
 * WHY:  Balance exploration (epistemic) and exploitation (extrinsic)
 * HOW:  G'(π) = G(π) - α·EpistemicValue(π) - β·Empowerment - γ·Novelty
 *
 * @param sys Curiosity system
 * @param fep FEP system
 * @param efe Expected free energy structure (modified in-place)
 * @return 0 on success, negative on error
 */
int fep_curiosity_modulate_efe(
    fep_curiosity_system_t* sys,
    fep_system_t* fep,
    fep_efe_t* efe
);

/**
 * @brief Select action with curiosity-driven exploration
 *
 * WHAT: Choose action balancing exploration and exploitation
 * WHY:  Pure exploitation can lead to local optima
 * HOW:  Evaluate policies with epistemic bonus, softmax selection
 *
 * @param sys Curiosity system
 * @param fep FEP system
 * @param action Output action index
 * @return 0 on success, negative on error
 */
int fep_curiosity_select_action(
    fep_curiosity_system_t* sys,
    fep_system_t* fep,
    uint32_t* action
);

/* ============================================================================
 * State Tracking API
 * ============================================================================ */

/**
 * @brief Record observation for novelty tracking
 *
 * WHAT: Update state visitation memory
 * WHY:  Novelty detection requires state history
 * HOW:  Hash state, increment count in memory bank
 *
 * @param sys Curiosity system
 * @param observation Observation vector
 * @param dim Dimensionality
 * @return 0 on success, negative on error
 */
int fep_curiosity_record_observation(
    fep_curiosity_system_t* sys,
    const float* observation,
    size_t dim
);

/**
 * @brief Get current curiosity state
 *
 * WHAT: Query current epistemic values
 * WHY:  Monitor exploration drive for debugging/analysis
 * HOW:  Copy internal state to output structure
 *
 * @param sys Curiosity system
 * @param state Output state structure
 * @return 0 on success, negative on error
 */
int fep_curiosity_get_state(
    const fep_curiosity_system_t* sys,
    fep_curiosity_state_t* state
);

/**
 * @brief Get curiosity statistics
 *
 * WHAT: Query cumulative statistics
 * WHY:  Monitor learning progress, exploration efficiency
 * HOW:  Copy statistics to output structure
 *
 * @param sys Curiosity system
 * @param stats Output statistics structure
 * @return 0 on success, negative on error
 */
int fep_curiosity_get_stats(
    const fep_curiosity_system_t* sys,
    fep_curiosity_stats_t* stats
);

/* ============================================================================
 * FEP System Integration API
 * ============================================================================ */

/**
 * @brief Connect curiosity system to FEP system
 *
 * WHAT: Establish bidirectional integration
 * WHY:  Curiosity modulates action selection in FEP
 * HOW:  Store FEP pointer, enable bio-async messaging
 *
 * @param curiosity Curiosity system
 * @param fep FEP system
 * @return 0 on success, negative on error
 */
int fep_curiosity_connect(
    fep_curiosity_system_t* curiosity,
    fep_system_t* fep
);

/**
 * @brief Disconnect curiosity from FEP system
 *
 * WHAT: Remove integration
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Clear FEP pointer, disable bio-async
 *
 * @param curiosity Curiosity system
 * @return 0 on success, negative on error
 */
int fep_curiosity_disconnect(fep_curiosity_system_t* curiosity);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable inter-module messaging
 * WHY:  Curiosity signals can influence other cognitive modules
 * HOW:  Register with bio-async as BIO_MODULE_FEP_CURIOSITY
 *
 * @param sys Curiosity system
 * @return 0 on success, negative on error
 */
int fep_curiosity_connect_bio_async(fep_curiosity_system_t* sys);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from messaging
 * WHY:  Clean shutdown
 * HOW:  Deregister module context
 *
 * @param sys Curiosity system
 * @return 0 on success, negative on error
 */
int fep_curiosity_disconnect_bio_async(fep_curiosity_system_t* sys);

/**
 * @brief Check if bio-async is connected
 *
 * @param sys Curiosity system
 * @return true if connected, false otherwise
 */
bool fep_curiosity_is_bio_async_connected(const fep_curiosity_system_t* sys);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert curiosity type to string
 *
 * @param type Curiosity type
 * @return Human-readable string
 */
const char* fep_curiosity_type_to_string(fep_curiosity_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEP_CURIOSITY_H */
