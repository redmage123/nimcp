/**
 * @file nimcp_cochlea_thalamic_bridge.h
 * @brief Cochlea-Thalamus (MGN) integration bridge
 *
 * WHAT: Connect cochlear output to thalamic relay (Medial Geniculate Nucleus)
 * WHY:  Enable attention gating and cortical routing of auditory information
 * HOW:  MGN relay with TRN (thalamic reticular nucleus) attention control
 *
 * BIOLOGICAL BASIS:
 * - MGN: Medial Geniculate Nucleus - primary auditory thalamic relay
 * - TRN: Thalamic Reticular Nucleus - attention gating via inhibition
 * - Tonic/Burst modes: Continuous vs attention-grabbing signaling
 * - Attention modulates which frequencies pass to cortex
 *
 * BIDIRECTIONAL DATA FLOWS:
 * - OUTBOUND: Cochlea → Thalamus: Frequency activations, onset events
 * - INBOUND:  Thalamus → Cochlea: Attention weights, gain modulation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_THALAMIC_BRIDGE_H
#define NIMCP_COCHLEA_THALAMIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_cochlea.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct thalamus thalamus_t;
typedef struct thalamic_router thalamic_router_t;

//=============================================================================
// Constants
//=============================================================================

/** MGN relay parameters */
#define COCHLEA_THALAMIC_MGN_CHANNELS       64      /**< MGN frequency channels */
#define COCHLEA_THALAMIC_RELAY_LATENCY_MS   5.0f    /**< Thalamic relay latency */

/** Attention parameters */
#define COCHLEA_THALAMIC_ATTN_BANDWIDTH     1.0f    /**< Attention bandwidth (octaves) */
#define COCHLEA_THALAMIC_ATTN_GAIN_DB       12.0f   /**< Maximum attention gain */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief MGN firing mode
 *
 * BIOLOGICAL: Thalamic neurons switch between modes
 */
typedef enum {
    MGN_MODE_TONIC,                 /**< Continuous firing - faithful relay */
    MGN_MODE_BURST,                 /**< Burst firing - attention grabbing */
    MGN_MODE_SUPPRESSED             /**< Suppressed by TRN - gated out */
} mgn_mode_t;

/**
 * @brief Attention source type
 */
typedef enum {
    ATTN_SOURCE_BOTTOM_UP,          /**< Salience-driven (loud/novel) */
    ATTN_SOURCE_TOP_DOWN,           /**< Goal-directed (cocktail party) */
    ATTN_SOURCE_SPATIAL,            /**< Location-based attention */
    ATTN_SOURCE_FEATURE             /**< Feature-based (specific sound) */
} attention_source_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief MGN relay output
 */
typedef struct {
    float* relay_activation;        /**< Per-channel relay strength [num_channels] */
    mgn_mode_t* channel_mode;       /**< Per-channel firing mode [num_channels] */
    float* burst_strength;          /**< Burst strength if in burst mode */
    uint32_t num_channels;

    /* Aggregate measures */
    float total_relay_strength;     /**< Sum of all channel activations */
    bool attention_event;           /**< Burst mode triggered */
    float peak_frequency_hz;        /**< Dominant relayed frequency */
} mgn_output_t;

/**
 * @brief Attention state
 */
typedef struct {
    attention_source_t source;      /**< Current attention source */

    /* Frequency-based attention */
    float attended_freq_hz;         /**< Center of attention */
    float attention_bandwidth;      /**< Width in octaves */
    float attention_gain_db;        /**< Gain at attended frequency */

    /* Spatial attention (for localization) */
    float attended_azimuth;         /**< Attended direction */
    float spatial_bandwidth;        /**< Spatial attention width */

    /* Per-channel attention weights */
    float* channel_weights;         /**< Attention weights [num_channels] */
    uint32_t num_channels;

    /* TRN inhibition pattern */
    float* trn_inhibition;          /**< Inhibition from TRN [num_channels] */
} attention_state_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* MGN parameters */
    uint32_t num_mgn_channels;      /**< Number of MGN channels */
    float relay_latency_ms;         /**< Relay latency */

    /* Burst mode parameters */
    float burst_threshold;          /**< Threshold for burst mode */
    float burst_duration_ms;        /**< Burst duration */

    /* Attention parameters */
    float attention_bandwidth;      /**< Default attention bandwidth */
    float max_attention_gain_db;    /**< Maximum attention gain */
    bool enable_spatial_attention;  /**< Enable spatial attention */

    /* TRN parameters */
    float trn_inhibition_strength;  /**< TRN inhibition strength */
    float trn_decay_ms;             /**< TRN inhibition decay */

} cochlea_thalamic_config_t;

/**
 * @brief Cochlea-Thalamic bridge instance
 */
typedef struct cochlea_thalamic_bridge cochlea_thalamic_bridge_t;

//=============================================================================
// Configuration Helpers
//=============================================================================

/**
 * @brief Get default bridge configuration
 */
cochlea_thalamic_config_t cochlea_thalamic_config_default(void);

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create cochlea-thalamic bridge
 *
 * @param cochlea Cochlea instance
 * @param thalamus Thalamus instance (can be NULL)
 * @param config Bridge configuration
 * @return Bridge instance or NULL
 */
cochlea_thalamic_bridge_t* cochlea_thalamic_bridge_create(
    cochlea_t* cochlea,
    thalamus_t* thalamus,
    const cochlea_thalamic_config_t* config
);

/**
 * @brief Destroy bridge
 */
void cochlea_thalamic_bridge_destroy(cochlea_thalamic_bridge_t* bridge);

/**
 * @brief Update bridge (process cochlear output through MGN)
 *
 * @param bridge Bridge instance
 * @param cochlea_output Current cochlear output
 * @param dt_ms Time step
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_thalamic_bridge_update(
    cochlea_thalamic_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
);

/**
 * @brief Reset bridge state
 */
nimcp_error_t cochlea_thalamic_bridge_reset(cochlea_thalamic_bridge_t* bridge);

//=============================================================================
// Attention Control
//=============================================================================

/**
 * @brief Set frequency-based attention
 *
 * @param bridge Bridge instance
 * @param frequency_hz Center frequency to attend
 * @param bandwidth Bandwidth in octaves
 * @param gain_db Attention gain
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_thalamic_set_frequency_attention(
    cochlea_thalamic_bridge_t* bridge,
    float frequency_hz,
    float bandwidth,
    float gain_db
);

/**
 * @brief Set spatial attention
 *
 * @param bridge Bridge instance
 * @param azimuth_deg Attended azimuth
 * @param bandwidth Spatial bandwidth
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_thalamic_set_spatial_attention(
    cochlea_thalamic_bridge_t* bridge,
    float azimuth_deg,
    float bandwidth
);

/**
 * @brief Clear attention (reset to uniform)
 */
nimcp_error_t cochlea_thalamic_clear_attention(cochlea_thalamic_bridge_t* bridge);

/**
 * @brief Get current attention state
 */
nimcp_error_t cochlea_thalamic_get_attention(
    const cochlea_thalamic_bridge_t* bridge,
    attention_state_t* state
);

//=============================================================================
// MGN Access
//=============================================================================

/**
 * @brief Get MGN output
 */
nimcp_error_t cochlea_thalamic_get_mgn_output(
    const cochlea_thalamic_bridge_t* bridge,
    mgn_output_t* output
);

/**
 * @brief Force channel to burst mode
 */
nimcp_error_t cochlea_thalamic_trigger_burst(
    cochlea_thalamic_bridge_t* bridge,
    uint32_t channel
);

//=============================================================================
// Bidirectional Verification
//=============================================================================

/**
 * @brief Verify bidirectional data flow
 */
bool cochlea_thalamic_verify_bidirectional(
    const cochlea_thalamic_bridge_t* bridge
);

uint64_t cochlea_thalamic_get_last_outbound(
    const cochlea_thalamic_bridge_t* bridge
);

uint64_t cochlea_thalamic_get_last_inbound(
    const cochlea_thalamic_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_THALAMIC_BRIDGE_H */
