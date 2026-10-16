/**
 * @file nimcp_portia_classification.c
 * @brief Target Classification System Implementation
 *
 * WHAT: Fast target tracking and classification
 * WHY:  Portia spiders identify and track prey
 * HOW:  Registry-based tracking with classification engine
 */

#include "portia/nimcp_portia_classification.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_helpers.h"
#include "security/nimcp_security.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "nimcp.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

#define LOG_MODULE "portia_classification"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_classification)

//=============================================================================
// Thread-Local Error Handling
//=============================================================================

static __thread char g_classification_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

/**
 * WHAT: Set error message
 * WHY:  Thread-safe error reporting
 * HOW:  Use thread-local storage
 */
static void classification_set_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(g_classification_error, sizeof(g_classification_error), format, args);
    va_end(args);
}

const char* portia_classification_get_error(void)
{
    return g_classification_error;
}

//=============================================================================
// Internal Structure
//=============================================================================

struct portia_classifier_struct {
    uint32_t magic;                          /**< Validation magic */
    portia_classification_config_t config;   /**< Configuration */
    target_registry_t registry;              /**< Target tracking */
    nimcp_mutex_t lock;                      /**< Thread safety */
    bio_module_context_t bio_ctx;            /**< Bio-async context (pointer) */
    bool bio_async_enabled;                  /**< Bio-async active */
    uint64_t last_prune_ms;                  /**< Last prune time */
};

#define CLASSIFIER_MAGIC 0x504F5254  // 'PORT'

//=============================================================================
// Bio-Async Message Handlers
//=============================================================================

/**
 * WHAT: Process incoming bio-async messages
 * WHY:  Handle classification queries from other modules
 * HOW:  Dispatch based on message type
 */
static nimcp_error_t classification_inbox_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)response_promise;  /* Unused for now */
    portia_classifier_t classifier = (portia_classifier_t)user_data;

    // Validate inputs
    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        LOG_ERROR("Invalid message in inbox handler");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!classifier) {
        LOG_ERROR("Invalid classifier in inbox handler");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    LOG_DEBUG("Received bio-async message type 0x%04X", header->type);

    // Handle message types
    switch (header->type) {
        case BIO_MSG_INTROSPECTION_QUERY:
            LOG_DEBUG("Processing target query request");
            // Future: respond with target info
            break;

        case BIO_MSG_SECURITY_EVENT:
            LOG_DEBUG("Processing threat assessment request");
            // Future: respond with threat list
            break;

        default:
            LOG_DEBUG("Unhandled message type 0x%04X", header->type);
            break;
    }
    return NIMCP_SUCCESS;
}

//=============================================================================
// KG-Driven Wiring Callback
//=============================================================================

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 */
static int classification_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_INTROSPECTION_QUERY:
                bio_router_register_handler(ctx, message_types[i], classification_inbox_handler);
                registered++;
                break;
            case BIO_MSG_SECURITY_EVENT:
                bio_router_register_handler(ctx, message_types[i], classification_inbox_handler);
                registered++;
                break;
            default:
                LOG_DEBUG("Unknown message type 0x%04X in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_INFO("KG-driven wiring callback registered %d handlers", registered);
    return (registered > 0) ? 0 : -1;
}

/**
 * WHAT: Broadcast target classification event
 * WHY:  Notify other modules of classification updates
 * HOW:  Send bio-async message via acetylcholine (attention)
 */
static void broadcast_classification_event(
    portia_classifier_t classifier,
    uint32_t target_id,
    target_class_t classification,
    float confidence)
{
    if (!classifier->bio_async_enabled) {
        return;
    }

    // Create classification message
    struct {
        uint32_t target_id;
        uint32_t classification;
        float confidence;
    } payload = {
        .target_id = target_id,
        .classification = (uint32_t)classification,
        .confidence = confidence
    };

    // Create message with header
    struct {
        bio_message_header_t header;
        uint32_t target_id;
        uint32_t classification;
        float confidence;
    } msg = {
        .header = {
            .type = BIO_MSG_PORTIA_PLAN_CREATED,  /* Using Portia message type */
            .flags = BIO_MSG_FLAG_BROADCAST,
            .payload_size = sizeof(payload)
        },
        .target_id = payload.target_id,
        .classification = payload.classification,
        .confidence = payload.confidence
    };

    nimcp_error_t result = bio_router_broadcast(classifier->bio_ctx, &msg, sizeof(msg));
    if (result != NIMCP_SUCCESS) {
        LOG_WARN("Failed to broadcast classification event: %d", result);
    } else {
        LOG_DEBUG("Broadcast classification for target %u: class=%d, conf=%.2f",
                  target_id, classification, confidence);
    }

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "classification_broadcast",
                  "target_id=%u class=%d confidence=%.2f",
                  target_id, classification, confidence);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

portia_classifier_t portia_classification_init(
    const portia_classification_config_t* config)
{
    // Validate input
    if (!(config != NULL)) {
        LOG_ERROR("Invalid config pointer");
        classification_set_error("Invalid configuration pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid config pointer in portia_classification_init");
        return NULL;
    }

    if (config->max_targets == 0 || config->max_targets > 10000) {
        LOG_ERROR("Invalid max_targets: %u", config->max_targets);
        classification_set_error("Invalid max_targets (must be 1-10000)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid max_targets in portia_classification_init");
        return NULL;
    }

    LOG_INFO("Initializing classification system: max_targets=%u, threshold=%.2f",
             config->max_targets, config->classification_threshold);

    // Allocate classifier
    portia_classifier_t classifier = nimcp_calloc(1, sizeof(*classifier));
    if (!classifier) {
        LOG_ERROR("Failed to allocate classifier");
        classification_set_error("Memory allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate classifier in portia_classification_init");
        return NULL;
    }

    // Initialize fields
    classifier->magic = CLASSIFIER_MAGIC;
    memcpy(&classifier->config, config, sizeof(*config));

    // Allocate target registry
    classifier->registry.target_capacity = config->max_targets;
    classifier->registry.targets = nimcp_calloc(
        config->max_targets, sizeof(target_info_t));

    if (!classifier->registry.targets) {
        LOG_ERROR("Failed to allocate target registry");
        classification_set_error("Target registry allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate target registry in portia_classification_init");
        nimcp_free(classifier);
        return NULL;
    }

    classifier->registry.target_count = 0;
    classifier->registry.next_id = 1;

    // Initialize mutex
    if (nimcp_mutex_init(&classifier->lock, false) != 0) {
        LOG_ERROR("Failed to initialize mutex");
        classification_set_error("Mutex initialization failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to initialize mutex in portia_classification_init");
        nimcp_free(classifier->registry.targets);
        nimcp_free(classifier);
        return NULL;
    }

    classifier->last_prune_ms = nimcp_time_monotonic_ms();

    // Initialize bio-async if enabled
    if (config->enable_bio_async) {
        bio_module_info_t info = {
            .module_id = BIO_MODULE_PORTIA_CLASSIFICATION,
            .module_name = "portia_classification",
            .inbox_capacity = 64,
            .user_data = classifier
        };

        classifier->bio_ctx = bio_router_register_module(&info);
        if (classifier->bio_ctx) {
            /* KG-Driven Wiring: Register callback for orchestrator to invoke */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_PORTIA_CLASSIFICATION,
                (void*)classification_wiring_handler_callback,
                classifier
            );

            if (cb_result == NIMCP_SUCCESS) {
                LOG_INFO("Bio-async registered with KG-driven wiring callback (module_id=0x%04X)",
                         BIO_MODULE_PORTIA_CLASSIFICATION);
            } else {
                /* Fallback: Direct registration if orchestrator not available */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(classifier->bio_ctx,
                                               BIO_MSG_INTROSPECTION_QUERY,
                                               classification_inbox_handler)
                );
                LOG_INFO("Bio-async registered with legacy handler registration (module_id=0x%04X)",
                         BIO_MODULE_PORTIA_CLASSIFICATION);
            }
            classifier->bio_async_enabled = true;
        } else {
            LOG_WARN("Failed to register bio-async module");
            classifier->bio_async_enabled = false;
        }
    }

    LOG_INFO("Classification system initialized successfully");
    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "init",
                  "max_targets=%u", config->max_targets);

    return classifier;
}

void portia_classification_destroy(portia_classifier_t classifier)
{
    if (!(classifier != NULL)) {
        return;
    }

    if (classifier->magic != CLASSIFIER_MAGIC) {
        LOG_ERROR("Invalid classifier magic");
        return;
    }

    LOG_INFO("Destroying classification system");

    // Unregister bio-async if enabled
    if (classifier->bio_async_enabled && classifier->bio_ctx) {
        bio_router_unregister_module(classifier->bio_ctx);
    }

    // Free registry
    if (classifier->registry.targets) {
        nimcp_free(classifier->registry.targets);
    }

    // Destroy mutex
    nimcp_mutex_destroy(&classifier->lock);

    // Clear magic and free
    classifier->magic = 0;
    nimcp_free(classifier);

    LOG_INFO("Classification system destroyed");
}

//=============================================================================
// Target Management
//=============================================================================

uint32_t portia_classification_add_target(
    portia_classifier_t classifier,
    float x, float y, float z,
    float size)
{
    // Validate inputs
    if (!(classifier != NULL)) {
        LOG_ERROR("Invalid classifier pointer");
        classification_set_error("Invalid classifier");
        return 0;
    }

    if (classifier->magic != CLASSIFIER_MAGIC) {
        LOG_ERROR("Invalid classifier magic");
        classification_set_error("Invalid classifier magic");
        return 0;
    }

    if (!isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(size)) {
        LOG_ERROR("Invalid position or size values");
        classification_set_error("Invalid position or size");
        return 0;
    }

    nimcp_mutex_lock(&classifier->lock);

    // Check capacity
    if (classifier->registry.target_count >= classifier->registry.target_capacity) {
        LOG_WARN("Target registry full");
        classification_set_error("Target registry at capacity");
        nimcp_mutex_unlock(&classifier->lock);
        return 0;
    }

    // Find free slot
    uint32_t slot = 0;
    for (uint32_t i = 0; i < classifier->registry.target_capacity; i++) {
        if (!classifier->registry.targets[i].active) {
            slot = i;
            break;
        }
    }

    // Assign ID and initialize
    uint32_t target_id = classifier->registry.next_id++;
    target_info_t* target = &classifier->registry.targets[slot];

    target->id = target_id;
    target->classification = TARGET_CLASS_UNKNOWN;
    target->confidence = 0.0F;
    target->x = x;
    target->y = y;
    target->z = z;
    target->vx = 0.0F;
    target->vy = 0.0F;
    target->vz = 0.0F;
    target->size = size;
    target->first_seen_ms = nimcp_time_monotonic_ms();
    target->last_seen_ms = target->first_seen_ms;
    target->observation_count = 1;
    target->active = true;

    classifier->registry.target_count++;

    nimcp_mutex_unlock(&classifier->lock);

    LOG_DEBUG("Added target %u at (%.2f, %.2f, %.2f) size=%.2f",
              target_id, x, y, z, size);

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "target_registered",
                  "id=%u x=%.2f y=%.2f z=%.2f", target_id, x, y, z);

    return target_id;
}

int portia_classification_update(
    portia_classifier_t classifier,
    uint32_t target_id,
    float x, float y, float z)
{
    // Validate inputs
    if (!(classifier != NULL)) {
        LOG_ERROR("Invalid classifier pointer");
        classification_set_error("Invalid classifier");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (classifier->magic != CLASSIFIER_MAGIC) {
        LOG_ERROR("Invalid classifier magic");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!isfinite(x) || !isfinite(y) || !isfinite(z)) {
        LOG_ERROR("Invalid position values");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&classifier->lock);

    // Find target
    target_info_t* target = NULL;
    for (uint32_t i = 0; i < classifier->registry.target_capacity; i++) {
        if (classifier->registry.targets[i].active &&
            classifier->registry.targets[i].id == target_id) {
            target = &classifier->registry.targets[i];
            break;
        }
    }

    if (!target) {
        LOG_WARN("Target %u not found", target_id);
        classification_set_error("Target not found");
        nimcp_mutex_unlock(&classifier->lock);
        return NIMCP_ERROR_NOT_FOUND;
    }

    // Compute velocity
    uint64_t now_ms = nimcp_time_monotonic_ms();
    float dt = (now_ms - target->last_seen_ms) / 1000.0F;

    if (dt > 0.001F) {  // Avoid division by zero
        target->vx = (x - target->x) / dt;
        target->vy = (y - target->y) / dt;
        target->vz = (z - target->z) / dt;
    }

    // Update position
    target->x = x;
    target->y = y;
    target->z = z;
    target->last_seen_ms = now_ms;
    target->observation_count++;

    nimcp_mutex_unlock(&classifier->lock);

    LOG_DEBUG("Updated target %u: pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f)",
              target_id, x, y, z, target->vx, target->vy, target->vz);

    return NIMCP_SUCCESS;
}

int portia_classification_classify(
    portia_classifier_t classifier,
    uint32_t target_id,
    target_class_t* classification,
    float* confidence)
{
    // Validate inputs
    if (!(classifier != NULL)) {
        LOG_ERROR("Invalid classifier pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!(classification != NULL)) {
        LOG_ERROR("Invalid classification pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!(confidence != NULL)) {
        LOG_ERROR("Invalid confidence pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&classifier->lock);

    // Find target
    target_info_t* target = NULL;
    for (uint32_t i = 0; i < classifier->registry.target_capacity; i++) {
        if (classifier->registry.targets[i].active &&
            classifier->registry.targets[i].id == target_id) {
            target = &classifier->registry.targets[i];
            break;
        }
    }

    if (!target) {
        nimcp_mutex_unlock(&classifier->lock);
        return NIMCP_ERROR_NOT_FOUND;
    }

    // Classification heuristics based on observations
    float speed = sqrtf(target->vx * target->vx +
                       target->vy * target->vy +
                       target->vz * target->vz);

    float conf = 0.0F;
    target_class_t class = TARGET_CLASS_UNKNOWN;

    // Need multiple observations for confidence
    if (target->observation_count < 3) {
        class = TARGET_CLASS_UNKNOWN;
        conf = 0.1F;
    }
    // Fast moving = threat or prey
    else if (speed > 2.0F) {
        if (target->size > 1.0F) {
            class = TARGET_CLASS_THREAT;
            conf = 0.7F + (speed / 10.0F) * 0.2F;
        } else {
            class = TARGET_CLASS_PREY;
            conf = 0.6F + (speed / 10.0F) * 0.3F;
        }
    }
    // Slow/stationary = neutral or obstacle
    else if (speed < 0.5F) {
        if (target->size < 0.5F) {
            class = TARGET_CLASS_NEUTRAL;
            conf = 0.8F;
        } else {
            class = TARGET_CLASS_OBSTACLE;
            conf = 0.9F;
        }
    }
    // Medium speed = prey
    else {
        class = TARGET_CLASS_PREY;
        conf = 0.7F;
    }

    // Confidence increases with observations
    float obs_bonus = fminf(target->observation_count / 10.0F, 0.2F);
    conf = fminf(conf + obs_bonus, 1.0F);

    // Update target
    target->classification = class;
    target->confidence = conf;

    *classification = class;
    *confidence = conf;

    nimcp_mutex_unlock(&classifier->lock);

    LOG_DEBUG("Classified target %u: class=%d, confidence=%.2f",
              target_id, class, conf);

    // Broadcast classification event
    broadcast_classification_event(classifier, target_id, class, conf);

    return NIMCP_SUCCESS;
}

uint32_t portia_classification_get_threats(
    portia_classifier_t classifier,
    uint32_t* threats,
    uint32_t max_threats)
{
    if (!(classifier != NULL)) {
        return 0;
    }

    if (!threats) {
        return 0;
    }

    nimcp_mutex_lock(&classifier->lock);

    uint32_t count = 0;
    for (uint32_t i = 0; i < classifier->registry.target_capacity &&
                        count < max_threats; i++) {
        target_info_t* target = &classifier->registry.targets[i];
        if (target->active &&
            target->classification == TARGET_CLASS_THREAT &&
            target->confidence >= classifier->config.classification_threshold) {
            threats[count++] = target->id;
        }
    }

    nimcp_mutex_unlock(&classifier->lock);

    LOG_DEBUG("Found %u threats", count);
    return count;
}

uint32_t portia_classification_prune(portia_classifier_t classifier)
{
    if (!(classifier != NULL)) {
        return 0;
    }

    if (classifier->magic != CLASSIFIER_MAGIC) {
        return 0;
    }

    nimcp_mutex_lock(&classifier->lock);

    uint64_t now_ms = nimcp_time_monotonic_ms();
    uint32_t pruned = 0;

    for (uint32_t i = 0; i < classifier->registry.target_capacity; i++) {
        target_info_t* target = &classifier->registry.targets[i];
        if (target->active) {
            uint64_t age_ms = now_ms - target->last_seen_ms;
            if (age_ms > classifier->config.retention_time_ms) {
                LOG_DEBUG("Pruning stale target %u (age=%llu ms)",
                          target->id, (unsigned long long)age_ms);
                target->active = false;
                classifier->registry.target_count--;
                pruned++;
            }
        }
    }

    classifier->last_prune_ms = now_ms;

    nimcp_mutex_unlock(&classifier->lock);

    if (pruned > 0) {
        LOG_INFO("Pruned %u stale targets", pruned);
        bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "Targets pruned", "count=%u", pruned);
    }

    return pruned;
}

//=============================================================================
// Query Functions
//=============================================================================

int portia_classification_get_target(
    portia_classifier_t classifier,
    uint32_t target_id,
    target_info_t* info)
{
    if (!(classifier != NULL)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!(info != NULL)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&classifier->lock);

    for (uint32_t i = 0; i < classifier->registry.target_capacity; i++) {
        if (classifier->registry.targets[i].active &&
            classifier->registry.targets[i].id == target_id) {
            memcpy(info, &classifier->registry.targets[i], sizeof(*info));
            nimcp_mutex_unlock(&classifier->lock);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_mutex_unlock(&classifier->lock);
    return NIMCP_ERROR_NOT_FOUND;
}

uint32_t portia_classification_get_count(portia_classifier_t classifier)
{
    if (!(classifier != NULL)) {
        return 0;
    }

    nimcp_mutex_lock(&classifier->lock);
    uint32_t count = classifier->registry.target_count;
    nimcp_mutex_unlock(&classifier->lock);

    return count;
}
