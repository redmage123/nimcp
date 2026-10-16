/**
 * @file nimcp_biology_sim.c
 * @brief Biology Simulator -- population dynamics, metabolism, genetics, ecosystems
 *
 * WHAT: Simulates biological systems: populations, food webs, metabolic pathways,
 *       genetic inheritance, body physiology, and homeostasis.
 * WHY:  Provides biology prior for world model. "Plants need sunlight to grow"
 *       and "predators reduce prey populations" require biological reasoning.
 * HOW:  Lotka-Volterra dynamics for predator-prey, logistic growth for producers,
 *       RK2 (midpoint) integration, Mendelian genetics, homeostatic body model.
 */

#include "cognitive/physics/nimcp_biology_sim.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "BIOLOGY_SIM"

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline float minf(float a, float b) { return a < b ? a : b; }

/**
 * Temperature effect on metabolic rate (Q10 rule approximation).
 * Rate doubles for every 10C increase, baseline at 20C.
 */
static float temperature_metabolic_factor(float temp_c) {
    float delta = (temp_c - 20.0f) / 10.0f;
    /* clamp to prevent extreme factors */
    delta = clampf(delta, -3.0f, 3.0f);
    return powf(2.0f, delta);
}

/**
 * Sunlight factor for producer growth.
 * Combines raw sunlight intensity with time-of-day and season.
 */
static float sunlight_growth_factor(const bio_environment_t *env) {
    /* base sunlight intensity */
    float factor = env->sunlight;

    /* time-of-day modulation: peak at noon (12h), zero at midnight */
    float hour_rad = (env->time_of_day - 12.0f) / 12.0f * 3.14159265f;
    float day_mod = maxf(0.0f, cosf(hour_rad));
    factor *= day_mod;

    /* seasonal modulation: peak in summer (season=0.5), low in winter (0/1) */
    float season_rad = (env->season - 0.5f) * 2.0f * 3.14159265f;
    float season_mod = 0.5f + 0.5f * cosf(season_rad);
    factor *= season_mod;

    return clampf(factor, 0.0f, 1.0f);
}

/* ============================================================================
 * Default Config
 * ============================================================================ */

bio_config_t biology_sim_default_config(void) {
    bio_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dt = 1.0f;                     /* 1 day per step */
    cfg.extinction_threshold = 0.5f;   /* below 0.5 individuals = extinct */
    cfg.energy_transfer_eff = 0.10f;   /* ~10% trophic efficiency */
    cfg.enable_genetics = false;
    cfg.enable_physiology = false;
    return cfg;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

biology_sim_t* biology_sim_create(const bio_config_t *config) {
    biology_sim_t *sim = (biology_sim_t *)nimcp_calloc(1, sizeof(biology_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate biology_sim_t");
        return NULL;
    }

    if (config) {
        sim->config = *config;
    } else {
        sim->config = biology_sim_default_config();
    }

    /* Default environment */
    sim->environment.temperature = 20.0f;
    sim->environment.sunlight = 0.8f;
    sim->environment.water_available = 1.0f;
    sim->environment.oxygen_level = 0.21f;
    sim->environment.co2_level = 400.0f;
    sim->environment.nutrient_level = 0.7f;
    sim->environment.time_of_day = 12.0f;
    sim->environment.season = 0.5f;

    sim->initialized = true;

    LOG_INFO(LOG_TAG, "Biology sim created: dt=%.2f, extinction_thresh=%.2f, "
             "trophic_eff=%.2f", sim->config.dt, sim->config.extinction_threshold,
             sim->config.energy_transfer_eff);

    return sim;
}

void biology_sim_destroy(biology_sim_t *sim) {
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Biology sim destroyed: steps=%lu, extinctions=%lu",
             (unsigned long)sim->step_count, (unsigned long)sim->extinctions);
    nimcp_free(sim);
}

/* ============================================================================
 * Add Species / Interaction / Gene
 * ============================================================================ */

uint32_t biology_sim_add_species(biology_sim_t *sim, const bio_species_t *sp) {
    if (!sim || !sp) return (uint32_t)-1;
    if (sim->num_species >= BIO_MAX_SPECIES) {
        LOG_WARN(LOG_TAG, "Species array full (%u)", BIO_MAX_SPECIES);
        return (uint32_t)-1;
    }
    uint32_t id = sim->num_species;
    sim->species[id] = *sp;
    sim->species[id].id = id;
    sim->species[id].active = true;
    if (sim->species[id].health <= 0.0f) {
        sim->species[id].health = 1.0f;
    }
    sim->num_species++;
    return id;
}

uint32_t biology_sim_add_interaction(biology_sim_t *sim, const bio_interaction_t *inter) {
    if (!sim || !inter) return (uint32_t)-1;
    if (sim->num_interactions >= BIO_MAX_INTERACTIONS) {
        LOG_WARN(LOG_TAG, "Interaction array full (%u)", BIO_MAX_INTERACTIONS);
        return (uint32_t)-1;
    }
    uint32_t id = sim->num_interactions;
    sim->interactions[id] = *inter;
    sim->interactions[id].active = true;
    sim->num_interactions++;
    return id;
}

uint32_t biology_sim_add_gene(biology_sim_t *sim, const bio_gene_t *gene) {
    if (!sim || !gene) return (uint32_t)-1;
    if (sim->num_genes >= BIO_MAX_GENES) {
        LOG_WARN(LOG_TAG, "Gene array full (%u)", BIO_MAX_GENES);
        return (uint32_t)-1;
    }
    uint32_t id = sim->num_genes;
    sim->genes[id] = *gene;
    sim->num_genes++;
    return id;
}

/* ============================================================================
 * Set Environment
 * ============================================================================ */

void biology_sim_set_environment(biology_sim_t *sim, const bio_environment_t *env) {
    if (!sim || !env) return;
    sim->environment = *env;
}

/* ============================================================================
 * Biodiversity (Shannon Index)
 * ============================================================================ */

float biology_sim_biodiversity(const biology_sim_t *sim) {
    if (!sim || sim->num_species == 0) return 0.0f;

    /* Compute total population of active species */
    float total = 0.0f;
    for (uint32_t i = 0; i < sim->num_species; i++) {
        if (sim->species[i].active && sim->species[i].population > 0.0f) {
            total += sim->species[i].population;
        }
    }
    if (total <= 0.0f) return 0.0f;

    /* H = -sum(pi * ln(pi)) */
    float H = 0.0f;
    for (uint32_t i = 0; i < sim->num_species; i++) {
        if (!sim->species[i].active || sim->species[i].population <= 0.0f) continue;
        float pi = sim->species[i].population / total;
        if (pi > 1e-12f) {
            H -= pi * logf(pi);
        }
    }
    return H;
}

/* ============================================================================
 * Total Biomass
 * ============================================================================ */

float biology_sim_total_biomass(const biology_sim_t *sim) {
    if (!sim) return 0.0f;
    float total = 0.0f;
    for (uint32_t i = 0; i < sim->num_species; i++) {
        if (sim->species[i].active) {
            total += sim->species[i].population;
        }
    }
    return total;
}

/* ============================================================================
 * Population Derivatives
 *
 * Computes dP/dt for each species based on:
 *   - Logistic growth for producers: dN/dt = r*N*(1 - N/K) * sunlight_factor
 *   - Lotka-Volterra for consumers via interactions
 *   - Environmental modulation (temperature, water)
 * ============================================================================ */

static void compute_derivatives(const biology_sim_t *sim,
                                const float *pops,
                                float *dpdt) {
    const bio_environment_t *env = &sim->environment;
    float temp_factor = temperature_metabolic_factor(env->temperature);
    float sun_factor  = sunlight_growth_factor(env);
    float water_factor = clampf(env->water_available, 0.0f, 1.0f);

    /* Initialize derivatives to zero */
    for (uint32_t i = 0; i < sim->num_species; i++) {
        dpdt[i] = 0.0f;
    }

    /* Intrinsic growth for each species */
    for (uint32_t i = 0; i < sim->num_species; i++) {
        const bio_species_t *sp = &sim->species[i];
        if (!sp->active || pops[i] <= 0.0f) continue;

        float N = pops[i];
        float K = sp->carrying_capacity;
        float r = sp->growth_rate;

        if (sp->trophic_level == BIO_TROPHIC_PRODUCER) {
            /* Logistic growth modulated by sunlight and water */
            float growth_mod = sun_factor * water_factor;
            dpdt[i] += r * N * (1.0f - N / K) * growth_mod;
        } else if (sp->trophic_level == BIO_TROPHIC_DECOMPOSER) {
            /* Decomposers grow based on available dead matter (nutrient level) */
            float nutrient_mod = clampf(env->nutrient_level, 0.0f, 1.0f);
            dpdt[i] += r * N * (1.0f - N / K) * nutrient_mod;
        } else {
            /* Consumers: natural death rate (they need food from interactions) */
            dpdt[i] -= sp->death_rate * N;
        }

        /* Temperature affects all metabolic rates */
        dpdt[i] *= temp_factor;
    }

    /* Interaction effects */
    for (uint32_t k = 0; k < sim->num_interactions; k++) {
        const bio_interaction_t *inter = &sim->interactions[k];
        if (!inter->active) continue;

        uint32_t a = inter->species_a;
        uint32_t b = inter->species_b;
        if (a >= sim->num_species || b >= sim->num_species) continue;
        if (!sim->species[a].active || !sim->species[b].active) continue;

        float Pa = pops[a];
        float Pb = pops[b];
        if (Pa <= 0.0f && Pb <= 0.0f) continue;

        float beta = inter->strength;
        float eff  = inter->efficiency;

        switch (inter->type) {
        case BIO_INTERACT_PREDATION:
            /* A eats B: Lotka-Volterra
             * dPb/dt -= beta * Pa * Pb   (prey decreases)
             * dPa/dt += eff * beta * Pa * Pb  (predator gains from prey) */
            {
                float kill_rate = beta * Pa * Pb;
                dpdt[b] -= kill_rate;
                dpdt[a] += eff * kill_rate;
            }
            break;

        case BIO_INTERACT_COMPETITION:
            /* Both lose proportional to the other's population */
            dpdt[a] -= beta * Pa * Pb / maxf(sim->species[a].carrying_capacity, 1.0f);
            dpdt[b] -= beta * Pa * Pb / maxf(sim->species[b].carrying_capacity, 1.0f);
            break;

        case BIO_INTERACT_MUTUALISM:
        case BIO_INTERACT_POLLINATION:
            /* Both benefit */
            dpdt[a] += beta * Pb * 0.01f;
            dpdt[b] += beta * Pa * 0.01f;
            break;

        case BIO_INTERACT_PARASITISM:
            /* A benefits, B harmed */
            dpdt[a] += eff * beta * Pb;
            dpdt[b] -= beta * Pa;
            break;

        case BIO_INTERACT_COMMENSALISM:
            /* A benefits, B unaffected */
            dpdt[a] += eff * beta * Pb * 0.01f;
            break;

        case BIO_INTERACT_DECOMPOSITION:
            /* A (decomposer) benefits from dead matter of B */
            {
                float dead_matter = maxf(0.0f,
                    sim->species[b].carrying_capacity - Pb) * 0.001f;
                dpdt[a] += eff * beta * dead_matter;
            }
            break;

        default:
            break;
        }
    }
}

/* ============================================================================
 * Ecosystem Step (RK2 Midpoint Method)
 * ============================================================================ */

int biology_sim_step(biology_sim_t *sim, float dt) {
    if (!sim) return -1;

    float override_dt = (dt > 0.0f) ? dt : sim->config.dt;
    uint32_t n = sim->num_species;
    if (n == 0) return 0;

    /* Temporary arrays on stack (max 64 species, small) */
    float pops[BIO_MAX_SPECIES];
    float k1[BIO_MAX_SPECIES];
    float k2[BIO_MAX_SPECIES];
    float mid[BIO_MAX_SPECIES];

    /* Copy current populations */
    for (uint32_t i = 0; i < n; i++) {
        pops[i] = sim->species[i].population;
    }

    /* --- RK2 Stage 1: compute k1 = f(t, y) --- */
    compute_derivatives(sim, pops, k1);

    /* --- RK2 Stage 2: compute midpoint y_mid = y + 0.5*dt*k1, k2 = f(t+0.5dt, y_mid) --- */
    for (uint32_t i = 0; i < n; i++) {
        mid[i] = pops[i] + 0.5f * override_dt * k1[i];
        /* clamp midpoint to prevent negative intermediates */
        mid[i] = maxf(mid[i], 0.0f);
    }
    compute_derivatives(sim, mid, k2);

    /* --- Update: y_new = y + dt * k2 --- */
    for (uint32_t i = 0; i < n; i++) {
        if (!sim->species[i].active) continue;

        float new_pop = pops[i] + override_dt * k2[i];

        /* Clamp to [0, carrying_capacity * 2] */
        float cap = sim->species[i].carrying_capacity;
        new_pop = clampf(new_pop, 0.0f, cap * 2.0f);

        /* Energy bookkeeping: producers generate, consumers deplete */
        if (sim->species[i].trophic_level == BIO_TROPHIC_PRODUCER) {
            float gained = maxf(0.0f, new_pop - pops[i]) *
                           sim->species[i].energy_need;
            sim->species[i].energy_stored += gained * sim->config.energy_transfer_eff;
        } else {
            sim->species[i].energy_stored -=
                sim->species[i].energy_need * override_dt;
            sim->species[i].energy_stored =
                maxf(sim->species[i].energy_stored, 0.0f);
        }

        /* Check extinction */
        if (new_pop < sim->config.extinction_threshold) {
            if (sim->species[i].active && pops[i] >= sim->config.extinction_threshold) {
                LOG_WARN(LOG_TAG, "Species '%s' went extinct (pop=%.2f)",
                         sim->species[i].name, new_pop);
                sim->extinctions++;
            }
            new_pop = 0.0f;
            sim->species[i].active = false;
        }

        sim->species[i].population = new_pop;

        /* Update health based on ratio to carrying capacity */
        if (sim->species[i].active && cap > 0.0f) {
            float ratio = new_pop / cap;
            sim->species[i].health = clampf(ratio, 0.0f, 1.0f);
        }
    }

    /* Update aggregate statistics */
    sim->total_biomass = biology_sim_total_biomass(sim);
    sim->biodiversity_index = biology_sim_biodiversity(sim);

    /* Compute total energy flow through ecosystem */
    sim->total_energy = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        if (sim->species[i].active) {
            sim->total_energy += sim->species[i].energy_stored;
        }
    }

    sim->step_count++;
    return 0;
}

/* ============================================================================
 * Body Physiology Step
 * ============================================================================ */

int biology_sim_step_body(biology_sim_t *sim, float dt) {
    if (!sim) return -1;

    float override_dt = (dt > 0.0f) ? dt : sim->config.dt;
    bio_body_t *body = &sim->body;

    if (body->num_organs == 0) return 0;

    /* --- Organ energy consumption --- */
    float total_consumption = 0.0f;
    for (uint32_t i = 0; i < body->num_organs; i++) {
        bio_organ_t *organ = &body->organs[i];
        if (organ->health <= 0.0f) continue;

        float consumption = organ->metabolic_rate * override_dt * organ->health;
        total_consumption += consumption;

        /* Function level degrades if health is low */
        if (organ->health < 0.5f) {
            organ->function_level *= (0.5f + organ->health);
        }
        organ->function_level = clampf(organ->function_level, 0.0f, 1.0f);
    }

    /* --- Blood glucose regulation (homeostasis) --- */
    /* Normal range: 4.0 - 6.0 mmol/L */
    float glucose_consumption = total_consumption * 0.1f;
    body->blood_glucose -= glucose_consumption;
    /* Liver produces glucose when low (gluconeogenesis) */
    if (body->blood_glucose < 4.0f) {
        /* Find liver organ */
        for (uint32_t i = 0; i < body->num_organs; i++) {
            if (strncmp(body->organs[i].name, "liver", 5) == 0) {
                float liver_output = body->organs[i].function_level * 0.5f * override_dt;
                body->blood_glucose += liver_output;
                break;
            }
        }
    }
    /* Insulin effect: bring down high glucose */
    if (body->blood_glucose > 7.0f) {
        body->blood_glucose -= (body->blood_glucose - 6.0f) * 0.2f * override_dt;
    }
    body->blood_glucose = clampf(body->blood_glucose, 0.0f, 20.0f);

    /* --- Body temperature regulation (homeostasis around 37C) --- */
    float target_temp = 37.0f;
    float temp_error = target_temp - body->body_temperature;
    /* Negative feedback: body adjusts toward target */
    body->body_temperature += temp_error * 0.1f * override_dt;
    /* Environmental influence */
    float env_temp = sim->environment.temperature;
    body->body_temperature += (env_temp - body->body_temperature) * 0.01f * override_dt;
    body->body_temperature = clampf(body->body_temperature, 25.0f, 45.0f);

    /* --- Blood oxygen regulation --- */
    /* Lungs provide oxygen */
    for (uint32_t i = 0; i < body->num_organs; i++) {
        if (strncmp(body->organs[i].name, "lungs", 5) == 0) {
            float lung_output = body->organs[i].function_level * 0.05f * override_dt;
            body->blood_oxygen += lung_output;
            break;
        }
    }
    /* Organs consume oxygen */
    float oxygen_consumption = total_consumption * 0.02f;
    body->blood_oxygen -= oxygen_consumption;
    body->blood_oxygen = clampf(body->blood_oxygen, 0.0f, 1.0f);

    /* --- Hydration decay --- */
    body->hydration -= 0.01f * override_dt;
    body->hydration = clampf(body->hydration, 0.0f, 1.0f);

    /* --- Immune strength: degrades slowly, organs support it --- */
    body->immune_strength -= 0.005f * override_dt;
    body->immune_strength = clampf(body->immune_strength, 0.0f, 1.0f);

    /* --- Organ health degrades if body conditions are poor --- */
    for (uint32_t i = 0; i < body->num_organs; i++) {
        bio_organ_t *organ = &body->organs[i];
        /* Low oxygen damages organs */
        if (body->blood_oxygen < 0.3f) {
            organ->health -= 0.02f * override_dt;
        }
        /* Low glucose damages organs */
        if (body->blood_glucose < 2.0f) {
            organ->health -= 0.01f * override_dt;
        }
        /* Hypothermia / hyperthermia */
        if (body->body_temperature < 32.0f || body->body_temperature > 42.0f) {
            organ->health -= 0.03f * override_dt;
        }
        organ->health = clampf(organ->health, 0.0f, 1.0f);
    }

    return 0;
}

/* ============================================================================
 * Violation Checking
 * ============================================================================ */

bio_violation_t biology_sim_check_violations(const biology_sim_t *sim,
                                              const bio_species_t *predicted_species,
                                              uint32_t num_species) {
    if (!sim || !predicted_species) return BIO_VIOLATION_NONE;

    bio_violation_t flags = BIO_VIOLATION_NONE;

    for (uint32_t i = 0; i < num_species && i < sim->num_species; i++) {
        const bio_species_t *pred = &predicted_species[i];
        const bio_species_t *cur  = &sim->species[i];

        /* Negative population */
        if (pred->population < 0.0f) {
            flags |= BIO_VIOLATION_NEGATIVE_POP;
        }

        /* Exceeds double carrying capacity */
        if (pred->population > cur->carrying_capacity * 2.0f) {
            flags |= BIO_VIOLATION_EXCEED_CAPACITY;
        }

        /* Dead species suddenly hunting */
        if (!cur->active && pred->population > 0.0f &&
            cur->trophic_level >= BIO_TROPHIC_PRIMARY) {
            flags |= BIO_VIOLATION_DEAD_PREDATION;
        }

        /* Plant growing without any sunlight */
        if (cur->trophic_level == BIO_TROPHIC_PRODUCER &&
            sim->environment.sunlight < 0.01f &&
            pred->population > cur->population * 1.01f) {
            flags |= BIO_VIOLATION_PLANT_NO_LIGHT;
        }

        /* Impossible growth rate: more than 100% per step */
        if (cur->population > 0.0f) {
            float growth_ratio = pred->population / cur->population;
            if (growth_ratio > 2.0f) {
                flags |= BIO_VIOLATION_IMPOSSIBLE_GROWTH;
            }
        }
    }

    /* Energy from nothing: total energy increased without producer input */
    float pred_energy = 0.0f;
    float cur_energy  = 0.0f;
    for (uint32_t i = 0; i < num_species && i < sim->num_species; i++) {
        pred_energy += predicted_species[i].energy_stored;
        cur_energy  += sim->species[i].energy_stored;
    }
    /* Allow small increase from producers, flag large unexplained increase */
    float producer_capacity = 0.0f;
    for (uint32_t i = 0; i < sim->num_species; i++) {
        if (sim->species[i].trophic_level == BIO_TROPHIC_PRODUCER &&
            sim->species[i].active) {
            producer_capacity += sim->species[i].population *
                                 sim->species[i].energy_need;
        }
    }
    if (pred_energy > cur_energy + producer_capacity * 2.0f) {
        flags |= BIO_VIOLATION_ENERGY_FROM_NOTHING;
    }

    return flags;
}

/* ============================================================================
 * Mendelian Genetics Cross (Punnett Square)
 * ============================================================================ */

void biology_sim_cross(const biology_sim_t *sim, uint32_t gene_id,
                        char parent1_a1, char parent1_a2,
                        char parent2_a1, char parent2_a2,
                        float offspring_probs[4], char offspring_genotypes[4][2]) {
    (void)sim;
    (void)gene_id;

    /*
     * Standard Punnett square: cross parent1 (a1, a2) x parent2 (a1, a2)
     * 4 possible offspring:
     *   p1_a1 + p2_a1, p1_a1 + p2_a2, p1_a2 + p2_a1, p1_a2 + p2_a2
     * Each with 0.25 probability.
     * Merge identical genotypes (order doesn't matter: Bb = bB).
     */

    char raw[4][2] = {
        { parent1_a1, parent2_a1 },
        { parent1_a1, parent2_a2 },
        { parent1_a2, parent2_a1 },
        { parent1_a2, parent2_a2 },
    };

    /* Normalize: dominant allele first (uppercase first) */
    for (int i = 0; i < 4; i++) {
        if (raw[i][0] > raw[i][1]) {
            /* Swap so that uppercase (smaller ASCII) comes first */
            char tmp = raw[i][0];
            raw[i][0] = raw[i][1];
            raw[i][1] = tmp;
        }
    }

    /* Count unique genotypes and their probabilities */
    int unique_count = 0;
    char unique[4][2];
    float unique_prob[4] = {0};

    for (int i = 0; i < 4; i++) {
        bool found = false;
        for (int j = 0; j < unique_count; j++) {
            if (unique[j][0] == raw[i][0] && unique[j][1] == raw[i][1]) {
                unique_prob[j] += 0.25f;
                found = true;
                break;
            }
        }
        if (!found) {
            unique[unique_count][0] = raw[i][0];
            unique[unique_count][1] = raw[i][1];
            unique_prob[unique_count] = 0.25f;
            unique_count++;
        }
    }

    /* Fill output arrays */
    for (int i = 0; i < 4; i++) {
        if (i < unique_count) {
            offspring_genotypes[i][0] = unique[i][0];
            offspring_genotypes[i][1] = unique[i][1];
            offspring_probs[i] = unique_prob[i];
        } else {
            offspring_genotypes[i][0] = '\0';
            offspring_genotypes[i][1] = '\0';
            offspring_probs[i] = 0.0f;
        }
    }
}

/* ============================================================================
 * Preset Ecosystems
 * ============================================================================ */

/* Helper to build a species struct */
static bio_species_t make_species(const char *name, bio_kingdom_t kingdom,
                                   bio_trophic_level_t trophic, float pop,
                                   float K, float r, float birth, float death,
                                   float energy_need, float sunlight_need) {
    bio_species_t sp;
    memset(&sp, 0, sizeof(sp));
    strncpy(sp.name, name, BIO_MAX_NAME_LEN - 1);
    sp.kingdom = kingdom;
    sp.trophic_level = trophic;
    sp.population = pop;
    sp.carrying_capacity = K;
    sp.growth_rate = r;
    sp.birth_rate = birth;
    sp.death_rate = death;
    sp.energy_need = energy_need;
    sp.water_need = 0.5f;
    sp.sunlight_need = sunlight_need;
    sp.oxygen_need = (kingdom == BIO_KINGDOM_ANIMAL) ? 0.5f : 0.0f;
    sp.co2_need = (trophic == BIO_TROPHIC_PRODUCER) ? 0.3f : 0.0f;
    sp.health = 1.0f;
    sp.energy_stored = energy_need * 10.0f;
    sp.active = true;
    return sp;
}

/* Helper to build an interaction */
static bio_interaction_t make_interaction(uint32_t a, uint32_t b,
                                           bio_interaction_type_t type,
                                           float strength, float efficiency) {
    bio_interaction_t inter;
    memset(&inter, 0, sizeof(inter));
    inter.species_a = a;
    inter.species_b = b;
    inter.type = type;
    inter.strength = strength;
    inter.efficiency = efficiency;
    inter.active = true;
    return inter;
}

/* ---------- Grassland Ecosystem ---------- */

void biology_sim_load_grassland(biology_sim_t *sim) {
    if (!sim) return;

    /* Reset */
    sim->num_species = 0;
    sim->num_interactions = 0;
    sim->num_genes = 0;
    sim->step_count = 0;
    sim->extinctions = 0;

    /*                         name          kingdom             trophic              pop     K       r     birth death energy sun */
    bio_species_t grass   = make_species("grass",     BIO_KINGDOM_PLANT,  BIO_TROPHIC_PRODUCER,   5000, 10000, 0.30f, 0.3f, 0.05f, 0.1f, 0.8f);
    bio_species_t rabbits = make_species("rabbits",   BIO_KINGDOM_ANIMAL, BIO_TROPHIC_PRIMARY,     200,   800, 0.20f, 0.2f, 0.08f, 0.5f, 0.0f);
    bio_species_t foxes   = make_species("foxes",     BIO_KINGDOM_ANIMAL, BIO_TROPHIC_SECONDARY,    30,   100, 0.05f, 0.1f, 0.05f, 1.0f, 0.0f);
    bio_species_t hawks   = make_species("hawks",     BIO_KINGDOM_ANIMAL, BIO_TROPHIC_TERTIARY,     15,    50, 0.03f, 0.08f,0.04f, 1.2f, 0.0f);
    bio_species_t insects = make_species("insects",   BIO_KINGDOM_ANIMAL, BIO_TROPHIC_PRIMARY,    1000,  5000, 0.40f, 0.4f, 0.20f, 0.05f,0.0f);
    bio_species_t mice    = make_species("mice",      BIO_KINGDOM_ANIMAL, BIO_TROPHIC_PRIMARY,     300,  1000, 0.25f, 0.25f,0.10f, 0.3f, 0.0f);
    bio_species_t decomp  = make_species("decomposers", BIO_KINGDOM_FUNGI, BIO_TROPHIC_DECOMPOSER, 500, 3000, 0.15f, 0.15f,0.10f, 0.2f, 0.0f);

    uint32_t g  = biology_sim_add_species(sim, &grass);
    uint32_t rb = biology_sim_add_species(sim, &rabbits);
    uint32_t fx = biology_sim_add_species(sim, &foxes);
    uint32_t hk = biology_sim_add_species(sim, &hawks);
    uint32_t in = biology_sim_add_species(sim, &insects);
    uint32_t mc = biology_sim_add_species(sim, &mice);
    uint32_t dc = biology_sim_add_species(sim, &decomp);

    /* Predation: rabbits eat grass, foxes eat rabbits, hawks eat mice, foxes eat mice */
    bio_interaction_t i1 = make_interaction(rb, g,  BIO_INTERACT_PREDATION, 0.0005f, 0.10f);
    bio_interaction_t i2 = make_interaction(fx, rb, BIO_INTERACT_PREDATION, 0.002f,  0.10f);
    bio_interaction_t i3 = make_interaction(hk, mc, BIO_INTERACT_PREDATION, 0.003f,  0.10f);
    bio_interaction_t i4 = make_interaction(fx, mc, BIO_INTERACT_PREDATION, 0.001f,  0.10f);
    bio_interaction_t i5 = make_interaction(hk, in, BIO_INTERACT_PREDATION, 0.0001f, 0.10f);

    /* Insects eat grass */
    bio_interaction_t i6 = make_interaction(in, g,  BIO_INTERACT_PREDATION, 0.0001f, 0.08f);

    /* Competition: rabbits vs mice for grass */
    bio_interaction_t i7 = make_interaction(rb, mc, BIO_INTERACT_COMPETITION, 0.0001f, 0.0f);

    /* Decomposition: decomposers break down all dead matter */
    bio_interaction_t i8 = make_interaction(dc, g,  BIO_INTERACT_DECOMPOSITION, 0.01f, 0.15f);
    bio_interaction_t i9 = make_interaction(dc, rb, BIO_INTERACT_DECOMPOSITION, 0.01f, 0.15f);

    /* Pollination: insects help grass */
    bio_interaction_t i10 = make_interaction(in, g, BIO_INTERACT_POLLINATION, 0.001f, 0.0f);

    biology_sim_add_interaction(sim, &i1);
    biology_sim_add_interaction(sim, &i2);
    biology_sim_add_interaction(sim, &i3);
    biology_sim_add_interaction(sim, &i4);
    biology_sim_add_interaction(sim, &i5);
    biology_sim_add_interaction(sim, &i6);
    biology_sim_add_interaction(sim, &i7);
    biology_sim_add_interaction(sim, &i8);
    biology_sim_add_interaction(sim, &i9);
    biology_sim_add_interaction(sim, &i10);

    /* Update initial stats */
    sim->total_biomass = biology_sim_total_biomass(sim);
    sim->biodiversity_index = biology_sim_biodiversity(sim);

    LOG_INFO(LOG_TAG, "Loaded grassland ecosystem: %u species, %u interactions, "
             "biomass=%.0f", sim->num_species, sim->num_interactions, sim->total_biomass);
}

/* ---------- Ocean Ecosystem ---------- */

void biology_sim_load_ocean(biology_sim_t *sim) {
    if (!sim) return;

    sim->num_species = 0;
    sim->num_interactions = 0;
    sim->num_genes = 0;
    sim->step_count = 0;
    sim->extinctions = 0;

    bio_species_t phyto    = make_species("phytoplankton", BIO_KINGDOM_PLANT,  BIO_TROPHIC_PRODUCER,  8000, 20000, 0.35f, 0.35f, 0.10f, 0.05f, 0.7f);
    bio_species_t zoo      = make_species("zooplankton",   BIO_KINGDOM_ANIMAL, BIO_TROPHIC_PRIMARY,   3000,  8000, 0.25f, 0.25f, 0.12f, 0.1f,  0.0f);
    bio_species_t smallf   = make_species("small_fish",    BIO_KINGDOM_ANIMAL, BIO_TROPHIC_PRIMARY,    500,  2000, 0.15f, 0.15f, 0.08f, 0.4f,  0.0f);
    bio_species_t largef   = make_species("large_fish",    BIO_KINGDOM_ANIMAL, BIO_TROPHIC_SECONDARY,  100,   500, 0.08f, 0.08f, 0.05f, 0.8f,  0.0f);
    bio_species_t sharks   = make_species("sharks",        BIO_KINGDOM_ANIMAL, BIO_TROPHIC_TERTIARY,    20,    80, 0.02f, 0.03f, 0.02f, 2.0f,  0.0f);
    bio_species_t seaweed  = make_species("seaweed",       BIO_KINGDOM_PLANT,  BIO_TROPHIC_PRODUCER,  4000, 10000, 0.20f, 0.20f, 0.08f, 0.08f, 0.5f);

    uint32_t ph = biology_sim_add_species(sim, &phyto);
    uint32_t zo = biology_sim_add_species(sim, &zoo);
    uint32_t sf = biology_sim_add_species(sim, &smallf);
    uint32_t lf = biology_sim_add_species(sim, &largef);
    uint32_t sk = biology_sim_add_species(sim, &sharks);
    uint32_t sw = biology_sim_add_species(sim, &seaweed);

    /* Food chain: zoo eats phyto, small_fish eat zoo, large_fish eat small_fish, sharks eat large_fish */
    biology_sim_add_interaction(sim, &(bio_interaction_t){ zo, ph, BIO_INTERACT_PREDATION, 0.0003f, 0.10f, true });
    biology_sim_add_interaction(sim, &(bio_interaction_t){ sf, zo, BIO_INTERACT_PREDATION, 0.0005f, 0.10f, true });
    biology_sim_add_interaction(sim, &(bio_interaction_t){ lf, sf, BIO_INTERACT_PREDATION, 0.002f,  0.10f, true });
    biology_sim_add_interaction(sim, &(bio_interaction_t){ sk, lf, BIO_INTERACT_PREDATION, 0.005f,  0.10f, true });

    /* Small fish also eat phytoplankton */
    biology_sim_add_interaction(sim, &(bio_interaction_t){ sf, ph, BIO_INTERACT_PREDATION, 0.0001f, 0.08f, true });

    /* Sharks also eat small fish */
    biology_sim_add_interaction(sim, &(bio_interaction_t){ sk, sf, BIO_INTERACT_PREDATION, 0.001f,  0.10f, true });

    /* Competition: phytoplankton vs seaweed for sunlight */
    biology_sim_add_interaction(sim, &(bio_interaction_t){ ph, sw, BIO_INTERACT_COMPETITION, 0.00005f, 0.0f, true });

    sim->total_biomass = biology_sim_total_biomass(sim);
    sim->biodiversity_index = biology_sim_biodiversity(sim);

    LOG_INFO(LOG_TAG, "Loaded ocean ecosystem: %u species, %u interactions, "
             "biomass=%.0f", sim->num_species, sim->num_interactions, sim->total_biomass);
}

/* ---------- Forest Ecosystem ---------- */

void biology_sim_load_forest(biology_sim_t *sim) {
    if (!sim) return;

    sim->num_species = 0;
    sim->num_interactions = 0;
    sim->num_genes = 0;
    sim->step_count = 0;
    sim->extinctions = 0;

    bio_species_t trees   = make_species("trees",    BIO_KINGDOM_PLANT,  BIO_TROPHIC_PRODUCER,   2000,  5000, 0.05f, 0.05f, 0.01f, 0.2f,  0.9f);
    bio_species_t shrubs  = make_species("shrubs",   BIO_KINGDOM_PLANT,  BIO_TROPHIC_PRODUCER,   3000,  8000, 0.15f, 0.15f, 0.03f, 0.1f,  0.6f);
    bio_species_t deer    = make_species("deer",     BIO_KINGDOM_ANIMAL, BIO_TROPHIC_PRIMARY,     150,   400, 0.10f, 0.12f, 0.06f, 0.8f,  0.0f);
    bio_species_t wolves  = make_species("wolves",   BIO_KINGDOM_ANIMAL, BIO_TROPHIC_SECONDARY,    25,    60, 0.04f, 0.06f, 0.03f, 1.5f,  0.0f);
    bio_species_t birds   = make_species("birds",    BIO_KINGDOM_ANIMAL, BIO_TROPHIC_PRIMARY,     400,  1500, 0.20f, 0.20f, 0.10f, 0.3f,  0.0f);
    bio_species_t insects = make_species("insects",  BIO_KINGDOM_ANIMAL, BIO_TROPHIC_PRIMARY,    2000,  8000, 0.40f, 0.40f, 0.20f, 0.05f, 0.0f);
    bio_species_t fungi   = make_species("fungi",    BIO_KINGDOM_FUNGI,  BIO_TROPHIC_DECOMPOSER,  800,  3000, 0.10f, 0.10f, 0.05f, 0.15f, 0.0f);

    uint32_t tr = biology_sim_add_species(sim, &trees);
    uint32_t sh = biology_sim_add_species(sim, &shrubs);
    uint32_t de = biology_sim_add_species(sim, &deer);
    uint32_t wo = biology_sim_add_species(sim, &wolves);
    uint32_t bi = biology_sim_add_species(sim, &birds);
    uint32_t in_id = biology_sim_add_species(sim, &insects);
    uint32_t fu = biology_sim_add_species(sim, &fungi);

    /* Deer eat shrubs, wolves eat deer, birds eat insects */
    biology_sim_add_interaction(sim, &(bio_interaction_t){ de, sh, BIO_INTERACT_PREDATION, 0.0004f, 0.10f, true });
    biology_sim_add_interaction(sim, &(bio_interaction_t){ wo, de, BIO_INTERACT_PREDATION, 0.003f,  0.10f, true });
    biology_sim_add_interaction(sim, &(bio_interaction_t){ bi, in_id, BIO_INTERACT_PREDATION, 0.0002f, 0.10f, true });

    /* Insects eat trees (bark beetles etc.) */
    biology_sim_add_interaction(sim, &(bio_interaction_t){ in_id, tr, BIO_INTERACT_PREDATION, 0.00005f, 0.05f, true });

    /* Competition: trees vs shrubs for sunlight */
    biology_sim_add_interaction(sim, &(bio_interaction_t){ tr, sh, BIO_INTERACT_COMPETITION, 0.00002f, 0.0f, true });

    /* Mutualism: fungi and trees (mycorrhizal) */
    biology_sim_add_interaction(sim, &(bio_interaction_t){ fu, tr, BIO_INTERACT_MUTUALISM, 0.005f, 0.0f, true });

    /* Decomposition: fungi decompose dead trees and deer */
    biology_sim_add_interaction(sim, &(bio_interaction_t){ fu, tr, BIO_INTERACT_DECOMPOSITION, 0.01f, 0.15f, true });
    biology_sim_add_interaction(sim, &(bio_interaction_t){ fu, de, BIO_INTERACT_DECOMPOSITION, 0.01f, 0.15f, true });

    /* Pollination: insects pollinate shrubs */
    biology_sim_add_interaction(sim, &(bio_interaction_t){ in_id, sh, BIO_INTERACT_POLLINATION, 0.001f, 0.0f, true });

    sim->total_biomass = biology_sim_total_biomass(sim);
    sim->biodiversity_index = biology_sim_biodiversity(sim);

    LOG_INFO(LOG_TAG, "Loaded forest ecosystem: %u species, %u interactions, "
             "biomass=%.0f", sim->num_species, sim->num_interactions, sim->total_biomass);
}

/* ============================================================================
 * Human Body Physiology Preset
 * ============================================================================ */

void biology_sim_load_human_body(biology_sim_t *sim) {
    if (!sim) return;

    bio_body_t *body = &sim->body;
    memset(body, 0, sizeof(bio_body_t));

    /* Define organs with metabolic rates (watts, roughly scaled) */
    struct {
        const char *name;
        float metabolic_rate;
    } organ_defs[] = {
        { "heart",    1.2f  },  /* ~1.2W continuous */
        { "lungs",    0.5f  },
        { "liver",    1.5f  },  /* liver is very metabolically active */
        { "kidneys",  0.7f  },
        { "brain",    1.3f  },  /* brain uses ~20% of body energy */
        { "stomach",  0.4f  },
        { "muscles",  0.8f  },  /* resting muscle tone */
    };

    uint32_t n_organs = sizeof(organ_defs) / sizeof(organ_defs[0]);
    if (n_organs > BIO_MAX_ORGANS) n_organs = BIO_MAX_ORGANS;

    for (uint32_t i = 0; i < n_organs; i++) {
        strncpy(body->organs[i].name, organ_defs[i].name, BIO_MAX_NAME_LEN - 1);
        body->organs[i].health = 1.0f;
        body->organs[i].metabolic_rate = organ_defs[i].metabolic_rate;
        body->organs[i].function_level = 1.0f;
    }
    body->num_organs = n_organs;

    /* Homeostatic setpoints */
    body->body_temperature = 37.0f;
    body->blood_oxygen = 0.98f;
    body->blood_glucose = 5.0f;   /* normal fasting: 4-6 mmol/L */
    body->hydration = 0.95f;
    body->immune_strength = 0.90f;

    LOG_INFO(LOG_TAG, "Loaded human body physiology: %u organs, temp=%.1fC, "
             "glucose=%.1f mmol/L", body->num_organs, body->body_temperature,
             body->blood_glucose);
}
