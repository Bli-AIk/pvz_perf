# v3.3 release validation

Windows Vulkan Release/LTO, Win7 OpenGL Release/LTO and Android ARMv9-A full-LTO builds completed. The final packages are built from the same v3.3 source, with per-platform SO/EXE, atlas/PAK and shader pairing checks.

- 65 selected CTest cases passed under the MSVC environment. The independent complete Vulkan renderer run passed with zero validation errors; its original CTest 60-second limit was insufficient, so the standalone complete run is retained.
- OpenGL font weights and renderer tests passed. This Windows-host test does not replace testing on a physical Windows 7 installation.
- Production runtime suite passed plants, zombies, minigames, puzzles, survival, adventure, hidden modes, garden/tree, intro, loss and complete Almanac previews before reaching the actor-birth assertion. That assertion incorrectly expected a retained on-board zombie to advance during intro despite its intentionally disabled actor update; it was corrected to expect the held phase in that context. The focused 620-context actor-birth run then passed. No production timing change was made for this assertion.
- Focused shared touch selection, cannons, swept pickups, garden tools, ICE3/ICE4 (48 cases), grave generation and HUD seams passed. Settings round-trip and selection occlusion evidence is in ../selection-layer-volume-20261104/.
- Atlas native UVs and CPU particle runtime records are checked against authored pixel rectangles and source pixel digests. Retired GPU particle records must be absent.
- The Android package contains matching native SO, PAK64, runtime shaders, empty scale.json and generated perfect/endless seeds. APK v3 signature and 16-KiB ZIP alignment verified; ADB install -r succeeded, versionName=3.3/versionCode=12, no existing saves replaced.
- Phone was locked during final unattended installation. Process launch was observed, but no claim is made that the final package received interactive gameplay validation while the user slept. Existing historical diag failures were not treated as new failures. Always-on was disabled and the phone returned to Dozing/screen off after use.

## Explicit exclusions

The full 68-case CTest registry was not all green: the original-1073 source audit needs untracked local Ghidra evidence that is absent, and the optional Magpie catalog compile case rejects the upstream Bilateral_Mean kernel's HLSL variable-length array. Neither was labeled passed or silently weakened. The five baked effects used by GPU/pipeline tests compiled and those tests passed; the release only distributes its existing supported Anime4K_Upscale_Denoise_UL and SSimDownscaler entry points. No scope expansion of the external effect compiler was made for this release.

## HDR decision

Android HDR remains uncalibrated on NX769J and is disabled for v3.3 by the user's instruction. Experimental Android HDR calibration/Java layer metadata and HDR output SPIR-V are excluded. Windows Vulkan/DX12 linear RGBA16F scRGB remains, validated separately by numeric channel and desktop WSI tests. No speed or power improvement is inferred from the output format change.

DAT64 stays v4/schema 0x10745 and player details PVZUSR64 v13. Touch state is not serialized. Earlier save/PAK incompatibility boundaries remain documented in the release notes.
