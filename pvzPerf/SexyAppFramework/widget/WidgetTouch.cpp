#include "WidgetManager.h"
#include "Widget.h"
#include "WidgetLifetimeHold.h"

namespace Sexy {
namespace {
TouchEvent LocalEvent(Widget& widget, const TouchEvent& event)
{
    const auto origin = widget.GetAbsPos();
    return {event.id, event.x - origin.mX, event.y - origin.mY};
}
}

void WidgetManager::NoteTouchInput()
{
    mLastPointerInput = PointerInputSource::Touch;
    mLastInputUpdateCnt = mUpdateCnt;
    if (mMouseHoverEnabled)
    {
        mMouseHoverEnabled = false;
        if (mOverWidget) RefreshPointerState(*mOverWidget);
    }
}

void WidgetManager::NoteMouseInput()
{
    mLastPointerInput = PointerInputSource::Mouse;
    mLastInputUpdateCnt = mUpdateCnt;
    if (!mMouseHoverEnabled)
    {
        mMouseHoverEnabled = true;
        if (mOverWidget) RefreshPointerState(*mOverWidget);
    }
}

void WidgetManager::RefreshPointerState(Widget& widget)
{
    widget.mIsDown = mLastDownWidget == &widget && mDownButtons != 0;
    widget.mIsOver = mMouseHoverEnabled && mOverWidget == &widget;
    if (mTouchCount == 0) return;
    for (const auto& contact : mTouches)
        if (contact.widget == &widget)
        {
            widget.mIsDown = true;
            // Both aggregate flags are final once any pointer is over this
            // captured widget. Do not hit-test every other finger again.
            const auto local = LocalEvent(widget, contact.event);
            if (widget.mIsOver ||
                (GetWidgetAt(contact.event.x, contact.event.y, nullptr, nullptr) == &widget &&
                 (contact.pressedTarget == &widget ||
                  widget.TouchHitTest(local.x, local.y) == contact.pressedTarget)))
            {
                widget.mIsOver = true;
                return;
            }
        }
}

void WidgetManager::TouchBegan(const TouchEvent& event)
{
    NoteTouchInput();
    TouchCapture* empty = nullptr;
    for (auto& contact : mTouches)
    {
        if (contact.widget && contact.event.id == event.id) return;
        if (!contact.widget && !empty) empty = &contact;
    }
    if (!empty) return;
    Widget* target = GetWidgetAt(event.x, event.y, nullptr, nullptr);
    if (!target) return;
    WidgetLifetimeHold lifetime(*target);
    const auto local = LocalEvent(*target, event);
    *empty = {target, event, target->TouchHitTest(local.x, local.y)};
    ++mTouchCount;
    if (target->mWantsFocus) SetFocus(target);
    if (empty->widget != target) return; // A focus callback can remove the target.
    RefreshPointerState(*target);
    target->TouchBegan(LocalEvent(*target, event));
    RefreshPointerState(*target);
}

void WidgetManager::TouchMoved(const TouchEvent& event)
{
    if (mTouchCount == 0) return;
    for (auto& contact : mTouches)
        if (contact.widget && contact.event.id == event.id)
        {
            NoteTouchInput();
            Widget* target = contact.widget;
            WidgetLifetimeHold lifetime(*target);
            contact.event = event;
            RefreshPointerState(*target);
            target->TouchMoved(LocalEvent(*target, event));
            RefreshPointerState(*target);
            return;
        }
}

void WidgetManager::TouchEnded(const TouchEvent& event)
{
    if (mTouchCount == 0) return;
    for (auto& contact : mTouches)
        if (contact.widget && contact.event.id == event.id)
        {
            NoteTouchInput();
            Widget* target = contact.widget;
            WidgetLifetimeHold lifetime(*target);
            const auto local = LocalEvent(*target, event);
            const bool sameTarget = target->TouchHitTest(local.x, local.y) == contact.pressedTarget;
            contact = {};
            --mTouchCount; // Retire before a callback opens a modal or removes itself.
            target->mIsDown = false;
            target->mIsOver = GetWidgetAt(event.x, event.y, nullptr, nullptr) == target;
            if (sameTarget) target->TouchEnded(LocalEvent(*target, event));
            else target->TouchCanceled(LocalEvent(*target, event));
            RefreshPointerState(*target);
            return;
        }
}

void WidgetManager::TouchCanceled(const TouchEvent& event)
{
    if (mTouchCount == 0) return;
    for (auto& contact : mTouches)
        if (contact.widget && contact.event.id == event.id)
        {
            Widget* target = contact.widget;
            WidgetLifetimeHold lifetime(*target);
            const auto local = LocalEvent(*target, contact.event);
            contact = {};
            --mTouchCount;
            RefreshPointerState(*target);
            target->TouchCanceled(local);
            RefreshPointerState(*target);
            return;
        }
}

void WidgetManager::CancelTouches(Widget* target)
{
    if (mTouchCount == 0) return;
    for (auto& contact : mTouches)
        if (contact.widget && (!target || contact.widget == target))
        {
            Widget* widget = contact.widget;
            WidgetLifetimeHold lifetime(*widget);
            const auto event = LocalEvent(*widget, contact.event);
            contact = {};
            --mTouchCount;
            RefreshPointerState(*widget);
            widget->TouchCanceled(event);
            RefreshPointerState(*widget);
        }
}

}
