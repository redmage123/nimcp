/**
 * @file nimcp_oscillations_sleep_bridge.h
 * @brief Sleep-Oscillations Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake and brain oscillations
 * WHY:  Brain oscillations DEFINE sleep stages - this is the core sleep-brain interface
 * HOW:  Sleep state sets dominant oscillation frequency, power, and coupling
 *
 * BIOLOGICAL BASIS - Oscillations define sleep stages:
 * - AWAKE: Beta/Gamma (13-100 Hz) - active processing, binding
 * - DROWSY: Alpha (8-13 Hz) - relaxed, eyes closed
 * - LIGHT_NREM: Theta (4-8 Hz) + sleep spindles (12-14 Hz) + K-complexes
 * - DEEP_NREM: Delta (0.5-4 Hz) - slow wave sleep, consolidation
 * - REM: Theta + PGO waves - desynchronized, dream state
 *
 * Key oscillatory phenomena during sleep:
 * - Sleep spindles (12-14 Hz): Memory consolidation, thalamocortical
 * - Sharp wave ripples (150-200 Hz): Hippocampal replay
 * - Slow oscillations (< 1 Hz): Up/down states during NREM
 * - K-complexes: Large transients in NREM
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OSCILLATIONS_SLEEP_BRIDGE_H
#define NIMCP_OSCILLATIONS_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dominant frequency by sleep state (Hz) */
#define OSC_SLEEP_FREQ_AWAKE        25.0f   /* Beta/Gamma */
#define OSC_SLEEP_FREQ_DROWSY       10.0f   /* Alpha */
#define OSC_SLEEP_FREQ_LIGHT_NREM   6.0f    /* Theta + spindles */
#define OSC_SLEEP_FREQ_DEEP_NREM    2.0f    /* Delta */
#define OSC_SLEEP_FREQ_REM          6.0f    /* Theta */

/* Band power distribution (which band dominates) */
typedef enum {
    OSC_BAND_DELTA = 0,   /* 0.5-4 Hz */
    OSC_BAND_THETA,       /* 4-8 Hz */
    OSC_BAND_ALPHA,       /* 8-13 Hz */
    OSC_BAND_BETA,        /* 13-30 Hz */
    OSC_BAND_GAMMA        /* 30-100 Hz */
} oscillation_band_t;

/* Sleep spindle activity (NREM specific) */
#define OSC_SLEEP_SPINDLE_AWAKE     0.0f
#define OSC_SLEEP_SPINDLE_DROWSY    0.1f
#define OSC_SLEEP_SPINDLE_LIGHT     0.8f    /* Peak spindles */
#define OSC_SLEEP_SPINDLE_DEEP      0.4f
#define OSC_SLEEP_SPINDLE_REM       0.0f

typedef struct {
    bool enable_frequency_modulation;
    bool enable_power_modulation;
    bool enable_spindle_generation;
    float modulation_strength;
} oscillations_sleep_config_t;

typedef struct {
    float dominant_frequency;
    oscillation_band_t dominant_band;
    float delta_power;
    float theta_power;
    float alpha_power;
    float beta_power;
    float gamma_power;
    float spindle_activity;
    float ripple_activity;
    sleep_state_t current_state;
    float sleep_pressure;
} oscillations_sleep_effects_t;

typedef struct oscillations_sleep_bridge_struct* oscillations_sleep_bridge_t;

int oscillations_sleep_default_config(oscillations_sleep_config_t* config);
oscillations_sleep_bridge_t oscillations_sleep_bridge_create(const oscillations_sleep_config_t* config, sleep_system_t sleep);
void oscillations_sleep_bridge_destroy(oscillations_sleep_bridge_t bridge);
int oscillations_sleep_update(oscillations_sleep_bridge_t bridge);
int oscillations_sleep_get_effects(const oscillations_sleep_bridge_t bridge, oscillations_sleep_effects_t* effects);
float oscillations_sleep_get_frequency(const oscillations_sleep_bridge_t bridge);
oscillation_band_t oscillations_sleep_get_band(const oscillations_sleep_bridge_t bridge);

float oscillations_sleep_freq_for_state(sleep_state_t state);
oscillation_band_t oscillations_sleep_band_for_state(sleep_state_t state);
float oscillations_sleep_spindle_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OSCILLATIONS_SLEEP_BRIDGE_H */
