//=============================================================================
// nimcp_brain_learning.h - Brain Learning Module
//=============================================================================
/**
 * @file nimcp_brain_learning.h
 * @brief Learning, training, and reward-based learning for brain networks
 *
 * WHAT: High-level supervised and reinforcement learning APIs
 * WHY:  Provides labeled example learning, batch training, reward signals
 * HOW:  Wraps adaptive network learning with epistemic filtering, curiosity,
 *       quantum optimization, and biological security
 *
 * ARCHITECTURE:
 * - Supervised learning: brain_learn_example(), brain_learn_batch()
 * - Reinforcement learning: brain_apply_reward_learning()
 * - Knowledge distillation: brain_learn_from_llm()
 * - Meta-learning: Adaptive learning rate based on loss trends
 * - Quantum optimization: Periodic weight optimization via quantum annealing
 *
 * PERFORMANCE:
 * - Single example: O(s*n) where s = sparsity, n = active_neurons
 * - Batch learning: O(m*s*n) where m = num_examples
 * - Quantum annealing: O(annealing_steps * num_weights) - run infrequently
 *
 * BIOLOGICAL INTEGRATION:
 * - Epistemic filtering: Skepticism toward low-quality training data
 * - Curiosity-driven LR boost: 40% faster learning on novel patterns
 * - Excitotoxicity monitoring: Security against adversarial attacks
 * - Emotional tagging: Joy for success, remorse for errors
 * - Memory encoding: Engram formation during learning
 *
 * @author NIMCP Development Team
 * @version 1.0
 */

#ifndef NIMCP_BRAIN_LEARNING_H
#define NIMCP_BRAIN_LEARNING_H

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Learning API - Public Functions
//=============================================================================

/**
 * @brief Learn from single labeled example
 *
 * WHAT: Supervised learning from a single input-label pair
 * WHY:  Primary learning interface - updates network weights to match label
 * HOW:  Forward pass, backpropagation with task-specific loss, weight update
 *
 * FEATURES:
 * - Epistemic filtering: Reduces confidence for suspicious/biased labels
 * - Curiosity boost: Increases learning rate for novel inputs (up to 40%)
 * - Biological security: Monitors for excitotoxicity attacks
 * - Emotional integration: Joy for low loss, remorse for high loss
 * - Memory encoding: Creates engrams for episodic memory
 * - Quantum optimization: Periodic weight optimization (configurable)
 * - Adaptive LR: Adjusts learning rate based on loss trend
 *
 * COMPLEXITY: O(s*n) where s = sparsity, n = active_neurons
 * PERFORMANCE: ~0.1-1ms for small networks, ~10ms for large
 *
 * BIOLOGICAL BASIS:
 * - Hebbian learning: "Cells that fire together wire together"
 * - Dopamine-modulated plasticity: Reward prediction errors
 * - Sleep pressure: Learning accumulates adenosine (sleep need)
 * - Synaptic tagging: Eligibility traces for temporal credit assignment
 *
 * @param brain Brain handle
 * @param features Input feature vector
 * @param num_features Feature count (must match brain->config.num_inputs)
 * @param label Target label (string, auto-creates one-hot encoding)
 * @param confidence Training weight [0.0-1.0] (epistemic multiplier applied)
 * @return Loss value (lower is better) or -1.0f on error
 *
 * @example
 * float features[] = {0.5, 0.8, 0.2};
 * float loss = brain_learn_example(brain, features, 3, "cat", 0.9f);
 * if (loss < 0) {
 *     // Error occurred
 * }
 */
float brain_learn_example(brain_t brain, const float* features, uint32_t num_features,
                          const char* label, float confidence);

/**
 * @brief Learn from a dense target vector (distillation / generative training)
 *
 * WHAT: Train toward an arbitrary dense output vector instead of a one-hot label
 * WHY:  Enables teacher-model distillation and semantic embedding training
 * HOW:  Builds training_example_t with dense target, uses LEARN_MODE_DISTILLATION
 *
 * @param brain Brain handle
 * @param features Input feature vector
 * @param num_features Feature count (must match brain->config.num_inputs)
 * @param target Dense target output vector
 * @param target_size Target vector size (must match brain->config.num_outputs)
 * @param label Optional semantic label for tracking (can be NULL)
 * @param confidence Training weight [0.0-1.0]
 * @return Loss value (lower is better) or -1.0f on error
 */
float brain_learn_vector(brain_t brain, const float* features, uint32_t num_features,
                          const float* target, uint32_t target_size,
                          const char* label, float confidence);

/**
 * @brief Learn from batch of examples
 *
 * WHAT: Mini-batch gradient descent for efficient learning
 * WHY:  More efficient than individual calls, enables batch statistics
 * HOW:  Iteratively calls brain_learn_example() and averages loss
 *
 * COMPLEXITY: O(m*s*n) where m = num_examples
 *
 * ADVANTAGES:
 * - Batch normalization: More stable gradients
 * - Computational efficiency: Better memory locality
 * - Statistical robustness: Averages out noise
 *
 * @param brain Brain handle
 * @param examples Array of training examples
 * @param num_examples Example count
 * @return Average loss over batch or -1.0f on error
 *
 * @example
 * brain_example_t batch[] = {
 *     {.features = cat_features, .num_features = 3, .label = "cat", .confidence = 0.9f},
 *     {.features = dog_features, .num_features = 3, .label = "dog", .confidence = 0.85f}
 * };
 * float avg_loss = brain_learn_batch(brain, batch, 2);
 */
float brain_learn_batch(brain_t brain, const brain_example_t* examples, uint32_t num_examples);

/**
 * @brief Learn batch with per-example loss output
 *
 * @param brain Brain handle
 * @param examples Array of examples
 * @param num_examples Example count
 * @param losses_out Caller-allocated float[num_examples] for per-example losses (can be NULL)
 * @return Average loss or -1 on error
 */
float brain_learn_batch_detailed(brain_t brain, const brain_example_t* examples,
                                 uint32_t num_examples, float* losses_out);

/**
 * @brief Apply reward-based reinforcement learning
 *
 * WHAT: Three-factor learning rule (Hebbian + Reward + Dopamine)
 * WHY:  Enable temporal credit assignment for RL tasks
 * HOW:  Apply reward to eligibility-traced synapses
 *
 * BIOLOGY:
 * - Eligibility traces: Mark recently active synapses ("synaptic tags")
 * - Dopamine bursts: Trigger weight consolidation ("capture")
 * - Reward signal: Modulates synaptic changes (Frey & Morris 1997)
 *
 * COMPLEXITY: O(n × s) where n = neurons, s = synapses_per_neuron
 * USE CASE: Reinforcement learning, temporal credit assignment, game playing
 *
 * ALGORITHM:
 * 1. Forward pass marks active synapses with eligibility traces
 * 2. Reward signal converts traces → weight changes
 * 3. Positive reward strengthens, negative reward weakens
 * 4. Trace decay ensures temporal locality (recent actions credited)
 *
 * @param brain Brain handle
 * @param reward Reward signal in range [-1.0, 1.0]
 *               - Positive: Strengthen recently active connections
 *               - Negative: Weaken recently active connections
 *               - Zero: No change (neutral outcome)
 * @return Number of synapses modified
 *
 * @example
 * // Agent makes a decision
 * brain_decide(brain, state, 3);
 *
 * // Environment provides reward
 * if (action_succeeded) {
 *     brain_apply_reward_learning(brain, 1.0f);  // Positive reinforcement
 * } else {
 *     brain_apply_reward_learning(brain, -0.5f); // Negative reinforcement
 * }
 */
uint32_t brain_apply_reward_learning(brain_t brain, float reward);

/**
 * @brief Enable multi-network training (LNN + CNN alongside adaptive)
 *
 * WHAT: Creates auxiliary LNN and CNN networks matched to the brain's dimensions,
 *       enabling ensemble training through the dispatch system.
 * WHY:  Ensemble learning: different architectures capture different patterns.
 *       LNN captures temporal dynamics, CNN captures spatial features,
 *       Adaptive SNN captures spike-timing correlations.
 * HOW:  Creates LNN network (NCP architecture), CNN trainer (dense layers),
 *       initializes training contexts, sets active_network_type to HYBRID.
 *
 * BIOLOGICAL BASIS:
 * - Multiple cortical pathways process same input through different architectures
 * - Ensemble coding: population-level diversity improves robustness
 * - Complementary learning systems (McClelland et al. 1995)
 *
 * COMPLEXITY: O(1) - Network creation is allocation-dominated
 *
 * @param brain Brain to augment
 * @return 0 on success, -1 on error
 */
int brain_enable_multi_network_training(brain_t brain);

/**
 * @brief Learn by querying an LLM teacher
 *
 * WHAT: Knowledge distillation from large language model to brain network
 * WHY:  Transfer LLM knowledge into fast, compact neural network
 * HOW:  Query LLM for label/confidence, then call brain_learn_example()
 *
 * COMPLEXITY: O(s*n) + LLM query time
 * USE CASE: Compress LLM knowledge into efficient brain, few-shot learning
 *
 * KNOWLEDGE DISTILLATION:
 * - Teacher: Large, slow, accurate model (LLM)
 * - Student: Small, fast, approximate model (brain)
 * - Transfer: Student learns to mimic teacher's decisions
 *
 * BENEFITS:
 * - Speed: Brain inference ~1000x faster than LLM
 * - Size: Brain ~1000x smaller than LLM
 * - Deployment: Brain runs offline, no API costs
 * - Interpretability: Brain structure is inspectable
 *
 * @param brain Brain handle (student network)
 * @param input Input features to query teacher about
 * @param num_features Feature count
 * @param llm_fn Teacher function (LLM query callback)
 * @param llm_context Context passed to teacher function
 * @return Loss value or -1.0f on error
 *
 * @example
 * float llm_teacher(const float* input, uint32_t n, void* ctx,
 *                   char* label, size_t label_size) {
 *     // Query LLM API with input features
 *     // Return confidence, write label to buffer
 *     return 0.95f;
 * }
 *
 * float loss = brain_learn_from_llm(brain, features, 3, llm_teacher, NULL);
 */
float brain_learn_from_llm(brain_t brain, const float* input, uint32_t num_features,
                           llm_teacher_fn_t llm_fn, void* llm_context);

//=============================================================================
// Internal Learning Helpers (used by brain core)
//=============================================================================

/**
 * @brief Find or create output label index
 *
 * WHAT: Maps string labels to numeric indices for one-hot encoding
 * WHY:  Enables human-readable classification with numeric targets
 * HOW:  Linear search through existing labels, create new if not found
 *
 * COMPLEXITY: O(k) where k = num_existing_labels
 * OPTIMIZATION: Linear search sufficient for small label sets (<100)
 *
 * @param brain Brain handle
 * @param label Label string to find/create
 * @return Label index, or 0 if capacity exceeded
 */
uint32_t nimcp_brain_learning_get_or_create_label_index(brain_t brain, const char* label);

/**
 * @brief Convert label to one-hot encoded output vector
 *
 * WHAT: Transforms string label to neural network target vector
 * WHY:  One-hot encoding is standard for classification tasks
 * HOW:  Zero all outputs, set label_index to confidence value
 *
 * COMPLEXITY: O(n) where n = num_outputs
 *
 * @param brain Brain handle
 * @param label Label string
 * @param output Output buffer (pre-allocated, size = num_outputs)
 * @param confidence Value to set for label index (typically 0.0-1.0)
 */
void nimcp_brain_learning_label_to_output(brain_t brain, const char* label,
                                          float* output, float confidence);

/**
 * @brief Adapt learning rate based on loss trend (meta-learning)
 *
 * WHAT: Online meta-learning - adjust LR based on recent loss history
 * WHY:  Faster convergence with adaptive step sizes
 * HOW:  Track loss in circular buffer, compute trend, scale LR
 *
 * ALGORITHM:
 * 1. Store loss in rolling window (size 10)
 * 2. Compute recent_avg vs older_avg
 * 3. If loss decreasing → accelerate (LR *= 1.05)
 * 4. If loss increasing → slow down (LR *= 0.9)
 * 5. Clamp to [0.1x, 10x] of base rate
 *
 * BIOLOGICAL BASIS:
 * - Meta-learning: "Learning to learn" (prefrontal cortex)
 * - Synaptic homeostasis: Plasticity rates self-regulate
 * - Exploration-exploitation: Adapt search strategy
 *
 * COMPLEXITY: O(1) - Fixed window size
 *
 * @param brain Brain handle
 * @param current_loss Most recent loss value
 */
void nimcp_brain_learning_adapt_learning_rate(brain_t brain, float current_loss);

/**
 * @brief Energy function for quantum annealing weight optimization
 *
 * WHAT: Objective function for quantum weight optimization
 * WHY:  Provides optimization target for escaping local minima
 * HOW:  L2 regularization (sum of squared weights)
 *
 * NOTE: Full implementation would use validation loss
 * This simplified version uses L2 norm as proxy
 *
 * COMPLEXITY: O(d) where d = vector dimension
 *
 * @param weights Weight vector to evaluate
 * @param dim Vector dimension
 * @param user_data Unused (reserved for future extensions)
 * @return Energy value (lower is better)
 */
float nimcp_brain_learning_quantum_weight_energy(const float* weights, uint32_t dim,
                                                  void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_LEARNING_H */
