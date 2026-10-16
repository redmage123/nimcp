/**
 * @file nimcp_tom_substrate_bridge.h
 * @brief Theory of Mind substrate integration for metabolic modulation
 *
 * WHAT: Bridges ToM module with neural substrate for ATP-based mentalizing capacity
 * WHY: ToM requires prefrontal and temporal-parietal networks with high metabolic demands
 * HOW: Models ATP depletion effects on perspective-taking, belief tracking, and empathy
 *
 * Biological basis:
 * - Mentalizing (inferring others' mental states) relies on medial prefrontal cortex (mPFC)
 *   and temporo-parietal junction (TPJ), both metabolically expensive
 * - ATP depletion in these regions impairs capacity to model complex mental states
 * - Fatigue accumulation reduces perspective-taking accuracy and belief tracking depth
 * - Metabolic stress limits empathic processing and social cognition
 * - Chronic metabolic dysfunction can lead to ToM deficits (autism, schizophrenia)
 */

#ifndef NIMCP_TOM_SUBSTRATE_BRIDGE_H
#define NIMCP_TOM_SUBSTRATE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/nimcp_theory_of_mind.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"

/* Bio-async module ID for ToM substrate bridge */
#define BIO_MODULE_SUBSTRATE_TOM 0x1206

/* ATP thresholds for ToM processing (mM units) */
#define TOM_SUBSTRATE_ATP_CRITICAL 0.5f    /* Below: severe mentalizing impairment */
#define TOM_SUBSTRATE_ATP_LOW 1.0f         /* Below: reduced perspective-taking */
#define TOM_SUBSTRATE_ATP_NORMAL 2.0f      /* Normal ToM processing */
#define TOM_SUBSTRATE_ATP_OPTIMAL 3.0f     /* Optimal mentalizing capacity */

/* Fatigue thresholds for ToM functions */
#define TOM_SUBSTRATE_FATIGUE_LOW 0.3f     /* Slight perspective-taking reduction */
#define TOM_SUBSTRATE_FATIGUE_MODERATE 0.6f /* Moderate belief tracking impairment */
#define TOM_SUBSTRATE_FATIGUE_HIGH 0.8f    /* Severe mentalizing deficits */

/**
 * Metabolic effects on Theory of Mind processing
 *
 * WHAT: Quantifies substrate impact on mentalizing, perspective-taking, and empathy
 * WHY: ToM functions are metabolically expensive and degrade under resource stress
 * HOW: Computes capacity factors based on ATP, fatigue, and stress levels
 */
typedef struct {
    float mentalizing_capacity;  /* [0-1] Ability to infer others' mental states */
    float perspective_taking;    /* [0-1] Perspective-taking accuracy */
    float belief_tracking;       /* [0-1] Capacity to track others' beliefs */
    float empathy_factor;        /* [0-1] Empathic processing capacity */
    bool is_impaired;            /* True if ToM functions are significantly impaired */
} tom_substrate_effects_t;

/**
 * Configuration for ToM substrate bridge
 *
 * WHAT: Controls which metabolic factors modulate ToM processing
 * WHY: Allows tuning of ToM metabolic sensitivity for different scenarios
 * HOW: Enable/disable specific effects and set sensitivity parameters
 */
typedef struct {
    /* Feature enables */
    bool enable_atp_modulation;        /* ATP affects mentalizing capacity */
    bool enable_fatigue_modulation;    /* Fatigue reduces perspective-taking */
    bool enable_stress_modulation;     /* Stress impairs belief tracking */
    bool enable_empathy_modulation;    /* Metabolic state affects empathy */

    /* Sensitivity parameters [0-1] */
    float atp_sensitivity;             /* How strongly ATP affects mentalizing */
    float fatigue_sensitivity;         /* How strongly fatigue affects perspective-taking */
    float stress_sensitivity;          /* How strongly stress affects belief tracking */
    float empathy_sensitivity;         /* How strongly metabolism affects empathy */

    /* Impairment threshold */
    float impairment_threshold;        /* Below: is_impaired = true (default 0.5) */

    /* Update frequency */
    uint32_t update_interval_ms;       /* How often to recompute effects */
} tom_substrate_config_t;

/**
 * Statistics for ToM substrate bridge
 *
 * WHAT: Tracks ToM metabolic modulation statistics
 * WHY: Monitoring helps diagnose ToM impairments and metabolic issues
 * HOW: Accumulates counts and extrema over bridge lifetime
 */
typedef struct {
    uint64_t total_updates;                 /* Total update calls */
    uint64_t impairment_episodes;           /* Times ToM was impaired */
    uint64_t severe_impairment_episodes;    /* Times mentalizing < 0.3 */

    /* Current values */
    float current_mentalizing;              /* Current mentalizing capacity */
    float current_perspective_taking;       /* Current perspective-taking */
    float current_belief_tracking;          /* Current belief tracking */
    float current_empathy;                  /* Current empathy factor */

    /* Extrema tracking */
    float min_mentalizing;                  /* Lowest mentalizing capacity seen */
    float max_mentalizing;                  /* Highest mentalizing capacity seen */
    float avg_mentalizing;                  /* Running average mentalizing */

    /* ATP correlation */
    float avg_atp_level;                    /* Average ATP during updates */
    float avg_fatigue_level;                /* Average fatigue during updates */
} tom_substrate_stats_t;

/**
 * ToM substrate bridge
 *
 * WHAT: Connects Theory of Mind module to neural substrate
 * WHY: ToM processing depends critically on metabolic state
 * HOW: Monitors substrate metrics and modulates ToM capacities
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    neural_substrate_t* substrate;          /* Neural substrate being monitored */
    theory_of_mind_t tom;                   /* ToM module being modulated */

    tom_substrate_config_t config;          /* Bridge configuration */
    tom_substrate_effects_t effects;        /* Current metabolic effects */
    tom_substrate_stats_t stats;            /* Statistics */

    /* Timestamp tracking */
    uint64_t last_update_time_ms;           /* Last update timestamp */
} tom_substrate_bridge_t;

/**
 * Initialize default ToM substrate configuration
 *
 * WHAT: Sets sensible defaults for ToM metabolic modulation
 * WHY: Provides baseline configuration for typical use cases
 * HOW: Enables all modulations with moderate sensitivities
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int tom_substrate_default_config(tom_substrate_config_t* config);

/**
 * Get default ToM substrate configuration (backward compat)
 *
 * WHAT: Returns a default configuration struct
 * WHY: Old API used return-struct pattern
 * HOW: Creates and returns a config with default values
 *
 * @return Default configuration struct
 */
tom_substrate_config_t tom_substrate_get_default_config(void);

/**
 * Create ToM substrate bridge
 *
 * WHAT: Creates bridge between ToM module and neural substrate
 * WHY: Enables metabolic modulation of mentalizing functions
 * HOW: Allocates bridge, connects modules, initializes state
 *
 * @param config Bridge configuration
 * @param tom Theory of Mind module
 * @param substrate Neural substrate
 * @return Bridge instance or NULL on error
 */
tom_substrate_bridge_t* tom_substrate_bridge_create(
    const tom_substrate_config_t* config,
    theory_of_mind_t tom,
    neural_substrate_t* substrate
);

/**
 * Destroy ToM substrate bridge
 *
 * WHAT: Cleans up ToM substrate bridge
 * WHY: Prevents memory leaks and resource cleanup
 * HOW: Disconnects modules, releases resources, frees bridge
 *
 * @param bridge Bridge to destroy
 */
void tom_substrate_bridge_destroy(tom_substrate_bridge_t* bridge);

/**
 * Connect bridge to bio-async router
 *
 * WHAT: Registers ToM substrate bridge with bio-async messaging
 * WHY: Enables inter-module communication about ToM metabolic state
 * HOW: Registers BIO_MODULE_SUBSTRATE_TOM with router
 *
 * @param bridge Bridge to connect
 * @return 0 on success, negative on error
 */
int tom_substrate_connect_bio_async(tom_substrate_bridge_t* bridge);

/**
 * Disconnect bridge from bio-async router
 *
 * WHAT: Unregisters ToM substrate bridge from bio-async
 * WHY: Clean shutdown of messaging integration
 * HOW: Unregisters module and clears context
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, negative on error
 */
int tom_substrate_disconnect_bio_async(tom_substrate_bridge_t* bridge);

/**
 * Check if bridge is connected to bio-async
 *
 * WHAT: Tests whether bio-async integration is active
 * WHY: Allows conditional messaging behavior
 * HOW: Returns bio_async_enabled flag state
 *
 * @param bridge Bridge to check
 * @return true if connected, false otherwise
 */
bool tom_substrate_is_bio_async_connected(const tom_substrate_bridge_t* bridge);

/**
 * Update ToM substrate effects
 *
 * WHAT: Recomputes metabolic effects on ToM processing
 * WHY: Keeps ToM capacities synchronized with substrate state
 * HOW: Samples substrate metrics, applies modulation formulas, updates effects
 *
 * Biological model:
 * - Mentalizing capacity decreases exponentially with ATP depletion
 * - Perspective-taking accuracy degrades linearly with fatigue
 * - Belief tracking depth limited by metabolic stress
 * - Empathy factor reduced under resource constraints
 *
 * @param bridge Bridge to update
 * @return 0 on success, negative on error
 */
int tom_substrate_update(tom_substrate_bridge_t* bridge);

/**
 * Get current mentalizing capacity
 *
 * WHAT: Returns current ability to infer mental states
 * WHY: ToM module needs to scale mentalizing depth
 * HOW: Returns mentalizing_capacity from latest effects
 *
 * @param bridge Bridge to query
 * @return Mentalizing capacity [0-1] or -1.0 on error
 */
float tom_substrate_get_mentalizing_capacity(const tom_substrate_bridge_t* bridge);

/**
 * Get current perspective-taking accuracy
 *
 * WHAT: Returns current perspective-taking performance
 * WHY: Determines accuracy of modeling others' viewpoints
 * HOW: Returns perspective_taking from latest effects
 *
 * @param bridge Bridge to query
 * @return Perspective-taking [0-1] or -1.0 on error
 */
float tom_substrate_get_perspective_taking(const tom_substrate_bridge_t* bridge);

/**
 * Get current belief tracking capacity
 *
 * WHAT: Returns current ability to track others' beliefs
 * WHY: Limits complexity of belief state tracking
 * HOW: Returns belief_tracking from latest effects
 *
 * @param bridge Bridge to query
 * @return Belief tracking [0-1] or -1.0 on error
 */
float tom_substrate_get_belief_tracking(const tom_substrate_bridge_t* bridge);

/**
 * Get current empathy factor
 *
 * WHAT: Returns current empathic processing capacity
 * WHY: Determines emotional resonance with others
 * HOW: Returns empathy_factor from latest effects
 *
 * @param bridge Bridge to query
 * @return Empathy factor [0-1] or -1.0 on error
 */
float tom_substrate_get_empathy_factor(const tom_substrate_bridge_t* bridge);

/**
 * Get complete substrate effects
 *
 * WHAT: Returns full effects structure
 * WHY: Allows comprehensive ToM metabolic state inspection
 * HOW: Copies current effects to output parameter
 *
 * @param bridge Bridge to query
 * @param effects Output effects structure
 * @return 0 on success, negative on error
 */
int tom_substrate_get_effects(
    const tom_substrate_bridge_t* bridge,
    tom_substrate_effects_t* effects
);

/**
 * Check if ToM is metabolically impaired
 *
 * WHAT: Tests whether ToM functions are significantly degraded
 * WHY: Critical for detecting ToM failure states
 * HOW: Returns is_impaired flag from latest effects
 *
 * @param bridge Bridge to check
 * @return true if impaired, false otherwise
 */
bool tom_substrate_is_impaired(const tom_substrate_bridge_t* bridge);

/**
 * Get bridge statistics
 *
 * WHAT: Returns accumulated ToM substrate statistics
 * WHY: Enables monitoring and diagnostics
 * HOW: Copies stats structure to output parameter
 *
 * @param bridge Bridge to query
 * @param stats Output statistics structure
 * @return 0 on success, negative on error
 */
int tom_substrate_get_stats(
    const tom_substrate_bridge_t* bridge,
    tom_substrate_stats_t* stats
);

/* ============================================================================
 * Backward Compatibility API (theory_of_mind namespace)
 * ============================================================================
 * These functions provide backward compatibility with the older API used by
 * nimcp_theory_of_mind.c. They are aliases/wrappers for the new unified API.
 * ============================================================================ */

/**
 * Update bridge (backward compat wrapper)
 * Alias for tom_substrate_update()
 */
int tom_substrate_bridge_update(tom_substrate_bridge_t* bridge);

/**
 * Apply effects via bio-async (backward compat)
 * Broadcasts ToM substrate effects via bio-async messaging
 */
int tom_substrate_bridge_apply_effects(tom_substrate_bridge_t* bridge);

/**
 * Get effects (backward compat wrapper)
 * Alias for tom_substrate_get_effects()
 */
int tom_substrate_bridge_get_effects(const tom_substrate_bridge_t* bridge,
                                     tom_substrate_effects_t* effects);

/**
 * Register with bio-async router (backward compat)
 * Alias for tom_substrate_connect_bio_async()
 */
int tom_substrate_bridge_register_bio_async(tom_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TOM_SUBSTRATE_BRIDGE_H */
