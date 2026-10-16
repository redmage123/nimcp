//=============================================================================
// nimcp_creative_memory_bridge.h - Creative-Memory Integration Bridge
//=============================================================================
/**
 * @file nimcp_creative_memory_bridge.h
 * @brief Bridge connecting creative system to memory/hippocampus
 *
 * WHAT: Manages storage and retrieval of artistic experiences
 * WHY:  Learning from and building upon past artistic experiences
 * HOW:  Interface to hippocampus for creative episodic and semantic memory
 *
 * MEMORY TYPES:
 * - Episodic: Specific artistic experiences (when I heard X, saw Y)
 * - Semantic: General artistic knowledge (styles, techniques, artists)
 * - Procedural: Creative skills and techniques
 * - Preferential: Personal aesthetic preferences
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_CREATIVE_MEMORY_BRIDGE_H
#define NIMCP_CREATIVE_MEMORY_BRIDGE_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct creative_memory_bridge creative_memory_bridge_t;

//=============================================================================
// Creative Memory Types
//=============================================================================

/**
 * @brief Types of creative memories
 */
typedef enum {
    CREATIVE_MEM_EPISODIC = 0,     /**< Specific experiences */
    CREATIVE_MEM_SEMANTIC,          /**< General knowledge */
    CREATIVE_MEM_PROCEDURAL,        /**< Skills and techniques */
    CREATIVE_MEM_PREFERENTIAL       /**< Aesthetic preferences */
} creative_memory_type_t;

/**
 * @brief Episodic creative memory (specific experience)
 */
typedef struct {
    uint64_t memory_id;             /**< Unique ID */
    art_modality_t modality;        /**< Art modality */
    char work_title[128];           /**< Title of work experienced */
    char artist_name[64];           /**< Artist/creator name */
    aesthetic_evaluation_t evaluation; /**< How it was evaluated */
    aesthetic_emotional_response_t emotions; /**< Emotional response */
    uint64_t experience_time;       /**< When experienced (unix timestamp) */
    float salience;                 /**< [0-1] How memorable */
    float personal_significance;    /**< [0-1] Personal meaning */
    uint32_t recall_count;          /**< Times recalled */
    uint64_t last_recall_time;      /**< Last recall timestamp */
} creative_episodic_memory_t;

/**
 * @brief Semantic creative memory (general knowledge)
 */
typedef struct {
    uint64_t memory_id;             /**< Unique ID */
    art_modality_t modality;        /**< Art modality */
    char concept[64];               /**< Concept name */
    char description[512];          /**< Description */
    style_embedding_t style;        /**< Associated style embedding */
    float confidence;               /**< [0-1] Confidence in knowledge */
    uint64_t acquisition_time;      /**< When learned */
    uint64_t last_reinforcement;    /**< Last reinforcement time */
    uint32_t reinforcement_count;   /**< Times reinforced */
} creative_semantic_memory_t;

/**
 * @brief Preferential memory (aesthetic preference)
 */
typedef struct {
    art_modality_t modality;        /**< Art modality */
    int32_t archetype_id;           /**< Preferred archetype (or -1) */
    float preference_strength;      /**< [-1,+1] Dislike to like */
    float certainty;                /**< [0-1] How certain of preference */
    uint32_t sample_count;          /**< Number of experiences forming this */
    uint64_t last_update;           /**< Last update time */
} creative_preference_t;

/**
 * @brief Memory query result
 */
typedef struct {
    void* memories;                 /**< Array of memories (type depends on query) */
    uint32_t count;                 /**< Number of results */
    creative_memory_type_t type;    /**< Type of memories returned */
    float avg_relevance;            /**< Average relevance score */
} creative_memory_query_result_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Memory bridge configuration
 */
typedef struct {
    /* Capacity limits */
    uint32_t max_episodic_memories;     /**< Max episodic memories */
    uint32_t max_semantic_memories;     /**< Max semantic memories */
    uint32_t max_preferences;           /**< Max preference entries */

    /* Consolidation settings */
    float salience_threshold;           /**< Min salience to store */
    bool enable_consolidation;          /**< Enable sleep consolidation */
    float forgetting_rate;              /**< Rate of forgetting (per day) */

    /* Retrieval settings */
    uint32_t max_retrieval_results;     /**< Max results per query */
    float relevance_threshold;          /**< Min relevance to return */

    /* Integration */
    bool use_hippocampus;               /**< Use external hippocampus */
    bool use_semantic_memory;           /**< Use semantic memory system */
} creative_memory_bridge_config_t;

/**
 * @brief Initialize config with defaults
 */
void creative_memory_bridge_config_defaults(creative_memory_bridge_config_t* config);

//=============================================================================
// Bridge Structure
//=============================================================================

/**
 * @brief Creative-memory bridge
 */
struct creative_memory_bridge {
    creative_memory_bridge_config_t config;

    /* Internal storage (if not using external systems) */
    creative_episodic_memory_t* episodic_memories;
    uint32_t episodic_count;
    uint32_t episodic_capacity;

    creative_semantic_memory_t* semantic_memories;
    uint32_t semantic_count;
    uint32_t semantic_capacity;

    creative_preference_t* preferences;
    uint32_t preference_count;
    uint32_t preference_capacity;

    /* External integration */
    void* hippocampus;                  /**< Hippocampus pointer */
    void* semantic_memory_system;       /**< Semantic memory system pointer */

    /* Statistics */
    uint64_t total_stores;
    uint64_t total_retrievals;
    float avg_retrieval_time_ms;
    uint64_t last_consolidation_time;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create memory bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge or NULL on error
 */
creative_memory_bridge_t* creative_memory_bridge_create(
    const creative_memory_bridge_config_t* config);

/**
 * @brief Destroy memory bridge
 *
 * @param bridge Bridge to destroy
 */
void creative_memory_bridge_destroy(creative_memory_bridge_t* bridge);

//=============================================================================
// Storage API
//=============================================================================

/**
 * @brief Store episodic memory of artistic experience
 *
 * @param bridge Bridge
 * @param modality Art modality
 * @param work_title Title of work
 * @param artist_name Artist name
 * @param eval Aesthetic evaluation
 * @param emotions Emotional response
 * @param salience How memorable [0-1]
 * @return Memory ID on success, 0 on failure
 */
uint64_t creative_memory_store_episodic(creative_memory_bridge_t* bridge,
                                         art_modality_t modality,
                                         const char* work_title,
                                         const char* artist_name,
                                         const aesthetic_evaluation_t* eval,
                                         const aesthetic_emotional_response_t* emotions,
                                         float salience);

/**
 * @brief Store semantic knowledge about art
 *
 * @param bridge Bridge
 * @param modality Art modality
 * @param concept Concept name
 * @param description Description
 * @param style Associated style (optional)
 * @return Memory ID on success, 0 on failure
 */
uint64_t creative_memory_store_semantic(creative_memory_bridge_t* bridge,
                                         art_modality_t modality,
                                         const char* concept,
                                         const char* description,
                                         const style_embedding_t* style);

/**
 * @brief Update aesthetic preference
 *
 * @param bridge Bridge
 * @param modality Art modality
 * @param archetype_id Archetype ID (or -1 for general modality)
 * @param preference Preference value [-1,+1]
 * @return 0 on success, -1 on error
 */
int creative_memory_update_preference(creative_memory_bridge_t* bridge,
                                       art_modality_t modality,
                                       int32_t archetype_id,
                                       float preference);

//=============================================================================
// Retrieval API
//=============================================================================

/**
 * @brief Recall episodic memories by similarity
 *
 * @param bridge Bridge
 * @param style Query style
 * @param max_results Maximum results
 * @param out Output array (caller allocated)
 * @return Number of memories retrieved
 */
uint32_t creative_memory_recall_by_style(creative_memory_bridge_t* bridge,
                                          const style_embedding_t* style,
                                          uint32_t max_results,
                                          creative_episodic_memory_t* out);

/**
 * @brief Recall episodic memories by emotion
 *
 * @param bridge Bridge
 * @param emotion Query emotion
 * @param max_results Maximum results
 * @param out Output array (caller allocated)
 * @return Number of memories retrieved
 */
uint32_t creative_memory_recall_by_emotion(creative_memory_bridge_t* bridge,
                                            const aesthetic_emotional_response_t* emotion,
                                            uint32_t max_results,
                                            creative_episodic_memory_t* out);

/**
 * @brief Recall semantic knowledge by concept
 *
 * @param bridge Bridge
 * @param concept Concept query
 * @param max_results Maximum results
 * @param out Output array (caller allocated)
 * @return Number of memories retrieved
 */
uint32_t creative_memory_recall_semantic(creative_memory_bridge_t* bridge,
                                          const char* concept,
                                          uint32_t max_results,
                                          creative_semantic_memory_t* out);

/**
 * @brief Get preference for archetype
 *
 * @param bridge Bridge
 * @param modality Art modality
 * @param archetype_id Archetype ID
 * @param out Output preference
 * @return 0 on success, -1 if no preference exists
 */
int creative_memory_get_preference(const creative_memory_bridge_t* bridge,
                                    art_modality_t modality,
                                    int32_t archetype_id,
                                    creative_preference_t* out);

/**
 * @brief Calculate familiarity with content
 *
 * Based on similarity to stored memories
 *
 * @param bridge Bridge
 * @param style Content style
 * @return Familiarity score [0-1]
 */
float creative_memory_familiarity(const creative_memory_bridge_t* bridge,
                                   const style_embedding_t* style);

//=============================================================================
// Maintenance API
//=============================================================================

/**
 * @brief Run consolidation (move to long-term memory)
 *
 * @param bridge Bridge
 * @return Number of memories consolidated
 */
uint32_t creative_memory_consolidate(creative_memory_bridge_t* bridge);

/**
 * @brief Apply forgetting to memories
 *
 * @param bridge Bridge
 * @param elapsed_days Days since last forgetting
 * @return Number of memories affected
 */
uint32_t creative_memory_forget(creative_memory_bridge_t* bridge,
                                 float elapsed_days);

/**
 * @brief Reinforce memory by recall
 *
 * @param bridge Bridge
 * @param memory_id Memory ID
 * @param memory_type Memory type
 * @return 0 on success, -1 on error
 */
int creative_memory_reinforce(creative_memory_bridge_t* bridge,
                               uint64_t memory_id,
                               creative_memory_type_t memory_type);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Set hippocampus for external storage
 *
 * @param bridge Bridge
 * @param hippocampus Hippocampus pointer
 */
void creative_memory_set_hippocampus(creative_memory_bridge_t* bridge,
                                      void* hippocampus);

/**
 * @brief Set semantic memory system
 *
 * @param bridge Bridge
 * @param semantic Semantic memory system pointer
 */
void creative_memory_set_semantic_system(creative_memory_bridge_t* bridge,
                                          void* semantic);

/**
 * @brief Sync with external systems
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int creative_memory_sync_external(creative_memory_bridge_t* bridge);

//=============================================================================
// Query Result Cleanup
//=============================================================================

/**
 * @brief Free query result resources
 *
 * @param result Query result to free
 */
void creative_memory_query_result_free(creative_memory_query_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CREATIVE_MEMORY_BRIDGE_H */
