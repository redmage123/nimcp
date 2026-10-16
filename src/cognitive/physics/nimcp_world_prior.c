/**
 * @file nimcp_world_prior.c
 * @brief Unified World Prior — aggregates physics, chemistry, biology constraints
 */

#include "cognitive/physics/nimcp_world_prior.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define EMA_ALPHA 0.01f

/* ============================================================================
 * Public API
 * ============================================================================ */

wp_config_t world_prior_default_config(void) {
    return (wp_config_t){
        .physics_weight = 1.0f,
        .chemistry_weight = 0.5f,
        .biology_weight = 0.3f,
        .adaptive_weights = true,
        .enabled_domains = WP_DOMAIN_ALL,
    };
}

world_prior_t* world_prior_create(const wp_config_t* config) {
    wp_config_t cfg = config ? *config : world_prior_default_config();

    world_prior_t* wp = nimcp_calloc(1, sizeof(*wp));
    if (!wp) return NULL;

    wp->config = cfg;
    wp->physics_loss_ema = 1.0f;
    wp->chemistry_loss_ema = 1.0f;
    wp->biology_loss_ema = 1.0f;
    wp->initialized = true;

    LOG_INFO("World prior created: domains=0x%02x (phys=%s chem=%s bio=%s)",
             cfg.enabled_domains,
             (cfg.enabled_domains & WP_DOMAIN_PHYSICS) ? "yes" : "no",
             (cfg.enabled_domains & WP_DOMAIN_CHEMISTRY) ? "yes" : "no",
             (cfg.enabled_domains & WP_DOMAIN_BIOLOGY) ? "yes" : "no");
    return wp;
}

void world_prior_destroy(world_prior_t* wp) {
    if (!wp) return;
    nimcp_free(wp);
}

void world_prior_connect(world_prior_t* wp,
                          physics_prior_t* physics,
                          chemistry_sim_t* chemistry,
                          biology_sim_t* biology) {
    if (!wp) return;
    wp->physics = physics;
    wp->chemistry = chemistry;
    wp->biology = biology;
}

float world_prior_compute_loss(world_prior_t* wp, uint32_t domain_hint) {
    if (!wp) return 0;

    float total = 0;
    uint32_t domains = domain_hint ? domain_hint : wp->config.enabled_domains;

    /* Physics loss */
    if ((domains & WP_DOMAIN_PHYSICS) && wp->physics) {
        pp_stats_t ps = physics_prior_get_stats(wp->physics);
        float phys_loss = ps.learned_error_ema;
        total += wp->config.physics_weight * phys_loss;
        wp->physics_loss_ema = (1 - EMA_ALPHA) * wp->physics_loss_ema + EMA_ALPHA * phys_loss;
        wp->stats.physics_loss_avg = wp->physics_loss_ema;
    }

    /* Chemistry loss: mass conservation drift */
    if ((domains & WP_DOMAIN_CHEMISTRY) && wp->chemistry) {
        float chem_loss = fabsf(wp->chemistry->mass_drift);
        total += wp->config.chemistry_weight * chem_loss;
        wp->chemistry_loss_ema = (1 - EMA_ALPHA) * wp->chemistry_loss_ema + EMA_ALPHA * chem_loss;
        wp->stats.chemistry_loss_avg = wp->chemistry_loss_ema;
    }

    /* Biology loss: energy conservation + impossible growth */
    if ((domains & WP_DOMAIN_BIOLOGY) && wp->biology) {
        float bio_loss = 0;
        /* Check for impossible population states */
        for (uint32_t i = 0; i < wp->biology->num_species; i++) {
            const bio_species_t* sp = &wp->biology->species[i];
            if (!sp->active) continue;
            /* Penalty for exceeding carrying capacity */
            if (sp->carrying_capacity > 0.0f && sp->population > sp->carrying_capacity * 1.5f) {
                bio_loss += (sp->population - sp->carrying_capacity) / sp->carrying_capacity;
            }
        }
        total += wp->config.biology_weight * bio_loss;
        wp->biology_loss_ema = (1 - EMA_ALPHA) * wp->biology_loss_ema + EMA_ALPHA * bio_loss;
        wp->stats.biology_loss_avg = wp->biology_loss_ema;
    }

    wp->stats.total_loss_avg = (1 - EMA_ALPHA) * wp->stats.total_loss_avg + EMA_ALPHA * total;
    wp->stats.total_checks++;
    return total;
}

uint32_t world_prior_check_violations(world_prior_t* wp) {
    if (!wp) return 0;

    uint32_t violated = 0;

    if ((wp->config.enabled_domains & WP_DOMAIN_PHYSICS) && wp->physics) {
        pp_stats_t ps = physics_prior_get_stats(wp->physics);
        if (ps.violations_detected > 0) {
            violated |= WP_DOMAIN_PHYSICS;
            wp->stats.physics_violations++;
        }
    }

    if ((wp->config.enabled_domains & WP_DOMAIN_CHEMISTRY) && wp->chemistry) {
        if (fabsf(wp->chemistry->mass_drift) > 0.01f) {
            violated |= WP_DOMAIN_CHEMISTRY;
            wp->stats.chemistry_violations++;
        }
    }

    if ((wp->config.enabled_domains & WP_DOMAIN_BIOLOGY) && wp->biology) {
        for (uint32_t i = 0; i < wp->biology->num_species; i++) {
            if (wp->biology->species[i].active && wp->biology->species[i].population < 0) {
                violated |= WP_DOMAIN_BIOLOGY;
                wp->stats.biology_violations++;
                break;
            }
        }
    }

    return violated;
}

int world_prior_step(world_prior_t* wp, float dt) {
    if (!wp) return -1;

    /* Physics steps are handled externally (by the physics engine in the main loop) */

    /* Chemistry step */
    if ((wp->config.enabled_domains & WP_DOMAIN_CHEMISTRY) && wp->chemistry) {
        chemistry_sim_step(wp->chemistry, dt);
    }

    /* Biology step */
    if ((wp->config.enabled_domains & WP_DOMAIN_BIOLOGY) && wp->biology) {
        biology_sim_step(wp->biology, dt);
    }

    return 0;
}

wp_stats_t world_prior_get_stats(const world_prior_t* wp) {
    if (!wp) return (wp_stats_t){0};
    return wp->stats;
}
