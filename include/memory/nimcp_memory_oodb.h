#ifndef NIMCP_MEMORY_OODB_H
#define NIMCP_MEMORY_OODB_H

/**
 * @file nimcp_memory_oodb.h
 * @brief Object-Oriented Database cache for the memory store.
 *
 * In-memory object graph with lazy loading from SQLite and write-back
 * on checkpoint. Objects have direct pointer references (no SQL joins
 * for graph traversal). Polymorphic base class (memory_object_t) with
 * typed subclasses for engrams, concepts, relations, and autobio.
 *
 * Architecture:
 *   brain → OODB cache (in-memory, pointer chasing) → SQLite (persistent)
 *   Reads: cache first, load from SQLite on miss
 *   Writes: cache + dirty flag, flush to SQLite on checkpoint
 *   Graph: direct pointer traversal, O(1) per hop
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct nimcp_memory_store nimcp_memory_store_t;
typedef struct nimcp_oodb nimcp_oodb_t;

/* ============================================================================
 * Object Types & Base
 * ============================================================================ */

typedef enum {
    OODB_TYPE_ENGRAM = 0,
    OODB_TYPE_CONCEPT = 1,
    OODB_TYPE_RELATION = 2,
    OODB_TYPE_AUTOBIO = 3
} oodb_object_type_t;

typedef enum {
    OODB_STATE_STUB = 0,     /* ID only — not loaded from DB yet */
    OODB_STATE_LOADED,       /* Fully loaded from DB */
    OODB_STATE_DIRTY,        /* Modified — needs write-back */
    OODB_STATE_NEW           /* Created in memory — never persisted */
} oodb_object_state_t;

/**
 * @brief Base memory object — all types embed this as first field.
 * Enables polymorphic handling via type tag + pointer casting.
 */
typedef struct oodb_object {
    uint64_t id;
    oodb_object_type_t type;
    oodb_object_state_t state;
    uint64_t timestamp_us;
    float importance;
    uint32_t source_device_id;
    uint32_t training_step;
    uint32_t curriculum_stage;
    char tags[512];

    /* LRU tracking */
    uint64_t last_access_us;
    uint32_t access_count;

    /* Linked list for LRU eviction */
    struct oodb_object* lru_prev;
    struct oodb_object* lru_next;
} oodb_object_t;

/**
 * @brief Engram object — neural activation pattern
 */
typedef struct {
    oodb_object_t base;              /* Must be first for polymorphism */

    uint32_t neuron_count;
    uint32_t* neuron_ids;
    float* activations;
    float* embedding;
    uint32_t embedding_dim;

    float valence, arousal, intensity;
    float consolidation_strength;
    float decay_rate, vividness;
    uint32_t recall_count;
    uint32_t memory_type;
    uint32_t engram_state;
    char label[256];

    /* Direct pointer to linked concept (NULL if none) */
    struct oodb_concept_object* linked_concept;
} oodb_engram_t;

/**
 * @brief Concept object — semantic knowledge node
 */
typedef struct oodb_concept_object {
    oodb_object_t base;

    char label[256];
    uint32_t category;
    float* embedding;
    uint32_t embedding_dim;
    float base_activation;
    uint32_t access_count_semantic;

    /* Direct pointer to source engram */
    oodb_engram_t* source_engram;

    /* Direct pointers to outgoing relations (up to 64) */
    struct oodb_relation_object* relations[64];
    uint32_t relation_count;
} oodb_concept_t;

/**
 * @brief Relation object — knowledge graph edge
 */
typedef struct oodb_relation_object {
    oodb_object_t base;

    oodb_concept_t* source;          /* Direct pointer — no ID lookup */
    oodb_concept_t* target;          /* Direct pointer — no ID lookup */
    uint32_t relation_type;
    float strength;
} oodb_relation_t;

/**
 * @brief Autobiographical memory object
 */
typedef struct {
    oodb_object_t base;

    uint32_t memory_type;
    char what_happened[512];
    char why_it_happened[512];
    char outcome[256];
    int32_t valence;
    float arousal, emotional_intensity;
    bool identity_defining;
    bool is_core_memory;
} oodb_autobio_t;

/* ============================================================================
 * OODB Configuration
 * ============================================================================ */

typedef struct {
    uint32_t max_cached_objects;      /* LRU cache size (default: 10000) */
    uint32_t max_engrams;             /* Engram cache limit (default: 4096) */
    uint32_t max_concepts;            /* Concept cache limit (default: 2048) */
    bool lazy_loading;                /* Load from SQLite on first access (default: true) */
    bool write_back;                  /* Dirty objects flush on checkpoint (default: true) */
    bool auto_link;                   /* Auto-create pointers between objects (default: true) */
} nimcp_oodb_config_t;

typedef struct {
    uint32_t cached_objects;
    uint32_t cached_engrams;
    uint32_t cached_concepts;
    uint32_t cached_relations;
    uint32_t cached_autobio;
    uint32_t dirty_objects;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t evictions;
    uint64_t flushes;
} nimcp_oodb_stats_t;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_oodb_config_t nimcp_oodb_config_default(void);

nimcp_oodb_t* nimcp_oodb_create(
    const nimcp_oodb_config_t* config,
    nimcp_memory_store_t* backing_store);  /* SQLite store for persistence */

void nimcp_oodb_destroy(nimcp_oodb_t* oodb);

/* ============================================================================
 * Object Access (cache-first, lazy-load from SQLite on miss)
 * ============================================================================ */

oodb_engram_t* nimcp_oodb_get_engram(nimcp_oodb_t* oodb, uint64_t id);
oodb_concept_t* nimcp_oodb_get_concept(nimcp_oodb_t* oodb, uint64_t id);
oodb_relation_t* nimcp_oodb_get_relation(nimcp_oodb_t* oodb, uint64_t id);
oodb_autobio_t* nimcp_oodb_get_autobio(nimcp_oodb_t* oodb, uint64_t id);

/* ============================================================================
 * Object Creation (creates in cache as NEW, persists on flush)
 * ============================================================================ */

oodb_engram_t* nimcp_oodb_create_engram(nimcp_oodb_t* oodb, uint64_t id);
oodb_concept_t* nimcp_oodb_create_concept(nimcp_oodb_t* oodb, uint64_t id);
oodb_relation_t* nimcp_oodb_create_relation(nimcp_oodb_t* oodb, uint64_t id);
oodb_autobio_t* nimcp_oodb_create_autobio(nimcp_oodb_t* oodb, uint64_t id);

/* Mark object as modified (will be written back on flush) */
void nimcp_oodb_mark_dirty(oodb_object_t* obj);

/* ============================================================================
 * Graph Traversal (direct pointer chasing, no SQL)
 * ============================================================================ */

/**
 * @brief Traverse concept graph via direct pointers.
 * Returns array of concept pointers reachable within max_hops.
 * O(E) where E = edges traversed — no SQL round-trips.
 */
typedef struct {
    oodb_concept_t** concepts;
    uint32_t* depths;
    float* cumulative_strength;
    uint32_t count;
    uint32_t capacity;
} nimcp_oodb_traversal_t;

nimcp_oodb_traversal_t* nimcp_oodb_traverse(
    nimcp_oodb_t* oodb,
    oodb_concept_t* start,
    uint32_t max_hops,
    float min_strength);

void nimcp_oodb_traversal_destroy(nimcp_oodb_traversal_t* result);

/* ============================================================================
 * Tag Search (in-memory, across all cached objects)
 * ============================================================================ */

typedef struct {
    oodb_object_t** objects;
    uint32_t count;
    uint32_t capacity;
} nimcp_oodb_search_result_t;

nimcp_oodb_search_result_t* nimcp_oodb_search_by_tag(
    nimcp_oodb_t* oodb, const char* tag, uint32_t max_results);

nimcp_oodb_search_result_t* nimcp_oodb_search_by_type(
    nimcp_oodb_t* oodb, oodb_object_type_t type, uint32_t max_results);

void nimcp_oodb_search_result_destroy(nimcp_oodb_search_result_t* result);

/* ============================================================================
 * Persistence (flush dirty objects to SQLite)
 * ============================================================================ */

/** Flush all dirty objects to the backing SQLite store */
int nimcp_oodb_flush(nimcp_oodb_t* oodb);

/** Pre-warm cache: load recent/important objects from SQLite */
int nimcp_oodb_prewarm(nimcp_oodb_t* oodb, uint32_t count);

int nimcp_oodb_get_stats(const nimcp_oodb_t* oodb, nimcp_oodb_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEMORY_OODB_H */
