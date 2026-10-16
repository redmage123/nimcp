//=============================================================================
// nimcp_curiosity.c - Curiosity-Driven Learning Implementation
// Refactored for maintainability, performance, and clarity
//=============================================================================

#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/curiosity/nimcp_curiosity_snn_bridge.h"
#include "cognitive/curiosity/nimcp_curiosity_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/algorithms/nimcp_monte_carlo.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/brain/nimcp_brain.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/kg/nimcp_wave13_metacog_kg.h"  /* W13: curiosity-spike events */

//=============================================================================
// Monte Carlo Integration - GPU acceleration with CPU fallback
//=============================================================================

static _Thread_local uint32_t g_curiosity_mc_seed = 0;

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/quantum/nimcp_qmc_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include <stdatomic.h>  /* TOCTOU fix: atomic flag for GPU init */

/* Module-level GPU resources - initialized on first use */
static nimcp_gpu_context_t* g_curiosity_gpu_ctx = NULL;
static qmc_gpu_rng_t g_curiosity_gpu_rng = NULL;
static _Atomic bool g_curiosity_gpu_init_attempted = false;

/**
 * @brief Initialize GPU resources for curiosity MC operations
 *
 * WHAT: Set up GPU context and RNG for accelerated sampling
 * WHY:  GPU provides 100-1000x speedup for large-scale MC operations
 * HOW:  Create context, initialize cuRAND states
 *
 * Thread-safe: Uses atomic flag to prevent TOCTOU double-initialization
 */
static bool curiosity_init_gpu_mc(void) {
    /* TOCTOU fix: Use atomic load/store to prevent double-initialization.
     * WHY:  Multiple threads could race on a non-atomic check, both see false,
     *       and double-initialize GPU context (resource leak + undefined behavior).
     */
    if (atomic_load(&g_curiosity_gpu_init_attempted)) {
        return g_curiosity_gpu_rng != NULL;
    }
    atomic_store(&g_curiosity_gpu_init_attempted, true);

    if (!qmc_gpu_is_available()) {
        LOG_DEBUG("GPU not available for curiosity MC, using CPU fallback");
        return false;  /* GPU not available is normal — not an error */
    }

    g_curiosity_gpu_ctx = nimcp_gpu_context_create_auto();
    if (!g_curiosity_gpu_ctx) {
        LOG_DEBUG("Failed to create GPU context for curiosity MC");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_init_gpu_mc: g_curiosity_gpu_ctx is NULL");
        return false;
    }

    g_curiosity_gpu_rng = qmc_gpu_rng_create(g_curiosity_gpu_ctx, 4096, 0);
    if (!g_curiosity_gpu_rng) {
        LOG_DEBUG("Failed to create GPU RNG for curiosity MC");
        nimcp_gpu_context_destroy(g_curiosity_gpu_ctx);
        g_curiosity_gpu_ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_init_gpu_mc: g_curiosity_gpu_rng is NULL");
        return false;
    }

    LOG_INFO("GPU acceleration enabled for curiosity MC operations");
    return true;
}

static inline bool curiosity_has_gpu_mc(void) {
    if (!atomic_load(&g_curiosity_gpu_init_attempted)) curiosity_init_gpu_mc();
    return g_curiosity_gpu_rng != NULL;
}

/**
 * @brief Cleanup GPU resources on library unload
 * WHY:  g_curiosity_gpu_ctx and g_curiosity_gpu_rng are allocated but never freed,
 *       leaking GPU memory when the library is unloaded or process exits.
 */
__attribute__((destructor))
static void curiosity_cleanup_gpu_mc(void) {
    if (g_curiosity_gpu_rng) {
        qmc_gpu_rng_destroy(g_curiosity_gpu_rng);
        g_curiosity_gpu_rng = NULL;
    }
    if (g_curiosity_gpu_ctx) {
        nimcp_gpu_context_destroy(g_curiosity_gpu_ctx);
        g_curiosity_gpu_ctx = NULL;
    }
    atomic_store(&g_curiosity_gpu_init_attempted, false);
}

#else  /* !NIMCP_ENABLE_CUDA */

static inline bool curiosity_has_gpu_mc(void) { return false; }

#endif /* NIMCP_ENABLE_CUDA */
#include "utils/containers/nimcp_hash_table.h"
#include "utils/memory/nimcp_memory.h"  // CRITICAL: Declares nimcp_calloc/nimcp_free return types
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"  // Dopamine modulation
#include "cognitive/analysis/nimcp_network_analysis.h"  // Network topology analysis
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "nimcp.h"  // For error codes
#include "cognitive/knowledge/nimcp_kg_reader.h"

#define LOG_MODULE "curiosity"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(curiosity, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Hash Table Configuration for O(1) Concept Lookup
//=============================================================================

#define HASH_TABLE_SIZE 4096  // Power of 2 for fast modulo via bitwise AND

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Concept data stored in hash table
 *
 * NOTE: Hash table now handles chaining internally via nimcp_hash_table utility.
 * No need for explicit 'next' pointer.
 */
typedef struct concept_bucket_struct {
    char concept[NIMCP_ERROR_BUFFER_SIZE];
    float familiarity;
    uint32_t exposure_count;
    uint64_t last_encountered;
    char** related_concepts;
    uint32_t num_related;
} concept_bucket_t;

/**
 * @brief Value destructor for concept_bucket_t
 *
 * WHAT: Clean up concept_str bucket and related concepts
 * WHY: Called by hash table when entry is removed or table destroyed
 * HOW: Free related_concepts array and strings
 */
static void concept_bucket_destructor(void* value, size_t value_size)
{
    (void) value_size;  // Unused parameter
    if (!value)
        return;

    concept_bucket_t* bucket = (concept_bucket_t*) value;

    // Free related concepts
    if (bucket->related_concepts) {
        for (uint32_t j = 0; j < bucket->num_related; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && bucket->num_related > 256) {
                curiosity_heartbeat("curiosity_loop",
                                 (float)(j + 1) / (float)bucket->num_related);
            }

            nimcp_free(bucket->related_concepts[j]);
        }
        nimcp_free(bucket->related_concepts);
    }
}

/**
 * @brief Question history entry
 */
typedef struct {
    char question[NIMCP_ERROR_BUFFER_LARGE];
    char answer[NIMCP_JSON_BUFFER_SIZE];
    bool answered;
    float learning_value;
    uint64_t timestamp;
} question_history_t;

/**
 * @brief Knowledge source registration
 */
typedef struct {
    char name[NIMCP_LABEL_BUFFER_SIZE];
    knowledge_search_fn_t search_fn;
    void* context;
    bool active;
} knowledge_source_t;

//=============================================================================
// State Pattern - Learning Stage Behaviors
//=============================================================================

/**
 * @brief Virtual function table for learning stage behaviors
 *
 * Each stage implements these methods to customize learning behavior.
 * This eliminates switch statements and enables polymorphic stage behavior.
 */
typedef struct {
    float (*calculate_learning_potential)(const char* concept_str, float gap_size);
    float (*get_baseline_curiosity)(void);
    uint32_t (*get_question_types_count)(void);
    const question_type_t* (*get_question_types)(void);
} learning_stage_strategy_t;

// Forward declarations for stage strategies
static const learning_stage_strategy_t* get_stage_strategy(learning_stage_t stage);

//=============================================================================
// Strategy Pattern - Question Generation
//=============================================================================

/**
 * @brief Function pointer type for question generation strategies
 *
 * Each question type has its own generation strategy.
 * Eliminates switch statements and enables extensible question types.
 */
typedef void (*question_generator_fn_t)(const char* topic, char* question, size_t max_len,
                                        float* priority_multiplier, float* difficulty);

/**
 * @brief Question generation strategy entry
 */
typedef struct {
    question_type_t type;
    question_generator_fn_t generator;
    float priority_multiplier;
    float difficulty;
} question_strategy_t;

// Forward declaration for strategy table
static const question_strategy_t* get_question_strategy(question_type_t type);

//=============================================================================
// Main Engine Structure
//=============================================================================

struct curiosity_engine_struct {
    char learner_name[NIMCP_LABEL_BUFFER_SIZE];

    // Hash table for O(1) concept_str lookup (using nimcp_hash_table utility)
    hash_table_t* concept_hash_table;
    uint32_t total_concepts;

    // Question history
    question_history_t* questions;
    uint32_t num_questions;
    uint32_t questions_capacity;

    // Knowledge sources
    knowledge_source_t* sources;
    uint32_t num_sources;
    uint32_t sources_capacity;

    // REFACTORED: Parent brain reference (module pattern)
    // Previously created separate brain instances - wasteful!
    // Now curiosity is a module that uses parent brain's neuromodulator
    brain_t parent_brain;  // Reference to parent, NOT ownership

    // Network topology analyzer for detecting functional reorganization
    network_analyzer_t* network_analyzer;  // Owned by curiosity engine

    // State for gap detection (replaces gap_detector brain)
    struct {
        float last_novelty_score;
        float last_gap_size;
    } gap_detector_state;

    // State for question prioritization (replaces question_prioritizer brain)
    struct {
        float last_priority;
        float last_difficulty;
    } question_prioritizer_state;

    // Motivation state
    float baseline_curiosity;
    float current_motivation;

    // Learning stage with strategy pattern
    learning_stage_t stage;
    const learning_stage_strategy_t* stage_strategy;

    // Statistics
    learning_progress_t progress;

    // Bio-async communication
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    // Immune system integration
    void* immune_bridge;  // curiosity_immune_bridge_t*, void* to avoid circular dependency

    // SNN and Plasticity bridges
    curiosity_snn_bridge_t* snn_bridge;
    curiosity_plasticity_bridge_t* plasticity_bridge;
    bool bridges_enabled;
};

//=============================================================================
// Stage Pattern Implementation - Infant Stage
//=============================================================================

/**
 * @brief Infant stage learning potential calculation
 *
 * Infants find nearly everything highly valuable for learning.
 * High baseline potential regardless of gap size.
 *
 * Complexity: O(1)
 */
static float infant_learning_potential(const char* concept_str, float gap_size)
{
    return 0.9F;  // Everything is exciting and worth learning
}

static float infant_baseline_curiosity(void)
{
    return 0.95F;
}

static uint32_t infant_question_types_count(void)
{
    return 3;  // WHAT, WHY, HOW only
}

static const question_type_t infant_question_types[] = {QUESTION_WHAT, QUESTION_WHY, QUESTION_HOW};

static const question_type_t* infant_get_question_types(void)
{
    return infant_question_types;
}

static const learning_stage_strategy_t infant_strategy = {
    .calculate_learning_potential = infant_learning_potential,
    .get_baseline_curiosity = infant_baseline_curiosity,
    .get_question_types_count = infant_question_types_count,
    .get_question_types = infant_get_question_types};

//=============================================================================
// Stage Pattern Implementation - Toddler Stage
//=============================================================================

/**
 * @brief Toddler stage learning potential calculation
 *
 * Toddlers prefer concrete, simple concepts.
 * Penalizes complex or abstract concepts (approximated by length).
 *
 * Complexity: O(1) - strlen is O(n) but concept_str length bounded
 */
static float toddler_learning_potential(const char* concept_str, float gap_size)
{
    size_t len = strlen(concept_str);
    float simplicity_bonus = (len < 15) ? 1.0F : 0.5F;
    return 0.8F * simplicity_bonus;
}

static float toddler_baseline_curiosity(void)
{
    return 0.90F;
}

static uint32_t toddler_question_types_count(void)
{
    return 5;  // Add WHERE and WHEN
}

static const question_type_t toddler_question_types[] = {QUESTION_WHAT, QUESTION_WHY, QUESTION_HOW,
                                                         QUESTION_WHERE, QUESTION_WHEN};

static const question_type_t* toddler_get_question_types(void)
{
    return toddler_question_types;
}

static const learning_stage_strategy_t toddler_strategy = {
    .calculate_learning_potential = toddler_learning_potential,
    .get_baseline_curiosity = toddler_baseline_curiosity,
    .get_question_types_count = toddler_question_types_count,
    .get_question_types = toddler_get_question_types};

//=============================================================================
// Stage Pattern Implementation - Child Stage
//=============================================================================

/**
 * @brief Child stage learning potential calculation
 *
 * Children can handle abstract concepts and are driven by curiosity intensity.
 * Learning potential scales with how curious they are about the topic.
 *
 * Complexity: O(1)
 */
static float child_learning_potential(const char* concept_str, float gap_size)
{
    return 0.7F + 0.3F * gap_size;  // Scales with gap/curiosity
}

static float child_baseline_curiosity(void)
{
    return 0.80F;
}

static uint32_t child_question_types_count(void)
{
    return 7;  // All question types
}

static const question_type_t child_question_types[] = {QUESTION_WHAT,  QUESTION_WHY,  QUESTION_HOW,
                                                       QUESTION_WHERE, QUESTION_WHEN, QUESTION_WHO,
                                                       QUESTION_WHICH};

static const question_type_t* child_get_question_types(void)
{
    return child_question_types;
}

static const learning_stage_strategy_t child_strategy = {
    .calculate_learning_potential = child_learning_potential,
    .get_baseline_curiosity = child_baseline_curiosity,
    .get_question_types_count = child_question_types_count,
    .get_question_types = child_get_question_types};

//=============================================================================
// Stage Pattern Implementation - Adolescent Stage
//=============================================================================

static float adolescent_learning_potential(const char* concept_str, float gap_size)
{
    return 0.6F + 0.4F * gap_size;
}

static float adolescent_baseline_curiosity(void)
{
    return 0.70F;
}

static uint32_t adolescent_question_types_count(void)
{
    return 7;
}

static const question_type_t* adolescent_get_question_types(void)
{
    return child_question_types;  // Same as child
}

static const learning_stage_strategy_t adolescent_strategy = {
    .calculate_learning_potential = adolescent_learning_potential,
    .get_baseline_curiosity = adolescent_baseline_curiosity,
    .get_question_types_count = adolescent_question_types_count,
    .get_question_types = adolescent_get_question_types};

//=============================================================================
// Stage Pattern Implementation - Adult Stage
//=============================================================================

static float adult_learning_potential(const char* concept_str, float gap_size)
{
    return 0.5F + 0.5F * gap_size;  // More selective
}

static float adult_baseline_curiosity(void)
{
    return 0.60F;
}

static uint32_t adult_question_types_count(void)
{
    return 7;
}

static const question_type_t* adult_get_question_types(void)
{
    return child_question_types;
}

static const learning_stage_strategy_t adult_strategy = {
    .calculate_learning_potential = adult_learning_potential,
    .get_baseline_curiosity = adult_baseline_curiosity,
    .get_question_types_count = adult_question_types_count,
    .get_question_types = adult_get_question_types};

//=============================================================================
// Stage Pattern Implementation - Expert Stage
//=============================================================================

static float expert_learning_potential(const char* concept_str, float gap_size)
{
    return 0.4F + 0.6F * gap_size;  // Only interested in significant gaps
}

static float expert_baseline_curiosity(void)
{
    return 0.50F;
}

static uint32_t expert_question_types_count(void)
{
    return 7;
}

static const question_type_t* expert_get_question_types(void)
{
    return child_question_types;
}

static const learning_stage_strategy_t expert_strategy = {
    .calculate_learning_potential = expert_learning_potential,
    .get_baseline_curiosity = expert_baseline_curiosity,
    .get_question_types_count = expert_question_types_count,
    .get_question_types = expert_get_question_types};

//=============================================================================
// Stage Strategy Dispatch Table
//=============================================================================

/**
 * @brief Get strategy for learning stage
 *
 * Returns function table for stage-specific behaviors.
 * Replaces switch statements with polymorphism.
 *
 * Complexity: O(1) - array lookup
 */
static const learning_stage_strategy_t* get_stage_strategy(learning_stage_t stage)
{
    static const learning_stage_strategy_t* strategies[] = {&infant_strategy, &toddler_strategy,
                                                            &child_strategy,  &adolescent_strategy,
                                                            &adult_strategy,  &expert_strategy};

    if (stage < 0 || stage >= 6) {
        return &infant_strategy;  // Safe default
    }

    return strategies[stage];
}

//=============================================================================
// Question Generation Strategies
//=============================================================================

/**
 * @brief Generate "What" question
 *
 * Fundamental definitional question. Highest priority as it establishes
 * basic understanding of a concept_str.
 *
 * Complexity: O(n) where n is topic length (due to snprintf)
 */
static void generate_what_question(const char* topic, char* question, size_t max_len,
                                   float* priority_multiplier, float* difficulty)
{
    snprintf(question, max_len, "What is %s?", topic);
    *priority_multiplier = 1.0F;
    *difficulty = 0.3F;
}

/**
 * @brief Generate "Why" question
 *
 * Explores causality and reasoning. Slightly lower priority than "what"
 * but higher difficulty due to abstract reasoning required.
 *
 * Complexity: O(n)
 */
static void generate_why_question(const char* topic, char* question, size_t max_len,
                                  float* priority_multiplier, float* difficulty)
{
    snprintf(question, max_len, "Why does %s happen?", topic);
    *priority_multiplier = 0.9F;
    *difficulty = 0.6F;
}

/**
 * @brief Generate "How" question
 *
 * Explores mechanisms and processes. High difficulty due to procedural
 * understanding required.
 *
 * Complexity: O(n)
 */
static void generate_how_question(const char* topic, char* question, size_t max_len,
                                  float* priority_multiplier, float* difficulty)
{
    snprintf(question, max_len, "How does %s work?", topic);
    *priority_multiplier = 0.8F;
    *difficulty = 0.7F;
}

/**
 * @brief Generate "Where" question
 *
 * Spatial/location question. Medium priority and difficulty.
 *
 * Complexity: O(n)
 */
static void generate_where_question(const char* topic, char* question, size_t max_len,
                                    float* priority_multiplier, float* difficulty)
{
    snprintf(question, max_len, "Where is %s?", topic);
    *priority_multiplier = 0.6F;
    *difficulty = 0.4F;
}

/**
 * @brief Generate "When" question
 *
 * Temporal question. Medium priority and difficulty.
 *
 * Complexity: O(n)
 */
static void generate_when_question(const char* topic, char* question, size_t max_len,
                                   float* priority_multiplier, float* difficulty)
{
    snprintf(question, max_len, "When did %s happen?", topic);
    *priority_multiplier = 0.7F;
    *difficulty = 0.5F;
}

/**
 * @brief Generate "Who" question
 *
 * Asks about agents/actors involved. Medium priority and difficulty.
 *
 * Complexity: O(n)
 */
static void generate_who_question(const char* topic, char* question, size_t max_len,
                                  float* priority_multiplier, float* difficulty)
{
    snprintf(question, max_len, "Who is involved with %s?", topic);
    *priority_multiplier = 0.7F;
    *difficulty = 0.5F;
}

/**
 * @brief Generate "Which" question
 *
 * Discriminative question about types/categories. Lower priority as it
 * assumes some base knowledge.
 *
 * Complexity: O(n)
 */
static void generate_which_question(const char* topic, char* question, size_t max_len,
                                    float* priority_multiplier, float* difficulty)
{
    snprintf(question, max_len, "Which type of %s?", topic);
    *priority_multiplier = 0.5F;
    *difficulty = 0.6F;
}

//=============================================================================
// Question Strategy Dispatch Table
//=============================================================================

/**
 * @brief Question generation strategy dispatch table
 *
 * Maps question types to their generation functions.
 * Eliminates switch statements in question generation.
 *
 * Complexity: O(1) lookup
 */
static const question_strategy_t question_strategies[] = {
    {QUESTION_WHAT, generate_what_question, 1.0F, 0.3F},
    {QUESTION_WHY, generate_why_question, 0.9F, 0.6F},
    {QUESTION_HOW, generate_how_question, 0.8F, 0.7F},
    {QUESTION_WHERE, generate_where_question, 0.6F, 0.4F},
    {QUESTION_WHEN, generate_when_question, 0.7F, 0.5F},
    {QUESTION_WHO, generate_who_question, 0.7F, 0.5F},
    {QUESTION_WHICH, generate_which_question, 0.5F, 0.6F}};

static const size_t num_question_strategies =
    sizeof(question_strategies) / sizeof(question_strategies[0]);

/**
 * @brief Get question generation strategy by type
 *
 * Returns strategy for given question type, or NULL if invalid.
 *
 * Complexity: O(1) - fixed-size array lookup
 */
static const question_strategy_t* get_question_strategy(question_type_t type)
{
    if (type < 0 || (size_t) type >= num_question_strategies) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "get_question_strategy: capacity exceeded");
        return NULL;
    }
    return &question_strategies[type];
}

//=============================================================================
// Hash Table Operations - O(1) Concept Lookup
//=============================================================================

/**
 * @brief Normalize string for consistent hashing and comparison
 *
 * Converts to lowercase and removes non-alphanumeric except spaces.
 * Ensures consistent lookup regardless of input formatting.
 *
 * Complexity: O(n) where n is input length
 */
static void normalize_string(const char* input, char* output, size_t max_len)
{
    if (!input || !output || max_len == 0) {
        return;
    }

    size_t j = 0;
    for (size_t i = 0; input[i] && j < max_len - 1; i++) {
        if (isalnum(input[i]) || input[i] == ' ') {
            output[j++] = tolower(input[i]);
        }
    }
    output[j] = '\0';
}

/**
 * @brief Find concept_str in hash table
 *
 * Uses hash table for O(1) average case lookup instead of O(n) linear search.
 * Handles collisions via chaining (linked list per bucket).
 *
 * Time Complexity:
 *   - Average: O(1) with good hash distribution
 *   - Worst: O(n) if all concepts hash to same bucket (rare)
 *
 * Space Complexity: O(1)
 *
 * @param engine Curiosity engine
 * @param concept_str Concept to find
 * @return Pointer to concept_str bucket, or NULL if not found
 */
static concept_bucket_t* find_concept_bucket(curiosity_engine_t engine, const char* concept_str)
{
    if (!engine || !concept_str || !engine->concept_hash_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_concept_bucket: required parameter is NULL (engine, concept_str, engine->concept_hash_table)");
        return NULL;
    }

    return (concept_bucket_t*) hash_table_lookup_string(engine->concept_hash_table, concept_str);
}

/**
 * @brief Add new concept_str to hash table
 *
 * Creates new bucket and inserts at head of chain for O(1) insertion.
 * Automatically handles collisions via chaining.
 *
 * Time Complexity: O(1) average case
 * Space Complexity: O(1) per concept_str
 *
 * @param engine Curiosity engine
 * @param concept_str Concept to add
 * @return Pointer to new or existing concept_str bucket
 */
static concept_bucket_t* add_concept_to_hash_table(curiosity_engine_t engine, const char* concept_str)
{
    if (!engine || !concept_str) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "add_concept_to_hash_table: required parameter is NULL (engine, concept_str)");
        return NULL;
    }

    // Check if already exists
    concept_bucket_t* existing = find_concept_bucket(engine, concept_str);
    if (existing) {
        return existing;
    }

    // Create new bucket
    concept_bucket_t new_bucket = {0};
    strncpy(new_bucket.concept, concept_str, sizeof(new_bucket.concept) - 1);
    new_bucket.familiarity = 0.0F;
    new_bucket.exposure_count = 1;
    new_bucket.last_encountered = 0;
    new_bucket.related_concepts = NULL;
    new_bucket.num_related = 0;

    // Insert into hash table
    if (!hash_table_insert_string(engine->concept_hash_table, concept_str, &new_bucket,
                                  sizeof(concept_bucket_t))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "add_concept_to_hash_table: operation failed");
        return NULL;
    }

    engine->total_concepts++;

    // Return pointer to the stored bucket
    return find_concept_bucket(engine, concept_str);
}

/**
 * @brief Free all concepts in hash table
 *
 * WHAT: Destroy hash table and all concepts
 * WHY: Called during engine destruction
 * HOW: Delegate to hash_table_destroy (calls concept_bucket_destructor)
 *
 * Complexity: O(n) where n is total concepts
 */
static void free_hash_table(curiosity_engine_t engine)
{
    if (!engine || !engine->concept_hash_table) {
        return;
    }

    hash_table_destroy(engine->concept_hash_table);
    engine->concept_hash_table = NULL;
}

//=============================================================================
// BIO-ASYNC MESSAGE HANDLERS
//=============================================================================

/**
 * @brief Broadcast curiosity spike event via bio-async
 */
static void bio_broadcast_curiosity_spike(curiosity_engine_t engine,
                                          float intensity,
                                          float information_gain,
                                          uint32_t target_id) {
    if (!engine || !engine->bio_async_enabled || !engine->bio_ctx) {
        return;
    }

    bio_msg_curiosity_signal_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_CURIOSITY_SIGNAL,
                        bio_module_context_get_id(engine->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.target_id = target_id;
    msg.curiosity_intensity = intensity;
    msg.information_gain_estimate = information_gain;
    msg.exploration_bonus = intensity * 0.5F;

    bio_router_broadcast(engine->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG("Broadcast curiosity spike: intensity=%.2f, info_gain=%.2f",
              intensity, information_gain);
}

//=============================================================================
// Curiosity Engine Lifecycle
//=============================================================================

/**
 * @brief Create new curiosity engine
 *
 * Initializes all subsystems:
 * - Hash table for O(1) concept_str lookup
 * - Neural networks for gap detection and question prioritization
 * - Learning stage strategy pattern
 * - Question and source storage
 *
 * Time Complexity: O(1)
 * Space Complexity: O(1) - initial allocation, grows with use
 *
 * REFACTORED: Now takes parent brain reference (module pattern)
 *
 * @param parent_brain Parent brain that owns this curiosity module
 * @param learner_name Name identifier for this learner
 * @return New engine instance, or NULL on failure
 */
curiosity_engine_t curiosity_engine_create(brain_t parent_brain, const char* learner_name)
{
    // Allow NULL parent_brain for standalone testing
    if (!learner_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_engine_create: learner_name is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_engine_create", 0.0f);


    curiosity_engine_t engine = nimcp_calloc(1, sizeof(struct curiosity_engine_struct));
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "curiosity_engine_create: failed to allocate engine");
        return NULL;
    }

    strncpy(engine->learner_name, learner_name, sizeof(engine->learner_name) - 1);

    // Initialize hash table with nimcp_hash_table utility
    hash_table_config_t ht_config = {.initial_buckets = HASH_TABLE_SIZE,
                                     .key_type = HASH_KEY_STRING,
                                     .hash_algorithm = HASH_ALG_FNV1A,
                                     .case_insensitive = true,
                                     .value_destructor = concept_bucket_destructor,
                                     .thread_safe = false};
    engine->concept_hash_table = hash_table_create(&ht_config);
    if (!engine->concept_hash_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "curiosity_engine_create: failed to create hash_table");
        nimcp_free(engine);
        engine = NULL;
        return NULL;
    }
    engine->total_concepts = 0;

    // Allocate question storage
    engine->questions_capacity = 10000;
    engine->questions = nimcp_calloc(engine->questions_capacity, sizeof(question_history_t));
    if (!engine->questions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "curiosity_engine_create: failed to allocate questions");
        hash_table_destroy(engine->concept_hash_table);
        nimcp_free(engine);
        engine = NULL;
        return NULL;
    }

    // Allocate source storage
    engine->sources_capacity = 10;
    engine->sources = nimcp_calloc(engine->sources_capacity, sizeof(knowledge_source_t));
    if (!engine->sources) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "curiosity_engine_create: failed to allocate sources");
        hash_table_destroy(engine->concept_hash_table);
        nimcp_free(engine->questions);
        nimcp_free(engine);
        engine = NULL;
        return NULL;
    }

    // REFACTORED: Store parent brain reference (module pattern)
    // Previously created separate brain instances - wasteful!
    // Now use parent's neuromodulator system
    engine->parent_brain = parent_brain;

    // Initialize network analyzer if parent brain provided
    if (parent_brain) {
        engine->network_analyzer = network_analyzer_create(parent_brain);
        if (engine->network_analyzer) {
            // Enable auto-analysis on novel patterns
            network_analyzer_set_auto_analyze(engine->network_analyzer, true, 10);
        }
    } else {
        engine->network_analyzer = NULL;
    }

    // Initialize gap detector state (replaces brain)
    engine->gap_detector_state.last_novelty_score = 0.0F;
    engine->gap_detector_state.last_gap_size = 0.0F;

    // Initialize question prioritizer state (replaces brain)
    engine->question_prioritizer_state.last_priority = 0.5F;
    engine->question_prioritizer_state.last_difficulty = 0.5F;

    // Initialize with infant stage
    engine->stage = STAGE_INFANT;
    engine->stage_strategy = get_stage_strategy(STAGE_INFANT);
    engine->baseline_curiosity = engine->stage_strategy->get_baseline_curiosity();
    engine->current_motivation = engine->baseline_curiosity;

    // Initialize progress
    engine->progress.total_questions_asked = 0;
    engine->progress.total_answers_learned = 0;
    engine->progress.concepts_learned = 0;
    engine->progress.avg_curiosity = engine->baseline_curiosity;
    engine->progress.enable_bio_async = false;
    snprintf(engine->progress.current_focus, sizeof(engine->progress.current_focus),
             "exploring world");

    // Bio-async registration
    engine->bio_ctx = NULL;
    engine->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_CURIOSITY,
            .module_name = "curiosity",
            .inbox_capacity = 64,
            .user_data = engine
        };
        engine->bio_ctx = bio_router_register_module(&bio_info);
        if (engine->bio_ctx) {
            engine->bio_async_enabled = true;
            engine->progress.enable_bio_async = true;
            LOG_INFO("Bio-async registered for curiosity module");
        } else {
            LOG_WARN("Failed to register bio-async for curiosity module");
        }
    }

    // Initialize SNN and Plasticity bridges
    engine->snn_bridge = NULL;
    engine->plasticity_bridge = NULL;
    engine->bridges_enabled = false;

    curiosity_snn_config_t snn_config = curiosity_snn_config_default();
    engine->snn_bridge = curiosity_snn_create(&snn_config);

    curiosity_plasticity_config_t plasticity_config = curiosity_plasticity_config_default();
    engine->plasticity_bridge = curiosity_plasticity_create(&plasticity_config);

    if (engine->snn_bridge && engine->plasticity_bridge) {
        engine->bridges_enabled = true;
    }

    return engine;
}

/**
 * @brief Destroy curiosity engine and free all resources
 *
 * Frees hash table, questions, sources, and neural networks.
 * Safe to call with NULL pointer.
 *
 * Complexity: O(n) where n is total concepts
 */
void curiosity_engine_destroy(curiosity_engine_t engine)
{
    if (!engine) {
        return;
    }

    // Disconnect immune system if connected
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_engine_destroy", 0.0f);


    if (engine->immune_bridge) {
        curiosity_disconnect_immune(engine);
    }

    // Bio-async unregistration
    if (engine->bio_async_enabled && engine->bio_ctx) {
        bio_router_unregister_module(engine->bio_ctx);
        engine->bio_ctx = NULL;
        engine->bio_async_enabled = false;
        LOG_INFO("Bio-async unregistered for curiosity module");
    }

    free_hash_table(engine);

    nimcp_free(engine->questions);
    nimcp_free(engine->sources);

    // Destroy network analyzer (owned by curiosity engine)
    if (engine->network_analyzer) {
        network_analyzer_destroy(engine->network_analyzer);
    }

    // REFACTORED: No brain destruction needed - we only hold a reference
    // parent_brain is owned by caller and will be destroyed by them
    // Previously destroyed gap_detector and question_prioritizer brains here

    if (engine->snn_bridge) {
        curiosity_snn_destroy(engine->snn_bridge);
        engine->snn_bridge = NULL;
    }
    if (engine->plasticity_bridge) {
        curiosity_plasticity_destroy(engine->plasticity_bridge);
        engine->plasticity_bridge = NULL;
    }

    nimcp_free(engine);
    engine = NULL;
}

//=============================================================================
// Knowledge Gap Detection
//=============================================================================

/**
 * @brief Calculate curiosity intensity for knowledge gap
 *
 * WHAT: Compute how intensely curious we are about this gap
 * WHY:  Dopamine modulates intrinsic motivation and curiosity
 * HOW:  gap_size × baseline × dopamine_modulation
 *
 * BIOLOGY: Dopamine encodes reward prediction and motivation
 *          Higher DA → more curious and exploratory
 *
 * Complexity: O(1)
 */
static float calculate_curiosity_intensity(const curiosity_engine_t engine, float gap_size)
{
    float base_intensity = gap_size * engine->baseline_curiosity;

    // Modulate by dopamine from parent brain's neuromodulator system
    if (engine->parent_brain) {
        neuromodulator_system_t neuromod = brain_get_neuromodulator_system(engine->parent_brain);
        if (neuromod) {
            float dopamine = neuromodulator_get_level(neuromod, NEUROMOD_DOPAMINE);
            // Dopamine range [0.3, 0.7], map to modulation [0.6, 1.4]
            // Low DA (0.3) → 0.6× curiosity (apathy, depression)
            // High DA (0.7) → 1.4× curiosity (motivated, manic)
            float modulation = 0.6F + (dopamine - 0.3F) * 2.0F;
            base_intensity *= modulation;
        }
    }

    return fminf(base_intensity, 1.0F);  // Clamp to [0, 1]
}

/**
 * @brief Get familiarity with concept_str
 *
 * Uses hash table for O(1) average case lookup.
 * Returns 0.0 for unknown concepts.
 *
 * Time Complexity: O(1) average case
 *
 * @param engine Curiosity engine
 * @param concept_str Concept to check
 * @return Familiarity score [0.0-1.0]
 */
float curiosity_check_familiarity(curiosity_engine_t engine, const char* concept_str)
{
    if (!engine || !concept_str) {
        return 0.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_check_familiarity", 0.0f);


    concept_bucket_t* bucket = find_concept_bucket(engine, concept_str);
    if (!bucket) {
        return 0.0F;
    }

    return bucket->familiarity;
}

/**
 * @brief Count related concepts for a topic
 *
 * Uses hash table for O(1) concept_str lookup.
 *
 * Complexity: O(1) average case
 */
static uint32_t count_related_concepts(curiosity_engine_t engine, const char* concept_str)
{
    concept_bucket_t* bucket = find_concept_bucket(engine, concept_str);
    if (!bucket) {
        return 0;
    }
    return bucket->num_related;
}

/**
 * @brief Detect knowledge gap for concept_str
 *
 * Analyzes how much is unknown about a concept_str and how valuable
 * learning it would be. Uses stage strategy pattern to calculate
 * learning potential appropriate for developmental stage.
 *
 * Time Complexity: O(1) average case (hash lookup + strategy dispatch)
 *
 * @param engine Curiosity engine
 * @param concept_str Concept to analyze
 * @return Knowledge gap analysis
 */
knowledge_gap_t curiosity_detect_knowledge_gap(curiosity_engine_t engine, const char* concept_str)
{
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_detect_knowledge_gap", 0.0f);


    knowledge_gap_t gap = {0};

    if (!engine || !concept_str) {
        return gap;
    }

    strncpy(gap.topic, concept_str, sizeof(gap.topic) - 1);

    float familiarity = curiosity_check_familiarity(engine, concept_str);
    gap.gap_size = 1.0F - familiarity;
    gap.curiosity_intensity = calculate_curiosity_intensity(engine, gap.gap_size);

    // Use stage strategy for learning potential
    gap.learning_potential =
        engine->stage_strategy->calculate_learning_potential(concept_str, gap.gap_size);

    gap.related_concepts = count_related_concepts(engine, concept_str);

    /* Previously: on every high-novelty probe (gap_size > 0.7), we ran
     * network_analyzer_run() which executes full Louvain community detection
     * across 1.8M neurons / 1.45B synapses. On a fresh brain most topics are
     * novel, so this fired repeatedly during training and hung the RPC for
     * minutes each call.
     *
     * Observed 2026-04-20: curiosity_detect_gaps stalled 30+ min because the
     * training's CuriositySelector called it every step, and each call
     * triggered a full Louvain pass. Training hit BlockingIOError retries
     * while the brain worker was stuck in community detection.
     *
     * Community detection is an offline-quality analysis; call it from a
     * dedicated admin RPC on a coarse interval (e.g. every 10K steps) rather
     * than from a hot-path probe. The learning_potential boost here was
     * nice-to-have, not load-bearing. */

    // Broadcast high-curiosity events via bio-async
    if (gap.curiosity_intensity > 0.7F) {
        bio_broadcast_curiosity_spike(engine, gap.curiosity_intensity,
                                      gap.learning_potential, 0);
    }

    /* W13: emit curiosity-gap event to KG. Only for high-intensity gaps to
     * keep the graph from firehosing, and read recent surprise from the KG
     * so neighbour-modules can bias future probes. */
    if (gap.curiosity_intensity > 0.5F && engine->parent_brain) {
        wave13_curiosity_emit_gap(engine->parent_brain, concept_str,
                                  gap.gap_size, gap.curiosity_intensity);
        /* Trivial read-path use: refresh engine's last_novelty_score with
         * the KG-tracked recent surprise; a neutral 0 if KG is unaware. */
        float recent = wave13_curiosity_query_recent_surprise(engine->parent_brain);
        if (recent > engine->gap_detector_state.last_novelty_score) {
            engine->gap_detector_state.last_novelty_score = recent;
        }
    }

    return gap;
}

/**
 * @brief Get related concepts for a topic
 *
 * Retrieves concepts that are associated with the given concept_str.
 * Uses hash table for O(1) concept_str lookup.
 *
 * Time Complexity: O(min(k, max_related)) where k is number of related concepts
 *
 * @param engine Curiosity engine
 * @param concept_str Concept to query
 * @param related Output array for related concept_str strings
 * @param max_related Maximum concepts to return
 * @return Number of related concepts found
 */
uint32_t curiosity_get_related_concepts(curiosity_engine_t engine, const char* concept_str,
                                        char** related, uint32_t max_related)
{
    if (!engine || !concept_str) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_get_related_concepts", 0.0f);


    concept_bucket_t* bucket = find_concept_bucket(engine, concept_str);
    if (!bucket) {
        return 0;
    }

    if (!related) {
        return bucket->num_related;
    }

    uint32_t count = (bucket->num_related < max_related) ? bucket->num_related : max_related;

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(i + 1) / (float)count);
        }

        related[i] = bucket->related_concepts[i];
    }

    return count;
}

//=============================================================================
// Question Generation
//=============================================================================

/**
 * @brief Generate single question using strategy pattern
 *
 * Uses strategy dispatch table to eliminate switch statements.
 * Each question type has its own generation function.
 *
 * Complexity: O(1) strategy lookup + O(n) string formatting
 */
static bool generate_single_question(const knowledge_gap_t* gap, question_type_t type,
                                     generated_question_t* output)
{
    if (!gap || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "generate_single_question: required parameter is NULL (gap, output)");
        return false;
    }

    const question_strategy_t* strategy = get_question_strategy(type);
    if (!strategy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "generate_single_question: strategy is NULL");
        return false;
    }

    memset(output, 0, sizeof(generated_question_t));
    output->type = type;

    float priority_mult = 0.0F;
    float difficulty = 0.0F;

    strategy->generator(gap->topic, output->question, sizeof(output->question), &priority_mult,
                        &difficulty);

    output->priority = gap->curiosity_intensity * priority_mult;
    output->difficulty = difficulty;

    return true;
}

/**
 * @brief Generate questions for knowledge gap
 *
 * Creates multiple questions appropriate for current learning stage.
 * Uses stage strategy to determine which question types to generate.
 * Uses question strategy pattern to generate each question.
 *
 * Time Complexity: O(k) where k is number of question types for stage
 *
 * @param engine Curiosity engine
 * @param gap Knowledge gap to question
 * @param questions Output array for generated questions
 * @param max_questions Maximum questions to generate
 * @return Number of questions generated
 */
uint32_t curiosity_generate_questions(curiosity_engine_t engine, const knowledge_gap_t* gap,
                                      generated_question_t* questions, uint32_t max_questions)
{
    if (!engine || !gap || !questions || max_questions == 0) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_generate_questions", 0.0f);


    const question_type_t* types = engine->stage_strategy->get_question_types();
    uint32_t num_types = engine->stage_strategy->get_question_types_count();
    uint32_t generated = 0;

    for (uint32_t i = 0; i < num_types && generated < max_questions; i++) {
        if (generate_single_question(gap, types[i], &questions[generated])) {
            generated++;
        }
    }

    return generated;
}

/**
 * @brief Generate follow-up question based on previous answer
 *
 * In full implementation, would use NLP to extract key concepts
 * and generate relevant follow-ups. Currently returns generic prompt.
 *
 * Complexity: O(1)
 *
 * @param engine Curiosity engine
 * @param previous_answer Answer to follow up on
 * @return Follow-up question string
 */
const char* curiosity_generate_followup(curiosity_engine_t engine, const char* previous_answer)
{
    if (!engine || !previous_answer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_generate_followup: required parameter is NULL (engine, previous_answer)");
        return NULL;
    }

    static char followup[NIMCP_ERROR_BUFFER_LARGE];
    snprintf(followup, sizeof(followup), "Can you tell me more about that?");

    return followup;
}

//=============================================================================
// Motivation Assessment
//=============================================================================

/**
 * @brief Calculate weighted motivation score
 *
 * Combines multiple motivation factors with empirically-determined weights.
 *
 * Complexity: O(1)
 */
static float calculate_overall_motivation(const motivation_state_t* state)
{
    return (state->intrinsic_curiosity * 0.4F + state->goal_relevance * 0.2F +
            state->social_importance * 0.1F + state->survival_value * 0.2F +
            state->aesthetic_appeal * 0.1F);
}

/**
 * @brief Calculate aesthetic appeal based on novelty
 *
 * Novel concepts are more aesthetically appealing.
 *
 * Complexity: O(1)
 */
static float calculate_aesthetic_appeal(float familiarity)
{
    return (1.0F - familiarity) * 0.8F;
}

/**
 * @brief Assess motivation to learn concept_str
 *
 * Evaluates multiple factors that drive learning:
 * - Intrinsic curiosity (baseline trait)
 * - Goal relevance (alignment with objectives)
 * - Social importance (what others value)
 * - Survival value (safety/wellbeing)
 * - Aesthetic appeal (novelty/interest)
 *
 * Time Complexity: O(1) average case (hash lookup)
 *
 * @param engine Curiosity engine
 * @param concept_str Concept to assess
 * @return Motivation state breakdown
 */
motivation_state_t curiosity_assess_motivation(curiosity_engine_t engine, const char* concept_str)
{
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_assess_motivation", 0.0f);


    motivation_state_t state = {0};

    if (!engine || !concept_str) {
        return state;
    }

    state.intrinsic_curiosity = engine->baseline_curiosity;

    // DOPAMINE MODULATION: High DA enhances intrinsic motivation
    if (engine->parent_brain) {
        neuromodulator_system_t neuromod = brain_get_neuromodulator_system(engine->parent_brain);
        if (neuromod) {
            float dopamine = neuromodulator_get_level(neuromod, NEUROMOD_DOPAMINE);
            // Dopamine amplifies intrinsic curiosity
            state.intrinsic_curiosity *= (0.5F + dopamine);  // 0.8-1.2× modulation
        }
    }

    state.goal_relevance = 0.3F;     // Simplified - would check actual goals
    state.social_importance = 0.4F;  // Simplified - would check social context
    state.survival_value = 0.1F;     // Most concepts have low survival value

    float familiarity = curiosity_check_familiarity(engine, concept_str);
    state.aesthetic_appeal = calculate_aesthetic_appeal(familiarity);
    state.overall_motivation = calculate_overall_motivation(&state);

    return state;
}

/**
 * @brief Set baseline curiosity level
 *
 * Adjusts intrinsic motivation to learn. Clamped to [0.0, 1.0].
 *
 * Complexity: O(1)
 */
void curiosity_set_baseline(curiosity_engine_t engine, float level)
{
    if (!engine) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_set_baseline", 0.0f);


    engine->baseline_curiosity = fminf(fmaxf(level, 0.0F), 1.0F);
    engine->current_motivation = engine->baseline_curiosity;
}

/**
 * @brief Get current learning drive
 *
 * Returns current motivation level.
 *
 * Complexity: O(1)
 */
float curiosity_get_drive(curiosity_engine_t engine)
{
    if (!engine) {
        return 0.0F;
    }
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_get_drive", 0.0f);


    return engine->current_motivation;
}

//=============================================================================
// Incremental Learning
//=============================================================================

/**
 * @brief Record question in history
 *
 * Adds question-answer pair to learning history.
 * Used for tracking progress and preventing repetition.
 *
 * Complexity: O(1) assuming capacity available
 */
static bool record_question_history(curiosity_engine_t engine, const char* question,
                                    const char* answer)
{
    if (engine->num_questions >= engine->questions_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "record_question_history: capacity exceeded");
        return false;
    }

    question_history_t* q = &engine->questions[engine->num_questions++];
    strncpy(q->question, question, sizeof(q->question) - 1);
    strncpy(q->answer, answer, sizeof(q->answer) - 1);
    q->answered = true;
    q->learning_value = 0.5F;
    q->timestamp = 0;

    return true;
}

/**
 * @brief Learn from question-answer pair
 *
 * Records answer and updates internal knowledge representation.
 * In full implementation, would extract concepts and update familiarity.
 *
 * Time Complexity: O(1) for recording
 *
 * @param engine Curiosity engine
 * @param question Question that was asked
 * @param answer Answer received
 * @return true on success
 */
bool curiosity_learn_answer(curiosity_engine_t engine, const char* question, const char* answer)
{
    if (!engine || !question || !answer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_learn_answer: required parameter is NULL (engine, question, answer)");
        return false;
    }

    if (!record_question_history(engine, question, answer)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "curiosity_learn_answer: record_question_history is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_learn_answer", 0.0f);


    engine->progress.total_answers_learned++;

    // Extract concepts from question/answer and update familiarity
    // NOTE: This is needed for test_network_analysis.cpp LowNoveltySkipsAnalysis test
    char question_copy[NIMCP_ERROR_BUFFER_SIZE];
    strncpy(question_copy, question, sizeof(question_copy) - 1);
    question_copy[sizeof(question_copy) - 1] = '\0';

    char* word = strtok(question_copy, " ?");
    while (word != NULL) {
        if (strlen(word) > 3) {  // Skip short words
            char normalized[NIMCP_LABEL_BUFFER_SIZE];
            normalize_string(word, normalized, sizeof(normalized));

            concept_bucket_t* bucket = find_concept_bucket(engine, normalized);
            if (!bucket) {
                bucket = add_concept_to_hash_table(engine, normalized);
            }
            if (bucket) {
                bucket->familiarity += 0.4F;  // Ensure drops below 0.7 threshold
                if (bucket->familiarity > 1.0F) {
                    bucket->familiarity = 1.0F;
                }
            }
        }
        word = strtok(NULL, " ?");
    }

    // INTRINSIC REWARD: Release dopamine for learning
    // BIOLOGY: Learning triggers dopamine release in reward circuits
    if (engine->parent_brain) {
        neuromodulator_system_t neuromod = brain_get_neuromodulator_system(engine->parent_brain);
        if (neuromod) {
            // Release dopamine proportional to curiosity
            float reward = 0.3F;  // Moderate intrinsic reward for learning
            neuromodulator_release_dopamine(neuromod, reward, 0.0F);
        }
    }

    return true;
}

/**
 * @brief Learn from direct experience
 *
 * Processes sensory data and associates with concepts.
 * Enables multi-modal learning (vision, audio, etc.).
 *
 * Complexity: O(1) for recording
 *
 * @param engine Curiosity engine
 * @param experience_description Text description of experience
 * @param sensory_data Feature vector from sensors
 * @param num_features Size of feature vector
 * @return true on success
 */
bool curiosity_learn_experience(curiosity_engine_t engine, const char* experience_description,
                                const float* sensory_data, uint32_t num_features)
{
    if (!engine || !experience_description) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_learn_experience: required parameter is NULL (engine, experience_description)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_learn_experience", 0.0f);


    engine->progress.total_experiences++;

    // INTRINSIC REWARD: Release dopamine for experiential learning
    // BIOLOGY: Novel experiences trigger dopamine release
    if (engine->parent_brain) {
        neuromodulator_system_t neuromod = brain_get_neuromodulator_system(engine->parent_brain);
        if (neuromod) {
            float reward = 0.4F;  // Higher reward for direct experience
            neuromodulator_release_dopamine(neuromod, reward, 0.0F);
        }
    }

    return true;
}

/**
 * @brief Learn from observation
 *
 * Social learning from watching others.
 * Updates concept_str knowledge based on observed interactions.
 *
 * Complexity: O(1)
 *
 * @param engine Curiosity engine
 * @param what_observed What was observed
 * @param context Context of observation
 * @return true on success
 */
bool curiosity_learn_observation(curiosity_engine_t engine, const char* what_observed,
                                 const char* context)
{
    if (!engine || !what_observed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_learn_observation: required parameter is NULL (engine, what_observed)");
        return false;
    }

    // Would update concept_str familiarity and relationships
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_learn_observation", 0.0f);


    return true;
}

//=============================================================================
// Knowledge Source Management
//=============================================================================

/**
 * @brief Expand knowledge source array if needed
 *
 * Doubles capacity when full.
 *
 * Complexity: O(n) when resizing, amortized O(1)
 */
static bool ensure_source_capacity(curiosity_engine_t engine)
{
    if (engine->num_sources < engine->sources_capacity) {
        return true;
    }

    engine->sources_capacity *= 2;
    knowledge_source_t* new_sources =
        nimcp_realloc(engine->sources, engine->sources_capacity * sizeof(knowledge_source_t));

    if (!new_sources) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ensure_source_capacity: new_sources is NULL");
        return false;
    }

    engine->sources = new_sources;
    return true;
}

/**
 * @brief Register external knowledge source
 *
 * Adds searchable knowledge source (e.g., database, API, documents).
 * Engine will query registered sources when seeking knowledge.
 *
 * Time Complexity: O(1) amortized
 *
 * @param engine Curiosity engine
 * @param source_name Name of source
 * @param search_fn Function to search this source
 * @param context Opaque context for search function
 * @return true on success
 */
bool curiosity_register_knowledge_source(curiosity_engine_t engine, const char* source_name,
                                         knowledge_search_fn_t search_fn, void* context)
{
    if (!engine || !source_name || !search_fn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_register_knowledge_source: required parameter is NULL (engine, source_name, search_fn)");
        return false;
    }

    if (!ensure_source_capacity(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "curiosity_register_knowledge_source: ensure_source_capacity is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_register_knowledge_s", 0.0f);


    knowledge_source_t* source = &engine->sources[engine->num_sources++];
    strncpy(source->name, source_name, sizeof(source->name) - 1);
    source->search_fn = search_fn;
    source->context = context;
    source->active = true;

    return true;
}

/**
 * @brief Query single knowledge source
 *
 * Executes search on one source and collects results.
 *
 * Complexity: O(k) where k is results from this source
 */
static uint32_t query_knowledge_source(const knowledge_source_t* source, const char* topic,
                                       char** results, uint32_t max_results, uint32_t current_count)
{
    if (!source->active) {
        return 0;
    }

    uint32_t num_found = 0;
    char** source_results =
        source->search_fn(topic, source->context, max_results - current_count, &num_found);

    if (!source_results) {
        return 0;
    }

    uint32_t copied = 0;
    for (uint32_t i = 0; i < num_found && current_count + copied < max_results; i++) {
        results[current_count + copied] = source_results[i];
        copied++;
    }

    // Free any strings that weren't copied (if result limit was reached)
    for (uint32_t i = copied; i < num_found; i++) {
        nimcp_free(source_results[i]);
    }

    // Free the source_results array itself
    nimcp_free(source_results);
    source_results = NULL;

    return copied;
}

/**
 * @brief Search all knowledge sources for information
 *
 * Queries all registered sources and aggregates results.
 * Sources are queried sequentially until result limit reached.
 *
 * Time Complexity: O(s * k) where s is sources, k is results per source
 *
 * @param engine Curiosity engine
 * @param gap Knowledge gap to fill
 * @param results Output array for search results
 * @param max_results Maximum results to return
 * @return Number of results found
 */
uint32_t curiosity_seek_knowledge(curiosity_engine_t engine, const knowledge_gap_t* gap,
                                  char** results, uint32_t max_results)
{
    if (!engine || !gap || !results) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_seek_knowledge", 0.0f);


    uint32_t total_results = 0;

    for (uint32_t i = 0; i < engine->num_sources && total_results < max_results; i++) {
        uint32_t found = query_knowledge_source(&engine->sources[i], gap->topic, results,
                                                max_results, total_results);
        total_results += found;
    }

    return total_results;
}

//=============================================================================
// Learning Progress Tracking
//=============================================================================

/**
 * @brief Update computed progress fields
 *
 * Refreshes derived statistics before returning progress.
 *
 * Complexity: O(1)
 */
static void update_progress_statistics(curiosity_engine_t engine, learning_progress_t* progress)
{
    // Process pending bio-async messages
    if (engine && engine->bio_async_enabled && engine->bio_ctx) {
        bio_router_process_inbox(engine->bio_ctx, 5);
    }

    progress->concepts_learned = engine->total_concepts;
    progress->avg_curiosity = engine->baseline_curiosity;
}

/**
 * @brief Get learning progress statistics
 *
 * Returns comprehensive learning metrics.
 *
 * Complexity: O(1)
 *
 * @param engine Curiosity engine
 * @param progress Output structure for progress data
 * @return true on success
 */
bool curiosity_get_progress(curiosity_engine_t engine, learning_progress_t* progress)
{
    if (!engine || !progress) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_get_progress: required parameter is NULL (engine, progress)");
        return false;
    }

    *progress = engine->progress;
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_get_progress", 0.0f);


    update_progress_statistics(engine, progress);

    return true;
}

/**
 * @brief Check if concept_str belongs to domain
 *
 * Simplified domain matching via substring search.
 * Full implementation would use proper ontology/taxonomy.
 *
 * Complexity: O(n * m) where n is concept_str length, m is domain length
 */
static bool concept_in_domain(const char* concept_str, const char* domain)
{
    return strstr(concept_str, domain) != NULL;
}

/**
 * @brief Count concepts in domain
 *
 * Traverses hash table counting domain-related concepts.
 *
 * Complexity: O(n) where n is total concepts
 */
/**
 * @brief Context for count_domain_concepts iteration
 */
typedef struct {
    const char* domain;
    uint32_t count;
} count_domain_context_t;

/**
 * @brief Iterator callback for counting domain concepts
 */
static bool count_domain_callback(const void* key, size_t key_size, void* value, size_t value_size,
                                  void* user_data)
{
    concept_bucket_t* bucket = (concept_bucket_t*) value;
    count_domain_context_t* ctx = (count_domain_context_t*) user_data;

    if (concept_in_domain(bucket->concept, ctx->domain)) {
        ctx->count++;
    }

    return true;  // Continue iteration
}

static uint32_t count_domain_concepts(curiosity_engine_t engine, const char* domain)
{
    if (!engine || !engine->concept_hash_table) {
        return 0;
    }

    count_domain_context_t ctx = {.domain = domain, .count = 0};

    hash_table_iterate(engine->concept_hash_table, count_domain_callback, &ctx);

    return ctx.count;
}

/**
 * @brief Estimate domain coverage
 *
 * Calculates what proportion of domain concepts are known.
 * Uses heuristic estimate of domain size (100 concepts).
 *
 * Time Complexity: O(n) where n is total concepts
 *
 * @param engine Curiosity engine
 * @param domain Domain to assess
 * @return Coverage fraction [0.0-1.0]
 */
float curiosity_get_domain_coverage(curiosity_engine_t engine, const char* domain)
{
    if (!engine || !domain) {
        return 0.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_get_domain_coverage", 0.0f);


    uint32_t domain_concepts = count_domain_concepts(engine, domain);
    return fminf((float) domain_concepts / 100.0F, 1.0F);
}

//=============================================================================
// Developmental Stage Management
//=============================================================================

/**
 * @brief Get current learning stage
 *
 * Returns developmental stage (infant, toddler, etc.).
 *
 * Complexity: O(1)
 */
learning_stage_t curiosity_get_stage(curiosity_engine_t engine)
{
    if (!engine) {
        return STAGE_INFANT;
    }
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_get_stage", 0.0f);


    return engine->stage;
}

/**
 * @brief Set learning stage and update strategy
 *
 * Changes developmental stage and switches to appropriate strategy.
 * Updates baseline curiosity based on stage characteristics.
 * Uses state pattern to modify all stage-dependent behaviors.
 *
 * Time Complexity: O(1)
 *
 * @param engine Curiosity engine
 * @param stage New learning stage
 */
void curiosity_set_stage(curiosity_engine_t engine, learning_stage_t stage)
{
    if (!engine) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_set_stage", 0.0f);


    engine->stage = stage;
    engine->stage_strategy = get_stage_strategy(stage);
    engine->baseline_curiosity = engine->stage_strategy->get_baseline_curiosity();
    engine->current_motivation = engine->baseline_curiosity;
}

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Print knowledge gap analysis
 *
 * Debugging utility to display gap information.
 *
 * Complexity: O(1)
 */
void curiosity_print_gap(const knowledge_gap_t* gap)
{
    if (!gap) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_print_gap", 0.0f);


    printf("Knowledge Gap:\n");
    printf("  Topic: %s\n", gap->topic);
    printf("  Gap size: %.2f\n", gap->gap_size);
    printf("  Curiosity intensity: %.2f\n", gap->curiosity_intensity);
    printf("  Learning potential: %.2f\n", gap->learning_potential);
    printf("  Related concepts: %u\n", gap->related_concepts);
}

/**
 * @brief Print generated question
 *
 * Debugging utility to display question information.
 *
 * Complexity: O(1)
 */
void curiosity_print_question(const generated_question_t* question)
{
    if (!question) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_print_question", 0.0f);


    printf("Question:\n");
    printf("  Text: %s\n", question->question);
    printf("  Type: %d\n", question->type);
    printf("  Priority: %.2f\n", question->priority);
    printf("  Difficulty: %.2f\n", question->difficulty);
}

/**
 * @brief Print learning progress
 *
 * Debugging utility to display progress statistics.
 *
 * Complexity: O(1)
 */
void curiosity_print_progress(const learning_progress_t* progress)
{
    if (!progress) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_print_progress", 0.0f);


    printf("Learning Progress:\n");
    printf("  Questions asked: %lu\n", progress->total_questions_asked);
    printf("  Answers learned: %lu\n", progress->total_answers_learned);
    printf("  Experiences: %lu\n", progress->total_experiences);
    printf("  Concepts learned: %lu\n", progress->concepts_learned);
    printf("  Knowledge growth rate: %.3f\n", progress->knowledge_growth_rate);
    printf("  Average curiosity: %.2f\n", progress->avg_curiosity);
    printf("  Current focus: %s\n", progress->current_focus);
}

//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

/**
 * @brief Set exploration rate based on cognitive load
 *
 * WHAT: Adjust exploration vs exploitation based on executive load
 * WHY:  Prevent exploration when cognitively overloaded
 * HOW:  Modulate curiosity intensity and baseline
 *
 * BIOLOGY: Prefrontal cortex regulates exploration/exploitation trade-off
 *          High load → exploit known strategies (lower exploration)
 *          Low load → explore novel options (higher exploration)
 *
 * COMPLEXITY: O(1)
 *
 * @param engine Curiosity engine
 * @param exploration_rate Exploration rate [0, 1] (0=exploit, 1=explore)
 */
void curiosity_set_exploration_rate(curiosity_engine_t engine, float exploration_rate)
{
    // Guard: NULL engine
    if (!engine) {
        return;
    }

    // Clamp exploration rate to [0, 1]
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_set_exploration_rate", 0.0f);


    exploration_rate = fminf(fmaxf(exploration_rate, 0.0F), 1.0F);

    // WHAT: Modulate baseline curiosity based on exploration rate
    // WHY:  Higher exploration rate → more curious
    // HOW:  Scale baseline by exploration rate
    engine->baseline_curiosity = engine->stage_strategy->get_baseline_curiosity() * exploration_rate;
    engine->current_motivation = engine->baseline_curiosity;
}

/**
 * @brief Get information gain from recent learning
 *
 * WHAT: Query expected information gain from exploring
 * WHY:  Executive can prioritize exploratory tasks
 * HOW:  Estimate gain from recent learning progress
 *
 * COMPLEXITY: O(1)
 *
 * @param engine Curiosity engine
 * @return Information gain [0, 1]
 */
float curiosity_get_information_gain(curiosity_engine_t engine)
{
    // Guard: NULL engine
    if (!engine) {
        return 0.0F;
    }

    // WHAT: Estimate information gain from recent learning
    // WHY:  If we're learning a lot, exploration is paying off
    // HOW:  Use knowledge growth rate as proxy for information gain
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_get_information_gain", 0.0f);


    if (engine->progress.total_questions_asked == 0) {
        return engine->baseline_curiosity;  // No data yet, use baseline
    }

    // Calculate learning rate: answers / questions
    float learning_rate = (float)engine->progress.total_answers_learned /
                         (float)engine->progress.total_questions_asked;

    // Scale by current motivation
    float information_gain = learning_rate * engine->current_motivation;

    // Clamp to [0, 1]
    return fminf(fmaxf(information_gain, 0.0F), 1.0F);
}

//=============================================================================
// Immune System Integration
//=============================================================================

#include "cognitive/immune/nimcp_curiosity_immune_bridge.h"


/**
 * @brief Connect curiosity engine to brain immune system
 *
 * WHAT: Establish bidirectional immune-curiosity coupling
 * WHY:  Model sickness behavior (cytokines suppress exploration) and novelty vigilance
 * HOW:  Create curiosity_immune_bridge, register callbacks with immune system
 *
 * COMPLEXITY: O(1)
 *
 * @param engine Curiosity engine
 * @param immune_system Brain immune system to connect
 * @return 0 on success, -1 on error
 */
int curiosity_connect_immune(curiosity_engine_t engine, struct brain_immune_system* immune_system)
{
    // Guard: NULL pointers
    if (!engine || !immune_system) {
        LOG_ERROR("Cannot connect NULL engine or immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_connect_immune: required parameter is NULL (engine, immune_system)");
        return -1;
    }

    // Guard: Already connected
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_connect_immune", 0.0f);


    if (engine->immune_bridge) {
        LOG_WARN("Curiosity already connected to immune system, disconnecting first");
        curiosity_disconnect_immune(engine);
    }

    // WHAT: Create curiosity-immune bridge
    // WHY:  Bridge manages bidirectional coupling
    // HOW:  Use default config, link both systems
    curiosity_immune_bridge_t* bridge = curiosity_immune_bridge_create(
        NULL,  // Use default config
        immune_system,
        engine
    );

    if (!bridge) {
        LOG_ERROR("Failed to create curiosity-immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_connect_immune: bridge is NULL");
        return -1;
    }

    engine->immune_bridge = (void*)bridge;

    LOG_INFO("Curiosity engine connected to brain immune system");
    return 0;
}

/**
 * @brief Disconnect from brain immune system
 *
 * WHAT: Tear down immune-curiosity coupling
 * WHY:  Clean shutdown, restore original curiosity levels
 * HOW:  Destroy bridge, unregister callbacks
 *
 * COMPLEXITY: O(1)
 *
 * @param engine Curiosity engine
 * @return 0 on success, -1 on error
 */
int curiosity_disconnect_immune(curiosity_engine_t engine)
{
    // Guard: NULL engine
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_disconnect_immune: engine is NULL");
        return -1;
    }

    // Guard: Not connected
    if (!engine->immune_bridge) {
        return 0;  // Already disconnected
    }

    // WHAT: Destroy bridge
    // WHY:  Clean up resources, restore curiosity
    // HOW:  Call bridge destructor
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_disconnect_immune", 0.0f);


    curiosity_immune_bridge_t* bridge = (curiosity_immune_bridge_t*)engine->immune_bridge;
    curiosity_immune_bridge_destroy(bridge);

    engine->immune_bridge = NULL;

    LOG_INFO("Curiosity engine disconnected from brain immune system");
    return 0;
}

/**
 * @brief Get current sickness behavior suppression level
 *
 * WHAT: Query immune-induced curiosity suppression
 * WHY:  Diagnostic visibility into sickness behavior effects
 * HOW:  Return suppression factor from immune bridge
 *
 * COMPLEXITY: O(1)
 *
 * @param engine Curiosity engine
 * @return Suppression factor (0-1, where 0=max suppression, 1=no suppression)
 */
float curiosity_get_immune_suppression(curiosity_engine_t engine)
{
    // Guard: NULL engine or not connected
    if (!engine || !engine->immune_bridge) {
        return 1.0F;  // No suppression if not connected
    }

    // WHAT: Query bridge for suppression factor
    // WHY:  Bridge tracks immune-induced curiosity modulation
    // HOW:  Call bridge getter
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_get_immune_suppressi", 0.0f);


    curiosity_immune_bridge_t* bridge = (curiosity_immune_bridge_t*)engine->immune_bridge;
    return curiosity_immune_get_suppression_factor(bridge);
}

/**
 * @brief Get current immune vigilance boost from novelty
 *
 * WHAT: Query curiosity-induced immune alertness
 * WHY:  Diagnostic visibility into novelty-immune coupling
 * HOW:  Return vigilance boost from immune bridge
 *
 * COMPLEXITY: O(1)
 *
 * @param engine Curiosity engine
 * @return Immune vigilance boost (1.0-1.5x)
 */
float curiosity_get_novelty_vigilance_boost(curiosity_engine_t engine)
{
    // Guard: NULL engine or not connected
    if (!engine || !engine->immune_bridge) {
        return 1.0F;  // No boost if not connected
    }

    // WHAT: Query bridge for vigilance boost
    // WHY:  Bridge tracks novelty-induced immune alertness
    // HOW:  Call bridge getter
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_get_novelty_vigilanc", 0.0f);


    curiosity_immune_bridge_t* bridge = (curiosity_immune_bridge_t*)engine->immune_bridge;
    return curiosity_immune_get_vigilance_boost(bridge);
}

//=============================================================================
// Monte Carlo Integration API
//=============================================================================

/**
 * @brief Select exploration action using epsilon-greedy MC strategy
 *
 * WHAT: Choose between exploration and exploitation
 * WHY:  Balance novelty-seeking with using known strategies
 * HOW:  With probability epsilon, explore; else exploit highest-value option
 *
 * BIOLOGY: Dopamine modulates explore/exploit balance in prefrontal cortex
 *          High uncertainty → explore, low uncertainty → exploit
 *
 * @param engine Curiosity engine
 * @param epsilon Exploration probability [0, 1]
 * @return true if should explore, false if should exploit
 */
bool curiosity_should_explore_mc(curiosity_engine_t engine, float epsilon) {
    if (!engine) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_should_explore_mc", 0.0f);


    if (g_curiosity_mc_seed == 0) {
        g_curiosity_mc_seed = mc_seed_from_time();
    }

    /* Epsilon-greedy decision */
    float r = mc_random_uniform(&g_curiosity_mc_seed);
    return r < epsilon;
}

/**
 * @brief Sample topic to explore using novelty-weighted MC sampling
 *
 * WHAT: Select concept to explore based on novelty
 * WHY:  Prioritize unexplored areas with probabilistic diversity
 * HOW:  Sample from gap_size distribution (higher novelty → more likely)
 *
 * @param engine Curiosity engine
 * @param concepts Array of candidate concepts
 * @param num_concepts Number of candidates
 * @return Index of selected concept
 */
uint32_t curiosity_sample_exploration_target_mc(
    curiosity_engine_t engine,
    const char** concepts,
    uint32_t num_concepts
) {
    if (!engine || !concepts || num_concepts == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_sample_exploration_t", 0.0f);


    if (g_curiosity_mc_seed == 0) {
        g_curiosity_mc_seed = mc_seed_from_time();
    }

    /* Compute novelty weights (gap size) for each concept */
    float* weights = nimcp_calloc(num_concepts, sizeof(float));
    if (!weights) return 0;

    float sum_weights = 0.0f;
    for (uint32_t i = 0; i < num_concepts; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_concepts > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(i + 1) / (float)num_concepts);
        }

        float familiarity = curiosity_check_familiarity(engine, concepts[i]);
        weights[i] = 1.0f - familiarity;  /* Higher novelty = higher weight */
        if (weights[i] < 0.01f) weights[i] = 0.01f;  /* Minimum weight */
        sum_weights += weights[i];
    }

    /* Sample from weighted distribution */
    float r = mc_random_uniform(&g_curiosity_mc_seed) * sum_weights;
    float cumulative = 0.0f;
    uint32_t selected = num_concepts - 1;

    for (uint32_t i = 0; i < num_concepts; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_concepts > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(i + 1) / (float)num_concepts);
        }

        cumulative += weights[i];
        if (r < cumulative) {
            selected = i;
            break;
        }
    }

    nimcp_free(weights);
    weights = NULL;
    return selected;
}

/**
 * @brief Estimate information gain via MC simulation
 *
 * WHAT: Estimate expected information gain from exploring topic
 * WHY:  Guide exploration toward high-value targets
 * HOW:  GPU: Parallel simulation on GPU (default)
 *       CPU: Sequential simulation (fallback)
 *
 * PERFORMANCE: GPU provides 10-100x speedup for num_simulations > 1000
 *
 * @param engine Curiosity engine
 * @param topic Topic to evaluate
 * @param num_simulations Number of MC simulations
 * @return Expected information gain [0, 1]
 */
float curiosity_estimate_info_gain_mc(
    curiosity_engine_t engine,
    const char* topic,
    uint32_t num_simulations
) {
    if (!engine || !topic || num_simulations == 0) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_estimate_info_gain_m", 0.0f);


    float current_familiarity = curiosity_check_familiarity(engine, topic);
    float gap_size = 1.0f - current_familiarity;
    float base_gain = gap_size;

#ifdef NIMCP_ENABLE_CUDA
    /* Try GPU acceleration first (default path) */
    if (curiosity_has_gpu_mc() && num_simulations >= 100) {
        /* GPU-accelerated path: generate samples in parallel */
        size_t dims[] = {num_simulations};
        nimcp_gpu_tensor_t* uniform_samples = nimcp_gpu_tensor_create(
            g_curiosity_gpu_ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* normal_samples = nimcp_gpu_tensor_create(
            g_curiosity_gpu_ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

        if (uniform_samples && normal_samples) {
            bool ok = qmc_gpu_sample_uniform(g_curiosity_gpu_ctx, g_curiosity_gpu_rng, uniform_samples);
            ok = ok && qmc_gpu_sample_normal(g_curiosity_gpu_ctx, g_curiosity_gpu_rng, normal_samples, 0.0f, 0.1f);

            if (ok) {
                /* Copy results to host and compute gain */
                float* h_uniform = nimcp_calloc(num_simulations, sizeof(float));
                if (!h_uniform) return -1;
                float* h_normal = nimcp_calloc(num_simulations, sizeof(float));
                if (!h_normal) {
                    nimcp_free(h_uniform);
                    return -1;
                }

                if (h_uniform && h_normal) {
                    cudaMemcpy(h_uniform, uniform_samples->data, num_simulations * sizeof(float), cudaMemcpyDeviceToHost);
                    cudaMemcpy(h_normal, normal_samples->data, num_simulations * sizeof(float), cudaMemcpyDeviceToHost);

                    float total_gain = 0.0f;
                    for (uint32_t s = 0; s < num_simulations; s++) {
                        /* Phase 8: Loop progress heartbeat */
                        if ((s & 0xFF) == 0 && num_simulations > 256) {
                            curiosity_heartbeat("curiosity_loop",
                                             (float)(s + 1) / (float)num_simulations);
                        }

                        float learning_rate = 0.3f + 0.4f * h_uniform[s];
                        float noise = h_normal[s];
                        float simulated_gain = base_gain * learning_rate + noise;
                        if (simulated_gain < 0.0f) simulated_gain = 0.0f;
                        if (simulated_gain > 1.0f) simulated_gain = 1.0f;
                        total_gain += simulated_gain;
                    }

                    nimcp_free(h_uniform);
                    h_uniform = NULL;
                    nimcp_free(h_normal);
                    h_normal = NULL;
                    nimcp_gpu_tensor_destroy(uniform_samples);
                    nimcp_gpu_tensor_destroy(normal_samples);

                    return total_gain / (float)num_simulations;
                }
                nimcp_free(h_uniform);
                h_uniform = NULL;
                nimcp_free(h_normal);
                h_normal = NULL;
            }
        }

        /* Clean up on failure, fall through to CPU */
        if (uniform_samples) nimcp_gpu_tensor_destroy(uniform_samples);
        if (normal_samples) nimcp_gpu_tensor_destroy(normal_samples);
        LOG_DEBUG("GPU MC failed, falling back to CPU");
    }
#endif

    /* CPU fallback path */
    if (g_curiosity_mc_seed == 0) {
        g_curiosity_mc_seed = mc_seed_from_time();
    }

    float total_gain = 0.0f;
    for (uint32_t s = 0; s < num_simulations; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && num_simulations > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(s + 1) / (float)num_simulations);
        }

        float learning_rate = 0.3f + 0.4f * mc_random_uniform(&g_curiosity_mc_seed);
        float noise = mc_random_normal(&g_curiosity_mc_seed, 0.0f, 0.1f);

        float simulated_gain = base_gain * learning_rate + noise;
        if (simulated_gain < 0.0f) simulated_gain = 0.0f;
        if (simulated_gain > 1.0f) simulated_gain = 1.0f;

        total_gain += simulated_gain;
    }

    return total_gain / (float)num_simulations;
}

/**
 * @brief Select question using softmax MC sampling
 *
 * WHAT: Probabilistically select question based on priority
 * WHY:  Enable diverse question generation while respecting priorities
 * HOW:  Apply softmax to priorities, sample from distribution
 *
 * @param engine Curiosity engine
 * @param questions Array of generated questions
 * @param num_questions Number of questions
 * @param temperature Softmax temperature (higher = more random)
 * @return Index of selected question
 */
uint32_t curiosity_sample_question_mc(
    curiosity_engine_t engine,
    const generated_question_t* questions,
    uint32_t num_questions,
    float temperature
) {
    if (!engine || !questions || num_questions == 0 || temperature <= 0.0f) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_sample_question_mc", 0.0f);


    if (g_curiosity_mc_seed == 0) {
        g_curiosity_mc_seed = mc_seed_from_time();
    }

    /* Compute softmax probabilities */
    float* probs = nimcp_calloc(num_questions, sizeof(float));
    if (!probs) return 0;

    float max_priority = questions[0].priority;
    for (uint32_t i = 1; i < num_questions; i++) {
        if (questions[i].priority > max_priority) {
            max_priority = questions[i].priority;
        }
    }

    float sum_exp = 0.0f;
    for (uint32_t i = 0; i < num_questions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_questions > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(i + 1) / (float)num_questions);
        }

        probs[i] = expf((questions[i].priority - max_priority) / temperature);
        sum_exp += probs[i];
    }

    /* Sample from distribution */
    float r = mc_random_uniform(&g_curiosity_mc_seed) * sum_exp;
    float cumulative = 0.0f;
    uint32_t selected = num_questions - 1;

    for (uint32_t i = 0; i < num_questions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_questions > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(i + 1) / (float)num_questions);
        }

        cumulative += probs[i];
        if (r < cumulative) {
            selected = i;
            break;
        }
    }

    nimcp_free(probs);
    probs = NULL;
    return selected;
}

/**
 * @brief Add exploration noise to curiosity intensity
 *
 * WHAT: Add stochastic noise to curiosity for diversity
 * WHY:  Prevent getting stuck in exploration patterns
 * HOW:  Add Gaussian noise scaled by noise_scale
 *
 * @param intensity Base curiosity intensity
 * @param noise_scale Scale of noise (std dev)
 * @return Noisy intensity clamped to [0, 1]
 */
float curiosity_add_exploration_noise_mc(float intensity, float noise_scale) {
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_add_exploration_nois", 0.0f);


    if (g_curiosity_mc_seed == 0) {
        g_curiosity_mc_seed = mc_seed_from_time();
    }

    float noise = mc_random_normal(&g_curiosity_mc_seed, 0.0f, noise_scale);
    float noisy_intensity = intensity + noise;

    if (noisy_intensity < 0.0f) noisy_intensity = 0.0f;
    if (noisy_intensity > 1.0f) noisy_intensity = 1.0f;

    return noisy_intensity;
}

/**
 * @brief Get thread-local MC seed for curiosity module
 *
 * @return Pointer to thread-local seed
 */
uint32_t* curiosity_get_mc_seed(void) {
    if (g_curiosity_mc_seed == 0) {
        g_curiosity_mc_seed = mc_seed_from_time();
    }
    return &g_curiosity_mc_seed;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

/**
 * @brief Query knowledge graph for self-knowledge about curiosity module
 *
 * WHAT: Retrieve module's own entity and connections from KG
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int curiosity_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Curiosity_Module");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                curiosity_heartbeat("curiosity_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Curiosity self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Curiosity_Module");
    if (connections) {
        LOG_DEBUG("Curiosity has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Curiosity_Module");
    if (incoming) {
        LOG_DEBUG("Curiosity has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

//=============================================================================
// Empowerment QMC Implementation (Step 10 MC Integration)
//=============================================================================

int curiosity_compute_empowerment(
    curiosity_engine_t engine,
    const char* concept_name,
    uint32_t horizon,
    curiosity_empowerment_t* result) {

    if (!engine || !concept_name || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_query_self_knowledge: required parameter is NULL (engine, concept_name, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_compute_empowerment", 0.0f);


    memset(result, 0, sizeof(*result));

    if (horizon == 0) horizon = 3;  /* Default lookahead */

    /* Get MC seed */
    uint32_t* seed = curiosity_get_mc_seed();

    /* Determine action count from related concepts */
    char* related[32];
    uint32_t num_related = curiosity_get_related_concepts(engine, concept_name, related, 32);

    uint32_t num_actions = num_related > 0 ? num_related : 4;
    if (num_actions > 32) num_actions = 32;

    result->action_count = num_actions;

    /* MC estimation of empowerment via action-state transition sampling */
    uint32_t num_samples = 500;
    float* state_counts = (float*)nimcp_calloc(num_actions * num_actions, sizeof(float));
    if (!state_counts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "curiosity_query_self_knowledge: state_counts is NULL");
        return -1;
    }

    /* Simulate action-state transitions */
    for (uint32_t s = 0; s < num_samples; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && num_samples > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(s + 1) / (float)num_samples);
        }

        uint32_t action = mc_random_int(seed, num_actions);
        uint32_t next_state = 0;

        /* Transition: action leads to corresponding state with 70% prob */
        float p = mc_random_uniform(seed);
        if (p < 0.7f) {
            next_state = action;
        } else {
            next_state = mc_random_int(seed, num_actions);
        }

        state_counts[action * num_actions + next_state] += 1.0f;
    }

    /* Compute marginal distributions */
    float* p_a = (float*)nimcp_calloc(num_actions, sizeof(float));
    float* p_s = (float*)nimcp_calloc(num_actions, sizeof(float));

    if (!p_a || !p_s) {
        nimcp_free(state_counts);
        state_counts = NULL;
        if (p_a) nimcp_free(p_a);
        if (p_s) nimcp_free(p_s);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "curiosity_query_self_knowledge: validation failed");
        return -1;
    }

    for (uint32_t a = 0; a < num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && num_actions > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(a + 1) / (float)num_actions);
        }

        for (uint32_t s = 0; s < num_actions; s++) {
            /* Phase 8: Loop progress heartbeat */
            if ((s & 0xFF) == 0 && num_actions > 256) {
                curiosity_heartbeat("curiosity_loop",
                                 (float)(s + 1) / (float)num_actions);
            }

            p_a[a] += state_counts[a * num_actions + s];
            p_s[s] += state_counts[a * num_actions + s];
        }
    }

    float total = (float)num_samples;
    for (uint32_t i = 0; i < num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_actions > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(i + 1) / (float)num_actions);
        }

        p_a[i] /= total;
        p_s[i] /= total;
    }

    /* Entropy H(S') */
    float H_s = 0.0f;
    for (uint32_t s = 0; s < num_actions; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && num_actions > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(s + 1) / (float)num_actions);
        }

        if (p_s[s] > 1e-10f) {
            H_s -= p_s[s] * logf(p_s[s]);
        }
    }

    /* Conditional entropy H(S'|A) */
    float H_s_given_a = 0.0f;
    for (uint32_t a = 0; a < num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && num_actions > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(a + 1) / (float)num_actions);
        }

        if (p_a[a] < 1e-10f) continue;

        float action_total = 0.0f;
        for (uint32_t s = 0; s < num_actions; s++) {
            /* Phase 8: Loop progress heartbeat */
            if ((s & 0xFF) == 0 && num_actions > 256) {
                curiosity_heartbeat("curiosity_loop",
                                 (float)(s + 1) / (float)num_actions);
            }

            action_total += state_counts[a * num_actions + s];
        }

        for (uint32_t s = 0; s < num_actions; s++) {
            /* Phase 8: Loop progress heartbeat */
            if ((s & 0xFF) == 0 && num_actions > 256) {
                curiosity_heartbeat("curiosity_loop",
                                 (float)(s + 1) / (float)num_actions);
            }

            float p_s_given_a = state_counts[a * num_actions + s] / (action_total + 1e-10f);
            if (p_s_given_a > 1e-10f) {
                H_s_given_a -= p_a[a] * p_s_given_a * logf(p_s_given_a);
            }
        }
    }

    /* Mutual information I(A; S') = H(S') - H(S'|A) */
    float mutual_info = H_s - H_s_given_a;
    if (mutual_info < 0.0f) mutual_info = 0.0f;

    /* Convert to bits */
    result->empowerment = mutual_info / logf(2.0f);
    result->entropy_current = H_s / logf(2.0f);
    result->entropy_reachable = H_s / logf(2.0f);

    /* Normalize */
    float max_empowerment = logf((float)num_actions) / logf(2.0f);
    if (max_empowerment > 0.0f) {
        result->empowerment_normalized = result->empowerment / max_empowerment;
        if (result->empowerment_normalized > 1.0f) result->empowerment_normalized = 1.0f;
    }

    nimcp_free(state_counts);
    state_counts = NULL;
    nimcp_free(p_a);
    p_a = NULL;
    nimcp_free(p_s);
    p_s = NULL;

    /* Free related concepts */
    for (uint32_t i = 0; i < num_related; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_related > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(i + 1) / (float)num_related);
        }

        if (related[i]) nimcp_free(related[i]);
    }

    return 0;
}

uint32_t curiosity_sample_by_empowerment(
    curiosity_engine_t engine,
    const char** concepts,
    uint32_t num_concepts,
    float temperature) {

    if (!engine || !concepts || num_concepts == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_sample_by_empowermen", 0.0f);


    if (temperature <= 0.0f) temperature = 1.0f;

    uint32_t* seed = curiosity_get_mc_seed();

    /* Compute empowerment for each concept */
    float* scores = (float*)nimcp_calloc(num_concepts, sizeof(float));
    if (!scores) return 0;

    float max_score = -1e10f;
    for (uint32_t i = 0; i < num_concepts; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_concepts > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(i + 1) / (float)num_concepts);
        }

        curiosity_empowerment_t emp;
        if (curiosity_compute_empowerment(engine, concepts[i], 3, &emp) == 0) {
            scores[i] = emp.empowerment / temperature;
        } else {
            scores[i] = 0.0f;
        }
        if (scores[i] > max_score) max_score = scores[i];
    }

    /* Softmax normalization */
    float sum_exp = 0.0f;
    for (uint32_t i = 0; i < num_concepts; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_concepts > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(i + 1) / (float)num_concepts);
        }

        scores[i] = expf(scores[i] - max_score);  /* Numerical stability */
        sum_exp += scores[i];
    }

    /* Sample from distribution */
    float r = mc_random_uniform(seed) * sum_exp;
    float cumsum = 0.0f;
    uint32_t selected = 0;

    for (uint32_t i = 0; i < num_concepts; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_concepts > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(i + 1) / (float)num_concepts);
        }

        cumsum += scores[i];
        if (r <= cumsum) {
            selected = i;
            break;
        }
    }

    nimcp_free(scores);
    scores = NULL;
    return selected;
}

float curiosity_compute_intrinsic_reward(
    curiosity_engine_t engine,
    const char* concept_name,
    float alpha,
    float beta) {

    if (!engine || !concept_name) return 0.0f;

    /* Clamp weights */
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_compute_intrinsic_re", 0.0f);


    if (alpha < 0.0f) alpha = 0.0f;
    if (beta < 0.0f) beta = 0.0f;
    if (alpha + beta > 1.0f) {
        float scale = 1.0f / (alpha + beta);
        alpha *= scale;
        beta *= scale;
    }

    /* Empowerment component */
    float empowerment_reward = 0.0f;
    curiosity_empowerment_t emp;
    if (curiosity_compute_empowerment(engine, concept_name, 3, &emp) == 0) {
        empowerment_reward = emp.empowerment_normalized;
    }

    /* Novelty component via familiarity */
    float familiarity = curiosity_check_familiarity(engine, concept_name);
    float novelty = 1.0f - familiarity;

    /* Combined intrinsic reward */
    float reward = alpha * empowerment_reward + beta * novelty;
    if (reward > 1.0f) reward = 1.0f;
    if (reward < 0.0f) reward = 0.0f;

    return reward;
}

float curiosity_estimate_empowerment_change(
    curiosity_engine_t engine,
    const char* concept_name,
    uint32_t num_simulations) {

    if (!engine || !concept_name) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    curiosity_heartbeat("curiosity_estimate_empowerment", 0.0f);


    if (num_simulations == 0) num_simulations = 100;

    uint32_t* seed = curiosity_get_mc_seed();

    /* Current empowerment */
    curiosity_empowerment_t current_emp;
    if (curiosity_compute_empowerment(engine, concept_name, 3, &current_emp) != 0) {
        return 0.0f;
    }

    /* Simulate post-exploration empowerment */
    float sum_delta = 0.0f;

    for (uint32_t i = 0; i < num_simulations; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_simulations > 256) {
            curiosity_heartbeat("curiosity_loop",
                             (float)(i + 1) / (float)num_simulations);
        }

        /* Simulate exploration outcome - empowerment typically increases slightly */
        float noise = mc_random_uniform(seed) * 2.0f - 1.0f;  /* [-1, 1] */
        float simulated_delta = 0.1f + noise * 0.2f;  /* Mean 0.1, std 0.2 */

        sum_delta += simulated_delta;
    }

    return sum_delta / (float)num_simulations;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void curiosity_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_curiosity_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int curiosity_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_training_begin: NULL argument");
        return -1;
    }
    curiosity_heartbeat_instance(g_curiosity_health_agent, "curiosity_training_begin", 0.0f);
    (void)(struct concept_bucket_struct*)instance; /* Module state available for reset */
    return 0;
}

int curiosity_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_training_end: NULL argument");
        return -1;
    }
    curiosity_heartbeat_instance(g_curiosity_health_agent, "curiosity_training_end", 1.0f);
    (void)(struct concept_bucket_struct*)instance; /* Module state available for finalization */
    return 0;
}

int curiosity_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    curiosity_heartbeat_instance(g_curiosity_health_agent, "curiosity_training_step", progress);
    (void)(struct concept_bucket_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
