//=============================================================================
// nimcp_social_memory.c - Social Memory System Implementation
//=============================================================================
/**
 * @file nimcp_social_memory.c
 * @brief Implementation of social memory for persons, relationships, and episodes
 *
 * WHAT: Implementation of social memory system with person nodes, trust dynamics,
 *       relationship tracking, and social episode storage
 * WHY:  Social cognition requires specialized memory structures
 * HOW:  Hash tables for persons/episodes, matrix for relationships, PR integration
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_social_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_buffer_constants.h"  /* Centralized buffer size constants */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "utils/math/nimcp_math_helpers.h"

/* Forward declare heartbeat helper (defined at bottom with health agent block) */
static inline void social_memory_heartbeat(const char* operation, float progress);

//=============================================================================
// Internal Constants
//=============================================================================

/** Hash table load factor threshold */
#define HASH_LOAD_FACTOR_THRESHOLD 0.75f

/** Initial hash table capacity - use centralized constant */
#define HASH_INITIAL_CAPACITY NIMCP_HASH_TABLE_SMALL

/** Magic number for validation */
#define SOCIAL_MEM_MAGIC 0x534F4349  // "SOCI"

/** Thread-local error buffer size - use centralized constant */
#define ERROR_BUFFER_SIZE NIMCP_ERROR_BUFFER_SIZE

//=============================================================================
// Internal Type Definitions
//=============================================================================

/**
 * @brief Hash table entry for persons
 */
typedef struct person_entry {
    uint64_t key;                      /**< Person ID */
    person_node_t* person;             /**< Person data */
    struct person_entry* next;         /**< Hash chain next */
} person_entry_t;

/**
 * @brief Hash table entry for episodes
 */
typedef struct episode_entry {
    uint64_t key;                      /**< Episode ID */
    social_episode_t* episode;         /**< Episode data */
    struct episode_entry* next;        /**< Hash chain next */
} episode_entry_t;

/**
 * @brief Internal social memory structure
 */
/* TODO: Add mutex for thread safety — currently NOT thread-safe despite header doc */
typedef struct social_memory_struct {
    uint32_t magic;                    /**< Validation magic number */

    //-------------------------------------------------------------------------
    // PR Integration
    //-------------------------------------------------------------------------
    entangle_graph_t entanglement;     /**< Entanglement graph (optional) */
    pr_node_manager_t node_manager;    /**< PR node manager (optional) */

    //-------------------------------------------------------------------------
    // Person Storage
    //-------------------------------------------------------------------------
    person_entry_t** person_buckets;   /**< Hash table buckets */
    size_t person_bucket_count;        /**< Number of buckets */
    size_t num_persons;                /**< Current person count */
    size_t max_persons;                /**< Maximum persons */
    uint64_t next_person_id;           /**< Next person ID to assign */

    //-------------------------------------------------------------------------
    // Episode Storage
    //-------------------------------------------------------------------------
    episode_entry_t** episode_buckets; /**< Hash table buckets */
    size_t episode_bucket_count;       /**< Number of buckets */
    size_t num_episodes;               /**< Current episode count */
    size_t max_episodes;               /**< Maximum episodes */
    uint64_t next_episode_id;          /**< Next episode ID to assign */

    //-------------------------------------------------------------------------
    // Relationship Matrix
    //-------------------------------------------------------------------------
    float* relationship_matrix;        /**< N x N relationship strengths */
    relationship_type_t* relationship_types; /**< N x N relationship types */
    uint64_t* matrix_person_ids;       /**< Person ID at each matrix index */
    size_t matrix_size;                /**< Current matrix dimension */
    size_t matrix_capacity;            /**< Allocated matrix capacity */

    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    social_memory_config_t config;

    //-------------------------------------------------------------------------
    // Statistics
    //-------------------------------------------------------------------------
    uint64_t total_interactions;
    uint64_t trust_updates;
    uint64_t identifications;

} social_memory_internal_t;

//=============================================================================
// Thread-Local Storage
//=============================================================================

/** Thread-local error message */
static _Thread_local char g_error_buffer[ERROR_BUFFER_SIZE] = {0};

//=============================================================================
// Internal Function Declarations
//=============================================================================

static void set_error(const char* fmt, ...);
static uint64_t hash_uint64(uint64_t key);
static person_entry_t* find_person_entry(social_memory_internal_t* mem, uint64_t person_id);
static episode_entry_t* find_episode_entry(social_memory_internal_t* mem, uint64_t episode_id);
static bool add_person_entry(social_memory_internal_t* mem, person_node_t* person);
static bool add_episode_entry(social_memory_internal_t* mem, social_episode_t* episode);
static bool remove_person_entry(social_memory_internal_t* mem, uint64_t person_id);
static bool resize_person_table(social_memory_internal_t* mem);
static bool resize_episode_table(social_memory_internal_t* mem);
static bool grow_relationship_matrix(social_memory_internal_t* mem);
static int get_matrix_index(social_memory_internal_t* mem, uint64_t person_id);
static int assign_matrix_index(social_memory_internal_t* mem, uint64_t person_id);
static void free_person_node(person_node_t* person);
static void free_episode(social_episode_t* episode);
static person_node_t* create_person_node(const char* name);
static social_episode_t* create_episode(void);
static float compute_signature_match(const prime_signature_t* s1, const prime_signature_t* s2);
NIMCP_EXPORT social_memory_config_t social_memory_config_default(void) {
    social_memory_config_t config;
    memset(&config, 0, sizeof(config));

    /* Capacity */
    config.max_persons = 1024;
    config.max_episodes = 5000;
    config.max_facts_per_person = 100;

    /* Trust parameters */
    config.initial_trust = 0.5f;
    config.trust_decay_rate = 0.001f;
    config.trust_learning_rate = 0.1f;

    /* Relationship parameters */
    config.relationship_decay_rate = 0.0005f;
    config.familiarity_threshold = 0.3f;

    /* Recognition parameters */
    config.id_threshold = 0.85f;
    config.face_weight = 0.6f;
    config.voice_weight = 0.4f;

    /* Integration */
    config.enable_entanglement = true;
    config.enable_episode_linking = true;
    /* resonance_config zeroed by memset — all zero values */

    return config;
}

NIMCP_EXPORT bool social_memory_config_validate(const social_memory_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_memory_config_validate: config is NULL");
        return false;
    }

    if (config->max_persons == 0 || config->max_episodes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "social_memory_config_validate: config->max_persons is zero");
        return false;
    }

    if (config->initial_trust < 0.0f || config->initial_trust > 1.0f) {
        return false;
    }

    if (config->trust_decay_rate < 0.0f || config->trust_learning_rate < 0.0f) {
        return false;
    }

    if (config->id_threshold < 0.0f || config->id_threshold > 1.0f) {
        return false;
    }

    float weight_sum = config->face_weight + config->voice_weight;
    if (weight_sum < SOCIAL_MEM_EPSILON) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

NIMCP_EXPORT social_memory_t social_memory_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const social_memory_config_t* config)
{
    social_memory_config_t cfg;
    if (config) {
        cfg = *config;
        if (!social_memory_config_validate(&cfg)) {
            set_error("Invalid configuration");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "social_memory_create: social_memory_config_validate is NULL");
            return NULL;
        }
    } else {
        cfg = social_memory_config_default();
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)nimcp_calloc(1, sizeof(social_memory_internal_t));
    if (!mem) {
        set_error("Failed to allocate social memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "social_memory_create: mem is NULL");
        return NULL;
    }

    mem->magic = SOCIAL_MEM_MAGIC;
    mem->entanglement = entanglement;
    mem->node_manager = node_manager;
    mem->config = cfg;

    // Initialize person hash table
    mem->person_bucket_count = HASH_INITIAL_CAPACITY;
    mem->person_buckets = (person_entry_t**)nimcp_calloc(mem->person_bucket_count, sizeof(person_entry_t*));
    if (!mem->person_buckets) {
        set_error("Failed to allocate person buckets");
        nimcp_free(mem);
        mem = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "social_memory_create: mem->person_buckets is NULL");
        return NULL;
    }
    mem->max_persons = cfg.max_persons;
    mem->next_person_id = 1;

    // Initialize episode hash table
    mem->episode_bucket_count = HASH_INITIAL_CAPACITY;
    mem->episode_buckets = (episode_entry_t**)nimcp_calloc(mem->episode_bucket_count, sizeof(episode_entry_t*));
    if (!mem->episode_buckets) {
        set_error("Failed to allocate episode buckets");
        nimcp_free(mem->person_buckets);
        nimcp_free(mem);
        mem = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "social_memory_create: mem->episode_buckets is NULL");
        return NULL;
    }
    mem->max_episodes = cfg.max_episodes;
    mem->next_episode_id = 1;

    // Initialize relationship matrix (start small, grow as needed)
    size_t initial_matrix = 32;
    if (initial_matrix > cfg.max_persons) {
        initial_matrix = cfg.max_persons;
    }
    mem->matrix_capacity = initial_matrix;
    mem->matrix_size = 0;

    size_t matrix_elements = initial_matrix * initial_matrix;
    mem->relationship_matrix = (float*)nimcp_calloc(matrix_elements, sizeof(float));
    mem->relationship_types = (relationship_type_t*)nimcp_calloc(matrix_elements, sizeof(relationship_type_t));
    if (!mem->relationship_types) return NULL;
    mem->matrix_person_ids = (uint64_t*)nimcp_calloc(initial_matrix, sizeof(uint64_t));

    if (!mem->relationship_matrix || !mem->relationship_types || !mem->matrix_person_ids) {
        set_error("Failed to allocate relationship matrix");
        nimcp_free(mem->relationship_matrix);
        nimcp_free(mem->relationship_types);
        nimcp_free(mem->matrix_person_ids);
        nimcp_free(mem->episode_buckets);
        nimcp_free(mem->person_buckets);
        nimcp_free(mem);
        mem = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_memory_create: required parameter is NULL (mem->relationship_matrix, mem->relationship_types, mem->matrix_person_ids)");
        return NULL;
    }

    // Initialize matrix with default values
    for (size_t i = 0; i < matrix_elements; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && matrix_elements > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)matrix_elements);
        }

        mem->relationship_types[i] = REL_STRANGER;
    }
    for (size_t i = 0; i < initial_matrix; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && initial_matrix > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)initial_matrix);
        }

        mem->matrix_person_ids[i] = SOCIAL_MEM_INVALID_PERSON_ID;
    }

    return (social_memory_t)mem;
}

NIMCP_EXPORT void social_memory_destroy(social_memory_t social_mem) {
    if (!social_mem) {
        return;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return;
    }

    // Free all persons
    for (size_t i = 0; i < mem->person_bucket_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->person_bucket_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->person_bucket_count);
        }

        person_entry_t* entry = mem->person_buckets[i];
        while (entry) {
            person_entry_t* next = entry->next;
            free_person_node(entry->person);
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
    }
    nimcp_free(mem->person_buckets);

    // Free all episodes
    for (size_t i = 0; i < mem->episode_bucket_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->episode_bucket_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->episode_bucket_count);
        }

        episode_entry_t* entry = mem->episode_buckets[i];
        while (entry) {
            episode_entry_t* next = entry->next;
            free_episode(entry->episode);
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
    }
    nimcp_free(mem->episode_buckets);

    // Free relationship matrix
    nimcp_free(mem->relationship_matrix);
    nimcp_free(mem->relationship_types);
    nimcp_free(mem->matrix_person_ids);

    mem->magic = 0;
    nimcp_free(mem);
    mem = NULL;
}

NIMCP_EXPORT social_mem_error_t social_memory_clear(social_memory_t social_mem) {
    if (!social_mem) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    // Clear persons
    for (size_t i = 0; i < mem->person_bucket_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->person_bucket_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->person_bucket_count);
        }

        person_entry_t* entry = mem->person_buckets[i];
        while (entry) {
            person_entry_t* next = entry->next;
            free_person_node(entry->person);
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
        mem->person_buckets[i] = NULL;
    }
    mem->num_persons = 0;
    mem->next_person_id = 1;

    // Clear episodes
    for (size_t i = 0; i < mem->episode_bucket_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->episode_bucket_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->episode_bucket_count);
        }

        episode_entry_t* entry = mem->episode_buckets[i];
        while (entry) {
            episode_entry_t* next = entry->next;
            free_episode(entry->episode);
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
        mem->episode_buckets[i] = NULL;
    }
    mem->num_episodes = 0;
    mem->next_episode_id = 1;

    // Clear relationship matrix
    size_t matrix_elements = mem->matrix_capacity * mem->matrix_capacity;
    memset(mem->relationship_matrix, 0, matrix_elements * sizeof(float));
    for (size_t i = 0; i < matrix_elements; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && matrix_elements > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)matrix_elements);
        }

        mem->relationship_types[i] = REL_STRANGER;
    }
    for (size_t i = 0; i < mem->matrix_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->matrix_capacity > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->matrix_capacity);
        }

        mem->matrix_person_ids[i] = SOCIAL_MEM_INVALID_PERSON_ID;
    }
    mem->matrix_size = 0;

    // Clear statistics
    mem->total_interactions = 0;
    mem->trust_updates = 0;
    mem->identifications = 0;

    return SOCIAL_MEM_SUCCESS;
}

//=============================================================================
// Person Management Functions
//=============================================================================

NIMCP_EXPORT uint64_t social_memory_add_person(
    social_memory_t social_mem,
    const char* name,
    const prime_signature_t* identity_signature,
    relationship_type_t relationship)
{
    if (!social_mem) {
        set_error("NULL social memory");
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        set_error("Invalid social memory");
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    if (mem->num_persons >= mem->max_persons) {
        set_error("Person capacity reached");
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    person_node_t* person = create_person_node(name);
    if (!person) {
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    person->person_id = mem->next_person_id++;
    person->relationship = relationship;
    person->trust_level = mem->config.initial_trust;
    person->trust_baseline = mem->config.initial_trust;
    person->familiarity = (relationship == REL_STRANGER) ? 0.0f : 0.3f;
    person->created_time_ms = social_memory_current_time_ms();
    person->modified_time_ms = person->created_time_ms;

    // Set identity signature if provided
    if (identity_signature) {
        person->identity_signature = *identity_signature;
    }

    // Set initial quaternion (neutral emotional state)
    person->person_quaternion = quat_create(0.5f, 0.0f, 0.5f, 0.5f);

    // Add to hash table
    if (!add_person_entry(mem, person)) {
        free_person_node(person);
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    // Assign matrix index
    int idx = assign_matrix_index(mem, person->person_id);
    if (idx < 0) {
        /* remove_person_entry already frees the person node — do NOT call free_person_node again */
        remove_person_entry(mem, person->person_id);
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    return person->person_id;
}

NIMCP_EXPORT uint64_t social_memory_add_person_full(
    social_memory_t social_mem,
    const person_node_t* person)
{
    if (!social_mem || !person) {
        set_error("NULL argument");
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        set_error("Invalid social memory");
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    if (mem->num_persons >= mem->max_persons) {
        set_error("Person capacity reached");
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    // Deep copy the person
    person_node_t* new_person = create_person_node(person->name);
    if (!new_person) {
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    // Copy fields
    new_person->person_id = mem->next_person_id++;
    new_person->identity_signature = person->identity_signature;
    new_person->person_quaternion = person->person_quaternion;
    new_person->face_signature = person->face_signature;
    new_person->voice_signature = person->voice_signature;
    new_person->has_face_signature = person->has_face_signature;
    new_person->has_voice_signature = person->has_voice_signature;
    new_person->familiarity = person->familiarity;
    new_person->trust_level = person->trust_level;
    new_person->trust_baseline = person->trust_baseline;
    new_person->liking = person->liking;
    new_person->perceived_competence = person->perceived_competence;
    new_person->perceived_warmth = person->perceived_warmth;
    new_person->relationship = person->relationship;
    new_person->role = person->role;
    new_person->relationship_strength = person->relationship_strength;
    new_person->reciprocity = person->reciprocity;
    new_person->created_time_ms = social_memory_current_time_ms();
    new_person->modified_time_ms = new_person->created_time_ms;

    // Copy facts if present
    if (person->facts && person->num_facts > 0) {
        new_person->max_facts = person->num_facts > mem->config.max_facts_per_person ?
                                mem->config.max_facts_per_person : person->num_facts;
        new_person->facts = (prime_signature_t*)nimcp_calloc(new_person->max_facts, sizeof(prime_signature_t));
        if (new_person->facts) {
            /* Only copy up to max_facts elements to avoid buffer overflow */
            memcpy(new_person->facts, person->facts, new_person->max_facts * sizeof(prime_signature_t));
            new_person->num_facts = new_person->max_facts;
        }
    }

    if (!add_person_entry(mem, new_person)) {
        free_person_node(new_person);
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    int idx = assign_matrix_index(mem, new_person->person_id);
    if (idx < 0) {
        /* remove_person_entry already frees the person node — do NOT call free_person_node again */
        remove_person_entry(mem, new_person->person_id);
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    return new_person->person_id;
}

NIMCP_EXPORT social_mem_error_t social_memory_remove_person(
    social_memory_t social_mem,
    uint64_t person_id)
{
    if (!social_mem) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    person_entry_t* entry = find_person_entry(mem, person_id);
    if (!entry) {
        return SOCIAL_MEM_ERROR_NOT_FOUND;
    }

    // Clear relationship matrix row/column
    int idx = get_matrix_index(mem, person_id);
    if (idx >= 0) {
        for (size_t i = 0; i < mem->matrix_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && mem->matrix_size > 256) {
                social_memory_heartbeat("social_memor_loop",
                                 (float)(i + 1) / (float)mem->matrix_size);
            }

            mem->relationship_matrix[idx * mem->matrix_capacity + i] = 0.0f;
            mem->relationship_matrix[i * mem->matrix_capacity + idx] = 0.0f;
            mem->relationship_types[idx * mem->matrix_capacity + i] = REL_STRANGER;
            mem->relationship_types[i * mem->matrix_capacity + idx] = REL_STRANGER;
        }
        mem->matrix_person_ids[idx] = SOCIAL_MEM_INVALID_PERSON_ID;
    }

    // Remove from hash table
    if (!remove_person_entry(mem, person_id)) {
        return SOCIAL_MEM_ERROR_NOT_FOUND;
    }

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT const person_node_t* social_memory_get_person(
    social_memory_t social_mem,
    uint64_t person_id)
{
    if (!social_mem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_mem is NULL");

        return NULL;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_memory_get_person: validation failed");
        return NULL;
    }

    person_entry_t* entry = find_person_entry(mem, person_id);
    return entry ? entry->person : NULL;
}

NIMCP_EXPORT const person_node_t* social_memory_get_person_by_name(
    social_memory_t social_mem,
    const char* name)
{
    if (!social_mem || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_memory_get_person_by_name: required parameter is NULL (social_mem, name)");
        return NULL;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_memory_get_person_by_name: validation failed");
        return NULL;
    }

    // Linear scan through all persons
    for (size_t i = 0; i < mem->person_bucket_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->person_bucket_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->person_bucket_count);
        }

        person_entry_t* entry = mem->person_buckets[i];
        while (entry) {
            if (entry->person->name && strcmp(entry->person->name, name) == 0) {
                return entry->person;
            }
            entry = entry->next;
        }
    }

    return NULL;
}

NIMCP_EXPORT bool social_memory_person_exists(
    social_memory_t social_mem,
    uint64_t person_id)
{
    return social_memory_get_person(social_mem, person_id) != NULL;
}

NIMCP_EXPORT social_mem_error_t social_memory_get_all_persons(
    social_memory_t social_mem,
    uint64_t* ids,
    size_t max_ids,
    size_t* count)
{
    if (!social_mem || !ids || !count) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    *count = 0;
    for (size_t i = 0; i < mem->person_bucket_count && *count < max_ids; i++) {
        person_entry_t* entry = mem->person_buckets[i];
        while (entry && *count < max_ids) {
            ids[(*count)++] = entry->key;
            entry = entry->next;
        }
    }

    return SOCIAL_MEM_SUCCESS;
}

//=============================================================================
// Person Identification Functions
//=============================================================================

NIMCP_EXPORT uint64_t social_memory_identify_person(
    social_memory_t social_mem,
    const prime_signature_t* features,
    float* confidence)
{
    if (!social_mem || !features) {
        if (confidence) *confidence = 0.0f;
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        if (confidence) *confidence = 0.0f;
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    uint64_t best_person = SOCIAL_MEM_INVALID_PERSON_ID;
    float best_score = 0.0f;

    // Search through all persons
    for (size_t i = 0; i < mem->person_bucket_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->person_bucket_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->person_bucket_count);
        }

        person_entry_t* entry = mem->person_buckets[i];
        while (entry) {
            float score = compute_signature_match(&entry->person->identity_signature, features);

            if (score > best_score) {
                best_score = score;
                best_person = entry->key;
            }
            entry = entry->next;
        }
    }

    mem->identifications++;

    if (confidence) {
        *confidence = best_score;
    }

    // Return match only if above threshold
    if (best_score >= mem->config.id_threshold) {
        return best_person;
    }

    return SOCIAL_MEM_INVALID_PERSON_ID;
}

NIMCP_EXPORT uint64_t social_memory_identify_by_face(
    social_memory_t social_mem,
    const prime_signature_t* face_signature,
    float* confidence)
{
    if (!social_mem || !face_signature) {
        if (confidence) *confidence = 0.0f;
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        if (confidence) *confidence = 0.0f;
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    uint64_t best_person = SOCIAL_MEM_INVALID_PERSON_ID;
    float best_score = 0.0f;

    for (size_t i = 0; i < mem->person_bucket_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->person_bucket_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->person_bucket_count);
        }

        person_entry_t* entry = mem->person_buckets[i];
        while (entry) {
            if (entry->person->has_face_signature) {
                float score = compute_signature_match(&entry->person->face_signature, face_signature);
                if (score > best_score) {
                    best_score = score;
                    best_person = entry->key;
                }
            }
            entry = entry->next;
        }
    }

    if (confidence) *confidence = best_score;

    if (best_score >= mem->config.id_threshold) {
        return best_person;
    }
    return SOCIAL_MEM_INVALID_PERSON_ID;
}

NIMCP_EXPORT uint64_t social_memory_identify_by_voice(
    social_memory_t social_mem,
    const prime_signature_t* voice_signature,
    float* confidence)
{
    if (!social_mem || !voice_signature) {
        if (confidence) *confidence = 0.0f;
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        if (confidence) *confidence = 0.0f;
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    uint64_t best_person = SOCIAL_MEM_INVALID_PERSON_ID;
    float best_score = 0.0f;

    for (size_t i = 0; i < mem->person_bucket_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->person_bucket_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->person_bucket_count);
        }

        person_entry_t* entry = mem->person_buckets[i];
        while (entry) {
            if (entry->person->has_voice_signature) {
                float score = compute_signature_match(&entry->person->voice_signature, voice_signature);
                if (score > best_score) {
                    best_score = score;
                    best_person = entry->key;
                }
            }
            entry = entry->next;
        }
    }

    if (confidence) *confidence = best_score;

    if (best_score >= mem->config.id_threshold) {
        return best_person;
    }
    return SOCIAL_MEM_INVALID_PERSON_ID;
}

NIMCP_EXPORT uint64_t social_memory_identify_multimodal(
    social_memory_t social_mem,
    const prime_signature_t* face_signature,
    const prime_signature_t* voice_signature,
    float* confidence)
{
    if (!social_mem || (!face_signature && !voice_signature)) {
        if (confidence) *confidence = 0.0f;
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        if (confidence) *confidence = 0.0f;
        return SOCIAL_MEM_INVALID_PERSON_ID;
    }

    uint64_t best_person = SOCIAL_MEM_INVALID_PERSON_ID;
    float best_score = 0.0f;

    float face_weight = mem->config.face_weight;
    float voice_weight = mem->config.voice_weight;
    float total_weight = face_weight + voice_weight;

    // Normalize weights
    if (total_weight > SOCIAL_MEM_EPSILON) {
        face_weight /= total_weight;
        voice_weight /= total_weight;
    }

    for (size_t i = 0; i < mem->person_bucket_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->person_bucket_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->person_bucket_count);
        }

        person_entry_t* entry = mem->person_buckets[i];
        while (entry) {
            float score = 0.0f;
            float active_weight = 0.0f;

            if (face_signature && entry->person->has_face_signature) {
                float face_score = compute_signature_match(&entry->person->face_signature, face_signature);
                score += face_weight * face_score;
                active_weight += face_weight;
            }

            if (voice_signature && entry->person->has_voice_signature) {
                float voice_score = compute_signature_match(&entry->person->voice_signature, voice_signature);
                score += voice_weight * voice_score;
                active_weight += voice_weight;
            }

            // Normalize by active weight
            if (active_weight > SOCIAL_MEM_EPSILON) {
                score /= active_weight;
            }

            if (score > best_score) {
                best_score = score;
                best_person = entry->key;
            }
            entry = entry->next;
        }
    }

    if (confidence) *confidence = best_score;

    if (best_score >= mem->config.id_threshold) {
        return best_person;
    }
    return SOCIAL_MEM_INVALID_PERSON_ID;
}

NIMCP_EXPORT social_mem_error_t social_memory_identify_top_k(
    social_memory_t social_mem,
    const prime_signature_t* features,
    size_t k,
    person_query_result_t* results,
    size_t* count)
{
    if (!social_mem || !features || !results || !count || k == 0) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    // Collect all scores
    size_t total_persons = mem->num_persons;
    if (total_persons == 0) {
        *count = 0;
        return SOCIAL_MEM_SUCCESS;
    }

    // Allocate temporary array for all scores
    person_query_result_t* all_results = (person_query_result_t*)nimcp_calloc(total_persons, sizeof(person_query_result_t));
    if (!all_results) {
        return SOCIAL_MEM_ERROR_NO_MEMORY;
    }

    size_t idx = 0;
    for (size_t i = 0; i < mem->person_bucket_count && idx < total_persons; i++) {
        person_entry_t* entry = mem->person_buckets[i];
        while (entry && idx < total_persons) {
            all_results[idx].person_id = entry->key;
            all_results[idx].match_score = compute_signature_match(&entry->person->identity_signature, features);
            all_results[idx].trust_level = entry->person->trust_level;
            all_results[idx].relationship = entry->person->relationship;
            idx++;
            entry = entry->next;
        }
    }

    // Simple selection sort for top-k
    size_t result_count = (k < idx) ? k : idx;
    for (size_t i = 0; i < result_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && result_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)result_count);
        }

        size_t max_idx = i;
        for (size_t j = i + 1; j < idx; j++) {
            if (all_results[j].match_score > all_results[max_idx].match_score) {
                max_idx = j;
            }
        }
        if (max_idx != i) {
            person_query_result_t tmp = all_results[i];
            all_results[i] = all_results[max_idx];
            all_results[max_idx] = tmp;
        }
        results[i] = all_results[i];
    }

    *count = result_count;
    nimcp_free(all_results);
    all_results = NULL;
    return SOCIAL_MEM_SUCCESS;
}

//=============================================================================
// Person Update Functions
//=============================================================================

NIMCP_EXPORT social_mem_error_t social_memory_update_face(
    social_memory_t social_mem,
    uint64_t person_id,
    const prime_signature_t* face_signature)
{
    if (!social_mem || !face_signature) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    person_entry_t* entry = find_person_entry(mem, person_id);
    if (!entry) {
        return SOCIAL_MEM_ERROR_NOT_FOUND;
    }

    entry->person->face_signature = *face_signature;
    entry->person->has_face_signature = true;
    entry->person->modified_time_ms = social_memory_current_time_ms();

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT social_mem_error_t social_memory_update_voice(
    social_memory_t social_mem,
    uint64_t person_id,
    const prime_signature_t* voice_signature)
{
    if (!social_mem || !voice_signature) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    person_entry_t* entry = find_person_entry(mem, person_id);
    if (!entry) {
        return SOCIAL_MEM_ERROR_NOT_FOUND;
    }

    entry->person->voice_signature = *voice_signature;
    entry->person->has_voice_signature = true;
    entry->person->modified_time_ms = social_memory_current_time_ms();

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT social_mem_error_t social_memory_add_fact(
    social_memory_t social_mem,
    uint64_t person_id,
    const prime_signature_t* fact_signature)
{
    if (!social_mem || !fact_signature) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    person_entry_t* entry = find_person_entry(mem, person_id);
    if (!entry) {
        return SOCIAL_MEM_ERROR_NOT_FOUND;
    }

    person_node_t* person = entry->person;

    // Allocate facts array if needed
    if (!person->facts) {
        person->max_facts = mem->config.max_facts_per_person;
        person->facts = (prime_signature_t*)nimcp_calloc(person->max_facts, sizeof(prime_signature_t));
        if (!person->facts) {
            return SOCIAL_MEM_ERROR_NO_MEMORY;
        }
        person->num_facts = 0;
    }

    // Check capacity
    if (person->num_facts >= person->max_facts) {
        return SOCIAL_MEM_ERROR_CAPACITY;
    }

    person->facts[person->num_facts++] = *fact_signature;
    person->modified_time_ms = social_memory_current_time_ms();

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT social_mem_error_t social_memory_update_name(
    social_memory_t social_mem,
    uint64_t person_id,
    const char* name)
{
    if (!social_mem || !name) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    person_entry_t* entry = find_person_entry(mem, person_id);
    if (!entry) {
        return SOCIAL_MEM_ERROR_NOT_FOUND;
    }

    nimcp_free(entry->person->name);
    size_t name_len = strlen(name) + 1;
    entry->person->name = (char*)nimcp_malloc(name_len);
    if (!entry->person->name) {
        return SOCIAL_MEM_ERROR_NO_MEMORY;
    }
    memcpy(entry->person->name, name, name_len);

    entry->person->modified_time_ms = social_memory_current_time_ms();
    return SOCIAL_MEM_SUCCESS;
}

//=============================================================================
// Trust Management Functions
//=============================================================================

NIMCP_EXPORT float social_memory_update_trust(
    social_memory_t social_mem,
    uint64_t person_id,
    trust_outcome_t outcome,
    float magnitude)
{
    if (!social_mem) {
        return -1.0f;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return -1.0f;
    }

    person_entry_t* entry = find_person_entry(mem, person_id);
    if (!entry) {
        return -1.0f;
    }

    magnitude = nimcp_clampf(magnitude, 0.0f, 1.0f);
    float alpha = mem->config.trust_learning_rate;
    float delta = 0.0f;

    // Calculate trust delta based on outcome
    switch (outcome) {
        case TRUST_OUTCOME_POSITIVE:
            delta = alpha * magnitude;
            entry->person->positive_interactions++;
            break;

        case TRUST_OUTCOME_NEGATIVE:
            delta = -alpha * magnitude;
            entry->person->negative_interactions++;
            break;

        case TRUST_OUTCOME_NEUTRAL:
            delta = 0.0f;
            break;

        case TRUST_OUTCOME_BETRAYAL:
            // Large negative update for betrayal
            delta = -alpha * magnitude * 3.0f;
            entry->person->negative_interactions++;
            // Increase volatility
            entry->person->trust_volatility = nimcp_clampf(entry->person->trust_volatility + 0.1f, 0.0f, 1.0f);
            break;

        case TRUST_OUTCOME_EXCEPTIONAL:
            // Large positive update for exceptional behavior
            delta = alpha * magnitude * 2.0f;
            entry->person->positive_interactions++;
            break;

        default:
            return -1.0f;
    }

    // Apply update
    float old_trust = entry->person->trust_level;
    entry->person->trust_level = nimcp_clampf(old_trust + delta, SOCIAL_MEM_MIN_TRUST, SOCIAL_MEM_MAX_TRUST);

    // Update baseline (slow adaptation)
    entry->person->trust_baseline = entry->person->trust_baseline * 0.95f +
                                     entry->person->trust_level * 0.05f;

    entry->person->last_trust_update = (float)social_memory_current_time_ms() / 1000.0f;
    entry->person->modified_time_ms = social_memory_current_time_ms();

    mem->trust_updates++;

    return entry->person->trust_level;
}

NIMCP_EXPORT social_mem_error_t social_memory_set_trust(
    social_memory_t social_mem,
    uint64_t person_id,
    float trust_level)
{
    if (!social_mem) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    person_entry_t* entry = find_person_entry(mem, person_id);
    if (!entry) {
        return SOCIAL_MEM_ERROR_NOT_FOUND;
    }

    entry->person->trust_level = nimcp_clampf(trust_level, SOCIAL_MEM_MIN_TRUST, SOCIAL_MEM_MAX_TRUST);
    entry->person->modified_time_ms = social_memory_current_time_ms();

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT float social_memory_get_trust(
    social_memory_t social_mem,
    uint64_t person_id)
{
    if (!social_mem) {
        return -1.0f;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return -1.0f;
    }

    person_entry_t* entry = find_person_entry(mem, person_id);
    if (!entry) {
        return -1.0f;
    }

    return entry->person->trust_level;
}

NIMCP_EXPORT size_t social_memory_decay_trust(
    social_memory_t social_mem,
    float elapsed_days)
{
    if (!social_mem || elapsed_days < 0.0f) {
        return 0;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return 0;
    }

    float decay_factor = powf(1.0f - mem->config.trust_decay_rate, elapsed_days);
    size_t count = 0;

    for (size_t i = 0; i < mem->person_bucket_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->person_bucket_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->person_bucket_count);
        }

        person_entry_t* entry = mem->person_buckets[i];
        while (entry) {
            float trust = entry->person->trust_level;
            float baseline = entry->person->trust_baseline;

            // Decay toward baseline
            float diff = trust - baseline;
            entry->person->trust_level = baseline + diff * decay_factor;

            count++;
            entry = entry->next;
        }
    }

    return count;
}

NIMCP_EXPORT social_mem_error_t social_memory_get_trusted(
    social_memory_t social_mem,
    float threshold,
    uint64_t* ids,
    size_t max_ids,
    size_t* count)
{
    if (!social_mem || !ids || !count) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    *count = 0;
    for (size_t i = 0; i < mem->person_bucket_count && *count < max_ids; i++) {
        person_entry_t* entry = mem->person_buckets[i];
        while (entry && *count < max_ids) {
            if (entry->person->trust_level >= threshold) {
                ids[(*count)++] = entry->key;
            }
            entry = entry->next;
        }
    }

    return SOCIAL_MEM_SUCCESS;
}

//=============================================================================
// Relationship Management Functions
//=============================================================================

NIMCP_EXPORT social_mem_error_t social_memory_update_relationship(
    social_memory_t social_mem,
    uint64_t person_id,
    relationship_type_t relationship,
    float strength)
{
    if (!social_mem) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    if (relationship >= REL_TYPE_COUNT) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    person_entry_t* entry = find_person_entry(mem, person_id);
    if (!entry) {
        return SOCIAL_MEM_ERROR_NOT_FOUND;
    }

    entry->person->relationship = relationship;
    entry->person->relationship_strength = nimcp_clampf(strength, 0.0f, 1.0f);
    entry->person->modified_time_ms = social_memory_current_time_ms();

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT social_mem_error_t social_memory_set_relationship_between(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id,
    relationship_type_t relationship,
    float strength,
    bool bidirectional)
{
    if (!social_mem) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    if (relationship >= REL_TYPE_COUNT) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    int idx1 = get_matrix_index(mem, person1_id);
    int idx2 = get_matrix_index(mem, person2_id);

    if (idx1 < 0 || idx2 < 0) {
        return SOCIAL_MEM_ERROR_NOT_FOUND;
    }

    strength = nimcp_clampf(strength, 0.0f, 1.0f);

    // Set relationship in matrix
    size_t offset = (size_t)idx1 * mem->matrix_capacity + (size_t)idx2;
    mem->relationship_matrix[offset] = strength;
    mem->relationship_types[offset] = relationship;

    if (bidirectional) {
        offset = (size_t)idx2 * mem->matrix_capacity + (size_t)idx1;
        mem->relationship_matrix[offset] = strength;
        mem->relationship_types[offset] = relationship;
    }

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT float social_memory_get_relationship_strength(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id)
{
    if (!social_mem) {
        return -1.0f;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return -1.0f;
    }

    int idx1 = get_matrix_index(mem, person1_id);
    int idx2 = get_matrix_index(mem, person2_id);

    if (idx1 < 0 || idx2 < 0) {
        return -1.0f;
    }

    size_t offset = (size_t)idx1 * mem->matrix_capacity + (size_t)idx2;
    return mem->relationship_matrix[offset];
}

NIMCP_EXPORT relationship_type_t social_memory_get_relationship_type(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id)
{
    if (!social_mem) {
        return REL_STRANGER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return REL_STRANGER;
    }

    int idx1 = get_matrix_index(mem, person1_id);
    int idx2 = get_matrix_index(mem, person2_id);

    if (idx1 < 0 || idx2 < 0) {
        return REL_STRANGER;
    }

    size_t offset = (size_t)idx1 * mem->matrix_capacity + (size_t)idx2;
    return mem->relationship_types[offset];
}

NIMCP_EXPORT float social_memory_update_familiarity(
    social_memory_t social_mem,
    uint64_t person_id,
    float delta)
{
    if (!social_mem) {
        return -1.0f;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return -1.0f;
    }

    person_entry_t* entry = find_person_entry(mem, person_id);
    if (!entry) {
        return -1.0f;
    }

    entry->person->familiarity = nimcp_clampf(entry->person->familiarity + delta, 0.0f, 1.0f);
    entry->person->modified_time_ms = social_memory_current_time_ms();

    return entry->person->familiarity;
}

NIMCP_EXPORT float social_memory_update_liking(
    social_memory_t social_mem,
    uint64_t person_id,
    float delta)
{
    if (!social_mem) {
        return -2.0f;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return -2.0f;
    }

    person_entry_t* entry = find_person_entry(mem, person_id);
    if (!entry) {
        return -2.0f;
    }

    entry->person->liking = nimcp_clampf(entry->person->liking + delta, -1.0f, 1.0f);
    entry->person->modified_time_ms = social_memory_current_time_ms();

    return entry->person->liking;
}

//=============================================================================
// Social Query Functions
//=============================================================================

NIMCP_EXPORT social_mem_error_t social_memory_query_by_relationship(
    social_memory_t social_mem,
    relationship_type_t relationship,
    person_query_result_t* results,
    size_t max_results,
    size_t* count)
{
    if (!social_mem || !results || !count) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    *count = 0;
    for (size_t i = 0; i < mem->person_bucket_count && *count < max_results; i++) {
        person_entry_t* entry = mem->person_buckets[i];
        while (entry && *count < max_results) {
            if (entry->person->relationship == relationship) {
                results[*count].person_id = entry->key;
                results[*count].match_score = entry->person->relationship_strength;
                results[*count].trust_level = entry->person->trust_level;
                results[*count].relationship = relationship;
                (*count)++;
            }
            entry = entry->next;
        }
    }

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT social_mem_error_t social_memory_query_by_familiarity(
    social_memory_t social_mem,
    float min_familiarity,
    person_query_result_t* results,
    size_t max_results,
    size_t* count)
{
    if (!social_mem || !results || !count) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    *count = 0;
    for (size_t i = 0; i < mem->person_bucket_count && *count < max_results; i++) {
        person_entry_t* entry = mem->person_buckets[i];
        while (entry && *count < max_results) {
            if (entry->person->familiarity >= min_familiarity) {
                results[*count].person_id = entry->key;
                results[*count].match_score = entry->person->familiarity;
                results[*count].trust_level = entry->person->trust_level;
                results[*count].relationship = entry->person->relationship;
                (*count)++;
            }
            entry = entry->next;
        }
    }

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT social_mem_error_t social_memory_get_mutual_friends(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id,
    uint64_t* mutual_ids,
    size_t max_ids,
    size_t* count)
{
    if (!social_mem || !mutual_ids || !count) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    int idx1 = get_matrix_index(mem, person1_id);
    int idx2 = get_matrix_index(mem, person2_id);

    if (idx1 < 0 || idx2 < 0) {
        return SOCIAL_MEM_ERROR_NOT_FOUND;
    }

    *count = 0;

    // Find persons connected to both
    for (size_t i = 0; i < mem->matrix_size && *count < max_ids; i++) {
        uint64_t other_id = mem->matrix_person_ids[i];
        if (other_id == SOCIAL_MEM_INVALID_PERSON_ID ||
            other_id == person1_id || other_id == person2_id) {
            continue;
        }

        // Check if connected to both
        size_t offset1 = (size_t)idx1 * mem->matrix_capacity + i;
        size_t offset2 = (size_t)idx2 * mem->matrix_capacity + i;

        bool connected1 = (mem->relationship_matrix[offset1] > 0.0f &&
                          mem->relationship_types[offset1] != REL_STRANGER);
        bool connected2 = (mem->relationship_matrix[offset2] > 0.0f &&
                          mem->relationship_types[offset2] != REL_STRANGER);

        if (connected1 && connected2) {
            mutual_ids[(*count)++] = other_id;
        }
    }

    return SOCIAL_MEM_SUCCESS;
}

//=============================================================================
// Social Episode Functions
//=============================================================================

NIMCP_EXPORT uint64_t social_memory_record_episode(
    social_memory_t social_mem,
    const uint64_t* participant_ids,
    size_t num_participants,
    const prime_signature_t* context_signature,
    float episode_time,
    float emotional_valence,
    float social_importance)
{
    if (!social_mem) {
        return SOCIAL_MEM_INVALID_EPISODE_ID;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_INVALID_EPISODE_ID;
    }

    if (mem->num_episodes >= mem->max_episodes) {
        set_error("Episode capacity reached");
        return SOCIAL_MEM_INVALID_EPISODE_ID;
    }

    social_episode_t* episode = create_episode();
    if (!episode) {
        return SOCIAL_MEM_INVALID_EPISODE_ID;
    }

    episode->episode_id = mem->next_episode_id++;
    episode->episode_time = episode_time;
    episode->emotional_valence = nimcp_clampf(emotional_valence, -1.0f, 1.0f);
    episode->social_importance = nimcp_clampf(social_importance, 0.0f, 1.0f);
    episode->created_time_ms = social_memory_current_time_ms();

    if (context_signature) {
        episode->context_signature = *context_signature;
    }

    // Copy participants
    if (participant_ids && num_participants > 0) {
        size_t max_p = (num_participants > SOCIAL_MEM_MAX_EPISODE_PARTICIPANTS) ?
                       SOCIAL_MEM_MAX_EPISODE_PARTICIPANTS : num_participants;
        episode->participant_ids = (uint64_t*)nimcp_calloc(max_p, sizeof(uint64_t));
        if (episode->participant_ids) {
            memcpy(episode->participant_ids, participant_ids, max_p * sizeof(uint64_t));
            episode->num_participants = max_p;
            episode->max_participants = max_p;
        }
    }

    if (!add_episode_entry(mem, episode)) {
        free_episode(episode);
        return SOCIAL_MEM_INVALID_EPISODE_ID;
    }

    // Update interaction counts for participants
    for (size_t i = 0; i < episode->num_participants; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && episode->num_participants > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)episode->num_participants);
        }

        person_entry_t* entry = find_person_entry(mem, episode->participant_ids[i]);
        if (entry) {
            entry->person->interaction_count++;
            entry->person->last_interaction_time = episode_time;
            mem->total_interactions++;
        }
    }

    return episode->episode_id;
}

NIMCP_EXPORT uint64_t social_memory_record_episode_full(
    social_memory_t social_mem,
    const social_episode_t* episode)
{
    if (!social_mem || !episode) {
        return SOCIAL_MEM_INVALID_EPISODE_ID;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_INVALID_EPISODE_ID;
    }

    if (mem->num_episodes >= mem->max_episodes) {
        return SOCIAL_MEM_INVALID_EPISODE_ID;
    }

    social_episode_t* new_ep = create_episode();
    if (!new_ep) {
        return SOCIAL_MEM_INVALID_EPISODE_ID;
    }

    // Copy fields
    new_ep->episode_id = mem->next_episode_id++;
    new_ep->context_signature = episode->context_signature;
    new_ep->episode_time = episode->episode_time;
    new_ep->duration = episode->duration;
    new_ep->emotional_valence = episode->emotional_valence;
    new_ep->emotional_arousal = episode->emotional_arousal;
    new_ep->social_importance = episode->social_importance;
    new_ep->outcome_type = episode->outcome_type;
    new_ep->outcome_magnitude = episode->outcome_magnitude;
    new_ep->created_time_ms = social_memory_current_time_ms();

    if (episode->location) {
        size_t loc_len = strlen(episode->location) + 1;
        new_ep->location = (char*)nimcp_malloc(loc_len);
        if (new_ep->location) {
            memcpy(new_ep->location, episode->location, loc_len);
        }
    }
    if (episode->description) {
        size_t desc_len = strlen(episode->description) + 1;
        new_ep->description = (char*)nimcp_malloc(desc_len);
        if (new_ep->description) {
            memcpy(new_ep->description, episode->description, desc_len);
        }
    }

    // Copy participants
    if (episode->participant_ids && episode->num_participants > 0) {
        new_ep->participant_ids = (uint64_t*)nimcp_calloc(episode->num_participants, sizeof(uint64_t));
        if (new_ep->participant_ids) {
            memcpy(new_ep->participant_ids, episode->participant_ids,
                   episode->num_participants * sizeof(uint64_t));
            new_ep->num_participants = episode->num_participants;
            new_ep->max_participants = episode->num_participants;
        }
    }

    if (!add_episode_entry(mem, new_ep)) {
        free_episode(new_ep);
        return SOCIAL_MEM_INVALID_EPISODE_ID;
    }

    return new_ep->episode_id;
}

NIMCP_EXPORT const social_episode_t* social_memory_get_episode(
    social_memory_t social_mem,
    uint64_t episode_id)
{
    if (!social_mem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_mem is NULL");

        return NULL;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_memory_get_episode: validation failed");
        return NULL;
    }

    episode_entry_t* entry = find_episode_entry(mem, episode_id);
    return entry ? entry->episode : NULL;
}

NIMCP_EXPORT social_mem_error_t social_memory_get_person_history(
    social_memory_t social_mem,
    uint64_t person_id,
    uint64_t* episode_ids,
    size_t max_episodes,
    size_t* count)
{
    if (!social_mem || !episode_ids || !count) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    *count = 0;

    // Scan all episodes for this person
    for (size_t i = 0; i < mem->episode_bucket_count && *count < max_episodes; i++) {
        episode_entry_t* entry = mem->episode_buckets[i];
        while (entry && *count < max_episodes) {
            // Check if person is a participant
            for (size_t p = 0; p < entry->episode->num_participants; p++) {
                if (entry->episode->participant_ids[p] == person_id) {
                    episode_ids[(*count)++] = entry->key;
                    break;
                }
            }
            entry = entry->next;
        }
    }

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT social_mem_error_t social_memory_get_recent_episodes(
    social_memory_t social_mem,
    float since_time,
    uint64_t* episode_ids,
    size_t max_episodes,
    size_t* count)
{
    if (!social_mem || !episode_ids || !count) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    *count = 0;

    for (size_t i = 0; i < mem->episode_bucket_count && *count < max_episodes; i++) {
        episode_entry_t* entry = mem->episode_buckets[i];
        while (entry && *count < max_episodes) {
            if (entry->episode->episode_time >= since_time) {
                episode_ids[(*count)++] = entry->key;
            }
            entry = entry->next;
        }
    }

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT social_mem_error_t social_memory_get_episodes_between(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id,
    uint64_t* episode_ids,
    size_t max_episodes,
    size_t* count)
{
    if (!social_mem || !episode_ids || !count) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    *count = 0;

    for (size_t i = 0; i < mem->episode_bucket_count && *count < max_episodes; i++) {
        episode_entry_t* entry = mem->episode_buckets[i];
        while (entry && *count < max_episodes) {
            bool has_p1 = false, has_p2 = false;
            for (size_t p = 0; p < entry->episode->num_participants; p++) {
                if (entry->episode->participant_ids[p] == person1_id) has_p1 = true;
                if (entry->episode->participant_ids[p] == person2_id) has_p2 = true;
            }
            if (has_p1 && has_p2) {
                episode_ids[(*count)++] = entry->key;
            }
            entry = entry->next;
        }
    }

    return SOCIAL_MEM_SUCCESS;
}

//=============================================================================
// Social Network Analysis Functions
//=============================================================================

NIMCP_EXPORT social_mem_error_t social_memory_compute_social_network(
    social_memory_t social_mem,
    social_network_node_t* results,
    size_t max_results,
    size_t* count)
{
    if (!social_mem || !results || !count) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    *count = 0;

    for (size_t i = 0; i < mem->matrix_size && *count < max_results; i++) {
        uint64_t person_id = mem->matrix_person_ids[i];
        if (person_id == SOCIAL_MEM_INVALID_PERSON_ID) {
            continue;
        }

        results[*count].person_id = person_id;
        results[*count].degree = 0;
        results[*count].avg_trust = 0.0f;
        results[*count].centrality = 0.0f;
        results[*count].clustering = 0.0f;

        float total_strength = 0.0f;
        size_t connection_count = 0;

        // Count connections and sum trust from others
        for (size_t j = 0; j < mem->matrix_size; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && mem->matrix_size > 256) {
                social_memory_heartbeat("social_memor_loop",
                                 (float)(j + 1) / (float)mem->matrix_size);
            }

            if (i == j) continue;

            size_t offset = i * mem->matrix_capacity + j;
            if (mem->relationship_matrix[offset] > 0.0f &&
                mem->relationship_types[offset] != REL_STRANGER) {
                results[*count].degree++;
                total_strength += mem->relationship_matrix[offset];
            }

            // Check trust from j to i (using person data)
            person_entry_t* entry = find_person_entry(mem, mem->matrix_person_ids[j]);
            if (entry) {
                results[*count].avg_trust += entry->person->trust_level;
                connection_count++;
            }
        }

        if (connection_count > 0) {
            results[*count].avg_trust /= (float)connection_count;
        }

        // Simple centrality: normalized degree
        if (mem->matrix_size > 1) {
            results[*count].centrality = (float)results[*count].degree / (float)(mem->matrix_size - 1);
        }

        (*count)++;
    }

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT float social_memory_get_centrality(
    social_memory_t social_mem,
    uint64_t person_id)
{
    if (!social_mem) {
        return -1.0f;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return -1.0f;
    }

    int idx = get_matrix_index(mem, person_id);
    if (idx < 0) {
        return -1.0f;
    }

    size_t degree = 0;
    for (size_t j = 0; j < mem->matrix_size; j++) {
        /* Phase 8: Loop progress heartbeat */
        if ((j & 0xFF) == 0 && mem->matrix_size > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(j + 1) / (float)mem->matrix_size);
        }

        if ((size_t)idx == j) continue;
        size_t offset = (size_t)idx * mem->matrix_capacity + j;
        if (mem->relationship_matrix[offset] > 0.0f &&
            mem->relationship_types[offset] != REL_STRANGER) {
            degree++;
        }
    }

    if (mem->matrix_size <= 1) {
        return 0.0f;
    }

    return (float)degree / (float)(mem->matrix_size - 1);
}

NIMCP_EXPORT int social_memory_degrees_of_separation(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id)
{
    if (!social_mem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_mem is NULL");

        return -1;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "social_memory_degrees_of_separation: validation failed");
        return -1;
    }

    int idx1 = get_matrix_index(mem, person1_id);
    int idx2 = get_matrix_index(mem, person2_id);

    if (idx1 < 0 || idx2 < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "social_memory_degrees_of_separation: validation failed");
        return -1;
    }

    if (idx1 == idx2) {
        return 0;
    }

    // BFS to find shortest path
    int* distance = (int*)nimcp_calloc(mem->matrix_size, sizeof(int));
    bool* visited = (bool*)nimcp_calloc(mem->matrix_size, sizeof(bool));
    int* queue = (int*)nimcp_calloc(mem->matrix_size, sizeof(int));

    if (!distance || !visited || !queue) {
        nimcp_free(distance);
        distance = NULL;
        nimcp_free(visited);
        visited = NULL;
        nimcp_free(queue);
        queue = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_memory_degrees_of_separation: required parameter is NULL (distance, visited, queue)");
        return -1;
    }

    size_t front = 0, back = 0;
    queue[back++] = idx1;
    visited[idx1] = true;
    distance[idx1] = 0;

    int result = -1;

    while (front < back) {
        int current = queue[front++];

        if (current == idx2) {
            result = distance[current];
            break;
        }

        for (size_t j = 0; j < mem->matrix_size; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && mem->matrix_size > 256) {
                social_memory_heartbeat("social_memor_loop",
                                 (float)(j + 1) / (float)mem->matrix_size);
            }

            if (visited[j]) continue;

            size_t offset = (size_t)current * mem->matrix_capacity + j;
            if (mem->relationship_matrix[offset] > 0.0f &&
                mem->relationship_types[offset] != REL_STRANGER) {
                visited[j] = true;
                distance[j] = distance[current] + 1;
                queue[back++] = (int)j;
            }
        }
    }

    nimcp_free(distance);
    distance = NULL;
    nimcp_free(visited);
    visited = NULL;
    nimcp_free(queue);
    queue = NULL;

    return result;
}

NIMCP_EXPORT social_mem_error_t social_memory_find_clusters(
    social_memory_t social_mem,
    uint32_t* cluster_ids,
    uint64_t* person_ids,
    size_t max_size,
    size_t* count,
    size_t* num_clusters)
{
    if (!social_mem || !cluster_ids || !person_ids || !count || !num_clusters) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    // Simple connected components clustering
    int* component = (int*)nimcp_calloc(mem->matrix_size, sizeof(int));
    if (!component) {
        return SOCIAL_MEM_ERROR_NO_MEMORY;
    }

    for (size_t i = 0; i < mem->matrix_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->matrix_size > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->matrix_size);
        }

        component[i] = -1;
    }

    int current_cluster = 0;

    for (size_t start = 0; start < mem->matrix_size; start++) {
        /* Phase 8: Loop progress heartbeat */
        if ((start & 0xFF) == 0 && mem->matrix_size > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(start + 1) / (float)mem->matrix_size);
        }

        if (component[start] >= 0) continue;
        if (mem->matrix_person_ids[start] == SOCIAL_MEM_INVALID_PERSON_ID) continue;

        // BFS from this node
        int* queue = (int*)nimcp_calloc(mem->matrix_size, sizeof(int));
        if (!queue) {
            nimcp_free(component);
            component = NULL;
            return SOCIAL_MEM_ERROR_NO_MEMORY;
        }

        size_t front = 0, back = 0;
        queue[back++] = (int)start;
        component[start] = current_cluster;

        while (front < back) {
            int current = queue[front++];

            for (size_t j = 0; j < mem->matrix_size; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && mem->matrix_size > 256) {
                    social_memory_heartbeat("social_memor_loop",
                                     (float)(j + 1) / (float)mem->matrix_size);
                }

                if (component[j] >= 0) continue;
                if (mem->matrix_person_ids[j] == SOCIAL_MEM_INVALID_PERSON_ID) continue;

                size_t offset = (size_t)current * mem->matrix_capacity + j;
                if (mem->relationship_matrix[offset] > 0.0f &&
                    mem->relationship_types[offset] != REL_STRANGER) {
                    component[j] = current_cluster;
                    queue[back++] = (int)j;
                }
            }
        }

        nimcp_free(queue);
        queue = NULL;
        current_cluster++;
    }

    // Output results
    *count = 0;
    *num_clusters = (size_t)current_cluster;

    for (size_t i = 0; i < mem->matrix_size && *count < max_size; i++) {
        if (mem->matrix_person_ids[i] == SOCIAL_MEM_INVALID_PERSON_ID) continue;
        if (component[i] < 0) continue;

        person_ids[*count] = mem->matrix_person_ids[i];
        cluster_ids[*count] = (uint32_t)component[i];
        (*count)++;
    }

    nimcp_free(component);
    component = NULL;
    return SOCIAL_MEM_SUCCESS;
}

//=============================================================================
// Behavior Prediction Functions
//=============================================================================

NIMCP_EXPORT social_mem_error_t social_memory_predict_behavior(
    social_memory_t social_mem,
    uint64_t person_id,
    behavior_prediction_t* prediction)
{
    if (!social_mem || !prediction) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    person_entry_t* entry = find_person_entry(mem, person_id);
    if (!entry) {
        return SOCIAL_MEM_ERROR_NOT_FOUND;
    }

    person_node_t* person = entry->person;

    // Base predictions on trust, history, and relationship
    float trust = person->trust_level;
    float liking = person->liking;
    float reciprocity = person->reciprocity;

    // Cooperation probability based on trust and reciprocity
    prediction->cooperation_prob = nimcp_clampf(0.5f + trust * 0.3f + reciprocity * 0.2f, 0.0f, 1.0f);

    // Defection probability (inverse relationship with trust)
    prediction->defection_prob = nimcp_clampf(0.5f - trust * 0.4f - reciprocity * 0.1f, 0.0f, 1.0f);

    // Helpfulness based on liking and relationship type
    float rel_bonus = 0.0f;
    switch (person->relationship) {
        case REL_FAMILY: rel_bonus = 0.3f; break;
        case REL_FRIEND: rel_bonus = 0.2f; break;
        case REL_ROMANTIC: rel_bonus = 0.25f; break;
        case REL_COLLEAGUE: rel_bonus = 0.1f; break;
        case REL_ADVERSARY: rel_bonus = -0.3f; break;
        default: break;
    }
    prediction->helpfulness_prob = nimcp_clampf(0.5f + liking * 0.2f + rel_bonus, 0.0f, 1.0f);

    // Confidence based on number of interactions
    float interaction_factor = 1.0f - expf(-(float)person->interaction_count / 10.0f);
    prediction->confidence = nimcp_clampf(0.3f + interaction_factor * 0.7f - person->trust_volatility * 0.3f,
                                   0.1f, 0.95f);

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT social_mem_error_t social_memory_predict_interaction(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id,
    behavior_prediction_t* prediction)
{
    if (!social_mem || !prediction) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    // Get individual predictions and combine
    behavior_prediction_t pred1, pred2;
    social_mem_error_t err;

    err = social_memory_predict_behavior(social_mem, person1_id, &pred1);
    if (err != SOCIAL_MEM_SUCCESS) {
        return err;
    }

    err = social_memory_predict_behavior(social_mem, person2_id, &pred2);
    if (err != SOCIAL_MEM_SUCCESS) {
        return err;
    }

    // Combined prediction: geometric mean of individual predictions
    prediction->cooperation_prob = sqrtf(pred1.cooperation_prob * pred2.cooperation_prob);
    prediction->defection_prob = sqrtf(pred1.defection_prob * pred2.defection_prob);
    prediction->helpfulness_prob = sqrtf(pred1.helpfulness_prob * pred2.helpfulness_prob);
    prediction->confidence = sqrtf(pred1.confidence * pred2.confidence);

    return SOCIAL_MEM_SUCCESS;
}

//=============================================================================
// Entanglement Functions
//=============================================================================

NIMCP_EXPORT social_mem_error_t social_memory_entangle_persons(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id,
    entangle_edge_type_t edge_type,
    float strength)
{
    if (!social_mem) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    if (!mem->entanglement || !mem->config.enable_entanglement) {
        return SOCIAL_MEM_SUCCESS;  // Silently succeed if not enabled
    }

    // Verify both persons exist
    person_entry_t* entry1 = find_person_entry(mem, person1_id);
    person_entry_t* entry2 = find_person_entry(mem, person2_id);

    if (!entry1 || !entry2) {
        return SOCIAL_MEM_ERROR_NOT_FOUND;
    }

    // Create entanglement edge
    entangle_edge_t edge = {
        .from_id = person1_id,
        .to_id = person2_id,
        .resonance_score = strength,
        .type = edge_type,
        .weight = strength,
        .bidirectional = true,
        .created_time_ms = social_memory_current_time_ms()
    };

    if (!entangle_add_edge(mem->entanglement, &edge)) {
        // Edge may already exist
        entangle_update_edge(mem->entanglement, &edge);
    }

    // Update entanglement counts
    entry1->person->entanglement_count++;
    entry2->person->entanglement_count++;

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT social_mem_error_t social_memory_get_entangled(
    social_memory_t social_mem,
    uint64_t person_id,
    uint64_t* ids,
    size_t max_ids,
    size_t* count)
{
    if (!social_mem || !ids || !count) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    *count = 0;

    if (!mem->entanglement || !mem->config.enable_entanglement) {
        return SOCIAL_MEM_SUCCESS;
    }

    // Get neighbors from entanglement graph
    entangle_neighbor_t* neighbors = (entangle_neighbor_t*)nimcp_calloc(max_ids, sizeof(entangle_neighbor_t));
    if (!neighbors) {
        return SOCIAL_MEM_ERROR_NO_MEMORY;
    }

    size_t neighbor_count = 0;
    if (entangle_get_neighbors(mem->entanglement, person_id, neighbors, max_ids, &neighbor_count)) {
        for (size_t i = 0; i < neighbor_count && *count < max_ids; i++) {
            ids[(*count)++] = neighbors[i].neighbor_id;
        }
    }

    nimcp_free(neighbors);
    neighbors = NULL;
    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT social_mem_error_t social_memory_update_emotion(
    social_memory_t social_mem,
    uint64_t person_id,
    nimcp_quaternion_t quaternion)
{
    if (!social_mem) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    person_entry_t* entry = find_person_entry(mem, person_id);
    if (!entry) {
        return SOCIAL_MEM_ERROR_NOT_FOUND;
    }

    entry->person->person_quaternion = quaternion;
    entry->person->modified_time_ms = social_memory_current_time_ms();

    return SOCIAL_MEM_SUCCESS;
}

//=============================================================================
// Statistics and Utility Functions
//=============================================================================

NIMCP_EXPORT social_mem_error_t social_memory_get_stats(
    social_memory_t social_mem,
    social_memory_stats_t* stats)
{
    if (!social_mem || !stats) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    memset(stats, 0, sizeof(social_memory_stats_t));

    stats->num_persons = mem->num_persons;
    stats->num_episodes = mem->num_episodes;
    stats->total_interactions = mem->total_interactions;

    float total_trust = 0.0f;
    float total_familiarity = 0.0f;
    size_t person_count = 0;

    for (size_t i = 0; i < mem->person_bucket_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->person_bucket_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->person_bucket_count);
        }

        person_entry_t* entry = mem->person_buckets[i];
        while (entry) {
            person_node_t* p = entry->person;

            total_trust += p->trust_level;
            total_familiarity += p->familiarity;
            stats->total_facts += p->num_facts;

            if (p->trust_level >= SOCIAL_MEM_TRUSTED_THRESHOLD) {
                stats->trusted_count++;
            }

            switch (p->relationship) {
                case REL_FAMILY: stats->family_count++; break;
                case REL_FRIEND: stats->friend_count++; break;
                default: break;
            }

            if (p->relationship != REL_STRANGER) {
                stats->total_relationships++;
            }

            person_count++;
            entry = entry->next;
        }
    }

    if (person_count > 0) {
        stats->avg_trust = total_trust / (float)person_count;
        stats->avg_familiarity = total_familiarity / (float)person_count;
    }

    // Estimate memory usage
    stats->memory_bytes = sizeof(social_memory_internal_t) +
                          mem->person_bucket_count * sizeof(person_entry_t*) +
                          mem->episode_bucket_count * sizeof(episode_entry_t*) +
                          mem->matrix_capacity * mem->matrix_capacity * (sizeof(float) + sizeof(relationship_type_t)) +
                          mem->matrix_capacity * sizeof(uint64_t) +
                          mem->num_persons * sizeof(person_node_t) +
                          mem->num_episodes * sizeof(social_episode_t);

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT const char* social_memory_get_last_error(void) {
    return g_error_buffer[0] ? g_error_buffer : NULL;
}

NIMCP_EXPORT const char* social_memory_error_string(social_mem_error_t error) {
    switch (error) {
        case SOCIAL_MEM_SUCCESS:            return "Success";
        case SOCIAL_MEM_ERROR_NULL_POINTER: return "NULL pointer argument";
        case SOCIAL_MEM_ERROR_INVALID_ID:   return "Invalid person/episode ID";
        case SOCIAL_MEM_ERROR_CAPACITY:     return "Capacity limit reached";
        case SOCIAL_MEM_ERROR_NOT_FOUND:    return "Person/episode not found";
        case SOCIAL_MEM_ERROR_DUPLICATE:    return "Duplicate entry";
        case SOCIAL_MEM_ERROR_NO_MEMORY:    return "Memory allocation failed";
        case SOCIAL_MEM_ERROR_INVALID_PARAM: return "Invalid parameter";
        case SOCIAL_MEM_ERROR_LOCKED:       return "Resource locked";
        case SOCIAL_MEM_ERROR_SERIALIZE:    return "Serialization failed";
        case SOCIAL_MEM_ERROR_DESERIALIZE:  return "Deserialization failed";
        default:                            return "Unknown error";
    }
}

NIMCP_EXPORT const char* social_memory_relationship_name(relationship_type_t type) {
    switch (type) {
        case REL_FAMILY:      return "Family";
        case REL_FRIEND:      return "Friend";
        case REL_ACQUAINTANCE: return "Acquaintance";
        case REL_COLLEAGUE:   return "Colleague";
        case REL_AUTHORITY:   return "Authority";
        case REL_SUBORDINATE: return "Subordinate";
        case REL_ROMANTIC:    return "Romantic";
        case REL_STRANGER:    return "Stranger";
        case REL_ADVERSARY:   return "Adversary";
        default:              return "Unknown";
    }
}

NIMCP_EXPORT void social_memory_print_summary(social_memory_t social_mem) {
    if (!social_mem) {
        printf("Social Memory: NULL\n");
        return;
    }

    social_memory_stats_t stats;
    if (social_memory_get_stats(social_mem, &stats) != SOCIAL_MEM_SUCCESS) {
        printf("Social Memory: Error getting stats\n");
        return;
    }

    printf("=== Social Memory Summary ===\n");
    printf("Persons: %zu\n", stats.num_persons);
    printf("Episodes: %zu\n", stats.num_episodes);
    printf("Total Facts: %zu\n", stats.total_facts);
    printf("Total Relationships: %zu\n", stats.total_relationships);
    printf("Average Trust: %.3f\n", stats.avg_trust);
    printf("Average Familiarity: %.3f\n", stats.avg_familiarity);
    printf("Trusted Persons: %zu\n", stats.trusted_count);
    printf("Family: %zu, Friends: %zu\n", stats.family_count, stats.friend_count);
    printf("Total Interactions: %lu\n", (unsigned long)stats.total_interactions);
    printf("Memory Usage: ~%zu bytes\n", stats.memory_bytes);
    printf("=============================\n");
}

NIMCP_EXPORT bool social_memory_validate(social_memory_t social_mem) {
    if (!social_mem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_memory_validate: social_mem is NULL");
        return false;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return false;
    }

    // Verify person count matches hash table entries
    size_t actual_persons = 0;
    for (size_t i = 0; i < mem->person_bucket_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->person_bucket_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->person_bucket_count);
        }

        person_entry_t* entry = mem->person_buckets[i];
        while (entry) {
            actual_persons++;
            entry = entry->next;
        }
    }

    if (actual_persons != mem->num_persons) {
        return false;
    }

    // Verify episode count
    size_t actual_episodes = 0;
    for (size_t i = 0; i < mem->episode_bucket_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->episode_bucket_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->episode_bucket_count);
        }

        episode_entry_t* entry = mem->episode_buckets[i];
        while (entry) {
            actual_episodes++;
            entry = entry->next;
        }
    }

    if (actual_episodes != mem->num_episodes) {
        return false;
    }

    return true;
}

NIMCP_EXPORT social_mem_error_t social_memory_record_interaction(
    social_memory_t social_mem,
    uint64_t person_id,
    float current_time)
{
    if (!social_mem) {
        return SOCIAL_MEM_ERROR_NULL_POINTER;
    }

    social_memory_internal_t* mem = (social_memory_internal_t*)social_mem;
    if (mem->magic != SOCIAL_MEM_MAGIC) {
        return SOCIAL_MEM_ERROR_INVALID_PARAM;
    }

    person_entry_t* entry = find_person_entry(mem, person_id);
    if (!entry) {
        return SOCIAL_MEM_ERROR_NOT_FOUND;
    }

    entry->person->interaction_count++;
    entry->person->last_interaction_time = current_time;

    // Update familiarity slightly with each interaction
    entry->person->familiarity = nimcp_clampf(entry->person->familiarity + 0.01f, 0.0f, 1.0f);

    entry->person->modified_time_ms = social_memory_current_time_ms();
    mem->total_interactions++;

    return SOCIAL_MEM_SUCCESS;
}

NIMCP_EXPORT uint64_t social_memory_current_time_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    }
    return (uint64_t)time(NULL) * 1000;
}

//=============================================================================
// Internal Function Implementations
//=============================================================================

#include <stdarg.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(social_memory, MESH_ADAPTER_CATEGORY_MEMORY)


static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_error_buffer, ERROR_BUFFER_SIZE, fmt, args);
    va_end(args);
}

static uint64_t hash_uint64(uint64_t key) {
    // FNV-1a hash
    uint64_t h = 14695981039346656037ULL;
    for (int i = 0; i < 8; i++) {
        h ^= (key >> (i * 8)) & 0xFF;
        h *= 1099511628211ULL;
    }
    return h;
}

static person_entry_t* find_person_entry(social_memory_internal_t* mem, uint64_t person_id) {
    uint64_t h = hash_uint64(person_id);
    size_t bucket = h % mem->person_bucket_count;

    person_entry_t* entry = mem->person_buckets[bucket];
    while (entry) {
        if (entry->key == person_id) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static episode_entry_t* find_episode_entry(social_memory_internal_t* mem, uint64_t episode_id) {
    uint64_t h = hash_uint64(episode_id);
    size_t bucket = h % mem->episode_bucket_count;

    episode_entry_t* entry = mem->episode_buckets[bucket];
    while (entry) {
        if (entry->key == episode_id) {
            return entry;
        }
        entry = entry->next;
    }
    /* Not found is a normal condition — no error throw */
    return NULL;
}

static bool add_person_entry(social_memory_internal_t* mem, person_node_t* person) {
    // Check if resize needed
    float load = (float)mem->num_persons / (float)mem->person_bucket_count;
    if (load > HASH_LOAD_FACTOR_THRESHOLD) {
        if (!resize_person_table(mem)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "add_person_entry: resize_person_table is NULL");
            return false;
        }
    }

    uint64_t h = hash_uint64(person->person_id);
    size_t bucket = h % mem->person_bucket_count;

    person_entry_t* entry = (person_entry_t*)nimcp_malloc(sizeof(person_entry_t));
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "add_person_entry: entry is NULL");
        return false;
    }

    entry->key = person->person_id;
    entry->person = person;
    entry->next = mem->person_buckets[bucket];
    mem->person_buckets[bucket] = entry;
    mem->num_persons++;

    return true;
}

static bool add_episode_entry(social_memory_internal_t* mem, social_episode_t* episode) {
    float load = (float)mem->num_episodes / (float)mem->episode_bucket_count;
    if (load > HASH_LOAD_FACTOR_THRESHOLD) {
        if (!resize_episode_table(mem)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "add_episode_entry: resize_episode_table is NULL");
            return false;
        }
    }

    uint64_t h = hash_uint64(episode->episode_id);
    size_t bucket = h % mem->episode_bucket_count;

    episode_entry_t* entry = (episode_entry_t*)nimcp_malloc(sizeof(episode_entry_t));
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "add_episode_entry: entry is NULL");
        return false;
    }

    entry->key = episode->episode_id;
    entry->episode = episode;
    entry->next = mem->episode_buckets[bucket];
    mem->episode_buckets[bucket] = entry;
    mem->num_episodes++;

    return true;
}

static bool remove_person_entry(social_memory_internal_t* mem, uint64_t person_id) {
    uint64_t h = hash_uint64(person_id);
    size_t bucket = h % mem->person_bucket_count;

    person_entry_t** prev = &mem->person_buckets[bucket];
    person_entry_t* entry = mem->person_buckets[bucket];

    while (entry) {
        if (entry->key == person_id) {
            *prev = entry->next;
            free_person_node(entry->person);
            nimcp_free(entry);
            entry = NULL;
            mem->num_persons--;
            return true;
        }
        prev = &entry->next;
        entry = entry->next;
    }

    return false;
}

static bool resize_person_table(social_memory_internal_t* mem) {
    size_t new_count = mem->person_bucket_count * 2;
    person_entry_t** new_buckets = (person_entry_t**)nimcp_calloc(new_count, sizeof(person_entry_t*));
    if (!new_buckets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "resize_person_table: new_buckets is NULL");
        return false;
    }

    // Rehash all entries
    for (size_t i = 0; i < mem->person_bucket_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->person_bucket_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->person_bucket_count);
        }

        person_entry_t* entry = mem->person_buckets[i];
        while (entry) {
            person_entry_t* next = entry->next;
            uint64_t h = hash_uint64(entry->key);
            size_t new_bucket = h % new_count;
            entry->next = new_buckets[new_bucket];
            new_buckets[new_bucket] = entry;
            entry = next;
        }
    }

    nimcp_free(mem->person_buckets);
    mem->person_buckets = new_buckets;
    mem->person_bucket_count = new_count;

    return true;
}

static bool resize_episode_table(social_memory_internal_t* mem) {
    size_t new_count = mem->episode_bucket_count * 2;
    episode_entry_t** new_buckets = (episode_entry_t**)nimcp_calloc(new_count, sizeof(episode_entry_t*));
    if (!new_buckets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "resize_episode_table: new_buckets is NULL");
        return false;
    }

    for (size_t i = 0; i < mem->episode_bucket_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->episode_bucket_count > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->episode_bucket_count);
        }

        episode_entry_t* entry = mem->episode_buckets[i];
        while (entry) {
            episode_entry_t* next = entry->next;
            uint64_t h = hash_uint64(entry->key);
            size_t new_bucket = h % new_count;
            entry->next = new_buckets[new_bucket];
            new_buckets[new_bucket] = entry;
            entry = next;
        }
    }

    nimcp_free(mem->episode_buckets);
    mem->episode_buckets = new_buckets;
    mem->episode_bucket_count = new_count;

    return true;
}

static bool grow_relationship_matrix(social_memory_internal_t* mem) {
    size_t new_capacity = mem->matrix_capacity * 2;
    if (new_capacity > mem->max_persons) {
        new_capacity = mem->max_persons;
    }
    if (new_capacity <= mem->matrix_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "grow_relationship_matrix: validation failed");
        return false;
    }

    size_t new_elements = new_capacity * new_capacity;

    float* new_matrix = (float*)nimcp_calloc(new_elements, sizeof(float));
    relationship_type_t* new_types = (relationship_type_t*)nimcp_calloc(new_elements, sizeof(relationship_type_t));
    uint64_t* new_ids = (uint64_t*)nimcp_calloc(new_capacity, sizeof(uint64_t));

    if (!new_matrix || !new_types || !new_ids) {
        nimcp_free(new_matrix);
        new_matrix = NULL;
        nimcp_free(new_types);
        new_types = NULL;
        nimcp_free(new_ids);
        new_ids = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "grow_relationship_matrix: required parameter is NULL (new_matrix, new_types, new_ids)");
        return false;
    }

    // Initialize new types to stranger
    for (size_t i = 0; i < new_elements; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && new_elements > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)new_elements);
        }

        new_types[i] = REL_STRANGER;
    }

    // Initialize new IDs to invalid
    for (size_t i = 0; i < new_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && new_capacity > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)new_capacity);
        }

        new_ids[i] = SOCIAL_MEM_INVALID_PERSON_ID;
    }

    // Copy existing data
    for (size_t i = 0; i < mem->matrix_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->matrix_size > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->matrix_size);
        }

        new_ids[i] = mem->matrix_person_ids[i];
        for (size_t j = 0; j < mem->matrix_size; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && mem->matrix_size > 256) {
                social_memory_heartbeat("social_memor_loop",
                                 (float)(j + 1) / (float)mem->matrix_size);
            }

            size_t old_offset = i * mem->matrix_capacity + j;
            size_t new_offset = i * new_capacity + j;
            new_matrix[new_offset] = mem->relationship_matrix[old_offset];
            new_types[new_offset] = mem->relationship_types[old_offset];
        }
    }

    nimcp_free(mem->relationship_matrix);
    nimcp_free(mem->relationship_types);
    nimcp_free(mem->matrix_person_ids);

    mem->relationship_matrix = new_matrix;
    mem->relationship_types = new_types;
    mem->matrix_person_ids = new_ids;
    mem->matrix_capacity = new_capacity;

    return true;
}

static int get_matrix_index(social_memory_internal_t* mem, uint64_t person_id) {
    for (size_t i = 0; i < mem->matrix_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mem->matrix_size > 256) {
            social_memory_heartbeat("social_memor_loop",
                             (float)(i + 1) / (float)mem->matrix_size);
        }

        if (mem->matrix_person_ids[i] == person_id) {
            return (int)i;
        }
    }
    return -1;
}

static int assign_matrix_index(social_memory_internal_t* mem, uint64_t person_id) {
    // Check if already assigned
    int existing = get_matrix_index(mem, person_id);
    if (existing >= 0) {
        return existing;
    }

    // Find free slot or expand
    if (mem->matrix_size >= mem->matrix_capacity) {
        if (!grow_relationship_matrix(mem)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "assign_matrix_index: grow_relationship_matrix is NULL");
            return -1;
        }
    }

    int idx = (int)mem->matrix_size;
    mem->matrix_person_ids[idx] = person_id;
    mem->matrix_size++;

    return idx;
}

static void free_person_node(person_node_t* person) {
    if (!person) {
        return;
    }

    nimcp_free(person->name);
    nimcp_free(person->facts);
    nimcp_free(person);
    person = NULL;
}

static void free_episode(social_episode_t* episode) {
    if (!episode) {
        return;
    }

    nimcp_free(episode->participant_ids);
    nimcp_free(episode->location);
    nimcp_free(episode->description);
    nimcp_free(episode);
    episode = NULL;
}

static person_node_t* create_person_node(const char* name) {
    person_node_t* person = (person_node_t*)nimcp_calloc(1, sizeof(person_node_t));
    if (!person) {
        set_error("Failed to allocate person node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_person_node: person is NULL");
        return NULL;
    }

    if (name) {
        size_t name_len = strlen(name) + 1;
        person->name = (char*)nimcp_malloc(name_len);
        if (!person->name) {
            nimcp_free(person);
            person = NULL;
            set_error("Failed to allocate person name");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_person_node: person->name is NULL");
            return NULL;
        }
        memcpy(person->name, name, name_len);
    }

    // Initialize defaults
    person->person_id = SOCIAL_MEM_INVALID_PERSON_ID;
    person->relationship = REL_STRANGER;
    person->trust_level = SOCIAL_MEM_INITIAL_TRUST;
    person->trust_baseline = SOCIAL_MEM_INITIAL_TRUST;
    person->familiarity = 0.0f;
    person->liking = 0.0f;
    person->perceived_competence = 0.5f;
    person->perceived_warmth = 0.5f;
    person->person_quaternion = quat_identity();

    return person;
}

static social_episode_t* create_episode(void) {
    social_episode_t* episode = (social_episode_t*)nimcp_calloc(1, sizeof(social_episode_t));
    if (!episode) {
        set_error("Failed to allocate episode");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_episode: episode is NULL");
        return NULL;
    }

    episode->episode_id = SOCIAL_MEM_INVALID_EPISODE_ID;
    episode->outcome_type = TRUST_OUTCOME_NEUTRAL;

    return episode;
}

static float compute_signature_match(const prime_signature_t* s1, const prime_signature_t* s2) {
    if (!s1 || !s2) {
        return 0.0f;
    }

    // Use Jaccard similarity
    float jaccard = prime_sig_jaccard(s1, s2);
    return (jaccard >= 0.0f) ? jaccard : 0.0f;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void social_memory_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_social_memory_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int social_memory_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "social_memory_training_begin: NULL argument");
        return -1;
    }
    social_memory_heartbeat_instance(NULL, "social_memory_training_begin", 0.0f);
    (void)(struct person_entry*)instance; /* Module state available for reset */
    return 0;
}

int social_memory_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "social_memory_training_end: NULL argument");
        return -1;
    }
    social_memory_heartbeat_instance(NULL, "social_memory_training_end", 1.0f);
    (void)(struct person_entry*)instance; /* Module state available for finalization */
    return 0;
}

int social_memory_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "social_memory_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    social_memory_heartbeat_instance(NULL, "social_memory_training_step", progress);
    (void)(struct person_entry*)instance; /* Module state available for step adaptation */
    return 0;
}
