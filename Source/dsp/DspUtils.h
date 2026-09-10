#pragma once

#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace dsp
{
    inline constexpr float kPi = 3.14159265358979323846f;

    // Tiny xorshift PRNG: allocation-free and cheap enough to call per grain.
    struct FastRandom
    {
        uint32_t state = 0x9E3779B9u;

        void seed (uint32_t s) { state = s != 0 ? s : 0x9E3779B9u; }

        uint32_t next()
        {
            uint32_t x = state;
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            return state = x;
        }

        float unipolar() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // [0, 1)
        float bipolar()  { return unipolar() * 2.0f - 1.0f; }                        // [-1, 1)
        int   below (int n) { return (int) (next() % (uint32_t) std::max (1, n)); }
    };

    // 4-point Hermite interpolation. Reads from a circular buffer.
    inline float hermite (float xm1, float x0, float x1, float x2, float t)
    {
        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float w = c + v;
        const float a = w + v + (x2 - x0) * 0.5f;
        const float b = w + a;
        return ((((a * t) - b) * t + c) * t + x0);
    }

    // Simple circular buffer with fractional, interpolated reads.
    class CircularBuffer
    {
    public:
        void resize (int newLength)
        {
            length = std::max (16, newLength);
            data.assign ((size_t) length, 0.0f);
        }

        void clear() { std::fill (data.begin(), data.end(), 0.0f); }

        int size() const { return length; }

        float& operator[] (int i) { return data[(size_t) i]; }
        float  operator[] (int i) const { return data[(size_t) i]; }

        int wrap (int i) const
        {
            if (i >= length) i -= length;
            if (i < 0)       i += length;
            return i;
        }

        // Read at an absolute fractional position (already inside [0, length)).
        float readCubic (double pos) const
        {
            const int i0 = (int) pos;
            const float t = (float) (pos - (double) i0);
            const int im1 = i0 == 0 ? length - 1 : i0 - 1;
            const int i1  = i0 + 1 >= length ? i0 + 1 - length : i0 + 1;
            const int i2  = i1 + 1 >= length ? i1 + 1 - length : i1 + 1;
            return hermite (data[(size_t) im1], data[(size_t) i0], data[(size_t) i1], data[(size_t) i2], t);
        }

        // Read "delay" samples behind a write index.
        float readDelayed (int writeIndex, double delaySamples) const
        {
            double pos = (double) writeIndex - delaySamples;
            while (pos < 0.0)               pos += (double) length;
            while (pos >= (double) length)  pos -= (double) length;
            return readCubic (pos);
        }

    private:
        std::vector<float> data;
        int length = 16;
    };

    // One-pole lowpass. g computed from a cutoff.
    struct OnePole
    {
        float z = 0.0f, g = 1.0f;

        void setCutoff (float hz, double sampleRate)
        {
            const float f = std::clamp (hz, 1.0f, (float) sampleRate * 0.45f);
            g = 1.0f - std::exp (-2.0f * kPi * f / (float) sampleRate);
        }

        float lowpass (float x)  { z += g * (x - z); return z; }
        float highpass (float x) { return x - lowpass (x); }
        void reset() { z = 0.0f; }
    };

    inline float softClip (float x) { return std::tanh (x); }

    inline float semitonesToRatio (float st) { return std::exp2 (st / 12.0f); }

    // Equal-power balance. pan in [-1, 1]; 0 = unity on both channels.
    inline void panGains (float pan, float& gl, float& gr)
    {
        const float a = (pan + 1.0f) * (kPi * 0.25f);
        const float s = 1.41421356f;
        gl = std::cos (a) * s;
        gr = std::sin (a) * s;
    }
}
