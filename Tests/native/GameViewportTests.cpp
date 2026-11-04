#include "SexyAppFramework/graphics/GameViewport.h"
#include "SexyAppFramework/graphics/vulkan/VulkanViewport.h"
#include "Lawn/Widget/SelectorLayout.h"
#include "Lawn/Widget/SeedChooserLayout.h"
#include "Lawn/Widget/ZombiesWonLayout.h"
#include "Sexy.TodLib/ReanimRenderSemantics.h"
#include "Lawn/ToolTipLayout.h"
#include "Lawn/GardenHudLayout.h"
#include <stdexcept>

using namespace Sexy;

static void Check(bool condition) {
    if (!condition) throw std::runtime_error("Game viewport regression");
}

static void CheckZombiesWonLayout() {
    static_assert(ReanimRenderSemantics::IsFullscreenTrack(ReanimTrackId::fullscreen));
    static_assert(!ReanimRenderSemantics::IsFullscreenTrack(ReanimTrackId::fullscreen2));
    static_assert(!ReanimRenderSemantics::IsFullscreenTrack(ReanimTrackId::ZombiesWon));
    // Retail camera and canvas positions at 800x600.
    Check(ZombiesWonLayout::CameraX(220, 0, 800) == 220);
    Check(ZombiesWonLayout::CanvasX(0, 800, 220) == -220);
    // The widest battle viewport is deliberately asymmetric: [-220,1180].
    Check(ZombiesWonLayout::CameraX(220, -220, 1400) == 0);
    Check(ZombiesWonLayout::CanvasX(-220, 1400, 0) == 80);
    for (int width = 800; width <= 1400; ++width) {
        const auto view = GameViewport::Fit(width, 600).WithinWorld(-220, 1180);
        for (int requested = 0; requested <= 220; ++requested) {
            const int camera = ZombiesWonLayout::CameraX(requested, view.left, width);
            // Both visible edges must stay inside the unchanged world backdrop.
            Check(view.left - camera >= -220);
            Check(view.left + width - camera <= 1180);
            const int canvas = ZombiesWonLayout::CanvasX(view.left, width, camera);
            Check(canvas + camera - view.left == (width - 800) / 2);
        }
        for (const int dialogWidth : {200, 339, 400, 587}) {
            const int x = ZombiesWonLayout::DialogX(view.left, width, dialogWidth);
            const int padding = x - view.left;
            Check(padding == (width - dialogWidth) / 2);
            Check(width - dialogWidth - padding >= padding);
            for (const int draggedX : {x, x - 100, x + 75}) {
                const int menuX = ZombiesWonLayout::MenuX(view.left, width, draggedX);
                Check(draggedX + menuX - view.left + 163 == width - 2);
            }
        }
    }
}

static void CheckGardenToolButtons() {
    // Independent copy of the old per-button scan. Include masks the game
    // cannot currently produce so unlocking another tool cannot hide an error.
    for (int mask = 0; mask < 1024; ++mask) {
        std::array<bool, 10> available{};
        for (int slot = 0; slot < 10; ++slot)
            available[slot] = (mask & (1 << slot)) != 0;
        for (const auto frame : {
            GardenHudLayout::ViewportInBoard(0, 800, 0, 0),
            GardenHudLayout::ViewportInBoard(-300, 1400, -80, 17),
            GardenHudLayout::ViewportInBoard(-220, 1400, 37, -12),
            GardenHudLayout::ViewportInBoard(-133, 1067, 0, 0)}) {
            for (const bool tree : {false, true}) {
                // Exercise every integer fade displacement, including both
                // endpoints. Tree mode never applies the garden fade curve.
                for (int fade = -72; fade <= 0; ++fade) {
                    const int offset = tree ? 0 : fade;
                    const auto buttons = GardenHudLayout::LayoutTools(available, frame, tree, offset);
                    Check(buttons.y == frame.top + offset);
                    Check(buttons.x[9] == frame.left + 564);
                    for (int tool = 0; tool < 10; ++tool) {
                        if (tree) {
                            // CanUseGameObject only admits these two in tree mode.
                            if (tool == 8)
                                Check(buttons.x[tool] == frame.left + 30);
                            continue;
                        }
                        int expected = frame.left + 30;
                        bool unlocked = true;
                        for (int other = 0; other < 10; ++other) {
                            if (other != 8 && !available[other]) {
                                unlocked = false;
                                break;
                            }
                        }
                        if (unlocked)
                            expected = frame.left;
                        for (int other = 0; other < tool; ++other) {
                            if (available[other])
                                expected += 70;
                        }
                        if (tool == 9)
                            expected = frame.left + 564;
                        Check(buttons.x[tool] == expected);
                    }
                }
            }
        }
    }
}

static void CheckGardenHudLayout() {
    constexpr auto original = GardenHudLayout::ViewportInBoard(0, 800, 0, 0);
    static_assert(original.left == 0 && original.right == 800 && original.top == 0);
    static_assert(GardenHudLayout::RightButtonX(628, original) == 628);
    static_assert(GardenHudLayout::RightButtonX(678, original) == 678);
    // Regression path: centered 1400 viewport with the intro's stale Board.x=-80.
    constexpr auto stale = GardenHudLayout::ViewportInBoard(-300, 1400, -80, 0);
    static_assert(stale.left == -220 && stale.right == 1180);
    static_assert(GardenHudLayout::RightButtonX(628, stale) == 1008);
    static_assert(GardenHudLayout::CursorInBoard(400, -80, 25) == 455);
    for (const int width : {800, 801, 999, 1065, 1066, 1067, 1241, 1399, 1400}) {
        const auto view = GameViewport::Fit(width, 600);
        for (const int left : {view.left, std::max(view.left, -220)}) {
            for (const int boardX : {-80, 0, 37}) {
                for (const int boardY : {-12, 0, 17}) {
                    const auto frame = GardenHudLayout::ViewportInBoard(left, width, boardX, boardY);
                    const int drawX = boardX - left;
                    Check(frame.left + drawX == 0 && frame.right + drawX == width);
                    Check(frame.top + boardY == 0);
                    // Locked/unlocked tool banks, Tree Food and Next Garden.
                    for (const int originalX : {0, 30, 70, 140, 210, 350, 490, 564}) {
                        const int x = frame.left + originalX;
                        for (const int fadeY : {0, -36, -72}) {
                            const int y = frame.top + fadeY;
                            Check(x + drawX == originalX && y + boardY == fadeY);
                            for (const int dx : {-1, 0, 69, 70}) {
                                for (const int dy : {-1, 0, 71, 72}) {
                                    const int worldX = x + boardX + dx;
                                    const int worldY = y + boardY + dy;
                                    const int localX = GardenHudLayout::CursorInBoard(worldX, boardX, 0);
                                    const int localY = GardenHudLayout::CursorInBoard(worldY, boardY, 0);
                                    const bool hit = localX >= x && localX < x + 70 &&
                                        localY >= y && localY < y + 72;
                                    Check(hit == (dx >= 0 && dx < 70 && dy >= 0 && dy < 72));
                                }
                            }
                            const int tipX = ToolTipLayout::Position(x + 35, 120, true, frame.left, frame.right);
                            Check(tipX + drawX >= 0 && tipX + drawX + 120 <= width);
                        }
                    }
                    // Main Menu and Shop retain their 800-layout right insets.
                    for (const int originalX : {628, 678}) {
                        const int x = GardenHudLayout::RightButtonX(originalX, frame);
                        const int buttonWidth = originalX == 628 ? 163 : 109;
                        Check(x + drawX == width - (800 - originalX));
                        Check(frame.right - (x + buttonWidth) == (originalX == 628 ? 9 : 13));
                        const int worldMouse = x + boardX + buttonWidth / 2;
                        Check(GardenHudLayout::CursorInBoard(worldMouse, boardX, 0) == x + buttonWidth / 2);
                        Check(frame.right + drawX == width); // Shop's hidden endpoint.
                    }
                    for (const int pixelX : {0, 1, width / 2, width - 1}) {
                        for (const int pixelY : {0, 35, 300, 599}) {
                            const int mouseX = pixelX + left;
                            const int cursorX = GardenHudLayout::CursorInBoard(mouseX, boardX, 25);
                            const int cursorY = GardenHudLayout::CursorInBoard(pixelY, boardY, 35);
                            Check(cursorX + drawX == pixelX - 25);
                            Check(cursorY + boardY == pixelY - 35);
                            // Normal watering can's original image grab point.
                            Check(cursorX + drawX - 3 == pixelX - 28);
                            Check(cursorY + boardY + 12 == pixelY - 23);
                            // BeginDraw may contain the prior tick's cursor;
                            // the garden Draw adapter replaces only that origin.
                            const int oldX = 100, oldY = 200;
                            Check(drawX + oldX + (cursorX - oldX) == pixelX - 25);
                            Check(boardY + oldY + (cursorY - oldY) == pixelY - 35);
                        }
                    }
                    for (const int gridX : {113, 306, 669}) {
                        const int worldMouse = gridX + boardX + 40;
                        Check(GardenHudLayout::CursorInBoard(worldMouse, boardX, 0) == gridX + 40);
                    }
                }
            }
        }
    }
}

int main() {
    for (const int height : {600, 900, 1200}) {
        const auto checkStartup = [height](int monitorWidth,int monitorHeight,int logicalWidth) {
            const auto size=WindowStartupSize::FromMonitor(monitorWidth,monitorHeight,height);
            Check(size.height==height);
            Check(size.width==int((logicalWidth*1LL*height+300)/600));
        };
        checkStartup(1600,1200,800);
        checkStartup(1920,1080,1067);
        checkStartup(1920,1200,960);
        checkStartup(2520,1080,1400);
        checkStartup(3440,1440,1400);
        checkStartup(1080,1920,800);
    }
    Check(WindowStartupSize::FromMonitor(1920,1080,0).height==600);
    CheckZombiesWonLayout();
    CheckGardenToolButtons();
    CheckGardenHudLayout();
    // The legacy X/Y paths truncate at different points. Compare their actual
    // helpers with the original formulas, including fractional graphics shifts.
    for (const float translation : {-380.5f, -380.0f, -133.5f, -0.5f, 0.0f, 0.5f, 87.0f, 87.5f, 220.5f}) {
        for (const int minimum : {0, 459}) {
            for (const int extent : {1, 120, 900}) {
                for (const bool centered : {false, true}) {
                    for (const int anchor : {-1000, 0, 500, 1500}) {
                        int legacy = anchor - (centered ? extent / 2 : 0);
                        if (minimum - translation > legacy)
                            legacy = minimum - static_cast<int>(translation);
                        else if (legacy + extent + translation > 800)
                            legacy = static_cast<int>(800 - translation - extent);
                        Check(ToolTipLayout::LegacyX(anchor, extent, centered, minimum, 800, translation) == legacy);
                        if (translation == static_cast<int>(translation))
                            Check(ToolTipLayout::Position(anchor, extent, centered,
                                minimum - static_cast<int>(translation), 800 - static_cast<int>(translation)) == legacy);
                    }
                }
            }
        }
        for (const int bottom : {503, 570, 600}) {
            for (const int height : {1, 120, 900}) {
                for (const int anchor : {-1000, 0, 500, 1500}) {
                    int legacy = anchor;
                    if (-translation > legacy)
                        legacy = static_cast<int>(-translation);
                    else if (bottom < anchor + height + translation)
                        legacy = bottom - static_cast<int>(translation) - height;
                    Check(ToolTipLayout::LegacyY(anchor, height, bottom, translation) == legacy);
                }
            }
        }
    }
    static_assert(ToolTipLayout::LegacyX(800, 120, false, 0, 800, 0.5f) == 679);
    static_assert(ToolTipLayout::LegacyY(600, 120, 600, 0.5f) == 480);
    for (const int width : {800, 801, 999, 1065, 1066, 1067, 1241, 1399, 1400}) {
        const auto view = GameViewport::Fit(width, 600);
        const int chooserX = view.left;
        const int right = view.left + width;
        for (const int buttonX : {154, 332}) {
            const int buttonY = buttonX == 154 ? 545 : 546;
            const int buttonWidth = buttonX == 154 ? 156 : 100;
            const int buttonHeight = buttonX == 154 ? 42 : 30;
            for (const int chooserY : {-516, -400, -100, 0, 50, 516}) {
                for (const int dx : {-1, 0, buttonWidth - 1, buttonWidth}) {
                    for (const int dy : {-1, 0, buttonHeight - 1, buttonHeight}) {
                        const int worldX = chooserX + buttonX + dx;
                        const int worldY = chooserY + buttonY + dy;
                        // Start/Random now use their chooser parent in both
                        // IsMouseOver and Update, matching the draw transform.
                        const int localX = SeedChooserLayout::RebaseX(worldX, 0, chooserX);
                        const int localY = worldY - chooserY;
                        Check(localX == buttonX + dx && localY == buttonY + dy);
                        const bool hit = localX >= buttonX && localX < buttonX + buttonWidth &&
                            localY >= buttonY && localY < buttonY + buttonHeight;
                        Check(hit == (dx >= 0 && dx < buttonWidth && dy >= 0 && dy < buttonHeight));
                    }
                }
            }
        }
        for (const int originalX : {560, 680}) {
            const int buttonX = SeedChooserLayout::RightButtonX(originalX, width);
            Check(right - (chooserX + buttonX) == 800 - originalX);
            // GameButton hover subtracts its parent origin; MouseDown already
            // receives chooser-local coordinates. Both must select the same box.
            for (const int offset : {-1, 0, 55, 110}) {
                const int mouseWorld = chooserX + buttonX + offset;
                const int hoverX = SeedChooserLayout::RebaseX(mouseWorld, 0, chooserX);
                Check(hoverX == buttonX + offset);
                const int oldOrigin = 0;
                const int clickBeforeResize = mouseWorld - oldOrigin;
                const int clickX = SeedChooserLayout::RebaseX(clickBeforeResize, oldOrigin, chooserX);
                Check(clickX == hoverX);
                Check((hoverX >= buttonX && hoverX < buttonX + 110) == (offset >= 0 && offset < 110));
            }
        }
        for (const int packetX : {22, 73, 124, 277, 481}) {
            // Bank coordinates and chooser packet storage remain canonical.
            // Rendering must add the left viewport origin exactly once.
            const int bankX = 10 + packetX;
            for (const int drawingOrigin : {0, chooserX}) {
                const int bankLocal = SeedChooserLayout::RebaseX(bankX, view.left, drawingOrigin);
                Check(bankLocal + drawingOrigin == view.left + bankX);
            }
            const int bankLocal = SeedChooserLayout::RebaseX(bankX, view.left, chooserX);
            Check(bankLocal == bankX);
            Check(SeedChooserLayout::RebaseX(chooserX + bankX + 25, 0, chooserX) == bankLocal + 25);
            const int bankTip = ToolTipLayout::Position(bankLocal - 35, 120, false, 0, width);
            Check(bankTip + chooserX == view.left + std::clamp(bankX - 35, 0, width - 120));
            for (const int chooserPacket : {22, 393, 464}) {
                for (int weight = 0; weight <= 8; ++weight) {
                    // Rebasing is affine for any eased flight progress, both
                    // chooser-to-bank and bank-to-chooser (including Imitater).
                    const int forward = ((8 - weight) * chooserPacket + weight * bankX) / 8;
                    const int reverse = ((8 - weight) * bankX + weight * chooserPacket) / 8;
                    Check(SeedChooserLayout::RebaseX(forward, view.left, chooserX) + chooserX ==
                        view.left + forward);
                    Check(SeedChooserLayout::RebaseX(reverse, view.left, chooserX) + chooserX ==
                        view.left + reverse);
                }
            }
        }
        for (const int camera : {-380, -247, -100, 0, 87, 220}) {
            const int panelWidth = 459;
            const int tooltipWidth = 120;
            const int minLocal = SeedChooserLayout::RebaseX(panelWidth, chooserX, camera);
            const int maxLocal = SeedChooserLayout::RebaseX(width, view.left, camera);
            Check(minLocal + camera == chooserX + panelWidth);
            Check(maxLocal + camera == right);
            for (const int worldAnchor : {chooserX + panelWidth, right - 20, right + 20}) {
                const int boardAnchor = SeedChooserLayout::RebaseX(worldAnchor, 0, camera);
                const int clickInChooser = SeedChooserLayout::RebaseX(worldAnchor, 0, chooserX);
                Check(SeedChooserLayout::RebaseX(clickInChooser, chooserX, camera) == boardAnchor);
                const int localX = ToolTipLayout::Position(boardAnchor, tooltipWidth, true, minLocal, maxLocal);
                const int worldX = localX + camera;
                Check(worldX >= chooserX + panelWidth && worldX + tooltipWidth <= right);
                Check(worldX == std::clamp(worldAnchor - tooltipWidth / 2,
                    chooserX + panelWidth, right - tooltipWidth));
                // Actual Board graphics includes camera - viewport.left.
                const int pixelX = localX + camera - view.left;
                Check(pixelX >= panelWidth && pixelX + tooltipWidth <= width);
                if (width > 800 && worldAnchor >= right - 20)
                    Check(pixelX + tooltipWidth > 800);
            }
        }
    }
    static_assert(SeedChooserLayout::RightButtonX(560, 800) == 560);
    static_assert(SeedChooserLayout::RebaseX(47, -133, -133) == 47);
    static_assert(SeedChooserLayout::RebaseX(47, -133, 0) == -86);
    static_assert(SeedChooserLayout::RightButtonX(680, 1066) - 133 == 813);
    static_assert(ToolTipLayout::Position(1160, 120, true, 573, 1180) == 1060);
    static_assert(GameViewport::Fit(800, 600).width == 800);
    static_assert(GameViewport::Fit(1600, 1000).left == -80);
    static_assert(GameViewport::Fit(1920, 1080).width == 1067);
    static_assert(GameViewport::Fit(3440, 1440).left == -300);
    static_assert(GameViewport::Fit(1400, 600).WithinWorld(-220, 1180).left == -220);
    static_assert(GameViewport::Fit(800, 600).WithinWorld(-220, 1180).left == 0);
    static_assert(GameViewport::Fit(1240, 600).WithinWorld(-220, 1180).left == -220);
    static_assert(GameViewport::Fit(600, 1000).width == 800);
    static_assert(SelectorLayout::ZombatarSlideDistance(0, 800) == 800);
    static_assert(SelectorLayout::ZombatarSlideDistance(-133, 1066) == 667);
    static_assert(SelectorLayout::ZombatarSlideDistance(-300, 1400) == 500);
    static_assert(WindowTrackLimits::FromScaleOutputHeight(0).height == 600);
    static_assert(WindowTrackLimits::FromScaleOutputHeight(-1).minimumWidth == 800);
    static_assert(WindowTrackLimits::FromScaleOutputHeight(1200).maximumWidth == 2800);
    static_assert(GameViewport::Fit(1001, 750).width == 801);
    for (const int height : {600, 601, 750, 900, 1000, 1200, 1500}) {
        const auto limits = WindowTrackLimits::FromScaleOutputHeight(height);
        Check(limits.height == height);
        Check(limits.ConstrainWidth(-100) == limits.minimumWidth);
        Check(limits.ConstrainWidth(0) == limits.minimumWidth);
        Check(limits.ConstrainWidth(100000) == limits.maximumWidth);
        int previousWidth = 0;
        for (int logicalWidth = 800; logicalWidth <= GameViewport::MaximumWidth; ++logicalWidth) {
            const int pixels = int((logicalWidth * 1LL * height + 300) / 600);
            Check(pixels > previousWidth);
            Check(limits.ConstrainWidth(pixels) == pixels);
            Check(GameViewport::Fit(pixels, height).width == logicalWidth);
            previousWidth = pixels;
        }
        // Dragging through every physical pixel must stay bounded and settle
        // without another width change when the scale pipeline is rebuilt.
        for (int pixels = limits.minimumWidth - 20; pixels <= limits.maximumWidth + 20; ++pixels) {
            const int constrained = limits.ConstrainWidth(pixels);
            Check(constrained >= limits.minimumWidth && constrained <= limits.maximumWidth);
            Check(limits.ConstrainWidth(constrained) == constrained);
            const auto view = GameViewport::Fit(constrained, height);
            Check(view.height == 600 && view.width >= 800 && view.width <= GameViewport::MaximumWidth);
            Check(int((view.width * 1LL * height + 300) / 600) == constrained);
        }
    }
    for (int width = 800; width <= GameViewport::MaximumWidth; ++width) {
        const auto view = GameViewport::Fit(width, 600);
        Check(view.width == width);
        const int pan = SelectorLayout::ZombatarSlideDistance(view.left, view.width);
        Check(pan >= 500 && pan <= 800);
        Check(pan + view.left + view.width == 1600);
        for (int camera = 0; camera <= pan; ++camera) {
            Check(camera + view.left >= -300);
            Check(camera + view.left + view.width <= 1600);
        }
        const int leftPad = -view.left;
        const int rightPad = width - 800 - leftPad;
        Check(rightPad == leftPad || rightPad == leftPad + 1);
        const auto battle = view.WithinWorld(-220, 1180);
        Check(battle.left >= -220 && battle.left + battle.width <= 1180);
        const int cameraMinimum = battle.left + battle.width - 1180;
        const int cameraMaximum = battle.left + 220;
        Check(cameraMinimum <= cameraMaximum);
        for (const int worldX : {0, 40, 399, 799}) {
            const auto point = WindowPointToScene(worldX - battle.left + 0.5, 300.5,
                width, 600, width, 600, {0, 0, width, 600}, width, 600);
            Check(point.x + battle.left == worldX && point.y == 300);
        }
        for (int worldX = 0; worldX < 800; ++worldX) {
            const auto point = WindowPointToScene(worldX + leftPad + 0.5, 300.5,
                width, 600, width, 600, {0, 0, width, 600}, width, 600);
            Check(point.x + view.left == worldX && point.y == 300);
        }
    }
    for (const auto size : {GpuPoint{800,600}, {801,600}, {999,600},
                            {1065,600}, {1400,600}, {1399,600}, {960,600}, {1920,1080},
                            {3440,1440}, {601,1000}, {1599,901}}) {
        const auto view = GameViewport::Fit(size.x, size.y);
        const auto fit = FitSceneToSurface(view.width, view.height, size.x, size.y);
        Check(view.left == -(view.width - 800) / 2);
        const auto center = WindowPointToScene(fit.x + fit.width / 2.0,
            fit.y + fit.height / 2.0, size.x, size.y, size.x, size.y,
            fit, view.width, view.height);
        Check(center.x + view.left == 400 && center.y == 300);
        for (const int worldX : {0, 40, 399, 799}) {
            const double windowX = fit.x +
                (worldX - view.left + 0.5) * fit.width / view.width;
            const auto point = WindowPointToScene(windowX,
                fit.y + 300.5 * fit.height / view.height,
                size.x, size.y, size.x, size.y, fit, view.width, view.height);
            Check(point.x + view.left == worldX && point.y == 300);
        }
        const auto outside = WindowPointToScene(fit.x - 1, fit.y - 1,
            size.x, size.y, size.x, size.y, fit, view.width, view.height);
        Check(outside.x + view.left < view.left && outside.y < 0);
    }
    for (const int height : {600, 900, 1200}) {
        auto limits=WindowTrackLimits::FromScaleOutputHeight(height);
        limits.minimumWidth=int((890LL*height+300)/600);
        Check(limits.ConstrainWidth(1)==limits.minimumWidth);
        Check(limits.ConstrainWidth(limits.minimumWidth)==limits.minimumWidth);
        const auto view=SelectGameViewport(limits.minimumWidth,height,true);
        Check(view.width==890 && view.left==-90);
    }
    Check(!SelectLandscapeUI(false,false,1920,1080));
    Check(!SelectLandscapeUI(true,true,800,600));
    Check(!SelectLandscapeUI(true,true,889,600));
    Check(SelectLandscapeUI(true,true,890,600));
    Check(!SelectLandscapeUI(true,true,1779,1200));
    Check(SelectLandscapeUI(true,true,1780,1200));
    Check(SelectLandscapeUI(true,true,3200,2136));
    Check(SelectLandscapeUI(true,true,1500,1000));
    Check(SelectLandscapeUI(true,true,2400,1080));
    Check(!SelectLandscapeUI(true,true,0,0));
    Check(SelectLandscapeUI(true,false,800,600));
    const auto fullscreen=FitSceneToSurface(960,600,800,600);
    Check(fullscreen.x==0&&fullscreen.y==50&&fullscreen.width==800&&fullscreen.height==500);
    const auto phone=GameViewport::Fit(2480,1116);
    const auto world=phone.WithinWorld(-220,1180);
    const int restX=phone.left-world.left;
    Check(phone.width==1333&&phone.left==-266&&restX==-46);
    const auto full=GameViewport::Fit(2520,1080);
    const auto fullWorld=full.WithinWorld(-220,1180);
    Check(full.width==1400&&full.left-(full.left-fullWorld.left)==-220);
    Check(full.left+full.width-(full.left-fullWorld.left)==1180);
    const auto ultra=GameViewport::Fit(3000,1200);
    const auto pillarbox=FitSceneToSurface(ultra.width,ultra.height,3000,1200);
    Check(ultra.width==1400&&pillarbox.x==100&&pillarbox.width==2800&&pillarbox.y==0);
    for(const bool horizontal:{false,true}) {
        for(int width=800;width<=1400;++width) {
            const auto view=SelectGameViewport(width,600,horizontal);
            Check(view.width==width);
            const int left=-view.left,right=width-800-left;
            if(width<=900)Check(horizontal?(left==width-800&&right==0):(right==width-800&&left==0));
            else if(width<=1000)Check(horizontal?(left==100&&right==width-900):(right==100&&left==width-900));
            else if(width<=1240)Check(right==left||right==left+1);
            else Check(left==220&&right==width-1020);
        }
    }
    const auto nativeMinimum=FitSceneToSurface(890,600,800,600);
    Check(nativeMinimum.width==800&&nativeMinimum.height==539&&nativeMinimum.y==30);
    bool rejected = false;
    try { (void)GameViewport::Fit(0, 600); }
    catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected);
}
