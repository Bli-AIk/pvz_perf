#include "GameViewport.h"

namespace Sexy {
bool SelectLandscapeUI(bool requested, bool mobile, int clientWidth, int clientHeight) {
    return requested && (!mobile || (clientWidth > 0 && clientHeight > 0 &&
        600LL * clientWidth >= 890LL * clientHeight));
}
GameViewport SelectGameViewport(int clientWidth,int clientHeight,bool landscape) {
    auto view=GameViewport::Fit(clientWidth,clientHeight);
    const int extra=view.width-800;
    const int first=std::min(extra,100);
    const int second=std::clamp(extra-100,0,100);
    const int preferredLeft=(landscape?first:second)+std::max(0,extra-200)/2;
    // Once a side exhausts its native artwork, use the remaining other side.
    const int left=std::clamp(preferredLeft,std::max(0,extra-GameViewport::MaximumRightExpansion),
        std::min(extra,GameViewport::MaximumLeftExpansion));
    view.left=-left;
    return view;
}
}
