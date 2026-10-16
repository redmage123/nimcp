/**
 * @file nimcp_tensorrt_export.h
 * @brief TensorRT Export for NIMCP Quantized Models
 *
 * WHAT: Export NIMCP INT8 quantized models to TensorRT format
 * WHY:  Deploy optimized models using NVIDIA's inference runtime
 * HOW:  Generate TensorRT engine files with calibration data
 *
 * TENSORRT INTEGRATION:
 *
 *   +------------------------------------------------------------------+
 *   |                    TENSORRT EXPORT PIPELINE                      |
 *   |                                                                  |
 *   |  +-------------------+    +--------------------+                 |
 *   |  | NIMCP INT8 Model  |--->| Network Definition |                 |
 *   |  | (Weights + Params)|    | (TensorRT Builder) |                 |
 *   |  +-------------------+    +--------------------+                 |
 *   |                                    |                             |
 *   |                                    v                             |
 *   |  +-------------------+    +--------------------+                 |
 *   |  | Calibration Data  |--->| INT8 Calibrator    |                 |
 *   |  | (FP32 samples)    |    | (IInt8Calibrator)  |                 |
 *   |  +-------------------+    +--------------------+                 |
 *   |                                    |                             |
 *   |                                    v                             |
 *   |  +-------------------+    +--------------------+                 |
 *   |  | TensorRT Engine   |<---| Engine Build       |                 |
 *   |  | (.engine/.trt)    |    | (Optimization)     |                 |
 *   |  +-------------------+    +--------------------+                 |
 *   +------------------------------------------------------------------+
 *
 * EXPORT FORMATS:
 * - .engine/.trt: Serialized TensorRT engine (platform-specific)
 * - .onnx: ONNX with quantization info (portable)
 * - .json: Calibration cache for reproducibility
 *
 * SUPPORTED LAYERS:
 * - Linear/Dense with INT8 weights
 * - Conv2D with per-channel quantization
 * - Batch Normalization (fused with Conv)
 * - Activation functions (ReLU, GELU, SiLU)
 * - Residual connections
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_TENSORRT_EXPORT_H
#define NIMCP_TENSORRT_EXPORT_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "gpu/inference/nimcp_int8_inference.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//=============================================================================
// Constants
//=============================================================================

#define TRT_MAX_WORKSPACE_SIZE      (1ULL << 30)  /**< 1GB default workspace */
#define TRT_MAX_BATCH_SIZE          256           /**< Default max batch size */
#define TRT_CALIBRATION_CACHE_SIZE  (1024 * 1024) /**< 1MB calibration cache */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief TensorRT export format
 */
typedef enum {
    TRT_FORMAT_ENGINE = 0,      /**< Serialized TensorRT engine */
    TRT_FORMAT_ONNX,            /**< ONNX with quantization ops */
    TRT_FORMAT_CALIB_CACHE,     /**< Calibration cache only */
    TRT_FORMAT_COUNT
} nimcp_trt_format_t;

/**
 * @brief TensorRT precision mode
 */
typedef enum {
    TRT_PRECISION_FP32 = 0,     /**< Full precision */
    TRT_PRECISION_FP16,         /**< Half precision */
    TRT_PRECISION_INT8,         /**< INT8 quantized */
    TRT_PRECISION_TF32,         /**< TensorFloat-32 */
    TRT_PRECISION_COUNT
} nimcp_trt_precision_t;

/**
 * @brief TensorRT layer type for export
 */
typedef enum {
    TRT_LAYER_DENSE = 0,        /**< Fully connected layer */
    TRT_LAYER_CONV2D,           /**< 2D convolution */
    TRT_LAYER_CONV2D_BN,        /**< Conv2D + BatchNorm (fused) */
    TRT_LAYER_ACTIVATION,       /**< Activation function */
    TRT_LAYER_POOLING,          /**< Pooling layer */
    TRT_LAYER_SOFTMAX,          /**< Softmax layer */
    TRT_LAYER_RESIDUAL_ADD,     /**< Residual addition */
    TRT_LAYER_LAYERNORM,        /**< Layer normalization */
    TRT_LAYER_ATTENTION,        /**< Self-attention (as plugin) */
    TRT_LAYER_COUNT
} nimcp_trt_layer_type_t;

/**
 * @brief TensorRT activation type
 */
typedef enum {
    TRT_ACTIVATION_RELU = 0,
    TRT_ACTIVATION_SIGMOID,
    TRT_ACTIVATION_TANH,
    TRT_ACTIVATION_GELU,
    TRT_ACTIVATION_SILU,
    TRT_ACTIVATION_RELU6,
    TRT_ACTIVATION_NONE,
    TRT_ACTIVATION_COUNT
} nimcp_trt_activation_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief TensorRT export configuration
 */
typedef struct nimcp_trt_config {
    /* Basic settings */
    nimcp_trt_precision_t precision;    /**< Target precision */
    nimcp_trt_format_t format;          /**< Output format */
    int max_batch_size;                 /**< Maximum batch size */
    size_t max_workspace_size;          /**< GPU workspace memory */

    /* INT8 settings */
    bool use_strict_types;              /**< Enforce INT8 types strictly */
    bool use_dla;                       /**< Use Deep Learning Accelerator */
    int dla_core;                       /**< DLA core to use (0 or 1) */

    /* Optimization settings */
    int builder_optimization_level;     /**< 0-5, higher = more optimization */
    bool enable_sparse_weights;         /**< Enable structured sparsity */
    bool enable_timing_cache;           /**< Cache kernel timing for faster builds */

    /* Dynamic shapes */
    bool enable_dynamic_shapes;         /**< Enable dynamic batch/sequence */
    int min_batch_size;                 /**< Minimum batch for dynamic */
    int opt_batch_size;                 /**< Optimal batch for dynamic */

    /* Calibration settings */
    int num_calibration_batches;        /**< Number of calibration batches */
    const char* calibration_cache_path; /**< Path to save/load calibration cache */

    /* Output */
    const char* output_path;            /**< Output file path */
    bool verbose;                       /**< Enable verbose logging */
} nimcp_trt_config_t;

/**
 * @brief TensorRT layer definition for export
 */
typedef struct nimcp_trt_layer_def {
    char name[256];                     /**< Layer name */
    nimcp_trt_layer_type_t type;        /**< Layer type */

    /* Dimensions */
    int in_features;                    /**< Input features/channels */
    int out_features;                   /**< Output features/channels */
    int kernel_h;                       /**< Kernel height (conv) */
    int kernel_w;                       /**< Kernel width (conv) */
    int stride;                         /**< Stride (conv/pool) */
    int padding;                        /**< Padding (conv/pool) */
    int groups;                         /**< Groups for grouped conv */

    /* Activation */
    nimcp_trt_activation_t activation;  /**< Fused activation */

    /* Quantization */
    nimcp_int8_quant_params_t weight_quant;  /**< Weight quantization params */
    nimcp_int8_quant_params_t input_quant;   /**< Input quantization params */
    nimcp_int8_quant_params_t output_quant;  /**< Output quantization params */

    /* Weights (host memory, will be copied to TRT) */
    void* weights;                      /**< Weight data (FP32 or INT8) */
    void* bias;                         /**< Bias data (FP32) */
    bool weights_quantized;             /**< Weights already quantized */
} nimcp_trt_layer_def_t;

/**
 * @brief TensorRT network definition for export
 */
typedef struct nimcp_trt_network_def {
    char name[256];                     /**< Network name */

    /* Input specification */
    int num_inputs;                     /**< Number of inputs */
    char** input_names;                 /**< Input tensor names */
    int** input_dims;                   /**< Input dimensions [num_inputs][4] */
    int* input_ranks;                   /**< Rank of each input */

    /* Output specification */
    int num_outputs;                    /**< Number of outputs */
    char** output_names;                /**< Output tensor names */

    /* Layers */
    nimcp_trt_layer_def_t* layers;      /**< Layer definitions */
    int num_layers;                     /**< Number of layers */

    /* Calibration data */
    float** calibration_data;           /**< FP32 calibration inputs */
    int num_calibration_samples;        /**< Number of calibration samples */
} nimcp_trt_network_def_t;

/**
 * @brief Export result/status
 */
typedef struct nimcp_trt_export_result {
    bool success;                       /**< Export succeeded */
    char error_message[512];            /**< Error message if failed */
    size_t engine_size;                 /**< Size of exported engine */
    char output_path[512];              /**< Actual output path */

    /* Performance estimates */
    float estimated_latency_ms;         /**< Estimated inference latency */
    float estimated_throughput;         /**< Estimated throughput (samples/sec) */
    size_t estimated_memory_mb;         /**< Estimated GPU memory usage */
} nimcp_trt_export_result_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief TensorRT exporter context (opaque)
 */
typedef struct nimcp_trt_exporter_s nimcp_trt_exporter_t;

/**
 * @brief TensorRT calibrator context (opaque)
 */
typedef struct nimcp_trt_calibrator_s nimcp_trt_calibrator_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default TensorRT export configuration
 *
 * DEFAULTS:
 * - INT8 precision
 * - Engine format
 * - 256 max batch size
 * - 1GB workspace
 * - Level 3 optimization
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_trt_default_config(nimcp_trt_config_t* config);

/**
 * @brief Get FP16 export configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_trt_fp16_config(nimcp_trt_config_t* config);

/**
 * @brief Get INT8 export configuration with strict quantization
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_trt_int8_strict_config(nimcp_trt_config_t* config);

/**
 * @brief Validate export configuration
 *
 * @param config Configuration to validate
 * @return 0 if valid, negative on error with specific error code
 */
NIMCP_EXPORT int nimcp_trt_validate_config(const nimcp_trt_config_t* config);

//=============================================================================
// Network Definition API
//=============================================================================

/**
 * @brief Create a network definition for export
 *
 * @param name Network name
 * @param num_layers Number of layers
 * @return Network definition or NULL on failure
 */
NIMCP_EXPORT nimcp_trt_network_def_t* nimcp_trt_network_create(
    const char* name,
    int num_layers
);

/**
 * @brief Destroy network definition
 *
 * @param network Network to destroy
 */
NIMCP_EXPORT void nimcp_trt_network_destroy(nimcp_trt_network_def_t* network);

/**
 * @brief Add input to network definition
 *
 * @param network Network definition
 * @param name Input name
 * @param dims Input dimensions
 * @param rank Number of dimensions
 * @return Input index or negative on error
 */
NIMCP_EXPORT int nimcp_trt_network_add_input(
    nimcp_trt_network_def_t* network,
    const char* name,
    const int* dims,
    int rank
);

/**
 * @brief Mark tensor as network output
 *
 * @param network Network definition
 * @param name Output tensor name
 * @return Output index or negative on error
 */
NIMCP_EXPORT int nimcp_trt_network_add_output(
    nimcp_trt_network_def_t* network,
    const char* name
);

/**
 * @brief Add dense (fully connected) layer
 *
 * @param network Network definition
 * @param layer_idx Layer index
 * @param name Layer name
 * @param in_features Input features
 * @param out_features Output features
 * @param weights Weight data (FP32)
 * @param bias Bias data (FP32, can be NULL)
 * @param activation Fused activation
 * @param quant_params Quantization parameters (NULL for FP32 weights)
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_trt_network_add_dense(
    nimcp_trt_network_def_t* network,
    int layer_idx,
    const char* name,
    int in_features,
    int out_features,
    const float* weights,
    const float* bias,
    nimcp_trt_activation_t activation,
    const nimcp_int8_quant_params_t* quant_params
);

/**
 * @brief Add Conv2D layer
 *
 * @param network Network definition
 * @param layer_idx Layer index
 * @param name Layer name
 * @param in_channels Input channels
 * @param out_channels Output channels
 * @param kernel_h Kernel height
 * @param kernel_w Kernel width
 * @param stride Stride
 * @param padding Padding
 * @param weights Weight data [out_ch, in_ch, kH, kW]
 * @param bias Bias data (can be NULL)
 * @param activation Fused activation
 * @param quant_params Quantization parameters
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_trt_network_add_conv2d(
    nimcp_trt_network_def_t* network,
    int layer_idx,
    const char* name,
    int in_channels,
    int out_channels,
    int kernel_h,
    int kernel_w,
    int stride,
    int padding,
    const float* weights,
    const float* bias,
    nimcp_trt_activation_t activation,
    const nimcp_int8_quant_params_t* quant_params
);

/**
 * @brief Add activation layer
 *
 * @param network Network definition
 * @param layer_idx Layer index
 * @param name Layer name
 * @param activation Activation type
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_trt_network_add_activation(
    nimcp_trt_network_def_t* network,
    int layer_idx,
    const char* name,
    nimcp_trt_activation_t activation
);

/**
 * @brief Add residual connection
 *
 * @param network Network definition
 * @param layer_idx Layer index
 * @param name Layer name
 * @param input_a First input layer name
 * @param input_b Second input layer name
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_trt_network_add_residual(
    nimcp_trt_network_def_t* network,
    int layer_idx,
    const char* name,
    const char* input_a,
    const char* input_b
);

/**
 * @brief Set calibration data for INT8 export
 *
 * @param network Network definition
 * @param data Calibration input data [num_samples][input_size]
 * @param num_samples Number of calibration samples
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_trt_network_set_calibration_data(
    nimcp_trt_network_def_t* network,
    float** data,
    int num_samples
);

//=============================================================================
// Exporter API
//=============================================================================

/**
 * @brief Create TensorRT exporter
 *
 * @param ctx GPU context
 * @param config Export configuration
 * @return Exporter or NULL on failure
 */
NIMCP_EXPORT nimcp_trt_exporter_t* nimcp_trt_exporter_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_trt_config_t* config
);

/**
 * @brief Destroy exporter
 *
 * @param exporter Exporter to destroy
 */
NIMCP_EXPORT void nimcp_trt_exporter_destroy(nimcp_trt_exporter_t* exporter);

/**
 * @brief Export network definition to TensorRT engine
 *
 * @param exporter Exporter context
 * @param network Network definition
 * @param result Output: export result
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_trt_export(
    nimcp_trt_exporter_t* exporter,
    const nimcp_trt_network_def_t* network,
    nimcp_trt_export_result_t* result
);

/**
 * @brief Export NIMCP INT8 model directly to TensorRT
 *
 * Convenience function that handles conversion automatically
 *
 * @param exporter Exporter context
 * @param model NIMCP INT8 quantized model
 * @param input_names Input tensor names
 * @param input_dims Input dimensions
 * @param num_inputs Number of inputs
 * @param result Output: export result
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_trt_export_int8_model(
    nimcp_trt_exporter_t* exporter,
    const nimcp_int8_model_t* model,
    const char** input_names,
    const int** input_dims,
    int num_inputs,
    nimcp_trt_export_result_t* result
);

//=============================================================================
// Direct Export Functions
//=============================================================================

/**
 * @brief Export quantized model to TensorRT engine file
 *
 * High-level convenience function for simple export
 *
 * @param ctx GPU context
 * @param model NIMCP INT8 model
 * @param output_path Output file path
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool nimcp_export_tensorrt(
    nimcp_gpu_context_t* ctx,
    const nimcp_int8_model_t* model,
    const char* output_path
);

/**
 * @brief Export to ONNX format with quantization ops
 *
 * @param model NIMCP INT8 model
 * @param output_path Output file path
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool nimcp_export_onnx_quantized(
    const nimcp_int8_model_t* model,
    const char* output_path
);

/**
 * @brief Save calibration cache to file
 *
 * @param calibrator Calibrator with computed params
 * @param output_path Output file path
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool nimcp_trt_save_calibration_cache(
    const nimcp_int8_calibrator_t* calibrator,
    const char* output_path
);

/**
 * @brief Load calibration cache from file
 *
 * @param calibrator Calibrator to populate
 * @param input_path Input file path
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool nimcp_trt_load_calibration_cache(
    nimcp_int8_calibrator_t* calibrator,
    const char* input_path
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get TensorRT version string
 *
 * @return Version string or "TensorRT not available"
 */
NIMCP_EXPORT const char* nimcp_trt_version(void);

/**
 * @brief Check if TensorRT is available
 *
 * @return true if TensorRT is available
 */
NIMCP_EXPORT bool nimcp_trt_available(void);

/**
 * @brief Check if GPU supports INT8
 *
 * @param ctx GPU context
 * @return true if INT8 is supported
 */
NIMCP_EXPORT bool nimcp_trt_int8_supported(nimcp_gpu_context_t* ctx);

/**
 * @brief Check if DLA is available
 *
 * @param ctx GPU context
 * @return Number of DLA cores (0 if not available)
 */
NIMCP_EXPORT int nimcp_trt_dla_available(nimcp_gpu_context_t* ctx);

/**
 * @brief Get format name
 *
 * @param format Export format
 * @return Format name string
 */
NIMCP_EXPORT const char* nimcp_trt_format_name(nimcp_trt_format_t format);

/**
 * @brief Get precision name
 *
 * @param precision Precision mode
 * @return Precision name string
 */
NIMCP_EXPORT const char* nimcp_trt_precision_name(nimcp_trt_precision_t precision);

/**
 * @brief Get layer type name
 *
 * @param type Layer type
 * @return Layer type name string
 */
NIMCP_EXPORT const char* nimcp_trt_layer_type_name(nimcp_trt_layer_type_t type);

/**
 * @brief Get activation name
 *
 * @param activation Activation type
 * @return Activation name string
 */
NIMCP_EXPORT const char* nimcp_trt_activation_name(nimcp_trt_activation_t activation);

/**
 * @brief Print export result summary
 *
 * @param result Export result
 */
NIMCP_EXPORT void nimcp_trt_print_result(const nimcp_trt_export_result_t* result);

/**
 * @brief Estimate engine performance
 *
 * @param network Network definition
 * @param config Export configuration
 * @param latency_ms Output: estimated latency
 * @param throughput Output: estimated throughput
 * @param memory_mb Output: estimated memory
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_trt_estimate_performance(
    const nimcp_trt_network_def_t* network,
    const nimcp_trt_config_t* config,
    float* latency_ms,
    float* throughput,
    size_t* memory_mb
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TENSORRT_EXPORT_H */
