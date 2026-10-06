#pragma once

#include <cmath>
#include <cstdint>

namespace gristo {

// Stateless, deterministic randomness. Every random decision in Gristo is a hash of
// (seed, stream, position), so a render is identical regardless of block size, and an
// offline bounce matches realtime playback.
namespace rnd {

inline std::uint32_t mix(std::uint32_t h)
{
    h ^= h >> 16;
    h *= 0x7feb352dU;
    h ^= h >> 15;
    h *= 0x846ca68bU;
    h ^= h >> 16;
    return h;
}

inline std::uint32_t combine(std::uint32_t seed, std::uint32_t v)
{
    return mix(seed ^ (v + 0x9e3779b9U + (seed << 6) + (seed >> 2)));
}

inline std::uint32_t hash(std::uint32_t seed, std::uint32_t stream, std::int64_t a, std::int64_t b = 0)
{
    std::uint32_t h = combine(seed, stream);
    h = combine(h, static_cast<std::uint32_t>(a));
    h = combine(h, static_cast<std::uint32_t>(static_cast<std::uint64_t>(a) >> 32));
    h = combine(h, static_cast<std::uint32_t>(b));
    return h;
}

// Uniform in [0, 1).
inline float unit(std::uint32_t h) { return static_cast<float>(h >> 8) * (1.0f / 16777216.0f); }

// Uniform in [-1, 1).
inline float bipolar(std::uint32_t h) { return unit(h) * 2.0f - 1.0f; }

// Triangular distribution in (-1, 1): more small deviations than large ones, which
// sounds more like a player than uniform noise.
inline float triangular(std::uint32_t seed, std::uint32_t stream, std::int64_t a, std::int64_t b = 0)
{
    return unit(hash(seed, stream, a, b)) + unit(hash(seed, stream + 0x51u, a, b)) - 1.0f;
}

// Smooth value noise in [-1, 1] over continuous position x (lattice spacing 1).
// If period > 0 the noise repeats every `period` lattice units.
inline float smoothNoise(std::uint32_t seed, std::uint32_t stream, double x, int period = 0)
{
    const double fl = std::floor(x);
    std::int64_t i0 = static_cast<std::int64_t>(fl);
    std::int64_t i1 = i0 + 1;
    if (period > 0)
    {
        i0 = ((i0 % period) + period) % period;
        i1 = ((i1 % period) + period) % period;
    }
    const float t = static_cast<float>(x - fl);
    const float s = t * t * (3.0f - 2.0f * t);
    const float a = bipolar(hash(seed, stream, i0));
    const float b = bipolar(hash(seed, stream, i1));
    return a + (b - a) * s;
}

} // namespace rnd
} // namespace gristo
