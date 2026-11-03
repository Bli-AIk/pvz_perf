#pragma once
#include "../NativeWindow.h"
#include <array>

namespace Sexy {
struct AndroidTouchContact {
    unsigned long long id = 0;
    double x = 0, y = 0;
    bool active = false, moved = false;
};

// Game-thread state. Move samples reuse fixed slots until a FIFO boundary.
class AndroidTouchInput {
public:
    std::array<AndroidTouchContact, 10> contacts{};
    unsigned int count = 0;
    NativeWindowEventCallback callback = nullptr;
    void* context = nullptr;

    void Begin(unsigned long long id, double x, double y);
    void Move(unsigned long long id, double x, double y);
    void End(unsigned long long id, double x, double y);
    void Cancel(unsigned long long id);
    void CancelAll();
    void FlushMoves();
};
}
