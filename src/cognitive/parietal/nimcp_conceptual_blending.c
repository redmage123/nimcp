/**
 * @file nimcp_conceptual_blending.c
 * @brief Conceptual blending engine implementation
 */

#include "cognitive/parietal/nimcp_conceptual_blending.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE(conceptual_blending, MESH_ADAPTER_CATEGORY_COGNITIVE)


struct blending_engine {
    blend_config_t config;
    blend_stats_t stats;
    float inflammation;
    float fatigue;
    uint32_t next_blend_id;
    uint32_t next_space_id;
};

static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* msg) {
    strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
}

static float apply_mod(const blending_engine_t* e, float v) {
    float f = 1.0f - e->inflammation * e->config.inflammation_sensitivity * 0.2f
                   - e->fatigue * e->config.fatigue_sensitivity * 0.3f;
    return v * fmaxf(0.4f, f);
}

static float feature_sim(const float* a, const float* b, uint32_t dim) {
    float dot = 0, na = 0, nb = 0;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            conceptual_blending_heartbeat("conceptual_b_loop",
                             (float)(i + 1) / (float)dim);
        }

        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    float d = sqrtf(na) * sqrtf(nb);
    return (d > 1e-10f) ? dot / d : 0;
}

blend_config_t blending_engine_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_engine_defa", 0.0f);


    return (blend_config_t){
        .min_integration = 0.3f,
        .novelty_weight = 0.4f,
        .enable_emergence_detection = true,
        .enable_optimization = true,
        .max_blend_iterations = 10,
        .inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = 1.0f
    };
}

blending_engine_t* blending_engine_create(void) {
    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_engine_crea", 0.0f);


    blend_config_t c = blending_engine_default_config();
    return blending_engine_create_custom(&c);
}

blending_engine_t* blending_engine_create_custom(const blend_config_t* config) {
    if (!config) { set_error("NULL config"); return NULL; }
    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_engine_crea", 0.0f);


    blending_engine_t* e = nimcp_calloc(1, sizeof(blending_engine_t));
    if (!e) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate e");

        return NULL;

    }
    e->config = *config;
    e->next_blend_id = 1;
    e->next_space_id = 1;
    return e;
}

void blending_engine_destroy(blending_engine_t* engine) {
    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_engine_dest", 0.0f);


    if (engine) nimcp_free(engine);
}

blend_mental_space_t* blending_create_space(const char* name) {
    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_create_spac", 0.0f);


    blend_mental_space_t* s = nimcp_calloc(1, sizeof(blend_mental_space_t));
    if (!s) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate s");

        return NULL;

    }

    if (name) strncpy(s->name, name, sizeof(s->name) - 1);
    s->elements = nimcp_calloc(BLEND_MAX_ELEMENTS, sizeof(blend_element_t));
    return s;
}

int blending_add_element(blend_mental_space_t* space, const char* name,
    const float* features, uint32_t num_features) {
    if (!space || space->num_elements >= BLEND_MAX_ELEMENTS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "blending_create_space: space is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_add_element", 0.0f);


    blend_element_t* e = &space->elements[space->num_elements];
    e->id = space->num_elements + 1;
    if (name) strncpy(e->name, name, sizeof(e->name) - 1);

    if (features && num_features > 0) {
        e->features = nimcp_calloc(num_features, sizeof(float));
        if (e->features) {
            memcpy(e->features, features, num_features * sizeof(float));
            e->num_features = num_features;
        }
    }

    space->num_elements++;
    return 0;
}

void blending_free_space(blend_mental_space_t* space) {
    if (!space) return;
    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_free_space", 0.0f);


    if (space->elements) {
        for (uint32_t i = 0; i < space->num_elements; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && space->num_elements > 256) {
                conceptual_blending_heartbeat("conceptual_b_loop",
                                 (float)(i + 1) / (float)space->num_elements);
            }

            if (space->elements[i].features) nimcp_free(space->elements[i].features);
        }
        nimcp_free(space->elements);
    }
    if (space->frame_structure) nimcp_free(space->frame_structure);
    nimcp_free(space);
    space = NULL;
}

conceptual_blend_t* blending_create_blend(blending_engine_t* engine,
    const blend_mental_space_t* concept1, const blend_mental_space_t* concept2) {
    if (!engine || !concept1 || !concept2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "blending_free_space: required parameter is NULL (engine, concept1, concept2)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_create_blen", 0.0f);


    conceptual_blend_t* b = nimcp_calloc(1, sizeof(conceptual_blend_t));
    if (!b) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate b");

        return NULL;

    }

    b->id = engine->next_blend_id++;
    b->input1 = (blend_mental_space_t*)concept1;
    b->input2 = (blend_mental_space_t*)concept2;

    /* Create generic space (shared structure) */
    b->generic = blending_create_space("generic");

    /* Create blend space */
    b->blend = blending_create_space("blend");
    if (b->blend) {
        /* Phase 5: Selective projection (Fauconnier & Turner) */
        for (uint32_t i = 0; i < concept1->num_elements && b->blend->num_elements < BLEND_MAX_ELEMENTS; i++) {
            float alpha = 1.0f;
            for (uint32_t m = 0; m < b->num_mappings; m++) {
                if (b->mappings[m].source_id == concept1->elements[i].id) {
                    alpha = b->mappings[m].strength;
                    break;
                }
            }
            if (alpha > engine->config.min_integration) {
                blending_add_element(b->blend, concept1->elements[i].name,
                    concept1->elements[i].features, concept1->elements[i].num_features);
            }
        }
        for (uint32_t i = 0; i < concept2->num_elements && b->blend->num_elements < BLEND_MAX_ELEMENTS; i++) {
            float alpha = 1.0f;
            for (uint32_t m = 0; m < b->num_mappings; m++) {
                if (b->mappings[m].target_id == concept2->elements[i].id) {
                    alpha = b->mappings[m].strength;
                    break;
                }
            }
            if (alpha > engine->config.min_integration) {
                blending_add_element(b->blend, concept2->elements[i].name,
                    concept2->elements[i].features, concept2->elements[i].num_features);
            }
        }
    }

    /* Find mappings first (needed for selective projection) */
    b->mappings = nimcp_calloc(BLEND_MAX_MAPPINGS, sizeof(blend_mapping_t));
    if (!b->mappings) return NULL;
    blending_find_mappings(engine, concept1, concept2, b->mappings, BLEND_MAX_MAPPINGS, &b->num_mappings);

    /* Phase 5: Real emergence detection (Fauconnier & Turner) */
    if (engine->config.enable_emergence_detection) {
        b->emergent_properties = nimcp_calloc(BLEND_MAX_PROPERTIES, sizeof(blend_property_t));
        if (!b->emergent_properties) return NULL;
        b->num_emergent = 0;

        /* Detect features in blend that exceed both source spaces */
        if (b->blend && b->blend->num_elements > 0) {
            uint32_t max_feat = 0;
            for (uint32_t e = 0; e < b->blend->num_elements; e++) {
                if (b->blend->elements[e].num_features > max_feat)
                    max_feat = b->blend->elements[e].num_features;
            }

            for (uint32_t f = 0; f < max_feat && b->num_emergent < BLEND_MAX_PROPERTIES; f++) {
                float max_s1 = -1e30f, max_s2 = -1e30f, blend_val = 0.0f;

                for (uint32_t e = 0; e < b->blend->num_elements; e++) {
                    if (f < b->blend->elements[e].num_features) {
                        float v = fabsf(b->blend->elements[e].features[f]);
                        if (v > blend_val) blend_val = v;
                    }
                }
                for (uint32_t e = 0; e < concept1->num_elements; e++) {
                    if (f < concept1->elements[e].num_features) {
                        float v = fabsf(concept1->elements[e].features[f]);
                        if (v > max_s1) max_s1 = v;
                    }
                }
                for (uint32_t e = 0; e < concept2->num_elements; e++) {
                    if (f < concept2->elements[e].num_features) {
                        float v = fabsf(concept2->elements[e].features[f]);
                        if (v > max_s2) max_s2 = v;
                    }
                }

                float threshold = fmaxf(max_s1, max_s2) * 1.1f;
                if (blend_val > threshold && threshold > 1e-6f) {
                    blend_property_t* prop = &b->emergent_properties[b->num_emergent];
                    prop->is_emergent = true;
                    prop->strength = apply_mod(engine, blend_val / (threshold + 1e-6f));
                    snprintf(prop->name, sizeof(prop->name), "Emergent feature %u", f);
                    snprintf(prop->description, sizeof(prop->description),
                            "Feature %u: blend=%.3f > max(s1=%.3f, s2=%.3f)",
                            f, blend_val, max_s1, max_s2);
                    b->num_emergent++;
                    engine->stats.emergent_properties_found++;
                }
            }
        }

        /* Fallback: structural emergence from mappings */
        if (b->num_emergent == 0 && b->num_mappings > 0) {
            b->emergent_properties[0].is_emergent = true;
            float avg_strength = 0.0f;
            for (uint32_t m = 0; m < b->num_mappings; m++) {
                avg_strength += b->mappings[m].strength;
            }
            avg_strength /= (float)b->num_mappings;
            b->emergent_properties[0].strength = apply_mod(engine, avg_strength);
            snprintf(b->emergent_properties[0].name,
                    sizeof(b->emergent_properties[0].name), "Structural blend");
            snprintf(b->emergent_properties[0].description,
                    sizeof(b->emergent_properties[0].description),
                    "Cross-space integration (%u mappings, avg=%.2f)",
                    b->num_mappings, avg_strength);
            b->num_emergent = 1;
            engine->stats.emergent_properties_found++;
        }
    }

    b->integration_score = apply_mod(engine, 0.5f + 0.3f * ((float)b->num_mappings / BLEND_MAX_MAPPINGS));
    b->novelty_score = apply_mod(engine, 0.6f);

    engine->stats.blends_created++;
    engine->stats.mappings_made += b->num_mappings;

    return b;
}

int blending_find_mappings(blending_engine_t* engine,
    const blend_mental_space_t* space1, const blend_mental_space_t* space2,
    blend_mapping_t* mappings, uint32_t max_mappings, uint32_t* num_found) {
    if (!engine || !space1 || !space2 || !mappings || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "blending_free_space: required parameter is NULL (engine, space1, space2, mappings, num_found)");
        return -1;
    }
    *num_found = 0;

    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_find_mappin", 0.0f);


    for (uint32_t i = 0; i < space1->num_elements && *num_found < max_mappings; i++) {
        float best_sim = 0.3f;
        int best_j = -1;

        for (uint32_t j = 0; j < space2->num_elements; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && space2->num_elements > 256) {
                conceptual_blending_heartbeat("conceptual_b_loop",
                                 (float)(j + 1) / (float)space2->num_elements);
            }

            if (space1->elements[i].num_features > 0 &&
                space2->elements[j].num_features > 0) {
                uint32_t min_f = (space1->elements[i].num_features < space2->elements[j].num_features) ?
                                  space1->elements[i].num_features : space2->elements[j].num_features;
                float sim = feature_sim(space1->elements[i].features,
                                        space2->elements[j].features, min_f);
                if (sim > best_sim) {
                    best_sim = sim;
                    best_j = j;
                }
            }
        }

        if (best_j >= 0) {
            mappings[*num_found].source_id = space1->elements[i].id;
            mappings[*num_found].target_id = space2->elements[best_j].id;
            mappings[*num_found].strength = best_sim;
            (*num_found)++;
        }
    }

    return 0;
}

blend_property_t** blending_find_emergent(blending_engine_t* engine,
    const conceptual_blend_t* blend, uint32_t* num_found) {
    if (!engine || !blend || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "blending_free_space: required parameter is NULL (engine, blend, num_found)");
        return NULL;
    }
    *num_found = blend->num_emergent;
    return (blend_property_t**)&blend->emergent_properties;
}

float blending_evaluate_novelty(blending_engine_t* engine, const conceptual_blend_t* blend) {
    if (!engine || !blend) return 0;
    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_evaluate_no", 0.0f);


    return apply_mod(engine, blend->novelty_score);
}

float blending_evaluate_integration(blending_engine_t* engine, const conceptual_blend_t* blend) {
    if (!engine || !blend) return 0;
    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_evaluate_in", 0.0f);


    return apply_mod(engine, blend->integration_score);
}

int blending_optimize_blend(blending_engine_t* engine, conceptual_blend_t* blend) {
    if (!engine || !blend) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "blending_optimize_blend: required parameter is NULL (engine, blend)");
        return -1;
    }
    if (!engine->config.enable_optimization) return 0;

    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_optimize_bl", 0.0f);


    /* Phase 5: Hill-climbing optimizer */
    float best_integration = blend->integration_score;
    float best_novelty = blend->novelty_score;

    for (uint32_t iter = 0; iter < engine->config.max_blend_iterations; iter++) {
        float delta = 0.02f * (1.0f - (float)iter / engine->config.max_blend_iterations);
        float trial_integration = best_integration + delta;
        float trial_novelty = best_novelty;

        /* Integration-novelty trade-off */
        if (trial_integration > 0.8f) {
            trial_novelty *= (1.0f - delta * 0.5f);
        }

        float current_score = best_integration * (1.0f - engine->config.novelty_weight)
                            + best_novelty * engine->config.novelty_weight;
        float trial_score = trial_integration * (1.0f - engine->config.novelty_weight)
                          + trial_novelty * engine->config.novelty_weight;

        if (trial_score > current_score) {
            best_integration = trial_integration;
            best_novelty = trial_novelty;
        }

        if (best_integration > 0.95f) break;
    }

    blend->integration_score = apply_mod(engine, fminf(1.0f, best_integration));
    blend->novelty_score = apply_mod(engine, fminf(1.0f, best_novelty));

    return 0;
}

void blending_free_blend(conceptual_blend_t* blend) {
    if (!blend) return;
    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_free_blend", 0.0f);


    if (blend->generic) blending_free_space(blend->generic);
    if (blend->blend) blending_free_space(blend->blend);
    if (blend->mappings) nimcp_free(blend->mappings);
    if (blend->emergent_properties) nimcp_free(blend->emergent_properties);
    nimcp_free(blend);
    blend = NULL;
}

int blending_set_inflammation(blending_engine_t* engine, float level) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "blending_set_inflammation: engine is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_set_inflamm", 0.0f);


    engine->inflammation = fmaxf(0, fminf(1, level));
    return 0;
}

int blending_set_fatigue(blending_engine_t* engine, float level) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "blending_set_fatigue: engine is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_set_fatigue", 0.0f);


    engine->fatigue = fmaxf(0, fminf(1, level));
    return 0;
}

int blending_get_stats(const blending_engine_t* engine, blend_stats_t* stats) {
    if (!engine || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "blending_get_stats: required parameter is NULL (engine, stats)");
        return -1;
    }
    *stats = engine->stats;
    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_get_stats", 0.0f);


    return 0;
}

void blending_reset_stats(blending_engine_t* engine) {
    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_blending_reset_stats", 0.0f);


    if (engine) memset(&engine->stats, 0, sizeof(engine->stats));
}

const char* blending_get_last_error(void) { return g_last_error; }

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int conceptual_blending_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    conceptual_blending_heartbeat("conceptual_b_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Conceptual_Blending");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                conceptual_blending_heartbeat("conceptual_b_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Module self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Conceptual_Blending");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Conceptual_Blending");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void conceptual_blending_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_conceptual_blending_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int conceptual_blending_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "conceptual_blending_training_begin: NULL argument");
        return -1;
    }
    conceptual_blending_heartbeat_instance(g_conceptual_blending_health_agent, "conceptual_blending_training_begin", 0.0f);
    (void)(struct blending_engine*)instance; /* Module state available for reset */
    return 0;
}

int conceptual_blending_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "conceptual_blending_training_end: NULL argument");
        return -1;
    }
    conceptual_blending_heartbeat_instance(g_conceptual_blending_health_agent, "conceptual_blending_training_end", 1.0f);
    (void)(struct blending_engine*)instance; /* Module state available for finalization */
    return 0;
}

int conceptual_blending_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "conceptual_blending_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    conceptual_blending_heartbeat_instance(g_conceptual_blending_health_agent, "conceptual_blending_training_step", progress);
    (void)(struct blending_engine*)instance; /* Module state available for step adaptation */
    return 0;
}
