/**
 * @file nimcp_endocannabinoid_bridges.c
 * @brief Endocannabinoid System Bridge Integrations (STUB)
 * @date 2026-03-05
 *
 * Stub bridge implementations for integrating the ECS with other brain subsystems:
 * - SNN/STDP: retrograde modulation of spike-timing-dependent plasticity
 * - Training: reward dampening via CB1 presynaptic suppression
 * - Inference: tonic gain modulation
 * - Immune: CB2 anti-inflammatory pathway
 * - Hypothalamus: appetite regulation via CB1
 * - Somatosensory: pain gate modulation
 * - Bio-async: asynchronous state broadcast
 * - Substrate GPU: GPU-accelerated ECS dynamics
 */

#include "core/brain/regions/endocannabinoid/nimcp_endocannabinoid.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#include <string.h>
#include <math.h>
#include <time.h>

#define LOG_MODULE "ENDOCANNABINOID_BRIDGES"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(endocannabinoid_bridges, MESH_ADAPTER_CATEGORY_COGNITIVE)

static float nimcp_clampf_br(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/*=============================================================================
 * SNN/STDP Bridge: Retrograde Modulation of Plasticity
 *===========================================================================*/

/**
 * @brief Modulate STDP learning rate based on ECS retrograde signal
 *
 * 2-AG retrograde signaling suppresses presynaptic release, which
 * attenuates STDP potentiation at active synapses (DSI/DSE effect).
 *
 * @param system ECS instance
 * @param base_stdp_rate Base STDP learning rate
 * @param modulated_rate Output: ECS-modulated STDP rate
 * @return 0 on success, -1 on error
 */
int ecb_bridge_snn_modulate_stdp(endocannabinoid_system_t* system,
                                  float base_stdp_rate,
                                  float* modulated_rate) {
    if (!system || !modulated_rate) {
        if (modulated_rate) *modulated_rate = base_stdp_rate;
        return -1;
    }

    /* Stub: retrograde suppression attenuates STDP */
    float suppression = ecb_get_presynaptic_suppression(system, 0);
    *modulated_rate = base_stdp_rate * (1.0f - suppression * 0.5f);
    *modulated_rate = nimcp_clampf_br(*modulated_rate, 0.0f, base_stdp_rate);

    return 0;
}

/**
 * @brief Get ECS retrograde weight for SNN spike propagation
 *
 * @param system ECS instance
 * @param pre_neuron_id Presynaptic neuron
 * @param post_neuron_id Postsynaptic neuron
 * @return Weight multiplier [0-1], 1.0 = no modulation
 */
float ecb_bridge_snn_get_retrograde_weight(endocannabinoid_system_t* system,
                                            uint32_t pre_neuron_id,
                                            uint32_t post_neuron_id) {
    if (!system) {
        return 1.0f;
    }

    /* Stub: use global suppression as proxy */
    float suppression = ecb_get_presynaptic_suppression(system, 0);
    (void)pre_neuron_id;
    (void)post_neuron_id;
    return 1.0f - suppression;
}

/*=============================================================================
 * Training Bridge: Reward Dampening
 *===========================================================================*/

/**
 * @brief Modulate reward signal through ECS dampening
 *
 * High 2-AG / tonic CB1 activation dampens reward signals,
 * preventing over-reinforcement and supporting homeostatic plasticity.
 *
 * @param system ECS instance
 * @param raw_reward Raw reward signal
 * @param dampened_reward Output: ECS-dampened reward
 * @return 0 on success, -1 on error
 */
int ecb_bridge_training_dampen_reward(endocannabinoid_system_t* system,
                                       float raw_reward,
                                       float* dampened_reward) {
    if (!system || !dampened_reward) {
        if (dampened_reward) *dampened_reward = raw_reward;
        return -1;
    }

    /* Stub: tonic inhibition dampens reward proportionally */
    float two_ag = ecb_get_retrograde_signal(system, ECB_2AG);
    if (two_ag < 0.0f) two_ag = 0.0f;

    float damping = two_ag * 0.3f;  /* Max 30% reward dampening */
    *dampened_reward = raw_reward * (1.0f - damping);

    return 0;
}

/**
 * @brief Get ECS-modulated learning rate factor
 *
 * @param system ECS instance
 * @return Learning rate multiplier [0.5-1.0]
 */
float ecb_bridge_training_get_lr_factor(endocannabinoid_system_t* system) {
    if (!system) {
        return 1.0f;
    }

    /* Stub: tonic inhibition reduces learning rate slightly */
    float aea = ecb_get_retrograde_signal(system, ECB_AEA);
    if (aea < 0.0f) aea = 0.0f;

    return nimcp_clampf_br(1.0f - aea * 0.3f, 0.5f, 1.0f);
}

/*=============================================================================
 * Inference Bridge: Tonic Gain Modulation
 *===========================================================================*/

/**
 * @brief Get tonic gain modulation for inference forward pass
 *
 * CB1 tonic inhibition modulates the gain of neural responses,
 * implementing a form of divisive normalization.
 *
 * @param system ECS instance
 * @param region_id Brain region index
 * @return Gain factor [0.5-1.0], lower = more suppressed
 */
float ecb_bridge_inference_get_tonic_gain(endocannabinoid_system_t* system,
                                           uint32_t region_id) {
    if (!system || system->magic != ECB_MAGIC) {
        return 1.0f;
    }
    if (region_id >= ECB_NUM_REGIONS) {
        region_id = 0;
    }

    /* Stub: gain inversely proportional to CB1 density * tonic inhibition */
    nimcp_mutex_lock((nimcp_mutex_t*)system->lock);
    float gain = 1.0f - system->cb1_density[region_id] * system->tonic_inhibition * 0.5f;
    nimcp_mutex_unlock((nimcp_mutex_t*)system->lock);

    return nimcp_clampf_br(gain, 0.5f, 1.0f);
}

/*=============================================================================
 * Immune Bridge: CB2 Anti-Inflammatory Pathway
 *===========================================================================*/

/**
 * @brief Get CB2-mediated anti-inflammatory signal
 *
 * CB2 receptor activation on microglia and immune cells reduces
 * neuroinflammation. Returns suppression factor for immune response.
 *
 * @param system ECS instance
 * @param inflammation_level Current inflammation level [0-1]
 * @param suppression_out Output: immune suppression factor [0-1]
 * @return 0 on success, -1 on error
 */
int ecb_bridge_immune_get_cb2_suppression(endocannabinoid_system_t* system,
                                           float inflammation_level,
                                           float* suppression_out) {
    if (!system || !suppression_out) {
        if (suppression_out) *suppression_out = 0.0f;
        return -1;
    }

    /* Stub: CB2 activation scales with AEA level and CB2 gain */
    float aea = ecb_get_retrograde_signal(system, ECB_AEA);
    if (aea < 0.0f) aea = 0.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)system->lock);
    float cb2_activation = aea * system->config.cb2_gain;
    nimcp_mutex_unlock((nimcp_mutex_t*)system->lock);

    *suppression_out = nimcp_clampf_br(cb2_activation * 0.4f, 0.0f, 0.6f);

    (void)inflammation_level;  /* Future: adaptive CB2 upregulation */
    return 0;
}

/*=============================================================================
 * Hypothalamus Bridge: Appetite Regulation
 *===========================================================================*/

/**
 * @brief Get CB1-mediated appetite drive signal
 *
 * Hypothalamic CB1 activation increases appetite drive.
 * 2-AG and AEA both contribute via lateral hypothalamic CB1.
 *
 * @param system ECS instance
 * @return Appetite drive [0-1], higher = more hunger
 */
float ecb_bridge_hypothalamus_get_appetite_drive(endocannabinoid_system_t* system) {
    if (!system || system->magic != ECB_MAGIC) {
        return 0.5f;  /* Neutral baseline */
    }

    nimcp_mutex_lock((nimcp_mutex_t*)system->lock);

    /* Stub: appetite driven by 2-AG and AEA acting on hypothalamic CB1 */
    float two_ag_component = system->two_ag_level * 0.6f;
    float aea_component    = system->aea_level * 0.3f;
    float appetite = two_ag_component + aea_component;
    appetite = nimcp_clampf_br(appetite, 0.0f, 1.0f);

    nimcp_mutex_unlock((nimcp_mutex_t*)system->lock);

    return appetite;
}

/*=============================================================================
 * Somatosensory Bridge: Pain Gate
 *===========================================================================*/

/**
 * @brief Apply ECS pain gate to somatosensory pain pathway
 *
 * Wraps ecb_modulate_pain for bridge-compatible interface.
 *
 * @param system ECS instance
 * @param raw_pain Raw pain signal [0-1]
 * @return Gated pain signal [0-1]
 */
float ecb_bridge_somatosensory_pain_gate(endocannabinoid_system_t* system,
                                          float raw_pain) {
    if (!system) {
        return raw_pain;
    }

    float modulated = raw_pain;
    int rc = ecb_modulate_pain(system, raw_pain, &modulated);
    if (rc != 0) {
        return raw_pain;
    }
    return modulated;
}

/*=============================================================================
 * Bio-Async Bridge: Asynchronous State Broadcast
 *===========================================================================*/

/**
 * @brief ECS state snapshot for bio-async broadcast
 */
typedef struct {
    float two_ag_level;
    float aea_level;
    float dsi_strength;
    float dse_strength;
    float tonic_inhibition;
    uint64_t timestamp_us;
} ecb_bio_async_state_t;

/**
 * @brief Capture current ECS state for bio-async messaging
 *
 * @param system ECS instance
 * @param state_out Output: state snapshot
 * @return 0 on success, -1 on error
 */
int ecb_bridge_bio_async_capture_state(endocannabinoid_system_t* system,
                                        ecb_bio_async_state_t* state_out) {
    if (!system || !state_out) {
        return -1;
    }
    if (system->magic != ECB_MAGIC) {
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)system->lock);

    state_out->two_ag_level     = system->two_ag_level;
    state_out->aea_level        = system->aea_level;
    state_out->dsi_strength     = system->dsi_strength;
    state_out->dse_strength     = system->dse_strength;
    state_out->tonic_inhibition = system->tonic_inhibition;
    state_out->timestamp_us     = system->last_update_us;

    nimcp_mutex_unlock((nimcp_mutex_t*)system->lock);

    return 0;
}

/*=============================================================================
 * Substrate GPU Bridge: GPU-Accelerated ECS (Stub)
 *===========================================================================*/

/**
 * @brief Upload ECS density maps to GPU for parallel computation
 *
 * @param system ECS instance
 * @param gpu_ctx GPU context handle (opaque)
 * @return 0 on success, -1 on error (or GPU not available)
 */
int ecb_bridge_gpu_upload_density_maps(endocannabinoid_system_t* system,
                                        void* gpu_ctx) {
    if (!system || !gpu_ctx) {
        return -1;
    }

    /* GPU density map upload: serialize ECS region densities for GPU processing.
     * Currently a no-op as GPU ECS kernels are not yet implemented.
     * When added, this will upload CB1/CB2 receptor density maps and
     * endocannabinoid concentration fields to GPU memory. */
    LOG_DEBUG(LOG_MODULE, "GPU density map upload: system ready, ctx=%p", gpu_ctx);
    return 0;
}

/**
 * @brief Run ECS update on GPU
 *
 * @param system ECS instance
 * @param gpu_ctx GPU context handle
 * @param dt_s Time step in seconds
 * @return 0 on success, -1 on error
 */
int ecb_bridge_gpu_update(endocannabinoid_system_t* system,
                           void* gpu_ctx,
                           float dt_s) {
    if (!system || !gpu_ctx) {
        return -1;
    }

    /* CPU fallback: run ECS update on CPU.
     * When GPU kernels are available, this will launch CUDA kernels
     * for parallel diffusion of AEA/2-AG across brain regions. */
    LOG_DEBUG(LOG_MODULE, "GPU ECS update: falling back to CPU (dt=%.4fs)", dt_s);
    (void)gpu_ctx;
    return ecb_update(system, dt_s);
}
