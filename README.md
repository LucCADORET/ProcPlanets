# webgpu-rtr
A webgpu real time renderer

## Build

- clone the `glfw`, `glfw3webgpu`, `glm` git submodules to the `external` folder
- clone the `imgui` repo and add to it a `CMakeLists.txt` to it found here: https://eliemichel.github.io/LearnWebGPU/basic-3d-rendering/some-interaction/simple-gui.html#setting-up-imgui

## Content

There are various scenes in the `src/scenes` folder that showcase different stuff, just adapt the `Engine.cpp` code to load the stuff you want to see (TODO: be more specific and user friendly)

## TODOs

More important is on top (or not)

- add skybox reflection and not just a random light
- add ocean and ocean shader
- add some textures
  - PBR textures ? trilinear texturing ?
- compute the planet on the GPU
- add atmosphere
- add LOD
- add flight simulator
- Improve GUI: make input/sliders and not just sliders
- Get rid of the seams between faces ?
- Get rid of the unused assets
- Show a timer of the time taken to build the planet