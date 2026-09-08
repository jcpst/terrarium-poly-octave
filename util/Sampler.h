#pragma once

#include <algorithm>
#include <cstddef>

class Sampler
{
public:
    static constexpr size_t MAX_SAMPLES = 48000 * 4; // 4 seconds at 48kHz

    enum class State { Idle, Recording, Ready };

    Sampler(float* buffer, size_t capacity)
        : _buf(buffer), _capacity(capacity)
    {}

    void startRecording()
    {
        _record_pos = 0;
        _length = 0;
        _state = State::Recording;
    }

    void recordSample(float sample)
    {
        if (_state != State::Recording || _record_pos >= _capacity)
            return;
        _buf[_record_pos++] = sample;
        _length = _record_pos;
        if (_record_pos >= _capacity)
            stopRecording();
    }

    void stopRecording()
    {
        if (_state != State::Recording)
            return;
        _play_pos = 0.0f;
        _state = _length > 0 ? State::Ready : State::Idle;
    }

    void setRootPitch(float hz)
    {
        _root_pitch = hz > 0.0f ? hz : 440.0f;
    }

    void setLivePitch(float hz)
    {
        if (_root_pitch <= 0.0f || hz <= 0.0f)
            return;
        _rate = std::clamp(hz / _root_pitch, 0.25f, 4.0f);
    }

    float nextSample()
    {
        if (_state != State::Ready || _length == 0)
            return 0.0f;

        const size_t i0 = static_cast<size_t>(_play_pos);
        const float frac = _play_pos - static_cast<float>(i0);
        const size_t i1 = (i0 + 1 < _length) ? i0 + 1 : 0;
        const float out = _buf[i0] * (1.0f - frac) + _buf[i1] * frac;

        _play_pos += _rate;
        if (_play_pos >= static_cast<float>(_length))
            _play_pos -= static_cast<float>(_length);

        return out;
    }

    void reset()
    {
        _state = State::Idle;
        _length = 0;
        _record_pos = 0;
        _play_pos = 0.0f;
        _root_pitch = 0.0f;
        _rate = 1.0f;
    }

    bool isRecording()  const { return _state == State::Recording; }
    bool hasRecording() const { return _state == State::Ready; }

private:
    float*  _buf;
    size_t  _capacity;
    size_t  _length     = 0;
    size_t  _record_pos = 0;
    float   _play_pos   = 0.0f;
    float   _root_pitch = 0.0f;
    float   _rate       = 1.0f;
    State   _state      = State::Idle;
};
