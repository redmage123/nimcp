/**
 * @file test_cochlea_responsiveness.cpp
 * @brief Regression: the cochlea must respond to sound.
 *
 * WHAT: Drives the full cochlea (basilar membrane -> hair cells -> auditory
 *       nerve) with silence and pure tones and checks the auditory-nerve
 *       spike output.
 * WHY:  Until 229c995bd the basilar-membrane and hair-cell stages were stubs:
 *       the nerve fired at its spontaneous rate whatever the sound, and
 *       spike_trains was never allocated. Existing tests passed throughout
 *       because none checked that the output depends on the input.
 * HOW:  16 kHz, 64 channels, 40 ms blocks (the configuration used by
 *       tools/cochlea_emit), 1 s of each stimulus after cochlea_reset.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "perception/nimcp_cochlea.h"
#include "perception/nimcp_auditory_nerve.h"

namespace {

const uint32_t kSampleRate = 16000;
const uint32_t kChannels = 64;
const uint32_t kBlock = 640;          // 40 ms
const uint32_t kSeconds = 1;

class CochleaResponsivenessTest : public ::testing::Test {
protected:
    cochlea_t* cochlea = nullptr;
    cochlea_output_t* output = nullptr;

    void SetUp() override {
        cochlea_config_t cfg = cochlea_config_default(BM_MODE_HUMAN, kSampleRate);
        cfg.num_channels = kChannels;
        cfg.output_spikes = true;
        cfg.enable_bio_async = false;
        cfg.enable_brain_kg = false;
        cfg.enable_logging = false;
        cochlea = cochlea_create(&cfg);
        ASSERT_NE(cochlea, nullptr);
        output = cochlea_output_create(cochlea, kBlock);
        ASSERT_NE(output, nullptr);
    }

    void TearDown() override {
        if (output) cochlea_output_destroy(output);
        if (cochlea) cochlea_destroy(cochlea);
    }

    /** Mean spikes per 40 ms block, per channel, for 1 s of signal. */
    std::vector<double> spikes_per_block(const std::vector<float>& x) {
        std::vector<double> acc(kChannels, 0.0);
        EXPECT_EQ(cochlea_reset(cochlea), NIMCP_SUCCESS);
        uint32_t blocks = 0;
        for (size_t off = 0; off + kBlock <= x.size(); off += kBlock) {
            EXPECT_EQ(cochlea_process(cochlea, x.data() + off, kBlock, output), NIMCP_SUCCESS);
            for (uint32_t ch = 0; ch < kChannels; ch++) {
                const uint8_t* tr = output->anf_output->spike_trains[ch];
                for (uint32_t k = 0; k < kBlock; k++) acc[ch] += tr[k];
            }
            blocks++;
        }
        for (double& a : acc) a /= blocks;
        return acc;
    }

    static std::vector<float> tone(float hz, float amplitude) {
        std::vector<float> x(kSampleRate * kSeconds);
        for (size_t k = 0; k < x.size(); k++)
            x[k] = amplitude * sinf(2.0f * (float)M_PI * hz * k / kSampleRate);
        return x;
    }

    static double total(const std::vector<double>& v) {
        double t = 0;
        for (double a : v) t += a;
        return t;
    }

    static uint32_t peak_channel(const std::vector<double>& v) {
        uint32_t best = 0;
        for (uint32_t ch = 1; ch < v.size(); ch++)
            if (v[ch] > v[best]) best = ch;
        return best;
    }

    /** Channel whose centre frequency is nearest hz. */
    uint32_t nearest_channel(float hz) {
        uint32_t best = 0;
        for (uint32_t ch = 1; ch < kChannels; ch++)
            if (fabsf(cochlea_get_channel_freq(cochlea, ch) - hz) <
                fabsf(cochlea_get_channel_freq(cochlea, best) - hz))
                best = ch;
        return best;
    }
};

TEST_F(CochleaResponsivenessTest, SpikeTrainsAreAllocated) {
    ASSERT_NE(output->anf_output, nullptr);
    ASSERT_NE(output->anf_output->spike_trains, nullptr);
    ASSERT_NE(output->anf_output->firing_rate, nullptr);
    for (uint32_t ch = 0; ch < kChannels; ch++)
        EXPECT_NE(output->anf_output->spike_trains[ch], nullptr) << "channel " << ch;
}

TEST_F(CochleaResponsivenessTest, SilenceGivesUniformSpontaneousFiring) {
    std::vector<double> s = spikes_per_block(std::vector<float>(kSampleRate * kSeconds, 0.0f));
    double mean = total(s) / kChannels;
    EXPECT_GT(mean, 0.0) << "no spontaneous activity";
    for (uint32_t ch = 0; ch < kChannels; ch++)
        EXPECT_NEAR(s[ch], mean, 0.35 * mean) << "channel " << ch << " is not at the spontaneous rate";
}

TEST_F(CochleaResponsivenessTest, TonesPeakAtTheirCharacteristicFrequency) {
    for (float hz : {250.0f, 1000.0f, 4000.0f}) {
        std::vector<double> s = spikes_per_block(tone(hz, 0.01f));
        int peak = (int)peak_channel(s);
        int want = (int)nearest_channel(hz);
        EXPECT_LE(std::abs(peak - want), 1)
            << hz << " Hz peaked at channel " << peak << " (CF "
            << cochlea_get_channel_freq(cochlea, (uint32_t)peak) << " Hz), expected channel "
            << want << " (CF " << cochlea_get_channel_freq(cochlea, (uint32_t)want) << " Hz)";
    }
}

TEST_F(CochleaResponsivenessTest, SoundRaisesFiringAboveSilence) {
    double silence = total(spikes_per_block(std::vector<float>(kSampleRate * kSeconds, 0.0f)));
    double sound = total(spikes_per_block(tone(1000.0f, 0.01f)));
    EXPECT_GT(sound, 1.5 * silence) << "a 1 kHz tone barely changes nerve output";
}

TEST_F(CochleaResponsivenessTest, LouderToneGivesMoreSpikes) {
    double quiet = total(spikes_per_block(tone(1000.0f, 0.01f)));
    double loud = total(spikes_per_block(tone(1000.0f, 0.1f)));   // +20 dB
    EXPECT_GT(loud, 1.3 * quiet) << "+20 dB did not raise firing";
}

}  // namespace
