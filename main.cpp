#include <cassert>

#include <q/support/literals.hpp>
#include <q/fx/biquad.hpp>

#include <util/EffectState.h>
#include <util/Multirate.h>
#include <util/OctaveGenerator.h>
#include <util/PitchTracker.h>
#include <util/Sampler.h>
#include <util/Terrarium.h>

namespace q = cycfi::q;
using namespace q::literals;

Terrarium terrarium;
EffectState interface_state;
bool enable_effect = false;

static float DSY_SDRAM_BSS g_sample_buffer[Sampler::MAX_SAMPLES];
static Sampler sampler(g_sample_buffer, Sampler::MAX_SAMPLES);
static float g_detected_pitch = 0.0f;

//=============================================================================
void processAudioBlock(
    daisy::AudioHandle::InputBuffer in,
    daisy::AudioHandle::OutputBuffer out,
    size_t size)
{
    static const auto sample_rate = terrarium.seed.AudioSampleRate();

    static Decimator decimate;
    static Interpolator interpolate;
    static OctaveGenerator octave(sample_rate / resample_factor);
    static q::highshelf eq1(-11, 140_Hz, sample_rate);
    static q::lowshelf eq2(5, 160_Hz, sample_rate);
    static PitchTracker pitch_tracker(sample_rate / resample_factor);

    const auto& s = interface_state;

    for (size_t i = 0; i <= (size - resample_factor); i += resample_factor)
    {
        std::span<const float, resample_factor> in_chunk(
            &(in[0][i]), resample_factor);
        const auto sample = decimate(in_chunk);

        if (pitch_tracker.update(sample))
        {
            g_detected_pitch = pitch_tracker.frequency();
            sampler.setLivePitch(g_detected_pitch);
        }

        float octave_mix = 0;
        octave.update(sample);
        octave_mix += s.up1Level() * octave.up1();
        octave_mix += s.down1Level() * octave.down1();
        octave_mix += s.down2Level() * octave.down2();

        auto out_chunk = interpolate(octave_mix);
        for (size_t j = 0; j < out_chunk.size(); ++j)
        {
            const auto dry_signal = in[0][i+j];
            float mix = eq2(eq1(out_chunk[j]));

            if (sampler.isRecording())
                sampler.recordSample(dry_signal);

            if (sampler.hasRecording())
                mix += sampler.nextSample();

            mix += s.dryLevel() * dry_signal;

            out[0][i+j] = enable_effect ? mix : dry_signal;
            out[1][i+j] = 0;
        }
    }
}

//=============================================================================
int main()
{
    terrarium.Init(true);
    assert(terrarium.seed.AudioSampleRate() == 48000);
    assert(terrarium.seed.AudioBlockSize() % resample_factor == 0);

    auto& knob_dry   = terrarium.knobs[0];
    auto& knob_down2 = terrarium.knobs[3];
    auto& knob_down1 = terrarium.knobs[4];
    auto& knob_up1   = terrarium.knobs[5];

    auto& stomp_bypass = terrarium.stomps[0];
    auto& stomp_sample = terrarium.stomps[1];

    auto& led_enable = terrarium.leds[0];
    auto& led_sample = terrarium.leds[1];

    terrarium.seed.StartAudio(processAudioBlock);

    int blink = 0;
    terrarium.Loop(100, [&](){
        interface_state.setDryRatio(knob_dry.Process());
        interface_state.setUp1Ratio(knob_up1.Process());
        interface_state.setDown1Ratio(knob_down1.Process());
        interface_state.setDown2Ratio(knob_down2.Process());

        if (stomp_bypass.RisingEdge())
            enable_effect = !enable_effect;

        if (stomp_sample.RisingEdge())
        {
            if (!sampler.hasRecording() && !sampler.isRecording())
            {
                sampler.startRecording();
            }
            else if (sampler.isRecording())
            {
                sampler.stopRecording();
                sampler.setRootPitch(g_detected_pitch);
            }
            else
            {
                sampler.reset();
            }
        }

        led_enable.Set(enable_effect ? 1.0f : 0.0f);

        if (sampler.isRecording())
            led_sample.Set((++blink / 5) % 2 ? 1.0f : 0.0f);
        else
            led_sample.Set(sampler.hasRecording() ? 1.0f : 0.0f);
    });
}
