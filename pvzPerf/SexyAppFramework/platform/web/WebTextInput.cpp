#include "WebTextInput.h"
#include "platform/TabletCaret.h"
#include "SexyAppBase.h"
#include "widget/EditWidget.h"
#include "widget/WidgetManager.h"
#include <SDL.h>
#include <emscripten.h>
#include <algorithm>
#include <string>

EM_JS(void, WebShowEditor, (const char* value, int session, int limit, int cursor, int selection), {
    Module.pvzTextInput.show(UTF8ToString(value),session,limit,cursor,selection);
});
EM_JS(void, WebSyncEditor, (const char* value), { Module.pvzTextInput.sync(UTF8ToString(value)); });
EM_JS(void, WebHideEditor, (), { Module.pvzTextInput.hide(); });
EM_JS(int, WebTakeEdit, (char* buffer, int capacity, int* metadata), {
    const edit=Module.pvzTextInput.take();if(!edit)return 0;
    const count=Math.min(edit.text.length,capacity-1);
    for(let i=0;i<count;++i)HEAPU8[buffer+i]=edit.text.charCodeAt(i);
    HEAPU8[buffer+count]=0;
    HEAP32[metadata>>2]=edit.session;HEAP32[(metadata>>2)+1]=edit.start;
    HEAP32[(metadata>>2)+2]=edit.end;HEAP32[(metadata>>2)+3]=edit.action;
    return 1;
});

namespace Sexy {
namespace {
EditWidget* editor = nullptr;
int session = 0;
std::string published;
}
void NativeTabletCaret::Begin(NativeWindowHandle) {
    auto* focus = gSexyAppBase->mWidgetManager->mFocusWidget;
    editor = dynamic_cast<EditWidget*>(focus);
    ++session; published.clear();
}
void NativeTabletCaret::Position(int, int) {
    if (editor && editor->mString != published) {
        published = editor->mString;
        WebSyncEditor(published.c_str());
    }
}
void NativeTabletCaret::Show(NativeWindowHandle) {
    if (!editor) return;
    published = editor->mString;
    WebShowEditor(published.c_str(), session,
        editor->mMaxChars > 0 ? std::min(editor->mMaxChars,4095) : 4095,
        editor->mCursorPos, editor->mHilitePos);
}
void NativeTabletCaret::End(NativeWindowHandle) {
    editor = nullptr; ++session; published.clear(); WebHideEditor();
}
void ProcessWebTextInput() {
    if (!editor) return; // No mailbox/JS calls in ordinary gameplay frames.
    char text[4096]; int metadata[4];
    if (!WebTakeEdit(text,sizeof(text),metadata) || metadata[0] != session) return;
    if (gSexyAppBase->mWidgetManager->mFocusWidget != editor) return;
    auto* target = editor;
    const std::string old = target->mString;
    const std::string next = text;
    size_t prefix=0,suffix=0;
    while(prefix<old.size() && prefix<next.size() && old[prefix]==next[prefix])++prefix;
    while(suffix<old.size()-prefix && suffix<next.size()-prefix &&
          old[old.size()-suffix-1]==next[next.size()-suffix-1])++suffix;
    target->mCursorPos=static_cast<int>(old.size()-suffix);
    target->mHilitePos=static_cast<int>(prefix);
    if(old.size()-suffix>prefix)target->KeyDown(KEYCODE_BACK);
    for(size_t i=prefix;i<next.size()-suffix;++i) {
        const unsigned char value=next[i];
        if(value>=32 && value<=126)target->KeyChar(static_cast<char>(value));
    }
    const int length=static_cast<int>(target->mString.size());
    target->mCursorPos=std::clamp(metadata[2],0,length);
    target->mHilitePos=metadata[1]==metadata[2]?-1:std::clamp(metadata[1],0,length);
    target->MarkDirty();
    published=target->mString;WebSyncEditor(published.c_str());
    if(metadata[3]==1)target->KeyDown(KEYCODE_RETURN); // May destroy the editor.
    else if(metadata[3]==2) {
        SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_ESCAPE;SDL_PushEvent(&event);
    }
}
}
