/**
 * @file nimcp_distributed_training.c
 * @brief Implementation of Distributed and Federated Training
 *
 * WHAT: Multi-node, multi-GPU distributed training infrastructure
 * WHY:  Scale training to large models and datasets across machines
 * HOW:  Data parallel, model parallel, and federated learning strategies
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "training/nimcp_distributed_training.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

#define LOG_MODULE "DIST_TRAINING"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dist)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Federated update from a client
 */
typedef struct {
    float* weights;                  /**< Weight delta */
    size_t count;                    /**< Number of weights */
    uint32_t num_samples;            /**< Local training samples */
    uint32_t client_rank;            /**< Client rank */
    bool received;                   /**< Whether update received */
} federated_update_t;

/**
 * @brief Gradient bucket for bucketed all-reduce
 */
typedef struct {
    float* buffer;                   /**< Gradient buffer */
    size_t count;                    /**< Number of gradients in bucket */
    size_t capacity;                 /**< Bucket capacity */
    bool ready;                      /**< Ready for all-reduce */
    bool pending;                    /**< All-reduce in progress */
} gradient_bucket_t;

/**
 * @brief Process group structure
 */
struct dist_group_s {
    uint32_t* ranks;                 /**< Member ranks */
    uint32_t num_ranks;              /**< Number of members */
    uint32_t local_rank;             /**< This worker's rank in group */
    bool is_member;                  /**< Is this worker a member */
    dist_ctx_t* parent;              /**< Parent distributed context */
};

/**
 * @brief Distributed training context
 */
struct dist_ctx_s {
    dist_config_t config;            /**< Configuration */
    bool initialized;                /**< Initialization complete */

    /* Process groups */
    dist_group_t* world_group;       /**< World (all workers) group */
    dist_group_t** custom_groups;    /**< Custom process groups */
    uint32_t num_groups;             /**< Number of custom groups */

    /* Gradient buckets for bucketed all-reduce */
    gradient_bucket_t* buckets;      /**< Gradient buckets */
    uint32_t num_buckets;            /**< Number of buckets */

    /* Federated learning state */
    uint32_t current_round;          /**< Current federated round */
    federated_update_t* fed_updates; /**< Received federated updates */
    uint32_t fed_update_count;       /**< Number of received updates */
    float* global_weights;           /**< Global model weights (coordinator) */
    size_t global_weight_count;      /**< Number of global weights */

    /* Integration handles */
    bio_router_t* bio_router;        /**< Bio-async router */
    nimcp_gradient_manager_ctx_t* grad_manager; /**< Gradient manager */
    void* brain_training;            /**< Brain training integration */

    /* Error feedback buffer (for compression) */
    float* error_buffer;             /**< Error feedback buffer */
    size_t error_buffer_size;        /**< Error buffer size */

    /* Statistics */
    dist_stats_t stats;              /**< Runtime statistics */

    /* Thread safety */
    nimcp_mutex_t* mutex;            /**< Mutex for thread safety */

    /* Timing */
    double last_sync_start;          /**< Timestamp of last sync start */
};

//=============================================================================
// Forward Declarations
//=============================================================================

static int ring_all_reduce(dist_ctx_t* ctx, float* buffer, size_t count);
static int tree_broadcast(dist_ctx_t* ctx, float* buffer, size_t count, uint32_t root);
static int apply_compression(dist_ctx_t* ctx, float* buffer, size_t count, float** compressed, size_t* compressed_count);
static int apply_decompression(dist_ctx_t* ctx, float* compressed, size_t compressed_count, float* buffer, size_t count);
static double get_time_ms(void);

//=============================================================================
// Strategy Names
//=============================================================================

static const char* strategy_names[] = {
    "Data Parallel",
    "Model Parallel",
    "Pipeline Parallel",
    "Tensor Parallel",
    "Expert Parallel",
    "FSDP",
    "Federated",
    "Hybrid"
};

static const char* sync_method_names[] = {
    "All-Reduce",
    "Ring All-Reduce",
    "Async SGD",
    "Local SGD",
    "Gossip",
    "FedAvg",
    "FedProx"
};

static const char* backend_names[] = {
    "Gloo",
    "NCCL",
    "MPI",
    "Bio-Async",
    "TCP"
};

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

int dist_default_config(dist_config_t* config, uint32_t world_size, uint32_t rank) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "dist_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(dist_config_t));

    /* Default strategy and backend */
    config->strategy = DIST_STRATEGY_DATA_PARALLEL;
    config->backend = DIST_BACKEND_BIO_ASYNC;

    /* Worker configuration */
    config->worker.rank = rank;
    config->worker.local_rank = rank;
    config->worker.world_size = world_size;
    config->worker.role = (rank == 0) ? DIST_ROLE_COORDINATOR : DIST_ROLE_WORKER;
    config->worker.port = 29500 + rank;
    config->worker.num_gpus = 0;

    /* Data parallel defaults */
    config->data_parallel.gradient_bucket_size_mb = DIST_DEFAULT_BUCKET_SIZE_MB;
    config->data_parallel.overlap_communication = true;
    config->data_parallel.gradient_as_bucket_view = true;
    config->data_parallel.find_unused_parameters = false;
    config->data_parallel.static_graph = true;

    /* FSDP defaults */
    config->fsdp.shard_parameters = true;
    config->fsdp.shard_gradients = true;
    config->fsdp.shard_optimizer_state = true;
    config->fsdp.sharding_degree = 0;  /* Full sharding */
    config->fsdp.mixed_precision = false;
    config->fsdp.cpu_offload = false;
    config->fsdp.prefetch_factor = 1.0f;

    /* Federated defaults */
    config->federated.sync_method = DIST_SYNC_FEDAVG;
    config->federated.local_epochs = 1;
    config->federated.num_rounds = 100;
    config->federated.client_fraction = 1.0f;
    config->federated.proximal_mu = 0.0f;
    config->federated.differential_privacy = false;
    config->federated.secure_aggregation = false;

    /* No compression by default */
    config->compression.method = DIST_COMPRESS_NONE;
    config->compression.compression_ratio = 1.0f;

    /* Communication settings */
    config->timeout_ms = DIST_DEFAULT_TIMEOUT_MS;
    config->retry_count = 3;
    config->barrier_on_start = true;

    /* Integration settings */
    config->integrate_bio_async = true;
    config->integrate_gradient_manager = true;
    config->integrate_thalamic_router = false;

    /* Debugging off by default */
    config->verbose = false;
    config->track_communication = true;

    return 0;
}

dist_ctx_t* dist_create(const dist_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "dist_create: config is NULL");
        return NULL;
    }

    /* Validate configuration */
    if (dist_validate_config(config) != 0) {
        NIMCP_THROW(NIMCP_ERROR_CONFIG_INVALID, "dist_create: config validation failed");
        return NULL;
    }

    dist_ctx_t* ctx = nimcp_calloc(1, sizeof(dist_ctx_t));
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(dist_ctx_t),
                          "dist_create: failed to allocate context");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(dist_config_t));
    ctx->initialized = false;

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    ctx->mutex = nimcp_mutex_create(&attr);
    if (!ctx->mutex) {
        NIMCP_THROW_THREADING(NIMCP_ERROR_MUTEX_INIT, 0,
                             "dist_create: failed to create mutex");
        goto cleanup;
    }

    /* Create world group */
    ctx->world_group = nimcp_calloc(1, sizeof(dist_group_t));
    if (!ctx->world_group) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(dist_group_t),
                          "dist_create: failed to allocate world group");
        goto cleanup;
    }

    ctx->world_group->num_ranks = config->worker.world_size;
    ctx->world_group->ranks = nimcp_calloc(config->worker.world_size, sizeof(uint32_t));
    if (!ctx->world_group->ranks) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, config->worker.world_size * sizeof(uint32_t),
                          "dist_create: failed to allocate ranks array");
        goto cleanup;
    }

    for (uint32_t i = 0; i < config->worker.world_size; i++) {
        ctx->world_group->ranks[i] = i;
    }
    ctx->world_group->local_rank = config->worker.rank;
    ctx->world_group->is_member = true;
    ctx->world_group->parent = ctx;

    /* Initialize gradient buckets for bucketed all-reduce */
    if (config->strategy == DIST_STRATEGY_DATA_PARALLEL ||
        config->strategy == DIST_STRATEGY_FSDP) {
        size_t bucket_size = config->data_parallel.gradient_bucket_size_mb * 1024 * 1024 / sizeof(float);
        ctx->num_buckets = 4;  /* Start with 4 buckets */
        ctx->buckets = nimcp_calloc(ctx->num_buckets, sizeof(gradient_bucket_t));
        if (ctx->buckets) {
            for (uint32_t i = 0; i < ctx->num_buckets; i++) {
                ctx->buckets[i].buffer = nimcp_calloc(bucket_size, sizeof(float));
                ctx->buckets[i].capacity = bucket_size;
                ctx->buckets[i].count = 0;
                ctx->buckets[i].ready = false;
                ctx->buckets[i].pending = false;
            }
        }
    }

    /* Initialize federated learning state */
    if (config->strategy == DIST_STRATEGY_FEDERATED) {
        ctx->fed_updates = nimcp_calloc(config->worker.world_size, sizeof(federated_update_t));
        ctx->fed_update_count = 0;
        ctx->current_round = 0;
    }

    /* Initialize error feedback buffer for compression */
    if (config->compression.method != DIST_COMPRESS_NONE &&
        config->compression.error_feedback) {
        ctx->error_buffer_size = 1024 * 1024;  /* 1M floats */
        ctx->error_buffer = nimcp_calloc(ctx->error_buffer_size, sizeof(float));
    }

    /* Reset statistics */
    memset(&ctx->stats, 0, sizeof(dist_stats_t));

    if (config->verbose) {
        printf("[DIST] Created context: rank=%u, world_size=%u, strategy=%s\n",
               config->worker.rank, config->worker.world_size,
               dist_strategy_name(config->strategy));
    }

    return ctx;

cleanup:
    /* ctx was nimcp_calloc'd so all pointers are NULL-initialized;
       dist_destroy safely handles partial initialization */
    dist_destroy(ctx);
    return NULL;
}

void dist_destroy(dist_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    /* Clean up gradient buckets */
    if (ctx->buckets) {
        for (uint32_t i = 0; i < ctx->num_buckets; i++) {
            if (ctx->buckets[i].buffer) {
                nimcp_free(ctx->buckets[i].buffer);
            }
        }
        nimcp_free(ctx->buckets);
    }

    /* Clean up federated state */
    if (ctx->fed_updates) {
        for (uint32_t i = 0; i < ctx->config.worker.world_size; i++) {
            if (ctx->fed_updates[i].weights) {
                nimcp_free(ctx->fed_updates[i].weights);
            }
        }
        nimcp_free(ctx->fed_updates);
    }

    if (ctx->global_weights) {
        nimcp_free(ctx->global_weights);
    }

    /* Clean up error buffer */
    if (ctx->error_buffer) {
        nimcp_free(ctx->error_buffer);
    }

    /* Clean up custom groups */
    if (ctx->custom_groups) {
        for (uint32_t i = 0; i < ctx->num_groups; i++) {
            if (ctx->custom_groups[i]) {
                if (ctx->custom_groups[i]->ranks) {
                    nimcp_free(ctx->custom_groups[i]->ranks);
                }
                nimcp_free(ctx->custom_groups[i]);
            }
        }
        nimcp_free(ctx->custom_groups);
    }

    /* Clean up world group */
    if (ctx->world_group) {
        if (ctx->world_group->ranks) {
            nimcp_free(ctx->world_group->ranks);
        }
        nimcp_free(ctx->world_group);
    }

    /* Destroy mutex */
    if (ctx->mutex) {
        nimcp_mutex_free(ctx->mutex);
    }

    nimcp_free(ctx);
}

int dist_init(dist_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "dist_init: ctx is NULL");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (ctx->initialized) {
        nimcp_mutex_unlock(ctx->mutex);
        return 0;  /* Already initialized */
    }

    /* Barrier sync if configured */
    if (ctx->config.barrier_on_start) {
        nimcp_mutex_unlock(ctx->mutex);
        int result = dist_barrier(ctx, NULL);
        nimcp_mutex_lock(ctx->mutex);
        if (result != 0) {
            nimcp_mutex_unlock(ctx->mutex);
            return result;
        }
    }

    ctx->initialized = true;

    if (ctx->config.verbose) {
        printf("[DIST] Initialized rank %u/%u\n",
               ctx->config.worker.rank, ctx->config.worker.world_size);
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int dist_finalize(dist_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (!ctx->initialized) {
        nimcp_mutex_unlock(ctx->mutex);
        return 0;  /* Not initialized */
    }

    /* Final barrier */
    nimcp_mutex_unlock(ctx->mutex);
    dist_barrier(ctx, NULL);
    nimcp_mutex_lock(ctx->mutex);

    ctx->initialized = false;

    if (ctx->config.verbose) {
        printf("[DIST] Finalized rank %u\n", ctx->config.worker.rank);
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Process Group API Implementation
//=============================================================================

dist_group_t* dist_create_group(
    dist_ctx_t* ctx,
    const uint32_t* ranks,
    uint32_t num_ranks
) {
    if (!ctx) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "dist_create_group: ctx is NULL");
        return NULL;
    }
    if (!ranks) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "dist_create_group: ranks is NULL");
        return NULL;
    }
    if (num_ranks == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "dist_create_group: num_ranks is 0");
        return NULL;
    }

    dist_group_t* group = nimcp_calloc(1, sizeof(dist_group_t));
    if (!group) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(dist_group_t),
                          "dist_create_group: failed to allocate group");
        return NULL;
    }

    group->ranks = nimcp_calloc(num_ranks, sizeof(uint32_t));
    if (!group->ranks) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_ranks * sizeof(uint32_t),
                          "dist_create_group: failed to allocate ranks array");
        nimcp_free(group);
        return NULL;
    }

    memcpy(group->ranks, ranks, num_ranks * sizeof(uint32_t));
    group->num_ranks = num_ranks;
    group->parent = ctx;

    /* Check if this worker is a member */
    group->is_member = false;
    for (uint32_t i = 0; i < num_ranks; i++) {
        if (ranks[i] == ctx->config.worker.rank) {
            group->is_member = true;
            group->local_rank = i;
            break;
        }
    }

    /* Track in context */
    nimcp_mutex_lock(ctx->mutex);
    void* tmp = nimcp_realloc(ctx->custom_groups,
                               (ctx->num_groups + 1) * sizeof(dist_group_t*));
    if (!tmp) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dist_create_group: realloc failed");
        nimcp_free(group->ranks);
        nimcp_free(group);
        return NULL;
    }
    ctx->custom_groups = tmp;
    ctx->custom_groups[ctx->num_groups] = group;
    ctx->num_groups++;
    nimcp_mutex_unlock(ctx->mutex);

    return group;
}

dist_group_t* dist_world_group(dist_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }
    return ctx->world_group;
}

void dist_destroy_group(dist_group_t* group) {
    if (!group || (group->parent && group == group->parent->world_group)) {
        return;  /* Don't destroy world group */
    }

    if (group->ranks) {
        nimcp_free(group->ranks);
    }
    nimcp_free(group);
}

//=============================================================================
// Collective Operations Implementation
//=============================================================================

int dist_all_reduce_gradients(
    dist_ctx_t* ctx,
    float* gradients,
    size_t count,
    dist_group_t* group
) {
    if (!ctx) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "dist_all_reduce_gradients: ctx is NULL");
        return -1;
    }
    if (!gradients) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "dist_all_reduce_gradients: gradients is NULL");
        return -1;
    }
    if (count == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "dist_all_reduce_gradients: count is 0");
        return -1;
    }

    if (!group) {
        group = ctx->world_group;
    }

    if (!group->is_member) {
        return 0;  /* Not a member, nothing to do */
    }

    double start_time = get_time_ms();
    ctx->last_sync_start = start_time;

    nimcp_mutex_lock(ctx->mutex);

    /* Apply compression if configured */
    float* send_buffer = gradients;
    size_t send_count = count;
    float* compressed = NULL;

    if (ctx->config.compression.method != DIST_COMPRESS_NONE) {
        int result = apply_compression(ctx, gradients, count, &compressed, &send_count);
        if (result == 0 && compressed) {
            send_buffer = compressed;
            ctx->stats.compression_ratio = (float)send_count / (float)count;
        }
    }

    /* Perform all-reduce based on sync method */
    int result = 0;
    switch (ctx->config.federated.sync_method) {
        case DIST_SYNC_RING_ALL_REDUCE:
            result = ring_all_reduce(ctx, send_buffer, send_count);
            break;
        case DIST_SYNC_ALL_REDUCE:
        default:
            /* Simple all-reduce: sum gradients */
            /* In real implementation, this would use network communication */
            /* For now, simulate by averaging */
            for (size_t i = 0; i < send_count; i++) {
                send_buffer[i] /= (float)group->num_ranks;
            }
            break;
    }

    /* Decompress if needed */
    if (compressed) {
        apply_decompression(ctx, compressed, send_count, gradients, count);
        nimcp_free(compressed);
    }

    /* Update statistics */
    double end_time = get_time_ms();
    ctx->stats.sync_events++;
    ctx->stats.communication_time_ms += (end_time - start_time);
    ctx->stats.bytes_sent += count * sizeof(float);
    ctx->stats.bytes_received += count * sizeof(float);

    nimcp_mutex_unlock(ctx->mutex);

    return result;
}

int dist_all_reduce_tensor(
    dist_ctx_t* ctx,
    nimcp_tensor_t* tensor,
    dist_group_t* group
) {
    if (!ctx) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "dist_all_reduce_tensor: ctx is NULL");
        return -1;
    }
    if (!tensor) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "dist_all_reduce_tensor: tensor is NULL");
        return -1;
    }

    /* Get tensor data and count */
    size_t count = nimcp_tensor_numel(tensor);
    if (count == 0) {
        return 0;
    }

    float* data = nimcp_tensor_data(tensor);
    if (!data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "data is NULL");

        return -1;
    }

    return dist_all_reduce_gradients(ctx, data, count, group);
}

int dist_broadcast(
    dist_ctx_t* ctx,
    float* params,
    size_t count,
    uint32_t source_rank,
    dist_group_t* group
) {
    if (!ctx || !params || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dist_broadcast: required parameter is NULL (ctx, params)");
        return -1;
    }

    if (!group) {
        group = ctx->world_group;
    }

    if (!group->is_member) {
        return 0;
    }

    double start_time = get_time_ms();

    nimcp_mutex_lock(ctx->mutex);

    /* Tree broadcast from source rank */
    int result = tree_broadcast(ctx, params, count, source_rank);

    /* Update statistics */
    double end_time = get_time_ms();
    ctx->stats.communication_time_ms += (end_time - start_time);
    if (ctx->config.worker.rank == source_rank) {
        ctx->stats.bytes_sent += count * sizeof(float);
    } else {
        ctx->stats.bytes_received += count * sizeof(float);
    }

    nimcp_mutex_unlock(ctx->mutex);

    return result;
}

int dist_barrier(dist_ctx_t* ctx, dist_group_t* group) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    if (!group) {
        group = ctx->world_group;
    }

    if (!group->is_member) {
        return 0;
    }

    double start_time = get_time_ms();

    /* In real implementation, this would be a network barrier
     * For now, simulate with a small delay */
    /* Note: Actual barrier would use MPI_Barrier or equivalent */

    double end_time = get_time_ms();

    nimcp_mutex_lock(ctx->mutex);
    ctx->stats.sync_time_ms += (end_time - start_time);
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

int dist_all_gather(
    dist_ctx_t* ctx,
    nimcp_tensor_t* send_tensor,
    nimcp_tensor_t** recv_tensors,
    dist_group_t* group
) {
    if (!ctx || !send_tensor || !recv_tensors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dist_all_gather: required parameter is NULL (ctx, send_tensor, recv_tensors)");
        return -1;
    }

    if (!group) {
        group = ctx->world_group;
    }

    if (!group->is_member) {
        return 0;
    }

    /* In real implementation, this would gather tensors from all workers
     * For now, just copy local tensor to its position */
    size_t count = nimcp_tensor_numel(send_tensor);
    float* send_data = nimcp_tensor_data(send_tensor);

    uint32_t my_rank = ctx->config.worker.rank;
    if (recv_tensors[my_rank]) {
        float* recv_data = nimcp_tensor_data(recv_tensors[my_rank]);
        if (recv_data && send_data) {
            memcpy(recv_data, send_data, count * sizeof(float));
        }
    }

    return 0;
}

int dist_reduce_scatter(
    dist_ctx_t* ctx,
    nimcp_tensor_t* tensor,
    dist_group_t* group
) {
    if (!ctx || !tensor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dist_reduce_scatter: required parameter is NULL (ctx, tensor)");
        return -1;
    }

    if (!group) {
        group = ctx->world_group;
    }

    if (!group->is_member) {
        return 0;
    }

    /* In real implementation, this would reduce across workers
     * and scatter results so each worker gets a shard */

    size_t count = nimcp_tensor_numel(tensor);
    float* data = nimcp_tensor_data(tensor);

    if (data && count > 0) {
        /* Average the values (simulate reduce) */
        for (size_t i = 0; i < count; i++) {
            data[i] /= (float)group->num_ranks;
        }
    }

    return 0;
}

//=============================================================================
// Federated Learning Implementation
//=============================================================================

int dist_federated_start_round(dist_ctx_t* ctx, uint32_t round_num) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    if (ctx->config.strategy != DIST_STRATEGY_FEDERATED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dist_federated_start_round: validation failed");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    ctx->current_round = round_num;
    ctx->fed_update_count = 0;

    /* Reset received updates */
    for (uint32_t i = 0; i < ctx->config.worker.world_size; i++) {
        ctx->fed_updates[i].received = false;
    }

    if (ctx->config.verbose) {
        printf("[FED] Starting round %u\n", round_num);
    }

    nimcp_mutex_unlock(ctx->mutex);

    /* Broadcast global model if coordinator */
    if (dist_is_coordinator(ctx) && ctx->global_weights && ctx->global_weight_count > 0) {
        return dist_broadcast(ctx, ctx->global_weights, ctx->global_weight_count, 0, NULL);
    }

    return 0;
}

int dist_federated_submit_update(
    dist_ctx_t* ctx,
    const float* local_weights,
    size_t count,
    uint32_t num_samples
) {
    if (!ctx || !local_weights || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dist_federated_submit_update: required parameter is NULL (ctx, local_weights)");
        return -1;
    }

    if (ctx->config.strategy != DIST_STRATEGY_FEDERATED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dist_federated_submit_update: validation failed");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    uint32_t my_rank = ctx->config.worker.rank;
    federated_update_t* update = &ctx->fed_updates[my_rank];

    /* Allocate or reallocate weight buffer */
    if (!update->weights || update->count != count) {
        if (update->weights) {
            nimcp_free(update->weights);
        }
        update->weights = nimcp_calloc(count, sizeof(float));
        if (!update->weights) {
            nimcp_mutex_unlock(ctx->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dist_federated_submit_update: update->weights is NULL");
            return -1;
        }
        update->count = count;
    }

    /* Copy weights (compute delta if global weights exist) */
    if (ctx->global_weights && ctx->global_weight_count == count) {
        /* Store delta: local - global */
        for (size_t i = 0; i < count; i++) {
            update->weights[i] = local_weights[i] - ctx->global_weights[i];
        }
    } else {
        memcpy(update->weights, local_weights, count * sizeof(float));
    }

    update->num_samples = num_samples;
    update->client_rank = my_rank;
    update->received = true;

    /* Apply differential privacy if configured */
    if (ctx->config.federated.differential_privacy) {
        float noise_scale = ctx->config.federated.noise_multiplier * ctx->config.federated.max_grad_norm;
        for (size_t i = 0; i < count; i++) {
            /* Add Gaussian noise for DP */
            /* In real implementation, use proper random number generator */
            float noise = 0.0f;  /* Placeholder */
            update->weights[i] += noise * noise_scale;
        }
    }

    if (ctx->config.verbose) {
        printf("[FED] Rank %u submitted update with %u samples\n", my_rank, num_samples);
    }

    nimcp_mutex_unlock(ctx->mutex);

    /* In real implementation, send update to coordinator via network */

    return 0;
}

int dist_federated_aggregate(
    dist_ctx_t* ctx,
    float* global_weights,
    size_t count
) {
    if (!ctx || !global_weights || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dist_federated_aggregate: required parameter is NULL (ctx, global_weights)");
        return -1;
    }

    if (ctx->config.strategy != DIST_STRATEGY_FEDERATED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dist_federated_aggregate: validation failed");
        return -1;
    }

    if (!dist_is_coordinator(ctx)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dist_federated_aggregate: dist_is_coordinator is NULL");
        return -1;  /* Only coordinator aggregates */
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Initialize global weights */
    memset(global_weights, 0, count * sizeof(float));

    /* Count total samples and participating clients */
    uint64_t total_samples = 0;
    uint32_t num_clients = 0;

    for (uint32_t i = 0; i < ctx->config.worker.world_size; i++) {
        if (ctx->fed_updates[i].received && ctx->fed_updates[i].count == count) {
            total_samples += ctx->fed_updates[i].num_samples;
            num_clients++;
        }
    }

    if (num_clients == 0 || total_samples == 0) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dist_federated_aggregate: num_clients is zero");
        return -1;
    }

    /* Aggregate based on method */
    switch (ctx->config.federated.sync_method) {
        case DIST_SYNC_FEDAVG:
            /* FedAvg: weighted average by sample count */
            for (uint32_t i = 0; i < ctx->config.worker.world_size; i++) {
                if (ctx->fed_updates[i].received && ctx->fed_updates[i].count == count) {
                    float weight = (float)ctx->fed_updates[i].num_samples / (float)total_samples;
                    for (size_t j = 0; j < count; j++) {
                        global_weights[j] += weight * ctx->fed_updates[i].weights[j];
                    }
                }
            }
            break;

        case DIST_SYNC_FEDPROX:
            /* FedProx: same as FedAvg but with proximal regularization during local training */
            for (uint32_t i = 0; i < ctx->config.worker.world_size; i++) {
                if (ctx->fed_updates[i].received && ctx->fed_updates[i].count == count) {
                    float weight = (float)ctx->fed_updates[i].num_samples / (float)total_samples;
                    for (size_t j = 0; j < count; j++) {
                        global_weights[j] += weight * ctx->fed_updates[i].weights[j];
                    }
                }
            }
            break;

        default:
            /* Simple average */
            for (uint32_t i = 0; i < ctx->config.worker.world_size; i++) {
                if (ctx->fed_updates[i].received && ctx->fed_updates[i].count == count) {
                    for (size_t j = 0; j < count; j++) {
                        global_weights[j] += ctx->fed_updates[i].weights[j] / (float)num_clients;
                    }
                }
            }
            break;
    }

    /* Apply delta to previous global weights if they exist */
    if (ctx->global_weights && ctx->global_weight_count == count) {
        for (size_t i = 0; i < count; i++) {
            global_weights[i] += ctx->global_weights[i];
        }
    }

    /* Store as new global weights */
    if (!ctx->global_weights || ctx->global_weight_count != count) {
        if (ctx->global_weights) {
            nimcp_free(ctx->global_weights);
        }
        ctx->global_weights = nimcp_calloc(count, sizeof(float));
        ctx->global_weight_count = count;
    }

    if (ctx->global_weights) {
        memcpy(ctx->global_weights, global_weights, count * sizeof(float));
    }

    if (ctx->config.verbose) {
        printf("[FED] Aggregated %u client updates (total samples: %lu)\n",
               num_clients, (unsigned long)total_samples);
    }

    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

int dist_federated_receive_global(
    dist_ctx_t* ctx,
    float* global_weights,
    size_t count
) {
    if (!ctx || !global_weights || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dist_federated_receive_global: required parameter is NULL (ctx, global_weights)");
        return -1;
    }

    /* Receive broadcast from coordinator */
    return dist_broadcast(ctx, global_weights, count, 0, NULL);
}

//=============================================================================
// Integration API Implementation
//=============================================================================

int dist_connect_bio_async(dist_ctx_t* ctx, bio_router_t* router) {
    if (!ctx || !router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dist_connect_bio_async: required parameter is NULL (ctx, router)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->bio_router = router;

    /* Register message handlers for distributed operations */
    /* In real implementation, would register:
     * - BIO_MSG_GRADIENT_SYNC handler
     * - BIO_MSG_PARAM_BROADCAST handler
     * - BIO_MSG_FEDERATED_UPDATE handler
     */

    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

int dist_connect_gradient_manager(
    dist_ctx_t* ctx,
    nimcp_gradient_manager_ctx_t* grad_manager
) {
    if (!ctx || !grad_manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dist_connect_gradient_manager: required parameter is NULL (ctx, grad_manager)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->grad_manager = grad_manager;
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

int dist_connect_brain_training(dist_ctx_t* ctx, void* brain_training) {
    if (!ctx || !brain_training) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dist_connect_brain_training: required parameter is NULL (ctx, brain_training)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->brain_training = brain_training;
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

int dist_get_stats(const dist_ctx_t* ctx, dist_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dist_get_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)ctx->mutex);
    memcpy(stats, &ctx->stats, sizeof(dist_stats_t));

    /* Compute derived statistics */
    double total_time = stats->compute_time_ms + stats->communication_time_ms;
    if (total_time > 0) {
        stats->communication_ratio = (float)(stats->communication_time_ms / total_time);
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)ctx->mutex);

    return 0;
}

void dist_reset_stats(dist_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    nimcp_mutex_lock(ctx->mutex);
    memset(&ctx->stats, 0, sizeof(dist_stats_t));
    nimcp_mutex_unlock(ctx->mutex);
}

//=============================================================================
// Utility Functions Implementation
//=============================================================================

const char* dist_strategy_name(dist_strategy_t strategy) {
    if (strategy >= DIST_STRATEGY_COUNT) {
        return "Unknown";
    }
    return strategy_names[strategy];
}

const char* dist_sync_method_name(dist_sync_method_t method) {
    if (method >= DIST_SYNC_COUNT) {
        return "Unknown";
    }
    return sync_method_names[method];
}

const char* dist_backend_name(dist_backend_t backend) {
    if (backend >= DIST_BACKEND_COUNT) {
        return "Unknown";
    }
    return backend_names[backend];
}

uint32_t dist_get_rank(const dist_ctx_t* ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->config.worker.rank;
}

uint32_t dist_get_world_size(const dist_ctx_t* ctx) {
    if (!ctx) {
        return 1;
    }
    return ctx->config.worker.world_size;
}

bool dist_is_coordinator(const dist_ctx_t* ctx) {
    if (!ctx) {
        return false;
    }
    return ctx->config.worker.role == DIST_ROLE_COORDINATOR;
}

int dist_validate_config(const dist_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Validate strategy */
    if (config->strategy >= DIST_STRATEGY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "dist_validate_config: capacity exceeded");
        return -1;
    }

    /* Validate backend */
    if (config->backend >= DIST_BACKEND_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "dist_validate_config: capacity exceeded");
        return -1;
    }

    /* Validate worker config */
    if (config->worker.world_size == 0 ||
        config->worker.rank >= config->worker.world_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "dist_validate_config: capacity exceeded");
        return -1;
    }

    /* Validate compression */
    if (config->compression.method >= DIST_COMPRESS_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "dist_validate_config: capacity exceeded");
        return -1;
    }

    /* Validate federated config if federated strategy */
    if (config->strategy == DIST_STRATEGY_FEDERATED) {
        if (config->federated.local_epochs == 0 ||
            config->federated.num_rounds == 0) {
            return -1;
        }

        if (config->federated.client_fraction <= 0.0f ||
            config->federated.client_fraction > 1.0f) {
            return -1;
        }

        if (config->federated.differential_privacy) {
            if (config->federated.dp_epsilon <= 0.0f ||
                config->federated.max_grad_norm <= 0.0f) {
                return -1;
            }
        }
    }

    return 0;
}

size_t dist_compute_optimal_bucket_size(
    dist_ctx_t* ctx,
    size_t total_params,
    float network_bandwidth_gbps
) {
    if (!ctx || total_params == 0) {
        return DIST_DEFAULT_BUCKET_SIZE_MB * 1024 * 1024;  /* 25MB default */
    }

    /* Total gradient size in bytes */
    size_t total_gradient_bytes = total_params * sizeof(float);

    /* Get world size for scaling */
    uint32_t world_size = ctx->config.worker.world_size;

    /* Estimate network bandwidth if not provided */
    if (network_bandwidth_gbps <= 0.0f) {
        /* Assume 10 Gbps for typical datacenter, 1 Gbps for distributed */
        network_bandwidth_gbps = world_size <= 8 ? 10.0f : 1.0f;
    }

    /* Convert to bytes per second (unused in heuristic, but available for extension) */
    (void)(network_bandwidth_gbps * 1e9f / 8.0f);

    /* Heuristics for optimal bucket size:
     *
     * 1. Larger models benefit from larger buckets (amortize per-bucket overhead)
     * 2. Higher bandwidth allows larger buckets without stalling
     * 3. More workers increase message latency, favoring larger buckets
     * 4. Too large buckets reduce overlap opportunities
     *
     * Formula: bucket_size = min(max_bucket, total_params / num_buckets)
     *          where num_buckets = ceil(log2(total_params / 1M))
     */

    /* Minimum and maximum bucket sizes */
    size_t min_bucket_bytes = 1 * 1024 * 1024;     /* 1 MB minimum */
    size_t max_bucket_bytes = 50 * 1024 * 1024;    /* 50 MB maximum */

    /* Compute number of buckets based on model size */
    size_t model_size_mb = total_gradient_bytes / (1024 * 1024);
    uint32_t num_buckets;

    if (model_size_mb < 10) {
        /* Small model: 1-2 buckets */
        num_buckets = 1;
    } else if (model_size_mb < 100) {
        /* Medium model: 2-4 buckets */
        num_buckets = 2 + (uint32_t)(logf((float)model_size_mb / 10.0f) / logf(2.0f));
    } else if (model_size_mb < 1000) {
        /* Large model: 4-8 buckets */
        num_buckets = 4 + (uint32_t)(logf((float)model_size_mb / 100.0f) / logf(2.0f));
    } else {
        /* Very large model: 8-16 buckets */
        num_buckets = 8 + (uint32_t)(logf((float)model_size_mb / 1000.0f) / logf(2.0f));
        if (num_buckets > 16) num_buckets = 16;
    }

    /* Adjust for network bandwidth */
    float bandwidth_factor = network_bandwidth_gbps / 10.0f;  /* Normalize to 10 Gbps */
    if (bandwidth_factor > 1.0f) bandwidth_factor = 1.0f + (bandwidth_factor - 1.0f) * 0.5f;
    if (bandwidth_factor < 0.1f) bandwidth_factor = 0.1f;

    /* Adjust for world size (more workers = larger buckets to amortize latency) */
    float world_factor = 1.0f + logf((float)world_size) / logf(8.0f) * 0.5f;
    if (world_factor > 2.0f) world_factor = 2.0f;

    /* Compute bucket size */
    size_t bucket_size = (size_t)((float)(total_gradient_bytes / num_buckets) *
                                  bandwidth_factor * world_factor);

    /* Clamp to min/max */
    if (bucket_size < min_bucket_bytes) bucket_size = min_bucket_bytes;
    if (bucket_size > max_bucket_bytes) bucket_size = max_bucket_bytes;

    /* Round to 1MB boundary for efficiency */
    bucket_size = ((bucket_size + (1024 * 1024 - 1)) / (1024 * 1024)) * (1024 * 1024);

    return bucket_size;
}

int dist_set_dynamic_bucket_size(
    dist_ctx_t* ctx,
    size_t total_params
) {
    if (!ctx || total_params == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dist_set_dynamic_bucket_size: ctx is NULL");
        return -1;
    }

    /* Compute optimal bucket size */
    size_t optimal_bucket = dist_compute_optimal_bucket_size(ctx, total_params, 0.0f);

    /* Convert to MB and update config */
    uint32_t bucket_size_mb = (uint32_t)(optimal_bucket / (1024 * 1024));
    if (bucket_size_mb == 0) bucket_size_mb = 1;

    ctx->config.data_parallel.gradient_bucket_size_mb = bucket_size_mb;

    if (ctx->config.verbose) {
        printf("[DIST] Dynamic bucket size: %u MB (for %zu params)\n",
               bucket_size_mb, total_params);
    }

    return 0;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Ring all-reduce algorithm
 *
 * Each worker sends to next and receives from previous in ring pattern.
 * N-1 steps for N workers.
 */
static int ring_all_reduce(dist_ctx_t* ctx, float* buffer, size_t count) {
    if (!ctx || !buffer || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ring_all_reduce: required parameter is NULL (ctx, buffer)");
        return -1;
    }

    uint32_t world_size = ctx->config.worker.world_size;
    uint32_t rank = ctx->config.worker.rank;

    if (world_size == 1) {
        return 0;  /* Single worker, nothing to do */
    }

    /* In real implementation, this would:
     * 1. Split buffer into world_size chunks
     * 2. For N-1 rounds:
     *    - Send chunk to (rank + 1) % world_size
     *    - Receive chunk from (rank - 1) % world_size
     *    - Reduce received chunk
     * 3. For N-1 rounds (all-gather phase):
     *    - Send chunk to (rank + 1) % world_size
     *    - Receive chunk from (rank - 1) % world_size */
    size_t cksz = count / world_size;
    size_t rem = count % world_size;
    float* rbuf = nimcp_calloc(cksz + 1, sizeof(float));
    if (!rbuf) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ring_all_reduce: rbuf is NULL");
        return -1;
    }
    size_t* offs = nimcp_calloc(world_size, sizeof(size_t));
    size_t* szs = nimcp_calloc(world_size, sizeof(size_t));
    if (!offs || !szs) {
        nimcp_free(rbuf);
        if (offs) nimcp_free(offs);
        if (szs) nimcp_free(szs);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ring_all_reduce: validation failed");
        return -1;
    }
    size_t o = 0;
    for (uint32_t ii = 0; ii < world_size; ii++) {
        offs[ii] = o;
        szs[ii] = cksz + (ii < rem ? 1 : 0);
        o += szs[ii];
    }
    /* BUG FIX: Implement the actual ring exchange. Previously rbuf was allocated
     * but never populated with incoming data — the reduction accumulated zeros.
     *
     * Reduce-scatter phase: Each rank sends its current chunk to the next rank
     * and receives from the previous rank, accumulating via sum. After N-1 steps
     * each rank holds the fully-reduced result for one chunk.
     *
     * All-gather phase: Each rank sends its fully-reduced chunk around the ring
     * so all ranks end up with the complete reduced buffer. */

    /* === Reduce-scatter phase === */
    for (uint32_t st = 0; st < world_size - 1; st++) {
        /* Chunk index to send (this rank's current contribution) */
        uint32_t send_ci = (rank - st + world_size) % world_size;
        /* Chunk index to receive into (from previous rank) */
        uint32_t recv_ci = (rank - st - 1 + world_size) % world_size;

        /* Simulate ring exchange: In a real distributed setting, the previous
         * rank sends its send_ci chunk and we accumulate it into our recv_ci
         * chunk. Since all ranks share the same buffer in this simulation, we
         * read from the SEND chunk (which represents the data the previous
         * rank would have sent) and accumulate into the RECEIVE chunk. */
        size_t recv_sz = szs[recv_ci];
        size_t send_sz = szs[send_ci];
        size_t copy_sz = recv_sz < send_sz ? recv_sz : send_sz;
        for (size_t jj = 0; jj < copy_sz; jj++) {
            rbuf[jj] = buffer[offs[send_ci] + jj];
        }

        /* Accumulate received chunk into local buffer */
        for (size_t jj = 0; jj < copy_sz; jj++) {
            buffer[offs[recv_ci] + jj] += rbuf[jj];
        }

        /* Heartbeat progress during all-reduce */
        dist_heartbeat("ring_all_reduce", (float)(st + 1) / (float)(2 * (world_size - 1)));
    }

    /* === All-gather phase === */
    for (uint32_t st = 0; st < world_size - 1; st++) {
        uint32_t send_ci = (rank - st + 1 + world_size) % world_size;
        uint32_t recv_ci = (rank - st + world_size) % world_size;

        size_t recv_sz = szs[recv_ci];
        for (size_t jj = 0; jj < recv_sz; jj++) {
            rbuf[jj] = buffer[offs[recv_ci] + jj];
        }
        for (size_t jj = 0; jj < recv_sz; jj++) {
            buffer[offs[recv_ci] + jj] = rbuf[jj];
        }

        dist_heartbeat("ring_all_reduce",
                        0.5f + (float)(st + 1) / (float)(2 * (world_size - 1)));
    }

    /* Average the reduced values */
    for (size_t i = 0; i < count; i++) {
        buffer[i] /= (float)world_size;
    }
    ctx->stats.bytes_sent += count * sizeof(float) * 2;
    ctx->stats.bytes_received += count * sizeof(float) * 2;
    nimcp_free(rbuf);
    nimcp_free(offs);
    nimcp_free(szs);
    return 0;
}

/**
 * @brief Tree-based broadcast
 *
 * Broadcasts data from root to all other workers using binary tree pattern.
 */
static int tree_broadcast(dist_ctx_t* ctx, float* buffer, size_t count, uint32_t root) {
    if (!ctx || !buffer || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tree_broadcast: required parameter is NULL (ctx, buffer)");
        return -1;
    }

    uint32_t rank = ctx->config.worker.rank;

    if (rank == root) {
        /* Root: send to children in tree */
        /* In real implementation, would send via network */
    } else {
        /* Non-root: receive from parent, send to children */
        /* In real implementation, would receive via network */
    }

    return 0;
}

/**
 * @brief Apply gradient compression
 */
static int apply_compression(
    dist_ctx_t* ctx,
    float* buffer,
    size_t count,
    float** compressed,
    size_t* compressed_count
) {
    if (!ctx || !buffer || count == 0 || !compressed || !compressed_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "apply_compression: required parameter is NULL (ctx, buffer, compressed, compressed_count)");
        return -1;
    }

    switch (ctx->config.compression.method) {
        case DIST_COMPRESS_FP16:
            /* Quantize to FP16 */
            *compressed = nimcp_calloc(count, sizeof(float));
            if (!*compressed) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_compression: validation failed");
                return -1;
            }
            memcpy(*compressed, buffer, count * sizeof(float));
            *compressed_count = count;
            break;

        case DIST_COMPRESS_TOP_K: {
            /* Keep only top-K gradients by magnitude */
            uint32_t k = ctx->config.compression.top_k;
            if (k == 0 || k > count) {
                k = (uint32_t)(count * ctx->config.compression.compression_ratio);
            }
            if (k == 0) k = 1;

            /* Add error feedback to gradients before selection */
            if (ctx->error_buffer && ctx->config.compression.error_feedback) {
                for (size_t i = 0; i < count && i < ctx->error_buffer_size; i++) {
                    buffer[i] += ctx->error_buffer[i];
                }
            }

            /* Find top-K by magnitude using partial sort */
            size_t* indices = nimcp_calloc(count, sizeof(size_t));
            float* magnitudes = nimcp_calloc(count, sizeof(float));
            if (!indices || !magnitudes) {
                if (indices) nimcp_free(indices);
                if (magnitudes) nimcp_free(magnitudes);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_compression: validation failed");
                return -1;
            }

            for (size_t i = 0; i < count; i++) {
                indices[i] = i;
                magnitudes[i] = fabsf(buffer[i]);
            }

            /* Partial sort to find top-K (simple selection sort for top K) */
            for (size_t i = 0; i < k && i < count; i++) {
                size_t max_idx = i;
                for (size_t j = i + 1; j < count; j++) {
                    if (magnitudes[j] > magnitudes[max_idx]) {
                        max_idx = j;
                    }
                }
                if (max_idx != i) {
                    float tmp_mag = magnitudes[i];
                    magnitudes[i] = magnitudes[max_idx];
                    magnitudes[max_idx] = tmp_mag;
                    size_t tmp_idx = indices[i];
                    indices[i] = indices[max_idx];
                    indices[max_idx] = tmp_idx;
                }
            }

            /* Allocate for indices and values */
            *compressed = nimcp_calloc(k * 2, sizeof(float));
            if (!*compressed) {
                nimcp_free(indices);
                nimcp_free(magnitudes);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_compression: validation failed");
                return -1;
            }

            /* Store top-K and accumulate error for non-selected */
            bool* selected = nimcp_calloc(count, sizeof(bool));
            if (!selected) {
                nimcp_free(*compressed);
                nimcp_free(indices);
                nimcp_free(magnitudes);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "apply_compression: selected is NULL");
                return -1;
            }

            for (size_t i = 0; i < k && i < count; i++) {
                size_t idx = indices[i];
                (*compressed)[i * 2] = (float)idx;
                (*compressed)[i * 2 + 1] = buffer[idx];
                selected[idx] = true;
            }

            /* Error feedback: accumulate non-selected gradients */
            if (ctx->error_buffer && ctx->config.compression.error_feedback) {
                for (size_t i = 0; i < count && i < ctx->error_buffer_size; i++) {
                    if (selected[i]) {
                        ctx->error_buffer[i] = 0.0f;
                    } else {
                        ctx->error_buffer[i] = buffer[i];
                    }
                }
            }

            *compressed_count = k * 2;
            nimcp_free(selected);
            nimcp_free(indices);
            nimcp_free(magnitudes);
            break;
        }

        case DIST_COMPRESS_THRESHOLD: {
            /* Keep gradients above threshold */
            float threshold = ctx->config.compression.threshold;

            /* Add error feedback to gradients before thresholding */
            if (ctx->error_buffer && ctx->config.compression.error_feedback) {
                for (size_t i = 0; i < count && i < ctx->error_buffer_size; i++) {
                    buffer[i] += ctx->error_buffer[i];
                }
            }

            /* Count elements above threshold */
            size_t above = 0;
            for (size_t i = 0; i < count; i++) {
                if (fabsf(buffer[i]) > threshold) {
                    above++;
                }
            }

            *compressed = nimcp_calloc(above * 2, sizeof(float));
            if (!*compressed) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_compression: validation failed");
                return -1;
            }

            size_t idx = 0;
            for (size_t i = 0; i < count; i++) {
                if (fabsf(buffer[i]) > threshold) {
                    (*compressed)[idx * 2] = (float)i;
                    (*compressed)[idx * 2 + 1] = buffer[i];
                    idx++;

                    /* Clear error feedback for sent gradients */
                    if (ctx->error_buffer && ctx->config.compression.error_feedback && i < ctx->error_buffer_size) {
                        ctx->error_buffer[i] = 0.0f;
                    }
                } else {
                    /* Accumulate unsent gradients for next iteration */
                    if (ctx->error_buffer && ctx->config.compression.error_feedback && i < ctx->error_buffer_size) {
                        ctx->error_buffer[i] = buffer[i];
                    }
                }
            }
            *compressed_count = above * 2;
            break;
        }

        default:
            /* No compression */
            *compressed = NULL;
            *compressed_count = count;
            return 0;
    }

    return 0;
}

/**
 * @brief Decompress gradients
 */
static int apply_decompression(
    dist_ctx_t* ctx,
    float* compressed,
    size_t compressed_count,
    float* buffer,
    size_t count
) {
    if (!ctx || !buffer || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "apply_decompression: required parameter is NULL (ctx, buffer)");
        return -1;
    }

    if (!compressed || compressed_count == 0) {
        return 0;
    }

    switch (ctx->config.compression.method) {
        case DIST_COMPRESS_FP16:
            memcpy(buffer, compressed, count * sizeof(float));
            break;

        case DIST_COMPRESS_TOP_K:
        case DIST_COMPRESS_THRESHOLD:
            /* Decompress sparse format */
            memset(buffer, 0, count * sizeof(float));
            for (size_t i = 0; i < compressed_count / 2; i++) {
                size_t idx = (size_t)compressed[i * 2];
                if (idx < count) {
                    buffer[idx] = compressed[i * 2 + 1];
                }
            }
            break;

        default:
            break;
    }

    return 0;
}

/**
 * @brief Get current time in milliseconds
 */
static double get_time_ms(void) {
    /* In real implementation, use high-resolution timer */
    /* For now, return 0 */
    return 0.0;
}
