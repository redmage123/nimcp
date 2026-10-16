//=============================================================================
// nimcp_spinal_cord.h - Spinal Cord / Motor Output Module
//=============================================================================
/**
 * @file nimcp_spinal_cord.h
 * @brief Spinal Cord public API for motor output, CPGs, and reflex arcs
 *
 * WHAT: Spinal cord simulation with motor pools, CPGs, and reflex arcs
 * WHY:  Bridges cortical motor commands to final motor output with
 *       biologically-plausible spinal processing (reflexes, CPGs, gating)
 * HOW:  Central pattern generators for rhythmic output, reflex arcs for
 *       fast responses, gate control for pain/sensory modulation
 *
 * BIOLOGICAL BASIS:
 * - Motor pools: Groups of alpha motor neurons innervating a single muscle
 * - CPGs: Spinal oscillator circuits for locomotion (Brown, 1911)
 * - Reflex arcs: Monosynaptic stretch (Ia), withdrawal, crossed extension
 * - Descending tracts: Corticospinal, rubrospinal, vestibulospinal
 * - Gate control theory: Melzack & Wall (1965) pain modulation
 * - Muscle spindle Ia/II afferents and Golgi tendon organ Ib afferents
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-03-05
 */

#ifndef NIMCP_SPINAL_CORD_H
#define NIMCP_SPINAL_CORD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define SPINAL_CORD_MAGIC       0x5D17A1C0u  /* SP1NALC0-ish in hex */
#define SPINAL_MAX_MOTOR_POOLS  64
#define SPINAL_MAX_CPGS         32
#define SPINAL_MAX_REFLEXES     16

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Reflex arc types
 *
 * STRETCH:            Monosynaptic Ia reflex (knee jerk)
 * WITHDRAWAL:         Polysynaptic nociceptive flexion reflex
 * CROSSED_EXTENSION:  Contralateral extensor activation during withdrawal
 */
typedef enum {
    REFLEX_STRETCH            = 0,
    REFLEX_WITHDRAWAL         = 1,
    REFLEX_CROSSED_EXTENSION  = 2,
    REFLEX_TYPE_COUNT         = 3
} reflex_type_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Spinal cord configuration
 *
 * WHAT: Parameters for spinal cord initialization
 * WHY:  Allow tuning of motor pool size, CPG count, reflex parameters
 */
typedef struct {
    uint32_t num_motor_pools;          /**< Number of motor pools to create */
    uint32_t neurons_per_pool;         /**< Neurons per motor pool */
    uint32_t num_cpgs;                 /**< Number of central pattern generators */
    uint32_t num_reflexes;             /**< Number of reflex arcs */
    float    default_cpg_frequency;    /**< Default CPG oscillation frequency (Hz) */
    float    default_reflex_gain;      /**< Default reflex gain [0.0-1.0] */
} spinal_config_t;

//=============================================================================
// Sub-structures
//=============================================================================

/**
 * @brief Central Pattern Generator
 *
 * BIOLOGICAL: Half-center oscillator model for rhythmic motor output.
 * Reciprocal inhibition between flexor/extensor half-centers produces
 * alternating activation for locomotion, respiration, etc.
 */
typedef struct {
    float frequency;        /**< Oscillation frequency (Hz) */
    float phase;            /**< Current phase (radians) */
    float amplitude;        /**< Output amplitude [0.0-1.0] */
    float flexor_output;    /**< Flexor half-center output */
    float extensor_output;  /**< Extensor half-center output */
    bool  active;           /**< Whether this CPG is running */
} cpg_t;

/**
 * @brief Reflex arc
 *
 * BIOLOGICAL: Hardwired sensory-motor pathway with configurable gain.
 * Latency models conduction delay through interneuron chain.
 */
typedef struct {
    reflex_type_t type;      /**< Reflex type (stretch, withdrawal, crossed) */
    float gain;              /**< Reflex gain multiplier */
    float threshold;         /**< Activation threshold */
    float latency_ms;        /**< Conduction latency (ms) */
    uint32_t input_pool;     /**< Sensory input pool index */
    uint32_t output_pool;    /**< Motor output pool index */
} reflex_arc_t;

/**
 * @brief Motor neuron pool
 *
 * BIOLOGICAL: Group of alpha motor neurons (motor unit) innervating
 * a single muscle. Activation levels map to muscle force via
 * Henneman's size principle (recruitment order).
 */
typedef struct {
    uint32_t num_neurons;    /**< Number of motor neurons in pool */
    float*   activations;    /**< Current activation levels [num_neurons] */
    float*   target_forces;  /**< Target force outputs [num_neurons] */
} motor_pool_t;

//=============================================================================
// Thread layer for mutex type
//=============================================================================

#include "utils/thread/nimcp_thread.h"

//=============================================================================
// Main spinal cord structure
//=============================================================================

/**
 * @brief Spinal cord system
 *
 * WHAT: Complete spinal cord simulation
 * WHY:  Final common pathway for motor output with spinal processing
 * HOW:  Integrates descending commands, CPGs, reflexes, and sensory feedback
 *
 * DESCENDING TRACTS:
 * - Corticospinal: Voluntary fine motor control (lateral tract)
 * - Rubrospinal: Flexor facilitation, postural control
 * - Vestibulospinal: Balance and postural reflexes
 *
 * SENSORY AFFERENTS:
 * - Muscle spindle Ia: Dynamic stretch (velocity)
 * - Muscle spindle II: Static stretch (length)
 * - Golgi tendon organ Ib: Muscle tension
 */
typedef struct spinal_cord {
    uint32_t         magic;                /**< Magic number for validation (0x5D17A1C0) */
    spinal_config_t  config;               /**< Configuration */

    /* Motor pools */
    uint32_t         num_motor_pools;      /**< Number of active motor pools */
    motor_pool_t*    motor_pools;          /**< Motor pool array */

    /* Central pattern generators */
    uint32_t         num_cpgs;             /**< Number of CPGs */
    cpg_t*           cpgs;                 /**< CPG array */

    /* Reflex arcs */
    uint32_t         num_reflexes;         /**< Number of reflex arcs */
    reflex_arc_t*    reflexes;             /**< Reflex arc array */

    /* Sensory afferents (proprioceptive feedback) */
    float*           muscle_spindle_ia;    /**< Ia afferent signals (dynamic stretch) [num_motor_pools] */
    float*           muscle_spindle_ii;    /**< II afferent signals (static stretch) [num_motor_pools] */
    float*           golgi_tendon_ib;      /**< Ib afferent signals (tension) [num_motor_pools] */

    /* Descending tract inputs */
    float*           corticospinal_input;  /**< Corticospinal tract input [num_motor_pools] */
    float*           rubrospinal_input;    /**< Rubrospinal tract input [num_motor_pools] */
    float*           vestibulospinal_input;/**< Vestibulospinal tract input [num_motor_pools] */

    /* Gate control */
    float            gate_control_level;   /**< Gate control modulation [0.0-1.0] */

    /* Timing */
    uint64_t         last_update_us;       /**< Last update timestamp (microseconds) */

    /* Thread safety */
    nimcp_mutex_t*   lock;                 /**< Mutex for thread-safe access */
} spinal_cord_t;

//=============================================================================
// Public API
//=============================================================================

/**
 * @brief Get default spinal cord configuration
 *
 * @return Default configuration with sensible biological parameters
 */
spinal_config_t spinal_default_config(void);

/**
 * @brief Create a spinal cord system
 *
 * WHAT: Allocates and initializes all spinal cord components
 * WHY:  Entry point for spinal cord subsystem creation
 * HOW:  Allocates motor pools, CPGs, reflex arcs, sensory buffers
 *
 * @param config Configuration parameters
 * @return Pointer to created spinal cord, or NULL on failure
 *
 * COMPLEXITY: O(num_motor_pools * neurons_per_pool)
 * THREAD-SAFE: No (call during brain creation)
 */
spinal_cord_t* spinal_create(const spinal_config_t* config);

/**
 * @brief Destroy a spinal cord system
 *
 * @param system Spinal cord to destroy (NULL-safe)
 *
 * COMPLEXITY: O(num_motor_pools)
 */
void spinal_destroy(spinal_cord_t* system);

/**
 * @brief Update spinal cord simulation
 *
 * WHAT: Steps CPGs, processes reflexes, computes motor output
 * WHY:  Continuous spinal processing at simulation rate
 * HOW:  1. Step CPG oscillators, 2. Process reflex arcs,
 *        3. Integrate descending input, 4. Compute motor pool output
 *
 * @param system Spinal cord system
 * @param dt_s   Time step in seconds
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(num_cpgs + num_reflexes + num_motor_pools * neurons_per_pool)
 * THREAD-SAFE: Yes (acquires lock)
 */
int spinal_update(spinal_cord_t* system, float dt_s);

/**
 * @brief Activate a central pattern generator
 *
 * @param system Spinal cord system
 * @param cpg_id CPG index to activate
 * @return 0 on success, -1 on error
 */
int spinal_activate_cpg(spinal_cord_t* system, uint32_t cpg_id);

/**
 * @brief Deactivate a central pattern generator
 *
 * @param system Spinal cord system
 * @param cpg_id CPG index to deactivate
 * @return 0 on success, -1 on error
 */
int spinal_deactivate_cpg(spinal_cord_t* system, uint32_t cpg_id);

/**
 * @brief Set corticospinal tract input
 *
 * WHAT: Provides voluntary motor commands from motor cortex
 * WHY:  Primary descending pathway for fine motor control
 *
 * @param system Spinal cord system
 * @param input  Input array (copied)
 * @param size   Number of elements (must match num_motor_pools)
 * @return 0 on success, -1 on error
 */
int spinal_set_corticospinal_input(spinal_cord_t* system, const float* input, uint32_t size);

/**
 * @brief Get motor output from a specific pool
 *
 * @param system  Spinal cord system
 * @param pool_id Motor pool index
 * @param output  Output buffer (caller-allocated, size >= pool->num_neurons)
 * @param size    Buffer size
 * @return 0 on success, -1 on error
 */
int spinal_get_motor_output(const spinal_cord_t* system, uint32_t pool_id,
                            float* output, uint32_t size);

/**
 * @brief Trigger a reflex arc
 *
 * WHAT: Activates a specific reflex with given stimulus intensity
 * WHY:  Fast spinal-level response bypassing cortical processing
 *
 * @param system    Spinal cord system
 * @param reflex_id Reflex arc index
 * @param stimulus  Stimulus intensity [0.0-1.0]
 * @return 0 on success, -1 on error
 */
int spinal_trigger_reflex(spinal_cord_t* system, uint32_t reflex_id, float stimulus);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SPINAL_CORD_H */
