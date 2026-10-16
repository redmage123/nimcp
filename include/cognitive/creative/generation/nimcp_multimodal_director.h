//=============================================================================
// nimcp_multimodal_director.h - Full-Length Multimodal Film Direction
//=============================================================================
/**
 * @file nimcp_multimodal_director.h
 * @brief Coordinates full-length film and multimodal project creation
 *
 * WHAT: Directs complete films, documentaries, and multimedia projects
 * WHY:  Enable AI to create coherent, full-length creative works
 * HOW:  Orchestrates text, music, visual, and video generators
 *
 * CAPABILITIES:
 * - Concept development: Transform idea into full project plan
 * - Screenplay generation: Write complete scripts
 * - Storyboarding: Generate visual sequences
 * - Score composition: Create accompanying music
 * - Video assembly: Produce final video
 * - Quality assurance: Ensure coherence across modalities
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_MULTIMODAL_DIRECTOR_H
#define NIMCP_MULTIMODAL_DIRECTOR_H

#include "cognitive/creative/nimcp_creative.h"
#include "cognitive/creative/generation/nimcp_text_generation.h"
#include "cognitive/creative/generation/nimcp_music_generation.h"
#include "cognitive/creative/generation/nimcp_visual_generation.h"
#include "cognitive/creative/generation/nimcp_video_generation.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Project Phase Types
//=============================================================================

/**
 * @brief Project development phases
 */
typedef enum {
    PHASE_CONCEPT = 0,             /**< Initial concept development */
    PHASE_TREATMENT,               /**< Treatment/expanded outline */
    PHASE_SCREENPLAY,              /**< Full screenplay */
    PHASE_STORYBOARD,              /**< Visual storyboard */
    PHASE_PREVIZ,                  /**< Pre-visualization */
    PHASE_PRODUCTION,              /**< Main production */
    PHASE_POST_PRODUCTION,         /**< Post-production/editing */
    PHASE_SCORING,                 /**< Music scoring */
    PHASE_FINAL_MIX,               /**< Final audio/video mix */
    PHASE_DELIVERY,                /**< Final delivery */
    PHASE_COUNT
} project_phase_t;

/**
 * @brief Phase status
 */
typedef struct {
    project_phase_t phase;         /**< Phase */
    float progress;                /**< [0-1] Progress within phase */
    bool completed;                /**< Phase complete */
    char status_message[256];      /**< Current status */
    uint64_t start_time;           /**< Phase start time */
    uint64_t estimated_completion; /**< Estimated completion time */
} phase_status_t;

//=============================================================================
// Character and Story Types
//=============================================================================

/**
 * @brief Character definition
 */
typedef struct {
    char name[64];                 /**< Character name */
    char description[512];         /**< Physical/personality description */
    char backstory[1024];          /**< Character backstory */
    char motivation[256];          /**< Character motivation */
    char arc[256];                 /**< Character arc description */
    float importance;              /**< [0-1] Story importance */
    bool is_protagonist;           /**< Is this the protagonist? */
    bool is_antagonist;            /**< Is this the antagonist? */
    visual_image_t* reference_image; /**< Visual reference (optional) */
} character_def_t;

/**
 * @brief Story beat
 */
typedef struct {
    char description[256];         /**< Beat description */
    float timestamp_minutes;       /**< Approximate timing */
    float emotional_intensity;     /**< [0-1] Emotional intensity */
    char* characters_involved;     /**< Characters in beat (comma-sep) */
    char location[128];            /**< Location */
} story_beat_t;

/**
 * @brief Story structure
 */
typedef struct {
    char structure_type[32];       /**< "3-act", "5-act", "hero's journey", etc. */
    story_beat_t* beats;           /**< Story beats */
    uint32_t num_beats;            /**< Number of beats */
    char inciting_incident[256];   /**< Inciting incident */
    char midpoint[256];            /**< Midpoint */
    char climax[256];              /**< Climax */
    char resolution[256];          /**< Resolution */
} story_structure_t;

//=============================================================================
// Extended Scene Types
//=============================================================================

/**
 * @brief Extended scene specification
 */
typedef struct {
    /* Basic info */
    uint32_t scene_number;         /**< Scene number */
    char slug_line[128];           /**< INT./EXT. LOCATION - TIME */
    char description[512];         /**< Full scene description */

    /* Timing */
    float duration_seconds;        /**< Scene duration */
    float start_time_minutes;      /**< Start time in film */

    /* Content */
    char* dialogue;                /**< Scene dialogue (script format) */
    char* action;                  /**< Action descriptions */
    character_def_t** characters;  /**< Characters in scene */
    uint32_t num_characters;       /**< Number of characters */

    /* Visual */
    visual_image_t* establishing_shot; /**< Establishing shot */
    visual_image_t* keyframes[8];  /**< Key visual moments */
    uint32_t num_keyframes;        /**< Number of keyframes */
    camera_spec_t* camera_work;    /**< Camera specifications */
    uint32_t num_camera_specs;     /**< Number of camera specs */

    /* Audio */
    music_generation_result_t* music_cue; /**< Music for scene */
    char sound_design[256];        /**< Sound design notes */
    char* dialogue_audio;          /**< Generated dialogue audio path */

    /* Mood/style */
    char mood[64];                 /**< Scene mood */
    char lighting[64];             /**< Lighting description */
    float tension_level;           /**< [0-1] Tension/stakes */
    float pacing;                  /**< [0-1] Pacing (slow-fast) */
} extended_scene_t;

//=============================================================================
// Project Types
//=============================================================================

/**
 * @brief Extended project specification
 */
typedef struct {
    /* Project info */
    creative_project_type_t type;  /**< Project type */
    char title[128];               /**< Project title */
    char logline[256];             /**< One-line premise */
    char synopsis[2048];           /**< Full synopsis */
    char theme[256];               /**< Central theme */
    char target_audience[64];      /**< Target audience */
    char rating[8];                /**< Target rating */

    /* Story */
    story_structure_t structure;   /**< Story structure */
    character_def_t* characters;   /**< Character definitions */
    uint32_t num_characters;       /**< Number of characters */

    /* Scenes */
    extended_scene_t* scenes;      /**< Scene specifications */
    uint32_t num_scenes;           /**< Number of scenes */

    /* Style */
    style_embedding_t visual_style; /**< Overall visual style */
    style_embedding_t music_style; /**< Overall music style */
    cinematic_style_archetype_t director_style; /**< Directorial style */

    /* Technical */
    uint32_t width;                /**< Output width */
    uint32_t height;               /**< Output height */
    float fps;                     /**< Output FPS */
    float target_duration_minutes; /**< Target duration */
} extended_project_spec_t;

/**
 * @brief Project production progress
 */
typedef struct {
    phase_status_t phases[PHASE_COUNT];
    project_phase_t current_phase;
    float overall_progress;        /**< [0-1] Overall completion */
    uint32_t scenes_completed;     /**< Scenes produced */
    uint32_t scenes_total;         /**< Total scenes */
    float estimated_time_remaining_minutes;
    bool has_errors;
    char current_task[256];
} production_progress_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Multimodal director configuration
 */
typedef struct {
    /* Generator references (must be set) */
    text_generator_t* text_gen;
    music_generator_t* music_gen;
    visual_generator_t* visual_gen;
    video_generator_t* video_gen;

    /* Quality settings */
    video_quality_t output_quality;
    float min_scene_quality;
    float min_coherence_threshold;
    bool enable_quality_iterations;
    uint32_t max_quality_iterations;

    /* Style consistency */
    float style_consistency_weight;
    bool enforce_style_throughout;

    /* Production settings */
    bool generate_storyboard;      /**< Generate storyboard before production */
    bool generate_previz;          /**< Generate pre-visualization */
    bool parallel_scene_generation; /**< Generate scenes in parallel */
    uint32_t max_parallel_scenes;

    /* Resource limits */
    uint64_t max_memory_bytes;
    float max_generation_hours;    /**< Max total generation time */
} multimodal_director_config_t;

/**
 * @brief Initialize config with defaults
 */
void multimodal_director_config_defaults(multimodal_director_config_t* config);

//=============================================================================
// Director Structure
//=============================================================================

/**
 * @brief Multimodal director
 */
struct multimodal_director {
    multimodal_director_config_t config;

    /* Generators */
    text_generator_t* text_gen;
    music_generator_t* music_gen;
    visual_generator_t* visual_gen;
    video_generator_t* video_gen;

    /* Current project */
    extended_project_spec_t* current_project;
    production_progress_t progress;

    /* Integration */
    void* aesthetic_evaluator;
    void* creative_bridge;
    void* influence_blender;
    void* style_repr;

    /* Statistics */
    uint64_t projects_completed;
    float avg_quality_score;
    float avg_production_time_hours;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create multimodal director
 *
 * @param config Configuration
 * @return Director or NULL on error
 */
multimodal_director_t* multimodal_director_create(
    const multimodal_director_config_t* config);

/**
 * @brief Destroy multimodal director
 *
 * @param dir Director to destroy
 */
void multimodal_director_destroy(multimodal_director_t* dir);

//=============================================================================
// Concept Development API
//=============================================================================

/**
 * @brief Develop concept from description
 *
 * @param dir Director
 * @param description Initial concept description
 * @param type Project type
 * @param spec Output project specification
 * @return 0 on success, -1 on error
 */
int director_develop_concept(multimodal_director_t* dir,
                              const char* description,
                              creative_project_type_t type,
                              extended_project_spec_t* spec);

/**
 * @brief Generate treatment (expanded outline)
 *
 * @param dir Director
 * @param spec Project specification (modified in place)
 * @return 0 on success, -1 on error
 */
int director_generate_treatment(multimodal_director_t* dir,
                                 extended_project_spec_t* spec);

/**
 * @brief Generate characters
 *
 * @param dir Director
 * @param spec Project specification
 * @param num_characters Number of characters to generate
 * @return 0 on success, -1 on error
 */
int director_generate_characters(multimodal_director_t* dir,
                                  extended_project_spec_t* spec,
                                  uint32_t num_characters);

/**
 * @brief Generate story structure
 *
 * @param dir Director
 * @param spec Project specification
 * @param structure_type "3-act", "5-act", "hero's journey", etc.
 * @return 0 on success, -1 on error
 */
int director_generate_structure(multimodal_director_t* dir,
                                 extended_project_spec_t* spec,
                                 const char* structure_type);

//=============================================================================
// Pre-Production API
//=============================================================================

/**
 * @brief Generate full screenplay
 *
 * @param dir Director
 * @param spec Project specification
 * @param result Output text result with screenplay
 * @return 0 on success, -1 on error
 */
int director_generate_screenplay(multimodal_director_t* dir,
                                  const extended_project_spec_t* spec,
                                  text_generation_result_t* result);

/**
 * @brief Generate storyboard
 *
 * @param dir Director
 * @param spec Project specification
 * @return 0 on success, -1 on error
 */
int director_generate_storyboard(multimodal_director_t* dir,
                                  extended_project_spec_t* spec);

/**
 * @brief Generate pre-visualization
 *
 * @param dir Director
 * @param spec Project specification
 * @param result Output video result with animatic
 * @return 0 on success, -1 on error
 */
int director_generate_previz(multimodal_director_t* dir,
                              const extended_project_spec_t* spec,
                              video_generation_result_t* result);

//=============================================================================
// Production API
//=============================================================================

/**
 * @brief Produce full project
 *
 * Main production function - generates complete video output.
 *
 * @param dir Director
 * @param spec Project specification
 * @param output Output project
 * @return 0 on success, -1 on error
 */
int director_produce(multimodal_director_t* dir,
                      const extended_project_spec_t* spec,
                      project_output_t* output);

/**
 * @brief Produce single scene
 *
 * @param dir Director
 * @param scene Scene specification
 * @param output Output video result
 * @return 0 on success, -1 on error
 */
int director_produce_scene(multimodal_director_t* dir,
                            const extended_scene_t* scene,
                            video_generation_result_t* output);

/**
 * @brief Generate scene music cue
 *
 * @param dir Director
 * @param scene Scene specification
 * @param result Output music result
 * @return 0 on success, -1 on error
 */
int director_generate_scene_music(multimodal_director_t* dir,
                                   const extended_scene_t* scene,
                                   music_generation_result_t* result);

//=============================================================================
// Post-Production API
//=============================================================================

/**
 * @brief Assemble scenes into final video
 *
 * @param dir Director
 * @param scene_videos Array of scene videos
 * @param num_scenes Number of scenes
 * @param output Final assembled video
 * @return 0 on success, -1 on error
 */
int director_assemble_final(multimodal_director_t* dir,
                             const video_generation_result_t* scene_videos,
                             uint32_t num_scenes,
                             project_output_t* output);

/**
 * @brief Generate full score for project
 *
 * @param dir Director
 * @param spec Project specification
 * @param result Output music result
 * @return 0 on success, -1 on error
 */
int director_generate_full_score(multimodal_director_t* dir,
                                  const extended_project_spec_t* spec,
                                  music_generation_result_t* result);

/**
 * @brief Mix final audio
 *
 * @param dir Director
 * @param dialogue Dialogue audio tracks
 * @param music Music track
 * @param sfx Sound effects
 * @param output Output mixed audio
 * @return 0 on success, -1 on error
 */
int director_mix_audio(multimodal_director_t* dir,
                        const float** dialogue,
                        const music_generation_result_t* music,
                        const float* sfx,
                        float** output);

//=============================================================================
// Progress API
//=============================================================================

/**
 * @brief Get production progress
 *
 * @param dir Director
 * @param progress Output progress
 * @return 0 on success, -1 on error
 */
int director_get_progress(const multimodal_director_t* dir,
                           production_progress_t* progress);

/**
 * @brief Set progress callback
 *
 * @param dir Director
 * @param callback Callback function
 * @param user_data User data for callback
 */
typedef void (*progress_callback_t)(const production_progress_t* progress,
                                     void* user_data);
void director_set_progress_callback(multimodal_director_t* dir,
                                     progress_callback_t callback,
                                     void* user_data);

//=============================================================================
// Quality Control API
//=============================================================================

/**
 * @brief Evaluate project coherence
 *
 * @param dir Director
 * @param spec Project specification
 * @return Coherence score [0-1]
 */
float director_evaluate_coherence(const multimodal_director_t* dir,
                                   const extended_project_spec_t* spec);

/**
 * @brief Evaluate scene quality
 *
 * @param dir Director
 * @param scene Scene
 * @param video Scene video
 * @return Quality score [0-1]
 */
float director_evaluate_scene(const multimodal_director_t* dir,
                               const extended_scene_t* scene,
                               const video_generation_result_t* video);

//=============================================================================
// Cleanup
//=============================================================================

/**
 * @brief Free extended project specification
 *
 * @param spec Specification to free
 */
void extended_project_spec_free(extended_project_spec_t* spec);

/**
 * @brief Free extended scene
 *
 * @param scene Scene to free
 */
void extended_scene_free(extended_scene_t* scene);

/**
 * @brief Free character definition
 *
 * @param character Character to free
 */
void character_def_free(character_def_t* character);

/**
 * @brief Free story structure
 *
 * @param structure Structure to free
 */
void story_structure_free(story_structure_t* structure);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MULTIMODAL_DIRECTOR_H */
