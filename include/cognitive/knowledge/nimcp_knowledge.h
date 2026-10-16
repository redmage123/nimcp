//=============================================================================
// nimcp_knowledge.h - Multi-Domain Knowledge Acquisition
//=============================================================================

#ifndef NIMCP_KNOWLEDGE_H
#define NIMCP_KNOWLEDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "core/brain/nimcp_brain.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "common/nimcp_export.h"
#include "utils/geometry/nimcp_hyperbolic.h"  // Part B1.1: Hyperbolic embeddings

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_knowledge.h
 * @brief Multi-domain knowledge acquisition like human learning
 *
 * CORE DIRECTIVE: All knowledge acquisition serves the ultimate goal of
 * improving the human condition by fostering understanding, empathy,
 * wisdom, and the betterment of humanity.
 *
 * Learn incrementally across domains:
 * - Literature (stories, poetry, narrative)
 * - Art (visual, music, aesthetics)
 * - Ethics (right/wrong, Golden Rule, values)
 * - History (events, people, cause/effect)
 * - Science (natural world, cause/effect)
 * - Social (relationships, culture, communication)
 * - Technical (how things work, skills)
 *
 * Philosophy: Learn like a human, not a database
 * - Start with simple concepts
 * - Build on previous knowledge
 * - Learn through stories and examples
 * - Understand context and meaning
 * - Connect across domains
 * - Apply knowledge to improve human wellbeing
 * - No massive pre-training
 */

//=============================================================================
// Knowledge Domains
//=============================================================================

/**
 * @brief Knowledge domains (like school subjects + life experience)
 */
typedef enum {
    KNOWLEDGE_DOMAIN_LANGUAGE,    /**< Words, grammar, communication */
    KNOWLEDGE_DOMAIN_LITERATURE,  /**< Stories, poetry, books */
    KNOWLEDGE_DOMAIN_ART,         /**< Visual art, music, creativity */
    KNOWLEDGE_DOMAIN_ETHICS,      /**< Right/wrong, values, morality */
    KNOWLEDGE_DOMAIN_HISTORY,     /**< Past events, people, civilizations */
    KNOWLEDGE_DOMAIN_SCIENCE,     /**< Natural world, physics, biology */
    KNOWLEDGE_DOMAIN_MATHEMATICS, /**< Numbers, patterns, logic */
    KNOWLEDGE_DOMAIN_SOCIAL,      /**< Relationships, society, culture */
    KNOWLEDGE_DOMAIN_TECHNICAL,   /**< How things work, skills */
    KNOWLEDGE_DOMAIN_PHILOSOPHY,  /**< Meaning, existence, thinking */
    KNOWLEDGE_DOMAIN_GENERAL      /**< General world knowledge */
} knowledge_domain_t;

/**
 * @brief Knowledge system handle
 */
typedef struct knowledge_system_struct* knowledge_system_t;

//=============================================================================
// Knowledge Representation
//=============================================================================

/**
 * @brief A piece of knowledge
 *
 * PART B1.1: Extended with hyperbolic embeddings for hierarchical representation
 * - Hyperbolic space naturally represents trees/hierarchies
 * - 200x memory reduction: 5D hyperbolic vs 1000D Euclidean
 * - Preserves parent-child relationships via hyperbolic distance
 */
typedef struct {
    char concept_name[256];    /**< Main concept (renamed from 'concept' for C++20 compatibility) */
    knowledge_domain_t domain; /**< Which domain */
    char definition[1024];     /**< What it means */
    char context[512];         /**< When/where/why relevant */
    char** examples;           /**< Example instances */
    uint32_t num_examples;
    char** related_concepts; /**< Related ideas */
    uint32_t num_related;
    float confidence;             /**< How well understood (0-1) */
    uint64_t learned_timestamp;   /**< When learned */
    uint32_t reinforcement_count; /**< How many times reinforced */
    char confidence_key[24];      /**< B-tree key: formatted "%08.6f_%05u" — 24 bytes fits max-width uint32 safely */

    // PART B1.1: Hyperbolic Knowledge Embeddings (geometric representation)
    poincare_point_t *hyperbolic_embedding;  /**< Position in Poincaré ball (hierarchical) */
    float *euclidean_embedding;              /**< Fallback Euclidean embedding (backward compat) */
    uint32_t embedding_dim;                  /**< Dimension of embedding (5-10 for hyperbolic, 100-1000 for Euclidean) */
    bool use_hyperbolic;                     /**< true = use hyperbolic, false = use Euclidean */
    float hierarchical_level;                /**< Distance from root (0 = root concept, higher = more specific) */
    uint32_t parent_index;                   /**< Index of parent concept in hierarchy (UINT32_MAX = root) */
} knowledge_item_t;

/**
 * @brief A story or narrative (key to human learning!)
 */
typedef struct {
    char title[256];
    char author[128];
    char summary[2048]; /**< Story summary */
    char** characters;  /**< Who's in it */
    uint32_t num_characters;
    char** themes; /**< What it's about (love, courage, etc.) */
    uint32_t num_themes;
    char** moral_lessons; /**< What we learn from it */
    uint32_t num_lessons;
    knowledge_domain_t primary_domain;
    char cultural_context[256]; /**< Where/when from */
} narrative_knowledge_t;

/**
 * @brief Artistic/aesthetic knowledge
 */
typedef struct {
    char work_title[256];
    char creator[128];
    char medium[64]; /**< painting, sculpture, music, etc. */
    char description[1024];
    char** aesthetic_qualities; /**< beautiful, haunting, joyful, etc. */
    uint32_t num_qualities;
    char emotional_impact[256]; /**< How it makes you feel */
    char historical_significance[512];
} aesthetic_knowledge_t;

/**
 * @brief Historical knowledge
 */
typedef struct {
    char event_name[256];
    uint64_t timestamp_year; /**< Approximate year */
    char** key_people;
    uint32_t num_people;
    char causes[1024];      /**< Why it happened */
    char effects[1024];     /**< What resulted */
    char significance[512]; /**< Why it matters */
    char** related_events;
    uint32_t num_related_events;
} historical_knowledge_t;

//=============================================================================
// Knowledge System Creation
//=============================================================================

/**
 * @brief Create knowledge system
 *
 * @param learner_name Name for learner
 * @return Knowledge system or NULL on error
 */
knowledge_system_t knowledge_system_create(const char* learner_name);

/**
 * @brief Destroy knowledge system
 *
 * @param system System to destroy
 */
void knowledge_system_destroy(knowledge_system_t system);

//=============================================================================
// Learning from Different Sources
//=============================================================================

/**
 * @brief Learn from text
 *
 * Read and understand text incrementally
 *
 * @param system Knowledge system
 * @param text Text to learn from
 * @param domain Which domain (or GENERAL)
 * @return Number of concepts learned
 */
uint32_t knowledge_learn_from_text(knowledge_system_t system, const char* text,
                                   knowledge_domain_t domain);

/**
 * @brief Learn from story/narrative
 *
 * Stories are how humans learn values, ethics, social behavior
 *
 * @param system Knowledge system
 * @param story Story to learn from
 * @return true on success
 */
bool knowledge_learn_from_story(knowledge_system_t system, const narrative_knowledge_t* story);

/**
 * @brief Learn from art/aesthetics
 *
 * Learn about beauty, emotion, creativity
 *
 * @param system Knowledge system
 * @param art_piece Art to learn from
 * @return true on success
 */
bool knowledge_learn_from_art(knowledge_system_t system, const aesthetic_knowledge_t* art_piece);

/**
 * @brief Learn from historical event
 *
 * Understand cause and effect, human nature, civilization
 *
 * @param system Knowledge system
 * @param event Historical event
 * @return true on success
 */
bool knowledge_learn_from_history(knowledge_system_t system, const historical_knowledge_t* event);

/**
 * @brief Learn from conversation
 *
 * Social learning - learn by talking/listening
 *
 * @param system Knowledge system
 * @param dialogue Conversation text
 * @param participants Who's talking
 * @param num_participants Number of participants
 * @return Number of concepts learned
 */
uint32_t knowledge_learn_from_conversation(knowledge_system_t system, const char* dialogue,
                                           const char** participants, uint32_t num_participants);

/**
 * @brief Learn from demonstration
 *
 * Learn by watching (like infant learning to walk)
 *
 * @param system Knowledge system
 * @param what_demonstrated What's being shown
 * @param steps Steps observed
 * @param num_steps Number of steps
 * @return true on success
 */
bool knowledge_learn_from_demonstration(knowledge_system_t system, const char* what_demonstrated,
                                        const char** steps, uint32_t num_steps);

//=============================================================================
// Knowledge Retrieval & Understanding
//=============================================================================

/**
 * @brief Retrieve knowledge about concept
 *
 * @param system Knowledge system
 * @param concept Concept to retrieve
 * @param item Output knowledge item
 * @return true if found
 */
bool knowledge_retrieve(knowledge_system_t system, const char* concept_str, knowledge_item_t* item);

/**
 * @brief Understand concept in context
 *
 * Not just definition - understand meaning and use
 *
 * @param system Knowledge system
 * @param concept Concept to understand
 * @param context Context it appears in
 * @param explanation Output explanation
 * @param max_length Max explanation length
 * @return Length of explanation
 */
uint32_t knowledge_understand(knowledge_system_t system, const char* concept_str, const char* context,
                              char* explanation, uint32_t max_length);

/**
 * @brief Explain concept simply
 *
 * Explain like you would to a child
 *
 * @param system Knowledge system
 * @param concept Concept to explain
 * @param target_age Target age level (3-18)
 * @param explanation Output simple explanation
 * @param max_length Max length
 * @return Length of explanation
 */
uint32_t knowledge_explain_simply(knowledge_system_t system, const char* concept_str,
                                  uint32_t target_age, char* explanation, uint32_t max_length);

//=============================================================================
// Cross-Domain Learning
//=============================================================================

/**
 * @brief Find connections across domains
 *
 * Like learning that "Romeo & Juliet" connects literature + history + ethics
 *
 * @param system Knowledge system
 * @param concept Central concept
 * @param connections Output array for cross-domain connections
 * @param max_connections Maximum connections
 * @return Number of connections found
 */
uint32_t knowledge_find_connections(knowledge_system_t system, const char* concept_str,
                                    knowledge_item_t* connections, uint32_t max_connections);

/**
 * @brief Apply knowledge from one domain to another
 *
 * Transfer learning: use story lessons in real situations
 *
 * @param system Knowledge system
 * @param source_domain Where knowledge comes from
 * @param target_domain Where to apply it
 * @param situation Current situation
 * @param application Output suggested application
 * @param max_length Max length
 * @return true if transfer successful
 */
bool knowledge_transfer_learning(knowledge_system_t system, knowledge_domain_t source_domain,
                                 knowledge_domain_t target_domain, const char* situation,
                                 char* application, uint32_t max_length);

//=============================================================================
// Incremental Building
//=============================================================================

/**
 * @brief Build on existing knowledge
 *
 * Learn new concept by connecting to what's already known
 *
 * @param system Knowledge system
 * @param new_concept New thing to learn
 * @param based_on_concept What it's similar to
 * @param differences How it differs
 * @return true on success
 */
bool knowledge_build_on(knowledge_system_t system, const char* new_concept,
                        const char* based_on_concept, const char* differences);

/**
 * @brief Reinforce existing knowledge
 *
 * Strengthen understanding through repetition (like practicing)
 *
 * @param system Knowledge system
 * @param concept Concept to reinforce
 * @param new_example New example of concept
 * @return true on success
 */
bool knowledge_reinforce(knowledge_system_t system, const char* concept_str, const char* new_example);

//=============================================================================
// Knowledge Organization
//=============================================================================

/**
 * @brief Organize knowledge into mental models
 *
 * Create structured understanding (like mental schemas)
 *
 * @param system Knowledge system
 * @param domain Domain to organize
 * @return true on success
 */
bool knowledge_organize_domain(knowledge_system_t system, knowledge_domain_t domain);

/**
 * @brief Get knowledge map
 *
 * Visual representation of what's known and connections
 *
 * @param system Knowledge system
 * @param domain Specific domain or all
 * @param map_data Output graph data
 * @param max_nodes Maximum nodes in map
 * @return Number of nodes in map
 */
uint32_t knowledge_get_map(knowledge_system_t system, knowledge_domain_t domain,
                           void* map_data,  // Would be graph structure
                           uint32_t max_nodes);

//=============================================================================
// Reading & Sources
//=============================================================================

/**
 * @brief Read book/document incrementally
 *
 * Like a human reading a book page by page
 *
 * @param system Knowledge system
 * @param book_title Title of book
 * @param book_text Full text of book
 * @param reading_speed Pages per session
 * @return Number of concepts learned
 */
uint32_t knowledge_read_book(knowledge_system_t system, const char* book_title,
                             const char* book_text, uint32_t reading_speed);

/**
 * @brief Process reading progress
 *
 * Continue reading where left off
 *
 * @param system Knowledge system
 * @param book_title Book being read
 * @param continue_reading Continue from bookmark
 * @return Progress percentage (0-100)
 */
uint32_t knowledge_continue_reading(knowledge_system_t system, const char* book_title,
                                    bool continue_reading);

/**
 * @brief Get reading recommendations
 *
 * Suggest what to read next based on knowledge gaps
 *
 * @param system Knowledge system
 * @param domain Domain to explore
 * @param recommendations Output array
 * @param max_recommendations Maximum to suggest
 * @return Number of recommendations
 */
uint32_t knowledge_get_reading_list(knowledge_system_t system, knowledge_domain_t domain,
                                    char** recommendations, uint32_t max_recommendations);

//=============================================================================
// Knowledge Assessment
//=============================================================================

/**
 * @brief Knowledge coverage statistics
 */
typedef struct {
    knowledge_domain_t domain;
    uint32_t concepts_known;
    uint32_t estimated_total;  /**< Estimate of domain size */
    float coverage_percentage; /**< % of domain known */
    float avg_confidence;      /**< Average understanding */
    char gaps[5][256];         /**< Top knowledge gaps */
    uint32_t num_gaps;
} domain_knowledge_t;

/**
 * @brief Assess knowledge in domain
 *
 * @param system Knowledge system
 * @param domain Domain to assess
 * @param assessment Output assessment
 * @return true on success
 */
bool knowledge_assess_domain(knowledge_system_t system, knowledge_domain_t domain,
                             domain_knowledge_t* assessment);

/**
 * @brief Get overall knowledge summary
 *
 * @param system Knowledge system
 * @param all_domains Output array for all domain assessments
 * @param max_domains Maximum domains
 * @return Number of domains assessed
 */
uint32_t knowledge_get_summary(knowledge_system_t system, domain_knowledge_t* all_domains,
                               uint32_t max_domains);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get domain name
 */
const char* knowledge_domain_name(knowledge_domain_t domain);

/**
 * @brief Print knowledge item
 */
void knowledge_print_item(const knowledge_item_t* item);

/**
 * @brief Print domain assessment
 */
void knowledge_print_assessment(const domain_knowledge_t* assessment);

/**
 * @brief Save knowledge to file
 *
 * Persistent memory (like human long-term memory)
 *
 * @param system Knowledge system
 * @param filepath Path to save
 * @return true on success
 */
bool knowledge_save(knowledge_system_t system, const char* filepath);

/**
 * @brief Load knowledge from file
 *
 * Resume learning from previous session
 *
 * @param filepath Path to load from
 * @return Knowledge system or NULL on error
 */
knowledge_system_t knowledge_load(const char* filepath);

//=============================================================================
// Testing API
//=============================================================================

/**
 * @brief Add knowledge item directly (for testing)
 *
 * @param system Knowledge system
 * @param item Item to add
 * @return true on success
 */
bool knowledge_add_item(knowledge_system_t system, const knowledge_item_t* item);

//=============================================================================
// Symbolic Logic Integration (Phase 11: Logic Wiring)
//=============================================================================

// Forward declaration for symbolic logic integration
typedef struct symbolic_logic symbolic_logic_t;

/**
 * @brief Add knowledge item to symbolic logic as facts
 *
 * WHAT: Convert knowledge concepts to logical predicates (IsA, Concept)
 * WHY:  Enable logical reasoning over knowledge graph (audit recommendation)
 * HOW:  Create logic clauses from knowledge relationships
 *
 * EXAMPLE:
 *   Concept: "cat" with related: "animal"
 *   → Logic facts: IsA(cat, animal), Concept(cat)
 *
 * @param logic Symbolic logic engine
 * @param item Knowledge item to convert to logic facts
 * @return Number of facts added to logic engine
 *
 * THREAD-SAFE: No (requires external synchronization)
 * COMPLEXITY: O(k) where k = number of related concepts
 */
uint32_t knowledge_add_to_symbolic_logic(
    symbolic_logic_t* logic,
    const knowledge_item_t* item
);

//=============================================================================
// B-TREE INDEXED QUERIES (New in v2.5.1)
//=============================================================================

/**
 * @brief Get knowledge items by confidence range
 *
 * WHAT: Query items within confidence range using B-tree
 * WHY: Efficient queries for well/poorly understood concepts
 * HOW: B-tree indexed by confidence enables O(log n + k) range queries
 *
 * USE CASES:
 * - Find well-understood concepts (0.8, 1.0)
 * - Find weak knowledge needing reinforcement (0.0, 0.4)
 * - Find moderately confident items (0.4, 0.7)
 *
 * @param system Knowledge system
 * @param min_confidence Minimum confidence (0.0-1.0)
 * @param max_confidence Maximum confidence (0.0-1.0)
 * @param results_out Output array (caller must free)
 * @return Number of items in range
 *
 * COMPLEXITY: O(log n + k) where k = results in range
 * THREAD-SAFE: Yes
 */
uint32_t knowledge_get_by_confidence_range(knowledge_system_t system,
                                            float min_confidence,
                                            float max_confidence,
                                            knowledge_item_t** results_out);

/**
 * @brief Get all knowledge ordered by confidence
 *
 * WHAT: Return all items sorted by confidence (low to high)
 * WHY: Review knowledge progression
 * HOW: B-tree in-order traversal provides sorted output
 *
 * @param system Knowledge system
 * @param results_out Output array (caller must free)
 * @return Number of items
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 */
uint32_t knowledge_get_all_ordered_by_confidence(knowledge_system_t system,
                                                   knowledge_item_t** results_out);

//=============================================================================
// PART B1.1: Hyperbolic Knowledge Embedding API
//=============================================================================

/**
 * @brief Initialize hyperbolic embedding for a knowledge item
 *
 * WHAT: Creates hyperbolic embedding in Poincaré ball
 * WHY: Enables 200x memory reduction for hierarchical knowledge
 * HOW: Places concept at appropriate radius based on hierarchical level
 *
 * @param item Knowledge item to initialize
 * @param dim Embedding dimension (typically 5)
 * @param hierarchical_level Distance from root
 * @param parent Parent concept (NULL if root)
 * @return true on success
 */
bool knowledge_init_hyperbolic_embedding(knowledge_item_t *item, uint32_t dim,
                                         float hierarchical_level,
                                         const knowledge_item_t *parent);

/**
 * @brief Compute hyperbolic distance between two knowledge items
 *
 * @param item1 First item
 * @param item2 Second item
 * @return Hyperbolic distance, or -1.0 if no embeddings
 */
float knowledge_hyperbolic_distance(const knowledge_item_t *item1,
                                    const knowledge_item_t *item2);

/**
 * @brief Find k nearest neighbors in hyperbolic space
 *
 * @param system Knowledge system
 * @param query_item Query concept
 * @param k Number of neighbors
 * @param neighbors_out Output array [k]
 * @param distances_out Output distances [k] (can be NULL)
 * @return Number found
 */
uint32_t knowledge_hyperbolic_knn(knowledge_system_t system,
                                  const knowledge_item_t *query_item,
                                  uint32_t k,
                                  knowledge_item_t **neighbors_out,
                                  float *distances_out);

/**
 * @brief Update embedding via Riemannian SGD
 *
 * @param item Item to update
 * @param euclidean_gradient Gradient [dim]
 * @param learning_rate Step size
 * @return true on success
 */
bool knowledge_hyperbolic_sgd_step(knowledge_item_t *item,
                                   const float *euclidean_gradient,
                                   float learning_rate);

/**
 * @brief Learn hyperbolic embeddings for all items
 *
 * @param system Knowledge system
 * @param num_epochs Optimization iterations
 * @param learning_rate Initial LR
 * @return Final loss
 */
float knowledge_learn_hyperbolic_embeddings(knowledge_system_t system,
                                            uint32_t num_epochs,
                                            float learning_rate);

/**
 * @brief Convert Euclidean to hyperbolic
 *
 * @param item Item with Euclidean embedding
 * @param target_dim Target dimension
 * @return true on success
 */
bool knowledge_euclidean_to_hyperbolic(knowledge_item_t *item, uint32_t target_dim);

/**
 * @brief Get hierarchical path to root
 *
 * @param system Knowledge system
 * @param concept_name Concept to trace
 * @param path_out Output array
 * @param max_depth Maximum length
 * @return Path length
 */
uint32_t knowledge_get_hierarchical_path(knowledge_system_t system,
                                         const char *concept_name,
                                         knowledge_item_t **path_out,
                                         uint32_t max_depth);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_KNOWLEDGE_H
