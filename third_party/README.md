# third_party

- `stb_image.h` — stb_image v2.30 by Sean Barrett (nothings/stb), unmodified, fetched from
  `https://raw.githubusercontent.com/nothings/stb/master/stb_image.h`. Public domain / MIT (the file ends with both
  notices). Used by `src/formats/png.c` (the only file defining `STB_IMAGE_IMPLEMENTATION`) to read PNG files
  that are not 8-bit indexed.
- `stb_image_write.h` — stb_image_write v1.19 by Sean Barrett (nothings/stb), unmodified, fetched from
  `https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h`. Public domain / MIT (the file ends with both
  notices). `core/formats/png.c` (the only file defining `STB_IMAGE_WRITE_IMPLEMENTATION`) uses only its zlib compressor, `stbi_zlib_compress`, to pack the IDAT of the PNGs
  the tools write; the file writers of stb_image_write are not called.
- `imgui/` — Dear ImGui v1.92.9b, **docking** branch, tag `v1.92.9b-docking` (ocornut/imgui), unmodified, fetched from
  `https://raw.githubusercontent.com/ocornut/imgui/v1.92.9b-docking/` (the core `imgui*.cpp/.h`, `imconfig.h`, `imstb_*.h`, `LICENSE.txt`) and
  its `backends/imgui_impl_sdl3.*` and `backends/imgui_impl_sdlrenderer3.*`. MIT (`LICENSES/MIT-imgui.txt`). Only the track editor
  (`editor/`, option `TNFS_EDITOR`) links it, and it is the only C++ in the tree besides `editor/ui.cpp`. Do not move to another tag
  without running the editor's headless test (`tests/editor_headless_track.sh` and `tests/editor_headless_car.sh`); the docking branch is not the stable one.
