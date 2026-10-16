//=============================================================================
// nimcp_hemispheric_sleep_bridge.h - Hemispheric Brain Sleep Integration
//=============================================================================
/**
 * @file nimcp_hemispheric_sleep_bridge.h
 * @brief Bidirectional integration between hemispheric brain and sleep system
 *
 * WHAT: Integration layer connecting hemispheric brain with sleep states
 * WHY:  Sleep affects hemispheres asymmetrically; callosum recovers during sleep;
 *       memory consolidation involves inter-hemispheric transfer
 * HOW:  Sleep stage modulation of per-hemisphere activity, callosum bandwidth,
 *       and lateralization plasticity
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → HEMISPHERIC PATHWAYS:
 * ------------------------------
 * 1. Asymmetric Sleep Depth (Unihemispheric Sleep):
 *    - Marine mammals show one hemisphere sleeping while other wakes
 *    - Humans show subtle lateralization during sleep stages
 *    - Left hemisphere often enters deep sleep first
 *    - Right hemisphere maintains more vigilance
 *
 * 2. Memory Consolidation:
 *    - NREM: Hippocampal replay → cortical transfer
 *    - REM: Inter-hemispheric integration via callosum
 *    - Slow wave sleep: Synaptic homeostasis (both hemispheres)
 *    - Different hemisphere specializations consolidate at different times
 *
 * 3. Callosum Recovery:
 *    - Callosum efficiency degrades with fatigue
 *    - Sleep restores callosum myelination
 *    - REM particularly important for callosum health
 *    - Bandwidth recovers progressively through sleep stages
 *
 * 4. Lateralization Plasticity:
 *    - Sleep allows dominance shifts to stabilize
 *    - REM promotes creative cross-hemisphere connections
 *    - NREM consolidates hemisphere-specific learning
 *
 * HEMISPHERIC → SLEEP PATHWAYS:
 * ------------------------------
 * 1. Activity-Dependent Sleep Pressure:
 *    - Heavily-used hemisphere accumulates more adenosine
 *    - Asymmetric sleep pressure based on daytime usage
 *
 * 2. Callosum State Affects Sleep Quality:
 *    - Impaired callosum → fragmented sleep
 *    - Split-brain → independent sleep cycles per hemisphere
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    HEMISPHERIC-SLEEP BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐ ║
 * ║   │                   SLEEP → HEMISPHERIC                               │ ║
 * ║   │                                                                      │ ║
 * ║   │   Sleep Stage     Left Hemisphere    Right Hemisphere               │ ║
 * ║   │   ───────────     ───────────────    ────────────────               │ ║
 * ║   │   AWAKE        →  100% activity       100% activity                 │ ║
 * ║   │   NREM1        →   80% activity        85% activity (vigilant)      │ ║
 * ║   │   NREM2        →   50% activity        60% activity                 │ ║
 * ║   │   NREM3 (SWS)  →   20% activity        25% activity                 │ ║
 * ║   │   REM          →   70% activity        70% activity (dreaming)      │ ║
 * ║   │                                                                      │ ║
 * ║   │   Callosum Bandwidth Recovery:                                       │ ║
 * ║   │   AWAKE=degrading, NREM1=stable, NREM2=+5%, NREM3=+10%, REM=+15%    │ ║
 * ║   │                                                                      │ ║
 * ║   │   Memory Consolidation Mode:                                         │ ║
 * ║   │   NREM: Hemisphere-specific, SWS: Both, REM: Inter-hemispheric      │ ║
 * ║   └─────────────────────────────────────────────────────────────────────┘ ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_HEMISPHERIC_SLEEP_BRIDGE_H
#define NIMCP_HEMISPHERIC_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/free_energy/nimcp_fep_sleep.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "async/nimcp_bio_async.h"
#include "utils/thread/nimcp_thread.h"

// Sleep stage aliases for clearer naming
#define SLEEP_STAGE_AWAKE SLEEP_STAGE_WAKE
#define SLEEP_STAGE_NREM1 SLEEP_STAGE_N1
#define SLEEP_STAGE_NREM2 SLEEP_STAGE_N2
#define SLEEP_STAGE_NREM3 SLEEP_STAGE_SWS

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Activity factors per sleep stage (left hemisphere - enters sleep first) */
#define HEMI_SLEEP_ACTIVITY_LEFT_AWAKE       1.00f
#define HEMI_SLEEP_ACTIVITY_LEFT_NREM1       0.80f
#define HEMI_SLEEP_ACTIVITY_LEFT_NREM2       0.50f
#define HEMI_SLEEP_ACTIVITY_LEFT_NREM3       0.20f
#define HEMI_SLEEP_ACTIVITY_LEFT_REM         0.70f

/** Activity factors per sleep stage (right hemisphere - more vigilant) */
#define HEMI_SLEEP_ACTIVITY_RIGHT_AWAKE      1.00f
#define HEMI_SLEEP_ACTIVITY_RIGHT_NREM1      0.85f
#define HEMI_SLEEP_ACTIVITY_RIGHT_NREM2      0.60f
#define HEMI_SLEEP_ACTIVITY_RIGHT_NREM3      0.25f
#define HEMI_SLEEP_ACTIVITY_RIGHT_REM        0.70f

/** Callosum bandwidth recovery rates per sleep stage */
#define HEMI_SLEEP_CALLOSUM_AWAKE_RATE      -0.02f   // Degrades when awake
#define HEMI_SLEEP_CALLOSUM_NREM1_RATE       0.00f   // Stable
#define HEMI_SLEEP_CALLOSUM_NREM2_RATE       0.05f   // Slight recovery
#define HEMI_SLEEP_CALLOSUM_NREM3_RATE       0.10f   // Good recovery
#define HEMI_SLEEP_CALLOSUM_REM_RATE         0.15f   // Best recovery

/** Learning rate factors per sleep stage */
#define HEMI_SLEEP_LR_AWAKE                  1.00f
#define HEMI_SLEEP_LR_NREM1                  0.30f   // Minimal learning
#define HEMI_SLEEP_LR_NREM2                  0.10f   // Almost no learning
#define HEMI_SLEEP_LR_NREM3                  0.05f   // Consolidation, not learning
#define HEMI_SLEEP_LR_REM                    0.20f   // Some creative learning

/** Plasticity factors per sleep stage */
#define HEMI_SLEEP_PLASTICITY_AWAKE          1.00f
#define HEMI_SLEEP_PLASTICITY_NREM1          0.80f
#define HEMI_SLEEP_PLASTICITY_NREM2          1.20f   // Enhanced consolidation
#define HEMI_SLEEP_PLASTICITY_NREM3          1.50f   // Strong consolidation
#define HEMI_SLEEP_PLASTICITY_REM            1.30f   // Creative plasticity

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Memory consolidation mode
 */
typedef enum {
    CONSOLIDATION_NONE,              /**< No active consolidation */
    CONSOLIDATION_HEMISPHERE_LEFT,   /**< Left hemisphere consolidating */
    CONSOLIDATION_HEMISPHERE_RIGHT,  /**< Right hemisphere consolidating */
    CONSOLIDATION_BILATERAL,         /**< Both hemispheres (SWS) */
    CONSOLIDATION_INTERHEMISPHERIC   /**< Cross-hemisphere transfer (REM) */
} consolidation_mode_t;

/**
 * @brief Per-hemisphere sleep effects
 */
typedef struct {
    float activity_factor;           /**< Activity multiplier (0.0-1.0) */
    float learning_rate_factor;      /**< LR multiplier (0.0-1.0) */
    float consolidation_strength;    /**< Consolidation effectiveness */
    bool is_dreaming;                /**< REM dreaming state */
} hemisphere_sleep_effects_t;

/**
 * @brief Callosum sleep effects
 */
typedef struct {
    float bandwidth_recovery;        /**< Bandwidth recovery rate */
    float current_efficiency;        /**< Current callosum efficiency (0.0-1.0) */
    float interhemispheric_transfer; /**< Transfer rate for consolidation */
    bool recovery_active;            /**< Recovery mode active */
} callosum_sleep_effects_t;

/**
 * @brief Lateralization sleep effects
 */
typedef struct {
    float plasticity_factor;         /**< Plasticity rate multiplier */
    bool stabilization_active;       /**< Dominance stabilization in progress */
    consolidation_mode_t consolidation_mode;
} lateralization_sleep_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    // Asymmetry settings
    float left_sleep_depth_bias;     /**< Left tends to sleep deeper (0.0-1.0) */
    float right_vigilance_bias;      /**< Right maintains more vigilance (0.0-1.0) */

    // Recovery settings
    float callosum_recovery_rate;    /**< Base callosum recovery rate */
    float max_callosum_efficiency;   /**< Maximum recoverable efficiency */

    // Consolidation settings
    bool enable_consolidation;       /**< Enable memory consolidation */
    float consolidation_threshold;   /**< Min sleep depth for consolidation */

    // Bio-async settings
    bool enable_bio_async;           /**< Enable bio-async messaging */
} hemispheric_sleep_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t updates;                /**< Total update calls */
    uint64_t sleep_cycles;           /**< Complete sleep cycles */
    float total_consolidation_time;  /**< Total time in consolidation */
    float avg_callosum_efficiency;   /**< Average callosum efficiency */
    float peak_recovery_rate;        /**< Peak callosum recovery rate */
    uint64_t interhemispheric_transfers; /**< REM transfer events */
} hemispheric_sleep_stats_t;

/**
 * @brief Hemispheric sleep bridge structure
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    // Connected systems
    hemispheric_brain_t* brain;      /**< Hemispheric brain */
    fep_sleep_system_t* sleep_system; /**< FEP sleep system */

    // Configuration
    hemispheric_sleep_config_t config;

    // Current effects
    hemisphere_sleep_effects_t left_effects;
    hemisphere_sleep_effects_t right_effects;
    callosum_sleep_effects_t callosum_effects;
    lateralization_sleep_effects_t lateralization_effects;

    // Current sleep state
    fep_sleep_stage_t current_stage;
    float sleep_depth;               /**< Overall sleep depth (0.0-1.0) */

    // Statistics
    hemispheric_sleep_stats_t stats;

    // Bio-async
    // Thread safety
    // State
    bool initialized;
} hemispheric_sleep_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default bridge configuration
 */
hemispheric_sleep_config_t hemispheric_sleep_default_config(void);

/**
 * @brief Create hemispheric sleep bridge
 *
 * @param config Bridge configuration
 * @param brain Hemispheric brain to connect
 * @param sleep Sleep system to connect
 * @return Bridge instance or NULL on failure
 */
hemispheric_sleep_bridge_t* hemispheric_sleep_create(
    const hemispheric_sleep_config_t* config,
    hemispheric_brain_t* brain,
    fep_sleep_system_t* sleep
);

/**
 * @brief Destroy hemispheric sleep bridge
 */
void hemispheric_sleep_destroy(hemispheric_sleep_bridge_t* bridge);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state from sleep system
 *
 * @param bridge Bridge instance
 * @return 0 on success, negative on error
 */
int hemispheric_sleep_update(hemispheric_sleep_bridge_t* bridge);

/**
 * @brief Apply computed effects to hemispheric brain
 *
 * @param bridge Bridge instance
 * @return 0 on success, negative on error
 */
int hemispheric_sleep_apply_modulation(hemispheric_sleep_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current left hemisphere effects
 */
hemisphere_sleep_effects_t hemispheric_sleep_get_left_effects(
    const hemispheric_sleep_bridge_t* bridge
);

/**
 * @brief Get current right hemisphere effects
 */
hemisphere_sleep_effects_t hemispheric_sleep_get_right_effects(
    const hemispheric_sleep_bridge_t* bridge
);

/**
 * @brief Get current callosum effects
 */
callosum_sleep_effects_t hemispheric_sleep_get_callosum_effects(
    const hemispheric_sleep_bridge_t* bridge
);

/**
 * @brief Get effective activity level for hemisphere
 *
 * @param bridge Bridge instance
 * @param hemisphere Which hemisphere
 * @return Activity level (0.0-1.0)
 */
float hemispheric_sleep_get_activity_level(
    const hemispheric_sleep_bridge_t* bridge,
    hemisphere_id_t hemisphere
);

/**
 * @brief Get current consolidation mode
 */
consolidation_mode_t hemispheric_sleep_get_consolidation_mode(
    const hemispheric_sleep_bridge_t* bridge
);

/**
 * @brief Check if hemisphere is in REM dreaming
 */
bool hemispheric_sleep_is_dreaming(
    const hemispheric_sleep_bridge_t* bridge,
    hemisphere_id_t hemisphere
);

//=============================================================================
// Control API
//=============================================================================

/**
 * @brief Manually set sleep stage (for testing)
 */
int hemispheric_sleep_set_stage(
    hemispheric_sleep_bridge_t* bridge,
    fep_sleep_stage_t stage
);

/**
 * @brief Trigger inter-hemispheric consolidation transfer
 *
 * WHAT: Force cross-hemisphere memory transfer
 * WHY:  Simulate REM inter-hemispheric integration
 */
int hemispheric_sleep_trigger_transfer(hemispheric_sleep_bridge_t* bridge);

/**
 * @brief Reset callosum efficiency to baseline
 */
int hemispheric_sleep_reset_callosum(hemispheric_sleep_bridge_t* bridge);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 */
hemispheric_sleep_stats_t hemispheric_sleep_get_stats(
    const hemispheric_sleep_bridge_t* bridge
);

/**
 * @brief Reset statistics
 */
void hemispheric_sleep_reset_stats(hemispheric_sleep_bridge_t* bridge);

//=============================================================================
// Bio-async API
//=============================================================================

/**
 * @brief Connect to bio-async router
 */
int hemispheric_sleep_connect_bio_async(hemispheric_sleep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int hemispheric_sleep_disconnect_bio_async(hemispheric_sleep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_HEMISPHERIC_SLEEP_BRIDGE_H
