#pragma once

#include <juce_core/juce_core.h>

/**
    Costanti di versione e configurazione del canale di update.
    Quando bumpi una release: aggiorna `kVersion`, ricompila, poi crea un
    nuovo tag sul repo pubblico con quel numero esatto.

    Repo singolo pubblico: codice + release nello stesso posto.
*/
namespace AppInfo
{
    inline constexpr auto kVersion        = "1.0.0";
    inline constexpr auto kReleasesOwner  = "WaxStefanoMusic";
    inline constexpr auto kReleasesRepo   = "Analizzatore-Audio";

    inline juce::String latestReleaseApiUrl()
    {
        return juce::String ("https://api.github.com/repos/")
             + kReleasesOwner + "/" + kReleasesRepo + "/releases/latest";
    }

    inline juce::String releasesPageUrl()
    {
        return juce::String ("https://github.com/")
             + kReleasesOwner + "/" + kReleasesRepo + "/releases";
    }
}
