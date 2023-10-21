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

#include "core/Engine.h"
#include "resource/ResourceManager.h"

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <iostream>
#include <cassert>
#include <filesystem>
#include <sstream>
#include <string>
#include <array>

using namespace wgpu;
using VertexAttributes = ResourceManager::VertexAttributes;

constexpr float PI = 3.14159265358979323846f;

void onWindowMouseMove(GLFWwindow* m_window, double xpos, double ypos) {
    auto pApp = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(m_window));
    if (pApp != nullptr) pApp->onMouseMove(xpos, ypos);
}
void onWindowMouseButton(GLFWwindow* m_window, int button, int action, int mods) {
    auto pApp = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(m_window));
    if (pApp != nullptr) pApp->onMouseButton(button, action, mods);
}
void onWindowScroll(GLFWwindow* m_window, double xoffset, double yoffset) {
    auto pApp = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(m_window));
    if (pApp != nullptr) pApp->onScroll(xoffset, yoffset);
}

bool Engine::onInit() {
    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW!" << std::endl;
        return false;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    mWindow = glfwCreateWindow(windowHeight, windowWidth, "Learn WebGPU", NULL, NULL);
    if (!mWindow) {
        std::cerr << "Could not open window!" << std::endl;
        return false;
    }

    mRenderer.init(mWindow);

    // setup the skybox
    mRenderer.setSkyboxPipeline();

    // setup the ocean
    mRenderer.setOceanPipeline();

    // Setup GLFW callbacks
    glfwSetWindowUserPointer(mWindow, this);
    glfwSetCursorPosCallback(mWindow, onWindowMouseMove);
    glfwSetMouseButtonCallback(mWindow, onWindowMouseButton);
    glfwSetScrollCallback(mWindow, onWindowScroll);

    initGui();

    return true;
}

void Engine::onFrame() {
    // if the settings changed, take down the current pipeline and rebuild the planet
    GUISettings settings = mRenderer.getGUISettings();
    if (settings.planetSettingsChanged) {
        mRenderer.terminatePlanetPipeline();
        std::vector<VertexAttributes> vertexData;
        std::vector<uint32_t> indices;
        mPlanetGenerator.generatePlanetData(vertexData, indices, settings);

        // TODO: why remake the whole pipeline ? only the data changes
        mRenderer.setPlanetPipeline(vertexData, indices);

        // update the view matrix to match the current camera position
        updateViewMatrix();
    }

    glfwPollEvents();
    updateDragInertia();
    mRenderer.onFrame();
}

void Engine::onFinish() {
    mRenderer.terminate();
    glfwDestroyWindow(mWindow);
    glfwTerminate();
}

bool Engine::isRunning() {
    return !glfwWindowShouldClose(mWindow);
}

void Engine::onMouseMove(double xpos, double ypos) {
    if (mDragState.active) {
        glm::vec2 currentMouse = glm::vec2(-(float)xpos, (float)ypos);
        glm::vec2 delta = (currentMouse - mDragState.startMouse) * mDragState.sensitivity;
        mCameraState.angles.x = mDragState.startCameraState.angles.x - delta.x;
        mCameraState.angles.y = mDragState.startCameraState.angles.y + delta.y;
        // Clamp to avoid going too far when orbitting up/down
        mCameraState.angles.y = glm::clamp(mCameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);
        updateViewMatrix();

        // Inertia
        mDragState.velocity = delta - mDragState.previousDelta;
        // std::cout << m_drag.velocity.x << " " << m_drag.velocity.y << std::endl;
        mDragState.previousDelta = delta;
    }
}

void Engine::onMouseButton(int button, int action, [[maybe_unused]] int modifiers) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        // Don't rotate the camera if the mouse is already captured by an ImGui
        // interaction at this frame.
        return;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        switch (action) {
            case GLFW_PRESS:
                mDragState.active = true;
                double xpos, ypos;
                glfwGetCursorPos(mWindow, &xpos, &ypos);
                mDragState.startMouse = glm::vec2(-(float)xpos, (float)ypos);
                mDragState.startCameraState = mCameraState;
                break;
            case GLFW_RELEASE:
                mDragState.active = false;
                break;
        }
    }
}

void Engine::onScroll([[maybe_unused]] double xoffset, double yoffset) {
    mCameraState.zoom += mDragState.scrollSensitivity * (float)yoffset;
    mCameraState.zoom = glm::clamp(mCameraState.zoom, -4.0f, 2.0f);
    updateViewMatrix();
}

void Engine::updateViewMatrix() {
    float cx = cos(mCameraState.angles.x);
    float sx = sin(mCameraState.angles.x);
    float cy = cos(mCameraState.angles.y);
    float sy = sin(mCameraState.angles.y);
    glm::vec3 position = glm::vec3(cx * cy, sy, sx * cy) * std::exp(-mCameraState.zoom);
    mRenderer.updateCamera(position);
}

void Engine::updateDragInertia() {
    constexpr float eps = 1e-4f;
    // Apply inertia only when the user released the click.
    if (!mDragState.active) {
        // Avoid updating the matrix when the velocity is no longer noticeable
        if (std::abs(mDragState.velocity.x) < eps && std::abs(mDragState.velocity.y) < eps) {
            return;
        }
        mCameraState.angles.x -= mDragState.velocity.x;
        mCameraState.angles.y += mDragState.velocity.y;
        mCameraState.angles.y = glm::clamp(mCameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);
        // Dampen the velocity so that it decreases exponentially and stops
        // after a few frames.
        mDragState.velocity *= mDragState.intertia;
        updateViewMatrix();
    }
}

void Engine::initGui() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOther(mWindow, true);

    // Need access to the wgpu device to init the UI
    Device device = mRenderer.getDevice();
    auto swapChainFormat = mRenderer.getSwapChainFormat();
    auto depthTextureFormat = mRenderer.getDepthTextureFormat();
    ImGui_ImplWGPU_Init(device, 3, swapChainFormat, depthTextureFormat);
}
