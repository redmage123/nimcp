/**
 * @file nimcp_occipital_thalamic_bridge.h
 * @brief Bridge between Occipital Cortex and thalamic router
 *
 * WHAT: Routes occipital visual signals through thalamic relay nuclei
 * WHY: All primary visual information passes through LGN before reaching V1
 * HOW: Manages LGN, Pulvinar, SC pathways with attention gating
 *
 * BIOLOGICAL BASIS:
 * - LGN (Lateral Geniculate Nucleus): Primary visual relay to V1
 *   - Magnocellular (M) pathway: Motion, low contrast, fast
 *   - Parvocellular (P) pathway: Color, form, high resolution
 *   - Koniocellular (K) pathway: Blue-yellow color
 * - Pulvinar: Higher visual association, attention modulation
 * - Superior Colliculus (SC): Saccade control, fast motion detection
 * - TRN (Thalamic Reticular Nucleus): Inhibitory gating
 *
 * PATHWAYS:
 * - Retina → LGN → V1 (primary visual pathway)
 * - V1 ↔ Pulvinar (attention feedback loop)
 * - SC → Pulvinar → V5/MT (fast motion pathway)
 * - TRN gates all thalamic nuclei based on arousal/attention
 *
 * @author NIMCP Team
 * @date 2025-01-01
 */

#ifndef NIMCP_OCCIPITAL_THALAMIC_BRIDGE_H
#define NIMCP_OCCIPITAL_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

/** Bio-async module ID for occipital thalamic bridge */
#define BIO_MODULE_THALAMIC_OCCIPITAL 0x2D11

/** Signal types for occipital-thalamic routing */
#define OCCIPITAL_SIGNAL_V1          0x2D01  /**< V1 primary visual */
#define OCCIPITAL_SIGNAL_V2          0x2D02  /**< V2 secondary visual */
#define OCCIPITAL_SIGNAL_V3          0x2D03  /**< V3 form processing */
#define OCCIPITAL_SIGNAL_V4          0x2D04  /**< V4 color/form */
#define OCCIPITAL_SIGNAL_V5          0x2D05  /**< V5/MT motion */
#define OCCIPITAL_SIGNAL_DORSAL      0x2D10  /**< Dorsal stream aggregate */
#define OCCIPITAL_SIGNAL_VENTRAL     0x2D11  /**< Ventral stream aggregate */
#define OCCIPITAL_SIGNAL_MAGNO       0x2D20  /**< Magnocellular pathway */
#define OCCIPITAL_SIGNAL_PARVO       0x2D21  /**< Parvocellular pathway */
#define OCCIPITAL_SIGNAL_KONIO       0x2D22  /**< Koniocellular pathway */

/*=============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Thalamic nucleus types for visual pathways
 */
typedef enum {
    OCCIPITAL_THALAMIC_LGN = 0,       /**< Lateral Geniculate Nucleus */
    OCCIPITAL_THALAMIC_PULVINAR,      /**< Pulvinar (attention, association) */
    OCCIPITAL_THALAMIC_SC,            /**< Superior Colliculus (saccades) */
    OCCIPITAL_THALAMIC_TRN,           /**< Thalamic Reticular Nucleus (gating) */
    OCCIPITAL_THALAMIC_COUNT
} occipital_thalamic_nucleus_t;

/**
 * @brief LGN pathway types
 */
typedef enum {
    LGN_PATHWAY_MAGNOCELLULAR = 0,    /**< Motion, low contrast, fast */
    LGN_PATHWAY_PARVOCELLULAR,        /**< Color, form, high resolution */
    LGN_PATHWAY_KONIOCELLULAR,        /**< Blue-yellow color */
    LGN_PATHWAY_COUNT
} lgn_pathway_type_t;

/**
 * @brief Thalamic routing state
 */
typedef struct {
    float lgn_gain;                   /**< LGN overall relay gain [0-2] */
    float pulvinar_attention;         /**< Pulvinar attention level [0-1] */
    float sc_saccade_readiness;       /**< SC saccade preparation [0-1] */
    float trn_gate;                   /**< TRN gating factor [0-1] */

    /* LGN pathway-specific gains */
    float magno_gain;                 /**< M-pathway gain [0-2] */
    float parvo_gain;                 /**< P-pathway gain [0-2] */
    float konio_gain;                 /**< K-pathway gain [0-2] */

    /* Pathway activity flags */
    bool magnocellular_active;        /**< M-pathway receiving signals */
    bool parvocellular_active;        /**< P-pathway receiving signals */
    bool koniocellular_active;        /**< K-pathway receiving signals */

    /* Retinotopic information */
    float attention_x;                /**< Attended location X [0-1] */
    float attention_y;                /**< Attended location Y [0-1] */
    float attention_radius;           /**< Attention spotlight radius [0-1] */

    /* Feedback state */
    float cortical_feedback;          /**< V1 → LGN feedback strength [0-1] */
    bool feedback_active;             /**< Feedback loop engaged */
} occipital_thalamic_state_t;

/**
 * @brief Visual signal for thalamic routing
 */
typedef struct {
    uint32_t signal_type;             /**< Signal type (OCCIPITAL_SIGNAL_*) */
    lgn_pathway_type_t pathway;       /**< LGN pathway preference */
    float visual_intensity;           /**< Signal intensity [0-1] */
    float contrast;                   /**< Local contrast [0-1] */
    float spatial_frequency;          /**< Spatial frequency (cycles/degree) */
    float temporal_frequency;         /**< Temporal frequency (Hz) */
    float retinotopic_x;              /**< Retinotopic X position [0-1] */
    float retinotopic_y;              /**< Retinotopic Y position [0-1] */
    float eccentricity;               /**< Distance from fovea [0-1] */
    void* content;                    /**< Signal payload */
    uint32_t content_size;            /**< Payload size in bytes */
    uint64_t timestamp_us;            /**< Timestamp in microseconds */
} occipital_thalamic_signal_t;

/**
 * @brief Routing request
 */
typedef struct {
    occipital_thalamic_nucleus_t source;  /**< Source nucleus */
    float* signal;                    /**< Signal vector */
    uint32_t signal_dim;              /**< Signal dimension */
    float urgency;                    /**< Routing urgency [0-1] */
    float attention_boost;            /**< Attention modulation [-1, 1] */
    uint32_t visual_field_quadrant;   /**< Quadrant for retinotopic routing */
    double timestamp_ms;              /**< Request timestamp */
} occipital_thalamic_request_t;

/**
 * @brief Routing response
 */
typedef struct {
    float* routed_signal;             /**< Routed signal vector */
    uint32_t signal_dim;              /**< Signal dimension */
    float effective_gain;             /**< Applied gain */
    float gating_applied;             /**< TRN gating factor applied */
    bool was_suppressed;              /**< Signal was gated out */
    double latency_ms;                /**< Routing latency */
    occipital_thalamic_nucleus_t routed_via;  /**< Which nucleus routed */
} occipital_thalamic_response_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Enable/disable toggles */
    bool enable_attention_gating;     /**< Gate signals by attention */
    bool enable_contrast_boost;       /**< Boost high-contrast signals */
    bool enable_retinotopic_routing;  /**< Route by visual field position */
    bool enable_magno_parvo_separation; /**< Separate M/P pathways */
    bool enable_cortical_feedback;    /**< V1 → LGN feedback loop */
    bool enable_bio_async;            /**< Bio-async messaging */

    /* Threshold parameters */
    float min_visual_intensity;       /**< Minimum intensity for routing */
    float contrast_threshold;         /**< Contrast for boost activation */
    float attention_threshold;        /**< Minimum attention for enhancement */

    /* Pathway boost factors */
    float magnocellular_boost;        /**< M-pathway gain [0-2] */
    float parvocellular_boost;        /**< P-pathway gain [0-2] */
    float koniocellular_boost;        /**< K-pathway gain [0-2] */

    /* Decay rates */
    float attention_decay_rate;       /**< Attention decay per update */
    float feedback_decay_rate;        /**< Cortical feedback decay */

    /* Latency simulation */
    float lgn_latency_ms;             /**< LGN relay latency (default: 2ms) */
    float pulvinar_latency_ms;        /**< Pulvinar latency (default: 5ms) */
} occipital_thalamic_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Signal counts */
    uint64_t v1_signals_routed;       /**< V1 signals processed */
    uint64_t v2_signals_routed;       /**< V2 signals processed */
    uint64_t dorsal_signals;          /**< Dorsal stream signals */
    uint64_t ventral_signals;         /**< Ventral stream signals */
    uint64_t signals_suppressed;      /**< Signals gated by TRN */

    /* Pathway statistics */
    uint64_t magno_signals;           /**< M-pathway signals */
    uint64_t parvo_signals;           /**< P-pathway signals */
    uint64_t konio_signals;           /**< K-pathway signals */

    /* Performance metrics */
    float avg_visual_intensity;       /**< Average signal intensity */
    float avg_routing_latency_ms;     /**< Average routing latency */
    float avg_lgn_gain;               /**< Average LGN gain */
    float avg_pulvinar_attention;     /**< Average attention level */

    /* Bio-async */
    uint64_t bio_messages_sent;       /**< Messages broadcast */
    uint64_t bio_messages_received;   /**< Messages received */
    uint64_t attention_requests;      /**< Attention modulation requests */
} occipital_thalamic_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct occipital_thalamic_bridge occipital_thalamic_bridge_t;

/*=============================================================================
 * Configuration API
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * @return Default biologically-motivated configuration
 */
occipital_thalamic_config_t occipital_thalamic_default_config(void);

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create occipital thalamic bridge
 *
 * @param occipital Occipital adapter handle (void* for flexibility)
 * @param router Thalamic router handle (required)
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
occipital_thalamic_bridge_t* occipital_thalamic_bridge_create(
    void* occipital,
    thalamic_router_t* router,
    const occipital_thalamic_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
void occipital_thalamic_bridge_destroy(occipital_thalamic_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_bridge_reset(occipital_thalamic_bridge_t* bridge);

/*=============================================================================
 * Signal Routing API
 *===========================================================================*/

/**
 * @brief Route visual signal through thalamus
 *
 * Applies attention gating, contrast boost, and pathway routing.
 *
 * @param bridge Bridge handle
 * @param signal Signal to route
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_route_signal(
    occipital_thalamic_bridge_t* bridge,
    const occipital_thalamic_signal_t* signal
);

/**
 * @brief Route V1 signal (convenience function)
 *
 * @param bridge Bridge handle
 * @param visual_data Visual data payload
 * @param intensity Signal intensity
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_route_v1(
    occipital_thalamic_bridge_t* bridge,
    const void* visual_data,
    float intensity
);

/**
 * @brief Route dorsal stream signal
 *
 * @param bridge Bridge handle
 * @param motion_data Motion features
 * @param motion_strength Motion signal strength
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_route_dorsal(
    occipital_thalamic_bridge_t* bridge,
    const void* motion_data,
    float motion_strength
);

/**
 * @brief Route ventral stream signal
 *
 * @param bridge Bridge handle
 * @param form_data Form/color features
 * @param form_strength Signal strength
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_route_ventral(
    occipital_thalamic_bridge_t* bridge,
    const void* form_data,
    float form_strength
);

/**
 * @brief Advanced routing with request/response
 *
 * @param bridge Bridge handle
 * @param request Routing request
 * @param response Output routing response
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_route_advanced(
    occipital_thalamic_bridge_t* bridge,
    const occipital_thalamic_request_t* request,
    occipital_thalamic_response_t* response
);

/*=============================================================================
 * Attention and State API
 *===========================================================================*/

/**
 * @brief Set global attention level
 *
 * @param bridge Bridge handle
 * @param attention Attention level [0-1]
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_set_attention(
    occipital_thalamic_bridge_t* bridge,
    float attention
);

/**
 * @brief Set spatial attention location
 *
 * @param bridge Bridge handle
 * @param x Attended location X [0-1]
 * @param y Attended location Y [0-1]
 * @param radius Attention spotlight radius [0-1]
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_set_spatial_attention(
    occipital_thalamic_bridge_t* bridge,
    float x,
    float y,
    float radius
);

/**
 * @brief Get current attention level
 *
 * @param bridge Bridge handle
 * @param attention Output attention value
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_get_attention(
    const occipital_thalamic_bridge_t* bridge,
    float* attention
);

/**
 * @brief Set nucleus-specific gain
 *
 * @param bridge Bridge handle
 * @param nucleus Target nucleus
 * @param gain Gain value [0-2]
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_set_nucleus_gain(
    occipital_thalamic_bridge_t* bridge,
    occipital_thalamic_nucleus_t nucleus,
    float gain
);

/**
 * @brief Get current thalamic state
 *
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_get_state(
    const occipital_thalamic_bridge_t* bridge,
    occipital_thalamic_state_t* state
);

/**
 * @brief Apply cortical feedback
 *
 * V1 sends feedback to LGN to modulate incoming signals.
 *
 * @param bridge Bridge handle
 * @param feedback_signal Feedback signal
 * @param signal_dim Signal dimension
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_apply_feedback(
    occipital_thalamic_bridge_t* bridge,
    const float* feedback_signal,
    uint32_t signal_dim
);

/*=============================================================================
 * Bio-Async Communication
 *===========================================================================*/

/**
 * @brief Register with bio-async router
 *
 * Registers handlers for:
 * - BIO_MSG_ATTENTION_MODULATE: Attention changes
 * - BIO_MSG_VISUAL_INPUT: Visual input from LGN
 * - BIO_MSG_SACCADE_COMMAND: Eye movement signals
 *
 * @param bridge Bridge handle
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_bridge_register_bio_async(
    occipital_thalamic_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Broadcast visual routing event
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_bridge_broadcast_routing(
    occipital_thalamic_bridge_t* bridge
);

/**
 * @brief Process pending bio-async messages
 *
 * @param bridge Bridge handle
 * @param max_messages Max messages to process (0 = all)
 * @return Number processed, -1 on error
 */
int occipital_thalamic_bridge_process_messages(
    occipital_thalamic_bridge_t* bridge,
    uint32_t max_messages
);

/*=============================================================================
 * Statistics API
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_bridge_get_stats(
    const occipital_thalamic_bridge_t* bridge,
    occipital_thalamic_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void occipital_thalamic_bridge_reset_stats(occipital_thalamic_bridge_t* bridge);

/*=============================================================================
 * Query API
 *===========================================================================*/

/**
 * @brief Check if LGN is actively routing
 *
 * @param bridge Bridge handle
 * @return true if LGN gain > 0
 */
bool occipital_thalamic_is_lgn_active(
    const occipital_thalamic_bridge_t* bridge
);

/**
 * @brief Get configuration
 *
 * @param bridge Bridge handle
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int occipital_thalamic_bridge_get_config(
    const occipital_thalamic_bridge_t* bridge,
    occipital_thalamic_config_t* config
);

/**
 * @brief Get nucleus name string
 *
 * @param nucleus Nucleus type
 * @return Human-readable name
 */
const char* occipital_thalamic_nucleus_name(occipital_thalamic_nucleus_t nucleus);

/**
 * @brief Get pathway name string
 *
 * @param pathway LGN pathway type
 * @return Human-readable name
 */
const char* occipital_thalamic_pathway_name(lgn_pathway_type_t pathway);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OCCIPITAL_THALAMIC_BRIDGE_H */
