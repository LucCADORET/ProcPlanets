# ProcPlanets

Create high-scale planets.

## Build

- clone the `glfw`, `glfw3webgpu`, `glm` git submodules to the `external` folder
- clone the `imgui` repo and add to it a `CMakeLists.txt` to it found here: https://eliemichel.github.io/LearnWebGPU/basic-3d-rendering/some-interaction/simple-gui.html#setting-up-imgui

## Features

- Procedural shape generation with noise
- TODO: Triplanar mapping of PBR textures
  - TODO: texture bombing
  - TODO: BRDF ??
- TODO: Multiple level of details
- TODO: Flight simulator:
  - TODO: Physics and collision detection

## TODOs

More important is on top (or not)

- shadows:
  - how to make the shadows less "blocky" ? is there other methods to do the shadows ?
  - view frustrum of the shadow rendering should change depending on the planet radius (breaks if planet too big)
  - could add percentage-closer filtering
- improve the terrain edition for more "earth-like" aspect
  - plateaux
  - less regular "wavyness" with a lot of flat parts but also some high and some deep parts ?
- add skybox reflection and not just a random light
- add props on the surface
  - trees
  - rocks ??
- add ocean and ocean shader
- add some textures
  - PBR textures ? trilinear texturing ?
- compute the planet on the GPU
- add atmosphere
- add LOD
- add flight simulator
  - spawn from camera
  - simple collision with the mesh ?
- add a bike/car drive on the surface ?
- Improve GUI
  - make input/sliders and not just sliders
  - Show a timer of the time taken to build the planet
  - show the current amount of polygons/vertices
- Get rid of the seams between faces ?
- Get rid of the unused assets