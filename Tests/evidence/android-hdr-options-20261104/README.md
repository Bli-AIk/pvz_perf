# Android HDR options and scale template

- Android ARMv9-a Release/LTO and Windows Vulkan Release builds passed; frame diagnostics disabled.
- Java Release compilation and APK assembly passed using the installed GraalVM JDK 25.
- Existing native HDR renderer tests passed, including 140076 PQ test channels, strict format contracts, real Windows HDR10 WSI, resize and live constants; zero validation errors. This does not validate Android display output.
- Generated string validation preserved all 911 original entries; the platform option label is a separate static string, selected in generated .cpp.
- Signed APK v2/v3 and 16 KiB ZIP alignment verified. package-verification.json proves the APK contains the native library, PAK64 and HDR shader from the same Android build tree and the empty scale template.
- Android HDR, unsupported-device uncheck/writeback, and external scale-file extraction still require device verification. No APK was installed and no device saves/settings were modified during this task.
- The default scale template uses multiplier 1 and empty effects. Load-time super-resolution and disk scaling cache remain unimplemented.

## Linear-output follow-up

The NX769J reports HDR10/HLG, 668-nit peak, 334.001-nit average maximum and 0.002-nit minimum. The isolated surface probe exposes 17 format/space pairs including RGBA16F + extended linear sRGB, but no PQ HDR10 pair; changing the root window color mode does not change the list. The isolated probe app was removed after capture.

Production Vulkan and DX12 HDR outputs now use linear RGBA16F. The current native test covers 140076 channels in each of legacy PQ and linear scRGB, with max errors 8.49e-6 and 9.14e-6 respectively, plus real desktop scRGB WSI and resize. The PQ equations are test-only. Original package-verification.json describes the earlier PQ-only test APK, not this follow-up build.

On-device output was confirmed as RGBA16161616F_UBWC / dataspace 0x18410000 with vendor HWC reporting HDR. The user reported a gray/dim screen despite very bright SDR screenshots. Adding Java extended-range layer metadata did not visibly improve it; do not claim this is calibrated. SurfaceFlinger reported a current white point of 549.200012 nits at brightness 1.0, while the app fallback was 203. A 549.2-nit explicit calibration trial reduced the visible gray appearance according to the user, but did not fully fix it; screenshots became even brighter. The next isolation trial keeps this white point and sets HDR strength to zero while retaining linear FP16 output. Its visual result is pending.

## v3.3 release decision

The strength-zero trial still looked gray; normalization against the display's SDR white point looked darker. Returning to SDR restored both game and system UI. The user confirmed that Bilibili HDR also grays the system UI on this device. These observations do not identify a project-only cause and do not establish a calibrated Android HDR path.

By the user's instruction, v3.3 skips Android HDR. Experimental Java layer metadata, calibration overrides and Android HDR shaders are excluded from the release. The option is disabled and retained `hdr=true` settings normalize to false. The empty scale.json shell remains. Windows Vulkan/DX12 linear scRGB is retained with its separately validated desktop contract. Earlier package verification files in this directory describe historical experiments, not the final v3.3 APK; final release verification is recorded separately.
