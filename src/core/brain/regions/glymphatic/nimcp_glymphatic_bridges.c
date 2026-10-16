/**
 * @file nimcp_glymphatic_bridges.c
 * @brief Glymphatic System - Bridge Integrations
 * @version 1.0.0
 * @date 2026-03-05
 *
 * WHAT: Bridge connections between glymphatic system and other brain subsystems
 * WHY:  The glymphatic system interacts with sleep, immune, glial, hypothalamus,
 *       training, inference, world model, thalamic relay, bio-async, and GPU substrate
 * HOW:  Stub implementations for each bridge, providing integration hooks
 *
 * INTEGRATION POINTS:
 * - Sleep Bridge: Primary driver of clearance efficiency
 * - Immune/BBB Bridge: High waste triggers inflammation alert
 * - Glial Bridge: AQP4 expression on astrocytic endfeet
 * - Hypothalamus Bridge: Circadian modulation of clearance
 * - Training Bridge: Waste penalty on learning rate
 * - Inference Bridge: Confidence penalty from waste accumulation
 * - World Model Bridge: Waste state affects world model accuracy
 * - Thalamic Bridge: Relay gain modulation during clearance
 * - Bio-Async Bridge: Publishes glymphatic state for distributed coordination
 * - Substrate GPU Bridge: GPU-accelerated waste diffusion (future)
 *
 * @author NIMCP Development Team
 */

#include "core/brain/regions/glymphatic/nimcp_glymphatic.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#include <math.h>
#include <string.h>

#define LOG_MODULE "GLYMPHATIC_BRIDGES"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(glymphatic_bridges, MESH_ADAPTER_CATEGORY_GLIAL)

/*=============================================================================
 * Local Helpers
 *===========================================================================*/

static float nimcp_clampf(float val, float lo, float hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

static bool glymphatic_bridge_is_valid(const glymphatic_system_t* system) {
    return system && system->magic == GLYM_MAGIC;
}

/*=============================================================================
 * Sleep Bridge
 *
 * Primary integration: sleep state drives clearance efficiency.
 * The sleep system notifies the glymphatic system of state transitions.
 *===========================================================================*/

/**
 * @brief Sleep bridge callback: notify glymphatic of sleep state change
 *
 * WHAT: Called by sleep/wake system when sleep stage transitions occur
 * WHY:  Sleep state is the primary modulator of glymphatic clearance
 * HOW:  Delegates to glymphatic_on_sleep_state_change
 *
 * @param system      The glymphatic system
 * @param sleep_state New sleep state (GLYM_SLEEP_* constant)
 * @return 0 on success, -1 on error
 */
int glymphatic_bridge_on_sleep_transition(glymphatic_system_t* system,
                                           uint32_t sleep_state) {
    if (!glymphatic_bridge_is_valid(system)) {
        return -1;
    }
    return glymphatic_on_sleep_state_change(system, sleep_state);
}

/**
 * @brief Sleep bridge: get sleep quality metric from clearance performance
 *
 * WHAT: Returns a sleep quality indicator based on clearance efficiency
 * WHY:  Sleep system can use this to assess whether sleep is restorative
 * HOW:  Compares waste cleared vs accumulated during current sleep episode
 *
 * @param system The glymphatic system
 * @return Sleep quality (0.0-1.0), higher = more restorative
 */
float glymphatic_bridge_get_sleep_quality(const glymphatic_system_t* system) {
    if (!glymphatic_bridge_is_valid(system)) {
        return 0.5f;
    }
    /* Quality inversely proportional to remaining waste */
    float quality = 1.0f - system->waste_accumulation;
    return nimcp_clampf(quality, 0.0f, 1.0f);
}

/*=============================================================================
 * Immune/BBB Bridge
 *
 * High waste levels trigger inflammation alerts through the immune system.
 * Blood-brain barrier (BBB) integrity affects clearance efficiency.
 *===========================================================================*/

/**
 * @brief Immune bridge: check if waste levels warrant inflammation alert
 *
 * WHAT: Returns whether waste accumulation exceeds alert threshold
 * WHY:  Chronic waste buildup activates neuroinflammatory pathways
 * HOW:  Compares waste_accumulation against config.waste_alert_threshold
 *
 * @param system The glymphatic system
 * @return true if inflammation alert should be raised
 */
bool glymphatic_bridge_immune_should_alert(const glymphatic_system_t* system) {
    if (!glymphatic_bridge_is_valid(system)) {
        return false;
    }
    return system->waste_accumulation >= system->config.waste_alert_threshold;
}

/**
 * @brief Immune bridge: get inflammation severity from waste levels
 *
 * WHAT: Returns severity of neuroinflammation signal from waste buildup
 * WHY:  Immune system needs graded signal, not just binary alert
 * HOW:  Maps waste above threshold to 0.0-1.0 severity scale
 *
 * @param system The glymphatic system
 * @return Inflammation severity (0.0-1.0)
 */
float glymphatic_bridge_immune_get_severity(const glymphatic_system_t* system) {
    if (!glymphatic_bridge_is_valid(system)) {
        return 0.0f;
    }
    float threshold = system->config.waste_alert_threshold;
    if (system->waste_accumulation < threshold) {
        return 0.0f;
    }
    /* Scale 0-1 above threshold */
    float severity = (system->waste_accumulation - threshold) / (1.0f - threshold);
    return nimcp_clampf(severity, 0.0f, 1.0f);
}

/**
 * @brief Immune bridge: set BBB integrity factor
 *
 * WHAT: Adjusts clearance based on blood-brain barrier health
 * WHY:  BBB disruption impairs glymphatic drainage
 * HOW:  Modulates AQP4 expression proportional to BBB integrity
 *
 * @param system    The glymphatic system
 * @param integrity BBB integrity (0.0-1.0, 1.0 = fully intact)
 * @return 0 on success, -1 on error
 */
int glymphatic_bridge_immune_set_bbb_integrity(glymphatic_system_t* system,
                                                float integrity) {
    if (!glymphatic_bridge_is_valid(system)) {
        return -1;
    }
    if (!isfinite(integrity)) {
        return -1;
    }
    integrity = nimcp_clampf(integrity, 0.0f, 1.0f);

    nimcp_mutex_lock(system->lock);
    /* BBB disruption reduces effective AQP4 function */
    system->aquaporin4_expression = system->config.aqp4_expression * integrity;
    nimcp_mutex_unlock(system->lock);

    return 0;
}

/*=============================================================================
 * Glial Bridge
 *
 * Astrocytes express AQP4 channels that drive glymphatic clearance.
 * Glial activity and health directly affect clearance capacity.
 *===========================================================================*/

/**
 * @brief Glial bridge: update AQP4 expression from astrocyte state
 *
 * WHAT: Sets AQP4 expression level based on astrocyte activity
 * WHY:  AQP4 polarization on astrocytic endfeet is rate-limiting for clearance
 * HOW:  Updates aquaporin4_expression field
 *
 * @param system         The glymphatic system
 * @param aqp4_level     AQP4 expression (0.0-1.0)
 * @return 0 on success, -1 on error
 */
int glymphatic_bridge_glial_set_aqp4(glymphatic_system_t* system,
                                      float aqp4_level) {
    if (!glymphatic_bridge_is_valid(system)) {
        return -1;
    }
    if (!isfinite(aqp4_level)) {
        return -1;
    }

    nimcp_mutex_lock(system->lock);
    system->aquaporin4_expression = nimcp_clampf(aqp4_level, 0.0f, 1.0f);
    nimcp_mutex_unlock(system->lock);

    return 0;
}

/**
 * @brief Glial bridge: get clearance demand for astrocyte activation
 *
 * WHAT: Returns how much astrocyte support the glymphatic system needs
 * WHY:  Astrocytes should upregulate AQP4 when waste is high
 * HOW:  Returns waste level as a demand signal
 *
 * @param system The glymphatic system
 * @return Clearance demand (0.0-1.0)
 */
float glymphatic_bridge_glial_get_demand(const glymphatic_system_t* system) {
    if (!glymphatic_bridge_is_valid(system)) {
        return 0.0f;
    }
    return system->waste_accumulation;
}

/*=============================================================================
 * Hypothalamus Bridge
 *
 * Circadian modulation: SCN in hypothalamus regulates sleep timing,
 * which indirectly controls glymphatic clearance windows.
 *===========================================================================*/

/**
 * @brief Hypothalamus bridge: apply circadian modulation
 *
 * WHAT: Modulates glymphatic sensitivity based on circadian phase
 * WHY:  Clearance efficiency varies with circadian rhythm even within sleep
 * HOW:  Adjusts base clearance rate by circadian factor
 *
 * @param system          The glymphatic system
 * @param circadian_phase Circadian phase (0.0-1.0, 0=midnight, 0.5=noon)
 * @return 0 on success, -1 on error
 */
int glymphatic_bridge_hypothalamus_circadian(glymphatic_system_t* system,
                                              float circadian_phase) {
    if (!glymphatic_bridge_is_valid(system)) {
        return -1;
    }
    if (!isfinite(circadian_phase)) {
        return -1;
    }

    /* Clearance peaks in early night (phase ~0.0-0.2) */
    /* Stub: log and return success. Full implementation would modulate
     * base_clearance_rate by a circadian factor curve. */
    (void)circadian_phase;

    NIMCP_LOGGING_DEBUG("Glymphatic: circadian phase %.2f received", circadian_phase);
    return 0;
}

/*=============================================================================
 * Training Bridge
 *
 * Waste accumulation penalizes learning rate: high waste = impaired plasticity.
 * This models the biological finding that sleep deprivation impairs learning.
 *===========================================================================*/

/**
 * @brief Training bridge: get learning rate penalty from waste accumulation
 *
 * WHAT: Returns a multiplier to apply to learning rate based on waste levels
 * WHY:  High metabolic waste impairs synaptic plasticity
 * HOW:  Maps waste level to LR multiplier: 1.0 (clean) to 0.3 (saturated)
 *
 * @param system The glymphatic system
 * @return Learning rate multiplier (0.3-1.0)
 */
float glymphatic_bridge_training_get_lr_penalty(const glymphatic_system_t* system) {
    if (!glymphatic_bridge_is_valid(system)) {
        return 1.0f;  /* No penalty if system unavailable */
    }

    /* Linear penalty: waste=0 -> mult=1.0, waste=1 -> mult=0.3 */
    float penalty = 1.0f - (system->waste_accumulation * 0.7f);
    return nimcp_clampf(penalty, 0.3f, 1.0f);
}

/**
 * @brief Training bridge: check if training should pause for clearance
 *
 * WHAT: Returns whether waste levels are too high for effective learning
 * WHY:  Biological parallel: sleep deprivation makes learning futile
 * HOW:  Returns true if waste exceeds 90% of max
 *
 * @param system The glymphatic system
 * @return true if training should pause
 */
bool glymphatic_bridge_training_should_pause(const glymphatic_system_t* system) {
    if (!glymphatic_bridge_is_valid(system)) {
        return false;
    }
    return system->waste_accumulation > 0.9f;
}

/*=============================================================================
 * Inference Bridge
 *
 * Waste accumulation reduces inference confidence: tired brain = uncertain outputs.
 *===========================================================================*/

/**
 * @brief Inference bridge: get confidence penalty from waste accumulation
 *
 * WHAT: Returns a multiplier to apply to inference confidence
 * WHY:  Metabolic waste accumulation degrades neural signal clarity
 * HOW:  Maps waste level to confidence multiplier: 1.0 (clean) to 0.5 (saturated)
 *
 * @param system The glymphatic system
 * @return Confidence multiplier (0.5-1.0)
 */
float glymphatic_bridge_inference_get_confidence_penalty(
    const glymphatic_system_t* system) {
    if (!glymphatic_bridge_is_valid(system)) {
        return 1.0f;
    }

    /* Gentler penalty than training: waste=0 -> 1.0, waste=1 -> 0.5 */
    float penalty = 1.0f - (system->waste_accumulation * 0.5f);
    return nimcp_clampf(penalty, 0.5f, 1.0f);
}

/*=============================================================================
 * World Model Bridge
 *
 * World model accuracy degrades with waste accumulation.
 *===========================================================================*/

/**
 * @brief World model bridge: get accuracy penalty from waste
 *
 * WHAT: Returns accuracy degradation factor for world model predictions
 * WHY:  High waste impairs predictive processing fidelity
 * HOW:  Maps waste to accuracy multiplier
 *
 * @param system The glymphatic system
 * @return Accuracy multiplier (0.6-1.0)
 */
float glymphatic_bridge_world_model_get_accuracy_penalty(
    const glymphatic_system_t* system) {
    if (!glymphatic_bridge_is_valid(system)) {
        return 1.0f;
    }

    float penalty = 1.0f - (system->waste_accumulation * 0.4f);
    return nimcp_clampf(penalty, 0.6f, 1.0f);
}

/*=============================================================================
 * Thalamic Bridge
 *
 * Thalamic relay gain is modulated during glymphatic clearance.
 * During deep sleep, thalamic gating reduces to allow clearance.
 *===========================================================================*/

/**
 * @brief Thalamic bridge: get relay gain modulation
 *
 * WHAT: Returns thalamic relay gain modifier during glymphatic activity
 * WHY:  During active clearance, sensory gating should be reduced
 * HOW:  Returns lower gain when system is ACTIVE or FLUSHING
 *
 * @param system The glymphatic system
 * @return Relay gain modifier (0.2-1.0)
 */
float glymphatic_bridge_thalamic_get_relay_gain(const glymphatic_system_t* system) {
    if (!glymphatic_bridge_is_valid(system)) {
        return 1.0f;
    }

    switch (system->state) {
        case GLYM_ACTIVE:   return 0.2f;  /* Heavy clearance, minimal relay */
        case GLYM_FLUSHING: return 0.3f;  /* Still reduced */
        case GLYM_PRIMING:  return 0.6f;  /* Transitioning down */
        case GLYM_INACTIVE:
        default:            return 1.0f;  /* Full relay */
    }
}

/*=============================================================================
 * Bio-Async Bridge
 *
 * Publishes glymphatic state for distributed coordination via bio-async router.
 *===========================================================================*/

/**
 * @brief Bio-async bridge: get glymphatic state summary for broadcast
 *
 * WHAT: Packs glymphatic state into a compact summary for bio-async publishing
 * WHY:  Other subsystems need glymphatic state for coordinated response
 * HOW:  Fills output struct with key metrics
 *
 * @param system          The glymphatic system
 * @param out_state       Output state enum value
 * @param out_waste       Output waste level
 * @param out_clearance   Output clearance rate
 * @return 0 on success, -1 on error
 */
int glymphatic_bridge_bio_async_get_summary(const glymphatic_system_t* system,
                                             uint32_t* out_state,
                                             float* out_waste,
                                             float* out_clearance) {
    if (!glymphatic_bridge_is_valid(system)) {
        return -1;
    }

    if (out_state)    *out_state    = (uint32_t)system->state;
    if (out_waste)    *out_waste    = system->waste_accumulation;
    if (out_clearance) *out_clearance = system->clearance_rate;

    return 0;
}

/*=============================================================================
 * Substrate GPU Bridge
 *
 * GPU-accelerated waste diffusion simulation (future implementation).
 *===========================================================================*/

/**
 * @brief GPU substrate bridge: check if GPU acceleration is available
 *
 * WHAT: Returns whether GPU-accelerated diffusion is available
 * WHY:  Large-scale waste diffusion simulation can benefit from GPU
 * HOW:  Stub: always returns false until GPU diffusion kernel is implemented
 *
 * @param system The glymphatic system
 * @return true if GPU acceleration available
 */
bool glymphatic_bridge_gpu_is_available(const glymphatic_system_t* system) {
    /* GPU diffusion available when system is valid and has regions to process.
     * Currently uses CPU fallback — returns false to signal callers to use
     * the CPU path directly, avoiding unnecessary GPU upload/download overhead
     * for the current network sizes. Enable when region count exceeds ~10K. */
    if (!system) return false;
    return false;  /* CPU fallback preferred for current network sizes */
}

/**
 * @brief GPU substrate bridge: run GPU-accelerated diffusion step
 *
 * WHAT: Offloads waste diffusion computation to GPU
 * WHY:  Spatial diffusion across large networks is compute-intensive
 * HOW:  Stub: returns -1 (not implemented)
 *
 * @param system The glymphatic system
 * @param dt_s   Time delta in seconds
 * @return 0 on success, -1 on error/not implemented
 */
int glymphatic_bridge_gpu_diffusion_step(glymphatic_system_t* system,
                                          float dt_s) {
    if (!system) return -1;
    if (dt_s <= 0.0f) return -1;

    /* CPU fallback: delegate to the standard glymphatic update.
     * When GPU implementation is added, this will upload waste concentration
     * maps, run diffusion kernels, and download results. */
    return glymphatic_update(system, dt_s);
}
