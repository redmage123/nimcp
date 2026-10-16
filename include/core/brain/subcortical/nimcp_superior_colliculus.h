//=============================================================================
// nimcp_superior_colliculus.h - Superior Colliculus for Gaze/Orienting
//=============================================================================
/**
 * @file nimcp_superior_colliculus.h
 * @brief Superior colliculus implementation for orienting and gaze control
 *
 * BIOLOGICAL BASIS:
 * The superior colliculus (SC) receives BG output (from SNr) for:
 * - Saccadic eye movements
 * - Orienting responses
 * - Attention shifts
 * - Multi-sensory integration
 *
 * PATHWAYS:
 * - SNr → SC: Disinhibition enables saccades
 * - SC → brainstem: Eye movement commands
 * - SC → thalamus → cortex: Corollary discharge
 *
 * LAYERS:
 * - Superficial: Visual input (retinotopic map)
 * - Intermediate: Motor map (saccade vectors)
 * - Deep: Multimodal integration
 *
 * @version 1.0.0
 * @date 2025-12-30
 */

#ifndef NIMCP_SUPERIOR_COLLICULUS_H
#define NIMCP_SUPERIOR_COLLICULUS_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define SC_MAP_WIDTH            64      /**< Collicular map width */
#define SC_MAP_HEIGHT           64      /**< Collicular map height */
#define SC_MAX_SACCADE_DEG      45.0f   /**< Maximum saccade amplitude */
#define SC_FIXATION_ZONE_DEG    3.0f    /**< Central fixation zone */

/** Timing parameters */
#define SC_SACCADE_LATENCY_MS   150.0f  /**< Typical saccade latency */
#define SC_EXPRESS_LATENCY_MS   80.0f   /**< Express saccade latency */
#define SC_FIXATION_DURATION_MS 200.0f  /**< Minimum fixation time */

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================ */

/**
 * @brief SC layer
 */
typedef enum {
    SC_LAYER_SUPERFICIAL,           /**< Visual input */
    SC_LAYER_INTERMEDIATE,          /**< Motor output */
    SC_LAYER_DEEP,                  /**< Multimodal */
    SC_LAYER_COUNT
} sc_layer_t;

/**
 * @brief Saccade type
 */
typedef enum {
    SC_SACCADE_NONE,                /**< No saccade */
    SC_SACCADE_VOLUNTARY,           /**< Goal-directed */
    SC_SACCADE_REFLEXIVE,           /**< Stimulus-driven */
    SC_SACCADE_EXPRESS,             /**< Ultra-fast reflexive */
    SC_SACCADE_ANTISACCADE,         /**< Opposite to stimulus */
    SC_SACCADE_MEMORY,              /**< To remembered location */
    SC_SACCADE_COUNT
} sc_saccade_type_t;

/**
 * @brief Fixation state
 */
typedef enum {
    SC_FIXATION_ACTIVE,             /**< Currently fixating */
    SC_FIXATION_PREPARING,          /**< Preparing saccade */
    SC_FIXATION_EXECUTING,          /**< Saccade in progress */
    SC_FIXATION_LANDING,            /**< Post-saccadic */
    SC_FIXATION_COUNT
} sc_fixation_state_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief 2D position
 */
typedef struct {
    float x;                        /**< Horizontal position (deg) */
    float y;                        /**< Vertical position (deg) */
} sc_position_t;

/**
 * @brief Saccade command
 */
typedef struct {
    sc_position_t target;           /**< Target position */
    sc_position_t start;            /**< Starting position */
    float amplitude;                /**< Saccade amplitude (deg) */
    float direction;                /**< Saccade direction (rad) */
    float velocity;                 /**< Peak velocity (deg/s) */
    float duration;                 /**< Saccade duration (ms) */
    sc_saccade_type_t type;
    float confidence;               /**< Target confidence */
} sc_saccade_t;

/**
 * @brief Visual target
 */
typedef struct {
    sc_position_t position;
    float salience;                 /**< Visual salience */
    float priority;                 /**< Selection priority */
    uint32_t id;
    bool is_tracked;
} sc_target_t;

/**
 * @brief SC configuration
 */
typedef struct {
    uint32_t map_width;
    uint32_t map_height;

    float saccade_threshold;        /**< Activity threshold for saccade */
    float fixation_strength;        /**< Strength of fixation neurons */
    float snr_gain;                 /**< SNr input gain */

    float express_saccade_threshold;
    bool enable_antisaccades;
    bool enable_memory_saccades;

    float visual_decay;             /**< Visual activation decay */
    float motor_decay;              /**< Motor activation decay */
} sc_config_t;

/**
 * @brief SC statistics
 */
typedef struct {
    sc_fixation_state_t fixation_state;
    sc_position_t current_gaze;
    uint32_t saccade_count;
    float avg_saccade_latency;
    float avg_saccade_amplitude;
    uint32_t express_saccade_count;
    float fixation_stability;
} sc_stats_t;

/**
 * @brief Main SC handle
 */
typedef struct superior_colliculus superior_colliculus_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

void sc_default_config(sc_config_t* config);
superior_colliculus_t* sc_create(const sc_config_t* config);
void sc_destroy(superior_colliculus_t* sc);
int sc_reset(superior_colliculus_t* sc);

/* ============================================================================
 * VISUAL INPUT API
 * ============================================================================ */

/**
 * @brief Set visual input to superficial layer
 */
int sc_set_visual_input(superior_colliculus_t* sc,
                         const float* visual_map,
                         uint32_t width,
                         uint32_t height);

/**
 * @brief Add visual target
 */
int sc_add_target(superior_colliculus_t* sc,
                   const sc_target_t* target);

/**
 * @brief Update target position
 */
int sc_update_target(superior_colliculus_t* sc,
                      uint32_t target_id,
                      const sc_position_t* position);

/**
 * @brief Remove target
 */
int sc_remove_target(superior_colliculus_t* sc, uint32_t target_id);

/* ============================================================================
 * SNr INPUT API (Basal Ganglia)
 * ============================================================================ */

/**
 * @brief Receive SNr inhibitory input
 * @param sc SC handle
 * @param snr_output SNr activity map (inhibitory)
 * @param width Map width
 * @param height Map height
 */
int sc_receive_snr_input(superior_colliculus_t* sc,
                          const float* snr_output,
                          uint32_t width,
                          uint32_t height);

/**
 * @brief Set SNr disinhibition for specific target
 */
int sc_set_snr_disinhibition(superior_colliculus_t* sc,
                              const sc_position_t* target,
                              float disinhibition);

/* ============================================================================
 * SACCADE CONTROL API
 * ============================================================================ */

/**
 * @brief Check if saccade is ready
 */
bool sc_is_saccade_ready(const superior_colliculus_t* sc);

/**
 * @brief Get pending saccade command
 */
int sc_get_saccade(const superior_colliculus_t* sc, sc_saccade_t* saccade);

/**
 * @brief Execute saccade (move gaze)
 */
int sc_execute_saccade(superior_colliculus_t* sc);

/**
 * @brief Cancel pending saccade
 */
int sc_cancel_saccade(superior_colliculus_t* sc);

/**
 * @brief Command voluntary saccade to target
 */
int sc_command_saccade(superior_colliculus_t* sc,
                        const sc_position_t* target,
                        sc_saccade_type_t type);

/**
 * @brief Get current gaze position
 */
int sc_get_gaze(const superior_colliculus_t* sc, sc_position_t* gaze);

/* ============================================================================
 * FIXATION API
 * ============================================================================ */

/**
 * @brief Strengthen fixation (suppress saccades)
 */
int sc_strengthen_fixation(superior_colliculus_t* sc, float strength);

/**
 * @brief Release fixation (allow saccades)
 */
int sc_release_fixation(superior_colliculus_t* sc);

/**
 * @brief Get fixation state
 */
sc_fixation_state_t sc_get_fixation_state(const superior_colliculus_t* sc);

/* ============================================================================
 * PROCESSING API
 * ============================================================================ */

/**
 * @brief Step SC dynamics
 */
int sc_step(superior_colliculus_t* sc, float dt_ms);

/**
 * @brief Get motor layer activity
 */
int sc_get_motor_map(const superior_colliculus_t* sc,
                      float* motor_map,
                      uint32_t* width,
                      uint32_t* height);

/**
 * @brief Get statistics
 */
int sc_get_stats(const superior_colliculus_t* sc, sc_stats_t* stats);

/* ============================================================================
 * COROLLARY DISCHARGE API
 * ============================================================================ */

/**
 * @brief Get corollary discharge (efference copy) of saccade
 */
int sc_get_corollary_discharge(const superior_colliculus_t* sc,
                                sc_saccade_t* cd);

/**
 * @brief Get predicted post-saccadic position
 */
int sc_get_predicted_gaze(const superior_colliculus_t* sc,
                           sc_position_t* predicted);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SUPERIOR_COLLICULUS_H */
