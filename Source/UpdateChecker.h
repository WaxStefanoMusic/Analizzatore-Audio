#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

/**
    Verifica in background se esiste una release del plugin piu' recente di
    quella attualmente in esecuzione, interrogando l'API GitHub. Se la
    risposta indica una versione maggiore, mostra al thread UI un popup
    "Aggiornamento Disponibile" con i pulsanti "Aggiorna Ora" / "Piu' Tardi".

    L'oggetto puo' essere creato e dimenticato: il thread di fetch si
    auto-distrugge al termine. La verifica viene eseguita una sola volta
    per ciclo di vita del processo (host DAW), per non spammare l'utente.
*/
class UpdateChecker
{
public:
    /** Avvia il check (asincrono). Si auto-cancella se l'oggetto parent
        viene distrutto prima del completamento del fetch.                  */
    static void checkAsync (juce::Component* parentToCenterOver);

    /** Confronto semver semplice: "1.2.3" vs "1.2.4" -> true sul secondo.
        Tag con prefisso 'v' (es. "v1.0.0") vengono gestiti.                */
    static bool isNewerVersion (const juce::String& candidate,
                                const juce::String& current);

private:
    UpdateChecker() = delete;
    static std::atomic<bool> alreadyChecked;
};
