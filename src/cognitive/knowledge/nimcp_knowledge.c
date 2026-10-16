//=============================================================================
// nimcp_knowledge.c - Multi-Domain Knowledge Acquisition Implementation
// REFACTORED: Using Strategy Pattern, Repository Pattern, and Search Indices
//=============================================================================

#include "cognitive/knowledge/nimcp_knowledge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/knowledge/nimcp_knowledge_snn_bridge.h"
#include "cognitive/knowledge/nimcp_knowledge_plasticity_bridge.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/brain/nimcp_brain.h"
/* W6-collision fix: W7's in-flight part_core.c uses brain->internal_kg
 * fields (from brain_struct) + brain_kg_* API but didn't include the
 * headers. Pulling them in here so the #include-style SRP build works.
 * Proper fix is for W7 to put these includes in part_core itself when
 * it gets committed. */
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "utils/memory/nimcp_memory.h"  // CRITICAL: Declares nimcp_calloc/nimcp_free return types
#include "utils/containers/nimcp_btree.h"

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/logging/nimcp_logging.h"
#include "nimcp.h"  // For error codes
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "knowledge"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(knowledge, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Constants
//=============================================================================

#define HASH_TABLE_SIZE 4096
#define MAX_CONCEPT_LENGTH 256
#define MAX_DEFINITION_LENGTH 1024
#define INITIAL_CAPACITY 10000
#define CONFIDENCE_INCREMENT 0.1f
#define CONFIDENCE_MAX 1.0f

//=============================================================================
// Internal Data Structures with Documented Invariants
//=============================================================================

/**
 * @brief Reading progress tracking structure
 *
 * Invariants:
 * - current_page <= total_pages
 * - comprehension_score in range [0.0, 1.0]
 * - book_title is null-terminated
 */
typedef struct {
    char book_title[NIMCP_ERROR_BUFFER_SIZE];
    uint32_t current_page;
    uint32_t total_pages;
    float comprehension_score;
} reading_progress_t;

/**
 * @brief Hash table entry for O(log n) concept_str lookup
 *
 * Implements chaining for collision resolution.
 * Invariants:
 * - index points to valid item in knowledge_system items array
 * - next is NULL or points to valid hash_entry_t
 */
typedef struct hash_entry_struct {
    char* concept;
    uint32_t index;
    struct hash_entry_struct* next;
} hash_entry_t;

/**
 * @brief Hash table for fast concept_str lookup
 *
 * Provides O(1) average case lookup instead of O(n) linear search.
 * Invariants:
 * - size equals HASH_TABLE_SIZE
 * - entries is array of HASH_TABLE_SIZE pointers
 */
typedef struct {
    hash_entry_t** entries;
    uint32_t size;
} knowledge_hash_table_t;

/**
 * @brief Strategy interface for domain-specific learning
 *
 * Strategy Pattern: Different learning strategies for different knowledge types
 * (narrative, art, history, etc.)
 */
typedef struct {
    const char* domain_name;
    bool (*learn)(void* system, const void* data);
    void (*optimize)(void* system);
    float (*assess)(const void* system);
} learning_strategy_t;

/**
 * @brief Repository for knowledge storage abstraction
 *
 * Repository Pattern: Abstracts storage implementation from business logic.
 * Invariants:
 * - num_items <= capacity
 * - index is synchronized with items array
 * - confidence_btree is synchronized with items array
 */
typedef struct {
    knowledge_item_t* items;
    uint32_t num_items;
    uint32_t capacity;
    knowledge_hash_table_t* index;       /* O(1) lookup by concept_str name */
    btree_t* confidence_btree;            /* O(log n) range queries by confidence */
} knowledge_repository_t;

/**
 * @brief Main knowledge system structure
 *
 * Invariants:
 * - All pointers are either NULL or point to valid allocated memory
 * - num_* counters are always <= corresponding *_capacity values
 * - domain_brains array has exactly 11 entries
 * - domain_stats array has exactly 11 entries
 */
struct knowledge_system_struct {
    char learner_name[NIMCP_LABEL_BUFFER_SIZE];

    knowledge_repository_t* repository;

    narrative_knowledge_t* narratives;
    uint32_t num_narratives;
    uint32_t narratives_capacity;

    aesthetic_knowledge_t* artworks;
    uint32_t num_artworks;
    uint32_t artworks_capacity;

    historical_knowledge_t* history;
    uint32_t num_history;
    uint32_t history_capacity;

    // REMOVED: brain_t domain_brains[11] - Never used, completely dead code

    reading_progress_t* reading_list;
    uint32_t num_reading;
    uint32_t reading_capacity;

    // Unified brain for knowledge system (provides curiosity module)
    // Previously created curiosity independently - now follows "one brain, many modules" pattern
    brain_t knowledge_brain;

    domain_knowledge_t domain_stats[11];

    learning_strategy_t* strategies[11];

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Bio-async registration status */

    // SNN and Plasticity bridges
    knowledge_snn_bridge_t* snn_bridge;           /**< SNN bridge for neural encoding */
    knowledge_plasticity_bridge_t* plasticity_bridge;  /**< Plasticity bridge for learning */
    bool bridges_enabled;                         /**< Bridges enabled flag */
};

//=============================================================================
// Text Processing Utilities - Single Pass O(n) Algorithms
//=============================================================================

// Minimum word length for meaningful concept_str extraction
#define MIN_CONCEPT_LENGTH 3

/**
 * @brief Check if word should be skipped during extraction
 *
 * Filters common words and short tokens that provide little semantic value.
 *
 * @param word Word to check (must be non-NULL)
 * @return true if word should be skipped
 *
 * Why: Reduces noise in concept_str extraction, focusing on meaningful terms.
 * Could be extended with proper stop word list.
 */

//=============================================================================
// BIO-ASYNC MESSAGE HANDLERS
//=============================================================================

/* Forward declaration of handler */
static nimcp_error_t handle_knowledge_query(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data);

/**
 * @brief KG-driven wiring callback for knowledge module
 */


// Forward declarations for static functions (SRP split)
static int compare_confidence(const char* key1, const char* key2);
static const char* extract_confidence_key(const void* data);
static void free_knowledge_item(void* data);
static uint32_t hash_concept(const char* concept_str);
static knowledge_hash_table_t* knowledge_hash_table_create(void);
static bool knowledge_hash_table_insert(knowledge_hash_table_t* table, const char* concept_str, uint32_t index);
static int32_t knowledge_hash_table_find(knowledge_hash_table_t* table, const char* concept_str);
static void knowledge_hash_table_destroy(knowledge_hash_table_t* table);
static knowledge_repository_t* repository_create(uint32_t initial_capacity);
static int32_t repository_find(knowledge_repository_t* repo, const char* concept_str);
static int32_t repository_add(knowledge_repository_t* repo, const knowledge_item_t* item);
static knowledge_item_t* repository_get(knowledge_repository_t* repo, uint32_t index);
static void repository_destroy(knowledge_repository_t* repo);
static bool should_skip_word(const char* word);
static uint32_t extract_concepts_optimized(const char* text, char concepts[][256], uint32_t max_concepts);
static void create_context_string(const char* text, char* output, uint32_t max_length);
static void normalize_concept_case(const char* concept_str, char* output, uint32_t max_length);
static char** deep_copy_string_array(char** src, uint32_t count);
static bool strategy_learn_narrative(void* system, const void* data);
static bool strategy_learn_aesthetic(void* system, const void* data);
static bool strategy_learn_historical(void* system, const void* data);
static float calculate_domain_confidence(knowledge_system_t system, knowledge_domain_t domain);
static void update_domain_stats(knowledge_system_t system, knowledge_domain_t domain);
static void initialize_domain_stats(domain_knowledge_t* stats, knowledge_domain_t domain);
static int knowledge_wiring_handler_callback( bio_module_context_t ctx, const bio_message_type_t* message_types, uint32_t message_count, void* user_data);
static void bio_broadcast_knowledge_update(knowledge_system_t system, uint32_t concepts_learned, knowledge_domain_t domain);
static void free_narratives(narrative_knowledge_t* narratives, uint32_t num_narratives);
static void free_artworks(aesthetic_knowledge_t* artworks, uint32_t num_artworks);
static void free_history(historical_knowledge_t* history, uint32_t num_history);
static bool process_concept(knowledge_system_t system, const char* concept_str, const char* text, knowledge_domain_t domain);
static bool is_cross_domain_related(const knowledge_item_t* item1, const knowledge_item_t* item2, const char* target_concept);
static atomic_formula_t* create_simple_atomic(const char* predicate, const char* arg1, const char* arg2);
static logic_clause_t* create_clause_from_atomic(atomic_formula_t* atom, float confidence);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_knowledge_part_helpers.c"  // 26 functions: helpers
#include "nimcp_knowledge_part_lifecycle.c"  // 6 functions: lifecycle
#include "nimcp_knowledge_part_stats.c"  // 2 functions: stats
#include "nimcp_knowledge_part_processing.c"  // 3 functions: processing
#include "nimcp_knowledge_part_core.c"  // 21 functions: core
#include "nimcp_knowledge_part_accessors.c"  // 6 functions: accessors
#include "nimcp_knowledge_part_io.c"  // 6 functions: io
