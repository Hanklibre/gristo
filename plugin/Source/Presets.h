#pragma once

#include "Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

class GristoProcessor;

// Patch presets: `.gristo` files (XML) holding everything musical (map, voices,
// groove, humanize, random amounts, lanes and routings, arps, sounds) but not the
// routing to the outside world (MIDI notes, channels, Out Shift, Sound on/level,
// Free Run), so a preset made on the tablet doesn't break the DAW setup.
// Message thread only.
class PresetManager
{
public:
    static constexpr const char* kExtension = ".gristo";

    explicit PresetManager(GristoProcessor& processor);

    struct Factory
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values; // param id -> plain value; everything else default
    };
    static const std::vector<Factory>& factoryPresets();

    static juce::File userFolder(); // created on demand
    juce::Array<juce::File> userPresets() const;

    void loadInit();
    void loadFactory(int index);
    bool loadFile(const juce::File& file);
    bool saveFile(const juce::File& file, const juce::String& name);
    juce::File importFile(const juce::File& source); // copies into the user folder; returns the copy
    // URL versions (also Android content:// links from the system file picker).
    juce::File importUrl(const juce::URL& source); // returns the stored copy, or {} if not a preset
    bool exportUrl(const juce::URL& target) const; // writes the current state under the current name
    juce::File writeTemporary() const;             // the current state as a temp file, for sharing

    // Steps through factory presets then user presets.
    void step(int direction);

    juce::String currentName() const
    {
        const juce::SpinLock::ScopedLockType lock(identityLock_);
        return name_;
    }
    juce::File currentFile() const // empty for factory / init
    {
        const juce::SpinLock::ScopedLockType lock(identityLock_);
        return file_;
    }
    bool isDirty() const;
    void markClean();
    // After exporting a copy: keep working on the previous preset identity.
    void restoreIdentity(const juce::String& name, const juce::File& file, bool dirty);

    // Persisted with the plugin state so the name survives reopening a project.
    void writeTo(juce::ValueTree& state) const;
    void readFrom(const juce::ValueTree& state);

    static bool isRoutingParameter(const juce::String& id);

private:
    juce::ValueTree capture(const juce::String& name) const;
    void apply(const juce::ValueTree& preset);
    std::int64_t stateHash() const;
    int currentIndex() const; // in the combined factory + user list, -1 if none
    void setIdentity(const juce::String& name, const juce::File& file, int factoryIndex);

    GristoProcessor& processor_;
    // The identity is read when a host saves state, possibly from another thread.
    mutable juce::SpinLock identityLock_;
    juce::String name_ { "Init" };
    juce::File file_;
    int factoryIndex_ = -1;
    std::atomic<std::int64_t> cleanHash_ { 0 }; // written when a host restores state, maybe off the UI thread
};
