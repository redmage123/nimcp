/**
 * @file nimcp_inner_speech.h
 * @brief Inner speech loop — iterative self-refinement via generate/re-encode cycle
 *
 * WHAT: Brain generates text, re-encodes it, infers again, refines. Multiple
 *       passes of self-talk before producing a final answer.
 * WHY:  "Thinking before speaking" — like Vygotsky's inner speech, the brain
 *       can rehearse and refine outputs internally before committing.
 * HOW:  Loop: output -> generate text -> encode text -> blend with original ->
 *       repeat until cosine similarity converges or max iterations reached.
 */

#ifndef NIMCP_INNER_SPEECH_H
#define NIMCP_INNER_SPEECH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INNER_SPEECH_BUFFER_DIM  4096
#define INNER_SPEECH_MAX_ITER    16
#define INNER_SPEECH_MAX_TEXT    2048

typedef struct {
    uint32_t max_iterations;          /* Max refinement loops (default 3) */
    float convergence_threshold;      /* Cosine sim to stop (default 0.95) */
    float refinement_lr;              /* Blending not LR per se (default 0.0001) */
    float blend_original;             /* Weight for original output (default 0.6) */
    float blend_encoded;              /* Weight for re-encoded output (default 0.4) */
} nimcp_inner_speech_config_t;

typedef struct nimcp_inner_speech nimcp_inner_speech_t;

/* Lifecycle */
nimcp_inner_speech_t* nimcp_inner_speech_create(const nimcp_inner_speech_config_t* config);
void nimcp_inner_speech_destroy(nimcp_inner_speech_t* handle);

/**
 * @brief Iteratively refine brain output via inner speech loop
 *
 * @param handle        Inner speech handle
 * @param brain_output  Input embedding from brain forward pass
 * @param output_dim    Dimension of brain_output (max INNER_SPEECH_BUFFER_DIM)
 * @param refined_output Output buffer for refined embedding (same dim)
 * @param refined_text  Output buffer for final generated text
 * @param max_text      Size of refined_text buffer
 * @return 0 on success, -1 on failure
 */
int nimcp_inner_speech_refine(nimcp_inner_speech_t* handle,
    const float* brain_output, uint32_t output_dim,
    float* refined_output, char* refined_text, uint32_t max_text);

/**
 * @brief Get number of iterations used in last refinement call
 */
uint32_t nimcp_inner_speech_get_iterations(const nimcp_inner_speech_t* handle);

/**
 * @brief Return default configuration
 */
nimcp_inner_speech_config_t nimcp_inner_speech_config_default(void);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_INNER_SPEECH_H */
