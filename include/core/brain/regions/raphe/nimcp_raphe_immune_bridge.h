/**
 * @file nimcp_raphe_immune_bridge.h
 * @brief Raphe Nuclei - Brain Immune System Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between Raphe (serotonin) and brain immune system
 * WHY:  Enable 5-HT-mediated immune modulation and cytokine effects on mood
 * HOW:  5-HT modulates immune cells; inflammation affects serotonin signaling
 *
 * THEORETICAL FOUNDATIONS:
 * - Dantzer et al. (2008): Inflammation and depression
 * - Maes et al. (2011): Cytokines and serotonergic function
 * - Leonard (2010): 5-HT immunomodulation
 *
 * BIOLOGICAL BASIS:
 * - 5-HT modulates immune cell activity
 * - Inflammation depletes tryptophan (5-HT precursor)
 * - Cytokines affect SERT and 5-HT receptors
 * - Depression-inflammation link via 5-HT
 *
 * INTEGRATION FLOWS:
 *
 * Raphe --> Immune:
 *   1. 5-HT modulates cytokine release
 *   2. Mood state affects immune vigilance
 *   3. Stress-5-HT interactions shape immunity
 *   4. 5-HT influences microglial activity
 *
 * Immune --> Raphe:
 *   1. Inflammation reduces 5-HT synthesis
 *   2. Cytokines increase SERT activity
 *   3. IDO activation depletes tryptophan
 *   4. Chronic inflammation causes 5-HT deficit
 *
 * @see nimcp_raphe.h
 * @see nimcp_brain_immune.h
 */

#ifndef NIMCP_RAPHE_IMMUNE_BRIDGE_H
#define NIMCP_RAPHE_IMMUNE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_raphe_adapter_struct;
typedef struct nimcp_raphe_adapter_struct* nimcp_raphe_adapter_t;
struct nimcp_brain_immune_system;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Inflammation threshold for 5-HT effects */
#define RAPHE_IMMUNE_INFLAM_THRESHOLD   0.3f

/** @brief Maximum 5-HT depletion by inflammation */
#define RAPHE_IMMUNE_DEPLETION_MAX      0.6f

/** @brief Bio-async module ID */
#define BIO_MODULE_RAPHE_IMMUNE_BRIDGE  0x0E30

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief 5-HT-immune interaction mode
 */
typedef enum {
    RAPHE_IMMUNE_MODE_NORMAL = 0,    /**< Normal 5-HT-immune balance */
    RAPHE_IMMUNE_MODE_INFLAMED,      /**< Inflammation affecting 5-HT */
    RAPHE_IMMUNE_MODE_DEPLETED,      /**< 5-HT depletion state */
    RAPHE_IMMUNE_MODE_RECOVERING     /**< Recovery phase */
} nimcp_raphe_immune_mode_t;

/**
 * @brief Tryptophan status
 */
typedef enum {
    RAPHE_IMMUNE_TRP_NORMAL = 0,     /**< Normal tryptophan */
    RAPHE_IMMUNE_TRP_REDUCED,        /**< Reduced by IDO */
    RAPHE_IMMUNE_TRP_DEPLETED,       /**< Severely depleted */
    RAPHE_IMMUNE_TRP_RECOVERING      /**< Recovering */
} nimcp_raphe_immune_trp_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Raphe-Immune bridge configuration
 */
typedef struct {
    /* Inflammation effects */
    float inflammation_threshold;    /**< Threshold for 5-HT effects */
    float depletion_max;             /**< Maximum 5-HT depletion */
    float depletion_tau_ms;          /**< Depletion time constant */

    /* IDO pathway */
    bool enable_ido_pathway;         /**< Model IDO activation */
    float ido_gain;                  /**< IDO effect strength */
    float tryptophan_decay;          /**< Tryptophan depletion rate */

    /* 5-HT immunomodulation */
    float ht5_immune_gain;           /**< 5-HT effect on immune */
    bool enable_microglial_mod;      /**< 5-HT microglial effects */

    /* Recovery */
    float recovery_rate;             /**< Recovery rate */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_raphe_immune_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Immune modulation output
 */
typedef struct {
    float cytokine_modulation;       /**< 5-HT effect on cytokines */
    float microglial_modulation;     /**< 5-HT effect on microglia */
    float immune_vigilance;          /**< Overall immune vigilance */
} nimcp_raphe_immune_modulation_t;

/**
 * @brief Immune feedback to Raphe
 */
typedef struct {
    nimcp_raphe_immune_mode_t mode;  /**< Current interaction mode */
    float inflammation_level;        /**< Overall inflammation [0-1] */
    float ht5_depletion;             /**< 5-HT depletion factor */
    nimcp_raphe_immune_trp_t trp_status; /**< Tryptophan status */
    float ido_activity;              /**< IDO activation level */
    float sert_upregulation;         /**< SERT activity increase */
} nimcp_raphe_immune_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_raphe_immune_mode_t current_mode;
    float current_depletion;
    nimcp_raphe_immune_trp_t trp_status;
    float accumulated_inflammation;
    float recovery_progress;
} nimcp_raphe_immune_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t depletion_events;
    float total_inflammation_time;
    float avg_depletion;
    float max_depletion;
    float avg_ido_activity;
} nimcp_raphe_immune_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_raphe_immune_bridge nimcp_raphe_immune_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_raphe_immune_config_t nimcp_raphe_immune_config_default(void);

nimcp_raphe_immune_bridge_t* nimcp_raphe_immune_create(
    const nimcp_raphe_immune_config_t* config
);

void nimcp_raphe_immune_destroy(nimcp_raphe_immune_bridge_t* bridge);

int nimcp_raphe_immune_reset(nimcp_raphe_immune_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_raphe_immune_connect_raphe(
    nimcp_raphe_immune_bridge_t* bridge,
    nimcp_raphe_adapter_t raphe_adapter
);

int nimcp_raphe_immune_connect_immune(
    nimcp_raphe_immune_bridge_t* bridge,
    struct nimcp_brain_immune_system* immune
);

/*=============================================================================
 * Raphe --> Immune API
 *===========================================================================*/

/**
 * @brief Compute immune modulation from 5-HT state
 */
int nimcp_raphe_immune_compute_modulation(
    nimcp_raphe_immune_bridge_t* bridge,
    nimcp_raphe_immune_modulation_t* modulation
);

/**
 * @brief Apply 5-HT immunomodulation
 */
int nimcp_raphe_immune_apply_modulation(
    nimcp_raphe_immune_bridge_t* bridge,
    float ht5_concentration
);

/*=============================================================================
 * Immune --> Raphe API
 *===========================================================================*/

/**
 * @brief Receive immune feedback
 */
int nimcp_raphe_immune_receive_feedback(
    nimcp_raphe_immune_bridge_t* bridge,
    const nimcp_raphe_immune_feedback_t* feedback
);

/**
 * @brief Get 5-HT depletion from inflammation
 */
float nimcp_raphe_immune_get_depletion(nimcp_raphe_immune_bridge_t* bridge);

/**
 * @brief Get tryptophan availability
 */
float nimcp_raphe_immune_get_tryptophan(nimcp_raphe_immune_bridge_t* bridge);

/**
 * @brief Check if depression-like state active
 */
bool nimcp_raphe_immune_is_depressed(nimcp_raphe_immune_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_raphe_immune_update(nimcp_raphe_immune_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_raphe_immune_get_state(
    const nimcp_raphe_immune_bridge_t* bridge,
    nimcp_raphe_immune_bridge_state_t* state
);

int nimcp_raphe_immune_get_stats(
    const nimcp_raphe_immune_bridge_t* bridge,
    nimcp_raphe_immune_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RAPHE_IMMUNE_BRIDGE_H */
