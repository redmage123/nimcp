/**
 * @file nimcp_astrocytes_immune_bridge.h
 * @brief Single-Cell Astrocyte-Immune Bridge (A1/A2 Phenotypes)
 * @version 2.0.0
 * @date 2025-12-27
 *
 * WHAT: Bridge for individual astrocyte phenotype switching
 * WHY:  Single-cell level A1/A2 transitions during inflammation
 * HOW:  Works with nimcp_astrocyte_t (individual cells)
 *
 * NOTE: Uses "astro_cell_*" prefix to avoid conflicts with the
 * polymorphic base class (astro_immune_*) in nimcp_astrocyte_immune_base.h
 *
 * BIOLOGICAL BASIS:
 * - A1 astrocytes (neurotoxic): Induced by IL-1alpha, TNF, C1q from microglia
 * - A2 astrocytes (neuroprotective): Induced by ischemia, release neurotrophins
 * - Chronic inflammation leads to glial scar formation
 * - Astrocyte-microglia crosstalk regulates inflammation resolution
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ASTROCYTES_IMMUNE_BRIDGE_H
#define NIMCP_ASTROCYTES_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ASTRO_IMMUNE_MODULE_NAME "astro_cell_immune_bridge"

typedef struct astro_immune_bridge astro_immune_bridge_t;
typedef struct nimcp_astrocyte nimcp_astrocyte_t;

/**
 * @brief Astrocyte reactivity phenotype (single-cell)
 */
typedef enum {
    ASTRO_QUIESCENT = 0,    /**< Normal resting state */
    ASTRO_A1_REACTIVE,      /**< Neurotoxic (inflammation-induced) */
    ASTRO_A2_REACTIVE,      /**< Neuroprotective (ischemia-induced) */
    ASTRO_SCAR_FORMING      /**< Chronic inflammation -> glial scar */
} astrocyte_reactivity_t;

typedef struct {
    float il1_a1_induction;           /**< IL-1 induces A1 */
    float tnf_a1_induction;           /**< TNF induces A1 */
    float il6_reactivity_gain;        /**< IL-6 amplifies reactivity */
    float il10_a2_promotion;          /**< IL-10 promotes A2 */
    float scar_formation_threshold;   /**< When scar forms */
    float glutamate_clearance_base;   /**< Base clearance rate */
    bool enable_bio_async;
    uint32_t inbox_capacity;
} astro_immune_config_t;

typedef struct {
    float il1_effect;
    float tnf_effect;
    float il6_effect;
    float il10_effect;
    float a1_drive;
    float a2_drive;
} astro_cytokine_effects_t;

typedef struct {
    uint64_t reactivity_changes;
    uint64_t a1_activations;
    uint64_t a2_activations;
    uint64_t scar_formations;
    float glutamate_clearance_rate;
    float gliotransmitter_release;
} astro_immune_stats_t;

struct astro_immune_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    astro_immune_config_t config;
    nimcp_astrocyte_t* astrocyte;
    brain_immune_system_t* immune_system;
    astrocyte_reactivity_t reactivity_state;
    astro_cytokine_effects_t cytokine_effects;
    float glutamate_clearance_rate;
    float gliotransmitter_release;
    float scar_formation_progress;
    float reactivity_level;
    astro_immune_stats_t stats;
    bool initialized;
};

/* ============================================================================
 * Lifecycle API (astro_cell_* prefix)
 * ============================================================================ */

int astro_cell_default_config(astro_immune_config_t* config);

astro_immune_bridge_t* astro_cell_create(
    const astro_immune_config_t* config,
    nimcp_astrocyte_t* astrocyte,
    brain_immune_system_t* immune_system);

void astro_cell_destroy(astro_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int astro_cell_connect_bio_async(astro_immune_bridge_t* bridge);
int astro_cell_disconnect_bio_async(astro_immune_bridge_t* bridge);
bool astro_cell_is_bio_async_connected(const astro_immune_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

int astro_cell_update_cytokine_effects(astro_immune_bridge_t* bridge);
int astro_cell_update_reactivity(astro_immune_bridge_t* bridge, float dt_ms);
int astro_cell_update(astro_immune_bridge_t* bridge, float dt_ms);

/* ============================================================================
 * Query API
 * ============================================================================ */

astrocyte_reactivity_t astro_cell_get_reactivity(const astro_immune_bridge_t* bridge);
float astro_cell_get_glutamate_clearance(const astro_immune_bridge_t* bridge);
int astro_cell_get_stats(const astro_immune_bridge_t* bridge, astro_immune_stats_t* stats);
void astro_cell_reset_stats(astro_immune_bridge_t* bridge);

/* ============================================================================
 * Utility
 * ============================================================================ */

const char* astrocyte_reactivity_to_string(astrocyte_reactivity_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ASTROCYTES_IMMUNE_BRIDGE_H */
