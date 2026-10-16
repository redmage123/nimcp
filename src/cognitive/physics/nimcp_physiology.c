/**
 * @file nimcp_physiology.c
 * @brief Physiology simulation engine -- cardiovascular, respiratory, renal,
 *        endocrine, thermoregulation, acid-base balance
 *
 * WHAT: Simulates human organ system physiology with real equations.
 * WHY:  Physiological prior for world model reasoning about health and function.
 * HOW:  Frank-Starling for cardiac output, Hill equation for O2-Hb dissociation,
 *       Henderson-Hasselbalch for blood pH, negative feedback hormone loops,
 *       Poiseuille flow for hemodynamics, thermoregulation feedback control.
 */

#include "cognitive/physics/nimcp_physiology.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "PHYSIOLOGY"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

physiology_config_t physiology_default_config(void) {
    physiology_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dt = 1.0f;                  /* 1 minute time step */
    cfg.temperature_c = 22.0f;     /* ambient temperature */
    cfg.enable_cardiovascular = true;
    cfg.enable_respiratory = true;
    cfg.enable_renal = true;
    cfg.enable_endocrine = true;
    cfg.enable_thermoregulation = true;
    cfg.activity_level = 0.0f;      /* at rest */
    return cfg;
}

physiology_sim_t* physiology_create(const physiology_config_t* config) {
    physiology_sim_t* sim = (physiology_sim_t*)nimcp_calloc(1, sizeof(physiology_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate physiology sim");
        return NULL;
    }
    sim->config = config ? *config : physiology_default_config();
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Physiology sim created (dt=%.1f min)", sim->config.dt);
    return sim;
}

void physiology_destroy(physiology_sim_t* sim) {
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Physiology sim destroyed after %lu steps", sim->stats.step_count);
    nimcp_free(sim);
}

physiology_stats_t physiology_get_stats(const physiology_sim_t* sim) {
    physiology_stats_t empty;
    memset(&empty, 0, sizeof(empty));
    return sim ? sim->stats : empty;
}

/* ============================================================================
 * Cardiovascular System
 * ============================================================================ */

/**
 * Frank-Starling mechanism: stroke volume increases with preload.
 * SV = SV_base * (1 + k * (preload - preload_base))
 * Saturates at high preload (descending limb of Starling curve).
 */
float physiology_frank_starling(float preload_ml) {
    /* Baseline: 120 mL EDV -> 70 mL SV */
    float base_edv = 120.0f;
    float base_sv = 70.0f;
    float k = 0.8f; /* Starling gain */

    float sv = base_sv * (1.0f + k * (preload_ml - base_edv) / base_edv);

    /* Descending limb: over-stretching reduces contractility */
    if (preload_ml > 180.0f) {
        float excess = (preload_ml - 180.0f) / 60.0f;
        sv *= expf(-excess);
    }

    return clampf(sv, 10.0f, 150.0f);
}

/**
 * Cardiac output: CO = HR * SV / 1000 (L/min)
 */
float physiology_cardiac_output(float hr, float sv_ml) {
    return hr * sv_ml / 1000.0f;
}

/**
 * Mean arterial pressure: MAP = CO * TPR
 */
float physiology_map(float co, float tpr) {
    return co * tpr;
}

/**
 * Poiseuille's law: Q = pi * r^4 * dP / (8 * mu * L)
 * r in cm, dP in mmHg, mu in centipoise, L in cm.
 * Returns flow in mL/min.
 */
float physiology_poiseuille_flow(float radius_cm, float dp_mmhg,
                                  float viscosity_cp, float length_cm) {
    if (viscosity_cp < 1e-6f || length_cm < 1e-6f) return 0.0f;

    /* Convert mmHg to dyn/cm^2: 1 mmHg = 1333.22 dyn/cm^2 */
    float dp_dyn = dp_mmhg * 1333.22f;
    /* Convert centipoise to dyn*s/cm^2: 1 cP = 0.01 dyn*s/cm^2 */
    float mu_cgs = viscosity_cp * 0.01f;

    float r4 = radius_cm * radius_cm * radius_cm * radius_cm;
    float q_cm3_s = (float)M_PI * r4 * dp_dyn / (8.0f * mu_cgs * length_cm);

    return q_cm3_s * 60.0f; /* cm^3/s -> mL/min */
}

int physiology_step_cardiovascular(physiology_sim_t* sim, float dt) {
    if (!sim || !sim->config.enable_cardiovascular) return 0;
    physio_cardiovascular_t* cv = &sim->cardiovascular;

    /* Activity modifies heart rate (sympathetic drive) */
    float target_hr = PHYSIO_RESTING_HR *
                      (1.0f + 2.0f * sim->config.activity_level);
    cv->heart_rate += (target_hr - cv->heart_rate) * 0.1f * dt;

    /* Frank-Starling: SV from preload */
    cv->stroke_volume_ml = physiology_frank_starling(cv->preload);

    /* Activity-induced SV increase (sympathetic inotropy) */
    cv->stroke_volume_ml *= (1.0f + 0.5f * sim->config.activity_level);
    cv->stroke_volume_ml = clampf(cv->stroke_volume_ml, 20.0f, 200.0f);

    /* Cardiac output */
    cv->cardiac_output = physiology_cardiac_output(cv->heart_rate, cv->stroke_volume_ml);

    /* Ejection fraction */
    cv->ejection_fraction = cv->stroke_volume_ml / (cv->preload + 1e-6f);
    cv->ejection_fraction = clampf(cv->ejection_fraction, 0.1f, 0.85f);

    /* TPR adjusts with activity (vasodilation during exercise) */
    float target_tpr = PHYSIO_TPR_NORMAL * (1.0f - 0.4f * sim->config.activity_level);
    cv->tpr += (target_tpr - cv->tpr) * 0.05f * dt;

    /* MAP = CO * TPR */
    cv->map_mmhg = physiology_map(cv->cardiac_output, cv->tpr);

    /* Pulse pressure */
    float pp = cv->stroke_volume_ml * 0.4f; /* simplified */
    cv->systolic_mmhg = cv->map_mmhg + pp * 0.33f;
    cv->diastolic_mmhg = cv->map_mmhg - pp * 0.67f;

    /* Venous return = cardiac output at steady state */
    cv->venous_return_l_min = cv->cardiac_output;

    return 0;
}

/* ============================================================================
 * Respiratory System
 * ============================================================================ */

/**
 * Hill equation for oxygen-hemoglobin dissociation:
 * SO2 = pO2^n / (P50^n + pO2^n)
 * n = 2.7 (Hill coefficient, cooperative binding)
 * P50 = 26.6 mmHg (pO2 at 50% saturation)
 */
float physiology_hill_equation(float po2, float p50, float n) {
    if (po2 <= 0.0f) return 0.0f;
    float po2_n = powf(po2, n);
    float p50_n = powf(p50, n);
    return po2_n / (p50_n + po2_n);
}

/**
 * Alveolar gas equation:
 * PAO2 = FiO2 * (P_atm - P_H2O) - PaCO2 / RQ
 * P_atm = 760 mmHg, P_H2O = 47 mmHg, RQ = 0.8 (respiratory quotient)
 */
float physiology_alveolar_gas(float fio2, float paco2, float rq) {
    float p_atm = 760.0f;
    float p_h2o = 47.0f;
    float pio2 = fio2 * (p_atm - p_h2o);
    return pio2 - paco2 / rq;
}

int physiology_step_respiratory(physiology_sim_t* sim, float dt) {
    if (!sim || !sim->config.enable_respiratory) return 0;
    physio_respiratory_t* resp = &sim->respiratory;

    /* Adjust respiratory rate with activity and CO2 */
    float co2_drive = (resp->paco2_mmhg - 40.0f) * 0.5f; /* central chemoreceptor */
    float activity_drive = sim->config.activity_level * 30.0f;
    float target_rr = PHYSIO_RESP_RATE + co2_drive + activity_drive;
    resp->resp_rate += (target_rr - resp->resp_rate) * 0.2f * dt;
    resp->resp_rate = clampf(resp->resp_rate, 6.0f, 60.0f);

    /* Tidal volume increases with exercise */
    float target_tv = PHYSIO_TIDAL_VOLUME_ML *
                      (1.0f + 3.0f * sim->config.activity_level);
    resp->tidal_volume_ml += (target_tv - resp->tidal_volume_ml) * 0.1f * dt;

    /* Minute ventilation */
    resp->minute_ventilation = resp->resp_rate * resp->tidal_volume_ml / 1000.0f;

    /* CO2 production increases with activity */
    float vco2 = 0.2f * (1.0f + 5.0f * sim->config.activity_level); /* L/min */
    float rq = 0.8f;

    /* PaCO2 depends on ventilation: PaCO2 = VCO2 * 0.863 / VA */
    float va = resp->minute_ventilation - 0.15f * resp->resp_rate; /* dead space */
    if (va > 0.1f) {
        float target_paco2 = vco2 * 0.863f / va * 1000.0f;
        /* Smooth transition */
        resp->paco2_mmhg += (target_paco2 - resp->paco2_mmhg) * 0.1f * dt;
    }
    resp->paco2_mmhg = clampf(resp->paco2_mmhg, 15.0f, 80.0f);

    /* Alveolar gas equation */
    resp->pao2_mmhg = physiology_alveolar_gas(resp->fio2, resp->paco2_mmhg, rq);
    resp->pao2_mmhg = clampf(resp->pao2_mmhg, 20.0f, 150.0f);

    /* O2-Hb dissociation (Hill equation) */
    resp->sao2 = physiology_hill_equation(resp->pao2_mmhg, PHYSIO_P50_MMHG, PHYSIO_HILL_N);
    resp->svo2 = physiology_hill_equation(resp->pvo2_mmhg, PHYSIO_P50_MMHG, PHYSIO_HILL_N);

    /* V/Q matching */
    if (resp->minute_ventilation > 0.0f && sim->cardiovascular.cardiac_output > 0.0f) {
        resp->vq_ratio = resp->minute_ventilation / sim->cardiovascular.cardiac_output;
    }

    /* Venous pO2 based on O2 extraction */
    float o2_delivery = sim->cardiovascular.cardiac_output * resp->sao2 *
                        sim->blood.hemoglobin_g_dl * 1.34f; /* mL O2/min */
    float o2_consumption = 250.0f * (1.0f + 5.0f * sim->config.activity_level); /* mL/min */
    float extraction = o2_consumption / (o2_delivery + 1e-6f);
    extraction = clampf(extraction, 0.0f, 0.85f);
    resp->svo2 = resp->sao2 * (1.0f - extraction);
    /* Inverse Hill to get pVO2 */
    if (resp->svo2 > 0.01f && resp->svo2 < 0.99f) {
        float ratio = resp->svo2 / (1.0f - resp->svo2);
        resp->pvo2_mmhg = PHYSIO_P50_MMHG * powf(ratio, 1.0f / PHYSIO_HILL_N);
    }

    return 0;
}

/* ============================================================================
 * Renal System
 * ============================================================================ */

/**
 * Glomerular Filtration Rate:
 * GFR = Kf * (P_gc - P_bc - pi_gc)
 * Net filtration pressure = hydrostatic - capsule - oncotic
 */
float physiology_gfr(float kf, float pgc, float pbc, float pi_gc) {
    float nfp = pgc - pbc - pi_gc;
    if (nfp < 0.0f) return 0.0f;
    return kf * nfp;
}

/**
 * Renal clearance: C = (U * V) / P
 * U = urine concentration, V = urine flow rate, P = plasma concentration
 */
float physiology_clearance(float urine_conc, float urine_flow, float plasma_conc) {
    if (plasma_conc < 1e-10f) return 0.0f;
    return urine_conc * urine_flow / plasma_conc;
}

/**
 * Free water clearance: CH2O = V - Cosm
 * Positive = dilute urine (excess water), Negative = concentrated urine
 */
float physiology_free_water_clearance(float urine_flow, float osmolar_clearance) {
    return urine_flow - osmolar_clearance;
}

int physiology_step_renal(physiology_sim_t* sim, float dt) {
    if (!sim || !sim->config.enable_renal) return 0;
    physio_renal_t* renal = &sim->renal;

    /* GFR depends on MAP (autoregulation between 80-180 mmHg) */
    float map = sim->cardiovascular.map_mmhg;
    float pgc;
    if (map >= 80.0f && map <= 180.0f) {
        pgc = PHYSIO_PGC_MMHG; /* autoregulated */
    } else if (map < 80.0f) {
        pgc = PHYSIO_PGC_MMHG * (map / 80.0f);
    } else {
        pgc = PHYSIO_PGC_MMHG * (1.0f + 0.5f * (map - 180.0f) / 180.0f);
    }

    renal->gfr_ml_min = physiology_gfr(PHYSIO_KF_NORMAL, pgc,
                                         PHYSIO_PBC_MMHG, PHYSIO_PI_GC_MMHG);

    /* RPF tracks cardiac output */
    renal->rpf_ml_min = sim->cardiovascular.cardiac_output * 1000.0f * 0.25f / 5.0f;
    /* ~25% of CO goes to kidneys */
    renal->rpf_ml_min = clampf(renal->rpf_ml_min, 200.0f, 1200.0f);

    renal->filtration_fraction = renal->gfr_ml_min / (renal->rpf_ml_min + 1e-6f);

    /* Urine output: affected by ADH */
    float adh = sim->endocrine.hormones[PHYSIO_HORMONE_ADH].concentration;
    float adh_effect = 1.0f / (1.0f + adh * 0.1f); /* ADH increases reabsorption */
    renal->urine_output_ml_min = renal->gfr_ml_min * 0.01f * adh_effect;
    renal->urine_output_ml_min = clampf(renal->urine_output_ml_min, 0.3f, 15.0f);

    /* Electrolyte balance (simplified) */
    float aldosterone = sim->endocrine.hormones[PHYSIO_HORMONE_ALDOSTERONE].concentration;
    /* Aldosterone increases Na+ reabsorption and K+ secretion */
    float na_reabsorption = 0.99f + aldosterone * 0.005f;
    na_reabsorption = clampf(na_reabsorption, 0.95f, 0.999f);

    renal->serum_sodium += (140.0f - renal->serum_sodium) * 0.01f * dt;
    renal->serum_potassium += (4.2f - renal->serum_potassium) * 0.01f * dt;

    /* Creatinine clearance as GFR marker */
    renal->serum_creatinine = PHYSIO_GFR_NORMAL_ML_MIN / (renal->gfr_ml_min + 1e-6f);
    renal->serum_creatinine = clampf(renal->serum_creatinine, 0.3f, 10.0f);

    return 0;
}

/* ============================================================================
 * Endocrine System
 * ============================================================================ */

int physiology_step_insulin_glucose(physiology_sim_t* sim, float dt) {
    if (!sim) return -1;
    physio_endocrine_t* endo = &sim->endocrine;

    float glucose = endo->blood_glucose_mg_dl;
    float insulin = endo->hormones[PHYSIO_HORMONE_INSULIN].concentration;
    float glucagon = endo->hormones[PHYSIO_HORMONE_GLUCAGON].concentration;

    /* Insulin secretion: stimulated by high glucose (beta cells) */
    /* Sigmoid response centered at glucose = 100 mg/dL */
    float insulin_stimulus = 1.0f / (1.0f + expf(-(glucose - 100.0f) * 0.05f));
    float insulin_production = insulin_stimulus * 2.0f;
    float insulin_clearance = insulin * 0.1f; /* ~10 min half-life */

    endo->hormones[PHYSIO_HORMONE_INSULIN].concentration +=
        (insulin_production - insulin_clearance) * dt;
    endo->hormones[PHYSIO_HORMONE_INSULIN].concentration =
        clampf(endo->hormones[PHYSIO_HORMONE_INSULIN].concentration, 0.0f, 100.0f);

    /* Glucagon secretion: stimulated by LOW glucose (alpha cells) */
    float glucagon_stimulus = 1.0f / (1.0f + expf((glucose - 80.0f) * 0.1f));
    float glucagon_production = glucagon_stimulus * 1.0f;
    float glucagon_clearance = glucagon * 0.15f;

    endo->hormones[PHYSIO_HORMONE_GLUCAGON].concentration +=
        (glucagon_production - glucagon_clearance) * dt;
    endo->hormones[PHYSIO_HORMONE_GLUCAGON].concentration =
        clampf(endo->hormones[PHYSIO_HORMONE_GLUCAGON].concentration, 0.0f, 50.0f);

    /* Glucose dynamics */
    /* Insulin promotes glucose uptake (lowers glucose) */
    float uptake = insulin * endo->insulin_sensitivity * 0.5f;
    /* Glucagon promotes glycogenolysis (raises glucose) */
    float production = glucagon * 0.3f;
    /* Exercise increases glucose consumption */
    float exercise_use = sim->config.activity_level * 5.0f;

    glucose += (production - uptake - exercise_use) * dt;
    endo->blood_glucose_mg_dl = clampf(glucose, 30.0f, 500.0f);

    return 0;
}

int physiology_step_endocrine(physiology_sim_t* sim, float dt) {
    if (!sim || !sim->config.enable_endocrine) return 0;
    physio_endocrine_t* endo = &sim->endocrine;

    /* Insulin-glucose dynamics */
    physiology_step_insulin_glucose(sim, dt);

    /* HPA axis: hypothalamus -> CRH -> pituitary ACTH -> adrenal cortisol */
    float stress = sim->config.activity_level; /* activity as stress proxy */
    float cortisol = endo->hormones[PHYSIO_HORMONE_CORTISOL].concentration;

    /* Negative feedback: high cortisol suppresses HPA */
    float hpa_drive = stress * 2.0f - cortisol * 0.1f;
    hpa_drive = clampf(hpa_drive, 0.0f, 5.0f);
    endo->hpa_axis_activity = hpa_drive;

    float cortisol_production = hpa_drive * 0.5f;
    float cortisol_clearance = cortisol * 0.05f; /* ~90 min half-life */
    endo->hormones[PHYSIO_HORMONE_CORTISOL].concentration +=
        (cortisol_production - cortisol_clearance) * dt;
    endo->hormones[PHYSIO_HORMONE_CORTISOL].concentration =
        clampf(endo->hormones[PHYSIO_HORMONE_CORTISOL].concentration, 0.0f, 60.0f);

    /* Thyroid axis: TSH -> T4 -> T3, negative feedback */
    float tsh = endo->hormones[PHYSIO_HORMONE_TSH].concentration;
    float t4 = endo->hormones[PHYSIO_HORMONE_T4].concentration;

    float tsh_production = 0.5f - t4 * 0.05f; /* Negative feedback */
    tsh_production = clampf(tsh_production, 0.0f, 3.0f);
    endo->hormones[PHYSIO_HORMONE_TSH].concentration +=
        (tsh_production - tsh * 0.1f) * dt;
    endo->hormones[PHYSIO_HORMONE_TSH].concentration =
        clampf(endo->hormones[PHYSIO_HORMONE_TSH].concentration, 0.0f, 20.0f);

    float t4_production = tsh * 1.0f;
    endo->hormones[PHYSIO_HORMONE_T4].concentration +=
        (t4_production - t4 * 0.05f) * dt;
    endo->hormones[PHYSIO_HORMONE_T4].concentration =
        clampf(endo->hormones[PHYSIO_HORMONE_T4].concentration, 0.0f, 30.0f);

    /* ADH: released by hypothalamus in response to osmolality */
    float osmolality = sim->renal.serum_sodium * 2.0f + sim->endocrine.blood_glucose_mg_dl / 18.0f;
    float adh_drive = (osmolality - 285.0f) * 0.1f; /* normal ~285-295 mOsm */
    adh_drive = clampf(adh_drive, 0.0f, 5.0f);
    endo->hormones[PHYSIO_HORMONE_ADH].concentration +=
        (adh_drive - endo->hormones[PHYSIO_HORMONE_ADH].concentration * 0.2f) * dt;
    endo->hormones[PHYSIO_HORMONE_ADH].concentration =
        clampf(endo->hormones[PHYSIO_HORMONE_ADH].concentration, 0.0f, 20.0f);

    return 0;
}

/* ============================================================================
 * Thermoregulation
 * ============================================================================ */

int physiology_step_thermoregulation(physiology_sim_t* sim, float dt) {
    if (!sim || !sim->config.enable_thermoregulation) return 0;
    physio_thermoregulation_t* therm = &sim->thermoreg;

    therm->ambient_temp_c = sim->config.temperature_c;

    /* Metabolic heat production: basal + exercise */
    therm->metabolic_heat_w = PHYSIO_BASAL_METABOLIC_RATE_W *
        (1.0f + 10.0f * sim->config.activity_level);

    /* Heat loss mechanisms */
    float temp_gradient = therm->core_temp_c - therm->ambient_temp_c;

    /* Radiation + convection (proportional to gradient) */
    float passive_loss = temp_gradient * 2.0f; /* W per degree gradient */

    /* Shivering: activated below 35C core */
    if (therm->core_temp_c < PHYSIO_SHIVER_THRESHOLD_C) {
        therm->shivering_intensity = clampf(
            (PHYSIO_SHIVER_THRESHOLD_C - therm->core_temp_c) / 3.0f, 0.0f, 1.0f);
        /* Shivering generates heat */
        therm->metabolic_heat_w += therm->shivering_intensity * 300.0f;
    } else {
        therm->shivering_intensity *= 0.9f; /* decay */
    }

    /* Sweating: activated above 37.5C core */
    if (therm->core_temp_c > PHYSIO_SWEAT_THRESHOLD_C) {
        therm->sweating_rate = clampf(
            (therm->core_temp_c - PHYSIO_SWEAT_THRESHOLD_C) / 3.0f, 0.0f, 1.0f);
        /* Evaporative cooling: up to 600W */
        float evap_loss = therm->sweating_rate * 600.0f;
        passive_loss += evap_loss;
    } else {
        therm->sweating_rate *= 0.9f;
    }

    /* Vasodilation: increases heat loss via skin */
    if (therm->core_temp_c > 37.0f) {
        therm->vasodilation = clampf(
            (therm->core_temp_c - 37.0f) / 2.0f, 0.0f, 1.0f);
        passive_loss *= (1.0f + therm->vasodilation);
    } else {
        /* Vasoconstriction in cold */
        therm->vasodilation = 0.0f;
        passive_loss *= 0.5f;
    }

    therm->heat_loss_w = passive_loss;

    /* Core temperature change: dT/dt = (heat_production - heat_loss) / heat_capacity */
    /* Body heat capacity ~3500 J/C for 70kg person (specific heat * mass) */
    float heat_capacity = 3500.0f; /* J/C */
    float net_heat = therm->metabolic_heat_w - therm->heat_loss_w;
    float dT = net_heat * (dt * 60.0f) / heat_capacity; /* dt in min -> seconds */
    therm->core_temp_c += dT;
    therm->core_temp_c = clampf(therm->core_temp_c, PHYSIO_LETHAL_LOW_C, PHYSIO_LETHAL_HIGH_C);

    /* Skin temperature follows core with lag, influenced by ambient */
    float target_skin = 0.7f * therm->core_temp_c + 0.3f * therm->ambient_temp_c;
    therm->skin_temp_c += (target_skin - therm->skin_temp_c) * 0.05f * dt;

    return 0;
}

/* ============================================================================
 * Acid-Base Balance
 * ============================================================================ */

/**
 * Henderson-Hasselbalch equation:
 * pH = pKa + log10([HCO3-] / (0.03 * pCO2))
 * pKa = 6.1 for carbonic acid system
 */
float physiology_henderson_hasselbalch(float hco3, float pco2) {
    if (hco3 <= 0.0f || pco2 <= 0.0f) return 7.0f;
    float ratio = hco3 / (PHYSIO_CO2_SOLUBILITY * pco2);
    return PHYSIO_HH_PKA + log10f(ratio);
}

/**
 * Classify acid-base disorder from pH, pCO2, HCO3.
 */
physio_acidbase_status_t physiology_classify_acidbase(float ph, float pco2,
                                                       float hco3) {
    bool acidemia = ph < 7.35f;
    bool alkalemia = ph > 7.45f;

    if (!acidemia && !alkalemia) return PHYSIO_ACIDBASE_NORMAL;

    if (acidemia) {
        if (pco2 > 45.0f) return PHYSIO_ACIDBASE_RESP_ACIDOSIS;
        if (hco3 < 22.0f) return PHYSIO_ACIDBASE_MET_ACIDOSIS;
        return PHYSIO_ACIDBASE_MIXED;
    }

    /* alkalemia */
    if (pco2 < 35.0f) return PHYSIO_ACIDBASE_RESP_ALKALOSIS;
    if (hco3 > 26.0f) return PHYSIO_ACIDBASE_MET_ALKALOSIS;
    return PHYSIO_ACIDBASE_MIXED;
}

int physiology_step_acidbase(physiology_sim_t* sim, float dt) {
    if (!sim) return -1;
    physio_acidbase_t* ab = &sim->acidbase;

    ab->pco2_mmhg = sim->respiratory.paco2_mmhg;

    /* Renal compensation: HCO3 adjusts over hours-days */
    float target_hco3 = PHYSIO_NORMAL_HCO3_MEQ_L;
    if (ab->pco2_mmhg > 45.0f) {
        /* Respiratory acidosis: kidneys retain HCO3 */
        target_hco3 = PHYSIO_NORMAL_HCO3_MEQ_L +
                       0.35f * (ab->pco2_mmhg - 40.0f);
    } else if (ab->pco2_mmhg < 35.0f) {
        /* Respiratory alkalosis: kidneys excrete HCO3 */
        target_hco3 = PHYSIO_NORMAL_HCO3_MEQ_L -
                       0.5f * (40.0f - ab->pco2_mmhg);
    }

    ab->hco3_meq_l += (target_hco3 - ab->hco3_meq_l) * 0.005f * dt;
    ab->hco3_meq_l = clampf(ab->hco3_meq_l, 5.0f, 45.0f);

    /* Henderson-Hasselbalch */
    ab->blood_ph = physiology_henderson_hasselbalch(ab->hco3_meq_l, ab->pco2_mmhg);
    ab->blood_ph = clampf(ab->blood_ph, 6.8f, 7.8f);

    /* Base excess: BE = HCO3 - 24.4 + 16.2 * (pH - 7.4) */
    ab->base_excess = ab->hco3_meq_l - 24.4f + 16.2f * (ab->blood_ph - 7.4f);

    /* Anion gap: Na - (Cl + HCO3), normal 8-12 */
    float na = sim->renal.serum_sodium;
    float cl = na - ab->hco3_meq_l - 10.0f; /* approximate Cl from Na and AG */
    ab->anion_gap = na - (cl + ab->hco3_meq_l);

    /* Classify */
    ab->status = physiology_classify_acidbase(ab->blood_ph, ab->pco2_mmhg, ab->hco3_meq_l);

    return 0;
}

/* ============================================================================
 * Main Step
 * ============================================================================ */

int physiology_step(physiology_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;

    physiology_step_cardiovascular(sim, dt);
    physiology_step_respiratory(sim, dt);
    physiology_step_renal(sim, dt);
    physiology_step_endocrine(sim, dt);
    physiology_step_thermoregulation(sim, dt);
    physiology_step_acidbase(sim, dt);

    sim->sim_time_min += dt;

    /* Update stats */
    sim->stats.step_count++;
    sim->stats.cardiac_output = sim->cardiovascular.cardiac_output;
    sim->stats.blood_ph = sim->acidbase.blood_ph;
    sim->stats.core_temperature = sim->thermoreg.core_temp_c;
    sim->stats.gfr = sim->renal.gfr_ml_min;
    sim->stats.blood_glucose = sim->endocrine.blood_glucose_mg_dl;
    sim->stats.sao2 = sim->respiratory.sao2;
    sim->stats.map = sim->cardiovascular.map_mmhg;

    /* Check homeostasis */
    sim->stats.homeostasis_maintained =
        (sim->acidbase.blood_ph >= 7.35f && sim->acidbase.blood_ph <= 7.45f) &&
        (sim->thermoreg.core_temp_c >= 36.0f && sim->thermoreg.core_temp_c <= 38.0f) &&
        (sim->endocrine.blood_glucose_mg_dl >= 70.0f &&
         sim->endocrine.blood_glucose_mg_dl <= 140.0f) &&
        (sim->respiratory.sao2 >= 0.90f) &&
        (sim->cardiovascular.map_mmhg >= 65.0f && sim->cardiovascular.map_mmhg <= 110.0f);

    return 0;
}

/* ============================================================================
 * Preset: Healthy Adult at Rest
 * ============================================================================ */

void physiology_load_healthy_adult(physiology_sim_t* sim) {
    if (!sim) return;

    /* Cardiovascular */
    sim->cardiovascular.heart_rate = PHYSIO_RESTING_HR;
    sim->cardiovascular.stroke_volume_ml = PHYSIO_RESTING_SV_ML;
    sim->cardiovascular.cardiac_output = PHYSIO_RESTING_CO_L_MIN;
    sim->cardiovascular.map_mmhg = PHYSIO_MAP_NORMAL_MMHG;
    sim->cardiovascular.systolic_mmhg = PHYSIO_SYSTOLIC_NORMAL;
    sim->cardiovascular.diastolic_mmhg = PHYSIO_DIASTOLIC_NORMAL;
    sim->cardiovascular.tpr = PHYSIO_TPR_NORMAL;
    sim->cardiovascular.ejection_fraction = PHYSIO_EJECTION_FRACTION;
    sim->cardiovascular.preload = 120.0f; /* EDV mL */
    sim->cardiovascular.blood_volume_l = PHYSIO_BLOOD_VOLUME_L;
    sim->cardiovascular.venous_return_l_min = PHYSIO_RESTING_CO_L_MIN;

    /* Respiratory */
    sim->respiratory.resp_rate = PHYSIO_RESP_RATE;
    sim->respiratory.tidal_volume_ml = PHYSIO_TIDAL_VOLUME_ML;
    sim->respiratory.minute_ventilation = PHYSIO_MINUTE_VENTILATION;
    sim->respiratory.pao2_mmhg = PHYSIO_PAO2_NORMAL;
    sim->respiratory.paco2_mmhg = PHYSIO_PACO2_NORMAL;
    sim->respiratory.pvo2_mmhg = PHYSIO_PVO2_NORMAL;
    sim->respiratory.sao2 = 0.98f;
    sim->respiratory.svo2 = 0.75f;
    sim->respiratory.vq_ratio = PHYSIO_VQ_RATIO_NORMAL;
    sim->respiratory.fio2 = 0.21f;

    /* Renal */
    sim->renal.gfr_ml_min = PHYSIO_GFR_NORMAL_ML_MIN;
    sim->renal.rpf_ml_min = PHYSIO_RPF_ML_MIN;
    sim->renal.filtration_fraction = PHYSIO_FILTRATION_FRACTION;
    sim->renal.urine_output_ml_min = PHYSIO_URINE_OUTPUT_ML_MIN;
    sim->renal.serum_creatinine = 1.0f;
    sim->renal.bun = 15.0f;
    sim->renal.serum_sodium = 140.0f;
    sim->renal.serum_potassium = 4.2f;

    /* Endocrine */
    sim->endocrine.blood_glucose_mg_dl = PHYSIO_GLUCOSE_FASTING_MG_DL;
    sim->endocrine.insulin_sensitivity = 0.8f;
    sim->endocrine.hormones[PHYSIO_HORMONE_INSULIN].concentration = PHYSIO_INSULIN_FASTING_UU_ML;
    sim->endocrine.hormones[PHYSIO_HORMONE_INSULIN].set_point = PHYSIO_INSULIN_FASTING_UU_ML;
    sim->endocrine.hormones[PHYSIO_HORMONE_GLUCAGON].concentration = 5.0f;
    sim->endocrine.hormones[PHYSIO_HORMONE_CORTISOL].concentration = PHYSIO_CORTISOL_NORMAL_UG_DL;
    sim->endocrine.hormones[PHYSIO_HORMONE_TSH].concentration = PHYSIO_TSH_NORMAL_MU_ML;
    sim->endocrine.hormones[PHYSIO_HORMONE_T4].concentration = PHYSIO_T4_NORMAL_UG_DL;
    sim->endocrine.hormones[PHYSIO_HORMONE_ADH].concentration = 2.0f;
    sim->endocrine.hormones[PHYSIO_HORMONE_ALDOSTERONE].concentration = 5.0f;
    sim->endocrine.hormones[PHYSIO_HORMONE_EPINEPHRINE].concentration = 0.5f;

    /* Thermoregulation */
    sim->thermoreg.core_temp_c = PHYSIO_CORE_TEMP_C;
    sim->thermoreg.skin_temp_c = 33.0f;
    sim->thermoreg.ambient_temp_c = 22.0f;
    sim->thermoreg.metabolic_heat_w = PHYSIO_BASAL_METABOLIC_RATE_W;

    /* Acid-base */
    sim->acidbase.blood_ph = PHYSIO_NORMAL_PH;
    sim->acidbase.hco3_meq_l = PHYSIO_NORMAL_HCO3_MEQ_L;
    sim->acidbase.pco2_mmhg = PHYSIO_NORMAL_PCO2_MMHG;
    sim->acidbase.base_excess = 0.0f;
    sim->acidbase.anion_gap = 10.0f;
    sim->acidbase.status = PHYSIO_ACIDBASE_NORMAL;

    /* Blood */
    sim->blood.hemoglobin_g_dl = 14.5f;
    sim->blood.hematocrit = 0.43f;
    sim->blood.wbc_per_ul = 7000.0f;
    sim->blood.platelets_per_ul = 250000.0f;
    sim->blood.plasma_protein_g_dl = 7.0f;

    LOG_INFO(LOG_TAG, "Loaded healthy adult at rest: HR=%.0f, BP=%.0f/%.0f, "
             "SpO2=%.0f%%, pH=%.2f, Gluc=%.0f, Temp=%.1fC",
             sim->cardiovascular.heart_rate,
             sim->cardiovascular.systolic_mmhg,
             sim->cardiovascular.diastolic_mmhg,
             sim->respiratory.sao2 * 100.0f,
             sim->acidbase.blood_ph,
             sim->endocrine.blood_glucose_mg_dl,
             sim->thermoreg.core_temp_c);
}
