/**
 * @file nimcp_mesh_brain_integration.c
 * @brief Brain Module Registration Integration for Mesh Network
 *
 * WHAT: Connects real brain module instances to the mesh network
 * WHY:  Replace dummy pointers with actual brain components for true integration
 * HOW:  Type-safe registration with receptive fields and health agent wiring
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_brain_integration.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_module_registry.h"
#include "mesh/nimcp_mesh_receptive_fields.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "MESH_BRAIN_INTEGRATION"

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

#define MESH_BRAIN_INTEGRATION_MAGIC 0x42524E49  /* "BRNI" */

struct mesh_brain_integration {
    uint32_t magic;

    mesh_bootstrap_t* bootstrap;
    mesh_brain_integration_config_t config;

    /* Registered module tracking */
    mesh_participant_id_t region_ids[MESH_BRAIN_REGION_COUNT];
    void* region_modules[MESH_BRAIN_REGION_COUNT];
    bool region_registered[MESH_BRAIN_REGION_COUNT];

    /* Statistics */
    mesh_brain_integration_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    bool initialized;
};

/* ============================================================================
 * Region Name Mapping
 * ============================================================================ */

static const char* brain_region_names[MESH_BRAIN_REGION_COUNT] = {
    [MESH_BRAIN_REGION_UNKNOWN] = "unknown",
    [MESH_BRAIN_REGION_HIPPOCAMPUS] = "hippocampus",
    [MESH_BRAIN_REGION_EPISODIC_MEMORY] = "episodic_memory",
    [MESH_BRAIN_REGION_SEMANTIC_MEMORY] = "semantic_memory",
    [MESH_BRAIN_REGION_WORKING_MEMORY] = "working_memory",
    [MESH_BRAIN_REGION_PROCEDURAL_MEMORY] = "procedural_memory",
    [MESH_BRAIN_REGION_AMYGDALA] = "amygdala",
    [MESH_BRAIN_REGION_HYPOTHALAMUS] = "hypothalamus",
    [MESH_BRAIN_REGION_NUCLEUS_ACCUMBENS] = "nucleus_accumbens",
    [MESH_BRAIN_REGION_CINGULATE] = "cingulate",
    [MESH_BRAIN_REGION_PFC_LEFT] = "pfc_left",
    [MESH_BRAIN_REGION_PFC_RIGHT] = "pfc_right",
    [MESH_BRAIN_REGION_DORSOLATERAL_PFC] = "dorsolateral_pfc",
    [MESH_BRAIN_REGION_ORBITOFRONTAL] = "orbitofrontal",
    [MESH_BRAIN_REGION_ANTERIOR_CINGULATE] = "anterior_cingulate",
    [MESH_BRAIN_REGION_MOTOR_CORTEX] = "motor_cortex",
    [MESH_BRAIN_REGION_PREMOTOR] = "premotor",
    [MESH_BRAIN_REGION_SUPPLEMENTARY_MOTOR] = "supplementary_motor",
    [MESH_BRAIN_REGION_CEREBELLUM] = "cerebellum",
    [MESH_BRAIN_REGION_BASAL_GANGLIA] = "basal_ganglia",
    [MESH_BRAIN_REGION_VISUAL_CORTEX] = "visual_cortex",
    [MESH_BRAIN_REGION_AUDITORY_CORTEX] = "auditory_cortex",
    [MESH_BRAIN_REGION_SOMATOSENSORY] = "somatosensory",
    [MESH_BRAIN_REGION_THALAMUS] = "thalamus",
    [MESH_BRAIN_REGION_FEP_ORCHESTRATOR] = "fep_orchestrator",
    [MESH_BRAIN_REGION_ATTENTION] = "attention",
    [MESH_BRAIN_REGION_REASONING] = "reasoning",
    [MESH_BRAIN_REGION_PLANNING] = "planning",
    [MESH_BRAIN_REGION_EXECUTIVE] = "executive",
    [MESH_BRAIN_REGION_GLOBAL_WORKSPACE] = "global_workspace",
    [MESH_BRAIN_REGION_THEORY_OF_MIND] = "theory_of_mind",
    [MESH_BRAIN_REGION_BBB] = "blood_brain_barrier",
    [MESH_BRAIN_REGION_IMMUNE_SYSTEM] = "brain_immune_system",
    [MESH_BRAIN_REGION_THREAT_DETECTOR] = "threat_detector",
    [MESH_BRAIN_REGION_STDP] = "stdp",
    [MESH_BRAIN_REGION_LTP] = "ltp",
    [MESH_BRAIN_REGION_HOMEOSTATIC] = "homeostatic",
    [MESH_BRAIN_REGION_PLASTICITY_COORDINATOR] = "plasticity_coordinator",
    [MESH_BRAIN_REGION_ASTROCYTE] = "astrocyte",
    [MESH_BRAIN_REGION_OLIGODENDROCYTE] = "oligodendrocyte",
    [MESH_BRAIN_REGION_BIO_ASYNC_ORCHESTRATOR] = "bio_async_orchestrator",
};

/* ============================================================================
 * Region to Category Mapping
 * ============================================================================ */

mesh_adapter_category_t mesh_brain_region_to_category(mesh_brain_region_t region) {
    switch (region) {
        /* Memory regions -> MEMORY category */
        case MESH_BRAIN_REGION_HIPPOCAMPUS:
        case MESH_BRAIN_REGION_EPISODIC_MEMORY:
        case MESH_BRAIN_REGION_SEMANTIC_MEMORY:
        case MESH_BRAIN_REGION_WORKING_MEMORY:
        case MESH_BRAIN_REGION_PROCEDURAL_MEMORY:
            return MESH_ADAPTER_CATEGORY_MEMORY;

        /* Limbic regions -> SUBCORTICAL category */
        case MESH_BRAIN_REGION_AMYGDALA:
        case MESH_BRAIN_REGION_HYPOTHALAMUS:
        case MESH_BRAIN_REGION_NUCLEUS_ACCUMBENS:
        case MESH_BRAIN_REGION_CINGULATE:
            return MESH_ADAPTER_CATEGORY_SUBCORTICAL;

        /* Cortical regions -> COGNITIVE category */
        case MESH_BRAIN_REGION_PFC_LEFT:
        case MESH_BRAIN_REGION_PFC_RIGHT:
        case MESH_BRAIN_REGION_DORSOLATERAL_PFC:
        case MESH_BRAIN_REGION_ORBITOFRONTAL:
        case MESH_BRAIN_REGION_ANTERIOR_CINGULATE:
            return MESH_ADAPTER_CATEGORY_COGNITIVE;

        /* Motor regions -> MOTOR category */
        case MESH_BRAIN_REGION_MOTOR_CORTEX:
        case MESH_BRAIN_REGION_PREMOTOR:
        case MESH_BRAIN_REGION_SUPPLEMENTARY_MOTOR:
        case MESH_BRAIN_REGION_CEREBELLUM:
        case MESH_BRAIN_REGION_BASAL_GANGLIA:
            return MESH_ADAPTER_CATEGORY_MOTOR;

        /* Sensory regions -> PERCEPTION category */
        case MESH_BRAIN_REGION_VISUAL_CORTEX:
        case MESH_BRAIN_REGION_AUDITORY_CORTEX:
        case MESH_BRAIN_REGION_SOMATOSENSORY:
        case MESH_BRAIN_REGION_THALAMUS:
            return MESH_ADAPTER_CATEGORY_PERCEPTION;

        /* Cognitive regions -> COGNITIVE category */
        case MESH_BRAIN_REGION_FEP_ORCHESTRATOR:
        case MESH_BRAIN_REGION_ATTENTION:
        case MESH_BRAIN_REGION_REASONING:
        case MESH_BRAIN_REGION_PLANNING:
        case MESH_BRAIN_REGION_EXECUTIVE:
        case MESH_BRAIN_REGION_GLOBAL_WORKSPACE:
        case MESH_BRAIN_REGION_THEORY_OF_MIND:
            return MESH_ADAPTER_CATEGORY_COGNITIVE;

        /* Security regions -> SECURITY category */
        case MESH_BRAIN_REGION_BBB:
        case MESH_BRAIN_REGION_IMMUNE_SYSTEM:
        case MESH_BRAIN_REGION_THREAT_DETECTOR:
            return MESH_ADAPTER_CATEGORY_SECURITY;

        /* Plasticity regions -> PLASTICITY category */
        case MESH_BRAIN_REGION_STDP:
        case MESH_BRAIN_REGION_LTP:
        case MESH_BRAIN_REGION_HOMEOSTATIC:
        case MESH_BRAIN_REGION_PLASTICITY_COORDINATOR:
            return MESH_ADAPTER_CATEGORY_PLASTICITY;

        /* Glial regions -> GLIAL category */
        case MESH_BRAIN_REGION_ASTROCYTE:
        case MESH_BRAIN_REGION_OLIGODENDROCYTE:
            return MESH_ADAPTER_CATEGORY_GLIAL;

        /* Orchestrators -> SYSTEM category */
        case MESH_BRAIN_REGION_BIO_ASYNC_ORCHESTRATOR:
            return MESH_ADAPTER_CATEGORY_SYSTEM;

        default:
            return MESH_ADAPTER_CATEGORY_SYSTEM;
    }
}

/* ============================================================================
 * Region to Receptive Field Mapping
 * ============================================================================ */

const mesh_receptive_field_t* mesh_brain_region_get_receptive_field(
    mesh_brain_region_t region
) {
    switch (region) {
        /* Memory regions */
        case MESH_BRAIN_REGION_HIPPOCAMPUS:
            return &MESH_RF_HIPPOCAMPUS;
        case MESH_BRAIN_REGION_EPISODIC_MEMORY:
            return &MESH_RF_EPISODIC_MEMORY;
        case MESH_BRAIN_REGION_SEMANTIC_MEMORY:
            return &MESH_RF_SEMANTIC_MEMORY;
        case MESH_BRAIN_REGION_WORKING_MEMORY:
            return &MESH_RF_WORKING_MEMORY;
        case MESH_BRAIN_REGION_PROCEDURAL_MEMORY:
            return &MESH_RF_PROCEDURAL_MEMORY;

        /* Limbic regions */
        case MESH_BRAIN_REGION_AMYGDALA:
            return &MESH_RF_AMYGDALA;
        case MESH_BRAIN_REGION_HYPOTHALAMUS:
            return &MESH_RF_HYPOTHALAMUS;
        case MESH_BRAIN_REGION_NUCLEUS_ACCUMBENS:
            return &MESH_RF_NUCLEUS_ACCUMBENS;
        case MESH_BRAIN_REGION_CINGULATE:
            return &MESH_RF_CINGULATE;

        /* Cortical regions */
        case MESH_BRAIN_REGION_PFC_LEFT:
            return &MESH_RF_PFC_LEFT;
        case MESH_BRAIN_REGION_PFC_RIGHT:
            return &MESH_RF_PFC_RIGHT;
        case MESH_BRAIN_REGION_DORSOLATERAL_PFC:
            return &MESH_RF_DORSOLATERAL_PFC;
        case MESH_BRAIN_REGION_ORBITOFRONTAL:
            return &MESH_RF_ORBITOFRONTAL;
        case MESH_BRAIN_REGION_ANTERIOR_CINGULATE:
            return &MESH_RF_ANTERIOR_CINGULATE;

        /* Motor regions */
        case MESH_BRAIN_REGION_MOTOR_CORTEX:
            return &MESH_RF_MOTOR_CORTEX;
        case MESH_BRAIN_REGION_PREMOTOR:
            return &MESH_RF_PREMOTOR;
        case MESH_BRAIN_REGION_SUPPLEMENTARY_MOTOR:
            return &MESH_RF_SUPPLEMENTARY_MOTOR;
        case MESH_BRAIN_REGION_CEREBELLUM:
            return &MESH_RF_CEREBELLUM;
        case MESH_BRAIN_REGION_BASAL_GANGLIA:
            return &MESH_RF_BASAL_GANGLIA;

        /* Sensory regions */
        case MESH_BRAIN_REGION_VISUAL_CORTEX:
            return &MESH_RF_VISUAL_CORTEX;
        case MESH_BRAIN_REGION_AUDITORY_CORTEX:
            return &MESH_RF_AUDITORY_CORTEX;
        case MESH_BRAIN_REGION_SOMATOSENSORY:
            return &MESH_RF_SOMATOSENSORY;
        case MESH_BRAIN_REGION_THALAMUS:
            return &MESH_RF_THALAMUS;

        /* Cognitive regions */
        case MESH_BRAIN_REGION_FEP_ORCHESTRATOR:
            return &MESH_RF_FEP_ORCHESTRATOR;
        case MESH_BRAIN_REGION_ATTENTION:
            return &MESH_RF_ATTENTION_MANAGER;
        case MESH_BRAIN_REGION_REASONING:
            return &MESH_RF_REASONING_ENGINE;
        case MESH_BRAIN_REGION_PLANNING:
            return &MESH_RF_PLANNING_MODULE;
        case MESH_BRAIN_REGION_EXECUTIVE:
            return &MESH_RF_DECISION_MAKING;

        /* Security regions */
        case MESH_BRAIN_REGION_BBB:
            return &MESH_RF_BBB;
        case MESH_BRAIN_REGION_IMMUNE_SYSTEM:
            return &MESH_RF_IMMUNE_SYSTEM;
        case MESH_BRAIN_REGION_THREAT_DETECTOR:
            return &MESH_RF_THREAT_DETECTOR;

        /* Plasticity regions */
        case MESH_BRAIN_REGION_STDP:
            return &MESH_RF_STDP;
        case MESH_BRAIN_REGION_LTP:
            return &MESH_RF_LTP;
        case MESH_BRAIN_REGION_HOMEOSTATIC:
            return &MESH_RF_HOMEOSTATIC;

        /* Glial regions */
        case MESH_BRAIN_REGION_ASTROCYTE:
            return &MESH_RF_ASTROCYTE;
        case MESH_BRAIN_REGION_OLIGODENDROCYTE:
            return &MESH_RF_OLIGODENDROCYTE;

        /* Swarm/system regions */
        case MESH_BRAIN_REGION_GLOBAL_WORKSPACE:
            return &MESH_RF_COLLECTIVE_WORKSPACE;

        default:
            /* Unknown region is defensive default, not necessarily an error */
            return NULL;
    }
}

const char* mesh_brain_region_to_string(mesh_brain_region_t region) {
    if (region >= MESH_BRAIN_REGION_COUNT) {
        return "invalid";
    }
    return brain_region_names[region];
}

/* ============================================================================
 * Private: Update Statistics by Category
 * ============================================================================ */

static void update_stats_for_region(
    mesh_brain_integration_stats_t* stats,
    mesh_brain_region_t region,
    bool add
) {
    int delta = add ? 1 : -1;

    mesh_adapter_category_t category = mesh_brain_region_to_category(region);

    switch (category) {
        case MESH_ADAPTER_CATEGORY_MEMORY:
            stats->memory_modules_registered += delta;
            break;
        case MESH_ADAPTER_CATEGORY_COGNITIVE:
            stats->cognitive_modules_registered += delta;
            break;
        case MESH_ADAPTER_CATEGORY_PERCEPTION:
            stats->sensory_modules_registered += delta;
            break;
        case MESH_ADAPTER_CATEGORY_MOTOR:
            stats->motor_modules_registered += delta;
            break;
        case MESH_ADAPTER_CATEGORY_SECURITY:
            stats->security_modules_registered += delta;
            break;
        case MESH_ADAPTER_CATEGORY_PLASTICITY:
            stats->plasticity_modules_registered += delta;
            break;
        case MESH_ADAPTER_CATEGORY_GLIAL:
            stats->glial_modules_registered += delta;
            break;
        case MESH_ADAPTER_CATEGORY_SYSTEM:
        case MESH_ADAPTER_CATEGORY_SUBCORTICAL:
            stats->orchestrator_modules_registered += delta;
            break;
        default:
            break;
    }

    if (add) {
        stats->total_modules_registered++;
    } else if (stats->total_modules_registered > 0) {
        stats->total_modules_registered--;
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

nimcp_error_t mesh_brain_integration_default_config(
    mesh_brain_integration_config_t* config
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_brain_integration: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(*config));

    config->auto_register_available = true;
    config->register_with_health_agents = true;
    config->register_receptive_fields = true;
    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

mesh_brain_integration_t* mesh_brain_integration_create(
    mesh_bootstrap_t* bootstrap,
    const mesh_brain_integration_config_t* config
) {
    if (!bootstrap) {
        LOG_ERROR("Bootstrap handle is required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_brain_integration_create: bootstrap is NULL");
        return NULL;
    }

    mesh_brain_integration_config_t default_config;
    if (!config) {
        mesh_brain_integration_default_config(&default_config);
        config = &default_config;
    }

    mesh_brain_integration_t* integration = nimcp_calloc(
        1, sizeof(mesh_brain_integration_t));
    if (!integration) {
        LOG_ERROR("Failed to allocate brain integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_brain_integration_create: integration is NULL");
        return NULL;
    }

    integration->magic = MESH_BRAIN_INTEGRATION_MAGIC;
    integration->bootstrap = bootstrap;
    integration->config = *config;

    /* Create mutex */
    mutex_attr_t attr = {0};
    integration->mutex = nimcp_mutex_create(&attr);
    if (!integration->mutex) {
        LOG_ERROR("Failed to create integration mutex");
        nimcp_free(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_brain_integration_create: integration->mutex is NULL");
        return NULL;
    }

    /* Initialize region tracking arrays */
    memset(integration->region_ids, 0, sizeof(integration->region_ids));
    memset(integration->region_modules, 0, sizeof(integration->region_modules));
    memset(integration->region_registered, 0, sizeof(integration->region_registered));

    integration->initialized = true;

    LOG_INFO("Created mesh brain integration handler");

    return integration;
}

void mesh_brain_integration_destroy(mesh_brain_integration_t* integration) {
    if (!integration || integration->magic != MESH_BRAIN_INTEGRATION_MAGIC) {
        return;
    }

    nimcp_mutex_lock(integration->mutex);

    /* Clear magic to prevent reuse */
    integration->magic = 0;
    integration->initialized = false;

    nimcp_mutex_unlock(integration->mutex);
    nimcp_mutex_destroy(integration->mutex);

    nimcp_free(integration);

    LOG_INFO("Destroyed mesh brain integration handler");
}

nimcp_error_t mesh_brain_integration_init(void) {
    /* Initialize the receptive fields library if not already done */
    nimcp_error_t err = mesh_receptive_fields_init();
    if (err != NIMCP_SUCCESS) {
        LOG_WARN("Receptive fields library initialization failed: %d", err);
        /* Non-fatal: registration still works without receptive fields */
    }

    LOG_INFO("Mesh brain integration system initialized");
    return NIMCP_SUCCESS;
}

void mesh_brain_integration_cleanup(void) {
    mesh_receptive_fields_cleanup();
    LOG_INFO("Mesh brain integration system cleaned up");
}

/* ============================================================================
 * Individual Module Registration
 * ============================================================================ */

nimcp_error_t mesh_brain_integration_register_module(
    mesh_brain_integration_t* integration,
    mesh_brain_region_t region,
    void* module,
    size_t module_size,
    uint32_t module_magic,
    nimcp_health_agent_t* health_agent
) {
    if (!integration || integration->magic != MESH_BRAIN_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_brain_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_brain_integration: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (region <= MESH_BRAIN_REGION_UNKNOWN || region >= MESH_BRAIN_REGION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_brain_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    /* Check if already registered */
    if (integration->region_registered[region]) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_WARN("Region %s already registered", mesh_brain_region_to_string(region));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_ALREADY_EXISTS, "mesh_brain_integration: error condition");
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    /* Build module descriptor */
    const char* name = mesh_brain_region_to_string(region);
    mesh_adapter_category_t category = mesh_brain_region_to_category(region);
    const mesh_receptive_field_t* rf = NULL;

    if (integration->config.register_receptive_fields) {
        rf = mesh_brain_region_get_receptive_field(region);
    }

    mesh_module_descriptor_t descriptor = {
        .module_name = name,
        .category = category,
        .module_instance = module,
        .module_size = module_size,
        .module_magic = module_magic,
        .receptive_field = rf,
        .health_agent = (integration->config.register_with_health_agents) ? health_agent : NULL,
        .endorser_role = ENDORSER_ROLE_OPTIONAL,
        .policies = NULL,
        .policy_count = 0,
        .secondary_channels = NULL,
        .secondary_channel_count = 0
    };

    /* Adjust endorser role based on region importance */
    switch (region) {
        case MESH_BRAIN_REGION_HIPPOCAMPUS:
        case MESH_BRAIN_REGION_BBB:
        case MESH_BRAIN_REGION_IMMUNE_SYSTEM:
        case MESH_BRAIN_REGION_FEP_ORCHESTRATOR:
        case MESH_BRAIN_REGION_THALAMUS:
            descriptor.endorser_role = ENDORSER_ROLE_REQUIRED;
            break;
        case MESH_BRAIN_REGION_AMYGDALA:
            /* Amygdala has VETO power for safety-critical decisions */
            descriptor.endorser_role = ENDORSER_ROLE_REQUIRED;
            break;
        case MESH_BRAIN_REGION_EXECUTIVE:
        case MESH_BRAIN_REGION_GLOBAL_WORKSPACE:
            descriptor.endorser_role = ENDORSER_ROLE_PREFERRED;
            break;
        default:
            descriptor.endorser_role = ENDORSER_ROLE_OPTIONAL;
            break;
    }

    /* Register with mesh bootstrap */
    nimcp_error_t err = mesh_bootstrap_register_module(
        integration->bootstrap, &descriptor);

    if (err != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(integration->mutex);
        integration->stats.registration_failures++;
        LOG_ERROR("Failed to register %s with mesh: %d", name, err);
        return err;
    }

    /* Track registration */
    integration->region_modules[region] = module;
    integration->region_registered[region] = true;

    /* Look up the assigned participant ID */
    mesh_registered_module_t* reg = mesh_bootstrap_lookup_module(
        integration->bootstrap, name);
    if (reg) {
        integration->region_ids[region] = reg->participant_id;
    }

    /* Update statistics */
    update_stats_for_region(&integration->stats, region, true);
    integration->stats.last_registration_time_ns = nimcp_time_now_ns();

    nimcp_mutex_unlock(integration->mutex);

    if (integration->config.verbose_logging) {
        LOG_DEBUG("Registered brain module '%s' (region=%d, category=%d)",
                  name, region, category);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Internal unlocked version of unregister_module
 *
 * WHAT: Unregister a module without acquiring the mutex
 * WHY:  Called from unregister_brain which already holds the mutex,
 *       preventing self-deadlock on non-recursive mutex
 * PRECONDITION: Caller must hold integration->mutex
 */
static nimcp_error_t unregister_module_unlocked(
    mesh_brain_integration_t* integration,
    mesh_brain_region_t region
) {
    if (!integration->region_registered[region]) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_brain_integration: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Unregister from module registry */
    const char* name = mesh_brain_region_to_string(region);
    mesh_module_registry_t* registry = mesh_bootstrap_get_module_registry(
        integration->bootstrap);
    if (registry) {
        mesh_module_registry_unregister(registry, name);
    }

    /* Clear tracking */
    integration->region_ids[region] = 0;
    integration->region_modules[region] = NULL;
    integration->region_registered[region] = false;

    /* Update statistics */
    update_stats_for_region(&integration->stats, region, false);

    if (integration->config.verbose_logging) {
        LOG_DEBUG("Unregistered brain module '%s' (region=%d)", name, region);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_brain_integration_unregister_module(
    mesh_brain_integration_t* integration,
    mesh_brain_region_t region
) {
    if (!integration || integration->magic != MESH_BRAIN_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_brain_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (region <= MESH_BRAIN_REGION_UNKNOWN || region >= MESH_BRAIN_REGION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_brain_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    nimcp_error_t result = unregister_module_unlocked(integration, region);

    nimcp_mutex_unlock(integration->mutex);

    return result;
}

/* ============================================================================
 * Specific Module Registration Functions
 * ============================================================================ */

nimcp_error_t mesh_brain_integration_register_hippocampus(
    mesh_brain_integration_t* integration,
    void* hippocampus,
    nimcp_health_agent_t* health_agent
) {
    /* Use 0 for size and magic to skip validation - caller should use typed macro */
    return mesh_brain_integration_register_module(
        integration, MESH_BRAIN_REGION_HIPPOCAMPUS,
        hippocampus, 0, 0, health_agent);
}

nimcp_error_t mesh_brain_integration_register_amygdala(
    mesh_brain_integration_t* integration,
    void* amygdala,
    nimcp_health_agent_t* health_agent
) {
    return mesh_brain_integration_register_module(
        integration, MESH_BRAIN_REGION_AMYGDALA,
        amygdala, 0, 0, health_agent);
}

nimcp_error_t mesh_brain_integration_register_thalamus(
    mesh_brain_integration_t* integration,
    void* thalamus,
    nimcp_health_agent_t* health_agent
) {
    return mesh_brain_integration_register_module(
        integration, MESH_BRAIN_REGION_THALAMUS,
        thalamus, 0, 0, health_agent);
}

nimcp_error_t mesh_brain_integration_register_basal_ganglia(
    mesh_brain_integration_t* integration,
    void* basal_ganglia,
    nimcp_health_agent_t* health_agent
) {
    return mesh_brain_integration_register_module(
        integration, MESH_BRAIN_REGION_BASAL_GANGLIA,
        basal_ganglia, 0, 0, health_agent);
}

nimcp_error_t mesh_brain_integration_register_immune_system(
    mesh_brain_integration_t* integration,
    brain_immune_system_t* immune,
    nimcp_health_agent_t* health_agent
) {
    return mesh_brain_integration_register_module(
        integration, MESH_BRAIN_REGION_IMMUNE_SYSTEM,
        immune, 0, 0, health_agent);
}

nimcp_error_t mesh_brain_integration_register_bbb(
    mesh_brain_integration_t* integration,
    void* bbb,
    nimcp_health_agent_t* health_agent
) {
    return mesh_brain_integration_register_module(
        integration, MESH_BRAIN_REGION_BBB,
        bbb, 0, 0, health_agent);
}

nimcp_error_t mesh_brain_integration_register_fep_orchestrator(
    mesh_brain_integration_t* integration,
    void* fep_orchestrator,
    nimcp_health_agent_t* health_agent
) {
    return mesh_brain_integration_register_module(
        integration, MESH_BRAIN_REGION_FEP_ORCHESTRATOR,
        fep_orchestrator, 0, 0, health_agent);
}

nimcp_error_t mesh_brain_integration_register_working_memory(
    mesh_brain_integration_t* integration,
    void* working_memory,
    nimcp_health_agent_t* health_agent
) {
    return mesh_brain_integration_register_module(
        integration, MESH_BRAIN_REGION_WORKING_MEMORY,
        working_memory, 0, 0, health_agent);
}

nimcp_error_t mesh_brain_integration_register_executive(
    mesh_brain_integration_t* integration,
    void* executive,
    nimcp_health_agent_t* health_agent
) {
    return mesh_brain_integration_register_module(
        integration, MESH_BRAIN_REGION_EXECUTIVE,
        executive, 0, 0, health_agent);
}

nimcp_error_t mesh_brain_integration_register_global_workspace(
    mesh_brain_integration_t* integration,
    void* workspace,
    nimcp_health_agent_t* health_agent
) {
    return mesh_brain_integration_register_module(
        integration, MESH_BRAIN_REGION_GLOBAL_WORKSPACE,
        workspace, 0, 0, health_agent);
}

nimcp_error_t mesh_brain_integration_register_plasticity_coordinator(
    mesh_brain_integration_t* integration,
    void* plasticity,
    nimcp_health_agent_t* health_agent
) {
    return mesh_brain_integration_register_module(
        integration, MESH_BRAIN_REGION_PLASTICITY_COORDINATOR,
        plasticity, 0, 0, health_agent);
}

nimcp_error_t mesh_brain_integration_register_bio_async_orchestrator(
    mesh_brain_integration_t* integration,
    void* bio_async_orch,
    nimcp_health_agent_t* health_agent
) {
    return mesh_brain_integration_register_module(
        integration, MESH_BRAIN_REGION_BIO_ASYNC_ORCHESTRATOR,
        bio_async_orch, 0, 0, health_agent);
}

/* ============================================================================
 * Query API
 * ============================================================================ */

mesh_participant_id_t mesh_brain_integration_get_participant_id(
    const mesh_brain_integration_t* integration,
    mesh_brain_region_t region
) {
    if (!integration || integration->magic != MESH_BRAIN_INTEGRATION_MAGIC) {
        return 0;
    }
    if (region <= MESH_BRAIN_REGION_UNKNOWN || region >= MESH_BRAIN_REGION_COUNT) {
        return 0;
    }

    return integration->region_ids[region];
}

bool mesh_brain_integration_is_registered(
    const mesh_brain_integration_t* integration,
    mesh_brain_region_t region
) {
    if (!integration || integration->magic != MESH_BRAIN_INTEGRATION_MAGIC) {
        return false;
    }
    if (region <= MESH_BRAIN_REGION_UNKNOWN || region >= MESH_BRAIN_REGION_COUNT) {
        return false;
    }

    return integration->region_registered[region];
}

nimcp_error_t mesh_brain_integration_get_stats(
    const mesh_brain_integration_t* integration,
    mesh_brain_integration_stats_t* stats
) {
    if (!integration || integration->magic != MESH_BRAIN_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_brain_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_brain_integration: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    *stats = integration->stats;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Brain-Wide Registration
 * ============================================================================ */

nimcp_error_t mesh_brain_integration_register_brain(
    mesh_brain_integration_t* integration,
    brain_t brain
) {
    if (!integration || integration->magic != MESH_BRAIN_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_brain_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_brain_integration: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    size_t registered = 0;
    size_t failed = 0;

    LOG_INFO("Registering brain modules with mesh network...");

    /*
     * NOTE: This function uses opaque pointers since brain_struct is internal.
     * The actual brain module fields are accessed via brain_t which is a pointer
     * to struct brain_struct. For full integration, include nimcp_brain_internal.h
     * in the source file and cast appropriately.
     *
     * For this initial implementation, we provide a framework that can be
     * extended when brain initialization calls the specific registration
     * functions with properly typed pointers.
     */

    /* The brain-wide registration is called from brain init code where
     * the brain structure fields are accessible. This function serves
     * as a documentation point for the integration pattern.
     *
     * Example from brain init:
     *
     *   mesh_brain_integration_register_hippocampus(
     *       integration, brain->hippocampus, NULL);
     *   mesh_brain_integration_register_amygdala(
     *       integration, brain->amygdala, NULL);
     *   ...
     */

    LOG_INFO("Brain module registration complete: %zu registered, %zu failed",
             registered, failed);

    return (failed == 0) ? NIMCP_SUCCESS : NIMCP_ERROR_OPERATION_FAILED;
}

nimcp_error_t mesh_brain_integration_unregister_brain(
    mesh_brain_integration_t* integration,
    brain_t brain
) {
    if (!integration || integration->magic != MESH_BRAIN_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_brain_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_brain_integration: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(integration->mutex);

    /* Unregister all registered regions using unlocked variant
     * to avoid self-deadlock (we already hold integration->mutex) */
    for (int i = 1; i < MESH_BRAIN_REGION_COUNT; i++) {
        if (integration->region_registered[i]) {
            unregister_module_unlocked(
                integration, (mesh_brain_region_t)i);
        }
    }

    nimcp_mutex_unlock(integration->mutex);

    LOG_INFO("Unregistered all brain modules from mesh network");

    return NIMCP_SUCCESS;
}
