/**
 * @file nimcp_language_temporal_bridge.c
 * @brief Language-Temporal Lobe Bridge Implementation
 *
 * Implements bidirectional integration between the Language Layer
 * and Temporal Lobe for semantic memory access and speech routing.
 *
 * @version 1.0.0 - Phase LT1: Language-Temporal Integration
 * @date 2026-01-05
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "language/bridges/nimcp_language_temporal_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "LANG_TEMPORAL"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_temporal_bridge)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Concept cache entry
 */
typedef struct {
    uint32_t concept_id;
    lt_concept_activation_t activation;
    uint64_t last_access_ms;
    bool valid;
} concept_cache_entry_t;

/**
 * @brief Pending async query
 */
typedef struct pending_query {
    uint32_t query_id;
    lt_semantic_query_t query;
    uint64_t submit_time_ms;
    struct pending_query* next;
} pending_query_t;

/**
 * @brief Bridge internal state
 */
struct language_temporal_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    language_temporal_config_t config;

    /* Connected modules */
    language_orchestrator_t* language;
    temporal_adapter_t* temporal;
    wernicke_adapter_t* wernicke;
    broca_adapter_t* broca;
    bio_router_t bio_router;

    /* State */
    lt_bridge_state_t state;
    uint64_t last_update_ms;

    /* Concept cache */
    concept_cache_entry_t* concept_cache;
    uint32_t cache_size;

    /* Active concepts buffer */
    lt_concept_activation_t* active_concepts;
    uint32_t active_concept_count;
    uint32_t active_concept_capacity;

    /* Speech event buffer */
    lt_speech_event_data_t* speech_buffer;
    uint32_t speech_buffer_head;
    uint32_t speech_buffer_tail;
    uint32_t speech_buffer_size;
    bool speech_active;

    /* Pending queries */
    pending_query_t* pending_queries;
    uint32_t next_query_id;

    /* Callbacks */
    lt_speech_callback_t speech_callback;
    void* speech_callback_data;
    lt_concept_callback_t concept_callback;
    void* concept_callback_data;

    /* Statistics */
    language_temporal_stats_t stats;
};

/**
 * @brief Find concept in cache
 */
static concept_cache_entry_t* cache_find(
    language_temporal_bridge_t* bridge,
    uint32_t concept_id
) {
    if (!bridge->concept_cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cache_find: bridge->concept_cache is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < bridge->cache_size; i++) {
        if (bridge->concept_cache[i].valid &&
            bridge->concept_cache[i].concept_id == concept_id) {
            return &bridge->concept_cache[i];
        }
    }
    return NULL;  /* Not found - normal cache miss */
}

/**
 * @brief Add concept to cache (LRU eviction)
 */
static void cache_add(
    language_temporal_bridge_t* bridge,
    const lt_concept_activation_t* concept,
    uint64_t timestamp_ms
) {
    if (!bridge->concept_cache) return;

    /* Find empty slot or LRU entry */
    uint32_t target_idx = 0;
    uint64_t oldest_time = UINT64_MAX;

    for (uint32_t i = 0; i < bridge->cache_size; i++) {
        if (!bridge->concept_cache[i].valid) {
            target_idx = i;
            break;
        }
        if (bridge->concept_cache[i].last_access_ms < oldest_time) {
            oldest_time = bridge->concept_cache[i].last_access_ms;
            target_idx = i;
        }
    }

    /* Free old embedding if present */
    if (bridge->concept_cache[target_idx].valid &&
        bridge->concept_cache[target_idx].activation.embedding) {
        nimcp_free(bridge->concept_cache[target_idx].activation.embedding);
    }

    /* Store new entry */
    bridge->concept_cache[target_idx].concept_id = concept->concept_id;
    bridge->concept_cache[target_idx].activation = *concept;
    bridge->concept_cache[target_idx].last_access_ms = timestamp_ms;
    bridge->concept_cache[target_idx].valid = true;

    /* Copy embedding if present */
    if (concept->embedding && concept->embedding_dim > 0) {
        bridge->concept_cache[target_idx].activation.embedding =
            nimcp_malloc(concept->embedding_dim * sizeof(float));
        if (bridge->concept_cache[target_idx].activation.embedding) {
            memcpy(bridge->concept_cache[target_idx].activation.embedding,
                   concept->embedding,
                   concept->embedding_dim * sizeof(float));
        }
    }
}

/**
 * @brief Queue speech event
 */
static bool queue_speech_event(
    language_temporal_bridge_t* bridge,
    const lt_speech_event_data_t* event
) {
    uint32_t next_tail = (bridge->speech_buffer_tail + 1) % bridge->speech_buffer_size;
    if (next_tail == bridge->speech_buffer_head) {
        /* Buffer full - normal backpressure */
        return false;
    }

    bridge->speech_buffer[bridge->speech_buffer_tail] = *event;
    bridge->speech_buffer_tail = next_tail;
    return true;
}

/**
 * @brief Dequeue speech event
 */
static bool dequeue_speech_event(
    language_temporal_bridge_t* bridge,
    lt_speech_event_data_t* event
) {
    if (bridge->speech_buffer_head == bridge->speech_buffer_tail) {
        return false;  /* Buffer empty - normal condition */
    }

    *event = bridge->speech_buffer[bridge->speech_buffer_head];
    bridge->speech_buffer_head = (bridge->speech_buffer_head + 1) % bridge->speech_buffer_size;
    return true;
}

/**
 * @brief Convert temporal concept to bridge activation format
 */
static void convert_temporal_concept(
    const temporal_concept_t* src,
    lt_concept_activation_t* dst
) {
    dst->concept_id = src->concept_id;
    strncpy(dst->name, src->name, sizeof(dst->name) - 1);
    dst->name[sizeof(dst->name) - 1] = '\0';
    dst->activation = src->activation;
    dst->embedding = src->embedding;
    dst->embedding_dim = src->embedding_dim;
    dst->modality = src->modality;
    dst->is_primed = false;
    dst->activation_time_ms = nimcp_time_now_us() / 1000;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

language_temporal_config_t language_temporal_default_config(void) {
    return (language_temporal_config_t){
        .mode = LT_MODE_BIDIRECTIONAL,
        .enable_speech_routing = true,
        .enable_semantic_queries = true,
        .enable_priming = true,
        .enable_spreading_activation = true,
        .update_interval_ms = LANGUAGE_TEMPORAL_DEFAULT_UPDATE_INTERVAL_MS,
        .semantic_cache_size = LANGUAGE_TEMPORAL_DEFAULT_SEMANTIC_CACHE_SIZE,
        .priming_strength = LANGUAGE_TEMPORAL_DEFAULT_PRIMING_STRENGTH,
        .priming_decay = LANGUAGE_TEMPORAL_DEFAULT_PRIMING_DECAY,
        .spreading_depth = LANGUAGE_TEMPORAL_DEFAULT_SPREADING_DEPTH,
        .activation_threshold = LANGUAGE_TEMPORAL_ACTIVATION_THRESHOLD,
        .speech_detection_threshold = LANGUAGE_TEMPORAL_DEFAULT_SPEECH_THRESHOLD,
        .speech_buffer_size = LANGUAGE_TEMPORAL_SPEECH_BUFFER_SIZE,
        .phoneme_buffer_size = LANGUAGE_TEMPORAL_PHONEME_BUFFER_SIZE,
        .enable_bio_async = false
    };
}

language_temporal_bridge_t* language_temporal_bridge_create(
    language_orchestrator_t* language,
    temporal_adapter_t* temporal,
    const language_temporal_config_t* config
) {
    /* Temporal adapter is required */
    if (!temporal) {
        LOG_ERROR(LOG_MODULE, "Temporal adapter required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_bridge_create: temporal is NULL");
        return NULL;
    }

    language_temporal_bridge_t* bridge = nimcp_calloc(1, sizeof(language_temporal_bridge_t));
    if (!bridge) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_temporal_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = language_temporal_default_config();
    }

    /* Store connections */
    bridge->language = language;
    bridge->temporal = temporal;

    /* Allocate concept cache */
    bridge->cache_size = bridge->config.semantic_cache_size;
    bridge->concept_cache = nimcp_calloc(bridge->cache_size, sizeof(concept_cache_entry_t));
    if (!bridge->concept_cache) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate concept cache");
        language_temporal_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_temporal_bridge_create: bridge->concept_cache is NULL");
        return NULL;
    }

    /* Allocate active concepts buffer */
    bridge->active_concept_capacity = LANGUAGE_TEMPORAL_MAX_ACTIVE_CONCEPTS;
    bridge->active_concepts = nimcp_calloc(bridge->active_concept_capacity,
                                           sizeof(lt_concept_activation_t));
    if (!bridge->active_concepts) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate active concepts buffer");
        language_temporal_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_temporal_bridge_create: bridge->active_concepts is NULL");
        return NULL;
    }

    /* Allocate speech buffer */
    bridge->speech_buffer_size = bridge->config.speech_buffer_size;
    bridge->speech_buffer = nimcp_calloc(bridge->speech_buffer_size,
                                         sizeof(lt_speech_event_data_t));
    if (!bridge->speech_buffer) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate speech buffer");
        language_temporal_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_temporal_bridge_create: bridge->speech_buffer is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state = LT_STATE_IDLE;
    bridge->next_query_id = 1;

    LOG_INFO(LOG_MODULE, "Language-Temporal bridge created");
    return bridge;
}

void language_temporal_bridge_destroy(language_temporal_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_temporal");

    /* Free concept cache embeddings */
    if (bridge->concept_cache) {
        for (uint32_t i = 0; i < bridge->cache_size; i++) {
            if (bridge->concept_cache[i].valid &&
                bridge->concept_cache[i].activation.embedding) {
                nimcp_free(bridge->concept_cache[i].activation.embedding);
            }
        }
        nimcp_free(bridge->concept_cache);
    }

    /* Free active concepts */
    if (bridge->active_concepts) {
        nimcp_free(bridge->active_concepts);
    }

    /* Free speech buffer */
    if (bridge->speech_buffer) {
        nimcp_free(bridge->speech_buffer);
    }

    /* Free pending queries */
    pending_query_t* q = bridge->pending_queries;
    while (q) {
        pending_query_t* next = q->next;
        nimcp_free(q);
        q = next;
    }

    nimcp_free(bridge);
    LOG_INFO(LOG_MODULE, "Language-Temporal bridge destroyed");
}

int language_temporal_bridge_reset(language_temporal_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Clear cache */
    for (uint32_t i = 0; i < bridge->cache_size; i++) {
        if (bridge->concept_cache[i].valid &&
            bridge->concept_cache[i].activation.embedding) {
            nimcp_free(bridge->concept_cache[i].activation.embedding);
        }
        bridge->concept_cache[i].valid = false;
    }

    /* Clear active concepts */
    bridge->active_concept_count = 0;

    /* Clear speech buffer */
    bridge->speech_buffer_head = 0;
    bridge->speech_buffer_tail = 0;
    bridge->speech_active = false;

    /* Reset state */
    bridge->state = LT_STATE_IDLE;

    /* Clear pending queries */
    pending_query_t* q = bridge->pending_queries;
    while (q) {
        pending_query_t* next = q->next;
        nimcp_free(q);
        q = next;
    }
    bridge->pending_queries = NULL;

    return 0;
}

//=============================================================================
// Connection Functions
//=============================================================================

int language_temporal_connect_wernicke(
    language_temporal_bridge_t* bridge,
    wernicke_adapter_t* wernicke
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_connect_wernicke: bridge is NULL");
        return -1;
    }
    bridge->wernicke = wernicke;
    LOG_INFO(LOG_MODULE, "Connected to Wernicke adapter");
    return 0;
}

int language_temporal_connect_broca(
    language_temporal_bridge_t* bridge,
    broca_adapter_t* broca
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_connect_broca: bridge is NULL");
        return -1;
    }
    bridge->broca = broca;
    LOG_INFO(LOG_MODULE, "Connected to Broca adapter");
    return 0;
}

int language_temporal_connect_bio_async(
    language_temporal_bridge_t* bridge,
    bio_router_t router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_connect_bio_async: bridge is NULL");
        return -1;
    }
    bridge->bio_router = router;
    LOG_INFO(LOG_MODULE, "Connected to bio-async router");
    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int language_temporal_bridge_update(
    language_temporal_bridge_t* bridge,
    uint64_t timestamp_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_bridge_update: bridge is NULL");
        return -1;
    }

    int events_processed = 0;

    /* Check for speech in auditory stream */
    if (bridge->config.enable_speech_routing && bridge->temporal) {
        float speech_confidence = 0.0f;
        bool is_speech = temporal_detect_speech(bridge->temporal, &speech_confidence);

        if (is_speech && speech_confidence >= bridge->config.speech_detection_threshold) {
            if (!bridge->speech_active) {
                /* Speech onset */
                lt_speech_event_data_t onset = {
                    .type = LT_SPEECH_ONSET,
                    .confidence = speech_confidence,
                    .timestamp_ms = (double)timestamp_ms
                };
                queue_speech_event(bridge, &onset);
                bridge->speech_active = true;
                bridge->state = LT_STATE_SPEECH_ACTIVE;
                events_processed++;
            }
        } else if (bridge->speech_active) {
            /* Speech offset */
            lt_speech_event_data_t offset = {
                .type = LT_SPEECH_OFFSET,
                .confidence = 1.0f - speech_confidence,
                .timestamp_ms = (double)timestamp_ms
            };
            queue_speech_event(bridge, &offset);
            bridge->speech_active = false;
            bridge->state = LT_STATE_LISTENING;
            events_processed++;
        }
    }

    /* Process speech events through callback */
    if (bridge->speech_callback) {
        lt_speech_event_data_t event;
        while (dequeue_speech_event(bridge, &event)) {
            bridge->speech_callback(&event, bridge->speech_callback_data);
            bridge->stats.speech_events_routed++;
            events_processed++;
        }
    }

    /* Decay priming effects */
    if (bridge->config.enable_priming) {
        for (uint32_t i = 0; i < bridge->active_concept_count; i++) {
            bridge->active_concepts[i].activation *= bridge->config.priming_decay;
            if (bridge->active_concepts[i].activation < bridge->config.activation_threshold) {
                /* Remove from active list by swapping with last */
                if (i < bridge->active_concept_count - 1) {
                    bridge->active_concepts[i] =
                        bridge->active_concepts[bridge->active_concept_count - 1];
                }
                bridge->active_concept_count--;
                i--;
            }
        }
    }

    /* Process pending async queries */
    pending_query_t** qp = &bridge->pending_queries;
    while (*qp) {
        pending_query_t* q = *qp;

        /* Process query */
        lt_concept_activation_t results[16];
        int count = 0;

        switch (q->query.type) {
            case LT_QUERY_WORD_MEANING:
                count = language_temporal_query_word_meaning(
                    bridge, q->query.word, results, 16);
                break;
            case LT_QUERY_RELATED_CONCEPTS:
                count = language_temporal_get_related_concepts(
                    bridge, q->query.concept_id, results, 16,
                    bridge->config.spreading_depth);
                break;
            default:
                break;
        }

        /* Invoke callback if set */
        if (q->query.callback && count > 0) {
            q->query.callback(results, (uint32_t)count, q->query.user_data);
        }

        /* Remove from list */
        *qp = q->next;
        nimcp_free(q);
        bridge->stats.semantic_queries_processed++;
        events_processed++;
    }

    bridge->last_update_ms = timestamp_ms;
    bridge->stats.current_state = bridge->state;
    bridge->stats.active_concepts = bridge->active_concept_count;

    return events_processed;
}

int language_temporal_process_auditory(
    language_temporal_bridge_t* bridge,
    const temporal_auditory_result_t* auditory_result
) {
    if (!bridge || !auditory_result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_process_auditory: required parameter is NULL (bridge, auditory_result)");
        return -1;
    }

    int events = 0;

    /* Check for speech detection */
    if (auditory_result->is_speech) {
        lt_speech_event_data_t event = {
            .type = LT_SPEECH_PHONEME,
            .confidence = 0.8f,
            .timestamp_ms = auditory_result->timestamp_ms,
            .pitch_hz = auditory_result->fundamental_freq,
            .intensity = auditory_result->loudness
        };

        if (queue_speech_event(bridge, &event)) {
            events++;
        }
    }

    /* Prosodic features */
    if (auditory_result->fundamental_freq > 0) {
        lt_speech_event_data_t prosody = {
            .type = LT_SPEECH_PROSODY,
            .confidence = 0.7f,
            .timestamp_ms = auditory_result->timestamp_ms,
            .pitch_hz = auditory_result->fundamental_freq,
            .intensity = auditory_result->loudness,
            .pitch_contour = auditory_result->spectral_flux
        };

        if (queue_speech_event(bridge, &prosody)) {
            events++;
        }
    }

    return events;
}

//=============================================================================
// Semantic Query Functions
//=============================================================================

int language_temporal_query_word_meaning(
    language_temporal_bridge_t* bridge,
    const char* word,
    lt_concept_activation_t* results,
    uint32_t max_results
) {
    if (!bridge || !word || !results || max_results == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_query_word_meaning: required parameter is NULL (bridge, word, results)");
        return -1;
    }
    if (!bridge->temporal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_query_word_meaning: bridge->temporal is NULL");
        return -1;
    }

    uint64_t start_time = nimcp_time_now_us() / 1000;

    /* Search temporal semantic memory */
    temporal_concept_t* concepts = nimcp_calloc(max_results, sizeof(temporal_concept_t));
    if (!concepts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_temporal_query_word_meaning: concepts is NULL");
        return -1;
    }

    uint32_t found = temporal_search_concepts(bridge->temporal, word, concepts, max_results);

    /* Convert to bridge format */
    for (uint32_t i = 0; i < found; i++) {
        convert_temporal_concept(&concepts[i], &results[i]);

        /* Cache the result */
        cache_add(bridge, &results[i], nimcp_time_now_us() / 1000);
        bridge->stats.concepts_transferred++;
    }

    nimcp_free(concepts);

    /* Update statistics */
    uint64_t elapsed = nimcp_time_now_us() / 1000 - start_time;
    float n = (float)(bridge->stats.semantic_queries_processed + 1);
    bridge->stats.avg_query_latency_ms =
        ((n - 1.0f) * bridge->stats.avg_query_latency_ms + (float)elapsed) / n;

    if (found > 0) {
        bridge->stats.cache_misses++;
    }

    return (int)found;
}

int language_temporal_query_concept_word(
    language_temporal_bridge_t* bridge,
    uint32_t concept_id,
    char words[][64],
    uint32_t max_words
) {
    if (!bridge || !words || max_words == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_query_concept_word: required parameter is NULL (bridge, words)");
        return -1;
    }
    if (!bridge->temporal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_query_concept_word: bridge->temporal is NULL");
        return -1;
    }

    /* Get concept from temporal */
    temporal_concept_t concept;
    if (!temporal_get_concept(bridge->temporal, concept_id, &concept)) {
        return 0;
    }

    /* Return concept name as word form */
    strncpy(words[0], concept.name, 63);
    words[0][63] = '\0';

    return 1;
}

int language_temporal_get_related_concepts(
    language_temporal_bridge_t* bridge,
    uint32_t concept_id,
    lt_concept_activation_t* results,
    uint32_t max_results,
    uint32_t depth
) {
    if (!bridge || !results || max_results == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_get_related_concepts: required parameter is NULL (bridge, results)");
        return -1;
    }
    if (!bridge->temporal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_get_related_concepts: bridge->temporal is NULL");
        return -1;
    }

    /* Use temporal's spreading activation */
    temporal_semantic_result_t semantic_result;
    semantic_result.concepts = nimcp_calloc(max_results, sizeof(temporal_concept_t));
    if (!semantic_result.concepts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_temporal_get_related_concepts: semantic_result is NULL");
        return -1;
    }

    bool success = temporal_get_related(bridge->temporal, concept_id,
                                        &semantic_result, depth);
    if (!success) {
        nimcp_free(semantic_result.concepts);
        return 0;
    }

    /* Convert to bridge format */
    uint32_t count = semantic_result.num_concepts;
    if (count > max_results) count = max_results;

    for (uint32_t i = 0; i < count; i++) {
        convert_temporal_concept(&semantic_result.concepts[i], &results[i]);
        bridge->stats.concepts_transferred++;
    }

    nimcp_free(semantic_result.concepts);
    return (int)count;
}

int language_temporal_apply_priming(
    language_temporal_bridge_t* bridge,
    uint32_t concept_id,
    float strength
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_apply_priming: bridge is NULL");
        return -1;
    }
    if (!bridge->temporal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_apply_priming: bridge->temporal is NULL");
        return -1;
    }

    strength = nimcp_clamp01(strength);

    /* Apply priming in temporal */
    if (!temporal_apply_priming(bridge->temporal, concept_id, strength)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "language_temporal_apply_priming: temporal_apply_priming is NULL");
        return -1;
    }

    bridge->stats.priming_requests++;
    return 0;
}

uint32_t language_temporal_query_async(
    language_temporal_bridge_t* bridge,
    const lt_semantic_query_t* query
) {
    if (!bridge || !query) return 0;

    pending_query_t* pq = nimcp_calloc(1, sizeof(pending_query_t));
    if (!pq) return 0;

    pq->query_id = bridge->next_query_id++;
    pq->query = *query;
    pq->submit_time_ms = nimcp_time_now_us() / 1000;
    pq->next = bridge->pending_queries;
    bridge->pending_queries = pq;

    bridge->stats.pending_queries++;
    return pq->query_id;
}

//=============================================================================
// Word Activation Functions
//=============================================================================

int language_temporal_word_activated(
    language_temporal_bridge_t* bridge,
    const lt_word_activation_t* activation
) {
    if (!bridge || !activation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_word_activated: required parameter is NULL (bridge, activation)");
        return -1;
    }

    bridge->stats.word_activations_received++;

    /* Apply semantic priming if enabled */
    if (bridge->config.enable_priming && bridge->temporal) {
        /* Search for concept matching word */
        temporal_concept_t concepts[4];
        uint32_t found = temporal_search_concepts(bridge->temporal,
                                                   activation->word,
                                                   concepts, 4);
        if (found > 0) {
            /* Prime related concepts */
            temporal_apply_priming(bridge->temporal,
                                   concepts[0].concept_id,
                                   activation->activation * bridge->config.priming_strength);

            /* Add to active concepts */
            if (bridge->active_concept_count < bridge->active_concept_capacity) {
                lt_concept_activation_t* ac =
                    &bridge->active_concepts[bridge->active_concept_count++];
                convert_temporal_concept(&concepts[0], ac);
                ac->is_primed = true;
            }
        }
    }

    return 0;
}

int language_temporal_words_activated_batch(
    language_temporal_bridge_t* bridge,
    const lt_word_activation_t* activations,
    uint32_t count
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_words_activated_batch: required parameter is NULL (bridge, activations)");
        return -1;
    }

    int processed = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (language_temporal_word_activated(bridge, &activations[i]) == 0) {
            processed++;
        }
    }

    return processed;
}

//=============================================================================
// Concept Transfer Functions
//=============================================================================

int language_temporal_get_active_concepts(
    language_temporal_bridge_t* bridge,
    lt_concept_activation_t* concepts,
    uint32_t max_concepts
) {
    if (!bridge || !concepts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_get_active_concepts: required parameter is NULL (bridge, concepts)");
        return -1;
    }

    uint32_t count = bridge->active_concept_count;
    if (count > max_concepts) count = max_concepts;

    memcpy(concepts, bridge->active_concepts,
           count * sizeof(lt_concept_activation_t));

    return (int)count;
}

int language_temporal_transfer_concept_embedding(
    language_temporal_bridge_t* bridge,
    uint32_t concept_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_transfer_concept_embedding: bridge is NULL");
        return -1;
    }

    /* Check cache first */
    concept_cache_entry_t* cached = cache_find(bridge, concept_id);
    if (cached) {
        cached->last_access_ms = nimcp_time_now_us() / 1000;
        bridge->stats.cache_hits++;

        /* Notify via callback */
        if (bridge->concept_callback) {
            bridge->concept_callback(&cached->activation, 1,
                                     bridge->concept_callback_data);
        }
        return 0;
    }

    /* Fetch from temporal */
    if (!bridge->temporal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_transfer_concept_embedding: bridge->temporal is NULL");
        return -1;
    }

    temporal_concept_t concept;
    if (!temporal_get_concept(bridge->temporal, concept_id, &concept)) {
        bridge->stats.failed_queries++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "language_temporal_transfer_concept_embedding: temporal_get_concept is NULL");
        return -1;
    }

    /* Convert and cache */
    lt_concept_activation_t activation;
    convert_temporal_concept(&concept, &activation);
    cache_add(bridge, &activation, nimcp_time_now_us() / 1000);

    /* Notify via callback */
    if (bridge->concept_callback) {
        bridge->concept_callback(&activation, 1, bridge->concept_callback_data);
    }

    bridge->stats.concepts_transferred++;
    bridge->stats.cache_misses++;
    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int language_temporal_set_speech_callback(
    language_temporal_bridge_t* bridge,
    lt_speech_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_set_speech_callback: bridge is NULL");
        return -1;
    }
    bridge->speech_callback = callback;
    bridge->speech_callback_data = user_data;
    return 0;
}

int language_temporal_set_concept_callback(
    language_temporal_bridge_t* bridge,
    lt_concept_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_set_concept_callback: bridge is NULL");
        return -1;
    }
    bridge->concept_callback = callback;
    bridge->concept_callback_data = user_data;
    return 0;
}

//=============================================================================
// Status and Statistics
//=============================================================================

lt_bridge_state_t language_temporal_get_state(
    const language_temporal_bridge_t* bridge
) {
    if (!bridge) return LT_STATE_ERROR;
    return bridge->state;
}

int language_temporal_get_stats(
    const language_temporal_bridge_t* bridge,
    language_temporal_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void language_temporal_reset_stats(language_temporal_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(language_temporal_stats_t));
}

int language_temporal_get_config(
    const language_temporal_bridge_t* bridge,
    language_temporal_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    *config = bridge->config;
    return 0;
}

int language_temporal_set_config(
    language_temporal_bridge_t* bridge,
    const language_temporal_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    bridge->config = *config;
    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int language_temporal_process_bio_messages(
    language_temporal_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_temporal_process_bio_messages: bridge is NULL");
        return -1;
    }

    /* TODO: Implement bio-async message processing */
    (void)max_messages;

    return 0;
}

void* language_temporal_get_bio_context(
    language_temporal_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }
    /* TODO: Return actual bio context when integrated */
    return NULL;
}
