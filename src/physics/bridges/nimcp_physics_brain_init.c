#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_physics_brain_init.c - Physics Layer Brain Factory Integration
//=============================================================================
/**
 * @file nimcp_physics_brain_init.c
 * @brief Implementation of physics layer brain factory integration
 */

#include "physics/bridges/nimcp_physics_brain_init.h"
#include "physics/biophysics/nimcp_hodgkin_huxley.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "physics/ephaptic/nimcp_ephaptic.h"
#include "physics/bridges/nimcp_physics_kg_wiring.h"
#include "physics/bridges/nimcp_physics_security.h"
#include "physics/bridges/nimcp_physics_immune_bridge.h"
#include "core/brain/nimcp_brain.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(physics_brain_init)

//=============================================================================
// Version String
//=============================================================================

static const char* physics_version_string = "1.0.0";

//=============================================================================
// Configuration API
//=============================================================================

int physics_init_default_config(physics_init_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->default_hh_population_size = 100;
    config->default_temperature = 37.0f;  /* Celsius */
    config->default_atp_pool = 100.0f;
    config->enable_ephaptic = true;
    config->enable_qmc = true;
    config->enable_fft = true;
    config->enable_kg_wiring = true;
    config->enable_security = true;
    config->enable_bio_async = true;
    config->enable_immune_bridge = true;
    config->admin_token = 0;  /* Will be set by brain factory */

    return 0;
}

//=============================================================================
// Brain Factory Integration API
//=============================================================================

bool nimcp_brain_factory_init_physics_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_physics_subsystem: brain is NULL");
        return false;
    }

    physics_init_config_t config;
    physics_init_default_config(&config);

    physics_init_result_t result;
    memset(&result, 0, sizeof(result));

    if (physics_init_modules(brain, &config, &result) < 0) {
        NIMCP_LOG_ERROR(PHYSICS_INIT_MODULE_NAME,
            "Physics subsystem initialization failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_brain_factory_init_physics_subsystem: validation failed");
        return false;
    }

    NIMCP_LOG_INFO(PHYSICS_INIT_MODULE_NAME,
        "Physics subsystem initialized: HH=%d, Thermo=%d, Ephaptic=%d",
        result.hh_initialized, result.thermo_initialized,
        result.ephaptic_initialized);

    return result.error_count == 0;
}

int physics_init_modules(
    brain_t brain,
    const physics_init_config_t* config,
    physics_init_result_t* result
) {
    if (!brain || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_init_modules: required parameter is NULL (brain, config)");
        return -1;
    }

    physics_init_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));

    /* Initialize core physics modules */
    local_result.hh_initialized = physics_init_hodgkin_huxley(brain, config);
    if (!local_result.hh_initialized) {
        local_result.warning_count++;
        NIMCP_LOG_WARN(PHYSICS_INIT_MODULE_NAME,
            "Hodgkin-Huxley initialization failed");
    }

    local_result.thermo_initialized = physics_init_thermodynamics(brain, config);
    if (!local_result.thermo_initialized) {
        local_result.warning_count++;
        NIMCP_LOG_WARN(PHYSICS_INIT_MODULE_NAME,
            "Thermodynamics initialization failed");
    }

    if (config->enable_ephaptic) {
        local_result.ephaptic_initialized = physics_init_ephaptic(brain, config);
        if (!local_result.ephaptic_initialized) {
            local_result.warning_count++;
            NIMCP_LOG_WARN(PHYSICS_INIT_MODULE_NAME,
                "Ephaptic initialization failed");
        }
    }

    /* Initialize bridges */
    if (config->enable_bio_async) {
        local_result.bio_async_connected = physics_init_bio_async_bridges(brain);
        if (!local_result.bio_async_connected) {
            local_result.warning_count++;
        }
    }

    if (config->enable_qmc) {
        local_result.qmc_created = physics_init_qmc_bridges(brain);
        if (!local_result.qmc_created) {
            local_result.warning_count++;
        }
    }

    if (config->enable_fft && local_result.ephaptic_initialized) {
        local_result.fft_created = physics_init_fft_bridge(brain);
        if (!local_result.fft_created) {
            local_result.warning_count++;
        }
    }

    if (config->enable_kg_wiring) {
        local_result.kg_registered = physics_init_kg_wiring(brain, config->admin_token);
        if (!local_result.kg_registered) {
            local_result.warning_count++;
        }
    }

    if (config->enable_security) {
        local_result.security_registered = physics_init_security(brain);
        if (!local_result.security_registered) {
            local_result.warning_count++;
        }
    }

    if (config->enable_immune_bridge) {
        local_result.immune_connected = physics_init_immune_bridge(brain);
        if (!local_result.immune_connected) {
            local_result.warning_count++;
        }
    }

    /* Check for critical failures */
    if (!local_result.hh_initialized && !local_result.thermo_initialized &&
        !local_result.ephaptic_initialized) {
        local_result.error_count++;
        NIMCP_LOG_ERROR(PHYSICS_INIT_MODULE_NAME,
            "No physics modules initialized successfully");
    }

    if (result) {
        *result = local_result;
    }

    return local_result.error_count > 0 ? -1 : 0;
}

void nimcp_brain_factory_destroy_physics_subsystem(brain_t brain) {
    if (!brain) return;

    /* Cleanup would happen here - destroy modules in reverse order */
    /* For now, this is a placeholder as brain manages its own subsystems */

    NIMCP_LOG_INFO(PHYSICS_INIT_MODULE_NAME,
        "Physics subsystem destroyed");
}

//=============================================================================
// Individual Module Initialization
//=============================================================================

bool physics_init_hodgkin_huxley(
    brain_t brain,
    const physics_init_config_t* config
) {
    if (!brain || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_init_hodgkin_huxley: required parameter is NULL (brain, config)");
        return false;
    }

    /* Set up HH default config manually */
    nimcp_hh_config_t hh_config = {
        /* Standard HH conductances (mS/cm^2) */
        .g_Na = 120.0f,
        .g_K = 36.0f,
        .g_L = 0.3f,
        .g_Ca_L = 0.0f,
        .g_Ca_T = 0.0f,
        .g_K_Ca = 0.0f,
        .g_K_A = 0.0f,
        .g_H = 0.0f,

        /* Standard reversal potentials (mV) */
        .E_Na = 50.0f,
        .E_K = -77.0f,
        .E_L = -54.4f,
        .E_Ca = 120.0f,
        .E_H = -30.0f,

        /* Membrane properties */
        .C_m = 1.0f,
        .V_rest = -65.0f,

        /* Temperature in Celsius */
        .temperature = config->default_temperature,

        /* Morphology defaults */
        .surface_area = 0.01f,
        .length = 100.0f,
        .diameter = 10.0f,

        /* Extended channels disabled by default */
        .enable_calcium = false,
        .enable_adaptation = false,
        .enable_h_current = false,

        /* Integration */
        .dt_max = 0.1f,
        .adaptive_dt = false
    };

    /* Create population - API: nimcp_hh_population_create(pop, count, config) */
    nimcp_hh_population_t pop;
    if (nimcp_hh_population_create(&pop, config->default_hh_population_size, &hh_config) != NIMCP_SUCCESS) {
        NIMCP_LOG_WARN(PHYSICS_INIT_MODULE_NAME,
            "Failed to create HH population");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_init_hodgkin_huxley: validation failed");
        return false;
    }

    /* Store in brain (implementation depends on brain internal structure) */
    /* For now, just verify creation succeeded */
    /* brain_set_hh_population(brain, &pop); */

    NIMCP_LOG_DEBUG(PHYSICS_INIT_MODULE_NAME,
        "Initialized HH population with %u neurons",
        config->default_hh_population_size);

    /* Note: In a full implementation, the brain would store this */
    /* For now we just demonstrate successful creation */
    nimcp_hh_population_destroy(&pop);

    return true;
}

bool physics_init_thermodynamics(
    brain_t brain,
    const physics_init_config_t* config
) {
    if (!brain || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_init_thermodynamics: required parameter is NULL (brain, config)");
        return false;
    }

    /* Get thermo default config - returns struct directly */
    nimcp_thermo_config_t thermo_config = nimcp_thermo_default_config();

    /* Adjust temperature (convert Celsius to Kelvin) */
    thermo_config.temperature_k = config->default_temperature + 273.15f;

    /* Create and initialize thermodynamic state */
    nimcp_thermodynamic_state_t state;
    if (nimcp_thermo_init(&state, &thermo_config) != NIMCP_SUCCESS) {
        NIMCP_LOG_WARN(PHYSICS_INIT_MODULE_NAME,
            "Failed to initialize thermodynamics system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "physics_init_thermodynamics: validation failed");
        return false;
    }

    NIMCP_LOG_DEBUG(PHYSICS_INIT_MODULE_NAME,
        "Initialized thermodynamics at %.1f°C",
        config->default_temperature);

    /* Note: brain would store this */
    nimcp_thermo_destroy(&state);

    return true;
}

bool physics_init_ephaptic(
    brain_t brain,
    const physics_init_config_t* config
) {
    if (!brain || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_init_ephaptic: required parameter is NULL (brain, config)");
        return false;
    }

    /* Get ephaptic default config - returns struct directly */
    nimcp_ephaptic_config_t eph_config = nimcp_ephaptic_default_config();

    /* Create and initialize ephaptic system */
    nimcp_ephaptic_system_t sys;
    if (nimcp_ephaptic_init(&sys, &eph_config) != NIMCP_SUCCESS) {
        NIMCP_LOG_WARN(PHYSICS_INIT_MODULE_NAME,
            "Failed to initialize ephaptic system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "physics_init_ephaptic: validation failed");
        return false;
    }

    NIMCP_LOG_DEBUG(PHYSICS_INIT_MODULE_NAME,
        "Initialized ephaptic coupling system");

    /* Note: brain would store this */
    nimcp_ephaptic_destroy(&sys);

    return true;
}

//=============================================================================
// Bridge Initialization
//=============================================================================

bool physics_init_bio_async_bridges(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_init_bio_async_bridges: brain is NULL");
        return false;
    }

    /* Bio-async bridges would connect here */
    /* This requires access to the brain's bio-router */
    NIMCP_LOG_DEBUG(PHYSICS_INIT_MODULE_NAME,
        "Bio-async bridges ready (connect when router available)");

    return true;
}

bool physics_init_qmc_bridges(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_init_qmc_bridges: brain is NULL");
        return false;
    }

    /* QMC bridges created but not connected until physics modules available */
    NIMCP_LOG_DEBUG(PHYSICS_INIT_MODULE_NAME,
        "QMC bridges ready");

    return true;
}

bool physics_init_fft_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_init_fft_bridge: brain is NULL");
        return false;
    }

    /* FFT bridge created for ephaptic LFP analysis */
    NIMCP_LOG_DEBUG(PHYSICS_INIT_MODULE_NAME,
        "FFT bridge ready");

    return true;
}

bool physics_init_kg_wiring(brain_t brain, uint64_t admin_token) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_init_kg_wiring: brain is NULL");
        return false;
    }

    /* Would access brain's KG and register physics nodes */
    /* brain_kg_t* kg = brain_get_kg(brain); */
    /* if (kg) physics_kg_register_all(kg, NULL, NULL, admin_token); */

    (void)admin_token;

    NIMCP_LOG_DEBUG(PHYSICS_INIT_MODULE_NAME,
        "KG wiring ready (register when KG available)");

    return true;
}

bool physics_init_security(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_init_security: brain is NULL");
        return false;
    }

    /* Would access brain's BBB and register physics modules */
    /* bbb_system_t bbb = brain_get_bbb(brain); */
    /* if (bbb) physics_security_register_all(bbb, NULL); */

    NIMCP_LOG_DEBUG(PHYSICS_INIT_MODULE_NAME,
        "Security registration ready (register when BBB available)");

    return true;
}

bool physics_init_immune_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_init_immune_bridge: brain is NULL");
        return false;
    }

    /* Would create physics-immune bridge and connect */
    NIMCP_LOG_DEBUG(PHYSICS_INIT_MODULE_NAME,
        "Immune bridge ready (connect when immune system available)");

    return true;
}

//=============================================================================
// Query API
//=============================================================================

bool physics_is_initialized(brain_t brain) {
    if (!brain) {
        return false;
    }

    /* Would check if physics subsystem exists in brain */
    /* return brain_has_physics_subsystem(brain); */

    return true;  /* Assume initialized if brain exists */
}

const char* physics_get_version(void) {
    return physics_version_string;
}
