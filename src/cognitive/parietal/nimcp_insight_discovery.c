/**
 * @file nimcp_insight_discovery.c
 * @brief Insight and discovery engine implementation
 */

#include "cognitive/parietal/nimcp_insight_discovery.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE(insight_discovery, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * INTERNAL TYPES
 * ============================================================================ */

typedef struct {
    uint32_t id;
    insight_problem_t* problem;
    float progress;
    uint64_t start_time;
    uint32_t iterations;
    bool ready;
    insight_eureka_t* result;
} incubation_entry_t;

struct insight_engine {
    insight_config_t config;
    insight_stats_t stats;

    incubation_entry_t* incubation_queue;
    uint32_t num_incubating;
    uint32_t next_problem_id;
    uint32_t next_eureka_id;

    float inflammation;
    float fatigue;
};

static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

static void set_error(const char* msg) {
    strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
}

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static float apply_modulation(const insight_engine_t* e, float v) {
    float f = 1.0f - e->inflammation * e->config.inflammation_sensitivity * 0.3f
                   - e->fatigue * e->config.fatigue_sensitivity * 0.4f;
    return v * fmaxf(0.3f, f);
}

/* ============================================================================
 * LIFECYCLE
 * ============================================================================ */

insight_config_t insight_engine_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_engine_defau", 0.0f);


    return (insight_config_t){
        .impasse_threshold = 0.7f,
        .incubation_rate = 0.05f,
        .constraint_relaxation_rate = 0.1f,
        .restructuring_threshold = 0.4f,
        .enable_incubation = true,
        .enable_constraint_relaxation = true,
        .enable_perspective_shifting = true,
        .max_restructuring_attempts = 10,
        .incubation_queue_size = INSIGHT_MAX_INCUBATION_QUEUE,
        .inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = 1.0f
    };
}

insight_engine_t* insight_engine_create(void) {
    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_engine_creat", 0.0f);


    insight_config_t config = insight_engine_default_config();
    return insight_engine_create_custom(&config);
}

insight_engine_t* insight_engine_create_custom(const insight_config_t* config) {
    if (!config) { set_error("NULL config"); return NULL; }

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_engine_creat", 0.0f);


    insight_engine_t* engine = nimcp_calloc(1, sizeof(insight_engine_t));
    if (!engine) { set_error("Alloc failed"); return NULL; }

    engine->config = *config;

    engine->incubation_queue = nimcp_calloc(config->incubation_queue_size,
                                            sizeof(incubation_entry_t));
    if (!engine->incubation_queue) {
        nimcp_free(engine);
        engine = NULL;
        set_error("Queue alloc failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "insight_engine_create_custom: engine->incubation_queue is NULL");
        return NULL;
    }

    engine->next_problem_id = 1;
    engine->next_eureka_id = 1;

    return engine;
}

void insight_engine_destroy(insight_engine_t* engine) {
    if (!engine) return;

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_engine_destr", 0.0f);


    if (engine->incubation_queue) {
        for (uint32_t i = 0; i < engine->num_incubating; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && engine->num_incubating > 256) {
                insight_discovery_heartbeat("insight_disc_loop",
                                 (float)(i + 1) / (float)engine->num_incubating);
            }

            if (engine->incubation_queue[i].result) {
                insight_free_eureka(engine->incubation_queue[i].result);
            }
        }
        nimcp_free(engine->incubation_queue);
    }
    nimcp_free(engine);
    engine = NULL;
}

/* ============================================================================
 * PROBLEM MANAGEMENT
 * ============================================================================ */

insight_problem_t* insight_create_problem(const char* description) {
    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_create_probl", 0.0f);


    insight_problem_t* problem = nimcp_calloc(1, sizeof(insight_problem_t));
    if (!problem) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate problem");

        return NULL;

    }

    if (description) {
        strncpy(problem->description, description, sizeof(problem->description) - 1);
    }

    problem->constraints = nimcp_calloc(INSIGHT_MAX_CONSTRAINTS, sizeof(insight_constraint_t));
    problem->perspectives = nimcp_calloc(INSIGHT_MAX_PERSPECTIVES, sizeof(insight_perspective_t));

    return problem;
}

int insight_add_constraint(insight_problem_t* problem, const char* description,
                          float binding_strength, bool is_explicit) {
    if (!problem || problem->num_constraints >= INSIGHT_MAX_CONSTRAINTS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "insight_create_problem: problem is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_add_constrai", 0.0f);


    insight_constraint_t* c = &problem->constraints[problem->num_constraints];
    c->id = problem->num_constraints + 1;
    if (description) strncpy(c->description, description, sizeof(c->description) - 1);
    c->binding_strength = fmaxf(0, fminf(1, binding_strength));
    c->is_explicit = is_explicit;
    c->is_relaxable = !is_explicit;
    c->relaxation_cost = c->binding_strength * 0.5f;

    problem->num_constraints++;
    return 0;
}

int insight_add_perspective(insight_problem_t* problem, const char* description,
                           const float* representation, uint32_t dim) {
    if (!problem || problem->num_perspectives >= INSIGHT_MAX_PERSPECTIVES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "insight_create_problem: problem is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_add_perspect", 0.0f);


    insight_perspective_t* p = &problem->perspectives[problem->num_perspectives];
    p->id = problem->num_perspectives + 1;
    if (description) strncpy(p->description, description, sizeof(p->description) - 1);

    if (representation && dim > 0) {
        p->representation = nimcp_calloc(dim, sizeof(float));
        if (p->representation) {
            memcpy(p->representation, representation, dim * sizeof(float));
            p->repr_dim = dim;
        }
    }

    p->novelty = (problem->num_perspectives == 0) ? 0.0f : 0.5f;
    p->utility = 0.5f;

    problem->num_perspectives++;
    return 0;
}

void insight_free_problem(insight_problem_t* problem) {
    if (!problem) return;

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_free_problem", 0.0f);


    if (problem->state) nimcp_free(problem->state);
    if (problem->goal) nimcp_free(problem->goal);
    if (problem->constraints) nimcp_free(problem->constraints);

    if (problem->perspectives) {
        for (uint32_t i = 0; i < problem->num_perspectives; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && problem->num_perspectives > 256) {
                insight_discovery_heartbeat("insight_disc_loop",
                                 (float)(i + 1) / (float)problem->num_perspectives);
            }

            if (problem->perspectives[i].representation) {
                nimcp_free(problem->perspectives[i].representation);
            }
        }
        nimcp_free(problem->perspectives);
    }

    nimcp_free(problem);
    problem = NULL;
}

/* ============================================================================
 * INCUBATION
 * ============================================================================ */

uint32_t insight_incubate(insight_engine_t* engine, insight_problem_t* problem) {
    if (!engine || !problem) return 0;
    if (!engine->config.enable_incubation) return 0;
    if (engine->num_incubating >= engine->config.incubation_queue_size) return 0;

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_incubate", 0.0f);


    incubation_entry_t* entry = &engine->incubation_queue[engine->num_incubating];
    entry->id = engine->next_problem_id++;
    entry->problem = problem;
    entry->progress = 0.0f;
    entry->start_time = get_timestamp_us();
    entry->iterations = 0;
    entry->ready = false;
    entry->result = NULL;

    engine->num_incubating++;
    return entry->id;
}

int insight_check_incubation(insight_engine_t* engine, uint32_t problem_id,
                            insight_eureka_t** result) {
    if (!engine || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_incubate: required parameter is NULL (engine, result)");
        return -1;
    }
    *result = NULL;

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_check_incuba", 0.0f);


    for (uint32_t i = 0; i < engine->num_incubating; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && engine->num_incubating > 256) {
            insight_discovery_heartbeat("insight_disc_loop",
                             (float)(i + 1) / (float)engine->num_incubating);
        }

        if (engine->incubation_queue[i].id == problem_id) {
            if (engine->incubation_queue[i].ready) {
                *result = engine->incubation_queue[i].result;
                engine->incubation_queue[i].result = NULL;
                return 1;
            }
            return 0;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "insight_incubate: validation failed");
    return -1;
}

int insight_process_incubation_step(insight_engine_t* engine) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_process_incubation_step: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_process_incu", 0.0f);


    int insights = 0;
    float rate = engine->config.incubation_rate * (1.0f - engine->fatigue * 0.5f);

    for (uint32_t i = 0; i < engine->num_incubating; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && engine->num_incubating > 256) {
            insight_discovery_heartbeat("insight_disc_loop",
                             (float)(i + 1) / (float)engine->num_incubating);
        }

        incubation_entry_t* e = &engine->incubation_queue[i];
        if (e->ready) continue;

        e->iterations++;
        e->progress += rate * (1.0f - e->problem->impasse_level * 0.3f);

        if (e->progress >= 1.0f) {
            e->ready = true;

            /* Generate eureka */
            e->result = nimcp_calloc(1, sizeof(insight_eureka_t));
            if (e->result) {
                e->result->id = engine->next_eureka_id++;
                e->result->problem = e->problem;
                e->result->surprise_magnitude = 0.6f + 0.4f * ((float)nimcp_tl_rand() / RAND_MAX);
                e->result->elegance = 0.5f + 0.5f * e->progress;
                e->result->confidence = apply_modulation(engine, 0.7f);
                e->result->incubation_time_us = get_timestamp_us() - e->start_time;
                e->result->restructuring_attempts = e->iterations;
                snprintf(e->result->description, sizeof(e->result->description),
                        "Insight after %u iterations", e->iterations);

                engine->stats.insights_generated++;
                insights++;
            }
        }
    }

    return insights;
}

int insight_cancel_incubation(insight_engine_t* engine, uint32_t problem_id) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_cancel_incubation: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_cancel_incub", 0.0f);


    for (uint32_t i = 0; i < engine->num_incubating; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && engine->num_incubating > 256) {
            insight_discovery_heartbeat("insight_disc_loop",
                             (float)(i + 1) / (float)engine->num_incubating);
        }

        if (engine->incubation_queue[i].id == problem_id) {
            if (engine->incubation_queue[i].result) {
                insight_free_eureka(engine->incubation_queue[i].result);
            }
            /* Shift remaining */
            for (uint32_t j = i; j < engine->num_incubating - 1; j++) {
                engine->incubation_queue[j] = engine->incubation_queue[j + 1];
            }
            engine->num_incubating--;
            return 0;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "insight_cancel_incubation: operation failed");
    return -1;
}

/* ============================================================================
 * CONSTRAINT RELAXATION
 * ============================================================================ */

int insight_identify_blocking_constraints(insight_engine_t* engine,
                                         const insight_problem_t* problem,
                                         insight_constraint_t* blocking,
                                         uint32_t max_constraints,
                                         uint32_t* num_found) {
    if (!engine || !problem || !blocking || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_cancel_incubation: required parameter is NULL (engine, problem, blocking, num_found)");
        return -1;
    }
    *num_found = 0;

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_identify_blo", 0.0f);


    for (uint32_t i = 0; i < problem->num_constraints && *num_found < max_constraints; i++) {
        if (problem->constraints[i].binding_strength > 0.5f) {
            blocking[(*num_found)++] = problem->constraints[i];
        }
    }

    engine->stats.impasses_detected++;
    return 0;
}

int insight_relax_constraint(insight_engine_t* engine,
                            insight_problem_t* problem,
                            uint32_t constraint_id) {
    if (!engine || !problem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_cancel_incubation: required parameter is NULL (engine, problem)");
        return -1;
    }
    if (!engine->config.enable_constraint_relaxation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_cancel_incubation: engine->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_relax_constr", 0.0f);


    for (uint32_t i = 0; i < problem->num_constraints; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && problem->num_constraints > 256) {
            insight_discovery_heartbeat("insight_disc_loop",
                             (float)(i + 1) / (float)problem->num_constraints);
        }

        if (problem->constraints[i].id == constraint_id) {
            if (!problem->constraints[i].is_relaxable) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_cancel_incubation: problem->constraints is NULL");
                return -1;
            }

            problem->constraints[i].binding_strength *= 0.5f;
            problem->impasse_level *= 0.8f;

            engine->stats.constraints_relaxed++;
            return 0;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_cancel_incubation: problem->constraints is NULL");
    return -1;
}

int insight_find_relaxable_constraints(insight_engine_t* engine,
                                       const insight_problem_t* problem,
                                       uint32_t* constraint_ids,
                                       uint32_t max_constraints,
                                       uint32_t* num_found) {
    if (!engine || !problem || !constraint_ids || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_cancel_incubation: required parameter is NULL (engine, problem, constraint_ids, num_found)");
        return -1;
    }
    *num_found = 0;

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_find_relaxab", 0.0f);


    for (uint32_t i = 0; i < problem->num_constraints && *num_found < max_constraints; i++) {
        if (problem->constraints[i].is_relaxable) {
            constraint_ids[(*num_found)++] = problem->constraints[i].id;
        }
    }
    return 0;
}

/* ============================================================================
 * RESTRUCTURING
 * ============================================================================ */

insight_restructuring_t* insight_attempt_restructure(insight_engine_t* engine,
                                                    insight_problem_t* problem) {
    if (!engine || !problem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_cancel_incubation: required parameter is NULL (engine, problem)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_attempt_rest", 0.0f);


    insight_restructuring_t* r = nimcp_calloc(1, sizeof(insight_restructuring_t));
    if (!r) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate r");

        return NULL;

    }

    /* Find and relax a constraint */
    for (uint32_t i = 0; i < problem->num_constraints; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && problem->num_constraints > 256) {
            insight_discovery_heartbeat("insight_disc_loop",
                             (float)(i + 1) / (float)problem->num_constraints);
        }

        if (problem->constraints[i].is_relaxable &&
            problem->constraints[i].binding_strength > 0.3f) {
            r->constraint_relaxed = problem->constraints[i].id;
            problem->constraints[i].binding_strength *= 0.5f;
            break;
        }
    }

    /* Shift perspective if available */
    if (problem->num_perspectives > 1) {
        r->new_perspective = (problem->current_perspective + 1) % problem->num_perspectives;
        problem->current_perspective = r->new_perspective;
        engine->stats.perspectives_shifted++;
    }

    r->novelty = 0.5f + 0.5f * ((float)nimcp_tl_rand() / RAND_MAX);
    snprintf(r->description, sizeof(r->description), "Restructured problem");

    return r;
}

int insight_shift_perspective(insight_engine_t* engine,
                             insight_problem_t* problem,
                             uint32_t new_perspective) {
    if (!engine || !problem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_cancel_incubation: required parameter is NULL (engine, problem)");
        return -1;
    }
    if (!engine->config.enable_perspective_shifting) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_cancel_incubation: engine->config is NULL");
        return -1;
    }
    if (new_perspective >= problem->num_perspectives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "insight_cancel_incubation: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_shift_perspe", 0.0f);


    problem->current_perspective = new_perspective;
    problem->impasse_level *= 0.7f;

    engine->stats.perspectives_shifted++;
    return 0;
}

int insight_generate_perspectives(insight_engine_t* engine,
                                 insight_problem_t* problem,
                                 uint32_t max_perspectives) {
    if (!engine || !problem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_cancel_incubation: required parameter is NULL (engine, problem)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_generate_per", 0.0f);


    uint32_t generated = 0;
    while (problem->num_perspectives < INSIGHT_MAX_PERSPECTIVES &&
           generated < max_perspectives) {
        char desc[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(desc, sizeof(desc), "Generated perspective %u",
                problem->num_perspectives + 1);
        insight_add_perspective(problem, desc, NULL, 0);
        generated++;
    }

    return (int)generated;
}

void insight_free_restructuring(insight_restructuring_t* r) {
    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_free_restruc", 0.0f);


    if (r) nimcp_free(r);
}

/* ============================================================================
 * EUREKA DETECTION
 * ============================================================================ */

bool insight_check_eureka(insight_engine_t* engine,
                         const insight_problem_t* problem,
                         insight_eureka_t** result) {
    if (!engine || !problem || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_free_restructuring: required parameter is NULL (engine, problem, result)");
        return false;
    }
    *result = NULL;

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_check_eureka", 0.0f);


    if (problem->is_solved) {
        *result = nimcp_calloc(1, sizeof(insight_eureka_t));
        if (*result) {
            (*result)->id = engine->next_eureka_id++;
            (*result)->surprise_magnitude = 0.8f;
            (*result)->elegance = 0.7f;
            (*result)->confidence = apply_modulation(engine, 0.9f);
            engine->stats.insights_generated++;
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "insight_free_restructuring: operation failed");
    return false;
}

float insight_estimate_surprise(insight_engine_t* engine,
                               const insight_eureka_t* eureka) {
    if (!engine || !eureka) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_estimate_sur", 0.0f);


    return apply_modulation(engine, eureka->surprise_magnitude);
}

int insight_verify_eureka(insight_engine_t* engine, insight_eureka_t* eureka) {
    if (!engine || !eureka) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_verify_eureka: required parameter is NULL (engine, eureka)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_verify_eurek", 0.0f);


    eureka->verified = true;
    eureka->confidence = fminf(1.0f, eureka->confidence * 1.2f);
    return 0;
}

void insight_free_eureka(insight_eureka_t* eureka) {
    if (!eureka) return;
    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_free_eureka", 0.0f);


    if (eureka->solution) nimcp_free(eureka->solution);
    nimcp_free(eureka);
    eureka = NULL;
}

/* ============================================================================
 * IMPASSE
 * ============================================================================ */

int insight_detect_impasse(insight_engine_t* engine,
                          const insight_problem_t* problem,
                          insight_impasse_t* impasse) {
    if (!engine || !problem || !impasse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_free_eureka: required parameter is NULL (engine, problem, impasse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_detect_impas", 0.0f);


    memset(impasse, 0, sizeof(insight_impasse_t));
    impasse->stuckness_level = problem->impasse_level;
    impasse->ready_for_incubation = (impasse->stuckness_level >= engine->config.impasse_threshold);

    return 0;
}

int insight_resolve_impasse(insight_engine_t* engine,
                           insight_problem_t* problem,
                           const insight_impasse_t* impasse) {
    if (!engine || !problem || !impasse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_free_eureka: required parameter is NULL (engine, problem, impasse)");
        return -1;
    }

    /* Try relaxing constraints */
    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_resolve_impa", 0.0f);


    for (uint32_t i = 0; i < problem->num_constraints; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && problem->num_constraints > 256) {
            insight_discovery_heartbeat("insight_disc_loop",
                             (float)(i + 1) / (float)problem->num_constraints);
        }

        if (problem->constraints[i].is_relaxable) {
            insight_relax_constraint(engine, problem, problem->constraints[i].id);
            break;
        }
    }

    /* Shift perspective */
    if (problem->num_perspectives > 1) {
        insight_shift_perspective(engine, problem,
            (problem->current_perspective + 1) % problem->num_perspectives);
    }

    problem->impasse_level *= 0.5f;
    engine->stats.impasses_resolved++;

    return 0;
}

/* ============================================================================
 * MODULATION
 * ============================================================================ */

int insight_set_inflammation(insight_engine_t* engine, float level) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_set_inflammation: engine is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_set_inflamma", 0.0f);


    engine->inflammation = fmaxf(0, fminf(1, level));
    return 0;
}

int insight_set_fatigue(insight_engine_t* engine, float level) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_set_fatigue: engine is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_set_fatigue", 0.0f);


    engine->fatigue = fmaxf(0, fminf(1, level));
    return 0;
}

/* ============================================================================
 * STATISTICS
 * ============================================================================ */

int insight_get_stats(const insight_engine_t* engine, insight_stats_t* stats) {
    if (!engine || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insight_get_stats: required parameter is NULL (engine, stats)");
        return -1;
    }
    *stats = engine->stats;
    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_get_stats", 0.0f);


    return 0;
}

void insight_reset_stats(insight_engine_t* engine) {
    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_insight_reset_stats", 0.0f);


    if (engine) memset(&engine->stats, 0, sizeof(engine->stats));
}

const char* insight_get_last_error(void) {
    return g_last_error;
}

/* ============================================================================
 * W14 (2026-04-24): KG runtime emit + read-path for insight discovery.
 *
 * Callers that hold a brain_t (e.g. the parietal orchestrator) invoke this
 * after insight_check_eureka / insight_resolve_impasse to register the
 * discovery in brain->internal_kg and query the last-surprise bias.
 * ============================================================================ */
#include "cognitive/kg/nimcp_wave14_math_genius_kg.h"
float insight_discovery_wave14_kg_emit_eureka(
    struct brain_struct* brain,
    uint32_t problem_id,
    float surprise,
    float elegance)
{
    if (!brain) return 0.5f;
    wave14_insight_emit_eureka(brain, problem_id, surprise, elegance);
    return wave14_insight_query_surprise_bias(brain);
}

/* ============================================================================
 * KG SELF-AWARENESS INTEGRATION
 * ============================================================================ */

int insight_discovery_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    insight_discovery_heartbeat("insight_disc_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Insight_Discovery_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                insight_discovery_heartbeat("insight_disc_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Insight discovery self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Insight_Discovery_Module");
    if (connections) {
        LOG_DEBUG("Insight discovery has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Insight_Discovery_Module");
    if (incoming) {
        LOG_DEBUG("Insight discovery has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void insight_discovery_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_insight_discovery_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Functions
 * ============================================================================ */

int insight_discovery_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "insight_discovery_training_begin: NULL argument");
        return -1;
    }
    insight_discovery_heartbeat_instance(g_insight_discovery_health_agent, "insight_discovery_training_begin", 0.0f);
    insight_engine_t* eng = (insight_engine_t*)instance;
    eng->stats.problems_processed = 0;
    eng->stats.insights_generated = 0;
    eng->stats.impasses_detected = 0;
    eng->stats.impasses_resolved = 0;
    eng->stats.constraints_relaxed = 0;
    eng->stats.perspectives_shifted = 0;
    eng->stats.avg_incubation_time_us = 0.0f;
    eng->stats.avg_surprise_magnitude = 0.0f;
    eng->stats.insight_success_rate = 0.0f;
    eng->num_incubating = 0;
    NIMCP_LOGGING_INFO("Insight discovery training begin: counters reset");
    return 0;
}

int insight_discovery_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "insight_discovery_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    insight_discovery_heartbeat_instance(g_insight_discovery_health_agent, "insight_discovery_training_step", progress);
    insight_engine_t* eng = (insight_engine_t*)instance;
    eng->stats.problems_processed++;
    /* Progressive incubation rate adaptation */
    float decay = 1.0f - 0.25f * progress;
    if (decay < 0.5f) decay = 0.5f;
    eng->config.incubation_rate *= decay;
    /* Sharpen restructuring threshold as training progresses */
    eng->config.restructuring_threshold += 0.01f * progress;
    if (eng->config.restructuring_threshold > 0.9f)
        eng->config.restructuring_threshold = 0.9f;
    /* Reduce impasse threshold to catch more insight opportunities */
    eng->config.impasse_threshold -= 0.005f * progress;
    if (eng->config.impasse_threshold < 0.3f)
        eng->config.impasse_threshold = 0.3f;
    return 0;
}

int insight_discovery_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "insight_discovery_training_end: NULL argument");
        return -1;
    }
    insight_discovery_heartbeat_instance(g_insight_discovery_health_agent, "insight_discovery_training_end", 1.0f);
    insight_engine_t* eng = (insight_engine_t*)instance;
    float success_rate = 0.0f;
    if (eng->stats.problems_processed > 0) {
        success_rate = (float)eng->stats.insights_generated /
                       (float)eng->stats.problems_processed;
    }
    eng->stats.insight_success_rate = success_rate;
    NIMCP_LOGGING_INFO("Insight discovery training end: %lu problems, %lu insights, success_rate=%.4f",
                       (unsigned long)eng->stats.problems_processed,
                       (unsigned long)eng->stats.insights_generated,
                       success_rate);
    return 0;
}
