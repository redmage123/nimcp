/**
 * @file nimcp_brain_training_integration.c
 * @brief Brain-Training Integration Module Implementation
 *
 * Phase TM-3: Integrates training modules (Loss Functions, Optimizers) with
 * the brain system and security framework.
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 */

#include "middleware/training/nimcp_brain_training_integration.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "plasticity/nimcp_second_messengers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_tier.h"
#include "portia/nimcp_portia.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_dimension_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(brain_training_integration)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Loss context slot
 */
typedef struct {
    nimcp_loss_context_t* ctx;
    uint32_t id;
    bool active;
} loss_slot_t;

/**
 * @brief Optimizer context slot
 */
typedef struct {
    nimcp_optimizer_context_t* ctx;
    uint32_t id;
    bool active;
} optimizer_slot_t;

/**
 * @brief LR Scheduler context slot
 */
typedef struct {
    nimcp_lr_scheduler_ctx_t* ctx;
    uint32_t id;
    bool active;
} scheduler_slot_t;

/**
 * @brief Gradient manager context slot
 */
typedef struct {
    nimcp_gradient_manager_ctx_t* ctx;
    uint32_t id;
    bool active;
} gradmgr_slot_t;

/**
 * @brief Brain-Training integration context
 */
struct nimcp_brain_training_ctx {
    /* Configuration */
    nimcp_brain_training_config_t config;

    /* Security integration */
    nimcp_sec_integration_t* security_ctx;
    uint32_t loss_module_id;
    uint32_t optimizer_module_id;
    uint32_t scheduler_module_id;
    uint32_t gradmgr_module_id;
    bool security_registered;

    /* Memory manager */
    unified_mem_manager_t memory_mgr;

    /* Loss function contexts */
    loss_slot_t loss_slots[NIMCP_TRAINING_MAX_LOSS_CONTEXTS];
    uint32_t loss_count;
    uint32_t next_loss_id;

    /* Optimizer contexts */
    optimizer_slot_t optimizer_slots[NIMCP_TRAINING_MAX_OPTIMIZER_CONTEXTS];
    uint32_t optimizer_count;
    uint32_t next_optimizer_id;

    /* LR Scheduler contexts */
    scheduler_slot_t scheduler_slots[NIMCP_TRAINING_MAX_SCHEDULER_CONTEXTS];
    uint32_t scheduler_count;
    uint32_t next_scheduler_id;

    /* Gradient manager contexts */
    gradmgr_slot_t gradmgr_slots[NIMCP_TRAINING_MAX_GRADMGR_CONTEXTS];
    uint32_t gradmgr_count;
    uint32_t next_gradmgr_id;

    /* Training state */
    nimcp_training_mode_t mode;
    uint64_t current_epoch;
    uint64_t current_batch;

    /* Statistics */
    nimcp_training_session_stats_t stats;

    /* Event callback */
    nimcp_training_event_callback_t event_callback;
    void* callback_user_data;

    /* Convergence/divergence tracking */
    float last_loss;
    uint32_t stable_loss_count;
    bool converged;
    bool diverged;

    /* Early stopping state */
    float best_loss;
    uint32_t early_stop_wait;
    bool early_stopped;

    /* Random state for dropout */
    uint32_t dropout_seed;

    /* Plasticity Bridge integration (Phase TPB-1) */
    tpb_context_t* plasticity_bridge;
    float biological_modulation_strength;
    float prev_loss;          /* For RPE computation */
    float cumulative_da;      /* For avg dopamine tracking */
    float cumulative_lr_mod;  /* For avg LR modulation tracking */
    uint64_t bio_update_count;

    /* Training Callbacks integration (Phase TCB-1) */
    tcb_context_t* callbacks;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Second Messenger Cascade integration */
    second_messenger_system_t* second_messengers;
    bool second_messengers_enabled;

    /* Portia Resource Management integration */
    void* portia_context;              /* portia_context_t* - opaque to avoid circular dep */
    platform_tier_t current_tier;
    bool resource_aware_training;
    float tier_batch_size_multiplier;
    float tier_lr_multiplier;
    bool training_paused;
    uint32_t degradation_level;
};

/* ============================================================================
 * Event Type and Mode Names
 * ============================================================================ */

static const char* s_training_event_names[] = {
    "NONE",
    "EPOCH_START",
    "EPOCH_END",
    "BATCH_START",
    "BATCH_END",
    "LOSS_COMPUTED",
    "GRADIENTS_READY",
    "WEIGHTS_UPDATED",
    "LR_CHANGED",
    "CONVERGENCE",
    "DIVERGENCE",
    "GRAD_CLIPPED",
    "GRAD_ACCUMULATED",
    "REGULARIZED",
    "EARLY_STOP"
};

static const char* s_training_mode_names[] = {
    "TRAIN",
    "EVAL",
    "INFERENCE"
};


// Forward declarations for static functions (SRP split)
static int find_free_loss_slot(nimcp_brain_training_ctx_t* ctx);
static int find_loss_slot_by_id(nimcp_brain_training_ctx_t* ctx, uint32_t loss_id);
static int find_free_optimizer_slot(nimcp_brain_training_ctx_t* ctx);
static int find_optimizer_slot_by_id(nimcp_brain_training_ctx_t* ctx, uint32_t optimizer_id);
static int find_free_scheduler_slot(nimcp_brain_training_ctx_t* ctx);
static int find_scheduler_slot_by_id(nimcp_brain_training_ctx_t* ctx, uint32_t scheduler_id);
static int find_free_gradmgr_slot(nimcp_brain_training_ctx_t* ctx);
static int find_gradmgr_slot_by_id(nimcp_brain_training_ctx_t* ctx, uint32_t gradmgr_id);
static void calculate_tier_multipliers( platform_tier_t tier, float* batch_multiplier, float* lr_multiplier);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_brain_training_integration_part_core.c"  // 20 functions: core
#include "nimcp_brain_training_integration_part_accessors.c"  // 23 functions: accessors
#include "nimcp_brain_training_integration_part_lifecycle.c"  // 18 functions: lifecycle
#include "nimcp_brain_training_integration_part_helpers.c"  // 5 functions: helpers
#include "nimcp_brain_training_integration_part_processing.c"  // 6 functions: processing
