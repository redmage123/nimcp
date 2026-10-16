/**
 * @file nimcp_astrocyte_immune_base.h
 * @brief Abstract Base for Astrocyte-Immune Bridges (OO Inheritance Pattern)
 * @version 2.0.0
 * @date 2025-12-27
 *
 * WHAT: Polymorphic base type for all astrocyte-immune bridge implementations
 * WHY:  Consolidate shared functionality, enable runtime polymorphism
 * HOW:  C struct composition with virtual function table (vtable)
 *
 * DESIGN PATTERN:
 * ================================================================================
 * Uses C-style inheritance via struct composition:
 * - Base struct is first member of derived structs (allows safe casting)
 * - Virtual function table (vtable) enables polymorphic dispatch
 * - Factory functions create appropriate derived types
 *
 * INHERITANCE HIERARCHY:
 * ```
 *   astrocyte_immune_base_t (abstract)
 *       │
 *       ├── astrocyte_plasticity_immune_bridge_t
 *       │   Works with: astrocyte_plasticity_t
 *       │   Focus: Synaptic modulation (D-serine, glutamate uptake)
 *       │
 *       ├── astrocyte_network_immune_bridge_t
 *       │   Works with: astrocyte_network_t
 *       │   Focus: Network-level (BBB, calcium waves, inflammation)
 *       │
 *       └── astrocyte_cell_immune_bridge_t
 *           Works with: nimcp_astrocyte_t
 *           Focus: Single-cell phenotype (A1/A2, scar formation)
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ASTROCYTE_IMMUNE_BASE_H
#define NIMCP_ASTROCYTE_IMMUNE_BASE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/immune/nimcp_brain_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct astrocyte_immune_base astrocyte_immune_base_t;
typedef struct astrocyte_immune_ops astrocyte_immune_ops_t;

/* ============================================================================
 * Bridge Type Enumeration
 * ============================================================================ */

/**
 * @brief Astrocyte-immune bridge type discriminator
 */
typedef enum {
    ASTRO_IMMUNE_TYPE_PLASTICITY = 0,   /**< Works with astrocyte_plasticity_t */
    ASTRO_IMMUNE_TYPE_NETWORK,          /**< Works with astrocyte_network_t */
    ASTRO_IMMUNE_TYPE_CELL,             /**< Works with nimcp_astrocyte_t */
    ASTRO_IMMUNE_TYPE_COUNT
} astrocyte_immune_type_t;

/* ============================================================================
 * Shared State Structures
 * ============================================================================ */

/**
 * @brief Astrocyte reactive phenotype
 *
 * NOTE: This is an alias to maintain compatibility with the existing
 * astrocyte_reactive_state_t from the plasticity module. We add
 * ASTROCYTE_PHENOTYPE_SCAR_FORMING as a distinct value.
 */
typedef enum {
    ASTROCYTE_PHENOTYPE_RESTING = 0,      /**< Normal homeostatic state */
    ASTROCYTE_PHENOTYPE_A1_REACTIVE,      /**< Neurotoxic (inflammation-induced) */
    ASTROCYTE_PHENOTYPE_A2_REACTIVE,      /**< Neuroprotective (ischemia-induced) */
    ASTROCYTE_PHENOTYPE_SCAR_FORMING      /**< Chronic inflammation -> glial scar */
} astrocyte_phenotype_t;

/**
 * @brief Cytokine effects on astrocytes (shared across all bridge types)
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_effect;               /**< IL-1β reactivity contribution */
    float il6_effect;               /**< IL-6 reactivity contribution */
    float tnf_effect;               /**< TNF-α reactivity contribution */
    float ifn_gamma_effect;         /**< IFN-γ reactivity contribution */

    /* Anti-inflammatory effects */
    float il10_effect;              /**< IL-10 resolution effect */

    /* Aggregate metrics */
    float total_reactivity;         /**< Combined reactivity [0-1] */
    float glutamate_clearance;      /**< Clearance efficiency [0-1] */
    float d_serine_modulation;      /**< D-serine factor [0-2] (1.0 = normal) */

    /* State flags */
    bool is_reactive;               /**< Above reactive threshold */
    bool is_astrogliosis;           /**< Severe reactivity */
} astro_cytokine_state_t;

/**
 * @brief Inflammation state affecting astrocytes
 */
typedef struct {
    brain_inflammation_level_t level;   /**< Current inflammation level */
    float duration_sec;                 /**< How long inflamed */
    bool is_chronic;                    /**< >= chronic threshold */
    float reactive_fraction;            /**< Fraction of astrocytes reactive [0-1] */
    float bbb_permeability;             /**< BBB integrity loss [0-1] */
    bool glial_scar_forming;            /**< Chronic reactivity -> scarring */
} astro_inflammation_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint32_t reactivity_events;
    uint32_t cytokine_releases;
    uint32_t phenotype_transitions;
    uint32_t dysfunction_alerts;
} astro_immune_stats_t;

/* ============================================================================
 * Virtual Function Table (Vtable)
 * ============================================================================ */

/**
 * @brief Virtual operations for astrocyte-immune bridges
 *
 * Each derived type implements these functions for polymorphic behavior.
 */
struct astrocyte_immune_ops {
    /**
     * @brief Destroy the bridge (type-specific cleanup)
     */
    void (*destroy)(astrocyte_immune_base_t* bridge);

    /**
     * @brief Update bridge state (bidirectional coupling)
     * @param bridge The bridge instance
     * @param delta_ms Time since last update
     * @return 0 on success
     */
    int (*update)(astrocyte_immune_base_t* bridge, uint64_t delta_ms);

    /**
     * @brief Apply cytokine effects to astrocytes
     * @return 0 on success
     */
    int (*apply_cytokine_effects)(astrocyte_immune_base_t* bridge);

    /**
     * @brief Apply inflammation effects
     * @return 0 on success
     */
    int (*apply_inflammation_effects)(astrocyte_immune_base_t* bridge);

    /**
     * @brief Compute current reactivity factor
     * @return Reactivity [0-1]
     */
    float (*compute_reactivity)(const astrocyte_immune_base_t* bridge);

    /**
     * @brief Compute glutamate clearance efficiency
     * @return Clearance [0-1]
     */
    float (*compute_glutamate_clearance)(const astrocyte_immune_base_t* bridge);

    /**
     * @brief Release cytokines from reactive astrocytes
     * @return 0 on success
     */
    int (*release_reactive_cytokines)(astrocyte_immune_base_t* bridge);

    /**
     * @brief Get current phenotype
     */
    astrocyte_phenotype_t (*get_phenotype)(const astrocyte_immune_base_t* bridge);

    /**
     * @brief Type-specific extension point (optional)
     */
    int (*extension)(astrocyte_immune_base_t* bridge, uint32_t op_code, void* data);
};

/* ============================================================================
 * Base Structure
 * ============================================================================ */

/**
 * @brief Abstract base for all astrocyte-immune bridges
 *
 * IMPORTANT: This struct MUST be the first member of all derived types
 * to enable safe casting between base and derived pointers.
 */
struct astrocyte_immune_base {
    /* Infrastructure (inherited from bridge_base_t) */
    bridge_base_t infra;                /**< MUST be first: mutex, bio-async, etc. */

    /* Type discriminator */
    astrocyte_immune_type_t type;       /**< Which derived type this is */

    /* Virtual function table */
    const astrocyte_immune_ops_t* ops;  /**< Polymorphic operations */

    /* Shared state */
    brain_immune_system_t* immune_system;   /**< Connected immune system */
    astro_cytokine_state_t cytokine_state;  /**< Current cytokine effects */
    astro_inflammation_state_t inflammation; /**< Current inflammation state */
    astro_immune_stats_t stats;             /**< Statistics */

    /* Timing */
    uint64_t last_update_us;            /**< Last update timestamp */
    float chronic_accumulator;          /**< Chronic inflammation accumulator */

    /* Feature flags (common) */
    bool enable_cytokine_effects;
    bool enable_inflammation_effects;
    bool enable_reactive_cytokines;
};

/* ============================================================================
 * Polymorphic API (operates on base pointer)
 * ============================================================================ */

/**
 * @brief Destroy any astrocyte-immune bridge
 *
 * WHAT: Type-safe destruction via virtual dispatch
 * WHY:  Caller doesn't need to know concrete type
 * HOW:  Calls derived type's destroy function
 */
void astro_immune_destroy(astrocyte_immune_base_t* bridge);

/**
 * @brief Update bridge state
 */
int astro_immune_update(astrocyte_immune_base_t* bridge, uint64_t delta_ms);

/**
 * @brief Apply cytokine effects
 */
int astro_immune_apply_cytokines(astrocyte_immune_base_t* bridge);

/**
 * @brief Apply inflammation effects
 */
int astro_immune_apply_inflammation(astrocyte_immune_base_t* bridge);

/**
 * @brief Get reactivity factor
 */
float astro_immune_get_reactivity(const astrocyte_immune_base_t* bridge);

/**
 * @brief Get glutamate clearance efficiency
 */
float astro_immune_get_glutamate_clearance(const astrocyte_immune_base_t* bridge);

/**
 * @brief Get current phenotype
 */
astrocyte_phenotype_t astro_immune_get_phenotype(const astrocyte_immune_base_t* bridge);

/**
 * @brief Check if experiencing astrogliosis
 */
bool astro_immune_has_astrogliosis(const astrocyte_immune_base_t* bridge);

/**
 * @brief Get bridge type
 */
astrocyte_immune_type_t astro_immune_get_type(const astrocyte_immune_base_t* bridge);

/**
 * @brief Get cytokine state (read-only)
 */
int astro_immune_get_cytokine_state(
    const astrocyte_immune_base_t* bridge,
    astro_cytokine_state_t* out_state
);

/**
 * @brief Get inflammation state (read-only)
 */
int astro_immune_get_inflammation_state(
    const astrocyte_immune_base_t* bridge,
    astro_inflammation_state_t* out_state
);

/**
 * @brief Get statistics
 */
int astro_immune_get_stats(
    const astrocyte_immune_base_t* bridge,
    astro_immune_stats_t* out_stats
);

/* ============================================================================
 * Bio-Async Integration (common implementation)
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 */
int astro_immune_connect_bio_async(astrocyte_immune_base_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int astro_immune_disconnect_bio_async(astrocyte_immune_base_t* bridge);

/**
 * @brief Check if bio-async connected
 */
bool astro_immune_is_bio_async_connected(const astrocyte_immune_base_t* bridge);

/* ============================================================================
 * Base Initialization Helper (for derived types)
 * ============================================================================ */

/**
 * @brief Initialize base portion of derived bridge
 *
 * Called by derived type constructors to set up common state.
 *
 * @param base Pointer to base portion of derived struct
 * @param type The derived type discriminator
 * @param ops Virtual function table for this type
 * @param immune_system Connected immune system
 * @return 0 on success
 */
int astro_immune_base_init(
    astrocyte_immune_base_t* base,
    astrocyte_immune_type_t type,
    const astrocyte_immune_ops_t* ops,
    brain_immune_system_t* immune_system
);

/**
 * @brief Cleanup base portion of derived bridge
 *
 * Called by derived type destructors.
 */
void astro_immune_base_cleanup(astrocyte_immune_base_t* base);

/* ============================================================================
 * Phenotype String Conversion
 * ============================================================================ */

/**
 * @brief Convert phenotype enum to string
 */
const char* astrocyte_phenotype_to_string(astrocyte_phenotype_t phenotype);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ASTROCYTE_IMMUNE_BASE_H */
