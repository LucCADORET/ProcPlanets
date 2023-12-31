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
    // default to true for the initial render
    // is only for the planet shape stuff
    bool planetSettingsChanged = true;
    int resolution = 100;
    float radius = 1.0;
    float frequency = 1.0f;
    int octaves = 8;

    // terrain material settings
    float baseColor[3]{0.48, 0.39, 0.31};
    float terrainShininess = 16.0f;
    float terrainKSpecular = 1.0f;

    // ocean settings
    float oceanRadius = 1.5f;
    float oceanColor[3]{0.00, 0.55, 1.00};
    float oceanShininess = 32.0f;
    float oceanKSpecular = 1.0f;
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
    wgpu::Device getDevice() { return mDevice; };
    wgpu::TextureFormat getSwapChainFormat() { return mSwapChainFormat; };
    wgpu::TextureFormat getDepthTextureFormat() { return mDepthTextureFormat; };
    void updateCamera(glm::vec3 position);
    void resizeSwapChain(GLFWwindow* window);
    GUISettings getGUISettings() { return mGUISettings; };

   private:
    void buildSwapChain(GLFWwindow* window);
    void buildDepthTexture();
    void buildShadowDepthTexture();
    void updateGui(wgpu::RenderPassEncoder renderPass);
    bool setShadowPipeline();
    void setOceanSettings();
    void setTerrainMaterialSettings();

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
        float terrainShininess;
        float terrainKSpecular;

        // swapchain height size
        float width;
        float height;
        float _pad1[2];

        // ocean settings
        vec4 oceanColor;
        float oceanRadius;
        float oceanShininess;
        float oceanKSpecular;
        float _pad2[1];
    };
    // Have the compiler check byte alignment
    static_assert(sizeof(SceneUniforms) % 16 == 0);

    // some constant settings
    // TODO: make the sun further ? Shadows are buggy on big planet.
    vec4 mSunPosition = vec4({54.0f, 7.77f, 2.5f, 0.0f});
    float near = 0.01f;
    float far = 100.0f;
    float fov = 45.0f;

    wgpu::Instance mInstance = nullptr;
    wgpu::Surface mSurface = nullptr;
    wgpu::Adapter mAdapter = nullptr;
    wgpu::Device mDevice = nullptr;
    wgpu::Queue mQueue = nullptr;
    wgpu::SwapChain mSwapChain = nullptr;
    wgpu::SwapChainDescriptor mSwapChainDesc;

    // depth texture
    wgpu::Texture mDepthTexture = nullptr;
    wgpu::TextureView mDepthTextureView = nullptr;

    // should public and/or gettable ?
    wgpu::TextureFormat mSwapChainFormat = wgpu::TextureFormat::Undefined;
    wgpu::TextureFormat mDepthTextureFormat = wgpu::TextureFormat::Depth24Plus;

    // the planet geometry pipeline data
    wgpu::RenderPipeline mPipeline = nullptr;
    wgpu::Sampler mSampler = nullptr;
    wgpu::Buffer mVertexBuffer = nullptr;
    wgpu::Buffer mIndexBuffer = nullptr;
    wgpu::Buffer mUniformBuffer = nullptr;
    wgpu::BindGroup mBindGroup = nullptr;

    // Error callback set on the device (for debugging)
    std::unique_ptr<wgpu::ErrorCallback> mErrorCallbackHandle;

    // Model rendering part
    SceneUniforms mUniforms;
    int mVertexCount;
    int mIndexCount;
    vector<ResourceManager::VertexAttributes> mVertexData;
    vector<uint32_t> mIndexData;
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