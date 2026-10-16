/**
 * @file nimcp_memory_oodb.c
 * @brief Object-Oriented Database in-memory cache for the memory store
 *
 * WHAT: In-memory object graph with LRU eviction, lazy loading from SQLite,
 *       and write-back on flush. Objects have direct pointer references for
 *       O(1) graph traversal.
 * WHY:  SQL joins are too slow for real-time graph traversal during inference.
 *       The OODB cache keeps hot objects in memory with direct pointer links,
 *       falling back to SQLite on cache miss.
 * HOW:  Per-type arrays with linear scan, doubly-linked LRU list for eviction,
 *       BFS traversal via direct concept->relations[] pointers.
 */

#include "memory/nimcp_memory_oodb.h"
#include "memory/nimcp_memory_store.h"
#include "utils/memory/nimcp_memory.h"

#include <string.h>
#include <time.h>

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct nimcp_oodb {
    nimcp_oodb_config_t config;
    nimcp_memory_store_t* backing_store;  /* SQLite (not owned) */

    /* Per-type cache arrays */
    oodb_engram_t** engram_cache;
    uint32_t engram_count;
    oodb_concept_t** concept_cache;
    uint32_t concept_count;
    oodb_relation_t** relation_cache;
    uint32_t relation_count;
    oodb_autobio_t** autobio_cache;
    uint32_t autobio_count;

    /* LRU list (doubly-linked via base objects) */
    oodb_object_t* lru_head;  /* Most recently accessed */
    oodb_object_t* lru_tail;  /* Least recently accessed */
    uint32_t total_cached;

    /* Stats */
    nimcp_oodb_stats_t stats;
};

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t oodb_now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/** Remove an object from the LRU list without freeing it. */
static void lru_remove(nimcp_oodb_t* oodb, oodb_object_t* obj) {
    if (!obj) return;

    if (obj->lru_prev) {
        obj->lru_prev->lru_next = obj->lru_next;
    } else {
        oodb->lru_head = obj->lru_next;
    }

    if (obj->lru_next) {
        obj->lru_next->lru_prev = obj->lru_prev;
    } else {
        oodb->lru_tail = obj->lru_prev;
    }

    obj->lru_prev = NULL;
    obj->lru_next = NULL;
}

/** Push an object to the front (MRU position) of the LRU list. */
static void lru_push_front(nimcp_oodb_t* oodb, oodb_object_t* obj) {
    if (!obj) return;

    obj->lru_prev = NULL;
    obj->lru_next = oodb->lru_head;

    if (oodb->lru_head) {
        oodb->lru_head->lru_prev = obj;
    }
    oodb->lru_head = obj;

    if (!oodb->lru_tail) {
        oodb->lru_tail = obj;
    }
}

/** Touch an object: update access time and move to MRU position. */
static void lru_touch(nimcp_oodb_t* oodb, oodb_object_t* obj) {
    if (!obj) return;
    obj->last_access_us = oodb_now_us();
    obj->access_count++;
    lru_remove(oodb, obj);
    lru_push_front(oodb, obj);
}

/** Free an object's owned heap allocations (arrays, embeddings). */
static void oodb_free_object_data(oodb_object_t* obj) {
    if (!obj) return;

    switch (obj->type) {
        case OODB_TYPE_ENGRAM: {
            oodb_engram_t* e = (oodb_engram_t*)obj;
            if (e->neuron_ids) nimcp_free(e->neuron_ids);
            if (e->activations) nimcp_free(e->activations);
            if (e->embedding) nimcp_free(e->embedding);
            break;
        }
        case OODB_TYPE_CONCEPT: {
            oodb_concept_t* c = (oodb_concept_t*)obj;
            if (c->embedding) nimcp_free(c->embedding);
            break;
        }
        case OODB_TYPE_RELATION:
        case OODB_TYPE_AUTOBIO:
            /* No heap allocations */
            break;
    }
}

/** Flush a single dirty/new object to the backing store. Returns 0 on success. */
static int oodb_flush_object(nimcp_oodb_t* oodb, oodb_object_t* obj) {
    if (!oodb->backing_store) return -1;
    if (obj->state != OODB_STATE_DIRTY && obj->state != OODB_STATE_NEW) return 0;

    int rc = 0;
    switch (obj->type) {
        case OODB_TYPE_ENGRAM: {
            oodb_engram_t* e = (oodb_engram_t*)obj;
            nimcp_engram_record_t rec;
            memset(&rec, 0, sizeof(rec));
            rec.engram_id = e->base.id;
            rec.timestamp_us = e->base.timestamp_us;
            rec.memory_type = e->memory_type;
            rec.state = e->engram_state;
            rec.neuron_count = e->neuron_count;
            rec.neuron_ids = e->neuron_ids;
            rec.activations = e->activations;
            rec.embedding = e->embedding;
            rec.embedding_dim = e->embedding_dim;
            rec.valence = e->valence;
            rec.arousal = e->arousal;
            rec.intensity = e->intensity;
            rec.consolidation_strength = e->consolidation_strength;
            rec.decay_rate = e->decay_rate;
            rec.vividness = e->vividness;
            rec.importance = e->base.importance;
            rec.recall_count = e->recall_count;
            rec.source_device_id = e->base.source_device_id;
            strncpy(rec.label, e->label, sizeof(rec.label) - 1);
            rc = nimcp_memory_store_engram_put(oodb->backing_store, &rec);
            break;
        }
        case OODB_TYPE_CONCEPT: {
            oodb_concept_t* c = (oodb_concept_t*)obj;
            nimcp_concept_record_t rec;
            memset(&rec, 0, sizeof(rec));
            rec.concept_id = c->base.id;
            rec.timestamp_us = c->base.timestamp_us;
            strncpy(rec.label, c->label, sizeof(rec.label) - 1);
            rec.category = c->category;
            rec.embedding = c->embedding;
            rec.embedding_dim = c->embedding_dim;
            rec.base_activation = c->base_activation;
            rec.access_count = c->access_count_semantic;
            rec.source_device_id = c->base.source_device_id;
            if (c->source_engram) {
                rec.source_engram_id = c->source_engram->base.id;
            }
            rc = nimcp_memory_store_concept_put(oodb->backing_store, &rec);
            break;
        }
        case OODB_TYPE_RELATION: {
            oodb_relation_t* r = (oodb_relation_t*)obj;
            nimcp_relation_record_t rec;
            memset(&rec, 0, sizeof(rec));
            rec.relation_id = r->base.id;
            rec.relation_type = r->relation_type;
            rec.strength = r->strength;
            rec.timestamp_us = r->base.timestamp_us;
            if (r->source) rec.source_concept_id = r->source->base.id;
            if (r->target) rec.target_concept_id = r->target->base.id;
            rc = nimcp_memory_store_relation_put(oodb->backing_store, &rec);
            break;
        }
        case OODB_TYPE_AUTOBIO: {
            oodb_autobio_t* a = (oodb_autobio_t*)obj;
            nimcp_autobio_record_t rec;
            memset(&rec, 0, sizeof(rec));
            rec.memory_id = a->base.id;
            rec.timestamp_us = a->base.timestamp_us;
            rec.memory_type = a->memory_type;
            strncpy(rec.what_happened, a->what_happened, sizeof(rec.what_happened) - 1);
            strncpy(rec.why_it_happened, a->why_it_happened, sizeof(rec.why_it_happened) - 1);
            strncpy(rec.outcome, a->outcome, sizeof(rec.outcome) - 1);
            rec.valence = a->valence;
            rec.importance = a->base.importance;
            rec.arousal = a->arousal;
            rec.emotional_intensity = a->emotional_intensity;
            rec.identity_defining = a->identity_defining;
            rec.is_core_memory = a->is_core_memory;
            rec.source_device_id = a->base.source_device_id;
            rc = nimcp_memory_store_autobio_put(oodb->backing_store, &rec);
            break;
        }
    }

    /* Also write metadata */
    if (rc == 0) {
        nimcp_metadata_record_t meta;
        memset(&meta, 0, sizeof(meta));
        meta.entry_id = obj->id;
        meta.type = (nimcp_memory_entry_type_t)obj->type;
        meta.timestamp_us = obj->timestamp_us;
        meta.importance = obj->importance;
        meta.source_device_id = obj->source_device_id;
        meta.training_step = obj->training_step;
        meta.curriculum_stage = obj->curriculum_stage;
        strncpy(meta.tags, obj->tags, sizeof(meta.tags) - 1);

        /* Copy label from typed object */
        switch (obj->type) {
            case OODB_TYPE_ENGRAM:
                strncpy(meta.label, ((oodb_engram_t*)obj)->label, sizeof(meta.label) - 1);
                break;
            case OODB_TYPE_CONCEPT:
                strncpy(meta.label, ((oodb_concept_t*)obj)->label, sizeof(meta.label) - 1);
                break;
            default:
                break;
        }

        nimcp_memory_store_metadata_put(oodb->backing_store, &meta);
        obj->state = OODB_STATE_LOADED;
    }

    return rc;
}

/** Remove an engram from the engram cache array. */
static void cache_remove_engram(nimcp_oodb_t* oodb, oodb_engram_t* e) {
    for (uint32_t i = 0; i < oodb->engram_count; i++) {
        if (oodb->engram_cache[i] == e) {
            oodb->engram_cache[i] = oodb->engram_cache[oodb->engram_count - 1];
            oodb->engram_count--;
            return;
        }
    }
}

static void cache_remove_concept(nimcp_oodb_t* oodb, oodb_concept_t* c) {
    for (uint32_t i = 0; i < oodb->concept_count; i++) {
        if (oodb->concept_cache[i] == c) {
            oodb->concept_cache[i] = oodb->concept_cache[oodb->concept_count - 1];
            oodb->concept_count--;
            return;
        }
    }
}

static void cache_remove_relation(nimcp_oodb_t* oodb, oodb_relation_t* r) {
    for (uint32_t i = 0; i < oodb->relation_count; i++) {
        if (oodb->relation_cache[i] == r) {
            oodb->relation_cache[i] = oodb->relation_cache[oodb->relation_count - 1];
            oodb->relation_count--;
            return;
        }
    }
}

static void cache_remove_autobio(nimcp_oodb_t* oodb, oodb_autobio_t* a) {
    for (uint32_t i = 0; i < oodb->autobio_count; i++) {
        if (oodb->autobio_cache[i] == a) {
            oodb->autobio_cache[i] = oodb->autobio_cache[oodb->autobio_count - 1];
            oodb->autobio_count--;
            return;
        }
    }
}

/** Evict the LRU tail object. Flushes if dirty. */
static void oodb_evict_lru(nimcp_oodb_t* oodb) {
    oodb_object_t* victim = oodb->lru_tail;
    if (!victim) return;

    /* Flush if dirty */
    if (victim->state == OODB_STATE_DIRTY || victim->state == OODB_STATE_NEW) {
        oodb_flush_object(oodb, victim);
    }

    /* Remove from LRU list */
    lru_remove(oodb, victim);

    /* Remove from type-specific cache */
    switch (victim->type) {
        case OODB_TYPE_ENGRAM:
            cache_remove_engram(oodb, (oodb_engram_t*)victim);
            oodb->stats.cached_engrams--;
            break;
        case OODB_TYPE_CONCEPT:
            cache_remove_concept(oodb, (oodb_concept_t*)victim);
            oodb->stats.cached_concepts--;
            break;
        case OODB_TYPE_RELATION:
            cache_remove_relation(oodb, (oodb_relation_t*)victim);
            oodb->stats.cached_relations--;
            break;
        case OODB_TYPE_AUTOBIO:
            cache_remove_autobio(oodb, (oodb_autobio_t*)victim);
            oodb->stats.cached_autobio--;
            break;
    }

    oodb->total_cached--;
    oodb->stats.cached_objects--;
    oodb->stats.evictions++;

    /* Free data and object */
    oodb_free_object_data(victim);
    nimcp_free(victim);
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_oodb_config_t nimcp_oodb_config_default(void) {
    nimcp_oodb_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_cached_objects = 10000;
    cfg.max_engrams = 4096;
    cfg.max_concepts = 2048;
    cfg.lazy_loading = true;
    cfg.write_back = true;
    cfg.auto_link = true;
    return cfg;
}

nimcp_oodb_t* nimcp_oodb_create(
    const nimcp_oodb_config_t* config,
    nimcp_memory_store_t* backing_store)
{
    nimcp_oodb_t* oodb = (nimcp_oodb_t*)nimcp_calloc(1, sizeof(nimcp_oodb_t));
    if (!oodb) return NULL;

    if (config) {
        oodb->config = *config;
    } else {
        oodb->config = nimcp_oodb_config_default();
    }

    oodb->backing_store = backing_store;

    /* Allocate per-type cache arrays */
    uint32_t max_eng = oodb->config.max_engrams ? oodb->config.max_engrams : 4096;
    uint32_t max_con = oodb->config.max_concepts ? oodb->config.max_concepts : 2048;
    /* Relations and autobio share the general pool limit */
    uint32_t max_rel = oodb->config.max_cached_objects ? oodb->config.max_cached_objects : 10000;
    uint32_t max_auto = max_rel;

    oodb->engram_cache = (oodb_engram_t**)nimcp_calloc(max_eng, sizeof(oodb_engram_t*));
    oodb->concept_cache = (oodb_concept_t**)nimcp_calloc(max_con, sizeof(oodb_concept_t*));
    oodb->relation_cache = (oodb_relation_t**)nimcp_calloc(max_rel, sizeof(oodb_relation_t*));
    oodb->autobio_cache = (oodb_autobio_t**)nimcp_calloc(max_auto, sizeof(oodb_autobio_t*));

    if (!oodb->engram_cache || !oodb->concept_cache ||
        !oodb->relation_cache || !oodb->autobio_cache) {
        nimcp_oodb_destroy(oodb);
        return NULL;
    }

    return oodb;
}

void nimcp_oodb_destroy(nimcp_oodb_t* oodb) {
    if (!oodb) return;

    /* Free all cached objects */
    for (uint32_t i = 0; i < oodb->engram_count; i++) {
        if (oodb->engram_cache[i]) {
            oodb_free_object_data(&oodb->engram_cache[i]->base);
            nimcp_free(oodb->engram_cache[i]);
        }
    }
    for (uint32_t i = 0; i < oodb->concept_count; i++) {
        if (oodb->concept_cache[i]) {
            oodb_free_object_data(&oodb->concept_cache[i]->base);
            nimcp_free(oodb->concept_cache[i]);
        }
    }
    for (uint32_t i = 0; i < oodb->relation_count; i++) {
        if (oodb->relation_cache[i]) {
            nimcp_free(oodb->relation_cache[i]);
        }
    }
    for (uint32_t i = 0; i < oodb->autobio_count; i++) {
        if (oodb->autobio_cache[i]) {
            nimcp_free(oodb->autobio_cache[i]);
        }
    }

    nimcp_free(oodb->engram_cache);
    nimcp_free(oodb->concept_cache);
    nimcp_free(oodb->relation_cache);
    nimcp_free(oodb->autobio_cache);
    nimcp_free(oodb);
}

/* ============================================================================
 * Object Creation
 * ============================================================================ */

oodb_engram_t* nimcp_oodb_create_engram(nimcp_oodb_t* oodb, uint64_t id) {
    if (!oodb) return NULL;

    /* Evict if at capacity */
    if (oodb->engram_count >= oodb->config.max_engrams) {
        oodb_evict_lru(oodb);
    }
    if (oodb->total_cached >= oodb->config.max_cached_objects) {
        oodb_evict_lru(oodb);
    }

    oodb_engram_t* e = (oodb_engram_t*)nimcp_calloc(1, sizeof(oodb_engram_t));
    if (!e) return NULL;

    e->base.id = id;
    e->base.type = OODB_TYPE_ENGRAM;
    e->base.state = OODB_STATE_NEW;
    e->base.last_access_us = oodb_now_us();
    e->base.access_count = 1;

    oodb->engram_cache[oodb->engram_count++] = e;
    oodb->total_cached++;
    oodb->stats.cached_objects++;
    oodb->stats.cached_engrams++;

    lru_push_front(oodb, &e->base);

    return e;
}

oodb_concept_t* nimcp_oodb_create_concept(nimcp_oodb_t* oodb, uint64_t id) {
    if (!oodb) return NULL;

    if (oodb->concept_count >= oodb->config.max_concepts) {
        oodb_evict_lru(oodb);
    }
    if (oodb->total_cached >= oodb->config.max_cached_objects) {
        oodb_evict_lru(oodb);
    }

    oodb_concept_t* c = (oodb_concept_t*)nimcp_calloc(1, sizeof(oodb_concept_t));
    if (!c) return NULL;

    c->base.id = id;
    c->base.type = OODB_TYPE_CONCEPT;
    c->base.state = OODB_STATE_NEW;
    c->base.last_access_us = oodb_now_us();
    c->base.access_count = 1;

    oodb->concept_cache[oodb->concept_count++] = c;
    oodb->total_cached++;
    oodb->stats.cached_objects++;
    oodb->stats.cached_concepts++;

    lru_push_front(oodb, &c->base);

    return c;
}

oodb_relation_t* nimcp_oodb_create_relation(nimcp_oodb_t* oodb, uint64_t id) {
    if (!oodb) return NULL;

    if (oodb->total_cached >= oodb->config.max_cached_objects) {
        oodb_evict_lru(oodb);
    }

    oodb_relation_t* r = (oodb_relation_t*)nimcp_calloc(1, sizeof(oodb_relation_t));
    if (!r) return NULL;

    r->base.id = id;
    r->base.type = OODB_TYPE_RELATION;
    r->base.state = OODB_STATE_NEW;
    r->base.last_access_us = oodb_now_us();
    r->base.access_count = 1;

    oodb->relation_cache[oodb->relation_count++] = r;
    oodb->total_cached++;
    oodb->stats.cached_objects++;
    oodb->stats.cached_relations++;

    lru_push_front(oodb, &r->base);

    return r;
}

oodb_autobio_t* nimcp_oodb_create_autobio(nimcp_oodb_t* oodb, uint64_t id) {
    if (!oodb) return NULL;

    if (oodb->total_cached >= oodb->config.max_cached_objects) {
        oodb_evict_lru(oodb);
    }

    oodb_autobio_t* a = (oodb_autobio_t*)nimcp_calloc(1, sizeof(oodb_autobio_t));
    if (!a) return NULL;

    a->base.id = id;
    a->base.type = OODB_TYPE_AUTOBIO;
    a->base.state = OODB_STATE_NEW;
    a->base.last_access_us = oodb_now_us();
    a->base.access_count = 1;

    oodb->autobio_cache[oodb->autobio_count++] = a;
    oodb->total_cached++;
    oodb->stats.cached_objects++;
    oodb->stats.cached_autobio++;

    lru_push_front(oodb, &a->base);

    return a;
}

/* ============================================================================
 * Object Access (cache-first, lazy-load from SQLite on miss)
 * ============================================================================ */

oodb_engram_t* nimcp_oodb_get_engram(nimcp_oodb_t* oodb, uint64_t id) {
    if (!oodb) return NULL;

    /* Search cache */
    for (uint32_t i = 0; i < oodb->engram_count; i++) {
        if (oodb->engram_cache[i] && oodb->engram_cache[i]->base.id == id) {
            oodb->stats.cache_hits++;
            lru_touch(oodb, &oodb->engram_cache[i]->base);
            return oodb->engram_cache[i];
        }
    }

    /* Cache miss — try backing store */
    oodb->stats.cache_misses++;

    if (!oodb->backing_store || !oodb->config.lazy_loading) return NULL;

    nimcp_engram_record_t rec;
    memset(&rec, 0, sizeof(rec));
    if (nimcp_memory_store_engram_get(oodb->backing_store, id, &rec) != 0) {
        return NULL;
    }

    /* Evict if full */
    if (oodb->engram_count >= oodb->config.max_engrams ||
        oodb->total_cached >= oodb->config.max_cached_objects) {
        oodb_evict_lru(oodb);
    }

    /* Create from record */
    oodb_engram_t* e = (oodb_engram_t*)nimcp_calloc(1, sizeof(oodb_engram_t));
    if (!e) return NULL;

    e->base.id = rec.engram_id;
    e->base.type = OODB_TYPE_ENGRAM;
    e->base.state = OODB_STATE_LOADED;
    e->base.timestamp_us = rec.timestamp_us;
    e->base.importance = rec.importance;
    e->base.source_device_id = rec.source_device_id;
    e->base.last_access_us = oodb_now_us();
    e->base.access_count = 1;

    e->neuron_count = rec.neuron_count;
    e->embedding_dim = rec.embedding_dim;
    e->valence = rec.valence;
    e->arousal = rec.arousal;
    e->intensity = rec.intensity;
    e->consolidation_strength = rec.consolidation_strength;
    e->decay_rate = rec.decay_rate;
    e->vividness = rec.vividness;
    e->recall_count = rec.recall_count;
    e->memory_type = rec.memory_type;
    e->engram_state = rec.state;
    strncpy(e->label, rec.label, sizeof(e->label) - 1);

    /* Deep-copy arrays */
    if (rec.neuron_ids && rec.neuron_count > 0) {
        e->neuron_ids = (uint32_t*)nimcp_malloc(rec.neuron_count * sizeof(uint32_t));
        if (e->neuron_ids) {
            memcpy(e->neuron_ids, rec.neuron_ids, rec.neuron_count * sizeof(uint32_t));
        }
    }
    if (rec.activations && rec.neuron_count > 0) {
        e->activations = (float*)nimcp_malloc(rec.neuron_count * sizeof(float));
        if (e->activations) {
            memcpy(e->activations, rec.activations, rec.neuron_count * sizeof(float));
        }
    }
    if (rec.embedding && rec.embedding_dim > 0) {
        e->embedding = (float*)nimcp_malloc(rec.embedding_dim * sizeof(float));
        if (e->embedding) {
            memcpy(e->embedding, rec.embedding, rec.embedding_dim * sizeof(float));
        }
    }

    oodb->engram_cache[oodb->engram_count++] = e;
    oodb->total_cached++;
    oodb->stats.cached_objects++;
    oodb->stats.cached_engrams++;

    lru_push_front(oodb, &e->base);

    return e;
}

oodb_concept_t* nimcp_oodb_get_concept(nimcp_oodb_t* oodb, uint64_t id) {
    if (!oodb) return NULL;

    for (uint32_t i = 0; i < oodb->concept_count; i++) {
        if (oodb->concept_cache[i] && oodb->concept_cache[i]->base.id == id) {
            oodb->stats.cache_hits++;
            lru_touch(oodb, &oodb->concept_cache[i]->base);
            return oodb->concept_cache[i];
        }
    }

    oodb->stats.cache_misses++;

    if (!oodb->backing_store || !oodb->config.lazy_loading) return NULL;

    nimcp_concept_record_t rec;
    memset(&rec, 0, sizeof(rec));
    if (nimcp_memory_store_concept_get(oodb->backing_store, id, &rec) != 0) {
        return NULL;
    }

    if (oodb->concept_count >= oodb->config.max_concepts ||
        oodb->total_cached >= oodb->config.max_cached_objects) {
        oodb_evict_lru(oodb);
    }

    oodb_concept_t* c = (oodb_concept_t*)nimcp_calloc(1, sizeof(oodb_concept_t));
    if (!c) return NULL;

    c->base.id = rec.concept_id;
    c->base.type = OODB_TYPE_CONCEPT;
    c->base.state = OODB_STATE_LOADED;
    c->base.timestamp_us = rec.timestamp_us;
    c->base.source_device_id = rec.source_device_id;
    c->base.last_access_us = oodb_now_us();
    c->base.access_count = 1;

    strncpy(c->label, rec.label, sizeof(c->label) - 1);
    c->category = rec.category;
    c->embedding_dim = rec.embedding_dim;
    c->base_activation = rec.base_activation;
    c->access_count_semantic = rec.access_count;

    if (rec.embedding && rec.embedding_dim > 0) {
        c->embedding = (float*)nimcp_malloc(rec.embedding_dim * sizeof(float));
        if (c->embedding) {
            memcpy(c->embedding, rec.embedding, rec.embedding_dim * sizeof(float));
        }
    }

    oodb->concept_cache[oodb->concept_count++] = c;
    oodb->total_cached++;
    oodb->stats.cached_objects++;
    oodb->stats.cached_concepts++;

    lru_push_front(oodb, &c->base);

    return c;
}

oodb_relation_t* nimcp_oodb_get_relation(nimcp_oodb_t* oodb, uint64_t id) {
    if (!oodb) return NULL;

    for (uint32_t i = 0; i < oodb->relation_count; i++) {
        if (oodb->relation_cache[i] && oodb->relation_cache[i]->base.id == id) {
            oodb->stats.cache_hits++;
            lru_touch(oodb, &oodb->relation_cache[i]->base);
            return oodb->relation_cache[i];
        }
    }

    oodb->stats.cache_misses++;
    return NULL;  /* Relations not lazy-loaded (need pointer resolution) */
}

oodb_autobio_t* nimcp_oodb_get_autobio(nimcp_oodb_t* oodb, uint64_t id) {
    if (!oodb) return NULL;

    for (uint32_t i = 0; i < oodb->autobio_count; i++) {
        if (oodb->autobio_cache[i] && oodb->autobio_cache[i]->base.id == id) {
            oodb->stats.cache_hits++;
            lru_touch(oodb, &oodb->autobio_cache[i]->base);
            return oodb->autobio_cache[i];
        }
    }

    oodb->stats.cache_misses++;
    return NULL;  /* Autobio lazy-load not implemented yet */
}

/* ============================================================================
 * Dirty Marking
 * ============================================================================ */

void nimcp_oodb_mark_dirty(oodb_object_t* obj) {
    if (!obj) return;
    obj->state = OODB_STATE_DIRTY;
    /* Note: caller should also call lru_touch if they have the oodb pointer.
     * Since we only have the object here, we update the timestamp. */
    obj->last_access_us = oodb_now_us();
}

/* ============================================================================
 * Graph Traversal (BFS via direct pointers)
 * ============================================================================ */

nimcp_oodb_traversal_t* nimcp_oodb_traverse(
    nimcp_oodb_t* oodb,
    oodb_concept_t* start,
    uint32_t max_hops,
    float min_strength)
{
    if (!oodb || !start) return NULL;

    uint32_t init_cap = 64;
    nimcp_oodb_traversal_t* result = (nimcp_oodb_traversal_t*)nimcp_calloc(1, sizeof(nimcp_oodb_traversal_t));
    if (!result) return NULL;

    result->concepts = (oodb_concept_t**)nimcp_calloc(init_cap, sizeof(oodb_concept_t*));
    result->depths = (uint32_t*)nimcp_calloc(init_cap, sizeof(uint32_t));
    result->cumulative_strength = (float*)nimcp_calloc(init_cap, sizeof(float));
    if (!result->concepts || !result->depths || !result->cumulative_strength) {
        nimcp_oodb_traversal_destroy(result);
        return NULL;
    }
    result->capacity = init_cap;
    result->count = 0;

    /* BFS queue: concept pointer + depth + cumulative strength */
    typedef struct {
        oodb_concept_t* concept;
        uint32_t depth;
        float strength;
    } bfs_entry_t;

    uint32_t queue_cap = 256;
    bfs_entry_t* queue = (bfs_entry_t*)nimcp_calloc(queue_cap, sizeof(bfs_entry_t));
    if (!queue) {
        nimcp_oodb_traversal_destroy(result);
        return NULL;
    }

    /* Visited set: track by concept pointer (simple linear scan) */
    uint32_t visited_cap = 256;
    oodb_concept_t** visited = (oodb_concept_t**)nimcp_calloc(visited_cap, sizeof(oodb_concept_t*));
    if (!visited) {
        nimcp_free(queue);
        nimcp_oodb_traversal_destroy(result);
        return NULL;
    }

    uint32_t visited_count = 0;
    uint32_t queue_head = 0, queue_tail = 0;

    /* Mark start as visited */
    visited[visited_count++] = start;

    /* Seed queue with start */
    queue[queue_tail].concept = start;
    queue[queue_tail].depth = 0;
    queue[queue_tail].strength = 1.0f;
    queue_tail++;

    while (queue_head < queue_tail) {
        bfs_entry_t entry = queue[queue_head++];

        if (entry.depth >= max_hops) continue;

        oodb_concept_t* current = entry.concept;

        for (uint32_t r = 0; r < current->relation_count; r++) {
            oodb_relation_t* rel = current->relations[r];
            if (!rel || !rel->target) continue;

            if (rel->strength < min_strength) continue;

            oodb_concept_t* neighbor = rel->target;

            /* Check visited */
            bool seen = false;
            for (uint32_t v = 0; v < visited_count; v++) {
                if (visited[v] == neighbor) {
                    seen = true;
                    break;
                }
            }
            if (seen) continue;

            /* Mark visited */
            if (visited_count >= visited_cap) {
                visited_cap *= 2;
                oodb_concept_t** new_visited = (oodb_concept_t**)nimcp_calloc(visited_cap, sizeof(oodb_concept_t*));
                if (new_visited) {
                    memcpy(new_visited, visited, visited_count * sizeof(oodb_concept_t*));
                    nimcp_free(visited);
                    visited = new_visited;
                }
            }
            visited[visited_count++] = neighbor;

            /* Add to result */
            if (result->count >= result->capacity) {
                uint32_t new_cap = result->capacity * 2;
                oodb_concept_t** new_concepts = (oodb_concept_t**)nimcp_calloc(new_cap, sizeof(oodb_concept_t*));
                uint32_t* new_depths = (uint32_t*)nimcp_calloc(new_cap, sizeof(uint32_t));
                float* new_str = (float*)nimcp_calloc(new_cap, sizeof(float));
                if (new_concepts && new_depths && new_str) {
                    memcpy(new_concepts, result->concepts, result->count * sizeof(oodb_concept_t*));
                    memcpy(new_depths, result->depths, result->count * sizeof(uint32_t));
                    memcpy(new_str, result->cumulative_strength, result->count * sizeof(float));
                    nimcp_free(result->concepts);
                    nimcp_free(result->depths);
                    nimcp_free(result->cumulative_strength);
                    result->concepts = new_concepts;
                    result->depths = new_depths;
                    result->cumulative_strength = new_str;
                    result->capacity = new_cap;
                } else {
                    nimcp_free(new_concepts);
                    nimcp_free(new_depths);
                    nimcp_free(new_str);
                    break;
                }
            }

            float cum_strength = entry.strength * rel->strength;
            result->concepts[result->count] = neighbor;
            result->depths[result->count] = entry.depth + 1;
            result->cumulative_strength[result->count] = cum_strength;
            result->count++;

            /* Enqueue */
            if (queue_tail >= queue_cap) {
                uint32_t new_qcap = queue_cap * 2;
                bfs_entry_t* new_queue = (bfs_entry_t*)nimcp_calloc(new_qcap, sizeof(bfs_entry_t));
                if (new_queue) {
                    memcpy(new_queue, queue, queue_tail * sizeof(bfs_entry_t));
                    nimcp_free(queue);
                    queue = new_queue;
                    queue_cap = new_qcap;
                }
            }
            queue[queue_tail].concept = neighbor;
            queue[queue_tail].depth = entry.depth + 1;
            queue[queue_tail].strength = cum_strength;
            queue_tail++;
        }
    }

    nimcp_free(queue);
    nimcp_free(visited);

    return result;
}

void nimcp_oodb_traversal_destroy(nimcp_oodb_traversal_t* result) {
    if (!result) return;
    nimcp_free(result->concepts);
    nimcp_free(result->depths);
    nimcp_free(result->cumulative_strength);
    nimcp_free(result);
}

/* ============================================================================
 * Tag & Type Search
 * ============================================================================ */

nimcp_oodb_search_result_t* nimcp_oodb_search_by_tag(
    nimcp_oodb_t* oodb, const char* tag, uint32_t max_results)
{
    if (!oodb || !tag) return NULL;

    nimcp_oodb_search_result_t* result = (nimcp_oodb_search_result_t*)nimcp_calloc(1, sizeof(nimcp_oodb_search_result_t));
    if (!result) return NULL;

    uint32_t cap = max_results > 0 ? max_results : 256;
    result->objects = (oodb_object_t**)nimcp_calloc(cap, sizeof(oodb_object_t*));
    if (!result->objects) {
        nimcp_free(result);
        return NULL;
    }
    result->capacity = cap;
    result->count = 0;

    /* Scan all cached objects */
    #define OODB_TAG_SCAN(arr, arr_count) \
        for (uint32_t _ti = 0; _ti < (arr_count) && result->count < cap; _ti++) { \
            if ((arr)[_ti] && strstr((arr)[_ti]->base.tags, tag)) { \
                result->objects[result->count++] = &(arr)[_ti]->base; \
            } \
        }

    OODB_TAG_SCAN(oodb->engram_cache, oodb->engram_count)
    OODB_TAG_SCAN(oodb->concept_cache, oodb->concept_count)
    OODB_TAG_SCAN(oodb->relation_cache, oodb->relation_count)
    OODB_TAG_SCAN(oodb->autobio_cache, oodb->autobio_count)

    #undef OODB_TAG_SCAN

    return result;
}

nimcp_oodb_search_result_t* nimcp_oodb_search_by_type(
    nimcp_oodb_t* oodb, oodb_object_type_t type, uint32_t max_results)
{
    if (!oodb) return NULL;

    nimcp_oodb_search_result_t* result = (nimcp_oodb_search_result_t*)nimcp_calloc(1, sizeof(nimcp_oodb_search_result_t));
    if (!result) return NULL;

    uint32_t cap = max_results > 0 ? max_results : 256;
    result->objects = (oodb_object_t**)nimcp_calloc(cap, sizeof(oodb_object_t*));
    if (!result->objects) {
        nimcp_free(result);
        return NULL;
    }
    result->capacity = cap;
    result->count = 0;

    switch (type) {
        case OODB_TYPE_ENGRAM:
            for (uint32_t i = 0; i < oodb->engram_count && result->count < cap; i++) {
                if (oodb->engram_cache[i]) {
                    result->objects[result->count++] = &oodb->engram_cache[i]->base;
                }
            }
            break;
        case OODB_TYPE_CONCEPT:
            for (uint32_t i = 0; i < oodb->concept_count && result->count < cap; i++) {
                if (oodb->concept_cache[i]) {
                    result->objects[result->count++] = &oodb->concept_cache[i]->base;
                }
            }
            break;
        case OODB_TYPE_RELATION:
            for (uint32_t i = 0; i < oodb->relation_count && result->count < cap; i++) {
                if (oodb->relation_cache[i]) {
                    result->objects[result->count++] = &oodb->relation_cache[i]->base;
                }
            }
            break;
        case OODB_TYPE_AUTOBIO:
            for (uint32_t i = 0; i < oodb->autobio_count && result->count < cap; i++) {
                if (oodb->autobio_cache[i]) {
                    result->objects[result->count++] = &oodb->autobio_cache[i]->base;
                }
            }
            break;
    }

    return result;
}

void nimcp_oodb_search_result_destroy(nimcp_oodb_search_result_t* result) {
    if (!result) return;
    nimcp_free(result->objects);
    nimcp_free(result);
}

/* ============================================================================
 * Persistence
 * ============================================================================ */

int nimcp_oodb_flush(nimcp_oodb_t* oodb) {
    if (!oodb) return -1;
    if (!oodb->backing_store) return -1;

    int errors = 0;

    #define OODB_FLUSH_CACHE(arr, arr_count) \
        for (uint32_t _fi = 0; _fi < (arr_count); _fi++) { \
            if ((arr)[_fi]) { \
                oodb_object_t* _fobj = &(arr)[_fi]->base; \
                if (_fobj->state == OODB_STATE_DIRTY || _fobj->state == OODB_STATE_NEW) { \
                    if (oodb_flush_object(oodb, _fobj) != 0) errors++; \
                } \
            } \
        }

    OODB_FLUSH_CACHE(oodb->engram_cache, oodb->engram_count)
    OODB_FLUSH_CACHE(oodb->concept_cache, oodb->concept_count)
    OODB_FLUSH_CACHE(oodb->relation_cache, oodb->relation_count)
    OODB_FLUSH_CACHE(oodb->autobio_cache, oodb->autobio_count)

    #undef OODB_FLUSH_CACHE

    oodb->stats.flushes++;

    return errors > 0 ? -1 : 0;
}

int nimcp_oodb_prewarm(nimcp_oodb_t* oodb, uint32_t count) {
    if (!oodb || !oodb->backing_store || count == 0) return -1;

    /* Query recent engrams from the last hour */
    uint64_t now = oodb_now_us();
    uint64_t one_hour_ago = (now > 3600000000ULL) ? (now - 3600000000ULL) : 0;

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_time(oodb->backing_store, one_hour_ago, now, count);

    if (!results) return -1;

    for (uint32_t i = 0; i < results->count; i++) {
        nimcp_oodb_get_engram(oodb, results->ids[i]);
    }

    uint32_t loaded = results->count;
    nimcp_memory_search_result_destroy(results);

    return (int)loaded;
}

int nimcp_oodb_get_stats(const nimcp_oodb_t* oodb, nimcp_oodb_stats_t* stats) {
    if (!oodb || !stats) return -1;
    *stats = oodb->stats;

    /* Recount dirty objects */
    stats->dirty_objects = 0;
    for (uint32_t i = 0; i < oodb->engram_count; i++) {
        if (oodb->engram_cache[i] &&
            (oodb->engram_cache[i]->base.state == OODB_STATE_DIRTY ||
             oodb->engram_cache[i]->base.state == OODB_STATE_NEW)) {
            stats->dirty_objects++;
        }
    }
    for (uint32_t i = 0; i < oodb->concept_count; i++) {
        if (oodb->concept_cache[i] &&
            (oodb->concept_cache[i]->base.state == OODB_STATE_DIRTY ||
             oodb->concept_cache[i]->base.state == OODB_STATE_NEW)) {
            stats->dirty_objects++;
        }
    }
    for (uint32_t i = 0; i < oodb->relation_count; i++) {
        if (oodb->relation_cache[i] &&
            (oodb->relation_cache[i]->base.state == OODB_STATE_DIRTY ||
             oodb->relation_cache[i]->base.state == OODB_STATE_NEW)) {
            stats->dirty_objects++;
        }
    }
    for (uint32_t i = 0; i < oodb->autobio_count; i++) {
        if (oodb->autobio_cache[i] &&
            (oodb->autobio_cache[i]->base.state == OODB_STATE_DIRTY ||
             oodb->autobio_cache[i]->base.state == OODB_STATE_NEW)) {
            stats->dirty_objects++;
        }
    }

    return 0;
}
