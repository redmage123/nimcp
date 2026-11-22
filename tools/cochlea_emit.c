/**
 * @file cochlea_emit.c
 * @brief nimcp's cochlea as a streaming front end: PCM in, auditory-nerve
 *        spike counts out. Nothing is written to disk.
 *
 * stdin  (repeated, one utterance each):  uint32 n_samples, float32[n] mono PCM
 *                                         at the cochlea's sample rate (16 kHz)
 * stdout (repeated, one utterance each):  uint32 n_frames,
 *                                         uint16[n_frames * NCH] spike counts:
 *                                         per frame of BIN samples (40 ms) and
 *                                         channel, the spikes across that
 *                                         channel's auditory-nerve fibres.
 * The cochlea is not reset between utterances (continuous listening). A
 * trailing part of an utterance shorter than one frame is carried into
 * the next utterance.
 *
 * Build: gcc -O2 -std=gnu11 -Iinclude tools/cochlea_emit.c -Lbuild/lib -lnimcp \
 *            -Wl,-rpath,$PWD/build/lib -lm -o build/cochlea_emit
 * Usage: python3 -m athena.data.librispeech --emit-pcm < file.tar.gz | build/cochlea_emit
 *        Optional args: sample_rate (16000) channels (64) bin_ms (40)
 */
#include "perception/nimcp_cochlea.h"
#include "perception/nimcp_auditory_nerve.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_full(void* buf, size_t n) {
    return fread(buf, 1, n, stdin) == n;
}

int main(int argc, char** argv) {
    uint32_t sr = argc > 1 ? (uint32_t)atoi(argv[1]) : 16000u;
    uint32_t nch = argc > 2 ? (uint32_t)atoi(argv[2]) : 64u;
    uint32_t bin_ms = argc > 3 ? (uint32_t)atoi(argv[3]) : 40u;
    uint32_t bin = sr * bin_ms / 1000u;
    if (sr == 0 || nch == 0 || bin == 0) {
        fprintf(stderr, "cochlea_emit: bad arguments\n");
        return 2;
    }

    cochlea_config_t cfg = cochlea_config_default(BM_MODE_HUMAN, sr);
    cfg.num_channels = nch;
    cfg.output_spikes = true;
    cfg.enable_bio_async = false;
    cfg.enable_brain_kg = false;
    cfg.enable_logging = false;
    cochlea_t* c = cochlea_create(&cfg);
    if (!c) { fprintf(stderr, "cochlea_emit: cochlea_create failed\n"); return 1; }
    cochlea_output_t* out = cochlea_output_create(c, bin);
    if (!out || !out->anf_output || !out->anf_output->spike_trains) {
        fprintf(stderr, "cochlea_emit: no spike-train output\n");
        return 1;
    }
    fprintf(stderr, "cochlea_emit: %u Hz, %u channels (CF %.0f-%.0f Hz), %u ms frames\n", sr, nch,
            cochlea_get_channel_freq(c, 0), cochlea_get_channel_freq(c, nch - 1), bin_ms);

    float* carry = (float*)calloc(bin, sizeof(float));
    uint32_t n_carry = 0;
    float* pcm = NULL;
    size_t pcm_cap = 0;
    uint16_t* frames = NULL;
    size_t frames_cap = 0;
    uint64_t utterances = 0, total_frames = 0;

    for (;;) {
        uint32_t n;
        if (!read_full(&n, sizeof(n))) break;
        size_t need = (size_t)n_carry + n;
        if (need > pcm_cap) {
            pcm_cap = need * 2;
            pcm = (float*)realloc(pcm, pcm_cap * sizeof(float));
            if (!pcm) { fprintf(stderr, "cochlea_emit: out of memory\n"); return 1; }
        }
        memcpy(pcm, carry, n_carry * sizeof(float));
        if (n > 0 && !read_full(pcm + n_carry, (size_t)n * sizeof(float))) break;

        uint32_t n_frames = (uint32_t)(need / bin);
        if ((size_t)n_frames * nch > frames_cap) {
            frames_cap = (size_t)n_frames * nch * 2 + nch;
            frames = (uint16_t*)realloc(frames, frames_cap * sizeof(uint16_t));
            if (!frames) { fprintf(stderr, "cochlea_emit: out of memory\n"); return 1; }
        }
        for (uint32_t f = 0; f < n_frames; f++) {
            if (cochlea_process(c, pcm + (size_t)f * bin, bin, out) != 0) {
                fprintf(stderr, "cochlea_emit: cochlea_process failed\n");
                return 1;
            }
            for (uint32_t ch = 0; ch < nch; ch++) {
                uint32_t s = 0;
                const uint8_t* tr = out->anf_output->spike_trains[ch];
                for (uint32_t k = 0; k < bin; k++) s += tr[k];
                frames[(size_t)f * nch + ch] = (uint16_t)(s > 65535u ? 65535u : s);
            }
        }
        n_carry = (uint32_t)(need - (size_t)n_frames * bin);
        memcpy(carry, pcm + (size_t)n_frames * bin, n_carry * sizeof(float));

        if (fwrite(&n_frames, sizeof(n_frames), 1, stdout) != 1) break;
        if (n_frames && fwrite(frames, sizeof(uint16_t), (size_t)n_frames * nch, stdout)
                        != (size_t)n_frames * nch) break;
        utterances++;
        total_frames += n_frames;
        if (utterances % 200 == 0) fflush(stdout);
    }
    fflush(stdout);
    fprintf(stderr, "cochlea_emit: %llu utterances, %llu frames (%.2f h)\n",
            (unsigned long long)utterances, (unsigned long long)total_frames,
            total_frames * bin_ms / 3.6e6);
    free(carry); free(pcm); free(frames);
    cochlea_output_destroy(out);
    cochlea_destroy(c);
    return 0;
}
