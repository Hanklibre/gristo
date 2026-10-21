#include "Presets.h"

#include "PluginProcessor.h"

namespace p = gristo::params;

namespace {

constexpr int kFormatVersion = 1;
// Presets are a few kB; anything far bigger is the wrong file (a photo or video picked
// by mistake) and reading it whole on the message thread could stall or kill the app.
constexpr juce::int64 kMaxPresetBytes = 1 << 20;

// Each id is matched without its trailing index (note0..note5 -> note).
const char* const kRoutingBases[] = { "note", "chan", "arpChan", "arpShift", "chChan", "chShift", "kitOn", "kitGain", "freeRun", "freeTempo" };

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
    // Documents/Gristo/Presets: on Android that's the shared Documents folder,
    // reachable over USB. If the system refuses it, fall back to the app's own data.
    static const juce::File folder = [] {
        auto docs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Gristo").getChildFile("Presets");
        if (docs.createDirectory() && docs.hasWriteAccess())
            return docs;
        auto own = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Gristo").getChildFile("Presets");
        own.createDirectory();
        return own;
    }();
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
            if (const auto v = static_cast<float>(child["value"]); std::isfinite(v)) // "nan" in a file stays out
                values[child["id"].toString()] = v;

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
    setIdentity("Init", {}, -1);
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
    setIdentity(list[static_cast<size_t>(index)].name, {}, index);
    markClean();
}

bool PresetManager::loadFile(const juce::File& file)
{
    if (file.getSize() > kMaxPresetBytes)
        return false;
    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName("GristoPreset"))
        return false;
    const auto preset = juce::ValueTree::fromXml(*xml);
    apply(preset);
    setIdentity(preset.getProperty("name", file.getFileNameWithoutExtension()).toString(), file, -1);
    markClean();
    return true;
}

bool PresetManager::saveFile(const juce::File& file, const juce::String& name)
{
    const auto xml = capture(name).createXml();
    if (xml == nullptr || !xml->writeTo(file))
        return false;
    setIdentity(name, file, -1);
    markClean();
    return true;
}

juce::File PresetManager::importFile(const juce::File& source)
{
    if (source.isAChildOf(userFolder()))
        return source;
    if (source.getSize() > kMaxPresetBytes)
        return {};
    // Only real presets are copied in: a wrong file shouldn't land in My presets.
    const auto xml = juce::XmlDocument::parse(source);
    if (xml == nullptr || !xml->hasTagName("GristoPreset"))
        return {};
    auto target = userFolder().getChildFile(source.getFileName());
    if (target.existsAsFile())
        target = target.getNonexistentSibling(false);
    return source.copyFileTo(target) ? target : juce::File();
}

juce::File PresetManager::importUrl(const juce::URL& source)
{
    if (source.isLocalFile())
        return importFile(source.getLocalFile());
    std::unique_ptr<juce::InputStream> in(source.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)));
    if (in == nullptr)
        return {};
    const auto length = in->getTotalLength(); // -1 when unknown
    if (length > kMaxPresetBytes)
        return {};
    juce::MemoryBlock data;
    in->readIntoMemoryBlock(data, kMaxPresetBytes + 1);
    if (static_cast<juce::int64>(data.getSize()) > kMaxPresetBytes)
        return {};
    const auto text = data.toString();
    const auto xml = juce::parseXML(text);
    if (xml == nullptr || !xml->hasTagName("GristoPreset"))
        return {};
    auto name = juce::File::createLegalFileName(xml->getStringAttribute("name", "Imported"));
    auto target = userFolder().getChildFile(name + kExtension);
    if (target.existsAsFile())
        target = target.getNonexistentSibling(false);
    return target.replaceWithText(text) ? target : juce::File();
}

bool PresetManager::exportUrl(const juce::URL& target) const
{
    const auto xml = capture(currentName()).createXml();
    if (xml == nullptr)
        return false;
    if (target.isLocalFile())
        return xml->writeTo(target.getLocalFile());
    std::unique_ptr<juce::OutputStream> out(target.createOutputStream());
    if (out == nullptr)
        return false;
    out->writeText(xml->toString(), false, false, "\n");
    out->flush();
    return true;
}

juce::File PresetManager::writeTemporary() const
{
    auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("GristoShare");
    folder.createDirectory();
    const auto name = currentName();
    auto file = folder.getChildFile(juce::File::createLegalFileName(name) + kExtension);
    const auto xml = capture(name).createXml();
    return xml != nullptr && xml->writeTo(file) ? file : juce::File();
}

int PresetManager::currentIndex() const
{
    int factory = -1;
    juce::File file;
    {
        const juce::SpinLock::ScopedLockType lock(identityLock_);
        factory = factoryIndex_;
        file = file_;
    }
    if (factory >= 0)
        return factory;
    const auto files = userPresets();
    const int i = files.indexOf(file);
    return i >= 0 ? static_cast<int>(factoryPresets().size()) + i : -1;
}

void PresetManager::step(int direction)
{
    const int factoryCount = static_cast<int>(factoryPresets().size());
    const auto files = userPresets();
    const int total = factoryCount + files.size();
    if (total == 0)
        return;
    // Skip files that don't load (damaged, or not a preset) instead of getting stuck on them.
    int index = currentIndex();
    for (int tries = 0; tries < total; ++tries)
    {
        index = index < 0 ? (direction > 0 ? 0 : total - 1) : (index + direction + total) % total;
        if (index < factoryCount)
        {
            loadFactory(index);
            return;
        }
        if (loadFile(files[index - factoryCount]))
            return;
    }
}

std::int64_t PresetManager::stateHash() const
{
    std::uint64_t hash = 1469598103934665603ULL; // FNV-1a, unsigned so it may wrap
    auto mix = [&hash](float v) {
        hash ^= static_cast<std::uint64_t>(std::llround(static_cast<double>(v) * 10000.0));
        hash *= 1099511628211ULL;
    };
    for (auto* param : processor_.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            if (!isRoutingParameter(ranged->getParameterID()))
                mix(ranged->getValue());
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
        for (int k = 0; k < gristo::kMaxModPoints; ++k)
            mix(p::lanePoint(processor_.modPoints, lane, k).load());
    return static_cast<std::int64_t>(hash);
}

void PresetManager::setIdentity(const juce::String& name, const juce::File& file, int factoryIndex)
{
    const juce::SpinLock::ScopedLockType lock(identityLock_);
    name_ = name;
    file_ = file;
    factoryIndex_ = factoryIndex;
}

bool PresetManager::isDirty() const { return stateHash() != cleanHash_; }
void PresetManager::markClean() { cleanHash_ = stateHash(); }

void PresetManager::restoreIdentity(const juce::String& name, const juce::File& file, bool dirty)
{
    setIdentity(name, file, -1);
    if (dirty)
        cleanHash_ = 0; // still shows as changed
}

void PresetManager::writeTo(juce::ValueTree& state) const
{
    const juce::SpinLock::ScopedLockType lock(identityLock_);
    state.setProperty("presetName", name_, nullptr);
    state.setProperty("presetFile", file_.getFullPathName(), nullptr);
    state.setProperty("presetFactory", factoryIndex_, nullptr);
}

void PresetManager::readFrom(const juce::ValueTree& state)
{
    const auto name = state.getProperty("presetName", "Init").toString();
    const auto path = state.getProperty("presetFile", "").toString();
    setIdentity(name, path.isNotEmpty() && juce::File::isAbsolutePath(path) ? juce::File(path) : juce::File(),
                static_cast<int>(state.getProperty("presetFactory", -1)));
    // The restored project is the reference: unchanged until the user edits it.
    markClean();
}
