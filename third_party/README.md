# third_party

- `stb_image.h` — stb_image v2.30 by Sean Barrett (nothings/stb), unmodified, fetched from
  `https://raw.githubusercontent.com/nothings/stb/master/stb_image.h`. Public domain / MIT (the file ends with both
  notices). Used by `src/formats/png.c` (the only file defining `STB_IMAGE_IMPLEMENTATION`) to read PNG files
  that are not 8-bit indexed.
- `imgui/` — Dear ImGui v1.92.9b, **docking** branch, tag `v1.92.9b-docking` (ocornut/imgui), unmodified, fetched from
  `https://raw.githubusercontent.com/ocornut/imgui/v1.92.9b-docking/` (the core `imgui*.cpp/.h`, `imconfig.h`, `imstb_*.h`, `LICENSE.txt`) and
  its `backends/imgui_impl_sdl3.*` and `backends/imgui_impl_sdlrenderer3.*`. MIT (`LICENSES/MIT-imgui.txt`). Only the track editor
  (`editor/`, option `TNFS_EDITOR`) links it, and it is the only C++ in the tree besides `editor/ui.cpp`. Do not move to another tag
  without running the editor's headless test (`tests/editor_headless.sh`); the docking branch is not the stable one.
