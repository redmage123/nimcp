/**
 * @file nimcp_memory_store.h
 * @brief Persistent memory store backed by SQLite with FTS5 and vector search
 *
 * WHAT: Durable storage for engrams, semantic concepts, relations, and
 *       autobiographical memories with full-text and vector similarity search.
 * WHY:  Brain needs persistent recall across restarts; SQLite gives ACID
 *       guarantees with WAL for concurrent read/write, FTS5 for label search,
 *       and brute-force cosine similarity for embedding-based recall.
 * HOW:  Single SQLite database, write-buffered for throughput, bloom filter
 *       for duplicate detection, prepared statements for low-latency ops.
 *
 * THREAD-SAFE: Yes (internal mutex around all DB operations)
 */

#ifndef NIMCP_MEMORY_STORE_H
#define NIMCP_MEMORY_STORE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct nimcp_memory_store nimcp_memory_store_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    const char* db_path;              /* SQLite database file path */
    uint32_t hot_cache_size;          /* In-memory cache entries (default: 512) */
    uint32_t write_buffer_size;       /* Buffer writes before flush (default: 100) */
    uint32_t write_flush_interval_ms; /* Auto-flush interval (default: 100) */
    uint32_t embedding_dim;           /* Embedding dimension (default: 1024) */
    bool enable_fts;                  /* Enable FTS5 full-text search */
    bool enable_wal;                  /* Enable WAL mode (default: true) */
    bool enable_questdb_sync;         /* Write-through to QuestDB */
    bool enable_bloom_filter;         /* Bloom filter for duplicate detection */
    uint32_t bloom_filter_size;       /* Bloom filter bits (default: 1M) */
    float importance_index_threshold; /* Only index embeddings above this importance */
    /* QuestDB config (only used if enable_questdb_sync) */
    const char* questdb_host;
    uint16_t questdb_ilp_port;
} nimcp_memory_store_config_t;

/* ============================================================================
 * Engram Record (stored in SQLite)
 * ============================================================================ */

typedef struct {
    uint64_t engram_id;
    uint64_t timestamp_us;
    uint32_t memory_type;          /* EPISODIC, SEMANTIC, PROCEDURAL, etc. */
    uint32_t state;                /* ENCODING, LABILE, CONSOLIDATED, etc. */

    /* Neural pattern (stored as blob) */
    uint32_t neuron_count;
    uint32_t* neuron_ids;
    float* activations;

    /* Embedding for vector search (stored as blob) */
    float* embedding;
    uint32_t embedding_dim;

    /* Emotional context */
    float valence;
    float arousal;
    float intensity;

    /* Consolidation */
    float consolidation_strength;
    float decay_rate;
    float vividness;
    float importance;
    uint32_t recall_count;
    uint64_t last_recall_us;

    /* Label/description for FTS */
    char label[256];

    /* Source device (0 = local/master, nonzero = remote device ID) */
    uint32_t source_device_id;
} nimcp_engram_record_t;

/* ============================================================================
 * Concept Record
 * ============================================================================ */

typedef struct {
    uint64_t concept_id;
    uint64_t timestamp_us;
    char label[256];
    uint32_t category;

    float* embedding;
    uint32_t embedding_dim;

    float base_activation;
    uint32_t access_count;
    uint64_t source_engram_id;
    uint32_t source_device_id;
} nimcp_concept_record_t;

/* ============================================================================
 * Relation Record
 * ============================================================================ */

typedef struct {
    uint64_t relation_id;
    uint64_t source_concept_id;
    uint64_t target_concept_id;
    uint32_t relation_type;
    float strength;
    uint64_t timestamp_us;
} nimcp_relation_record_t;

/* ============================================================================
 * Autobiographical Record
 * ============================================================================ */

typedef struct {
    uint64_t memory_id;
    uint64_t timestamp_us;
    uint32_t memory_type;
    char what_happened[512];
    char why_it_happened[512];
    char outcome[256];
    int32_t valence;
    float importance;
    float arousal;
    float emotional_intensity;
    bool identity_defining;
    bool is_core_memory;
    uint32_t source_device_id;
} nimcp_autobio_record_t;

/* ============================================================================
 * Search Results
 * ============================================================================ */

typedef struct {
    uint64_t* ids;
    float* distances;              /* For vector search: lower = more similar */
    uint32_t count;
    uint32_t capacity;
} nimcp_memory_search_result_t;

typedef struct {
    uint64_t* node_ids;
    uint32_t* depths;
    float* weights;
    uint32_t count;
} nimcp_graph_traversal_result_t;

/* ============================================================================
 * Metadata Catalog — unified cross-type memory index
 *
 * Every memory entry (engram, concept, autobio) gets a metadata record
 * that enables type-agnostic search across ALL memory types.
 * ============================================================================ */

typedef enum {
    NIMCP_MEMORY_TYPE_ENGRAM = 0,
    NIMCP_MEMORY_TYPE_CONCEPT = 1,
    NIMCP_MEMORY_TYPE_RELATION = 2,
    NIMCP_MEMORY_TYPE_AUTOBIO = 3
} nimcp_memory_entry_type_t;

typedef struct {
    uint64_t entry_id;                /* ID within its type table */
    nimcp_memory_entry_type_t type;   /* Which table this lives in */
    uint64_t timestamp_us;
    float importance;
    uint32_t source_device_id;
    uint32_t training_step;           /* Which training step created this */
    uint32_t curriculum_stage;        /* Which curriculum stage (0-3) */
    uint32_t model_version;           /* Model version at creation time */
    char label[256];                  /* Label/description (for search) */
    char tags[512];                   /* Comma-separated tags ("visual,bird,novel") */
} nimcp_metadata_record_t;

typedef struct {
    nimcp_metadata_record_t* records;
    uint32_t count;
    uint32_t capacity;
} nimcp_metadata_search_result_t;

/* ============================================================================
 * Metadata Operations
 * ============================================================================ */

/**
 * @brief Store metadata for a memory entry.
 * Called automatically by engram_put/concept_put/autobio_put if tags provided.
 */
int nimcp_memory_store_metadata_put(
    nimcp_memory_store_t* store,
    const nimcp_metadata_record_t* record);

/**
 * @brief Search across ALL memory types by tag.
 * Returns entries from engrams, concepts, and autobio that match any tag.
 * Tags are comma-separated: "bird,visual" matches entries with either tag.
 */
nimcp_metadata_search_result_t* nimcp_memory_store_metadata_search_tags(
    nimcp_memory_store_t* store,
    const char* tags,
    uint32_t max_results);

/**
 * @brief Search across ALL memory types by text (FTS5 on label field).
 */
nimcp_metadata_search_result_t* nimcp_memory_store_metadata_search_text(
    nimcp_memory_store_t* store,
    const char* query,
    uint32_t max_results);

/**
 * @brief Search by training provenance (step range, stage, device).
 */
nimcp_metadata_search_result_t* nimcp_memory_store_metadata_search_provenance(
    nimcp_memory_store_t* store,
    uint32_t min_step, uint32_t max_step,
    int32_t curriculum_stage,  /* -1 = any */
    int32_t device_id,         /* -1 = any */
    uint32_t max_results);

/**
 * @brief Get all metadata for a specific entry.
 */
int nimcp_memory_store_metadata_get(
    nimcp_memory_store_t* store,
    uint64_t entry_id,
    nimcp_memory_entry_type_t type,
    nimcp_metadata_record_t* record);

/**
 * @brief Add tags to an existing entry.
 */
int nimcp_memory_store_metadata_add_tags(
    nimcp_memory_store_t* store,
    uint64_t entry_id,
    nimcp_memory_entry_type_t type,
    const char* new_tags);

void nimcp_metadata_search_result_destroy(nimcp_metadata_search_result_t* result);

/* ============================================================================
 * Store Statistics
 * ============================================================================ */

typedef struct {
    uint64_t total_engrams;
    uint64_t total_concepts;
    uint64_t total_relations;
    uint64_t total_autobio;
    uint64_t total_writes;
    uint64_t total_reads;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t write_buffer_flushes;
    uint64_t bloom_filter_hits;
    float avg_write_latency_ms;
    float avg_read_latency_ms;
    uint64_t db_size_bytes;
} nimcp_memory_store_stats_t;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_memory_store_config_t nimcp_memory_store_config_default(void);

nimcp_memory_store_t* nimcp_memory_store_create(const nimcp_memory_store_config_t* config);

void nimcp_memory_store_destroy(nimcp_memory_store_t* store);

/* Flush write buffer to disk (called automatically on interval) */
int nimcp_memory_store_flush(nimcp_memory_store_t* store);

/* Checkpoint: flush + WAL checkpoint + sync QuestDB */
int nimcp_memory_store_checkpoint(nimcp_memory_store_t* store);

/* ============================================================================
 * Engram Operations
 * ============================================================================ */

int nimcp_memory_store_engram_put(nimcp_memory_store_t* store, const nimcp_engram_record_t* record);

int nimcp_memory_store_engram_get(nimcp_memory_store_t* store, uint64_t engram_id, nimcp_engram_record_t* record);

/* Vector similarity search: find engrams with similar embeddings */
nimcp_memory_search_result_t* nimcp_memory_store_engram_search_similar(
    nimcp_memory_store_t* store, const float* query_embedding, uint32_t dim,
    uint32_t top_k, float threshold);

/* Full-text search on engram labels */
nimcp_memory_search_result_t* nimcp_memory_store_engram_search_text(
    nimcp_memory_store_t* store, const char* query, uint32_t max_results);

/* Time-range query */
nimcp_memory_search_result_t* nimcp_memory_store_engram_search_time(
    nimcp_memory_store_t* store, uint64_t start_us, uint64_t end_us,
    uint32_t max_results);

/* Update engram (consolidation state, recall count, etc.) */
int nimcp_memory_store_engram_update(nimcp_memory_store_t* store, const nimcp_engram_record_t* record);

/* Delete old/unimportant engrams */
int nimcp_memory_store_engram_prune(nimcp_memory_store_t* store,
    float min_importance, uint32_t min_recall_count, uint64_t older_than_us);

/* ============================================================================
 * Concept Operations
 * ============================================================================ */

int nimcp_memory_store_concept_put(nimcp_memory_store_t* store, const nimcp_concept_record_t* record);

int nimcp_memory_store_concept_get(nimcp_memory_store_t* store, uint64_t concept_id, nimcp_concept_record_t* record);

nimcp_memory_search_result_t* nimcp_memory_store_concept_search_similar(
    nimcp_memory_store_t* store, const float* query_embedding, uint32_t dim,
    uint32_t top_k, float threshold);

nimcp_memory_search_result_t* nimcp_memory_store_concept_search_text(
    nimcp_memory_store_t* store, const char* label_query, uint32_t max_results);

int nimcp_memory_store_concept_update_activation(
    nimcp_memory_store_t* store, uint64_t concept_id, float new_activation, uint32_t access_count);

/* ============================================================================
 * Relation Operations (Knowledge Graph)
 * ============================================================================ */

int nimcp_memory_store_relation_put(nimcp_memory_store_t* store, const nimcp_relation_record_t* record);

/* Graph traversal: multi-hop BFS from start node */
nimcp_graph_traversal_result_t* nimcp_memory_store_relation_traverse(
    nimcp_memory_store_t* store, uint64_t start_concept_id,
    uint32_t max_hops, float min_strength);

/* Get all relations for a concept */
nimcp_memory_search_result_t* nimcp_memory_store_relation_get_for_concept(
    nimcp_memory_store_t* store, uint64_t concept_id, uint32_t max_results);

/* ============================================================================
 * Autobiographical Operations
 * ============================================================================ */

int nimcp_memory_store_autobio_put(nimcp_memory_store_t* store, const nimcp_autobio_record_t* record);

nimcp_memory_search_result_t* nimcp_memory_store_autobio_search_text(
    nimcp_memory_store_t* store, const char* query, uint32_t max_results);

nimcp_memory_search_result_t* nimcp_memory_store_autobio_search_time(
    nimcp_memory_store_t* store, uint64_t start_us, uint64_t end_us,
    uint32_t max_results);

/* ============================================================================
 * Bloom Filter (duplicate detection)
 * ============================================================================ */

bool nimcp_memory_store_bloom_check(nimcp_memory_store_t* store, const float* embedding, uint32_t dim);
void nimcp_memory_store_bloom_add(nimcp_memory_store_t* store, const float* embedding, uint32_t dim);

/* ============================================================================
 * Consolidation & Maintenance
 * ============================================================================ */

/* Run during sleep: consolidate engrams -> semantic concepts */
int nimcp_memory_store_consolidate(nimcp_memory_store_t* store);

/* Garbage collect: remove low-importance, unrecalled, old memories */
int nimcp_memory_store_gc(nimcp_memory_store_t* store,
    float min_importance, uint32_t max_age_days);

/* Rebuild vector index (run during idle/sleep) */
int nimcp_memory_store_rebuild_index(nimcp_memory_store_t* store);

/* ============================================================================
 * Result Cleanup
 * ============================================================================ */

void nimcp_memory_search_result_destroy(nimcp_memory_search_result_t* result);
void nimcp_memory_graph_result_destroy(nimcp_graph_traversal_result_t* result);

/* ============================================================================
 * Statistics
 * ============================================================================ */

int nimcp_memory_store_get_stats(nimcp_memory_store_t* store, nimcp_memory_store_stats_t* stats);

/**
 * @brief Check if the memory store is healthy (no unrecoverable flush errors).
 *
 * @return true if healthy, false if a COMMIT/flush failure has been detected.
 *         A false return means persistent writes should be skipped to prevent
 *         cascading a broken SQLite into a training crash.
 */
bool nimcp_memory_store_is_healthy(const nimcp_memory_store_t* store);

/* ============================================================================
 * Remote Sensory Stream Ingestion (Master-side)
 *
 * When a remote device (drone, phone, robot) sends its sensory experience
 * to the master, the master ingests it into its own memory store with the
 * source_device_id tagged. This builds a unified memory across all devices.
 * ============================================================================ */

/**
 * @brief Sensory stream record received from a remote device
 */
typedef struct {
    uint32_t source_device_id;        /* Which device sent this */
    uint64_t timestamp_us;            /* When the device experienced it */
    char label[256];                  /* What the device experienced */
    float* features;                  /* Input embedding (device's sensory data) */
    uint32_t feature_dim;
    float* output;                    /* Device's neural response (output vector) */
    uint32_t output_dim;
    float loss;                       /* Device's loss on this input */
    float valence;                    /* Emotional valence */
    float arousal;                    /* Emotional arousal */
    float importance;                 /* Device-assessed importance */
} nimcp_sensory_stream_record_t;

/**
 * @brief Ingest a remote device's sensory experience into master memory.
 *
 * Creates an engram, semantic concept (if labeled), and autobio entry
 * (if important enough) — all tagged with source_device_id.
 *
 * @param store Memory store
 * @param record Sensory stream from remote device
 * @return 0 on success, -1 on error
 */
int nimcp_memory_store_ingest_remote_stream(
    nimcp_memory_store_t* store,
    const nimcp_sensory_stream_record_t* record);

/**
 * @brief Query memories from a specific device.
 *
 * @param store Memory store
 * @param device_id Source device ID (0 = local only)
 * @param max_results Maximum results
 * @return Search result with matching memory IDs
 */
nimcp_memory_search_result_t* nimcp_memory_store_query_by_device(
    nimcp_memory_store_t* store, uint32_t device_id, uint32_t max_results);

/**
 * @brief Cross-device correlation: find co-occurring experiences
 * across different devices within a time window.
 *
 * @param store Memory store
 * @param time_window_us Max time delta between experiences (e.g., 5 seconds)
 * @param max_results Maximum correlation pairs
 * @return Search result with correlated memory ID pairs
 */
nimcp_memory_search_result_t* nimcp_memory_store_cross_device_correlations(
    nimcp_memory_store_t* store, uint64_t time_window_us, uint32_t max_results);

/* ============================================================================
 * Security Audit Trail — append-only event log (memory store layer)
 * ============================================================================ */

typedef struct {
    uint64_t timestamp_us;
    uint32_t event_type;       /* 0=info, 1=warning, 2=threat, 3=breach, 4=recovery */
    uint32_t source_module_id;
    char description[256];
    char details[512];
} nimcp_memory_audit_event_t;

int nimcp_memory_store_audit_log(nimcp_memory_store_t* store, const nimcp_memory_audit_event_t* event);

nimcp_memory_search_result_t* nimcp_memory_store_audit_search(
    nimcp_memory_store_t* store, uint32_t min_severity,
    uint64_t start_us, uint64_t end_us, uint32_t max_results);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEMORY_STORE_H */
