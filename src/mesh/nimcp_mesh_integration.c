/**
 * @file nimcp_mesh_integration.c
 * @brief Central Mesh Network Integration Manager Implementation
 *
 * WHAT: Unified manager for all NIMCP mesh network components
 * WHY:  Single point for mesh lifecycle and component wiring
 * HOW:  Creates channels, adapters, policies, and coordinates updates
 */

#include "mesh/nimcp_mesh_integration.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

/* ============================================================================
 * Structure Definition
 * ============================================================================ */

struct mesh_integration {
    uint32_t magic;
    mesh_integration_config_t config;
    
    /* Core components */
    mesh_participant_registry_t* registry;
    mesh_channel_t* channels[MESH_NUM_STANDARD_CHANNELS];
    mesh_coordinator_pool_t* coordinator_pools[MESH_NUM_STANDARD_CHANNELS];
    mesh_ordering_service_t* ordering;
    mesh_msp_t* msp;
    mesh_endorsement_collector_t* endorsement_collector;
    mesh_cross_router_t router;
    
    /* Adapter tracking */
    mesh_adapter_base_t* adapters[MESH_MAX_ADAPTERS];
    size_t adapter_count;
    
    /* Statistics */
    mesh_integration_stats_t stats;
    
    /* State */
    bool is_initialized;
    uint64_t last_update_ns;
    
    nimcp_mutex_t* mutex;
};

#define MESH_INTEGRATION_MAGIC 0x494E5447  /* "INTG" */

/* ============================================================================
 * Private: Channel Names
 * ============================================================================ */

static const char* channel_names[MESH_NUM_STANDARD_CHANNELS] = {
    "system",
    "left_hemisphere",
    "right_hemisphere",
    "subcortical",
    "gpu_compute"
};

/* ============================================================================
 * Private: Create Standard Policies
 * ============================================================================ */

static nimcp_error_t create_standard_policies(mesh_integration_t* integration) {
    /*
     * Brain-Inspired Pattern Routing: No static policies needed.
     *
     * The old system used static policies like:
     *   "motor_command" -> "motor_cortex AND cerebellum"
     *   "memory_store"  -> "hippocampus AND (PFC OR salience > 0.7)"
     *
     * The new system uses pattern-based self-selection:
     *   - Each module registers a receptive field (pattern it responds to)
     *   - Transactions carry pattern vectors
     *   - Modules self-select based on pattern similarity
     *   - Activation level determines role (REQUIRED, PREFERRED, OPTIONAL, VETO)
     *
     * This function is kept for API compatibility but does nothing.
     * Endorser selection is handled by the pattern router.
     */
    (void)integration;

    LOG_INFO("Using brain-inspired pattern routing instead of static policies");

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Public API: Lifecycle
 * ============================================================================ */

nimcp_error_t mesh_integration_default_config(mesh_integration_config_t* config) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;
    
    memset(config, 0, sizeof(*config));
    
    config->max_participants = 512;
    config->world_state_capacity = 1024;
    config->gossip_rounds = 3;
    config->convergence_threshold = 0.01f;
    config->ordering_batch_size = 64;
    config->ordering_batch_timeout_ms = 50;
    config->coordinators_per_channel = 4;
    config->election_timeout_ms = 150.0f;
    config->enable_bbb_integration = true;
    config->enable_immune_integration = true;
    config->base_interval_ms = 10.0f;
    config->jitter_amplitude_ms = 2.0f;
    config->enable_logging = true;
    
    return NIMCP_SUCCESS;
}

mesh_integration_t* mesh_integration_create(
    const mesh_integration_config_t* config
) {
    mesh_integration_config_t default_config;
    if (!config) {
        mesh_integration_default_config(&default_config);
        config = &default_config;
    }
    
    mesh_integration_t* integration = nimcp_calloc(1, sizeof(*integration));
    if (!integration) {
        LOG_ERROR("Failed to allocate mesh integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_integration_create: integration is NULL");
        return NULL;
    }
    
    integration->magic = MESH_INTEGRATION_MAGIC;
    integration->config = *config;
    
    /* Create mutex */
    mutex_attr_t attr = {0};
    integration->mutex = nimcp_mutex_create(&attr);
    if (!integration->mutex) {
        LOG_ERROR("Failed to create integration mutex");
        nimcp_free(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_integration_create: integration->mutex is NULL");
        return NULL;
    }
    
    /* Create participant registry */
    mesh_registry_config_t reg_config;
    mesh_registry_default_config(&reg_config);
    reg_config.initial_capacity = config->max_participants;
    
    integration->registry = mesh_registry_create(&reg_config);
    if (!integration->registry) {
        LOG_ERROR("Failed to create participant registry");
        mesh_integration_destroy(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_integration_create: integration->registry is NULL");
        return NULL;
    }
    
    /* Create standard channels */
    for (int i = 0; i < MESH_NUM_STANDARD_CHANNELS; i++) {
        mesh_channel_config_t ch_config;
        mesh_channel_default_config(&ch_config);
        ch_config.channel_name = channel_names[i];
        ch_config.channel_id = (mesh_channel_id_t)i;
        ch_config.world_state_capacity = config->world_state_capacity;
        ch_config.gossip_rounds_per_update = config->gossip_rounds;
        ch_config.convergence_threshold = config->convergence_threshold;
        ch_config.base_interval_ms = config->base_interval_ms;
        ch_config.jitter_amplitude_ms = config->jitter_amplitude_ms;
        
        integration->channels[i] = mesh_channel_create(&ch_config, 
                                                        integration->registry);
        if (!integration->channels[i]) {
            LOG_ERROR("Failed to create channel %d (%s)", i, channel_names[i]);
            mesh_integration_destroy(integration);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_integration_create: integration->channels is NULL");
            return NULL;
        }
    }
    
    /* Create coordinator pools for each channel */
    for (int i = 0; i < MESH_NUM_STANDARD_CHANNELS; i++) {
        mesh_coordinator_pool_config_t pool_config;
        mesh_coordinator_pool_default_config(&pool_config);
        pool_config.channel = (mesh_channel_id_t)i;
        pool_config.min_size = 1;
        pool_config.max_size = config->coordinators_per_channel;
        pool_config.election_timeout_ms = config->election_timeout_ms;

        integration->coordinator_pools[i] =
            mesh_coordinator_pool_create(&pool_config, integration->registry,
                                          integration->channels[i]);
        if (!integration->coordinator_pools[i]) {
            LOG_WARN("Failed to create coordinator pool for channel %d", i);
            /* Non-fatal - continue without pool */
        }
    }
    
    /* Create ordering service (requires a coordinator pool as orderer) */
    /* Use the SYSTEM channel's coordinator pool as the orderer pool */
    mesh_ordering_config_t ord_config;
    mesh_ordering_default_config(&ord_config);
    ord_config.batch_size = config->ordering_batch_size;
    ord_config.batch_timeout_ms = (float)config->ordering_batch_timeout_ms;

    integration->ordering = mesh_ordering_create(&ord_config,
                                                  integration->coordinator_pools[MESH_CHANNEL_SYSTEM]);
    if (!integration->ordering) {
        LOG_WARN("Failed to create ordering service - will operate without ordering");
        /* Non-fatal - continue without ordering service */
    }

    /* Create MSP */
    mesh_msp_config_t msp_config;
    mesh_msp_default_config(&msp_config);
    /* BBB and immune integration handled via handles - set to NULL for now */
    msp_config.bbb_handle = NULL;  /* Will be linked later if needed */
    msp_config.immune_handle = NULL;

    integration->msp = mesh_msp_create(&msp_config, integration->registry);
    if (!integration->msp) {
        LOG_WARN("Failed to create MSP - will operate without MSP");
        /* Non-fatal - continue without MSP */
    }

    /* Create endorsement collector */
    mesh_endorsement_collector_config_t end_config;
    mesh_endorsement_collector_default_config(&end_config);

    /* FIXME: endorsement_collector requires a mesh_pattern_router_t*, but integration
     * has no pattern router yet. Passing NULL defers router-dependent features. */
    integration->endorsement_collector =
        mesh_endorsement_collector_create(&end_config, NULL);
    if (!integration->endorsement_collector) {
        LOG_ERROR("Failed to create endorsement collector");
        mesh_integration_destroy(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_integration_create: integration->endorsement_collector is NULL");
        return NULL;
    }

    /* Cross-channel router requires a system coordinator - skip for now */
    /* Can be linked later via mesh_integration_link_system_coordinator() */
    integration->router = NULL;
    LOG_DEBUG("Cross-channel router not created - can be linked later");
    
    /* Create standard endorsement policies */
    create_standard_policies(integration);
    
    integration->is_initialized = true;
    integration->last_update_ns = nimcp_time_now_ns();
    
    LOG_INFO("Created mesh integration with %d channels, %zu max participants",
             MESH_NUM_STANDARD_CHANNELS, config->max_participants);
    
    return integration;
}

void mesh_integration_destroy(mesh_integration_t* integration) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) return;
    
    nimcp_mutex_lock(integration->mutex);
    
    /* Cleanup adapters */
    for (size_t i = 0; i < integration->adapter_count; i++) {
        if (integration->adapters[i]) {
            mesh_adapter_base_cleanup(integration->adapters[i]);
            integration->adapters[i] = NULL;
        }
    }
    
    /* Destroy router */
    if (integration->router) {
        mesh_cross_router_destroy(integration->router);
    }
    
    /* Destroy endorsement collector */
    if (integration->endorsement_collector) {
        mesh_endorsement_collector_destroy(integration->endorsement_collector);
    }
    
    /* Destroy MSP */
    if (integration->msp) {
        mesh_msp_destroy(integration->msp);
    }
    
    /* Destroy ordering service */
    if (integration->ordering) {
        mesh_ordering_destroy(integration->ordering);
    }
    
    /* Destroy coordinator pools */
    for (int i = 0; i < MESH_NUM_STANDARD_CHANNELS; i++) {
        if (integration->coordinator_pools[i]) {
            mesh_coordinator_pool_destroy(integration->coordinator_pools[i]);
        }
    }
    
    /* Destroy channels */
    for (int i = 0; i < MESH_NUM_STANDARD_CHANNELS; i++) {
        if (integration->channels[i]) {
            mesh_channel_destroy(integration->channels[i]);
        }
    }
    
    /* Destroy registry */
    if (integration->registry) {
        mesh_registry_destroy(integration->registry);
    }
    
    nimcp_mutex_unlock(integration->mutex);
    nimcp_mutex_destroy(integration->mutex);
    
    integration->magic = 0;
    nimcp_free(integration);
    
    LOG_INFO("Destroyed mesh integration");
}

/* ============================================================================
 * Public API: Component Access
 * ============================================================================ */

mesh_participant_registry_t* mesh_integration_get_registry(
    mesh_integration_t* integration
) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_integration_get_registry: integration is NULL");
        return NULL;
    }
    nimcp_mutex_lock(integration->mutex);
    mesh_participant_registry_t* val = integration->registry;
    nimcp_mutex_unlock(integration->mutex);
    return val;
}

mesh_channel_t* mesh_integration_get_channel(
    mesh_integration_t* integration,
    mesh_channel_id_t channel_id
) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_integration_get_channel: integration is NULL");
        return NULL;
    }
    if (channel_id >= MESH_NUM_STANDARD_CHANNELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_integration_get_channel: capacity exceeded");
        return NULL;
    }
    nimcp_mutex_lock(integration->mutex);
    mesh_channel_t* val = integration->channels[channel_id];
    nimcp_mutex_unlock(integration->mutex);
    return val;
}

mesh_ordering_service_t* mesh_integration_get_ordering(
    mesh_integration_t* integration
) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_integration_get_ordering: integration is NULL");
        return NULL;
    }
    nimcp_mutex_lock(integration->mutex);
    mesh_ordering_service_t* val = integration->ordering;
    nimcp_mutex_unlock(integration->mutex);
    return val;
}

mesh_msp_t* mesh_integration_get_msp(mesh_integration_t* integration) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_integration_get_msp: integration is NULL");
        return NULL;
    }
    nimcp_mutex_lock(integration->mutex);
    mesh_msp_t* val = integration->msp;
    nimcp_mutex_unlock(integration->mutex);
    return val;
}

mesh_endorsement_collector_t* mesh_integration_get_endorsement_collector(
    mesh_integration_t* integration
) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_integration_get_endorsement_collector: integration is NULL");
        return NULL;
    }
    nimcp_mutex_lock(integration->mutex);
    mesh_endorsement_collector_t* val = integration->endorsement_collector;
    nimcp_mutex_unlock(integration->mutex);
    return val;
}

mesh_cross_router_t mesh_integration_get_router(
    mesh_integration_t* integration
) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_integration_get_router: integration is NULL");
        return NULL;
    }
    nimcp_mutex_lock(integration->mutex);
    mesh_cross_router_t val = integration->router;
    nimcp_mutex_unlock(integration->mutex);
    return val;
}

mesh_coordinator_pool_t* mesh_integration_get_coordinator_pool(
    mesh_integration_t* integration,
    mesh_channel_id_t channel_id
) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_integration_get_coordinator_pool: integration is NULL");
        return NULL;
    }
    if (channel_id >= MESH_NUM_STANDARD_CHANNELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_integration_get_coordinator_pool: capacity exceeded");
        return NULL;
    }
    nimcp_mutex_lock(integration->mutex);
    mesh_coordinator_pool_t* val = integration->coordinator_pools[channel_id];
    nimcp_mutex_unlock(integration->mutex);
    return val;
}

/* ============================================================================
 * Public API: Adapter Registration
 * ============================================================================ */

nimcp_error_t mesh_integration_register_adapter(
    mesh_integration_t* integration,
    mesh_adapter_base_t* adapter_base
) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!adapter_base) return NIMCP_ERROR_NULL_POINTER;
    
    nimcp_mutex_lock(integration->mutex);
    
    if (integration->adapter_count >= MESH_MAX_ADAPTERS) {
        nimcp_mutex_unlock(integration->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_integration: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }
    
    /* Register with mesh registry */
    nimcp_error_t err = mesh_adapter_base_register(adapter_base, 
                                                    integration->registry);
    if (err != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(integration->mutex);
        return err;
    }
    
    /* Join home channel */
    mesh_channel_id_t home_channel = adapter_base->config.home_channel;
    if (home_channel < MESH_NUM_STANDARD_CHANNELS) {
        err = mesh_adapter_base_join_channel(adapter_base,
                                              integration->channels[home_channel]);
        if (err != NIMCP_SUCCESS) {
            LOG_WARN("Failed to join home channel %u", home_channel);
        }
    }
    
    /* Join secondary channels */
    for (size_t i = 0; i < adapter_base->config.secondary_channel_count; i++) {
        mesh_channel_id_t sec_channel = adapter_base->config.secondary_channels[i];
        if (sec_channel < MESH_NUM_STANDARD_CHANNELS) {
            mesh_adapter_base_join_channel(adapter_base,
                                           integration->channels[sec_channel]);
        }
    }
    
    /* Add to endorsement policies */
    for (size_t i = 0; i < adapter_base->config.policy_count; i++) {
        if (adapter_base->config.policies[i]) {
            mesh_adapter_base_add_to_policy(
                adapter_base,
                integration->endorsement_collector,
                adapter_base->config.policies[i],
                adapter_base->config.endorser_role
            );
        }
    }
    
    /* Track adapter */
    integration->adapters[integration->adapter_count++] = adapter_base;
    integration->stats.total_adapters++;
    
    if (home_channel < MESH_NUM_STANDARD_CHANNELS) {
        integration->stats.adapters_by_channel[home_channel]++;
    }
    
    nimcp_mutex_unlock(integration->mutex);
    
    LOG_INFO("Registered adapter '%s' (ID=0x%llx) with mesh integration",
             adapter_base->config.module_name,
             (unsigned long long)adapter_base->participant_id);
    
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_integration_unregister_adapter(
    mesh_integration_t* integration,
    mesh_participant_id_t participant_id
) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    
    nimcp_mutex_lock(integration->mutex);
    
    /* Find and remove adapter */
    for (size_t i = 0; i < integration->adapter_count; i++) {
        if (integration->adapters[i] && 
            integration->adapters[i]->participant_id == participant_id) {
            
            mesh_adapter_base_cleanup(integration->adapters[i]);
            
            /* Shift remaining adapters */
            for (size_t j = i; j < integration->adapter_count - 1; j++) {
                integration->adapters[j] = integration->adapters[j + 1];
            }
            integration->adapter_count--;
            integration->adapters[integration->adapter_count] = NULL;
            
            nimcp_mutex_unlock(integration->mutex);
            return NIMCP_SUCCESS;
        }
    }
    
    nimcp_mutex_unlock(integration->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_integration: error condition");
    return NIMCP_ERROR_NOT_FOUND;
}

mesh_adapter_base_t* mesh_integration_get_adapter(
    mesh_integration_t* integration,
    mesh_participant_id_t participant_id
) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_integration_get_adapter: integration is NULL");
        return NULL;
    }

    nimcp_mutex_lock(integration->mutex);
    mesh_adapter_base_t* result = NULL;
    for (size_t i = 0; i < integration->adapter_count; i++) {
        if (integration->adapters[i] &&
            integration->adapters[i]->participant_id == participant_id) {
            result = integration->adapters[i];
            break;
        }
    }
    nimcp_mutex_unlock(integration->mutex);

    /* Not found is normal lookup result, not an error */
    return result;
}

/* ============================================================================
 * Public API: Convenience Registration
 * ============================================================================ */

static mesh_participant_id_t register_with_category(
    mesh_integration_t* integration,
    void* module,
    const char* name,
    mesh_adapter_category_t category,
    const mesh_adapter_callbacks_t* callbacks
) {
    if (!integration || !module || !name) return 0;
    
    /* Allocate adapter base */
    mesh_adapter_base_t* base = nimcp_calloc(1, sizeof(mesh_adapter_base_t) + 
                                              sizeof(mesh_adapter_callbacks_t));
    if (!base) return 0;
    
    /* Setup configuration */
    mesh_adapter_config_t config;
    mesh_adapter_config_init(&config, name, category);
    
    /* Copy callbacks after base */
    if (callbacks) {
        memcpy((char*)base + sizeof(mesh_adapter_base_t), callbacks,
               sizeof(mesh_adapter_callbacks_t));
    }
    
    /* Initialize adapter base */
    if (mesh_adapter_base_init(base, module, &config, callbacks) != NIMCP_SUCCESS) {
        nimcp_free(base);
        return 0;
    }
    
    /* Register with integration */
    if (mesh_integration_register_adapter(integration, base) != NIMCP_SUCCESS) {
        mesh_adapter_base_cleanup(base);
        nimcp_free(base);
        return 0;
    }
    
    return base->participant_id;
}

mesh_participant_id_t mesh_integration_register_cognitive(
    mesh_integration_t* integration,
    void* module,
    const char* name,
    const mesh_adapter_callbacks_t* callbacks
) {
    return register_with_category(integration, module, name,
                                  MESH_ADAPTER_CATEGORY_COGNITIVE, callbacks);
}

mesh_participant_id_t mesh_integration_register_subcortical(
    mesh_integration_t* integration,
    void* module,
    const char* name,
    const mesh_adapter_callbacks_t* callbacks
) {
    return register_with_category(integration, module, name,
                                  MESH_ADAPTER_CATEGORY_SUBCORTICAL, callbacks);
}

mesh_participant_id_t mesh_integration_register_perception(
    mesh_integration_t* integration,
    void* module,
    const char* name,
    const mesh_adapter_callbacks_t* callbacks
) {
    return register_with_category(integration, module, name,
                                  MESH_ADAPTER_CATEGORY_PERCEPTION, callbacks);
}

mesh_participant_id_t mesh_integration_register_security(
    mesh_integration_t* integration,
    void* module,
    const char* name,
    const mesh_adapter_callbacks_t* callbacks
) {
    return register_with_category(integration, module, name,
                                  MESH_ADAPTER_CATEGORY_SECURITY, callbacks);
}

mesh_participant_id_t mesh_integration_register_gpu(
    mesh_integration_t* integration,
    void* module,
    const char* name,
    const mesh_adapter_callbacks_t* callbacks
) {
    return register_with_category(integration, module, name,
                                  MESH_ADAPTER_CATEGORY_GPU, callbacks);
}

mesh_participant_id_t mesh_integration_register_swarm(
    mesh_integration_t* integration,
    void* module,
    const char* name,
    const mesh_adapter_callbacks_t* callbacks
) {
    return register_with_category(integration, module, name,
                                  MESH_ADAPTER_CATEGORY_SWARM, callbacks);
}

/* ============================================================================
 * Public API: Special Component Registration
 * ============================================================================ */

mesh_participant_id_t mesh_integration_register_amygdala(
    mesh_integration_t* integration,
    void* amygdala
) {
    if (!integration || !amygdala) return 0;
    
    mesh_adapter_base_t* base = nimcp_calloc(1, sizeof(mesh_adapter_base_t));
    if (!base) return 0;
    
    mesh_adapter_config_t config;
    mesh_adapter_config_init(&config, "amygdala", MESH_ADAPTER_CATEGORY_SUBCORTICAL);
    config.endorser_role = ENDORSER_ROLE_VETO;  /* Amygdala can VETO */
    
    /* Add to emergency policy */
    static const char* amygdala_policies[] = { MESH_POLICY_EMERGENCY };
    config.policies = amygdala_policies;
    config.policy_count = 1;
    
    /* Also join both hemispheres for threat detection */
    config.secondary_channels[0] = MESH_CHANNEL_LEFT_HEMISPHERE;
    config.secondary_channels[1] = MESH_CHANNEL_RIGHT_HEMISPHERE;
    config.secondary_channel_count = 2;
    
    if (mesh_adapter_base_init(base, amygdala, &config, NULL) != NIMCP_SUCCESS) {
        nimcp_free(base);
        return 0;
    }
    
    if (mesh_integration_register_adapter(integration, base) != NIMCP_SUCCESS) {
        mesh_adapter_base_cleanup(base);
        nimcp_free(base);
        return 0;
    }
    
    /* Add as VETO endorser for emergency policy */
    mesh_adapter_base_add_to_policy(base, integration->endorsement_collector,
                                     MESH_POLICY_EMERGENCY, ENDORSER_ROLE_VETO);
    
    LOG_INFO("Registered amygdala with VETO role for emergency policy");
    return base->participant_id;
}

mesh_participant_id_t mesh_integration_register_hippocampus(
    mesh_integration_t* integration,
    void* hippocampus
) {
    if (!integration || !hippocampus) return 0;
    
    mesh_adapter_base_t* base = nimcp_calloc(1, sizeof(mesh_adapter_base_t));
    if (!base) return 0;
    
    mesh_adapter_config_t config;
    mesh_adapter_config_init(&config, "hippocampus", MESH_ADAPTER_CATEGORY_MEMORY);
    config.endorser_role = ENDORSER_ROLE_REQUIRED;
    
    /* Connect to both hemispheres for memory consolidation */
    config.secondary_channels[0] = MESH_CHANNEL_LEFT_HEMISPHERE;
    config.secondary_channels[1] = MESH_CHANNEL_RIGHT_HEMISPHERE;
    config.secondary_channel_count = 2;
    
    if (mesh_adapter_base_init(base, hippocampus, &config, NULL) != NIMCP_SUCCESS) {
        nimcp_free(base);
        return 0;
    }
    
    if (mesh_integration_register_adapter(integration, base) != NIMCP_SUCCESS) {
        mesh_adapter_base_cleanup(base);
        nimcp_free(base);
        return 0;
    }
    
    /* Add as REQUIRED endorser for memory_store policy */
    mesh_adapter_base_add_to_policy(base, integration->endorsement_collector,
                                     MESH_POLICY_MEMORY_STORE, ENDORSER_ROLE_REQUIRED);
    
    LOG_INFO("Registered hippocampus as REQUIRED for memory_store policy");
    return base->participant_id;
}

mesh_participant_id_t mesh_integration_register_thalamus(
    mesh_integration_t* integration,
    void* thalamus
) {
    if (!integration || !thalamus) return 0;
    
    mesh_adapter_base_t* base = nimcp_calloc(1, sizeof(mesh_adapter_base_t));
    if (!base) return 0;
    
    mesh_adapter_config_t config;
    mesh_adapter_config_init(&config, "thalamus", MESH_ADAPTER_CATEGORY_SUBCORTICAL);
    config.type = MESH_PARTICIPANT_GATEWAY;  /* Thalamus is a gateway */
    config.home_channel = MESH_CHANNEL_SYSTEM;  /* System-wide routing */
    
    /* Connect to ALL channels for routing */
    config.secondary_channels[0] = MESH_CHANNEL_LEFT_HEMISPHERE;
    config.secondary_channels[1] = MESH_CHANNEL_RIGHT_HEMISPHERE;
    config.secondary_channels[2] = MESH_CHANNEL_SUBCORTICAL;
    config.secondary_channels[3] = MESH_CHANNEL_GPU_COMPUTE;
    config.secondary_channel_count = 4;
    
    if (mesh_adapter_base_init(base, thalamus, &config, NULL) != NIMCP_SUCCESS) {
        nimcp_free(base);
        return 0;
    }
    
    if (mesh_integration_register_adapter(integration, base) != NIMCP_SUCCESS) {
        mesh_adapter_base_cleanup(base);
        nimcp_free(base);
        return 0;
    }
    
    LOG_INFO("Registered thalamus as GATEWAY for cross-channel routing");
    return base->participant_id;
}

mesh_participant_id_t mesh_integration_register_motor_cortex(
    mesh_integration_t* integration,
    void* motor_cortex
) {
    if (!integration || !motor_cortex) return 0;
    
    mesh_adapter_base_t* base = nimcp_calloc(1, sizeof(mesh_adapter_base_t));
    if (!base) return 0;
    
    mesh_adapter_config_t config;
    mesh_adapter_config_init(&config, "motor_cortex", MESH_ADAPTER_CATEGORY_MOTOR);
    config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;
    config.endorser_role = ENDORSER_ROLE_REQUIRED;
    
    if (mesh_adapter_base_init(base, motor_cortex, &config, NULL) != NIMCP_SUCCESS) {
        nimcp_free(base);
        return 0;
    }
    
    if (mesh_integration_register_adapter(integration, base) != NIMCP_SUCCESS) {
        mesh_adapter_base_cleanup(base);
        nimcp_free(base);
        return 0;
    }
    
    /* Add as REQUIRED endorser for motor_command policy */
    mesh_adapter_base_add_to_policy(base, integration->endorsement_collector,
                                     MESH_POLICY_MOTOR_COMMAND, ENDORSER_ROLE_REQUIRED);
    
    LOG_INFO("Registered motor_cortex as REQUIRED for motor_command policy");
    return base->participant_id;
}

mesh_participant_id_t mesh_integration_register_cerebellum(
    mesh_integration_t* integration,
    void* cerebellum
) {
    if (!integration || !cerebellum) return 0;
    
    mesh_adapter_base_t* base = nimcp_calloc(1, sizeof(mesh_adapter_base_t));
    if (!base) return 0;
    
    mesh_adapter_config_t config;
    mesh_adapter_config_init(&config, "cerebellum", MESH_ADAPTER_CATEGORY_GPU);
    config.home_channel = MESH_CHANNEL_GPU_COMPUTE;
    config.endorser_role = ENDORSER_ROLE_REQUIRED;
    
    /* Also connects to subcortical for motor coordination */
    config.secondary_channels[0] = MESH_CHANNEL_SUBCORTICAL;
    config.secondary_channel_count = 1;
    
    if (mesh_adapter_base_init(base, cerebellum, &config, NULL) != NIMCP_SUCCESS) {
        nimcp_free(base);
        return 0;
    }
    
    if (mesh_integration_register_adapter(integration, base) != NIMCP_SUCCESS) {
        mesh_adapter_base_cleanup(base);
        nimcp_free(base);
        return 0;
    }
    
    /* Add as REQUIRED endorser for motor_command policy */
    mesh_adapter_base_add_to_policy(base, integration->endorsement_collector,
                                     MESH_POLICY_MOTOR_COMMAND, ENDORSER_ROLE_REQUIRED);
    mesh_adapter_base_add_to_policy(base, integration->endorsement_collector,
                                     MESH_POLICY_GPU_BATCH, ENDORSER_ROLE_REQUIRED);
    
    LOG_INFO("Registered cerebellum for GPU channel and motor_command policy");
    return base->participant_id;
}

mesh_participant_id_t mesh_integration_register_prefrontal_cortex(
    mesh_integration_t* integration,
    void* pfc,
    bool is_left_hemisphere
) {
    if (!integration || !pfc) return 0;
    
    mesh_adapter_base_t* base = nimcp_calloc(1, sizeof(mesh_adapter_base_t));
    if (!base) return 0;
    
    const char* name = is_left_hemisphere ? "pfc_left" : "pfc_right";
    
    mesh_adapter_config_t config;
    mesh_adapter_config_init(&config, name, MESH_ADAPTER_CATEGORY_COGNITIVE);
    config.type = MESH_PARTICIPANT_COORDINATOR;  /* PFC is a coordinator */
    config.home_channel = is_left_hemisphere ? 
                          MESH_CHANNEL_LEFT_HEMISPHERE : 
                          MESH_CHANNEL_RIGHT_HEMISPHERE;
    config.endorser_role = ENDORSER_ROLE_REQUIRED;
    
    if (mesh_adapter_base_init(base, pfc, &config, NULL) != NIMCP_SUCCESS) {
        nimcp_free(base);
        return 0;
    }
    
    if (mesh_integration_register_adapter(integration, base) != NIMCP_SUCCESS) {
        mesh_adapter_base_cleanup(base);
        nimcp_free(base);
        return 0;
    }
    
    /* Add to cognitive and cross_hemisphere policies */
    mesh_adapter_base_add_to_policy(base, integration->endorsement_collector,
                                     MESH_POLICY_COGNITIVE, ENDORSER_ROLE_REQUIRED);
    mesh_adapter_base_add_to_policy(base, integration->endorsement_collector,
                                     MESH_POLICY_CROSS_HEMISPHERE, ENDORSER_ROLE_REQUIRED);
    
    LOG_INFO("Registered %s as COORDINATOR", name);
    return base->participant_id;
}

mesh_participant_id_t mesh_integration_register_basal_ganglia(
    mesh_integration_t* integration,
    void* basal_ganglia
) {
    if (!integration || !basal_ganglia) return 0;
    
    mesh_adapter_base_t* base = nimcp_calloc(1, sizeof(mesh_adapter_base_t));
    if (!base) return 0;
    
    mesh_adapter_config_t config;
    mesh_adapter_config_init(&config, "basal_ganglia", MESH_ADAPTER_CATEGORY_SUBCORTICAL);
    config.endorser_role = ENDORSER_ROLE_REQUIRED;
    
    if (mesh_adapter_base_init(base, basal_ganglia, &config, NULL) != NIMCP_SUCCESS) {
        nimcp_free(base);
        return 0;
    }
    
    if (mesh_integration_register_adapter(integration, base) != NIMCP_SUCCESS) {
        mesh_adapter_base_cleanup(base);
        nimcp_free(base);
        return 0;
    }
    
    /* Add to motor policy */
    mesh_adapter_base_add_to_policy(base, integration->endorsement_collector,
                                     MESH_POLICY_MOTOR_COMMAND, ENDORSER_ROLE_OPTIONAL);
    
    LOG_INFO("Registered basal_ganglia for motor selection");
    return base->participant_id;
}

/* ============================================================================
 * Public API: Update and Processing
 * ============================================================================ */

nimcp_error_t mesh_integration_update(
    mesh_integration_t* integration,
    uint64_t delta_ms
) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    
    nimcp_mutex_lock(integration->mutex);
    
    /* Update all channels */
    for (int i = 0; i < MESH_NUM_STANDARD_CHANNELS; i++) {
        if (integration->channels[i]) {
            mesh_channel_update(integration->channels[i], delta_ms);
        }
    }
    
    /* Update coordinator pools */
    for (int i = 0; i < MESH_NUM_STANDARD_CHANNELS; i++) {
        if (integration->coordinator_pools[i]) {
            mesh_coordinator_pool_update(integration->coordinator_pools[i], delta_ms);
        }
    }
    
    /* Update ordering service */
    if (integration->ordering) {
        mesh_ordering_update(integration->ordering, delta_ms);
    }
    
    integration->last_update_ns = nimcp_time_now_ns();
    
    nimcp_mutex_unlock(integration->mutex);
    
    return NIMCP_SUCCESS;
}

size_t mesh_integration_process_transactions(mesh_integration_t* integration) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) return 0;
    
    size_t processed = 0;
    
    nimcp_mutex_lock(integration->mutex);
    
    /* Create batch and sequence */
    if (integration->ordering) {
        mesh_ordering_create_batch(integration->ordering);
        mesh_ordering_sequence_batch(integration->ordering);
        
        mesh_ordering_stats_t stats;
        mesh_ordering_get_stats(integration->ordering, &stats);
        processed = stats.transactions_ordered;
    }
    
    integration->stats.transactions_total += processed;
    
    nimcp_mutex_unlock(integration->mutex);
    
    return processed;
}

bool mesh_integration_has_converged(const mesh_integration_t* integration) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) {
        return false;
    }

    nimcp_mutex_lock(((mesh_integration_t*)integration)->mutex);
    bool converged = true;
    for (int i = 0; i < MESH_NUM_STANDARD_CHANNELS; i++) {
        if (integration->channels[i] &&
            !mesh_channel_has_converged(integration->channels[i])) {
            converged = false;
            break;
        }
    }
    nimcp_mutex_unlock(((mesh_integration_t*)integration)->mutex);
    return converged;
}

float mesh_integration_get_free_energy(const mesh_integration_t* integration) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) return 1.0f;

    nimcp_mutex_lock(((mesh_integration_t*)integration)->mutex);
    float total_fe = 0.0f;
    int count = 0;

    for (int i = 0; i < MESH_NUM_STANDARD_CHANNELS; i++) {
        if (integration->channels[i]) {
            total_fe += mesh_channel_get_free_energy(integration->channels[i]);
            count++;
        }
    }
    nimcp_mutex_unlock(((mesh_integration_t*)integration)->mutex);

    return count > 0 ? total_fe / count : 1.0f;
}

/* ============================================================================
 * Public API: Transaction Submission
 * ============================================================================ */

nimcp_error_t mesh_integration_submit_transaction(
    mesh_integration_t* integration,
    mesh_transaction_t* tx
) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(integration->mutex);
    if (!integration->ordering) {
        nimcp_mutex_unlock(integration->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mesh_integration_submit_transaction: ordering is NULL");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }
    mesh_ordering_service_t* ordering = integration->ordering;
    nimcp_mutex_unlock(integration->mutex);

    /* Submit to ordering service */
    return mesh_ordering_submit(ordering, tx);
}

nimcp_error_t mesh_integration_submit_belief(
    mesh_integration_t* integration,
    mesh_participant_id_t proposer,
    const mesh_belief_t* belief
) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!belief) return NIMCP_ERROR_NULL_POINTER;

    /* Introduce belief to appropriate channel */
    mesh_channel_id_t channel_id = belief->channel;
    if (channel_id >= MESH_NUM_STANDARD_CHANNELS) {
        channel_id = MESH_CHANNEL_SYSTEM;
    }

    nimcp_mutex_lock(integration->mutex);
    mesh_channel_t* channel = integration->channels[channel_id];
    nimcp_mutex_unlock(integration->mutex);

    if (!channel) return NIMCP_ERROR_NOT_FOUND;

    return mesh_channel_introduce_belief(channel, belief);
}

/* ============================================================================
 * Public API: Statistics
 * ============================================================================ */

nimcp_error_t mesh_integration_get_stats(
    const mesh_integration_t* integration,
    mesh_integration_stats_t* stats
) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(((mesh_integration_t*)integration)->mutex);
    *stats = integration->stats;
    nimcp_mutex_unlock(((mesh_integration_t*)integration)->mutex);

    /* Compute free energy outside the lock (get_free_energy takes its own lock) */
    stats->system_free_energy = mesh_integration_get_free_energy(integration);

    return NIMCP_SUCCESS;
}

void mesh_integration_reset_stats(mesh_integration_t* integration) {
    if (!integration || integration->magic != MESH_INTEGRATION_MAGIC) return;
    
    nimcp_mutex_lock(integration->mutex);
    memset(&integration->stats, 0, sizeof(integration->stats));
    integration->stats.total_adapters = integration->adapter_count;
    nimcp_mutex_unlock(integration->mutex);
}
