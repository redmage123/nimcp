//=============================================================================
// nimcp_video_generation.h - Creative Video Generation
//=============================================================================
/**
 * @file nimcp_video_generation.h
 * @brief Generates video content and animations
 *
 * WHAT: Creates video from frames, animations, and temporal sequences
 * WHY:  Enable AI to produce video/cinema content
 * HOW:  Frame synthesis, temporal coherence, video diffusion
 *
 * APPROACHES:
 * - Frame-by-frame: Generate individual frames with coherence
 * - Video diffusion: End-to-end video generation
 * - Keyframe interpolation: Key frames + interpolation
 * - Animation: Character/object animation
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_VIDEO_GENERATION_H
#define NIMCP_VIDEO_GENERATION_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Video Generation Types
//=============================================================================

/**
 * @brief Video generation methods
 */
typedef enum {
    VIDEO_METHOD_FRAME_BY_FRAME = 0, /**< Generate frames individually */
    VIDEO_METHOD_VIDEO_DIFFUSION,   /**< Video diffusion model */
    VIDEO_METHOD_KEYFRAME_INTERP,   /**< Keyframe + interpolation */
    VIDEO_METHOD_ANIMATION,         /**< Character animation */
    VIDEO_METHOD_HYBRID             /**< Combined approach */
} video_method_t;

/**
 * @brief Video quality presets
 */
typedef enum {
    VIDEO_QUALITY_DRAFT = 0,       /**< Fast, lower quality (preview) */
    VIDEO_QUALITY_STANDARD,        /**< Balanced quality/speed */
    VIDEO_QUALITY_HIGH,            /**< High quality */
    VIDEO_QUALITY_CINEMATIC        /**< Highest quality (slow) */
} video_quality_t;

/**
 * @brief Video codec options
 */
typedef enum {
    VIDEO_CODEC_H264 = 0,          /**< H.264/AVC */
    VIDEO_CODEC_H265,              /**< H.265/HEVC */
    VIDEO_CODEC_VP9,               /**< VP9 */
    VIDEO_CODEC_AV1,               /**< AV1 */
    VIDEO_CODEC_PRORES             /**< ProRes (for editing) */
} video_codec_t;

//=============================================================================
// Keyframe Types
//=============================================================================

/**
 * @brief Keyframe specification
 */
typedef struct {
    float timestamp;               /**< Timestamp in seconds */
    visual_image_t* image;         /**< Keyframe image (or NULL to generate) */
    const char* prompt;            /**< Prompt for this keyframe */
    const char* camera;            /**< Camera position/movement */
    float transition_duration;     /**< Transition to next keyframe */
} video_keyframe_t;

/**
 * @brief Camera movement types
 */
typedef enum {
    CAMERA_STATIC = 0,             /**< No movement */
    CAMERA_PAN_LEFT,               /**< Pan left */
    CAMERA_PAN_RIGHT,              /**< Pan right */
    CAMERA_TILT_UP,                /**< Tilt up */
    CAMERA_TILT_DOWN,              /**< Tilt down */
    CAMERA_ZOOM_IN,                /**< Zoom in */
    CAMERA_ZOOM_OUT,               /**< Zoom out */
    CAMERA_DOLLY_IN,               /**< Dolly forward */
    CAMERA_DOLLY_OUT,              /**< Dolly backward */
    CAMERA_TRACKING,               /**< Follow subject */
    CAMERA_CRANE,                  /**< Crane movement */
    CAMERA_ORBIT,                  /**< Orbit around subject */
    CAMERA_HANDHELD                /**< Handheld/shake effect */
} camera_movement_t;

/**
 * @brief Camera specification
 */
typedef struct {
    camera_movement_t movement;    /**< Type of movement */
    float speed;                   /**< Movement speed multiplier */
    float start_x, start_y;        /**< Start position (normalized) */
    float end_x, end_y;            /**< End position (normalized) */
    float zoom_start, zoom_end;    /**< Zoom range (1.0 = no zoom) */
} camera_spec_t;

//=============================================================================
// Video Generation Request
//=============================================================================

/**
 * @brief Video generation request
 */
typedef struct {
    /* Basic parameters */
    const char* prompt;            /**< Main prompt/description */
    const char* negative_prompt;   /**< What to avoid */
    style_embedding_t* style;      /**< Target style */

    /* Dimensions and timing */
    uint32_t width;                /**< Frame width */
    uint32_t height;               /**< Frame height */
    float fps;                     /**< Frames per second */
    float duration_seconds;        /**< Video duration */

    /* Method settings */
    video_method_t method;         /**< Generation method */
    video_quality_t quality;       /**< Quality preset */

    /* Keyframes (for keyframe-based generation) */
    video_keyframe_t* keyframes;   /**< Keyframe array */
    uint32_t num_keyframes;        /**< Number of keyframes */

    /* Camera */
    camera_spec_t* camera;         /**< Camera specification */

    /* Audio */
    const char* music_prompt;      /**< Music/audio prompt (optional) */
    bool generate_audio;           /**< Generate accompanying audio */

    /* Seed */
    uint64_t seed;                 /**< Random seed */

    /* Output settings */
    video_codec_t codec;           /**< Output codec */
    uint32_t bitrate_kbps;         /**< Output bitrate */
} video_generation_request_t;

/**
 * @brief Video frame
 */
typedef struct {
    visual_image_t image;          /**< Frame image */
    float timestamp;               /**< Frame timestamp */
    uint32_t frame_number;         /**< Frame index */
} video_frame_t;

/**
 * @brief Video generation result
 */
typedef struct {
    /* Video data */
    video_frame_t* frames;         /**< Frame array (if not encoded) */
    uint32_t num_frames;           /**< Number of frames */
    uint8_t* encoded_data;         /**< Encoded video data */
    uint64_t encoded_size;         /**< Encoded data size */

    /* Metadata */
    uint32_t width;
    uint32_t height;
    float fps;
    float duration_seconds;
    video_codec_t codec;

    /* Audio (if generated) */
    float* audio_data;             /**< Audio waveform */
    uint64_t audio_samples;        /**< Number of samples */
    uint32_t audio_sample_rate;    /**< Audio sample rate */

    /* Quality */
    aesthetic_evaluation_t evaluation;
    float temporal_coherence;      /**< [0-1] Frame-to-frame consistency */

    /* Performance */
    float generation_time_seconds;

    /* Status */
    bool success;
    char error_message[256];
} video_generation_result_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Video generator configuration
 */
typedef struct {
    /* Model settings */
    char video_model_path[256];    /**< Video generation model */
    char interpolation_model_path[256]; /**< Frame interpolation model */
    bool use_gpu;
    int32_t gpu_device_id;

    /* Default settings */
    uint32_t default_width;
    uint32_t default_height;
    float default_fps;
    video_quality_t default_quality;
    video_method_t default_method;

    /* Temporal coherence */
    float temporal_consistency_weight; /**< Weight for temporal consistency */
    bool enable_frame_interpolation; /**< Interpolate between generated frames */

    /* Resource limits */
    uint64_t max_vram_bytes;
    uint32_t max_frames_in_memory; /**< Max frames to hold in memory */

    /* Output settings */
    video_codec_t default_codec;
    uint32_t default_bitrate_kbps;
} video_generator_config_t;

/**
 * @brief Initialize config with defaults
 */
void video_generator_config_defaults(video_generator_config_t* config);

//=============================================================================
// Generator Structure
//=============================================================================

/**
 * @brief Video generator
 */
struct video_generator {
    video_generator_config_t config;

    /* Models */
    void* video_model;             /**< Video diffusion model */
    void* frame_model;             /**< Frame generation model */
    void* interpolation_model;     /**< Frame interpolation model */
    void* animation_model;         /**< Animation model */

    /* Integration */
    void* visual_generator;        /**< For frame generation */
    void* music_generator;         /**< For audio generation */
    void* aesthetic_evaluator;
    void* creative_bridge;

    /* Current style */
    style_embedding_t* current_style;

    /* Statistics */
    uint64_t videos_generated;
    float avg_quality_score;
    float avg_generation_time_seconds;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create video generator
 *
 * @param config Configuration (NULL for defaults)
 * @return Generator or NULL on error
 */
video_generator_t* video_generator_create(const video_generator_config_t* config);

/**
 * @brief Destroy video generator
 *
 * @param gen Generator to destroy
 */
void video_generator_destroy(video_generator_t* gen);

//=============================================================================
// Generation API
//=============================================================================

/**
 * @brief Generate video from request
 *
 * @param gen Generator
 * @param request Generation request
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int video_generate(video_generator_t* gen,
                   const video_generation_request_t* request,
                   video_generation_result_t* result);

/**
 * @brief Generate video from keyframes
 *
 * @param gen Generator
 * @param keyframes Keyframe array
 * @param num_keyframes Number of keyframes
 * @param fps Target FPS
 * @param style Target style (optional)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int video_generate_from_keyframes(video_generator_t* gen,
                                   const video_keyframe_t* keyframes,
                                   uint32_t num_keyframes,
                                   float fps,
                                   const style_embedding_t* style,
                                   video_generation_result_t* result);

/**
 * @brief Generate video from image (animate)
 *
 * @param gen Generator
 * @param image Static image
 * @param motion_prompt Description of motion
 * @param duration_seconds Duration
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int video_animate_image(video_generator_t* gen,
                        const visual_image_t* image,
                        const char* motion_prompt,
                        float duration_seconds,
                        video_generation_result_t* result);

//=============================================================================
// Frame Operations API
//=============================================================================

/**
 * @brief Interpolate between two frames
 *
 * @param gen Generator
 * @param frame_a First frame
 * @param frame_b Second frame
 * @param num_frames Number of intermediate frames
 * @param frames Output frames (caller allocates num_frames)
 * @return 0 on success, -1 on error
 */
int video_interpolate_frames(video_generator_t* gen,
                              const visual_image_t* frame_a,
                              const visual_image_t* frame_b,
                              uint32_t num_frames,
                              visual_image_t* frames);

/**
 * @brief Apply camera movement to frames
 *
 * @param gen Generator
 * @param frames Input frames
 * @param num_frames Number of frames
 * @param camera Camera specification
 * @param result Output result with camera applied
 * @return 0 on success, -1 on error
 */
int video_apply_camera(video_generator_t* gen,
                        const video_frame_t* frames,
                        uint32_t num_frames,
                        const camera_spec_t* camera,
                        video_generation_result_t* result);

//=============================================================================
// Video Editing API
//=============================================================================

/**
 * @brief Extend video (add more content)
 *
 * @param gen Generator
 * @param existing Existing video
 * @param additional_seconds Seconds to add
 * @param continuation_prompt Prompt for continuation
 * @param result Output extended video
 * @return 0 on success, -1 on error
 */
int video_extend(video_generator_t* gen,
                 const video_generation_result_t* existing,
                 float additional_seconds,
                 const char* continuation_prompt,
                 video_generation_result_t* result);

/**
 * @brief Concatenate videos
 *
 * @param videos Array of videos
 * @param num_videos Number of videos
 * @param transition Transition type ("cut", "fade", "dissolve")
 * @param transition_duration Transition duration in seconds
 * @param result Output concatenated video
 * @return 0 on success, -1 on error
 */
int video_concatenate(const video_generation_result_t* videos,
                      uint32_t num_videos,
                      const char* transition,
                      float transition_duration,
                      video_generation_result_t* result);

//=============================================================================
// Export API
//=============================================================================

/**
 * @brief Export video to file
 *
 * @param result Video result
 * @param path Output path
 * @param format Format ("mp4", "webm", "mov", "gif")
 * @return 0 on success, -1 on error
 */
int video_export(const video_generation_result_t* result,
                 const char* path,
                 const char* format);

/**
 * @brief Export frames as image sequence
 *
 * @param result Video result
 * @param dir Output directory
 * @param format Image format ("png", "jpg")
 * @param name_pattern Name pattern (e.g., "frame_%04d")
 * @return 0 on success, -1 on error
 */
int video_export_frames(const video_generation_result_t* result,
                        const char* dir,
                        const char* format,
                        const char* name_pattern);

//=============================================================================
// Cleanup
//=============================================================================

/**
 * @brief Free video generation result
 *
 * @param result Result to free
 */
void video_generation_result_free(video_generation_result_t* result);

/**
 * @brief Free video keyframe
 *
 * @param keyframe Keyframe to free
 */
void video_keyframe_free(video_keyframe_t* keyframe);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VIDEO_GENERATION_H */
