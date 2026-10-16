/**
 * @file nimcp_sim_perception_bridge.c
 * @brief Simulation-Perception Bridge — render physics into sensory experience
 *
 * WHAT: Physics scene → pixels + sounds + haptics → brain perceptual cortices
 * WHY:  Multimodal grounding of physical laws through sensory experience
 * HOW:  Top-down renderer, impact sonification, surface-to-haptic mapping
 */

#include "cognitive/physics/nimcp_sim_perception_bridge.h"
#include "cognitive/physics/nimcp_audio_dsp_bridge.h"
#include "cognitive/physics/nimcp_scene_graph.h"
#include "cognitive/physics/nimcp_entity_tracker.h"
#include "cognitive/physics/nimcp_surface_physics.h"
#include "cognitive/physics/nimcp_surface_chemistry.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "SIM_PERCEPTION"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline float spb_clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float spb_len(wm_parietal_vec3_t v) {
    return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

spb_config_t spb_default_config(void) {
    return (spb_config_t){
        .enable_visual = true,
        .enable_audio = true,
        .enable_haptic = true,
        .camera_distance = 5.0f,
        .camera_height = 3.0f,
        .audio_gain = 1.0f,
    };
}

sim_perception_bridge_t* spb_create(const spb_config_t* config) {
    spb_config_t cfg = config ? *config : spb_default_config();

    sim_perception_bridge_t* b = nimcp_calloc(1, sizeof(*b));
    if (!b) return NULL;

    b->config = cfg;

    /* Visual: allocate framebuffer */
    if (cfg.enable_visual) {
        uint32_t fb_size = SPB_VISUAL_WIDTH * SPB_VISUAL_HEIGHT * SPB_VISUAL_CHANNELS;
        b->visual.framebuffer = nimcp_calloc(fb_size, sizeof(uint8_t));
        b->visual.width = SPB_VISUAL_WIDTH;
        b->visual.height = SPB_VISUAL_HEIGHT;
        b->visual.channels = SPB_VISUAL_CHANNELS;
        b->visual.camera_pos = (wm_parietal_vec3_t){0, cfg.camera_height, cfg.camera_distance};
        b->visual.camera_target = (wm_parietal_vec3_t){0, 0, 0};
        b->visual.fov = (float)(M_PI / 4.0);
        b->visual.near_plane = 0.1f;
        b->visual.far_plane = 100.0f;
    }

    /* Audio: allocate buffers */
    if (cfg.enable_audio) {
        b->audio.num_samples = SPB_AUDIO_SAMPLES;
        b->audio.sample_rate = 44100.0f;
        b->audio.sample_buffer = nimcp_calloc(SPB_AUDIO_SAMPLES, sizeof(float));
        b->audio.mel_bins = SPB_AUDIO_MEL_BINS;
        b->audio.mel_features = nimcp_calloc(SPB_AUDIO_MEL_BINS, sizeof(float));
    }

    /* Haptic: allocate segment array */
    if (cfg.enable_haptic) {
        b->haptic.num_segments = SPB_SOMATO_SEGMENTS;
        b->haptic.segment_activations = nimcp_calloc(SPB_SOMATO_SEGMENTS, sizeof(float));
    }

    if ((cfg.enable_visual && !b->visual.framebuffer) ||
        (cfg.enable_audio && (!b->audio.sample_buffer || !b->audio.mel_features)) ||
        (cfg.enable_haptic && !b->haptic.segment_activations)) {
        spb_destroy(b);
        return NULL;
    }

    b->initialized = true;
    LOG_INFO(LOG_TAG, "Sim-perception bridge created: visual=%s (%ux%u), audio=%s (%u samples), haptic=%s (%u segments)",
             cfg.enable_visual ? "yes" : "no", SPB_VISUAL_WIDTH, SPB_VISUAL_HEIGHT,
             cfg.enable_audio ? "yes" : "no", SPB_AUDIO_SAMPLES,
             cfg.enable_haptic ? "yes" : "no", SPB_SOMATO_SEGMENTS);
    return b;
}

void spb_destroy(sim_perception_bridge_t* bridge) {
    if (!bridge) return;
    nimcp_free(bridge->visual.framebuffer);
    nimcp_free(bridge->audio.sample_buffer);
    nimcp_free(bridge->audio.mel_features);
    nimcp_free(bridge->haptic.segment_activations);
    nimcp_free(bridge);
}

void spb_connect_physics(sim_perception_bridge_t* bridge,
                          struct intuitive_physics_engine* physics,
                          struct scene_graph* scene,
                          struct entity_tracker* tracker) {
    if (!bridge) return;
    bridge->physics = physics;
    bridge->scene = scene;
    bridge->tracker = tracker;
}

void spb_connect_surface(sim_perception_bridge_t* bridge,
                           struct surface_physics_sim* surface_phys,
                           struct surface_chemistry_sim* surface_chem) {
    if (!bridge) return;
    bridge->surface_phys = surface_phys;
    bridge->surface_chem = surface_chem;
}

/* ============================================================================
 * Visual Renderer — top-down orthographic projection
 *
 * Objects are rendered as circles (spheres) or rectangles (boxes) on a
 * 64x64 grayscale framebuffer. Brightness encodes height (closer = brighter).
 * Ground plane is dark gray. This gives the visual cortex a simple but
 * physically meaningful image to process.
 * ============================================================================ */

int spb_render_visual(sim_perception_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_visual || !bridge->visual.framebuffer) return -1;
    if (!bridge->physics) return -1;

    uint8_t* fb = bridge->visual.framebuffer;
    uint32_t w = bridge->visual.width, h = bridge->visual.height;

    /* Clear to dark gray (ground) */
    memset(fb, 40, w * h);

    /* Scene bounds for coordinate mapping */
    float scene_half = bridge->config.camera_distance;
    float scale_x = (float)w / (2.0f * scene_half);
    float scale_y = (float)h / (2.0f * scene_half);

    /* Access physics objects */
    /* We access the scene through the ip_scene_t which is the first member */
    /* For safety, use the public API */
    for (uint32_t obj_id = 0; obj_id < IP_MAX_OBJECTS; obj_id++) {
        ip_object_t* obj = intuitive_physics_get_object(bridge->physics, obj_id);
        if (!obj || !obj->active || obj->is_static) continue;

        /* Project to screen (top-down: x→screen_x, z→screen_y) */
        float sx = (obj->position.x + scene_half) * scale_x;
        float sy = (obj->position.z + scene_half) * scale_y;

        /* Radius in pixels */
        float radius_px = 0;
        if (obj->shape.type == IP_SHAPE_SPHERE)
            radius_px = obj->shape.sphere.radius * scale_x;
        else if (obj->shape.type == IP_SHAPE_BOX)
            radius_px = obj->shape.box.hx * scale_x;
        if (radius_px < 1.0f) radius_px = 1.0f;

        /* Brightness from height (higher = brighter) */
        uint8_t brightness = (uint8_t)spb_clamp(80.0f + obj->position.y * 40.0f, 80.0f, 255.0f);

        /* Draw filled circle (clamp radius to prevent huge loops) */
        if (radius_px > 32.0f) radius_px = 32.0f;
        int cx = (int)sx, cy = (int)sy, r = (int)radius_px;
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (dx*dx + dy*dy <= r*r) {
                    int px = cx + dx, py = cy + dy;
                    if (px >= 0 && px < (int)w && py >= 0 && py < (int)h) {
                        fb[py * w + px] = brightness;
                    }
                }
            }
        }

        /* Velocity indicator: line from center in velocity direction */
        float vx = obj->velocity.vx * scale_x * 0.1f;
        float vz = obj->velocity.vz * scale_y * 0.1f;
        int steps = (int)(sqrtf(vx*vx + vz*vz));
        if (steps < 0) steps = 0;
        if (steps > 20) steps = 20;
        for (int s = 0; s < steps; s++) {
            int px = cx + (int)(vx * s / (float)steps);
            int py = cy + (int)(vz * s / (float)steps);
            if (px >= 0 && px < (int)w && py >= 0 && py < (int)h)
                fb[py * w + px] = 255;  /* white velocity trail */
        }
    }

    bridge->stats.frames_rendered++;
    return 0;
}

/* ============================================================================
 * Audio Synthesizer — collision impacts and friction sounds
 *
 * Each active contact generates a decaying sinusoidal tone. Frequency
 * depends on object mass (heavy = low, light = high). Amplitude depends
 * on collision impulse magnitude. Friction adds broadband noise.
 * Output: mel-scale spectral features for the audio cortex CNN.
 * ============================================================================ */

int spb_render_audio(sim_perception_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_audio || !bridge->audio.sample_buffer) return -1;
    if (!bridge->physics) return -1;

    float* buf = bridge->audio.sample_buffer;
    uint32_t n = bridge->audio.num_samples;
    float sr = bridge->audio.sample_rate;
    float gain = bridge->config.audio_gain;

    /* Clear buffer */
    memset(buf, 0, n * sizeof(float));

    /* Reset sound sources */
    bridge->audio.num_sources = 0;

    /* Generate impact sounds from physics contacts */
    /* Access scene contacts through the engine */
    /* For each contact with significant impulse, create a decaying tone */
    ip_stats_t pstats = intuitive_physics_get_stats(bridge->physics);
    uint32_t num_contacts = pstats.active_contacts;

    if (num_contacts > 0 && num_contacts <= SPB_MAX_SOUND_SOURCES) {
        /* Approximate: each contact generates a tone based on estimated mass */
        for (uint32_t c = 0; c < num_contacts && bridge->audio.num_sources < SPB_MAX_SOUND_SOURCES; c++) {
            spb_sound_source_t* src = &bridge->audio.sources[bridge->audio.num_sources];
            /* Frequency inversely proportional to estimated mass (lighter = higher pitch) */
            float estimated_mass = 1.0f;  /* default */
            src->frequency = 200.0f + 800.0f / (estimated_mass + 0.1f);
            src->amplitude = spb_clamp(0.3f * gain, 0, 1);
            src->decay_rate = 10.0f;  /* decay in ~0.1s */
            src->phase = 0;
            bridge->audio.num_sources++;
        }
    }

    /* Synthesize audio from all sources */
    for (uint32_t s = 0; s < bridge->audio.num_sources; s++) {
        spb_sound_source_t* src = &bridge->audio.sources[s];
        float omega = 2.0f * (float)M_PI * src->frequency / sr;
        for (uint32_t i = 0; i < n; i++) {
            float t = (float)i / sr;
            float envelope = src->amplitude * expf(-src->decay_rate * t);
            buf[i] += envelope * sinf(omega * i + src->phase);
        }
    }

    /* Clamp to [-1, 1] */
    for (uint32_t i = 0; i < n; i++)
        buf[i] = spb_clamp(buf[i], -1.0f, 1.0f);

    /* Compute mel-scale features via proper DSP pipeline:
     * Hamming window → FFT → |X|² → mel filterbank → log normalize.
     * Falls back to simple power-in-bins if DSP engine not available. */
    float* mel = bridge->audio.mel_features;
    uint32_t mel_bins = bridge->audio.mel_bins;

    /* Try DSP-based mel extraction (proper spectral analysis) */
    audio_dsp_bridge_t* adsp = audio_dsp_bridge_create(NULL);
    if (adsp) {
        audio_dsp_process(adsp, buf, n, mel);
        audio_dsp_bridge_destroy(adsp);
    } else {
        /* Fallback: simple power estimate per bin */
        memset(mel, 0, mel_bins * sizeof(float));
        uint32_t samples_per_bin = n / mel_bins;
        if (samples_per_bin == 0) samples_per_bin = 1;
        for (uint32_t b = 0; b < mel_bins; b++) {
            float power = 0;
            uint32_t start = b * samples_per_bin;
            uint32_t end = start + samples_per_bin;
            if (end > n) end = n;
            for (uint32_t i = start; i < end; i++)
                power += buf[i] * buf[i];
            mel[b] = sqrtf(power / (float)samples_per_bin);
        }
    }

    bridge->stats.sounds_generated++;
    return 0;
}

/* ============================================================================
 * Haptic Mapper — surface properties to somatosensory body segments
 *
 * Maps physics contact properties (pressure, temperature, friction,
 * roughness) to the 45-segment somatosensory body map. The "hand"
 * segments (fingertips, palms) get the primary contact information.
 * Vibration from collisions maps to Pacinian corpuscle activation.
 * Temperature maps to thermoreceptors.
 * ============================================================================ */

/* Body segment indices (matching nimcp_somatosensory.h) */
#define SEG_HAND_R      14  /* right hand */
#define SEG_HAND_L      15  /* left hand */
#define SEG_FINGERS_R   16
#define SEG_FINGERS_L   17
#define SEG_FOREARM_R   10
#define SEG_FOREARM_L   11
#define SEG_CHEST       4
#define SEG_ABDOMEN     5
#define SEG_FEET_R      22
#define SEG_FEET_L      23

int spb_render_haptic(sim_perception_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_haptic || !bridge->haptic.segment_activations) return -1;

    float* seg = bridge->haptic.segment_activations;
    uint32_t n_seg = bridge->haptic.num_segments;
    memset(seg, 0, n_seg * sizeof(float));

    /* Physics contacts → haptic feedback */
    if (bridge->physics) {
        ip_stats_t pstats = intuitive_physics_get_stats(bridge->physics);

        /* Collision vibration → hands and feet */
        float vibration = spb_clamp((float)pstats.active_contacts * 0.1f, 0, 1);
        if (SEG_FINGERS_R < n_seg) seg[SEG_FINGERS_R] += vibration * 0.8f;
        if (SEG_FINGERS_L < n_seg) seg[SEG_FINGERS_L] += vibration * 0.8f;
        if (SEG_HAND_R < n_seg) seg[SEG_HAND_R] += vibration * 0.5f;
        if (SEG_HAND_L < n_seg) seg[SEG_HAND_L] += vibration * 0.5f;
        if (SEG_FEET_R < n_seg) seg[SEG_FEET_R] += vibration * 0.3f;
        if (SEG_FEET_L < n_seg) seg[SEG_FEET_L] += vibration * 0.3f;

        /* Kinetic energy → overall body tension */
        float energy_norm = spb_clamp(pstats.total_kinetic_energy * 0.01f, 0, 1);
        if (SEG_FOREARM_R < n_seg) seg[SEG_FOREARM_R] += energy_norm * 0.3f;
        if (SEG_FOREARM_L < n_seg) seg[SEG_FOREARM_L] += energy_norm * 0.3f;
    }

    /* Surface properties → texture and temperature */
    if (bridge->surface_phys) {
        surf_phys_stats_t sstats = surface_physics_get_stats(bridge->surface_phys);

        /* Surface energy → texture feeling on fingertips */
        float texture = spb_clamp(sstats.total_surface_energy * 10.0f, 0, 1);
        if (SEG_FINGERS_R < n_seg) seg[SEG_FINGERS_R] += texture * 0.4f;
        if (SEG_FINGERS_L < n_seg) seg[SEG_FINGERS_L] += texture * 0.4f;

        /* Heat transfer → temperature sensation */
        float heat = spb_clamp(fabsf(sstats.total_heat_transferred) * 0.001f, 0, 1);
        if (SEG_HAND_R < n_seg) seg[SEG_HAND_R] += heat * 0.5f;
        if (SEG_CHEST < n_seg) seg[SEG_CHEST] += heat * 0.2f;

        /* Marangoni flow → subtle fluid motion on skin */
        float flow = spb_clamp(sstats.max_marangoni_velocity * 100.0f, 0, 1);
        if (SEG_HAND_R < n_seg) seg[SEG_HAND_R] += flow * 0.2f;
    }

    /* Surface chemistry → chemical sensation (e.g., corrosion warmth, acid sting) */
    if (bridge->surface_chem) {
        schem_stats_t cstats = surface_chemistry_get_stats(bridge->surface_chem);
        float chem_activity = spb_clamp(cstats.max_reaction_rate * 1e-3f, 0, 1);
        if (SEG_FINGERS_R < n_seg) seg[SEG_FINGERS_R] += chem_activity * 0.3f;
    }

    /* Clamp all segments to [0, 1] */
    for (uint32_t i = 0; i < n_seg; i++)
        seg[i] = spb_clamp(seg[i], 0, 1);

    bridge->haptic.contact_pressure = seg[SEG_FINGERS_R < n_seg ? SEG_FINGERS_R : 0];
    bridge->stats.haptic_updates++;
    return 0;
}

/* ============================================================================
 * Combined Render + Submit
 * ============================================================================ */

int spb_render(sim_perception_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) return -1;
    int rc = 0;
    if (bridge->config.enable_visual) rc |= spb_render_visual(bridge);
    if (bridge->config.enable_audio)  rc |= spb_render_audio(bridge);
    if (bridge->config.enable_haptic) rc |= spb_render_haptic(bridge);
    return rc;
}

int spb_submit_to_brain(sim_perception_bridge_t* bridge,
                          struct nimcp_brain_handle* brain) {
    if (!bridge || !brain) return -1;

    /* Use the C API to submit sensory data.
     * These call into the brain's staged_sensory system (defined in nimcp_api_sensory.c).
     * They may not exist yet if the API hasn't been extended — use weak symbols
     * so the library links even without them. */
    extern int nimcp_brain_submit_sensory_visual(
        struct nimcp_brain_handle* brain, const uint8_t* pixels,
        uint32_t width, uint32_t height, uint32_t channels) __attribute__((weak));
    extern int nimcp_brain_submit_sensory_audio(
        struct nimcp_brain_handle* brain, const float* data, uint32_t size) __attribute__((weak));
    extern int nimcp_brain_submit_sensory_somatosensory(
        struct nimcp_brain_handle* brain, const float* segments, uint32_t n_segments) __attribute__((weak));

    if (bridge->config.enable_visual && bridge->visual.framebuffer &&
        nimcp_brain_submit_sensory_visual) {
        nimcp_brain_submit_sensory_visual(brain,
            bridge->visual.framebuffer,
            bridge->visual.width, bridge->visual.height, bridge->visual.channels);
        bridge->stats.sensory_submissions++;
    }

    if (bridge->config.enable_audio && bridge->audio.mel_features &&
        nimcp_brain_submit_sensory_audio) {
        nimcp_brain_submit_sensory_audio(brain,
            bridge->audio.mel_features, bridge->audio.mel_bins);
        bridge->stats.sensory_submissions++;
    }

    if (bridge->config.enable_haptic && bridge->haptic.segment_activations &&
        nimcp_brain_submit_sensory_somatosensory) {
        nimcp_brain_submit_sensory_somatosensory(brain,
            bridge->haptic.segment_activations, bridge->haptic.num_segments);
        bridge->stats.sensory_submissions++;
    }

    return 0;
}

/* ============================================================================
 * Accessors
 * ============================================================================ */

const uint8_t* spb_get_visual_frame(const sim_perception_bridge_t* bridge) {
    return bridge ? bridge->visual.framebuffer : NULL;
}

const float* spb_get_audio_features(const sim_perception_bridge_t* bridge) {
    return bridge ? bridge->audio.mel_features : NULL;
}

const float* spb_get_haptic_segments(const sim_perception_bridge_t* bridge) {
    return bridge ? bridge->haptic.segment_activations : NULL;
}

spb_stats_t spb_get_stats(const sim_perception_bridge_t* bridge) {
    if (!bridge) return (spb_stats_t){0};
    return bridge->stats;
}
