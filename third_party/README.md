# Vendored dependencies

| Library | Version | License | What is included |
|---|---|---|---|
| OpenCV | 4.10.0 (official Windows build, `x64/vc16`) | Apache 2.0, see `opencv/LICENSE.txt` | headers, `opencv_world4100.lib`, `opencv_world4100.dll` (Release only, because the Debug DLL is over GitHub's 100 MB limit) |
| cvui | 2.9.0-beta | MIT, see `cvui/LICENSE.md` | `cvui.h` (single header) |
| GoogleTest | 1.15.2 | BSD-3-Clause, see `googletest/LICENSE` | source (docs/CI removed) |

Only the simulator (`watch_sim`) links OpenCV/cvui. The `ap` library has no third-party dependency.
