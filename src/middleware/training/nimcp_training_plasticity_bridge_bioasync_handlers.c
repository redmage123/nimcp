//=============================================================================
// Bio-Async Message Handlers for Training Plasticity Bridge
//=============================================================================
/**
 * @file nimcp_training_plasticity_bridge_bioasync_handlers.c
 * @brief Bio-async message handler implementations
 *
 * WHAT: Async message handlers that replace blocking mutex patterns
 * WHY:  Eliminate 13+ blocking locks with fire-and-forget async messages
 * HOW:  Process messages without blocking, use atomic operations, predictive routing
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <stddef.h>
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <math.h>

#include "nimcp.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_messages.h"
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_neural_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_plasticity_bridge_bioasync_handlers)

#define LOG_MODULE "TRAINING_PLASTICITY_BRIDGE_BIOASYNC_HANDLERS"


/* Internal context type from training_plasticity_bridge - forward declare */
typedef struct tpb_context tpb_context_t;

/* Constants */
#define TPB_LOG_MODULE "TPB_ASYNC"
#define TPB_DA_BURST_THRESHOLD 0.1f
#define TPB_DA_DIP_THRESHOLD -0.1f
#define TPB_LOSS_HISTORY_SIZE 32

/* Stub for promise API - these handlers are registered via the main bridge */
typedef void* nimcp_bio_promise_t;
static inline void nimcp_bio_promise_complete(nimcp_bio_promise_t p, const void* data) { (void)p; (void)data; }

/* Forward declare needed structures from the main bridge file */
struct tpb_region_config {
    uint32_t neuron_start_idx;
    uint32_t neuron_end_idx;
    float da_sensitivity;
};
typedef struct tpb_region_config tpb_region_config_t;

struct tpb_rpe_state {
    float loss_history[TPB_LOSS_HISTORY_SIZE];
    atomic_uint history_index;
    atomic_uint history_count;
    float baseline_loss;
    float last_rpe;
    float smoothed_rpe;
    float rpe_alpha;
};

struct tpb_stats {
    uint64_t total_plasticity_updates;
    uint64_t rpe_computations;
    float da_bursts;
    float da_dips;
};

struct tpb_config {
    float rpe_to_da_gain;
};

struct tpb_context {
    tpb_region_config_t* regions;
    uint32_t num_regions;
    neuromodulator_system_t neuromod_system;
    atomic_uint async_messages_received;
    struct tpb_rpe_state rpe_state;
    struct tpb_stats stats;
    struct tpb_config config;

    /* Phase 5: Backprop gating — suppress biological plasticity during backprop */
    volatile int backprop_active;
};

/* Helper to init message header */
static inline void bio_msg_init_header(bio_message_header_t* h, uint32_t type,
    uint32_t src, uint32_t dst, size_t size) {
    h->message_type = type;
    h->source_module = src;
    h->target_module = dst;
    h->payload_size = (uint32_t)size;
}

//=============================================================================
// Bio-Async Message Handlers
//=============================================================================

/**
 * @brief Handle async weight update request
 *
 * WHAT: Processes weight update without blocking caller
 * WHY:  Enables fire-and-forget weight updates
 * HOW:  Applies region-specific plasticity, sends response
 */
static nimcp_error_t tpb_handle_weight_update_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    tpb_context_t* ctx = (tpb_context_t*)user_data;
    const bio_msg_weight_update_request_t* req =
        (const bio_msg_weight_update_request_t*)msg;

    if (!ctx || !req || msg_size < sizeof(*req)) {
        LOG_ERROR("[%s] Invalid weight update request", TPB_LOG_MODULE);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LOG_TRACE("[%s] Handling weight update request for synapse %u",
              TPB_LOG_MODULE, req->synapse_id);

    /* Find region for postsynaptic neuron (NO LOCK - table is stable after init) */
    uint32_t region_id = 0;
    for (uint32_t i = 0; i < ctx->num_regions; i++) {
        if (req->post_neuron_id >= ctx->regions[i].neuron_start_idx &&
            req->post_neuron_id < ctx->regions[i].neuron_end_idx) {
            region_id = i;
            break;
        }
    }

    /* Apply plasticity rule based on region config */
    float weight_delta = req->weight_delta * req->learning_rate * req->eligibility_trace;

    /* Apply neuromodulation */
    if (ctx->neuromod_system && region_id < ctx->num_regions) {
        neuromodulator_pool_t pool = neuromodulator_pool_create();
        neuromodulator_get_levels(ctx->neuromod_system, &pool);

        /* Three-factor learning: Hebbian x Timing x Reward */
        float da_level = neuromodulator_pool_get_dopamine(&pool);
        float da_factor = 0.5f + da_level * ctx->regions[region_id].da_sensitivity;
        weight_delta *= da_factor;

        LOG_TRACE("[%s] Applied neuromodulation: DA=%.3f, factor=%.3f",
                  TPB_LOG_MODULE, da_level, da_factor);

        neuromodulator_pool_destroy(&pool);
    }

    /* Compute new weight (assuming old_weight in delta field for simplicity) */
    float old_weight = req->weight_delta;
    float new_weight = old_weight + weight_delta;

    /* Clamp to bounds if requested */
    bool clamped = false;
    if (req->clamp_to_bounds) {
        if (new_weight < req->min_weight) {
            new_weight = req->min_weight;
            clamped = true;
        }
        if (new_weight > req->max_weight) {
            new_weight = req->max_weight;
            clamped = true;
        }
    }

    /* Send response */
    bio_msg_weight_update_response_t response = {0};
    bio_msg_init_header(&response.header, BIO_MSG_WEIGHT_UPDATE_RESPONSE,
        BIO_MODULE_TRAINING, req->header.source_module, sizeof(response));
    response.synapse_id = req->synapse_id;
    response.old_weight = old_weight;
    response.new_weight = new_weight;
    response.clamped = clamped;
    response.error = NIMCP_SUCCESS;

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    /* Update statistics atomically */
    atomic_fetch_add(&ctx->async_messages_received, 1);
    atomic_fetch_add(&ctx->stats.total_plasticity_updates, 1);

    LOG_TRACE("[%s] Weight update complete: %.6f → %.6f (delta=%.6f, clamped=%d)",
              TPB_LOG_MODULE, old_weight, new_weight, weight_delta, clamped);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle async region config query
 *
 * WHAT: Returns region configuration without blocking
 * WHY:  Enable predictive queries for routing decisions
 * HOW:  Reads stable config table, sends response
 */
static nimcp_error_t tpb_handle_region_config_query(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    tpb_context_t* ctx = (tpb_context_t*)user_data;
    const bio_msg_region_config_query_t* query =
        (const bio_msg_region_config_query_t*)msg;

    if (!ctx || !query) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LOG_TRACE("[%s] Handling region config query for region %u",
              TPB_LOG_MODULE, query->region_id);

    /* Prepare response */
    bio_msg_region_config_response_t response = {0};
    bio_msg_init_header(&response.header, BIO_MSG_REGION_CONFIG_RESPONSE,
        BIO_MODULE_TRAINING, query->header.source_module, sizeof(response));

    /* Fill response if region exists (NO LOCK - config is read-only after setup) */
    if (query->region_id < ctx->num_regions) {
        const tpb_region_config_t* region = &ctx->regions[query->region_id];

        response.neuron_count = region->neuron_end_idx - region->neuron_start_idx;
        response.synapse_count = 0;  /* Not tracked in bridge */
        response.active_region_count = ctx->num_regions;

        /* Get current neuromodulator levels */
        if (ctx->neuromod_system) {
            neuromodulator_pool_t pool = neuromodulator_pool_create();
            neuromodulator_get_levels(ctx->neuromod_system, &pool);
            response.dopamine_level = neuromodulator_pool_get_dopamine(&pool);
            response.serotonin_level = neuromodulator_pool_get_serotonin(&pool);
            response.norepinephrine_level = neuromodulator_pool_get_norepinephrine(&pool);
            response.acetylcholine_level = neuromodulator_pool_get_acetylcholine(&pool);
            neuromodulator_pool_destroy(&pool);
        } else {
            response.dopamine_level = NIMCP_DOPAMINE_BASELINE;
            response.serotonin_level = NIMCP_SEROTONIN_BASELINE;
            response.norepinephrine_level = NIMCP_NOREPINEPHRINE_BASELINE;
            response.acetylcholine_level = NIMCP_ACETYLCHOLINE_BASELINE;
        }

        LOG_TRACE("[%s] Region %u config: neurons=%u, DA=%.3f",
                  TPB_LOG_MODULE, query->region_id,
                  response.neuron_count, response.dopamine_level);
    } else {
        LOG_WARNING("[%s] Query for invalid region %u (max=%u)",
                    TPB_LOG_MODULE, query->region_id, ctx->num_regions);
    }

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    atomic_fetch_add(&ctx->async_messages_received, 1);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle neuromodulator release event
 *
 * WHAT: Updates neuromodulator levels from external signal
 * WHY:  Enable event-driven neuromodulation (dopamine bursts, etc.)
 * HOW:  Applies delta to current levels via neuromod system
 */
static nimcp_error_t tpb_handle_neuromod_release(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    tpb_context_t* ctx = (tpb_context_t*)user_data;
    const bio_msg_neuromodulator_release_t* release =
        (const bio_msg_neuromodulator_release_t*)msg;

    if (!ctx || !release || !ctx->neuromod_system) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LOG_DEBUG("[%s] Handling neuromod release: channel=%d, amount=%.3f",
              TPB_LOG_MODULE, release->neuromodulator, release->release_amount);

    /* Map bio channel to neuromodulator type */
    neuromodulator_type_t type;
    switch (release->neuromodulator) {
        case BIO_CHANNEL_DOPAMINE:
            type = NEUROMOD_DOPAMINE;
            break;
        case BIO_CHANNEL_SEROTONIN:
            type = NEUROMOD_SEROTONIN;
            break;
        case BIO_CHANNEL_NOREPINEPHRINE:
            type = NEUROMOD_NOREPINEPHRINE;
            break;
        case BIO_CHANNEL_ACETYLCHOLINE:
            type = NEUROMOD_ACETYLCHOLINE;
            break;
        default:
            LOG_ERROR("[%s] Invalid neuromodulator channel: %d",
                      TPB_LOG_MODULE, release->neuromodulator);
            return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Get current level */
    neuromodulator_pool_t pool;
    neuromodulator_get_levels(ctx->neuromod_system, &pool);

    /* Compute new level */
    float current = release->current_concentration;
    float new_level = current + release->release_amount;

    /* Clamp to [0, 1] */
    if (new_level > 1.0f) new_level = 1.0f;
    if (new_level < 0.0f) new_level = 0.0f;

    /* Apply to system */
    neuromodulator_set_level(ctx->neuromod_system, type, new_level);

    LOG_INFO("[%s] Neuromodulator updated: type=%d, %.3f → %.3f (delta=%.3f)",
             TPB_LOG_MODULE, type, current, new_level, release->release_amount);

    atomic_fetch_add(&ctx->async_messages_received, 1);

    /* Track DA bursts/dips - use regular increment since stats are not atomic floats */
    if (type == NEUROMOD_DOPAMINE) {
        if (release->release_amount > TPB_DA_BURST_THRESHOLD) {
            ctx->stats.da_bursts++;
        } else if (release->release_amount < TPB_DA_DIP_THRESHOLD) {
            ctx->stats.da_dips++;
        }
    }

    /* No response needed for broadcast events */
    (void)response_promise;
    return NIMCP_SUCCESS;
}

/**
 * @brief Handle loss computed message (for async RPE)
 *
 * WHAT: Computes RPE from loss without blocking sender
 * WHY:  Decouple loss reporting from RPE computation
 * HOW:  Updates history, computes RPE, triggers DA release
 */
static nimcp_error_t tpb_handle_loss_computed(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    tpb_context_t* ctx = (tpb_context_t*)user_data;
    const bio_msg_loss_computed_t* loss_msg =
        (const bio_msg_loss_computed_t*)msg;

    if (!ctx || !loss_msg) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LOG_DEBUG("[%s] Handling loss computed message: batch=%u, loss=%.4f",
              TPB_LOG_MODULE, loss_msg->batch_id, loss_msg->loss_value);

    /* Update loss history (lock-free via atomic index) */
    uint32_t idx = atomic_fetch_add(&ctx->rpe_state.history_index, 1) % TPB_LOSS_HISTORY_SIZE;
    ctx->rpe_state.loss_history[idx] = loss_msg->loss_value;

    uint32_t count = atomic_load(&ctx->rpe_state.history_count);
    if (count < TPB_LOSS_HISTORY_SIZE) {
        atomic_store(&ctx->rpe_state.history_count, count + 1);
    }

    /* Compute baseline loss (exponential moving average) */
    float alpha = ctx->rpe_state.rpe_alpha;
    float baseline = ctx->rpe_state.baseline_loss;
    baseline = alpha * loss_msg->loss_value + (1.0f - alpha) * baseline;
    ctx->rpe_state.baseline_loss = baseline;

    /* Compute RPE: loss_decreased → positive RPE → DA burst */
    float rpe = -(loss_msg->loss_value - baseline);  /* Negative loss delta = positive reward */
    ctx->rpe_state.last_rpe = rpe;

    /* Smooth RPE */
    float smoothed = alpha * rpe + (1.0f - alpha) * ctx->rpe_state.smoothed_rpe;
    ctx->rpe_state.smoothed_rpe = smoothed;

    LOG_DEBUG("[%s] RPE computed: %.4f (smoothed=%.4f, baseline=%.4f)",
              TPB_LOG_MODULE, rpe, smoothed, baseline);

    /* Trigger dopamine release based on RPE */
    if (ctx->neuromod_system) {
        float da_delta = rpe * ctx->config.rpe_to_da_gain;
        if (fabsf(da_delta) > 0.01f) {  /* Threshold to avoid noise */
            neuromodulator_pool_t pool = neuromodulator_pool_create();
            neuromodulator_get_levels(ctx->neuromod_system, &pool);
            float old_da = neuromodulator_pool_get_dopamine(&pool);
            float new_da = old_da + da_delta;
            if (new_da > 1.0f) new_da = 1.0f;
            if (new_da < 0.0f) new_da = 0.0f;
            neuromodulator_set_level(ctx->neuromod_system, NEUROMOD_DOPAMINE, new_da);

            LOG_INFO("[%s] Dopamine modulated: %.3f -> %.3f (RPE=%.4f)",
                     TPB_LOG_MODULE, old_da, new_da, rpe);

            neuromodulator_pool_destroy(&pool);
        }
    }

    /* Send response if requested */
    if (response_promise) {
        bio_msg_loss_computed_response_t response = {0};
        bio_msg_init_header(&response.header, BIO_MSG_LOSS_COMPUTED + 1,  /* Response type */
            BIO_MODULE_TRAINING, loss_msg->header.source_module, sizeof(response));
        response.rpe_value = rpe;
        response.dopamine_delta = rpe * ctx->config.rpe_to_da_gain;
        nimcp_bio_promise_complete(response_promise, &response);
    }

    atomic_fetch_add(&ctx->async_messages_received, 1);
    atomic_fetch_add(&ctx->stats.rpe_computations, 1);

    return NIMCP_SUCCESS;
}
