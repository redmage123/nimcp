/**
 * @file nimcp_brain_init_cortical_interneurons.c
 * @brief Factory initialization for cortical interneuron subsystem
 * @version 1.0.0
 * @date 2026-03-05
 *
 * WHAT: Initialize cortical interneuron system during brain creation.
 * WHY:  Cortical interneurons are essential for E/I balance, gamma oscillations,
 *       attention gating (VIP disinhibition), and prediction error (SST feedback).
 *       Without interneurons, cortical columns lack inhibitory circuit diversity.
 * HOW:  Creates interneuron system with default config (100 interneurons across
 *       5 types), then connects stub bridges for cortical columns, plasticity,
 *       training, inference, thalamic TRN, bio-async, immune, and substrate GPU.
 *
 * BIOLOGICAL BASIS:
 * - ~20% of cortical neurons are GABAergic interneurons
 * - PV basket cells (40%): fast-spiking, gamma generation, perisomatic inhibition
 * - PV chandelier cells (10%): axo-axonic, gate AP initiation
 * - SST Martinotti cells (25%): dendrite-targeting, L5->L1 feedback
 * - VIP cells (15%): disinhibition, attention gating
 * - NGF L1 cells (10%): volume transmission, tonic inhibition
 */

#include "core/brain/factory/init/nimcp_brain_init_cortical_interneurons.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/cortical_columns/nimcp_cortical_interneurons.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_CORTICAL_INTERNEURONS"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_cortical_interneurons, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Bridge function declarations from nimcp_cortical_interneurons_bridges.c */
extern int cint_bridge_connect_cortical_columns(cortical_interneuron_system_t* system, void* column_pool);
extern int cint_bridge_connect_plasticity(cortical_interneuron_system_t* system, void* plasticity_ctx);
extern int cint_bridge_connect_training(cortical_interneuron_system_t* system, void* training_ctx);
extern int cint_bridge_connect_inference(cortical_interneuron_system_t* system, void* inference_ctx);
extern int cint_bridge_connect_thalamic_trn(cortical_interneuron_system_t* system, void* trn_ctx);
extern int cint_bridge_connect_bio_async(cortical_interneuron_system_t* system);
extern int cint_bridge_connect_immune(cortical_interneuron_system_t* system, void* immune_ctx);
extern int cint_bridge_connect_substrate_gpu(cortical_interneuron_system_t* system, void* gpu_ctx);

bool nimcp_brain_factory_init_cortical_interneurons_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_cortical_interneurons_subsystem: brain is NULL");
        return false;
    }

    /* Idempotency: skip if already initialized */
    if (brain->cortical_interneurons) {
        LOG_INFO(LOG_MODULE, "Cortical interneurons already initialized, skipping");
        return true;
    }

    /* Create interneuron system with default config */
    cint_config_t config;
    cint_default_config(&config);

    cortical_interneuron_system_t* system = cint_create(&config);
    if (!system) {
        LOG_WARN(LOG_MODULE, "Failed to create cortical interneuron system (non-fatal)");
        return true;
    }

    /* Store on brain */
    brain->cortical_interneurons = system;
    brain->cortical_interneurons_enabled = true;

    /* Connect integration bridges (all are stubs, non-fatal if they fail) */

    /* Cortical columns bridge */
    if (brain->cortical_column_pool) {
        cint_bridge_connect_cortical_columns(system, brain->cortical_column_pool);
    }

    /* Plasticity bridge */
    if (brain->plasticity_coordinator) {
        cint_bridge_connect_plasticity(system, brain->plasticity_coordinator);
    }

    /* Training bridge */
    if (brain->training_ctx) {
        cint_bridge_connect_training(system, brain->training_ctx);
    }

    /* Bio-async bridge */
    cint_bridge_connect_bio_async(system);

    /* Immune bridge */
    if (brain->immune_system) {
        cint_bridge_connect_immune(system, brain->immune_system);
    }

    /* GPU bridge */
    if (brain->gpu_enabled) {
        cint_bridge_connect_substrate_gpu(system, NULL); /* GPU ctx looked up dynamically */
    }

    LOG_INFO(LOG_MODULE, "Cortical interneuron subsystem initialized: %u interneurons, "
             "E/I target=%.1f",
             system->num_interneurons, config.target_ei_ratio);

    return true;
}

void nimcp_brain_factory_destroy_cortical_interneurons_subsystem(brain_t brain)
{
    if (!brain) return;

    if (brain->cortical_interneurons) {
        cint_destroy(brain->cortical_interneurons);
        brain->cortical_interneurons = NULL;
        brain->cortical_interneurons_enabled = false;

        LOG_INFO(LOG_MODULE, "Cortical interneuron subsystem destroyed");
    }
}
