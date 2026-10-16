//=============================================================================
// nimcp_language_cerebellum_bridge.h - Language-Cerebellum Timing Bridge
//=============================================================================
/**
 * @file nimcp_language_cerebellum_bridge.h
 * @brief Bridge for speech timing, rhythm, and motor coordination
 *
 * BIOLOGICAL BASIS:
 * - Lateral Cerebellum: Speech timing and sequencing
 * - Cerebellar-cortical loops: Motor prediction for speech
 * - Timing circuits: Rhythm, duration, rate control
 * - Error prediction: Forward models for speech production
 */

#ifndef NIMCP_LANGUAGE_CEREBELLUM_BRIDGE_H
#define NIMCP_LANGUAGE_CEREBELLUM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"
#include "language/nimcp_language_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct language_cerebellum_bridge language_cerebellum_bridge_t;
typedef struct language_orchestrator language_orchestrator_t;

#define LANGUAGE_CEREBELLUM_BIO_MODULE_ID 0x0827

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    uint32_t update_interval_ms;
    bool enable_timing_control;
    bool enable_rhythm_generation;
    bool enable_error_prediction;
    bool enable_bio_async;
    float default_speech_rate;        /**< Syllables per second */
    float timing_precision;           /**< Timing precision [0-1] */
} language_cerebellum_config_t;

//=============================================================================
// Enumerations
//=============================================================================

typedef enum {
    LC_STATE_IDLE = 0,
    LC_STATE_TIMING_ACTIVE,
    LC_STATE_RHYTHM_ACTIVE,
    LC_STATE_PREDICTING,
    LC_STATE_ERROR
} lc_cerebellum_state_t;

typedef enum {
    RHYTHM_ISOCHRONOUS = 0,           /**< Equal timing between beats */
    RHYTHM_STRESS_TIMED,              /**< English-like stress timing */
    RHYTHM_SYLLABLE_TIMED,            /**< French/Spanish-like timing */
    RHYTHM_MORA_TIMED,                /**< Japanese-like mora timing */
    RHYTHM_COUNT
} speech_rhythm_type_t;

typedef enum {
    TIMING_ERROR_NONE = 0,
    TIMING_ERROR_TOO_FAST,
    TIMING_ERROR_TOO_SLOW,
    TIMING_ERROR_IRREGULAR,
    TIMING_ERROR_DRIFT,
    TIMING_ERROR_COUNT
} timing_error_type_t;

//=============================================================================
// Data Structures
//=============================================================================

typedef struct {
    uint32_t syllable_count;          /**< Number of syllables */
    float* durations_ms;              /**< Duration per syllable */
    float* onset_times_ms;            /**< Onset time per syllable */
    speech_rhythm_type_t rhythm;      /**< Rhythm pattern */
    float speech_rate;                /**< Overall speech rate */
} timing_pattern_t;

typedef struct {
    float predicted_duration_ms;      /**< Predicted phoneme duration */
    float actual_duration_ms;         /**< Actual duration observed */
    float error_ms;                   /**< Prediction error */
    timing_error_type_t error_type;   /**< Type of timing error */
} timing_prediction_t;

typedef struct {
    float beat_interval_ms;           /**< Interval between beats */
    float phase;                      /**< Current phase [0-1] */
    float tempo;                      /**< Beats per minute */
    speech_rhythm_type_t type;        /**< Rhythm type */
} rhythm_state_t;

typedef struct {
    uint64_t timing_adjustments;      /**< Timing adjustments made */
    uint64_t rhythm_beats;            /**< Rhythm beats generated */
    uint64_t predictions_made;        /**< Timing predictions made */
    uint64_t prediction_errors;       /**< Significant prediction errors */
    float avg_prediction_error_ms;    /**< Average prediction error */
    float timing_accuracy;            /**< Overall timing accuracy [0-1] */
    lc_cerebellum_state_t state;      /**< Current bridge state */
} language_cerebellum_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

language_cerebellum_config_t language_cerebellum_default_config(void);

language_cerebellum_bridge_t* language_cerebellum_bridge_create(
    language_orchestrator_t* language,
    cerebellum_adapter_t* cerebellum,
    const language_cerebellum_config_t* config
);

void language_cerebellum_bridge_destroy(language_cerebellum_bridge_t* bridge);

//=============================================================================
// Update Function
//=============================================================================

int language_cerebellum_bridge_update(
    language_cerebellum_bridge_t* bridge,
    uint64_t timestamp_ms
);

//=============================================================================
// Timing Control
//=============================================================================

int language_cerebellum_set_speech_rate(
    language_cerebellum_bridge_t* bridge,
    float syllables_per_second
);

int language_cerebellum_get_timing_pattern(
    language_cerebellum_bridge_t* bridge,
    uint32_t syllable_count,
    speech_rhythm_type_t rhythm,
    timing_pattern_t* pattern
);

int language_cerebellum_adjust_timing(
    language_cerebellum_bridge_t* bridge,
    float adjustment_ms
);

//=============================================================================
// Rhythm Generation
//=============================================================================

int language_cerebellum_start_rhythm(
    language_cerebellum_bridge_t* bridge,
    speech_rhythm_type_t type,
    float tempo_bpm
);

int language_cerebellum_stop_rhythm(
    language_cerebellum_bridge_t* bridge
);

int language_cerebellum_get_rhythm_state(
    const language_cerebellum_bridge_t* bridge,
    rhythm_state_t* state
);

int language_cerebellum_sync_to_beat(
    language_cerebellum_bridge_t* bridge,
    float* next_beat_ms
);

//=============================================================================
// Error Prediction
//=============================================================================

int language_cerebellum_predict_duration(
    language_cerebellum_bridge_t* bridge,
    const char* phoneme,
    float* predicted_ms
);

int language_cerebellum_report_actual(
    language_cerebellum_bridge_t* bridge,
    const char* phoneme,
    float actual_ms,
    timing_prediction_t* result
);

int language_cerebellum_update_model(
    language_cerebellum_bridge_t* bridge,
    const timing_prediction_t* feedback
);

//=============================================================================
// Status Functions
//=============================================================================

int language_cerebellum_get_stats(
    const language_cerebellum_bridge_t* bridge,
    language_cerebellum_stats_t* stats
);

lc_cerebellum_state_t language_cerebellum_get_state(
    const language_cerebellum_bridge_t* bridge
);

//=============================================================================
// Cleanup
//=============================================================================

void language_cerebellum_free_timing_pattern(timing_pattern_t* pattern);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_CEREBELLUM_BRIDGE_H */
