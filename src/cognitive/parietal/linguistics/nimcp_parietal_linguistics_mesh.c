/**
 * @file nimcp_parietal_linguistics_mesh.c
 * @brief Linguistics Mesh Coordinator Implementation - Version 2.0.0
 * @version 2.0.0
 * @date 2025-01-31
 *
 * Implements distributed consensus for language processing using:
 * - 12 Mathematical Algorithms (BP, Loopy BP, Consensus, Natural Gradient, etc.)
 * - Free Energy Principle convergence
 * - Byzantine fault tolerance (Trimmed Mean, Krum, Voting)
 * - Full infrastructure integration (BBB, Health, KG, Exception)
 *
 * @author NIMCP Development Team
 */

#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_mesh.h"
#include "core/brain/nimcp_brain_kg.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include "constants/nimcp_timing_constants.h"
#include "constants/nimcp_learning_constants.h"

/* ============================================================================
 * LOG MODULE IDENTIFIER
 * ============================================================================ */

#ifndef LOG_MODULE_LING_MESH
#define LOG_MODULE_LING_MESH "LING_MESH"
#endif

/* ============================================================================
 * INTERNAL CONSTANTS
 * ============================================================================ */

#define MESH_MAGIC                      0x4D455348  /* "MESH" */
#define MESH_MAX_BELIEFS_PER_REQUEST    64
#define MESH_BELIEF_VECTOR_DIM          16
#define MESH_MAX_PENDING_REQUESTS       16
#define MESH_MAX_NEIGHBORS              32

/* Algorithm constants */
#define BP_CONVERGENCE_EPS              NIMCP_EPSILON_LARGE
#define FISHER_MATRIX_SIZE              (MESH_BELIEF_VECTOR_DIM * MESH_BELIEF_VECTOR_DIM)
#define PAGERANK_CONVERGENCE_EPS        NIMCP_EPSILON_NUMERICAL
#define LAPLACIAN_MAX_EIGENVALUE        4.0f   /* Max eigenvalue for step size */

/* ============================================================================
 * INTERNAL TYPES
 * ============================================================================ */

/**
 * @brief Active belief in the mesh
 */
typedef struct {
    linguistics_belief_t belief;            /**< Core belief data */
    float credibility_weight;               /**< Source credibility */
    float pagerank_weight;                  /**< PageRank-derived weight */
    bool propagated;                        /**< Has been gossiped */
    uint32_t propagation_count;             /**< Times propagated */
    bool excluded_bft;                      /**< Excluded by Byzantine tolerance */

    /* Belief propagation messages */
    float messages_in[MESH_MAX_NEIGHBORS][MESH_BELIEF_VECTOR_DIM];  /**< Incoming BP messages */
    float messages_out[MESH_MAX_NEIGHBORS][MESH_BELIEF_VECTOR_DIM]; /**< Outgoing BP messages */
    uint32_t neighbor_count;                /**< Number of neighbors */

    /* Covariance for CI */
    float covariance_diag[MESH_BELIEF_VECTOR_DIM]; /**< Diagonal covariance */

    /* Natural gradient */
    float fisher_diag[MESH_BELIEF_VECTOR_DIM];     /**< Fisher diagonal approx */
    float gradient[MESH_BELIEF_VECTOR_DIM];         /**< Gradient */
} mesh_active_belief_t;

/**
 * @brief Pending async request
 */
typedef struct {
    uint64_t request_id;                    /**< Unique request ID */
    linguistics_request_t request;          /**< Original request */
    linguistics_response_t response;        /**< Building response */
    linguistics_convergence_state_t conv_state; /**< Convergence state */
    uint64_t start_time_ms;                 /**< When request started */
    uint32_t timeout_ms;                    /**< Timeout */
    bool active;                            /**< Is active */
    bool complete;                          /**< Is complete */
} mesh_pending_request_t;

/**
 * @brief Graph Laplacian data
 */
typedef struct {
    float* laplacian;                       /**< L = D - A (NxN) */
    float* degree_matrix;                   /**< D (diagonal) */
    float* adjacency;                       /**< A (NxN) */
    uint32_t size;                          /**< Matrix dimension */
    bool normalized;                        /**< Is normalized Laplacian */
} mesh_laplacian_t;

/**
 * @brief Main mesh coordinator structure
 */
struct linguistics_mesh {
    uint32_t magic;                         /**< Validation magic */

    /* Configuration */
    linguistics_mesh_config_t config;

    /* Participants */
    linguistics_mesh_participant_t participants[LINGUISTICS_MAX_MESH_PARTICIPANTS];
    uint32_t participant_count;

    /* Active beliefs */
    mesh_active_belief_t beliefs[MESH_MAX_BELIEFS_PER_REQUEST];
    uint32_t belief_count;

    /* Pending requests */
    mesh_pending_request_t pending_requests[MESH_MAX_PENDING_REQUESTS];
    uint64_t next_request_id;

    /* Current convergence state */
    linguistics_convergence_state_t current_state;

    /* Statistics (extended internal version) */
    linguistics_mesh_extended_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t creation_time_ms;
    uint64_t last_update_ms;
    uint64_t last_heartbeat_ms;

    /* Graph topology */
    mesh_laplacian_t laplacian;
    float* adjacency_matrix;                /**< NxN adjacency */
    float* pagerank_scores;                 /**< PageRank scores per participant */
    bool topology_dirty;                    /**< Topology needs recalculation */

    /* Infrastructure handles */
    bbb_system_t bbb;                       /**< BBB system handle */
    health_monitor_t health;                /**< Health monitor handle */
    brain_kg_t* kg;                         /**< Brain KG handle */
    brain_kg_node_id_t kg_node_id;          /**< Our KG node ID */

    /* Bio-async state */
    bool bio_async_connected;
    bool health_connected;
    bool kg_connected;
    bool heartbeat_running;

    /* Lyapunov stability */
    float lyapunov_P[MESH_BELIEF_VECTOR_DIM]; /**< Lyapunov P matrix diagonal */
    float prev_lyapunov;                     /**< Previous Lyapunov value */
};

/* Thread-local error message */
static _Thread_local char g_mesh_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/* ============================================================================
 * ERROR HANDLING HELPERS
 * ============================================================================ */

/**
 * @brief Set error message
 */
static void mesh_set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_mesh_error, sizeof(g_mesh_error), fmt, args);
    va_end(args);
}

/**
 * @brief Validate mesh handle
 */
static bool mesh_is_valid(const linguistics_mesh_t* mesh) {
    return mesh && mesh->magic == MESH_MAGIC;
}

/**
 * @brief Record exception to stats
 */
static void mesh_record_exception(linguistics_mesh_t* mesh) {
    if (mesh && mesh->magic == MESH_MAGIC) {
        mesh->stats.exception_count++;
    }
}

/**
 * @brief Record health operation
 */
static void mesh_record_health_operation(linguistics_mesh_t* mesh, float latency_ms, bool success) {
    if (!mesh || !mesh->health_connected) return;

    mesh->stats.health_checks++;
    if (!success) {
        mesh->stats.anomalies_detected++;
    }

    /* Update health score based on recent success rate */
    float success_rate = (float)mesh->stats.successful_convergences /
                        (float)(mesh->stats.total_requests + 1);
    mesh->stats.current_health_score = success_rate * 100.0f;
}

/* ============================================================================
 * BBB VALIDATION HELPERS
 * ============================================================================ */

/**
 * @brief Validate input string via BBB
 */
static int mesh_validate_input_string(linguistics_mesh_t* mesh, const char* str, const char* field_name) {
    if (!str) {
        mesh_set_error("%s is NULL", field_name);
        if (mesh) mesh->stats.bbb_rejections++;
        return LING_ERR_NULL_POINTER;
    }

    size_t len = strlen(str);
    if (len == 0) {
        mesh_set_error("%s is empty", field_name);
        if (mesh) mesh->stats.bbb_rejections++;
        return LING_ERR_INVALID_PARAM;
    }

    if (len > LING_MESH_MAX_INPUT_LENGTH) {
        mesh_set_error("%s exceeds max length (%zu > %d)",
                      field_name, len, LING_MESH_MAX_INPUT_LENGTH);
        if (mesh) mesh->stats.bbb_rejections++;
        return LING_ERR_BUFFER_OVERFLOW;
    }

    /* Check for null bytes in middle of string */
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\0') {
            mesh_set_error("%s contains embedded null byte", field_name);
            if (mesh) mesh->stats.bbb_rejections++;
            return LING_ERR_INVALID_PARAM;
        }
    }

    return LING_ERR_OK;
}

/**
 * @brief Validate request via BBB
 */
static int mesh_validate_request(linguistics_mesh_t* mesh, const linguistics_request_t* request) {
    if (!request) {
        mesh_set_error("Request is NULL");
        mesh->stats.bbb_rejections++;
        return LING_ERR_NULL_POINTER;
    }

    /* Validate request type */
    if (request->type < 0 || request->type >= LING_REQUEST_TYPE_COUNT) {
        mesh_set_error("Invalid request type: %d", request->type);
        mesh->stats.bbb_rejections++;
        return LING_ERR_INVALID_PARAM;
    }

    /* Validate input string if present */
    if (request->input_word[0] != '\0') {
        int ret = mesh_validate_input_string(mesh, request->input_word, "request.input_word");
        if (ret != LING_ERR_OK) {
            return ret;
        }
    }

    return LING_ERR_OK;
}

/* ============================================================================
 * PARTICIPANT HELPERS
 * ============================================================================ */

/**
 * @brief Find participant by module ID
 */
static linguistics_mesh_participant_t* mesh_find_participant(
    linguistics_mesh_t* mesh,
    uint32_t module_id
) {
    for (uint32_t i = 0; i < mesh->participant_count; i++) {
        if (mesh->participants[i].module_id == module_id && mesh->participants[i].active) {
            return &mesh->participants[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_find_participant: validation failed");
    return NULL;
}

/**
 * @brief Find pending request by ID
 */
static mesh_pending_request_t* mesh_find_pending_request(
    linguistics_mesh_t* mesh,
    uint64_t request_id
) {
    for (uint32_t i = 0; i < MESH_MAX_PENDING_REQUESTS; i++) {
        if (mesh->pending_requests[i].active &&
            mesh->pending_requests[i].request_id == request_id) {
            return &mesh->pending_requests[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_find_pending_request: operation failed");
    return NULL;
}

/* ============================================================================
 * MATHEMATICAL ALGORITHM HELPERS
 * ============================================================================ */

/**
 * @brief Compute precision-weighted average of beliefs (basic consensus)
 */
static void mesh_compute_collective_belief(
    linguistics_mesh_t* mesh,
    linguistics_collective_belief_t* collective
) {
    memset(collective, 0, sizeof(*collective));

    if (mesh->belief_count == 0) {
        return;
    }

    /* Compute total precision for weighting (excluding BFT-excluded) */
    float total_precision = 0.0f;
    uint32_t active_count = 0;

    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (mesh->beliefs[i].excluded_bft) continue;

        float weight = mesh->beliefs[i].belief.precision *
                      mesh->beliefs[i].credibility_weight;

        /* Apply PageRank weighting if available */
        if (mesh->pagerank_scores && i < mesh->participant_count) {
            weight *= mesh->beliefs[i].pagerank_weight;
        }

        total_precision += weight;
        active_count++;
    }

    if (total_precision < 1e-6f) {
        return;
    }

    /* Copy topic from first non-excluded belief */
    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (!mesh->beliefs[i].excluded_bft) {
            strncpy(collective->topic, mesh->beliefs[i].belief.topic,
                    sizeof(collective->topic) - 1);
            collective->vector_dim = mesh->beliefs[i].belief.vector_dim;
            break;
        }
    }

    /* Precision-weighted average */
    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (mesh->beliefs[i].excluded_bft) continue;

        float weight = (mesh->beliefs[i].belief.precision *
                       mesh->beliefs[i].credibility_weight);
        if (mesh->pagerank_scores && i < mesh->participant_count) {
            weight *= mesh->beliefs[i].pagerank_weight;
        }
        weight /= total_precision;

        collective->collective_certainty +=
            weight * mesh->beliefs[i].belief.certainty;

        for (uint32_t j = 0; j < collective->vector_dim && j < MESH_BELIEF_VECTOR_DIM; j++) {
            collective->belief_vector[j] +=
                weight * mesh->beliefs[i].belief.belief_vector[j];
        }
    }

    collective->collective_precision = total_precision / (active_count > 0 ? active_count : 1);
    collective->contributing_participants = active_count;
    collective->byzantine_excluded = mesh->belief_count - active_count;
}

/**
 * @brief Compute agreement score (cosine similarity of beliefs)
 */
static float mesh_compute_agreement(linguistics_mesh_t* mesh) {
    if (mesh->belief_count < 2) {
        return 1.0f;  /* Single belief = perfect agreement */
    }

    /* Get collective belief */
    linguistics_collective_belief_t collective;
    mesh_compute_collective_belief(mesh, &collective);

    /* Compute average cosine similarity to collective */
    float total_similarity = 0.0f;
    float collective_norm = 0.0f;
    uint32_t active_count = 0;

    for (uint32_t j = 0; j < collective.vector_dim && j < MESH_BELIEF_VECTOR_DIM; j++) {
        collective_norm += collective.belief_vector[j] * collective.belief_vector[j];
    }
    collective_norm = sqrtf(collective_norm + 1e-6f);

    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (mesh->beliefs[i].excluded_bft) continue;

        float dot = 0.0f;
        float belief_norm = 0.0f;

        for (uint32_t j = 0; j < collective.vector_dim && j < MESH_BELIEF_VECTOR_DIM; j++) {
            dot += mesh->beliefs[i].belief.belief_vector[j] *
                   collective.belief_vector[j];
            belief_norm += mesh->beliefs[i].belief.belief_vector[j] *
                          mesh->beliefs[i].belief.belief_vector[j];
        }

        belief_norm = sqrtf(belief_norm + 1e-6f);
        float similarity = dot / (belief_norm * collective_norm + 1e-6f);

        /* Precision-weight the similarity */
        total_similarity += similarity * mesh->beliefs[i].belief.precision;
        active_count++;
    }

    /* Normalize */
    float total_precision = 0.0f;
    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (!mesh->beliefs[i].excluded_bft) {
            total_precision += mesh->beliefs[i].belief.precision;
        }
    }

    return total_similarity / (total_precision + 1e-6f);
}

/**
 * @brief Compute free energy (simplified: variance of beliefs)
 */
static float mesh_compute_free_energy(linguistics_mesh_t* mesh) {
    if (mesh->belief_count < 2) {
        return 0.0f;
    }

    /* Free energy ≈ average prediction error variance */
    linguistics_collective_belief_t collective;
    mesh_compute_collective_belief(mesh, &collective);

    float total_error = 0.0f;
    uint32_t active_count = 0;

    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (mesh->beliefs[i].excluded_bft) continue;

        float error_sq = 0.0f;

        for (uint32_t j = 0; j < collective.vector_dim && j < MESH_BELIEF_VECTOR_DIM; j++) {
            float err = mesh->beliefs[i].belief.belief_vector[j] -
                       collective.belief_vector[j];
            error_sq += err * err;
        }

        /* Precision-weighted error */
        total_error += error_sq * mesh->beliefs[i].belief.precision;
        active_count++;
    }

    return (active_count > 0) ? (total_error / active_count) : 0.0f;
}

/**
 * @brief Compute KL divergence between beliefs
 * D_KL(P||Q) = Σ P(x) * log(P(x)/Q(x))
 */
static float mesh_compute_kl_internal(linguistics_mesh_t* mesh) {
    if (mesh->belief_count < 2) {
        return 0.0f;
    }

    float total_kl = 0.0f;
    uint32_t pairs = 0;

    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (mesh->beliefs[i].excluded_bft) continue;

        for (uint32_t j = i + 1; j < mesh->belief_count; j++) {
            if (mesh->beliefs[j].excluded_bft) continue;

            float kl = 0.0f;
            uint32_t dim = mesh->beliefs[i].belief.vector_dim;
            if (dim > MESH_BELIEF_VECTOR_DIM) dim = MESH_BELIEF_VECTOR_DIM;

            for (uint32_t k = 0; k < dim; k++) {
                /* Treat belief vectors as probability distributions (normalized) */
                float p = fabsf(mesh->beliefs[i].belief.belief_vector[k]) + 1e-6f;
                float q = fabsf(mesh->beliefs[j].belief.belief_vector[k]) + 1e-6f;

                kl += p * logf(p / q);
            }

            total_kl += fabsf(kl);  /* Use absolute value for symmetric metric */
            pairs++;
        }
    }

    return (pairs > 0) ? (total_kl / pairs) : 0.0f;
}

/**
 * @brief Compute Lyapunov function V(x) = x^T * P * x
 */
static float mesh_compute_lyapunov_internal(linguistics_mesh_t* mesh) {
    linguistics_collective_belief_t collective;
    mesh_compute_collective_belief(mesh, &collective);

    float V = 0.0f;

    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (mesh->beliefs[i].excluded_bft) continue;

        /* Compute deviation from collective */
        for (uint32_t j = 0; j < collective.vector_dim && j < MESH_BELIEF_VECTOR_DIM; j++) {
            float x = mesh->beliefs[i].belief.belief_vector[j] -
                     collective.belief_vector[j];

            /* V = x^T * P * x, using diagonal P */
            V += x * mesh->lyapunov_P[j] * x;
        }
    }

    return V;
}

/* ============================================================================
 * BELIEF PROPAGATION (SUM-PRODUCT) ALGORITHM
 * ============================================================================ */

/**
 * @brief Initialize BP messages
 */
static void mesh_bp_init_messages(linguistics_mesh_t* mesh) {
    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        mesh->beliefs[i].neighbor_count = 0;

        /* Initialize messages to uniform */
        for (uint32_t n = 0; n < MESH_MAX_NEIGHBORS; n++) {
            for (uint32_t d = 0; d < MESH_BELIEF_VECTOR_DIM; d++) {
                mesh->beliefs[i].messages_in[n][d] = 1.0f / MESH_BELIEF_VECTOR_DIM;
                mesh->beliefs[i].messages_out[n][d] = 1.0f / MESH_BELIEF_VECTOR_DIM;
            }
        }

        /* Set neighbor count based on topology (default: fully connected) */
        mesh->beliefs[i].neighbor_count = mesh->belief_count - 1;
        if (mesh->beliefs[i].neighbor_count > MESH_MAX_NEIGHBORS) {
            mesh->beliefs[i].neighbor_count = MESH_MAX_NEIGHBORS;
        }
    }
}

/**
 * @brief BP message passing step
 */
static float mesh_bp_step_internal(linguistics_mesh_t* mesh) {
    float total_change = 0.0f;

    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (mesh->beliefs[i].excluded_bft) continue;

        uint32_t dim = mesh->beliefs[i].belief.vector_dim;
        if (dim > MESH_BELIEF_VECTOR_DIM) dim = MESH_BELIEF_VECTOR_DIM;

        /* Compute outgoing messages to each neighbor */
        uint32_t n_idx = 0;
        for (uint32_t j = 0; j < mesh->belief_count && n_idx < mesh->beliefs[i].neighbor_count; j++) {
            if (i == j || mesh->beliefs[j].excluded_bft) continue;

            /* Message from i to j: product of local belief and incoming messages except from j */
            float new_message[MESH_BELIEF_VECTOR_DIM];
            float msg_sum = 0.0f;

            for (uint32_t d = 0; d < dim; d++) {
                new_message[d] = mesh->beliefs[i].belief.belief_vector[d];

                /* Multiply by all incoming messages except from j */
                uint32_t k_idx = 0;
                for (uint32_t k = 0; k < mesh->belief_count && k_idx < MESH_MAX_NEIGHBORS; k++) {
                    if (k == i || k == j || mesh->beliefs[k].excluded_bft) continue;
                    new_message[d] *= mesh->beliefs[i].messages_in[k_idx][d];
                    k_idx++;
                }

                msg_sum += new_message[d];
            }

            /* Normalize message */
            if (msg_sum > 1e-6f) {
                for (uint32_t d = 0; d < dim; d++) {
                    new_message[d] /= msg_sum;
                }
            }

            /* Apply damping for loopy BP */
            float damping = mesh->config.alg_config.bp_damping;
            for (uint32_t d = 0; d < dim; d++) {
                float old_val = mesh->beliefs[i].messages_out[n_idx][d];
                float new_val = damping * old_val + (1.0f - damping) * new_message[d];

                total_change += fabsf(new_val - old_val);
                mesh->beliefs[i].messages_out[n_idx][d] = new_val;
            }

            n_idx++;
        }
    }

    /* Update incoming messages (transpose of outgoing) */
    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (mesh->beliefs[i].excluded_bft) continue;

        uint32_t n_idx = 0;
        for (uint32_t j = 0; j < mesh->belief_count && n_idx < MESH_MAX_NEIGHBORS; j++) {
            if (i == j || mesh->beliefs[j].excluded_bft) continue;

            /* Find j's message to i */
            uint32_t j_idx = 0;
            for (uint32_t k = 0; k < mesh->belief_count && j_idx < MESH_MAX_NEIGHBORS; k++) {
                if (k == j) continue;
                if (k == i) {
                    /* Copy j's outgoing message to i's incoming slot */
                    memcpy(mesh->beliefs[i].messages_in[n_idx],
                           mesh->beliefs[j].messages_out[j_idx],
                           sizeof(float) * MESH_BELIEF_VECTOR_DIM);
                    break;
                }
                if (!mesh->beliefs[k].excluded_bft) j_idx++;
            }
            n_idx++;
        }
    }

    return total_change;
}

/**
 * @brief Compute BP marginals (final beliefs)
 */
static void mesh_bp_compute_marginals(linguistics_mesh_t* mesh) {
    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (mesh->beliefs[i].excluded_bft) continue;

        uint32_t dim = mesh->beliefs[i].belief.vector_dim;
        if (dim > MESH_BELIEF_VECTOR_DIM) dim = MESH_BELIEF_VECTOR_DIM;

        /* Marginal = local belief * product of all incoming messages */
        float marginal[MESH_BELIEF_VECTOR_DIM];
        float marginal_sum = 0.0f;

        for (uint32_t d = 0; d < dim; d++) {
            marginal[d] = mesh->beliefs[i].belief.belief_vector[d];

            for (uint32_t n = 0; n < mesh->beliefs[i].neighbor_count; n++) {
                marginal[d] *= mesh->beliefs[i].messages_in[n][d];
            }

            marginal_sum += marginal[d];
        }

        /* Normalize and update belief */
        if (marginal_sum > 1e-6f) {
            for (uint32_t d = 0; d < dim; d++) {
                mesh->beliefs[i].belief.belief_vector[d] = marginal[d] / (fabsf(marginal_sum) > 1e-7f ? marginal_sum : 1e-7f);
            }
        }
    }
}

/* ============================================================================
 * NATURAL GRADIENT DESCENT
 * ============================================================================ */

/**
 * @brief Compute Fisher information matrix (diagonal approximation)
 */
static void mesh_compute_fisher_diagonal(linguistics_mesh_t* mesh) {
    linguistics_collective_belief_t collective;
    mesh_compute_collective_belief(mesh, &collective);

    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (mesh->beliefs[i].excluded_bft) continue;

        uint32_t dim = mesh->beliefs[i].belief.vector_dim;
        if (dim > MESH_BELIEF_VECTOR_DIM) dim = MESH_BELIEF_VECTOR_DIM;

        for (uint32_t d = 0; d < dim; d++) {
            /* Fisher diagonal ≈ (gradient)^2 / p (for exponential family) */
            float grad = collective.belief_vector[d] -
                        mesh->beliefs[i].belief.belief_vector[d];
            float p = fabsf(mesh->beliefs[i].belief.belief_vector[d]) + 1e-6f;

            mesh->beliefs[i].fisher_diag[d] = (grad * grad) / p +
                                              mesh->config.alg_config.fisher_epsilon;
            mesh->beliefs[i].gradient[d] = grad;
        }
    }
}

/**
 * @brief Natural gradient step: θ_{t+1} = θ_t - η * F^{-1} * ∇L(θ)
 */
static float mesh_natural_gradient_step_internal(linguistics_mesh_t* mesh) {
    mesh_compute_fisher_diagonal(mesh);

    float total_change = 0.0f;
    float lr = mesh->config.belief_learning_rate;

    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (mesh->beliefs[i].excluded_bft) continue;

        uint32_t dim = mesh->beliefs[i].belief.vector_dim;
        if (dim > MESH_BELIEF_VECTOR_DIM) dim = MESH_BELIEF_VECTOR_DIM;

        for (uint32_t d = 0; d < dim; d++) {
            /* Natural gradient: F^{-1} * grad */
            float nat_grad = mesh->beliefs[i].gradient[d] / mesh->beliefs[i].fisher_diag[d];

            /* Precision-weighted update */
            float update = lr * mesh->beliefs[i].belief.precision * nat_grad;

            total_change += fabsf(update);
            mesh->beliefs[i].belief.belief_vector[d] += update;
        }
    }

    return total_change;
}

/* ============================================================================
 * GRAPH LAPLACIAN CONSENSUS
 * ============================================================================ */

/**
 * @brief Initialize Laplacian from adjacency matrix
 */
static void mesh_init_laplacian(linguistics_mesh_t* mesh) {
    uint32_t n = mesh->participant_count;
    if (n == 0) return;

    /* Allocate if needed */
    if (!mesh->laplacian.laplacian) {
        mesh->laplacian.laplacian = nimcp_calloc(n * n, sizeof(float));
        if (!mesh->laplacian.laplacian) return;
        mesh->laplacian.degree_matrix = nimcp_calloc(n, sizeof(float));
        if (!mesh->laplacian.degree_matrix) {
            nimcp_free(mesh->laplacian.laplacian);
            mesh->laplacian.laplacian = NULL;
            return;
        }
        mesh->laplacian.adjacency = nimcp_calloc(n * n, sizeof(float));
        if (!mesh->laplacian.adjacency) {
            nimcp_free(mesh->laplacian.laplacian);
            mesh->laplacian.laplacian = NULL;
            nimcp_free(mesh->laplacian.degree_matrix);
            mesh->laplacian.degree_matrix = NULL;
            return;
        }
        mesh->laplacian.size = n;
    }

    /* Default: fully connected with uniform weights */
    if (!mesh->adjacency_matrix) {
        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t j = 0; j < n; j++) {
                if (i != j) {
                    mesh->laplacian.adjacency[i * n + j] = 1.0f / (n - 1);
                }
            }
        }
    } else {
        memcpy(mesh->laplacian.adjacency, mesh->adjacency_matrix, n * n * sizeof(float));
    }

    /* Compute degree matrix D */
    for (uint32_t i = 0; i < n; i++) {
        mesh->laplacian.degree_matrix[i] = 0.0f;
        for (uint32_t j = 0; j < n; j++) {
            mesh->laplacian.degree_matrix[i] += mesh->laplacian.adjacency[i * n + j];
        }
    }

    /* Compute Laplacian L = D - A */
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            if (i == j) {
                mesh->laplacian.laplacian[i * n + j] = mesh->laplacian.degree_matrix[i];
            } else {
                mesh->laplacian.laplacian[i * n + j] = -mesh->laplacian.adjacency[i * n + j];
            }
        }
    }

    /* Normalize if configured */
    if (mesh->config.alg_config.laplacian_use_normalized) {
        for (uint32_t i = 0; i < n; i++) {
            float d_i = mesh->laplacian.degree_matrix[i];
            if (d_i > 1e-6f) {
                float inv_sqrt_d_i = 1.0f / sqrtf(d_i);
                for (uint32_t j = 0; j < n; j++) {
                    float d_j = mesh->laplacian.degree_matrix[j];
                    if (d_j > 1e-6f) {
                        float inv_sqrt_d_j = 1.0f / sqrtf(d_j);
                        mesh->laplacian.laplacian[i * n + j] *=
                            inv_sqrt_d_i * inv_sqrt_d_j;
                    }
                }
            }
        }
    }

    mesh->laplacian.normalized = mesh->config.alg_config.laplacian_use_normalized;
    mesh->topology_dirty = false;
}

/**
 * @brief Laplacian consensus step: ẋ = -L * x
 */
static float mesh_laplacian_step_internal(linguistics_mesh_t* mesh) {
    if (mesh->topology_dirty || !mesh->laplacian.laplacian) {
        mesh_init_laplacian(mesh);
    }

    uint32_t n = mesh->belief_count;
    if (n == 0) return 0.0f;

    float total_change = 0.0f;
    float step_size = mesh->config.alg_config.laplacian_step_size;

    /* For each dimension, apply Laplacian update */
    for (uint32_t d = 0; d < MESH_BELIEF_VECTOR_DIM; d++) {
        /* Collect x_d from all beliefs */
        float x[MESH_MAX_BELIEFS_PER_REQUEST];
        uint32_t idx = 0;

        for (uint32_t i = 0; i < mesh->belief_count; i++) {
            if (!mesh->beliefs[i].excluded_bft && d < mesh->beliefs[i].belief.vector_dim) {
                x[idx++] = mesh->beliefs[i].belief.belief_vector[d];
            }
        }

        if (idx < 2) continue;

        /* Compute -L * x */
        float Lx[MESH_MAX_BELIEFS_PER_REQUEST];
        for (uint32_t i = 0; i < idx && i < mesh->laplacian.size; i++) {
            Lx[i] = 0.0f;
            for (uint32_t j = 0; j < idx && j < mesh->laplacian.size; j++) {
                Lx[i] += mesh->laplacian.laplacian[i * mesh->laplacian.size + j] * x[j];
            }
        }

        /* Update: x_{t+1} = x_t - step_size * L * x_t */
        idx = 0;
        for (uint32_t i = 0; i < mesh->belief_count; i++) {
            if (!mesh->beliefs[i].excluded_bft && d < mesh->beliefs[i].belief.vector_dim) {
                float update = -step_size * Lx[idx];
                total_change += fabsf(update);
                mesh->beliefs[i].belief.belief_vector[d] += update;
                idx++;
            }
        }
    }

    return total_change;
}

/* ============================================================================
 * COVARIANCE INTERSECTION
 * ============================================================================ */

/**
 * @brief Covariance Intersection fusion
 * Optimal when correlations are unknown
 */
static int mesh_covariance_intersection_internal(
    linguistics_mesh_t* mesh,
    linguistics_collective_belief_t* collective
) {
    memset(collective, 0, sizeof(*collective));

    uint32_t active_count = 0;
    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (!mesh->beliefs[i].excluded_bft) active_count++;
    }

    if (active_count == 0) {
        return LING_ERR_MESH_NOT_READY;
    }

    /* Get vector dim from first active belief */
    uint32_t dim = 0;
    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (!mesh->beliefs[i].excluded_bft) {
            dim = mesh->beliefs[i].belief.vector_dim;
            if (dim > MESH_BELIEF_VECTOR_DIM) dim = MESH_BELIEF_VECTOR_DIM;
            strncpy(collective->topic, mesh->beliefs[i].belief.topic,
                    sizeof(collective->topic) - 1);
            break;
        }
    }

    collective->vector_dim = dim;

    /* CI formula: P_c^{-1} = Σ ω_i * P_i^{-1}
     * For diagonal: p_c_d^{-1} = Σ ω_i * p_i_d^{-1}
     */
    float omega = mesh->config.alg_config.ci_omega;

    /* Initialize covariances from precision */
    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (mesh->beliefs[i].excluded_bft) continue;

        /* Convert precision to covariance (diagonal) */
        float inv_prec = 1.0f / (mesh->beliefs[i].belief.precision + 1e-6f);
        for (uint32_t d = 0; d < dim; d++) {
            mesh->beliefs[i].covariance_diag[d] = inv_prec;
        }
    }

    /* Compute fused covariance */
    float fused_cov_inv[MESH_BELIEF_VECTOR_DIM] = {0};
    float fused_mean_weighted[MESH_BELIEF_VECTOR_DIM] = {0};

    float weight_per_belief = omega / (active_count > 0 ? active_count : 1);

    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (mesh->beliefs[i].excluded_bft) continue;

        for (uint32_t d = 0; d < dim; d++) {
            float cov_inv = 1.0f / (mesh->beliefs[i].covariance_diag[d] + 1e-6f);

            fused_cov_inv[d] += weight_per_belief * cov_inv;
            fused_mean_weighted[d] += weight_per_belief * cov_inv *
                                      mesh->beliefs[i].belief.belief_vector[d];
        }
    }

    /* Fused mean and covariance */
    float trace_cov = 0.0f;
    for (uint32_t d = 0; d < dim; d++) {
        float fused_cov = 1.0f / (fused_cov_inv[d] + 1e-6f);
        collective->belief_vector[d] = fused_mean_weighted[d] * fused_cov;
        trace_cov += fused_cov;
    }

    collective->covariance_trace = trace_cov;
    collective->collective_precision = (dim > 0) ? (dim / trace_cov) : 1.0f;
    collective->contributing_participants = active_count;

    /* Compute certainty as inverse of average uncertainty */
    collective->collective_certainty = 1.0f / (1.0f + trace_cov / dim);

    return LING_ERR_OK;
}

/* ============================================================================
 * PAGERANK FOR CREDIBILITY
 * ============================================================================ */

/**
 * @brief Update PageRank credibility scores
 */
static int mesh_update_pagerank_internal(linguistics_mesh_t* mesh) {
    uint32_t n = mesh->participant_count;
    if (n == 0) return LING_ERR_OK;

    /* Allocate PageRank scores if needed */
    if (!mesh->pagerank_scores) {
        mesh->pagerank_scores = nimcp_calloc(n, sizeof(float));
        if (!mesh->pagerank_scores) return -1;
    }

    /* Initialize uniform */
    for (uint32_t i = 0; i < n; i++) {
        mesh->pagerank_scores[i] = 1.0f / (n > 0 ? n : 1);
    }

    float damping = mesh->config.alg_config.pagerank_damping;
    uint32_t max_iter = mesh->config.alg_config.pagerank_iterations;

    /* Build transition matrix from adjacency */
    if (!mesh->adjacency_matrix) {
        /* Default: uniform transition */
        for (uint32_t iter = 0; iter < max_iter; iter++) {
            float new_scores[LINGUISTICS_MAX_MESH_PARTICIPANTS];
            float uniform = (1.0f - damping) / (n > 0 ? n : 1);

            for (uint32_t i = 0; i < n; i++) {
                new_scores[i] = uniform;

                /* Sum contributions from neighbors */
                for (uint32_t j = 0; j < n; j++) {
                    if (j != i && mesh->participants[j].active) {
                        new_scores[i] += damping * mesh->pagerank_scores[j] / (n - 1);
                    }
                }
            }

            /* Check convergence */
            float diff = 0.0f;
            for (uint32_t i = 0; i < n; i++) {
                diff += fabsf(new_scores[i] - mesh->pagerank_scores[i]);
                mesh->pagerank_scores[i] = new_scores[i];
            }

            if (diff < PAGERANK_CONVERGENCE_EPS) break;
        }
    } else {
        /* Use adjacency matrix */
        for (uint32_t iter = 0; iter < max_iter; iter++) {
            float new_scores[LINGUISTICS_MAX_MESH_PARTICIPANTS];
            float uniform = (1.0f - damping) / (n > 0 ? n : 1);

            for (uint32_t i = 0; i < n; i++) {
                new_scores[i] = uniform;

                for (uint32_t j = 0; j < n; j++) {
                    float out_degree = 0.0f;
                    for (uint32_t k = 0; k < n; k++) {
                        out_degree += mesh->adjacency_matrix[j * n + k];
                    }

                    if (out_degree > 1e-6f) {
                        new_scores[i] += damping * mesh->pagerank_scores[j] *
                                        mesh->adjacency_matrix[j * n + i] / out_degree;
                    }
                }
            }

            float diff = 0.0f;
            for (uint32_t i = 0; i < n; i++) {
                diff += fabsf(new_scores[i] - mesh->pagerank_scores[i]);
                mesh->pagerank_scores[i] = new_scores[i];
            }

            if (diff < PAGERANK_CONVERGENCE_EPS) break;
        }
    }

    /* Update participant PageRank weights */
    for (uint32_t i = 0; i < n; i++) {
        mesh->participants[i].pagerank_score = mesh->pagerank_scores[i];
    }

    /* Update belief PageRank weights */
    for (uint32_t i = 0; i < mesh->belief_count && i < n; i++) {
        mesh->beliefs[i].pagerank_weight = mesh->pagerank_scores[i];
    }

    return LING_ERR_OK;
}

/* ============================================================================
 * BYZANTINE FAULT TOLERANCE
 * ============================================================================ */

/**
 * @brief Compare function for qsort
 */
static int mesh_bft_compare(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

/**
 * @brief Apply trimmed mean/median Byzantine tolerance
 */
static uint32_t mesh_apply_trimmed_bft(linguistics_mesh_t* mesh) {
    if (mesh->belief_count < 3) {
        return 0;  /* Need at least 3 beliefs for trimming */
    }

    float trim_fraction = mesh->config.alg_config.bft_trim_fraction;
    uint32_t trim_count = (uint32_t)(mesh->belief_count * trim_fraction);
    if (trim_count == 0) trim_count = 1;

    uint32_t excluded = 0;

    /* For each dimension, identify outliers */
    for (uint32_t d = 0; d < MESH_BELIEF_VECTOR_DIM; d++) {
        float values[MESH_MAX_BELIEFS_PER_REQUEST];
        uint32_t indices[MESH_MAX_BELIEFS_PER_REQUEST];
        uint32_t count = 0;

        for (uint32_t i = 0; i < mesh->belief_count; i++) {
            if (mesh->beliefs[i].excluded_bft) continue;
            if (d >= mesh->beliefs[i].belief.vector_dim) continue;

            values[count] = mesh->beliefs[i].belief.belief_vector[d];
            indices[count] = i;
            count++;
        }

        if (count <= 2 * trim_count) continue;

        /* Sort values (simple bubble sort for small arrays) */
        for (uint32_t i = 0; i < count - 1; i++) {
            for (uint32_t j = 0; j < count - i - 1; j++) {
                if (values[j] > values[j + 1]) {
                    float tmp_v = values[j];
                    values[j] = values[j + 1];
                    values[j + 1] = tmp_v;

                    uint32_t tmp_i = indices[j];
                    indices[j] = indices[j + 1];
                    indices[j + 1] = tmp_i;
                }
            }
        }

        /* Mark lowest and highest as outliers */
        for (uint32_t i = 0; i < trim_count && i < count; i++) {
            if (!mesh->beliefs[indices[i]].excluded_bft) {
                mesh->beliefs[indices[i]].excluded_bft = true;
                excluded++;
            }
            if (!mesh->beliefs[indices[count - 1 - i]].excluded_bft) {
                mesh->beliefs[indices[count - 1 - i]].excluded_bft = true;
                excluded++;
            }
        }
    }

    mesh->stats.trimmed_beliefs += excluded;
    return excluded;
}

/**
 * @brief Apply Krum algorithm for Byzantine tolerance
 * Selects belief closest to most other beliefs
 */
static uint32_t mesh_apply_krum_bft(linguistics_mesh_t* mesh) {
    uint32_t n = mesh->belief_count;
    uint32_t f = mesh->config.alg_config.bft_max_faulty;

    if (n <= 2 * f + 1) {
        return 0;  /* Not enough beliefs for Krum */
    }

    /* Compute pairwise distances */
    float distances[MESH_MAX_BELIEFS_PER_REQUEST][MESH_MAX_BELIEFS_PER_REQUEST];

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i; j < n; j++) {
            if (i == j) {
                distances[i][j] = 0.0f;
                continue;
            }

            float dist = 0.0f;
            uint32_t dim = mesh->beliefs[i].belief.vector_dim;
            if (dim > MESH_BELIEF_VECTOR_DIM) dim = MESH_BELIEF_VECTOR_DIM;

            for (uint32_t d = 0; d < dim; d++) {
                float diff = mesh->beliefs[i].belief.belief_vector[d] -
                            mesh->beliefs[j].belief.belief_vector[d];
                dist += diff * diff;
            }

            distances[i][j] = sqrtf(dist);
            distances[j][i] = distances[i][j];
        }
    }

    /* For each belief, compute sum of n-f-2 closest distances */
    float krum_scores[MESH_MAX_BELIEFS_PER_REQUEST];
    uint32_t k = n - f - 2;

    for (uint32_t i = 0; i < n; i++) {
        /* Collect distances to others */
        float dists[MESH_MAX_BELIEFS_PER_REQUEST];
        for (uint32_t j = 0; j < n; j++) {
            dists[j] = distances[i][j];
        }

        /* Sort distances */
        for (uint32_t a = 0; a < n - 1; a++) {
            for (uint32_t b = 0; b < n - a - 1; b++) {
                if (dists[b] > dists[b + 1]) {
                    float tmp = dists[b];
                    dists[b] = dists[b + 1];
                    dists[b + 1] = tmp;
                }
            }
        }

        /* Sum k closest (skip self at index 0) */
        krum_scores[i] = 0.0f;
        for (uint32_t j = 1; j <= k && j < n; j++) {
            krum_scores[i] += dists[j] * dists[j];
        }
    }

    /* Mark all except the one with minimum Krum score */
    uint32_t best_idx = 0;
    float best_score = krum_scores[0];

    for (uint32_t i = 1; i < n; i++) {
        if (krum_scores[i] < best_score) {
            best_score = krum_scores[i];
            best_idx = i;
        }
    }

    /* Exclude all but best */
    uint32_t excluded = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (i != best_idx && !mesh->beliefs[i].excluded_bft) {
            mesh->beliefs[i].excluded_bft = true;
            excluded++;
        }
    }

    mesh->stats.byzantine_detections += excluded;
    return excluded;
}

/**
 * @brief Apply Byzantine fault tolerance based on mode
 */
static uint32_t mesh_apply_bft_internal(linguistics_mesh_t* mesh) {
    switch (mesh->config.alg_config.bft_mode) {
        case LING_MESH_BFT_TRIMMED_MEAN:
        case LING_MESH_BFT_TRIMMED_MEDIAN:
            return mesh_apply_trimmed_bft(mesh);

        case LING_MESH_BFT_KRUM:
            return mesh_apply_krum_bft(mesh);

        case LING_MESH_BFT_VOTING:
            /* Handled in convergence loop */
            return 0;

        case LING_MESH_BFT_NONE:
        default:
            return 0;
    }
}

/* ============================================================================
 * FEP BELIEF UPDATE
 * ============================================================================ */

/**
 * @brief Perform FEP belief update: μ' = μ - lr * Π * ε
 */
static void mesh_fep_update_belief(
    linguistics_mesh_t* mesh,
    mesh_active_belief_t* belief,
    const linguistics_collective_belief_t* collective
) {
    float lr = mesh->config.belief_learning_rate;
    float precision = belief->belief.precision * belief->credibility_weight;

    /* Apply PageRank weight if available */
    if (belief->pagerank_weight > 0.0f) {
        precision *= belief->pagerank_weight;
    }

    for (uint32_t j = 0; j < belief->belief.vector_dim && j < MESH_BELIEF_VECTOR_DIM; j++) {
        /* Prediction error */
        float epsilon = collective->belief_vector[j] -
                       belief->belief.belief_vector[j];

        /* Precision-weighted update */
        belief->belief.belief_vector[j] += lr * precision * epsilon;
    }

    /* Update certainty toward collective */
    float certainty_error = collective->collective_certainty -
                           belief->belief.certainty;
    belief->belief.certainty += lr * precision * certainty_error;

    /* Clamp certainty */
    if (belief->belief.certainty < 0.0f) belief->belief.certainty = 0.0f;
    if (belief->belief.certainty > 1.0f) belief->belief.certainty = 1.0f;
}

/* ============================================================================
 * BELIEF COLLECTION
 * ============================================================================ */

/**
 * @brief Collect beliefs from all participants
 */
static int mesh_collect_beliefs(
    linguistics_mesh_t* mesh,
    const linguistics_request_t* request
) {
    mesh->belief_count = 0;

    for (uint32_t i = 0; i < mesh->participant_count; i++) {
        if (!mesh->participants[i].active ||
            !mesh->participants[i].handler.process) {
            continue;
        }

        linguistics_belief_t belief;
        memset(&belief, 0, sizeof(belief));

        int ret = mesh->participants[i].handler.process(
            mesh->participants[i].handler.ctx,
            request,
            &belief
        );

        if (ret == 0 && mesh->belief_count < MESH_MAX_BELIEFS_PER_REQUEST) {
            mesh->beliefs[mesh->belief_count].belief = belief;
            mesh->beliefs[mesh->belief_count].credibility_weight =
                mesh->participants[i].credibility;
            mesh->beliefs[mesh->belief_count].pagerank_weight =
                mesh->participants[i].pagerank_score;
            mesh->beliefs[mesh->belief_count].propagated = false;
            mesh->beliefs[mesh->belief_count].propagation_count = 0;
            mesh->beliefs[mesh->belief_count].excluded_bft = false;

            mesh->belief_count++;
            mesh->participants[i].beliefs_contributed++;
            mesh->participants[i].last_precision = belief.precision;
        }
    }

    return (mesh->belief_count > 0) ? 0 : LING_ERR_MESH_NOT_READY;
}

/**
 * @brief Run gossip propagation round
 */
static uint32_t mesh_gossip_round_internal(linguistics_mesh_t* mesh) {
    uint32_t propagated = 0;

    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (mesh->beliefs[i].excluded_bft) continue;

        /* Skip already propagated or below threshold */
        if (mesh->beliefs[i].propagated ||
            mesh->beliefs[i].belief.certainty < mesh->config.gossip_probability) {
            continue;
        }

        /* Mark as propagated */
        mesh->beliefs[i].propagated = true;
        mesh->beliefs[i].propagation_count++;
        propagated++;

        /* Apply decay */
        mesh->beliefs[i].belief.certainty *= (1.0f - mesh->config.belief_decay_rate);
    }

    return propagated;
}

/* ============================================================================
 * CONVERGENCE LOOP
 * ============================================================================ */

/**
 * @brief Run selected algorithm step
 */
static float mesh_run_algorithm_step(linguistics_mesh_t* mesh) {
    switch (mesh->config.algorithm) {
        case LING_MESH_ALG_BELIEF_PROPAGATION:
        case LING_MESH_ALG_LOOPY_BP:
            return mesh_bp_step_internal(mesh);

        case LING_MESH_ALG_NATURAL_GRADIENT:
            return mesh_natural_gradient_step_internal(mesh);

        case LING_MESH_ALG_LAPLACIAN_CONSENSUS:
            return mesh_laplacian_step_internal(mesh);

        case LING_MESH_ALG_WEIGHTED_CONSENSUS:
        case LING_MESH_ALG_METROPOLIS_HASTINGS:
        case LING_MESH_ALG_FEP_BASIC:
        default: {
            /* Basic FEP update */
            linguistics_collective_belief_t collective;
            mesh_compute_collective_belief(mesh, &collective);

            float total_change = 0.0f;
            for (uint32_t i = 0; i < mesh->belief_count; i++) {
                if (mesh->beliefs[i].excluded_bft) continue;

                float prev_cert = mesh->beliefs[i].belief.certainty;
                mesh_fep_update_belief(mesh, &mesh->beliefs[i], &collective);
                total_change += fabsf(mesh->beliefs[i].belief.certainty - prev_cert);
            }

            return mesh_compute_free_energy(mesh);
        }
    }
}

/**
 * @brief Run FEP convergence loop
 */
static int mesh_run_convergence(
    linguistics_mesh_t* mesh,
    linguistics_convergence_state_t* state,
    uint32_t timeout_ms
) {
    uint64_t start_time = nimcp_time_monotonic_ms();

    /* Initialize BP messages if using BP */
    if (mesh->config.algorithm == LING_MESH_ALG_BELIEF_PROPAGATION ||
        mesh->config.algorithm == LING_MESH_ALG_LOOPY_BP) {
        mesh_bp_init_messages(mesh);
    }

    /* Update PageRank if enabled */
    if (mesh->config.enable_kg_discovery) {
        mesh_update_pagerank_internal(mesh);
    }

    /* Apply Byzantine tolerance */
    if (mesh->config.alg_config.bft_mode != LING_MESH_BFT_NONE &&
        mesh->config.alg_config.bft_mode != LING_MESH_BFT_VOTING) {
        mesh_apply_bft_internal(mesh);
    }

    float prev_free_energy = mesh_compute_free_energy(mesh);
    mesh->prev_lyapunov = mesh_compute_lyapunov_internal(mesh);

    state->state = MESH_STATE_CONVERGING;
    state->iteration = 0;
    state->free_energy = prev_free_energy;
    state->voting_triggered = false;
    state->algorithm_used = mesh->config.algorithm;

    while (state->iteration < mesh->config.max_iterations) {
        /* Check timeout */
        uint64_t elapsed = nimcp_time_monotonic_ms() - start_time;
        if (elapsed > timeout_ms) {
            state->state = MESH_STATE_TIMEOUT;
            mesh->stats.timeouts++;
            return LING_ERR_MESH_TIMEOUT;
        }

        /* Gossip round */
        mesh_gossip_round_internal(mesh);

        /* Algorithm step */
        float delta = mesh_run_algorithm_step(mesh);

        /* Compute metrics */
        float free_energy = mesh_compute_free_energy(mesh);
        float agreement = mesh_compute_agreement(mesh);
        float kl_div = mesh_compute_kl_internal(mesh);
        float lyapunov = mesh_compute_lyapunov_internal(mesh);

        state->free_energy = free_energy;
        state->free_energy_delta = prev_free_energy - free_energy;
        state->agreement_score = agreement;
        state->kl_divergence = kl_div;
        state->lyapunov_value = lyapunov;
        state->lyapunov_decreasing = (lyapunov < mesh->prev_lyapunov);
        state->iteration++;

        /* Update algorithm usage stats */
        mesh->stats.algorithm_usage[mesh->config.algorithm]++;

        /* Check convergence */
        if (agreement >= mesh->config.agreement_threshold) {
            state->state = MESH_STATE_CONVERGED;

            /* Compute marginals for BP */
            if (mesh->config.algorithm == LING_MESH_ALG_BELIEF_PROPAGATION ||
                mesh->config.algorithm == LING_MESH_ALG_LOOPY_BP) {
                mesh_bp_compute_marginals(mesh);
            }

            return LING_ERR_OK;
        }

        /* Check deadlock (free energy not decreasing) */
        if (state->iteration > mesh->config.convergence_window) {
            if (fabsf(state->free_energy_delta) < mesh->config.convergence_threshold) {
                state->deadlock_score = 1.0f - agreement;

                if (state->deadlock_score > 0.5f && mesh->config.enable_voting_fallback) {
                    /* Trigger voting fallback */
                    state->state = MESH_STATE_VOTING;
                    state->voting_triggered = true;
                    break;
                }
            }
        }

        prev_free_energy = free_energy;
        mesh->prev_lyapunov = lyapunov;
    }

    /* If voting was triggered or max iterations reached */
    if (state->voting_triggered) {
        /* Voting fallback: select highest-precision belief */
        float best_precision = 0.0f;
        uint32_t best_idx = 0;

        for (uint32_t i = 0; i < mesh->belief_count; i++) {
            if (mesh->beliefs[i].excluded_bft) continue;

            float effective_precision = mesh->beliefs[i].belief.precision *
                                        mesh->beliefs[i].credibility_weight *
                                        (mesh->beliefs[i].pagerank_weight > 0 ?
                                         mesh->beliefs[i].pagerank_weight : 1.0f);

            if (effective_precision > best_precision) {
                best_precision = effective_precision;
                best_idx = i;
            }
        }

        /* Replace collective with winner */
        if (mesh->belief_count > 0) {
            for (uint32_t i = 0; i < mesh->belief_count; i++) {
                if (i != best_idx && !mesh->beliefs[i].excluded_bft) {
                    memcpy(&mesh->beliefs[i].belief.belief_vector,
                           &mesh->beliefs[best_idx].belief.belief_vector,
                           sizeof(mesh->beliefs[i].belief.belief_vector));
                }
            }
        }

        state->state = MESH_STATE_CONVERGED;
        state->agreement_score = 1.0f;
        mesh->stats.voting_fallbacks++;
    } else if (state->agreement_score >= mesh->config.agreement_threshold) {
        state->state = MESH_STATE_CONVERGED;
    } else {
        state->state = MESH_STATE_FAILED;
        mesh->stats.failed_convergences++;
        return LING_ERR_CONVERGENCE_FAILED;
    }

    return LING_ERR_OK;
}

/**
 * @brief Build response from converged beliefs
 */
static void mesh_build_response(
    linguistics_mesh_t* mesh,
    const linguistics_request_t* request,
    linguistics_response_t* response,
    const linguistics_convergence_state_t* state
) {
    memset(response, 0, sizeof(*response));

    response->type = request->type;
    response->request_id = 0;

    /* Get collective belief */
    linguistics_collective_belief_t collective;

    /* Use covariance intersection if configured */
    if (mesh->config.algorithm == LING_MESH_ALG_COVARIANCE_INTERSECT) {
        mesh_covariance_intersection_internal(mesh, &collective);
    } else {
        mesh_compute_collective_belief(mesh, &collective);
    }

    response->confidence = collective.collective_certainty;
    response->precision = collective.collective_precision;
    response->iterations = state->iteration;
    response->agreement_score = state->agreement_score;
    response->converged = (state->state == MESH_STATE_CONVERGED);
    response->used_voting_fallback = state->voting_triggered;

    /* Type-specific response building */
    switch (request->type) {
        case LING_REQUEST_PARSE_SPATIAL:
            response->spatial.overall_confidence = collective.collective_certainty;
            response->spatial.frame_confidence = collective.collective_certainty;
            if (collective.vector_dim >= 4) {
                response->spatial.preposition =
                    (spatial_preposition_t)((int)(collective.belief_vector[0] *
                                            SPATIAL_PREPOSITION_COUNT));
                response->spatial.frame =
                    (reference_frame_t)((int)(collective.belief_vector[1] *
                                        REF_FRAME_COUNT));
                response->spatial.distance_membership = collective.belief_vector[2];
                response->spatial.angle_membership = collective.belief_vector[3];
            }
            break;

        case LING_REQUEST_PARSE_NUMBER:
            response->numerical.confidence = collective.collective_certainty;
            if (collective.vector_dim >= 3) {
                response->numerical.magnitude = collective.belief_vector[0] * 1000.0f;
                response->numerical.uncertainty = collective.belief_vector[1];
                response->numerical.type =
                    (number_word_type_t)((int)(collective.belief_vector[2] *
                                         NUM_WORD_TYPE_COUNT));
            }
            break;

        case LING_REQUEST_ENCODE_PHONOLOGICAL:
            response->phonological.activation = collective.collective_certainty;
            break;

        default:
            break;
    }

    if (!response->converged) {
        response->error_code = LING_ERR_CONVERGENCE_FAILED;
        strncpy(response->error_message, "Mesh failed to converge",
                sizeof(response->error_message) - 1);
    }
}

/* ============================================================================
 * LIFECYCLE API IMPLEMENTATION
 * ============================================================================ */

linguistics_mesh_algorithm_config_t linguistics_mesh_default_algorithm_config(void) {
    linguistics_mesh_algorithm_config_t config = {
        /* Belief Propagation */
        .bp_max_iterations = LING_MESH_BP_MAX_ITERATIONS,
        .bp_damping = LING_MESH_BP_DAMPING_FACTOR,
        .bp_convergence_threshold = BP_CONVERGENCE_EPS,

        /* Natural Gradient */
        .fisher_epsilon = LING_MESH_NATURAL_GRAD_EPSILON,
        .use_diagonal_fisher = true,

        /* Covariance Intersection */
        .ci_omega = LING_MESH_COV_INTERSECT_OMEGA,
        .ci_optimize_omega = false,

        /* PageRank */
        .pagerank_damping = LING_MESH_PAGERANK_DAMPING,
        .pagerank_iterations = 20,

        /* Graph Laplacian */
        .laplacian_use_normalized = true,
        .laplacian_step_size = 0.1f,

        /* Byzantine Tolerance */
        .bft_mode = LING_MESH_BFT_NONE,
        .bft_trim_fraction = LING_MESH_TRIMMED_FRACTION,
        .bft_max_faulty = 2
    };

    return config;
}

linguistics_mesh_config_t linguistics_mesh_default_config(void) {
    linguistics_mesh_config_t config = {
        /* Convergence */
        .agreement_threshold = LINGUISTICS_MESH_AGREEMENT_THRESHOLD,
        .convergence_threshold = 0.001f,
        .max_iterations = LINGUISTICS_MESH_MAX_ITERATIONS,
        .convergence_window = LING_MESH_DEFAULT_CONVERGENCE_WINDOW,

        /* FEP */
        .belief_learning_rate = LINGUISTICS_FEP_LEARNING_RATE,
        .precision_floor = LINGUISTICS_PRECISION_FLOOR,
        .precision_ceiling = LINGUISTICS_PRECISION_CEILING,

        /* Gossip */
        .gossip_probability = LINGUISTICS_GOSSIP_PROBABILITY,
        .credibility_weight = 0.8f,
        .belief_decay_rate = 0.05f,

        /* Fallback */
        .enable_voting_fallback = true,
        .bft_threshold = 0.333f,

        /* Algorithm */
        .algorithm = LING_MESH_ALG_FEP_BASIC,
        .metric = LING_MESH_METRIC_COSINE,
        .alg_config = linguistics_mesh_default_algorithm_config(),

        /* Integration */
        .enable_bio_async = true,
        .enable_kg_discovery = true,
        .enable_statistics = true,
        .enable_logging = true,

        /* Infrastructure */
        .enable_bbb = true,
        .enable_health_monitor = true,
        .enable_heartbeat = true,
        .enable_kg_wiring = true,
        .enable_exception_handling = true,

        /* Channel */
        .default_channel = LING_MESH_CHANNEL_ATTENTION,

        /* Performance */
        .timeout_ms = NIMCP_DEFAULT_TIMEOUT_MS,
        .heartbeat_interval_ms = LING_MESH_HEARTBEAT_INTERVAL_MS
    };

    return config;
}

bool linguistics_mesh_validate_config(const linguistics_mesh_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "linguistics_mesh_validate_config: config is NULL");
        return false;
    }

    if (config->agreement_threshold < 0.0f || config->agreement_threshold > 1.0f) {
        return false;
    }
    if (config->belief_learning_rate <= 0.0f || config->belief_learning_rate > 1.0f) {
        return false;
    }
    if (config->precision_floor <= 0.0f) {
        return false;
    }
    if (config->precision_ceiling <= config->precision_floor) {
        return false;
    }
    if (config->gossip_probability < 0.0f || config->gossip_probability > 1.0f) {
        return false;
    }
    if (config->max_iterations == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "linguistics_mesh_validate_config: config->max_iterations is zero");
        return false;
    }
    if (config->algorithm >= LING_MESH_ALG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "linguistics_mesh_validate_config: capacity exceeded");
        return false;
    }
    if (config->metric >= LING_MESH_METRIC_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "linguistics_mesh_validate_config: capacity exceeded");
        return false;
    }

    return true;
}

linguistics_mesh_t* linguistics_mesh_create(const linguistics_mesh_config_t* config) {
    linguistics_mesh_config_t actual_config;

    if (config) {
        if (!linguistics_mesh_validate_config(config)) {
            mesh_set_error("Invalid mesh configuration");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "linguistics_mesh_create: linguistics_mesh_validate_config is NULL");
            return NULL;
        }
        actual_config = *config;
    } else {
        actual_config = linguistics_mesh_default_config();
    }

    linguistics_mesh_t* mesh = nimcp_calloc(1, sizeof(linguistics_mesh_t));
    if (!mesh) {
        mesh_set_error("Failed to allocate mesh coordinator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "linguistics_mesh_create: mesh is NULL");
        return NULL;
    }

    mesh->magic = MESH_MAGIC;
    mesh->config = actual_config;
    mesh->creation_time_ms = nimcp_time_monotonic_ms();
    mesh->next_request_id = 1;
    mesh->topology_dirty = true;

    /* Initialize Lyapunov P matrix (identity) */
    for (uint32_t i = 0; i < MESH_BELIEF_VECTOR_DIM; i++) {
        mesh->lyapunov_P[i] = 1.0f;
    }

    /* Create mutex */
    mesh->mutex = nimcp_mutex_create(NULL);
    if (!mesh->mutex) {
        mesh_set_error("Failed to create mesh mutex");
        nimcp_free(mesh);
        mesh = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "linguistics_mesh_create: mesh->mutex is NULL");
        return NULL;
    }

    /* Initialize convergence state */
    mesh->current_state.state = MESH_STATE_IDLE;

    if (actual_config.enable_logging) {
        nimcp_log_info(LOG_MODULE_LING_MESH,
            "Created linguistics mesh coordinator v2.0 (alg=%s, agreement=%.2f, lr=%.2f)",
            linguistics_mesh_algorithm_to_string(actual_config.algorithm),
            actual_config.agreement_threshold,
            actual_config.belief_learning_rate);
    }

    return mesh;
}

void linguistics_mesh_destroy(linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return;
    }

    /* Stop heartbeat if running */
    if (mesh->heartbeat_running) {
        linguistics_mesh_stop_heartbeat(mesh);
    }

    mesh->magic = 0;

    /* Free allocated arrays */
    if (mesh->adjacency_matrix) {
        nimcp_free(mesh->adjacency_matrix);
    }
    if (mesh->pagerank_scores) {
        nimcp_free(mesh->pagerank_scores);
    }
    if (mesh->laplacian.laplacian) {
        nimcp_free(mesh->laplacian.laplacian);
    }
    if (mesh->laplacian.degree_matrix) {
        nimcp_free(mesh->laplacian.degree_matrix);
    }
    if (mesh->laplacian.adjacency) {
        nimcp_free(mesh->laplacian.adjacency);
    }

    /* Free participant neighbor arrays */
    for (uint32_t i = 0; i < mesh->participant_count; i++) {
        if (mesh->participants[i].neighbors) {
            nimcp_free(mesh->participants[i].neighbors);
        }
        if (mesh->participants[i].neighbor_weights) {
            nimcp_free(mesh->participants[i].neighbor_weights);
        }
        if (mesh->participants[i].covariance_diag) {
            nimcp_free(mesh->participants[i].covariance_diag);
        }
    }

    if (mesh->mutex) {
        nimcp_mutex_destroy(mesh->mutex);
    }

    if (mesh->config.enable_logging) {
        nimcp_log_info(LOG_MODULE_LING_MESH,
            "Destroyed linguistics mesh coordinator (requests=%" PRIu64 ", success_rate=%.2f%%)",
            mesh->stats.total_requests,
            mesh->stats.total_requests > 0 ?
                100.0f * mesh->stats.successful_convergences / mesh->stats.total_requests : 0.0f);
    }

    nimcp_free(mesh);
    mesh = NULL;
}

/* ============================================================================
 * INFRASTRUCTURE INTEGRATION API IMPLEMENTATION
 * ============================================================================ */

int linguistics_mesh_connect_bbb(linguistics_mesh_t* mesh, bbb_system_t bbb) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(mesh->mutex);
    mesh->bbb = bbb;
    nimcp_mutex_unlock(mesh->mutex);

    if (mesh->config.enable_logging) {
        nimcp_log_info(LOG_MODULE_LING_MESH, "Connected to BBB system");
    }

    return LING_ERR_OK;
}

int linguistics_mesh_connect_health(linguistics_mesh_t* mesh, health_monitor_t health) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(mesh->mutex);
    mesh->health = health;
    mesh->health_connected = true;
    nimcp_mutex_unlock(mesh->mutex);

    if (mesh->config.enable_logging) {
        nimcp_log_info(LOG_MODULE_LING_MESH, "Connected to health monitor");
    }

    return LING_ERR_OK;
}

int linguistics_mesh_connect_kg(linguistics_mesh_t* mesh, brain_kg_t* kg) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }
    if (!kg) {
        return LING_ERR_NULL_POINTER;
    }

    nimcp_mutex_lock(mesh->mutex);
    mesh->kg = kg;
    mesh->kg_connected = true;

    /* Idempotent registration — see docs/claude/kg-node-naming-registry.md.
     * LING_MESH_KG_NODE_NAME is "parietal_linguistics_mesh" in the header. */
    brain_kg_node_id_t existing = brain_kg_find_node(kg, LING_MESH_KG_NODE_NAME);
    if (existing != BRAIN_KG_INVALID_NODE) {
        mesh->kg_node_id = existing;
    } else {
        mesh->kg_node_id = brain_kg_add_node(
            kg,
            LING_MESH_KG_NODE_NAME,
            BRAIN_KG_NODE_COGNITIVE,
            "Parietal linguistics consensus mesh (FEP/BP-based token agreement)"
        );
    }

    /* Wire mesh into parietal + language sub-graph if the peer nodes exist. */
    if (mesh->kg_node_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t parietal = brain_kg_find_node(kg, "parietal_cortex");
        if (parietal != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, parietal, mesh->kg_node_id,
                              BRAIN_KG_EDGE_CONNECTS_TO,
                              "parietal hosts linguistics mesh", 0.8f);
        }
        brain_kg_node_id_t wernicke = brain_kg_find_node(kg, "wernicke");
        if (wernicke != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, mesh->kg_node_id, wernicke,
                              BRAIN_KG_EDGE_COORDINATES_WITH,
                              "mesh consensus informs wernicke comprehension", 0.7f);
        }
    }

    nimcp_mutex_unlock(mesh->mutex);

    if (mesh->config.enable_logging) {
        nimcp_log_info(LOG_MODULE_LING_MESH, "Connected to brain KG (node_id=%u)",
                       mesh->kg_node_id);
    }

    return LING_ERR_OK;
}

int linguistics_mesh_start_heartbeat(linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(mesh->mutex);
    mesh->heartbeat_running = true;
    mesh->last_heartbeat_ms = nimcp_time_monotonic_ms();
    nimcp_mutex_unlock(mesh->mutex);

    if (mesh->config.enable_logging) {
        nimcp_log_debug(LOG_MODULE_LING_MESH, "Started heartbeat monitoring");
    }

    return LING_ERR_OK;
}

int linguistics_mesh_stop_heartbeat(linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(mesh->mutex);
    mesh->heartbeat_running = false;
    nimcp_mutex_unlock(mesh->mutex);

    return LING_ERR_OK;
}

void linguistics_mesh_heartbeat_pulse(linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return;
    }

    nimcp_mutex_lock(mesh->mutex);
    mesh->last_heartbeat_ms = nimcp_time_monotonic_ms();
    nimcp_mutex_unlock(mesh->mutex);
}

brain_kg_node_id_t linguistics_mesh_get_kg_node_id(const linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return 0;  /* BRAIN_KG_INVALID_NODE */
    }
    return mesh->kg_node_id;
}

/* ============================================================================
 * PARTICIPANT REGISTRATION API IMPLEMENTATION
 * ============================================================================ */

int linguistics_mesh_register_participant(
    linguistics_mesh_t* mesh,
    uint32_t module_id,
    const char* module_name,
    linguistics_mesh_handler_t handler
) {
    if (!mesh_is_valid(mesh)) {
        mesh_record_exception(mesh);
        return LING_ERR_NOT_INITIALIZED;
    }

    /* BBB validation */
    if (mesh->config.enable_bbb) {
        int ret = mesh_validate_input_string(mesh, module_name, "module_name");
        if (ret != LING_ERR_OK) {
            mesh_record_exception(mesh);
            return ret;
        }
    }

    if (!handler.process) {
        mesh_set_error("Handler process callback is NULL");
        mesh_record_exception(mesh);
        return LING_ERR_NULL_POINTER;
    }

    nimcp_mutex_lock(mesh->mutex);

    /* Check if already registered */
    if (mesh_find_participant(mesh, module_id)) {
        nimcp_mutex_unlock(mesh->mutex);
        return LING_ERR_INVALID_PARAM;
    }

    /* Find empty slot */
    if (mesh->participant_count >= LINGUISTICS_MAX_MESH_PARTICIPANTS) {
        nimcp_mutex_unlock(mesh->mutex);
        return LING_ERR_BUFFER_OVERFLOW;
    }

    linguistics_mesh_participant_t* p = &mesh->participants[mesh->participant_count];
    memset(p, 0, sizeof(*p));

    p->module_id = module_id;
    strncpy(p->module_name, module_name, sizeof(p->module_name) - 1);
    p->handler = handler;
    p->credibility = 1.0f;
    p->pagerank_score = 1.0f / (mesh->participant_count + 1);
    p->active = true;
    p->last_update_ms = nimcp_time_monotonic_ms();

    mesh->participant_count++;
    mesh->stats.active_participants = mesh->participant_count;
    mesh->topology_dirty = true;

    nimcp_mutex_unlock(mesh->mutex);

    if (mesh->config.enable_logging) {
        nimcp_log_debug(LOG_MODULE_LING_MESH,
            "Registered participant: %s (id=0x%04X, total=%u)",
            module_name, module_id, mesh->participant_count);
    }

    return LING_ERR_OK;
}

int linguistics_mesh_unregister_participant(
    linguistics_mesh_t* mesh,
    uint32_t module_id
) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(mesh->mutex);

    linguistics_mesh_participant_t* p = mesh_find_participant(mesh, module_id);
    if (!p) {
        nimcp_mutex_unlock(mesh->mutex);
        return LING_ERR_INVALID_PARAM;
    }

    p->active = false;
    mesh->stats.active_participants--;
    mesh->topology_dirty = true;

    nimcp_mutex_unlock(mesh->mutex);

    if (mesh->config.enable_logging) {
        nimcp_log_debug(LOG_MODULE_LING_MESH,
            "Unregistered participant: %s (id=0x%04X)", p->module_name, module_id);
    }

    return LING_ERR_OK;
}

int linguistics_mesh_set_credibility(
    linguistics_mesh_t* mesh,
    uint32_t module_id,
    float credibility
) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }
    if (credibility < 0.0f || credibility > 1.0f) {
        return LING_ERR_INVALID_PARAM;
    }

    nimcp_mutex_lock(mesh->mutex);

    linguistics_mesh_participant_t* p = mesh_find_participant(mesh, module_id);
    if (!p) {
        nimcp_mutex_unlock(mesh->mutex);
        return LING_ERR_INVALID_PARAM;
    }

    p->credibility = credibility;

    nimcp_mutex_unlock(mesh->mutex);

    return LING_ERR_OK;
}

int linguistics_mesh_set_topology(
    linguistics_mesh_t* mesh,
    const float* adjacency_matrix
) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }
    if (!adjacency_matrix) {
        return LING_ERR_NULL_POINTER;
    }

    nimcp_mutex_lock(mesh->mutex);

    uint32_t n = mesh->participant_count;
    size_t size = n * n * sizeof(float);

    if (!mesh->adjacency_matrix) {
        mesh->adjacency_matrix = nimcp_malloc(size);
    }

    if (mesh->adjacency_matrix) {
        memcpy(mesh->adjacency_matrix, adjacency_matrix, size);
        mesh->topology_dirty = true;
    }

    nimcp_mutex_unlock(mesh->mutex);

    return LING_ERR_OK;
}

int linguistics_mesh_update_pagerank(linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(mesh->mutex);
    int ret = mesh_update_pagerank_internal(mesh);
    nimcp_mutex_unlock(mesh->mutex);

    return ret;
}

const linguistics_mesh_participant_t* linguistics_mesh_get_participant(
    const linguistics_mesh_t* mesh,
    uint32_t module_id
) {
    if (!mesh_is_valid(mesh)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "linguistics_mesh_get_participant: mesh_is_valid is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < mesh->participant_count; i++) {
        if (mesh->participants[i].module_id == module_id &&
            mesh->participants[i].active) {
            return &mesh->participants[i];
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "linguistics_mesh_get_participant: operation failed");
    return NULL;
}

uint32_t linguistics_mesh_get_participant_count(const linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return 0;
    }
    return mesh->stats.active_participants;
}

/* ============================================================================
 * REQUEST API IMPLEMENTATION
 * ============================================================================ */

int linguistics_mesh_request(
    linguistics_mesh_t* mesh,
    const linguistics_request_t* request,
    linguistics_response_t* response,
    uint32_t timeout_ms
) {
    if (!mesh_is_valid(mesh)) {
        mesh_record_exception(mesh);
        return LING_ERR_NOT_INITIALIZED;
    }
    if (!request || !response) {
        mesh_record_exception(mesh);
        return LING_ERR_NULL_POINTER;
    }

    /* BBB validation */
    if (mesh->config.enable_bbb) {
        int ret = mesh_validate_request(mesh, request);
        if (ret != LING_ERR_OK) {
            mesh_record_exception(mesh);
            return ret;
        }
    }

    uint32_t actual_timeout = timeout_ms > 0 ? timeout_ms : mesh->config.timeout_ms;
    uint64_t start_time = nimcp_time_monotonic_ms();

    nimcp_mutex_lock(mesh->mutex);

    /* Update state */
    mesh->current_state.state = MESH_STATE_BROADCASTING;
    mesh->stats.total_requests++;

    /* Pulse heartbeat */
    if (mesh->heartbeat_running) {
        mesh->last_heartbeat_ms = start_time;
    }

    /* Collect beliefs from all participants */
    int ret = mesh_collect_beliefs(mesh, request);
    if (ret != LING_ERR_OK) {
        mesh->current_state.state = MESH_STATE_FAILED;
        mesh->stats.failed_convergences++;
        nimcp_mutex_unlock(mesh->mutex);

        if (mesh->config.enable_logging) {
            nimcp_log_warn(LOG_MODULE_LING_MESH,
                "Failed to collect beliefs for request type=%s",
                linguistics_request_type_to_string(request->type));
        }

        return ret;
    }

    /* Run convergence */
    linguistics_convergence_state_t conv_state;
    ret = mesh_run_convergence(mesh, &conv_state, actual_timeout);

    /* Build response */
    mesh_build_response(mesh, request, response, &conv_state);

    /* Update statistics */
    uint64_t elapsed = nimcp_time_monotonic_ms() - start_time;
    mesh->stats.avg_latency_ms =
        (mesh->stats.avg_latency_ms * (mesh->stats.total_requests - 1) + elapsed) /
        mesh->stats.total_requests;

    if (response->converged) {
        mesh->stats.successful_convergences++;
        mesh->stats.avg_agreement_score =
            (mesh->stats.avg_agreement_score * (mesh->stats.successful_convergences - 1) +
             response->agreement_score) / mesh->stats.successful_convergences;
    }

    mesh->stats.avg_iterations =
        (mesh->stats.avg_iterations * (mesh->stats.total_requests - 1) +
         conv_state.iteration) / mesh->stats.total_requests;

    mesh->stats.avg_kl_divergence =
        (mesh->stats.avg_kl_divergence * (mesh->stats.total_requests - 1) +
         conv_state.kl_divergence) / mesh->stats.total_requests;

    mesh->stats.avg_lyapunov_value =
        (mesh->stats.avg_lyapunov_value * (mesh->stats.total_requests - 1) +
         conv_state.lyapunov_value) / mesh->stats.total_requests;

    mesh->current_state = conv_state;

    /* Record health operation */
    mesh_record_health_operation(mesh, (float)elapsed, response->converged);

    nimcp_mutex_unlock(mesh->mutex);

    if (mesh->config.enable_logging) {
        nimcp_log_debug(LOG_MODULE_LING_MESH,
            "Request completed: type=%s, alg=%s, converged=%d, iter=%u, agreement=%.2f, latency=%.1fms",
            linguistics_request_type_to_string(request->type),
            linguistics_mesh_algorithm_to_string(conv_state.algorithm_used),
            response->converged, conv_state.iteration,
            conv_state.agreement_score, (float)elapsed);
    }

    return ret;
}

int linguistics_mesh_request_async(
    linguistics_mesh_t* mesh,
    const linguistics_request_t* request,
    uint64_t* request_id_out
) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }
    if (!request || !request_id_out) {
        return LING_ERR_NULL_POINTER;
    }

    /* BBB validation */
    if (mesh->config.enable_bbb) {
        int ret = mesh_validate_request(mesh, request);
        if (ret != LING_ERR_OK) {
            return ret;
        }
    }

    nimcp_mutex_lock(mesh->mutex);

    /* Find free slot */
    mesh_pending_request_t* pending = NULL;
    for (uint32_t i = 0; i < MESH_MAX_PENDING_REQUESTS; i++) {
        if (!mesh->pending_requests[i].active) {
            pending = &mesh->pending_requests[i];
            break;
        }
    }

    if (!pending) {
        nimcp_mutex_unlock(mesh->mutex);
        return LING_ERR_BUFFER_OVERFLOW;
    }

    /* Initialize pending request */
    memset(pending, 0, sizeof(*pending));
    pending->request_id = mesh->next_request_id++;
    pending->request = *request;
    pending->start_time_ms = nimcp_time_monotonic_ms();
    pending->timeout_ms = mesh->config.timeout_ms;
    pending->active = true;
    pending->complete = false;

    *request_id_out = pending->request_id;

    nimcp_mutex_unlock(mesh->mutex);

    return LING_ERR_OK;
}

int linguistics_mesh_poll(
    linguistics_mesh_t* mesh,
    uint64_t request_id,
    linguistics_response_t* response,
    linguistics_convergence_state_t* state_out
) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(mesh->mutex);

    mesh_pending_request_t* pending = mesh_find_pending_request(mesh, request_id);
    if (!pending) {
        nimcp_mutex_unlock(mesh->mutex);
        return LING_ERR_INVALID_PARAM;
    }

    if (pending->complete) {
        if (response) {
            *response = pending->response;
        }
        if (state_out) {
            *state_out = pending->conv_state;
        }
        pending->active = false;
        nimcp_mutex_unlock(mesh->mutex);
        return 0;  /* Complete */
    }

    /* Check timeout */
    uint64_t elapsed = nimcp_time_monotonic_ms() - pending->start_time_ms;
    if (elapsed > pending->timeout_ms) {
        pending->conv_state.state = MESH_STATE_TIMEOUT;
        pending->complete = true;
        pending->response.error_code = LING_ERR_MESH_TIMEOUT;
        mesh->stats.timeouts++;
        nimcp_mutex_unlock(mesh->mutex);
        return 0;  /* Complete (with timeout error) */
    }

    nimcp_mutex_unlock(mesh->mutex);
    return 1;  /* Still processing */
}

int linguistics_mesh_cancel(
    linguistics_mesh_t* mesh,
    uint64_t request_id
) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(mesh->mutex);

    mesh_pending_request_t* pending = mesh_find_pending_request(mesh, request_id);
    if (!pending) {
        nimcp_mutex_unlock(mesh->mutex);
        return LING_ERR_INVALID_PARAM;
    }

    pending->active = false;
    pending->complete = true;

    nimcp_mutex_unlock(mesh->mutex);

    return LING_ERR_OK;
}

/* ============================================================================
 * ALGORITHM API IMPLEMENTATION
 * ============================================================================ */

int linguistics_mesh_set_algorithm(
    linguistics_mesh_t* mesh,
    linguistics_mesh_algorithm_t algorithm
) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }
    if (algorithm >= LING_MESH_ALG_COUNT) {
        return LING_ERR_INVALID_PARAM;
    }

    nimcp_mutex_lock(mesh->mutex);
    mesh->config.algorithm = algorithm;
    nimcp_mutex_unlock(mesh->mutex);

    if (mesh->config.enable_logging) {
        nimcp_log_debug(LOG_MODULE_LING_MESH,
            "Set algorithm to: %s", linguistics_mesh_algorithm_to_string(algorithm));
    }

    return LING_ERR_OK;
}

int linguistics_mesh_set_metric(
    linguistics_mesh_t* mesh,
    linguistics_mesh_metric_t metric
) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }
    if (metric >= LING_MESH_METRIC_COUNT) {
        return LING_ERR_INVALID_PARAM;
    }

    nimcp_mutex_lock(mesh->mutex);
    mesh->config.metric = metric;
    nimcp_mutex_unlock(mesh->mutex);

    return LING_ERR_OK;
}

float linguistics_mesh_bp_step(linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return 0.0f;
    }

    nimcp_mutex_lock(mesh->mutex);
    float delta = mesh_bp_step_internal(mesh);
    mesh->current_state.iteration++;
    nimcp_mutex_unlock(mesh->mutex);

    return delta;
}

float linguistics_mesh_fep_step(linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return 0.0f;
    }

    nimcp_mutex_lock(mesh->mutex);

    float prev_fe = mesh_compute_free_energy(mesh);

    /* Compute collective */
    linguistics_collective_belief_t collective;
    mesh_compute_collective_belief(mesh, &collective);

    /* Update each belief */
    for (uint32_t i = 0; i < mesh->belief_count; i++) {
        if (!mesh->beliefs[i].excluded_bft) {
            mesh_fep_update_belief(mesh, &mesh->beliefs[i], &collective);
        }
    }

    float new_fe = mesh_compute_free_energy(mesh);
    float delta = prev_fe - new_fe;

    mesh->current_state.free_energy = new_fe;
    mesh->current_state.free_energy_delta = delta;
    mesh->current_state.iteration++;

    nimcp_mutex_unlock(mesh->mutex);

    return delta;
}

float linguistics_mesh_natural_grad_step(linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return 0.0f;
    }

    nimcp_mutex_lock(mesh->mutex);
    float delta = mesh_natural_gradient_step_internal(mesh);
    mesh->current_state.iteration++;
    nimcp_mutex_unlock(mesh->mutex);

    return delta;
}

uint32_t linguistics_mesh_gossip_round(linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return 0;
    }

    nimcp_mutex_lock(mesh->mutex);
    uint32_t propagated = mesh_gossip_round_internal(mesh);
    nimcp_mutex_unlock(mesh->mutex);

    return propagated;
}

float linguistics_mesh_laplacian_step(linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return 0.0f;
    }

    nimcp_mutex_lock(mesh->mutex);
    float delta = mesh_laplacian_step_internal(mesh);
    mesh->current_state.iteration++;
    nimcp_mutex_unlock(mesh->mutex);

    return delta;
}

int linguistics_mesh_covariance_intersection(
    linguistics_mesh_t* mesh,
    linguistics_collective_belief_t* collective
) {
    if (!mesh_is_valid(mesh) || !collective) {
        return LING_ERR_NULL_POINTER;
    }

    nimcp_mutex_lock(mesh->mutex);
    int ret = mesh_covariance_intersection_internal(mesh, collective);
    nimcp_mutex_unlock(mesh->mutex);

    return ret;
}

uint32_t linguistics_mesh_apply_bft(linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return 0;
    }

    nimcp_mutex_lock(mesh->mutex);
    uint32_t excluded = mesh_apply_bft_internal(mesh);
    nimcp_mutex_unlock(mesh->mutex);

    return excluded;
}

float linguistics_mesh_compute_lyapunov(const linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return 0.0f;
    }

    linguistics_mesh_t* mutable_mesh = (linguistics_mesh_t*)mesh;
    return mesh_compute_lyapunov_internal(mutable_mesh);
}

float linguistics_mesh_compute_kl_divergence(const linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return 0.0f;
    }

    linguistics_mesh_t* mutable_mesh = (linguistics_mesh_t*)mesh;
    return mesh_compute_kl_internal(mutable_mesh);
}

/* ============================================================================
 * CONVERGENCE API IMPLEMENTATION
 * ============================================================================ */

int linguistics_mesh_get_convergence_state(
    const linguistics_mesh_t* mesh,
    linguistics_convergence_state_t* state
) {
    if (!mesh_is_valid(mesh) || !state) {
        return LING_ERR_NULL_POINTER;
    }

    *state = mesh->current_state;
    return LING_ERR_OK;
}

int linguistics_mesh_get_collective_belief(
    const linguistics_mesh_t* mesh,
    const char* topic,
    linguistics_collective_belief_t* belief
) {
    if (!mesh_is_valid(mesh) || !belief) {
        return LING_ERR_NULL_POINTER;
    }

    linguistics_mesh_t* mutable_mesh = (linguistics_mesh_t*)mesh;
    mesh_compute_collective_belief(mutable_mesh, belief);

    return LING_ERR_OK;
}

int linguistics_mesh_force_voting(linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(mesh->mutex);

    mesh->current_state.state = MESH_STATE_VOTING;
    mesh->current_state.voting_triggered = true;

    nimcp_mutex_unlock(mesh->mutex);

    return LING_ERR_OK;
}

/* ============================================================================
 * CONFIGURATION API IMPLEMENTATION
 * ============================================================================ */

int linguistics_mesh_update_config(
    linguistics_mesh_t* mesh,
    const linguistics_mesh_config_t* config
) {
    if (!mesh_is_valid(mesh) || !config) {
        return LING_ERR_NULL_POINTER;
    }
    if (!linguistics_mesh_validate_config(config)) {
        return LING_ERR_INVALID_PARAM;
    }

    nimcp_mutex_lock(mesh->mutex);
    mesh->config = *config;
    mesh->topology_dirty = true;
    nimcp_mutex_unlock(mesh->mutex);

    return LING_ERR_OK;
}

int linguistics_mesh_get_config(
    const linguistics_mesh_t* mesh,
    linguistics_mesh_config_t* config
) {
    if (!mesh_is_valid(mesh) || !config) {
        return LING_ERR_NULL_POINTER;
    }

    *config = mesh->config;
    return LING_ERR_OK;
}

int linguistics_mesh_set_agreement_threshold(
    linguistics_mesh_t* mesh,
    float threshold
) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }
    if (threshold < 0.0f || threshold > 1.0f) {
        return LING_ERR_INVALID_PARAM;
    }

    nimcp_mutex_lock(mesh->mutex);
    mesh->config.agreement_threshold = threshold;
    nimcp_mutex_unlock(mesh->mutex);

    return LING_ERR_OK;
}

int linguistics_mesh_set_learning_rate(
    linguistics_mesh_t* mesh,
    float learning_rate
) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }
    if (learning_rate <= 0.0f || learning_rate > 1.0f) {
        return LING_ERR_INVALID_PARAM;
    }

    nimcp_mutex_lock(mesh->mutex);
    mesh->config.belief_learning_rate = learning_rate;
    nimcp_mutex_unlock(mesh->mutex);

    return LING_ERR_OK;
}

/* ============================================================================
 * STATISTICS API IMPLEMENTATION
 * ============================================================================ */

int linguistics_mesh_get_stats(
    const linguistics_mesh_t* mesh,
    linguistics_mesh_stats_t* stats
) {
    if (!mesh_is_valid(mesh) || !stats) {
        return LING_ERR_NULL_POINTER;
    }

    /* Copy from extended internal stats to simpler public stats */
    stats->total_requests = mesh->stats.total_requests;
    stats->successful_convergences = mesh->stats.successful_convergences;
    stats->voting_fallbacks = mesh->stats.voting_fallbacks;
    stats->timeouts = mesh->stats.timeouts;
    stats->deadlocks = mesh->stats.failed_convergences;  /* Map failed to deadlocks */
    stats->avg_iterations = mesh->stats.avg_iterations;
    stats->avg_agreement_score = mesh->stats.avg_agreement_score;
    stats->avg_latency_ms = mesh->stats.avg_latency_ms;
    stats->active_participants = mesh->stats.active_participants;

    return LING_ERR_OK;
}

void linguistics_mesh_reset_stats(linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return;
    }

    nimcp_mutex_lock(mesh->mutex);

    uint32_t active = mesh->stats.active_participants;
    float health = mesh->stats.current_health_score;
    memset(&mesh->stats, 0, sizeof(mesh->stats));
    mesh->stats.active_participants = active;
    mesh->stats.current_health_score = health;

    nimcp_mutex_unlock(mesh->mutex);
}

float linguistics_mesh_get_avg_latency(const linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return 0.0f;
    }
    return mesh->stats.avg_latency_ms;
}

float linguistics_mesh_get_success_rate(const linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh) || mesh->stats.total_requests == 0) {
        return 0.0f;
    }
    return (float)mesh->stats.successful_convergences /
           (float)mesh->stats.total_requests;
}

float linguistics_mesh_get_health_score(const linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return 0.0f;
    }
    return mesh->stats.current_health_score;
}

/* ============================================================================
 * BIO-ASYNC INTEGRATION API IMPLEMENTATION
 * ============================================================================ */

int linguistics_mesh_connect_bio_async(linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(mesh->mutex);
    mesh->bio_async_connected = true;
    nimcp_mutex_unlock(mesh->mutex);

    if (mesh->config.enable_logging) {
        nimcp_log_info(LOG_MODULE_LING_MESH, "Connected to bio-async router");
    }

    return LING_ERR_OK;
}

int linguistics_mesh_disconnect_bio_async(linguistics_mesh_t* mesh) {
    if (!mesh_is_valid(mesh)) {
        return LING_ERR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(mesh->mutex);
    mesh->bio_async_connected = false;
    nimcp_mutex_unlock(mesh->mutex);

    return LING_ERR_OK;
}

int linguistics_mesh_process_message(
    linguistics_mesh_t* mesh,
    const void* msg,
    size_t msg_len
) {
    if (!mesh_is_valid(mesh) || !msg) {
        return LING_ERR_NULL_POINTER;
    }

    /* Process incoming belief message from bio-async */
    /* Placeholder for future bio-async integration */

    return LING_ERR_OK;
}

/* ============================================================================
 * DEBUG API IMPLEMENTATION
 * ============================================================================ */

void linguistics_mesh_print_state(
    const linguistics_mesh_t* mesh,
    bool verbose
) {
    if (!mesh_is_valid(mesh)) {
        fprintf(stderr, "Invalid mesh handle\n");
        return;
    }

    fprintf(stderr, "=== Linguistics Mesh Coordinator v2.0 ===\n");
    fprintf(stderr, "Algorithm: %s\n",
            linguistics_mesh_algorithm_to_string(mesh->config.algorithm));
    fprintf(stderr, "State: %s\n",
            linguistics_mesh_state_to_string(mesh->current_state.state));
    fprintf(stderr, "Participants: %u active\n", mesh->stats.active_participants);
    fprintf(stderr, "Requests: %" PRIu64 " total, %" PRIu64 " converged, %" PRIu64 " failed\n",
            mesh->stats.total_requests,
            mesh->stats.successful_convergences,
            mesh->stats.failed_convergences);
    fprintf(stderr, "Avg latency: %.2f ms\n", mesh->stats.avg_latency_ms);
    fprintf(stderr, "Avg agreement: %.2f\n", mesh->stats.avg_agreement_score);
    fprintf(stderr, "Avg KL divergence: %.4f\n", mesh->stats.avg_kl_divergence);
    fprintf(stderr, "Voting fallbacks: %" PRIu64 "\n", mesh->stats.voting_fallbacks);
    fprintf(stderr, "Byzantine detections: %" PRIu64 "\n", mesh->stats.byzantine_detections);
    fprintf(stderr, "Health score: %.1f%%\n", mesh->stats.current_health_score);

    if (verbose) {
        fprintf(stderr, "\n--- Participants ---\n");
        for (uint32_t i = 0; i < mesh->participant_count; i++) {
            if (mesh->participants[i].active) {
                fprintf(stderr, "  [0x%04X] %s: cred=%.2f, pr=%.3f, beliefs=%" PRIu64 "\n",
                        mesh->participants[i].module_id,
                        mesh->participants[i].module_name,
                        mesh->participants[i].credibility,
                        mesh->participants[i].pagerank_score,
                        mesh->participants[i].beliefs_contributed);
            }
        }

        fprintf(stderr, "\n--- Current Beliefs (%u) ---\n", mesh->belief_count);
        for (uint32_t i = 0; i < mesh->belief_count; i++) {
            fprintf(stderr, "  [%u] %s: cert=%.2f, prec=%.2f, bft=%s\n",
                    i, mesh->beliefs[i].belief.topic,
                    mesh->beliefs[i].belief.certainty,
                    mesh->beliefs[i].belief.precision,
                    mesh->beliefs[i].excluded_bft ? "excluded" : "active");
        }

        fprintf(stderr, "\n--- Algorithm Usage ---\n");
        for (int i = 0; i < LING_MESH_ALG_COUNT; i++) {
            if (mesh->stats.algorithm_usage[i] > 0) {
                fprintf(stderr, "  %s: %" PRIu64 "\n",
                        linguistics_mesh_algorithm_to_string(i),
                        mesh->stats.algorithm_usage[i]);
            }
        }
    }
}

const char* linguistics_mesh_get_last_error(void) {
    return g_mesh_error;
}

/* ============================================================================
 * STRING CONVERSION IMPLEMENTATION
 * ============================================================================ */

const char* linguistics_mesh_state_to_string(mesh_convergence_state_t state) {
    switch (state) {
        case MESH_STATE_IDLE:         return "IDLE";
        case MESH_STATE_BROADCASTING: return "BROADCASTING";
        case MESH_STATE_COLLECTING:   return "COLLECTING";
        case MESH_STATE_PROPAGATING:  return "PROPAGATING";
        case MESH_STATE_CONVERGING:   return "CONVERGING";
        case MESH_STATE_VOTING:       return "VOTING";
        case MESH_STATE_CONVERGED:    return "CONVERGED";
        case MESH_STATE_FAILED:       return "FAILED";
        case MESH_STATE_TIMEOUT:      return "TIMEOUT";
        default:                      return "UNKNOWN";
    }
}

const char* linguistics_request_type_to_string(linguistics_request_type_t type) {
    switch (type) {
        case LING_REQUEST_PARSE_SPATIAL:        return "PARSE_SPATIAL";
        case LING_REQUEST_PARSE_NUMBER:         return "PARSE_NUMBER";
        case LING_REQUEST_ENCODE_PHONOLOGICAL:  return "ENCODE_PHONOLOGICAL";
        case LING_REQUEST_SELECT_FRAME:         return "SELECT_FRAME";
        case LING_REQUEST_GENERATE_NUMBER_WORD: return "GENERATE_NUMBER_WORD";
        case LING_REQUEST_REHEARSE:             return "REHEARSE";
        case LING_REQUEST_RETRIEVE:             return "RETRIEVE";
        case LING_REQUEST_SIMILARITY:           return "SIMILARITY";
        default:                                return "UNKNOWN";
    }
}

const char* linguistics_mesh_algorithm_to_string(linguistics_mesh_algorithm_t algorithm) {
    switch (algorithm) {
        case LING_MESH_ALG_FEP_BASIC:            return "FEP_BASIC";
        case LING_MESH_ALG_BELIEF_PROPAGATION:   return "BELIEF_PROPAGATION";
        case LING_MESH_ALG_LOOPY_BP:             return "LOOPY_BP";
        case LING_MESH_ALG_WEIGHTED_CONSENSUS:   return "WEIGHTED_CONSENSUS";
        case LING_MESH_ALG_METROPOLIS_HASTINGS:  return "METROPOLIS_HASTINGS";
        case LING_MESH_ALG_LAPLACIAN_CONSENSUS:  return "LAPLACIAN_CONSENSUS";
        case LING_MESH_ALG_NATURAL_GRADIENT:     return "NATURAL_GRADIENT";
        case LING_MESH_ALG_COVARIANCE_INTERSECT: return "COVARIANCE_INTERSECTION";
        case LING_MESH_ALG_HYBRID:               return "HYBRID";
        default:                                 return "UNKNOWN";
    }
}

const char* linguistics_mesh_metric_to_string(linguistics_mesh_metric_t metric) {
    switch (metric) {
        case LING_MESH_METRIC_COSINE:       return "COSINE";
        case LING_MESH_METRIC_EUCLIDEAN:    return "EUCLIDEAN";
        case LING_MESH_METRIC_KL_DIVERGENCE:return "KL_DIVERGENCE";
        case LING_MESH_METRIC_MUTUAL_INFO:  return "MUTUAL_INFO";
        default:                            return "UNKNOWN";
    }
}
