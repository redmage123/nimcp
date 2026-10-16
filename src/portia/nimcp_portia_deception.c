/**
 * @file nimcp_portia_deception.c
 * @brief Stealth and Deception System Implementation
 *
 * WHAT: Stealth, mimicry, and countermeasure operations
 * WHY:  Portia spiders use deceptive signals
 * HOW:  Mode control, emission management, mimicry profiles
 */

#include "portia/nimcp_portia_deception.h"
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

#define LOG_MODULE "portia_deception"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_deception)

//=============================================================================
// Thread-Local Error Handling
//=============================================================================

static __thread char g_deception_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

/**
 * WHAT: Set error message
 * WHY:  Thread-safe error reporting
 * HOW:  Use thread-local storage
 */
static void deception_set_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(g_deception_error, sizeof(g_deception_error), format, args);
    va_end(args);
}

const char* portia_deception_get_error(void)
{
    return g_deception_error;
}

//=============================================================================
// Internal Structure
//=============================================================================

struct portia_deception_struct {
    uint32_t magic;                       /**< Validation magic */
    portia_deception_config_t config;     /**< Configuration */
    stealth_state_t state;                /**< Current state */
    mimicry_profile_t* profiles;          /**< Profile registry */
    uint32_t profile_count;               /**< Active profile count */
    uint32_t profile_capacity;            /**< Profile capacity */
    nimcp_mutex_t lock;                   /**< Thread safety */
    bio_module_context_t bio_ctx;         /**< Bio-async context */
    bool bio_async_enabled;               /**< Bio-async active */
};

#define DECEPTION_MAGIC 0x44454345  // 'DECE'

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Calculate stealth effectiveness
 * WHY:  Determine current detection risk
 * HOW:  Factor in mode, emissions, and jamming
 */
static float calculate_effectiveness(const portia_deception_t deception)
{
    float effectiveness = 0.0F;

    switch (deception->state.mode) {
        case STEALTH_MODE_NONE:
            effectiveness = 0.0F;
            break;

        case STEALTH_MODE_PASSIVE:
            // Effectiveness inversely proportional to emissions
            effectiveness = 1.0F - deception->state.emission_level;
            break;

        case STEALTH_MODE_ACTIVE:
            // Active mode provides better stealth
            effectiveness = 0.8F - (deception->state.emission_level * 0.5F);
            if (deception->state.jamming_active) {
                effectiveness += 0.15F;
            }
            break;

        case STEALTH_MODE_MIMICRY:
            // Mimicry effectiveness depends on profile
            if (deception->state.mimicry_profile > 0 &&
                deception->state.mimicry_profile <= deception->profile_count) {
                uint32_t idx = deception->state.mimicry_profile - 1;
                effectiveness = deception->profiles[idx].effectiveness;
            } else {
                effectiveness = 0.5F;  // Default mimicry
            }
            break;

        default:
            effectiveness = 0.0F;
            break;
    }

    // Clamp to [0, 1]
    return fmaxf(0.0F, fminf(1.0F, effectiveness));
}

//=============================================================================
// Bio-Async Message Handlers
//=============================================================================

/**
 * WHAT: Process incoming bio-async messages
 * WHY:  Handle stealth mode requests from other modules
 * HOW:  Dispatch based on message type
 */
static nimcp_error_t deception_inbox_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)response_promise;  /* Unused for now */
    portia_deception_t deception = (portia_deception_t)user_data;

    // Validate inputs
    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        LOG_ERROR("Invalid message in inbox handler");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!deception) {
        LOG_ERROR("Invalid deception handle in inbox handler");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    LOG_DEBUG("Received bio-async message type 0x%04X", header->type);

    // Handle message types - using existing Portia message types
    switch (header->type) {
        case BIO_MSG_PORTIA_POWER_STATE_CHANGE:
            LOG_DEBUG("Processing power state change");
            // Future: extract mode from payload and set
            break;

        case BIO_MSG_SECURITY_EVENT:
            LOG_DEBUG("Processing security event");
            // Future: extract level from payload and set
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
static int deception_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_PORTIA_POWER_STATE_CHANGE:
            case BIO_MSG_SECURITY_EVENT:
                bio_router_register_handler(ctx, message_types[i], deception_inbox_handler);
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
 * WHAT: Broadcast stealth state change event
 * WHY:  Notify other modules of mode changes
 * HOW:  Send bio-async message via router
 */
static void broadcast_state_change(
    portia_deception_t deception,
    stealth_mode_t old_mode,
    stealth_mode_t new_mode)
{
    if (!deception->bio_async_enabled || !deception->bio_ctx) {
        return;
    }

    // Create state change message with header
    struct {
        bio_message_header_t header;
        uint32_t old_mode;
        uint32_t new_mode;
        float effectiveness;
    } msg = {
        .header = {
            .type = BIO_MSG_PORTIA_POWER_STATE_CHANGE,
            .flags = BIO_MSG_FLAG_BROADCAST,
            .payload_size = sizeof(uint32_t) * 2 + sizeof(float)
        },
        .old_mode = (uint32_t)old_mode,
        .new_mode = (uint32_t)new_mode,
        .effectiveness = deception->state.effectiveness
    };

    nimcp_error_t result = bio_router_broadcast(deception->bio_ctx, &msg, sizeof(msg));
    if (result != NIMCP_SUCCESS) {
        LOG_WARN("Failed to broadcast state change: %d", result);
    } else {
        LOG_DEBUG("Broadcast state change: %d -> %d, eff=%.2f",
                  old_mode, new_mode, deception->state.effectiveness);
    }

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "state_change",
                  "old_mode=%d new_mode=%d effectiveness=%.2f",
                  old_mode, new_mode, deception->state.effectiveness);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

portia_deception_t portia_deception_init(
    const portia_deception_config_t* config)
{
    // Validate input
    if (!(config != NULL)) {
        LOG_ERROR("Invalid config pointer");
        deception_set_error("Invalid configuration pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_deception_init: validation failed");
        return NULL;
    }

    if (config->default_emission_level < 0.0F ||
        config->default_emission_level > 1.0F) {
        LOG_ERROR("Invalid default_emission_level: %.2f",
                  config->default_emission_level);
        deception_set_error("Invalid emission level (must be 0.0-1.0)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_deception_init: operation failed");
        return NULL;
    }

    LOG_INFO("Initializing deception system: stealth=%d, mimicry=%d, jamming=%d",
             config->enable_stealth, config->enable_mimicry, config->enable_jamming);

    // Allocate deception system
    portia_deception_t deception = nimcp_calloc(1, sizeof(*deception));
    if (!deception) {
        LOG_ERROR("Failed to allocate deception system");
        deception_set_error("Memory allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "deception is NULL");

        return NULL;
    }

    // Initialize fields
    deception->magic = DECEPTION_MAGIC;
    memcpy(&deception->config, config, sizeof(*config));

    // Initialize state
    deception->state.mode = STEALTH_MODE_NONE;
    deception->state.emission_level = config->default_emission_level;
    deception->state.mimicry_profile = 0;
    deception->state.jamming_active = false;
    deception->state.effectiveness = 0.0F;
    deception->state.mode_started_ms = nimcp_time_monotonic_ms();

    // Allocate profile registry if mimicry enabled
    if (config->enable_mimicry && config->profile_count > 0) {
        deception->profile_capacity = config->profile_count;
        deception->profiles = nimcp_calloc(
            config->profile_count, sizeof(mimicry_profile_t));

        if (!deception->profiles) {
            LOG_ERROR("Failed to allocate profile registry");
            deception_set_error("Profile registry allocation failed");
            nimcp_free(deception);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_deception_init: deception->profiles is NULL");
            return NULL;
        }

        deception->profile_count = 0;  // No profiles registered yet
    }

    // Initialize mutex
    if (nimcp_mutex_init(&deception->lock, NULL) != 0) {
        LOG_ERROR("Failed to initialize mutex");
        deception_set_error("Mutex initialization failed");
        if (deception->profiles) {
            nimcp_free(deception->profiles);
        }
        nimcp_free(deception);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_deception_init: validation failed");
        return NULL;
    }

    // Initialize bio-async if enabled
    if (config->enable_bio_async) {
        bio_module_info_t info = {
            .module_id = BIO_MODULE_PORTIA_DECEPTION,
            .module_name = "portia_deception",
            .inbox_capacity = 32,
            .user_data = deception
        };

        deception->bio_ctx = bio_router_register_module(&info);
        if (deception->bio_ctx) {
            /* KG-Driven Wiring: Register callback for orchestrator to invoke */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_PORTIA_DECEPTION,
                (void*)deception_wiring_handler_callback,
                deception
            );

            if (cb_result == NIMCP_SUCCESS) {
                LOG_INFO("Bio-async registered with KG-driven wiring callback (module_id=0x%04X)",
                         BIO_MODULE_PORTIA_DECEPTION);
            } else {
                /* Fallback: Direct registration if orchestrator not available */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(deception->bio_ctx,
                                               BIO_MSG_SECURITY_EVENT,
                                               deception_inbox_handler)
                );
                LOG_INFO("Bio-async registered with legacy handler registration (module_id=0x%04X)",
                         BIO_MODULE_PORTIA_DECEPTION);
            }
            deception->bio_async_enabled = true;
        } else {
            LOG_WARN("Failed to register bio-async module");
            deception->bio_async_enabled = false;
        }
    }

    LOG_INFO("Deception system initialized successfully");
    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "init",
                  "stealth=%d mimicry=%d jamming=%d",
                  config->enable_stealth, config->enable_mimicry,
                  config->enable_jamming);

    return deception;
}

void portia_deception_destroy(portia_deception_t deception)
{
    if (!(deception != NULL)) {
        return;
    }

    if (deception->magic != DECEPTION_MAGIC) {
        LOG_ERROR("Invalid deception magic");
        return;
    }

    LOG_INFO("Destroying deception system");

    // Unregister bio-async if enabled
    if (deception->bio_async_enabled && deception->bio_ctx) {
        bio_router_unregister_module(deception->bio_ctx);
    }

    // Free profile registry
    if (deception->profiles) {
        nimcp_free(deception->profiles);
    }

    // Destroy mutex
    nimcp_mutex_destroy(&deception->lock);

    // Clear magic and free
    deception->magic = 0;
    nimcp_free(deception);

    LOG_INFO("Deception system destroyed");
}

//=============================================================================
// Stealth Operations
//=============================================================================

int portia_deception_set_mode(
    portia_deception_t deception,
    stealth_mode_t mode)
{
    // Validate inputs
    if (!(deception != NULL)) {
        LOG_ERROR("Invalid deception pointer");
        deception_set_error("Invalid deception handle");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (deception->magic != DECEPTION_MAGIC) {
        LOG_ERROR("Invalid deception magic");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (mode < STEALTH_MODE_NONE || mode > STEALTH_MODE_MIMICRY) {
        LOG_ERROR("Invalid stealth mode: %d", mode);
        deception_set_error("Invalid stealth mode");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Check capabilities
    if (mode == STEALTH_MODE_PASSIVE && !deception->config.enable_stealth) {
        LOG_ERROR("Stealth not enabled");
        deception_set_error("Stealth capability not enabled");
        return NIMCP_ERROR_NOT_SUPPORTED;
    }

    if (mode == STEALTH_MODE_ACTIVE && !deception->config.enable_stealth) {
        LOG_ERROR("Active stealth not enabled");
        deception_set_error("Active stealth not enabled");
        return NIMCP_ERROR_NOT_SUPPORTED;
    }

    if (mode == STEALTH_MODE_MIMICRY && !deception->config.enable_mimicry) {
        LOG_ERROR("Mimicry not enabled");
        deception_set_error("Mimicry capability not enabled");
        return NIMCP_ERROR_NOT_SUPPORTED;
    }

    nimcp_mutex_lock(&deception->lock);

    stealth_mode_t old_mode = deception->state.mode;
    deception->state.mode = mode;
    deception->state.mode_started_ms = nimcp_time_monotonic_ms();

    // Recalculate effectiveness
    deception->state.effectiveness = calculate_effectiveness(deception);

    nimcp_mutex_unlock(&deception->lock);

    LOG_INFO("Stealth mode changed: %d -> %d, effectiveness=%.2f",
             old_mode, mode, deception->state.effectiveness);

    // Broadcast state change
    broadcast_state_change(deception, old_mode, mode);

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "mode_change",
                  "old=%d new=%d eff=%.2f", old_mode, mode,
                  deception->state.effectiveness);

    return NIMCP_SUCCESS;
}

int portia_deception_emit(
    portia_deception_t deception,
    float level)
{
    // Validate inputs
    if (!(deception != NULL)) {
        LOG_ERROR("Invalid deception pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (deception->magic != DECEPTION_MAGIC) {
        LOG_ERROR("Invalid deception magic");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (level < 0.0F || level > 1.0F) {
        LOG_ERROR("Invalid emission level: %.2f", level);
        deception_set_error("Emission level must be 0.0-1.0");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&deception->lock);

    float old_level = deception->state.emission_level;
    deception->state.emission_level = level;

    // Recalculate effectiveness
    deception->state.effectiveness = calculate_effectiveness(deception);

    nimcp_mutex_unlock(&deception->lock);

    LOG_DEBUG("Emission level changed: %.2f -> %.2f, eff=%.2f",
              old_level, level, deception->state.effectiveness);

    return NIMCP_SUCCESS;
}

float portia_deception_get_effectiveness(portia_deception_t deception)
{
    if (!(deception != NULL)) {
        LOG_ERROR("Invalid deception pointer");
        return -1.0F;
    }

    if (deception->magic != DECEPTION_MAGIC) {
        LOG_ERROR("Invalid deception magic");
        return -1.0F;
    }

    nimcp_mutex_lock(&deception->lock);
    float effectiveness = deception->state.effectiveness;
    nimcp_mutex_unlock(&deception->lock);

    return effectiveness;
}

//=============================================================================
// Mimicry Operations
//=============================================================================

int portia_deception_mimic(
    portia_deception_t deception,
    uint32_t profile_id)
{
    // Validate inputs
    if (!(deception != NULL)) {
        LOG_ERROR("Invalid deception pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (deception->magic != DECEPTION_MAGIC) {
        LOG_ERROR("Invalid deception magic");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!deception->config.enable_mimicry) {
        LOG_ERROR("Mimicry not enabled");
        deception_set_error("Mimicry capability not enabled");
        return NIMCP_ERROR_NOT_SUPPORTED;
    }

    if (profile_id == 0 || profile_id > deception->profile_count) {
        LOG_ERROR("Invalid profile ID: %u", profile_id);
        deception_set_error("Invalid profile ID");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&deception->lock);

    uint32_t idx = profile_id - 1;
    if (!deception->profiles[idx].active) {
        LOG_ERROR("Profile %u not active", profile_id);
        deception_set_error("Profile not active");
        nimcp_mutex_unlock(&deception->lock);
        return NIMCP_ERROR_INVALID_TYPE;
    }

    deception->state.mimicry_profile = profile_id;
    deception->state.mode = STEALTH_MODE_MIMICRY;

    // Recalculate effectiveness
    deception->state.effectiveness = calculate_effectiveness(deception);

    nimcp_mutex_unlock(&deception->lock);

    LOG_INFO("Activated mimicry profile %u (%s), eff=%.2f",
             profile_id, deception->profiles[idx].name,
             deception->state.effectiveness);

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "Mimicry activated",
                  "profile_id=%u name=%s", profile_id,
                  deception->profiles[idx].name);

    return NIMCP_SUCCESS;
}

uint32_t portia_deception_register_profile(
    portia_deception_t deception,
    const mimicry_profile_t* profile)
{
    // Validate inputs
    if (!(deception != NULL)) {
        LOG_ERROR("Invalid deception pointer");
        return 0;
    }

    if (!(profile != NULL)) {
        LOG_ERROR("Invalid profile pointer");
        return 0;
    }

    if (deception->magic != DECEPTION_MAGIC) {
        LOG_ERROR("Invalid deception magic");
        return 0;
    }

    if (!deception->config.enable_mimicry) {
        LOG_ERROR("Mimicry not enabled");
        deception_set_error("Mimicry not enabled");
        return 0;
    }

    nimcp_mutex_lock(&deception->lock);

    // Check capacity
    if (deception->profile_count >= deception->profile_capacity) {
        LOG_ERROR("Profile registry full");
        deception_set_error("Profile registry at capacity");
        nimcp_mutex_unlock(&deception->lock);
        return 0;
    }

    // Find free slot
    uint32_t slot = deception->profile_count;
    uint32_t profile_id = slot + 1;

    // Copy profile
    memcpy(&deception->profiles[slot], profile, sizeof(*profile));
    deception->profiles[slot].profile_id = profile_id;
    deception->profiles[slot].active = true;

    deception->profile_count++;

    nimcp_mutex_unlock(&deception->lock);

    LOG_INFO("Registered mimicry profile %u: %s (eff=%.2f)",
             profile_id, profile->name, profile->effectiveness);

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "Mimicry profile registered",
                  "id=%u name=%s", profile_id, profile->name);

    return profile_id;
}

uint32_t portia_deception_get_profiles(
    portia_deception_t deception,
    mimicry_profile_t* profiles,
    uint32_t max_profiles)
{
    if (!(deception != NULL)) {
        return 0;
    }

    if (!profiles) {
        return 0;
    }

    nimcp_mutex_lock(&deception->lock);

    uint32_t count = 0;
    for (uint32_t i = 0; i < deception->profile_count && count < max_profiles; i++) {
        if (deception->profiles[i].active) {
            memcpy(&profiles[count], &deception->profiles[i],
                   sizeof(mimicry_profile_t));
            count++;
        }
    }

    nimcp_mutex_unlock(&deception->lock);

    LOG_DEBUG("Retrieved %u profiles", count);
    return count;
}

//=============================================================================
// Countermeasure Operations
//=============================================================================

int portia_deception_jam(
    portia_deception_t deception,
    bool enable)
{
    // Validate inputs
    if (!(deception != NULL)) {
        LOG_ERROR("Invalid deception pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (deception->magic != DECEPTION_MAGIC) {
        LOG_ERROR("Invalid deception magic");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!deception->config.enable_jamming) {
        LOG_ERROR("Jamming not enabled");
        deception_set_error("Jamming capability not enabled");
        return NIMCP_ERROR_NOT_SUPPORTED;
    }

    nimcp_mutex_lock(&deception->lock);

    bool old_state = deception->state.jamming_active;
    deception->state.jamming_active = enable;

    // Recalculate effectiveness
    deception->state.effectiveness = calculate_effectiveness(deception);

    nimcp_mutex_unlock(&deception->lock);

    LOG_INFO("Jamming %s (eff=%.2f)", enable ? "enabled" : "disabled",
             deception->state.effectiveness);

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "Jamming state changed",
                  "old=%d new=%d", old_state, enable);

    return NIMCP_SUCCESS;
}

int portia_deception_get_state(
    portia_deception_t deception,
    stealth_state_t* state)
{
    if (!(deception != NULL)) {
        LOG_ERROR("Invalid deception pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!(state != NULL)) {
        LOG_ERROR("Invalid state pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (deception->magic != DECEPTION_MAGIC) {
        LOG_ERROR("Invalid deception magic");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&deception->lock);
    memcpy(state, &deception->state, sizeof(*state));
    nimcp_mutex_unlock(&deception->lock);

    return NIMCP_SUCCESS;
}
