/**
 * @file nimcp_superhuman_intra_coordinator.h
 * @brief Superhuman Layer Intra-Layer Coordinator
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Coordinates communication between enhanced perception modules
 * WHY:  Superhuman capabilities must integrate for coherent enhanced perception
 * HOW:  Manages cross-enhancement binding and capability coordination
 *
 * SUPERHUMAN LAYER MODULES:
 * =========================
 * - Eagle Vision: Enhanced visual acuity, long-range focus
 * - Echolocation: Spatial awareness through sound
 * - Time Dilation: Enhanced temporal processing under stress
 * - Magnetoreception: Earth's magnetic field sensing
 * - Electroreception: Electric field sensing
 * - Infrared Vision: Heat detection, thermal imaging
 * - Ultraviolet Vision: UV spectrum perception
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SUPERHUMAN_INTRA_COORDINATOR_H
#define NIMCP_SUPERHUMAN_INTRA_COORDINATOR_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module IDs */
#define SUPERHUMAN_MODULE_EAGLE_VISION      0x0001
#define SUPERHUMAN_MODULE_ECHOLOCATION      0x0002
#define SUPERHUMAN_MODULE_TIME_DILATION     0x0003
#define SUPERHUMAN_MODULE_MAGNETORECEPTION  0x0004
#define SUPERHUMAN_MODULE_ELECTRORECEPTION  0x0005
#define SUPERHUMAN_MODULE_INFRARED          0x0006
#define SUPERHUMAN_MODULE_ULTRAVIOLET       0x0007
#define SUPERHUMAN_MODULE_COUNT             7

/* Message types */
#define SUPERHUMAN_MSG_ENHANCED_VISUAL      (NIMCP_LAYER_MSG_MODULE_BASE + 0x0080)
#define SUPERHUMAN_MSG_ECHO_RETURN          (NIMCP_LAYER_MSG_MODULE_BASE + 0x0081)
#define SUPERHUMAN_MSG_TIME_SCALE           (NIMCP_LAYER_MSG_MODULE_BASE + 0x0082)
#define SUPERHUMAN_MSG_MAGNETIC_FIELD       (NIMCP_LAYER_MSG_MODULE_BASE + 0x0083)
#define SUPERHUMAN_MSG_ELECTRIC_FIELD       (NIMCP_LAYER_MSG_MODULE_BASE + 0x0084)
#define SUPERHUMAN_MSG_THERMAL              (NIMCP_LAYER_MSG_MODULE_BASE + 0x0085)
#define SUPERHUMAN_MSG_UV_DETECTION         (NIMCP_LAYER_MSG_MODULE_BASE + 0x0086)
#define SUPERHUMAN_MSG_CAPABILITY_BLEND     (NIMCP_LAYER_MSG_MODULE_BASE + 0x0087)

typedef struct nimcp_superhuman_intra_struct* nimcp_superhuman_intra_t;

typedef struct {
    /* Module enables */
    bool enable_eagle_vision;
    bool enable_echolocation;
    bool enable_time_dilation;
    bool enable_magnetoreception;
    bool enable_electroreception;
    bool enable_infrared;
    bool enable_ultraviolet;

    /* Key couplings (sparse - not all pairs meaningful) */
    float vision_echo_coupling;         /**< Visual-echo spatial fusion */
    float vision_infrared_coupling;     /**< Visible + thermal fusion */
    float vision_uv_coupling;           /**< Visible + UV fusion */
    float echo_electroreception_coupling; /**< Echo + electric for hunting */
    float time_all_coupling;            /**< Time dilation affects all */

    uint32_t sync_interval_ms;
    float coherence_threshold;
    bool enable_capability_blending;
    bool enable_logging;
    bool enable_metrics;
} nimcp_superhuman_intra_config_t;

typedef struct {
    bool eagle_vision_active;
    bool echolocation_active;
    bool time_dilation_active;
    bool magnetoreception_active;
    bool electroreception_active;
    bool infrared_active;
    bool ultraviolet_active;

    float visual_acuity;
    float echo_precision;
    float time_scale_factor;
    float magnetic_field_strength;
    float electric_field_strength;
    float thermal_sensitivity;
    float uv_sensitivity;

    float capability_blend_strength;
    float layer_coherence;
} nimcp_superhuman_intra_state_t;

typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t enhanced_visual_events;
    uint64_t echo_returns;
    uint64_t time_dilations;
    uint64_t magnetic_detections;
    uint64_t electric_detections;
    uint64_t thermal_detections;
    uint64_t uv_detections;
    uint64_t capability_blends;
    float avg_enhancement_factor;
    float avg_coherence;
} nimcp_superhuman_intra_stats_t;

NIMCP_EXPORT nimcp_superhuman_intra_config_t nimcp_superhuman_intra_default_config(void);
NIMCP_EXPORT nimcp_superhuman_intra_t nimcp_superhuman_intra_create(const nimcp_superhuman_intra_config_t* config);
NIMCP_EXPORT void nimcp_superhuman_intra_destroy(nimcp_superhuman_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_init(nimcp_superhuman_intra_t coord, nimcp_layer_registry_t registry);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_shutdown(nimcp_superhuman_intra_t coord);

NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_connect_eagle_vision(nimcp_superhuman_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_connect_echolocation(nimcp_superhuman_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_connect_time_dilation(nimcp_superhuman_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_connect_magnetoreception(nimcp_superhuman_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_connect_electroreception(nimcp_superhuman_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_connect_infrared(nimcp_superhuman_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_connect_ultraviolet(nimcp_superhuman_intra_t coord, void* module, nimcp_module_interface_t* interface);

NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_update(nimcp_superhuman_intra_t coord, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_sync(nimcp_superhuman_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_send(nimcp_superhuman_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_broadcast(nimcp_superhuman_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_get_state(nimcp_superhuman_intra_t coord, nimcp_superhuman_intra_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_get_stats(nimcp_superhuman_intra_t coord, nimcp_superhuman_intra_stats_t* stats_out);
NIMCP_EXPORT float nimcp_superhuman_intra_get_coherence(nimcp_superhuman_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_intra_reset_stats(nimcp_superhuman_intra_t coord);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SUPERHUMAN_INTRA_COORDINATOR_H */
