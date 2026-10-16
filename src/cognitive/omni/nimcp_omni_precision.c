/**
 * @file nimcp_omni_precision.c
 * @brief Implementation of Omnidirectional Inference Precision Weighting
 */

#include "cognitive/omni/nimcp_omni_precision.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/omni/nimcp_omni_kg_sync.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(omni_precision, MESH_ADAPTER_CATEGORY_COGNITIVE)

void omni_precision_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_omni_precision_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int omni_precision_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_precision_training_begin: NULL argument");
        return -1;
    }
    omni_precision_heartbeat_instance(g_omni_precision_health_agent, "training_begin", 0.0f);
    (void)ctx;
    return 0;
}

int omni_precision_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_precision_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_precision_heartbeat_instance(g_omni_precision_health_agent, "training_step", progress);
    (void)ctx;
    return 0;
}

int omni_precision_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_precision_training_end: NULL argument");
        return -1;
    }
    omni_precision_heartbeat_instance(g_omni_precision_health_agent, "training_end", 1.0f);
    (void)ctx;
    return 0;
}

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PRECISION_EDGE_INITIAL_CAPACITY    64
#define PRECISION_EMA_ALPHA                0.1f

/* Bio-async module ID for precision context */
#define BIO_MODULE_OMNI_PRECISION          0x0E60

/* ============================================================================
 * Static Helpers
 * ============================================================================ */

static omni_module_precision_t* find_module(omni_precision_ctx_t* ctx,
                                             uint16_t module_id) {
    for (uint32_t i = 0; i < ctx->module_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->module_count > 256) {
            omni_precision_heartbeat("omni_precisi_loop",
                             (float)(i + 1) / (float)ctx->module_count);
        }

        if (ctx->modules[i].module_id == module_id && ctx->modules[i].active) {
            return &ctx->modules[i];
        }
    }
    return NULL;
}

static const omni_module_precision_t* find_module_const(
    const omni_precision_ctx_t* ctx,
    uint16_t module_id)
{
    for (uint32_t i = 0; i < ctx->module_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->module_count > 256) {
            omni_precision_heartbeat("omni_precisi_loop",
                             (float)(i + 1) / (float)ctx->module_count);
        }

        if (ctx->modules[i].module_id == module_id && ctx->modules[i].active) {
            return &ctx->modules[i];
        }
    }
    return NULL;
}

static float compute_bayesian_precision_update(float current_precision,
                                                float prediction_error,
                                                float learning_rate) {
    /* Bayesian update: precision increases when errors are small */
    float error_variance = prediction_error * prediction_error + 0.01f;
    float observed_precision = 1.0f / (fabsf(error_variance) > 1e-7f ? error_variance : 1e-7f);

    /* Blend current and observed precision */
    float new_precision = (1.0f - learning_rate) * current_precision +
                          learning_rate * observed_precision;

    return omni_precision_clamp(new_precision);
}

static float compute_gradient_precision_update(float current_precision,
                                                float prediction_error,
                                                float learning_rate) {
    /* Gradient descent on precision: maximize log-likelihood */
    float grad = 0.5f * (1.0f / current_precision -
                         prediction_error * prediction_error);
    float new_precision = current_precision + learning_rate * grad;

    return omni_precision_clamp(new_precision);
}

static float compute_ema_precision_update(float current_precision,
                                           float prediction_error,
                                           float learning_rate) {
    /* Exponential moving average of inverse error variance */
    float error_variance = prediction_error * prediction_error + 0.01f;
    float observed_precision = 1.0f / (fabsf(error_variance) > 1e-7f ? error_variance : 1e-7f);

    float new_precision = current_precision * (1.0f - learning_rate) +
                          observed_precision * learning_rate;

    return omni_precision_clamp(new_precision);
}

static void update_module_aggregate(omni_module_precision_t* module) {
    float sum = 0.0f;
    uint32_t count = 0;

    for (int i = 0; i < OMNI_PREC_CHANNEL_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OMNI_PREC_CHANNEL_COUNT > 256) {
            omni_precision_heartbeat("omni_precisi_loop",
                             (float)(i + 1) / (float)OMNI_PREC_CHANNEL_COUNT);
        }

        if (module->channels[i].enabled) {
            sum += module->channels[i].value;
            count++;
        }
    }

    module->aggregate_precision = (count > 0) ? (sum / count) : OMNI_PRECISION_DEFAULT;
    module->confidence = omni_precision_to_confidence(module->aggregate_precision);
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int omni_precision_default_config(omni_precision_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    memset(config, 0, sizeof(omni_precision_config_t));

    config->update_mode = OMNI_PREC_UPDATE_BAYESIAN;
    config->learning_rate = OMNI_PRECISION_DEFAULT_LR;
    config->decay_rate = OMNI_PRECISION_DECAY;

    config->route_mode = OMNI_PREC_ROUTE_INDEPENDENT;
    config->enable_propagation = true;

    config->min_precision = OMNI_PRECISION_MIN;
    config->max_precision = OMNI_PRECISION_MAX;

    config->enable_fep_integration = true;
    config->enable_kg_sync = true;
    config->enable_bio_async = true;

    config->enable_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

omni_precision_ctx_t* omni_precision_create(const omni_precision_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_create", 0.0f);


    omni_precision_ctx_t* ctx = nimcp_calloc(1, sizeof(omni_precision_ctx_t));
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ctx");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        memcpy(&ctx->config, config, sizeof(omni_precision_config_t));
    } else {
        omni_precision_default_config(&ctx->config);
    }

    /* Initialize edge array */
    ctx->edge_capacity = PRECISION_EDGE_INITIAL_CAPACITY;
    ctx->edges = nimcp_calloc(ctx->edge_capacity, sizeof(omni_precision_edge_t));
    if (!ctx->edges) {
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_precision_create: ctx->edges is NULL");
        return NULL;
    }

    /* Create mutex */
    ctx->mutex = nimcp_mutex_create(NULL);
    if (!ctx->mutex) {
        nimcp_free(ctx->edges);
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_precision_create: ctx->mutex is NULL");
        return NULL;
    }

    /* Initialize modules to default state */
    for (int i = 0; i < OMNI_PRECISION_MAX_MODULES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OMNI_PRECISION_MAX_MODULES > 256) {
            omni_precision_heartbeat("omni_precisi_loop",
                             (float)(i + 1) / (float)OMNI_PRECISION_MAX_MODULES);
        }

        ctx->modules[i].active = false;
        for (int j = 0; j < OMNI_PREC_CHANNEL_COUNT; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && OMNI_PREC_CHANNEL_COUNT > 256) {
                omni_precision_heartbeat("omni_precisi_loop",
                                 (float)(j + 1) / (float)OMNI_PREC_CHANNEL_COUNT);
            }

            ctx->modules[i].channels[j].value = OMNI_PRECISION_DEFAULT;
            ctx->modules[i].channels[j].variance = 1.0f;
            ctx->modules[i].channels[j].error_history = 0.0f;
            ctx->modules[i].channels[j].update_count = 0;
            ctx->modules[i].channels[j].enabled = false;
        }
    }

    memset(&ctx->stats, 0, sizeof(omni_precision_stats_t));
    ctx->stats.min_precision = OMNI_PRECISION_MAX;
    ctx->stats.max_precision = OMNI_PRECISION_MIN;

    return ctx;
}

void omni_precision_destroy(omni_precision_ctx_t* ctx) {
    if (!ctx) return;

    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_destroy", 0.0f);


    if (ctx->bio_async_connected) {
        omni_precision_disconnect_bio_async(ctx);
    }

    if (ctx->edges) {
        nimcp_free(ctx->edges);
    }

    if (ctx->mutex) {
        nimcp_mutex_destroy(ctx->mutex);
    }

    nimcp_free(ctx);
    ctx = NULL;
}

int omni_precision_reset(omni_precision_ctx_t* ctx) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_reset", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");

    nimcp_mutex_lock(ctx->mutex);

    /* Reset all module precisions */
    for (uint32_t i = 0; i < ctx->module_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->module_count > 256) {
            omni_precision_heartbeat("omni_precisi_loop",
                             (float)(i + 1) / (float)ctx->module_count);
        }

        for (int j = 0; j < OMNI_PREC_CHANNEL_COUNT; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && OMNI_PREC_CHANNEL_COUNT > 256) {
                omni_precision_heartbeat("omni_precisi_loop",
                                 (float)(j + 1) / (float)OMNI_PREC_CHANNEL_COUNT);
            }

            ctx->modules[i].channels[j].value = OMNI_PRECISION_DEFAULT;
            ctx->modules[i].channels[j].variance = 1.0f;
            ctx->modules[i].channels[j].error_history = 0.0f;
            ctx->modules[i].channels[j].update_count = 0;
        }
        ctx->modules[i].aggregate_precision = OMNI_PRECISION_DEFAULT;
        ctx->modules[i].confidence = 0.5f;
    }

    /* Reset statistics */
    memset(&ctx->stats, 0, sizeof(omni_precision_stats_t));
    ctx->stats.min_precision = OMNI_PRECISION_MAX;
    ctx->stats.max_precision = OMNI_PRECISION_MIN;

    nimcp_mutex_unlock(ctx->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

int omni_precision_register_module(omni_precision_ctx_t* ctx,
                                    uint16_t module_id,
                                    const char* name,
                                    float initial_precision) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_register_module", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    NIMCP_CHECK_THROW(name, NIMCP_ERROR_INVALID_PARAM, "name is NULL");

    nimcp_mutex_lock(ctx->mutex);

    /* Check if already registered */
    if (find_module(ctx, module_id) != NULL) {
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_SUCCESS; /* Already registered */
    }

    /* Find empty slot */
    if (ctx->module_count >= OMNI_PRECISION_MAX_MODULES) {
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    omni_module_precision_t* module = &ctx->modules[ctx->module_count];
    module->module_id = module_id;
    strncpy(module->module_name, name, sizeof(module->module_name) - 1);
    module->module_name[sizeof(module->module_name) - 1] = '\0';
    module->active = true;

    /* Initialize all channels with initial precision */
    for (int i = 0; i < OMNI_PREC_CHANNEL_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OMNI_PREC_CHANNEL_COUNT > 256) {
            omni_precision_heartbeat("omni_precisi_loop",
                             (float)(i + 1) / (float)OMNI_PREC_CHANNEL_COUNT);
        }

        module->channels[i].value = initial_precision;
        module->channels[i].variance = 1.0f;
        module->channels[i].error_history = 0.0f;
        module->channels[i].update_count = 0;
        module->channels[i].enabled = false; /* Must be explicitly enabled */
    }

    module->aggregate_precision = initial_precision;
    module->confidence = omni_precision_to_confidence(initial_precision);

    ctx->module_count++;

    nimcp_mutex_unlock(ctx->mutex);
    return NIMCP_SUCCESS;
}

int omni_precision_enable_channel(omni_precision_ctx_t* ctx,
                                   uint16_t module_id,
                                   omni_precision_channel_t channel,
                                   float initial_precision) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_enable_channel", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    NIMCP_CHECK_THROW(channel < OMNI_PREC_CHANNEL_COUNT, NIMCP_ERROR_INVALID_PARAM, "channel out of range");

    nimcp_mutex_lock(ctx->mutex);

    omni_module_precision_t* module = find_module(ctx, module_id);
    if (!module) {
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    module->channels[channel].value = omni_precision_clamp(initial_precision);
    module->channels[channel].enabled = true;
    update_module_aggregate(module);

    nimcp_mutex_unlock(ctx->mutex);
    return NIMCP_SUCCESS;
}

int omni_precision_unregister_module(omni_precision_ctx_t* ctx,
                                      uint16_t module_id) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_unregister_module", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");

    nimcp_mutex_lock(ctx->mutex);

    omni_module_precision_t* module = find_module(ctx, module_id);
    if (!module) {
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    module->active = false;

    nimcp_mutex_unlock(ctx->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int omni_precision_update(omni_precision_ctx_t* ctx,
                           uint16_t module_id,
                           omni_precision_channel_t channel,
                           float prediction_error) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_update", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    NIMCP_CHECK_THROW(channel < OMNI_PREC_CHANNEL_COUNT, NIMCP_ERROR_INVALID_PARAM, "channel out of range");

    nimcp_mutex_lock(ctx->mutex);

    omni_module_precision_t* module = find_module(ctx, module_id);
    if (!module) {
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    if (!module->channels[channel].enabled) {
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_ERROR_INVALID_STATE;
    }

    float current = module->channels[channel].value;
    float new_precision = 0.0f;

    switch (ctx->config.update_mode) {
        case OMNI_PREC_UPDATE_BAYESIAN:
            new_precision = compute_bayesian_precision_update(
                current, prediction_error, ctx->config.learning_rate);
            break;
        case OMNI_PREC_UPDATE_GRADIENT:
            new_precision = compute_gradient_precision_update(
                current, prediction_error, ctx->config.learning_rate);
            break;
        case OMNI_PREC_UPDATE_EXPONENTIAL:
            new_precision = compute_ema_precision_update(
                current, prediction_error, ctx->config.learning_rate);
            break;
        case OMNI_PREC_UPDATE_FIXED:
        default:
            new_precision = current;
            break;
    }

    module->channels[channel].value = new_precision;
    module->channels[channel].error_history =
        module->channels[channel].error_history * (1.0f - PRECISION_EMA_ALPHA) +
        fabsf(prediction_error) * PRECISION_EMA_ALPHA;
    module->channels[channel].update_count++;

    update_module_aggregate(module);

    /* Update statistics */
    ctx->stats.total_updates++;
    if (new_precision < ctx->stats.min_precision) {
        ctx->stats.min_precision = new_precision;
    }
    if (new_precision > ctx->stats.max_precision) {
        ctx->stats.max_precision = new_precision;
    }

    nimcp_mutex_unlock(ctx->mutex);
    return NIMCP_SUCCESS;
}

int omni_precision_update_from_fep(omni_precision_ctx_t* ctx,
                                    uint16_t module_id,
                                    const void* fep) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_update_from_fep", 0.0f);


    const fep_system_t* fep_sys = (const fep_system_t*)fep;
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    NIMCP_CHECK_THROW(fep, NIMCP_ERROR_INVALID_PARAM, "fep is NULL");

    /* Get FEP free energy as proxy for prediction error */
    float fe = fep_get_free_energy(fep_sys);
    if (isnan(fe)) fe = 0.0f;

    /* Update all enabled channels based on FEP state */
    nimcp_mutex_lock(ctx->mutex);

    omni_module_precision_t* module = find_module(ctx, module_id);
    if (!module) {
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    for (int i = 0; i < OMNI_PREC_CHANNEL_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OMNI_PREC_CHANNEL_COUNT > 256) {
            omni_precision_heartbeat("omni_precisi_loop",
                             (float)(i + 1) / (float)OMNI_PREC_CHANNEL_COUNT);
        }

        if (module->channels[i].enabled) {
            float current = module->channels[i].value;
            /* Higher free energy = lower precision (more uncertainty) */
            float new_precision = current / (1.0f + ctx->config.learning_rate * fe);
            module->channels[i].value = omni_precision_clamp(new_precision);
        }
    }

    update_module_aggregate(module);

    nimcp_mutex_unlock(ctx->mutex);
    return NIMCP_SUCCESS;
}

int omni_precision_propagate(omni_precision_ctx_t* ctx) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_propagate", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    if (!ctx->config.enable_propagation) return NIMCP_SUCCESS;

    nimcp_mutex_lock(ctx->mutex);

    /* Propagate precision through edges */
    for (uint32_t e = 0; e < ctx->edge_count; e++) {
        /* Phase 8: Loop progress heartbeat */
        if ((e & 0xFF) == 0 && ctx->edge_count > 256) {
            omni_precision_heartbeat("omni_precisi_loop",
                             (float)(e + 1) / (float)ctx->edge_count);
        }

        const omni_precision_edge_t* edge = &ctx->edges[e];

        const omni_module_precision_t* source =
            find_module_const(ctx, edge->source_module);
        omni_module_precision_t* target =
            find_module(ctx, edge->target_module);

        if (!source || !target) continue;
        if (!source->channels[edge->channel].enabled) continue;
        if (!target->channels[edge->channel].enabled) continue;

        float source_prec = source->channels[edge->channel].value;
        float target_prec = target->channels[edge->channel].value;

        /* Weighted blend of source precision into target */
        float propagated = target_prec * (1.0f - edge->weight) +
                           source_prec * edge->weight;
        target->channels[edge->channel].value = omni_precision_clamp(propagated);
    }

    /* Update aggregates for all affected modules */
    for (uint32_t i = 0; i < ctx->module_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->module_count > 256) {
            omni_precision_heartbeat("omni_precisi_loop",
                             (float)(i + 1) / (float)ctx->module_count);
        }

        if (ctx->modules[i].active) {
            update_module_aggregate(&ctx->modules[i]);
        }
    }

    ctx->stats.propagations++;

    nimcp_mutex_unlock(ctx->mutex);
    return NIMCP_SUCCESS;
}

int omni_precision_decay(omni_precision_ctx_t* ctx) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_decay", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");

    nimcp_mutex_lock(ctx->mutex);

    for (uint32_t i = 0; i < ctx->module_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->module_count > 256) {
            omni_precision_heartbeat("omni_precisi_loop",
                             (float)(i + 1) / (float)ctx->module_count);
        }

        if (!ctx->modules[i].active) continue;

        for (int j = 0; j < OMNI_PREC_CHANNEL_COUNT; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && OMNI_PREC_CHANNEL_COUNT > 256) {
                omni_precision_heartbeat("omni_precisi_loop",
                                 (float)(j + 1) / (float)OMNI_PREC_CHANNEL_COUNT);
            }

            if (!ctx->modules[i].channels[j].enabled) continue;

            float current = ctx->modules[i].channels[j].value;
            float decayed = current * ctx->config.decay_rate;

            /* Don't decay below floor */
            if (decayed < OMNI_PRECISION_FLOOR) {
                decayed = OMNI_PRECISION_FLOOR;
            }

            ctx->modules[i].channels[j].value = decayed;
        }

        update_module_aggregate(&ctx->modules[i]);
    }

    nimcp_mutex_unlock(ctx->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

float omni_precision_get(const omni_precision_ctx_t* ctx,
                          uint16_t module_id,
                          omni_precision_channel_t channel) {
    if (!ctx) return OMNI_PRECISION_DEFAULT;
    if (channel >= OMNI_PREC_CHANNEL_COUNT) return OMNI_PRECISION_DEFAULT;

    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_get", 0.0f);


    nimcp_mutex_lock(((omni_precision_ctx_t*)ctx)->mutex);

    const omni_module_precision_t* module = find_module_const(ctx, module_id);
    float result = OMNI_PRECISION_DEFAULT;

    if (module && module->channels[channel].enabled) {
        result = module->channels[channel].value;
    }

    nimcp_mutex_unlock(((omni_precision_ctx_t*)ctx)->mutex);
    return result;
}

float omni_precision_get_aggregate(omni_precision_ctx_t* ctx,
                                    uint16_t module_id) {
    if (!ctx) return OMNI_PRECISION_DEFAULT;

    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_get_aggregate", 0.0f);


    nimcp_mutex_lock(((omni_precision_ctx_t*)ctx)->mutex);

    const omni_module_precision_t* module = find_module_const(ctx, module_id);
    float result = module ? module->aggregate_precision : OMNI_PRECISION_DEFAULT;

    nimcp_mutex_unlock(((omni_precision_ctx_t*)ctx)->mutex);
    return result;
}

float omni_precision_get_confidence(omni_precision_ctx_t* ctx,
                                     uint16_t module_id) {
    if (!ctx) return 0.5f;

    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_get_confidence", 0.0f);


    nimcp_mutex_lock(((omni_precision_ctx_t*)ctx)->mutex);

    const omni_module_precision_t* module = find_module_const(ctx, module_id);
    float result = module ? module->confidence : 0.5f;

    nimcp_mutex_unlock(((omni_precision_ctx_t*)ctx)->mutex);
    return result;
}

int omni_precision_get_all_channels(const omni_precision_ctx_t* ctx,
                                     uint16_t module_id,
                                     float* precisions) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_get_all_channels", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    NIMCP_CHECK_THROW(precisions, NIMCP_ERROR_INVALID_PARAM, "precisions array is NULL");

    nimcp_mutex_lock(((omni_precision_ctx_t*)ctx)->mutex);

    const omni_module_precision_t* module = find_module_const(ctx, module_id);
    if (!module) {
        nimcp_mutex_unlock(((omni_precision_ctx_t*)ctx)->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    for (int i = 0; i < OMNI_PREC_CHANNEL_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OMNI_PREC_CHANNEL_COUNT > 256) {
            omni_precision_heartbeat("omni_precisi_loop",
                             (float)(i + 1) / (float)OMNI_PREC_CHANNEL_COUNT);
        }

        precisions[i] = module->channels[i].enabled ?
                        module->channels[i].value : 0.0f;
    }

    nimcp_mutex_unlock(((omni_precision_ctx_t*)ctx)->mutex);
    return NIMCP_SUCCESS;
}

int omni_precision_get_stats(omni_precision_ctx_t* ctx,
                              omni_precision_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_get_stats", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_INVALID_PARAM, "stats is NULL");

    nimcp_mutex_lock(((omni_precision_ctx_t*)ctx)->mutex);
    memcpy(stats, &ctx->stats, sizeof(omni_precision_stats_t));
    nimcp_mutex_unlock(((omni_precision_ctx_t*)ctx)->mutex);

    return NIMCP_SUCCESS;
}

int omni_precision_reset_stats(omni_precision_ctx_t* ctx) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");

    nimcp_mutex_lock(ctx->mutex);
    memset(&ctx->stats, 0, sizeof(omni_precision_stats_t));
    ctx->stats.min_precision = OMNI_PRECISION_MAX;
    ctx->stats.max_precision = OMNI_PRECISION_MIN;
    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Edge API
 * ============================================================================ */

int omni_precision_add_edge(omni_precision_ctx_t* ctx,
                             uint16_t source_module,
                             uint16_t target_module,
                             omni_precision_channel_t channel,
                             float weight) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_add_edge", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    NIMCP_CHECK_THROW(channel < OMNI_PREC_CHANNEL_COUNT, NIMCP_ERROR_INVALID_PARAM, "channel out of range");

    nimcp_mutex_lock(ctx->mutex);

    /* Expand edge array if needed */
    if (ctx->edge_count >= ctx->edge_capacity) {
        uint32_t new_capacity = ctx->edge_capacity * 2;
        omni_precision_edge_t* new_edges = nimcp_realloc(
            ctx->edges, new_capacity * sizeof(omni_precision_edge_t));
        if (!new_edges) {
            nimcp_mutex_unlock(ctx->mutex);
            return NIMCP_ERROR_NO_MEMORY;
        }
        ctx->edges = new_edges;
        ctx->edge_capacity = new_capacity;
    }

    omni_precision_edge_t* edge = &ctx->edges[ctx->edge_count];
    edge->source_module = source_module;
    edge->target_module = target_module;
    edge->channel = channel;
    edge->weight = weight;
    ctx->edge_count++;

    nimcp_mutex_unlock(ctx->mutex);
    return NIMCP_SUCCESS;
}

int omni_precision_add_bidirectional_edge(omni_precision_ctx_t* ctx,
                                           uint16_t module_a,
                                           uint16_t module_b,
                                           float weight) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_add_bidirectional_ed", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");

    /* Add forward edge */
    int rc = omni_precision_add_edge(ctx, module_a, module_b,
                                      OMNI_PREC_CHANNEL_FORWARD, weight);
    if (rc != NIMCP_SUCCESS) return rc;

    /* Add backward edge */
    rc = omni_precision_add_edge(ctx, module_b, module_a,
                                  OMNI_PREC_CHANNEL_BACKWARD, weight);
    return rc;
}

int omni_precision_remove_edge(omni_precision_ctx_t* ctx,
                                uint16_t source_module,
                                uint16_t target_module,
                                omni_precision_channel_t channel) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_remove_edge", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");

    nimcp_mutex_lock(ctx->mutex);

    for (uint32_t i = 0; i < ctx->edge_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->edge_count > 256) {
            omni_precision_heartbeat("omni_precisi_loop",
                             (float)(i + 1) / (float)ctx->edge_count);
        }

        if (ctx->edges[i].source_module == source_module &&
            ctx->edges[i].target_module == target_module &&
            ctx->edges[i].channel == channel) {
            /* Shift remaining edges */
            memmove(&ctx->edges[i], &ctx->edges[i + 1],
                    (ctx->edge_count - i - 1) * sizeof(omni_precision_edge_t));
            ctx->edge_count--;
            nimcp_mutex_unlock(ctx->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_mutex_unlock(ctx->mutex);
    return NIMCP_ERROR_NOT_FOUND;
}

/* ============================================================================
 * Integration API
 * ============================================================================ */

int omni_precision_connect_fep(omni_precision_ctx_t* ctx,
                                void* fep) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_connect_fep", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");

    nimcp_mutex_lock(ctx->mutex);
    ctx->fep = fep;
    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

int omni_precision_connect_kg_sync(omni_precision_ctx_t* ctx,
                                    omni_kg_sync_t* kg_sync) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_connect_kg_sync", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");

    nimcp_mutex_lock(ctx->mutex);
    ctx->kg_sync = kg_sync;
    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

int omni_precision_sync_to_kg(omni_precision_ctx_t* ctx) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_sync_to_kg", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    NIMCP_CHECK_THROW(ctx->kg_sync, NIMCP_ERROR_INVALID_STATE, "kg_sync is not connected");

    /* Sync precision weights to KG edge attributes */
    /* This would update MODULATES_PRECISION edges in the KG */
    /* Implementation depends on KG sync API */

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

static nimcp_error_t handle_precision_update_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_precision_ctx_t* ctx = (omni_precision_ctx_t*)user_data;
    if (!ctx || !msg) return NIMCP_ERROR_INVALID_PARAM;

    if (msg_size >= sizeof(omni_precision_update_msg_t)) {
        const omni_precision_update_msg_t* update =
            (const omni_precision_update_msg_t*)msg;

        /* Apply the precision update */
        omni_precision_update(ctx, update->source_module,
                              update->channel, update->prediction_error);
    }

    (void)response_promise;
    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_free_energy_report(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    const omni_precision_ctx_t* ctx = (const omni_precision_ctx_t*)user_data;
    if (!ctx || !msg) return NIMCP_ERROR_INVALID_PARAM;

    /* Update avg prediction error from free energy report */
    /* This enables FEP-based precision updates */

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int omni_precision_connect_bio_async(omni_precision_ctx_t* ctx) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    if (ctx->bio_async_connected) return NIMCP_SUCCESS;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_OMNI_PRECISION,
        .module_name = "omni_precision",
        .inbox_capacity = 64,
        .user_data = ctx
    };

    bio_module_context_t bio_ctx = bio_router_register_module(&info);
    if (!bio_ctx) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    ctx->bio_context = bio_ctx;

    bio_router_register_handler(bio_ctx, BIO_MSG_OMNI_PRECISION_UPDATE,
                                 handle_precision_update_request);
    bio_router_register_handler(bio_ctx, BIO_MSG_OMNI_FREE_ENERGY_REPORT,
                                 handle_free_energy_report);

    ctx->bio_async_connected = true;
    return NIMCP_SUCCESS;
}

int omni_precision_disconnect_bio_async(omni_precision_ctx_t* ctx) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    if (!ctx->bio_async_connected) return NIMCP_SUCCESS;

    if (ctx->bio_context) {
        bio_router_unregister_module(ctx->bio_context);
        ctx->bio_context = NULL;
    }

    ctx->bio_async_connected = false;
    return NIMCP_SUCCESS;
}

bool omni_precision_is_bio_async_connected(const omni_precision_ctx_t* ctx) {
    if (!ctx) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_is_bio_async_connect", 0.0f);


    return ctx->bio_async_connected;
}

int omni_precision_broadcast_update(omni_precision_ctx_t* ctx,
                                     uint16_t module_id,
                                     omni_precision_channel_t channel) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_broadcast_update", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    NIMCP_CHECK_THROW(ctx->bio_async_connected, NIMCP_ERROR_INVALID_STATE, "bio-async is not connected");

    nimcp_mutex_lock(ctx->mutex);

    const omni_module_precision_t* module = find_module_const(ctx, module_id);
    if (!module) {
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    omni_precision_update_msg_t msg = {
        .source_module = module_id,
        .channel = channel,
        .new_precision = module->channels[channel].value,
        .prediction_error = module->channels[channel].error_history,
        .timestamp_us = 0  /* Would use actual timestamp */
    };

    nimcp_mutex_unlock(ctx->mutex);

    /* Broadcast to all modules */
    bio_router_broadcast(ctx->bio_context, &msg, sizeof(msg));

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility API
 * ============================================================================ */

float omni_precision_from_variance(float variance) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_from_variance", 0.0f);


    if (variance <= 0.0f) variance = 0.01f;
    float precision = 1.0f / variance;
    return omni_precision_clamp(precision);
}

float omni_precision_weight_error(float error, float precision) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_weight_error", 0.0f);


    return precision * error;
}

float omni_precision_to_confidence(float precision) {
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_to_confidence", 0.0f);


    if (precision < 0.0f) precision = 0.0f;
    return precision / (precision + 1.0f);
}

float omni_precision_clamp(float precision) {
    if (precision < OMNI_PRECISION_MIN) return OMNI_PRECISION_MIN;
    if (precision > OMNI_PRECISION_MAX) return OMNI_PRECISION_MAX;
    /* Phase 8: Heartbeat at operation start */
    omni_precision_heartbeat("omni_precisi_clamp", 0.0f);


    return precision;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_precision_channel_to_string(omni_precision_channel_t channel) {
    switch (channel) {
        case OMNI_PREC_CHANNEL_FORWARD: return "FORWARD";
        case OMNI_PREC_CHANNEL_BACKWARD: return "BACKWARD";
        case OMNI_PREC_CHANNEL_LATERAL: return "LATERAL";
        case OMNI_PREC_CHANNEL_HIERARCHICAL_UP: return "HIERARCHICAL_UP";
        case OMNI_PREC_CHANNEL_HIERARCHICAL_DOWN: return "HIERARCHICAL_DOWN";
        case OMNI_PREC_CHANNEL_ASSOCIATIVE: return "ASSOCIATIVE";
        case OMNI_PREC_CHANNEL_TEMPORAL: return "TEMPORAL";
        default: return "UNKNOWN";
    }
}

const char* omni_precision_update_mode_to_string(omni_precision_update_mode_t mode) {
    switch (mode) {
        case OMNI_PREC_UPDATE_BAYESIAN: return "BAYESIAN";
        case OMNI_PREC_UPDATE_GRADIENT: return "GRADIENT";
        case OMNI_PREC_UPDATE_EXPONENTIAL: return "EXPONENTIAL";
        case OMNI_PREC_UPDATE_FIXED: return "FIXED";
        default: return "UNKNOWN";
    }
}

const char* omni_precision_route_mode_to_string(omni_precision_route_mode_t mode) {
    switch (mode) {
        case OMNI_PREC_ROUTE_INDEPENDENT: return "INDEPENDENT";
        case OMNI_PREC_ROUTE_HIERARCHICAL: return "HIERARCHICAL";
        case OMNI_PREC_ROUTE_GRAPH: return "GRAPH";
        case OMNI_PREC_ROUTE_BROADCAST: return "BROADCAST";
        default: return "UNKNOWN";
    }
}
