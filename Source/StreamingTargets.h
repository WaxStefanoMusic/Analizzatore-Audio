#pragma once

#include <juce_core/juce_core.h>
#include <array>

/**
    Streaming-platform loudness/true-peak targets.
    Sources: published platform specs (current as of 2024-2026 — verify with the
    individual services if you need authoritative numbers).
*/
struct StreamingTarget
{
    juce::String name;
    float        lufsTarget;   // integrated LUFS target
    float        truePeakDb;   // max true-peak
    juce::String notes;
};

namespace StreamingTargets
{
    inline const std::array<StreamingTarget, 9> all = { {
        { "Custom",              -14.0f, -1.0f, "User-defined target" },
        { "Spotify",             -14.0f, -1.0f, "Default normalization" },
        { "Spotify (Loud)",      -11.0f, -2.0f, "Loud setting in client" },
        { "YouTube",             -14.0f, -1.0f, "" },
        { "Apple Music",         -16.0f, -1.0f, "Sound Check" },
        { "Tidal",               -14.0f, -1.0f, "" },
        { "Amazon Music",        -14.0f, -2.0f, "" },
        { "SoundCloud",          -14.0f, -1.0f, "" },
        { "Broadcast EBU R128",  -23.0f, -1.0f, "Broadcast / TV" },
    } };

    inline const StreamingTarget& byIndex (int idx) noexcept
    {
        if (idx < 0 || idx >= (int) all.size()) return all[0];
        return all[(size_t) idx];
    }

    inline juce::StringArray names()
    {
        juce::StringArray a;
        for (const auto& t : all) a.add (t.name);
        return a;
    }
}
