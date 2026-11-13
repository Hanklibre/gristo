#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>

#include <array>
#include <atomic>
#include <map>

// MIDI controllers: any CC or note (per MIDI channel) can be bound to a parameter or
// an action. The audio thread only looks a message up and queues it; bindings are made
// and applied on the message thread.
struct ControlTarget
{
    enum Kind { None = 0, Param, Toggle, Set, SetXY, Action };
    enum ActionId { Scene = 0, Mutate, Catch, Undo, Redo };
    int kind = None;
    juce::String paramA, paramB; // parameter ids
    float valueA = 0.0f, valueB = 0.0f; // Set / SetXY: plain values
    int action = 0, arg = 0;

    static ControlTarget param(const juce::String& id) { ControlTarget t; t.kind = Param; t.paramA = id; return t; }
    static ControlTarget toggle(const juce::String& id) { ControlTarget t; t.kind = Toggle; t.paramA = id; return t; }
    static ControlTarget set(const juce::String& id, float value) { ControlTarget t; t.kind = Set; t.paramA = id; t.valueA = value; return t; }
    static ControlTarget setXY(const juce::String& a, float va, const juce::String& b, float vb)
    {
        ControlTarget t; t.kind = SetXY; t.paramA = a; t.valueA = va; t.paramB = b; t.valueB = vb; return t;
    }
    static ControlTarget act(int action, int arg = 0) { ControlTarget t; t.kind = Action; t.action = action; t.arg = arg; return t; }
    // The same control on screen (values aside for Param / Toggle).
    bool sameControl(const ControlTarget& o) const
    {
        if (kind == Action || o.kind == Action)
            return kind == o.kind && action == o.action && arg == o.arg;
        const bool plain = (kind == Param || kind == Toggle) && (o.kind == Param || o.kind == Toggle);
        return paramA == o.paramA && (plain || (kind == o.kind && valueA == o.valueA && paramB == o.paramB && valueB == o.valueB));
    }
};

class ControlMap
{
public:
    static constexpr int kKeys = 16 * 2 * 128;
    static int key(int channel, bool note, int number) { return ((juce::jlimit(1, 16, channel) - 1) * 2 + (note ? 1 : 0)) * 128 + (number & 127); }
    static juce::String describe(int key)
    {
        return juce::String((key / 128) % 2 == 1 ? "note " : "CC ") + juce::String(key % 128) + ", channel " + juce::String(key / 256 + 1);
    }

    ControlMap()
    {
        for (auto& b : bound_)
            b.store(false);
    }

    // --- Audio thread. True: the message belongs to a controller, not to the arps.
    bool handle(const juce::MidiMessage& m)
    {
        const bool cc = m.isController(), on = m.isNoteOn(), off = m.isNoteOff();
        if (!cc && !on && !off)
            return false;
        const int k = key(m.getChannel(), !cc, cc ? m.getControllerNumber() : m.getNoteNumber());
        if (learning_.load(std::memory_order_relaxed) && (cc || on))
        {
            learning_.store(false);
            push({ k, 0.0f, true });
            return true;
        }
        if (!bound_[static_cast<size_t>(k)].load(std::memory_order_relaxed))
            return false;
        push({ k, cc ? static_cast<float>(m.getControllerValue()) / 127.0f : (on ? 1.0f : 0.0f), false });
        return true;
    }

    // --- Message thread.
    void bind(int key, const ControlTarget& target)
    {
        for (auto it = map_.begin(); it != map_.end();) // one controller per on-screen control
            if (it->second.sameControl(target))
            {
                bound_[static_cast<size_t>(it->first)].store(false);
                it = map_.erase(it);
            }
            else
                ++it;
        map_[key] = target;
        bound_[static_cast<size_t>(key)].store(true);
    }
    void unbind(int key)
    {
        map_.erase(key);
        bound_[static_cast<size_t>(key)].store(false);
    }
    void clearChannel(int channel) // 0 = everything
    {
        for (auto it = map_.begin(); it != map_.end();)
            if (channel == 0 || it->first / 256 + 1 == channel)
            {
                bound_[static_cast<size_t>(it->first)].store(false);
                it = map_.erase(it);
            }
            else
                ++it;
    }
    int keyFor(const ControlTarget& target) const // -1: not bound
    {
        for (const auto& [k, t] : map_)
            if (t.sameControl(target))
                return k;
        return -1;
    }
    const std::map<int, ControlTarget>& bindings() const { return map_; }

    void startLearn(const ControlTarget& target)
    {
        learnTarget_ = target;
        learning_.store(true);
    }
    void cancelLearn() { learning_.store(false); }
    bool learning() const { return learning_.load(); }
    bool learning(const ControlTarget& target) const { return learning() && learnTarget_.sameControl(target); }

    // Calls apply(target, value 0..1, fromNote) for every queued message; learns.
    template <typename Apply>
    void drain(Apply&& apply)
    {
        int start1, size1, start2, size2;
        fifo_.prepareToRead(fifo_.getNumReady(), start1, size1, start2, size2);
        auto each = [&](int start, int size) {
            for (int i = start; i < start + size; ++i)
            {
                const Event e = events_[static_cast<size_t>(i)];
                if (e.learned)
                {
                    ControlTarget t = learnTarget_;
                    const bool note = (e.key / 128) % 2 == 1;
                    if (t.kind == ControlTarget::Param && note && learnAsToggle)
                        t.kind = ControlTarget::Toggle;
                    bind(e.key, t);
                    continue;
                }
                const auto it = map_.find(e.key);
                if (it != map_.end())
                    apply(it->second, e.value, (e.key / 128) % 2 == 1);
            }
        };
        each(start1, size1);
        each(start2, size2);
        fifo_.finishedRead(size1 + size2);
    }
    bool learnAsToggle = false; // set with startLearn: a button learned onto a switch flips it

    juce::ValueTree toTree() const
    {
        juce::ValueTree tree("MIDIMAP");
        for (const auto& [k, t] : map_)
        {
            juce::ValueTree b("B");
            b.setProperty("key", k, nullptr);
            b.setProperty("kind", t.kind, nullptr);
            b.setProperty("a", t.paramA, nullptr);
            b.setProperty("b", t.paramB, nullptr);
            b.setProperty("va", t.valueA, nullptr);
            b.setProperty("vb", t.valueB, nullptr);
            b.setProperty("action", t.action, nullptr);
            b.setProperty("arg", t.arg, nullptr);
            tree.appendChild(b, nullptr);
        }
        return tree;
    }
    void fromTree(const juce::ValueTree& tree)
    {
        clearChannel(0);
        for (const auto& b : tree)
        {
            const int k = b.getProperty("key", -1);
            ControlTarget t;
            t.kind = juce::jlimit(0, static_cast<int>(ControlTarget::Action), static_cast<int>(b.getProperty("kind", 0)));
            t.paramA = b.getProperty("a").toString();
            t.paramB = b.getProperty("b").toString();
            const float va = b.getProperty("va", 0.0f), vb = b.getProperty("vb", 0.0f);
            t.valueA = std::isfinite(va) ? va : 0.0f;
            t.valueB = std::isfinite(vb) ? vb : 0.0f;
            t.action = b.getProperty("action", 0);
            t.arg = b.getProperty("arg", 0);
            if (k >= 0 && k < kKeys && t.kind != ControlTarget::None)
            {
                map_[k] = t;
                bound_[static_cast<size_t>(k)].store(true);
            }
        }
    }

private:
    struct Event
    {
        int key;
        float value;
        bool learned;
    };
    void push(Event e)
    {
        int start1, size1, start2, size2;
        fifo_.prepareToWrite(1, start1, size1, start2, size2);
        if (size1 > 0)
            events_[static_cast<size_t>(start1)] = e;
        else if (size2 > 0)
            events_[static_cast<size_t>(start2)] = e;
        fifo_.finishedWrite(size1 + size2);
    }

    std::array<std::atomic<bool>, kKeys> bound_;
    std::atomic<bool> learning_ { false };
    juce::AbstractFifo fifo_ { 1024 };
    std::array<Event, 1024> events_ {};
    std::map<int, ControlTarget> map_; // message thread
    ControlTarget learnTarget_;
};
