#pragma once
#include <functional>

class LawnApp;

void CheckOpeningFlows(LawnApp& app, std::function<bool(bool*)>& nestedPump,
    void (*processDeletes)(LawnApp&));
