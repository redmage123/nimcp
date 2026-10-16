//=============================================================================
// nimcp_creative_knowledge_bridge.h - Creative Knowledge Graph Integration
//=============================================================================
/**
 * @file nimcp_creative_knowledge_bridge.h
 * @brief Integrates creative system with knowledge graph for art knowledge
 *
 * WHAT: Provides access to structured art/culture knowledge
 * WHY:  Enable informed creative decisions based on art history
 * HOW:  Interface to brain's knowledge graph with art-specific queries
 *
 * KNOWLEDGE DOMAINS:
 * - Art History: Movements, periods, key works
 * - Artists: Biographies, styles, influences
 * - Techniques: Methods, materials, processes
 * - Theory: Aesthetic theories, critical frameworks
 * - Context: Historical, cultural, social context
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_CREATIVE_KNOWLEDGE_BRIDGE_H
#define NIMCP_CREATIVE_KNOWLEDGE_BRIDGE_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Knowledge Entity Types
//=============================================================================

/**
 * @brief Art knowledge entity types
 */
typedef enum {
    ART_ENTITY_ARTIST = 0,         /**< Artist/creator */
    ART_ENTITY_WORK,               /**< Specific artwork */
    ART_ENTITY_MOVEMENT,           /**< Art movement */
    ART_ENTITY_PERIOD,             /**< Historical period */
    ART_ENTITY_TECHNIQUE,          /**< Technique/method */
    ART_ENTITY_CONCEPT,            /**< Theoretical concept */
    ART_ENTITY_GENRE,              /**< Genre classification */
    ART_ENTITY_VENUE,              /**< Museum/gallery/venue */
    ART_ENTITY_AWARD,              /**< Award/recognition */
    ART_ENTITY_INFLUENCE,          /**< Influence relationship */
    ART_ENTITY_COUNT
} art_entity_type_t;

/**
 * @brief Art knowledge relationship types
 */
typedef enum {
    ART_REL_CREATED_BY = 0,        /**< Work created by artist */
    ART_REL_INFLUENCED_BY,         /**< X influenced by Y */
    ART_REL_PART_OF_MOVEMENT,      /**< Part of movement */
    ART_REL_USES_TECHNIQUE,        /**< Uses technique */
    ART_REL_CONTEMPORARY_OF,       /**< Contemporary of */
    ART_REL_STUDENT_OF,            /**< Student of */
    ART_REL_TEACHER_OF,            /**< Teacher of */
    ART_REL_INSPIRED,              /**< X inspired Y */
    ART_REL_SIMILAR_TO,            /**< Similar to */
    ART_REL_CONTRASTS_WITH,        /**< Contrasts with */
    ART_REL_PRECURSOR_TO,          /**< Precursor to */
    ART_REL_EVOLVED_FROM,          /**< Evolved from */
    ART_REL_HOUSED_AT,             /**< Housed at venue */
    ART_REL_COUNT
} art_relation_type_t;

//=============================================================================
// Knowledge Query Types
//=============================================================================

/**
 * @brief Artist information
 */
typedef struct {
    uint64_t entity_id;            /**< KG entity ID */
    char name[128];                /**< Artist name */
    char birth_year[16];           /**< Birth year */
    char death_year[16];           /**< Death year (if applicable) */
    char nationality[64];          /**< Nationality */
    art_modality_t primary_modality; /**< Primary art form */
    int32_t primary_archetype;     /**< Primary style archetype */
    char biography[1024];          /**< Short biography */
    char* movements;               /**< Associated movements (comma-sep) */
    char* techniques;              /**< Used techniques (comma-sep) */
    float influence_score;         /**< [0-1] Historical influence */
} art_artist_info_t;

/**
 * @brief Artwork information
 */
typedef struct {
    uint64_t entity_id;            /**< KG entity ID */
    char title[256];               /**< Work title */
    char creator[128];             /**< Creator name */
    char year[16];                 /**< Creation year */
    art_modality_t modality;       /**< Art modality */
    char medium[128];              /**< Medium/materials */
    char description[1024];        /**< Description */
    char movement[64];             /**< Associated movement */
    char location[128];            /**< Current location */
    float cultural_significance;   /**< [0-1] Cultural importance */
} art_work_info_t;

/**
 * @brief Art movement information
 */
typedef struct {
    uint64_t entity_id;            /**< KG entity ID */
    char name[128];                /**< Movement name */
    char period[64];               /**< Time period */
    char description[1024];        /**< Description */
    char* key_artists;             /**< Key artists (comma-sep) */
    char* key_works;               /**< Key works (comma-sep) */
    char* characteristics;         /**< Characteristics (comma-sep) */
    char precursor[128];           /**< Precursor movement */
    char successor[128];           /**< Successor movement */
} art_movement_info_t;

/**
 * @brief Knowledge query result
 */
typedef struct {
    void* entities;                /**< Array of entities */
    uint32_t count;                /**< Number of entities */
    art_entity_type_t entity_type; /**< Type of entities returned */
    float avg_relevance;           /**< Average relevance score */
} art_knowledge_result_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Knowledge bridge configuration
 */
typedef struct {
    /* Query settings */
    uint32_t max_query_results;    /**< Maximum results per query */
    float min_relevance;           /**< Minimum relevance threshold */
    bool cache_queries;            /**< Cache query results */
    uint32_t cache_size;           /**< Cache size (entries) */

    /* Knowledge sources */
    char kg_file_path[512];        /**< Path to knowledge graph file */
    bool use_external_kg;          /**< Use external KG (brain's KG) */
    bool fetch_from_web;           /**< Fetch missing data from web */

    /* Update settings */
    bool auto_learn;               /**< Auto-add new knowledge */
    float learn_confidence_threshold; /**< Min confidence to add */
} creative_knowledge_bridge_config_t;

/**
 * @brief Initialize config with defaults
 */
void creative_knowledge_bridge_config_defaults(
    creative_knowledge_bridge_config_t* config);

//=============================================================================
// Bridge Structure
//=============================================================================

/**
 * @brief Creative knowledge bridge
 */
struct creative_knowledge_bridge {
    creative_knowledge_bridge_config_t config;

    /* Internal knowledge store */
    void* knowledge_graph;         /**< Internal KG (if not using external) */

    /* External integration */
    void* brain_kg;                /**< External brain KG pointer */
    void* kg_reader;               /**< KG reader pointer */

    /* Query cache */
    void* query_cache;             /**< Query result cache */

    /* Statistics */
    uint64_t queries_performed;
    uint64_t cache_hits;
    float avg_query_time_ms;
};

/** @brief Typedef for creative_knowledge_bridge */
typedef struct creative_knowledge_bridge creative_knowledge_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create knowledge bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge or NULL on error
 */
creative_knowledge_bridge_t* creative_knowledge_bridge_create(
    const creative_knowledge_bridge_config_t* config);

/**
 * @brief Destroy knowledge bridge
 *
 * @param bridge Bridge to destroy
 */
void creative_knowledge_bridge_destroy(creative_knowledge_bridge_t* bridge);

/**
 * @brief Set external brain KG
 *
 * @param bridge Bridge
 * @param brain_kg Brain KG pointer
 */
void creative_knowledge_set_brain_kg(creative_knowledge_bridge_t* bridge,
                                      void* brain_kg);

//=============================================================================
// Artist Query API
//=============================================================================

/**
 * @brief Get artist by name
 *
 * @param bridge Bridge
 * @param name Artist name
 * @param out Output artist info
 * @return 0 on success, -1 if not found
 */
int creative_knowledge_get_artist(creative_knowledge_bridge_t* bridge,
                                   const char* name,
                                   art_artist_info_t* out);

/**
 * @brief Find artists by style
 *
 * @param bridge Bridge
 * @param modality Art modality
 * @param archetype_id Style archetype
 * @param max_results Maximum results
 * @param results Output array (caller allocated)
 * @return Number of results
 */
uint32_t creative_knowledge_artists_by_style(creative_knowledge_bridge_t* bridge,
                                              art_modality_t modality,
                                              int32_t archetype_id,
                                              uint32_t max_results,
                                              art_artist_info_t* results);

/**
 * @brief Get artists influenced by another
 *
 * @param bridge Bridge
 * @param artist_name Influencing artist name
 * @param max_results Maximum results
 * @param results Output array (caller allocated)
 * @return Number of results
 */
uint32_t creative_knowledge_influenced_by(creative_knowledge_bridge_t* bridge,
                                           const char* artist_name,
                                           uint32_t max_results,
                                           art_artist_info_t* results);

//=============================================================================
// Work Query API
//=============================================================================

/**
 * @brief Get work by title
 *
 * @param bridge Bridge
 * @param title Work title
 * @param out Output work info
 * @return 0 on success, -1 if not found
 */
int creative_knowledge_get_work(creative_knowledge_bridge_t* bridge,
                                 const char* title,
                                 art_work_info_t* out);

/**
 * @brief Find works by artist
 *
 * @param bridge Bridge
 * @param artist_name Artist name
 * @param max_results Maximum results
 * @param results Output array (caller allocated)
 * @return Number of results
 */
uint32_t creative_knowledge_works_by_artist(creative_knowledge_bridge_t* bridge,
                                             const char* artist_name,
                                             uint32_t max_results,
                                             art_work_info_t* results);

/**
 * @brief Find influential works in a movement
 *
 * @param bridge Bridge
 * @param movement_name Movement name
 * @param max_results Maximum results
 * @param results Output array (caller allocated)
 * @return Number of results
 */
uint32_t creative_knowledge_key_works(creative_knowledge_bridge_t* bridge,
                                       const char* movement_name,
                                       uint32_t max_results,
                                       art_work_info_t* results);

//=============================================================================
// Movement Query API
//=============================================================================

/**
 * @brief Get movement by name
 *
 * @param bridge Bridge
 * @param name Movement name
 * @param out Output movement info
 * @return 0 on success, -1 if not found
 */
int creative_knowledge_get_movement(creative_knowledge_bridge_t* bridge,
                                     const char* name,
                                     art_movement_info_t* out);

/**
 * @brief Find movements by period
 *
 * @param bridge Bridge
 * @param start_year Start year
 * @param end_year End year
 * @param modality Art modality (or -1 for all)
 * @param max_results Maximum results
 * @param results Output array (caller allocated)
 * @return Number of results
 */
uint32_t creative_knowledge_movements_by_period(creative_knowledge_bridge_t* bridge,
                                                 int32_t start_year,
                                                 int32_t end_year,
                                                 int32_t modality,
                                                 uint32_t max_results,
                                                 art_movement_info_t* results);

/**
 * @brief Get movement lineage (precursors and successors)
 *
 * @param bridge Bridge
 * @param movement_name Movement name
 * @param precursors Output precursors (caller allocated)
 * @param max_precursors Max precursors
 * @param successors Output successors (caller allocated)
 * @param max_successors Max successors
 * @param num_precursors Output: actual precursors
 * @param num_successors Output: actual successors
 */
void creative_knowledge_movement_lineage(creative_knowledge_bridge_t* bridge,
                                          const char* movement_name,
                                          art_movement_info_t* precursors,
                                          uint32_t max_precursors,
                                          art_movement_info_t* successors,
                                          uint32_t max_successors,
                                          uint32_t* num_precursors,
                                          uint32_t* num_successors);

//=============================================================================
// Context Query API
//=============================================================================

/**
 * @brief Get historical context for a work
 *
 * @param bridge Bridge
 * @param work Work info
 * @param context Output context description (caller allocated, min 1024 bytes)
 * @return 0 on success, -1 on error
 */
int creative_knowledge_historical_context(creative_knowledge_bridge_t* bridge,
                                           const art_work_info_t* work,
                                           char* context);

/**
 * @brief Get cultural significance of a style
 *
 * @param bridge Bridge
 * @param modality Art modality
 * @param archetype_id Style archetype
 * @param significance Output description (caller allocated, min 1024 bytes)
 * @return 0 on success, -1 on error
 */
int creative_knowledge_style_significance(creative_knowledge_bridge_t* bridge,
                                           art_modality_t modality,
                                           int32_t archetype_id,
                                           char* significance);

//=============================================================================
// Learning API
//=============================================================================

/**
 * @brief Add artist to knowledge base
 *
 * @param bridge Bridge
 * @param info Artist info to add
 * @return Entity ID on success, 0 on error
 */
uint64_t creative_knowledge_add_artist(creative_knowledge_bridge_t* bridge,
                                        const art_artist_info_t* info);

/**
 * @brief Add work to knowledge base
 *
 * @param bridge Bridge
 * @param info Work info to add
 * @return Entity ID on success, 0 on error
 */
uint64_t creative_knowledge_add_work(creative_knowledge_bridge_t* bridge,
                                      const art_work_info_t* info);

/**
 * @brief Add relationship
 *
 * @param bridge Bridge
 * @param from_id Source entity ID
 * @param to_id Target entity ID
 * @param relation_type Relationship type
 * @return 0 on success, -1 on error
 */
int creative_knowledge_add_relation(creative_knowledge_bridge_t* bridge,
                                     uint64_t from_id,
                                     uint64_t to_id,
                                     art_relation_type_t relation_type);

//=============================================================================
// Cleanup
//=============================================================================

/**
 * @brief Free query result
 *
 * @param result Result to free
 */
void art_knowledge_result_free(art_knowledge_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CREATIVE_KNOWLEDGE_BRIDGE_H */
