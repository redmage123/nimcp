/**
 * @file nimcp_world_prior.h
 * @brief Unified World Prior — physics + chemistry + biology constraints
 *
 * WHAT: Orchestrates all three simulation domains as priors on the learned
 *       world model. Provides a single interface for constraint checking,
 *       loss computation, and violation detection.
 * WHY:  The world model should make predictions consistent with ALL natural
 *       laws — not just physics, but also chemistry and biology.
 * HOW:  Aggregates violations and loss from each domain, with per-domain
 *       weights that adapt based on error ratios.
 */

#ifndef NIMCP_WORLD_PRIOR_H
#define NIMCP_WORLD_PRIOR_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/physics/nimcp_physics_prior.h"
#include "cognitive/physics/nimcp_chemistry_sim.h"
#include "cognitive/physics/nimcp_biology_sim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Domain flags
 * ============================================================================ */

typedef enum {
    WP_DOMAIN_PHYSICS   = (1 << 0),
    WP_DOMAIN_CHEMISTRY = (1 << 1),
    WP_DOMAIN_BIOLOGY   = (1 << 2),
    WP_DOMAIN_ALL       = 0x07,
} wp_domain_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    float       physics_weight;     /* weight for physics loss (default: 1.0) */
    float       chemistry_weight;   /* weight for chemistry loss (default: 0.5) */
    float       biology_weight;     /* weight for biology loss (default: 0.3) */
    bool        adaptive_weights;   /* auto-adapt from error ratios */
    uint32_t    enabled_domains;    /* bitmask of wp_domain_t */
} wp_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    total_checks;
    uint64_t    physics_violations;
    uint64_t    chemistry_violations;
    uint64_t    biology_violations;
    float       physics_loss_avg;
    float       chemistry_loss_avg;
    float       biology_loss_avg;
    float       total_loss_avg;
} wp_stats_t;

/* ============================================================================
 * World Prior
 * ============================================================================ */

typedef struct world_prior {
    /* Domain engines (may be NULL if domain disabled) */
    physics_prior_t*    physics;        /* NOT owned */
    chemistry_sim_t*    chemistry;      /* NOT owned */
    biology_sim_t*      biology;        /* NOT owned */

    wp_config_t         config;
    wp_stats_t          stats;

    /* Running loss averages */
    float               physics_loss_ema;
    float               chemistry_loss_ema;
    float               biology_loss_ema;

    bool                initialized;
} world_prior_t;

/* ============================================================================
 * API
 * ============================================================================ */

world_prior_t* world_prior_create(const wp_config_t* config);
void world_prior_destroy(world_prior_t* wp);

/** Connect domain engines (non-owning) */
void world_prior_connect(world_prior_t* wp,
                          physics_prior_t* physics,
                          chemistry_sim_t* chemistry,
                          biology_sim_t* biology);

/**
 * Compute total world model loss across all enabled domains.
 * Returns: weighted sum of physics + chemistry + biology losses.
 */
float world_prior_compute_loss(world_prior_t* wp, uint32_t domain_hint);

/**
 * Check for violations in the current world state.
 * Returns: bitmask of violated domains.
 */
uint32_t world_prior_check_violations(world_prior_t* wp);

/**
 * Step all simulation engines forward by dt.
 */
int world_prior_step(world_prior_t* wp, float dt);

/** Get stats */
wp_stats_t world_prior_get_stats(const world_prior_t* wp);

/** Default config */
wp_config_t world_prior_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WORLD_PRIOR_H */
