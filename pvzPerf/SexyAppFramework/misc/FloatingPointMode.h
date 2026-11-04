#pragma once

namespace Sexy {
// Call once on each game-owned thread, before its floating-point work.
void EnableDenormalFlushForCurrentThread() noexcept;
}
