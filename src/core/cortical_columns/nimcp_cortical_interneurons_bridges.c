/**
 * @file nimcp_cortical_interneurons_bridges.c
 * @brief Cortical Interneuron System - Bridge Integrations
 * @version 1.0.0
 * @date 2026-03-05
 *
 * WHAT: Integration bridges connecting cortical interneurons to other NIMCP subsystems.
 * WHY:  Interneurons interact with plasticity (STDP modulation), training (E/I homeostasis),
 *       inference (gamma-gated readout), thalamic TRN (reticular nucleus inhibition),
 *       immune (neuroinflammation), substrate GPU (parallel interneuron simulation),
 *       and bio-async (state broadcasting).
 * HOW:  STUB implementations that provide the integration interface. Each bridge function
 *       validates inputs, logs the call, and returns success. Actual integration logic
 *       will be wired when the corresponding subsystems are connected during brain init.
 *
 * BIOLOGICAL BASIS:
 * - Interneurons receive neuromodulatory input (ACh, DA, 5-HT) affecting plasticity
 * - E/I balance is maintained homeostatically during training
 * - Gamma oscillations gate information readout during inference
 * - TRN (thalamic reticular nucleus) is primarily inhibitory, works with cortical interneurons
 * - Neuroinflammation disrupts interneuron function (especially PV cells)
 */

#include "core/cortical_columns/nimcp_cortical_interneurons.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <string.h>

#define LOG_MODULE "CORTICAL_INTERNEURONS_BRIDGES"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(cortical_interneurons_bridges, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Forward declaration */
float cint_bridge_get_inhibitory_tone(const cortical_interneuron_system_t* system);

/* ============================================================================
 * Cortical Columns Bridge
 * ============================================================================ */

/**
 * @brief Connect interneuron system to cortical columns subsystem
 *
 * WHAT: Register interneuron system with cortical column pool
 * WHY:  Interneurons operate within cortical columns, providing local inhibition
 * HOW:  STUB - logs registration and returns success
 *
 * @param system Interneuron system
 * @param column_pool Cortical column pool (opaque pointer)
 * @return 0 on success, -1 on error
 */
int cint_bridge_connect_cortical_columns(cortical_interneuron_system_t* system,
                                          void* column_pool)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_bridge_connect_cortical_columns: system is NULL");
        return -1;
    }

    if (!column_pool) {
        LOG_INFO(LOG_MODULE, "Cortical columns bridge: column_pool is NULL, skipping");
        return 0;
    }

    LOG_INFO(LOG_MODULE, "Connected interneurons to cortical columns: %u interneurons, E/I=%.2f",
             system->num_interneurons, system->ei_balance);
    return 0;
}

/**
 * @brief Disconnect interneuron system from cortical columns
 *
 * @param system Interneuron system
 * @return 0 on success, -1 on error
 */
int cint_bridge_disconnect_cortical_columns(cortical_interneuron_system_t* system)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_bridge_disconnect_cortical_columns: system is NULL");
        return -1;
    }

    LOG_INFO(LOG_MODULE, "Disconnected interneurons from cortical columns");
    return 0;
}

/* ============================================================================
 * STDP / Plasticity Bridge
 * ============================================================================ */

/**
 * @brief Connect interneuron system to STDP/plasticity subsystem
 *
 * WHAT: Register for plasticity modulation signals
 * WHY:  Interneuron-mediated inhibition gates STDP windows and plasticity rules
 *       PV cells sharpen STDP timing windows; SST cells modulate dendritic plasticity
 * HOW:  STUB - will wire to plasticity coordinator when available
 *
 * @param system Interneuron system
 * @param plasticity_ctx Plasticity context (opaque pointer)
 * @return 0 on success, -1 on error
 */
int cint_bridge_connect_plasticity(cortical_interneuron_system_t* system,
                                    void* plasticity_ctx)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_bridge_connect_plasticity: system is NULL");
        return -1;
    }

    if (!plasticity_ctx) {
        LOG_INFO(LOG_MODULE, "Plasticity bridge: plasticity_ctx is NULL, skipping");
        return 0;
    }

    LOG_INFO(LOG_MODULE, "Connected interneurons to STDP/plasticity: gamma=%.3f, E/I=%.2f",
             system->gamma_power, system->ei_balance);
    return 0;
}

/**
 * @brief Get STDP modulation factor from interneuron state
 *
 * WHAT: Compute how interneuron activity should modulate STDP learning
 * WHY:  PV basket cell gamma oscillations gate STDP timing windows
 * HOW:  Returns modulation factor based on gamma power and E/I balance
 *
 * @param system Interneuron system
 * @return Modulation factor [0.0-2.0], 1.0 = no modulation
 */
float cint_bridge_get_stdp_modulation(const cortical_interneuron_system_t* system)
{
    if (!system) return 1.0f;

    /* Higher gamma power -> sharper STDP windows -> stronger modulation */
    float gamma_factor = 1.0f + system->gamma_power * 0.5f;

    /* E/I imbalance reduces plasticity (protective mechanism) */
    float ei_dev = fabsf(system->ei_balance - system->config.target_ei_ratio);
    float ei_factor = 1.0f / (1.0f + ei_dev * 0.5f);

    float modulation = gamma_factor * ei_factor;
    if (!isfinite(modulation)) return 1.0f;

    /* Clamp to [0.1, 2.0] */
    if (modulation < 0.1f) modulation = 0.1f;
    if (modulation > 2.0f) modulation = 2.0f;

    return modulation;
}

/* ============================================================================
 * Training Bridge
 * ============================================================================ */

/**
 * @brief Connect interneuron system to training subsystem
 *
 * WHAT: Register for training-time E/I homeostasis
 * WHY:  Training can disrupt E/I balance; interneurons must adapt to maintain stability
 * HOW:  STUB - will register with training dispatch when available
 *
 * @param system Interneuron system
 * @param training_ctx Training context (opaque pointer)
 * @return 0 on success, -1 on error
 */
int cint_bridge_connect_training(cortical_interneuron_system_t* system,
                                  void* training_ctx)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_bridge_connect_training: system is NULL");
        return -1;
    }

    if (!training_ctx) {
        LOG_INFO(LOG_MODULE, "Training bridge: training_ctx is NULL, skipping");
        return 0;
    }

    LOG_INFO(LOG_MODULE, "Connected interneurons to training: %u neurons, target_ei=%.1f",
             system->num_interneurons, system->config.target_ei_ratio);
    return 0;
}

/**
 * @brief Notify interneurons of training batch completion
 *
 * WHAT: Post-batch E/I balance adjustment
 * WHY:  Training weight updates may shift E/I balance; interneurons compensate
 * HOW:  STUB - would adjust inhibitory strengths based on post-batch E/I measurement
 *
 * @param system Interneuron system
 * @param batch_loss Current batch loss
 * @param batch_accuracy Current batch accuracy
 * @return 0 on success, -1 on error
 */
int cint_bridge_training_post_batch(cortical_interneuron_system_t* system,
                                     float batch_loss,
                                     float batch_accuracy)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_bridge_training_post_batch: system is NULL");
        return -1;
    }

    /* Post-batch E/I homeostasis: adjust inhibitory strength to maintain target ratio.
     * High loss → increase inhibition slightly (regularization effect)
     * E/I deviation → correct toward target */
    float ei_error = system->ei_balance - system->config.target_ei_ratio;
    float correction = -0.01f * ei_error;  /* Negative feedback: push toward target */

    /* High loss increases inhibition (prevents runaway excitation) */
    if (isfinite(batch_loss) && batch_loss > 1.0f) {
        correction += 0.005f * fminf(batch_loss, 10.0f);
    }

    /* Apply correction to all interneuron inhibition strengths */
    for (uint32_t i = 0; i < system->num_interneurons; i++) {
        system->interneurons[i].inhibition_strength += correction;
        if (system->interneurons[i].inhibition_strength < 0.0f)
            system->interneurons[i].inhibition_strength = 0.0f;
        if (system->interneurons[i].inhibition_strength > 1.0f)
            system->interneurons[i].inhibition_strength = 1.0f;
    }

    return 0;
}

/* ============================================================================
 * Inference Bridge
 * ============================================================================ */

/**
 * @brief Connect interneuron system to inference pipeline
 *
 * WHAT: Register for gamma-gated inference readout
 * WHY:  Gamma oscillations from PV basket cells gate the timing of inference readout
 * HOW:  STUB - will wire to inference coordinator when available
 *
 * @param system Interneuron system
 * @param inference_ctx Inference context (opaque pointer)
 * @return 0 on success, -1 on error
 */
int cint_bridge_connect_inference(cortical_interneuron_system_t* system,
                                   void* inference_ctx)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_bridge_connect_inference: system is NULL");
        return -1;
    }

    if (!inference_ctx) {
        LOG_INFO(LOG_MODULE, "Inference bridge: inference_ctx is NULL, skipping");
        return 0;
    }

    LOG_INFO(LOG_MODULE, "Connected interneurons to inference: gamma_gate=%.3f",
             system->gamma_power);
    return 0;
}

/**
 * @brief Get inference readout gate from gamma oscillations
 *
 * WHAT: Determine if gamma phase allows inference readout
 * WHY:  Information is read out at specific phases of gamma oscillation
 * HOW:  Returns gate factor based on current gamma power
 *
 * @param system Interneuron system
 * @return Gate factor [0.0-1.0], 1.0 = fully open
 */
float cint_bridge_get_inference_gate(const cortical_interneuron_system_t* system)
{
    if (!system) return 1.0f;

    /* Higher gamma power -> more structured readout -> higher gate */
    return system->gamma_power;
}

/* ============================================================================
 * Thalamic TRN Bridge
 * ============================================================================ */

/**
 * @brief Connect interneuron system to thalamic reticular nucleus (TRN)
 *
 * WHAT: Integrate cortical interneurons with thalamic inhibitory circuits
 * WHY:  TRN is a sheet of inhibitory neurons surrounding the thalamus;
 *       cortical interneurons and TRN form a distributed inhibitory network
 *       that gates thalamocortical information flow
 * HOW:  STUB - will wire to thalamic bridge when available
 *
 * @param system Interneuron system
 * @param trn_ctx Thalamic reticular nucleus context (opaque pointer)
 * @return 0 on success, -1 on error
 */
int cint_bridge_connect_thalamic_trn(cortical_interneuron_system_t* system,
                                      void* trn_ctx)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_bridge_connect_thalamic_trn: system is NULL");
        return -1;
    }

    if (!trn_ctx) {
        LOG_INFO(LOG_MODULE, "Thalamic TRN bridge: trn_ctx is NULL, skipping");
        return 0;
    }

    LOG_INFO(LOG_MODULE, "Connected interneurons to thalamic TRN: inhibitory_tone=%.3f",
             cint_bridge_get_inhibitory_tone(system));
    return 0;
}

/**
 * @brief Get cortical inhibitory tone for TRN modulation
 *
 * WHAT: Provide cortical inhibitory state to TRN for feedback modulation
 * WHY:  TRN adjusts thalamocortical gating based on cortical inhibitory state
 * HOW:  Returns aggregate inhibitory tone from all interneuron types
 *
 * @param system Interneuron system
 * @return Inhibitory tone [0.0-1.0]
 */
float cint_bridge_get_inhibitory_tone(const cortical_interneuron_system_t* system)
{
    if (!system || system->num_interneurons == 0) return 0.0f;

    float total = 0.0f;
    for (uint32_t i = 0; i < system->num_interneurons; i++) {
        total += system->interneurons[i].inhibition_strength;
    }

    float tone = total / (float)system->num_interneurons;
    if (!isfinite(tone)) return 0.0f;
    if (tone < 0.0f) tone = 0.0f;
    if (tone > 1.0f) tone = 1.0f;
    return tone;
}

/* ============================================================================
 * Bio-Async Bridge
 * ============================================================================ */

/**
 * @brief Connect interneuron system to bio-async router
 *
 * WHAT: Register with bio-async messaging system for state broadcasting
 * WHY:  Other modules need to know about E/I balance, gamma power, disinhibition
 * HOW:  STUB - will register with bio_router when available
 *
 * @param system Interneuron system
 * @return 0 on success, -1 on error
 */
int cint_bridge_connect_bio_async(cortical_interneuron_system_t* system)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_bridge_connect_bio_async: system is NULL");
        return -1;
    }

    LOG_INFO(LOG_MODULE, "Connected interneurons to bio-async: %u neurons",
             system->num_interneurons);
    return 0;
}

/**
 * @brief Disconnect interneuron system from bio-async router
 *
 * @param system Interneuron system
 * @return 0 on success, -1 on error
 */
int cint_bridge_disconnect_bio_async(cortical_interneuron_system_t* system)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_bridge_disconnect_bio_async: system is NULL");
        return -1;
    }

    LOG_INFO(LOG_MODULE, "Disconnected interneurons from bio-async");
    return 0;
}

/* ============================================================================
 * Immune Bridge
 * ============================================================================ */

/**
 * @brief Connect interneuron system to brain immune system
 *
 * WHAT: Register for neuroinflammation signals
 * WHY:  Neuroinflammation preferentially affects PV interneurons, disrupting
 *       gamma oscillations and E/I balance (implicated in schizophrenia, autism)
 * HOW:  STUB - will register with immune bridge coordinator when available
 *
 * @param system Interneuron system
 * @param immune_ctx Brain immune system context (opaque pointer)
 * @return 0 on success, -1 on error
 */
int cint_bridge_connect_immune(cortical_interneuron_system_t* system,
                                void* immune_ctx)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_bridge_connect_immune: system is NULL");
        return -1;
    }

    if (!immune_ctx) {
        LOG_INFO(LOG_MODULE, "Immune bridge: immune_ctx is NULL, skipping");
        return 0;
    }

    LOG_INFO(LOG_MODULE, "Connected interneurons to immune system: %u neurons",
             system->num_interneurons);
    return 0;
}

/**
 * @brief Apply inflammation modulation to interneurons
 *
 * WHAT: Reduce interneuron function in response to neuroinflammation
 * WHY:  Pro-inflammatory cytokines impair PV cell function and reduce gamma
 * HOW:  STUB - would reduce PV firing capacity proportional to inflammation level
 *
 * @param system Interneuron system
 * @param inflammation_level Inflammation level [0.0-1.0]
 * @return 0 on success, -1 on error
 */
int cint_bridge_apply_inflammation(cortical_interneuron_system_t* system,
                                    float inflammation_level)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_bridge_apply_inflammation: system is NULL");
        return -1;
    }

    if (!isfinite(inflammation_level)) return -1;

    /* Clamp */
    if (inflammation_level < 0.0f) inflammation_level = 0.0f;
    if (inflammation_level > 1.0f) inflammation_level = 1.0f;

    /* Neuroinflammation preferentially affects PV interneurons.
     * Reduce PV cell inhibition strength and gamma power proportionally. */
    float attenuation = 1.0f - 0.5f * inflammation_level;  /* Max 50% reduction */

    for (uint32_t i = 0; i < system->num_interneurons; i++) {
        if (system->interneurons[i].type == CINT_PV_BASKET ||
            system->interneurons[i].type == CINT_PV_CHANDELIER) {
            system->interneurons[i].inhibition_strength *= attenuation;
        }
    }

    /* Reduce gamma power (PV cells drive gamma oscillations) */
    system->gamma_power *= attenuation;
    if (system->gamma_power < 0.0f) system->gamma_power = 0.0f;

    LOG_DEBUG(LOG_MODULE, "Applied inflammation %.3f: PV attenuation=%.3f, gamma=%.3f",
             inflammation_level, attenuation, system->gamma_power);

    return 0;
}

/* ============================================================================
 * Substrate GPU Bridge
 * ============================================================================ */

/**
 * @brief Connect interneuron system to substrate GPU for parallel simulation
 *
 * WHAT: Register interneuron arrays for GPU-accelerated parallel update
 * WHY:  Large interneuron populations benefit from SIMD/GPU parallelism
 * HOW:  STUB - will upload interneuron state to GPU weight cache when available
 *
 * @param system Interneuron system
 * @param gpu_ctx GPU context (opaque pointer)
 * @return 0 on success, -1 on error
 */
int cint_bridge_connect_substrate_gpu(cortical_interneuron_system_t* system,
                                       void* gpu_ctx)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_bridge_connect_substrate_gpu: system is NULL");
        return -1;
    }

    if (!gpu_ctx) {
        LOG_INFO(LOG_MODULE, "Substrate GPU bridge: gpu_ctx is NULL, skipping");
        return 0;
    }

    LOG_INFO(LOG_MODULE, "Connected interneurons to substrate GPU: %u neurons for parallel simulation",
             system->num_interneurons);
    return 0;
}

/**
 * @brief Synchronize interneuron state from GPU
 *
 * WHAT: Download updated interneuron state from GPU to host
 * WHY:  After GPU-accelerated update, host needs current state for query APIs
 * HOW:  STUB - would call nimcp_gpu_tensor_to_host for interneuron arrays
 *
 * @param system Interneuron system
 * @return 0 on success, -1 on error
 */
int cint_bridge_sync_from_gpu(cortical_interneuron_system_t* system)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_bridge_sync_from_gpu: system is NULL");
        return -1;
    }

    /* Sync GPU-computed interneuron states back to host.
     * In production, this would cudaMemcpy updated firing rates and
     * inhibition strengths from GPU buffers to system->interneurons. */
    LOG_DEBUG(LOG_MODULE, "GPU sync: %u interneurons", system->num_interneurons);
    return 0;
}
