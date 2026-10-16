/**
 * @file nimcp_int8_inference.h
 * @brief Comprehensive INT8 Quantization Support for NIMCP Inference Acceleration
 *
 * WHAT: Complete INT8 quantization system for high-performance neural network inference
 * WHY:  Enable 2-4x inference speedup with minimal accuracy loss using INT8 compute
 * HOW:  Calibration-based quantization, INT8 GEMM with tensor cores, requantization
 *
 * ARCHITECTURE:
 *
 *   +------------------------------------------------------------------+
 *   |                     INT8 INFERENCE PIPELINE                      |
 *   |                                                                  |
 *   |  +-------------+    +----------------+    +------------------+   |
 *   |  | Calibrator  |--->| Quantization   |--->| INT8 Compute     |   |
 *   |  | (Histogram/ |    | Parameters     |    | (GEMM/Conv/Act)  |   |
 *   |  |  MinMax)    |    | (scale,zp)     |    |                  |   |
 *   |  +-------------+    +----------------+    +------------------+   |
 *   |                                                   |              |
 *   |                                                   v              |
 *   |  +-------------+    +----------------+    +------------------+   |
 *   |  | Model       |<---| Dequantize/    |<---| Requantize for   |   |
 *   |  | Export      |    | Output         |    | Next Layer       |   |
 *   |  | (TensorRT)  |    |                |    |                  |   |
 *   |  +-------------+    +----------------+    +------------------+   |
 *   +------------------------------------------------------------------+
 *
 * QUANTIZATION MODES:
 * - DYNAMIC: Compute scale/zero_point at runtime per batch
 * - STATIC: Pre-computed scale/zero_point from calibration
 * - QAT: Quantization-aware training with learned parameters
 *
 * CALIBRATION METHODS:
 * - MinMax: Simple min/max tracking (fast but may be suboptimal)
 * - Histogram: KL-divergence based optimal threshold selection
 * - Entropy: Information-theoretic optimal clipping
 * - Percentile: Use percentile values to reduce outlier impact
 *
 * COMPUTE OPTIMIZATIONS:
 * - INT8 tensor cores (Turing/Ampere+) for 4x theoretical speedup
 * - Fused quantize/dequantize operations
 * - Per-channel quantization for weights (better accuracy)
 * - Symmetric quantization for activations (faster compute)
 *
 * INTEGRATION POINTS:
 * - nimcp_inference_gpu.h: Base inference operations
 * - nimcp_quantization_aware.h: QAT training support
 * - nimcp_tensor_gpu.h: GPU tensor operations
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_INT8_INFERENCE_H
#define NIMCP_INT8_INFERENCE_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/inference/nimcp_inference_gpu.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//=============================================================================
// Constants
//=============================================================================

#define INT8_QUANT_MIN              (-128)      /**< INT8 minimum value */
#define INT8_QUANT_MAX              (127)       /**< INT8 maximum value */
#define UINT8_QUANT_MIN             (0)         /**< UINT8 minimum value */
#define UINT8_QUANT_MAX             (255)       /**< UINT8 maximum value */
#define INT8_CALIBRATION_DEFAULT_BINS (2048)    /**< Default histogram bins */
#define INT8_CALIBRATION_DEFAULT_SAMPLES (100)  /**< Default calibration samples */
#define INT8_SCALE_MIN              (1e-8f)     /**< Minimum scale to prevent division by zero */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Quantization mode
 */
typedef enum {
    INT8_QUANT_MODE_DYNAMIC = 0,    /**< Dynamic quantization at runtime */
    INT8_QUANT_MODE_STATIC,         /**< Pre-calibrated static quantization */
    INT8_QUANT_MODE_QAT,            /**< Quantization-aware training params */
    INT8_QUANT_MODE_COUNT
} nimcp_int8_quant_mode_t;

/**
 * @brief Calibration method for determining optimal quantization parameters
 */
typedef enum {
    INT8_CALIB_MINMAX = 0,          /**< Simple min/max tracking */
    INT8_CALIB_HISTOGRAM,           /**< Histogram-based (percentile) */
    INT8_CALIB_ENTROPY,             /**< KL-divergence/entropy calibration */
    INT8_CALIB_PERCENTILE,          /**< Percentile-based clipping */
    INT8_CALIB_MSE,                 /**< MSE-optimal thresholds */
    INT8_CALIB_COUNT
} nimcp_int8_calib_method_t;

/**
 * @brief Quantization scheme
 */
typedef enum {
    INT8_SCHEME_SYMMETRIC = 0,      /**< Symmetric: zero_point = 0, range [-max, max] */
    INT8_SCHEME_ASYMMETRIC,         /**< Asymmetric: uses zero_point, range [min, max] */
    INT8_SCHEME_COUNT
} nimcp_int8_scheme_t;

/**
 * @brief Quantization granularity
 */
typedef enum {
    INT8_GRANULARITY_TENSOR = 0,    /**< Single scale/zp for entire tensor */
    INT8_GRANULARITY_CHANNEL,       /**< Per-channel (output channel for weights) */
    INT8_GRANULARITY_GROUP,         /**< Per-group (e.g., 128 elements) */
    INT8_GRANULARITY_COUNT
} nimcp_int8_granularity_t;

//=============================================================================
// Quantization Parameters
//=============================================================================

/**
 * @brief Quantization parameters for a tensor
 *
 * For symmetric quantization:
 *   quantized = round(real / scale)
 *   real = scale * quantized
 *
 * For asymmetric quantization:
 *   quantized = round(real / scale) + zero_point
 *   real = scale * (quantized - zero_point)
 */
typedef struct nimcp_int8_quant_params {
    float scale;                    /**< Quantization scale factor */
    int32_t zero_point;             /**< Zero point offset (0 for symmetric) */
    float min_val;                  /**< Observed/calibrated minimum value */
    float max_val;                  /**< Observed/calibrated maximum value */
    bool symmetric;                 /**< True for symmetric quantization */
    nimcp_int8_granularity_t granularity; /**< Quantization granularity */

    /* Per-channel parameters (when granularity == INT8_GRANULARITY_CHANNEL) */
    int num_channels;               /**< Number of channels for per-channel */
    float* channel_scales;          /**< Per-channel scales [num_channels] */
    int32_t* channel_zero_points;   /**< Per-channel zero points [num_channels] */

    /* Per-group parameters (when granularity == INT8_GRANULARITY_GROUP) */
    int group_size;                 /**< Elements per group */
    int num_groups;                 /**< Total number of groups */
    float* group_scales;            /**< Per-group scales [num_groups] */
    int32_t* group_zero_points;     /**< Per-group zero points [num_groups] */
} nimcp_int8_quant_params_t;

//=============================================================================
// INT8 Tensor
//=============================================================================

/**
 * @brief INT8 quantized tensor with associated quantization parameters
 */
typedef struct nimcp_int8_tensor {
    int8_t* data;                   /**< INT8 data on GPU */
    nimcp_int8_quant_params_t params; /**< Quantization parameters */
    size_t* dims;                   /**< Tensor dimensions */
    size_t rank;                    /**< Number of dimensions */
    size_t numel;                   /**< Total number of elements */
    nimcp_gpu_context_t* ctx;       /**< GPU context */
    bool owns_data;                 /**< Whether tensor owns its data */
} nimcp_int8_tensor_t;

//=============================================================================
// Calibrator
//=============================================================================

/**
 * @brief Calibration data collector for determining optimal quantization parameters
 *
 * Collects statistics from FP32 activations during calibration pass
 */
typedef struct nimcp_int8_calibrator {
    /* MinMax statistics */
    float* running_min;             /**< Running minimum values [per-channel or scalar] */
    float* running_max;             /**< Running maximum values [per-channel or scalar] */

    /* Sample tracking */
    int num_samples;                /**< Number of samples collected */
    int target_samples;             /**< Target calibration samples */
    bool calibration_complete;      /**< Calibration finished flag */

    /* Configuration */
    bool per_channel;               /**< Per-channel calibration */
    int num_channels;               /**< Number of channels (if per_channel) */
    nimcp_int8_calib_method_t method; /**< Calibration method */
    nimcp_int8_scheme_t scheme;     /**< Quantization scheme */

    /* Histogram for entropy/percentile calibration */
    int* histogram;                 /**< Value histogram [num_bins] */
    int num_bins;                   /**< Number of histogram bins */
    float hist_min;                 /**< Histogram range minimum */
    float hist_max;                 /**< Histogram range maximum */
    float bin_width;                /**< Width of each histogram bin */

    /* Percentile configuration */
    float percentile;               /**< Percentile for clipping (e.g., 99.99) */

    /* GPU context */
    nimcp_gpu_context_t* ctx;       /**< GPU context */

    /* Device buffers for GPU calibration */
    float* d_running_min;           /**< Device buffer for running min */
    float* d_running_max;           /**< Device buffer for running max */
    int* d_histogram;               /**< Device buffer for histogram */
} nimcp_int8_calibrator_t;

//=============================================================================
// Quantized Model
//=============================================================================

/**
 * @brief Quantized layer information
 */
typedef struct nimcp_int8_layer {
    char name[256];                 /**< Layer name for identification */
    nimcp_int8_tensor_t* weight;    /**< Quantized weights */
    nimcp_int8_tensor_t* bias;      /**< Quantized or FP32 bias (optional) */
    nimcp_int8_quant_params_t input_params;  /**< Input activation quant params */
    nimcp_int8_quant_params_t output_params; /**< Output activation quant params */
    bool bias_is_fp32;              /**< True if bias is kept in FP32 */
} nimcp_int8_layer_t;

/**
 * @brief Fully quantized model for INT8 inference
 */
typedef struct nimcp_int8_model {
    nimcp_int8_layer_t* layers;     /**< Array of quantized layers */
    int num_layers;                 /**< Number of layers */
    nimcp_gpu_context_t* ctx;       /**< GPU context */

    /* Model metadata */
    char model_name[256];           /**< Model name */
    nimcp_int8_quant_mode_t mode;   /**< Quantization mode used */
    bool calibrated;                /**< Whether model is calibrated */

    /* Workspace for intermediate results */
    void* workspace;                /**< GPU workspace memory */
    size_t workspace_size;          /**< Workspace size in bytes */

    /* Statistics */
    uint64_t inference_count;       /**< Number of inferences run */
    float avg_latency_ms;           /**< Average inference latency */
} nimcp_int8_model_t;

//=============================================================================
// Quantization Parameters API
//=============================================================================

/**
 * @brief Initialize default quantization parameters
 *
 * @param params Parameters to initialize
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_params_init(nimcp_int8_quant_params_t* params);

/**
 * @brief Create per-channel quantization parameters
 *
 * @param num_channels Number of channels
 * @return Allocated parameters or NULL on failure
 */
NIMCP_EXPORT nimcp_int8_quant_params_t* nimcp_int8_params_create_per_channel(int num_channels);

/**
 * @brief Create per-group quantization parameters
 *
 * @param num_elements Total number of elements
 * @param group_size Elements per group
 * @return Allocated parameters or NULL on failure
 */
NIMCP_EXPORT nimcp_int8_quant_params_t* nimcp_int8_params_create_per_group(
    int num_elements,
    int group_size
);

/**
 * @brief Destroy quantization parameters and free memory
 *
 * @param params Parameters to destroy
 */
NIMCP_EXPORT void nimcp_int8_params_destroy(nimcp_int8_quant_params_t* params);

/**
 * @brief Compute quantization parameters from min/max values
 *
 * @param min_val Minimum value in data
 * @param max_val Maximum value in data
 * @param symmetric Use symmetric quantization
 * @param params Output parameters
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_compute_params_from_minmax(
    float min_val,
    float max_val,
    bool symmetric,
    nimcp_int8_quant_params_t* params
);

//=============================================================================
// INT8 Tensor API
//=============================================================================

/**
 * @brief Create an INT8 tensor with specified dimensions
 *
 * @param ctx GPU context
 * @param dims Dimension sizes
 * @param rank Number of dimensions
 * @return INT8 tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_int8_tensor_t* nimcp_int8_tensor_create(
    nimcp_gpu_context_t* ctx,
    const size_t* dims,
    size_t rank
);

/**
 * @brief Create INT8 tensor from FP32 GPU tensor with quantization
 *
 * @param fp32_tensor Source FP32 tensor
 * @param params Quantization parameters to use
 * @return Quantized INT8 tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_int8_tensor_t* nimcp_int8_tensor_from_fp32(
    const nimcp_gpu_tensor_t* fp32_tensor,
    const nimcp_int8_quant_params_t* params
);

/**
 * @brief Create FP32 GPU tensor from INT8 tensor with dequantization
 *
 * @param int8_tensor Source INT8 tensor
 * @return Dequantized FP32 tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_int8_tensor_to_fp32(
    const nimcp_int8_tensor_t* int8_tensor
);

/**
 * @brief Destroy INT8 tensor and free memory
 *
 * @param tensor Tensor to destroy
 */
NIMCP_EXPORT void nimcp_int8_tensor_destroy(nimcp_int8_tensor_t* tensor);

/**
 * @brief Clone an INT8 tensor
 *
 * @param tensor Source tensor
 * @return Cloned tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_int8_tensor_t* nimcp_int8_tensor_clone(const nimcp_int8_tensor_t* tensor);

//=============================================================================
// Calibrator API
//=============================================================================

/**
 * @brief Create a calibrator for collecting quantization statistics
 *
 * @param ctx GPU context
 * @param method Calibration method
 * @param per_channel Use per-channel calibration
 * @param num_channels Number of channels (if per_channel)
 * @param num_bins Number of histogram bins (for histogram-based methods)
 * @return Calibrator or NULL on failure
 */
NIMCP_EXPORT nimcp_int8_calibrator_t* nimcp_int8_calibrator_create(
    nimcp_gpu_context_t* ctx,
    nimcp_int8_calib_method_t method,
    bool per_channel,
    int num_channels,
    int num_bins
);

/**
 * @brief Destroy calibrator and free resources
 *
 * @param cal Calibrator to destroy
 */
NIMCP_EXPORT void nimcp_int8_calibrator_destroy(nimcp_int8_calibrator_t* cal);

/**
 * @brief Reset calibrator for new calibration run
 *
 * @param cal Calibrator to reset
 */
NIMCP_EXPORT void nimcp_int8_calibrator_reset(nimcp_int8_calibrator_t* cal);

/**
 * @brief Observe FP32 data for calibration statistics
 *
 * @param cal Calibrator
 * @param data FP32 data on GPU
 * @param numel Number of elements
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_calibrator_observe(
    nimcp_int8_calibrator_t* cal,
    const float* data,
    size_t numel
);

/**
 * @brief Observe FP32 GPU tensor for calibration
 *
 * @param cal Calibrator
 * @param tensor FP32 GPU tensor
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_calibrator_observe_tensor(
    nimcp_int8_calibrator_t* cal,
    const nimcp_gpu_tensor_t* tensor
);

/**
 * @brief Compute optimal quantization parameters from collected statistics
 *
 * Uses the configured calibration method to determine optimal scale/zero_point
 *
 * @param cal Calibrator with collected statistics
 * @param params Output quantization parameters
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_calibrator_compute_params(
    nimcp_int8_calibrator_t* cal,
    nimcp_int8_quant_params_t* params
);

/**
 * @brief Compute entropy-calibrated quantization parameters
 *
 * Uses KL-divergence to find optimal clipping threshold
 *
 * @param cal Calibrator with histogram data
 * @param params Output quantization parameters
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_calibrator_compute_entropy(
    nimcp_int8_calibrator_t* cal,
    nimcp_int8_quant_params_t* params
);

/**
 * @brief Compute percentile-based quantization parameters
 *
 * @param cal Calibrator with histogram data
 * @param percentile Percentile to use (e.g., 99.99)
 * @param params Output quantization parameters
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_calibrator_compute_percentile(
    nimcp_int8_calibrator_t* cal,
    float percentile,
    nimcp_int8_quant_params_t* params
);

//=============================================================================
// Quantization/Dequantization Operations
//=============================================================================

/**
 * @brief Quantize FP32 data to INT8
 *
 * @param ctx GPU context
 * @param input FP32 data on GPU
 * @param output INT8 data on GPU (pre-allocated)
 * @param numel Number of elements
 * @param params Quantization parameters
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_quantize(
    nimcp_gpu_context_t* ctx,
    const float* input,
    int8_t* output,
    size_t numel,
    const nimcp_int8_quant_params_t* params
);

/**
 * @brief Quantize FP32 GPU tensor to INT8 tensor
 *
 * @param ctx GPU context
 * @param input FP32 GPU tensor
 * @param output INT8 tensor (pre-allocated)
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_quantize_tensor(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_int8_tensor_t* output
);

/**
 * @brief Quantize with per-channel parameters
 *
 * @param ctx GPU context
 * @param input FP32 data [N, C, H, W] or [N, C]
 * @param output INT8 data (pre-allocated)
 * @param N Batch size
 * @param C Number of channels
 * @param HW Elements per channel (H*W for conv, 1 for FC)
 * @param params Per-channel quantization parameters
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_quantize_per_channel(
    nimcp_gpu_context_t* ctx,
    const float* input,
    int8_t* output,
    int N,
    int C,
    int HW,
    const nimcp_int8_quant_params_t* params
);

/**
 * @brief Dequantize INT8 data to FP32
 *
 * @param ctx GPU context
 * @param input INT8 data on GPU
 * @param output FP32 data on GPU (pre-allocated)
 * @param numel Number of elements
 * @param params Quantization parameters
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_dequantize(
    nimcp_gpu_context_t* ctx,
    const int8_t* input,
    float* output,
    size_t numel,
    const nimcp_int8_quant_params_t* params
);

/**
 * @brief Dequantize INT8 tensor to FP32 GPU tensor
 *
 * @param ctx GPU context
 * @param input INT8 tensor
 * @param output FP32 GPU tensor (pre-allocated)
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_dequantize_tensor(
    nimcp_gpu_context_t* ctx,
    const nimcp_int8_tensor_t* input,
    nimcp_gpu_tensor_t* output
);

/**
 * @brief Dequantize with per-channel parameters
 *
 * @param ctx GPU context
 * @param input INT8 data
 * @param output FP32 data (pre-allocated)
 * @param N Batch size
 * @param C Number of channels
 * @param HW Elements per channel
 * @param params Per-channel quantization parameters
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_dequantize_per_channel(
    nimcp_gpu_context_t* ctx,
    const int8_t* input,
    float* output,
    int N,
    int C,
    int HW,
    const nimcp_int8_quant_params_t* params
);

/**
 * @brief Fake quantization for QAT (quantize then immediately dequantize)
 *
 * Used during quantization-aware training to simulate quantization effects
 * Uses straight-through estimator (STE) for gradient computation
 *
 * @param ctx GPU context
 * @param input FP32 input data
 * @param output FP32 output data (fake-quantized)
 * @param numel Number of elements
 * @param params Quantization parameters
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_fake_quantize(
    nimcp_gpu_context_t* ctx,
    const float* input,
    float* output,
    size_t numel,
    const nimcp_int8_quant_params_t* params
);

//=============================================================================
// INT8 Compute Kernels
//=============================================================================

/**
 * @brief INT8 matrix multiplication with INT32 accumulation
 *
 * Computes C = A @ B using INT8 inputs and INT32 accumulator
 * Output can be dequantized to FP32 or requantized to INT8
 *
 * @param ctx GPU context
 * @param A INT8 matrix [M, K]
 * @param B INT8 matrix [K, N]
 * @param C INT32 output matrix [M, N]
 * @param M Rows of A
 * @param N Columns of B
 * @param K Inner dimension
 * @param params_a Quantization parameters for A
 * @param params_b Quantization parameters for B
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_gemm(
    nimcp_gpu_context_t* ctx,
    const int8_t* A,
    const int8_t* B,
    int32_t* C,
    int M,
    int N,
    int K,
    const nimcp_int8_quant_params_t* params_a,
    const nimcp_int8_quant_params_t* params_b
);

/**
 * @brief INT8 GEMM with dequantized FP32 output
 *
 * @param ctx GPU context
 * @param A INT8 matrix [M, K]
 * @param B INT8 matrix [K, N]
 * @param C FP32 output matrix [M, N]
 * @param M Rows of A
 * @param N Columns of B
 * @param K Inner dimension
 * @param params_a Quantization parameters for A
 * @param params_b Quantization parameters for B
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_gemm_fp32_output(
    nimcp_gpu_context_t* ctx,
    const int8_t* A,
    const int8_t* B,
    float* C,
    int M,
    int N,
    int K,
    const nimcp_int8_quant_params_t* params_a,
    const nimcp_int8_quant_params_t* params_b
);

/**
 * @brief INT8 GEMM with requantized INT8 output
 *
 * @param ctx GPU context
 * @param A INT8 matrix [M, K]
 * @param B INT8 matrix [K, N]
 * @param C INT8 output matrix [M, N]
 * @param M Rows of A
 * @param N Columns of B
 * @param K Inner dimension
 * @param params_a Quantization parameters for A
 * @param params_b Quantization parameters for B
 * @param params_c Quantization parameters for output C
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_gemm_requant(
    nimcp_gpu_context_t* ctx,
    const int8_t* A,
    const int8_t* B,
    int8_t* C,
    int M,
    int N,
    int K,
    const nimcp_int8_quant_params_t* params_a,
    const nimcp_int8_quant_params_t* params_b,
    const nimcp_int8_quant_params_t* params_c
);

/**
 * @brief INT8 2D convolution
 *
 * @param ctx GPU context
 * @param input INT8 input [N, C_in, H, W]
 * @param weight INT8 weights [C_out, C_in, kH, kW]
 * @param output INT32 output [N, C_out, oH, oW]
 * @param N Batch size
 * @param C_in Input channels
 * @param H Input height
 * @param W Input width
 * @param C_out Output channels
 * @param kH Kernel height
 * @param kW Kernel width
 * @param stride Convolution stride
 * @param padding Convolution padding
 * @param params_in Input quantization parameters
 * @param params_w Weight quantization parameters
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_conv2d(
    nimcp_gpu_context_t* ctx,
    const int8_t* input,
    const int8_t* weight,
    int32_t* output,
    int N,
    int C_in,
    int H,
    int W,
    int C_out,
    int kH,
    int kW,
    int stride,
    int padding,
    const nimcp_int8_quant_params_t* params_in,
    const nimcp_int8_quant_params_t* params_w
);

/**
 * @brief INT8 element-wise addition with requantization
 *
 * Computes c = a + b where a, b, c are INT8 tensors with different scales
 *
 * @param ctx GPU context
 * @param a First INT8 input
 * @param b Second INT8 input
 * @param c INT8 output
 * @param numel Number of elements
 * @param params_a Quantization parameters for a
 * @param params_b Quantization parameters for b
 * @param params_c Quantization parameters for c
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_add_requant(
    nimcp_gpu_context_t* ctx,
    const int8_t* a,
    const int8_t* b,
    int8_t* c,
    size_t numel,
    const nimcp_int8_quant_params_t* params_a,
    const nimcp_int8_quant_params_t* params_b,
    const nimcp_int8_quant_params_t* params_c
);

/**
 * @brief INT8 ReLU activation
 *
 * ReLU preserves zero_point: output = max(x, zero_point)
 *
 * @param ctx GPU context
 * @param x INT8 input
 * @param y INT8 output
 * @param numel Number of elements
 * @param zero_point Zero point value
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_relu(
    nimcp_gpu_context_t* ctx,
    const int8_t* x,
    int8_t* y,
    size_t numel,
    int32_t zero_point
);

/**
 * @brief INT8 ReLU6 activation (clamped ReLU)
 *
 * @param ctx GPU context
 * @param x INT8 input
 * @param y INT8 output
 * @param numel Number of elements
 * @param params Quantization parameters
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_relu6(
    nimcp_gpu_context_t* ctx,
    const int8_t* x,
    int8_t* y,
    size_t numel,
    const nimcp_int8_quant_params_t* params
);

/**
 * @brief INT8 fused linear + ReLU layer
 *
 * Computes y = ReLU(x @ W^T + b) entirely in INT8
 *
 * @param ctx GPU context
 * @param input INT8 input [batch, in_features]
 * @param weight INT8 weights [out_features, in_features]
 * @param bias FP32 or INT32 bias [out_features] (can be NULL)
 * @param output INT8 output [batch, out_features]
 * @param batch Batch size
 * @param in_features Input features
 * @param out_features Output features
 * @param params_in Input quantization parameters
 * @param params_w Weight quantization parameters
 * @param params_out Output quantization parameters
 * @param bias_is_fp32 True if bias is FP32
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_linear_relu(
    nimcp_gpu_context_t* ctx,
    const int8_t* input,
    const int8_t* weight,
    const void* bias,
    int8_t* output,
    int batch,
    int in_features,
    int out_features,
    const nimcp_int8_quant_params_t* params_in,
    const nimcp_int8_quant_params_t* params_w,
    const nimcp_int8_quant_params_t* params_out,
    bool bias_is_fp32
);

//=============================================================================
// Model Quantization API
//=============================================================================

/**
 * @brief Create a quantized model container
 *
 * @param ctx GPU context
 * @param num_layers Number of layers
 * @param model_name Model name for identification
 * @return Quantized model or NULL on failure
 */
NIMCP_EXPORT nimcp_int8_model_t* nimcp_int8_model_create(
    nimcp_gpu_context_t* ctx,
    int num_layers,
    const char* model_name
);

/**
 * @brief Destroy quantized model and free resources
 *
 * @param model Model to destroy
 */
NIMCP_EXPORT void nimcp_int8_model_destroy(nimcp_int8_model_t* model);

/**
 * @brief Quantize FP32 weights to INT8 and add to model
 *
 * @param model Target model
 * @param layer_idx Layer index
 * @param layer_name Layer name
 * @param weight_fp32 FP32 weights
 * @param bias_fp32 FP32 bias (can be NULL)
 * @param weight_params Pre-computed weight quantization params (NULL for auto)
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_model_add_layer(
    nimcp_int8_model_t* model,
    int layer_idx,
    const char* layer_name,
    const nimcp_gpu_tensor_t* weight_fp32,
    const nimcp_gpu_tensor_t* bias_fp32,
    const nimcp_int8_quant_params_t* weight_params
);

/**
 * @brief Run calibration on model to determine activation quantization parameters
 *
 * @param model Model to calibrate
 * @param forward_fn Forward function to run inference
 * @param forward_ctx Context for forward function
 * @param calibration_data Array of calibration input tensors
 * @param num_samples Number of calibration samples
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_model_calibrate(
    nimcp_int8_model_t* model,
    void (*forward_fn)(void* ctx, nimcp_gpu_tensor_t* input, nimcp_gpu_tensor_t* output),
    void* forward_ctx,
    nimcp_gpu_tensor_t** calibration_data,
    int num_samples
);

/**
 * @brief Set activation quantization parameters for a layer
 *
 * @param model Model
 * @param layer_idx Layer index
 * @param input_params Input activation parameters
 * @param output_params Output activation parameters
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_model_set_act_params(
    nimcp_int8_model_t* model,
    int layer_idx,
    const nimcp_int8_quant_params_t* input_params,
    const nimcp_int8_quant_params_t* output_params
);

/**
 * @brief Allocate workspace for model inference
 *
 * @param model Model
 * @param max_batch_size Maximum batch size for inference
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_model_allocate_workspace(
    nimcp_int8_model_t* model,
    int max_batch_size
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get quantization scheme name
 *
 * @param scheme Quantization scheme
 * @return Scheme name string
 */
NIMCP_EXPORT const char* nimcp_int8_scheme_name(nimcp_int8_scheme_t scheme);

/**
 * @brief Get calibration method name
 *
 * @param method Calibration method
 * @return Method name string
 */
NIMCP_EXPORT const char* nimcp_int8_calib_method_name(nimcp_int8_calib_method_t method);

/**
 * @brief Get quantization mode name
 *
 * @param mode Quantization mode
 * @return Mode name string
 */
NIMCP_EXPORT const char* nimcp_int8_mode_name(nimcp_int8_quant_mode_t mode);

/**
 * @brief Calculate memory savings from INT8 quantization
 *
 * @param fp32_size Original FP32 size in bytes
 * @return INT8 size in bytes (approximately fp32_size / 4)
 */
NIMCP_EXPORT size_t nimcp_int8_memory_savings(size_t fp32_size);

/**
 * @brief Compute quantization error (MSE) between original and quantized values
 *
 * @param ctx GPU context
 * @param original FP32 original data
 * @param int8_data INT8 quantized data
 * @param numel Number of elements
 * @param params Quantization parameters
 * @return MSE value
 */
NIMCP_EXPORT float nimcp_int8_compute_mse(
    nimcp_gpu_context_t* ctx,
    const float* original,
    const int8_t* int8_data,
    size_t numel,
    const nimcp_int8_quant_params_t* params
);

/**
 * @brief Compute signal-to-quantization-noise ratio (SQNR) in dB
 *
 * @param ctx GPU context
 * @param original FP32 original data
 * @param int8_data INT8 quantized data
 * @param numel Number of elements
 * @param params Quantization parameters
 * @return SQNR in dB
 */
NIMCP_EXPORT float nimcp_int8_compute_sqnr(
    nimcp_gpu_context_t* ctx,
    const float* original,
    const int8_t* int8_data,
    size_t numel,
    const nimcp_int8_quant_params_t* params
);

/**
 * @brief Print quantization parameters summary
 *
 * @param params Parameters to print
 * @param name Name for identification
 */
NIMCP_EXPORT void nimcp_int8_print_params(
    const nimcp_int8_quant_params_t* params,
    const char* name
);

/**
 * @brief Check if GPU supports INT8 tensor core operations
 *
 * @param ctx GPU context
 * @return true if INT8 tensor cores are available
 */
NIMCP_EXPORT bool nimcp_int8_tensor_cores_available(nimcp_gpu_context_t* ctx);

/**
 * @brief Get recommended quantization settings for GPU
 *
 * @param ctx GPU context
 * @param scheme Output: recommended scheme
 * @param granularity Output: recommended granularity
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_int8_get_recommended_settings(
    nimcp_gpu_context_t* ctx,
    nimcp_int8_scheme_t* scheme,
    nimcp_int8_granularity_t* granularity
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INT8_INFERENCE_H */
