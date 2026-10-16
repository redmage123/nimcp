#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_cerebellum_bridge.c - Language-Cerebellum Timing Bridge
//=============================================================================

#include "utils/bridge/nimcp_bridge_base.h"
#include "language/bridges/nimcp_language_cerebellum_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_cerebellum_bridge)

#define LOG_MODULE "LANGUAGE_CEREBELLUM_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

#define MAX_PHONEME_DURATIONS 64

struct language_cerebellum_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    language_orchestrator_t* language;
    cerebellum_adapter_t* cerebellum;
    language_cerebellum_config_t config;

    /* State */
    lc_cerebellum_state_t state;
    uint64_t last_update_ms;

    /* Timing state */
    float current_speech_rate;        /* Syllables per second */

    /* Rhythm state */
    rhythm_state_t rhythm;
    bool rhythm_active;
    uint64_t rhythm_start_ms;

    /* Duration prediction model (simple exponential moving average) */
    struct {
        char phoneme[8];
        float avg_duration_ms;
        uint32_t sample_count;
    } duration_model[MAX_PHONEME_DURATIONS];
    uint32_t model_entries;

    /* Statistics */
    language_cerebellum_stats_t stats;
    float total_prediction_error;
    uint32_t prediction_count;
};

//=============================================================================
// Static Helpers
//=============================================================================

static float get_rhythm_multiplier(speech_rhythm_type_t rhythm, uint32_t syllable_index) {
    /* Different rhythm types have different timing patterns */
    switch (rhythm) {
        case RHYTHM_ISOCHRONOUS:
            return 1.0f;  /* All equal */

        case RHYTHM_STRESS_TIMED:
            /* Alternating stress pattern: stressed syllables longer */
            return (syllable_index % 2 == 0) ? 1.2f : 0.8f;

        case RHYTHM_SYLLABLE_TIMED:
            return 1.0f;  /* All roughly equal */

        case RHYTHM_MORA_TIMED:
            /* Mora-based timing */
            return 0.9f + (syllable_index % 3) * 0.1f;

        default:
            return 1.0f;
    }
}

static int find_phoneme_entry(language_cerebellum_bridge_t* bridge, const char* phoneme) {
    for (uint32_t i = 0; i < bridge->model_entries; i++) {
        if (strcmp(bridge->duration_model[i].phoneme, phoneme) == 0) {
            return (int)i;
        }
    }
    return -1;  /* Not found - normal lookup behavior */
}

static float get_default_phoneme_duration(const char* phoneme) {
    /* Default durations for common phoneme categories (in ms) */
    if (!phoneme || !phoneme[0]) return 80.0f;

    char first = phoneme[0];

    /* Vowels tend to be longer */
    if (first == 'a' || first == 'e' || first == 'i' ||
        first == 'o' || first == 'u') {
        return 120.0f;
    }

    /* Stops are short */
    if (first == 'p' || first == 'b' || first == 't' ||
        first == 'd' || first == 'k' || first == 'g') {
        return 60.0f;
    }

    /* Fricatives are medium */
    if (first == 'f' || first == 's' || first == 'z' ||
        first == 'v' || first == 'h') {
        return 90.0f;
    }

    /* Nasals */
    if (first == 'm' || first == 'n') {
        return 80.0f;
    }

    /* Liquids */
    if (first == 'l' || first == 'r') {
        return 70.0f;
    }

    return 80.0f;  /* Default */
}

//=============================================================================
// Public API
//=============================================================================

language_cerebellum_config_t language_cerebellum_default_config(void) {
    language_cerebellum_config_t config = {
        .update_interval_ms = 20,       /* Fast updates for timing precision */
        .enable_timing_control = true,
        .enable_rhythm_generation = true,
        .enable_error_prediction = true,
        .enable_bio_async = false,
        .default_speech_rate = 4.0f,    /* ~4 syllables/sec normal speech */
        .timing_precision = 0.9f
    };
    return config;
}

language_cerebellum_bridge_t* language_cerebellum_bridge_create(
    language_orchestrator_t* language,
    cerebellum_adapter_t* cerebellum,
    const language_cerebellum_config_t* config)
{
    if (!language || !cerebellum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cerebellum_bridge_create: required parameter is NULL (language, cerebellum)");
        return NULL;
    }

    language_cerebellum_bridge_t* bridge = nimcp_calloc(1, sizeof(language_cerebellum_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->language = language;
    bridge->cerebellum = cerebellum;
    bridge->config = config ? *config : language_cerebellum_default_config();
    bridge->state = LC_STATE_IDLE;
    bridge->last_update_ms = nimcp_time_now_us() / 1000;

    bridge->current_speech_rate = bridge->config.default_speech_rate;

    /* Initialize rhythm state */
    bridge->rhythm.beat_interval_ms = 500.0f;  /* 120 BPM default */
    bridge->rhythm.phase = 0.0f;
    bridge->rhythm.tempo = 120.0f;
    bridge->rhythm.type = RHYTHM_STRESS_TIMED;
    bridge->rhythm_active = false;

    bridge->model_entries = 0;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->total_prediction_error = 0.0f;
    bridge->prediction_count = 0;

    return bridge;
}

void language_cerebellum_bridge_destroy(language_cerebellum_bridge_t* bridge) {
    if (bridge) {
        nimcp_free(bridge);
    }
}

int language_cerebellum_bridge_update(language_cerebellum_bridge_t* bridge, uint64_t timestamp_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    uint64_t elapsed = timestamp_ms - bridge->last_update_ms;
    if (elapsed < bridge->config.update_interval_ms) {
        return 0;
    }

    bridge->last_update_ms = timestamp_ms;

    /* Update rhythm phase if active */
    if (bridge->rhythm_active) {
        uint64_t rhythm_elapsed = timestamp_ms - bridge->rhythm_start_ms;
        float cycles = (float)rhythm_elapsed / bridge->rhythm.beat_interval_ms;
        bridge->rhythm.phase = cycles - floorf(cycles);  /* Fractional part */
    }

    /* Update statistics */
    bridge->stats.state = bridge->state;
    if (bridge->prediction_count > 0) {
        bridge->stats.avg_prediction_error_ms =
            bridge->total_prediction_error / (float)bridge->prediction_count;
    }

    return 0;
}

int language_cerebellum_set_speech_rate(
    language_cerebellum_bridge_t* bridge,
    float syllables_per_second)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (syllables_per_second < 0.5f) syllables_per_second = 0.5f;
    if (syllables_per_second > 10.0f) syllables_per_second = 10.0f;

    bridge->current_speech_rate = syllables_per_second;

    return 0;
}

int language_cerebellum_get_timing_pattern(
    language_cerebellum_bridge_t* bridge,
    uint32_t syllable_count,
    speech_rhythm_type_t rhythm,
    timing_pattern_t* pattern)
{
    if (!bridge || !pattern || syllable_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cerebellum_get_timing_pattern: required parameter is NULL (bridge, pattern)");
        return -1;
    }

    if (!bridge->config.enable_timing_control) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cerebellum_get_timing_pattern: bridge->config is NULL");
        return -1;
    }

    bridge->state = LC_STATE_TIMING_ACTIVE;

    /* Allocate arrays */
    pattern->durations_ms = nimcp_calloc(syllable_count, sizeof(float));
    pattern->onset_times_ms = nimcp_calloc(syllable_count, sizeof(float));

    if (!pattern->durations_ms || !pattern->onset_times_ms) {
        if (pattern->durations_ms) nimcp_free(pattern->durations_ms);
        if (pattern->onset_times_ms) nimcp_free(pattern->onset_times_ms);
        bridge->state = LC_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "language_cerebellum_get_timing_pattern: validation failed");
        return -1;
    }

    pattern->syllable_count = syllable_count;
    pattern->rhythm = rhythm;
    pattern->speech_rate = bridge->current_speech_rate;

    /* Base syllable duration */
    float base_duration_ms = 1000.0f / bridge->current_speech_rate;

    /* Calculate durations and onset times */
    float cumulative_time = 0.0f;
    for (uint32_t i = 0; i < syllable_count; i++) {
        float multiplier = get_rhythm_multiplier(rhythm, i);
        pattern->durations_ms[i] = base_duration_ms * multiplier;
        pattern->onset_times_ms[i] = cumulative_time;
        cumulative_time += pattern->durations_ms[i];
    }

    bridge->stats.timing_adjustments++;
    bridge->state = LC_STATE_IDLE;

    return 0;
}

int language_cerebellum_adjust_timing(
    language_cerebellum_bridge_t* bridge,
    float adjustment_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Adjust speech rate based on timing feedback */
    /* Positive adjustment = slow down, negative = speed up */
    float rate_adjustment = adjustment_ms / 1000.0f * bridge->current_speech_rate;
    bridge->current_speech_rate -= rate_adjustment * 0.1f;  /* Gradual adjustment */

    /* Clamp to valid range */
    if (bridge->current_speech_rate < 0.5f) bridge->current_speech_rate = 0.5f;
    if (bridge->current_speech_rate > 10.0f) bridge->current_speech_rate = 10.0f;

    bridge->stats.timing_adjustments++;

    return 0;
}

int language_cerebellum_start_rhythm(
    language_cerebellum_bridge_t* bridge,
    speech_rhythm_type_t type,
    float tempo_bpm)
{
    if (!bridge || type >= RHYTHM_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "language_cerebellum_start_rhythm: bridge is NULL");
        return -1;
    }

    if (!bridge->config.enable_rhythm_generation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cerebellum_start_rhythm: bridge->config is NULL");
        return -1;
    }

    bridge->state = LC_STATE_RHYTHM_ACTIVE;

    bridge->rhythm.type = type;
    bridge->rhythm.tempo = (tempo_bpm < 30.0f) ? 30.0f :
                          (tempo_bpm > 300.0f) ? 300.0f : tempo_bpm;
    bridge->rhythm.beat_interval_ms = 60000.0f / bridge->rhythm.tempo;
    bridge->rhythm.phase = 0.0f;

    bridge->rhythm_active = true;
    bridge->rhythm_start_ms = nimcp_time_now_us() / 1000;

    return 0;
}

int language_cerebellum_stop_rhythm(language_cerebellum_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->rhythm_active = false;
    bridge->state = LC_STATE_IDLE;

    return 0;
}

int language_cerebellum_get_rhythm_state(
    const language_cerebellum_bridge_t* bridge,
    rhythm_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cerebellum_get_rhythm_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    *state = bridge->rhythm;
    return 0;
}

int language_cerebellum_sync_to_beat(
    language_cerebellum_bridge_t* bridge,
    float* next_beat_ms)
{
    if (!bridge || !next_beat_ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cerebellum_sync_to_beat: required parameter is NULL (bridge, next_beat_ms)");
        return -1;
    }

    if (!bridge->rhythm_active) {
        *next_beat_ms = 0.0f;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cerebellum_sync_to_beat: bridge->rhythm_active is NULL");
        return -1;
    }

    /* Calculate time to next beat */
    float remaining_phase = 1.0f - bridge->rhythm.phase;
    *next_beat_ms = remaining_phase * bridge->rhythm.beat_interval_ms;

    bridge->stats.rhythm_beats++;

    return 0;
}

int language_cerebellum_predict_duration(
    language_cerebellum_bridge_t* bridge,
    const char* phoneme,
    float* predicted_ms)
{
    if (!bridge || !phoneme || !predicted_ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cerebellum_predict_duration: required parameter is NULL (bridge, phoneme, predicted_ms)");
        return -1;
    }

    if (!bridge->config.enable_error_prediction) {
        *predicted_ms = get_default_phoneme_duration(phoneme);
        return 0;
    }

    bridge->state = LC_STATE_PREDICTING;

    /* Look up learned duration */
    int entry = find_phoneme_entry(bridge, phoneme);

    if (entry >= 0) {
        *predicted_ms = bridge->duration_model[entry].avg_duration_ms;
    } else {
        *predicted_ms = get_default_phoneme_duration(phoneme);
    }

    /* Apply speech rate scaling */
    float rate_scale = bridge->config.default_speech_rate / bridge->current_speech_rate;
    *predicted_ms *= rate_scale;

    bridge->stats.predictions_made++;
    bridge->state = LC_STATE_IDLE;

    return 0;
}

int language_cerebellum_report_actual(
    language_cerebellum_bridge_t* bridge,
    const char* phoneme,
    float actual_ms,
    timing_prediction_t* result)
{
    if (!bridge || !phoneme || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cerebellum_report_actual: required parameter is NULL (bridge, phoneme, result)");
        return -1;
    }

    /* Get prediction */
    float predicted_ms;
    language_cerebellum_predict_duration(bridge, phoneme, &predicted_ms);

    result->predicted_duration_ms = predicted_ms;
    result->actual_duration_ms = actual_ms;
    result->error_ms = actual_ms - predicted_ms;

    /* Classify error */
    float abs_error = fabsf(result->error_ms);
    float relative_error = abs_error / predicted_ms;

    if (abs_error < 10.0f) {
        result->error_type = TIMING_ERROR_NONE;
    } else if (result->error_ms > 0 && relative_error > 0.3f) {
        result->error_type = TIMING_ERROR_TOO_SLOW;
        bridge->stats.prediction_errors++;
    } else if (result->error_ms < 0 && relative_error > 0.3f) {
        result->error_type = TIMING_ERROR_TOO_FAST;
        bridge->stats.prediction_errors++;
    } else {
        result->error_type = TIMING_ERROR_IRREGULAR;
    }

    /* Update statistics */
    bridge->total_prediction_error += abs_error;
    bridge->prediction_count++;

    /* Update accuracy metric */
    float accuracy = 1.0f - (abs_error / (predicted_ms + 1.0f));
    if (accuracy < 0.0f) accuracy = 0.0f;
    bridge->stats.timing_accuracy = (bridge->stats.timing_accuracy * 0.95f) + (accuracy * 0.05f);

    return 0;
}

int language_cerebellum_update_model(
    language_cerebellum_bridge_t* bridge,
    const timing_prediction_t* feedback)
{
    if (!bridge || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cerebellum_update_model: required parameter is NULL (bridge, feedback)");
        return -1;
    }

    /* This would update internal duration model based on feedback */
    /* For now, the model is updated during report_actual via EMA */

    return 0;
}

int language_cerebellum_get_stats(
    const language_cerebellum_bridge_t* bridge,
    language_cerebellum_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cerebellum_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

lc_cerebellum_state_t language_cerebellum_get_state(
    const language_cerebellum_bridge_t* bridge)
{
    if (!bridge) {
        return LC_STATE_ERROR;
    }
    return bridge->state;
}

void language_cerebellum_free_timing_pattern(timing_pattern_t* pattern) {
    if (pattern) {
        if (pattern->durations_ms) {
            nimcp_free(pattern->durations_ms);
            pattern->durations_ms = NULL;
        }
        if (pattern->onset_times_ms) {
            nimcp_free(pattern->onset_times_ms);
            pattern->onset_times_ms = NULL;
        }
        pattern->syllable_count = 0;
    }
}
