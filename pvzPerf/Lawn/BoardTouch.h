#pragma once
#include "widget/TouchEvent.h"
#include <array>

class CursorObject;
class CursorPreview;

struct BoardTouchContact {
    unsigned long long id = 0;
    CursorObject* cursor = nullptr;
    CursorPreview* preview = nullptr;
    int x = 0;
    int y = 0;
    int startX = 0;
    int startY = 0;
    bool active = false;
    bool ignoreUp = false;
    bool cannon = false; // Cannon aiming belongs to this finger, outside the shared plant/tool selection.
    bool toggleSelectionOnRelease = false;
    bool fieldCardPress = false; // Source is already held by cursor->mCoinID; never sweep-select another card.
    bool fieldCardDragged = false;
    bool collected = false; // Only aquarium feeding reads this; other touch commands still run.
    bool aquariumFeed = false; // Empty-space press; movement permanently cancels this tap.
};

// Owned by Board, outside its serialized tail. Reused across gestures.
struct BoardTouchState {
    std::array<BoardTouchContact, 10> contacts{};
    BoardTouchContact* selection = nullptr; // One shared plant/tool selection, retained after release.
    CursorObject* mouseCursor = nullptr; // Alias; Board retains ownership.
    CursorPreview* mousePreview = nullptr;
    unsigned int activeCount = 0;
};
