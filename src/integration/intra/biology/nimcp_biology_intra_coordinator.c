/**
 * @file nimcp_biology_intra_coordinator.c
 * @brief Biology Layer Intra-Layer Coordinator Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/intra/biology/nimcp_biology_intra_coordinator.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(biology_intra_coordinator)

typedef struct {
    void* module;
    nimcp_module_interface_t interface;
    bool connected;
} module_slot_t;

struct nimcp_biology_intra_struct {
    nimcp_biology_intra_config_t config;
    nimcp_layer_registry_t registry;
    module_slot_t epigenetics;
    module_slot_t neurogenesis;
    module_slot_t gene_expression;
    nimcp_biology_intra_state_t state;
    nimcp_biology_intra_stats_t stats;
    bool is_initialized;
};

nimcp_biology_intra_config_t nimcp_biology_intra_default_config(void) {
    nimcp_biology_intra_config_t config = {
        .enable_epigenetics = true,
        .enable_neurogenesis = true,
        .enable_gene_expression = true,
        .epigenetics_genesis_coupling = 0.5f,
        .epigenetics_expression_coupling = 0.6f,
        .genesis_expression_coupling = 0.4f,
        .sync_interval_ms = 10,
        .coherence_threshold = NIMCP_PHASE_COHERENCE_THRESHOLD,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_biology_intra_t nimcp_biology_intra_create(const nimcp_biology_intra_config_t* config) {
    nimcp_biology_intra_t coord = (nimcp_biology_intra_t)nimcp_calloc(1, sizeof(struct nimcp_biology_intra_struct));
    NIMCP_API_CHECK_ALLOC(coord, "Failed to allocate biology intra coordinator");
    coord->config = config ? *config : nimcp_biology_intra_default_config();
    return coord;
}

void nimcp_biology_intra_destroy(nimcp_biology_intra_t coord) {
    if (!coord) return;
    if (coord->is_initialized) nimcp_biology_intra_shutdown(coord);
    nimcp_free(coord);
}

nimcp_layer_error_t nimcp_biology_intra_init(nimcp_biology_intra_t coord, nimcp_layer_registry_t registry) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in biology_intra_init");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in biology_intra_init");
    if (coord->is_initialized) return NIMCP_LAYER_ERR_ALREADY_REGISTERED;
    coord->registry = registry;
    coord->is_initialized = true;
    coord->state.layer_coherence = 1.0f;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_intra_shutdown(nimcp_biology_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in biology_intra_shutdown");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    coord->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_intra_connect_epigenetics(nimcp_biology_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in connect_epigenetics");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL in connect_epigenetics");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL in connect_epigenetics");
    coord->epigenetics.module = module;
    coord->epigenetics.interface = *interface;
    coord->epigenetics.connected = true;
    coord->state.epigenetics_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_intra_connect_neurogenesis(nimcp_biology_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in connect_neurogenesis");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL in connect_neurogenesis");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL in connect_neurogenesis");
    coord->neurogenesis.module = module;
    coord->neurogenesis.interface = *interface;
    coord->neurogenesis.connected = true;
    coord->state.neurogenesis_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_intra_connect_gene_expression(nimcp_biology_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in connect_gene_expression");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL in connect_gene_expression");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL in connect_gene_expression");
    coord->gene_expression.module = module;
    coord->gene_expression.interface = *interface;
    coord->gene_expression.connected = true;
    coord->state.gene_expression_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_intra_update(nimcp_biology_intra_t coord, float dt) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in biology_intra_update");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    /* Update methylation level with decay toward baseline */
    coord->state.methylation_level += (0.5f - coord->state.methylation_level) * dt * 0.05f;
    coord->stats.avg_methylation = coord->stats.avg_methylation * NIMCP_EMA_DECAY_DEFAULT + coord->state.methylation_level * NIMCP_LEARNING_RATE_DEFAULT;
    coord->stats.avg_expression = coord->stats.avg_expression * NIMCP_EMA_DECAY_DEFAULT + coord->state.expression_level * NIMCP_LEARNING_RATE_DEFAULT;
    coord->stats.avg_coherence = coord->stats.avg_coherence * NIMCP_EMA_DECAY_DEFAULT + coord->state.layer_coherence * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_intra_sync(nimcp_biology_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in biology_intra_sync");
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_intra_send(nimcp_biology_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in biology_intra_send");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in biology_intra_send");
    (void)target_module;
    coord->stats.messages_sent++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_intra_broadcast(nimcp_biology_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in biology_intra_broadcast");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in biology_intra_broadcast");
    (void)source_module;
    coord->stats.messages_sent += BIOLOGY_MODULE_COUNT;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_intra_get_state(nimcp_biology_intra_t coord, nimcp_biology_intra_state_t* state_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in get_state");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL in get_state");
    *state_out = coord->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_intra_get_stats(nimcp_biology_intra_t coord, nimcp_biology_intra_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in get_stats");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL in get_stats");
    *stats_out = coord->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_biology_intra_get_coherence(nimcp_biology_intra_t coord) {
    return coord ? coord->state.layer_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_biology_intra_reset_stats(nimcp_biology_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in reset_stats");
    memset(&coord->stats, 0, sizeof(coord->stats));
    return NIMCP_LAYER_OK;
}
