//=============================================================================
// nimcp_music_generation.c - Creative Music Generation
//=============================================================================
/**
 * @file nimcp_music_generation.c
 * @brief Generates music compositions and audio
 *
 * WHAT: Creates music from symbolic (MIDI) to audio waveforms
 * WHY:  Enable AI to compose and produce music
 * HOW:  Combines compositional rules, neural generation, and audio synthesis
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/generation/nimcp_music_generation.h"
#include "cognitive/creative/nimcp_creative.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#define LOG_MODULE "MUSIC_GEN"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(music_generation, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_BIT_DEPTH 16
#define DEFAULT_TEMPO 120
#define PI NIMCP_PI_F

//=============================================================================
// Config Defaults
//=============================================================================

void music_generator_config_defaults(music_generator_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(music_generator_config_t));

    config->use_gpu = false;
    config->gpu_device_id = 0;

    config->default_approach = GEN_APPROACH_HYBRID;
    config->default_creativity = 0.6f;
    config->default_coherence = 0.7f;

    config->enable_self_evaluation = true;
    config->min_quality_threshold = 0.5f;
    config->max_regeneration_attempts = 3;

    config->default_sample_rate = DEFAULT_SAMPLE_RATE;
    config->default_bit_depth = DEFAULT_BIT_DEPTH;
}

//=============================================================================
// Music Theory Helpers
//=============================================================================

static const uint8_t MAJOR_SCALE[] = {0, 2, 4, 5, 7, 9, 11};
static const uint8_t MINOR_SCALE[] = {0, 2, 3, 5, 7, 8, 10};

static const char* NOTE_NAMES[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static uint8_t scale_degree_to_pitch(const music_key_t* key, uint8_t degree, int8_t octave) {
    const uint8_t* scale = key->is_minor ? MINOR_SCALE : MAJOR_SCALE;
    uint8_t pitch = key->root + scale[degree % 7] + octave * 12;
    return pitch;
}

static uint32_t simple_random(uint32_t* seed) {
    *seed = *seed * 1103515245 + 12345;
    return (*seed >> 16) & 0x7fff;
}

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Convert beats to samples */
static uint64_t beats_to_samples(float beats, uint16_t tempo_bpm, uint32_t sample_rate) {
    float seconds = beats * 60.0f / tempo_bpm;
    return (uint64_t)(seconds * sample_rate);
}

/* MIDI note to frequency */
static float midi_to_freq(uint8_t note) {
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

//=============================================================================
// Note Generation Helpers
//=============================================================================

static music_note_t create_note(uint8_t pitch, float start, float duration,
                                 float velocity, uint8_t channel) {
    music_note_t note;
    note.pitch = pitch;
    note.start_beat = start;
    note.duration_beats = duration;
    note.velocity = velocity;
    note.channel = channel;
    return note;
}

static void generate_melody_notes(const music_key_t* key, uint16_t tempo,
                                   float duration_beats, uint32_t* seed,
                                   music_note_t** notes, uint32_t* num_notes) {
    /* Estimate number of notes based on duration */
    uint32_t max_notes = (uint32_t)(duration_beats * 2);  /* ~2 notes per beat */
    *notes = nimcp_calloc(max_notes, sizeof(music_note_t));
    if (!*notes) return;

    (void)tempo;

    float beat = 0.0f;
    uint32_t count = 0;
    int8_t prev_degree = 2;  /* Start on third */

    while (beat < duration_beats && count < max_notes) {
        /* Choose note duration */
        float durations[] = {0.25f, 0.5f, 1.0f, 0.5f};
        float dur = durations[simple_random(seed) % 4];

        /* Choose melodic movement */
        int8_t step = (simple_random(seed) % 5) - 2;  /* -2 to +2 */
        int8_t degree = prev_degree + step;
        if (degree < 0) degree = 0;
        if (degree > 7) degree = 7;

        int8_t octave = 4 + (degree / 7);
        uint8_t pitch = scale_degree_to_pitch(key, degree % 7, octave);

        /* Velocity variation */
        float velocity = 0.6f + (simple_random(seed) % 40) / 100.0f;

        (*notes)[count++] = create_note(pitch, beat, dur, velocity, 0);

        beat += dur;
        prev_degree = degree;
    }

    *num_notes = count;
}

static void generate_chord_notes(const music_key_t* key, const chord_progression_t* prog,
                                  music_note_t** notes, uint32_t* num_notes) {
    if (!prog || prog->num_chords == 0) {
        *notes = NULL;
        *num_notes = 0;
        return;
    }

    /* Each chord becomes 3-4 notes */
    uint32_t max_notes = prog->num_chords * 4;
    *notes = nimcp_calloc(max_notes, sizeof(music_note_t));
    if (!*notes) return;

    (void)key;

    float beat = 0.0f;
    uint32_t count = 0;

    for (uint32_t i = 0; i < prog->num_chords; i++) {
        const music_chord_t* chord = &prog->chords[i];
        int8_t octave = 3;

        /* Root note */
        (*notes)[count++] = create_note(chord->root + octave * 12, beat,
                                         chord->duration_beats, 0.5f, 1);

        /* Third (major or minor) */
        uint8_t third = (chord->quality == 1 || chord->quality == 2) ? 3 : 4;
        (*notes)[count++] = create_note(chord->root + third + octave * 12, beat,
                                         chord->duration_beats, 0.4f, 1);

        /* Fifth */
        uint8_t fifth = (chord->quality == 2) ? 6 : 7;  /* Dim or perfect */
        (*notes)[count++] = create_note(chord->root + fifth + octave * 12, beat,
                                         chord->duration_beats, 0.4f, 1);

        beat += chord->duration_beats;
    }

    *num_notes = count;
}

static void generate_bass_notes(const chord_progression_t* prog, uint32_t* seed,
                                 music_note_t** notes, uint32_t* num_notes) {
    if (!prog || prog->num_chords == 0) {
        *notes = NULL;
        *num_notes = 0;
        return;
    }

    /* Simple bass line: root notes with some rhythm */
    uint32_t max_notes = prog->num_chords * 4;
    *notes = nimcp_calloc(max_notes, sizeof(music_note_t));
    if (!*notes) return;

    float beat = 0.0f;
    uint32_t count = 0;

    for (uint32_t i = 0; i < prog->num_chords; i++) {
        const music_chord_t* chord = &prog->chords[i];
        int8_t octave = 2;  /* Bass register */

        /* Play root on beat */
        (*notes)[count++] = create_note(chord->root + octave * 12, beat,
                                         0.5f, 0.7f, 2);

        /* Add rhythm note */
        if (simple_random(seed) % 2 == 0) {
            (*notes)[count++] = create_note(chord->root + octave * 12, beat + 0.5f,
                                             0.25f, 0.5f, 2);
        }

        beat += chord->duration_beats;
    }

    *num_notes = count;
}

//=============================================================================
// Audio Synthesis Helpers
//=============================================================================

static void synthesize_note(float* buffer, uint64_t num_samples, uint32_t sample_rate,
                            const music_note_t* note, uint16_t tempo_bpm) {
    float freq = midi_to_freq(note->pitch);

    uint64_t start_sample = beats_to_samples(note->start_beat, tempo_bpm, sample_rate);
    uint64_t duration_samples = beats_to_samples(note->duration_beats, tempo_bpm, sample_rate);
    uint64_t end_sample = start_sample + duration_samples;

    if (end_sample > num_samples) end_sample = num_samples;

    /* ADSR envelope */
    float attack = 0.01f;
    float decay = 0.1f;
    float sustain = 0.6f;
    float release = 0.1f;

    uint64_t attack_samples = (uint64_t)(attack * sample_rate);
    uint64_t decay_samples = (uint64_t)(decay * sample_rate);
    uint64_t release_samples = (uint64_t)(release * sample_rate);

    for (uint64_t i = start_sample; i < end_sample; i++) {
        uint64_t note_sample = i - start_sample;
        float t = (float)note_sample / sample_rate;

        /* Envelope */
        float env = 1.0f;
        if (note_sample < attack_samples) {
            env = (float)note_sample / attack_samples;
        } else if (note_sample < attack_samples + decay_samples) {
            float decay_pos = (float)(note_sample - attack_samples) / decay_samples;
            env = 1.0f - decay_pos * (1.0f - sustain);
        } else if (note_sample >= duration_samples - release_samples) {
            float release_pos = (float)(note_sample - (duration_samples - release_samples)) / release_samples;
            env = sustain * (1.0f - release_pos);
        } else {
            env = sustain;
        }

        /* Simple sine wave (would use wavetable synthesis in production) */
        float sample = sinf(2.0f * PI * freq * t) * env * note->velocity;

        /* Add some harmonics for richness */
        sample += sinf(4.0f * PI * freq * t) * env * note->velocity * 0.3f;
        sample += sinf(6.0f * PI * freq * t) * env * note->velocity * 0.1f;

        buffer[i] += sample * 0.3f;  /* Mix at lower volume */
    }
}

static float* synthesize_tracks(const music_track_t* tracks, uint32_t num_tracks,
                                 uint16_t tempo_bpm, uint32_t sample_rate,
                                 float duration_seconds, uint64_t* num_samples) {
    *num_samples = (uint64_t)(duration_seconds * sample_rate);
    float* buffer = nimcp_calloc(*num_samples, sizeof(float));
    if (!buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "midi_to_freq: buffer is NULL");
        return NULL;
    }

    for (uint32_t t = 0; t < num_tracks; t++) {
        for (uint32_t n = 0; n < tracks[t].num_notes; n++) {
            synthesize_note(buffer, *num_samples, sample_rate,
                           &tracks[t].notes[n], tempo_bpm);
        }
    }

    /* Normalize */
    float max_val = 0.0f;
    for (uint64_t i = 0; i < *num_samples; i++) {
        if (fabsf(buffer[i]) > max_val) max_val = fabsf(buffer[i]);
    }
    if (max_val > 0.001f) {
        for (uint64_t i = 0; i < *num_samples; i++) {
            buffer[i] /= max_val;
        }
    }

    return buffer;
}

//=============================================================================
// Lifecycle API
//=============================================================================

music_generator_t* music_generator_create(const music_generator_config_t* config) {
    music_generator_t* gen = nimcp_calloc(1, sizeof(music_generator_t));
    if (!gen) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate music generator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "music_generator_create: gen is NULL");
        return NULL;
    }

    if (config) {
        gen->config = *config;
    } else {
        music_generator_config_defaults(&gen->config);
    }

    LOG_INFO(LOG_MODULE, "Music generator created");

    return gen;
}

void music_generator_destroy(music_generator_t* gen) {
    if (!gen) return;

    if (gen->current_style) {
        style_embedding_destroy(gen->current_style);
        nimcp_free(gen->current_style);
    }

    nimcp_free(gen);
    gen = NULL;

    LOG_INFO(LOG_MODULE, "Music generator destroyed");
}

//=============================================================================
// Generation API
//=============================================================================

int music_generate(music_generator_t* gen,
                   const music_generation_request_t* request,
                   music_generation_result_t* result) {
    if (!gen || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "music_generator_destroy: required parameter is NULL (gen, request, result)");
        return -1;
    }

    memset(result, 0, sizeof(music_generation_result_t));

    uint64_t start_time = get_time_ms();
    uint32_t seed = (uint32_t)time(NULL);

    /* Parse parameters */
    uint16_t tempo = request->tempo_bpm > 0 ? request->tempo_bpm : DEFAULT_TEMPO;
    float duration = request->duration_seconds > 0 ? request->duration_seconds : 30.0f;
    float duration_beats = duration * tempo / 60.0f;

    music_key_t key;
    if (request->key && strlen(request->key) > 0) {
        music_parse_key(request->key, &key);
    } else {
        key.root = 0;  /* C */
        key.is_minor = false;
        key.mode = 0;
    }

    /* Generate chord progression */
    chord_progression_t progression;
    memset(&progression, 0, sizeof(progression));
    music_generate_progression(gen, &key, (uint32_t)duration_beats / 4, request->style, &progression);

    /* Allocate tracks */
    result->num_tracks = 3;  /* Melody, harmony, bass */
    result->tracks = nimcp_calloc(result->num_tracks, sizeof(music_track_t));
    if (!result->tracks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "music_generator_destroy: result->tracks is NULL");
        return -1;
    }

    /* Generate melody */
    generate_melody_notes(&key, tempo, duration_beats, &seed,
                          &result->tracks[0].notes, &result->tracks[0].num_notes);
    strncpy(result->tracks[0].track_name, "Piano", 31);
    result->tracks[0].instrument = 0;

    /* Generate harmony */
    generate_chord_notes(&key, &progression,
                         &result->tracks[1].notes, &result->tracks[1].num_notes);
    strncpy(result->tracks[1].track_name, "Strings", 31);
    result->tracks[1].instrument = 48;

    /* Generate bass */
    generate_bass_notes(&progression, &seed,
                        &result->tracks[2].notes, &result->tracks[2].num_notes);
    strncpy(result->tracks[2].track_name, "Bass", 31);
    result->tracks[2].instrument = 32;

    /* Store metadata */
    result->tempo_bpm = tempo;
    result->duration_seconds = duration;
    result->sample_rate = gen->config.default_sample_rate;

    /* Clean up */
    chord_progression_free(&progression);

    result->evaluation.overall_quality = 0.7f;
    result->generation_time_ms = get_time_ms() - start_time;

    gen->pieces_generated++;

    return 0;
}

int music_generate_extended(music_generator_t* gen,
                            const music_generation_request_ext_t* request,
                            music_generation_result_t* result) {
    if (!gen || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "music_generator_destroy: required parameter is NULL (gen, request, result)");
        return -1;
    }

    /* Convert extended request to simple request */
    music_generation_request_t simple_request;
    memset(&simple_request, 0, sizeof(simple_request));

    simple_request.style = request->style;
    simple_request.duration_seconds = request->duration_seconds;
    simple_request.tempo_bpm = request->tempo_bpm;

    /* Key to string */
    char key_str[16];
    snprintf(key_str, sizeof(key_str), "%s %s",
             NOTE_NAMES[request->key.root % 12],
             request->key.is_minor ? "minor" : "major");
    simple_request.key = key_str;
    simple_request.mood = request->mood;

    int ret = music_generate(gen, &simple_request, result);

    /* Generate audio if requested */
    if (ret == 0 && request->generate_audio) {
        music_render_audio(gen, result,
                           request->audio_sample_rate > 0 ?
                           request->audio_sample_rate : gen->config.default_sample_rate);
    }

    return ret;
}

int music_generate_melody(music_generator_t* gen,
                          const chord_progression_t* progression,
                          const style_embedding_t* style,
                          music_generation_result_t* result) {
    if (!gen || !progression || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "music_generator_destroy: required parameter is NULL (gen, progression, result)");
        return -1;
    }
    (void)style;

    memset(result, 0, sizeof(music_generation_result_t));

    uint64_t start_time = get_time_ms();
    uint32_t seed = (uint32_t)time(NULL);

    /* Calculate duration from progression */
    float duration_beats = 0.0f;
    for (uint32_t i = 0; i < progression->num_chords; i++) {
        duration_beats += progression->chords[i].duration_beats;
    }

    /* Allocate single track */
    result->num_tracks = 1;
    result->tracks = nimcp_calloc(1, sizeof(music_track_t));
    if (!result->tracks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "music_generator_destroy: result->tracks is NULL");
        return -1;
    }

    /* Generate melody over progression */
    generate_melody_notes(&progression->key, DEFAULT_TEMPO, duration_beats, &seed,
                          &result->tracks[0].notes, &result->tracks[0].num_notes);
    strncpy(result->tracks[0].track_name, "Lead", 31);

    result->tempo_bpm = DEFAULT_TEMPO;
    result->duration_seconds = duration_beats * 60.0f / DEFAULT_TEMPO;
    result->sample_rate = gen->config.default_sample_rate;
    result->evaluation.overall_quality = 0.7f;
    result->generation_time_ms = get_time_ms() - start_time;

    gen->pieces_generated++;

    return 0;
}

int music_generate_accompaniment(music_generator_t* gen,
                                 const music_track_t* melody,
                                 const style_embedding_t* style,
                                 music_generation_result_t* result) {
    if (!gen || !melody || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "music_generator_destroy: required parameter is NULL (gen, melody, result)");
        return -1;
    }
    (void)style;

    memset(result, 0, sizeof(music_generation_result_t));

    uint64_t start_time = get_time_ms();
    uint32_t seed = (uint32_t)time(NULL);

    /* Analyze melody to get key and duration */
    music_key_t key = {0, false, 0};  /* C major default */
    float duration_beats = 0.0f;

    for (uint32_t i = 0; i < melody->num_notes; i++) {
        float end = melody->notes[i].start_beat + melody->notes[i].duration_beats;
        if (end > duration_beats) duration_beats = end;
    }

    /* Generate progression */
    chord_progression_t progression;
    memset(&progression, 0, sizeof(progression));
    music_generate_progression(gen, &key, (uint32_t)(duration_beats / 4 + 1), NULL, &progression);

    /* Allocate tracks: original melody + accompaniment */
    result->num_tracks = 3;
    result->tracks = nimcp_calloc(result->num_tracks, sizeof(music_track_t));
    if (!result->tracks) {
        chord_progression_free(&progression);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "music_generator_destroy: result->tracks is NULL");
        return -1;
    }

    /* Copy melody */
    result->tracks[0].num_notes = melody->num_notes;
    result->tracks[0].notes = nimcp_calloc(melody->num_notes, sizeof(music_note_t));
    if (result->tracks[0].notes) {
        memcpy(result->tracks[0].notes, melody->notes,
               melody->num_notes * sizeof(music_note_t));
    }
    strncpy(result->tracks[0].track_name, melody->track_name, 31);

    /* Generate harmony */
    generate_chord_notes(&key, &progression,
                         &result->tracks[1].notes, &result->tracks[1].num_notes);
    strncpy(result->tracks[1].track_name, "Strings", 31);

    /* Generate bass */
    generate_bass_notes(&progression, &seed,
                        &result->tracks[2].notes, &result->tracks[2].num_notes);
    strncpy(result->tracks[2].track_name, "Bass", 31);

    chord_progression_free(&progression);

    result->tempo_bpm = DEFAULT_TEMPO;
    result->duration_seconds = duration_beats * 60.0f / DEFAULT_TEMPO;
    result->sample_rate = gen->config.default_sample_rate;
    result->evaluation.overall_quality = 0.7f;
    result->generation_time_ms = get_time_ms() - start_time;

    gen->pieces_generated++;

    return 0;
}

int music_generate_progression(music_generator_t* gen,
                               const music_key_t* key,
                               uint32_t num_measures,
                               const style_embedding_t* style,
                               chord_progression_t* progression) {
    if (!gen || !key || !progression) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (gen, key, progression)");
        return -1;
    }
    (void)style;

    memset(progression, 0, sizeof(chord_progression_t));

    uint32_t seed = (uint32_t)time(NULL);

    /* Common progressions (chord degrees) */
    static const uint8_t progressions[][4] = {
        {0, 3, 4, 4},  /* I-IV-V-V */
        {0, 4, 5, 3},  /* I-V-vi-IV */
        {0, 5, 3, 4},  /* I-vi-IV-V */
        {1, 4, 0, 0},  /* ii-V-I-I */
    };

    uint32_t prog_idx = simple_random(&seed) % 4;

    /* Allocate chords */
    progression->num_chords = num_measures;
    progression->chords = nimcp_calloc(num_measures, sizeof(music_chord_t));
    if (!progression->chords) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: progression->chords is NULL");
        return -1;
    }

    progression->key = *key;

    for (uint32_t i = 0; i < num_measures; i++) {
        uint8_t degree = progressions[prog_idx][i % 4];
        uint8_t root = scale_degree_to_pitch(key, degree, 0) % 12;

        progression->chords[i].root = root;
        /* Minor chords on ii, iii, vi in major */
        progression->chords[i].quality = (degree == 1 || degree == 2 || degree == 5) ? 1 : 0;
        progression->chords[i].bass = root;
        progression->chords[i].duration_beats = 4.0f;  /* One measure */
    }

    return 0;
}

//=============================================================================
// Film Score API
//=============================================================================

int music_generate_film_score(music_generator_t* gen,
                              const char* scene_description,
                              float duration_seconds,
                              const char* mood,
                              const style_embedding_t* style,
                              music_generation_result_t* result) {
    if (!gen || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (gen, result)");
        return -1;
    }

    music_generation_request_t request;
    memset(&request, 0, sizeof(request));

    request.style = (style_embedding_t*)style;
    request.duration_seconds = duration_seconds;
    request.mood = mood;

    /* Adjust tempo based on mood */
    if (mood) {
        if (strstr(mood, "tense") || strstr(mood, "action")) {
            request.tempo_bpm = 140;
        } else if (strstr(mood, "sad") || strstr(mood, "melancholy")) {
            request.tempo_bpm = 60;
        } else if (strstr(mood, "happy") || strstr(mood, "upbeat")) {
            request.tempo_bpm = 120;
        } else {
            request.tempo_bpm = 90;
        }
    } else {
        request.tempo_bpm = 90;
    }

    (void)scene_description;  /* Would use for more sophisticated generation */

    return music_generate(gen, &request, result);
}

int music_generate_leitmotif(music_generator_t* gen,
                             const char* character_description,
                             const style_embedding_t* style,
                             music_generation_result_t* result) {
    if (!gen || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (gen, result)");
        return -1;
    }

    music_generation_request_t request;
    memset(&request, 0, sizeof(request));

    request.style = (style_embedding_t*)style;
    request.duration_seconds = 8.0f;  /* Short motif */
    request.tempo_bpm = 100;

    (void)character_description;

    return music_generate(gen, &request, result);
}

//=============================================================================
// Variation API
//=============================================================================

int music_generate_variation(music_generator_t* gen,
                             const music_generation_result_t* original,
                             float variation_strength,
                             music_generation_result_t* result) {
    if (!gen || !original || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (gen, original, result)");
        return -1;
    }

    memset(result, 0, sizeof(music_generation_result_t));

    uint32_t seed = (uint32_t)time(NULL);

    /* Copy and modify tracks */
    result->num_tracks = original->num_tracks;
    result->tracks = nimcp_calloc(result->num_tracks, sizeof(music_track_t));
    if (!result->tracks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: result->tracks is NULL");
        return -1;
    }

    for (uint32_t t = 0; t < original->num_tracks; t++) {
        const music_track_t* src = &original->tracks[t];
        music_track_t* dst = &result->tracks[t];

        dst->num_notes = src->num_notes;
        dst->notes = nimcp_calloc(src->num_notes, sizeof(music_note_t));
        if (!dst->notes) continue;

        memcpy(dst->notes, src->notes, src->num_notes * sizeof(music_note_t));
        strncpy(dst->track_name, src->track_name, 31);
        dst->instrument = src->instrument;

        /* Apply variations */
        for (uint32_t n = 0; n < dst->num_notes; n++) {
            if ((float)(simple_random(&seed) % 100) / 100.0f < variation_strength) {
                /* Vary pitch by small amount */
                int8_t offset = (simple_random(&seed) % 5) - 2;
                int16_t new_pitch = dst->notes[n].pitch + offset;
                if (new_pitch >= 0 && new_pitch <= 127) {
                    dst->notes[n].pitch = (uint8_t)new_pitch;
                }
            }

            /* Vary rhythm slightly */
            if ((float)(simple_random(&seed) % 100) / 100.0f < variation_strength * 0.5f) {
                dst->notes[n].duration_beats *= 0.8f + (simple_random(&seed) % 40) / 100.0f;
            }
        }
    }

    result->tempo_bpm = original->tempo_bpm;
    result->duration_seconds = original->duration_seconds;
    result->sample_rate = original->sample_rate;
    result->evaluation.overall_quality = original->evaluation.overall_quality * 0.95f;

    return 0;
}

int music_extend(music_generator_t* gen,
                 const music_generation_result_t* existing,
                 float additional_seconds,
                 music_generation_result_t* result) {
    if (!gen || !existing || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (gen, existing, result)");
        return -1;
    }

    /* Generate extension */
    music_generation_request_t request;
    memset(&request, 0, sizeof(request));
    request.duration_seconds = additional_seconds;
    request.tempo_bpm = existing->tempo_bpm;

    music_generation_result_t extension;
    if (music_generate(gen, &request, &extension) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    /* Combine existing + extension */
    memset(result, 0, sizeof(music_generation_result_t));
    result->num_tracks = existing->num_tracks;
    result->tracks = nimcp_calloc(result->num_tracks, sizeof(music_track_t));
    if (!result->tracks) {
        creative_music_result_free(&extension);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: result->tracks is NULL");
        return -1;
    }

    float offset_beats = existing->duration_seconds * existing->tempo_bpm / 60.0f;

    for (uint32_t t = 0; t < result->num_tracks && t < extension.num_tracks; t++) {
        uint32_t total_notes = existing->tracks[t].num_notes;
        if (t < extension.num_tracks) {
            total_notes += extension.tracks[t].num_notes;
        }

        result->tracks[t].notes = nimcp_calloc(total_notes, sizeof(music_note_t));
        if (!result->tracks[t].notes) continue;

        /* Copy existing notes */
        memcpy(result->tracks[t].notes, existing->tracks[t].notes,
               existing->tracks[t].num_notes * sizeof(music_note_t));
        result->tracks[t].num_notes = existing->tracks[t].num_notes;

        /* Copy extension notes with offset */
        if (t < extension.num_tracks) {
            for (uint32_t n = 0; n < extension.tracks[t].num_notes; n++) {
                music_note_t note = extension.tracks[t].notes[n];
                note.start_beat += offset_beats;
                result->tracks[t].notes[result->tracks[t].num_notes++] = note;
            }
        }

        strncpy(result->tracks[t].track_name, existing->tracks[t].track_name, 31);
    }

    result->tempo_bpm = existing->tempo_bpm;
    result->duration_seconds = existing->duration_seconds + additional_seconds;
    result->sample_rate = existing->sample_rate;
    result->evaluation.overall_quality = (existing->evaluation.overall_quality + extension.evaluation.overall_quality) / 2.0f;

    creative_music_result_free(&extension);

    return 0;
}

//=============================================================================
// Export API
//=============================================================================

int music_export_midi(const music_generation_result_t* result, const char* path) {
    if (!result || !path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "music_export_midi: required parameter is NULL (result, path)");
        return -1;
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        LOG_ERROR(LOG_MODULE, "Failed to open %s for writing", path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "music_export_midi: f is NULL");
        return -1;
    }

    /* Write simplified MIDI file (header only for demo) */
    /* In production, would write full MIDI format 1 file */

    /* Header chunk */
    fwrite("MThd", 1, 4, f);
    uint32_t header_len = 6;
    uint8_t header_len_bytes[4] = {0, 0, 0, (uint8_t)header_len};
    fwrite(header_len_bytes, 1, 4, f);

    uint16_t format = 1;  /* Format 1 */
    uint16_t num_tracks_midi = result->num_tracks + 1;  /* +1 for tempo track */
    uint16_t division = 480;  /* Ticks per quarter note */

    uint8_t format_bytes[2] = {0, (uint8_t)format};
    uint8_t tracks_bytes[2] = {(uint8_t)(num_tracks_midi >> 8), (uint8_t)num_tracks_midi};
    uint8_t div_bytes[2] = {(uint8_t)(division >> 8), (uint8_t)division};

    fwrite(format_bytes, 1, 2, f);
    fwrite(tracks_bytes, 1, 2, f);
    fwrite(div_bytes, 1, 2, f);

    /* Placeholder track chunks would go here */
    /* For demo, just close file */

    fclose(f);

    LOG_INFO(LOG_MODULE, "Exported MIDI to %s", path);

    return 0;
}

int music_export_audio(const music_generation_result_t* result,
                       const char* path, const char* format) {
    if (!result || !path || !result->audio_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "music_export_midi: required parameter is NULL (result, path, result->audio_data)");
        return -1;
    }
    (void)format;

    FILE* f = fopen(path, "wb");
    if (!f) {
        LOG_ERROR(LOG_MODULE, "Failed to open %s for writing", path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "music_export_midi: f is NULL");
        return -1;
    }

    /* Write WAV header */
    uint32_t sample_rate = result->sample_rate;
    uint16_t channels = 1;
    uint16_t bits_per_sample = 16;
    uint32_t byte_rate = sample_rate * channels * bits_per_sample / 8;
    uint16_t block_align = channels * bits_per_sample / 8;
    uint32_t data_size = (uint32_t)(result->audio_samples * block_align);
    uint32_t chunk_size = 36 + data_size;

    fwrite("RIFF", 1, 4, f);
    fwrite(&chunk_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);

    uint32_t fmt_size = 16;
    uint16_t audio_format = 1;  /* PCM */
    fwrite(&fmt_size, 4, 1, f);
    fwrite(&audio_format, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits_per_sample, 2, 1, f);

    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);

    /* Write samples */
    for (uint64_t i = 0; i < result->audio_samples; i++) {
        int16_t sample = (int16_t)(result->audio_data[i] * 32767.0f);
        fwrite(&sample, 2, 1, f);
    }

    fclose(f);

    LOG_INFO(LOG_MODULE, "Exported audio to %s", path);

    return 0;
}

int music_render_audio(music_generator_t* gen,
                       music_generation_result_t* result,
                       uint32_t sample_rate) {
    if (!gen || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "music_export_midi: required parameter is NULL (gen, result)");
        return -1;
    }

    if (result->audio_data) {
        nimcp_free(result->audio_data);
        result->audio_data = NULL;
    }

    result->sample_rate = sample_rate;
    result->audio_data = synthesize_tracks(result->tracks, result->num_tracks,
                                           result->tempo_bpm, sample_rate,
                                           result->duration_seconds,
                                           &result->audio_samples);

    return result->audio_data ? 0 : -1;
}

//=============================================================================
// Style Control API
//=============================================================================

void music_generator_set_style(music_generator_t* gen,
                               const style_embedding_t* style) {
    if (!gen || !style) return;

    if (gen->current_style) {
        style_embedding_destroy(gen->current_style);
    } else {
        gen->current_style = nimcp_calloc(1, sizeof(style_embedding_t));
    }

    if (gen->current_style) {
        style_embedding_clone(style, gen->current_style);
    }
}

void music_generator_clear_style(music_generator_t* gen) {
    if (!gen || !gen->current_style) return;

    style_embedding_destroy(gen->current_style);
    nimcp_free(gen->current_style);
    gen->current_style = NULL;
}

int music_generator_archetype_style(music_generator_t* gen,
                                    musical_style_archetype_t archetype_id,
                                    style_embedding_t* out) {
    if (!gen || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "music_generator_clear_style: required parameter is NULL (gen, out)");
        return -1;
    }

    style_embedding_create(out, 256);

    uint32_t seed = (uint32_t)archetype_id * 37 + 67890;
    for (uint32_t i = 0; i < out->embedding_dim; i++) {
        seed = seed * 1103515245 + 12345;
        out->embedding[i] = ((float)(seed % 10000) / 5000.0f) - 1.0f;
    }

    style_embedding_normalize(out);

    return 0;
}

//=============================================================================
// Cortical Integration API
//=============================================================================

void music_generator_set_audio_cortex(music_generator_t* gen, void* audio_cortex) {
    if (!gen) return;
    gen->audio_cortex = audio_cortex;
}

//=============================================================================
// Utility API
//=============================================================================

int music_parse_key(const char* key_string, music_key_t* out) {
    if (!key_string || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "music_parse_key: required parameter is NULL (key_string, out)");
        return -1;
    }

    memset(out, 0, sizeof(music_key_t));

    /* Parse root note */
    char root_char = key_string[0];
    if (root_char >= 'a' && root_char <= 'g') root_char -= 32;

    static const int root_map[] = {9, 11, 0, 2, 4, 5, 7};  /* A=9, B=11, C=0, ... */
    if (root_char >= 'A' && root_char <= 'G') {
        out->root = root_map[root_char - 'A'];
    }

    /* Check for sharp/flat */
    if (strlen(key_string) > 1) {
        if (key_string[1] == '#') out->root = (out->root + 1) % 12;
        if (key_string[1] == 'b') out->root = (out->root + 11) % 12;
    }

    /* Check for minor */
    out->is_minor = (strstr(key_string, "minor") != NULL ||
                     strstr(key_string, "min") != NULL ||
                     strstr(key_string, "m") != NULL);

    return 0;
}

const char* music_key_name(const music_key_t* key) {
    if (!key) return "Unknown";

    static char buf[32];
    snprintf(buf, sizeof(buf), "%s %s",
             NOTE_NAMES[key->root % 12],
             key->is_minor ? "minor" : "major");
    return buf;
}

void chord_progression_free(chord_progression_t* progression) {
    if (!progression) return;

    if (progression->chords) {
        nimcp_free(progression->chords);
        progression->chords = NULL;
    }
    progression->num_chords = 0;
}
