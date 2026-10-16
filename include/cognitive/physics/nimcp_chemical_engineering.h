/**
 * @file nimcp_chemical_engineering.h
 * @brief Chemical Engineering — reactors, mass transfer, heat exchange, process control
 *
 * CSTR/PFR/batch reactor design, mass balance, energy balance,
 * distillation, absorption, PID control.
 */

#ifndef NIMCP_CHEMICAL_ENGINEERING_H
#define NIMCP_CHEMICAL_ENGINEERING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHEME_MAX_REACTORS      16
#define CHEME_MAX_STREAMS       32
#define CHEME_MAX_SEPARATORS    8
#define CHEME_MAX_EXCHANGERS    8
#define CHEME_MAX_CONTROLLERS   8
#define CHEME_MAX_COMPONENTS    16
#define CHEME_MAX_NAME          32

typedef enum {
    CHEME_REACTOR_BATCH = 0,    /* well-mixed, no flow */
    CHEME_REACTOR_CSTR  = 1,    /* continuous stirred tank */
    CHEME_REACTOR_PFR   = 2,    /* plug flow reactor */
    CHEME_REACTOR_PBR   = 3,    /* packed bed reactor */
} cheme_reactor_type_t;

typedef struct {
    uint32_t        id;
    char            name[CHEME_MAX_NAME];
    cheme_reactor_type_t type;
    float           volume;             /* m³ */
    float           temperature;        /* K */
    float           pressure;           /* Pa */
    float           feed_rate;          /* m³/s (volumetric) */
    float           concentrations[CHEME_MAX_COMPONENTS]; /* mol/L per component */
    float           conversion;         /* X: fraction of limiting reactant consumed */
    float           selectivity;        /* desired product / total product */
    float           space_time;         /* τ = V/v₀ (s) */
    float           heat_generated;     /* W */
    bool            isothermal;
    bool            active;
} cheme_reactor_t;

typedef struct {
    float           flow_rate;          /* m³/s */
    float           temperature;        /* K */
    float           pressure;           /* Pa */
    float           concentrations[CHEME_MAX_COMPONENTS];
} cheme_stream_t;

typedef struct {
    uint32_t        id;
    float           duty;               /* W (heat transfer rate) */
    float           T_hot_in, T_hot_out;
    float           T_cold_in, T_cold_out;
    float           U;                  /* overall heat transfer coefficient W/(m²·K) */
    float           area;               /* m² */
    float           lmtd;              /* log mean temperature difference */
    bool            active;
} cheme_heat_exchanger_t;

typedef struct {
    uint32_t        id;
    float           setpoint;
    float           measurement;
    float           output;             /* control signal [0..1] */
    float           Kp, Ki, Kd;         /* PID gains */
    float           integral_sum;
    float           prev_error;
    float           output_min, output_max;
    bool            active;
} cheme_pid_controller_t;

typedef struct {
    float       dt;
    float       ambient_temperature;
    bool        enable_heat_exchange;
    bool        enable_pid_control;
} cheme_config_t;

typedef struct {
    uint64_t    step_count;
    float       total_production;       /* mol produced */
    float       total_heat_duty;        /* J total */
    float       max_conversion;
    float       avg_selectivity;
} cheme_stats_t;

typedef struct chemical_engineering_sim {
    cheme_reactor_t         reactors[CHEME_MAX_REACTORS];
    uint32_t                num_reactors;
    cheme_heat_exchanger_t  exchangers[CHEME_MAX_EXCHANGERS];
    uint32_t                num_exchangers;
    cheme_pid_controller_t  controllers[CHEME_MAX_CONTROLLERS];
    uint32_t                num_controllers;
    cheme_config_t          config;
    cheme_stats_t           stats;
    float                   time;
    bool                    initialized;
} chemical_engineering_sim_t;

chemical_engineering_sim_t* chemical_engineering_create(const cheme_config_t* config);
void chemical_engineering_destroy(chemical_engineering_sim_t* sim);
uint32_t chemical_engineering_add_reactor(chemical_engineering_sim_t* sim, const cheme_reactor_t* r);
uint32_t chemical_engineering_add_exchanger(chemical_engineering_sim_t* sim, const cheme_heat_exchanger_t* hx);
uint32_t chemical_engineering_add_controller(chemical_engineering_sim_t* sim, const cheme_pid_controller_t* pid);
int chemical_engineering_step(chemical_engineering_sim_t* sim, float dt);

/** CSTR design: τ = C_A0·X / (-r_A) */
float cheme_cstr_volume(float feed_rate, float C_A0, float conversion, float rate);
/** PFR design: V = F_A0 ∫(dX/-r_A) from 0 to X */
float cheme_pfr_volume(float feed_rate, float C_A0, float conversion, float k, uint32_t order);
/** Batch reactor time: t = C_A0 ∫(dX/-r_A) */
float cheme_batch_time(float C_A0, float conversion, float k, uint32_t order);
/** LMTD for heat exchangers */
float cheme_lmtd(float dT1, float dT2);
/** Effectiveness-NTU for heat exchangers */
float cheme_effectiveness_ntu(float NTU, float Cr);
/** PID control step */
float cheme_pid_step(cheme_pid_controller_t* pid, float measurement, float dt);
/** Damkohler number: Da = k·τ·C_A0^(n-1) */
float cheme_damkohler(float k, float tau, float C_A0, uint32_t order);

cheme_config_t chemical_engineering_default_config(void);
cheme_stats_t chemical_engineering_get_stats(const chemical_engineering_sim_t* sim);

#ifdef __cplusplus
}
#endif
#endif
