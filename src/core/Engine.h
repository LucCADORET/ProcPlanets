/**
 * This file is part of the "Learn WebGPU for C++" book.
 *   https://github.com/eliemichel/LearnWebGPU
 *
 * MIT License
 * Copyright (c) 2022-2023 Elie Michel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <webgpu/webgpu.hpp>
#include <glm/glm.hpp>
#include "core/Renderer.h"
#include "procgen/PlanetGenerator.h"

// Forward declare
struct GLFWwindow;

/**
 * The Engine class sets up the window, the renderer, and the GUI
 * It also calls the rendering loop, and handles the inputs (e.g. to handle the camera)
 */
class Engine {
   public:
    // A function called only once at the beginning. Returns false is init failed.
    bool onInit();

    // A function called at each frame, guaranteed never to be called before `onInit`.
    void onFrame();

    // A function called only once at the very end.
    void onFinish();

    // A function that tells if the application is still running.
    bool isRunning();

    // A function called when the window is resized.
    void onResize();

    // Mouse events
    void onMouseMove(double xpos, double ypos);
    void onMouseButton(int button, int action, int mods);
    void onScroll(double xoffset, double yoffset);

   private:
    void updateViewMatrix();
    void updateDragInertia();

    void initGui();                                      // called in onInit
    void updateGui(wgpu::RenderPassEncoder renderPass);  // called in onFrame

   private:
    // Used to init the window size
    int windowHeight = 1600;
    int windowWidth = 900;

    /**
     * A representation of the camera view transform that is as close as
     * possible to the user input.
     */
    struct CameraState {
        // angles.x is the rotation of the camera around the global vertical axis, affected by mouse.x
        // angles.y is the rotation of the camera around its local horizontal axis, affected by mouse.y
        glm::vec2 angles = {0.0f, 0.0f};
        // zoom is the position of the camera along its local forward axis, affected by the scroll wheel
        float zoom = -4.0f;
    };

    /**
     * State that the application remembers while dragging the mouse (which orbits the camera)
     */
    struct DragState {
        // Whether a drag action is ongoing (i.e., we are between mouse press and mouse release)
        bool active = false;
        // The position of the mouse at the beginning of the drag action
        glm::vec2 startMouse;
        // The camera state at the beginning of the drag action
        CameraState startCameraState;

        // Inertia
        glm::vec2 velocity = {0.0, 0.0};
        glm::vec2 previousDelta;
        float intertia = 0.9f;

        // Constant settings
        float sensitivity = 0.01f;
        float scrollSensitivity = 0.1f;
    };

    // Everything that is initialized in `onInit` and needed in `onFrame`.
    GLFWwindow* mWindow = nullptr;
    Renderer mRenderer;

    CameraState mCameraState;
    DragState mDragState;

    PlanetGenerator mPlanetGenerator;
};