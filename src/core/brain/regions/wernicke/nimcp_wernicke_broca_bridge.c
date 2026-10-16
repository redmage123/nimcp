/**
 * @file nimcp_wernicke_broca_bridge.c
 * @brief Arcuate fasciculus bridge implementation
 *
 * Implements bidirectional communication between Wernicke's area (comprehension)
 * and Broca's area (production) via the arcuate fasciculus fiber tract.
 *
 * @version Phase W3: Wernicke's Area Bridges
 * @date 2026-01-04
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_broca_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(wernicke_broca_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "WERNICKE_BROCA_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Message queue node
 */
typedef struct wbb_queue_node {
    wbb_message_t message;
    struct wbb_queue_node* next;
} wbb_queue_node_t;

/**
 * @brief Message queue
 */
typedef struct {
    wbb_queue_node_t* head;
    wbb_queue_node_t* tail;
    uint32_t count;
    uint32_t max_size;
} wbb_queue_t;

/**
 * @brief Bridge internal state
 */
struct wernicke_broca_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    wbb_config_t config;

    /* Endpoint connections */
    void* wernicke;                  /**< Wernicke adapter handle */
    void* broca;                     /**< Broca adapter handle */
    void* bio_router;                /**< Bio-async router */

    /* Message queues */
    wbb_queue_t to_broca_queue;      /**< Messages to Broca */
    wbb_queue_t to_wernicke_queue;   /**< Messages to Wernicke (efference) */

    /* Working buffers */
    float* semantic_buffer;          /**< Semantic vector buffer */
    uint8_t* phoneme_buffer;         /**< Phoneme sequence buffer */

    /* Last comprehension for monitoring */
    wbb_comprehension_t last_comprehension;
    bool has_last_comprehension;

    /* Last efference copy for comparison */
    wbb_efference_copy_t last_efference;
    bool has_last_efference;

    /* Statistics */
    wbb_stats_t stats;

    /* Sequence numbering */
    uint32_t next_sequence;

    /* State */
    bool initialized;
};

/*=============================================================================
 * QUEUE HELPERS
 *===========================================================================*/

static void queue_init(wbb_queue_t* queue, uint32_t max_size) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    queue->max_size = max_size;
}

static void queue_destroy(wbb_queue_t* queue) {
    wbb_queue_node_t* node = queue->head;
    while (node) {
        wbb_queue_node_t* next = node->next;
        /* Free payload buffers if present */
        if (node->message.type == WBB_MSG_COMPREHENSION) {
            nimcp_free(node->message.payload.comprehension.semantic_vector);
            nimcp_free(node->message.payload.comprehension.phonemes);
        } else if (node->message.type == WBB_MSG_EFFERENCE_COPY) {
            nimcp_free(node->message.payload.efference.planned_phonemes);
            nimcp_free(node->message.payload.efference.motor_plan);
        } else if (node->message.type == WBB_MSG_REHEARSAL) {
            nimcp_free(node->message.payload.rehearsal.phonemes);
        }
        nimcp_free(node);
        node = next;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
}

static bool queue_enqueue(wbb_queue_t* queue, const wbb_message_t* message) {
    if (queue->count >= queue->max_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "queue_enqueue: capacity exceeded");
        return false;  /* Queue full */
    }

    wbb_queue_node_t* node = nimcp_calloc(1, sizeof(wbb_queue_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "queue_enqueue: node is NULL");
        return false;
    }

    node->message = *message;
    node->next = NULL;

    if (queue->tail) {
        queue->tail->next = node;
    } else {
        queue->head = node;
    }
    queue->tail = node;
    queue->count++;

    return true;
}

static bool queue_dequeue(wbb_queue_t* queue, wbb_message_t* message) {
    if (!queue->head) {
        return false;  /* Queue empty — normal condition */
    }

    wbb_queue_node_t* node = queue->head;
    *message = node->message;

    queue->head = node->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->count--;

    nimcp_free(node);
    return true;
}

static bool queue_peek(const wbb_queue_t* queue, wbb_message_t* message) {
    if (!queue->head) {
        return false;  /* Queue empty — normal condition */
    }
    *message = queue->head->message;
    return true;
}

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    /* Simple timestamp - could be replaced with actual clock */
    static _Atomic uint64_t counter = 0;
    return counter++;
}

/**
 * @brief Compute phoneme sequence similarity
 */
static float compute_phoneme_similarity(
    const uint8_t* seq1, uint32_t len1,
    const uint8_t* seq2, uint32_t len2
) {
    if (!seq1 || !seq2 || len1 == 0 || len2 == 0) {
        return 0.0f;
    }

    /* Simple match ratio */
    uint32_t matches = 0;
    uint32_t min_len = (len1 < len2) ? len1 : len2;
    uint32_t max_len = (len1 > len2) ? len1 : len2;

    for (uint32_t i = 0; i < min_len; i++) {
        if (seq1[i] == seq2[i]) {
            matches++;
        }
    }

    return (float)matches / (float)max_len;
}

/**
 * @brief Compute semantic vector similarity (cosine)
 */
static float compute_semantic_similarity(
    const float* vec1, const float* vec2, uint32_t dim
) {
    if (!vec1 || !vec2 || dim == 0) {
        return 0.0f;
    }

    float dot = 0.0f, norm1 = 0.0f, norm2 = 0.0f;

    for (uint32_t i = 0; i < dim; i++) {
        dot += vec1[i] * vec2[i];
        norm1 += vec1[i] * vec1[i];
        norm2 += vec2[i] * vec2[i];
    }

    if (norm1 < 1e-10f || norm2 < 1e-10f) {
        return 0.0f;
    }

    return dot / (sqrtf(norm1) * sqrtf(norm2));
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

wbb_config_t wbb_default_config(void) {
    return (wbb_config_t){
        .buffer_size = WBB_DEFAULT_BUFFER_SIZE,
        .phoneme_buffer_size = WBB_DEFAULT_PHONEME_BUFFER,
        .semantic_dim = WBB_DEFAULT_SEMANTIC_DIM,
        .transmission_delay_ms = WBB_DEFAULT_TRANSMISSION_DELAY_MS,
        .dorsal_delay_ms = 30.0f,
        .ventral_delay_ms = 70.0f,
        .repetition_threshold = WBB_DEFAULT_REPETITION_THRESHOLD,
        .monitoring_threshold = WBB_DEFAULT_MONITORING_THRESHOLD,
        .error_detection_threshold = 0.3f,
        .enable_dorsal_stream = true,
        .enable_ventral_stream = true,
        .enable_self_monitoring = true,
        .enable_working_memory = true,
        .enable_error_correction = true,
        .enable_bio_async = false
    };
}

wernicke_broca_bridge_t* wbb_create(
    void* wernicke,
    void* broca,
    const wbb_config_t* config
) {
    /* Wernicke adapter is required for arcuate fasciculus bridge */
    if (!wernicke) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke is NULL");

        return NULL;
    }

    wernicke_broca_bridge_t* bridge = nimcp_calloc(1, sizeof(wernicke_broca_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = wbb_default_config();
    }

    /* Store endpoints */
    bridge->wernicke = wernicke;
    bridge->broca = broca;

    /* Initialize queues */
    queue_init(&bridge->to_broca_queue, bridge->config.buffer_size);
    queue_init(&bridge->to_wernicke_queue, bridge->config.buffer_size);

    /* Allocate working buffers */
    bridge->semantic_buffer = nimcp_calloc(bridge->config.semantic_dim, sizeof(float));
    bridge->phoneme_buffer = nimcp_calloc(bridge->config.phoneme_buffer_size, sizeof(uint8_t));

    if (!bridge->semantic_buffer || !bridge->phoneme_buffer) {
        wbb_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wbb_create: required parameter is NULL (bridge->semantic_buffer, bridge->phoneme_buffer)");
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "wernicke_broca") != 0) {
        wbb_destroy(bridge);
        return NULL;
    }

    /* Initialize state */
    bridge->has_last_comprehension = false;
    bridge->has_last_efference = false;
    bridge->next_sequence = 1;
    bridge->initialized = true;

    return bridge;
}

void wbb_destroy(wernicke_broca_bridge_t* bridge) {
    if (!bridge) return;

    /* Cleanup bridge base */
    bridge_base_cleanup(&bridge->base);

    /* Destroy queues */
    queue_destroy(&bridge->to_broca_queue);
    queue_destroy(&bridge->to_wernicke_queue);

    /* Free buffers */
    nimcp_free(bridge->semantic_buffer);
    nimcp_free(bridge->phoneme_buffer);

    /* Free last comprehension payload */
    if (bridge->has_last_comprehension) {
        nimcp_free(bridge->last_comprehension.semantic_vector);
        nimcp_free(bridge->last_comprehension.phonemes);
    }

    /* Free last efference payload */
    if (bridge->has_last_efference) {
        nimcp_free(bridge->last_efference.planned_phonemes);
        nimcp_free(bridge->last_efference.motor_plan);
    }

    nimcp_free(bridge);
}

int wbb_reset(wernicke_broca_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_reset: bridge is NULL");
        return -1;
    }

    /* Clear queues */
    queue_destroy(&bridge->to_broca_queue);
    queue_destroy(&bridge->to_wernicke_queue);
    queue_init(&bridge->to_broca_queue, bridge->config.buffer_size);
    queue_init(&bridge->to_wernicke_queue, bridge->config.buffer_size);

    /* Clear last states */
    if (bridge->has_last_comprehension) {
        nimcp_free(bridge->last_comprehension.semantic_vector);
        nimcp_free(bridge->last_comprehension.phonemes);
        bridge->has_last_comprehension = false;
    }

    if (bridge->has_last_efference) {
        nimcp_free(bridge->last_efference.planned_phonemes);
        nimcp_free(bridge->last_efference.motor_plan);
        bridge->has_last_efference = false;
    }

    /* Reset sequence */
    bridge->next_sequence = 1;

    return 0;
}

/*=============================================================================
 * CONNECTION MANAGEMENT
 *===========================================================================*/

int wbb_connect_bio_async(
    wernicke_broca_bridge_t* bridge,
    void* router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_connect_bio_async: bridge is NULL");
        return -1;
    }
    bridge->bio_router = router;
    return 0;
}

int wbb_set_wernicke(
    wernicke_broca_bridge_t* bridge,
    void* wernicke
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_set_wernicke: bridge is NULL");
        return -1;
    }
    bridge->wernicke = wernicke;
    return 0;
}

int wbb_set_broca(
    wernicke_broca_bridge_t* bridge,
    void* broca
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_set_broca: bridge is NULL");
        return -1;
    }
    bridge->broca = broca;
    return 0;
}

/*=============================================================================
 * WERNICKE → BROCA (Comprehension to Production)
 *===========================================================================*/

int wbb_forward_comprehension(
    wernicke_broca_bridge_t* bridge,
    const wbb_comprehension_t* comprehension,
    wbb_stream_t stream
) {
    if (!bridge || !comprehension) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_forward_comprehension: required parameter is NULL (bridge, comprehension)");
        return -1;
    }

    /* Check stream availability */
    if (stream == WBB_STREAM_DORSAL && !bridge->config.enable_dorsal_stream) {
        return -1;  /* Dorsal stream disabled — not an error */
    }
    if (stream == WBB_STREAM_VENTRAL && !bridge->config.enable_ventral_stream) {
        return -1;  /* Ventral stream disabled — not an error */
    }

    /* Create message */
    wbb_message_t message = {0};
    message.type = WBB_MSG_COMPREHENSION;
    message.stream = stream;
    message.timestamp_ms = get_timestamp_ms();
    message.sequence_num = bridge->next_sequence++;

    /* Copy comprehension data */
    message.payload.comprehension = *comprehension;

    /* Deep copy semantic vector */
    if (comprehension->semantic_vector && comprehension->semantic_dim > 0) {
        message.payload.comprehension.semantic_vector =
            nimcp_malloc(comprehension->semantic_dim * sizeof(float));
        if (message.payload.comprehension.semantic_vector) {
            memcpy(message.payload.comprehension.semantic_vector,
                   comprehension->semantic_vector,
                   comprehension->semantic_dim * sizeof(float));
        }
    }

    /* Deep copy phonemes */
    if (comprehension->phonemes && comprehension->num_phonemes > 0) {
        message.payload.comprehension.phonemes =
            nimcp_malloc(comprehension->num_phonemes);
        if (message.payload.comprehension.phonemes) {
            memcpy(message.payload.comprehension.phonemes,
                   comprehension->phonemes,
                   comprehension->num_phonemes);
        }
    }

    /* Enqueue */
    if (!queue_enqueue(&bridge->to_broca_queue, &message)) {
        nimcp_free(message.payload.comprehension.semantic_vector);
        nimcp_free(message.payload.comprehension.phonemes);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wbb_forward_comprehension: queue_enqueue is NULL");
        return -1;
    }

    /* Store for monitoring */
    if (bridge->has_last_comprehension) {
        nimcp_free(bridge->last_comprehension.semantic_vector);
        nimcp_free(bridge->last_comprehension.phonemes);
    }
    bridge->last_comprehension = message.payload.comprehension;
    bridge->has_last_comprehension = true;

    /* Update stats */
    bridge->stats.messages_sent++;
    bridge->stats.comprehensions_forwarded++;

    return 0;
}

int wbb_request_repetition(
    wernicke_broca_bridge_t* bridge,
    const uint8_t* phonemes,
    uint32_t num_phonemes
) {
    if (!bridge || !phonemes || num_phonemes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_request_repetition: required parameter is NULL (bridge, phonemes)");
        return -1;
    }

    if (!bridge->config.enable_dorsal_stream) {
        return -1;  /* Dorsal stream disabled — required for repetition */
    }

    /* Create comprehension with phonemes only */
    wbb_comprehension_t comp = {0};
    comp.phonemes = (uint8_t*)nimcp_malloc(num_phonemes);
    if (!comp.phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wbb_request_repetition: comp is NULL");
        return -1;
    }
    memcpy(comp.phonemes, phonemes, num_phonemes);
    comp.num_phonemes = num_phonemes;
    comp.confidence = 1.0f;  /* Direct repetition request */

    /* Create message */
    wbb_message_t message = {0};
    message.type = WBB_MSG_REPETITION_REQUEST;
    message.stream = WBB_STREAM_DORSAL;
    message.timestamp_ms = get_timestamp_ms();
    message.sequence_num = bridge->next_sequence++;
    message.payload.comprehension = comp;

    if (!queue_enqueue(&bridge->to_broca_queue, &message)) {
        nimcp_free(comp.phonemes);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wbb_request_repetition: queue_enqueue is NULL");
        return -1;
    }

    bridge->stats.messages_sent++;
    bridge->stats.repetition_requests++;

    return 0;
}

int wbb_send_response_intent(
    wernicke_broca_bridge_t* bridge,
    const float* semantic_vector,
    uint32_t semantic_dim,
    uint8_t intent_type
) {
    if (!bridge || !semantic_vector || semantic_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_send_response_intent: required parameter is NULL (bridge, semantic_vector)");
        return -1;
    }

    if (!bridge->config.enable_ventral_stream) {
        return -1;  /* Ventral stream disabled — required for semantic production */
    }

    /* Create comprehension with semantic only */
    wbb_comprehension_t comp = {0};
    comp.semantic_vector = nimcp_malloc(semantic_dim * sizeof(float));
    if (!comp.semantic_vector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wbb_send_response_intent: comp is NULL");
        return -1;
    }
    memcpy(comp.semantic_vector, semantic_vector, semantic_dim * sizeof(float));
    comp.semantic_dim = semantic_dim;
    comp.confidence = 1.0f;

    /* Create message */
    wbb_message_t message = {0};
    message.type = WBB_MSG_RESPONSE_INTENT;
    message.stream = WBB_STREAM_VENTRAL;
    message.timestamp_ms = get_timestamp_ms();
    message.sequence_num = bridge->next_sequence++;
    message.payload.comprehension = comp;

    if (!queue_enqueue(&bridge->to_broca_queue, &message)) {
        nimcp_free(comp.semantic_vector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wbb_send_response_intent: queue_enqueue is NULL");
        return -1;
    }

    bridge->stats.messages_sent++;

    return 0;
}

/*=============================================================================
 * BROCA → WERNICKE (Self-Monitoring)
 *===========================================================================*/

int wbb_receive_efference_copy(
    wernicke_broca_bridge_t* bridge,
    wbb_efference_copy_t* efference
) {
    if (!bridge || !efference) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_receive_efference_copy: required parameter is NULL (bridge, efference)");
        return -1;
    }

    if (!bridge->config.enable_self_monitoring) {
        return 1;  /* Self-monitoring disabled */
    }

    /* Check for pending efference copy */
    wbb_message_t message;
    if (!queue_peek(&bridge->to_wernicke_queue, &message)) {
        return 1;  /* No message available */
    }

    if (message.type != WBB_MSG_EFFERENCE_COPY) {
        return 1;  /* Not an efference copy */
    }

    /* Dequeue and copy */
    queue_dequeue(&bridge->to_wernicke_queue, &message);
    *efference = message.payload.efference;

    /* Store for monitoring */
    if (bridge->has_last_efference) {
        nimcp_free(bridge->last_efference.planned_phonemes);
        nimcp_free(bridge->last_efference.motor_plan);
    }
    bridge->last_efference = *efference;
    bridge->has_last_efference = true;

    bridge->stats.messages_received++;
    bridge->stats.efference_copies_received++;

    return 0;
}

int wbb_compare_production(
    wernicke_broca_bridge_t* bridge,
    const wbb_comprehension_t* intended,
    const wbb_efference_copy_t* efference,
    wbb_monitoring_result_t* result
) {
    if (!bridge || !intended || !efference || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_compare_production: required parameter is NULL (bridge, intended, efference, result)");
        return -1;
    }

    memset(result, 0, sizeof(wbb_monitoring_result_t));

    /* Compare phoneme sequences */
    float phoneme_sim = compute_phoneme_similarity(
        intended->phonemes, intended->num_phonemes,
        efference->planned_phonemes, efference->num_planned_phonemes
    );
    result->phoneme_match = (phoneme_sim >= bridge->config.monitoring_threshold);

    /* Semantic comparison (if available) */
    result->semantic_match = true;  /* Default if no semantic data */
    /* TODO: Add semantic comparison when motor plan includes semantics */

    /* Overall match */
    result->match_score = phoneme_sim;

    /* Error detection */
    if (phoneme_sim < bridge->config.error_detection_threshold) {
        result->error_detected = true;
        result->error_type = 1;  /* Phoneme error */

        /* Find first mismatch position */
        uint32_t min_len = (intended->num_phonemes < efference->num_planned_phonemes) ?
                          intended->num_phonemes : efference->num_planned_phonemes;
        for (uint32_t i = 0; i < min_len; i++) {
            if (intended->phonemes[i] != efference->planned_phonemes[i]) {
                result->error_position = i;
                break;
            }
        }
    }

    bridge->stats.monitoring_checks++;
    if (result->error_detected) {
        bridge->stats.errors_detected++;
    }

    return 0;
}

int wbb_send_error_signal(
    wernicke_broca_bridge_t* bridge,
    uint8_t error_type,
    uint32_t position,
    const uint8_t* correction
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_send_error_signal: bridge is NULL");
        return -1;
    }

    if (!bridge->config.enable_error_correction) {
        return -1;  /* Error correction disabled — not an error */
    }

    /* Create error message */
    wbb_message_t message = {0};
    message.type = WBB_MSG_ERROR_SIGNAL;
    message.stream = WBB_STREAM_DORSAL;  /* Use dorsal for quick feedback */
    message.timestamp_ms = get_timestamp_ms();
    message.sequence_num = bridge->next_sequence++;

    /* Store error info in comprehension payload */
    message.payload.comprehension.confidence = 0.0f;  /* Indicates error */
    message.payload.comprehension.thematic_role = error_type;
    message.payload.comprehension.word_id = position;

    /* Copy correction if provided */
    if (correction) {
        /* Assume correction is null-terminated phoneme string */
        uint32_t corr_len = 0;
        while (correction[corr_len] != 0 && corr_len < 32) corr_len++;

        if (corr_len > 0) {
            message.payload.comprehension.phonemes = nimcp_malloc(corr_len);
            if (message.payload.comprehension.phonemes) {
                memcpy(message.payload.comprehension.phonemes, correction, corr_len);
                message.payload.comprehension.num_phonemes = corr_len;
            }
        }
    }

    if (!queue_enqueue(&bridge->to_broca_queue, &message)) {
        nimcp_free(message.payload.comprehension.phonemes);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wbb_send_error_signal: queue_enqueue is NULL");
        return -1;
    }

    bridge->stats.messages_sent++;

    return 0;
}

/*=============================================================================
 * WORKING MEMORY REHEARSAL
 *===========================================================================*/

int wbb_request_rehearsal(
    wernicke_broca_bridge_t* bridge,
    const uint8_t* phonemes,
    uint32_t num_phonemes,
    uint32_t repetitions
) {
    if (!bridge || !phonemes || num_phonemes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_request_rehearsal: required parameter is NULL (bridge, phonemes)");
        return -1;
    }

    if (!bridge->config.enable_working_memory) {
        return -1;  /* Working memory disabled — not an error */
    }

    /* Create rehearsal request */
    wbb_rehearsal_t rehearsal = {0};
    rehearsal.phonemes = nimcp_malloc(num_phonemes);
    if (!rehearsal.phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wbb_request_rehearsal: rehearsal is NULL");
        return -1;
    }
    memcpy(rehearsal.phonemes, phonemes, num_phonemes);
    rehearsal.num_phonemes = num_phonemes;
    rehearsal.repetitions = repetitions;
    rehearsal.decay_rate = 0.1f;

    /* Create message */
    wbb_message_t message = {0};
    message.type = WBB_MSG_REHEARSAL;
    message.stream = WBB_STREAM_DORSAL;
    message.timestamp_ms = get_timestamp_ms();
    message.sequence_num = bridge->next_sequence++;
    message.payload.rehearsal = rehearsal;

    if (!queue_enqueue(&bridge->to_broca_queue, &message)) {
        nimcp_free(rehearsal.phonemes);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wbb_request_rehearsal: queue_enqueue is NULL");
        return -1;
    }

    bridge->stats.messages_sent++;

    return 0;
}

int wbb_process_rehearsal(wernicke_broca_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_process_rehearsal: bridge is NULL");
        return -1;
    }

    /* Check for returning rehearsal */
    wbb_message_t message;
    if (!queue_peek(&bridge->to_wernicke_queue, &message)) {
        return 0;  /* No message */
    }

    if (message.type != WBB_MSG_REHEARSAL) {
        return 0;  /* Not a rehearsal */
    }

    /* Dequeue and process */
    queue_dequeue(&bridge->to_wernicke_queue, &message);

    /* Rehearsal would refresh phonological memory here */
    /* For now, just decrement repetitions and re-queue if needed */
    if (message.payload.rehearsal.repetitions > 1) {
        message.payload.rehearsal.repetitions--;
        queue_enqueue(&bridge->to_broca_queue, &message);
    } else {
        nimcp_free(message.payload.rehearsal.phonemes);
    }

    bridge->stats.messages_received++;

    return 0;
}

/*=============================================================================
 * MESSAGE HANDLING
 *===========================================================================*/

int wbb_process_messages(
    wernicke_broca_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_process_messages: bridge is NULL");
        return -1;
    }

    uint32_t processed = 0;
    uint32_t limit = (max_messages == 0) ?
                     bridge->to_broca_queue.count + bridge->to_wernicke_queue.count :
                     max_messages;

    /* Process to-Broca messages */
    while (processed < limit && bridge->to_broca_queue.count > 0) {
        wbb_message_t message;
        if (queue_dequeue(&bridge->to_broca_queue, &message)) {
            /* Would dispatch to Broca here */
            /* For now, just count as processed */

            /* Free payload */
            if (message.type == WBB_MSG_COMPREHENSION ||
                message.type == WBB_MSG_REPETITION_REQUEST ||
                message.type == WBB_MSG_RESPONSE_INTENT) {
                nimcp_free(message.payload.comprehension.semantic_vector);
                nimcp_free(message.payload.comprehension.phonemes);
            } else if (message.type == WBB_MSG_REHEARSAL) {
                nimcp_free(message.payload.rehearsal.phonemes);
            }

            processed++;
        }
    }

    /* Process to-Wernicke messages */
    while (processed < limit && bridge->to_wernicke_queue.count > 0) {
        wbb_message_t message;
        if (queue_dequeue(&bridge->to_wernicke_queue, &message)) {
            /* Would dispatch to Wernicke here */

            /* Free payload */
            if (message.type == WBB_MSG_EFFERENCE_COPY) {
                nimcp_free(message.payload.efference.planned_phonemes);
                nimcp_free(message.payload.efference.motor_plan);
            } else if (message.type == WBB_MSG_REHEARSAL) {
                nimcp_free(message.payload.rehearsal.phonemes);
            }

            processed++;
        }
    }

    return (int)processed;
}

uint32_t wbb_pending_count(const wernicke_broca_bridge_t* bridge) {
    if (!bridge) return 0;
    return bridge->to_broca_queue.count + bridge->to_wernicke_queue.count;
}

int wbb_peek_message(
    const wernicke_broca_bridge_t* bridge,
    wbb_message_t* message
) {
    if (!bridge || !message) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_peek_message: required parameter is NULL (bridge, message)");
        return -1;
    }

    /* Check to-Broca queue first */
    if (queue_peek(&bridge->to_broca_queue, message)) {
        return 0;
    }

    /* Then to-Wernicke queue */
    if (queue_peek(&bridge->to_wernicke_queue, message)) {
        return 0;
    }

    return 1;  /* No messages */
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

int wbb_get_stats(
    const wernicke_broca_bridge_t* bridge,
    wbb_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void wbb_reset_stats(wernicke_broca_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(wbb_stats_t));
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

int wbb_get_config(
    const wernicke_broca_bridge_t* bridge,
    wbb_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    *config = bridge->config;
    return 0;
}

int wbb_set_config(
    wernicke_broca_bridge_t* bridge,
    const wbb_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wbb_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    bridge->config = *config;
    return 0;
}
