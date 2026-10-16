/**
 * @file nimcp_physiology.h
 * @brief Physiology simulation engine for world model
 *
 * WHAT: Simulates organ system physiology: cardiovascular (Frank-Starling,
 *       Poiseuille), respiratory (gas exchange, Hill equation), renal
 *       (GFR, clearance), endocrine (hormone feedback), thermoregulation,
 *       and acid-base balance.
 * WHY:  Provides physiological reasoning for world model. Understanding
 *       how organ systems maintain homeostasis enables reasoning about
 *       health, disease, and biological function.
 * HOW:  Frank-Starling for cardiac output, Hill equation for O2-hemoglobin
 *       dissociation, Henderson-Hasselbalch for blood pH, negative feedback
 *       loops for hormone regulation, Poiseuille flow for hemodynamics.
 *
 * THEORETICAL FOUNDATION:
 *   - Cardiac: CO = HR * SV, MAP = CO * TPR
 *   - Poiseuille: Q = pi*r^4*dP / (8*mu*L)
 *   - Hill: SO2 = pO2^n / (P50^n + pO2^n), n=2.7, P50=26.6 mmHg
 *   - GFR: Kf * (P_gc - P_bc - pi_gc), normal ~125 mL/min
 *   - Henderson-Hasselbalch: pH = 6.1 + log([HCO3-] / (0.03 * pCO2))
 *   - Thermoregulation: core 37C, shiver <35C, sweat >37.5C
 */

#ifndef NIMCP_PHYSIOLOGY_H
#define NIMCP_PHYSIOLOGY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Real human physiology values
 * ============================================================================ */

/* Cardiovascular */
#define PHYSIO_RESTING_HR           72.0f   /* beats/min */
#define PHYSIO_RESTING_SV_ML        70.0f   /* stroke volume mL */
#define PHYSIO_RESTING_CO_L_MIN     5.0f    /* cardiac output L/min */
#define PHYSIO_MAP_NORMAL_MMHG      93.0f   /* mean arterial pressure */
#define PHYSIO_SYSTOLIC_NORMAL      120.0f  /* mmHg */
#define PHYSIO_DIASTOLIC_NORMAL     80.0f   /* mmHg */
#define PHYSIO_TPR_NORMAL           18.6f   /* mmHg*min/L (MAP/CO) */
#define PHYSIO_BLOOD_VISCOSITY_CP   3.5f    /* centipoise */
#define PHYSIO_BLOOD_VOLUME_L       5.0f    /* total blood volume */
#define PHYSIO_EJECTION_FRACTION    0.60f   /* normal EF */

/* Respiratory */
#define PHYSIO_TIDAL_VOLUME_ML      500.0f  /* mL per breath */
#define PHYSIO_RESP_RATE            15.0f   /* breaths/min */
#define PHYSIO_MINUTE_VENTILATION   7.5f    /* L/min */
#define PHYSIO_FRC_ML               2400.0f /* functional residual capacity */
#define PHYSIO_VITAL_CAPACITY_ML    4800.0f
#define PHYSIO_HILL_N               2.7f    /* Hill coefficient for Hb-O2 */
#define PHYSIO_P50_MMHG             26.6f   /* pO2 at 50% saturation */
#define PHYSIO_PAO2_NORMAL          100.0f  /* alveolar pO2 mmHg */
#define PHYSIO_PACO2_NORMAL         40.0f   /* alveolar pCO2 mmHg */
#define PHYSIO_PVO2_NORMAL          40.0f   /* venous pO2 mmHg */
#define PHYSIO_VQ_RATIO_NORMAL      0.8f    /* ventilation/perfusion */

/* Renal */
#define PHYSIO_GFR_NORMAL_ML_MIN    125.0f  /* glomerular filtration rate */
#define PHYSIO_KF_NORMAL            12.5f   /* mL/min/mmHg filtration coeff */
#define PHYSIO_PGC_MMHG             55.0f   /* glomerular capillary pressure */
#define PHYSIO_PBC_MMHG             15.0f   /* Bowman's capsule pressure */
#define PHYSIO_PI_GC_MMHG           30.0f   /* oncotic pressure */
#define PHYSIO_RPF_ML_MIN           660.0f  /* renal plasma flow */
#define PHYSIO_FILTRATION_FRACTION  0.19f   /* GFR/RPF */
#define PHYSIO_URINE_OUTPUT_ML_MIN  1.0f    /* normal ~1 mL/min */

/* Acid-base */
#define PHYSIO_NORMAL_PH            7.40f
#define PHYSIO_NORMAL_HCO3_MEQ_L    24.0f   /* mEq/L */
#define PHYSIO_NORMAL_PCO2_MMHG     40.0f   /* mmHg */
#define PHYSIO_HH_PKA               6.1f    /* Henderson-Hasselbalch pKa */
#define PHYSIO_CO2_SOLUBILITY       0.03f   /* mEq/L per mmHg */

/* Endocrine */
#define PHYSIO_INSULIN_FASTING_UU_ML 10.0f  /* uU/mL fasting insulin */
#define PHYSIO_GLUCOSE_FASTING_MG_DL 90.0f  /* mg/dL fasting glucose */
#define PHYSIO_GLUCOSE_SET_POINT     100.0f /* mg/dL target */
#define PHYSIO_CORTISOL_NORMAL_UG_DL 15.0f  /* ug/dL morning cortisol */
#define PHYSIO_TSH_NORMAL_MU_ML      2.0f   /* mU/mL */
#define PHYSIO_T4_NORMAL_UG_DL       8.0f   /* ug/dL */

/* Thermoregulation */
#define PHYSIO_CORE_TEMP_C           37.0f
#define PHYSIO_SHIVER_THRESHOLD_C    35.0f
#define PHYSIO_SWEAT_THRESHOLD_C     37.5f
#define PHYSIO_HYPOTHERMIA_C         35.0f
#define PHYSIO_HYPERTHERMIA_C        40.0f
#define PHYSIO_LETHAL_LOW_C          28.0f
#define PHYSIO_LETHAL_HIGH_C         42.0f
#define PHYSIO_BASAL_METABOLIC_RATE_W 80.0f /* watts at rest */

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    PHYSIO_SYSTEM_CARDIOVASCULAR = 0,
    PHYSIO_SYSTEM_RESPIRATORY,
    PHYSIO_SYSTEM_RENAL,
    PHYSIO_SYSTEM_ENDOCRINE,
    PHYSIO_SYSTEM_THERMOREGULATION,
    PHYSIO_SYSTEM_COUNT
} physio_system_type_t;

typedef enum {
    PHYSIO_HORMONE_INSULIN = 0,
    PHYSIO_HORMONE_GLUCAGON,
    PHYSIO_HORMONE_CORTISOL,
    PHYSIO_HORMONE_TSH,
    PHYSIO_HORMONE_T4,
    PHYSIO_HORMONE_T3,
    PHYSIO_HORMONE_ADH,         /* Antidiuretic hormone */
    PHYSIO_HORMONE_ALDOSTERONE,
    PHYSIO_HORMONE_EPINEPHRINE,
    PHYSIO_HORMONE_COUNT
} physio_hormone_type_t;

typedef enum {
    PHYSIO_ACIDBASE_NORMAL = 0,
    PHYSIO_ACIDBASE_RESP_ACIDOSIS,      /* High pCO2 -> low pH */
    PHYSIO_ACIDBASE_RESP_ALKALOSIS,     /* Low pCO2 -> high pH */
    PHYSIO_ACIDBASE_MET_ACIDOSIS,       /* Low HCO3 -> low pH */
    PHYSIO_ACIDBASE_MET_ALKALOSIS,      /* High HCO3 -> high pH */
    PHYSIO_ACIDBASE_MIXED,
    PHYSIO_ACIDBASE_COUNT
} physio_acidbase_status_t;

/* ============================================================================
 * Structs
 * ============================================================================ */

/** Cardiovascular state */
typedef struct {
    float   heart_rate;         /* beats/min */
    float   stroke_volume_ml;   /* mL */
    float   cardiac_output;     /* L/min = HR * SV / 1000 */
    float   map_mmhg;           /* mean arterial pressure */
    float   systolic_mmhg;
    float   diastolic_mmhg;
    float   tpr;                /* total peripheral resistance */
    float   ejection_fraction;  /* [0..1] */
    float   preload;            /* end-diastolic volume (mL) */
    float   afterload;          /* aortic pressure proxy */
    float   blood_volume_l;
    float   venous_return_l_min;
} physio_cardiovascular_t;

/** Respiratory state */
typedef struct {
    float   resp_rate;          /* breaths/min */
    float   tidal_volume_ml;
    float   minute_ventilation; /* L/min */
    float   pao2_mmhg;         /* alveolar O2 */
    float   paco2_mmhg;        /* alveolar CO2 */
    float   pvo2_mmhg;         /* venous O2 */
    float   sao2;              /* arterial O2 saturation [0..1] */
    float   svo2;              /* venous O2 saturation */
    float   vq_ratio;          /* ventilation/perfusion */
    float   fio2;              /* fraction inspired O2 (0.21 normal) */
} physio_respiratory_t;

/** Renal state */
typedef struct {
    float   gfr_ml_min;        /* glomerular filtration rate */
    float   rpf_ml_min;        /* renal plasma flow */
    float   filtration_fraction;
    float   urine_output_ml_min;
    float   serum_creatinine;  /* mg/dL (normal 0.7-1.3) */
    float   bun;               /* blood urea nitrogen mg/dL */
    float   serum_sodium;      /* mEq/L (normal 135-145) */
    float   serum_potassium;   /* mEq/L (normal 3.5-5.0) */
    float   free_water_clearance; /* mL/min */
} physio_renal_t;

/** Hormone state */
typedef struct {
    physio_hormone_type_t type;
    float   concentration;      /* current level */
    float   set_point;          /* homeostatic target */
    float   production_rate;    /* units/min */
    float   clearance_rate;     /* 1/min half-life */
    float   sensitivity;        /* receptor sensitivity [0..1] */
} physio_hormone_t;

/** Endocrine system */
typedef struct {
    physio_hormone_t hormones[PHYSIO_HORMONE_COUNT];
    float   blood_glucose_mg_dl;
    float   insulin_sensitivity; /* [0..1] */
    float   hpa_axis_activity;   /* hypothalamic-pituitary-adrenal [0..1] */
    float   thyroid_activity;    /* [0..1] */
} physio_endocrine_t;

/** Thermoregulation state */
typedef struct {
    float   core_temp_c;
    float   skin_temp_c;
    float   ambient_temp_c;
    float   metabolic_heat_w;       /* watts of heat production */
    float   shivering_intensity;    /* [0..1] */
    float   sweating_rate;          /* [0..1] */
    float   vasodilation;           /* [0..1] cutaneous blood flow */
    float   heat_loss_w;            /* watts of heat dissipation */
} physio_thermoregulation_t;

/** Acid-base balance */
typedef struct {
    float   blood_ph;
    float   hco3_meq_l;        /* bicarbonate */
    float   pco2_mmhg;
    float   base_excess;       /* mEq/L */
    float   anion_gap;         /* mEq/L */
    physio_acidbase_status_t status;
} physio_acidbase_t;

/** Blood parameters */
typedef struct {
    float   hemoglobin_g_dl;    /* g/dL (normal 12-17) */
    float   hematocrit;         /* [0..1] (normal 0.36-0.50) */
    float   wbc_per_ul;         /* white blood cells */
    float   platelets_per_ul;
    float   plasma_protein_g_dl; /* albumin + globulin */
} physio_blood_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    float   dt;                     /* time step in minutes */
    float   temperature_c;          /* ambient temperature */
    bool    enable_cardiovascular;
    bool    enable_respiratory;
    bool    enable_renal;
    bool    enable_endocrine;
    bool    enable_thermoregulation;
    float   activity_level;         /* [0..1] exercise intensity */
} physiology_config_t;

/* ============================================================================
 * Stats
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    float       cardiac_output;
    float       blood_ph;
    float       core_temperature;
    float       gfr;
    float       blood_glucose;
    float       sao2;
    float       map;
    bool        homeostasis_maintained;
} physiology_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct physiology_sim {
    physio_cardiovascular_t      cardiovascular;
    physio_respiratory_t         respiratory;
    physio_renal_t               renal;
    physio_endocrine_t           endocrine;
    physio_thermoregulation_t    thermoreg;
    physio_acidbase_t            acidbase;
    physio_blood_t               blood;
    physiology_config_t          config;
    physiology_stats_t           stats;
    float                        sim_time_min;
    bool                         initialized;
} physiology_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

physiology_sim_t* physiology_create(const physiology_config_t* config);
void physiology_destroy(physiology_sim_t* sim);
int physiology_step(physiology_sim_t* sim, float dt);
physiology_config_t physiology_default_config(void);
physiology_stats_t physiology_get_stats(const physiology_sim_t* sim);

/** Cardiovascular */
float physiology_frank_starling(float preload_ml);
float physiology_cardiac_output(float hr, float sv_ml);
float physiology_map(float co, float tpr);
float physiology_poiseuille_flow(float radius_cm, float dp_mmhg,
                                  float viscosity_cp, float length_cm);
int physiology_step_cardiovascular(physiology_sim_t* sim, float dt);

/** Respiratory */
float physiology_hill_equation(float po2, float p50, float n);
float physiology_alveolar_gas(float fio2, float paco2, float rq);
int physiology_step_respiratory(physiology_sim_t* sim, float dt);

/** Renal */
float physiology_gfr(float kf, float pgc, float pbc, float pi_gc);
float physiology_clearance(float urine_conc, float urine_flow, float plasma_conc);
float physiology_free_water_clearance(float urine_flow, float osmolar_clearance);
int physiology_step_renal(physiology_sim_t* sim, float dt);

/** Endocrine */
int physiology_step_endocrine(physiology_sim_t* sim, float dt);
int physiology_step_insulin_glucose(physiology_sim_t* sim, float dt);

/** Thermoregulation */
int physiology_step_thermoregulation(physiology_sim_t* sim, float dt);

/** Acid-base */
float physiology_henderson_hasselbalch(float hco3, float pco2);
physio_acidbase_status_t physiology_classify_acidbase(float ph, float pco2,
                                                       float hco3);
int physiology_step_acidbase(physiology_sim_t* sim, float dt);

/** Load preset: healthy adult at rest */
void physiology_load_healthy_adult(physiology_sim_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSIOLOGY_H */
