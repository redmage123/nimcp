#include "constants/nimcp_math_constants.h"
/**
 * @file nimcp_frequency_constants.h
 * @brief Centralized frequency constants for NIMCP
 * @version 1.0.0
 * @date 2025-02-03
 *
 * WHAT: Defines all frequency-related constants used throughout the codebase
 * WHY:  Ensures biological plausibility, eliminates magic numbers, enables tuning
 * HOW:  Single header with organization by frequency band and subsystem
 *
 * Usage: #include "constants/nimcp_frequency_constants.h"
 */

#ifndef NIMCP_FREQUENCY_CONSTANTS_H
#define NIMCP_FREQUENCY_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * System Update Rates
 *===========================================================================*/

/** @brief Default update rate for general processing (30 Hz) */
#define NIMCP_DEFAULT_UPDATE_RATE_HZ        30.0f

/** @brief Fast update rate for real-time systems (60 Hz) */
#define NIMCP_FAST_UPDATE_RATE_HZ           60.0f

/** @brief High-speed update rate for neural processing (100 Hz) */
#define NIMCP_HIGH_UPDATE_RATE_HZ           100.0f

/** @brief Stabilization update rate (200 Hz) */
#define NIMCP_STABILIZATION_RATE_HZ         200.0f

/** @brief Default time step for 60 Hz processing (16.67ms) */
#define NIMCP_DT_60HZ_MS                    16.67f

/** @brief Default time step for 30 Hz processing (33.33ms) */
#define NIMCP_DT_30HZ_MS                    33.33f

/*=============================================================================
 * Thalamic and Subcortical Update Rates
 *===========================================================================*/

/** @brief Thalamic relay update rate (100 Hz) */
#define NIMCP_THALAMIC_UPDATE_RATE_HZ       100.0f

/** @brief Proprioceptive update rate (100 Hz) */
#define NIMCP_PROPRIOCEPTIVE_RATE_HZ        100.0f

/** @brief Motor execution rate (100 Hz) */
#define NIMCP_MOTOR_EXECUTION_RATE_HZ       100.0f

/** @brief Interoceptive update rate (10 Hz) */
#define NIMCP_INTEROCEPTIVE_RATE_HZ         10.0f

/** @brief Prosody sample rate (100 Hz) */
#define NIMCP_PROSODY_SAMPLE_RATE_HZ        100.0f

/*=============================================================================
 * Neural Oscillation Bands - Delta (0.5-4 Hz)
 *===========================================================================*/

/** @brief Delta band low frequency (0.5 Hz) */
#define NIMCP_DELTA_LOW_HZ                  0.5f

/** @brief Delta band high frequency (4 Hz) */
#define NIMCP_DELTA_HIGH_HZ                 4.0f

/** @brief Delta band center frequency (2 Hz) */
#define NIMCP_DELTA_CENTER_HZ               2.0f

/*=============================================================================
 * Neural Oscillation Bands - Theta (4-8 Hz)
 *===========================================================================*/

/** @brief Theta band low frequency (4 Hz) */
#define NIMCP_THETA_LOW_HZ                  4.0f

/** @brief Theta band high frequency (8 Hz) */
#define NIMCP_THETA_HIGH_HZ                 8.0f

/** @brief Theta band center frequency (6 Hz) */
#define NIMCP_THETA_CENTER_HZ               6.0f

/** @brief Theta frequency for speech processing (5 Hz) */
#define NIMCP_THETA_SPEECH_HZ               5.0f

/*=============================================================================
 * Neural Oscillation Bands - Alpha (8-12 Hz)
 *===========================================================================*/

/** @brief Alpha band low frequency (8 Hz) */
#define NIMCP_ALPHA_LOW_HZ                  8.0f

/** @brief Alpha band high frequency (12 Hz) */
#define NIMCP_ALPHA_HIGH_HZ                 12.0f

/** @brief Alpha band center frequency (10 Hz) */
#define NIMCP_ALPHA_CENTER_HZ               10.0f

/** @brief Claustrum alpha frequency (10 Hz) */
#define NIMCP_CLAUSTRUM_ALPHA_HZ            10.0f

/*=============================================================================
 * Neural Oscillation Bands - Beta (12-30 Hz)
 *===========================================================================*/

/** @brief Beta band low frequency (12 Hz) */
#define NIMCP_BETA_LOW_HZ                   12.0f

/** @brief Beta band high frequency (30 Hz) */
#define NIMCP_BETA_HIGH_HZ                  30.0f

/** @brief Beta band center/peak frequency (20 Hz) */
#define NIMCP_BETA_CENTER_HZ                20.0f

/** @brief Low beta for motor suppression (13 Hz) */
#define NIMCP_BETA_MOTOR_LOW_HZ             13.0f

/*=============================================================================
 * Neural Oscillation Bands - Gamma (30-100 Hz)
 *===========================================================================*/

/** @brief Gamma band low frequency (30 Hz) */
#define NIMCP_GAMMA_LOW_HZ                  30.0f

/** @brief Gamma band high frequency (100 Hz) */
#define NIMCP_GAMMA_HIGH_HZ                 100.0f

/** @brief Gamma band center frequency (40 Hz) */
#define NIMCP_GAMMA_CENTER_HZ               40.0f

/** @brief Standard gamma for binding (40 Hz) */
#define NIMCP_GAMMA_BINDING_HZ              40.0f

/** @brief Gamma bandwidth for attention (10 Hz range: 30-50 Hz) */
#define NIMCP_GAMMA_ATTENTION_BW_HZ         10.0f

/** @brief Gamma frequency for speech (25 Hz) */
#define NIMCP_GAMMA_SPEECH_HZ               25.0f

/** @brief Swarm consciousness gamma (40 Hz) */
#define NIMCP_GAMMA_SWARM_HZ                40.0f

/** @brief Claustrum gamma for binding (40 Hz) */
#define NIMCP_CLAUSTRUM_GAMMA_HZ            40.0f

/** @brief Pathological gamma minimum for seizure detection (100 Hz) */
#define NIMCP_PATHOLOGICAL_GAMMA_MIN_HZ     100.0f

/*=============================================================================
 * Neuron Firing Rates
 *===========================================================================*/

/** @brief Locus coeruleus tonic baseline (2 Hz) */
#define NIMCP_LC_TONIC_BASELINE_HZ          2.0f

/** @brief Locus coeruleus phasic maximum (20 Hz) */
#define NIMCP_LC_PHASIC_MAX_HZ              20.0f

/** @brief Prefrontal persistent activity (30 Hz) */
#define NIMCP_PREFRONTAL_PERSISTENT_HZ      30.0f

/** @brief Fast-spiking interneuron max rate (150 Hz) */
#define NIMCP_FSI_MAX_RATE_HZ               150.0f

/** @brief Tonically active neuron baseline (5 Hz) */
#define NIMCP_TAN_TONIC_RATE_HZ             5.0f

/** @brief Low-threshold spiking burst rate (30 Hz) */
#define NIMCP_LTS_BURST_RATE_HZ             30.0f

/** @brief Thalamic burst mode rate (300 Hz) */
#define NIMCP_THALAMIC_BURST_HZ             300.0f

/** @brief SNN default maximum rate (100 Hz) */
#define NIMCP_SNN_DEFAULT_MAX_RATE_HZ       100.0f

/** @brief A1 neuron maximum spike rate (300 Hz) */
#define NIMCP_A1_MAX_SPIKE_RATE_HZ          300.0f

/** @brief Oligodendrocyte activity threshold (1 Hz) */
#define NIMCP_OLIGO_ACTIVITY_THRESHOLD_HZ   1.0f

/** @brief Epigenetic gene induction rate (50 Hz) */
#define NIMCP_EPIGEN_GENE_INDUCTION_HZ      50.0f

/*=============================================================================
 * Audio Frequency Constants
 *===========================================================================*/

/** @brief Human hearing minimum frequency (20 Hz) */
#define NIMCP_HUMAN_AUDIO_MIN_HZ            20.0f

/** @brief Human hearing maximum frequency (20 kHz) */
#define NIMCP_HUMAN_AUDIO_MAX_HZ            20000.0f

/** @brief Mel break frequency (700 Hz) */
#define NIMCP_MEL_BREAK_FREQ_HZ             700.0f

/** @brief Phase locking limit for auditory nerve (4 kHz) */
#define NIMCP_PHASE_LOCK_LIMIT_HZ           4000.0f

/** @brief Base pitch frequency (120 Hz) */
#define NIMCP_BASE_PITCH_HZ                 120.0f

/*=============================================================================
 * Cochlea Extended - Dog Hearing
 *===========================================================================*/

/** @brief Dog hearing minimum frequency (67 Hz) */
#define NIMCP_DOG_AUDIO_MIN_HZ              67.0f

/** @brief Dog hearing maximum frequency (65 kHz) */
#define NIMCP_DOG_AUDIO_MAX_HZ              65000.0f

/** @brief Dog ultrasonic threshold (20 kHz) */
#define NIMCP_DOG_ULTRASONIC_START_HZ       20000.0f

/*=============================================================================
 * Cochlea Extended - Bat Hearing
 *===========================================================================*/

/** @brief Bat hearing minimum frequency (1 kHz) */
#define NIMCP_BAT_AUDIO_MIN_HZ              1000.0f

/** @brief Bat hearing maximum frequency (200 kHz) */
#define NIMCP_BAT_AUDIO_MAX_HZ              200000.0f

/** @brief Bat echolocation minimum frequency (20 kHz) */
#define NIMCP_BAT_ECHOLOCATION_MIN_HZ       20000.0f

/*=============================================================================
 * Wing and Motor Frequencies
 *===========================================================================*/

/** @brief Dragonfly wing maximum frequency (40 Hz) */
#define NIMCP_WING_MAX_FREQ_HZ              40.0f

/** @brief Dragonfly wing minimum frequency (20 Hz) */
#define NIMCP_WING_MIN_FREQ_HZ              20.0f

/*=============================================================================
 * Sampling and Signal Processing Rates
 *===========================================================================*/

/** @brief Oscillation detection sample rate (1 kHz) */
#define NIMCP_OSC_SAMPLE_RATE_HZ            1000.0f

/** @brief Pink noise default sample rate (1 kHz) */
#define NIMCP_PINK_NOISE_SAMPLE_RATE_HZ     1000.0f

/** @brief Cochlea substrate max spike rate (300 Hz) */
#define NIMCP_COCHLEA_MAX_SPIKE_RATE_HZ     300.0f

/** @brief Cochlea phase lock cutoff (4 kHz) */
#define NIMCP_COCHLEA_PHASE_LOCK_CUTOFF_HZ  4000.0f

/*=============================================================================
 * Update Rate Conversion Macros
 *===========================================================================*/

/** @brief Convert Hz to period in milliseconds */
#define NIMCP_HZ_TO_PERIOD_MS(hz)           (1000.0f / (hz))

/** @brief Convert Hz to period in seconds */
#define NIMCP_HZ_TO_PERIOD_SEC(hz)          (1.0f / (hz))

/** @brief Convert period in ms to Hz */
#define NIMCP_PERIOD_MS_TO_HZ(ms)           (1000.0f / (ms))

/** @brief Convert angular frequency (rad/s) to Hz */
#define NIMCP_RAD_TO_HZ(rad)                ((rad) / (NIMCP_TWO_PI_F))

/** @brief Convert Hz to angular frequency (rad/s) */
#define NIMCP_HZ_TO_RAD(hz)                 ((hz) * NIMCP_TWO_PI_F)

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FREQUENCY_CONSTANTS_H */
