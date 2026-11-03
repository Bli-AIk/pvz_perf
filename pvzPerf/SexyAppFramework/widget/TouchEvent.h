#pragma once

namespace Sexy {
enum class PointerInputSource : unsigned char { None, Mouse, Touch };

// Stable for a contact's lifetime; coordinates are logical, local to the target.
// ID namespaces distinguish native touch sources without changing mouse position.
struct TouchEvent {
    unsigned long long id = 0;
    int x = 0;
    int y = 0;
};

class Widget;
struct TouchCapture {
    Widget* widget = nullptr;
    TouchEvent event;
    const void* pressedTarget = nullptr; // Identity only; never dereferenced.
};

}
