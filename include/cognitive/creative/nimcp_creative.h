//=============================================================================
// nimcp_creative.h - Creative Cortex Main API
//=============================================================================
/**
 * @file nimcp_creative.h
 * @brief Creative Cortex module for art appreciation, inspiration, and generation
 *
 * WHAT: Comprehensive creative system enabling NIMCP to appreciate, learn from,
 *       and generate artistic content across multiple modalities.
 * WHY:  Enable AI systems to engage with and create art at a high level
 * HOW:  Combines aesthetic evaluation, style analysis, and generative models
 *
 * MODALITIES SUPPORTED:
 * - Text: Poetry, literature, screenplays, lyrics
 * - Music: Composition, arrangement, audio generation
 * - Visual: Still images, paintings, digital art
 * - Video: Cinema, animation, music videos
 *
 * ARCHITECTURE:
 * - Appreciation: Aesthetic evaluation, emotion bridging, memory integration
 * - Inspiration: Style archetypes, influence blending, pattern extraction
 * - Generation: Text, music, visual, video generators with multimodal direction
 * - External: ONNX Runtime, Diffusion models, GANs, cloud APIs
 *
 * INTEGRATION:
 * - Temporal Lobe: Auditory/music processing, semantic memory
 * - Emotion System: Aesthetic emotional responses
 * - Hippocampus: Artistic experience storage
 * - Ethics: Copyright checking, harmful content prevention
 * - VAE: Style latent representations
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_CREATIVE_H
#define NIMCP_CREATIVE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct creative_orchestrator creative_orchestrator_t;
typedef struct aesthetic_evaluator aesthetic_evaluator_t;
typedef struct style_representer style_representer_t;
typedef struct influence_blender influence_blender_t;
typedef struct pattern_extractor pattern_extractor_t;
typedef struct text_generator text_generator_t;
typedef struct music_generator music_generator_t;
typedef struct visual_generator visual_generator_t;
typedef struct video_generator video_generator_t;
typedef struct multimodal_director multimodal_director_t;
typedef struct creative_bridge creative_bridge_t;

//=============================================================================
// Art Modality Types
//=============================================================================

/**
 * @brief Supported art modalities for creative processing
 */
typedef enum {
    ART_MODALITY_TEXT_POETRY = 0,      /**< Poetry and verse */
    ART_MODALITY_TEXT_PROSE,           /**< Prose literature */
    ART_MODALITY_TEXT_SCREENPLAY,      /**< Film/TV screenplays */
    ART_MODALITY_TEXT_LYRICS,          /**< Song lyrics */
    ART_MODALITY_TEXT_DIALOGUE,        /**< Dramatic dialogue */

    ART_MODALITY_MUSIC_CLASSICAL = 10, /**< Classical music */
    ART_MODALITY_MUSIC_FILM_SCORE,     /**< Film/TV scores */
    ART_MODALITY_MUSIC_JAZZ,           /**< Jazz music */
    ART_MODALITY_MUSIC_ELECTRONIC,     /**< Electronic music */
    ART_MODALITY_MUSIC_FOLK,           /**< Folk/world music */

    ART_MODALITY_VISUAL_PAINTING = 20, /**< Paintings and fine art */
    ART_MODALITY_VISUAL_DIGITAL,       /**< Digital art */
    ART_MODALITY_VISUAL_PHOTO,         /**< Photography */
    ART_MODALITY_VISUAL_ILLUSTRATION,  /**< Illustrations */
    ART_MODALITY_VISUAL_3D,            /**< 3D renders */

    ART_MODALITY_VIDEO_CINEMA = 30,    /**< Cinematic films */
    ART_MODALITY_VIDEO_ANIMATION,      /**< Animation */
    ART_MODALITY_VIDEO_DOCUMENTARY,    /**< Documentaries */
    ART_MODALITY_VIDEO_MUSIC_VIDEO,    /**< Music videos */
    ART_MODALITY_VIDEO_SHORT,          /**< Short films */

    ART_MODALITY_COUNT
} art_modality_t;

/**
 * @brief Get modality category (text, music, visual, video)
 */
static inline uint8_t art_modality_category(art_modality_t modality) {
    if (modality < 10) return 0;  /* Text */
    if (modality < 20) return 1;  /* Music */
    if (modality < 30) return 2;  /* Visual */
    return 3;                     /* Video */
}

/**
 * @brief Check if modality is a text modality
 */
static inline bool art_modality_is_text(art_modality_t modality) {
    return modality < 10;
}

/**
 * @brief Check if modality is a music modality
 */
static inline bool art_modality_is_music(art_modality_t modality) {
    return modality >= 10 && modality < 20;
}

/**
 * @brief Check if modality is a visual modality
 */
static inline bool art_modality_is_visual(art_modality_t modality) {
    return modality >= 20 && modality < 30;
}

/**
 * @brief Check if modality is a video modality
 */
static inline bool art_modality_is_video(art_modality_t modality) {
    return modality >= 30;
}

//=============================================================================
// Aesthetic Dimensions (Berlyne's Aesthetics Theory)
//=============================================================================

/**
 * @brief Berlyne's aesthetic dimensions for evaluating art
 *
 * Based on D.E. Berlyne's "Aesthetics and Psychobiology" (1971)
 * - Novelty: How new/surprising the work is
 * - Complexity: Structural/compositional complexity
 * - Familiarity: Connection to known works/patterns
 * - Hedonic Tone: Pleasure/displeasure response
 * - Arousal Potential: Level of stimulation induced
 */
typedef struct {
    float novelty;              /**< [0-1] How new/surprising */
    float complexity;           /**< [0-1] Structural complexity */
    float familiarity;          /**< [0-1] Connection to known works */
    float hedonic_tone;         /**< [-1,+1] Pleasure response */
    float arousal_potential;    /**< [0-1] Stimulation level */
} aesthetic_dimensions_t;

/**
 * @brief Emotional response to aesthetic experience
 *
 * Combines Plutchik's basic emotions with compound aesthetic emotions
 */
typedef struct {
    /* Plutchik's basic emotions */
    float joy;                  /**< [0-1] Joy/happiness */
    float trust;                /**< [0-1] Trust/acceptance */
    float fear;                 /**< [0-1] Fear/apprehension */
    float surprise;             /**< [0-1] Surprise/amazement */
    float sadness;              /**< [0-1] Sadness/melancholy */
    float anticipation;         /**< [0-1] Anticipation/interest */
    float anger;                /**< [0-1] Anger (rarely triggered) */
    float disgust;              /**< [0-1] Disgust (rarely triggered) */

    /* Compound aesthetic emotions */
    float awe;                  /**< [0-1] Awe (surprise + fear + joy) */
    float sublime;              /**< [0-1] Sublime (awe + vastness) */
    float contemplation;        /**< [0-1] Deep thought/reflection */
    float nostalgia;            /**< [0-1] Bittersweet remembrance */
    float catharsis;            /**< [0-1] Emotional release */
    float transcendence;        /**< [0-1] Beyond ordinary experience */
} aesthetic_emotional_response_t;

/**
 * @brief Complete aesthetic evaluation result
 */
typedef struct {
    aesthetic_dimensions_t dimensions;       /**< Berlyne dimensions */
    aesthetic_emotional_response_t emotions; /**< Emotional response */
    float overall_quality;                   /**< [0-1] Overall aesthetic quality */
    float technical_skill;                   /**< [0-1] Technical execution */
    float originality;                       /**< [0-1] Originality score */
    float coherence;                         /**< [0-1] Internal consistency */
    float expressiveness;                    /**< [0-1] Emotional expressiveness */
    art_modality_t modality;                 /**< Modality evaluated */
    uint64_t evaluation_time_us;             /**< Evaluation timestamp */
} aesthetic_evaluation_t;

//=============================================================================
// Style Archetypes
//=============================================================================

/**
 * @brief Literary style archetypes based on famous authors
 */
typedef enum {
    STYLE_LIT_HEMINGWAY = 0,   /**< Minimalist, declarative, spare prose */
    STYLE_LIT_TOLSTOY,         /**< Epic, psychological, detailed */
    STYLE_LIT_JOYCE,           /**< Stream of consciousness, experimental */
    STYLE_LIT_POE,             /**< Gothic, atmospheric, suspenseful */
    STYLE_LIT_AUSTEN,          /**< Witty, social, romantic */
    STYLE_LIT_SHAKESPEARE,     /**< Poetic, dramatic, universal */
    STYLE_LIT_BORGES,          /**< Labyrinthine, philosophical, fantastical */
    STYLE_LIT_KAFKA,           /**< Absurdist, bureaucratic, alienating */
    STYLE_LIT_MARQUEZ,         /**< Magical realism, lush, cyclical */
    STYLE_LIT_DOSTOEVSKY,      /**< Psychological depth, moral complexity */
    STYLE_LIT_WOOLF,           /**< Interior monologue, impressionistic */
    STYLE_LIT_FAULKNER,        /**< Southern gothic, temporal shifts */
    STYLE_LIT_COUNT
} literary_style_archetype_t;

/**
 * @brief Musical style archetypes based on famous composers
 */
typedef enum {
    STYLE_MUSIC_BACH = 0,      /**< Baroque counterpoint, mathematical */
    STYLE_MUSIC_BEETHOVEN,     /**< Romantic, heroic, dramatic */
    STYLE_MUSIC_DEBUSSY,       /**< Impressionistic, atmospheric, fluid */
    STYLE_MUSIC_JOHN_WILLIAMS, /**< Cinematic, leitmotif-driven, epic */
    STYLE_MUSIC_MILES_DAVIS,   /**< Cool jazz, modal, innovative */
    STYLE_MUSIC_HANS_ZIMMER,   /**< Hybrid orchestral-electronic, intense */
    STYLE_MUSIC_STRAVINSKY,    /**< Modernist, rhythmic, dissonant */
    STYLE_MUSIC_ENNIO_MORRICONE, /**< Western, eclectic, memorable themes */
    STYLE_MUSIC_SAKAMOTO,      /**< Minimalist, ambient, electronic-classical */
    STYLE_MUSIC_GLASS,         /**< Minimalist, repetitive, hypnotic */
    STYLE_MUSIC_COPLAND,       /**< American, pastoral, folk-influenced */
    STYLE_MUSIC_RAVEL,         /**< Orchestral color, precision, jazz influence */
    STYLE_MUSIC_COUNT
} musical_style_archetype_t;

/**
 * @brief Visual style archetypes based on famous artists
 */
typedef enum {
    STYLE_VIS_VAN_GOGH = 0,    /**< Post-impressionist, swirling, vivid */
    STYLE_VIS_MONET,           /**< Impressionist, light, atmospheric */
    STYLE_VIS_PICASSO,         /**< Cubist, fragmented, multi-perspective */
    STYLE_VIS_DALI,            /**< Surrealist, dreamlike, precise */
    STYLE_VIS_WARHOL,          /**< Pop art, repetition, commercial */
    STYLE_VIS_REMBRANDT,       /**< Baroque, chiaroscuro, intimate */
    STYLE_VIS_KLIMT,           /**< Art nouveau, gold, decorative */
    STYLE_VIS_ESCHER,          /**< Mathematical, impossible, tessellated */
    STYLE_VIS_HOKUSAI,         /**< Ukiyo-e, nature, dynamic lines */
    STYLE_VIS_BASQUIAT,        /**< Neo-expressionist, raw, symbolic */
    STYLE_VIS_CARAVAGGIO,      /**< Dramatic lighting, realistic, intense */
    STYLE_VIS_KANDINSKY,       /**< Abstract, color theory, musical */
    STYLE_VIS_COUNT
} visual_style_archetype_t;

/**
 * @brief Cinematic style archetypes based on famous directors
 */
typedef enum {
    STYLE_CINEMA_KUBRICK = 0,  /**< Precise framing, cold, cerebral */
    STYLE_CINEMA_SPIELBERG,    /**< Wonder, emotion, spectacle */
    STYLE_CINEMA_TARANTINO,    /**< Dialogue-driven, nonlinear, stylized violence */
    STYLE_CINEMA_NOLAN,        /**< Complex narrative, practical effects, cerebral */
    STYLE_CINEMA_TARKOVSKY,    /**< Poetic, long takes, nature imagery */
    STYLE_CINEMA_MIYAZAKI,     /**< Hand-drawn, ecological, fantastical */
    STYLE_CINEMA_HITCHCOCK,    /**< Suspense, psychological, voyeuristic */
    STYLE_CINEMA_WELLES,       /**< Deep focus, innovative angles, ambitious */
    STYLE_CINEMA_KUROSAWA,     /**< Epic, weather, humanist */
    STYLE_CINEMA_FINCHER,      /**< Dark, meticulous, thriller */
    STYLE_CINEMA_VILLENEUVE,   /**< Atmospheric, slow-burn, visual */
    STYLE_CINEMA_COPPOLA,      /**< Epic family saga, shadows, operatic */
    STYLE_CINEMA_COUNT
} cinematic_style_archetype_t;

/**
 * @brief Style embedding vector (latent representation)
 */
typedef struct {
    float* embedding;           /**< Embedding vector */
    uint32_t embedding_dim;     /**< Embedding dimension (typically 256-1024) */
    art_modality_t modality;    /**< Source modality */
    int32_t archetype_id;       /**< Archetype ID or -1 for custom */
    char style_name[64];        /**< Human-readable style name */
    float confidence;           /**< [0-1] Confidence in style identification */
} style_embedding_t;

//=============================================================================
// Creative Influence System
//=============================================================================

/**
 * @brief A single creative influence for blending
 */
typedef struct {
    style_embedding_t style;    /**< Style embedding */
    float weight;               /**< [0-1] Influence weight */
    char source_work[128];      /**< Source work title (optional) */
    char source_artist[64];     /**< Source artist name (optional) */
    bool is_positive;           /**< true = toward, false = away from */
} creative_influence_t;

/**
 * @brief Result of blending multiple influences
 */
typedef struct {
    style_embedding_t blended_style;  /**< Resulting blended style */
    style_embedding_t style;          /**< Alias for blended_style (for compatibility) */
    float* influence_weights;         /**< Final weights after normalization */
    uint32_t num_influences;          /**< Number of influences blended */
    float coherence_score;            /**< [0-1] How coherent the blend is */
    float coherence;                  /**< Alias for coherence_score */
    float novelty_score;              /**< [0-1] How novel the blend is */
    float originality;                /**< Alias for novelty_score */
    bool is_valid;                    /**< True if blend passes all constraints */
} influence_blend_result_t;

//=============================================================================
// Generation Request/Result Types
//=============================================================================

/**
 * @brief Text generation types
 */
typedef enum {
    TEXT_GEN_POETRY = 0,       /**< Poetry (various forms) */
    TEXT_GEN_SHORT_STORY,      /**< Short story */
    TEXT_GEN_NOVEL_CHAPTER,    /**< Novel chapter */
    TEXT_GEN_SCREENPLAY,       /**< Screenplay/script */
    TEXT_GEN_DIALOGUE,         /**< Character dialogue */
    TEXT_GEN_LYRICS,           /**< Song lyrics */
    TEXT_GEN_ESSAY,            /**< Essays/articles */
    TEXT_GEN_COUNT
} text_generation_type_t;

/**
 * @brief Text generation request
 */
typedef struct {
    text_generation_type_t type;        /**< Type of text to generate */
    const char* prompt;                 /**< Generation prompt/seed */
    size_t prompt_len;                  /**< Prompt length */
    style_embedding_t* style;           /**< Target style (optional) */
    uint32_t max_length;                /**< Max output length in tokens */
    float temperature;                  /**< [0-2] Sampling temperature */
    float top_p;                        /**< [0-1] Nucleus sampling threshold */
    const char* theme;                  /**< Thematic guidance (optional) */
    const char* tone;                   /**< Tonal guidance (optional) */
} text_generation_request_t;

/**
 * @brief Text generation result
 */
typedef struct {
    char* text;                         /**< Generated text (heap allocated) */
    size_t text_len;                    /**< Text length */
    aesthetic_evaluation_t evaluation;  /**< Self-evaluation */
    float generation_time_ms;           /**< Time to generate */
    uint32_t tokens_generated;          /**< Number of tokens */
    bool success;                       /**< Generation success */
    char error_message[256];            /**< Error message if failed */
} text_generation_result_t;

/**
 * @brief Music note representation
 */
typedef struct {
    uint8_t pitch;              /**< MIDI pitch (0-127, 60 = middle C) */
    uint8_t channel;            /**< MIDI channel (0-15) */
    float velocity;             /**< [0-1] Note velocity */
    float start_beat;           /**< Start time in beats */
    float duration_beats;       /**< Duration in beats */
    uint8_t instrument;         /**< General MIDI instrument (0-127) */
} music_note_t;

/**
 * @brief Music track (collection of notes)
 */
typedef struct {
    music_note_t* notes;        /**< Array of notes */
    uint32_t num_notes;         /**< Number of notes */
    uint32_t max_notes;         /**< Maximum capacity of notes array */
    char track_name[64];        /**< Track name */
    uint8_t channel;            /**< Default channel */
    uint8_t instrument;         /**< Default instrument */
} music_track_t;

/**
 * @brief Music generation request
 */
typedef struct {
    style_embedding_t* style;           /**< Target style */
    float duration_seconds;             /**< Target duration */
    uint16_t tempo_bpm;                 /**< Tempo in BPM */
    uint8_t time_sig_numerator;         /**< Time signature numerator */
    uint8_t time_sig_denominator;       /**< Time signature denominator */
    const char* key;                    /**< Key signature (e.g., "C major") */
    const char* mood;                   /**< Mood guidance */
    uint32_t num_tracks;                /**< Number of tracks to generate */
    bool generate_audio;                /**< Also generate audio waveform */
    uint32_t sample_rate;               /**< Audio sample rate if generating audio */
} music_generation_request_t;

/**
 * @brief Music generation result
 */
typedef struct {
    music_track_t* tracks;              /**< Generated tracks */
    uint32_t num_tracks;                /**< Number of tracks */
    float* audio_data;                  /**< Audio waveform (if requested) */
    uint64_t audio_samples;             /**< Number of audio samples */
    uint32_t sample_rate;               /**< Audio sample rate */
    uint16_t tempo_bpm;                 /**< Tempo in BPM */
    float duration_seconds;             /**< Actual duration */
    aesthetic_evaluation_t evaluation;  /**< Self-evaluation */
    float generation_time_ms;           /**< Time to generate */
    bool success;                       /**< Generation success */
    char error_message[256];            /**< Error message if failed */
} music_generation_result_t;

/**
 * @brief Visual image representation
 */
typedef struct {
    uint8_t* pixels;            /**< Pixel data (RGB or RGBA) */
    uint32_t width;             /**< Image width */
    uint32_t height;            /**< Image height */
    uint8_t channels;           /**< Number of channels (3=RGB, 4=RGBA) */
    bool owns_pixels;           /**< true if pixels should be freed on cleanup */
} visual_image_t;

/**
 * @brief Visual generation request
 */
typedef struct {
    const char* prompt;                 /**< Text prompt describing image */
    const char* negative_prompt;        /**< What to avoid (optional) */
    style_embedding_t* style;           /**< Target style */
    uint32_t width;                     /**< Output width */
    uint32_t height;                    /**< Output height */
    uint32_t steps;                     /**< Diffusion steps */
    float guidance_scale;               /**< CFG scale (7.5 typical) */
    uint64_t seed;                      /**< Random seed (0 for random) */
    visual_image_t* reference_image;    /**< Reference image for img2img (optional) */
    float reference_strength;           /**< [0-1] How much to follow reference */
} visual_generation_request_t;

/**
 * @brief Visual generation result
 */
typedef struct {
    visual_image_t image;               /**< Generated image */
    aesthetic_evaluation_t evaluation;  /**< Self-evaluation */
    float generation_time_ms;           /**< Time to generate */
    uint64_t seed_used;                 /**< Actual seed used */
    bool success;                       /**< Generation success */
    char error_message[256];            /**< Error message if failed */
} visual_generation_result_t;

/**
 * @brief Creative project types for multimodal direction
 */
typedef enum {
    PROJECT_SHORT_FILM = 0,    /**< Short film (< 40 min) */
    PROJECT_FEATURE_FILM,      /**< Feature film (90-180 min) */
    PROJECT_TV_EPISODE,        /**< TV episode */
    PROJECT_MUSIC_VIDEO,       /**< Music video */
    PROJECT_DOCUMENTARY,       /**< Documentary */
    PROJECT_ANIMATION,         /**< Animated feature/short */
    PROJECT_ADVERTISEMENT,     /**< Commercial/ad */
    PROJECT_COUNT
} creative_project_type_t;

/**
 * @brief Scene specification for video
 */
typedef struct {
    char description[512];      /**< Scene description */
    float duration_seconds;     /**< Scene duration */
    char* dialogue;             /**< Dialogue script (heap allocated) */
    visual_image_t* keyframes;  /**< Key visual frames */
    uint32_t num_keyframes;     /**< Number of keyframes */
    music_track_t* music_cue;   /**< Music for this scene (optional) */
    char mood[64];              /**< Scene mood */
    char location[128];         /**< Location description */
} scene_spec_t;

/**
 * @brief Full project specification
 */
typedef struct {
    creative_project_type_t type;       /**< Project type */
    char title[128];                    /**< Project title */
    char logline[256];                  /**< One-line summary */
    char synopsis[2048];                /**< Full synopsis */
    scene_spec_t* scenes;               /**< Scene specifications */
    uint32_t num_scenes;                /**< Number of scenes */
    style_embedding_t visual_style;     /**< Visual style for whole project */
    style_embedding_t music_style;      /**< Music style for whole project */
    cinematic_style_archetype_t cinema_style; /**< Cinematic reference */
    float target_duration_minutes;      /**< Target duration */
    char target_rating[8];              /**< Target rating (G, PG, PG-13, R) */
} project_specification_t;

/**
 * @brief Project output (video/film)
 */
typedef struct {
    uint8_t* video_data;                /**< Video data (encoded) */
    uint64_t video_size;                /**< Video data size in bytes */
    const char* codec;                  /**< Video codec used */
    float duration_seconds;             /**< Actual duration */
    uint32_t width;                     /**< Video width */
    uint32_t height;                    /**< Video height */
    float fps;                          /**< Frames per second */
    music_generation_result_t soundtrack; /**< Generated soundtrack */
    text_generation_result_t screenplay;  /**< Generated screenplay */
    aesthetic_evaluation_t evaluation;    /**< Overall quality evaluation */
    float generation_time_seconds;        /**< Total generation time */
    bool success;                         /**< Generation success */
    char error_message[256];              /**< Error message if failed */
} project_output_t;

//=============================================================================
// Validation Types (Defense-in-Depth)
//=============================================================================

/**
 * @brief Creative validation result codes
 */
typedef enum {
    CREATIVE_VALIDATION_PASS = 0,  /**< Content passes all checks */
    CREATIVE_VALIDATION_WARN,      /**< Content has warnings but allowed */
    CREATIVE_VALIDATION_ESCALATE,  /**< Needs human review */
    CREATIVE_VALIDATION_DENY       /**< Content blocked */
} creative_validation_result_t;

/**
 * @brief Reasons for denying creative content
 */
typedef enum {
    CREATIVE_DENY_NONE = 0,        /**< No denial */
    CREATIVE_DENY_COPYRIGHT,       /**< Too similar to existing work */
    CREATIVE_DENY_HARMFUL_CONTENT, /**< Ethics violation */
    CREATIVE_DENY_QUALITY,         /**< Below quality threshold */
    CREATIVE_DENY_INCOHERENT,      /**< Fails coherence check */
    CREATIVE_DENY_EXPLICIT,        /**< Inappropriate explicit content */
    CREATIVE_DENY_BIAS,            /**< Contains harmful bias */
    CREATIVE_DENY_MISINFORMATION   /**< Contains false claims presented as fact */
} creative_deny_reason_t;

/**
 * @brief Full validation report
 */
typedef struct {
    creative_validation_result_t result;    /**< Overall result */
    creative_deny_reason_t deny_reason;     /**< Reason if denied */
    float copyright_similarity;             /**< [0-1] Max similarity to known works */
    char similar_work[128];                 /**< Most similar known work */
    float quality_score;                    /**< [0-1] Quality score */
    float coherence_score;                  /**< [0-1] Coherence score */
    bool has_harmful_content;               /**< Harmful content detected */
    char harmful_content_type[64];          /**< Type of harmful content */
    uint32_t num_warnings;                  /**< Number of warnings */
    char warnings[4][256];                  /**< Warning messages */
} creative_validation_report_t;

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief Creative system configuration
 */
typedef struct {
    /* System enables */
    bool enable_appreciation;           /**< Enable appreciation subsystem */
    bool enable_inspiration;            /**< Enable inspiration subsystem */
    bool enable_text_generation;        /**< Enable text generation */
    bool enable_music_generation;       /**< Enable music generation */
    bool enable_visual_generation;      /**< Enable visual generation */
    bool enable_video_generation;       /**< Enable video generation */
    bool enable_multimodal_direction;   /**< Enable full film direction */

    /* Quality thresholds */
    float min_quality_threshold;        /**< Minimum acceptable quality */
    float copyright_similarity_threshold; /**< Max acceptable copyright similarity */

    /* Resource limits */
    uint32_t max_generation_time_ms;    /**< Max time for single generation */
    uint64_t max_memory_bytes;          /**< Max memory for generation */

    /* External model paths */
    char onnx_model_dir[512];           /**< ONNX model directory */
    char diffusion_model_path[512];     /**< Diffusion model path */
    char music_model_path[512];         /**< Music model path */
    char text_model_path[512];          /**< Text model path */

    /* API configuration */
    bool use_cloud_fallback;            /**< Fall back to cloud APIs */
    char api_base_url[256];             /**< Cloud API base URL */
    char api_key[128];                  /**< API key (if using cloud) */

    /* Device configuration */
    uint8_t device_type;                /**< 0=CPU, 1=CUDA, 2=TensorRT */
    int32_t device_id;                  /**< GPU device ID (if applicable) */

    /* Integration flags */
    bool integrate_with_emotion;        /**< Connect to emotion system */
    bool integrate_with_memory;         /**< Connect to hippocampus */
    bool integrate_with_ethics;         /**< Enable ethics validation */
    bool integrate_with_immune;         /**< Connect to immune system */
} creative_config_t;

/**
 * @brief Initialize creative config with defaults
 */
void creative_config_init_defaults(creative_config_t* config);

//=============================================================================
// Orchestrator API
//=============================================================================

/**
 * @brief Create creative orchestrator
 *
 * @param config Configuration
 * @return Orchestrator handle or NULL on error
 */
creative_orchestrator_t* creative_orchestrator_create(const creative_config_t* config);

/**
 * @brief Destroy creative orchestrator
 *
 * @param orchestrator Orchestrator to destroy
 */
void creative_orchestrator_destroy(creative_orchestrator_t* orchestrator);

/**
 * @brief Update creative system (call periodically)
 *
 * @param orchestrator Orchestrator
 * @param dt_us Time delta in microseconds
 * @return 0 on success, -1 on error
 */
int creative_orchestrator_update(creative_orchestrator_t* orchestrator, uint64_t dt_us);

//=============================================================================
// Appreciation API
//=============================================================================

/**
 * @brief Evaluate aesthetic quality of text
 *
 * @param orchestrator Orchestrator with appreciation enabled
 * @param text Text content
 * @param len Text length
 * @param modality Text modality
 * @param out Output evaluation
 * @return 0 on success, -1 on error
 */
int creative_evaluate_text(creative_orchestrator_t* orchestrator,
                           const char* text, size_t len,
                           art_modality_t modality,
                           aesthetic_evaluation_t* out);

/**
 * @brief Evaluate aesthetic quality of music
 *
 * @param orchestrator Orchestrator
 * @param tracks Music tracks
 * @param num_tracks Number of tracks
 * @param out Output evaluation
 * @return 0 on success, -1 on error
 */
int creative_evaluate_music(creative_orchestrator_t* orchestrator,
                            const music_track_t* tracks, uint32_t num_tracks,
                            aesthetic_evaluation_t* out);

/**
 * @brief Evaluate aesthetic quality of image
 *
 * @param orchestrator Orchestrator
 * @param image Image to evaluate
 * @param out Output evaluation
 * @return 0 on success, -1 on error
 */
int creative_evaluate_visual(creative_orchestrator_t* orchestrator,
                             const visual_image_t* image,
                             aesthetic_evaluation_t* out);

//=============================================================================
// Inspiration API
//=============================================================================

/**
 * @brief Extract style from content
 *
 * @param orchestrator Orchestrator
 * @param content Content pointer (type depends on modality)
 * @param modality Content modality
 * @param out Output style embedding
 * @return 0 on success, -1 on error
 */
int creative_extract_style(creative_orchestrator_t* orchestrator,
                           const void* content,
                           art_modality_t modality,
                           style_embedding_t* out);

/**
 * @brief Blend multiple influences into new style
 *
 * @param orchestrator Orchestrator
 * @param influences Array of influences
 * @param num_influences Number of influences
 * @param out Output blend result
 * @return 0 on success, -1 on error
 */
int creative_blend_influences(creative_orchestrator_t* orchestrator,
                              const creative_influence_t* influences,
                              uint32_t num_influences,
                              influence_blend_result_t* out);

/**
 * @brief Get style embedding for archetype
 *
 * @param orchestrator Orchestrator
 * @param modality Modality (determines which archetype enum to use)
 * @param archetype_id Archetype ID from appropriate enum
 * @param out Output style embedding
 * @return 0 on success, -1 on error
 */
int creative_get_archetype_style(creative_orchestrator_t* orchestrator,
                                 art_modality_t modality,
                                 int32_t archetype_id,
                                 style_embedding_t* out);

//=============================================================================
// Generation API
//=============================================================================

/**
 * @brief Generate text content
 *
 * @param orchestrator Orchestrator
 * @param request Generation request
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int creative_generate_text(creative_orchestrator_t* orchestrator,
                           const text_generation_request_t* request,
                           text_generation_result_t* result);

/**
 * @brief Generate music
 *
 * @param orchestrator Orchestrator
 * @param request Generation request
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int creative_generate_music(creative_orchestrator_t* orchestrator,
                            const music_generation_request_t* request,
                            music_generation_result_t* result);

/**
 * @brief Generate visual image
 *
 * @param orchestrator Orchestrator
 * @param request Generation request
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int creative_generate_visual(creative_orchestrator_t* orchestrator,
                             const visual_generation_request_t* request,
                             visual_generation_result_t* result);

/**
 * @brief Develop full project concept
 *
 * @param orchestrator Orchestrator
 * @param concept_description Initial concept description
 * @param type Project type
 * @param out_spec Output project specification
 * @return 0 on success, -1 on error
 */
int creative_develop_project(creative_orchestrator_t* orchestrator,
                             const char* concept_description,
                             creative_project_type_t type,
                             project_specification_t* out_spec);

/**
 * @brief Produce full project (video generation)
 *
 * @param orchestrator Orchestrator
 * @param spec Project specification
 * @param output Output project
 * @return 0 on success, -1 on error
 */
int creative_produce_project(creative_orchestrator_t* orchestrator,
                             const project_specification_t* spec,
                             project_output_t* output);

//=============================================================================
// Validation API
//=============================================================================

/**
 * @brief Validate generated content
 *
 * @param orchestrator Orchestrator
 * @param content Content to validate
 * @param modality Content modality
 * @param report Output validation report
 * @return 0 on success, -1 on error
 */
int creative_validate_content(creative_orchestrator_t* orchestrator,
                              const void* content,
                              art_modality_t modality,
                              creative_validation_report_t* report);

//=============================================================================
// Export API
//=============================================================================

/**
 * @brief Export music to MIDI file
 *
 * @param result Music generation result
 * @param path Output file path
 * @return 0 on success, -1 on error
 */
int creative_export_midi(const music_generation_result_t* result, const char* path);

/**
 * @brief Export music to audio file
 *
 * @param result Music generation result
 * @param path Output file path
 * @param format Audio format ("wav", "mp3", "flac")
 * @return 0 on success, -1 on error
 */
int creative_export_audio(const music_generation_result_t* result,
                          const char* path, const char* format);

/**
 * @brief Export image to file
 *
 * @param image Image to export
 * @param path Output file path
 * @param format Image format ("png", "jpg", "bmp")
 * @return 0 on success, -1 on error
 */
int creative_export_image(const visual_image_t* image,
                          const char* path, const char* format);

/**
 * @brief Export video to file
 *
 * @param output Project output containing video
 * @param path Output file path
 * @param format Video format ("mp4", "webm", "mov")
 * @return 0 on success, -1 on error
 */
int creative_export_video(const project_output_t* output,
                          const char* path, const char* format);

//=============================================================================
// Cleanup Helpers
//=============================================================================

/**
 * @brief Free text generation result resources
 */
void creative_text_result_free(text_generation_result_t* result);

/**
 * @brief Free music generation result resources
 */
void creative_music_result_free(music_generation_result_t* result);

/**
 * @brief Free visual generation result resources
 */
void creative_visual_result_free(visual_generation_result_t* result);

/**
 * @brief Free project specification resources
 */
void creative_project_spec_free(project_specification_t* spec);

/**
 * @brief Free project output resources
 */
void creative_project_output_free(project_output_t* output);

//=============================================================================
// Style Embedding Operations API
//=============================================================================

/**
 * @brief Create a style embedding with specified dimension
 *
 * @param embedding Embedding to initialize
 * @param dim Embedding dimension
 * @return 0 on success, -1 on error
 */
int style_embedding_create(style_embedding_t* embedding, uint32_t dim);

/**
 * @brief Destroy/free a style embedding's allocated resources
 *
 * @param embedding Embedding to destroy
 */
void style_embedding_destroy(style_embedding_t* embedding);

/**
 * @brief Clone a style embedding
 *
 * @param src Source embedding
 * @param dst Destination embedding
 * @return 0 on success, -1 on error
 */
int style_embedding_clone(const style_embedding_t* src, style_embedding_t* dst);

/**
 * @brief Calculate similarity between two style embeddings
 *
 * @param a First embedding
 * @param b Second embedding
 * @return Similarity [0-1] (cosine similarity, normalized)
 */
float style_embedding_similarity(const style_embedding_t* a, const style_embedding_t* b);

/**
 * @brief Interpolate between two style embeddings
 *
 * @param a First embedding
 * @param b Second embedding
 * @param t Interpolation factor [0-1]
 * @param out Output interpolated embedding
 * @return 0 on success, -1 on error
 */
int style_embedding_interpolate(const style_embedding_t* a,
                                 const style_embedding_t* b,
                                 float t,
                                 style_embedding_t* out);

/**
 * @brief Normalize a style embedding to unit length
 *
 * @param embedding Embedding to normalize (in-place)
 */
void style_embedding_normalize(style_embedding_t* embedding);

//=============================================================================
// Cleanup API
//=============================================================================

/**
 * @brief Free style embedding resources
 */
void creative_style_embedding_free(style_embedding_t* style);

/**
 * @brief Free influence blend result resources
 */
void creative_blend_result_free(influence_blend_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CREATIVE_H */
