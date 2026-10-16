// nimcp_brain_part_helpers.c - helpers functions
// Part of nimcp_brain.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain.c

#include "training/nimcp_cortex_cnn.h"


/**
 * @brief Publish brain state change event via bio-async
 *
 * @param event_type Type of brain event (creation, destruction, etc.)
 * @param neuron_count Number of neurons (0 if not applicable)
 * @param channel Neuromodulator channel to use
 */
static void brain_publish_state_event(bio_message_type_t event_type,
                                       uint32_t neuron_count,
                                       nimcp_bio_channel_type_t channel)
{
    // Use atomic load for thread-safe access to global state
    if (!__atomic_load_n(&g_brain_bio_initialized, __ATOMIC_ACQUIRE) ||
        !__atomic_load_n(&g_brain_bio_ctx, __ATOMIC_ACQUIRE)) {
        return;  // Graceful degradation
    }

    LOG_MODULE_DEBUG("BRAIN", "Publishing state event: type=%d, neurons=%u",
                     event_type, neuron_count);

    // Create brain state response message
    bio_msg_brain_state_response_t msg = {0};
    bio_msg_init_header(&msg.header, event_type, BIO_MODULE_BRAIN,
                        0, sizeof(msg));  // target=0 (broadcast)
    msg.header.channel = channel;
    msg.neuron_count = neuron_count;

    // Publish via router
    nimcp_error_t result = bio_router_broadcast(g_brain_bio_ctx, &msg, sizeof(msg));
    if (result != NIMCP_SUCCESS) {
        LOG_MODULE_WARN("BRAIN", "Failed to publish state event: error=%d", result);
    }
}


//=============================================================================
// Strategy Pattern - Task-Specific Behaviors
//=============================================================================

//=============================================================================
// Strategy Implementations
//=============================================================================

/**
 * @brief Classification strategy - softmax output, cross-entropy loss
 *
 * WHY: Classification needs probabilities summing to 1.0
 * WHEN: Multi-class or binary classification tasks
 * COMPLEXITY: O(n) for softmax normalization
 */
static float strategy_classification_lr(void)
{
    return 0.01F;
}


static void strategy_classification_transform(float* output, uint32_t size)
{
    // P1-52 FIX: Guard against NULL output pointer
    if (!output) return;
    // Guard: size=0 would cause OOB read on output[0]
    if (size == 0) return;

    // Softmax normalization for probability distribution
    float max_val = output[0];
    for (uint32_t i = 1; i < size; i++) {
        if (output[i] > max_val)
            max_val = output[i];
    }

    float sum = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        output[i] = expf(output[i] - max_val);
        sum += output[i];
    }

    for (uint32_t i = 0; i < size; i++) {
        output[i] /= sum;
    }
}


static float strategy_classification_loss(const float* pred, const float* target, uint32_t size)
{
    // Cross-entropy loss
    float loss = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        if (target[i] > 0.0F) {
            loss -= target[i] * logf(fmaxf(pred[i], 1e-10F));
        }
    }
    return loss;
}


/**
 * @brief Regression strategy - linear output, MSE loss
 *
 * WHY: Regression predicts continuous values
 * WHEN: Predicting real-valued outputs
 * COMPLEXITY: O(n) for MSE calculation
 */
static float strategy_regression_lr(void)
{
    return 0.005F;
}


static void strategy_regression_transform(float* output, uint32_t size)
{
    // No transformation - use raw values
    (void) output;
    (void) size;
}


static float strategy_regression_loss(const float* pred, const float* target, uint32_t size)
{
    // Guard: Prevent division by zero when size is 0
    if (size == 0) return 0.0f;

    // Mean squared error
    float loss = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        float diff = pred[i] - target[i];
        loss += diff * diff;
    }
    return loss / size;
}


/**
 * @brief Pattern matching strategy - high LR, binary output
 *
 * WHY: Pattern matching needs fast adaptation
 * WHEN: Recognizing specific patterns quickly
 * COMPLEXITY: O(n)
 */
static float strategy_pattern_lr(void)
{
    return 0.02F;
}


static void strategy_pattern_transform(float* output, uint32_t size)
{
    // Threshold to binary
    for (uint32_t i = 0; i < size; i++) {
        output[i] = output[i] > 0.5F ? 1.0F : 0.0F;
    }
}


static float strategy_pattern_loss(const float* pred, const float* target, uint32_t size)
{
    // Guard: Prevent division by zero when size is 0
    if (size == 0) return 0.0f;

    // Binary cross-entropy
    float loss = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        float p = fmaxf(fminf(pred[i], 0.9999F), 0.0001F);
        loss -= target[i] * logf(p) + (1.0F - target[i]) * logf(1.0F - p);
    }
    return loss / size;
}


/**
 * @brief Association learning strategy - Hebbian-focused
 *
 * WHY: Association learning uses different plasticity rules
 * WHEN: Building associative memories
 * COMPLEXITY: O(n)
 */
static float strategy_association_lr(void)
{
    return 0.05F;
}


static void strategy_association_transform(float* output, uint32_t size)
{
    // Normalize to unit range
    float max_val = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        if (fabsf(output[i]) > max_val)
            max_val = fabsf(output[i]);
    }

    if (max_val > 0.0F) {
        for (uint32_t i = 0; i < size; i++) {
            output[i] /= max_val;
        }
    }
}


static float strategy_association_loss(const float* pred, const float* target, uint32_t size)
{
    // Cosine distance
    float dot = 0.0F, norm_p = 0.0F, norm_t = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        dot += pred[i] * target[i];
        norm_p += pred[i] * pred[i];
        norm_t += target[i] * target[i];
    }
    float cosine = dot / (sqrtf(norm_p) * sqrtf(norm_t) + 1e-10F);
    return 1.0F - cosine;
}

static float get_default_sparsity(brain_size_t size)
{
    switch (size) {
        case BRAIN_SIZE_TINY:
            return 0.70F;
        case BRAIN_SIZE_SMALL:
            return 0.80F;
        case BRAIN_SIZE_MEDIUM:
            return 0.85F;
        case BRAIN_SIZE_LARGE:
            return 0.90F;
        default:
            return 0.80F;
    }
}


//=============================================================================
// Configuration Builders
//=============================================================================

/**
 * @brief Build spike parameters for brain configuration
 *
 * WHY: Separates spike config from main creation logic
 * Makes configuration more maintainable and testable
 *
 * COMPLEXITY: O(1)
 *
 * @param sparsity_target Target sparsity level
 * @return Spike parameters structure
 */
static adaptive_spike_params_t build_spike_params(float sparsity_target)
{
    adaptive_spike_params_t params = {0};
    params.k_factor = 0.5F;
    params.sparsity_target = sparsity_target;
    params.encoding = SPIKE_ENCODING_PASSTHROUGH;  /* Raw floats for gradient training */
    params.enable_soft_reset = true;
    params.enable_adaptation = true;
    params.adaptation_window = 100;
    params.min_threshold = 0.0001F;
    params.max_threshold = 10.0F;

    return params;
}

static bool is_cached_input(brain_t brain, const float* features, uint32_t num_features)
{
    if (!brain->last_input || !brain->cached_decision) {
        return false;
    }
    if (brain->input_size != num_features) {
        return false;
    }

    return memcmp(brain->last_input, features, num_features * sizeof(float)) == 0;
}


/**
 * @brief Cache decision for input
 *
 * WHY: Store decision for potential reuse
 * Improves batch processing performance
 *
 * COMPLEXITY: O(n) for input copy
 *
 * @param brain Brain handle
 * @param features Input to cache
 * @param num_features Feature count
 * @param decision Decision to cache
 */
static void cache_decision(brain_t brain, const float* features, uint32_t num_features,
                           brain_decision_t* decision)
{
    // CRITICAL: This function must only be called while cache_mutex is locked!
    // Caller is responsible for mutex protection.

    // Resize input buffer if needed (defensive: handle size changes)
    if (!brain->last_input || brain->input_size != num_features) {
        // Allocate new buffer BEFORE freeing old one to maintain consistency
        float* new_input = nimcp_malloc(num_features * sizeof(float));
        if (!new_input) {
            set_error("Failed to allocate cache input buffer");
            return;
        }

        // Free old buffer after successful allocation
        nimcp_free(brain->last_input);
        brain->last_input = new_input;
        brain->input_size = num_features;
    }

    memcpy(brain->last_input, features, num_features * sizeof(float));

    // Create new decision copy FIRST (before freeing old)
    // This reduces the race window where cached_decision could be NULL
    // Deep copy: cache owns its own data independently
    // Thread safety is ensured by cache_mutex held by caller
    brain_decision_t* new_cached = copy_decision_deep(decision);
    if (!new_cached) {
        set_error("Failed to copy decision for cache");
        return;
    }

    // Now atomically replace old cached decision
    brain_decision_t* old_cached = brain->cached_decision;
    brain->cached_decision = new_cached;

    // Free old decision AFTER replacement (cache always has valid decision)
    if (old_cached) {
        brain_free_decision(old_cached);
    }
}

static int mutex_lock_with_timeout(nimcp_platform_mutex_t* mutex, uint64_t timeout_us)
{
    if (!mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mutex is NULL");

        return -1;
    }

    uint64_t start_time = nimcp_time_monotonic_us();
    uint64_t elapsed = 0;
    uint32_t backoff_us = 100;  // Start with 100 microsecond backoff

    while (elapsed < timeout_us) {
        // Try to acquire lock without blocking
        int result = nimcp_platform_mutex_trylock(mutex);
        if (result == 0) {
            // Successfully acquired lock
            return 0;
        }

        // Lock not available - wait with exponential backoff
        usleep(backoff_us);

        // Exponential backoff with cap at 10ms
        backoff_us = (backoff_us * 2 > 10000) ? 10000 : backoff_us * 2;

        elapsed = nimcp_time_monotonic_us() - start_time;
    }

    // Timeout - log critical error
    LOG_MODULE_ERROR("BRAIN", "CRITICAL: Mutex lock timeout after %lu us - potential deadlock detected",
                     (unsigned long)timeout_us);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mutex_lock_with_timeout: operation failed");
    return -1;
}


/**
 * @brief Force unlock mutex with recovery logging
 *
 * WHAT: Attempts emergency mutex unlock when normal unlock fails
 * WHY:  Recover from potential deadlock situations
 * HOW:  Multiple unlock attempts with logging for diagnostics
 *
 * WARNING: This should only be called as a last resort when normal
 * unlock fails. It may indicate a programming error (unlocking mutex
 * not owned by this thread) or memory corruption.
 *
 * @param mutex Mutex to force unlock
 * @param context Description of where the issue occurred
 */
static void force_unlock_with_logging(nimcp_platform_mutex_t* mutex, const char* context)
{
    if (!mutex) {
        return;
    }

    // Attempt unlock multiple times (some implementations may require it
    // if mutex was locked recursively or corrupted)
    for (int attempt = 0; attempt < 3; attempt++) {
        int result = nimcp_platform_mutex_unlock(mutex);
        if (result == 0) {
            LOG_MODULE_WARN("BRAIN", "Emergency unlock succeeded on attempt %d at %s",
                             attempt + 1, context);
            return;
        }
    }

    // All attempts failed - log critical error
    LOG_MODULE_ERROR("BRAIN", "CRITICAL: Emergency unlock failed after 3 attempts at %s - "
                    "system may be in inconsistent state. Consider process restart.",
                    context);

    // Set error for caller to handle
    set_error("CRITICAL: Mutex permanently locked at %s - restart recommended", context);
}


//=============================================================================
// Brain Factory - Creation with Validation
//=============================================================================

/**
 * @brief Validate brain creation parameters
 *
 * WHY: Guard clause pattern - early exit on invalid input
 * Prevents invalid state propagation
 *
 * COMPLEXITY: O(1)
 *
 * @param task_name Brain name
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @return true if valid
 */
static bool validate_creation_params(const char* task_name, uint32_t num_inputs,
                                     uint32_t num_outputs)
{
    if (!task_name) {
        set_error("task_name cannot be NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_creation_params: task_name is NULL");
        return false;
    }

    if (num_inputs == 0) {
        set_error("num_inputs must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_creation_params: num_inputs is zero");
        return false;
    }

    if (num_inputs > 10000) {
        set_error("num_inputs must be <= 10000");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_creation_params: validation failed");
        return false;
    }

    if (num_outputs == 0) {
        set_error("num_outputs must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_creation_params: num_outputs is zero");
        return false;
    }

    if (num_outputs > 10000) {
        set_error("num_outputs must be <= 10000");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_creation_params: validation failed");
        return false;
    }

    return true;
}



/**
 * @brief Generate or configure personality from brain config
 *
 * WHAT: Create personality profile based on configuration
 * WHY:  Each brain needs unique personality for individuality
 * HOW:  Random generation or explicit specification
 *
 * @param config Brain configuration with personality settings
 * @return Allocated personality profile or NULL on error
 *
 * COMPLEXITY: O(1)
 */
static personality_profile_t* create_personality(const brain_config_t* config)
{
    // Guard: NULL check
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }

    // Allocate personality profile
    personality_profile_t* profile = nimcp_malloc(sizeof(personality_profile_t));
    if (!profile) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "profile is NULL");

        return NULL;

    }

    // Generate personality based on configuration
    if (config->use_random_personality) {
        // Random generation with configured probabilities
        personality_generation_config_t gen_config;
        gen_config.trait_mean = config->personality_trait_mean;
        gen_config.trait_stddev = config->personality_trait_stddev;
        gen_config.female_probability = config->female_probability;
        gen_config.male_probability = config->male_probability;
        gen_config.non_binary_probability = config->non_binary_probability;
        gen_config.seed = config->personality_seed;
        gen_config.enforce_balanced_traits = false;

        *profile = personality_generate_random(&gen_config);
    } else {
        // Explicit specification
        personality_traits_t traits;
        traits.openness = config->explicit_openness;
        traits.conscientiousness = config->explicit_conscientiousness;
        traits.extraversion = config->explicit_extraversion;
        traits.agreeableness = config->explicit_agreeableness;
        traits.neuroticism = config->explicit_neuroticism;

        identity_profile_t identity = {0};
        identity.gender = (gender_identity_t)config->explicit_gender;
        identity.sexuality = (sexual_orientation_t)config->explicit_sexuality;
        identity.gender_certainty = 1.0F;
        identity.sexuality_certainty = 1.0F;
        identity.gender_is_core_identity = true;
        identity.sexuality_is_core_identity = false;

        *profile = personality_create_custom(&traits, &identity);
    }

    return profile;
}


/**
 * @brief Convert label to one-hot encoded output vector
 *
 * WHY: Transforms string labels to neural network targets
 * One-hot encoding standard for classification
 *
 * COMPLEXITY: O(n) where n = num_outputs
 *
 * @param brain Brain handle
 * @param label Label string
 * @param output Output buffer
 * @param confidence Confidence value for label
 */
static void label_to_output(brain_t brain, const char* label, float* output, float confidence)
{
    uint32_t label_idx = get_or_create_label_index(brain, label);

    /* Guard: OOB label index → clamp to last valid output */
    if (label_idx >= brain->config.num_outputs) {
        LOG_WARN(LOG_MODULE, "label_to_output: label_idx %u >= num_outputs %u, clamping",
                 label_idx, brain->config.num_outputs);
        if (brain->config.num_outputs == 0) return;
        label_idx = brain->config.num_outputs - 1;
    }

    memset(output, 0, brain->config.num_outputs * sizeof(float));
    output[label_idx] = confidence;
}


/**
 * WHAT: Adapt learning rate based on loss trend (Phase 11: Simple Meta-Learning)
 * WHY:  Accelerate when loss decreasing, slow down when loss increasing
 * HOW:  Track loss in rolling window, compute trend, adjust LR
 *
 * COMPLEXITY: O(1)
 *
 * BIOLOGICAL BASIS:
 * - Meta-learning: "learning to learn"
 * - Homeostatic regulation of synaptic plasticity
 */
static void adapt_learning_rate_from_loss(brain_t brain, float current_loss)
{
    // Guard: NULL check
    if (!brain) {
        return;
    }

    // Guard: Initialize base_learning_rate on first call
    if (brain->base_learning_rate == 0.0F) {
        brain->base_learning_rate = brain->config.learning_rate;
    }

    // P3-50 FIX: Replace magic number 10 with named constant LOSS_HISTORY_SIZE
    // Store current loss in circular buffer
    brain->loss_history[brain->loss_history_index] = current_loss;
    brain->loss_history_index = (brain->loss_history_index + 1) % LOSS_HISTORY_SIZE;
    if (brain->loss_history_count < LOSS_HISTORY_SIZE) {
        brain->loss_history_count++;
    }

    // Need at least 3 samples to compute trend
    if (brain->loss_history_count < 3) {
        return;
    }

    // P2-52 FIX: Read loss_history in correct chronological order.
    // The circular buffer wraps around, so entry at raw index [0] may not be
    // the oldest. Compute the start offset for correct chronological access.
    uint32_t start_offset = (brain->loss_history_count < LOSS_HISTORY_SIZE)
        ? 0 : brain->loss_history_index;  // Points to oldest entry when full

    // Compute loss trend: recent avg vs older avg
    float recent_avg = 0.0F;
    float older_avg = 0.0F;
    uint32_t half = brain->loss_history_count / 2;

    // Older half (chronologically first entries)
    for (uint32_t i = 0; i < half; i++) {
        older_avg += brain->loss_history[(start_offset + i) % LOSS_HISTORY_SIZE];
    }
    older_avg /= half;

    // Recent half (chronologically later entries)
    for (uint32_t i = half; i < brain->loss_history_count; i++) {
        recent_avg += brain->loss_history[(start_offset + i) % LOSS_HISTORY_SIZE];
    }
    recent_avg /= (brain->loss_history_count - half);

    // Compute trend
    float trend = recent_avg - older_avg;

    // Adapt learning rate
    if (trend < -0.01F) {
        brain->config.learning_rate *= 1.05F;  // Accelerate
    } else if (trend > 0.01F) {
        brain->config.learning_rate *= 0.9F;   // Slow down
    }

    // Bounds: [0.1x, 10x] of base rate
    float min_lr = brain->base_learning_rate * 0.1F;
    float max_lr = brain->base_learning_rate * 10.0F;
    if (brain->config.learning_rate < min_lr) {
        brain->config.learning_rate = min_lr;
    }
    if (brain->config.learning_rate > max_lr) {
        brain->config.learning_rate = max_lr;
    }
}


/**
 * @brief Energy function for quantum annealing weight optimization
 *
 * WHAT: Compute L2 regularization energy for given weights
 * WHY:  Simple proxy energy function for weight optimization
 * HOW:  Sum of squared weights, normalized by dimension
 *
 * NOTE: Full implementation would use validation loss
 *
 * @param weights Weight vector
 * @param dim Vector dimension
 * @param user_data Unused
 * @return Energy (lower is better)
 */
static float quantum_weight_energy(const float* weights, uint32_t dim, void* user_data)
{
    (void)user_data;  // Unused
    float energy = 0.0F;
    for (uint32_t i = 0; i < dim; i++) {
        energy += weights[i] * weights[i];
    }
    return energy / (float)dim;  // Normalized
}


/**
 * @brief Learn from single labeled example
 *
 * WHY: Primary learning interface - supervised learning
 * Updates network weights to match label
 *
 * COMPLEXITY: O(s*n) where s = sparsity, n = active_neurons
 * PERFORMANCE: ~0.1-1ms for small networks, ~10ms for large
 *
 * @param brain Brain handle
 * @param features Input features
 * @param num_features Feature count
 * @param label Target label
 * @param confidence Training weight
 * @return Loss value or -1 on error
 */
// brain_learn_example() - MOVED TO: src/core/brain/learning/nimcp_brain_learning.c

/**
 * @brief Learn from batch of examples
 *
 * WHY: More efficient than individual calls
 * Enables mini-batch gradient descent
 *
 * COMPLEXITY: O(m*s*n) where m = num_examples
 *
 * @param brain Brain handle
 * @param examples Array of examples
 * @param num_examples Example count
 * @return Average loss or -1 on error
 */
// brain_learn_batch() - MOVED TO: src/core/brain/learning/nimcp_brain_learning.c

/**
 * @brief Apply reward-based reinforcement learning
 *
 * WHAT: Apply eligibility-trace-based learning with reward signal
 * WHY:  Enable temporal credit assignment for RL tasks
 * HOW:  Call neural_network_apply_reward_learning() with reward and dopamine
 *
 * BIOLOGY: Three-factor learning rule (Hebbian + Reward + Dopamine)
 * - Eligibility traces mark recently active synapses ("synaptic tags")
 * - Dopamine bursts trigger consolidation ("capture")
 * - Reward signal modulates weight changes (Frey & Morris 1997)
 *
 * COMPLEXITY: O(n × s) where n=neurons, s=synapses_per_neuron
 * USE CASE: Reinforcement learning, temporal credit assignment
 *
 * @param brain Brain handle
 * @param reward Reward signal (0-1 positive, -1-0 negative)
 * @return Number of synapses modified
 */
// brain_apply_reward_learning() - MOVED TO: src/core/brain/learning/nimcp_brain_learning.c

/**
 * @brief Learn by querying an LLM teacher
 *
 * WHY: Enables distillation from larger models
 * Brain learns to mimic LLM decisions efficiently
 *
 * COMPLEXITY: O(s*n) + LLM query time
 * USE CASE: Compress LLM knowledge into fast neural network
 *
 * @param brain Brain handle
 * @param input Input features
 * @param num_features Feature count
 * @param llm_fn Teacher function
 * @param llm_context Context for teacher
 * @return Loss value or -1 on error
 */
// brain_learn_from_llm() - MOVED TO: src/core/brain/learning/nimcp_brain_learning.c

//=============================================================================
// Inference API
//=============================================================================

/**
 * @brief Allocate decision structure
 *
 * COMPLEXITY: O(1)
 *
 * Phase 1.5: Initializes CoW fields - newly allocated decisions own their data
 */
brain_decision_t* allocate_decision(uint32_t output_size)
{
    brain_decision_t* decision = nimcp_calloc(1, sizeof(brain_decision_t));
    if (!decision) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "decision is NULL");

        return NULL;

    }

    decision->output_size = output_size;
    decision->output_vector = nimcp_malloc(output_size * sizeof(float));

    if (!decision->output_vector) {
        nimcp_free(decision);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_decision: decision->output_vector is NULL");
        return NULL;
    }

    // Phase 1.5: Initialize CoW fields - this decision owns its data
    decision->_cow_refcount = NULL;    // NULL means we own the data
    decision->_cow_is_shallow = false; // Not a shallow copy

    return decision;
}


//=============================================================================
// Parallel Forward Pass Infrastructure
//=============================================================================

/**
 * @brief Context for parallel forward pass tasks
 *
 * Each task writes ONLY to its own output slots — no shared writes.
 * Shared inputs (brain, features) are read-only by all tasks.
 */
typedef struct {
    /* Shared inputs (read-only by all tasks) */
    brain_t brain;
    const float* features;
    uint32_t num_features;
    uint32_t output_size;
    bool use_readonly;

    /* Per-network output slots (each written by ONE task only) */
    float* adaptive_output;
    uint32_t adaptive_active;
    float adaptive_confidence;
    bool adaptive_success;

    float* cnn_output;
    float cnn_confidence;
    char cnn_label[64];
    bool cnn_has_label;
    bool cnn_success;

    float* snn_output;
    float snn_confidence;
    bool snn_success;
} forward_task_ctx_t;

/**
 * @brief Adaptive network forward task (thread pool worker)
 */
static void forward_adaptive_task(void* arg)
{
    forward_task_ctx_t* ctx = (forward_task_ctx_t*)arg;

    if (ctx->use_readonly) {
        ctx->adaptive_active = adaptive_network_forward_readonly(
            ctx->brain->network, ctx->features, ctx->num_features,
            ctx->adaptive_output, ctx->output_size, 0);
    } else {
        ctx->adaptive_active = adaptive_network_forward(
            ctx->brain->network, ctx->features, ctx->num_features,
            ctx->adaptive_output, ctx->output_size, 0);
    }

    /* Softmax-max confidence: numerically stable max-normalized softmax */
    float max_val = -1e30f;
    for (uint32_t i = 0; i < ctx->output_size; i++) {
        if (isfinite(ctx->adaptive_output[i]) && ctx->adaptive_output[i] > max_val)
            max_val = ctx->adaptive_output[i];
    }
    float sum_exp = 0.0f;
    for (uint32_t i = 0; i < ctx->output_size; i++) {
        if (isfinite(ctx->adaptive_output[i]))
            sum_exp += expf(ctx->adaptive_output[i] - max_val);
    }
    ctx->adaptive_confidence = (sum_exp > 0.0f) ? (1.0f / sum_exp) : 0.01f;
    if (ctx->adaptive_confidence > 1.0f) ctx->adaptive_confidence = 1.0f;
    if (ctx->adaptive_confidence < 0.01f) ctx->adaptive_confidence = 0.01f;

    ctx->adaptive_success = true;
}

/**
 * @brief CNN forward task (thread pool worker)
 */
static void forward_cnn_task(void* arg)
{
    forward_task_ctx_t* ctx = (forward_task_ctx_t*)arg;

    uint32_t input_dims[2] = {1, ctx->num_features};
    nimcp_tensor_t* cnn_input = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_F32);
    if (!cnn_input) return;

    float* in_data = (float*)nimcp_tensor_data(cnn_input);
    if (in_data) {
        memcpy(in_data, ctx->features, ctx->num_features * sizeof(float));
    }

    cnn_forward_result_t fwd = {0};
    nimcp_error_t rc = cnn_trainer_forward(ctx->brain->cnn_trainer, cnn_input, &fwd);
    if (rc == NIMCP_SUCCESS && fwd.output) {
        const float* cnn_out = (const float*)nimcp_tensor_data(fwd.output);
        if (cnn_out) {
            /* Copy CNN output to ctx slot */
            for (uint32_t i = 0; i < ctx->output_size; i++) {
                ctx->cnn_output[i] = isfinite(cnn_out[i]) ? cnn_out[i] : 0.0f;
            }

            /* Argmax + softmax-max confidence */
            uint32_t cnn_max_idx = 0;
            float max_val = -1e30f;
            for (uint32_t i = 0; i < ctx->output_size; i++) {
                if (ctx->cnn_output[i] > max_val) {
                    max_val = ctx->cnn_output[i];
                    cnn_max_idx = i;
                }
            }
            float sum_exp = 0.0f;
            for (uint32_t i = 0; i < ctx->output_size; i++) {
                sum_exp += expf(ctx->cnn_output[i] - max_val);
            }
            ctx->cnn_confidence = (sum_exp > 0.0f) ? (1.0f / sum_exp) : 0.01f;
            if (ctx->cnn_confidence > 1.0f) ctx->cnn_confidence = 1.0f;
            if (ctx->cnn_confidence < 0.01f) ctx->cnn_confidence = 0.01f;

            /* Extract label from CNN argmax */
            if (ctx->brain->output_labels && cnn_max_idx < ctx->brain->num_output_labels
                && ctx->brain->output_labels[cnn_max_idx]) {
                strncpy(ctx->cnn_label, ctx->brain->output_labels[cnn_max_idx],
                        sizeof(ctx->cnn_label) - 1);
                ctx->cnn_label[sizeof(ctx->cnn_label) - 1] = '\0';
                ctx->cnn_has_label = true;
            }

            ctx->cnn_success = true;
        }
    }
    nimcp_tensor_destroy(cnn_input);
}

/**
 * @brief SNN forward task (thread pool worker)
 */
static void forward_snn_task(void* arg)
{
    forward_task_ctx_t* ctx = (forward_task_ctx_t*)arg;

    /* Average-pool features to match SNN input dimension (capped at 256) */
    uint32_t snn_in = ctx->brain->snn_network->config.n_inputs;
    uint32_t snn_out = ctx->brain->snn_network->config.n_outputs;
    float* snn_input = NULL;
    const float* input_ptr = ctx->features;
    uint32_t input_dim = ctx->num_features;

    if (ctx->num_features > snn_in) {
        snn_input = nimcp_calloc(snn_in, sizeof(float));
        if (!snn_input) return;
        uint32_t stride = ctx->num_features / snn_in;
        /* Average-pool to SNN input dimension */
        float pool_min = 1e30f, pool_max = -1e30f;
        for (uint32_t i = 0; i < snn_in; i++) {
            float sum = 0.0f;
            uint32_t start = i * stride;
            uint32_t end = (i + 1 < snn_in) ? (i + 1) * stride : ctx->num_features;
            if (end > ctx->num_features) end = ctx->num_features;
            for (uint32_t j = start; j < end; j++) sum += ctx->features[j];
            snn_input[i] = (end > start) ? sum / (float)(end - start) : 0.0f;
            if (snn_input[i] < pool_min) pool_min = snn_input[i];
            if (snn_input[i] > pool_max) pool_max = snn_input[i];
        }
        /* Normalize pooled features to [0, 1] so SNN input_current_scale
         * maps directly to membrane current in mV. BERT embeddings after
         * avg pooling are in ~[-0.05, 0.05] — without normalization the
         * currents are < 1mV and neurons never spike. */
        float pool_range = pool_max - pool_min;
        if (pool_range > 1e-8f) {
            for (uint32_t i = 0; i < snn_in; i++) {
                snn_input[i] = (snn_input[i] - pool_min) / pool_range;
            }
        }
        /* Blend somatosensory data into SNN input — mechanoreceptors
         * produce spike trains proportional to stimulus intensity.
         * Inject somatosensory features as additional current drive. */
        if (ctx->brain->staged_sensory.somato_data &&
            ctx->brain->staged_sensory.somato_segments > 0) {
            uint32_t somato_n = ctx->brain->staged_sensory.somato_segments;
            uint32_t inject_start = (snn_in > somato_n) ? snn_in - somato_n : 0;
            uint32_t inject_n = (somato_n < snn_in - inject_start)
                              ? somato_n : snn_in - inject_start;
            for (uint32_t i = 0; i < inject_n; i++) {
                float somato_val = ctx->brain->staged_sensory.somato_data[i];
                /* Somatosensory is already [0,1] from encode_sensory.
                 * Blend with normalized pooled features. */
                snn_input[inject_start + i] = 0.5f * snn_input[inject_start + i]
                                            + 0.5f * somato_val;
            }
        }

        input_ptr = snn_input;
        input_dim = snn_in;
    }

    uint32_t out_dim = (ctx->output_size < snn_out) ? ctx->output_size : snn_out;
    /* Simulate for 100ms — LIF neurons with tau_mem=20ms need ~61ms to spike
     * from v_rest=-70mV to v_thresh=-50mV with input I=21mV (feature 0.3 * scale 70).
     * At dt=0.1ms this is 1000 steps. Stronger inputs spike faster. */
    int rc = snn_network_forward(ctx->brain->snn_network, input_ptr, input_dim,
                                  ctx->snn_output, out_dim, 100.0f);
    nimcp_free(snn_input);
    if (rc != 0) return;

    /* Spike coherence confidence: 1 - entropy/max_entropy of softmax distribution.
     * Only read out_dim elements (what snn_network_forward actually wrote). */
    float max_val = -1e30f;
    for (uint32_t i = 0; i < out_dim; i++) {
        if (isfinite(ctx->snn_output[i]) && ctx->snn_output[i] > max_val)
            max_val = ctx->snn_output[i];
    }
    float sum_exp = 0.0f;
    float entropy = 0.0f;
    for (uint32_t i = 0; i < out_dim; i++) {
        float p = isfinite(ctx->snn_output[i]) ? expf(ctx->snn_output[i] - max_val) : 0.0f;
        sum_exp += p;
    }
    if (sum_exp > 0.0f) {
        for (uint32_t i = 0; i < out_dim; i++) {
            float p = isfinite(ctx->snn_output[i]) ? expf(ctx->snn_output[i] - max_val) / sum_exp : 0.0f;
            if (p > 1e-8f) {
                entropy -= p * logf(p);
            }
        }
    }
    float max_entropy = (out_dim > 1) ? logf((float)out_dim) : 1.0f;
    ctx->snn_confidence = (max_entropy > 0.0f) ? (1.0f - entropy / max_entropy) : 0.01f;
    if (ctx->snn_confidence > 1.0f) ctx->snn_confidence = 1.0f;
    if (ctx->snn_confidence < 0.01f) ctx->snn_confidence = 0.01f;

    ctx->snn_success = true;
}

/**
 * @brief Confidence-weighted fusion of parallel network outputs
 *
 * Replaces hardcoded 0.7/0.3 blend with dynamic confidence weighting.
 * Each successful network contributes confidence * output[i], normalized
 * by total confidence.
 */
static void fuse_forward_outputs(const forward_task_ctx_t* ctx,
                                  float* fused_output, uint32_t output_size,
                                  uint32_t* active_neurons_out,
                                  brain_decision_t* decision)
{
    /* Zero fused output */
    memset(fused_output, 0, output_size * sizeof(float));

    /* Phase 6: Use explicit fusion weights when enabled */
    if (ctx->brain && ctx->brain->enable_fusion) {
        /* Softmax normalization of fusion weights for active networks */
        float weights[3] = {0};  /* adaptive, CNN, SNN */
        float wsum = 0.0f;
        if (ctx->adaptive_success) { weights[0] = ctx->brain->fusion_weights[0]; wsum += weights[0]; }
        if (ctx->cnn_success)      { weights[1] = ctx->brain->fusion_weights[1]; wsum += weights[1]; }
        if (ctx->snn_success)      { weights[2] = ctx->brain->fusion_weights[2]; wsum += weights[2]; }
        if (wsum > 0.0f) {
            float inv = 1.0f / wsum;
            for (int k = 0; k < 3; k++) weights[k] *= inv;
        } else {
            weights[0] = 1.0f;  /* Fallback to adaptive only */
        }

        /* Compute per-network L2 norms for scale normalization.
         * Without this, the network with largest activation magnitudes
         * dominates regardless of fusion weights. */
        float norm_adapt = 0.0f, norm_cnn = 0.0f, norm_snn = 0.0f;
        if (ctx->adaptive_success) {
            for (uint32_t i = 0; i < output_size; i++)
                norm_adapt += ctx->adaptive_output[i] * ctx->adaptive_output[i];
            norm_adapt = sqrtf(norm_adapt + 1e-8f);
        }
        if (ctx->cnn_success) {
            for (uint32_t i = 0; i < output_size; i++)
                norm_cnn += ctx->cnn_output[i] * ctx->cnn_output[i];
            norm_cnn = sqrtf(norm_cnn + 1e-8f);
        }
        if (ctx->snn_success) {
            for (uint32_t i = 0; i < output_size; i++) {
                if (isfinite(ctx->snn_output[i]))
                    norm_snn += ctx->snn_output[i] * ctx->snn_output[i];
            }
            norm_snn = sqrtf(norm_snn + 1e-8f);
        }

        if (ctx->adaptive_success) {
            float scale = weights[0] / norm_adapt;
            for (uint32_t i = 0; i < output_size; i++)
                fused_output[i] += scale * ctx->adaptive_output[i];
            *active_neurons_out = ctx->adaptive_active;
        }
        if (ctx->cnn_success) {
            float scale = weights[1] / norm_cnn;
            for (uint32_t i = 0; i < output_size; i++)
                fused_output[i] += scale * ctx->cnn_output[i];
            if (ctx->cnn_has_label) {
                strncpy(decision->label, ctx->cnn_label, sizeof(decision->label) - 1);
                decision->label[sizeof(decision->label) - 1] = '\0';
            }
        }
        if (ctx->snn_success) {
            float scale = weights[2] / norm_snn;
            for (uint32_t i = 0; i < output_size; i++) {
                if (isfinite(ctx->snn_output[i]))
                    fused_output[i] += scale * ctx->snn_output[i];
            }
        }
        return;
    }

    /* Legacy confidence-weighted fusion with L2 normalization.
     * L2-normalize each network's output before weighting to prevent
     * magnitude-dominant networks from overwhelming the ensemble. */
    float total_confidence = 0.0f;

    /* Compute per-network L2 norms */
    float norm_a = 0.0f, norm_c = 0.0f, norm_s = 0.0f;
    if (ctx->adaptive_success) {
        for (uint32_t i = 0; i < output_size; i++)
            norm_a += ctx->adaptive_output[i] * ctx->adaptive_output[i];
        norm_a = sqrtf(norm_a + 1e-8f);
    }
    if (ctx->cnn_success) {
        for (uint32_t i = 0; i < output_size; i++)
            norm_c += ctx->cnn_output[i] * ctx->cnn_output[i];
        norm_c = sqrtf(norm_c + 1e-8f);
    }
    if (ctx->snn_success) {
        for (uint32_t i = 0; i < output_size; i++) {
            if (isfinite(ctx->snn_output[i]))
                norm_s += ctx->snn_output[i] * ctx->snn_output[i];
        }
        norm_s = sqrtf(norm_s + 1e-8f);
    }

    /* Adaptive contribution (always expected to succeed) */
    if (ctx->adaptive_success) {
        float w = ctx->adaptive_confidence;
        float scale = w / norm_a;
        for (uint32_t i = 0; i < output_size; i++) {
            fused_output[i] += scale * ctx->adaptive_output[i];
        }
        total_confidence += w;
        *active_neurons_out = ctx->adaptive_active;
    }

    /* CNN contribution */
    if (ctx->cnn_success) {
        float w = ctx->cnn_confidence;
        float scale = w / norm_c;
        for (uint32_t i = 0; i < output_size; i++) {
            fused_output[i] += scale * ctx->cnn_output[i];
        }
        total_confidence += w;

        /* CNN label override (if available) */
        if (ctx->cnn_has_label) {
            strncpy(decision->label, ctx->cnn_label, sizeof(decision->label) - 1);
            decision->label[sizeof(decision->label) - 1] = '\0';
        }
    }

    /* SNN contribution */
    if (ctx->snn_success) {
        float w = ctx->snn_confidence;
        float scale = w / norm_s;
        for (uint32_t i = 0; i < output_size; i++) {
            if (isfinite(ctx->snn_output[i])) {
                fused_output[i] += scale * ctx->snn_output[i];
            }
        }
        total_confidence += w;
    }

    /* Normalize by total confidence */
    if (total_confidence > 0.0f) {
        float inv_conf = 1.0f / total_confidence;
        for (uint32_t i = 0; i < output_size; i++) {
            fused_output[i] *= inv_conf;
        }
    }
}

/**
 * @brief Perform forward pass through network
 *
 * COMPLEXITY: O(s*n) where s = sparsity
 *
 * When inference_pool is available and secondary networks (CNN/SNN) exist,
 * runs adaptive + CNN + SNN in parallel, then fuses outputs with confidence
 * weighting. LNN gating always runs sequentially post-fusion.
 *
 * @param brain Brain handle
 * @param features Input features
 * @param num_features Feature count
 * @param decision Decision to populate
 * @return Number of active neurons
 */
uint32_t perform_forward_pass(brain_t brain, const float* features, uint32_t num_features,
                              brain_decision_t* decision)
{
    uint64_t start_time = nimcp_time_monotonic_us();
    uint32_t active_neurons = 0;
    bool has_cnn = (brain->cnn_trainer != NULL);
    bool has_snn = (brain->snn_network != NULL);

    /* Declare before parallel/sequential branch so lnn_gating goto can use them */
    const float* fwd_features = features;
    uint32_t fwd_num = num_features;

    /* === PER-SUBSYSTEM WALL-CLOCK TIMING (NIMCP_DEBUG_TIMING=1) ===
     * The brain_decide caller already times "forward" as a single phase.
     * These markers split that phase into cortex-CNN / adaptive / CNN /
     * SNN / LNN buckets so we can identify which subsystem dominates as
     * brain state grows (e.g., cortex CNN inference on CPU). */
    static int _pf_dbg_checked = 0, _pf_dbg = 0;
    if (!_pf_dbg_checked) {
        const char* env = getenv("NIMCP_DEBUG_TIMING");
        _pf_dbg = (env && env[0] == '1');
        _pf_dbg_checked = 1;
    }
    uint64_t _pf_t0 = _pf_dbg ? nimcp_time_get_us() : 0;
    uint64_t _pf_t_cortex = 0, _pf_t_adaptive = 0, _pf_t_cnn = 0;
    uint64_t _pf_t_snn = 0, _pf_t_lnn = 0, _pf_t_end = 0;
    #define PF_MARK(var) do { if (_pf_dbg) (var) = nimcp_time_get_us(); } while (0)
    #define PF_MS(a, b) (((b) - (a)) / 1000.0)

    /* === Parallel path: submit adaptive + CNN + SNN to thread pool === */
    /* DISABLED: GPU forward from worker threads fails silently (CUDA context
     * not propagated). Sequential path calls GPU forward from main thread. */
    if (0 && brain->inference_pool && (has_cnn || has_snn)) {
        forward_task_ctx_t* ctx = nimcp_calloc(1, sizeof(forward_task_ctx_t));
        if (!ctx) goto sequential_fallback;

        ctx->brain = brain;
        ctx->features = features;
        ctx->num_features = num_features;
        ctx->output_size = decision->output_size;
        ctx->use_readonly = brain->can_use_readonly;

        /* OPTIMIZATION: Reuse pre-allocated inference buffers when possible.
         * Eliminates 3 calloc+free round-trips per forward pass. Buffers are
         * lazily allocated on first use and grow if output_size changes. */
        if (brain->inference_buf_size < decision->output_size || !brain->inference_buf_adaptive) {
            /* (Re-)allocate pre-allocated buffers to match current output size */
            nimcp_free(brain->inference_buf_adaptive);
            nimcp_free(brain->inference_buf_cnn);
            nimcp_free(brain->inference_buf_snn);
            brain->inference_buf_adaptive = nimcp_calloc(decision->output_size, sizeof(float));
            brain->inference_buf_cnn = nimcp_calloc(decision->output_size, sizeof(float));
            brain->inference_buf_snn = nimcp_calloc(decision->output_size, sizeof(float));
            brain->inference_buf_size = decision->output_size;
            if (!brain->inference_buf_adaptive) {
                nimcp_free(ctx);
                goto sequential_fallback;
            }
        } else {
            /* Zero pre-allocated buffers for clean reuse */
            memset(brain->inference_buf_adaptive, 0, decision->output_size * sizeof(float));
            if (brain->inference_buf_cnn)
                memset(brain->inference_buf_cnn, 0, decision->output_size * sizeof(float));
            if (brain->inference_buf_snn)
                memset(brain->inference_buf_snn, 0, decision->output_size * sizeof(float));
        }

        ctx->adaptive_output = brain->inference_buf_adaptive;
        if (has_cnn) {
            ctx->cnn_output = brain->inference_buf_cnn;
            if (!ctx->cnn_output) {
                nimcp_free(ctx);
                goto sequential_fallback;
            }
        }
        if (has_snn) {
            ctx->snn_output = brain->inference_buf_snn;
            if (!ctx->snn_output) {
                nimcp_free(ctx);
                goto sequential_fallback;
            }
        }

        /* Submit tasks to pool */
        nimcp_pool_submit(brain->inference_pool, forward_adaptive_task, ctx);
        if (has_cnn) {
            nimcp_pool_submit(brain->inference_pool, forward_cnn_task, ctx);
        }
        if (has_snn) {
            nimcp_pool_submit(brain->inference_pool, forward_snn_task, ctx);
        }

        /* Wait for all tasks to complete */
        nimcp_pool_wait(brain->inference_pool);

        /* Fuse outputs with confidence weighting */
        fuse_forward_outputs(ctx, decision->output_vector, decision->output_size,
                             &active_neurons, decision);

        /* Cleanup — only free the context struct, not the output buffers
         * (they are pre-allocated in brain->inference_buf_* and reused). */
        nimcp_free(ctx);

        goto lnn_gating;
    }

sequential_fallback:

    /* === Sequential fallback (original code path) === */

    /* 0.5. Per-cortex CNN feature extraction — run before adaptive to augment features.
     * Each cortex CNN processes its modality's staged sensory data independently,
     * then embeddings are fused via cross-modal attention and prepended to features. */
    {
        extern const float* cortex_cnn_forward_visual(struct cortex_cnn_processor* proc,
            const uint8_t* pixels, uint32_t w, uint32_t h, uint32_t ch);
        extern const float* cortex_cnn_forward_audio(struct cortex_cnn_processor* proc,
            const float* mel, uint32_t mel_size);
        extern const float* cortex_cnn_forward_speech(struct cortex_cnn_processor* proc,
            const float* phonemes, uint32_t size);
        extern const float* cortex_cnn_forward_somato(struct cortex_cnn_processor* proc,
            const float* segments, uint32_t n_segments);
        extern uint32_t cortex_cnn_fuse(struct cortex_cnn_processor* procs[], uint32_t count,
            float* fused_out, uint32_t max_dim);

        bool has_cortex = false;
        for (int ci = 0; ci < 4; ci++) {
            if (brain->cortex_cnns[ci]) { has_cortex = true; break; }
        }

        if (has_cortex) {
            /* Run forward on each cortex with available sensory data */
            if (brain->cortex_cnns[0] && brain->staged_sensory.visual_frame) {
                cortex_cnn_forward_visual(brain->cortex_cnns[0],
                    brain->staged_sensory.visual_frame,
                    brain->staged_sensory.visual_width,
                    brain->staged_sensory.visual_height,
                    brain->staged_sensory.visual_channels);
            }
            if (brain->cortex_cnns[1] && brain->staged_sensory.audio_data) {
                cortex_cnn_forward_audio(brain->cortex_cnns[1],
                    brain->staged_sensory.audio_data,
                    brain->staged_sensory.audio_size);
            }
            if (brain->cortex_cnns[2] && brain->staged_sensory.speech_data) {
                cortex_cnn_forward_speech(brain->cortex_cnns[2],
                    brain->staged_sensory.speech_data,
                    brain->staged_sensory.speech_size);
            }
            if (brain->cortex_cnns[3] && brain->staged_sensory.somato_data) {
                cortex_cnn_forward_somato(brain->cortex_cnns[3],
                    brain->staged_sensory.somato_data,
                    brain->staged_sensory.somato_segments);
            }

            /* Fuse embeddings via cross-modal attention.
             * Sum actual embedding dims from active cortex CNNs.
             * SNN-primary: visual/audio=256, speech/somato=128 → total 768. */
            uint32_t max_fused = 0;
            for (int ci = 0; ci < 4; ci++) {
                if (brain->cortex_cnns[ci]) {
                    cortex_cnn_metrics_t m;
                    if (cortex_cnn_get_metrics(
                            (cortex_cnn_processor_t*)brain->cortex_cnns[ci], &m) == 0) {
                        max_fused += m.embedding_dim;
                    }
                }
            }
            #define CORTEX_CNN_MAX_FUSED_EMBED_DIM  768  /* 256+256+128+128 */
            if (max_fused == 0) max_fused = CORTEX_CNN_MAX_FUSED_EMBED_DIM;
            if (!brain->cortex_cnn_fused_embedding ||
                brain->cortex_cnn_fused_dim < max_fused) {
                nimcp_free(brain->cortex_cnn_fused_embedding);
                brain->cortex_cnn_fused_embedding = (float*)nimcp_calloc(max_fused, sizeof(float));
            }
            if (brain->cortex_cnn_fused_embedding) {
                brain->cortex_cnn_fused_dim = cortex_cnn_fuse(
                    (struct cortex_cnn_processor**)brain->cortex_cnns, 4,
                    brain->cortex_cnn_fused_embedding, max_fused);
            }
        }
    }

    /* Blend cortex CNN fused embedding into features for perceptual grounding.
     * Uses thread-local static buffer to avoid malloc (which caused SIGSEGV
     * in Release builds due to optimizer register spilling). Max 4096 features. */
    /* fwd_features/fwd_num already declared at function scope */
    static __thread float s_blended[4096];
    if (brain->cortex_cnn_fused_embedding && brain->cortex_cnn_fused_dim > 0 &&
        num_features <= 4096) {
        memcpy(s_blended, features, num_features * sizeof(float));
        uint32_t inject_n = (brain->cortex_cnn_fused_dim < num_features)
                          ? brain->cortex_cnn_fused_dim : num_features;
        for (uint32_t i = 0; i < inject_n; i++) {
            s_blended[i] += 0.3f * brain->cortex_cnn_fused_embedding[i];
        }
        fwd_features = s_blended;
    }

    PF_MARK(_pf_t_cortex);  /* end cortex CNN forward + fusion */

    /* 1. Adaptive network — primary forward pass (always runs) */
    if (brain->can_use_readonly) {
        active_neurons = adaptive_network_forward_readonly(
            brain->network, fwd_features, fwd_num, decision->output_vector, decision->output_size, 0);
    } else {
        active_neurons = adaptive_network_forward(
            brain->network, fwd_features, fwd_num, decision->output_vector, decision->output_size, 0);
    }
    PF_MARK(_pf_t_adaptive);  /* end adaptive (ANN main net) forward */

    PROBE_STAGE(brain, PROBE_INF_ADAPTIVE_FWD, {
        PROBE_SET_INT(&_ctx, "active_neurons", (int64_t)active_neurons);
        float _out_norm = 0.0f;
        for (uint32_t _i = 0; _i < decision->output_size; _i++)
            _out_norm += decision->output_vector[_i] * decision->output_vector[_i];
        PROBE_SET_FLOAT(&_ctx, "output_norm", sqrtf(_out_norm));
        PROBE_SET_INT(&_ctx, "output_size", (int64_t)decision->output_size);
    });

    /* 2. CNN forward — classification overlay (label from CNN, embedding from adaptive) */
    if (brain->cnn_trainer) {
        uint32_t input_dims[2] = {1, num_features};
        nimcp_tensor_t* cnn_input = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_F32);
        if (cnn_input) {
            float* in_data = (float*)nimcp_tensor_data(cnn_input);
            if (in_data) {
                memcpy(in_data, features, num_features * sizeof(float));
            }

            cnn_forward_result_t fwd = {0};
            nimcp_error_t rc = cnn_trainer_forward(brain->cnn_trainer, cnn_input, &fwd);
            if (rc == NIMCP_SUCCESS && fwd.output) {
                const float* cnn_out = (const float*)nimcp_tensor_data(fwd.output);
                if (cnn_out) {
                    /* Find CNN argmax — use as classification label */
                    uint32_t cnn_max_idx = 0;
                    float cnn_max_val = -1e30f;
                    for (uint32_t i = 0; i < decision->output_size; i++) {
                        if (isfinite(cnn_out[i]) && cnn_out[i] > cnn_max_val) {
                            cnn_max_val = cnn_out[i];
                            cnn_max_idx = i;
                        }
                    }
                    /* Override label with CNN's classification if labels exist */
                    if (brain->output_labels && cnn_max_idx < brain->num_output_labels
                        && brain->output_labels[cnn_max_idx]) {
                        strncpy(decision->label, brain->output_labels[cnn_max_idx],
                                sizeof(decision->label) - 1);
                        decision->label[sizeof(decision->label) - 1] = '\0';
                    }
                }
            }
            nimcp_tensor_destroy(cnn_input);
        }
    }
    PF_MARK(_pf_t_cnn);  /* end CNN trainer forward */

    /* 3. SNN forward — spike-based blending */
    if (brain->snn_network) {
        uint32_t snn_in = brain->snn_network->config.n_inputs;
        uint32_t snn_out = brain->snn_network->config.n_outputs;
        float* snn_output = nimcp_calloc(decision->output_size, sizeof(float));
        float* snn_input = NULL;
        const float* snn_input_ptr = fwd_features;
        uint32_t snn_input_dim = fwd_num;

        /* Match features to SNN input dimension: pool down or zero-pad up */
        if (num_features > snn_in) {
            /* Average-pool features down, then normalize to [0,1] */
            snn_input = nimcp_calloc(snn_in, sizeof(float));
            if (snn_input) {
                uint32_t stride = num_features / snn_in;
                float pool_min = 1e30f, pool_max = -1e30f;
                for (uint32_t i = 0; i < snn_in; i++) {
                    float sum = 0.0f;
                    uint32_t start = i * stride;
                    uint32_t end = (i + 1 < snn_in) ? (i + 1) * stride : num_features;
                    for (uint32_t j = start; j < end; j++) sum += features[j];
                    snn_input[i] = sum / (float)(end - start);
                    if (snn_input[i] < pool_min) pool_min = snn_input[i];
                    if (snn_input[i] > pool_max) pool_max = snn_input[i];
                }
                float pool_range = pool_max - pool_min;
                if (pool_range > 1e-8f) {
                    for (uint32_t i = 0; i < snn_in; i++) {
                        snn_input[i] = (snn_input[i] - pool_min) / pool_range;
                    }
                }
                snn_input_ptr = snn_input;
                snn_input_dim = snn_in;
            }
        } else if (num_features < snn_in) {
            /* Zero-pad up to SNN input dimension */
            snn_input = nimcp_calloc(snn_in, sizeof(float));
            if (snn_input) {
                memcpy(snn_input, fwd_features, num_features * sizeof(float));
                snn_input_ptr = snn_input;
                snn_input_dim = snn_in;
            }
        }

        uint32_t snn_out_dim = (decision->output_size < snn_out) ? decision->output_size : snn_out;
        if (snn_output) {
            int rc = snn_network_forward(brain->snn_network, snn_input_ptr, snn_input_dim,
                                         snn_output, snn_out_dim, 100.0f);
            if (rc == 0) {
                /* Blend adaptive + SNN outputs */
                float w_adapt = 0.7f, w_snn = 0.3f;
                if (brain->enable_fusion) {
                    float ws = brain->fusion_weights[0] + brain->fusion_weights[2];
                    if (ws > 0.0f) {
                        w_adapt = brain->fusion_weights[0] / ws;
                        w_snn = brain->fusion_weights[2] / ws;
                    }
                }
                for (uint32_t i = 0; i < snn_out_dim; i++) {
                    if (isfinite(snn_output[i])) {
                        decision->output_vector[i] = w_adapt * decision->output_vector[i]
                                                   + w_snn * snn_output[i];
                    }
                }
            }
            nimcp_free(snn_output);
        }
        nimcp_free(snn_input);

        PROBE_STAGE(brain, PROBE_INF_SNN_FWD, {
            float _snn_norm = 0.0f;
            for (uint32_t _i = 0; _i < snn_out_dim; _i++)
                _snn_norm += decision->output_vector[_i] * decision->output_vector[_i];
            PROBE_SET_FLOAT(&_ctx, "snn_output_norm", sqrtf(_snn_norm));
            PROBE_SET_INT(&_ctx, "snn_in", (int64_t)snn_in);
            PROBE_SET_INT(&_ctx, "snn_out", (int64_t)snn_out);
        });
    }

    /* 3.5. SNN routing bridge — update cross-region spike routing */
    if (brain->snn_routing_bridge) {
        snn_routing_bridge_update(brain->snn_routing_bridge, 1.0f);
    }
    PF_MARK(_pf_t_snn);  /* end SNN forward + routing bridge */

lnn_gating:

    /* 4. LNN forward — temporal context modulation (multiplicative sigmoid gating) */
    /* Always sequential: LNN modulates the fused result */
    if (brain->lnn_network) {
        uint32_t lnn_out_size = brain->lnn_network->n_outputs;
        uint32_t lnn_in_size = brain->lnn_network->n_inputs;
        uint32_t in_dims[1] = {lnn_in_size};
        uint32_t out_dims[1] = {lnn_out_size};

        nimcp_tensor_t* lnn_input = nimcp_tensor_create(in_dims, 1, NIMCP_DTYPE_F32);
        nimcp_tensor_t* lnn_output = nimcp_tensor_create(out_dims, 1, NIMCP_DTYPE_F32);

        if (lnn_input && lnn_output) {
            float* li_data = (float*)nimcp_tensor_data(lnn_input);
            if (li_data) {
                /* Average-pool features (with cortex blend) into LNN input size */
                if (fwd_num > lnn_in_size) {
                    uint32_t stride = fwd_num / lnn_in_size;
                    for (uint32_t i = 0; i < lnn_in_size; i++) {
                        float sum = 0.0f;
                        uint32_t start = i * stride;
                        uint32_t end = (i + 1 < lnn_in_size) ? (i + 1) * stride : fwd_num;
                        if (end > fwd_num) end = fwd_num;
                        for (uint32_t j = start; j < end; j++) {
                            sum += fwd_features[j];
                        }
                        li_data[i] = (end > start) ? sum / (float)(end - start) : 0.0f;
                    }
                } else {
                    uint32_t copy_n = (fwd_num < lnn_in_size) ? fwd_num : lnn_in_size;
                    memcpy(li_data, fwd_features, copy_n * sizeof(float));
                    if (copy_n < lnn_in_size)
                        memset(li_data + copy_n, 0, (lnn_in_size - copy_n) * sizeof(float));
                }

                /* Blend somatosensory data into LNN input — touch sensations
                 * have continuous temporal dynamics (pressure building,
                 * temperature changing, texture being explored). The LNN's
                 * ODE solver naturally captures these dynamics.
                 * Inject into last N channels of LNN input. */
                if (brain->staged_sensory.somato_data &&
                    brain->staged_sensory.somato_segments > 0) {
                    uint32_t somato_n = brain->staged_sensory.somato_segments;
                    /* Map somatosensory into the last portion of LNN input.
                     * If LNN has 128 inputs and somato has 45 dims, fill
                     * inputs [83..127] with somatosensory data. This gives
                     * the LNN temporal processor direct access to touch. */
                    uint32_t inject_start = (lnn_in_size > somato_n)
                                         ? lnn_in_size - somato_n : 0;
                    uint32_t inject_n = (somato_n < lnn_in_size - inject_start)
                                      ? somato_n : lnn_in_size - inject_start;
                    for (uint32_t i = 0; i < inject_n; i++) {
                        /* Additive blend: existing feature + somatosensory */
                        float somato_val = brain->staged_sensory.somato_data[i];
                        li_data[inject_start + i] = 0.5f * li_data[inject_start + i]
                                                  + 0.5f * somato_val;
                    }
                }
            }

            /* HNN probe: if any layer is configured for Hamiltonian dynamics
             * AND has its H-network + momentum tensor allocated, this forward
             * call will route through lnn_layer_forward_hamiltonian. We can
             * only count "configured to take HNN path" here — the in-loop
             * fallback (e.g. p==NULL) is invisible from this site. The
             * fallback counter is incremented opportunistically when use_hamiltonian
             * is set but H_net is NULL (layer was meant to be HNN but isn't). */
            if (brain->lnn_network && brain->lnn_network->n_layers > 0 &&
                brain->lnn_network->layers && brain->lnn_network->layers[0]) {
                lnn_layer_t* l0 = brain->lnn_network->layers[0];
                if (l0->use_hamiltonian && l0->H_net) {
                    brain->module_activity.hnn_forward_invocations++;
                } else if (l0->use_hamiltonian && !l0->H_net) {
                    brain->module_activity.hnn_fallback_invocations++;
                }
            }
            int rc = lnn_forward_step(brain->lnn_network, lnn_input, lnn_output, 0.01f);
            if (rc == 0) {
                const float* lo_data = (const float*)nimcp_tensor_data(lnn_output);
                if (lo_data) {
                    /* LNN gating on first lnn_out_size outputs */
                    uint32_t gate_size = (lnn_out_size < decision->output_size)
                                       ? lnn_out_size : decision->output_size;
                    if (brain->enable_fusion && brain->fusion_weights[3] > 0.0f) {
                        /* Fusion mode: additive blending with LNN weight */
                        float w_lnn = brain->fusion_weights[3];
                        for (uint32_t i = 0; i < gate_size; i++) {
                            if (isfinite(lo_data[i])) {
                                decision->output_vector[i] = (1.0f - w_lnn) * decision->output_vector[i]
                                                           + w_lnn * lo_data[i];
                            }
                        }
                    } else {
                        /* Legacy: multiplicative sigmoid gating */
                        for (uint32_t i = 0; i < gate_size; i++) {
                            if (isfinite(lo_data[i])) {
                                float gate = 1.0f / (1.0f + expf(-lo_data[i]));
                                decision->output_vector[i] *= gate;
                            }
                        }
                    }
                }
            }
        }
        if (lnn_input) nimcp_tensor_destroy(lnn_input);
        if (lnn_output) nimcp_tensor_destroy(lnn_output);

        PROBE_STAGE(brain, PROBE_INF_LNN_GATE, {
            PROBE_SET_INT(&_ctx, "lnn_in_size", (int64_t)lnn_in_size);
            PROBE_SET_INT(&_ctx, "lnn_out_size", (int64_t)lnn_out_size);
        });
    }
    PF_MARK(_pf_t_lnn);  /* end LNN gating */

    decision->inference_time_us = nimcp_time_elapsed_us(start_time);

    /* === EMIT PER-SUBSYSTEM TIMING SUMMARY (NIMCP_DEBUG_TIMING=1) === */
    if (_pf_dbg) {
        PF_MARK(_pf_t_end);
        double total_ms = PF_MS(_pf_t0, _pf_t_end);
        if (total_ms > 500.0) {  /* only log slow forward passes (cycle is 8.5s) */
            NIMCP_LOGGING_INFO(
                "perform_forward_pass (ms): cortex=%.0f adaptive=%.0f cnn=%.0f "
                "snn=%.0f lnn=%.0f TOTAL=%.0f",
                PF_MS(_pf_t0,         _pf_t_cortex),
                PF_MS(_pf_t_cortex,   _pf_t_adaptive),
                PF_MS(_pf_t_adaptive, _pf_t_cnn),
                PF_MS(_pf_t_cnn,      _pf_t_snn),
                PF_MS(_pf_t_snn,      _pf_t_lnn),
                total_ms);
        }
    }
    #undef PF_MARK
    #undef PF_MS

    return active_neurons;
}


/**
 * @brief Find maximum output and determine label
 *
 * COMPLEXITY: O(n) where n = num_outputs
 */
static void determine_output_label(brain_t brain, brain_decision_t* decision)
{
    // P2-50 FIX: Guard against output_size==0 to prevent OOB access on output_vector[0]
    if (decision->output_size == 0) return;

    // W7-1 FIX (C-INF-1): NaN-safe argmax. If output contains NaN values, standard
    // comparison (NaN > max_value) is always false, silently picking index 0.
    // Use isfinite() to skip NaN/Inf values and detect all-NaN output.
    // Previously initialized max_value = output_vector[0] which propagated NaN.
    uint32_t max_idx = 0;
    float max_value = -FLT_MAX;
    bool has_valid = false;

    for (uint32_t i = 0; i < decision->output_size; i++) {
        if (isfinite(decision->output_vector[i]) && decision->output_vector[i] > max_value) {
            max_value = decision->output_vector[i];
            max_idx = i;
            has_valid = true;
        }
    }

    // If all outputs are NaN/Inf, set label to UNKNOWN with zero confidence
    if (!has_valid) {
        strncpy(decision->label, "UNKNOWN", sizeof(decision->label) - 1);
        decision->label[sizeof(decision->label) - 1] = '\0';
        decision->confidence = 0.0f;
        return;
    }

    // W7-2 FIX (C-INF-2): 3-way NULL guard before accessing output_labels.
    // Check: (1) output_labels array is non-NULL, (2) max_idx is within bounds,
    // AND (3) the specific entry is non-NULL. Previous code only checked bounds.
    if (brain->output_labels && max_idx < brain->num_output_labels
        && brain->output_labels[max_idx]) {
        strncpy(decision->label, brain->output_labels[max_idx], sizeof(decision->label) - 1);
    } else {
        decision->label[0] = '\0';
    }

    // W7-3 FIX (C-INF-3): Use ratio normalization (max/sum_abs) matching predict_fast,
    // replacing the fixed-divisor formula (max/10.0) that gave inconsistent
    // confidence values compared to predict_fast for the same inputs.
    // Also clamp to [0, 1] to prevent negative confidence from negative max_value.
    float sum_abs = 0.0f;
    for (uint32_t i = 0; i < decision->output_size; i++) {
        if (isfinite(decision->output_vector[i]))
            sum_abs += fabsf(decision->output_vector[i]);
    }
    float raw_conf = (sum_abs > 0.0f) ? (max_value / sum_abs) : 0.0f;
    decision->confidence = fmaxf(0.0f, fminf(raw_conf, 1.0f));
}


/**
 * @brief Populate interpretability information
 *
 * COMPLEXITY: O(n)
 */
static void populate_interpretability(brain_t brain, const float* features, uint32_t num_features,
                                      uint32_t active_neurons, brain_decision_t* decision)
{
    decision->num_active_neurons = active_neurons;
    decision->sparsity = adaptive_network_get_sparsity(brain->network);

    if (brain->config.enable_explanations) {
        adaptive_network_explain(brain->network, features, num_features, decision->explanation,
                                 sizeof(decision->explanation));
    }

    // W7-9: Guard against nimcp_malloc(0) when active_neurons==0.
    // malloc(0) is implementation-defined and may return NULL or a non-NULL
    // pointer that must not be dereferenced.
    if (active_neurons == 0) return;

    // Populate active neuron IDs
    decision->active_neuron_ids = nimcp_malloc(active_neurons * sizeof(uint32_t));
    if (!decision->active_neuron_ids) {
        // W7-10: Reset num_active_neurons to 0 on alloc failure.
        // Without this, num_active_neurons is non-zero but pointer is NULL,
        // causing callers that iterate active_neuron_ids to dereference NULL.
        decision->num_active_neurons = 0;
        set_error("Failed to allocate active neuron IDs array (%u neurons)", active_neurons);
        return;
    }
    for (uint32_t i = 0; i < active_neurons; i++) {
        decision->active_neuron_ids[i] = i;
    }
}


//=============================================================================
// Mirror Neuron Integration Helpers (Phase 10.11)
//=============================================================================

/**
 * @brief Convert brain decision to mirror neuron action
 *
 * WHAT: Transform brain decision into action_t for mirror neuron system
 * WHY:  Enable mirror neurons to learn from brain's own decisions
 * HOW:  Extract decision features, confidence, and output as action representation
 *
 * COMPLEXITY: O(n) where n = num_outputs (feature copying)
 *
 * @param decision Brain decision
 * @param action_id Unique action identifier
 * @param action_name Human-readable action name
 * @return action_t struct for mirror neuron system
 */
static action_t brain_decision_to_action(const brain_decision_t* decision,
                                         uint32_t action_id,
                                         const char* action_name)
{
    action_t action;
    memset(&action, 0, sizeof(action_t));

    if (!decision || !action_name) {
        return action;
    }

    action.action_id = action_id;
    strncpy(action.action_name, action_name, sizeof(action.action_name) - 1);
    action.agent_id = 0;  // 0 = self
    action.timestamp = nimcp_time_get_ms();
    action.confidence = decision->confidence;

    // Use output activations as action features (up to 32)
    action.num_features = (decision->output_size < 32) ? decision->output_size : 32;
    for (uint32_t i = 0; i < action.num_features; i++) {
        action.features[i] = decision->output_vector[i];
    }

    return action;
}


/**
 * @brief Convert input features to observed action
 *
 * WHAT: Transform input features into action_t for observation pathway
 * WHY:  Enable mirror neurons to learn from observed patterns
 * HOW:  Treat input as observed action with features
 *
 * COMPLEXITY: O(n) where n = num_features (copying)
 *
 * @param features Input features
 * @param num_features Number of features
 * @param agent_id ID of agent performing action (0 = self, >0 = other)
 * @return action_t struct for mirror neuron system
 */
static action_t features_to_action(const float* features, uint32_t num_features,
                                   uint32_t agent_id)
{
    action_t action;
    memset(&action, 0, sizeof(action_t));

    if (!features) {
        return action;
    }

    action.action_id = 0;  // Will be assigned by mirror neuron system
    snprintf(action.action_name, sizeof(action.action_name), "observed_%u", agent_id);
    action.agent_id = agent_id;
    action.timestamp = nimcp_time_get_ms();
    action.confidence = 1.0F;

    // Copy features (up to 32)
    action.num_features = (num_features < 32) ? num_features : 32;
    for (uint32_t i = 0; i < action.num_features; i++) {
        action.features[i] = features[i];
    }

    return action;
}


/**
 * @brief Observe action performed by another agent (Phase 10.11)
 *
 * WHAT: Record observed action in mirror neuron system for observational learning
 * WHY:  Enable learning from watching others (imitation, social cognition)
 * HOW:  Convert input features to observed action and send to mirror neurons
 *
 * This is the OBSERVATION PATHWAY for mirror neurons. When the brain observes
 * another agent performing an action, this function records it for learning.
 *
 * USE CASES:
 * - Robot watching human demonstration
 * - Agent observing another agent's behavior
 * - Learning from video/sensor data of actions
 * - Social learning and imitation
 *
 * COMPLEXITY: O(n) where n = num_features
 * THREAD-SAFE: No (requires external synchronization)
 *
 * @param brain Brain handle
 * @param features Observed action features (sensor data, visual features, etc.)
 * @param num_features Number of features
 * @param agent_id ID of agent being observed (must be > 0, as 0 = self)
 * @return true on success, false on error
 */
// brain_observe_action() - MOVED TO: src/core/brain/inference/nimcp_brain_inference.c

/**
 * @brief Free decision result
 *
 * WHY: Proper memory management for decision results
 * Handles all allocated sub-structures
 *
 * COMPLEXITY: O(1)
 *
 * @param decision Decision to free
 */
// brain_free_decision() - MOVED TO: src/core/brain/inference/nimcp_brain_inference.c

/**
 * @brief Batch inference
 *
 * WHY: More efficient than individual calls for large batches
 * Enables parallel processing opportunities
 *
 * COMPLEXITY: O(m*s*n) where m = num_inputs
 *
 * @param brain Brain handle
 * @param inputs Array of input vectors
 * @param num_inputs Number of inputs
 * @param features_per_input Features per input
 * @param decisions Output decisions array (allocated by caller)
 * @return true on success
 */
// brain_decide_batch() - MOVED TO: src/core/brain/inference/nimcp_brain_inference.c

//=============================================================================
// Persistence API
//=============================================================================

/**
 * @brief Save working memory state to file (Phase 10.2)
 *
 * WHAT: Serialize working memory items for COW snapshot persistence
 * WHY:  Preserve active representations across save/load/snapshot operations
 * HOW:  Write marker → size/capacity → each item's data
 *
 * COMPLEXITY: O(n*m) where n = items, m = avg item size
 *
 * @param wm Working memory instance (nullable)
 * @param file File handle (non-NULL)
 * @return true on success, false on error
 */
static bool save_working_memory_state(working_memory_t* wm, FILE* file)
{
    // Guard: NULL file handle
    if (!file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "save_working_memory_state: file is NULL");
        return false;
    }

    bool success = true;

    // Guard: No working memory → write marker and return
    if (!wm) {
        uint8_t has_wm = 0;
        if (fwrite(&has_wm, sizeof(uint8_t), 1, file) != 1) {
            success = false;
        }
        return success;
    }

    // Write existence marker
    uint8_t has_wm = 1;
    if (fwrite(&has_wm, sizeof(uint8_t), 1, file) != 1) {
        success = false;
    }

    // Get current state
    working_memory_stats_t stats;
    working_memory_get_stats(wm, &stats);

    // Count valid items first so the written count matches items actually serialized.
    // The load function reads this count and expects exactly that many items to follow.
    uint32_t valid_count = 0;
    for (uint32_t i = 0; i < stats.current_size; i++) {
        uint32_t item_size = 0;
        const float* item = working_memory_get(wm, i, &item_size);
        if (item && item_size > 0) {
            valid_count++;
        }
    }

    // Write metadata: valid_count (not current_size) so load reads the correct number
    if (fwrite(&valid_count, sizeof(uint32_t), 1, file) != 1) {
        success = false;
    }
    if (fwrite(&stats.capacity, sizeof(uint32_t), 1, file) != 1) {
        success = false;
    }

    // Write each valid item
    for (uint32_t i = 0; i < stats.current_size; i++) {
        uint32_t item_size = 0;
        const float* item = working_memory_get(wm, i, &item_size);

        // Guard: Invalid item → skip (already excluded from valid_count)
        if (!item || item_size == 0) {
            continue;
        }

        if (fwrite(&item_size, sizeof(uint32_t), 1, file) != 1) {
            success = false;
        }
        if (fwrite(item, sizeof(float), item_size, file) != item_size) {
            success = false;
        }
    }

    return success;
}


/**
 * @brief Save metadata file
 *
 * WHAT: Persist brain configuration and output labels
 * WHY:  Enable full state reconstruction on load
 * HOW:  Write config → labels → working memory state
 *
 * COMPLEXITY: O(k + n*m) where k = labels, n = WM items, m = item size
 *
 * @param brain Brain instance (non-NULL)
 * @param filepath Base filepath (non-NULL)
 * @return true on success, false on error
 */
static bool save_metadata(brain_t brain, const char* filepath)
{
    // Guard: NULL parameters handled by caller

    char meta_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(meta_path, sizeof(meta_path), "%s.meta", filepath);

    FILE* meta_file = fopen(meta_path, "wb");
    if (!meta_file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "save_metadata: meta_file is NULL");
        return false;
    }

    bool success = true;

    // Write version header (v1.0 format)
    nimcp_file_header_t header = {
        .magic = {NIMCP_MAGIC_0, NIMCP_MAGIC_1, NIMCP_MAGIC_2, NIMCP_MAGIC_3},
        .version_major = NIMCP_FORMAT_VERSION_MAJOR,
        .version_minor = NIMCP_FORMAT_VERSION_MINOR,
        .flags = 0,  // No compression/encryption yet
        .reserved = 0
    };
    if (fwrite(&header, sizeof(nimcp_file_header_t), 1, meta_file) != 1) {
        success = false;
    }

    // Write configuration
    if (fwrite(&brain->config, sizeof(brain_config_t), 1, meta_file) != 1) {
        success = false;
    }
    if (fwrite(&brain->num_output_labels, sizeof(uint32_t), 1, meta_file) != 1) {
        success = false;
    }

    // Write output labels
    for (uint32_t i = 0; i < brain->num_output_labels; i++) {
        uint32_t len = strlen(brain->output_labels[i]) + 1;
        if (fwrite(&len, sizeof(uint32_t), 1, meta_file) != 1) {
            success = false;
        }
        if (fwrite(brain->output_labels[i], len, 1, meta_file) != 1) {
            success = false;
        }
    }

    // Phase 10.2: Save working memory state
    if (!save_working_memory_state(brain->working_memory, meta_file)) {
        success = false;
    }

    // Save brain statistics (performance metrics)
    if (fwrite(&brain->stats, sizeof(brain_stats_t), 1, meta_file) != 1) {
        success = false;
    }

    // Save wellbeing state (Phase 9.3)
    if (fwrite(&brain->last_distress, sizeof(distress_assessment_t), 1, meta_file) != 1) {
        success = false;
    }
    if (fwrite(&brain->wellbeing_monitoring_enabled, sizeof(bool), 1, meta_file) != 1) {
        success = false;
    }
    if (fwrite(&brain->wellbeing_check_interval_ms, sizeof(uint64_t), 1, meta_file) != 1) {
        success = false;
    }
    if (fwrite(&brain->last_wellbeing_check_time, sizeof(uint64_t), 1, meta_file) != 1) {
        success = false;
    }

    // Save simulation time tracking
    if (fwrite(&brain->current_time_us, sizeof(uint64_t), 1, meta_file) != 1) {
        success = false;
    }
    if (fwrite(&brain->last_glial_update_us, sizeof(uint64_t), 1, meta_file) != 1) {
        success = false;
    }

    // Save knowledge system state (if exists)
    bool has_knowledge = (brain->knowledge != NULL);
    if (fwrite(&has_knowledge, sizeof(bool), 1, meta_file) != 1) {
        success = false;
    }
    if (has_knowledge) {
        char knowledge_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(knowledge_path, sizeof(knowledge_path), "%s.knowledge", filepath);
        knowledge_save(brain->knowledge, knowledge_path);
    }

    // Save emotional system state (Phase 10.2 - NOT A MODULE)
    // Note: Emotional tagging uses stateless utility functions, not a system object
    bool has_emotional = false;  // No emotional_system module (just tagging functions)
    if (fwrite(&has_emotional, sizeof(bool), 1, meta_file) != 1) {
        success = false;
    }

    // Save executive controller state (Phase 10.3 - if exists)
    bool has_executive = (brain->executive != NULL);
    if (fwrite(&has_executive, sizeof(bool), 1, meta_file) != 1) {
        success = false;
    }
    if (has_executive) {
        // WHAT: Save executive controller state to separate file
        // WHY:  Preserve task queue, statistics, and configuration
        // HOW:  Use executive_save API with dedicated file
        char executive_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(executive_path, sizeof(executive_path), "%s.executive", filepath);
        FILE* exec_file = fopen(executive_path, "wb");
        if (exec_file) {
            executive_save(brain->executive, exec_file);
            fclose(exec_file);
        }
    }

    // Save sleep system state (Phase 10.4)
    // Sleep system is embedded struct, always save
    // TODO: Add sleep_system_save API when available
    // For now, skip to maintain backward compatibility

    // Save pink noise neuromodulator state (if exists)
    bool has_pink_noise = (brain->pink_noise != NULL);
    if (fwrite(&has_pink_noise, sizeof(bool), 1, meta_file) != 1) {
        success = false;
    }
    if (has_pink_noise) {
        // WHAT: Save pink noise neuromodulator state to separate file
        // WHY:  Preserve neuromodulator levels and pink noise generators
        // HOW:  Use neuromod_pink_save API with dedicated file
        char pink_noise_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(pink_noise_path, sizeof(pink_noise_path), "%s.pink_noise", filepath);
        FILE* pink_file = fopen(pink_noise_path, "wb");
        if (pink_file) {
            neuromod_pink_save(brain->pink_noise, pink_file);
            fclose(pink_file);
        }
    }

    // Save mirror neurons state (Phase 10.11 - if exists)
    bool has_mirror_neurons = (brain->mirror_neurons != NULL);
    if (fwrite(&has_mirror_neurons, sizeof(bool), 1, meta_file) != 1) {
        success = false;
    }
    if (has_mirror_neurons) {
        // WHAT: Save mirror neuron system state to separate file
        // WHY:  Preserve learned action associations and statistics
        // HOW:  Use mirror_neurons_save API with dedicated file
        char mirror_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(mirror_path, sizeof(mirror_path), "%s.mirror_neurons", filepath);
        FILE* mirror_file = fopen(mirror_path, "wb");
        if (mirror_file) {
            mirror_neurons_save(brain->mirror_neurons, mirror_file);
            fclose(mirror_file);
        }
    }

    fclose(meta_file);
    return success;
}


/**
 * @brief Save brain to file
 *
 * WHY: Enables model persistence across sessions
 * Saves both network and metadata
 *
 * COMPLEXITY: O(n*c) where n = neurons, c = connections
 *
 * @param brain Brain handle
 * @param filepath Path to save to
 * @return true on success
 */
// brain_save() - MOVED TO: src/core/brain/persistence/nimcp_brain_persistence.c

/**
 * @brief Load single working memory item from file (Phase 10.2)
 *
 * WHAT: Deserialize one item and add to working memory buffer
 * WHY:  Restore individual active representations
 * HOW:  Read size → allocate → read data → add to buffer → free temp
 *
 * COMPLEXITY: O(m) where m = item size
 *
 * @param wm Working memory instance (non-NULL)
 * @param file File handle (non-NULL)
 * @return true on success, false on error
 */
static bool load_working_memory_item(working_memory_t* wm, FILE* file)
{
    #define MAX_ITEM_SIZE 10000  // Sanity check limit

    // Guard: NULL parameters
    if (!wm || !file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "load_working_memory_item: required parameter is NULL (wm, file)");
        return false;
    }

    uint32_t item_size = 0;
    if (fread(&item_size, sizeof(uint32_t), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_working_memory_item: validation failed");
        return false;
    }

    // Guard: Invalid size
    if (item_size == 0 || item_size > MAX_ITEM_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_working_memory_item: item_size is zero");
        return false;
    }

    // Allocate temporary buffer
    float* item = nimcp_malloc(item_size * sizeof(float));
    if (!item) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "load_working_memory_item: item is NULL");
        return false;
    }

    // Read item data
    if (fread(item, sizeof(float), item_size, file) != item_size) {
        nimcp_free(item);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_working_memory_item: validation failed");
        return false;
    }

    // Add to working memory (use default salience since not persisted)
    const float DEFAULT_SALIENCE = 0.5F;
    bool success = working_memory_add(wm, item, item_size, DEFAULT_SALIENCE);

    nimcp_free(item);
    return success;

    #undef MAX_ITEM_SIZE
}


/**
 * @brief Load working memory state from file (Phase 10.2)
 *
 * WHAT: Deserialize working memory items from COW snapshot
 * WHY:  Restore active representations after load/restore
 * HOW:  Read marker → initialize if needed → load each item
 *
 * COMPLEXITY: O(n*m) where n = items, m = avg item size
 *
 * @param brain Brain instance (non-NULL)
 * @param file File handle (non-NULL)
 * @return true on success (non-fatal on WM failure)
 */
static bool load_working_memory_state(brain_t brain, FILE* file)
{
    // Guard: NULL parameters
    if (!brain || !file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "load_working_memory_state: required parameter is NULL (brain, file)");
        return false;
    }

    // Read existence marker
    uint8_t has_wm = 0;
    if (fread(&has_wm, sizeof(uint8_t), 1, file) != 1) {
        return true;  // EOF or old format → non-fatal
    }

    // Guard: No working memory in snapshot
    if (has_wm == 0) {
        return true;  // Nothing to load → success
    }

    // Read metadata
    uint32_t wm_size = 0, wm_capacity = 0;
    if (fread(&wm_size, sizeof(uint32_t), 1, file) != 1) {
        return true;  // Non-fatal
    }
    if (fread(&wm_capacity, sizeof(uint32_t), 1, file) != 1) {
        return true;  // Non-fatal
    }

    // Initialize working memory if enabled but not yet created
    if (!brain->working_memory && brain->config.enable_working_memory) {
        if (!init_working_memory_subsystem(brain)) {
            LOG_WARN(LOG_MODULE, "Failed to initialize working memory on load");
            return true;  // Non-fatal: continue without WM
        }
    }

    // Guard: Working memory not available
    if (!brain->working_memory) {
        return true;  // Skip loading → non-fatal
    }

    // Load each item
    for (uint32_t i = 0; i < wm_size; i++) {
        load_working_memory_item(brain->working_memory, file);
        // Errors loading individual items are non-fatal
    }

    return true;
}


/**
 * @brief Load metadata file
 *
 * WHAT: Deserialize brain configuration and output labels
 * WHY:  Reconstruct full brain state from persistent storage
 * HOW:  Read config → validate → load labels → load working memory
 *
 * COMPLEXITY: O(k + n*m) where k = labels, n = WM items, m = item size
 *
 * @param brain Brain instance (non-NULL)
 * @param filepath Base filepath (non-NULL)
 * @return true on success, false on error
 */
static bool load_metadata(brain_t brain, const char* filepath)
{
    char meta_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(meta_path, sizeof(meta_path), "%s.meta", filepath);

    FILE* meta_file = fopen(meta_path, "rb");
    if (!meta_file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "load_metadata: meta_file is NULL");
        return false;
    }

    // Try to read version header
    nimcp_file_header_t header;
    size_t header_read = fread(&header, sizeof(nimcp_file_header_t), 1, meta_file);

    bool has_version_header = false;
    if (header_read == 1) {
        // Check magic bytes
        if (header.magic[0] == NIMCP_MAGIC_0 &&
            header.magic[1] == NIMCP_MAGIC_1 &&
            header.magic[2] == NIMCP_MAGIC_2 &&
            header.magic[3] == NIMCP_MAGIC_3) {

            has_version_header = true;

            // Validate version compatibility
            if (header.version_major != NIMCP_FORMAT_VERSION_MAJOR) {
                LOG_ERROR(LOG_MODULE, "Incompatible format version %u.%u (expected %u.x)",
                          header.version_major, header.version_minor,
                          NIMCP_FORMAT_VERSION_MAJOR);
                fclose(meta_file);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_metadata: validation failed");
                return false;
            }

            LOG_INFO(LOG_MODULE, "Loading brain metadata v%u.%u",
                     header.version_major, header.version_minor);

            // TODO: Handle format flags (compression, encryption)
            if (header.flags & NIMCP_FORMAT_FLAG_COMPRESSED) {
                LOG_WARN(LOG_MODULE, "Compressed format not yet supported, skipping");
            }
            if (header.flags & NIMCP_FORMAT_FLAG_ENCRYPTED) {
                LOG_WARN(LOG_MODULE, "Encrypted format not yet supported, skipping");
            }
        } else {
            // Not a versioned file - rewind and read as legacy format
            has_version_header = false;
            fseek(meta_file, 0, SEEK_SET);
        }
    } else {
        // File too small for header - legacy format
        fseek(meta_file, 0, SEEK_SET);
    }

    if (!has_version_header) {
        LOG_INFO(LOG_MODULE, "Loading brain metadata (legacy format, no version header)");
    }

    // Read configuration - failure caught by subsequent field validation
    if (fread(&brain->config, sizeof(brain_config_t), 1, meta_file) != 1) {
        LOG_ERROR(LOG_MODULE, "Failed to read brain config from metadata file");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_metadata: validation failed");
        return false;
    }

    // Validate brain->config fields after reading
    // Validate learning_rate (float field - NaN/Inf check)
    if (!nimcp_validate_float_field(&brain->config.learning_rate,
                                    sizeof(brain->config.learning_rate))) {
        LOG_ERROR(LOG_MODULE, "Invalid learning_rate in loaded config (NaN or Inf)");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_metadata: operation failed");
        return false;
    }

    // Validate sparsity_target (float field - NaN/Inf check)
    if (!nimcp_validate_float_field(&brain->config.sparsity_target,
                                    sizeof(brain->config.sparsity_target))) {
        LOG_ERROR(LOG_MODULE, "Invalid sparsity_target in loaded config (NaN or Inf)");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_metadata: operation failed");
        return false;
    }

    // Validate num_inputs (integer field, range 1-10000)
    if (!nimcp_validate_integer_field(&brain->config.num_inputs,
                                      sizeof(brain->config.num_inputs))) {
        LOG_ERROR(LOG_MODULE, "Invalid num_inputs in loaded config");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_metadata: operation failed");
        return false;
    }
    if (brain->config.num_inputs < 1 || brain->config.num_inputs > 10000) {
        LOG_ERROR(LOG_MODULE, "num_inputs out of range (1-10000): %u", brain->config.num_inputs);
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_metadata: validation failed");
        return false;
    }

    // Validate num_outputs (integer field, range 1-10000)
    if (!nimcp_validate_integer_field(&brain->config.num_outputs,
                                      sizeof(brain->config.num_outputs))) {
        LOG_ERROR(LOG_MODULE, "Invalid num_outputs in loaded config");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_metadata: operation failed");
        return false;
    }
    if (brain->config.num_outputs < 1 || brain->config.num_outputs > 10000) {
        LOG_ERROR(LOG_MODULE, "num_outputs out of range (1-10000): %u",
                  brain->config.num_outputs);
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_metadata: validation failed");
        return false;
    }

    if (fread(&brain->num_output_labels, sizeof(uint32_t), 1, meta_file) != 1) {
        LOG_ERROR(LOG_MODULE, "Failed to read num_output_labels from metadata file");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_metadata: validation failed");
        return false;
    }

    // SECURITY: Strict validation limits to prevent buffer overflow attacks
    #define MAX_OUTPUT_LABELS 10000     // Maximum number of labels
    #define MAX_LABEL_LENGTH 256        // Maximum length of a single label

    // Validate num_output_labels (range 0-10000, 0 means no labels)
    if (!nimcp_validate_integer_field(&brain->num_output_labels,
                                      sizeof(brain->num_output_labels))) {
        LOG_ERROR(LOG_MODULE, "Invalid num_output_labels in loaded metadata");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_metadata: operation failed");
        return false;
    }
    if (brain->num_output_labels > MAX_OUTPUT_LABELS) {
        LOG_ERROR(LOG_MODULE, "SECURITY: num_output_labels %u exceeds maximum %d - file may be maliciously crafted",
                  brain->num_output_labels, MAX_OUTPUT_LABELS);
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_metadata: validation failed");
        return false;
    }

    // Handle case where there are no labels
    if (brain->num_output_labels == 0) {
        brain->output_labels = NULL;
        fclose(meta_file);
        return true;
    }

    brain->output_labels = nimcp_malloc(brain->num_output_labels * sizeof(char*));
    if (!brain->output_labels) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate output_labels array");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "load_metadata: brain->output_labels is NULL");
        return false;
    }

    uint32_t i;
    for (i = 0; i < brain->num_output_labels; i++) {
        uint32_t len;
        if (fread(&len, sizeof(uint32_t), 1, meta_file) != 1) {
            LOG_ERROR(LOG_MODULE, "Failed to read label length at index %u", i);
            goto cleanup;
        }

        // SECURITY: Validate label length to prevent buffer overflow
        if (len == 0 || len > MAX_LABEL_LENGTH) {
            LOG_ERROR(LOG_MODULE, "SECURITY: Label %u length %u exceeds maximum %d - file may be maliciously crafted",
                      i, len, MAX_LABEL_LENGTH);
            goto cleanup;
        }

        // Validate integer field integrity
        if (!nimcp_validate_integer_field(&len, sizeof(len))) {
            LOG_ERROR(LOG_MODULE, "Invalid label length at index %u", i);
            goto cleanup;
        }

        brain->output_labels[i] = nimcp_malloc(len);
        if (!brain->output_labels[i]) {
            LOG_ERROR(LOG_MODULE, "Failed to allocate label at index %u", i);
            goto cleanup;
        }

        if (fread(brain->output_labels[i], len, 1, meta_file) != 1) {
            LOG_ERROR(LOG_MODULE, "Failed to read label content at index %u", i);
            goto cleanup;
        }
    }

    // Phase 10.2: Load working memory state
    load_working_memory_state(brain, meta_file);

    // Load brain statistics (performance metrics)
    if (fread(&brain->stats, sizeof(brain_stats_t), 1, meta_file) != 1) {
        // Non-fatal: use default stats if not available (backward compatibility)
        init_brain_stats(&brain->stats, brain->config.task_name, brain->config.size,
                        brain->config.num_inputs, brain->config.learning_rate);
    }

    // Load wellbeing state (Phase 9.3)
    if (fread(&brain->last_distress, sizeof(distress_assessment_t), 1, meta_file) == 1 &&
        fread(&brain->wellbeing_monitoring_enabled, sizeof(bool), 1, meta_file) == 1 &&
        fread(&brain->wellbeing_check_interval_ms, sizeof(uint64_t), 1, meta_file) == 1 &&
        fread(&brain->last_wellbeing_check_time, sizeof(uint64_t), 1, meta_file) == 1) {
        // Successfully loaded wellbeing state
    }

    // Load simulation time tracking (may not exist in old snapshots)
    if (fread(&brain->current_time_us, sizeof(uint64_t), 1, meta_file) == 1 &&
        fread(&brain->last_glial_update_us, sizeof(uint64_t), 1, meta_file) == 1) {
        // Successfully loaded time tracking
    } else {
        // Old snapshot, initialize to 0
        brain->current_time_us = 0;
        brain->last_glial_update_us = 0;
    }

    // Load knowledge system state (if exists)
    bool has_knowledge = false;
    if (fread(&has_knowledge, sizeof(bool), 1, meta_file) == 1 && has_knowledge) {
        char knowledge_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(knowledge_path, sizeof(knowledge_path), "%s.knowledge", filepath);
        brain->knowledge = knowledge_load(knowledge_path);
        // Non-fatal if knowledge load fails
    }

    // Load emotional system state (Phase 10.2 - NOT A MODULE)
    // Note: Emotional tagging uses stateless utility functions, not a system object
    bool has_emotional = false;
    if (fread(&has_emotional, sizeof(bool), 1, meta_file) == 1 && has_emotional) {
        // Placeholder for backward compatibility (old saves might have this flag set)
        // No action needed - emotional tagging uses stateless functions
    }

    // Load executive controller state (Phase 10.3 - if exists)
    bool has_executive = false;
    if (fread(&has_executive, sizeof(bool), 1, meta_file) == 1 && has_executive) {
        // WHAT: Load executive controller state from separate file
        // WHY:  Restore task queue, statistics, and configuration
        // HOW:  Use executive_load API with dedicated file
        char executive_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(executive_path, sizeof(executive_path), "%s.executive", filepath);
        FILE* exec_file = fopen(executive_path, "rb");
        if (exec_file) {
            brain->executive = executive_load(exec_file);
            fclose(exec_file);
            // Set brain reference for neuromodulation integration
            if (brain->executive) {
                executive_set_brain(brain->executive, brain);
            }
        }
    }

    // Load pink noise neuromodulator state (if exists)
    bool has_pink_noise = false;
    if (fread(&has_pink_noise, sizeof(bool), 1, meta_file) == 1 && has_pink_noise) {
        // WHAT: Load pink noise neuromodulator state from separate file
        // WHY:  Restore neuromodulator levels and pink noise generators
        // HOW:  Use neuromod_pink_load API with dedicated file
        char pink_noise_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(pink_noise_path, sizeof(pink_noise_path), "%s.pink_noise", filepath);
        FILE* pink_file = fopen(pink_noise_path, "rb");
        if (pink_file) {
            brain->pink_noise = neuromod_pink_load(pink_file);
            fclose(pink_file);
        }
    }

    // Load mirror neurons state (Phase 10.11 - if exists)
    bool has_mirror_neurons = false;
    if (fread(&has_mirror_neurons, sizeof(bool), 1, meta_file) == 1 && has_mirror_neurons) {
        // WHAT: Load mirror neuron system state from separate file
        // WHY:  Restore learned action associations and statistics
        // HOW:  Use mirror_neurons_load API with dedicated file
        char mirror_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(mirror_path, sizeof(mirror_path), "%s.mirror_neurons", filepath);
        FILE* mirror_file = fopen(mirror_path, "rb");
        if (mirror_file) {
            brain->mirror_neurons = mirror_neurons_load(mirror_file);
            fclose(mirror_file);
            // Set brain reference for neuromodulation integration
            if (brain->mirror_neurons) {
                mirror_neurons_set_brain(brain->mirror_neurons, brain);
            }
        }
    }

    fclose(meta_file);
    return true;

cleanup:
    // Free any allocated labels before the failed one
    for (uint32_t j = 0; j < i; j++) {
        nimcp_free(brain->output_labels[j]);
    }
    nimcp_free(brain->output_labels);
    brain->output_labels = NULL;
    fclose(meta_file);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: operation failed");
    return false;
}


/**
 * @brief Load brain from file
 *
 * WHY: Restores saved brain state
 * Reconstructs network and metadata
 *
 * COMPLEXITY: O(n*c) where n = neurons, c = connections
 *
 * @param filepath Path to load from
 * @return Brain handle or NULL on error
 */
// brain_load() - MOVED TO: src/core/brain/persistence/nimcp_brain_persistence.c

//=============================================================================
// Snapshot API - Named State Snapshots
//=============================================================================

/**
 * @brief Create snapshot directory if it doesn't exist
 *
 * @param snapshot_dir Directory path
 * @return true on success, false on error
 */
static bool ensure_snapshot_dir(const char* snapshot_dir)
{
    if (!snapshot_dir) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensure_snapshot_dir: snapshot_dir is NULL");
        return false;
    }

    // Try to create directory (will fail silently if already exists)
    #ifdef _WIN32
    _mkdir(snapshot_dir);
    #else
    mkdir(snapshot_dir, 0755);
    #endif

    return true;
}


/**
 * @brief Get default snapshot directory
 *
 * @param brain Brain instance
 * @return Snapshot directory path
 */
static const char* get_snapshot_dir(brain_t brain)
{
    if (brain->config.snapshot_dir) {
        return brain->config.snapshot_dir;
    }
    return "./snapshots";  // Default
}
