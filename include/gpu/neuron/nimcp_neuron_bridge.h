/**
 * @file nimcp_neuron_bridge.h
 * @brief Neuron Inference Bridge — NIMCP Neural Network ↔ NeuronCore
 *
 * WHAT: Bridges NIMCP's adaptive neural network to AWS Inferentia NeuronCores
 * WHY:  Accelerate inference forward pass on Inferentia hardware (5-10x vs CPU)
 * HOW:  Extract weights → ONNX → neuron-cc → NEFF → NeuronCore execute
 *
 * ARCHITECTURE:
 *   CPU (brain_decide)                    NeuronCore
 *   ─────────────────                     ──────────
 *   brain load/init
 *     → extract weight matrices ────────→ compile to NEFF (via ONNX + neuron-cc)
 *                                              load onto NeuronCore
 *   brain_decide() called
 *     → input features ─────────────────→ [input tensor]
 *                                              NeuronCore execute
 *                                              (all layers fused)
 *     ← output vector ←────────────────── [output tensor]
 *     → 28 cognitive stages (CPU)
 *
 * KEY DESIGN DECISIONS:
 * - ONNX as intermediate format (NIMCP already has ONNX, neuron-cc accepts it)
 * - Rate-limited recompilation (NEFF takes 30-60s, amortized over N inferences)
 * - Inference-only: NeuronCores are for serving, not training
 * - BF16 internally: tolerance-based validation (< 0.01f vs CPU)
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2026
 */

#ifndef NIMCP_NEURON_BRIDGE_H
#define NIMCP_NEURON_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "common/nimcp_export.h"
#include "gpu/neuron/nimcp_neuron_context.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct neural_network_struct;
typedef struct neural_network_struct* neural_network_t;

//=============================================================================
// Configuration
//=============================================================================

/** Default recompile interval (number of inferences between weight syncs) */
#define NEURON_DEFAULT_RECOMPILE_INTERVAL 1000

/** Default NEFF cache directory */
#define NEURON_DEFAULT_CACHE_DIR "/tmp/nimcp_neff_cache"

/** Maximum number of layers supported for ONNX export */
#define NEURON_MAX_LAYERS 256

//=============================================================================
// Inference Cache
//=============================================================================

/**
 * @brief Neuron inference cache — holds compiled model and I/O buffers
 *
 * WHAT: Caches the compiled NEFF model and host-side I/O buffers
 * WHY:  NEFF compilation is expensive (30-60s); cache and reuse
 * HOW:  Store topology hash, recompile only when weights change
 */
typedef struct nimcp_neuron_inference_cache {
    // Device context (not owned — caller manages lifetime)
    nimcp_neuron_context_t* ctx;

    // Network topology
    uint32_t* layer_sizes;           /**< Layer dimensions [num_layers] */
    uint32_t num_layers;             /**< Number of layers */
    uint32_t input_size;             /**< Input dimension */
    uint32_t output_size;            /**< Output dimension */

    // NEFF state
    char neff_path[512];             /**< Path to compiled NEFF file */
    bool neff_loaded;                /**< True if NEFF is loaded on device */

    // Host I/O buffers (pre-allocated to avoid malloc on hot path)
    float* host_input;               /**< Host input buffer [input_size] */
    float* host_output;              /**< Host output buffer [output_size] */

    // Weight tracking
    bool weights_dirty;              /**< True if CPU weights changed since last compile */
    uint64_t inference_count;        /**< Inferences since last compile */
    uint32_t recompile_interval;     /**< Recompile after this many dirty inferences */

    // Cache directory
    char cache_dir[256];             /**< Directory for NEFF cache files */
} nimcp_neuron_inference_cache_t;

//=============================================================================
// Cache Lifecycle API
//=============================================================================

/**
 * @brief Create a Neuron inference cache
 *
 * @param ctx Neuron context (not owned — caller manages lifetime)
 * @param layer_sizes Array of layer dimensions
 * @param num_layers Number of layers
 * @return Cache pointer, or NULL on error
 */
NIMCP_EXPORT nimcp_neuron_inference_cache_t* nimcp_neuron_cache_create(
    nimcp_neuron_context_t* ctx,
    const uint32_t* layer_sizes,
    uint32_t num_layers);

/**
 * @brief Destroy Neuron inference cache
 *
 * @param cache Cache to destroy (NULL-safe)
 */
NIMCP_EXPORT void nimcp_neuron_cache_destroy(nimcp_neuron_inference_cache_t* cache);

//=============================================================================
// Compilation API
//=============================================================================

/**
 * @brief Export network weights to ONNX format
 *
 * WHAT: Extracts weight matrices from neural_network_t and writes ONNX
 * WHY:  ONNX is the interchange format accepted by neuron-cc compiler
 * HOW:  Iterates layers, extracts contiguous weight matrices
 *
 * @param cache Inference cache (provides topology)
 * @param net Source neural network
 * @param onnx_path Output ONNX file path
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nimcp_neuron_export_onnx(nimcp_neuron_inference_cache_t* cache,
                                           neural_network_t net,
                                           const char* onnx_path);

/**
 * @brief Compile ONNX to NEFF using neuron-cc
 *
 * WHAT: Invokes neuron-cc compiler to produce optimized NeuronCore binary
 * WHY:  NEFF is the only format NeuronCores can execute
 * HOW:  popen("neuron-cc compile ...") — separate process
 *
 * NOTE: Takes 30-60 seconds. Call at brain load time, not on hot path.
 *
 * @param cache Inference cache
 * @param net Source neural network (for weight extraction)
 * @param output_path Output NEFF file path
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nimcp_neuron_compile_neff(nimcp_neuron_inference_cache_t* cache,
                                            neural_network_t net,
                                            const char* output_path);

/**
 * @brief Load a pre-compiled NEFF model
 *
 * @param cache Inference cache
 * @param neff_path Path to existing NEFF file
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nimcp_neuron_load_neff_model(nimcp_neuron_inference_cache_t* cache,
                                               const char* neff_path);

//=============================================================================
// Inference API (Hot Path)
//=============================================================================

/**
 * @brief Execute forward pass on NeuronCore
 *
 * WHAT: Runs the dense forward pass on NeuronCore hardware
 * WHY:  This is the inference hot path — where speedup is realized
 * HOW:  Copy input → NeuronCore execute → copy output
 *
 * If weights are dirty and recompile_interval reached, triggers recompile.
 * Falls back to returning -1 if NEFF not loaded (caller should use CPU).
 *
 * @param cache Inference cache (must have NEFF loaded)
 * @param input Input features [input_size floats]
 * @param input_size Number of input elements
 * @param output Output features [output_size floats]
 * @param output_size Number of output elements
 * @return 0 on success, -1 on error (caller should fall back to CPU)
 */
NIMCP_EXPORT int nimcp_neuron_forward_pass(nimcp_neuron_inference_cache_t* cache,
                                            const float* input, uint32_t input_size,
                                            float* output, uint32_t output_size);

/**
 * @brief Execute batched forward pass on NeuronCore
 *
 * @param cache Inference cache
 * @param inputs Batched input [batch_size × input_size floats]
 * @param batch_size Number of samples in batch
 * @param input_size Input dimension per sample
 * @param outputs Batched output [batch_size × output_size floats]
 * @param output_size Output dimension per sample
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nimcp_neuron_forward_pass_batch(nimcp_neuron_inference_cache_t* cache,
                                                  const float* inputs, uint32_t batch_size,
                                                  uint32_t input_size,
                                                  float* outputs, uint32_t output_size);

//=============================================================================
// Weight Synchronization
//=============================================================================

/**
 * @brief Mark weights as dirty (trigger recompile on next interval)
 *
 * WHAT: Flags that CPU-side weights have changed
 * WHY:  After online learning, NeuronCore's compiled model is stale
 * HOW:  Sets dirty flag; actual recompile happens after recompile_interval inferences
 *
 * @param cache Inference cache
 */
NIMCP_EXPORT void nimcp_neuron_invalidate_weights(nimcp_neuron_inference_cache_t* cache);

/**
 * @brief Check if Neuron inference is ready
 *
 * @param cache Inference cache
 * @return true if NEFF is loaded and ready for inference
 */
NIMCP_EXPORT bool nimcp_neuron_is_ready(const nimcp_neuron_inference_cache_t* cache);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURON_BRIDGE_H
