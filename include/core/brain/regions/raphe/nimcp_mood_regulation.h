/**
 * @file nimcp_mood_regulation.h
 * @brief Mood regulation system for Raphe Nuclei
 * @date 2026-01-11
 *
 * Models the serotonergic contribution to mood regulation:
 * - Mood valence (positive/negative affect)
 * - Emotional stability (mood variability)
 * - Anxiety modulation
 * - Stress response
 * - Circadian mood variation
 */

#ifndef NIMCP_MOOD_REGULATION_H
#define NIMCP_MOOD_REGULATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * Constants
 *===========================================================================*/

#define MOOD_DEFAULT_NEUTRAL       0.0f     /* Neutral valence */
#define MOOD_DEFAULT_STABILITY     0.7f     /* Baseline stability */
#define MOOD_TIME_CONSTANT         10000.0f /* Mood change time constant (ms) */
#define ANXIETY_BASELINE           0.3f     /* Baseline anxiety */
#define MOOD_MAX_HISTORY           64       /* Mood history entries */

/*=============================================================================
 * Enumerations
 *===========================================================================*/

typedef enum {
    MOOD_TRIGGER_5HT = 0,           /**< Serotonin level change */
    MOOD_TRIGGER_STRESS,            /**< Stress input */
    MOOD_TRIGGER_REWARD,            /**< Reward/pleasure signal */
    MOOD_TRIGGER_SOCIAL,            /**< Social interaction */
    MOOD_TRIGGER_CIRCADIAN,         /**< Time-of-day effect */
    MOOD_TRIGGER_IMMUNE,            /**< Immune/inflammation */
    MOOD_TRIGGER_COUNT
} nimcp_mood_trigger_t;

/*=============================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief Mood regulation configuration
 */
typedef struct {
    float time_constant;            /**< Mood change rate (ms) */
    float stability_baseline;       /**< Default stability */
    float anxiety_baseline;         /**< Default anxiety */
    float stress_sensitivity;       /**< Stress -> mood impact */
    float reward_sensitivity;       /**< Reward -> mood impact */
    float ht_mood_gain;             /**< 5-HT -> mood gain */
    float circadian_amplitude;      /**< Circadian mood swing */
} nimcp_mood_config_t;

/**
 * @brief Mood state snapshot
 */
typedef struct {
    float valence;                  /**< Current mood [-1, +1] */
    float stability;                /**< Emotional stability [0-1] */
    float anxiety;                  /**< Anxiety level [0-1] */
    float irritability;             /**< Irritability [0-1] */
    float energy;                   /**< Psychomotor energy [0-1] */
    float timestamp;                /**< When recorded (ms) */
} nimcp_mood_snapshot_t;

/**
 * @brief Mood regulation system
 */
typedef struct {
    bool initialized;

    /* Current state */
    float valence;
    float stability;
    float anxiety;
    float irritability;
    float energy;

    /* Dynamics */
    float valence_velocity;         /**< Rate of mood change */
    float target_valence;           /**< Mood is moving toward */

    /* Inputs */
    float stress_input;
    float reward_input;
    float social_input;
    float circadian_phase;

    /* 5-HT state */
    float current_5ht;
    float baseline_5ht;

    /* History */
    nimcp_mood_snapshot_t history[MOOD_MAX_HISTORY];
    uint32_t history_count;
    uint32_t history_index;

    /* Statistics */
    float avg_valence;
    float valence_variance;
    float time_depressed;
    float time_positive;

    /* Configuration */
    nimcp_mood_config_t config;

} nimcp_mood_system_t;

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

int nimcp_mood_init(nimcp_mood_system_t* system, const nimcp_mood_config_t* config);
int nimcp_mood_shutdown(nimcp_mood_system_t* system);
int nimcp_mood_reset(nimcp_mood_system_t* system);
nimcp_mood_config_t nimcp_mood_default_config(void);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_mood_update(nimcp_mood_system_t* system, float ht_level, float dt);

/*=============================================================================
 * Input API
 *===========================================================================*/

int nimcp_mood_apply_stress(nimcp_mood_system_t* system, float stress);
int nimcp_mood_apply_reward(nimcp_mood_system_t* system, float reward);
int nimcp_mood_apply_social(nimcp_mood_system_t* system, float social_input);
int nimcp_mood_set_circadian_phase(nimcp_mood_system_t* system, float phase);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_mood_get_valence(nimcp_mood_system_t* system, float* valence);
int nimcp_mood_get_stability(nimcp_mood_system_t* system, float* stability);
int nimcp_mood_get_anxiety(nimcp_mood_system_t* system, float* anxiety);
int nimcp_mood_get_energy(nimcp_mood_system_t* system, float* energy);

int nimcp_mood_get_state(nimcp_mood_system_t* system, nimcp_mood_snapshot_t* state);

/*=============================================================================
 * Analysis API
 *===========================================================================*/

int nimcp_mood_get_trend(nimcp_mood_system_t* system, float* trend);
int nimcp_mood_get_variability(nimcp_mood_system_t* system, float* variability);
int nimcp_mood_is_depressed(nimcp_mood_system_t* system, bool* depressed);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MOOD_REGULATION_H */
