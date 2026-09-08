#pragma once

#include <q/pitch/pitch_detector.hpp>
#include <q/support/literals.hpp>

namespace q = cycfi::q;
using namespace q::literals;

class PitchTracker
{
public:
    PitchTracker(float sample_rate)
        : _pd{82.41_Hz, 1318.51_Hz, sample_rate, -40_dB}
    {}

    // Returns true when a new pitch is available.
    bool update(float sample)
    {
        return _pd(sample);
    }

    float frequency() const
    {
        return _pd.get_frequency();
    }

private:
    q::pitch_detector _pd;
};
