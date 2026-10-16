/**
 * @file nimcp_snn_love_bridge.h
 * @brief SNN-Love integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and attachment/bonding system
 * WHY:  Enable spike-based attachment processing and oxytocin-like patterns
 * HOW:  Convert love states to sustained patterns, track bonding events
 *
 * BIOLOGICAL BASIS:
 * - Oxytocin release promotes sustained neural synchrony in social brain regions
 * - Attachment encoded in persistent activity patterns in ventral pallidum
 * - Bonding involves long-lasting potentiation of reward circuits
 * - Prairie vole pair bonding shows sustained nucleus accumbens activity
 *
 * INTEGRATION:
 * - SNN → Love: Decode sustained activity into attachment strength
 * - Love → SNN: Generate oxytocin-like sustained population synchrony
 * - Bonding events tracked via persistent activity episodes
 * - Attachment modulates population coherence and coupling
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_LOVE_BRIDGE_H
#define NIMCP_SNN_LOVE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

/* Forward declaration */
typedef struct love_system_s love_system_t;

//=============================================================================
// Configuration Types
//=============================================================================

typedef struct snn_love_config_s {
    float attachment_threshold;        /**< Threshold for attachment detection */
    float bonding_strength_gain;       /**< Gain for bonding strength */
    float sustained_activity_min_ms;   /**< Min duration for sustained activity (ms) */
    float oxytocin_baseline;           /**< Baseline oxytocin-like activity */
    float oxytocin_bonding_peak;       /**< Peak oxytocin during bonding */
    float synchrony_threshold;         /**< Threshold for population synchrony */
    float coupling_strength;           /**< Coupling strength between populations */
    bool enable_bonding_tracking;      /**< Track bonding events */
    bool enable_synchrony_detection;   /**< Detect population synchrony */
    uint32_t attachment_pop_id;        /**< Population encoding attachment */
    uint32_t bonding_pop_id;           /**< Population encoding bonding state */
    float update_interval_ms;          /**< How often to sync */
    bool enable_bio_async;             /**< Enable bio-async messaging */
} snn_love_config_t;

typedef struct snn_sustained_activity_state_s {
    float duration_ms;                 /**< Duration of sustained activity (ms) */
    float amplitude;                   /**< Activity amplitude [0, 1] */
    float stability;                   /**< Activity stability [0, 1] */
    bool is_sustained;                 /**< Currently sustained */
} snn_sustained_activity_state_t;

typedef struct snn_love_state_s {
    float attachment_level;            /**< Current attachment level [0, 1] */
    uint32_t bonding_events_count;     /**< Total bonding events */
    snn_sustained_activity_state_t sustained; /**< Sustained activity state */
    float oxytocin_level;              /**< Current oxytocin-like signal [0, 1] */
    float population_synchrony;        /**< Population synchrony level [0, 1] */
    float bonding_strength;            /**< Current bonding strength [0, 1] */
    float coupling_factor;             /**< Population coupling factor */
    uint32_t sync_count;               /**< Number of syncs performed */
    float avg_attachment_level;        /**< Average attachment level */
    float avg_synchrony;               /**< Average population synchrony */
} snn_love_state_t;

typedef struct snn_love_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    love_system_t* love_system;
    snn_love_config_t config;
    snn_love_state_t state;
    snn_population_t* attachment_pop;
    snn_population_t* bonding_pop;
    snn_encoder_t* encoder;
    snn_decoder_t* decoder;
    float last_update_time;
    float sustained_start_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_love_bridge_t;

//=============================================================================
// API Functions
//=============================================================================

void snn_love_config_default(snn_love_config_t* config);

snn_love_bridge_t* snn_love_bridge_create(
    const snn_love_config_t* config,
    snn_network_t* snn,
    love_system_t* love_system
);

void snn_love_bridge_destroy(snn_love_bridge_t* bridge);

int snn_love_bridge_connect_bio_async(snn_love_bridge_t* bridge);
int snn_love_bridge_disconnect_bio_async(snn_love_bridge_t* bridge);
bool snn_love_bridge_is_bio_async_connected(const snn_love_bridge_t* bridge);

int snn_love_bridge_update(snn_love_bridge_t* bridge, float dt);

int snn_love_decode_from_spikes(
    snn_love_bridge_t* bridge,
    float* attachment_level_out,
    float* bonding_strength_out
);

int snn_love_detect_sustained_activity(
    snn_love_bridge_t* bridge,
    snn_sustained_activity_state_t* sustained_state
);

int snn_love_detect_synchrony(
    snn_love_bridge_t* bridge,
    float* synchrony_out
);

int snn_love_modulate_populations(snn_love_bridge_t* bridge);

int snn_love_encode_to_spikes(
    snn_love_bridge_t* bridge,
    float attachment_level,
    float bonding_strength
);

int snn_love_bridge_get_state(
    const snn_love_bridge_t* bridge,
    snn_love_state_t* state
);

float snn_love_get_attachment_level(const snn_love_bridge_t* bridge);
float snn_love_get_bonding_strength(const snn_love_bridge_t* bridge);
bool snn_love_is_sustained(const snn_love_bridge_t* bridge);

int snn_love_get_stats(
    const snn_love_bridge_t* bridge,
    uint32_t* sync_count,
    uint32_t* bonding_events_count,
    float* avg_attachment_level
);

void snn_love_reset_stats(snn_love_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_LOVE_BRIDGE_H */
