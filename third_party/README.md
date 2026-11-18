# Vendored dependencies

| Library | Version | License | What is included |
|---|---|---|---|
| OpenCV | 4.10.0 (official Windows build, `x64/vc16`) | Apache 2.0, see `opencv/LICENSE.txt` | headers, `opencv_world4100.lib`, `opencv_world4100.dll` (Release only, because the Debug DLL is over GitHub's 100 MB limit) |
| cvui | 2.9.0-beta | MIT, see `cvui/LICENSE.md` | `cvui.h` (single header) |
| GoogleTest | 1.15.2 | BSD-3-Clause, see `googletest/LICENSE` | source (docs/CI removed) |

Only the simulator (`watch_sim`) links OpenCV/cvui. The `ap` library has no third-party dependency.

## Runtime files in `dist/`

| File | Source | Why it is redistributable |
|---|---|---|
| `opencv_world4100.dll` | copied from `opencv/bin/` | Apache 2.0 (same blob as above, Git stores it once) |
| `msvcp140*.dll`, `vcruntime140*.dll`, `concrt140.dll` | `VC\Redist\MSVC\<ver>\x64\Microsoft.VC14x.CRT`, picked by CMake `InstallRequiredSystemLibraries` | Listed as *Distributable Code* in the Visual Studio license (app-local deployment) |

Everything else the binary imports (`kernel32`, `user32`, `gdi32`, `ole32`, `comdlg32`, `ws2_32`, Media Foundation, and the UCRT `api-ms-win-crt-*`) is part of Windows 10/11. This was checked with `dumpbin /dependents` on every file in `dist/`, and by running the binary from a copied folder with `PATH` limited to `System32`, which confirmed that every non-Windows module was loaded from that folder.
