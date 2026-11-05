#include "LawnApp.h"
#include "Lawn/Board.h"
#include "Resources.h"
#include "Sexy.TodLib/TodStringFile.h"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <emscripten.h>

bool (*gAppCloseRequest)();
extern "C" {
EMSCRIPTEN_KEEPALIVE int pvz_web_scene() { return gLawnApp ? static_cast<int>(gLawnApp->mGameScene) : -1; }
EMSCRIPTEN_KEEPALIVE int pvz_web_has_board() { return gLawnApp && gLawnApp->mBoard ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE int pvz_web_paused() { return gLawnApp && gLawnApp->mBoard && gLawnApp->mBoard->mPaused; }
EMSCRIPTEN_KEEPALIVE int pvz_web_active() { return gLawnApp && gLawnApp->mActive; }
EMSCRIPTEN_KEEPALIVE unsigned int pvz_web_updates() { return gLawnApp ? gLawnApp->mUpdateCount : 0; }
EMSCRIPTEN_KEEPALIVE unsigned int pvz_web_draws() { return gLawnApp ? gLawnApp->mDrawCount : 0; }
}
int main() {
    setenv("PVZ_APPDATA_DIR", "/persist/development-v331/", 1);
    try {
        gTodStringFormats = gLawnStringFormats;
        gTodStringFormatCount = gLawnStringFormatCount;
        gAppCloseRequest = LawnGetCloseRequest;
        gLawnApp = new LawnApp;
        gLawnApp->mTabletPC = EM_ASM_INT({ return Module.pvzMobileInput ? 1 : 0; });
        gLawnApp->mChangeDirTo = "/";
        gLawnApp->mIsWindowed = true;
        gLawnApp->mOnlyAllowOneCopyToRun = false;
        gLawnApp->mCmdLineParsed = true;
        gLawnApp->Init();
        gLawnApp->Start();
        gLawnApp->Shutdown();
        delete gLawnApp; gLawnApp = nullptr;
        EM_ASM({ Module.FS.syncfs(false, error => { if (error) console.error(error); Module.pvzOnQuit?.(); }); });
    } catch (const std::exception& error) {
        emscripten_log(EM_LOG_ERROR, "pvz_perf Web: %s", error.what());
        return 1;
    }
    return 0;
}
