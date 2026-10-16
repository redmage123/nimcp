/**
 * @file nimcp_swarm_quorum.c
 * @brief Implementation of Quorum Sensing and Decision Threshold system
 *
 * Implements distributed decision-making inspired by bacterial quorum sensing
 * and honeybee nest-site selection through signal accumulation, threshold
 * activation, positive feedback, and cross-inhibition.
 */

#include "swarm/nimcp_swarm_quorum.h"
#include "core/brain/nimcp_brain.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdio.h>

#define LOG_MODULE "swarm_quorum"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_quorum)

/* Default configuration values */
#define DEFAULT_BASE_THRESHOLD 0.7
#define DEFAULT_THRESHOLD_VARIANCE 0.1
#define DEFAULT_DECAY_RATE 0.05
#define DEFAULT_AMPLIFICATION_FACTOR 1.5
#define DEFAULT_INHIBITION_STRENGTH 0.3
#define DEFAULT_HYSTERESIS_WIDTH 0.1
#define DEFAULT_COMMIT_THRESHOLD_LOW 0.3
#define DEFAULT_COMMIT_THRESHOLD_HIGH 0.7
#define DEFAULT_AMPLIFY_THRESHOLD 0.9
#define DEFAULT_MIN_QUORUM_SIZE 3
#define DEFAULT_CASCADE_SPEED 0.2

/* Initial capacities */
#define INITIAL_COMMITMENT_CAPACITY 32
#define INITIAL_DECISION_CAPACITY 16

/* Bio-async message topics */
#define TOPIC_QUORUM_SIGNAL "quorum.signal"
#define TOPIC_QUORUM_COMMIT "quorum.commit"
#define TOPIC_QUORUM_DECISION "quorum.decision"

/* Helper macros */
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* Signal type names */
static const char* signal_names[NIMCP_SIGNAL_COUNT] = {
    "ATTACK", "RETREAT", "EXPLORE", "DEFEND",
    "RESOURCE", "ALERT", "FORMATION", "LEADER"
};

/* Decision type names */
static const char* decision_names[NIMCP_DECISION_COUNT] = {
    "TARGET_SELECT", "FORMATION_CHANGE", "RETREAT",
    "RESOURCE_ALLOC", "LEADER_ELECT", "PATROL_ROUTE",
    "ATTACK_TIMING"
};

/* Commitment state names */
static const char* commitment_names[] = {
    "UNCOMMITTED", "LEANING", "COMMITTED", "AMPLIFYING"
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Initialize a signal molecule with default values
 */
static void init_signal_molecule(
    nimcp_signal_molecule_t* signal,
    double threshold,
    double decay_rate,
    double amplification,
    double inhibition,
    double hysteresis_width
) {
    signal->concentration = 0.0;
    signal->threshold = threshold;
    signal->decay_rate = decay_rate;
    signal->amplification = amplification;
    signal->inhibition = inhibition;
    signal->last_update_time = nimcp_time_get_us();
    signal->committed_count = 0;
    signal->threshold_reached = false;
    signal->hysteresis_low = MAX(0.0, threshold - hysteresis_width / 2.0);
    signal->hysteresis_high = MIN(1.0, threshold + hysteresis_width / 2.0);
}

/* Forward declaration for unlocked helpers */
static int check_vote_consistency_unlocked(
    nimcp_swarm_quorum_t* quorum,
    uint32_t* contradicting_agents,
    uint32_t* count
);

/**
 * @brief Find commitment by drone ID
 */
static nimcp_drone_commitment_t* find_commitment(
    nimcp_swarm_quorum_t* quorum,
    uint32_t drone_id
) {
    for (uint32_t i = 0; i < quorum->commitment_count; i++) {
        if (quorum->commitments[i].drone_id == drone_id) {
            return &quorum->commitments[i];
        }
    }
    return NULL;  /* Not found - normal search miss */
}

/**
 * @brief Add new commitment record
 */
static nimcp_drone_commitment_t* add_commitment(
    nimcp_swarm_quorum_t* quorum,
    uint32_t drone_id
) {
    /* Expand array if needed */
    if (quorum->commitment_count >= quorum->commitment_capacity) {
        /* Check for overflow before doubling capacity */
        if (quorum->commitment_capacity > UINT32_MAX / 2) {
            LOG_ERROR("Commitment capacity overflow");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "add_commitment: capacity overflow");
            return NULL;
        }
        uint32_t new_capacity = quorum->commitment_capacity * 2;
        /* Check for allocation size overflow */
        if (new_capacity > SIZE_MAX / sizeof(nimcp_drone_commitment_t)) {
            LOG_ERROR("Commitment array size overflow");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "add_commitment: validation failed");
            return NULL;
        }
        nimcp_drone_commitment_t* new_array = (nimcp_drone_commitment_t*)nimcp_realloc(
            quorum->commitments,
            new_capacity * sizeof(nimcp_drone_commitment_t)
        );
        if (!new_array) {
            LOG_ERROR("Failed to expand commitment array");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "add_commitment: new_array is NULL");
            return NULL;
        }
        quorum->commitments = new_array;
        quorum->commitment_capacity = new_capacity;
    }

    /* Initialize new commitment */
    nimcp_drone_commitment_t* commit = &quorum->commitments[quorum->commitment_count++];
    commit->drone_id = drone_id;
    commit->signal = NIMCP_SIGNAL_ATTACK; /* Default */
    commit->state = NIMCP_COMMIT_UNCOMMITTED;
    commit->commitment_strength = 0.0;
    commit->commitment_time = nimcp_time_get_us();
    commit->recruitment_count = 0;
    commit->is_amplifying = false;

    return commit;
}

/**
 * @brief Update commitment state based on strength
 */
static void update_commitment_state(
    nimcp_drone_commitment_t* commit,
    double strength,
    const nimcp_quorum_config_t* config
) {
    nimcp_commitment_state_t old_state = commit->state;

    if (strength < config->commitment_threshold_low) {
        commit->state = NIMCP_COMMIT_UNCOMMITTED;
        commit->is_amplifying = false;
    } else if (strength < config->commitment_threshold_high) {
        commit->state = NIMCP_COMMIT_LEANING;
        commit->is_amplifying = false;
    } else if (strength < config->amplification_threshold) {
        commit->state = NIMCP_COMMIT_COMMITTED;
        commit->is_amplifying = false;
    } else {
        commit->state = NIMCP_COMMIT_COMMITTED;
        commit->is_amplifying = true;
    }

    /* Update commitment time on state change */
    if (old_state != commit->state) {
        commit->commitment_time = nimcp_time_get_us();
    }
}

/**
 * @brief Calculate signal decay over time
 */
static double calculate_decay(double concentration, double decay_rate, double delta_time_sec) {
    /* Exponential decay */
    return concentration * exp(-decay_rate * delta_time_sec);
}

/**
 * @brief Add decision record
 */
static bool add_decision(
    nimcp_swarm_quorum_t* quorum,
    nimcp_decision_type_t type,
    nimcp_signal_type_t signal,
    double consensus,
    uint32_t participating,
    uint32_t committed,
    void* data
) {
    /* Expand array if needed */
    if (quorum->decision_count >= quorum->decision_capacity) {
        /* Check for overflow before doubling capacity */
        if (quorum->decision_capacity > UINT32_MAX / 2) {
            LOG_ERROR("Decision capacity overflow");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "add_decision: validation failed");
            return false;
        }
        uint32_t new_capacity = quorum->decision_capacity * 2;
        /* Check for allocation size overflow */
        if (new_capacity > SIZE_MAX / sizeof(nimcp_quorum_decision_t)) {
            LOG_ERROR("Decision array size overflow");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "add_decision: validation failed");
            return false;
        }
        nimcp_quorum_decision_t* new_array = (nimcp_quorum_decision_t*)nimcp_realloc(
            quorum->decisions,
            new_capacity * sizeof(nimcp_quorum_decision_t)
        );
        if (!new_array) {
            LOG_ERROR("Failed to expand decision array");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "add_decision: realloc failed");
            return false;
        }
        quorum->decisions = new_array;
        quorum->decision_capacity = new_capacity;
    }

    /* Add decision */
    nimcp_quorum_decision_t* decision = &quorum->decisions[quorum->decision_count++];
    decision->type = type;
    decision->winning_signal = signal;
    decision->consensus_strength = consensus;
    decision->decision_time = nimcp_time_get_us();
    decision->participating_drones = participating;
    decision->committed_drones = committed;
    decision->is_final = false;
    decision->decision_data = data;

    return true;
}

/* ============================================================================
 * Core API Implementation
 * ============================================================================ */

void nimcp_swarm_quorum_default_config(nimcp_quorum_config_t* config) {
    if (!config) return;

    config->base_threshold = DEFAULT_BASE_THRESHOLD;
    config->threshold_variance = DEFAULT_THRESHOLD_VARIANCE;
    config->decay_rate = DEFAULT_DECAY_RATE;
    config->amplification_factor = DEFAULT_AMPLIFICATION_FACTOR;
    config->inhibition_strength = DEFAULT_INHIBITION_STRENGTH;
    config->hysteresis_width = DEFAULT_HYSTERESIS_WIDTH;
    config->commitment_threshold_low = DEFAULT_COMMIT_THRESHOLD_LOW;
    config->commitment_threshold_high = DEFAULT_COMMIT_THRESHOLD_HIGH;
    config->amplification_threshold = DEFAULT_AMPLIFY_THRESHOLD;
    config->min_quorum_size = DEFAULT_MIN_QUORUM_SIZE;
    config->cascade_speed = DEFAULT_CASCADE_SPEED;
    config->enable_cross_inhibition = true;
    config->enable_positive_feedback = true;
    config->enable_hysteresis = true;
}

nimcp_swarm_quorum_t* nimcp_swarm_quorum_create(
    const nimcp_quorum_config_t* config,
    struct nimcp_brain* brain
) {
    LOG_DEBUG("Creating quorum sensing system");

    nimcp_swarm_quorum_t* quorum = (nimcp_swarm_quorum_t*)nimcp_calloc(
        1, sizeof(nimcp_swarm_quorum_t)
    );
    if (!quorum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_swarm_quorum_create: failed to allocate quorum system");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        quorum->config = *config;
    } else {
        nimcp_swarm_quorum_default_config(&quorum->config);
    }

    /* Initialize signal molecules */
    for (int i = 0; i < NIMCP_SIGNAL_COUNT; i++) {
        /* Add some variance to thresholds for diversity */
        double threshold = quorum->config.base_threshold +
            (nimcp_tl_rand() / (double)RAND_MAX - 0.5) * quorum->config.threshold_variance;
        threshold = CLAMP(threshold, 0.5, 0.95);

        init_signal_molecule(
            &quorum->signals[i],
            threshold,
            quorum->config.decay_rate,
            quorum->config.amplification_factor,
            quorum->config.inhibition_strength,
            quorum->config.hysteresis_width
        );
    }

    /* Allocate commitment array */
    quorum->commitment_capacity = INITIAL_COMMITMENT_CAPACITY;
    quorum->commitments = (nimcp_drone_commitment_t*)nimcp_calloc(
        quorum->commitment_capacity,
        sizeof(nimcp_drone_commitment_t)
    );
    if (!quorum->commitments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_swarm_quorum_create: failed to allocate commitment array");
        nimcp_free(quorum);
        return NULL;
    }

    /* Allocate decision array */
    quorum->decision_capacity = INITIAL_DECISION_CAPACITY;
    quorum->decisions = (nimcp_quorum_decision_t*)nimcp_calloc(
        quorum->decision_capacity,
        sizeof(nimcp_quorum_decision_t)
    );
    if (!quorum->decisions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_swarm_quorum_create: failed to allocate decision array");
        nimcp_free(quorum->commitments);
        nimcp_free(quorum);
        return NULL;
    }

    /* Create mutex */
    quorum->mutex = nimcp_platform_mutex_create();
    if (!quorum->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_swarm_quorum_create: failed to create mutex");
        nimcp_free(quorum->decisions);
        nimcp_free(quorum->commitments);
        nimcp_free(quorum);
        return NULL;
    }

    /* Initialize state */
    quorum->brain = brain;
    quorum->creation_time = nimcp_time_get_us();
    quorum->is_active = true;

    /* Register with bio-async router */
    quorum->bio_ctx = NULL;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_SWARM_QUORUM,
            .module_name = "swarm_quorum",
            .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
            .user_data = quorum
        };
        quorum->bio_ctx = bio_router_register_module(&bio_info);
        if (quorum->bio_ctx) {
            LOG_DEBUG("Registered with bio-async router");
        }
    }

    /* Register bio-async handlers if brain provided */
    if (brain) {
        nimcp_quorum_register_handlers(quorum, brain);
    }

    LOG_INFO("Quorum sensing system created successfully");
    return quorum;
}

void nimcp_swarm_quorum_destroy(nimcp_swarm_quorum_t* quorum) {
    if (!quorum) return;

    LOG_DEBUG("Destroying quorum sensing system");

    /* Unregister from bio-async router */
    if (quorum->bio_ctx && bio_router_is_initialized()) {
        bio_router_unregister_module(quorum->bio_ctx);
        quorum->bio_ctx = NULL;
    }

    if (quorum->mutex) {
        nimcp_platform_mutex_destroy(quorum->mutex);
        nimcp_free(quorum->mutex);
        quorum->mutex = NULL;
    }

    nimcp_free(quorum->decisions);
    nimcp_free(quorum->commitments);
    nimcp_free(quorum);
}

/* ============================================================================
 * Signal Broadcasting
 * ============================================================================ */

bool nimcp_quorum_broadcast_signal(
    nimcp_swarm_quorum_t* quorum,
    uint32_t drone_id,
    nimcp_signal_type_t signal,
    double strength
) {
    if (!quorum || signal >= NIMCP_SIGNAL_COUNT) {
        LOG_ERROR("Invalid parameters for signal broadcast");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "nimcp_quorum_broadcast_signal: quorum is NULL");
        return false;
    }

    strength = CLAMP(strength, 0.0, 1.0);

    nimcp_platform_mutex_lock(quorum->mutex);

    /* Update local signal concentration */
    nimcp_signal_molecule_t* sig = &quorum->signals[signal];
    sig->concentration = MIN(1.0, sig->concentration + strength);
    sig->last_update_time = nimcp_time_get_us();

    /* Update statistics */
    quorum->stats.total_signals_broadcast++;

    nimcp_platform_mutex_unlock(quorum->mutex);

    /* Bio-async broadcast - stubbed for now (requires brain integration) */
    (void)quorum->brain;  /* Suppress unused warning */

    LOG_DEBUG("Drone %u broadcast signal %s with strength %.2f",
                    drone_id, signal_names[signal], strength);

    return true;
}

bool nimcp_quorum_receive_signal(
    nimcp_swarm_quorum_t* quorum,
    nimcp_signal_type_t signal,
    double strength,
    uint32_t source_drone
) {
    if (!quorum || signal >= NIMCP_SIGNAL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "nimcp_quorum_receive_signal: quorum is NULL");
        return false;
    }

    strength = CLAMP(strength, 0.0, 1.0);

    nimcp_platform_mutex_lock(quorum->mutex);

    /* Accumulate signal concentration */
    nimcp_signal_molecule_t* sig = &quorum->signals[signal];
    sig->concentration = MIN(1.0, sig->concentration + strength * 0.5); /* Reduced weight for received signals */
    sig->last_update_time = nimcp_time_get_us();

    nimcp_platform_mutex_unlock(quorum->mutex);

    LOG_DEBUG("Received signal %s (strength %.2f) from drone %u",
                    signal_names[signal], strength, source_drone);

    return true;
}

void nimcp_quorum_update_signals(
    nimcp_swarm_quorum_t* quorum,
    double delta_time
) {
    if (!quorum || delta_time <= 0.0) return;

    nimcp_platform_mutex_lock(quorum->mutex);

    double delta_time_sec = delta_time / 1000.0; /* Convert ms to seconds */

    for (int i = 0; i < NIMCP_SIGNAL_COUNT; i++) {
        nimcp_signal_molecule_t* sig = &quorum->signals[i];

        /* Apply decay */
        sig->concentration = calculate_decay(
            sig->concentration,
            sig->decay_rate,
            delta_time_sec
        );

        /* Apply positive feedback if enabled */
        if (quorum->config.enable_positive_feedback && sig->committed_count > 0) {
            double amplification = 1.0 + (sig->committed_count * 0.1 * sig->amplification);
            sig->concentration = MIN(1.0, sig->concentration * amplification);
        }

        /* Update threshold state with hysteresis */
        if (quorum->config.enable_hysteresis) {
            if (!sig->threshold_reached && sig->concentration >= sig->hysteresis_high) {
                sig->threshold_reached = true;
                LOG_INFO("Signal %s reached threshold (%.2f >= %.2f)",
                              signal_names[i], sig->concentration, sig->hysteresis_high);
            } else if (sig->threshold_reached && sig->concentration < sig->hysteresis_low) {
                sig->threshold_reached = false;
                LOG_INFO("Signal %s dropped below threshold (%.2f < %.2f)",
                              signal_names[i], sig->concentration, sig->hysteresis_low);
            }
        } else {
            sig->threshold_reached = (sig->concentration >= sig->threshold);
        }

        sig->last_update_time = nimcp_time_get_us();
    }

    nimcp_platform_mutex_unlock(quorum->mutex);
}

/* ============================================================================
 * Commitment Management
 * ============================================================================ */

bool nimcp_quorum_update_commitment(
    nimcp_swarm_quorum_t* quorum,
    uint32_t drone_id,
    nimcp_signal_type_t signal,
    double strength
) {
    if (!quorum || signal >= NIMCP_SIGNAL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "nimcp_quorum_update_commitment: quorum is NULL");
        return false;
    }

    strength = CLAMP(strength, 0.0, 1.0);

    nimcp_platform_mutex_lock(quorum->mutex);

    /* Find or create commitment */
    nimcp_drone_commitment_t* commit = find_commitment(quorum, drone_id);
    if (!commit) {
        commit = add_commitment(quorum, drone_id);
        if (!commit) {
            nimcp_platform_mutex_unlock(quorum->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_quorum_update_commitment: commit is NULL");
            return false;
        }
    }

    /* Update committed count for old signal */
    if (commit->state >= NIMCP_COMMIT_COMMITTED && commit->signal != signal) {
        if (quorum->signals[commit->signal].committed_count > 0) {
            quorum->signals[commit->signal].committed_count--;
        }
    }

    /* Update commitment */
    nimcp_commitment_state_t old_state = commit->state;
    commit->signal = signal;
    commit->commitment_strength = strength;
    update_commitment_state(commit, strength, &quorum->config);

    /* Update committed count for new signal */
    if (commit->state >= NIMCP_COMMIT_COMMITTED) {
        quorum->signals[signal].committed_count++;
    }

    /* Update statistics */
    if (old_state != commit->state) {
        quorum->stats.total_commitments++;
    }

    nimcp_platform_mutex_unlock(quorum->mutex);

    /* Bio-async commitment broadcast - stubbed for now (requires brain integration) */
    (void)commit;

    LOG_DEBUG("Drone %u commitment updated: signal=%s, state=%s, strength=%.2f",
                    drone_id, signal_names[signal],
                    commitment_names[commit->state], strength);

    return true;
}

const nimcp_drone_commitment_t* nimcp_quorum_get_commitment(
    const nimcp_swarm_quorum_t* quorum,
    uint32_t drone_id
) {
    if (!quorum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_quorum_get_commitment: quorum is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < quorum->commitment_count; i++) {
        if (quorum->commitments[i].drone_id == drone_id) {
            return &quorum->commitments[i];
        }
    }

    return NULL;  /* Normal search miss - drone not found */
}

bool nimcp_quorum_remove_commitment(
    nimcp_swarm_quorum_t* quorum,
    uint32_t drone_id
) {
    if (!quorum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_quorum_remove_commitment: quorum is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(quorum->mutex);

    for (uint32_t i = 0; i < quorum->commitment_count; i++) {
        if (quorum->commitments[i].drone_id == drone_id) {
            nimcp_drone_commitment_t* commit = &quorum->commitments[i];

            /* Update committed count */
            if (commit->state >= NIMCP_COMMIT_COMMITTED) {
                if (quorum->signals[commit->signal].committed_count > 0) {
                    quorum->signals[commit->signal].committed_count--;
                }
            }

            /* Remove by swapping with last element */
            if (i < quorum->commitment_count - 1) {
                quorum->commitments[i] = quorum->commitments[quorum->commitment_count - 1];
            }
            quorum->commitment_count--;

            nimcp_platform_mutex_unlock(quorum->mutex);
            LOG_DEBUG("Removed commitment for drone %u", drone_id);
            return true;
        }
    }

    nimcp_platform_mutex_unlock(quorum->mutex);
    LOG_DEBUG("nimcp_quorum_remove_commitment: drone %u not found", drone_id);
    return false;  /* Normal search miss - drone not found */
}

/**
 * @brief Internal unlocked cascade helper (caller must hold mutex)
 */
static uint32_t trigger_cascade_unlocked(
    nimcp_swarm_quorum_t* quorum,
    nimcp_signal_type_t signal
) {
    uint32_t recruited = 0;
    double cascade_strength = quorum->config.cascade_speed;

    /* Recruit uncommitted and leaning drones */
    for (uint32_t i = 0; i < quorum->commitment_count; i++) {
        nimcp_drone_commitment_t* commit = &quorum->commitments[i];

        if (commit->state < NIMCP_COMMIT_COMMITTED) {
            /* Increase commitment strength */
            commit->commitment_strength += cascade_strength;
            commit->signal = signal;
            update_commitment_state(commit, commit->commitment_strength, &quorum->config);

            if (commit->state >= NIMCP_COMMIT_COMMITTED) {
                recruited++;
                quorum->signals[signal].committed_count++;
            }
        }
    }

    if (recruited > 0) {
        quorum->stats.cascade_events++;
        LOG_INFO("Cascade for signal %s recruited %u drones",
                      signal_names[signal], recruited);
    }

    return recruited;
}

uint32_t nimcp_quorum_trigger_cascade(
    nimcp_swarm_quorum_t* quorum,
    nimcp_signal_type_t signal
) {
    if (!quorum || signal >= NIMCP_SIGNAL_COUNT) {
        return 0;
    }

    nimcp_platform_mutex_lock(quorum->mutex);
    uint32_t recruited = trigger_cascade_unlocked(quorum, signal);
    nimcp_platform_mutex_unlock(quorum->mutex);

    return recruited;
}

/* ============================================================================
 * Threshold and Decision Logic
 * ============================================================================ */

bool nimcp_quorum_check_threshold(
    const nimcp_swarm_quorum_t* quorum,
    nimcp_signal_type_t signal
) {
    if (!quorum || signal >= NIMCP_SIGNAL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "nimcp_quorum_check_threshold: quorum is NULL");
        return false;
    }

    const nimcp_signal_molecule_t* sig = &quorum->signals[signal];

    /* Check both concentration threshold and minimum quorum size */
    return sig->threshold_reached &&
           sig->committed_count >= quorum->config.min_quorum_size;
}

/**
 * @brief Internal unlocked cross-inhibition helper (caller must hold mutex)
 */
static void apply_cross_inhibition_unlocked(
    nimcp_swarm_quorum_t* quorum,
    nimcp_signal_type_t winning_signal
) {
    if (!quorum->config.enable_cross_inhibition) {
        return;
    }

    double inhibition = quorum->config.inhibition_strength;

    /* Suppress all other signals */
    for (int i = 0; i < NIMCP_SIGNAL_COUNT; i++) {
        if (i != winning_signal) {
            quorum->signals[i].concentration *= (1.0 - inhibition);
            LOG_DEBUG("Cross-inhibition: %s suppressed by %s (%.2f)",
                          signal_names[i], signal_names[winning_signal],
                          quorum->signals[i].concentration);
        }
    }
}

void nimcp_quorum_apply_cross_inhibition(
    nimcp_swarm_quorum_t* quorum,
    nimcp_signal_type_t winning_signal
) {
    if (!quorum || winning_signal >= NIMCP_SIGNAL_COUNT) {
        return;
    }

    nimcp_platform_mutex_lock(quorum->mutex);
    apply_cross_inhibition_unlocked(quorum, winning_signal);
    nimcp_platform_mutex_unlock(quorum->mutex);
}

bool nimcp_quorum_make_decision(
    nimcp_swarm_quorum_t* quorum,
    nimcp_decision_type_t decision_type,
    void* decision_data
) {
    if (!quorum || decision_type >= NIMCP_DECISION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "nimcp_quorum_make_decision: quorum is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(quorum->mutex);

    /* Find signal that reached threshold */
    nimcp_signal_type_t winning_signal = NIMCP_SIGNAL_COUNT;
    double max_concentration = 0.0;

    for (int i = 0; i < NIMCP_SIGNAL_COUNT; i++) {
        if (nimcp_quorum_check_threshold(quorum, i)) {
            if (quorum->signals[i].concentration > max_concentration) {
                max_concentration = quorum->signals[i].concentration;
                winning_signal = i;
            }
        }
    }

    if (winning_signal == NIMCP_SIGNAL_COUNT) {
        /* No quorum reached - normal condition */
        quorum->stats.failed_quorums++;
        nimcp_platform_mutex_unlock(quorum->mutex);
        LOG_DEBUG("No quorum reached for decision type %s",
                       decision_names[decision_type]);
        return false;
    }

    /* Calculate consensus strength */
    double consensus = nimcp_quorum_get_consensus_strength(quorum);

    /* Count participating drones */
    uint32_t participating = quorum->commitment_count;
    uint32_t committed = quorum->signals[winning_signal].committed_count;

    /* Add decision record */
    bool success = add_decision(
        quorum,
        decision_type,
        winning_signal,
        consensus,
        participating,
        committed,
        decision_data
    );

    if (success) {
        quorum->stats.total_decisions++;
        quorum->stats.successful_quorums++;

        /* Update statistics */
        if (committed > quorum->stats.max_committed_drones) {
            quorum->stats.max_committed_drones = committed;
        }
        if (quorum->stats.min_committed_drones == 0 ||
            committed < quorum->stats.min_committed_drones) {
            quorum->stats.min_committed_drones = committed;
        }

        /* Apply cross-inhibition (unlocked - we already hold the mutex) */
        apply_cross_inhibition_unlocked(quorum, winning_signal);

        /* Trigger cascade for winning signal (unlocked - we already hold the mutex) */
        trigger_cascade_unlocked(quorum, winning_signal);

        LOG_INFO("Decision made: type=%s, signal=%s, consensus=%.2f, committed=%u",
                      decision_names[decision_type], signal_names[winning_signal],
                      consensus, committed);
    }

    nimcp_platform_mutex_unlock(quorum->mutex);

    /* Bio-async decision broadcast - stubbed for now (requires brain integration) */

    return success;
}

const nimcp_quorum_decision_t* nimcp_quorum_get_last_decision(
    const nimcp_swarm_quorum_t* quorum
) {
    if (!quorum || quorum->decision_count == 0) {
        return NULL;  /* No decisions yet - normal condition */
    }

    return &quorum->decisions[quorum->decision_count - 1];
}

bool nimcp_quorum_finalize_decision(
    nimcp_swarm_quorum_t* quorum,
    uint32_t decision_index
) {
    if (!quorum || decision_index >= quorum->decision_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "nimcp_quorum_finalize_decision: quorum is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(quorum->mutex);
    quorum->decisions[decision_index].is_final = true;
    nimcp_platform_mutex_unlock(quorum->mutex);

    LOG_INFO("Decision %u finalized", decision_index);
    return true;
}

/* ============================================================================
 * Positive Feedback
 * ============================================================================ */

double nimcp_quorum_apply_positive_feedback(
    nimcp_swarm_quorum_t* quorum,
    nimcp_signal_type_t signal
) {
    if (!quorum || signal >= NIMCP_SIGNAL_COUNT) {
        return 0.0;
    }

    if (!quorum->config.enable_positive_feedback) {
        return quorum->signals[signal].concentration;
    }

    nimcp_platform_mutex_lock(quorum->mutex);

    nimcp_signal_molecule_t* sig = &quorum->signals[signal];

    /* Amplify based on committed drones */
    if (sig->committed_count > 0) {
        double amplification = 1.0 + (sig->committed_count * sig->amplification / 10.0);
        sig->concentration = MIN(1.0, sig->concentration * amplification);
    }

    double result = sig->concentration;
    nimcp_platform_mutex_unlock(quorum->mutex);

    return result;
}

uint32_t nimcp_quorum_recruit_drones(
    nimcp_swarm_quorum_t* quorum,
    nimcp_signal_type_t signal,
    uint32_t recruiter_id
) {
    if (!quorum || signal >= NIMCP_SIGNAL_COUNT) {
        return 0;
    }

    nimcp_platform_mutex_lock(quorum->mutex);

    /* Find recruiter */
    nimcp_drone_commitment_t* recruiter = find_commitment(quorum, recruiter_id);
    if (!recruiter || recruiter->state < NIMCP_COMMIT_COMMITTED) {
        nimcp_platform_mutex_unlock(quorum->mutex);
        return 0;
    }

    uint32_t recruited = 0;
    double recruitment_strength = 0.3; /* Strength added to uncommitted drones */

    /* Recruit uncommitted drones */
    for (uint32_t i = 0; i < quorum->commitment_count; i++) {
        nimcp_drone_commitment_t* commit = &quorum->commitments[i];

        if (commit->state == NIMCP_COMMIT_UNCOMMITTED ||
            commit->state == NIMCP_COMMIT_LEANING) {
            commit->commitment_strength += recruitment_strength;
            commit->signal = signal;
            update_commitment_state(commit, commit->commitment_strength, &quorum->config);

            if (commit->state >= NIMCP_COMMIT_COMMITTED) {
                recruited++;
                recruiter->recruitment_count++;
            }
        }
    }

    nimcp_platform_mutex_unlock(quorum->mutex);

    if (recruited > 0) {
        LOG_DEBUG("Drone %u recruited %u drones to signal %s",
                       recruiter_id, recruited, signal_names[signal]);
    }

    return recruited;
}

/* ============================================================================
 * Query and Statistics
 * ============================================================================ */

double nimcp_quorum_get_signal_concentration(
    const nimcp_swarm_quorum_t* quorum,
    nimcp_signal_type_t signal
) {
    if (!quorum || signal >= NIMCP_SIGNAL_COUNT) {
        return 0.0;
    }

    return quorum->signals[signal].concentration;
}

uint32_t nimcp_quorum_get_committed_count(
    const nimcp_swarm_quorum_t* quorum,
    nimcp_signal_type_t signal
) {
    if (!quorum || signal >= NIMCP_SIGNAL_COUNT) {
        return 0;
    }

    return quorum->signals[signal].committed_count;
}

double nimcp_quorum_get_consensus_strength(
    const nimcp_swarm_quorum_t* quorum
) {
    if (!quorum || quorum->commitment_count == 0) {
        return 0.0;
    }

    /* Find dominant signal */
    uint32_t max_committed = 0;
    for (int i = 0; i < NIMCP_SIGNAL_COUNT; i++) {
        if (quorum->signals[i].committed_count > max_committed) {
            max_committed = quorum->signals[i].committed_count;
        }
    }

    /* Consensus = fraction of drones committed to dominant signal */
    return (double)max_committed / (double)quorum->commitment_count;
}

const nimcp_quorum_stats_t* nimcp_quorum_get_stats(
    const nimcp_swarm_quorum_t* quorum
) {
    return quorum ? &quorum->stats : NULL;
}

void nimcp_quorum_reset_stats(nimcp_swarm_quorum_t* quorum) {
    if (!quorum) return;

    nimcp_platform_mutex_lock(quorum->mutex);
    memset(&quorum->stats, 0, sizeof(nimcp_quorum_stats_t));
    nimcp_platform_mutex_unlock(quorum->mutex);

    LOG_INFO("Quorum statistics reset");
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

bool nimcp_quorum_handle_message(
    nimcp_swarm_quorum_t* quorum,
    const bio_message_header_t* msg
) {
    if (!quorum || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_quorum_handle_message: required parameter is NULL (quorum, msg)");
        return false;
    }

    /* Message handling stubbed - requires custom message type integration */
    /* When integrated with bio-async, would parse based on message type */
    (void)msg->type;  /* Suppress unused warning */

    LOG_DEBUG("nimcp_quorum_handle_message: message type 0x%x not handled (stub)", msg->type);
    return false;  /* Not yet implemented */
}

bool nimcp_quorum_register_handlers(
    nimcp_swarm_quorum_t* quorum,
    struct nimcp_brain* brain
) {
    if (!quorum || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_quorum_register_handlers: required parameter is NULL (quorum, brain)");
        return false;
    }

    quorum->brain = brain;

    /* Register with bio-async system */
    /* Note: Actual registration would depend on brain's message routing */

    LOG_INFO("Quorum system registered with brain");
    return true;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* nimcp_quorum_signal_name(nimcp_signal_type_t signal) {
    if (signal >= NIMCP_SIGNAL_COUNT) {
        return "UNKNOWN";
    }
    return signal_names[signal];
}

const char* nimcp_quorum_decision_name(nimcp_decision_type_t decision) {
    if (decision >= NIMCP_DECISION_COUNT) {
        return "UNKNOWN";
    }
    return decision_names[decision];
}

const char* nimcp_quorum_commitment_name(nimcp_commitment_state_t state) {
    if (state > NIMCP_COMMIT_AMPLIFYING) {
        return "UNKNOWN";
    }
    return commitment_names[state];
}

void nimcp_quorum_print_state(const nimcp_swarm_quorum_t* quorum) {
    if (!quorum) return;

    printf("\n=== Quorum Sensing System State ===\n");
    printf("Active: %s\n", quorum->is_active ? "Yes" : "No");
    printf("Total Commitments: %u\n", quorum->commitment_count);
    printf("Total Decisions: %u\n", quorum->decision_count);

    printf("\nSignal Concentrations:\n");
    for (int i = 0; i < NIMCP_SIGNAL_COUNT; i++) {
        const nimcp_signal_molecule_t* sig = &quorum->signals[i];
        printf("  %s: %.3f (threshold: %.2f, committed: %u) %s\n",
               signal_names[i],
               sig->concentration,
               sig->threshold,
               sig->committed_count,
               sig->threshold_reached ? "[REACHED]" : "");
    }

    printf("\nCommitments:\n");
    for (uint32_t i = 0; i < quorum->commitment_count; i++) {
        const nimcp_drone_commitment_t* commit = &quorum->commitments[i];
        printf("  Drone %u: %s -> %s (%.2f) %s\n",
               commit->drone_id,
               signal_names[commit->signal],
               commitment_names[commit->state],
               commit->commitment_strength,
               commit->is_amplifying ? "[AMPLIFYING]" : "");
    }

    printf("\nStatistics:\n");
    printf("  Total Decisions: %lu\n", quorum->stats.total_decisions);
    printf("  Successful Quorums: %lu\n", quorum->stats.successful_quorums);
    printf("  Failed Quorums: %lu\n", quorum->stats.failed_quorums);
    printf("  Cascade Events: %lu\n", quorum->stats.cascade_events);
    printf("  Total Signals Broadcast: %lu\n", quorum->stats.total_signals_broadcast);
    printf("  Total Commitments: %lu\n", quorum->stats.total_commitments);

    printf("\n");
}

/* ============================================================================
 * Logic Validation Implementation
 * ============================================================================ */

/**
 * @brief Get default logic validation configuration
 *
 * WHAT: Provides sensible defaults for logic validation
 * WHY:  Simplify usage of logic validation features
 * HOW:  Sets majority voting with consistency checks
 */
void quorum_logic_default_config(quorum_logic_config_t* config) {
    if (!config) return;

    config->gate_type = LOGIC_GATE_AND;  /* Default to unanimous */
    config->threshold = 0.5F;             /* Majority threshold */
    config->require_consistency = true;   /* Check for contradictions */
    config->min_agents = 3;               /* Minimum 3 agents */
    config->confidence_threshold = 0.7F;  /* 70% confidence required */
}

/**
 * @brief Validate quorum decision using logic gates
 *
 * WHAT: Validates quorum decisions through neural logic gate evaluation
 * WHY:  Ensures logical consistency and correctness of distributed decisions
 * HOW:  Uses AND/OR/XOR/IMPLIES gates to verify voting patterns
 */
int quorum_validate_with_logic(
    nimcp_swarm_quorum_t* quorum,
    const quorum_logic_config_t* logic_cfg
) {
    if (!quorum || !logic_cfg) {
        LOG_ERROR("Invalid parameters for logic validation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quorum_validate_with_logic: required parameter is NULL (quorum, logic_cfg)");
        return -1;
    }

    /* Guard clause: Check minimum agents */
    if (quorum->commitment_count < logic_cfg->min_agents) {
        LOG_WARN("Insufficient agents for validation: %u < %u",
                 quorum->commitment_count, logic_cfg->min_agents);
        return 0;
    }

    nimcp_platform_mutex_lock(quorum->mutex);

    /* Find winning signal */
    nimcp_signal_type_t winning_signal = NIMCP_SIGNAL_COUNT;
    uint32_t max_committed = 0;

    for (int i = 0; i < NIMCP_SIGNAL_COUNT; i++) {
        if (quorum->signals[i].committed_count > max_committed) {
            max_committed = quorum->signals[i].committed_count;
            winning_signal = i;
        }
    }

    if (winning_signal == NIMCP_SIGNAL_COUNT) {
        nimcp_platform_mutex_unlock(quorum->mutex);
        LOG_DEBUG("No winning signal found for validation");
        return 0;
    }

    /* Calculate vote fractions for logic evaluation */
    float winning_fraction = (float)max_committed / (float)quorum->commitment_count;
    float confidence = quorum->signals[winning_signal].concentration;

    /* Guard clause: Check confidence threshold */
    if (confidence < logic_cfg->confidence_threshold) {
        nimcp_platform_mutex_unlock(quorum->mutex);
        LOG_WARN("Confidence below threshold: %.2f < %.2f",
                 confidence, logic_cfg->confidence_threshold);
        return 0;
    }

    int validation_result = 0;

    /* Evaluate based on gate type */
    switch (logic_cfg->gate_type) {
        case LOGIC_GATE_AND:
            /* Unanimous: All agents must agree */
            validation_result = (winning_fraction >= 0.99F) ? 1 : 0;
            LOG_DEBUG("AND validation: fraction=%.2f, result=%d",
                     winning_fraction, validation_result);
            break;

        case LOGIC_GATE_OR:
            /* Permissive: Any significant support passes */
            validation_result = (winning_fraction > 0.0F) ? 1 : 0;
            LOG_DEBUG("OR validation: fraction=%.2f, result=%d",
                     winning_fraction, validation_result);
            break;

        case LOGIC_GATE_XOR:
            /* Exclusive: Exactly one clear winner, no ties */
            {
                uint32_t second_max = 0;
                for (int i = 0; i < NIMCP_SIGNAL_COUNT; i++) {
                    if (i != winning_signal &&
                        quorum->signals[i].committed_count > second_max) {
                        second_max = quorum->signals[i].committed_count;
                    }
                }
                /* Clear winner with no close second */
                validation_result = (max_committed > second_max * 2) ? 1 : 0;
                LOG_DEBUG("XOR validation: max=%u, second=%u, result=%d",
                         max_committed, second_max, validation_result);
            }
            break;

        case LOGIC_GATE_IMPLIES:
            /* Threshold-based: If threshold met, then consensus required */
            if (confidence >= logic_cfg->threshold) {
                /* Antecedent true: consequent must be true */
                validation_result = (winning_fraction >= logic_cfg->threshold) ? 1 : 0;
            } else {
                /* Antecedent false: implication vacuously true */
                validation_result = 1;
            }
            LOG_DEBUG("IMPLIES validation: confidence=%.2f, threshold=%.2f, result=%d",
                     confidence, logic_cfg->threshold, validation_result);
            break;

        default:
            /* Fallback: Simple threshold */
            validation_result = (winning_fraction >= logic_cfg->threshold) ? 1 : 0;
            LOG_DEBUG("Threshold validation: fraction=%.2f, threshold=%.2f, result=%d",
                     winning_fraction, logic_cfg->threshold, validation_result);
            break;
    }

    /* Optional consistency check using XOR */
    if (logic_cfg->require_consistency && validation_result == 1) {
        uint32_t contradicting_agents[256];
        uint32_t contradiction_count = 0;

        /* Use unlocked version since we already hold quorum->mutex */
        int consistency = check_vote_consistency_unlocked(
            quorum,
            contradicting_agents,
            &contradiction_count
        );

        if (consistency == 1) {
            /* Contradictions found */
            validation_result = 0;
            LOG_WARN("Validation failed: %u contradicting agents detected",
                     contradiction_count);
        }
    }

    nimcp_platform_mutex_unlock(quorum->mutex);

    LOG_INFO("Logic validation complete: signal=%s, result=%s",
             signal_names[winning_signal],
             validation_result ? "PASS" : "FAIL");

    return validation_result;
}

/**
 * @brief Internal unlocked helper for vote consistency check
 *
 * WHAT: Core logic for detecting contradicting votes (no locking)
 * WHY:  Allows use from already-locked contexts to prevent deadlock
 * HOW:  Uses XOR gate to detect mutually exclusive votes
 *
 * REQUIRES: Caller must hold quorum->mutex
 */
static int check_vote_consistency_unlocked(
    nimcp_swarm_quorum_t* quorum,
    uint32_t* contradicting_agents,
    uint32_t* count
) {
    *count = 0;

    /* Define mutually exclusive signal pairs (XOR constraints) */
    struct {
        nimcp_signal_type_t signal_a;
        nimcp_signal_type_t signal_b;
    } exclusive_pairs[] = {
        {NIMCP_SIGNAL_ATTACK, NIMCP_SIGNAL_RETREAT},
        {NIMCP_SIGNAL_ATTACK, NIMCP_SIGNAL_DEFEND},
        {NIMCP_SIGNAL_EXPLORE, NIMCP_SIGNAL_DEFEND},
    };
    const int num_pairs = sizeof(exclusive_pairs) / sizeof(exclusive_pairs[0]);

    /* Check each drone's commitments for contradictions */
    for (uint32_t i = 0; i < quorum->commitment_count; i++) {
        nimcp_drone_commitment_t* commit = &quorum->commitments[i];

        /* Check against exclusive pairs */
        for (int p = 0; p < num_pairs; p++) {
            /* Check if this drone is committed to an exclusive signal */
            if (commit->signal == exclusive_pairs[p].signal_a) {
                /* Check if opposing signal also has support from this or nearby drones */
                uint32_t opposing_count = quorum->signals[exclusive_pairs[p].signal_b].committed_count;

                if (opposing_count > 0) {
                    /* Contradiction: both exclusive signals have support */
                    if (*count < 256) {
                        contradicting_agents[*count] = commit->drone_id;
                        (*count)++;
                    }
                    LOG_DEBUG("Contradiction detected: drone %u supports %s while %s also has support",
                             commit->drone_id,
                             signal_names[exclusive_pairs[p].signal_a],
                             signal_names[exclusive_pairs[p].signal_b]);
                }
            }
        }
    }

    int result = (*count > 0) ? 1 : 0;

    if (result == 1) {
        LOG_WARN("Consistency check found %u contradicting agents", *count);
    } else {
        LOG_DEBUG("Consistency check passed: no contradictions found");
    }

    return result;
}

/**
 * @brief Check vote consistency using XOR logic
 *
 * WHAT: Detects contradicting votes among agents
 * WHY:  Identifies Byzantine behavior or conflicting opinions
 * HOW:  Uses XOR gate to detect mutually exclusive votes
 */
int quorum_check_vote_consistency(
    nimcp_swarm_quorum_t* quorum,
    uint32_t* contradicting_agents,
    uint32_t* count
) {
    if (!quorum || !contradicting_agents || !count) {
        LOG_ERROR("Invalid parameters for consistency check");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quorum_check_vote_consistency: required parameter is NULL (quorum, contradicting_agents, count)");
        return -1;
    }

    nimcp_platform_mutex_lock(quorum->mutex);
    int result = check_vote_consistency_unlocked(quorum, contradicting_agents, count);
    nimcp_platform_mutex_unlock(quorum->mutex);

    return result;
}

/**
 * @brief Evaluate quorum using IMPLIES logic gate
 *
 * WHAT: Conditional consensus validation (if A then B)
 * WHY:  Enforce logical rules on decision-making
 * HOW:  Uses IMPLIES gate to check antecedent → consequent
 */
int quorum_evaluate_implication(
    nimcp_swarm_quorum_t* quorum,
    nimcp_signal_type_t antecedent_signal,
    nimcp_signal_type_t consequent_signal,
    bool* implication_holds
) {
    if (!quorum || !implication_holds ||
        antecedent_signal >= NIMCP_SIGNAL_COUNT ||
        consequent_signal >= NIMCP_SIGNAL_COUNT) {
        LOG_ERROR("Invalid parameters for implication evaluation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "quorum_evaluate_implication: operation failed");
        return -1;
    }

    nimcp_platform_mutex_lock(quorum->mutex);

    /* Get signal concentrations */
    float antecedent_concentration = quorum->signals[antecedent_signal].concentration;
    float consequent_concentration = quorum->signals[consequent_signal].concentration;

    /* IMPLIES logic: A → B = ¬A ∨ B */
    /* Implication is false only when A is true and B is false */

    float antecedent_threshold = 0.5F;
    float consequent_threshold = 0.5F;

    bool antecedent_active = (antecedent_concentration >= antecedent_threshold);
    bool consequent_active = (consequent_concentration >= consequent_threshold);

    /* Evaluate implication */
    if (antecedent_active && !consequent_active) {
        /* Antecedent true, consequent false: implication fails */
        *implication_holds = false;
        LOG_WARN("Implication FAILED: %s (%.2f) → %s (%.2f)",
                 signal_names[antecedent_signal], antecedent_concentration,
                 signal_names[consequent_signal], consequent_concentration);
    } else {
        /* All other cases: implication holds */
        *implication_holds = true;
        LOG_DEBUG("Implication HOLDS: %s (%.2f) → %s (%.2f)",
                  signal_names[antecedent_signal], antecedent_concentration,
                  signal_names[consequent_signal], consequent_concentration);
    }

    nimcp_platform_mutex_unlock(quorum->mutex);

    return 0;
}
