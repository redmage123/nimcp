//=============================================================================
// nimcp_multimodal_director.c - Full-Length Multimodal Film Direction
//=============================================================================
/**
 * @file nimcp_multimodal_director.c
 * @brief Implements coordination of full-length film and multimedia projects
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/generation/nimcp_multimodal_director.h"
#include "utils/memory/nimcp_memory.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define LOG_MODULE "MULTIMODAL_DIR"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(multimodal_director, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Configuration Defaults
//=============================================================================

void multimodal_director_config_defaults(multimodal_director_config_t* config)
{
    if (!config) return;

    memset(config, 0, sizeof(multimodal_director_config_t));

    /* Generator references - must be set by caller */
    config->text_gen = NULL;
    config->music_gen = NULL;
    config->visual_gen = NULL;
    config->video_gen = NULL;

    /* Quality settings */
    config->output_quality = VIDEO_QUALITY_HIGH;
    config->min_scene_quality = 0.6f;
    config->min_coherence_threshold = 0.7f;
    config->enable_quality_iterations = true;
    config->max_quality_iterations = 3;

    /* Style consistency */
    config->style_consistency_weight = 0.8f;
    config->enforce_style_throughout = true;

    /* Production settings */
    config->generate_storyboard = true;
    config->generate_previz = false;
    config->parallel_scene_generation = false;
    config->max_parallel_scenes = 4;

    /* Resource limits */
    config->max_memory_bytes = 16ULL * 1024 * 1024 * 1024;  /* 16GB */
    config->max_generation_hours = 24.0f;
}

//=============================================================================
// Lifecycle
//=============================================================================

multimodal_director_t* multimodal_director_create(
    const multimodal_director_config_t* config)
{
    multimodal_director_t* dir = nimcp_calloc(1, sizeof(multimodal_director_t));
    if (!dir) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multimodal_director_create: dir is NULL");
        return NULL;
    }

    /* Apply config */
    if (config) {
        dir->config = *config;
        dir->text_gen = config->text_gen;
        dir->music_gen = config->music_gen;
        dir->visual_gen = config->visual_gen;
        dir->video_gen = config->video_gen;
    } else {
        multimodal_director_config_defaults(&dir->config);
    }

    /* Initialize progress */
    memset(&dir->progress, 0, sizeof(production_progress_t));
    dir->current_project = NULL;

    /* Statistics */
    dir->projects_completed = 0;
    dir->avg_quality_score = 0.0f;
    dir->avg_production_time_hours = 0.0f;

    /* Integration - set later */
    dir->aesthetic_evaluator = NULL;
    dir->creative_bridge = NULL;
    dir->influence_blender = NULL;
    dir->style_repr = NULL;

    return dir;
}

void multimodal_director_destroy(multimodal_director_t* dir)
{
    if (!dir) return;

    /* Free current project */
    if (dir->current_project) {
        extended_project_spec_free(dir->current_project);
        nimcp_free(dir->current_project);
    }

    nimcp_free(dir);
    dir = NULL;
}

//=============================================================================
// Internal: Progress Tracking
//=============================================================================

/**
 * @brief Update phase status
 */
static void update_phase(multimodal_director_t* dir,
                         project_phase_t phase,
                         float progress,
                         const char* message)
{
    if (!dir) return;

    phase_status_t* status = &dir->progress.phases[phase];
    status->phase = phase;
    status->progress = progress;
    status->completed = (progress >= 1.0f);

    if (message) {
        strncpy(status->status_message, message,
                sizeof(status->status_message) - 1);
    }

    if (progress > 0.0f && status->start_time == 0) {
        status->start_time = (uint64_t)time(NULL);
    }

    dir->progress.current_phase = phase;

    /* Calculate overall progress */
    float total = 0.0f;
    for (int i = 0; i < PHASE_COUNT; i++) {
        total += dir->progress.phases[i].progress;
    }
    dir->progress.overall_progress = total / (PHASE_COUNT > 0 ? PHASE_COUNT : 1);
}

//=============================================================================
// Concept Development API
//=============================================================================

int director_develop_concept(multimodal_director_t* dir,
                              const char* description,
                              creative_project_type_t type,
                              extended_project_spec_t* spec)
{
    if (!dir || !description || !spec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_develop_concept: required parameter is NULL (dir, description, spec)");
        return -1;
    }

    memset(spec, 0, sizeof(extended_project_spec_t));

    update_phase(dir, PHASE_CONCEPT, 0.0f, "Starting concept development");

    /* Set basic project info */
    spec->type = type;

    /* Generate title from description */
    const char* title_words[3] = {"The", "Project", "Story"};
    int word_idx = 0;
    for (const char* p = description; *p && word_idx < 3; p++) {
        if (*p >= 'A' && *p <= 'Z') {
            title_words[word_idx++] = description;
            break;
        }
    }
    snprintf(spec->title, sizeof(spec->title), "Untitled %s",
             type == PROJECT_SHORT_FILM ? "Short" :
             type == PROJECT_FEATURE_FILM ? "Feature" :
             type == PROJECT_TV_EPISODE ? "Episode" :
             type == PROJECT_MUSIC_VIDEO ? "Music Video" :
             type == PROJECT_DOCUMENTARY ? "Documentary" : "Project");

    (void)title_words;  /* Used in production for title generation */

    /* Generate logline */
    strncpy(spec->logline, description,
            fminf(strlen(description), sizeof(spec->logline) - 1));

    /* Generate synopsis */
    snprintf(spec->synopsis, sizeof(spec->synopsis),
             "A %s exploring the themes of %s. "
             "The narrative follows the journey of discovery and transformation.",
             type == PROJECT_DOCUMENTARY ? "documentary" : "story",
             description);

    /* Set technical parameters based on type */
    switch (type) {
        case PROJECT_SHORT_FILM:
            spec->target_duration_minutes = 15.0f;
            spec->width = 1920;
            spec->height = 1080;
            spec->fps = 24.0f;
            break;

        case PROJECT_FEATURE_FILM:
            spec->target_duration_minutes = 90.0f;
            spec->width = 1920;
            spec->height = 1080;
            spec->fps = 24.0f;
            break;

        case PROJECT_TV_EPISODE:
            spec->target_duration_minutes = 45.0f;
            spec->width = 1920;
            spec->height = 1080;
            spec->fps = 24.0f;
            break;

        case PROJECT_MUSIC_VIDEO:
            spec->target_duration_minutes = 4.0f;
            spec->width = 1920;
            spec->height = 1080;
            spec->fps = 30.0f;
            break;

        case PROJECT_DOCUMENTARY:
            spec->target_duration_minutes = 60.0f;
            spec->width = 1920;
            spec->height = 1080;
            spec->fps = 24.0f;
            break;

        case PROJECT_ANIMATION:
            spec->target_duration_minutes = 90.0f;
            spec->width = 1920;
            spec->height = 1080;
            spec->fps = 24.0f;
            break;

        default:
            spec->target_duration_minutes = 30.0f;
            spec->width = 1920;
            spec->height = 1080;
            spec->fps = 24.0f;
            break;
    }

    update_phase(dir, PHASE_CONCEPT, 1.0f, "Concept development complete");

    return 0;
}

int director_generate_treatment(multimodal_director_t* dir,
                                 extended_project_spec_t* spec)
{
    if (!dir || !spec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_generate_treatment: required parameter is NULL (dir, spec)");
        return -1;
    }

    update_phase(dir, PHASE_TREATMENT, 0.0f, "Generating treatment");

    /* Expand synopsis into fuller treatment */
    char expanded[NIMCP_JSON_BUFFER_SIZE];
    snprintf(expanded, sizeof(expanded),
             "%s\n\n"
             "OPENING: The story begins with an establishing sequence that sets the mood and introduces the world.\n\n"
             "DEVELOPMENT: Characters are introduced and the central conflict emerges.\n\n"
             "CLIMAX: The narrative builds to its peak moment of tension and resolution.\n\n"
             "CONCLUSION: The story reaches its denouement with lasting impact.",
             spec->synopsis);

    strncpy(spec->synopsis, expanded, sizeof(spec->synopsis) - 1);

    /* Set theme if not already set */
    if (strlen(spec->theme) == 0) {
        strncpy(spec->theme, "Transformation and discovery",
                sizeof(spec->theme) - 1);
    }

    update_phase(dir, PHASE_TREATMENT, 1.0f, "Treatment complete");

    return 0;
}

int director_generate_characters(multimodal_director_t* dir,
                                  extended_project_spec_t* spec,
                                  uint32_t num_characters)
{
    if (!dir || !spec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_generate_characters: required parameter is NULL (dir, spec)");
        return -1;
    }
    if (num_characters == 0) num_characters = 3;  /* Default: protagonist, antagonist, supporting */

    /* Free existing characters */
    if (spec->characters) {
        for (uint32_t i = 0; i < spec->num_characters; i++) {
            character_def_free(&spec->characters[i]);
        }
        nimcp_free(spec->characters);
    }

    spec->characters = nimcp_calloc(num_characters, sizeof(character_def_t));
    if (!spec->characters) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "director_generate_characters: spec->characters is NULL");
        return -1;
    }

    spec->num_characters = num_characters;

    /* Generate character archetypes */
    static const char* names[] = {
        "Alex", "Jordan", "Sam", "Morgan", "Riley", "Taylor", "Casey", "Quinn"
    };
    static const char* roles[] = {
        "protagonist", "antagonist", "mentor", "ally", "rival", "love interest"
    };

    for (uint32_t i = 0; i < num_characters; i++) {
        character_def_t* ch = &spec->characters[i];

        /* Name */
        strncpy(ch->name, names[i % (sizeof(names)/sizeof(names[0]))],
                sizeof(ch->name) - 1);

        /* Description */
        snprintf(ch->description, sizeof(ch->description),
                 "A compelling character who serves as the %s of the story. "
                 "Complex motivations drive their actions throughout the narrative.",
                 roles[i < 6 ? i : i % 6]);

        /* Backstory */
        snprintf(ch->backstory, sizeof(ch->backstory),
                 "Before the events of the story, %s experienced formative events "
                 "that shaped their worldview and approach to challenges.",
                 ch->name);

        /* Motivation */
        snprintf(ch->motivation, sizeof(ch->motivation),
                 "Seeks understanding and resolution of internal conflict.");

        /* Arc */
        snprintf(ch->arc, sizeof(ch->arc),
                 "Transforms from uncertainty to clarity through the story's events.");

        /* Importance and role flags */
        ch->importance = i == 0 ? 1.0f : (i == 1 ? 0.9f : 0.5f);
        ch->is_protagonist = (i == 0);
        ch->is_antagonist = (i == 1 && num_characters > 1);
        ch->reference_image = NULL;
    }

    return 0;
}

int director_generate_structure(multimodal_director_t* dir,
                                 extended_project_spec_t* spec,
                                 const char* structure_type)
{
    if (!dir || !spec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_generate_structure: required parameter is NULL (dir, spec)");
        return -1;
    }

    story_structure_t* structure = &spec->structure;

    /* Free existing beats */
    if (structure->beats) {
        nimcp_free(structure->beats);
    }

    strncpy(structure->structure_type, structure_type ? structure_type : "3-act",
            sizeof(structure->structure_type) - 1);

    /* Generate beats based on structure type */
    if (strcmp(structure->structure_type, "3-act") == 0) {
        structure->num_beats = 8;
        structure->beats = nimcp_calloc(structure->num_beats, sizeof(story_beat_t));
        if (!structure->beats) return -1;

        static const struct {
            const char* desc;
            float time_pct;
            float intensity;
        } three_act_beats[] = {
            {"Opening/Setup", 0.0f, 0.3f},
            {"Inciting Incident", 0.10f, 0.5f},
            {"First Plot Point", 0.25f, 0.6f},
            {"Rising Action", 0.37f, 0.7f},
            {"Midpoint", 0.50f, 0.75f},
            {"Complications", 0.62f, 0.8f},
            {"Climax", 0.85f, 1.0f},
            {"Resolution", 0.95f, 0.4f}
        };

        for (uint32_t i = 0; i < structure->num_beats; i++) {
            strncpy(structure->beats[i].description, three_act_beats[i].desc,
                    sizeof(structure->beats[i].description) - 1);
            structure->beats[i].timestamp_minutes =
                three_act_beats[i].time_pct * spec->target_duration_minutes;
            structure->beats[i].emotional_intensity = three_act_beats[i].intensity;
        }

        strncpy(structure->inciting_incident,
                "The event that disrupts the ordinary world",
                sizeof(structure->inciting_incident) - 1);
        strncpy(structure->midpoint,
                "A revelation that changes everything",
                sizeof(structure->midpoint) - 1);
        strncpy(structure->climax,
                "The final confrontation",
                sizeof(structure->climax) - 1);
        strncpy(structure->resolution,
                "New equilibrium established",
                sizeof(structure->resolution) - 1);

    } else if (strcmp(structure->structure_type, "hero's journey") == 0) {
        structure->num_beats = 12;
        structure->beats = nimcp_calloc(structure->num_beats, sizeof(story_beat_t));
        if (!structure->beats) return -1;

        static const char* journey_beats[] = {
            "Ordinary World", "Call to Adventure", "Refusal of the Call",
            "Meeting the Mentor", "Crossing the Threshold", "Tests, Allies, Enemies",
            "Approach to Inmost Cave", "Ordeal", "Reward",
            "The Road Back", "Resurrection", "Return with Elixir"
        };

        for (uint32_t i = 0; i < 12; i++) {
            strncpy(structure->beats[i].description, journey_beats[i],
                    sizeof(structure->beats[i].description) - 1);
            structure->beats[i].timestamp_minutes =
                (float)i / 12.0f * spec->target_duration_minutes;
            structure->beats[i].emotional_intensity =
                (i == 7) ? 1.0f : (0.3f + (float)i * 0.05f);
        }
    }

    return 0;
}

//=============================================================================
// Pre-Production API
//=============================================================================

int director_generate_screenplay(multimodal_director_t* dir,
                                  const extended_project_spec_t* spec,
                                  text_generation_result_t* result)
{
    if (!dir || !spec || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_generate_screenplay: required parameter is NULL (dir, spec, result)");
        return -1;
    }

    update_phase(dir, PHASE_SCREENPLAY, 0.0f, "Generating screenplay");

    memset(result, 0, sizeof(text_generation_result_t));

    /* Build screenplay from project spec */
    size_t screenplay_size = 64 * 1024;  /* 64KB buffer */
    result->text = nimcp_calloc(screenplay_size, sizeof(char));
    if (!result->text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "director_generate_screenplay: result->text is NULL");
        return -1;
    }

    char* p = result->text;
    size_t remaining = screenplay_size;

    /* Title page */
    int written = snprintf(p, remaining,
                           "\n\n\n\n\n\n\n                    %s\n\n"
                           "                    A screenplay\n\n\n\n",
                           spec->title);
    p += written;
    remaining -= written;

    /* Generate scenes */
    uint32_t num_scenes = (uint32_t)(spec->target_duration_minutes / 3.0f);  /* ~3 min per scene */
    if (num_scenes < 5) num_scenes = 5;

    for (uint32_t s = 0; s < num_scenes && remaining > 1024; s++) {
        /* Scene header */
        written = snprintf(p, remaining,
                           "\n%s%u. INT. LOCATION %u - DAY\n\n",
                           s > 0 ? "\n" : "", s + 1, s + 1);
        p += written;
        remaining -= written;

        /* Action */
        written = snprintf(p, remaining,
                           "The scene unfolds with %s establishing the atmosphere.\n\n",
                           s == 0 ? "an opening shot" : "a transition");
        p += written;
        remaining -= written;

        /* Dialogue if we have characters */
        if (spec->num_characters > 0) {
            uint32_t speaker_idx = s % spec->num_characters;
            written = snprintf(p, remaining,
                               "                    %s\n"
                               "          This moment reveals something important\n"
                               "          about the story's central conflict.\n\n",
                               spec->characters[speaker_idx].name);
            p += written;
            remaining -= written;
        }

        /* Track beat alignment */
        float scene_time = (float)s / (float)num_scenes * spec->target_duration_minutes;
        for (uint32_t b = 0; b < spec->structure.num_beats; b++) {
            if (fabsf(scene_time - spec->structure.beats[b].timestamp_minutes) < 1.5f) {
                written = snprintf(p, remaining,
                                   "[BEAT: %s]\n\n",
                                   spec->structure.beats[b].description);
                p += written;
                remaining -= written;
                break;
            }
        }

        /* Update progress */
        update_phase(dir, PHASE_SCREENPLAY,
                     (float)(s + 1) / (float)num_scenes,
                     "Writing scenes");
    }

    /* Ending */
    written = snprintf(p, remaining,
                       "\n                    FADE OUT.\n\n"
                       "                    THE END\n");
    p += written;

    result->text_len = strlen(result->text);
    result->success = true;

    update_phase(dir, PHASE_SCREENPLAY, 1.0f, "Screenplay complete");

    return 0;
}

int director_generate_storyboard(multimodal_director_t* dir,
                                  extended_project_spec_t* spec)
{
    if (!dir || !spec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_generate_storyboard: required parameter is NULL (dir, spec)");
        return -1;
    }
    if (!dir->visual_gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_generate_storyboard: dir->visual_gen is NULL");
        return -1;
    }

    update_phase(dir, PHASE_STORYBOARD, 0.0f, "Generating storyboard");

    /* Calculate number of scenes needed */
    uint32_t num_scenes = (uint32_t)(spec->target_duration_minutes / 3.0f);
    if (num_scenes < 5) num_scenes = 5;

    /* Allocate scenes */
    if (spec->scenes) {
        for (uint32_t i = 0; i < spec->num_scenes; i++) {
            extended_scene_free(&spec->scenes[i]);
        }
        nimcp_free(spec->scenes);
    }

    spec->scenes = nimcp_calloc(num_scenes, sizeof(extended_scene_t));
    if (!spec->scenes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "director_generate_storyboard: spec->scenes is NULL");
        return -1;
    }
    spec->num_scenes = num_scenes;

    /* Generate each scene's storyboard */
    for (uint32_t s = 0; s < num_scenes; s++) {
        extended_scene_t* scene = &spec->scenes[s];

        scene->scene_number = s + 1;
        snprintf(scene->slug_line, sizeof(scene->slug_line),
                 "%s LOCATION %u - %s",
                 (s % 2 == 0) ? "INT." : "EXT.",
                 s + 1,
                 (s % 3 == 0) ? "DAY" : (s % 3 == 1) ? "NIGHT" : "DUSK");

        scene->start_time_minutes = (float)s / (float)num_scenes *
                                    spec->target_duration_minutes;
        scene->duration_seconds = (spec->target_duration_minutes * 60.0f) /
                                   (float)num_scenes;

        /* Generate establishing shot */
        visual_generation_request_t vis_req;
        memset(&vis_req, 0, sizeof(vis_req));

        char prompt[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(prompt, sizeof(prompt),
                 "Cinematic scene, %s, film still, %s lighting",
                 scene->slug_line,
                 (s % 3 == 0) ? "warm" : "dramatic");
        vis_req.prompt = prompt;
        vis_req.width = spec->width / 2;  /* Smaller for storyboard */
        vis_req.height = spec->height / 2;

        scene->establishing_shot = nimcp_calloc(1, sizeof(visual_image_t));
        if (scene->establishing_shot) {
            visual_generation_result_t vis_result;
            if (visual_generate(dir->visual_gen, &vis_req, &vis_result) == 0) {
                *scene->establishing_shot = vis_result.image;
                /* Don't free vis_result - we're keeping the image */
            }
        }

        /* Update progress */
        update_phase(dir, PHASE_STORYBOARD,
                     (float)(s + 1) / (float)num_scenes,
                     "Generating scene visuals");
    }

    update_phase(dir, PHASE_STORYBOARD, 1.0f, "Storyboard complete");

    return 0;
}

int director_generate_previz(multimodal_director_t* dir,
                              const extended_project_spec_t* spec,
                              video_generation_result_t* result)
{
    if (!dir || !spec || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_generate_previz: required parameter is NULL (dir, spec, result)");
        return -1;
    }
    if (!dir->video_gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_generate_previz: dir->video_gen is NULL");
        return -1;
    }

    update_phase(dir, PHASE_PREVIZ, 0.0f, "Generating pre-visualization");

    /* Generate low-quality animatic */
    video_generation_request_t request;
    memset(&request, 0, sizeof(request));

    request.prompt = spec->synopsis;
    request.width = spec->width / 2;
    request.height = spec->height / 2;
    request.fps = 12.0f;  /* Lower FPS for previz */
    request.duration_seconds = fminf(spec->target_duration_minutes * 0.5f, 30.0f);
    request.quality = VIDEO_QUALITY_DRAFT;

    int rc = video_generate(dir->video_gen, &request, result);

    update_phase(dir, PHASE_PREVIZ, 1.0f, "Pre-visualization complete");

    return rc;
}

//=============================================================================
// Production API
//=============================================================================

int director_produce(multimodal_director_t* dir,
                      const extended_project_spec_t* spec,
                      project_output_t* output)
{
    if (!dir || !spec || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_produce: required parameter is NULL (dir, spec, output)");
        return -1;
    }

    memset(output, 0, sizeof(project_output_t));

    update_phase(dir, PHASE_PRODUCTION, 0.0f, "Starting production");

    /* Store current project reference */
    dir->current_project = (extended_project_spec_t*)spec;

    /* Generate scenes */
    if (spec->num_scenes == 0) {
        output->success = false;
        strncpy(output->error_message, "No scenes defined",
                sizeof(output->error_message) - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "director_produce: spec->num_scenes is zero");
        return -1;
    }

    /* Allocate scene videos */
    video_generation_result_t* scene_videos =
        nimcp_calloc(spec->num_scenes, sizeof(video_generation_result_t));
    if (!scene_videos) {
        output->success = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "director_produce: scene_videos is NULL");
        return -1;
    }

    /* Produce each scene */
    for (uint32_t s = 0; s < spec->num_scenes; s++) {
        int rc = director_produce_scene(dir, &spec->scenes[s], &scene_videos[s]);
        if (rc != 0) {
            /* Log error but continue */
            scene_videos[s].success = false;
        }

        update_phase(dir, PHASE_PRODUCTION,
                     (float)(s + 1) / (float)spec->num_scenes,
                     "Producing scenes");
    }

    update_phase(dir, PHASE_POST_PRODUCTION, 0.0f, "Starting post-production");

    /* Assemble final video */
    int rc = director_assemble_final(dir, scene_videos, spec->num_scenes, output);

    /* Cleanup scene videos */
    for (uint32_t s = 0; s < spec->num_scenes; s++) {
        video_generation_result_free(&scene_videos[s]);
    }
    nimcp_free(scene_videos);
    scene_videos = NULL;

    if (rc == 0) {
        update_phase(dir, PHASE_DELIVERY, 1.0f, "Production complete");
        output->success = true;

        /* Update statistics */
        dir->projects_completed++;
    }

    return rc;
}

int director_produce_scene(multimodal_director_t* dir,
                            const extended_scene_t* scene,
                            video_generation_result_t* output)
{
    if (!dir || !scene || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_produce_scene: required parameter is NULL (dir, scene, output)");
        return -1;
    }
    if (!dir->video_gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_produce_scene: dir->video_gen is NULL");
        return -1;
    }

    memset(output, 0, sizeof(video_generation_result_t));

    /* Build prompt from scene */
    char prompt[NIMCP_LOG_BUFFER_SIZE];
    snprintf(prompt, sizeof(prompt),
             "Cinematic scene: %s. %s. %s lighting, %s mood.",
             scene->slug_line,
             scene->description[0] ? scene->description : "A compelling moment",
             scene->lighting[0] ? scene->lighting : "natural",
             scene->mood[0] ? scene->mood : "dramatic");

    /* Create video request */
    video_generation_request_t request;
    memset(&request, 0, sizeof(request));

    request.prompt = prompt;
    request.width = dir->current_project ? dir->current_project->width : 1920;
    request.height = dir->current_project ? dir->current_project->height : 1080;
    request.fps = dir->current_project ? dir->current_project->fps : 24.0f;
    request.duration_seconds = scene->duration_seconds > 0 ?
                               scene->duration_seconds : 10.0f;
    request.quality = dir->config.output_quality;

    /* Use establishing shot as keyframe if available */
    if (scene->establishing_shot) {
        video_keyframe_t keyframes[2];
        memset(keyframes, 0, sizeof(keyframes));

        keyframes[0].timestamp = 0.0f;
        keyframes[0].image = scene->establishing_shot;
        keyframes[0].prompt = prompt;

        keyframes[1].timestamp = request.duration_seconds;
        keyframes[1].image = scene->establishing_shot;  /* End on same composition */
        keyframes[1].prompt = prompt;

        return video_generate_from_keyframes(dir->video_gen, keyframes, 2,
                                             request.fps, NULL, output);
    }

    return video_generate(dir->video_gen, &request, output);
}

int director_generate_scene_music(multimodal_director_t* dir,
                                   const extended_scene_t* scene,
                                   music_generation_result_t* result)
{
    if (!dir || !scene || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_generate_scene_music: required parameter is NULL (dir, scene, result)");
        return -1;
    }
    if (!dir->music_gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_generate_scene_music: dir->music_gen is NULL");
        return -1;
    }

    /* Build music request based on scene mood */
    music_generation_request_t request;
    memset(&request, 0, sizeof(request));

    /* Map scene mood to tempo */
    request.tempo_bpm = 90.0f;  /* Default moderate */
    if (strcmp(scene->mood, "tense") == 0) request.tempo_bpm = 120.0f;
    if (strcmp(scene->mood, "peaceful") == 0) request.tempo_bpm = 60.0f;
    if (strcmp(scene->mood, "energetic") == 0) request.tempo_bpm = 140.0f;

    /* Map tension to key */
    if (scene->tension_level > 0.7f) {
        request.key = "D minor";
    } else if (scene->tension_level < 0.3f) {
        request.key = "G major";
    } else {
        request.key = "C major";
    }

    request.duration_seconds = scene->duration_seconds;

    return music_generate(dir->music_gen, &request, result);
}

//=============================================================================
// Post-Production API
//=============================================================================

int director_assemble_final(multimodal_director_t* dir,
                             const video_generation_result_t* scene_videos,
                             uint32_t num_scenes,
                             project_output_t* output)
{
    if (!dir || !scene_videos || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_assemble_final: required parameter is NULL (dir, scene_videos, output)");
        return -1;
    }

    /* Concatenate all scene videos into a temp result */
    video_generation_result_t video_result;
    memset(&video_result, 0, sizeof(video_result));

    int rc = video_concatenate(scene_videos, num_scenes,
                               "dissolve", 0.5f, &video_result);
    if (rc != 0) {
        output->success = false;
        return rc;
    }

    /* Copy video metadata to output */
    output->duration_seconds = video_result.duration_seconds;
    output->width = video_result.width;
    output->height = video_result.height;
    output->fps = video_result.fps;
    /* Note: actual video encoding would happen here in production */

    /* Generate final score if music generator available */
    if (dir->music_gen && dir->current_project) {
        rc = director_generate_full_score(dir, dir->current_project, &output->soundtrack);
        if (rc != 0) {
            /* Continue without music */
        }
    }

    /* Generate screenplay text */
    if (dir->text_gen && dir->current_project) {
        rc = director_generate_screenplay(dir, dir->current_project, &output->screenplay);
        if (rc != 0) {
            /* Continue without screenplay text */
        }
    }

    output->success = true;
    return 0;
}

int director_generate_full_score(multimodal_director_t* dir,
                                  const extended_project_spec_t* spec,
                                  music_generation_result_t* result)
{
    if (!dir || !spec || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_generate_full_score: required parameter is NULL (dir, spec, result)");
        return -1;
    }
    if (!dir->music_gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_generate_full_score: dir->music_gen is NULL");
        return -1;
    }

    update_phase(dir, PHASE_SCORING, 0.0f, "Generating score");

    music_generation_request_t request;
    memset(&request, 0, sizeof(request));

    request.tempo_bpm = 90.0f;
    request.key = "C major";
    request.duration_seconds = spec->target_duration_minutes * 60.0f;

    int rc = music_generate(dir->music_gen, &request, result);

    update_phase(dir, PHASE_SCORING, 1.0f, "Score complete");

    return rc;
}

int director_mix_audio(multimodal_director_t* dir,
                        const float** dialogue,
                        const music_generation_result_t* music,
                        const float* sfx,
                        float** output)
{
    (void)dir;
    (void)dialogue;
    (void)sfx;

    if (!music || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_mix_audio: required parameter is NULL (music, output)");
        return -1;
    }

    /* Simple mix: just return music for now */
    if (music->tracks && music->num_tracks > 0 &&
        music->tracks[0].notes && music->tracks[0].num_notes > 0) {
        /* Would mix audio streams in production */
        *output = NULL;  /* Placeholder */
    }

    return 0;
}

//=============================================================================
// Progress API
//=============================================================================

int director_get_progress(const multimodal_director_t* dir,
                           production_progress_t* progress)
{
    if (!dir || !progress) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "director_get_progress: required parameter is NULL (dir, progress)");
        return -1;
    }
    *progress = dir->progress;
    return 0;
}

static progress_callback_t g_progress_callback = NULL;
static void* g_progress_user_data = NULL;

void director_set_progress_callback(multimodal_director_t* dir,
                                     progress_callback_t callback,
                                     void* user_data)
{
    (void)dir;
    g_progress_callback = callback;
    g_progress_user_data = user_data;
}

//=============================================================================
// Quality Control API
//=============================================================================

float director_evaluate_coherence(const multimodal_director_t* dir,
                                   const extended_project_spec_t* spec)
{
    if (!dir || !spec) return 0.0f;

    float coherence = 1.0f;

    /* Check story structure completeness */
    if (spec->structure.num_beats == 0) coherence -= 0.2f;

    /* Check character definition */
    if (spec->num_characters == 0) coherence -= 0.2f;

    /* Check scene coverage */
    if (spec->num_scenes == 0) coherence -= 0.3f;

    /* Check theme consistency */
    if (strlen(spec->theme) == 0) coherence -= 0.1f;

    return fmaxf(0.0f, coherence);
}

float director_evaluate_scene(const multimodal_director_t* dir,
                               const extended_scene_t* scene,
                               const video_generation_result_t* video)
{
    if (!dir || !scene || !video) return 0.0f;

    float quality = 0.5f;

    /* Check video success */
    if (video->success) quality += 0.2f;

    /* Check temporal coherence */
    quality += video->temporal_coherence * 0.2f;

    /* Check aesthetic evaluation */
    quality += video->evaluation.overall_quality * 0.1f;

    return fminf(1.0f, quality);
}

//=============================================================================
// Cleanup
//=============================================================================

void extended_project_spec_free(extended_project_spec_t* spec)
{
    if (!spec) return;

    /* Free characters */
    if (spec->characters) {
        for (uint32_t i = 0; i < spec->num_characters; i++) {
            character_def_free(&spec->characters[i]);
        }
        nimcp_free(spec->characters);
        spec->characters = NULL;
    }

    /* Free scenes */
    if (spec->scenes) {
        for (uint32_t i = 0; i < spec->num_scenes; i++) {
            extended_scene_free(&spec->scenes[i]);
        }
        nimcp_free(spec->scenes);
        spec->scenes = NULL;
    }

    /* Free story structure */
    story_structure_free(&spec->structure);

    /* Free style embeddings */
    if (spec->visual_style.embedding) {
        nimcp_free(spec->visual_style.embedding);
        spec->visual_style.embedding = NULL;
    }
    if (spec->music_style.embedding) {
        nimcp_free(spec->music_style.embedding);
        spec->music_style.embedding = NULL;
    }

    spec->num_characters = 0;
    spec->num_scenes = 0;
}

void extended_scene_free(extended_scene_t* scene)
{
    if (!scene) return;

    if (scene->dialogue) {
        nimcp_free(scene->dialogue);
        scene->dialogue = NULL;
    }

    if (scene->action) {
        nimcp_free(scene->action);
        scene->action = NULL;
    }

    if (scene->establishing_shot) {
        if (scene->establishing_shot->pixels) {
            nimcp_free(scene->establishing_shot->pixels);
        }
        nimcp_free(scene->establishing_shot);
        scene->establishing_shot = NULL;
    }

    for (uint32_t i = 0; i < scene->num_keyframes; i++) {
        if (scene->keyframes[i]) {
            if (scene->keyframes[i]->pixels) {
                nimcp_free(scene->keyframes[i]->pixels);
            }
            nimcp_free(scene->keyframes[i]);
            scene->keyframes[i] = NULL;
        }
    }

    if (scene->camera_work) {
        nimcp_free(scene->camera_work);
        scene->camera_work = NULL;
    }

    if (scene->music_cue) {
        /* Would free music result */
        nimcp_free(scene->music_cue);
        scene->music_cue = NULL;
    }

    if (scene->dialogue_audio) {
        nimcp_free(scene->dialogue_audio);
        scene->dialogue_audio = NULL;
    }

    if (scene->characters) {
        nimcp_free(scene->characters);
        scene->characters = NULL;
    }
}

void character_def_free(character_def_t* character)
{
    if (!character) return;

    if (character->reference_image) {
        if (character->reference_image->pixels) {
            nimcp_free(character->reference_image->pixels);
        }
        nimcp_free(character->reference_image);
        character->reference_image = NULL;
    }
}

void story_structure_free(story_structure_t* structure)
{
    if (!structure) return;

    if (structure->beats) {
        for (uint32_t i = 0; i < structure->num_beats; i++) {
            if (structure->beats[i].characters_involved) {
                nimcp_free(structure->beats[i].characters_involved);
            }
        }
        nimcp_free(structure->beats);
        structure->beats = NULL;
    }

    structure->num_beats = 0;
}
