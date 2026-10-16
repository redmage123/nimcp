/**
 * @file nimcp_graduated_autonomy.c
 * @brief Graduated Autonomy Implementation
 * @version 1.0.0
 * @date 2026-02-01
 */

#include "security/nimcp_graduated_autonomy.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "utils/memory/nimcp_memory.h"

#define LOG_CATEGORY "graduated_autonomy"

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/* Forward declaration for health agent */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/exception/nimcp_exception_macros.h"

BRIDGE_BOILERPLATE_MESH_ONLY(autonomy, MESH_ADAPTER_CATEGORY_SECURITY)


struct graduated_autonomy {
    uint32_t magic;
    nimcp_mutex_t* mutex;
    graduated_autonomy_config_t config;
    autonomy_domain_t domains[AUTONOMY_MAX_DOMAINS];
    size_t domain_count;
    graduated_autonomy_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_connected;

    /* Brain immune integration */
    void* brain_immune;
};

static bool is_valid_handle(const graduated_autonomy_t* system) {
    return system != NULL && system->magic == GRADUATED_AUTONOMY_MAGIC;
}

static void safe_strcpy(char* dest, const char* src, size_t max_len) {
    if (dest == NULL || max_len == 0) return;
    if (src == NULL) { dest[0] = '\0'; return; }
    size_t len = strlen(src);
    if (len >= max_len) len = max_len - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
}

static autonomy_domain_t* find_domain(graduated_autonomy_t* system, const char* name) {
    for (size_t i = 0; i < system->domain_count; i++) {
        if (strcmp(system->domains[i].name, name) == 0) {
            return &system->domains[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_domain: validation failed");
    return NULL;
}

static autonomy_domain_t* get_or_create_domain(
    graduated_autonomy_t* system,
    const char* name)
{
    autonomy_domain_t* domain = find_domain(system, name);
    if (domain) return domain;

    if (system->domain_count >= AUTONOMY_MAX_DOMAINS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "get_or_create_domain: capacity exceeded");
        return NULL;
    }

    domain = &system->domains[system->domain_count++];
    memset(domain, 0, sizeof(*domain));
    safe_strcpy(domain->name, name, AUTONOMY_DOMAIN_NAME_MAX);
    domain->level = system->config.default_level;
    domain->trust_alpha = 1.0f;  /* Beta(1,1) prior = uniform */
    domain->trust_beta = 1.0f;

    return domain;
}

graduated_autonomy_config_t graduated_autonomy_default_config(void) {
    graduated_autonomy_config_t config;
    memset(&config, 0, sizeof(config));
    config.default_level = AUTONOMY_SUGGEST;
    config.actions_required_for_upgrade = 100;
    config.violations_for_downgrade = 3;
    config.min_trust_for_upgrade = 0.9f;
    config.allow_self_upgrade_request = false;
    return config;
}

graduated_autonomy_t* graduated_autonomy_create(
    const graduated_autonomy_config_t* config)
{
    graduated_autonomy_t* system = nimcp_calloc(1, sizeof(graduated_autonomy_t));
    if (system == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "graduated_autonomy_create: validation failed");
        return NULL;
    }

    system->mutex = nimcp_mutex_create(NULL);
    if (system->mutex == NULL) { nimcp_free(system); return NULL; }

    if (config) memcpy(&system->config, config, sizeof(*config));
    else system->config = graduated_autonomy_default_config();

    system->magic = GRADUATED_AUTONOMY_MAGIC;
    NIMCP_LOG_INFO(LOG_CATEGORY, "Graduated autonomy system created");
    return system;
}

void graduated_autonomy_destroy(graduated_autonomy_t* system) {
    if (!is_valid_handle(system)) return;

    /* Unregister from bio-async */
    if (system->bio_async_connected && system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
        system->bio_async_connected = false;
    }

    system->magic = 0;
    if (system->mutex) nimcp_mutex_destroy(system->mutex);
    nimcp_free(system);
    NIMCP_LOG_INFO(LOG_CATEGORY, "Graduated autonomy system destroyed");
}

nimcp_error_t graduated_autonomy_update_trust(
    graduated_autonomy_t* system,
    const char* domain,
    bool action_was_aligned)
{
    if (!is_valid_handle(system) || domain == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "graduated_autonomy: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);

    autonomy_domain_t* d = get_or_create_domain(system, domain);
    if (d == NULL) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "graduated_autonomy: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    /* Bayesian beta-binomial update */
    if (action_was_aligned) {
        d->trust_alpha += 1.0f;
        d->actions_without_violation++;
    } else {
        d->trust_beta += 1.0f;
        d->actions_without_violation = 0;
    }
    d->total_actions++;

    /* Check for level upgrade */
    float trust_mean = d->trust_alpha / (d->trust_alpha + d->trust_beta);
    if (trust_mean >= system->config.min_trust_for_upgrade &&
        d->actions_without_violation >= system->config.actions_required_for_upgrade &&
        d->level < AUTONOMY_SUPERVISED) {
        d->level++;
        system->stats.level_upgrades++;
        NIMCP_LOG_INFO(LOG_CATEGORY, "Domain '%s' upgraded to level %d",
            domain, d->level);
    }

    /* Check for downgrade on violations */
    if (!action_was_aligned) {
        uint32_t recent_violations = (uint32_t)(d->trust_beta - 1.0f);
        if (recent_violations >= system->config.violations_for_downgrade &&
            d->level > AUTONOMY_NONE) {
            d->level--;
            system->stats.level_downgrades++;
            NIMCP_LOG_WARN(LOG_CATEGORY, "Domain '%s' downgraded to level %d",
                domain, d->level);
        }
    }

    system->stats.trust_updates++;

    nimcp_mutex_unlock(system->mutex);
    return NIMCP_OK;
}

autonomy_level_t graduated_autonomy_get_level(
    graduated_autonomy_t* system,
    const char* domain)
{
    if (!is_valid_handle(system) || domain == NULL) {
        return AUTONOMY_NONE;
    }

    nimcp_mutex_lock(system->mutex);
    autonomy_domain_t* d = find_domain(system, domain);
    autonomy_level_t level = d ? d->level : system->config.default_level;
    nimcp_mutex_unlock(system->mutex);

    return level;
}

nimcp_error_t graduated_autonomy_request_increase(
    graduated_autonomy_t* system,
    const char* domain,
    const char* justification,
    bool* granted)
{
    if (!is_valid_handle(system) || granted == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "graduated_autonomy: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);

    /* Self-upgrade requests require explicit permission */
    if (!system->config.allow_self_upgrade_request) {
        *granted = false;
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_OK;
    }

    autonomy_domain_t* d = get_or_create_domain(system, domain);
    if (d == NULL) {
        *granted = false;
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "graduated_autonomy: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    float trust_mean = d->trust_alpha / (d->trust_alpha + d->trust_beta);
    *granted = (trust_mean >= system->config.min_trust_for_upgrade &&
                d->level < AUTONOMY_SUPERVISED);

    if (*granted) {
        d->level++;
        system->stats.level_upgrades++;
        NIMCP_LOG_INFO(LOG_CATEGORY, "Autonomy increase granted for '%s': %s",
            domain, justification);
    }

    nimcp_mutex_unlock(system->mutex);
    return NIMCP_OK;
}

nimcp_error_t graduated_autonomy_get_trust(
    const graduated_autonomy_t* system,
    const char* domain,
    float* trust_mean,
    float* trust_variance)
{
    if (!is_valid_handle(system) || domain == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "graduated_autonomy: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    graduated_autonomy_t* mutable_system = (graduated_autonomy_t*)system;
    nimcp_mutex_lock(mutable_system->mutex);

    autonomy_domain_t* d = find_domain(mutable_system, domain);
    if (d == NULL) {
        /* Return default prior Beta(1,1) values for unknown domains */
        if (trust_mean) *trust_mean = 0.5f;
        if (trust_variance) *trust_variance = 0.25f;
        nimcp_mutex_unlock(mutable_system->mutex);
        return NIMCP_OK;  /* Unknown domain uses default prior */
    }

    float a = d->trust_alpha;
    float b = d->trust_beta;
    if (trust_mean) *trust_mean = a / (a + b);
    if (trust_variance) *trust_variance = (a * b) / ((a + b) * (a + b) * (a + b + 1));

    nimcp_mutex_unlock(mutable_system->mutex);
    return NIMCP_OK;
}

nimcp_error_t graduated_autonomy_get_stats(
    const graduated_autonomy_t* system,
    graduated_autonomy_stats_t* stats)
{
    if (!is_valid_handle(system) || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "graduated_autonomy: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    graduated_autonomy_t* mutable_system = (graduated_autonomy_t*)system;
    nimcp_mutex_lock(mutable_system->mutex);
    memcpy(stats, &system->stats, sizeof(*stats));
    nimcp_mutex_unlock(mutable_system->mutex);

    return NIMCP_OK;
}

nimcp_error_t graduated_autonomy_connect_bio_async(graduated_autonomy_t* system) {
    if (!is_valid_handle(system)) return NIMCP_ERROR_INVALID_ARGUMENT;

    nimcp_mutex_lock(system->mutex);

    if (system->bio_async_connected) {
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_OK;
    }

    bio_module_info_t module_info = {
        .module_id = BIO_MODULE_GRADUATED_AUTONOMY,
        .module_name = "graduated_autonomy",
        .inbox_capacity = 0,
        .user_data = system
    };

    system->bio_ctx = bio_router_register_module(&module_info);
    if (!system->bio_ctx) {
        NIMCP_LOG_WARN(LOG_CATEGORY, "Bio-async registration failed - continuing without");
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_OK;
    }

    system->bio_async_connected = true;
    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Connected to bio-async messaging");
    return NIMCP_OK;
}

/* ============================================================================
 * Brain Immune Integration
 * ============================================================================ */

nimcp_error_t graduated_autonomy_connect_brain_immune(
    graduated_autonomy_t* system,
    struct brain_immune* brain_immune)
{
    if (!is_valid_handle(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "graduated_autonomy: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);
    system->brain_immune = brain_immune;
    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Connected to brain immune system");
    return NIMCP_OK;
}

const char* graduated_autonomy_level_name(autonomy_level_t level) {
    switch (level) {
        case AUTONOMY_NONE:         return "none";
        case AUTONOMY_SUGGEST:      return "suggest";
        case AUTONOMY_BOUNDED:      return "bounded";
        case AUTONOMY_SUPERVISED:   return "supervised";
        case AUTONOMY_TRUSTED:      return "trusted";
        default:                    return "unknown";
    }
}
