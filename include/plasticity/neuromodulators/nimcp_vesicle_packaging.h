/**
 * @file nimcp_vesicle_packaging.h
 * @brief Synaptic vesicle packaging and release dynamics
 *
 * BIOLOGICAL CONTEXT:
 * Neurotransmitters are packaged in vesicles and released in quantal packets:
 * - Vesicle capacity: ~3000-10000 molecules per vesicle
 * - Release probability: 0.1-0.5 per action potential
 * - Refill time: 5-30 seconds
 * - Three vesicle pools: Readily Releasable Pool (RRP), Recycling Pool, Reserve Pool
 *
 * SHORT-TERM PLASTICITY:
 * - Facilitation: Increased Pr after recent activity (residual calcium)
 * - Depression: RRP depletion during high-frequency stimulation
 *
 * PHASE: C2.3 Enhancement #3 (Synaptic Vesicle Packaging)
 * DEPENDENCIES: nimcp_phasic_tonic.h, nimcp_receptor_subtypes.h
 * INTEGRATION: nimcp_neuromodulators.c
 *
 * @author Claude Code
 * @date 2025-11-13
 */

#ifndef NIMCP_VESICLE_PACKAGING_H
#define NIMCP_VESICLE_PACKAGING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONSTANTS - Biological Parameters
//=============================================================================

/** Default readily releasable pool size (number of vesicles) */
#define VESICLE_DEFAULT_RRP_SIZE 10

/** Default recycling pool size */
#define VESICLE_DEFAULT_RECYCLING_SIZE 100

/** Default reserve pool size */
#define VESICLE_DEFAULT_RESERVE_SIZE 1000

/** Default release probability [0-1] */
#define VESICLE_DEFAULT_RELEASE_PROBABILITY 0.3f

/** Molecules per vesicle (quantal size) */
#define VESICLE_DEFAULT_QUANTAL_SIZE 5000.0f

/** Refill rate (vesicles per second from recycling to RRP) */
#define VESICLE_DEFAULT_REFILL_RATE 2.0f

/** Mobilization rate (vesicles per second from reserve to recycling) */
#define VESICLE_DEFAULT_MOBILIZATION_RATE 0.5f

/** Calcium decay time constant for facilitation (ms) */
#define VESICLE_CALCIUM_DECAY_TAU 100.0f

/** Facilitation gain (how much residual Ca²⁺ increases Pr) */
#define VESICLE_FACILITATION_GAIN 2.0f

/** Depletion threshold (RRP below this = depleted state) */
#define VESICLE_DEPLETION_THRESHOLD 3

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @brief Synaptic vesicle pool state
 *
 * BIOLOGICAL MODEL:
 * Three-pool model (Rizzoli & Betz, 2005):
 * - Readily Releasable Pool (RRP): Docked vesicles ready to release
 * - Recycling Pool: Vesicles undergoing endocytosis/refilling
 * - Reserve Pool: Long-term storage, mobilized during sustained activity
 *
 * DYNAMICS:
 * Reserve → Recycling → RRP → Release → Recycling
 *
 * WHY: Models short-term synaptic depression (STD) and facilitation (STF)
 */
typedef struct {
    // Vesicle pools
    uint32_t readily_releasable_pool;  /**< RRP: immediately available */
    uint32_t recycling_pool;           /**< Vesicles being refilled */
    uint32_t reserve_pool;             /**< Long-term storage */

    // Pool capacity limits
    uint32_t rrp_capacity;             /**< Maximum RRP size */
    uint32_t recycling_capacity;       /**< Maximum recycling pool */
    uint32_t reserve_capacity;         /**< Maximum reserve pool */

    // Release parameters
    float release_probability;         /**< Base Pr [0-1] */
    float facilitated_pr;              /**< Current Pr with facilitation */
    float vesicle_quantal_size;        /**< Molecules per vesicle */

    // Refill dynamics
    float refill_rate;                 /**< Vesicles/sec: recycling→RRP */
    float mobilization_rate;           /**< Vesicles/sec: reserve→recycling */
    float refill_accumulator;          /**< Fractional vesicles accumulator for refill */
    float mobilization_accumulator;    /**< Fractional vesicles accumulator for mobilization */

    // Facilitation state (residual calcium)
    float calcium_residual;            /**< Residual Ca²⁺ [0-∞] */
    float ca_decay_tau;                /**< Decay time constant (ms) */
    float facilitation_gain;           /**< Ca²⁺ → Pr scaling */

    // Depletion state
    bool is_depleted;                  /**< RRP exhausted? */
    float depletion_factor;            /**< 1.0 = full, 0.0 = empty */

    // Statistics
    uint64_t total_releases;           /**< Lifetime vesicle releases */
    uint64_t total_depleted_events;    /**< Times RRP hit zero */
    uint64_t total_refills;            /**< Vesicles refilled to RRP */
    float avg_release_per_spike;       /**< Average vesicles per AP */

    // Timing
    uint64_t last_release_time_us;     /**< Last release timestamp (µs) */
    uint64_t last_refill_time_us;      /**< Last refill timestamp */

} vesicle_pool_state_t;

/**
 * @brief Configuration for vesicle pool initialization
 */
typedef struct {
    // Initial pool sizes
    uint32_t initial_rrp;
    uint32_t initial_recycling;
    uint32_t initial_reserve;

    // Capacity limits
    uint32_t rrp_capacity;
    uint32_t recycling_capacity;
    uint32_t reserve_capacity;

    // Release parameters
    float base_release_probability;
    float quantal_size;

    // Kinetics
    float refill_rate;
    float mobilization_rate;

    // Facilitation parameters
    float ca_decay_tau;
    float facilitation_gain;

    // Feature flags
    bool enable_facilitation;          /**< Enable Ca²⁺-dependent facilitation */
    bool enable_depression;            /**< Enable RRP depletion */
    bool enable_reserve_mobilization;  /**< Mobilize reserve during sustained activity */

} vesicle_pool_config_t;

//=============================================================================
// CORE API - Lifecycle
//=============================================================================

/**
 * @brief Initialize vesicle pool with default parameters
 *
 * WHAT: Set up three-pool system with biological defaults
 * WHY:  Start with realistic vesicle dynamics
 * HOW:  Initialize pools, set Pr, zero statistics
 *
 * @param pool Vesicle pool to initialize
 */
void vesicle_pool_init(vesicle_pool_state_t* pool);

/**
 * @brief Initialize vesicle pool with custom configuration
 *
 * @param pool Vesicle pool to initialize
 * @param config Configuration parameters
 */
void vesicle_pool_init_with_config(vesicle_pool_state_t* pool,
                                   const vesicle_pool_config_t* config);

/**
 * @brief Get default vesicle pool configuration
 *
 * @return Default configuration with biological parameters
 */
vesicle_pool_config_t vesicle_pool_get_default_config(void);

/**
 * @brief Reset vesicle pool to initial state
 *
 * WHAT: Restore pools to full capacity
 * WHY:  Reset after pharmacological manipulation or testing
 * HOW:  Restore pool sizes, zero Ca²⁺, reset flags
 *
 * @param pool Vesicle pool to reset
 */
void vesicle_pool_reset(vesicle_pool_state_t* pool);

//=============================================================================
// RELEASE DYNAMICS - Vesicle Fusion
//=============================================================================

/**
 * @brief Release neurotransmitter in response to action potential
 *
 * WHAT: Binomial release of vesicles from RRP
 * WHY:  Models quantal release (del Castillo & Katz, 1954)
 * HOW:  For each RRP vesicle, release with probability Pr
 *
 * ALGORITHM:
 * 1. Check if depleted → return 0
 * 2. For each vesicle in RRP:
 *    - Sample uniform random [0,1]
 *    - If < facilitated_pr → release
 * 3. Decrement RRP by released count
 * 4. Update calcium residual (for facilitation)
 * 5. Check depletion threshold
 * 6. Return total molecules released
 *
 * COMPLEXITY: O(RRP) ≈ O(10) per call
 *
 * @param pool Vesicle pool state
 * @param action_potential True if neuron fired
 * @param current_time_us Current time (microseconds)
 * @return Molecules of neurotransmitter released (0 if no AP or depleted)
 */
float vesicle_pool_release(vesicle_pool_state_t* pool,
                           bool action_potential,
                           uint64_t current_time_us);

/**
 * @brief Update facilitation state based on residual calcium
 *
 * WHAT: Increase Pr due to residual Ca²⁺ from recent activity
 * WHY:  Models short-term facilitation (Zucker & Regehr, 2002)
 * HOW:  Pr = base_pr * (1 + facilitation_gain * Ca_residual)
 *
 * DYNAMICS:
 * - Ca_residual += 1.0 per spike
 * - Ca_residual decays exponentially: τ ≈ 100ms
 * - Pr increases with Ca_residual (typically 2-3x at steady state)
 *
 * @param pool Vesicle pool state
 * @param dt Time step (milliseconds)
 */
void vesicle_pool_update_facilitation(vesicle_pool_state_t* pool, float dt);

//=============================================================================
// REFILL DYNAMICS - Pool Recovery
//=============================================================================

/**
 * @brief Refill RRP from recycling pool
 *
 * WHAT: Move vesicles from recycling pool to RRP
 * WHY:  Recover from depletion after high-frequency stimulation
 * HOW:  Transfer vesicles at constant rate (2/sec default)
 *
 * ALGORITHM:
 * 1. Compute vesicles to transfer: refill_rate * dt
 * 2. Limit by recycling pool availability
 * 3. Limit by RRP capacity
 * 4. Transfer vesicles: recycling → RRP
 * 5. Check if recovered from depletion
 *
 * @param pool Vesicle pool state
 * @param dt Time step (seconds)
 */
void vesicle_pool_refill(vesicle_pool_state_t* pool, float dt);

/**
 * @brief Mobilize reserve pool to recycling pool
 *
 * WHAT: Transfer vesicles from reserve to recycling
 * WHY:  Sustain release during prolonged activity
 * HOW:  Gradual mobilization (0.5/sec default)
 *
 * BIOLOGICAL BASIS:
 * During sustained stimulation (>1 min), reserve pool mobilizes to
 * replenish recycling pool (Rizzoli & Betz, 2005)
 *
 * @param pool Vesicle pool state
 * @param dt Time step (seconds)
 */
void vesicle_pool_mobilize_reserve(vesicle_pool_state_t* pool, float dt);

/**
 * @brief Update all vesicle pool dynamics
 *
 * WHAT: Refill RRP, mobilize reserve, update facilitation
 * WHY:  Single call for complete pool dynamics update
 * HOW:  Call refill, mobilize, and facilitation updates
 *
 * USAGE:
 * Call once per timestep (typically 0.1-1.0 ms):
 * vesicle_pool_update(pool, 0.001f); // 1ms timestep
 *
 * @param pool Vesicle pool state
 * @param dt Time step (seconds)
 */
void vesicle_pool_update(vesicle_pool_state_t* pool, float dt);

//=============================================================================
// STATISTICS & MONITORING
//=============================================================================

/**
 * @brief Get vesicle pool statistics
 *
 * @param pool Vesicle pool state
 * @param rrp_count Output: current RRP size
 * @param recycling_count Output: current recycling pool size
 * @param reserve_count Output: current reserve pool size
 * @param depletion_fraction Output: how depleted (0.0=full, 1.0=empty)
 * @param facilitated_pr Output: current release probability with facilitation
 */
void vesicle_pool_get_stats(const vesicle_pool_state_t* pool,
                            uint32_t* rrp_count,
                            uint32_t* recycling_count,
                            uint32_t* reserve_count,
                            float* depletion_fraction,
                            float* facilitated_pr);

/**
 * @brief Check if vesicle pool is depleted
 *
 * @param pool Vesicle pool state
 * @return True if RRP below threshold
 */
bool vesicle_pool_is_depleted(const vesicle_pool_state_t* pool);

/**
 * @brief Get average vesicles released per action potential
 *
 * @param pool Vesicle pool state
 * @return Average quantal content (typically 1-5 vesicles)
 */
float vesicle_pool_get_avg_release(const vesicle_pool_state_t* pool);

//=============================================================================
// PHARMACOLOGICAL INTERVENTIONS
//=============================================================================

/**
 * @brief Simulate botulinum toxin (blocks vesicle release)
 *
 * WHAT: Reduce release probability to near zero
 * WHY:  Model Botox mechanism (SNAP-25 cleavage)
 * HOW:  Set Pr = 0.01 (99% blockade)
 *
 * @param pool Vesicle pool state
 * @param blockade Blockade strength [0-1] (1.0 = complete)
 */
void vesicle_pool_apply_botulinum(vesicle_pool_state_t* pool, float blockade);

/**
 * @brief Simulate amphetamine (depletes vesicles)
 *
 * WHAT: Rapidly deplete RRP and recycling pools
 * WHY:  Model psychostimulant mechanism (reverse transport)
 * HOW:  Drain RRP to 10%, recycling to 30%
 *
 * @param pool Vesicle pool state
 * @param depletion Depletion factor [0-1] (1.0 = complete)
 */
void vesicle_pool_apply_amphetamine(vesicle_pool_state_t* pool, float depletion);

/**
 * @brief Simulate 4-aminopyridine (increases release)
 *
 * WHAT: Increase release probability
 * WHY:  Model K⁺ channel blocker (prolongs Ca²⁺ influx)
 * HOW:  Increase Pr by 50-100%
 *
 * @param pool Vesicle pool state
 * @param potentiation Potentiation factor (1.5 = 50% increase)
 */
void vesicle_pool_apply_4ap(vesicle_pool_state_t* pool, float potentiation);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_VESICLE_PACKAGING_H
