/**
 * @file nimcp_language_generator.h
 * @brief Language generation engine using LNN temporal dynamics
 *
 * WHAT: Converts cognitive state vectors into token sequences (text)
 * WHY:  Bridges internal reasoning representations to natural language output
 * HOW:  LNN decoder evolves hidden state through ODE dynamics; output projection
 *       maps hidden state to vocabulary logits; decoding strategy selects tokens
 *
 * ARCHITECTURE:
 * ```
 * Cognitive State [state_dim]
 *        |
 *        v
 * ┌──────────────────────┐
 * │ Cognitive Projection  │  [state_dim x hidden_dim]
 * └──────────┬───────────┘
 *            v
 * ┌──────────────────────┐
 * │    LNN Decoder        │  NCP: sensory -> inter -> command -> motor
 * │  (ODE temporal dyn.)  │  Continuous-time hidden state evolution
 * └──────────┬───────────┘
 *            v
 * ┌──────────────────────┐
 * │  Output Projection    │  [hidden_dim x vocab_size] + bias
 * └──────────┬───────────┘
 *            v
 *     logits [vocab_size]
 *            |
 *   temperature / rep.penalty / strategy
 *            |
 *            v
 *     next token id
 * ```
 *
 * DECODING STRATEGIES:
 * - Greedy:      argmax(logits)
 * - Sampling:    categorical(softmax(logits / T))
 * - Top-K:       sample from top-k logits
 * - Top-P:       nucleus sampling (cumulative probability threshold)
 * - Beam Search: maintain beam_width hypotheses
 *
 * TRAINING:
 * - Cross-entropy loss on teacher-forced token sequences
 * - SGD weight updates on output projection + cognitive projection
 * - LNN forward step provides temporal context
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#ifndef NIMCP_LANGUAGE_GENERATOR_H
#define NIMCP_LANGUAGE_GENERATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

/** Default maximum tokens per generation */
#define LANGGEN_DEFAULT_MAX_SEQ_LEN    256

/** Default LNN hidden dimension */
#define LANGGEN_DEFAULT_HIDDEN_DIM     128

/** Default LNN neurons per layer */
#define LANGGEN_DEFAULT_NUM_NEURONS    64

/** Default sampling temperature */
#define LANGGEN_DEFAULT_TEMPERATURE    0.8f

/** Default nucleus sampling threshold */
#define LANGGEN_DEFAULT_TOP_P          0.9f

/** Default top-k sampling */
#define LANGGEN_DEFAULT_TOP_K          50

/** Default beam width */
#define LANGGEN_DEFAULT_BEAM_WIDTH     4

/** Default repetition penalty */
#define LANGGEN_DEFAULT_REP_PENALTY    1.2f

/** Default learning rate for training */
#define LANGGEN_DEFAULT_LEARNING_RATE  0.001f

/** Repetition tracking buffer capacity */
#define LANGGEN_RECENT_CAPACITY        64

/** End-of-sequence sentinel (configurable at create time) */
#define LANGGEN_DEFAULT_EOS_ID         2

/** Module ID for logging */
#define BIO_MODULE_LANGUAGE_GENERATOR  0x0500

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Token decoding strategy
 *
 * WHAT: Selects how the next token is chosen from logits
 * WHY:  Different strategies trade off diversity vs. quality
 */
typedef enum {
    GENERATION_GREEDY = 0,       /**< Always pick highest probability token */
    GENERATION_SAMPLING,         /**< Sample from full distribution */
    GENERATION_TOP_K,            /**< Sample from top-k tokens */
    GENERATION_TOP_P,            /**< Nucleus sampling */
    GENERATION_BEAM_SEARCH       /**< Beam search (greedy beams) */
} generation_strategy_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Generator configuration
 *
 * WHAT: All tuneable parameters for language generation
 * WHY:  Centralise knobs so callers can tweak without touching internals
 */
typedef struct {
    uint32_t max_sequence_length;   /**< Max tokens to generate */
    uint32_t hidden_dim;            /**< LNN hidden dimension */
    uint32_t num_lnn_neurons;       /**< Neurons per LNN layer */
    float    temperature;           /**< Sampling temperature (>0) */
    float    top_p;                 /**< Nucleus sampling threshold (0,1] */
    uint32_t top_k;                 /**< Top-k sampling */
    generation_strategy_t strategy; /**< Decoding strategy */
    uint32_t beam_width;            /**< Beam search width */
    float    repetition_penalty;    /**< Penalty for repeated tokens (>=1.0) */
    float    learning_rate;         /**< SGD learning rate for train_step */
    uint32_t eos_id;                /**< End-of-sequence token ID */
} generator_config_t;

/*=============================================================================
 * Result Structure
 *===========================================================================*/

/**
 * @brief Output of a generation call
 *
 * WHAT: Contains generated text, token IDs, and quality metrics
 * WHY:  Callers need both raw tokens and decoded text plus diagnostics
 *
 * OWNERSHIP: All heap pointers are owned by this struct.
 *            Call generation_result_cleanup() to free.
 */
typedef struct {
    char*     text;               /**< Decoded text (heap, NUL-terminated) */
    uint32_t* token_ids;          /**< Token ID array (heap) */
    uint32_t  num_tokens;         /**< Number of tokens generated */
    float*    token_confidences;  /**< Per-token confidence (heap) */
    float     overall_confidence; /**< Mean confidence across tokens */
    float     perplexity;         /**< Perplexity of generated sequence */
} generation_result_t;

/*=============================================================================
 * Statistics
 *===========================================================================*/

/**
 * @brief Cumulative generator statistics
 */
typedef struct {
    uint32_t total_generations;
    uint32_t total_tokens_generated;
    float    avg_perplexity;
    float    avg_tokens_per_generation;
} generator_stats_t;

/*=============================================================================
 * Opaque Handle
 *===========================================================================*/

/** Forward declaration of internal struct */
typedef struct language_generator language_generator_t;

/*=============================================================================
 * Configuration Helper
 *===========================================================================*/

/**
 * @brief Return a generator_config_t populated with sensible defaults
 *
 * WHAT: Factory for default config
 * WHY:  Prevents uninitialised-field bugs
 * HOW:  Fills every field with the LANGGEN_DEFAULT_* constants
 */
generator_config_t generator_default_config(void);

/*=============================================================================
 * Lifecycle
 *===========================================================================*/

/**
 * @brief Create a language generator
 *
 * WHAT: Allocate and initialise the generator, including the LNN decoder
 * WHY:  Encapsulates all generation state behind an opaque handle
 * HOW:  1. Copy config  2. Create NCP LNN  3. Alloc output projection with
 *       Xavier init  4. Alloc repetition buffer
 *
 * @param config      Generator configuration (NULL => defaults)
 * @param tokenizer   Opaque tokenizer handle (NOT owned; caller manages lifetime)
 * @param embedding   Opaque embedding handle (NOT owned; caller manages lifetime)
 * @param vocab_size  Vocabulary size (number of unique tokens)
 * @param embed_dim   Embedding vector dimension
 * @return Generator handle or NULL on failure
 */
language_generator_t* language_generator_create(
    const generator_config_t* config,
    void* tokenizer,
    void* embedding,
    uint32_t vocab_size,
    uint32_t embed_dim);

/**
 * @brief Destroy generator and release all owned resources
 *
 * WHAT: Free LNN, projection matrices, buffers
 * WHY:  Prevent memory leaks
 * HOW:  NULL-safe; destroys LNN, frees weight matrices, frees struct
 *
 * @param gen Generator handle (NULL is safe, no-op)
 */
void language_generator_destroy(language_generator_t* gen);

/*=============================================================================
 * Core Generation
 *===========================================================================*/

/**
 * @brief Generate text from a cognitive state vector
 *
 * WHAT: Autoregressive token generation driven by LNN temporal dynamics
 * WHY:  This is the primary generation entry point
 * HOW:  Project cognitive state -> seed LNN -> loop {forward, project logits,
 *       decode, embed next token} until EOS or max length
 *
 * @param gen              Generator handle
 * @param cognitive_state  Float array [state_dim] from reasoning chain
 * @param state_dim        Dimension of cognitive_state
 * @param result           Output result (caller owns, call cleanup when done)
 * @return 0 on success, -1 on error
 */
int language_generator_generate(
    language_generator_t* gen,
    const float* cognitive_state,
    uint32_t state_dim,
    generation_result_t* result);

/**
 * @brief Generate text from a text prompt
 *
 * WHAT: Encode prompt tokens, teacher-force through LNN, then generate
 * WHY:  Enables text continuation / completion
 * HOW:  tokenizer_encode -> embed each token -> LNN forward steps ->
 *       switch to autoregressive generation
 *
 * @param gen    Generator handle
 * @param prompt Input text prompt (NUL-terminated)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int language_generator_generate_from_prompt(
    language_generator_t* gen,
    const char* prompt,
    generation_result_t* result);

/**
 * @brief Generate text from a reasoning chain conclusion
 *
 * WHAT: Extract a cognitive feature vector from the chain and generate
 * WHY:  Lets the generator articulate the output of a reasoning process
 * HOW:  Builds a feature vector from chain metadata (confidence, steps,
 *       hashed conclusion), then delegates to language_generator_generate
 *
 * @param gen   Generator handle
 * @param chain Opaque reasoning_chain_t pointer
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int language_generator_generate_from_reasoning(
    language_generator_t* gen,
    const void* chain,
    generation_result_t* result);

/*=============================================================================
 * Result Cleanup
 *===========================================================================*/

/**
 * @brief Free all heap memory inside a generation_result_t
 *
 * WHAT: Release text, token_ids, token_confidences
 * WHY:  Caller owns the result struct but not the heap buffers inside it
 * HOW:  nimcp_free each pointer, zero the struct
 *
 * @param result Result to clean up (NULL-safe)
 */
void generation_result_cleanup(generation_result_t* result);

/*=============================================================================
 * Training
 *===========================================================================*/

/**
 * @brief Perform one training step (teacher-forced cross-entropy)
 *
 * WHAT: Feed input_ids through LNN, compute cross-entropy against targets,
 *       back-propagate through output projection
 * WHY:  Enables supervised fine-tuning of the generation head
 * HOW:  For each position: embed -> LNN forward -> logits -> CE loss ->
 *       gradient -> SGD update on output_projection, output_bias
 *
 * @param gen        Generator handle
 * @param input_ids  Input token IDs [seq_len]
 * @param target_ids Target token IDs [seq_len] (shifted by 1 from input)
 * @param seq_len    Sequence length
 * @param loss       Output: average cross-entropy loss over sequence
 * @return 0 on success, -1 on error
 */
int language_generator_train_step(
    language_generator_t* gen,
    const uint32_t* input_ids,
    const uint32_t* target_ids,
    uint32_t seq_len,
    float* loss);

/*=============================================================================
 * Statistics / Control
 *===========================================================================*/

/**
 * @brief Retrieve cumulative generation statistics
 */
int language_generator_get_stats(
    const language_generator_t* gen,
    generator_stats_t* stats);

/**
 * @brief Change the sampling temperature at runtime
 */
void language_generator_set_temperature(
    language_generator_t* gen,
    float temperature);

/**
 * @brief Change the decoding strategy at runtime
 */
void language_generator_set_strategy(
    language_generator_t* gen,
    generation_strategy_t strategy);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_GENERATOR_H */
