/**
 * @file nimcp_common.h
 * @brief NIMCP common definitions and utilities
 *
 * Provides common definitions, types, macros, and utility functions used
 * throughout the NIMCP protocol implementation. Includes error handling,
 * logging, memory management, thread synchronization, and network utilities.
 *
 * @version 1.0.0
 * @date 2024
 */

#ifndef NIMCP_COMMON_H
#define NIMCP_COMMON_H


#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
    #include <stdatomic.h>
#endif
#include <math.h>
#include <string.h>
#include <time.h>

#ifdef __linux__
    #include <dirent.h>
    #include <dlfcn.h>
    #include <lz4.h>
#elif defined(_WIN32)
    #include <direct.h>
    #include <lz4.h>
    #include <windows.h>
#endif

typedef struct timespec nimcp_timespec_t;

/* Message Priority Levels */
typedef enum NimcpMessagePriority {
    NIMCP_PRIORITY_LOW = 0,
    NIMCP_PRIORITY_NORMAL = 1,
    NIMCP_PRIORITY_HIGH = 2,
    NIMCP_PRIORITY_URGENT = 3
} NimcpMessagePriority;

/* Result type - defined in nimcp_error_codes.h as nimcp_error_t */
#ifndef NIMCP_RESULT_T_DEFINED
#define NIMCP_RESULT_T_DEFINED
typedef int32_t nimcp_result_t;
#endif

/* Message type definition */
typedef struct nimcp_message {
    void* data;     /* Message payload */
    size_t size;    /* Size of payload */
    uint32_t type;  /* Message type */
    uint32_t flags; /* Message flags */
} nimcp_message_t;


/* ============================================================================
 * Error Code System
 * ============================================================================
 *
 * NIMCP uses a unified error code system with POSITIVE integers:
 *   - NIMCP_SUCCESS = 0
 *   - Info status codes: 1-999 (NIMCP_INFO_* prefix, non-error conditions)
 *   - Error codes: 1000+ (NIMCP_ERROR_* prefix, organized by category)
 *
 * IMPORTANT: Info codes vs Error codes
 *   NIMCP_INFO_NOT_FOUND (4)   = Non-error: item simply not present
 *   NIMCP_ERROR_NOT_FOUND (1009) = Error: required resource missing
 *
 *   NIMCP_INFO_TIMEOUT (3)     = Non-error: poll/try timed out, can retry
 *   NIMCP_ERROR_TIMEOUT (1010)  = Error: operation failed due to timeout
 *
 * See nimcp_error_codes.h for the canonical type and error code definitions.
 * This file provides protocol-specific codes and compatibility aliases.
 * ============================================================================ */

#define MAX_ROUTES 256

/**
 * @defgroup error_codes Error Codes
 * @{
 */

/* Success Code */
#ifndef NIMCP_SUCCESS
#define NIMCP_SUCCESS 0                     /**< Operation completed successfully */
#endif

/* Success with Information Codes (1-999)
 * These are status codes that indicate non-error conditions.
 * They use the INFO_ prefix to distinguish from ERROR_ codes.
 * Note: Some legacy code may use names without INFO_ prefix.
 */
#ifndef NIMCP_INFO_PENDING
#define NIMCP_INFO_PENDING 1        /**< Operation is in progress */
#endif
#ifndef NIMCP_PENDING
#define NIMCP_PENDING NIMCP_INFO_PENDING    /**< Alias for compatibility */
#endif

#ifndef NIMCP_INFO_WOULD_BLOCK
#define NIMCP_INFO_WOULD_BLOCK 2    /**< Non-blocking operation would block */
#endif
#ifndef NIMCP_WOULD_BLOCK
#define NIMCP_WOULD_BLOCK NIMCP_INFO_WOULD_BLOCK    /**< Alias for compatibility */
#endif

#ifndef NIMCP_INFO_TIMEOUT
#define NIMCP_INFO_TIMEOUT 3        /**< Operation timed out (informational, not error) */
#endif
#ifndef NIMCP_TIMEOUT
#define NIMCP_TIMEOUT NIMCP_INFO_TIMEOUT    /**< Alias for compatibility */
#endif
/* Note: NIMCP_INFO_TIMEOUT=3 is informational; NIMCP_ERROR_TIMEOUT=1010 is an error */

#ifndef NIMCP_INFO_NOT_FOUND
#define NIMCP_INFO_NOT_FOUND 4      /**< Item not found (informational, not error) */
#endif
#ifndef NIMCP_NOT_FOUND
#define NIMCP_NOT_FOUND NIMCP_INFO_NOT_FOUND  /**< Alias for compatibility */
#endif
/* Note: NIMCP_INFO_NOT_FOUND=4 is informational; NIMCP_ERROR_NOT_FOUND=1009 is an error */

#ifndef NIMCP_INFO_BUFFER_FULL
#define NIMCP_INFO_BUFFER_FULL 5    /**< Buffer capacity reached (informational) */
#endif
#ifndef NIMCP_BUFFER_FULL
#define NIMCP_BUFFER_FULL NIMCP_INFO_BUFFER_FULL    /**< Alias for compatibility */
#endif

#ifndef NIMCP_INFO_END_OF_STREAM
#define NIMCP_INFO_END_OF_STREAM 6  /**< End of stream reached */
#endif
#ifndef NIMCP_END_OF_STREAM
#define NIMCP_END_OF_STREAM NIMCP_INFO_END_OF_STREAM    /**< Alias for compatibility */
#endif

#ifndef NIMCP_INFO_ALREADY_EXISTS
#define NIMCP_INFO_ALREADY_EXISTS 7 /**< Item already exists (informational) */
#endif
#ifndef NIMCP_ALREADY_EXISTS
#define NIMCP_ALREADY_EXISTS NIMCP_INFO_ALREADY_EXISTS  /**< Alias for compatibility */
#endif

/* ============================================================================
 * Generic Errors (1000-1999)
 * ============================================================================ */

#ifndef NIMCP_ERROR
#define NIMCP_ERROR 1000                    /**< Generic error */
#endif
#ifndef NIMCP_ERROR_NOT_IMPLEMENTED
#define NIMCP_ERROR_NOT_IMPLEMENTED 1001    /**< Feature not implemented */
#endif
#ifndef NIMCP_ERROR_INVALID_PARAM
#define NIMCP_ERROR_INVALID_PARAM 1002      /**< Invalid parameter */
#endif
#ifndef NIMCP_INVALID_PARAM
#define NIMCP_INVALID_PARAM 1002            /**< Invalid parameter (alias) */
#endif
#ifndef NIMCP_ERROR_NULL_POINTER
#define NIMCP_ERROR_NULL_POINTER 1003       /**< Null pointer error */
#endif
#ifndef NIMCP_ERROR_NULL_ARG
#define NIMCP_ERROR_NULL_ARG 1003           /**< Null argument (alias for NULL_POINTER) */
#endif
#ifndef NIMCP_ERROR_INVALID
#define NIMCP_ERROR_INVALID 1004            /**< Invalid argument value */
#endif
#ifndef NIMCP_ERROR_NOT_FOUND
#define NIMCP_ERROR_NOT_FOUND 1009          /**< Resource not found (error) */
#endif
#ifndef NIMCP_ERROR_TIMEOUT
#define NIMCP_ERROR_TIMEOUT 1010            /**< Operation timed out (error) */
#endif
#ifndef NIMCP_INVALID_STATE
#define NIMCP_INVALID_STATE 1005            /**< Invalid state for operation */
#endif
#ifndef NIMCP_NOT_INITIALIZED
#define NIMCP_NOT_INITIALIZED 1007          /**< Component not initialized */
#endif
#ifndef NIMCP_NOT_IMPLEMENTED
#define NIMCP_NOT_IMPLEMENTED 1001          /**< Feature not implemented (alias) */
#endif
#ifndef NIMCP_OPERATION_CANCELED
#define NIMCP_OPERATION_CANCELED 1011       /**< Operation was canceled */
#endif
#ifndef NIMCP_PERMISSION_DENIED
#define NIMCP_PERMISSION_DENIED 1012        /**< Permission denied */
#endif
#ifndef NIMCP_ERROR_NOT_SUPPORTED
#define NIMCP_ERROR_NOT_SUPPORTED 1013      /**< Feature not supported */
#endif

/* ============================================================================
 * Memory and Buffer Errors (2000-2999)
 * ============================================================================ */

#ifndef NIMCP_ERROR_MEMORY
#define NIMCP_ERROR_MEMORY 2000             /**< Memory allocation error */
#endif
#ifndef NIMCP_NO_MEMORY
#define NIMCP_NO_MEMORY 2000                /**< Memory allocation failed (alias) */
#endif
#ifndef NIMCP_ERROR_BUFFER_TOO_SMALL
#define NIMCP_ERROR_BUFFER_TOO_SMALL 2001   /**< Buffer too small for operation */
#endif
#ifndef NIMCP_BUFFER_TOO_SMALL
#define NIMCP_BUFFER_TOO_SMALL 2001         /**< Buffer too small (alias) */
#endif
#ifndef NIMCP_ERROR_BUFFER_OVERFLOW
#define NIMCP_ERROR_BUFFER_OVERFLOW 2002    /**< Buffer overflow detected */
#endif
#ifndef NIMCP_ERROR_BUFFER_UNDERFLOW
#define NIMCP_ERROR_BUFFER_UNDERFLOW 2003   /**< Buffer underflow error */
#endif

/* ============================================================================
 * I/O and Serialization Errors (4000-4999)
 * ============================================================================ */

#ifndef NIMCP_IO_ERROR
#define NIMCP_IO_ERROR 4000                 /**< I/O error */
#endif
#ifndef NIMCP_ERROR_SERIALIZATION
#define NIMCP_ERROR_SERIALIZATION 4006      /**< Serialization failed */
#endif
#ifndef NIMCP_ERROR_SERIALIZER
#define NIMCP_ERROR_SERIALIZER 4006         /**< Serializer error (alias) */
#endif
#ifndef NIMCP_ERROR_DESERIALIZATION
#define NIMCP_ERROR_DESERIALIZATION 4007    /**< Deserialization failed */
#endif
#ifndef NIMCP_NETWORK_ERROR
#define NIMCP_NETWORK_ERROR 4008            /**< Network operation failed */
#endif
#ifndef NIMCP_SOCKET_ERROR
#define NIMCP_SOCKET_ERROR 4009             /**< Socket operation failed */
#endif

/* ============================================================================
 * Configuration Errors (5000-5999)
 * ============================================================================ */

#ifndef NIMCP_CONFIG_ERROR
#define NIMCP_CONFIG_ERROR 5000             /**< Configuration error */
#endif
#ifndef NIMCP_INIT_FAILED
#define NIMCP_INIT_FAILED 5001              /**< Initialization failed */
#endif
#ifndef NIMCP_ALREADY_RUNNING
#define NIMCP_ALREADY_RUNNING 5002          /**< Already running */
#endif
#ifndef NIMCP_NOT_RUNNING
#define NIMCP_NOT_RUNNING 5003              /**< Not running */
#endif

/* ============================================================================
 * Threading/Concurrency Errors (6000-6999)
 * ============================================================================ */

#ifndef NIMCP_THREAD_ERROR
#define NIMCP_THREAD_ERROR 6000             /**< Thread operation failed */
#endif
#ifndef NIMCP_LOCK_ERROR
#define NIMCP_LOCK_ERROR 6002               /**< Lock operation failed */
#endif
#ifndef NIMCP_TIMEOUT_ERROR
#define NIMCP_TIMEOUT_ERROR 6010            /**< Timeout error (threading context) */
#endif
#ifndef NIMCP_SYSTEM_ERROR
#define NIMCP_SYSTEM_ERROR 6011             /**< System call failed */
#endif

/* ============================================================================
 * Protocol-Specific Errors (1100-1199)
 * ============================================================================ */

#ifndef NIMCP_ERROR_INVALID_PACKET
#define NIMCP_ERROR_INVALID_PACKET 1100     /**< Invalid packet structure */
#endif
#ifndef NIMCP_ERROR_INVALID_HEADER
#define NIMCP_ERROR_INVALID_HEADER 1101     /**< Invalid packet header */
#endif
#ifndef NIMCP_ERROR_INVALID_MAGIC
#define NIMCP_ERROR_INVALID_MAGIC 1102      /**< Invalid magic number */
#endif
#ifndef NIMCP_ERROR_INVALID_VERSION
#define NIMCP_ERROR_INVALID_VERSION 1103    /**< Unsupported protocol version */
#endif
#ifndef NIMCP_ERROR_VERSION_MISMATCH
#define NIMCP_ERROR_VERSION_MISMATCH 1103   /**< Protocol version mismatch (alias) */
#endif
#ifndef NIMCP_VERSION_MISMATCH
#define NIMCP_VERSION_MISMATCH 1103         /**< Version mismatch (alias) */
#endif
#ifndef NIMCP_ERROR_INVALID_TYPE
#define NIMCP_ERROR_INVALID_TYPE 1104       /**< Invalid message type */
#endif
#ifndef NIMCP_ERROR_INVALID_FLAGS
#define NIMCP_ERROR_INVALID_FLAGS 1105      /**< Invalid packet flags */
#endif
#ifndef NIMCP_ERROR_INVALID_SEQUENCE
#define NIMCP_ERROR_INVALID_SEQUENCE 1106   /**< Invalid sequence number */
#endif
#ifndef NIMCP_INVALID_SEQUENCE
#define NIMCP_INVALID_SEQUENCE 1106         /**< Invalid sequence (alias) */
#endif
#ifndef NIMCP_ERROR_INVALID_SIZE
#define NIMCP_ERROR_INVALID_SIZE 1107       /**< Invalid payload size */
#endif
#ifndef NIMCP_ERROR_PAYLOAD_TOO_LARGE
#define NIMCP_ERROR_PAYLOAD_TOO_LARGE 1108  /**< Payload exceeds maximum size */
#endif
#ifndef NIMCP_ERROR_INVALID_MESSAGE
#define NIMCP_ERROR_INVALID_MESSAGE 1109    /**< Invalid message format */
#endif
#ifndef NIMCP_INVALID_MSG
#define NIMCP_INVALID_MSG 1109              /**< Invalid message (alias) */
#endif
#ifndef NIMCP_ERROR_MESSAGE_TOO_LARGE
#define NIMCP_ERROR_MESSAGE_TOO_LARGE 1108  /**< Message too large (alias) */
#endif
#ifndef NIMCP_INVALID_CHECKSUM
#define NIMCP_INVALID_CHECKSUM 1110         /**< Invalid message checksum */
#endif
#ifndef NIMCP_PROTO_ERROR
#define NIMCP_PROTO_ERROR 1111              /**< Protocol error */
#endif
#ifndef NIMCP_HANDSHAKE_FAILED
#define NIMCP_HANDSHAKE_FAILED 1112         /**< Protocol handshake failed */
#endif
#ifndef NIMCP_ERROR_QUEUE_FULL
#define NIMCP_ERROR_QUEUE_FULL 1113         /**< Message queue is full */
#endif
#ifndef NIMCP_QUEUE_FULL
#define NIMCP_QUEUE_FULL 1113               /**< Queue full (alias) */
#endif
#ifndef NIMCP_QUEUE_EMPTY
#define NIMCP_QUEUE_EMPTY 1114              /**< Message queue is empty */
#endif

/* ============================================================================
 * Security/Crypto Errors (1200-1299)
 * ============================================================================ */

#ifndef NIMCP_AUTH_FAILED
#define NIMCP_AUTH_FAILED 1200              /**< Authentication failed */
#endif
#ifndef NIMCP_CRYPTO_ERROR
#define NIMCP_CRYPTO_ERROR 1201             /**< Cryptographic operation failed */
#endif
#ifndef NIMCP_ERROR_INVALID_SIGNATURE
#define NIMCP_ERROR_INVALID_SIGNATURE 1202  /**< Invalid or corrupt signature */
#endif
#ifndef NIMCP_ERROR_SECURITY_REQUIRED
#define NIMCP_ERROR_SECURITY_REQUIRED 1203  /**< Security context required */
#endif
#ifndef NIMCP_ERROR_ENCRYPTION_FAILED
#define NIMCP_ERROR_ENCRYPTION_FAILED 1204  /**< Encryption operation failed */
#endif
#ifndef NIMCP_ERROR_DECRYPTION_FAILED
#define NIMCP_ERROR_DECRYPTION_FAILED 1205  /**< Decryption operation failed */
#endif
#ifndef NIMCP_ERROR_SIGNATURE_FAILED
#define NIMCP_ERROR_SIGNATURE_FAILED 1206   /**< Signature generation failed */
#endif
#ifndef NIMCP_ERROR_VERIFICATION_FAILED
#define NIMCP_ERROR_VERIFICATION_FAILED 1207 /**< Signature verification failed */
#endif

/** @} */
/* Version Information - Use nimcp.h version if already defined */
#ifndef NIMCP_VERSION_MAJOR
#define NIMCP_VERSION_MAJOR 2
#endif
#ifndef NIMCP_VERSION_MINOR
#define NIMCP_VERSION_MINOR 6
#endif
#ifndef NIMCP_VERSION_PATCH
#define NIMCP_VERSION_PATCH 3
#endif
#ifndef NIMCP_VERSION_STRING
#define NIMCP_VERSION_STRING "2.6.3"
#endif

/* P2-10: WARNING - MIN/MAX macros evaluate arguments twice.
 * Do NOT pass expressions with side effects (e.g., MIN(i++, j++)).
 * For side-effect-safe versions, use NIMCP_MIN/NIMCP_MAX below. */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* P2-10: Type-safe MIN/MAX using GCC statement expressions (no double-eval).
 * Falls back to standard macros on non-GCC compilers. */
#ifdef __GNUC__
#define NIMCP_MIN(a, b) \
    __extension__ ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a < _b ? _a : _b; })
#define NIMCP_MAX(a, b) \
    __extension__ ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a > _b ? _a : _b; })
#else
#define NIMCP_MIN(a, b) MIN(a, b)
#define NIMCP_MAX(a, b) MAX(a, b)
#endif

/* P2-12: NIMCP_ALIGN requires 'a' to be a power of two.
 * Behavior is undefined if a == 0 or a is not a power of 2. */
#define NIMCP_ALIGN(x, a) (((x) + ((a) -1)) & ~((a) -1))

/* Platform Detection - Use nimcp_platform.h if already defined */
#ifndef NIMCP_PLATFORM_LINUX
#if defined(_WIN32) || defined(_WIN64)
    #define NIMCP_PLATFORM_WINDOWS
#elif defined(__linux__)
    #define NIMCP_PLATFORM_LINUX
#elif defined(__APPLE__)
    #define NIMCP_PLATFORM_MACOS
#else
    #error "Unsupported platform"
#endif
#endif


/* Message size constants */
#ifndef MAX_MESSAGE_SIZE
#define MAX_MESSAGE_SIZE (1024 * 1024)  /* 1MB */
#endif
#ifndef MAX_BATCH_SIZE
#define MAX_BATCH_SIZE 1000
#endif
#ifndef MAX_PENDING_MESSAGES
#define MAX_PENDING_MESSAGES 10000
#endif
#ifndef MESSAGE_QUEUE_TIMEOUT_MS
#define MESSAGE_QUEUE_TIMEOUT_MS 1000
#endif

/* Common Type Definitions */
typedef uint32_t nimcp_handle_t;
#define NIMCP_INVALID_HANDLE ((nimcp_handle_t) 0)

/* Time-related Definitions */
#define NIMCP_TIMEOUT_INFINITE ((uint32_t) -1)
#define NIMCP_TIMEOUT_IMMEDIATE ((uint32_t) 0)

/* Size-related Definitions */
#define NIMCP_MAX_ERROR_LENGTH 128
#define NIMCP_DEFAULT_BUFFER_SIZE 4096


/* Common Utility Functions */
/**
 * @brief Convert result code to string description
 * @param result Result code
 * @return String description
 */
const char* nimcp_result_to_string(nimcp_result_t result);

/**
 * @brief Get version string
 * @return Version string
 */
const char* nimcp_version_string(void);

/**
 * @brief Sleep for specified milliseconds
 * @param milliseconds Sleep duration
 */
void nimcp_sleep(uint32_t milliseconds);

/* Error Handling */
/**
 * @brief Error information structure
 * Conditionally defined to avoid conflict with nimcp_error_codes.h
 */
#ifndef NIMCP_ERROR_INFO_T_DEFINED
#define NIMCP_ERROR_INFO_T_DEFINED
typedef struct nimcp_error_info_struct {
    nimcp_result_t code;
    char message[NIMCP_MAX_ERROR_LENGTH];
} nimcp_error_info_t;
#endif

/**
 * @brief Get current error information
 * @param info Error info structure to fill
 */
void nimcp_get_error_info(nimcp_error_info_t* info);

/**
 * @brief Set error information
 * @param code Error code
 * @param format Error message format
 * @param ... Format arguments
 */
void nimcp_set_error_info(nimcp_result_t code, const char* format, ...);

/**
 * @brief Clear current error state
 */
void nimcp_clear_error(void);

/* Basic Logging Levels */
typedef enum {
    NIMCP_LOG_NONE = 0,
    NIMCP_LOG_ERROR,
    NIMCP_LOG_WARN,
    NIMCP_LOG_INFO,
    NIMCP_LOG_DEBUG,
    NIMCP_LOG_TRACE
} nimcp_log_level_t;

/* Logging Function Type */
typedef void (*nimcp_log_callback_t)(nimcp_log_level_t level, const char* file, int line,
                                     const char* func, const char* message);

/* Logging Configuration */
/**
 * @brief Set logging level
 * @param level Log level
 */
void nimcp_set_log_level(nimcp_log_level_t level);

/**
 * @brief Set logging callback
 * @param callback Log callback function
 */
void nimcp_set_log_callback(nimcp_log_callback_t callback);


/* Endian Conversion Utilities */
/**
 * @brief Convert host to network byte order (16-bit)
 */
uint16_t nimcp_htons(uint16_t value);

/**
 * @brief Convert host to network byte order (32-bit)
 */
uint32_t nimcp_htonl(uint32_t value);

/**
 * @brief Convert host to network byte order (64-bit)
 */
uint64_t nimcp_htonll(uint64_t value);

/**
 * @brief Convert network to host byte order (16-bit)
 */
uint16_t nimcp_ntohs(uint16_t value);

/**
 * @brief Convert network to host byte order (32-bit)
 */
uint32_t nimcp_ntohl(uint32_t value);

/**
 * @brief Convert network to host byte order (64-bit)
 */
uint64_t nimcp_ntohll(uint64_t value);

/* ============================================================================
 * State Validation Types and Constants
 * ============================================================================ */

/* Field type enumeration */
typedef enum {
    NIMCP_FIELD_INTEGER = 0,
    NIMCP_FIELD_FLOAT = 1,
    NIMCP_FIELD_STRING = 2,
    NIMCP_FIELD_ARRAY = 3
} NimcpFieldType;

/* Array element type enumeration */
typedef enum {
    NIMCP_ARRAY_INTEGER = 0,
    NIMCP_ARRAY_FLOAT = 1,
    NIMCP_ARRAY_STRING = 2
} NimcpArrayElementType;

/* State field descriptor */
typedef struct {
    NimcpFieldType type; /* Field type */
    uint32_t offset;     /* Offset from start of state data */
    uint32_t size;       /* Size of field in bytes */
} NimcpStateField;

/* State header */
typedef struct {
    uint32_t magic;       /* Magic number for validation */
    uint32_t field_count; /* Number of fields */
} NimcpStateHeader;

/* Array field header */
typedef struct {
    NimcpArrayElementType element_type; /* Type of array elements */
    uint32_t element_size;              /* Size of each element */
    uint32_t element_count;             /* Number of elements */
    uint32_t _padding;                  /* Padding to ensure 8-byte alignment of array data */
} NimcpArrayHeader;

/* State validation constants */
#define NIMCP_STATE_MAGIC 0x4E494D43 /* 'NIMC' */
#define NIMCP_MAX_FIELDS 1024
#define NIMCP_INT32_MIN (-2147483647 - 1)
#define NIMCP_INT32_MAX 2147483647
#define NIMCP_INT64_MIN (-9223372036854775807LL - 1)
#define NIMCP_INT64_MAX 9223372036854775807LL
#define NIMCP_FLOAT_MAX 3.40282347e+38F
#define NIMCP_DOUBLE_MAX 1.7976931348623157e+308
#define NIMCP_STRING_MAX_LENGTH 65536
#define NIMCP_ARRAY_MAX_ELEMENTS 100000
#define NIMCP_ARRAY_ALIGNMENT 8

/* ============================================================================
 * Common Mathematical & Algorithm Constants
 * ============================================================================ */

/* Linear Congruential Generator (LCG) constants - POSIX standard */
#define NIMCP_LCG_MULTIPLIER    1103515245U
#define NIMCP_LCG_INCREMENT     12345U
#define NIMCP_LCG_MODULUS       0x80000000U  /* 2^31 */

/* Exponential Moving Average (EMA) weights */
#define NIMCP_EMA_WEIGHT_SLOW       0.9f    /* High smoothing */
#define NIMCP_EMA_WEIGHT_FAST       0.1f    /* Low smoothing (1 - slow) */
#define NIMCP_EMA_WEIGHT_MEDIUM     0.95f   /* Very high smoothing */
#define NIMCP_EMA_WEIGHT_MEDIUM_NEW 0.05f   /* Very low new value weight */

/* Adaptive rate control */
#define NIMCP_RATE_INCREASE_FACTOR  1.1f    /* Default rate increase multiplier */
#define NIMCP_RATE_DECREASE_FACTOR  0.9f    /* Default rate decrease multiplier */

/* ============================================================================
 * Bio-Async Module Inbox Capacities
 * ============================================================================ */

#define NIMCP_INBOX_CAPACITY_SMALL      32   /* Low-traffic modules */
#define NIMCP_INBOX_CAPACITY_MEDIUM     64   /* Standard modules */
#define NIMCP_INBOX_CAPACITY_LARGE      128  /* High-traffic modules */
#define NIMCP_INBOX_CAPACITY_XLARGE     256  /* Critical/hub modules */
#define NIMCP_INBOX_CAPACITY_DEFAULT    NIMCP_INBOX_CAPACITY_MEDIUM

/* ============================================================================
 * Middleware Buffer Capacities
 * ============================================================================ */

#define NIMCP_BUFFER_CAPACITY_DEFAULT   1024 /* Default buffer/queue capacity */
#define NIMCP_HISTORY_WINDOW_SIZE       100  /* Default history tracking window */
#define NIMCP_EVENT_BATCH_SIZE          32   /* Events to process per batch */
#define NIMCP_LOSS_HISTORY_SIZE         32   /* Training loss history buffer */
#define NIMCP_FE_HISTORY_SIZE           100  /* Free energy history for cortical training */

/* Feature extractor limits */
#define NIMCP_FEATURE_BUFFER_CAPACITY   1024 /* Rate signal buffer size */
#define NIMCP_ATTENTION_SHIFTS_HISTORY  100  /* Attention shift tracking window */
#define NIMCP_TRACKED_WEIGHTS_CAPACITY  64   /* Training weight tracking capacity */

/* Signal pool parameters */
#define NIMCP_SIGNAL_POOL_BLOCK_SIZE    128  /* Fits up to 32 floats per signal */
#define NIMCP_SIGNAL_POOL_NUM_BLOCKS    1024 /* Signals in flight */

/* Sequence detector parameters */
#define NIMCP_PE_EMBEDDING_DIM          64   /* Positional encoding embedding dimension */

/* Latency histogram size */
#define NIMCP_LATENCY_HISTOGRAM_SIZE    32   /* Flow tracker latency bins */

/* ============================================================================
 * Time Conversion Constants
 * ============================================================================ */

#ifndef NIMCP_MS_PER_SEC
#define NIMCP_MS_PER_SEC        1000U
#endif
#ifndef NIMCP_US_PER_MS
#define NIMCP_US_PER_MS         1000U
#endif
#ifndef NIMCP_NS_PER_US
#define NIMCP_NS_PER_US         1000U
#endif
#ifndef NIMCP_NS_PER_MS
#define NIMCP_NS_PER_MS         1000000U
#endif
#ifndef NIMCP_NS_PER_SEC
#define NIMCP_NS_PER_SEC        1000000000ULL
#endif
#ifndef NIMCP_US_PER_SEC
#define NIMCP_US_PER_SEC        1000000U
#endif

/* Common timeout values (milliseconds) */
#define NIMCP_TIMEOUT_SHORT_MS      100U
#define NIMCP_TIMEOUT_MEDIUM_MS     500U
#define NIMCP_TIMEOUT_LONG_MS       1000U
#define NIMCP_TIMEOUT_VERY_LONG_MS  5000U

/* ============================================================================
 * Biological Simulation Constants
 * ============================================================================ */

/* Neuron/synapse defaults */
#define NIMCP_DEFAULT_SYNAPSE_STRENGTH      0.5f
#define NIMCP_DEFAULT_LEARNING_RATE         0.01f
#define NIMCP_DEFAULT_DECAY_RATE            0.001f
#define NIMCP_SYNAPSE_STRENGTH_MIN          0.0f
#define NIMCP_SYNAPSE_STRENGTH_MAX          1.0f

/* Plasticity constants */
#define NIMCP_PLASTICITY_RATE_DEFAULT       0.1f
#define NIMCP_PLASTICITY_THRESHOLD_LOW      0.3f
#define NIMCP_PLASTICITY_THRESHOLD_HIGH     0.7f

/* Energy/metabolic defaults */
#define NIMCP_ATP_LEVEL_FULL                1.0f
#define NIMCP_ATP_LEVEL_HEALTHY             0.7f
#define NIMCP_ATP_LEVEL_IMPAIRED            0.5f
#define NIMCP_ATP_LEVEL_SUPPRESSED          0.3f
#define NIMCP_ATP_LEVEL_CRITICAL            0.1f

/* ============================================================================
 * Memory Pool Constants
 * ============================================================================ */

#define NIMCP_POOL_BLOCK_SIZE_TINY          64
#define NIMCP_POOL_BLOCK_SIZE_SMALL         256
#define NIMCP_POOL_BLOCK_SIZE_MEDIUM        1024
#define NIMCP_POOL_BLOCK_SIZE_LARGE         4096
#define NIMCP_POOL_BLOCK_SIZE_HUGE          16384

#define NIMCP_POOL_INITIAL_BLOCKS           16
#define NIMCP_POOL_GROWTH_FACTOR            2
#define NIMCP_POOL_MAX_BLOCKS               65536

/* ============================================================================
 * Thread Pool Constants
 * ============================================================================ */

#define NIMCP_THREAD_POOL_MIN_THREADS       1
#define NIMCP_THREAD_POOL_MAX_THREADS       256
#define NIMCP_THREAD_POOL_DEFAULT_THREADS   4
#define NIMCP_THREAD_POOL_QUEUE_SIZE        1024

/* ============================================================================
 * Cognitive/Mental Health Thresholds
 * ============================================================================ */

#define NIMCP_STRESS_LEVEL_LOW              0.2f
#define NIMCP_STRESS_LEVEL_MODERATE         0.5f
#define NIMCP_STRESS_LEVEL_HIGH             0.7f
#define NIMCP_STRESS_LEVEL_CRITICAL         0.9f

#define NIMCP_COGNITIVE_LOAD_LOW            0.3f
#define NIMCP_COGNITIVE_LOAD_OPTIMAL        0.6f
#define NIMCP_COGNITIVE_LOAD_HIGH           0.8f
#define NIMCP_COGNITIVE_LOAD_OVERLOAD       0.95f

/* ============================================================================
 * Network/Protocol Constants
 * ============================================================================ */

#define NIMCP_MAX_CONNECTIONS               1024
#define NIMCP_MAX_RETRIES                   3
#define NIMCP_RETRY_DELAY_MS                100
#define NIMCP_KEEPALIVE_INTERVAL_MS         30000
#define NIMCP_CONNECTION_TIMEOUT_MS         5000

/* ============================================================================
 * Validation Macros
 * ============================================================================ */

/**
 * @brief Validate that enum value is within valid range
 *
 * WHAT: Checks if enum value is non-negative and less than max count
 * WHY:  Prevent array out-of-bounds access when using enums as array indices
 * HOW:  Evaluates to true if value is in range [0, max), false otherwise
 *
 * @param val Enum value to validate
 * @param max Maximum valid value (exclusive, typically ENUM_COUNT)
 * @return true if valid, false otherwise
 *
 * EXAMPLE:
 *   if (!NIMCP_VALIDATE_ENUM(threat->type, THREAT_COUNT)) {
 *       LOG_ERROR("Invalid threat type: %d", threat->type);
 *       return NIMCP_INVALID_PARAM;
 *   }
 */
#define NIMCP_VALIDATE_ENUM(val, max) ((val) >= 0 && (val) < (max))

#endif /* NIMCP_COMMON_H */
