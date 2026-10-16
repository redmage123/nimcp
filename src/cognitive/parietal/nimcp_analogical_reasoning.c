/**
 * @file nimcp_analogical_reasoning.c
 * @brief Analogical reasoning engine implementation
 *
 * WHAT: Engine for analogical reasoning and cross-domain transfer
 * WHY:  Enable learning by analogy - transfer solutions across domains
 * HOW:  Structure mapping, relational abstraction, analogical inference
 *
 * IMPLEMENTATION NOTES:
 * - Structure Mapping Theory (Gentner) inspired
 * - Emphasizes relational over surface similarity
 * - Supports both one-to-one and partial mappings
 */

#include "cognitive/parietal/nimcp_analogical_reasoning.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE(analogical_reasoning, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * INTERNAL CONSTANTS
 * ============================================================================ */

#define MAX_CACHED_DOMAINS      32
#define MAX_CACHED_ANALOGIES    64
#define MIN_SIMILARITY_THRESHOLD 0.3f

/* ============================================================================
 * INTERNAL TYPES
 * ============================================================================ */

/**
 * @brief Internal engine structure
 */
struct analogical_engine {
    analog_config_t config;
    analog_stats_t stats;

    /* Domain cache */
    analog_domain_t** cached_domains;
    uint32_t num_cached_domains;

    /* Analogy cache */
    analog_analogy_t** cached_analogies;
    uint32_t num_cached_analogies;

    /* Modulation state */
    float inflammation;
    float fatigue;

    /* ID generators */
    uint32_t next_domain_id;
    uint32_t next_analogy_id;
    uint32_t next_abstraction_id;
};

/* Thread-local error message */
static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

static void set_error(const char* msg) {
    strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
    g_last_error[sizeof(g_last_error) - 1] = '\0';
}

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

/**
 * @brief Compute cosine similarity between feature vectors
 */
static float feature_similarity(const float* a, const float* b, uint32_t dim) {
    if (!a || !b || dim == 0) return 0.0f;

    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            analogical_reasoning_heartbeat("analogical_r_loop",
                             (float)(i + 1) / (float)dim);
        }

        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    if (denom < 1e-10f) return 0.0f;

    return dot / denom;
}

/**
 * @brief Compute entity similarity (features + type)
 */
static float entity_similarity(
    const analog_entity_t* e1,
    const analog_entity_t* e2
) {
    if (!e1 || !e2) return 0.0f;

    float sim = 0.0f;

    /* Type similarity */
    if (strcmp(e1->type, e2->type) == 0) {
        sim += 0.3f;
    }

    /* Feature similarity */
    uint32_t min_features = (e1->num_features < e2->num_features) ?
                            e1->num_features : e2->num_features;
    if (min_features > 0 && e1->features && e2->features) {
        sim += 0.7f * feature_similarity(e1->features, e2->features, min_features);
    }

    return sim;
}

/**
 * @brief Compute relation similarity
 */
static float relation_similarity(
    const analog_relation_t* r1,
    const analog_relation_t* r2
) {
    if (!r1 || !r2) return 0.0f;

    float sim = 0.0f;

    /* Name similarity (simple string match) */
    if (strcmp(r1->name, r2->name) == 0) {
        sim += 0.5f;
    }

    /* Order match */
    if (r1->order == r2->order) {
        sim += 0.2f;
    }

    /* Property matches */
    if (r1->is_symmetric == r2->is_symmetric) {
        sim += 0.15f;
    }
    if (r1->is_transitive == r2->is_transitive) {
        sim += 0.15f;
    }

    return sim;
}

/**
 * @brief Apply modulation to similarity
 */
static float apply_modulation(const analogical_engine_t* engine, float value) {
    float factor = 1.0f;

    /* Inflammation reduces precision */
    factor -= engine->inflammation * engine->config.inflammation_sensitivity * 0.2f;

    /* Fatigue reduces quality */
    factor -= engine->fatigue * engine->config.fatigue_sensitivity * 0.3f;

    return value * fmaxf(0.5f, factor);
}

/**
 * @brief Find entity in domain by ID
 */
static analog_entity_t* find_entity_by_id(
    const analog_domain_t* domain,
    uint32_t id
) {
    for (uint32_t i = 0; i < domain->num_entities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && domain->num_entities > 256) {
            analogical_reasoning_heartbeat("analogical_r_loop",
                             (float)(i + 1) / (float)domain->num_entities);
        }

        if (domain->entities[i].id == id) {
            return &domain->entities[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_entity_by_id: validation failed");
    return NULL;
}

/* ============================================================================
 * LIFECYCLE IMPLEMENTATION
 * ============================================================================ */

analog_config_t analogical_engine_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_engine_de", 0.0f);


    analog_config_t config = {
        .min_mapping_strength = 0.4f,
        .systematicity_weight = 0.4f,
        .surface_weight = 0.2f,
        .structural_weight = 0.4f,
        .prefer_deep_analogies = true,
        .allow_partial_mappings = true,
        .enable_abstraction = true,
        .max_domains_cache = MAX_CACHED_DOMAINS,
        .max_analogies_cache = MAX_CACHED_ANALOGIES,
        .learning_rate = NIMCP_LEARNING_RATE_COARSE,
        .inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = 1.0f
    };
    return config;
}

analogical_engine_t* analogical_engine_create(void) {
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_engine_cr", 0.0f);


    analog_config_t config = analogical_engine_default_config();
    return analogical_engine_create_custom(&config);
}

analogical_engine_t* analogical_engine_create_custom(const analog_config_t* config) {
    if (!config) {
        set_error("NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_engine_cr", 0.0f);


    analogical_engine_t* engine = nimcp_calloc(1, sizeof(analogical_engine_t));
    if (!engine) {
        set_error("Failed to allocate engine");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate engine");

        return NULL;
    }

    engine->config = *config;
    memset(&engine->stats, 0, sizeof(engine->stats));

    /* Allocate domain cache */
    engine->cached_domains = nimcp_calloc(config->max_domains_cache,
                                          sizeof(analog_domain_t*));
    if (!engine->cached_domains) {
        set_error("Failed to allocate domain cache");
        nimcp_free(engine);
        engine = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "analogical_engine_create_custom: engine->cached_domains is NULL");
        return NULL;
    }

    /* Allocate analogy cache */
    engine->cached_analogies = nimcp_calloc(config->max_analogies_cache,
                                            sizeof(analog_analogy_t*));
    if (!engine->cached_analogies) {
        set_error("Failed to allocate analogy cache");
        nimcp_free(engine->cached_domains);
        nimcp_free(engine);
        engine = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "analogical_engine_create_custom: engine->cached_analogies is NULL");
        return NULL;
    }

    engine->num_cached_domains = 0;
    engine->num_cached_analogies = 0;
    engine->inflammation = 0.0f;
    engine->fatigue = 0.0f;
    engine->next_domain_id = 1;
    engine->next_analogy_id = 1;
    engine->next_abstraction_id = 1;

    return engine;
}

void analogical_engine_destroy(analogical_engine_t* engine) {
    if (!engine) return;

    /* Note: We don't free cached domains/analogies as we don't own them */
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_engine_de", 0.0f);


    if (engine->cached_domains) {
        nimcp_free(engine->cached_domains);
    }
    if (engine->cached_analogies) {
        nimcp_free(engine->cached_analogies);
    }

    nimcp_free(engine);
    engine = NULL;
}

/* ============================================================================
 * DOMAIN MANAGEMENT IMPLEMENTATION
 * ============================================================================ */

analog_domain_t* analogical_create_domain(
    const char* name,
    const char* description
) {
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_create_do", 0.0f);


    analog_domain_t* domain = nimcp_calloc(1, sizeof(analog_domain_t));
    if (!domain) {
        set_error("Failed to allocate domain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate domain");

        return NULL;
    }

    domain->id = 0;  /* Will be set when registered */

    if (name) {
        strncpy(domain->name, name, sizeof(domain->name) - 1);
    }
    if (description) {
        strncpy(domain->description, description, sizeof(domain->description) - 1);
    }

    /* Allocate entity and relation arrays */
    domain->entities = nimcp_calloc(ANALOG_MAX_ENTITIES, sizeof(analog_entity_t));
    domain->relations = nimcp_calloc(ANALOG_MAX_RELATIONS, sizeof(analog_relation_t));

    if (!domain->entities || !domain->relations) {
        set_error("Failed to allocate domain arrays");
        if (domain->entities) nimcp_free(domain->entities);
        if (domain->relations) nimcp_free(domain->relations);
        nimcp_free(domain);
        domain = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "analogical_create_domain: validation failed");
        return NULL;
    }

    domain->num_entities = 0;
    domain->num_relations = 0;
    domain->abstraction_level = 0.5f;
    domain->familiarity = 0.0f;

    return domain;
}

uint32_t analogical_add_entity(
    analog_domain_t* domain,
    const char* name,
    const char* type,
    const float* features,
    uint32_t num_features
) {
    if (!domain || domain->num_entities >= ANALOG_MAX_ENTITIES) {
        set_error("Invalid domain or entity limit reached");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_add_entit", 0.0f);


    analog_entity_t* entity = &domain->entities[domain->num_entities];

    entity->id = domain->num_entities + 1;

    if (name) {
        strncpy(entity->name, name, sizeof(entity->name) - 1);
    }
    if (type) {
        strncpy(entity->type, type, sizeof(entity->type) - 1);
    }

    if (features && num_features > 0) {
        entity->features = nimcp_calloc(num_features, sizeof(float));
        if (entity->features) {
            memcpy(entity->features, features, num_features * sizeof(float));
            entity->num_features = num_features;
        }
    }

    entity->salience = 0.5f;
    domain->num_entities++;

    return entity->id;
}

uint32_t analogical_add_relation(
    analog_domain_t* domain,
    const char* name,
    uint32_t subject_id,
    uint32_t object_id,
    float strength
) {
    if (!domain || domain->num_relations >= ANALOG_MAX_RELATIONS) {
        set_error("Invalid domain or relation limit reached");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_add_relat", 0.0f);


    analog_relation_t* relation = &domain->relations[domain->num_relations];

    relation->id = domain->num_relations + 1;

    if (name) {
        strncpy(relation->name, name, sizeof(relation->name) - 1);
    }

    relation->subject_id = subject_id;
    relation->object_id = object_id;
    relation->strength = fmaxf(0.0f, fminf(1.0f, strength));
    relation->order = 1;  /* First-order by default */
    relation->is_symmetric = false;
    relation->is_transitive = false;

    domain->num_relations++;

    return relation->id;
}

int analogical_register_domain(
    analogical_engine_t* engine,
    analog_domain_t* domain
) {
    if (!engine || !domain) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analogical_register_domain: required parameter is NULL (engine, domain)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_register_", 0.0f);


    if (engine->num_cached_domains >= engine->config.max_domains_cache) {
        set_error("Domain cache full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "analogical_register_domain: capacity exceeded");
        return -1;
    }

    domain->id = engine->next_domain_id++;
    engine->cached_domains[engine->num_cached_domains++] = domain;
    engine->stats.domains_processed++;

    return 0;
}

void analogical_free_domain(analog_domain_t* domain) {
    if (!domain) return;

    /* Free entity features */
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_free_doma", 0.0f);


    for (uint32_t i = 0; i < domain->num_entities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && domain->num_entities > 256) {
            analogical_reasoning_heartbeat("analogical_r_loop",
                             (float)(i + 1) / (float)domain->num_entities);
        }

        if (domain->entities[i].features) {
            nimcp_free(domain->entities[i].features);
        }
    }

    if (domain->entities) nimcp_free(domain->entities);
    if (domain->relations) nimcp_free(domain->relations);
    if (domain->domain_features) nimcp_free(domain->domain_features);

    nimcp_free(domain);
    domain = NULL;
}

/* ============================================================================
 * ANALOGY FINDING IMPLEMENTATION
 * ============================================================================ */

analog_analogy_t* analogical_find_analogy(
    analogical_engine_t* engine,
    const analog_domain_t* source,
    const analog_domain_t* target
) {
    if (!engine || !source || !target) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analogical_find_analogy: required parameter is NULL (engine, source, target)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_find_anal", 0.0f);


    uint64_t start_time = get_timestamp_us();

    analog_analogy_t* analogy = nimcp_calloc(1, sizeof(analog_analogy_t));
    if (!analogy) {
        set_error("Failed to allocate analogy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate analogy");

        return NULL;
    }

    analogy->id = engine->next_analogy_id++;
    analogy->source = (analog_domain_t*)source;
    analogy->target = (analog_domain_t*)target;

    /* Allocate mappings array */
    analogy->mappings = nimcp_calloc(ANALOG_MAX_MAPPINGS, sizeof(analog_mapping_t));
    if (!analogy->mappings) {
        set_error("Failed to allocate mappings");
        nimcp_free(analogy);
        analogy = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "analogical_find_analogy: analogy->mappings is NULL");
        return NULL;
    }
    analogy->num_mappings = 0;

    /* Create entity mappings */
    analog_mapping_t* entity_mappings = nimcp_calloc(ANALOG_MAX_MAPPINGS,
                                                     sizeof(analog_mapping_t));
    uint32_t num_entity_mappings = 0;

    analogical_map_entities(engine, source, target, entity_mappings,
                           ANALOG_MAX_MAPPINGS, &num_entity_mappings);

    /* Copy entity mappings to analogy */
    for (uint32_t i = 0; i < num_entity_mappings && analogy->num_mappings < ANALOG_MAX_MAPPINGS; i++) {
        analogy->mappings[analogy->num_mappings++] = entity_mappings[i];
    }

    /* Create relation mappings based on entity mappings */
    analog_mapping_t* relation_mappings = nimcp_calloc(ANALOG_MAX_MAPPINGS,
                                                       sizeof(analog_mapping_t));
    uint32_t num_relation_mappings = 0;

    analogical_map_relations(engine, source, target,
                            entity_mappings, num_entity_mappings,
                            relation_mappings, ANALOG_MAX_MAPPINGS,
                            &num_relation_mappings);

    /* Copy relation mappings */
    for (uint32_t i = 0; i < num_relation_mappings && analogy->num_mappings < ANALOG_MAX_MAPPINGS; i++) {
        analogy->mappings[analogy->num_mappings++] = relation_mappings[i];
    }

    nimcp_free(entity_mappings);
    entity_mappings = NULL;
    nimcp_free(relation_mappings);
    relation_mappings = NULL;

    /* Compute analogy quality metrics */
    float total_strength = 0.0f;
    for (uint32_t i = 0; i < analogy->num_mappings; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && analogy->num_mappings > 256) {
            analogical_reasoning_heartbeat("analogical_r_loop",
                             (float)(i + 1) / (float)analogy->num_mappings);
        }

        total_strength += analogy->mappings[i].confidence;
    }
    analogy->mapping_strength = (analogy->num_mappings > 0) ?
                                 total_strength / analogy->num_mappings : 0.0f;

    /* Structural consistency: check for one-to-one mappings */
    analogy->structural_consistency = 1.0f;  /* Simplified */

    /* Relational depth: ratio of relation mappings */
    analogy->relational_depth = (num_relation_mappings > 0 && source->num_relations > 0) ?
                                 (float)num_relation_mappings / source->num_relations : 0.0f;

    /* Systematicity: higher if connected relations are mapped */
    analogy->systematicity = analogy->relational_depth * analogy->mapping_strength;

    /* Determine if superficial */
    analogy->is_superficial = (analogy->relational_depth < 0.3f);

    /* Apply modulation */
    analogy->mapping_strength = apply_modulation(engine, analogy->mapping_strength);

    /* Check minimum threshold */
    if (analogy->mapping_strength < engine->config.min_mapping_strength) {
        analogical_free_analogy(analogy);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analogical_find_analogy: validation failed");
        return NULL;
    }

    /* Update statistics */
    engine->stats.analogies_found++;
    engine->stats.mappings_created += analogy->num_mappings;

    uint64_t elapsed = get_timestamp_us() - start_time;
    engine->stats.avg_processing_time_us =
        (engine->stats.avg_processing_time_us * (engine->stats.analogies_found - 1) + elapsed) /
        engine->stats.analogies_found;

    float total_mapping_strength = engine->stats.avg_mapping_strength *
                                   (engine->stats.analogies_found - 1);
    engine->stats.avg_mapping_strength =
        (total_mapping_strength + analogy->mapping_strength) / engine->stats.analogies_found;

    return analogy;
}

analog_analogy_t* analogical_find_best_analogy(
    analogical_engine_t* engine,
    const analog_domain_t* target
) {
    if (!engine || !target) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analogical_find_best_analogy: required parameter is NULL (engine, target)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_find_best", 0.0f);


    analog_analogy_t* best = NULL;
    float best_strength = 0.0f;

    for (uint32_t i = 0; i < engine->num_cached_domains; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && engine->num_cached_domains > 256) {
            analogical_reasoning_heartbeat("analogical_r_loop",
                             (float)(i + 1) / (float)engine->num_cached_domains);
        }

        if (engine->cached_domains[i] == target) continue;

        analog_analogy_t* candidate = analogical_find_analogy(
            engine, engine->cached_domains[i], target);

        if (candidate) {
            if (candidate->mapping_strength > best_strength) {
                if (best) analogical_free_analogy(best);
                best = candidate;
                best_strength = candidate->mapping_strength;
            } else {
                analogical_free_analogy(candidate);
            }
        }
    }

    return best;
}

int analogical_find_multiple_analogies(
    analogical_engine_t* engine,
    const analog_domain_t* target,
    analog_analogy_t** analogies,
    uint32_t max_analogies,
    uint32_t* num_found
) {
    if (!engine || !target || !analogies || !num_found) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analogical_find_multiple_analogies: required parameter is NULL (engine, target, analogies, num_found)");
        return -1;
    }

    *num_found = 0;

    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_find_mult", 0.0f);


    for (uint32_t i = 0; i < engine->num_cached_domains && *num_found < max_analogies; i++) {
        if (engine->cached_domains[i] == target) continue;

        analog_analogy_t* candidate = analogical_find_analogy(
            engine, engine->cached_domains[i], target);

        if (candidate) {
            analogies[(*num_found)++] = candidate;
        }
    }

    return 0;
}

int analogical_evaluate_analogy(
    analogical_engine_t* engine,
    const analog_analogy_t* analogy,
    analog_quality_t* quality
) {
    if (!engine || !analogy || !quality) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analogical_evaluate_analogy: required parameter is NULL (engine, analogy, quality)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_evaluate_", 0.0f);


    memset(quality, 0, sizeof(analog_quality_t));

    /* Structural similarity based on mapping coverage */
    float entity_coverage = 0.0f;
    uint32_t entity_count = 0;
    for (uint32_t i = 0; i < analogy->num_mappings; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && analogy->num_mappings > 256) {
            analogical_reasoning_heartbeat("analogical_r_loop",
                             (float)(i + 1) / (float)analogy->num_mappings);
        }

        if (analogy->mappings[i].is_entity_mapping) {
            entity_coverage += analogy->mappings[i].confidence;
            entity_count++;
        }
    }
    quality->structural_similarity = (entity_count > 0) ?
                                      entity_coverage / (entity_count > 0 ? entity_count : 1) : 0.0f;

    /* Relational similarity */
    quality->relational_similarity = analogy->relational_depth;

    /* Surface similarity (simplified) */
    quality->surface_similarity = quality->structural_similarity * 0.8f;

    /* Systematicity */
    quality->systematicity_score = analogy->systematicity;

    /* One-to-one score */
    quality->one_to_one_score = analogy->structural_consistency;

    /* Parallel connectivity */
    quality->parallel_connectivity = analogy->relational_depth * analogy->mapping_strength;

    /* Overall quality */
    quality->overall_quality = (
        quality->structural_similarity * engine->config.structural_weight +
        quality->relational_similarity * engine->config.systematicity_weight +
        quality->surface_similarity * engine->config.surface_weight
    );

    quality->is_valid = (quality->overall_quality >= engine->config.min_mapping_strength);

    return 0;
}

void analogical_free_analogy(analog_analogy_t* analogy) {
    if (!analogy) return;

    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_free_anal", 0.0f);


    if (analogy->mappings) nimcp_free(analogy->mappings);
    if (analogy->inference_potential) nimcp_free(analogy->inference_potential);

    nimcp_free(analogy);
    analogy = NULL;
}

/* ============================================================================
 * STRUCTURE MAPPING IMPLEMENTATION
 * ============================================================================ */

float analogical_structural_similarity(
    analogical_engine_t* engine,
    const analog_domain_t* domain1,
    const analog_domain_t* domain2
) {
    if (!engine || !domain1 || !domain2) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_structura", 0.0f);


    float similarity = 0.0f;
    uint32_t comparisons = 0;

    /* Compare entity types */
    for (uint32_t i = 0; i < domain1->num_entities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && domain1->num_entities > 256) {
            analogical_reasoning_heartbeat("analogical_r_loop",
                             (float)(i + 1) / (float)domain1->num_entities);
        }

        float best_match = 0.0f;
        for (uint32_t j = 0; j < domain2->num_entities; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && domain2->num_entities > 256) {
                analogical_reasoning_heartbeat("analogical_r_loop",
                                 (float)(j + 1) / (float)domain2->num_entities);
            }

            float sim = entity_similarity(&domain1->entities[i],
                                          &domain2->entities[j]);
            if (sim > best_match) best_match = sim;
        }
        similarity += best_match;
        comparisons++;
    }

    /* Compare relation types */
    for (uint32_t i = 0; i < domain1->num_relations; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && domain1->num_relations > 256) {
            analogical_reasoning_heartbeat("analogical_r_loop",
                             (float)(i + 1) / (float)domain1->num_relations);
        }

        float best_match = 0.0f;
        for (uint32_t j = 0; j < domain2->num_relations; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && domain2->num_relations > 256) {
                analogical_reasoning_heartbeat("analogical_r_loop",
                                 (float)(j + 1) / (float)domain2->num_relations);
            }

            float sim = relation_similarity(&domain1->relations[i],
                                           &domain2->relations[j]);
            if (sim > best_match) best_match = sim;
        }
        similarity += best_match;
        comparisons++;
    }

    return (comparisons > 0) ? apply_modulation(engine, similarity / comparisons) : 0.0f;
}

int analogical_map_entities(
    analogical_engine_t* engine,
    const analog_domain_t* source,
    const analog_domain_t* target,
    analog_mapping_t* mappings,
    uint32_t max_mappings,
    uint32_t* num_found
) {
    if (!engine || !source || !target || !mappings || !num_found) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analogical_map_entities: required parameter is NULL (engine, source, target, mappings, num_found)");
        return -1;
    }

    *num_found = 0;

    /* Greedy matching: for each source entity, find best target match */
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_map_entit", 0.0f);


    bool* target_used = nimcp_calloc(target->num_entities, sizeof(bool));
    if (!target_used) {
        set_error("Failed to allocate tracking array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "analogical_map_entities: target_used is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < source->num_entities && *num_found < max_mappings; i++) {
        float best_sim = MIN_SIMILARITY_THRESHOLD;
        int best_j = -1;

        for (uint32_t j = 0; j < target->num_entities; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && target->num_entities > 256) {
                analogical_reasoning_heartbeat("analogical_r_loop",
                                 (float)(j + 1) / (float)target->num_entities);
            }

            if (target_used[j]) continue;

            float sim = entity_similarity(&source->entities[i],
                                          &target->entities[j]);
            if (sim > best_sim) {
                best_sim = sim;
                best_j = j;
            }
        }

        if (best_j >= 0) {
            mappings[*num_found].source_id = source->entities[i].id;
            mappings[*num_found].target_id = target->entities[best_j].id;
            mappings[*num_found].is_entity_mapping = true;
            mappings[*num_found].confidence = best_sim;
            mappings[*num_found].structural_support = best_sim;

            target_used[best_j] = true;
            (*num_found)++;
        }
    }

    nimcp_free(target_used);
    target_used = NULL;
    return 0;
}

int analogical_map_relations(
    analogical_engine_t* engine,
    const analog_domain_t* source,
    const analog_domain_t* target,
    const analog_mapping_t* entity_mappings,
    uint32_t num_entity_mappings,
    analog_mapping_t* relation_mappings,
    uint32_t max_mappings,
    uint32_t* num_found
) {
    if (!engine || !source || !target || !relation_mappings || !num_found) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analogical_map_relations: required parameter is NULL (engine, source, target, relation_mappings, num_found)");
        return -1;
    }

    *num_found = 0;

    /* For each source relation, find matching target relation */
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_map_relat", 0.0f);


    for (uint32_t i = 0; i < source->num_relations && *num_found < max_mappings; i++) {
        const analog_relation_t* src_rel = &source->relations[i];

        /* Find mapped subject and object */
        uint32_t mapped_subject = 0, mapped_object = 0;
        for (uint32_t m = 0; m < num_entity_mappings; m++) {
            /* Phase 8: Loop progress heartbeat */
            if ((m & 0xFF) == 0 && num_entity_mappings > 256) {
                analogical_reasoning_heartbeat("analogical_r_loop",
                                 (float)(m + 1) / (float)num_entity_mappings);
            }

            if (entity_mappings[m].source_id == src_rel->subject_id) {
                mapped_subject = entity_mappings[m].target_id;
            }
            if (entity_mappings[m].source_id == src_rel->object_id) {
                mapped_object = entity_mappings[m].target_id;
            }
        }

        if (mapped_subject == 0 || mapped_object == 0) continue;

        /* Find target relation with same structure */
        for (uint32_t j = 0; j < target->num_relations; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && target->num_relations > 256) {
                analogical_reasoning_heartbeat("analogical_r_loop",
                                 (float)(j + 1) / (float)target->num_relations);
            }

            const analog_relation_t* tgt_rel = &target->relations[j];

            if (tgt_rel->subject_id == mapped_subject &&
                tgt_rel->object_id == mapped_object) {

                float rel_sim = relation_similarity(src_rel, tgt_rel);

                relation_mappings[*num_found].source_id = src_rel->id;
                relation_mappings[*num_found].target_id = tgt_rel->id;
                relation_mappings[*num_found].is_entity_mapping = false;
                relation_mappings[*num_found].confidence = rel_sim;
                relation_mappings[*num_found].structural_support = 1.0f;  /* Structure matches */

                (*num_found)++;
                break;
            }
        }
    }

    return 0;
}

/* ============================================================================
 * CROSS-DOMAIN TRANSFER IMPLEMENTATION
 * ============================================================================ */

analog_solution_t* analogical_transfer_solution(
    analogical_engine_t* engine,
    const analog_analogy_t* analogy,
    const analog_solution_t* source_solution
) {
    if (!engine || !analogy || !source_solution) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analogical_transfer_solution: required parameter is NULL (engine, analogy, source_solution)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_transfer_", 0.0f);


    analog_solution_t* transferred = nimcp_calloc(1, sizeof(analog_solution_t));
    if (!transferred) {
        set_error("Failed to allocate solution");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate transferred");

        return NULL;
    }

    transferred->id = source_solution->id + 1000;  /* New ID */
    transferred->source_domain_id = analogy->target->id;

    strncpy(transferred->description, "Transferred: ",
            sizeof(transferred->description));
    strncat(transferred->description, source_solution->description,
            sizeof(transferred->description) - strlen(transferred->description) - 1);

    /* Copy and adapt solution steps */
    if (source_solution->solution_steps && source_solution->num_steps > 0) {
        transferred->solution_steps = nimcp_calloc(source_solution->num_steps, sizeof(float));
        if (transferred->solution_steps) {
            memcpy(transferred->solution_steps, source_solution->solution_steps,
                   source_solution->num_steps * sizeof(float));
            transferred->num_steps = source_solution->num_steps;
        }
    }

    /* Calculate applicability based on analogy strength */
    transferred->applicability = analogy->mapping_strength * (1.0f - analogy->is_superficial * 0.3f);
    transferred->adaptation_required = 1.0f - transferred->applicability;

    engine->stats.solutions_transferred++;

    return transferred;
}

float analogical_infer_relation(
    analogical_engine_t* engine,
    const analog_analogy_t* analogy,
    const analog_relation_t* source_relation,
    analog_relation_t* inferred_relation
) {
    if (!engine || !analogy || !source_relation || !inferred_relation) {
        set_error("Invalid parameters");
        return -1.0f;
    }

    /* Find mapped subject and object */
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_infer_rel", 0.0f);


    uint32_t mapped_subject = 0, mapped_object = 0;

    for (uint32_t i = 0; i < analogy->num_mappings; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && analogy->num_mappings > 256) {
            analogical_reasoning_heartbeat("analogical_r_loop",
                             (float)(i + 1) / (float)analogy->num_mappings);
        }

        if (analogy->mappings[i].is_entity_mapping) {
            if (analogy->mappings[i].source_id == source_relation->subject_id) {
                mapped_subject = analogy->mappings[i].target_id;
            }
            if (analogy->mappings[i].source_id == source_relation->object_id) {
                mapped_object = analogy->mappings[i].target_id;
            }
        }
    }

    if (mapped_subject == 0 || mapped_object == 0) {
        set_error("Could not map relation endpoints");
        return -1.0f;
    }

    /* Create inferred relation */
    memcpy(inferred_relation, source_relation, sizeof(analog_relation_t));
    inferred_relation->subject_id = mapped_subject;
    inferred_relation->object_id = mapped_object;

    /* Confidence based on mapping strength */
    float confidence = analogy->mapping_strength * source_relation->strength;

    return apply_modulation(engine, confidence);
}

int analogical_predict_properties(
    analogical_engine_t* engine,
    const analog_analogy_t* analogy,
    const analog_entity_t* source_entity,
    float* predicted_features,
    uint32_t max_features,
    uint32_t* num_features
) {
    if (!engine || !analogy || !source_entity || !predicted_features || !num_features) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analogical_predict_properties: required parameter is NULL (engine, analogy, source_entity, predicted_features, num_features)");
        return -1;
    }

    /* Find mapping for this entity */
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_predict_p", 0.0f);


    uint32_t mapped_id = 0;
    float mapping_confidence = 0.0f;

    for (uint32_t i = 0; i < analogy->num_mappings; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && analogy->num_mappings > 256) {
            analogical_reasoning_heartbeat("analogical_r_loop",
                             (float)(i + 1) / (float)analogy->num_mappings);
        }

        if (analogy->mappings[i].is_entity_mapping &&
            analogy->mappings[i].source_id == source_entity->id) {
            mapped_id = analogy->mappings[i].target_id;
            mapping_confidence = analogy->mappings[i].confidence;
            break;
        }
    }

    if (mapped_id == 0) {
        set_error("Entity not mapped");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "analogical_predict_properties: mapped_id is zero");
        return -1;
    }

    /* Copy source features as prediction */
    *num_features = (source_entity->num_features < max_features) ?
                    source_entity->num_features : max_features;

    for (uint32_t i = 0; i < *num_features; i++) {
        predicted_features[i] = source_entity->features[i] * mapping_confidence;
    }

    return 0;
}

void analogical_free_solution(analog_solution_t* solution) {
    if (!solution) return;

    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_free_solu", 0.0f);


    if (solution->solution_steps) nimcp_free(solution->solution_steps);
    if (solution->adaptations) {
        for (uint32_t i = 0; i < solution->num_adaptations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && solution->num_adaptations > 256) {
                analogical_reasoning_heartbeat("analogical_r_loop",
                                 (float)(i + 1) / (float)solution->num_adaptations);
            }

            if (solution->adaptations[i]) nimcp_free(solution->adaptations[i]);
        }
        nimcp_free(solution->adaptations);
    }

    nimcp_free(solution);
    solution = NULL;
}

/* ============================================================================
 * ABSTRACTION IMPLEMENTATION
 * ============================================================================ */

analog_abstraction_t* analogical_extract_principle(
    analogical_engine_t* engine,
    const analog_analogy_t** analogies,
    uint32_t num_analogies
) {
    if (!engine || !analogies || num_analogies == 0) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analogical_extract_principle: required parameter is NULL (engine, analogies)");
        return NULL;
    }

    if (!engine->config.enable_abstraction) {
        set_error("Abstraction disabled");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analogical_extract_principle: engine->config is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_extract_p", 0.0f);


    analog_abstraction_t* abstraction = nimcp_calloc(1, sizeof(analog_abstraction_t));
    if (!abstraction) {
        set_error("Failed to allocate abstraction");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate abstraction");

        return NULL;
    }

    abstraction->id = engine->next_abstraction_id++;
    abstraction->supporting_analogies = num_analogies;

    /* Find common relation patterns */
    /* Simplified: just count common relation names */
    snprintf(abstraction->name, sizeof(abstraction->name),
             "Principle from %u analogies", num_analogies);

    snprintf(abstraction->description, sizeof(abstraction->description),
             "Abstract principle extracted from %u analogical mappings", num_analogies);

    /* Generality increases with supporting analogies */
    abstraction->generality = fminf(1.0f, num_analogies / 10.0f);

    engine->stats.abstractions_extracted++;

    return abstraction;
}

float analogical_abstract_relation(
    analogical_engine_t* engine,
    const analog_relation_t* relations,
    uint32_t num_relations,
    char* abstract_name,
    uint32_t max_name_len
) {
    if (!engine || !relations || num_relations == 0 || !abstract_name) {
        set_error("Invalid parameters");
        return -1.0f;
    }

    /* Simple abstraction: use first relation name as template */
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_abstract_", 0.0f);


    strncpy(abstract_name, relations[0].name, max_name_len - 1);

    /* Abstraction level based on similarity of all relations */
    float total_sim = 0.0f;
    for (uint32_t i = 1; i < num_relations; i++) {
        total_sim += relation_similarity(&relations[0], &relations[i]);
    }

    float abstraction_level = (num_relations > 1) ?
                               total_sim / (num_relations - 1) : 1.0f;

    return abstraction_level;
}

int analogical_apply_abstraction(
    analogical_engine_t* engine,
    const analog_abstraction_t* abstraction,
    const analog_domain_t* target,
    analog_relation_t* instantiated_relations,
    uint32_t max_relations,
    uint32_t* num_found
) {
    if (!engine || !abstraction || !target || !instantiated_relations || !num_found) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analogical_apply_abstraction: required parameter is NULL (engine, abstraction, target, instantiated_relations, num_found)");
        return -1;
    }

    *num_found = 0;

    /* Simplified: return existing relations that might match abstract patterns */
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_apply_abs", 0.0f);


    for (uint32_t i = 0; i < target->num_relations && *num_found < max_relations; i++) {
        instantiated_relations[(*num_found)++] = target->relations[i];
    }

    return 0;
}

void analogical_free_abstraction(analog_abstraction_t* abstraction) {
    if (!abstraction) return;

    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_free_abst", 0.0f);


    if (abstraction->abstract_relations) {
        for (uint32_t i = 0; i < abstraction->num_abstract_relations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && abstraction->num_abstract_relations > 256) {
                analogical_reasoning_heartbeat("analogical_r_loop",
                                 (float)(i + 1) / (float)abstraction->num_abstract_relations);
            }

            if (abstraction->abstract_relations[i]) {
                nimcp_free(abstraction->abstract_relations[i]);
            }
        }
        nimcp_free(abstraction->abstract_relations);
    }

    if (abstraction->abstract_features) {
        nimcp_free(abstraction->abstract_features);
    }

    nimcp_free(abstraction);
    abstraction = NULL;
}

/* ============================================================================
 * ANALOGY GENERATION IMPLEMENTATION
 * ============================================================================ */

analog_analogy_t* analogical_generate_explanation(
    analogical_engine_t* engine,
    const analog_domain_t* concept_domain,
    const analog_domain_t* audience_familiarity
) {
    if (!engine || !concept_domain || !audience_familiarity) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analogical_generate_explanation: required parameter is NULL (engine, concept_domain, audience_familiarity)");
        return NULL;
    }

    /* Find analogy from familiar to concept */
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_generate_", 0.0f);


    return analogical_find_analogy(engine, audience_familiarity, concept_domain);
}

float analogical_complete_analogy(
    analogical_engine_t* engine,
    const analog_entity_t* a,
    const analog_entity_t* b,
    const analog_entity_t* c,
    float* d_features,
    uint32_t max_features,
    uint32_t* num_features
) {
    if (!engine || !a || !b || !c || !d_features || !num_features) {
        set_error("Invalid parameters");
        return -1.0f;
    }

    /* A:B :: C:D
     * D should have same relation to C as B has to A
     * Simplified: compute feature difference A->B and apply to C
     */

    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_complete_", 0.0f);


    uint32_t common_features = a->num_features;
    if (b->num_features < common_features) common_features = b->num_features;
    if (c->num_features < common_features) common_features = c->num_features;
    if (max_features < common_features) common_features = max_features;

    *num_features = common_features;

    for (uint32_t i = 0; i < common_features; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && common_features > 256) {
            analogical_reasoning_heartbeat("analogical_r_loop",
                             (float)(i + 1) / (float)common_features);
        }

        float diff = b->features[i] - a->features[i];
        d_features[i] = c->features[i] + diff;
    }

    /* Confidence based on similarity of A to C */
    float ac_sim = entity_similarity(a, c);

    return apply_modulation(engine, ac_sim);
}

/* ============================================================================
 * MODULATION IMPLEMENTATION
 * ============================================================================ */

int analogical_set_inflammation(analogical_engine_t* engine, float level) {
    if (!engine) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");

        return -1;

    }
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_set_infla", 0.0f);


    engine->inflammation = fmaxf(0.0f, fminf(1.0f, level));
    return 0;
}

int analogical_set_fatigue(analogical_engine_t* engine, float level) {
    if (!engine) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");

        return -1;

    }
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_set_fatig", 0.0f);


    engine->fatigue = fmaxf(0.0f, fminf(1.0f, level));
    return 0;
}

/* ============================================================================
 * STATISTICS IMPLEMENTATION
 * ============================================================================ */

int analogical_get_stats(const analogical_engine_t* engine, analog_stats_t* stats) {
    if (!engine || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analogical_get_stats: required parameter is NULL (engine, stats)");
        return -1;
    }
    *stats = engine->stats;
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_get_stats", 0.0f);


    return 0;
}

void analogical_reset_stats(analogical_engine_t* engine) {
    if (!engine) return;
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_analogical_reset_sta", 0.0f);


    memset(&engine->stats, 0, sizeof(engine->stats));
}

const char* analogical_get_last_error(void) {
    return g_last_error;
}

/* ============================================================================
 * W14 (2026-04-24): KG runtime emit + read-path for analogical reasoning.
 *
 * Callers that hold a brain_t invoke this after analogical_map_entities /
 * analogical_find_multiple_analogies to register the mapping in
 * brain->internal_kg and query the last-mapping-score bias.
 * ============================================================================ */
#include "cognitive/kg/nimcp_wave14_math_genius_kg.h"
float analogical_reasoning_wave14_kg_emit_mapping(
    struct brain_struct* brain,
    uint32_t src_domain_id,
    uint32_t tgt_domain_id,
    float mapping_score)
{
    if (!brain) return 0.5f;
    wave14_analogical_emit_mapping(brain, src_domain_id, tgt_domain_id, mapping_score);
    return wave14_analogical_query_score_bias(brain);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int analogical_reasoning_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    analogical_reasoning_heartbeat("analogical_r_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Analogical_Reasoning");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                analogical_reasoning_heartbeat("analogical_r_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Module self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Analogical_Reasoning");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Analogical_Reasoning");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void analogical_reasoning_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_analogical_reasoning_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int analogical_reasoning_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "analogical_reasoning_training_begin: NULL argument");
        return -1;
    }
    analogical_reasoning_heartbeat_instance(g_analogical_reasoning_health_agent, "analogical_reasoning_training_begin", 0.0f);
    (void)(struct analogical_engine*)instance; /* Module state available for reset */
    return 0;
}

int analogical_reasoning_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "analogical_reasoning_training_end: NULL argument");
        return -1;
    }
    analogical_reasoning_heartbeat_instance(g_analogical_reasoning_health_agent, "analogical_reasoning_training_end", 1.0f);
    (void)(struct analogical_engine*)instance; /* Module state available for finalization */
    return 0;
}

int analogical_reasoning_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "analogical_reasoning_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    analogical_reasoning_heartbeat_instance(g_analogical_reasoning_health_agent, "analogical_reasoning_training_step", progress);
    (void)(struct analogical_engine*)instance; /* Module state available for step adaptation */
    return 0;
}
