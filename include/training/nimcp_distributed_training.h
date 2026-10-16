/**
 * @file nimcp_distributed_training.h
 * @brief Distributed and Federated Training for NIMCP
 *
 * WHAT: Multi-node, multi-GPU distributed training infrastructure
 * WHY:  Scale training to large models and datasets across machines
 * HOW:  Data parallel, model parallel, and federated learning strategies
 *
 * FRAMEWORK COMPARISON:
 * - PyTorch: DistributedDataParallel, FSDP, RPC
 * - JAX: pjit, pmap, mesh sharding
 * - TensorFlow: tf.distribute.Strategy, MultiWorkerMirroredStrategy
 *
 * NIMCP APPROACH:
 * - Leverages existing bio-async for inter-node messaging
 * - Integrates with gradient_manager for gradient synchronization
 * - Supports biological brain-inspired decentralized learning
 *
 * BIOLOGICAL GROUNDING:
 * - Brain regions learn locally with global coordination
 * - Sparse communication between cortical areas
 * - Federated learning mirrors distributed cortical processing
 *
 * INTEGRATION POINTS:
 * - bio_async: Inter-node messaging
 * - gradient_manager: Gradient synchronization
 * - thalamic_router: Route signals between distributed nodes
 * - brain_training_integration: Coordinate with brain factory
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_DISTRIBUTED_TRAINING_H
#define NIMCP_DISTRIBUTED_TRAINING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"
#include "middleware/training/nimcp_gradient_manager.h"
#include "middleware/training/nimcp_optimizers.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define DIST_MAX_WORKERS              256    /**< Maximum worker nodes */
#define DIST_MAX_GPUS_PER_NODE        16     /**< Maximum GPUs per node */
#define DIST_DEFAULT_BUCKET_SIZE_MB   25     /**< Default gradient bucket size */
#define DIST_DEFAULT_TIMEOUT_MS       300000 /**< Default timeout (5 minutes) */

/* Bio-async module IDs */
#define BIO_MODULE_DISTRIBUTED        0x0800 /**< Distributed training module */
#define BIO_MSG_GRADIENT_SYNC         0x0801 /**< Gradient synchronization */
#define BIO_MSG_PARAM_BROADCAST       0x0802 /**< Parameter broadcast */
#define BIO_MSG_FEDERATED_UPDATE      0x0803 /**< Federated learning update */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Distributed training strategy
 *
 * COMPARISON:
 * - PyTorch: DistributedDataParallel (data parallel), FSDP (hybrid)
 * - JAX: pmap (data parallel), pjit (model parallel)
 * - TensorFlow: MirroredStrategy, TPUStrategy
 */
typedef enum {
    DIST_STRATEGY_DATA_PARALLEL = 0, /**< Replicate model, shard data */
    DIST_STRATEGY_MODEL_PARALLEL,    /**< Shard model across devices */
    DIST_STRATEGY_PIPELINE_PARALLEL, /**< Stage model layers on devices */
    DIST_STRATEGY_TENSOR_PARALLEL,   /**< Shard tensors within layers */
    DIST_STRATEGY_EXPERT_PARALLEL,   /**< Mixture of Experts parallelism */
    DIST_STRATEGY_FSDP,              /**< Fully Sharded Data Parallel */
    DIST_STRATEGY_FEDERATED,         /**< Federated learning (decentralized) */
    DIST_STRATEGY_HYBRID,            /**< Combine multiple strategies */
    DIST_STRATEGY_COUNT
} dist_strategy_t;

/**
 * @brief Gradient synchronization method
 */
typedef enum {
    DIST_SYNC_ALL_REDUCE = 0,        /**< Synchronized all-reduce */
    DIST_SYNC_RING_ALL_REDUCE,       /**< Ring-based all-reduce */
    DIST_SYNC_ASYNC_SGD,             /**< Asynchronous SGD (Hogwild) */
    DIST_SYNC_LOCAL_SGD,             /**< Local SGD with periodic sync */
    DIST_SYNC_GOSSIP,                /**< Gossip-based averaging */
    DIST_SYNC_FEDAVG,                /**< Federated Averaging */
    DIST_SYNC_FEDPROX,               /**< FedProx (regularized federated) */
    DIST_SYNC_COUNT
} dist_sync_method_t;

/**
 * @brief Communication backend
 */
typedef enum {
    DIST_BACKEND_GLOO = 0,           /**< Gloo (CPU, Ethernet) */
    DIST_BACKEND_NCCL,               /**< NCCL (NVIDIA GPU) */
    DIST_BACKEND_MPI,                /**< MPI (HPC clusters) */
    DIST_BACKEND_BIO_ASYNC,          /**< NIMCP bio-async (native) */
    DIST_BACKEND_TCP,                /**< TCP sockets (fallback) */
    DIST_BACKEND_COUNT
} dist_backend_t;

/**
 * @brief Worker role in distributed training
 */
typedef enum {
    DIST_ROLE_WORKER = 0,            /**< Training worker */
    DIST_ROLE_COORDINATOR,           /**< Parameter server / coordinator */
    DIST_ROLE_AGGREGATOR,            /**< Gradient aggregator */
    DIST_ROLE_EVALUATOR,             /**< Evaluation worker */
    DIST_ROLE_COUNT
} dist_role_t;

/**
 * @brief Gradient compression method
 *
 * BIOLOGICAL GROUNDING:
 * - Brain uses sparse communication between regions
 * - Top-K compression mirrors attention-based communication
 */
typedef enum {
    DIST_COMPRESS_NONE = 0,          /**< No compression */
    DIST_COMPRESS_FP16,              /**< FP16 quantization */
    DIST_COMPRESS_INT8,              /**< INT8 quantization */
    DIST_COMPRESS_TOP_K,             /**< Top-K sparsification */
    DIST_COMPRESS_RANDOM_K,          /**< Random-K sparsification */
    DIST_COMPRESS_THRESHOLD,         /**< Threshold-based sparsification */
    DIST_COMPRESS_POWERSGD,          /**< PowerSGD low-rank approximation */
    DIST_COMPRESS_TERNARY,           /**< Ternary quantization */
    DIST_COMPRESS_COUNT
} dist_compression_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Worker node configuration
 */
typedef struct {
    uint32_t rank;                   /**< Global rank (0 = coordinator) */
    uint32_t local_rank;             /**< Rank within node */
    uint32_t world_size;             /**< Total number of workers */
    dist_role_t role;                /**< Worker role */
    char hostname[256];              /**< Node hostname */
    uint16_t port;                   /**< Communication port */
    uint32_t gpus[DIST_MAX_GPUS_PER_NODE]; /**< GPU device IDs */
    uint32_t num_gpus;               /**< Number of GPUs on this node */
} dist_worker_config_t;

/**
 * @brief Data parallel configuration
 */
typedef struct {
    uint32_t gradient_bucket_size_mb; /**< Bucket size for all-reduce */
    bool overlap_communication;       /**< Overlap compute and communication */
    bool gradient_as_bucket_view;     /**< Avoid gradient copy */
    bool find_unused_parameters;      /**< Handle unused model params */
    bool static_graph;                /**< Model graph doesn't change */
} dist_data_parallel_config_t;

/**
 * @brief Model parallel configuration
 */
typedef struct {
    uint32_t* layer_to_device_map;   /**< Which device owns each layer */
    uint32_t num_layers;             /**< Number of layers to map */
    bool auto_balance;               /**< Auto-balance layer distribution */
    float balance_threshold;         /**< Imbalance threshold for rebalancing */
} dist_model_parallel_config_t;

/**
 * @brief Pipeline parallel configuration
 */
typedef struct {
    uint32_t num_stages;             /**< Number of pipeline stages */
    uint32_t num_microbatches;       /**< Microbatches per minibatch */
    bool interleaved_schedule;       /**< Use interleaved 1F1B schedule */
    bool async_pipeline;             /**< Async pipeline (PipeDream) */
} dist_pipeline_config_t;

/**
 * @brief FSDP (Fully Sharded Data Parallel) configuration
 *
 * COMPARISON (PyTorch FSDP):
 * - Shards parameters, gradients, and optimizer state
 * - ZeRO-3 equivalent from DeepSpeed
 */
typedef struct {
    bool shard_parameters;           /**< Shard model parameters */
    bool shard_gradients;            /**< Shard gradients */
    bool shard_optimizer_state;      /**< Shard optimizer state */
    uint32_t sharding_degree;        /**< Number of shards (0 = world_size) */
    bool mixed_precision;            /**< Enable AMP with FSDP */
    bool cpu_offload;                /**< Offload to CPU memory */
    float prefetch_factor;           /**< Prefetch factor for next shard */
} dist_fsdp_config_t;

/**
 * @brief Federated learning configuration
 *
 * BIOLOGICAL GROUNDING:
 * - Models decentralized brain learning
 * - Local synaptic changes consolidated globally
 * - Privacy-preserving (each node keeps data local)
 */
typedef struct {
    dist_sync_method_t sync_method;  /**< Synchronization algorithm */
    uint32_t local_epochs;           /**< Local training epochs per round */
    uint32_t num_rounds;             /**< Total federated rounds */
    float client_fraction;           /**< Fraction of clients per round */

    /* FedProx regularization */
    float proximal_mu;               /**< Proximal term coefficient */

    /* Privacy settings */
    bool differential_privacy;       /**< Enable differential privacy */
    float dp_epsilon;                /**< DP epsilon parameter */
    float dp_delta;                  /**< DP delta parameter */
    float noise_multiplier;          /**< Gaussian noise multiplier */
    float max_grad_norm;             /**< Max gradient norm for clipping */

    /* Secure aggregation */
    bool secure_aggregation;         /**< Enable secure aggregation */
} dist_federated_config_t;

/**
 * @brief Gradient compression configuration
 */
typedef struct {
    dist_compression_t method;       /**< Compression method */
    float compression_ratio;         /**< Target compression ratio */
    uint32_t top_k;                  /**< Top-K elements to keep */
    float threshold;                 /**< Threshold for sparsification */
    uint32_t power_iter;             /**< PowerSGD iterations */
    bool error_feedback;             /**< Use error feedback correction */
} dist_compression_config_t;

/**
 * @brief Complete distributed training configuration
 */
typedef struct {
    /* Strategy and backend */
    dist_strategy_t strategy;        /**< Parallelism strategy */
    dist_backend_t backend;          /**< Communication backend */

    /* Worker configuration */
    dist_worker_config_t worker;     /**< This worker's config */

    /* Strategy-specific configs */
    dist_data_parallel_config_t data_parallel;
    dist_model_parallel_config_t model_parallel;
    dist_pipeline_config_t pipeline;
    dist_fsdp_config_t fsdp;
    dist_federated_config_t federated;

    /* Communication settings */
    dist_compression_config_t compression;
    uint32_t timeout_ms;             /**< Communication timeout */
    uint32_t retry_count;            /**< Retry on failure */
    bool barrier_on_start;           /**< Sync before training */

    /* Integration settings */
    bool integrate_bio_async;        /**< Use bio-async for messaging */
    bool integrate_gradient_manager; /**< Use existing gradient_manager */
    bool integrate_thalamic_router;  /**< Route through thalamic layer */

    /* Debugging */
    bool verbose;                    /**< Print distributed status */
    bool track_communication;        /**< Track communication time */
} dist_config_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Distributed training statistics
 */
typedef struct {
    uint64_t total_steps;            /**< Total training steps */
    uint64_t sync_events;            /**< Gradient sync events */
    uint64_t bytes_sent;             /**< Total bytes sent */
    uint64_t bytes_received;         /**< Total bytes received */

    /* Timing */
    double compute_time_ms;          /**< Total compute time */
    double communication_time_ms;    /**< Total communication time */
    double sync_time_ms;             /**< Synchronization time */
    double idle_time_ms;             /**< Idle/waiting time */
    float communication_ratio;       /**< Comm time / total time */

    /* Efficiency */
    float effective_throughput;      /**< Samples/second across cluster */
    float scaling_efficiency;        /**< actual / ideal speedup */
    float compression_ratio;         /**< Achieved compression ratio */

    /* Health */
    uint64_t timeout_count;          /**< Timeout events */
    uint64_t retry_count;            /**< Retry events */
    uint64_t straggler_events;       /**< Slow worker events */
} dist_stats_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief Distributed training context (opaque)
 */
typedef struct dist_ctx_s dist_ctx_t;

/**
 * @brief Distributed process group (opaque)
 */
typedef struct dist_group_s dist_group_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default distributed configuration
 *
 * @param config Configuration to initialize
 * @param world_size Total number of workers
 * @param rank This worker's rank
 * @return 0 on success, negative on error
 */
int dist_default_config(dist_config_t* config, uint32_t world_size, uint32_t rank);

/**
 * @brief Create distributed training context
 *
 * WHAT: Initialize distributed training infrastructure
 * WHY:  Set up communication, create process groups
 * HOW:  Initialize backend, create buffers, establish connections
 *
 * INTEGRATION:
 * - Registers with bio-async router if enabled
 * - Connects to gradient_manager for gradient sync
 * - Sets up thalamic routing for distributed signals
 *
 * @param config Distributed configuration
 * @return Distributed context or NULL on failure
 */
dist_ctx_t* dist_create(const dist_config_t* config);

/**
 * @brief Destroy distributed context
 *
 * @param ctx Context to destroy (NULL-safe)
 */
void dist_destroy(dist_ctx_t* ctx);

/**
 * @brief Initialize distributed training (call on all workers)
 *
 * WHAT: Collective initialization across all workers
 * WHY:  Synchronize workers before training
 * HOW:  Barrier sync, verify connectivity, share configs
 *
 * @param ctx Distributed context
 * @return 0 on success, negative on error
 */
int dist_init(dist_ctx_t* ctx);

/**
 * @brief Finalize distributed training (call on all workers)
 *
 * WHAT: Clean shutdown of distributed training
 * WHY:  Proper cleanup before exit
 * HOW:  Barrier sync, flush buffers, close connections
 *
 * @param ctx Distributed context
 * @return 0 on success, negative on error
 */
int dist_finalize(dist_ctx_t* ctx);

//=============================================================================
// Process Group API
//=============================================================================

/**
 * @brief Create process group
 *
 * WHAT: Create subset of workers for communication
 * WHY:  Enable hierarchical communication patterns
 * HOW:  Subset of world with shared communicator
 *
 * @param ctx Distributed context
 * @param ranks Array of ranks in group
 * @param num_ranks Number of ranks
 * @return Process group or NULL on failure
 */
dist_group_t* dist_create_group(
    dist_ctx_t* ctx,
    const uint32_t* ranks,
    uint32_t num_ranks
);

/**
 * @brief Get world process group
 *
 * @param ctx Distributed context
 * @return World group (all workers)
 */
dist_group_t* dist_world_group(dist_ctx_t* ctx);

/**
 * @brief Destroy process group
 *
 * @param group Group to destroy
 */
void dist_destroy_group(dist_group_t* group);

//=============================================================================
// Collective Operations API
//=============================================================================

/**
 * @brief All-reduce gradients
 *
 * WHAT: Reduce gradients across all workers
 * WHY:  Synchronize learning across data parallel workers
 * HOW:  Ring all-reduce or tree-reduce algorithm
 *
 * COMPARISON (PyTorch equivalent):
 * ```python
 * torch.distributed.all_reduce(gradients, op=ReduceOp.SUM)
 * gradients /= world_size
 * ```
 *
 * @param ctx Distributed context
 * @param gradients Gradient buffer (modified in place)
 * @param count Number of gradients
 * @param group Process group (NULL = world)
 * @return 0 on success, negative on error
 */
int dist_all_reduce_gradients(
    dist_ctx_t* ctx,
    float* gradients,
    size_t count,
    dist_group_t* group
);

/**
 * @brief All-reduce tensor
 *
 * @param ctx Distributed context
 * @param tensor Tensor to reduce (modified in place)
 * @param group Process group (NULL = world)
 * @return 0 on success, negative on error
 */
int dist_all_reduce_tensor(
    dist_ctx_t* ctx,
    nimcp_tensor_t* tensor,
    dist_group_t* group
);

/**
 * @brief Broadcast parameters from source rank
 *
 * WHAT: Send parameters from one worker to all others
 * WHY:  Initialize workers with same parameters
 * HOW:  Tree-based broadcast from source
 *
 * @param ctx Distributed context
 * @param params Parameter buffer
 * @param count Number of parameters
 * @param source_rank Rank to broadcast from
 * @param group Process group (NULL = world)
 * @return 0 on success, negative on error
 */
int dist_broadcast(
    dist_ctx_t* ctx,
    float* params,
    size_t count,
    uint32_t source_rank,
    dist_group_t* group
);

/**
 * @brief Barrier synchronization
 *
 * WHAT: Block until all workers reach barrier
 * WHY:  Synchronization point across cluster
 *
 * @param ctx Distributed context
 * @param group Process group (NULL = world)
 * @return 0 on success, negative on error
 */
int dist_barrier(dist_ctx_t* ctx, dist_group_t* group);

/**
 * @brief All-gather tensors from all workers
 *
 * @param ctx Distributed context
 * @param send_tensor Tensor to send
 * @param recv_tensors Array of received tensors
 * @param group Process group (NULL = world)
 * @return 0 on success, negative on error
 */
int dist_all_gather(
    dist_ctx_t* ctx,
    nimcp_tensor_t* send_tensor,
    nimcp_tensor_t** recv_tensors,
    dist_group_t* group
);

/**
 * @brief Reduce-scatter operation
 *
 * WHAT: Reduce and scatter result to workers
 * WHY:  Efficient for FSDP gradient sharding
 *
 * @param ctx Distributed context
 * @param tensor Input tensor (modified to receive shard)
 * @param group Process group (NULL = world)
 * @return 0 on success, negative on error
 */
int dist_reduce_scatter(
    dist_ctx_t* ctx,
    nimcp_tensor_t* tensor,
    dist_group_t* group
);

//=============================================================================
// Federated Learning API
//=============================================================================

/**
 * @brief Start federated learning round
 *
 * WHAT: Begin new federated learning round
 * WHY:  Coordinate federated training cycles
 * HOW:  Broadcast global model, select participating clients
 *
 * @param ctx Distributed context
 * @param round_num Round number
 * @return 0 on success, negative on error
 */
int dist_federated_start_round(dist_ctx_t* ctx, uint32_t round_num);

/**
 * @brief Submit local model update
 *
 * WHAT: Send local model update to coordinator
 * WHY:  Contribute local learning to global model
 * HOW:  Compute model delta, compress, send to aggregator
 *
 * @param ctx Distributed context
 * @param local_weights Local model weights
 * @param count Number of weights
 * @param num_samples Number of local training samples
 * @return 0 on success, negative on error
 */
int dist_federated_submit_update(
    dist_ctx_t* ctx,
    const float* local_weights,
    size_t count,
    uint32_t num_samples
);

/**
 * @brief Aggregate federated updates (coordinator only)
 *
 * WHAT: Combine updates from participating clients
 * WHY:  Create new global model
 * HOW:  Weighted average based on sample counts
 *
 * @param ctx Distributed context
 * @param global_weights Output global weights
 * @param count Number of weights
 * @return 0 on success, negative on error
 */
int dist_federated_aggregate(
    dist_ctx_t* ctx,
    float* global_weights,
    size_t count
);

/**
 * @brief Receive updated global model
 *
 * WHAT: Get aggregated global model from coordinator
 * WHY:  Update local model for next round
 *
 * @param ctx Distributed context
 * @param global_weights Output global weights
 * @param count Number of weights
 * @return 0 on success, negative on error
 */
int dist_federated_receive_global(
    dist_ctx_t* ctx,
    float* global_weights,
    size_t count
);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async for distributed communication
 * WHY:  Leverage NIMCP's native messaging system
 * HOW:  Register handlers, configure routing
 *
 * INTEGRATION:
 * - Uses bio_router for inter-node messaging
 * - Integrates with thalamic routing for signal distribution
 *
 * @param ctx Distributed context
 * @param router Bio-async router
 * @return 0 on success, negative on error
 */
int dist_connect_bio_async(dist_ctx_t* ctx, bio_router_t* router);

/**
 * @brief Connect to gradient manager
 *
 * WHAT: Integrate with existing gradient management
 * WHY:  Coordinate gradient scaling with distributed sync
 *
 * @param ctx Distributed context
 * @param grad_manager Gradient manager
 * @return 0 on success, negative on error
 */
int dist_connect_gradient_manager(
    dist_ctx_t* ctx,
    nimcp_gradient_manager_ctx_t* grad_manager
);

/**
 * @brief Connect to brain training integration
 *
 * WHAT: Integrate with brain factory training
 * WHY:  Distribute brain training across nodes
 *
 * @param ctx Distributed context
 * @param brain_training Brain training integration context
 * @return 0 on success, negative on error
 */
int dist_connect_brain_training(dist_ctx_t* ctx, void* brain_training);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get distributed training statistics
 *
 * @param ctx Distributed context
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int dist_get_stats(const dist_ctx_t* ctx, dist_stats_t* stats);

/**
 * @brief Reset distributed statistics
 *
 * @param ctx Distributed context
 */
void dist_reset_stats(dist_ctx_t* ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get strategy name
 */
const char* dist_strategy_name(dist_strategy_t strategy);

/**
 * @brief Get sync method name
 */
const char* dist_sync_method_name(dist_sync_method_t method);

/**
 * @brief Get backend name
 */
const char* dist_backend_name(dist_backend_t backend);

/**
 * @brief Get this worker's rank
 */
uint32_t dist_get_rank(const dist_ctx_t* ctx);

/**
 * @brief Get world size
 */
uint32_t dist_get_world_size(const dist_ctx_t* ctx);

/**
 * @brief Check if this worker is coordinator
 */
bool dist_is_coordinator(const dist_ctx_t* ctx);

/**
 * @brief Validate distributed configuration
 */
int dist_validate_config(const dist_config_t* config);

/**
 * @brief Compute optimal bucket size for gradient synchronization
 *
 * WHAT: Dynamically determine the best bucket size for all-reduce
 * WHY:  Fixed 25MB bucket size doesn't adapt to model size and network
 * HOW:  Consider gradient count, network bandwidth, and latency
 *
 * Factors considered:
 * - Total gradient size: larger models need larger buckets
 * - Network bandwidth: higher bandwidth allows larger buckets
 * - Network latency: higher latency favors fewer, larger buckets
 * - Worker count: more workers may need adjusted bucket sizes
 *
 * @param ctx Distributed context
 * @param total_params Total number of model parameters
 * @param network_bandwidth_gbps Estimated network bandwidth in Gbps (0 for auto)
 * @return Optimal bucket size in bytes
 */
size_t dist_compute_optimal_bucket_size(
    dist_ctx_t* ctx,
    size_t total_params,
    float network_bandwidth_gbps
);

/**
 * @brief Set dynamic bucket size based on model
 *
 * @param ctx Distributed context
 * @param total_params Total number of model parameters
 * @return 0 on success, negative on error
 */
int dist_set_dynamic_bucket_size(
    dist_ctx_t* ctx,
    size_t total_params
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DISTRIBUTED_TRAINING_H */
