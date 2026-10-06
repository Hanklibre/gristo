#include "gristo/DrumMap.h"

#include <algorithm>
#include <cassert>

namespace gristo {

const char* voiceName(int voice)
{
    static const char* const names[kNumVoices] = { "Kick", "Snare", "Hihat 1", "Hihat 2", "Perc 1", "Perc 2" };
    return (voice >= 0 && voice < kNumVoices) ? names[voice] : "?";
}

DrumMap::DrumMap(const DrumMapData& data) : data_(data)
{
    assert(data_.width >= 2 && data_.height >= 2 && data_.levels != nullptr);
}

std::uint8_t DrumMap::nodeLevel(int nodeX, int nodeY, int voice, int patternStep) const
{
    const int node = nodeY * data_.width + nodeX;
    return data_.levels[(node * kNumVoices + voice) * kPatternSteps + patternStep];
}

const char* DrumMap::nodeName(int nodeX, int nodeY) const
{
    if (data_.nodeNames == nullptr) return "";
    return data_.nodeNames[nodeY * data_.width + nodeX];
}

float DrumMap::level(int voice, int patternStep, float x, float y) const
{
    const float fx = std::clamp(x, 0.0f, 1.0f) * static_cast<float>(data_.width - 1);
    const float fy = std::clamp(y, 0.0f, 1.0f) * static_cast<float>(data_.height - 1);
    const int ix = std::min(static_cast<int>(fx), data_.width - 2);
    const int iy = std::min(static_cast<int>(fy), data_.height - 2);
    const float tx = fx - static_cast<float>(ix);
    const float ty = fy - static_cast<float>(iy);

    const float a = nodeLevel(ix, iy, voice, patternStep);
    const float b = nodeLevel(ix + 1, iy, voice, patternStep);
    const float c = nodeLevel(ix, iy + 1, voice, patternStep);
    const float d = nodeLevel(ix + 1, iy + 1, voice, patternStep);
    const float top = a + (b - a) * tx;
    const float bottom = c + (d - c) * tx;
    return (top + (bottom - top) * ty) * (1.0f / 255.0f);
}

} // namespace gristo
