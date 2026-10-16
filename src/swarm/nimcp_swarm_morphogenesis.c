/**
 * @file nimcp_swarm_morphogenesis.c
 * @brief Implementation of Swarm Morphogenesis - Role Specialization System
 *
 * Inspired by cellular differentiation and slime mold specialization
 */

#include "swarm/nimcp_swarm_morphogenesis.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/validation/nimcp_common.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_morphogenesis)

/* ============================================================================
 * Constants and Configuration
 * ============================================================================ */

#define NIMCP_MORPH_DEFAULT_HISTORY_SIZE 100
#define NIMCP_MORPH_DEFAULT_GRID_SIZE 64
#define NIMCP_MORPH_MIN_COOLDOWN 1.0f
#define NIMCP_MORPH_MAX_COOLDOWN 300.0f
#define NIMCP_MORPH_GRADIENT_DECAY NIMCP_ELIGIBILITY_DECAY_DEFAULT
#define NIMCP_MORPH_POSITION_EPSILON 0.001f
#define NIMCP_MORPH_BALANCE_THRESHOLD 0.7f

/* Bio-async message types */
#define NIMCP_BIO_MSG_ROLE_CHANGE 0x4001
#define NIMCP_BIO_MSG_ROLE_REQUEST 0x4002
#define NIMCP_BIO_MSG_REBALANCE 0x4003

/* ============================================================================
 * Role Specifications (Biological Inspiration)
 * ============================================================================ */

static const NimcpRoleSpec ROLE_SPECS[NIMCP_SWARM_ROLE_COUNT] = {
    /* GENERALIST - Jack of all trades, master of none */
    {
        .role = NIMCP_SWARM_ROLE_GENERALIST,
        .name = "Generalist",
        .capabilities = NIMCP_ROLE_CAP_EXPLORE | NIMCP_ROLE_CAP_WORK |
                       NIMCP_ROLE_CAP_ADAPT | NIMCP_ROLE_CAP_SENSE,
        .energy_cost_multiplier = 1.0F,
        .processing_power = 1.0F,
        .sensing_range = 1.0F,
        .communication_range = 1.0F
    },
    /* SCOUT - Like scout ants, explores environment */
    {
        .role = NIMCP_SWARM_ROLE_SCOUT,
        .name = "Scout",
        .capabilities = NIMCP_ROLE_CAP_EXPLORE | NIMCP_ROLE_CAP_SENSE |
                       NIMCP_ROLE_CAP_ADAPT,
        .energy_cost_multiplier = 1.2F,
        .processing_power = 0.8F,
        .sensing_range = 2.0F,
        .communication_range = 0.8F
    },
    /* RELAY - Like neurons, specialized for communication */
    {
        .role = NIMCP_SWARM_ROLE_RELAY,
        .name = "Relay",
        .capabilities = NIMCP_ROLE_CAP_RELAY | NIMCP_ROLE_CAP_SENSE,
        .energy_cost_multiplier = 0.8F,
        .processing_power = 0.6F,
        .sensing_range = 1.0F,
        .communication_range = 2.5F
    },
    /* SENTINEL - Like immune cells, defends perimeter */
    {
        .role = NIMCP_SWARM_ROLE_SENTINEL,
        .name = "Sentinel",
        .capabilities = NIMCP_ROLE_CAP_DEFEND | NIMCP_ROLE_CAP_SENSE,
        .energy_cost_multiplier = 1.0F,
        .processing_power = 1.2F,
        .sensing_range = 1.5F,
        .communication_range = 1.0F
    },
    /* WORKER - Like worker bees, executes tasks */
    {
        .role = NIMCP_SWARM_ROLE_WORKER,
        .name = "Worker",
        .capabilities = NIMCP_ROLE_CAP_WORK | NIMCP_ROLE_CAP_ADAPT,
        .energy_cost_multiplier = 1.0F,
        .processing_power = 1.5F,
        .sensing_range = 0.8F,
        .communication_range = 0.8F
    },
    /* MEDIC - Like support cells, repairs others */
    {
        .role = NIMCP_SWARM_ROLE_MEDIC,
        .name = "Medic",
        .capabilities = NIMCP_ROLE_CAP_REPAIR | NIMCP_ROLE_CAP_SENSE,
        .energy_cost_multiplier = 1.1F,
        .processing_power = 1.0F,
        .sensing_range = 1.2F,
        .communication_range = 1.2F
    },
    /* LEADER - Like queen bee, coordinates swarm */
    {
        .role = NIMCP_SWARM_ROLE_LEADER,
        .name = "Leader",
        .capabilities = NIMCP_ROLE_CAP_COORDINATE | NIMCP_ROLE_CAP_SENSE |
                       NIMCP_ROLE_CAP_RELAY,
        .energy_cost_multiplier = 1.3F,
        .processing_power = 2.0F,
        .sensing_range = 1.5F,
        .communication_range = 2.0F
    }
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Calculate distance between two 3D points
 */
static inline float calculate_distance(const float p1[3], const float p2[3]) {
    float dx = p1[0] - p2[0];
    float dy = p1[1] - p2[1];
    float dz = p1[2] - p2[2];
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

/**
 * @brief Find drone state by ID
 */
static NimcpDroneRoleState* find_drone_state(
    NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id
) {
    if (!morph || !morph->drone_states) {
        return NULL;  /* Defensive guard - not an error */
    }

    for (uint32_t i = 0; i < morph->active_drones; i++) {
        if (morph->drone_states[i].drone_id == drone_id) {
            return &morph->drone_states[i];
        }
    }

    return NULL;  /* Not found - normal search miss */
}

/**
 * @brief Update swarm center position
 */
static void update_swarm_center(NimcpSwarmMorphogenesis* morph) {
    if (!morph || morph->active_drones == 0) {
        return;
    }

    float sum_x = 0.0F, sum_y = 0.0F, sum_z = 0.0F;

    for (uint32_t i = 0; i < morph->active_drones; i++) {
        sum_x += morph->drone_states[i].position[0];
        sum_y += morph->drone_states[i].position[1];
        sum_z += morph->drone_states[i].position[2];
    }

    morph->swarm_center[0] = sum_x / morph->active_drones;
    morph->swarm_center[1] = sum_y / morph->active_drones;
    morph->swarm_center[2] = sum_z / morph->active_drones;
}

/**
 * @brief Update role distribution statistics
 */
static void update_role_distribution(NimcpSwarmMorphogenesis* morph) {
    if (!morph) {
        return;
    }

    /* Reset counts */
    memset(morph->distribution.role_counts, 0,
           sizeof(morph->distribution.role_counts));

    /* Count roles */
    for (uint32_t i = 0; i < morph->active_drones; i++) {
        NimcpSwarmRole role = morph->drone_states[i].current_role;
        if (role >= 0 && role < NIMCP_SWARM_ROLE_COUNT) {
            morph->distribution.role_counts[role]++;
        }
    }

    morph->distribution.total_drones = morph->active_drones;

    /* Calculate ratios */
    if (morph->active_drones > 0) {
        for (int i = 0; i < NIMCP_SWARM_ROLE_COUNT; i++) {
            morph->distribution.role_ratios[i] =
                (float)morph->distribution.role_counts[i] / morph->active_drones;
        }
    }

    /* Calculate balance score (Shannon entropy-based) */
    float entropy = 0.0F;
    float max_entropy = logf((float)NIMCP_SWARM_ROLE_COUNT);

    for (int i = 0; i < NIMCP_SWARM_ROLE_COUNT; i++) {
        if (morph->distribution.role_ratios[i] > 0.0F) {
            entropy -= morph->distribution.role_ratios[i] *
                      logf(morph->distribution.role_ratios[i]);
        }
    }

    morph->distribution.balance_score =
        (max_entropy > 0.0F) ? (entropy / max_entropy) : 0.0F;

    /* Check if rebalancing needed */
    morph->distribution.needs_rebalancing =
        morph->distribution.balance_score < morph->rebalance_threshold;
}

/**
 * @brief Record role transition in history
 */
static void record_transition(
    NimcpDroneRoleState* drone_state,
    NimcpSwarmRole from_role,
    NimcpSwarmRole to_role,
    NimcpDifferentiationTrigger trigger
) {
    if (!drone_state || !drone_state->transition_history) {
        return;
    }

    uint32_t idx = drone_state->history_count % drone_state->history_size;
    NimcpRoleTransition* trans = &drone_state->transition_history[idx];

    trans->timestamp = nimcp_time_monotonic_ns();
    trans->from_role = from_role;
    trans->to_role = to_role;
    trans->trigger = trigger;

    /* Copy current morphogen values */
    for (int i = 0; i < NIMCP_MORPHOGEN_COUNT; i++) {
        trans->morphogen_values[i] = drone_state->gradients[i].concentration;
    }

    if (drone_state->history_count < drone_state->history_size) {
        drone_state->history_count++;
    }
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

NimcpSwarmMorphogenesis* nimcp_swarm_morphogenesis_create(
    uint32_t max_drones,
    float transition_cooldown,
    float rebalance_threshold
) {
    LOG_INFO("Creating swarm morphogenesis system: max_drones=%u, "
                   "cooldown=%.2f, rebalance_threshold=%.2f",
                   max_drones, transition_cooldown, rebalance_threshold);

    if (max_drones == 0 || max_drones > 100000) {
        LOG_ERROR("Invalid max_drones: %u", max_drones);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_swarm_morphogenesis_create: max_drones is zero");
        return NULL;
    }

    if (transition_cooldown < NIMCP_MORPH_MIN_COOLDOWN ||
        transition_cooldown > NIMCP_MORPH_MAX_COOLDOWN) {
        LOG_ERROR("Invalid transition_cooldown: %.2f", transition_cooldown);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_swarm_morphogenesis_create: transition_cooldown out of range");
        return NULL;
    }

    if (rebalance_threshold < 0.0F || rebalance_threshold > 1.0F) {
        LOG_ERROR("Invalid rebalance_threshold: %.2f", rebalance_threshold);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_swarm_morphogenesis_create: rebalance_threshold out of range");
        return NULL;
    }

    NimcpSwarmMorphogenesis* morph = nimcp_malloc(sizeof(NimcpSwarmMorphogenesis));
    if (!morph) {
        LOG_ERROR("Failed to allocate morphogenesis system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "morph is NULL");

        return NULL;
    }

    memset(morph, 0, sizeof(NimcpSwarmMorphogenesis));

    /* Set configuration */
    morph->max_drones = max_drones;
    morph->transition_cooldown = transition_cooldown;
    morph->rebalance_threshold = rebalance_threshold;
    morph->morphogen_update_rate = 10.0F; /* 10 Hz default */
    morph->gradient_grid_size = NIMCP_MORPH_DEFAULT_GRID_SIZE;

    /* Copy role specifications */
    memcpy(morph->role_specs, ROLE_SPECS, sizeof(ROLE_SPECS));

    /* Allocate drone states */
    morph->drone_states = nimcp_malloc(max_drones * sizeof(NimcpDroneRoleState));
    if (!morph->drone_states) {
        LOG_ERROR("Failed to allocate drone states");
        nimcp_free(morph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_swarm_morphogenesis_create: morph->drone_states is NULL");
        return NULL;
    }
    memset(morph->drone_states, 0, max_drones * sizeof(NimcpDroneRoleState));

    /* Allocate global gradients */
    morph->global_gradients = nimcp_malloc(
        NIMCP_MORPHOGEN_COUNT * sizeof(NimcpMorphogenGradient)
    );
    if (!morph->global_gradients) {
        LOG_ERROR("Failed to allocate global gradients");
        nimcp_free(morph->drone_states);
        nimcp_free(morph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_swarm_morphogenesis_create: morph->global_gradients is NULL");
        return NULL;
    }
    memset(morph->global_gradients, 0,
           NIMCP_MORPHOGEN_COUNT * sizeof(NimcpMorphogenGradient));

    /* Initialize gradient thresholds */
    for (int i = 0; i < NIMCP_MORPHOGEN_COUNT; i++) {
        morph->global_gradients[i].type = (NimcpMorphogenType)i;
        morph->global_gradients[i].threshold_scout = 0.7F;
        morph->global_gradients[i].threshold_relay = 0.5F;
        morph->global_gradients[i].threshold_sentinel = 0.6F;
        morph->global_gradients[i].threshold_worker = 0.4F;
        morph->global_gradients[i].threshold_medic = 0.3F;
        morph->global_gradients[i].threshold_leader = 0.8F;
    }

    /* Initialize synchronization */
    if (nimcp_platform_mutex_init(&morph->lock, false) != 0) {
        LOG_ERROR("Failed to initialize mutex");
        nimcp_free(morph->global_gradients);
        nimcp_free(morph->drone_states);
        nimcp_free(morph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_swarm_morphogenesis_create: validation failed");
        return NULL;
    }

    nimcp_atomic_init_i32(&morph->update_in_progress, 0);

    morph->initialized = true;

    LOG_INFO("Swarm morphogenesis system created successfully");
    return morph;
}

void nimcp_swarm_morphogenesis_destroy(NimcpSwarmMorphogenesis* morph) {
    if (!morph) {
        return;
    }

    LOG_INFO("Destroying swarm morphogenesis system");

    nimcp_platform_mutex_lock(&morph->lock);

    /* Free transition histories */
    if (morph->drone_states) {
        for (uint32_t i = 0; i < morph->active_drones; i++) {
            if (morph->drone_states[i].transition_history) {
                nimcp_free(morph->drone_states[i].transition_history);
            }
        }
        nimcp_free(morph->drone_states);
    }

    if (morph->global_gradients) {
        nimcp_free(morph->global_gradients);
    }

    nimcp_platform_mutex_unlock(&morph->lock);
    nimcp_platform_mutex_destroy(&morph->lock);

    morph->initialized = false;
    nimcp_free(morph);

    LOG_INFO("Swarm morphogenesis system destroyed");
}

nimcp_result_t nimcp_swarm_morphogenesis_init_bio_async(
    NimcpSwarmMorphogenesis* morph
) {
    if (!morph || !morph->initialized) {
        LOG_ERROR("Invalid morphogenesis system");
        return NIMCP_INVALID_PARAM;
    }

    LOG_INFO("Initializing bio-async integration for morphogenesis");

    nimcp_platform_mutex_lock(&morph->lock);

    /* Create module info for bio-router registration */
    bio_module_info_t bio_info = {
        .module_id = 0,  /* Auto-assign */
        .module_name = "morphogenesis",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_LARGE,
        .user_data = morph
    };

    /* Register with bio-router */
    morph->bio_ctx = bio_router_register_module(&bio_info);
    if (!morph->bio_ctx) {
        LOG_ERROR("Failed to register morphogenesis with bio-router");
        nimcp_platform_mutex_unlock(&morph->lock);
        return NIMCP_ERROR;
    }

    morph->bio_async_enabled = true;

    nimcp_platform_mutex_unlock(&morph->lock);

    LOG_INFO("Bio-async integration initialized successfully");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Drone Management Functions
 * ============================================================================ */

nimcp_result_t nimcp_swarm_morphogenesis_register_drone(
    NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    const float position[3]
) {
    if (!morph || !morph->initialized) {
        return NIMCP_INVALID_PARAM;
    }

    if (!position) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&morph->lock);

    if (morph->active_drones >= morph->max_drones) {
        LOG_ERROR("Cannot register drone: max capacity reached");
        nimcp_platform_mutex_unlock(&morph->lock);
        return NIMCP_NO_MEMORY;
    }

    /* Check if drone already registered */
    if (find_drone_state(morph, drone_id) != NULL) {
        LOG_WARN("Drone %u already registered", drone_id);
        nimcp_platform_mutex_unlock(&morph->lock);
        return NIMCP_ALREADY_EXISTS;
    }

    /* Initialize drone state */
    NimcpDroneRoleState* state = &morph->drone_states[morph->active_drones];
    memset(state, 0, sizeof(NimcpDroneRoleState));

    state->drone_id = drone_id;
    state->current_role = NIMCP_SWARM_ROLE_GENERALIST;
    state->desired_role = NIMCP_SWARM_ROLE_GENERALIST;

    state->position[0] = position[0];
    state->position[1] = position[1];
    state->position[2] = position[2];

    state->role_adoption_time = nimcp_time_monotonic_ns();
    state->last_transition_time = nimcp_time_monotonic_ns();
    state->fitness_score = 1.0F;
    state->adaptation_rate = 1.0F;

    /* Allocate transition history */
    state->history_size = NIMCP_MORPH_DEFAULT_HISTORY_SIZE;
    state->transition_history = nimcp_malloc(
        state->history_size * sizeof(NimcpRoleTransition)
    );
    if (!state->transition_history) {
        LOG_ERROR("Failed to allocate transition history");
        nimcp_platform_mutex_unlock(&morph->lock);
        return NIMCP_NO_MEMORY;
    }
    memset(state->transition_history, 0,
           state->history_size * sizeof(NimcpRoleTransition));

    /* Initialize gradients */
    for (int i = 0; i < NIMCP_MORPHOGEN_COUNT; i++) {
        state->gradients[i].type = (NimcpMorphogenType)i;
        state->gradients[i].concentration = 0.0F;
    }

    morph->active_drones++;

    /* Update swarm state */
    update_swarm_center(morph);
    update_role_distribution(morph);

    nimcp_platform_mutex_unlock(&morph->lock);

    LOG_INFO("Registered drone %u at position (%.2f, %.2f, %.2f)",
                   drone_id, position[0], position[1], position[2]);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_morphogenesis_unregister_drone(
    NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id
) {
    if (!morph || !morph->initialized) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&morph->lock);

    /* Find drone */
    int32_t drone_idx = -1;
    for (uint32_t i = 0; i < morph->active_drones; i++) {
        if (morph->drone_states[i].drone_id == drone_id) {
            drone_idx = (int32_t)i;
            break;
        }
    }

    if (drone_idx < 0) {
        LOG_WARN("Drone %u not found", drone_id);
        nimcp_platform_mutex_unlock(&morph->lock);
        return NIMCP_NOT_FOUND;
    }

    /* Free transition history */
    if (morph->drone_states[drone_idx].transition_history) {
        nimcp_free(morph->drone_states[drone_idx].transition_history);
    }

    /* Move last drone to this position */
    if ((uint32_t)drone_idx < morph->active_drones - 1) {
        memcpy(&morph->drone_states[drone_idx],
               &morph->drone_states[morph->active_drones - 1],
               sizeof(NimcpDroneRoleState));
    }

    morph->active_drones--;

    /* Update swarm state */
    update_swarm_center(morph);
    update_role_distribution(morph);

    nimcp_platform_mutex_unlock(&morph->lock);

    LOG_INFO("Unregistered drone %u", drone_id);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_morphogenesis_update_position(
    NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    const float position[3]
) {
    if (!morph || !morph->initialized || !position) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&morph->lock);

    NimcpDroneRoleState* state = find_drone_state(morph, drone_id);
    if (!state) {
        nimcp_platform_mutex_unlock(&morph->lock);
        return NIMCP_NOT_FOUND;
    }

    /* Update position */
    state->position[0] = position[0];
    state->position[1] = position[1];
    state->position[2] = position[2];

    /* Update distance from swarm center */
    state->swarm_center_distance = calculate_distance(
        state->position,
        morph->swarm_center
    );

    nimcp_platform_mutex_unlock(&morph->lock);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Role Assignment and Differentiation
 * ============================================================================ */

nimcp_result_t nimcp_swarm_morphogenesis_get_role(
    const NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    NimcpSwarmRole* out_role
) {
    if (!morph || !morph->initialized || !out_role) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&morph->lock);

    NimcpDroneRoleState* state = find_drone_state(
        (NimcpSwarmMorphogenesis*)morph,
        drone_id
    );
    if (!state) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&morph->lock);
        return NIMCP_NOT_FOUND;
    }

    *out_role = state->current_role;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&morph->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_morphogenesis_assign_role(
    NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    NimcpSwarmRole new_role,
    bool force
) {
    if (!morph || !morph->initialized) {
        return NIMCP_INVALID_PARAM;
    }

    if (new_role < 0 || new_role >= NIMCP_SWARM_ROLE_COUNT) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&morph->lock);

    NimcpDroneRoleState* state = find_drone_state(morph, drone_id);
    if (!state) {
        nimcp_platform_mutex_unlock(&morph->lock);
        return NIMCP_NOT_FOUND;
    }

    /* Check cooldown unless forced */
    if (!force && state->cooldown_remaining > 0.0F) {
        LOG_WARN("Drone %u in cooldown (%.2f seconds remaining)",
                       drone_id, state->cooldown_remaining);
        nimcp_platform_mutex_unlock(&morph->lock);
        return NIMCP_ERROR;
    }

    NimcpSwarmRole old_role = state->current_role;

    if (old_role == new_role) {
        nimcp_platform_mutex_unlock(&morph->lock);
        return NIMCP_SUCCESS; /* Already in target role */
    }

    /* Record transition */
    NimcpDifferentiationTrigger trigger = force ?
        NIMCP_DIFF_TRIGGER_COMMAND : NIMCP_DIFF_TRIGGER_POSITION;
    record_transition(state, old_role, new_role, trigger);

    /* Update role */
    state->current_role = new_role;
    state->desired_role = new_role;
    state->role_adoption_time = nimcp_time_monotonic_ns();
    state->last_transition_time = nimcp_time_monotonic_ns();
    state->transition_count++;
    state->cooldown_remaining = morph->transition_cooldown;
    state->in_transition = false;

    /* Update statistics */
    if (old_role == NIMCP_SWARM_ROLE_GENERALIST) {
        morph->total_differentiations++;
    } else if (new_role == NIMCP_SWARM_ROLE_GENERALIST) {
        morph->total_dedifferentiations++;
    }

    if (force) {
        morph->forced_transitions++;
    } else {
        morph->automatic_transitions++;
    }

    /* Update distribution */
    update_role_distribution(morph);

    nimcp_platform_mutex_unlock(&morph->lock);

    LOG_INFO("Drone %u role changed: %s -> %s (forced=%d)",
                   drone_id,
                   ROLE_SPECS[old_role].name,
                   ROLE_SPECS[new_role].name,
                   force);

    /* Broadcast role change if bio-async enabled */
    if (morph->bio_async_enabled) {
        nimcp_swarm_morphogenesis_broadcast_role_change(morph, drone_id, new_role);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_morphogenesis_dedifferentiate(
    NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id
) {
    return nimcp_swarm_morphogenesis_assign_role(
        morph,
        drone_id,
        NIMCP_SWARM_ROLE_GENERALIST,
        false
    );
}

nimcp_result_t nimcp_swarm_morphogenesis_can_differentiate(
    const NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    bool* out_can_differentiate
) {
    if (!morph || !morph->initialized || !out_can_differentiate) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&morph->lock);

    NimcpDroneRoleState* state = find_drone_state(
        (NimcpSwarmMorphogenesis*)morph,
        drone_id
    );
    if (!state) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&morph->lock);
        return NIMCP_NOT_FOUND;
    }

    *out_can_differentiate = (state->cooldown_remaining <= 0.0F);

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&morph->lock);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Morphogen Gradient Functions
 * ============================================================================ */

nimcp_result_t nimcp_swarm_morphogenesis_update_gradients(
    NimcpSwarmMorphogenesis* morph,
    float delta_time
) {
    if (!morph || !morph->initialized) {
        return NIMCP_INVALID_PARAM;
    }

    if (nimcp_atomic_load_i32(&morph->update_in_progress, NIMCP_MEMORY_ORDER_SEQ_CST)) {
        return NIMCP_SUCCESS; /* Already updating */
    }

    nimcp_atomic_store_i32(&morph->update_in_progress, 1, NIMCP_MEMORY_ORDER_SEQ_CST);
    nimcp_platform_mutex_lock(&morph->lock);

    /* Update swarm center first */
    update_swarm_center(morph);

    /* Update morphogen concentrations for each drone */
    for (uint32_t i = 0; i < morph->active_drones; i++) {
        NimcpDroneRoleState* state = &morph->drone_states[i];

        /* Update distance from center */
        state->swarm_center_distance = calculate_distance(
            state->position,
            morph->swarm_center
        );

        /* Center distance morphogen (higher at edges) */
        float max_distance = 100.0F; /* Normalize to 100 units */
        float normalized_dist = fminf(state->swarm_center_distance / max_distance, 1.0F);
        state->gradients[NIMCP_MORPHOGEN_CENTER_DISTANCE].concentration = normalized_dist;

        /* Connectivity morphogen (based on number of nearby drones) */
        uint32_t nearby_count = 0;
        float connectivity_radius = 10.0F;
        for (uint32_t j = 0; j < morph->active_drones; j++) {
            if (i != j) {
                float dist = calculate_distance(
                    state->position,
                    morph->drone_states[j].position
                );
                if (dist < connectivity_radius) {
                    nearby_count++;
                }
            }
        }
        float normalized_connectivity = fminf((float)nearby_count / 10.0F, 1.0F);
        state->gradients[NIMCP_MORPHOGEN_CONNECTIVITY].concentration =
            normalized_connectivity;

        /* Apply gradient decay */
        for (int m = 0; m < NIMCP_MORPHOGEN_COUNT; m++) {
            state->gradients[m].concentration *= NIMCP_MORPH_GRADIENT_DECAY;
        }

        /* Update cooldown */
        if (state->cooldown_remaining > 0.0F) {
            state->cooldown_remaining -= delta_time;
            if (state->cooldown_remaining < 0.0F) {
                state->cooldown_remaining = 0.0F;
            }
        }
    }

    nimcp_platform_mutex_unlock(&morph->lock);
    nimcp_atomic_store_i32(&morph->update_in_progress, 0, NIMCP_MEMORY_ORDER_SEQ_CST);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_morphogenesis_set_morphogen(
    NimcpSwarmMorphogenesis* morph,
    NimcpMorphogenType morphogen_type,
    const float position[3],
    float concentration
) {
    if (!morph || !morph->initialized || !position) {
        return NIMCP_INVALID_PARAM;
    }

    if (morphogen_type < 0 || morphogen_type >= NIMCP_MORPHOGEN_COUNT) {
        return NIMCP_INVALID_PARAM;
    }

    if (concentration < 0.0F || concentration > 1.0F) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&morph->lock);

    /* Find nearest drone and set its morphogen */
    float min_dist = FLT_MAX;
    NimcpDroneRoleState* nearest = NULL;

    for (uint32_t i = 0; i < morph->active_drones; i++) {
        float dist = calculate_distance(position, morph->drone_states[i].position);
        if (dist < min_dist) {
            min_dist = dist;
            nearest = &morph->drone_states[i];
        }
    }

    if (nearest) {
        nearest->gradients[morphogen_type].concentration = concentration;
    }

    nimcp_platform_mutex_unlock(&morph->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_morphogenesis_get_morphogen(
    const NimcpSwarmMorphogenesis* morph,
    NimcpMorphogenType morphogen_type,
    const float position[3],
    float* out_concentration
) {
    if (!morph || !morph->initialized || !position || !out_concentration) {
        return NIMCP_INVALID_PARAM;
    }

    if (morphogen_type < 0 || morphogen_type >= NIMCP_MORPHOGEN_COUNT) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&morph->lock);

    /* Find nearest drone and get its morphogen */
    float min_dist = FLT_MAX;
    const NimcpDroneRoleState* nearest = NULL;

    for (uint32_t i = 0; i < morph->active_drones; i++) {
        float dist = calculate_distance(position, morph->drone_states[i].position);
        if (dist < min_dist) {
            min_dist = dist;
            nearest = &morph->drone_states[i];
        }
    }

    if (nearest) {
        *out_concentration = nearest->gradients[morphogen_type].concentration;
    } else {
        *out_concentration = 0.0F;
    }

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&morph->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_morphogenesis_evaluate_differentiation(
    const NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    NimcpSwarmRole* out_suggested_role,
    float* out_confidence
) {
    if (!morph || !morph->initialized || !out_suggested_role || !out_confidence) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&morph->lock);

    NimcpDroneRoleState* state = find_drone_state(
        (NimcpSwarmMorphogenesis*)morph,
        drone_id
    );
    if (!state) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&morph->lock);
        return NIMCP_NOT_FOUND;
    }

    /* Evaluate based on morphogen gradients */
    float center_dist = state->gradients[NIMCP_MORPHOGEN_CENTER_DISTANCE].concentration;
    float connectivity = state->gradients[NIMCP_MORPHOGEN_CONNECTIVITY].concentration;
    float resource = state->gradients[NIMCP_MORPHOGEN_RESOURCE].concentration;
    float threat = state->gradients[NIMCP_MORPHOGEN_THREAT].concentration;

    NimcpSwarmRole suggested = NIMCP_SWARM_ROLE_GENERALIST;
    float confidence = 0.5F;

    /* Scout: high center distance, low connectivity */
    if (center_dist > 0.7F && connectivity < 0.3F) {
        suggested = NIMCP_SWARM_ROLE_SCOUT;
        confidence = 0.8F;
    }
    /* Relay: medium connectivity, medium center distance */
    else if (connectivity > 0.4F && connectivity < 0.7F) {
        suggested = NIMCP_SWARM_ROLE_RELAY;
        confidence = 0.7F;
    }
    /* Sentinel: high threat or high center distance with medium connectivity */
    else if (threat > 0.6F || (center_dist > 0.6F && connectivity > 0.4F)) {
        suggested = NIMCP_SWARM_ROLE_SENTINEL;
        confidence = 0.75F;
    }
    /* Worker: high resource proximity, medium connectivity */
    else if (resource > 0.5F && connectivity > 0.3F) {
        suggested = NIMCP_SWARM_ROLE_WORKER;
        confidence = 0.8F;
    }
    /* Leader: low center distance, high connectivity */
    else if (center_dist < 0.3F && connectivity > 0.7F) {
        suggested = NIMCP_SWARM_ROLE_LEADER;
        confidence = 0.85F;
    }
    /* Medic: medium everything, balanced position */
    else if (connectivity > 0.5F && center_dist < 0.5F) {
        suggested = NIMCP_SWARM_ROLE_MEDIC;
        confidence = 0.65F;
    }

    *out_suggested_role = suggested;
    *out_confidence = confidence;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&morph->lock);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Load Balancing Functions
 * ============================================================================ */

nimcp_result_t nimcp_swarm_morphogenesis_get_distribution(
    const NimcpSwarmMorphogenesis* morph,
    NimcpRoleDistribution* out_distribution
) {
    if (!morph || !morph->initialized || !out_distribution) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&morph->lock);

    memcpy(out_distribution, &morph->distribution, sizeof(NimcpRoleDistribution));

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&morph->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_morphogenesis_is_balanced(
    const NimcpSwarmMorphogenesis* morph,
    bool* out_balanced,
    float* out_balance_score
) {
    if (!morph || !morph->initialized || !out_balanced || !out_balance_score) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&morph->lock);

    *out_balanced = !morph->distribution.needs_rebalancing;
    *out_balance_score = morph->distribution.balance_score;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&morph->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_morphogenesis_rebalance(
    NimcpSwarmMorphogenesis* morph,
    const float* target_distribution
) {
    if (!morph || !morph->initialized) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&morph->lock);

    LOG_INFO("Rebalancing swarm role distribution");

    /* Default target distribution if none provided */
    float default_targets[NIMCP_SWARM_ROLE_COUNT] = {
        0.30F, /* Generalist */
        0.15F, /* Scout */
        0.15F, /* Relay */
        0.15F, /* Sentinel */
        0.15F, /* Worker */
        0.05F, /* Medic */
        0.05F  /* Leader */
    };

    const float* targets = target_distribution ? target_distribution : default_targets;

    /* Calculate target counts */
    uint32_t target_counts[NIMCP_SWARM_ROLE_COUNT];
    for (int i = 0; i < NIMCP_SWARM_ROLE_COUNT; i++) {
        target_counts[i] = (uint32_t)(targets[i] * morph->active_drones);
    }

    /* Reassign roles to match targets */
    for (int role = 0; role < NIMCP_SWARM_ROLE_COUNT; role++) {
        int32_t diff = (int32_t)target_counts[role] -
                       (int32_t)morph->distribution.role_counts[role];

        if (diff > 0) {
            /* Need more of this role - find generalists to convert */
            uint32_t converted = 0;
            for (uint32_t i = 0; i < morph->active_drones && converted < (uint32_t)diff; i++) {
                NimcpDroneRoleState* state = &morph->drone_states[i];
                if (state->current_role == NIMCP_SWARM_ROLE_GENERALIST &&
                    state->cooldown_remaining <= 0.0F) {

                    record_transition(state, state->current_role,
                                    (NimcpSwarmRole)role,
                                    NIMCP_DIFF_TRIGGER_IMBALANCE);

                    state->current_role = (NimcpSwarmRole)role;
                    state->desired_role = (NimcpSwarmRole)role;
                    state->role_adoption_time = nimcp_time_monotonic_ns();
                    state->last_transition_time = nimcp_time_monotonic_ns();
                    state->transition_count++;
                    state->cooldown_remaining = morph->transition_cooldown;

                    converted++;
                    morph->automatic_transitions++;
                }
            }
        }
    }

    /* Update distribution */
    update_role_distribution(morph);

    nimcp_platform_mutex_unlock(&morph->lock);

    LOG_INFO("Rebalancing complete - balance score: %.2f",
                   morph->distribution.balance_score);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Role Capability Functions
 * ============================================================================ */

uint32_t nimcp_swarm_morphogenesis_get_role_capabilities(NimcpSwarmRole role) {
    if (role < 0 || role >= NIMCP_SWARM_ROLE_COUNT) {
        return 0;
    }
    return ROLE_SPECS[role].capabilities;
}

bool nimcp_swarm_morphogenesis_role_has_capability(
    NimcpSwarmRole role,
    NimcpRoleCapability capability
) {
    if (role < 0 || role >= NIMCP_SWARM_ROLE_COUNT) {
        return false;
    }
    return (ROLE_SPECS[role].capabilities & capability) != 0;
}

const char* nimcp_swarm_morphogenesis_role_name(NimcpSwarmRole role) {
    if (role < 0 || role >= NIMCP_SWARM_ROLE_COUNT) {
        return "Unknown";
    }
    return ROLE_SPECS[role].name;
}

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

nimcp_result_t nimcp_swarm_morphogenesis_get_transition_history(
    const NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    NimcpRoleTransition* out_history,
    uint32_t max_entries,
    uint32_t* out_count
) {
    if (!morph || !morph->initialized || !out_history || !out_count) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&morph->lock);

    NimcpDroneRoleState* state = find_drone_state(
        (NimcpSwarmMorphogenesis*)morph,
        drone_id
    );
    if (!state) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&morph->lock);
        return NIMCP_NOT_FOUND;
    }

    uint32_t count = state->history_count < max_entries ?
                     state->history_count : max_entries;

    memcpy(out_history, state->transition_history,
           count * sizeof(NimcpRoleTransition));

    *out_count = count;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&morph->lock);

    return NIMCP_SUCCESS;
}

void nimcp_swarm_morphogenesis_get_statistics(
    const NimcpSwarmMorphogenesis* morph,
    uint64_t* out_total_diff,
    uint64_t* out_total_dediff,
    uint64_t* out_forced,
    uint64_t* out_automatic
) {
    if (!morph || !morph->initialized) {
        return;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&morph->lock);

    if (out_total_diff) *out_total_diff = morph->total_differentiations;
    if (out_total_dediff) *out_total_dediff = morph->total_dedifferentiations;
    if (out_forced) *out_forced = morph->forced_transitions;
    if (out_automatic) *out_automatic = morph->automatic_transitions;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&morph->lock);
}

void nimcp_swarm_morphogenesis_print_state(
    const NimcpSwarmMorphogenesis* morph,
    bool verbose
) {
    if (!morph || !morph->initialized) {
        printf("Morphogenesis system not initialized\n");
        return;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&morph->lock);

    printf("\n=== Swarm Morphogenesis State ===\n");
    printf("Active Drones: %u / %u\n", morph->active_drones, morph->max_drones);
    printf("Swarm Center: (%.2f, %.2f, %.2f)\n",
           morph->swarm_center[0], morph->swarm_center[1], morph->swarm_center[2]);

    printf("\nRole Distribution:\n");
    for (int i = 0; i < NIMCP_SWARM_ROLE_COUNT; i++) {
        printf("  %s: %u (%.1f%%)\n",
               ROLE_SPECS[i].name,
               morph->distribution.role_counts[i],
               morph->distribution.role_ratios[i] * 100.0F);
    }

    printf("\nBalance Score: %.2f (threshold: %.2f)\n",
           morph->distribution.balance_score,
           morph->rebalance_threshold);
    printf("Needs Rebalancing: %s\n",
           morph->distribution.needs_rebalancing ? "Yes" : "No");

    printf("\nStatistics:\n");
    printf("  Total Differentiations: %lu\n", morph->total_differentiations);
    printf("  Total De-differentiations: %lu\n", morph->total_dedifferentiations);
    printf("  Forced Transitions: %lu\n", morph->forced_transitions);
    printf("  Automatic Transitions: %lu\n", morph->automatic_transitions);

    if (verbose && morph->active_drones > 0) {
        printf("\nDrone Details:\n");
        for (uint32_t i = 0; i < morph->active_drones; i++) {
            const NimcpDroneRoleState* state = &morph->drone_states[i];
            printf("  Drone %u: %s, transitions=%u, cooldown=%.1fs\n",
                   state->drone_id,
                   ROLE_SPECS[state->current_role].name,
                   state->transition_count,
                   state->cooldown_remaining);
        }
    }

    printf("================================\n\n");

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&morph->lock);
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

nimcp_result_t nimcp_swarm_morphogenesis_process_messages(
    NimcpSwarmMorphogenesis* morph
) {
    if (!morph || !morph->initialized || !morph->bio_async_enabled) {
        return NIMCP_INVALID_PARAM;
    }

    if (!morph->bio_ctx) {
        return NIMCP_ERROR;
    }

    /* Process inbox using bio-router */
    size_t processed = bio_router_process_inbox(morph->bio_ctx, 100);
    if (processed > 0) {
        /* Update distribution after processing any role-related messages */
        nimcp_platform_mutex_lock(&morph->lock);
        update_role_distribution(morph);
        nimcp_platform_mutex_unlock(&morph->lock);
        LOG_DEBUG("Processed %zu morphogenesis messages", processed);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_morphogenesis_broadcast_role_change(
    NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    NimcpSwarmRole new_role
) {
    if (!morph || !morph->initialized || !morph->bio_async_enabled) {
        return NIMCP_INVALID_PARAM;
    }

    if (!morph->bio_ctx) {
        return NIMCP_ERROR;
    }

    /* Create bio-async message with embedded header */
    typedef struct {
        bio_message_header_t header;
        uint32_t drone_id;
        uint32_t role;
    } bio_msg_role_change_t;

    bio_msg_role_change_t msg = {0};
    bio_msg_init_header(&msg.header, NIMCP_BIO_MSG_ROLE_CHANGE,
                        bio_module_context_get_id(morph->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.drone_id = drone_id;
    msg.role = (uint32_t)new_role;

    /* Broadcast to all modules */
    bio_router_broadcast(morph->bio_ctx, &msg, sizeof(msg));

    return NIMCP_SUCCESS;
}
