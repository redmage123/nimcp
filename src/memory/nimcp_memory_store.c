/**
 * @file nimcp_memory_store.c
 * @brief Persistent memory store — SQLite + FTS5 + brute-force vector search
 *
 * WHAT: Durable storage for engrams, concepts, relations, autobiographical
 *       memories with full-text search (FTS5) and cosine-similarity vector
 *       retrieval.
 * WHY:  Brain needs persistent recall across restarts with sub-millisecond
 *       reads and batched writes for throughput.
 * HOW:  SQLite WAL mode, prepared statements, write buffer with flush,
 *       bloom filter for duplicate detection, AVX2/SSE cosine similarity.
 */

#include "memory/nimcp_memory_store.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct nimcp_memory_store {
    sqlite3* db;
    nimcp_memory_store_config_t config;

    /* Write buffer (batches writes for efficiency) */
    nimcp_engram_record_t* engram_buffer;
    uint32_t engram_buffer_count;
    nimcp_concept_record_t* concept_buffer;
    uint32_t concept_buffer_count;

    /* Prepared statements (reused for performance) */
    sqlite3_stmt* stmt_engram_insert;
    sqlite3_stmt* stmt_engram_get;
    sqlite3_stmt* stmt_engram_update;
    sqlite3_stmt* stmt_concept_insert;
    sqlite3_stmt* stmt_concept_get;
    sqlite3_stmt* stmt_relation_insert;
    sqlite3_stmt* stmt_autobio_insert;

    /* Bloom filter */
    uint8_t* bloom_filter;
    uint32_t bloom_size;

    /* Stats */
    nimcp_memory_store_stats_t stats;

    /* Health flag — set to false on unrecoverable flush errors so that
     * callers (e.g. brain_learn_vector) can skip persistent writes and
     * avoid cascading a broken SQLite into a training crash. */
    bool healthy;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Bloom Filter Hash Functions
 * ============================================================================ */

static uint32_t bloom_hash_fnv1a(const uint8_t* data, uint32_t len) {
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t bloom_hash_djb2(const uint8_t* data, uint32_t len) {
    uint32_t hash = 5381;
    for (uint32_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + data[i];
    }
    return hash;
}

static uint32_t bloom_hash_murmur3_mix(const uint8_t* data, uint32_t len) {
    uint32_t h = 0xdeadbeef;
    for (uint32_t i = 0; i + 3 < len; i += 4) {
        uint32_t k;
        memcpy(&k, data + i, 4);
        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        h ^= k;
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }
    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

/* ============================================================================
 * Cosine Similarity (scalar fallback, SIMD where available)
 * ============================================================================ */

#if defined(__AVX2__)
#include <immintrin.h>

static float cosine_similarity(const float* a, const float* b, uint32_t dim) {
    __m256 sum_ab = _mm256_setzero_ps();
    __m256 sum_aa = _mm256_setzero_ps();
    __m256 sum_bb = _mm256_setzero_ps();

    uint32_t i = 0;
    for (; i + 7 < dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        sum_ab = _mm256_fmadd_ps(va, vb, sum_ab);
        sum_aa = _mm256_fmadd_ps(va, va, sum_aa);
        sum_bb = _mm256_fmadd_ps(vb, vb, sum_bb);
    }

    /* Horizontal sum */
    float tmp_ab[8], tmp_aa[8], tmp_bb[8];
    _mm256_storeu_ps(tmp_ab, sum_ab);
    _mm256_storeu_ps(tmp_aa, sum_aa);
    _mm256_storeu_ps(tmp_bb, sum_bb);

    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (int j = 0; j < 8; j++) {
        dot += tmp_ab[j];
        norm_a += tmp_aa[j];
        norm_b += tmp_bb[j];
    }

    /* Scalar remainder */
    for (; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    if (denom < 1e-12f) return 0.0f;
    return dot / denom;
}

#elif defined(__SSE2__)
#include <xmmintrin.h>
#include <emmintrin.h>

static float cosine_similarity(const float* a, const float* b, uint32_t dim) {
    __m128 sum_ab = _mm_setzero_ps();
    __m128 sum_aa = _mm_setzero_ps();
    __m128 sum_bb = _mm_setzero_ps();

    uint32_t i = 0;
    for (; i + 3 < dim; i += 4) {
        __m128 va = _mm_loadu_ps(a + i);
        __m128 vb = _mm_loadu_ps(b + i);
        sum_ab = _mm_add_ps(sum_ab, _mm_mul_ps(va, vb));
        sum_aa = _mm_add_ps(sum_aa, _mm_mul_ps(va, va));
        sum_bb = _mm_add_ps(sum_bb, _mm_mul_ps(vb, vb));
    }

    float tmp_ab[4], tmp_aa[4], tmp_bb[4];
    _mm_storeu_ps(tmp_ab, sum_ab);
    _mm_storeu_ps(tmp_aa, sum_aa);
    _mm_storeu_ps(tmp_bb, sum_bb);

    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (int j = 0; j < 4; j++) {
        dot += tmp_ab[j];
        norm_a += tmp_aa[j];
        norm_b += tmp_bb[j];
    }

    for (; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    if (denom < 1e-12f) return 0.0f;
    return dot / denom;
}

#else
/* Pure scalar fallback */
static float cosine_similarity(const float* a, const float* b, uint32_t dim) {
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    if (denom < 1e-12f) return 0.0f;
    return dot / denom;
}
#endif

/* ============================================================================
 * SQL Schema
 * ============================================================================ */

static const char* SCHEMA_SQL =
    /* Engrams table */
    "CREATE TABLE IF NOT EXISTS engrams ("
    "  engram_id INTEGER PRIMARY KEY,"
    "  timestamp_us INTEGER NOT NULL,"
    "  memory_type INTEGER,"
    "  state INTEGER,"
    "  neuron_count INTEGER,"
    "  neuron_ids BLOB,"
    "  activations BLOB,"
    "  embedding BLOB,"
    "  embedding_dim INTEGER,"
    "  valence REAL,"
    "  arousal REAL,"
    "  intensity REAL,"
    "  consolidation_strength REAL,"
    "  decay_rate REAL,"
    "  vividness REAL,"
    "  importance REAL,"
    "  recall_count INTEGER DEFAULT 0,"
    "  last_recall_us INTEGER DEFAULT 0,"
    "  label TEXT,"
    "  source_device_id INTEGER DEFAULT 0"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_engrams_time ON engrams(timestamp_us);"
    "CREATE INDEX IF NOT EXISTS idx_engrams_importance ON engrams(importance DESC);"
    "CREATE INDEX IF NOT EXISTS idx_engrams_state ON engrams(state);"

    /* Semantic concepts */
    "CREATE TABLE IF NOT EXISTS concepts ("
    "  concept_id INTEGER PRIMARY KEY,"
    "  timestamp_us INTEGER NOT NULL,"
    "  label TEXT,"
    "  category INTEGER,"
    "  embedding BLOB,"
    "  embedding_dim INTEGER,"
    "  base_activation REAL DEFAULT 0.5,"
    "  access_count INTEGER DEFAULT 0,"
    "  source_engram_id INTEGER DEFAULT 0,"
    "  source_device_id INTEGER DEFAULT 0"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_concepts_label ON concepts(label);"

    /* Relations (knowledge graph edges) */
    "CREATE TABLE IF NOT EXISTS relations ("
    "  relation_id INTEGER PRIMARY KEY,"
    "  source_concept_id INTEGER NOT NULL,"
    "  target_concept_id INTEGER NOT NULL,"
    "  relation_type INTEGER,"
    "  strength REAL DEFAULT 0.5,"
    "  timestamp_us INTEGER,"
    "  FOREIGN KEY (source_concept_id) REFERENCES concepts(concept_id),"
    "  FOREIGN KEY (target_concept_id) REFERENCES concepts(concept_id)"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_relations_source ON relations(source_concept_id);"
    "CREATE INDEX IF NOT EXISTS idx_relations_target ON relations(target_concept_id);"

    /* Autobiographical memories */
    "CREATE TABLE IF NOT EXISTS autobio ("
    "  memory_id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  timestamp_us INTEGER NOT NULL,"
    "  memory_type INTEGER,"
    "  what_happened TEXT,"
    "  why_it_happened TEXT,"
    "  outcome TEXT,"
    "  valence INTEGER,"
    "  importance REAL,"
    "  arousal REAL,"
    "  emotional_intensity REAL,"
    "  identity_defining INTEGER DEFAULT 0,"
    "  is_core_memory INTEGER DEFAULT 0,"
    "  source_device_id INTEGER DEFAULT 0"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_autobio_time ON autobio(timestamp_us);"
    "CREATE INDEX IF NOT EXISTS idx_autobio_importance ON autobio(importance DESC);"

    /* Metadata catalog — unified cross-type index */
    "CREATE TABLE IF NOT EXISTS metadata ("
    "  rowid INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  entry_id INTEGER NOT NULL,"
    "  entry_type INTEGER NOT NULL,"   /* 0=engram, 1=concept, 2=relation, 3=autobio */
    "  timestamp_us INTEGER NOT NULL,"
    "  importance REAL DEFAULT 0.5,"
    "  source_device_id INTEGER DEFAULT 0,"
    "  training_step INTEGER DEFAULT 0,"
    "  curriculum_stage INTEGER DEFAULT 0,"
    "  model_version INTEGER DEFAULT 0,"
    "  label TEXT,"
    "  tags TEXT"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_meta_type ON metadata(entry_type);"
    "CREATE INDEX IF NOT EXISTS idx_meta_step ON metadata(training_step);"
    "CREATE INDEX IF NOT EXISTS idx_meta_device ON metadata(source_device_id);"
    "CREATE INDEX IF NOT EXISTS idx_meta_importance ON metadata(importance DESC);"

    /* Security audit trail — append-only event log */
    "CREATE TABLE IF NOT EXISTS audit_log ("
    "  rowid INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  timestamp_us INTEGER NOT NULL,"
    "  event_type INTEGER NOT NULL,"
    "  source_module_id INTEGER DEFAULT 0,"
    "  description TEXT,"
    "  details TEXT"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_audit_time ON audit_log(timestamp_us);"
    "CREATE INDEX IF NOT EXISTS idx_audit_type ON audit_log(event_type);";

static const char* FTS_SCHEMA_SQL =
    /* FTS5 for full-text search on engram labels */
    "CREATE VIRTUAL TABLE IF NOT EXISTS engrams_fts USING fts5("
    "  label, content=engrams, content_rowid=engram_id"
    ");"
    /* FTS5 for concepts */
    "CREATE VIRTUAL TABLE IF NOT EXISTS concepts_fts USING fts5("
    "  label, content=concepts, content_rowid=concept_id"
    ");"
    /* FTS5 for autobiographical search */
    "CREATE VIRTUAL TABLE IF NOT EXISTS autobio_fts USING fts5("
    "  what_happened, why_it_happened, outcome,"
    "  content=autobio, content_rowid=memory_id"
    ");"
    /* FTS5 for metadata catalog (cross-type search) */
    "CREATE VIRTUAL TABLE IF NOT EXISTS metadata_fts USING fts5("
    "  label, tags, content=metadata, content_rowid=rowid"
    ");";

/* ============================================================================
 * Prepared Statement SQL
 * ============================================================================ */

static const char* SQL_ENGRAM_INSERT =
    "INSERT OR REPLACE INTO engrams "
    "(engram_id, timestamp_us, memory_type, state, neuron_count, "
    " neuron_ids, activations, embedding, embedding_dim, "
    " valence, arousal, intensity, consolidation_strength, "
    " decay_rate, vividness, importance, recall_count, last_recall_us, label, source_device_id) "
    "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

static const char* SQL_ENGRAM_GET =
    "SELECT engram_id, timestamp_us, memory_type, state, neuron_count, "
    "  neuron_ids, activations, embedding, embedding_dim, "
    "  valence, arousal, intensity, consolidation_strength, "
    "  decay_rate, vividness, importance, recall_count, last_recall_us, label, source_device_id "
    "FROM engrams WHERE engram_id = ?";

static const char* SQL_ENGRAM_UPDATE =
    "UPDATE engrams SET "
    "  state = ?, consolidation_strength = ?, decay_rate = ?, "
    "  vividness = ?, importance = ?, recall_count = ?, last_recall_us = ? "
    "WHERE engram_id = ?";

static const char* SQL_CONCEPT_INSERT =
    "INSERT OR REPLACE INTO concepts "
    "(concept_id, timestamp_us, label, category, embedding, embedding_dim, "
    " base_activation, access_count, source_engram_id, source_device_id) "
    "VALUES (?,?,?,?,?,?,?,?,?,?)";

static const char* SQL_CONCEPT_GET =
    "SELECT concept_id, timestamp_us, label, category, embedding, embedding_dim, "
    "  base_activation, access_count, source_engram_id, source_device_id "
    "FROM concepts WHERE concept_id = ?";

static const char* SQL_RELATION_INSERT =
    "INSERT OR REPLACE INTO relations "
    "(relation_id, source_concept_id, target_concept_id, relation_type, strength, timestamp_us) "
    "VALUES (?,?,?,?,?,?)";

static const char* SQL_AUTOBIO_INSERT =
    "INSERT INTO autobio "
    "(timestamp_us, memory_type, what_happened, why_it_happened, outcome, "
    " valence, importance, arousal, emotional_intensity, "
    " identity_defining, is_core_memory, source_device_id) "
    "VALUES (?,?,?,?,?,?,?,?,?,?,?,?)";

/* ============================================================================
 * Helper: execute a multi-statement SQL string
 * ============================================================================ */

static int exec_sql(sqlite3* db, const char* sql) {
    char* err_msg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        LOG_ERROR("SQLite exec error: %s (sql truncated: %.80s...)", err_msg ? err_msg : "unknown", sql);
        sqlite3_free(err_msg);
        return -1;
    }
    return 0;
}

/* ============================================================================
 * Helper: allocate a search result
 * ============================================================================ */

static nimcp_memory_search_result_t* search_result_create(uint32_t capacity) {
    nimcp_memory_search_result_t* r = (nimcp_memory_search_result_t*)nimcp_calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->capacity = capacity;
    r->count = 0;
    r->ids = (uint64_t*)nimcp_calloc(capacity, sizeof(uint64_t));
    r->distances = (float*)nimcp_calloc(capacity, sizeof(float));
    if (!r->ids || !r->distances) {
        nimcp_free(r->ids);
        nimcp_free(r->distances);
        nimcp_free(r);
        return NULL;
    }
    return r;
}

/* ============================================================================
 * Helper: top-K insertion (maintain sorted by descending similarity)
 * ============================================================================ */

static void topk_insert(nimcp_memory_search_result_t* r, uint64_t id, float similarity, uint32_t top_k) {
    /* distance = 1 - similarity (lower distance = more similar) */
    float distance = 1.0f - similarity;

    if (r->count < top_k) {
        /* Still filling — just append and maintain sorted order */
        uint32_t pos = r->count;
        while (pos > 0 && r->distances[pos - 1] > distance) {
            r->ids[pos] = r->ids[pos - 1];
            r->distances[pos] = r->distances[pos - 1];
            pos--;
        }
        r->ids[pos] = id;
        r->distances[pos] = distance;
        r->count++;
    } else if (distance < r->distances[r->count - 1]) {
        /* Better than worst — replace and re-sort */
        uint32_t pos = r->count - 1;
        r->ids[pos] = id;
        r->distances[pos] = distance;
        while (pos > 0 && r->distances[pos - 1] > distance) {
            /* Swap */
            uint64_t tmp_id = r->ids[pos - 1];
            float tmp_d = r->distances[pos - 1];
            r->ids[pos - 1] = r->ids[pos];
            r->distances[pos - 1] = r->distances[pos];
            r->ids[pos] = tmp_id;
            r->distances[pos] = tmp_d;
            pos--;
        }
    }
}

/* ============================================================================
 * Lifecycle: config default
 * ============================================================================ */

nimcp_memory_store_config_t nimcp_memory_store_config_default(void) {
    nimcp_memory_store_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.db_path = "nimcp_memory.db";
    cfg.hot_cache_size = 512;
    cfg.write_buffer_size = 100;
    cfg.write_flush_interval_ms = 100;
    cfg.embedding_dim = 1024;
    cfg.enable_fts = true;
    cfg.enable_wal = true;
    cfg.enable_questdb_sync = false;
    cfg.enable_bloom_filter = true;
    cfg.bloom_filter_size = 1048576; /* 1M bits = 128 KB */
    cfg.importance_index_threshold = 0.1f;
    cfg.questdb_host = NULL;
    cfg.questdb_ilp_port = 9009;
    return cfg;
}

/* ============================================================================
 * Lifecycle: create
 * ============================================================================ */

nimcp_memory_store_t* nimcp_memory_store_create(const nimcp_memory_store_config_t* config) {
    if (!config || !config->db_path) {
        LOG_ERROR("memory_store_create: NULL config or db_path");
        return NULL;
    }

    nimcp_memory_store_t* store = (nimcp_memory_store_t*)nimcp_calloc(1, sizeof(*store));
    if (!store) {
        LOG_ERROR("memory_store_create: allocation failed");
        return NULL;
    }

    store->config = *config;
    store->healthy = true;

    /* Open SQLite database */
    int rc = sqlite3_open(config->db_path, &store->db);
    if (rc != SQLITE_OK) {
        LOG_ERROR("memory_store_create: sqlite3_open failed: %s", sqlite3_errmsg(store->db));
        sqlite3_close(store->db);
        nimcp_free(store);
        return NULL;
    }

    /* Enable WAL mode for concurrent reads */
    if (config->enable_wal) {
        exec_sql(store->db, "PRAGMA journal_mode=WAL;");
    }
    exec_sql(store->db, "PRAGMA page_size=4096;");
    exec_sql(store->db, "PRAGMA synchronous=NORMAL;");
    exec_sql(store->db, "PRAGMA cache_size=-8192;"); /* 8 MB cache */
    exec_sql(store->db, "PRAGMA foreign_keys=ON;");

    /* Create tables */
    if (exec_sql(store->db, SCHEMA_SQL) != 0) {
        LOG_ERROR("memory_store_create: schema creation failed");
        sqlite3_close(store->db);
        nimcp_free(store);
        return NULL;
    }

    /* Create FTS5 virtual tables */
    if (config->enable_fts) {
        if (exec_sql(store->db, FTS_SCHEMA_SQL) != 0) {
            LOG_WARN("memory_store_create: FTS5 creation failed (may not be compiled in)");
            /* Non-fatal: store works without FTS */
        }
    }

    /* Prepare statements */
    int prep_ok = 1;
    prep_ok &= (sqlite3_prepare_v2(store->db, SQL_ENGRAM_INSERT, -1, &store->stmt_engram_insert, NULL) == SQLITE_OK);
    prep_ok &= (sqlite3_prepare_v2(store->db, SQL_ENGRAM_GET, -1, &store->stmt_engram_get, NULL) == SQLITE_OK);
    prep_ok &= (sqlite3_prepare_v2(store->db, SQL_ENGRAM_UPDATE, -1, &store->stmt_engram_update, NULL) == SQLITE_OK);
    prep_ok &= (sqlite3_prepare_v2(store->db, SQL_CONCEPT_INSERT, -1, &store->stmt_concept_insert, NULL) == SQLITE_OK);
    prep_ok &= (sqlite3_prepare_v2(store->db, SQL_CONCEPT_GET, -1, &store->stmt_concept_get, NULL) == SQLITE_OK);
    prep_ok &= (sqlite3_prepare_v2(store->db, SQL_RELATION_INSERT, -1, &store->stmt_relation_insert, NULL) == SQLITE_OK);
    prep_ok &= (sqlite3_prepare_v2(store->db, SQL_AUTOBIO_INSERT, -1, &store->stmt_autobio_insert, NULL) == SQLITE_OK);

    if (!prep_ok) {
        LOG_ERROR("memory_store_create: prepared statement compilation failed: %s", sqlite3_errmsg(store->db));
        sqlite3_close(store->db);
        nimcp_free(store);
        return NULL;
    }

    /* Allocate write buffers */
    uint32_t buf_size = config->write_buffer_size > 0 ? config->write_buffer_size : 100;
    store->engram_buffer = (nimcp_engram_record_t*)nimcp_calloc(buf_size, sizeof(nimcp_engram_record_t));
    store->concept_buffer = (nimcp_concept_record_t*)nimcp_calloc(buf_size, sizeof(nimcp_concept_record_t));
    store->engram_buffer_count = 0;
    store->concept_buffer_count = 0;

    if (!store->engram_buffer || !store->concept_buffer) {
        LOG_ERROR("memory_store_create: buffer allocation failed");
        sqlite3_finalize(store->stmt_engram_insert);
        sqlite3_finalize(store->stmt_engram_get);
        sqlite3_finalize(store->stmt_engram_update);
        sqlite3_finalize(store->stmt_concept_insert);
        sqlite3_finalize(store->stmt_concept_get);
        sqlite3_finalize(store->stmt_relation_insert);
        sqlite3_finalize(store->stmt_autobio_insert);
        sqlite3_close(store->db);
        nimcp_free(store->engram_buffer);
        nimcp_free(store->concept_buffer);
        nimcp_free(store);
        return NULL;
    }

    /* Allocate bloom filter */
    if (config->enable_bloom_filter) {
        store->bloom_size = config->bloom_filter_size > 0 ? config->bloom_filter_size : 1048576;
        uint32_t bloom_bytes = (store->bloom_size + 7) / 8;
        store->bloom_filter = (uint8_t*)nimcp_calloc(bloom_bytes, 1);
        if (!store->bloom_filter) {
            LOG_WARN("memory_store_create: bloom filter allocation failed, disabling");
            store->bloom_size = 0;
        }
    }

    /* Initialize mutex */
    store->mutex = nimcp_mutex_create(NULL);
    if (!store->mutex) {
        LOG_ERROR("memory_store_create: mutex creation failed");
        /* Continue without mutex — single-threaded use still works */
    }

    memset(&store->stats, 0, sizeof(store->stats));

    LOG_INFO("memory_store_create: opened %s (WAL=%d, FTS=%d, bloom=%u bits)",
             config->db_path, config->enable_wal, config->enable_fts, store->bloom_size);

    return store;
}

/* ============================================================================
 * Lifecycle: destroy
 * ============================================================================ */

void nimcp_memory_store_destroy(nimcp_memory_store_t* store) {
    if (!store) return;

    /* Flush remaining buffered writes */
    nimcp_memory_store_flush(store);

    /* Finalize prepared statements */
    sqlite3_finalize(store->stmt_engram_insert);
    sqlite3_finalize(store->stmt_engram_get);
    sqlite3_finalize(store->stmt_engram_update);
    sqlite3_finalize(store->stmt_concept_insert);
    sqlite3_finalize(store->stmt_concept_get);
    sqlite3_finalize(store->stmt_relation_insert);
    sqlite3_finalize(store->stmt_autobio_insert);

    /* Close database */
    if (store->db) {
        sqlite3_close(store->db);
    }

    /* Free buffers */
    nimcp_free(store->engram_buffer);
    nimcp_free(store->concept_buffer);
    nimcp_free(store->bloom_filter);

    /* Free mutex */
    if (store->mutex) {
        nimcp_mutex_free(store->mutex);
    }

    nimcp_free(store);
}

/* ============================================================================
 * Flush: write buffered records to SQLite in a single transaction
 * ============================================================================ */

int nimcp_memory_store_flush(nimcp_memory_store_t* store) {
    if (!store) return -1;
    if (store->engram_buffer_count == 0 && store->concept_buffer_count == 0) {
        return 0; /* Nothing to flush */
    }

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    exec_sql(store->db, "BEGIN TRANSACTION;");
    uint32_t flush_errors = 0;

    /* Flush engram buffer */
    for (uint32_t i = 0; i < store->engram_buffer_count; i++) {
        nimcp_engram_record_t* e = &store->engram_buffer[i];

        /* V5 Security: Ensure null-terminated labels before SQLite binding */
        e->label[sizeof(e->label) - 1] = '\0';

        sqlite3_stmt* s = store->stmt_engram_insert;
        sqlite3_reset(s);

        sqlite3_bind_int64(s, 1, (sqlite3_int64)e->engram_id);
        sqlite3_bind_int64(s, 2, (sqlite3_int64)e->timestamp_us);
        sqlite3_bind_int(s, 3, (int)e->memory_type);
        sqlite3_bind_int(s, 4, (int)e->state);
        sqlite3_bind_int(s, 5, (int)e->neuron_count);

        if (e->neuron_ids && e->neuron_count > 0) {
            sqlite3_bind_blob(s, 6, e->neuron_ids, (int)(e->neuron_count * sizeof(uint32_t)), SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(s, 6);
        }

        if (e->activations && e->neuron_count > 0) {
            sqlite3_bind_blob(s, 7, e->activations, (int)(e->neuron_count * sizeof(float)), SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(s, 7);
        }

        if (e->embedding && e->embedding_dim > 0) {
            sqlite3_bind_blob(s, 8, e->embedding, (int)(e->embedding_dim * sizeof(float)), SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(s, 8);
        }

        sqlite3_bind_int(s, 9, (int)e->embedding_dim);
        sqlite3_bind_double(s, 10, (double)e->valence);
        sqlite3_bind_double(s, 11, (double)e->arousal);
        sqlite3_bind_double(s, 12, (double)e->intensity);
        sqlite3_bind_double(s, 13, (double)e->consolidation_strength);
        sqlite3_bind_double(s, 14, (double)e->decay_rate);
        sqlite3_bind_double(s, 15, (double)e->vividness);
        sqlite3_bind_double(s, 16, (double)e->importance);
        sqlite3_bind_int(s, 17, (int)e->recall_count);
        sqlite3_bind_int64(s, 18, (sqlite3_int64)e->last_recall_us);
        sqlite3_bind_text(s, 19, e->label, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(s, 20, (int)e->source_device_id);

        int rc = sqlite3_step(s);
        if (rc != SQLITE_DONE) {
            LOG_WARN("memory_store_flush: engram insert failed (id=%lu): %s",
                     (unsigned long)e->engram_id, sqlite3_errmsg(store->db));
            flush_errors++;
        }

        /* Insert into FTS5 index (parameterized to prevent SQL injection) */
        if (store->config.enable_fts && e->label[0] != '\0') {
            sqlite3_stmt* fts_stmt = NULL;
            if (sqlite3_prepare_v2(store->db,
                    "INSERT INTO engrams_fts(rowid, label) VALUES (?, ?)",
                    -1, &fts_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(fts_stmt, 1, (sqlite3_int64)e->engram_id);
                sqlite3_bind_text(fts_stmt, 2, e->label, -1, SQLITE_TRANSIENT);
                sqlite3_step(fts_stmt);
                sqlite3_finalize(fts_stmt);
            }
        }
    }
    store->engram_buffer_count = 0;

    /* Flush concept buffer */
    for (uint32_t i = 0; i < store->concept_buffer_count; i++) {
        nimcp_concept_record_t* c = &store->concept_buffer[i];

        /* V5 Security: Ensure null-terminated labels before SQLite binding */
        c->label[sizeof(c->label) - 1] = '\0';

        sqlite3_stmt* s = store->stmt_concept_insert;
        sqlite3_reset(s);

        sqlite3_bind_int64(s, 1, (sqlite3_int64)c->concept_id);
        sqlite3_bind_int64(s, 2, (sqlite3_int64)c->timestamp_us);
        sqlite3_bind_text(s, 3, c->label, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(s, 4, (int)c->category);

        if (c->embedding && c->embedding_dim > 0) {
            sqlite3_bind_blob(s, 5, c->embedding, (int)(c->embedding_dim * sizeof(float)), SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(s, 5);
        }

        sqlite3_bind_int(s, 6, (int)c->embedding_dim);
        sqlite3_bind_double(s, 7, (double)c->base_activation);
        sqlite3_bind_int(s, 8, (int)c->access_count);
        sqlite3_bind_int64(s, 9, (sqlite3_int64)c->source_engram_id);
        sqlite3_bind_int(s, 10, (int)c->source_device_id);

        int rc = sqlite3_step(s);
        if (rc != SQLITE_DONE) {
            LOG_WARN("memory_store_flush: concept insert failed (id=%lu): %s",
                     (unsigned long)c->concept_id, sqlite3_errmsg(store->db));
            flush_errors++;
        }

        /* Insert into concepts FTS5 index (parameterized) */
        if (store->config.enable_fts && c->label[0] != '\0') {
            sqlite3_stmt* fts_stmt = NULL;
            if (sqlite3_prepare_v2(store->db,
                    "INSERT INTO concepts_fts(rowid, label) VALUES (?, ?)",
                    -1, &fts_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(fts_stmt, 1, (sqlite3_int64)c->concept_id);
                sqlite3_bind_text(fts_stmt, 2, c->label, -1, SQLITE_TRANSIENT);
                sqlite3_step(fts_stmt);
                sqlite3_finalize(fts_stmt);
            }
        }
    }
    store->concept_buffer_count = 0;

    if (flush_errors > 0) {
        exec_sql(store->db, "ROLLBACK;");
        store->healthy = false;
        LOG_WARN("memory_store_flush: rolled back transaction (%u errors) — "
                 "marking store unhealthy", flush_errors);
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return -1;
    }

    exec_sql(store->db, "COMMIT;");
    store->stats.write_buffer_flushes++;

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return 0;
}

/* ============================================================================
 * Checkpoint: flush + WAL checkpoint
 * ============================================================================ */

int nimcp_memory_store_checkpoint(nimcp_memory_store_t* store) {
    if (!store) return -1;

    int ret = nimcp_memory_store_flush(store);
    if (ret != 0) return ret;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    if (store->config.enable_wal) {
        int wal_pages = 0, checkpointed = 0;
        int rc = sqlite3_wal_checkpoint_v2(store->db, NULL, SQLITE_CHECKPOINT_PASSIVE,
                                            &wal_pages, &checkpointed);
        if (rc != SQLITE_OK) {
            LOG_WARN("memory_store_checkpoint: WAL checkpoint failed: %s", sqlite3_errmsg(store->db));
        } else {
            LOG_INFO("memory_store_checkpoint: WAL %d pages, %d checkpointed", wal_pages, checkpointed);
        }
    }

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return 0;
}

/* ============================================================================
 * Engram Operations
 * ============================================================================ */

int nimcp_memory_store_engram_put(nimcp_memory_store_t* store, const nimcp_engram_record_t* record) {
    if (!store || !record) return -1;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    uint32_t buf_size = store->config.write_buffer_size > 0 ? store->config.write_buffer_size : 100;

    /* Copy record into write buffer (shallow copy — blobs are TRANSIENT on flush) */
    store->engram_buffer[store->engram_buffer_count] = *record;
    store->engram_buffer_count++;
    store->stats.total_writes++;

    /* Add embedding to bloom filter */
    if (store->bloom_filter && record->embedding && record->embedding_dim > 0) {
        nimcp_memory_store_bloom_add(store, record->embedding, record->embedding_dim);
    }

    bool need_flush = (store->engram_buffer_count >= buf_size);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    if (need_flush) {
        nimcp_memory_store_flush(store);
    }

    return 0;
}

int nimcp_memory_store_engram_get(nimcp_memory_store_t* store, uint64_t engram_id, nimcp_engram_record_t* record) {
    if (!store || !record) return -1;

    /* Flush first to ensure we can read recently buffered data */
    nimcp_memory_store_flush(store);

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    sqlite3_stmt* s = store->stmt_engram_get;
    sqlite3_reset(s);
    sqlite3_bind_int64(s, 1, (sqlite3_int64)engram_id);

    int rc = sqlite3_step(s);
    if (rc != SQLITE_ROW) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        store->stats.cache_misses++;
        return -1; /* Not found */
    }

    memset(record, 0, sizeof(*record));

    record->engram_id = (uint64_t)sqlite3_column_int64(s, 0);
    record->timestamp_us = (uint64_t)sqlite3_column_int64(s, 1);
    record->memory_type = (uint32_t)sqlite3_column_int(s, 2);
    record->state = (uint32_t)sqlite3_column_int(s, 3);
    record->neuron_count = (uint32_t)sqlite3_column_int(s, 4);

    /* Neuron IDs blob */
    int blob_size = sqlite3_column_bytes(s, 5);
    if (blob_size > 0 && record->neuron_count > 0) {
        record->neuron_ids = (uint32_t*)nimcp_malloc((size_t)blob_size);
        if (record->neuron_ids) {
            memcpy(record->neuron_ids, sqlite3_column_blob(s, 5), (size_t)blob_size);
        }
    }

    /* Activations blob */
    blob_size = sqlite3_column_bytes(s, 6);
    if (blob_size > 0 && record->neuron_count > 0) {
        record->activations = (float*)nimcp_malloc((size_t)blob_size);
        if (record->activations) {
            memcpy(record->activations, sqlite3_column_blob(s, 6), (size_t)blob_size);
        }
    }

    /* Embedding blob */
    record->embedding_dim = (uint32_t)sqlite3_column_int(s, 8);
    blob_size = sqlite3_column_bytes(s, 7);
    if (blob_size > 0 && record->embedding_dim > 0) {
        record->embedding = (float*)nimcp_malloc((size_t)blob_size);
        if (record->embedding) {
            memcpy(record->embedding, sqlite3_column_blob(s, 7), (size_t)blob_size);
        }
    }

    record->valence = (float)sqlite3_column_double(s, 9);
    record->arousal = (float)sqlite3_column_double(s, 10);
    record->intensity = (float)sqlite3_column_double(s, 11);
    record->consolidation_strength = (float)sqlite3_column_double(s, 12);
    record->decay_rate = (float)sqlite3_column_double(s, 13);
    record->vividness = (float)sqlite3_column_double(s, 14);
    record->importance = (float)sqlite3_column_double(s, 15);
    record->recall_count = (uint32_t)sqlite3_column_int(s, 16);
    record->last_recall_us = (uint64_t)sqlite3_column_int64(s, 17);

    const char* label_text = (const char*)sqlite3_column_text(s, 18);
    if (label_text) {
        strncpy(record->label, label_text, sizeof(record->label) - 1);
        record->label[sizeof(record->label) - 1] = '\0';
    }

    record->source_device_id = (uint32_t)sqlite3_column_int(s, 19);

    store->stats.total_reads++;
    store->stats.cache_hits++;

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return 0;
}

int nimcp_memory_store_engram_update(nimcp_memory_store_t* store, const nimcp_engram_record_t* record) {
    if (!store || !record) return -1;

    /* Flush first to avoid stale buffer data */
    nimcp_memory_store_flush(store);

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    sqlite3_stmt* s = store->stmt_engram_update;
    sqlite3_reset(s);

    sqlite3_bind_int(s, 1, (int)record->state);
    sqlite3_bind_double(s, 2, (double)record->consolidation_strength);
    sqlite3_bind_double(s, 3, (double)record->decay_rate);
    sqlite3_bind_double(s, 4, (double)record->vividness);
    sqlite3_bind_double(s, 5, (double)record->importance);
    sqlite3_bind_int(s, 6, (int)record->recall_count);
    sqlite3_bind_int64(s, 7, (sqlite3_int64)record->last_recall_us);
    sqlite3_bind_int64(s, 8, (sqlite3_int64)record->engram_id);

    int rc = sqlite3_step(s);
    int ret = (rc == SQLITE_DONE) ? 0 : -1;
    if (ret != 0) {
        LOG_WARN("memory_store engram_update failed (id=%lu): %s",
                 (unsigned long)record->engram_id, sqlite3_errmsg(store->db));
    }

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return ret;
}

/* ============================================================================
 * Vector Similarity Search (brute-force cosine with SIMD)
 * ============================================================================ */

nimcp_memory_search_result_t* nimcp_memory_store_engram_search_similar(
    nimcp_memory_store_t* store, const float* query_embedding, uint32_t dim,
    uint32_t top_k, float threshold)
{
    if (!store || !query_embedding || dim == 0 || top_k == 0) return NULL;

    /* Flush so all buffered data is searchable */
    nimcp_memory_store_flush(store);

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    nimcp_memory_search_result_t* result = search_result_create(top_k);
    if (!result) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return NULL;
    }

    /* Query candidates: importance > threshold, ordered by importance */
    const char* sql = "SELECT engram_id, embedding, embedding_dim FROM engrams "
                      "WHERE embedding IS NOT NULL AND importance >= ? "
                      "ORDER BY importance DESC LIMIT 100000";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        LOG_WARN("memory_store engram_search_similar: prepare failed: %s", sqlite3_errmsg(store->db));
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        nimcp_memory_search_result_destroy(result);
        return NULL;
    }

    sqlite3_bind_double(stmt, 1, (double)store->config.importance_index_threshold);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        uint64_t eid = (uint64_t)sqlite3_column_int64(stmt, 0);
        uint32_t edim = (uint32_t)sqlite3_column_int(stmt, 2);
        int blob_bytes = sqlite3_column_bytes(stmt, 1);

        if (edim != dim || blob_bytes != (int)(edim * sizeof(float))) continue;

        const float* emb = (const float*)sqlite3_column_blob(stmt, 1);
        if (!emb) continue;

        float sim = cosine_similarity(query_embedding, emb, dim);
        if (sim >= threshold) {
            topk_insert(result, eid, sim, top_k);
        }
    }

    sqlite3_finalize(stmt);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return result;
}

/* ============================================================================
 * Full-Text Search
 * ============================================================================ */

nimcp_memory_search_result_t* nimcp_memory_store_engram_search_text(
    nimcp_memory_store_t* store, const char* query, uint32_t max_results)
{
    if (!store || !query || max_results == 0) return NULL;

    nimcp_memory_store_flush(store);

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    nimcp_memory_search_result_t* result = search_result_create(max_results);
    if (!result) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return NULL;
    }

    const char* sql;
    if (store->config.enable_fts) {
        sql = "SELECT rowid FROM engrams_fts WHERE engrams_fts MATCH ? LIMIT ?";
    } else {
        sql = "SELECT engram_id FROM engrams WHERE label LIKE '%' || ? || '%' LIMIT ?";
    }

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        LOG_WARN("memory_store engram_search_text: prepare failed: %s", sqlite3_errmsg(store->db));
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        nimcp_memory_search_result_destroy(result);
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, query, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, (int)max_results);

    while (sqlite3_step(stmt) == SQLITE_ROW && result->count < max_results) {
        result->ids[result->count] = (uint64_t)sqlite3_column_int64(stmt, 0);
        result->distances[result->count] = 0.0f; /* FTS rank not returned here */
        result->count++;
    }

    sqlite3_finalize(stmt);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return result;
}

/* ============================================================================
 * Time-Range Search
 * ============================================================================ */

nimcp_memory_search_result_t* nimcp_memory_store_engram_search_time(
    nimcp_memory_store_t* store, uint64_t start_us, uint64_t end_us,
    uint32_t max_results)
{
    if (!store || max_results == 0) return NULL;

    nimcp_memory_store_flush(store);

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    nimcp_memory_search_result_t* result = search_result_create(max_results);
    if (!result) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return NULL;
    }

    const char* sql = "SELECT engram_id FROM engrams "
                      "WHERE timestamp_us BETWEEN ? AND ? "
                      "ORDER BY timestamp_us DESC LIMIT ?";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        nimcp_memory_search_result_destroy(result);
        return NULL;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)start_us);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)end_us);
    sqlite3_bind_int(stmt, 3, (int)max_results);

    while (sqlite3_step(stmt) == SQLITE_ROW && result->count < max_results) {
        result->ids[result->count] = (uint64_t)sqlite3_column_int64(stmt, 0);
        result->distances[result->count] = 0.0f;
        result->count++;
    }

    sqlite3_finalize(stmt);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return result;
}

/* ============================================================================
 * Engram Prune
 * ============================================================================ */

int nimcp_memory_store_engram_prune(nimcp_memory_store_t* store,
    float min_importance, uint32_t min_recall_count, uint64_t older_than_us)
{
    if (!store) return -1;

    nimcp_memory_store_flush(store);

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    const char* sql = "DELETE FROM engrams "
                      "WHERE importance < ? AND recall_count < ? AND timestamp_us < ?";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return -1;
    }

    sqlite3_bind_double(stmt, 1, (double)min_importance);
    sqlite3_bind_int(stmt, 2, (int)min_recall_count);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)older_than_us);

    rc = sqlite3_step(stmt);
    int deleted = sqlite3_changes(store->db);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_WARN("memory_store engram_prune failed: %s", sqlite3_errmsg(store->db));
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return -1;
    }

    LOG_INFO("memory_store engram_prune: deleted %d engrams", deleted);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return deleted;
}

/* ============================================================================
 * Concept Operations
 * ============================================================================ */

int nimcp_memory_store_concept_put(nimcp_memory_store_t* store, const nimcp_concept_record_t* record) {
    if (!store || !record) return -1;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    uint32_t buf_size = store->config.write_buffer_size > 0 ? store->config.write_buffer_size : 100;

    store->concept_buffer[store->concept_buffer_count] = *record;
    store->concept_buffer_count++;
    store->stats.total_writes++;

    bool need_flush = (store->concept_buffer_count >= buf_size);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    if (need_flush) {
        nimcp_memory_store_flush(store);
    }

    return 0;
}

int nimcp_memory_store_concept_get(nimcp_memory_store_t* store, uint64_t concept_id, nimcp_concept_record_t* record) {
    if (!store || !record) return -1;

    nimcp_memory_store_flush(store);

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    sqlite3_stmt* s = store->stmt_concept_get;
    sqlite3_reset(s);
    sqlite3_bind_int64(s, 1, (sqlite3_int64)concept_id);

    int rc = sqlite3_step(s);
    if (rc != SQLITE_ROW) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return -1;
    }

    memset(record, 0, sizeof(*record));

    record->concept_id = (uint64_t)sqlite3_column_int64(s, 0);
    record->timestamp_us = (uint64_t)sqlite3_column_int64(s, 1);

    const char* label_text = (const char*)sqlite3_column_text(s, 2);
    if (label_text) {
        strncpy(record->label, label_text, sizeof(record->label) - 1);
        record->label[sizeof(record->label) - 1] = '\0';
    }

    record->category = (uint32_t)sqlite3_column_int(s, 3);

    record->embedding_dim = (uint32_t)sqlite3_column_int(s, 5);
    int blob_bytes = sqlite3_column_bytes(s, 4);
    if (blob_bytes > 0 && record->embedding_dim > 0) {
        record->embedding = (float*)nimcp_malloc((size_t)blob_bytes);
        if (record->embedding) {
            memcpy(record->embedding, sqlite3_column_blob(s, 4), (size_t)blob_bytes);
        }
    }

    record->base_activation = (float)sqlite3_column_double(s, 6);
    record->access_count = (uint32_t)sqlite3_column_int(s, 7);
    record->source_engram_id = (uint64_t)sqlite3_column_int64(s, 8);
    record->source_device_id = (uint32_t)sqlite3_column_int(s, 9);

    store->stats.total_reads++;

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return 0;
}

nimcp_memory_search_result_t* nimcp_memory_store_concept_search_similar(
    nimcp_memory_store_t* store, const float* query_embedding, uint32_t dim,
    uint32_t top_k, float threshold)
{
    if (!store || !query_embedding || dim == 0 || top_k == 0) return NULL;

    nimcp_memory_store_flush(store);

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    nimcp_memory_search_result_t* result = search_result_create(top_k);
    if (!result) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return NULL;
    }

    const char* sql = "SELECT concept_id, embedding, embedding_dim FROM concepts "
                      "WHERE embedding IS NOT NULL LIMIT 100000";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        nimcp_memory_search_result_destroy(result);
        return NULL;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        uint64_t cid = (uint64_t)sqlite3_column_int64(stmt, 0);
        uint32_t edim = (uint32_t)sqlite3_column_int(stmt, 2);
        int blob_bytes = sqlite3_column_bytes(stmt, 1);

        if (edim != dim || blob_bytes != (int)(edim * sizeof(float))) continue;

        const float* emb = (const float*)sqlite3_column_blob(stmt, 1);
        if (!emb) continue;

        float sim = cosine_similarity(query_embedding, emb, dim);
        if (sim >= threshold) {
            topk_insert(result, cid, sim, top_k);
        }
    }

    sqlite3_finalize(stmt);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return result;
}

nimcp_memory_search_result_t* nimcp_memory_store_concept_search_text(
    nimcp_memory_store_t* store, const char* label_query, uint32_t max_results)
{
    if (!store || !label_query || max_results == 0) return NULL;

    nimcp_memory_store_flush(store);

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    nimcp_memory_search_result_t* result = search_result_create(max_results);
    if (!result) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return NULL;
    }

    const char* sql;
    if (store->config.enable_fts) {
        sql = "SELECT rowid FROM concepts_fts WHERE concepts_fts MATCH ? LIMIT ?";
    } else {
        sql = "SELECT concept_id FROM concepts WHERE label LIKE '%' || ? || '%' LIMIT ?";
    }

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        nimcp_memory_search_result_destroy(result);
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, label_query, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, (int)max_results);

    while (sqlite3_step(stmt) == SQLITE_ROW && result->count < max_results) {
        result->ids[result->count] = (uint64_t)sqlite3_column_int64(stmt, 0);
        result->distances[result->count] = 0.0f;
        result->count++;
    }

    sqlite3_finalize(stmt);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return result;
}

int nimcp_memory_store_concept_update_activation(
    nimcp_memory_store_t* store, uint64_t concept_id, float new_activation, uint32_t access_count)
{
    if (!store) return -1;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    const char* sql = "UPDATE concepts SET base_activation = ?, access_count = ? WHERE concept_id = ?";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return -1;
    }

    sqlite3_bind_double(stmt, 1, (double)new_activation);
    sqlite3_bind_int(stmt, 2, (int)access_count);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)concept_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* ============================================================================
 * Relation Operations
 * ============================================================================ */

int nimcp_memory_store_relation_put(nimcp_memory_store_t* store, const nimcp_relation_record_t* record) {
    if (!store || !record) return -1;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    sqlite3_stmt* s = store->stmt_relation_insert;
    sqlite3_reset(s);

    sqlite3_bind_int64(s, 1, (sqlite3_int64)record->relation_id);
    sqlite3_bind_int64(s, 2, (sqlite3_int64)record->source_concept_id);
    sqlite3_bind_int64(s, 3, (sqlite3_int64)record->target_concept_id);
    sqlite3_bind_int(s, 4, (int)record->relation_type);
    sqlite3_bind_double(s, 5, (double)record->strength);
    sqlite3_bind_int64(s, 6, (sqlite3_int64)record->timestamp_us);

    int rc = sqlite3_step(s);
    store->stats.total_writes++;

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

nimcp_graph_traversal_result_t* nimcp_memory_store_relation_traverse(
    nimcp_memory_store_t* store, uint64_t start_concept_id,
    uint32_t max_hops, float min_strength)
{
    if (!store || max_hops == 0) return NULL;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    /* Recursive CTE for multi-hop BFS */
    const char* sql =
        "WITH RECURSIVE hops(node_id, depth, strength) AS ("
        "  SELECT target_concept_id, 1, strength FROM relations "
        "  WHERE source_concept_id = ? AND strength >= ?"
        "  UNION ALL"
        "  SELECT r.target_concept_id, h.depth + 1, h.strength * r.strength "
        "  FROM relations r JOIN hops h ON r.source_concept_id = h.node_id "
        "  WHERE h.depth < ? AND r.strength >= ?"
        ") "
        "SELECT DISTINCT node_id, MIN(depth), MAX(strength) FROM hops "
        "GROUP BY node_id ORDER BY depth ASC LIMIT 1000";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        LOG_WARN("memory_store relation_traverse: prepare failed: %s", sqlite3_errmsg(store->db));
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return NULL;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)start_concept_id);
    sqlite3_bind_double(stmt, 2, (double)min_strength);
    sqlite3_bind_int(stmt, 3, (int)max_hops);
    sqlite3_bind_double(stmt, 4, (double)min_strength);

    /* Collect results into temporary arrays */
    uint32_t capacity = 256;
    uint32_t count = 0;
    uint64_t* node_ids = (uint64_t*)nimcp_calloc(capacity, sizeof(uint64_t));
    uint32_t* depths = (uint32_t*)nimcp_calloc(capacity, sizeof(uint32_t));
    float* weights = (float*)nimcp_calloc(capacity, sizeof(float));

    if (!node_ids || !depths || !weights) {
        nimcp_free(node_ids);
        nimcp_free(depths);
        nimcp_free(weights);
        sqlite3_finalize(stmt);
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return NULL;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            uint32_t new_cap = capacity * 2;
            uint64_t* new_ids = (uint64_t*)nimcp_calloc(new_cap, sizeof(uint64_t));
            uint32_t* new_depths = (uint32_t*)nimcp_calloc(new_cap, sizeof(uint32_t));
            float* new_weights = (float*)nimcp_calloc(new_cap, sizeof(float));
            if (!new_ids || !new_depths || !new_weights) {
                nimcp_free(new_ids);
                nimcp_free(new_depths);
                nimcp_free(new_weights);
                break;
            }
            memcpy(new_ids, node_ids, count * sizeof(uint64_t));
            memcpy(new_depths, depths, count * sizeof(uint32_t));
            memcpy(new_weights, weights, count * sizeof(float));
            nimcp_free(node_ids);
            nimcp_free(depths);
            nimcp_free(weights);
            node_ids = new_ids;
            depths = new_depths;
            weights = new_weights;
            capacity = new_cap;
        }

        node_ids[count] = (uint64_t)sqlite3_column_int64(stmt, 0);
        depths[count] = (uint32_t)sqlite3_column_int(stmt, 1);
        weights[count] = (float)sqlite3_column_double(stmt, 2);
        count++;
    }

    sqlite3_finalize(stmt);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    nimcp_graph_traversal_result_t* result = (nimcp_graph_traversal_result_t*)nimcp_calloc(1, sizeof(*result));
    if (!result) {
        nimcp_free(node_ids);
        nimcp_free(depths);
        nimcp_free(weights);
        return NULL;
    }

    result->node_ids = node_ids;
    result->depths = depths;
    result->weights = weights;
    result->count = count;

    return result;
}

nimcp_memory_search_result_t* nimcp_memory_store_relation_get_for_concept(
    nimcp_memory_store_t* store, uint64_t concept_id, uint32_t max_results)
{
    if (!store || max_results == 0) return NULL;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    nimcp_memory_search_result_t* result = search_result_create(max_results);
    if (!result) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return NULL;
    }

    const char* sql = "SELECT relation_id, strength FROM relations "
                      "WHERE source_concept_id = ? OR target_concept_id = ? "
                      "ORDER BY strength DESC LIMIT ?";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        nimcp_memory_search_result_destroy(result);
        return NULL;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)concept_id);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)concept_id);
    sqlite3_bind_int(stmt, 3, (int)max_results);

    while (sqlite3_step(stmt) == SQLITE_ROW && result->count < max_results) {
        result->ids[result->count] = (uint64_t)sqlite3_column_int64(stmt, 0);
        result->distances[result->count] = (float)sqlite3_column_double(stmt, 1);
        result->count++;
    }

    sqlite3_finalize(stmt);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return result;
}

/* ============================================================================
 * Autobiographical Operations
 * ============================================================================ */

int nimcp_memory_store_autobio_put(nimcp_memory_store_t* store, const nimcp_autobio_record_t* record) {
    if (!store || !record) return -1;

    /* V5 Security: Ensure null-terminated string fields before SQLite binding.
     * The record uses fixed-size char arrays (what_happened[512], etc.).
     * Cast away const — we are only ensuring the last byte is '\0'. */
    nimcp_autobio_record_t* rec_mut = (nimcp_autobio_record_t*)record;
    rec_mut->what_happened[sizeof(rec_mut->what_happened) - 1] = '\0';
    rec_mut->why_it_happened[sizeof(rec_mut->why_it_happened) - 1] = '\0';
    rec_mut->outcome[sizeof(rec_mut->outcome) - 1] = '\0';

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    sqlite3_stmt* s = store->stmt_autobio_insert;
    sqlite3_reset(s);

    sqlite3_bind_int64(s, 1, (sqlite3_int64)record->timestamp_us);
    sqlite3_bind_int(s, 2, (int)record->memory_type);
    sqlite3_bind_text(s, 3, record->what_happened, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, record->why_it_happened, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 5, record->outcome, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s, 6, (int)record->valence);
    sqlite3_bind_double(s, 7, (double)record->importance);
    sqlite3_bind_double(s, 8, (double)record->arousal);
    sqlite3_bind_double(s, 9, (double)record->emotional_intensity);
    sqlite3_bind_int(s, 10, record->identity_defining ? 1 : 0);
    sqlite3_bind_int(s, 11, record->is_core_memory ? 1 : 0);
    sqlite3_bind_int(s, 12, (int)record->source_device_id);

    int rc = sqlite3_step(s);
    store->stats.total_writes++;

    /* Insert into autobio FTS5 index */
    if (store->config.enable_fts && rc == SQLITE_DONE) {
        sqlite3_int64 rowid = sqlite3_last_insert_rowid(store->db);
        const char* fts_sql = "INSERT INTO autobio_fts(rowid, what_happened, why_it_happened, outcome) "
                              "VALUES (?, ?, ?, ?)";
        sqlite3_stmt* fts_stmt = NULL;
        if (sqlite3_prepare_v2(store->db, fts_sql, -1, &fts_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(fts_stmt, 1, rowid);
            sqlite3_bind_text(fts_stmt, 2, record->what_happened, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(fts_stmt, 3, record->why_it_happened, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(fts_stmt, 4, record->outcome, -1, SQLITE_TRANSIENT);
            sqlite3_step(fts_stmt);
            sqlite3_finalize(fts_stmt);
        }
    }

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

nimcp_memory_search_result_t* nimcp_memory_store_autobio_search_text(
    nimcp_memory_store_t* store, const char* query, uint32_t max_results)
{
    if (!store || !query || max_results == 0) return NULL;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    nimcp_memory_search_result_t* result = search_result_create(max_results);
    if (!result) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return NULL;
    }

    const char* sql;
    if (store->config.enable_fts) {
        sql = "SELECT rowid FROM autobio_fts WHERE autobio_fts MATCH ? LIMIT ?";
    } else {
        sql = "SELECT memory_id FROM autobio WHERE "
              "what_happened LIKE '%' || ? || '%' OR "
              "why_it_happened LIKE '%' || ? || '%' OR "
              "outcome LIKE '%' || ? || '%' LIMIT ?";
    }

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        nimcp_memory_search_result_destroy(result);
        return NULL;
    }

    if (store->config.enable_fts) {
        sqlite3_bind_text(stmt, 1, query, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, (int)max_results);
    } else {
        sqlite3_bind_text(stmt, 1, query, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, query, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, query, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, (int)max_results);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW && result->count < max_results) {
        result->ids[result->count] = (uint64_t)sqlite3_column_int64(stmt, 0);
        result->distances[result->count] = 0.0f;
        result->count++;
    }

    sqlite3_finalize(stmt);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return result;
}

nimcp_memory_search_result_t* nimcp_memory_store_autobio_search_time(
    nimcp_memory_store_t* store, uint64_t start_us, uint64_t end_us,
    uint32_t max_results)
{
    if (!store || max_results == 0) return NULL;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    nimcp_memory_search_result_t* result = search_result_create(max_results);
    if (!result) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return NULL;
    }

    const char* sql = "SELECT memory_id FROM autobio "
                      "WHERE timestamp_us BETWEEN ? AND ? "
                      "ORDER BY timestamp_us DESC LIMIT ?";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        nimcp_memory_search_result_destroy(result);
        return NULL;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)start_us);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)end_us);
    sqlite3_bind_int(stmt, 3, (int)max_results);

    while (sqlite3_step(stmt) == SQLITE_ROW && result->count < max_results) {
        result->ids[result->count] = (uint64_t)sqlite3_column_int64(stmt, 0);
        result->distances[result->count] = 0.0f;
        result->count++;
    }

    sqlite3_finalize(stmt);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return result;
}

/* ============================================================================
 * Bloom Filter
 * ============================================================================ */

bool nimcp_memory_store_bloom_check(nimcp_memory_store_t* store, const float* embedding, uint32_t dim) {
    if (!store || !store->bloom_filter || !embedding || dim == 0) return false;

    uint32_t byte_len = dim * sizeof(float);
    const uint8_t* data = (const uint8_t*)embedding;

    uint32_t h1 = bloom_hash_fnv1a(data, byte_len) % store->bloom_size;
    uint32_t h2 = bloom_hash_djb2(data, byte_len) % store->bloom_size;
    uint32_t h3 = bloom_hash_murmur3_mix(data, byte_len) % store->bloom_size;

    return (store->bloom_filter[h1 / 8] & (1u << (h1 % 8))) &&
           (store->bloom_filter[h2 / 8] & (1u << (h2 % 8))) &&
           (store->bloom_filter[h3 / 8] & (1u << (h3 % 8)));
}

void nimcp_memory_store_bloom_add(nimcp_memory_store_t* store, const float* embedding, uint32_t dim) {
    if (!store || !store->bloom_filter || !embedding || dim == 0) return;

    uint32_t byte_len = dim * sizeof(float);
    const uint8_t* data = (const uint8_t*)embedding;

    uint32_t h1 = bloom_hash_fnv1a(data, byte_len) % store->bloom_size;
    uint32_t h2 = bloom_hash_djb2(data, byte_len) % store->bloom_size;
    uint32_t h3 = bloom_hash_murmur3_mix(data, byte_len) % store->bloom_size;

    store->bloom_filter[h1 / 8] |= (uint8_t)(1u << (h1 % 8));
    store->bloom_filter[h2 / 8] |= (uint8_t)(1u << (h2 % 8));
    store->bloom_filter[h3 / 8] |= (uint8_t)(1u << (h3 % 8));
}

/* ============================================================================
 * Consolidation: promote high-importance engrams to semantic concepts
 * ============================================================================ */

int nimcp_memory_store_consolidate(nimcp_memory_store_t* store) {
    if (!store) return -1;

    nimcp_memory_store_flush(store);

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    const char* sql =
        "SELECT e.engram_id, e.embedding, e.embedding_dim, e.label, e.importance "
        "FROM engrams e "
        "WHERE e.state = 3 "       /* CONSOLIDATED */
        "  AND e.importance > 0.5 "
        "  AND NOT EXISTS (SELECT 1 FROM concepts c WHERE c.source_engram_id = e.engram_id) "
        "ORDER BY e.importance DESC "
        "LIMIT 100";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        LOG_WARN("memory_store consolidate: prepare failed: %s", sqlite3_errmsg(store->db));
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return -1;
    }

    int promoted = 0;

    exec_sql(store->db, "BEGIN TRANSACTION;");

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        uint64_t engram_id = (uint64_t)sqlite3_column_int64(stmt, 0);
        int blob_bytes = sqlite3_column_bytes(stmt, 1);
        uint32_t edim = (uint32_t)sqlite3_column_int(stmt, 2);
        const char* label = (const char*)sqlite3_column_text(stmt, 3);
        float importance = (float)sqlite3_column_double(stmt, 4);

        /* Insert as new concept using the engram's data */
        sqlite3_stmt* ins = store->stmt_concept_insert;
        sqlite3_reset(ins);

        /* Use engram_id as concept_id for traceability */
        sqlite3_bind_int64(ins, 1, (sqlite3_int64)engram_id);

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        uint64_t now_us = (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
        sqlite3_bind_int64(ins, 2, (sqlite3_int64)now_us);

        sqlite3_bind_text(ins, 3, label ? label : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(ins, 4, 0); /* category: uncategorized */

        if (blob_bytes > 0) {
            sqlite3_bind_blob(ins, 5, sqlite3_column_blob(stmt, 1), blob_bytes, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(ins, 5);
        }

        sqlite3_bind_int(ins, 6, (int)edim);
        sqlite3_bind_double(ins, 7, (double)importance);
        sqlite3_bind_int(ins, 8, 0); /* access_count */
        sqlite3_bind_int64(ins, 9, (sqlite3_int64)engram_id);
        sqlite3_bind_int(ins, 10, 0); /* source_device_id: local */

        if (sqlite3_step(ins) == SQLITE_DONE) {
            promoted++;
        }
    }

    exec_sql(store->db, "COMMIT;");

    sqlite3_finalize(stmt);

    LOG_INFO("memory_store consolidate: promoted %d engrams to concepts", promoted);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return promoted;
}

/* ============================================================================
 * Garbage Collection
 * ============================================================================ */

int nimcp_memory_store_gc(nimcp_memory_store_t* store,
    float min_importance, uint32_t max_age_days)
{
    if (!store) return -1;

    nimcp_memory_store_flush(store);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t now_us = (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
    uint64_t cutoff_us = now_us - (uint64_t)max_age_days * 86400ULL * 1000000ULL;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    const char* sql = "DELETE FROM engrams WHERE importance < ? AND recall_count = 0 AND timestamp_us < ?";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return -1;
    }

    sqlite3_bind_double(stmt, 1, (double)min_importance);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)cutoff_us);

    rc = sqlite3_step(stmt);
    int deleted = sqlite3_changes(store->db);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_WARN("memory_store gc failed: %s", sqlite3_errmsg(store->db));
    } else {
        LOG_INFO("memory_store gc: deleted %d low-importance unrecalled engrams older than %u days",
                 deleted, max_age_days);
    }

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return deleted;
}

/* ============================================================================
 * Rebuild Index (no-op for brute-force; placeholder for future ANN index)
 * ============================================================================ */

int nimcp_memory_store_rebuild_index(nimcp_memory_store_t* store) {
    if (!store) return -1;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    /* ANALYZE helps SQLite query planner pick optimal indices */
    exec_sql(store->db, "ANALYZE;");

    /* Rebuild FTS index if enabled */
    if (store->config.enable_fts) {
        exec_sql(store->db, "INSERT INTO engrams_fts(engrams_fts) VALUES('rebuild');");
        exec_sql(store->db, "INSERT INTO concepts_fts(concepts_fts) VALUES('rebuild');");
        exec_sql(store->db, "INSERT INTO autobio_fts(autobio_fts) VALUES('rebuild');");
    }

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    LOG_INFO("memory_store rebuild_index: ANALYZE + FTS rebuild complete");

    return 0;
}

/* ============================================================================
 * Result Cleanup
 * ============================================================================ */

void nimcp_memory_search_result_destroy(nimcp_memory_search_result_t* result) {
    if (!result) return;
    nimcp_free(result->ids);
    nimcp_free(result->distances);
    nimcp_free(result);
}

void nimcp_memory_graph_result_destroy(nimcp_graph_traversal_result_t* result) {
    if (!result) return;
    nimcp_free(result->node_ids);
    nimcp_free(result->depths);
    nimcp_free(result->weights);
    nimcp_free(result);
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

bool nimcp_memory_store_is_healthy(const nimcp_memory_store_t* store) {
    if (!store) return false;
    return store->healthy;
}

int nimcp_memory_store_get_stats(nimcp_memory_store_t* store, nimcp_memory_store_stats_t* stats) {
    if (!store || !stats) return -1;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    /* Copy accumulated stats */
    *stats = store->stats;

    /* Query live counts from DB */
    sqlite3_stmt* stmt = NULL;

    if (sqlite3_prepare_v2(store->db, "SELECT COUNT(*) FROM engrams", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats->total_engrams = (uint64_t)sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (sqlite3_prepare_v2(store->db, "SELECT COUNT(*) FROM concepts", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats->total_concepts = (uint64_t)sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (sqlite3_prepare_v2(store->db, "SELECT COUNT(*) FROM relations", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats->total_relations = (uint64_t)sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (sqlite3_prepare_v2(store->db, "SELECT COUNT(*) FROM autobio", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats->total_autobio = (uint64_t)sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    /* DB file size via PRAGMA */
    if (sqlite3_prepare_v2(store->db, "PRAGMA page_count", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            uint64_t pages = (uint64_t)sqlite3_column_int64(stmt, 0);
            stats->db_size_bytes = pages * 4096; /* page_size=4096 */
        }
        sqlite3_finalize(stmt);
    }

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return 0;
}

/* ============================================================================
 * Remote Sensory Stream Ingestion
 * ============================================================================ */

int nimcp_memory_store_ingest_remote_stream(
    nimcp_memory_store_t* store,
    const nimcp_sensory_stream_record_t* record)
{
    if (!store || !record) return -1;

    /* Create an engram from the remote sensory data */
    nimcp_engram_record_t engram;
    memset(&engram, 0, sizeof(engram));

    /* Use timestamp + device_id hash as engram_id to avoid collisions */
    engram.engram_id = record->timestamp_us ^ ((uint64_t)record->source_device_id << 48);
    engram.timestamp_us = record->timestamp_us;
    engram.memory_type = 0; /* EPISODIC */
    engram.state = 0;       /* ENCODING */
    engram.neuron_count = 0;
    engram.neuron_ids = NULL;
    engram.activations = NULL;
    engram.embedding = record->features;
    engram.embedding_dim = record->feature_dim;
    engram.valence = record->valence;
    engram.arousal = record->arousal;
    engram.intensity = (record->valence > 0 ? record->valence : -record->valence) * record->arousal;
    engram.consolidation_strength = 0.0f;
    engram.decay_rate = 0.01f;
    engram.vividness = 1.0f;
    engram.importance = record->importance;
    engram.recall_count = 0;
    engram.last_recall_us = 0;
    engram.source_device_id = record->source_device_id;
    strncpy(engram.label, record->label, sizeof(engram.label) - 1);
    engram.label[sizeof(engram.label) - 1] = '\0';

    int ret = nimcp_memory_store_engram_put(store, &engram);
    if (ret != 0) return ret;

    /* If labeled and important enough, create a semantic concept */
    if (record->label[0] != '\0' && record->importance > 0.3f) {
        nimcp_concept_record_t concept;
        memset(&concept, 0, sizeof(concept));
        concept.concept_id = engram.engram_id + 1;
        concept.timestamp_us = record->timestamp_us;
        strncpy(concept.label, record->label, sizeof(concept.label) - 1);
        concept.label[sizeof(concept.label) - 1] = '\0';
        concept.category = 0;
        concept.embedding = record->features;
        concept.embedding_dim = record->feature_dim;
        concept.base_activation = record->importance;
        concept.access_count = 1;
        concept.source_engram_id = engram.engram_id;
        concept.source_device_id = record->source_device_id;
        nimcp_memory_store_concept_put(store, &concept);
    }

    /* If highly important, also create an autobiographical entry */
    if (record->importance > 0.7f) {
        nimcp_autobio_record_t autobio;
        memset(&autobio, 0, sizeof(autobio));
        autobio.timestamp_us = record->timestamp_us;
        autobio.memory_type = 0;
        snprintf(autobio.what_happened, sizeof(autobio.what_happened),
                 "Device %u experienced: %s", record->source_device_id, record->label);
        snprintf(autobio.why_it_happened, sizeof(autobio.why_it_happened),
                 "Sensory input with loss=%.4f", (double)record->loss);
        snprintf(autobio.outcome, sizeof(autobio.outcome),
                 "Importance=%.2f, valence=%.2f", (double)record->importance, (double)record->valence);
        autobio.valence = (int32_t)(record->valence > 0 ? 1 : (record->valence < 0 ? -1 : 0));
        autobio.importance = record->importance;
        autobio.arousal = record->arousal;
        autobio.emotional_intensity = fabsf(record->valence) * record->arousal;
        autobio.identity_defining = false;
        autobio.is_core_memory = (record->importance > 0.9f);
        autobio.source_device_id = record->source_device_id;
        nimcp_memory_store_autobio_put(store, &autobio);
    }

    return 0;
}

nimcp_memory_search_result_t* nimcp_memory_store_query_by_device(
    nimcp_memory_store_t* store, uint32_t device_id, uint32_t max_results)
{
    if (!store || max_results == 0) return NULL;

    nimcp_memory_store_flush(store);

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    nimcp_memory_search_result_t* result = search_result_create(max_results);
    if (!result) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return NULL;
    }

    const char* sql = "SELECT engram_id FROM engrams "
                      "WHERE source_device_id = ? "
                      "ORDER BY timestamp_us DESC LIMIT ?";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        nimcp_memory_search_result_destroy(result);
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, (int)device_id);
    sqlite3_bind_int(stmt, 2, (int)max_results);

    while (sqlite3_step(stmt) == SQLITE_ROW && result->count < max_results) {
        result->ids[result->count] = (uint64_t)sqlite3_column_int64(stmt, 0);
        result->distances[result->count] = 0.0f;
        result->count++;
    }

    sqlite3_finalize(stmt);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return result;
}

nimcp_memory_search_result_t* nimcp_memory_store_cross_device_correlations(
    nimcp_memory_store_t* store, uint64_t time_window_us, uint32_t max_results)
{
    if (!store || max_results == 0) return NULL;

    nimcp_memory_store_flush(store);

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    nimcp_memory_search_result_t* result = search_result_create(max_results);
    if (!result) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return NULL;
    }

    /* Find pairs of engrams from different devices with timestamps within the window */
    const char* sql =
        "SELECT a.engram_id, b.engram_id "
        "FROM engrams a JOIN engrams b "
        "ON a.source_device_id <> b.source_device_id "
        "  AND a.source_device_id > 0 AND b.source_device_id > 0 "
        "  AND ABS(a.timestamp_us - b.timestamp_us) <= ? "
        "  AND a.engram_id < b.engram_id "
        "ORDER BY ABS(a.timestamp_us - b.timestamp_us) ASC "
        "LIMIT ?";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        nimcp_memory_search_result_destroy(result);
        return NULL;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)time_window_us);
    sqlite3_bind_int(stmt, 2, (int)max_results);

    while (sqlite3_step(stmt) == SQLITE_ROW && result->count < max_results) {
        result->ids[result->count] = (uint64_t)sqlite3_column_int64(stmt, 0);
        result->distances[result->count] = 0.0f;
        result->count++;
    }

    sqlite3_finalize(stmt);

    if (store->mutex) nimcp_mutex_unlock(store->mutex);

    return result;
}

/* ============================================================================
 * Remote Sensory Stream Ingestion
 * ============================================================================ */


/* ============================================================================
 * Metadata Catalog Implementation
 * ============================================================================ */

int nimcp_memory_store_metadata_put(
    nimcp_memory_store_t* store,
    const nimcp_metadata_record_t* record)
{
    if (!store || !store->db || !record) return -1;

    /* V5 Security: Ensure null-terminated label/tags before SQLite binding */
    nimcp_metadata_record_t* rec_mut = (nimcp_metadata_record_t*)record;
    rec_mut->label[sizeof(rec_mut->label) - 1] = '\0';
    rec_mut->tags[sizeof(rec_mut->tags) - 1] = '\0';

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    sqlite3_stmt* stmt = NULL;
    const char* sql =
        "INSERT OR REPLACE INTO metadata "
        "(entry_id, entry_type, timestamp_us, importance, source_device_id, "
        " training_step, curriculum_stage, model_version, label, tags) "
        "VALUES (?,?,?,?,?,?,?,?,?,?)";

    int rc = -1;
    if (sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, (sqlite3_int64)record->entry_id);
        sqlite3_bind_int(stmt, 2, (int)record->type);
        sqlite3_bind_int64(stmt, 3, (sqlite3_int64)record->timestamp_us);
        sqlite3_bind_double(stmt, 4, (double)record->importance);
        sqlite3_bind_int(stmt, 5, (int)record->source_device_id);
        sqlite3_bind_int(stmt, 6, (int)record->training_step);
        sqlite3_bind_int(stmt, 7, (int)record->curriculum_stage);
        sqlite3_bind_int(stmt, 8, (int)record->model_version);
        sqlite3_bind_text(stmt, 9, record->label, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 10, record->tags, -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            rc = 0;

            /* Also insert into metadata FTS5 */
            sqlite3_stmt* fts = NULL;
            if (sqlite3_prepare_v2(store->db,
                    "INSERT INTO metadata_fts(rowid, label, tags) "
                    "VALUES (last_insert_rowid(), ?, ?)",
                    -1, &fts, NULL) == SQLITE_OK) {
                sqlite3_bind_text(fts, 1, record->label, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(fts, 2, record->tags, -1, SQLITE_TRANSIENT);
                sqlite3_step(fts);
                sqlite3_finalize(fts);
            }
        }
        sqlite3_finalize(stmt);
    }

    store->stats.total_writes++;
    if (store->mutex) nimcp_mutex_unlock(store->mutex);
    return rc;
}

nimcp_metadata_search_result_t* nimcp_memory_store_metadata_search_tags(
    nimcp_memory_store_t* store,
    const char* tags,
    uint32_t max_results)
{
    if (!store || !store->db || !tags) return NULL;

    nimcp_metadata_search_result_t* result = nimcp_calloc(1, sizeof(*result));
    if (!result) return NULL;

    result->records = nimcp_calloc(max_results, sizeof(nimcp_metadata_record_t));
    if (!result->records) {
        nimcp_free(result);
        return NULL;
    }
    result->capacity = max_results;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    /* Use FTS5 to search tags */
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(store->db,
            "SELECT m.entry_id, m.entry_type, m.timestamp_us, m.importance, "
            "  m.source_device_id, m.training_step, m.curriculum_stage, "
            "  m.model_version, m.label, m.tags "
            "FROM metadata m "
            "JOIN metadata_fts f ON m.rowid = f.rowid "
            "WHERE metadata_fts MATCH ? "
            "ORDER BY m.importance DESC LIMIT ?",
            -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, tags, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, (int)max_results);

        while (sqlite3_step(stmt) == SQLITE_ROW && result->count < max_results) {
            nimcp_metadata_record_t* r = &result->records[result->count];
            r->entry_id = (uint64_t)sqlite3_column_int64(stmt, 0);
            r->type = (nimcp_memory_entry_type_t)sqlite3_column_int(stmt, 1);
            r->timestamp_us = (uint64_t)sqlite3_column_int64(stmt, 2);
            r->importance = (float)sqlite3_column_double(stmt, 3);
            r->source_device_id = (uint32_t)sqlite3_column_int(stmt, 4);
            r->training_step = (uint32_t)sqlite3_column_int(stmt, 5);
            r->curriculum_stage = (uint32_t)sqlite3_column_int(stmt, 6);
            r->model_version = (uint32_t)sqlite3_column_int(stmt, 7);
            const char* lbl = (const char*)sqlite3_column_text(stmt, 8);
            if (lbl) strncpy(r->label, lbl, sizeof(r->label) - 1);
            const char* tgs = (const char*)sqlite3_column_text(stmt, 9);
            if (tgs) strncpy(r->tags, tgs, sizeof(r->tags) - 1);
            result->count++;
        }
        sqlite3_finalize(stmt);
    }

    store->stats.total_reads++;
    if (store->mutex) nimcp_mutex_unlock(store->mutex);
    return result;
}

nimcp_metadata_search_result_t* nimcp_memory_store_metadata_search_text(
    nimcp_memory_store_t* store,
    const char* query,
    uint32_t max_results)
{
    /* Text search on metadata labels — same as tag search but on label field */
    return nimcp_memory_store_metadata_search_tags(store, query, max_results);
}

nimcp_metadata_search_result_t* nimcp_memory_store_metadata_search_provenance(
    nimcp_memory_store_t* store,
    uint32_t min_step, uint32_t max_step,
    int32_t curriculum_stage,
    int32_t device_id,
    uint32_t max_results)
{
    if (!store || !store->db) return NULL;

    nimcp_metadata_search_result_t* result = nimcp_calloc(1, sizeof(*result));
    if (!result) return NULL;

    result->records = nimcp_calloc(max_results, sizeof(nimcp_metadata_record_t));
    if (!result->records) {
        nimcp_free(result);
        return NULL;
    }
    result->capacity = max_results;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    /* Build dynamic SQL based on filter params */
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT entry_id, entry_type, timestamp_us, importance, "
        "  source_device_id, training_step, curriculum_stage, "
        "  model_version, label, tags "
        "FROM metadata "
        "WHERE training_step BETWEEN %u AND %u "
        "%s%s"
        "ORDER BY training_step DESC LIMIT %u",
        min_step, max_step,
        (curriculum_stage >= 0) ? "AND curriculum_stage = " : "",
        (curriculum_stage >= 0) ? "" : "",
        max_results);

    /* Use parameterized for the optional filters */
    sqlite3_stmt* stmt = NULL;
    const char* param_sql =
        "SELECT entry_id, entry_type, timestamp_us, importance, "
        "  source_device_id, training_step, curriculum_stage, "
        "  model_version, label, tags "
        "FROM metadata "
        "WHERE training_step BETWEEN ? AND ? "
        "  AND (? < 0 OR curriculum_stage = ?) "
        "  AND (? < 0 OR source_device_id = ?) "
        "ORDER BY training_step DESC LIMIT ?";

    if (sqlite3_prepare_v2(store->db, param_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, (int)min_step);
        sqlite3_bind_int(stmt, 2, (int)max_step);
        sqlite3_bind_int(stmt, 3, curriculum_stage);
        sqlite3_bind_int(stmt, 4, curriculum_stage);
        sqlite3_bind_int(stmt, 5, device_id);
        sqlite3_bind_int(stmt, 6, device_id);
        sqlite3_bind_int(stmt, 7, (int)max_results);

        while (sqlite3_step(stmt) == SQLITE_ROW && result->count < max_results) {
            nimcp_metadata_record_t* r = &result->records[result->count];
            r->entry_id = (uint64_t)sqlite3_column_int64(stmt, 0);
            r->type = (nimcp_memory_entry_type_t)sqlite3_column_int(stmt, 1);
            r->timestamp_us = (uint64_t)sqlite3_column_int64(stmt, 2);
            r->importance = (float)sqlite3_column_double(stmt, 3);
            r->source_device_id = (uint32_t)sqlite3_column_int(stmt, 4);
            r->training_step = (uint32_t)sqlite3_column_int(stmt, 5);
            r->curriculum_stage = (uint32_t)sqlite3_column_int(stmt, 6);
            r->model_version = (uint32_t)sqlite3_column_int(stmt, 7);
            const char* lbl = (const char*)sqlite3_column_text(stmt, 8);
            if (lbl) strncpy(r->label, lbl, sizeof(r->label) - 1);
            const char* tgs = (const char*)sqlite3_column_text(stmt, 9);
            if (tgs) strncpy(r->tags, tgs, sizeof(r->tags) - 1);
            result->count++;
        }
        sqlite3_finalize(stmt);
    }

    (void)sql; /* Unused — kept param_sql approach */
    store->stats.total_reads++;
    if (store->mutex) nimcp_mutex_unlock(store->mutex);
    return result;
}

int nimcp_memory_store_metadata_get(
    nimcp_memory_store_t* store,
    uint64_t entry_id,
    nimcp_memory_entry_type_t type,
    nimcp_metadata_record_t* record)
{
    if (!store || !store->db || !record) return -1;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    sqlite3_stmt* stmt = NULL;
    int rc = -1;
    if (sqlite3_prepare_v2(store->db,
            "SELECT entry_id, entry_type, timestamp_us, importance, "
            "  source_device_id, training_step, curriculum_stage, "
            "  model_version, label, tags "
            "FROM metadata WHERE entry_id = ? AND entry_type = ?",
            -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, (sqlite3_int64)entry_id);
        sqlite3_bind_int(stmt, 2, (int)type);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            record->entry_id = (uint64_t)sqlite3_column_int64(stmt, 0);
            record->type = (nimcp_memory_entry_type_t)sqlite3_column_int(stmt, 1);
            record->timestamp_us = (uint64_t)sqlite3_column_int64(stmt, 2);
            record->importance = (float)sqlite3_column_double(stmt, 3);
            record->source_device_id = (uint32_t)sqlite3_column_int(stmt, 4);
            record->training_step = (uint32_t)sqlite3_column_int(stmt, 5);
            record->curriculum_stage = (uint32_t)sqlite3_column_int(stmt, 6);
            record->model_version = (uint32_t)sqlite3_column_int(stmt, 7);
            const char* lbl = (const char*)sqlite3_column_text(stmt, 8);
            if (lbl) strncpy(record->label, lbl, sizeof(record->label) - 1);
            const char* tgs = (const char*)sqlite3_column_text(stmt, 9);
            if (tgs) strncpy(record->tags, tgs, sizeof(record->tags) - 1);
            rc = 0;
        }
        sqlite3_finalize(stmt);
    }

    store->stats.total_reads++;
    if (store->mutex) nimcp_mutex_unlock(store->mutex);
    return rc;
}

int nimcp_memory_store_metadata_add_tags(
    nimcp_memory_store_t* store,
    uint64_t entry_id,
    nimcp_memory_entry_type_t type,
    const char* new_tags)
{
    if (!store || !store->db || !new_tags) return -1;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    /* Append new tags to existing tags (comma-separated) */
    sqlite3_stmt* stmt = NULL;
    int rc = -1;
    if (sqlite3_prepare_v2(store->db,
            "UPDATE metadata SET tags = CASE "
            "  WHEN tags IS NULL OR tags = '' THEN ? "
            "  ELSE tags || ',' || ? "
            "END WHERE entry_id = ? AND entry_type = ?",
            -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, new_tags, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, new_tags, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, (sqlite3_int64)entry_id);
        sqlite3_bind_int(stmt, 4, (int)type);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            rc = 0;
        }
        sqlite3_finalize(stmt);
    }

    if (store->mutex) nimcp_mutex_unlock(store->mutex);
    return rc;
}

void nimcp_metadata_search_result_destroy(nimcp_metadata_search_result_t* result)
{
    if (!result) return;
    nimcp_free(result->records);
    nimcp_free(result);
}

/* ============================================================================
 * Security Audit Trail Implementation
 * ============================================================================ */

int nimcp_memory_store_audit_log(nimcp_memory_store_t* store, const nimcp_memory_audit_event_t* event)
{
    if (!store || !event || !store->db) return -1;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    sqlite3_stmt* stmt = NULL;
    int rc = -1;

    if (sqlite3_prepare_v2(store->db,
            "INSERT INTO audit_log (timestamp_us, event_type, source_module_id, description, details) "
            "VALUES (?, ?, ?, ?, ?)",
            -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, (sqlite3_int64)event->timestamp_us);
        sqlite3_bind_int(stmt, 2, (int)event->event_type);
        sqlite3_bind_int(stmt, 3, (int)event->source_module_id);
        sqlite3_bind_text(stmt, 4, event->description, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, event->details, -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            rc = 0;
        }
        sqlite3_finalize(stmt);
    }

    if (store->mutex) nimcp_mutex_unlock(store->mutex);
    return rc;
}

nimcp_memory_search_result_t* nimcp_memory_store_audit_search(
    nimcp_memory_store_t* store, uint32_t min_severity,
    uint64_t start_us, uint64_t end_us, uint32_t max_results)
{
    if (!store || !store->db) return NULL;
    if (max_results == 0) max_results = 100;

    if (store->mutex) nimcp_mutex_lock(store->mutex);

    nimcp_memory_search_result_t* result = nimcp_calloc(1, sizeof(*result));
    if (!result) {
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return NULL;
    }

    result->ids = nimcp_calloc(max_results, sizeof(uint64_t));
    result->distances = nimcp_calloc(max_results, sizeof(float));
    if (!result->ids || !result->distances) {
        nimcp_free(result->ids);
        nimcp_free(result->distances);
        nimcp_free(result);
        if (store->mutex) nimcp_mutex_unlock(store->mutex);
        return NULL;
    }
    result->capacity = max_results;
    result->count = 0;

    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(store->db,
            "SELECT rowid, event_type FROM audit_log "
            "WHERE event_type >= ? AND timestamp_us >= ? AND timestamp_us <= ? "
            "ORDER BY timestamp_us DESC LIMIT ?",
            -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, (int)min_severity);
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)start_us);
        sqlite3_bind_int64(stmt, 3, (sqlite3_int64)end_us);
        sqlite3_bind_int(stmt, 4, (int)max_results);

        while (sqlite3_step(stmt) == SQLITE_ROW && result->count < max_results) {
            result->ids[result->count] = (uint64_t)sqlite3_column_int64(stmt, 0);
            result->distances[result->count] = (float)sqlite3_column_int(stmt, 1);
            result->count++;
        }
        sqlite3_finalize(stmt);
    }

    if (store->mutex) nimcp_mutex_unlock(store->mutex);
    return result;
}
