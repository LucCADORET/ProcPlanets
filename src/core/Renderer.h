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
    int resolution = 100;
    float radius = 1.0;

    // noise settings
    float frequency = 1.0f;
    int octaves = 8;
};

class Renderer {
   public:
    bool init(GLFWwindow* window);
    bool setPlanetPipeline(
        std::vector<VertexAttributes> const& vertexData,
        std::vector<uint32_t> const& indices);
    bool setSkyboxPipeline();
    bool setOceanPipeline();
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
    void buildDepthTexture();
    void buildShadowDepthTexture();
    void updateGui(wgpu::RenderPassEncoder renderPass);
    bool setShadowPipeline();

    // (Just aliases to make notations lighter)
    using mat4x4 = glm::mat4x4;
    using vec4 = glm::vec4;
    using vec3 = glm::vec3;
    using vec2 = glm::vec2;

    struct SceneUniforms {
        // Transform matrices
        mat4x4 projectionMatrix;
        mat4x4 invProjectionMatrix;
        mat4x4 viewMatrix;
        mat4x4 invViewMatrix;
        mat4x4 modelMatrix;
        mat4x4 lightViewProjMatrix;

        // Some more stuff, sometimes unused
        vec4 color;
        vec4 lightDirection;
        vec4 baseColor;
        vec4 viewPosition;
        float time;
        float fov;

        // swapchain height size
        float width;
        float height;
    };
    // Have the compiler check byte alignment
    static_assert(sizeof(SceneUniforms) % 16 == 0);

    // some constant settings
    // TODO: make the sun it further, it could collide with the model if it gets better
    // It is used for lighting calculation mostly
    vec4 mSunPosition = vec4({54.0f, 7.77f, 2.5f, 0.0f});
    vec4 mPlanetAlbedo = vec4({0.48, 0.39, 0.31, 1.0f});
    float near = 0.01f;
    float far = 100.0f;
    float fov = 45.0f;

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

    // shadow related stuff
    wgpu::RenderPipeline mShadowPipeline = nullptr;
    wgpu::BindGroup mShadowBindGroup = nullptr;
    unsigned int mShadowDepthTextureSize = 4096;
    wgpu::TextureView mShadowDepthTextureView = nullptr;
    wgpu::Texture mShadowDepthTexture = nullptr;
    wgpu::TextureFormat mShadowDepthTextureFormat = wgpu::TextureFormat::Depth32Float;
    wgpu::Sampler mShadowSampler = nullptr;

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

    // Ocean stuff
    wgpu::RenderPipeline mOceanPipeline = nullptr;
    wgpu::BindGroup mOceanBindGroup = nullptr;
    wgpu::TextureView mOceanNMTextureView = nullptr;  // keep track of it for later cleanup
    wgpu::Texture mOceanNMTexture = nullptr;

    // GUI related stuff
    GUISettings mGUISettings;
};