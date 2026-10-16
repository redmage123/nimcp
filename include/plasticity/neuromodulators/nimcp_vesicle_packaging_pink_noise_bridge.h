/**
 * @file nimcp_vesicle_packaging_pink_noise_bridge.h
 * @brief Pink Noise Bridge for Synaptic Vesicle Packaging
 *
 * WHAT: Integrates 1/f pink noise with vesicle packaging to model quantal variability
 * WHY:  Biological synaptic release exhibits stochastic variability with 1/f statistics
 *       - Vesicle count fluctuations follow power-law distributions
 *       - Release probability varies with pink noise temporal structure
 *       - Quantal variability is critical for synaptic reliability and learning
 * HOW:  Apply pink noise modulation to RRP size, Pr, and quantal content
 *
 * BIOLOGICAL CONTEXT:
 * Synaptic transmission is inherently noisy due to:
 * - Stochastic vesicle docking (RRP fluctuations)
 * - Variable calcium channel opening (Pr fluctuations)
 * - Quantal content variability (vesicle size variations)
 *
 * These fluctuations exhibit 1/f noise characteristics (Faisal et al., 2008):
 * - Enhances signal detection through stochastic resonance
 * - Enables multi-timescale plasticity (fast + slow components)
 * - Improves information transmission in neural networks
 *
 * INTEGRATION POINTS:
 * ┌─────────────────────────────────────────────────────────────────┐
 * │              Vesicle Packaging Pink Noise Bridge                │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  Pink Noise Generator  ──┐                                     │
 * │  (1/f spectrum)          │                                     │
 * │                          ├──► RRP Size Modulation              │
 * │  Vesicle Pool State ─────┘    Release Probability Modulation   │
 * │  (RRP, Pr, quantal)           Quantal Content Modulation       │
 * │                                      │                         │
 * │                                      ▼                         │
 * │                            Noisy Vesicle Release               │
 * │                           (biologically realistic)             │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * MODULATION TARGETS:
 * 1. RRP Size: Fluctuates around mean (±10-20%)
 * 2. Release Probability: Multiplicative noise (±5-15%)
 * 3. Quantal Content: Vesicle size variations (±10-30%)
 *
 * PERFORMANCE:
 * - O(1) per update (streaming pink noise)
 * - Minimal overhead (~5% CPU increase)
 * - Real-time suitable for large networks
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 * @version 1.0.0
 */

#ifndef NIMCP_VESICLE_PACKAGING_PINK_NOISE_BRIDGE_H
#define NIMCP_VESICLE_PACKAGING_PINK_NOISE_BRIDGE_H

#include "plasticity/neuromodulators/nimcp_vesicle_packaging.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "plasticity/noise/nimcp_pink_noise.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

typedef struct vesicle_pink_noise_bridge vesicle_pink_noise_bridge_t;

//=============================================================================
// CONFIGURATION STRUCTURES
//=============================================================================

/**
 * @brief Modulation targets for pink noise
 *
 * WHAT: Flags controlling which vesicle parameters receive noise
 * WHY:  Flexible control over noise application
 * HOW:  Bitfield enables/disables each modulation type
 */
typedef enum {
    VESICLE_NOISE_NONE           = 0x00,  /**< No modulation */
    VESICLE_NOISE_RRP_SIZE       = 0x01,  /**< Modulate RRP size */
    VESICLE_NOISE_RELEASE_PROB   = 0x02,  /**< Modulate release probability */
    VESICLE_NOISE_QUANTAL_SIZE   = 0x04,  /**< Modulate quantal content */
    VESICLE_NOISE_REFILL_RATE    = 0x08,  /**< Modulate refill rate */
    VESICLE_NOISE_ALL            = 0x0F   /**< All modulations enabled */
} vesicle_noise_target_t;

/**
 * @brief Configuration for vesicle pink noise bridge
 *
 * WHAT: Parameters controlling pink noise integration with vesicle pools
 * WHY:  Customize noise characteristics for different synapse types
 * HOW:  Separate noise configs for each modulation target
 */
typedef struct {
    // Noise generation parameters
    pink_noise_config_t rrp_noise_config;      /**< Pink noise for RRP size */
    pink_noise_config_t pr_noise_config;       /**< Pink noise for release probability */
    pink_noise_config_t quantal_noise_config;  /**< Pink noise for quantal size */
    pink_noise_config_t refill_noise_config;   /**< Pink noise for refill rate */

    // Modulation targets
    uint32_t modulation_targets;  /**< Bitfield of vesicle_noise_target_t */

    // Modulation strength (multiplicative factors)
    float rrp_modulation_strength;      /**< RRP size variation (0.0-0.5, default: 0.15) */
    float pr_modulation_strength;       /**< Pr variation (0.0-0.3, default: 0.10) */
    float quantal_modulation_strength;  /**< Quantal size variation (0.0-0.5, default: 0.20) */
    float refill_modulation_strength;   /**< Refill rate variation (0.0-0.3, default: 0.10) */

    // Clamping ranges (prevent unphysical values)
    float min_rrp_fraction;      /**< Minimum RRP as fraction of capacity (default: 0.3) */
    float max_rrp_fraction;      /**< Maximum RRP as fraction of capacity (default: 1.0) */
    float min_pr;                /**< Minimum release probability (default: 0.01) */
    float max_pr;                /**< Maximum release probability (default: 0.95) */
    float min_quantal_fraction;  /**< Minimum quantal size fraction (default: 0.5) */
    float max_quantal_fraction;  /**< Maximum quantal size fraction (default: 1.5) */

    // Update rate
    float update_rate_hz;  /**< How often to update noise (default: 1000.0 Hz = 1ms) */

    // Thread safety
    bool enable_threading;  /**< Enable mutex protection (default: false) */

} vesicle_pink_noise_config_t;

/**
 * @brief Current noise modulation state
 *
 * WHAT: Tracks current noise values applied to vesicle parameters
 * WHY:  Enables inspection and debugging of noise modulation
 * HOW:  Updated each step, accessible via getter functions
 */
typedef struct {
    float rrp_noise;       /**< Current RRP size noise value */
    float pr_noise;        /**< Current Pr noise value */
    float quantal_noise;   /**< Current quantal size noise value */
    float refill_noise;    /**< Current refill rate noise value */

    float rrp_modulated;      /**< Modulated RRP size */
    float pr_modulated;       /**< Modulated release probability */
    float quantal_modulated;  /**< Modulated quantal size */
    float refill_modulated;   /**< Modulated refill rate */

    uint64_t update_count;    /**< Number of updates performed */
    uint64_t last_update_us;  /**< Last update timestamp (microseconds) */
} vesicle_noise_state_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Performance and behavior metrics for noise bridge
 * WHY:  Monitor noise quality and modulation effects
 * HOW:  Accumulated statistics over lifetime
 */
typedef struct {
    uint64_t total_updates;         /**< Total update calls */
    uint64_t total_modulations;     /**< Total modulations applied */

    // Noise statistics
    float avg_rrp_noise;       /**< Average RRP noise value */
    float avg_pr_noise;        /**< Average Pr noise value */
    float avg_quantal_noise;   /**< Average quantal noise value */
    float avg_refill_noise;    /**< Average refill noise value */

    // Modulation statistics
    float avg_rrp_modulated;      /**< Average modulated RRP */
    float avg_pr_modulated;       /**< Average modulated Pr */
    float avg_quantal_modulated;  /**< Average modulated quantal */
    float avg_refill_modulated;   /**< Average modulated refill */

    // Clamping statistics
    uint64_t rrp_clamped_low;   /**< Times RRP clamped to min */
    uint64_t rrp_clamped_high;  /**< Times RRP clamped to max */
    uint64_t pr_clamped_low;    /**< Times Pr clamped to min */
    uint64_t pr_clamped_high;   /**< Times Pr clamped to max */

    float update_rate_hz;  /**< Actual update rate */
} vesicle_pink_noise_stats_t;

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

/**
 * @brief Get default pink noise bridge configuration
 *
 * WHAT: Provides sensible defaults for vesicle noise modulation
 * WHY:  Easy starting point based on biological measurements
 * HOW:  Returns config with literature-derived parameters
 *
 * DEFAULT VALUES:
 * - RRP modulation: 15% (±1-2 vesicles for RRP=10)
 * - Pr modulation: 10% (Pr varies 0.27-0.33 for base Pr=0.3)
 * - Quantal modulation: 20% (vesicle size variation)
 * - Refill modulation: 10% (refill rate variation)
 * - Pink noise: α=1.0 (true 1/f spectrum)
 * - Update rate: 1000 Hz (1ms resolution)
 *
 * @return Default configuration
 */
vesicle_pink_noise_config_t vesicle_pink_noise_get_default_config(void);

/**
 * @brief Create vesicle pink noise bridge
 *
 * WHAT: Initializes bridge with pink noise generators for vesicle modulation
 * WHY:  Enable biologically realistic stochastic vesicle release
 * HOW:  Create noise generators for each target, allocate state
 *
 * ALGORITHM:
 * 1. Validate configuration
 * 2. Allocate bridge structure
 * 3. Create pink noise generators (RRP, Pr, quantal, refill)
 * 4. Initialize state and statistics
 * 5. Initialize mutex if threading enabled
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 *
 * @note Caller must call vesicle_pink_noise_destroy() when done
 */
vesicle_pink_noise_bridge_t* vesicle_pink_noise_create(const vesicle_pink_noise_config_t* config);

/**
 * @brief Destroy vesicle pink noise bridge
 *
 * WHAT: Cleans up bridge and frees all resources
 * WHY:  Prevent memory leaks
 * HOW:  Destroy noise generators, free memory, destroy mutex
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void vesicle_pink_noise_destroy(vesicle_pink_noise_bridge_t* bridge);

//=============================================================================
// CONNECTION FUNCTIONS
//=============================================================================

/**
 * @brief Connect bridge to vesicle pool
 *
 * WHAT: Associates bridge with specific vesicle pool for modulation
 * WHY:  Enable noise application to pool parameters
 * HOW:  Store pool pointer in bridge state
 *
 * @param bridge Bridge handle
 * @param pool Vesicle pool to modulate
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t vesicle_pink_noise_connect_pool(vesicle_pink_noise_bridge_t* bridge,
                                               vesicle_pool_state_t* pool);

/**
 * @brief Disconnect bridge from vesicle pool
 *
 * WHAT: Removes association between bridge and pool
 * WHY:  Stop noise modulation
 * HOW:  Clear pool pointer
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t vesicle_pink_noise_disconnect_pool(vesicle_pink_noise_bridge_t* bridge);

/**
 * @brief Check if bridge is connected to pool
 *
 * WHAT: Tests if bridge has active pool connection
 * WHY:  Validate before applying modulation
 * HOW:  Check pool pointer non-null
 *
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool vesicle_pink_noise_is_connected(const vesicle_pink_noise_bridge_t* bridge);

//=============================================================================
// MODULATION FUNCTIONS
//=============================================================================

/**
 * @brief Update pink noise generators
 *
 * WHAT: Generates new noise samples for all enabled targets
 * WHY:  Advance noise state for next modulation application
 * HOW:  Call pink_noise_generate_sample() for each generator
 *
 * ALGORITHM:
 * 1. Check if update needed (based on update_rate_hz and time)
 * 2. Generate new noise samples for enabled targets
 * 3. Update noise state structure
 * 4. Update statistics
 *
 * @param bridge Bridge handle
 * @param current_time_us Current time (microseconds)
 * @return NIMCP_SUCCESS or error code
 *
 * @note Call this before vesicle_pink_noise_apply_modulation()
 */
nimcp_error_t vesicle_pink_noise_update(vesicle_pink_noise_bridge_t* bridge,
                                         uint64_t current_time_us);

/**
 * @brief Apply noise modulation to vesicle pool
 *
 * WHAT: Modulates vesicle pool parameters with current noise values
 * WHY:  Inject biologically realistic stochastic variability
 * HOW:  Apply multiplicative noise to enabled parameters, clamp to ranges
 *
 * ALGORITHM:
 * 1. Check pool connection
 * 2. For each enabled target:
 *    a. Get base value from pool
 *    b. Apply multiplicative noise: value_new = value_base * (1 + strength * noise)
 *    c. Clamp to biological range
 *    d. Update pool parameter
 * 3. Update statistics
 *
 * MODULATION FORMULAS:
 * - RRP: RRP_new = clamp(RRP_base * (1 + α_rrp * noise_rrp), min_rrp, max_rrp)
 * - Pr:  Pr_new  = clamp(Pr_base * (1 + α_pr * noise_pr), min_pr, max_pr)
 * - Q:   Q_new   = clamp(Q_base * (1 + α_q * noise_q), min_q, max_q)
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS or error code
 *
 * @note Call vesicle_pink_noise_update() first to generate new noise
 */
nimcp_error_t vesicle_pink_noise_apply_modulation(vesicle_pink_noise_bridge_t* bridge);

/**
 * @brief Update and apply in single call
 *
 * WHAT: Convenience function combining update + apply
 * WHY:  Simplify common usage pattern
 * HOW:  Call update() then apply()
 *
 * @param bridge Bridge handle
 * @param current_time_us Current time (microseconds)
 * @return NIMCP_SUCCESS or error code
 *
 * USAGE:
 * @code
 * vesicle_pink_noise_update_and_apply(bridge, current_time);
 * float molecules = vesicle_pool_release(pool, action_potential, current_time);
 * @endcode
 */
nimcp_error_t vesicle_pink_noise_update_and_apply(vesicle_pink_noise_bridge_t* bridge,
                                                   uint64_t current_time_us);

//=============================================================================
// CONTROL FUNCTIONS
//=============================================================================

/**
 * @brief Enable pink noise modulation
 *
 * WHAT: Enables noise application to vesicle pool
 * WHY:  Turn on stochastic modulation
 * HOW:  Set enabled flag
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t vesicle_pink_noise_enable(vesicle_pink_noise_bridge_t* bridge);

/**
 * @brief Disable pink noise modulation
 *
 * WHAT: Disables noise application (generators still run)
 * WHY:  Temporarily remove noise without destroying bridge
 * HOW:  Clear enabled flag
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t vesicle_pink_noise_disable(vesicle_pink_noise_bridge_t* bridge);

/**
 * @brief Check if modulation is enabled
 *
 * WHAT: Tests if noise modulation is active
 * WHY:  Query modulation state
 * HOW:  Return enabled flag
 *
 * @param bridge Bridge handle
 * @return true if enabled, false otherwise
 */
bool vesicle_pink_noise_is_enabled(const vesicle_pink_noise_bridge_t* bridge);

/**
 * @brief Set modulation targets
 *
 * WHAT: Changes which parameters receive noise modulation
 * WHY:  Dynamic control over modulation targets
 * HOW:  Update modulation_targets bitfield
 *
 * @param bridge Bridge handle
 * @param targets Bitfield of vesicle_noise_target_t flags
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t vesicle_pink_noise_set_targets(vesicle_pink_noise_bridge_t* bridge,
                                              uint32_t targets);

/**
 * @brief Get current modulation targets
 *
 * WHAT: Retrieves active modulation targets
 * WHY:  Query which parameters are being modulated
 * HOW:  Return modulation_targets bitfield
 *
 * @param bridge Bridge handle
 * @return Bitfield of active targets, 0 on error
 */
uint32_t vesicle_pink_noise_get_targets(const vesicle_pink_noise_bridge_t* bridge);

//=============================================================================
// QUERY FUNCTIONS
//=============================================================================

/**
 * @brief Get current noise state
 *
 * WHAT: Retrieves current noise values and modulated parameters
 * WHY:  Inspect noise modulation for debugging/analysis
 * HOW:  Copy internal state to output struct
 *
 * @param bridge Bridge handle
 * @param state Output noise state structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t vesicle_pink_noise_get_state(const vesicle_pink_noise_bridge_t* bridge,
                                            vesicle_noise_state_t* state);

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieves performance and behavior statistics
 * WHY:  Monitor noise quality and modulation effects
 * HOW:  Copy internal stats to output struct
 *
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t vesicle_pink_noise_get_stats(const vesicle_pink_noise_bridge_t* bridge,
                                            vesicle_pink_noise_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * WHAT: Resets all statistics counters to zero
 * WHY:  Enable periodic monitoring
 * HOW:  Zero all counters, reset averages
 *
 * @param bridge Bridge handle
 */
void vesicle_pink_noise_reset_stats(vesicle_pink_noise_bridge_t* bridge);

//=============================================================================
// CONFIGURATION FUNCTIONS
//=============================================================================

/**
 * @brief Set modulation strength for specific target
 *
 * WHAT: Changes noise modulation strength for one parameter
 * WHY:  Fine-tune noise characteristics during runtime
 * HOW:  Update strength parameter in config
 *
 * @param bridge Bridge handle
 * @param target Target parameter (single flag, not bitfield)
 * @param strength New modulation strength (0.0-1.0)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t vesicle_pink_noise_set_strength(vesicle_pink_noise_bridge_t* bridge,
                                               vesicle_noise_target_t target,
                                               float strength);

/**
 * @brief Get modulation strength for specific target
 *
 * WHAT: Retrieves current modulation strength
 * WHY:  Query noise configuration
 * HOW:  Return strength parameter
 *
 * @param bridge Bridge handle
 * @param target Target parameter
 * @param strength Output strength value
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t vesicle_pink_noise_get_strength(const vesicle_pink_noise_bridge_t* bridge,
                                               vesicle_noise_target_t target,
                                               float* strength);

/**
 * @brief Reset pink noise generators
 *
 * WHAT: Resets all noise generators to initial state
 * WHY:  Restart noise sequences for reproducibility or new trial
 * HOW:  Call pink_noise_reset() for each generator
 *
 * @param bridge Bridge handle
 * @param new_seed New random seed (0 = use configured seeds)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t vesicle_pink_noise_reset_generators(vesicle_pink_noise_bridge_t* bridge,
                                                   uint32_t new_seed);

//=============================================================================
// VALIDATION FUNCTIONS
//=============================================================================

/**
 * @brief Validate pink noise bridge configuration
 *
 * WHAT: Checks if configuration parameters are valid
 * WHY:  Prevent invalid bridge creation
 * HOW:  Range checks on all parameters
 *
 * VALIDATION RULES:
 * - Modulation strengths ∈ [0, 1]
 * - Min fractions < max fractions
 * - Update rate > 0
 * - Pink noise configs valid
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
bool vesicle_pink_noise_validate_config(const vesicle_pink_noise_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VESICLE_PACKAGING_PINK_NOISE_BRIDGE_H */
