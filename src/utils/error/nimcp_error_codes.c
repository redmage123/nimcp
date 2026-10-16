/**
 * @file nimcp_error_codes.c
 * @brief Error code system implementation
 *
 * @author NIMCP Team
 * @date 2025-11-09
 */

#include "utils/error/nimcp_error_codes.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(error_codes)

//=============================================================================
// Forward Declarations
//=============================================================================

// Forward declaration of extended error context API (defined below)
static nimcp_error_context_t* get_thread_error_context(void);

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static pthread_key_t g_error_key;
static pthread_once_t g_error_key_once = PTHREAD_ONCE_INIT;

static void error_destructor(void* value)
{
    if (value) {
        nimcp_free(value);
    }
}

static void create_error_key(void)
{
    pthread_key_create(&g_error_key, error_destructor);
}

static nimcp_error_info_t* get_thread_error(void)
{
    pthread_once(&g_error_key_once, create_error_key);

    nimcp_error_info_t* info = (nimcp_error_info_t*)pthread_getspecific(g_error_key);
    if (!info) {
        info = (nimcp_error_info_t*)nimcp_calloc(1, sizeof(nimcp_error_info_t));
        pthread_setspecific(g_error_key, info);
    }
    return info;
}

//=============================================================================
// Error Message Lookup
//=============================================================================

const char* nimcp_error_to_string(nimcp_error_t code)
{
    LOG_TRACE("Entering nimcp_error_to_string");
    switch (code) {
        // Success
        case NIMCP_SUCCESS: return "Success";
        case NIMCP_SUCCESS_WITH_WARNINGS: return "Success with warnings";
        case NIMCP_SUCCESS_PARTIAL: return "Partial success";

        // Generic errors
        case NIMCP_ERROR_UNKNOWN: return "Unknown error";
        case NIMCP_ERROR_NOT_IMPLEMENTED: return "Feature not implemented";
        case NIMCP_ERROR_INVALID_PARAMETER: return "Invalid parameter";
        case NIMCP_ERROR_NULL_POINTER: return "Null pointer";
        case NIMCP_ERROR_OUT_OF_RANGE: return "Value out of range";
        case NIMCP_ERROR_INVALID_STATE: return "Invalid state";
        case NIMCP_ERROR_OPERATION_FAILED: return "Operation failed";
        case NIMCP_ERROR_NOT_INITIALIZED: return "Not initialized";
        case NIMCP_ERROR_ALREADY_EXISTS: return "Already exists";
        case NIMCP_ERROR_NOT_FOUND: return "Not found";
        case NIMCP_ERROR_TIMEOUT: return "Timeout";
        case NIMCP_ERROR_CANCELLED: return "Cancelled";
        case NIMCP_ERROR_PERMISSION_DENIED: return "Permission denied";

        // Memory errors
        case NIMCP_ERROR_NO_MEMORY: return "Out of memory";
        case NIMCP_ERROR_BUFFER_TOO_SMALL: return "Buffer too small";
        case NIMCP_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case NIMCP_ERROR_MEMORY_CORRUPTION: return "Memory corruption detected";
        case NIMCP_ERROR_INVALID_ADDRESS: return "Invalid memory address";
        case NIMCP_ERROR_MEMORY_LEAK: return "Memory leak detected";
        case NIMCP_ERROR_DOUBLE_FREE: return "Double free detected";

        // Brain/Network errors
        case NIMCP_ERROR_BRAIN_CREATION: return "Brain creation failed";
        case NIMCP_ERROR_BRAIN_INVALID: return "Invalid brain";
        case NIMCP_ERROR_NETWORK_CREATION: return "Network creation failed";
        case NIMCP_ERROR_NETWORK_INVALID: return "Invalid network";
        case NIMCP_ERROR_DIMENSION_MISMATCH: return "Dimension mismatch";
        case NIMCP_ERROR_WEIGHT_INIT: return "Weight initialization failed";
        case NIMCP_ERROR_FORWARD_PASS: return "Forward pass failed";
        case NIMCP_ERROR_BACKWARD_PASS: return "Backward pass failed";
        case NIMCP_ERROR_LEARNING_FAILED: return "Learning failed";
        case NIMCP_ERROR_INFERENCE_FAILED: return "Inference failed";
        case NIMCP_ERROR_COW_FAILED: return "Copy-on-write failed";
        case NIMCP_ERROR_CLONE_FAILED: return "Clone failed";

        // I/O errors
        case NIMCP_ERROR_FILE_NOT_FOUND: return "File not found";
        case NIMCP_ERROR_FILE_READ: return "File read error";
        case NIMCP_ERROR_FILE_WRITE: return "File write error";
        case NIMCP_ERROR_FILE_OPEN: return "File open error";
        case NIMCP_ERROR_FILE_CLOSE: return "File close error";
        case NIMCP_ERROR_FILE_CORRUPT: return "File corrupted";
        case NIMCP_ERROR_SERIALIZATION: return "Serialization failed";
        case NIMCP_ERROR_DESERIALIZATION: return "Deserialization failed";
        case NIMCP_ERROR_NETWORK_IO: return "Network I/O error";
        case NIMCP_ERROR_SOCKET_ERROR: return "Socket error";

        // Config errors
        case NIMCP_ERROR_CONFIG_INVALID: return "Invalid configuration";
        case NIMCP_ERROR_CONFIG_PARSE: return "Config parse error";
        case NIMCP_ERROR_CONFIG_MISSING: return "Required config missing";
        case NIMCP_ERROR_CONFIG_TYPE: return "Config type mismatch";
        case NIMCP_ERROR_CONFIG_RANGE: return "Config value out of range";
        case NIMCP_ERROR_CONFIG_RELOAD: return "Config reload failed";

        // Threading errors
        case NIMCP_ERROR_THREAD_CREATE: return "Thread creation failed";
        case NIMCP_ERROR_THREAD_JOIN: return "Thread join failed";
        case NIMCP_ERROR_MUTEX_LOCK: return "Mutex lock failed";
        case NIMCP_ERROR_MUTEX_UNLOCK: return "Mutex unlock failed";
        case NIMCP_ERROR_MUTEX_INIT: return "Mutex init failed";
        case NIMCP_ERROR_DEADLOCK: return "Deadlock detected";
        case NIMCP_ERROR_RACE_CONDITION: return "Race condition detected";
        case NIMCP_ERROR_THREAD_SYNC: return "Thread synchronization failed";

        // Signal/Crash errors
        case NIMCP_ERROR_SIGNAL_RECEIVED: return "Signal received";
        case NIMCP_ERROR_SIGSEGV: return "Segmentation fault";
        case NIMCP_ERROR_SIGABRT: return "Abort signal";
        case NIMCP_ERROR_SIGFPE: return "Floating point exception";
        case NIMCP_ERROR_SIGBUS: return "Bus error";
        case NIMCP_ERROR_SIGILL: return "Illegal instruction";
        case NIMCP_ERROR_CRASH_RECOVERY: return "Crash recovery failed";
        case NIMCP_ERROR_CHECKPOINT_SAVE: return "Checkpoint save failed";
        case NIMCP_ERROR_CHECKPOINT_LOAD: return "Checkpoint load failed";

        // Phase 10 errors
        case NIMCP_ERROR_WORKING_MEMORY: return "Working memory error";
        case NIMCP_ERROR_EMOTIONAL_TAGGING: return "Emotional tagging error";
        case NIMCP_ERROR_EXECUTIVE_CONTROL: return "Executive control error";
        case NIMCP_ERROR_SLEEP_WAKE: return "Sleep/wake cycle error";
        case NIMCP_ERROR_MENTAL_HEALTH: return "Mental health monitor error";
        case NIMCP_ERROR_THEORY_OF_MIND: return "Theory of mind error";
        case NIMCP_ERROR_EXPLANATIONS: return "Natural explanations error";
        case NIMCP_ERROR_META_LEARNING: return "Meta-learning error";
        case NIMCP_ERROR_PREDICTIVE: return "Predictive processing error";

        // Motor cortex errors
        case NIMCP_ERROR_MOTOR_INVALID_INPUT: return "Motor: Invalid input";
        case NIMCP_ERROR_MOTOR_PLANNING: return "Motor: Planning failure";
        case NIMCP_ERROR_MOTOR_EXECUTION: return "Motor: Execution failure";
        case NIMCP_ERROR_MOTOR_TRAJECTORY: return "Motor: Trajectory infeasible";
        case NIMCP_ERROR_MOTOR_EFFECTOR: return "Motor: Effector conflict";
        case NIMCP_ERROR_MOTOR_TIMING: return "Motor: Timing violation";
        case NIMCP_ERROR_MOTOR_BUFFER: return "Motor: Buffer overflow";
        case NIMCP_ERROR_MOTOR_INTERNAL: return "Motor: Internal error";

        // Hippocampus errors
        case NIMCP_ERROR_HIPPOCAMPUS_INVALID_INPUT: return "Hippocampus: Invalid input";
        case NIMCP_ERROR_HIPPOCAMPUS_ENCODING: return "Hippocampus: Encoding failure";
        case NIMCP_ERROR_HIPPOCAMPUS_RETRIEVAL: return "Hippocampus: Retrieval failure";
        case NIMCP_ERROR_HIPPOCAMPUS_NAVIGATION: return "Hippocampus: Navigation failure";
        case NIMCP_ERROR_HIPPOCAMPUS_MEMORY_FULL: return "Hippocampus: Memory full";
        case NIMCP_ERROR_HIPPOCAMPUS_PATTERN_SEP: return "Hippocampus: Pattern separation failure";
        case NIMCP_ERROR_HIPPOCAMPUS_PATTERN_COMP: return "Hippocampus: Pattern completion failure";
        case NIMCP_ERROR_HIPPOCAMPUS_BUFFER: return "Hippocampus: Buffer overflow";
        case NIMCP_ERROR_HIPPOCAMPUS_INTERNAL: return "Hippocampus: Internal error";

        // Entorhinal cortex errors
        case NIMCP_ERROR_ENTORHINAL_INVALID_INPUT: return "Entorhinal: Invalid input";
        case NIMCP_ERROR_ENTORHINAL_GRID_DRIFT: return "Entorhinal: Grid drift";
        case NIMCP_ERROR_ENTORHINAL_PATH_INTEGRATION: return "Entorhinal: Path integration failure";
        case NIMCP_ERROR_ENTORHINAL_GATEWAY_BLOCKED: return "Entorhinal: Memory gateway blocked";
        case NIMCP_ERROR_ENTORHINAL_SECURITY: return "Entorhinal: Security violation";
        case NIMCP_ERROR_ENTORHINAL_IMMUNE: return "Entorhinal: Immune rejection";
        case NIMCP_ERROR_ENTORHINAL_SUBSTRATE: return "Entorhinal: Substrate depleted";
        case NIMCP_ERROR_ENTORHINAL_SYNC: return "Entorhinal: Sync failure";
        case NIMCP_ERROR_ENTORHINAL_BUFFER: return "Entorhinal: Buffer overflow";
        case NIMCP_ERROR_ENTORHINAL_INTERNAL: return "Entorhinal: Internal error";

        // Prefrontal cortex errors
        case NIMCP_ERROR_PREFRONTAL_INVALID_INPUT: return "Prefrontal: Invalid input";
        case NIMCP_ERROR_PREFRONTAL_PLANNING: return "Prefrontal: Planning failure";
        case NIMCP_ERROR_PREFRONTAL_WORKING_MEMORY: return "Prefrontal: Working memory error";
        case NIMCP_ERROR_PREFRONTAL_INHIBITION: return "Prefrontal: Inhibition failure";
        case NIMCP_ERROR_PREFRONTAL_INTERNAL: return "Prefrontal: Internal error";

        // Cerebellum errors
        case NIMCP_ERROR_CEREBELLUM_INVALID_INPUT: return "Cerebellum: Invalid input";
        case NIMCP_ERROR_CEREBELLUM_TIMING: return "Cerebellum: Timing error";
        case NIMCP_ERROR_CEREBELLUM_PREDICTION: return "Cerebellum: Prediction error";
        case NIMCP_ERROR_CEREBELLUM_INTERNAL: return "Cerebellum: Internal error";

        // Thalamus errors
        case NIMCP_ERROR_THALAMUS_INVALID_INPUT: return "Thalamus: Invalid input";
        case NIMCP_ERROR_THALAMUS_RELAY: return "Thalamus: Relay failure";
        case NIMCP_ERROR_THALAMUS_GATING: return "Thalamus: Gating error";
        case NIMCP_ERROR_THALAMUS_INTERNAL: return "Thalamus: Internal error";

        // Hypothalamus errors
        case NIMCP_ERROR_HYPOTHALAMUS_INVALID_INPUT: return "Hypothalamus: Invalid input";
        case NIMCP_ERROR_HYPOTHALAMUS_HOMEOSTASIS: return "Hypothalamus: Homeostasis error";
        case NIMCP_ERROR_HYPOTHALAMUS_MOTIVATION: return "Hypothalamus: Motivation error";
        case NIMCP_ERROR_HYPOTHALAMUS_INTERNAL: return "Hypothalamus: Internal error";

        // Amygdala errors
        case NIMCP_ERROR_AMYGDALA_INVALID_INPUT: return "Amygdala: Invalid input";
        case NIMCP_ERROR_AMYGDALA_FEAR_PROCESSING: return "Amygdala: Fear processing error";
        case NIMCP_ERROR_AMYGDALA_EMOTIONAL_TAG: return "Amygdala: Emotional tagging error";
        case NIMCP_ERROR_AMYGDALA_INTERNAL: return "Amygdala: Internal error";

        // Basal ganglia errors
        case NIMCP_ERROR_BASAL_GANGLIA_INVALID_INPUT: return "Basal Ganglia: Invalid input";
        case NIMCP_ERROR_BASAL_GANGLIA_ACTION_SELECT: return "Basal Ganglia: Action selection error";
        case NIMCP_ERROR_BASAL_GANGLIA_DOPAMINE: return "Basal Ganglia: Dopamine error";
        case NIMCP_ERROR_BASAL_GANGLIA_INTERNAL: return "Basal Ganglia: Internal error";

        // Cingulate cortex errors
        case NIMCP_ERROR_CINGULATE_INVALID_INPUT: return "Cingulate: Invalid input";
        case NIMCP_ERROR_CINGULATE_CONFLICT: return "Cingulate: Conflict error";
        case NIMCP_ERROR_CINGULATE_ERROR_MONITORING: return "Cingulate: Error monitoring failure";
        case NIMCP_ERROR_CINGULATE_INTERNAL: return "Cingulate: Internal error";

        // Insula errors
        case NIMCP_ERROR_INSULA_INVALID_INPUT: return "Insula: Invalid input";
        case NIMCP_ERROR_INSULA_INTEROCEPTION: return "Insula: Interoception error";
        case NIMCP_ERROR_INSULA_AWARENESS: return "Insula: Awareness error";
        case NIMCP_ERROR_INSULA_INTERNAL: return "Insula: Internal error";

        // Occipital lobe errors
        case NIMCP_ERROR_OCCIPITAL_INVALID_INPUT: return "Occipital: Invalid input";
        case NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING: return "Occipital: Visual processing error";
        case NIMCP_ERROR_OCCIPITAL_FEATURE_EXTRACTION: return "Occipital: Feature extraction error";
        case NIMCP_ERROR_OCCIPITAL_INTERNAL: return "Occipital: Internal error";

        // Parietal lobe errors
        case NIMCP_ERROR_PARIETAL_INVALID_INPUT: return "Parietal: Invalid input";
        case NIMCP_ERROR_PARIETAL_SPATIAL: return "Parietal: Spatial processing error";
        case NIMCP_ERROR_PARIETAL_ATTENTION: return "Parietal: Attention error";
        case NIMCP_ERROR_PARIETAL_INTERNAL: return "Parietal: Internal error";

        // Temporal lobe errors
        case NIMCP_ERROR_TEMPORAL_INVALID_INPUT: return "Temporal: Invalid input";
        case NIMCP_ERROR_TEMPORAL_AUDITORY: return "Temporal: Auditory processing error";
        case NIMCP_ERROR_TEMPORAL_SEMANTIC: return "Temporal: Semantic error";
        case NIMCP_ERROR_TEMPORAL_INTERNAL: return "Temporal: Internal error";

        // Broca's area errors
        case NIMCP_ERROR_BROCA_INVALID_INPUT: return "Broca: Invalid input";
        case NIMCP_ERROR_BROCA_PRODUCTION: return "Broca: Language production error";
        case NIMCP_ERROR_BROCA_SYNTAX: return "Broca: Syntax error";
        case NIMCP_ERROR_BROCA_INTERNAL: return "Broca: Internal error";

        // Wernicke's area errors
        case NIMCP_ERROR_WERNICKE_INVALID_INPUT: return "Wernicke: Invalid input";
        case NIMCP_ERROR_WERNICKE_COMPREHENSION: return "Wernicke: Comprehension error";
        case NIMCP_ERROR_WERNICKE_SEMANTIC: return "Wernicke: Semantic error";
        case NIMCP_ERROR_WERNICKE_INTERNAL: return "Wernicke: Internal error";

        // Brainstem errors
        case NIMCP_ERROR_BRAINSTEM_INVALID_INPUT: return "Brainstem: Invalid input";
        case NIMCP_ERROR_BRAINSTEM_VITAL_FUNCTION: return "Brainstem: Vital function error";
        case NIMCP_ERROR_BRAINSTEM_AROUSAL: return "Brainstem: Arousal error";
        case NIMCP_ERROR_BRAINSTEM_INTERNAL: return "Brainstem: Internal error";

        // Parahippocampal region errors
        case NIMCP_ERROR_PARAHIPPOCAMPAL_INVALID_INPUT: return "Parahippocampal: Invalid input";
        case NIMCP_ERROR_PARAHIPPOCAMPAL_SCENE: return "Parahippocampal: Scene processing error";
        case NIMCP_ERROR_PARAHIPPOCAMPAL_CONTEXT: return "Parahippocampal: Context error";
        case NIMCP_ERROR_PARAHIPPOCAMPAL_INTERNAL: return "Parahippocampal: Internal error";

        // Perirhinal cortex errors
        case NIMCP_ERROR_PERIRHINAL_INVALID_INPUT: return "Perirhinal: Invalid input";
        case NIMCP_ERROR_PERIRHINAL_OBJECT_RECOGNITION: return "Perirhinal: Object recognition error";
        case NIMCP_ERROR_PERIRHINAL_FAMILIARITY: return "Perirhinal: Familiarity error";
        case NIMCP_ERROR_PERIRHINAL_INTERNAL: return "Perirhinal: Internal error";

        // VTA errors
        case NIMCP_ERROR_VTA_INVALID_INPUT: return "VTA: Invalid input";
        case NIMCP_ERROR_VTA_REWARD: return "VTA: Reward error";
        case NIMCP_ERROR_VTA_DOPAMINE: return "VTA: Dopamine error";
        case NIMCP_ERROR_VTA_INTERNAL: return "VTA: Internal error";

        default: return "Unknown error code";
    }
}

const char* nimcp_error_get_category_name(nimcp_error_t code)
{
    LOG_TRACE("Entering nimcp_error_get_category_name");
    int category = nimcp_error_get_category(code);

    switch (category) {
        case 0: return "Success";
        case 1: return "Generic Error";
        case 2: return "Memory Error";
        case 3: return "Brain/Network Error";
        case 4: return "I/O Error";
        case 5: return "Configuration Error";
        case 6: return "Threading Error";
        case 7: return "Signal/Crash Error";
        case 8: return "Cognitive Error";
        case 10: return "Motor Cortex Error";
        case 11: return "Brain Region Error";
        default:
            if (category >= 10 && category < 20) {
                return "Brain Region Error";
            }
            return "Unknown Category";
    }
}

const char* nimcp_error_get_brain_region_name(nimcp_error_t err)
{
    if (err < 10000 || err >= 20000) {
        return "Unknown";
    }

    int region_code = (err - 10000) / 100;

    switch (region_code) {
        case 0: return "Motor Cortex";
        case 1: return "Hippocampus";
        case 2: return "Entorhinal Cortex";
        case 3: return "Prefrontal Cortex";
        case 4: return "Cerebellum";
        case 5: return "Thalamus";
        case 6: return "Hypothalamus";
        case 7: return "Amygdala";
        case 8: return "Basal Ganglia";
        case 9: return "Cingulate Cortex";
        case 10: return "Insula";
        case 11: return "Occipital Lobe";
        case 12: return "Parietal Lobe";
        case 13: return "Temporal Lobe";
        case 14: return "Broca's Area";
        case 15: return "Wernicke's Area";
        case 16: return "Brainstem";
        case 17: return "Parahippocampal Region";
        case 18: return "Perirhinal Cortex";
        case 19: return "VTA";
        default: return "Unknown Brain Region";
    }
}

void nimcp_error_set(nimcp_error_t code, const char* file, int line,
                     const char* function, const char* message)
{
    nimcp_error_info_t* info = get_thread_error();
    if (!info) return;

    info->code = code;
    info->file = file;
    info->line = line;
    info->function = function;
    info->message = message ? message : nimcp_error_to_string(code);
    info->context = NULL;
}

const nimcp_error_info_t* nimcp_error_get_last(void)
{
    LOG_TRACE("Entering nimcp_error_get_last");
    return get_thread_error();
}

void nimcp_error_clear(void)
{
    LOG_TRACE("Entering nimcp_error_clear");
    nimcp_error_info_t* info = get_thread_error();
    if (info) {
        memset(info, 0, sizeof(nimcp_error_info_t));
    }

    // Also clear the extended error context (used by nimcp_set_error_ex)
    nimcp_error_context_t* ctx = get_thread_error_context();
    if (ctx) {
        ctx->code = NIMCP_SUCCESS;
        ctx->file = NULL;
        ctx->line = 0;
        ctx->function = NULL;
        ctx->timestamp = 0;
        ctx->message[0] = '\0';
    }
}

void nimcp_error_print(nimcp_error_t code)
{
    LOG_TRACE("Entering nimcp_error_print");
    fprintf(stderr, "[ERROR %d] %s: %s\n",
            code,
            nimcp_error_get_category_name(code),
            nimcp_error_to_string(code));
}

void nimcp_error_print_detailed(const nimcp_error_info_t* info)
{
    LOG_TRACE("Entering nimcp_error_print_detailed");
    if (!info) return;

    fprintf(stderr, "\n=== ERROR DETAILS ===\n");
    fprintf(stderr, "Code: %d\n", info->code);
    fprintf(stderr, "Category: %s\n", nimcp_error_get_category_name(info->code));
    fprintf(stderr, "Message: %s\n", info->message ? info->message : "None");
    if (info->file) {
        fprintf(stderr, "File: %s:%d\n", info->file, info->line);
    }
    if (info->function) {
        fprintf(stderr, "Function: %s\n", info->function);
    }
    fprintf(stderr, "====================\n");
}

//=============================================================================
// Extended Error Context API (Thread-Local with Formatted Messages)
//=============================================================================

// Thread-local storage for error context with formatted message buffer
static pthread_key_t g_error_context_key;
static pthread_once_t g_error_context_key_once = PTHREAD_ONCE_INIT;

static void error_context_destructor(void* value)
{
    if (value) {
        nimcp_free(value);
    }
}

static void create_error_context_key(void)
{
    pthread_key_create(&g_error_context_key, error_context_destructor);
}

static nimcp_error_context_t* get_thread_error_context(void)
{
    pthread_once(&g_error_context_key_once, create_error_context_key);

    nimcp_error_context_t* ctx = (nimcp_error_context_t*)pthread_getspecific(g_error_context_key);
    if (!ctx) {
        ctx = (nimcp_error_context_t*)nimcp_calloc(1, sizeof(nimcp_error_context_t));
        pthread_setspecific(g_error_context_key, ctx);
    }
    return ctx;
}

void nimcp_set_error(nimcp_error_t code, const char* format, ...)
{
    nimcp_error_context_t* ctx = get_thread_error_context();
    if (!ctx) return;

    ctx->code = code;
    ctx->file = NULL;
    ctx->line = 0;
    ctx->function = NULL;
    ctx->timestamp = 0; // Could add timestamp if needed

    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(ctx->message, NIMCP_ERROR_MSG_MAX, format, args);
        va_end(args);
    } else {
        snprintf(ctx->message, NIMCP_ERROR_MSG_MAX, "%s", nimcp_error_to_string(code));
    }
}

void nimcp_set_error_ex(nimcp_error_t code, const char* file, int line,
                        const char* func, const char* format, ...)
{
    nimcp_error_context_t* ctx = get_thread_error_context();
    if (!ctx) return;

    ctx->code = code;
    ctx->file = file;
    ctx->line = line;
    ctx->function = func;
    ctx->timestamp = 0; // Could add timestamp if needed

    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(ctx->message, NIMCP_ERROR_MSG_MAX, format, args);
        va_end(args);
    } else {
        snprintf(ctx->message, NIMCP_ERROR_MSG_MAX, "%s", nimcp_error_to_string(code));
    }

    // Also set the legacy error info for backward compatibility
    nimcp_error_set(code, file, line, func, ctx->message);
}

nimcp_error_t nimcp_get_last_error(void)
{
    nimcp_error_context_t* ctx = get_thread_error_context();
    return ctx ? ctx->code : NIMCP_SUCCESS;
}

const char* nimcp_get_error_message(void)
{
    nimcp_error_context_t* ctx = get_thread_error_context();
    if (!ctx || ctx->message[0] == '\0') {
        return "";
    }
    return ctx->message;
}

const nimcp_error_context_t* nimcp_get_error_context(void)
{
    return get_thread_error_context();
}
