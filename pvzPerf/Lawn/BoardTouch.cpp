#include "BoardTouch.h"
#include "LandscapeHud.h"
#include "Board.h"
#include "CursorObject.h"
#include "SeedPacket.h"
#include "Coin.h"
#include "Plant.h"
#include "Cutscene.h"
#include "Challenge.h"
#include "GridItem.h"
#include "ZenGarden.h"
#include "Widget/GameButton.h"
#include "LawnApp.h"
#include "Resources.h"
#include "graphics/Graphics.h"
#include "graphics/AtlasDraw.h"
#include "Sexy.TodLib/generated/SceneAtlasData.h"
#include "widget/WidgetManager.h"
#include "widget/WidgetLifetimeHold.h"
#include <utility>

namespace {
// Existing commands consume Board's current cursor. Select a contact only for
// the synchronous call; the mouse retains its own cursor, coordinates and flags.
class ContactScope {
    Board& board;
    BoardTouchContact& contact;
    CursorObject* mouseCursor;
    CursorPreview* mousePreview;
    int cursorNumber;
    Sexy::TimeCounter cannonDelay;
    int cannonX, cannonY;
    bool ignoreUp;
public:
    ContactScope(Board& target, BoardTouchContact& input, bool selectCursor = true);
    ~ContactScope();
};

ContactScope::ContactScope(Board& target, BoardTouchContact& input, bool selectCursor)
    : board(target), contact(input),
      mouseCursor(target.mCursorObject), mousePreview(target.mCursorPreview),
      cursorNumber(target.mApp->mCursorNum), cannonDelay(target.mCobCannonCursorDelayCounter),
      cannonX(target.mCobCannonMouseX), cannonY(target.mCobCannonMouseY),
      ignoreUp(target.mIgnoreMouseUp)
{
    if (selectCursor)
    {
        board.mCursorObject = contact.cursor;
        board.mCursorPreview = contact.preview;
        // Touch uses drag distance, not the mouse's timed tap-to-aim guard.
        board.mCobCannonCursorDelayCounter = 0;
        board.mCobCannonMouseX = contact.startX;
        board.mCobCannonMouseY = contact.startY;
    }
    board.mIgnoreMouseUp = contact.ignoreUp;
}

ContactScope::~ContactScope()
{
    board.mCobCannonCursorDelayCounter = cannonDelay;
    board.mCobCannonMouseX = cannonX;
    board.mCobCannonMouseY = cannonY;
    contact.ignoreUp = board.mIgnoreMouseUp;
    board.mIgnoreMouseUp = ignoreUp;
    board.mCursorObject = mouseCursor;
    board.mCursorPreview = mousePreview;
    board.mApp->mCursorNum = cursorNumber;
}

BoardTouchContact* FindContact(Board& board, unsigned long long id)
{
    if (board.mTouchState && board.mTouchState->activeCount != 0)
        for (auto& contact : board.mTouchState->contacts)
            if (contact.active && contact.id == id) return &contact;
    return nullptr;
}

bool Interactive(const Board& board)
{
    return !board.mPaused && board.mTimeStopCounter == 0 &&
        board.mBoardFadeOutCounter < 0 && board.mApp->GetDialogCount() == 0 &&
        (board.mApp->mGameScene == SCENE_PLAYING || board.mCutScene->IsInShovelTutorial());
}

struct TouchCollection {
    bool collected = false;
    bool leftBoard = false;
};

TouchCollection CollectAlongTouch(Board& board, int fromX, int fromY, int toX, int toY)
{
    TouchCollection result;
    if (!Interactive(board)) return result;
    Coin* coin = nullptr;
    while (board.IterateCoins(coin))
        if (coin->TouchHitTest(fromX, fromY, toX, toY))
        {
            const bool award = coin->IsLevelAward();
            coin->MouseDown(toX, toY, 1);
            result.collected |= coin->mIsBeingCollected;
            if (award && coin->mIsBeingCollected)
            {
                // A reward may start the next screen or fan out new coins.
                // Do not continue this scan or commit the held plant/cannon.
                board.mApp->mWidgetManager->CancelTouches(&board);
                board.ClearTouchSelection();
                result.leftBoard = true;
                return result;
            }
        }
    return result;
}

bool FieldCardContains(const Coin& coin, int x, int y)
{
    return x >= coin.mPosX && x < coin.mPosX + coin.mWidth &&
        y >= coin.mPosY && y < coin.mPosY + coin.mHeight;
}

Coin* FieldCardAt(Board& board, int x, int y)
{
    Coin* coin = nullptr;
    Coin* top = nullptr;
    while (board.IterateCoins(coin))
    {
        if (coin->mType != COIN_USABLE_SEED_PACKET || !FieldCardContains(*coin, x, y)) continue;
        if (!top || coin->mRenderOrder >= top->mRenderOrder) top = coin;
    }
    return top;
}

bool FieldCardDragged(const BoardTouchContact& contact, int x, int y)
{
    // Logical-pixel movement tolerance; independent of elapsed time or refresh rate.
    static constexpr long long tapSlop = 6;
    const long long dx = static_cast<long long>(x) - contact.startX;
    const long long dy = static_cast<long long>(y) - contact.startY;
    return contact.fieldCardDragged || dx * dx + dy * dy > tapSlop * tapSlop;
}

GameObjectType SelectedGardenTool(CursorType cursor)
{
    // Plant pickup keeps the source tool selected until placement or cancellation.
    static constexpr GameObjectType tools[] = {
        OBJECT_TYPE_NONE, OBJECT_TYPE_NONE, OBJECT_TYPE_NONE, OBJECT_TYPE_GLOVE,
        OBJECT_TYPE_NONE, OBJECT_TYPE_WHEELBARROW, OBJECT_TYPE_NONE, OBJECT_TYPE_NONE,
        OBJECT_TYPE_NONE, OBJECT_TYPE_WATERING_CAN, OBJECT_TYPE_FERTILIZER,
        OBJECT_TYPE_BUG_SPRAY, OBJECT_TYPE_PHONOGRAPH, OBJECT_TYPE_CHOCOLATE,
        OBJECT_TYPE_GLOVE, OBJECT_TYPE_MONEY_SIGN, OBJECT_TYPE_WHEELBARROW,
        OBJECT_TYPE_TREE_FOOD
    };
    static_assert(std::size(tools) == CURSOR_TYPE_TREE_FOOD + 1);
    const unsigned int index = static_cast<unsigned int>(cursor);
    return index < std::size(tools) ? tools[index] : OBJECT_TYPE_NONE;
}

bool GardenMode(const Board& board)
{
    return board.mApp->mGameMode == GAMEMODE_LIMBO_ZEN_GARDEN ||
        board.mApp->mGameMode == GAMEMODE_TREE_OF_WISDOM;
}

GameObjectType GardenControlAt(Board& board, int x, int y)
{
    if (!GardenMode(board)) return OBJECT_TYPE_NONE;
    for (int index = OBJECT_TYPE_WATERING_CAN; index <= OBJECT_TYPE_NEXT_GARDEN; ++index)
    {
        const auto tool = static_cast<GameObjectType>(index);
        if (!board.CanUseGameObject(tool) || (tool == OBJECT_TYPE_TREE_FOOD &&
            !board.mChallenge->TreeOfWisdomCanFeed())) continue;
        Sexy::Rect bounds = board.GetShovelButtonRect();
        board.GetZenButtonRect(tool, bounds);
        if (bounds.Contains(x, y)) return tool;
    }
    return OBJECT_TYPE_NONE;
}

bool IsSelectedControl(Board& board, int x, int y)
{
    const auto& cursor = *board.mCursorObject;
    if (GardenMode(board))
    {
        const auto tool = SelectedGardenTool(cursor.mCursorType);
        if (tool == OBJECT_TYPE_NONE) return false;
        Sexy::Rect bounds = board.GetShovelButtonRect();
        board.GetZenButtonRect(tool, bounds);
        return bounds.Contains(x, y);
    }
    if (cursor.mCursorType == CURSOR_TYPE_SHOVEL)
        return board.mShowShovel && board.GetShovelButtonRect().Contains(x, y);
    if (cursor.mCursorType == CURSOR_TYPE_PLANT_FROM_USABLE_COIN)
    {
        const auto* coin = board.mCoins.DataArrayTryToGet(cursor.mCoinID);
        return coin && !coin->mDead && FieldCardContains(*coin, x, y);
    }
    if (cursor.mCursorType != CURSOR_TYPE_PLANT_FROM_BANK) return false;
    HitResult hit;
    return board.mSeedBank->MouseHitTest(x, y, &hit) &&
        static_cast<SeedPacket*>(hit.mObject)->mIndex == cursor.mSeedBankIndex;
}

Plant* ReadyCannonAt(Board& board, int x, int y)
{
    if (!Interactive(board)) return nullptr;
    const auto* selected = board.mTouchState->selection;
    if (selected && selected->cursor->mCursorType != CURSOR_TYPE_NORMAL && !selected->cannon)
        return nullptr;
    // The existing cell table registers both halves of a cob cannon.
    auto* plant = board.GetTopPlantAt(board.PixelToGridX(x, y), board.PixelToGridY(x, y),
        TOPPLANT_ONLY_NORMAL_POSITION);
    return plant && !plant->mDead && plant->mSeedType == SEED_COBCANNON &&
        plant->mState == STATE_COBCANNON_READY ? plant : nullptr;
}

bool CannonClaimed(Board& board, Plant& cannon)
{
    const auto id = static_cast<PlantID>(board.mPlants.DataArrayGetID(&cannon));
    const auto& mouse = *board.mTouchState->mouseCursor;
    if (mouse.mCursorType == CURSOR_TYPE_COBCANNON_TARGET && mouse.mCobCannonPlantID == id)
        return true;
    for (const auto& contact : board.mTouchState->contacts)
        if (contact.active && contact.cannon && contact.cursor->mCobCannonPlantID == id)
            return true;
    return false;
}

void ValidateSelection(Board& board)
{
    const auto& cursor = *board.mCursorObject;
    if (cursor.mCursorType == CURSOR_TYPE_PLANT_FROM_USABLE_COIN)
    {
        const auto* coin = board.mCoins.DataArrayTryToGet(cursor.mCoinID);
        if (!coin || coin->mDead) board.ClearCursor();
    }
    else if (cursor.mCursorType == CURSOR_TYPE_PLANT_FROM_GLOVE ||
        cursor.mCursorType == CURSOR_TYPE_COBCANNON_TARGET)
    {
        const auto* plant = board.mPlants.DataArrayTryToGet(cursor.mCursorType ==
            CURSOR_TYPE_PLANT_FROM_GLOVE ? cursor.mGlovePlantID : cursor.mCobCannonPlantID);
        if (!plant || plant->mDead || (cursor.mCursorType == CURSOR_TYPE_COBCANNON_TARGET &&
            plant->mState != STATE_COBCANNON_READY)) board.ClearCursor();
    }
}

void UpdateContact(Board& board, BoardTouchContact& contact)
{
    if (contact.ignoreUp) return;
    ContactScope scope(board, contact);
    ValidateSelection(board);
    contact.preview->Update(contact.x, contact.y);
    contact.cursor->Update(contact.x, contact.y);
}

void ReleaseContact(Board& board, BoardTouchContact& contact)
{
    contact.active = false;
    --board.mTouchState->activeCount;
    if (!contact.cursor) return;
    if (contact.cannon)
        board.ClearCursor(); // Every cannon gesture ends here; no pending tap-to-aim target.
    contact.cannon = false;
    contact.toggleSelectionOnRelease = false;
    contact.fieldCardPress = contact.fieldCardDragged = false;
    contact.cursor->mVisible = false;
    contact.preview->mVisible = false;
    // A transient hammer must not leave an unowned animation in a saved pool.
    contact.cursor->Die();
}

void DrawRoofGuide(Board& board, Sexy::Graphics& graphics, int col, int firstRow, int lastRow)
{
    // Free's roof cells join adjacent column heights; rectangles create a staircase.
    const int next = col < MAX_GRID_SIZE_X - 1 ? col + 1 : col;
    const int left = board.GridToPixelX(col, firstRow);
    const int right = left + board.GridToPixelX(1, 0) - board.GridToPixelX(0, 0);
    const int bottomHeight = lastRow == 4 ? 80 : board.GridToPixelY(col, 1) - board.GridToPixelY(col, 0);
    const Sexy::Point corners[] = {
        {left, board.GridToPixelY(col, firstRow) + 20},
        {right, board.GridToPixelY(next, firstRow) + 20},
        {right, board.GridToPixelY(next, lastRow) + 20 + bottomHeight},
        {left, board.GridToPixelY(col, lastRow) + 20 + bottomHeight}
    };
    graphics.PolyFill(corners, 4, true); // Convex GPU geometry; no CPU scanline rasterization.
}

void DrawTouchGuides(Board& board, const BoardTouchContact& contact, const Sexy::Graphics& graphics)
{
    // Preview update already checks CanPlantAt; reuse its result instead of
    // repeating planting rules during drawing. Shovels use plant glow only.
    if (!board.IsPlantInCursor() || !contact.preview->mVisible || !Interactive(board) ||
        board.mApp->mGameMode == GAMEMODE_LIMBO_ZEN_GARDEN) return;
    const int col = contact.preview->mGridX;
    const int row = contact.preview->mGridY;
    const int rows = board.StageHasPool() ? 6 : 5;
    if (col < 0 || col >= MAX_GRID_SIZE_X || row < 0 || row >= rows) return;
    const int cellWidth = board.GridToPixelX(1, 0) - board.GridToPixelX(0, 0);
    const int cellHeight = board.GridToPixelY(col, 1) - board.GridToPixelY(col, 0);
    const int left = board.GridToPixelX(col, row), top = board.GridToPixelY(col, row);
    if (!Sexy::Rect(left, top, cellWidth, cellHeight).Contains(contact.x, contact.y)) return;
    Sexy::Graphics guides(graphics);
    guides.mDrawMode = Sexy::Graphics::DRAWMODE_ADDITIVE;
    guides.mColor = Sexy::Color(255, 255, 255, 64);
    if (board.StageHasRoof())
    {
        DrawRoofGuide(board, guides, col, 0, rows - 1);
        for (int x = 0; x < MAX_GRID_SIZE_X; ++x) DrawRoofGuide(board, guides, x, row, row);
    }
    else
    {
        guides.FillRect(left, board.GridToPixelY(col, 0), cellWidth, cellHeight * rows);
        guides.FillRect(board.GridToPixelX(0, row), top, cellWidth * MAX_GRID_SIZE_X, cellHeight);
    }
}

void DrawSelectionFrame(Sexy::Graphics& graphics, const Sexy::Rect& bounds, bool native = false)
{
    if(native) {
        // WP's native yellow/orange corner art supplies contrast without a
        // wider solid stroke. Reposition the corners; never scale the pixels.
        static constexpr auto first=static_cast<unsigned int>(LandscapeUIAtlas::Sprite::SELECTED_TL);
        graphics.mColorizeImages=false;
        graphics.mLinearBlend=false;
        for(unsigned int corner=0;corner<4;++corner) {
            const bool right=corner&1,bottom=corner&2;
            const int x=bounds.mX+(right?bounds.mWidth-27:-5);
            const int y=bounds.mY+(bottom?bounds.mHeight-28:-2);
            Sexy::DrawAtlasImage(&graphics,{LandscapeUIAtlas::Texture,0},
                LandscapeUIAtlas::Images[first+corner],x,y);
        }
        return;
    }
    // Original TV image: four 42x36 corners in a 104x144 image. Draw each
    // corner at half size on the classic HUD.
    static constexpr auto first = static_cast<unsigned int>(CommonUIAtlas::Sprite::TOUCH_FRAME_TL);
    graphics.mColorizeImages = false;
    for (unsigned int i = 0; i < 4; ++i)
    {
        const int x = i & 1 ? bounds.mX + bounds.mWidth - 18 : bounds.mX - 3;
        const int y = i & 2 ? bounds.mY + bounds.mHeight - 15 : bounds.mY - 3;
        Sexy::DrawAtlasImage(&graphics, {CommonUIAtlas::Texture, 0},
            CommonUIAtlas::Images[first + i], Sexy::Rect(x, y, 21, 18),
            Sexy::Rect(0,0,42,36));
    }
}
}

const void* Board::TouchHitTest(int x, int y)
{
    if (mCursorObject->mCursorType == CURSOR_TYPE_COBCANNON_TARGET)
        ClearCursor(); // Also cancel before routing a touch to Menu/Store.
    RefreshHudLayout();
    if (CanInteractWithBoardButtons())
    {
        if (mMenuButton->HitTest(x, y, this)) return mMenuButton;
        if (mStoreButton && mStoreButton->HitTest(x, y, this)) return mStoreButton;
    }
    return this;
}

void Board::TouchBegan(const Sexy::TouchEvent& event)
{
    Sexy::WidgetLifetimeHold lifetime(*this);
    if (FindContact(*this, event.id)) return;
    if (mCursorObject->mCursorType == CURSOR_TYPE_COBCANNON_TARGET)
        ClearCursor(); // Touch input cancels the mouse's pending cannon target.
    const auto collection = CollectAlongTouch(*this, event.x, event.y, event.x, event.y);
    if (collection.leftBoard) return;
    if (!mTouchState)
    {
        mTouchState = new BoardTouchState;
        mTouchState->mouseCursor = mCursorObject;
        mTouchState->mousePreview = mCursorPreview;
    }
    // Selection changes owner, never remains selected in two input contexts.
    if (mTouchState->selection && !mTouchState->selection->active &&
        (mTouchState->selection->cursor->mCursorType == CURSOR_TYPE_NORMAL ||
         mTouchState->selection->cursor->mCursorType == CURSOR_TYPE_HAMMER) &&
        mCursorObject->mCursorType != CURSOR_TYPE_NORMAL &&
        mCursorObject->mCursorType != CURSOR_TYPE_HAMMER &&
        mCursorObject->mCursorType != CURSOR_TYPE_COBCANNON_TARGET)
    {
        std::swap(*mCursorObject, *mTouchState->selection->cursor);
        std::swap(*mCursorPreview, *mTouchState->selection->preview);
        mTouchState->mouseCursor = mCursorObject;
        mTouchState->mousePreview = mCursorPreview;
    }
    BoardTouchContact* available = mTouchState->selection;
    if (available && available->active) available = nullptr;
    if (!available)
        for (auto& contact : mTouchState->contacts)
            if (!contact.active) {available = &contact;break;}
    if (available)
    {
        auto& contact = *available;
        contact.id = event.id;
        contact.x = event.x;
        contact.y = event.y;
        contact.startX = event.x;
        contact.startY = event.y;
        contact.active = true;
        contact.cannon = false;
        contact.ignoreUp = false;
        contact.toggleSelectionOnRelease = false;
        contact.fieldCardPress = contact.fieldCardDragged = false;
        contact.collected = collection.collected;
        contact.aquariumFeed = false;
        ++mTouchState->activeCount;
        if (!mTouchState->selection)
        {
            contact.cursor = new CursorObject;
            contact.preview = new CursorPreview;
            mTouchState->selection = &contact;
            if (mCursorObject->mCursorType != CURSOR_TYPE_NORMAL &&
                mCursorObject->mCursorType != CURSOR_TYPE_HAMMER &&
                mCursorObject->mCursorType != CURSOR_TYPE_COBCANNON_TARGET)
            {
                std::swap(*mCursorObject, *contact.cursor);
                std::swap(*mCursorPreview, *contact.preview);
                mTouchState->mouseCursor = mCursorObject;
                mTouchState->mousePreview = mCursorPreview;
            }
        }
        bool selectsCard = false, selectsShovel = false, boardButton = false;
        Coin* fieldCard = nullptr;
        GameObjectType gardenControl = OBJECT_TYPE_NONE;
        if (mTouchState->selection == &contact)
        {
            ContactScope scope(*this, contact);
            ValidateSelection(*this);
            HitResult hit;
            selectsCard = mSeedBank->MouseHitTest(event.x, event.y, &hit);
            selectsShovel = mShowShovel && GetShovelButtonRect().Contains(event.x, event.y);
            boardButton = CanInteractWithBoardButtons() &&
                (mMenuButton->HitTest(event.x, event.y, this) ||
                 (mStoreButton && mStoreButton->HitTest(event.x, event.y, this)));
            if (Interactive(*this) && !boardButton)
                gardenControl = GardenControlAt(*this, event.x, event.y);
            if (Interactive(*this) && !selectsCard && !selectsShovel && !boardButton &&
                gardenControl == OBJECT_TYPE_NONE)
                fieldCard = FieldCardAt(*this, event.x, event.y);
        }
        // An explicitly selectable card covers a cannon; swept sun/money do not.
        if (auto* cannon = fieldCard ? nullptr : ReadyCannonAt(*this, event.x, event.y))
        {
            if (CannonClaimed(*this, *cannon))
            {
                contact.ignoreUp = true;
                return;
            }
            if (!contact.cursor) contact.cursor = new CursorObject;
            if (!contact.preview) contact.preview = new CursorPreview;
            contact.cannon = true;
            {
                ContactScope scope(*this, contact);
                cannon->MouseDown(event.x, event.y, 1);
            }
            UpdateContact(*this, contact);
            return;
        }
        if (mTouchState->selection != &contact)
            return; // Other fingers may aim independent cannons, but never select another card/tool.
        if (mApp->IsWhackAZombieLevel() && !HasLevelAwardDropped() &&
            (contact.cursor->mCursorType == CURSOR_TYPE_NORMAL || contact.cursor->mCursorType == CURSOR_TYPE_HAMMER))
        {
            if (contact.cursor->mReanimCursorID == REANIMATIONID_NULL)
                contact.cursor->CreateHammerAnimation();
            contact.cursor->mCursorType = CURSOR_TYPE_HAMMER;
        }
        {
            ContactScope scope(*this, contact);
            ValidateSelection(*this);
            if (gardenControl != OBJECT_TYPE_NONE)
            {
                HitResult hit{};
                hit.mObjectType = gardenControl;
                // Retain the existing glove/wheelbarrow transfers and Dave dialogs.
                if (mApp->mGameMode == GAMEMODE_LIMBO_ZEN_GARDEN &&
                    mApp->mZenGarden->MouseDownZenGarden(event.x, event.y, 1, &hit))
                    contact.ignoreUp = mIgnoreMouseUp = true;
                else if (gardenControl == OBJECT_TYPE_NEXT_GARDEN)
                {
                    RefreshSeedPacketFromCursor();
                    MouseDown(event.x, event.y, 1);
                    contact.ignoreUp = mIgnoreMouseUp = true;
                }
                else
                {
                    contact.toggleSelectionOnRelease = IsSelectedControl(*this, event.x, event.y);
                    if (!contact.toggleSelectionOnRelease)
                    {
                        RefreshSeedPacketFromCursor();
                        PickUpTool(gardenControl);
                    }
                }
            }
            else if (fieldCard)
            {
                const auto id = static_cast<CoinID>(mCoins.DataArrayGetID(fieldCard));
                contact.toggleSelectionOnRelease = mCursorObject->mCursorType == CURSOR_TYPE_PLANT_FROM_USABLE_COIN &&
                    mCursorObject->mCoinID == id;
                if (fieldCard->mIsBeingCollected && !contact.toggleSelectionOnRelease)
                    contact.ignoreUp = mIgnoreMouseUp = true; // Another input owns the visible top card.
                else if (!contact.toggleSelectionOnRelease)
                {
                    contact.fieldCardPress = true;
                    RefreshSeedPacketFromCursor();
                    fieldCard->MouseDown(event.x, event.y, 1);
                }
                else contact.fieldCardPress = true;
            }
            else
            {
                contact.toggleSelectionOnRelease = IsSelectedControl(*this, event.x, event.y);
                if ((selectsCard || selectsShovel) && !contact.toggleSelectionOnRelease &&
                    mCursorObject->mCursorType != CURSOR_TYPE_NORMAL)
                    RefreshSeedPacketFromCursor(); // All cards and tools share this selection.
                GridItem* pot = nullptr;
                if (!selectsCard && !selectsShovel && !boardButton && Interactive(*this) && mApp->IsScaryPotterLevel() &&
                    mChallenge->mChallengeState != STATECHALLENGE_SCARY_POTTER_MALLETING &&
                    event.x >= LAWN_XMIN && event.y >= LAWN_YMIN)
                    pot = GetGridItemAt(GRIDITEM_SCARY_POT, PixelToGridX(event.x, event.y), PixelToGridY(event.x, event.y));
                if (pot)
                {
                    HitResult hit;
                    hit.mObjectType = OBJECT_TYPE_SCARY_POT;
                    hit.mObject = pot;
                    mChallenge->MouseDown(event.x, event.y, 1, &hit);
                    contact.ignoreUp = mIgnoreMouseUp = true; // This entire gesture belongs to the vase.
                }
                else if (mCursorObject->mCursorType == CURSOR_TYPE_NORMAL ||
                    mCursorObject->mCursorType == CURSOR_TYPE_HAMMER)
                {
                    // Empty aquarium space feeds on a completed tap. A sun
                    // collected above is already absent from MouseHitTest.
                    HitResult hit;
                    if (mApp->mGameMode == GAMEMODE_MINIGAME_ZOMBIQUARIUM &&
                        !MouseHitTest(event.x, event.y, &hit))
                        contact.aquariumFeed = Interactive(*this);
                    else
                        MouseDown(event.x, event.y, 1);
                }
            }
            // A retained selection acts on release. Pressing the lawn must not
            // plant once on down and a second time on up.
        }
        if (contact.active) UpdateContact(*this, contact);
    }
}

void Board::TouchMoved(const Sexy::TouchEvent& event)
{
    Sexy::WidgetLifetimeHold lifetime(*this);
    if (auto* contact = FindContact(*this, event.id))
    {
        const auto collection = CollectAlongTouch(*this, contact->x, contact->y, event.x, event.y);
        if (collection.leftBoard) return;
        contact->collected |= collection.collected;
        if (contact->aquariumFeed && FieldCardDragged(*contact, event.x, event.y))
            contact->aquariumFeed = false;
        contact->x = event.x;
        contact->y = event.y;
        if (contact->fieldCardPress)
            contact->fieldCardDragged = FieldCardDragged(*contact, event.x, event.y);
        if (contact->ignoreUp || !contact->cursor || (contact != mTouchState->selection && !contact->cannon)) return;
        ContactScope scope(*this, *contact);
        ValidateSelection(*this);
        MouseDrag(event.x, event.y);
        // Match the mouse path: refresh visuals once in UpdateGameObjects,
        // after game state has advanced, rather than once per motion sample.
    }
}

void Board::TouchEnded(const Sexy::TouchEvent& event)
{
    Sexy::WidgetLifetimeHold lifetime(*this);
    auto* contact = FindContact(*this, event.id);
    if (!contact) return;
    const auto collection = CollectAlongTouch(*this, contact->x, contact->y, event.x, event.y);
    if (collection.leftBoard)
    {
        // WidgetManager retires the up capture before dispatch, so its
        // CancelTouches cannot cancel this last Board-side contact.
        TouchCanceled(event);
        return;
    }
    contact->collected |= collection.collected;
    contact->x = event.x;
    contact->y = event.y;
    if (contact->fieldCardPress)
        contact->fieldCardDragged = FieldCardDragged(*contact, event.x, event.y);
    if (!contact->cursor || (contact != mTouchState->selection && !contact->cannon))
        { ReleaseContact(*this, *contact); return; }
    const bool cannonGesture = contact->cannon;
    {
        ContactScope scope(*this, *contact);
        ValidateSelection(*this);
        if (Interactive(*this) && !contact->ignoreUp)
        {
            if (contact->aquariumFeed)
            {
                HitResult hit;
                if (!contact->collected && !FieldCardDragged(*contact, event.x, event.y) &&
                    !MouseHitTest(event.x, event.y, &hit))
                    mChallenge->MouseDown(event.x, event.y, 1, &hit);
            }
            else if (contact->fieldCardPress && (!contact->fieldCardDragged || IsSelectedControl(*this, event.x, event.y)))
            {
                // A tap changes selection only, even when the card lies over an empty planting cell.
                if (contact->toggleSelectionOnRelease && !contact->fieldCardDragged && IsSelectedControl(*this, event.x, event.y))
                    RefreshSeedPacketFromCursor();
            }
            else if (!contact->fieldCardPress && contact->toggleSelectionOnRelease && IsSelectedControl(*this, event.x, event.y))
                RefreshSeedPacketFromCursor();
            else if (GardenMode(*this) && GardenControlAt(*this, event.x, event.y) != OBJECT_TYPE_NONE)
            {
                // Releasing on a toolbar is selection only, never a tool use.
                // A carried plant can still be put into the wheelbarrow by dragging.
                if (mApp->mGameMode == GAMEMODE_LIMBO_ZEN_GARDEN &&
                    mCursorObject->mCursorType == CURSOR_TYPE_PLANT_FROM_GLOVE &&
                    GardenControlAt(*this, event.x, event.y) == OBJECT_TYPE_WHEELBARROW)
                {
                    HitResult hit{};
                    hit.mObjectType = OBJECT_TYPE_WHEELBARROW;
                    mApp->mZenGarden->MouseDownZenGarden(event.x, event.y, 1, &hit);
                }
            }
            else if (IsPlantInCursor())
            {
                // A card reserves availability at pickup, but sun is consumed only
                // at commit. Other contacts or the mouse may have spent it since.
                const int cost = GetCurrentPlantCost(mCursorObject->mType, mCursorObject->mImitaterType);
                if (mCursorObject->mCursorType != CURSOR_TYPE_PLANT_FROM_BANK ||
                    mApp->mEasyPlantingCheat || HasConveyorBeltSeedBank() || CanTakeSunMoney(cost))
                {
                    const auto seed = GetSeedTypeInCursor();
                    const int col = PlantingPixelToGridX(event.x, event.y, seed);
                    const int row = PlantingPixelToGridY(event.x, event.y, seed);
                    if (col >= 0 && col < MAX_GRID_SIZE_X && row >= 0 && row < MAX_GRID_SIZE_Y)
                        MouseDownWithPlant(event.x, event.y, 1);
                }
            }
            else if (contact->cannon && mCursorObject->mCursorType == CURSOR_TYPE_COBCANNON_TARGET)
            {
                const long long dx = static_cast<long long>(event.x) - contact->startX;
                const long long dy = static_cast<long long>(event.y) - contact->startY;
                const long long cellWidth = GridToPixelX(1, 0) - GridToPixelX(0, 0);
                if (dx * dx + dy * dy > cellWidth * cellWidth)
                    MouseDownCobcannonFire(event.x, event.y, 1);
            }
            else if (mCursorObject->mCursorType != CURSOR_TYPE_NORMAL &&
                mCursorObject->mCursorType != CURSOR_TYPE_HAMMER &&
                !GetShovelButtonRect().Contains(event.x, event.y) &&
                mApp->mScreenBounds.Contains(event.x + mX, event.y + mY))
            {
                UpdatePointerHighlights(event.x, event.y); // Garden tools consume the current target highlight.
                MouseDownWithTool(event.x, event.y, 1, mCursorObject->mCursorType);
            }
        }
        ReleaseContact(*this, *contact);
    }
    if (cannonGesture) return;
    // Restore the real mouse cursor before menu callbacks can save or pump a
    // nested modal. Release routing uses the event's own coordinates.
    ContactScope releasePosition(*this, *contact, false);
    MouseUp(event.x, event.y, 1);
}

void Board::TouchCanceled(const Sexy::TouchEvent& event)
{
    if (auto* contact = FindContact(*this, event.id))
    {
        if (!contact->cursor || (contact != mTouchState->selection && !contact->cannon))
            { ReleaseContact(*this, *contact); return; }
        ContactScope scope(*this, *contact);
        ValidateSelection(*this);
        if (contact->cannon) ClearCursor();
        ReleaseContact(*this, *contact);
    }
}

void Board::UpdateTouchCursors()
{
    if (!mTouchState || mTouchState->activeCount == 0) return;
    for (auto& contact : mTouchState->contacts)
        if (contact.active && contact.cursor &&
            (&contact == mTouchState->selection || contact.cannon))
            UpdateContact(*this, contact);
}

bool Board::UpdateTouchHighlights()
{
    if (!mTouchState || !mTouchState->selection) return false;
    auto& contact = *mTouchState->selection;
    if (!contact.active || contact.ignoreUp) return false;
    ContactScope scope(*this, contact);
    ValidateSelection(*this);
    UpdatePointerHighlights(contact.x, contact.y);
    return true;
}

void Board::DrawTouchCursors(Sexy::Graphics* g)
{
    if (!mTouchState || mTouchState->activeCount == 0 || mTimeStopCounter > 0) return;
    for (auto& contact : mTouchState->contacts)
        if (contact.active && contact.cursor && contact.cursor->mVisible &&
            contact.cursor->mCursorType != CURSOR_TYPE_NORMAL)
        {
            ContactScope scope(*this, contact);
            Sexy::Graphics local(*g);
            if (contact.cursor->BeginDraw(&local)) contact.cursor->Draw(&local, contact.x, contact.y);
        }
}

void Board::DrawTouchPreviews(Sexy::Graphics* g)
{
    if (!mTouchState || mTouchState->activeCount == 0) return;
    for (auto& contact : mTouchState->contacts)
        if (contact.active && !contact.ignoreUp && contact.preview &&
            (&contact == mTouchState->selection || contact.cannon))
        {
            ContactScope scope(*this, contact);
            Sexy::Graphics local(*g);
            DrawTouchGuides(*this, contact, local);
            if (contact.preview->BeginDraw(&local)) contact.preview->Draw(&local);
        }
}

void Board::DrawTouchSelection(Sexy::Graphics* g)
{
    if (!Interactive(*this)) return;
    const auto& cursor = mTouchState && mTouchState->selection &&
        !mTouchState->selection->cannon &&
        mTouchState->selection->cursor->mCursorType != CURSOR_TYPE_HAMMER &&
        mTouchState->selection->cursor->mCursorType != CURSOR_TYPE_NORMAL ?
        *mTouchState->selection->cursor : *mCursorObject;
    Sexy::Rect bounds;
    const auto gardenTool = GardenMode(*this) ? SelectedGardenTool(cursor.mCursorType) : OBJECT_TYPE_NONE;
    if (gardenTool != OBJECT_TYPE_NONE)
    {
        bounds = GetShovelButtonRect();
        GetZenButtonRect(gardenTool, bounds);
    }
    else if (cursor.mCursorType == CURSOR_TYPE_SHOVEL && mShowShovel)
        bounds = GetShovelButtonRect();
    else if (cursor.mCursorType == CURSOR_TYPE_PLANT_FROM_BANK && mSeedBank->mVisible &&
        cursor.mSeedBankIndex >= 0 && cursor.mSeedBankIndex < mSeedBank->mNumPackets)
    {
        const auto& packet = mSeedBank->mSeedPackets[cursor.mSeedBankIndex];
        bounds = mLandscapeHud ? LandscapeHud::PacketBounds(*this,cursor.mSeedBankIndex) :
            Sexy::Rect(mApp->mScreenBounds.mX - mX + mSeedBank->mX + packet.mX + packet.mOffsetX,
            mSeedBank->mY + packet.mY - mY, packet.mWidth, packet.mHeight);
        // Hit/slot pitch is 48, but the authored card face is 53 pixels high.
        // Frame the full artwork so the bottom corners clear the price label.
        if (mLandscapeHud) bounds.mHeight=53;
    }
    else return;
    Sexy::Graphics local(*g);
    DrawSelectionFrame(local, bounds, mLandscapeHud);
}

void Board::DrawFieldCardSelection(Sexy::Graphics* g, const Coin& coin)
{
    if (!Interactive(*this)) return;
    const auto& cursor = mTouchState && mTouchState->selection &&
        !mTouchState->selection->cannon &&
        mTouchState->selection->cursor->mCursorType != CURSOR_TYPE_HAMMER &&
        mTouchState->selection->cursor->mCursorType != CURSOR_TYPE_NORMAL ?
        *mTouchState->selection->cursor : *mCursorObject;
    if (cursor.mCursorType != CURSOR_TYPE_PLANT_FROM_USABLE_COIN ||
        mCoins.DataArrayTryToGet(cursor.mCoinID) != &coin || coin.mDead)
        return;
    // Keep the frame in its source card's painter order, not the top UI pass.
    Sexy::Graphics local(*g);
    DrawSelectionFrame(local, Sexy::Rect(static_cast<int>(coin.mPosX),
        static_cast<int>(coin.mPosY), coin.mWidth, coin.mHeight));
}

void Board::PrepareMouseSelection()
{
    if (!mTouchState || !mTouchState->selection || mCursorObject != mTouchState->mouseCursor) return;
    auto& contact = *mTouchState->selection;
    if (contact.cannon || contact.cursor->mCursorType == CURSOR_TYPE_NORMAL ||
        contact.cursor->mCursorType == CURSOR_TYPE_HAMMER) return;
    // A mouse command supersedes this plant/tool gesture, but not other cannons.
    contact.ignoreUp = true;
    std::swap(*mCursorObject, *contact.cursor);
    std::swap(*mCursorPreview, *contact.preview);
    mTouchState->mouseCursor = mCursorObject;
    mTouchState->mousePreview = mCursorPreview;
}

void Board::DisposeTouchState()
{
    if (!mTouchState) return;
    ClearTouchSelection();
    for (auto& contact : mTouchState->contacts)
    {
        if (contact.cursor) contact.cursor->Die();
        delete contact.cursor;
        delete contact.preview;
    }
    delete std::exchange(mTouchState, nullptr);
}

void Board::ClearTouchSelection()
{
    if (!mTouchState || !mTouchState->selection) return;
    auto& contact = *mTouchState->selection;
    ContactScope scope(*this, contact);
    ValidateSelection(*this);
    RefreshSeedPacketFromCursor();
    contact.cursor->mVisible = contact.preview->mVisible = false;
}

void Board::ReindexTouchConveyorCards(int removedIndex)
{
    if (!mTouchState || !mTouchState->selection) return;
    const auto reindex = [&](CursorObject& cursor)
    {
        if (cursor.mCursorType != CURSOR_TYPE_PLANT_FROM_BANK ||
            cursor.mSeedBankIndex <= removedIndex) return;
        --cursor.mSeedBankIndex;
        // RemoveSeed makes shifted packets available. A held card remains reserved.
        mSeedBank->mSeedPackets[cursor.mSeedBankIndex].mActive = false;
    };
    reindex(*mTouchState->mouseCursor);
    reindex(*mTouchState->selection->cursor);
}
