#include "UpdateChecker.h"
#include "Version.h"

std::atomic<bool> UpdateChecker::alreadyChecked { false };

namespace
{
    void showUpdatePopup (juce::Component::SafePointer<juce::Component> parent,
                          const juce::String& latestTag,
                          const juce::String& pageUrl)
    {
        if (parent == nullptr) return;

        const juce::String message =
            juce::String::fromUTF8 (
                "È disponibile una nuova versione del plugin.\n\n"
                "Versione attuale: ")
            + AppInfo::kVersion
            + juce::String::fromUTF8 ("\nNuova versione:    ")
            + latestTag
            + juce::String::fromUTF8 (
                "\n\nVuoi aprire ora la pagina di download?");

        auto opts = juce::MessageBoxOptions::makeOptionsYesNo (
            juce::MessageBoxIconType::InfoIcon,
            juce::String::fromUTF8 ("Aggiornamento Disponibile"),
            message,
            juce::String::fromUTF8 ("Aggiorna Ora"),
            juce::String::fromUTF8 ("Più Tardi"),
            parent.getComponent());

        juce::AlertWindow::showAsync (opts,
            [pageUrl] (int result)
            {
                if (result == 1)   // primo pulsante "Aggiorna Ora"
                    juce::URL (pageUrl).launchInDefaultBrowser();
            });
    }

    void doFetchAndPrompt (juce::Component::SafePointer<juce::Component> parent)
    {
        juce::URL url (AppInfo::latestReleaseApiUrl());

        // InputStreamOptions ha un membro const => operator= e' deleted.
        // Costruzione + chain-building in un'unica espressione.
        const auto opts = juce::URL::InputStreamOptions (
                              juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs (8000)
            .withHttpRequestCmd ("GET")
            .withExtraHeaders (
                juce::String ("User-Agent: AnalizzatoreAudio/") + AppInfo::kVersion + "\r\n"
              + "Accept: application/vnd.github+json");

        auto stream = url.createInputStream (opts);
        if (stream == nullptr) return;

        const juce::String content = stream->readEntireStreamAsString();
        const auto json = juce::JSON::parse (content);
        if (! json.isObject()) return;

        const juce::String tag  = json.getProperty ("tag_name", "").toString();
        const juce::String html = json.getProperty ("html_url", "").toString();
        if (tag.isEmpty()) return;

        if (! UpdateChecker::isNewerVersion (tag, AppInfo::kVersion))
            return;

        const juce::String pageUrl = html.isNotEmpty()
            ? html
            : AppInfo::releasesPageUrl();

        juce::MessageManager::callAsync (
            [parent, tag, pageUrl]() mutable
            {
                showUpdatePopup (parent, tag, pageUrl);
            });
    }
}

// ---------------------------------------------------------------------------
void UpdateChecker::checkAsync (juce::Component* parentToCenterOver)
{
    if (alreadyChecked.exchange (true))
        return;

    juce::Component::SafePointer<juce::Component> safe (parentToCenterOver);

    // Differisco di 1.5 s per non rallentare l'apertura dell'editor,
    // poi lancio un fire-and-forget thread che si auto-pulisce.
    juce::Timer::callAfterDelay (1500,
        [safe]()
        {
            juce::Thread::launch ([safe]()
            {
                doFetchAndPrompt (safe);
            });
        });
}

bool UpdateChecker::isNewerVersion (const juce::String& candidate,
                                    const juce::String& current)
{
    auto normalise = [] (juce::String s) -> juce::String
    {
        s = s.trim();
        if (s.startsWithIgnoreCase ("v")) s = s.substring (1);
        const int dash = s.indexOfChar ('-');
        if (dash >= 0) s = s.substring (0, dash);
        return s;
    };

    const juce::String A = normalise (candidate);
    const juce::String B = normalise (current);

    juce::StringArray ap, bp;
    ap.addTokens (A, ".", "");
    bp.addTokens (B, ".", "");

    const int n = juce::jmax (ap.size(), bp.size());
    for (int i = 0; i < n; ++i)
    {
        const int va = i < ap.size() ? ap[i].getIntValue() : 0;
        const int vb = i < bp.size() ? bp[i].getIntValue() : 0;
        if (va != vb) return va > vb;
    }
    return false;
}
