/**
 * @file nimcp_ecology.c
 * @brief Ecology simulation engine -- energy pyramids, nutrient cycling,
 *        succession, island biogeography, diversity indices, food web stability
 *
 * WHAT: Simulates ecological systems with real ecology equations.
 * WHY:  Ecological prior for world model reasoning about ecosystems.
 * HOW:  10% trophic transfer, N/P/C cycle kinetics, Lotka-Volterra competition,
 *       species-area relationship, Shannon/Simpson diversity, May's criterion.
 */

#include "cognitive/physics/nimcp_ecology.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "ECOLOGY"

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline float maxf(float a, float b) { return a > b ? a : b; }

/* ============================================================================
 * Create / Destroy / Config
 * ============================================================================ */

ecology_config_t ecology_default_config(void) {
    ecology_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dt = 1.0f;                      /* 1 day time step */
    cfg.temperature_c = 20.0f;
    cfg.enable_nutrient_cycling = true;
    cfg.enable_succession = true;
    cfg.enable_biogeography = false;
    cfg.trophic_efficiency = ECOL_TROPHIC_EFFICIENCY;
    cfg.disturbance_rate = 0.001f;
    return cfg;
}

ecology_sim_t* ecology_create(const ecology_config_t* config) {
    ecology_sim_t* sim = (ecology_sim_t*)nimcp_calloc(1, sizeof(ecology_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate ecology sim");
        return NULL;
    }
    sim->config = config ? *config : ecology_default_config();

    /* Initialize nutrient pools with defaults */
    for (int i = 0; i < ECOL_NUTRIENT_POOL_COUNT; i++) {
        sim->nutrients[i].type = (ecol_nutrient_pool_type_t)i;
    }
    sim->nutrients[ECOL_NUTRIENT_N_ATMOSPHERIC].amount = 78000.0f; /* ppm, vast reservoir */
    sim->nutrients[ECOL_NUTRIENT_N_AMMONIUM].amount = 5.0f;
    sim->nutrients[ECOL_NUTRIENT_N_NITRATE].amount = 10.0f;
    sim->nutrients[ECOL_NUTRIENT_N_ORGANIC].amount = 50.0f;
    sim->nutrients[ECOL_NUTRIENT_P_DISSOLVED].amount = 2.0f;
    sim->nutrients[ECOL_NUTRIENT_P_ORGANIC].amount = 20.0f;
    sim->nutrients[ECOL_NUTRIENT_C_ATMOSPHERIC].amount = 420.0f; /* ppm CO2 */
    sim->nutrients[ECOL_NUTRIENT_C_ORGANIC].amount = 100.0f;

    sim->succession.stage = ECOL_SUCCESSION_BARE;
    sim->initialized = true;

    LOG_INFO(LOG_TAG, "Ecology sim created (dt=%.1f days)", sim->config.dt);
    return sim;
}

void ecology_destroy(ecology_sim_t* sim) {
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Ecology sim destroyed after %lu steps", sim->stats.step_count);
    nimcp_free(sim);
}

ecology_stats_t ecology_get_stats(const ecology_sim_t* sim) {
    ecology_stats_t empty;
    memset(&empty, 0, sizeof(empty));
    return sim ? sim->stats : empty;
}

/* ============================================================================
 * Species / Food Web Management
 * ============================================================================ */

int ecology_add_species(ecology_sim_t* sim, const ecol_species_t* sp) {
    if (!sim || !sp) return -1;
    if (sim->num_species >= ECOL_MAX_SPECIES) return -1;
    sim->species[sim->num_species] = *sp;
    sim->num_species++;
    return 0;
}

int ecology_add_link(ecology_sim_t* sim, const ecol_food_web_link_t* link) {
    if (!sim || !link) return -1;
    if (sim->num_links >= ECOL_MAX_SPECIES * 4) return -1;
    sim->links[sim->num_links] = *link;
    sim->num_links++;
    return 0;
}

/* ============================================================================
 * Diversity Indices
 * ============================================================================ */

/**
 * Shannon-Wiener diversity: H' = -sum(pi * ln(pi))
 * Higher H' = more diverse community.
 */
float ecology_shannon_index(const float* abundances, uint32_t n) {
    float total = 0.0f;
    for (uint32_t i = 0; i < n; i++) total += abundances[i];
    if (total <= 0.0f) return 0.0f;

    float h = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float pi = abundances[i] / total;
        if (pi > 1e-10f) {
            h -= pi * logf(pi);
        }
    }
    return h;
}

/**
 * Simpson's diversity: D = 1 - sum(pi^2)
 * Higher D = more diverse (0 = monoculture, 1 = infinite diversity).
 */
float ecology_simpson_index(const float* abundances, uint32_t n) {
    float total = 0.0f;
    for (uint32_t i = 0; i < n; i++) total += abundances[i];
    if (total <= 0.0f) return 0.0f;

    float sum_pi2 = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float pi = abundances[i] / total;
        sum_pi2 += pi * pi;
    }
    return 1.0f - sum_pi2;
}

ecol_diversity_t ecology_compute_diversity(const ecology_sim_t* sim) {
    ecol_diversity_t div;
    memset(&div, 0, sizeof(div));
    if (!sim || sim->num_species == 0) return div;

    float abundances[ECOL_MAX_SPECIES];
    uint32_t active_count = 0;
    float total_abund = 0.0f;

    for (uint32_t i = 0; i < sim->num_species; i++) {
        if (sim->species[i].active && sim->species[i].abundance > 0.0f) {
            abundances[active_count] = sim->species[i].abundance;
            total_abund += sim->species[i].abundance;
            active_count++;
        }
    }

    div.species_richness = (float)active_count;
    div.total_abundance = total_abund;
    div.shannon_h = ecology_shannon_index(abundances, active_count);
    div.simpson_d = ecology_simpson_index(abundances, active_count);

    /* Pielou's evenness: J = H / ln(S).  One species = perfect evenness */
    if (active_count == 1) {
        div.evenness = 1.0f;
    } else if (active_count > 1) {
        div.evenness = div.shannon_h / logf((float)active_count);
    }

    return div;
}

/* ============================================================================
 * Energy Pyramid
 * ============================================================================ */

int ecology_compute_energy_pyramid(ecology_sim_t* sim) {
    if (!sim) return -1;

    memset(sim->energy_pyramid, 0, sizeof(sim->energy_pyramid));

    for (uint32_t i = 0; i < sim->num_species; i++) {
        if (!sim->species[i].active) continue;
        int level = sim->species[i].trophic_level;
        if (level >= 0 && level < ECOL_TROPHIC_LEVEL_COUNT) {
            sim->energy_pyramid[level] += sim->species[i].abundance *
                                           sim->species[i].biomass_per_individual *
                                           sim->species[i].metabolic_rate;
        }
    }

    return 0;
}

/* ============================================================================
 * Nutrient Cycling
 * ============================================================================ */

int ecology_step_nitrogen_cycle(ecology_sim_t* sim, float dt) {
    if (!sim) return -1;

    float* n_atm = &sim->nutrients[ECOL_NUTRIENT_N_ATMOSPHERIC].amount;
    float* n_nh4 = &sim->nutrients[ECOL_NUTRIENT_N_AMMONIUM].amount;
    float* n_no3 = &sim->nutrients[ECOL_NUTRIENT_N_NITRATE].amount;
    float* n_org = &sim->nutrients[ECOL_NUTRIENT_N_ORGANIC].amount;

    /* Nitrogen fixation: N2 -> NH4+ (biological, by Rhizobium etc.) */
    float fixation = ECOL_N_FIXATION_RATE * dt;
    *n_atm -= fixation;
    *n_nh4 += fixation;

    /* Nitrification: NH4+ -> NO2- -> NO3- (by Nitrosomonas, Nitrobacter) */
    float nitrification = ECOL_NITRIFICATION_RATE * *n_nh4 * dt;
    nitrification = clampf(nitrification, 0.0f, *n_nh4);
    *n_nh4 -= nitrification;
    *n_no3 += nitrification;

    /* Plant uptake: NO3- -> organic N */
    float producer_demand = 0.0f;
    for (uint32_t i = 0; i < sim->num_species; i++) {
        if (sim->species[i].active && sim->species[i].trophic_level == ECOL_TROPHIC_PRODUCER) {
            producer_demand += sim->species[i].abundance * 0.001f;
        }
    }
    float uptake = ECOL_N_UPTAKE_RATE * (*n_no3) * clampf(producer_demand, 0.0f, 1.0f) * dt;
    uptake = clampf(uptake, 0.0f, *n_no3);
    *n_no3 -= uptake;
    *n_org += uptake;

    /* Mineralization: organic N -> NH4+ (decomposition) */
    float mineralization = ECOL_MINERALIZATION_RATE * *n_org * dt;
    mineralization = clampf(mineralization, 0.0f, *n_org);
    *n_org -= mineralization;
    *n_nh4 += mineralization;

    /* Denitrification: NO3- -> N2 (anaerobic bacteria) */
    float denitrification = ECOL_DENITRIFICATION_RATE * *n_no3 * dt;
    denitrification = clampf(denitrification, 0.0f, *n_no3);
    *n_no3 -= denitrification;
    *n_atm += denitrification;

    return 0;
}

int ecology_step_phosphorus_cycle(ecology_sim_t* sim, float dt) {
    if (!sim) return -1;

    float* p_dis = &sim->nutrients[ECOL_NUTRIENT_P_DISSOLVED].amount;
    float* p_org = &sim->nutrients[ECOL_NUTRIENT_P_ORGANIC].amount;

    /* Weathering: rock -> dissolved P (very slow) */
    *p_dis += ECOL_P_WEATHERING_RATE * dt;

    /* Plant uptake: dissolved -> organic */
    float uptake = ECOL_P_UPTAKE_RATE * *p_dis * dt;
    uptake = clampf(uptake, 0.0f, *p_dis);
    *p_dis -= uptake;
    *p_org += uptake;

    /* Decomposition: organic -> dissolved */
    float decomp = ECOL_P_DECOMP_RATE * *p_org * dt;
    decomp = clampf(decomp, 0.0f, *p_org);
    *p_org -= decomp;
    *p_dis += decomp;

    /* Sedimentation: dissolved -> sediment (loss from system) */
    float sed = ECOL_P_SEDIMENTATION_RATE * *p_dis * dt;
    *p_dis -= clampf(sed, 0.0f, *p_dis);

    return 0;
}

int ecology_step_carbon_cycle(ecology_sim_t* sim, float dt) {
    if (!sim) return -1;

    float* c_atm = &sim->nutrients[ECOL_NUTRIENT_C_ATMOSPHERIC].amount;
    float* c_org = &sim->nutrients[ECOL_NUTRIENT_C_ORGANIC].amount;

    /* Photosynthesis: CO2 -> organic C */
    float photo = ECOL_C_PHOTOSYNTHESIS_RATE * (*c_atm / 420.0f) * dt;
    *c_atm -= photo;
    *c_org += photo;

    /* Respiration: organic C -> CO2 (all organisms) */
    float total_biomass = 0.0f;
    for (uint32_t i = 0; i < sim->num_species; i++) {
        if (sim->species[i].active) {
            total_biomass += sim->species[i].abundance * sim->species[i].biomass_per_individual;
        }
    }
    float resp = ECOL_C_RESPIRATION_RATE * (total_biomass / (total_biomass + 100.0f)) * dt;
    float actual_resp = clampf(resp, 0.0f, *c_org);
    *c_org -= actual_resp;
    *c_atm += actual_resp;

    /* Decomposition: dead organic -> CO2 */
    float decomp = ECOL_C_DECOMPOSITION_RATE * *c_org * 0.1f * dt;
    decomp = clampf(decomp, 0.0f, *c_org);
    *c_org -= decomp;
    *c_atm += decomp;

    return 0;
}

/* ============================================================================
 * Island Biogeography
 * ============================================================================ */

/**
 * Species-area relationship: S = c * A^z
 * MacArthur & Wilson (1967).
 */
float ecology_species_area(float c, float area, float z) {
    if (area <= 0.0f || c <= 0.0f) return 0.0f;
    return c * powf(area, z);
}

int ecology_step_island_biogeography(ecology_sim_t* sim, uint32_t island_idx, float dt) {
    if (!sim || island_idx >= sim->num_islands) return -1;
    ecol_island_t* isle = &sim->islands[island_idx];

    float P = isle->mainland_pool_size;
    if (P <= 0.0f) return 0;

    /* Immigration rate: I = I0 * (1 - S/P) */
    /* Decreases as island fills up */
    float I0 = isle->immigration_rate;
    float imm = I0 * (1.0f - isle->species_count / P);
    imm = maxf(imm, 0.0f);

    /* Extinction rate: E = E0 * (S/P) */
    /* Increases with more species (competition) */
    float E0 = isle->extinction_rate;
    float ext = E0 * (isle->species_count / P);
    ext = maxf(ext, 0.0f);

    /* Distance effect: immigration decreases with distance */
    float dist_factor = expf(-0.01f * isle->distance_to_mainland_km);
    imm *= dist_factor;

    /* Area effect: extinction decreases with area */
    float area_factor = 1.0f / (1.0f + 0.1f * isle->area_km2);
    ext *= area_factor;

    /* Update species count: dS/dt = immigration - extinction */
    isle->species_count += (imm - ext) * dt;
    isle->species_count = clampf(isle->species_count, 0.0f, P);

    return 0;
}

/* ============================================================================
 * Succession
 * ============================================================================ */

int ecology_step_succession(ecology_sim_t* sim, float dt) {
    if (!sim || !sim->config.enable_succession) return 0;
    ecol_succession_state_t* succ = &sim->succession;

    float dt_yr = dt / 365.0f; /* days to years */
    succ->time_in_stage_yr += dt_yr;

    /* Soil accumulation */
    succ->soil_depth_cm += 0.1f * dt_yr; /* ~0.1 cm/year */

    /* Stage transitions based on time thresholds */
    bool advance = false;
    switch (succ->stage) {
        case ECOL_SUCCESSION_BARE:
            if (succ->time_in_stage_yr > 1.0f) advance = true;
            break;
        case ECOL_SUCCESSION_PIONEER:
            if (succ->time_in_stage_yr > ECOL_SUCCESSION_PIONEER_YR) advance = true;
            break;
        case ECOL_SUCCESSION_EARLY:
            if (succ->time_in_stage_yr > ECOL_SUCCESSION_EARLY_YR) advance = true;
            break;
        case ECOL_SUCCESSION_MID:
            if (succ->time_in_stage_yr > ECOL_SUCCESSION_MID_YR) advance = true;
            break;
        case ECOL_SUCCESSION_LATE:
            if (succ->time_in_stage_yr > ECOL_SUCCESSION_LATE_YR) advance = true;
            break;
        case ECOL_SUCCESSION_CLIMAX:
            /* Stable state */
            break;
        default:
            break;
    }

    if (advance && succ->stage < ECOL_SUCCESSION_CLIMAX) {
        succ->stage++;
        succ->time_in_stage_yr = 0.0f;
    }

    /* Update metrics based on stage */
    float stage_f = (float)succ->stage / (float)(ECOL_SUCCESSION_STAGE_COUNT - 1);
    succ->species_richness = 2.0f + stage_f * 50.0f; /* 2 -> 52 species */
    succ->total_biomass = stage_f * stage_f * 500.0f; /* accelerating biomass */
    succ->canopy_cover = clampf(stage_f * 1.2f, 0.0f, 1.0f);

    return 0;
}

/* ============================================================================
 * Food Web Stability (May's Criterion)
 * ============================================================================ */

/**
 * May's stability criterion: sqrt(S * C) * sigma < 1
 * S = number of species, C = connectance, sigma = mean interaction strength.
 * Returns the stability metric; < 1 means stable.
 */
float ecology_may_stability(uint32_t S, float C, float sigma) {
    if (S == 0) return 0.0f;
    return sqrtf((float)S * C) * sigma;
}

/* ============================================================================
 * Competition (Lotka-Volterra)
 * ============================================================================ */

int ecology_step_competition(ecology_sim_t* sim, float dt) {
    if (!sim) return -1;

    /* Lotka-Volterra competition: dN_i/dt = r_i * N_i * (K_i - N_i - sum(alpha_ij * N_j)) / K_i */
    float dN[ECOL_MAX_SPECIES];
    memset(dN, 0, sizeof(dN));

    for (uint32_t i = 0; i < sim->num_species; i++) {
        ecol_species_t* sp = &sim->species[i];
        if (!sp->active || sp->abundance <= 0.0f) continue;

        float competition_sum = 0.0f;
        for (uint32_t l = 0; l < sim->num_links; l++) {
            ecol_food_web_link_t* link = &sim->links[l];
            if (link->type != ECOL_INTERACTION_COMPETITION) continue;

            if (link->species_a == i && link->species_b < sim->num_species) {
                competition_sum += link->alpha * sim->species[link->species_b].abundance;
            } else if (link->species_b == i && link->species_a < sim->num_species) {
                competition_sum += link->alpha * sim->species[link->species_a].abundance;
            }
        }

        /* Predation: trophic interactions */
        float predation_loss = 0.0f;
        float predation_gain = 0.0f;
        for (uint32_t l = 0; l < sim->num_links; l++) {
            ecol_food_web_link_t* link = &sim->links[l];
            if (link->type != ECOL_INTERACTION_PREDATION) continue;

            /* species_a eats species_b */
            if (link->species_b == i && link->species_a < sim->num_species) {
                predation_loss += link->strength * sim->species[link->species_a].abundance;
            }
            if (link->species_a == i && link->species_b < sim->num_species) {
                predation_gain += link->strength * sim->species[link->species_b].abundance *
                                  sim->config.trophic_efficiency;
            }
        }

        /* dN/dt = r * N * (K - N - competition) / K - predation_loss + predation_gain */
        float logistic = sp->growth_rate * sp->abundance *
            (sp->carrying_capacity - sp->abundance - competition_sum) /
            (sp->carrying_capacity + 1e-6f);

        dN[i] = logistic - predation_loss * sp->abundance * 0.001f +
                predation_gain * sp->abundance * 0.001f;
    }

    /* Apply changes */
    for (uint32_t i = 0; i < sim->num_species; i++) {
        sim->species[i].abundance += dN[i] * dt;
        if (sim->species[i].abundance < 1.0f) {
            sim->species[i].abundance = 0.0f;
            sim->species[i].active = false;
        }
    }

    return 0;
}

/* ============================================================================
 * Main Step
 * ============================================================================ */

int ecology_step(ecology_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;

    /* Competition and predation */
    ecology_step_competition(sim, dt);

    /* Nutrient cycling */
    if (sim->config.enable_nutrient_cycling) {
        ecology_step_nitrogen_cycle(sim, dt);
        ecology_step_phosphorus_cycle(sim, dt);
        ecology_step_carbon_cycle(sim, dt);
    }

    /* Succession */
    if (sim->config.enable_succession) {
        ecology_step_succession(sim, dt);
    }

    /* Island biogeography */
    if (sim->config.enable_biogeography) {
        for (uint32_t i = 0; i < sim->num_islands; i++) {
            ecology_step_island_biogeography(sim, i, dt);
        }
    }

    /* Energy pyramid */
    ecology_compute_energy_pyramid(sim);

    /* Diversity */
    sim->diversity = ecology_compute_diversity(sim);

    /* Update stats */
    sim->stats.step_count++;
    sim->stats.shannon_diversity = sim->diversity.shannon_h;
    sim->stats.simpson_diversity = sim->diversity.simpson_d;
    sim->stats.species_count = (uint32_t)sim->diversity.species_richness;

    float total_biomass = 0.0f;
    float total_energy = 0.0f;
    for (int i = 0; i < ECOL_TROPHIC_LEVEL_COUNT; i++) {
        total_energy += sim->energy_pyramid[i];
    }
    for (uint32_t i = 0; i < sim->num_species; i++) {
        if (sim->species[i].active) {
            total_biomass += sim->species[i].abundance * sim->species[i].biomass_per_individual;
        }
    }
    sim->stats.total_biomass = total_biomass;
    sim->stats.total_energy = total_energy;
    sim->stats.succession_stage = sim->succession.stage;

    sim->stats.n_total = sim->nutrients[ECOL_NUTRIENT_N_AMMONIUM].amount +
                          sim->nutrients[ECOL_NUTRIENT_N_NITRATE].amount +
                          sim->nutrients[ECOL_NUTRIENT_N_ORGANIC].amount;
    sim->stats.p_total = sim->nutrients[ECOL_NUTRIENT_P_DISSOLVED].amount +
                          sim->nutrients[ECOL_NUTRIENT_P_ORGANIC].amount;
    sim->stats.c_total = sim->nutrients[ECOL_NUTRIENT_C_ATMOSPHERIC].amount +
                          sim->nutrients[ECOL_NUTRIENT_C_ORGANIC].amount;

    /* Food web stability */
    float connectance = (sim->num_species > 1) ?
        (float)sim->num_links / ((float)sim->num_species * ((float)sim->num_species - 1.0f) / 2.0f) :
        0.0f;
    float mean_strength = 0.0f;
    for (uint32_t i = 0; i < sim->num_links; i++) {
        mean_strength += sim->links[i].strength;
    }
    if (sim->num_links > 0) mean_strength /= sim->num_links;
    sim->stats.food_web_stability = ecology_may_stability(sim->num_species, connectance, mean_strength);

    return 0;
}

/* ============================================================================
 * Preset: Temperate Forest Ecosystem
 * ============================================================================ */

void ecology_load_temperate_forest(ecology_sim_t* sim) {
    if (!sim) return;

    sim->num_species = 0;
    sim->num_links = 0;

    /* Producers */
    ecol_species_t oak = {0};
    strncpy(oak.name, "Oak trees", ECOL_MAX_NAME_LEN - 1);
    oak.trophic_level = ECOL_TROPHIC_PRODUCER;
    oak.abundance = 500.0f; oak.carrying_capacity = 800.0f;
    oak.growth_rate = 0.01f; oak.biomass_per_individual = 500.0f;
    oak.metabolic_rate = 0.5f; oak.niche_position = 0.5f;
    oak.active = true;
    ecology_add_species(sim, &oak);

    ecol_species_t grass = {0};
    strncpy(grass.name, "Grasses", ECOL_MAX_NAME_LEN - 1);
    grass.trophic_level = ECOL_TROPHIC_PRODUCER;
    grass.abundance = 10000.0f; grass.carrying_capacity = 20000.0f;
    grass.growth_rate = 0.1f; grass.biomass_per_individual = 0.1f;
    grass.metabolic_rate = 0.01f; grass.niche_position = 0.3f;
    grass.active = true;
    ecology_add_species(sim, &grass);

    /* Primary consumers */
    ecol_species_t deer = {0};
    strncpy(deer.name, "White-tail deer", ECOL_MAX_NAME_LEN - 1);
    deer.trophic_level = ECOL_TROPHIC_PRIMARY_CONSUMER;
    deer.abundance = 200.0f; deer.carrying_capacity = 500.0f;
    deer.growth_rate = 0.05f; deer.biomass_per_individual = 80.0f;
    deer.metabolic_rate = 2.0f; deer.active = true;
    ecology_add_species(sim, &deer);

    ecol_species_t rabbit = {0};
    strncpy(rabbit.name, "Rabbits", ECOL_MAX_NAME_LEN - 1);
    rabbit.trophic_level = ECOL_TROPHIC_PRIMARY_CONSUMER;
    rabbit.abundance = 1000.0f; rabbit.carrying_capacity = 3000.0f;
    rabbit.growth_rate = 0.15f; rabbit.biomass_per_individual = 2.0f;
    rabbit.metabolic_rate = 0.3f; rabbit.active = true;
    ecology_add_species(sim, &rabbit);

    /* Secondary consumer */
    ecol_species_t fox = {0};
    strncpy(fox.name, "Red fox", ECOL_MAX_NAME_LEN - 1);
    fox.trophic_level = ECOL_TROPHIC_SECONDARY_CONSUMER;
    fox.abundance = 50.0f; fox.carrying_capacity = 150.0f;
    fox.growth_rate = 0.03f; fox.biomass_per_individual = 6.0f;
    fox.metabolic_rate = 1.5f; fox.active = true;
    ecology_add_species(sim, &fox);

    /* Tertiary consumer */
    ecol_species_t wolf = {0};
    strncpy(wolf.name, "Grey wolf", ECOL_MAX_NAME_LEN - 1);
    wolf.trophic_level = ECOL_TROPHIC_TERTIARY_CONSUMER;
    wolf.abundance = 15.0f; wolf.carrying_capacity = 40.0f;
    wolf.growth_rate = 0.02f; wolf.biomass_per_individual = 40.0f;
    wolf.metabolic_rate = 3.0f; wolf.active = true;
    ecology_add_species(sim, &wolf);

    /* Decomposer */
    ecol_species_t fungi = {0};
    strncpy(fungi.name, "Decomposer fungi", ECOL_MAX_NAME_LEN - 1);
    fungi.trophic_level = ECOL_TROPHIC_DECOMPOSER;
    fungi.abundance = 5000.0f; fungi.carrying_capacity = 10000.0f;
    fungi.growth_rate = 0.08f; fungi.biomass_per_individual = 0.01f;
    fungi.metabolic_rate = 0.005f; fungi.active = true;
    ecology_add_species(sim, &fungi);

    /* Food web links */
    ecol_food_web_link_t link = {0};

    /* Herbivory: deer eat grass, rabbits eat grass */
    link.type = ECOL_INTERACTION_PREDATION; link.strength = 0.02f;
    link.species_a = 2; link.species_b = 1; /* deer eat grass */
    ecology_add_link(sim, &link);
    link.species_a = 3; link.species_b = 1; link.strength = 0.03f; /* rabbits eat grass */
    ecology_add_link(sim, &link);

    /* Predation: fox eats rabbits, wolf eats deer */
    link.species_a = 4; link.species_b = 3; link.strength = 0.04f; /* fox eats rabbit */
    ecology_add_link(sim, &link);
    link.species_a = 5; link.species_b = 2; link.strength = 0.03f; /* wolf eats deer */
    ecology_add_link(sim, &link);
    link.species_a = 5; link.species_b = 4; link.strength = 0.01f; /* wolf eats fox */
    ecology_add_link(sim, &link);

    /* Competition: deer and rabbits for grass */
    link.type = ECOL_INTERACTION_COMPETITION;
    link.species_a = 2; link.species_b = 3; link.alpha = 0.3f; link.strength = 0.01f;
    ecology_add_link(sim, &link);

    sim->succession.stage = ECOL_SUCCESSION_LATE;
    sim->succession.canopy_cover = 0.8f;
    sim->succession.soil_depth_cm = 50.0f;

    LOG_INFO(LOG_TAG, "Loaded temperate forest: 7 species, 6 links");
}
