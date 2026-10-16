/**
 * @file nimcp_occipital_adapter.c
 * @brief Implementation of Occipital Cortex (Visual Cortex) brain adapter
 *
 * WHAT: Unified adapter connecting occipital cortex sub-modules to the brain system
 * WHY:  Enable seamless integration with cognitive layers, training, and event system
 * HOW:  Orchestrates V1-V5 visual hierarchy as a cohesive visual processing unit
 *
 * @version Phase O1: Occipital Cortex Brain Integration
 * @date 2025-12-30
 */

#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(occipital_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define OCCIPITAL_LOG_MODULE "OCCIPITAL"

/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define MAX_FEATURES_PER_AREA 256
#define MAX_MOTION_VECTORS 128
#define MAX_COLOR_PERCEPTS 64
#define V1_GABOR_SIZE 7
#define FEATURE_DESCRIPTOR_SIZE 128

/*=============================================================================
 * INTERNAL STRUCTURES - V1 Processor
 *===========================================================================*/

/**
 * @brief V1 (Primary Visual Cortex) processor - edge detection, orientation
 */
struct v1_processor {
    /* Gabor filter bank */
    float** gabor_filters;           /**< Gabor kernels [orientation][scale] */
    uint32_t num_orientations;
    uint32_t num_scales;

    /* Response maps */
    float* orientation_responses;    /**< Orientation energy per location */
    float* contrast_map;             /**< Local contrast */

    /* Detected edges */
    visual_feature_t* edges;
    uint32_t edge_count;
    uint32_t max_edges;

    /* Statistics */
    uint64_t total_edges;
    float last_processing_time_ms;
};

/**
 * @brief V2 processor - contour integration, texture
 */
struct v2_processor {
    /* Contour integration */
    float* association_field;        /**< Association field weights */
    float* contour_strength;         /**< Contour strength map */

    /* Texture processing */
    float* texture_features;         /**< Texture descriptor */

    /* Detected contours */
    visual_feature_t* contours;
    uint32_t contour_count;
    uint32_t max_contours;

    float last_processing_time_ms;
};

/**
 * @brief V3 processor - dynamic form processing (intermediate)
 */
struct v3_processor {
    /* Form processing */
    float* form_features;
    uint32_t form_count;

    float last_processing_time_ms;
};

/**
 * @brief V4 processor - color constancy, complex form
 */
struct v4_processor {
    /* Color processing */
    float* color_constants;          /**< Color constancy adjusted values */
    color_percept_t* color_percepts;
    uint32_t color_count;
    uint32_t max_colors;

    /* Complex form */
    visual_feature_t* complex_forms;
    uint32_t form_count;
    uint32_t max_forms;

    /* Color space */
    uint32_t color_space;            /**< 0=RGB, 1=LAB, 2=HSV */

    float last_processing_time_ms;
};

/**
 * @brief V5/MT processor - motion detection, optic flow
 */
struct v5_mt_processor {
    /* Motion detection */
    float* motion_energy;            /**< Motion energy map */
    float** frame_buffer;            /**< Temporal frame buffer */
    uint32_t frame_buffer_size;
    uint32_t current_frame_idx;

    /* Motion vectors */
    motion_vector_t* motion_vectors;
    uint32_t motion_count;
    uint32_t max_motions;

    /* Global motion */
    float global_dx;
    float global_dy;

    /* Optic flow */
    float* optic_flow_x;
    float* optic_flow_y;

    float last_processing_time_ms;
};

/*=============================================================================
 * INTERNAL ADAPTER STRUCTURE
 *===========================================================================*/

/**
 * @brief Internal adapter structure
 */
struct occipital_adapter {
    /* Configuration */
    occipital_config_t config;

    /* Input buffer */
    visual_input_t current_input;
    float* input_buffer;             /**< Copy of input image */
    bool has_input;

    /* Sub-modules */
    v1_processor_t* v1;
    v2_processor_t* v2;
    v3_processor_t* v3;
    v4_processor_t* v4;
    v5_mt_processor_t* v5;

    /* Attention state */
    float attention_x;
    float attention_y;
    float attention_radius;
    float attention_gain;
    bool spatial_attention_active;
    visual_feature_type_t attended_feature_type;
    float feature_attention_gain;
    bool feature_attention_active;

    /* Callbacks */
    occipital_feature_callback_t feature_callback;
    void* feature_user_data;
    occipital_motion_callback_t motion_callback;
    void* motion_user_data;
    occipital_event_callback_t event_callback;
    void* event_user_data;

    /* State */
    occipital_status_t status;
    occipital_error_t last_error;
    double current_time_ms;

    /* Bio-async communication context */
    bio_module_context_t bio_ctx;
    nimcp_bio_channel_type_t default_channel;

    /* Memory pool for feature descriptors */
    memory_pool_t descriptor_pool;

    /* Statistics */
    occipital_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Emit event to callback
 */
static void emit_event(occipital_adapter_t* adapter, uint32_t event_type, const void* data) {
    if (adapter->config.enable_events && adapter->event_callback) {
        adapter->event_callback(event_type, data, adapter->event_user_data);
    }
}

/**
 * @brief Set error state
 */
static void set_error(occipital_adapter_t* adapter, occipital_error_t error) {
    if (!adapter) return;
    adapter->last_error = error;
    if (error != OCCIPITAL_ERROR_NONE) {
        adapter->status = OCCIPITAL_STATUS_ERROR;
        LOG_ERROR(OCCIPITAL_LOG_MODULE, "Error set: %d", error);
    }
}

/**
 * @brief Apply attention modulation to feature
 */
static float apply_attention(const occipital_adapter_t* adapter,
                             float x, float y,
                             visual_feature_type_t feature_type) {
    float gain = 1.0f;

    /* Spatial attention */
    if (adapter->spatial_attention_active) {
        float dx = x - adapter->attention_x;
        float dy = y - adapter->attention_y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist < adapter->attention_radius) {
            /* Gaussian attention profile */
            float sigma = adapter->attention_radius / 2.0f;
            float att = expf(-(dist * dist) / (2.0f * sigma * sigma));
            gain *= 1.0f + (adapter->attention_gain - 1.0f) * att;
        }
    }

    /* Feature-based attention */
    if (adapter->feature_attention_active &&
        feature_type == adapter->attended_feature_type) {
        gain *= adapter->feature_attention_gain;
    }

    return gain;
}

/*=============================================================================
 * V1 PROCESSOR IMPLEMENTATION
 *===========================================================================*/

static v1_processor_t* v1_create(uint32_t num_orientations, uint32_t num_scales,
                                  uint32_t width, uint32_t height) {
    v1_processor_t* v1 = nimcp_calloc(1, sizeof(v1_processor_t));
    if (!v1) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "v1 is NULL");

        return NULL;

    }

    v1->num_orientations = num_orientations;
    v1->num_scales = num_scales;
    v1->max_edges = MAX_FEATURES_PER_AREA;

    /* Allocate Gabor filter bank */
    v1->gabor_filters = nimcp_calloc(num_orientations, sizeof(float*));
    if (!v1->gabor_filters) {
        nimcp_free(v1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "set_error: v1->gabor_filters is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < num_orientations; i++) {
        v1->gabor_filters[i] = nimcp_calloc(V1_GABOR_SIZE * V1_GABOR_SIZE, sizeof(float));
        if (!v1->gabor_filters[i]) {
            for (uint32_t j = 0; j < i; j++) nimcp_free(v1->gabor_filters[j]);
            nimcp_free(v1->gabor_filters);
            nimcp_free(v1);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "set_error: v1->gabor_filters is NULL");
            return NULL;
        }

        /* Initialize Gabor filter for this orientation */
        float theta = (float)i * NIMCP_PI_F / (float)num_orientations;
        float sigma = 2.0f;
        float lambda = 4.0f;
        float gamma = 0.5f;

        for (int y = 0; y < V1_GABOR_SIZE; y++) {
            for (int x = 0; x < V1_GABOR_SIZE; x++) {
                float xp = (float)(x - V1_GABOR_SIZE/2);
                float yp = (float)(y - V1_GABOR_SIZE/2);
                float x_theta = xp * cosf(theta) + yp * sinf(theta);
                float y_theta = -xp * sinf(theta) + yp * cosf(theta);
                float gaussian = expf(-(x_theta*x_theta + gamma*gamma*y_theta*y_theta) /
                                      (2.0f * sigma * sigma));
                float sinusoid = cosf(NIMCP_TWO_PI_F * x_theta / lambda);
                v1->gabor_filters[i][y * V1_GABOR_SIZE + x] = gaussian * sinusoid;
            }
        }
    }

    /* Allocate response maps */
    uint32_t map_size = width * height;
    v1->orientation_responses = nimcp_calloc(map_size * num_orientations, sizeof(float));
    v1->contrast_map = nimcp_calloc(map_size, sizeof(float));
    v1->edges = nimcp_calloc(v1->max_edges, sizeof(visual_feature_t));

    if (!v1->orientation_responses || !v1->contrast_map || !v1->edges) {
        for (uint32_t i = 0; i < num_orientations; i++) {
            if (v1->gabor_filters[i]) nimcp_free(v1->gabor_filters[i]);
        }
        nimcp_free(v1->gabor_filters);
        if (v1->orientation_responses) nimcp_free(v1->orientation_responses);
        if (v1->contrast_map) nimcp_free(v1->contrast_map);
        if (v1->edges) nimcp_free(v1->edges);
        nimcp_free(v1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "v1_create: allocation failed");
        return NULL;
    }

    return v1;
}

static void v1_destroy(v1_processor_t* v1) {
    if (!v1) return;

    if (v1->gabor_filters) {
        for (uint32_t i = 0; i < v1->num_orientations; i++) {
            if (v1->gabor_filters[i]) nimcp_free(v1->gabor_filters[i]);
        }
        nimcp_free(v1->gabor_filters);
    }
    if (v1->orientation_responses) nimcp_free(v1->orientation_responses);
    if (v1->contrast_map) nimcp_free(v1->contrast_map);
    if (v1->edges) nimcp_free(v1->edges);
    nimcp_free(v1);
}

static bool v1_process(v1_processor_t* v1, const float* input,
                       uint32_t width, uint32_t height, uint32_t channels,
                       const occipital_adapter_t* adapter) {
    if (!v1 || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "v1_destroy: required parameter is NULL (v1, input)");
        return false;
    }

    v1->edge_count = 0;

    /* Process each orientation */
    for (uint32_t ori = 0; ori < v1->num_orientations; ori++) {
        float* response = &v1->orientation_responses[ori * width * height];
        float* kernel = v1->gabor_filters[ori];
        int half = V1_GABOR_SIZE / 2;

        /* Convolve with Gabor filter */
        for (uint32_t y = half; y < height - half; y++) {
            for (uint32_t x = half; x < width - half; x++) {
                float sum = 0.0f;
                for (int ky = -half; ky <= half; ky++) {
                    for (int kx = -half; kx <= half; kx++) {
                        /* Use grayscale (average of channels) */
                        float pixel = 0.0f;
                        for (uint32_t c = 0; c < channels; c++) {
                            pixel += input[c * width * height + (y + ky) * width + (x + kx)];
                        }
                        pixel /= (float)channels;
                        sum += pixel * kernel[(ky + half) * V1_GABOR_SIZE + (kx + half)];
                    }
                }
                response[y * width + x] = fabsf(sum);
            }
        }
    }

    /* Detect edges (local maxima across orientations) */
    float threshold = 0.1f;
    for (uint32_t y = 1; y < height - 1 && v1->edge_count < v1->max_edges; y++) {
        for (uint32_t x = 1; x < width - 1 && v1->edge_count < v1->max_edges; x++) {
            float max_response = 0.0f;
            uint32_t max_ori = 0;

            for (uint32_t ori = 0; ori < v1->num_orientations; ori++) {
                float r = v1->orientation_responses[ori * width * height + y * width + x];
                if (r > max_response) {
                    max_response = r;
                    max_ori = ori;
                }
            }

            if (max_response > threshold) {
                /* Apply attention modulation */
                float att_gain = apply_attention(adapter,
                    (float)x / (float)width,
                    (float)y / (float)height,
                    VISUAL_FEATURE_EDGE);

                visual_feature_t* edge = &v1->edges[v1->edge_count];
                edge->type = VISUAL_FEATURE_EDGE;
                edge->source_area = VISUAL_AREA_V1;
                edge->x = (float)x / (float)width;
                edge->y = (float)y / (float)height;
                edge->scale = 1.0f;
                edge->orientation = (float)max_ori * NIMCP_PI_F / (float)v1->num_orientations;
                edge->strength = nimcp_clampf(max_response * att_gain, 0.0f, 1.0f);
                edge->descriptor = NULL;
                edge->descriptor_size = 0;
                v1->edge_count++;
            }
        }
    }

    v1->total_edges += v1->edge_count;
    return true;
}

/*=============================================================================
 * V2 PROCESSOR IMPLEMENTATION
 *===========================================================================*/

static v2_processor_t* v2_create(uint32_t width, uint32_t height) {
    v2_processor_t* v2 = nimcp_calloc(1, sizeof(v2_processor_t));
    if (!v2) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "v2 is NULL");

        return NULL;

    }

    v2->max_contours = MAX_FEATURES_PER_AREA;
    v2->contour_strength = nimcp_calloc(width * height, sizeof(float));
    v2->contours = nimcp_calloc(v2->max_contours, sizeof(visual_feature_t));

    if (!v2->contour_strength || !v2->contours) {
        if (v2->contour_strength) nimcp_free(v2->contour_strength);
        if (v2->contours) nimcp_free(v2->contours);
        nimcp_free(v2);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "v2_create: validation failed");
        return NULL;
    }

    return v2;
}

static void v2_destroy(v2_processor_t* v2) {
    if (!v2) return;
    if (v2->contour_strength) nimcp_free(v2->contour_strength);
    if (v2->contours) nimcp_free(v2->contours);
    if (v2->association_field) nimcp_free(v2->association_field);
    if (v2->texture_features) nimcp_free(v2->texture_features);
    nimcp_free(v2);
}

static bool v2_process(v2_processor_t* v2, const v1_processor_t* v1,
                       uint32_t width, uint32_t height,
                       const occipital_adapter_t* adapter) {
    if (!v2 || !v1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "v2_destroy: required parameter is NULL (v2, v1)");
        return false;
    }

    v2->contour_count = 0;

    /* Contour integration: link nearby edges with similar orientations */
    for (uint32_t i = 0; i < v1->edge_count && v2->contour_count < v2->max_contours; i++) {
        const visual_feature_t* edge = &v1->edges[i];

        /* Simple contour: edges that are strong enough */
        if (edge->strength > 0.3f) {
            float att_gain = apply_attention(adapter, edge->x, edge->y, VISUAL_FEATURE_ORIENTATION);

            visual_feature_t* contour = &v2->contours[v2->contour_count];
            contour->type = VISUAL_FEATURE_ORIENTATION;
            contour->source_area = VISUAL_AREA_V2;
            contour->x = edge->x;
            contour->y = edge->y;
            contour->scale = edge->scale;
            contour->orientation = edge->orientation;
            contour->strength = nimcp_clampf(edge->strength * 1.2f * att_gain, 0.0f, 1.0f);
            contour->descriptor = NULL;
            contour->descriptor_size = 0;
            v2->contour_count++;
        }
    }

    return true;
}

/*=============================================================================
 * V3 PROCESSOR IMPLEMENTATION (Stub)
 *===========================================================================*/

static v3_processor_t* v3_create(uint32_t width, uint32_t height) {
    v3_processor_t* v3 = nimcp_calloc(1, sizeof(v3_processor_t));
    return v3;
}

static void v3_destroy(v3_processor_t* v3) {
    if (v3) {
        if (v3->form_features) nimcp_free(v3->form_features);
        nimcp_free(v3);
    }
}

/*=============================================================================
 * V4 PROCESSOR IMPLEMENTATION
 *===========================================================================*/

static v4_processor_t* v4_create(uint32_t width, uint32_t height, uint32_t color_space) {
    v4_processor_t* v4 = nimcp_calloc(1, sizeof(v4_processor_t));
    if (!v4) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "v4 is NULL");

        return NULL;

    }

    v4->max_colors = MAX_COLOR_PERCEPTS;
    v4->max_forms = MAX_FEATURES_PER_AREA;
    v4->color_space = color_space;

    v4->color_percepts = nimcp_calloc(v4->max_colors, sizeof(color_percept_t));
    v4->complex_forms = nimcp_calloc(v4->max_forms, sizeof(visual_feature_t));

    if (!v4->color_percepts || !v4->complex_forms) {
        if (v4->color_percepts) nimcp_free(v4->color_percepts);
        if (v4->complex_forms) nimcp_free(v4->complex_forms);
        nimcp_free(v4);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "v4_create: validation failed");
        return NULL;
    }

    return v4;
}

static void v4_destroy(v4_processor_t* v4) {
    if (!v4) return;
    if (v4->color_constants) nimcp_free(v4->color_constants);
    if (v4->color_percepts) nimcp_free(v4->color_percepts);
    if (v4->complex_forms) nimcp_free(v4->complex_forms);
    nimcp_free(v4);
}

static bool v4_process(v4_processor_t* v4, const float* input,
                       uint32_t width, uint32_t height, uint32_t channels,
                       const occipital_adapter_t* adapter) {
    if (!v4 || !input || channels < 3) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "v4_destroy: required parameter is NULL (v4, input)");
        return false;
    }

    v4->color_count = 0;
    v4->form_count = 0;

    /* Simple color region detection using grid sampling */
    uint32_t grid_step = 16;
    for (uint32_t y = 0; y < height && v4->color_count < v4->max_colors; y += grid_step) {
        for (uint32_t x = 0; x < width && v4->color_count < v4->max_colors; x += grid_step) {
            /* Sample RGB */
            float r = input[0 * width * height + y * width + x];
            float g = input[1 * width * height + y * width + x];
            float b = input[2 * width * height + y * width + x];

            /* Convert to HSV */
            float max_c = fmaxf(r, fmaxf(g, b));
            float min_c = fminf(r, fminf(g, b));
            float delta = max_c - min_c;

            float h = 0.0f, s = 0.0f, v_val = max_c;
            if (delta > 0.001f) {
                s = delta / max_c;
                if (max_c == r) {
                    h = 60.0f * fmodf((g - b) / delta, 6.0f);
                } else if (max_c == g) {
                    h = 60.0f * ((b - r) / delta + 2.0f);
                } else {
                    h = 60.0f * ((r - g) / delta + 4.0f);
                }
                if (h < 0) h += 360.0f;
            }

            /* Only record salient colors */
            if (s > 0.2f && v_val > 0.2f) {
                float att_gain = apply_attention(adapter,
                    (float)x / (float)width,
                    (float)y / (float)height,
                    VISUAL_FEATURE_COLOR);

                color_percept_t* percept = &v4->color_percepts[v4->color_count];
                percept->hue = h;
                percept->saturation = nimcp_clampf(s * att_gain, 0.0f, 1.0f);
                percept->brightness = v_val;
                percept->x = (float)x / (float)width;
                percept->y = (float)y / (float)height;
                percept->size = (float)grid_step / (float)width;
                v4->color_count++;
            }
        }
    }

    return true;
}

/*=============================================================================
 * V5/MT PROCESSOR IMPLEMENTATION
 *===========================================================================*/

static v5_mt_processor_t* v5_create(uint32_t width, uint32_t height, uint32_t num_frames) {
    v5_mt_processor_t* v5 = nimcp_calloc(1, sizeof(v5_mt_processor_t));
    if (!v5) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "v5 is NULL");

        return NULL;

    }

    v5->max_motions = MAX_MOTION_VECTORS;
    v5->frame_buffer_size = num_frames;

    v5->motion_vectors = nimcp_calloc(v5->max_motions, sizeof(motion_vector_t));
    v5->frame_buffer = nimcp_calloc(num_frames, sizeof(float*));
    v5->optic_flow_x = nimcp_calloc(width * height, sizeof(float));
    v5->optic_flow_y = nimcp_calloc(width * height, sizeof(float));

    if (!v5->motion_vectors || !v5->frame_buffer ||
        !v5->optic_flow_x || !v5->optic_flow_y) {
        if (v5->motion_vectors) nimcp_free(v5->motion_vectors);
        if (v5->frame_buffer) nimcp_free(v5->frame_buffer);
        if (v5->optic_flow_x) nimcp_free(v5->optic_flow_x);
        if (v5->optic_flow_y) nimcp_free(v5->optic_flow_y);
        nimcp_free(v5);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "v5_create: allocation failed");
        return NULL;
    }

    for (uint32_t i = 0; i < num_frames; i++) {
        v5->frame_buffer[i] = nimcp_calloc(width * height, sizeof(float));
        if (!v5->frame_buffer[i]) {
            for (uint32_t j = 0; j < i; j++) nimcp_free(v5->frame_buffer[j]);
            nimcp_free(v5->frame_buffer);
            nimcp_free(v5->motion_vectors);
            nimcp_free(v5->optic_flow_x);
            nimcp_free(v5->optic_flow_y);
            nimcp_free(v5);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "v5_create: v5->frame_buffer is NULL");
            return NULL;
        }
    }

    return v5;
}

static void v5_destroy(v5_mt_processor_t* v5) {
    if (!v5) return;
    if (v5->motion_vectors) nimcp_free(v5->motion_vectors);
    if (v5->frame_buffer) {
        for (uint32_t i = 0; i < v5->frame_buffer_size; i++) {
            if (v5->frame_buffer[i]) nimcp_free(v5->frame_buffer[i]);
        }
        nimcp_free(v5->frame_buffer);
    }
    if (v5->motion_energy) nimcp_free(v5->motion_energy);
    if (v5->optic_flow_x) nimcp_free(v5->optic_flow_x);
    if (v5->optic_flow_y) nimcp_free(v5->optic_flow_y);
    nimcp_free(v5);
}

static bool v5_process(v5_mt_processor_t* v5, const float* input,
                       uint32_t width, uint32_t height, uint32_t channels,
                       const occipital_adapter_t* adapter) {
    if (!v5 || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "v5_destroy: required parameter is NULL (v5, input)");
        return false;
    }

    /* Store current frame (grayscale) */
    float* current_frame = v5->frame_buffer[v5->current_frame_idx];
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            float pixel = 0.0f;
            for (uint32_t c = 0; c < channels; c++) {
                pixel += input[c * width * height + y * width + x];
            }
            current_frame[y * width + x] = pixel / (float)channels;
        }
    }

    v5->motion_count = 0;
    v5->global_dx = 0.0f;
    v5->global_dy = 0.0f;

    /* Simple frame differencing for motion */
    uint32_t prev_idx = (v5->current_frame_idx + v5->frame_buffer_size - 1) % v5->frame_buffer_size;
    float* prev_frame = v5->frame_buffer[prev_idx];

    /* Detect motion in grid cells */
    uint32_t grid_step = 16;
    uint32_t motion_samples = 0;

    for (uint32_t y = grid_step; y < height - grid_step && v5->motion_count < v5->max_motions; y += grid_step) {
        for (uint32_t x = grid_step; x < width - grid_step && v5->motion_count < v5->max_motions; x += grid_step) {
            /* Compute temporal difference */
            float diff = 0.0f;
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    diff += fabsf(current_frame[(y + dy) * width + (x + dx)] -
                                 prev_frame[(y + dy) * width + (x + dx)]);
                }
            }
            diff /= 25.0f;

            if (diff > 0.05f) {
                /* Simple block matching for motion vector */
                float best_dx = 0.0f, best_dy = 0.0f;
                float best_match = 1e9f;

                for (int sdy = -4; sdy <= 4; sdy++) {
                    for (int sdx = -4; sdx <= 4; sdx++) {
                        float match = 0.0f;
                        for (int by = -2; by <= 2; by++) {
                            for (int bx = -2; bx <= 2; bx++) {
                                int curr_y = (int)y + by;
                                int curr_x = (int)x + bx;
                                int prev_y = curr_y + sdy;
                                int prev_x = curr_x + sdx;
                                if (prev_y >= 0 && prev_y < (int)height &&
                                    prev_x >= 0 && prev_x < (int)width) {
                                    match += fabsf(current_frame[curr_y * width + curr_x] -
                                                  prev_frame[prev_y * width + prev_x]);
                                }
                            }
                        }
                        if (match < best_match) {
                            best_match = match;
                            best_dx = (float)sdx;
                            best_dy = (float)sdy;
                        }
                    }
                }

                if (fabsf(best_dx) > 0.5f || fabsf(best_dy) > 0.5f) {
                    float att_gain = apply_attention(adapter,
                        (float)x / (float)width,
                        (float)y / (float)height,
                        VISUAL_FEATURE_MOTION);

                    motion_vector_t* mv = &v5->motion_vectors[v5->motion_count];
                    mv->x = (float)x / (float)width;
                    mv->y = (float)y / (float)height;
                    mv->dx = best_dx * att_gain;
                    mv->dy = best_dy * att_gain;
                    mv->confidence = nimcp_clampf(1.0f - best_match / 25.0f, 0.0f, 1.0f);
                    v5->motion_count++;

                    v5->global_dx += best_dx;
                    v5->global_dy += best_dy;
                    motion_samples++;
                }
            }
        }
    }

    if (motion_samples > 0) {
        v5->global_dx /= (float)motion_samples;
        v5->global_dy /= (float)motion_samples;
    }

    v5->current_frame_idx = (v5->current_frame_idx + 1) % v5->frame_buffer_size;
    return true;
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS (Forward declarations)
 *===========================================================================*/

static nimcp_error_t handle_visual_input_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_attention_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_feature_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/*=============================================================================
 * KG-DRIVEN WIRING CALLBACK
 *===========================================================================*/

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * Called by the wiring orchestrator with message types discovered from KG
 * HANDLES_MESSAGE relations. Registers the appropriate handlers dynamically.
 */
static int occipital_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;
    }

    occipital_adapter_t* adapter = (occipital_adapter_t*)user_data;
    if (!adapter) {
        LOG_WARNING(OCCIPITAL_LOG_MODULE, "Wiring callback: adapter is NULL");
        return 0;
    }

    LOG_DEBUG(OCCIPITAL_LOG_MODULE,
              "KG wiring callback: registering %u message handlers", message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_VISUAL_INPUT_REQUEST:
                bio_router_register_handler(ctx, message_types[i],
                    handle_visual_input_request);
                LOG_DEBUG(OCCIPITAL_LOG_MODULE,
                          "Registered handler: BIO_MSG_VISUAL_INPUT_REQUEST");
                break;

            case BIO_MSG_ATTENTION_MODULATION:
                bio_router_register_handler(ctx, message_types[i],
                    handle_attention_request);
                LOG_DEBUG(OCCIPITAL_LOG_MODULE,
                          "Registered handler: BIO_MSG_ATTENTION_MODULATION");
                break;

            case BIO_MSG_VISUAL_FEATURE_QUERY:
                bio_router_register_handler(ctx, message_types[i],
                    handle_feature_query);
                LOG_DEBUG(OCCIPITAL_LOG_MODULE,
                          "Registered handler: BIO_MSG_VISUAL_FEATURE_QUERY");
                break;

            default:
                LOG_DEBUG(OCCIPITAL_LOG_MODULE,
                          "Unknown message type 0x%04X in wiring callback",
                          message_types[i]);
                break;
        }
    }

    return 0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

occipital_config_t occipital_default_config(void) {
    occipital_config_t config;

    /* Image dimensions */
    config.image_width = OCCIPITAL_DEFAULT_IMAGE_WIDTH;
    config.image_height = OCCIPITAL_DEFAULT_IMAGE_HEIGHT;
    config.color_channels = OCCIPITAL_DEFAULT_COLOR_CHANNELS;

    /* V1 configuration */
    config.v1_num_orientations = OCCIPITAL_DEFAULT_NUM_ORIENTATIONS;
    config.v1_num_scales = OCCIPITAL_DEFAULT_NUM_SCALES;
    config.v1_enable_gabor = true;
    config.v1_enable_contrast_norm = true;

    /* V2 configuration */
    config.v2_enable_contour = true;
    config.v2_enable_texture = true;
    config.v2_enable_figure_ground = true;

    /* V4 configuration */
    config.v4_enable_color = true;
    config.v4_enable_complex_form = true;
    config.v4_color_space = 0;  /* RGB */

    /* V5/MT configuration */
    config.v5_enable_motion = true;
    config.v5_motion_frames = OCCIPITAL_DEFAULT_MOTION_FRAMES;
    config.v5_enable_optic_flow = true;

    /* Processing options */
    config.active_stream = VISUAL_STREAM_BOTH;
    config.max_features = OCCIPITAL_DEFAULT_MAX_FEATURES;
    config.enable_attention = true;
    config.enable_feedback = false;

    /* Event system */
    config.enable_events = true;

    /* Training */
    config.enable_training = false;
    config.learning_rate = NIMCP_LEARNING_RATE_FINE;

    /* Bio-async: enabled by default, use dopamine for visual processing */
    config.enable_bio_async = true;
    config.default_channel = BIO_CHANNEL_DOPAMINE;

    return config;
}

occipital_adapter_t* occipital_create(const occipital_config_t* config) {
    LOG_INFO(OCCIPITAL_LOG_MODULE, "Creating occipital cortex adapter");

    occipital_adapter_t* adapter = nimcp_calloc(1, sizeof(occipital_adapter_t));
    if (!adapter) {
        LOG_ERROR(OCCIPITAL_LOG_MODULE, "Failed to allocate adapter memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_create: adapter is NULL");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        adapter->config = *config;
        LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Using provided configuration");
    } else {
        adapter->config = occipital_default_config();
        LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Using default configuration");
    }

    uint32_t width = adapter->config.image_width;
    uint32_t height = adapter->config.image_height;

    /* Create V1 processor */
    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Creating V1 processor");
    adapter->v1 = v1_create(adapter->config.v1_num_orientations,
                            adapter->config.v1_num_scales,
                            width, height);
    if (!adapter->v1) {
        LOG_ERROR(OCCIPITAL_LOG_MODULE, "Failed to create V1 processor");
        occipital_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_create: adapter->v1 is NULL");
        return NULL;
    }

    /* Create V2 processor */
    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Creating V2 processor");
    adapter->v2 = v2_create(width, height);
    if (!adapter->v2) {
        LOG_ERROR(OCCIPITAL_LOG_MODULE, "Failed to create V2 processor");
        occipital_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_create: adapter->v2 is NULL");
        return NULL;
    }

    /* Create V3 processor */
    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Creating V3 processor");
    adapter->v3 = v3_create(width, height);
    if (!adapter->v3) {
        LOG_ERROR(OCCIPITAL_LOG_MODULE, "Failed to create V3 processor");
        occipital_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_create: adapter->v3 is NULL");
        return NULL;
    }

    /* Create V4 processor */
    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Creating V4 processor");
    adapter->v4 = v4_create(width, height, adapter->config.v4_color_space);
    if (!adapter->v4) {
        LOG_ERROR(OCCIPITAL_LOG_MODULE, "Failed to create V4 processor");
        occipital_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_create: adapter->v4 is NULL");
        return NULL;
    }

    /* Create V5/MT processor */
    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Creating V5/MT processor");
    adapter->v5 = v5_create(width, height, adapter->config.v5_motion_frames);
    if (!adapter->v5) {
        LOG_ERROR(OCCIPITAL_LOG_MODULE, "Failed to create V5/MT processor");
        occipital_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_create: adapter->v5 is NULL");
        return NULL;
    }

    /* Allocate input buffer */
    uint32_t buffer_size = width * height * adapter->config.color_channels;
    adapter->input_buffer = nimcp_calloc(buffer_size, sizeof(float));
    if (!adapter->input_buffer) {
        LOG_ERROR(OCCIPITAL_LOG_MODULE, "Failed to allocate input buffer");
        occipital_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_create: adapter->input_buffer is NULL");
        return NULL;
    }

    /* Initialize memory pool for feature descriptors */
    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Creating descriptor memory pool");
    memory_pool_config_t pool_config = {
        .block_size = FEATURE_DESCRIPTOR_SIZE * sizeof(float),
        .num_blocks = MAX_FEATURES_PER_AREA * VISUAL_AREA_COUNT,
        .alignment = 16,
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    adapter->descriptor_pool = memory_pool_create(&pool_config);
    if (!adapter->descriptor_pool) {
        LOG_ERROR(OCCIPITAL_LOG_MODULE, "Failed to create descriptor pool");
        occipital_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_create: adapter->descriptor_pool is NULL");
        return NULL;
    }

    /* Initialize bio-async communication */
    adapter->bio_ctx = NULL;
    adapter->default_channel = adapter->config.default_channel;

    if (adapter->config.enable_bio_async && bio_router_is_initialized()) {
        LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Registering with bio-async router");

        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_OCCIPITAL,
            .module_name = "occipital_cortex",
            .inbox_capacity = 64,
            .user_data = adapter
        };

        adapter->bio_ctx = bio_router_register_module(&bio_info);
        if (adapter->bio_ctx) {
            /* Try KG-driven wiring callback first */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_OCCIPITAL,
                (void*)occipital_wiring_handler_callback,
                adapter
            );

            if (cb_result != NIMCP_SUCCESS) {
                /* Fall back to legacy direct registration */
                LOG_DEBUG(OCCIPITAL_LOG_MODULE,
                          "KG wiring not available, using legacy registration");

                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_VISUAL_INPUT_REQUEST, handle_visual_input_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_ATTENTION_MODULATION, handle_attention_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_VISUAL_FEATURE_QUERY, handle_feature_query)
                );
            }

            LOG_INFO(OCCIPITAL_LOG_MODULE, "Bio-async handlers registered");
        } else {
            LOG_WARNING(OCCIPITAL_LOG_MODULE, "Failed to register with bio-async router");
        }
    }

    /* Initialize state */
    adapter->status = OCCIPITAL_STATUS_IDLE;
    adapter->last_error = OCCIPITAL_ERROR_NONE;
    adapter->has_input = false;
    adapter->spatial_attention_active = false;
    adapter->feature_attention_active = false;

    LOG_INFO(OCCIPITAL_LOG_MODULE, "Occipital cortex adapter created successfully");
    return adapter;
}

void occipital_destroy(occipital_adapter_t* adapter) {
    if (!adapter) return;

    LOG_INFO(OCCIPITAL_LOG_MODULE, "Destroying occipital cortex adapter");

    /* Unregister from bio-async router */
    if (adapter->bio_ctx) {
        LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Unregistering from bio-async router");
        bio_router_unregister_module(adapter->bio_ctx);
        adapter->bio_ctx = NULL;
    }

    /* Destroy sub-modules */
    if (adapter->v1) {
        LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Destroying V1 processor");
        v1_destroy(adapter->v1);
    }
    if (adapter->v2) {
        LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Destroying V2 processor");
        v2_destroy(adapter->v2);
    }
    if (adapter->v3) {
        LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Destroying V3 processor");
        v3_destroy(adapter->v3);
    }
    if (adapter->v4) {
        LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Destroying V4 processor");
        v4_destroy(adapter->v4);
    }
    if (adapter->v5) {
        LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Destroying V5/MT processor");
        v5_destroy(adapter->v5);
    }

    /* Free input buffer */
    if (adapter->input_buffer) {
        LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Freeing input buffer");
        nimcp_free(adapter->input_buffer);
    }

    /* Destroy memory pool */
    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Destroying descriptor pool");
    memory_pool_destroy(adapter->descriptor_pool);

    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Occipital cortex adapter destroyed");
    nimcp_free(adapter);
}

bool occipital_reset(occipital_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_reset: adapter is NULL");
        return false;
    }

    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Resetting adapter state");

    /* Reset state */
    adapter->status = OCCIPITAL_STATUS_IDLE;
    adapter->last_error = OCCIPITAL_ERROR_NONE;
    adapter->has_input = false;
    adapter->spatial_attention_active = false;
    adapter->feature_attention_active = false;

    /* Clear processor states */
    if (adapter->v1) adapter->v1->edge_count = 0;
    if (adapter->v2) adapter->v2->contour_count = 0;
    if (adapter->v4) {
        adapter->v4->color_count = 0;
        adapter->v4->form_count = 0;
    }
    if (adapter->v5) adapter->v5->motion_count = 0;

    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Adapter reset complete");
    return true;
}

/*=============================================================================
 * VISUAL PROCESSING PIPELINE
 *===========================================================================*/

bool occipital_set_input(occipital_adapter_t* adapter, const visual_input_t* input) {
    if (!adapter || !input || !input->data) {
        set_error(adapter, OCCIPITAL_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_set_input: required parameter is NULL (adapter, input, input->data)");
        return false;
    }

    /* Validate dimensions */
    if (input->width != adapter->config.image_width ||
        input->height != adapter->config.image_height ||
        input->channels != adapter->config.color_channels) {
        LOG_ERROR(OCCIPITAL_LOG_MODULE, "Input dimensions mismatch: expected %ux%ux%u, got %ux%ux%u",
                  adapter->config.image_width, adapter->config.image_height,
                  adapter->config.color_channels,
                  input->width, input->height, input->channels);
        set_error(adapter, OCCIPITAL_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_set_input: operation failed");
        return false;
    }

    /* Copy input data */
    uint32_t buffer_size = input->width * input->height * input->channels;
    memcpy(adapter->input_buffer, input->data, buffer_size * sizeof(float));

    adapter->current_input = *input;
    adapter->current_input.data = adapter->input_buffer;
    adapter->has_input = true;
    adapter->status = OCCIPITAL_STATUS_IDLE;

    return true;
}

bool occipital_process(occipital_adapter_t* adapter, visual_processing_result_t* result) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_process: adapter is NULL");
        return false;
    }

    if (!adapter->has_input) {
        set_error(adapter, OCCIPITAL_ERROR_NO_INPUT);
        /* No input available — caller should set input first, not immune-worthy */
        return false;
    }

    visual_processing_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));

    uint32_t width = adapter->config.image_width;
    uint32_t height = adapter->config.image_height;
    uint32_t channels = adapter->config.color_channels;

    /* Phase 1: V1 processing */
    adapter->status = OCCIPITAL_STATUS_V1_PROCESSING;
    if (!v1_process(adapter->v1, adapter->input_buffer, width, height, channels, adapter)) {
        set_error(adapter, OCCIPITAL_ERROR_V1_FAILURE);
        if (result) *result = local_result;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_process: validation failed");
        return false;
    }
    local_result.v1_processed = true;
    local_result.edge_count = adapter->v1->edge_count;

    /* Build orientation histogram */
    for (uint32_t i = 0; i < adapter->v1->edge_count; i++) {
        uint32_t bin = (uint32_t)(adapter->v1->edges[i].orientation * 8.0f / NIMCP_PI_F) % 8;
        local_result.orientation_histogram[bin]++;
    }

    emit_event(adapter, 1 /* V1_COMPLETE */, NULL);

    /* Phase 2: V2 processing */
    adapter->status = OCCIPITAL_STATUS_V2_PROCESSING;
    if (!v2_process(adapter->v2, adapter->v1, width, height, adapter)) {
        set_error(adapter, OCCIPITAL_ERROR_V2_FAILURE);
        if (result) *result = local_result;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_process: validation failed");
        return false;
    }
    local_result.v2_processed = true;
    local_result.contour_count = adapter->v2->contour_count;
    local_result.figure_ground_segmented = adapter->config.v2_enable_figure_ground;

    emit_event(adapter, 2 /* V2_COMPLETE */, NULL);

    /* Phase 3: V4 processing (ventral stream) */
    if (adapter->config.active_stream == VISUAL_STREAM_VENTRAL ||
        adapter->config.active_stream == VISUAL_STREAM_BOTH) {
        adapter->status = OCCIPITAL_STATUS_V4_PROCESSING;
        if (!v4_process(adapter->v4, adapter->input_buffer, width, height, channels, adapter)) {
            set_error(adapter, OCCIPITAL_ERROR_V4_FAILURE);
            if (result) *result = local_result;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_process: validation failed");
            return false;
        }
        local_result.v4_processed = true;
        local_result.color_region_count = adapter->v4->color_count;
        local_result.complex_form_count = adapter->v4->form_count;

        emit_event(adapter, 3 /* V4_COMPLETE */, NULL);
    }

    /* Phase 4: V5/MT processing (dorsal stream) */
    if (adapter->config.active_stream == VISUAL_STREAM_DORSAL ||
        adapter->config.active_stream == VISUAL_STREAM_BOTH) {
        adapter->status = OCCIPITAL_STATUS_V5_PROCESSING;
        if (!v5_process(adapter->v5, adapter->input_buffer, width, height, channels, adapter)) {
            set_error(adapter, OCCIPITAL_ERROR_V5_FAILURE);
            if (result) *result = local_result;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_process: validation failed");
            return false;
        }
        local_result.v5_processed = true;
        local_result.motion_vector_count = adapter->v5->motion_count;
        local_result.global_motion_dx = adapter->v5->global_dx;
        local_result.global_motion_dy = adapter->v5->global_dy;

        emit_event(adapter, 4 /* V5_COMPLETE */, NULL);
    }

    /* Integration */
    adapter->status = OCCIPITAL_STATUS_INTEGRATION;
    local_result.total_features = local_result.edge_count + local_result.contour_count +
                                  local_result.color_region_count + local_result.motion_vector_count;
    local_result.ready_for_downstream = true;

    /* Update statistics */
    adapter->stats.frames_processed++;
    adapter->stats.features_extracted += local_result.total_features;
    adapter->stats.edges_detected += local_result.edge_count;
    adapter->stats.motions_detected += local_result.motion_vector_count;
    adapter->stats.successful_frames++;

    /* Invoke feature callbacks */
    if (adapter->feature_callback) {
        for (uint32_t i = 0; i < adapter->v1->edge_count; i++) {
            adapter->feature_callback(&adapter->v1->edges[i], adapter->feature_user_data);
        }
    }

    if (adapter->motion_callback) {
        for (uint32_t i = 0; i < adapter->v5->motion_count; i++) {
            adapter->motion_callback(&adapter->v5->motion_vectors[i], adapter->motion_user_data);
        }
    }

    adapter->status = OCCIPITAL_STATUS_READY;
    emit_event(adapter, 5 /* PROCESSING_COMPLETE */, &local_result);

    if (result) *result = local_result;
    return true;
}

bool occipital_process_v1(occipital_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_process_v1: adapter is NULL");
        return false;
    }
    if (!adapter->has_input) return false;

    adapter->status = OCCIPITAL_STATUS_V1_PROCESSING;
    bool success = v1_process(adapter->v1, adapter->input_buffer,
                              adapter->config.image_width, adapter->config.image_height,
                              adapter->config.color_channels, adapter);
    if (!success) {
        set_error(adapter, OCCIPITAL_ERROR_V1_FAILURE);
    }
    return success;
}

bool occipital_process_v2(occipital_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_process_v2: adapter is NULL");
        return false;
    }
    if (!adapter->v1) return false;

    adapter->status = OCCIPITAL_STATUS_V2_PROCESSING;
    bool success = v2_process(adapter->v2, adapter->v1,
                              adapter->config.image_width, adapter->config.image_height,
                              adapter);
    if (!success) {
        set_error(adapter, OCCIPITAL_ERROR_V2_FAILURE);
    }
    return success;
}

bool occipital_process_v4(occipital_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_process_v4: adapter is NULL");
        return false;
    }
    if (!adapter->has_input) return false;

    adapter->status = OCCIPITAL_STATUS_V4_PROCESSING;
    bool success = v4_process(adapter->v4, adapter->input_buffer,
                              adapter->config.image_width, adapter->config.image_height,
                              adapter->config.color_channels, adapter);
    if (!success) {
        set_error(adapter, OCCIPITAL_ERROR_V4_FAILURE);
    }
    return success;
}

bool occipital_process_v5(occipital_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_process_v5: adapter is NULL");
        return false;
    }
    if (!adapter->has_input) return false;

    adapter->status = OCCIPITAL_STATUS_V5_PROCESSING;
    bool success = v5_process(adapter->v5, adapter->input_buffer,
                              adapter->config.image_width, adapter->config.image_height,
                              adapter->config.color_channels, adapter);
    if (!success) {
        set_error(adapter, OCCIPITAL_ERROR_V5_FAILURE);
    }
    return success;
}

/*=============================================================================
 * FEATURE ACCESS
 *===========================================================================*/

uint32_t occipital_get_feature_count(const occipital_adapter_t* adapter, visual_area_t area) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_get_feature_count: adapter is NULL");
        return 0;
    }

    switch (area) {
        case VISUAL_AREA_V1: return adapter->v1 ? adapter->v1->edge_count : 0;
        case VISUAL_AREA_V2: return adapter->v2 ? adapter->v2->contour_count : 0;
        case VISUAL_AREA_V4: return adapter->v4 ? adapter->v4->color_count + adapter->v4->form_count : 0;
        case VISUAL_AREA_V5_MT: return adapter->v5 ? adapter->v5->motion_count : 0;
        case VISUAL_AREA_COUNT:
            return (adapter->v1 ? adapter->v1->edge_count : 0) +
                   (adapter->v2 ? adapter->v2->contour_count : 0) +
                   (adapter->v4 ? adapter->v4->color_count + adapter->v4->form_count : 0);
        default: return 0;
    }
}

bool occipital_get_feature(const occipital_adapter_t* adapter, visual_area_t area,
                           uint32_t index, visual_feature_t* feature) {
    if (!adapter || !feature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_get_feature: required parameter is NULL");
        return false;
    }

    switch (area) {
        case VISUAL_AREA_V1:
            if (adapter->v1 && index < adapter->v1->edge_count) {
                *feature = adapter->v1->edges[index];
                return true;
            }
            break;
        case VISUAL_AREA_V2:
            if (adapter->v2 && index < adapter->v2->contour_count) {
                *feature = adapter->v2->contours[index];
                return true;
            }
            break;
        case VISUAL_AREA_V4:
            if (adapter->v4 && index < adapter->v4->form_count) {
                *feature = adapter->v4->complex_forms[index];
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

bool occipital_get_features(const occipital_adapter_t* adapter, visual_area_t area,
                            visual_feature_t* features, uint32_t* count) {
    if (!adapter || !features || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_get_features: required parameter is NULL");
        return false;
    }

    uint32_t available = occipital_get_feature_count(adapter, area);
    uint32_t to_copy = (*count < available) ? *count : available;

    switch (area) {
        case VISUAL_AREA_V1:
            if (adapter->v1) {
                memcpy(features, adapter->v1->edges, to_copy * sizeof(visual_feature_t));
            }
            break;
        case VISUAL_AREA_V2:
            if (adapter->v2) {
                memcpy(features, adapter->v2->contours, to_copy * sizeof(visual_feature_t));
            }
            break;
        case VISUAL_AREA_V4:
            if (adapter->v4) {
                memcpy(features, adapter->v4->complex_forms, to_copy * sizeof(visual_feature_t));
            }
            break;
        default:
            *count = 0;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_get_feature_count: validation failed");
            return false;
    }

    *count = to_copy;
    return true;
}

bool occipital_get_motion_vectors(const occipital_adapter_t* adapter,
                                  motion_vector_t* vectors, uint32_t* count) {
    if (!adapter || !vectors || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_get_motion_vectors: required parameter is NULL");
        return false;
    }
    if (!adapter->v5) return false;

    uint32_t to_copy = (*count < adapter->v5->motion_count) ? *count : adapter->v5->motion_count;
    memcpy(vectors, adapter->v5->motion_vectors, to_copy * sizeof(motion_vector_t));
    *count = to_copy;
    return true;
}

bool occipital_get_color_percepts(const occipital_adapter_t* adapter,
                                  color_percept_t* percepts, uint32_t* count) {
    if (!adapter || !percepts || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_get_color_percepts: required parameter is NULL");
        return false;
    }
    if (!adapter->v4) return false;

    uint32_t to_copy = (*count < adapter->v4->color_count) ? *count : adapter->v4->color_count;
    memcpy(percepts, adapter->v4->color_percepts, to_copy * sizeof(color_percept_t));
    *count = to_copy;
    return true;
}

/*=============================================================================
 * ATTENTION MODULATION
 *===========================================================================*/

bool occipital_apply_spatial_attention(occipital_adapter_t* adapter,
                                       float x, float y, float radius, float gain) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_apply_spatial_attention: adapter is NULL");
        return false;
    }

    adapter->attention_x = nimcp_clampf(x, 0.0f, 1.0f);
    adapter->attention_y = nimcp_clampf(y, 0.0f, 1.0f);
    adapter->attention_radius = nimcp_clampf(radius, 0.0f, 1.0f);
    adapter->attention_gain = gain;
    adapter->spatial_attention_active = true;

    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Spatial attention: (%.2f, %.2f) radius=%.2f gain=%.2f",
              x, y, radius, gain);
    return true;
}

bool occipital_apply_feature_attention(occipital_adapter_t* adapter,
                                       visual_feature_type_t feature_type, float gain) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_apply_feature_attention: adapter is NULL");
        return false;
    }

    adapter->attended_feature_type = feature_type;
    adapter->feature_attention_gain = gain;
    adapter->feature_attention_active = true;

    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Feature attention: type=%d gain=%.2f", feature_type, gain);
    return true;
}

/*=============================================================================
 * CALLBACKS AND EVENTS
 *===========================================================================*/

bool occipital_set_feature_callback(occipital_adapter_t* adapter,
                                    occipital_feature_callback_t callback, void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_set_feature_callback: adapter is NULL");
        return false;
    }
    adapter->feature_callback = callback;
    adapter->feature_user_data = user_data;
    return true;
}

bool occipital_set_motion_callback(occipital_adapter_t* adapter,
                                   occipital_motion_callback_t callback, void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_set_motion_callback: adapter is NULL");
        return false;
    }
    adapter->motion_callback = callback;
    adapter->motion_user_data = user_data;
    return true;
}

bool occipital_set_event_callback(occipital_adapter_t* adapter,
                                  occipital_event_callback_t callback, void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_set_event_callback: adapter is NULL");
        return false;
    }
    adapter->event_callback = callback;
    adapter->event_user_data = user_data;
    return true;
}

/*=============================================================================
 * TRAINING INTERFACE
 *===========================================================================*/

bool occipital_train(occipital_adapter_t* adapter, const visual_feature_t* target_features,
                     uint32_t num_features, float learning_rate) {
    if (!adapter || !target_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_train: required parameter is NULL");
        return false;
    }
    if (num_features == 0) return false;
    if (!adapter->config.enable_training) return false;

    /* Simple training: compare detected features to target and compute loss */
    adapter->stats.training_iterations++;

    if (learning_rate <= 0.0f) {
        learning_rate = adapter->config.learning_rate;
    }

    /* Placeholder training loss computation */
    uint32_t detected = occipital_get_feature_count(adapter, VISUAL_AREA_COUNT);
    float loss = fabsf((float)detected - (float)num_features) / fmaxf((float)num_features, 1.0f);
    adapter->stats.training_loss = loss;

    return true;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

occipital_status_t occipital_get_status(const occipital_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_get_status: adapter is NULL");
        return OCCIPITAL_STATUS_ERROR;
    }
    return adapter->status;
}

occipital_error_t occipital_get_last_error(const occipital_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_get_last_error: adapter is NULL");
        return OCCIPITAL_ERROR_INTERNAL;
    }
    return adapter->last_error;
}

const char* occipital_error_string(occipital_error_t error) {
    switch (error) {
        case OCCIPITAL_ERROR_NONE: return "No error";
        case OCCIPITAL_ERROR_INVALID_INPUT: return "Invalid input";
        case OCCIPITAL_ERROR_V1_FAILURE: return "V1 processing failed";
        case OCCIPITAL_ERROR_V2_FAILURE: return "V2 processing failed";
        case OCCIPITAL_ERROR_V4_FAILURE: return "V4 processing failed";
        case OCCIPITAL_ERROR_V5_FAILURE: return "V5/MT processing failed";
        case OCCIPITAL_ERROR_INTEGRATION_FAILURE: return "Integration failed";
        case OCCIPITAL_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case OCCIPITAL_ERROR_NO_INPUT: return "No input image";
        case OCCIPITAL_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* occipital_status_string(occipital_status_t status) {
    switch (status) {
        case OCCIPITAL_STATUS_IDLE: return "Idle";
        case OCCIPITAL_STATUS_V1_PROCESSING: return "V1 processing";
        case OCCIPITAL_STATUS_V2_PROCESSING: return "V2 processing";
        case OCCIPITAL_STATUS_V4_PROCESSING: return "V4 processing";
        case OCCIPITAL_STATUS_V5_PROCESSING: return "V5/MT processing";
        case OCCIPITAL_STATUS_INTEGRATION: return "Integrating features";
        case OCCIPITAL_STATUS_READY: return "Ready";
        case OCCIPITAL_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

bool occipital_get_stats(const occipital_adapter_t* adapter, occipital_stats_t* stats) {
    if (!adapter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_get_stats: required parameter is NULL");
        return false;
    }
    *stats = adapter->stats;
    return true;
}

bool occipital_get_config(const occipital_adapter_t* adapter, occipital_config_t* config) {
    if (!adapter || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_get_config: required parameter is NULL");
        return false;
    }
    *config = adapter->config;
    return true;
}

/*=============================================================================
 * SUB-MODULE ACCESS
 *===========================================================================*/

v1_processor_t* occipital_get_v1_processor(occipital_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_get_v1_processor: adapter is NULL");
        return NULL;
    }
    return adapter->v1;
}

v2_processor_t* occipital_get_v2_processor(occipital_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_get_v2_processor: adapter is NULL");
        return NULL;
    }
    return adapter->v2;
}

v4_processor_t* occipital_get_v4_processor(occipital_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_get_v4_processor: adapter is NULL");
        return NULL;
    }
    return adapter->v4;
}

v5_mt_processor_t* occipital_get_v5_processor(occipital_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_get_v5_processor: adapter is NULL");
        return NULL;
    }
    return adapter->v5;
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION API
 *===========================================================================*/

bio_module_context_t occipital_get_bio_context(occipital_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_get_bio_context: adapter is NULL");
        return NULL;
    }
    return adapter->bio_ctx;
}

uint32_t occipital_process_bio_messages(occipital_adapter_t* adapter, uint32_t max_messages) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_process_bio_messages: adapter is NULL");
        return 0;
    }
    if (!adapter->bio_ctx) return 0;

    uint32_t processed = bio_router_process_inbox(adapter->bio_ctx, max_messages);
    if (processed > 0) {
        LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Processed %u bio-async messages", processed);
    }
    return processed;
}

nimcp_bio_future_t occipital_request_lgn_input_async(occipital_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_request_lgn_input_async: adapter is NULL");
        return NULL;
    }
    if (!adapter->bio_ctx) {
        LOG_WARNING(OCCIPITAL_LOG_MODULE, "Cannot request LGN input: bio-async not available");
        return NULL;
    }

    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Requesting LGN input");

    /* Create visual input request message */
    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = BIO_MSG_VISUAL_INPUT_REQUEST;
    msg.source_module = BIO_MODULE_OCCIPITAL;
    msg.target_module = BIO_MODULE_THALAMUS;
    msg.payload_size = sizeof(msg);
    msg.channel = adapter->default_channel;

    nimcp_bio_promise_t promise = bio_router_send_async(
        adapter->bio_ctx, &msg, sizeof(msg), adapter->default_channel);

    if (!promise) {
        LOG_ERROR(OCCIPITAL_LOG_MODULE, "Failed to send LGN input request");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_request_lgn_input_async: promise is NULL");
        return NULL;
    }

    return nimcp_bio_promise_get_future(promise);
}

nimcp_error_t occipital_broadcast_features(occipital_adapter_t* adapter,
                                           const visual_processing_result_t* result) {
    if (!adapter || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_broadcast_features: required parameter is NULL");
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    if (!adapter->bio_ctx) {
        LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Cannot broadcast: bio-async not available");
        return NIMCP_SUCCESS;
    }

    LOG_INFO(OCCIPITAL_LOG_MODULE, "Broadcasting features (edges=%u, motions=%u)",
             result->edge_count, result->motion_vector_count);

    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = BIO_MSG_VISUAL_FEATURES_READY;
    msg.source_module = BIO_MODULE_OCCIPITAL;
    msg.target_module = 0;
    msg.payload_size = sizeof(msg);
    msg.channel = adapter->default_channel;
    msg.flags = BIO_MSG_FLAG_BROADCAST;

    return bio_router_broadcast(adapter->bio_ctx, &msg, sizeof(msg));
}

nimcp_error_t occipital_handle_attention(occipital_adapter_t* adapter,
                                         float x, float y,
                                         visual_feature_type_t feature_type,
                                         float gain) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_handle_attention: adapter is NULL");
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Handling attention: pos=(%.2f,%.2f), type=%d, gain=%.2f",
              x, y, feature_type, gain);

    occipital_apply_spatial_attention(adapter, x, y, 0.2f, gain);
    occipital_apply_feature_attention(adapter, feature_type, gain);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS (Implementation)
 *===========================================================================*/

static nimcp_error_t handle_visual_input_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    occipital_adapter_t* adapter = (occipital_adapter_t*)user_data;
    (void)msg;
    (void)msg_size;

    if (!adapter) {
        LOG_ERROR(OCCIPITAL_LOG_MODULE, "Invalid visual input request");
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Handling visual input request");

    /* Process pending input if available */
    if (adapter->has_input) {
        visual_processing_result_t result;
        occipital_process(adapter, &result);

        if (response_promise) {
            nimcp_bio_promise_complete(response_promise, &result);
        }
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_attention_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    occipital_adapter_t* adapter = (occipital_adapter_t*)user_data;
    (void)msg_size;
    (void)response_promise;

    if (!adapter) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    /* Extract attention parameters from message */
    /* For now, use default attention */
    occipital_apply_spatial_attention(adapter, 0.5f, 0.5f, 0.3f, 1.5f);

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_feature_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    occipital_adapter_t* adapter = (occipital_adapter_t*)user_data;
    (void)msg;
    (void)msg_size;

    if (!adapter) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG(OCCIPITAL_LOG_MODULE, "Handling feature query");

    /* Return current feature count */
    uint32_t count = occipital_get_feature_count(adapter, VISUAL_AREA_COUNT);

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &count);
    }

    return NIMCP_SUCCESS;
}
