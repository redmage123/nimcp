//=============================================================================
// nimcp_pink_noise_sleep.h - Sleep/Wake Pink Noise Integration
//=============================================================================
/**
 * @file nimcp_pink_noise_sleep.h
 * @brief Pink noise modulation based on sleep/wake and arousal states
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Modulate pink noise characteristics based on sleep stages and arousal
 * WHY:  Neural noise profiles vary dramatically across sleep/wake states:
 *       - Wake: Low amplitude, α≈1.0 (pink), high precision
 *       - N1: Moderate amplitude, α≈0.9, reduced precision
 *       - N2: Higher amplitude, spindles add bursts
 *       - N3: Highest amplitude, α≈1.5 (redder), slow oscillations
 *       - REM: Variable, dream-like bursts, α≈0.8 (whiter)
 *
 * HOW:  Sleep stage determines base parameters; arousal modulates amplitude.
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Slow wave sleep: 1/f^β where β≈1.5 (redder spectrum)
 * - REM sleep: More variable, closer to white noise during dreams
 * - Awake: Optimal 1/f pink noise for information processing
 * - Sleep spindles: 11-16 Hz bursts during N2
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PINK_NOISE_SLEEP_H
#define NIMCP_PINK_NOISE_SLEEP_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "plasticity/noise/nimcp_pink_noise.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Sleep Stage Types
//=============================================================================

typedef enum {
    PINK_SLEEP_WAKE = 0,        /**< Fully awake */
    PINK_SLEEP_DROWSY,          /**< Transition to sleep */
    PINK_SLEEP_N1,              /**< Light sleep stage 1 */
    PINK_SLEEP_N2,              /**< Light sleep stage 2 (spindles) */
    PINK_SLEEP_N3,              /**< Deep slow-wave sleep */
    PINK_SLEEP_REM,             /**< REM/dream sleep */
    PINK_SLEEP_COUNT
} pink_sleep_stage_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    float amplitude;            /**< Noise amplitude for this stage */
    float alpha;                /**< Spectral exponent */
    float spindle_probability;  /**< Probability of spindle burst (N2) */
    float spindle_frequency;    /**< Spindle frequency Hz (11-16) */
} pink_sleep_stage_params_t;

typedef struct {
    pink_sleep_stage_params_t stages[PINK_SLEEP_COUNT];
    float arousal_amplitude_gain;   /**< How arousal affects amplitude */
    float transition_rate;          /**< Rate of parameter transitions */
    float sample_rate;
    uint32_t seed;
} pink_sleep_config_t;

//=============================================================================
// State Structure
//=============================================================================

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    pink_sleep_config_t config;
    pink_noise_generator_t noise_generator;
    pink_sleep_stage_t current_stage;
    float arousal_level;            /**< 0=deep sleep, 1=alert */
    float current_amplitude;
    float current_alpha;
    float spindle_phase;
    bool in_spindle;
    uint64_t sample_count;
    uint64_t stage_duration;
} pink_sleep_bridge_t;

//=============================================================================
// API Functions
//=============================================================================

pink_sleep_config_t pink_sleep_default_config(void);
pink_sleep_bridge_t* pink_sleep_create(const pink_sleep_config_t* config);
void pink_sleep_destroy(pink_sleep_bridge_t* bridge);

int pink_sleep_set_stage(pink_sleep_bridge_t* bridge, pink_sleep_stage_t stage);
int pink_sleep_set_arousal(pink_sleep_bridge_t* bridge, float arousal);
int pink_sleep_step(pink_sleep_bridge_t* bridge);

float pink_sleep_get_amplitude(const pink_sleep_bridge_t* bridge);
float pink_sleep_get_alpha(const pink_sleep_bridge_t* bridge);
float pink_sleep_generate_sample(pink_sleep_bridge_t* bridge);

const char* pink_sleep_stage_name(pink_sleep_stage_t stage);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PINK_NOISE_SLEEP_H
