/**
 * @file nimcp_vae_introspection_bridge.h
 * @brief Bridge between VAE and Introspection System for Internal State Encoding
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integrates VAE with introspection for internal state representation
 *
 * WHY:  VAE provides compressed internal state representation:
 *       - Brain state snapshots as VAE training data
 *       - Internal state compression via latent encoding
 *       - Uncertainty estimation from latent variance
 *       - Active pattern encoding for self-awareness
 *       - Temporal dynamics in latent space
 *
 * HOW:  Bridge maps introspection to VAE operations:
 *       - Neuron populations → latent encoding
 *       - Brain state → compressed representation
 *       - Latent variance → uncertainty estimate
 *       - Pattern activity → latent dimensions
 *
 * INTROSPECTION TARGETS:
 * ======================
 * - Active neuron populations
 * - Module activity levels
 * - Resource utilization
 * - Processing bottlenecks
 * - Attention focus
 * - Cognitive load
 *
 * BIO_MODULE: 0x1F19 (VAE-Introspection Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_INTROSPECTION_BRIDGE_H
#define NIMCP_VAE_INTROSPECTION_BRIDGE_H

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

#define VAE_INTRO_BRIDGE_VERSION        "1.0.0"
#define BIO_MODULE_VAE_INTRO_BRIDGE     0x1F19

/** Maximum modules to track */
#define VAE_INTRO_MAX_MODULES           64

/** Maximum patterns to track */
#define VAE_INTRO_MAX_PATTERNS          128

/** Error code range (32500-32509) */
#define NIMCP_ERROR_VAE_INTRO_BASE          32500
#define NIMCP_ERROR_VAE_INTRO_NULL          32501
#define NIMCP_ERROR_VAE_INTRO_NOT_CONNECTED 32502
#define NIMCP_ERROR_VAE_INTRO_ENCODE_FAILED 32503
#define NIMCP_ERROR_VAE_INTRO_NO_MEMORY     32504
#define NIMCP_ERROR_VAE_INTRO_SAMPLE_FAILED 32505

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief State extraction strategy (maps to introspection modes)
 */
typedef enum {
    VAE_INTRO_STRATEGY_FAST = 0,     /**< Fast sampling (~10%) */
    VAE_INTRO_STRATEGY_BALANCED,      /**< Balanced (~30%) */
    VAE_INTRO_STRATEGY_DETAILED       /**< Full scan (~100%) */
} vae_intro_strategy_t;

/**
 * @brief Introspection focus area
 */
typedef enum {
    VAE_INTRO_FOCUS_GLOBAL = 0,      /**< Whole brain state */
    VAE_INTRO_FOCUS_COGNITIVE,        /**< Cognitive modules only */
    VAE_INTRO_FOCUS_PERCEPTUAL,       /**< Perceptual processing */
    VAE_INTRO_FOCUS_MOTOR,            /**< Motor preparation */
    VAE_INTRO_FOCUS_EMOTIONAL,        /**< Emotional state */
    VAE_INTRO_FOCUS_MEMORY            /**< Memory systems */
} vae_intro_focus_t;

/**
 * @brief Bridge state
 */
typedef enum {
    VAE_INTRO_STATE_DISCONNECTED = 0,
    VAE_INTRO_STATE_CONNECTED,
    VAE_INTRO_STATE_SAMPLING,
    VAE_INTRO_STATE_ENCODING,
    VAE_INTRO_STATE_ERROR
} vae_intro_bridge_state_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Module activity representation
 */
typedef struct {
    uint32_t module_id;
    const char* module_name;
    float activity_level;             /**< [0, 1] */
    float resource_usage;             /**< [0, 1] */
    uint32_t active_neurons;
    float avg_firing_rate;
} vae_intro_module_state_t;

/**
 * @brief Pattern activity representation
 */
typedef struct {
    uint32_t pattern_id;
    const char* pattern_name;
    float activation;                 /**< [0, 1] */
    bool is_active;
    uint64_t last_active_time_us;
} vae_intro_pattern_state_t;

/**
 * @brief Brain state snapshot
 */
typedef struct {
    /* Global metrics */
    float global_activity;            /**< Overall brain activity */
    float cognitive_load;             /**< Cognitive processing load */
    float attention_level;            /**< Attention focus strength */
    float arousal_level;              /**< Arousal/alertness */

    /* Uncertainty estimates */
    float epistemic_uncertainty;      /**< Model uncertainty */
    float aleatoric_uncertainty;      /**< Data uncertainty */

    /* Module states */
    vae_intro_module_state_t* modules;
    uint32_t num_modules;

    /* Active patterns */
    vae_intro_pattern_state_t* patterns;
    uint32_t num_patterns;

    /* Timing */
    uint64_t snapshot_time_us;
    float sampling_duration_us;
} vae_intro_brain_state_t;

/**
 * @brief Main bridge configuration
 */
typedef struct {
    vae_intro_strategy_t default_strategy;
    vae_intro_focus_t default_focus;

    /* Sampling parameters */
    float sampling_rate;              /**< How often to sample (Hz) */
    uint32_t max_neurons_sampled;
    bool track_temporal_dynamics;

    /* Latent mapping */
    uint32_t module_latent_dim;       /**< Latent dims per module */
    uint32_t pattern_latent_dim;      /**< Latent dims for patterns */
    uint32_t global_latent_dim;       /**< Global state latent dims */

    /* Uncertainty */
    bool compute_uncertainty;
    float uncertainty_threshold;

    /* History */
    bool enable_history;
    uint32_t history_length;

    bool enable_logging;
} vae_intro_bridge_config_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

typedef struct {
    float* latent;                    /**< Encoded brain state */
    uint32_t latent_dim;
    float* variance;                  /**< Latent variance → uncertainty */
    float estimated_uncertainty;      /**< Overall uncertainty estimate */
    vae_intro_brain_state_t state;    /**< Original brain state */
    uint64_t encoding_time_us;
} vae_intro_encode_result_t;

typedef struct {
    vae_intro_brain_state_t predicted_state;
    float* latent_trajectory;         /**< Latent states over time */
    uint32_t trajectory_length;
    float prediction_confidence;
    uint64_t prediction_time_us;
} vae_intro_predict_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t total_samples;
    uint64_t total_encodes;
    float avg_global_activity;
    float avg_cognitive_load;
    float avg_uncertainty;
    float avg_encoding_latency_us;
    uint64_t creation_time_us;
    uint64_t last_sample_us;
} vae_intro_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

typedef struct vae_intro_bridge {
    vae_intro_bridge_config_t config;
    vae_system_t* vae;
    void* introspection_ctx;
    vae_intro_bridge_state_t state;
    bool is_initialized;

    /* Current state */
    vae_intro_brain_state_t current_state;
    float* current_latent;
    uint32_t latent_dim;

    /* State history */
    float** latent_history;
    uint32_t history_head;
    uint32_t history_count;

    /* Working buffers */
    float* encode_buffer;
    float* variance_buffer;

    /* Statistics */
    vae_intro_bridge_stats_t stats;
    uint64_t creation_time_us;
} vae_intro_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_intro_bridge_default_config(vae_intro_bridge_config_t* config);
vae_intro_bridge_t* vae_intro_bridge_create(const vae_intro_bridge_config_t* config);
void vae_intro_bridge_destroy(vae_intro_bridge_t* bridge);
int vae_intro_bridge_connect_vae(vae_intro_bridge_t* bridge, vae_system_t* vae);
int vae_intro_bridge_connect_introspection(vae_intro_bridge_t* bridge, void* introspection);
int vae_intro_bridge_disconnect(vae_intro_bridge_t* bridge);
bool vae_intro_bridge_is_connected(const vae_intro_bridge_t* bridge);

/* ============================================================================
 * Sampling API
 * ============================================================================ */

int vae_intro_sample_state(vae_intro_bridge_t* bridge,
                            vae_intro_strategy_t strategy,
                            vae_intro_focus_t focus,
                            vae_intro_brain_state_t* state);

int vae_intro_sample_current(vae_intro_bridge_t* bridge);

/* ============================================================================
 * Encoding API
 * ============================================================================ */

int vae_intro_encode_state(vae_intro_bridge_t* bridge,
                            const vae_intro_brain_state_t* state,
                            vae_intro_encode_result_t* result);

int vae_intro_encode_current(vae_intro_bridge_t* bridge,
                              vae_intro_encode_result_t* result);

int vae_intro_encode_modules(vae_intro_bridge_t* bridge,
                              const vae_intro_module_state_t* modules,
                              uint32_t num_modules,
                              float* latent, uint32_t* latent_dim);

/* ============================================================================
 * Uncertainty API
 * ============================================================================ */

int vae_intro_compute_uncertainty(vae_intro_bridge_t* bridge,
                                   float* epistemic, float* aleatoric);

int vae_intro_get_uncertainty_from_latent(vae_intro_bridge_t* bridge,
                                           const float* variance,
                                           uint32_t dim,
                                           float* uncertainty);

/* ============================================================================
 * Prediction API
 * ============================================================================ */

int vae_intro_predict_next_state(vae_intro_bridge_t* bridge,
                                  uint32_t steps_ahead,
                                  vae_intro_predict_result_t* result);

int vae_intro_extrapolate_trajectory(vae_intro_bridge_t* bridge,
                                      uint32_t num_steps,
                                      float* trajectory,
                                      uint32_t* trajectory_dim);

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_intro_bridge_state_t vae_intro_bridge_get_state(const vae_intro_bridge_t* bridge);
int vae_intro_bridge_get_stats(const vae_intro_bridge_t* bridge,
                                vae_intro_bridge_stats_t* stats);
int vae_intro_get_current_latent(const vae_intro_bridge_t* bridge,
                                  float* latent, uint32_t* dim);

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_intro_encode_result_free(vae_intro_encode_result_t* result);
void vae_intro_predict_result_free(vae_intro_predict_result_t* result);
void vae_intro_brain_state_free(vae_intro_brain_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_INTROSPECTION_BRIDGE_H */
