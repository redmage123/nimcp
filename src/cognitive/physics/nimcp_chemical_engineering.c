/**
 * @file nimcp_chemical_engineering.c
 * @brief Chemical Engineering simulator — reactors, heat exchange, PID control
 *
 * Implements CSTR, PFR, and batch reactor design equations, LMTD and
 * effectiveness-NTU heat exchanger calculations, PID controller,
 * Damkohler number, and full step function updating conversion,
 * heat duty, and control outputs.
 */

#include "cognitive/physics/nimcp_chemical_engineering.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_TAG "CHEME"

#define CONC_EPSILON    1e-12f
#define TEMP_EPSILON    0.01f
#define VOL_EPSILON     1e-10f
#define R_GAS           8.314f   /* J/(mol*K) */

/* ============================================================================
 * Default config
 * ============================================================================ */

cheme_config_t chemical_engineering_default_config(void)
{
    cheme_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dt                   = 0.1f;
    cfg.ambient_temperature  = 298.15f;
    cfg.enable_heat_exchange = true;
    cfg.enable_pid_control   = true;
    return cfg;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

chemical_engineering_sim_t* chemical_engineering_create(const cheme_config_t* config)
{
    chemical_engineering_sim_t* sim =
        (chemical_engineering_sim_t*)nimcp_calloc(1, sizeof(chemical_engineering_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate chemical_engineering_sim_t");
        return NULL;
    }
    sim->config = config ? *config : chemical_engineering_default_config();
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Chemical engineering sim created");
    return sim;
}

void chemical_engineering_destroy(chemical_engineering_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Destroying chemical engineering sim (steps=%lu)",
             (unsigned long)sim->stats.step_count);
    nimcp_free(sim);
}

/* ============================================================================
 * Add entities
 * ============================================================================ */

uint32_t chemical_engineering_add_reactor(chemical_engineering_sim_t* sim,
                                           const cheme_reactor_t* r)
{
    if (!sim || !r) return UINT32_MAX;
    if (sim->num_reactors >= CHEME_MAX_REACTORS) {
        LOG_WARN(LOG_TAG, "Max reactors reached (%d)", CHEME_MAX_REACTORS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_reactors++;
    sim->reactors[idx] = *r;
    sim->reactors[idx].id = idx;
    sim->reactors[idx].active = true;
    return idx;
}

uint32_t chemical_engineering_add_exchanger(chemical_engineering_sim_t* sim,
                                             const cheme_heat_exchanger_t* hx)
{
    if (!sim || !hx) return UINT32_MAX;
    if (sim->num_exchangers >= CHEME_MAX_EXCHANGERS) {
        LOG_WARN(LOG_TAG, "Max exchangers reached (%d)", CHEME_MAX_EXCHANGERS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_exchangers++;
    sim->exchangers[idx] = *hx;
    sim->exchangers[idx].id = idx;
    sim->exchangers[idx].active = true;
    return idx;
}

uint32_t chemical_engineering_add_controller(chemical_engineering_sim_t* sim,
                                              const cheme_pid_controller_t* pid)
{
    if (!sim || !pid) return UINT32_MAX;
    if (sim->num_controllers >= CHEME_MAX_CONTROLLERS) {
        LOG_WARN(LOG_TAG, "Max controllers reached (%d)", CHEME_MAX_CONTROLLERS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_controllers++;
    sim->controllers[idx] = *pid;
    sim->controllers[idx].id = idx;
    sim->controllers[idx].active = true;
    return idx;
}

/* ============================================================================
 * CSTR design equation
 * tau = C_A0 * X / (-r_A)
 * For nth order: -r_A = k * C_A^n = k * (C_A0 * (1-X))^n
 * V = v0 * tau
 * ============================================================================ */

float cheme_cstr_volume(float feed_rate, float C_A0, float conversion, float rate)
{
    if (fabsf(rate) < CONC_EPSILON) return 1e30f;
    float tau = C_A0 * conversion / fabsf(rate);
    return feed_rate * tau;
}

/* ============================================================================
 * PFR design equation
 * V = F_A0 * integral(dX / (-r_A)) from 0 to X
 * For 1st order: -r_A = k*C_A0*(1-X), integral = -ln(1-X)/k
 * For 2nd order: -r_A = k*C_A0^2*(1-X)^2, integral = X/(k*C_A0*(1-X))
 * ============================================================================ */

float cheme_pfr_volume(float feed_rate, float C_A0, float conversion,
                        float k, uint32_t order)
{
    if (k < CONC_EPSILON || C_A0 < CONC_EPSILON) return 0.0f;
    if (conversion >= 1.0f) conversion = 0.999f;
    if (conversion <= 0.0f) return 0.0f;

    float F_A0 = feed_rate * C_A0;
    float integral = 0.0f;

    switch (order) {
    case 0:
        /* Zero order: -r_A = k, integral = X*C_A0/k */
        integral = conversion * C_A0 / k;
        break;
    case 1:
        /* First order: integral = -ln(1-X) / k */
        integral = -logf(1.0f - conversion) / k;
        break;
    case 2:
        /* Second order: integral = X / (k*C_A0*(1-X)) */
        integral = conversion / (k * C_A0 * (1.0f - conversion));
        break;
    default:
        /* General: numerical approximation using Simpson's rule */
        {
            int N = 100;
            float h = conversion / (float)N;
            float sum = 0.0f;
            for (int i = 0; i <= N; i++) {
                float Xi = (float)i * h;
                float C_A = C_A0 * (1.0f - Xi);
                float r_A = k * powf(C_A, (float)order);
                if (r_A < CONC_EPSILON) r_A = CONC_EPSILON;
                float w = (i == 0 || i == N) ? 1.0f :
                          (i % 2 == 0) ? 2.0f : 4.0f;
                sum += w / r_A;
            }
            integral = (h / 3.0f) * sum * C_A0;
        }
        break;
    }

    return F_A0 * integral;
}

/* ============================================================================
 * Batch reactor time
 * t = C_A0 * integral(dX / (-r_A)) from 0 to X
 * Same integrals as PFR but returns time instead of volume.
 * ============================================================================ */

float cheme_batch_time(float C_A0, float conversion, float k, uint32_t order)
{
    if (k < CONC_EPSILON || C_A0 < CONC_EPSILON) return 0.0f;
    if (conversion >= 1.0f) conversion = 0.999f;
    if (conversion <= 0.0f) return 0.0f;

    switch (order) {
    case 0:
        /* t = C_A0 * X / k */
        return C_A0 * conversion / k;
    case 1:
        /* t = -ln(1-X) / k */
        return -logf(1.0f - conversion) / k;
    case 2:
        /* t = X / (k * C_A0 * (1-X)) */
        return conversion / (k * C_A0 * (1.0f - conversion));
    default:
        /* Fall back to PFR integral / C_A0 approximation */
        return cheme_pfr_volume(1.0f, C_A0, conversion, k, order) / C_A0;
    }
}

/* ============================================================================
 * LMTD (Log Mean Temperature Difference)
 * LMTD = (dT1 - dT2) / ln(dT1/dT2)
 * Handles special case dT1 == dT2 (arithmetic mean).
 * ============================================================================ */

float cheme_lmtd(float dT1, float dT2)
{
    if (dT1 < TEMP_EPSILON && dT2 < TEMP_EPSILON) return 0.0f;
    if (dT1 < TEMP_EPSILON) return dT2;
    if (dT2 < TEMP_EPSILON) return dT1;

    float ratio = dT1 / dT2;
    /* When dT1 ~ dT2, use arithmetic mean to avoid 0/0 */
    if (fabsf(ratio - 1.0f) < 0.01f) {
        return (dT1 + dT2) / 2.0f;
    }
    return (dT1 - dT2) / logf(ratio);
}

/* ============================================================================
 * Effectiveness-NTU
 * For a single-pass counterflow heat exchanger:
 * epsilon = (1 - exp(-NTU*(1-Cr))) / (1 - Cr*exp(-NTU*(1-Cr)))
 * where Cr = Cmin/Cmax
 * Special case: Cr=0 (one fluid changes phase): epsilon = 1 - exp(-NTU)
 * ============================================================================ */

float cheme_effectiveness_ntu(float NTU, float Cr)
{
    if (NTU < 0.0f) NTU = 0.0f;
    if (Cr < 0.0f) Cr = 0.0f;
    if (Cr > 1.0f) Cr = 1.0f;

    if (Cr < 0.001f) {
        /* Phase change: epsilon = 1 - exp(-NTU) */
        return 1.0f - expf(-NTU);
    }

    if (fabsf(Cr - 1.0f) < 0.001f) {
        /* Balanced flow: epsilon = NTU/(1+NTU) */
        return NTU / (1.0f + NTU);
    }

    float exp_term = expf(-NTU * (1.0f - Cr));
    float numer = 1.0f - exp_term;
    float denom = 1.0f - Cr * exp_term;
    if (fabsf(denom) < 1e-10f) return 1.0f;
    return numer / denom;
}

/* ============================================================================
 * PID controller step
 * output = Kp*e + Ki*integral(e) + Kd*de/dt
 * ============================================================================ */

float cheme_pid_step(cheme_pid_controller_t* pid, float measurement, float dt)
{
    if (!pid || dt <= 0.0f) return 0.0f;

    float error = pid->setpoint - measurement;
    pid->measurement = measurement;

    /* Proportional term */
    float P = pid->Kp * error;

    /* Integral term with anti-windup */
    pid->integral_sum += error * dt;
    float I = pid->Ki * pid->integral_sum;

    /* Derivative term (on error, with filtering) */
    float de_dt = (error - pid->prev_error) / dt;
    float D = pid->Kd * de_dt;
    pid->prev_error = error;

    /* Total output */
    float output = P + I + D;

    /* Clamp output to limits */
    if (output < pid->output_min) {
        output = pid->output_min;
        /* Anti-windup: prevent integral from growing when saturated */
        if (error * pid->Ki > 0.0f) pid->integral_sum -= error * dt;
    }
    if (output > pid->output_max) {
        output = pid->output_max;
        if (error * pid->Ki < 0.0f) pid->integral_sum -= error * dt;
    }

    pid->output = output;
    return output;
}

/* ============================================================================
 * Damkohler number: Da = k * tau * C_A0^(n-1)
 * Da >> 1: reaction is fast relative to residence time
 * Da << 1: reaction is slow
 * ============================================================================ */

float cheme_damkohler(float k, float tau, float C_A0, uint32_t order)
{
    return k * tau * powf(C_A0, (float)(order - 1));
}

/* ============================================================================
 * Step — update reactors, heat exchangers, PID controllers
 * ============================================================================ */

int chemical_engineering_step(chemical_engineering_sim_t* sim, float dt)
{
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;
    if (dt <= 0.0f) dt = 0.1f;

    float total_production = 0.0f;
    float total_heat = 0.0f;
    float max_conv = 0.0f;
    float sum_sel = 0.0f;
    uint32_t n_active_reactors = 0;

    /* --- Phase 1: Update reactors --- */
    for (uint32_t i = 0; i < sim->num_reactors; i++) {
        cheme_reactor_t* r = &sim->reactors[i];
        if (!r->active) continue;
        n_active_reactors++;

        /* Compute space time */
        if (r->feed_rate > CONC_EPSILON && r->type != CHEME_REACTOR_BATCH) {
            r->space_time = r->volume / r->feed_rate;
        }

        /* Assume first-order reaction: -r_A = k * C_A where k ~ 0.1 s^-1 default */
        float k = 0.1f;  /* default rate constant */
        float C_A0 = r->concentrations[0];  /* component 0 is limiting reactant */
        float C_A = C_A0 * (1.0f - r->conversion);

        switch (r->type) {
        case CHEME_REACTOR_CSTR:
            /* Steady-state CSTR: X = tau*k / (1 + tau*k) for 1st order */
            if (r->space_time > 0.0f) {
                float Da = k * r->space_time;
                float X_ss = Da / (1.0f + Da);
                /* Approach steady state */
                r->conversion += (X_ss - r->conversion) * (1.0f - expf(-dt / r->space_time));
            }
            break;

        case CHEME_REACTOR_PFR:
        case CHEME_REACTOR_PBR:
            /* PFR: X = 1 - exp(-k*tau) for 1st order */
            if (r->space_time > 0.0f) {
                float X_ss = 1.0f - expf(-k * r->space_time);
                r->conversion += (X_ss - r->conversion) * (1.0f - expf(-dt / r->space_time));
            }
            break;

        case CHEME_REACTOR_BATCH:
            /* Batch: dX/dt = k*(1-X) for 1st order */
            {
                float dX = k * (1.0f - r->conversion) * dt;
                r->conversion += dX;
            }
            break;
        }

        /* Clamp conversion */
        if (r->conversion > 0.999f) r->conversion = 0.999f;
        if (r->conversion < 0.0f) r->conversion = 0.0f;

        /* Update concentration */
        r->concentrations[0] = C_A0 * (1.0f - r->conversion);

        /* Heat generated: Q = -dH_rxn * r * V */
        float heat_rxn = 50000.0f;  /* default -50 kJ/mol exothermic */
        float r_A = k * r->concentrations[0];
        r->heat_generated = heat_rxn * r_A * r->volume;

        /* Non-isothermal temperature change */
        if (!r->isothermal) {
            float Cp = 4180.0f;  /* water heat capacity J/(kg*K) */
            float mass_flow = r->feed_rate * 1000.0f;  /* kg/s (assume water density) */
            if (mass_flow > CONC_EPSILON) {
                float dT = r->heat_generated * dt / (mass_flow * Cp);
                r->temperature += dT;
            }
        }

        total_production += r->conversion * C_A0 * r->volume;
        total_heat += r->heat_generated * dt;
        if (r->conversion > max_conv) max_conv = r->conversion;
        sum_sel += r->selectivity;
    }

    /* --- Phase 2: Heat exchangers --- */
    if (sim->config.enable_heat_exchange) {
        for (uint32_t i = 0; i < sim->num_exchangers; i++) {
            cheme_heat_exchanger_t* hx = &sim->exchangers[i];
            if (!hx->active) continue;

            /* Compute LMTD */
            float dT1 = hx->T_hot_in - hx->T_cold_out;
            float dT2 = hx->T_hot_out - hx->T_cold_in;
            if (dT1 < 0.0f) dT1 = 0.0f;
            if (dT2 < 0.0f) dT2 = 0.0f;
            hx->lmtd = cheme_lmtd(dT1, dT2);

            /* Q = U * A * LMTD */
            hx->duty = hx->U * hx->area * hx->lmtd;
            total_heat += hx->duty * dt;
        }
    }

    /* --- Phase 3: PID controllers --- */
    if (sim->config.enable_pid_control) {
        for (uint32_t i = 0; i < sim->num_controllers; i++) {
            cheme_pid_controller_t* pid = &sim->controllers[i];
            if (!pid->active) continue;
            cheme_pid_step(pid, pid->measurement, dt);
        }
    }

    /* --- Update stats --- */
    sim->stats.step_count++;
    sim->stats.total_production += total_production * dt;
    sim->stats.total_heat_duty += total_heat;
    sim->stats.max_conversion = max_conv;
    sim->stats.avg_selectivity = n_active_reactors > 0
        ? sum_sel / (float)n_active_reactors : 0.0f;
    sim->time += dt;

    return 0;
}

/* ============================================================================
 * Stats
 * ============================================================================ */

cheme_stats_t chemical_engineering_get_stats(const chemical_engineering_sim_t* sim)
{
    cheme_stats_t zero;
    memset(&zero, 0, sizeof(zero));
    if (!sim) return zero;
    return sim->stats;
}
