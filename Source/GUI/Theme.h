#pragma once

#include <juce_graphics/juce_graphics.h>
#include <algorithm>

namespace Theme
{
    inline const juce::Colour bg          { 0xff121418 };
    inline const juce::Colour panel       { 0xff1a1d24 };
    inline const juce::Colour panelLight  { 0xff242832 };
    inline const juce::Colour text        { 0xffe4e6ea };
    inline const juce::Colour textDim     { 0xff8a8f99 };
    inline const juce::Colour accent      { 0xff5ec0ff };  // live (blue)
    inline const juce::Colour green       { 0xff5ee07a };  // OK / under target
    inline const juce::Colour amber       { 0xffe7b34a };  // warning
    inline const juce::Colour red         { 0xffe75a5a };  // over
    inline const juce::Colour aColour     { 0xffffa552 };  // snapshot A (orange)
    inline const juce::Colour bColour     { 0xffb085ff };  // snapshot B (purple)
    inline const juce::Colour grid        { 0x22ffffff };
}

/**
    Resolution-independent scaling helpers.
    Each panel computes a `scale` factor from the ratio between its current
    bounds and the "design" bounds it was authored for, and multiplies all
    pixel constants by it. This way the same UI looks proportional from
    600×400 up to 4K and beyond.
*/
namespace Scale
{
    inline float forArea (juce::Rectangle<int> area, int designW, int designH) noexcept
    {
        if (designW <= 0 || designH <= 0) return 1.0f;
        const float sx = (float) area.getWidth()  / (float) designW;
        const float sy = (float) area.getHeight() / (float) designH;
        // Use the smaller axis so nothing overflows; clamp to a sane range.
        return juce::jlimit (0.5f, 4.0f, std::min (sx, sy));
    }

    inline int   px (float v, float scale) noexcept { return (int) std::round (v * scale); }
    inline int   px (int   v, float scale) noexcept { return (int) std::round ((float) v * scale); }
    inline float pxf (float v, float scale) noexcept { return v * scale; }

    inline juce::Font font (float baseSize, float scale, bool bold = false) noexcept
    {
        return juce::Font (juce::FontOptions (
            baseSize * scale,
            bold ? juce::Font::bold : juce::Font::plain));
    }
}
