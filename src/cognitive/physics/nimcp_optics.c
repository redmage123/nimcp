/**
 * @file nimcp_optics.c
 * @brief Optics simulation — ray tracing, lenses, diffraction, polarization
 *
 * WHAT: Sequential ray tracing through optical elements with Snell's law,
 *       Fresnel coefficients, thin lens/mirror equations, diffraction gratings.
 * WHY:  Reasoning about vision, cameras, telescopes, lasers, fiber optics.
 * HOW:  Rays propagate along z-axis, interact with surfaces at specified
 *       z-positions. Refraction/reflection computed via Snell + Fresnel.
 */

#include "cognitive/physics/nimcp_optics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "OPTICS"

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline void normalize3(float* dx, float* dy, float* dz) {
    float len = sqrtf((*dx)*(*dx) + (*dy)*(*dy) + (*dz)*(*dz));
    if (len > 1e-20f) { *dx /= len; *dy /= len; *dz /= len; }
}

/* ============================================================================
 * Default Config
 * ============================================================================ */

opt_config_t opt_default_config(void) {
    opt_config_t c;
    memset(&c, 0, sizeof(c));
    c.max_trace_depth   = OPT_MAX_TRACE_DEPTH;
    c.min_intensity     = 1e-6f;
    c.default_wavelength = OPT_LAMBDA_GREEN;
    c.ambient_n         = OPT_N_AIR;
    c.enable_fresnel    = true;
    c.enable_polarization = false;
    c.enable_dispersion = false;
    return c;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

optics_sim_t* opt_create(const opt_config_t* config) {
    opt_config_t cfg = config ? *config : opt_default_config();

    optics_sim_t* sim = nimcp_calloc(1, sizeof(optics_sim_t));
    if (!sim) return NULL;

    sim->config = cfg;
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Created optics sim (max_depth=%u, fresnel=%d)",
             cfg.max_trace_depth, cfg.enable_fresnel);
    return sim;
}

void opt_destroy(optics_sim_t* sim) {
    if (!sim) return;
    nimcp_free(sim);
}

/* ============================================================================
 * Ray-Surface Interaction
 * ============================================================================ */

/** Trace a single ray through one surface. Returns 0 on interaction, -1 if missed. */
static int trace_ray_surface(optics_sim_t* sim, opt_ray_t* ray, const opt_surface_t* surf) {
    if (!ray->active || !surf->active) return -1;
    if (ray->intensity < sim->config.min_intensity) { ray->active = false; return -1; }

    /* Propagate ray to surface z-plane */
    if (fabsf(ray->dz) < 1e-20f) return -1;   /* ray parallel to surface */
    float t = (surf->position - ray->oz) / ray->dz;
    if (t < 1e-6f) return -1;  /* surface behind ray */

    float hit_x = ray->ox + ray->dx * t;
    float hit_y = ray->oy + ray->dy * t;

    /* Aperture check */
    float r2 = (hit_x - surf->center_x) * (hit_x - surf->center_x) +
               (hit_y - surf->center_y) * (hit_y - surf->center_y);
    if (r2 > surf->aperture * surf->aperture) return -1;

    /* Update ray origin to hit point */
    ray->ox = hit_x;
    ray->oy = hit_y;
    ray->oz = surf->position;

    if (surf->type == OPT_SURFACE_GRATING) {
        /* Diffraction grating: d*sin(theta_out) = d*sin(theta_in) + m*lambda */
        float sin_in = ray->dx; /* approximation for paraxial */
        float d = surf->grating_spacing;
        if (d < 1e-20f) return -1;
        float sin_out = sin_in + (float)surf->grating_order * ray->wavelength / d;
        if (fabsf(sin_out) >= 1.0f) { ray->active = false; return -1; } /* evanescent */
        ray->dx = sin_out;
        ray->dz = sqrtf(1.0f - sin_out * sin_out);
        ray->bounces++;
        sim->stats.total_refractions++;
        return 0;
    }

    if (surf->type == OPT_SURFACE_SLIT) {
        /* Single slit diffraction: main lobe half-angle = lambda/a */
        float half_angle = ray->wavelength / (surf->slit_width + 1e-20f);
        /* Intensity modulation: sinc^2(pi*a*sin(theta)/lambda) */
        float sin_theta = ray->dx;
        float u = (float)M_PI * surf->slit_width * sin_theta / (ray->wavelength + 1e-30f);
        float sinc = (fabsf(u) < 1e-6f) ? 1.0f : sinf(u) / u;
        ray->intensity *= sinc * sinc;
        (void)half_angle;
        ray->bounces++;
        return 0;
    }

    /* Flat or spherical surface — compute local surface normal */
    float nx = 0.0f, ny = 0.0f, nz = -1.0f;  /* default: flat, pointing back toward ray */
    if (surf->type == OPT_SURFACE_SPHERICAL && fabsf(surf->radius_of_curvature) > 1e-10f) {
        /* Spherical surface: normal from center of curvature to hit point */
        float R = surf->radius_of_curvature;
        float cz = surf->position + R;  /* center of curvature behind surface */
        float snx = hit_x - surf->center_x;
        float sny = hit_y - surf->center_y;
        float snz = surf->position - cz;
        float slen = sqrtf(snx*snx + sny*sny + snz*snz);
        if (slen > 1e-10f) { nx = snx/slen; ny = sny/slen; nz = snz/slen; }
    }

    /* Ensure normal points against ray direction */
    float dot_in = ray->dx * nx + ray->dy * ny + ray->dz * nz;
    if (dot_in > 0.0f) { nx = -nx; ny = -ny; nz = -nz; dot_in = -dot_in; }

    float cos_i = -dot_in;  /* cos(theta_i) */
    float n1 = surf->n_before;
    float n2 = surf->n_after;

    if (surf->interaction == OPT_INTERACT_REFLECT || cos_i < 1e-10f) {
        /* Pure reflection: r = d - 2*(d.n)*n */
        ray->dx = ray->dx + 2.0f * cos_i * nx;
        ray->dy = ray->dy + 2.0f * cos_i * ny;
        ray->dz = ray->dz + 2.0f * cos_i * nz;
        normalize3(&ray->dx, &ray->dy, &ray->dz);
        ray->intensity *= surf->reflectivity;
        ray->bounces++;
        sim->stats.total_reflections++;
        return 0;
    }

    /* Snell's law refraction */
    float ratio = n1 / n2;
    float sin2_t = ratio * ratio * (1.0f - cos_i * cos_i);

    if (sin2_t > 1.0f) {
        /* Total internal reflection */
        ray->dx = ray->dx + 2.0f * cos_i * nx;
        ray->dy = ray->dy + 2.0f * cos_i * ny;
        ray->dz = ray->dz + 2.0f * cos_i * nz;
        normalize3(&ray->dx, &ray->dy, &ray->dz);
        ray->bounces++;
        sim->stats.total_reflections++;
        sim->stats.total_internal_reflections++;
        return 0;
    }

    float cos_t = sqrtf(1.0f - sin2_t);

    /* Fresnel coefficients */
    float reflectance = 0.0f;
    if (sim->config.enable_fresnel && surf->interaction == OPT_INTERACT_BOTH) {
        reflectance = opt_fresnel_average(n1, n2, cos_i);
    }

    if (reflectance > 0.01f && surf->interaction == OPT_INTERACT_BOTH) {
        /* Partial reflection: reduce transmitted intensity */
        ray->intensity *= (1.0f - reflectance);
    }

    /* Refracted direction */
    float k = ratio;
    ray->dx = k * ray->dx + (k * cos_i - cos_t) * nx;
    ray->dy = k * ray->dy + (k * cos_i - cos_t) * ny;
    ray->dz = k * ray->dz + (k * cos_i - cos_t) * nz;
    normalize3(&ray->dx, &ray->dy, &ray->dz);
    ray->bounces++;
    sim->stats.total_refractions++;

    return 0;
}

/* ============================================================================
 * Step / Trace
 * ============================================================================ */

int opt_step(optics_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) return -1;
    (void)dt;

    /* Sort surfaces by z-position would be ideal; for now iterate in order */
    for (uint32_t ri = 0; ri < sim->num_rays; ri++) {
        opt_ray_t* ray = &sim->rays[ri];
        if (!ray->active) continue;

        /* Find next surface this ray will hit */
        float min_t = 1e30f;
        int best_si = -1;
        for (uint32_t si = 0; si < sim->num_surfaces; si++) {
            opt_surface_t* surf = &sim->surfaces[si];
            if (!surf->active) continue;
            if (fabsf(ray->dz) < 1e-20f) continue;
            float t = (surf->position - ray->oz) / ray->dz;
            if (t > 1e-6f && t < min_t) {
                min_t = t;
                best_si = (int)si;
            }
        }
        if (best_si >= 0) {
            trace_ray_surface(sim, ray, &sim->surfaces[best_si]);
        } else {
            ray->active = false;    /* no more surfaces to hit */
        }
    }

    sim->stats.step_count++;
    sim->stats.rays_traced = sim->num_rays;
    return 0;
}

int opt_trace_all(optics_sim_t* sim) {
    if (!sim || !sim->initialized) return -1;

    for (uint32_t ri = 0; ri < sim->num_rays; ri++) {
        opt_ray_t* ray = &sim->rays[ri];
        uint32_t max_steps = sim->config.max_trace_depth;
        while (ray->active && ray->bounces < max_steps) {
            float min_t = 1e30f;
            int best_si = -1;
            for (uint32_t si = 0; si < sim->num_surfaces; si++) {
                opt_surface_t* surf = &sim->surfaces[si];
                if (!surf->active) continue;
                if (fabsf(ray->dz) < 1e-20f) continue;
                float t = (surf->position - ray->oz) / ray->dz;
                if (t > 1e-6f && t < min_t) {
                    min_t = t;
                    best_si = (int)si;
                }
            }
            if (best_si >= 0) {
                trace_ray_surface(sim, ray, &sim->surfaces[best_si]);
            } else {
                break;
            }
        }
        sim->stats.total_intensity_out += ray->intensity;
    }

    sim->stats.rays_traced = sim->num_rays;
    /* Compute average path length */
    float total_path = 0.0f;
    for (uint32_t ri = 0; ri < sim->num_rays; ri++) {
        total_path += (float)sim->rays[ri].bounces;
    }
    sim->stats.avg_path_length = (sim->num_rays > 0) ?
        total_path / (float)sim->num_rays : 0.0f;
    return 0;
}

opt_stats_t opt_get_stats(const optics_sim_t* sim) {
    if (!sim) { opt_stats_t s; memset(&s, 0, sizeof(s)); return s; }
    return sim->stats;
}

/* ============================================================================
 * Setup
 * ============================================================================ */

uint32_t opt_add_ray(optics_sim_t* sim, const opt_ray_t* ray) {
    if (!sim || sim->num_rays >= OPT_MAX_RAYS) return UINT32_MAX;
    uint32_t idx = sim->num_rays;
    sim->rays[idx] = *ray;
    sim->rays[idx].active = true;
    if (sim->rays[idx].wavelength <= 0.0f)
        sim->rays[idx].wavelength = sim->config.default_wavelength;
    sim->stats.total_intensity_in += ray->intensity;
    sim->num_rays++;
    return idx;
}

uint32_t opt_add_surface(optics_sim_t* sim, const opt_surface_t* surface) {
    if (!sim || sim->num_surfaces >= OPT_MAX_SURFACES) return UINT32_MAX;
    uint32_t idx = sim->num_surfaces;
    sim->surfaces[idx] = *surface;
    sim->surfaces[idx].active = true;
    sim->num_surfaces++;
    return idx;
}

uint32_t opt_add_thin_lens(optics_sim_t* sim, float position, float focal_length,
                            float aperture, float n_lens) {
    /* Model thin lens as two refracting surfaces very close together */
    /* Use lensmaker's equation: 1/f = (n-1)(1/R1 - 1/R2) with R2 = -R1 */
    float R = 2.0f * focal_length * (n_lens - 1.0f);

    opt_surface_t s1 = {0};
    s1.type = OPT_SURFACE_SPHERICAL;
    s1.interaction = OPT_INTERACT_REFRACT;
    s1.position = position - 0.0001f;
    s1.aperture = aperture;
    s1.radius_of_curvature = R;
    s1.n_before = sim->config.ambient_n;
    s1.n_after = n_lens;
    s1.active = true;

    opt_surface_t s2 = {0};
    s2.type = OPT_SURFACE_SPHERICAL;
    s2.interaction = OPT_INTERACT_REFRACT;
    s2.position = position + 0.0001f;
    s2.aperture = aperture;
    s2.radius_of_curvature = -R;
    s2.n_before = n_lens;
    s2.n_after = sim->config.ambient_n;
    s2.active = true;

    uint32_t idx = opt_add_surface(sim, &s1);
    opt_add_surface(sim, &s2);
    return idx;
}

uint32_t opt_add_mirror(optics_sim_t* sim, float position, float radius_curvature,
                          float aperture) {
    opt_surface_t s = {0};
    s.type = OPT_SURFACE_SPHERICAL;
    s.interaction = OPT_INTERACT_REFLECT;
    s.position = position;
    s.aperture = aperture;
    s.radius_of_curvature = radius_curvature;
    s.reflectivity = 0.98f;
    s.active = true;
    return opt_add_surface(sim, &s);
}

uint32_t opt_add_grating(optics_sim_t* sim, float position, float line_spacing,
                           int32_t order, float aperture) {
    opt_surface_t s = {0};
    s.type = OPT_SURFACE_GRATING;
    s.interaction = OPT_INTERACT_REFRACT;
    s.position = position;
    s.aperture = aperture;
    s.grating_spacing = line_spacing;
    s.grating_order = order;
    s.n_before = sim->config.ambient_n;
    s.n_after = sim->config.ambient_n;
    s.active = true;
    return opt_add_surface(sim, &s);
}

void opt_generate_ray_fan(optics_sim_t* sim, float ox, float oy, float oz,
                           float target_z, float fan_half_angle,
                           uint32_t num_rays, float wavelength) {
    if (!sim) return;
    for (uint32_t i = 0; i < num_rays && sim->num_rays < OPT_MAX_RAYS; i++) {
        float frac = (num_rays > 1) ?
            -1.0f + 2.0f * (float)i / (float)(num_rays - 1) : 0.0f;
        float angle = frac * fan_half_angle;
        opt_ray_t ray = {0};
        ray.ox = ox; ray.oy = oy; ray.oz = oz;
        ray.dx = sinf(angle);
        ray.dy = 0.0f;
        ray.dz = cosf(angle);
        ray.wavelength = wavelength > 0.0f ? wavelength : sim->config.default_wavelength;
        ray.intensity = 1.0f;
        ray.active = true;
        (void)target_z;
        opt_add_ray(sim, &ray);
    }
}

/* ============================================================================
 * Physics Computations
 * ============================================================================ */

float opt_snell_refraction(float n1, float n2, float sin_theta_i) {
    float sin_t = n1 * sin_theta_i / n2;
    if (fabsf(sin_t) > 1.0f) return -1.0f;     /* total internal reflection */
    return sin_t;
}

float opt_fresnel_rs(float n1, float n2, float cos_theta_i, float cos_theta_t) {
    float num = n1 * cos_theta_i - n2 * cos_theta_t;
    float den = n1 * cos_theta_i + n2 * cos_theta_t;
    if (fabsf(den) < 1e-20f) return 1.0f;
    float r = num / den;
    return r * r;
}

float opt_fresnel_rp(float n1, float n2, float cos_theta_i, float cos_theta_t) {
    float num = n2 * cos_theta_i - n1 * cos_theta_t;
    float den = n2 * cos_theta_i + n1 * cos_theta_t;
    if (fabsf(den) < 1e-20f) return 1.0f;
    float r = num / den;
    return r * r;
}

float opt_fresnel_average(float n1, float n2, float cos_theta_i) {
    float sin2_i = 1.0f - cos_theta_i * cos_theta_i;
    float sin2_t = (n1/n2) * (n1/n2) * sin2_i;
    if (sin2_t >= 1.0f) return 1.0f;  /* TIR */
    float cos_t = sqrtf(1.0f - sin2_t);
    return 0.5f * (opt_fresnel_rs(n1, n2, cos_theta_i, cos_t) +
                   opt_fresnel_rp(n1, n2, cos_theta_i, cos_t));
}

float opt_thin_lens_image(float focal_length, float object_distance) {
    /* 1/f = 1/do + 1/di  =>  di = f*do / (do - f) */
    float denom = object_distance - focal_length;
    if (fabsf(denom) < 1e-10f) return 1e10f;   /* object at focus -> image at infinity */
    return focal_length * object_distance / denom;
}

float opt_mirror_image(float focal_length, float object_distance) {
    return opt_thin_lens_image(focal_length, object_distance);
}

float opt_magnification(float object_distance, float image_distance) {
    if (fabsf(object_distance) < 1e-10f) return 0.0f;
    return -image_distance / object_distance;
}

float opt_lensmaker(float n_lens, float R1, float R2) {
    /* 1/f = (n-1) * (1/R1 - 1/R2) */
    float inv_f = (n_lens - 1.0f) * (1.0f / R1 - 1.0f / R2);
    if (fabsf(inv_f) < 1e-20f) return 1e10f;
    return 1.0f / inv_f;
}

float opt_grating_angle(float grating_spacing, int32_t order, float wavelength) {
    /* d*sin(theta) = m*lambda  =>  theta = asin(m*lambda/d) */
    float sin_theta = (float)order * wavelength / grating_spacing;
    if (fabsf(sin_theta) > 1.0f) return -1.0f;  /* no diffracted order */
    return asinf(sin_theta);
}

float opt_slit_minimum_angle(float slit_width, int32_t order, float wavelength) {
    /* a*sin(theta) = m*lambda (minima) */
    if (order == 0) return 0.0f;
    float sin_theta = (float)order * wavelength / slit_width;
    if (fabsf(sin_theta) > 1.0f) return -1.0f;
    return asinf(sin_theta);
}

float opt_malus_law(float I0, float angle) {
    float c = cosf(angle);
    return I0 * c * c;
}

float opt_brewster_angle(float n1, float n2) {
    return atanf(n2 / n1);
}

float opt_critical_angle(float n1, float n2) {
    if (n1 <= n2) return (float)M_PI / 2.0f;   /* no TIR if n1 <= n2 */
    return asinf(n2 / n1);
}

void opt_wavelength_to_rgb(float wavelength, float* r, float* g, float* b) {
    /* Approximate visible spectrum mapping (380-780 nm) */
    float w = wavelength * 1e9f;    /* convert to nm */
    *r = *g = *b = 0.0f;
    if (w >= 380.0f && w < 440.0f) {
        *r = -(w - 440.0f) / 60.0f; *b = 1.0f;
    } else if (w >= 440.0f && w < 490.0f) {
        *g = (w - 440.0f) / 50.0f; *b = 1.0f;
    } else if (w >= 490.0f && w < 510.0f) {
        *g = 1.0f; *b = -(w - 510.0f) / 20.0f;
    } else if (w >= 510.0f && w < 580.0f) {
        *r = (w - 510.0f) / 70.0f; *g = 1.0f;
    } else if (w >= 580.0f && w < 645.0f) {
        *r = 1.0f; *g = -(w - 645.0f) / 65.0f;
    } else if (w >= 645.0f && w <= 780.0f) {
        *r = 1.0f;
    }
    /* Intensity falloff at edges */
    float factor = 1.0f;
    if (w >= 380.0f && w < 420.0f) factor = 0.3f + 0.7f * (w - 380.0f) / 40.0f;
    else if (w > 700.0f && w <= 780.0f) factor = 0.3f + 0.7f * (780.0f - w) / 80.0f;
    *r *= factor; *g *= factor; *b *= factor;
}

/* ============================================================================
 * Legacy API
 * ============================================================================ */

optics_sim_t* optics_create(const optics_config_t* c) { return opt_create(c); }
void optics_destroy(optics_sim_t* s) { opt_destroy(s); }
int optics_step(optics_sim_t* s, float dt) { return opt_step(s, dt); }
optics_config_t optics_default_config(void) { return opt_default_config(); }
optics_stats_t optics_get_stats(const optics_sim_t* s) { return opt_get_stats(s); }
