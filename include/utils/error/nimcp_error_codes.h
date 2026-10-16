/**
 * @file nimcp_error_codes.h
 * @brief Comprehensive error code system for NIMCP
 *
 * WHAT: Structured error codes replacing bool returns
 * WHY:  Better diagnostics, error propagation, and debugging
 * HOW:  Enum-based error codes with category grouping
 *
 * ERROR CODE RANGES:
 * 0000-0999: Success codes
 * 1000-1999: Generic errors
 * 2000-2999: Memory errors
 * 3000-3999: Brain/Network errors
 * 4000-4999: I/O errors
 * 5000-5999: Configuration errors
 * 6000-6999: Threading/Concurrency errors
 * 7000-7999: Signal/Crash errors
 * 8000-8999: Phase 10 cognitive errors
 * 9000-9099: Security errors (BBB, threats, access control)
 *
 * @author NIMCP Team
 * @date 2025-11-09
 */

#ifndef NIMCP_ERROR_CODES_H
#define NIMCP_ERROR_CODES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Error Code Type
//=============================================================================

#ifndef NIMCP_ERROR_TYPE_DEFINED
#define NIMCP_ERROR_TYPE_DEFINED
typedef int32_t nimcp_error_t;
#endif

//=============================================================================
// Success Codes (0-999)
// Note: NIMCP_SUCCESS may be defined in nimcp_common.h with same value
//=============================================================================

#ifndef NIMCP_SUCCESS
#define NIMCP_SUCCESS                   0    /**< Operation successful */
#endif
#ifndef NIMCP_OK
#define NIMCP_OK                        0    /**< Alias for success */
#endif
#define NIMCP_SUCCESS_WITH_WARNINGS     1    /**< Success but with warnings */
#define NIMCP_SUCCESS_PARTIAL           2    /**< Partial success */

//=============================================================================
// Generic Errors (1000-1999)
// Note: This is the canonical error code system. All error codes use positive
// values (1000+) following the convention: NIMCP_SUCCESS = 0, errors >= 1000.
// Guard macros prevent redefinition when other headers are included first.
//=============================================================================

#define NIMCP_ERROR_UNKNOWN             1000 /**< Unknown error */
#ifndef NIMCP_ERROR_NOT_IMPLEMENTED
#define NIMCP_ERROR_NOT_IMPLEMENTED     1001 /**< Feature not implemented */
#endif
#define NIMCP_ERROR_INVALID_PARAMETER   1002 /**< Invalid function parameter */
#ifndef NIMCP_ERROR_INVALID_PARAM
#define NIMCP_ERROR_INVALID_PARAM       NIMCP_ERROR_INVALID_PARAMETER /**< Alias for shorter name */
#endif
#ifndef NIMCP_ERROR_INVALID_ARGUMENT
#define NIMCP_ERROR_INVALID_ARGUMENT    NIMCP_ERROR_INVALID_PARAMETER /**< Alias for clarity */
#endif
#ifndef NIMCP_ERROR_NULL_POINTER
#define NIMCP_ERROR_NULL_POINTER        1003 /**< Unexpected NULL pointer */
#endif
#define NIMCP_ERROR_OUT_OF_RANGE        1004 /**< Value out of valid range */
#define NIMCP_ERROR_INVALID_STATE       1005 /**< Invalid object state */
#define NIMCP_ERROR_OPERATION_FAILED    1006 /**< Generic operation failure */
#define NIMCP_ERROR_NOT_INITIALIZED     1007 /**< Object not initialized */
#define NIMCP_ERROR_ALREADY_EXISTS      1008 /**< Resource already exists */
#ifndef NIMCP_ERROR_NOT_FOUND
#define NIMCP_ERROR_NOT_FOUND           1009 /**< Resource not found */
#endif
#ifndef NIMCP_ERROR_TIMEOUT
#define NIMCP_ERROR_TIMEOUT             1010 /**< Operation timed out */
#endif
#define NIMCP_ERROR_CANCELLED           1011 /**< Operation cancelled */
#define NIMCP_ERROR_PERMISSION_DENIED   1012 /**< Permission denied */

//=============================================================================
// GPU/Hardware Errors (1100-1199)
//=============================================================================

#ifndef NIMCP_ERROR_GPU
#define NIMCP_ERROR_GPU                 1100 /**< GPU operation failed */
#endif
#define NIMCP_ERROR_GPU_NOT_AVAILABLE   1101 /**< GPU not available */
#define NIMCP_ERROR_GPU_MEMORY          1102 /**< GPU memory allocation failed */
#define NIMCP_ERROR_CUDA                1103 /**< CUDA error */
#define NIMCP_ERROR_KERNEL_LAUNCH       1104 /**< Kernel launch failed */
#define NIMCP_ERROR_GPU_SYNC            1105 /**< GPU synchronization failed */

//=============================================================================
// Memory Errors (2000-2999)
//=============================================================================

#define NIMCP_ERROR_NO_MEMORY           2000 /**< Memory allocation failed */
#ifndef NIMCP_ERROR_MEMORY
#define NIMCP_ERROR_MEMORY              NIMCP_ERROR_NO_MEMORY /**< Alias for consistency */
#endif
#ifndef NIMCP_ERROR_BUFFER_TOO_SMALL
#define NIMCP_ERROR_BUFFER_TOO_SMALL    2001 /**< Buffer too small */
#endif
#ifndef NIMCP_ERROR_BUFFER_OVERFLOW
#define NIMCP_ERROR_BUFFER_OVERFLOW     2002 /**< Buffer overflow detected */
#endif
#define NIMCP_ERROR_MEMORY_CORRUPTION   2003 /**< Memory corruption detected */
#define NIMCP_ERROR_INVALID_ADDRESS     2004 /**< Invalid memory address */
#define NIMCP_ERROR_MEMORY_LEAK         2005 /**< Memory leak detected */
#define NIMCP_ERROR_DOUBLE_FREE         2006 /**< Double free detected */
#define NIMCP_ERROR_CAPACITY_EXCEEDED   2007 /**< Capacity limit exceeded */
#ifndef NIMCP_ERROR_OUT_OF_MEMORY
#define NIMCP_ERROR_OUT_OF_MEMORY       2008 /**< Out of memory (alias) */
#endif
#ifndef NIMCP_ERROR_QUEUE_FULL
#define NIMCP_ERROR_QUEUE_FULL          2009 /**< Queue is full */
#endif
#ifndef NIMCP_ERROR_OVERFLOW
#define NIMCP_ERROR_OVERFLOW            2010 /**< Integer overflow detected */
#endif

//=============================================================================
// Mesh Network Errors (2100-2199)
//=============================================================================

#define NIMCP_ERROR_MESH_BASE           2100 /**< Base for mesh network errors */
#define NIMCP_ERROR_NOT_LEADER          2101 /**< Node is not the leader */
#define NIMCP_ERROR_UNAUTHORIZED        2102 /**< Unauthorized operation */
#define NIMCP_ERROR_CHANNEL_ACCESS_DENIED 2103 /**< Channel access denied */
#define NIMCP_ERROR_MESH_CHANNEL_FULL   2104 /**< Channel participant limit reached */
#define NIMCP_ERROR_MESH_TX_FAILED      2105 /**< Transaction failed */
#define NIMCP_ERROR_MESH_ENDORSEMENT    2106 /**< Endorsement policy not satisfied */
#define NIMCP_ERROR_MESH_ORDERING       2107 /**< Ordering service error */
#define NIMCP_ERROR_MESH_CONSENSUS      2108 /**< Consensus not reached */
#define NIMCP_ERROR_CONFLICT_LOST       2109 /**< Lost conflict resolution */
#define NIMCP_ERROR_BACKPRESSURE        2110 /**< Backpressure - queue nearly full */
#define NIMCP_ERROR_PARTITION_DETECTED  2111 /**< Network partition detected */
#define NIMCP_ERROR_SPLIT_BRAIN         2112 /**< Split-brain scenario detected */
#define NIMCP_ERROR_CASCADE_FAILURE     2113 /**< Cascading failure detected */
#define NIMCP_ERROR_NOT_READY           2114 /**< System or component not ready */

//=============================================================================
// Brain/Network Errors (3000-3999)
//=============================================================================

#define NIMCP_ERROR_BRAIN_CREATION      3000 /**< Brain creation failed */
#define NIMCP_ERROR_BRAIN_INVALID       3001 /**< Invalid brain instance */
#define NIMCP_ERROR_NETWORK_CREATION    3002 /**< Neural network creation failed */
#define NIMCP_ERROR_NETWORK_INVALID     3003 /**< Invalid network structure */
#define NIMCP_ERROR_DIMENSION_MISMATCH  3004 /**< Dimension mismatch */
#define NIMCP_ERROR_WEIGHT_INIT         3005 /**< Weight initialization failed */
#define NIMCP_ERROR_FORWARD_PASS        3006 /**< Forward pass failed */
#define NIMCP_ERROR_BACKWARD_PASS       3007 /**< Backward pass failed */
#define NIMCP_ERROR_LEARNING_FAILED     3008 /**< Learning step failed */
#define NIMCP_ERROR_INFERENCE_FAILED    3009 /**< Inference failed */
#define NIMCP_ERROR_COW_FAILED          3010 /**< Copy-on-write operation failed */
#define NIMCP_ERROR_CLONE_FAILED        3011 /**< Brain clone failed */

/* KG Module Wiring Errors (3050-3099) */
#define NIMCP_ERROR_KG_WIRING_BASE            3050 /**< KG wiring base error */
#define NIMCP_ERROR_KG_WIRING_CREATE          3050 /**< KG wiring creation failed */
#define NIMCP_ERROR_KG_WIRING_NULL            3051 /**< NULL wiring descriptor */
#define NIMCP_ERROR_KG_WIRING_INPUTS_FULL     3052 /**< Max 32 inputs reached */
#define NIMCP_ERROR_KG_WIRING_OUTPUTS_FULL    3053 /**< Max 32 outputs reached */
#define NIMCP_ERROR_KG_WIRING_HANDLERS_FULL   3054 /**< Max 64 handlers reached */
#define NIMCP_ERROR_KG_WIRING_METADATA_FULL   3055 /**< Max 16 metadata entries */
#define NIMCP_ERROR_KG_WIRING_STRING_TOO_LONG 3056 /**< String exceeds max length */
#define NIMCP_ERROR_KG_WIRING_INVALID_NAME    3057 /**< Invalid module name */
#define NIMCP_ERROR_KG_WIRING_INVALID_TYPE    3058 /**< Invalid module type */
#define NIMCP_ERROR_KG_WIRING_WEIGHT_ALLOC    3059 /**< Weight allocation failed */
#define NIMCP_ERROR_KG_WIRING_WEIGHT_INVALID  3060 /**< NULL weights with size > 0 */
#define NIMCP_ERROR_KG_WIRING_VALIDATION      3061 /**< Validation failed */
#define NIMCP_ERROR_KG_WIRING_DUPLICATE       3062 /**< Duplicate entry */

//=============================================================================
// I/O Errors (4000-4999)
//=============================================================================

#define NIMCP_ERROR_FILE_NOT_FOUND      4000 /**< File not found */
#define NIMCP_ERROR_FILE_READ           4001 /**< File read error */
#define NIMCP_ERROR_FILE_WRITE          4002 /**< File write error */
#define NIMCP_ERROR_FILE_OPEN           4003 /**< File open error */
#define NIMCP_ERROR_FILE_CLOSE          4004 /**< File close error */
#define NIMCP_ERROR_FILE_CORRUPT        4005 /**< File corrupted */
#ifndef NIMCP_ERROR_SERIALIZATION
#define NIMCP_ERROR_SERIALIZATION       4006 /**< Serialization failed */
#endif
#ifndef NIMCP_ERROR_DESERIALIZATION
#define NIMCP_ERROR_DESERIALIZATION     4007 /**< Deserialization failed */
#endif
#define NIMCP_ERROR_NETWORK_IO          4008 /**< Network I/O error */
#define NIMCP_ERROR_SOCKET_ERROR        4009 /**< Socket operation failed */
#define NIMCP_ERROR_IO                  4010 /**< Generic I/O error */

//=============================================================================
// Configuration Errors (5000-5999)
//=============================================================================

#define NIMCP_ERROR_CONFIG_INVALID      5000 /**< Invalid configuration */
#define NIMCP_ERROR_CONFIG_PARSE        5001 /**< Config parse error */
#define NIMCP_ERROR_CONFIG_MISSING      5002 /**< Required config missing */
#define NIMCP_ERROR_CONFIG_TYPE         5003 /**< Config type mismatch */
#define NIMCP_ERROR_CONFIG_RANGE        5004 /**< Config value out of range */
#define NIMCP_ERROR_CONFIG_RELOAD       5005 /**< Config reload failed */

//=============================================================================
// Threading/Concurrency Errors (6000-6999)
//=============================================================================

#define NIMCP_ERROR_THREAD_CREATE       6000 /**< Thread creation failed */
#define NIMCP_ERROR_THREAD_JOIN         6001 /**< Thread join failed */
#define NIMCP_ERROR_MUTEX_LOCK          6002 /**< Mutex lock failed */
#define NIMCP_ERROR_MUTEX_UNLOCK        6003 /**< Mutex unlock failed */
#define NIMCP_ERROR_MUTEX_INIT          6004 /**< Mutex init failed */
#define NIMCP_ERROR_DEADLOCK            6005 /**< Deadlock detected */
#define NIMCP_ERROR_RACE_CONDITION      6006 /**< Race condition detected */
#define NIMCP_ERROR_THREAD_SYNC         6007 /**< Thread synchronization failed */

//=============================================================================
// Signal/Crash Errors (7000-7999)
//=============================================================================

#define NIMCP_ERROR_SIGNAL_RECEIVED     7000 /**< Signal received */
#define NIMCP_ERROR_SIGSEGV             7001 /**< Segmentation fault */
#define NIMCP_ERROR_SIGABRT             7002 /**< Abort signal */
#define NIMCP_ERROR_SIGFPE              7003 /**< Floating point exception */
#define NIMCP_ERROR_SIGBUS              7004 /**< Bus error */
#define NIMCP_ERROR_SIGILL              7005 /**< Illegal instruction */
#define NIMCP_ERROR_CRASH_RECOVERY      7006 /**< Crash recovery failed */
#define NIMCP_ERROR_CHECKPOINT_SAVE     7007 /**< Checkpoint save failed */
#define NIMCP_ERROR_CHECKPOINT_LOAD     7008 /**< Checkpoint load failed */

//=============================================================================
// Phase 10 Cognitive Errors (8000-8999)
//=============================================================================

#define NIMCP_ERROR_WORKING_MEMORY      8000 /**< Working memory error */
#define NIMCP_ERROR_EMOTIONAL_TAGGING   8001 /**< Emotional tagging error */
#define NIMCP_ERROR_EXECUTIVE_CONTROL   8002 /**< Executive control error */
#define NIMCP_ERROR_SLEEP_WAKE          8003 /**< Sleep/wake cycle error */
#define NIMCP_ERROR_MENTAL_HEALTH       8004 /**< Mental health monitor error */
#define NIMCP_ERROR_THEORY_OF_MIND      8005 /**< Theory of mind error */
#define NIMCP_ERROR_EXPLANATIONS        8006 /**< Natural explanations error */
#define NIMCP_ERROR_META_LEARNING       8007 /**< Meta-learning error */
#define NIMCP_ERROR_PREDICTIVE          8008 /**< Predictive processing error */

//=============================================================================
// Security Errors (9000-9099)
//=============================================================================

#define NIMCP_ERROR_SECURITY_BASE           9000 /**< Security error base */
#define NIMCP_ERROR_BBB_REJECTED            9001 /**< Blood-Brain Barrier rejected input */
#define NIMCP_ERROR_BBB_VALIDATION          9002 /**< BBB validation failed */
#define NIMCP_ERROR_SECURITY_THREAT         9003 /**< Security threat detected */
#define NIMCP_ERROR_ACCESS_DENIED           9004 /**< Security access denied */
#define NIMCP_ERROR_SIGNATURE_INVALID       9005 /**< Signature validation failed */
#ifndef NIMCP_ERROR_ENCRYPTION_FAILED
#define NIMCP_ERROR_ENCRYPTION_FAILED       9006 /**< Encryption operation failed */
#endif
#ifndef NIMCP_ERROR_DECRYPTION_FAILED
#define NIMCP_ERROR_DECRYPTION_FAILED       9007 /**< Decryption operation failed */
#endif
#define NIMCP_ERROR_CERTIFICATE_INVALID     9008 /**< Certificate validation failed */
#define NIMCP_ERROR_POLICY_VIOLATION        9009 /**< Security policy violation */
#define NIMCP_ERROR_QUARANTINE_REQUIRED     9010 /**< Resource requires quarantine */
#define NIMCP_ERROR_ETHICS_VIOLATION        9011 /**< Ethics evaluation denied operation */
#define NIMCP_ERROR_LGSS_DENIED             9012 /**< LGSS safety gate denied operation */
#define NIMCP_ERROR_SYSTEM_HALTED           9013 /**< System is in emergency halt state */
#define NIMCP_ERROR_HALT_IN_PROGRESS        9014 /**< Emergency halt is in progress */
#define NIMCP_ERROR_TRIPWIRE_TRIGGERED      9015 /**< Safety tripwire was triggered */
#define NIMCP_ERROR_CORRIGIBILITY_VIOLATION 9016 /**< Corrigibility constraint violated */
#define NIMCP_ERROR_ALIGNMENT_DRIFT         9017 /**< Alignment drift detected */
#define NIMCP_ERROR_CAPABILITY_EXCEEDED     9018 /**< Capability envelope exceeded */

//=============================================================================
// Brain Region Errors (10000-19999)
// Each brain region gets a 100-code range for module-specific errors.
// This allows unified error handling while preserving module-specific semantics.
//
// MAPPING:
// Module-local enum value 0 -> _BASE + 0 (success/none)
// Module-local enum value 1 -> _BASE + 1 (invalid input)
// Module-local enum value N -> _BASE + N
//=============================================================================

/* Motor Cortex Errors (10000-10099) */
#define NIMCP_ERROR_MOTOR_BASE          10000
#define NIMCP_ERROR_MOTOR_NONE          (NIMCP_ERROR_MOTOR_BASE + 0)  /**< No error */
#define NIMCP_ERROR_MOTOR_INVALID_INPUT (NIMCP_ERROR_MOTOR_BASE + 1)  /**< Invalid input */
#define NIMCP_ERROR_MOTOR_PLANNING      (NIMCP_ERROR_MOTOR_BASE + 2)  /**< Planning failure */
#define NIMCP_ERROR_MOTOR_EXECUTION     (NIMCP_ERROR_MOTOR_BASE + 3)  /**< Execution failure */
#define NIMCP_ERROR_MOTOR_TRAJECTORY    (NIMCP_ERROR_MOTOR_BASE + 4)  /**< Trajectory infeasible */
#define NIMCP_ERROR_MOTOR_EFFECTOR      (NIMCP_ERROR_MOTOR_BASE + 5)  /**< Effector conflict */
#define NIMCP_ERROR_MOTOR_TIMING        (NIMCP_ERROR_MOTOR_BASE + 6)  /**< Timing violation */
#define NIMCP_ERROR_MOTOR_BUFFER        (NIMCP_ERROR_MOTOR_BASE + 7)  /**< Buffer overflow */
#define NIMCP_ERROR_MOTOR_INTERNAL      (NIMCP_ERROR_MOTOR_BASE + 8)  /**< Internal error */

/* Hippocampus Errors (10100-10199) */
#define NIMCP_ERROR_HIPPOCAMPUS_BASE              10100
#define NIMCP_ERROR_HIPPOCAMPUS_NONE              (NIMCP_ERROR_HIPPOCAMPUS_BASE + 0)
#define NIMCP_ERROR_HIPPOCAMPUS_INVALID_INPUT     (NIMCP_ERROR_HIPPOCAMPUS_BASE + 1)
#define NIMCP_ERROR_HIPPOCAMPUS_ENCODING          (NIMCP_ERROR_HIPPOCAMPUS_BASE + 2)
#define NIMCP_ERROR_HIPPOCAMPUS_RETRIEVAL         (NIMCP_ERROR_HIPPOCAMPUS_BASE + 3)
#define NIMCP_ERROR_HIPPOCAMPUS_NAVIGATION        (NIMCP_ERROR_HIPPOCAMPUS_BASE + 4)
#define NIMCP_ERROR_HIPPOCAMPUS_MEMORY_FULL       (NIMCP_ERROR_HIPPOCAMPUS_BASE + 5)
#define NIMCP_ERROR_HIPPOCAMPUS_PATTERN_SEP       (NIMCP_ERROR_HIPPOCAMPUS_BASE + 6)
#define NIMCP_ERROR_HIPPOCAMPUS_PATTERN_COMP      (NIMCP_ERROR_HIPPOCAMPUS_BASE + 7)
#define NIMCP_ERROR_HIPPOCAMPUS_BUFFER            (NIMCP_ERROR_HIPPOCAMPUS_BASE + 8)
#define NIMCP_ERROR_HIPPOCAMPUS_INTERNAL          (NIMCP_ERROR_HIPPOCAMPUS_BASE + 9)

/* Entorhinal Cortex Errors (10200-10299) */
#define NIMCP_ERROR_ENTORHINAL_BASE               10200
#define NIMCP_ERROR_ENTORHINAL_NONE               (NIMCP_ERROR_ENTORHINAL_BASE + 0)
#define NIMCP_ERROR_ENTORHINAL_INVALID_INPUT      (NIMCP_ERROR_ENTORHINAL_BASE + 1)
#define NIMCP_ERROR_ENTORHINAL_GRID_DRIFT         (NIMCP_ERROR_ENTORHINAL_BASE + 2)
#define NIMCP_ERROR_ENTORHINAL_PATH_INTEGRATION   (NIMCP_ERROR_ENTORHINAL_BASE + 3)
#define NIMCP_ERROR_ENTORHINAL_GATEWAY_BLOCKED    (NIMCP_ERROR_ENTORHINAL_BASE + 4)
#define NIMCP_ERROR_ENTORHINAL_SECURITY           (NIMCP_ERROR_ENTORHINAL_BASE + 5)
#define NIMCP_ERROR_ENTORHINAL_IMMUNE             (NIMCP_ERROR_ENTORHINAL_BASE + 6)
#define NIMCP_ERROR_ENTORHINAL_SUBSTRATE          (NIMCP_ERROR_ENTORHINAL_BASE + 7)
#define NIMCP_ERROR_ENTORHINAL_SYNC               (NIMCP_ERROR_ENTORHINAL_BASE + 8)
#define NIMCP_ERROR_ENTORHINAL_BUFFER             (NIMCP_ERROR_ENTORHINAL_BASE + 9)
#define NIMCP_ERROR_ENTORHINAL_INTERNAL           (NIMCP_ERROR_ENTORHINAL_BASE + 10)

/* Prefrontal Cortex Errors (10300-10399) */
#define NIMCP_ERROR_PREFRONTAL_BASE               10300
#define NIMCP_ERROR_PREFRONTAL_NONE               (NIMCP_ERROR_PREFRONTAL_BASE + 0)
#define NIMCP_ERROR_PREFRONTAL_INVALID_INPUT      (NIMCP_ERROR_PREFRONTAL_BASE + 1)
#define NIMCP_ERROR_PREFRONTAL_PLANNING           (NIMCP_ERROR_PREFRONTAL_BASE + 2)
#define NIMCP_ERROR_PREFRONTAL_WORKING_MEMORY     (NIMCP_ERROR_PREFRONTAL_BASE + 3)
#define NIMCP_ERROR_PREFRONTAL_INHIBITION         (NIMCP_ERROR_PREFRONTAL_BASE + 4)
#define NIMCP_ERROR_PREFRONTAL_INTERNAL           (NIMCP_ERROR_PREFRONTAL_BASE + 5)

/* Cerebellum Errors (10400-10499) */
#define NIMCP_ERROR_CEREBELLUM_BASE               10400
#define NIMCP_ERROR_CEREBELLUM_NONE               (NIMCP_ERROR_CEREBELLUM_BASE + 0)
#define NIMCP_ERROR_CEREBELLUM_INVALID_INPUT      (NIMCP_ERROR_CEREBELLUM_BASE + 1)
#define NIMCP_ERROR_CEREBELLUM_TIMING             (NIMCP_ERROR_CEREBELLUM_BASE + 2)
#define NIMCP_ERROR_CEREBELLUM_PREDICTION         (NIMCP_ERROR_CEREBELLUM_BASE + 3)
#define NIMCP_ERROR_CEREBELLUM_INTERNAL           (NIMCP_ERROR_CEREBELLUM_BASE + 4)

/* Thalamus Errors (10500-10599) */
#define NIMCP_ERROR_THALAMUS_BASE                 10500
#define NIMCP_ERROR_THALAMUS_NONE                 (NIMCP_ERROR_THALAMUS_BASE + 0)
#define NIMCP_ERROR_THALAMUS_INVALID_INPUT        (NIMCP_ERROR_THALAMUS_BASE + 1)
#define NIMCP_ERROR_THALAMUS_RELAY                (NIMCP_ERROR_THALAMUS_BASE + 2)
#define NIMCP_ERROR_THALAMUS_GATING               (NIMCP_ERROR_THALAMUS_BASE + 3)
#define NIMCP_ERROR_THALAMUS_INTERNAL             (NIMCP_ERROR_THALAMUS_BASE + 4)

/* Hypothalamus Errors (10600-10699) */
#define NIMCP_ERROR_HYPOTHALAMUS_BASE             10600
#define NIMCP_ERROR_HYPOTHALAMUS_NONE             (NIMCP_ERROR_HYPOTHALAMUS_BASE + 0)
#define NIMCP_ERROR_HYPOTHALAMUS_INVALID_INPUT    (NIMCP_ERROR_HYPOTHALAMUS_BASE + 1)
#define NIMCP_ERROR_HYPOTHALAMUS_HOMEOSTASIS      (NIMCP_ERROR_HYPOTHALAMUS_BASE + 2)
#define NIMCP_ERROR_HYPOTHALAMUS_MOTIVATION       (NIMCP_ERROR_HYPOTHALAMUS_BASE + 3)
#define NIMCP_ERROR_HYPOTHALAMUS_INTERNAL         (NIMCP_ERROR_HYPOTHALAMUS_BASE + 4)

/* Amygdala Errors (10700-10799) */
#define NIMCP_ERROR_AMYGDALA_BASE                 10700
#define NIMCP_ERROR_AMYGDALA_NONE                 (NIMCP_ERROR_AMYGDALA_BASE + 0)
#define NIMCP_ERROR_AMYGDALA_INVALID_INPUT        (NIMCP_ERROR_AMYGDALA_BASE + 1)
#define NIMCP_ERROR_AMYGDALA_FEAR_PROCESSING      (NIMCP_ERROR_AMYGDALA_BASE + 2)
#define NIMCP_ERROR_AMYGDALA_EMOTIONAL_TAG        (NIMCP_ERROR_AMYGDALA_BASE + 3)
#define NIMCP_ERROR_AMYGDALA_INTERNAL             (NIMCP_ERROR_AMYGDALA_BASE + 4)

/* Basal Ganglia Errors (10800-10899) */
#define NIMCP_ERROR_BASAL_GANGLIA_BASE            10800
#define NIMCP_ERROR_BASAL_GANGLIA_NONE            (NIMCP_ERROR_BASAL_GANGLIA_BASE + 0)
#define NIMCP_ERROR_BASAL_GANGLIA_INVALID_INPUT   (NIMCP_ERROR_BASAL_GANGLIA_BASE + 1)
#define NIMCP_ERROR_BASAL_GANGLIA_ACTION_SELECT   (NIMCP_ERROR_BASAL_GANGLIA_BASE + 2)
#define NIMCP_ERROR_BASAL_GANGLIA_DOPAMINE        (NIMCP_ERROR_BASAL_GANGLIA_BASE + 3)
#define NIMCP_ERROR_BASAL_GANGLIA_INTERNAL        (NIMCP_ERROR_BASAL_GANGLIA_BASE + 4)

/* Cingulate Cortex Errors (10900-10999) */
#define NIMCP_ERROR_CINGULATE_BASE                10900
#define NIMCP_ERROR_CINGULATE_NONE                (NIMCP_ERROR_CINGULATE_BASE + 0)
#define NIMCP_ERROR_CINGULATE_INVALID_INPUT       (NIMCP_ERROR_CINGULATE_BASE + 1)
#define NIMCP_ERROR_CINGULATE_CONFLICT            (NIMCP_ERROR_CINGULATE_BASE + 2)
#define NIMCP_ERROR_CINGULATE_ERROR_MONITORING    (NIMCP_ERROR_CINGULATE_BASE + 3)
#define NIMCP_ERROR_CINGULATE_INTERNAL            (NIMCP_ERROR_CINGULATE_BASE + 4)

/* Insula Errors (11000-11099) */
#define NIMCP_ERROR_INSULA_BASE                   11000
#define NIMCP_ERROR_INSULA_NONE                   (NIMCP_ERROR_INSULA_BASE + 0)
#define NIMCP_ERROR_INSULA_INVALID_INPUT          (NIMCP_ERROR_INSULA_BASE + 1)
#define NIMCP_ERROR_INSULA_INTEROCEPTION          (NIMCP_ERROR_INSULA_BASE + 2)
#define NIMCP_ERROR_INSULA_AWARENESS              (NIMCP_ERROR_INSULA_BASE + 3)
#define NIMCP_ERROR_INSULA_INTERNAL               (NIMCP_ERROR_INSULA_BASE + 4)

/* Occipital Lobe Errors (11100-11199) */
#define NIMCP_ERROR_OCCIPITAL_BASE                11100
#define NIMCP_ERROR_OCCIPITAL_NONE                (NIMCP_ERROR_OCCIPITAL_BASE + 0)
#define NIMCP_ERROR_OCCIPITAL_INVALID_INPUT       (NIMCP_ERROR_OCCIPITAL_BASE + 1)
#define NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING   (NIMCP_ERROR_OCCIPITAL_BASE + 2)
#define NIMCP_ERROR_OCCIPITAL_FEATURE_EXTRACTION  (NIMCP_ERROR_OCCIPITAL_BASE + 3)
#define NIMCP_ERROR_OCCIPITAL_INTERNAL            (NIMCP_ERROR_OCCIPITAL_BASE + 4)

/* Parietal Lobe Errors (11200-11299) */
#define NIMCP_ERROR_PARIETAL_BASE                 11200
#define NIMCP_ERROR_PARIETAL_NONE                 (NIMCP_ERROR_PARIETAL_BASE + 0)
#define NIMCP_ERROR_PARIETAL_INVALID_INPUT        (NIMCP_ERROR_PARIETAL_BASE + 1)
#define NIMCP_ERROR_PARIETAL_SPATIAL              (NIMCP_ERROR_PARIETAL_BASE + 2)
#define NIMCP_ERROR_PARIETAL_ATTENTION            (NIMCP_ERROR_PARIETAL_BASE + 3)
#define NIMCP_ERROR_PARIETAL_INTERNAL             (NIMCP_ERROR_PARIETAL_BASE + 4)

/* Temporal Lobe Errors (11300-11399) */
#define NIMCP_ERROR_TEMPORAL_BASE                 11300
#define NIMCP_ERROR_TEMPORAL_NONE                 (NIMCP_ERROR_TEMPORAL_BASE + 0)
#define NIMCP_ERROR_TEMPORAL_INVALID_INPUT        (NIMCP_ERROR_TEMPORAL_BASE + 1)
#define NIMCP_ERROR_TEMPORAL_AUDITORY             (NIMCP_ERROR_TEMPORAL_BASE + 2)
#define NIMCP_ERROR_TEMPORAL_SEMANTIC             (NIMCP_ERROR_TEMPORAL_BASE + 3)
#define NIMCP_ERROR_TEMPORAL_INTERNAL             (NIMCP_ERROR_TEMPORAL_BASE + 4)

/* Broca's Area Errors (11400-11499) */
#define NIMCP_ERROR_BROCA_BASE                    11400
#define NIMCP_ERROR_BROCA_NONE                    (NIMCP_ERROR_BROCA_BASE + 0)
#define NIMCP_ERROR_BROCA_INVALID_INPUT           (NIMCP_ERROR_BROCA_BASE + 1)
#define NIMCP_ERROR_BROCA_PRODUCTION              (NIMCP_ERROR_BROCA_BASE + 2)
#define NIMCP_ERROR_BROCA_SYNTAX                  (NIMCP_ERROR_BROCA_BASE + 3)
#define NIMCP_ERROR_BROCA_INTERNAL                (NIMCP_ERROR_BROCA_BASE + 4)

/* Wernicke's Area Errors (11500-11599) */
#define NIMCP_ERROR_WERNICKE_BASE                 11500
#define NIMCP_ERROR_WERNICKE_NONE                 (NIMCP_ERROR_WERNICKE_BASE + 0)
#define NIMCP_ERROR_WERNICKE_INVALID_INPUT        (NIMCP_ERROR_WERNICKE_BASE + 1)
#define NIMCP_ERROR_WERNICKE_COMPREHENSION        (NIMCP_ERROR_WERNICKE_BASE + 2)
#define NIMCP_ERROR_WERNICKE_SEMANTIC             (NIMCP_ERROR_WERNICKE_BASE + 3)
#define NIMCP_ERROR_WERNICKE_INTERNAL             (NIMCP_ERROR_WERNICKE_BASE + 4)

/* Brainstem Errors (11600-11699) */
#define NIMCP_ERROR_BRAINSTEM_BASE                11600
#define NIMCP_ERROR_BRAINSTEM_NONE                (NIMCP_ERROR_BRAINSTEM_BASE + 0)
#define NIMCP_ERROR_BRAINSTEM_INVALID_INPUT       (NIMCP_ERROR_BRAINSTEM_BASE + 1)
#define NIMCP_ERROR_BRAINSTEM_VITAL_FUNCTION      (NIMCP_ERROR_BRAINSTEM_BASE + 2)
#define NIMCP_ERROR_BRAINSTEM_AROUSAL             (NIMCP_ERROR_BRAINSTEM_BASE + 3)
#define NIMCP_ERROR_BRAINSTEM_INTERNAL            (NIMCP_ERROR_BRAINSTEM_BASE + 4)

/* Parahippocampal Region Errors (11700-11799) */
#define NIMCP_ERROR_PARAHIPPOCAMPAL_BASE          11700
#define NIMCP_ERROR_PARAHIPPOCAMPAL_NONE          (NIMCP_ERROR_PARAHIPPOCAMPAL_BASE + 0)
#define NIMCP_ERROR_PARAHIPPOCAMPAL_INVALID_INPUT (NIMCP_ERROR_PARAHIPPOCAMPAL_BASE + 1)
#define NIMCP_ERROR_PARAHIPPOCAMPAL_SCENE         (NIMCP_ERROR_PARAHIPPOCAMPAL_BASE + 2)
#define NIMCP_ERROR_PARAHIPPOCAMPAL_CONTEXT       (NIMCP_ERROR_PARAHIPPOCAMPAL_BASE + 3)
#define NIMCP_ERROR_PARAHIPPOCAMPAL_INTERNAL      (NIMCP_ERROR_PARAHIPPOCAMPAL_BASE + 4)

/* Perirhinal Cortex Errors (11800-11899) */
#define NIMCP_ERROR_PERIRHINAL_BASE               11800
#define NIMCP_ERROR_PERIRHINAL_NONE               (NIMCP_ERROR_PERIRHINAL_BASE + 0)
#define NIMCP_ERROR_PERIRHINAL_INVALID_INPUT      (NIMCP_ERROR_PERIRHINAL_BASE + 1)
#define NIMCP_ERROR_PERIRHINAL_OBJECT_RECOGNITION (NIMCP_ERROR_PERIRHINAL_BASE + 2)
#define NIMCP_ERROR_PERIRHINAL_FAMILIARITY        (NIMCP_ERROR_PERIRHINAL_BASE + 3)
#define NIMCP_ERROR_PERIRHINAL_INTERNAL           (NIMCP_ERROR_PERIRHINAL_BASE + 4)

/* VTA Errors (11900-11999) */
#define NIMCP_ERROR_VTA_BASE                      11900
#define NIMCP_ERROR_VTA_NONE                      (NIMCP_ERROR_VTA_BASE + 0)
#define NIMCP_ERROR_VTA_INVALID_INPUT             (NIMCP_ERROR_VTA_BASE + 1)
#define NIMCP_ERROR_VTA_REWARD                    (NIMCP_ERROR_VTA_BASE + 2)
#define NIMCP_ERROR_VTA_DOPAMINE                  (NIMCP_ERROR_VTA_BASE + 3)
#define NIMCP_ERROR_VTA_INTERNAL                  (NIMCP_ERROR_VTA_BASE + 4)

//=============================================================================
// Error Information Structure
// Note: nimcp_common.h defines a simpler version; guard prevents conflict
//=============================================================================

/**
 * @brief Detailed error information (extended version)
 *
 * Note: This is the extended version with file/line/function info.
 * nimcp_common.h defines a simpler version with just code and message.
 * We use a different struct name to avoid conflict.
 */
typedef struct nimcp_error_info_extended {
    nimcp_error_t code;        /**< Error code */
    const char* message;       /**< Human-readable message */
    const char* file;          /**< Source file where error occurred */
    int line;                  /**< Line number where error occurred */
    const char* function;      /**< Function where error occurred */
    void* context;             /**< Optional context data */
} nimcp_error_info_extended_t;

// Provide nimcp_error_info_t if not already defined by nimcp_common.h
#ifndef NIMCP_ERROR_INFO_T_DEFINED
#define NIMCP_ERROR_INFO_T_DEFINED
typedef nimcp_error_info_extended_t nimcp_error_info_t;
#endif

//=============================================================================
// Error Handling API
//=============================================================================

/**
 * @brief Check if error code indicates success
 */
static inline bool nimcp_error_is_success(nimcp_error_t code)
{
    return (code >= 0 && code < 1000);
}

/**
 * @brief Check if error code indicates failure
 */
static inline bool nimcp_error_is_failure(nimcp_error_t code)
{
    return (code >= 1000);
}

/**
 * @brief Get error category from error code
 */
static inline int nimcp_error_get_category(nimcp_error_t code)
{
    return (code / 1000);
}

/**
 * @brief Get human-readable error message
 *
 * @param code Error code
 * @return Error message string
 */
const char* nimcp_error_to_string(nimcp_error_t code);

/**
 * @brief Get error category name
 *
 * @param code Error code
 * @return Category name (e.g., "Memory Error")
 */
const char* nimcp_error_get_category_name(nimcp_error_t code);

/**
 * @brief Set last error (thread-local)
 *
 * @param code Error code
 * @param file Source file (use __FILE__)
 * @param line Line number (use __LINE__)
 * @param function Function name (use __func__)
 * @param message Optional message (can be NULL)
 */
void nimcp_error_set(nimcp_error_t code, const char* file, int line,
                     const char* function, const char* message);

/**
 * @brief Get last error info (thread-local)
 *
 * @return Pointer to error info (valid until next error_set)
 */
const nimcp_error_info_t* nimcp_error_get_last(void);

/**
 * @brief Clear last error
 */
void nimcp_error_clear(void);

/**
 * @brief Print error to stderr
 *
 * @param code Error code
 */
void nimcp_error_print(nimcp_error_t code);

/**
 * @brief Print detailed error info to stderr
 *
 * @param info Error information
 */
void nimcp_error_print_detailed(const nimcp_error_info_t* info);

//=============================================================================
// Convenience Macros
//=============================================================================

/**
 * @brief Set error with file/line/function info
 */
#define NIMCP_SET_ERROR(code, msg) \
    nimcp_error_set((code), __FILE__, __LINE__, __func__, (msg))

/**
 * @brief Return error with auto-set
 */
#define NIMCP_RETURN_ERROR(code, msg) \
    do { \
        NIMCP_SET_ERROR((code), (msg)); \
        return (code); \
    } while (0)

/**
 * @brief Check condition and return error if false
 */
#define NIMCP_CHECK(cond, code, msg) \
    do { \
        if (!(cond)) { \
            NIMCP_RETURN_ERROR((code), (msg)); \
        } \
    } while (0)

/**
 * @brief Propagate error if not success
 */
#define NIMCP_PROPAGATE_ERROR(expr) \
    do { \
        nimcp_error_t _err = (expr); \
        if (nimcp_error_is_failure(_err)) { \
            return _err; \
        } \
    } while (0)

//=============================================================================
// Cleanup Stack Pattern for Guaranteed Resource Cleanup
//=============================================================================

/**
 * @brief Maximum number of cleanup handlers in a stack
 */
#define NIMCP_CLEANUP_STACK_MAX 32

/**
 * @brief Cleanup function type
 *
 * @param resource Pointer to resource to clean up
 */
typedef void (*nimcp_cleanup_fn)(void* resource);

/**
 * @brief Cleanup stack entry
 */
typedef struct nimcp_cleanup_entry {
    nimcp_cleanup_fn cleanup;    /**< Cleanup function */
    void* resource;              /**< Resource to clean up */
    const char* name;            /**< Resource name (for debugging) */
} nimcp_cleanup_entry_t;

/**
 * @brief Cleanup stack for guaranteed resource cleanup
 *
 * Use this pattern for complex initialization with multiple resources:
 *
 * @code
 * nimcp_cleanup_stack_t cleanup = {0};
 *
 * void* res1 = allocate_resource1();
 * if (!res1) goto cleanup_and_exit;
 * nimcp_cleanup_push(&cleanup, free, res1, "resource1");
 *
 * void* res2 = allocate_resource2();
 * if (!res2) goto cleanup_and_exit;
 * nimcp_cleanup_push(&cleanup, free, res2, "resource2");
 *
 * // Success - clear stack to prevent cleanup
 * nimcp_cleanup_clear(&cleanup);
 * return success_result;
 *
 * cleanup_and_exit:
 *     nimcp_cleanup_execute(&cleanup);
 *     return error_result;
 * @endcode
 */
typedef struct nimcp_cleanup_stack {
    nimcp_cleanup_entry_t entries[NIMCP_CLEANUP_STACK_MAX];
    size_t count;
} nimcp_cleanup_stack_t;

/**
 * @brief Initialize a cleanup stack
 *
 * @param stack Cleanup stack to initialize
 */
static inline void nimcp_cleanup_init(nimcp_cleanup_stack_t* stack) {
    if (stack) {
        stack->count = 0;
    }
}

/**
 * @brief Push a cleanup handler onto the stack
 *
 * @param stack Cleanup stack
 * @param cleanup Cleanup function to call
 * @param resource Resource pointer to pass to cleanup function
 * @param name Resource name for debugging (can be NULL)
 * @return true on success, false if stack is full
 */
static inline bool nimcp_cleanup_push(nimcp_cleanup_stack_t* stack,
                                       nimcp_cleanup_fn cleanup,
                                       void* resource,
                                       const char* name) {
    if (!stack || stack->count >= NIMCP_CLEANUP_STACK_MAX) {
        return false;
    }
    nimcp_cleanup_entry_t* entry = &stack->entries[stack->count++];
    entry->cleanup = cleanup;
    entry->resource = resource;
    entry->name = name;
    return true;
}

/**
 * @brief Execute all cleanup handlers in reverse order (LIFO)
 *
 * @param stack Cleanup stack to execute
 */
static inline void nimcp_cleanup_execute(nimcp_cleanup_stack_t* stack) {
    if (!stack) return;

    // Execute in reverse order (LIFO)
    while (stack->count > 0) {
        stack->count--;
        nimcp_cleanup_entry_t* entry = &stack->entries[stack->count];
        if (entry->cleanup && entry->resource) {
            entry->cleanup(entry->resource);
        }
    }
}

/**
 * @brief Clear the cleanup stack without executing handlers
 *
 * Call this when initialization succeeds and resources should not be cleaned up.
 *
 * @param stack Cleanup stack to clear
 */
static inline void nimcp_cleanup_clear(nimcp_cleanup_stack_t* stack) {
    if (stack) {
        stack->count = 0;
    }
}

/**
 * @brief Pop the last cleanup handler without executing it
 *
 * Useful when ownership of a resource is transferred.
 *
 * @param stack Cleanup stack
 * @return Popped entry, or entry with NULL cleanup if stack is empty
 */
static inline nimcp_cleanup_entry_t nimcp_cleanup_pop(nimcp_cleanup_stack_t* stack) {
    nimcp_cleanup_entry_t empty = {NULL, NULL, NULL};
    if (!stack || stack->count == 0) {
        return empty;
    }
    return stack->entries[--stack->count];
}

//=============================================================================
// Extended Error Context API (Thread-Local with Formatted Messages)
//=============================================================================

/**
 * @brief Maximum length for error messages
 */
#define NIMCP_ERROR_MSG_MAX 512

/**
 * @brief Thread-local error context with formatted message support
 */
typedef struct nimcp_error_context {
    nimcp_error_t code;                      /**< Error code */
    char message[NIMCP_ERROR_MSG_MAX];       /**< Formatted error message */
    const char* file;                        /**< Source file (__FILE__) */
    int line;                                /**< Source line (__LINE__) */
    const char* function;                    /**< Function name (__func__) */
    uint64_t timestamp;                      /**< Time of error (microseconds) */
} nimcp_error_context_t;

/**
 * @brief Set thread-local error with formatted message
 *
 * @param code Error code
 * @param format Printf-style format string
 * @param ... Format arguments
 *
 * Example:
 * @code
 * nimcp_set_error(NIMCP_ERROR_INVALID_PARAMETER, "Expected size > 0, got %zu", size);
 * @endcode
 */
void nimcp_set_error(nimcp_error_t code, const char* format, ...);

/**
 * @brief Set error with source location (use NIMCP_ERROR_SET macro instead)
 *
 * @param code Error code
 * @param file Source file name
 * @param line Source line number
 * @param func Function name
 * @param format Message format
 * @param ... Format arguments
 */
void nimcp_set_error_ex(nimcp_error_t code, const char* file, int line,
                        const char* func, const char* format, ...);

/**
 * @brief Get last error code (thread-local)
 *
 * @return Last error code, or NIMCP_SUCCESS if no error
 */
nimcp_error_t nimcp_get_last_error(void);

/**
 * @brief Get last error message (thread-local)
 *
 * @return Error message string, or empty string if no error
 */
const char* nimcp_get_error_message(void);

/**
 * @brief Get full error context (thread-local)
 *
 * @return Pointer to error context, or NULL if no context available
 */
const nimcp_error_context_t* nimcp_get_error_context(void);

/**
 * @brief Alias for nimcp_error_to_string for compatibility
 */
#define nimcp_error_string nimcp_error_to_string

//=============================================================================
// Additional Convenience Macros
//=============================================================================

/**
 * @brief Set error with automatic file/line/function info and formatted message
 *
 * Example:
 * @code
 * if (!ptr) {
 *     NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "Parameter '%s' is NULL", "brain");
 *     return NIMCP_ERROR_NULL_POINTER;
 * }
 * @endcode
 */
#define NIMCP_ERROR_SET(code, fmt, ...) \
    nimcp_set_error_ex((code), __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * @brief Return error code after setting error context with formatted message
 *
 * Example:
 * @code
 * if (!ptr) {
 *     NIMCP_ERROR_RETURN(NIMCP_ERROR_NULL_POINTER, "ptr is NULL");
 * }
 * @endcode
 */
#define NIMCP_ERROR_RETURN(code, fmt, ...) \
    do { \
        NIMCP_ERROR_SET(code, fmt, ##__VA_ARGS__); \
        return (code); \
    } while (0)

/**
 * @brief Check condition and return error if false with formatted message
 *
 * Example:
 * @code
 * NIMCP_ERROR_CHECK(ptr != NULL, NIMCP_ERROR_NULL_POINTER, "ptr is NULL");
 * NIMCP_ERROR_CHECK(size > 0, NIMCP_ERROR_INVALID_PARAMETER, "size=%zu must be > 0", size);
 * @endcode
 */
#define NIMCP_ERROR_CHECK(cond, code, fmt, ...) \
    do { \
        if (!(cond)) { \
            NIMCP_ERROR_RETURN(code, fmt, ##__VA_ARGS__); \
        } \
    } while (0)

//=============================================================================
// NULL Pointer Validation Macros (Standard Pattern)
//=============================================================================

/**
 * @brief Check for NULL pointer and return NIMCP_ERROR_NULL_POINTER
 *
 * Standard pattern for NULL pointer validation across the codebase.
 * Uses early return with LOG_ERROR for consistent error reporting.
 *
 * @param ptr Pointer to check
 * @param msg Message describing the parameter
 *
 * Example:
 * @code
 * nimcp_error_t my_function(const config_t* config, adapter_t* adapter) {
 *     NIMCP_CHECK_NULL(config, "config");
 *     NIMCP_CHECK_NULL(adapter, "adapter");
 *     // ... function body ...
 *     return NIMCP_SUCCESS;
 * }
 * @endcode
 */
#define NIMCP_CHECK_NULL(ptr, msg) \
    do { \
        if (!(ptr)) { \
            NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: %s", msg); \
            return NIMCP_ERROR_NULL_POINTER; \
        } \
    } while (0)

/**
 * @brief Check for NULL pointer and return false (for bool-returning functions)
 *
 * @param ptr Pointer to check
 * @param msg Message describing the parameter
 *
 * Example:
 * @code
 * bool motor_reset(motor_adapter_t* adapter) {
 *     NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
 *     // ... function body ...
 *     return true;
 * }
 * @endcode
 */
#define NIMCP_CHECK_NULL_BOOL(ptr, msg) \
    do { \
        if (!(ptr)) { \
            NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: %s", msg); \
            return false; \
        } \
    } while (0)

/**
 * @brief Check for NULL pointer and return NULL (for pointer-returning functions)
 *
 * @param ptr Pointer to check
 * @param msg Message describing the parameter
 *
 * Example:
 * @code
 * adapter_t* adapter_create(const config_t* config) {
 *     NIMCP_CHECK_NULL_PTR(config, "config");
 *     // ... function body ...
 *     return adapter;
 * }
 * @endcode
 */
#define NIMCP_CHECK_NULL_PTR(ptr, msg) \
    do { \
        if (!(ptr)) { \
            NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: %s", msg); \
            return NULL; \
        } \
    } while (0)

/**
 * @brief Check for NULL pointer and return void (for void-returning functions)
 *
 * @param ptr Pointer to check
 * @param msg Message describing the parameter
 *
 * Example:
 * @code
 * void adapter_destroy(adapter_t* adapter) {
 *     NIMCP_CHECK_NULL_VOID(adapter, "adapter");
 *     // ... cleanup code ...
 * }
 * @endcode
 */
#define NIMCP_CHECK_NULL_VOID(ptr, msg) \
    do { \
        if (!(ptr)) { \
            NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: %s", msg); \
            return; \
        } \
    } while (0)

/**
 * @brief Check array bounds and return error if out of range
 *
 * @param index Index to check
 * @param max Maximum valid index (exclusive)
 * @param msg Message describing the parameter
 *
 * Example:
 * @code
 * nimcp_error_t get_item(adapter_t* adapter, uint32_t index) {
 *     NIMCP_CHECK_BOUNDS(index, adapter->count, "index");
 *     // ... function body ...
 *     return NIMCP_SUCCESS;
 * }
 * @endcode
 */
#define NIMCP_CHECK_BOUNDS(index, max, msg) \
    do { \
        if ((index) >= (max)) { \
            NIMCP_ERROR_SET(NIMCP_ERROR_OUT_OF_RANGE, "Out of bounds: %s (%u >= %u)", \
                           msg, (unsigned)(index), (unsigned)(max)); \
            return NIMCP_ERROR_OUT_OF_RANGE; \
        } \
    } while (0)

/**
 * @brief Check array bounds and return NULL if out of range
 *
 * @param index Index to check
 * @param max Maximum valid index (exclusive)
 * @param msg Message describing the parameter
 */
#define NIMCP_CHECK_BOUNDS_PTR(index, max, msg) \
    do { \
        if ((index) >= (max)) { \
            NIMCP_ERROR_SET(NIMCP_ERROR_OUT_OF_RANGE, "Out of bounds: %s (%u >= %u)", \
                           msg, (unsigned)(index), (unsigned)(max)); \
            return NULL; \
        } \
    } while (0)

//=============================================================================
// Legacy FEP Bridge Compatibility
//=============================================================================

/**
 * @brief Check if error code indicates success (alias for nimcp_error_is_success)
 */
static inline bool nimcp_is_ok(nimcp_error_t code) {
    return (code >= 0 && code < 1000);
}

/**
 * @brief Check if error code indicates failure (alias for nimcp_error_is_failure)
 */
static inline bool nimcp_is_error(nimcp_error_t code) {
    return (code >= 1000);
}

/**
 * @brief Convert FEP bridge return value to nimcp_error_t
 *
 * FEP bridges use 0 for success, -1 for error. This converts to nimcp_error_t.
 *
 * @param fep_result FEP bridge return value (0 or -1)
 * @return NIMCP_SUCCESS if fep_result == 0, NIMCP_ERROR_OPERATION_FAILED otherwise
 */
static inline nimcp_error_t nimcp_from_fep_result(int fep_result) {
    return (fep_result == 0) ? NIMCP_SUCCESS : NIMCP_ERROR_OPERATION_FAILED;
}

/**
 * @brief Convert nimcp_error_t to FEP bridge return value
 *
 * @param error NIMCP error code
 * @return 0 if success, -1 if error
 */
static inline int nimcp_to_fep_result(nimcp_error_t error) {
    return nimcp_is_ok(error) ? 0 : -1;
}

//=============================================================================
// Result Type Pattern (Optional)
//=============================================================================

/**
 * @brief Generic result type macro for functions that return values
 *
 * This macro generates a result struct type for a given value type.
 * The struct contains either the value on success, or an error code on failure.
 *
 * Example:
 * @code
 * // Define result type for float
 * NIMCP_DEFINE_RESULT(float, FloatResult);
 *
 * // Function returning result
 * nimcp_FloatResult_t compute_value(int input) {
 *     if (input < 0) {
 *         return NIMCP_RESULT_ERR(FloatResult, NIMCP_ERROR_INVALID_PARAMETER);
 *     }
 *     return NIMCP_RESULT_OK(FloatResult, (float)input * 2.0f);
 * }
 *
 * // Usage
 * nimcp_FloatResult_t result = compute_value(5);
 * if (result.is_ok) {
 *     printf("Value: %f\n", result.value);
 * } else {
 *     printf("Error: %s\n", nimcp_error_string(result.error));
 * }
 * @endcode
 */
#define NIMCP_DEFINE_RESULT(type, name) \
    typedef struct nimcp_##name { \
        bool is_ok; \
        union { \
            type value; \
            nimcp_error_t error; \
        }; \
    } nimcp_##name##_t

/**
 * @brief Create a success result
 */
#define NIMCP_RESULT_OK(name, val) \
    ((nimcp_##name##_t){.is_ok = true, .value = (val)})

/**
 * @brief Create an error result
 */
#define NIMCP_RESULT_ERR(name, err) \
    ((nimcp_##name##_t){.is_ok = false, .error = (err)})

//=============================================================================
// Brain Region Error Conversion API
//
// These functions convert module-local error codes to unified NIMCP error codes.
// Use these at module boundaries to provide consistent error reporting.
//
// Each brain region has its own error_t type (e.g., motor_error_t) that maps
// directly to the unified range: module_error_value -> NIMCP_ERROR_MODULE_BASE + value
//=============================================================================

/**
 * @brief Convert motor_error_t to nimcp_error_t
 *
 * @param err Motor module error code (motor_error_t enum value)
 * @return Corresponding NIMCP error code
 *
 * Example:
 * @code
 * motor_error_t local_err = motor_get_last_error(adapter);
 * nimcp_error_t unified = motor_error_to_nimcp(local_err);
 * @endcode
 */
static inline nimcp_error_t motor_error_to_nimcp(int err) {
    if (err == 0) return NIMCP_SUCCESS;
    return NIMCP_ERROR_MOTOR_BASE + err;
}

/**
 * @brief Convert nimcp_error_t to motor_error_t
 *
 * @param err NIMCP error code (must be in motor range or NIMCP_SUCCESS)
 * @return Corresponding motor module error code
 */
static inline int nimcp_to_motor_error(nimcp_error_t err) {
    if (err == NIMCP_SUCCESS) return 0;
    if (err >= NIMCP_ERROR_MOTOR_BASE && err < NIMCP_ERROR_MOTOR_BASE + 100) {
        return err - NIMCP_ERROR_MOTOR_BASE;
    }
    return 8; /* MOTOR_ERROR_INTERNAL as default */
}

/**
 * @brief Convert hippocampus_error_t to nimcp_error_t
 */
static inline nimcp_error_t hippocampus_error_to_nimcp(int err) {
    if (err == 0) return NIMCP_SUCCESS;
    return NIMCP_ERROR_HIPPOCAMPUS_BASE + err;
}

/**
 * @brief Convert nimcp_error_t to hippocampus_error_t
 */
static inline int nimcp_to_hippocampus_error(nimcp_error_t err) {
    if (err == NIMCP_SUCCESS) return 0;
    if (err >= NIMCP_ERROR_HIPPOCAMPUS_BASE && err < NIMCP_ERROR_HIPPOCAMPUS_BASE + 100) {
        return err - NIMCP_ERROR_HIPPOCAMPUS_BASE;
    }
    return 9; /* HIPPOCAMPUS_ERROR_INTERNAL as default */
}

/**
 * @brief Convert entorhinal_error_t to nimcp_error_t
 */
static inline nimcp_error_t entorhinal_error_to_nimcp(int err) {
    if (err == 0) return NIMCP_SUCCESS;
    return NIMCP_ERROR_ENTORHINAL_BASE + err;
}

/**
 * @brief Convert nimcp_error_t to entorhinal_error_t
 */
static inline int nimcp_to_entorhinal_error(nimcp_error_t err) {
    if (err == NIMCP_SUCCESS) return 0;
    if (err >= NIMCP_ERROR_ENTORHINAL_BASE && err < NIMCP_ERROR_ENTORHINAL_BASE + 100) {
        return err - NIMCP_ERROR_ENTORHINAL_BASE;
    }
    return 10; /* ENTORHINAL_ERROR_INTERNAL as default */
}

/**
 * @brief Check if error code is in a brain region range
 *
 * @param err NIMCP error code
 * @return true if error is from a brain region module (10000-19999)
 */
static inline bool nimcp_error_is_brain_region(nimcp_error_t err) {
    return (err >= 10000 && err < 20000);
}

/**
 * @brief Get brain region name from error code
 *
 * @param err NIMCP error code
 * @return Brain region name string, or "Unknown" if not a brain region error
 */
const char* nimcp_error_get_brain_region_name(nimcp_error_t err);

/**
 * @brief Generic macro for defining module error converters
 *
 * Usage:
 * @code
 * // Define converters for a new module
 * NIMCP_DEFINE_ERROR_CONVERTER(prefrontal, PREFRONTAL, 5)
 *
 * // Generates:
 * // - prefrontal_error_to_nimcp(int err) -> nimcp_error_t
 * // - nimcp_to_prefrontal_error(nimcp_error_t err) -> int
 * @endcode
 */
#define NIMCP_DEFINE_ERROR_CONVERTER(name, NAME, internal_max) \
    static inline nimcp_error_t name##_error_to_nimcp(int err) { \
        if (err == 0) return NIMCP_SUCCESS; \
        return NIMCP_ERROR_##NAME##_BASE + err; \
    } \
    static inline int nimcp_to_##name##_error(nimcp_error_t err) { \
        if (err == NIMCP_SUCCESS) return 0; \
        if (err >= NIMCP_ERROR_##NAME##_BASE && err < NIMCP_ERROR_##NAME##_BASE + 100) { \
            return err - NIMCP_ERROR_##NAME##_BASE; \
        } \
        return internal_max; \
    }

/* Generate converters for remaining brain regions */
NIMCP_DEFINE_ERROR_CONVERTER(prefrontal, PREFRONTAL, 5)
NIMCP_DEFINE_ERROR_CONVERTER(cerebellum, CEREBELLUM, 4)
NIMCP_DEFINE_ERROR_CONVERTER(thalamus, THALAMUS, 4)
NIMCP_DEFINE_ERROR_CONVERTER(hypothalamus, HYPOTHALAMUS, 4)
NIMCP_DEFINE_ERROR_CONVERTER(amygdala, AMYGDALA, 4)
NIMCP_DEFINE_ERROR_CONVERTER(basal_ganglia, BASAL_GANGLIA, 4)
NIMCP_DEFINE_ERROR_CONVERTER(cingulate, CINGULATE, 4)
NIMCP_DEFINE_ERROR_CONVERTER(insula, INSULA, 4)
NIMCP_DEFINE_ERROR_CONVERTER(occipital, OCCIPITAL, 4)
NIMCP_DEFINE_ERROR_CONVERTER(parietal, PARIETAL, 4)
NIMCP_DEFINE_ERROR_CONVERTER(temporal, TEMPORAL, 4)
NIMCP_DEFINE_ERROR_CONVERTER(broca, BROCA, 4)
NIMCP_DEFINE_ERROR_CONVERTER(wernicke, WERNICKE, 4)
NIMCP_DEFINE_ERROR_CONVERTER(brainstem, BRAINSTEM, 4)
NIMCP_DEFINE_ERROR_CONVERTER(parahippocampal, PARAHIPPOCAMPAL, 4)
NIMCP_DEFINE_ERROR_CONVERTER(perirhinal, PERIRHINAL, 4)
NIMCP_DEFINE_ERROR_CONVERTER(vta, VTA, 4)

#ifdef __cplusplus
}
#endif

#endif // NIMCP_ERROR_CODES_H
