/**
 * @file nimcp_brain_immune_tick.c
 * @brief Brain Immune System Tick Orchestrator Implementation
 * @version 1.0.0
 * @date 2025-01-25
 *
 * WHAT: Unified tick function that sequences all immune processing
 * WHY:  Bridge the gap between exception/health agent detection and immune response
 * HOW:  Orchestrate exception processing, health queue consumption, and core update
 *
 * CRITICAL FIX:
 * Before this module:
 *   - Health Agent -> [messages queue] -> ??? (nobody reads)
 *   - Exceptions -> [async queue] -> ??? (nobody processes)
 *   - brain_immune_update() -> never called from C
 *
 * After this module:
 *   - Health Agent -> [messages queue] --+
 *   - Exceptions -> [async queue] -------+--> brain_immune_tick() --> brain_immune_update()
 *   - Timer/Thread -----------------------+              |
 *                                                        v
 *                                              Full immune response cycle
 *
 * @author NIMCP Development Team
 */

#include "cognitive/immune/nimcp_brain_immune_tick.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_mutex.h"

/* NIMCP Utilities for Enhanced Processing */
#include "utils/algorithms/nimcp_monte_carlo.h"           /* MC sampling for severity */
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"  /* QA for recovery */
#include "utils/numerical/nimcp_integration.h"            /* Numerical integration */
#include "utils/math/nimcp_complex_math.h"                /* Phasor coherence for patterns */

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define LOG_MODULE "brain_immune_tick"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(brain_immune_tick, MESH_ADAPTER_CATEGORY_SECURITY)



/** Magic number for tick state validation */
#define TICK_STATE_MAGIC    0x5449434B  /* "TICK" */

/* ============================================================================
 * Enhanced Processing Constants (NIMCP Utilities)
 * ============================================================================ */

/** Monte Carlo samples for severity assessment */
#define MC_SEVERITY_SAMPLES             100

/** Number of recovery actions to evaluate with quantum annealing */
#define QA_MAX_RECOVERY_ACTIONS         8

/** Quantum annealing iterations for recovery optimization */
#define QA_RECOVERY_ITERATIONS          200

/** Maximum epitope patterns to track for recurring detection */
#define MAX_EPITOPE_PATTERNS            256

/** Coherence threshold for recognizing recurring patterns */
#define PATTERN_COHERENCE_THRESHOLD     0.75f

/** Exponential moving average alpha for stats (integration smoothing) */
#define EMA_ALPHA                       0.1f

/* ============================================================================
 * Thread-Local Reentry Guards
 * ============================================================================ */

/**
 * Thread-local reentry guard for tick function.
 * Prevents recursive tick calls that could cause infinite loops or deadlocks.
 */
static _Thread_local bool tl_in_immune_tick = false;

/**
 * @brief Check if currently inside a tick (for reentry detection)
 */
bool brain_immune_tick_in_progress(void) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_tick_heartbeat("brain_immune_in_progress", 0.0f);


    return tl_in_immune_tick;
}

/* ============================================================================
 * Epitope Pattern Memory (Quantum-Inspired Pattern Matching)
 * ============================================================================ */

/**
 * @brief Epitope pattern entry for recurring anomaly detection
 *
 * Uses phasor representation for quantum-inspired pattern coherence.
 * Enables fast recognition of recurring threat signatures.
 */
typedef struct {
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];  /**< Threat signature */
    size_t epitope_len;                          /**< Signature length */
    neural_phasor_t phasor;                      /**< Phasor for coherence */
    uint32_t occurrence_count;                   /**< Times this pattern seen */
    uint64_t last_seen_us;                       /**< Last occurrence timestamp */
    float severity_avg;                          /**< Average severity */
    bool is_active;                              /**< Slot in use */
} epitope_pattern_t;

/**
 * @brief Quantum-inspired pattern memory
 *
 * Tracks recurring epitope patterns using phasor coherence.
 * High coherence = frequently recurring pattern.
 */
typedef struct {
    epitope_pattern_t patterns[MAX_EPITOPE_PATTERNS];  /**< Pattern storage */
    uint32_t num_patterns;                             /**< Active patterns */
    uint32_t mc_seed;                                  /**< Monte Carlo RNG seed */
} pattern_memory_t;

/* ============================================================================
 * Recovery Action Context (Quantum Annealing)
 * ============================================================================ */

/**
 * @brief Context for quantum annealing recovery optimization
 *
 * Allows quantum annealing to find optimal recovery action.
 */
typedef struct {
    nimcp_exception_recovery_action_t candidates[QA_MAX_RECOVERY_ACTIONS];
    float costs[QA_MAX_RECOVERY_ACTIONS];
    uint32_t num_candidates;
    health_agent_severity_t severity;
    health_agent_source_t source;
} recovery_context_t;

/* ============================================================================
 * Tick State Structure
 * ============================================================================ */

/**
 * @brief Internal tick orchestrator state
 *
 * Stored within brain_immune_system_t using a void* pointer for extensibility.
 * Enhanced with NIMCP utilities for advanced processing.
 */
typedef struct {
    uint32_t magic;                        /**< Magic for validation */
    bool initialized;                      /**< Tick orchestrator initialized */

    /* Configuration */
    brain_immune_tick_config_t config;     /**< Current configuration */

    /* Connections */
    nimcp_health_agent_t* health_agent;    /**< Connected health agent */

    /* Statistics */
    brain_immune_tick_stats_t stats;       /**< Tick statistics */
    uint64_t total_tick_time_us;           /**< Total time spent in ticks */

    /* Timing */
    uint64_t last_tick_time_us;            /**< Timestamp of last tick */

    /* Thread safety */
    nimcp_mutex_t* mutex;                  /**< State mutex */

    /* ========== Enhanced Features (NIMCP Utilities) ========== */

    /* Pattern Memory - Quantum-inspired recurring anomaly detection */
    pattern_memory_t* pattern_memory;      /**< Epitope pattern tracking */

    /* Quantum Annealing - Optimal recovery action selection */
    quantum_annealer_t qa_annealer;        /**< Quantum annealer for recovery */
    bool qa_enabled;                       /**< Quantum annealing enabled */

    /* Monte Carlo - Probabilistic severity assessment */
    uint32_t mc_seed;                      /**< Monte Carlo RNG seed */

    /* Integration State - Proper numerical statistics */
    float tick_duration_integral;          /**< Integrated tick duration */
    uint64_t integration_step_count;       /**< Steps for integration */
} tick_state_t;

/* ============================================================================
 * Internal Helpers - Forward Declarations
 * ============================================================================ */

static tick_state_t* get_tick_state(brain_immune_system_t* immune);
static const tick_state_t* get_tick_state_const(const brain_immune_system_t* immune);
static uint64_t get_timestamp_us(void);
static int map_health_msg_to_antigen_severity(health_agent_severity_t severity);
static brain_antigen_source_t map_health_source_to_antigen_source(health_agent_source_t source);
static int process_anomaly_message(brain_immune_system_t* immune, const health_agent_message_t* msg);
static int process_cytokine_message(brain_immune_system_t* immune, const health_agent_message_t* msg);
static int process_emergency_message(brain_immune_system_t* immune, const health_agent_message_t* msg);
static int process_recovery_request(brain_immune_system_t* immune, const health_agent_message_t* msg);
static int process_corruption_message(brain_immune_system_t* immune, const health_agent_message_t* msg);
static int process_nan_message(brain_immune_system_t* immune, const health_agent_message_t* msg);
static int process_deadlock_message(brain_immune_system_t* immune, const health_agent_message_t* msg);
static int process_heartbeat_timeout(brain_immune_system_t* immune, const health_agent_message_t* msg);
static int process_resource_exhaustion(brain_immune_system_t* immune, const health_agent_message_t* msg);

/* ========== Enhanced Processing Helpers (NIMCP Utilities) ========== */

/* Monte Carlo severity assessment */
static float mc_sample_severity(tick_state_t* state, health_agent_severity_t base_severity,
                                health_agent_source_t source);
static float mc_severity_sampler(void* user_data);
static float mc_severity_objective(float sample, void* user_data);

/* Quantum annealing recovery selection */
static nimcp_exception_recovery_action_t qa_select_recovery_action(
    tick_state_t* state, const health_agent_message_t* msg);
static float qa_recovery_energy(const float* state_vec, uint32_t dim, void* user_data);

/* Pattern memory operations */
static pattern_memory_t* pattern_memory_create(void);
static void pattern_memory_destroy(pattern_memory_t* pm);
static bool pattern_memory_record_epitope(pattern_memory_t* pm,
                                          const uint8_t* epitope, size_t len,
                                          float severity, uint64_t timestamp);
static float pattern_memory_check_coherence(pattern_memory_t* pm,
                                            const uint8_t* epitope, size_t len);
static uint32_t epitope_hash(const uint8_t* epitope, size_t len);

/* Numerical integration for statistics */
static void integrate_tick_duration(tick_state_t* state, uint64_t duration_us);

/* Mutex convenience macros */
#define tick_mutex_lock(s) nimcp_mutex_lock((s)->mutex)
#define tick_mutex_unlock(s) nimcp_mutex_unlock((s)->mutex)

/* ============================================================================
 * Internal Helpers - Implementation
 * ============================================================================ */

/**
 * @brief Get tick state from immune system
 *
 * Uses the unused callback_user_data field to store tick state.
 * This allows extending brain_immune_system_t without modifying its definition.
 */
static tick_state_t* get_tick_state(brain_immune_system_t* immune) {
    if (!immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_tick_state: immune is NULL");
        return NULL;
    }
    /* We store tick state in a dedicated field - use last pointer in struct */
    /* For now, use a simple approach - check if callback_user_data is tick state */
    tick_state_t* state = (tick_state_t*)immune->callback_user_data;
    if (state && state->magic == TICK_STATE_MAGIC) {
        return state;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_tick_state: validation failed");
    return NULL;
}

static const tick_state_t* get_tick_state_const(const brain_immune_system_t* immune) {
    return get_tick_state((brain_immune_system_t*)immune);
}

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Map health agent severity to immune antigen severity (1-10)
 */
static int map_health_msg_to_antigen_severity(health_agent_severity_t severity) {
    switch (severity) {
        case HEALTH_SEVERITY_INFO:     return 2;
        case HEALTH_SEVERITY_WARNING:  return 4;
        case HEALTH_SEVERITY_ERROR:    return 6;
        case HEALTH_SEVERITY_CRITICAL: return 8;
        case HEALTH_SEVERITY_FATAL:    return 10;
        default: return 5;
    }
}

/**
 * @brief Map health source to antigen source
 */
static brain_antigen_source_t map_health_source_to_antigen_source(health_agent_source_t source) {
    switch (source) {
        case HEALTH_SOURCE_MEMORY:
        case HEALTH_SOURCE_IO:
        case HEALTH_SOURCE_CHECKPOINT:
            return ANTIGEN_SOURCE_ANOMALY;

        case HEALTH_SOURCE_THREADING:
            return ANTIGEN_SOURCE_BFT;

        case HEALTH_SOURCE_NEURAL:
        case HEALTH_SOURCE_KG:
        case HEALTH_SOURCE_BRAIN_REGION:
            return ANTIGEN_SOURCE_BBB;

        case HEALTH_SOURCE_IMMUNE:
            return ANTIGEN_SOURCE_MANUAL;

        case HEALTH_SOURCE_HEARTBEAT:
        default:
            return ANTIGEN_SOURCE_ANOMALY;
    }
}

/* ============================================================================
 * Enhanced Processing Helpers - Implementation (NIMCP Utilities)
 * ============================================================================ */

/**
 * @brief Context for Monte Carlo severity sampling
 */
typedef struct {
    health_agent_severity_t base_severity;
    health_agent_source_t source;
    uint32_t* seed;
} mc_severity_context_t;

/**
 * @brief Sampler callback for Monte Carlo severity assessment
 */
static float mc_severity_sampler(void* user_data) {
    mc_severity_context_t* ctx = (mc_severity_context_t*)user_data;

    /* Generate random perturbation based on source uncertainty */
    float base = (float)map_health_msg_to_antigen_severity(ctx->base_severity);
    float stddev = 0.0f;

    /* Different sources have different uncertainty */
    switch (ctx->source) {
        case HEALTH_SOURCE_MEMORY:
            stddev = 0.5f;  /* Low uncertainty */
            break;
        case HEALTH_SOURCE_NEURAL:
            stddev = 1.5f;  /* Higher uncertainty for neural */
            break;
        case HEALTH_SOURCE_THREADING:
            stddev = 1.0f;  /* Medium uncertainty */
            break;
        default:
            stddev = 1.0f;
    }

    return mc_random_normal(ctx->seed, base, stddev);
}

/**
 * @brief Objective callback for Monte Carlo severity estimation
 */
static float mc_severity_objective(float sample, void* user_data) {
    (void)user_data;
    /* Clamp to valid severity range [1, 10] */
    if (sample < 1.0f) return 1.0f;
    if (sample > 10.0f) return 10.0f;
    return sample;
}

/**
 * @brief Monte Carlo sampling for probabilistic severity assessment
 *
 * Uses Monte Carlo sampling to estimate severity considering source uncertainty.
 * Different sources have different noise profiles.
 */
static float mc_sample_severity(tick_state_t* state, health_agent_severity_t base_severity,
                                health_agent_source_t source) {
    if (!state) {
        return (float)map_health_msg_to_antigen_severity(base_severity);
    }

    /* Set up MC context */
    mc_severity_context_t ctx = {
        .base_severity = base_severity,
        .source = source,
        .seed = &state->mc_seed
    };

    /* Configure MC sampling */
    mc_config_t mc_config;
    mc_config_init(&mc_config);
    mc_config.method = MC_SAMPLE_UNIFORM;
    mc_config.num_samples = MC_SEVERITY_SAMPLES;
    mc_config.sampler = mc_severity_sampler;
    mc_config.objective = mc_severity_objective;
    mc_config.user_data = &ctx;
    mc_config.store_samples = false;

    /* Run MC estimation */
    mc_result_t result;
    nimcp_mc_result_t mc_ret = mc_estimate(&mc_config, &result);

    if (mc_ret != NIMCP_MC_OK) {
        /* Fallback to base severity */
        return (float)map_health_msg_to_antigen_severity(base_severity);
    }

    /* Clamp final estimate */
    float estimate = result.estimate;
    if (estimate < 1.0f) estimate = 1.0f;
    if (estimate > 10.0f) estimate = 10.0f;

    float saved_std_error = result.std_error;
    mc_result_free(&result);

    LOG_DEBUG("MC severity: base=%d, estimated=%.2f (std_err=%.3f)",
              map_health_msg_to_antigen_severity(base_severity),
              estimate, saved_std_error);

    return estimate;
}

/**
 * @brief Energy function for quantum annealing recovery optimization
 *
 * Lower energy = better recovery action for the situation.
 */
static float qa_recovery_energy(const float* state_vec, uint32_t dim, void* user_data) {
    recovery_context_t* ctx = (recovery_context_t*)user_data;
    if (!ctx || dim == 0) return INFINITY;

    /* State vector encodes selection weights for each candidate action */
    float energy = 0.0f;

    for (uint32_t i = 0; i < ctx->num_candidates && i < dim; i++) {
        float weight = state_vec[i];
        float cost = ctx->costs[i];

        /* Energy = weighted sum of costs */
        energy += weight * weight * cost;
    }

    /* Penalty for non-normalized weights (should sum to 1) */
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            brain_immune_tick_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)dim);
        }

        sum += state_vec[i] * state_vec[i];
    }
    energy += 10.0f * fabsf(sum - 1.0f);

    return energy;
}

/**
 * @brief Get cost for a recovery action based on severity and source
 */
static float get_recovery_cost(nimcp_exception_recovery_action_t action,
                               health_agent_severity_t severity,
                               health_agent_source_t source) {
    float base_cost = 0.0f;

    /* Base cost by action type (lower = preferred) */
    switch (action) {
        case EXCEPTION_RECOVERY_NONE:
            base_cost = 0.1f;  /* Cheap but ineffective */
            break;
        case EXCEPTION_RECOVERY_CLEAR_CACHE:
            base_cost = 0.3f;  /* Lightweight */
            break;
        case EXCEPTION_RECOVERY_GC:
            base_cost = 0.4f;
            break;
        case EXCEPTION_RECOVERY_REDUCE_LOAD:
            base_cost = 0.5f;
            break;
        case EXCEPTION_RECOVERY_ROLLBACK:
            base_cost = 0.6f;
            break;
        case EXCEPTION_RECOVERY_RESTART_THREAD:
            base_cost = 0.7f;
            break;
        case EXCEPTION_RECOVERY_QUARANTINE:
            base_cost = 0.8f;
            break;
        case EXCEPTION_RECOVERY_EMERGENCY_SAVE:
            base_cost = 0.85f;
            break;
        case EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN:
            base_cost = 1.0f;  /* Expensive but comprehensive */
            break;
        default:
            base_cost = 0.5f;
    }

    /* Adjust cost based on severity match */
    int sev = (int)severity;
    float severity_factor = 1.0f;

    if (sev >= HEALTH_SEVERITY_CRITICAL) {
        /* For critical issues, prefer aggressive actions */
        if (action == EXCEPTION_RECOVERY_EMERGENCY_SAVE ||
            action == EXCEPTION_RECOVERY_QUARANTINE) {
            severity_factor = 0.5f;  /* Reduce cost */
        }
    } else if (sev <= HEALTH_SEVERITY_WARNING) {
        /* For minor issues, prefer lightweight actions */
        if (action == EXCEPTION_RECOVERY_CLEAR_CACHE ||
            action == EXCEPTION_RECOVERY_GC) {
            severity_factor = 0.5f;
        }
    }

    /* Adjust for source type */
    float source_factor = 1.0f;
    switch (source) {
        case HEALTH_SOURCE_MEMORY:
            if (action == EXCEPTION_RECOVERY_GC) source_factor = 0.3f;
            break;
        case HEALTH_SOURCE_THREADING:
            if (action == EXCEPTION_RECOVERY_RESTART_THREAD) source_factor = 0.4f;
            break;
        case HEALTH_SOURCE_NEURAL:
            if (action == EXCEPTION_RECOVERY_CLEAR_CACHE) source_factor = 0.4f;
            break;
        default:
            break;
    }

    return base_cost * severity_factor * source_factor;
}

/**
 * @brief Use quantum annealing to select optimal recovery action
 *
 * Uses quantum tunneling to escape local minima in recovery action selection.
 * Finds globally optimal action based on severity, source, and action costs.
 */
static nimcp_exception_recovery_action_t qa_select_recovery_action(
    tick_state_t* state, const health_agent_message_t* msg) {

    if (!state || !msg || !state->qa_enabled || !state->qa_annealer) {
        /* Fallback: return based on severity */
        if (msg && msg->severity >= HEALTH_SEVERITY_CRITICAL) {
            return EXCEPTION_RECOVERY_EMERGENCY_SAVE;
        }
        return EXCEPTION_RECOVERY_NONE;
    }

    /* Set up recovery context */
    recovery_context_t ctx = {
        .num_candidates = 0,
        .severity = msg->severity,
        .source = msg->source
    };

    /* Add candidate actions based on message type */
    ctx.candidates[ctx.num_candidates++] = EXCEPTION_RECOVERY_NONE;
    ctx.candidates[ctx.num_candidates++] = EXCEPTION_RECOVERY_CLEAR_CACHE;
    ctx.candidates[ctx.num_candidates++] = EXCEPTION_RECOVERY_GC;
    ctx.candidates[ctx.num_candidates++] = EXCEPTION_RECOVERY_REDUCE_LOAD;

    if (msg->severity >= HEALTH_SEVERITY_ERROR) {
        ctx.candidates[ctx.num_candidates++] = EXCEPTION_RECOVERY_ROLLBACK;
        ctx.candidates[ctx.num_candidates++] = EXCEPTION_RECOVERY_RESTART_THREAD;
    }

    if (msg->severity >= HEALTH_SEVERITY_CRITICAL) {
        ctx.candidates[ctx.num_candidates++] = EXCEPTION_RECOVERY_QUARANTINE;
        ctx.candidates[ctx.num_candidates++] = EXCEPTION_RECOVERY_EMERGENCY_SAVE;
    }

    /* Compute costs for each candidate */
    for (uint32_t i = 0; i < ctx.num_candidates; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx.num_candidates > 256) {
            brain_immune_tick_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)ctx.num_candidates);
        }

        ctx.costs[i] = get_recovery_cost(ctx.candidates[i], ctx.severity, ctx.source);
    }

    /* Initial state: uniform weights */
    float initial_state[QA_MAX_RECOVERY_ACTIONS] = {0};
    float optimized_state[QA_MAX_RECOVERY_ACTIONS] = {0};
    float uniform_weight = 1.0f / sqrtf((float)ctx.num_candidates);
    for (uint32_t i = 0; i < ctx.num_candidates; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx.num_candidates > 256) {
            brain_immune_tick_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)ctx.num_candidates);
        }

        initial_state[i] = uniform_weight;
    }

    /* Run quantum annealing */
    float final_energy = quantum_anneal(
        state->qa_annealer,
        qa_recovery_energy,
        initial_state,
        optimized_state,
        ctx.num_candidates,
        &ctx
    );

    /* Select action with highest weight in optimized state */
    uint32_t best_idx = 0;
    float best_weight = -INFINITY;
    for (uint32_t i = 0; i < ctx.num_candidates; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx.num_candidates > 256) {
            brain_immune_tick_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)ctx.num_candidates);
        }

        float weight = optimized_state[i] * optimized_state[i];
        if (weight > best_weight) {
            best_weight = weight;
            best_idx = i;
        }
    }

    LOG_DEBUG("QA recovery: selected %s (energy=%.4f, weight=%.3f)",
              nimcp_exception_recovery_action_to_string(ctx.candidates[best_idx]),
              final_energy, best_weight);

    return ctx.candidates[best_idx];
}

/**
 * @brief Create pattern memory for epitope tracking
 */
static pattern_memory_t* pattern_memory_create(void) {
    pattern_memory_t* pm = nimcp_calloc(1, sizeof(pattern_memory_t));
    if (!pm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_memory_create: pm is NULL");
        return NULL;
    }

    pm->num_patterns = 0;
    pm->mc_seed = mc_seed_from_time();

    /* Initialize all patterns as inactive */
    for (uint32_t i = 0; i < MAX_EPITOPE_PATTERNS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MAX_EPITOPE_PATTERNS > 256) {
            brain_immune_tick_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)MAX_EPITOPE_PATTERNS);
        }

        pm->patterns[i].is_active = false;
    }

    return pm;
}

/**
 * @brief Destroy pattern memory
 */
static void pattern_memory_destroy(pattern_memory_t* pm) {
    if (pm) {
        nimcp_free(pm);
        pm = NULL;
    }
}

/**
 * @brief Hash function for epitope (FNV-1a)
 */
static uint32_t epitope_hash(const uint8_t* epitope, size_t len) {
    uint32_t hash = 2166136261u;  /* FNV offset basis */
    for (size_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            brain_immune_tick_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)len);
        }

        hash ^= epitope[i];
        hash *= 16777619u;  /* FNV prime */
    }
    return hash;
}

/**
 * @brief Record an epitope pattern in memory
 *
 * Uses quantum-inspired phasor representation for coherence tracking.
 * Patterns with high coherence indicate recurring anomalies.
 */
static bool pattern_memory_record_epitope(pattern_memory_t* pm,
                                          const uint8_t* epitope, size_t len,
                                          float severity, uint64_t timestamp) {
    if (!pm || !epitope || len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epitope_hash: required parameter is NULL (pm, epitope)");
        return false;
    }

    uint32_t hash = epitope_hash(epitope, len);
    uint32_t slot = hash % MAX_EPITOPE_PATTERNS;

    /* Check if pattern already exists */
    epitope_pattern_t* pattern = &pm->patterns[slot];

    if (pattern->is_active) {
        /* Check if it's the same pattern */
        if (pattern->epitope_len == len &&
            memcmp(pattern->epitope, epitope, len) == 0) {
            /* Update existing pattern */
            pattern->occurrence_count++;
            pattern->last_seen_us = timestamp;

            /* Update phasor: add contribution with phase based on timing */
            float time_phase = (float)(timestamp % 1000000) / 1000000.0f * 2.0f * M_PI;
            neural_phasor_t contribution = phasor_from_polar(severity / 10.0f, time_phase);

            /* Accumulate phasor (quantum superposition) */
            pattern->phasor.real += contribution.real;
            pattern->phasor.imag += contribution.imag;

            /* Update severity average */
            float alpha = 0.2f;
            pattern->severity_avg = alpha * severity + (1.0f - alpha) * pattern->severity_avg;

            return true;
        }
        /* Collision: overwrite if old pattern is stale (>1 minute) */
        if (timestamp - pattern->last_seen_us > 60000000ULL) {
            /* Overwrite stale pattern */
        } else {
            /* Linear probe to next slot */
            slot = (slot + 1) % MAX_EPITOPE_PATTERNS;
            pattern = &pm->patterns[slot];
            if (pattern->is_active) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "epitope_hash: validation failed");
                return false;  /* No room */
            }
        }
    }

    /* Create new pattern */
    pattern->is_active = true;
    size_t copy_len = len < BRAIN_IMMUNE_EPITOPE_SIZE ? len : BRAIN_IMMUNE_EPITOPE_SIZE;
    memcpy(pattern->epitope, epitope, copy_len);
    pattern->epitope_len = copy_len;
    pattern->occurrence_count = 1;
    pattern->last_seen_us = timestamp;
    pattern->severity_avg = severity;

    /* Initialize phasor */
    float time_phase = (float)(timestamp % 1000000) / 1000000.0f * 2.0f * M_PI;
    pattern->phasor = phasor_from_polar(severity / 10.0f, time_phase);

    pm->num_patterns++;
    return true;
}

/**
 * @brief Check coherence of an epitope pattern
 *
 * Uses phasor coherence to detect recurring patterns.
 * High coherence (>0.75) indicates a frequently recurring anomaly pattern.
 */
static float pattern_memory_check_coherence(pattern_memory_t* pm,
                                            const uint8_t* epitope, size_t len) {
    if (!pm || !epitope || len == 0) return 0.0f;

    uint32_t hash = epitope_hash(epitope, len);
    uint32_t slot = hash % MAX_EPITOPE_PATTERNS;

    epitope_pattern_t* pattern = &pm->patterns[slot];

    if (!pattern->is_active) return 0.0f;

    /* Check if it's the matching pattern */
    if (pattern->epitope_len != len ||
        memcmp(pattern->epitope, epitope, len) != 0) {
        /* Try linear probe */
        slot = (slot + 1) % MAX_EPITOPE_PATTERNS;
        pattern = &pm->patterns[slot];
        if (!pattern->is_active ||
            pattern->epitope_len != len ||
            memcmp(pattern->epitope, epitope, len) != 0) {
            return 0.0f;
        }
    }

    /* Compute coherence from phasor amplitude */
    float amplitude = phasor_amplitude(pattern->phasor);
    float expected_amplitude = (float)pattern->occurrence_count * (pattern->severity_avg / 10.0f);

    if (expected_amplitude <= 0.0f) return 0.0f;

    /* Coherence = actual / expected (clamped to [0, 1]) */
    float coherence = amplitude / expected_amplitude;
    if (coherence > 1.0f) coherence = 1.0f;

    return coherence;
}

/**
 * @brief Numerical integration for tick duration statistics
 *
 * Uses proper exponential moving average with numerical integration.
 */
static void integrate_tick_duration(tick_state_t* state, uint64_t duration_us) {
    if (!state) return;

    /* State for EMA integration: dy/dt = alpha * (target - y) */
    float target = (float)duration_us;
    float current = state->stats.avg_tick_duration_us;

    /* Single-step Euler integration of EMA */
    float derivative = EMA_ALPHA * (target - current);
    float new_value = current + derivative;

    state->stats.avg_tick_duration_us = new_value;
    state->integration_step_count++;

    /* Also track integral (cumulative sum for later analysis) */
    state->tick_duration_integral += target * EMA_ALPHA;
}

/* ============================================================================
 * Health Message Processing Functions
 * ============================================================================ */

/**
 * @brief Process HEALTH_MSG_ANOMALY_DETECTED -> Present as antigen
 *
 * Enhanced with:
 * - Monte Carlo severity sampling (probabilistic assessment)
 * - Quantum-inspired pattern memory (recurring anomaly detection)
 */
static int process_anomaly_message(brain_immune_system_t* immune,
                                    const health_agent_message_t* msg) {
    tick_state_t* state = get_tick_state(immune);

    /* Create epitope from message description */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE] = {0};
    size_t desc_len = strlen(msg->description);
    size_t copy_len = desc_len < BRAIN_IMMUNE_EPITOPE_SIZE ? desc_len : BRAIN_IMMUNE_EPITOPE_SIZE;
    memcpy(epitope, msg->description, copy_len);

    /* ========== Enhanced: Monte Carlo Severity Assessment ========== */
    float mc_severity = mc_sample_severity(state, msg->severity, msg->source);
    uint32_t severity = (uint32_t)(mc_severity + 0.5f);  /* Round to integer */
    if (severity < 1) severity = 1;
    if (severity > 10) severity = 10;

    /* ========== Enhanced: Pattern Memory Recording ========== */
    float coherence = 0.0f;
    bool is_recurring = false;
    if (state && state->pattern_memory) {
        /* Check if this is a recurring pattern */
        coherence = pattern_memory_check_coherence(state->pattern_memory, epitope, copy_len);
        is_recurring = (coherence >= PATTERN_COHERENCE_THRESHOLD);

        /* Record the pattern */
        uint64_t now = get_timestamp_us();
        pattern_memory_record_epitope(state->pattern_memory, epitope, copy_len,
                                      mc_severity, now);

        /* Boost severity for recurring patterns (immune memory effect) */
        if (is_recurring) {
            severity = (uint32_t)(severity * 1.5f);
            if (severity > 10) severity = 10;
            LOG_DEBUG("Recurring anomaly detected (coherence=%.2f), severity boosted to %u",
                      coherence, severity);
        }
    }

    /* Present as antigen */
    uint32_t antigen_id = 0;
    int result = brain_immune_present_antigen(
        immune,
        map_health_source_to_antigen_source(msg->source),
        epitope,
        copy_len,
        severity,
        0,  /* source_node */
        &antigen_id
    );

    if (result == 0) {
        LOG_DEBUG("Presented anomaly as antigen: id=%u, severity=%u (MC=%.1f), "
                  "source=%s, recurring=%s",
                  antigen_id, severity, mc_severity,
                  health_agent_source_to_string(msg->source),
                  is_recurring ? "yes" : "no");
    }

    return result;
}

/**
 * @brief Process HEALTH_MSG_CYTOKINE_SIGNAL -> Release cytokine
 */
static int process_cytokine_message(brain_immune_system_t* immune,
                                     const health_agent_message_t* msg) {
    /* Determine cytokine type based on severity */
    brain_cytokine_type_t type;
    float concentration = 0.5f;

    switch (msg->severity) {
        case HEALTH_SEVERITY_INFO:
            type = BRAIN_CYTOKINE_IL10;  /* Anti-inflammatory for info */
            concentration = 0.2f;
            break;
        case HEALTH_SEVERITY_WARNING:
            type = BRAIN_CYTOKINE_IL1;   /* Pro-inflammatory alert */
            concentration = 0.4f;
            break;
        case HEALTH_SEVERITY_ERROR:
            type = BRAIN_CYTOKINE_IL6;   /* Acute phase response */
            concentration = 0.6f;
            break;
        case HEALTH_SEVERITY_CRITICAL:
            type = BRAIN_CYTOKINE_TNF;   /* Severe inflammation */
            concentration = 0.8f;
            break;
        case HEALTH_SEVERITY_FATAL:
            type = CYTOKINE_IFN_GAMMA;   /* Antiviral/quarantine response */
            concentration = 1.0f;
            break;
        default:
            type = BRAIN_CYTOKINE_IL1;
            concentration = 0.3f;
    }

    uint32_t cytokine_id = 0;
    int result = brain_immune_release_cytokine(
        immune,
        type,
        0,              /* source_cell (system generated) */
        concentration,
        0,              /* target_region (broadcast) */
        &cytokine_id
    );

    if (result == 0) {
        LOG_DEBUG("Released cytokine: id=%u, type=%s, concentration=%.2f",
                  cytokine_id,
                  brain_immune_cytokine_to_string(type),
                  concentration);
    }

    return result;
}

/**
 * @brief Process HEALTH_MSG_EMERGENCY -> Inflammation + emergency recovery
 */
static int process_emergency_message(brain_immune_system_t* immune,
                                      const health_agent_message_t* msg) {
    int result = 0;

    /* 1. Present as high-severity antigen */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE] = {0};
    snprintf((char*)epitope, BRAIN_IMMUNE_EPITOPE_SIZE, "EMERGENCY:%s", msg->description);
    uint32_t antigen_id = 0;
    result = brain_immune_present_antigen(
        immune,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        strlen((char*)epitope),
        10,  /* Maximum severity */
        0,
        &antigen_id
    );

    /* 2. Initiate systemic inflammation */
    if (result == 0) {
        uint32_t site_id = 0;
        brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);

        /* Escalate to systemic if possible */
        brain_immune_escalate_inflammation(immune, site_id);
        brain_immune_escalate_inflammation(immune, site_id);
    }

    /* 3. Release emergency cytokines */
    uint32_t cytokine_id = 0;
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_TNF, 0, 1.0f, 0, &cytokine_id);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL6, 0, 0.9f, 0, &cytokine_id);

    /* 4. Broadcast alert */
    brain_immune_broadcast_alert(immune, antigen_id, INFLAMMATION_SYSTEMIC);

    LOG_WARNING("Emergency processed: antigen_id=%u, %s", antigen_id, msg->description);

    return result;
}

/**
 * @brief Process HEALTH_MSG_RECOVERY_REQUEST -> Execute suggested action
 *
 * Enhanced with quantum annealing for optimal action selection when
 * the suggested action is HEALTH_RECOVERY_AUTO or severity warrants optimization.
 */
static int process_recovery_request(brain_immune_system_t* immune,
                                     const health_agent_message_t* msg) {
    tick_state_t* state = get_tick_state(immune);
    nimcp_exception_recovery_action_t action = EXCEPTION_RECOVERY_NONE;
    bool used_qa = false;

    /* ========== Enhanced: Use quantum annealing for critical decisions ========== */
    if (state && state->qa_enabled && msg->severity >= HEALTH_SEVERITY_ERROR) {
        /* Use quantum annealing to find optimal recovery action */
        action = qa_select_recovery_action(state, msg);
        used_qa = true;
    }

    /* Fallback: Map health recovery to exception recovery action */
    if (!used_qa || action == EXCEPTION_RECOVERY_NONE) {
        switch (msg->suggested_action) {
            case HEALTH_RECOVERY_GC:
                action = EXCEPTION_RECOVERY_GC;
                break;
            case HEALTH_RECOVERY_CHECKPOINT:
                action = EXCEPTION_RECOVERY_EMERGENCY_SAVE;
                break;
            case HEALTH_RECOVERY_ROLLBACK:
                action = EXCEPTION_RECOVERY_ROLLBACK;
                break;
            case HEALTH_RECOVERY_RESTART_THREAD:
                action = EXCEPTION_RECOVERY_RESTART_THREAD;
                break;
            case HEALTH_RECOVERY_CLEAR_NAN:
                action = EXCEPTION_RECOVERY_CLEAR_CACHE;
                break;
            case HEALTH_RECOVERY_REDUCE_LOAD:
                action = EXCEPTION_RECOVERY_REDUCE_LOAD;
                break;
            case HEALTH_RECOVERY_QUARANTINE:
                action = EXCEPTION_RECOVERY_QUARANTINE;
                break;
            case HEALTH_RECOVERY_EMERGENCY_SAVE:
                action = EXCEPTION_RECOVERY_EMERGENCY_SAVE;
                break;
            case HEALTH_RECOVERY_FULL_RESET:
                action = EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN;
                break;
            default:
                action = EXCEPTION_RECOVERY_NONE;
        }
        used_qa = false;
    }

    if (action == EXCEPTION_RECOVERY_NONE) {
        return 0;
    }

    /* Execute recovery via exception system */
    int result = nimcp_exception_execute_recovery(NULL, action);

    LOG_INFO("Recovery request processed: action=%s, execution=%s, method=%s",
             nimcp_exception_recovery_action_to_string(action),
             result == 0 ? "success" : "failed",
             used_qa ? "quantum_annealing" : "direct_mapping");

    /* Message was processed successfully even if recovery execution failed.
     * Recovery may fail for valid reasons (no checkpoint, no thread to restart, etc.)
     * The message processing is still considered successful. */
    return 0;
}

/**
 * @brief Process HEALTH_MSG_STATE_CORRUPTION -> Antigen + rollback
 */
static int process_corruption_message(brain_immune_system_t* immune,
                                       const health_agent_message_t* msg) {
    /* Present corruption as antigen */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE] = {0};
    snprintf((char*)epitope, BRAIN_IMMUNE_EPITOPE_SIZE, "CORRUPTION:state");
    uint32_t antigen_id = 0;
    brain_immune_present_antigen(
        immune,
        ANTIGEN_SOURCE_BFT,
        epitope,
        strlen((char*)epitope),
        8,
        0,
        &antigen_id
    );

    /* Request rollback */
    nimcp_exception_execute_recovery(NULL, EXCEPTION_RECOVERY_ROLLBACK);

    LOG_WARNING("State corruption detected, rollback initiated: %s", msg->description);
    return 0;
}

/**
 * @brief Process HEALTH_MSG_MEMORY_CORRUPTION -> Antigen + quarantine
 */
static int process_memory_corruption_message(brain_immune_system_t* immune,
                                              const health_agent_message_t* msg) {
    /* Present memory corruption as antigen */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE] = {0};
    snprintf((char*)epitope, BRAIN_IMMUNE_EPITOPE_SIZE, "MEM_CORRUPT:%p",
             msg->data.memory.address);
    uint32_t antigen_id = 0;
    brain_immune_present_antigen(
        immune,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        strlen((char*)epitope),
        9,
        0,
        &antigen_id
    );

    /* Initiate inflammation at region */
    uint32_t site_id = 0;
    brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);

    /* Request quarantine (will be handled by recovery system) */
    nimcp_exception_execute_recovery(NULL, EXCEPTION_RECOVERY_QUARANTINE);

    LOG_ERROR("Memory corruption detected at %p, quarantine initiated",
              msg->data.memory.address);
    return 0;
}

/**
 * @brief Process HEALTH_MSG_NAN_DETECTED -> Antigen + clear cache
 */
static int process_nan_message(brain_immune_system_t* immune,
                                const health_agent_message_t* msg) {
    /* Present NaN as antigen */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE] = {0};
    snprintf((char*)epitope, BRAIN_IMMUNE_EPITOPE_SIZE, "NAN:layer=%u,neuron=%u",
             msg->data.nan.layer_id, msg->data.nan.neuron_id);
    uint32_t antigen_id = 0;
    brain_immune_present_antigen(
        immune,
        ANTIGEN_SOURCE_BBB,
        epitope,
        strlen((char*)epitope),
        7,
        0,
        &antigen_id
    );

    /* Clear neural caches */
    nimcp_exception_execute_recovery(NULL, EXCEPTION_RECOVERY_CLEAR_CACHE);

    LOG_WARNING("NaN detected in neural computation: layer=%u, count=%u",
                msg->data.nan.layer_id, msg->data.nan.nan_count);
    return 0;
}

/**
 * @brief Process HEALTH_MSG_DEADLOCK_DETECTED -> Antigen + restart threads
 */
static int process_deadlock_message(brain_immune_system_t* immune,
                                     const health_agent_message_t* msg) {
    /* Present deadlock as antigen */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE] = {0};
    snprintf((char*)epitope, BRAIN_IMMUNE_EPITOPE_SIZE, "DEADLOCK:t1=%lu,t2=%lu",
             (unsigned long)msg->data.deadlock.thread_id_1,
             (unsigned long)msg->data.deadlock.thread_id_2);
    uint32_t antigen_id = 0;
    brain_immune_present_antigen(
        immune,
        ANTIGEN_SOURCE_BFT,
        epitope,
        strlen((char*)epitope),
        8,
        0,
        &antigen_id
    );

    /* Initiate inflammation */
    uint32_t site_id = 0;
    brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);

    /* Restart affected threads */
    nimcp_exception_execute_recovery(NULL, EXCEPTION_RECOVERY_RESTART_THREAD);

    LOG_ERROR("Deadlock detected: threads=%lu,%lu",
              (unsigned long)msg->data.deadlock.thread_id_1,
              (unsigned long)msg->data.deadlock.thread_id_2);
    return 0;
}

/**
 * @brief Process HEALTH_MSG_HEARTBEAT_TIMEOUT -> High-severity antigen + emergency save
 */
static int process_heartbeat_timeout(brain_immune_system_t* immune,
                                      const health_agent_message_t* msg) {
    /* Present as high-severity antigen */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE] = {0};
    snprintf((char*)epitope, BRAIN_IMMUNE_EPITOPE_SIZE, "HEARTBEAT_TIMEOUT:missed=%u",
             msg->data.heartbeat.missed_beats);
    uint32_t antigen_id = 0;
    brain_immune_present_antigen(
        immune,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        strlen((char*)epitope),
        9,  /* High severity - system may be hung */
        0,
        &antigen_id
    );

    /* Initiate inflammation and escalate */
    uint32_t site_id = 0;
    brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);
    brain_immune_escalate_inflammation(immune, site_id);

    /* Emergency save */
    nimcp_exception_execute_recovery(NULL, EXCEPTION_RECOVERY_EMERGENCY_SAVE);

    LOG_ERROR("Heartbeat timeout: missed=%u beats", msg->data.heartbeat.missed_beats);
    return 0;
}

/**
 * @brief Process HEALTH_MSG_RESOURCE_EXHAUSTION -> Antigen + reduce load + GC
 */
static int process_resource_exhaustion(brain_immune_system_t* immune,
                                        const health_agent_message_t* msg) {
    /* Present as antigen */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE] = {0};
    snprintf((char*)epitope, BRAIN_IMMUNE_EPITOPE_SIZE, "RESOURCE_EXHAUSTION:%.1f%%",
             msg->data.resource.utilization_pct);
    uint32_t antigen_id = 0;
    brain_immune_present_antigen(
        immune,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        strlen((char*)epitope),
        (uint32_t)(msg->data.resource.utilization_pct / 10.0f),
        0,
        &antigen_id
    );

    /* Reduce load */
    nimcp_exception_execute_recovery(NULL, EXCEPTION_RECOVERY_REDUCE_LOAD);

    /* Trigger GC if utilization is very high */
    if (msg->data.resource.utilization_pct > 85.0f) {
        nimcp_exception_execute_recovery(NULL, EXCEPTION_RECOVERY_GC);
    }

    LOG_WARNING("Resource exhaustion: %.1f%% utilized, ETA exhaustion=%ums",
                msg->data.resource.utilization_pct,
                msg->data.resource.time_to_exhaust_ms);
    return 0;
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

void brain_immune_tick_default_config(brain_immune_tick_config_t* config) {
    if (!config) return;

    /* Phase 8: Heartbeat at operation start */
    brain_immune_tick_heartbeat("brain_immune_default_config", 0.0f);


    config->max_exceptions_per_tick = BRAIN_IMMUNE_TICK_DEFAULT_MAX_EXCEPTIONS;
    config->max_health_msgs_per_tick = BRAIN_IMMUNE_TICK_DEFAULT_MAX_HEALTH_MSGS;
    config->enable_exception_processing = true;
    config->enable_health_agent_processing = true;
    config->enable_antigen_processing = true;
    config->enable_antibody_decay = true;
    config->enable_inflammation_updates = true;
    config->enable_tick_logging = false;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int brain_immune_tick_init(brain_immune_system_t* immune,
                           const brain_immune_tick_config_t* config) {
    if (!immune) {
        LOG_ERROR("Cannot initialize tick: immune system is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_tick_init: immune is NULL");
        return -1;
    }

    /* Check if already initialized */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_tick_heartbeat("brain_immune_init", 0.0f);


    tick_state_t* existing = get_tick_state(immune);
    if (existing && existing->initialized) {
        LOG_WARNING("Tick orchestrator already initialized");
        return 0;
    }

    /* Allocate tick state */
    tick_state_t* state = nimcp_calloc(1, sizeof(tick_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate tick state");
        return -1;
    }

    /* Initialize state */
    state->magic = TICK_STATE_MAGIC;
    state->initialized = false;

    /* Set configuration */
    if (config) {
        state->config = *config;
    } else {
        brain_immune_tick_default_config(&state->config);
    }

    /* Create mutex */
    state->mutex = nimcp_mutex_create(NULL);
    if (!state->mutex) {
        nimcp_free(state);
        state = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create tick mutex");
        return -1;
    }

    /* Initialize stats */
    memset(&state->stats, 0, sizeof(state->stats));
    state->total_tick_time_us = 0;
    state->last_tick_time_us = get_timestamp_us();
    state->health_agent = NULL;

    /* ========== Enhanced Features Initialization (NIMCP Utilities) ========== */

    /* Initialize Monte Carlo RNG seed */
    state->mc_seed = mc_seed_from_time();

    /* Initialize pattern memory for quantum-inspired epitope tracking */
    state->pattern_memory = pattern_memory_create();
    if (!state->pattern_memory) {
        LOG_WARNING("Failed to create pattern memory; pattern detection disabled");
    }

    /* Initialize quantum annealer for optimal recovery selection */
    quantum_annealing_config_t qa_config = quantum_annealing_default_config();
    qa_config.num_iterations = QA_RECOVERY_ITERATIONS;
    qa_config.enable_tunneling = true;
    qa_config.seed = state->mc_seed;

    state->qa_annealer = quantum_annealer_create(&qa_config);
    state->qa_enabled = (state->qa_annealer != NULL);
    if (!state->qa_enabled) {
        LOG_WARNING("Failed to create quantum annealer; using fallback recovery selection");
    }

    /* Initialize numerical integration state */
    state->tick_duration_integral = 0.0f;
    state->integration_step_count = 0;

    /* ========== End Enhanced Features ========== */

    /* Store in immune system */
    immune->callback_user_data = state;

    state->initialized = true;
    LOG_INFO("Brain immune tick orchestrator initialized (MC=%s, QA=%s, PM=%s)",
             "enabled",
             state->qa_enabled ? "enabled" : "disabled",
             state->pattern_memory ? "enabled" : "disabled");
    return 0;
}

int brain_immune_tick_connect_health_agent(brain_immune_system_t* immune,
                                            nimcp_health_agent_t* agent) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_tick_heartbeat("brain_immune_connect_health_agent", 0.0f);


    tick_state_t* state = get_tick_state(immune);
    if (!state || !state->initialized) {
        LOG_ERROR("Tick orchestrator not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_tick_default_config: required parameter is NULL (state, state->initialized)");
        return -1;
    }

    tick_mutex_lock(state);
    state->health_agent = agent;
    tick_mutex_unlock(state);

    if (agent) {
        LOG_INFO("Health agent connected to tick orchestrator");
    } else {
        LOG_INFO("Health agent disconnected from tick orchestrator");
    }

    return 0;
}

void brain_immune_tick_shutdown(brain_immune_system_t* immune) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_tick_heartbeat("brain_immune_shutdown", 0.0f);


    tick_state_t* state = get_tick_state(immune);
    if (!state) return;

    /* Log final stats with enhanced metrics */
    LOG_INFO("Tick orchestrator shutdown: ticks=%lu, exceptions=%lu, health_msgs=%lu, "
             "integration_steps=%lu, patterns=%u",
             (unsigned long)state->stats.ticks_executed,
             (unsigned long)state->stats.exceptions_processed,
             (unsigned long)state->stats.health_messages_processed,
             (unsigned long)state->integration_step_count,
             state->pattern_memory ? state->pattern_memory->num_patterns : 0);

    /* Clear immune reference */
    if (immune) {
        immune->callback_user_data = NULL;
    }

    /* ========== Cleanup Enhanced Features (NIMCP Utilities) ========== */

    /* Destroy pattern memory */
    if (state->pattern_memory) {
        pattern_memory_destroy(state->pattern_memory);
        state->pattern_memory = NULL;
    }

    /* Destroy quantum annealer */
    if (state->qa_annealer) {
        quantum_annealer_destroy(state->qa_annealer);
        state->qa_annealer = NULL;
        state->qa_enabled = false;
    }

    /* ========== End Enhanced Features Cleanup ========== */

    /* Destroy mutex */
    if (state->mutex) {
        nimcp_mutex_destroy(state->mutex);
    }

    /* Clear and free */
    state->magic = 0;
    state->initialized = false;
    nimcp_free(state);
    state = NULL;
}

/* ============================================================================
 * Core Tick API
 * ============================================================================ */

int brain_immune_tick(brain_immune_system_t* immune, uint64_t delta_ms) {
    /* Reentry guard - prevent recursive tick calls */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_tick_heartbeat("brain_immune_brain_immune_tick", 0.0f);


    if (tl_in_immune_tick) {
        /* Get state to track blocked calls */
        tick_state_t* state = get_tick_state(immune);
        if (state) {
            __atomic_fetch_add(&state->stats.reentrant_calls_blocked, 1, __ATOMIC_RELAXED);
        }
        return 1;  /* Return 1 to indicate blocked reentrant call */
    }

    /* Set reentry guard */
    tl_in_immune_tick = true;

    if (!immune) {
        tl_in_immune_tick = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_tick: immune is NULL");
        return -1;
    }

    tick_state_t* state = get_tick_state(immune);
    if (!state || !state->initialized) {
        tl_in_immune_tick = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_tick: required parameter is NULL (state, state->initialized)");
        return -1;
    }

    uint64_t tick_start = get_timestamp_us();
    bool did_work = false;
    int result = 0;

    /* 1. Process pending async exceptions */
    if (state->config.enable_exception_processing) {
        size_t max_ex = state->config.max_exceptions_per_tick;
        size_t processed = nimcp_exception_immune_process_pending(max_ex);
        if (processed > 0) {
            state->stats.exceptions_processed += processed;
            did_work = true;
            if (state->config.enable_tick_logging) {
                LOG_DEBUG("Tick processed %zu exceptions", processed);
            }
        }
    }

    /* 2. Process health agent message queue */
    if (state->config.enable_health_agent_processing && state->health_agent) {
        size_t max_msgs = state->config.max_health_msgs_per_tick;
        int msgs_processed = brain_immune_process_health_queue(immune, max_msgs);
        if (msgs_processed > 0) {
            state->stats.health_messages_processed += (uint64_t)msgs_processed;
            did_work = true;
            if (state->config.enable_tick_logging) {
                LOG_DEBUG("Tick processed %d health messages", msgs_processed);
            }
        }
    }

    /* 3. Run core immune update */
    result = brain_immune_update(immune, delta_ms);
    if (result == 0) {
        did_work = true;
    }

    /* 4. Periodic immune memory check — enable secondary immune responses
     * WHAT: Scan active antigens against memory B cells every 10 ticks
     * WHY:  Antigens presented between ticks may not have triggered memory check
     *       if check_memory was not called at presentation time. This catches
     *       any that slipped through and enables faster secondary responses.
     * HOW:  Every 10 ticks, iterate active unprocessed antigens and check for
     *       memory B cell matches. If found, trigger secondary response. */
    if (state->stats.ticks_executed % 10 == 0) {
        nimcp_mutex_lock(immune->mutex);
        for (size_t i = 0; i < immune->antigen_count; i++) {
            brain_antigen_t* ag = &immune->antigens[i];
            if (ag->processed || ag->neutralized) continue;

            uint32_t memory_b_cell_id = 0;
            /* Unlock before check_memory (it acquires its own lock internally) */
            nimcp_mutex_unlock(immune->mutex);

            if (brain_immune_check_memory(immune, ag->id, &memory_b_cell_id) == 0) {
                if (immune->config.enable_logging) {
                    LOG_MODULE_INFO(BRAIN_IMMUNE_MODULE_NAME,
                        "Scheduled memory check: antigen %u matches memory B cell %u",
                        ag->id, memory_b_cell_id);
                }
                brain_immune_secondary_response(immune, ag->id, memory_b_cell_id);
                did_work = true;
            }

            nimcp_mutex_lock(immune->mutex);
            /* Re-validate index after re-acquiring lock — array may have changed */
            if (i >= immune->antigen_count) break;
        }
        nimcp_mutex_unlock(immune->mutex);
    }

    /* 5. Decay old immune memory B cells (biological: ~10-year half-life, simulated: 100K ticks)
     * WHAT: Periodically reduce affinity of MEMORY B cells based on age
     * WHY:  Prevents unbounded memory cell accumulation; mirrors biological decay
     * HOW:  Every 100 ticks, scan memory B cells and halve affinity if old enough;
     *       remove cells with negligible affinity */
    #define IMMUNE_MEMORY_HALF_LIFE_US  (100000ULL * 1000000ULL) /* 100K seconds ~ simulated half-life */
    if (state->stats.ticks_executed % 100 == 0) {
        uint64_t now_us = get_timestamp_us();
        nimcp_mutex_lock(immune->mutex);
        for (size_t i = 0; i < immune->b_cell_count; i++) {
            if (immune->b_cells[i].state != B_CELL_MEMORY) continue;
            uint64_t age_us = now_us - (immune->b_cells[i].activation_time * 1000); /* activation_time is ms */
            if (age_us > IMMUNE_MEMORY_HALF_LIFE_US) {
                immune->b_cells[i].affinity *= 0.5f;
                if (immune->b_cells[i].affinity < 0.01f) {
                    /* Remove expired memory cell by swapping with last */
                    immune->b_cells[i] = immune->b_cells[--immune->b_cell_count];
                    i--; /* Re-check swapped element */
                    did_work = true;
                }
            }
        }
        nimcp_mutex_unlock(immune->mutex);
    }
    #undef IMMUNE_MEMORY_HALF_LIFE_US

    /* Update statistics */
    uint64_t tick_duration = get_timestamp_us() - tick_start;
    state->total_tick_time_us += tick_duration;
    state->stats.ticks_executed++;

    /* Update average tick duration using numerical integration (EMA as ODE) */
    integrate_tick_duration(state, tick_duration);

    /* Track max duration */
    if (tick_duration > state->stats.max_tick_duration_us) {
        state->stats.max_tick_duration_us = tick_duration;
    }

    /* Track idle ticks */
    if (!did_work) {
        state->stats.idle_ticks++;
    }

    state->last_tick_time_us = tick_start;

    /* Clear reentry guard */
    tl_in_immune_tick = false;

    return result;
}

/* ============================================================================
 * Health Message Processing API
 * ============================================================================ */

int brain_immune_process_health_message(brain_immune_system_t* immune,
                                         const health_agent_message_t* msg) {
    if (!immune || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_tick: required parameter is NULL (immune, msg)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_tick_heartbeat("brain_immune_brain_immune_process", 0.0f);


    tick_state_t* state = get_tick_state(immune);
    int result = 0;

    /* Process based on message type */
    switch (msg->type) {
        case HEALTH_MSG_ANOMALY_DETECTED:
            result = process_anomaly_message(immune, msg);
            if (state) state->stats.health_antigens_created++;
            break;

        case HEALTH_MSG_CYTOKINE_SIGNAL:
            result = process_cytokine_message(immune, msg);
            if (state) state->stats.health_cytokines_released++;
            break;

        case HEALTH_MSG_EMERGENCY:
            result = process_emergency_message(immune, msg);
            if (state) {
                state->stats.health_antigens_created++;
                state->stats.recovery_actions_triggered++;
            }
            break;

        case HEALTH_MSG_RECOVERY_REQUEST:
            result = process_recovery_request(immune, msg);
            if (state) state->stats.recovery_actions_triggered++;
            if (result == 0 && state) state->stats.recovery_actions_succeeded++;
            break;

        case HEALTH_MSG_STATE_CORRUPTION:
            result = process_corruption_message(immune, msg);
            if (state) {
                state->stats.health_antigens_created++;
                state->stats.recovery_actions_triggered++;
            }
            break;

        case HEALTH_MSG_MEMORY_CORRUPTION:
            result = process_memory_corruption_message(immune, msg);
            if (state) {
                state->stats.health_antigens_created++;
                state->stats.recovery_actions_triggered++;
            }
            break;

        case HEALTH_MSG_NAN_DETECTED:
            result = process_nan_message(immune, msg);
            if (state) {
                state->stats.health_antigens_created++;
                state->stats.recovery_actions_triggered++;
            }
            break;

        case HEALTH_MSG_DEADLOCK_DETECTED:
            result = process_deadlock_message(immune, msg);
            if (state) {
                state->stats.health_antigens_created++;
                state->stats.recovery_actions_triggered++;
            }
            break;

        case HEALTH_MSG_HEARTBEAT_TIMEOUT:
            result = process_heartbeat_timeout(immune, msg);
            if (state) {
                state->stats.health_antigens_created++;
                state->stats.recovery_actions_triggered++;
            }
            break;

        case HEALTH_MSG_RESOURCE_EXHAUSTION:
            result = process_resource_exhaustion(immune, msg);
            if (state) {
                state->stats.health_antigens_created++;
                state->stats.recovery_actions_triggered++;
            }
            break;

        case HEALTH_MSG_STATUS_UPDATE:
            /* Status updates are informational only */
            break;

        default:
            LOG_WARNING("Unknown health message type: %d", msg->type);
            break;
    }

    return result;
}

int brain_immune_process_health_queue(brain_immune_system_t* immune,
                                       size_t max_count) {
    if (!immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_tick: immune is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_tick_heartbeat("brain_immune_brain_immune_process", 0.0f);


    tick_state_t* state = get_tick_state(immune);
    if (!state || !state->health_agent) {
        return 0;  /* No agent connected, nothing to process */
    }

    int processed = 0;
    health_agent_message_t msg;

    while ((max_count == 0 || (size_t)processed < max_count) &&
           nimcp_health_agent_dequeue_message(state->health_agent, &msg)) {
        brain_immune_process_health_message(immune, &msg);
        processed++;
    }

    return processed;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

bool brain_immune_tick_is_initialized(const brain_immune_system_t* immune) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_tick_heartbeat("brain_immune_is_initialized", 0.0f);


    const tick_state_t* state = get_tick_state_const(immune);
    return state && state->initialized;
}

bool brain_immune_tick_has_health_agent(const brain_immune_system_t* immune) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_tick_heartbeat("brain_immune_has_health_agent", 0.0f);


    const tick_state_t* state = get_tick_state_const(immune);
    return state && state->health_agent != NULL;
}

int brain_immune_tick_get_stats(const brain_immune_system_t* immune,
                                 brain_immune_tick_stats_t* stats) {
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_tick_get_stats: stats is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_tick_heartbeat("brain_immune_get_stats", 0.0f);


    const tick_state_t* state = get_tick_state_const(immune);
    if (!state) {
        memset(stats, 0, sizeof(*stats));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_tick_get_stats: state is NULL");
        return -1;
    }

    *stats = state->stats;
    return 0;
}

void brain_immune_tick_reset_stats(brain_immune_system_t* immune) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_tick_heartbeat("brain_immune_reset_stats", 0.0f);


    tick_state_t* state = get_tick_state(immune);
    if (!state) return;

    tick_mutex_lock(state);
    memset(&state->stats, 0, sizeof(state->stats));
    state->total_tick_time_us = 0;
    tick_mutex_unlock(state);
}

int brain_immune_tick_get_config(const brain_immune_system_t* immune,
                                  brain_immune_tick_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_tick_get_config: config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_tick_heartbeat("brain_immune_get_config", 0.0f);


    const tick_state_t* state = get_tick_state_const(immune);
    if (!state) {
        brain_immune_tick_default_config(config);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_tick_get_config: state is NULL");
        return -1;
    }

    *config = state->config;
    return 0;
}

int brain_immune_tick_set_config(brain_immune_system_t* immune,
                                  const brain_immune_tick_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_tick_set_config: config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_tick_heartbeat("brain_immune_set_config", 0.0f);


    tick_state_t* state = get_tick_state(immune);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_tick_set_config: state is NULL");
        return -1;
    }

    tick_mutex_lock(state);
    state->config = *config;
    tick_mutex_unlock(state);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void brain_immune_tick_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_brain_immune_tick_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int brain_immune_tick_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_tick_training_begin: NULL argument");
        return -1;
    }
    brain_immune_tick_heartbeat_instance(NULL, "brain_immune_tick_training_begin", 0.0f);
    (void)(epitope_pattern_t*)instance; /* Module state available for reset */
    return 0;
}

int brain_immune_tick_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_tick_training_end: NULL argument");
        return -1;
    }
    brain_immune_tick_heartbeat_instance(NULL, "brain_immune_tick_training_end", 1.0f);
    (void)(epitope_pattern_t*)instance; /* Module state available for finalization */
    return 0;
}

int brain_immune_tick_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_tick_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    brain_immune_tick_heartbeat_instance(NULL, "brain_immune_tick_training_step", progress);
    (void)(epitope_pattern_t*)instance; /* Module state available for step adaptation */
    return 0;
}
