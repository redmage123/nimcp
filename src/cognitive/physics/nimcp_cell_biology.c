/**
 * @file nimcp_cell_biology.c
 * @brief Cell Biology simulation engine -- cell cycle, membrane transport,
 *        signal transduction, mitosis/meiosis, apoptosis
 *
 * WHAT: Simulates cellular processes with real biology equations and constants.
 * WHY:  Cell biology prior for world model reasoning about living systems.
 * HOW:  Phase-based cell cycle with CDK/cyclin checkpoints, Fick's law,
 *       Michaelis-Menten transport, van't Hoff osmotic pressure, caspase cascade.
 */

#include "cognitive/physics/nimcp_cell_biology.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "CELL_BIOLOGY"

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/**
 * Simple linear congruential PRNG for stochastic processes (meiosis crossover).
 */
static float cell_random(uint32_t* state) {
    *state = *state * 1664525u + 1013904223u;
    return (float)(*state & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

/* ============================================================================
 * Create / Destroy / Config
 * ============================================================================ */

cell_biology_config_t cell_biology_default_config(void) {
    cell_biology_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dt = 0.1f;                  /* 0.1 hours = 6 minutes */
    cfg.temperature_c = 37.0f;
    cfg.enable_apoptosis = true;
    cfg.enable_signal_transduction = true;
    cfg.enable_meiosis = false;
    cfg.growth_factor_conc = 0.5f;
    cfg.nutrient_level = 0.8f;
    cfg.oxygen_level = 0.95f;
    return cfg;
}

cell_biology_sim_t* cell_biology_create(const cell_biology_config_t* config) {
    cell_biology_sim_t* sim = (cell_biology_sim_t*)nimcp_calloc(1, sizeof(cell_biology_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate cell biology sim");
        return NULL;
    }
    sim->config = config ? *config : cell_biology_default_config();
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Cell biology sim created (dt=%.2fh, T=%.1fC)",
             sim->config.dt, sim->config.temperature_c);
    return sim;
}

void cell_biology_destroy(cell_biology_sim_t* sim) {
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Cell biology sim destroyed after %lu steps", sim->stats.step_count);
    nimcp_free(sim);
}

cell_biology_stats_t cell_biology_get_stats(const cell_biology_sim_t* sim) {
    cell_biology_stats_t empty;
    memset(&empty, 0, sizeof(empty));
    return sim ? sim->stats : empty;
}

/* ============================================================================
 * Cell Management
 * ============================================================================ */

int cell_biology_add_cell(cell_biology_sim_t* sim, const cell_sim_cell_t* cell) {
    if (!sim || !cell) return -1;
    if (sim->num_cells >= CELL_MAX_CELLS) {
        LOG_WARN(LOG_TAG, "Max cells reached (%d)", CELL_MAX_CELLS);
        return -1;
    }
    sim->cells[sim->num_cells] = *cell;
    sim->cells[sim->num_cells].id = sim->num_cells;
    sim->num_cells++;
    return 0;
}

/* ============================================================================
 * Cell Cycle -- CDK/Cyclin checkpoint logic
 * ============================================================================ */

/**
 * Update cyclin levels based on current cell cycle phase.
 * Real biology: cyclins accumulate and degrade in phase-specific patterns.
 */
static void update_cyclin_levels(cell_sim_cell_t* cell, float dt, float growth_factor) {
    cell_cycle_regulators_t* r = &cell->regulators;
    float progress = cell->phase_progress;

    switch (cell->phase) {
        case CELL_PHASE_G1:
            /* CyclinD accumulates in response to growth factors */
            r->cyclin_d += growth_factor * 0.1f * dt;
            r->cyclin_d = clampf(r->cyclin_d, 0.0f, 1.0f);
            /* CyclinE rises late G1 for S-phase entry */
            if (progress > 0.7f) {
                r->cyclin_e += 0.15f * dt;
                r->cyclin_e = clampf(r->cyclin_e, 0.0f, 1.0f);
            }
            /* Rb phosphorylation releases E2F */
            r->rb = clampf(1.0f - r->cyclin_d, 0.0f, 1.0f);
            break;

        case CELL_PHASE_S:
            /* CyclinA accumulates for G2 transition */
            r->cyclin_a += 0.12f * dt;
            r->cyclin_a = clampf(r->cyclin_a, 0.0f, 1.0f);
            /* CyclinE degrades after S-phase initiation */
            r->cyclin_e *= (1.0f - 0.2f * dt);
            break;

        case CELL_PHASE_G2:
            /* CyclinB (MPF component) accumulates for mitosis */
            r->cyclin_b += 0.2f * dt;
            r->cyclin_b = clampf(r->cyclin_b, 0.0f, 1.0f);
            /* CyclinA maintained */
            break;

        case CELL_PHASE_M_PROPHASE:
        case CELL_PHASE_M_METAPHASE:
        case CELL_PHASE_M_ANAPHASE:
            /* CyclinB active, drives mitosis */
            break;

        case CELL_PHASE_M_TELOPHASE:
        case CELL_PHASE_CYTOKINESIS:
            /* APC/C triggers cyclinB destruction */
            r->cyclin_b *= (1.0f - 0.5f * dt);
            r->cyclin_a *= (1.0f - 0.5f * dt);
            break;

        default:
            break;
    }

    /* p53 response to DNA damage */
    if (cell->dna_integrity < 0.9f) {
        r->p53 += 0.3f * (1.0f - cell->dna_integrity) * dt;
        r->p53 = clampf(r->p53, 0.0f, 1.0f);
        /* p21 CDK inhibitor induced by p53 */
        r->p21 += r->p53 * 0.2f * dt;
        r->p21 = clampf(r->p21, 0.0f, 1.0f);
    } else {
        r->p53 *= (1.0f - 0.1f * dt);
        r->p21 *= (1.0f - 0.15f * dt);
    }
}

bool cell_biology_check_checkpoint(const cell_biology_sim_t* sim, uint32_t cell_idx) {
    if (!sim || cell_idx >= sim->num_cells) return false;
    const cell_sim_cell_t* cell = &sim->cells[cell_idx];
    const cell_cycle_regulators_t* r = &cell->regulators;

    /* CDK inhibitor p21 blocks all transitions */
    if (r->p21 > 0.5f) return false;

    switch (cell->phase) {
        case CELL_PHASE_G1:
            /* Restriction point: CyclinD/CDK4 must exceed threshold, Rb phosphorylated */
            return r->cyclin_d >= CELL_G1_CYCLIN_D_THRESHOLD &&
                   r->rb < 0.4f && cell->dna_integrity > 0.95f;

        case CELL_PHASE_S:
            /* S/G2 transition: DNA replication complete */
            return r->cyclin_a >= CELL_G2_CYCLIN_A_THRESHOLD &&
                   cell->phase_progress >= 0.95f;

        case CELL_PHASE_G2:
            /* G2/M checkpoint: CyclinB/CDK1 (MPF) activated, no DNA damage */
            return r->cyclin_b >= CELL_M_CYCLIN_B_THRESHOLD &&
                   cell->dna_integrity > 0.98f;

        case CELL_PHASE_M_METAPHASE:
            /* Spindle assembly checkpoint: all kinetochores attached */
            return cell->phase_progress >= 0.9f;

        default:
            return true; /* other phases auto-progress */
    }
}

int cell_biology_advance_cycle(cell_biology_sim_t* sim, uint32_t cell_idx, float dt) {
    if (!sim || cell_idx >= sim->num_cells) return -1;
    cell_sim_cell_t* cell = &sim->cells[cell_idx];
    if (!cell->alive || cell->phase == CELL_PHASE_APOPTOSIS) return 0;

    /* Phase duration determines progress rate */
    float phase_duration = CELL_PHASE_G1_DURATION;
    switch (cell->phase) {
        case CELL_PHASE_G1: phase_duration = CELL_PHASE_G1_DURATION; break;
        case CELL_PHASE_S:  phase_duration = CELL_PHASE_S_DURATION;  break;
        case CELL_PHASE_G2: phase_duration = CELL_PHASE_G2_DURATION; break;
        case CELL_PHASE_M_PROPHASE:
        case CELL_PHASE_M_METAPHASE:
        case CELL_PHASE_M_ANAPHASE:
        case CELL_PHASE_M_TELOPHASE:
        case CELL_PHASE_CYTOKINESIS:
            phase_duration = CELL_PHASE_M_DURATION / 5.0f; /* each M sub-phase */
            break;
        default:
            return 0;
    }

    /* ATP and nutrients affect cycle speed */
    float rate_factor = cell->atp_level * sim->config.nutrient_level;
    rate_factor = clampf(rate_factor, 0.1f, 1.5f);

    cell->phase_progress += (dt / phase_duration) * rate_factor;
    cell->age_hours += dt;

    /* Update cyclins */
    update_cyclin_levels(cell, dt, sim->config.growth_factor_conc);

    /* Check transition */
    if (cell->phase_progress >= 1.0f && cell_biology_check_checkpoint(sim, cell_idx)) {
        cell->phase_progress = 0.0f;
        /* Advance to next phase */
        if (cell->phase == CELL_PHASE_CYTOKINESIS) {
            cell->phase = CELL_PHASE_G1;
            sim->stats.divisions_completed++;
        } else if (cell->phase < CELL_PHASE_CYTOKINESIS) {
            cell->phase++;
        }
    } else if (cell->phase_progress > 1.0f) {
        cell->phase_progress = 1.0f; /* held at checkpoint */
    }

    /* DNA replication during S-phase */
    if (cell->phase == CELL_PHASE_S) {
        /* Chromosome duplication progresses with phase */
        if (cell->phase_progress > 0.9f && cell->is_diploid) {
            cell->chromosome_count = 92; /* 4n: replicated before segregation */
        }
    }

    return 0;
}

/* ============================================================================
 * Mitosis / Meiosis
 * ============================================================================ */

int cell_biology_mitosis(cell_biology_sim_t* sim, uint32_t cell_idx) {
    if (!sim || cell_idx >= sim->num_cells) return -1;
    if (sim->num_cells >= CELL_MAX_CELLS - 1) return -1;
    cell_sim_cell_t* parent = &sim->cells[cell_idx];

    /* Create daughter cell as copy */
    cell_sim_cell_t daughter = *parent;
    daughter.id = sim->num_cells;
    daughter.phase = CELL_PHASE_G1;
    daughter.phase_progress = 0.0f;
    daughter.volume_um3 = parent->volume_um3 * 0.5f; /* half volume */
    daughter.chromosome_count = 46; /* diploid restored */
    daughter.age_hours = 0.0f;

    /* Reset cyclins in both */
    memset(&daughter.regulators, 0, sizeof(cell_cycle_regulators_t));
    memset(&parent->regulators, 0, sizeof(cell_cycle_regulators_t));

    parent->volume_um3 *= 0.5f;
    parent->phase = CELL_PHASE_G1;
    parent->phase_progress = 0.0f;
    parent->chromosome_count = 46;

    sim->cells[sim->num_cells] = daughter;
    sim->num_cells++;
    sim->stats.divisions_completed++;

    return 0;
}

int cell_biology_meiosis(cell_biology_sim_t* sim, uint32_t cell_idx,
                         float crossover_rate) {
    if (!sim || cell_idx >= sim->num_cells) return -1;
    if (sim->num_cells >= CELL_MAX_CELLS - 3) return -1; /* need room for 4 gametes */
    cell_sim_cell_t* parent = &sim->cells[cell_idx];

    uint32_t rng_state = (uint32_t)(parent->age_hours * 1000.0f + parent->id * 7919);

    /* Meiosis I: homologous separation -> 2 haploid cells */
    /* Meiosis II: sister chromatid separation -> 4 gametes */
    for (int g = 0; g < 3; g++) { /* 3 additional gametes (parent becomes 4th) */
        if (sim->num_cells >= CELL_MAX_CELLS) break;

        cell_sim_cell_t gamete;
        memset(&gamete, 0, sizeof(gamete));
        gamete.id = sim->num_cells;
        gamete.phase = CELL_PHASE_G0;
        gamete.alive = true;
        gamete.is_diploid = false;
        gamete.chromosome_count = 23; /* haploid */
        gamete.volume_um3 = parent->volume_um3 * 0.25f;
        gamete.atp_level = parent->atp_level * 0.5f;
        gamete.membrane_potential_mv = -70.0f;
        gamete.dna_integrity = 1.0f;

        /* Independent assortment: random allele selection */
        /* Crossing over: probability of recombination per chromosome */
        float recomb = cell_random(&rng_state);
        if (recomb < crossover_rate) {
            gamete.dna_integrity = 0.99f; /* slight mark for recombined */
        }

        sim->cells[sim->num_cells] = gamete;
        sim->num_cells++;
    }

    /* Convert parent to 4th gamete */
    parent->is_diploid = false;
    parent->chromosome_count = 23;
    parent->volume_um3 *= 0.25f;
    parent->phase = CELL_PHASE_G0;

    return 0;
}

/* ============================================================================
 * Membrane Transport
 * ============================================================================ */

int cell_biology_add_channel(cell_biology_sim_t* sim,
                             const cell_transport_channel_t* ch) {
    if (!sim || !ch) return -1;
    if (sim->num_channels >= CELL_MAX_CHANNELS) return -1;
    sim->channels[sim->num_channels] = *ch;
    sim->num_channels++;
    return 0;
}

/**
 * Fick's first law of diffusion: J = -D * dC/dx
 * Returns flux in mol/(m^2*s)
 */
float cell_biology_fick_diffusion(float D, float dC, float dx) {
    if (dx < 1e-12f) return 0.0f;
    return -D * (dC / dx);
}

/**
 * Michaelis-Menten kinetics for facilitated transport:
 * v = Vmax * [S] / (Km + [S])
 */
float cell_biology_facilitated_transport(float vmax, float km, float substrate) {
    if (substrate < 0.0f) substrate = 0.0f;
    return vmax * substrate / (km + substrate);
}

/**
 * van't Hoff equation for osmotic pressure:
 * pi = i * M * R * T
 * where i = van't Hoff factor, M = molarity, R = gas constant, T = temperature (K)
 */
float cell_biology_osmotic_pressure(float i_factor, float molarity, float temp_k) {
    return i_factor * molarity * CELL_GAS_CONSTANT * temp_k;
}

int cell_biology_step_transport(cell_biology_sim_t* sim, uint32_t cell_idx, float dt) {
    if (!sim || cell_idx >= sim->num_cells) return -1;
    cell_sim_cell_t* cell = &sim->cells[cell_idx];
    float total_flux = 0.0f;

    float dt_sec = dt * 3600.0f; /* hours to seconds */
    float dx = CELL_MEMBRANE_THICKNESS_NM * 1e-9f; /* nm to m */
    float temp_k = sim->config.temperature_c + 273.15f;

    for (uint32_t i = 0; i < sim->num_channels; i++) {
        cell_transport_channel_t* ch = &sim->channels[i];
        if (!ch->open) continue;

        float flux = 0.0f;

        switch (ch->type) {
            case CELL_TRANSPORT_PASSIVE_DIFFUSION: {
                /* Fick's law: J = -D * dC/dx */
                float dC = cell->extracellular_na - cell->intracellular_na; /* example: Na+ */
                flux = cell_biology_fick_diffusion(ch->diffusion_coeff, dC, dx);
                break;
            }
            case CELL_TRANSPORT_FACILITATED: {
                /* Michaelis-Menten */
                float substrate = cell->extracellular_k; /* example: glucose transporter */
                flux = cell_biology_facilitated_transport(ch->vmax, ch->km, substrate);
                break;
            }
            case CELL_TRANSPORT_ACTIVE_PRIMARY: {
                /* Na+/K+ ATPase: 3 Na+ out, 2 K+ in per ATP */
                if (cell->atp_level > 0.1f) {
                    flux = ch->vmax * cell->atp_level;
                    cell->intracellular_na -= 3.0f * flux * dt_sec * 0.001f;
                    cell->intracellular_k  += 2.0f * flux * dt_sec * 0.001f;
                    cell->atp_level -= ch->atp_cost * fabsf(flux) * dt_sec * 0.0001f;
                    cell->atp_level = clampf(cell->atp_level, 0.0f, 1.0f);
                }
                break;
            }
            case CELL_TRANSPORT_OSMOSIS: {
                /* van't Hoff: water follows solute gradient */
                float int_osm = cell->intracellular_na + cell->intracellular_k;
                float ext_osm = cell->extracellular_na + cell->extracellular_k;
                float pi_diff = cell_biology_osmotic_pressure(1.0f,
                    (ext_osm - int_osm) * 0.001f, temp_k);
                /* Volume change proportional to osmotic pressure difference */
                cell->volume_um3 += pi_diff * 0.001f * dt_sec;
                flux = pi_diff;
                break;
            }
            default:
                break;
        }

        ch->current_flux = flux;
        total_flux += fabsf(flux);
    }

    /* Update membrane potential using Goldman-like approximation */
    float pk_pi = cell->intracellular_k / (cell->extracellular_k + 0.001f);
    float pna_ratio = cell->extracellular_na / (cell->intracellular_na + 0.001f);
    float log_arg = pk_pi * 0.95f + pna_ratio * 0.05f;
    if (log_arg < 1e-10f) log_arg = 1e-10f;  /* prevent log10f(<=0) */
    cell->membrane_potential_mv = -61.5f * log10f(log_arg);
    cell->membrane_potential_mv = clampf(cell->membrane_potential_mv, -90.0f, 40.0f);

    sim->stats.total_membrane_flux += total_flux;
    return 0;
}

/* ============================================================================
 * Signal Transduction
 * ============================================================================ */

int cell_biology_add_signal(cell_biology_sim_t* sim,
                            const cell_signal_pathway_t* sig) {
    if (!sim || !sig) return -1;
    if (sim->num_signals >= CELL_MAX_SIGNALS) return -1;
    sim->signals[sim->num_signals] = *sig;
    sim->num_signals++;
    return 0;
}

int cell_biology_step_signals(cell_biology_sim_t* sim, float dt) {
    if (!sim) return -1;

    float max_amp = 0.0f;

    for (uint32_t i = 0; i < sim->num_signals; i++) {
        cell_signal_pathway_t* sig = &sim->signals[i];

        /* Receptor occupancy: simple binding kinetics */
        /* Occupancy = [L] / (Kd + [L]), assume Kd = 10 nM */
        float kd = 10.0f; /* nM */
        sig->receptor_occupancy = sig->ligand_concentration /
                                  (kd + sig->ligand_concentration);

        /* Signal cascade: each step amplifies ~1000x */
        float signal = sig->receptor_occupancy;
        for (uint32_t step = 0; step < sig->cascade_depth && step < CELL_MAX_CASCADE_STEPS; step++) {
            /* Amplification with saturation (Hill-like) */
            float amplified = signal * CELL_SIGNAL_AMPLIFICATION;
            sig->kinase_activity[step] = amplified / (1.0f + amplified);
            signal = sig->kinase_activity[step];
        }

        sig->response_magnitude = signal;

        /* Second messenger dynamics (cAMP for GPCR) */
        if (sig->type == CELL_SIGNAL_GPCR) {
            sig->second_messenger += sig->receptor_occupancy * 5.0f * dt;
            sig->second_messenger *= expf(-sig->decay_rate * dt);
            sig->second_messenger = clampf(sig->second_messenger, 0.0f, 100.0f);
        }

        /* Signal decay */
        sig->ligand_concentration *= expf(-sig->decay_rate * 0.1f * dt);

        if (sig->response_magnitude > max_amp) {
            max_amp = sig->response_magnitude;
        }
    }

    sim->stats.max_signal_amplitude = max_amp;
    return 0;
}

/* ============================================================================
 * Apoptosis -- Caspase Cascade
 * ============================================================================ */

int cell_biology_step_apoptosis(cell_biology_sim_t* sim, uint32_t cell_idx, float dt) {
    if (!sim || cell_idx >= sim->num_cells) return -1;
    if (!sim->config.enable_apoptosis) return 0;

    cell_sim_cell_t* cell = &sim->cells[cell_idx];
    cell_apoptosis_state_t* apop = &sim->apoptosis;

    /* DNA damage and p53 drive pro-apoptotic Bax */
    float damage_signal = (1.0f - cell->dna_integrity) + cell->regulators.p53 * 0.5f;
    apop->bax += damage_signal * 0.1f * dt;
    apop->bax = clampf(apop->bax, 0.0f, 1.0f);

    /* Bcl-2 anti-apoptotic (survival signals reduce it) */
    apop->bcl2 += sim->config.growth_factor_conc * 0.05f * dt;
    apop->bcl2 *= (1.0f - 0.02f * dt); /* slow decay */
    apop->bcl2 = clampf(apop->bcl2, 0.0f, 1.0f);

    /* Mitochondrial cytochrome c release: Bax overcomes Bcl-2 */
    if (apop->bax > apop->bcl2 + 0.2f) {
        apop->cytochrome_c += (apop->bax - apop->bcl2) * 0.3f * dt;
        apop->cytochrome_c = clampf(apop->cytochrome_c, 0.0f, 1.0f);
    }

    /* Initiator caspase-9: activated by cytochrome c (apoptosome) */
    if (apop->cytochrome_c > CELL_CASPASE_ACTIVATION) {
        apop->caspase_9 += apop->cytochrome_c * 0.4f * dt;
        apop->caspase_9 = clampf(apop->caspase_9, 0.0f, 1.0f);
    }

    /* Executioner caspase-3: activated by caspase-9 */
    apop->caspase_3 += apop->caspase_9 * 0.5f * dt;
    apop->caspase_3 = clampf(apop->caspase_3, 0.0f, 1.0f);

    /* Point of no return */
    if (apop->caspase_3 > CELL_APOPTOSIS_POINT_OF_NO_RETURN && !apop->committed) {
        apop->committed = true;
        LOG_DEBUG(LOG_TAG, "Cell %u committed to apoptosis", cell_idx);
    }

    /* DNA fragmentation and cell death */
    if (apop->committed) {
        apop->dna_fragmentation += 0.3f * dt;
        apop->dna_fragmentation = clampf(apop->dna_fragmentation, 0.0f, 1.0f);
        if (apop->dna_fragmentation > 0.9f) {
            cell->alive = false;
            cell->phase = CELL_PHASE_APOPTOSIS;
            sim->stats.cells_in_apoptosis++;
        }
    }

    return 0;
}

/* ============================================================================
 * Main Step
 * ============================================================================ */

int cell_biology_step(cell_biology_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;

    uint32_t alive_count = 0;
    uint32_t mitosis_count = 0;
    float total_atp = 0.0f;
    float total_volume = 0.0f;

    for (uint32_t i = 0; i < sim->num_cells; i++) {
        cell_sim_cell_t* cell = &sim->cells[i];
        if (!cell->alive) continue;

        /* ATP production (simplified: aerobic respiration) */
        float atp_production = sim->config.oxygen_level * sim->config.nutrient_level * 0.1f * dt;
        float atp_consumption = 0.05f * dt; /* basal metabolic cost */
        cell->atp_level += atp_production - atp_consumption;
        cell->atp_level = clampf(cell->atp_level, 0.0f, 1.0f);

        /* Cell cycle progression */
        if (cell->phase != CELL_PHASE_G0 && cell->phase != CELL_PHASE_APOPTOSIS) {
            cell_biology_advance_cycle(sim, i, dt);
        }

        /* Cell growth: volume increases during G1 and G2 */
        if (cell->phase == CELL_PHASE_G1 || cell->phase == CELL_PHASE_G2) {
            cell->volume_um3 += cell->volume_um3 * 0.01f * dt * cell->atp_level;
        }

        /* Membrane transport */
        cell_biology_step_transport(sim, i, dt);

        /* Apoptosis check */
        if (sim->config.enable_apoptosis) {
            cell_biology_step_apoptosis(sim, i, dt);
        }

        /* Auto-mitosis when reaching cytokinesis completion */
        if (cell->phase == CELL_PHASE_CYTOKINESIS && cell->phase_progress >= 0.95f) {
            cell_biology_mitosis(sim, i);
            mitosis_count++;
        }

        alive_count++;
        total_atp += cell->atp_level;
        total_volume += cell->volume_um3;
    }

    /* Signal transduction */
    if (sim->config.enable_signal_transduction) {
        cell_biology_step_signals(sim, dt);
    }

    /* Update stats */
    sim->stats.step_count++;
    sim->stats.total_cells = alive_count;
    sim->stats.cells_in_mitosis = mitosis_count;
    sim->stats.mean_atp = alive_count > 0 ? total_atp / alive_count : 0.0f;
    sim->stats.mean_volume = alive_count > 0 ? total_volume / alive_count : 0.0f;

    return 0;
}

/* ============================================================================
 * Preset Loader
 * ============================================================================ */

void cell_biology_load_mammalian_cell(cell_biology_sim_t* sim) {
    if (!sim) return;

    /* Reset */
    sim->num_cells = 0;
    sim->num_channels = 0;
    sim->num_signals = 0;

    /* Create a typical mammalian somatic cell */
    cell_sim_cell_t cell;
    memset(&cell, 0, sizeof(cell));
    cell.id = 0;
    cell.phase = CELL_PHASE_G1;
    cell.phase_progress = 0.0f;
    cell.volume_um3 = 4000.0f;          /* ~4000 um^3 typical */
    cell.membrane_potential_mv = -70.0f;
    cell.atp_level = 0.8f;
    cell.dna_integrity = 1.0f;
    cell.chromosome_count = 46;
    cell.is_diploid = true;
    cell.alive = true;
    cell.intracellular_na = 12.0f;
    cell.intracellular_k = 140.0f;
    cell.extracellular_na = 145.0f;
    cell.extracellular_k = 4.0f;
    cell.intracellular_ca = 0.0001f;    /* 100 nM resting */

    /* Organelles */
    cell.num_organelles = 5;
    cell.organelles[0] = (cell_organelle_t){CELL_ORGANELLE_NUCLEUS, 1.0f, 1.0f, 1};
    cell.organelles[1] = (cell_organelle_t){CELL_ORGANELLE_MITOCHONDRIA, 1.0f, 0.8f, 1000};
    cell.organelles[2] = (cell_organelle_t){CELL_ORGANELLE_ER_ROUGH, 1.0f, 0.7f, 1};
    cell.organelles[3] = (cell_organelle_t){CELL_ORGANELLE_GOLGI, 1.0f, 0.6f, 1};
    cell.organelles[4] = (cell_organelle_t){CELL_ORGANELLE_LYSOSOME, 1.0f, 0.5f, 200};

    cell_biology_add_cell(sim, &cell);

    /* Na+/K+ ATPase channel */
    cell_transport_channel_t nak;
    memset(&nak, 0, sizeof(nak));
    strncpy(nak.name, "Na/K-ATPase", CELL_MAX_NAME_LEN - 1);
    nak.type = CELL_TRANSPORT_ACTIVE_PRIMARY;
    nak.vmax = 200.0f;     /* ions/s */
    nak.atp_cost = 1.0f;   /* 1 ATP per cycle */
    nak.open = true;
    cell_biology_add_channel(sim, &nak);

    /* K+ leak channel (passive) */
    cell_transport_channel_t kleak;
    memset(&kleak, 0, sizeof(kleak));
    strncpy(kleak.name, "K-leak", CELL_MAX_NAME_LEN - 1);
    kleak.type = CELL_TRANSPORT_PASSIVE_DIFFUSION;
    kleak.diffusion_coeff = 1.96e-9f;  /* K+ diffusion coefficient m^2/s */
    kleak.open = true;
    cell_biology_add_channel(sim, &kleak);

    /* GPCR signaling pathway (e.g., epinephrine) */
    cell_signal_pathway_t gpcr;
    memset(&gpcr, 0, sizeof(gpcr));
    strncpy(gpcr.name, "beta-adrenergic", CELL_MAX_NAME_LEN - 1);
    gpcr.type = CELL_SIGNAL_GPCR;
    gpcr.ligand_concentration = 0.0f;
    gpcr.cascade_depth = 4;
    gpcr.decay_rate = 0.1f;
    cell_biology_add_signal(sim, &gpcr);

    LOG_INFO(LOG_TAG, "Loaded mammalian somatic cell with Na/K pump, K leak, GPCR signaling");
}
