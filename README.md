<div align=center>

<img src="extras/banner.png" alt="Banner" width="30%">

</div>
<h1 align=center>NetherSX2 · Switch Port</h1>

A wrapper/port of NetherSX2 to the Nintendo Switch.
It loads the original Android emulator core `libemucore.so`, 
patches it, and runs it inside a minimal Android-like
environment natively.

Everything ships as a **single `NetherSX2.nro`**: it bundles both emulator cores
(Patched `4248` + Classic `3668`), three renderer backends (native Deko3D, OpenGL, Vulkan/NVK)
and each build's data files, and extracts the ones you pick to the SD card at
launch. An SDL cover-art launcher chooses the game, the renderer, and per-game
settings, then chainloads the emulator.

The launcher supports multiple library folders across SD, USB mass storage
(FAT32, exFAT, NTFS), and SMB shares. The Recent, All games, and Favorites tabs
organize the library; press **Y** for a game's menu and use the settings icon
for global options.

The PS2 theme uses an animated background. Classic keeps the same menu colors
and displays the selected game's artwork behind the library. SteamGridDB cover
downloads let you choose a cover and then an optional background. **Settings >
Library & storage > Download covers** automatically downloads missing covers and
backgrounds while keeping existing local artwork.

**L + R + Plus** to open the quick menu for save states, controller rebinding,
selectable per-game PNACH cheat codes, RetroAchievements progress,
frame-limiter control, reset, and exit.
The cheat browser reads `/switch/nxmasters/cheats/<CRC>.pnach`, recognizes PNACH
`[sections]` and legacy `// code name` blocks.

No emulator core, BIOS, or game assets are included in this repository.

### How to install

1. Copy `NetherSX2.nro` into `/switch/` on your SD card.
2. Put your own PS2 BIOS dump into `/switch/nxmasters/bios/`.

The launcher creates the rest of the folder tree on first run:

```
/switch/NetherSX2.nro
/switch/nxmasters/
  .emu/         <- extracted when selected
  bios/         <- your PS2 BIOS dump           (you supply)
  resources/    <- shaders / GameIndex / fonts  (auto-extracted per core)
  covers/       <- cached covers and background artwork
  gamecfg/      <- per-game launcher settings
  lsfg/         <- optional user-supplied Lossless.dll
  forwarders/   <- HOME shortcut launch records
  hidden.ini    <- hidden library entries
  launcher.ini  <- the launcher's saved config
```

### Notes

This will not run in applet/album mode — it needs the full memory of a game
override. Launch it by holding **R** while opening an installed title, or use a
forwarder.

A PS2 BIOS dump is required and must come from your own console; the launcher
warns at startup when `bios/` is empty.

Casual RetroAchievements are available from **Settings > RetroAchievements**.
Sign in once in the launcher, then enable achievements before starting a game.
Successful sign-in stores the returned account token in `launcher.ini`.
Achievement progress and descriptions can be viewed during play from
**L + R + Plus > Achievements**.

Select **Settings > Graphics > Renderer > Deko3D (Native)** to use the native GS
backend. It replaces the core's GPU device and display callbacks with Deko3D
commands. The Deko emulator NRO is linked separately, without EGL, GLES, Vulkan,
NVK, or Gallium driver archives. The SDL launcher uses its own graphics runtime
and exits before the emulator starts.

LSFG 2x Frame Generation is available for Vulkan and Deko3D in
**Settings > Frame Generation**.
You must provide your own `Lossless.dll` at `/switch/nxmasters/lsfg/Lossless.dll`.
LSFG must be enabled before launching a game so its shader session can be
prepared. Frame Generation starts **Off** in every game and must be enabled
manually from **L + R + Plus > Frame Generation**. Skip Duplicate Frames is
automatically enabled for prepared LSFG sessions so only unique game frames
are interpolated.

### How to build

Install the devkitPro Switch toolchain and portlibs:

```sh
pacman -S devkitA64 switch-tools libnx switch-sdl2 switch-sdl2_ttf \
          switch-sdl2_image switch-curl switch-mesa switch-libdrm_nouveau \
          switch-ntfs-3g switch-libpng deko3d
```

Provide a compatible Mesa NVK package under `vulkan/include` and `vulkan/lib`.
Extract the 4248 and 3668 APKs into sibling folders named
`NetherSX2-v2.2n-4248` and `NetherSX2-v2.2n-3668`, or set `CORES_DIR` to the
folder containing them. Alternatively, extract the supplied inputs without
installing the Mesa SDK into the toolchain:

```sh
python3 tools/prepare_build_inputs.py --apk-4248 /path/to/NetherSX2-v2.2n-4248.apk \
  --apk-3668 /path/to/NetherSX2-v2.2n-3668.apk --mesa-sdk /path/to/mesa-sdk.zip
export MESA_SDK_ROOT="$PWD/out/mesa-sdk"
```

CMake, Ninja, Git, Bison, Flex, Python, and Python Mako are required. The build
downloads pinned libsmb2, libusbhsfs, UAM and SPIRV-Cross revisions. Deko3D is
built as an independent emulator NRO and bundled alongside the existing backends.

```sh
./build_all.sh
```

### Credits

* The AetherSX2 / PCSX2 developers for the emulator.
* The NetherSX2 maintainers for the patched Android builds.
* fgsfds for the Switch so-loader groundwork reused here.
* TheOfficialFloW for the original Android so-loader lineage.
* Dantiicu for Switch Vulkan driver.
* Slluxx for IconGrabber.
* [Autorun](https://github.com/autorunhq/autorun) for the launcher design reference
  and sliders settings icon.
* devkitPro and fincs for Deko3D and UAM.
* Dolphin-NX, the Dolphin Emulator Project, and PalindromicBreadLoaf for the
  runtime UAM integration and portable native LSFG library adapted here.

### Support

If you enjoy my work and want to support me :

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/D1D1P2MOG)

### Legal

This project has no affiliation with the AetherSX2 or PCSX2 developers. No BIOS,
game images, or the emulator core are distributed here; you must supply your own
BIOS dump and legally-owned game images. We do not condone piracy.

Unless noted otherwise, the source in this repository is under the MIT License
(see LICENSE). The vendored LSFG-VK subset under `third_party/lsfg-vk` is
GPL-3.0-or-later. The native LSFG library under `third_party/lsfg-deko3d` is also
GPL-3.0-or-later, and its adapted UAM bridge/build integration under
`source/deko3d/compiler` is GPL-2.0-or-later. Their original notices are preserved.
The sliders settings icon from Autorun is LGPL-2.1-or-later; its notice and license are included in
`launcher/romfs/icons/`.
