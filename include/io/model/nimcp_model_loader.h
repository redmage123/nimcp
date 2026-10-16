//=============================================================================
// nimcp_model_loader.h - Pre-trained Model Loading and Validation
//=============================================================================
/**
 * @file nimcp_model_loader.h
 * @brief Comprehensive pre-trained model loading, validation, and versioning
 *
 * WHAT: Full model loading implementation for NIMCP pre-trained models
 * WHY:  Enable deployment of trained models without re-training
 * HOW:  File I/O + deserialization + validation + version compatibility
 *
 * ARCHITECTURE:
 * 1. Model File Format (.nimcp):
 *    - Header: magic, version, architecture info, checksums
 *    - Metadata: training config, performance metrics, creation date
 *    - Weights: Serialized neural network parameters
 *    - Architecture: Network topology and layer configuration
 *
 * 2. Loading Pipeline:
 *    a. Open file, validate header magic
 *    b. Check version compatibility
 *    c. Verify checksum integrity
 *    d. Deserialize architecture
 *    e. Load weights into network
 *    f. Validate loaded model
 *
 * 3. Versioning Strategy:
 *    - Major version: Breaking format changes (incompatible)
 *    - Minor version: New features (backward compatible)
 *    - Patch version: Bug fixes only
 *
 * BIO-ASYNC INTEGRATION:
 * - Module ID: 0x011A (BIO_MODULE_MODEL_LOADER)
 * - Publishes: load progress, validation results, version warnings
 * - Channels: DOPAMINE (success), SEROTONIN (state changes)
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 * @version 1.0.0
 */

#ifndef NIMCP_MODEL_LOADER_H
#define NIMCP_MODEL_LOADER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Model Format Constants
//=============================================================================

/**
 * @brief Model file magic bytes: "NIMP" (NIMCP Model Package)
 */
#define NIMCP_MODEL_MAGIC_0 'N'
#define NIMCP_MODEL_MAGIC_1 'I'
#define NIMCP_MODEL_MAGIC_2 'M'
#define NIMCP_MODEL_MAGIC_3 'P'
#define NIMCP_MODEL_MAGIC_VALUE 0x504D494E  // "NIMP" as little-endian uint32

/**
 * @brief Current model format version
 *
 * VERSION HISTORY:
 * - v1.0.0: Initial format with basic weights and architecture
 * - v1.1.0: Added training metadata and performance metrics
 * - v1.2.0: Added layer freezing hints for fine-tuning
 * - v2.0.0: Restructured format with improved compression
 */
#define NIMCP_MODEL_VERSION_MAJOR 2
#define NIMCP_MODEL_VERSION_MINOR 0
#define NIMCP_MODEL_VERSION_PATCH 0

/**
 * @brief Minimum supported version for backward compatibility
 */
#define NIMCP_MODEL_MIN_MAJOR 1
#define NIMCP_MODEL_MIN_MINOR 0

/**
 * @brief Model format flags
 */
#define NIMCP_MODEL_FLAG_COMPRESSED    0x00000001  /**< Model data is LZ4 compressed */
#define NIMCP_MODEL_FLAG_ENCRYPTED     0x00000002  /**< Model is AES-256 encrypted */
#define NIMCP_MODEL_FLAG_CHECKSUM_CRC  0x00000004  /**< Has CRC32 checksum */
#define NIMCP_MODEL_FLAG_CHECKSUM_SHA  0x00000008  /**< Has SHA256 checksum */
#define NIMCP_MODEL_FLAG_QUANTIZED     0x00000010  /**< Weights are quantized */
#define NIMCP_MODEL_FLAG_SPARSE        0x00000020  /**< Sparse weight storage */
#define NIMCP_MODEL_FLAG_HAS_METADATA  0x00000040  /**< Extended metadata present */
#define NIMCP_MODEL_FLAG_FINE_TUNABLE  0x00000080  /**< Model supports fine-tuning */

/**
 * @brief Maximum sizes for security validation
 */
#define NIMCP_MODEL_MAX_NEURONS      1000000   /**< Max neurons (1M) */
#define NIMCP_MODEL_MAX_SYNAPSES     100000000 /**< Max synapses (100M) */
#define NIMCP_MODEL_MAX_LAYERS       1000      /**< Max layers */
#define NIMCP_MODEL_MAX_FILE_SIZE    (10ULL * 1024 * 1024 * 1024) /**< 10GB max */
#define NIMCP_MODEL_MAX_STRING_LEN   1024      /**< Max string length */

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief Model loader result codes
 */
typedef enum {
    NIMCP_MODEL_SUCCESS = 0,                      /**< Operation successful */

    // File errors (100-199)
    NIMCP_MODEL_ERROR_FILE_NOT_FOUND = 100,       /**< Model file not found */
    NIMCP_MODEL_ERROR_FILE_READ = 101,            /**< Error reading file */
    NIMCP_MODEL_ERROR_FILE_WRITE = 102,           /**< Error writing file */
    NIMCP_MODEL_ERROR_FILE_SIZE = 103,            /**< File size exceeds limit */
    NIMCP_MODEL_ERROR_FILE_PERMISSION = 104,      /**< Permission denied */

    // Format errors (200-299)
    NIMCP_MODEL_ERROR_INVALID_MAGIC = 200,        /**< Invalid magic bytes */
    NIMCP_MODEL_ERROR_VERSION_UNSUPPORTED = 201,  /**< Version not supported */
    NIMCP_MODEL_ERROR_VERSION_TOO_NEW = 202,      /**< Version newer than loader */
    NIMCP_MODEL_ERROR_FORMAT_CORRUPT = 203,       /**< File format corrupted */
    NIMCP_MODEL_ERROR_CHECKSUM_MISMATCH = 204,    /**< Checksum verification failed */
    NIMCP_MODEL_ERROR_INCOMPLETE = 205,           /**< File truncated/incomplete */

    // Memory errors (300-399)
    NIMCP_MODEL_ERROR_MEMORY = 300,               /**< Memory allocation failed */
    NIMCP_MODEL_ERROR_BUFFER_TOO_SMALL = 301,     /**< Output buffer too small */

    // Validation errors (400-499)
    NIMCP_MODEL_ERROR_INVALID_PARAM = 400,        /**< Invalid parameter */
    NIMCP_MODEL_ERROR_INVALID_ARCHITECTURE = 401, /**< Invalid architecture */
    NIMCP_MODEL_ERROR_INVALID_WEIGHTS = 402,      /**< Invalid weight data */
    NIMCP_MODEL_ERROR_NEURON_COUNT = 403,         /**< Neuron count exceeds limit */
    NIMCP_MODEL_ERROR_SYNAPSE_COUNT = 404,        /**< Synapse count exceeds limit */
    NIMCP_MODEL_ERROR_NAN_DETECTED = 405,         /**< NaN values in weights */
    NIMCP_MODEL_ERROR_INF_DETECTED = 406,         /**< Inf values in weights */

    // Compatibility errors (500-599)
    NIMCP_MODEL_ERROR_INCOMPATIBLE = 500,         /**< Model incompatible with runtime */
    NIMCP_MODEL_ERROR_MISSING_FEATURE = 501,      /**< Required feature not available */
    NIMCP_MODEL_ERROR_ARCH_MISMATCH = 502,        /**< Architecture mismatch */

    // Crypto errors (600-699)
    NIMCP_MODEL_ERROR_DECRYPT_FAILED = 600,       /**< Decryption failed */
    NIMCP_MODEL_ERROR_WRONG_PASSWORD = 601,       /**< Wrong password */
    NIMCP_MODEL_ERROR_CRYPTO_UNAVAILABLE = 602,   /**< Crypto library not available */

    // Compression errors (700-799)
    NIMCP_MODEL_ERROR_DECOMPRESS_FAILED = 700,    /**< Decompression failed */
    NIMCP_MODEL_ERROR_COMPRESS_FAILED = 701       /**< Compression failed */
} nimcp_model_result_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Model file header (fixed size: 64 bytes)
 *
 * Layout:
 * [0-3]   magic:         4 bytes ("NIMP")
 * [4-5]   version_major: 2 bytes
 * [6-7]   version_minor: 2 bytes
 * [8-9]   version_patch: 2 bytes
 * [10-13] flags:         4 bytes
 * [14-17] header_size:   4 bytes (for extensibility)
 * [18-21] data_offset:   4 bytes (where weight data starts)
 * [22-25] data_size:     4 bytes (compressed size if applicable)
 * [26-29] original_size: 4 bytes (uncompressed size)
 * [30-33] checksum_crc:  4 bytes (CRC32 of data section)
 * [34-65] checksum_sha:  32 bytes (SHA256, if NIMCP_MODEL_FLAG_CHECKSUM_SHA)
 */
typedef struct {
    uint8_t  magic[4];           /**< Magic bytes "NIMP" */
    uint16_t version_major;      /**< Major version */
    uint16_t version_minor;      /**< Minor version */
    uint16_t version_patch;      /**< Patch version */
    uint32_t flags;              /**< Format flags */
    uint32_t header_size;        /**< Total header size (for extensibility) */
    uint32_t data_offset;        /**< Offset to weight data */
    uint32_t data_size;          /**< Size of data section (may be compressed) */
    uint32_t original_size;      /**< Original uncompressed size */
    uint32_t checksum_crc;       /**< CRC32 checksum of data */
    uint8_t  checksum_sha[32];   /**< SHA256 checksum (optional) */
} nimcp_model_header_t;

/**
 * @brief Model architecture descriptor
 *
 * Describes the neural network architecture for validation and reconstruction.
 */
typedef struct {
    uint32_t num_neurons;        /**< Total number of neurons */
    uint32_t num_synapses;       /**< Total number of synapses */
    uint32_t num_layers;         /**< Number of layers (0 = fully connected) */
    uint32_t input_size;         /**< Input dimension */
    uint32_t output_size;        /**< Output dimension */

    // Layer configuration (if num_layers > 0)
    uint32_t* layer_sizes;       /**< Array of layer sizes [num_layers] */
    uint8_t*  layer_types;       /**< Array of layer types [num_layers] */

    // Network topology hints
    float    sparsity;           /**< Network sparsity (0.0-1.0) */
    float    ei_ratio;           /**< Excitatory/inhibitory ratio */
    uint8_t  connectivity_type;  /**< 0=dense, 1=sparse, 2=small-world */

    // Fine-tuning hints
    uint32_t sensory_layer_end;  /**< Last sensory layer index */
    uint32_t cognitive_layer_end;/**< Last cognitive layer index */
    bool     fine_tunable;       /**< Model supports fine-tuning */
} nimcp_model_architecture_t;

/**
 * @brief Model training metadata
 *
 * Information about how the model was trained.
 */
typedef struct {
    char     model_name[64];     /**< Model identifier */
    char     model_version[16];  /**< Semantic version (e.g., "v1.2.3") */
    char     training_date[32];  /**< ISO 8601 date */
    char     framework_version[16]; /**< NIMCP version used for training */

    // Training configuration
    uint32_t training_epochs;    /**< Number of training epochs */
    uint32_t training_samples;   /**< Number of training samples */
    float    final_loss;         /**< Final training loss */
    float    final_accuracy;     /**< Final training accuracy (if classification) */
    float    learning_rate;      /**< Final learning rate */

    // Performance metrics
    float    inference_time_ms;  /**< Typical inference time */
    float    memory_usage_mb;    /**< Model memory footprint */

    // Task information
    uint8_t  task_type;          /**< Task type (classification, regression, etc.) */
    char     task_description[128]; /**< Human-readable task description */
} nimcp_model_metadata_t;

/**
 * @brief Model weight storage format
 *
 * Defines how weights are stored in the model file.
 */
typedef enum {
    NIMCP_WEIGHT_FORMAT_FLOAT32 = 0,  /**< Standard 32-bit float */
    NIMCP_WEIGHT_FORMAT_FLOAT16 = 1,  /**< 16-bit half precision */
    NIMCP_WEIGHT_FORMAT_BFLOAT16 = 2, /**< Brain float 16 */
    NIMCP_WEIGHT_FORMAT_INT8 = 3,     /**< 8-bit quantized */
    NIMCP_WEIGHT_FORMAT_INT4 = 4,     /**< 4-bit quantized */
    NIMCP_WEIGHT_FORMAT_SPARSE = 5    /**< Sparse COO format */
} nimcp_weight_format_t;

/**
 * @brief Model loading options
 */
typedef struct {
    bool     validate_checksum;  /**< Verify checksum on load (default: true) */
    bool     validate_weights;   /**< Check weights for NaN/Inf (default: true) */
    bool     allow_newer_version;/**< Allow loading newer format versions */
    bool     verbose;            /**< Print loading progress */

    // Memory options
    bool     lazy_load;          /**< Defer weight loading until first use */
    bool     memory_map;         /**< Use memory-mapped I/O */
    size_t   read_buffer_size;   /**< File read buffer size (0 = default 64KB) */

    // Decryption
    const char* password;        /**< Decryption password (NULL if unencrypted) */
    size_t      password_len;    /**< Password length */

    // Architecture override
    bool     ignore_arch_mismatch; /**< Ignore minor architecture differences */
} nimcp_model_load_options_t;

/**
 * @brief Model saving options
 */
typedef struct {
    bool     compress;           /**< LZ4 compress the model (default: true) */
    uint8_t  compression_level;  /**< Compression level 1-12 (default: 6) */
    bool     include_checksum;   /**< Include CRC32 checksum (default: true) */
    bool     include_sha256;     /**< Include SHA256 checksum (default: false) */
    bool     include_metadata;   /**< Include training metadata */

    // Encryption
    const char* password;        /**< Encryption password (NULL = no encryption) */
    size_t      password_len;    /**< Password length */

    // Quantization
    nimcp_weight_format_t weight_format; /**< Weight storage format */
    bool     prune_weights;      /**< Prune small weights to increase sparsity */
    float    prune_threshold;    /**< Weights below this are pruned */

    // Fine-tuning support
    bool     mark_fine_tunable;  /**< Mark model as supporting fine-tuning */
    uint32_t sensory_layer_end;  /**< For fine-tuning layer freezing */
    uint32_t cognitive_layer_end;/**< For fine-tuning layer freezing */
} nimcp_model_save_options_t;

/**
 * @brief Loaded model handle
 *
 * Opaque handle returned by nimcp_model_load().
 */
typedef struct nimcp_loaded_model nimcp_loaded_model_t;

/**
 * @brief Model validation result
 */
typedef struct {
    nimcp_model_result_t result; /**< Overall validation result */
    bool     header_valid;       /**< Header validation passed */
    bool     checksum_valid;     /**< Checksum verification passed */
    bool     architecture_valid; /**< Architecture validation passed */
    bool     weights_valid;      /**< Weight validation passed */
    bool     version_compatible; /**< Version is compatible */

    // Statistics from validation
    uint32_t nan_count;          /**< Number of NaN values found */
    uint32_t inf_count;          /**< Number of Inf values found */
    float    weight_min;         /**< Minimum weight value */
    float    weight_max;         /**< Maximum weight value */
    float    weight_mean;        /**< Mean weight value */
    float    weight_std;         /**< Weight standard deviation */

    // Version info
    uint16_t file_version_major; /**< File format major version */
    uint16_t file_version_minor; /**< File format minor version */
    uint16_t file_version_patch; /**< File format patch version */

    // Error details
    char     error_message[256]; /**< Human-readable error description */
} nimcp_model_validation_t;

//=============================================================================
// Core API Functions
//=============================================================================

/**
 * @brief Get default model loading options
 *
 * @return Default options with sensible values
 */
NIMCP_EXPORT nimcp_model_load_options_t nimcp_model_load_options_default(void);

/**
 * @brief Get default model saving options
 *
 * @return Default options with sensible values
 */
NIMCP_EXPORT nimcp_model_save_options_t nimcp_model_save_options_default(void);

/**
 * @brief Validate model file without loading
 *
 * WHAT: Validates model file format, checksum, and compatibility
 * WHY:  Quick validation before committing to full load
 * HOW:  Read header, verify checksum, check version compatibility
 *
 * @param filepath Path to model file
 * @param validation Output validation result
 * @return NIMCP_MODEL_SUCCESS on success, error code otherwise
 */
NIMCP_EXPORT nimcp_model_result_t nimcp_model_validate_file(
    const char* filepath,
    nimcp_model_validation_t* validation
);

/**
 * @brief Load model from file
 *
 * WHAT: Full model loading with deserialization and validation
 * WHY:  Primary entry point for using pre-trained models
 * HOW:  Open file -> validate -> decompress -> deserialize -> validate weights
 *
 * @param filepath Path to model file
 * @param options Loading options (NULL for defaults)
 * @param model_out Output: loaded model handle
 * @return NIMCP_MODEL_SUCCESS on success, error code otherwise
 */
NIMCP_EXPORT nimcp_model_result_t nimcp_model_load(
    const char* filepath,
    const nimcp_model_load_options_t* options,
    nimcp_loaded_model_t** model_out
);

/**
 * @brief Load model from memory buffer
 *
 * @param data Model data buffer
 * @param size Buffer size
 * @param options Loading options (NULL for defaults)
 * @param model_out Output: loaded model handle
 * @return NIMCP_MODEL_SUCCESS on success, error code otherwise
 */
NIMCP_EXPORT nimcp_model_result_t nimcp_model_load_from_buffer(
    const uint8_t* data,
    size_t size,
    const nimcp_model_load_options_t* options,
    nimcp_loaded_model_t** model_out
);

/**
 * @brief Save model to file
 *
 * @param model Model to save
 * @param filepath Output file path
 * @param options Saving options (NULL for defaults)
 * @return NIMCP_MODEL_SUCCESS on success, error code otherwise
 */
NIMCP_EXPORT nimcp_model_result_t nimcp_model_save(
    const nimcp_loaded_model_t* model,
    const char* filepath,
    const nimcp_model_save_options_t* options
);

/**
 * @brief Free loaded model
 *
 * @param model Model handle to free
 */
NIMCP_EXPORT void nimcp_model_free(nimcp_loaded_model_t* model);

//=============================================================================
// Model Inspection API
//=============================================================================

/**
 * @brief Get model header information
 *
 * @param model Loaded model
 * @param header_out Output header (copied)
 * @return NIMCP_MODEL_SUCCESS on success
 */
NIMCP_EXPORT nimcp_model_result_t nimcp_model_get_header(
    const nimcp_loaded_model_t* model,
    nimcp_model_header_t* header_out
);

/**
 * @brief Get model architecture information
 *
 * @param model Loaded model
 * @param arch_out Output architecture (caller must free layer_sizes/layer_types)
 * @return NIMCP_MODEL_SUCCESS on success
 */
NIMCP_EXPORT nimcp_model_result_t nimcp_model_get_architecture(
    const nimcp_loaded_model_t* model,
    nimcp_model_architecture_t* arch_out
);

/**
 * @brief Get model metadata
 *
 * @param model Loaded model
 * @param meta_out Output metadata
 * @return NIMCP_MODEL_SUCCESS on success
 */
NIMCP_EXPORT nimcp_model_result_t nimcp_model_get_metadata(
    const nimcp_loaded_model_t* model,
    nimcp_model_metadata_t* meta_out
);

/**
 * @brief Get model weight data pointer
 *
 * WHAT: Get direct access to weight array for integration with brain
 * WHY:  Efficient weight transfer without extra copies
 * HOW:  Returns pointer to internal weight buffer
 *
 * @param model Loaded model
 * @param weights_out Output: pointer to weights (do not free)
 * @param num_weights_out Output: number of weight values
 * @param format_out Output: weight format
 * @return NIMCP_MODEL_SUCCESS on success
 */
NIMCP_EXPORT nimcp_model_result_t nimcp_model_get_weights(
    const nimcp_loaded_model_t* model,
    const float** weights_out,
    size_t* num_weights_out,
    nimcp_weight_format_t* format_out
);

//=============================================================================
// Version Compatibility API
//=============================================================================

/**
 * @brief Check if a model version is compatible with current loader
 *
 * @param major Major version to check
 * @param minor Minor version to check
 * @return true if compatible
 */
NIMCP_EXPORT bool nimcp_model_version_compatible(uint16_t major, uint16_t minor);

/**
 * @brief Get version string for loaded model
 *
 * @param model Loaded model
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return NIMCP_MODEL_SUCCESS on success
 */
NIMCP_EXPORT nimcp_model_result_t nimcp_model_get_version_string(
    const nimcp_loaded_model_t* model,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Get current loader version string
 *
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
NIMCP_EXPORT void nimcp_model_loader_version(char* buffer, size_t buffer_size);

//=============================================================================
// Error Handling API
//=============================================================================

/**
 * @brief Get human-readable error message for result code
 *
 * @param result Result code
 * @return Static error message string
 */
NIMCP_EXPORT const char* nimcp_model_strerror(nimcp_model_result_t result);

/**
 * @brief Get detailed error message from last operation
 *
 * Thread-local error message with more detail than strerror.
 *
 * @return Error message or empty string if no error
 */
NIMCP_EXPORT const char* nimcp_model_get_last_error(void);

/**
 * @brief Clear last error message
 */
NIMCP_EXPORT void nimcp_model_clear_error(void);

//=============================================================================
// Integration with brain_t API
//=============================================================================

/**
 * @brief Create brain from loaded model
 *
 * WHAT: Construct brain_t from nimcp_loaded_model_t
 * WHY:  Bridge between model loader and brain API
 * HOW:  Create brain, copy weights, set configuration
 *
 * @param model Loaded model
 * @param task Task type for brain
 * @return Brain handle or NULL on error
 */
struct brain_struct;  // Forward declaration
NIMCP_EXPORT struct brain_struct* nimcp_model_create_brain(
    const nimcp_loaded_model_t* model,
    uint8_t task
);

/**
 * @brief Save brain as model file
 *
 * WHAT: Export brain_t to model file format
 * WHY:  Enable saving trained brains in portable format
 * HOW:  Extract weights, serialize architecture, write to file
 *
 * @param brain Brain to save
 * @param filepath Output file path
 * @param options Saving options (NULL for defaults)
 * @param metadata Training metadata to include (NULL for defaults)
 * @return NIMCP_MODEL_SUCCESS on success
 */
NIMCP_EXPORT nimcp_model_result_t nimcp_brain_save_as_model(
    const struct brain_struct* brain,
    const char* filepath,
    const nimcp_model_save_options_t* options,
    const nimcp_model_metadata_t* metadata
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MODEL_LOADER_H
