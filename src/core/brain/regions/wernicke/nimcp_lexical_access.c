/**
 * @file nimcp_lexical_access.c
 * @brief Implementation of lexical access for Wernicke's area
 *
 * WHAT: Word recognition using Cohort model with frequency effects
 * WHY:  Map phoneme sequences to lexical entries
 * HOW:  Incremental cohort activation and reduction
 *
 * @version Phase W2: Wernicke's Area Lexical Access
 * @date 2026-01-04
 */

#include "core/brain/regions/wernicke/nimcp_lexical_access.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lexical_access, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Hash bucket entry for lexicon
 */
typedef struct lexicon_node {
    lexical_entry_t entry;
    struct lexicon_node* next;
} lexicon_node_t;

/**
 * @brief Phoneme trie node for fast prefix lookup
 */
typedef struct trie_node {
    struct trie_node* children[PHONEME_COUNT];
    uint32_t* word_ids;                  /**< Words ending here */
    uint32_t num_words;
    uint32_t words_capacity;
} trie_node_t;

/**
 * @brief Lexical access internal state
 */
struct lexical_access {
    /* Configuration */
    lexical_config_t config;

    /* Lexicon storage */
    lexicon_node_t** hash_table;         /**< Word lookup by ID/string */
    uint32_t hash_buckets;
    uint32_t num_words;
    uint32_t next_word_id;

    /* Phoneme trie for cohort generation */
    trie_node_t* trie_root;

    /* Word array for direct ID access */
    lexical_entry_t** entries;           /**< Direct access by word_id */
    uint32_t entries_capacity;

    /* Current recognition state */
    cohort_state_t cohort;
    phoneme_t* current_phonemes;         /**< Phonemes being processed */
    uint32_t current_phoneme_count;
    uint32_t current_phoneme_capacity;

    /* Priming state */
    priming_context_t priming;

    /* Statistics */
    lexical_stats_t stats;

    /* Batch H followup: writer/reader guard for hash_table + entries[]
     * + num_words/next_word_id. Same race shape as the Broca fix:
     * comprehend paths walking the chain race against trainer-side
     * lexical_add_entry / lexical_add_word from GL→Wernicke mirroring
     * + the lexicon-file loader. NULL = legacy unlocked behavior
     * (allocator failed during init); helpers degrade to no-op locking. */
    nimcp_mutex_t* lexicon_mutex;
};

/*=============================================================================
 * HASH FUNCTIONS
 *===========================================================================*/

static uint32_t hash_string(const char* str, uint32_t buckets)
{
    uint32_t hash = 5381;
    int c;
    while ((c = *str++) != 0) {
        hash = ((hash << 5) + hash) + (uint32_t)c;
    }
    return hash % buckets;
}

static uint32_t hash_word_id(uint32_t id, uint32_t buckets)
{
    return id % buckets;
}

/*=============================================================================
 * TRIE FUNCTIONS
 *===========================================================================*/

static trie_node_t* trie_create_node(void)
{
    trie_node_t* node = (trie_node_t*)nimcp_calloc(1, sizeof(trie_node_t));
    return node;
}

static void trie_destroy_node(trie_node_t* node)
{
    if (!node) return;

    for (int i = 0; i < PHONEME_COUNT; i++) {
        if (node->children[i]) {
            trie_destroy_node(node->children[i]);
        }
    }

    if (node->word_ids) nimcp_free(node->word_ids);
    nimcp_free(node);
}

static bool trie_insert(trie_node_t* root, const uint8_t* phonemes,
                        uint32_t num_phonemes, uint32_t word_id)
{
    trie_node_t* current = root;

    for (uint32_t i = 0; i < num_phonemes; i++) {
        uint8_t p = phonemes[i];
        if (p >= PHONEME_COUNT) continue;

        if (!current->children[p]) {
            current->children[p] = trie_create_node();
            if (!current->children[p]) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "trie_insert: failed to allocate trie child node");
                return false;
            }
        }
        current = current->children[p];
    }

    /* Add word_id to terminal node */
    if (current->num_words >= current->words_capacity) {
        uint32_t new_cap = current->words_capacity == 0 ? 4 : current->words_capacity * 2;
        uint32_t* new_ids = (uint32_t*)nimcp_realloc(current->word_ids, new_cap * sizeof(uint32_t));
        if (!new_ids) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "trie_insert: new_ids is NULL");
            return false;
        }
        current->word_ids = new_ids;
        current->words_capacity = new_cap;
    }

    current->word_ids[current->num_words++] = word_id;
    return true;
}

static void trie_collect_words(trie_node_t* node, uint32_t* word_ids,
                               uint32_t* count, uint32_t max_words)
{
    if (!node || *count >= max_words) return;

    /* Add words at this node */
    for (uint32_t i = 0; i < node->num_words && *count < max_words; i++) {
        word_ids[(*count)++] = node->word_ids[i];
    }

    /* Recurse into children */
    for (int i = 0; i < PHONEME_COUNT && *count < max_words; i++) {
        if (node->children[i]) {
            trie_collect_words(node->children[i], word_ids, count, max_words);
        }
    }
}

static trie_node_t* trie_traverse(trie_node_t* root, const uint8_t* phonemes,
                                   uint32_t num_phonemes)
{
    trie_node_t* current = root;

    for (uint32_t i = 0; i < num_phonemes; i++) {
        uint8_t p = phonemes[i];
        if (p >= PHONEME_COUNT) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "trie_traverse: capacity exceeded");
            return NULL;
        }

        if (!current->children[p]) {
            return NULL;  /* Phoneme path not found — normal lookup miss */
        }
        current = current->children[p];
    }

    return current;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

lexical_config_t lexical_default_config(void)
{
    lexical_config_t config = {
        .lexicon_size = LEX_DEFAULT_LEXICON_SIZE,
        .hash_buckets = LEX_DEFAULT_HASH_BUCKETS,

        .max_word_length = LEX_DEFAULT_MAX_WORD_LENGTH,
        .max_phoneme_length = LEX_DEFAULT_MAX_PHONEME_LENGTH,

        .max_cohort_size = LEX_DEFAULT_MAX_COHORT_SIZE,
        .cohort_activation_threshold = 0.1f,
        .uniqueness_threshold = 0.8f,

        .frequency_weight = LEX_DEFAULT_FREQUENCY_WEIGHT,
        .phoneme_match_weight = LEX_DEFAULT_PHONEME_MATCH_WEIGHT,
        .context_weight = 0.2f,

        .embedding_dim = LEX_DEFAULT_EMBEDDING_DIM,
        .enable_embeddings = false,

        .enable_priming = true,
        .priming_decay = NIMCP_EMA_DECAY_FAST,
        .priming_boost = 0.3f
    };
    return config;
}

lexical_access_t* lexical_create(const lexical_config_t* config)
{
    lexical_config_t cfg = config ? *config : lexical_default_config();

    lexical_access_t* lex = (lexical_access_t*)nimcp_calloc(1, sizeof(lexical_access_t));
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lexical_create: failed to allocate lexical_access");
        return NULL;
    }

    lex->config = cfg;
    lex->hash_buckets = cfg.hash_buckets;
    lex->next_word_id = 1;

    /* Allocate hash table */
    lex->hash_table = (lexicon_node_t**)nimcp_calloc(cfg.hash_buckets, sizeof(lexicon_node_t*));
    if (!lex->hash_table) {
        nimcp_free(lex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lexical_create: lex->hash_table is NULL");
        return NULL;
    }

    /* Allocate entries array */
    lex->entries_capacity = cfg.lexicon_size;
    lex->entries = (lexical_entry_t**)nimcp_calloc(cfg.lexicon_size, sizeof(lexical_entry_t*));
    if (!lex->entries) {
        nimcp_free(lex->hash_table);
        nimcp_free(lex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lexical_create: lex->entries is NULL");
        return NULL;
    }

    /* Create phoneme trie */
    lex->trie_root = trie_create_node();
    if (!lex->trie_root) {
        nimcp_free(lex->entries);
        nimcp_free(lex->hash_table);
        nimcp_free(lex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lexical_create: lex->trie_root is NULL");
        return NULL;
    }

    /* Allocate cohort */
    lex->cohort.max_members = cfg.max_cohort_size;
    lex->cohort.members = (cohort_member_t*)nimcp_calloc(cfg.max_cohort_size, sizeof(cohort_member_t));
    if (!lex->cohort.members) {
        trie_destroy_node(lex->trie_root);
        nimcp_free(lex->entries);
        nimcp_free(lex->hash_table);
        nimcp_free(lex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lexical_create: failed to allocate cohort members");
        return NULL;
    }

    /* Allocate current phoneme buffer */
    lex->current_phoneme_capacity = cfg.max_phoneme_length * 2;
    lex->current_phonemes = (phoneme_t*)nimcp_calloc(lex->current_phoneme_capacity, sizeof(phoneme_t));
    if (!lex->current_phonemes) {
        nimcp_free(lex->cohort.members);
        trie_destroy_node(lex->trie_root);
        nimcp_free(lex->entries);
        nimcp_free(lex->hash_table);
        nimcp_free(lex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lexical_create: failed to allocate phoneme buffer");
        return NULL;
    }

    /* Allocate priming context */
    if (cfg.enable_priming) {
        lex->priming.max_primed = 64;
        lex->priming.primed_concepts = (uint32_t*)nimcp_calloc(64, sizeof(uint32_t));
        lex->priming.priming_levels = (float*)nimcp_calloc(64, sizeof(float));
    }

    /* Batch H followup — guard hash_table + entries[] + counters. NULL
     * on creation failure is non-fatal; the helpers degrade to unlocked. */
    lex->lexicon_mutex = nimcp_mutex_create(NULL);
    if (!lex->lexicon_mutex) {
        NIMCP_LOG_WARN("lexical", "lexicon_mutex create failed — running unlocked");
    }

    NIMCP_LOG_INFO("lexical", "Created lexical access (buckets=%u, max_cohort=%u)",
                   cfg.hash_buckets, cfg.max_cohort_size);

    return lex;
}

void lexical_destroy(lexical_access_t* lex)
{
    if (!lex) return;

    /* Free hash table entries */
    if (lex->hash_table) {
        for (uint32_t i = 0; i < lex->hash_buckets; i++) {
            lexicon_node_t* node = lex->hash_table[i];
            while (node) {
                lexicon_node_t* next = node->next;
                /* Free internal allocations */
                if (node->entry.sense_ids) nimcp_free(node->entry.sense_ids);
                if (node->entry.neighbors) nimcp_free(node->entry.neighbors);
                if (node->entry.embedding) nimcp_free(node->entry.embedding);
                nimcp_free(node);
                node = next;
            }
        }
        nimcp_free(lex->hash_table);
    }

    /* Free entries array (pointers only, actual entries in hash table) */
    if (lex->entries) nimcp_free(lex->entries);

    /* Free trie */
    if (lex->trie_root) trie_destroy_node(lex->trie_root);

    /* Free cohort */
    if (lex->cohort.members) nimcp_free(lex->cohort.members);

    /* Free phoneme buffer */
    if (lex->current_phonemes) nimcp_free(lex->current_phonemes);

    /* Free priming */
    if (lex->priming.primed_concepts) nimcp_free(lex->priming.primed_concepts);
    if (lex->priming.priming_levels) nimcp_free(lex->priming.priming_levels);

    if (lex->lexicon_mutex) {
        nimcp_mutex_free(lex->lexicon_mutex);
        lex->lexicon_mutex = NULL;
    }

    nimcp_free(lex);
}

bool lexical_reset(lexical_access_t* lex)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }

    /* Reset cohort */
    lex->cohort.num_members = 0;
    lex->cohort.phonemes_processed = 0;
    lex->cohort.uniqueness_reached = false;
    lex->cohort.winner_id = 0;
    lex->cohort.winner_confidence = 0.0f;

    /* Reset current phonemes */
    lex->current_phoneme_count = 0;

    return true;
}

/*=============================================================================
 * LEXICON MANAGEMENT
 *===========================================================================*/

bool lexical_add_entry(lexical_access_t* lex, const lexical_entry_t* entry)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entry is NULL");
        return false;
    }

    /* Batch H followup — pre-alloc the node + deep-copy buffers OUTSIDE
     * the lock so the critical section stays tight. The deep-copy heap
     * allocs don't touch lexical_access state. */
    lexicon_node_t* node = (lexicon_node_t*)nimcp_calloc(1, sizeof(lexicon_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lexical_add_entry: node is NULL");
        return false;
    }
    node->entry = *entry;
    if (entry->sense_ids && entry->num_senses > 0) {
        node->entry.sense_ids = (uint32_t*)nimcp_malloc(entry->num_senses * sizeof(uint32_t));
        if (node->entry.sense_ids) {
            memcpy(node->entry.sense_ids, entry->sense_ids,
                   entry->num_senses * sizeof(uint32_t));
        }
    }
    if (entry->neighbors && entry->num_neighbors > 0) {
        node->entry.neighbors = (uint32_t*)nimcp_malloc(entry->num_neighbors * sizeof(uint32_t));
        if (node->entry.neighbors) {
            memcpy(node->entry.neighbors, entry->neighbors,
                   entry->num_neighbors * sizeof(uint32_t));
        }
    }
    if (entry->embedding && lex->config.enable_embeddings) {
        node->entry.embedding = (float*)nimcp_malloc(lex->config.embedding_dim * sizeof(float));
        if (node->entry.embedding) {
            memcpy(node->entry.embedding, entry->embedding,
                   lex->config.embedding_dim * sizeof(float));
        }
    }

    nimcp_mutex_t* mtx = lex->lexicon_mutex;
    if (mtx) nimcp_mutex_lock(mtx);

    /* Capacity check under lock — a concurrent insert could push us
     * over while we were allocating above. */
    if (lex->num_words >= lex->config.lexicon_size) {
        if (mtx) nimcp_mutex_unlock(mtx);
        NIMCP_LOG_WARN("lexical", "Lexicon full (%u words)", lex->num_words);
        /* Free the staged node + deep-copies we won't be linking. */
        if (node->entry.sense_ids) nimcp_free(node->entry.sense_ids);
        if (node->entry.neighbors) nimcp_free(node->entry.neighbors);
        if (node->entry.embedding) nimcp_free(node->entry.embedding);
        nimcp_free(node);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lexical_add_entry: capacity exceeded");
        return false;
    }

    /* Assign word_id if not set — under lock so next_word_id increments
     * monotonically even with concurrent inserts. */
    if (node->entry.word_id == 0) {
        node->entry.word_id = lex->next_word_id++;
    }

    /* Insert into hash table by word string */
    uint32_t hash = hash_string(entry->orthography, lex->hash_buckets);
    node->next = lex->hash_table[hash];
    lex->hash_table[hash] = node;

    /* Store in entries array for ID lookup */
    uint32_t id = node->entry.word_id;
    if (id < lex->entries_capacity) {
        lex->entries[id] = &node->entry;
    }

    /* Insert into phoneme trie. trie_insert mutates the trie under the
     * same lock — readers walking the trie need this too. (trie_insert
     * is not exposed publicly so an in-flight reader of it must come
     * through this TU, where every read site is similarly mutex-guarded.) */
    trie_insert(lex->trie_root, entry->phonemes, entry->phoneme_count, node->entry.word_id);

    lex->num_words++;

    if (mtx) nimcp_mutex_unlock(mtx);

    return true;
}

uint32_t lexical_add_word(
    lexical_access_t* lex,
    const char* word,
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    float frequency,
    uint32_t concept_id)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return 0;
    }
    if (!word) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "word is NULL");
        return 0;
    }
    if (!phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonemes is NULL");
        return 0;
    }
    if (num_phonemes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "num_phonemes is 0");
        return 0;
    }

    lexical_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    entry.word_id = 0;  /* Will be assigned */
    strncpy(entry.orthography, word, LEX_DEFAULT_MAX_WORD_LENGTH - 1);

    if (num_phonemes > LEX_DEFAULT_MAX_PHONEME_LENGTH) {
        num_phonemes = LEX_DEFAULT_MAX_PHONEME_LENGTH;
    }
    for (uint32_t i = 0; i < num_phonemes; i++) {
        entry.phonemes[i] = (uint8_t)phonemes[i];
    }
    entry.phoneme_count = num_phonemes;

    entry.frequency = frequency;
    entry.concept_id = concept_id;
    entry.pos = POS_UNKNOWN;

    if (!lexical_add_entry(lex, &entry)) {
        return 0;
    }

    /* Return the assigned word_id (next_word_id - 1 since it was incremented) */
    return lex->next_word_id - 1;
}

bool lexical_get_entry(
    const lexical_access_t* lex,
    uint32_t word_id,
    lexical_entry_t* entry)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entry is NULL");
        return false;
    }
    if (word_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "word_id is 0");
        return false;
    }

    if (word_id < lex->entries_capacity && lex->entries[word_id]) {
        *entry = *lex->entries[word_id];
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lexical_get_entry: validation failed");
    return false;
}

bool lexical_lookup_word(
    const lexical_access_t* lex,
    const char* word,
    lexical_entry_t* entry)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }
    if (!word) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "word is NULL");
        return false;
    }
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entry is NULL");
        return false;
    }

    uint32_t hash = hash_string(word, lex->hash_buckets);

    /* Batch H followup — lookup must be guarded against concurrent inserts.
     * The cast to mutable mutex is safe: we're locking the chain walk only.
     * Copy out the entry before unlocking so the borrowed node reference
     * doesn't outlive the lock. */
    nimcp_mutex_t* mtx = lex->lexicon_mutex;
    if (mtx) nimcp_mutex_lock(mtx);
    lexicon_node_t* node = lex->hash_table[hash];
    while (node) {
        if (strcmp(node->entry.orthography, word) == 0) {
            *entry = node->entry;
            if (mtx) nimcp_mutex_unlock(mtx);
            return true;
        }
        node = node->next;
    }
    if (mtx) nimcp_mutex_unlock(mtx);

    /* Miss is a normal outcome (not a validation failure). The legacy
     * code threw a generic error here; keep the call to satisfy any
     * immune-system callbacks but drop the misleading severity to OK
     * via a debug-level message — actual error reporting happens via
     * the false return. */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lexical_lookup_word: word not in lexicon");
    return false;
}

uint32_t lexical_get_size(const lexical_access_t* lex)
{
    return lex ? lex->num_words : 0;
}

bool lexical_set_embedding(
    lexical_access_t* lex,
    uint32_t word_id,
    const float* embedding,
    uint32_t dim)
{
    if (!lex || !embedding || word_id == 0 || word_id >= lex->entries_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lexical_set_embedding: required parameter is NULL (lex, embedding)");
        return false;
    }

    lexical_entry_t* entry = lex->entries[word_id];
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lexical_set_embedding: entry is NULL");
        return false;
    }

    if (!entry->embedding) {
        entry->embedding = (float*)nimcp_malloc(dim * sizeof(float));
        if (!entry->embedding) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lexical_set_embedding: entry->embedding is NULL");
            return false;
        }
    }

    memcpy(entry->embedding, embedding, dim * sizeof(float));
    return true;
}

/*=============================================================================
 * WORD RECOGNITION (Cohort Model)
 *===========================================================================*/

bool lexical_begin_recognition(lexical_access_t* lex)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }

    /* Reset recognition state */
    lex->cohort.num_members = 0;
    lex->cohort.phonemes_processed = 0;
    lex->cohort.uniqueness_reached = false;
    lex->cohort.winner_id = 0;
    lex->cohort.winner_confidence = 0.0f;

    lex->current_phoneme_count = 0;

    return true;
}

bool lexical_process_phoneme(
    lexical_access_t* lex,
    phoneme_t phoneme,
    float confidence)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }

    /* Store phoneme */
    if (lex->current_phoneme_count < lex->current_phoneme_capacity) {
        lex->current_phonemes[lex->current_phoneme_count++] = phoneme;
    }

    uint32_t pos = lex->cohort.phonemes_processed;

    if (pos == 0) {
        /* First phoneme: Generate initial cohort from trie */
        trie_node_t* node = lex->trie_root->children[(uint8_t)phoneme];
        if (!node) {
            /* No words start with this phoneme */
            lex->cohort.phonemes_processed = 1;
            return true;
        }

        /* Collect all words under this prefix */
        uint32_t word_ids[LEX_DEFAULT_MAX_COHORT_SIZE];
        uint32_t count = 0;
        trie_collect_words(node, word_ids, &count, lex->config.max_cohort_size);

        /* Initialize cohort members */
        for (uint32_t i = 0; i < count && i < lex->config.max_cohort_size; i++) {
            cohort_member_t* member = &lex->cohort.members[i];
            member->word_id = word_ids[i];
            member->matched_phonemes = 1;
            member->is_prefix_match = true;

            /* Get entry for frequency */
            lexical_entry_t entry;
            if (lexical_get_entry(lex, word_ids[i], &entry)) {
                /* Normalize frequency to [0,1] using Zipf scale (1-7) */
                member->frequency_score = entry.frequency / 7.0f;
            } else {
                member->frequency_score = 0.5f;
            }

            /* Initial activation = frequency weighted */
            member->phoneme_match = confidence;
            member->context_score = 0.0f;

            /* Check for priming boost */
            if (lex->config.enable_priming && lex->priming.num_primed > 0) {
                if (lexical_get_entry(lex, word_ids[i], &entry)) {
                    for (uint32_t j = 0; j < lex->priming.num_primed; j++) {
                        if (lex->priming.primed_concepts[j] == entry.concept_id) {
                            member->context_score = lex->priming.priming_levels[j];
                            break;
                        }
                    }
                }
            }

            member->activation =
                lex->config.phoneme_match_weight * member->phoneme_match +
                lex->config.frequency_weight * member->frequency_score +
                lex->config.context_weight * member->context_score;
        }

        lex->cohort.num_members = count;
        lex->stats.cohort_activations++;

    } else {
        /* Subsequent phonemes: Update cohort by elimination */
        uint32_t new_count = 0;

        for (uint32_t i = 0; i < lex->cohort.num_members; i++) {
            cohort_member_t* member = &lex->cohort.members[i];

            if (!member->is_prefix_match) continue;

            /* Get word's phoneme at this position */
            lexical_entry_t entry;
            if (!lexical_get_entry(lex, member->word_id, &entry)) {
                member->is_prefix_match = false;
                continue;
            }

            /* Check if word is long enough and matches */
            if (pos < entry.phoneme_count &&
                entry.phonemes[pos] == (uint8_t)phoneme) {
                /* Match! Update activation */
                member->matched_phonemes++;
                member->phoneme_match =
                    (member->phoneme_match * pos + confidence) / (pos + 1);

                member->activation =
                    lex->config.phoneme_match_weight * member->phoneme_match +
                    lex->config.frequency_weight * member->frequency_score +
                    lex->config.context_weight * member->context_score;

                /* Keep in cohort */
                if (new_count != i) {
                    lex->cohort.members[new_count] = *member;
                }
                new_count++;

            } else {
                /* Mismatch: remove from cohort */
                member->is_prefix_match = false;
            }
        }

        lex->cohort.num_members = new_count;
    }

    lex->cohort.phonemes_processed++;

    /* Check for uniqueness point */
    if (lex->cohort.num_members == 1) {
        cohort_member_t* winner = &lex->cohort.members[0];
        if (winner->activation >= lex->config.uniqueness_threshold) {
            lex->cohort.uniqueness_reached = true;
            lex->cohort.winner_id = winner->word_id;
            lex->cohort.winner_confidence = winner->activation;
        }
    } else if (lex->cohort.num_members > 1) {
        /* Check if leader is sufficiently ahead */
        float max_act = 0.0f;
        float second_act = 0.0f;
        uint32_t winner_id = 0;

        for (uint32_t i = 0; i < lex->cohort.num_members; i++) {
            float act = lex->cohort.members[i].activation;
            if (act > max_act) {
                second_act = max_act;
                max_act = act;
                winner_id = lex->cohort.members[i].word_id;
            } else if (act > second_act) {
                second_act = act;
            }
        }

        /* Winner if significantly ahead */
        if (max_act - second_act > 0.3f && max_act >= lex->config.uniqueness_threshold) {
            lex->cohort.uniqueness_reached = true;
            lex->cohort.winner_id = winner_id;
            lex->cohort.winner_confidence = max_act;
        }
    }

    return true;
}

bool lexical_is_recognized(const lexical_access_t* lex)
{
    return lex && lex->cohort.uniqueness_reached;
}

bool lexical_get_result(
    const lexical_access_t* lex,
    lexical_result_t* result)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");
        return false;
    }

    memset(result, 0, sizeof(lexical_result_t));

    result->cohort_size_initial = lex->stats.cohort_activations > 0 ?
        (uint32_t)lex->stats.avg_cohort_size : 0;
    result->cohort_size_final = lex->cohort.num_members;
    result->recognition_point = lex->cohort.phonemes_processed;

    if (lex->cohort.uniqueness_reached) {
        result->word_recognized = true;
        result->word_id = lex->cohort.winner_id;
        result->confidence = lex->cohort.winner_confidence;
        result->uniqueness_point = lex->cohort.phonemes_processed;

        /* Get word string */
        lexical_entry_t entry;
        if (lexical_get_entry(lex, lex->cohort.winner_id, &entry)) {
            strncpy(result->word, entry.orthography, LEX_DEFAULT_MAX_WORD_LENGTH - 1);
        }

        /* Compute competition index */
        result->competition_index = result->cohort_size_final > 1 ?
            1.0f - (1.0f / (float)result->cohort_size_final) : 0.0f;

    } else if (lex->cohort.num_members > 0) {
        /* No uniqueness, but return best candidate */
        float max_act = 0.0f;
        uint32_t best_id = 0;

        for (uint32_t i = 0; i < lex->cohort.num_members; i++) {
            if (lex->cohort.members[i].activation > max_act) {
                max_act = lex->cohort.members[i].activation;
                best_id = lex->cohort.members[i].word_id;
            }
        }

        result->word_recognized = (max_act > 0.5f);
        result->word_id = best_id;
        result->confidence = max_act;

        lexical_entry_t entry;
        if (lexical_get_entry(lex, best_id, &entry)) {
            strncpy(result->word, entry.orthography, LEX_DEFAULT_MAX_WORD_LENGTH - 1);
        }

        result->competition_index = 1.0f - (1.0f / (float)lex->cohort.num_members);
    }

    /* Fill alternatives */
    result->num_alternatives = 0;
    for (uint32_t i = 0; i < lex->cohort.num_members && result->num_alternatives < 5; i++) {
        if (lex->cohort.members[i].word_id != result->word_id) {
            result->alt_word_ids[result->num_alternatives] = lex->cohort.members[i].word_id;
            result->alt_confidences[result->num_alternatives] = lex->cohort.members[i].activation;
            result->num_alternatives++;
        }
    }

    return true;
}

bool lexical_recognize_word(
    lexical_access_t* lex,
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    lexical_result_t* result)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }
    if (!phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonemes is NULL");
        return false;
    }
    if (num_phonemes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "num_phonemes is 0");
        return false;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");
        return false;
    }

    lex->stats.lookups++;

    /* Begin recognition */
    lexical_begin_recognition(lex);

    /* Process all phonemes */
    for (uint32_t i = 0; i < num_phonemes; i++) {
        lexical_process_phoneme(lex, phonemes[i], 1.0f);

        /* Early exit if recognized */
        if (lex->cohort.uniqueness_reached) {
            break;
        }
    }

    /* Check for exact match at end of phoneme sequence */
    if (!lex->cohort.uniqueness_reached && lex->cohort.num_members > 0) {
        /* Look for word that ends exactly here */
        for (uint32_t i = 0; i < lex->cohort.num_members; i++) {
            cohort_member_t* member = &lex->cohort.members[i];
            lexical_entry_t entry;
            if (lexical_get_entry(lex, member->word_id, &entry)) {
                if (entry.phoneme_count == num_phonemes) {
                    /* Exact length match - boost confidence */
                    member->activation += 0.2f;
                    if (member->activation > 1.0f) member->activation = 1.0f;

                    if (member->activation >= lex->config.uniqueness_threshold) {
                        lex->cohort.uniqueness_reached = true;
                        lex->cohort.winner_id = member->word_id;
                        lex->cohort.winner_confidence = member->activation;
                        break;
                    }
                }
            }
        }
    }

    /* Get result */
    lexical_get_result(lex, result);

    /* Update stats */
    if (result->word_recognized) {
        lex->stats.hits++;
        float new_avg_recog =
            (lex->stats.avg_recognition_point * (lex->stats.hits - 1) +
             result->recognition_point) / lex->stats.hits;
        if (isfinite(new_avg_recog)) lex->stats.avg_recognition_point = new_avg_recog;
        float new_avg_conf =
            (lex->stats.avg_confidence * (lex->stats.hits - 1) +
             result->confidence) / lex->stats.hits;
        if (isfinite(new_avg_conf)) lex->stats.avg_confidence = new_avg_conf;
    } else {
        lex->stats.misses++;
    }

    return result->word_recognized;
}

/*=============================================================================
 * COHORT ACCESS
 *===========================================================================*/

bool lexical_get_cohort(
    const lexical_access_t* lex,
    const cohort_state_t** state)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state is NULL");
        return false;
    }
    *state = &lex->cohort;
    return true;
}

bool lexical_get_cohort_member(
    const lexical_access_t* lex,
    uint32_t index,
    cohort_member_t* member)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }
    if (!member) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "member is NULL");
        return false;
    }
    if (index >= lex->cohort.num_members) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "index out of range");
        return false;
    }
    *member = lex->cohort.members[index];
    return true;
}

bool lexical_boost_word(
    lexical_access_t* lex,
    uint32_t word_id,
    float boost)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }
    if (word_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "word_id is 0");
        return false;
    }

    for (uint32_t i = 0; i < lex->cohort.num_members; i++) {
        if (lex->cohort.members[i].word_id == word_id) {
            lex->cohort.members[i].context_score += boost;
            lex->cohort.members[i].activation =
                lex->config.phoneme_match_weight * lex->cohort.members[i].phoneme_match +
                lex->config.frequency_weight * lex->cohort.members[i].frequency_score +
                lex->config.context_weight * lex->cohort.members[i].context_score;
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lexical_boost_word: operation failed");
    return false;
}

/*=============================================================================
 * PRIMING
 *===========================================================================*/

bool lexical_prime_concept(
    lexical_access_t* lex,
    uint32_t concept_id,
    float strength)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }
    if (!lex->config.enable_priming) return false;
    if (concept_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "concept_id is 0");
        return false;
    }

    /* Check if already primed */
    for (uint32_t i = 0; i < lex->priming.num_primed; i++) {
        if (lex->priming.primed_concepts[i] == concept_id) {
            lex->priming.priming_levels[i] += strength;
            if (lex->priming.priming_levels[i] > 1.0f) {
                lex->priming.priming_levels[i] = 1.0f;
            }
            return true;
        }
    }

    /* Add new primed concept */
    if (lex->priming.num_primed < lex->priming.max_primed) {
        lex->priming.primed_concepts[lex->priming.num_primed] = concept_id;
        lex->priming.priming_levels[lex->priming.num_primed] = strength;
        lex->priming.num_primed++;
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lexical_prime_concept: validation failed");
    return false;
}

bool lexical_prime_word(
    lexical_access_t* lex,
    uint32_t word_id,
    float strength)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }
    if (word_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "word_id is 0");
        return false;
    }

    lexical_entry_t entry;
    if (!lexical_get_entry(lex, word_id, &entry)) return false;

    return lexical_prime_concept(lex, entry.concept_id, strength);
}

bool lexical_decay_priming(lexical_access_t* lex)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }
    if (!lex->config.enable_priming) return false;

    uint32_t new_count = 0;
    for (uint32_t i = 0; i < lex->priming.num_primed; i++) {
        lex->priming.priming_levels[i] *= lex->config.priming_decay;

        if (lex->priming.priming_levels[i] > 0.01f) {
            if (new_count != i) {
                lex->priming.primed_concepts[new_count] = lex->priming.primed_concepts[i];
                lex->priming.priming_levels[new_count] = lex->priming.priming_levels[i];
            }
            new_count++;
        }
    }

    lex->priming.num_primed = new_count;
    return true;
}

void lexical_clear_priming(lexical_access_t* lex)
{
    if (!lex) return;
    lex->priming.num_primed = 0;
}

bool lexical_get_priming(
    const lexical_access_t* lex,
    const priming_context_t** context)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }
    if (!context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "context is NULL");
        return false;
    }
    *context = &lex->priming;
    return true;
}

/*=============================================================================
 * NEIGHBORHOOD
 *===========================================================================*/

bool lexical_get_neighbors(
    lexical_access_t* lex,
    uint32_t word_id,
    uint32_t* neighbors,
    uint32_t max_neighbors,
    uint32_t* num_neighbors)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }
    if (!neighbors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neighbors is NULL");
        return false;
    }
    if (!num_neighbors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "num_neighbors is NULL");
        return false;
    }

    *num_neighbors = 0;

    lexical_entry_t target;
    if (!lexical_get_entry(lex, word_id, &target)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lexical_get_neighbors: lexical_get_entry is NULL");
        return false;
    }

    /* If neighbors are cached, use them */
    if (target.neighbors && target.num_neighbors > 0) {
        uint32_t count = target.num_neighbors < max_neighbors ?
            target.num_neighbors : max_neighbors;
        memcpy(neighbors, target.neighbors, count * sizeof(uint32_t));
        *num_neighbors = count;
        return true;
    }

    /* Otherwise, compute neighbors (edit distance = 1) */
    /* This is expensive, so we limit the search */
    for (uint32_t id = 1; id < lex->entries_capacity && *num_neighbors < max_neighbors; id++) {
        if (id == word_id) continue;

        lexical_entry_t candidate;
        if (!lexical_get_entry(lex, id, &candidate)) continue;

        /* Check edit distance */
        int len_diff = (int)target.phoneme_count - (int)candidate.phoneme_count;
        if (len_diff < -1 || len_diff > 1) continue;

        /* Count differences */
        uint32_t diffs = 0;
        uint32_t ti = 0, ci = 0;
        while (ti < target.phoneme_count && ci < candidate.phoneme_count) {
            if (target.phonemes[ti] != candidate.phonemes[ci]) {
                diffs++;
                if (diffs > 1) break;

                if (len_diff == 1) ti++;
                else if (len_diff == -1) ci++;
                else { ti++; ci++; }
            } else {
                ti++; ci++;
            }
        }

        diffs += (target.phoneme_count - ti) + (candidate.phoneme_count - ci);

        if (diffs == 1) {
            neighbors[(*num_neighbors)++] = id;
        }
    }

    return true;
}

uint32_t lexical_get_neighborhood_density(
    const lexical_access_t* lex,
    uint32_t word_id)
{
    if (!lex || word_id == 0 || word_id >= lex->entries_capacity) return 0;

    lexical_entry_t* entry = lex->entries[word_id];
    if (!entry) return 0;

    return entry->num_neighbors;
}

/*=============================================================================
 * FREQUENCY & STATISTICS
 *===========================================================================*/

float lexical_get_frequency(
    const lexical_access_t* lex,
    uint32_t word_id)
{
    if (!lex || word_id == 0 || word_id >= lex->entries_capacity) return 0.0f;

    lexical_entry_t* entry = lex->entries[word_id];
    return entry ? entry->frequency : 0.0f;
}

bool lexical_update_frequency(
    lexical_access_t* lex,
    uint32_t word_id,
    float delta)
{
    if (!lex || word_id == 0 || word_id >= lex->entries_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "lexical_update_frequency: lex is NULL");
        return false;
    }

    lexical_entry_t* entry = lex->entries[word_id];
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lexical_update_frequency: entry is NULL");
        return false;
    }

    entry->frequency += delta;
    if (entry->frequency < 1.0f) entry->frequency = 1.0f;
    if (entry->frequency > 7.0f) entry->frequency = 7.0f;

    return true;
}

bool lexical_get_stats(
    const lexical_access_t* lex,
    lexical_stats_t* stats)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");
        return false;
    }
    *stats = lex->stats;
    return true;
}

/*=============================================================================
 * BATCH OPERATIONS
 *===========================================================================*/

int32_t lexical_load_lexicon(
    lexical_access_t* lex,
    const char* filepath)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return -1;
    }
    if (!filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filepath is NULL");
        return -1;
    }

    FILE* fp = fopen(filepath, "rb");
    if (!fp) {
        NIMCP_LOG_WARN("lexical", "Failed to open lexicon file: %s", filepath);
        return -1;
    }

    /* Read header: magic, version, num_entries */
    uint32_t magic = 0, version = 0, num_entries = 0;
    if (fread(&magic, sizeof(uint32_t), 1, fp) != 1 ||
        fread(&version, sizeof(uint32_t), 1, fp) != 1 ||
        fread(&num_entries, sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    if (magic != 0x4C455849) {  /* "LEXI" */
        NIMCP_LOG_WARN("lexical", "Invalid lexicon format");
        fclose(fp);
        return -1;
    }

    /* Read entries */
    int32_t loaded = 0;
    for (uint32_t i = 0; i < num_entries; i++) {
        lexical_entry_t entry;
        memset(&entry, 0, sizeof(entry));

        if (fread(&entry.word_id, sizeof(uint32_t), 1, fp) != 1 ||
            fread(entry.orthography, sizeof(entry.orthography), 1, fp) != 1 ||
            fread(entry.phonemes, sizeof(entry.phonemes), 1, fp) != 1 ||
            fread(&entry.phoneme_count, sizeof(uint32_t), 1, fp) != 1 ||
            fread(&entry.pos, sizeof(entry.pos), 1, fp) != 1 ||
            fread(&entry.frequency, sizeof(float), 1, fp) != 1 ||
            fread(&entry.syllable_count, sizeof(uint32_t), 1, fp) != 1 ||
            fread(&entry.concept_id, sizeof(uint32_t), 1, fp) != 1) {
            break;
        }

        if (lexical_add_entry(lex, &entry)) {
            loaded++;
        }
    }

    fclose(fp);
    NIMCP_LOG_INFO("lexical", "Loaded %d lexicon entries from %s", loaded, filepath);
    return loaded;
}

bool lexical_save_lexicon(
    const lexical_access_t* lex,
    const char* filepath)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return false;
    }
    if (!filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filepath is NULL");
        return false;
    }

    FILE* fp = fopen(filepath, "wb");
    if (!fp) {
        NIMCP_LOG_WARN("lexical", "Failed to open file for writing: %s", filepath);
        return false;
    }

    /* Write header */
    uint32_t magic = 0x4C455849;  /* "LEXI" */
    uint32_t version = 1;
    uint32_t num_entries = lex->num_words;
    fwrite(&magic, sizeof(uint32_t), 1, fp);
    fwrite(&version, sizeof(uint32_t), 1, fp);
    fwrite(&num_entries, sizeof(uint32_t), 1, fp);

    /* Write entries from direct access array */
    uint32_t written = 0;
    for (uint32_t i = 0; i < lex->entries_capacity; i++) {
        if (!lex->entries[i]) continue;
        lexical_entry_t* entry = lex->entries[i];

        fwrite(&entry->word_id, sizeof(uint32_t), 1, fp);
        fwrite(entry->orthography, sizeof(entry->orthography), 1, fp);
        fwrite(entry->phonemes, sizeof(entry->phonemes), 1, fp);
        fwrite(&entry->phoneme_count, sizeof(uint32_t), 1, fp);
        fwrite(&entry->pos, sizeof(entry->pos), 1, fp);
        fwrite(&entry->frequency, sizeof(float), 1, fp);
        fwrite(&entry->syllable_count, sizeof(uint32_t), 1, fp);
        fwrite(&entry->concept_id, sizeof(uint32_t), 1, fp);
        written++;
    }

    fclose(fp);
    NIMCP_LOG_INFO("lexical", "Saved %u lexicon entries to %s", written, filepath);
    return true;
}

uint32_t lexical_build_common_english(lexical_access_t* lex)
{
    if (!lex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lex is NULL");
        return 0;
    }

    /* Common English words with phoneme transcriptions */
    /* Format: word, phonemes, count, frequency (Zipf 1-7), concept_id */
    /* Static to avoid stack pressure — this function is called deep in the init chain */

    static const struct {
        const char* word;
        phoneme_t phonemes[10];
        uint32_t num_phonemes;
        float frequency;
    } common_words[] = {
        {"the", {PHONEME_DH, PHONEME_AH}, 2, 7.0f},
        {"a", {PHONEME_AH}, 1, 6.8f},
        {"is", {PHONEME_IH, PHONEME_Z}, 2, 6.5f},
        {"it", {PHONEME_IH, PHONEME_T}, 2, 6.4f},
        {"to", {PHONEME_T, PHONEME_UW}, 2, 6.7f},
        {"and", {PHONEME_AE, PHONEME_N, PHONEME_D}, 3, 6.6f},
        {"of", {PHONEME_AH, PHONEME_V}, 2, 6.6f},
        {"in", {PHONEME_IH, PHONEME_N}, 2, 6.3f},
        {"that", {PHONEME_DH, PHONEME_AE, PHONEME_T}, 3, 6.2f},
        {"you", {PHONEME_Y, PHONEME_UW}, 2, 6.1f},
        {"he", {PHONEME_H, PHONEME_IY}, 2, 5.9f},
        {"she", {PHONEME_SH, PHONEME_IY}, 2, 5.8f},
        {"we", {PHONEME_W, PHONEME_IY}, 2, 5.7f},
        {"they", {PHONEME_DH, PHONEME_EY}, 2, 5.6f},
        {"are", {PHONEME_AA, PHONEME_R}, 2, 5.8f},
        {"was", {PHONEME_W, PHONEME_AA, PHONEME_Z}, 3, 5.9f},
        {"have", {PHONEME_H, PHONEME_AE, PHONEME_V}, 3, 5.7f},
        {"has", {PHONEME_H, PHONEME_AE, PHONEME_Z}, 3, 5.5f},
        {"not", {PHONEME_N, PHONEME_AA, PHONEME_T}, 3, 5.6f},
        {"with", {PHONEME_W, PHONEME_IH, PHONEME_TH}, 3, 5.5f},
        {"this", {PHONEME_DH, PHONEME_IH, PHONEME_S}, 3, 5.4f},
        {"but", {PHONEME_B, PHONEME_AH, PHONEME_T}, 3, 5.4f},
        {"from", {PHONEME_F, PHONEME_R, PHONEME_AH, PHONEME_M}, 4, 5.3f},
        {"or", {PHONEME_AO, PHONEME_R}, 2, 5.3f},
        {"by", {PHONEME_B, PHONEME_AH, PHONEME_IY}, 3, 5.2f},
        {"one", {PHONEME_W, PHONEME_AH, PHONEME_N}, 3, 5.2f},
        {"all", {PHONEME_AO, PHONEME_L}, 2, 5.1f},
        {"word", {PHONEME_W, PHONEME_ER, PHONEME_D}, 3, 4.5f},
        {"cat", {PHONEME_K, PHONEME_AE, PHONEME_T}, 3, 4.0f},
        {"dog", {PHONEME_D, PHONEME_AO, PHONEME_G}, 3, 4.1f},
        {"house", {PHONEME_H, PHONEME_AO, PHONEME_S}, 3, 4.3f},
        {"tree", {PHONEME_T, PHONEME_R, PHONEME_IY}, 3, 4.0f},
        {"book", {PHONEME_B, PHONEME_UH, PHONEME_K}, 3, 4.2f},
        {"water", {PHONEME_W, PHONEME_AO, PHONEME_T, PHONEME_ER}, 4, 4.4f},
        {"time", {PHONEME_T, PHONEME_AH, PHONEME_IY, PHONEME_M}, 4, 5.0f},
        {"day", {PHONEME_D, PHONEME_EY}, 2, 4.8f},
        {"way", {PHONEME_W, PHONEME_EY}, 2, 4.9f},
        {"man", {PHONEME_M, PHONEME_AE, PHONEME_N}, 3, 4.7f},
        {"new", {PHONEME_N, PHONEME_UW}, 2, 4.8f},
        {"now", {PHONEME_N, PHONEME_AO}, 2, 4.7f},
        {"good", {PHONEME_G, PHONEME_UH, PHONEME_D}, 3, 4.6f},
        {"know", {PHONEME_N, PHONEME_OW}, 2, 4.8f},
        {"think", {PHONEME_TH, PHONEME_IH, PHONEME_NG, PHONEME_K}, 4, 4.5f},
        {"come", {PHONEME_K, PHONEME_AH, PHONEME_M}, 3, 4.6f},
        {"go", {PHONEME_G, PHONEME_OW}, 2, 4.7f},
        {"see", {PHONEME_S, PHONEME_IY}, 2, 4.6f},
        {"say", {PHONEME_S, PHONEME_EY}, 2, 4.7f},
        {"get", {PHONEME_G, PHONEME_EH, PHONEME_T}, 3, 4.6f},
        {"make", {PHONEME_M, PHONEME_EY, PHONEME_K}, 3, 4.5f},
        {"can", {PHONEME_K, PHONEME_AE, PHONEME_N}, 3, 4.8f},
    };

    uint32_t count = 0;
    uint32_t num_words = sizeof(common_words) / sizeof(common_words[0]);

    for (uint32_t i = 0; i < num_words; i++) {
        uint32_t id = lexical_add_word(
            lex,
            common_words[i].word,
            common_words[i].phonemes,
            common_words[i].num_phonemes,
            common_words[i].frequency,
            i + 1  /* concept_id = index + 1 */
        );
        if (id > 0) count++;
    }

    NIMCP_LOG_INFO("lexical", "Built %u common English words", count);
    return count;
}
