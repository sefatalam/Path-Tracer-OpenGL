# Path Tracer OpenGL

A real-time GPU path tracer built with OpenGL compute shaders. Scene geometry (spheres,
triangles, quads, and loaded meshes) is traced against a BVH entirely on the GPU, with
temporal accumulation while the camera is stationary.

- Lambertian, Metal, Dielectric, and emissive (diffuse light) materials
- Solid-color and image textures
- Mesh loading via Assimp (FBX, etc.)
- BVH acceleration structure built on the CPU, traced in the compute shader
- Free-fly camera with mouse look

## Requirements

Built with [MSYS2](https://www.msys2.org/) UCRT64 on Windows, using CMake + GCC.

Install the toolchain and dependencies:

```sh
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-glfw \
          mingw-w64-ucrt-x86_64-assimp \
          mingw-w64-ucrt-x86_64-zlib \
          mingw-w64-ucrt-x86_64-minizip \
          mingw-w64-ucrt-x86_64-bzip2
```

A GPU/driver supporting OpenGL 4.6 and `GL_ARB_bindless_texture` is required.

## Building

`CMakeLists.txt` links against `glfw3`, `assimp`, `opengl32`, and `gdi32`, and expects
their runtime DLLs and import libraries to be available locally rather than fetching
them itself. Copy the following from your MSYS2 UCRT64 install (`C:/msys64/ucrt64/...`)
into the project:

- Into the project root: `glfw3.dll`, `libassimp-6.dll`, `zlib1.dll`,
  `libminizip-1.dll`, `libbz2-1.dll`
- Into `lib/`: `libglfw3.a`, `libglfw3.dll.a`, `libassimp.dll.a`

Then configure and build:

```sh
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

The post-build step copies the required DLLs next to the built executable
automatically.

Run from the project root (shaders, models, and textures are loaded via relative
paths):

```sh
./build/GLRT.exe
```

## Controls

| Input          | Action              |
|----------------|---------------------|
| `W` / `A` / `S` / `D` | Move camera    |
| Mouse          | Look around         |
| `Space`        | Move up             |
| `Left Shift`   | Move down           |
| Scroll wheel   | Zoom (FOV)          |
| `Esc`          | Quit                |
