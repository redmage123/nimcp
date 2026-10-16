#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_hippocampus_bridge.c - Language-Hippocampus Memory Bridge
//=============================================================================

#include "utils/bridge/nimcp_bridge_base.h"
#include "language/bridges/nimcp_language_hippocampus_bridge.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_hippocampus_bridge)

#define LOG_MODULE "LANGUAGE_HIPPOCAMPUS_BRIDGE"


//=============================================================================
// Internal Constants
//=============================================================================

#define MAX_WORD_MEMORIES       2048
#define MAX_ASSOCIATIONS        4096
#define MAX_RETRIEVAL_RESULTS   32

//=============================================================================
// Internal Structures
//=============================================================================

typedef struct {
    word_memory_t memory;
    bool is_valid;
} word_memory_entry_t;

typedef struct {
    semantic_association_t association;
    bool is_valid;
} association_entry_t;

struct language_hippocampus_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    language_hippocampus_config_t config;

    language_orchestrator_t* language;
    hippocampus_adapter_t* hippocampus;
    broca_adapter_t* broca;
    wernicke_adapter_t* wernicke;
    bio_router_t router;

    lh_bridge_state_t state;
    bool is_initialized;

    word_memory_entry_t* memories;
    uint32_t memory_count;
    uint32_t next_memory_id;

    association_entry_t* associations;
    uint32_t association_count;

    float* feature_buffer;

    uint64_t last_update_ms;

    language_hippocampus_stats_t stats;

    lh_encoding_callback_t encoding_callback;
    void* encoding_callback_data;
    lh_retrieval_callback_t retrieval_callback;
    void* retrieval_callback_data;
    lh_consolidation_callback_t consolidation_callback;
    void* consolidation_callback_data;

};

//=============================================================================
// Helper Functions
//=============================================================================

static float compute_similarity(const float* a, const float* b, uint32_t size)
{
    if (!a || !b || size == 0) return 0.0f;

    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    if (norm_a < 1e-8f || norm_b < 1e-8f) return 0.0f;
    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

static int find_memory_slot(language_hippocampus_bridge_t* bridge)
{
    for (uint32_t i = 0; i < bridge->config.max_word_memories; i++) {
        if (!bridge->memories[i].is_valid) return (int)i;
    }
    return -1;  /* No empty slot - normal capacity behavior */
}

static word_memory_entry_t* find_memory_by_id(language_hippocampus_bridge_t* bridge, uint32_t id)
{
    for (uint32_t i = 0; i < bridge->config.max_word_memories; i++) {
        if (bridge->memories[i].is_valid && bridge->memories[i].memory.memory_id == id) {
            return &bridge->memories[i];
        }
    }
    return NULL;  /* Not found - normal lookup behavior */
}

static word_memory_entry_t* find_memory_by_word(language_hippocampus_bridge_t* bridge, const char* word)
{
    for (uint32_t i = 0; i < bridge->config.max_word_memories; i++) {
        if (bridge->memories[i].is_valid &&
            strcmp(bridge->memories[i].memory.word, word) == 0) {
            return &bridge->memories[i];
        }
    }
    return NULL;  /* Not found - normal lookup behavior */
}

static void apply_decay(language_hippocampus_bridge_t* bridge)
{
    for (uint32_t i = 0; i < bridge->config.max_word_memories; i++) {
        if (bridge->memories[i].is_valid && !bridge->memories[i].memory.is_consolidated) {
            bridge->memories[i].memory.strength *= (1.0f - bridge->config.decay_rate);
            if (bridge->memories[i].memory.strength < 0.01f) {
                bridge->memories[i].memory.strength = 0.01f;
            }
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

language_hippocampus_config_t language_hippocampus_default_config(void)
{
    language_hippocampus_config_t config;
    memset(&config, 0, sizeof(config));

    config.update_interval_ms = LH_DEFAULT_UPDATE_INTERVAL_MS;
    config.max_word_memories = LH_DEFAULT_MAX_WORD_MEMORIES;
    config.feature_dim = LH_DEFAULT_FEATURE_DIM;

    config.consolidation_threshold = LH_DEFAULT_CONSOLIDATION_THRESHOLD;
    config.retrieval_threshold = LH_DEFAULT_RETRIEVAL_THRESHOLD;
    config.decay_rate = LH_DEFAULT_DECAY_RATE;

    config.enable_pattern_separation = true;
    config.enable_pattern_completion = true;
    config.enable_consolidation = true;
    config.enable_replay = true;
    config.enable_semantic_priming = true;
    config.enable_bio_async = true;

    return config;
}

language_hippocampus_bridge_t* language_hippocampus_bridge_create(
    language_orchestrator_t* language,
    hippocampus_adapter_t* hippocampus,
    const language_hippocampus_config_t* config)
{
    language_hippocampus_bridge_t* bridge = nimcp_calloc(1, sizeof(language_hippocampus_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = language_hippocampus_default_config();
    }

    bridge->language = language;
    bridge->hippocampus = hippocampus;

    bridge->memories = nimcp_calloc(bridge->config.max_word_memories, sizeof(word_memory_entry_t));
    bridge->associations = nimcp_calloc(MAX_ASSOCIATIONS, sizeof(association_entry_t));
    bridge->feature_buffer = nimcp_calloc(bridge->config.feature_dim, sizeof(float));

    if (!bridge->memories || !bridge->associations || !bridge->feature_buffer) {
        language_hippocampus_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_hippocampus_bridge_create: required parameter is NULL (bridge->memories, bridge->associations, bridge->feature_buffer)");
        return NULL;
    }

    bridge->state = LH_STATE_IDLE;
    bridge->next_memory_id = 1;
    bridge->is_initialized = true;

    return bridge;
}

void language_hippocampus_bridge_destroy(language_hippocampus_bridge_t* bridge)
{
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_hippocampus");

    if (bridge->memories) {
        for (uint32_t i = 0; i < bridge->config.max_word_memories; i++) {
            if (bridge->memories[i].is_valid && bridge->memories[i].memory.semantic_features) {
                nimcp_free(bridge->memories[i].memory.semantic_features);
            }
        }
        nimcp_free(bridge->memories);
    }

    if (bridge->associations) nimcp_free(bridge->associations);
    if (bridge->feature_buffer) nimcp_free(bridge->feature_buffer);

    nimcp_free(bridge);
}

int language_hippocampus_bridge_reset(language_hippocampus_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_bridge_reset: bridge is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->config.max_word_memories; i++) {
        if (bridge->memories[i].is_valid && bridge->memories[i].memory.semantic_features) {
            nimcp_free(bridge->memories[i].memory.semantic_features);
        }
        bridge->memories[i].is_valid = false;
    }

    memset(bridge->associations, 0, MAX_ASSOCIATIONS * sizeof(association_entry_t));

    bridge->memory_count = 0;
    bridge->association_count = 0;
    bridge->state = LH_STATE_IDLE;

    return 0;
}

//=============================================================================
// Connection Functions
//=============================================================================

int language_hippocampus_connect_broca(language_hippocampus_bridge_t* bridge, broca_adapter_t* broca)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_connect_broca: bridge is NULL");
        return -1;
    }
    bridge->broca = broca;
    return 0;
}

int language_hippocampus_connect_wernicke(language_hippocampus_bridge_t* bridge, wernicke_adapter_t* wernicke)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_connect_wernicke: bridge is NULL");
        return -1;
    }
    bridge->wernicke = wernicke;
    return 0;
}

int language_hippocampus_connect_bio_async(language_hippocampus_bridge_t* bridge, bio_router_t router)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_connect_bio_async: bridge is NULL");
        return -1;
    }
    bridge->router = router;
    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int language_hippocampus_bridge_update(language_hippocampus_bridge_t* bridge, uint64_t timestamp_ms)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_bridge_update: required parameter is NULL (bridge, bridge->is_initialized)");
        return -1;
    }

    bridge->last_update_ms = timestamp_ms;
    apply_decay(bridge);

    bridge->stats.bridge_state = bridge->state;
    bridge->stats.current_memory_count = bridge->memory_count;

    return 0;
}

//=============================================================================
// Memory Encoding
//=============================================================================

uint32_t language_hippocampus_encode_word(language_hippocampus_bridge_t* bridge, const encoding_request_t* request)
{
    if (!bridge || !request) return 0;

    word_memory_entry_t* existing = find_memory_by_word(bridge, request->word);
    if (existing) {
        existing->memory.strength = fminf(1.0f, existing->memory.strength + 0.1f);
        existing->memory.access_count++;
        return existing->memory.memory_id;
    }

    int slot = find_memory_slot(bridge);
    if (slot < 0) return 0;

    word_memory_entry_t* entry = &bridge->memories[slot];
    memset(&entry->memory, 0, sizeof(word_memory_t));

    entry->memory.memory_id = bridge->next_memory_id++;
    strncpy(entry->memory.word, request->word, sizeof(entry->memory.word) - 1);
    entry->memory.type = request->type;
    entry->memory.encoding = request->encoding;
    entry->memory.encoding_time_ms = nimcp_time_now_us() / 1000;
    entry->memory.last_access_ms = entry->memory.encoding_time_ms;
    entry->memory.access_count = 1;

    switch (request->encoding) {
        case ENCODING_WEAK: entry->memory.strength = 0.3f; break;
        case ENCODING_MODERATE: entry->memory.strength = 0.6f; break;
        case ENCODING_STRONG: entry->memory.strength = 0.9f; break;
        default: entry->memory.strength = 0.5f; break;
    }

    if (request->semantic_features && request->feature_count > 0) {
        uint32_t fcount = request->feature_count < bridge->config.feature_dim ?
            request->feature_count : bridge->config.feature_dim;
        entry->memory.semantic_features = nimcp_calloc(fcount, sizeof(float));
        if (entry->memory.semantic_features) {
            memcpy(entry->memory.semantic_features, request->semantic_features, fcount * sizeof(float));
            entry->memory.feature_count = fcount;
        }
    }

    entry->is_valid = true;
    bridge->memory_count++;
    bridge->stats.words_encoded++;
    bridge->stats.memories_by_type[request->type]++;

    if (bridge->hippocampus && entry->memory.semantic_features) {
        hippocampus_encode_memory(bridge->hippocampus,
            entry->memory.semantic_features,
            entry->memory.feature_count,
            NULL,
            request->emotional_valence);
    }

    if (bridge->encoding_callback) {
        bridge->encoding_callback(&entry->memory, bridge->encoding_callback_data);
    }

    return entry->memory.memory_id;
}

int language_hippocampus_encode_association(language_hippocampus_bridge_t* bridge,
    uint32_t word_a_id, uint32_t word_b_id, float strength, const char* relation_type)
{
    if (!bridge || bridge->association_count >= MAX_ASSOCIATIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "language_hippocampus_encode_association: bridge is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->association_count; i++) {
        if (bridge->associations[i].is_valid &&
            bridge->associations[i].association.word_a_id == word_a_id &&
            bridge->associations[i].association.word_b_id == word_b_id) {
            bridge->associations[i].association.strength =
                fminf(1.0f, bridge->associations[i].association.strength + strength * 0.5f);
            return 0;
        }
    }

    association_entry_t* entry = &bridge->associations[bridge->association_count];
    entry->association.word_a_id = word_a_id;
    entry->association.word_b_id = word_b_id;
    entry->association.strength = strength;
    if (relation_type) {
        strncpy(entry->association.relation_type, relation_type,
            sizeof(entry->association.relation_type) - 1);
    }
    entry->is_valid = true;
    bridge->association_count++;

    return 0;
}

int language_hippocampus_strengthen_memory(language_hippocampus_bridge_t* bridge,
    uint32_t memory_id, float strength_boost)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_strengthen_memory: bridge is NULL");
        return -1;
    }

    word_memory_entry_t* entry = find_memory_by_id(bridge, memory_id);
    if (!entry) {
        return -1;  /* Entry not found by id - normal lookup miss */
    }

    entry->memory.strength = fminf(1.0f, entry->memory.strength + strength_boost);
    entry->memory.last_access_ms = nimcp_time_now_us() / 1000;
    entry->memory.access_count++;

    return 0;
}

//=============================================================================
// Memory Retrieval
//=============================================================================

int language_hippocampus_retrieve(language_hippocampus_bridge_t* bridge,
    const retrieval_request_t* request, lh_retrieval_result_t* result)
{
    if (!bridge || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_retrieve: required parameter is NULL (bridge, request, result)");
        return -1;
    }

    bridge->state = LH_STATE_RETRIEVING;
    bridge->stats.retrieval_attempts++;

    result->memories = nimcp_calloc(request->max_results, sizeof(word_memory_t));
    result->similarities = nimcp_calloc(request->max_results, sizeof(float));
    result->count = 0;
    result->status = MEM_OP_NOT_FOUND;

    if (!result->memories || !result->similarities) {
        if (result->memories) { nimcp_free(result->memories); result->memories = NULL; }
        if (result->similarities) { nimcp_free(result->similarities); result->similarities = NULL; }
        bridge->state = LH_STATE_IDLE;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_hippocampus_retrieve: failed to allocate result buffers");
        return -1;
    }

    float threshold = request->similarity_threshold > 0 ?
        request->similarity_threshold : bridge->config.retrieval_threshold;

    for (uint32_t i = 0; i < bridge->config.max_word_memories && result->count < request->max_results; i++) {
        if (!bridge->memories[i].is_valid) continue;

        float sim = 0.0f;

        if (request->cue_type == CUE_ORTHOGRAPHIC && request->cue_string[0]) {
            if (strstr(bridge->memories[i].memory.word, request->cue_string)) {
                sim = 0.8f;
            }
        } else if (request->cue_features && request->feature_count > 0 &&
                   bridge->memories[i].memory.semantic_features) {
            sim = compute_similarity(request->cue_features,
                bridge->memories[i].memory.semantic_features,
                request->feature_count < bridge->memories[i].memory.feature_count ?
                    request->feature_count : bridge->memories[i].memory.feature_count);
        }

        if (sim >= threshold) {
            result->memories[result->count] = bridge->memories[i].memory;
            result->similarities[result->count] = sim;
            result->count++;

            bridge->memories[i].memory.last_access_ms = nimcp_time_now_us() / 1000;
            bridge->memories[i].memory.access_count++;
        }
    }

    if (result->count > 0) {
        result->status = MEM_OP_SUCCESS;
        bridge->stats.successful_retrievals++;

        float sum = 0.0f;
        for (uint32_t i = 0; i < result->count; i++) sum += result->similarities[i];
        bridge->stats.avg_retrieval_similarity =
            0.9f * bridge->stats.avg_retrieval_similarity + 0.1f * (sum / result->count);
    }

    if (bridge->retrieval_callback) {
        bridge->retrieval_callback(result, bridge->retrieval_callback_data);
    }

    bridge->state = LH_STATE_IDLE;
    return 0;
}

int language_hippocampus_retrieve_by_word(language_hippocampus_bridge_t* bridge,
    const char* word, word_memory_t* memory)
{
    if (!bridge || !word || !memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_retrieve_by_word: required parameter is NULL (bridge, word, memory)");
        return -1;
    }

    word_memory_entry_t* entry = find_memory_by_word(bridge, word);
    if (!entry) {
        return -1;  /* Entry not found by word - normal lookup miss */
    }

    *memory = entry->memory;
    entry->memory.last_access_ms = nimcp_time_now_us() / 1000;
    entry->memory.access_count++;

    return 0;
}

int language_hippocampus_retrieve_associations(language_hippocampus_bridge_t* bridge,
    uint32_t word_id, semantic_association_t* associations, uint32_t max_associations)
{
    if (!bridge || !associations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_retrieve_associations: required parameter is NULL (bridge, associations)");
        return -1;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < bridge->association_count && count < max_associations; i++) {
        if (bridge->associations[i].is_valid &&
            (bridge->associations[i].association.word_a_id == word_id ||
             bridge->associations[i].association.word_b_id == word_id)) {
            associations[count++] = bridge->associations[i].association;
        }
    }

    return (int)count;
}

int language_hippocampus_pattern_complete(language_hippocampus_bridge_t* bridge,
    const char* partial_word, char* completed_words, uint32_t max_completions)
{
    if (!bridge || !partial_word || !completed_words) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_pattern_complete: required parameter is NULL (bridge, partial_word, completed_words)");
        return -1;
    }

    uint32_t count = 0;
    size_t partial_len = strlen(partial_word);

    for (uint32_t i = 0; i < bridge->config.max_word_memories && count < max_completions; i++) {
        if (bridge->memories[i].is_valid &&
            strncmp(bridge->memories[i].memory.word, partial_word, partial_len) == 0) {
            strncpy(completed_words + count * 64, bridge->memories[i].memory.word, 63);
            completed_words[count * 64 + 63] = '\0';
            count++;
        }
    }

    return (int)count;
}

//=============================================================================
// Memory Consolidation
//=============================================================================

int language_hippocampus_trigger_consolidation(language_hippocampus_bridge_t* bridge, float strength_threshold)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_trigger_consolidation: bridge is NULL");
        return -1;
    }

    bridge->state = LH_STATE_CONSOLIDATING;
    uint32_t consolidated = 0;

    for (uint32_t i = 0; i < bridge->config.max_word_memories; i++) {
        if (bridge->memories[i].is_valid &&
            !bridge->memories[i].memory.is_consolidated &&
            bridge->memories[i].memory.strength >= strength_threshold) {

            bridge->memories[i].memory.is_consolidated = true;
            bridge->memories[i].memory.encoding = ENCODING_CONSOLIDATED;
            bridge->stats.consolidations++;
            bridge->stats.consolidated_count++;
            consolidated++;

            if (bridge->consolidation_callback) {
                consolidation_event_t event;
                event.memory_id = bridge->memories[i].memory.memory_id;
                strncpy(event.word, bridge->memories[i].memory.word, sizeof(event.word) - 1);
                event.type = bridge->memories[i].memory.type;
                event.final_strength = bridge->memories[i].memory.strength;
                event.consolidation_time_ms = nimcp_time_now_us() / 1000;
                bridge->consolidation_callback(&event, bridge->consolidation_callback_data);
            }
        }
    }

    bridge->state = LH_STATE_IDLE;
    return (int)consolidated;
}

int language_hippocampus_trigger_replay(language_hippocampus_bridge_t* bridge,
    uint32_t num_memories, bool reverse_order)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_trigger_replay: bridge is NULL");
        return -1;
    }

    if (bridge->hippocampus) {
        hippocampus_trigger_replay(bridge->hippocampus, reverse_order, num_memories);
    }

    bridge->stats.replay_events++;
    return 0;
}

bool language_hippocampus_is_consolidated(const language_hippocampus_bridge_t* bridge, uint32_t memory_id)
{
    if (!bridge) {
        return false;
    }

    for (uint32_t i = 0; i < bridge->config.max_word_memories; i++) {
        if (bridge->memories[i].is_valid && bridge->memories[i].memory.memory_id == memory_id) {
            return bridge->memories[i].memory.is_consolidated;
        }
    }
    return false;
}

//=============================================================================
// Memory Management
//=============================================================================

int language_hippocampus_get_memory(const language_hippocampus_bridge_t* bridge,
    uint32_t memory_id, word_memory_t* memory)
{
    if (!bridge || !memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_get_memory: required parameter is NULL (bridge, memory)");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->config.max_word_memories; i++) {
        if (bridge->memories[i].is_valid && bridge->memories[i].memory.memory_id == memory_id) {
            *memory = bridge->memories[i].memory;
            return 0;
        }
    }
    return -1;  /* Not found - normal lookup behavior */
}

int language_hippocampus_delete_memory(language_hippocampus_bridge_t* bridge, uint32_t memory_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_delete_memory: bridge is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->config.max_word_memories; i++) {
        if (bridge->memories[i].is_valid && bridge->memories[i].memory.memory_id == memory_id) {
            if (bridge->memories[i].memory.semantic_features) {
                nimcp_free(bridge->memories[i].memory.semantic_features);
            }
            bridge->memories[i].is_valid = false;
            bridge->memory_count--;
            return 0;
        }
    }
    return -1;  /* Not found - normal lookup behavior */
}

uint32_t language_hippocampus_get_memory_count(const language_hippocampus_bridge_t* bridge)
{
    return bridge ? bridge->memory_count : 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int language_hippocampus_set_encoding_callback(language_hippocampus_bridge_t* bridge,
    lh_encoding_callback_t callback, void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_set_encoding_callback: bridge is NULL");
        return -1;
    }
    bridge->encoding_callback = callback;
    bridge->encoding_callback_data = user_data;
    return 0;
}

int language_hippocampus_set_retrieval_callback(language_hippocampus_bridge_t* bridge,
    lh_retrieval_callback_t callback, void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_set_retrieval_callback: bridge is NULL");
        return -1;
    }
    bridge->retrieval_callback = callback;
    bridge->retrieval_callback_data = user_data;
    return 0;
}

int language_hippocampus_set_consolidation_callback(language_hippocampus_bridge_t* bridge,
    lh_consolidation_callback_t callback, void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_set_consolidation_callback: bridge is NULL");
        return -1;
    }
    bridge->consolidation_callback = callback;
    bridge->consolidation_callback_data = user_data;
    return 0;
}

//=============================================================================
// Status and Statistics
//=============================================================================

lh_bridge_state_t language_hippocampus_get_state(const language_hippocampus_bridge_t* bridge)
{
    return bridge ? bridge->state : LH_STATE_ERROR;
}

int language_hippocampus_get_stats(const language_hippocampus_bridge_t* bridge,
    language_hippocampus_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void language_hippocampus_reset_stats(language_hippocampus_bridge_t* bridge)
{
    if (bridge) memset(&bridge->stats, 0, sizeof(bridge->stats));
}

int language_hippocampus_get_config(const language_hippocampus_bridge_t* bridge,
    language_hippocampus_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    *config = bridge->config;
    return 0;
}

int language_hippocampus_set_config(language_hippocampus_bridge_t* bridge,
    const language_hippocampus_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_hippocampus_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    bridge->config = *config;
    return 0;
}
