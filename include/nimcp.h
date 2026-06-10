/**
 * @file nimcp.h
 * @brief Unified NIMCP Public API - Single entry point for all language bindings
 * @version 0.9.0-beta
 * @date 2025-11-04
 *
 * This is the ONLY header file that language bindings should include.
 * It provides a consistent, stable API with proper namespacing.
 *
 * Design Goals:
 * - Single namespace (nimcp_* prefix for ALL symbols)
 * - Opaque handles (no struct exposure)
 * - Version stability (semantic versioning)
 * - Clean C API (C99 compatible)
 * - Language binding friendly (simple types, no complex macros)
 */

#ifndef NIMCP_H
#define NIMCP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/brain/learning/nimcp_brain_experience.h"
#include "core/brain/factory/nimcp_brain_innate.h"
#include "snn/bridges/nimcp_snn_language_bridge.h" /* snn_lang_config_t / stats for #15+#16 RPCs */

/* Deprecation macro for API evolution */
#if defined(__GNUC__) || defined(__clang__)
#define NIMCP_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
#define NIMCP_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#define NIMCP_DEPRECATED(msg)
#endif

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Version Information
//=============================================================================

#ifndef NIMCP_VERSION_MAJOR
#define NIMCP_VERSION_MAJOR 0
#endif
#ifndef NIMCP_VERSION_MINOR
#define NIMCP_VERSION_MINOR 9
#endif
#ifndef NIMCP_VERSION_PATCH
#define NIMCP_VERSION_PATCH 0
#endif
#ifndef NIMCP_VERSION_STRING
#define NIMCP_VERSION_STRING "0.9.0-beta"
#endif

/* P2-7: Define NIMCP_MAX_LABEL_SIZE for predict buffer sizing */
#ifndef NIMCP_MAX_LABEL_SIZE
#define NIMCP_MAX_LABEL_SIZE 64
#endif

/* Maximum size for decision explanation buffers */
#ifndef NIMCP_MAX_EXPLANATION_SIZE
#define NIMCP_MAX_EXPLANATION_SIZE 256
#endif

/**
 * @brief Get NIMCP version as string
 * @return Version string (e.g., "0.9.0-beta")
 */
const char* nimcp_version(void);

/**
 * @brief Get NIMCP version as integer (MAJOR * 10000 + MINOR * 100 + PATCH)
 * @return Version integer (e.g., 20601)
 */
int nimcp_version_int(void);

/**
 * @brief Get ABI layout hash for struct compatibility checking
 *
 * Returns a hash derived from sizeof(neuron_t), sizeof(sparse_synapse_storage_t),
 * and SPARSE_SYNAPSE_EMBEDDED_CAPACITY. If a Python .so was compiled against a
 * different struct layout, this hash will differ and import will fail with a
 * clear error message instead of a silent SIGSEGV.
 *
 * @return ABI layout hash (uint32_t encoded as int)
 */
int nimcp_abi_layout_hash(void);

//=============================================================================
// Opaque Handle Types (consistent naming: nimcp_*_t)
//=============================================================================

/**
 * @brief Opaque handle to a brain instance (high-level learning system)
 */
typedef struct nimcp_brain_handle* nimcp_brain_t;

/**
 * @brief Opaque handle to a neural network instance (low-level control)
 */
typedef struct nimcp_network_handle* nimcp_network_t;

/**
 * @brief Opaque handle to an ethics module
 */
typedef struct nimcp_ethics_handle* nimcp_ethics_t;

/**
 * @brief Opaque handle to a knowledge graph
 */
typedef struct nimcp_knowledge_handle* nimcp_knowledge_t;


//=============================================================================
// Enumerations (consistent naming: NIMCP_*)
//=============================================================================

/**
 * @brief Brain size presets
 */
typedef enum {
    NIMCP_BRAIN_TINY = 0,   /**< 100 neurons, <1MB,  ~0.1ms inference */
    NIMCP_BRAIN_SMALL = 1,  /**< 1K neurons,  ~10MB, ~0.5ms inference */
    NIMCP_BRAIN_MEDIUM = 2, /**< 10K neurons, ~50MB, ~5ms inference */
    NIMCP_BRAIN_LARGE = 3   /**< 100K neurons, ~500MB, ~50ms inference */
} nimcp_brain_size_t;

/**
 * @brief Brain task templates
 */
typedef enum {
    NIMCP_TASK_CLASSIFICATION = 0,   /**< Multi-class classification */
    NIMCP_TASK_REGRESSION = 1,       /**< Continuous value prediction */
    NIMCP_TASK_PATTERN_MATCHING = 2, /**< Pattern recognition */
    NIMCP_TASK_SEQUENCE = 3,         /**< Temporal sequence learning */
    NIMCP_TASK_ASSOCIATION = 4       /**< Association learning */
} nimcp_brain_task_t;

/**
 * @brief Network architecture types for training dispatch
 *
 * WHAT: Specifies which neural network architecture to use for training
 * WHY:  Different architectures require different training algorithms:
 *       - ADAPTIVE: Standard backpropagation (default)
 *       - SNN: Spike-timing dependent plasticity, surrogate gradients, eProp
 *       - LNN: Adjoint method for ODE-based liquid neural networks
 *       - CNN: Convolutional backpropagation with layer-wise gradients
 *       - HYBRID: Multiple network types with unified training
 * HOW:  Set in training config, brain dispatches to appropriate trainer
 */
typedef enum {
    NIMCP_NETWORK_ADAPTIVE = 0,  /**< Standard adaptive network (backprop) - default */
    NIMCP_NETWORK_SNN = 1,       /**< Spiking Neural Network (STDP/eProp/surrogate) */
    NIMCP_NETWORK_LNN = 2,       /**< Liquid Neural Network (adjoint ODE) */
    NIMCP_NETWORK_CNN = 3,       /**< Convolutional Neural Network */
    NIMCP_NETWORK_HYBRID = 4     /**< Mixed architecture (multiple network types) */
} nimcp_network_type_t;

/**
 * @brief SNN training method selection
 */
typedef enum {
    NIMCP_SNN_TRAIN_STDP = 0,        /**< Spike-Timing Dependent Plasticity */
    NIMCP_SNN_TRAIN_R_STDP = 1,      /**< Reward-modulated STDP */
    NIMCP_SNN_TRAIN_EPROP = 2,       /**< Eligibility Propagation (online) */
    NIMCP_SNN_TRAIN_SURROGATE = 3,   /**< Surrogate gradient backprop */
    NIMCP_SNN_TRAIN_HOMEOSTATIC = 4  /**< Homeostatic plasticity */
} nimcp_snn_train_method_t;

/**
 * @brief LNN training method selection
 */
typedef enum {
    NIMCP_LNN_TRAIN_ADJOINT = 0,     /**< Adjoint method (memory efficient) - default */
    NIMCP_LNN_TRAIN_BPTT = 1,        /**< Backprop through time */
    NIMCP_LNN_TRAIN_RTRL = 2,        /**< Real-time recurrent learning */
    NIMCP_LNN_TRAIN_EPROP = 3        /**< Eligibility propagation */
} nimcp_lnn_train_method_t;

/**
 * @brief Return codes for all NIMCP functions
 *
 * NIMCP uses a unified positive integer error code system:
 *   - NIMCP_OK/NIMCP_SUCCESS = 0
 *   - Error codes are positive integers (1000+)
 *
 * Note: These undefines prevent conflicts with internal error code macros
 * when this header is included after internal headers.
 */
#undef NIMCP_OK
#undef NIMCP_SUCCESS
#undef NIMCP_ERROR
#undef NIMCP_ERROR_NULL_ARG
#undef NIMCP_ERROR_INVALID
#undef NIMCP_ERROR_MEMORY
#undef NIMCP_ERROR_IO

typedef enum {
    NIMCP_OK = 0,                 /**< Success */
    NIMCP_ERROR = 1000,           /**< Generic error */
    NIMCP_ERROR_NULL_ARG = 1003,  /**< NULL argument provided */
    NIMCP_ERROR_INVALID = 1004,   /**< Invalid argument value */
    NIMCP_ERROR_MEMORY = 2000,    /**< Memory allocation failed */
    NIMCP_ERROR_IO = 4000         /**< I/O operation failed */
} nimcp_status_t;

/* Alias for compatibility */
#define NIMCP_SUCCESS NIMCP_OK

//=============================================================================
// Brain API - High-Level Learning Interface
//=============================================================================

/**
 * @brief Create a brain with preset configuration
 *
 * @param name Human-readable name (e.g., "ethics", "classifier")
 * @param size Brain size preset (NIMCP_BRAIN_TINY to NIMCP_BRAIN_LARGE)
 * @param task Task template (NIMCP_TASK_CLASSIFICATION, etc.)
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @return Brain handle or NULL on error
 */
nimcp_brain_t nimcp_brain_create(
    const char* name,
    nimcp_brain_size_t size,
    nimcp_brain_task_t task,
    uint32_t num_inputs,
    uint32_t num_outputs
);

/**
 * @brief Create brain with explicit neuron count (bypasses size preset)
 *
 * @param name Human-readable name
 * @param task Task template (NIMCP_TASK_CLASSIFICATION, etc.)
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param neuron_count Exact number of hidden neurons
 * @return Brain handle or NULL on error
 */
nimcp_brain_t nimcp_brain_create_with_neurons(
    const char* name,
    nimcp_brain_task_t task,
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t neuron_count
);

/**
 * @brief Create brain with ALL subsystems enabled (RESEARCH profile + extras)
 *
 * Uses the RESEARCH config profile as a base and additionally enables:
 * world model, creative system, LGSS safety, neuromodulators, immune bridges,
 * collective cognition bridges, spike NLP, health agent, cycle coordinator.
 * All lazy initialization is disabled — everything initializes at creation.
 *
 * @param name Brain name
 * @param task Task type
 * @param num_inputs Number of input features
 * @param num_outputs Number of output classes
 * @param neuron_count Number of neurons
 * @return Brain handle, or NULL on failure
 */
nimcp_brain_t nimcp_brain_create_full(
    const char* name,
    nimcp_brain_task_t task,
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t neuron_count
);

/**
 * @brief Create brain with fast initialization (core + training only)
 *
 * Initializes only the subsystems required for training and inference.
 * All other subsystems are deferred to first use (lazy initialization).
 * 3-5x faster than full init for large brains.
 *
 * @param name Brain name
 * @param task Task type
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param neuron_count Number of neurons
 * @return Brain handle or NULL on error
 */
nimcp_brain_t nimcp_brain_create_fast(
    const char* name,
    nimcp_brain_task_t task,
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t neuron_count
);

/**
 * @brief Destroy a brain and free all resources
 *
 * @param brain Brain handle
 */
void nimcp_brain_destroy(nimcp_brain_t brain);

/**
 * @brief Learn from a single example
 *
 * @param brain Brain handle
 * @param features Input features array
 * @param num_features Number of input features
 * @param label Output label/class
 * @param confidence Confidence/importance of this example (0.0-1.0)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_learn_example(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    const char* label,
    float confidence
);

/**
 * @brief Learn from a dense target vector (teacher distillation / generative training)
 *
 * Unlike nimcp_brain_learn_example() which converts a string label to one-hot,
 * this function trains toward an arbitrary dense target vector (e.g. a semantic
 * embedding from a teacher model). Uses LEARN_MODE_DISTILLATION internally.
 *
 * @param brain Brain handle
 * @param features Input features array
 * @param num_features Number of input features
 * @param target Dense target output vector
 * @param target_size Size of target vector (must match brain num_outputs)
 * @param label Optional semantic label for tracking (can be NULL)
 * @param confidence Training weight [0.0-1.0]
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_learn_vector(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    const float* target,
    uint32_t target_size,
    const char* label,
    float confidence
);

/**
 * @brief Unified experience: perceive input, predict output, and learn
 * @see brain_experience() for detailed documentation
 */
nimcp_status_t nimcp_brain_experience(
    nimcp_brain_t brain,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size,
    float teacher_reward,
    brain_experience_result_t* result
);

/**
 * @brief Configure experience-based learning
 */
nimcp_status_t nimcp_brain_experience_configure(
    nimcp_brain_t brain,
    const brain_experience_config_t* config
);

/**
 * @brief Provide a correction after an experience
 * @return Loss value (0=perfect), -1.0 on error
 */
float nimcp_brain_experience_correct(
    nimcp_brain_t brain,
    const float* expected,
    uint32_t expected_size
);

/**
 * @brief Direct attention to a specific modality
 */
nimcp_status_t nimcp_brain_experience_attend(
    nimcp_brain_t brain,
    const char* modality,
    float strength
);

/**
 * @brief Hardwire innate circuits into a brain
 * @see brain_innate_hardwire() for detailed documentation
 */
nimcp_status_t nimcp_brain_innate_hardwire(
    nimcp_brain_t brain,
    const innate_config_t* config
);

/**
 * @brief Learn from a batch of examples
 *
 * @param brain Brain handle
 * @param features_array Array of feature pointers (one per example)
 * @param num_features_array Array of feature counts (one per example, can be NULL if uniform)
 * @param labels Array of label strings (one per example)
 * @param confidences Array of confidence values (one per example, can be NULL for default 1.0)
 * @param num_examples Number of examples in the batch
 * @param losses_out Caller-allocated float[num_examples] for per-example losses (can be NULL)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_learn_batch(
    nimcp_brain_t brain,
    const float** features_array,
    const uint32_t* num_features_array,
    const char** labels,
    const float* confidences,
    uint32_t num_examples,
    float* losses_out
);

/**
 * @brief Batch vector learning with GPU gradient accumulation
 *
 * Processes multiple (features, target) pairs in a single mini-batch.
 * On GPU: accumulates gradients across all samples, applies averaged update once.
 * On CPU: falls back to per-sample learning.
 *
 * @param brain Brain handle
 * @param features_array Array of feature pointers [num_examples]
 * @param targets_array Array of target pointers [num_examples]
 * @param num_features Feature vector size (uniform for all examples)
 * @param target_size Target vector size (uniform for all examples)
 * @param num_examples Number of examples in the batch
 * @return Average loss across batch, or -1.0f on error
 */
float nimcp_brain_learn_vector_batch(
    nimcp_brain_t brain,
    const float** features_array,
    const float** targets_array,
    uint32_t num_features,
    uint32_t target_size,
    uint32_t num_examples,
    float learning_rate
);

/**
 * @brief Get the loss value from the most recent learn_example call
 *
 * @param brain Brain handle
 * @return Loss value (>= 0.0 on success, -1.0 if no learning has occurred)
 */
float nimcp_brain_get_last_loss(nimcp_brain_t brain);

/**
 * @brief Get the gradient L2 norm from the most recent learn() call
 *
 * @param brain Brain handle
 * @return Gradient norm (>= 0.0), or -1.0 if brain is NULL
 */
float nimcp_brain_get_last_gradient_norm(nimcp_brain_t brain);

/**
 * @brief Get running label-match accuracy (EMA)
 *
 * @param brain Brain handle
 * @return Running accuracy [0.0, 1.0] or 0.0 if brain is NULL
 */
float nimcp_brain_get_accuracy(nimcp_brain_t brain);

/**
 * @brief Get cross-modal concept_registry stats (Option-1 rebuild).
 *
 * Returns how many distinct referents the brain has interned and how
 * many cross-modal bindings have been recorded. Useful for verifying
 * the bind hook in brain_learn_vector is actually firing during
 * training. Both out params are NULL-tolerant.
 *
 * Returns NIMCP_OK on success (counters populated). NIMCP_OK with both
 * params zeroed if the brain was created without a concept_registry
 * (older brains loaded from pre-Slice-B checkpoints).
 *
 * @param brain                     Brain handle
 * @param total_referents           [out] count of unique interned referents
 * @param total_modality_bindings   [out] count of bind_modalities calls
 */
nimcp_status_t nimcp_brain_get_concept_registry_stats(
    nimcp_brain_t brain,
    size_t* total_referents,
    size_t* total_modality_bindings);

/**
 * @brief Make a decision/prediction
 *
 * @param brain Brain handle
 * @param features Input features array
 * @param num_features Number of input features
 * @param out_label Buffer to store predicted label (must be pre-allocated, min 64 bytes)
 * @param out_confidence Pointer to store prediction confidence (0.0-1.0)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_predict(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    char* out_label,
    float* out_confidence
);

/**
 * @brief Fast prediction — forward pass only, no cognitive stages
 *
 * WHAT: Runs neural network forward pass and returns predicted label
 * WHY:  nimcp_brain_predict() runs 28 cognitive stages (sleep, curiosity, theory of mind,
 *        mirror neurons, ethics, etc.) on every call. This function skips all that and
 *        does a pure forward pass, making it 10-100x faster for training loops.
 * HOW:  Calls adaptive_network_forward() directly, then determine_output_label()
 *
 * @param brain Brain handle
 * @param features Input features array
 * @param num_features Number of input features
 * @param out_label Buffer to store predicted label (must be pre-allocated, min 64 bytes)
 * @param out_confidence Pointer to store prediction confidence (0.0-1.0)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_predict_fast(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    char* out_label,
    float* out_confidence
);

/**
 * @brief Domain-scoped fast prediction (forward pass only)
 *
 * WHAT: Same as predict_fast but restricts argmax to output neurons whose label
 *       starts with the given domain prefix (e.g., "biology:" only considers
 *       neurons mapped to biology labels).
 * WHY:  Multi-domain training causes first-mover dominance — domains that train
 *       first develop stronger output activations, causing predict_fast to always
 *       return labels from the dominant domain for inputs with similar features.
 * HOW:  Forward pass + domain-filtered argmax + domain-scoped confidence
 *
 * @param brain Brain handle
 * @param features Input features array
 * @param num_features Number of input features
 * @param domain_prefix Domain prefix to filter by (e.g., "biology:"). If NULL,
 *        behaves identically to nimcp_brain_predict_fast.
 * @param out_label Buffer to store predicted label (must be pre-allocated, min 64 bytes)
 * @param out_confidence Pointer to store prediction confidence (0.0-1.0)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_predict_in_domain(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    const char* domain_prefix,
    char* out_label,
    float* out_confidence
);

/**
 * @brief Run inference and get raw output vector
 *
 * WHAT: Forward pass through network returning raw outputs
 * WHY:  For numeric predictions, embeddings, or when you don't need label classification
 * HOW:  Processes inputs through network, returns output activations
 *
 * @param brain Brain handle
 * @param features Input features array
 * @param num_features Number of input features
 * @param outputs Output array to fill (must be pre-allocated)
 * @param num_outputs Number of outputs (must match brain's output dimension)
 * @return NIMCP_OK on success, error code otherwise
 *
 * EXAMPLE:
 * ```c
 * float features[20] = {...};
 * float outputs[10];
 * nimcp_status_t result = nimcp_brain_infer(brain, features, 20, outputs, 10);
 * ```
 */
nimcp_status_t nimcp_brain_infer(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    float* outputs,
    uint32_t num_outputs
);

/**
 * @brief Run full cognitive pipeline and return complete decision data
 *
 * Unlike nimcp_brain_predict() which only returns label+confidence,
 * this function returns the full brain_decision_t contents including
 * explanation text, output vector, active neuron count, and sparsity.
 *
 * @param brain Brain handle
 * @param features Input feature array
 * @param num_features Number of features
 * @param out_label Buffer for predicted label (min 64 bytes)
 * @param out_confidence Pointer to store confidence
 * @param out_explanation Buffer for explanation text (min 256 bytes)
 * @param out_output_vector Buffer for raw output vector
 * @param out_output_size In: buffer capacity, Out: actual output size
 * @param out_num_active_neurons Pointer to store active neuron count
 * @param out_sparsity Pointer to store sparsity value
 * @param out_inference_time_us Pointer to store inference time in microseconds
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_decide_full(
    nimcp_brain_t brain,
    const float* features, uint32_t num_features,
    char* out_label, float* out_confidence,
    char* out_explanation,
    float* out_output_vector, uint32_t* out_output_size,
    uint32_t* out_num_active_neurons, float* out_sparsity,
    uint64_t* out_inference_time_us
);

/**
 * @brief Generate spoken text from brain's neural state
 *
 * Routes semantic representation through Broca's language production
 * pipeline (lexical selection → syntactic encoding → text output).
 *
 * @param brain Brain handle
 * @param semantic_input Semantic vector (brain output), or NULL to use last decision
 * @param semantic_dim Dimension of semantic_input (ignored if NULL)
 * @param out_text Output text buffer for generated utterance
 * @param text_max_len Maximum length of out_text buffer
 * @param out_confidence Pointer to store production confidence [0,1] (optional)
 * @param out_fluency Pointer to store fluency score [0,1] (optional)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_speak(
    nimcp_brain_t brain,
    const float* semantic_input,
    uint32_t semantic_dim,
    char* out_text,
    uint32_t text_max_len,
    float* out_confidence,
    float* out_fluency
);

/**
 * @brief Train the language generator on a text sequence
 *
 * Performs one training step using teacher-forced cross-entropy loss
 * on the LNN decoder. Trains the tokenizer vocabulary, embedding layer,
 * and output projection weights.
 *
 * @param brain Brain handle
 * @param input_text Input text for training
 * @param target_text Target text (next-token prediction target)
 * @param learning_rate Learning rate (0 = use default 0.001)
 * @param out_loss Pointer to store training loss (optional)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_train_language(
    nimcp_brain_t brain,
    const char* input_text,
    const char* target_text,
    float learning_rate,
    float* out_loss
);

/**
 * @brief Generate text from cognitive state using LNN decoder
 *
 * Uses the autoregressive LNN decoder with softmax sampling
 * to generate text token-by-token from a semantic input vector.
 * Falls back to language orchestrator if generator not initialized.
 *
 * @param brain Brain handle
 * @param prompt Text prompt for continuation (NULL for free generation)
 * @param semantic_input Semantic vector for cognitive-state generation (NULL if using prompt)
 * @param semantic_dim Dimension of semantic_input
 * @param out_text Output text buffer
 * @param text_max_len Maximum length of out_text buffer
 * @param out_confidence Pointer to store confidence [0,1] (optional)
 * @param out_perplexity Pointer to store perplexity (optional)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_generate_text(
    nimcp_brain_t brain,
    const char* prompt,
    const float* semantic_input,
    uint32_t semantic_dim,
    char* out_text,
    uint32_t text_max_len,
    float* out_confidence,
    float* out_perplexity
);

// =========================================================================
// GROUNDED LANGUAGE API (human-like word-concept binding)
// =========================================================================

/**
 * @brief Teach the brain a word through grounded experience
 *
 * Associates a word form with a sensory feature vector through Hebbian binding.
 * This models how humans learn words — by experiencing the referent.
 *
 * @param brain        Brain handle
 * @param word         Word form to learn
 * @param features     Sensory feature vector (e.g., visual features, semantic embedding)
 * @param feature_dim  Dimension of feature vector
 * @param modality     Sensory modality (0=visual, 1=auditory, 2=motor, 3=emotional, 4=spatial, 5=linguistic)
 * @param attention    Attentional weight [0,1] — higher = stronger binding
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_ground_word(
    nimcp_brain_t brain,
    const char* word,
    const float* features,
    uint32_t feature_dim,
    uint32_t modality,
    float attention
);

/**
 * @brief Teach the brain a word with explicit emotional context.
 *
 * Same as nimcp_brain_ground_word but also stamps the grounding event with
 * emotional valence/arousal. The legacy zero-emotion path delegates to this
 * function (valence=0, arousal=0), so there is exactly ONE implementation
 * of the C-level event construction.
 *
 * @param brain         Brain handle
 * @param word          Word form to learn
 * @param features      Sensory feature vector
 * @param feature_dim   Dimension of feature vector
 * @param modality      Sensory modality (see nimcp_brain_ground_word)
 * @param attention     Attentional weight [0,1]
 * @param valence       Emotional valence in [-1, +1]
 * @param arousal       Emotional arousal in [0, 1]
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_ground_word_with_emotion(
    nimcp_brain_t brain,
    const char* word,
    const float* features,
    uint32_t feature_dim,
    uint32_t modality,
    float attention,
    float valence,
    float arousal
);

/**
 * @brief Bootstrap the grounded lexicon from a curated JSON fixture.
 *
 * One-shot loader for data/lexicon/base_lexicon_v1.json (or any fixture
 * with the same schema). Each entry — { "form", "class", "features" } —
 * is forwarded to grounded_language_fast_map, creating word, concept,
 * binding, and context vector in one pass.
 *
 * Use case: avoid waiting for the brain to learn ~1500 common
 * nouns/verbs/adjectives from raw distributional exposure (slow). The
 * fixture seeds those content words so curriculum text immediately has
 * lexical anchors — function words like "the/is/and" are already
 * seeded by grounded_language_create.
 *
 * Schema (validated at load time):
 *   {
 *     "version": 1,
 *     "semantic_dim_hint": 128,
 *     "words": [
 *       { "form": "dog",   "class": "noun",      "features": [...] },
 *       { "form": "run",   "class": "verb",      "features": [...] },
 *       { "form": "red",   "class": "adjective", "features": [...] }
 *     ]
 *   }
 *
 * Behavior:
 *   - Malformed individual word entries are skipped (logged once at end);
 *     the call still returns NIMCP_OK if at least the top-level frame
 *     parsed.
 *   - Top-level malformed JSON or unsupported version returns NIMCP_ERROR
 *     / NIMCP_ERROR_INVALID respectively.
 *   - File-open / read failures return NIMCP_ERROR.
 *
 * @param brain      Brain handle (must have grounded_lang attached)
 * @param json_path  Path to a base_lexicon_v*.json file
 * @return NIMCP_OK on success, NIMCP_ERROR_INVALID on bad input/version,
 *         NIMCP_ERROR on file/parse failure.
 */
nimcp_status_t nimcp_brain_bootstrap_lexicon(
    nimcp_brain_t brain,
    const char* json_path
);

/**
 * @brief Bulk-load a packed binary lexicon (50K-150K words) into grounded_lang.
 *
 * Streams a .bin produced by scripts/build_wordnet_glove_lexicon.py
 * directly through grounded_language_fast_map(). Use this instead of
 * nimcp_brain_bootstrap_lexicon when you need 10× more vocabulary or
 * want a faster cold-start (binary parse vs JSON parse). Both loaders
 * coexist; you can call them in sequence.
 *
 * Binary format v1 (little-endian):
 *   Header: u32 magic('BLEX'), u32 version(=1), u32 word_count,
 *           u32 vector_dim(=GL_SEMANTIC_DIM=128), u32 record_max_form,
 *           u32 reserved[3]
 *   Record: u16 form_len, u8[form_len] form (ASCII, no NUL),
 *           u8 class_enum, u8 reserved, f32 vec[vector_dim]
 *
 * Behavior:
 *   - Each record is forwarded to grounded_language_fast_map() with the
 *     class_enum byte passed as the `category` argument.
 *   - A single bad record stops the load (logged), but earlier inserted
 *     entries are kept — partial loads are intentional.
 *   - Returns NIMCP_OK iff at least one entry was inserted.
 *
 * @param brain     Brain handle (must have grounded_lang attached)
 * @param bin_path  Path to a wordnet_glove_v*.bin file
 * @return NIMCP_OK if any entries inserted, NIMCP_ERROR on file/parse
 *         failure, NIMCP_ERROR_INVALID on bad magic/version/dim.
 */
nimcp_status_t nimcp_brain_load_bulk_lexicon(
    nimcp_brain_t brain,
    const char* bin_path
);

/**
 * @brief Learn language from text (distributional + syntactic patterns)
 *
 * Processes text to learn word co-occurrence patterns, infer word classes,
 * and extract syntactic templates — like how children learn from exposure.
 *
 * @param brain  Brain handle
 * @param text   Text to learn from
 * @param out_loss  Output: learning loss (lower = better alignment), can be NULL
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_learn_language(
    nimcp_brain_t brain,
    const char* text,
    float* out_loss
);

/**
 * @brief Learn from input-target text pairs (teacher-guided)
 *
 * Strengthens associations between input concepts and target words.
 * Like a parent pointing and naming, or correcting speech.
 *
 * @param brain        Brain handle
 * @param input_text   Stimulus/input text
 * @param target_text  Expected/correct response
 * @param learning_rate Hebbian learning rate (0 = default)
 * @param out_loss     Output: learning loss
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_learn_language_pair(
    nimcp_brain_t brain,
    const char* input_text,
    const char* target_text,
    float learning_rate,
    float* out_loss
);

/**
 * @brief Comprehend text using grounded semantics
 *
 * Activates grounded concept bindings for each word, integrates meaning,
 * and returns the semantic representation.
 *
 * @param brain         Brain handle
 * @param text          Text to comprehend
 * @param out_semantic  Output: semantic vector (caller allocates, at least 128 floats)
 * @param semantic_dim  Dimension of output semantic vector
 * @param out_confidence Output: comprehension confidence [0,1]
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_comprehend(
    nimcp_brain_t brain,
    const char* text,
    float* out_semantic,
    uint32_t semantic_dim,
    float* out_confidence
);

/**
 * @brief Produce text from semantic intent (grounded production)
 *
 * Generates text by finding words that map to concepts closest to the
 * semantic intent vector, then arranging them using learned syntactic patterns.
 *
 * @param brain         Brain handle
 * @param intent        Semantic intent vector (what to express)
 * @param intent_dim    Dimension of intent vector
 * @param out_text      Output: generated text buffer
 * @param text_max_len  Maximum output text length
 * @param out_confidence Output: production confidence [0,1]
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_produce_text(
    nimcp_brain_t brain,
    const float* intent,
    uint32_t intent_dim,
    char* out_text,
    uint32_t text_max_len,
    float* out_confidence
);

/**
 * @brief Respond to text input using grounded comprehension + production
 *
 * Full conversation turn: comprehend input, then produce grounded response.
 *
 * @param brain         Brain handle
 * @param input_text    Input text to respond to
 * @param out_response  Output: response text buffer
 * @param response_max  Maximum response buffer length
 * @param out_confidence Output: response confidence [0,1]
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_grounded_respond(
    nimcp_brain_t brain,
    const char* input_text,
    char* out_response,
    uint32_t response_max,
    float* out_confidence
);

/**
 * @brief Diagnostic snapshot of grounded-language state.
 *
 * Read-only counters/averages for language-collapse triage. Single
 * responsibility: surface the grounded_language module's internal stats
 * + the SNN-language bridge's blend without piggybacking on the existing
 * stats endpoints (those are already crowded).
 *
 * vocab_size = 0 with a non-NULL grounded_lang signals an init-only system.
 * snn_bridge_blend = -1.0f signals "no bridge attached".
 */
typedef struct {
    /* Lexicon + comprehension */
    uint32_t vocab_size;
    uint32_t total_bindings;
    uint32_t total_groundings;
    uint32_t total_comprehensions;
    uint32_t total_productions;
    float    avg_binding_strength;
    float    avg_comprehension_confidence;
    float    vocabulary_growth_rate;
    /* SNN-language bridge */
    float    snn_bridge_blend;        /**< Current blend factor [0,1] or -1 if no bridge */
    uint32_t bridge_total_productions;
    float    bridge_avg_word_confidence;
    float    bridge_avg_binding_weight;
    uint32_t bridge_active_bindings;
    /* Campaign feature flags + tunables (Audit-2 follow-up: lang_status surface).
     * Provides a single-call read of all default-OFF runtime knobs so the
     * trainer can see "what's enabled" without 9 separate getters. */
    uint8_t  enable_negation_inversion;
    uint8_t  enable_sense_disambiguation;
    uint8_t  enable_speech_act_classification;
    uint8_t  enable_sentence_segmentation;
    uint8_t  enable_topic_shift_detection;
    uint8_t  enable_reconsolidation;
    uint8_t  enable_anaphora_resolution;
    uint8_t  bridge_enable_da_modulation;
    uint8_t  bridge_enable_trigram_learning;
    uint8_t  _reserved_flag_1;
    float    reconsolidation_decay;
    float    topic_shift_threshold;
    uint32_t topic_shift_min_turns;
    /* Margin gate threshold for learn_next_token_pair / _triple LTD.
     * Default 1.5 — see snn_lang_config_t::ltd_margin in the bridge
     * header for the rule. Exposed here so lang_status can surface it
     * alongside the other bridge tunables. */
    float    bridge_ltd_margin;
    /* TC-11 decode telemetry — operators use these to decide if/when to
     * invest in the GPU port. avg_decode_us = decode_total_ns / total_decode_calls
     * is the trigger metric (>100µs at production scale = GPU port wins). */
    uint64_t bridge_decode_total_ns;
    uint64_t bridge_total_decode_calls;
    /* Bridge plasticity telemetry surface. Lets observers verify that a
     * flag flip (e.g. set_trigram_learning_enabled) actually drives the
     * underlying counter, and discriminate cascade self_train misses
     * between "lr=0" (no echo_correct calls) and "intent=0" (calls fire
     * but zero pairs strengthened) and "word not registered"
     * (target_misses bumps). All fields mirror snn_lang_stats_t. */
    uint64_t bridge_total_stdp_updates;
    uint64_t bridge_total_trigram_updates;
    uint64_t bridge_echo_correct_calls;
    uint64_t bridge_echo_correct_pairs;
    uint64_t bridge_echo_correct_target_misses;
    uint64_t bridge_comprehend_stdp_passes;
    uint64_t bridge_comprehend_stdp_pairs_fired;
    uint64_t bridge_da_gated_stdp_passes;
    float    bridge_last_da_modulation;
    /* Grounded-language next-token cold-start counter. Discriminates
     * trigram-path "stall" from "ramp": learn_next_token_triple bails
     * before inc_trigram_updates when either prev-word lacks a concept
     * binding (total <= 1e-6). A climbing counter alongside a flat
     * bridge_total_trigram_updates means the path IS reached but is
     * cold-start-bounced (will resolve as bindings accrue); a flat
     * counter means the path is never reached at all (labels too short). */
    uint64_t next_token_cold_start_skips;
    /* Wave-3 (2026-05-19) S4 lateral-inhibition counters — mirror of
     * snn_lang_stats_t equivalents. All zero when enable_lateral_inhibition
     * is false. winner_margin_sum and settled_steps_sum are running sums;
     * divide by decode_calls (and 1e6 for margin) to get means. */
    uint64_t bridge_lateral_inhibition_decode_calls;
    uint64_t bridge_lateral_inhibition_winner_margin_sum;
    uint64_t bridge_lateral_inhibition_settled_steps_sum;
    uint64_t bridge_lateral_inhibition_nan_fallbacks;
    /* NLP-1 / NLP-2 reachability telemetry (full-walkthrough 2026-05-25).
     * Both features were committed but unreachable at runtime (no toggle
     * surfaced through C/Python/RPC). These let an operator confirm the
     * flag flip drives the underlying pass, and that the pass actually
     * resolves (resolved/attempts ratio). coref_* are per-GL counters;
     * subword_oov_* are per-GL and only climb when a tokenizer is
     * attached (brain init wires brain_tokenizer when present). */
    uint8_t  enable_coref_resolution;
    uint8_t  enable_subword_oov_fallback;
    uint64_t coref_attempts;
    uint64_t coref_resolved;
    uint64_t subword_oov_attempts;
    uint64_t subword_oov_resolved;
    /* Produce-score rebalance (2026-05-27): current distributional weight in
     * the produce word score (raw = w*distributional + (1-w)*concept).
     * Default 0.4 = historical 0.4/0.6 split. */
    float    produce_distributional_weight;
    /* Produce-score frequency penalty (2026-06-01): current IDF-style penalty
     * applied in produce scoring (raw *= 1/(1+p*log1p(freq))). 0.0 = OFF. */
    float    produce_frequency_penalty;
    /* TF (tier-feedback) telemetry surface — exposed so operators can see
     * (a) how often the cascade emits deltas (tf_calls / tf_deltas_captured),
     * (b) which gate is rejecting (per-reason tf_outcome_blocked_*),
     * (c) what plasticity actually fired (tf_trigram_updates / tf_distrib_updates
     *     / tf_stdp_updates_pos / tf_stdp_updates_neg).
     * Mirrors gl_stats_t fields populated by nimcp_grounded_language_tf.c. */
    uint64_t tf_calls;
    uint64_t tf_deltas_captured;
    uint64_t tf_outcome_ok;
    uint64_t tf_outcome_blocked_stage;
    uint64_t tf_outcome_blocked_master;
    uint64_t tf_outcome_blocked_da;
    uint64_t tf_outcome_blocked_stale;
    uint64_t tf_outcome_blocked_retry;
    uint64_t tf_trigram_updates;
    uint64_t tf_distrib_updates;
    uint64_t tf_stdp_updates_pos;
    uint64_t tf_stdp_updates_neg;
    /* TF runtime knobs — the same five surfaced by the per-setter wrappers,
     * mirrored here so a single get_grounded_language_diagnostics() call gives
     * an operator the complete TF picture. */
    uint8_t  produce_corrector_feedback_enabled;  /* TF master */
    uint16_t tf_enabled_correctors;               /* per-corrector bitmask */
    float    tf_lr_trigram;
    float    tf_lr_distrib;
    float    tf_lr_bridge_stdp;
    /* T3-2 (2026-05-29): cohesive conjunction insertion runtime flag. */
    uint8_t  produce_t3_conjunction;
    /* Increment-1 (2026-06-02): SNN-as-generator A/B switch. 1 = produce sources
     * candidates from the SNN Broca spike cache (lexicon fallback). Default 0. */
    uint8_t  produce_via_snn;
    /* Metacognitive produce-floor modulation flag (2026-06-02). */
    uint8_t  metacog_gates_produce;
} nimcp_grounded_language_diagnostics_t;

/**
 * @brief Get grounded-language diagnostic snapshot.
 *
 * Best-effort: returns NIMCP_OK with zeroed struct (and bridge_blend = -1.0f)
 * when grounded_lang or snn_bridge are absent.
 */
nimcp_status_t nimcp_brain_get_grounded_language_diagnostics(
    nimcp_brain_t brain,
    nimcp_grounded_language_diagnostics_t* out
);

/**
 * @brief One-shot maintenance sweep: prune accumulated grounded-language
 * bindings to top-K per word.
 *
 * Wraps grounded_language_prune_bindings. Intended as a single call after the
 * 2026-05-16 cross-bind wedge fix to reclaim B per content word from the
 * function-word leak. For each lexicon entry, sorts bindings descending by
 * strength and truncates to @p max_bindings_per_word.
 *
 * Destructive — bindings dropped are not recoverable without checkpoint
 * rollback. Suggested K=128 retains the strongest bindings while bounding
 * cross-bind cost.
 *
 * @param brain                    Brain handle (must have grounded_lang)
 * @param max_bindings_per_word    K, the per-word cap (must be > 0)
 * @param dropped_out              Optional: total bindings dropped is written here
 * @return NIMCP_OK on success, NIMCP_ERROR_INVALID on bad input,
 *         NIMCP_ERROR if grounded_lang not attached.
 */
nimcp_status_t nimcp_brain_prune_lang_bindings(
    nimcp_brain_t brain,
    uint32_t max_bindings_per_word,
    uint64_t* dropped_out
);

/**
 * @brief Runtime fix for bridge->neuromod NULL state.
 *
 * The SNN language bridge's DA gate is conditioned on
 * bridge->neuromod != NULL. Both init paths
 * (nimcp_brain_init_language.c:841 and
 * nimcp_brain_init_language_pops.c:370) connect it only if
 * brain->neuromodulator_system already exists at the time the
 * language init wave runs. If init order puts the bridge first,
 * bridge->neuromod stays NULL forever — bridge_da_gated_stdp_passes
 * never increments. This API lets an operator re-connect them at
 * runtime once both exist (just stores a pointer, no concurrency
 * hazard).
 *
 * Returns NIMCP_OK on success, NIMCP_ERROR if the bridge or
 * neuromod system is missing.
 */
nimcp_status_t nimcp_brain_connect_lang_bridge_neuromod(nimcp_brain_t brain);

/**
 * @brief PA-4+ : FFT-based bigram spectral metrics.
 *
 * Diagnostic snapshot: returns the spectral structure of the bigram
 * count matrix accumulated by grounded_language_learn_text_bigrams().
 *
 * On first call, lazily creates and attaches an internal bigram-spectrum
 * tracker to the brain's grounded language module. The tracker is owned
 * for the brain's lifetime; subsequent calls return a fresh FFT-derived
 * snapshot of accumulated bigrams.
 *
 * Field semantics:
 *   peak_strength            (max |F| - mean |F|) / std |F| over the 2D
 *                            bigram-frequency spectrum (DC excluded).
 *                            High under repeating/periodic structure.
 *   low_freq_concentration   sum |F|^2 in the inner-quarter spectrum
 *                            divided by total |F|^2. High when a few
 *                            global hubs (function words) dominate.
 *   spectral_entropy         -Σ p log p where p = |F|^2 / Σ|F|^2 (DC
 *                            excluded). Low under structure.
 *
 * The bigram_spectral_metrics_t typedef lives in
 *   include/language/nimcp_bigram_spectrum.h
 * which this header includes. Callers can access the struct via either
 * nimcp.h or the language header.
 *
 * @param brain   Brain handle.
 * @param out     Output metrics struct (zeroed on error or empty matrix).
 * @return NIMCP_OK on success.
 */
#include "language/nimcp_bigram_spectrum.h"

nimcp_status_t nimcp_brain_get_bigram_spectral_metrics(
    nimcp_brain_t brain,
    bigram_spectral_metrics_t* out
);

/**
 * @brief Probe the comprehension pipeline for a single text.
 *
 * Runs grounded_language_comprehend(text) read-only and surfaces metrics
 * needed for collapse-detection: comprehension confidence, semantic vector
 * L2 norm, concept count, and the first N components of the semantic
 * vector for cross-prompt cosine analysis. Does NOT call produce(), so
 * this is safe to fan out across many probes without polluting state.
 *
 * @param brain               Brain handle
 * @param input_text          Text to comprehend
 * @param out_components      Output buffer for first N semantic-vector components
 * @param max_components      Capacity of out_components
 * @param out_components_written  Number of components written (≤ max_components)
 * @param out_l2_norm         L2 norm of full semantic vector (or 0.0 on error)
 * @param out_confidence      comprehension_confidence (or 0.0 on error)
 * @param out_concept_count   activated_concepts count (or 0 on error)
 * @return NIMCP_OK on success; struct fields zeroed on error.
 */
nimcp_status_t nimcp_brain_probe_comprehend(
    nimcp_brain_t brain,
    const char* input_text,
    float* out_components,
    uint32_t max_components,
    uint32_t* out_components_written,
    float* out_l2_norm,
    float* out_confidence,
    uint32_t* out_concept_count
);

/**
 * @brief Public phrase-entry POD copy (decouples API from internal gl_phrase_t,
 *        which carries heap pointers we don't want to expose).
 *
 * `form` is a fixed-size lowercased space-joined string (e.g., "good morning").
 * `component_words` is 2 (bigram) or 3 (trigram).
 */
#define NIMCP_PHRASE_FORM_MAX 128
typedef struct {
    char     form[NIMCP_PHRASE_FORM_MAX];
    uint32_t frequency;
    uint8_t  component_words;
} nimcp_phrase_entry_t;

/**
 * @brief Retrieve the top-K most frequent learned phrases (read-only).
 *
 * Thin wrapper over grounded_language_get_top_phrases() that copies entries
 * into a caller-owned buffer. Returns the number of entries written, which
 * may be less than max_phrases. Returns -1 if `brain` is NULL/invalid or
 * grounded_language is not initialised. A successful call with no qualifying
 * phrases returns 0.
 *
 * @param brain         Brain handle
 * @param out_phrases   Caller-allocated array of size >= max_phrases
 * @param max_phrases   Capacity of out_phrases
 * @return Count written (>= 0) or -1 on error.
 */
int nimcp_brain_get_top_phrases(
    nimcp_brain_t brain,
    nimcp_phrase_entry_t* out_phrases,
    uint32_t max_phrases
);

/**
 * @brief Retrieve per-modality binding counts (curriculum coverage probe).
 *
 * Thin wrapper over grounded_language_get_modality_counts(). Writes 6 values
 * indexed by gl_modality_t: [0]=VISUAL, [1]=AUDITORY, [2]=MOTOR,
 * [3]=EMOTIONAL, [4]=SPATIAL, [5]=LINGUISTIC.
 *
 * @param brain       Brain handle
 * @param out_counts  Caller-allocated uint32_t[6], zeroed on entry.
 * @return 0 on success, -1 if `brain`/`out_counts` is NULL or grounded
 *         language is not initialised.
 */
int nimcp_brain_get_modality_counts(
    nimcp_brain_t brain,
    uint32_t out_counts[6]
);

/**
 * @brief Set the SNN-language bridge blend factor at runtime.
 *
 * Fix path for collapse: cap blend at <0.9 so produce() doesn't bypass the
 * vector-template path when the bridge's spike confidence is low. Returns
 * NIMCP_ERROR_NOT_INITIALIZED if no bridge attached.
 */
nimcp_status_t nimcp_brain_set_snn_language_bridge_blend(
    nimcp_brain_t brain,
    float blend
);

/**
 * @brief Recompute the SNN-language bridge per-word_pop binding-weight L2 norm
 *        cache from the current binding state (Patch A salvage path).
 *
 * decode_spikes uses the cache to cosine-normalize word activations, removing
 * the rank-1 bias caused by curriculum-frequent words accumulating many
 * bindings. Brains that pre-date Patch A have an uninitialized cache; calling
 * this once after upgrade reseeds it without restart. Returns NIMCP_OK or
 * NIMCP_ERROR if no bridge is attached.
 */
nimcp_status_t nimcp_brain_recompute_snn_language_bridge_norms(
    nimcp_brain_t brain
);

/**
 * @brief Configure SNN-language bridge produce-time sampling (PA-6).
 *
 * @param brain        Brain handle.
 * @param temperature  0 = hard-argmax (legacy / default). >0 enables softmax
 *                     sampling over the top-K cosine-scored candidates with
 *                     this temperature (typical: 0.5–1.5; higher = flatter).
 * @param top_p        Nucleus truncation in (0,1]. 1.0 = no truncation.
 *                     Smaller values keep only the highest-probability mass.
 * @return NIMCP_OK on success, NIMCP_ERROR_INVALID for bad args, NIMCP_ERROR
 *         if no bridge attached.
 */
nimcp_status_t nimcp_brain_set_snn_language_bridge_sampling(
    nimcp_brain_t brain,
    float temperature,
    float top_p
);

/**
 * @brief Set the GloVe-aware decode blend (PA-5).
 *
 * decode_spikes ranks candidates by
 *   (1−blend) · cosine_binding + blend · cosine_glove(intent, word_emb).
 * blend = 0 (default) is binding-only; blend = 1 is embedding-only. Active
 * only when grounded_language has wired its embedding lookup down to the
 * bridge (automatic when both grounded_language_connect_embeddings() and
 * grounded_language_connect_snn_bridge() have been called).
 *
 * @return NIMCP_OK / NIMCP_ERROR_INVALID / NIMCP_ERROR.
 */
nimcp_status_t nimcp_brain_set_snn_language_bridge_glove_blend(
    nimcp_brain_t brain,
    float blend
);

/**
 * @brief PA-3: configure SNN-spike → bridge STDP routing.
 *
 * Master gate for the path that drains Broca/Wernicke spike_output through
 * the bridge each tick. Default OFF — must be explicitly enabled to avoid
 * recreating the prior accumulator-runaway sparsity collapse. tau_ms is
 * the activation decay time constant; required > 0 when enabled.
 */
nimcp_status_t nimcp_brain_set_snn_language_bridge_spike_routing(
    nimcp_brain_t brain,
    bool enabled,
    float tau_ms
);

/**
 * @brief PA-5+: select hyperbolic-distance GloVe metric.
 *
 * When enabled (and glove_blend > 0 + emb lookup attached), the embedding
 * term in decode_spikes uses 1 / (1 + d_H(query, word_emb)) where d_H is
 * the Poincaré-ball hyperbolic distance, instead of Euclidean cosine.
 * Default false reproduces PA-5 cosine behavior bit-for-bit.
 *
 * @return NIMCP_OK / NIMCP_ERROR_INVALID / NIMCP_ERROR.
 */
nimcp_status_t nimcp_brain_set_snn_language_bridge_hyperbolic_embeddings(
    nimcp_brain_t brain,
    bool enabled
);

/**
 * @brief PA-6+: select produce-time sampling mode.
 *
 * @param mode  0 = legacy auto (argmax / softmax+top-p driven by
 *              temperature; preserves PA-6 callers).
 *              1 = force softmax + nucleus top-p (PA-6).
 *              2 = quantum-Monte-Carlo MCMC sampling.
 * Modes 1 and 2 require a pre-set temperature > 0 to seed the candidate
 * distribution.
 *
 * @return NIMCP_OK / NIMCP_ERROR_INVALID / NIMCP_ERROR.
 */
nimcp_status_t nimcp_brain_set_snn_language_bridge_sampling_mode(
    nimcp_brain_t brain,
    int mode
);

/**
 * @brief TIER1-A: enable / configure beam-K decoding in produce().
 *
 * @param k  Beam width. 1 (default) preserves greedy behavior bit-for-bit.
 *           > 1 enables beam search with length-normalized cumulative
 *           log-prob ranking. Capped at 16. 0 is treated as 1.
 *
 * @return NIMCP_OK / NIMCP_ERROR.
 */
nimcp_status_t nimcp_brain_set_snn_language_bridge_beam_width(
    nimcp_brain_t brain,
    uint32_t k
);

/**
 * @brief TIER1-B: register the end-of-utterance word_pop.
 *
 * When sampled, snn_language_bridge_produce halts cleanly without
 * appending the EOS form to the output text.
 *
 * @param pop  Word_pop index, or UINT32_MAX to disable EOS.
 * @return NIMCP_OK / NIMCP_ERROR.
 */
nimcp_status_t nimcp_brain_set_snn_language_bridge_eos_word_pop(
    nimcp_brain_t brain,
    uint32_t pop
);

/**
 * @brief TIER1-C: configure the n-gram repetition penalty.
 *
 * Per produce step, every candidate whose word_pop appears in the last
 * `window` picks has its score multiplied by (1 - penalty) per match.
 * penalty = 0 (default) disables.
 *
 * @param penalty  In [0, 1]. Clamped if out of range.
 * @param window   Look-back length. 0 falls back to 3 when penalty > 0.
 * @return NIMCP_OK / NIMCP_ERROR_INVALID / NIMCP_ERROR.
 */
nimcp_status_t nimcp_brain_set_snn_language_bridge_repetition_penalty(
    nimcp_brain_t brain,
    float penalty,
    uint32_t window
);

/**
 * @brief Tier-4 #15: copy the entire SNN-language bridge config out for
 *        operator introspection.
 *
 * Every PA/MQ knob (blend, sampling, glove_blend, autoregressive, spike-
 * routing, hyperbolic, sampling_mode) has a setter; this is the lone
 * consolidated reader. Useful for ops/dashboards that need to answer
 * "what's currently configured?" without parsing setter call history.
 *
 * @return NIMCP_OK on success, NIMCP_ERROR if no bridge attached.
 */
nimcp_status_t nimcp_brain_get_snn_language_bridge_config(
    nimcp_brain_t brain,
    snn_lang_config_t* out
);

/**
 * @brief Tier-4 #17: explicitly seed the SNN-language bridge sampling RNG
 *        for deterministic test runs.
 *
 * The bridge's xorshift64* RNG is seeded at create() with
 * (time XOR pointer-mix). This wrapper overrides that seed so PA-6 / MQ-A
 * sampling tests can pin outcomes. seed=0 is silently remapped to 1
 * (xorshift64 collapses to permanent zero from a zero state).
 *
 * @return NIMCP_OK on success, NIMCP_ERROR if no bridge attached.
 */
nimcp_status_t nimcp_brain_set_snn_language_bridge_rng_seed(
    nimcp_brain_t brain,
    uint64_t seed
);

/**
 * @brief PA-4: train the SNN language bridge on a single (prev, next) bigram
 *        via a next-token contrastive update.
 *
 * Returns NIMCP_OK on success (update applied), NIMCP_ERROR if the bridge
 * is unattached or the call was a cold-start no-op (prev has no encoding
 * yet).
 */
nimcp_status_t nimcp_brain_learn_next_token_pair(
    nimcp_brain_t brain,
    const char* prev_word,
    const char* next_word,
    float lr
);

/**
 * @brief PA-4+: Riemannian / sigmoid-reparameterized next-token training
 *        on a single (prev, next) bigram. Same contract as the flat path
 *        but binding updates use a Fisher-preconditioned step that damps
 *        automatically near the [0, 1] boundaries.
 *
 * Returns NIMCP_OK on success (update applied), NIMCP_ERROR if the bridge
 * is unattached or the call was a cold-start no-op.
 */
nimcp_status_t nimcp_brain_learn_next_token_pair_riemannian(
    nimcp_brain_t brain,
    const char* prev_word,
    const char* next_word,
    float lr
);

/**
 * @brief PA-4: walk the bigrams of `text` and apply a next-token update
 *        for each pair. Returns count via *out_count (or NULL).
 */
nimcp_status_t nimcp_brain_learn_text_bigrams(
    nimcp_brain_t brain,
    const char* text,
    float lr,
    int* out_count
);

/**
 * @brief TA-4: toggle trigram next-token learning.
 *
 * When enabled, nimcp_brain_learn_text_bigrams also walks every
 * (w_t, w_{t+1}) → w_{t+2} trigram at half the bigram lr. Default OFF —
 * preserves PA-4 behavior bit-for-bit. Runtime-only flag; not persisted
 * across saves.
 *
 * Returns NIMCP_OK on success, NIMCP_ERROR if no SNN-language bridge is
 * attached.
 */
nimcp_status_t nimcp_brain_set_trigram_learning_enabled(
    nimcp_brain_t brain,
    bool enabled
);

/** Margin gate for the SNN language bridge's next-token LTD. See
 *  snn_lang_config_t::ltd_margin in the bridge header for the rule.
 *  margin=1.0 reproduces legacy unconditional LTD; ≥1.5 (default)
 *  protects strong-but-unrelated bindings from indiscriminate
 *  suppression. Clamped to [1.0, 100.0]; out-of-range or non-finite
 *  inputs return NIMCP_ERROR without mutating bridge state.
 *
 *  Runtime-only; not persisted across saves. */
nimcp_status_t nimcp_brain_set_ltd_margin(
    nimcp_brain_t brain,
    float margin
);

/** Re-randomize every existing bridge binding weight to uniform(w_min, w_max).
 *  See snn_language_bridge_reset_weights in the bridge header for the
 *  rationale (breaks rank-1 / homogenized collapse).
 *
 *  Writes the number of bindings reset to *out_count (may be NULL).
 *  Returns NIMCP_ERROR if no SNN-language bridge is attached or args are
 *  out of range. */
nimcp_status_t nimcp_brain_reset_lang_bridge_weights(
    nimcp_brain_t brain,
    float w_min,
    float w_max,
    int64_t* out_count
);

/** Reset every lexicon entry's distributional embedding (context_vector).
 *  When the distributional channel has converged across the vocab so that
 *  every word's context_vector points the same direction, comprehend's
 *  L2-normalized output collapses to a single eigenvector regardless of
 *  input. This reset breaks that homogenization. See
 *  grounded_language_reset_lexicon_distributional for the rule.
 *
 *  zero_and_mark_uninit=true: zero each vector + flag uninitialized (clean
 *    reset; vectors re-learn from next comprehend forward).
 *  zero_and_mark_uninit=false: re-randomize each vector uniformly in
 *    [-jitter, +jitter] and keep initialized=true (preserves "this word
 *    has been seen in some context" without preserving the direction). */
nimcp_status_t nimcp_brain_reset_lexicon_distributional(
    nimcp_brain_t brain,
    bool zero_and_mark_uninit,
    float jitter,
    int64_t* out_count
);

/* Reset concept grounding for a clean re-grounding pass: clears all lexicon
 * concept bindings AND wipes the brain's semantic_memory concept store (frees
 * capacity). Use after the grounding-feature fix to recover from concept
 * collapse. out_cleared receives the number of bindings cleared. */
nimcp_status_t nimcp_brain_reset_concept_grounding(
    nimcp_brain_t brain,
    int64_t* out_cleared
);

/* Classify a text via the toxicity classifier and return the scores. All
 * out-pointers are optional (pass NULL to skip a field). category buffer is
 * populated with the matched-category string (cleared to empty on no match).
 * Returns NIMCP_OK on success; NIMCP_ERROR if the classifier is not
 * attached or classify failed. (2026-05-21) */
nimcp_status_t nimcp_brain_classify_toxicity(
    nimcp_brain_t brain,
    const char* text,
    float* out_harm,
    float* out_fairness,
    float* out_anti_toxic,
    int* out_would_block,
    char* out_category,
    size_t out_category_len
);

/* Audit fix — campaign feature flag setters callable from the daemon RPC
 * surface + Python bindings. All have default-OFF semantics; calling with
 * enabled=false reverts to legacy behavior. */

/** TA-3: dopamine-modulated STDP on the SNN language bridge. */
nimcp_status_t nimcp_brain_set_da_modulation_enabled(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_set_da_modulation_gain(nimcp_brain_t brain, float gain);

/** CSTDP: comprehend-driven scoped STDP on the SNN language bridge.
 * Default OFF. When enabled, every comprehend call reinforces existing
 * strong concept↔word bindings via inline STDP (production-direction
 * order, scoped to the bindings actually touched by the input — bounded
 * 256-pair touched-set, no full 1.6M-binding hash walk per call). */
nimcp_status_t nimcp_brain_set_comprehend_stdp_enabled(nimcp_brain_t brain, bool enabled);

/** Echo-correct: supervised "produce this word" learning loop.
 *
 * Comprehends parent_text to get the semantic vector, then strengthens
 * each (active_concept_pop → target_word_pop) binding in the SNN
 * language bridge. This is the production-side training signal the
 * bridge has been missing — without it the produce path samples from
 * uniform-random weights across the entire 29K-word lexicon.
 *
 * Use the trainer pattern: every Nth narration, after the brain
 * comprehends "Look! A ball.", call:
 *   nimcp_brain_echo_and_correct(brain, "Look! A ball.", "ball", 1.0f, NULL)
 *
 * The target_word must already be registered in the bridge (via the
 * normal lexicon mirror path); unfamiliar words return
 * NIMCP_ERROR_OPERATION_FAILED.
 *
 * lr_scale multiplies the bridge's stdp_learning_rate. 1.0 = use the
 * default; lower values for cautious imprinting, higher for fast
 * supervised correction. DA modulation rides on top when enabled.
 *
 * If out_pairs_strengthened is non-NULL, it gets the count of
 * concept_pop bindings actually updated (== number of concept dims
 * with intent[i] >= comprehend_stdp_min_activation). */
nimcp_status_t nimcp_brain_echo_and_correct(
    nimcp_brain_t brain,
    const char* parent_text,
    const char* target_word,
    float lr_scale,
    int* out_pairs_strengthened);

/** Phase 2A: multi-region production cascade.
 *
 * Reframes language production as a coordinated multi-region cascade:
 * drive (hypothalamus) + goal (PFC + WM) + listener (ToM) + episodic
 * retrieval (hippocampus) + content composition + lexical (bridge) +
 * (deferred) syntactic (Broca) + phonological + motor.
 *
 * Each stage reads real state from its source module(s) when attached;
 * stages no-op gracefully when their source isn't present. The cascade
 * shapes a content_intent vector that drives the bridge — this is what
 * makes it different from grounded_respond, which feeds the bridge the
 * comprehend vector unmodified.
 *
 * Phase 2A skeleton: intent-formation stages (1-5) run with simplified
 * combine logic; output stages 7-9 are deferred until the GL→Broca
 * lexicon mirror lands.
 *
 * out_utterance / out_text_max — caller buffer for the produced text.
 *   Truncated if too small. Pass NULL/0 to skip.
 * out_word_count — # of words in produced utterance, optional.
 * out_confidence — cascade confidence [0,1], optional.
 *
 * Returns NIMCP_OK on success, NIMCP_ERROR on fatal cascade failure.
 * Per-stage failures are non-fatal — they're rolled into a lower
 * cascade confidence rather than aborting. */
nimcp_status_t nimcp_brain_produce_cascade(
    nimcp_brain_t brain,
    const char* prompt_or_null,
    char* out_utterance,
    uint32_t out_text_max,
    uint32_t* out_word_count,
    float* out_confidence);

/** Recurrent / biological-fidelity variant of produce_cascade.
 *
 *  Iterates the full 15-stage cascade up to `max_iters` times, checking
 *  convergence between iterations (utterance byte-stability + |Δself_match|
 *  <= self_match_eps). Mimics the settling dynamics of real cortex: each
 *  iteration's stage_self_train STDP shifts the bridge slightly, the next
 *  iteration reads the shifted bridge, the system settles toward a coherent
 *  attractor.
 *
 *  max_iters=0 uses the default of 8; self_match_eps<0 uses 0.01.
 *
 *  out_settling_steps receives the number of iterations actually taken
 *  before convergence (or max_iters if no convergence). Pass NULL to skip.
 *
 *  This is Slice 1 of the recurrent-language-architecture rewrite (see
 *  docs/claude/recurrent-language-architecture.md). Subsequent slices
 *  wire bidirectional Wernicke↔Broca within each iteration, FEP
 *  prediction-error hooks, lateral inhibition, etc. */
nimcp_status_t nimcp_brain_produce_cascade_recurrent(
    nimcp_brain_t brain,
    const char* prompt_or_null,
    uint32_t max_iters,
    float self_match_eps,
    char* out_utterance,
    uint32_t out_text_max,
    uint32_t* out_word_count,
    float* out_confidence,
    uint32_t* out_settling_steps);

/** TA-5: reconsolidation-on-contradiction. */
nimcp_status_t nimcp_brain_set_reconsolidation_enabled(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_set_reconsolidation_decay(nimcp_brain_t brain, float decay);

/** Slice 4 (recurrent-language-architecture) — lateral-inhibition in lexical
 *  selection. Default OFF preserves bit-for-bit legacy produce / cascade.
 *  When enabled, the bridge's per-word decode swaps the one-shot cosine
 *  argmax for a recurrent competition over the top-K candidates:
 *
 *    for k:  new_a[k] = sigmoid(a[k]*gain_self - sum_{j!=k} a[j]*gain_inh)
 *
 *  for `micro_steps` iterations. Re-rank by post-settling activations.
 *  Implements competitive selection in the cohort-model (Marslen-Wilson
 *  1987) / interactive-activation (McClelland 1981) tradition.
 *
 *  All three runtime-only — caller re-applies after each brain load.
 *  See docs/claude/recurrent-language-architecture.md for the design. */
nimcp_status_t nimcp_brain_set_lateral_inhibition_enabled(nimcp_brain_t brain,
                                                            bool enabled);
nimcp_status_t nimcp_brain_get_lateral_inhibition_enabled(nimcp_brain_t brain,
                                                            bool* out_enabled);
nimcp_status_t nimcp_brain_set_lateral_inhibition_params(nimcp_brain_t brain,
                                                          float gain_self,
                                                          float gain_inhibit,
                                                          uint32_t micro_steps);
nimcp_status_t nimcp_brain_get_lateral_inhibition_params(nimcp_brain_t brain,
                                                          float* out_gain_self,
                                                          float* out_gain_inhibit,
                                                          uint32_t* out_micro_steps);

/** TB-6: sentence-boundary segmentation. */
nimcp_status_t nimcp_brain_set_sentence_segmentation_enabled(nimcp_brain_t brain, bool enabled);

/** TB-7: produce length control. min/max=0 each disable that side. */
nimcp_status_t nimcp_brain_set_length_control(nimcp_brain_t brain,
                                                uint32_t min_words, uint32_t max_words);

/** TB-9: speech-act intent classification. */
nimcp_status_t nimcp_brain_set_speech_act_classification_enabled(nimcp_brain_t brain, bool enabled);

/** TB-10: topic-shift detection + tunables (enabled, threshold, min_turns). */
nimcp_status_t nimcp_brain_set_topic_shift_enabled(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_set_topic_shift_threshold(nimcp_brain_t brain, float threshold);
nimcp_status_t nimcp_brain_set_topic_shift_min_turns(nimcp_brain_t brain, uint32_t min_turns);

/** NLP-2: coreference / definite-NP resolution toggle. Default OFF.
 *  Runtime-only (re-apply after load) unless persisted via the LANC block.
 *  Disabling clears the per-GL mention ring so re-enable starts fresh. */
nimcp_status_t nimcp_brain_set_coref_resolution_enabled(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_get_coref_resolution_enabled(nimcp_brain_t brain, bool* out_enabled);

/** NLP-1: subword / morphological OOV fallback toggle. Default OFF.
 *  Enabling attaches the brain-native BPE tokenizer (brain->brain_tokenizer)
 *  to grounded_language when present — without a tokenizer the fallback
 *  no-ops gracefully (attempts stay 0). Runtime-only unless persisted via
 *  the LANC block. */
nimcp_status_t nimcp_brain_set_subword_oov_fallback_enabled(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_get_subword_oov_fallback_enabled(nimcp_brain_t brain, bool* out_enabled);

/** Tier 1 follow-up: gl-side autoregressive produce. Default OFF. When ON,
 *  grounded_language_produce conditions each emitted word on a running sum of
 *  prior words' context vectors (continuation-coherence bonus) instead of a
 *  fixed-intent rerank. Runtime toggle; persisted via the LANC block. */
nimcp_status_t nimcp_brain_set_autoregressive_produce(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_get_autoregressive_produce(nimcp_brain_t brain, bool* out_enabled);

/** Tier 2: produce-side pronominalization (recency anaphora). Default OFF.
 *  When ON, a re-mentioned non-person noun (within a recency window, across a
 *  verb) becomes it/they/them so replies don't repeat the noun. Person nouns
 *  guarded out. Runtime toggle; persisted via the LANC block. */
nimcp_status_t nimcp_brain_set_produce_pronominalize(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_get_produce_pronominalize(nimcp_brain_t brain, bool* out_enabled);

/** Tier 2: discourse-seeded autoregressive produce (cross-turn coherence).
 *  Default OFF. Only effective when autoregressive_produce is also ON and
 *  stage >= 2 — primes the AR emitted-context vector with the decayed
 *  most-recent discourse-turn vector so the opening words continue the topic
 *  of what was just said. Runtime toggle; persisted via the LANC block. */
nimcp_status_t nimcp_brain_set_produce_discourse_seed(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_get_produce_discourse_seed(nimcp_brain_t brain, bool* out_enabled);

/** FND-1: SVO clause/argument frame in produce (foundation — real predicate +
 *  agent/subject + patient/object structure vs. a reranked content-word bag).
 *  Default OFF; effective at stage >= 2, falls back to greedy emit on weak
 *  signal. Runtime toggle; persisted via the LANC block + runtime config. */
nimcp_status_t nimcp_brain_set_produce_clause_frame(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_get_produce_clause_frame(nimcp_brain_t brain, bool* out_enabled);

/** Increment-1 (2026-06-02): SNN-as-generator A/B switch. When ON, produce
 *  sources candidate words from the SNN bridge's Broca spike cache, with the
 *  lexicon producer as permanent fallback. Default OFF. Runtime-only. */
nimcp_status_t nimcp_brain_set_produce_via_snn(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_get_produce_via_snn(nimcp_brain_t brain, bool* out_enabled);

/** Metacognitive produce-floor modulation (2026-06-02): when ON, the cascade
 *  shifts the produce confidence floor from world-model surprise / wellbeing
 *  distress / introspection confidence (modulatory cognitive modules gate
 *  willingness-to-assert rather than inject content). Default OFF. Runtime-only. */
nimcp_status_t nimcp_brain_set_metacog_gates_produce(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_get_metacog_gates_produce(nimcp_brain_t brain, bool* out_enabled);

/** Phase-2 step 3 (2026-06-02): warm-start the SNN concept→word projection from
 *  the lexicon — sets each projection synapse weight to k*binding_strength.
 *  No-op-ish (returns 0 updated) when the projection wasn't built
 *  (NIMCP_LANG_PROJECTION off) or the lexicon is empty. out_updated receives
 *  the #synapses set. Call after the lexicon is populated (resume / training). */
nimcp_status_t nimcp_brain_warmstart_lang_projection(nimcp_brain_t brain, float k,
                                                     int* out_updated);

/** Build the SNN concept→word projection TOPOLOGY across all lexicon bindings
 *  (2026-06-09) — the missing "coverage half". Reopens the finalized Broca CSR,
 *  wires every concept↔word ensemble pair, re-finalizes. Without it Broca has no
 *  concept input and the SNN generates nothing. Run AFTER the lexicon is loaded;
 *  MUST run with SNN stepping paused (daemon serializes RPCs with learn_vector).
 *  w_init seeds the new synapse weights; warmstart/learn_pair then tune them.
 *  out_wired receives the #pairs wired. */
nimcp_status_t nimcp_brain_build_lang_projection(nimcp_brain_t brain, float w_init,
                                                 int* out_wired);

/** Diagnostic: concept→word projection round-trip selectivity. Seeds each of the
 *  first n_probe lexicon bindings' concept alone, steps the SNN n_steps at
 *  inject, and writes the decode rank of each binding's own word pop (0=top-1,
 *  -1=missing) into out_ranks (up to max_out); *out_n receives the count. */
nimcp_status_t nimcp_brain_snn_probe_selectivity(nimcp_brain_t brain,
                                                 uint32_t n_probe, uint32_t n_steps,
                                                 float inject, int* out_ranks,
                                                 uint32_t max_out, uint32_t* out_n);

/** Phase-2 produce-time SNN generation step (2026-06-03): seed the intent's
 *  concepts into Wernicke, step the SNN n_steps, leave Broca's spikes in the
 *  decode cache for produce_via_snn to read. The step that makes the SNN
 *  GENERATE from intent. COST: steps the full network — small n_steps only.
 *  Returns NIMCP_ERROR (caller falls back to lexicon) when projection/pops
 *  absent or no concept seed. */
nimcp_status_t nimcp_brain_snn_language_generate_step(nimcp_brain_t brain,
                                                      const float* intent,
                                                      uint32_t dim,
                                                      uint32_t n_steps);

/** Produce-score rebalance: weight on the distributional (context-vector)
 *  term vs the concept-binding term in produce word scoring
 *  (raw = w*distributional + (1-w)*concept). Default 0.4 = historical split;
 *  raise toward ~0.7 so intent-correct concrete words outrank broadly-
 *  concept-bound abstract words. Clamped [0,1]. Persisted via LANC + config. */
nimcp_status_t nimcp_brain_set_produce_distributional_weight(nimcp_brain_t brain, float weight);
nimcp_status_t nimcp_brain_get_produce_distributional_weight(nimcp_brain_t brain, float* out_weight);

/** Produce-score frequency penalty (2026-06-01): IDF-style damping of
 *  high-frequency words in produce scoring — raw *= 1/(1 + p*log1p(freq)).
 *  Default 0.0 = OFF. Raise (~0.1-0.3) to stop distributionally-central
 *  technical vocab monopolizing produce filler positions. Runtime-only
 *  (applied from JSON at start; not LANC-persisted). penalty<0 clamped to 0. */
nimcp_status_t nimcp_brain_set_produce_frequency_penalty(nimcp_brain_t brain, float penalty);
nimcp_status_t nimcp_brain_get_produce_frequency_penalty(nimcp_brain_t brain, float* out_penalty);

/** T3-1 (2026-05-28): givenness-driven definiteness — on a NOUN re-mention
 *  that didn't pronominalize (person nouns, unanchored slots), swap the
 *  preceding "a"/"an" to "the". Default OFF; stage-gated >=2 inside the
 *  helper. Runtime toggle; persisted via the LANC block + runtime config. */
nimcp_status_t nimcp_brain_set_produce_givenness_definite(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_get_produce_givenness_definite(nimcp_brain_t brain, bool* out_enabled);

/** T3-2 (2026-05-29): cohesive conjunction insertion — when the cascade's
 *  utterance contains an adjacent SVO pair without an existing connective,
 *  insert "and" before the second clause. Default ON; stage-gated >=2 inside
 *  the helper. Runtime toggle; persisted via the LANC block + runtime config. */
nimcp_status_t nimcp_brain_set_produce_t3_conjunction(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_get_produce_t3_conjunction(nimcp_brain_t brain, bool* out_enabled);

/** TF-2 (2026-05-28): Tier-feedback plasticity flags.
 *
 *  produce_corrector_feedback_enabled is the master switch. Default OFF —
 *  TF-1 still captures + counts deltas, but no plasticity fires.
 *  tf_enabled_correctors is a bitmask of source bits (see GL_TF_BIT_*).
 *  The three lrs clamp at per-path maxima (0.05 / 0.1 / 0.1). Bridge STDP
 *  default 0.0 even with master on — TF-5 walkthrough flips it.
 *  All persisted in the LANC block + runtime config JSON. */
nimcp_status_t nimcp_brain_set_produce_corrector_feedback_enabled(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_get_produce_corrector_feedback_enabled(nimcp_brain_t brain, bool* out_enabled);
nimcp_status_t nimcp_brain_set_tf_enabled_correctors(nimcp_brain_t brain, uint16_t mask);
nimcp_status_t nimcp_brain_get_tf_enabled_correctors(nimcp_brain_t brain, uint16_t* out_mask);
nimcp_status_t nimcp_brain_set_tf_lr_trigram(nimcp_brain_t brain, float lr);
nimcp_status_t nimcp_brain_get_tf_lr_trigram(nimcp_brain_t brain, float* out_lr);
nimcp_status_t nimcp_brain_set_tf_lr_distrib(nimcp_brain_t brain, float lr);
nimcp_status_t nimcp_brain_get_tf_lr_distrib(nimcp_brain_t brain, float* out_lr);
nimcp_status_t nimcp_brain_set_tf_lr_bridge_stdp(nimcp_brain_t brain, float lr);
nimcp_status_t nimcp_brain_get_tf_lr_bridge_stdp(nimcp_brain_t brain, float* out_lr);

/** Audit-2 B13: dialect / accent conditioning. NULL or empty clears.
 *  Truncates to GL_MAX_DIALECT_LEN-1 chars internally. */
nimcp_status_t nimcp_brain_set_dialect(nimcp_brain_t brain, const char* dialect);

/** Wave 2 Item #10 — cascade Stage 14 (self-train) control surface.
 *  Default OFF. Flipping `enabled=true` also installs default tunables
 *  (0.05 alpha, 1.0 lr_scale) if the caller has not set them yet. */
nimcp_status_t nimcp_brain_set_cascade_self_train_enabled(nimcp_brain_t brain,
                                                            bool enabled);
nimcp_status_t nimcp_brain_get_cascade_self_train_enabled(nimcp_brain_t brain,
                                                            bool* out_enabled);
/** alpha in [0,1] (0 freezes baseline, 1 = no smoothing), lr_scale in [0,10]
 *  (0 disables plasticity but EMA still updates). NaN/Inf/negative coerce
 *  to defaults. */
nimcp_status_t nimcp_brain_set_cascade_self_train_tunables(nimcp_brain_t brain,
                                                             float alpha,
                                                             float lr_scale);
/** Snapshot all four self-train fields. Any out-pointer may be NULL. */
nimcp_status_t nimcp_brain_get_cascade_self_train_state(nimcp_brain_t brain,
                                                          bool*  out_enabled,
                                                          float* out_baseline,
                                                          float* out_alpha,
                                                          float* out_lr_scale);

/** Slice D — external reward signal for cascade self-train gating.
 *  Called by the caregiver-critic / RL pipeline on every external reward
 *  event. Clamps to [-1.0, +1.0]; stamps a monotonic_us timestamp so
 *  cascade_stage_self_train can detect staleness (default TTL 5s).
 *  Returns 0 on success, -1 on invalid brain or non-finite reward. */
int nimcp_brain_set_last_external_reward(nimcp_brain_t brain, float reward);

/** Slice C — drive reward-modulated plasticity for the brain. Thin wrapper
 *  around the internal brain_apply_reward_learning(). The Slice C caregiver-
 *  critic pipeline calls this on every brain production. Reward is clamped
 *  to [-1, +1]. Negative reward drives anti-Hebbian (LTD) plasticity through
 *  the three-factor STDP path (see Slice F). Returns 0 on success, -1 on
 *  invalid brain or non-finite reward. */
int nimcp_brain_apply_reward_learning(nimcp_brain_t brain, float reward);

/** Slice D — configure the reward gating thresholds. reward_threshold in
 *  (0, 1]; reward_ttl_us > 0. Either argument zero/negative leaves the
 *  brain's current value unchanged (partial update). */
nimcp_status_t nimcp_brain_set_cascade_self_train_reward_gating(
    nimcp_brain_t brain,
    float reward_threshold,
    uint64_t reward_ttl_us);

/** Slice D — snapshot the three self-train gating counters surfaced in
 *  stats.cascade.self_train of lang_status. Any out-pointer may be NULL. */
nimcp_status_t nimcp_brain_get_cascade_self_train_gate_counters(
    nimcp_brain_t brain,
    uint64_t* out_skipped_stale,
    uint64_t* out_skipped_below_threshold,
    uint64_t* out_fired);

/** Slice E — developmental-stage scaffolding.
 *
 *  set_active_stage() advances the brain's current_stage field. The
 *  communication cascade reads this in cascade_stage_motor and enforces
 *  the stage's min/max word-count window + grammar template mask from
 *  the static table at
 *  include/cognitive/grounded_language/nimcp_stage_table.h.
 *
 *  set_vocab_mask_for_stage() installs / refreshes the grounded_language
 *  visibility mask so production only retrieves entries within the
 *  stage's max_visible_vocab cap. Pass the same stage as set_active_stage
 *  unless you specifically want a mask divergent from the cascade caps
 *  (mainly useful for ablation tests).
 *
 *  get_active_stage() / get_stage_constraints() snapshot current state.
 *
 *  Defaults: current_stage starts at 0 (single-word noun productions)
 *  unless set otherwise. The mask is uninstalled (every vocab entry
 *  visible) until set_vocab_mask_for_stage() is called. */
int nimcp_brain_set_active_stage(nimcp_brain_t brain, uint32_t stage);
int nimcp_brain_get_active_stage(nimcp_brain_t brain, uint32_t* out_stage);
int nimcp_brain_set_vocab_mask_for_stage(nimcp_brain_t brain, uint32_t stage);
int nimcp_brain_clear_vocab_mask(nimcp_brain_t brain);
/** Snapshot the stage table row for the requested stage. Any out-pointer
 *  may be NULL. Out-of-range stage clamps to the highest defined row. */
int nimcp_brain_get_stage_constraints(nimcp_brain_t brain,
                                       uint32_t stage,
                                       size_t* out_max_visible_vocab,
                                       uint32_t* out_min_produce_words,
                                       uint32_t* out_max_produce_words,
                                       uint32_t* out_allowed_grammar_mask);

/** Batch K — cascade lifetime telemetry counters. Snapshot + reset.
 *  `out` is zeroed and populated on success. Reset zeros every counter
 *  atomically (per-field; not transactional across fields). */
struct nimcp_cascade_counters; /* opaque forward decl — full type in
                                  include/language/nimcp_communication_cascade.h */
nimcp_status_t nimcp_brain_get_cascade_counters(
    nimcp_brain_t brain,
    struct nimcp_cascade_counters* out);
nimcp_status_t nimcp_brain_reset_cascade_counters(nimcp_brain_t brain);

/** Slice 3 — recurrent-cascade FEP prediction-error trajectory.
 *  Snapshots the most-recent run of nimcp_brain_produce_cascade_recurrent.
 *  Zero-init when the recurrent loop has never been invoked. Single-
 *  writer / multi-reader; lock-free. */
struct nimcp_cascade_fep_metrics; /* opaque forward decl — full type in
                                     include/language/nimcp_communication_cascade.h */
nimcp_status_t nimcp_brain_get_cascade_fep_metrics(
    nimcp_brain_t brain,
    struct nimcp_cascade_fep_metrics* out);

/** Audit Cat A #1 — toggle the cascade orchestrator path inside
 *  nimcp_brain_grounded_respond. Default OFF (legacy bridge passthrough).
 *  When ON, respond runs the 15-stage cascade and emits state->utterance
 *  (with speech_repair, self_comp validation, etc). Latency cost: ~10x
 *  vs the bridge-only path; opt-in based on the quality/latency tradeoff. */
nimcp_status_t nimcp_brain_set_respond_via_cascade(nimcp_brain_t brain,
                                                     bool enabled);
nimcp_status_t nimcp_brain_get_respond_via_cascade(nimcp_brain_t brain,
                                                     bool* out_enabled);

/** Tier 1 Step E — toggle reasoning-conclusion blending into the cascade's
 *  content intent. When ON (and the reasoning engine is enabled), respond
 *  invokes the reasoning engine once per prompt and biases production toward
 *  the conclusion. Default OFF: the reasoning invoke adds latency, so it's
 *  opt-in. No effect unless respond_via_cascade is also ON. */
nimcp_status_t nimcp_brain_set_reason_in_content(nimcp_brain_t brain,
                                                   bool enabled);
nimcp_status_t nimcp_brain_get_reason_in_content(nimcp_brain_t brain,
                                                   bool* out_enabled);

/** Non-SNN networks → language content_intent (2026-06-05). Per the directive
 *  that every network except the FNO feeds the SNN language generator, these
 *  four default-OFF gates each route one network into the cascade content
 *  stage (which seeds the Wernicke concept pop):
 *   - ann_in_content: blend the main ANN's last decision output_vector
 *   - lnn_in_content: blend the LNN liquid (temporal-context) state
 *   - cnn_in_content: blend the visual cortex's cached feature embedding
 *                     (no-op until vision is actually driven)
 *   - hnn_in_content: HNN has no semantic vector — its energy-deviation
 *                     modulates the produce confidence floor instead
 *  All default OFF; no effect unless respond_via_cascade is also ON. */
nimcp_status_t nimcp_brain_set_ann_in_content(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_get_ann_in_content(nimcp_brain_t brain, bool* out_enabled);
nimcp_status_t nimcp_brain_set_lnn_in_content(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_get_lnn_in_content(nimcp_brain_t brain, bool* out_enabled);
nimcp_status_t nimcp_brain_set_hnn_in_content(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_get_hnn_in_content(nimcp_brain_t brain, bool* out_enabled);
nimcp_status_t nimcp_brain_set_cnn_in_content(nimcp_brain_t brain, bool enabled);
nimcp_status_t nimcp_brain_get_cnn_in_content(nimcp_brain_t brain, bool* out_enabled);

/** Slice 6 — thalamic gating of cascade-stage bandwidth.
 *
 *  Per-stage scalar gain control on the cascade modeled on the pulvinar
 *  nucleus's role as central relay + gain controller. When enabled, each
 *  cascade stage's scaleable contributions (content_intent magnitude,
 *  lexical fluency, prosodic intensity, self-train lr_scale) are
 *  multiplied by a per-stage gate in [0, 1] derived from the brain's
 *  current arousal (NE) + attention (ACh) state. Default OFF preserves
 *  bit-for-bit legacy behavior.
 *
 *  Manual override: set stage weight to clear (weight < 0) or any
 *  specific value in [0, 1] to lock that stage's gate at the value
 *  until the next clear. Useful for ablation experiments.
 *
 *  Stage index is the bit position of cascade_stage_mask_t — see the
 *  nimcp_cascade_stage_idx_t enum in
 *  include/language/nimcp_communication_cascade.h. */
nimcp_status_t nimcp_brain_set_thalamic_gate_enabled(nimcp_brain_t brain,
                                                      bool enabled);
nimcp_status_t nimcp_brain_get_thalamic_gate_enabled(nimcp_brain_t brain,
                                                      bool* out_enabled);
/** weight < 0 clears the manual override (returns to auto-derived).
 *  weight in [0, 1] sets and locks. NaN/Inf clear. */
nimcp_status_t nimcp_brain_set_thalamic_gate_for_stage(nimcp_brain_t brain,
                                                        uint32_t stage_idx,
                                                        float weight);
/** Snapshot of all 15 per-stage gate weights + override flags. Any of
 *  out_weights / out_overrides may be NULL. Both arrays must have at
 *  least max_count entries. count_out (NULL-tolerant) is set to the
 *  number of entries populated. */
nimcp_status_t nimcp_brain_get_thalamic_gates(nimcp_brain_t brain,
                                                float* out_weights,
                                                bool*  out_overrides,
                                                uint32_t max_count,
                                                uint32_t* count_out);

/** Slice 5 — phonological-loop working memory buffer.
 *
 *  Baddeley's working-memory model: a short-term phonological store
 *  (~2 seconds of speech) + an articulatory rehearsal mechanism. When
 *  enabled, the recurrent cascade incrementally refines the utterance
 *  across iterations instead of rebuilding it from scratch each pass:
 *    - Iteration start: existing traces decay by `decay_rate`; traces
 *      below 0.05 are evicted.
 *    - After stage_lexical: produced words merge into the buffer
 *      (existing → refresh trace to 1.0, new → append, capped).
 *    - stage_syntactic / stage_self_comp read the buffer's surface
 *      form (words with trace >= 0.3) as the "intended utterance".
 *
 *  Default OFF — when not enabled, recurrent cascade behaves
 *  byte-identically to Slice 1+2. Lifetime owned by the brain. */
nimcp_status_t nimcp_brain_set_phonological_loop_enabled(nimcp_brain_t brain,
                                                          bool enabled);
/** Clamps to [0.0, 0.5]. NaN/Inf coerce to default (0.15). */
nimcp_status_t nimcp_brain_set_phonological_loop_decay(nimcp_brain_t brain,
                                                        float decay);
/** S5-H5: surface-render threshold. Words with trace ≥ this value count
 *  as "on the surface form" (visible via get_phonological_loop_state +
 *  consumed by stage_syntactic / stage_self_comp). Clamped to [0, 1];
 *  NaN/Inf or <=0 coerce to default (0.3). Lower values reveal traces
 *  in the silent decay band [0.05, default_threshold). */
nimcp_status_t nimcp_brain_set_phonological_loop_threshold(nimcp_brain_t brain,
                                                            float threshold);
/** Full reset — drops every word + trace, clears the surface buffer. */
nimcp_status_t nimcp_brain_clear_phonological_loop(nimcp_brain_t brain);

/** Read-only diagnostic snapshot. out_buffer receives the surface form
 *  (words with trace >= 0.3, space-separated, NUL-terminated; truncated
 *  to out_size-1). out_trace_count receives the count of currently-live
 *  traces (regardless of threshold). Any out-pointer may be NULL.
 *  Returns NIMCP_OK even when the loop is disabled (out_buffer = empty,
 *  out_trace_count = 0). */
nimcp_status_t nimcp_brain_get_phonological_loop_state(
    nimcp_brain_t brain,
    char* out_buffer,
    uint32_t out_size,
    uint32_t* out_trace_count);

/** Full diag struct — covers what the lightweight getter omits
 *  (decay_rate, enabled, avg trace strength). Forward-declared to keep
 *  this header light; the full type lives in the include/language/
 *  phonological-loop header. */
typedef struct nimcp_phonological_loop_diag {
    bool     enabled;
    uint32_t buffer_len;          /* strlen of current surface buffer */
    uint32_t trace_count;         /* number of currently-live traces */
    uint32_t trace_capacity;      /* slot capacity */
    uint32_t max_words;           /* configured cap */
    float    decay_rate;          /* configured per-iteration decay */
    float    avg_trace_strength;  /* mean of all live traces (0 if empty) */
    uint64_t last_refresh_ms;     /* monotonic ms of last decay/merge */
    /* S5-H5 (2026-05-19): operator-controlled surface threshold. Words
     * with trace >= this value count as on the surface form. */
    float    surface_threshold;
} nimcp_phonological_loop_diag_t;

nimcp_status_t nimcp_brain_get_phonological_loop_diag(
    nimcp_brain_t brain,
    nimcp_phonological_loop_diag_t* out);

/** Slice 7 — cerebellar prediction-correction in motor + prosody stages.
 *
 *  Wires the existing cerebellum adapter (cerebellum_predict_outcome /
 *  cerebellum_update_forward_model / cerebellum_broadcast_error from
 *  include/core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h) into
 *  cascade_stage_motor and cascade_stage_prosody. Each stage builds an
 *  8D feature vector, calls the cerebellum to predict the next motor /
 *  prosody pattern, runs its existing logic, then closes the loop by
 *  feeding the realised "actual" vector back to the cerebellum for
 *  forward-model learning + climbing-fiber error broadcast.
 *
 *  Between iterations of the recurrent cascade, accumulated prediction
 *  error above the threshold sets brain->cerebellar_correction_pending
 *  so the next iter's motor/prosody stages apply the cerebellar
 *  prediction as a bias scaled by `strength`.
 *
 *  Default OFF — when not enabled, motor + prosody stages run their
 *  existing logic with no cerebellar involvement (byte-identical to
 *  master for callers that don't flip the flag). When enabled and
 *  brain->cerebellum is NULL (minimal-init brain or cerebellum disabled
 *  at brain create), the stages silently no-op the cerebellar code path. */
nimcp_status_t nimcp_brain_set_cerebellar_correction_enabled(nimcp_brain_t brain,
                                                              bool enabled);
nimcp_status_t nimcp_brain_get_cerebellar_correction_enabled(nimcp_brain_t brain,
                                                              bool* out_enabled);
/** Clamps to [0, 1]. NaN/Inf coerce to 0.5 (mid bias). */
nimcp_status_t nimcp_brain_set_cerebellar_correction_strength(nimcp_brain_t brain,
                                                               float strength);

/** Set the PE-norm threshold above which the recurrent cascade flags
 *  correction_pending. The PE norm is the sum of two cosine-distance norms
 *  (motor + prosody), each in [0, 1], so the meaningful range is [0, 2].
 *  Clamped to [0.0, 2.0]; NaN/Inf coerce to default 0.20. */
nimcp_status_t nimcp_brain_set_cerebellar_pe_threshold(nimcp_brain_t brain,
                                                       float threshold);

/** Snapshot type — lifetime cerebellar diagnostics. */
typedef struct {
    int      enabled;                  /* current value of brain flag */
    float    strength;                 /* current correction strength */
    int      correction_pending;       /* mid-loop flag — most useful WITHIN
                                          a recurrent run; outside, always 0 */
    float    pe_threshold;             /* current PE threshold */
    /* Lifetime totals — incremented by the cascade. */
    uint64_t predictions_made;
    /* S7-H3 fix (2026-05-19): corrections_applied semantics disambiguated.
     * Pre-fix this counted STAGE invocations with correction_pending=true;
     * a recurrent iter with motor+prosody both correcting bumped it by 2,
     * which consumers reading "iters that applied correction" got 2x
     * inflated. Field RETAINED for backwards compatibility but its name in
     * the public diag struct now matches its actual semantics
     * (correction_stages_applied). The new correction_iters_applied
     * counter bumps once per recurrent iter only. */
    uint64_t corrections_applied;             /* deprecated alias of
                                                  correction_stages_applied; identical value */
    uint64_t correction_stages_applied;       /* stages that ran while
                                                  correction_pending was true */
    uint64_t correction_iters_applied;        /* recurrent iters during which
                                                  correction was applied (max 1/iter) */
    float    last_pe_norm;             /* most recent stage's PE-norm */
} nimcp_cerebellar_diag_t;

/** Snapshot cerebellar prediction-correction diagnostics. `out` is zeroed
 *  and populated on success. Cheap (no atomics — cascade is single-caller-
 *  at-a-time by contract). Read returns success even when the feature is
 *  disabled — caller inspects `out->enabled` to know. */
nimcp_status_t nimcp_brain_get_cerebellar_diag(nimcp_brain_t brain,
                                                nimcp_cerebellar_diag_t* out);

/**
 * @brief TA-4: train the bridge on a single (prev1, prev2) → next trigram.
 *
 * Encodes prev1 and prev2 as concept-activation vectors via the bridge's
 * existing reverse encoding, averages them into a context vector, then
 * applies LTP/LTD on `next`'s bindings using the merged context.
 *
 * Cold-start: returns NIMCP_ERROR if EITHER prev1 or prev2 has no prior
 * bindings (encoding all-zero). The flat (bigram) and Riemannian PA-4
 * APIs remain unchanged; this trigram path is purely additive.
 *
 * @return NIMCP_OK on update applied, NIMCP_ERROR on cold-start, no-op,
 *         or validation failure.
 */
nimcp_status_t nimcp_brain_learn_next_token_triple(
    nimcp_brain_t brain,
    const char* prev1,
    const char* prev2,
    const char* next_word,
    float lr
);

/*=============================================================================
 * Tier-2 grounded-language toggles
 *
 * #3: comprehend negation polarity (sign-flip activation_levels on cue).
 * #6: comprehend word-sense disambiguation (intent-cosine binding weighting).
 * #7: multi-turn discourse buffer (push / count / capacity).
 * Each wrapper returns NIMCP_ERROR if grounded_language is unattached.
 *===========================================================================*/

/** Toggle negation-driven activation-sign inversion (default ON). */
nimcp_status_t nimcp_brain_set_grounded_negation_enabled(
    nimcp_brain_t brain,
    bool enabled
);

/** Toggle word-sense disambiguation (default OFF — opt-in). */
nimcp_status_t nimcp_brain_set_grounded_sense_disambiguation_enabled(
    nimcp_brain_t brain,
    bool enabled
);

/** Append a turn (semantic_vec copied) to the discourse ring buffer. */
nimcp_status_t nimcp_brain_grounded_push_turn(
    nimcp_brain_t brain,
    const float* semantic_vec,
    uint32_t vec_dim,
    uint32_t n_words,
    bool is_user
);

/** Query current populated turn count, or -1 on bad/unattached brain. */
int nimcp_brain_grounded_get_discourse_turn_count(nimcp_brain_t brain);

/** Clamp the discourse capacity (oldest-first eviction on shrink). */
nimcp_status_t nimcp_brain_grounded_set_discourse_capacity(
    nimcp_brain_t brain,
    uint8_t capacity
);

/**
 * @brief Tier-1 #2: toggle rule-based anaphora / pronoun resolution inside
 *        the brain's grounded_language comprehend pass.
 *
 * Default: OFF. When enabled, comprehend tracks recent content nouns in
 * a small per-gl ring and resolves singular/plural pronouns
 * (he/she/it/they/...) to the most-recent matching referent, folding
 * the referent's lexicon-entry bindings into the activated_concepts
 * list at half strength. Returns NIMCP_ERROR if the brain has no
 * grounded_language module attached.
 */
nimcp_status_t nimcp_brain_set_anaphora_enabled(
    nimcp_brain_t brain,
    bool enabled
);

/**
 * @brief Toggle read-only engram integration on grounded_language.
 *
 * When enabled, comprehend() lays down a memory trace on each pass
 * (engram_encode) AND probes the engram store for related prior
 * activations (engram_recall), blending recalled neurons into the
 * activated_concepts result at half-weight.
 *
 * Read-only mode: encode + recall blend only. Does NOT modulate
 * lexicon bindings or feed back into engram strengths beyond the
 * engram_recall API's own internal book-keeping.
 *
 * The brain's engram_system (created at cognitive init) is wired in
 * automatically; callers don't pass it. Returns NIMCP_ERROR if either
 * grounded_language or engram_system is missing.
 *
 * @param brain    Brain handle.
 * @param enabled  true = active, false = no-op.
 */
nimcp_status_t nimcp_brain_set_grounded_engram_enabled(
    nimcp_brain_t brain,
    bool enabled
);

/**
 * @brief Toggle Tier-3 immune content inspection on grounded_language.
 *
 * When enabled, comprehend() runs cheap read-only heuristics over the
 * input + activations on every call. The resulting inflammation level
 * damps comprehension_confidence, registers an antigen with the brain
 * immune system on suspicious inputs, and (for strongly inflamed
 * inputs) skips the engram-encode hook so adversarial text doesn't
 * lay down memory traces.
 *
 * Heuristics: NaN/Inf in activations, statistical outliers (>5σ vs
 * running mean), repetition spam (>50% same token), lexicon collision
 * (>80% OOV on long inputs), and stacked negation cascades. Each
 * heuristic contributes a small delta to a [0..1]-clamped inflammation
 * level. Antigen registration triggers at 0.5; engram skip at 0.7.
 *
 * The brain's immune_system (created during the cognitive/safety init
 * wave) is wired in automatically; callers don't pass it. Returns
 * NIMCP_ERROR if either grounded_language or immune_system is missing.
 *
 * Default OFF.
 *
 * @param brain    Brain handle.
 * @param enabled  true = active, false = no-op.
 */
nimcp_status_t nimcp_brain_set_grounded_immune_enabled(
    nimcp_brain_t brain,
    bool enabled
);

/**
 * @brief Configure the autoregressive recurrent decoder (PA-2).
 *
 * @param brain               Brain handle.
 * @param intent_persistence  In [0,1]. 0 (default) reproduces legacy
 *                            in-place 70/30 blend behavior — the original
 *                            intent decays as state evolves toward recent
 *                            words. 1 keeps intent at full strength every
 *                            step (state ignored). Intermediate blends.
 * @param word_feedback       In [0,1]. How aggressively each picked word
 *                            reshapes the recurrent state. Default 0.3
 *                            matches the legacy hard-coded value.
 * @return NIMCP_OK / NIMCP_ERROR_INVALID / NIMCP_ERROR.
 */
nimcp_status_t nimcp_brain_set_snn_language_bridge_autoregressive(
    nimcp_brain_t brain,
    float intent_persistence,
    float word_feedback
);

/**
 * @brief Generate creative text by blending two concepts
 *
 * Conceptual blending — combine two semantic vectors to create novel expression.
 *
 * @param brain       Brain handle
 * @param vector_a    First concept vector
 * @param vector_b    Second concept vector
 * @param vec_dim     Dimension of vectors
 * @param blend_ratio Interpolation [0=all A, 1=all B, 0.5=even]
 * @param out_text    Output text buffer
 * @param text_max    Maximum output length
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_creative_blend(
    nimcp_brain_t brain,
    const float* vector_a,
    const float* vector_b,
    uint32_t vec_dim,
    float blend_ratio,
    char* out_text,
    uint32_t text_max
);

/**
 * @brief Connect two brains through collective cognition
 *
 * Links brain_b into brain_a's collective cognition system so they
 * share consciousness metrics, hyperscanning sync, and load balancing.
 * Both brains must have collective cognition enabled.
 *
 * @param brain_a First brain (host collective)
 * @param brain_b Second brain (joining collective)
 * @param instance_id Unique ID for brain_b in the collective
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_connect_collective(
    nimcp_brain_t brain_a,
    nimcp_brain_t brain_b,
    uint32_t instance_id
);

/**
 * @brief Avatar face state — visemes, FACS action units, emotion, gaze
 *
 * Collected from Broca's speech motor planner, emotional system, and
 * attention system. Drives a parameterized face mesh for synchronized
 * audio-visual communication.
 */
typedef struct {
    /* Viseme / mouth shape (from speech motor articulators) */
    float mouth_open;           /**< Jaw opening [0=closed, 1=wide open] */
    float lip_round;            /**< Lip rounding [0=spread, 1=rounded] */
    float lip_upper;            /**< Upper lip raise [0,1] */
    float lip_lower;            /**< Lower lip drop [0,1] */
    float tongue_position;      /**< Tongue front-back [0=front, 1=back] */
    uint8_t current_viseme;     /**< Active viseme ID (0-21, Preston Blair) */

    /* FACS Action Units (Ekman) — primary face muscles */
    float au1_inner_brow_raise; /**< AU1: Inner brow raiser (frontalis medial) */
    float au2_outer_brow_raise; /**< AU2: Outer brow raiser (frontalis lateral) */
    float au4_brow_lower;       /**< AU4: Brow lowerer (corrugator) */
    float au5_upper_lid_raise;  /**< AU5: Upper lid raiser (levator) */
    float au6_cheek_raise;      /**< AU6: Cheek raiser (orbicularis oculi) */
    float au7_lid_tighten;      /**< AU7: Lid tightener */
    float au9_nose_wrinkle;     /**< AU9: Nose wrinkler (levator labii) */
    float au10_upper_lip_raise; /**< AU10: Upper lip raiser */
    float au12_lip_corner_pull; /**< AU12: Lip corner puller (zygomatic) — SMILE */
    float au15_lip_corner_drop; /**< AU15: Lip corner depressor — FROWN */
    float au17_chin_raise;      /**< AU17: Chin raiser (mentalis) */
    float au20_lip_stretch;     /**< AU20: Lip stretcher (risorius) */
    float au23_lip_tighten;     /**< AU23: Lip tightener (orbicularis oris) */
    float au25_lips_part;       /**< AU25: Lips part */
    float au26_jaw_drop;        /**< AU26: Jaw drop (masseter relax) */
    float au28_lip_suck;        /**< AU28: Lip suck */

    /* Emotional state */
    float valence;              /**< Emotional valence [-1=negative, +1=positive] */
    float arousal;              /**< Emotional arousal [0=calm, 1=excited] */
    float dominance;            /**< Dominance [-1=submissive, +1=dominant] */
    uint32_t emotion_id;        /**< Primary emotion (0-18, emotion_category_t) */
    float emotion_intensity;    /**< Emotion intensity [0,1] */

    /* Gaze and head pose */
    float gaze_x;               /**< Horizontal gaze [-1=left, +1=right] */
    float gaze_y;               /**< Vertical gaze [-1=down, +1=up] */
    float head_pitch;           /**< Head pitch (nod) [-1=down, +1=up] */
    float head_yaw;             /**< Head yaw (turn) [-1=left, +1=right] */
    float head_roll;            /**< Head roll (tilt) [-1=left, +1=right] */
    float blink;                /**< Blink state [0=open, 1=closed] */

    /* Voice parameters (for TTS prosody sync) */
    float pitch_hz;             /**< Voice pitch (Hz) */
    float speaking_rate;        /**< Speaking rate multiplier (1.0=normal) */
    float volume;               /**< Voice volume [0,1] */

    /* Metadata */
    uint64_t timestamp_us;      /**< Timestamp in microseconds */
    bool is_speaking;           /**< Whether currently producing speech */
} nimcp_avatar_state_t;

/**
 * @brief Get current avatar face state for rendering
 *
 * Collects viseme data from speech motor planner, FACS action units from
 * emotional system, and gaze from attention. Call after decide_full() or
 * speak() to get synchronized state.
 *
 * @param brain Brain handle
 * @param state Output avatar state
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_get_avatar_state(
    nimcp_brain_t brain,
    nimcp_avatar_state_t* state
);

// =========================================================================
// UNIFIED COGNITIVE TRAINING API
// =========================================================================

/**
 * @brief Train all cognitive modules from text in one call
 *
 * Trains grounded language (distributional + syntactic), knowledge system,
 * language generator (LNN decoder), and grounded language pairs — all from
 * a single text input. This ensures all modules learn together, not just
 * the neural network weights.
 *
 * @param brain        Brain handle
 * @param text         Text to learn from
 * @param domain       Knowledge domain (0=language..10=general, -1 to skip knowledge)
 * @param target_text  Optional target/response text for pair learning (NULL to skip)
 * @param learning_rate Learning rate (0 = default 0.001)
 * @param out_loss     Output: aggregate loss across modules (optional)
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_train_cognitive(
    nimcp_brain_t brain,
    const char* text,
    int domain,
    const char* target_text,
    float learning_rate,
    float* out_loss
);

/**
 * @brief Get per-module cognitive training statistics
 */
nimcp_status_t nimcp_brain_get_cognitive_stats(
    nimcp_brain_t brain,
    uint32_t* out_stats,     /* array of step counts, 13 modules */
    float* out_losses,       /* array of last losses, 13 modules */
    uint32_t* out_count      /* number of modules written */
);

/**
 * @brief Get the cognitive transcript from the last brain_decide() call
 *
 * Retrieves transcript entries cached on the brain from the most recent
 * decision cycle. Each entry contains a module name, summary text,
 * salience score, and confidence.
 *
 * @param brain Brain handle
 * @param out_entries Output array of summary strings (256 chars each)
 * @param out_saliences Output array of salience values
 * @param out_confidences Output array of confidence values
 * @param out_modules Output array of module name strings
 * @param max_entries Maximum entries to return
 * @return Number of entries written
 */
uint32_t nimcp_brain_get_last_transcript(
    nimcp_brain_t brain,
    char (*out_entries)[256],
    float* out_saliences,
    float* out_confidences,
    const char** out_modules,
    uint32_t max_entries
);

/**
 * @brief Learn knowledge from text in a specific domain
 *
 * Feeds text to the knowledge system for multi-domain learning.
 *
 * @param brain   Brain handle
 * @param text    Text to learn from
 * @param domain  Knowledge domain (0-10, see knowledge_domain_t)
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_learn_knowledge(
    nimcp_brain_t brain,
    const char* text,
    int domain
);

/**
 * @brief Save brain to file
 *
 * @param brain Brain handle
 * @param filepath Path to save file
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_save(nimcp_brain_t brain, const char* filepath);

/**
 * @brief Freeze brain for inference-only mode
 *
 * Disables learning, locks weights, and optimizes for fast inference.
 * After freezing: learning rate = 0, GPU weight cache is finalized,
 * eligibility traces freed, weight dirty flag cleared.
 *
 * @param brain Brain handle
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_freeze(nimcp_brain_t brain);

/**
 * @brief Check if brain is frozen
 *
 * @param brain Brain handle
 * @return true if brain is in frozen inference-only mode
 */
bool nimcp_brain_is_frozen(nimcp_brain_t brain);

/**
 * @brief Load brain from file
 *
 * @param filepath Path to saved brain file
 * @return Brain handle or NULL on error
 */
nimcp_brain_t nimcp_brain_load(const char* filepath);

//=============================================================================
// Training Pipeline API - Loss Functions, Optimizers, LR Schedulers
//=============================================================================

/**
 * @brief Loss function types for training (public API)
 * @note Uses distinct names to avoid conflicts with internal training module types
 */
typedef enum {
    NIMCP_API_LOSS_MSE = 0,              /**< Mean Squared Error (regression) */
    NIMCP_API_LOSS_CROSS_ENTROPY = 1,    /**< Cross-Entropy (classification) */
    NIMCP_API_LOSS_BINARY_CE = 2,        /**< Binary Cross-Entropy */
    NIMCP_API_LOSS_HUBER = 3,            /**< Huber Loss (robust regression) */
    NIMCP_API_LOSS_MAE = 4,              /**< Mean Absolute Error */
    NIMCP_API_LOSS_FOCAL = 5,            /**< Focal Loss (imbalanced classes) */
    NIMCP_API_LOSS_KL_DIV = 6            /**< KL Divergence */
} nimcp_api_loss_t;

/**
 * @brief Optimizer types for training (public API)
 */
typedef enum {
    NIMCP_API_OPT_SGD = 0,               /**< Stochastic Gradient Descent */
    NIMCP_API_OPT_MOMENTUM = 1,          /**< SGD with Momentum */
    NIMCP_API_OPT_ADAM = 2,              /**< Adam optimizer */
    NIMCP_API_OPT_ADAMW = 3,             /**< AdamW (weight decay) */
    NIMCP_API_OPT_RMSPROP = 4,           /**< RMSprop */
    NIMCP_API_OPT_ADAGRAD = 5            /**< Adagrad */
} nimcp_api_optimizer_t;

/**
 * @brief Learning rate scheduler types (public API)
 */
typedef enum {
    NIMCP_API_SCHED_CONSTANT = 0,           /**< Constant learning rate */
    NIMCP_API_SCHED_STEP = 1,               /**< Step decay */
    NIMCP_API_SCHED_EXPONENTIAL = 2,        /**< Exponential decay */
    NIMCP_API_SCHED_COSINE = 3,             /**< Cosine annealing */
    NIMCP_API_SCHED_WARMUP_COSINE = 4,      /**< Warmup + Cosine annealing */
    NIMCP_API_SCHED_REDUCE_ON_PLATEAU = 5,  /**< Reduce when metric plateaus */
    NIMCP_API_SCHED_CYCLIC = 6              /**< Cyclic learning rate */
} nimcp_api_scheduler_t;

/**
 * @brief Training configuration for the training pipeline
 */
typedef struct {
    nimcp_api_loss_t loss_type;           /**< Loss function type */
    nimcp_api_optimizer_t optimizer_type; /**< Optimizer type */
    nimcp_api_scheduler_t scheduler_type; /**< LR scheduler type */

    float learning_rate;       /**< Initial learning rate (default: 0.001) */
    float weight_decay;        /**< L2 regularization (default: 0.0) */
    float momentum;            /**< Momentum for SGD/Momentum (default: 0.9) */
    float beta1;               /**< Adam beta1 (default: 0.9) */
    float beta2;               /**< Adam beta2 (default: 0.999) */
    float epsilon;             /**< Adam epsilon (default: 1e-8) */

    uint32_t scheduler_step_size;   /**< Steps between LR updates */
    float scheduler_gamma;          /**< LR decay factor (default: 0.1) */
    uint32_t warmup_steps;          /**< Warmup steps (default: 0) */

    bool enable_gradient_clipping;  /**< Enable gradient clipping */
    float gradient_clip_value;      /**< Max gradient norm (default: 1.0) */

    bool enable_biological_modulation; /**< Enable plasticity bridge modulation */
    float biological_blend;            /**< Biological modulation strength (0-1) */

    // === NETWORK TYPE DISPATCH (NEW) ===
    nimcp_network_type_t network_type;     /**< Network architecture type (default: ADAPTIVE) */

    // SNN-specific training options (used when network_type == NIMCP_NETWORK_SNN)
    nimcp_snn_train_method_t snn_method;   /**< SNN training method (default: STDP) */
    float snn_eligibility_tau;             /**< Eligibility trace decay (ms, default: 20.0) */
    float snn_reward_tau;                  /**< Reward signal decay (ms, default: 100.0) */
    float snn_surrogate_beta;              /**< Surrogate gradient steepness (default: 5.0) */

    // LNN-specific training options (used when network_type == NIMCP_NETWORK_LNN)
    nimcp_lnn_train_method_t lnn_method;   /**< LNN training method (default: ADJOINT) */
    uint32_t lnn_bptt_truncation;          /**< BPTT truncation length (default: 100) */
    bool lnn_use_adjoint_checkpointing;    /**< Memory-efficient checkpointing (default: true) */

    /* Rubric integration (cognitive quality monitoring during training) */
    bool enable_rubric;                /**< Enable periodic rubric evaluation (default: false) */
    uint32_t rubric_interval;          /**< Steps between evaluations (0 = epoch-only, default: 0) */
    float rubric_min_score;            /**< Minimum acceptable score [0-1] (0.0 = no threshold) */
    bool rubric_stop_on_threshold;     /**< Stop training if score < min (default: false) */
} nimcp_training_config_t;

/**
 * @brief Training result/statistics from a training step
 */
typedef struct {
    float loss;                /**< Loss value */
    float learning_rate;       /**< Current learning rate */
    uint32_t step;             /**< Training step number */
    bool early_stopped;        /**< True if early stopping triggered */
    float gradient_norm;       /**< Gradient norm (if clipping enabled) */

    /* Rubric (populated only when evaluated this step) */
    bool rubric_evaluated;             /**< True if rubric was evaluated this step */
    float rubric_score;                /**< Overall rubric score [0-1] */
    char rubric_grade;                 /**< Letter grade A/B/C/D/F */
    char rubric_grade_modifier;        /**< +/-/space */
} nimcp_training_result_t;

/**
 * @brief Get default training configuration
 *
 * Returns a sensible default configuration:
 * - Loss: Cross-Entropy
 * - Optimizer: Adam (lr=0.001, betas=0.9/0.999)
 * - Scheduler: Cosine annealing
 * - Biological modulation: enabled at 50%
 *
 * @return Default training configuration
 */
nimcp_training_config_t nimcp_training_config_default(void);

/**
 * @brief Configure brain's training pipeline
 *
 * Sets up the internal training coordinator with specified loss function,
 * optimizer, and learning rate scheduler. Must be called before using
 * nimcp_brain_train_step() or nimcp_brain_train_batch().
 *
 * @param brain Brain handle
 * @param config Training configuration
 * @return NIMCP_OK on success, error code otherwise
 *
 * EXAMPLE:
 * ```c
 * nimcp_training_config_t config = nimcp_training_config_default();
 * config.loss_type = NIMCP_API_LOSS_CROSS_ENTROPY;
 * config.optimizer_type = NIMCP_API_OPT_ADAM;
 * config.learning_rate = 0.001f;
 * nimcp_brain_configure_training(brain, &config);
 * ```
 */
nimcp_status_t nimcp_brain_configure_training(
    nimcp_brain_t brain,
    const nimcp_training_config_t* config
);

/**
 * @brief Train brain using the training pipeline (single step)
 *
 * Performs a complete training step using the configured training pipeline:
 * 1. Forward pass to get predictions
 * 2. Loss computation (via Loss Functions module)
 * 3. Gradient computation (backpropagation)
 * 4. Gradient health check (via Gradient Manager)
 * 5. Regularization (if enabled)
 * 6. Biological modulation (via Plasticity Bridge, if enabled)
 * 7. Weight update (via Optimizer)
 * 8. Learning rate update (via LR Scheduler)
 *
 * This uses the full training coordinator with all integrated modules.
 *
 * @param brain Brain handle
 * @param features Input features array
 * @param num_features Number of input features
 * @param targets Target output array (one-hot or continuous)
 * @param num_targets Number of target outputs
 * @param result Output: training result (can be NULL)
 * @return NIMCP_OK on success, error code otherwise
 *
 * EXAMPLE:
 * ```c
 * float features[784] = {...};
 * float targets[10] = {0,0,0,1,0,0,0,0,0,0};  // One-hot for class 3
 * nimcp_training_result_t result;
 * nimcp_brain_train_step(brain, features, 784, targets, 10, &result);
 * printf("Loss: %.4f, LR: %.6f\n", result.loss, result.learning_rate);
 * ```
 */
nimcp_status_t nimcp_brain_train_step(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    const float* targets,
    uint32_t num_targets,
    nimcp_training_result_t* result
);

/**
 * @brief Train brain on a batch of examples
 *
 * Performs training on multiple examples, accumulating gradients before
 * applying weight updates (mini-batch gradient descent).
 *
 * @param brain Brain handle
 * @param features 2D array of features [batch_size][num_features]
 * @param targets 2D array of targets [batch_size][num_targets]
 * @param batch_size Number of examples in batch
 * @param num_features Features per example
 * @param num_targets Targets per example
 * @param result Output: training result with averaged loss (can be NULL)
 * @return NIMCP_OK on success, error code otherwise
 *
 * EXAMPLE:
 * ```c
 * float features[32][784];  // 32 images, 784 pixels each
 * float targets[32][10];    // 32 one-hot labels
 * nimcp_training_result_t result;
 * nimcp_brain_train_batch(brain, (float*)features, (float*)targets,
 *                          32, 784, 10, &result);
 * ```
 */
nimcp_status_t nimcp_brain_train_batch(
    nimcp_brain_t brain,
    const float* features,
    const float* targets,
    uint32_t batch_size,
    uint32_t num_features,
    uint32_t num_targets,
    nimcp_training_result_t* result
);

/**
 * @brief Get current training statistics
 *
 * @param brain Brain handle
 * @param total_steps Output: total training steps
 * @param total_loss Output: cumulative loss
 * @param current_lr Output: current learning rate
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_get_training_stats(
    nimcp_brain_t brain,
    uint64_t* total_steps,
    float* total_loss,
    float* current_lr
);

/**
 * @brief Step the learning rate scheduler (call at epoch end)
 *
 * Updates the learning rate according to the configured scheduler.
 * For ReduceOnPlateau, provide the validation metric.
 *
 * @param brain Brain handle
 * @param validation_metric Validation metric (for ReduceOnPlateau, ignored otherwise)
 * @return New learning rate
 */
float nimcp_brain_step_scheduler(nimcp_brain_t brain, float validation_metric);

//=============================================================================
// Training Callbacks API - Event-driven training monitoring and control
//=============================================================================

/**
 * @brief Training callback event types (public API)
 */
typedef enum {
    NIMCP_CB_STEP_COMPLETE = 0,    /**< Training step finished */
    NIMCP_CB_EPOCH_COMPLETE,        /**< Epoch finished */
    NIMCP_CB_LOSS_COMPUTED,         /**< Loss calculated */
    NIMCP_CB_WEIGHTS_UPDATED,       /**< Weights modified */
    NIMCP_CB_LR_CHANGED,            /**< Learning rate changed */
    NIMCP_CB_CONVERGENCE,           /**< Early stopping triggered */
    NIMCP_CB_DIVERGENCE,            /**< Training instability */
    NIMCP_CB_CHECKPOINT,            /**< Checkpoint saved */
    NIMCP_CB_EVENT_COUNT
} nimcp_callback_event_t;

/**
 * @brief Callback return actions
 */
typedef enum {
    NIMCP_CB_ACTION_CONTINUE = 0,   /**< Continue training normally */
    NIMCP_CB_ACTION_STOP,           /**< Stop training loop */
    NIMCP_CB_ACTION_SKIP,           /**< Skip current step */
    NIMCP_CB_ACTION_ROLLBACK,       /**< Rollback to checkpoint */
    NIMCP_CB_ACTION_REDUCE_LR,      /**< Reduce learning rate */
    NIMCP_CB_ACTION_INCREASE_LR     /**< Increase learning rate */
} nimcp_callback_action_t;

/**
 * @brief Training metrics passed to callbacks
 */
typedef struct {
    uint64_t step;                  /**< Current training step */
    uint64_t epoch;                 /**< Current epoch */
    float loss;                     /**< Current loss value */
    float loss_ema;                 /**< Exponential moving average of loss */
    float learning_rate;            /**< Current learning rate */
    float gradient_norm;            /**< L2 norm of gradients */
    uint64_t step_time_us;          /**< Time for current step */
    bool is_converging;             /**< Loss trending down */
    bool is_diverging;              /**< Loss trending up rapidly */
} nimcp_callback_metrics_t;

/**
 * @brief Callback function signature
 *
 * @param event Event type that triggered callback
 * @param metrics Current training metrics
 * @param user_data User-provided context
 * @return Action for training loop to take
 */
typedef nimcp_callback_action_t (*nimcp_training_callback_fn)(
    nimcp_callback_event_t event,
    const nimcp_callback_metrics_t* metrics,
    void* user_data
);

/**
 * @brief Callback configuration for registration
 */
typedef struct {
    bool enable_auto_checkpoint;     /**< Enable automatic checkpointing */
    uint32_t checkpoint_interval;    /**< Steps between checkpoints */
    bool enable_early_stopping;      /**< Enable early stopping */
    uint32_t patience;               /**< Steps without improvement before stop */
    float min_delta;                 /**< Minimum improvement to reset patience */
    float divergence_threshold;      /**< Loss increase ratio for divergence */
    uint32_t log_interval;           /**< Steps between log outputs (0=disabled) */
} nimcp_callback_config_t;

/**
 * @brief Get default callback configuration
 *
 * @return Default configuration with sensible values
 */
nimcp_callback_config_t nimcp_callback_config_default(void);

/**
 * @brief Enable training callbacks for a brain
 *
 * Must be called after nimcp_brain_configure_training().
 * Enables the callback system with the specified configuration.
 *
 * @param brain Brain handle
 * @param config Callback configuration (NULL for defaults)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_enable_callbacks(
    nimcp_brain_t brain,
    const nimcp_callback_config_t* config
);

/**
 * @brief Disable training callbacks for a brain
 *
 * @param brain Brain handle
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_disable_callbacks(nimcp_brain_t brain);

/**
 * @brief Enable or disable FP16 mixed precision training
 *
 * When enabled, GPU backward pass and gradient accumulation use
 * automatic mixed precision (AMP) with FP16 compute and FP32 storage.
 * Includes dynamic loss scaling to prevent gradient underflow.
 *
 * Requires GPU acceleration to be active. The GPU weight cache must
 * be initialized (typically after the first learn step).
 *
 * @param brain Brain handle
 * @param enable true to enable, false to disable
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_enable_mixed_precision(nimcp_brain_t brain, bool enable);

/**
 * @brief Enable or disable gradient checkpointing for memory-efficient training
 *
 * When enabled, the GPU forward pass only retains activations at every Nth
 * layer (checkpoint boundaries). During backward pass, intermediate activations
 * are recomputed from the nearest checkpoint. This trades ~1 extra forward pass
 * per segment for O(sqrt(L)) activation memory instead of O(L).
 *
 * Requires GPU acceleration to be active. The GPU weight cache must
 * be initialized (typically after the first learn step).
 *
 * @param brain Brain handle
 * @param enable true to enable, false to disable
 * @param checkpoint_interval Layers between checkpoints (0 = default every 2 layers)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_enable_gradient_checkpointing(
    nimcp_brain_t brain, bool enable, uint32_t checkpoint_interval);

/**
 * @brief Enable or disable hemispheric architecture (callosum + lateralization)
 *
 * When enabled, the brain's neurons are logically split into left and right
 * hemispheres connected by a corpus callosum with 5 biological channels
 * (motor, sensory, cognitive, emotional, inhibitory). Cognitive domains are
 * routed to the dominant hemisphere via lateralization weights.
 *
 * Lightweight integration: no sub-brains are created (avoids 3x memory).
 * Instead, the existing network's neurons are logically partitioned.
 *
 * @param brain Brain handle
 * @param enable true to enable, false to disable
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_enable_hemispheric(nimcp_brain_t brain, bool enable);

/**
 * @brief Get lateralization dominance for a cognitive domain
 *
 * @param brain Brain handle
 * @param domain Cognitive domain index (0-11, see cognitive_domain_t)
 * @return Dominance value 0.0-1.0 (0=right dominant, 1=left dominant), -1 on error
 */
float nimcp_brain_get_lateralization(nimcp_brain_t brain, uint32_t domain);

/**
 * @brief Shift lateralization for a cognitive domain (plasticity)
 *
 * @param brain Brain handle
 * @param domain Cognitive domain index
 * @param shift Shift amount (+ve = more left, -ve = more right)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_shift_lateralization(
    nimcp_brain_t brain, uint32_t domain, float shift);

/**
 * @brief Get corpus callosum transfer count
 *
 * @param brain Brain handle
 * @return Total inter-hemispheric messages transferred, 0 if disabled
 */
uint64_t nimcp_brain_get_callosum_transfers(nimcp_brain_t brain);

/**
 * @brief Get current hemispheric balance
 *
 * @param brain Brain handle
 * @return Balance value [-1.0=left dominant, +1.0=right dominant], 0 if disabled
 */
float nimcp_brain_get_hemispheric_balance(nimcp_brain_t brain);

/**
 * @brief Get module activity counters (statue-suspect probe).
 *
 * Returns monotonic uint64 counters for hot-path modules whose ablation
 * effect is otherwise hard to distinguish from "module not firing". Used
 * by the ablation harness to confirm that a module is actually invoked
 * during inference/learning, separate from "ablation moves loss" outcome.
 *
 * Field order in out[] (caller MUST allocate at least 32 elements):
 *   [0]  astrocyte_modulation_events
 *   [1]  oligodendrocyte_myelin_apply
 *   [2]  microglia_pruning_events
 *   [3]  sleep_replay_events
 *   [4]  sleep_downscale_events
 *   [5]  lgss_input_rejections
 *   [6]  lgss_action_blocks
 *   [7]  lgss_motor_blocks
 *   [8]  lgss_training_blocks
 *   [9]  lgss_reward_blocks
 *   [10] ethics_violations
 *   [11] hnn_forward_invocations
 *   [12] hnn_fallback_invocations
 *   [13] cortical_column_forward_invocations
 *   [14] cortical_wta_winners_total
 *   [15] cortical_wta_calls
 *   [16] thalamic_routes_dispatched
 *   [17] thalamic_drops_backpressure
 *   [18] callosum_transfers
 *   [19] kg_consumer_hits
 *   [20..31] reserved (zero)
 *
 * @param brain Brain handle
 * @param out Caller-allocated uint64[32] receiver
 * @return NIMCP_OK on success; NIMCP_ERROR_INVALID_PARAM if brain or out is NULL
 */
nimcp_status_t nimcp_brain_get_module_activity(
    nimcp_brain_t brain, uint64_t out[32]);

/**
 * @brief Connect a cloud backend for hybrid edge-cloud inference
 *
 * When connected, the local brain will escalate uncertain decisions to the
 * cloud backend. Cloud responses are optionally distilled back to improve
 * the local brain over time.
 *
 * @param brain Local brain (edge device)
 * @param cloud_brain Backend brain (cloud/server) — uses in-process bridge
 * @param confidence_threshold Escalate if local confidence below this (0.0-1.0)
 * @param enable_distillation Train local brain from cloud answers
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_connect_cloud(nimcp_brain_t brain,
                                          nimcp_brain_t cloud_brain,
                                          float confidence_threshold,
                                          bool enable_distillation);

/**
 * @brief Disconnect cloud backend (return to standalone mode)
 * @param brain Brain handle
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_disconnect_cloud(nimcp_brain_t brain);

/**
 * @brief Get cloud inference statistics
 * @param brain Brain handle
 * @param total_queries Output: total queries
 * @param local_handled Output: queries handled locally
 * @param cloud_escalated Output: queries sent to cloud
 * @param distillation_steps Output: times local learned from cloud
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_get_cloud_stats(nimcp_brain_t brain,
                                            uint64_t* total_queries,
                                            uint64_t* local_handled,
                                            uint64_t* cloud_escalated,
                                            uint64_t* distillation_steps);

/**
 * @brief Process buffered distillation examples (batch learning from cloud)
 * @param brain Brain handle
 * @param max_examples Max examples to process (0 = all)
 * @return Number of examples processed
 */
uint32_t nimcp_brain_distill_cloud_batch(nimcp_brain_t brain, uint32_t max_examples);

/**
 * @brief Enable/disable recurrent forward pass (iterative refinement)
 *
 * @param brain Brain handle
 * @param enable Enable flag
 * @param max_iterations Maximum refinement iterations (1-10)
 * @param confidence_threshold Stop when confidence exceeds this (0.0-1.0)
 * @param blend_alpha Output-to-input blend ratio (0.0-1.0)
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_enable_recurrent(nimcp_brain_t brain, bool enable,
                                             uint32_t max_iterations,
                                             float confidence_threshold,
                                             float blend_alpha);

/**
 * @brief Enable/disable BPTT (backpropagation through time)
 *
 * @param brain Brain handle
 * @param enable Enable flag
 * @param window_size Temporal buffer size (1-32)
 * @param discount Gradient discount per step back (0.0-1.0)
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_enable_bptt(nimcp_brain_t brain, bool enable,
                                        uint32_t window_size, float discount);

/**
 * @brief Get recurrent iteration count from last brain_decide() call
 *
 * @param brain Brain handle
 * @return Number of iterations used in last decision
 */
uint32_t nimcp_brain_get_recurrent_iterations(nimcp_brain_t brain);

/**
 * @brief Register a callback for a specific event type
 *
 * @param brain Brain handle
 * @param event Event type to subscribe to
 * @param callback Callback function
 * @param user_data User context passed to callback
 * @param name Callback name for logging (can be NULL)
 * @return Callback ID (>0) on success, 0 on error
 *
 * EXAMPLE:
 * ```c
 * nimcp_callback_action_t my_logger(
 *     nimcp_callback_event_t event,
 *     const nimcp_callback_metrics_t* m,
 *     void* user_data)
 * {
 *     printf("Step %lu: loss=%.4f lr=%.6f\n", m->step, m->loss, m->learning_rate);
 *     return NIMCP_CB_ACTION_CONTINUE;
 * }
 *
 * uint32_t cb_id = nimcp_brain_register_callback(
 *     brain, NIMCP_CB_STEP_COMPLETE, my_logger, NULL, "logger");
 * ```
 */
uint32_t nimcp_brain_register_callback(
    nimcp_brain_t brain,
    nimcp_callback_event_t event,
    nimcp_training_callback_fn callback,
    void* user_data,
    const char* name
);

/**
 * @brief Unregister a callback
 *
 * @param brain Brain handle
 * @param callback_id Callback ID from registration
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_unregister_callback(
    nimcp_brain_t brain,
    uint32_t callback_id
);

/**
 * @brief Get callback statistics
 *
 * @param brain Brain handle
 * @param total_fired Output: total callbacks fired
 * @param avg_time_us Output: average callback execution time
 * @param early_stops Output: early stops triggered
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_get_callback_stats(
    nimcp_brain_t brain,
    uint64_t* total_fired,
    float* avg_time_us,
    uint32_t* early_stops
);

//=============================================================================
// Phase 2.8: Dynamic Brain Resizing
//=============================================================================

/**
 * @brief Manually resize brain to a specific neuron count
 *
 * @param brain Brain handle
 * @param new_neuron_count Target number of neurons
 * @return true on success, false on failure
 */
bool nimcp_brain_resize(nimcp_brain_t brain, uint32_t new_neuron_count);

/**
 * @brief Automatically resize brain based on hardware capabilities and utilization
 *
 * @param brain Brain handle
 * @return true if resized, false if no resize needed or on error
 */
bool nimcp_brain_auto_resize(nimcp_brain_t brain);

/**
 * @brief Get current neuron count
 *
 * @param brain Brain handle
 * @return Current number of neurons, or 0 on error
 */
uint32_t nimcp_brain_get_neuron_count(nimcp_brain_t brain);

/**
 * @brief Get brain utilization metrics
 *
 * @param brain Brain handle
 * @param utilization Output: percentage of neurons being utilized (0.0-1.0)
 * @param saturation Output: percentage of neurons at capacity (0.0-1.0)
 * @return true on success, false on error
 */
bool nimcp_brain_get_utilization_metrics(nimcp_brain_t brain, float* utilization, float* saturation);

/** Get active neuron count from last compute step (40-watt brain sparsity) */
uint32_t nimcp_brain_get_active_neuron_count(nimcp_brain_t brain);

/** Get sparsity ratio (fraction of neurons active) */
float nimcp_brain_get_sparsity_ratio(nimcp_brain_t brain);

//=============================================================================
// Brain Snapshots - Named, timestamped backups for versioning & A/B testing
//=============================================================================

/**
 * @brief Snapshot metadata information
 */
typedef struct {
    char name[128];           /**< Snapshot name */
    char description[512];    /**< User description */
    uint64_t timestamp;       /**< Unix timestamp when snapshot was created */
    uint32_t file_size;       /**< Size of snapshot file in bytes */
    bool is_compressed;       /**< Whether snapshot is compressed */
    bool is_encrypted;        /**< Whether snapshot is encrypted */
} nimcp_brain_snapshot_info_t;

/**
 * @brief Save a named snapshot of the brain state
 *
 * Snapshots are different from checkpoints:
 * - Checkpoints: Auto-saved to a single file for resumption
 * - Snapshots: Named, timestamped backups for versioning/backup/A/B testing
 *
 * Example usage:
 * ```c
 * nimcp_brain_snapshot_save(brain, "before_training", "Baseline state");
 * // Train the model...
 * nimcp_brain_snapshot_save(brain, "after_epoch_1", "After 1 epoch");
 * // More training...
 * nimcp_brain_snapshot_save(brain, "final", "Production model");
 * ```
 *
 * Snapshots are saved to: {snapshot_dir}/{name}_{timestamp}.snapshot
 *
 * @param brain Brain to snapshot
 * @param name Snapshot name (no path, just name)
 * @param description Optional description (can be NULL)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_status_t nimcp_brain_snapshot_save(
    nimcp_brain_t brain,
    const char* name,
    const char* description
);

/**
 * @brief Restore brain from a named snapshot
 *
 * @param brain Current brain (can be NULL to create new brain from snapshot)
 * @param name Snapshot name or full path to snapshot file
 * @return New brain instance restored from snapshot, or NULL on error
 */
nimcp_brain_t nimcp_brain_snapshot_restore(
    nimcp_brain_t brain,
    const char* name
);

/**
 * @brief List all available snapshots
 *
 * @param brain Brain instance (to get snapshot directory)
 * @param infos Array to store snapshot information
 * @param max_count Maximum number of snapshots to list
 * @param out_count Pointer to store actual count (can be NULL)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_status_t nimcp_brain_snapshot_list(
    nimcp_brain_t brain,
    nimcp_brain_snapshot_info_t* infos,
    uint32_t max_count,
    uint32_t* out_count
);

/**
 * @brief Delete a named snapshot
 *
 * @param brain Brain instance (to get snapshot directory)
 * @param name Snapshot name to delete
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_status_t nimcp_brain_snapshot_delete(
    nimcp_brain_t brain,
    const char* name
);

/**
 * @brief Create brain from YAML or JSON configuration file
 *
 * Supports loading brain configuration from YAML (.yaml, .yml) or JSON (.json) files.
 * Configuration includes architecture, training parameters, plasticity settings, and ethics.
 *
 * Example YAML config:
 * ```yaml
 * brain:
 *   name: "classifier"
 *   size: small           # tiny, small, medium, large
 *   task: classification  # classification, regression, pattern_matching, sequence, association
 *   architecture:
 *     num_inputs: 784
 *     num_outputs: 10
 *     num_hidden: 256
 *     learning_rate: 0.01
 * ```
 *
 * @param config_filepath Path to YAML or JSON configuration file
 * @return Brain handle or NULL on error
 */
nimcp_brain_t nimcp_brain_create_from_config(const char* config_filepath);

/**
 * @brief Brain probe statistics - comprehensive snapshot of brain state
 */
typedef struct {
    char task_name[64];           /**< Brain name */
    nimcp_brain_size_t size;      /**< Size preset */
    nimcp_brain_task_t task;      /**< Task type */
    uint32_t num_neurons;         /**< Total neurons */
    uint32_t num_synapses;        /**< Total synapses */
    uint32_t num_active_synapses; /**< Non-pruned synapses */

    uint64_t total_inferences;     /**< Total inference count */
    uint64_t total_learning_steps; /**< Total learning steps */

    float avg_sparsity;          /**< Average sparsity (0.0-1.0) */
    float avg_inference_time_us; /**< Average inference time (microseconds) */
    float current_learning_rate; /**< Current learning rate */

    float accuracy;      /**< Validation accuracy (0.0-1.0) */
    size_t memory_bytes; /**< Memory usage in bytes */

    uint32_t num_inputs;  /**< Number of inputs */
    uint32_t num_outputs; /**< Number of outputs */

    // Copy-on-Write (COW) cache statistics
    bool is_cow_clone;          /**< True if this brain is a COW clone */
    uint32_t cow_ref_count;     /**< Reference count for shared data (0 if not COW) */
    size_t cow_shared_bytes;    /**< Bytes shared via COW (0 if not COW) */
    size_t cow_private_bytes;   /**< Bytes private to this brain (always > 0) */

    // GPU status
    bool gpu_available;         /**< True if GPU acceleration is active */
} nimcp_brain_probe_t;

/**
 * @brief Probe brain state - get comprehensive statistics
 *
 * This function provides a snapshot of the brain's current state including
 * architecture, performance metrics, and resource usage. Similar to a network
 * probe, it returns all relevant information for monitoring and debugging.
 *
 * @param brain Brain handle
 * @param probe Output structure to fill with brain statistics
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_probe(nimcp_brain_t brain, nimcp_brain_probe_t* probe);

/**
 * @brief Broadcast brain probe data via bio-async message system
 *
 * WHAT: Sends brain probe metrics to all interested subscribers via bio-async
 * WHY:  Enables loose coupling - metrics module receives data without direct dependency
 * HOW:  Fills bio_msg_brain_probe_data_t and broadcasts via BIO_MSG_BRAIN_PROBE_DATA
 *
 * This function allows multiple brains to be monitored concurrently. Each brain
 * broadcasts its metrics using its unique brain_id (pointer address), allowing
 * subscribers to differentiate between different brain instances.
 *
 * @param brain Brain handle to probe and broadcast
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_broadcast_probe(nimcp_brain_t brain);

//=============================================================================
// Brain Health Probes - System-Level Metrics Collection
//=============================================================================

/**
 * @brief Immune system metrics snapshot
 *
 * WHAT: Aggregated immune system health metrics for external monitoring
 * WHY:  Enables health dashboards to track immune activity without
 *       exposing internal immune system structures
 * HOW:  Reads from brain_immune_system_t stats and continuous inflammation
 */
typedef struct {
    uint32_t total_exceptions;       /**< Total exceptions processed by immune system */
    uint32_t recovered_exceptions;   /**< Exceptions successfully recovered */
    float inflammation_level;        /**< Continuous inflammation [0.0-1.0] */
    uint32_t active_antibodies;      /**< Number of active antibody responses */

    /* Extended fields appended for monitoring (additive — old callers safe). */
    uint32_t active_t_cells;         /**< Active T-cell count */
    uint32_t active_b_cells;         /**< Active B-cell count */
    uint32_t memory_cells;           /**< Memory cell count */
    uint32_t bbb_threats_processed;  /**< BBB perimeter threats handled */
    float cytokine_il1;              /**< IL-1β concentration [0..1] */
    float cytokine_il6;              /**< IL-6 concentration [0..1] */
    float cytokine_il10;             /**< IL-10 (anti-inflammatory) [0..1] */
    float cytokine_tnf;              /**< TNF-α concentration [0..1] */
    float cytokine_ifn_gamma;        /**< IFN-γ concentration [0..1] */
    float cytokine_il4;              /**< IL-4 (Th2/class switching) [0..1] */
} nimcp_immune_metrics_t;

/**
 * @brief Synapse statistics snapshot
 *
 * WHAT: Current synapse count and growth delta since last probe call
 * WHY:  Tracks network connectivity growth over time
 * HOW:  Reads total synapses from brain_stats, computes delta from last call
 */
typedef struct {
    uint64_t total_synapses;         /**< Current total synapse count */
    int64_t  growth_since_last;      /**< Change since previous call (can be negative if pruned) */
} nimcp_synapse_stats_t;

/**
 * @brief Get process resident set size (RSS) in bytes
 *
 * WHAT: Returns the current process RSS from /proc/self/status (Linux)
 * WHY:  Monitors memory pressure without requiring external tools
 * HOW:  Parses VmRSS line from /proc/self/status; returns 0 on non-Linux
 *
 * @return Process RSS in bytes, or 0 if unavailable
 */
size_t nimcp_brain_get_memory_rss(void);

/**
 * @brief Get approximate GPU VRAM in use by this brain
 *
 * WHAT: Returns tracked GPU memory allocation from the brain's GPU context
 * WHY:  Enables GPU memory pressure monitoring for health dashboards
 * HOW:  Reads allocated_memory from brain's nimcp_gpu_context_t
 *
 * @param brain Brain handle
 * @return GPU VRAM bytes in use, or 0 if no GPU or brain is NULL
 */
size_t nimcp_brain_get_gpu_vram_used(nimcp_brain_t brain);

/**
 * @brief Get fraction of neurons with at least one outgoing synapse
 *
 * WHAT: Measures neuron utilization as connected_neurons / total_neurons
 * WHY:  Low utilization indicates many dead/orphan neurons (wasted capacity)
 * HOW:  Samples every Nth neuron (N = total/15000) via NEURON_OUT_COUNT
 *
 * @param brain Brain handle
 * @return Fraction [0.0-1.0], or 0.0 on error
 */
float nimcp_brain_get_neuron_utilization(nimcp_brain_t brain);

/**
 * @brief Get immune system metrics for the brain
 *
 * WHAT: Fills output struct with immune activity, inflammation, and recovery stats
 * WHY:  Enables external dashboards to monitor brain immune health
 * HOW:  Calls brain_immune_get_stats() on the brain's immune system
 *
 * @param brain Brain handle
 * @param out   Output structure (zeroed on error)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_get_immune_metrics(nimcp_brain_t brain, nimcp_immune_metrics_t* out);

/**
 * @brief Get synapse count and growth since last probe call
 *
 * WHAT: Returns current synapse total and delta from previous invocation
 * WHY:  Tracks network growth rate for adaptive training decisions
 * HOW:  Reads num_synapses from brain stats; stores previous value internally
 *
 * @param brain Brain handle
 * @param out   Output structure (zeroed on error)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_get_synapse_stats(nimcp_brain_t brain, nimcp_synapse_stats_t* out);

//=============================================================================
// Cognitive Output Rubric - Human-Style Quality Evaluation
//=============================================================================

/**
 * @brief Rubric evaluation result — flat public struct
 *
 * Two-tier quality assessment of brain_decide() output:
 *   Tier 1 (Structural): rule-based, automatable checks
 *   Tier 2 (Qualitative): holistic, subsystem-dependent scores
 *
 * Grade scale: A+ (>=0.93) through F (<0.50)
 */
typedef struct {
    /* Tier 1: Structural */
    float internal_consistency;      /**< [0-1] Output vector self-agreement */
    float confidence_calibration;    /**< [0-1] Confidence vs running accuracy */
    float completeness;              /**< [0-1] Non-zero output coverage */
    float reasoning_chain_quality;   /**< [0-1] Explanation depth & structure */
    float epistemic_quality;         /**< [0-1] Evidence quality */
    float ethical_alignment;         /**< [0-1] Golden Rule alignment */
    float tier1_score;               /**< Weighted aggregate of Tier 1 */

    /* Tier 2: Qualitative */
    float originality;               /**< [0-1] Creative novelty */
    float integration_depth;         /**< [0-1] Cross-system breadth */
    float communication_clarity;     /**< [0-1] Label specificity + readability */
    float engagement_quality;        /**< [0-1] Berlyne hedonic + arousal */
    float empathetic_accuracy;       /**< [0-1] Mirror neuron match quality */
    float information_density;       /**< [0-1] Normalized Shannon entropy */
    float tier2_score;               /**< Weighted aggregate of Tier 2 */

    /* Overall */
    float overall_score;             /**< [0-1] Weighted combination of tiers */
    char  grade;                     /**< A/B/C/D/F */
    char  grade_modifier;            /**< +/-/space */
    uint32_t subsystems_available;   /**< Bitmask of available subsystems */
    uint64_t evaluation_time_us;     /**< Wall-clock evaluation time */
} nimcp_rubric_t;

/**
 * @brief Evaluate quality of last brain decision using two-tier rubric
 *
 * WHAT: Grades brain output like a professor grading an essay
 * WHY:  Unified quality metric combining structural + qualitative dimensions
 * HOW:  Calls internal subsystems (epistemic, ethics, aesthetic, mirror neurons)
 *       and aggregates into a single grade. Missing subsystems score 0.5 (neutral).
 *
 * Must be called AFTER nimcp_brain_predict() or nimcp_brain_decide_full().
 *
 * @param brain Brain handle
 * @param rubric Output rubric result
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_rubric(nimcp_brain_t brain, nimcp_rubric_t* rubric);

/**
 * @brief Broadcast rubric data via bio-async message system
 *
 * @param brain Brain handle (must have been rubric'd first)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_broadcast_rubric(nimcp_brain_t brain);

/**
 * @brief Set dedicated validation features for rubric evaluation during training
 *
 * When set, rubric evaluations during training use these features instead of
 * the current training step's features. Features are copied internally.
 *
 * @param brain Brain handle
 * @param features Validation feature array (copied internally)
 * @param num_features Number of features
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_set_rubric_validation(
    nimcp_brain_t brain, const float* features, uint32_t num_features);

/**
 * @brief Get rubric statistics accumulated during training
 *
 * @param brain Brain handle
 * @param eval_count Output: number of rubric evaluations
 * @param min_score Output: minimum observed score
 * @param max_score Output: maximum observed score
 * @param avg_score Output: average score
 * @param last_rubric Output: most recent rubric result (NULL to skip)
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_get_rubric_training_stats(
    nimcp_brain_t brain,
    uint64_t* eval_count, float* min_score, float* max_score,
    float* avg_score, nimcp_rubric_t* last_rubric);

//=============================================================================
// Copy-on-Write (COW) Cache API - Efficient Memory Sharing
//=============================================================================

/**
 * @brief Opaque handle to a brain snapshot (for COW save/restore)
 */
typedef struct nimcp_brain_snapshot_handle* nimcp_brain_snapshot_t;

/**
 * @brief Clone a brain using copy-on-write (COW) caching
 *
 * WHAT: Creates a lightweight clone that shares memory with the original
 * WHY:  Enables efficient brain replication (86% memory savings)
 * HOW:  Uses nimcp_cache_reference() to share large structures (weights, connections)
 *
 * The clone initially shares all large data structures (neural network weights,
 * connections, knowledge base) with the original brain. Memory is only copied
 * when either brain modifies shared data (copy-on-write semantics).
 *
 * PERFORMANCE:
 * - Clone time: <10ms (vs ~1000ms for full copy)
 * - Memory overhead: ~1MB metadata (vs ~50MB full copy)
 * - Memory savings: 86% for replicas, 99% for snapshots
 *
 * THREAD SAFETY: Thread-safe
 * MEMORY: O(1) initially, O(n) after modifications
 *
 * @param original Brain to clone
 * @return Cloned brain handle or NULL on error
 *
 * EXAMPLE:
 * ```c
 * nimcp_brain_t original = nimcp_brain_create(...);
 * nimcp_brain_t clone = nimcp_brain_clone_cow(original);
 * // clone shares memory with original (fast, low memory)
 * nimcp_brain_learn_example(clone, ...);  // Triggers copy on first write
 * ```
 */
nimcp_brain_t nimcp_brain_clone_cow(nimcp_brain_t original);

/**
 * @brief Create instant snapshot of brain state using COW
 *
 * WHAT: Creates zero-copy snapshot for rollback/checkpointing
 * WHY:  Enables instant state capture without memory overhead
 * HOW:  Uses nimcp_cache_reference() to share all brain data
 *
 * Snapshots are instant (<1ms) and use minimal memory (~48 bytes overhead).
 * The original brain can continue learning while snapshot preserves the
 * original state. Snapshot automatically copies data if brain modifies it.
 *
 * PERFORMANCE:
 * - Snapshot time: <1ms (zero-copy)
 * - Memory overhead: ~48 bytes metadata
 * - Memory savings: 99% vs traditional snapshot
 *
 * THREAD SAFETY: Thread-safe
 * MEMORY: O(1) overhead
 *
 * @param brain Brain to snapshot
 * @return Snapshot handle or NULL on error
 *
 * EXAMPLE:
 * ```c
 * nimcp_brain_snapshot_t checkpoint = nimcp_brain_snapshot_cow(brain);
 * train_epochs(brain, 100);
 * if (performance < threshold) {
 *     nimcp_brain_restore_cow(brain, checkpoint);  // Instant rollback
 * }
 * nimcp_brain_snapshot_destroy(checkpoint);
 * ```
 */
nimcp_brain_snapshot_t nimcp_brain_snapshot_cow(nimcp_brain_t brain);

/**
 * @brief Restore brain state from COW snapshot
 *
 * WHAT: Restores brain to snapshot state
 * WHY:  Enables instant rollback to previous state
 * HOW:  Replaces current state with snapshot references
 *
 * The brain's current state is replaced with the snapshot state.
 * This is instant because it just swaps cached pointers.
 *
 * PERFORMANCE: <1ms (pointer swapping)
 * THREAD SAFETY: Thread-safe
 * MEMORY: O(1)
 *
 * @param brain Brain to restore
 * @param snapshot Snapshot to restore from
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_restore_cow(nimcp_brain_t brain, nimcp_brain_snapshot_t snapshot);

/**
 * @brief Destroy brain snapshot and release references
 *
 * @param snapshot Snapshot handle
 */
void nimcp_brain_snapshot_destroy(nimcp_brain_snapshot_t snapshot);

//=============================================================================
// Phase 10.2: Working Memory API - Active Representation Buffer
//=============================================================================

/**
 * @brief Add item to working memory for reasoning
 *
 * Adds a feature vector to the brain's working memory buffer (Miller's 7±2).
 * Items are stored with salience-based priority. When capacity is reached,
 * lowest-salience items are evicted. Items decay over time unless refreshed.
 *
 * WHAT: Store active representation in working memory
 * WHY:  Enable reasoning, planning, and cognitive processing over recent inputs
 * HOW:  Priority queue based on salience, automatic decay, capacity-limited
 *
 * @param brain Brain instance
 * @param data Item data (feature vector)
 * @param size Item size (number of floats)
 * @param salience Initial salience (0.0-1.0, higher = more important)
 * @return NIMCP_OK on success, error code otherwise
 *
 * @note Requires enable_working_memory=true in brain config
 * @note Items with salience < 0.01 are automatically removed during decay
 *
 * Example:
 * ```c
 * float features[64] = {...};
 * nimcp_brain_working_memory_add(brain, features, 64, 0.8);  // High salience
 * ```
 */
nimcp_status_t nimcp_brain_working_memory_add(
    nimcp_brain_t brain,
    const float* data,
    uint32_t size,
    float salience
);

/**
 * @brief Get item from working memory by index
 *
 * Retrieves an item from working memory. Items are ordered by salience
 * (index 0 = highest salience). The returned pointer is valid until
 * the next working memory operation.
 *
 * @param brain Brain instance
 * @param index Item index (0 = highest salience)
 * @param size_out Output: item size (number of floats)
 * @return Item data pointer or NULL if index invalid or working memory disabled
 *
 * Example:
 * ```c
 * uint32_t size;
 * const float* item = nimcp_brain_working_memory_get(brain, 0, &size);
 * if (item) {
 *     // Process highest-salience item
 * }
 * ```
 */
const float* nimcp_brain_working_memory_get(
    nimcp_brain_t brain,
    uint32_t index,
    uint32_t* size_out
);

/**
 * @brief Get working memory statistics
 *
 * Returns current size and capacity of working memory buffer.
 *
 * @param brain Brain instance
 * @param current_size_out Output: current number of items
 * @param capacity_out Output: maximum capacity (typically 7)
 * @return NIMCP_OK on success, error code otherwise
 *
 * Example:
 * ```c
 * uint32_t size, capacity;
 * nimcp_brain_working_memory_stats(brain, &size, &capacity);
 * printf("Working memory: %u/%u items\n", size, capacity);
 * ```
 */
nimcp_status_t nimcp_brain_working_memory_stats(
    nimcp_brain_t brain,
    uint32_t* current_size_out,
    uint32_t* capacity_out
);

/**
 * @brief Refresh item in working memory (prevent decay)
 *
 * Resets the timestamp of an item, preventing temporal decay.
 * This simulates attention/rehearsal in biological working memory
 * (frontal-parietal loop).
 *
 * @param brain Brain instance
 * @param index Item index to refresh
 * @return NIMCP_OK on success, error code otherwise
 *
 * Example:
 * ```c
 * // Keep important item in memory by refreshing it
 * nimcp_brain_working_memory_refresh(brain, 0);  // Refresh highest-salience
 * ```
 */
nimcp_status_t nimcp_brain_working_memory_refresh(
    nimcp_brain_t brain,
    uint32_t index
);

//=============================================================================
// Global Workspace API - Conscious Access and Broadcasting
//=============================================================================

/**
 * @brief Cognitive module identifiers for Global Workspace Theory
 *
 * These modules can compete for conscious access via the global workspace.
 * Based on Global Workspace Theory (Baars, 1988; Dehaene, 2011).
 */
typedef enum {
    NIMCP_MODULE_NONE = 0,
    NIMCP_MODULE_PERCEPTION,
    NIMCP_MODULE_WORKING_MEMORY,
    NIMCP_MODULE_EXECUTIVE,
    NIMCP_MODULE_THEORY_OF_MIND,
    NIMCP_MODULE_ETHICS,
    NIMCP_MODULE_ATTENTION,
    NIMCP_MODULE_EMOTION,
    NIMCP_MODULE_SALIENCE,
    NIMCP_MODULE_MOTOR,
    NIMCP_MODULE_LANGUAGE,
    NIMCP_MODULE_METACOGNITION,
    NIMCP_MODULE_CURIOSITY,
    NIMCP_MODULE_INTROSPECTION,
    NIMCP_MODULE_PREDICTIVE,
    NIMCP_MODULE_CONSOLIDATION,
    NIMCP_MODULE_EPISODIC_MEMORY,
    NIMCP_MODULE_SEMANTIC_MEMORY,
    NIMCP_MODULE_WELLBEING,
    NIMCP_MODULE_MENTAL_HEALTH,
    NIMCP_MODULE_GOAL_MOTIVATION,
    NIMCP_MODULE_COGNITIVE_CONTROL,
    NIMCP_MODULE_CUSTOM_START = 100
} nimcp_cognitive_module_t;

/**
 * @brief Compete for global workspace access
 *
 * WHAT: Submit content to global workspace for potential conscious broadcast
 * WHY:  Enable cross-module information integration via conscious access
 * HOW:  Content competes with other modules; winner gets broadcast
 *
 * @param brain Brain instance
 * @param module Source module identifier
 * @param content Content vector (size = workspace capacity, typically 256 floats)
 * @param content_dim Content dimension (must match workspace capacity)
 * @param strength Competition strength (0.0 to 1.0, higher = more likely to win)
 * @return NIMCP_OK if won and broadcast, NIMCP_ERROR otherwise
 *
 * @note Requires enable_global_workspace=true in brain config
 * @note Refractory period (default 50ms) prevents rapid successive broadcasts
 * @note Ignition threshold (default 0.6) gates conscious access
 *
 * Example:
 * @code
 * float features[256] = {...};
 * nimcp_status_t status = nimcp_brain_workspace_compete(
 *     brain, NIMCP_MODULE_PERCEPTION, features, 256, 0.85
 * );
 * if (status == NIMCP_OK) {
 *     printf("Content reached conscious access!\n");
 * }
 * @endcode
 */
nimcp_status_t nimcp_brain_workspace_compete(
    nimcp_brain_t brain,
    nimcp_cognitive_module_t module,
    const float* content,
    uint32_t content_dim,
    float strength
);

/**
 * @brief Read current global workspace broadcast
 *
 * WHAT: Retrieve content from current conscious broadcast
 * WHY:  Allow modules to access globally broadcast information
 * HOW:  Copy broadcast content to provided buffer
 *
 * @param brain Brain instance
 * @param content Output buffer for broadcast content
 * @param max_dim Maximum buffer size
 * @param actual_dim Output: actual content dimension
 * @param source_module Output: source module of broadcast
 * @return NIMCP_OK if broadcast available, NIMCP_ERROR otherwise
 *
 * Example:
 * @code
 * float content[256];
 * uint32_t dim;
 * nimcp_cognitive_module_t source;
 * if (nimcp_brain_workspace_read(brain, content, 256, &dim, &source) == NIMCP_OK) {
 *     printf("Broadcast from module %d, dimension %u\n", source, dim);
 * }
 * @endcode
 */
nimcp_status_t nimcp_brain_workspace_read(
    nimcp_brain_t brain,
    float* content,
    uint32_t max_dim,
    uint32_t* actual_dim,
    nimcp_cognitive_module_t* source_module
);

/**
 * @brief Subscribe module to workspace broadcasts
 *
 * WHAT: Register module to receive all future broadcasts
 * WHY:  Enable module to stay informed of conscious content
 * HOW:  Add module to subscriber list
 *
 * @param brain Brain instance
 * @param module Module to subscribe
 * @return NIMCP_OK on success, NIMCP_ERROR otherwise
 */
nimcp_status_t nimcp_brain_workspace_subscribe(
    nimcp_brain_t brain,
    nimcp_cognitive_module_t module
);

/**
 * @brief Unsubscribe module from workspace broadcasts
 *
 * @param brain Brain instance
 * @param module Module to unsubscribe
 * @return NIMCP_OK on success, NIMCP_ERROR otherwise
 */
nimcp_status_t nimcp_brain_workspace_unsubscribe(
    nimcp_brain_t brain,
    nimcp_cognitive_module_t module
);

/**
 * @brief Check if workspace has active broadcast
 *
 * @param brain Brain instance
 * @param has_broadcast Output: true if broadcast active
 * @return NIMCP_OK on success, NIMCP_ERROR otherwise
 */
nimcp_status_t nimcp_brain_workspace_has_broadcast(
    nimcp_brain_t brain,
    bool* has_broadcast
);

/**
 * @brief Get workspace statistics
 *
 * @param brain Brain instance
 * @param total_broadcasts Output: total broadcasts since creation
 * @param total_competitions Output: total competition attempts
 * @param avg_strength Output: average broadcast strength
 * @return NIMCP_OK on success, NIMCP_ERROR otherwise
 */
nimcp_status_t nimcp_brain_workspace_stats(
    nimcp_brain_t brain,
    uint32_t* total_broadcasts,
    uint32_t* total_competitions,
    float* avg_strength
);

//=============================================================================
// Neural Network API - Low-Level Interface (Advanced Users)
//=============================================================================

/**
 * @brief Create a neural network with custom configuration
 *
 * @param num_inputs Number of input neurons
 * @param num_outputs Number of output neurons
 * @param num_hidden Number of hidden neurons
 * @param learning_rate Learning rate (typically 0.001 - 0.1)
 * @return Network handle or NULL on error
 */
nimcp_network_t nimcp_network_create(
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t num_hidden,
    float learning_rate
);

/**
 * @brief Destroy a neural network
 *
 * @param network Network handle
 */
void nimcp_network_destroy(nimcp_network_t network);

/**
 * @brief Forward pass through network
 *
 * @param network Network handle
 * @param inputs Input array (size = num_inputs)
 * @param num_inputs Number of inputs
 * @param outputs Output array (size = num_outputs, pre-allocated)
 * @param num_outputs Number of outputs
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_network_forward(
    nimcp_network_t network,
    const float* inputs,
    uint32_t num_inputs,
    float* outputs,
    uint32_t num_outputs
);

/**
 * @brief Train network on a single example (supervised learning)
 *
 * @param network Network handle
 * @param inputs Input array
 * @param num_inputs Number of inputs
 * @param targets Target output array
 * @param num_targets Number of target outputs
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_network_train(
    nimcp_network_t network,
    const float* inputs,
    uint32_t num_inputs,
    const float* targets,
    uint32_t num_targets
);

//=============================================================================
// Ethics Module API
//=============================================================================

/**
 * @brief Create an ethics module
 *
 * @return Ethics module handle or NULL on error
 */
nimcp_ethics_t nimcp_ethics_create(void);

/**
 * @brief Destroy an ethics module
 *
 * @param ethics Ethics module handle
 */
void nimcp_ethics_destroy(nimcp_ethics_t ethics);

/**
 * @brief Check if an action is ethically acceptable
 *
 * @param ethics Ethics module handle
 * @param situation Situation features array
 * @param num_features Number of features
 * @param out_score Ethical score (-1.0 = harmful, 0.0 = neutral, 1.0 = beneficial)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_ethics_check(
    nimcp_ethics_t ethics,
    const float* situation,
    uint32_t num_features,
    float* out_score
);

//=============================================================================
// Knowledge Graph API
//=============================================================================

/**
 * @brief Create a knowledge graph
 *
 * @return Knowledge graph handle or NULL on error
 */
nimcp_knowledge_t nimcp_knowledge_create(void);

/**
 * @brief Destroy a knowledge graph
 *
 * @param knowledge Knowledge graph handle
 */
void nimcp_knowledge_destroy(nimcp_knowledge_t knowledge);

/**
 * @brief Add a fact to the knowledge graph
 *
 * @param knowledge Knowledge graph handle
 * @param subject Subject entity
 * @param predicate Relationship type
 * @param object Object entity
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_knowledge_add_fact(
    nimcp_knowledge_t knowledge,
    const char* subject,
    const char* predicate,
    const char* object
);

/**
 * @brief Query the knowledge graph
 *
 * @param knowledge Knowledge graph handle
 * @param query Query string
 * @param out_result Result buffer (pre-allocated, min 1024 bytes)
 * @param max_result_len Maximum result length
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_knowledge_query(
    nimcp_knowledge_t knowledge,
    const char* query,
    char* out_result,
    uint32_t max_result_len
);

//=============================================================================
// Complex Number & Oscillation API - Phase Coding & Neural Synchrony
//=============================================================================

/**
 * @brief Oscillation phasor (complex number) representation
 *
 * Represents neural oscillations as amplitude + phase:
 * - amplitude: Strength of oscillation (magnitude)
 * - phase: Timing within oscillation cycle (radians, -π to π)
 *
 * Mathematical form: z = amplitude·e^(i·phase)
 */
typedef struct {
    float amplitude;  /**< Oscillation amplitude (≥ 0) */
    float phase;      /**< Phase angle in radians (-π to π) */
} nimcp_oscillation_phasor_t;

/**
 * @brief Enable or disable complex oscillation features
 *
 * WHAT: Activates phase coding and complex number support in brain
 * WHY:  Complex oscillations enable phase-based memory and binding
 * HOW:  Configures neurons/synapses to track amplitude + phase
 *
 * When enabled:
 * - Neurons track oscillatory phase and amplitude
 * - Synapses compute phase-dependent weights
 * - Phase coherence and PAC metrics become available
 * - ~15% memory overhead for phase tracking
 *
 * Performance impact:
 * - Memory: +15% (phase data storage)
 * - Compute: +10% (complex arithmetic)
 * - Benefits: Phase-based memory, theta-gamma coupling
 *
 * @param brain Brain handle
 * @param enable true to enable, false to disable
 * @return true on success, false on error
 *
 * EXAMPLE:
 * ```c
 * nimcp_brain_t brain = nimcp_brain_create(...);
 * nimcp_enable_complex_oscillations(brain, true);
 * // Now can query phase coherence, PAC, etc.
 * ```
 */
bool nimcp_enable_complex_oscillations(nimcp_brain_t brain, bool enable);

/**
 * @brief Check if complex oscillation features are enabled
 *
 * @param brain Brain handle
 * @return true if enabled, false if disabled or error
 */
bool nimcp_is_complex_oscillations_enabled(nimcp_brain_t brain);

/**
 * @brief Get oscillation phasor for a specific neuron
 *
 * WHAT: Retrieves amplitude and phase of neuron's oscillatory activity
 * WHY:  Enables analysis of neural phase coding and synchronization
 * HOW:  Extracts complex phasor from neuron's oscillation state
 *
 * Returns the neuron's current oscillatory state as a phasor.
 * Requires complex oscillations to be enabled.
 *
 * @param brain Brain handle
 * @param neuron_id Neuron identifier
 * @return Phasor with amplitude and phase (returns {0,0} on error or if disabled)
 *
 * EXAMPLE:
 * ```c
 * nimcp_oscillation_phasor_t phasor = nimcp_get_oscillation_phasor(brain, 42);
 * printf("Neuron 42: amplitude=%.2f, phase=%.2f rad\n",
 *        phasor.amplitude, phasor.phase);
 * ```
 */
nimcp_oscillation_phasor_t nimcp_get_oscillation_phasor(
    nimcp_brain_t brain,
    uint32_t neuron_id
);

/**
 * @brief Compute phase coherence across multiple neurons
 *
 * WHAT: Measures phase synchronization between neural oscillations
 * WHY:  Phase coherence indicates coordinated neural activity
 * HOW:  Computes inter-trial phase coherence (ITPC) across neurons
 *
 * Phase coherence range: [0, 1] where:
 * - 0.0 = random phases (no synchronization)
 * - 0.3 = weak synchronization
 * - 0.6 = moderate synchronization
 * - 1.0 = perfect phase locking
 *
 * Neuroscience application:
 * - High coherence → neurons fire at same phase (coordinated)
 * - Low coherence → independent oscillations
 * - Used to detect neural binding and communication
 *
 * @param brain Brain handle
 * @param neuron_ids Array of neuron identifiers to analyze
 * @param count Number of neurons in array
 * @return Phase coherence value [0, 1] (returns 0.0 on error or if disabled)
 *
 * EXAMPLE:
 * ```c
 * uint32_t neurons[] = {10, 20, 30, 40, 50};
 * float coherence = nimcp_get_phase_coherence(brain, neurons, 5);
 * if (coherence > 0.6f) {
 *     printf("High synchronization detected!\n");
 * }
 * ```
 */
float nimcp_get_phase_coherence(
    nimcp_brain_t brain,
    const uint32_t* neuron_ids,
    uint32_t count
);

/**
 * @brief Compute phase-amplitude coupling (PAC) modulation index
 *
 * WHAT: Measures theta-gamma cross-frequency coupling strength
 * WHY:  PAC is a key mechanism for memory and attention
 * HOW:  Computes modulation of gamma amplitude by theta phase
 *
 * PAC modulation index range: [0, 1] where:
 * - 0.0 = no coupling (theta and gamma independent)
 * - 0.2 = weak coupling
 * - 0.4 = moderate coupling
 * - 0.6+ = strong coupling (indicates active encoding/retrieval)
 *
 * Neuroscience foundation:
 * - Hippocampal theta (4-8Hz) phase modulates gamma (30-100Hz) amplitude
 * - Used for sequence encoding (place cells), working memory
 * - Higher PAC correlates with better memory performance
 *
 * @param brain Brain handle
 * @param theta_freq Theta frequency in Hz (typically 4-8 Hz)
 * @param gamma_freq Gamma frequency in Hz (typically 30-100 Hz)
 * @return PAC modulation index [0, 1] (returns 0.0 on error or if disabled)
 *
 * EXAMPLE:
 * ```c
 * // Measure hippocampal theta-gamma coupling
 * float pac = nimcp_get_pac_modulation(brain, 6.0f, 40.0f);
 * if (pac > 0.4f) {
 *     printf("Strong theta-gamma coupling (memory encoding active)\n");
 * }
 * ```
 */
float nimcp_get_pac_modulation(
    nimcp_brain_t brain,
    float theta_freq,
    float gamma_freq
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error message for last error
 *
 * @return Error message string (statically allocated)
 */
const char* nimcp_get_error(void);

/**
 * @brief Check if the linked library version is compatible with the header version.
 * @return NIMCP_OK if compatible, NIMCP_ERROR if major version mismatch
 */
nimcp_status_t nimcp_version_check(void);

/**
 * @brief Initialize NIMCP library (call once at startup)
 *
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_init(void);

/**
 * @brief Shutdown NIMCP library (call once at cleanup)
 */
void nimcp_shutdown(void);

// === TRAINING MODE & ABLATION ===

/**
 * @brief Enable/disable training-mode fast path in brain_decide()
 * When active, expensive cognitive modules (reasoning, dialogue, imagination,
 * ethics, ToM, etc.) are skipped for 2-5x training speedup.
 */
void nimcp_brain_set_training_mode(nimcp_brain_t brain, bool active);

/* Per-network training ablation (Apr 11 2026).
 * Toggle whether a given sub-network participates in brain_learn_vector.
 * Reads occur every training call so flips take effect immediately —
 * no rebuild or restart required after initial deployment.
 *
 * Example: disable CNN training while keeping ANN/SNN/LNN active:
 *   nimcp_brain_set_train_cnn(brain, false);
 *
 * snn_only_recovery is a convenience preset equivalent to
 *   set_train_ann(false); set_train_cnn(false); set_train_lnn(false);
 * while leaving train_snn unchanged. Used to let the SNN re-converge
 * against a frozen ensemble after large BPTT behavior changes.
 */
void nimcp_brain_set_train_ann(nimcp_brain_t brain, bool enabled);
bool nimcp_brain_get_train_ann(nimcp_brain_t brain);
void nimcp_brain_set_train_cnn(nimcp_brain_t brain, bool enabled);
bool nimcp_brain_get_train_cnn(nimcp_brain_t brain);
void nimcp_brain_set_train_snn(nimcp_brain_t brain, bool enabled);
bool nimcp_brain_get_train_snn(nimcp_brain_t brain);
void nimcp_brain_set_train_lnn(nimcp_brain_t brain, bool enabled);
bool nimcp_brain_get_train_lnn(nimcp_brain_t brain);
void nimcp_brain_set_snn_only_recovery(nimcp_brain_t brain, bool enabled);
bool nimcp_brain_get_snn_only_recovery(nimcp_brain_t brain);

/* Ensemble warmup scale [0.0, 1.0] — probabilistic gate on non-SNN
 * training. 1.0 = full-rate (default), 0.0 = fully frozen (same effect
 * as snn_only_recovery for non-SNN networks), intermediate = Monte-Carlo
 * skip. Used to ramp ANN/CNN/LNN back in gradually after snn-only
 * recovery to avoid co-adaptation shock. Clamped to [0.0, 1.0]. */
void nimcp_brain_set_ensemble_warmup_scale(nimcp_brain_t brain, float scale);
float nimcp_brain_get_ensemble_warmup_scale(nimcp_brain_t brain);

/**
 * @brief Eagerly initialize all cognitive subsystems that brain_decide() would lazy-init.
 *
 * BRAIN_ENSURE_* macros in brain_decide() are NOT thread-safe. Call this once after
 * brain load (before concurrent access) to eliminate lazy-init race conditions.
 * Initializes: working memory, executive, symbolic logic, global workspace,
 * mirror neurons, glial, theory of mind, ethics, FEP orchestrator.
 *
 * @param brain Brain handle
 * @return Number of subsystems newly initialized (0-9), -1 on error
 */
int nimcp_brain_eager_init_cognitive(nimcp_brain_t brain);

/* ============================================================================
 * Unified Brain Probe System
 * ============================================================================ */

/** Probe metric value (matches probe_metric_t in nimcp_brain_probes.h) */
typedef struct nimcp_probe_metric {
    char     key[64];
    int      type;        /**< 0=float, 1=int, 2=string */
    union {
        float   f;
        int64_t i;
        char    s[64];
    } value;
    uint64_t timestamp_us;
} nimcp_probe_metric_t;

/**
 * @brief Create a probe that monitors brain modules or pipeline stages.
 * @param module_ids   Array of bio_module_id values (0 = use built-in)
 * @param num_modules  Number of module IDs
 * @param interval_ms  Sampling interval (0 = event-driven only)
 * @param mode         1=active, 2=passive, 3=hybrid
 * @param out_handle   Receives probe handle
 * @return 0 on success
 */
int nimcp_brain_create_probe(nimcp_brain_t brain,
    const uint16_t* module_ids, uint32_t num_modules,
    uint32_t interval_ms, uint32_t mode,
    uint32_t* out_handle);

/**
 * @brief Get metrics from a single probe.
 * @param handle       Probe handle from create_probe
 * @param out          Caller-allocated metric array
 * @param max          Size of out array
 * @param count        Receives number of metrics written
 * @return 0 on success
 */
int nimcp_brain_get_probe_metrics(nimcp_brain_t brain,
    uint32_t handle,
    nimcp_probe_metric_t* out, uint32_t max, uint32_t* count);

/**
 * @brief Get all probe metrics as JSON string.
 * @param out_json     Receives heap-allocated JSON (caller must free)
 * @return 0 on success
 */
int nimcp_brain_get_all_probe_metrics_json(nimcp_brain_t brain, char** out_json);

/** @brief Destroy a probe. */
void nimcp_brain_destroy_probe(nimcp_brain_t brain, uint32_t handle);

/** @brief Attach built-in probes (network, cognitive, dashboard, inference). */
int nimcp_brain_attach_builtin_probes(nimcp_brain_t brain, uint32_t interval_ms);

/**
 * @brief Enable/disable individual network types for ablation studies
 * @param train_cnn -1 = don't change, 0 = disable, 1 = enable
 * @param train_snn -1 = don't change, 0 = disable, 1 = enable
 * @param train_lnn -1 = don't change, 0 = disable, 1 = enable
 */
void nimcp_brain_set_network_ablation(nimcp_brain_t brain,
                                       int train_cnn, int train_snn, int train_lnn);

/**
 * @brief Get per-network training metrics for ablation analysis
 */
bool nimcp_brain_get_network_metrics(nimcp_brain_t brain,
                                      float* ema_ann, float* ema_cnn,
                                      float* ema_snn, float* ema_lnn,
                                      uint64_t* ann_steps, uint64_t* cnn_steps,
                                      uint64_t* snn_steps, uint64_t* lnn_steps);

// =========================================================================
// BINDING-LANGUAGE PUBLIC API (sensory, config, brain state, sub-network stats)
// =========================================================================

/** @brief Stage sensory data for the next decide_full() call */
nimcp_status_t nimcp_brain_submit_sensory(
    nimcp_brain_t brain,
    const char* modality,
    const float* data, uint32_t num_elements,
    uint32_t width, uint32_t height, uint32_t channels,
    uint32_t n_segments);

/** @brief Process image through visual cortex, return feature vector */
nimcp_status_t nimcp_brain_visual_cortex_process(
    nimcp_brain_t brain,
    const float* pixels, uint32_t num_pixels,
    uint32_t width, uint32_t height, uint32_t channels,
    float* out_features, uint32_t max_features,
    uint32_t* out_feature_count);

/** @brief Get per-cortex CNN processor metrics (up to 4 cortices) */
nimcp_status_t nimcp_brain_get_cortex_cnn_metrics(
    nimcp_brain_t brain,
    int* out_types, float* out_losses,
    uint64_t* out_fwd_steps, uint64_t* out_bwd_steps,
    float* out_embed_norms, uint32_t* out_count);

/** @brief Create NCP-architecture LNN temporal processor */
nimcp_status_t nimcp_brain_lnn_create(
    nimcp_brain_t brain,
    uint32_t n_sensory, uint32_t n_inter,
    uint32_t n_command, uint32_t n_output);

/** @brief Get LNN network statistics */
nimcp_status_t nimcp_brain_lnn_get_stats(
    nimcp_brain_t brain,
    uint64_t* out_forward_steps, uint64_t* out_backward_steps,
    uint64_t* out_ode_evals, float* out_avg_tau,
    float* out_state_norm, float* out_gradient_norm,
    uint32_t* out_nan_count, uint32_t* out_inf_count);

/** @brief Get SNN network statistics */
nimcp_status_t nimcp_brain_snn_get_stats(
    nimcp_brain_t brain,
    uint64_t* out_total_steps, uint64_t* out_total_spikes,
    float* out_mean_firing_rate, float* out_sparsity,
    float* out_synchrony, uint32_t* out_silent_neurons,
    uint32_t* out_hyperactive_neurons, int* out_health,
    size_t* out_memory_bytes);

/** @brief Get CNN trainer statistics */
nimcp_status_t nimcp_brain_cnn_get_stats(
    nimcp_brain_t brain,
    uint32_t* out_num_layers, size_t* out_num_parameters,
    uint32_t* out_num_labels, bool* out_active);

/** @brief Toggle fast training mode */
nimcp_status_t nimcp_brain_set_fast_training(nimcp_brain_t brain, bool enabled);

/** @brief Set task strategy: "regression"/"classification"/"pattern"/"association" */
nimcp_status_t nimcp_brain_set_task_type(nimcp_brain_t brain, const char* task_type);

/** @brief Enable/disable biological plasticity (TPB+EDP+coordinator) */
nimcp_status_t nimcp_brain_enable_biological_plasticity(nimcp_brain_t brain, bool enabled);

/** @brief Enable multi-network ensemble training */
nimcp_status_t nimcp_brain_enable_multi_network(nimcp_brain_t brain);

/** @brief Get medulla arousal level [0,1] */
float nimcp_brain_medulla_get_arousal(nimcp_brain_t brain);

/** @brief Get sleep pressure [0,1] */
float nimcp_brain_sleep_get_pressure(nimcp_brain_t brain);

/** @brief Get basal ganglia dopamine level */
float nimcp_brain_bg_get_dopamine(nimcp_brain_t brain);

/** @brief Get substrate health status ("OPTIMAL"/"STRESSED"/"COMPROMISED"/"CRITICAL"/"UNKNOWN") */
nimcp_status_t nimcp_brain_substrate_get_health(
    nimcp_brain_t brain,
    char* out_status, uint32_t max_len);

/** @brief Focus attention on a sensory modality */
nimcp_status_t nimcp_brain_focus_attention(
    nimcp_brain_t brain,
    const char* modality);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_H
