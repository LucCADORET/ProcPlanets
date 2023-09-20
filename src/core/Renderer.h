#pragma once

#include "resource/ResourceManager.h"

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

using VertexAttributes = ResourceManager::VertexAttributes;

struct GUISettings {
    bool changed = true;  // default to true for the initial render
    int resolution = 10;
    float radius = 1.0;
};

class Renderer {
   public:
    bool init(GLFWwindow* window);
    bool setPlanetPipeline(std::vector<VertexAttributes> vertexData, std::vector<uint32_t> indices);
    bool setSkyboxPipeline();
    void terminate();
    void terminatePlanetPipeline();
    void onFrame();
    wgpu::Device getDevice() { return m_device; };
    wgpu::TextureFormat getSwapChainFormat() { return m_swapChainFormat; };
    wgpu::TextureFormat getDepthTextureFormat() { return m_depthTextureFormat; };
    void updateCamera(glm::vec3 position);
    void resizeSwapChain(GLFWwindow* window);
    GUISettings getGUISettings() { return mGUISettings; };

   private:
    void buildSwapChain(GLFWwindow* window);
    void buildDepthBuffer();
    void updateGui(wgpu::RenderPassEncoder renderPass);

    // (Just aliases to make notations lighter)
    using mat4x4 = glm::mat4x4;
    using vec4 = glm::vec4;
    using vec3 = glm::vec3;
    using vec2 = glm::vec2;

    /**
     * The same structure as in the shader, replicated in C++
     */
    struct SceneUniforms {
        
        // Transform matrices
        mat4x4 projectionMatrix;
        mat4x4 viewMatrix;
        mat4x4 modelMatrix;

        // Some more stuff, sometimes unused
        vec4 color;
        vec4 lightDirection;
        vec4 baseColor;
        vec4 viewPosition;
        float time;
        float _pad[3];
    };
    // Have the compiler check byte alignment
    static_assert(sizeof(SceneUniforms) % 16 == 0);

    wgpu::Instance m_instance = nullptr;
    wgpu::Surface m_surface = nullptr;
    wgpu::Adapter m_adapter = nullptr;
    wgpu::Device m_device = nullptr;
    wgpu::Queue m_queue = nullptr;
    wgpu::SwapChain m_swapChain = nullptr;
    wgpu::SwapChainDescriptor m_swapChainDesc;

    // depth texture
    wgpu::Texture m_depthTexture = nullptr;
    wgpu::TextureView m_depthTextureView = nullptr;

    // should public and/or gettable ?
    wgpu::TextureFormat m_swapChainFormat = wgpu::TextureFormat::Undefined;
    wgpu::TextureFormat m_depthTextureFormat = wgpu::TextureFormat::Depth24Plus;

    // the scene pipeline and stuff related to it
    wgpu::RenderPipeline m_pipeline = nullptr;
    wgpu::Sampler m_sampler = nullptr;
    wgpu::Buffer m_vertexBuffer = nullptr;
    wgpu::Buffer m_indexBuffer = nullptr;
    wgpu::Buffer m_uniformBuffer = nullptr;
    wgpu::BindGroup m_bindGroup = nullptr;

    // Error callback set on the device (for debugging)
    std::unique_ptr<wgpu::ErrorCallback> m_errorCallbackHandle;

    // Model rendering part
    SceneUniforms m_uniforms;
    int m_vertexCount;
    int m_indexCount;
    vector<ResourceManager::VertexAttributes> m_vertexData;
    vector<uint32_t> m_indexData;
    wgpu::TextureView mBaseColorTextureView = nullptr;  // keep track of it for later cleanup
    wgpu::Texture mBaseColorTexture = nullptr;
    wgpu::TextureView mNormalMapTextureView = nullptr;  // keep track of it for later cleanup
    wgpu::Texture mNormalMapTexture = nullptr;

    // skybox related stuff
    wgpu::RenderPipeline mSkyboxPipeline = nullptr;
    wgpu::Sampler mSkyboxSampler = nullptr;
    wgpu::Buffer mSkyboxVertexBuffer = nullptr;
    wgpu::Buffer mSkyboxUniformBuffer = nullptr;
    wgpu::BindGroup mSkyboxBindGroup = nullptr;
    SceneUniforms mSkyboxUniforms;
    int mSkyboxVertexCount;
    vector<ResourceManager::VertexAttributes> mSkyboxVertexData;
    wgpu::TextureView mSkyboxTextureView = nullptr;  // keep track of it for later cleanup
    wgpu::Texture mSkyboxTexture = nullptr;

    // GUI related stuff
    GUISettings mGUISettings;
};