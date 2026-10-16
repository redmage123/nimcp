/**
 * @file nimcp_endocannabinoid.h
 * @brief Endocannabinoid System (ECS) - Retrograde Synaptic Modulation
 * @date 2026-03-05
 *
 * The endocannabinoid system is a retrograde signaling system critical for:
 * - Depolarization-induced suppression of inhibition (DSI)
 * - Depolarization-induced suppression of excitation (DSE)
 * - Pain modulation (analgesia via CB1/CB2)
 * - Appetite regulation (hypothalamic CB1)
 * - Immune modulation (peripheral CB2)
 * - Tonic inhibition of presynaptic release
 *
 * Key molecules:
 * 1. 2-AG (2-arachidonoylglycerol) - primary retrograde messenger, degraded by MAGL
 * 2. AEA (anandamide) - tonic endocannabinoid, degraded by FAAH
 *
 * Receptors:
 * - CB1: dense in cortex/hippocampus/basal ganglia, presynaptic suppression
 * - CB2: primarily immune cells and microglia, anti-inflammatory
 */

#ifndef NIMCP_ENDOCANNABINOID_H
#define NIMCP_ENDOCANNABINOID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * Constants
 *===========================================================================*/

#define ECB_MAGIC           0xECBCA4AB
#define ECB_NUM_REGIONS     32      /* max brain regions for density tracking */

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Endocannabinoid types
 */
typedef enum {
    ECB_2AG = 0,            /**< 2-arachidonoylglycerol (primary retrograde) */
    ECB_AEA,                /**< Anandamide (tonic endocannabinoid) */
    ECB_TYPE_COUNT
} ecb_type_t;

/**
 * @brief Cannabinoid receptor types
 */
typedef enum {
    ECB_CB1 = 0,            /**< CB1 receptor (presynaptic, CNS-dense) */
    ECB_CB2,                /**< CB2 receptor (immune, peripheral) */
    ECB_RECEPTOR_COUNT
} ecb_receptor_t;

/*=============================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief ECS configuration parameters
 */
typedef struct {
    float base_two_ag;      /**< Baseline 2-AG level [0-1] */
    float base_aea;         /**< Baseline AEA level [0-1] */
    float magl_rate;        /**< MAGL degradation rate for 2-AG (per second) */
    float faah_rate;        /**< FAAH degradation rate for AEA (per second) */
    float cb1_gain;         /**< CB1 receptor sensitivity gain */
    float cb2_gain;         /**< CB2 receptor sensitivity gain */
} ecb_config_t;

/**
 * @brief Main Endocannabinoid System structure
 */
typedef struct endocannabinoid_system {
    uint32_t magic;                         /**< Magic number 0xECBCANAB */
    ecb_config_t config;

    /* Receptor density maps per brain region */
    float cb1_density[ECB_NUM_REGIONS];     /**< CB1 density per region [0-1] */
    float cb2_density[ECB_NUM_REGIONS];     /**< CB2 density per region [0-1] */

    /* Endocannabinoid levels */
    float two_ag_level;                     /**< 2-AG: primary retrograde signal [0-1] */
    float aea_level;                        /**< Anandamide: tonic signal [0-1] */

    /* Enzyme activity */
    float magl_activity;                    /**< MAGL activity: degrades 2-AG [0-1] */
    float faah_activity;                    /**< FAAH activity: degrades AEA [0-1] */

    /* Synaptic modulation outputs */
    float dsi_strength;                     /**< Depolarization-induced suppression of inhibition [0-1] */
    float dse_strength;                     /**< Depolarization-induced suppression of excitation [0-1] */
    float tonic_inhibition;                 /**< Baseline CB1 presynaptic suppression [0-1] */

    /* Accumulated depolarization (for 2-AG synthesis drive) */
    float depolarization_accumulator;       /**< Running depolarization signal [0-1] */

    /* Timing */
    uint64_t last_update_us;                /**< Last update timestamp (microseconds) */

    /* Thread safety (nimcp_mutex_t* from utils/thread/nimcp_thread.h) */
    void* lock;
} endocannabinoid_system_t;

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Get default ECS configuration
 */
ecb_config_t ecb_default_config(void);

/**
 * @brief Create and initialize an Endocannabinoid System
 * @param config Configuration (NULL for defaults)
 * @return Allocated system or NULL on failure
 */
endocannabinoid_system_t* ecb_create(const ecb_config_t* config);

/**
 * @brief Destroy an Endocannabinoid System and free resources
 * @param system System to destroy (NULL-safe)
 */
void ecb_destroy(endocannabinoid_system_t* system);

/*=============================================================================
 * Update API
 *===========================================================================*/

/**
 * @brief Update ECS dynamics for one timestep
 *
 * (1) 2-AG synthesis proportional to recent depolarization, degradation by MAGL
 * (2) AEA tonic synthesis, degradation by FAAH
 * (3) DSI/DSE strength from 2-AG level via sigmoid
 * (4) Tonic inhibition from AEA level
 *
 * @param system ECS instance
 * @param dt_s   Time step in seconds
 * @return 0 on success, -1 on error
 */
int ecb_update(endocannabinoid_system_t* system, float dt_s);

/*=============================================================================
 * Synaptic Modulation API
 *===========================================================================*/

/**
 * @brief Signal postsynaptic depolarization to drive 2-AG synthesis
 * @param system ECS instance
 * @param neuron_id Neuron that depolarized
 * @param depolarization Depolarization magnitude [0-1]
 * @return 0 on success, -1 on error
 */
int ecb_on_postsynaptic_depolarization(endocannabinoid_system_t* system,
                                        uint32_t neuron_id,
                                        float depolarization);

/**
 * @brief Get presynaptic suppression factor for a synapse
 * @param system ECS instance
 * @param synapse_id Synapse identifier
 * @return Suppression factor [0-1] where 0 = no suppression, 1 = full suppression
 */
float ecb_get_presynaptic_suppression(endocannabinoid_system_t* system,
                                       uint32_t synapse_id);

/**
 * @brief Get retrograde signal level for a given endocannabinoid type
 * @param system ECS instance
 * @param type ECB_2AG or ECB_AEA
 * @return Signal level [0-1], or -1.0f on error
 */
float ecb_get_retrograde_signal(endocannabinoid_system_t* system,
                                 ecb_type_t type);

/*=============================================================================
 * Pain Modulation API
 *===========================================================================*/

/**
 * @brief Modulate a pain signal through ECS analgesic pathway
 * @param system ECS instance
 * @param pain_signal Raw pain signal [0-1]
 * @param modulated_out Output: modulated pain signal [0-1]
 * @return 0 on success, -1 on error
 */
int ecb_modulate_pain(endocannabinoid_system_t* system,
                       float pain_signal,
                       float* modulated_out);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENDOCANNABINOID_H */
