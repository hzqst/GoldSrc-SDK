<p align="center">
  <img src="media/goldsrc-sdk.png" alt="GoldSrc-SDK" width="1000">
</p>

<h1 align="center">GoldSrc-SDK</h1>

<p align="center">
  <strong>Half-Life 25th Anniversary engine, games, and Hammer 3.4 source.</strong><br>
  Visual Studio 2026 &middot; MSVC v145 &middot; Win32
</p>

GoldSrc-SDK brings together the Half-Life 25th Anniversary engine code, game client and server code, shared runtime libraries, SDK tools, and Hammer 3.4 source. The engine, games, and SDK utilities are organized into Visual Studio 2026 solutions using the MSVC v145 toolset.

## What's included

- **GoldSrc engine:** hardware and software rendering, the game launcher, and shared engine systems.
- **Game code:** client and server projects for the games listed below.
- **Servers and broadcasting:** Half-Life Dedicated Server, the dedicated engine, HLTV, and demo playback components.
- **Runtime libraries:** the filesystem, Tier0, VGUI, VGUI2, GameUI, the server browser, and particle manager.
- **A3D audio API:** 3D audio interfaces and implementation, built as `a3dapi.dll`.
- **Hammer 3.4:** editor source.
- **SDK utilities:** map compilation, model compilation and viewing, sprite generation, WAD tools, and other development utilities.

## Included games

| Game | Runtime directory |
| --- | --- |
| Half-Life | `valve` |
| Half-Life: Opposing Force | `gearbox` |
| Half-Life: Blue Shift | `bshift` |
| Counter-Strike | `cstrike` |
| Counter-Strike: Condition Zero | `czero` |
| Counter-Strike: Condition Zero — Deleted Scenes | `czeror` |
| Team Fortress Classic | `tfc` |
| Day of Defeat | `dod` |
| Deathmatch Classic | `dmc` |
| Ricochet | `ricochet` |

## Building

### Requirements

- **Visual Studio 2026** with the **Desktop development with C++** workload.
- **MSVC v145** C++ build tools.
- **Windows SDK 10.0.26100.0**.
- The dependency headers and libraries supplied under [`external/`](GoldSrc-SDK/external/) and [`lib/`](GoldSrc-SDK/lib/).

The build targets **Win32 (x86)**, with **Debug** and **Release** configurations.

### Visual Studio

1. Open [`projects/vs2026/projects.sln`](GoldSrc-SDK/projects/vs2026/projects.sln).
2. Select **Release** or **Debug**, and **Win32** as the platform.
3. Build the desired projects or the solution.
4. Open [`projects/vs2026/utils.sln`](GoldSrc-SDK/projects/vs2026/utils.sln) to build the SDK utilities.

### Command line

Run these commands from the repository root in the **Developer Command Prompt for VS 2026**:

```bat
cd GoldSrc-SDK
msbuild projects\vs2026\projects.sln /m /p:Configuration=Release /p:Platform=Win32
msbuild projects\vs2026\utils.sln /m /p:Configuration=Release /p:Platform=Win32
```

Use `/p:Configuration=Debug` for a debug build.

### CMake

[`CMakeLists.txt`](GoldSrc-SDK/CMakeLists.txt) builds the same targets with presets for both platforms. The Visual Studio 2026 generator needs CMake 4.2 or later.

```bat
cd GoldSrc-SDK
cmake --preset windows-x86
cmake --build --preset windows-x86-release
```

On Linux, the build needs CMake, Ninja and a GCC with 32-bit multilib support (`g++-multilib` on Debian and Ubuntu). The engines also need [vcpkg](https://github.com/microsoft/vcpkg): set `VCPKG_ROOT`, and the configure step builds the libraries listed in [`vcpkg.json`](GoldSrc-SDK/vcpkg.json) with the [`x86-linux`](GoldSrc-SDK/cmake/triplets/x86-linux.cmake) triplet. Building them needs `autoconf`, `autoconf-archive`, `automake`, `libtool` and `nasm`. Without `VCPKG_ROOT`, everything except the engines is built.

```bash
cd GoldSrc-SDK
export VCPKG_ROOT=~/vcpkg
cmake --preset linux-x86-release      # or linux-x86-debug
cmake --build --preset linux-x86-release
```

Linux binaries go to `bin/Linux32/<config>/`. The Linux build covers:

- The engine (`hw.so`), the dedicated server engine (`engine_i486.so`) and their launchers `hl_linux`, `hlds_linux` (text console) and `hltv`.
- Tier0 (`libtier0.so`), VGUI2 (`vgui2.so`), the filesystem, HLTV (`core.so`, `demoplayer.so`, `proxy.so`), the server browser (`platform/servers/serverbrowser_linux.so`), GameUI (`valve/cl_dlls/gameui.so`) and particleman.
- The client and server libraries of every included game. The server libraries are `hl.so`, `opfor.so`, `bshift.so`, `cs.so` (`cstrike` and `czero`), `cz.so`, `tfc.so`, `dod.so`, `dmc.so` and `mp.so` (`ricochet`).

The software renderer, the A3D API, Hammer and the SDK utilities are still Windows-only. On Linux the engine has no MP3 music, because the Miles Sound System it uses is not available ([`engine/miles_null.cpp`](GoldSrc-SDK/engine/miles_null.cpp)).

Like the Windows `swds.dll`, which is the full software engine, `engine_i486.so` is the full OpenGL engine run in dedicated mode. It therefore needs libGL, SDL2 and `vgui.so` to load, even on a server without a display. The HLTV console has no VGUI window on Linux.

Like Valve's Linux `vgui2.so`, the Linux VGUI2 has no surface or fonts of its own: the engine supplies the surface, and the clipboard goes through SDL2.

On Linux, `serverbrowser_linux.so` leaves the Tier0 symbols (`Plat_FloatTime`, `Error`) for the game's runtime libraries to resolve. Steam API modules link [`lib/public/linux32/libsteam_api.so`](GoldSrc-SDK/lib/public/linux32/), taken from the Linux Half-Life Dedicated Server (SteamCMD app 90). [`linux/libSDL2-2.0.so.0`](GoldSrc-SDK/linux/) is SDL 2.32.10 built with `vcpkg install --classic "sdl2[core,x11]" --triplet x86-linux --overlay-triplets=cmake/triplets` (X11, PulseAudio and PipeWire backends, loaded at run time). The build copies these libraries next to the binaries.

Configure with `-DGOLDSRC_BUILD_TESTS=ON` to register the regression tests under [`tests/`](tests/) with CTest; run them with `ctest --test-dir bin/obj/cmake/<preset>`.

## Build output

Runtime and game binaries are placed under `GoldSrc-SDK/`:

Copy the contents of `GoldSrc-SDK/deploy/platform/` into the game's `platform/` directory to install the accompanying UI resources, including the Career error translations.

```text
bin/Win32/Release/
bin/Win32/Debug/
```

SDK utility executables are placed in the corresponding `utils/` subdirectory. Intermediate build files are kept under `bin/obj/`.

| Component | Output |
| --- | --- |
| Game launcher | `hl.exe` |
| Dedicated server launcher | `hlds.exe` |
| HLTV launcher | `hltv.exe` |
| Hardware-rendered engine | `hw.dll` |
| Software-rendered engine | `sw.dll` |
| Dedicated server engine | `swds.dll` |
| A3D audio API | `a3dapi.dll` |
| HLTV and demo support | `Core.dll`, `DemoPlayer.dll`, `proxy.dll` |
| Filesystem | `filesystem_stdio.dll` |
| Shared runtime and UI | `tier0.dll`, `vgui.dll`, `vgui2.dll` |
| Game UI and effects | `valve/cl_dlls/GameUI.dll`, `valve/cl_dlls/particleman.dll` |
| Server browser | `platform/servers/serverbrowser.dll` |
| Game client/server DLLs | The game directories listed above, under `cl_dlls/` and `dlls/` |

## Source layout

The project files are in [`GoldSrc-SDK/`](GoldSrc-SDK/). Paths below are relative to that directory.

| Path | Contents |
| --- | --- |
| [`engine/`](GoldSrc-SDK/engine/) | Engine implementation |
| [`swds/`](GoldSrc-SDK/swds/) | Dedicated server engine project and platform support sources; shares engine code from `engine/` |
| [`a3dapi/`](GoldSrc-SDK/a3dapi/) | A3D audio API implementation |
| [`launcher/`](GoldSrc-SDK/launcher/) | Game launcher |
| [`dedicated/`](GoldSrc-SDK/dedicated/) | Dedicated server launcher |
| [`HLTV/`](GoldSrc-SDK/HLTV/) | HLTV, proxy, core, and demo playback code |
| [`cl_dll/`](GoldSrc-SDK/cl_dll/), [`dlls/`](GoldSrc-SDK/dlls/) | Half-Life client and server code |
| [`common/`](GoldSrc-SDK/common/), [`public/`](GoldSrc-SDK/public/), [`game_shared/`](GoldSrc-SDK/game_shared/), [`pm_shared/`](GoldSrc-SDK/pm_shared/) | Shared interfaces, game code, and movement code |
| [`filesystem/`](GoldSrc-SDK/filesystem/), [`tier0/`](GoldSrc-SDK/tier0/), [`vgui2/`](GoldSrc-SDK/vgui2/), [`GameUI/`](GoldSrc-SDK/GameUI/) | Filesystem, runtime support, and UI components |
| [`serverbrowser/`](GoldSrc-SDK/serverbrowser/) | Server browser (`ServerBrowser003`), built by `serverbrowser.vcxproj` or `linux/Makefile.serverbrowser` |
| [`worldcraft/`](GoldSrc-SDK/worldcraft/) | Hammer 3.4 editor source |
| [`utils/`](GoldSrc-SDK/utils/) | SDK utilities and VGUI source |
| [`projects/vs2026/`](GoldSrc-SDK/projects/vs2026/) | Visual Studio solutions, project files, and shared build settings |
| [`external/`](GoldSrc-SDK/external/), [`lib/`](GoldSrc-SDK/lib/) | Dependency headers and libraries |

Counter-Strike and Condition Zero share source through the projects under `projects/vs2026/`; `czero` is a runtime output directory rather than a separate top-level source folder.

## Credits

Original engine and game technology by Valve and the respective game developers, including Gearbox Software, Ritual Entertainment, and the original mod teams.

GoldSrc-SDK is a community project. Half-Life, Counter-Strike, and other game names and trademarks belong to their respective owners. Preserve the copyright and license notices supplied with each source component and dependency.
