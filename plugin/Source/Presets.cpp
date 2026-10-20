#include "Presets.h"

#include "PluginProcessor.h"

namespace p = gristo::params;

namespace {

constexpr int kFormatVersion = 1;

// Each id is matched without its trailing index (note0..note5 -> note).
const char* const kRoutingBases[] = { "note", "chan", "arpChan", "arpShift", "kitOn", "kitGain", "freeRun" };

juce::String withoutIndex(const juce::String& id)
{
    return id.trimCharactersAtEnd("0123456789_").trimCharactersAtEnd("0123456789");
}

} // namespace

PresetManager::PresetManager(GristoProcessor& processor) : processor_(processor) {}

bool PresetManager::isRoutingParameter(const juce::String& id)
{
    const auto base = withoutIndex(id);
    for (const char* r : kRoutingBases)
        if (base == r)
            return true;
    return false;
}

juce::File PresetManager::userFolder()
{
    auto folder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Gristo").getChildFile("Presets");
    folder.createDirectory();
    return folder;
}

juce::Array<juce::File> PresetManager::userPresets() const
{
    auto files = userFolder().findChildFiles(juce::File::findFiles, false, juce::String("*") + kExtension);
    struct ByName
    {
        static int compareElements(const juce::File& a, const juce::File& b)
        {
            return a.getFileNameWithoutExtension().compareNatural(b.getFileNameWithoutExtension());
        }
    } sorter;
    files.sort(sorter);
    return files;
}

juce::ValueTree PresetManager::capture(const juce::String& name) const
{
    juce::ValueTree preset("GristoPreset");
    preset.setProperty("version", kFormatVersion, nullptr);
    preset.setProperty("name", name, nullptr);
    for (auto* param : processor_.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
        {
            const auto id = ranged->getParameterID();
            if (isRoutingParameter(id))
                continue;
            juce::ValueTree child("PARAM");
            child.setProperty("id", id, nullptr);
            child.setProperty("value", ranged->convertFrom0to1(ranged->getValue()), nullptr);
            preset.appendChild(child, nullptr);
        }
    juce::ValueTree lanes("MODLANES");
    processor_.writeLanes(lanes);
    preset.appendChild(lanes, nullptr);
    return preset;
}

void PresetManager::apply(const juce::ValueTree& preset)
{
    std::map<juce::String, float> values;
    for (const auto& child : preset)
        if (child.hasType("PARAM"))
            values[child["id"].toString()] = static_cast<float>(child["value"]);

    // Everything musical not in the preset goes back to its default, so a preset
    // always sounds the same no matter what was loaded before.
    for (auto* param : processor_.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
        {
            const auto id = ranged->getParameterID();
            if (isRoutingParameter(id))
                continue;
            const auto it = values.find(id);
            const float normalized = it != values.end() ? ranged->convertTo0to1(it->second) : ranged->getDefaultValue();
            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost(normalized);
            ranged->endChangeGesture();
        }

    const auto lanes = preset.getChildWithName("MODLANES");
    if (lanes.isValid())
        processor_.readLanes(lanes);
    else
        processor_.resetLanes();
}

void PresetManager::loadInit()
{
    apply(juce::ValueTree("GristoPreset"));
    name_ = "Init";
    file_ = juce::File();
    factoryIndex_ = -1;
    markClean();
}

void PresetManager::loadFactory(int index)
{
    const auto& list = factoryPresets();
    if (index < 0 || index >= static_cast<int>(list.size()))
        return;
    juce::ValueTree preset("GristoPreset");
    for (const auto& [id, value] : list[static_cast<size_t>(index)].values)
    {
        juce::ValueTree child("PARAM");
        child.setProperty("id", id, nullptr);
        child.setProperty("value", value, nullptr);
        preset.appendChild(child, nullptr);
    }
    apply(preset);
    name_ = list[static_cast<size_t>(index)].name;
    file_ = juce::File();
    factoryIndex_ = index;
    markClean();
}

bool PresetManager::loadFile(const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName("GristoPreset"))
        return false;
    const auto preset = juce::ValueTree::fromXml(*xml);
    apply(preset);
    name_ = preset.getProperty("name", file.getFileNameWithoutExtension()).toString();
    file_ = file;
    factoryIndex_ = -1;
    markClean();
    return true;
}

bool PresetManager::saveFile(const juce::File& file, const juce::String& name)
{
    const auto xml = capture(name).createXml();
    if (xml == nullptr || !xml->writeTo(file))
        return false;
    name_ = name;
    file_ = file;
    factoryIndex_ = -1;
    markClean();
    return true;
}

juce::File PresetManager::importFile(const juce::File& source)
{
    if (source.isAChildOf(userFolder()))
        return source;
    auto target = userFolder().getChildFile(source.getFileName());
    if (target.existsAsFile())
        target = target.getNonexistentSibling(false);
    return source.copyFileTo(target) ? target : juce::File();
}

int PresetManager::currentIndex() const
{
    if (factoryIndex_ >= 0)
        return factoryIndex_;
    const auto files = userPresets();
    const int i = files.indexOf(file_);
    return i >= 0 ? static_cast<int>(factoryPresets().size()) + i : -1;
}

void PresetManager::step(int direction)
{
    const int factoryCount = static_cast<int>(factoryPresets().size());
    const auto files = userPresets();
    const int total = factoryCount + files.size();
    if (total == 0)
        return;
    const int current = currentIndex();
    const int next = current < 0 ? (direction > 0 ? 0 : total - 1) : (current + direction + total) % total;
    if (next < factoryCount)
        loadFactory(next);
    else
        loadFile(files[next - factoryCount]);
}

std::int64_t PresetManager::stateHash() const
{
    std::int64_t hash = 1469598103934665603LL;
    auto mix = [&hash](float v) {
        hash ^= static_cast<std::int64_t>(std::llround(static_cast<double>(v) * 10000.0));
        hash *= 1099511628211LL;
    };
    for (auto* param : processor_.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            if (!isRoutingParameter(ranged->getParameterID()))
                mix(ranged->getValue());
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
        for (int k = 0; k < gristo::kMaxModPoints; ++k)
            mix(p::lanePoint(processor_.modPoints, lane, k).load());
    return hash;
}

bool PresetManager::isDirty() const { return stateHash() != cleanHash_; }
void PresetManager::markClean() { cleanHash_ = stateHash(); }

void PresetManager::restoreIdentity(const juce::String& name, const juce::File& file, bool dirty)
{
    name_ = name;
    file_ = file;
    if (dirty)
        cleanHash_ = 0; // still shows as changed
}

void PresetManager::writeTo(juce::ValueTree& state) const
{
    state.setProperty("presetName", name_, nullptr);
    state.setProperty("presetFile", file_.getFullPathName(), nullptr);
    state.setProperty("presetFactory", factoryIndex_, nullptr);
}

void PresetManager::readFrom(const juce::ValueTree& state)
{
    name_ = state.getProperty("presetName", "Init").toString();
    const auto path = state.getProperty("presetFile", "").toString();
    file_ = path.isNotEmpty() && juce::File::isAbsolutePath(path) ? juce::File(path) : juce::File();
    factoryIndex_ = static_cast<int>(state.getProperty("presetFactory", -1));
    // The restored project is the reference: unchanged until the user edits it.
    markClean();
}
