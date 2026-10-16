/**
 * @file nimcp_mesh_adapter.c
 * @brief Mesh Network Adapter Framework Implementation
 *
 * WHAT: Generic adapter implementation for mesh network integration
 * WHY:  Standardized pattern for connecting NIMCP modules to mesh
 * HOW:  Base adapter with callbacks and registration helpers
 */

#include "mesh/nimcp_mesh_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

/* ============================================================================
 * Private: Default Callbacks
 * ============================================================================ */

static nimcp_error_t default_on_proposal(void* ctx, const mesh_transaction_t* tx) {
    (void)ctx;
    (void)tx;
    return NIMCP_SUCCESS;  /* Accept all proposals by default */
}

static nimcp_error_t default_on_endorse_request(
    void* ctx,
    const mesh_transaction_t* tx,
    mesh_endorsement_t* endorsement
) {
    mesh_adapter_base_t* base = (mesh_adapter_base_t*)ctx;
    
    /* Default: approve if module has callbacks, otherwise abstain */
    if (base && base->module) {
        endorsement->result = ENDORSEMENT_APPROVED;
    } else {
        endorsement->result = ENDORSEMENT_ABSTAIN;
    }
    
    endorsement->endorser_id = base ? base->participant_id : 0;
    endorsement->timestamp_ns = nimcp_time_now_ns();
    
    return NIMCP_SUCCESS;
}

static nimcp_error_t default_on_commit(void* ctx, const mesh_transaction_t* tx) {
    mesh_adapter_base_t* base = (mesh_adapter_base_t*)ctx;
    if (base) {
        base->commits_processed++;
    }
    (void)tx;
    return NIMCP_SUCCESS;
}

static nimcp_error_t default_on_belief_received(void* ctx, const mesh_belief_t* belief) {
    mesh_adapter_base_t* base = (mesh_adapter_base_t*)ctx;
    if (base) {
        base->beliefs_received++;
    }
    (void)belief;
    return NIMCP_SUCCESS;
}

static nimcp_error_t default_on_consensus_reached(
    void* ctx,
    const mesh_consensus_t* consensus
) {
    (void)ctx;
    (void)consensus;
    return NIMCP_SUCCESS;
}

static void default_get_beliefs(
    void* ctx,
    mesh_belief_set_t* beliefs_out
) {
    (void)ctx;
    if (beliefs_out) {
        beliefs_out->beliefs = NULL;
        beliefs_out->count = 0;
    }
}

static float default_get_free_energy(void* ctx) {
    (void)ctx;
    return 0.5f;  /* Neutral free energy */
}

static void default_get_health_metrics(void* ctx, health_metrics_t* metrics) {
    mesh_adapter_base_t* base = (mesh_adapter_base_t*)ctx;
    
    if (!metrics) return;
    
    memset(metrics, 0, sizeof(*metrics));
    metrics->participant = base ? base->participant_id : 0;
    metrics->cpu_utilization = 0.1f;
    metrics->memory_utilization = 0.1f;
    metrics->is_healthy = true;
    metrics->last_heartbeat_ns = nimcp_time_now_ns();
}

/* ============================================================================
 * Private: Wrapped Callbacks (call module-specific if available)
 * ============================================================================ */

typedef struct adapter_callback_ctx {
    mesh_adapter_base_t* base;
    mesh_adapter_callbacks_t* callbacks;
} adapter_callback_ctx_t;

static nimcp_error_t wrapped_on_proposal(void* ctx, const mesh_transaction_t* tx) {
    mesh_adapter_base_t* base = (mesh_adapter_base_t*)ctx;
    if (base) {
        base->proposals_received++;
    }
    return default_on_proposal(ctx, tx);
}

static nimcp_error_t wrapped_on_endorse_request(
    void* ctx,
    const mesh_transaction_t* tx,
    mesh_endorsement_t* endorsement
) {
    mesh_adapter_base_t* base = (mesh_adapter_base_t*)ctx;
    
    /* Check for module-specific endorsement handler */
    /* The callbacks are stored after the base in the adapter struct */
    mesh_adapter_callbacks_t* callbacks = 
        (mesh_adapter_callbacks_t*)((char*)base + sizeof(mesh_adapter_base_t));
    
    if (callbacks && callbacks->on_endorse && base->module) {
        endorsement->result = callbacks->on_endorse(base->module, tx);
    } else {
        endorsement->result = ENDORSEMENT_APPROVED;
    }
    
    endorsement->endorser_id = base->participant_id;
    endorsement->timestamp_ns = nimcp_time_now_ns();
    
    if (base) {
        base->endorsements_made++;
    }
    
    return NIMCP_SUCCESS;
}

static nimcp_error_t wrapped_on_commit(void* ctx, const mesh_transaction_t* tx) {
    mesh_adapter_base_t* base = (mesh_adapter_base_t*)ctx;
    
    mesh_adapter_callbacks_t* callbacks = 
        (mesh_adapter_callbacks_t*)((char*)base + sizeof(mesh_adapter_base_t));
    
    if (callbacks && callbacks->on_commit && base->module) {
        return callbacks->on_commit(base->module, tx);
    }
    
    return default_on_commit(ctx, tx);
}

static nimcp_error_t wrapped_on_belief_received(void* ctx, const mesh_belief_t* belief) {
    mesh_adapter_base_t* base = (mesh_adapter_base_t*)ctx;
    
    mesh_adapter_callbacks_t* callbacks = 
        (mesh_adapter_callbacks_t*)((char*)base + sizeof(mesh_adapter_base_t));
    
    if (callbacks && callbacks->on_belief && base->module) {
        return callbacks->on_belief(base->module, belief);
    }
    
    return default_on_belief_received(ctx, belief);
}

static float wrapped_get_free_energy(void* ctx) {
    mesh_adapter_base_t* base = (mesh_adapter_base_t*)ctx;
    
    mesh_adapter_callbacks_t* callbacks = 
        (mesh_adapter_callbacks_t*)((char*)base + sizeof(mesh_adapter_base_t));
    
    if (callbacks && callbacks->get_free_energy && base->module) {
        return callbacks->get_free_energy(base->module);
    }
    
    return default_get_free_energy(ctx);
}

static void wrapped_get_health_metrics(void* ctx, health_metrics_t* metrics) {
    mesh_adapter_base_t* base = (mesh_adapter_base_t*)ctx;
    
    mesh_adapter_callbacks_t* callbacks = 
        (mesh_adapter_callbacks_t*)((char*)base + sizeof(mesh_adapter_base_t));
    
    if (callbacks && callbacks->get_health && base->module) {
        callbacks->get_health(base->module, metrics);
        metrics->participant = base->participant_id;
        return;
    }
    
    default_get_health_metrics(ctx, metrics);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

nimcp_error_t mesh_adapter_base_init(
    mesh_adapter_base_t* base,
    void* module,
    const mesh_adapter_config_t* config,
    const mesh_adapter_callbacks_t* callbacks
) {
    if (!base || !config) return NIMCP_ERROR_NULL_POINTER;
    
    memset(base, 0, sizeof(*base));
    base->magic = MESH_ADAPTER_MAGIC;
    base->module = module;
    base->config = *config;
    
    /* Setup interface */
    memset(&base->interface, 0, sizeof(base->interface));
    
    if (config->module_name) {
        strncpy(base->interface.module_name, config->module_name, 
                MESH_MAX_NAME_LEN - 1);
    }
    
    base->interface.type = config->type;
    base->interface.home_channel = config->home_channel;
    base->interface.user_context = base;  /* Pass base as context */
    
    /* Setup callbacks - use wrapped versions that check for module-specific */
    base->interface.on_proposal = wrapped_on_proposal;
    base->interface.on_endorse_request = wrapped_on_endorse_request;
    base->interface.on_commit = wrapped_on_commit;
    base->interface.on_belief_received = wrapped_on_belief_received;
    base->interface.on_consensus_reached = default_on_consensus_reached;
    base->interface.get_beliefs = default_get_beliefs;
    base->interface.get_free_energy = wrapped_get_free_energy;
    base->interface.get_health_metrics = wrapped_get_health_metrics;
    
    LOG_DEBUG("Initialized mesh adapter for '%s' (category=%d, channel=%u)",
              config->module_name, config->category, config->home_channel);
    
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_adapter_base_register(
    mesh_adapter_base_t* base,
    mesh_participant_registry_t* registry
) {
    if (!base || base->magic != MESH_ADAPTER_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_adapter: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    
    mesh_participant_config_t participant_config;
    memset(&participant_config, 0, sizeof(participant_config));
    participant_config.module_name = base->config.module_name;
    participant_config.type = base->config.type;
    participant_config.home_channel = base->config.home_channel;
    participant_config.user_context = base;
    
    nimcp_error_t err = mesh_participant_register(
        registry,
        &base->interface,
        &participant_config,
        &base->participant_id
    );
    
    if (err == NIMCP_SUCCESS) {
        base->registry = registry;
        LOG_INFO("Registered mesh adapter '%s' with ID 0x%llx",
                 base->config.module_name, 
                 (unsigned long long)base->participant_id);
    }
    
    return err;
}

nimcp_error_t mesh_adapter_base_join_channel(
    mesh_adapter_base_t* base,
    mesh_channel_t* channel
) {
    if (!base || base->magic != MESH_ADAPTER_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_adapter: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!channel) return NIMCP_ERROR_NULL_POINTER;
    if (base->participant_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mesh_adapter: not initialized");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }
    
    nimcp_error_t err = mesh_channel_add_participant(channel, base->participant_id);
    
    if (err == NIMCP_SUCCESS) {
        LOG_DEBUG("Adapter '%s' joined channel %u",
                  base->config.module_name, mesh_channel_get_id(channel));
    }
    
    return err;
}

nimcp_error_t mesh_adapter_base_add_to_policy(
    mesh_adapter_base_t* base,
    mesh_endorsement_collector_t* collector,
    const char* policy_name,
    endorser_role_t role
) {
    if (!base || base->magic != MESH_ADAPTER_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_adapter: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!collector || !policy_name) return NIMCP_ERROR_NULL_POINTER;
    if (base->participant_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mesh_adapter: not initialized");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    /*
     * Brain-inspired pattern routing: No static policies needed.
     * Endorser selection is now based on pattern similarity with receptive fields.
     * This function is maintained for API compatibility but the actual endorser
     * selection happens in the pattern router based on activation levels.
     */
    LOG_DEBUG("Adapter '%s' role %d registered for pattern-based routing (policy_name: %s)",
              base->config.module_name, role, policy_name);

    (void)collector;  /* Pattern router handles endorser selection */

    return NIMCP_SUCCESS;
}

void mesh_adapter_base_cleanup(mesh_adapter_base_t* base) {
    if (!base || base->magic != MESH_ADAPTER_MAGIC) return;
    
    /* Unregister from mesh if registered */
    if (base->registry && base->participant_id != 0) {
        mesh_participant_unregister(base->registry, base->participant_id);
    }
    
    base->magic = 0;
    base->module = NULL;
    base->registry = NULL;
    base->participant_id = 0;
    
    LOG_DEBUG("Cleaned up mesh adapter '%s'", base->config.module_name);
}
