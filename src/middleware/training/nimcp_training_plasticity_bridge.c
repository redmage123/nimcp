#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_training_plasticity_bridge.c - Training-Plasticity Integration Bridge
//=============================================================================
/**
 * @file nimcp_training_plasticity_bridge.c
 * @brief Implementation of Training-Plasticity Bridge
 *
 * Phase TPB-1: Full implementation connecting training pipeline to biological plasticity
 *
 * @author NIMCP Development Team
 * @version 1.0.0
 * @date 2025-11-27
 */

#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "middleware/training/nimcp_perception_training_bridge.h"
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/platform/nimcp_platform_rwlock.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_plasticity_bridge)

#define LOG_MODULE "TRAINING_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Constants
//=============================================================================

#define TPB_LOG_MODULE "TPB"
#define TPB_EPSILON 1e-10f
#define TPB_DA_BURST_THRESHOLD 0.5f
#define TPB_DA_DIP_THRESHOLD -0.5f

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal bridge context structure
 */
struct tpb_context {
    /* Configuration */
    tpb_config_t config;

    /* RPE state */
    tpb_rpe_state_t rpe_state;
    nimcp_mutex_t rpe_mutex;

    /* Region routing */
    uint32_t num_regions;
    tpb_region_config_t regions[TPB_MAX_REGIONS];
    nimcp_platform_rwlock_t region_rwlock;

    /* Neuromodulator system */
    neuromodulator_system_t neuromod_system;
    bool owns_neuromod;

    /* CoW manager */
    cow_manager_t cow_manager;
    bool owns_cow;

    /* Thread pool */
    nimcp_thread_pool_t* thread_pool;

    /* Event bus */
    event_bus_t event_bus;

    /* Statistics */
    tpb_stats_t stats;
    nimcp_mutex_t stats_mutex;

    /* Training context connection */
    nimcp_brain_training_ctx_t* training_ctx;

    /* Callback context (Phase TCB-1) */
    tcb_context_t* callback_ctx;
    uint64_t callback_stats[4];  /* loss, weight, epoch, divergence */
    float callback_modulation[TCB_EVENT_COUNT];  /* per-event modulation factors */
    nimcp_mutex_t callback_mutex;

    /* Perception-Cortical Training Integration (Phase XBI) */
    perception_training_bridge_t* perception_training;
    cortical_training_bridge_t* cortical_training;

    /* State */
    atomic_bool initialized;
    atomic_bool shutdown;

    /* Phase 5: Backprop gating — suppress biological plasticity during backprop */
    volatile int backprop_active;
};

/**
 * @brief Batch plasticity work item
 */
typedef struct {
    tpb_context_t* ctx;
    uint32_t start_idx;
    uint32_t end_idx;
    const uint32_t* pre_neuron_ids;
    const uint32_t* post_neuron_ids;
    const float* pre_activities;
    const float* post_activities;
    const float* spike_deltas;
    float* weights;
    uint32_t updates_applied;
} tpb_batch_work_t;

//=============================================================================
// Forward Declarations
//=============================================================================

static void tpb_batch_worker(void* arg);
static float tpb_compute_rpe_temporal_diff(tpb_context_t* ctx, float current_loss);
static float tpb_compute_rpe_exp_avg(tpb_context_t* ctx, float current_loss);
static float tpb_compute_rpe_sliding_window(tpb_context_t* ctx, float current_loss);
static float tpb_compute_rpe_adaptive(tpb_context_t* ctx, float current_loss);
static float tpb_apply_stdp_rule(tpb_context_t* ctx, uint32_t region_id,
                                  float pre, float post, float delta);
static float tpb_apply_bcm_rule(tpb_context_t* ctx, uint32_t region_id,
                                 float pre, float post, float threshold);
static uint32_t tpb_find_region_for_neuron(tpb_context_t* ctx, uint32_t neuron_id);
static void tpb_update_neuromod_from_rpe(tpb_context_t* ctx, float rpe);

//=============================================================================
// Default Configuration
//=============================================================================

tpb_config_t tpb_config_default(void)
{
    tpb_config_t config;
    memset(&config, 0, sizeof(config));

    /* RPE defaults */
    config.rpe_mode = TPB_RPE_EXPONENTIAL_AVG;
    config.rpe_window_size = TPB_DEFAULT_RPE_WINDOW;
    config.rpe_smoothing_alpha = 0.1F;
    config.rpe_to_da_gain = 0.5F;

    /* LR modulation defaults */
    config.lr_modulation.mode = TPB_NEUROMOD_BALANCED;
    config.lr_modulation.da_weight = 0.4F;
    config.lr_modulation.ach_weight = 0.3F;
    config.lr_modulation.ht5_weight = 0.2F;
    config.lr_modulation.ne_weight = 0.1F;
    config.lr_modulation.min_lr_multiplier = 0.1F;
    config.lr_modulation.max_lr_multiplier = 5.0F;
    config.lr_modulation.use_sigmoid_scaling = true;
    config.lr_modulation.sigmoid_steepness = 2.0F;

    /* Thread pool */
    config.thread_pool_size = TPB_DEFAULT_THREAD_POOL_SIZE;

    /* Memory management */
    config.enable_cow = true;
    config.cow_manager = NULL;
    config.neuromod_system = NULL;

    /* Events */
    config.event_bus = NULL;
    config.publish_events = false;

    return config;
}

tpb_config_t tpb_config_preset(const char* preset_name)
{
    tpb_config_t config = tpb_config_default();

    if (!preset_name) {
        return config;
    }

    if (strcmp(preset_name, "reinforcement") == 0) {
        /* Strong dopamine modulation for RL */
        config.rpe_mode = TPB_RPE_TEMPORAL_DIFF;
        config.rpe_to_da_gain = 0.8F;
        config.lr_modulation.mode = TPB_NEUROMOD_DA_PRIMARY;
        config.lr_modulation.da_weight = 0.7F;
        config.lr_modulation.max_lr_multiplier = 10.0F;
    }
    else if (strcmp(preset_name, "supervised") == 0) {
        /* Balanced modulation for supervised learning */
        config.rpe_mode = TPB_RPE_EXPONENTIAL_AVG;
        config.rpe_to_da_gain = 0.3F;
        config.lr_modulation.mode = TPB_NEUROMOD_BALANCED;
    }
    else if (strcmp(preset_name, "unsupervised") == 0) {
        /* ACh-dominant for attention-based learning */
        config.rpe_mode = TPB_RPE_ADAPTIVE;
        config.lr_modulation.mode = TPB_NEUROMOD_ACH_PRIMARY;
        config.lr_modulation.ach_weight = 0.6F;
    }
    else if (strcmp(preset_name, "biological") == 0) {
        /* Maximum biological realism */
        config.rpe_mode = TPB_RPE_ADAPTIVE;
        config.rpe_to_da_gain = 0.6F;
        config.lr_modulation.mode = TPB_NEUROMOD_BALANCED;
        config.lr_modulation.use_sigmoid_scaling = true;
        config.thread_pool_size = 8;
    }

    return config;
}

//=============================================================================
// Region Configuration Presets
//=============================================================================

tpb_region_config_t tpb_region_cortical_default(void)
{
    tpb_region_config_t region;
    memset(&region, 0, sizeof(region));

    region.type = TPB_REGION_CORTICAL;
    region.name = "Cortical";
    region.primary_rule = TPB_RULE_STDP;
    region.secondary_rule = TPB_RULE_HOMEOSTATIC;
    region.enable_three_factor = true;

    region.da_sensitivity = 0.8F;
    region.ach_sensitivity = 1.2F;  /* High ACh sensitivity for attention */
    region.ht5_sensitivity = 0.5F;
    region.ne_sensitivity = 0.7F;

    region.base_learning_rate = 0.01F;
    region.lr_modulation_strength = 0.5F;
    region.plasticity_window_ms = 50.0F;

    return region;
}

tpb_region_config_t tpb_region_striatal_default(void)
{
    tpb_region_config_t region;
    memset(&region, 0, sizeof(region));

    region.type = TPB_REGION_STRIATAL;
    region.name = "Striatal";
    region.primary_rule = TPB_RULE_STDP;
    region.secondary_rule = TPB_RULE_ELIGIBILITY;
    region.enable_three_factor = true;

    region.da_sensitivity = 1.5F;  /* High DA for reward learning */
    region.ach_sensitivity = 0.6F;
    region.ht5_sensitivity = 0.8F;
    region.ne_sensitivity = 0.4F;

    region.base_learning_rate = 0.02F;
    region.lr_modulation_strength = 0.8F;  /* Strong modulation */
    region.plasticity_window_ms = 100.0F;  /* Longer window for RL */

    return region;
}

tpb_region_config_t tpb_region_hippocampal_default(void)
{
    tpb_region_config_t region;
    memset(&region, 0, sizeof(region));

    region.type = TPB_REGION_HIPPOCAMPAL;
    region.name = "Hippocampal";
    region.primary_rule = TPB_RULE_BCM;
    region.secondary_rule = TPB_RULE_STDP;
    region.enable_three_factor = true;

    region.da_sensitivity = 0.6F;
    region.ach_sensitivity = 1.4F;  /* Critical for memory encoding */
    region.ht5_sensitivity = 0.7F;
    region.ne_sensitivity = 1.0F;

    region.base_learning_rate = 0.005F;
    region.lr_modulation_strength = 0.6F;
    region.plasticity_window_ms = 40.0F;

    return region;
}

tpb_region_config_t tpb_region_cerebellar_default(void)
{
    tpb_region_config_t region;
    memset(&region, 0, sizeof(region));

    region.type = TPB_REGION_CEREBELLAR;
    region.name = "Cerebellar";
    region.primary_rule = TPB_RULE_ELIGIBILITY;  /* Error-driven */
    region.secondary_rule = TPB_RULE_ANTI_HEBBIAN;
    region.enable_three_factor = false;  /* Supervised, not reward-based */

    region.da_sensitivity = 0.3F;
    region.ach_sensitivity = 0.5F;
    region.ht5_sensitivity = 0.4F;
    region.ne_sensitivity = 0.6F;

    region.base_learning_rate = 0.001F;  /* Slow, precise learning */
    region.lr_modulation_strength = 0.3F;
    region.plasticity_window_ms = 200.0F;

    return region;
}

tpb_region_config_t tpb_region_amygdala_default(void)
{
    tpb_region_config_t region;
    memset(&region, 0, sizeof(region));

    region.type = TPB_REGION_AMYGDALA;
    region.name = "Amygdala";
    region.primary_rule = TPB_RULE_HEBBIAN;
    region.secondary_rule = TPB_RULE_ELIGIBILITY;
    region.enable_three_factor = true;

    region.da_sensitivity = 1.0F;
    region.ach_sensitivity = 0.8F;
    region.ht5_sensitivity = 1.2F;  /* Fear/anxiety modulation */
    region.ne_sensitivity = 1.5F;   /* High NE for threat response */

    region.base_learning_rate = 0.05F;  /* Fast fear learning */
    region.lr_modulation_strength = 0.7F;
    region.plasticity_window_ms = 30.0F;

    return region;
}

tpb_region_config_t tpb_region_prefrontal_default(void)
{
    tpb_region_config_t region;
    memset(&region, 0, sizeof(region));

    region.type = TPB_REGION_PREFRONTAL;
    region.name = "Prefrontal";
    region.primary_rule = TPB_RULE_STDP;
    region.secondary_rule = TPB_RULE_HOMEOSTATIC;
    region.enable_three_factor = true;

    region.da_sensitivity = 1.2F;  /* D1/D2 working memory modulation */
    region.ach_sensitivity = 1.0F;
    region.ht5_sensitivity = 0.9F;
    region.ne_sensitivity = 1.1F;

    region.base_learning_rate = 0.008F;
    region.lr_modulation_strength = 0.5F;
    region.plasticity_window_ms = 60.0F;

    return region;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

tpb_context_t* tpb_create(const tpb_config_t* config)
{
    tpb_context_t* ctx = nimcp_malloc(sizeof(tpb_context_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tpb_create: failed to allocate bridge context");
        LOG_ERROR("[%s] Failed to allocate bridge context", TPB_LOG_MODULE);
        return NULL;
    }
    memset(ctx, 0, sizeof(tpb_context_t));

    /* Store configuration */
    if (config) {
        ctx->config = *config;
    } else {
        ctx->config = tpb_config_default();
    }

    /* Initialize callback modulation factors to 1.0 (no modulation) */
    for (int i = 0; i < TCB_EVENT_COUNT; i++) {
        ctx->callback_modulation[i] = 1.0F;
    }

    /* Initialize RPE state */
    ctx->rpe_state.mode = ctx->config.rpe_mode;
    ctx->rpe_state.rpe_alpha = ctx->config.rpe_smoothing_alpha;
    ctx->rpe_state.baseline_loss = 0.0F;
    ctx->rpe_state.baseline_variance = 1.0F;

    /* Initialize mutexes */
    if (nimcp_mutex_init(&ctx->rpe_mutex, NULL) != NIMCP_SUCCESS) {
        LOG_ERROR("[%s] Failed to init RPE mutex", TPB_LOG_MODULE);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "tpb_create: validation failed");
        return NULL;
    }

    if (nimcp_mutex_init(&ctx->stats_mutex, NULL) != NIMCP_SUCCESS) {
        LOG_ERROR("[%s] Failed to init stats mutex", TPB_LOG_MODULE);
        nimcp_mutex_destroy(&ctx->rpe_mutex);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "tpb_create: validation failed");
        return NULL;
    }

    if (nimcp_mutex_init(&ctx->callback_mutex, NULL) != NIMCP_SUCCESS) {
        LOG_ERROR("[%s] Failed to init callback mutex", TPB_LOG_MODULE);
        nimcp_mutex_destroy(&ctx->stats_mutex);
        nimcp_mutex_destroy(&ctx->rpe_mutex);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "tpb_create: validation failed");
        return NULL;
    }

    /* Initialize RW lock for regions */
    if (nimcp_platform_rwlock_init(&ctx->region_rwlock) != 0) {
        LOG_ERROR("[%s] Failed to init region rwlock", TPB_LOG_MODULE);
        nimcp_mutex_destroy(&ctx->callback_mutex);
        nimcp_mutex_destroy(&ctx->stats_mutex);
        nimcp_mutex_destroy(&ctx->rpe_mutex);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "tpb_create: validation failed");
        return NULL;
    }

    /* Create or use provided neuromodulator system */
    if (ctx->config.neuromod_system) {
        ctx->neuromod_system = ctx->config.neuromod_system;
        ctx->owns_neuromod = false;
    } else {
        neuromodulator_config_t neuromod_config;
        memset(&neuromod_config, 0, sizeof(neuromod_config));
        neuromod_config.baseline_dopamine = 0.5F;
        neuromod_config.baseline_serotonin = 0.5F;
        neuromod_config.baseline_acetylcholine = 0.5F;
        neuromod_config.baseline_norepinephrine = 0.5F;
        neuromod_config.dopamine_decay = 2.0F;
        neuromod_config.serotonin_decay = 10.0F;
        neuromod_config.acetylcholine_decay = 0.5F;
        neuromod_config.norepinephrine_decay = 3.0F;
        neuromod_config.reward_dopamine_gain = ctx->config.rpe_to_da_gain;

        ctx->neuromod_system = neuromodulator_system_create(&neuromod_config);
        if (!ctx->neuromod_system) {
            LOG_ERROR("[%s] Failed to create neuromodulator system", TPB_LOG_MODULE);
            nimcp_platform_rwlock_destroy(&ctx->region_rwlock);
            nimcp_mutex_destroy(&ctx->stats_mutex);
            nimcp_mutex_destroy(&ctx->rpe_mutex);
            nimcp_free(ctx);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tpb_create: ctx->neuromod_system is NULL");
            return NULL;
        }
        ctx->owns_neuromod = true;
    }

    /* Set up CoW support - actual managers are created per-snapshot */
    if (ctx->config.enable_cow) {
        if (ctx->config.cow_manager) {
            ctx->cow_manager = ctx->config.cow_manager;
            ctx->owns_cow = false;
        } else {
            /* Use sentinel value to indicate CoW is enabled
             * Actual managers are created in tpb_snapshot_weights */
            ctx->cow_manager = (cow_manager_t)(uintptr_t)1;
            ctx->owns_cow = true;
        }
    }

    /* Create thread pool */
    if (ctx->config.thread_pool_size > 0) {
        ctx->thread_pool = nimcp_pool_create(ctx->config.thread_pool_size);
        if (!ctx->thread_pool) {
            LOG_WARNING("[%s] ", TPB_LOG_MODULE, "Failed to create thread pool, using single-threaded");
        }
    }

    /* Event bus */
    ctx->event_bus = ctx->config.event_bus;

    /* Initialize state */
    atomic_store(&ctx->initialized, true);
    atomic_store(&ctx->shutdown, false);

    LOG_INFO("[%s] ", TPB_LOG_MODULE, "Created Training-Plasticity Bridge (RPE mode=%d, threads=%u)",
                   ctx->config.rpe_mode, ctx->config.thread_pool_size);

    return ctx;
}

nimcp_result_t tpb_connect_training(tpb_context_t* ctx, nimcp_brain_training_ctx_t* training_ctx)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(training_ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "training_ctx is NULL");

    ctx->training_ctx = training_ctx;

    LOG_INFO("[%s] ", TPB_LOG_MODULE, "Connected to brain training context");
    return NIMCP_SUCCESS;
}

void tpb_destroy(tpb_context_t* ctx)
{
    if (!ctx) {
        return;
    }
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "training_plasticity");

    /* Signal shutdown */
    atomic_store(&ctx->shutdown, true);

    /* Destroy thread pool first (waits for completion) */
    if (ctx->thread_pool) {
        nimcp_pool_wait(ctx->thread_pool);
        nimcp_pool_destroy(ctx->thread_pool);
        ctx->thread_pool = NULL;
    }

    /* Destroy owned resources */
    /* Note: cow_manager may be a sentinel value (uintptr_t)1 if we created it,
     * so only destroy if it's a real pointer (> 1) */
    if (ctx->owns_cow && ctx->cow_manager && (uintptr_t)ctx->cow_manager > 1) {
        cow_manager_destroy(ctx->cow_manager);
    }

    if (ctx->owns_neuromod && ctx->neuromod_system) {
        neuromodulator_system_destroy(ctx->neuromod_system);
    }

    /* Destroy synchronization primitives */
    nimcp_platform_rwlock_destroy(&ctx->region_rwlock);
    nimcp_mutex_destroy(&ctx->stats_mutex);
    nimcp_mutex_destroy(&ctx->rpe_mutex);
    nimcp_mutex_destroy(&ctx->callback_mutex);

    LOG_INFO("[%s] ", TPB_LOG_MODULE, "Destroyed Training-Plasticity Bridge");

    nimcp_free(ctx);
}

//=============================================================================
// Loss-Dopamine Connector (RPE)
//=============================================================================

nimcp_result_t tpb_report_loss(tpb_context_t* ctx, float loss, float* rpe_out)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    /* Validate loss */
    if (isnan(loss) || isinf(loss)) {
        LOG_WARNING("[%s] ", TPB_LOG_MODULE, "Invalid loss value: %f", loss);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "Invalid loss value: %f", loss);
    }

    nimcp_mutex_lock(&ctx->rpe_mutex);

    float rpe = 0.0F;

    /* Compute RPE based on mode */
    switch (ctx->rpe_state.mode) {
        case TPB_RPE_TEMPORAL_DIFF:
            rpe = tpb_compute_rpe_temporal_diff(ctx, loss);
            break;
        case TPB_RPE_EXPONENTIAL_AVG:
            rpe = tpb_compute_rpe_exp_avg(ctx, loss);
            break;
        case TPB_RPE_SLIDING_WINDOW:
            rpe = tpb_compute_rpe_sliding_window(ctx, loss);
            break;
        case TPB_RPE_ADAPTIVE:
            rpe = tpb_compute_rpe_adaptive(ctx, loss);
            break;
    }

    /* Store in history */
    ctx->rpe_state.loss_history[ctx->rpe_state.history_index] = loss;
    ctx->rpe_state.history_index = (ctx->rpe_state.history_index + 1) % TPB_LOSS_HISTORY_SIZE;
    if (ctx->rpe_state.history_count < TPB_LOSS_HISTORY_SIZE) {
        ctx->rpe_state.history_count++;
    }

    /* Smooth RPE */
    ctx->rpe_state.smoothed_rpe = ctx->rpe_state.rpe_alpha * rpe +
                                  (1.0F - ctx->rpe_state.rpe_alpha) * ctx->rpe_state.smoothed_rpe;
    ctx->rpe_state.last_rpe = rpe;

    nimcp_mutex_unlock(&ctx->rpe_mutex);

    /* Update neuromodulator levels based on RPE */
    tpb_update_neuromod_from_rpe(ctx, rpe);

    /* Update statistics */
    nimcp_mutex_lock(&ctx->stats_mutex);
    ctx->stats.rpe_computations++;
    if (rpe > 0) {
        ctx->stats.total_positive_rpe += rpe;
    } else {
        ctx->stats.total_negative_rpe += rpe;
    }
    if (rpe > TPB_DA_BURST_THRESHOLD) {
        ctx->stats.da_bursts++;
    } else if (rpe < TPB_DA_DIP_THRESHOLD) {
        ctx->stats.da_dips++;
    }
    /* Update running average */
    float n = (float)ctx->stats.rpe_computations;
    ctx->stats.avg_rpe = ctx->stats.avg_rpe * ((n - 1.0F) / n) + rpe / n;
    nimcp_mutex_unlock(&ctx->stats_mutex);

    /* Callback */
    if (ctx->config.on_rpe_computed) {
        ctx->config.on_rpe_computed(rpe, ctx->config.callback_user_data);
    }

    if (rpe_out) {
        *rpe_out = rpe;
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t tpb_inject_reward(tpb_context_t* ctx, float da_delta)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    /* Clamp delta */
    if (da_delta > 1.0F) da_delta = 1.0F;
    if (da_delta < -1.0F) da_delta = -1.0F;

    /* Direct neuromodulator update */
    tpb_update_neuromod_from_rpe(ctx, da_delta);

    LOG_DEBUG("[%s] ", TPB_LOG_MODULE, "Injected reward signal: DA delta = %.3f", da_delta);

    return NIMCP_SUCCESS;
}

nimcp_result_t tpb_get_rpe_state(tpb_context_t* ctx, tpb_rpe_state_t* state_out)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(state_out != NULL, NIMCP_ERROR_INVALID_PARAM, "state_out is NULL");

    nimcp_mutex_lock(&ctx->rpe_mutex);
    *state_out = ctx->rpe_state;
    nimcp_mutex_unlock(&ctx->rpe_mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// RPE Computation Helpers
//=============================================================================

static float tpb_compute_rpe_temporal_diff(tpb_context_t* ctx, float current_loss)
{
    /* Simple TD-style: RPE = previous_loss - current_loss
     * Positive when loss decreases (good learning)
     * Negative when loss increases (poor learning)
     *
     * For more sophisticated implementations, we'd subtract expected improvement,
     * but for initial implementation, we keep it simple.
     */
    if (ctx->rpe_state.history_count == 0) {
        /* First call - no previous loss to compare, RPE = 0 */
        return 0.0F;
    }

    /* Get previous loss from history */
    uint32_t prev_idx;
    if (ctx->rpe_state.history_index == 0) {
        prev_idx = TPB_LOSS_HISTORY_SIZE - 1;
    } else {
        prev_idx = ctx->rpe_state.history_index - 1;
    }
    float prev_loss = ctx->rpe_state.loss_history[prev_idx];

    /* RPE: Loss decrease = positive, Loss increase = negative */
    float rpe = prev_loss - current_loss;

    /* Scale by gain */
    return rpe * ctx->config.rpe_to_da_gain;
}

static float tpb_compute_rpe_exp_avg(tpb_context_t* ctx, float current_loss)
{
    /* EMA baseline: RPE = (baseline - current_loss) / baseline_variance */
    if (ctx->rpe_state.history_count == 0) {
        ctx->rpe_state.baseline_loss = current_loss;
        return 0.0F;
    }

    float alpha = ctx->rpe_state.rpe_alpha;

    /* RPE: positive when loss is lower than expected */
    float prediction_error = ctx->rpe_state.baseline_loss - current_loss;
    float rpe = prediction_error / (ctx->rpe_state.baseline_variance + TPB_EPSILON);

    /* Update baseline */
    ctx->rpe_state.baseline_loss = alpha * current_loss + (1.0F - alpha) * ctx->rpe_state.baseline_loss;

    /* Update variance estimate */
    float error_sq = prediction_error * prediction_error;
    ctx->rpe_state.baseline_variance = alpha * error_sq +
                                       (1.0F - alpha) * ctx->rpe_state.baseline_variance;

    return rpe * ctx->config.rpe_to_da_gain;
}

static float tpb_compute_rpe_sliding_window(tpb_context_t* ctx, float current_loss)
{
    /* Sliding window average baseline */
    if (ctx->rpe_state.history_count < ctx->config.rpe_window_size) {
        /* Not enough history, use simple comparison */
        if (ctx->rpe_state.history_count == 0) {
            ctx->rpe_state.baseline_loss = current_loss;
            return 0.0F;
        }
        return (ctx->rpe_state.baseline_loss - current_loss) * ctx->config.rpe_to_da_gain;
    }

    /* Compute window average */
    float sum = 0.0F;
    uint32_t window = ctx->config.rpe_window_size;
    uint32_t start_idx = (ctx->rpe_state.history_index + TPB_LOSS_HISTORY_SIZE - window) % TPB_LOSS_HISTORY_SIZE;

    for (uint32_t i = 0; i < window; i++) {
        uint32_t idx = (start_idx + i) % TPB_LOSS_HISTORY_SIZE;
        sum += ctx->rpe_state.loss_history[idx];
    }
    float avg_loss = sum / (float)window;

    /* RPE: positive when current loss is lower than average */
    float rpe = (avg_loss - current_loss) / (avg_loss + TPB_EPSILON);

    ctx->rpe_state.baseline_loss = avg_loss;

    return rpe * ctx->config.rpe_to_da_gain;
}

static float tpb_compute_rpe_adaptive(tpb_context_t* ctx, float current_loss)
{
    /* Adaptive baseline with variance tracking for normalization */
    if (ctx->rpe_state.history_count == 0) {
        ctx->rpe_state.baseline_loss = current_loss;
        ctx->rpe_state.baseline_variance = 1.0F;
        return 0.0F;
    }

    float alpha = ctx->rpe_state.rpe_alpha;

    /* Prediction error */
    float prediction_error = ctx->rpe_state.baseline_loss - current_loss;

    /* Normalize by standard deviation */
    float std_dev = sqrtf(ctx->rpe_state.baseline_variance + TPB_EPSILON);
    float rpe = prediction_error / std_dev;

    /* Clip extreme RPE values */
    if (rpe > 3.0F) rpe = 3.0F;
    if (rpe < -3.0F) rpe = -3.0F;

    /* Update baseline (EMA) */
    ctx->rpe_state.baseline_loss = alpha * current_loss + (1.0F - alpha) * ctx->rpe_state.baseline_loss;

    /* Update variance (EMA of squared error) */
    float error_sq = prediction_error * prediction_error;
    ctx->rpe_state.baseline_variance = alpha * error_sq +
                                       (1.0F - alpha) * ctx->rpe_state.baseline_variance;

    return rpe * ctx->config.rpe_to_da_gain;
}

static void tpb_update_neuromod_from_rpe(tpb_context_t* ctx, float rpe)
{
    if (!ctx->neuromod_system) {
        return;
    }

    /* Convert RPE to dopamine change
     * Positive RPE → DA burst
     * Negative RPE → DA dip
     */
    float da_delta = rpe * ctx->config.rpe_to_da_gain;

    /* Get current levels */
    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(ctx->neuromod_system, &pool);

    /* Update dopamine */
    float new_da = neuromodulator_pool_get_dopamine(&pool) + da_delta;
    if (new_da > 1.0F) new_da = 1.0F;
    if (new_da < 0.0F) new_da = 0.0F;

    /* Also update norepinephrine for arousal on large RPE */
    float ne_delta = fabsf(rpe) * 0.2F;  /* Arousal from any prediction error */
    float new_ne = neuromodulator_pool_get_norepinephrine(&pool) + ne_delta;
    if (new_ne > 1.0F) new_ne = 1.0F;

    /* Apply updates */
    neuromodulator_set_level(ctx->neuromod_system, NEUROMOD_DOPAMINE, new_da);
    neuromodulator_set_level(ctx->neuromod_system, NEUROMOD_NOREPINEPHRINE, new_ne);

    /* Cleanup pool tensors */
    neuromodulator_pool_destroy(&pool);
}

//=============================================================================
// Region-Specific Plasticity Router
//=============================================================================

nimcp_result_t tpb_configure_region(tpb_context_t* ctx, const tpb_region_config_t* region_config,
                                     uint32_t* region_id_out)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(region_config != NULL, NIMCP_ERROR_INVALID_PARAM, "region_config is NULL");

    nimcp_platform_rwlock_wrlock(&ctx->region_rwlock);

    if (ctx->num_regions >= TPB_MAX_REGIONS) {
        nimcp_platform_rwlock_wrunlock(&ctx->region_rwlock);
        LOG_ERROR("[%s] Maximum regions exceeded", TPB_LOG_MODULE);
        return NIMCP_ERROR_MEMORY;
    }

    uint32_t region_id = ctx->num_regions;
    ctx->regions[region_id] = *region_config;
    ctx->num_regions++;

    nimcp_platform_rwlock_wrunlock(&ctx->region_rwlock);

    if (region_id_out) {
        *region_id_out = region_id;
    }

    LOG_INFO("[%s] ", TPB_LOG_MODULE, "Configured region %u: %s (neurons %u-%u)",
                   region_id, region_config->name ? region_config->name : "unnamed",
                   region_config->neuron_start_idx, region_config->neuron_end_idx);

    return NIMCP_SUCCESS;
}

nimcp_result_t tpb_route_weight_update(tpb_context_t* ctx, uint32_t neuron_id,
                                        float pre_activity, float post_activity,
                                        float spike_time_delta, float* weight_delta_out)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(weight_delta_out != NULL, NIMCP_ERROR_INVALID_PARAM, "weight_delta_out is NULL");

    nimcp_platform_rwlock_rdlock(&ctx->region_rwlock);

    /* Find region for this neuron */
    uint32_t region_id = tpb_find_region_for_neuron(ctx, neuron_id);

    float weight_delta = 0.0F;

    if (region_id < ctx->num_regions) {
        tpb_region_config_t* region = &ctx->regions[region_id];

        /* Get modulated learning rate */
        float base_lr = region->base_learning_rate;
        float modulated_lr = base_lr;
        tpb_get_modulated_lr(ctx, region_id, base_lr, &modulated_lr);

        /* Apply primary plasticity rule */
        switch (region->primary_rule) {
            case TPB_RULE_STDP:
                weight_delta = tpb_apply_stdp_rule(ctx, region_id, pre_activity,
                                                    post_activity, spike_time_delta);
                break;
            case TPB_RULE_BCM:
                weight_delta = tpb_apply_bcm_rule(ctx, region_id, pre_activity,
                                                   post_activity, 0.5F);
                break;
            case TPB_RULE_HEBBIAN:
                /* Simple Hebbian: delta_w = lr * pre * post */
                weight_delta = modulated_lr * pre_activity * post_activity;
                break;
            case TPB_RULE_ANTI_HEBBIAN:
                /* Anti-Hebbian: delta_w = -lr * pre * post */
                weight_delta = -modulated_lr * pre_activity * post_activity;
                break;
            case TPB_RULE_ELIGIBILITY:
                /* Eligibility trace rule: error-driven learning
                 * delta_w = lr * eligibility * error_signal
                 * spike_time_delta is used as error signal */
                {
                    float eligibility = pre_activity * post_activity;
                    float error_signal = spike_time_delta * 0.1F;  /* Scale error */
                    weight_delta = modulated_lr * eligibility * error_signal;
                }
                break;
            case TPB_RULE_HOMEOSTATIC:
                /* Homeostatic plasticity: maintain target firing rate
                 * delta_w = lr * (target_rate - post_activity) * pre_activity */
                {
                    float target_rate = 0.5F;  /* Default target */
                    weight_delta = modulated_lr * (target_rate - post_activity) * pre_activity;
                }
                break;
            default:
                weight_delta = 0.0F;
                break;
        }

        /* Apply learning rate modulation only for rules that use base_lr internally
         * (STDP/BCM). Rules already using modulated_lr skip this to avoid double modulation. */
        if (region->primary_rule == TPB_RULE_STDP || region->primary_rule == TPB_RULE_BCM) {
            weight_delta *= modulated_lr / (base_lr + TPB_EPSILON);
        }

        /* Update stats */
        nimcp_mutex_lock(&ctx->stats_mutex);
        ctx->stats.total_plasticity_updates++;
        ctx->stats.region_updates[region_id]++;
        float n = (float)ctx->stats.region_updates[region_id];
        ctx->stats.region_avg_delta[region_id] =
            ctx->stats.region_avg_delta[region_id] * ((n - 1.0F) / n) + fabsf(weight_delta) / n;
        nimcp_mutex_unlock(&ctx->stats_mutex);
    }

    nimcp_platform_rwlock_rdunlock(&ctx->region_rwlock);

    *weight_delta_out = weight_delta;
    return NIMCP_SUCCESS;
}

nimcp_result_t tpb_apply_plasticity_batch(tpb_context_t* ctx, uint32_t num_synapses,
                                           const uint32_t* pre_neuron_ids,
                                           const uint32_t* post_neuron_ids,
                                           const float* pre_activities,
                                           const float* post_activities,
                                           const float* spike_deltas,
                                           float* weights)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(pre_neuron_ids != NULL, NIMCP_ERROR_INVALID_PARAM, "pre_neuron_ids is NULL");
    NIMCP_CHECK_THROW(post_neuron_ids != NULL, NIMCP_ERROR_INVALID_PARAM, "post_neuron_ids is NULL");
    NIMCP_CHECK_THROW(pre_activities != NULL, NIMCP_ERROR_INVALID_PARAM, "pre_activities is NULL");
    NIMCP_CHECK_THROW(post_activities != NULL, NIMCP_ERROR_INVALID_PARAM, "post_activities is NULL");
    NIMCP_CHECK_THROW(spike_deltas != NULL, NIMCP_ERROR_INVALID_PARAM, "spike_deltas is NULL");
    NIMCP_CHECK_THROW(weights != NULL, NIMCP_ERROR_INVALID_PARAM, "weights is NULL");
    NIMCP_CHECK_THROW(num_synapses > 0, NIMCP_ERROR_INVALID_PARAM, "num_synapses is zero");

    struct timespec ts_start;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    uint64_t start_time = (uint64_t)ts_start.tv_sec * 1000000000ULL + (uint64_t)ts_start.tv_nsec;

    if (ctx->thread_pool && num_synapses > 1000) {
        /* Parallel execution for large batches */
        uint32_t num_workers = ctx->config.thread_pool_size;
        if (num_workers == 0) num_workers = 1;

        uint32_t chunk_size = (num_synapses + num_workers - 1) / num_workers;
        tpb_batch_work_t* work_items = nimcp_malloc(num_workers * sizeof(tpb_batch_work_t));
        if (!work_items) {
            return NIMCP_ERROR_MEMORY;
        }

        /* Submit work */
        for (uint32_t i = 0; i < num_workers; i++) {
            work_items[i].ctx = ctx;
            work_items[i].start_idx = i * chunk_size;
            work_items[i].end_idx = (i + 1) * chunk_size;
            if (work_items[i].end_idx > num_synapses) {
                work_items[i].end_idx = num_synapses;
            }
            work_items[i].pre_neuron_ids = pre_neuron_ids;
            work_items[i].post_neuron_ids = post_neuron_ids;
            work_items[i].pre_activities = pre_activities;
            work_items[i].post_activities = post_activities;
            work_items[i].spike_deltas = spike_deltas;
            work_items[i].weights = weights;
            work_items[i].updates_applied = 0;

            nimcp_result_t rc = nimcp_pool_submit(ctx->thread_pool, tpb_batch_worker, &work_items[i]);
            if (rc != NIMCP_OK) {
                NIMCP_LOGGING_ERROR("tpb_apply_batch_stdp: pool submit failed for worker %u (rc=%d)", i, rc);
            }
        }

        /* Wait for completion */
        nimcp_pool_wait(ctx->thread_pool);

        /* Aggregate statistics */
        uint32_t total_updates = 0;
        for (uint32_t i = 0; i < num_workers; i++) {
            total_updates += work_items[i].updates_applied;
        }

        nimcp_free(work_items);
    } else {
        /* Sequential execution */
        for (uint32_t i = 0; i < num_synapses; i++) {
            float delta = 0.0F;
            tpb_route_weight_update(ctx, post_neuron_ids[i], pre_activities[i],
                                    post_activities[i], spike_deltas[i], &delta);
            weights[i] += delta;
        }
    }

    /* Update timing statistics (wall time, not CPU time) */
    struct timespec ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    uint64_t elapsed = ((uint64_t)ts_end.tv_sec * 1000000000ULL + (uint64_t)ts_end.tv_nsec) - start_time;
    nimcp_mutex_lock(&ctx->stats_mutex);
    ctx->stats.total_time_ns += elapsed;
    nimcp_mutex_unlock(&ctx->stats_mutex);

    return NIMCP_SUCCESS;
}

static void tpb_batch_worker(void* arg)
{
    tpb_batch_work_t* work = (tpb_batch_work_t*)arg;
    if (!work) return;

    for (uint32_t i = work->start_idx; i < work->end_idx; i++) {
        float delta = 0.0F;
        if (tpb_route_weight_update(work->ctx, work->post_neuron_ids[i],
                                     work->pre_activities[i], work->post_activities[i],
                                     work->spike_deltas[i], &delta) == NIMCP_SUCCESS) {
            work->weights[i] += delta;
            work->updates_applied++;
        }
    }
}

static uint32_t tpb_find_region_for_neuron(tpb_context_t* ctx, uint32_t neuron_id)
{
    /* Binary search would be better for many regions, but linear is fine for < 32 */
    for (uint32_t i = 0; i < ctx->num_regions; i++) {
        if (neuron_id >= ctx->regions[i].neuron_start_idx &&
            neuron_id < ctx->regions[i].neuron_end_idx) {
            return i;
        }
    }
    return ctx->num_regions;  /* Not found */
}

//=============================================================================
// Plasticity Rule Implementations
//=============================================================================

static float tpb_apply_stdp_rule(tpb_context_t* ctx, uint32_t region_id,
                                  float pre, float post, float delta_t)
{
    tpb_region_config_t* region = &ctx->regions[region_id];

    /* STDP parameters */
    float tau_plus = region->plasticity_window_ms;
    float tau_minus = region->plasticity_window_ms;
    float a_plus = 0.005F;
    float a_minus = 0.00525F;

    /* THRESHOLD GATE: Skip update if pre or post activity is negligible.
     * WHY:  Without threshold, near-zero activities produce trivial weight
     *       changes that waste computation and introduce FP noise.
     */
    static const float STDP_ACTIVITY_THRESHOLD = 0.1F;
    if (fabsf(pre) < STDP_ACTIVITY_THRESHOLD || fabsf(post) < STDP_ACTIVITY_THRESHOLD) {
        return 0.0F;
    }

    float weight_change = 0.0F;

    if (delta_t > 0) {
        /* Pre-before-post: LTP */
        weight_change = a_plus * expf(-delta_t / tau_plus) * pre * post;

        nimcp_mutex_lock(&ctx->stats_mutex);
        ctx->stats.stdp_updates++;
        nimcp_mutex_unlock(&ctx->stats_mutex);
    } else if (delta_t < 0) {
        /* Post-before-pre: LTD */
        weight_change = -a_minus * expf(delta_t / tau_minus) * pre * post;

        nimcp_mutex_lock(&ctx->stats_mutex);
        ctx->stats.stdp_updates++;
        nimcp_mutex_unlock(&ctx->stats_mutex);
    }

    /* Apply three-factor modulation if enabled */
    if (region->enable_three_factor && ctx->neuromod_system) {
        neuromodulator_pool_t pool = neuromodulator_pool_create();
        neuromodulator_get_levels(ctx->neuromod_system, &pool);

        /* DA modulation: high DA amplifies, low DA suppresses */
        float da_factor = 0.5F + neuromodulator_pool_get_dopamine(&pool) * region->da_sensitivity;

        /* ACh modulation: attention focus */
        float ach_factor = 0.8F + neuromodulator_pool_get_acetylcholine(&pool) * region->ach_sensitivity * 0.4F;

        weight_change *= da_factor * ach_factor;
        neuromodulator_pool_destroy(&pool);
    }

    return weight_change * region->base_learning_rate;
}

static float tpb_apply_bcm_rule(tpb_context_t* ctx, uint32_t region_id,
                                 float pre, float post, float threshold)
{
    tpb_region_config_t* region = &ctx->regions[region_id];

    /* BCM rule: delta_w = eta * post * (post - theta) * pre */
    float weight_change = post * (post - threshold) * pre;

    /* Apply neuromodulation */
    if (region->enable_three_factor && ctx->neuromod_system) {
        neuromodulator_pool_t pool = neuromodulator_pool_create();
        neuromodulator_get_levels(ctx->neuromod_system, &pool);

        float da_factor = 0.5F + neuromodulator_pool_get_dopamine(&pool) * region->da_sensitivity;
        weight_change *= da_factor;
        neuromodulator_pool_destroy(&pool);
    }

    nimcp_mutex_lock(&ctx->stats_mutex);
    ctx->stats.bcm_updates++;
    nimcp_mutex_unlock(&ctx->stats_mutex);

    return weight_change * region->base_learning_rate;
}

//=============================================================================
// Neuromodulator-Learning Rate Modulator
//=============================================================================

nimcp_result_t tpb_get_modulated_lr(tpb_context_t* ctx, uint32_t region_id,
                                     float base_lr, float* modulated_lr_out)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(modulated_lr_out != NULL, NIMCP_ERROR_INVALID_PARAM, "modulated_lr_out is NULL");

    if (!ctx->neuromod_system) {
        *modulated_lr_out = base_lr;
        return NIMCP_SUCCESS;
    }

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(ctx->neuromod_system, &pool);

    float lr_multiplier = 1.0F;
    tpb_lr_modulation_config_t* mod_cfg = &ctx->config.lr_modulation;

    /* Get current neuromodulator levels */
    float dopamine = neuromodulator_pool_get_dopamine(&pool);
    float acetylcholine = neuromodulator_pool_get_acetylcholine(&pool);
    float serotonin = neuromodulator_pool_get_serotonin(&pool);
    float norepinephrine = neuromodulator_pool_get_norepinephrine(&pool);

    /* Get region-specific sensitivities */
    float da_sens = 1.0F, ach_sens = 1.0F, ht5_sens = 1.0F, ne_sens = 1.0F;
    float region_strength = 1.0F;
    if (region_id < ctx->num_regions) {
        da_sens = ctx->regions[region_id].da_sensitivity;
        ach_sens = ctx->regions[region_id].ach_sensitivity;
        ht5_sens = ctx->regions[region_id].ht5_sensitivity;
        ne_sens = ctx->regions[region_id].ne_sensitivity;
        region_strength = ctx->regions[region_id].lr_modulation_strength;
    }

    switch (mod_cfg->mode) {
        case TPB_NEUROMOD_DA_PRIMARY:
            lr_multiplier = 0.5F + dopamine * da_sens;
            break;

        case TPB_NEUROMOD_ACH_PRIMARY:
            lr_multiplier = 0.5F + acetylcholine * ach_sens;
            break;

        case TPB_NEUROMOD_5HT_PRIMARY:
            /* 5-HT modulates patience - inverse relationship with LR */
            lr_multiplier = 1.5F - serotonin * ht5_sens;
            break;

        case TPB_NEUROMOD_NE_PRIMARY:
            lr_multiplier = 0.5F + norepinephrine * ne_sens;
            break;

        case TPB_NEUROMOD_BALANCED:
        case TPB_NEUROMOD_CUSTOM:
            /* Use region sensitivities with normalized weights */
            lr_multiplier = mod_cfg->da_weight * dopamine * da_sens +
                           mod_cfg->ach_weight * acetylcholine * ach_sens +
                           mod_cfg->ht5_weight * (1.0F - serotonin) * ht5_sens +
                           mod_cfg->ne_weight * norepinephrine * ne_sens;
            /* Center around 1.0 by adding baseline */
            lr_multiplier = 0.5F + lr_multiplier;
            break;
    }

    /* Apply region-specific modulation strength to scale deviation from 1.0 */
    lr_multiplier = 1.0F + (lr_multiplier - 1.0F) * region_strength;

    /* Apply sigmoid scaling if enabled */
    if (mod_cfg->use_sigmoid_scaling) {
        float x = (lr_multiplier - 1.0F) * mod_cfg->sigmoid_steepness;
        lr_multiplier = 1.0F + (mod_cfg->max_lr_multiplier - mod_cfg->min_lr_multiplier) *
                        (1.0F / (1.0F + expf(-x)) - 0.5F);
    }

    /* Clamp to bounds */
    if (lr_multiplier < mod_cfg->min_lr_multiplier) {
        lr_multiplier = mod_cfg->min_lr_multiplier;
    }
    if (lr_multiplier > mod_cfg->max_lr_multiplier) {
        lr_multiplier = mod_cfg->max_lr_multiplier;
    }

    *modulated_lr_out = base_lr * lr_multiplier;

    /* Track statistics */
    nimcp_mutex_lock(&ctx->stats_mutex);
    float n = (float)(ctx->stats.total_plasticity_updates + 1);
    ctx->stats.avg_lr_multiplier = ctx->stats.avg_lr_multiplier * ((n - 1.0F) / n) + lr_multiplier / n;
    nimcp_mutex_unlock(&ctx->stats_mutex);

    neuromodulator_pool_destroy(&pool);
    return NIMCP_SUCCESS;
}

nimcp_result_t tpb_get_neuromod_levels(tpb_context_t* ctx, float* da_out,
                                        float* ach_out, float* ht5_out, float* ne_out)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    if (!ctx->neuromod_system) {
        if (da_out) *da_out = 0.5F;
        if (ach_out) *ach_out = 0.5F;
        if (ht5_out) *ht5_out = 0.5F;
        if (ne_out) *ne_out = 0.5F;
        return NIMCP_SUCCESS;
    }

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(ctx->neuromod_system, &pool);

    if (da_out) *da_out = neuromodulator_pool_get_dopamine(&pool);
    if (ach_out) *ach_out = neuromodulator_pool_get_acetylcholine(&pool);
    if (ht5_out) *ht5_out = neuromodulator_pool_get_serotonin(&pool);
    if (ne_out) *ne_out = neuromodulator_pool_get_norepinephrine(&pool);

    neuromodulator_pool_destroy(&pool);
    return NIMCP_SUCCESS;
}

nimcp_result_t tpb_set_neuromod_levels(tpb_context_t* ctx, float da, float ach,
                                        float ht5, float ne)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(ctx->neuromod_system != NULL, NIMCP_ERROR_INVALID_PARAM, "neuromod_system is NULL");

    if (da >= 0.0F) {
        neuromodulator_set_level(ctx->neuromod_system, NEUROMOD_DOPAMINE,
                                        da > 1.0F ? 1.0F : da);
    }
    if (ach >= 0.0F) {
        neuromodulator_set_level(ctx->neuromod_system, NEUROMOD_ACETYLCHOLINE,
                                        ach > 1.0F ? 1.0F : ach);
    }
    if (ht5 >= 0.0F) {
        neuromodulator_set_level(ctx->neuromod_system, NEUROMOD_SEROTONIN,
                                        ht5 > 1.0F ? 1.0F : ht5);
    }
    if (ne >= 0.0F) {
        neuromodulator_set_level(ctx->neuromod_system, NEUROMOD_NOREPINEPHRINE,
                                        ne > 1.0F ? 1.0F : ne);
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// STDP and BCM Integration
//=============================================================================

nimcp_result_t tpb_create_stdp_synapse(tpb_context_t* ctx, uint32_t region_id,
                                        stdp_synapse_t* synapse_out)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(synapse_out != NULL, NIMCP_ERROR_INVALID_PARAM, "synapse_out is NULL");

    stdp_synapse_init(synapse_out);

    if (region_id < ctx->num_regions) {
        tpb_region_config_t* region = &ctx->regions[region_id];

        synapse_out->learning_rate = region->base_learning_rate;
        synapse_out->tau_plus = region->plasticity_window_ms;
        synapse_out->tau_minus = region->plasticity_window_ms;
        synapse_out->enable_da_modulation = region->enable_three_factor;
        synapse_out->da_modulation_gain = region->da_sensitivity * 100.0F;
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t tpb_create_bcm_synapse(tpb_context_t* ctx, uint32_t region_id,
                                       bcm_synapse_t* synapse_out)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(synapse_out != NULL, NIMCP_ERROR_INVALID_PARAM, "synapse_out is NULL");

    memset(synapse_out, 0, sizeof(bcm_synapse_t));
    synapse_out->weight = 0.5F;
    synapse_out->threshold = 0.5F;

    if (region_id < ctx->num_regions) {
        tpb_region_config_t* region = &ctx->regions[region_id];
        /* BCM parameters based on region config */
        synapse_out->weight = region->base_learning_rate * 10.0F;  /* Scale appropriately */
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t tpb_update_stdp(tpb_context_t* ctx, stdp_synapse_t* synapse,
                                bool pre_spike, bool post_spike,
                                float current_time_ms, float* weight_delta_out)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(synapse != NULL, NIMCP_ERROR_INVALID_PARAM, "synapse is NULL");

    float delta = 0.0F;

    if (pre_spike) {
        delta += stdp_pre_spike_modulated(synapse, current_time_ms, ctx->neuromod_system);
    }
    if (post_spike) {
        delta += stdp_post_spike_modulated(synapse, current_time_ms, ctx->neuromod_system);
    }

    if (weight_delta_out) {
        *weight_delta_out = delta;
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t tpb_update_bcm(tpb_context_t* ctx, bcm_synapse_t* synapse,
                               float pre_activity, float post_activity,
                               float dt, float* weight_delta_out)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(synapse != NULL, NIMCP_ERROR_INVALID_PARAM, "synapse is NULL");

    /* BCM update: delta_w = eta * post * (post - theta) * pre */
    float weight_change = post_activity * (post_activity - synapse->threshold) * pre_activity;

    /* Update sliding threshold: theta_dot = (post^2 - theta) / tau */
    float tau_theta = 100.0F;  /* ms */
    float theta_change = (post_activity * post_activity - synapse->threshold) / tau_theta * dt * 1000.0F;
    synapse->threshold += theta_change;

    /* Clamp threshold */
    if (synapse->threshold < 0.01F) synapse->threshold = 0.01F;
    if (synapse->threshold > 0.99F) synapse->threshold = 0.99F;

    /* Apply neuromodulation */
    if (ctx->neuromod_system) {
        neuromodulator_pool_t pool = neuromodulator_pool_create();
        neuromodulator_get_levels(ctx->neuromod_system, &pool);
        float da_factor = 0.5F + neuromodulator_pool_get_dopamine(&pool);
        weight_change *= da_factor;
        neuromodulator_pool_destroy(&pool);
    }

    /* Apply to synapse */
    synapse->weight += weight_change * 0.01F;  /* Scale by learning rate */
    if (synapse->weight < 0.0F) synapse->weight = 0.0F;
    if (synapse->weight > 1.0F) synapse->weight = 1.0F;

    if (weight_delta_out) {
        *weight_delta_out = weight_change * 0.01F;
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Statistics and Monitoring
//=============================================================================

nimcp_result_t tpb_get_stats(tpb_context_t* ctx, tpb_stats_t* stats_out)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(stats_out != NULL, NIMCP_ERROR_INVALID_PARAM, "stats_out is NULL");

    nimcp_mutex_lock(&ctx->stats_mutex);
    *stats_out = ctx->stats;
    nimcp_mutex_unlock(&ctx->stats_mutex);

    return NIMCP_SUCCESS;
}

nimcp_result_t tpb_reset_stats(tpb_context_t* ctx)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    nimcp_mutex_lock(&ctx->stats_mutex);
    memset(&ctx->stats, 0, sizeof(tpb_stats_t));
    nimcp_mutex_unlock(&ctx->stats_mutex);

    return NIMCP_SUCCESS;
}

void tpb_print_status(tpb_context_t* ctx)
{
    if (!ctx) {
        return;
    }

    tpb_stats_t stats;
    tpb_get_stats(ctx, &stats);

    float da, ach, ht5, ne;
    tpb_get_neuromod_levels(ctx, &da, &ach, &ht5, &ne);

    printf("=== Training-Plasticity Bridge Status ===\n");
    printf("RPE Computations: %lu\n", (unsigned long)stats.rpe_computations);
    printf("Avg RPE: %.4f\n", stats.avg_rpe);
    printf("DA Bursts: %.0f, DA Dips: %.0f\n", stats.da_bursts, stats.da_dips);
    printf("Total Plasticity Updates: %lu\n", (unsigned long)stats.total_plasticity_updates);
    printf("  STDP: %lu, BCM: %lu, Homeostatic: %lu\n",
           (unsigned long)stats.stdp_updates, (unsigned long)stats.bcm_updates,
           (unsigned long)stats.homeostatic_updates);
    printf("Avg LR Multiplier: %.3f\n", stats.avg_lr_multiplier);
    printf("Neuromodulator Levels: DA=%.2f ACh=%.2f 5-HT=%.2f NE=%.2f\n", da, ach, ht5, ne);
    printf("Regions Configured: %u\n", ctx->num_regions);
    printf("Total Processing Time: %.2f ms\n", (double)stats.total_time_ns / 1e6);
    printf("==========================================\n");
}

//=============================================================================
// CoW Integration for Weight Snapshots
//=============================================================================

/**
 * @brief Internal snapshot wrapper structure
 *
 * This structure wraps a CoW manager and handle created specifically
 * for snapshot operations, allowing us to maintain the cow_handle_t
 * API while supporting dynamic weight array snapshots.
 */
typedef struct tpb_snapshot_wrapper {
    cow_manager_t manager;      /**< CoW manager for this snapshot */
    cow_handle_t handle;        /**< CoW handle referencing template */
    size_t size;                /**< Size of data in bytes */
    uint32_t magic;             /**< Magic number for validation */
} tpb_snapshot_wrapper_t;

#define TPB_SNAPSHOT_MAGIC 0x54504253  /* "TPBS" */

nimcp_result_t tpb_snapshot_weights(tpb_context_t* ctx, const float* weights,
                                     uint32_t num_weights, cow_handle_t* snapshot_out)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(weights != NULL, NIMCP_ERROR_INVALID_PARAM, "weights is NULL");
    NIMCP_CHECK_THROW(snapshot_out != NULL, NIMCP_ERROR_INVALID_PARAM, "snapshot_out is NULL");
    NIMCP_CHECK_THROW(num_weights > 0, NIMCP_ERROR_INVALID_PARAM, "num_weights is zero");

    if (!ctx->cow_manager) {
        LOG_WARNING("[%s] CoW not enabled, snapshot failed", TPB_LOG_MODULE);
        return NIMCP_ERROR_NOT_IMPLEMENTED;
    }

    /* Allocate wrapper structure */
    tpb_snapshot_wrapper_t* wrapper = (tpb_snapshot_wrapper_t*)nimcp_malloc(sizeof(tpb_snapshot_wrapper_t));
    if (!wrapper) {
        LOG_ERROR("[%s] Failed to allocate snapshot wrapper", TPB_LOG_MODULE);
        return NIMCP_ERROR_MEMORY;
    }

    /* Create CoW manager with weights as template */
    size_t data_size = num_weights * sizeof(float);
    cow_manager_config_t snap_config = cow_default_config(data_size, NULL);
    wrapper->manager = cow_manager_create(&snap_config, weights);
    if (!wrapper->manager) {
        LOG_ERROR("[%s] Failed to create snapshot manager", TPB_LOG_MODULE);
        nimcp_free(wrapper);
        return NIMCP_ERROR_MEMORY;
    }

    /* Acquire handle referencing the template */
    wrapper->handle = cow_acquire(wrapper->manager);
    if (!wrapper->handle) {
        LOG_ERROR("[%s] Failed to acquire snapshot handle", TPB_LOG_MODULE);
        cow_manager_destroy(wrapper->manager);
        nimcp_free(wrapper);
        return NIMCP_ERROR_MEMORY;
    }

    wrapper->size = data_size;
    wrapper->magic = TPB_SNAPSHOT_MAGIC;

    /* Return wrapper as cow_handle_t (opaque pointer) */
    *snapshot_out = (cow_handle_t)wrapper;

    nimcp_mutex_lock(&ctx->stats_mutex);
    ctx->stats.cow_saved_bytes += data_size;
    nimcp_mutex_unlock(&ctx->stats_mutex);

    return NIMCP_SUCCESS;
}

nimcp_result_t tpb_restore_weights(tpb_context_t* ctx, cow_handle_t snapshot, float* weights)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(snapshot != NULL, NIMCP_ERROR_INVALID_PARAM, "snapshot is NULL");
    NIMCP_CHECK_THROW(weights != NULL, NIMCP_ERROR_INVALID_PARAM, "weights is NULL");

    if (!ctx->cow_manager) {
        return NIMCP_ERROR_NOT_IMPLEMENTED;
    }

    /* Validate wrapper */
    tpb_snapshot_wrapper_t* wrapper = (tpb_snapshot_wrapper_t*)snapshot;
    if (wrapper->magic != TPB_SNAPSHOT_MAGIC) {
        LOG_ERROR("[%s] Invalid snapshot handle", TPB_LOG_MODULE);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Get read-only pointer to snapshot data */
    const float* snapshot_data = (const float*)cow_read(wrapper->handle);
    if (!snapshot_data) {
        LOG_ERROR("[%s] Failed to read snapshot data", TPB_LOG_MODULE);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memcpy(weights, snapshot_data, wrapper->size);

    return NIMCP_SUCCESS;
}

nimcp_result_t tpb_release_snapshot(tpb_context_t* ctx, cow_handle_t snapshot)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(snapshot != NULL, NIMCP_ERROR_INVALID_PARAM, "snapshot is NULL");

    if (!ctx->cow_manager) {
        return NIMCP_ERROR_NOT_IMPLEMENTED;
    }

    /* Validate wrapper */
    tpb_snapshot_wrapper_t* wrapper = (tpb_snapshot_wrapper_t*)snapshot;
    if (wrapper->magic != TPB_SNAPSHOT_MAGIC) {
        LOG_ERROR("[%s] Invalid snapshot handle in release", TPB_LOG_MODULE);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Release handle and destroy manager */
    cow_release(wrapper->handle);
    cow_manager_destroy(wrapper->manager);

    /* Clear magic and free wrapper */
    wrapper->magic = 0;
    nimcp_free(wrapper);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Callback Integration Functions (Phase TCB-1)
//=============================================================================

// Forward declarations for callback handlers
static tcb_action_t tpb_on_loss_computed(const tcb_event_t* event);
static tcb_action_t tpb_on_weights_updated(const tcb_event_t* event);
static tcb_action_t tpb_on_epoch_complete(const tcb_event_t* event);
static tcb_action_t tpb_on_divergence(const tcb_event_t* event);

nimcp_result_t tpb_connect_callbacks(tpb_context_t* ctx, tcb_context_t* callback_ctx)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    nimcp_mutex_lock(&ctx->callback_mutex);
    ctx->callback_ctx = callback_ctx;
    nimcp_mutex_unlock(&ctx->callback_mutex);

    LOG_DEBUG("[%s] Connected callback context: %p", TPB_LOG_MODULE, (void*)callback_ctx);
    return NIMCP_SUCCESS;
}

tcb_context_t* tpb_get_callback_context(const tpb_context_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tpb_get_callback_context: ctx is NULL");
        return NULL;
    }
    return ctx->callback_ctx;
}

nimcp_result_t tpb_register_plasticity_callbacks(tpb_context_t* ctx)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    if (!ctx->callback_ctx) {
        LOG_WARNING("[%s] No callback context connected, cannot register callbacks", TPB_LOG_MODULE);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "callback_ctx is NULL");
    }

    /* Register callback handlers with TCB */
    uint32_t id;

    id = tcb_register_simple(ctx->callback_ctx, TCB_EVENT_LOSS_COMPUTED,
                             tpb_on_loss_computed, ctx, "plasticity_loss");
    if (id == 0) {
        LOG_ERROR("[%s] Failed to register loss callback", TPB_LOG_MODULE);
        return NIMCP_ERROR_MEMORY;
    }

    id = tcb_register_simple(ctx->callback_ctx, TCB_EVENT_WEIGHTS_UPDATED,
                             tpb_on_weights_updated, ctx, "plasticity_weights");
    if (id == 0) {
        LOG_ERROR("[%s] Failed to register weights callback", TPB_LOG_MODULE);
        return NIMCP_ERROR_MEMORY;
    }

    id = tcb_register_simple(ctx->callback_ctx, TCB_EVENT_EPOCH_COMPLETE,
                             tpb_on_epoch_complete, ctx, "plasticity_epoch");
    if (id == 0) {
        LOG_ERROR("[%s] Failed to register epoch callback", TPB_LOG_MODULE);
        return NIMCP_ERROR_MEMORY;
    }

    id = tcb_register_simple(ctx->callback_ctx, TCB_EVENT_DIVERGENCE,
                             tpb_on_divergence, ctx, "plasticity_divergence");
    if (id == 0) {
        LOG_ERROR("[%s] Failed to register divergence callback", TPB_LOG_MODULE);
        return NIMCP_ERROR_MEMORY;
    }

    LOG_INFO("[%s] Plasticity callbacks registered", TPB_LOG_MODULE);

    return NIMCP_SUCCESS;
}

/**
 * @brief Loss computed callback handler
 */
static tcb_action_t tpb_on_loss_computed(const tcb_event_t* event)
{
    if (!event || !event->user_data) {
        return TCB_ACTION_CONTINUE;
    }

    tpb_context_t* ctx = (tpb_context_t*)event->user_data;

    /* Update stats */
    nimcp_mutex_lock(&ctx->callback_mutex);
    ctx->callback_stats[0]++;  /* loss_fired */
    nimcp_mutex_unlock(&ctx->callback_mutex);

    /* Report loss to compute RPE and update neuromodulators */
    float rpe = 0.0F;
    tpb_report_loss(ctx, event->metrics.loss, &rpe);

    /* Positive RPE = good learning, continue */
    if (rpe > 0.0F) {
        return TCB_ACTION_CONTINUE;
    }

    /* Negative RPE = learning getting worse, but still continue */
    return TCB_ACTION_CONTINUE;
}

/**
 * @brief Weights updated callback handler
 */
static tcb_action_t tpb_on_weights_updated(const tcb_event_t* event)
{
    if (!event || !event->user_data) {
        return TCB_ACTION_CONTINUE;
    }

    tpb_context_t* ctx = (tpb_context_t*)event->user_data;

    /* Update stats */
    nimcp_mutex_lock(&ctx->callback_mutex);
    ctx->callback_stats[1]++;  /* weight_fired */
    nimcp_mutex_unlock(&ctx->callback_mutex);

    /* Modulate neuromodulators based on gradient norm */
    if (ctx->neuromod_system && event->metrics.gradient_norm > 0.0F) {
        neuromodulator_pool_t pool = neuromodulator_pool_create();
        neuromodulator_get_levels(ctx->neuromod_system, &pool);

        /* Large gradient -> boost NE (arousal) */
        if (event->metrics.gradient_norm > 10.0F) {
            float new_ne = neuromodulator_pool_get_norepinephrine(&pool) + 0.1F;
            if (new_ne > 1.0F) new_ne = 1.0F;
            neuromodulator_set_level(ctx->neuromod_system, NEUROMOD_NOREPINEPHRINE, new_ne);
        }

        /* Small gradient -> boost ACh (exploration) */
        if (event->metrics.gradient_norm < 0.01F) {
            float new_ach = neuromodulator_pool_get_acetylcholine(&pool) + 0.1F;
            if (new_ach > 1.0F) new_ach = 1.0F;
            neuromodulator_set_level(ctx->neuromod_system, NEUROMOD_ACETYLCHOLINE, new_ach);
        }

        neuromodulator_pool_destroy(&pool);
    }

    return TCB_ACTION_CONTINUE;
}

/**
 * @brief Epoch complete callback handler
 *
 * Applies homeostatic scaling - drifts neuromodulators toward baseline (0.5)
 */
static tcb_action_t tpb_on_epoch_complete(const tcb_event_t* event)
{
    if (!event || !event->user_data) {
        return TCB_ACTION_CONTINUE;
    }

    tpb_context_t* ctx = (tpb_context_t*)event->user_data;

    /* Update stats */
    nimcp_mutex_lock(&ctx->callback_mutex);
    ctx->callback_stats[2]++;  /* epoch_fired */
    nimcp_mutex_unlock(&ctx->callback_mutex);

    LOG_DEBUG("[%s] Epoch %lu complete, loss=%.4f",
              TPB_LOG_MODULE, (unsigned long)event->metrics.epoch, event->metrics.loss);

    /* Homeostatic scaling: drift neuromodulators toward baseline (0.5) */
    if (ctx->neuromod_system) {
        float da, ach, ht5, ne;
        tpb_get_neuromod_levels(ctx, &da, &ach, &ht5, &ne);

        const float baseline = 0.5F;
        const float drift_rate = 0.2F;  /* 20% drift per epoch */

        da = da + (baseline - da) * drift_rate;
        ach = ach + (baseline - ach) * drift_rate;
        ht5 = ht5 + (baseline - ht5) * drift_rate;
        ne = ne + (baseline - ne) * drift_rate;

        tpb_set_neuromod_levels(ctx, da, ach, ht5, ne);
        LOG_DEBUG("[%s] Homeostatic drift: DA=%.2f, ACh=%.2f, 5-HT=%.2f, NE=%.2f",
                  TPB_LOG_MODULE, da, ach, ht5, ne);
    }

    return TCB_ACTION_CONTINUE;
}

/**
 * @brief Divergence callback handler
 *
 * Applies calming response - elevate 5-HT (calming) and reduce NE (arousal)
 */
static tcb_action_t tpb_on_divergence(const tcb_event_t* event)
{
    if (!event || !event->user_data) {
        return TCB_ACTION_CONTINUE;
    }

    tpb_context_t* ctx = (tpb_context_t*)event->user_data;

    /* Update stats */
    nimcp_mutex_lock(&ctx->callback_mutex);
    ctx->callback_stats[3]++;  /* divergence_fired */
    nimcp_mutex_unlock(&ctx->callback_mutex);

    /* Apply calming response: elevate 5-HT, reduce NE */
    if (ctx->neuromod_system) {
        float da, ach, ht5, ne;
        tpb_get_neuromod_levels(ctx, &da, &ach, &ht5, &ne);

        /* Calming response: 5-HT goes up, NE goes down */
        ht5 = (ht5 + 1.0F) * 0.5F;  /* Move toward 1.0 */
        if (ht5 < 0.6F) ht5 = 0.6F; /* Ensure above 0.5 */
        if (ht5 > 1.0F) ht5 = 1.0F;

        ne = ne * 0.4F;  /* Reduce arousal significantly */
        if (ne > 0.4F) ne = 0.4F;  /* Ensure below 0.5 */

        tpb_set_neuromod_levels(ctx, -1.0F, -1.0F, ht5, ne);
        LOG_DEBUG("[%s] Calming response: 5-HT=%.2f, NE=%.2f",
                  TPB_LOG_MODULE, ht5, ne);
    }

    /* Check for NaN/Inf loss */
    if (isnan(event->metrics.loss) || isinf(event->metrics.loss)) {
        LOG_ERROR("[%s] NaN/Inf loss detected, stopping training", TPB_LOG_MODULE);
        return TCB_ACTION_STOP_TRAINING;
    }

    /* Very high loss → rollback or reduce LR */
    if (event->metrics.loss > 100.0F) {
        LOG_WARNING("[%s] High loss detected (%.2f), requesting LR reduction",
                   TPB_LOG_MODULE, event->metrics.loss);
        return TCB_ACTION_REDUCE_LR;
    }

    return TCB_ACTION_ROLLBACK;
}

nimcp_result_t tpb_handle_callback_action(tpb_context_t* ctx, tcb_action_t action)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    float da, ach, ht5, ne;
    tpb_get_neuromod_levels(ctx, &da, &ach, &ht5, &ne);

    switch (action) {
        case TCB_ACTION_CONTINUE:
            /* No action needed */
            break;

        case TCB_ACTION_REDUCE_LR:
            /* Reduce DA, increase 5-HT */
            da = da * 0.8F;
            ht5 = ht5 + 0.1F;
            if (ht5 > 1.0F) ht5 = 1.0F;
            tpb_set_neuromod_levels(ctx, da, -1.0F, ht5, -1.0F);
            LOG_DEBUG("[%s] Reduced LR via neuromodulation (DA=%.2f, 5-HT=%.2f)",
                     TPB_LOG_MODULE, da, ht5);
            break;

        case TCB_ACTION_INCREASE_LR:
            /* Increase DA */
            da = da + 0.1F;
            if (da > 1.0F) da = 1.0F;
            tpb_set_neuromod_levels(ctx, da, -1.0F, -1.0F, -1.0F);
            LOG_DEBUG("[%s] Increased LR via neuromodulation (DA=%.2f)", TPB_LOG_MODULE, da);
            break;

        case TCB_ACTION_SKIP_STEP:
            /* Dampen all neuromodulators */
            da = da * 0.9F;
            ach = ach * 0.9F;
            tpb_set_neuromod_levels(ctx, da, ach, -1.0F, -1.0F);
            LOG_DEBUG("[%s] Dampened neuromodulators for skip step", TPB_LOG_MODULE);
            break;

        case TCB_ACTION_ROLLBACK:
        case TCB_ACTION_STOP_TRAINING:
            /* Reset neuromodulators to baseline */
            tpb_set_neuromod_levels(ctx, 0.5F, 0.5F, 0.5F, 0.5F);
            LOG_INFO("[%s] Reset neuromodulators due to action %d", TPB_LOG_MODULE, action);
            break;

        default:
            LOG_WARNING("[%s] Unknown callback action: %d", TPB_LOG_MODULE, action);
            break;
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t tpb_set_callback_modulation(tpb_context_t* ctx, tcb_event_type_t event, float modulation)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(event < TCB_EVENT_COUNT, NIMCP_ERROR_INVALID_PARAM, "event type out of range");

    /* Clamp modulation to valid range */
    if (modulation < 0.0F) modulation = 0.0F;
    if (modulation > 3.0F) modulation = 3.0F;

    LOG_DEBUG("[%s] Set callback modulation for event %d: %.2f", TPB_LOG_MODULE, event, modulation);

    /* Store per-event modulation factor */
    ctx->callback_modulation[event] = modulation;
    return NIMCP_SUCCESS;
}

nimcp_result_t tpb_get_callback_stats(const tpb_context_t* ctx,
                                      uint64_t* loss_fired,
                                      uint64_t* weight_fired,
                                      uint64_t* epoch_fired,
                                      uint64_t* divergence_fired)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    /* Cast away const for mutex lock - this is safe as mutex protects internal state */
    tpb_context_t* mutable_ctx = (tpb_context_t*)ctx;
    nimcp_mutex_lock(&mutable_ctx->callback_mutex);
    if (loss_fired) *loss_fired = ctx->callback_stats[0];
    if (weight_fired) *weight_fired = ctx->callback_stats[1];
    if (epoch_fired) *epoch_fired = ctx->callback_stats[2];
    if (divergence_fired) *divergence_fired = ctx->callback_stats[3];
    nimcp_mutex_unlock(&mutable_ctx->callback_mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Perception-Cortical Training Integration (Phase XBI)
//=============================================================================

nimcp_result_t tpb_connect_perception_training(
    tpb_context_t* ctx,
    perception_training_bridge_t* perception_bridge
) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    ctx->perception_training = perception_bridge;

    if (perception_bridge) {
        LOG_DEBUG("[%s] Connected perception training bridge", TPB_LOG_MODULE);
    } else {
        LOG_DEBUG("[%s] Disconnected perception training bridge", TPB_LOG_MODULE);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t tpb_connect_cortical_training(
    tpb_context_t* ctx,
    cortical_training_bridge_t* cortical_bridge
) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    ctx->cortical_training = cortical_bridge;

    if (cortical_bridge) {
        LOG_DEBUG("[%s] Connected cortical training bridge", TPB_LOG_MODULE);
    } else {
        LOG_DEBUG("[%s] Disconnected cortical training bridge", TPB_LOG_MODULE);
    }

    return NIMCP_SUCCESS;
}

float tpb_get_perception_plasticity_factor(const tpb_context_t* ctx)
{
    if (!ctx || !ctx->perception_training) {
        return 1.0f;  /* Default: no modulation */
    }

    perception_training_effects_t effects;
    if (perception_training_get_effects(ctx->perception_training, &effects) != 0 ||
        !effects.valid) {
        return 1.0f;
    }

    /* Modulation formula:
     * factor = lr_factor × (0.5 + 0.5 × visual_confidence)
     * Range: [0.25, 1.5] approximately
     */
    float factor = effects.lr_factor * (0.5f + 0.5f * effects.visual_confidence);

    /* Clamp to valid range */
    if (factor < 0.25f) factor = 0.25f;
    if (factor > 1.5f) factor = 1.5f;

    return factor;
}

float tpb_get_cortical_plasticity_factor(const tpb_context_t* ctx)
{
    if (!ctx || !ctx->cortical_training) {
        return 1.0f;  /* Default: no modulation */
    }

    cortical_training_effects_t effects;
    if (cortical_training_get_effects(ctx->cortical_training, &effects) != 0 ||
        !effects.valid) {
        return 1.0f;
    }

    /* Base factor from burst rate: higher burst = stable predictions = enhance LTP */
    float base = 0.5f + 0.5f * effects.burst_rate;

    float factor;
    if (effects.predictions_stable) {
        /* Consolidation boost: predictions converged, enhance learning */
        factor = base * 1.2f;
    } else {
        /* Error-driven: high prediction error → focus plasticity */
        float error_scale = 1.0f + 0.3f * fminf(effects.prediction_error_mag, 1.0f);
        factor = base * error_scale;
    }

    /* Clamp to valid range */
    if (factor < 0.25f) factor = 0.25f;
    if (factor > 1.5f) factor = 1.5f;

    return factor;
}

float tpb_get_combined_plasticity_factor(const tpb_context_t* ctx)
{
    if (!ctx) {
        return 1.0f;
    }

    float perception_factor = tpb_get_perception_plasticity_factor(ctx);
    float cortical_factor = tpb_get_cortical_plasticity_factor(ctx);

    /* Weighted geometric mean for stability */
    float combined = sqrtf(perception_factor * cortical_factor);

    /* Clamp to valid range */
    if (combined < 0.25f) combined = 0.25f;
    if (combined > 2.0f) combined = 2.0f;

    return combined;
}

bool tpb_is_perception_training_connected(const tpb_context_t* ctx)
{
    return ctx && ctx->perception_training != NULL;
}

bool tpb_is_cortical_training_connected(const tpb_context_t* ctx)
{
    return ctx && ctx->cortical_training != NULL;
}

//=============================================================================
// Phase 5: Backprop Gating
//=============================================================================

void nimcp_tpb_set_backprop_active(tpb_context_t* ctx, bool active) {
    if (!ctx) return;
    __atomic_store_n(&ctx->backprop_active, active ? 1 : 0, __ATOMIC_RELEASE);
}

bool nimcp_tpb_is_backprop_active(tpb_context_t* ctx) {
    if (!ctx) return false;
    return __atomic_load_n(&ctx->backprop_active, __ATOMIC_ACQUIRE) != 0;
}
