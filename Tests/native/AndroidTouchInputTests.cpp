#include "SexyAppFramework/platform/android/AndroidTouchInput.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>

namespace {
bool trackAllocations = false;
unsigned long long allocations = 0;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Recorder {
    std::array<Sexy::NativeWindowEvent, 128> events{};
    unsigned int count = 0;
    static void Record(void* context, const Sexy::NativeWindowEvent& event) {
        auto& self = *static_cast<Recorder*>(context);
        Require(self.count < self.events.size(), "Event overflow");
        self.events[self.count++] = event;
    }
};
}
void* operator new(std::size_t size) {
    if (trackAllocations) ++allocations;
    if (void* ptr = std::malloc(size ? size : 1)) return ptr;
    throw std::bad_alloc();
}
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }

int main()
{
    try {
        using Kind = Sexy::NativeWindowEventKind;
        Recorder recorder;
        Sexy::AndroidTouchInput input;
        input.callback = Recorder::Record;
        input.context = &recorder;
        trackAllocations = true;
        input.Begin(3, 10, 20);
        input.Begin(7, 30, 40);
        input.Begin(7, 30, 40);
        Require(input.count == 2 && recorder.count == 2, "Duplicate down or lost contact");
        for (int i = 0; i < 10000; ++i) input.Move(3, i, 20);
        Require(recorder.count == 2, "Move flood reached the game FIFO");
        input.End(3, 10000, 20);
        Require(recorder.count == 4 && recorder.events[2].kind == Kind::TouchMoved &&
            recorder.events[2].touchId == 3 && recorder.events[2].x == 9999 &&
            recorder.events[3].kind == Kind::TouchEnded && recorder.events[3].x == 10000,
            "Final move was not delivered before up");
        // Index 0 is now the surviving contact; only its stable ID matters.
        input.Move(7, 55, 66);
        input.FlushMoves();
        Require(input.count == 1 && recorder.events[4].touchId == 7 &&
            recorder.events[4].x == 55, "Surviving ID was replaced after index compaction");
        input.Cancel(7);
        input.End(7, 55, 66);
        Require(input.count == 0 && recorder.count == 6 &&
            recorder.events[5].kind == Kind::TouchCanceled, "Cancel became a release");
        input.Begin(9, 1, 1);
        input.Move(9, 2, 2);
        input.Begin(11, 3, 3);
        Require(recorder.events[7].kind == Kind::TouchMoved &&
            recorder.events[8].kind == Kind::TouchBegan, "Down overtook pending motion");
        input.Move(9, 4, 4);input.CancelAll();
        Require(input.count == 0 && recorder.events[9].kind == Kind::TouchCanceled &&
            recorder.events[10].kind == Kind::TouchCanceled, "Focus/resize cancellation lost contacts");
        recorder.count = 0;
        for (unsigned int i = 0; i < 10; ++i) input.Begin(100 + i, 0, 0);
        input.Begin(999, 0, 0);
        Require(input.count == 10 && recorder.count == 10, "Contact limit corrupted active slots");
        for (int sample = 1; sample <= 10000; ++sample)
            for (unsigned int i = 0; i < 10; ++i) input.Move(100 + i, sample, sample + i);
        input.FlushMoves();
        Require(recorder.count == 20, "Ten-finger burst did not coalesce to ten moves");
        input.FlushMoves();
        Require(recorder.count == 20, "Unchanged contacts generated repeated moves");
        for (unsigned int i = 0; i < 10; ++i) input.End(100 + i, 10000, 10000 + i);
        Require(recorder.count == 30 && input.count == 0, "Ten-finger release lost a transition");
        trackAllocations = false;
        Require(allocations == 0, "Touch coalescing allocated memory");
        std::puts("PASS Android touch: stable IDs/index compaction, ordered down/move/up, individual/all cancel, 10-contact bound, 100000 moves -> 10 callbacks, zero coalescer allocations");
        return 0;
    } catch (const std::exception& error) {
        trackAllocations = false;
        std::fprintf(stderr, "FAIL Android touch: %s\n", error.what());
        return 1;
    }
}
