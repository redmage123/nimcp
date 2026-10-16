/**
 * @file nimcp_immunology.c
 * @brief Immunology simulation engine -- innate/adaptive immunity, complement,
 *        antibody kinetics, cytokine dynamics, vaccine response
 *
 * WHAT: Simulates immune responses with real immunology equations.
 * WHY:  Immunological prior for world model reasoning about infection and defense.
 * HOW:  Complement cascade (3 pathways), antibody-antigen binding (Ka),
 *       clonal expansion (exponential), cytokine production/decay, memory cells.
 */

#include "cognitive/physics/nimcp_immunology.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "IMMUNOLOGY"

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ============================================================================
 * Create / Destroy / Config
 * ============================================================================ */

immunology_config_t immunology_default_config(void) {
    immunology_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dt = 1.0f;                  /* 1 hour time step */
    cfg.body_temperature_c = 37.0f;
    cfg.enable_complement = true;
    cfg.enable_adaptive = true;
    cfg.enable_inflammation = true;
    cfg.enable_memory_cells = true;
    cfg.immune_competence = 1.0f;
    return cfg;
}

immunology_sim_t* immunology_create(const immunology_config_t* config) {
    immunology_sim_t* sim = (immunology_sim_t*)nimcp_calloc(1, sizeof(immunology_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate immunology sim");
        return NULL;
    }
    sim->config = config ? *config : immunology_default_config();

    /* Initialize cytokine baselines */
    sim->cytokines[IMMUNO_CYTOKINE_IL1].type = IMMUNO_CYTOKINE_IL1;
    sim->cytokines[IMMUNO_CYTOKINE_IL1].concentration = IMMUNO_IL1_BASELINE;
    sim->cytokines[IMMUNO_CYTOKINE_IL1].baseline = IMMUNO_IL1_BASELINE;
    sim->cytokines[IMMUNO_CYTOKINE_IL1].decay_rate = 0.1f;

    sim->cytokines[IMMUNO_CYTOKINE_IL2].type = IMMUNO_CYTOKINE_IL2;
    sim->cytokines[IMMUNO_CYTOKINE_IL2].concentration = IMMUNO_IL2_BASELINE;
    sim->cytokines[IMMUNO_CYTOKINE_IL2].baseline = IMMUNO_IL2_BASELINE;
    sim->cytokines[IMMUNO_CYTOKINE_IL2].decay_rate = 0.15f;

    sim->cytokines[IMMUNO_CYTOKINE_IL6].type = IMMUNO_CYTOKINE_IL6;
    sim->cytokines[IMMUNO_CYTOKINE_IL6].concentration = IMMUNO_IL6_BASELINE;
    sim->cytokines[IMMUNO_CYTOKINE_IL6].baseline = IMMUNO_IL6_BASELINE;
    sim->cytokines[IMMUNO_CYTOKINE_IL6].decay_rate = 0.08f;

    sim->cytokines[IMMUNO_CYTOKINE_IL10].type = IMMUNO_CYTOKINE_IL10;
    sim->cytokines[IMMUNO_CYTOKINE_IL10].concentration = IMMUNO_IL10_BASELINE;
    sim->cytokines[IMMUNO_CYTOKINE_IL10].baseline = IMMUNO_IL10_BASELINE;
    sim->cytokines[IMMUNO_CYTOKINE_IL10].decay_rate = 0.12f;

    sim->cytokines[IMMUNO_CYTOKINE_TNF_ALPHA].type = IMMUNO_CYTOKINE_TNF_ALPHA;
    sim->cytokines[IMMUNO_CYTOKINE_TNF_ALPHA].concentration = IMMUNO_TNF_ALPHA_BASELINE;
    sim->cytokines[IMMUNO_CYTOKINE_TNF_ALPHA].baseline = IMMUNO_TNF_ALPHA_BASELINE;
    sim->cytokines[IMMUNO_CYTOKINE_TNF_ALPHA].decay_rate = 0.2f;

    sim->cytokines[IMMUNO_CYTOKINE_IFN_GAMMA].type = IMMUNO_CYTOKINE_IFN_GAMMA;
    sim->cytokines[IMMUNO_CYTOKINE_IFN_GAMMA].concentration = IMMUNO_IFNG_BASELINE;
    sim->cytokines[IMMUNO_CYTOKINE_IFN_GAMMA].baseline = IMMUNO_IFNG_BASELINE;
    sim->cytokines[IMMUNO_CYTOKINE_IFN_GAMMA].decay_rate = 0.18f;

    sim->complement.c3_level = 1.0f;
    sim->primary_response = true;
    sim->initialized = true;

    LOG_INFO(LOG_TAG, "Immunology sim created (dt=%.1fh)", sim->config.dt);
    return sim;
}

void immunology_destroy(immunology_sim_t* sim) {
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Immunology sim destroyed after %lu steps", sim->stats.step_count);
    nimcp_free(sim);
}

immunology_stats_t immunology_get_stats(const immunology_sim_t* sim) {
    immunology_stats_t empty;
    memset(&empty, 0, sizeof(empty));
    return sim ? sim->stats : empty;
}

/* ============================================================================
 * Pathogen / Cell Management
 * ============================================================================ */

int immunology_add_pathogen(immunology_sim_t* sim, const immuno_pathogen_t* p) {
    if (!sim || !p) return -1;
    if (sim->num_pathogens >= IMMUNO_MAX_PATHOGENS) return -1;
    sim->pathogens[sim->num_pathogens] = *p;
    sim->pathogens[sim->num_pathogens].id = sim->num_pathogens;
    sim->num_pathogens++;
    return 0;
}

int immunology_infect(immunology_sim_t* sim, uint32_t pathogen_idx, float load) {
    if (!sim || pathogen_idx >= sim->num_pathogens) return -1;
    sim->pathogens[pathogen_idx].load = load;
    sim->pathogens[pathogen_idx].active = true;
    sim->time_since_infection_h = 0.0f;
    LOG_INFO(LOG_TAG, "Infection: %s, load=%.1f (log10)",
             sim->pathogens[pathogen_idx].name, load);
    return 0;
}

int immunology_add_immune_cell(immunology_sim_t* sim, const immuno_cell_t* cell) {
    if (!sim || !cell) return -1;
    if (sim->num_cells >= IMMUNO_MAX_IMMUNE_CELLS) return -1;
    sim->cells[sim->num_cells] = *cell;
    sim->cells[sim->num_cells].id = sim->num_cells;
    sim->num_cells++;
    return 0;
}

/* ============================================================================
 * Antibody-Antigen Binding
 * ============================================================================ */

/**
 * Equilibrium antibody-antigen binding.
 * Ka = [Ab*Ag] / ([Ab] * [Ag])
 * => [Ab*Ag] = Ka * [Ab] * [Ag] / (1 + Ka * [Ag])  (with saturation)
 * Returns fraction of antigen bound [0..1].
 */
float immunology_antibody_binding(float ka, float ab_conc, float ag_conc) {
    if (ab_conc <= 0.0f || ag_conc <= 0.0f) return 0.0f;
    float bound = ka * ab_conc * ag_conc / (1.0f + ka * ag_conc);
    return clampf(bound / (ag_conc + 1e-10f), 0.0f, 1.0f);
}

int immunology_class_switch(immunology_sim_t* sim, uint32_t ab_idx,
                            immuno_antibody_class_t new_class) {
    if (!sim || ab_idx >= sim->num_antibodies) return -1;
    immuno_antibody_t* ab = &sim->antibodies[ab_idx];
    ab->ab_class = new_class;
    /* Affinity maturation: class-switched antibodies have higher affinity */
    ab->affinity_ka *= 10.0f;
    if (ab->affinity_ka > IMMUNO_KA_VERY_HIGH) ab->affinity_ka = IMMUNO_KA_VERY_HIGH;
    return 0;
}

/* ============================================================================
 * Complement Cascade
 * ============================================================================ */

int immunology_activate_complement(immunology_sim_t* sim,
                                   immuno_complement_pathway_t pathway) {
    if (!sim) return -1;
    sim->complement.activated = true;
    sim->complement.active_pathway = pathway;
    LOG_DEBUG(LOG_TAG, "Complement activated via pathway %d", pathway);
    return 0;
}

int immunology_step_complement(immunology_sim_t* sim, float dt) {
    if (!sim) return -1;
    immuno_complement_state_t* comp = &sim->complement;
    if (!comp->activated) return 0;

    /* C3 cleavage -> C3a (anaphylatoxin) + C3b (opsonin) */
    float cleavage_rate = 0.1f * dt;
    if (comp->c3_level > 0.0f) {
        float cleaved = comp->c3_level * cleavage_rate;
        comp->c3_level -= cleaved;
        comp->c3a_level += cleaved * 0.5f;
        comp->c3b_level += cleaved * 0.5f;
    }

    /* C5 convertase -> C5a (chemotaxis) + MAC assembly */
    if (comp->c3b_level > 0.3f) {
        comp->c5a_level += comp->c3b_level * 0.05f * dt;
        /* MAC (C5b-9) assembly */
        comp->mac_level += comp->c3b_level * 0.03f * dt;
        comp->mac_level = clampf(comp->mac_level, 0.0f, 1.0f);
    }

    /* Decay of complement fragments */
    float halflife_factor = expf(-logf(2.0f) * dt / IMMUNO_COMPLEMENT_HALF_LIFE_H);
    comp->c3a_level *= halflife_factor;
    comp->c5a_level *= halflife_factor;

    return 0;
}

/* ============================================================================
 * Cytokine Dynamics
 * ============================================================================ */

int immunology_step_cytokines(immunology_sim_t* sim, float dt) {
    if (!sim) return -1;

    /* Total pathogen load drives pro-inflammatory cytokine production */
    float total_load = 0.0f;
    for (uint32_t i = 0; i < sim->num_pathogens; i++) {
        if (sim->pathogens[i].active) {
            total_load += sim->pathogens[i].load;
        }
    }

    /* Active immune cells produce cytokines */
    float active_cells = 0.0f;
    for (uint32_t i = 0; i < sim->num_cells; i++) {
        if (sim->cells[i].active && sim->cells[i].activation > 0.3f) {
            active_cells += sim->cells[i].count * sim->cells[i].activation;
        }
    }

    for (int c = 0; c < IMMUNO_CYTOKINE_TYPE_COUNT; c++) {
        immuno_cytokine_t* cyt = &sim->cytokines[c];

        /* Production driven by pathogen load and activated immune cells */
        float production = 0.0f;
        switch (cyt->type) {
            case IMMUNO_CYTOKINE_IL1:
            case IMMUNO_CYTOKINE_IL6:
            case IMMUNO_CYTOKINE_TNF_ALPHA:
                /* Pro-inflammatory: produced by macrophages in response to pathogen */
                production = total_load * 2.0f + active_cells * 0.01f;
                break;
            case IMMUNO_CYTOKINE_IL2:
                /* T-cell growth factor: produced by activated T-helpers */
                production = active_cells * 0.005f;
                break;
            case IMMUNO_CYTOKINE_IFN_GAMMA:
                /* Antiviral: produced by NK cells and Th1 */
                production = total_load * 1.5f + active_cells * 0.008f;
                break;
            case IMMUNO_CYTOKINE_IL10:
                /* Anti-inflammatory: regulatory feedback */
                production = active_cells * 0.002f;
                /* Increases when inflammation is high (negative feedback) */
                production += sim->inflammation.severity * 3.0f;
                break;
            default:
                break;
        }
        cyt->production_rate = production;

        /* dC/dt = production - decay * (C - baseline) */
        float dC = production - cyt->decay_rate * (cyt->concentration - cyt->baseline);
        cyt->concentration += dC * dt;
        cyt->concentration = clampf(cyt->concentration, 0.0f, 10000.0f);
    }

    return 0;
}

/* ============================================================================
 * Innate Immune Response
 * ============================================================================ */

static void step_innate_response(immunology_sim_t* sim, float dt) {
    /* Phagocytosis by neutrophils and macrophages */
    for (uint32_t i = 0; i < sim->num_cells; i++) {
        immuno_cell_t* cell = &sim->cells[i];
        if (!cell->active) continue;

        if (cell->type == IMMUNO_CELL_NEUTROPHIL ||
            cell->type == IMMUNO_CELL_MACROPHAGE) {

            /* Activation by pathogen presence */
            float pathogen_signal = 0.0f;
            for (uint32_t p = 0; p < sim->num_pathogens; p++) {
                if (sim->pathogens[p].active) {
                    pathogen_signal += sim->pathogens[p].load *
                                       sim->pathogens[p].antigen_strength;
                }
            }
            cell->activation = clampf(pathogen_signal * 0.1f, 0.0f, 1.0f);

            /* Phagocytosis: kill rate proportional to activation and count */
            float kill_rate = cell->phagocytic_rate * cell->activation * cell->count * dt;
            for (uint32_t p = 0; p < sim->num_pathogens; p++) {
                if (sim->pathogens[p].active && !sim->pathogens[p].intracellular) {
                    /* Opsonization by complement enhances phagocytosis */
                    float opsonin_factor = 1.0f + sim->complement.c3b_level * 5.0f;
                    float killed = kill_rate * opsonin_factor * 0.001f;
                    sim->pathogens[p].load -= killed;
                    if (sim->pathogens[p].load < 0.0f) sim->pathogens[p].load = 0.0f;
                }
            }
        }

        /* NK cells kill virus-infected cells */
        if (cell->type == IMMUNO_CELL_NK) {
            for (uint32_t p = 0; p < sim->num_pathogens; p++) {
                if (sim->pathogens[p].active && sim->pathogens[p].intracellular) {
                    float nk_kill = cell->cytotoxicity * cell->count * 0.0005f * dt;
                    sim->pathogens[p].load -= nk_kill;
                    if (sim->pathogens[p].load < 0.0f) sim->pathogens[p].load = 0.0f;
                }
            }
        }
    }

    /* Inflammation */
    if (sim->config.enable_inflammation) {
        float il1 = sim->cytokines[IMMUNO_CYTOKINE_IL1].concentration;
        float il6 = sim->cytokines[IMMUNO_CYTOKINE_IL6].concentration;
        float tnf = sim->cytokines[IMMUNO_CYTOKINE_TNF_ALPHA].concentration;
        float il10 = sim->cytokines[IMMUNO_CYTOKINE_IL10].concentration;

        float pro_inflam = (il1 + il6 + tnf) / 3.0f;
        float anti_inflam = il10;

        sim->inflammation.severity = clampf(
            (pro_inflam - anti_inflam) / (IMMUNO_IL1_BASELINE * 20.0f),
            0.0f, 1.0f);
        sim->inflammation.vasodilation = sim->inflammation.severity * 0.8f;
        sim->inflammation.permeability = sim->inflammation.severity * 0.6f;
        sim->inflammation.chemotaxis = clampf(sim->complement.c5a_level * 2.0f, 0.0f, 1.0f);

        /* Fever: TNF and IL-1 are pyrogens */
        sim->inflammation.temperature_c = 37.0f +
            clampf((tnf + il1 - IMMUNO_TNF_ALPHA_BASELINE - IMMUNO_IL1_BASELINE) * 0.01f,
                   0.0f, 4.0f);
    }
}

/* ============================================================================
 * Adaptive Immune Response
 * ============================================================================ */

static void step_adaptive_response(immunology_sim_t* sim, float dt) {
    if (!sim->config.enable_adaptive) return;
    float time_h = sim->time_since_infection_h;

    /* Adaptive response onset delay */
    float onset = sim->primary_response ? IMMUNO_ADAPTIVE_ONSET_H : IMMUNO_MEMORY_ONSET_H;
    if (time_h < onset * 0.5f) return; /* ramp up starting at half the onset time */

    float response_strength = clampf((time_h - onset * 0.5f) / (onset * 0.5f), 0.0f, 1.0f);

    /* IL-2 drives clonal expansion */
    float il2 = sim->cytokines[IMMUNO_CYTOKINE_IL2].concentration;

    for (uint32_t i = 0; i < sim->num_cells; i++) {
        immuno_cell_t* cell = &sim->cells[i];
        if (!cell->active) continue;

        /* T-helper activation (CD4+ via MHC-II) */
        if (cell->type == IMMUNO_CELL_T_HELPER) {
            cell->activation = response_strength * cell->specificity;
            /* Clonal expansion: doubling time ~ 8 hours */
            if (cell->activation > 0.3f) {
                float growth = cell->count * (logf(2.0f) / IMMUNO_TCELL_DOUBLING_H) *
                               cell->activation * il2 / (IMMUNO_IL2_BASELINE + il2) * dt;
                cell->count += growth;
            }
        }

        /* Cytotoxic T-cell activation (CD8+ via MHC-I) */
        if (cell->type == IMMUNO_CELL_T_CYTOTOXIC) {
            cell->activation = response_strength * cell->specificity;
            if (cell->activation > 0.3f) {
                float growth = cell->count * (logf(2.0f) / IMMUNO_TCELL_DOUBLING_H) *
                               cell->activation * dt;
                cell->count += growth;

                /* Kill intracellular pathogens */
                for (uint32_t p = 0; p < sim->num_pathogens; p++) {
                    if (sim->pathogens[p].active && sim->pathogens[p].intracellular) {
                        float killed = cell->cytotoxicity * cell->count * 0.001f * dt;
                        sim->pathogens[p].load -= killed;
                        if (sim->pathogens[p].load < 0.0f) sim->pathogens[p].load = 0.0f;
                    }
                }
            }
        }

        /* B-cell -> Plasma cell differentiation and antibody production */
        if (cell->type == IMMUNO_CELL_B_NAIVE && cell->activation > 0.5f) {
            /* Differentiate some to plasma cells */
            if (sim->num_cells < IMMUNO_MAX_IMMUNE_CELLS) {
                immuno_cell_t plasma;
                memset(&plasma, 0, sizeof(plasma));
                plasma.id = sim->num_cells;
                plasma.type = IMMUNO_CELL_B_PLASMA;
                plasma.count = cell->count * 0.1f;
                plasma.activation = 1.0f;
                plasma.specificity = cell->specificity;
                plasma.target_pathogen = cell->target_pathogen;
                plasma.active = true;
                sim->cells[sim->num_cells] = plasma;
                sim->num_cells++;
            }
            cell->count *= 0.9f;
        }

        /* Plasma cells produce antibodies */
        if (cell->type == IMMUNO_CELL_B_PLASMA && cell->activation > 0.5f) {
            for (uint32_t a = 0; a < sim->num_antibodies; a++) {
                immuno_antibody_t* ab = &sim->antibodies[a];
                if (ab->target_pathogen == cell->target_pathogen) {
                    ab->concentration += cell->count * 0.01f * dt;
                }
            }
        }
    }

    /* Antibody neutralization of pathogens */
    for (uint32_t a = 0; a < sim->num_antibodies; a++) {
        immuno_antibody_t* ab = &sim->antibodies[a];
        if (!ab->active || ab->concentration < 0.01f) continue;

        for (uint32_t p = 0; p < sim->num_pathogens; p++) {
            if (sim->pathogens[p].active &&
                ab->target_pathogen == sim->pathogens[p].id) {
                float binding = immunology_antibody_binding(
                    ab->affinity_ka, ab->concentration, sim->pathogens[p].load);
                float neutralized = binding * ab->neutralization_rate * dt;
                sim->pathogens[p].load -= neutralized;
                if (sim->pathogens[p].load < 0.0f) sim->pathogens[p].load = 0.0f;
            }
        }

        /* Antibody decay (class-dependent half-life) */
        float halflife = IMMUNO_ANTIBODY_HALFLIFE_IGG_H;
        switch (ab->ab_class) {
            case IMMUNO_AB_IGM: halflife = IMMUNO_ANTIBODY_HALFLIFE_IGM_H; break;
            case IMMUNO_AB_IGA: halflife = IMMUNO_ANTIBODY_HALFLIFE_IGA_H; break;
            case IMMUNO_AB_IGE: halflife = IMMUNO_ANTIBODY_HALFLIFE_IGE_H; break;
            default: break;
        }
        ab->concentration *= expf(-logf(2.0f) * dt / halflife);
    }
}

/* ============================================================================
 * Pathogen Replication
 * ============================================================================ */

static void step_pathogen_growth(immunology_sim_t* sim, float dt) {
    for (uint32_t i = 0; i < sim->num_pathogens; i++) {
        immuno_pathogen_t* p = &sim->pathogens[i];
        if (!p->active || p->load <= 0.0f) continue;

        /* Exponential growth with carrying capacity (logistic) */
        float K = 12.0f; /* log10 carrying capacity (~10^12 bacteria) */
        float growth = p->replication_rate * p->load * (1.0f - p->load / K) * dt;
        p->load += growth;

        /* Immune evasion reduces effectiveness of immune response */
        /* Already factored into phagocytosis and antibody neutralization */

        /* Clear pathogen if load drops below threshold */
        if (p->load < 0.1f) {
            p->load = 0.0f;
            p->active = false;
            LOG_INFO(LOG_TAG, "Pathogen %s cleared", p->name);
        }
    }
}

/* ============================================================================
 * Vaccine
 * ============================================================================ */

int immunology_vaccinate(immunology_sim_t* sim, uint32_t pathogen_idx,
                         float antigen_dose) {
    if (!sim || pathogen_idx >= sim->num_pathogens) return -1;

    /* Vaccination presents antigen without replicating pathogen */
    /* Creates memory B and T cells for faster secondary response */
    sim->primary_response = false; /* next infection will be secondary */

    /* Add memory B cells */
    if (sim->num_cells < IMMUNO_MAX_IMMUNE_CELLS) {
        immuno_cell_t memory_b;
        memset(&memory_b, 0, sizeof(memory_b));
        memory_b.id = sim->num_cells;
        memory_b.type = IMMUNO_CELL_B_MEMORY;
        memory_b.count = antigen_dose * 100.0f;
        memory_b.activation = 0.0f;
        memory_b.specificity = 0.9f;
        memory_b.target_pathogen = pathogen_idx;
        memory_b.active = true;
        sim->cells[sim->num_cells] = memory_b;
        sim->num_cells++;
    }

    /* Add memory T cells */
    if (sim->num_cells < IMMUNO_MAX_IMMUNE_CELLS) {
        immuno_cell_t memory_t;
        memset(&memory_t, 0, sizeof(memory_t));
        memory_t.id = sim->num_cells;
        memory_t.type = IMMUNO_CELL_T_MEMORY;
        memory_t.count = antigen_dose * 50.0f;
        memory_t.activation = 0.0f;
        memory_t.specificity = 0.85f;
        memory_t.target_pathogen = pathogen_idx;
        memory_t.active = true;
        sim->cells[sim->num_cells] = memory_t;
        sim->num_cells++;
    }

    /* Generate initial IgG antibodies (affinity-matured) */
    if (sim->num_antibodies < IMMUNO_MAX_ANTIBODIES) {
        immuno_antibody_t ab;
        memset(&ab, 0, sizeof(ab));
        ab.id = sim->num_antibodies;
        ab.ab_class = IMMUNO_AB_IGG;
        ab.concentration = antigen_dose * 5.0f;
        ab.affinity_ka = IMMUNO_KA_HIGH;
        ab.target_pathogen = pathogen_idx;
        ab.neutralization_rate = 0.5f;
        ab.opsonization_factor = 3.0f;
        ab.active = true;
        sim->antibodies[sim->num_antibodies] = ab;
        sim->num_antibodies++;
    }

    LOG_INFO(LOG_TAG, "Vaccination against pathogen %u (dose=%.2f)", pathogen_idx, antigen_dose);
    return 0;
}

/* ============================================================================
 * Main Step
 * ============================================================================ */

int immunology_step(immunology_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;

    sim->time_since_infection_h += dt;

    /* Pathogen replication */
    step_pathogen_growth(sim, dt);

    /* Complement cascade */
    if (sim->config.enable_complement) {
        immunology_step_complement(sim, dt);
    }

    /* Cytokine dynamics */
    immunology_step_cytokines(sim, dt);

    /* Innate immune response */
    step_innate_response(sim, dt);

    /* Adaptive immune response */
    step_adaptive_response(sim, dt);

    /* Update stats */
    sim->stats.step_count++;
    sim->stats.active_pathogens = 0;
    sim->stats.total_pathogen_load = 0.0f;
    for (uint32_t i = 0; i < sim->num_pathogens; i++) {
        if (sim->pathogens[i].active) {
            sim->stats.active_pathogens++;
            sim->stats.total_pathogen_load += sim->pathogens[i].load;
        }
    }

    sim->stats.active_immune_cells = 0;
    for (uint32_t i = 0; i < sim->num_cells; i++) {
        if (sim->cells[i].active) sim->stats.active_immune_cells++;
    }

    sim->stats.antibody_types = sim->num_antibodies;
    sim->stats.total_antibody_conc = 0.0f;
    for (uint32_t i = 0; i < sim->num_antibodies; i++) {
        sim->stats.total_antibody_conc += sim->antibodies[i].concentration;
    }

    sim->stats.inflammation_level = sim->inflammation.severity;
    sim->stats.complement_activation = sim->complement.mac_level;
    sim->stats.infection_cleared = (sim->stats.active_pathogens == 0 &&
                                     sim->stats.step_count > 1);

    return 0;
}

/* ============================================================================
 * Preset: Bacterial Infection
 * ============================================================================ */

void immunology_load_bacterial_infection(immunology_sim_t* sim) {
    if (!sim) return;

    sim->num_pathogens = 0;
    sim->num_cells = 0;
    sim->num_antibodies = 0;

    /* S. aureus-like bacterium */
    immuno_pathogen_t staph;
    memset(&staph, 0, sizeof(staph));
    strncpy(staph.name, "S.aureus", IMMUNO_MAX_NAME_LEN - 1);
    staph.type = IMMUNO_PATHOGEN_BACTERIA;
    staph.load = 6.0f;             /* 10^6 bacteria */
    staph.virulence = 0.6f;
    staph.replication_rate = 0.3f; /* ~2.3h doubling time */
    staph.immune_evasion = 0.3f;
    staph.antigen_strength = 0.8f;
    staph.intracellular = false;
    staph.active = true;
    immunology_add_pathogen(sim, &staph);

    /* Neutrophils - first responders */
    immuno_cell_t neut;
    memset(&neut, 0, sizeof(neut));
    neut.type = IMMUNO_CELL_NEUTROPHIL;
    neut.count = IMMUNO_NEUTROPHIL_NORMAL;
    neut.phagocytic_rate = 5.0f;
    neut.active = true;
    immunology_add_immune_cell(sim, &neut);

    /* Macrophages */
    immuno_cell_t macro;
    memset(&macro, 0, sizeof(macro));
    macro.type = IMMUNO_CELL_MACROPHAGE;
    macro.count = 300.0f;
    macro.phagocytic_rate = 3.0f;
    macro.active = true;
    immunology_add_immune_cell(sim, &macro);

    /* NK cells */
    immuno_cell_t nk;
    memset(&nk, 0, sizeof(nk));
    nk.type = IMMUNO_CELL_NK;
    nk.count = 200.0f;
    nk.cytotoxicity = 0.6f;
    nk.active = true;
    immunology_add_immune_cell(sim, &nk);

    /* Naive B cells (will differentiate) */
    immuno_cell_t bcell;
    memset(&bcell, 0, sizeof(bcell));
    bcell.type = IMMUNO_CELL_B_NAIVE;
    bcell.count = 500.0f;
    bcell.specificity = 0.7f;
    bcell.target_pathogen = 0;
    bcell.active = true;
    immunology_add_immune_cell(sim, &bcell);

    /* T-helper cells */
    immuno_cell_t th;
    memset(&th, 0, sizeof(th));
    th.type = IMMUNO_CELL_T_HELPER;
    th.count = 400.0f;
    th.specificity = 0.65f;
    th.target_pathogen = 0;
    th.active = true;
    immunology_add_immune_cell(sim, &th);

    /* Initial IgM antibody (low affinity, first response) */
    immuno_antibody_t igm;
    memset(&igm, 0, sizeof(igm));
    igm.ab_class = IMMUNO_AB_IGM;
    igm.concentration = 0.1f;
    igm.affinity_ka = IMMUNO_KA_LOW;
    igm.target_pathogen = 0;
    igm.neutralization_rate = 0.1f;
    igm.opsonization_factor = 2.0f;
    igm.active = true;
    sim->antibodies[0] = igm;
    sim->num_antibodies = 1;

    /* Activate complement (classical pathway via antibody) */
    immunology_activate_complement(sim, IMMUNO_COMPLEMENT_CLASSICAL);

    sim->time_since_infection_h = 0.0f;
    sim->primary_response = true;

    LOG_INFO(LOG_TAG, "Loaded bacterial infection scenario (S.aureus, 10^6 load)");
}
