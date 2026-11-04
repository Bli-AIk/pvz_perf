#pragma once

#include "LawnApp.h"
#include "Lawn/Widget/LawnDialog.h"
#include "Lawn/Widget/GameButton.h"
#include "SexyAppFramework/graphics/Graphics.h"
#include "Resources.h"
#include <stdexcept>

// Run only in an isolated native fixture after loading the real LoadingFonts,
// LoadingImages and CommonUI atlas. No save, profile or native close is requested.
// The Quit dialog measures wrapped text without a draw destination. Measurement
// must share line breaking without entering the glyph draw path.
inline void CheckQuitDialogRuntime(LawnApp& app, Sexy::Graphics& target)
{
    if (app.GetDialog(Dialogs::DIALOG_QUIT))
        throw std::runtime_error("Quit regression requires no existing Quit dialog");

    Sexy::Graphics measure;
    measure.mFont = Sexy::FONT_DWARVENTODCRAFT15;
    const auto height = measure.GetWordWrappedHeight(180,
        "Measure wrapped text without a destination before creating Quit.");
    if (height <= 0)
        throw std::runtime_error("Destination-free font measurement returned no height");

    app.ConfirmQuit();
    auto* dialog = static_cast<LawnDialog*>(app.GetDialog(Dialogs::DIALOG_QUIT));
    if (!dialog || !dialog->mYesButton || !dialog->mNoButton)
        throw std::runtime_error("ConfirmQuit did not create both real dialog buttons");
    try
    {
        dialog->Draw(&target);
        dialog->mYesButton->Draw(&target);
        dialog->mNoButton->Draw(&target);
    }
    catch (...)
    {
        app.KillDialog(Dialogs::DIALOG_QUIT);
        throw;
    }
    app.KillDialog(Dialogs::DIALOG_QUIT);
}
