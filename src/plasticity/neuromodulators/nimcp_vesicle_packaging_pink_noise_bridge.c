/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * vesicle_packaging_pink_noise_create has zero callers in production
 * code. Wrapper around pink_noise_create that is unused. Either wire
 * a consumer or delete before the next major version. Do not extend.
 */

/**
 * @file nimcp_vesicle_packaging_pink_noise_bridge.c
 * @brief Implementation of pink noise bridge for vesicle packaging
 */

#include "plasticity/neuromodulators/nimcp_vesicle_packaging_pink_noise_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(vesicle_packaging_pink_noise_bridge)

/* Security integration */
#define LOG_MODULE "VESICLE_PACKAGING_PINK_NOISE_BRIDGE"


//=============================================================================
// INTERNAL STRUCTURES
//=============================================================================

/**
 * @brief Internal bridge state
 *
 * WHAT: Complete state for vesicle pink noise bridge
 * WHY:  Encapsulate all internal data
 * HOW:  Stores config, generators, state, statistics, pool pointer
 */
struct vesicle_pink_noise_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    // Configuration
    vesicle_pink_noise_config_t config;

    // Pink noise generators (one per target)
    pink_noise_generator_t rrp_generator;
    pink_noise_generator_t pr_generator;
    pink_noise_generator_t quantal_generator;
    pink_noise_generator_t refill_generator;

    // Connected vesicle pool
    vesicle_pool_state_t* pool;

    // Current noise state
    vesicle_noise_state_t state;

    // Statistics
    vesicle_pink_noise_stats_t stats;

    // Control flags
    bool enabled;
    bool connected;

    // Thread safety
};

BRIDGE_DEFINE_SECURITY_SETTERS(vesicle_pink_noise_bridge)

//=============================================================================
// HELPER MACROS
//=============================================================================

#define LOCK(bridge) \
    if ((bridge) && (bridge)->base.mutex) { nimcp_mutex_lock((bridge)->base.mutex); }

#define UNLOCK(bridge) \
    if ((bridge) && (bridge)->base.mutex) { nimcp_mutex_unlock((bridge)->base.mutex); }

#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

/**
 * @brief Get default pink noise bridge configuration
 *
 * WHAT: Provides sensible defaults for vesicle noise modulation
 * WHY:  Easy starting point based on biological measurements
 * HOW:  Initialize config with literature-derived parameters
 */
vesicle_pink_noise_config_t vesicle_pink_noise_get_default_config(void) {
    vesicle_pink_noise_config_t config;
    memset(&config, 0, sizeof(config));

    // Get default pink noise configs for each target
    config.rrp_noise_config = pink_noise_default_config();
    config.pr_noise_config = pink_noise_default_config();
    config.quantal_noise_config = pink_noise_default_config();
    config.refill_noise_config = pink_noise_default_config();

    // Customize amplitude for each target
    config.rrp_noise_config.amplitude = 0.15f;      // 15% RRP variation
    config.pr_noise_config.amplitude = 0.10f;       // 10% Pr variation
    config.quantal_noise_config.amplitude = 0.20f;  // 20% quantal variation
    config.refill_noise_config.amplitude = 0.10f;   // 10% refill variation

    // Use different seeds for each generator (avoid correlation)
    config.rrp_noise_config.seed = 12345;
    config.pr_noise_config.seed = 23456;
    config.quantal_noise_config.seed = 34567;
    config.refill_noise_config.seed = 45678;

    // Enable all modulation targets by default
    config.modulation_targets = VESICLE_NOISE_ALL;

    // Modulation strengths (how much noise affects parameters)
    config.rrp_modulation_strength = 0.15f;      // ±15% RRP variation
    config.pr_modulation_strength = 0.10f;       // ±10% Pr variation
    config.quantal_modulation_strength = 0.20f;  // ±20% quantal variation
    config.refill_modulation_strength = 0.10f;   // ±10% refill variation

    // Clamping ranges (prevent unphysical values)
    config.min_rrp_fraction = 0.3f;     // RRP ≥ 30% of capacity
    config.max_rrp_fraction = 1.0f;     // RRP ≤ 100% of capacity
    config.min_pr = 0.01f;              // Pr ≥ 0.01
    config.max_pr = 0.95f;              // Pr ≤ 0.95
    config.min_quantal_fraction = 0.5f; // Quantal ≥ 50% of base
    config.max_quantal_fraction = 1.5f; // Quantal ≤ 150% of base

    // Update rate (1000 Hz = 1ms resolution)
    config.update_rate_hz = 1000.0f;

    // Thread safety disabled by default
    config.enable_threading = false;

    return config;
}

/**
 * @brief Create vesicle pink noise bridge
 *
 * WHAT: Initializes bridge with pink noise generators
 * WHY:  Enable biologically realistic stochastic vesicle release
 * HOW:  Create generators, allocate state, initialize statistics
 */
vesicle_pink_noise_bridge_t* vesicle_pink_noise_create(const vesicle_pink_noise_config_t* config) {
    // Use defaults if config is NULL
    vesicle_pink_noise_config_t default_config = vesicle_pink_noise_get_default_config();
    const vesicle_pink_noise_config_t* cfg = config ? config : &default_config;

    // Validate configuration
    if (!vesicle_pink_noise_validate_config(cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vesicle_pink_noise_create: invalid configuration");
        return NULL;
    }

    // Allocate bridge structure
    vesicle_pink_noise_bridge_t* bridge = (vesicle_pink_noise_bridge_t*)nimcp_malloc(
        sizeof(vesicle_pink_noise_bridge_t)
    );
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vesicle_pink_noise_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(vesicle_pink_noise_bridge_t));

    // Copy configuration
    bridge->config = *cfg;

    // Create pink noise generators for enabled targets
    if (cfg->modulation_targets & VESICLE_NOISE_RRP_SIZE) {
        bridge->rrp_generator = pink_noise_create(&cfg->rrp_noise_config);
        if (!bridge->rrp_generator) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vesicle_pink_noise_create: failed to create rrp_generator");
            vesicle_pink_noise_destroy(bridge);
            return NULL;
        }
    }

    if (cfg->modulation_targets & VESICLE_NOISE_RELEASE_PROB) {
        bridge->pr_generator = pink_noise_create(&cfg->pr_noise_config);
        if (!bridge->pr_generator) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vesicle_pink_noise_create: failed to create pr_generator");
            vesicle_pink_noise_destroy(bridge);
            return NULL;
        }
    }

    if (cfg->modulation_targets & VESICLE_NOISE_QUANTAL_SIZE) {
        bridge->quantal_generator = pink_noise_create(&cfg->quantal_noise_config);
        if (!bridge->quantal_generator) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vesicle_pink_noise_create: failed to create quantal_generator");
            vesicle_pink_noise_destroy(bridge);
            return NULL;
        }
    }

    if (cfg->modulation_targets & VESICLE_NOISE_REFILL_RATE) {
        bridge->refill_generator = pink_noise_create(&cfg->refill_noise_config);
        if (!bridge->refill_generator) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vesicle_pink_noise_create: failed to create refill_generator");
            vesicle_pink_noise_destroy(bridge);
            return NULL;
        }
    }

    // Initialize state
    memset(&bridge->state, 0, sizeof(vesicle_noise_state_t));

    // Initialize statistics
    memset(&bridge->stats, 0, sizeof(vesicle_pink_noise_stats_t));
    bridge->stats.update_rate_hz = cfg->update_rate_hz;

    // Initialize flags
    bridge->enabled = true;
    bridge->connected = false;
    bridge->pool = NULL;

    // Initialize bridge base (allocates mutex, sets module name) if threading enabled
    if (cfg->enable_threading) {
        if (bridge_base_init(&bridge->base, 0, "vesicle_packaging_pink_noise") != 0) {
            vesicle_pink_noise_destroy(bridge);
            return NULL;
        }
    }

    NIMCP_LOGGING_INFO("Created %s bridge", "vesicle_packaging_pink_noise");
    return bridge;
}

/**
 * @brief Destroy vesicle pink noise bridge
 *
 * WHAT: Cleans up bridge and frees all resources
 * WHY:  Prevent memory leaks
 * HOW:  Destroy generators, free memory, destroy mutex
 */
void vesicle_pink_noise_destroy(vesicle_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "vesicle_packaging_pink_noise");
    }

    // Destroy pink noise generators
    if (bridge->rrp_generator) {
        pink_noise_destroy(bridge->rrp_generator);
    }
    if (bridge->pr_generator) {
        pink_noise_destroy(bridge->pr_generator);
    }
    if (bridge->quantal_generator) {
        pink_noise_destroy(bridge->quantal_generator);
    }
    if (bridge->refill_generator) {
        pink_noise_destroy(bridge->refill_generator);
    }

    // Cleanup bridge base (disconnects bio-async, destroys+frees mutex)
    bridge_base_cleanup(&bridge->base);

    // Free bridge structure
    nimcp_free(bridge);
}

//=============================================================================
// CONNECTION FUNCTIONS
//=============================================================================

/**
 * @brief Connect bridge to vesicle pool
 *
 * WHAT: Associates bridge with vesicle pool for modulation
 * WHY:  Enable noise application to pool parameters
 * HOW:  Store pool pointer, set connected flag
 */
nimcp_error_t vesicle_pink_noise_connect_pool(vesicle_pink_noise_bridge_t* bridge,
                                               vesicle_pool_state_t* pool) {
    BRIDGE_BBB_VALIDATE(bridge, pool, sizeof(*pool));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_connect_pool: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_connect_pool: pool is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    LOCK(bridge);
    bridge->pool = pool;
    bridge->connected = true;
    UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Disconnect bridge from vesicle pool
 *
 * WHAT: Removes association between bridge and pool
 * WHY:  Stop noise modulation
 * HOW:  Clear pool pointer, clear connected flag
 */
nimcp_error_t vesicle_pink_noise_disconnect_pool(vesicle_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_disconnect_pool: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    LOCK(bridge);
    bridge->pool = NULL;
    bridge->connected = false;
    UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Check if bridge is connected to pool
 *
 * WHAT: Tests if bridge has active pool connection
 * WHY:  Validate before applying modulation
 * HOW:  Check connected flag
 */
bool vesicle_pink_noise_is_connected(const vesicle_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    return bridge->connected && (bridge->pool != NULL);
}

//=============================================================================
// MODULATION FUNCTIONS
//=============================================================================

/**
 * @brief Update pink noise generators
 *
 * WHAT: Generates new noise samples for all enabled targets
 * WHY:  Advance noise state for next modulation application
 * HOW:  Call pink_noise_generate_sample() for each generator
 */
nimcp_error_t vesicle_pink_noise_update(vesicle_pink_noise_bridge_t* bridge,
                                         uint64_t current_time_us) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_update: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->enabled) {
        return NIMCP_SUCCESS;  // Not an error, just disabled
    }

    LOCK(bridge);

    // Check if update is needed based on update rate
    if (bridge->state.last_update_us > 0) {
        uint64_t dt_us = current_time_us - bridge->state.last_update_us;
        float dt_s = dt_us / 1000000.0f;
        float min_dt_s = 1.0f / bridge->config.update_rate_hz;

        if (dt_s < min_dt_s) {
            UNLOCK(bridge);
            return NIMCP_SUCCESS;  // Too soon to update
        }
    }

    // Generate new noise samples for enabled targets
    if (bridge->config.modulation_targets & VESICLE_NOISE_RRP_SIZE) {
        if (bridge->rrp_generator) {
            pink_noise_generate_sample(bridge->rrp_generator, &bridge->state.rrp_noise);
        }
    }

    if (bridge->config.modulation_targets & VESICLE_NOISE_RELEASE_PROB) {
        if (bridge->pr_generator) {
            pink_noise_generate_sample(bridge->pr_generator, &bridge->state.pr_noise);
        }
    }

    if (bridge->config.modulation_targets & VESICLE_NOISE_QUANTAL_SIZE) {
        if (bridge->quantal_generator) {
            pink_noise_generate_sample(bridge->quantal_generator, &bridge->state.quantal_noise);
        }
    }

    if (bridge->config.modulation_targets & VESICLE_NOISE_REFILL_RATE) {
        if (bridge->refill_generator) {
            pink_noise_generate_sample(bridge->refill_generator, &bridge->state.refill_noise);
        }
    }

    // Update state
    bridge->state.update_count++;
    bridge->state.last_update_us = current_time_us;

    // Update statistics
    bridge->stats.total_updates++;
    bridge->stats.avg_rrp_noise += (bridge->state.rrp_noise - bridge->stats.avg_rrp_noise) / bridge->stats.total_updates;
    bridge->stats.avg_pr_noise += (bridge->state.pr_noise - bridge->stats.avg_pr_noise) / bridge->stats.total_updates;
    bridge->stats.avg_quantal_noise += (bridge->state.quantal_noise - bridge->stats.avg_quantal_noise) / bridge->stats.total_updates;
    bridge->stats.avg_refill_noise += (bridge->state.refill_noise - bridge->stats.avg_refill_noise) / bridge->stats.total_updates;

    UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Apply noise modulation to vesicle pool
 *
 * WHAT: Modulates vesicle pool parameters with current noise values
 * WHY:  Inject biologically realistic stochastic variability
 * HOW:  Apply multiplicative noise, clamp to ranges
 */
nimcp_error_t vesicle_pink_noise_apply_modulation(vesicle_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_apply_modulation: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->enabled) {
        return NIMCP_SUCCESS;  // Not an error, just disabled
    }

    if (!bridge->connected || !bridge->pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "vesicle_pink_noise_apply_modulation: bridge not connected to pool");
        return NIMCP_ERROR_INVALID_STATE;
    }

    LOCK(bridge);

    vesicle_pool_state_t* pool = bridge->pool;

    // Modulate RRP size
    if (bridge->config.modulation_targets & VESICLE_NOISE_RRP_SIZE) {
        float base_rrp = (float)pool->readily_releasable_pool;
        float noise_factor = 1.0f + bridge->config.rrp_modulation_strength * bridge->state.rrp_noise;
        float modulated_rrp = base_rrp * noise_factor;

        // Clamp to capacity limits
        float min_rrp = pool->rrp_capacity * bridge->config.min_rrp_fraction;
        float max_rrp = pool->rrp_capacity * bridge->config.max_rrp_fraction;

        if (modulated_rrp < min_rrp) {
            modulated_rrp = min_rrp;
            bridge->stats.rrp_clamped_low++;
        }
        if (modulated_rrp > max_rrp) {
            modulated_rrp = max_rrp;
            bridge->stats.rrp_clamped_high++;
        }

        pool->readily_releasable_pool = (uint32_t)(modulated_rrp + 0.5f);  // Round to nearest
        bridge->state.rrp_modulated = modulated_rrp;
    }

    // Modulate release probability
    if (bridge->config.modulation_targets & VESICLE_NOISE_RELEASE_PROB) {
        float base_pr = pool->release_probability;
        float noise_factor = 1.0f + bridge->config.pr_modulation_strength * bridge->state.pr_noise;
        float modulated_pr = base_pr * noise_factor;

        // Clamp to valid range
        if (modulated_pr < bridge->config.min_pr) {
            modulated_pr = bridge->config.min_pr;
            bridge->stats.pr_clamped_low++;
        }
        if (modulated_pr > bridge->config.max_pr) {
            modulated_pr = bridge->config.max_pr;
            bridge->stats.pr_clamped_high++;
        }

        pool->facilitated_pr = modulated_pr;
        bridge->state.pr_modulated = modulated_pr;
    }

    // Modulate quantal size
    if (bridge->config.modulation_targets & VESICLE_NOISE_QUANTAL_SIZE) {
        float base_quantal = pool->vesicle_quantal_size;
        float noise_factor = 1.0f + bridge->config.quantal_modulation_strength * bridge->state.quantal_noise;
        float modulated_quantal = base_quantal * noise_factor;

        // Clamp to fraction range
        float min_quantal = base_quantal * bridge->config.min_quantal_fraction;
        float max_quantal = base_quantal * bridge->config.max_quantal_fraction;

        modulated_quantal = CLAMP(modulated_quantal, min_quantal, max_quantal);

        pool->vesicle_quantal_size = modulated_quantal;
        bridge->state.quantal_modulated = modulated_quantal;
    }

    // Modulate refill rate
    if (bridge->config.modulation_targets & VESICLE_NOISE_REFILL_RATE) {
        float base_refill = pool->refill_rate;
        float noise_factor = 1.0f + bridge->config.refill_modulation_strength * bridge->state.refill_noise;
        float modulated_refill = base_refill * noise_factor;

        // Clamp to positive values
        if (modulated_refill < 0.1f) {
            modulated_refill = 0.1f;
        }

        pool->refill_rate = modulated_refill;
        bridge->state.refill_modulated = modulated_refill;
    }

    // Update statistics
    bridge->stats.total_modulations++;
    bridge->stats.avg_rrp_modulated += (bridge->state.rrp_modulated - bridge->stats.avg_rrp_modulated) / bridge->stats.total_modulations;
    bridge->stats.avg_pr_modulated += (bridge->state.pr_modulated - bridge->stats.avg_pr_modulated) / bridge->stats.total_modulations;
    bridge->stats.avg_quantal_modulated += (bridge->state.quantal_modulated - bridge->stats.avg_quantal_modulated) / bridge->stats.total_modulations;
    bridge->stats.avg_refill_modulated += (bridge->state.refill_modulated - bridge->stats.avg_refill_modulated) / bridge->stats.total_modulations;

    UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Update and apply in single call
 *
 * WHAT: Convenience function combining update + apply
 * WHY:  Simplify common usage pattern
 * HOW:  Call update() then apply()
 */
nimcp_error_t vesicle_pink_noise_update_and_apply(vesicle_pink_noise_bridge_t* bridge,
                                                   uint64_t current_time_us) {
    nimcp_error_t err = vesicle_pink_noise_update(bridge, current_time_us);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    return vesicle_pink_noise_apply_modulation(bridge);
}

//=============================================================================
// CONTROL FUNCTIONS
//=============================================================================

/**
 * @brief Enable pink noise modulation
 *
 * WHAT: Enables noise application to vesicle pool
 * WHY:  Turn on stochastic modulation
 * HOW:  Set enabled flag
 */
nimcp_error_t vesicle_pink_noise_enable(vesicle_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_enable: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    LOCK(bridge);
    bridge->enabled = true;
    UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Disable pink noise modulation
 *
 * WHAT: Disables noise application
 * WHY:  Temporarily remove noise without destroying bridge
 * HOW:  Clear enabled flag
 */
nimcp_error_t vesicle_pink_noise_disable(vesicle_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_disable: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    LOCK(bridge);
    bridge->enabled = false;
    UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Check if modulation is enabled
 *
 * WHAT: Tests if noise modulation is active
 * WHY:  Query modulation state
 * HOW:  Return enabled flag
 */
bool vesicle_pink_noise_is_enabled(const vesicle_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    return bridge->enabled;
}

/**
 * @brief Set modulation targets
 *
 * WHAT: Changes which parameters receive noise modulation
 * WHY:  Dynamic control over modulation targets
 * HOW:  Update modulation_targets bitfield
 */
nimcp_error_t vesicle_pink_noise_set_targets(vesicle_pink_noise_bridge_t* bridge,
                                              uint32_t targets) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_set_targets: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Validate targets
    if (targets & ~VESICLE_NOISE_ALL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vesicle_pink_noise_set_targets: invalid targets");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LOCK(bridge);
    bridge->config.modulation_targets = targets;
    UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get current modulation targets
 *
 * WHAT: Retrieves active modulation targets
 * WHY:  Query which parameters are being modulated
 * HOW:  Return modulation_targets bitfield
 */
uint32_t vesicle_pink_noise_get_targets(const vesicle_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        return 0;
    }

    return bridge->config.modulation_targets;
}

//=============================================================================
// QUERY FUNCTIONS
//=============================================================================

/**
 * @brief Get current noise state
 *
 * WHAT: Retrieves current noise values and modulated parameters
 * WHY:  Inspect noise modulation for debugging/analysis
 * HOW:  Copy internal state to output struct
 */
nimcp_error_t vesicle_pink_noise_get_state(const vesicle_pink_noise_bridge_t* bridge,
                                            vesicle_noise_state_t* state) {
    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_get_state: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_get_state: state is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    LOCK((vesicle_pink_noise_bridge_t*)bridge);
    *state = bridge->state;
    UNLOCK((vesicle_pink_noise_bridge_t*)bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieves performance and behavior statistics
 * WHY:  Monitor noise quality and modulation effects
 * HOW:  Copy internal stats to output struct
 */
nimcp_error_t vesicle_pink_noise_get_stats(const vesicle_pink_noise_bridge_t* bridge,
                                            vesicle_pink_noise_stats_t* stats) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_get_stats: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_get_stats: stats is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    LOCK((vesicle_pink_noise_bridge_t*)bridge);
    *stats = bridge->stats;
    UNLOCK((vesicle_pink_noise_bridge_t*)bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Reset statistics
 *
 * WHAT: Resets all statistics counters to zero
 * WHY:  Enable periodic monitoring
 * HOW:  Zero all counters, reset averages
 */
void vesicle_pink_noise_reset_stats(vesicle_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    LOCK(bridge);
    float update_rate = bridge->stats.update_rate_hz;
    memset(&bridge->stats, 0, sizeof(vesicle_pink_noise_stats_t));
    bridge->stats.update_rate_hz = update_rate;
    UNLOCK(bridge);
}

//=============================================================================
// CONFIGURATION FUNCTIONS
//=============================================================================

/**
 * @brief Set modulation strength for specific target
 *
 * WHAT: Changes noise modulation strength for one parameter
 * WHY:  Fine-tune noise characteristics during runtime
 * HOW:  Update strength parameter in config
 */
nimcp_error_t vesicle_pink_noise_set_strength(vesicle_pink_noise_bridge_t* bridge,
                                               vesicle_noise_target_t target,
                                               float strength) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_set_strength: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (strength < 0.0f || strength > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vesicle_pink_noise_set_strength: strength out of range");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LOCK(bridge);

    switch (target) {
        case VESICLE_NOISE_RRP_SIZE:
            bridge->config.rrp_modulation_strength = strength;
            break;
        case VESICLE_NOISE_RELEASE_PROB:
            bridge->config.pr_modulation_strength = strength;
            break;
        case VESICLE_NOISE_QUANTAL_SIZE:
            bridge->config.quantal_modulation_strength = strength;
            break;
        case VESICLE_NOISE_REFILL_RATE:
            bridge->config.refill_modulation_strength = strength;
            break;
        default:
            UNLOCK(bridge);
            return NIMCP_ERROR_INVALID_PARAM;
    }

    UNLOCK(bridge);
    return NIMCP_SUCCESS;
}

/**
 * @brief Get modulation strength for specific target
 *
 * WHAT: Retrieves current modulation strength
 * WHY:  Query noise configuration
 * HOW:  Return strength parameter
 */
nimcp_error_t vesicle_pink_noise_get_strength(const vesicle_pink_noise_bridge_t* bridge,
                                               vesicle_noise_target_t target,
                                               float* strength) {
    BRIDGE_BBB_VALIDATE(bridge, strength, sizeof(*strength));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_get_strength: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!strength) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_get_strength: strength is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    switch (target) {
        case VESICLE_NOISE_RRP_SIZE:
            *strength = bridge->config.rrp_modulation_strength;
            break;
        case VESICLE_NOISE_RELEASE_PROB:
            *strength = bridge->config.pr_modulation_strength;
            break;
        case VESICLE_NOISE_QUANTAL_SIZE:
            *strength = bridge->config.quantal_modulation_strength;
            break;
        case VESICLE_NOISE_REFILL_RATE:
            *strength = bridge->config.refill_modulation_strength;
            break;
        default:
            return NIMCP_ERROR_INVALID_PARAM;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Reset pink noise generators
 *
 * WHAT: Resets all noise generators to initial state
 * WHY:  Restart noise sequences for reproducibility
 * HOW:  Call pink_noise_reset() for each generator
 */
nimcp_error_t vesicle_pink_noise_reset_generators(vesicle_pink_noise_bridge_t* bridge,
                                                   uint32_t new_seed) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_reset_generators: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    LOCK(bridge);

    if (bridge->rrp_generator) {
        pink_noise_reset(bridge->rrp_generator, new_seed);
    }
    if (bridge->pr_generator) {
        pink_noise_reset(bridge->pr_generator, new_seed ? new_seed + 1 : 0);
    }
    if (bridge->quantal_generator) {
        pink_noise_reset(bridge->quantal_generator, new_seed ? new_seed + 2 : 0);
    }
    if (bridge->refill_generator) {
        pink_noise_reset(bridge->refill_generator, new_seed ? new_seed + 3 : 0);
    }

    // Reset state
    memset(&bridge->state, 0, sizeof(vesicle_noise_state_t));

    UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

//=============================================================================
// VALIDATION FUNCTIONS
//=============================================================================

/**
 * @brief Validate pink noise bridge configuration
 *
 * WHAT: Checks if configuration parameters are valid
 * WHY:  Prevent invalid bridge creation
 * HOW:  Range checks on all parameters
 */
bool vesicle_pink_noise_validate_config(const vesicle_pink_noise_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pink_noise_validate_config: config is NULL");
        return false;
    }

    // Validate modulation strengths
    if (config->rrp_modulation_strength < 0.0f || config->rrp_modulation_strength > 1.0f) {
        return false;
    }
    if (config->pr_modulation_strength < 0.0f || config->pr_modulation_strength > 1.0f) {
        return false;
    }
    if (config->quantal_modulation_strength < 0.0f || config->quantal_modulation_strength > 1.0f) {
        return false;
    }
    if (config->refill_modulation_strength < 0.0f || config->refill_modulation_strength > 1.0f) {
        return false;
    }

    // Validate fraction ranges
    if (config->min_rrp_fraction < 0.0f || config->min_rrp_fraction > 1.0f) {
        return false;
    }
    if (config->max_rrp_fraction < 0.0f || config->max_rrp_fraction > 1.0f) {
        return false;
    }
    if (config->min_rrp_fraction >= config->max_rrp_fraction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "vesicle_pink_noise_validate_config: capacity exceeded");
        return false;
    }

    // Validate Pr ranges
    if (config->min_pr < 0.0f || config->min_pr > 1.0f) {
        return false;
    }
    if (config->max_pr < 0.0f || config->max_pr > 1.0f) {
        return false;
    }
    if (config->min_pr >= config->max_pr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "vesicle_pink_noise_validate_config: capacity exceeded");
        return false;
    }

    // Validate quantal fraction ranges
    if (config->min_quantal_fraction <= 0.0f) {
        return false;
    }
    if (config->max_quantal_fraction <= 0.0f) {
        return false;
    }
    if (config->min_quantal_fraction >= config->max_quantal_fraction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "vesicle_pink_noise_validate_config: capacity exceeded");
        return false;
    }

    // Validate update rate
    if (config->update_rate_hz <= 0.0f) {
        return false;
    }

    // Validate modulation targets
    if (config->modulation_targets & ~VESICLE_NOISE_ALL) {
        return false;
    }

    // Validate pink noise configs for enabled targets
    if (config->modulation_targets & VESICLE_NOISE_RRP_SIZE) {
        if (!pink_noise_validate_config(&config->rrp_noise_config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vesicle_pink_noise_validate_config: pink_noise_validate_config is NULL");
            return false;
        }
    }

    if (config->modulation_targets & VESICLE_NOISE_RELEASE_PROB) {
        if (!pink_noise_validate_config(&config->pr_noise_config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vesicle_pink_noise_validate_config: pink_noise_validate_config is NULL");
            return false;
        }
    }

    if (config->modulation_targets & VESICLE_NOISE_QUANTAL_SIZE) {
        if (!pink_noise_validate_config(&config->quantal_noise_config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vesicle_pink_noise_validate_config: pink_noise_validate_config is NULL");
            return false;
        }
    }

    if (config->modulation_targets & VESICLE_NOISE_REFILL_RATE) {
        if (!pink_noise_validate_config(&config->refill_noise_config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vesicle_pink_noise_validate_config: pink_noise_validate_config is NULL");
            return false;
        }
    }

    return true;
}
