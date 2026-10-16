/**
 * @file nimcp_jepa_weights.h
 * @brief JEPA Weight Loading and Management
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Load pretrained V-JEPA weights from converted format
 * WHY:  Enable transfer learning from Meta's V-JEPA 2 models
 * HOW:  Binary format converted from PyTorch via jepa_weight_converter.py
 *
 * WEIGHT CONVERSION PIPELINE:
 * ===========================
 *
 * 1. EXPORT from PyTorch:
 *    python tools/jepa_weight_converter.py \
 *        --input vjepa2_vitl.pth \
 *        --output weights/vjepa2_vitl.nimcp
 *
 * 2. LOAD in NIMCP:
 *    jepa_weights_load("weights/vjepa2_vitl.nimcp", predictor);
 *
 * BINARY FORMAT:
 * ==============
 * Header (64 bytes):
 *   - magic: 4 bytes "NJWT"
 *   - version: 4 bytes (1)
 *   - num_tensors: 4 bytes
 *   - total_params: 8 bytes
 *   - model_type: 4 bytes (enum)
 *   - latent_dim: 4 bytes
 *   - hidden_dim: 4 bytes
 *   - num_layers: 4 bytes
 *   - checksum: 4 bytes (CRC32)
 *   - reserved: 24 bytes
 *
 * Tensor entries:
 *   - name_len: 2 bytes
 *   - name: variable
 *   - ndims: 1 byte
 *   - dims: 4 bytes × ndims
 *   - dtype: 1 byte (0=f32, 1=f16, 2=bf16)
 *   - data: variable (4 × product(dims) for f32)
 *
 * SUPPORTED MODELS:
 * =================
 * - V-JEPA 2 ViT-L (300M params)
 * - V-JEPA 2 ViT-H (600M params)
 * - V-JEPA 2 ViT-G (1.1B params)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_JEPA_WEIGHTS_H
#define NIMCP_JEPA_WEIGHTS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Weight file magic number */
#define JEPA_WEIGHTS_MAGIC              0x54574A4E  /* "NJWT" */

/** @brief Current file format version */
#define JEPA_WEIGHTS_VERSION            1

/** @brief Maximum tensor name length */
#define JEPA_WEIGHTS_MAX_NAME_LEN       128

/** @brief Maximum tensor dimensions */
#define JEPA_WEIGHTS_MAX_DIMS           8

/** @brief Header size in bytes */
#define JEPA_WEIGHTS_HEADER_SIZE        64

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Supported V-JEPA model types
 */
typedef enum {
    JEPA_MODEL_CUSTOM = 0,          /**< User-defined architecture */
    JEPA_MODEL_VJEPA2_VITL,         /**< V-JEPA 2 ViT-Large (300M) */
    JEPA_MODEL_VJEPA2_VITH,         /**< V-JEPA 2 ViT-Huge (600M) */
    JEPA_MODEL_VJEPA2_VITG,         /**< V-JEPA 2 ViT-Giant (1.1B) */
    JEPA_MODEL_IJEPA_VITL,          /**< I-JEPA ViT-Large */
    JEPA_MODEL_IJEPA_VITH           /**< I-JEPA ViT-Huge */
} jepa_model_type_t;

/**
 * @brief Data types for weights
 */
typedef enum {
    JEPA_DTYPE_F32 = 0,             /**< 32-bit float */
    JEPA_DTYPE_F16,                 /**< 16-bit float */
    JEPA_DTYPE_BF16,                /**< bfloat16 */
    JEPA_DTYPE_INT8                 /**< Quantized int8 */
} jepa_weight_dtype_t;

/**
 * @brief Weight loading status
 */
typedef enum {
    JEPA_LOAD_SUCCESS = 0,          /**< All weights loaded */
    JEPA_LOAD_PARTIAL,              /**< Some weights loaded (dimension mismatch) */
    JEPA_LOAD_FAILED,               /**< Loading failed */
    JEPA_LOAD_INCOMPATIBLE          /**< Architecture mismatch */
} jepa_load_status_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Weight file header (packed for binary file compatibility)
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;                 /**< Magic number (NJWT) */
    uint32_t version;               /**< Format version */
    uint32_t num_tensors;           /**< Number of tensors */
    uint64_t total_params;          /**< Total parameter count */
    jepa_model_type_t model_type;   /**< Source model type */
    uint32_t latent_dim;            /**< Latent embedding dimension */
    uint32_t hidden_dim;            /**< Hidden layer dimension */
    uint32_t num_layers;            /**< Number of layers */
    uint32_t checksum;              /**< CRC32 checksum */
    uint8_t reserved[24];           /**< Reserved for future use */
} jepa_weights_header_t;

/**
 * @brief Single tensor descriptor
 */
typedef struct {
    char name[JEPA_WEIGHTS_MAX_NAME_LEN];   /**< Tensor name */
    uint32_t dims[JEPA_WEIGHTS_MAX_DIMS];   /**< Dimensions */
    uint32_t ndims;                          /**< Number of dimensions */
    uint64_t num_elements;                   /**< Total elements */
    jepa_weight_dtype_t dtype;               /**< Data type */
    float* data;                             /**< Weight data (f32) */
} jepa_tensor_desc_t;

/**
 * @brief Weight loading result
 */
typedef struct {
    jepa_load_status_t status;      /**< Overall status */
    uint32_t tensors_loaded;        /**< Number of tensors loaded */
    uint32_t tensors_skipped;       /**< Number skipped (size mismatch) */
    uint64_t params_loaded;         /**< Parameters loaded */
    char message[256];              /**< Status message */
} jepa_load_result_t;

/**
 * @brief Weight file handle
 */
typedef struct {
    jepa_weights_header_t header;   /**< File header */
    jepa_tensor_desc_t* tensors;    /**< Array of tensor descriptors */
    void* file_handle;              /**< Internal file handle */
    char filepath[512];             /**< Path to weight file */
    bool is_loaded;                 /**< Whether data is in memory */
} jepa_weights_t;

/* ============================================================================
 * File Loading API
 * ============================================================================ */

/**
 * @brief Open weight file and read header
 *
 * WHAT: Open file and validate header
 * WHY:  First step before loading weights
 * HOW:  Memory-map file, validate magic/version
 *
 * @param path Path to .nimcp weight file
 * @return Weight handle or NULL on failure
 */
jepa_weights_t* jepa_weights_open(const char* path);

/**
 * @brief Close weight file and free resources
 *
 * @param weights Weight handle to close (NULL safe)
 */
void jepa_weights_close(jepa_weights_t* weights);

/**
 * @brief Validate weight file
 *
 * WHAT: Check file integrity without loading
 * WHY:  Pre-flight check before loading
 *
 * @param path Path to weight file
 * @param expected_latent_dim Expected latent dimension (0 = any)
 * @return NIMCP_SUCCESS if valid
 */
int jepa_weights_validate(const char* path, uint32_t expected_latent_dim);

/**
 * @brief Get weight file info
 *
 * WHAT: Read header and return info without loading tensors
 * WHY:  Inspect weight file properties
 *
 * @param path Path to weight file
 * @param header Output header (can be NULL)
 * @return NIMCP_SUCCESS on success
 */
int jepa_weights_info(const char* path, jepa_weights_header_t* header);

/* ============================================================================
 * Weight Loading API
 * ============================================================================ */

/**
 * @brief Load weights into predictor
 *
 * WHAT: Load all compatible weights from file
 * WHY:  Main entry point for pretrained model loading
 * HOW:  Match tensor names to predictor layers, copy data
 *
 * @param path Path to weight file
 * @param predictor Predictor to load into
 * @return Load result with status and statistics
 */
jepa_load_result_t jepa_weights_load(const char* path,
                                      jepa_predictor_t* predictor);

/**
 * @brief Load specific layer weights
 *
 * WHAT: Load weights for a single layer
 * WHY:  Partial/selective weight loading
 *
 * @param weights Opened weight file
 * @param layer_name Layer name to find
 * @param output Output buffer (must be sized correctly)
 * @param expected_size Expected size in elements
 * @return NIMCP_SUCCESS on success
 */
int jepa_weights_load_tensor(jepa_weights_t* weights,
                              const char* layer_name,
                              float* output,
                              uint64_t expected_size);

/**
 * @brief Load weights with dimension adaptation
 *
 * WHAT: Load weights, adapting dimensions if needed
 * WHY:  Handle latent dimension mismatches
 * HOW:  Truncate or zero-pad as needed
 *
 * @param path Path to weight file
 * @param predictor Predictor to load into
 * @param allow_resize Allow dimension adaptation
 * @return Load result
 */
jepa_load_result_t jepa_weights_load_adaptive(const char* path,
                                               jepa_predictor_t* predictor,
                                               bool allow_resize);

/* ============================================================================
 * Weight Saving API
 * ============================================================================ */

/**
 * @brief Save predictor weights to file
 *
 * WHAT: Export trained weights to NIMCP format
 * WHY:  Save checkpoints, share models
 *
 * @param path Output path (.nimcp extension recommended)
 * @param predictor Predictor with weights to save
 * @return NIMCP_SUCCESS on success
 */
int jepa_weights_save(const char* path, const jepa_predictor_t* predictor);

/**
 * @brief Save weights with metadata
 *
 * @param path Output path
 * @param predictor Predictor to save
 * @param model_type Model type for header
 * @param extra_metadata Optional metadata string
 * @return NIMCP_SUCCESS on success
 */
int jepa_weights_save_with_meta(const char* path,
                                 const jepa_predictor_t* predictor,
                                 jepa_model_type_t model_type,
                                 const char* extra_metadata);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief List tensors in weight file
 *
 * WHAT: Print all tensor names and shapes
 * WHY:  Debugging, inspection
 *
 * @param weights Opened weight file
 */
void jepa_weights_list_tensors(const jepa_weights_t* weights);

/**
 * @brief Get tensor by name
 *
 * @param weights Opened weight file
 * @param name Tensor name
 * @return Tensor descriptor or NULL
 */
const jepa_tensor_desc_t* jepa_weights_get_tensor(const jepa_weights_t* weights,
                                                    const char* name);

/**
 * @brief Compute CRC32 checksum
 *
 * @param data Data buffer
 * @param size Size in bytes
 * @return CRC32 checksum
 */
uint32_t jepa_weights_crc32(const void* data, size_t size);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert model type to string
 *
 * @param type Model type
 * @return Human-readable string
 */
const char* jepa_model_type_to_string(jepa_model_type_t type);

/**
 * @brief Convert dtype to string
 *
 * @param dtype Weight data type
 * @return Human-readable string
 */
const char* jepa_weight_dtype_to_string(jepa_weight_dtype_t dtype);

/**
 * @brief Convert load status to string
 *
 * @param status Load status
 * @return Human-readable string
 */
const char* jepa_load_status_to_string(jepa_load_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JEPA_WEIGHTS_H */
