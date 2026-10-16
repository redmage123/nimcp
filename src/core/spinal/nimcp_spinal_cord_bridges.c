//=============================================================================
// nimcp_spinal_cord_bridges.c - Spinal Cord Bridge Integrations
//=============================================================================
/**
 * @file nimcp_spinal_cord_bridges.c
 * @brief Bridge integrations connecting spinal cord to other brain subsystems
 *
 * WHAT: Stub bridge implementations for spinal cord integration
 * WHY:  Spinal cord communicates with motor cortex, cerebellum, somatosensory,
 *       thalamus, training, substrate, bio-async, immune, and endocannabinoid
 * HOW:  Each bridge provides bidirectional data flow between spinal cord and
 *       the target subsystem using the NIMCP bridge pattern
 *
 * BRIDGE CATALOG:
 * 1. Motor cortex bridge:     Corticospinal commands -> motor pools
 * 2. White matter bridge:     Axonal tract connectivity
 * 3. Cerebellum bridge:       Motor coordination and timing
 * 4. Somatosensory bridge:    Proprioceptive feedback (Ia, II, Ib)
 * 5. Endocannabinoid bridge:  Gate control modulation
 * 6. Thalamic bridge:         Spinothalamic pain/temperature relay
 * 7. Training bridge:         Motor learning and adaptation
 * 8. Substrate GPU bridge:    GPU-accelerated motor pool computation
 * 9. Bio-async bridge:        Asynchronous inter-module messaging
 * 10. Immune bridge:          Neuroinflammation effects on motor output
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-03-05
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/spinal/nimcp_spinal_cord.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "SPINAL_BRIDGES"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(spinal_cord_bridges, MESH_ADAPTER_CATEGORY_MOTOR)

#include <string.h>
#include <math.h>

//=============================================================================
// Bridge Structures (Forward Declarations)
//=============================================================================

typedef struct spinal_motor_cortex_bridge {
    spinal_cord_t*  spinal;
    void*           motor_adapter;   /* motor_adapter_t* */
    bool            enabled;
} spinal_motor_cortex_bridge_t;

typedef struct spinal_white_matter_bridge {
    spinal_cord_t*  spinal;
    void*           white_matter;    /* white_matter_t* */
    bool            enabled;
} spinal_white_matter_bridge_t;

typedef struct spinal_cerebellum_bridge {
    spinal_cord_t*  spinal;
    void*           cerebellum;      /* cerebellum_adapter_t* */
    bool            enabled;
} spinal_cerebellum_bridge_t;

typedef struct spinal_somatosensory_bridge {
    spinal_cord_t*  spinal;
    void*           somatosensory;   /* nimcp_somatosensory_t* */
    bool            enabled;
} spinal_somatosensory_bridge_t;

typedef struct spinal_endocannabinoid_bridge {
    spinal_cord_t*  spinal;
    float           anandamide_level;   /* AEA concentration */
    float           two_ag_level;       /* 2-AG concentration */
    bool            enabled;
} spinal_endocannabinoid_bridge_t;

typedef struct spinal_thalamic_bridge {
    spinal_cord_t*  spinal;
    void*           thalamus;        /* thalamus_t* */
    bool            enabled;
} spinal_thalamic_bridge_t;

typedef struct spinal_training_bridge {
    spinal_cord_t*  spinal;
    void*           training_ctx;    /* nimcp_brain_training_ctx_t* */
    bool            enabled;
} spinal_training_bridge_t;

typedef struct spinal_substrate_gpu_bridge {
    spinal_cord_t*  spinal;
    void*           gpu_ctx;         /* substrate_gpu_context_t* */
    bool            enabled;
} spinal_substrate_gpu_bridge_t;

typedef struct spinal_bio_async_bridge {
    spinal_cord_t*  spinal;
    void*           bio_router;      /* bio_router_t* */
    bool            enabled;
} spinal_bio_async_bridge_t;

typedef struct spinal_immune_bridge {
    spinal_cord_t*  spinal;
    void*           immune_system;   /* brain_immune_system_t* */
    float           inflammation_factor;  /* [0.0-1.0], reduces motor output */
    bool            enabled;
} spinal_immune_bridge_t;

//=============================================================================
// 1. Motor Cortex Bridge
//=============================================================================

/**
 * @brief Create motor cortex bridge
 *
 * BIOLOGICAL: Corticospinal tract carries voluntary motor commands from
 * primary motor cortex (M1) to spinal motor neurons. The lateral
 * corticospinal tract (85% fibers) decussates at the pyramids.
 *
 * @param spinal  Spinal cord system
 * @param motor   Motor cortex adapter
 * @return Bridge instance, or NULL on failure
 */
spinal_motor_cortex_bridge_t* spinal_motor_cortex_bridge_create(
        spinal_cord_t* spinal, void* motor) {
    if (!spinal || !motor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_motor_cortex_bridge_create: NULL parameter");
        return NULL;
    }

    spinal_motor_cortex_bridge_t* bridge =
        (spinal_motor_cortex_bridge_t*)nimcp_calloc(1, sizeof(spinal_motor_cortex_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "spinal_motor_cortex_bridge_create: allocation failed");
        return NULL;
    }

    bridge->spinal = spinal;
    bridge->motor_adapter = motor;
    bridge->enabled = true;

    NIMCP_LOGGING_INFO("Motor cortex -> spinal cord bridge created");
    return bridge;
}

/**
 * @brief Update motor cortex bridge (transfer corticospinal commands)
 *
 * STUB: In full implementation, reads motor commands from motor adapter
 * and writes them to spinal corticospinal_input buffer.
 */
int spinal_motor_cortex_bridge_update(spinal_motor_cortex_bridge_t* bridge) {
    if (!bridge || !bridge->enabled) return 0;
    /* STUB: Transfer motor cortex output -> spinal corticospinal_input */
    (void)bridge;
    return 0;
}

void spinal_motor_cortex_bridge_destroy(spinal_motor_cortex_bridge_t* bridge) {
    nimcp_free(bridge);
}

//=============================================================================
// 2. White Matter Bridge
//=============================================================================

/**
 * @brief Create white matter bridge
 *
 * BIOLOGICAL: White matter tracts (corticospinal, rubrospinal, etc.)
 * are myelinated axon bundles connecting cortex to spinal cord.
 * Conduction velocity depends on myelination (60-120 m/s for Aalpha).
 */
spinal_white_matter_bridge_t* spinal_white_matter_bridge_create(
        spinal_cord_t* spinal, void* white_matter) {
    if (!spinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_white_matter_bridge_create: spinal is NULL");
        return NULL;
    }

    spinal_white_matter_bridge_t* bridge =
        (spinal_white_matter_bridge_t*)nimcp_calloc(1, sizeof(spinal_white_matter_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "spinal_white_matter_bridge_create: allocation failed");
        return NULL;
    }

    bridge->spinal = spinal;
    bridge->white_matter = white_matter;
    bridge->enabled = true;

    NIMCP_LOGGING_INFO("White matter -> spinal cord bridge created");
    return bridge;
}

int spinal_white_matter_bridge_update(spinal_white_matter_bridge_t* bridge) {
    if (!bridge || !bridge->enabled) return 0;
    /* STUB: Model conduction delay through white matter tracts */
    (void)bridge;
    return 0;
}

void spinal_white_matter_bridge_destroy(spinal_white_matter_bridge_t* bridge) {
    nimcp_free(bridge);
}

//=============================================================================
// 3. Cerebellum Bridge
//=============================================================================

/**
 * @brief Create cerebellum bridge
 *
 * BIOLOGICAL: Cerebellum receives efference copy of motor commands via
 * pontine nuclei (mossy fibers) and error signals via inferior olive
 * (climbing fibers). Returns corrective signals via deep nuclei -> thalamus.
 */
spinal_cerebellum_bridge_t* spinal_cerebellum_bridge_create(
        spinal_cord_t* spinal, void* cerebellum) {
    if (!spinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_cerebellum_bridge_create: spinal is NULL");
        return NULL;
    }

    spinal_cerebellum_bridge_t* bridge =
        (spinal_cerebellum_bridge_t*)nimcp_calloc(1, sizeof(spinal_cerebellum_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "spinal_cerebellum_bridge_create: allocation failed");
        return NULL;
    }

    bridge->spinal = spinal;
    bridge->cerebellum = cerebellum;
    bridge->enabled = true;

    NIMCP_LOGGING_INFO("Cerebellum -> spinal cord bridge created");
    return bridge;
}

int spinal_cerebellum_bridge_update(spinal_cerebellum_bridge_t* bridge) {
    if (!bridge || !bridge->enabled) return 0;
    /* STUB: Exchange efference copies and corrective signals */
    (void)bridge;
    return 0;
}

void spinal_cerebellum_bridge_destroy(spinal_cerebellum_bridge_t* bridge) {
    nimcp_free(bridge);
}

//=============================================================================
// 4. Somatosensory Bridge
//=============================================================================

/**
 * @brief Create somatosensory bridge
 *
 * BIOLOGICAL: Proprioceptive afferents from muscle spindles and Golgi
 * tendon organs provide feedback to spinal interneurons and motor neurons.
 * - Ia afferents: dynamic stretch (velocity) -> excitatory to homonymous MN
 * - II afferents: static stretch (length) -> excitatory
 * - Ib afferents: tension -> inhibitory (autogenic inhibition)
 */
spinal_somatosensory_bridge_t* spinal_somatosensory_bridge_create(
        spinal_cord_t* spinal, void* somatosensory) {
    if (!spinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_somatosensory_bridge_create: spinal is NULL");
        return NULL;
    }

    spinal_somatosensory_bridge_t* bridge =
        (spinal_somatosensory_bridge_t*)nimcp_calloc(1, sizeof(spinal_somatosensory_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "spinal_somatosensory_bridge_create: allocation failed");
        return NULL;
    }

    bridge->spinal = spinal;
    bridge->somatosensory = somatosensory;
    bridge->enabled = true;

    NIMCP_LOGGING_INFO("Somatosensory -> spinal cord bridge created");
    return bridge;
}

int spinal_somatosensory_bridge_update(spinal_somatosensory_bridge_t* bridge) {
    if (!bridge || !bridge->enabled) return 0;
    /* STUB: Transfer proprioceptive signals to spinal afferent buffers */
    (void)bridge;
    return 0;
}

void spinal_somatosensory_bridge_destroy(spinal_somatosensory_bridge_t* bridge) {
    nimcp_free(bridge);
}

//=============================================================================
// 5. Endocannabinoid Gate Control Bridge
//=============================================================================

/**
 * @brief Create endocannabinoid gate control bridge
 *
 * BIOLOGICAL: Endocannabinoids (AEA, 2-AG) modulate spinal gate control
 * via CB1 receptors on inhibitory interneurons in laminae I-II of the
 * dorsal horn. This implements Melzack & Wall's gate control theory
 * with endocannabinoid modulation.
 */
spinal_endocannabinoid_bridge_t* spinal_endocannabinoid_bridge_create(
        spinal_cord_t* spinal) {
    if (!spinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_endocannabinoid_bridge_create: spinal is NULL");
        return NULL;
    }

    spinal_endocannabinoid_bridge_t* bridge =
        (spinal_endocannabinoid_bridge_t*)nimcp_calloc(1, sizeof(spinal_endocannabinoid_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "spinal_endocannabinoid_bridge_create: allocation failed");
        return NULL;
    }

    bridge->spinal = spinal;
    bridge->anandamide_level = 0.0f;
    bridge->two_ag_level = 0.0f;
    bridge->enabled = true;

    NIMCP_LOGGING_INFO("Endocannabinoid gate control bridge created");
    return bridge;
}

int spinal_endocannabinoid_bridge_update(spinal_endocannabinoid_bridge_t* bridge) {
    if (!bridge || !bridge->enabled || !bridge->spinal) return 0;

    /* Endocannabinoids close the gate (reduce pain transmission).
     * Higher levels -> lower gate_control_level -> less pain throughput.
     * gate = 1.0 - 0.5*(AEA + 2-AG), clamped to [0.1, 1.0] */
    float ecb_total = bridge->anandamide_level + bridge->two_ag_level;
    if (!isfinite(ecb_total)) {
        ecb_total = 0.0f;
    }
    float gate = 1.0f - 0.5f * ecb_total;
    if (gate < 0.1f) gate = 0.1f;
    if (gate > 1.0f) gate = 1.0f;

    nimcp_mutex_lock(bridge->spinal->lock);
    bridge->spinal->gate_control_level = gate;
    nimcp_mutex_unlock(bridge->spinal->lock);
    return 0;
}

void spinal_endocannabinoid_bridge_destroy(spinal_endocannabinoid_bridge_t* bridge) {
    nimcp_free(bridge);
}

//=============================================================================
// 6. Thalamic Spinothalamic Bridge
//=============================================================================

/**
 * @brief Create thalamic bridge (spinothalamic tract)
 *
 * BIOLOGICAL: The spinothalamic tract relays pain, temperature, and
 * crude touch from spinal dorsal horn (laminae I, V) to VPL/VPM
 * nuclei of the thalamus, then to S1 cortex. Ascending pathway
 * for conscious perception of nociceptive/thermal stimuli.
 */
spinal_thalamic_bridge_t* spinal_thalamic_bridge_create(
        spinal_cord_t* spinal, void* thalamus) {
    if (!spinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_thalamic_bridge_create: spinal is NULL");
        return NULL;
    }

    spinal_thalamic_bridge_t* bridge =
        (spinal_thalamic_bridge_t*)nimcp_calloc(1, sizeof(spinal_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "spinal_thalamic_bridge_create: allocation failed");
        return NULL;
    }

    bridge->spinal = spinal;
    bridge->thalamus = thalamus;
    bridge->enabled = true;

    NIMCP_LOGGING_INFO("Spinothalamic -> thalamic bridge created");
    return bridge;
}

int spinal_thalamic_bridge_update(spinal_thalamic_bridge_t* bridge) {
    if (!bridge || !bridge->enabled) return 0;
    /* STUB: Relay pain/temperature signals to thalamus VPL */
    (void)bridge;
    return 0;
}

void spinal_thalamic_bridge_destroy(spinal_thalamic_bridge_t* bridge) {
    nimcp_free(bridge);
}

//=============================================================================
// 7. Training Bridge
//=============================================================================

/**
 * @brief Create training bridge
 *
 * BIOLOGICAL: Motor learning involves adaptation of spinal circuits:
 * - H-reflex conditioning (operant conditioning of stretch reflex)
 * - CPG frequency adaptation during locomotor learning
 * - Use-dependent plasticity of spinal interneuron networks
 */
spinal_training_bridge_t* spinal_training_bridge_create(
        spinal_cord_t* spinal, void* training_ctx) {
    if (!spinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_training_bridge_create: spinal is NULL");
        return NULL;
    }

    spinal_training_bridge_t* bridge =
        (spinal_training_bridge_t*)nimcp_calloc(1, sizeof(spinal_training_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "spinal_training_bridge_create: allocation failed");
        return NULL;
    }

    bridge->spinal = spinal;
    bridge->training_ctx = training_ctx;
    bridge->enabled = true;

    NIMCP_LOGGING_INFO("Training -> spinal cord bridge created");
    return bridge;
}

int spinal_training_bridge_update(spinal_training_bridge_t* bridge) {
    if (!bridge || !bridge->enabled) return 0;
    /* STUB: Apply motor learning updates to CPG/reflex parameters */
    (void)bridge;
    return 0;
}

void spinal_training_bridge_destroy(spinal_training_bridge_t* bridge) {
    nimcp_free(bridge);
}

//=============================================================================
// 8. Substrate GPU Bridge
//=============================================================================

/**
 * @brief Create substrate GPU bridge
 *
 * PURPOSE: Offload motor pool computation to GPU for large-scale
 * simulations with many motor pools and neurons per pool.
 */
spinal_substrate_gpu_bridge_t* spinal_substrate_gpu_bridge_create(
        spinal_cord_t* spinal, void* gpu_ctx) {
    if (!spinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_substrate_gpu_bridge_create: spinal is NULL");
        return NULL;
    }

    spinal_substrate_gpu_bridge_t* bridge =
        (spinal_substrate_gpu_bridge_t*)nimcp_calloc(1, sizeof(spinal_substrate_gpu_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "spinal_substrate_gpu_bridge_create: allocation failed");
        return NULL;
    }

    bridge->spinal = spinal;
    bridge->gpu_ctx = gpu_ctx;
    bridge->enabled = (gpu_ctx != NULL);

    NIMCP_LOGGING_INFO("Substrate GPU -> spinal cord bridge created (gpu=%s)",
                       bridge->enabled ? "yes" : "no");
    return bridge;
}

int spinal_substrate_gpu_bridge_update(spinal_substrate_gpu_bridge_t* bridge) {
    if (!bridge || !bridge->enabled) return 0;
    /* STUB: Offload motor pool integration to GPU kernels */
    (void)bridge;
    return 0;
}

void spinal_substrate_gpu_bridge_destroy(spinal_substrate_gpu_bridge_t* bridge) {
    nimcp_free(bridge);
}

//=============================================================================
// 9. Bio-Async Bridge
//=============================================================================

/**
 * @brief Create bio-async bridge
 *
 * PURPOSE: Enables asynchronous message passing between spinal cord
 * and other brain modules via the bio-async router. Publishes motor
 * output events and subscribes to descending command events.
 */
spinal_bio_async_bridge_t* spinal_bio_async_bridge_create(
        spinal_cord_t* spinal, void* bio_router) {
    if (!spinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_bio_async_bridge_create: spinal is NULL");
        return NULL;
    }

    spinal_bio_async_bridge_t* bridge =
        (spinal_bio_async_bridge_t*)nimcp_calloc(1, sizeof(spinal_bio_async_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "spinal_bio_async_bridge_create: allocation failed");
        return NULL;
    }

    bridge->spinal = spinal;
    bridge->bio_router = bio_router;
    bridge->enabled = (bio_router != NULL);

    NIMCP_LOGGING_INFO("Bio-async -> spinal cord bridge created");
    return bridge;
}

int spinal_bio_async_bridge_update(spinal_bio_async_bridge_t* bridge) {
    if (!bridge || !bridge->enabled) return 0;
    /* STUB: Publish motor output events, process incoming commands */
    (void)bridge;
    return 0;
}

void spinal_bio_async_bridge_destroy(spinal_bio_async_bridge_t* bridge) {
    nimcp_free(bridge);
}

//=============================================================================
// 10. Immune Bridge
//=============================================================================

/**
 * @brief Create immune bridge
 *
 * BIOLOGICAL: Neuroinflammation affects spinal motor function:
 * - Pro-inflammatory cytokines (IL-1beta, TNF-alpha) reduce motor neuron
 *   excitability and increase fatigue
 * - Microglial activation in ventral horn during neurodegeneration (ALS)
 * - Central sensitization amplifies pain signals through spinal dorsal horn
 */
spinal_immune_bridge_t* spinal_immune_bridge_create(
        spinal_cord_t* spinal, void* immune_system) {
    if (!spinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_immune_bridge_create: spinal is NULL");
        return NULL;
    }

    spinal_immune_bridge_t* bridge =
        (spinal_immune_bridge_t*)nimcp_calloc(1, sizeof(spinal_immune_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "spinal_immune_bridge_create: allocation failed");
        return NULL;
    }

    bridge->spinal = spinal;
    bridge->immune_system = immune_system;
    bridge->inflammation_factor = 0.0f;
    bridge->enabled = (immune_system != NULL);

    NIMCP_LOGGING_INFO("Immune -> spinal cord bridge created");
    return bridge;
}

int spinal_immune_bridge_update(spinal_immune_bridge_t* bridge) {
    if (!bridge || !bridge->enabled || !bridge->spinal) return 0;

    /* Inflammation reduces motor output capacity.
     * gate_control is modulated: higher inflammation -> more gating */
    float inflammation = bridge->inflammation_factor;
    if (!isfinite(inflammation)) {
        inflammation = 0.0f;
    }
    inflammation = (inflammation < 0.0f) ? 0.0f : (inflammation > 1.0f) ? 1.0f : inflammation;

    /* Reduce gate control level proportionally to inflammation */
    nimcp_mutex_lock(bridge->spinal->lock);
    float current_gate = bridge->spinal->gate_control_level;
    float immune_gate = current_gate * (1.0f - 0.4f * inflammation);
    if (immune_gate < 0.1f) immune_gate = 0.1f;
    bridge->spinal->gate_control_level = immune_gate;
    nimcp_mutex_unlock(bridge->spinal->lock);

    return 0;
}

void spinal_immune_bridge_destroy(spinal_immune_bridge_t* bridge) {
    nimcp_free(bridge);
}
