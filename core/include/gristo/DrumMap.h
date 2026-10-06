#pragma once

#include "gristo/Types.h"

namespace gristo {

// Raw map data as emitted by tools/mapbuilder/build_map.py.
struct DrumMapData
{
    int width = 0;
    int height = 0;
    const std::uint8_t* levels = nullptr;  // [node = y * width + x][voice][step]
    const char* const* nodeNames = nullptr; // [node], may be empty strings
};

// The map compiled into the plugin (core/src/MapData.cpp).
const DrumMapData& defaultMapData();

// A 2D field of drum patterns. Any (x, y) in [0, 1]^2 yields a pattern by bilinear
// interpolation of the four surrounding nodes.
class DrumMap
{
public:
    explicit DrumMap(const DrumMapData& data = defaultMapData());

    int width() const { return data_.width; }
    int height() const { return data_.height; }

    // Interpolated level in [0, 1] for a voice and pattern step (0..31).
    float level(int voice, int patternStep, float x, float y) const;

    std::uint8_t nodeLevel(int nodeX, int nodeY, int voice, int patternStep) const;
    const char* nodeName(int nodeX, int nodeY) const;

private:
    DrumMapData data_;
};

} // namespace gristo
