/**
 * @file nimcp_visual_adapter.h
 * @brief Visual Processing Adapter for Sensory Layer
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Adapts retina/visual cortex for Sensory layer integration
 * WHY:  Visual processing is primary sensory input for many tasks
 * HOW:  Implements nimcp_module_interface_t wrapping retina systems
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VISUAL_ADAPTER_H
#define NIMCP_VISUAL_ADAPTER_H

#include "integration/core/nimcp_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nimcp_visual_adapter_struct* nimcp_visual_adapter_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t num_channels;
    bool enable_uv;
    bool enable_nir;
    bool enable_thermal;
    bool enable_logging;
} nimcp_visual_adapter_config_t;

typedef struct {
    float mean_luminance;
    float contrast;
    float motion_energy;
    uint32_t salient_regions;
    bool is_active;
} nimcp_visual_adapter_state_t;

typedef struct {
    uint64_t updates_processed;
    uint64_t messages_handled;
    uint64_t frames_processed;
} nimcp_visual_adapter_stats_t;

NIMCP_EXPORT nimcp_visual_adapter_config_t nimcp_visual_adapter_default_config(void);
NIMCP_EXPORT nimcp_visual_adapter_t nimcp_visual_adapter_create(const nimcp_visual_adapter_config_t* config);
NIMCP_EXPORT void nimcp_visual_adapter_destroy(nimcp_visual_adapter_t adapter);
NIMCP_EXPORT nimcp_module_interface_t* nimcp_visual_adapter_get_interface(nimcp_visual_adapter_t adapter);
NIMCP_EXPORT nimcp_layer_error_t nimcp_visual_adapter_process_frame(nimcp_visual_adapter_t adapter, const float* pixels, uint32_t size);
NIMCP_EXPORT nimcp_layer_error_t nimcp_visual_adapter_get_features(nimcp_visual_adapter_t adapter, float* features_out, uint32_t max_features, uint32_t* count_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_visual_adapter_get_state(nimcp_visual_adapter_t adapter, nimcp_visual_adapter_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_visual_adapter_get_stats(nimcp_visual_adapter_t adapter, nimcp_visual_adapter_stats_t* stats_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_visual_adapter_reset_stats(nimcp_visual_adapter_t adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VISUAL_ADAPTER_H */
