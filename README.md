[![Build & Test](https://github.com/codethinki/cth_engine/actions/workflows/build-test.yml/badge.svg)](https://github.com/codethinki/cth_engine/actions/workflows/build-test.yml)

# cth_engine
vulkan 1.3 engine in c++26, work in progress, api not stable yet

built and tested with:
- linux: gcc 16, clang 23
- windows: msvc, clang 23

## libraries
- `cvk`: c++ wrapper of the vulkan c api
- `cgfw`: windows (win32, wayland) through [RGFW](https://github.com/ColleagueRiley/RGFW)
- `jvk`: coroutine layer on top of `cvk`: threads, queues, stage graph, object lifetimes

## demos
- `demo/triangle`: triangle with msaa
- `demo/cube`: spinning textured cube

# building

## requirements
- cmake 4.0+, ninja
- [vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` set
- gcc 16, clang 23 or msvc
- a vulkan 1.3 driver
- windows: the [vulkan sdk](https://vulkan.lunarg.com/sdk/home), linux: the packages of the validation layers and shaderc
  - glslc compiles the shaders, debug builds enable the validation layers

## build & test
```bash
git clone --recursive https://github.com/codethinki/cth_engine.git
cd cth_engine
cmake --preset gcc_release
cmake --build out/build/gcc_release
ctest --test-dir out/build/gcc_release
```

## presets
- `gcc_debug`, `gcc_release`, `clang_debug`, `clang_release`
- windows only: `msvc_debug`, `msvc_release`

debug builds enable the validation layers

# license
[don't be a dick](license.txt)
