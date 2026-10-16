/**
 * @file nimcp_sim_perception_bridge.h
 * @brief Simulation-Perception Bridge — render physics into sensory experience
 *
 * WHAT: Converts simulation engine state into sensory data that feeds the
 *       brain's perceptual cortices: visual scenes, collision sounds,
 *       haptic surface properties.
 * WHY:  Humans develop intuitive physics through SENSORY experience, not
 *       equations. The bridge gives the brain multimodal grounding of
 *       physical laws — it doesn't just know a ball falls, it "sees" it
 *       fall, "hears" the impact, and "feels" the bounce.
 * HOW:  Renders physics scene to a pixel framebuffer (visual cortex),
 *       synthesizes impact/friction sounds (audio cortex), and maps
 *       surface properties to body segments (somatosensory cortex).
 *       Submitted via brain's staged_sensory system before decide().
 *
 * INTEGRATION:
 *   Simulation engines → sim_perception_bridge → brain->staged_sensory
 *   → cortex_cnn_forward_* → multimodal fusion → decide/learn
 */

#ifndef NIMCP_SIM_PERCEPTION_BRIDGE_H
#define NIMCP_SIM_PERCEPTION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/physics/nimcp_intuitive_physics.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SPB_VISUAL_WIDTH        64
#define SPB_VISUAL_HEIGHT       64
#define SPB_VISUAL_CHANNELS     1       /* grayscale for simplicity */
#define SPB_AUDIO_SAMPLES       1024    /* ~23ms at 44.1kHz */
#define SPB_AUDIO_MEL_BINS      128
#define SPB_SOMATO_SEGMENTS     45      /* matches somatosensory body map */
#define SPB_MAX_SOUND_SOURCES   16

/* ============================================================================
 * Visual Renderer (physics scene → pixels)
 * ============================================================================ */

typedef struct {
    uint8_t*    framebuffer;        /* [width * height * channels] */
    uint32_t    width, height, channels;
    /* Camera */
    wm_parietal_vec3_t camera_pos;
    wm_parietal_vec3_t camera_target;
    float       fov;                /* field of view (radians) */
    float       near_plane, far_plane;
} spb_visual_renderer_t;

/* ============================================================================
 * Audio Synthesizer (collisions/friction → sound)
 * ============================================================================ */

typedef struct {
    float       frequency;          /* Hz */
    float       amplitude;          /* [0..1] */
    float       decay_rate;         /* exponential decay constant */
    float       phase;              /* current phase */
    wm_parietal_vec3_t position;
} spb_sound_source_t;

typedef struct {
    float*      sample_buffer;      /* [num_samples] */
    uint32_t    num_samples;
    float       sample_rate;        /* Hz (default: 44100) */
    float*      mel_features;       /* [mel_bins] */
    uint32_t    mel_bins;
    spb_sound_source_t sources[SPB_MAX_SOUND_SOURCES];
    uint32_t    num_sources;
} spb_audio_synthesizer_t;

/* ============================================================================
 * Haptic Mapper (surface properties → body segments)
 * ============================================================================ */

typedef struct {
    float*      segment_activations;/* [num_segments] */
    uint32_t    num_segments;
    /* Active contact info */
    float       contact_pressure;   /* N/m² */
    float       contact_temperature;/* K */
    float       contact_roughness;  /* [0..1] */
    float       contact_friction;   /* coefficient */
    float       vibration_intensity;/* [0..1] from collisions */
    uint32_t    contact_segment;    /* which body segment is touching */
} spb_haptic_mapper_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    bool        enable_visual;
    bool        enable_audio;
    bool        enable_haptic;
    float       camera_distance;    /* default: 5.0m */
    float       camera_height;      /* default: 2.0m */
    float       audio_gain;         /* amplitude scaling */
} spb_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    frames_rendered;
    uint64_t    sounds_generated;
    uint64_t    haptic_updates;
    uint64_t    sensory_submissions; /* total submit_sensory calls */
    float       avg_scene_objects;
    float       avg_sound_sources;
} spb_stats_t;

/* ============================================================================
 * Bridge
 * ============================================================================ */

typedef struct sim_perception_bridge {
    spb_visual_renderer_t   visual;
    spb_audio_synthesizer_t audio;
    spb_haptic_mapper_t     haptic;
    spb_config_t            config;
    spb_stats_t             stats;

    /* Connected engines (non-owning) */
    struct intuitive_physics_engine*    physics;
    struct scene_graph*                 scene;
    struct entity_tracker*              tracker;
    struct surface_physics_sim*         surface_phys;
    struct surface_chemistry_sim*       surface_chem;

    bool                    initialized;
} sim_perception_bridge_t;

/* ============================================================================
 * API
 * ============================================================================ */

sim_perception_bridge_t* spb_create(const spb_config_t* config);
void spb_destroy(sim_perception_bridge_t* bridge);

/** Connect simulation engines */
void spb_connect_physics(sim_perception_bridge_t* bridge,
                          struct intuitive_physics_engine* physics,
                          struct scene_graph* scene,
                          struct entity_tracker* tracker);

void spb_connect_surface(sim_perception_bridge_t* bridge,
                           struct surface_physics_sim* surface_phys,
                           struct surface_chemistry_sim* surface_chem);

/**
 * @brief Render current simulation state into all sensory modalities
 *
 * Generates visual frame, audio buffer, and haptic map from the current
 * state of all connected simulation engines. Call this each simulation
 * step, then submit the results to the brain.
 */
int spb_render(sim_perception_bridge_t* bridge);

/**
 * @brief Submit rendered sensory data to the brain
 *
 * Calls brain->submit_sensory for visual, audio, and somatosensory
 * modalities using the last rendered frame/audio/haptic data.
 *
 * @param bridge The bridge
 * @param brain Opaque brain handle (nimcp_brain_t)
 * @return 0 on success
 */
struct nimcp_brain_handle;
int spb_submit_to_brain(sim_perception_bridge_t* bridge,
                          struct nimcp_brain_handle* brain);

/* === Individual Renderers === */

/** Render physics scene to visual framebuffer (top-down orthographic) */
int spb_render_visual(sim_perception_bridge_t* bridge);

/** Generate audio from collisions and friction */
int spb_render_audio(sim_perception_bridge_t* bridge);

/** Map surface properties to haptic body segments */
int spb_render_haptic(sim_perception_bridge_t* bridge);

/** Get the visual framebuffer (for direct access) */
const uint8_t* spb_get_visual_frame(const sim_perception_bridge_t* bridge);

/** Get the audio mel features (for direct access) */
const float* spb_get_audio_features(const sim_perception_bridge_t* bridge);

/** Get the haptic segment activations (for direct access) */
const float* spb_get_haptic_segments(const sim_perception_bridge_t* bridge);

/** Get stats */
spb_stats_t spb_get_stats(const sim_perception_bridge_t* bridge);

/** Default config */
spb_config_t spb_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SIM_PERCEPTION_BRIDGE_H */
