//=============================================================================
// nimcp_creative_onnx_runtime.h - ONNX Runtime Wrapper for Creative Models
//=============================================================================
/**
 * @file nimcp_creative_onnx_runtime.h
 * @brief ONNX Runtime integration for running external AI models
 *
 * WHAT: Wrapper around ONNX Runtime for model inference
 * WHY:  Enable use of pretrained models (SDXL, MusicGen, etc.)
 * HOW:  Load ONNX models and run inference with tensor I/O
 *
 * SUPPORTED PROVIDERS:
 * - CPU: Default, works everywhere
 * - CUDA: NVIDIA GPU acceleration
 * - TensorRT: Optimized NVIDIA inference
 * - DirectML: Windows GPU
 * - CoreML: Apple Silicon
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_CREATIVE_ONNX_RUNTIME_H
#define NIMCP_CREATIVE_ONNX_RUNTIME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct onnx_session onnx_session_t;

//=============================================================================
// Device Types
//=============================================================================

/**
 * @brief ONNX execution providers
 */
typedef enum {
    ONNX_DEVICE_CPU = 0,           /**< CPU execution */
    ONNX_DEVICE_CUDA,              /**< CUDA GPU */
    ONNX_DEVICE_TENSORRT,          /**< TensorRT optimization */
    ONNX_DEVICE_DIRECTML,          /**< DirectML (Windows) */
    ONNX_DEVICE_COREML,            /**< CoreML (Apple) */
    ONNX_DEVICE_COUNT
} onnx_device_t;

/**
 * @brief Data types for tensors
 */
typedef enum {
    ONNX_TYPE_FLOAT32 = 0,         /**< 32-bit float */
    ONNX_TYPE_FLOAT16,             /**< 16-bit float */
    ONNX_TYPE_INT32,               /**< 32-bit integer */
    ONNX_TYPE_INT64,               /**< 64-bit integer */
    ONNX_TYPE_UINT8,               /**< 8-bit unsigned */
    ONNX_TYPE_INT8,                /**< 8-bit signed */
    ONNX_TYPE_BOOL,                /**< Boolean */
    ONNX_TYPE_COUNT
} onnx_dtype_t;

//=============================================================================
// Tensor Types
//=============================================================================

/**
 * @brief Tensor shape descriptor
 */
typedef struct {
    int64_t* dims;                 /**< Dimension sizes */
    uint32_t rank;                 /**< Number of dimensions */
} onnx_shape_t;

/**
 * @brief ONNX tensor (for I/O)
 */
typedef struct {
    void* data;                    /**< Tensor data */
    onnx_shape_t shape;            /**< Shape */
    onnx_dtype_t dtype;            /**< Data type */
    size_t size_bytes;             /**< Total size in bytes */
    bool owns_data;                /**< true if tensor owns data */
} onnx_tensor_t;

/**
 * @brief Model input/output metadata
 */
typedef struct {
    char name[128];                /**< I/O name */
    onnx_shape_t shape;            /**< Shape (-1 for dynamic dims) */
    onnx_dtype_t dtype;            /**< Data type */
} onnx_io_info_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief ONNX Runtime configuration
 */
typedef struct {
    onnx_device_t device;          /**< Execution provider */
    int32_t device_id;             /**< Device ID (for multi-GPU) */

    /* Threading */
    uint32_t num_threads;          /**< Intra-op threads (0=auto) */
    uint32_t inter_op_threads;     /**< Inter-op threads (0=auto) */

    /* Memory */
    uint64_t gpu_memory_limit;     /**< Max GPU memory (0=unlimited) */
    bool enable_memory_arena;      /**< Enable memory arena */
    bool enable_memory_pattern;    /**< Enable memory pattern optimization */

    /* Optimization */
    uint8_t optimization_level;    /**< 0=off, 1=basic, 2=extended, 3=all */
    bool enable_profiling;         /**< Enable profiling */

    /* Logging */
    uint8_t log_level;             /**< 0=verbose, 1=info, 2=warning, 3=error, 4=fatal */
} onnx_runtime_config_t;

/**
 * @brief Session-specific configuration
 */
typedef struct {
    /* Override runtime config */
    bool use_runtime_defaults;     /**< Use runtime's default settings */

    /* Dynamic batching */
    bool enable_dynamic_batching;  /**< Allow variable batch size */
    uint32_t max_batch_size;       /**< Maximum batch size */

    /* Optimization */
    bool enable_graph_optimization;
    char* optimized_model_path;    /**< Save optimized model here */

    /* Custom ops */
    char** custom_op_libs;         /**< Paths to custom op libraries */
    uint32_t num_custom_op_libs;
} onnx_session_config_t;

/**
 * @brief Initialize runtime config with defaults
 */
void onnx_runtime_config_defaults(onnx_runtime_config_t* config);

/**
 * @brief Initialize session config with defaults
 */
void onnx_session_config_defaults(onnx_session_config_t* config);

//=============================================================================
// Runtime Structure
//=============================================================================

/**
 * @brief ONNX Runtime wrapper
 */
struct creative_onnx_runtime {
    onnx_runtime_config_t config;

    /* ONNX Runtime handles (opaque) */
    void* ort_env;                 /**< OrtEnv */
    void* ort_session_options;     /**< Default session options */
    void* ort_memory_info;         /**< Memory info for allocations */
    void* ort_allocator;           /**< Allocator */

    /* Loaded sessions */
    onnx_session_t** sessions;
    uint32_t num_sessions;
    uint32_t sessions_capacity;

    /* Statistics */
    uint64_t total_inferences;
    float avg_inference_time_ms;
    uint64_t peak_memory_bytes;
};

/** @brief Typedef for creative_onnx_runtime */
typedef struct creative_onnx_runtime creative_onnx_runtime_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create ONNX runtime
 *
 * @param config Configuration (NULL for defaults)
 * @return Runtime or NULL on error
 */
creative_onnx_runtime_t* onnx_runtime_create(const onnx_runtime_config_t* config);

/**
 * @brief Destroy ONNX runtime
 *
 * @param runtime Runtime to destroy
 */
void onnx_runtime_destroy(creative_onnx_runtime_t* runtime);

/**
 * @brief Check if device is available
 *
 * @param device Device to check
 * @return true if available
 */
bool onnx_device_available(onnx_device_t device);

//=============================================================================
// Session Management API
//=============================================================================

/**
 * @brief Load model from file
 *
 * @param runtime Runtime
 * @param model_path Path to .onnx file
 * @param config Session config (NULL for defaults)
 * @return Session or NULL on error
 */
onnx_session_t* onnx_load_model(creative_onnx_runtime_t* runtime,
                                 const char* model_path,
                                 const onnx_session_config_t* config);

/**
 * @brief Load model from memory
 *
 * @param runtime Runtime
 * @param model_data Model data
 * @param model_size Model size in bytes
 * @param config Session config (NULL for defaults)
 * @return Session or NULL on error
 */
onnx_session_t* onnx_load_model_from_memory(creative_onnx_runtime_t* runtime,
                                             const void* model_data,
                                             size_t model_size,
                                             const onnx_session_config_t* config);

/**
 * @brief Unload model session
 *
 * @param runtime Runtime
 * @param session Session to unload
 */
void onnx_unload_model(creative_onnx_runtime_t* runtime,
                        onnx_session_t* session);

//=============================================================================
// Model Info API
//=============================================================================

/**
 * @brief Get number of inputs
 *
 * @param session Session
 * @return Number of inputs
 */
uint32_t onnx_session_num_inputs(const onnx_session_t* session);

/**
 * @brief Get number of outputs
 *
 * @param session Session
 * @return Number of outputs
 */
uint32_t onnx_session_num_outputs(const onnx_session_t* session);

/**
 * @brief Get input info
 *
 * @param session Session
 * @param index Input index
 * @param info Output info
 * @return 0 on success, -1 on error
 */
int onnx_session_input_info(const onnx_session_t* session,
                            uint32_t index,
                            onnx_io_info_t* info);

/**
 * @brief Get output info
 *
 * @param session Session
 * @param index Output index
 * @param info Output info
 * @return 0 on success, -1 on error
 */
int onnx_session_output_info(const onnx_session_t* session,
                             uint32_t index,
                             onnx_io_info_t* info);

/**
 * @brief Get input index by name
 *
 * @param session Session
 * @param name Input name
 * @return Index or -1 if not found
 */
int32_t onnx_session_input_index(const onnx_session_t* session,
                                  const char* name);

/**
 * @brief Get output index by name
 *
 * @param session Session
 * @param name Output name
 * @return Index or -1 if not found
 */
int32_t onnx_session_output_index(const onnx_session_t* session,
                                   const char* name);

//=============================================================================
// Inference API
//=============================================================================

/**
 * @brief Run inference
 *
 * @param session Session
 * @param inputs Array of input tensors
 * @param num_inputs Number of inputs
 * @param outputs Array of output tensors (pre-allocated or NULL)
 * @param num_outputs Number of outputs
 * @return 0 on success, -1 on error
 */
int onnx_run(onnx_session_t* session,
             const onnx_tensor_t** inputs, uint32_t num_inputs,
             onnx_tensor_t** outputs, uint32_t num_outputs);

/**
 * @brief Run inference with named I/O
 *
 * @param session Session
 * @param input_names Input names
 * @param inputs Input tensors
 * @param num_inputs Number of inputs
 * @param output_names Output names
 * @param outputs Output tensors
 * @param num_outputs Number of outputs
 * @return 0 on success, -1 on error
 */
int onnx_run_named(onnx_session_t* session,
                   const char** input_names,
                   const onnx_tensor_t** inputs, uint32_t num_inputs,
                   const char** output_names,
                   onnx_tensor_t** outputs, uint32_t num_outputs);

/**
 * @brief Run inference async
 *
 * @param session Session
 * @param inputs Input tensors
 * @param num_inputs Number of inputs
 * @param callback Completion callback
 * @param user_data User data for callback
 * @return 0 on success, -1 on error
 */
typedef void (*onnx_completion_callback_t)(onnx_tensor_t** outputs,
                                            uint32_t num_outputs,
                                            int status,
                                            void* user_data);

int onnx_run_async(onnx_session_t* session,
                   const onnx_tensor_t** inputs, uint32_t num_inputs,
                   onnx_completion_callback_t callback,
                   void* user_data);

//=============================================================================
// Tensor API
//=============================================================================

/**
 * @brief Create tensor
 *
 * @param runtime Runtime
 * @param shape Shape
 * @param dtype Data type
 * @return Tensor or NULL on error
 */
onnx_tensor_t* onnx_tensor_create(creative_onnx_runtime_t* runtime,
                                   const onnx_shape_t* shape,
                                   onnx_dtype_t dtype);

/**
 * @brief Create tensor from data
 *
 * @param data Data pointer (copied)
 * @param shape Shape
 * @param dtype Data type
 * @return Tensor or NULL on error
 */
onnx_tensor_t* onnx_tensor_from_data(const void* data,
                                      const onnx_shape_t* shape,
                                      onnx_dtype_t dtype);

/**
 * @brief Create tensor wrapping external data (no copy)
 *
 * @param data Data pointer (not copied, caller must keep valid)
 * @param shape Shape
 * @param dtype Data type
 * @return Tensor or NULL on error
 */
onnx_tensor_t* onnx_tensor_wrap(void* data,
                                 const onnx_shape_t* shape,
                                 onnx_dtype_t dtype);

/**
 * @brief Destroy tensor
 *
 * @param tensor Tensor to destroy
 */
void onnx_tensor_destroy(onnx_tensor_t* tensor);

/**
 * @brief Get tensor element count
 *
 * @param tensor Tensor
 * @return Number of elements
 */
int64_t onnx_tensor_numel(const onnx_tensor_t* tensor);

/**
 * @brief Get typed pointer to tensor data
 *
 * @param tensor Tensor
 * @return Data pointer
 */
void* onnx_tensor_data(onnx_tensor_t* tensor);

/**
 * @brief Copy tensor data to buffer
 *
 * @param tensor Source tensor
 * @param dst Destination buffer
 * @param dst_size Destination buffer size
 * @return 0 on success, -1 on error
 */
int onnx_tensor_copy_to(const onnx_tensor_t* tensor,
                        void* dst, size_t dst_size);

//=============================================================================
// Shape API
//=============================================================================

/**
 * @brief Create shape
 *
 * @param dims Dimension sizes
 * @param rank Number of dimensions
 * @return Shape (caller must free with onnx_shape_free)
 */
onnx_shape_t onnx_shape_create(const int64_t* dims, uint32_t rank);

/**
 * @brief Free shape
 *
 * @param shape Shape to free
 */
void onnx_shape_free(onnx_shape_t* shape);

/**
 * @brief Calculate total elements
 *
 * @param shape Shape
 * @return Number of elements
 */
int64_t onnx_shape_numel(const onnx_shape_t* shape);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Get data type size
 *
 * @param dtype Data type
 * @return Size in bytes
 */
size_t onnx_dtype_size(onnx_dtype_t dtype);

/**
 * @brief Get device name
 *
 * @param device Device type
 * @return Device name string
 */
const char* onnx_device_name(onnx_device_t device);

/**
 * @brief Get last error message
 *
 * @return Error message or NULL
 */
const char* onnx_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CREATIVE_ONNX_RUNTIME_H */
