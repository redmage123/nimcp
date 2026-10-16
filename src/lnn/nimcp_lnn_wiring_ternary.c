/**
 * @file nimcp_lnn_wiring_ternary.c
 * @brief Ternary Wiring Pattern Implementation for LNN
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Ternary wiring with connection strengths {STRONG=+1, WEAK=0, ABSENT=-1}
 * WHY:  Capture bidirectional connection semantics in neural wiring
 * HOW:  Generate topologies with ternary adjacency representation
 *
 * @author NIMCP Development Team
 */

#include "lnn/nimcp_lnn_wiring_ternary.h"
#include "lnn/nimcp_lnn_ternary.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lnn_wiring_ternary)

/*=============================================================================
 * Thread-Local RNG State
 *===========================================================================*/

static __thread unsigned int tls_seed = 0;
static __thread bool tls_seed_initialized = false;

/**
 * @brief Initialize thread-local RNG seed
 */
static void ensure_rng_initialized(void) {
    if (!tls_seed_initialized) {
        tls_seed = (unsigned int)time(NULL) ^ (unsigned int)pthread_self();
        tls_seed_initialized = true;
    }
}

/**
 * @brief Seed RNG with specific value
 */
static void seed_rng(uint64_t seed) {
    if (seed == 0) {
        ensure_rng_initialized();
    } else {
        tls_seed = (unsigned int)(seed ^ (seed >> 32));
        tls_seed_initialized = true;
    }
}

/**
 * @brief Thread-safe random float in [0, 1)
 */
static float randf(void) {
    ensure_rng_initialized();
    return (float)rand_r(&tls_seed) / (float)RAND_MAX;
}

/**
 * @brief Thread-safe random integer in [min, max)
 */
static uint32_t rand_range(uint32_t min, uint32_t max) {
    ensure_rng_initialized();
    if (max <= min) return min;
    return min + (rand_r(&tls_seed) % (max - min));
}

/*=============================================================================
 * Internal Helper Functions
 *===========================================================================*/

/**
 * @brief Generate random ternary wiring (Erdos-Renyi)
 */
static int generate_random_wiring(
    lnn_ternary_wiring_t* wiring,
    const lnn_ternary_wiring_config_t* config
) {
    float edge_prob = 1.0f - config->sparsity;

    for (uint32_t i = 0; i < config->n_neurons; i++) {
        for (uint32_t j = 0; j < config->n_neurons; j++) {
            if (i == j) {
                /* No self-connections by default */
                trit_matrix_set(wiring->adjacency, i, j, LNN_CONN_WEAK);
                continue;
            }

            if (randf() < edge_prob) {
                /* Create connection */
                trit_t strength;
                if (config->dale_law_compliant && wiring->neuron_type) {
                    /* Dale's law: neuron type determines sign */
                    strength = wiring->neuron_type[i] ? LNN_CONN_STRONG : LNN_CONN_ABSENT;
                } else {
                    /* Random E/I assignment */
                    strength = (randf() < config->excitatory_ratio)
                              ? LNN_CONN_STRONG : LNN_CONN_ABSENT;
                }
                trit_matrix_set(wiring->adjacency, i, j, strength);
                if (strength == LNN_CONN_STRONG) {
                    wiring->n_excitatory++;
                } else {
                    wiring->n_inhibitory++;
                }
            } else {
                trit_matrix_set(wiring->adjacency, i, j, LNN_CONN_WEAK);
                wiring->n_absent++;
            }
        }
    }

    return LNN_SUCCESS;
}

/**
 * @brief Generate small-world ternary wiring (Watts-Strogatz)
 */
static int generate_small_world_wiring(
    lnn_ternary_wiring_t* wiring,
    const lnn_ternary_wiring_config_t* config
) {
    uint32_t n = config->n_neurons;
    uint32_t k = config->k_neighbors;
    float p = config->rewire_prob;

    /* Initialize ring lattice */
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t offset = 1; offset <= k / 2; offset++) {
            uint32_t j_plus = (i + offset) % n;
            uint32_t j_minus = (i + n - offset) % n;

            trit_t strength = (randf() < config->excitatory_ratio)
                             ? LNN_CONN_STRONG : LNN_CONN_ABSENT;
            trit_matrix_set(wiring->adjacency, i, j_plus, strength);
            trit_matrix_set(wiring->adjacency, i, j_minus, strength);

            if (strength == LNN_CONN_STRONG) {
                wiring->n_excitatory += 2;
            } else {
                wiring->n_inhibitory += 2;
            }
        }
    }

    /* Rewiring phase */
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t offset = 1; offset <= k / 2; offset++) {
            if (randf() < p) {
                uint32_t j_old = (i + offset) % n;

                /* Find new target (not self, not already connected) */
                uint32_t j_new;
                int attempts = 0;
                do {
                    j_new = rand_range(0, n);
                    attempts++;
                } while ((j_new == i ||
                         trit_matrix_get(wiring->adjacency, i, j_new) != LNN_CONN_WEAK)
                        && attempts < 100);

                if (attempts < 100) {
                    /* Remove old, add new */
                    trit_t old_strength = trit_matrix_get(wiring->adjacency, i, j_old);
                    trit_matrix_set(wiring->adjacency, i, j_old, LNN_CONN_WEAK);
                    trit_matrix_set(wiring->adjacency, i, j_new, old_strength);
                }
            }
        }
    }

    return LNN_SUCCESS;
}

/**
 * @brief Generate NCP-style ternary wiring
 */
static int generate_ncp_wiring(
    lnn_ternary_wiring_t* wiring,
    const lnn_ternary_wiring_config_t* config
) {
    uint32_t sens_start = 0;
    uint32_t inter_start = config->n_sensory;
    uint32_t cmd_start = inter_start + config->n_inter;
    uint32_t motor_start = cmd_start + config->n_command;

    /* Assign roles */
    for (uint32_t i = 0; i < config->n_neurons; i++) {
        if (i < inter_start) {
            wiring->roles[i] = LNN_ROLE_SENSORY;
        } else if (i < cmd_start) {
            wiring->roles[i] = LNN_ROLE_INTER;
        } else if (i < motor_start) {
            wiring->roles[i] = LNN_ROLE_COMMAND;
        } else {
            wiring->roles[i] = LNN_ROLE_MOTOR;
        }
    }

    float connect_prob = 1.0f - config->sparsity;

    /* Sensory -> Inter (feedforward) */
    for (uint32_t i = sens_start; i < inter_start; i++) {
        for (uint32_t j = inter_start; j < cmd_start; j++) {
            if (randf() < connect_prob) {
                trit_t s = (randf() < config->excitatory_ratio)
                          ? LNN_CONN_STRONG : LNN_CONN_ABSENT;
                trit_matrix_set(wiring->adjacency, i, j, s);
                if (s == LNN_CONN_STRONG) wiring->n_excitatory++;
                else wiring->n_inhibitory++;
            }
        }
    }

    /* Inter -> Inter (recurrent) */
    for (uint32_t i = inter_start; i < cmd_start; i++) {
        for (uint32_t j = inter_start; j < cmd_start; j++) {
            if (i == j) continue;
            if (randf() < connect_prob * 0.5f) {  /* Sparser recurrence */
                trit_t s = (randf() < config->excitatory_ratio)
                          ? LNN_CONN_STRONG : LNN_CONN_ABSENT;
                trit_matrix_set(wiring->adjacency, i, j, s);
                if (s == LNN_CONN_STRONG) wiring->n_excitatory++;
                else wiring->n_inhibitory++;
            }
        }
    }

    /* Inter -> Command */
    for (uint32_t i = inter_start; i < cmd_start; i++) {
        for (uint32_t j = cmd_start; j < motor_start; j++) {
            if (randf() < connect_prob) {
                trit_t s = (randf() < config->excitatory_ratio)
                          ? LNN_CONN_STRONG : LNN_CONN_ABSENT;
                trit_matrix_set(wiring->adjacency, i, j, s);
                if (s == LNN_CONN_STRONG) wiring->n_excitatory++;
                else wiring->n_inhibitory++;
            }
        }
    }

    /* Command -> Motor */
    for (uint32_t i = cmd_start; i < motor_start; i++) {
        for (uint32_t j = motor_start; j < config->n_neurons; j++) {
            if (randf() < connect_prob) {
                trit_t s = (randf() < config->excitatory_ratio)
                          ? LNN_CONN_STRONG : LNN_CONN_ABSENT;
                trit_matrix_set(wiring->adjacency, i, j, s);
                if (s == LNN_CONN_STRONG) wiring->n_excitatory++;
                else wiring->n_inhibitory++;
            }
        }
    }

    /* Command -> Command (recurrent) */
    for (uint32_t i = cmd_start; i < motor_start; i++) {
        for (uint32_t j = cmd_start; j < motor_start; j++) {
            if (i == j) continue;
            if (randf() < connect_prob * 0.3f) {
                trit_t s = (randf() < config->excitatory_ratio)
                          ? LNN_CONN_STRONG : LNN_CONN_ABSENT;
                trit_matrix_set(wiring->adjacency, i, j, s);
                if (s == LNN_CONN_STRONG) wiring->n_excitatory++;
                else wiring->n_inhibitory++;
            }
        }
    }

    return LNN_SUCCESS;
}

/**
 * @brief Update statistics after wiring generation
 */
static void update_stats(lnn_ternary_wiring_t* wiring) {
    uint32_t total = wiring->n_neurons * wiring->n_neurons;
    uint32_t connected = wiring->n_excitatory + wiring->n_inhibitory;

    wiring->n_absent = total - connected;
    wiring->actual_sparsity = (float)wiring->n_absent / (float)total;

    if (wiring->n_inhibitory > 0) {
        wiring->actual_ei_ratio = (float)wiring->n_excitatory /
                                  (float)wiring->n_inhibitory;
    } else {
        wiring->actual_ei_ratio = (wiring->n_excitatory > 0) ? INFINITY : 1.0f;
    }
}

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

lnn_ternary_wiring_t* lnn_ternary_wiring_create(
    const lnn_ternary_wiring_config_t* config
) {
    /* Guard: validate input */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "NULL config in lnn_ternary_wiring_create");
        return NULL;
    }
    if (config->n_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "n_neurons must be > 0 in lnn_ternary_wiring_create");
        return NULL;
    }

    /* Seed RNG */
    seed_rng(config->seed);

    /* Allocate structure */
    lnn_ternary_wiring_t* wiring = nimcp_malloc(sizeof(lnn_ternary_wiring_t));
    if (!wiring) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(lnn_ternary_wiring_t),
                          "Failed to allocate wiring structure");
        return NULL;
    }
    memset(wiring, 0, sizeof(lnn_ternary_wiring_t));

    wiring->magic = LNN_TERNARY_WIRING_MAGIC;
    wiring->type = config->type;
    wiring->n_neurons = config->n_neurons;
    wiring->dale_law_compliant = config->dale_law_compliant;

    /* Store NCP parameters */
    wiring->n_sensory = config->n_sensory;
    wiring->n_inter = config->n_inter;
    wiring->n_command = config->n_command;
    wiring->n_motor = config->n_motor;

    /* Allocate adjacency matrix */
    wiring->adjacency = trit_matrix_create(
        config->n_neurons, config->n_neurons,
        config->use_packed_storage ? config->pack_mode : TERNARY_PACK_NONE
    );
    if (!wiring->adjacency) {
        nimcp_free(wiring);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_ternary_wiring_create: wiring->adjacency is NULL");
        return NULL;
    }

    /* Initialize to all WEAK (no connections) */
    trit_matrix_fill(wiring->adjacency, LNN_CONN_WEAK);

    /* Dale's law: assign neuron types */
    if (config->dale_law_compliant) {
        wiring->neuron_type = nimcp_malloc(config->n_neurons * sizeof(bool));
        if (!wiring->neuron_type) {
            trit_matrix_destroy(wiring->adjacency);
            nimcp_free(wiring);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_wiring_create: wiring->neuron_type is NULL");
            return NULL;
        }
        /* Assign excitatory/inhibitory randomly based on ratio */
        for (uint32_t i = 0; i < config->n_neurons; i++) {
            wiring->neuron_type[i] = (randf() < config->excitatory_ratio);
        }
    }

    /* Allocate roles for NCP */
    if (config->type == LNN_WIRING_NCP) {
        wiring->roles = nimcp_malloc(config->n_neurons * sizeof(lnn_neuron_role_t));
        if (!wiring->roles) {
            if (wiring->neuron_type) nimcp_free(wiring->neuron_type);
            trit_matrix_destroy(wiring->adjacency);
            nimcp_free(wiring);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_wiring_create: validation failed");
            return NULL;
        }
    }

    /* Generate wiring based on type */
    int result;
    switch (config->type) {
        case LNN_WIRING_RANDOM:
            result = generate_random_wiring(wiring, config);
            break;

        case LNN_WIRING_SMALL_WORLD:
            result = generate_small_world_wiring(wiring, config);
            break;

        case LNN_WIRING_NCP:
            result = generate_ncp_wiring(wiring, config);
            break;

        case LNN_WIRING_FULL:
            /* Full connectivity */
            for (uint32_t i = 0; i < config->n_neurons; i++) {
                for (uint32_t j = 0; j < config->n_neurons; j++) {
                    if (i == j) continue;
                    trit_t s = (randf() < config->excitatory_ratio)
                              ? LNN_CONN_STRONG : LNN_CONN_ABSENT;
                    trit_matrix_set(wiring->adjacency, i, j, s);
                    if (s == LNN_CONN_STRONG) wiring->n_excitatory++;
                    else wiring->n_inhibitory++;
                }
            }
            result = LNN_SUCCESS;
            break;

        default:
            NIMCP_LOGGING_WARN("Unsupported wiring type %d, using random",
                             config->type);
            result = generate_random_wiring(wiring, config);
            break;
    }

    if (result != LNN_SUCCESS) {
        lnn_ternary_wiring_destroy(wiring);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_ternary_wiring_create: validation failed");
        return NULL;
    }

    /* Update statistics */
    update_stats(wiring);

    return wiring;
}

lnn_ternary_wiring_t* lnn_ternary_wiring_from_binary(
    const lnn_wiring_t* wiring,
    float excitatory_ratio,
    uint64_t seed
) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_ternary_wiring_from_binary: wiring is NULL");
        return NULL;
    }

    seed_rng(seed);

    /* Create ternary wiring */
    lnn_ternary_wiring_config_t config;
    lnn_ternary_wiring_config_default(&config);
    config.type = wiring->type;
    config.n_neurons = wiring->n_neurons;
    config.excitatory_ratio = excitatory_ratio;

    lnn_ternary_wiring_t* ternary = nimcp_malloc(sizeof(lnn_ternary_wiring_t));
    if (!ternary) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_wiring_from_binary: ternary is NULL");
        return NULL;
    }
    memset(ternary, 0, sizeof(lnn_ternary_wiring_t));

    ternary->magic = LNN_TERNARY_WIRING_MAGIC;
    ternary->type = wiring->type;
    ternary->n_neurons = wiring->n_neurons;

    ternary->adjacency = trit_matrix_create(
        wiring->n_neurons, wiring->n_neurons, TERNARY_PACK_NONE
    );
    if (!ternary->adjacency) {
        nimcp_free(ternary);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_wiring_from_binary: ternary->adjacency is NULL");
        return NULL;
    }

    /* Initialize to WEAK */
    trit_matrix_fill(ternary->adjacency, LNN_CONN_WEAK);

    /* Convert from CSR to dense ternary */
    for (uint32_t i = 0; i < wiring->n_neurons; i++) {
        uint32_t start = wiring->row_ptr[i];
        uint32_t end = wiring->row_ptr[i + 1];

        for (uint32_t k = start; k < end; k++) {
            uint32_t j = wiring->col_idx[k];
            trit_t strength = (randf() < excitatory_ratio)
                             ? LNN_CONN_STRONG : LNN_CONN_ABSENT;
            trit_matrix_set(ternary->adjacency, i, j, strength);
            if (strength == LNN_CONN_STRONG) {
                ternary->n_excitatory++;
            } else {
                ternary->n_inhibitory++;
            }
        }
    }

    update_stats(ternary);
    return ternary;
}

void lnn_ternary_wiring_destroy(lnn_ternary_wiring_t* wiring) {
    if (!wiring) return;
    if (wiring->magic != LNN_TERNARY_WIRING_MAGIC) return;

    if (wiring->adjacency) trit_matrix_destroy(wiring->adjacency);
    if (wiring->neuron_type) nimcp_free(wiring->neuron_type);
    if (wiring->roles) nimcp_free(wiring->roles);

    wiring->magic = 0;
    nimcp_free(wiring);
}

lnn_ternary_wiring_t* lnn_ternary_wiring_clone(const lnn_ternary_wiring_t* src) {
    if (!src || src->magic != LNN_TERNARY_WIRING_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_ternary_wiring_clone: src is NULL");
        return NULL;
    }

    lnn_ternary_wiring_t* dst = nimcp_malloc(sizeof(lnn_ternary_wiring_t));
    if (!dst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_wiring_clone: dst is NULL");
        return NULL;
    }

    memcpy(dst, src, sizeof(lnn_ternary_wiring_t));

    /* Clone adjacency */
    dst->adjacency = trit_matrix_clone(src->adjacency);
    if (!dst->adjacency) {
        nimcp_free(dst);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_ternary_wiring_clone: dst->adjacency is NULL");
        return NULL;
    }

    /* Clone neuron types */
    if (src->neuron_type) {
        dst->neuron_type = nimcp_malloc(src->n_neurons * sizeof(bool));
        if (!dst->neuron_type) {
            trit_matrix_destroy(dst->adjacency);
            nimcp_free(dst);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_wiring_clone: dst->neuron_type is NULL");
            return NULL;
        }
        memcpy(dst->neuron_type, src->neuron_type, src->n_neurons * sizeof(bool));
    }

    /* Clone roles */
    if (src->roles) {
        dst->roles = nimcp_malloc(src->n_neurons * sizeof(lnn_neuron_role_t));
        if (!dst->roles) {
            if (dst->neuron_type) nimcp_free(dst->neuron_type);
            trit_matrix_destroy(dst->adjacency);
            nimcp_free(dst);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_wiring_clone: validation failed");
            return NULL;
        }
        memcpy(dst->roles, src->roles, src->n_neurons * sizeof(lnn_neuron_role_t));
    }

    return dst;
}

/*=============================================================================
 * Access Functions
 *===========================================================================*/

trit_t lnn_ternary_wiring_get(
    const lnn_ternary_wiring_t* wiring,
    uint32_t from,
    uint32_t to
) {
    if (!wiring || wiring->magic != LNN_TERNARY_WIRING_MAGIC) {
        return LNN_CONN_WEAK;
    }
    if (from >= wiring->n_neurons || to >= wiring->n_neurons) {
        return LNN_CONN_WEAK;
    }

    return trit_matrix_get(wiring->adjacency, from, to);
}

int lnn_ternary_wiring_set(
    lnn_ternary_wiring_t* wiring,
    uint32_t from,
    uint32_t to,
    trit_t strength
) {
    if (!wiring || wiring->magic != LNN_TERNARY_WIRING_MAGIC) {
        return LNN_ERROR_NULL_POINTER;
    }
    if (from >= wiring->n_neurons || to >= wiring->n_neurons) {
        return LNN_ERROR_INVALID_DIMENSION;
    }
    if (!TRIT_IS_VALID(strength)) {
        return LNN_ERROR_INVALID_PARAM;
    }

    /* Update counts */
    trit_t old = trit_matrix_get(wiring->adjacency, from, to);
    if (old == LNN_CONN_STRONG) wiring->n_excitatory--;
    else if (old == LNN_CONN_ABSENT) wiring->n_inhibitory--;
    else wiring->n_absent--;

    if (strength == LNN_CONN_STRONG) wiring->n_excitatory++;
    else if (strength == LNN_CONN_ABSENT) wiring->n_inhibitory++;
    else wiring->n_absent++;

    trit_matrix_set(wiring->adjacency, from, to, strength);
    return LNN_SUCCESS;
}

bool lnn_ternary_wiring_connected(
    const lnn_ternary_wiring_t* wiring,
    uint32_t from,
    uint32_t to
) {
    trit_t conn = lnn_ternary_wiring_get(wiring, from, to);
    return (conn != LNN_CONN_WEAK);
}

uint32_t lnn_ternary_wiring_get_targets(
    const lnn_ternary_wiring_t* wiring,
    uint32_t neuron,
    uint32_t* targets,
    trit_t* strengths,
    uint32_t max_count
) {
    if (!wiring || wiring->magic != LNN_TERNARY_WIRING_MAGIC) return 0;
    if (neuron >= wiring->n_neurons) return 0;
    if (!targets || max_count == 0) return 0;

    uint32_t count = 0;
    for (uint32_t j = 0; j < wiring->n_neurons && count < max_count; j++) {
        trit_t s = trit_matrix_get(wiring->adjacency, neuron, j);
        if (s != LNN_CONN_WEAK) {
            targets[count] = j;
            if (strengths) strengths[count] = s;
            count++;
        }
    }

    return count;
}

uint32_t lnn_ternary_wiring_get_sources(
    const lnn_ternary_wiring_t* wiring,
    uint32_t neuron,
    uint32_t* sources,
    trit_t* strengths,
    uint32_t max_count
) {
    if (!wiring || wiring->magic != LNN_TERNARY_WIRING_MAGIC) return 0;
    if (neuron >= wiring->n_neurons) return 0;
    if (!sources || max_count == 0) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < wiring->n_neurons && count < max_count; i++) {
        trit_t s = trit_matrix_get(wiring->adjacency, i, neuron);
        if (s != LNN_CONN_WEAK) {
            sources[count] = i;
            if (strengths) strengths[count] = s;
            count++;
        }
    }

    return count;
}

/*=============================================================================
 * Statistics
 *===========================================================================*/

void lnn_ternary_wiring_stats(
    const lnn_ternary_wiring_t* wiring,
    uint32_t* n_excitatory,
    uint32_t* n_inhibitory,
    uint32_t* n_absent,
    float* sparsity,
    float* ei_ratio
) {
    if (!wiring || wiring->magic != LNN_TERNARY_WIRING_MAGIC) {
        if (n_excitatory) *n_excitatory = 0;
        if (n_inhibitory) *n_inhibitory = 0;
        if (n_absent) *n_absent = 0;
        if (sparsity) *sparsity = 1.0f;
        if (ei_ratio) *ei_ratio = 1.0f;
        return;
    }

    if (n_excitatory) *n_excitatory = wiring->n_excitatory;
    if (n_inhibitory) *n_inhibitory = wiring->n_inhibitory;
    if (n_absent) *n_absent = wiring->n_absent;
    if (sparsity) *sparsity = wiring->actual_sparsity;
    if (ei_ratio) *ei_ratio = wiring->actual_ei_ratio;
}

uint32_t lnn_ternary_wiring_out_degree(
    const lnn_ternary_wiring_t* wiring,
    uint32_t neuron
) {
    if (!wiring || wiring->magic != LNN_TERNARY_WIRING_MAGIC) return 0;
    if (neuron >= wiring->n_neurons) return 0;

    uint32_t degree = 0;
    for (uint32_t j = 0; j < wiring->n_neurons; j++) {
        if (trit_matrix_get(wiring->adjacency, neuron, j) != LNN_CONN_WEAK) {
            degree++;
        }
    }
    return degree;
}

uint32_t lnn_ternary_wiring_in_degree(
    const lnn_ternary_wiring_t* wiring,
    uint32_t neuron
) {
    if (!wiring || wiring->magic != LNN_TERNARY_WIRING_MAGIC) return 0;
    if (neuron >= wiring->n_neurons) return 0;

    uint32_t degree = 0;
    for (uint32_t i = 0; i < wiring->n_neurons; i++) {
        if (trit_matrix_get(wiring->adjacency, i, neuron) != LNN_CONN_WEAK) {
            degree++;
        }
    }
    return degree;
}

/*=============================================================================
 * Configuration Helpers
 *===========================================================================*/

void lnn_ternary_wiring_config_default(lnn_ternary_wiring_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(lnn_ternary_wiring_config_t));

    config->type = LNN_WIRING_RANDOM;
    config->n_neurons = 100;
    config->sparsity = 0.5f;
    config->excitatory_ratio = 0.8f;  /* 80% excitatory (biological norm) */

    config->k_neighbors = 4;
    config->rewire_prob = 0.1f;
    config->m_edges = 2;

    config->seed = 0;
    config->use_packed_storage = false;
    config->pack_mode = TERNARY_PACK_NONE;
    config->dale_law_compliant = false;
}

int lnn_ternary_wiring_config_validate(const lnn_ternary_wiring_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "NULL config in lnn_ternary_wiring_config_validate");
        return LNN_ERROR_NULL_POINTER;
    }

    if (config->n_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "n_neurons must be > 0");
        return LNN_ERROR_INVALID_CONFIG;
    }

    if (config->sparsity < 0.0f || config->sparsity >= 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "sparsity must be in [0, 1), got %.4f", config->sparsity);
        return LNN_ERROR_INVALID_CONFIG;
    }

    if (config->excitatory_ratio < 0.0f || config->excitatory_ratio > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "excitatory_ratio must be in [0, 1], got %.4f",
                             config->excitatory_ratio);
        return LNN_ERROR_INVALID_CONFIG;
    }

    if (config->type == LNN_WIRING_NCP) {
        uint32_t sum = config->n_sensory + config->n_inter +
                      config->n_command + config->n_motor;
        if (sum != config->n_neurons) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                                 "NCP neuron counts must sum to n_neurons (%u != %u)",
                                 sum, config->n_neurons);
            return LNN_ERROR_INVALID_CONFIG;
        }
    }

    return LNN_SUCCESS;
}

/*=============================================================================
 * Conversion
 *===========================================================================*/

lnn_ternary_matrix_t* lnn_ternary_wiring_to_matrix(
    const lnn_ternary_wiring_t* wiring,
    ternary_pack_mode_t pack_mode
) {
    if (!wiring || wiring->magic != LNN_TERNARY_WIRING_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_ternary_wiring_to_matrix: wiring is NULL");
        return NULL;
    }

    /* Create ternary matrix */
    lnn_ternary_matrix_t* mat = lnn_ternary_matrix_create(
        wiring->n_neurons, wiring->n_neurons, pack_mode, false
    );
    if (!mat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_wiring_to_matrix: mat is NULL");
        return NULL;
    }

    /* Copy adjacency to dense matrix */
    for (uint32_t i = 0; i < wiring->n_neurons; i++) {
        for (uint32_t j = 0; j < wiring->n_neurons; j++) {
            trit_t val = trit_matrix_get(wiring->adjacency, i, j);
            trit_matrix_set(mat->dense, i, j, val);
        }
    }

    /* Update statistics */
    mat->nnz = wiring->n_excitatory + wiring->n_inhibitory;
    mat->sparsity = wiring->actual_sparsity;

    return mat;
}
