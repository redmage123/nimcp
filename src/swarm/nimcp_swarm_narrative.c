/**
 * @file nimcp_swarm_narrative.c
 * @brief Implementation of Swarm Narrative Memory System
 *
 * WHAT: Narrative memory storage and sharing for swarm agents
 * WHY:  Enable social learning through story sharing
 * HOW:  Neural-encoded narratives with gossip-based propagation
 *
 * @version 1.0
 * @date 2025
 */

#include "swarm/nimcp_swarm_narrative.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/validation/nimcp_common.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/containers/nimcp_hash_table.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "swarm_narrative"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_narrative)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ============================================================================
 * Constants and Configuration
 * ============================================================================ */

#define NARRATIVE_ID_BUFFER_SIZE 64
#define MIN_COHERENCE_SCORE 0.0f
#define MAX_COHERENCE_SCORE 1.0f
#define DEFAULT_MAX_NARRATIVES 1000
#define DEFAULT_MAX_EVENTS 100
#define DEFAULT_COHERENCE_THRESHOLD 0.3f
#define COHERENCE_TEMPORAL_WEIGHT 0.4f
#define COHERENCE_EMOTIONAL_WEIGHT 0.3f
#define COHERENCE_ENCODING_WEIGHT 0.3f

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Main narrative memory system structure
 */
struct swarm_narrative {
    /* Configuration */
    swarm_narrative_config_t config;

    /* Storage */
    hash_table_t* narratives;           /**< ID -> narrative_t* */
    hash_table_t* pending_narratives;   /**< ID -> narrative_t* (being constructed) */

    /* Statistics */
    uint32_t total_narratives;
    uint32_t total_events;
    uint32_t total_shares;
    float avg_coherence;

    /* Bio-async integration */
    void* bio_ctx;
    bio_module_context_t bio_module;
    bool bio_async_enabled;

    /* State */
    uint32_t next_narrative_id;
    uint32_t next_event_id;
    uint64_t last_update_ms;

    /* Synchronization */
    nimcp_platform_mutex_t* mutex;
    bool is_initialized;
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static float calculate_temporal_coherence(const narrative_t* narrative);
static float calculate_emotional_coherence(const narrative_t* narrative);
static float calculate_encoding_coherence(const narrative_t* narrative);
static float vector_cosine_similarity(const float* v1, const float* v2, uint32_t size);
static void generate_narrative_id(char* buffer, size_t size, uint32_t id);
static nimcp_result_t send_narrative_message(swarm_narrative_t* sn,
    const narrative_t* narrative, uint32_t target_agent);
static nimcp_result_t handle_narrative_shared(swarm_narrative_t* sn, const void* msg);
static void free_narrative(narrative_t* narrative);

/* ============================================================================
 * Internal Cleanup Helpers
 * ============================================================================ */

/**
 * @brief Free a single narrative and all its owned resources
 *
 * WHAT: Frees event encodings, event array, and narrative struct
 * WHY:  Each narrative owns heap-allocated event_encoding arrays
 * HOW:  Walk events, free each encoding, then free events array, then struct
 */
static void free_narrative(narrative_t* narrative)
{
    if (!narrative) {
        return;
    }

    if (narrative->events) {
        for (uint32_t i = 0; i < narrative->num_events; i++) {
            if (narrative->events[i].event_encoding) {
                nimcp_free(narrative->events[i].event_encoding);
                narrative->events[i].event_encoding = NULL;
            }
        }
        nimcp_free(narrative->events);
        narrative->events = NULL;
    }

    nimcp_free(narrative);
}

/**
 * @brief Hash table iterator that frees each narrative
 *
 * WHAT: Callback for hash_table_iterate to free narrative values
 * WHY:  The hash table stores pointers to narratives that need deep cleanup
 * HOW:  Extracts narrative_t* from stored value and calls free_narrative
 */
static bool narrative_cleanup_iterator(const void* key, size_t key_size, void* value,
                                        size_t value_size, void* user_data)
{
    (void)key; (void)key_size; (void)value_size; (void)user_data;

    if (value && value_size >= sizeof(void*)) {
        narrative_t* narrative = *(narrative_t**)value;
        free_narrative(narrative);
    }

    return true; /* continue iteration */
}

/* ============================================================================
 * Core API Implementation
 * ============================================================================ */

swarm_narrative_t* swarm_narrative_create(const swarm_narrative_config_t* config)
{
    /* WHAT: Create and initialize narrative memory system
     * WHY:  Required before any operations
     * HOW:  Allocate structure, create containers, set defaults
     */

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_narrative_create: config is NULL");
        return NULL;
    }

    LOG_INFO("Creating swarm narrative system (max_narratives=%u, max_events=%u)",
             config->max_narratives, config->max_events_per_narrative);

    swarm_narrative_t* sn = (swarm_narrative_t*)nimcp_malloc(sizeof(swarm_narrative_t));
    if (!sn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_narrative_create: failed to allocate narrative system");
        return NULL;
    }

    memset(sn, 0, sizeof(swarm_narrative_t));
    memcpy(&sn->config, config, sizeof(swarm_narrative_config_t));

    /* Create hash tables */
    hash_table_config_t ht_config = {
        .initial_buckets = 256,
        .key_type = HASH_KEY_STRING,
        .hash_algorithm = HASH_ALG_FNV1A,
        .value_destructor = NULL,
        .case_insensitive = false,
        .thread_safe = false
    };

    sn->narratives = hash_table_create(&ht_config);
    if (!sn->narratives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_narrative_create: failed to create narratives hash table");
        nimcp_free(sn);
        return NULL;
    }

    sn->pending_narratives = hash_table_create(&ht_config);
    if (!sn->pending_narratives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_narrative_create: failed to create pending hash table");
        hash_table_destroy(sn->narratives);
        nimcp_free(sn);
        return NULL;
    }

    /* Initialize state */
    sn->next_narrative_id = 1;
    sn->next_event_id = 1;
    sn->last_update_ms = nimcp_time_get_us() / 1000;

    /* Create mutex */
    sn->mutex = nimcp_platform_mutex_create();
    if (!sn->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_narrative_create: failed to create mutex");
        hash_table_destroy(sn->pending_narratives);
        hash_table_destroy(sn->narratives);
        nimcp_free(sn);
        return NULL;
    }

    sn->is_initialized = false;

    LOG_INFO("Swarm narrative system created successfully");
    return sn;
}

void swarm_narrative_destroy(swarm_narrative_t* sn)
{
    /* WHAT: Free all resources and cleanup
     * WHY:  Prevent memory leaks
     * HOW:  Free narratives, containers, mutex
     */

    if (!sn) {
        return;
    }

    LOG_INFO("Destroying swarm narrative system");

    if (sn->mutex) {
        nimcp_platform_mutex_lock(sn->mutex);
    }

    /* Free all narratives in the main table */
    if (sn->narratives) {
        hash_table_iterate(sn->narratives, narrative_cleanup_iterator, NULL);
        hash_table_destroy(sn->narratives);
    }

    /* Free all pending narratives (not yet finalized) */
    if (sn->pending_narratives) {
        hash_table_iterate(sn->pending_narratives, narrative_cleanup_iterator, NULL);
        hash_table_destroy(sn->pending_narratives);
    }

    if (sn->mutex) {
        nimcp_platform_mutex_unlock(sn->mutex);
        nimcp_platform_mutex_destroy(sn->mutex);
        nimcp_free(sn->mutex);
        sn->mutex = NULL;
    }

    nimcp_free(sn);

    LOG_INFO("Swarm narrative system destroyed");
}

/* ============================================================================
 * KG-Driven Wiring Callback
 * ============================================================================ */

/**
 * @brief Handler map for KG-driven wiring
 */
DEFINE_HANDLER_MAP_BEGIN(swarm_narrative)
    HANDLER_MAP_ENTRY(BIO_MSG_NARRATIVE_SHARED, (bio_message_handler_t)handle_narrative_shared)
DEFINE_HANDLER_MAP_END()

/**
 * @brief KG-driven wiring callback for swarm narrative module
 *
 * WHAT: Register message handlers based on KG-discovered wiring
 * WHY:  Enable dynamic handler registration from knowledge graph
 * HOW:  Iterate discovered message types and register matching handlers
 *
 * @param bio_ctx Bio-async module context
 * @param message_types Array of message types discovered from KG
 * @param message_count Number of message types in array
 * @param user_data User data (swarm_narrative_t pointer)
 * @return 0 on success, -1 on failure
 */
static int swarm_narrative_wiring_handler_callback(
    bio_module_context_t bio_ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;  /* swarm_narrative_t* context available if needed */

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        for (size_t j = 0; j < HANDLER_MAP_SIZE(swarm_narrative); j++) {
            if (g_swarm_narrative_handler_map[j].message_type == message_types[i]) {
                bio_router_register_handler(
                    bio_ctx,
                    message_types[i],
                    g_swarm_narrative_handler_map[j].handler
                );
                registered++;
                break;
            }
        }
    }

    return (registered > 0) ? 0 : -1;
}

nimcp_result_t swarm_narrative_init(swarm_narrative_t* sn, void* bio_ctx)
{
    /* WHAT: Initialize bio-async integration
     * WHY:  Enable narrative sharing via bio-router
     * HOW:  Register module and message handlers
     */

    if (!sn) {
        return NIMCP_INVALID_PARAM;
    }

    LOG_INFO("Initializing swarm narrative system");

    nimcp_platform_mutex_lock(sn->mutex);

    if (sn->is_initialized) {
        LOG_WARN("Narrative system already initialized");
        nimcp_platform_mutex_unlock(sn->mutex);
        return NIMCP_SUCCESS;
    }

    sn->bio_ctx = bio_ctx;
    sn->bio_async_enabled = (bio_ctx != NULL) && sn->config.enable_bio_async;

    if (sn->bio_async_enabled && bio_router_is_initialized()) {
        /* Register with bio-router */
        bio_module_info_t module_info = {
            .module_id = BIO_MODULE_SWARM_NARRATIVE,
            .module_name = "swarm_narrative",
            .inbox_capacity = NIMCP_INBOX_CAPACITY_LARGE,
            .user_data = sn
        };

        sn->bio_module = bio_router_register_module(&module_info);
        if (sn->bio_module) {
            LOG_INFO("Registered with bio-router");

            /* Try KG-driven wiring callback registration */
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_SWARM_NARRATIVE,
                (void*)swarm_narrative_wiring_handler_callback,
                sn
            );

            if (wiring_result != NIMCP_SUCCESS) {
                /* Fallback to legacy hardcoded registration */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(sn->bio_module, BIO_MSG_NARRATIVE_SHARED,
                        (bio_message_handler_t)handle_narrative_shared)
                );
                LOG_DEBUG("Swarm narrative using legacy handler registration (wiring callback unavailable)");
            } else {
                LOG_DEBUG("Swarm narrative registered KG-driven wiring callback");
            }
        } else {
            LOG_WARN("Failed to register with bio-router");
        }
    }

    sn->is_initialized = true;
    nimcp_platform_mutex_unlock(sn->mutex);

    LOG_INFO("Swarm narrative system initialized");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Narrative Creation Implementation
 * ============================================================================ */

int swarm_narrative_begin(swarm_narrative_t* sn, uint32_t teller_id, uint32_t* narrative_id)
{
    /* WHAT: Start creating a new narrative
     * WHY:  Transaction-like interface for narrative construction
     * HOW:  Allocate narrative structure, add to pending table
     */

    if (!sn || !narrative_id) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(sn->mutex);

    /* Allocate narrative */
    narrative_t* narrative = (narrative_t*)nimcp_malloc(sizeof(narrative_t));
    if (!narrative) {
        nimcp_platform_mutex_unlock(sn->mutex);
        return NIMCP_NO_MEMORY;
    }

    memset(narrative, 0, sizeof(narrative_t));

    /* Allocate event array */
    narrative->events = (narrative_event_t*)nimcp_malloc(
        sizeof(narrative_event_t) * sn->config.max_events_per_narrative);
    if (!narrative->events) {
        nimcp_free(narrative);
        nimcp_platform_mutex_unlock(sn->mutex);
        return NIMCP_NO_MEMORY;
    }

    /* Initialize narrative */
    narrative->narrative_id = sn->next_narrative_id++;
    narrative->teller_agent_id = teller_id;
    narrative->num_events = 0;
    narrative->coherence_score = 0.0F;
    narrative->share_count = 0;

    /* Add to pending table */
    char id_str[NARRATIVE_ID_BUFFER_SIZE];
    generate_narrative_id(id_str, sizeof(id_str), narrative->narrative_id);

    if (hash_table_insert_string(sn->pending_narratives, id_str,
                                  &narrative, sizeof(void*))) {
        *narrative_id = narrative->narrative_id;
        nimcp_platform_mutex_unlock(sn->mutex);

        LOG_DEBUG("Started narrative %u for agent %u", *narrative_id, teller_id);
        return NIMCP_SUCCESS;
    } else {
        nimcp_free(narrative->events);
        nimcp_free(narrative);
        nimcp_platform_mutex_unlock(sn->mutex);
        return NIMCP_ERROR;
    }
}

int swarm_narrative_add_event(swarm_narrative_t* sn, uint32_t narrative_id,
                               const narrative_event_t* event)
{
    /* WHAT: Add event to narrative being constructed
     * WHY:  Build up narrative incrementally
     * HOW:  Copy event data into narrative's event array
     */

    if (!sn || !event) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(sn->mutex);

    /* Lookup pending narrative */
    char id_str[NARRATIVE_ID_BUFFER_SIZE];
    generate_narrative_id(id_str, sizeof(id_str), narrative_id);

    narrative_t** narrative_ptr = (narrative_t**)hash_table_lookup_string(
        sn->pending_narratives, id_str);

    if (!narrative_ptr || !*narrative_ptr) {
        nimcp_platform_mutex_unlock(sn->mutex);
        return NIMCP_NOT_FOUND;
    }

    narrative_t* narrative = *narrative_ptr;

    /* Check capacity */
    if (narrative->num_events >= sn->config.max_events_per_narrative) {
        nimcp_platform_mutex_unlock(sn->mutex);
        LOG_WARN("Narrative %u has reached max events", narrative_id);
        return NIMCP_ERROR;
    }

    /* Copy event */
    narrative_event_t* dest = &narrative->events[narrative->num_events];
    dest->event_id = sn->next_event_id++;
    dest->agent_id = event->agent_id;
    dest->timestamp_ms = event->timestamp_ms;
    dest->encoding_size = event->encoding_size;
    dest->emotional_valence = event->emotional_valence;
    dest->importance = event->importance;

    /* Copy encoding */
    dest->event_encoding = (float*)nimcp_malloc(sizeof(float) * event->encoding_size);
    if (!dest->event_encoding) {
        nimcp_platform_mutex_unlock(sn->mutex);
        return NIMCP_NO_MEMORY;
    }
    memcpy(dest->event_encoding, event->event_encoding,
           sizeof(float) * event->encoding_size);

    narrative->num_events++;
    sn->total_events++;

    nimcp_platform_mutex_unlock(sn->mutex);

    LOG_DEBUG("Added event %u to narrative %u (%u events total)",
              dest->event_id, narrative_id, narrative->num_events);

    return NIMCP_SUCCESS;
}

int swarm_narrative_end(swarm_narrative_t* sn, uint32_t narrative_id)
{
    /* WHAT: Finalize narrative construction
     * WHY:  Calculate coherence, validate, and store
     * HOW:  Compute coherence, check threshold, move to main table
     */

    if (!sn) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(sn->mutex);

    /* Lookup pending narrative */
    char id_str[NARRATIVE_ID_BUFFER_SIZE];
    generate_narrative_id(id_str, sizeof(id_str), narrative_id);

    narrative_t** narrative_ptr = (narrative_t**)hash_table_lookup_string(
        sn->pending_narratives, id_str);

    if (!narrative_ptr || !*narrative_ptr) {
        nimcp_platform_mutex_unlock(sn->mutex);
        return NIMCP_NOT_FOUND;
    }

    narrative_t* narrative = *narrative_ptr;

    /* Require at least one event */
    if (narrative->num_events == 0) {
        nimcp_platform_mutex_unlock(sn->mutex);
        LOG_WARN("Cannot finalize narrative %u with no events", narrative_id);
        return NIMCP_ERROR;
    }

    /* Calculate coherence */
    narrative->coherence_score = narrative_calculate_coherence(narrative);

    /* Check threshold */
    if (narrative->coherence_score < sn->config.coherence_threshold) {
        nimcp_platform_mutex_unlock(sn->mutex);
        LOG_INFO("Narrative %u rejected (coherence %.3f < threshold %.3f)",
                 narrative_id, narrative->coherence_score, sn->config.coherence_threshold);
        return NIMCP_ERROR;
    }

    /* Move from pending to main table */
    hash_table_remove_string(sn->pending_narratives, id_str);

    if (hash_table_insert_string(sn->narratives, id_str,
                                  &narrative, sizeof(void*))) {
        sn->total_narratives++;
        sn->avg_coherence = (sn->avg_coherence * (sn->total_narratives - 1) +
                             narrative->coherence_score) / sn->total_narratives;

        nimcp_platform_mutex_unlock(sn->mutex);

        LOG_INFO("Finalized narrative %u (coherence=%.3f, events=%u)",
                 narrative_id, narrative->coherence_score, narrative->num_events);

        return NIMCP_SUCCESS;
    } else {
        /* Restore to pending on failure */
        hash_table_insert_string(sn->pending_narratives, id_str,
                                 &narrative, sizeof(void*));
        nimcp_platform_mutex_unlock(sn->mutex);
        return NIMCP_ERROR;
    }
}

/* ============================================================================
 * Narrative Sharing Implementation
 * ============================================================================ */

int swarm_narrative_share(swarm_narrative_t* sn, uint32_t narrative_id,
                           const uint32_t* target_agents, uint32_t num_targets)
{
    /* WHAT: Share narrative with specific agents
     * WHY:  Targeted narrative propagation
     * HOW:  Send via bio-router to each target
     */

    if (!sn || !target_agents || num_targets == 0) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(sn->mutex);

    /* Lookup narrative */
    char id_str[NARRATIVE_ID_BUFFER_SIZE];
    generate_narrative_id(id_str, sizeof(id_str), narrative_id);

    narrative_t** narrative_ptr = (narrative_t**)hash_table_lookup_string(
        sn->narratives, id_str);

    if (!narrative_ptr || !*narrative_ptr) {
        nimcp_platform_mutex_unlock(sn->mutex);
        return NIMCP_NOT_FOUND;
    }

    narrative_t* narrative = *narrative_ptr;

    /* Share with each target */
    uint32_t success_count = 0;
    for (uint32_t i = 0; i < num_targets; i++) {
        if (send_narrative_message(sn, narrative, target_agents[i]) == NIMCP_SUCCESS) {
            success_count++;
        }
    }

    narrative->share_count += success_count;
    sn->total_shares += success_count;

    nimcp_platform_mutex_unlock(sn->mutex);

    LOG_INFO("Shared narrative %u with %u/%u agents",
             narrative_id, success_count, num_targets);

    return (success_count > 0) ? NIMCP_SUCCESS : NIMCP_ERROR;
}

int swarm_narrative_broadcast(swarm_narrative_t* sn, uint32_t narrative_id)
{
    /* WHAT: Broadcast narrative to all agents
     * WHY:  Wide dissemination of important narratives
     * HOW:  Use bio-router broadcast
     */

    if (!sn) {
        return NIMCP_INVALID_PARAM;
    }

    if (!sn->bio_async_enabled) {
        LOG_WARN("Bio-async not enabled, cannot broadcast");
        return NIMCP_ERROR;
    }

    nimcp_platform_mutex_lock(sn->mutex);

    /* Lookup narrative */
    char id_str[NARRATIVE_ID_BUFFER_SIZE];
    generate_narrative_id(id_str, sizeof(id_str), narrative_id);

    narrative_t** narrative_ptr = (narrative_t**)hash_table_lookup_string(
        sn->narratives, id_str);

    if (!narrative_ptr || !*narrative_ptr) {
        nimcp_platform_mutex_unlock(sn->mutex);
        return NIMCP_NOT_FOUND;
    }

    narrative_t* narrative = *narrative_ptr;

    /* Build broadcast message */
    size_t msg_size = sizeof(bio_message_header_t) + sizeof(uint32_t) +
                      sizeof(narrative_t);
    uint8_t* buffer = (uint8_t*)nimcp_malloc(msg_size);
    if (!buffer) {
        nimcp_platform_mutex_unlock(sn->mutex);
        return NIMCP_NO_MEMORY;
    }

    bio_message_header_t* header = (bio_message_header_t*)buffer;
    header->type = BIO_MSG_NARRATIVE_SHARED;
    header->source_module = BIO_MODULE_SWARM_MEMORY;
    header->target_module = BIO_MODULE_ALL;
    header->timestamp_us = nimcp_time_get_us();
    header->channel = BIO_CHANNEL_ACETYLCHOLINE;
    header->payload_size = sizeof(uint32_t) + sizeof(narrative_t);
    header->flags = BIO_MSG_FLAG_BROADCAST;

    uint32_t* id_ptr = (uint32_t*)(buffer + sizeof(bio_message_header_t));
    *id_ptr = narrative_id;

    nimcp_result_t result = bio_router_broadcast(sn->bio_module, buffer, msg_size);

    if (result == NIMCP_SUCCESS) {
        narrative->share_count++;
        sn->total_shares++;
    }

    nimcp_free(buffer);
    nimcp_platform_mutex_unlock(sn->mutex);

    LOG_INFO("Broadcast narrative %u to swarm (result=%d)", narrative_id, result);

    return result;
}

/* ============================================================================
 * Narrative Retrieval Implementation
 * ============================================================================ */

/* Callback context for topic query iteration */
typedef struct {
    const float* topic_vector;
    uint32_t vec_size;
    narrative_t** results;
    float* scores;
    uint32_t count;
    uint32_t max_results;
} topic_query_ctx_t;

static bool topic_query_iterator(const void* key, size_t key_size, void* value,
                                  size_t value_size, void* user_data)
{
    (void)key; (void)key_size; (void)value_size;
    topic_query_ctx_t* ctx = (topic_query_ctx_t*)user_data;
    narrative_t* narr = (narrative_t*)value;

    if (!narr || narr->num_events == 0 || ctx->count >= ctx->max_results) {
        return ctx->count < ctx->max_results;
    }

    /* Compute average similarity across events */
    float total_sim = 0.0f;
    uint32_t matched = 0;
    for (uint32_t e = 0; e < narr->num_events; e++) {
        if (narr->events[e].event_encoding && narr->events[e].encoding_size == ctx->vec_size) {
            float sim = vector_cosine_similarity(ctx->topic_vector,
                                                  narr->events[e].event_encoding, ctx->vec_size);
            total_sim += sim;
            matched++;
        }
    }

    if (matched > 0) {
        float avg_sim = total_sim / (float)matched;
        if (avg_sim > 0.1f) {
            ctx->results[ctx->count] = narr;
            ctx->scores[ctx->count] = avg_sim;
            ctx->count++;
        }
    }

    return ctx->count < ctx->max_results;
}

int swarm_narrative_query_by_topic(swarm_narrative_t* sn, const float* topic_vector,
                                    uint32_t vec_size, narrative_t** results, uint32_t* count)
{
    /* WHAT: Find narratives similar to topic vector
     * WHY:  Content-based retrieval for relevant stories
     * HOW:  Cosine similarity between topic and event encodings
     */

    if (!sn || !topic_vector || !results || !count) {
        return NIMCP_INVALID_PARAM;
    }

    *count = 0;

    if (!sn->narratives || sn->total_narratives == 0) {
        return NIMCP_SUCCESS;
    }

    uint32_t max_results = 32;
    float* scores = nimcp_calloc(max_results, sizeof(float));
    if (!scores) return NIMCP_SUCCESS;

    topic_query_ctx_t ctx = {
        .topic_vector = topic_vector,
        .vec_size = vec_size,
        .results = results,
        .scores = scores,
        .count = 0,
        .max_results = max_results
    };

    hash_table_iterate(sn->narratives, topic_query_iterator, &ctx);

    /* Sort by score (simple insertion sort for small arrays) */
    for (uint32_t i = 1; i < ctx.count; i++) {
        narrative_t* tmp_narr = results[i];
        float tmp_score = scores[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && scores[j] < tmp_score) {
            results[j + 1] = results[j];
            scores[j + 1] = scores[j];
            j--;
        }
        results[j + 1] = tmp_narr;
        scores[j + 1] = tmp_score;
    }

    *count = ctx.count;
    nimcp_free(scores);
    return NIMCP_SUCCESS;
}

/* Callback context for popular narratives iteration */
typedef struct {
    narrative_t** results;
    uint32_t* shares;
    uint32_t count;
    uint32_t max_results;
} popular_ctx_t;

static bool popular_iterator(const void* key, size_t key_size, void* value,
                              size_t value_size, void* user_data)
{
    (void)key; (void)key_size; (void)value_size;
    popular_ctx_t* ctx = (popular_ctx_t*)user_data;
    narrative_t* narr = (narrative_t*)value;

    if (!narr || ctx->count >= ctx->max_results) {
        return ctx->count < ctx->max_results;
    }

    ctx->results[ctx->count] = narr;
    ctx->shares[ctx->count] = narr->share_count;
    ctx->count++;
    return ctx->count < ctx->max_results;
}

int swarm_narrative_get_popular(swarm_narrative_t* sn, narrative_t** results, uint32_t* count)
{
    /* WHAT: Get most shared narratives
     * WHY:  Identify culturally important narratives
     * HOW:  Sort by share_count
     */

    if (!sn || !results || !count) {
        return NIMCP_INVALID_PARAM;
    }

    *count = 0;

    if (!sn->narratives || sn->total_narratives == 0) {
        return NIMCP_SUCCESS;
    }

    uint32_t max_results = 32;
    uint32_t* shares = nimcp_calloc(max_results, sizeof(uint32_t));
    if (!shares) return NIMCP_SUCCESS;

    popular_ctx_t ctx = {
        .results = results,
        .shares = shares,
        .count = 0,
        .max_results = max_results
    };

    hash_table_iterate(sn->narratives, popular_iterator, &ctx);

    /* Sort by shares descending */
    for (uint32_t i = 1; i < ctx.count; i++) {
        narrative_t* tmp_narr = results[i];
        uint32_t tmp_shares = shares[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && shares[j] < tmp_shares) {
            results[j + 1] = results[j];
            shares[j + 1] = shares[j];
            j--;
        }
        results[j + 1] = tmp_narr;
        shares[j + 1] = tmp_shares;
    }

    *count = ctx.count;
    nimcp_free(shares);
    return NIMCP_SUCCESS;
}

int swarm_narrative_get(swarm_narrative_t* sn, uint32_t narrative_id, narrative_t** result)
{
    /* WHAT: Get narrative by ID
     * WHY:  Direct access
     * HOW:  Hash table lookup
     */

    if (!sn || !result) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(sn->mutex);

    char id_str[NARRATIVE_ID_BUFFER_SIZE];
    generate_narrative_id(id_str, sizeof(id_str), narrative_id);

    narrative_t** narrative_ptr = (narrative_t**)hash_table_lookup_string(
        sn->narratives, id_str);

    if (!narrative_ptr || !*narrative_ptr) {
        nimcp_platform_mutex_unlock(sn->mutex);
        return NIMCP_NOT_FOUND;
    }

    *result = *narrative_ptr;
    nimcp_platform_mutex_unlock(sn->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

nimcp_result_t swarm_narrative_process_message(swarm_narrative_t* sn, const void* msg)
{
    /* WHAT: Handle incoming narrative message
     * WHY:  Receive shared narratives
     * HOW:  Parse, deserialize, store
     */

    if (!sn || !msg) {
        return NIMCP_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    switch (header->type) {
        case BIO_MSG_NARRATIVE_SHARED:
            return handle_narrative_shared(sn, msg);

        default:
            LOG_DEBUG("Unhandled message type: 0x%x", header->type);
            break;
    }

    return NIMCP_SUCCESS;
}

uint32_t swarm_narrative_process_inbox(swarm_narrative_t* sn, uint32_t max_messages)
{
    /* WHAT: Process pending messages from inbox
     * WHY:  Receive narratives from other agents
     * HOW:  Poll bio-router inbox
     */

    if (!sn || !sn->bio_async_enabled || !sn->bio_module) {
        return 0;
    }

    return bio_router_process_inbox(sn->bio_module, max_messages);
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

int swarm_narrative_get_stats(swarm_narrative_t* sn, uint32_t* total_narratives,
                               uint32_t* total_events, float* avg_coherence,
                               uint32_t* total_shares)
{
    if (!sn) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(sn->mutex);

    if (total_narratives) *total_narratives = sn->total_narratives;
    if (total_events) *total_events = sn->total_events;
    if (avg_coherence) *avg_coherence = sn->avg_coherence;
    if (total_shares) *total_shares = sn->total_shares;

    nimcp_platform_mutex_unlock(sn->mutex);

    return NIMCP_SUCCESS;
}

void swarm_narrative_print_status(const swarm_narrative_t* sn, bool verbose)
{
    if (!sn) {
        return;
    }

    LOG_INFO("=== Swarm Narrative Status ===");
    LOG_INFO("Total narratives: %u", sn->total_narratives);
    LOG_INFO("Total events: %u", sn->total_events);
    LOG_INFO("Average coherence: %.3f", sn->avg_coherence);
    LOG_INFO("Total shares: %u", sn->total_shares);
    LOG_INFO("Bio-async enabled: %s", sn->bio_async_enabled ? "yes" : "no");

    if (verbose) {
        LOG_INFO("Max narratives: %u", sn->config.max_narratives);
        LOG_INFO("Max events per narrative: %u", sn->config.max_events_per_narrative);
        LOG_INFO("Coherence threshold: %.3f", sn->config.coherence_threshold);
        LOG_INFO("Compression enabled: %s", sn->config.enable_compression ? "yes" : "no");
    }
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

narrative_event_t* narrative_event_create(uint32_t agent_id, const float* encoding,
                                           uint32_t encoding_size, float emotional_valence,
                                           float importance)
{
    narrative_event_t* event = (narrative_event_t*)nimcp_malloc(sizeof(narrative_event_t));
    if (!event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "narrative_event_create: event is NULL");
        return NULL;
    }

    event->event_id = 0; /* Set by system */
    event->agent_id = agent_id;
    event->timestamp_ms = nimcp_time_get_us() / 1000;
    event->encoding_size = encoding_size;
    event->emotional_valence = emotional_valence;
    event->importance = importance;

    event->event_encoding = (float*)nimcp_malloc(sizeof(float) * encoding_size);
    if (!event->event_encoding) {
        nimcp_free(event);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "narrative_event_create: event->event_encoding is NULL");
        return NULL;
    }

    memcpy(event->event_encoding, encoding, sizeof(float) * encoding_size);

    return event;
}

void narrative_event_destroy(narrative_event_t* event)
{
    if (!event) {
        return;
    }

    if (event->event_encoding) {
        nimcp_free(event->event_encoding);
    }

    nimcp_free(event);
}

float narrative_calculate_coherence(const narrative_t* narrative)
{
    /* WHAT: Calculate narrative coherence score
     * WHY:  Quality metric for filtering
     * HOW:  Combine temporal, emotional, encoding coherence
     */

    if (!narrative || narrative->num_events == 0) {
        return 0.0F;
    }

    float temporal = calculate_temporal_coherence(narrative);
    float emotional = calculate_emotional_coherence(narrative);
    float encoding = calculate_encoding_coherence(narrative);

    float coherence = (temporal * COHERENCE_TEMPORAL_WEIGHT) +
                      (emotional * COHERENCE_EMOTIONAL_WEIGHT) +
                      (encoding * COHERENCE_ENCODING_WEIGHT);

    return fmaxf(MIN_COHERENCE_SCORE, fminf(MAX_COHERENCE_SCORE, coherence));
}

/* ============================================================================
 * Helper Functions Implementation
 * ============================================================================ */

static float calculate_temporal_coherence(const narrative_t* narrative)
{
    /* WHAT: Check temporal consistency
     * WHY:  Events should be in order
     * HOW:  Verify timestamps are increasing
     */

    if (narrative->num_events < 2) {
        return 1.0F;
    }

    uint32_t ordered_count = 0;
    for (uint32_t i = 1; i < narrative->num_events; i++) {
        if (narrative->events[i].timestamp_ms >= narrative->events[i-1].timestamp_ms) {
            ordered_count++;
        }
    }

    return (float)ordered_count / (narrative->num_events - 1);
}

static float calculate_emotional_coherence(const narrative_t* narrative)
{
    /* WHAT: Check emotional flow consistency
     * WHY:  Emotions should change gradually
     * HOW:  Measure emotional valence variance
     */

    if (narrative->num_events < 2) {
        return 1.0F;
    }

    float total_variance = 0.0F;
    for (uint32_t i = 1; i < narrative->num_events; i++) {
        float diff = narrative->events[i].emotional_valence -
                     narrative->events[i-1].emotional_valence;
        total_variance += fabsf(diff);
    }

    float avg_variance = total_variance / (narrative->num_events - 1);

    /* Lower variance = higher coherence */
    return fmaxf(0.0F, 1.0F - (avg_variance / 2.0F));
}

static float calculate_encoding_coherence(const narrative_t* narrative)
{
    /* WHAT: Check encoding similarity
     * WHY:  Related events should have similar encodings
     * HOW:  Average pairwise cosine similarity
     */

    if (narrative->num_events < 2) {
        return 1.0F;
    }

    float total_similarity = 0.0F;
    uint32_t pair_count = 0;

    /* Sample pairs (not all combinations for efficiency) */
    for (uint32_t i = 0; i < narrative->num_events - 1; i++) {
        float sim = vector_cosine_similarity(
            narrative->events[i].event_encoding,
            narrative->events[i+1].event_encoding,
            narrative->events[i].encoding_size
        );
        total_similarity += sim;
        pair_count++;
    }

    return (pair_count > 0) ? (total_similarity / pair_count) : 0.0F;
}

static float vector_cosine_similarity(const float* v1, const float* v2, uint32_t size)
{
    float dot_product = 0.0F;
    float norm1 = 0.0F;
    float norm2 = 0.0F;

    for (uint32_t i = 0; i < size; i++) {
        dot_product += v1[i] * v2[i];
        norm1 += v1[i] * v1[i];
        norm2 += v2[i] * v2[i];
    }

    norm1 = sqrtf(norm1);
    norm2 = sqrtf(norm2);

    if (norm1 < 1e-8F || norm2 < 1e-8F) {
        return 0.0F;
    }

    return dot_product / (norm1 * norm2);
}

static void generate_narrative_id(char* buffer, size_t size, uint32_t id)
{
    snprintf(buffer, size, "NARR_%08X", id);
}

static nimcp_result_t send_narrative_message(swarm_narrative_t* sn,
    const narrative_t* narrative, uint32_t target_agent)
{
    if (!sn->bio_async_enabled || !sn->bio_module) {
        return NIMCP_ERROR;
    }

    /* Build message (simplified - would need proper serialization) */
    size_t msg_size = sizeof(bio_message_header_t) + sizeof(uint32_t);
    uint8_t* buffer = (uint8_t*)nimcp_malloc(msg_size);
    if (!buffer) {
        return NIMCP_NO_MEMORY;
    }

    bio_message_header_t* header = (bio_message_header_t*)buffer;
    header->type = BIO_MSG_NARRATIVE_SHARED;
    header->source_module = BIO_MODULE_SWARM_MEMORY;
    header->target_module = BIO_MODULE_SWARM_MEMORY;
    header->timestamp_us = nimcp_time_get_us();
    header->channel = BIO_CHANNEL_ACETYLCHOLINE;
    header->payload_size = sizeof(uint32_t);
    header->flags = 0;

    uint32_t* id_ptr = (uint32_t*)(buffer + sizeof(bio_message_header_t));
    *id_ptr = narrative->narrative_id;

    nimcp_result_t result = bio_router_send(sn->bio_module, buffer, msg_size, 0);

    nimcp_free(buffer);
    return result;
}

static nimcp_result_t handle_narrative_shared(swarm_narrative_t* sn, const void* msg)
{
    /* WHAT: Handle received narrative
     * WHY:  Integrate shared narrative
     * HOW:  Parse, deserialize, store
     */

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    const uint32_t* narrative_id = (const uint32_t*)((const uint8_t*)msg +
                                                      sizeof(bio_message_header_t));

    LOG_DEBUG("Received shared narrative: id=%u, from=0x%x",
              *narrative_id, header->source_module);

    /* TODO: Implement full narrative deserialization and storage */

    return NIMCP_SUCCESS;
}
