/**
 * @file nimcp_vae_thalamic_bridge.h
 * @brief Bridge between VAE and Thalamic System for Attention-Gated Encoding
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integrates VAE with thalamic relay for attention-modulated processing
 *
 * WHY:  Thalamus-VAE integration enables:
 *       - Attention-gated sensory encoding
 *       - Thalamic burst mode → VAE exploration sampling
 *       - TRN inhibition → VAE output filtering
 *       - Multi-modal routing through thalamic nuclei
 *       - Cortical feedback → VAE top-down modulation
 *
 * HOW:  Bridge coordinates thalamic relay with VAE:
 *       - Attention weights modulate VAE encoder focus
 *       - Firing mode (tonic/burst) affects sampling strategy
 *       - TRN inhibition gates VAE outputs
 *       - Nucleus-specific routing for multimodal VAE
 *
 * THALAMIC NUCLEI:
 * ===============
 * - LGN: Visual relay (retina → V1)
 * - MGN: Auditory relay (IC → A1)
 * - VPL/VPM: Somatosensory relay
 * - Pulvinar: Attention modulation
 * - TRN: Inhibitory gating
 *
 * BIO_MODULE: 0x1F1F (VAE-Thalamic Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_THALAMIC_BRIDGE_H
#define NIMCP_VAE_THALAMIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/vae/nimcp_vae.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define VAE_THALAMIC_BRIDGE_VERSION    "1.0.0"
#define BIO_MODULE_VAE_THALAMIC_BRIDGE 0x1F1F

/** Maximum channels per nucleus */
#define VAE_THALAMIC_MAX_CHANNELS      256

/** Maximum nuclei */
#define VAE_THALAMIC_MAX_NUCLEI        16

/** Default burst threshold */
#define VAE_THALAMIC_BURST_THRESHOLD   0.3f

/** TRN inhibition strength */
#define VAE_THALAMIC_TRN_STRENGTH      0.7f

/** Error code range (32560-32569) */
#define NIMCP_ERROR_VAE_THAL_BASE          32560
#define NIMCP_ERROR_VAE_THAL_NULL          32561
#define NIMCP_ERROR_VAE_THAL_NOT_CONNECTED 32562
#define NIMCP_ERROR_VAE_THAL_RELAY_FAILED  32563
#define NIMCP_ERROR_VAE_THAL_GATING_FAILED 32564
#define NIMCP_ERROR_VAE_THAL_NO_MEMORY     32565
#define NIMCP_ERROR_VAE_THAL_INVALID_NUC   32566

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Thalamic nucleus types
 */
typedef enum {
    VAE_THAL_NUC_LGN = 0,         /**< Lateral Geniculate (visual) */
    VAE_THAL_NUC_MGN,              /**< Medial Geniculate (auditory) */
    VAE_THAL_NUC_VPL,              /**< Ventral Posterior Lateral (body) */
    VAE_THAL_NUC_VPM,              /**< Ventral Posterior Medial (face) */
    VAE_THAL_NUC_VA,               /**< Ventral Anterior (motor) */
    VAE_THAL_NUC_VL,               /**< Ventral Lateral (motor) */
    VAE_THAL_NUC_PULVINAR,         /**< Pulvinar (attention) */
    VAE_THAL_NUC_MD,               /**< Mediodorsal (executive) */
    VAE_THAL_NUC_ANTERIOR,         /**< Anterior (limbic/memory) */
    VAE_THAL_NUC_TRN,              /**< Reticular (inhibitory) */
    VAE_THAL_NUC_COUNT
} vae_thalamic_nucleus_t;

/**
 * @brief Thalamic firing mode
 */
typedef enum {
    VAE_THAL_MODE_TONIC = 0,      /**< Tonic - faithful relay */
    VAE_THAL_MODE_BURST,           /**< Burst - 2-7 spike bursts */
    VAE_THAL_MODE_INHIBITED        /**< TRN suppressed */
} vae_thalamic_mode_t;

/**
 * @brief Relay order
 */
typedef enum {
    VAE_THAL_FIRST_ORDER = 0,     /**< Sensory relay (subcortical → cortex) */
    VAE_THAL_HIGHER_ORDER          /**< Cortical relay (cortex → cortex) */
} vae_thalamic_relay_order_t;

/**
 * @brief Gating action
 */
typedef enum {
    VAE_THAL_GATE_PASS = 0,       /**< Pass through */
    VAE_THAL_GATE_SUPPRESS,        /**< Suppress output */
    VAE_THAL_GATE_AMPLIFY,         /**< Amplify output */
    VAE_THAL_GATE_MODULATE         /**< Multiplicative modulation */
} vae_thalamic_gate_action_t;

/**
 * @brief Bridge state
 */
typedef enum {
    VAE_THAL_STATE_DISCONNECTED = 0,
    VAE_THAL_STATE_CONNECTED,
    VAE_THAL_STATE_RELAYING,
    VAE_THAL_STATE_GATING,
    VAE_THAL_STATE_ERROR
} vae_thalamic_bridge_state_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Per-nucleus configuration
 */
typedef struct {
    vae_thalamic_nucleus_t nucleus;
    uint32_t num_channels;
    vae_thalamic_relay_order_t order;
    float burst_threshold;        /**< Threshold for burst mode */
    float attention_weight;       /**< Base attention weighting */
    float trn_inhibition;         /**< TRN inhibition strength */
    bool enable_adaptation;       /**< Firing rate adaptation */
    uint32_t latent_dim_start;    /**< First VAE latent dim for this nucleus */
    uint32_t latent_dim_count;    /**< Number of latent dims */
} vae_thalamic_nucleus_config_t;

/**
 * @brief Attention modulation configuration
 */
typedef struct {
    bool enable_attention_gating; /**< Gate VAE based on attention */
    float attention_min;          /**< Minimum attention level */
    float attention_max;          /**< Maximum attention level */
    float attention_decay;        /**< Decay rate for attention */
    bool pulvinar_modulation;     /**< Pulvinar provides attention signal */
} vae_thalamic_attention_config_t;

/**
 * @brief Burst mode configuration
 */
typedef struct {
    bool burst_triggers_exploration; /**< Burst mode → VAE exploration */
    float burst_temperature_scale;   /**< Sampling temp in burst mode */
    uint32_t burst_spike_count;      /**< Expected spikes per burst */
    float t_channel_recovery_ms;     /**< T-type Ca2+ recovery time */
} vae_thalamic_burst_config_t;

/**
 * @brief TRN gating configuration
 */
typedef struct {
    bool enable_trn_gating;       /**< TRN can gate VAE output */
    float trn_threshold;          /**< TRN activity threshold for gating */
    float gating_slope;           /**< Sigmoid slope for gating */
    bool lateral_inhibition;      /**< Enable lateral inhibition via TRN */
} vae_thalamic_trn_config_t;

/**
 * @brief Main bridge configuration
 */
typedef struct {
    /* Nucleus configurations */
    vae_thalamic_nucleus_config_t nuclei[VAE_THAL_NUC_COUNT];
    uint32_t active_nuclei_mask;  /**< Bitmask of active nuclei */

    /* Modulation configs */
    vae_thalamic_attention_config_t attention;
    vae_thalamic_burst_config_t burst;
    vae_thalamic_trn_config_t trn;

    /* Routing */
    bool enable_multimodal_routing;
    bool enable_cortical_feedback;
    float feedback_strength;

    /* Timing */
    float relay_latency_ms;       /**< Thalamic relay latency */
    float update_interval_ms;

    bool enable_logging;
} vae_thalamic_bridge_config_t;

/* ============================================================================
 * State Structures
 * ============================================================================ */

/**
 * @brief Per-nucleus state
 */
typedef struct {
    vae_thalamic_nucleus_t nucleus;
    vae_thalamic_mode_t mode;
    float current_attention;
    float avg_firing_rate_hz;
    float t_channel_state;        /**< T-type Ca2+ channel [0-1] */
    bool is_bursting;
    uint32_t burst_count;
    float trn_inhibition_level;
    uint64_t last_spike_us;
} vae_thalamic_nucleus_state_t;

/**
 * @brief Global thalamic state
 */
typedef struct {
    vae_thalamic_nucleus_state_t nuclei[VAE_THAL_NUC_COUNT];
    float global_attention;       /**< Overall attention level */
    float global_arousal;         /**< Arousal/alertness */
    vae_thalamic_mode_t dominant_mode;
    bool trn_active;
    uint64_t state_time_us;
} vae_thalamic_state_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Relay result
 */
typedef struct {
    float* relayed_latent;        /**< Gated/modulated latent */
    uint32_t latent_dim;
    float* attention_weights;     /**< Per-dimension attention */
    float* gate_values;           /**< Per-dimension gate [0-1] */
    vae_thalamic_mode_t mode_used;
    float effective_gain;
    uint64_t relay_time_us;
} vae_thalamic_relay_result_t;

/**
 * @brief Routing result for multimodal
 */
typedef struct {
    float* visual_latent;         /**< LGN-routed visual */
    float* auditory_latent;       /**< MGN-routed auditory */
    float* somatosensory_latent;  /**< VPL/VPM-routed touch */
    uint32_t visual_dim;
    uint32_t auditory_dim;
    uint32_t somato_dim;
    float* modality_weights;      /**< Attention per modality */
    uint64_t routing_time_us;
} vae_thalamic_routing_result_t;

/**
 * @brief Gating decision result
 */
typedef struct {
    vae_thalamic_gate_action_t action;
    float gate_strength;          /**< [0-1] gating strength */
    vae_thalamic_nucleus_t gating_source;
    float trn_contribution;
    char reason[64];              /**< Human-readable reason */
} vae_thalamic_gating_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t total_relays;
    uint64_t total_bursts;
    uint64_t total_gatings;
    uint64_t trn_suppressions;
    float avg_attention_level;
    float per_nucleus_activity[VAE_THAL_NUC_COUNT];
    float time_in_burst_mode_pct;
    float time_in_tonic_mode_pct;
    uint64_t creation_time_us;
    uint64_t last_relay_us;
} vae_thalamic_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

typedef struct vae_thalamic_bridge {
    vae_thalamic_bridge_config_t config;
    vae_system_t* vae;
    void* thalamus;               /**< Thalamus system context */
    vae_thalamic_bridge_state_t state;
    bool is_initialized;

    /* Current thalamic state */
    vae_thalamic_state_t thalamic_state;

    /* Per-nucleus buffers */
    float* nucleus_latents[VAE_THAL_NUC_COUNT];
    float* nucleus_gates[VAE_THAL_NUC_COUNT];

    /* Attention state */
    float* attention_weights;
    float global_attention;

    /* TRN state */
    float* trn_inhibition;
    uint32_t trn_dim;

    /* Cortical feedback */
    float* feedback_buffer;
    uint32_t feedback_dim;

    /* Working buffers */
    float* relay_buffer;
    float* gate_buffer;

    /* Statistics */
    vae_thalamic_bridge_stats_t stats;
    uint64_t creation_time_us;
} vae_thalamic_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_thalamic_bridge_default_config(vae_thalamic_bridge_config_t* config);
vae_thalamic_bridge_t* vae_thalamic_bridge_create(const vae_thalamic_bridge_config_t* config);
void vae_thalamic_bridge_destroy(vae_thalamic_bridge_t* bridge);
int vae_thalamic_bridge_connect_vae(vae_thalamic_bridge_t* bridge, vae_system_t* vae);
int vae_thalamic_bridge_connect_thalamus(vae_thalamic_bridge_t* bridge, void* thalamus);
int vae_thalamic_bridge_disconnect(vae_thalamic_bridge_t* bridge);
bool vae_thalamic_bridge_is_connected(const vae_thalamic_bridge_t* bridge);

/* ============================================================================
 * Relay API
 * ============================================================================ */

int vae_thalamic_relay(vae_thalamic_bridge_t* bridge,
                        vae_thalamic_nucleus_t nucleus,
                        const float* input_latent, uint32_t latent_dim,
                        vae_thalamic_relay_result_t* result);

int vae_thalamic_relay_multimodal(vae_thalamic_bridge_t* bridge,
                                   const float* visual, uint32_t vis_dim,
                                   const float* auditory, uint32_t aud_dim,
                                   const float* somatosensory, uint32_t soma_dim,
                                   vae_thalamic_routing_result_t* result);

int vae_thalamic_relay_with_feedback(vae_thalamic_bridge_t* bridge,
                                      vae_thalamic_nucleus_t nucleus,
                                      const float* input_latent, uint32_t latent_dim,
                                      const float* cortical_feedback, uint32_t fb_dim,
                                      vae_thalamic_relay_result_t* result);

/* ============================================================================
 * Attention API
 * ============================================================================ */

int vae_thalamic_set_attention(vae_thalamic_bridge_t* bridge,
                                vae_thalamic_nucleus_t nucleus,
                                float attention_level);

int vae_thalamic_set_global_attention(vae_thalamic_bridge_t* bridge,
                                       float attention_level);

int vae_thalamic_update_attention_from_pulvinar(vae_thalamic_bridge_t* bridge);

float vae_thalamic_get_attention(const vae_thalamic_bridge_t* bridge,
                                  vae_thalamic_nucleus_t nucleus);

/* ============================================================================
 * Gating API
 * ============================================================================ */

int vae_thalamic_compute_gating(vae_thalamic_bridge_t* bridge,
                                 vae_thalamic_nucleus_t nucleus,
                                 vae_thalamic_gating_result_t* result);

int vae_thalamic_apply_trn_inhibition(vae_thalamic_bridge_t* bridge,
                                       float* latent, uint32_t dim);

int vae_thalamic_set_trn_activity(vae_thalamic_bridge_t* bridge,
                                   const float* trn_activity, uint32_t dim);

/* ============================================================================
 * Mode API
 * ============================================================================ */

int vae_thalamic_set_mode(vae_thalamic_bridge_t* bridge,
                           vae_thalamic_nucleus_t nucleus,
                           vae_thalamic_mode_t mode);

vae_thalamic_mode_t vae_thalamic_get_mode(const vae_thalamic_bridge_t* bridge,
                                           vae_thalamic_nucleus_t nucleus);

int vae_thalamic_trigger_burst(vae_thalamic_bridge_t* bridge,
                                vae_thalamic_nucleus_t nucleus);

bool vae_thalamic_is_bursting(const vae_thalamic_bridge_t* bridge,
                               vae_thalamic_nucleus_t nucleus);

/* ============================================================================
 * State API
 * ============================================================================ */

int vae_thalamic_update_state(vae_thalamic_bridge_t* bridge);

int vae_thalamic_get_state(const vae_thalamic_bridge_t* bridge,
                            vae_thalamic_state_t* state);

int vae_thalamic_get_nucleus_state(const vae_thalamic_bridge_t* bridge,
                                    vae_thalamic_nucleus_t nucleus,
                                    vae_thalamic_nucleus_state_t* state);

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_thalamic_bridge_state_t vae_thalamic_bridge_get_state(const vae_thalamic_bridge_t* bridge);
int vae_thalamic_bridge_get_stats(const vae_thalamic_bridge_t* bridge,
                                   vae_thalamic_bridge_stats_t* stats);
const char* vae_thalamic_nucleus_to_string(vae_thalamic_nucleus_t nucleus);
const char* vae_thalamic_mode_to_string(vae_thalamic_mode_t mode);

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_thalamic_relay_result_free(vae_thalamic_relay_result_t* result);
void vae_thalamic_routing_result_free(vae_thalamic_routing_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_THALAMIC_BRIDGE_H */
