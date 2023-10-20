# ProcPlanets

A small tool to generate 3D planets, using native WebGPU. The goal of this tool is mainly to train myself on WebGPU, various graphics programming techniques, and procedural generation.

## Build

- clone the `glfw`, `glfw3webgpu`, `glm` git submodules to the `external` folder
- clone the `imgui` repo and add to it a `CMakeLists.txt` to it found here: https://eliemichel.github.io/LearnWebGPU/basic-3d-rendering/some-interaction/simple-gui.html#setting-up-imgui

## Features

- Skybox
- Procedural shape and normal generation with noise
- Post-process ocean on a ray-traced sphere
- Triplanar texture mapping
- GUI for tweaking the parameters
  - TODO: texture bombing
  - TODO: BRDF ??
- TODO: Multiple level of details
- TODO: Flight simulator:
  - TODO: Physics and collision detection

## TODOs

More important is on top (or not)

- add some textures ?
  - would be nice to have a drag and drop to test a texture
- atmosphere !!
- ocean shader
  - the transparency of the ocean when there's no light is ugly
  - actual waves ??
- add a slow rotation of the planet: can give a better idea of the of the looks of the shadows
- shadows:
  - how to make the shadows less "blocky" ? is there other methods to do the shadows ?
  - view frustrum of the shadow rendering should change depending on the planet radius (breaks if planet too big)
  - could add percentage-closer filtering
- improve the terrain edition for more "earth-like" aspect
  - plateaux
  - less regular "wavyness" with a lot of flat parts but also some high and some deep parts ?
- add props on the surface
  - trees
  - rocks ??
- add ocean and ocean shader
- compute the planet on the GPU
- add atmosphere
- add LOD
- add flight simulator
  - spawn from camera
  - simple collision with the mesh ?
- add a bike/car drive on the surface ?
- Improve GUI
  - Show a timer of the time taken to build the planet
  - show the current amount of polygons/vertices
- Get rid of the seams between faces ? (They are invisible on high res)
- Could improve the vertex normals computation (maybe not worth the hastle and computations)