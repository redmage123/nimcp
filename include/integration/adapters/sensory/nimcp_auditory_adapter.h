/**
 * @file nimcp_auditory_adapter.h
 * @brief Auditory Processing Adapter for Sensory Layer
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Adapts cochlea/auditory cortex for Sensory layer integration
 * WHY:  Auditory processing for speech, music, environmental sounds
 * HOW:  Implements nimcp_module_interface_t wrapping cochlea systems
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_AUDITORY_ADAPTER_H
#define NIMCP_AUDITORY_ADAPTER_H

#include "integration/core/nimcp_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nimcp_auditory_adapter_struct* nimcp_auditory_adapter_t;

typedef struct {
    uint32_t sample_rate;
    uint32_t num_channels;
    uint32_t num_frequency_bands;
    float min_freq_hz;
    float max_freq_hz;
    bool enable_speech_detection;
    bool enable_logging;
} nimcp_auditory_adapter_config_t;

typedef struct {
    float mean_amplitude;
    float dominant_frequency_hz;
    float speech_probability;
    uint32_t onset_events;
    bool is_active;
} nimcp_auditory_adapter_state_t;

typedef struct {
    uint64_t updates_processed;
    uint64_t messages_handled;
    uint64_t samples_processed;
} nimcp_auditory_adapter_stats_t;

NIMCP_EXPORT nimcp_auditory_adapter_config_t nimcp_auditory_adapter_default_config(void);
NIMCP_EXPORT nimcp_auditory_adapter_t nimcp_auditory_adapter_create(const nimcp_auditory_adapter_config_t* config);
NIMCP_EXPORT void nimcp_auditory_adapter_destroy(nimcp_auditory_adapter_t adapter);
NIMCP_EXPORT nimcp_module_interface_t* nimcp_auditory_adapter_get_interface(nimcp_auditory_adapter_t adapter);
NIMCP_EXPORT nimcp_layer_error_t nimcp_auditory_adapter_process_samples(nimcp_auditory_adapter_t adapter, const float* samples, uint32_t count);
NIMCP_EXPORT nimcp_layer_error_t nimcp_auditory_adapter_get_spectrum(nimcp_auditory_adapter_t adapter, float* spectrum_out, uint32_t max_bands, uint32_t* count_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_auditory_adapter_get_state(nimcp_auditory_adapter_t adapter, nimcp_auditory_adapter_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_auditory_adapter_get_stats(nimcp_auditory_adapter_t adapter, nimcp_auditory_adapter_stats_t* stats_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_auditory_adapter_reset_stats(nimcp_auditory_adapter_t adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUDITORY_ADAPTER_H */
