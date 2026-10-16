//=============================================================================
// nimcp_auto_architecture.h - Neural Architecture Search for NIMCP
//=============================================================================
/**
 * @file nimcp_auto_architecture.h
 * @brief Automatic architecture discovery for SNN/LNN/CNN hybrid networks
 *
 * WHAT: Neural Architecture Search (NAS) system for optimal topology discovery
 * WHY:  Manual architecture design is time-consuming and suboptimal; NAS finds
 *       better architectures faster while maintaining biological plausibility
 * HOW:  Multiple search strategies (RL, evolutionary, DARTS, pruning) explore
 *       topology space under biological and computational constraints
 *
 * BIOLOGICAL GROUNDING:
 * - Brain architecture emerges through evolutionary optimization
 * - Synaptic pruning shapes connectivity during development
 * - Energy efficiency constrains biological neural circuits
 * - Hierarchical organization optimizes information processing
 *
 * SEARCH STRATEGIES:
 * 1. Reinforcement Learning NAS: RNN controller generates architectures
 * 2. Evolutionary NAS: Genetic algorithms evolve topologies
 * 3. DARTS: Differentiable Architecture Search with continuous relaxation
 * 4. Pruning-based: Start dense, prune to discover sparse architectures
 *
 * OPTIMIZATION OBJECTIVES:
 * - Task performance (accuracy, loss)
 * - Energy efficiency (spike count, operations)
 * - Biological plausibility (connectivity patterns, time constants)
 * - Computational cost (latency, memory, FLOPs)
 * - Temporal precision (for time-series tasks)
 *
 * CODING STANDARDS:
 * - Guard clauses (no nested ifs)
 * - WHAT-WHY-HOW documentation
 * - Single Responsibility Principle
 * - Biological grounding for all design choices
 *
 * USAGE EXAMPLE:
 * ```c
 * // Configure search
 * auto_arch_config_t config;
 * auto_arch_default_config(&config);
 * config.search_method = AUTO_ARCH_EVOLUTIONARY;
 * config.max_evaluations = 1000;
 * config.population_size = 50;
 *
 * // Create search context
 * auto_arch_context_t* ctx = auto_arch_create(&config);
 *
 * // Define task and constraints
 * auto_arch_task_t task = {
 *     .type = AUTO_ARCH_TASK_CLASSIFICATION,
 *     .n_inputs = 784,
 *     .n_outputs = 10,
 *     .max_latency_ms = 50.0f,
 *     .target_accuracy = 0.95f
 * };
 * auto_arch_set_task(ctx, &task);
 *
 * // Run search
 * auto_arch_result_t* result = auto_arch_search(ctx, train_data, val_data);
 *
 * // Export best architecture
 * snn_config_t* snn_cfg = auto_arch_export_snn(result->best_arch);
 * lnn_config_t* lnn_cfg = auto_arch_export_lnn(result->best_arch);
 *
 * // Cleanup
 * auto_arch_result_destroy(result);
 * auto_arch_destroy(ctx);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 * @version 1.0.0
 */

#ifndef NIMCP_AUTO_ARCHITECTURE_H
#define NIMCP_AUTO_ARCHITECTURE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/tensor/nimcp_tensor.h"
#include "snn/nimcp_snn_types.h"
#include "lnn/nimcp_lnn_types.h"
#include "middleware/training/nimcp_optimizers.h"
#include "middleware/training/nimcp_gradient_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of layers in searched architecture */
#define AUTO_ARCH_MAX_LAYERS 32

/** Maximum population size for evolutionary search */
#define AUTO_ARCH_MAX_POPULATION 200

/** Maximum number of search iterations */
#define AUTO_ARCH_MAX_ITERATIONS 100000

/** Default validation split ratio */
#define AUTO_ARCH_VALIDATION_SPLIT 0.2f

/** Magic number for validation */
#define AUTO_ARCH_MAGIC 0x41415300  /* "AAS\0" */

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct auto_arch_context_s auto_arch_context_t;
typedef struct auto_arch_config_s auto_arch_config_t;
typedef struct auto_arch_architecture_s auto_arch_architecture_t;
typedef struct auto_arch_result_s auto_arch_result_t;
typedef struct auto_arch_layer_spec_s auto_arch_layer_spec_t;
typedef struct auto_arch_task_s auto_arch_task_t;
typedef struct auto_arch_constraints_s auto_arch_constraints_t;
typedef struct auto_arch_fitness_s auto_arch_fitness_t;
typedef struct auto_arch_search_space_s auto_arch_search_space_t;
typedef struct auto_arch_stats_s auto_arch_stats_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Architecture search methods
 *
 * WHAT: Strategies for exploring architecture space
 * WHY:  Different methods have different strengths (sample efficiency, etc.)
 * HOW:  Each method uses different optimization principles
 *
 * BIOLOGICAL ANALOG:
 * - RL: Trial-and-error learning (dopamine-based reinforcement)
 * - Evolutionary: Natural selection and mutation
 * - DARTS: Gradient-based optimization (like synaptic plasticity)
 * - Pruning: Synaptic pruning during brain development
 */
typedef enum {
    AUTO_ARCH_RL_NAS = 0,           /**< Reinforcement learning NAS (ENAS, NAS-RL) */
    AUTO_ARCH_EVOLUTIONARY,         /**< Evolutionary algorithms (genetic search) */
    AUTO_ARCH_DARTS,                /**< Differentiable Architecture Search */
    AUTO_ARCH_RANDOM_SEARCH,        /**< Random sampling (baseline) */
    AUTO_ARCH_BAYESIAN_OPT,         /**< Bayesian optimization (sample efficient) */
    AUTO_ARCH_PRUNING_BASED,        /**< Start dense, prune to discover structure */
    AUTO_ARCH_GRADIENT_BASED,       /**< Gradient-based optimization with masks */
    AUTO_ARCH_NEUROEVOLUTION,       /**< NEAT-style topology evolution */
    AUTO_ARCH_METHOD_COUNT
} auto_arch_method_t;

/**
 * @brief Network types to search
 *
 * WHAT: Type of neural network architecture to optimize
 * WHY:  Different network types have different search spaces
 * HOW:  Constraints and operations differ by network type
 */
typedef enum {
    AUTO_ARCH_TYPE_SNN = 0,         /**< Spiking Neural Network */
    AUTO_ARCH_TYPE_LNN,             /**< Liquid Neural Network */
    AUTO_ARCH_TYPE_CNN,             /**< Convolutional Neural Network */
    AUTO_ARCH_TYPE_HYBRID_SNN_LNN,  /**< SNN + LNN hybrid */
    AUTO_ARCH_TYPE_HYBRID_SNN_CNN,  /**< SNN + CNN hybrid */
    AUTO_ARCH_TYPE_HYBRID_ALL,      /**< SNN + LNN + CNN hybrid */
    AUTO_ARCH_TYPE_COUNT
} auto_arch_network_type_t;

/**
 * @brief Task types for architecture search
 *
 * WHAT: Type of task the architecture will be optimized for
 * WHY:  Different tasks benefit from different architectural properties
 * HOW:  Task type guides fitness function and search constraints
 */
typedef enum {
    AUTO_ARCH_TASK_CLASSIFICATION = 0, /**< Classification (e.g., MNIST) */
    AUTO_ARCH_TASK_REGRESSION,         /**< Regression (continuous output) */
    AUTO_ARCH_TASK_SEQUENCE,           /**< Sequence modeling (time series) */
    AUTO_ARCH_TASK_DETECTION,          /**< Object detection */
    AUTO_ARCH_TASK_SEGMENTATION,       /**< Semantic segmentation */
    AUTO_ARCH_TASK_REINFORCEMENT,      /**< Reinforcement learning control */
    AUTO_ARCH_TASK_GENERATION,         /**< Generative modeling */
    AUTO_ARCH_TASK_CUSTOM,             /**< User-defined fitness function */
    AUTO_ARCH_TASK_COUNT
} auto_arch_task_type_t;

/**
 * @brief Layer types in search space
 *
 * WHAT: Building blocks for architecture construction
 * WHY:  Different layer types provide different computational primitives
 * HOW:  Layers are combined to form complete architectures
 */
typedef enum {
    AUTO_ARCH_LAYER_SNN_LIF = 0,    /**< LIF spiking neuron layer */
    AUTO_ARCH_LAYER_SNN_IZHIKEVICH, /**< Izhikevich spiking neuron layer */
    AUTO_ARCH_LAYER_SNN_ADAPTIVE,   /**< Adaptive exponential integrate-and-fire */
    AUTO_ARCH_LAYER_LNN_LTC,        /**< Liquid time-constant layer */
    AUTO_ARCH_LAYER_DENSE,          /**< Fully connected layer */
    AUTO_ARCH_LAYER_CONV,           /**< Convolutional layer */
    AUTO_ARCH_LAYER_POOL,           /**< Pooling layer */
    AUTO_ARCH_LAYER_RECURRENT,      /**< Recurrent layer */
    AUTO_ARCH_LAYER_ATTENTION,      /**< Attention mechanism */
    AUTO_ARCH_LAYER_SKIP,           /**< Skip connection (ResNet-style) */
    AUTO_ARCH_LAYER_COUNT
} auto_arch_layer_type_t;

/**
 * @brief Connectivity patterns in search space
 *
 * WHAT: Wiring patterns for layer connections
 * WHY:  Connectivity affects computation, efficiency, and bio-plausibility
 * HOW:  Applied when connecting layers or neurons within layers
 *
 * BIOLOGICAL BASIS:
 * - Dense: Simple but not biologically realistic
 * - Sparse: Cortical connectivity (~10-20% connected)
 * - Small-world: Balance of local and long-range connections
 * - Scale-free: Hub structure found in cortex
 */
typedef enum {
    AUTO_ARCH_CONN_DENSE = 0,       /**< All-to-all connectivity */
    AUTO_ARCH_CONN_SPARSE_RANDOM,   /**< Random sparse (Erdos-Renyi) */
    AUTO_ARCH_CONN_SMALL_WORLD,     /**< Watts-Strogatz small-world */
    AUTO_ARCH_CONN_SCALE_FREE,      /**< Barabasi-Albert scale-free */
    AUTO_ARCH_CONN_MODULAR,         /**< Clustered modules */
    AUTO_ARCH_CONN_NCP,             /**< Neural Circuit Policy (LNN) */
    AUTO_ARCH_CONN_COLUMN,          /**< Cortical column structure */
    AUTO_ARCH_CONN_COUNT
} auto_arch_connectivity_t;

/**
 * @brief Fitness objectives for multi-objective optimization
 *
 * WHAT: Objectives to optimize during architecture search
 * WHY:  Real-world deployment requires balancing multiple goals
 * HOW:  Pareto frontier or weighted sum of objectives
 */
typedef enum {
    AUTO_ARCH_OBJ_ACCURACY = 0,     /**< Task performance (maximize) */
    AUTO_ARCH_OBJ_ENERGY,           /**< Energy efficiency (minimize spikes/ops) */
    AUTO_ARCH_OBJ_LATENCY,          /**< Inference latency (minimize) */
    AUTO_ARCH_OBJ_MEMORY,           /**< Memory usage (minimize) */
    AUTO_ARCH_OBJ_PARAMS,           /**< Parameter count (minimize) */
    AUTO_ARCH_OBJ_BIO_PLAUSIBILITY, /**< Biological similarity (maximize) */
    AUTO_ARCH_OBJ_TEMPORAL_PRECISION, /**< Timing precision (maximize for SNNs) */
    AUTO_ARCH_OBJ_ROBUSTNESS,       /**< Noise robustness (maximize) */
    AUTO_ARCH_OBJ_COUNT
} auto_arch_objective_t;

/**
 * @brief Search progress status
 *
 * WHAT: Current state of architecture search
 * WHY:  Track progress and detect completion or errors
 */
typedef enum {
    AUTO_ARCH_STATUS_IDLE = 0,      /**< Not started */
    AUTO_ARCH_STATUS_INITIALIZING,  /**< Setting up search */
    AUTO_ARCH_STATUS_SEARCHING,     /**< Actively searching */
    AUTO_ARCH_STATUS_EVALUATING,    /**< Evaluating candidate */
    AUTO_ARCH_STATUS_COMPLETED,     /**< Search completed successfully */
    AUTO_ARCH_STATUS_CONVERGED,     /**< Converged early */
    AUTO_ARCH_STATUS_MAX_ITERS,     /**< Reached max iterations */
    AUTO_ARCH_STATUS_TIMEOUT,       /**< Exceeded time budget */
    AUTO_ARCH_STATUS_ERROR          /**< Error occurred */
} auto_arch_status_t;

//=============================================================================
// Core Structures
//=============================================================================

/**
 * @brief Specification for a single layer in the architecture
 *
 * WHAT: Complete parameterization of one network layer
 * WHY:  Layers are the atomic units of architecture construction
 * HOW:  Specifies type, size, connectivity, and neuron parameters
 */
struct auto_arch_layer_spec_s {
    /* Layer identity */
    uint32_t layer_id;              /**< Layer index in network */
    auto_arch_layer_type_t type;    /**< Layer type (SNN, LNN, dense, etc.) */
    char name[64];                  /**< Human-readable name */

    /* Size parameters */
    uint32_t n_neurons;             /**< Number of neurons/units in layer */
    uint32_t n_inputs;              /**< Number of inputs to this layer */

    /* Connectivity */
    auto_arch_connectivity_t connectivity; /**< Wiring pattern */
    float sparsity;                 /**< Connection density [0, 1] */
    uint32_t* input_layers;         /**< Indices of layers providing input */
    uint32_t n_input_layers;        /**< Number of input layers */

    /* Neuron-specific parameters (SNN) */
    neuron_type_t neuron_type;      /**< Neuron model (LIF, Izhikevich, etc.) */
    float tau_mem;                  /**< Membrane time constant (ms) */
    float tau_syn;                  /**< Synaptic time constant (ms) */
    float v_thresh;                 /**< Spike threshold (mV) */
    float v_reset;                  /**< Reset potential (mV) */
    float refractory_period;        /**< Refractory period (ms) */

    /* LNN-specific parameters */
    lnn_activation_t activation;    /**< Activation function */
    float tau_base;                 /**< Base time constant for LTC neurons (ms) */
    float tau_min;                  /**< Minimum time constant (ms) */
    float tau_max;                  /**< Maximum time constant (ms) */
    bool learn_tau;                 /**< Whether to learn time constants */
    lnn_ode_method_t ode_method;    /**< ODE solver for LNN */

    /* Convolutional parameters */
    uint32_t kernel_size;           /**< Convolution kernel size */
    uint32_t stride;                /**< Convolution stride */
    uint32_t padding;               /**< Padding size */
    uint32_t n_channels;            /**< Number of channels/feature maps */

    /* Regularization */
    float dropout_rate;             /**< Dropout probability */
    float weight_decay;             /**< L2 regularization coefficient */

    /* Skip connections */
    bool has_skip;                  /**< Whether this layer has skip connection */
    uint32_t skip_source_layer;     /**< Source layer for skip connection */
};

/**
 * @brief Complete architecture specification
 *
 * WHAT: Full network architecture with all layers and connections
 * WHY:  Represents a candidate architecture in the search space
 * HOW:  Array of layer specs plus global parameters
 */
struct auto_arch_architecture_s {
    /* Identity */
    uint64_t arch_id;               /**< Unique architecture ID */
    uint32_t generation;            /**< Generation number (for evolutionary) */
    uint32_t parent_id;             /**< Parent architecture ID */

    /* Network structure */
    auto_arch_network_type_t network_type; /**< SNN, LNN, CNN, or hybrid */
    auto_arch_layer_spec_t* layers; /**< Array of layer specifications */
    uint32_t n_layers;              /**< Number of layers */
    uint32_t n_inputs;              /**< Input dimension */
    uint32_t n_outputs;             /**< Output dimension */

    /* Global parameters */
    float dt;                       /**< Simulation timestep (ms) for SNN/LNN */
    float learning_rate;            /**< Base learning rate */
    nimcp_optimizer_type_t optimizer; /**< Optimizer type */

    /* SNN-specific */
    snn_encoding_t input_encoding;  /**< Input encoding method */
    snn_decoding_t output_decoding; /**< Output decoding method */
    float encoding_time;            /**< Time window for encoding (ms) */

    /* Computed properties */
    uint64_t n_parameters;          /**< Total parameter count */
    uint64_t n_connections;         /**< Total synaptic connections */
    float avg_sparsity;             /**< Average connection sparsity */

    /* Validation */
    uint32_t magic;                 /**< Magic number for validation */
};

/**
 * @brief Task specification for architecture search
 *
 * WHAT: Complete specification of the task to optimize for
 * WHY:  Guides search process and fitness evaluation
 * HOW:  Defines inputs, outputs, metrics, and constraints
 */
struct auto_arch_task_s {
    /* Task type */
    auto_arch_task_type_t type;     /**< Classification, regression, etc. */

    /* Input/output dimensions */
    uint32_t n_inputs;              /**< Input dimension */
    uint32_t n_outputs;             /**< Output dimension */
    uint32_t sequence_length;       /**< Sequence length (for time series) */

    /* Performance targets */
    float target_accuracy;          /**< Target accuracy/performance [0, 1] */
    float max_latency_ms;           /**< Maximum acceptable latency (ms) */
    size_t max_memory_bytes;        /**< Maximum memory budget (bytes) */
    uint64_t max_operations;        /**< Maximum operations per inference */

    /* Training parameters */
    uint32_t n_epochs;              /**< Training epochs per evaluation */
    uint32_t batch_size;            /**< Mini-batch size */
    float validation_split;         /**< Validation set ratio [0, 1] */

    /* Custom fitness function (for AUTO_ARCH_TASK_CUSTOM) */
    float (*custom_fitness)(const auto_arch_architecture_t* arch,
                           const nimcp_tensor_t* predictions,
                           const nimcp_tensor_t* targets,
                           void* user_data);
    void* fitness_user_data;        /**< User data for custom fitness */
};

/**
 * @brief Search space constraints
 *
 * WHAT: Hard constraints on architecture search space
 * WHY:  Limit search to feasible and biologically plausible architectures
 * HOW:  Reject or repair architectures violating constraints
 *
 * BIOLOGICAL GROUNDING:
 * - Brain has physical constraints (energy, space, wiring)
 * - Connectivity patterns follow statistical regularities
 * - Time constants have biophysical bounds
 */
struct auto_arch_constraints_s {
    /* Size constraints */
    uint32_t min_layers;            /**< Minimum number of layers */
    uint32_t max_layers;            /**< Maximum number of layers */
    uint32_t min_neurons_per_layer; /**< Minimum neurons per layer */
    uint32_t max_neurons_per_layer; /**< Maximum neurons per layer */
    uint64_t max_total_neurons;     /**< Maximum total neuron count */
    uint64_t max_parameters;        /**< Maximum parameter count */

    /* Connectivity constraints */
    float min_sparsity;             /**< Minimum connection sparsity [0, 1] */
    float max_sparsity;             /**< Maximum connection sparsity [0, 1] */
    bool enforce_feedforward;       /**< Prohibit recurrent connections */
    bool allow_skip_connections;    /**< Allow ResNet-style skip connections */

    /* Temporal constraints (SNN/LNN) */
    float min_tau;                  /**< Minimum time constant (ms) */
    float max_tau;                  /**< Maximum time constant (ms) */
    float min_dt;                   /**< Minimum simulation timestep (ms) */
    float max_dt;                   /**< Maximum simulation timestep (ms) */

    /* Biological plausibility */
    float min_bio_score;            /**< Minimum biological plausibility [0, 1] */
    bool enforce_dales_law;         /**< Enforce Dale's principle (E/I segregation) */
    bool require_local_connectivity; /**< Prefer local over long-range connections */

    /* Computational constraints */
    uint64_t max_flops;             /**< Maximum FLOPs per inference */
    size_t max_memory;              /**< Maximum memory usage (bytes) */
    float max_latency_ms;           /**< Maximum inference latency (ms) */
    float min_energy_efficiency;    /**< Minimum ops per spike/activation */
};

/**
 * @brief Fitness metrics for a candidate architecture
 *
 * WHAT: Multi-objective fitness evaluation results
 * WHY:  Capture trade-offs between accuracy, efficiency, and bio-plausibility
 * HOW:  Compute each objective and combine for total fitness
 */
struct auto_arch_fitness_s {
    /* Primary objective */
    float accuracy;                 /**< Task accuracy/performance [0, 1] */
    float loss;                     /**< Task loss (lower is better) */

    /* Efficiency objectives */
    uint64_t n_operations;          /**< Operations per inference */
    uint64_t n_spikes;              /**< Spikes per inference (SNN) */
    float energy_per_inference;     /**< Energy consumption estimate (nJ) */
    float latency_ms;               /**< Inference latency (ms) */
    size_t memory_bytes;            /**< Memory usage (bytes) */

    /* Architecture properties */
    uint64_t n_parameters;          /**< Total parameter count */
    float sparsity;                 /**< Average connection sparsity */
    float bio_plausibility_score;   /**< Biological plausibility [0, 1] */

    /* Temporal properties (SNN) */
    float temporal_precision;       /**< Timing precision (ms) */
    float avg_firing_rate;          /**< Average firing rate (Hz) */

    /* Robustness */
    float noise_robustness;         /**< Performance under noise [0, 1] */

    /* Combined fitness */
    float total_fitness;            /**< Weighted combination of objectives */
    float pareto_rank;              /**< Pareto rank for multi-objective */

    /* Training statistics */
    float train_time_sec;           /**< Time to train (seconds) */
    uint32_t epochs_converged;      /**< Epochs to convergence */
    bool converged;                 /**< Whether training converged */
};

/**
 * @brief Search space specification
 *
 * WHAT: Defines the space of possible architectures to search
 * WHY:  Constrains search to relevant architectures
 * HOW:  Specifies allowed layer types, connectivity, ranges, etc.
 */
struct auto_arch_search_space_s {
    /* Layer types */
    bool allow_snn_lif;             /**< Allow LIF spiking neurons */
    bool allow_snn_izhikevich;      /**< Allow Izhikevich neurons */
    bool allow_lnn_ltc;             /**< Allow LTC neurons */
    bool allow_dense;               /**< Allow dense layers */
    bool allow_conv;                /**< Allow convolutional layers */
    bool allow_recurrent;           /**< Allow recurrent layers */

    /* Connectivity */
    bool allow_sparse;              /**< Allow sparse connectivity */
    bool allow_small_world;         /**< Allow small-world topology */
    bool allow_scale_free;          /**< Allow scale-free topology */
    bool allow_skip_connections;    /**< Allow skip connections */

    /* Parameter ranges */
    uint32_t min_layer_size;        /**< Minimum layer size */
    uint32_t max_layer_size;        /**< Maximum layer size */
    float min_sparsity;             /**< Minimum sparsity */
    float max_sparsity;             /**< Maximum sparsity */

    /* Neuron parameter ranges (SNN) */
    float tau_mem_min;              /**< Min membrane time constant (ms) */
    float tau_mem_max;              /**< Max membrane time constant (ms) */
    float tau_syn_min;              /**< Min synaptic time constant (ms) */
    float tau_syn_max;              /**< Max synaptic time constant (ms) */

    /* LNN parameter ranges */
    float tau_base_min;             /**< Min base time constant (ms) */
    float tau_base_max;             /**< Max base time constant (ms) */
};

/**
 * @brief Architecture search configuration
 *
 * WHAT: Complete configuration for architecture search
 * WHY:  Control all aspects of the search process
 * HOW:  Nested config with search method, task, constraints, etc.
 */
struct auto_arch_config_s {
    /* Search method */
    auto_arch_method_t search_method; /**< NAS algorithm to use */
    auto_arch_network_type_t network_type; /**< Network type to search */

    /* Search parameters */
    uint32_t max_evaluations;       /**< Maximum architecture evaluations */
    uint32_t max_iterations;        /**< Maximum search iterations */
    float max_time_hours;           /**< Maximum wall-clock time (hours) */
    uint32_t early_stop_patience;   /**< Stop if no improvement for N evals */

    /* Population parameters (evolutionary/RL) */
    uint32_t population_size;       /**< Population size for evolutionary */
    float mutation_rate;            /**< Mutation probability [0, 1] */
    float crossover_rate;           /**< Crossover probability [0, 1] */
    uint32_t tournament_size;       /**< Tournament selection size */

    /* RL NAS parameters */
    float rl_learning_rate;         /**< Controller learning rate */
    float rl_entropy_weight;        /**< Entropy regularization weight */
    uint32_t rl_controller_lstm_size; /**< LSTM hidden size for controller */

    /* DARTS parameters */
    float darts_alpha_lr;           /**< Architecture parameter learning rate */
    float darts_weight_lr;          /**< Weight parameter learning rate */
    uint32_t darts_warmup_epochs;   /**< Warmup epochs before arch search */

    /* Bayesian optimization parameters */
    uint32_t bo_n_initial_points;   /**< Initial random samples */
    float bo_acquisition_weight;    /**< Exploration vs exploitation */

    /* Multi-objective optimization */
    auto_arch_objective_t primary_objective; /**< Primary objective */
    float objective_weights[AUTO_ARCH_OBJ_COUNT]; /**< Weights for each objective */
    bool use_pareto_frontier;       /**< Use Pareto multi-objective opt */

    /* Search space and constraints */
    auto_arch_search_space_t search_space; /**< Allowed architectures */
    auto_arch_constraints_t constraints;   /**< Hard constraints */

    /* Training configuration for evaluation */
    uint32_t eval_epochs;           /**< Epochs for fitness evaluation */
    uint32_t eval_batch_size;       /**< Batch size for evaluation */
    float eval_learning_rate;       /**< Learning rate for evaluation */
    nimcp_optimizer_type_t eval_optimizer; /**< Optimizer for evaluation */

    /* Parallelization */
    uint32_t n_workers;             /**< Number of parallel workers */
    bool use_gpu;                   /**< Use GPU acceleration if available */

    /* Logging and checkpointing */
    bool verbose;                   /**< Print progress */
    uint32_t checkpoint_interval;   /**< Save checkpoint every N evals */
    const char* checkpoint_dir;     /**< Directory for checkpoints */
    const char* log_file;           /**< Log file path */

    /* Reproducibility */
    uint64_t random_seed;           /**< Random seed (0 = use time) */
};

/**
 * @brief Architecture search statistics
 *
 * WHAT: Statistics about the search process
 * WHY:  Monitor progress and diagnose search issues
 * HOW:  Track evaluations, improvements, convergence, etc.
 *
 * NOTE: This struct is defined before auto_arch_result_s because
 *       auto_arch_result_s contains an embedded auto_arch_stats_t member.
 */
struct auto_arch_stats_s {
    /* Progress */
    uint32_t total_evaluations;     /**< Total architectures evaluated */
    uint32_t iterations;            /**< Total search iterations */
    float elapsed_time_sec;         /**< Wall-clock time (seconds) */

    /* Best so far */
    float best_fitness_so_far;      /**< Best fitness seen */
    uint32_t best_at_iteration;     /**< Iteration when best found */

    /* Improvement tracking */
    uint32_t improvements;          /**< Number of improvements */
    uint32_t stagnant_iterations;   /**< Iterations without improvement */

    /* Fitness statistics */
    float avg_fitness;              /**< Average fitness of population */
    float min_fitness;              /**< Minimum fitness in population */
    float max_fitness;              /**< Maximum fitness in population */
    float fitness_std;              /**< Fitness standard deviation */

    /* Architecture statistics */
    float avg_layers;               /**< Average number of layers */
    float avg_neurons;              /**< Average neurons per layer */
    float avg_sparsity;             /**< Average connection sparsity */
    float avg_parameters;           /**< Average parameter count */

    /* Computational cost */
    uint64_t total_training_steps;  /**< Total gradient steps */
    float total_training_time_sec;  /**< Total training time */
    float avg_eval_time_sec;        /**< Average evaluation time */
};

/**
 * @brief Search result containing best architecture(s)
 *
 * WHAT: Complete results from architecture search
 * WHY:  Provide best architecture(s) and search statistics
 * HOW:  Stores best arch, Pareto frontier, and search history
 */
struct auto_arch_result_s {
    /* Best architecture */
    auto_arch_architecture_t* best_arch; /**< Highest fitness architecture */
    auto_arch_fitness_t best_fitness;    /**< Fitness of best architecture */

    /* Pareto frontier (multi-objective) */
    auto_arch_architecture_t** pareto_frontier; /**< Non-dominated archs */
    auto_arch_fitness_t* pareto_fitness; /**< Fitness for each Pareto arch */
    uint32_t n_pareto;                   /**< Number of Pareto optimal archs */

    /* Search history */
    auto_arch_architecture_t** history; /**< All evaluated architectures */
    auto_arch_fitness_t* history_fitness; /**< Fitness for each arch */
    uint32_t n_evaluated;               /**< Number of evaluated archs */

    /* Search statistics */
    auto_arch_stats_t stats;            /**< Search statistics */
    auto_arch_status_t status;          /**< Final search status */
};

/**
 * @brief Opaque context for architecture search
 *
 * WHAT: Internal state for architecture search
 * WHY:  Encapsulate search algorithm state
 * HOW:  Opaque pointer; internals defined in .c file
 */
struct auto_arch_context_s; /* Opaque - defined in implementation */

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Initialize default architecture search configuration
 *
 * WHAT: Populate config with sensible defaults
 * WHY:  Provide working baseline configuration
 * HOW:  Sets evolutionary search, 1000 evals, balanced objectives
 *
 * DEFAULT VALUES:
 * - Method: Evolutionary with population=50, 1000 evaluations
 * - Network type: SNN (LIF neurons)
 * - Primary objective: Accuracy (weight=0.7)
 * - Secondary: Energy efficiency (weight=0.3)
 * - Constraints: 2-8 layers, 100-1000 neurons/layer
 *
 * @param config Configuration struct to initialize
 * @return 0 on success, negative on error
 */
int auto_arch_default_config(auto_arch_config_t* config);

/**
 * @brief Initialize configuration for fast search (low budget)
 *
 * WHAT: Quick search with limited evaluations
 * WHY:  Rapid prototyping or resource-constrained scenarios
 * HOW:  Random search, 100 evaluations, minimal training
 *
 * @param config Configuration struct to populate
 * @return 0 on success, negative on error
 */
int auto_arch_fast_config(auto_arch_config_t* config);

/**
 * @brief Initialize configuration for thorough search (high budget)
 *
 * WHAT: Extensive search for optimal architecture
 * WHY:  Production deployments requiring best performance
 * HOW:  Evolutionary + DARTS hybrid, 10000 evaluations
 *
 * @param config Configuration struct to populate
 * @return 0 on success, negative on error
 */
int auto_arch_thorough_config(auto_arch_config_t* config);

/**
 * @brief Validate configuration before search
 *
 * WHAT: Check configuration for inconsistencies
 * WHY:  Catch errors early before expensive search
 * HOW:  Validate ranges, compatibility, constraints
 *
 * CHECKS:
 * - max_evaluations > 0
 * - population_size > 0 for evolutionary
 * - Constraint ranges are valid
 * - Task specification is complete
 * - Objective weights sum to ~1.0
 *
 * @param config Configuration to validate
 * @return 0 if valid, negative error code
 */
int auto_arch_validate_config(const auto_arch_config_t* config);

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create architecture search context
 *
 * WHAT: Allocate and initialize search context
 * WHY:  Factory function for search creation
 * HOW:  Allocates context, initializes search method, seeds RNG
 *
 * @param config Search configuration (validated before use)
 * @return Search context, or NULL on failure
 *
 * COMPLEXITY: O(population_size) for evolutionary methods
 */
auto_arch_context_t* auto_arch_create(const auto_arch_config_t* config);

/**
 * @brief Destroy architecture search context
 *
 * WHAT: Free all resources used by search
 * WHY:  Prevent memory leaks
 * HOW:  Destroys population, controller, search state
 *
 * @param ctx Search context (can be NULL)
 */
void auto_arch_destroy(auto_arch_context_t* ctx);

/**
 * @brief Set task specification for search
 *
 * WHAT: Define the task to optimize architecture for
 * WHY:  Task determines fitness function and constraints
 * HOW:  Stores task spec, validates compatibility with config
 *
 * @param ctx Search context
 * @param task Task specification
 * @return 0 on success, negative on error
 */
int auto_arch_set_task(auto_arch_context_t* ctx, const auto_arch_task_t* task);

/**
 * @brief Run architecture search
 *
 * WHAT: Execute the architecture search algorithm
 * WHY:  Find optimal architecture for task
 * HOW:  Iteratively generate, evaluate, and select architectures
 *
 * ALGORITHM (Evolutionary):
 * 1. Initialize random population of architectures
 * 2. For each generation:
 *    a. Evaluate fitness of each architecture
 *    b. Select parents via tournament selection
 *    c. Apply crossover and mutation
 *    d. Replace low-fitness individuals
 * 3. Return best architecture found
 *
 * @param ctx Search context
 * @param train_data Training data tensor [n_samples, n_inputs]
 * @param train_labels Training labels tensor [n_samples, n_outputs]
 * @param val_data Validation data (optional, can be NULL)
 * @param val_labels Validation labels (optional, can be NULL)
 * @return Search result with best architecture(s)
 *
 * COMPLEXITY: O(max_evaluations × eval_time)
 * where eval_time = O(epochs × n_samples × architecture_size)
 */
auto_arch_result_t* auto_arch_search(
    auto_arch_context_t* ctx,
    const nimcp_tensor_t* train_data,
    const nimcp_tensor_t* train_labels,
    const nimcp_tensor_t* val_data,
    const nimcp_tensor_t* val_labels
);

/**
 * @brief Resume search from checkpoint
 *
 * WHAT: Continue interrupted architecture search
 * WHY:  Long searches may be interrupted; avoid starting over
 * HOW:  Load population and search state from checkpoint
 *
 * @param ctx Search context
 * @param checkpoint_path Path to checkpoint file
 * @param train_data Training data
 * @param train_labels Training labels
 * @param val_data Validation data (optional)
 * @param val_labels Validation labels (optional)
 * @return Search result
 */
auto_arch_result_t* auto_arch_resume(
    auto_arch_context_t* ctx,
    const char* checkpoint_path,
    const nimcp_tensor_t* train_data,
    const nimcp_tensor_t* train_labels,
    const nimcp_tensor_t* val_data,
    const nimcp_tensor_t* val_labels
);

/**
 * @brief Evaluate a single architecture
 *
 * WHAT: Train and evaluate a specific architecture
 * WHY:  User-defined architectures or manual refinement
 * HOW:  Build network, train, compute fitness
 *
 * @param ctx Search context (for task and eval config)
 * @param arch Architecture to evaluate
 * @param train_data Training data
 * @param train_labels Training labels
 * @param val_data Validation data (optional)
 * @param val_labels Validation labels (optional)
 * @param fitness Output fitness metrics
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(epochs × n_samples × architecture_size)
 */
int auto_arch_evaluate(
    auto_arch_context_t* ctx,
    const auto_arch_architecture_t* arch,
    const nimcp_tensor_t* train_data,
    const nimcp_tensor_t* train_labels,
    const nimcp_tensor_t* val_data,
    const nimcp_tensor_t* val_labels,
    auto_arch_fitness_t* fitness
);

//=============================================================================
// Architecture Manipulation
//=============================================================================

/**
 * @brief Create random architecture within search space
 *
 * WHAT: Generate a random valid architecture
 * WHY:  Initialize population or explore search space
 * HOW:  Sample layer types, sizes, connectivity randomly
 *
 * @param ctx Search context
 * @return Random architecture, or NULL on failure
 */
auto_arch_architecture_t* auto_arch_random_architecture(auto_arch_context_t* ctx);

/**
 * @brief Mutate architecture (evolutionary search)
 *
 * WHAT: Apply random mutation to architecture
 * WHY:  Explore neighborhood of existing architecture
 * HOW:  Randomly modify layer sizes, types, or connections
 *
 * MUTATIONS:
 * - Add/remove layer
 * - Change layer size
 * - Change layer type
 * - Change connectivity pattern
 * - Modify neuron parameters
 *
 * @param arch Architecture to mutate (modified in place)
 * @param mutation_rate Mutation probability [0, 1]
 * @param ctx Search context (for constraints)
 * @return 0 on success, negative on error
 */
int auto_arch_mutate(
    auto_arch_architecture_t* arch,
    float mutation_rate,
    auto_arch_context_t* ctx
);

/**
 * @brief Crossover two architectures (evolutionary search)
 *
 * WHAT: Combine two parent architectures
 * WHY:  Exploit good building blocks from both parents
 * HOW:  Layer-wise crossover or uniform crossover
 *
 * @param parent1 First parent architecture
 * @param parent2 Second parent architecture
 * @param ctx Search context (for constraints)
 * @return Child architecture, or NULL on failure
 */
auto_arch_architecture_t* auto_arch_crossover(
    const auto_arch_architecture_t* parent1,
    const auto_arch_architecture_t* parent2,
    auto_arch_context_t* ctx
);

/**
 * @brief Clone architecture
 *
 * WHAT: Create deep copy of architecture
 * WHY:  Preserve architectures during search
 * HOW:  Allocate new architecture, copy all fields
 *
 * @param arch Architecture to clone
 * @return Cloned architecture, or NULL on failure
 */
auto_arch_architecture_t* auto_arch_clone(const auto_arch_architecture_t* arch);

/**
 * @brief Destroy architecture
 *
 * WHAT: Free architecture and all resources
 * WHY:  Prevent memory leaks
 * HOW:  Free layers array and arch struct
 *
 * @param arch Architecture to destroy (can be NULL)
 */
void auto_arch_architecture_destroy(auto_arch_architecture_t* arch);

/**
 * @brief Validate architecture against constraints
 *
 * WHAT: Check if architecture satisfies all constraints
 * WHY:  Ensure generated architectures are valid
 * HOW:  Check layer count, sizes, connectivity, etc.
 *
 * @param arch Architecture to validate
 * @param constraints Constraints to check
 * @return 0 if valid, negative error code
 */
int auto_arch_validate_architecture(
    const auto_arch_architecture_t* arch,
    const auto_arch_constraints_t* constraints
);

//=============================================================================
// Export/Import
//=============================================================================

/**
 * @brief Export architecture to SNN configuration
 *
 * WHAT: Convert architecture spec to snn_config_t
 * WHY:  Build trainable SNN from discovered architecture
 * HOW:  Map layers to populations, set neuron parameters
 *
 * @param arch Architecture to export
 * @return SNN configuration, or NULL on failure
 */
snn_config_t* auto_arch_export_snn(const auto_arch_architecture_t* arch);

/**
 * @brief Export architecture to LNN configuration
 *
 * WHAT: Convert architecture spec to lnn_config_t
 * WHY:  Build trainable LNN from discovered architecture
 * HOW:  Map layers to LNN layers, set time constants
 *
 * @param arch Architecture to export
 * @return LNN configuration, or NULL on failure
 */
lnn_config_t* auto_arch_export_lnn(const auto_arch_architecture_t* arch);

/**
 * @brief Import architecture from SNN configuration
 *
 * WHAT: Create architecture spec from existing SNN
 * WHY:  Initialize search from hand-designed architecture
 * HOW:  Extract layer specs from SNN populations
 *
 * @param snn_config SNN configuration
 * @return Architecture specification, or NULL on failure
 */
auto_arch_architecture_t* auto_arch_import_snn(const snn_config_t* snn_config);

/**
 * @brief Import architecture from LNN configuration
 *
 * WHAT: Create architecture spec from existing LNN
 * WHY:  Initialize search from hand-designed architecture
 * HOW:  Extract layer specs from LNN layers
 *
 * @param lnn_config LNN configuration
 * @return Architecture specification, or NULL on failure
 */
auto_arch_architecture_t* auto_arch_import_lnn(const lnn_config_t* lnn_config);

/**
 * @brief Save architecture to JSON file
 *
 * WHAT: Serialize architecture to human-readable format
 * WHY:  Persistence, sharing, version control
 * HOW:  Write JSON with all layer specs and parameters
 *
 * @param arch Architecture to save
 * @param filepath Output file path
 * @return 0 on success, negative on error
 */
int auto_arch_save_json(const auto_arch_architecture_t* arch, const char* filepath);

/**
 * @brief Load architecture from JSON file
 *
 * WHAT: Deserialize architecture from file
 * WHY:  Load saved architectures
 * HOW:  Parse JSON, reconstruct architecture struct
 *
 * @param filepath Input file path
 * @return Loaded architecture, or NULL on failure
 */
auto_arch_architecture_t* auto_arch_load_json(const char* filepath);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get current search statistics
 *
 * WHAT: Retrieve statistics about search progress
 * WHY:  Monitor search and diagnose issues
 * HOW:  Fill stats struct from context
 *
 * @param ctx Search context
 * @param stats Output statistics structure
 * @return 0 on success, negative on error
 */
int auto_arch_get_stats(const auto_arch_context_t* ctx, auto_arch_stats_t* stats);

/**
 * @brief Get current search status
 *
 * WHAT: Get current state of search process
 * WHY:  Check if search is running, completed, or errored
 * HOW:  Return status enum from context
 *
 * @param ctx Search context
 * @return Current search status
 */
auto_arch_status_t auto_arch_get_status(const auto_arch_context_t* ctx);

/**
 * @brief Get best architecture found so far
 *
 * WHAT: Retrieve current best architecture
 * WHY:  Monitor progress during long searches
 * HOW:  Clone best architecture from context
 *
 * @param ctx Search context
 * @return Best architecture so far, or NULL if none
 */
auto_arch_architecture_t* auto_arch_get_best(const auto_arch_context_t* ctx);

/**
 * @brief Compute biological plausibility score
 *
 * WHAT: Score how biologically realistic architecture is
 * WHY:  Neuromorphic hardware prefers bio-plausible networks
 * HOW:  Score based on sparsity, local connectivity, time constants
 *
 * SCORING:
 * - Sparsity: +0.3 if 80-95% sparse (cortical typical)
 * - Small-world: +0.2 if small-world topology
 * - Time constants: +0.2 if in biological range (1-100ms)
 * - Local connectivity: +0.2 if >70% local connections
 * - Dale's law: +0.1 if E/I segregation enforced
 *
 * @param arch Architecture to score
 * @return Biological plausibility score [0, 1]
 */
float auto_arch_compute_bio_score(const auto_arch_architecture_t* arch);

//=============================================================================
// Result Handling
//=============================================================================

/**
 * @brief Destroy search result
 *
 * WHAT: Free result and all contained architectures
 * WHY:  Prevent memory leaks
 * HOW:  Free best arch, Pareto frontier, history
 *
 * @param result Search result (can be NULL)
 */
void auto_arch_result_destroy(auto_arch_result_t* result);

/**
 * @brief Save search result to file
 *
 * WHAT: Persist complete search result
 * WHY:  Preserve search outcome for later analysis
 * HOW:  Save all architectures, fitness, and stats
 *
 * @param result Search result
 * @param filepath Output file path
 * @return 0 on success, negative on error
 */
int auto_arch_result_save(const auto_arch_result_t* result, const char* filepath);

/**
 * @brief Load search result from file
 *
 * WHAT: Load previously saved search result
 * WHY:  Analyze past searches
 * HOW:  Deserialize result from file
 *
 * @param filepath Input file path
 * @return Loaded result, or NULL on failure
 */
auto_arch_result_t* auto_arch_result_load(const char* filepath);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get search method name
 *
 * @param method Search method enum
 * @return String name of method
 */
const char* auto_arch_method_name(auto_arch_method_t method);

/**
 * @brief Get network type name
 *
 * @param type Network type enum
 * @return String name of type
 */
const char* auto_arch_network_type_name(auto_arch_network_type_t type);

/**
 * @brief Get task type name
 *
 * @param type Task type enum
 * @return String name of type
 */
const char* auto_arch_task_type_name(auto_arch_task_type_t type);

/**
 * @brief Get layer type name
 *
 * @param type Layer type enum
 * @return String name of type
 */
const char* auto_arch_layer_type_name(auto_arch_layer_type_t type);

/**
 * @brief Print architecture summary
 *
 * WHAT: Print human-readable architecture description
 * WHY:  Debugging and visualization
 * HOW:  Print layer-by-layer structure and parameters
 *
 * @param arch Architecture to print
 */
void auto_arch_print(const auto_arch_architecture_t* arch);

/**
 * @brief Print search result summary
 *
 * WHAT: Print summary of search results
 * WHY:  Quick overview of search outcome
 * HOW:  Print best architecture, fitness, and stats
 *
 * @param result Search result to print
 */
void auto_arch_result_print(const auto_arch_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUTO_ARCHITECTURE_H */
