#include "AndroidTouchInput.h"

namespace Sexy {
namespace {
void Emit(AndroidTouchInput& input, NativeWindowEventKind kind, const AndroidTouchContact& contact)
{
    if (!input.callback) return;
    NativeWindowEvent event{};
    event.kind = kind;
    event.touchId = contact.id;
    event.x = contact.x;
    event.y = contact.y;
    input.callback(input.context, event);
}
}

void AndroidTouchInput::Begin(unsigned long long id, double x, double y)
{
    AndroidTouchContact* empty = nullptr;
    for (auto& contact : contacts)
    {
        if (contact.active && contact.id == id) return;
        if (!contact.active && !empty) empty = &contact;
    }
    if (!empty) return;
    FlushMoves();
    *empty = {id, x, y, true, false};
    ++count;
    Emit(*this, NativeWindowEventKind::TouchBegan, *empty);
}

void AndroidTouchInput::Move(unsigned long long id, double x, double y)
{
    if (count == 0) return;
    for (auto& contact : contacts)
        if (contact.active && contact.id == id)
        {
            if (contact.x == x && contact.y == y) return;
            contact.x = x;
            contact.y = y;
            contact.moved = true;
            return;
        }
}

void AndroidTouchInput::End(unsigned long long id, double x, double y)
{
    if (count == 0) return;
    FlushMoves();
    for (auto& contact : contacts)
        if (contact.active && contact.id == id)
        {
            const AndroidTouchContact ended{id, x, y};
            contact = {};
            --count;
            Emit(*this, NativeWindowEventKind::TouchEnded, ended);
            return;
        }
}

void AndroidTouchInput::Cancel(unsigned long long id)
{
    if (count == 0) return;
    for (auto& contact : contacts)
        if (contact.active && contact.id == id)
        {
            const auto canceled = contact;
            contact = {};
            --count;
            Emit(*this, NativeWindowEventKind::TouchCanceled, canceled);
            return;
        }
}

void AndroidTouchInput::CancelAll()
{
    if (count == 0) return;
    count = 0;
    for (auto& contact : contacts)
        if (contact.active)
        {
            const auto canceled = contact;
            contact = {};
            Emit(*this, NativeWindowEventKind::TouchCanceled, canceled);
        }
}

void AndroidTouchInput::FlushMoves()
{
    if (count == 0) return;
    for (auto& contact : contacts)
        if (contact.active && contact.moved)
        {
            contact.moved = false;
            Emit(*this, NativeWindowEventKind::TouchMoved, contact);
        }
}
}
