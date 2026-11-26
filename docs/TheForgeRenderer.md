# The Forge renderer (work in progress)

This fork is replacing dhewm3's OpenGL renderer with [The Forge](https://github.com/ConfettiFX/The-Forge),
starting with its Vulkan backend on Windows.

## Current state (step 1)

* The Forge is built from source as part of the dhewm3 CMake build (`neo/sys/cmake/TheForge.cmake`).
* dhewm3's SDL window is created **without** an OpenGL context. The Forge creates a Vulkan
  device and swapchain for it and clears the screen every frame (dark blue-grey).
* The old renderer still runs, but every OpenGL call goes to a do-nothing "null GL"
  (`neo/renderer/forge/NullGL.cpp`), so nothing but the clear color is visible yet.
  The menus, console and game still run, just invisibly. To quit, press Alt+F4, or open the
  (invisible) console with `~` or Shift+Esc and type `quit`.
* ImGui (dhewm3's settings menu) and MSAA are disabled while `FORGE_RENDERER` is on.
* The Windows editor tools (`TOOLS`) and 32-bit builds are not supported with `FORGE_RENDERER`.

Next steps: put the renderer's GL usage (textures, vertex buffers, render state, ARB
programs) behind a small interface, then port the draw passes and shaders to The Forge
until a level renders.

## Building

The Forge is a git submodule in `neo/libs/TheForge` (pointing to the Meowcat285/The-Forge fork):

```
git clone --recurse-submodules https://github.com/Meowcat285/dhewm3.git
# or, in an existing checkout:
git submodule update --init
```

Then build as usual with CMake. Relevant options:

| Option           | Default                | Meaning |
|------------------|------------------------|---------|
| `FORGE_RENDERER` | `ON`                   | Use The Forge (Vulkan) instead of OpenGL. `OFF` builds the classic OpenGL renderer. |
| `FORGE_DEBUG`    | `OFF`                  | Build The Forge in debug mode (Vulkan validation layers if installed, more logging). |
| `THEFORGE_DIR`   | `neo/libs/TheForge`    | Use a different The Forge checkout. |

No Vulkan SDK is needed to build or run: The Forge loads Vulkan at runtime (via volk) and
ships its own Vulkan headers. Running needs a GPU driver with Vulkan support.

* **Windows**: Visual Studio 2019 or 2022, x64 only.
* **Linux**: additionally needs the X11 and Wayland client development headers
  (`libx11-dev`, `libwayland-dev` on Debian/Ubuntu).

## Files

* `neo/sys/cmake/TheForge.cmake`: builds the needed parts of The Forge as a static library.
* `neo/renderer/forge/ForgeConfig.h`: replaces The Forge's `Common_3/Application/Config.h`
  (through `EXTERNAL_CONFIG_FILEPATH`), so the The Forge sources stay unmodified.
  Disables the parts of The Forge dhewm3 doesn't use (Lua, UI, fonts, profiler, ...).
* `neo/renderer/forge/ForgeBackend.{h,cpp}`: creates the Forge renderer and swapchain for
  the SDL window and renders/presents frames. The only dhewm3 file that includes The Forge headers.
* `neo/renderer/forge/NullGL.cpp`: the do-nothing OpenGL implementation the old renderer
  talks to until it's ported.
* `neo/sys/glimp.cpp`: window creation, now calls into `ForgeBackend` when built with `FORGE_RENDERER`.

At runtime The Forge writes its GPU selection rules (`gpu.cfg`) to a `forge/` directory in
dhewm3's save directory and logs to the dhewm3 console (and `dhewm3log.txt`).
