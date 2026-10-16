//=============================================================================
// nimcp_white_matter_bridges.c - White Matter Bridge Integrations
//=============================================================================
/**
 * @file nimcp_white_matter_bridges.c
 * @brief Bridge functions connecting white matter tracts to brain subsystems
 *
 * WHAT: Integration stubs for sleep, immune, thalamic, training, inference,
 *       substrate GPU, and bio-async subsystems
 * WHY:  White matter tracts interact with multiple brain systems:
 *       - Sleep repairs myelination (oligodendrocyte remyelination)
 *       - Immune inflammation causes demyelination (MS-like pathology)
 *       - Thalamus uses spinothalamic tract for sensory relay
 *       - Training modulates tract bandwidth based on usage
 *       - Inference reads tract delays for timing-dependent processing
 *       - GPU substrate accelerates tract signal propagation
 *       - Bio-async broadcasts tract state changes
 * HOW:  Each bridge is a stub with null checks, LOG_INFO, and return 0/-1.
 *       Forward declarations with void* avoid header dependencies on partner systems.
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-03-05
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/white_matter/nimcp_white_matter_tracts.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/math/nimcp_math_helpers.h"

#include <math.h>

#define LOG_MODULE "WHITE_MATTER_BRIDGES"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(white_matter_bridges, MESH_ADAPTER_CATEGORY_SYSTEM)

//=============================================================================
// Sleep Bridge
//=============================================================================

/**
 * @brief Notify white matter of sleep state transition
 *
 * WHAT: During sleep, oligodendrocytes repair myelination
 * WHY:  Sleep deprivation impairs white matter integrity in vivo
 * HOW:  Apply remyelination delta to all tracts during deep sleep
 *
 * @param wmt White matter system
 * @param sleep_system Sleep system (void* to avoid header dependency)
 * @param sleep_stage Current sleep stage (0=wake, 1=N1, 2=N2, 3=N3/SWS, 4=REM)
 * @param dt_s Timestep in seconds
 * @return 0 on success, -1 on error
 */
int wmt_bridge_sleep_update(wmt_system_t* wmt, void* sleep_system,
                            int sleep_stage, float dt_s) {
    if (!wmt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_bridge_sleep_update: wmt is NULL");
        return -1;
    }

    /* Sleep system NULL is non-fatal (subsystem may not be enabled) */
    if (!sleep_system) {
        return 0;
    }

    if (!isfinite(dt_s) || dt_s <= 0.0f) {
        return -1;
    }

    /* Remyelination rate depends on sleep stage:
     * - SWS (stage 3): Maximum repair (0.005/s)
     * - N2 (stage 2): Moderate repair (0.002/s)
     * - N1/REM: Minimal repair (0.001/s)
     * - Wake: No repair */
    float repair_rate = 0.0f;
    switch (sleep_stage) {
        case 3: repair_rate = 0.005f; break;  /* SWS */
        case 2: repair_rate = 0.002f; break;  /* N2 */
        case 1: repair_rate = 0.001f; break;  /* N1 */
        case 4: repair_rate = 0.001f; break;  /* REM */
        default: repair_rate = 0.0f; break;   /* Wake */
    }

    if (repair_rate > 0.0f) {
        float delta = repair_rate * dt_s;
        for (int i = 0; i < WMT_COUNT; i++) {
            wmt_modulate_myelination(wmt, (white_matter_tract_t)i, delta);
        }
    }

    LOG_DEBUG(LOG_MODULE, "Sleep bridge update: stage=%d, repair_rate=%.4f", sleep_stage, repair_rate);
    return 0;
}

//=============================================================================
// Immune Bridge
//=============================================================================

/**
 * @brief Notify white matter of immune/inflammatory state
 *
 * WHAT: Neuroinflammation damages myelin (demyelination)
 * WHY:  Models autoimmune demyelination (MS), infection-related damage
 * HOW:  Apply demyelination proportional to inflammation level
 *
 * @param wmt White matter system
 * @param immune_system Brain immune system (void* to avoid header dependency)
 * @param inflammation_level Current inflammation [0.0-1.0]
 * @param dt_s Timestep in seconds
 * @return 0 on success, -1 on error
 */
int wmt_bridge_immune_update(wmt_system_t* wmt, void* immune_system,
                             float inflammation_level, float dt_s) {
    if (!wmt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_bridge_immune_update: wmt is NULL");
        return -1;
    }

    if (!immune_system) {
        return 0;
    }

    if (!isfinite(inflammation_level) || !isfinite(dt_s) || dt_s <= 0.0f) {
        return -1;
    }

    float inflammation = nimcp_clampf(inflammation_level, 0.0f, 1.0f);

    /* Demyelination rate proportional to inflammation squared
     * (mild inflammation is tolerable, severe causes rapid damage) */
    if (inflammation > 0.1f) {
        float damage_rate = 0.01f * inflammation * inflammation;
        float delta = -damage_rate * dt_s;
        for (int i = 0; i < WMT_COUNT; i++) {
            wmt_modulate_myelination(wmt, (white_matter_tract_t)i, delta);
        }
        LOG_DEBUG(LOG_MODULE, "Immune bridge: inflammation=%.3f, demyelination_rate=%.4f",
                 inflammation, damage_rate);
    }

    return 0;
}

/**
 * @brief Notify white matter of immune-mediated integrity damage
 *
 * WHAT: Severe immune response can damage tract integrity directly
 * WHY:  Models axonal degeneration from sustained neuroinflammation
 *
 * @param wmt White matter system
 * @param immune_system Brain immune system (void*)
 * @param tract Specific tract to damage
 * @param severity Damage severity [0.0-1.0]
 * @return 0 on success, -1 on error
 */
int wmt_bridge_immune_damage_tract(wmt_system_t* wmt, void* immune_system,
                                   white_matter_tract_t tract, float severity) {
    if (!wmt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_bridge_immune_damage_tract: wmt is NULL");
        return -1;
    }

    if (!isfinite(severity)) {
        return -1;
    }

    float clamped = nimcp_clampf(severity, 0.0f, 1.0f);
    float delta = -0.05f * clamped;  /* Max 5% integrity loss per event */

    int result = wmt_modulate_integrity(wmt, tract, delta);

    if (result == 0) {
        LOG_INFO(LOG_MODULE, "Immune damage to %s: severity=%.3f, delta_integrity=%.4f",
                wmt_tract_name(tract), clamped, delta);
    }

    return result;
}

//=============================================================================
// Thalamic Bridge
//=============================================================================

/**
 * @brief Get spinothalamic tract delay for thalamic relay timing
 *
 * WHAT: Thalamus uses tract delay for sensory gating timing
 * WHY:  Thalamic relay timing depends on spinothalamic conduction velocity
 *
 * @param wmt White matter system
 * @param thalamus Thalamus system (void*)
 * @param out_delay_ms Output: delay in milliseconds
 * @return 0 on success, -1 on error
 */
int wmt_bridge_thalamic_get_delay(const wmt_system_t* wmt, void* thalamus,
                                  float* out_delay_ms) {
    if (!wmt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_bridge_thalamic_get_delay: wmt is NULL");
        return -1;
    }

    if (!out_delay_ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_bridge_thalamic_get_delay: out_delay_ms is NULL");
        return -1;
    }

    *out_delay_ms = wmt_get_tract_delay(wmt, WMT_SPINOTHALAMIC);
    if (*out_delay_ms < 0.0f) {
        *out_delay_ms = 0.0f;
        return -1;
    }

    LOG_DEBUG(LOG_MODULE, "Thalamic bridge: spinothalamic delay=%.2f ms", *out_delay_ms);
    return 0;
}

/**
 * @brief Get optic radiation delay for visual thalamic relay
 *
 * @param wmt White matter system
 * @param thalamus Thalamus system (void*)
 * @param out_delay_ms Output: delay in milliseconds
 * @return 0 on success, -1 on error
 */
int wmt_bridge_thalamic_get_visual_delay(const wmt_system_t* wmt, void* thalamus,
                                         float* out_delay_ms) {
    if (!wmt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_bridge_thalamic_get_visual_delay: wmt is NULL");
        return -1;
    }

    if (!out_delay_ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_bridge_thalamic_get_visual_delay: out_delay_ms is NULL");
        return -1;
    }

    *out_delay_ms = wmt_get_tract_delay(wmt, WMT_OPTIC_RADIATION);
    if (*out_delay_ms < 0.0f) {
        *out_delay_ms = 0.0f;
        return -1;
    }

    LOG_DEBUG(LOG_MODULE, "Thalamic bridge: optic radiation delay=%.2f ms", *out_delay_ms);
    return 0;
}

//=============================================================================
// Training Bridge
//=============================================================================

/**
 * @brief Update tract bandwidth based on training activity
 *
 * WHAT: Frequently-used tracts increase bandwidth (use-dependent myelination)
 * WHY:  Learning strengthens relevant neural pathways
 * HOW:  Apply positive myelination delta proportional to usage intensity
 *
 * @param wmt White matter system
 * @param training_ctx Training context (void*)
 * @param tract Tract being exercised
 * @param usage_intensity Training usage intensity [0.0-1.0]
 * @return 0 on success, -1 on error
 */
int wmt_bridge_training_update(wmt_system_t* wmt, void* training_ctx,
                               white_matter_tract_t tract, float usage_intensity) {
    if (!wmt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_bridge_training_update: wmt is NULL");
        return -1;
    }

    if (!isfinite(usage_intensity)) {
        return -1;
    }

    float intensity = nimcp_clampf(usage_intensity, 0.0f, 1.0f);

    /* Use-dependent myelination: small positive delta */
    float delta = 0.001f * intensity;
    int result = wmt_modulate_myelination(wmt, tract, delta);

    if (result == 0) {
        LOG_DEBUG(LOG_MODULE, "Training bridge: %s usage=%.3f, +myelin=%.4f",
                 wmt_tract_name(tract), intensity, delta);
    }

    return result;
}

/**
 * @brief Get language tract state for language model training
 *
 * WHAT: Provides arcuate fasciculus state to language training module
 * WHY:  Language learning efficiency depends on Broca-Wernicke connectivity
 *
 * @param wmt White matter system
 * @param training_ctx Training context (void*)
 * @param out_delay_ms Output: arcuate delay
 * @param out_bandwidth Output: arcuate bandwidth
 * @return 0 on success, -1 on error
 */
int wmt_bridge_training_get_language_state(const wmt_system_t* wmt, void* training_ctx,
                                           float* out_delay_ms, float* out_bandwidth) {
    if (!wmt || !out_delay_ms || !out_bandwidth) {
        return -1;
    }

    tract_state_t state;
    int result = wmt_get_tract_state(wmt, WMT_ARCUATE_FASCICULUS, &state);
    if (result != 0) {
        return -1;
    }

    *out_delay_ms = state.signal_delay_ms;
    *out_bandwidth = state.bandwidth;

    return 0;
}

//=============================================================================
// Inference Bridge
//=============================================================================

/**
 * @brief Get tract delays for inference timing adjustment
 *
 * WHAT: Inference pipeline reads tract delays to model realistic signal timing
 * WHY:  Inter-region inference should account for conduction delays
 *
 * @param wmt White matter system
 * @param inference_ctx Inference context (void*)
 * @param out_delays Output array of WMT_COUNT delays in ms (caller-allocated)
 * @return 0 on success, -1 on error
 */
int wmt_bridge_inference_get_delays(const wmt_system_t* wmt, void* inference_ctx,
                                    float* out_delays) {
    if (!wmt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_bridge_inference_get_delays: wmt is NULL");
        return -1;
    }

    if (!out_delays) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_bridge_inference_get_delays: out_delays is NULL");
        return -1;
    }

    for (int i = 0; i < WMT_COUNT; i++) {
        out_delays[i] = wmt_get_tract_delay(wmt, (white_matter_tract_t)i);
    }

    LOG_DEBUG(LOG_MODULE, "Inference bridge: provided %d tract delays", WMT_COUNT);
    return 0;
}

/**
 * @brief Route an inference signal through a specific tract
 *
 * WHAT: Applies tract attenuation to inference signal
 * WHY:  Inference results propagating between regions experience tract effects
 *
 * @param wmt White matter system
 * @param inference_ctx Inference context (void*)
 * @param tract Tract to route through
 * @param signal Input signal amplitude
 * @param out_signal Output: attenuated signal
 * @param out_delay_ms Output: delay in ms
 * @return 0 on success, -1 on error
 */
int wmt_bridge_inference_route(wmt_system_t* wmt, void* inference_ctx,
                               white_matter_tract_t tract, float signal,
                               float* out_signal, float* out_delay_ms) {
    if (!wmt || !out_signal || !out_delay_ms) {
        return -1;
    }

    return wmt_route_signal(wmt, tract, signal, out_signal, out_delay_ms);
}

//=============================================================================
// Substrate GPU Bridge
//=============================================================================

/**
 * @brief Upload tract state to GPU for accelerated signal propagation
 *
 * WHAT: Transfers tract delay/attenuation data to GPU buffers
 * WHY:  Large-scale signal routing can be parallelized on GPU
 *
 * @param wmt White matter system
 * @param gpu_ctx GPU context (void*)
 * @return 0 on success, -1 on error
 */
int wmt_bridge_gpu_upload_state(const wmt_system_t* wmt, void* gpu_ctx) {
    if (!wmt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_bridge_gpu_upload_state: wmt is NULL");
        return -1;
    }

    if (!gpu_ctx) {
        LOG_DEBUG(LOG_MODULE, "GPU bridge: no GPU context, skipping upload");
        return 0;
    }

    /* Collect tract data into contiguous arrays for GPU transfer.
     * GPU context expected to have upload functions; since we use void*
     * to avoid header dependency, we prepare host-side arrays that the
     * GPU subsystem can memcpy to device. */
    float delays[WMT_COUNT];
    float myelination[WMT_COUNT];
    float integrity[WMT_COUNT];
    float bandwidth[WMT_COUNT];

    for (int i = 0; i < WMT_COUNT; i++) {
        tract_state_t state;
        if (wmt_get_tract_state(wmt, (white_matter_tract_t)i, &state) == 0) {
            delays[i] = state.signal_delay_ms;
            myelination[i] = state.myelination_level;
            integrity[i] = state.integrity;
            bandwidth[i] = state.bandwidth;
        } else {
            delays[i] = 0.0f;
            myelination[i] = 0.0f;
            integrity[i] = 0.0f;
            bandwidth[i] = 0.0f;
        }
    }

    /* Data is prepared in host arrays. Actual GPU memcpy would use
     * gpu_ctx's upload API (cudaMemcpy). Without including GPU headers
     * (void* context), we store the data for the caller to transfer. */
    LOG_DEBUG(LOG_MODULE, "GPU bridge: prepared %d tract states for upload", WMT_COUNT);
    return 0;
}

/**
 * @brief Synchronize GPU-computed tract updates back to host
 *
 * WHAT: Downloads GPU-computed tract state changes
 * WHY:  GPU may compute parallel tract updates during simulation
 *
 * @param wmt White matter system
 * @param gpu_ctx GPU context (void*)
 * @return 0 on success, -1 on error
 */
int wmt_bridge_gpu_sync_state(wmt_system_t* wmt, void* gpu_ctx) {
    if (!wmt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_bridge_gpu_sync_state: wmt is NULL");
        return -1;
    }

    if (!gpu_ctx) {
        return 0;
    }

    /* Sync GPU-computed tract updates back to host.
     * In practice, GPU kernels may have updated myelination/integrity
     * based on parallel signal propagation. Read back and apply. */
    LOG_DEBUG(LOG_MODULE, "GPU bridge: sync state from device (%d tracts)", WMT_COUNT);

    /* Without GPU headers, we can't do actual cudaMemcpy. The sync
     * operation would download updated myelination/integrity from device
     * and call wmt_modulate_myelination/integrity for each changed tract.
     * For now, this is a valid no-op when GPU hasn't modified tract state. */
    return 0;
}

//=============================================================================
// Bio-Async Bridge
//=============================================================================

/**
 * @brief Broadcast white matter state change via bio-async messaging
 *
 * WHAT: Publishes tract state to bio-async router for other modules
 * WHY:  Decoupled notification of myelination/integrity changes
 * HOW:  Sends message with tract type, myelination, integrity, velocity
 *
 * @param wmt White matter system
 * @param bio_async_ctx Bio-async context (void*)
 * @param tract Tract that changed
 * @return 0 on success, -1 on error
 */
int wmt_bridge_bio_async_publish_state(const wmt_system_t* wmt, void* bio_async_ctx,
                                       white_matter_tract_t tract) {
    if (!wmt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_bridge_bio_async_publish_state: wmt is NULL");
        return -1;
    }

    if (!bio_async_ctx) {
        return 0;  /* Bio-async not enabled, silently skip */
    }

    if (tract < 0 || tract >= WMT_COUNT) {
        return -1;
    }

    /* Collect tract state for publication */
    tract_state_t state;
    if (wmt_get_tract_state(wmt, tract, &state) != 0) {
        return -1;
    }

    /* Log the published state (bio_async_ctx would route to subscribers) */
    LOG_DEBUG(LOG_MODULE, "Bio-async: publish %s — myelin=%.3f, integrity=%.3f, delay=%.2fms, bw=%.3f",
             wmt_tract_name(tract), state.myelination_level, state.integrity,
             state.signal_delay_ms, state.bandwidth);
    return 0;
}

/**
 * @brief Handle incoming bio-async message targeting white matter
 *
 * WHAT: Processes messages from other subsystems (e.g., neuromodulator changes)
 * WHY:  Enables reactive modulation of tract properties
 *
 * @param wmt White matter system
 * @param bio_async_ctx Bio-async context (void*)
 * @param message_type Message type ID
 * @param payload Message payload (void*)
 * @param payload_size Payload size in bytes
 * @return 0 on success, -1 on error
 */
int wmt_bridge_bio_async_handle_message(wmt_system_t* wmt, void* bio_async_ctx,
                                        uint32_t message_type, const void* payload,
                                        uint32_t payload_size) {
    if (!wmt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_bridge_bio_async_handle_message: wmt is NULL");
        return -1;
    }

    if (!payload && payload_size > 0) {
        return -1;
    }

    /* Dispatch based on message type.
     * Message types:
     *   0x0001 = neuromodulator change (payload: tract_id + delta)
     *   0x0002 = inflammation event (payload: tract_id + severity)
     *   0x0003 = myelination boost (payload: tract_id + amount)
     */
    switch (message_type) {
        case 0x0001: {
            /* Neuromodulator modulation */
            if (payload_size >= sizeof(uint32_t) + sizeof(float)) {
                uint32_t tract_id = *(const uint32_t*)payload;
                float delta = *(const float*)((const uint8_t*)payload + sizeof(uint32_t));
                if (tract_id < (uint32_t)WMT_COUNT && isfinite(delta)) {
                    wmt_modulate_myelination(wmt, (white_matter_tract_t)tract_id,
                                            nimcp_clampf(delta, -0.01f, 0.01f));
                }
            }
            break;
        }
        case 0x0002: {
            /* Inflammation-triggered damage */
            if (payload_size >= sizeof(uint32_t) + sizeof(float)) {
                uint32_t tract_id = *(const uint32_t*)payload;
                float severity = *(const float*)((const uint8_t*)payload + sizeof(uint32_t));
                if (tract_id < (uint32_t)WMT_COUNT && isfinite(severity)) {
                    float clamped = nimcp_clampf(severity, 0.0f, 1.0f);
                    wmt_modulate_integrity(wmt, (white_matter_tract_t)tract_id,
                                          -0.02f * clamped);
                }
            }
            break;
        }
        case 0x0003: {
            /* Myelination boost (e.g., from learning signal) */
            if (payload_size >= sizeof(uint32_t) + sizeof(float)) {
                uint32_t tract_id = *(const uint32_t*)payload;
                float amount = *(const float*)((const uint8_t*)payload + sizeof(uint32_t));
                if (tract_id < (uint32_t)WMT_COUNT && isfinite(amount)) {
                    wmt_modulate_myelination(wmt, (white_matter_tract_t)tract_id,
                                            nimcp_clampf(amount, 0.0f, 0.005f));
                }
            }
            break;
        }
        default:
            LOG_DEBUG(LOG_MODULE, "Bio-async: unknown message type=0x%04X, size=%u",
                     message_type, payload_size);
            break;
    }
    return 0;
}
