/**
 * @file nimcp_information_geometry_bridge.h
 * @brief Information Geometry NIMCP Bridge - Full system integration
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Bridge connecting information geometry to NIMCP subsystems
 * WHY:  Enable KG wiring, exception handling, bio-async for info geometry
 * HOW:  Wraps nimcp_information_geometry with NIMCP integration patterns
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_INFORMATION_GEOMETRY_BRIDGE_H
#define NIMCP_INFORMATION_GEOMETRY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"
#include "physics/geometry/nimcp_information_geometry.h"
#include "core/brain/nimcp_brain_kg.h"
#include "core/brain/nimcp_kg_module_wiring.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Bridge Constants
//=============================================================================

#define INFO_GEOM_BRIDGE_MODULE_NAME    "info_geometry_bridge"
#define INFO_GEOM_BRIDGE_KG_MODULE_ID   0x0450

//=============================================================================
// Bridge Configuration
//=============================================================================

typedef struct {
    bool enable_kg_wiring;
    bool enable_exception_handling;
    bool enable_bio_async;
    bool enable_immune_presentation;
    bool enable_logging;
} info_geom_bridge_config_t;

//=============================================================================
// Bridge Handle
//=============================================================================

typedef struct info_geom_bridge_struct* info_geom_bridge_t;

//=============================================================================
// Bridge Lifecycle
//=============================================================================

NIMCP_EXPORT info_geom_bridge_config_t info_geom_bridge_default_config(void);
NIMCP_EXPORT info_geom_bridge_t info_geom_bridge_create(const info_geom_bridge_config_t* config);
NIMCP_EXPORT void info_geom_bridge_destroy(info_geom_bridge_t bridge);
NIMCP_EXPORT int info_geom_bridge_register_kg(info_geom_bridge_t bridge, brain_kg_t* kg);
NIMCP_EXPORT int info_geom_bridge_register_exception(info_geom_bridge_t bridge, void* handler);
NIMCP_EXPORT int info_geom_bridge_register_bio_async(info_geom_bridge_t bridge, void* channel);

//=============================================================================
// Wave W15: Runtime manifold-shift event emission + read path
//=============================================================================

#ifndef NIMCP_BRAIN_T_DEFINED
#define NIMCP_BRAIN_T_DEFINED
typedef struct brain_struct* brain_t;
#endif

/**
 * @brief Register brain handle for admin-token self-elevation on runtime emit.
 */
NIMCP_EXPORT void info_geom_bridge_kg_register_brain(
    info_geom_bridge_t bridge,
    brain_t brain
);

/**
 * @brief Emit an aggregated manifold-shift event.
 *
 * Rate-limit: call when curvature crosses a threshold, KL divergence spikes,
 * or embedding re-converges — never per-step.
 *
 * @param bridge      Bridge handle (must have been KG-registered)
 * @param kind        "manifold_shift", "curvature_spike", "kl_divergence", ...
 * @param curvature   Ricci curvature magnitude (nan to skip metadata)
 * @param kl_div      KL divergence (nan to skip)
 * @param ts_us       Timestamp in microseconds
 */
NIMCP_EXPORT void info_geom_bridge_kg_trigger_manifold_event(
    info_geom_bridge_t bridge,
    const char* kind,
    float curvature,
    float kl_div,
    uint64_t ts_us
);

/**
 * @brief Read-path: count manifold events matching a kind substring.
 */
NIMCP_EXPORT uint32_t info_geom_bridge_kg_count_events(
    info_geom_bridge_t bridge,
    const char* kind_substr
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INFORMATION_GEOMETRY_BRIDGE_H */
