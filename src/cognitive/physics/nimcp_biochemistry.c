/**
 * @file nimcp_biochemistry.c
 * @brief Biochemistry simulator — enzyme kinetics, metabolic pathways, signal transduction
 *
 * Implements Michaelis-Menten kinetics with all inhibition types, Hill equation,
 * pH/temperature activity curves, pathway flux computation, signal cascade
 * amplification, and glycolysis loading.
 */

#include "cognitive/physics/nimcp_biochemistry.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_TAG "BIOCHEM"

/** Minimum concentration to avoid division by zero */
#define CONC_EPSILON    1e-12f

/** Activation energy for thermal denaturation (kJ/mol) */
#define DENATURATION_EA 200.0f

/** Universal gas constant kJ/(mol*K) */
#define R_KJ            0.008314f

/* ============================================================================
 * Default config
 * ============================================================================ */

biochem_config_t biochemistry_default_config(void)
{
    biochem_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.temperature             = 310.15f;  /* 37 C (body temp) */
    cfg.pH                      = 7.4f;     /* physiological pH */
    cfg.dt                      = 0.001f;
    cfg.enable_allosteric       = true;
    cfg.enable_signal_cascades  = true;
    return cfg;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

biochemistry_sim_t* biochemistry_create(const biochem_config_t* config)
{
    biochemistry_sim_t* sim = (biochemistry_sim_t*)nimcp_calloc(1, sizeof(biochemistry_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate biochemistry_sim_t");
        return NULL;
    }
    sim->config = config ? *config : biochemistry_default_config();
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Biochemistry sim created (T=%.1fK, pH=%.1f)",
             sim->config.temperature, sim->config.pH);
    return sim;
}

void biochemistry_destroy(biochemistry_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Destroying biochemistry sim (steps=%lu)", (unsigned long)sim->stats.step_count);
    nimcp_free(sim);
}

/* ============================================================================
 * Add entities
 * ============================================================================ */

uint32_t biochemistry_add_metabolite(biochemistry_sim_t* sim, const biochem_metabolite_t* m)
{
    if (!sim || !m) return UINT32_MAX;
    if (sim->num_metabolites >= BIOCHEM_MAX_METABOLITES) {
        LOG_WARN(LOG_TAG, "Max metabolites reached (%d)", BIOCHEM_MAX_METABOLITES);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_metabolites++;
    sim->metabolites[idx] = *m;
    sim->metabolites[idx].id = idx;
    sim->metabolites[idx].active = true;
    return idx;
}

uint32_t biochemistry_add_enzyme(biochemistry_sim_t* sim, const biochem_enzyme_t* e)
{
    if (!sim || !e) return UINT32_MAX;
    if (sim->num_enzymes >= BIOCHEM_MAX_ENZYMES) {
        LOG_WARN(LOG_TAG, "Max enzymes reached (%d)", BIOCHEM_MAX_ENZYMES);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_enzymes++;
    sim->enzymes[idx] = *e;
    sim->enzymes[idx].id = idx;
    sim->enzymes[idx].active = true;
    return idx;
}

uint32_t biochemistry_add_pathway(biochemistry_sim_t* sim, const biochem_pathway_t* p)
{
    if (!sim || !p) return UINT32_MAX;
    if (sim->num_pathways >= BIOCHEM_MAX_PATHWAYS) {
        LOG_WARN(LOG_TAG, "Max pathways reached (%d)", BIOCHEM_MAX_PATHWAYS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_pathways++;
    sim->pathways[idx] = *p;
    sim->pathways[idx].id = idx;
    sim->pathways[idx].active = true;
    return idx;
}

uint32_t biochemistry_add_signal(biochemistry_sim_t* sim, const biochem_signal_t* s)
{
    if (!sim || !s) return UINT32_MAX;
    if (sim->num_signals >= BIOCHEM_MAX_SIGNALS) {
        LOG_WARN(LOG_TAG, "Max signals reached (%d)", BIOCHEM_MAX_SIGNALS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_signals++;
    sim->signals[idx] = *s;
    sim->signals[idx].id = idx;
    sim->signals[idx].active = true;
    return idx;
}

/* ============================================================================
 * Analytical functions
 * ============================================================================ */

/**
 * Michaelis-Menten: v = Vmax * [S] / (Km + [S])
 */
float biochemistry_michaelis_menten(float Vmax, float Km, float substrate_conc)
{
    if (Km + substrate_conc < CONC_EPSILON) return 0.0f;
    return Vmax * substrate_conc / (Km + substrate_conc);
}

/**
 * Competitive inhibition: v = Vmax*[S] / (Km*(1+[I]/Ki) + [S])
 * Inhibitor competes for active site, increases apparent Km.
 */
float biochemistry_competitive_inhibition(float Vmax, float Km, float S, float I, float Ki)
{
    if (Ki < CONC_EPSILON) return 0.0f;
    float Km_app = Km * (1.0f + I / Ki);
    if (Km_app + S < CONC_EPSILON) return 0.0f;
    return Vmax * S / (Km_app + S);
}

/**
 * Hill equation: v = Vmax * [S]^n / (K^n + [S]^n)
 * Models cooperativity: n>1 positive, n<1 negative, n=1 Michaelis-Menten.
 */
float biochemistry_hill_equation(float Vmax, float K, float S, float n)
{
    if (S < CONC_EPSILON || K < CONC_EPSILON) return 0.0f;
    float Sn = powf(S, n);
    float Kn = powf(K, n);
    if (Kn + Sn < CONC_EPSILON) return 0.0f;
    return Vmax * Sn / (Kn + Sn);
}

/**
 * pH dependence: bell-shaped activity curve
 * activity = 1 / (1 + 10^(pK1-pH) + 10^(pH-pK2))
 * pK1 = acidic ionization, pK2 = basic ionization
 */
float biochemistry_ph_activity(float pH, float pK1, float pK2)
{
    float denom = 1.0f + powf(10.0f, pK1 - pH) + powf(10.0f, pH - pK2);
    if (denom < CONC_EPSILON) return 0.0f;
    return 1.0f / denom;
}

/**
 * Temperature dependence: Arrhenius with thermal denaturation
 * Below optimum: activity increases exponentially (Arrhenius)
 * Above optimum: activity drops due to protein unfolding
 */
float biochemistry_temp_activity(float T, float T_opt, float Ea)
{
    if (T < 1.0f) return 0.0f;
    /* Arrhenius activation: k = A*exp(-Ea/(RT)) */
    float activation = expf(-Ea / (BIOCHEM_GAS_CONSTANT * T));
    float activation_opt = expf(-Ea / (BIOCHEM_GAS_CONSTANT * T_opt));
    if (activation_opt < CONC_EPSILON) return 0.0f;
    float arrhenius_factor = activation / activation_opt;

    /* Denaturation above optimum */
    float denaturation = 1.0f;
    if (T > T_opt) {
        float dT = T - T_opt;
        denaturation = expf(-DENATURATION_EA * dT * dT /
                            (BIOCHEM_GAS_CONSTANT * T * T_opt * T_opt));
    }
    float activity = arrhenius_factor * denaturation;
    if (activity > 1.0f) activity = 1.0f;
    if (activity < 0.0f) activity = 0.0f;
    return activity;
}

/* ============================================================================
 * Internal: compute enzyme rate with inhibition, pH, temperature
 * ============================================================================ */

/**
 * Compute the rate of a single enzyme considering:
 *  1. Substrate concentration (Michaelis-Menten or Hill)
 *  2. Inhibition type (competitive, uncompetitive, noncompetitive, allosteric)
 *  3. pH activity bell curve
 *  4. Temperature activity (Arrhenius + denaturation)
 *  5. Cofactor availability
 */
static float compute_enzyme_rate(const biochemistry_sim_t* sim, const biochem_enzyme_t* enz)
{
    if (!enz->active) return 0.0f;
    if (enz->substrate_id >= sim->num_metabolites) return 0.0f;

    float S = sim->metabolites[enz->substrate_id].concentration;
    float Vmax = enz->Vmax;
    float Km = enz->Km;

    /* --- Base rate: Hill or Michaelis-Menten --- */
    float rate;
    if (enz->hill_coefficient > 1.001f || enz->hill_coefficient < 0.999f) {
        rate = biochemistry_hill_equation(Vmax, Km, S, enz->hill_coefficient);
    } else {
        rate = biochemistry_michaelis_menten(Vmax, Km, S);
    }

    /* --- Inhibition --- */
    if (enz->inhibition_type != BIOCHEM_INHIBIT_NONE &&
        enz->inhibitor_id < sim->num_metabolites && enz->Ki > CONC_EPSILON) {
        float I = sim->metabolites[enz->inhibitor_id].concentration;
        float alpha = 1.0f + I / enz->Ki;

        switch (enz->inhibition_type) {
        case BIOCHEM_INHIBIT_COMPETITIVE:
            /* Increase apparent Km */
            rate = Vmax * S / (Km * alpha + S);
            break;
        case BIOCHEM_INHIBIT_UNCOMPETITIVE:
            /* Decrease apparent Vmax and Km equally */
            rate = (Vmax / alpha) * S / (Km / alpha + S);
            break;
        case BIOCHEM_INHIBIT_NONCOMPETITIVE:
            /* Decrease apparent Vmax only */
            rate = (Vmax / alpha) * S / (Km + S);
            break;
        case BIOCHEM_INHIBIT_ALLOSTERIC:
            if (sim->config.enable_allosteric) {
                /* Allosteric: sigmoidal inhibition */
                float n_hill = enz->hill_coefficient > 0.0f ? enz->hill_coefficient : 2.0f;
                float I_safe = I > 0.0f ? I : 0.0f;  /* prevent powf(-ve, frac) = NaN */
                float In = powf(I_safe, n_hill);
                float Kin = powf(enz->Ki > 0.0f ? enz->Ki : 1e-6f, n_hill);
                float allosteric_factor = Kin / (Kin + In);
                rate *= allosteric_factor;
            }
            break;
        default:
            break;
        }
    }

    /* --- Allosteric activation --- */
    if (sim->config.enable_allosteric && enz->activator_id < sim->num_metabolites &&
        enz->Ka > CONC_EPSILON) {
        float A = sim->metabolites[enz->activator_id].concentration;
        float activation = A / (enz->Ka + A);
        rate *= (1.0f + activation);
    }

    /* --- pH dependence (bell curve around optimum) --- */
    float pH_act = biochemistry_ph_activity(sim->config.pH,
                                            enz->pH_optimum - 2.0f,
                                            enz->pH_optimum + 2.0f);
    rate *= pH_act;

    /* --- Temperature dependence --- */
    float temp_act = biochemistry_temp_activity(sim->config.temperature,
                                                enz->temp_optimum, 50.0f);
    rate *= temp_act;

    /* --- Cofactor requirement --- */
    if (enz->cofactor_id < sim->num_metabolites) {
        float cof = sim->metabolites[enz->cofactor_id].concentration;
        if (cof < CONC_EPSILON) {
            rate *= 0.01f;  /* severely limited without cofactor */
        } else {
            float cof_sat = cof / (cof + 0.001f);  /* saturation curve */
            rate *= cof_sat;
        }
    }

    return rate;
}

/* ============================================================================
 * Step
 * ============================================================================ */

int biochemistry_step(biochemistry_sim_t* sim, float dt)
{
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;
    if (dt <= 0.0f) dt = 0.001f;

    float max_rate = 0.0f;

    /* --- Phase 1: Enzyme reactions --- */
    for (uint32_t i = 0; i < sim->num_enzymes; i++) {
        biochem_enzyme_t* enz = &sim->enzymes[i];
        if (!enz->active) continue;

        float rate = compute_enzyme_rate(sim, enz);
        if (rate > max_rate) max_rate = rate;

        /* Consume substrate, produce product */
        float consumed = rate * dt;
        if (enz->substrate_id < sim->num_metabolites) {
            float* s_conc = &sim->metabolites[enz->substrate_id].concentration;
            if (consumed > *s_conc) consumed = *s_conc;
            *s_conc -= consumed;
        }
        if (enz->product_id < sim->num_metabolites) {
            sim->metabolites[enz->product_id].concentration += consumed;
        }
        /* Consume cofactor (catalytic amounts) */
        if (enz->cofactor_id < sim->num_metabolites) {
            float* c_conc = &sim->metabolites[enz->cofactor_id].concentration;
            float cof_used = consumed * 0.001f;  /* catalytic, not stoichiometric */
            if (cof_used > *c_conc) cof_used = *c_conc;
            *c_conc -= cof_used;
        }
    }

    /* --- Phase 2: Pathway flux computation --- */
    for (uint32_t i = 0; i < sim->num_pathways; i++) {
        biochem_pathway_t* pw = &sim->pathways[i];
        if (!pw->active || pw->num_enzymes == 0) continue;

        /* Flux = minimum enzyme rate in the pathway (bottleneck) */
        float min_flux = 1e30f;
        for (uint32_t j = 0; j < pw->num_enzymes && j < BIOCHEM_MAX_STEPS; j++) {
            uint32_t eid = pw->enzyme_ids[j];
            if (eid < sim->num_enzymes) {
                float r = compute_enzyme_rate(sim, &sim->enzymes[eid]);
                if (r < min_flux) min_flux = r;
            }
        }
        pw->flux = (min_flux < 1e29f) ? min_flux : 0.0f;
    }

    /* --- Phase 3: Signal cascades --- */
    if (sim->config.enable_signal_cascades) {
        for (uint32_t i = 0; i < sim->num_signals; i++) {
            biochem_signal_t* sig = &sim->signals[i];
            if (!sig->active) continue;

            /* Input: receptor metabolite concentration */
            float input = 0.0f;
            if (sig->receptor_metabolite < sim->num_metabolites) {
                input = sim->metabolites[sig->receptor_metabolite].concentration;
            }

            /* Amplification: sigmoid with gain */
            float amplified = sig->amplification_factor * input /
                              (0.001f + input);

            /* Update signal strength with decay */
            sig->signal_strength += (amplified - sig->signal_strength * sig->decay_rate) * dt;
            if (sig->signal_strength < 0.0f) sig->signal_strength = 0.0f;
            if (sig->signal_strength > 1.0f) sig->signal_strength = 1.0f;

            /* Effect on downstream metabolite */
            if (sig->effector_metabolite < sim->num_metabolites) {
                sim->metabolites[sig->effector_metabolite].concentration +=
                    sig->signal_strength * 0.001f * dt;
            }
        }
    }

    /* --- Update stats --- */
    sim->stats.step_count++;
    sim->stats.max_enzyme_rate = max_rate;
    sim->time += dt;

    /* Compute total ATP and NADH */
    float total_atp = 0.0f, total_nadh = 0.0f, total_flux = 0.0f;
    for (uint32_t i = 0; i < sim->num_metabolites; i++) {
        if (sim->metabolites[i].is_energy_carrier) {
            if (strstr(sim->metabolites[i].name, "ATP") != NULL)
                total_atp += sim->metabolites[i].concentration;
            if (strstr(sim->metabolites[i].name, "NADH") != NULL)
                total_nadh += sim->metabolites[i].concentration;
        }
    }
    for (uint32_t i = 0; i < sim->num_pathways; i++) {
        if (sim->pathways[i].active) total_flux += sim->pathways[i].flux;
    }
    sim->stats.total_atp = total_atp;
    sim->stats.total_nadh = total_nadh;
    sim->stats.total_flux = total_flux;

    return 0;
}

/* ============================================================================
 * Load glycolysis pathway
 * ============================================================================ */

void biochemistry_load_glycolysis(biochemistry_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Loading glycolysis pathway (10 enzymes)");

    /* Key metabolites: glucose, G6P, F6P, F16BP, DHAP, G3P, 13BPG,
       3PG, 2PG, PEP, pyruvate, ATP, ADP, NAD+, NADH */
    const char* metab_names[] = {
        "glucose", "G6P", "F6P", "F16BP", "DHAP", "G3P",
        "13BPG", "3PG", "2PG", "PEP", "pyruvate",
        "ATP", "ADP", "NAD+", "NADH"
    };
    const float metab_conc[] = {
        5.0e-3f, 0.083e-3f, 0.014e-3f, 0.031e-3f, 0.14e-3f, 0.019e-3f,
        0.001e-3f, 0.18e-3f, 0.03e-3f, 0.023e-3f, 0.051e-3f,
        9.2e-3f, 0.6e-3f, 0.54e-3f, 0.033e-3f
    };
    const float metab_mw[] = {
        180.16f, 260.14f, 260.14f, 340.12f, 170.06f, 170.06f,
        266.03f, 186.06f, 186.06f, 168.04f, 88.06f,
        507.18f, 427.20f, 663.43f, 665.44f
    };
    const bool is_energy[] = {
        false, false, false, false, false, false,
        false, false, false, false, false,
        true, false, false, true
    };

    uint32_t metab_ids[15];
    for (int i = 0; i < 15; i++) {
        biochem_metabolite_t m;
        memset(&m, 0, sizeof(m));
        strncpy(m.name, metab_names[i], BIOCHEM_MAX_NAME - 1);
        m.concentration = metab_conc[i];
        m.molecular_weight = metab_mw[i];
        m.is_energy_carrier = is_energy[i];
        m.is_cofactor = (i == 13 || i == 14);  /* NAD+, NADH */
        metab_ids[i] = biochemistry_add_metabolite(sim, &m);
    }

    /* 10 glycolytic enzymes with real Km and Vmax estimates */
    struct {
        const char* name;
        float Vmax;     /* umol/min/mg -> simplified to mol/L/s */
        float Km;       /* mM */
        int sub;        /* substrate index */
        int prod;       /* product index */
        float pH_opt;
        float temp_opt;
    } enzymes[] = {
        { "hexokinase",         0.10f, 0.10e-3f,  0,  1, 7.6f, 310.15f },
        { "PGI",                0.50f, 0.40e-3f,  1,  2, 8.5f, 310.15f },
        { "PFK-1",              0.20f, 0.03e-3f,  2,  3, 7.0f, 310.15f },
        { "aldolase",           0.10f, 0.01e-3f,  3,  5, 7.4f, 310.15f },
        { "TPI",                1.00f, 0.50e-3f,  4,  5, 7.6f, 310.15f },
        { "GAPDH",              0.50f, 0.07e-3f,  5,  6, 8.4f, 310.15f },
        { "PGK",                0.80f, 1.10e-3f,  6,  7, 7.5f, 310.15f },
        { "PGM",                0.60f, 0.50e-3f,  7,  8, 7.5f, 310.15f },
        { "enolase",            0.30f, 0.04e-3f,  8,  9, 6.5f, 310.15f },
        { "pyruvate_kinase",    0.40f, 0.31e-3f,  9, 10, 7.4f, 310.15f },
    };

    uint32_t enz_ids[10];
    for (int i = 0; i < 10; i++) {
        biochem_enzyme_t e;
        memset(&e, 0, sizeof(e));
        strncpy(e.name, enzymes[i].name, BIOCHEM_MAX_NAME - 1);
        e.Vmax = enzymes[i].Vmax;
        e.Km = enzymes[i].Km;
        e.kcat = enzymes[i].Vmax * 10.0f;  /* rough kcat estimate */
        e.substrate_id = metab_ids[enzymes[i].sub];
        e.product_id = metab_ids[enzymes[i].prod];
        e.cofactor_id = UINT32_MAX;
        e.pH_optimum = enzymes[i].pH_opt;
        e.temp_optimum = enzymes[i].temp_opt;
        e.inhibition_type = BIOCHEM_INHIBIT_NONE;
        e.inhibitor_id = UINT32_MAX;
        e.activator_id = UINT32_MAX;
        e.hill_coefficient = 1.0f;
        enz_ids[i] = biochemistry_add_enzyme(sim, &e);
    }

    /* PFK-1 is allosterically inhibited by ATP, activated by AMP (use ADP as proxy) */
    sim->enzymes[enz_ids[2]].inhibition_type = BIOCHEM_INHIBIT_ALLOSTERIC;
    sim->enzymes[enz_ids[2]].inhibitor_id = metab_ids[11];  /* ATP */
    sim->enzymes[enz_ids[2]].Ki = 5.0e-3f;
    sim->enzymes[enz_ids[2]].activator_id = metab_ids[12];  /* ADP */
    sim->enzymes[enz_ids[2]].Ka = 0.1e-3f;
    sim->enzymes[enz_ids[2]].hill_coefficient = 2.0f;

    /* GAPDH uses NAD+ as cofactor, produces NADH */
    sim->enzymes[enz_ids[5]].cofactor_id = metab_ids[13];  /* NAD+ */

    /* Hexokinase: product inhibition by G6P */
    sim->enzymes[enz_ids[0]].inhibition_type = BIOCHEM_INHIBIT_COMPETITIVE;
    sim->enzymes[enz_ids[0]].inhibitor_id = metab_ids[1];  /* G6P */
    sim->enzymes[enz_ids[0]].Ki = 0.5e-3f;

    /* Pyruvate kinase: allosteric activation by F16BP */
    sim->enzymes[enz_ids[9]].activator_id = metab_ids[3];  /* F16BP */
    sim->enzymes[enz_ids[9]].Ka = 0.05e-3f;
    sim->enzymes[enz_ids[9]].hill_coefficient = 1.5f;

    /* Build glycolysis pathway */
    biochem_pathway_t pw;
    memset(&pw, 0, sizeof(pw));
    strncpy(pw.name, "glycolysis", BIOCHEM_MAX_NAME - 1);
    pw.num_enzymes = 10;
    for (int i = 0; i < 10; i++) pw.enzyme_ids[i] = enz_ids[i];
    pw.net_atp_yield = 2.0f;   /* net: 2 ATP per glucose */
    pw.net_nadh_yield = 2.0f;  /* 2 NADH per glucose */
    biochemistry_add_pathway(sim, &pw);

    LOG_INFO(LOG_TAG, "Glycolysis loaded: %u metabolites, %u enzymes, 1 pathway",
             sim->num_metabolites, sim->num_enzymes);
}

/* ============================================================================
 * Load stubs for krebs cycle and electron transport
 * ============================================================================ */

void biochemistry_load_krebs_cycle(biochemistry_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Krebs cycle: stub (not yet implemented)");
}

void biochemistry_load_electron_transport(biochemistry_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Electron transport chain: stub (not yet implemented)");
}

/* ============================================================================
 * Stats
 * ============================================================================ */

biochem_stats_t biochemistry_get_stats(const biochemistry_sim_t* sim)
{
    biochem_stats_t zero;
    memset(&zero, 0, sizeof(zero));
    if (!sim) return zero;
    return sim->stats;
}
