#pragma once

// Install core 1.0 implementations for the renderer's internal sync2-shaped calls.
// No 1.1+ entry point is imported or passed to the legacy driver.
void InstallVulkan10Commands();
