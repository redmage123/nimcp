/**
 * @file nimcp_wernicke_adapter.c
 * @brief Implementation of Wernicke's area adapter for language comprehension
 *
 * WHAT: Core implementation of language comprehension processing
 * WHY:  Enable speech understanding in NIMCP brain
 * HOW:  Phonological → Lexical → Semantic → Syntactic pipeline
 *
 * @version Phase W1: Wernicke's Area Core Integration
 * @date 2026-01-04
 */

#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "core/brain/regions/wernicke/nimcp_syntactic_comprehension.h"
#include "core/brain/regions/wernicke/nimcp_phonological_analyzer.h"
#include "core/brain/regions/wernicke/nimcp_lexical_access.h"
/* NOTE: nimcp_semantic_integrator.h cannot be included here because it
 * redefines `thematic_role_t` (ROLE_AGENT, ROLE_GOAL, ROLE_COUNT, ...)
 * which is already pulled in via nimcp_syntactic_comprehension.h above —
 * both headers define the same enum with overlapping values, causing
 * redeclaration errors. We only need the lifecycle entry points; the
 * opaque type semantic_integrator_t is already forward-declared in the
 * adapter header. Declare just the create/destroy functions locally. */
typedef struct semantic_config semantic_config_t;
extern semantic_integrator_t* semantic_create(const semantic_config_t* config);
extern void semantic_destroy(semantic_integrator_t* sem);

#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(wernicke_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Lexicon entry for hash table
 */
typedef struct lexicon_entry {
    wernicke_word_t word;
    struct lexicon_entry* next;
} lexicon_entry_t;

/**
 * @brief Working memory state
 */
typedef struct {
    phoneme_t buffer[WERNICKE_DEFAULT_WORKING_MEMORY_SLOTS * 4];
    uint32_t count;
    uint32_t capacity;
    float decay_rate;
    uint64_t last_rehearsal_ms;
} working_memory_t;

/**
 * @brief Phoneme buffer for processing
 */
typedef struct {
    phoneme_event_t* events;
    uint32_t count;
    uint32_t capacity;
} phoneme_buffer_t;

/**
 * @brief Wernicke adapter internal structure
 */
struct wernicke_adapter {
    /* Configuration */
    wernicke_config_t config;

    /* Processing state */
    wernicke_status_t status;
    wernicke_error_t last_error;

    /* Sub-modules (Phase 2 will implement these fully) */
    phonological_analyzer_t* phonological;
    lexical_access_t* lexical;
    semantic_integrator_t* semantic;
    syntactic_comprehension_t* syntactic;

    /* Lexicon (simple hash table) */
    lexicon_entry_t** lexicon_table;
    uint32_t lexicon_count;
    uint32_t lexicon_capacity;
    /* Walkthrough-3 fix — separate from lexical_access_t's own
     * lexicon_mutex (commit a6f965029). The wernicke_adapter owns a
     * SECOND phoneme-keyed lexicon_table here. Writers: wernicke_add_word
     * + gl_mirror_new_word_to_regions. Readers: wernicke_lookup_word
     * + wernicke_recognize_word + wernicke_destroy. Pre-fix the chain
     * walks raced the add_word inserts when the GL mirror ran
     * concurrently from comprehend on the RO socket pool. NULL on
     * alloc failure degrades to no-op locking. */
    nimcp_mutex_t* lexicon_mutex;

    /* Working memory */
    working_memory_t working_memory;

    /* Phoneme buffer */
    phoneme_buffer_t phoneme_buffer;

    /* External connections */
    broca_adapter_t* broca;
    speech_cortex_t* speech_cortex;
    semantic_memory_t* semantic_memory;
    brain_kg_t* kg;

    /* Callbacks */
    wernicke_word_callback_t word_callback;
    void* word_callback_data;
    wernicke_concept_callback_t concept_callback;
    void* concept_callback_data;
    wernicke_comprehension_callback_t comprehension_callback;
    void* comprehension_callback_data;
    wernicke_event_callback_t event_callback;
    void* event_callback_data;

    /* Statistics */
    wernicke_stats_t stats;

    /* Bio-async communication */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
    nimcp_bio_channel_type_t default_channel;

    /* SNN substrate binding (wernicke_substrate population — 64K neurons,
     * created by nimcp_brain_factory_init_language_pops). pop_id < 0 = unbound.
     * Adapter logic uses (snn, snn_pop_id) to read spike rates for gating
     * comprehension on actual cortical drive. Lifetime: SNN owns its pops;
     * adapter does NOT free them. */
    snn_network_t* snn;
    int snn_pop_id;

    /* Grounded-language binding (opaque). Future consumers will use
     * this to ingest comprehension results as auditory grounding
     * events. Lifetime: GL is owned by brain; adapter does NOT free. */
    void* grounded_lang;
};

/*=============================================================================
 * LEXICON HASH TABLE HELPERS
 *===========================================================================*/

/**
 * @brief Simple string hash function
 */
static uint32_t hash_string(const char* str, uint32_t table_size)
{
    uint32_t hash = 5381;
    int c;
    while ((c = *str++) != 0) {
        hash = ((hash << 5) + hash) + (uint32_t)c;
    }
    return hash % table_size;
}

/**
 * @brief Hash phoneme sequence
 */
static uint32_t hash_phonemes(const uint8_t* phonemes, uint32_t count, uint32_t table_size)
{
    uint32_t hash = 5381;
    for (uint32_t i = 0; i < count; i++) {
        hash = ((hash << 5) + hash) + phonemes[i];
    }
    return hash % table_size;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

wernicke_config_t wernicke_default_config(void)
{
    wernicke_config_t config = {
        /* Capacity limits */
        .max_phonemes = WERNICKE_DEFAULT_MAX_PHONEMES,
        .max_words = WERNICKE_DEFAULT_MAX_WORDS,
        .max_concepts = WERNICKE_DEFAULT_MAX_CONCEPTS,

        /* Lexicon configuration */
        .lexicon_size = WERNICKE_DEFAULT_LEXICON_SIZE,
        .enable_lexicon = true,

        /* Working memory */
        .working_memory_slots = WERNICKE_DEFAULT_WORKING_MEMORY_SLOTS,
        .enable_working_memory = true,

        /* Processing layers */
        .enable_phonological = true,
        .enable_lexical = true,
        .enable_semantic = true,
        .enable_syntactic = false,  /* Phase 5 */

        /* Feature configuration */
        .embedding_dim = WERNICKE_DEFAULT_EMBEDDING_DIM,
        .formant_count = WERNICKE_DEFAULT_FORMANT_COUNT,

        /* Cross-modal */
        .enable_audiovisual = false,  /* Phase 4 */
        .enable_prosody = true,

        /* External connections */
        .enable_broca_connection = true,
        .enable_semantic_memory = true,
        .enable_kg_registration = true,

        /* Event system */
        .enable_events = true,

        /* Training */
        .enable_training = true,
        .learning_rate = NIMCP_LEARNING_RATE_DEFAULT,

        /* Timing */
        .processing_window_ms = WERNICKE_DEFAULT_PROCESSING_WINDOW_MS,

        /* Bio-async */
        .enable_bio_async = true,
        .default_channel = BIO_CHANNEL_DOPAMINE
    };
    return config;
}

wernicke_adapter_t* wernicke_create(const wernicke_config_t* config)
{
    wernicke_config_t cfg = config ? *config : wernicke_default_config();

    /* Allocate adapter */
    wernicke_adapter_t* adapter = (wernicke_adapter_t*)nimcp_calloc(1, sizeof(wernicke_adapter_t));
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_create: failed to allocate adapter");
        return NULL;
    }

    adapter->config = cfg;
    adapter->status = WERNICKE_STATUS_IDLE;
    adapter->last_error = WERNICKE_ERROR_NONE;

    /* Allocate lexicon hash table */
    if (cfg.enable_lexicon) {
        adapter->lexicon_capacity = cfg.lexicon_size;
        adapter->lexicon_table = (lexicon_entry_t**)nimcp_calloc(cfg.lexicon_size, sizeof(lexicon_entry_t*));
        if (!adapter->lexicon_table) {
            nimcp_free(adapter);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_create: adapter->lexicon_table is NULL");
            return NULL;
        }
        adapter->lexicon_count = 0;
        /* Walkthrough-3 fix — guard add/lookup/recognize against
         * concurrent inserts. NULL on alloc failure degrades to
         * unlocked (matches Broca's pattern at commit b1417e2a9). */
        adapter->lexicon_mutex = nimcp_mutex_create(NULL);
        if (!adapter->lexicon_mutex) {
            NIMCP_LOG_WARN("wernicke", "lexicon_mutex create failed — running unlocked");
        }
    }

    /* Initialize working memory */
    if (cfg.enable_working_memory) {
        adapter->working_memory.capacity = cfg.working_memory_slots * 4; /* 4 phonemes per slot avg */
        adapter->working_memory.count = 0;
        adapter->working_memory.decay_rate = 0.1f;
        adapter->working_memory.last_rehearsal_ms = 0;
    }

    /* Allocate phoneme buffer */
    adapter->phoneme_buffer.capacity = cfg.max_phonemes;
    adapter->phoneme_buffer.events = (phoneme_event_t*)nimcp_calloc(cfg.max_phonemes, sizeof(phoneme_event_t));
    if (!adapter->phoneme_buffer.events) {
        if (adapter->lexicon_table) nimcp_free(adapter->lexicon_table);
        nimcp_free(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_create: failed to allocate phoneme buffer");
        return NULL;
    }
    adapter->phoneme_buffer.count = 0;

    /* Initialize bio-async if enabled */
    adapter->bio_async_enabled = cfg.enable_bio_async;
    adapter->default_channel = cfg.default_channel;
    adapter->bio_ctx = NULL;  /* Will be registered via wernicke_register_kg */

    /* Initialize SNN substrate binding to "unbound" — calloc gave 0, but 0
     * is a valid SNN pop id, so we MUST explicitly set -1 here. */
    adapter->snn = NULL;
    adapter->snn_pop_id = -1;

    /* Create comprehension submodules (NULL config = each module's defaults).
     * Without these, wernicke_get_phonological_analyzer/lexical_access/
     * semantic_integrator/syntactic_comprehension all return NULL and the
     * language cascade + init-time lexicon seeding stays statue.
     * Non-fatal on individual failures — leave the field NULL and let
     * downstream NULL-checks handle it. */
    adapter->phonological = wernicke_phonological_create(NULL);
    if (!adapter->phonological) {
        NIMCP_LOG_WARN("wernicke", "wernicke_phonological_create returned NULL — phonological analysis disabled");
    }

    adapter->lexical = lexical_create(NULL);
    if (!adapter->lexical) {
        NIMCP_LOG_WARN("wernicke", "lexical_create returned NULL — lexical access disabled");
    }

    adapter->semantic = semantic_create(NULL);
    if (!adapter->semantic) {
        NIMCP_LOG_WARN("wernicke", "semantic_create returned NULL — semantic integration disabled");
    }

    adapter->syntactic = syntactic_comprehension_create(NULL);
    if (!adapter->syntactic) {
        NIMCP_LOG_WARN("wernicke", "syntactic_comprehension_create returned NULL — parsing disabled");
    }

    /* Initialize statistics */
    memset(&adapter->stats, 0, sizeof(wernicke_stats_t));

    NIMCP_LOG_INFO("wernicke", "Created Wernicke's area adapter (lexicon=%u, wm=%u slots)",
                   cfg.lexicon_size, cfg.working_memory_slots);

    return adapter;
}

void wernicke_destroy(wernicke_adapter_t* adapter)
{
    if (!adapter) return;

    /* Free lexicon entries */
    if (adapter->lexicon_table) {
        for (uint32_t i = 0; i < adapter->lexicon_capacity; i++) {
            lexicon_entry_t* entry = adapter->lexicon_table[i];
            while (entry) {
                lexicon_entry_t* next = entry->next;
                nimcp_free(entry);
                entry = next;
            }
        }
        nimcp_free(adapter->lexicon_table);
    }
    if (adapter->lexicon_mutex) {
        nimcp_mutex_free(adapter->lexicon_mutex);
        adapter->lexicon_mutex = NULL;
    }

    /* Free phoneme buffer */
    if (adapter->phoneme_buffer.events) {
        nimcp_free(adapter->phoneme_buffer.events);
    }

    /* Free sub-modules — Phase 2: phonological, lexical, semantic, syntactic.
     * Each module's destroy() handles NULL safely; symmetric ordering with
     * create above is not strictly required but kept for readability. */
    if (adapter->phonological) {
        wernicke_phonological_destroy(adapter->phonological);
        adapter->phonological = NULL;
    }
    if (adapter->lexical) {
        lexical_destroy(adapter->lexical);
        adapter->lexical = NULL;
    }
    if (adapter->semantic) {
        semantic_destroy(adapter->semantic);
        adapter->semantic = NULL;
    }
    if (adapter->syntactic) {
        syntactic_comprehension_destroy(adapter->syntactic);
        adapter->syntactic = NULL;
    }

    NIMCP_LOG_INFO("wernicke", "Destroyed Wernicke's area adapter");

    nimcp_free(adapter);
}

bool wernicke_reset(wernicke_adapter_t* adapter)
{
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_reset: adapter is NULL");
        return false;
    }

    adapter->status = WERNICKE_STATUS_IDLE;
    adapter->last_error = WERNICKE_ERROR_NONE;

    /* Clear phoneme buffer */
    adapter->phoneme_buffer.count = 0;

    /* Clear working memory */
    adapter->working_memory.count = 0;

    NIMCP_LOG_DEBUG("wernicke", "Reset adapter state");
    return true;
}

/*=============================================================================
 * PHONOLOGICAL PROCESSING
 *===========================================================================*/

bool wernicke_process_audio(
    wernicke_adapter_t* adapter,
    const float* audio,
    uint32_t num_samples,
    uint32_t sample_rate,
    phoneme_event_t* phonemes,
    uint32_t max_phonemes,
    uint32_t* num_detected)
{
    if (!adapter || !audio || !phonemes || !num_detected) {
        if (adapter) adapter->last_error = WERNICKE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_process_audio: validation failed");
        return false;
    }

    adapter->status = WERNICKE_STATUS_PHONOLOGICAL;

    /* Phase 2 will implement full formant analysis
     * For now, delegate to speech cortex if connected */
    if (adapter->speech_cortex) {
        /* Use speech cortex for phoneme detection */
        bool result = speech_cortex_detect_phonemes(
            adapter->speech_cortex,
            audio,
            num_samples,
            phonemes,
            max_phonemes,
            num_detected
        );
        if (result) {
            adapter->stats.phonemes_processed += *num_detected;
        }
        adapter->status = WERNICKE_STATUS_IDLE;
        return result;
    }

    /* Fallback: No speech cortex connected */
    /* Phase 2 will implement standalone phoneme detection */
    *num_detected = 0;
    adapter->status = WERNICKE_STATUS_IDLE;
    NIMCP_LOG_WARN("wernicke", "No speech cortex connected for phoneme detection");
    return true;
}

bool wernicke_process_phonemes(
    wernicke_adapter_t* adapter,
    const phoneme_event_t* phonemes,
    uint32_t count)
{
    if (!adapter || !phonemes || count == 0) {
        if (adapter) adapter->last_error = WERNICKE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_process_phonemes: validation failed");
        return false;
    }

    if (count > adapter->phoneme_buffer.capacity) {
        adapter->last_error = WERNICKE_ERROR_BUFFER_OVERFLOW;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_process_phonemes: validation failed");
        return false;
    }

    adapter->status = WERNICKE_STATUS_PHONOLOGICAL;

    /* Store phonemes in buffer */
    memcpy(adapter->phoneme_buffer.events, phonemes, count * sizeof(phoneme_event_t));
    adapter->phoneme_buffer.count = count;
    adapter->stats.phonemes_processed += count;

    /* Calculate average confidence */
    float total_conf = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        total_conf += phonemes[i].confidence;
    }

    NIMCP_LOG_DEBUG("wernicke", "Processed %u phonemes (avg conf=%.2f)",
                   count, total_conf / (float)count);

    adapter->status = WERNICKE_STATUS_IDLE;
    return true;
}

/*=============================================================================
 * LEXICAL ACCESS
 *===========================================================================*/

bool wernicke_recognize_word(
    wernicke_adapter_t* adapter,
    const phoneme_t* phonemes,
    uint32_t count,
    wernicke_word_result_t* result)
{
    if (!adapter || !phonemes || count == 0 || !result) {
        if (adapter) adapter->last_error = WERNICKE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_recognize_word: validation failed");
        return false;
    }

    adapter->status = WERNICKE_STATUS_LEXICAL_ACCESS;

    /* Hash the phoneme sequence */
    uint32_t hash = hash_phonemes((const uint8_t*)phonemes, count, adapter->lexicon_capacity);

    /* Walkthrough-3 fix — guard chain walk. Snapshot the matched entry
     * BEFORE the callback fires (callback runs OUTSIDE the lock to avoid
     * re-entry if it writes back into wernicke). */
    bool matched = false;
    if (adapter->lexicon_mutex) nimcp_mutex_lock(adapter->lexicon_mutex);
    lexicon_entry_t* entry = adapter->lexicon_table[hash];
    while (entry) {
        if (entry->word.phoneme_count == count) {
            bool match = true;
            for (uint32_t i = 0; i < count; i++) {
                if (entry->word.phonemes[i] != (uint8_t)phonemes[i]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                result->word = entry->word;
                result->confidence = 1.0f;
                result->onset_time_ms = 0;
                result->offset_time_ms = 0;
                result->position_in_utterance = 0;
                matched = true;
                break;
            }
        }
        entry = entry->next;
    }
    if (adapter->lexicon_mutex) nimcp_mutex_unlock(adapter->lexicon_mutex);

    if (matched) {
        adapter->stats.words_recognized++;
        adapter->stats.successful_recognitions++;
        if (adapter->word_callback) {
            adapter->word_callback(result, adapter->word_callback_data);
        }
        NIMCP_LOG_DEBUG("wernicke", "Recognized word: %s (conf=%.2f)",
                       result->word.word, result->confidence);
        adapter->status = WERNICKE_STATUS_IDLE;
        return true;
    }

    /* Word not found — normal lookup miss, not an error */
    adapter->last_error = WERNICKE_ERROR_WORD_NOT_FOUND;
    adapter->stats.lexical_misses++;
    adapter->status = WERNICKE_STATUS_IDLE;
    return false;
}

bool wernicke_add_word(
    wernicke_adapter_t* adapter,
    const wernicke_word_t* word)
{
    /* Walkthrough-3 fix — relax the phoneme_count==0 rejection. The
     * GL→Wernicke lexicon mirror (gl_mirror_new_word_to_regions) sets
     * phoneme_count=0 because it doesn't have a G2P pipeline at the
     * point of new-word emission. Pre-fix, 100% of mirror calls were
     * rejected here, so wernicke's lexicon never got any words from
     * GL — silent capacity drop pattern (memory feedback_silent_capacity_drops).
     *
     * A phoneme_count=0 entry is a "graphemic seed" — it doesn't match
     * phoneme-keyed lookups (wernicke_recognize_word keys on phonemes)
     * but does match string-keyed lookups (wernicke_lookup_word
     * compares via strcmp on entry->word.word), which is the path the
     * mirror cares about. Phonemes can be derived on demand at first
     * phonological lookup. */
    if (!adapter || !word || !word->word[0]) {
        if (adapter) adapter->last_error = WERNICKE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_add_word: validation failed");
        return false;
    }

    if (!adapter->lexicon_table) {
        adapter->last_error = WERNICKE_ERROR_INTERNAL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_add_word: adapter->lexicon_table is NULL");
        return false;
    }

    /* Hash by phoneme sequence — for graphemic seeds (phoneme_count=0)
     * this hashes the empty buffer to a stable bucket, which is fine
     * since string-keyed lookups scan all buckets anyway. */
    uint32_t hash = hash_phonemes(word->phonemes, word->phoneme_count, adapter->lexicon_capacity);

    /* Create new entry. Pre-alloc OUTSIDE the lock to keep the
     * critical section tight. */
    lexicon_entry_t* entry = (lexicon_entry_t*)nimcp_malloc(sizeof(lexicon_entry_t));
    if (!entry) {
        adapter->last_error = WERNICKE_ERROR_INTERNAL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_add_word: entry is NULL");
        return false;
    }
    entry->word = *word;

    /* Walkthrough-3 fix — guard hash-chain link + count bump. */
    if (adapter->lexicon_mutex) nimcp_mutex_lock(adapter->lexicon_mutex);
    entry->next = adapter->lexicon_table[hash];
    adapter->lexicon_table[hash] = entry;
    adapter->lexicon_count++;
    if (adapter->lexicon_mutex) nimcp_mutex_unlock(adapter->lexicon_mutex);

    NIMCP_LOG_DEBUG("wernicke", "Added word to lexicon: %s (%u phonemes)",
                   word->word, word->phoneme_count);

    return true;
}

bool wernicke_lookup_word(
    const wernicke_adapter_t* adapter,
    const char* word_str,
    wernicke_word_t* entry)
{
    if (!adapter || !word_str || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_lookup_word: required parameter is NULL (adapter, word_str, entry)");
        return false;
    }

    if (!adapter->lexicon_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_lookup_word: adapter->lexicon_table is NULL");
        return false;
    }

    /* Walkthrough-3 fix — guard the chain walk against concurrent
     * inserts. Cast to non-const is safe: we lock for read-walk only. */
    nimcp_mutex_t* mtx = adapter->lexicon_mutex;
    if (mtx) nimcp_mutex_lock(mtx);

    /* Lexicon is indexed by phoneme hash, so we must search all buckets
     * to find a word by its string. This is O(n) but acceptable for
     * typical lexicon sizes. A secondary string index could be added
     * for O(1) string lookups if needed. */
    for (uint32_t i = 0; i < adapter->lexicon_capacity; i++) {
        lexicon_entry_t* e = adapter->lexicon_table[i];
        while (e) {
            if (strcmp(e->word.word, word_str) == 0) {
                *entry = e->word;
                if (mtx) nimcp_mutex_unlock(mtx);
                return true;
            }
            e = e->next;
        }
    }
    if (mtx) nimcp_mutex_unlock(mtx);

    /* Word not found — normal lookup miss, not an error */
    return false;
}

bool wernicke_predict_next_word(
    wernicke_adapter_t* adapter,
    const wernicke_context_t* context,
    wernicke_word_pred_t* prediction)
{
    if (!adapter || !prediction) {
        if (adapter) adapter->last_error = WERNICKE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_predict_next_word: validation failed");
        return false;
    }

    /* Phase 3 will implement n-gram prediction and semantic priming */
    /* For now, return empty prediction */
    prediction->num_candidates = 0;
    return true;
}

/*=============================================================================
 * SEMANTIC INTEGRATION
 *===========================================================================*/

bool wernicke_get_meaning(
    wernicke_adapter_t* adapter,
    const wernicke_word_result_t* word,
    wernicke_concept_t* concept_out)
{
    if (!adapter || !word || !concept_out) {
        if (adapter) adapter->last_error = WERNICKE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_get_meaning: validation failed");
        return false;
    }

    adapter->status = WERNICKE_STATUS_SEMANTIC;

    /* Phase 3 will implement full semantic memory integration */
    /* For now, create a basic concept from the word */
    concept_out->concept_id = word->word.concept_id;
    strncpy(concept_out->concept_name, word->word.word, sizeof(concept_out->concept_name) - 1);
    concept_out->concept_name[sizeof(concept_out->concept_name) - 1] = '\0';
    concept_out->activation = word->confidence;
    concept_out->embedding = NULL;
    concept_out->embedding_dim = 0;
    concept_out->related_concepts = NULL;
    concept_out->num_related = 0;

    adapter->stats.concepts_activated++;

    /* Fire callback if set */
    if (adapter->concept_callback) {
        adapter->concept_callback(concept_out, adapter->concept_callback_data);
    }

    adapter->status = WERNICKE_STATUS_IDLE;
    return true;
}

bool wernicke_disambiguate(
    wernicke_adapter_t* adapter,
    const wernicke_word_result_t* word,
    const wernicke_context_t* context,
    wernicke_concept_t* concept_out)
{
    if (!adapter || !word || !concept_out) {
        if (adapter) adapter->last_error = WERNICKE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_disambiguate: validation failed");
        return false;
    }

    /* Phase 3 will implement context-based disambiguation */
    /* For now, just return the first meaning */
    return wernicke_get_meaning(adapter, word, concept_out);
}

bool wernicke_spread_activation(
    wernicke_adapter_t* adapter,
    uint32_t concept_id,
    uint32_t depth,
    wernicke_concept_t* activated,
    uint32_t max_concepts,
    uint32_t* num_activated)
{
    if (!adapter || !activated || !num_activated) {
        if (adapter) adapter->last_error = WERNICKE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_spread_activation: validation failed");
        return false;
    }

    /* Phase 3 will implement spreading activation over semantic graph */
    *num_activated = 0;
    return true;
}

/*=============================================================================
 * SENTENCE COMPREHENSION
 *===========================================================================*/

bool wernicke_comprehend(
    wernicke_adapter_t* adapter,
    const float* audio,
    uint32_t num_samples,
    uint32_t sample_rate,
    wernicke_comprehension_t* result)
{
    if (!adapter || !audio || !result) {
        if (adapter) adapter->last_error = WERNICKE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_comprehend: validation failed");
        return false;
    }

    /* Initialize result */
    memset(result, 0, sizeof(wernicke_comprehension_t));

    /* Step 1: Phonological processing */
    phoneme_event_t* phonemes = adapter->phoneme_buffer.events;
    uint32_t num_phonemes = 0;

    if (!wernicke_process_audio(adapter, audio, num_samples, sample_rate,
                                phonemes, adapter->phoneme_buffer.capacity, &num_phonemes)) {
        adapter->last_error = WERNICKE_ERROR_PHONOLOGICAL_FAILURE;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_comprehend: operation failed");
        return false;
    }
    result->phoneme_count = num_phonemes;

    /* Step 2: Lexical access - word segmentation */
    /* Phase 2 will implement proper word boundary detection */
    adapter->status = WERNICKE_STATUS_LEXICAL_ACCESS;

    /* Step 3: Semantic integration */
    adapter->status = WERNICKE_STATUS_SEMANTIC;

    /* Step 4: Syntactic parsing (if enabled) */
    if (adapter->config.enable_syntactic) {
        adapter->status = WERNICKE_STATUS_SYNTACTIC;
        /* Phase 5 will implement parsing */
    }

    adapter->status = WERNICKE_STATUS_COMPREHENSION_READY;
    adapter->stats.utterances_comprehended++;

    /* Fire callback if set */
    if (adapter->comprehension_callback) {
        adapter->comprehension_callback(result, adapter->comprehension_callback_data);
    }

    adapter->status = WERNICKE_STATUS_IDLE;
    return true;
}

bool wernicke_parse_sentence(
    wernicke_adapter_t* adapter,
    const wernicke_word_result_t* words,
    uint32_t count,
    wernicke_parse_t* parse)
{
    if (!adapter || !words || !parse) {
        if (adapter) adapter->last_error = WERNICKE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_parse_sentence: validation failed");
        return false;
    }

    /* Phase 5 will implement CKY/Earley parsing */
    parse->root = NULL;
    parse->is_valid = false;
    parse->parse_confidence = 0.0f;
    parse->semantic_representation = NULL;

    return true;
}

void wernicke_free_comprehension(wernicke_comprehension_t* result)
{
    if (!result) return;

    if (result->words) {
        nimcp_free(result->words);
        result->words = NULL;
    }
    if (result->concepts) {
        nimcp_free(result->concepts);
        result->concepts = NULL;
    }
    if (result->parse) {
        wernicke_free_parse(result->parse);
        result->parse = NULL;
    }
}

void wernicke_free_parse(wernicke_parse_t* parse)
{
    if (!parse) return;

    /* Recursively free parse tree nodes */
    if (parse->root) {
        /* Phase 5 will implement proper tree deallocation */
    }
    if (parse->semantic_representation) {
        nimcp_free(parse->semantic_representation);
    }
}

/*=============================================================================
 * CROSS-MODAL INTEGRATION
 *===========================================================================*/

bool wernicke_integrate_audiovisual(
    wernicke_adapter_t* adapter,
    const phoneme_event_t* audio_phonemes,
    const float* visual_lip_shapes,
    uint32_t num_frames,
    phoneme_event_t* fused_phonemes,
    uint32_t* num_fused)
{
    if (!adapter || !audio_phonemes || !fused_phonemes || !num_fused) {
        if (adapter) adapter->last_error = WERNICKE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_integrate_audiovisual: validation failed");
        return false;
    }

    /* Phase 4 will implement McGurk effect and audiovisual fusion */
    /* For now, just copy audio phonemes */
    memcpy(fused_phonemes, audio_phonemes, num_frames * sizeof(phoneme_event_t));
    *num_fused = num_frames;
    adapter->stats.audiovisual_fusions++;

    return true;
}

/*=============================================================================
 * BROCA CONNECTION
 *===========================================================================*/

bool wernicke_connect_broca(
    wernicke_adapter_t* adapter,
    broca_adapter_t* broca)
{
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_connect_broca: adapter is NULL");
        return false;
    }

    adapter->broca = broca;
    NIMCP_LOG_INFO("wernicke", "Connected to Broca's area (arcuate fasciculus)");
    return true;
}

bool wernicke_send_to_broca(
    wernicke_adapter_t* adapter,
    const wernicke_comprehension_t* comprehension)
{
    if (!adapter || !comprehension) {
        if (adapter) adapter->last_error = WERNICKE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_send_to_broca: validation failed");
        return false;
    }

    if (!adapter->broca) {
        adapter->last_error = WERNICKE_ERROR_BROCA_DISCONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_send_to_broca: adapter->broca is NULL");
        return false;
    }

    /* Phase 3 will implement bio-async message to Broca */
    NIMCP_LOG_DEBUG("wernicke", "Sent comprehension to Broca's area");
    return true;
}

bool wernicke_receive_efference_copy(
    wernicke_adapter_t* adapter,
    const broca_efference_copy_t* efference)
{
    if (!adapter || !efference) {
        if (adapter) adapter->last_error = WERNICKE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_receive_efference_copy: validation failed");
        return false;
    }

    /* Phase 3 will implement self-monitoring via efference copy comparison */
    NIMCP_LOG_DEBUG("wernicke", "Received efference copy from Broca (%u phonemes)",
                   efference->phoneme_count);
    return true;
}

/*=============================================================================
 * WORKING MEMORY
 *===========================================================================*/

bool wernicke_wm_store(
    wernicke_adapter_t* adapter,
    const phoneme_t* phonemes,
    uint32_t count)
{
    if (!adapter || !phonemes) {
        if (adapter) adapter->last_error = WERNICKE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_wm_store: validation failed");
        return false;
    }

    if (adapter->working_memory.count + count > adapter->working_memory.capacity) {
        adapter->last_error = WERNICKE_ERROR_WORKING_MEMORY_FULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_wm_store: validation failed");
        return false;
    }

    memcpy(&adapter->working_memory.buffer[adapter->working_memory.count],
           phonemes, count * sizeof(phoneme_t));
    adapter->working_memory.count += count;

    NIMCP_LOG_DEBUG("wernicke", "Stored %u phonemes in working memory (total=%u)",
                   count, adapter->working_memory.count);
    return true;
}

bool wernicke_wm_rehearse(wernicke_adapter_t* adapter)
{
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_wm_rehearse: adapter is NULL");
        return false;
    }

    /* Reset decay timer */
    /* Phase 3 will implement proper rehearsal with re-activation */
    adapter->working_memory.last_rehearsal_ms = 0; /* Would use current time */

    NIMCP_LOG_DEBUG("wernicke", "Rehearsed working memory (%u items)",
                   adapter->working_memory.count);
    return true;
}

bool wernicke_wm_get_contents(
    const wernicke_adapter_t* adapter,
    phoneme_t* phonemes,
    uint32_t max_count,
    uint32_t* count)
{
    if (!adapter || !phonemes || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_wm_get_contents: required parameter is NULL (adapter, phonemes, count)");
        return false;
    }

    uint32_t copy_count = adapter->working_memory.count;
    if (copy_count > max_count) {
        copy_count = max_count;
    }

    memcpy(phonemes, adapter->working_memory.buffer, copy_count * sizeof(phoneme_t));
    *count = copy_count;

    return true;
}

void wernicke_wm_clear(wernicke_adapter_t* adapter)
{
    if (!adapter) return;

    adapter->working_memory.count = 0;
    NIMCP_LOG_DEBUG("wernicke", "Cleared working memory");
}

/*=============================================================================
 * EVENT INTEGRATION
 *===========================================================================*/

bool wernicke_set_word_callback(
    wernicke_adapter_t* adapter,
    wernicke_word_callback_t callback,
    void* user_data)
{
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_set_word_callback: adapter is NULL");
        return false;
    }

    adapter->word_callback = callback;
    adapter->word_callback_data = user_data;
    return true;
}

bool wernicke_set_concept_callback(
    wernicke_adapter_t* adapter,
    wernicke_concept_callback_t callback,
    void* user_data)
{
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_set_concept_callback: adapter is NULL");
        return false;
    }

    adapter->concept_callback = callback;
    adapter->concept_callback_data = user_data;
    return true;
}

bool wernicke_set_comprehension_callback(
    wernicke_adapter_t* adapter,
    wernicke_comprehension_callback_t callback,
    void* user_data)
{
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_set_comprehension_callback: adapter is NULL");
        return false;
    }

    adapter->comprehension_callback = callback;
    adapter->comprehension_callback_data = user_data;
    return true;
}

bool wernicke_set_event_callback(
    wernicke_adapter_t* adapter,
    wernicke_event_callback_t callback,
    void* user_data)
{
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_set_event_callback: adapter is NULL");
        return false;
    }

    adapter->event_callback = callback;
    adapter->event_callback_data = user_data;
    return true;
}

/*=============================================================================
 * TRAINING INTERFACE
 *===========================================================================*/

bool wernicke_train_word(
    wernicke_adapter_t* adapter,
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    const char* target_word,
    float learning_rate)
{
    if (!adapter || !phonemes || !target_word) {
        if (adapter) adapter->last_error = WERNICKE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_train_word: validation failed");
        return false;
    }

    /* Phase 3 will implement contrastive learning */
    /* For now, just add the word to lexicon if not present */
    wernicke_word_t word;
    if (!wernicke_lookup_word(adapter, target_word, &word)) {
        memset(&word, 0, sizeof(word));
        word.word_id = adapter->lexicon_count + 1;
        strncpy(word.word, target_word, sizeof(word.word) - 1);
        word.word[sizeof(word.word) - 1] = '\0';

        if (num_phonemes > sizeof(word.phonemes)) {
            num_phonemes = sizeof(word.phonemes);
        }
        for (uint32_t i = 0; i < num_phonemes; i++) {
            word.phonemes[i] = (uint8_t)phonemes[i];
        }
        word.phoneme_count = num_phonemes;
        word.frequency = 0.5f;

        wernicke_add_word(adapter, &word);
    }

    adapter->stats.training_iterations++;
    return true;
}

bool wernicke_train_semantic(
    wernicke_adapter_t* adapter,
    uint32_t word_id,
    uint32_t concept_id,
    float strength)
{
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_train_semantic: adapter is NULL");
        return false;
    }

    /* Phase 3 will implement Hebbian semantic learning */
    adapter->stats.training_iterations++;
    return true;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

wernicke_status_t wernicke_get_status(const wernicke_adapter_t* adapter)
{
    if (!adapter) return WERNICKE_STATUS_ERROR;
    return adapter->status;
}

wernicke_error_t wernicke_get_last_error(const wernicke_adapter_t* adapter)
{
    if (!adapter) return WERNICKE_ERROR_INTERNAL;
    return adapter->last_error;
}

const char* wernicke_error_string(wernicke_error_t error)
{
    switch (error) {
        case WERNICKE_ERROR_NONE:               return "No error";
        case WERNICKE_ERROR_INVALID_INPUT:      return "Invalid input";
        case WERNICKE_ERROR_PHONOLOGICAL_FAILURE: return "Phonological processing failed";
        case WERNICKE_ERROR_LEXICAL_FAILURE:    return "Lexical access failed";
        case WERNICKE_ERROR_SEMANTIC_FAILURE:   return "Semantic integration failed";
        case WERNICKE_ERROR_SYNTACTIC_FAILURE:  return "Syntactic parsing failed";
        case WERNICKE_ERROR_WORKING_MEMORY_FULL: return "Working memory full";
        case WERNICKE_ERROR_WORD_NOT_FOUND:     return "Word not in lexicon";
        case WERNICKE_ERROR_CONCEPT_NOT_FOUND:  return "Concept not found";
        case WERNICKE_ERROR_BUFFER_OVERFLOW:    return "Buffer overflow";
        case WERNICKE_ERROR_BROCA_DISCONNECTED: return "Broca's area not connected";
        case WERNICKE_ERROR_INTERNAL:           return "Internal error";
        default:                                 return "Unknown error";
    }
}

const char* wernicke_status_string(wernicke_status_t status)
{
    switch (status) {
        case WERNICKE_STATUS_IDLE:               return "Idle";
        case WERNICKE_STATUS_PHONOLOGICAL:       return "Phonological analysis";
        case WERNICKE_STATUS_LEXICAL_ACCESS:     return "Lexical access";
        case WERNICKE_STATUS_SEMANTIC:           return "Semantic integration";
        case WERNICKE_STATUS_SYNTACTIC:          return "Syntactic parsing";
        case WERNICKE_STATUS_COMPREHENSION_READY: return "Comprehension ready";
        case WERNICKE_STATUS_ERROR:              return "Error";
        default:                                  return "Unknown status";
    }
}

bool wernicke_get_stats(const wernicke_adapter_t* adapter, wernicke_stats_t* stats)
{
    if (!adapter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_get_stats: required parameter is NULL (adapter, stats)");
        return false;
    }

    *stats = adapter->stats;
    return true;
}

bool wernicke_get_config(const wernicke_adapter_t* adapter, wernicke_config_t* config)
{
    if (!adapter || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_get_config: required parameter is NULL (adapter, config)");
        return false;
    }

    *config = adapter->config;
    return true;
}

/*=============================================================================
 * SUB-MODULE ACCESS
 *===========================================================================*/

phonological_analyzer_t* wernicke_get_phonological_analyzer(wernicke_adapter_t* adapter)
{
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->phonological;
}

lexical_access_t* wernicke_get_lexical_access(wernicke_adapter_t* adapter)
{
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->lexical;
}

semantic_integrator_t* wernicke_get_semantic_integrator(wernicke_adapter_t* adapter)
{
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->semantic;
}

syntactic_comprehension_t* wernicke_get_syntactic_comprehension(wernicke_adapter_t* adapter)
{
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->syntactic;
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

bio_module_context_t wernicke_get_bio_context(wernicke_adapter_t* adapter)
{
    if (!adapter || !adapter->bio_async_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_get_bio_context: required parameter is NULL (adapter, adapter->bio_async_enabled)");
        return NULL;
    }
    return adapter->bio_ctx;
}

uint32_t wernicke_process_bio_messages(wernicke_adapter_t* adapter, uint32_t max_messages)
{
    if (!adapter || !adapter->bio_async_enabled) return 0;

    /* Phase 3 will implement full bio-async message handling */
    return 0;
}

nimcp_bio_future_t wernicke_request_word_async(
    wernicke_adapter_t* adapter,
    const phoneme_t* phonemes,
    uint32_t count)
{
    (void)count;
    if (!adapter || !phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_request_word_async: required parameter is NULL (adapter, phonemes)");
        return NULL;
    }

    /* Phase 3 will implement async word recognition */
    return NULL;
}

nimcp_error_t wernicke_broadcast_word(
    wernicke_adapter_t* adapter,
    const wernicke_word_result_t* word)
{
    if (!adapter || !word) return NIMCP_ERROR_INVALID_PARAM;

    /* Phase 3 will implement bio-async broadcast */
    return NIMCP_SUCCESS;
}

nimcp_error_t wernicke_broadcast_comprehension(
    wernicke_adapter_t* adapter,
    const wernicke_comprehension_t* comprehension)
{
    if (!adapter || !comprehension) return NIMCP_ERROR_INVALID_PARAM;

    /* Phase 3 will implement bio-async broadcast */
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * KNOWLEDGE GRAPH INTEGRATION
 *===========================================================================*/

bool wernicke_register_kg(
    wernicke_adapter_t* adapter,
    brain_kg_t* kg)
{
    if (!adapter || !kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_register_kg: required parameter is NULL (adapter, kg)");
        return false;
    }

    adapter->kg = kg;

    /* Phase 3 will implement full KG registration:
     * - Create Wernicke node
     * - Add edges to Broca, Semantic Memory, Temporal
     */

    NIMCP_LOG_INFO("wernicke", "Registered with brain knowledge graph");
    return true;
}

bool wernicke_connect_semantic_memory(
    wernicke_adapter_t* adapter,
    semantic_memory_t* semantic)
{
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_connect_semantic_memory: adapter is NULL");
        return false;
    }

    adapter->semantic_memory = semantic;
    NIMCP_LOG_INFO("wernicke", "Connected to semantic memory");
    return true;
}

/*=============================================================================
 * SNN SUBSTRATE BINDING (wernicke_substrate population)
 *===========================================================================*/

bool wernicke_attach_snn_pop(wernicke_adapter_t* adapter,
                             snn_network_t* snn,
                             int pop_id)
{
    if (!adapter) {
        NIMCP_LOG_WARN("wernicke", "wernicke_attach_snn_pop: adapter is NULL");
        return false;
    }
    /* Negative pop id or NULL snn = unbind. Both legs MUST clear together so
     * callers never see (snn != NULL, id < 0). */
    if (!snn || pop_id < 0) {
        adapter->snn = NULL;
        adapter->snn_pop_id = -1;
        NIMCP_LOG_DEBUG("wernicke", "SNN substrate binding cleared");
        return true;
    }
    adapter->snn = snn;
    adapter->snn_pop_id = pop_id;
    NIMCP_LOG_INFO("wernicke", "Attached to SNN pop id=%d (wernicke_substrate)", pop_id);
    return true;
}

int wernicke_get_snn_pop_id(const wernicke_adapter_t* adapter)
{
    if (!adapter) return -1;
    return adapter->snn_pop_id;
}

snn_network_t* wernicke_get_snn_network(const wernicke_adapter_t* adapter)
{
    if (!adapter) return NULL;
    return adapter->snn;
}

bool wernicke_attach_grounded_language(wernicke_adapter_t* adapter, void* gl)
{
    if (!adapter) return false;
    adapter->grounded_lang = gl;       /* NULL = unbind */
    return true;
}

void* wernicke_get_grounded_language(const wernicke_adapter_t* adapter)
{
    if (!adapter) return NULL;
    return adapter->grounded_lang;
}
