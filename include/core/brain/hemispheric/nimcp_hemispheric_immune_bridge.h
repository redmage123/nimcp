//=============================================================================
// nimcp_hemispheric_immune_bridge.h - Hemispheric Brain Immune Integration
//=============================================================================
/**
 * @file nimcp_hemispheric_immune_bridge.h
 * @brief Bidirectional integration between hemispheric brain and immune system
 *
 * WHAT: Integration layer connecting hemispheric brain with brain immune system
 * WHY:  Inflammation affects hemispheric processing asymmetrically; lateralization
 *       can shift during immune challenges; callosum function degrades with inflammation
 * HOW:  Cytokine modulation of per-hemisphere learning, callosum bandwidth, and
 *       lateralization plasticity
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → HEMISPHERIC PATHWAYS:
 * ------------------------------
 * 1. Asymmetric Inflammation Effects:
 *    - Left hemisphere: More vulnerable to language center impairment
 *    - Right hemisphere: Emotional processing becomes hyperactive under threat
 *    - IL-1β preferentially impairs hippocampal-dependent (bilateral) memory
 *    - TNF-α affects prefrontal function (both hemispheres, left dominant)
 *
 * 2. Callosum Degradation:
 *    - Chronic inflammation → reduced myelination
 *    - Callosum bandwidth decreases with systemic inflammation
 *    - Latency increases as oligodendrocytes are affected
 *    - Storm-level inflammation → near-complete callosum impairment
 *
 * 3. Lateralization Plasticity:
 *    - Inflammation can shift processing to less-affected hemisphere
 *    - Right hemisphere compensates for left hemisphere language damage
 *    - Plasticity increases during recovery phase (anti-inflammatory)
 *
 * 4. Fever-Induced Learning Suppression:
 *    - High inflammation → reduced learning rate (both hemispheres)
 *    - Left hemisphere (language, logic) more affected
 *    - Right hemisphere maintains emotional learning longer
 *
 * HEMISPHERIC → IMMUNE PATHWAYS:
 * ------------------------------
 * 1. Asymmetric Stress Response:
 *    - Right hemisphere (emotion) modulates HPA axis activation
 *    - Left hemisphere (executive) can suppress stress response
 *    - Hemispheric imbalance → immune dysregulation
 *
 * 2. Split-Brain Immune Effects:
 *    - Callosum disconnect → independent immune modulation per hemisphere
 *    - Each hemisphere may have different stress/threat perception
 *    - Reconnection requires immune coordination
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    HEMISPHERIC-IMMUNE BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐ ║
 * ║   │                   IMMUNE → HEMISPHERIC                               │ ║
 * ║   │                                                                      │ ║
 * ║   │   Inflammation     Left Hemisphere    Right Hemisphere               │ ║
 * ║   │   ────────────     ───────────────    ────────────────               │ ║
 * ║   │   NONE          →  100% LR            100% LR                        │ ║
 * ║   │   LOCAL         →   90% LR             95% LR (resilient)            │ ║
 * ║   │   REGIONAL      →   70% LR             80% LR                        │ ║
 * ║   │   SYSTEMIC      →   40% LR             50% LR                        │ ║
 * ║   │   STORM         →   10% LR             15% LR                        │ ║
 * ║   │                                                                      │ ║
 * ║   │   Callosum Bandwidth Degradation:                                    │ ║
 * ║   │   NONE=100%, LOCAL=90%, REGIONAL=70%, SYSTEMIC=40%, STORM=10%        │ ║
 * ║   │                                                                      │ ║
 * ║   │   Lateralization Plasticity:                                         │ ║
 * ║   │   NONE=100%, LOCAL=120% (enhanced), REGIONAL=80%, SYSTEMIC=50%       │ ║
 * ║   └─────────────────────────────────────────────────────────────────────┘ ║
 * ║                                                                            ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐ ║
 * ║   │                   HEMISPHERIC → IMMUNE                               │ ║
 * ║   │                                                                      │ ║
 * ║   │   - Hemispheric stress response triggers immune activation           │ ║
 * ║   │   - Split-brain mode: independent immune modulation                  │ ║
 * ║   │   - Lateralization imbalance → immune dysregulation signal           │ ║
 * ║   └─────────────────────────────────────────────────────────────────────┘ ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_HEMISPHERIC_IMMUNE_BRIDGE_H
#define NIMCP_HEMISPHERIC_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "async/nimcp_bio_async.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Learning rate factors per inflammation level (left hemisphere - more vulnerable) */
#define HEMI_IMMUNE_LR_LEFT_NONE        1.00f
#define HEMI_IMMUNE_LR_LEFT_LOCAL       0.90f
#define HEMI_IMMUNE_LR_LEFT_REGIONAL    0.70f
#define HEMI_IMMUNE_LR_LEFT_SYSTEMIC    0.40f
#define HEMI_IMMUNE_LR_LEFT_STORM       0.10f

/** Learning rate factors per inflammation level (right hemisphere - more resilient) */
#define HEMI_IMMUNE_LR_RIGHT_NONE       1.00f
#define HEMI_IMMUNE_LR_RIGHT_LOCAL      0.95f
#define HEMI_IMMUNE_LR_RIGHT_REGIONAL   0.80f
#define HEMI_IMMUNE_LR_RIGHT_SYSTEMIC   0.50f
#define HEMI_IMMUNE_LR_RIGHT_STORM      0.15f

/** Callosum bandwidth factors per inflammation level */
#define HEMI_IMMUNE_CALLOSUM_NONE       1.00f
#define HEMI_IMMUNE_CALLOSUM_LOCAL      0.90f
#define HEMI_IMMUNE_CALLOSUM_REGIONAL   0.70f
#define HEMI_IMMUNE_CALLOSUM_SYSTEMIC   0.40f
#define HEMI_IMMUNE_CALLOSUM_STORM      0.10f

/** Lateralization plasticity factors (LOCAL enhances - recovery response) */
#define HEMI_IMMUNE_PLASTICITY_NONE     1.00f
#define HEMI_IMMUNE_PLASTICITY_LOCAL    1.20f   // Enhanced during mild challenge
#define HEMI_IMMUNE_PLASTICITY_REGIONAL 0.80f
#define HEMI_IMMUNE_PLASTICITY_SYSTEMIC 0.50f
#define HEMI_IMMUNE_PLASTICITY_STORM    0.20f

/** Cytokine impact weights */
#define HEMI_IMMUNE_IL1_IMPACT          0.35f   // IL-1β - strong cognitive impact
#define HEMI_IMMUNE_IL6_IMPACT          0.25f   // IL-6 - moderate impact
#define HEMI_IMMUNE_TNF_IMPACT          0.30f   // TNF-α - prefrontal impact
#define HEMI_IMMUNE_IFN_IMPACT          0.10f   // IFN-γ - quarantine signal

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Per-hemisphere immune effects
 */
typedef struct {
    float learning_rate_factor;      /**< LR multiplier (0.0-1.0) */
    float attention_factor;          /**< Attention capacity factor */
    float memory_consolidation;      /**< Memory consolidation factor */
    float executive_function;        /**< Executive function factor */
    bool is_compensating;            /**< Is this hemisphere compensating for other? */
} hemisphere_immune_effects_t;

/**
 * @brief Callosum immune effects
 */
typedef struct {
    float bandwidth_factor;          /**< Bandwidth multiplier (0.0-1.0) */
    float latency_multiplier;        /**< Latency increase (1.0 = normal) */
    float reliability_factor;        /**< Message reliability (0.0-1.0) */
    bool degraded;                   /**< Is callosum significantly impaired? */
} callosum_immune_effects_t;

/**
 * @brief Lateralization immune effects
 */
typedef struct {
    float plasticity_factor;         /**< Plasticity rate multiplier */
    float shift_toward_right;        /**< Shift bias (-1.0 left, +1.0 right) */
    bool emergency_bilateral;        /**< Force bilateral processing? */
} lateralization_immune_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    // Sensitivity coefficients
    float left_vulnerability;        /**< Left hemisphere vulnerability (0.0-1.0) */
    float right_vulnerability;       /**< Right hemisphere vulnerability (0.0-1.0) */
    float callosum_sensitivity;      /**< Callosum inflammation sensitivity */

    // Compensation settings
    bool enable_compensation;        /**< Enable cross-hemisphere compensation */
    float compensation_threshold;    /**< Damage level to trigger compensation */

    // Plasticity settings
    bool enable_immune_plasticity;   /**< Allow inflammation to affect plasticity */
    float plasticity_recovery_rate;  /**< Recovery rate when inflammation clears */

    // Bio-async settings
    bool enable_bio_async;           /**< Enable bio-async messaging */
} hemispheric_immune_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t updates;                /**< Total update calls */
    uint64_t modulations_applied;    /**< Times modulation was applied */
    uint64_t compensation_events;    /**< Times compensation was triggered */
    uint64_t callosum_degradations;  /**< Times callosum was degraded */
    float avg_left_lr_factor;        /**< Average left LR factor */
    float avg_right_lr_factor;       /**< Average right LR factor */
    float avg_callosum_bandwidth;    /**< Average callosum bandwidth */
    float max_inflammation_seen;     /**< Maximum inflammation level seen */
} hemispheric_immune_stats_t;

/**
 * @brief Hemispheric immune bridge structure
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    // Connected systems
    hemispheric_brain_t* brain;              /**< Hemispheric brain */
    brain_immune_system_t* immune_system;    /**< Brain immune system */

    // Configuration
    hemispheric_immune_config_t config;

    // Current effects
    hemisphere_immune_effects_t left_effects;
    hemisphere_immune_effects_t right_effects;
    callosum_immune_effects_t callosum_effects;
    lateralization_immune_effects_t lateralization_effects;

    // Current inflammation level
    brain_inflammation_level_t current_inflammation;
    bool inflammation_override;  // True when set via set_inflammation()
    float cytokine_levels[BRAIN_CYTOKINE_COUNT];

    // Statistics
    hemispheric_immune_stats_t stats;

    // Bio-async
    // Thread safety
    // State
    bool initialized;
} hemispheric_immune_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default bridge configuration
 */
hemispheric_immune_config_t hemispheric_immune_default_config(void);

/**
 * @brief Create hemispheric immune bridge
 *
 * @param config Bridge configuration
 * @param brain Hemispheric brain to connect
 * @param immune Brain immune system to connect
 * @return Bridge instance or NULL on failure
 */
hemispheric_immune_bridge_t* hemispheric_immune_create(
    const hemispheric_immune_config_t* config,
    hemispheric_brain_t* brain,
    brain_immune_system_t* immune
);

/**
 * @brief Destroy hemispheric immune bridge
 */
void hemispheric_immune_destroy(hemispheric_immune_bridge_t* bridge);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state from immune system
 *
 * Reads current inflammation and cytokine levels, computes effects
 *
 * @param bridge Bridge instance
 * @return 0 on success, negative on error
 */
int hemispheric_immune_update(hemispheric_immune_bridge_t* bridge);

/**
 * @brief Apply computed effects to hemispheric brain
 *
 * Modulates learning rates, callosum bandwidth, lateralization
 *
 * @param bridge Bridge instance
 * @return 0 on success, negative on error
 */
int hemispheric_immune_apply_modulation(hemispheric_immune_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current left hemisphere effects
 */
hemisphere_immune_effects_t hemispheric_immune_get_left_effects(
    const hemispheric_immune_bridge_t* bridge
);

/**
 * @brief Get current right hemisphere effects
 */
hemisphere_immune_effects_t hemispheric_immune_get_right_effects(
    const hemispheric_immune_bridge_t* bridge
);

/**
 * @brief Get current callosum effects
 */
callosum_immune_effects_t hemispheric_immune_get_callosum_effects(
    const hemispheric_immune_bridge_t* bridge
);

/**
 * @brief Get effective learning rate for hemisphere
 *
 * @param bridge Bridge instance
 * @param hemisphere Which hemisphere (HEMISPHERE_LEFT or HEMISPHERE_RIGHT)
 * @param base_lr Base learning rate
 * @return Modulated learning rate
 */
float hemispheric_immune_get_effective_lr(
    const hemispheric_immune_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    float base_lr
);

/**
 * @brief Get effective callosum bandwidth
 *
 * @param bridge Bridge instance
 * @param base_bandwidth Base bandwidth (messages per second)
 * @return Modulated bandwidth
 */
uint32_t hemispheric_immune_get_effective_bandwidth(
    const hemispheric_immune_bridge_t* bridge,
    uint32_t base_bandwidth
);

/**
 * @brief Check if compensation is active
 *
 * @param bridge Bridge instance
 * @param compensating_hemisphere Output: which hemisphere is compensating
 * @return true if compensation is active
 */
bool hemispheric_immune_is_compensating(
    const hemispheric_immune_bridge_t* bridge,
    hemisphere_id_t* compensating_hemisphere
);

//=============================================================================
// Control API
//=============================================================================

/**
 * @brief Manually set inflammation level (for testing)
 */
int hemispheric_immune_set_inflammation(
    hemispheric_immune_bridge_t* bridge,
    brain_inflammation_level_t level
);

/**
 * @brief Trigger emergency bilateral processing
 *
 * Forces both hemispheres to process all domains regardless of lateralization
 */
int hemispheric_immune_trigger_emergency_bilateral(
    hemispheric_immune_bridge_t* bridge
);

/**
 * @brief Clear emergency mode and restore normal operation
 */
int hemispheric_immune_clear_emergency(hemispheric_immune_bridge_t* bridge);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 */
hemispheric_immune_stats_t hemispheric_immune_get_stats(
    const hemispheric_immune_bridge_t* bridge
);

/**
 * @brief Reset statistics
 */
void hemispheric_immune_reset_stats(hemispheric_immune_bridge_t* bridge);

//=============================================================================
// Bio-async API
//=============================================================================

/**
 * @brief Connect to bio-async router
 */
int hemispheric_immune_connect_bio_async(hemispheric_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int hemispheric_immune_disconnect_bio_async(hemispheric_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_HEMISPHERIC_IMMUNE_BRIDGE_H
