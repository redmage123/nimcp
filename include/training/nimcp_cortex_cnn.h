/**
 * @file nimcp_cortex_cnn.h
 * @brief Per-cortex CNN processors for modality-specific feature extraction
 *
 * WHAT: Independent CNN processors for visual, audio, speech, and somatosensory cortices
 * WHY:  Replace single classifier head with per-modality convolutional architectures
 *       that learn from raw sensory data via gradient-based training
 * HOW:  Each cortex CNN wraps a cnn_trainer_t with modality-specific architecture,
 *       registers via UTM vtable for composite loss / cross-network bridges,
 *       and fuses embeddings via cross-modal attention before adaptive network
 *
 * Architecture (SNN-primary, scaled embeddings):
 *   Visual (64x64x3): Conv2d(3->16,3x3)->BN->ReLU->Pool->Conv2d(16->32)->BN->ReLU->Pool
 *                      ->Conv2d(32->64)->BN->ReLU->GlobalAvgPool->Dense(64->256) => 256-dim
 *   Audio (1x128):     Conv2d(1->16,1x5)->BN->ReLU->Pool->Conv2d(16->32,1x3)->BN->ReLU
 *                      ->GlobalAvgPool->Dense(32->256) => 256-dim
 *   Speech (1x64):     Conv2d(1->16,1x3)->BN->ReLU->Conv2d(16->32,1x3)->BN->ReLU
 *                      ->GlobalAvgPool->Dense(32->128) => 128-dim
 *   Somato (45):       Dense(45->64)->ReLU->Dense(64->128) => 128-dim
 *
 * ~180K total params, <2MB memory, CPU-first.
 */

#ifndef NIMCP_CORTEX_CNN_H
#define NIMCP_CORTEX_CNN_H

#include <stdint.h>
#include <stdbool.h>

#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct cnn_trainer_s;
struct nimcp_trainable_network_ops;
typedef struct nimcp_trainable_network_ops nimcp_trainable_network_ops_t;

/* Forward decls — keep public header light (avoid pulling router +
 * substrate headers into cortex consumers). */
struct thalamic_router;
struct neural_substrate;

/* ========================================================================= */
/* Types                                                                      */
/* ========================================================================= */

/** Cortex modality types */
typedef enum cortex_cnn_type {
    CORTEX_CNN_VISUAL  = 0,  /**< Visual cortex: processes raw pixel data */
    CORTEX_CNN_AUDIO   = 1,  /**< Audio cortex: processes mel spectrograms */
    CORTEX_CNN_SPEECH  = 2,  /**< Speech cortex: processes phoneme features */
    CORTEX_CNN_SOMATO  = 3,  /**< Somatosensory cortex: processes body state */
    CORTEX_CNN_COUNT   = 4
} cortex_cnn_type_t;

/** Per-cortex metrics for monitoring */
typedef struct cortex_cnn_metrics {
    cortex_cnn_type_t type;
    float last_loss;             /**< Loss from last backward pass */
    float ema_loss;              /**< EMA of loss (alpha=0.01) */
    uint64_t forward_steps;      /**< Total forward passes */
    uint64_t backward_steps;     /**< Total backward passes */
    float embedding_norm;        /**< L2 norm of last embedding */
    float confidence;            /**< Last forward softmax max (for attention) */
    uint32_t embedding_dim;      /**< Output embedding dimension */
    uint32_t num_params;         /**< Approximate parameter count */
} cortex_cnn_metrics_t;

/** Cortex CNN processor (opaque) */
typedef struct cortex_cnn_processor cortex_cnn_processor_t;

/* ========================================================================= */
/* Lifecycle                                                                  */
/* ========================================================================= */

/**
 * @brief Create a cortex CNN processor for a specific modality
 *
 * WHAT: Factory that builds modality-specific CNN architecture
 * WHY:  Each modality has different input shapes and optimal architectures
 * HOW:  Creates cnn_trainer_t, adds modality-specific layers via cnn_trainer_add_*
 *
 * @param type       Modality type (VISUAL, AUDIO, SPEECH, SOMATO)
 * @param embedding_dim  Output embedding dimension (0 = use default for type)
 * @return Processor handle or NULL on failure
 */
cortex_cnn_processor_t* cortex_cnn_create(cortex_cnn_type_t type, uint32_t embedding_dim);

/**
 * @brief Destroy a cortex CNN processor and free all resources
 * @param proc  Processor to destroy (NULL-safe)
 */
void cortex_cnn_destroy(cortex_cnn_processor_t* proc);

/* ========================================================================= */
/* Forward Pass (modality-specific input formats)                             */
/* ========================================================================= */

/**
 * @brief Forward pass for visual cortex (raw pixel data)
 *
 * @param proc    Visual cortex processor
 * @param pixels  Raw pixel data (uint8, HWC layout)
 * @param w       Image width
 * @param h       Image height
 * @param ch      Number of channels (1 or 3)
 * @return Embedding pointer (owned by proc, valid until next forward), NULL on error
 */
const float* cortex_cnn_forward_visual(cortex_cnn_processor_t* proc,
                                        const uint8_t* pixels,
                                        uint32_t w, uint32_t h, uint32_t ch);

/**
 * @brief Forward pass for audio cortex (mel spectrogram)
 *
 * @param proc      Audio cortex processor
 * @param mel       Mel spectrogram features (float array)
 * @param mel_size  Number of mel bins
 * @return Embedding pointer (owned by proc), NULL on error
 */
const float* cortex_cnn_forward_audio(cortex_cnn_processor_t* proc,
                                       const float* mel, uint32_t mel_size);

/**
 * @brief Forward pass for speech cortex (phoneme features)
 *
 * @param proc  Speech cortex processor
 * @param phonemes  Phoneme feature vector
 * @param size      Number of features
 * @return Embedding pointer (owned by proc), NULL on error
 */
const float* cortex_cnn_forward_speech(cortex_cnn_processor_t* proc,
                                        const float* phonemes, uint32_t size);

/**
 * @brief Forward pass for somatosensory cortex (body state)
 *
 * @param proc        Somato cortex processor
 * @param segments    Per-segment body state vector
 * @param n_segments  Number of segments
 * @return Embedding pointer (owned by proc), NULL on error
 */
const float* cortex_cnn_forward_somato(cortex_cnn_processor_t* proc,
                                        const float* segments, uint32_t n_segments);

/* ========================================================================= */
/* Backward Pass / Training                                                   */
/* ========================================================================= */

/**
 * @brief Train the cortex CNN on a labeled example
 *
 * WHAT: Run backward pass + optimizer step on the most recent forward result
 * WHY:  Each cortex CNN learns independently from its modality data
 * HOW:  Build one-hot from label, run cnn_trainer_backward + cnn_trainer_step
 *
 * @param proc        Processor with a valid last forward result
 * @param label       Label string (looked up or created in internal label map)
 * @param num_outputs Number of output classes
 * @return Loss value, or -1.0f on error
 */
float cortex_cnn_backward(cortex_cnn_processor_t* proc,
                           const char* label, uint32_t num_outputs);

/* ========================================================================= */
/* Cross-Modal Attention Fusion                                               */
/* ========================================================================= */

/**
 * @brief Fuse embeddings from multiple cortex CNNs via attention-weighted concatenation
 *
 * WHAT: Combine per-cortex embeddings into a single fused vector
 * WHY:  Multi-modal integration weighted by per-cortex confidence
 * HOW:  attention[i] = softmax(confidence[i] / temperature)
 *       fused = concat(attention[i] * embedding[i] for each active cortex)
 *
 * @param procs     Array of cortex processors (NULL entries skipped)
 * @param count     Number of entries in procs array
 * @param fused_out Output buffer for fused embedding
 * @param max_dim   Maximum output dimension (buffer size)
 * @return Actual fused dimension written, or 0 on error
 */
uint32_t cortex_cnn_fuse(cortex_cnn_processor_t* procs[], uint32_t count,
                          float* fused_out, uint32_t max_dim);

/* ========================================================================= */
/* Metrics / Introspection                                                    */
/* ========================================================================= */

/**
 * @brief Get current metrics for a cortex CNN processor
 * @param proc  Processor to query
 * @param out   Output metrics struct
 * @return 0 on success, -1 on error
 */
int cortex_cnn_get_metrics(const cortex_cnn_processor_t* proc, cortex_cnn_metrics_t* out);

/**
 * @brief Get the embedding from the last forward pass
 * @param proc  Processor to query
 * @param dim   Output: embedding dimension
 * @return Embedding pointer (owned by proc), NULL if no forward pass done
 */
const float* cortex_cnn_get_embedding(const cortex_cnn_processor_t* proc, uint32_t* dim);

/**
 * @brief Get the cortex type of a processor
 */
cortex_cnn_type_t cortex_cnn_get_type(const cortex_cnn_processor_t* proc);

/**
 * @brief Get human-readable name for cortex type
 */
const char* cortex_cnn_type_name(cortex_cnn_type_t type);

/* FNO sub-processor attachment (per-modality spectral path) */
void  cortex_cnn_set_fno_audio (cortex_cnn_processor_t* proc, void* fno);
void  cortex_cnn_set_fno_visual(cortex_cnn_processor_t* proc, void* fno);
void  cortex_cnn_set_fno_speech(cortex_cnn_processor_t* proc, void* fno);
void  cortex_cnn_set_fno_somato(cortex_cnn_processor_t* proc, void* fno);
void* cortex_cnn_get_fno_audio (const cortex_cnn_processor_t* proc);
void* cortex_cnn_get_fno_visual(const cortex_cnn_processor_t* proc);
void* cortex_cnn_get_fno_speech(const cortex_cnn_processor_t* proc);
void* cortex_cnn_get_fno_somato(const cortex_cnn_processor_t* proc);

/* ========================================================================= */
/* UTM Integration                                                            */
/* ========================================================================= */

/**
 * @brief Create a UTM trainable adapter for a cortex CNN
 *
 * WHAT: Wrap cortex CNN behind nimcp_trainable_network_ops_t vtable
 * WHY:  Register in UTM for composite loss, cross-network bridges, anti-collapse
 * HOW:  Adapter translates flat float I/O to cortex CNN's tensor-based API
 *
 * @param proc  Cortex CNN processor (must outlive adapter)
 * @param ops   Output: vtable pointer
 * @param ctx   Output: adapter context (opaque)
 * @return 0 on success, -1 on error
 */
int cortex_cnn_utm_adapter_create(cortex_cnn_processor_t* proc,
                                   const nimcp_trainable_network_ops_t** ops,
                                   void** ctx);

/* ========================================================================= */
/* Thalamic Router Adapter                                                    */
/* ========================================================================= */

/**
 * @brief Attach a thalamic router to this cortex CNN processor
 *
 * WHAT: Creates an internal thalamic_channel_t whose source_id equals the
 *       processor's cortex_cnn_type_t so the router learns per-cortex routes
 *       via Hebbian strengthening.
 * WHY:  Gives each cortex an attention gate that scales its final embedding
 *       at the end of forward; lets the thalamic router prioritize
 *       downstream targets per modality.
 * HOW:  On attach, a single destination (id=0) is pre-registered so the
 *       first forward step has a valid gate slot. Passing router=NULL
 *       detaches (destroys the existing channel, if any).
 *
 * Double-attach is supported: the previous channel is destroyed before a
 * new one is created. The router itself is borrowed — the caller retains
 * ownership of the router's lifecycle.
 *
 * @param proc    Cortex CNN processor
 * @param router  Thalamic router to attach, or NULL to detach
 * @return NIMCP_SUCCESS on success, NIMCP_ERROR_NULL_POINTER if proc is NULL
 */
nimcp_error_t cortex_cnn_attach_thalamic_router(cortex_cnn_processor_t* proc,
                                                struct thalamic_router* router);

/* ========================================================================= */
/* Thalamic Adapter Tunables (process-wide)                                   */
/* ========================================================================= */

/* Setters — accept any nonzero as on, zero as off. */
void cortex_cnn_tune_set_thalamic_enabled(float v);
void cortex_cnn_tune_set_thalamic_featuremap_gain_on(float v);
void cortex_cnn_tune_set_thalamic_burst_dropout_reduce_on(float v);

/* Getters — diagnostic. */
float cortex_cnn_tune_get_thalamic_enabled(void);
float cortex_cnn_tune_get_thalamic_featuremap_gain_on(void);
float cortex_cnn_tune_get_thalamic_burst_dropout_reduce_on(void);

/* ========================================================================= */
/* Checkpoint                                                                 */
/* ========================================================================= */

/**
 * @brief Save cortex CNN weights to file
 * @return 0 on success, -1 on error
 */
int cortex_cnn_save(const cortex_cnn_processor_t* proc, const char* path);

/**
 * @brief Load cortex CNN weights from file
 * @return 0 on success, -1 on error
 */
int cortex_cnn_load(cortex_cnn_processor_t* proc, const char* path);

/* ========================================================================= */
/* Substrate Integration (Phase 3)                                            */
/* ========================================================================= */

/**
 * @brief Attach a neural substrate to a cortex CNN processor (borrowed pointer).
 *
 * WHAT: Stores a borrowed pointer to the substrate on the processor and zeroes
 *       the cached effect structs + update counter so the next forward step
 *       recomputes fresh effects.
 * WHY:  Each cortex modality has its own metabolic compartment (region_id =
 *       cortex type: visual=0, audio=1, speech=2, somato=3). Substrate ATP,
 *       temperature, ion and membrane state modulate CNN post-activation
 *       amplitude and effective learning rate.
 * HOW:  NULL-tolerant. Passing proc=NULL or substrate=NULL are both safe.
 *       The substrate lifecycle is the caller's responsibility — the cortex
 *       does not free it.
 *
 * @param proc       Cortex CNN processor (NULL-tolerant)
 * @param substrate  Neural substrate (NULL detaches)
 */
void cortex_cnn_attach_substrate(cortex_cnn_processor_t* proc,
                                 struct neural_substrate* substrate);

/* ------------------------------------------------------------------------- */
/* Substrate tunables (process-wide, same pattern as SNN/LNN adapters)       */
/* ------------------------------------------------------------------------- */

/** Master enable. Any nonzero is treated as 1.0. Default 1.0. */
void  cortex_cnn_tune_set_substrate_enabled(float v);
float cortex_cnn_tune_get_substrate_enabled(void);

/** Recompute period in forward steps. Clamped to [1, 10000]. Out-of-range
 *  values are silently ignored. Default 10.0. */
void  cortex_cnn_tune_set_substrate_update_period(float v);
float cortex_cnn_tune_get_substrate_update_period(void);

/** Activation modulation on/off. When on, post-activation embedding is scaled
 *  by dend_effects.integration_efficiency. Default 1.0. */
void  cortex_cnn_tune_set_substrate_activation_mod_on(float v);
float cortex_cnn_tune_get_substrate_activation_mod_on(void);

/** Plasticity modulation on/off. When on, the effective optimizer LR for this
 *  cortex's step is scaled by dend_effects.plasticity_mod. Default 1.0. */
void  cortex_cnn_tune_set_substrate_plasticity_mod_on(float v);
float cortex_cnn_tune_get_substrate_plasticity_mod_on(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTEX_CNN_H */
