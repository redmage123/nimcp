/**
 * @file nimcp_predictive_protocol.c
 * @brief Predictive communication protocol implementation
 *
 * WHAT: Implements pattern learning, Markov chain prediction, and LRU cache
 * WHY:  Reduce message latency by anticipating communication patterns
 * HOW:  Observe traffic -> learn patterns -> predict -> prefetch -> cache
 *
 * IMPLEMENTATION DETAILS:
 * - Pattern Storage: Hash table for O(1) pattern lookup
 * - Markov Chain: 2D transition matrix for state->state probabilities
 * - Cache: LRU cache with double-linked list + hash table
 * - Learning: Exponential moving average for frequency updates
 * - Statistics: Lock-protected counters for thread safety
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#define LOG_MODULE "predictive_protocol"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(predictive_protocol)

#include "async/nimcp_predictive_protocol.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

#define PREDICTIVE_MAGIC 0x50524544  // 'PRED'
#define MAX_PATTERN_HASH_BUCKETS 512
#define MAX_CACHE_HASH_BUCKETS 128
#define MAX_TRANSITION_HASH_BUCKETS 256  /* Hash buckets for Markov transitions */
#define DEFAULT_PREDICTION_HORIZON_MS 100
#define DEFAULT_CACHE_SIZE 256
#define DEFAULT_LEARNING_RATE NIMCP_LEARNING_RATE_COARSE
#define DEFAULT_MIN_CONFIDENCE 0.7f
#define DEFAULT_MAX_PATTERNS 1024
#define DEFAULT_MARKOV_ORDER 1

/*=============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Pattern state key (for Markov chain)
 */
typedef struct {
    bio_module_id_t source;
    bio_module_id_t target;
    bio_message_type_t msg_type;
} pattern_key_t;

/**
 * @brief Pattern entry in hash table
 */
typedef struct pattern_entry {
    pattern_key_t key;
    message_pattern_t pattern;
    struct pattern_entry* next;  /**< Hash bucket chain */
} pattern_entry_t;

/**
 * @brief Markov transition entry with hash chaining
 */
typedef struct markov_transition {
    pattern_key_t from_state;
    pattern_key_t to_state;
    uint32_t count;             /**< Transition count */
    float probability;          /**< P(to | from) */
    uint64_t last_updated_us;
    struct markov_transition* hash_next;  /**< Hash bucket chain */
} markov_transition_t;

/**
 * @brief Cache entry (LRU)
 */
typedef struct cache_entry {
    pattern_key_t key;
    void* data;
    size_t data_size;
    uint64_t prefetch_time_us;
    uint64_t last_access_us;
    bool used;                  /**< Whether data was ever retrieved */
    struct cache_entry* prev;   /**< LRU list */
    struct cache_entry* next;   /**< LRU list */
    struct cache_entry* hash_next; /**< Hash bucket chain */
} cache_entry_t;

/**
 * @brief Predictive protocol instance
 */
struct predictive_protocol_struct {
    uint32_t magic;
    predictive_config_t config;

    /* Pattern storage (hash table) */
    pattern_entry_t* pattern_buckets[MAX_PATTERN_HASH_BUCKETS];
    uint32_t pattern_count;

    /* Markov chain transitions (hash table for O(1) lookup) */
    markov_transition_t* transition_buckets[MAX_TRANSITION_HASH_BUCKETS];
    markov_transition_t* transitions;  /**< Array storage for iteration */
    uint32_t transition_count;
    uint32_t transition_capacity;

    /* LRU cache */
    cache_entry_t* cache_buckets[MAX_CACHE_HASH_BUCKETS];
    cache_entry_t* cache_head;  /**< MRU */
    cache_entry_t* cache_tail;  /**< LRU */
    uint32_t cache_count;

    /* Statistics */
    prefetch_result_t stats;

    /* Synchronization */
    nimcp_platform_mutex_t mutex;

    /* Last observed state (for Markov chain) */
    pattern_key_t last_state;
    bool has_last_state;
};

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * WHAT: Hash function for pattern keys
 * WHY:  Distribute patterns across hash buckets
 * HOW:  Simple FNV-1a hash over key fields
 */
static uint32_t hash_pattern_key(const pattern_key_t* key) {
    if (!key) return 0;

    uint32_t hash = 2166136261U;
    hash = (hash ^ key->source) * 16777619U;
    hash = (hash ^ key->target) * 16777619U;
    hash = (hash ^ key->msg_type) * 16777619U;
    return hash;
}

/**
 * WHAT: Compare pattern keys for equality
 * WHY:  Lookup patterns in hash table
 * HOW:  Compare all three fields
 */
static bool pattern_key_equal(const pattern_key_t* a, const pattern_key_t* b) {
    if (!a || !b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_key_equal: required parameter is NULL (a, b)");
        return false;
    }
    return a->source == b->source &&
           a->target == b->target &&
           a->msg_type == b->msg_type;
}

/**
 * WHAT: Hash function for Markov transition keys (from->to pair)
 * WHY:  O(1) lookup of transitions by state pair
 * HOW:  Combine hashes of from and to keys using FNV-1a
 */
static uint32_t hash_transition_key(const pattern_key_t* from, const pattern_key_t* to) {
    if (!from || !to) return 0;

    uint32_t hash = 2166136261U;
    /* Hash the 'from' state */
    hash = (hash ^ from->source) * 16777619U;
    hash = (hash ^ from->target) * 16777619U;
    hash = (hash ^ from->msg_type) * 16777619U;
    /* Hash the 'to' state */
    hash = (hash ^ to->source) * 16777619U;
    hash = (hash ^ to->target) * 16777619U;
    hash = (hash ^ to->msg_type) * 16777619U;
    return hash;
}

/**
 * WHAT: Find pattern entry in hash table
 * WHY:  Lookup existing pattern for updates
 * HOW:  Hash key, search bucket chain
 */
static pattern_entry_t* find_pattern(predictive_protocol_t proto, const pattern_key_t* key) {
    if (!proto || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_pattern: required parameter is NULL (proto, key)");
        return NULL;
    }

    uint32_t bucket = hash_pattern_key(key) % MAX_PATTERN_HASH_BUCKETS;
    pattern_entry_t* entry = proto->pattern_buckets[bucket];

    while (entry) {
        if (pattern_key_equal(&entry->key, key)) {
            return entry;
        }
        entry = entry->next;
    }

    // P2 fix: Removed false-positive NIMCP_THROW_TO_IMMUNE.
    // Not finding a pattern is normal (new pattern, first occurrence).
    return NULL;
}

/**
 * WHAT: Insert or update pattern
 * WHY:  Track message frequencies and intervals
 * HOW:  Find existing or create new, update with exponential moving average
 */
static void upsert_pattern(predictive_protocol_t proto, const pattern_key_t* key, uint64_t timestamp_us) {
    if (!proto || !key) return;

    pattern_entry_t* entry = find_pattern(proto, key);

    if (entry) {
        /* Update existing pattern */
        entry->pattern.frequency++;

        /* Update average interval with exponential moving average */
        if (entry->pattern.last_seen_us > 0) {
            float interval_ms = (timestamp_us - entry->pattern.last_seen_us) / 1000.0F;
            float alpha = proto->config.learning_rate;
            entry->pattern.avg_interval_ms = (1.0F - alpha) * entry->pattern.avg_interval_ms +
                                            alpha * interval_ms;
        }

        entry->pattern.last_seen_us = timestamp_us;
    } else {
        /* Create new pattern */
        if (proto->pattern_count >= proto->config.max_patterns) {
            LOG_WARN("Pattern table full, cannot add new pattern");
            return;
        }

        entry = nimcp_calloc(1, sizeof(pattern_entry_t));
        if (!entry) {
            LOG_ERROR("Failed to allocate pattern entry");
            return;
        }

        entry->key = *key;
        entry->pattern.source_module = key->source;
        entry->pattern.target_module = key->target;
        entry->pattern.msg_type = key->msg_type;
        entry->pattern.frequency = 1;
        entry->pattern.avg_interval_ms = 0.0F;
        entry->pattern.last_seen_us = timestamp_us;

        /* Insert into hash table */
        uint32_t bucket = hash_pattern_key(key) % MAX_PATTERN_HASH_BUCKETS;
        entry->next = proto->pattern_buckets[bucket];
        proto->pattern_buckets[bucket] = entry;

        proto->pattern_count++;
    }
}

/**
 * WHAT: Find Markov transition
 * WHY:  Lookup transition probability
 * HOW:  Hash table lookup for O(1) average case
 */
static markov_transition_t* find_transition(predictive_protocol_t proto,
                                            const pattern_key_t* from,
                                            const pattern_key_t* to) {
    if (!proto || !from || !to) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "upsert_pattern: required parameter is NULL (proto, from, to)");
        return NULL;
    }

    uint32_t bucket = hash_transition_key(from, to) % MAX_TRANSITION_HASH_BUCKETS;
    markov_transition_t* trans = proto->transition_buckets[bucket];

    while (trans) {
        if (pattern_key_equal(&trans->from_state, from) &&
            pattern_key_equal(&trans->to_state, to)) {
            return trans;
        }
        trans = trans->hash_next;
    }

    // P2 fix: Removed false-positive NIMCP_THROW_TO_IMMUNE.
    // Not finding a transition is normal (first observation of this state pair).
    return NULL;
}

/**
 * WHAT: Update Markov chain transition
 * WHY:  Learn state->state probabilities
 * HOW:  Increment count, recalculate probabilities; use hash table for O(1) insert
 */
static void update_transition(predictive_protocol_t proto,
                              const pattern_key_t* from,
                              const pattern_key_t* to,
                              uint64_t timestamp_us) {
    if (!proto || !from || !to) return;

    markov_transition_t* trans = find_transition(proto, from, to);

    if (trans) {
        /* Update existing transition */
        trans->count++;
        trans->last_updated_us = timestamp_us;
    } else {
        /* Create new transition */
        if (proto->transition_count >= proto->transition_capacity) {
            /* Resize transition array - need to rebuild hash table */
            uint32_t new_capacity = proto->transition_capacity * 2;
            markov_transition_t* new_array = nimcp_realloc(proto->transitions,
                                                           new_capacity * sizeof(markov_transition_t));
            if (!new_array) {
                LOG_ERROR("Failed to resize transition array");
                return;
            }
            proto->transitions = new_array;
            proto->transition_capacity = new_capacity;

            /* Rebuild hash table after realloc (pointers may have changed) */
            memset(proto->transition_buckets, 0, sizeof(proto->transition_buckets));
            for (uint32_t i = 0; i < proto->transition_count; i++) {
                markov_transition_t* t = &proto->transitions[i];
                uint32_t bucket = hash_transition_key(&t->from_state, &t->to_state) % MAX_TRANSITION_HASH_BUCKETS;
                t->hash_next = proto->transition_buckets[bucket];
                proto->transition_buckets[bucket] = t;
            }
        }

        trans = &proto->transitions[proto->transition_count];
        trans->from_state = *from;
        trans->to_state = *to;
        trans->count = 1;
        trans->probability = 0.0F;  // Will be calculated later
        trans->last_updated_us = timestamp_us;

        /* Insert into hash table */
        uint32_t bucket = hash_transition_key(from, to) % MAX_TRANSITION_HASH_BUCKETS;
        trans->hash_next = proto->transition_buckets[bucket];
        proto->transition_buckets[bucket] = trans;

        proto->transition_count++;
    }

    /* Recalculate probabilities from this state */
    uint32_t total_from_state = 0;
    for (uint32_t i = 0; i < proto->transition_count; i++) {
        if (pattern_key_equal(&proto->transitions[i].from_state, from)) {
            total_from_state += proto->transitions[i].count;
        }
    }

    if (total_from_state > 0) {
        for (uint32_t i = 0; i < proto->transition_count; i++) {
            if (pattern_key_equal(&proto->transitions[i].from_state, from)) {
                proto->transitions[i].probability = (float)proto->transitions[i].count / (float)total_from_state;
            }
        }
    }
}

/**
 * WHAT: Remove cache entry from LRU list
 * WHY:  Prepare for reordering or eviction
 * HOW:  Update double-linked list pointers
 */
static void cache_remove_from_lru(predictive_protocol_t proto, cache_entry_t* entry) {
    if (!proto || !entry) return;

    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        proto->cache_head = entry->next;
    }

    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        proto->cache_tail = entry->prev;
    }

    entry->prev = entry->next = NULL;
}

/**
 * WHAT: Add cache entry to head of LRU list (MRU position)
 * WHY:  Mark as most recently used
 * HOW:  Insert at head of double-linked list
 */
static void cache_add_to_head(predictive_protocol_t proto, cache_entry_t* entry) {
    if (!proto || !entry) return;

    entry->prev = NULL;
    entry->next = proto->cache_head;

    if (proto->cache_head) {
        proto->cache_head->prev = entry;
    }
    proto->cache_head = entry;

    if (!proto->cache_tail) {
        proto->cache_tail = entry;
    }
}

/**
 * WHAT: Find cache entry
 * WHY:  Check if data is already prefetched
 * HOW:  Hash lookup in cache buckets
 */
static cache_entry_t* find_cache_entry(predictive_protocol_t proto, const pattern_key_t* key) {
    if (!proto || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_cache_entry: required parameter is NULL (proto, key)");
        return NULL;
    }

    uint32_t bucket = hash_pattern_key(key) % MAX_CACHE_HASH_BUCKETS;
    cache_entry_t* entry = proto->cache_buckets[bucket];

    while (entry) {
        if (pattern_key_equal(&entry->key, key)) {
            return entry;
        }
        entry = entry->hash_next;
    }

    // P2 fix: Removed false-positive NIMCP_THROW_TO_IMMUNE.
    // Cache miss is normal and expected (first access or evicted entry).
    return NULL;
}

/**
 * WHAT: Evict LRU cache entry
 * WHY:  Make room for new prefetch
 * HOW:  Remove tail, free data, update stats
 */
static void cache_evict_lru(predictive_protocol_t proto) {
    if (!proto || !proto->cache_tail) return;

    cache_entry_t* victim = proto->cache_tail;

    /* Track wasted prefetch if never used */
    if (!victim->used) {
        proto->stats.wasted_prefetches++;
    }

    /* Remove from LRU list */
    cache_remove_from_lru(proto, victim);

    /* Remove from hash bucket */
    uint32_t bucket = hash_pattern_key(&victim->key) % MAX_CACHE_HASH_BUCKETS;
    cache_entry_t** ptr = &proto->cache_buckets[bucket];
    while (*ptr) {
        if (*ptr == victim) {
            *ptr = victim->hash_next;
            break;
        }
        ptr = &(*ptr)->hash_next;
    }

    /* Free data and entry */
    if (victim->data) {
        nimcp_free(victim->data);
    }
    nimcp_free(victim);

    proto->cache_count--;
}

/*=============================================================================
 * LIFECYCLE API
 *============================================================================*/

predictive_config_t predictive_protocol_default_config(void) {
    predictive_config_t config = {
        .prediction_horizon_ms = DEFAULT_PREDICTION_HORIZON_MS,
        .cache_size = DEFAULT_CACHE_SIZE,
        .learning_rate = DEFAULT_LEARNING_RATE,
        .min_confidence = DEFAULT_MIN_CONFIDENCE,
        .max_patterns = DEFAULT_MAX_PATTERNS,
        .markov_order = DEFAULT_MARKOV_ORDER,
        .enable_statistics = true
    };
    return config;
}

predictive_protocol_t predictive_protocol_create(const predictive_config_t* config) {
    predictive_config_t cfg = config ? *config : predictive_protocol_default_config();
    bool mutex_initialized = false;

    /* nimcp_calloc zeroes all pointers for safe cleanup */
    predictive_protocol_t proto = nimcp_calloc(1, sizeof(struct predictive_protocol_struct));
    if (!proto) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "predictive_protocol_create: failed to allocate predictive protocol");
        LOG_ERROR("Failed to allocate predictive protocol");
        return NULL;
    }

    proto->magic = PREDICTIVE_MAGIC;
    proto->config = cfg;

    /* Allocate transition array */
    proto->transition_capacity = 1024;  // Initial capacity
    proto->transitions = nimcp_calloc(proto->transition_capacity, sizeof(markov_transition_t));
    if (!proto->transitions) goto cleanup;

    /* Initialize mutex */
    if (nimcp_platform_mutex_init(&proto->mutex, false) != 0) goto cleanup;
    mutex_initialized = true;

    proto->has_last_state = false;

    LOG_INFO("Created predictive protocol (cache_size=%u, learning_rate=%.2f, min_confidence=%.2f)",
             cfg.cache_size, cfg.learning_rate, cfg.min_confidence);

    return proto;

cleanup:
    LOG_ERROR("Failed to allocate predictive protocol resources");
    if (mutex_initialized) {
        nimcp_platform_mutex_destroy(&proto->mutex);
    }
    nimcp_free(proto->transitions);
    nimcp_free(proto);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_protocol_create: allocation failed");
    return NULL;
}

void predictive_protocol_destroy(predictive_protocol_t proto) {
    if (!proto || proto->magic != PREDICTIVE_MAGIC) return;

    LOG_DEBUG("Destroying predictive protocol (patterns=%u, transitions=%u, cache=%u)",
              proto->pattern_count, proto->transition_count, proto->cache_count);

    nimcp_platform_mutex_lock(&proto->mutex);

    /* Free patterns */
    for (uint32_t i = 0; i < MAX_PATTERN_HASH_BUCKETS; i++) {
        pattern_entry_t* entry = proto->pattern_buckets[i];
        while (entry) {
            pattern_entry_t* next = entry->next;
            nimcp_free(entry);
            entry = next;
        }
    }

    /* Free transitions */
    if (proto->transitions) {
        nimcp_free(proto->transitions);
    }

    /* Free cache entries */
    cache_entry_t* cache_entry = proto->cache_head;
    while (cache_entry) {
        cache_entry_t* next = cache_entry->next;
        if (cache_entry->data) {
            nimcp_free(cache_entry->data);
        }
        nimcp_free(cache_entry);
        cache_entry = next;
    }

    proto->magic = 0;

    nimcp_platform_mutex_unlock(&proto->mutex);
    nimcp_platform_mutex_destroy(&proto->mutex);

    nimcp_free(proto);
}

/*=============================================================================
 * OBSERVATION AND LEARNING API
 *============================================================================*/

int predictive_protocol_observe(predictive_protocol_t proto,
                                 const bio_message_header_t* msg_header) {
    if (!proto || proto->magic != PREDICTIVE_MAGIC || !msg_header) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_destroy: required parameter is NULL (proto, msg_header)");
        return -1;
    }

    nimcp_platform_mutex_lock(&proto->mutex);

    uint64_t now = nimcp_platform_time_monotonic_us();

    /* Extract pattern key */
    pattern_key_t key = {
        .source = msg_header->source_module,
        .target = msg_header->target_module,
        .msg_type = msg_header->type
    };

    /* Update pattern frequencies */
    upsert_pattern(proto, &key, now);

    /* Update Markov chain if we have a previous state */
    if (proto->has_last_state) {
        update_transition(proto, &proto->last_state, &key, now);
    }

    /* Update last state */
    proto->last_state = key;
    proto->has_last_state = true;

    nimcp_platform_mutex_unlock(&proto->mutex);

    LOG_TRACE("Observed message: source=%u, target=%u, type=0x%04X",
              key.source, key.target, key.msg_type);

    return 0;
}

void predictive_protocol_reset_patterns(predictive_protocol_t proto) {
    if (!proto || proto->magic != PREDICTIVE_MAGIC) return;

    nimcp_platform_mutex_lock(&proto->mutex);

    LOG_INFO("Resetting predictive protocol patterns");

    /* Free all patterns */
    for (uint32_t i = 0; i < MAX_PATTERN_HASH_BUCKETS; i++) {
        pattern_entry_t* entry = proto->pattern_buckets[i];
        while (entry) {
            pattern_entry_t* next = entry->next;
            nimcp_free(entry);
            entry = next;
        }
        proto->pattern_buckets[i] = NULL;
    }
    proto->pattern_count = 0;

    /* Reset transitions */
    proto->transition_count = 0;
    memset(proto->transitions, 0, proto->transition_capacity * sizeof(markov_transition_t));

    /* Clear cache */
    cache_entry_t* cache_entry = proto->cache_head;
    while (cache_entry) {
        cache_entry_t* next = cache_entry->next;
        if (cache_entry->data) {
            nimcp_free(cache_entry->data);
        }
        nimcp_free(cache_entry);
        cache_entry = next;
    }
    proto->cache_head = proto->cache_tail = NULL;
    proto->cache_count = 0;
    memset(proto->cache_buckets, 0, sizeof(proto->cache_buckets));

    /* Reset last state */
    proto->has_last_state = false;

    nimcp_platform_mutex_unlock(&proto->mutex);
}

/*=============================================================================
 * PREDICTION API
 *============================================================================*/

/**
 * WHAT: Comparison function for sorting predictions by confidence
 * WHY:  Return highest confidence predictions first
 * HOW:  qsort-compatible comparison
 */
static int compare_predictions(const void* a, const void* b) {
    /* P1-49 fix: Removed NIMCP_THROW_TO_IMMUNE from qsort comparator.
     * qsort calls this O(N log N) times per sort - throwing here would
     * generate massive false positive noise. Return 0 for NULL inputs. */
    if (!a || !b) return 0;

    const prediction_t* pa = (const prediction_t*)a;
    const prediction_t* pb = (const prediction_t*)b;

    if (pa->confidence > pb->confidence) return -1;
    if (pa->confidence < pb->confidence) return 1;
    return 0;
}

uint32_t predictive_protocol_predict_next(predictive_protocol_t proto,
                                           const bio_message_header_t* current_msg,
                                           prediction_t* predictions,
                                           uint32_t max_predictions) {
    if (!proto || proto->magic != PREDICTIVE_MAGIC || !current_msg || !predictions || max_predictions == 0) {
        return 0;
    }

    nimcp_platform_mutex_lock(&proto->mutex);

    pattern_key_t current_key = {
        .source = current_msg->source_module,
        .target = current_msg->target_module,
        .msg_type = current_msg->type
    };

    /* Find all transitions from current state */
    uint32_t pred_count = 0;
    for (uint32_t i = 0; i < proto->transition_count && pred_count < max_predictions; i++) {
        markov_transition_t* trans = &proto->transitions[i];

        if (pattern_key_equal(&trans->from_state, &current_key)) {
            /* Check if confidence meets threshold */
            if (trans->probability >= proto->config.min_confidence) {
                predictions[pred_count].predicted_msg_type = trans->to_state.msg_type;
                predictions[pred_count].predicted_target = trans->to_state.target;
                predictions[pred_count].confidence = trans->probability;
                predictions[pred_count].prefetch_size = 1024;  // Default estimate
                predictions[pred_count].prefetch_data = NULL;
                pred_count++;
            }
        }
    }

    /* Sort by confidence (descending) */
    if (pred_count > 1) {
        qsort(predictions, pred_count, sizeof(prediction_t), compare_predictions);
    }

    if (proto->config.enable_statistics) {
        proto->stats.predictions_made += pred_count;
    }

    nimcp_platform_mutex_unlock(&proto->mutex);

    LOG_DEBUG("Generated %u predictions from state (source=%u, target=%u, type=0x%04X)",
              pred_count, current_key.source, current_key.target, current_key.msg_type);

    return pred_count;
}

float predictive_protocol_get_confidence(predictive_protocol_t proto,
                                         const bio_message_header_t* current_msg,
                                         bio_message_type_t next_type,
                                         bio_module_id_t next_target) {
    if (!proto || proto->magic != PREDICTIVE_MAGIC || !current_msg) return -1.0F;

    nimcp_platform_mutex_lock(&proto->mutex);

    pattern_key_t from_key = {
        .source = current_msg->source_module,
        .target = current_msg->target_module,
        .msg_type = current_msg->type
    };

    pattern_key_t to_key = {
        .source = current_msg->target_module,  // Next message source is current target
        .target = next_target,
        .msg_type = next_type
    };

    markov_transition_t* trans = find_transition(proto, &from_key, &to_key);

    float confidence = trans ? trans->probability : -1.0F;

    nimcp_platform_mutex_unlock(&proto->mutex);

    return confidence;
}

/*=============================================================================
 * PREFETCH API
 *============================================================================*/

int predictive_protocol_prefetch(predictive_protocol_t proto, const prediction_t* prediction) {
    if (!proto || proto->magic != PREDICTIVE_MAGIC || !prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_prefetch: required parameter is NULL (proto, prediction)");
        return -1;
    }

    nimcp_platform_mutex_lock(&proto->mutex);

    pattern_key_t key = {
        .source = 0,  // Not known for prefetch
        .target = prediction->predicted_target,
        .msg_type = prediction->predicted_msg_type
    };

    /* Check if already cached */
    if (find_cache_entry(proto, &key)) {
        nimcp_platform_mutex_unlock(&proto->mutex);
        LOG_TRACE("Already cached: target=%u, type=0x%04X", key.target, key.msg_type);
        return 0;
    }

    /* Evict LRU if cache is full */
    if (proto->cache_count >= proto->config.cache_size) {
        cache_evict_lru(proto);
    }

    /* Create cache entry */
    cache_entry_t* entry = nimcp_calloc(1, sizeof(cache_entry_t));
    if (!entry) {
        nimcp_platform_mutex_unlock(&proto->mutex);
        LOG_ERROR("Failed to allocate cache entry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_protocol_prefetch: entry is NULL");
        return -1;
    }

    entry->key = key;
    entry->data = NULL;  // No actual data in this simplified implementation
    entry->data_size = prediction->prefetch_size;
    entry->prefetch_time_us = nimcp_platform_time_monotonic_us();
    entry->last_access_us = entry->prefetch_time_us;
    entry->used = false;

    /* Insert into hash table */
    uint32_t bucket = hash_pattern_key(&key) % MAX_CACHE_HASH_BUCKETS;
    entry->hash_next = proto->cache_buckets[bucket];
    proto->cache_buckets[bucket] = entry;

    /* Add to head of LRU list */
    cache_add_to_head(proto, entry);

    proto->cache_count++;

    if (proto->config.enable_statistics) {
        proto->stats.prefetches_attempted++;
    }

    nimcp_platform_mutex_unlock(&proto->mutex);

    LOG_DEBUG("Prefetched: target=%u, type=0x%04X, confidence=%.2f",
              key.target, key.msg_type, prediction->confidence);

    return 0;
}

void* predictive_protocol_get_prefetched(predictive_protocol_t proto,
                                         bio_message_type_t msg_type,
                                         bio_module_id_t target) {
    if (!proto || proto->magic != PREDICTIVE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_prefetch: proto is NULL");
        return NULL;
    }

    nimcp_platform_mutex_lock(&proto->mutex);

    pattern_key_t key = {
        .source = 0,  // Don't care about source for lookup
        .target = target,
        .msg_type = msg_type
    };

    cache_entry_t* entry = find_cache_entry(proto, &key);

    void* data = NULL;
    if (entry) {
        /* Cache hit */
        data = entry->data;
        entry->used = true;
        entry->last_access_us = nimcp_platform_time_monotonic_us();

        /* Move to head (MRU) */
        cache_remove_from_lru(proto, entry);
        cache_add_to_head(proto, entry);

        if (proto->config.enable_statistics) {
            proto->stats.cache_hits++;

            /* Calculate latency saved (estimate) */
            uint64_t age_us = entry->last_access_us - entry->prefetch_time_us;
            float latency_saved_ms = age_us / 1000.0F;
            proto->stats.avg_latency_saved_ms = (proto->stats.avg_latency_saved_ms * 0.9F) +
                                               (latency_saved_ms * 0.1F);
        }

        LOG_TRACE("Cache HIT: target=%u, type=0x%04X", target, msg_type);
    } else {
        /* Cache miss */
        if (proto->config.enable_statistics) {
            proto->stats.cache_misses++;
        }

        LOG_TRACE("Cache MISS: target=%u, type=0x%04X", target, msg_type);
    }

    /* Update hit rate */
    if (proto->config.enable_statistics && (proto->stats.cache_hits + proto->stats.cache_misses) > 0) {
        proto->stats.hit_rate = (float)proto->stats.cache_hits /
                               (float)(proto->stats.cache_hits + proto->stats.cache_misses);
    }

    nimcp_platform_mutex_unlock(&proto->mutex);

    return data;
}

uint32_t predictive_protocol_invalidate(predictive_protocol_t proto, bio_message_type_t msg_type) {
    if (!proto || proto->magic != PREDICTIVE_MAGIC) return 0;

    nimcp_platform_mutex_lock(&proto->mutex);

    uint32_t invalidated = 0;

    for (uint32_t i = 0; i < MAX_CACHE_HASH_BUCKETS; i++) {
        cache_entry_t** ptr = &proto->cache_buckets[i];
        while (*ptr) {
            cache_entry_t* entry = *ptr;

            bool should_invalidate = (msg_type == 0) || (entry->key.msg_type == msg_type);

            if (should_invalidate) {
                /* Remove from hash bucket */
                *ptr = entry->hash_next;

                /* Remove from LRU list */
                cache_remove_from_lru(proto, entry);

                /* Track wasted prefetch if never used */
                if (!entry->used && proto->config.enable_statistics) {
                    proto->stats.wasted_prefetches++;
                }

                /* Free data and entry */
                if (entry->data) {
                    nimcp_free(entry->data);
                }
                nimcp_free(entry);

                proto->cache_count--;
                invalidated++;
            } else {
                ptr = &entry->hash_next;
            }
        }
    }

    nimcp_platform_mutex_unlock(&proto->mutex);

    LOG_DEBUG("Invalidated %u cache entries (msg_type=0x%04X)", invalidated, msg_type);

    return invalidated;
}

/*=============================================================================
 * STATISTICS API
 *============================================================================*/

int predictive_protocol_get_stats(predictive_protocol_t proto, prefetch_result_t* stats) {
    if (!proto || proto->magic != PREDICTIVE_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_get_stats: required parameter is NULL (proto, stats)");
        return -1;
    }

    nimcp_platform_mutex_lock(&proto->mutex);

    *stats = proto->stats;
    stats->current_cache_size = proto->cache_count;

    /* Calculate prediction accuracy (simplified estimate) */
    if (stats->predictions_made > 0) {
        stats->prediction_accuracy = (float)stats->cache_hits / (float)stats->predictions_made;
        if (stats->prediction_accuracy > 1.0F) {
            stats->prediction_accuracy = 1.0F;
        }
    }

    nimcp_platform_mutex_unlock(&proto->mutex);

    return 0;
}

void predictive_protocol_reset_stats(predictive_protocol_t proto) {
    if (!proto || proto->magic != PREDICTIVE_MAGIC) return;

    nimcp_platform_mutex_lock(&proto->mutex);

    memset(&proto->stats, 0, sizeof(proto->stats));

    nimcp_platform_mutex_unlock(&proto->mutex);

    LOG_DEBUG("Reset statistics");
}

/*=============================================================================
 * PATTERN QUERY API
 *============================================================================*/

uint32_t predictive_protocol_get_patterns(predictive_protocol_t proto,
                                          message_pattern_t* patterns,
                                          uint32_t max_patterns) {
    if (!proto || proto->magic != PREDICTIVE_MAGIC || !patterns || max_patterns == 0) {
        return 0;
    }

    nimcp_platform_mutex_lock(&proto->mutex);

    uint32_t count = 0;

    for (uint32_t i = 0; i < MAX_PATTERN_HASH_BUCKETS && count < max_patterns; i++) {
        pattern_entry_t* entry = proto->pattern_buckets[i];
        while (entry && count < max_patterns) {
            patterns[count++] = entry->pattern;
            entry = entry->next;
        }
    }

    nimcp_platform_mutex_unlock(&proto->mutex);

    return count;
}

int predictive_protocol_get_pattern(predictive_protocol_t proto,
                                    bio_module_id_t source,
                                    bio_module_id_t target,
                                    bio_message_type_t msg_type,
                                    message_pattern_t* pattern) {
    if (!proto || proto->magic != PREDICTIVE_MAGIC || !pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_reset_stats: required parameter is NULL (proto, pattern)");
        return -1;
    }

    nimcp_platform_mutex_lock(&proto->mutex);

    pattern_key_t key = {
        .source = source,
        .target = target,
        .msg_type = msg_type
    };

    pattern_entry_t* entry = find_pattern(proto, &key);

    int result = -1;
    if (entry) {
        *pattern = entry->pattern;
        result = 0;
    }

    nimcp_platform_mutex_unlock(&proto->mutex);

    return result;
}

/*=============================================================================
 * KNOWLEDGE GRAPH SELF-AWARENESS INTEGRATION
 *============================================================================*/

/**
 * @brief Query self-knowledge from the knowledge graph
 *
 * WHAT: Retrieves structural self-knowledge about the Predictive_Protocol module
 * WHY:  Enables runtime introspection and self-awareness capabilities
 * HOW:  Queries KG for Predictive_Protocol entity and logs observations/relations
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge was found, 0 otherwise
 */
int predictive_protocol_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Predictive_Protocol");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("Predictive_Protocol self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Predictive_Protocol");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Predictive_Protocol");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
