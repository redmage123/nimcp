/**
 * @file nimcp_entorhinal_omni_bridge.c
 * @brief Implementation of Entorhinal-Omnidirectional System Bridge
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include "core/brain/regions/entorhinal/nimcp_entorhinal_omni_bridge.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(entorhinal_omni_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "ENTORHINAL_OMNI_BRIDGE"


/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/


#define DEFAULT_MAX_TRACKING_DISTANCE   50.0f
#define DEFAULT_MIN_TRACKING_DISTANCE   0.1f
#define DEFAULT_THREAT_WEIGHT           0.4f
#define DEFAULT_OPPORTUNITY_WEIGHT      0.3f
#define DEFAULT_SALIENCE_WEIGHT         0.3f
#define DEFAULT_MEMORY_FAMILIARITY_WEIGHT 0.2f
#define DEFAULT_SPATIAL_MAP_UPDATE_RATE_HZ 30.0f
#define DEFAULT_OBJECT_TRACKING_RATE_HZ 15.0f
#define DEFAULT_MEMORY_OVERLAY_RATE_HZ  5.0f
#define DEFAULT_THREAT_DETECTION_THRESHOLD 0.3f
#define DEFAULT_OPPORTUNITY_DETECTION_THRESHOLD 0.3f
#define DEFAULT_ATTENTION_DECAY_RATE    0.1f
#define DEFAULT_NOVELTY_BOOST           0.3f
#define DEFAULT_THREAT_ATTENTION_BOOST  0.5f

static float normalize_angle(float angle) {
    while (angle < 0.0f) angle += 2.0f * M_PI;
    while (angle >= 2.0f * M_PI) angle -= 2.0f * M_PI;
    return angle;
}

static int angle_to_index(float angle) {
    angle = normalize_angle(angle);
    int idx = (int)(angle * 180.0f / M_PI);
    if (idx >= OMNI_ANGULAR_RESOLUTION) idx = OMNI_ANGULAR_RESOLUTION - 1;
    if (idx < 0) idx = 0;
    return idx;
}

static omni_sector_t angle_to_sector(float angle) {
    angle = normalize_angle(angle);
    float degrees = angle * 180.0f / M_PI;
    int sector = (int)((degrees + 22.5f) / 45.0f) % 8;
    return (omni_sector_t)sector;
}

static int distance_to_radial_bin(float distance, float max_dist) {
    if (distance <= 0.0f) return 0;
    if (distance >= max_dist) return OMNI_RADIAL_BINS - 1;
    return (int)(distance / max_dist * (OMNI_RADIAL_BINS - 1));
}

/*=============================================================================
 * LIFECYCLE IMPLEMENTATION
 *===========================================================================*/

entorhinal_omni_config_t entorhinal_omni_default_config(void) {
    entorhinal_omni_config_t config = {0};

    config.enable_threat_integration = true;
    config.enable_opportunity_integration = true;
    config.enable_object_tracking = true;
    config.enable_3d_mapping = false;  /* 2D by default */
    config.enable_memory_overlay = true;

    config.angular_resolution = OMNI_ANGULAR_RESOLUTION;
    config.radial_bins = OMNI_RADIAL_BINS;
    config.max_tracking_distance = DEFAULT_MAX_TRACKING_DISTANCE;
    config.min_tracking_distance = DEFAULT_MIN_TRACKING_DISTANCE;

    config.threat_weight = DEFAULT_THREAT_WEIGHT;
    config.opportunity_weight = DEFAULT_OPPORTUNITY_WEIGHT;
    config.salience_weight = DEFAULT_SALIENCE_WEIGHT;
    config.memory_familiarity_weight = DEFAULT_MEMORY_FAMILIARITY_WEIGHT;

    config.spatial_map_update_rate_hz = DEFAULT_SPATIAL_MAP_UPDATE_RATE_HZ;
    config.object_tracking_rate_hz = DEFAULT_OBJECT_TRACKING_RATE_HZ;
    config.memory_overlay_rate_hz = DEFAULT_MEMORY_OVERLAY_RATE_HZ;

    config.threat_detection_threshold = DEFAULT_THREAT_DETECTION_THRESHOLD;
    config.opportunity_detection_threshold = DEFAULT_OPPORTUNITY_DETECTION_THRESHOLD;
    config.object_tracking_threshold = 0.1f;
    config.memory_retrieval_threshold = 0.2f;

    config.attention_decay_rate = DEFAULT_ATTENTION_DECAY_RATE;
    config.novelty_boost = DEFAULT_NOVELTY_BOOST;
    config.threat_attention_boost = DEFAULT_THREAT_ATTENTION_BOOST;

    return config;
}

entorhinal_omni_bridge_state_t* entorhinal_omni_bridge_create(
    const entorhinal_omni_config_t* config)
{
    entorhinal_omni_bridge_state_t* bridge =
        (entorhinal_omni_bridge_state_t*)nimcp_calloc(1,
            sizeof(entorhinal_omni_bridge_state_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = entorhinal_omni_default_config();
    }

    /* Initialize 3D spherical map if enabled */
    if (bridge->config.enable_3d_mapping) {
        size_t map_size = OMNI_ANGULAR_RESOLUTION * OMNI_ELEVATION_BINS * OMNI_RADIAL_BINS;
        bridge->spatial_map.spherical_map = (float*)nimcp_calloc(map_size, sizeof(float));
        bridge->spatial_map.spherical_enabled = (bridge->spatial_map.spherical_map != NULL);
    }

    /* Initialize attention focus */
    bridge->attention_direction = 0.0f;
    bridge->attention_distance = 5.0f;
    bridge->attention_strength = 0.5f;

    return bridge;
}

void entorhinal_omni_bridge_destroy(
    entorhinal_omni_bridge_state_t* bridge)
{
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "entorhinal_omni");

    if (bridge->spatial_map.spherical_map) {
        nimcp_free(bridge->spatial_map.spherical_map);
    }

    nimcp_free(bridge);
}

int entorhinal_omni_bridge_connect(
    entorhinal_omni_bridge_state_t* bridge,
    nimcp_entorhinal_t* entorhinal,
    nimcp_omnidirectional_system_t* omni)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_bridge_connect: bridge is NULL");
        return -1;
    }

    bridge->entorhinal = entorhinal;
    bridge->omni = omni;
    bridge->connected = true;

    return 0;
}

int entorhinal_omni_bridge_disconnect(
    entorhinal_omni_bridge_state_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_bridge_disconnect: bridge is NULL");
        return -1;
    }

    bridge->entorhinal = NULL;
    bridge->omni = NULL;
    bridge->connected = false;

    return 0;
}

int entorhinal_omni_bridge_reset(
    entorhinal_omni_bridge_state_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Clear spatial map */
    memset(&bridge->spatial_map, 0, sizeof(omni_spatial_map_t));
    if (bridge->config.enable_3d_mapping && bridge->spatial_map.spherical_map) {
        size_t map_size = OMNI_ANGULAR_RESOLUTION * OMNI_ELEVATION_BINS * OMNI_RADIAL_BINS;
        memset(bridge->spatial_map.spherical_map, 0, map_size * sizeof(float));
        bridge->spatial_map.spherical_enabled = true;
    }

    /* Clear tracked objects */
    memset(bridge->tracked_objects, 0, sizeof(bridge->tracked_objects));
    bridge->num_tracked_objects = 0;

    /* Clear threats and opportunities */
    memset(bridge->threats, 0, sizeof(bridge->threats));
    bridge->num_threats = 0;
    memset(bridge->opportunities, 0, sizeof(bridge->opportunities));
    bridge->num_opportunities = 0;

    /* Clear memory overlay */
    memset(bridge->memory_familiarity, 0, sizeof(bridge->memory_familiarity));
    memset(bridge->memory_confidence, 0, sizeof(bridge->memory_confidence));

    /* Reset attention */
    bridge->attention_direction = 0.0f;
    bridge->attention_distance = 5.0f;
    bridge->attention_strength = 0.5f;

    /* Clear escape/approach vectors */
    memset(bridge->escape_vector, 0, sizeof(bridge->escape_vector));
    memset(bridge->approach_vector, 0, sizeof(bridge->approach_vector));

    /* Reset statistics */
    bridge->updates_processed = 0;
    bridge->threats_detected = 0;
    bridge->opportunities_detected = 0;
    bridge->objects_tracked_total = 0;
    bridge->mean_spatial_activity = 0.0f;

    return 0;
}

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW IMPLEMENTATION
 *===========================================================================*/

int entorhinal_omni_bridge_update(
    entorhinal_omni_bridge_state_t* bridge,
    float dt)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_bridge_update: bridge is NULL");
        return -1;
    }

    /* Decay attention over time */
    bridge->attention_strength *= expf(-bridge->config.attention_decay_rate * dt);

    /* Compute sector summaries from azimuthal data */
    for (int s = 0; s < OMNI_SECTOR_COUNT; s++) {
        float salience_sum = 0.0f;
        float threat_sum = 0.0f;
        float opportunity_sum = 0.0f;
        float familiarity_sum = 0.0f;
        int count = 0;

        /* Each sector spans 45 degrees */
        int start_angle = s * 45 - 22;
        int end_angle = s * 45 + 23;

        for (int a = start_angle; a < end_angle; a++) {
            int idx = (a + 360) % 360;
            salience_sum += bridge->spatial_map.azimuth_salience[idx];
            familiarity_sum += bridge->memory_familiarity[idx];

            /* Sum polar threat/opportunity over all distances */
            for (int d = 0; d < OMNI_RADIAL_BINS; d++) {
                threat_sum += bridge->spatial_map.polar_threat[idx][d];
                opportunity_sum += bridge->spatial_map.polar_opportunity[idx][d];
            }
            count++;
        }

        if (count > 0) {
            bridge->spatial_map.sector_salience[s] = salience_sum / count;
            bridge->spatial_map.sector_threat[s] = threat_sum / (count * OMNI_RADIAL_BINS);
            bridge->spatial_map.sector_opportunity[s] = opportunity_sum / (count * OMNI_RADIAL_BINS);
            bridge->spatial_map.sector_familiarity[s] = familiarity_sum / count;
        }
    }

    /* Find max threat and opportunity directions */
    float max_threat = 0.0f;
    float max_opportunity = 0.0f;
    int max_threat_idx = 0;
    int max_opp_idx = 0;

    for (int a = 0; a < OMNI_ANGULAR_RESOLUTION; a++) {
        for (int d = 0; d < OMNI_RADIAL_BINS; d++) {
            if (bridge->spatial_map.polar_threat[a][d] > max_threat) {
                max_threat = bridge->spatial_map.polar_threat[a][d];
                max_threat_idx = a;
                bridge->spatial_map.max_threat_distance =
                    (float)d / OMNI_RADIAL_BINS * bridge->config.max_tracking_distance;
            }
            if (bridge->spatial_map.polar_opportunity[a][d] > max_opportunity) {
                max_opportunity = bridge->spatial_map.polar_opportunity[a][d];
                max_opp_idx = a;
                bridge->spatial_map.max_opportunity_distance =
                    (float)d / OMNI_RADIAL_BINS * bridge->config.max_tracking_distance;
            }
        }
    }

    bridge->spatial_map.max_threat_direction = (float)max_threat_idx * M_PI / 180.0f;
    bridge->spatial_map.max_opportunity_direction = (float)max_opp_idx * M_PI / 180.0f;

    /* Compute escape vector (opposite to max threat) */
    if (max_threat > bridge->config.threat_detection_threshold) {
        float escape_angle = bridge->spatial_map.max_threat_direction + M_PI;
        bridge->escape_vector[0] = cosf(escape_angle);
        bridge->escape_vector[1] = sinf(escape_angle);
        bridge->escape_vector[2] = 0.0f;
    }

    /* Compute approach vector (toward max opportunity) */
    if (max_opportunity > bridge->config.opportunity_detection_threshold) {
        float approach_angle = bridge->spatial_map.max_opportunity_direction;
        bridge->approach_vector[0] = cosf(approach_angle);
        bridge->approach_vector[1] = sinf(approach_angle);
        bridge->approach_vector[2] = 0.0f;
    }

    /* Compute mean spatial activity */
    float activity_sum = 0.0f;
    for (int a = 0; a < OMNI_ANGULAR_RESOLUTION; a++) {
        activity_sum += bridge->spatial_map.azimuth_salience[a];
    }
    bridge->spatial_map.mean_surround_activity = activity_sum / OMNI_ANGULAR_RESOLUTION;
    bridge->mean_spatial_activity = bridge->mean_spatial_activity * 0.99f +
        bridge->spatial_map.mean_surround_activity * 0.01f;

    bridge->updates_processed++;

    return 0;
}

int entorhinal_omni_receive_spatial_map(
    entorhinal_omni_bridge_state_t* bridge,
    const omni_spatial_map_t* map)
{
    if (!bridge || !map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_receive_spatial_map: required parameter is NULL (bridge, map)");
        return -1;
    }

    /* Copy azimuthal data */
    memcpy(bridge->spatial_map.azimuth_salience, map->azimuth_salience,
        sizeof(map->azimuth_salience));
    memcpy(bridge->spatial_map.azimuth_distance, map->azimuth_distance,
        sizeof(map->azimuth_distance));
    memcpy(bridge->spatial_map.azimuth_velocity, map->azimuth_velocity,
        sizeof(map->azimuth_velocity));

    /* Copy polar data */
    memcpy(bridge->spatial_map.polar_occupancy, map->polar_occupancy,
        sizeof(map->polar_occupancy));
    memcpy(bridge->spatial_map.polar_threat, map->polar_threat,
        sizeof(map->polar_threat));
    memcpy(bridge->spatial_map.polar_opportunity, map->polar_opportunity,
        sizeof(map->polar_opportunity));

    return 0;
}

int entorhinal_omni_send_grid_representation(
    entorhinal_omni_bridge_state_t* bridge)
{
    if (!bridge || !bridge->entorhinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_send_grid_representation: required parameter is NULL (bridge, bridge->entorhinal)");
        return -1;
    }

    /* Would extract grid cell population vector and send to omni system */
    /* For now, just return success */

    return 0;
}

int entorhinal_omni_receive_threats(
    entorhinal_omni_bridge_state_t* bridge,
    const omni_threat_vector_t* threats,
    uint32_t num_threats)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_receive_threats: bridge is NULL");
        return -1;
    }
    if (num_threats > 0 && !threats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_receive_threats: threats is NULL");
        return -1;
    }

    uint32_t count = num_threats < OMNI_MAX_THREATS ? num_threats : OMNI_MAX_THREATS;
    memcpy(bridge->threats, threats, count * sizeof(omni_threat_vector_t));
    bridge->num_threats = count;
    bridge->threats_detected += count;

    /* Update polar threat map */
    for (uint32_t i = 0; i < count; i++) {
        int azimuth_idx = angle_to_index(threats[i].direction);
        int radial_idx = distance_to_radial_bin(threats[i].distance,
            bridge->config.max_tracking_distance);
        bridge->spatial_map.polar_threat[azimuth_idx][radial_idx] =
            fmaxf(bridge->spatial_map.polar_threat[azimuth_idx][radial_idx],
                threats[i].magnitude);
    }

    return 0;
}

int entorhinal_omni_receive_opportunities(
    entorhinal_omni_bridge_state_t* bridge,
    const omni_opportunity_vector_t* opportunities,
    uint32_t num_opportunities)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_receive_opportunities: bridge is NULL");
        return -1;
    }
    if (num_opportunities > 0 && !opportunities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_receive_opportunities: opportunities is NULL");
        return -1;
    }

    uint32_t count = num_opportunities < OMNI_MAX_OPPORTUNITIES ?
        num_opportunities : OMNI_MAX_OPPORTUNITIES;
    memcpy(bridge->opportunities, opportunities, count * sizeof(omni_opportunity_vector_t));
    bridge->num_opportunities = count;
    bridge->opportunities_detected += count;

    /* Update polar opportunity map */
    for (uint32_t i = 0; i < count; i++) {
        int azimuth_idx = angle_to_index(opportunities[i].direction);
        int radial_idx = distance_to_radial_bin(opportunities[i].distance,
            bridge->config.max_tracking_distance);
        bridge->spatial_map.polar_opportunity[azimuth_idx][radial_idx] =
            fmaxf(bridge->spatial_map.polar_opportunity[azimuth_idx][radial_idx],
                opportunities[i].value);
    }

    return 0;
}

int entorhinal_omni_send_memory_overlay(
    entorhinal_omni_bridge_state_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_send_memory_overlay: bridge is NULL");
        return -1;
    }

    /* Would compute memory familiarity for each direction based on
       grid cell patterns and send to omnidirectional system */

    return 0;
}

/*=============================================================================
 * SPATIAL QUERY API IMPLEMENTATION
 *===========================================================================*/

float entorhinal_omni_get_salience_at_direction(
    const entorhinal_omni_bridge_state_t* bridge,
    float azimuth)
{
    if (!bridge) return 0.0f;

    int idx = angle_to_index(azimuth);
    return bridge->spatial_map.azimuth_salience[idx];
}

float entorhinal_omni_get_threat_at_direction(
    const entorhinal_omni_bridge_state_t* bridge,
    float azimuth)
{
    if (!bridge) return 0.0f;

    int idx = angle_to_index(azimuth);

    /* Sum threat over all distances */
    float total_threat = 0.0f;
    for (int d = 0; d < OMNI_RADIAL_BINS; d++) {
        total_threat = fmaxf(total_threat, bridge->spatial_map.polar_threat[idx][d]);
    }

    return total_threat;
}

float entorhinal_omni_get_opportunity_at_direction(
    const entorhinal_omni_bridge_state_t* bridge,
    float azimuth)
{
    if (!bridge) return 0.0f;

    int idx = angle_to_index(azimuth);

    /* Sum opportunity over all distances */
    float total_opportunity = 0.0f;
    for (int d = 0; d < OMNI_RADIAL_BINS; d++) {
        total_opportunity = fmaxf(total_opportunity,
            bridge->spatial_map.polar_opportunity[idx][d]);
    }

    return total_opportunity;
}

float entorhinal_omni_get_familiarity_at_direction(
    const entorhinal_omni_bridge_state_t* bridge,
    float azimuth)
{
    if (!bridge) return 0.0f;

    int idx = angle_to_index(azimuth);
    return bridge->memory_familiarity[idx];
}

int entorhinal_omni_get_sector_summary(
    const entorhinal_omni_bridge_state_t* bridge,
    omni_sector_t sector,
    float* salience, float* threat, float* opportunity)
{
    if (!bridge || sector >= OMNI_SECTOR_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "entorhinal_omni_get_sector_summary: bridge is NULL");
        return -1;
    }

    if (salience) *salience = bridge->spatial_map.sector_salience[sector];
    if (threat) *threat = bridge->spatial_map.sector_threat[sector];
    if (opportunity) *opportunity = bridge->spatial_map.sector_opportunity[sector];

    return 0;
}

/*=============================================================================
 * OBJECT TRACKING API IMPLEMENTATION
 *===========================================================================*/

const omni_tracked_object_t* entorhinal_omni_get_tracked_object(
    const entorhinal_omni_bridge_state_t* bridge,
    uint32_t object_id)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < bridge->num_tracked_objects; i++) {
        if (bridge->tracked_objects[i].object_id == object_id) {
            return &bridge->tracked_objects[i];
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_get_tracked_object: validation failed");
    return NULL;
}

int entorhinal_omni_get_nearest_threat(
    const entorhinal_omni_bridge_state_t* bridge,
    omni_threat_vector_t* threat_out)
{
    if (!bridge || !threat_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_get_nearest_threat: required parameter is NULL (bridge, threat_out)");
        return -1;
    }
    if (bridge->num_threats == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_omni_get_nearest_threat: bridge->num_threats is zero");
        return -1;
    }

    /* Find nearest threat */
    float min_distance = 1e9f;
    uint32_t nearest_idx = 0;

    for (uint32_t i = 0; i < bridge->num_threats; i++) {
        if (bridge->threats[i].distance < min_distance) {
            min_distance = bridge->threats[i].distance;
            nearest_idx = i;
        }
    }

    *threat_out = bridge->threats[nearest_idx];

    return 0;
}

int entorhinal_omni_get_best_opportunity(
    const entorhinal_omni_bridge_state_t* bridge,
    omni_opportunity_vector_t* opportunity_out)
{
    if (!bridge || !opportunity_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_get_best_opportunity: required parameter is NULL (bridge, opportunity_out)");
        return -1;
    }
    if (bridge->num_opportunities == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_omni_get_best_opportunity: bridge->num_opportunities is zero");
        return -1;
    }

    /* Find best opportunity (highest value / distance ratio) */
    float best_score = -1e9f;
    uint32_t best_idx = 0;

    for (uint32_t i = 0; i < bridge->num_opportunities; i++) {
        float score = bridge->opportunities[i].value *
            bridge->opportunities[i].accessibility /
            (bridge->opportunities[i].distance + 0.1f);
        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    *opportunity_out = bridge->opportunities[best_idx];

    return 0;
}

int entorhinal_omni_get_escape_vector(
    const entorhinal_omni_bridge_state_t* bridge,
    float* vector_out)
{
    if (!bridge || !vector_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_get_escape_vector: required parameter is NULL (bridge, vector_out)");
        return -1;
    }

    vector_out[0] = bridge->escape_vector[0];
    vector_out[1] = bridge->escape_vector[1];
    vector_out[2] = bridge->escape_vector[2];

    return 0;
}

int entorhinal_omni_get_approach_vector(
    const entorhinal_omni_bridge_state_t* bridge,
    float* vector_out)
{
    if (!bridge || !vector_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_get_approach_vector: required parameter is NULL (bridge, vector_out)");
        return -1;
    }

    vector_out[0] = bridge->approach_vector[0];
    vector_out[1] = bridge->approach_vector[1];
    vector_out[2] = bridge->approach_vector[2];

    return 0;
}

/*=============================================================================
 * ATTENTION API IMPLEMENTATION
 *===========================================================================*/

int entorhinal_omni_set_attention_focus(
    entorhinal_omni_bridge_state_t* bridge,
    float azimuth, float distance)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_set_attention_focus: bridge is NULL");
        return -1;
    }

    bridge->attention_direction = normalize_angle(azimuth);
    bridge->attention_distance = distance;
    bridge->attention_strength = 1.0f;

    return 0;
}

int entorhinal_omni_get_attention_focus(
    const entorhinal_omni_bridge_state_t* bridge,
    float* azimuth_out, float* distance_out, float* strength_out)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_get_attention_focus: bridge is NULL");
        return -1;
    }

    if (azimuth_out) *azimuth_out = bridge->attention_direction;
    if (distance_out) *distance_out = bridge->attention_distance;
    if (strength_out) *strength_out = bridge->attention_strength;

    return 0;
}

int entorhinal_omni_compute_attended_representation(
    const entorhinal_omni_bridge_state_t* bridge,
    float* representation_out,
    uint32_t representation_size)
{
    if (!bridge || !representation_out || representation_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_compute_attended_representation: required parameter is NULL (bridge, representation_out)");
        return -1;
    }

    /* Compute attention-weighted representation */
    int center_idx = angle_to_index(bridge->attention_direction);
    float attention_width = 30.0f;  /* degrees */

    for (uint32_t i = 0; i < representation_size && i < OMNI_ANGULAR_RESOLUTION; i++) {
        float angle_diff = fabsf((float)i - (float)center_idx);
        if (angle_diff > 180.0f) angle_diff = 360.0f - angle_diff;

        /* Gaussian attention window */
        float attention_weight = expf(-angle_diff * angle_diff /
            (2.0f * attention_width * attention_width));
        attention_weight = attention_weight * bridge->attention_strength +
            (1.0f - bridge->attention_strength) / OMNI_ANGULAR_RESOLUTION;

        representation_out[i] = bridge->spatial_map.azimuth_salience[i] * attention_weight;
    }

    return 0;
}

/*=============================================================================
 * GRID CELL INTEGRATION API IMPLEMENTATION
 *===========================================================================*/

int entorhinal_omni_modulate_grid_cells(
    entorhinal_omni_bridge_state_t* bridge)
{
    if (!bridge || !bridge->entorhinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_modulate_grid_cells: required parameter is NULL (bridge, bridge->entorhinal)");
        return -1;
    }

    /* Would modulate grid cell activations based on omnidirectional input */
    /* Threat increases attention and grid cell gain in that direction */
    /* Familiarity modulates pattern completion */

    return 0;
}

int entorhinal_omni_get_boundary_signals(
    const entorhinal_omni_bridge_state_t* bridge,
    float* boundary_distances,
    float* boundary_directions,
    uint32_t max_boundaries,
    uint32_t* num_boundaries)
{
    if (!bridge || !boundary_distances || !boundary_directions || !num_boundaries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_get_boundary_signals: required parameter is NULL (bridge, boundary_distances, boundary_directions, num_boundaries)");
        return -1;
    }

    /* Extract boundary signals from spatial map for border cells */
    uint32_t count = 0;

    /* Look for local minima in distance (indicating nearby boundaries) */
    for (int a = 0; a < OMNI_ANGULAR_RESOLUTION && count < max_boundaries; a++) {
        float dist = bridge->spatial_map.azimuth_distance[a];
        if (dist > 0 && dist < bridge->config.max_tracking_distance * 0.5f) {
            /* Check if this is a boundary (distance less than neighbors) */
            int prev = (a - 1 + OMNI_ANGULAR_RESOLUTION) % OMNI_ANGULAR_RESOLUTION;
            int next = (a + 1) % OMNI_ANGULAR_RESOLUTION;

            if (dist <= bridge->spatial_map.azimuth_distance[prev] &&
                dist <= bridge->spatial_map.azimuth_distance[next]) {
                boundary_directions[count] = (float)a * M_PI / 180.0f;
                boundary_distances[count] = dist;
                count++;
            }
        }
    }

    *num_boundaries = count;

    return 0;
}

/*=============================================================================
 * DIAGNOSTICS API IMPLEMENTATION
 *===========================================================================*/

int entorhinal_omni_bridge_get_stats(
    const entorhinal_omni_bridge_state_t* bridge,
    uint64_t* updates_processed,
    uint64_t* threats_detected,
    uint64_t* opportunities_detected)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_bridge_get_stats: bridge is NULL");
        return -1;
    }

    if (updates_processed) *updates_processed = bridge->updates_processed;
    if (threats_detected) *threats_detected = bridge->threats_detected;
    if (opportunities_detected) *opportunities_detected = bridge->opportunities_detected;

    return 0;
}

int entorhinal_omni_bridge_log_diagnostics(
    const entorhinal_omni_bridge_state_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_omni_bridge_log_diagnostics: bridge is NULL");
        return -1;
    }

    /* Would log to nimcp_logger here */

    return 0;
}
