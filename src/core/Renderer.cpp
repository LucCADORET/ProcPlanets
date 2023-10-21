#include "core/Renderer.h"

using namespace wgpu;
using VertexAttributes = ResourceManager::VertexAttributes;

bool Renderer::init(GLFWwindow* window) {
    mInstance = createInstance(InstanceDescriptor{});
    if (!mInstance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        return false;
    }

    std::cout << "Requesting adapter..." << std::endl;
    mSurface = glfwGetWGPUSurface(mInstance, window);
    RequestAdapterOptions adapterOpts{};
    adapterOpts.compatibleSurface = mSurface;
    mAdapter = mInstance.requestAdapter(adapterOpts);
    std::cout << "Got adapter: " << mAdapter << std::endl;

    SupportedLimits supportedLimits;
    mAdapter.getLimits(&supportedLimits);

    std::cout << "Requesting device..." << std::endl;
    RequiredLimits requiredLimits = Default;
    requiredLimits.limits.maxVertexAttributes = 6;
    requiredLimits.limits.maxVertexBuffers = 1;
    requiredLimits.limits.maxBufferSize = 5000000 * sizeof(VertexAttributes);
    requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
    requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
    requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
    requiredLimits.limits.maxInterStageShaderComponents = 32;
    requiredLimits.limits.maxBindGroups = 2;
    //                                    ^ This was a 1
    requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
    requiredLimits.limits.maxUniformBufferBindingSize = 64 * 4 * sizeof(float);
    // Allow textures up to 4K
    requiredLimits.limits.maxTextureDimension1D = 4096;
    requiredLimits.limits.maxTextureDimension2D = 4096;
    requiredLimits.limits.maxTextureArrayLayers = 6;
    requiredLimits.limits.maxSampledTexturesPerShaderStage = 2;
    requiredLimits.limits.maxSamplersPerShaderStage = 1;

    DeviceDescriptor deviceDesc;
    deviceDesc.label = "My Device";
    deviceDesc.requiredFeaturesCount = 0;
    deviceDesc.requiredLimits = &requiredLimits;
    deviceDesc.defaultQueue.label = "The default queue";
    mDevice = mAdapter.requestDevice(deviceDesc);
    std::cout << "Got device: " << mDevice << std::endl;

    // Add an error callback for more debug info
    mErrorCallbackHandle = mDevice.setUncapturedErrorCallback([](ErrorType type, char const* message) {
        std::cout << "Device error: type " << type;
        if (message) std::cout << " (message: " << message << ")";
        std::cout << std::endl;
    });

    mQueue = mDevice.getQueue();

    // Create the uniform buffer that will be common to the planet, shadow and ocean pipeline
    BufferDescriptor bufferDesc;
    bufferDesc.size = sizeof(SceneUniforms);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;
    mUniformBuffer = mDevice.createBuffer(bufferDesc);

    buildSwapChain(window);
    buildShadowDepthTexture();
    return true;
}

void Renderer::onFrame() {
    // Update time in the uniform
    mUniforms.time = static_cast<float>(glfwGetTime());
    mQueue.writeBuffer(mUniformBuffer, offsetof(SceneUniforms, time), &mUniforms.time, sizeof(SceneUniforms::time));

    // the "current textureview" could be seen as the "context" in the JS version ?
    TextureView nextTexture = mSwapChain.getCurrentTextureView();
    if (!nextTexture) {
        std::cerr << "Cannot acquire next swap chain texture" << std::endl;
        return;
    }

    CommandEncoderDescriptor commandEncoderDesc;
    commandEncoderDesc.label = "Command Encoder";
    CommandEncoder encoder = mDevice.createCommandEncoder(commandEncoderDesc);

    // SHADOW PASS
    RenderPassDepthStencilAttachment shadowDepthStencilAttachment;
    shadowDepthStencilAttachment.view = mShadowDepthTextureView;
    shadowDepthStencilAttachment.depthClearValue = 1.0f;
    shadowDepthStencilAttachment.depthLoadOp = LoadOp::Clear;
    shadowDepthStencilAttachment.depthStoreOp = StoreOp::Store;
    shadowDepthStencilAttachment.depthReadOnly = false;
    shadowDepthStencilAttachment.stencilClearValue = 0;
#ifdef WEBGPU_BACKEND_WGPU
    shadowDepthStencilAttachment.stencilLoadOp = LoadOp::Clear;
    shadowDepthStencilAttachment.stencilStoreOp = StoreOp::Store;
#else
    shadowDepthStencilAttachment.stencilLoadOp = LoadOp::Undefined;
    shadowDepthStencilAttachment.stencilStoreOp = StoreOp::Undefined;
#endif
    shadowDepthStencilAttachment.stencilReadOnly = false;

    RenderPassDescriptor shadowPassDesc{};
    shadowPassDesc.label = "Shadow Render Pass";
    shadowPassDesc.colorAttachmentCount = 0;
    shadowPassDesc.colorAttachments = nullptr;
    shadowPassDesc.depthStencilAttachment = &shadowDepthStencilAttachment;
    shadowPassDesc.timestampWriteCount = 0;
    shadowPassDesc.timestampWrites = nullptr;
    RenderPassEncoder shadowPass = encoder.beginRenderPass(shadowPassDesc);

    // This should write in the shadow depth texture ?
    shadowPass.setPipeline(mShadowPipeline);
    shadowPass.setVertexBuffer(0, mVertexBuffer, 0, mVertexCount * sizeof(VertexAttributes));
    shadowPass.setIndexBuffer(mIndexBuffer, IndexFormat::Uint32, 0, mIndexCount * sizeof(uint32_t));
    shadowPass.setBindGroup(0, mShadowBindGroup, 0, nullptr);
    shadowPass.drawIndexed(mIndexCount, 1, 0, 0, 0);
    shadowPass.end();

    // SKYBOX + OCEAN + SCENE RENDER PASS
    RenderPassDepthStencilAttachment depthStencilAttachment;
    depthStencilAttachment.view = mDepthTextureView;
    depthStencilAttachment.depthClearValue = 1.0f;
    depthStencilAttachment.depthLoadOp = LoadOp::Clear;
    depthStencilAttachment.depthStoreOp = StoreOp::Store;
    depthStencilAttachment.depthReadOnly = false;
    depthStencilAttachment.stencilClearValue = 0;
#ifdef WEBGPU_BACKEND_WGPU
    depthStencilAttachment.stencilLoadOp = LoadOp::Clear;
    depthStencilAttachment.stencilStoreOp = StoreOp::Store;
#else
    depthStencilAttachment.stencilLoadOp = LoadOp::Undefined;
    depthStencilAttachment.stencilStoreOp = StoreOp::Undefined;
#endif
    depthStencilAttachment.stencilReadOnly = false;

    RenderPassColorAttachment renderPassColorAttachment{};
    renderPassColorAttachment.view = nextTexture;
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = LoadOp::Clear;
    renderPassColorAttachment.storeOp = StoreOp::Store;
    renderPassColorAttachment.clearValue = Color{0.65, 0.67, 1, 1.0};

    RenderPassDescriptor renderPassDesc{};
    renderPassDesc.label = "Scene Render pass";
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
    renderPassDesc.timestampWriteCount = 0;
    renderPassDesc.timestampWrites = nullptr;
    RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);

    // the skybox stuff
    renderPass.setPipeline(mSkyboxPipeline);
    renderPass.setVertexBuffer(0, mSkyboxVertexBuffer, 0, mSkyboxVertexCount * sizeof(VertexAttributes));
    renderPass.setBindGroup(0, mSkyboxBindGroup, 0, nullptr);
    renderPass.draw(mSkyboxVertexCount, 1, 0, 0);

    // the whole scene stuff
    renderPass.setPipeline(mPipeline);
    renderPass.setVertexBuffer(0, mVertexBuffer, 0, mVertexCount * sizeof(VertexAttributes));
    renderPass.setIndexBuffer(mIndexBuffer, IndexFormat::Uint32, 0, mIndexCount * sizeof(uint32_t));
    renderPass.setBindGroup(0, mBindGroup, 0, nullptr);
    renderPass.drawIndexed(mIndexCount, 1, 0, 0, 0);

    renderPass.end();

    // OCEAN RENDER PASS
    renderPassColorAttachment.loadOp = LoadOp::Load;  // Load the texture of the previous render passes

    RenderPassDescriptor oceanRenderPassDesc{};
    oceanRenderPassDesc.label = "Ocean Render Pass";
    oceanRenderPassDesc.colorAttachmentCount = 1;
    oceanRenderPassDesc.colorAttachments = &renderPassColorAttachment;
    oceanRenderPassDesc.depthStencilAttachment = nullptr;
    oceanRenderPassDesc.timestampWriteCount = 0;
    oceanRenderPassDesc.timestampWrites = nullptr;
    RenderPassEncoder oceanRenderPass = encoder.beginRenderPass(oceanRenderPassDesc);

    // The ocean stuff
    oceanRenderPass.setPipeline(mOceanPipeline);
    oceanRenderPass.setBindGroup(0, mOceanBindGroup, 0, nullptr);
    oceanRenderPass.draw(6, 1, 0, 0);  // draw a double triangle

    oceanRenderPass.end();

    // Write the GUI after everythin else to be above the rest
    RenderPassEncoder GUIRenderPass = encoder.beginRenderPass(renderPassDesc);
    updateGui(GUIRenderPass);
    GUIRenderPass.end();

    nextTexture.release();

    CommandBufferDescriptor cmdBufferDescriptor{};
    cmdBufferDescriptor.label = "Command buffer";
    CommandBuffer command = encoder.finish(cmdBufferDescriptor);
    mQueue.submit(command);

    mSwapChain.present();

#ifdef WEBGPU_BACKEND_DAWN
    // Check for pending error callbacks
    m_device.tick();
#endif
}

// Build a new swapchain if from the current window
// This will be re-called if the window changes size
void Renderer::buildSwapChain(GLFWwindow* window) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    // Destroy previously allocated swap chain
    if (mSwapChain != nullptr) {
        mSwapChain.release();
    }

    std::cout << "Creating swapchain..." << std::endl;
#ifdef WEBGPU_BACKEND_WGPU
    mSwapChainFormat = mSurface.getPreferredFormat(mAdapter);
#else
    m_swapChainFormat = TextureFormat::BGRA8Unorm;
#endif
    mSwapChainDesc.width = static_cast<uint32_t>(width);
    mSwapChainDesc.height = static_cast<uint32_t>(height);
    mSwapChainDesc.usage = TextureUsage::RenderAttachment;
    mSwapChainDesc.format = mSwapChainFormat;
    mSwapChainDesc.presentMode = PresentMode::Fifo;
    mSwapChain = mDevice.createSwapChain(mSurface, mSwapChainDesc);
    std::cout << "Swapchain: " << mSwapChain << std::endl;

    buildDepthTexture();
}

// Build a new depth buffer
// This has to be re-called if the swap chain changes (e.g. on window resize)
void Renderer::buildDepthTexture() {
    // Destroy previously allocated texture
    if (mDepthTexture != nullptr) {
        mDepthTextureView.release();
        mDepthTexture.destroy();
        mDepthTexture.release();
    }

    // Create the depth texture
    TextureDescriptor depthTextureDesc;
    depthTextureDesc.dimension = TextureDimension::_2D;
    depthTextureDesc.format = mDepthTextureFormat;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = 1;

    depthTextureDesc.size = {mSwapChainDesc.width, mSwapChainDesc.height, 1};
    depthTextureDesc.usage = TextureUsage::RenderAttachment | TextureUsage::TextureBinding;
    depthTextureDesc.viewFormatCount = 1;
    depthTextureDesc.viewFormats = (WGPUTextureFormat*)&mDepthTextureFormat;
    mDepthTexture = mDevice.createTexture(depthTextureDesc);
    std::cout << "Depth texture: " << mDepthTexture << std::endl;

    // Create the view of the depth texture manipulated by the rasterizer
    TextureViewDescriptor depthTextureViewDesc;
    depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
    depthTextureViewDesc.baseArrayLayer = 0;
    depthTextureViewDesc.arrayLayerCount = 1;
    depthTextureViewDesc.baseMipLevel = 0;
    depthTextureViewDesc.mipLevelCount = 1;
    depthTextureViewDesc.dimension = TextureViewDimension::_2D;
    depthTextureViewDesc.format = mDepthTextureFormat;
    mDepthTextureView = mDepthTexture.createView(depthTextureViewDesc);
    std::cout << "Depth texture view: " << mDepthTextureView << std::endl;
}

void Renderer::buildShadowDepthTexture() {
    // Create the depth texture
    TextureDescriptor depthTextureDesc;
    depthTextureDesc.dimension = TextureDimension::_2D;
    depthTextureDesc.format = mShadowDepthTextureFormat;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = 1;

    depthTextureDesc.size = {mShadowDepthTextureSize, mShadowDepthTextureSize, 1};
    depthTextureDesc.usage = TextureUsage::RenderAttachment | TextureUsage::TextureBinding;
    depthTextureDesc.viewFormatCount = 1;
    depthTextureDesc.viewFormats = (WGPUTextureFormat*)&mShadowDepthTextureFormat;
    mShadowDepthTexture = mDevice.createTexture(depthTextureDesc);
    std::cout << "Depth texture: " << mShadowDepthTexture << std::endl;

    // Create the view of the depth texture manipulated by the rasterizer
    TextureViewDescriptor depthTextureViewDesc;
    depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
    depthTextureViewDesc.baseArrayLayer = 0;
    depthTextureViewDesc.arrayLayerCount = 1;
    depthTextureViewDesc.baseMipLevel = 0;
    depthTextureViewDesc.mipLevelCount = 1;
    depthTextureViewDesc.dimension = TextureViewDimension::_2D;
    depthTextureViewDesc.format = mShadowDepthTextureFormat;
    mShadowDepthTextureView = mShadowDepthTexture.createView(depthTextureViewDesc);
    std::cout << "Depth texture view: " << mShadowDepthTextureView << std::endl;
}

// create a pipeline from a given resource bundle
bool Renderer::setPlanetPipeline(
    std::vector<VertexAttributes> const& vertexData,
    std::vector<uint32_t> const& indices) {
    mVertexData = vertexData;
    mIndexData = indices;

    // Load the shaders
    // std::cout << "Creating shader module..." << std::endl;
    string shaderPath = ASSETS_DIR "/planet/shader.wgsl";
    wgpu::ShaderModule shaderModule = ResourceManager::loadShaderModule(shaderPath, mDevice);
    // std::cout << "Shader module: " << shaderModule << std::endl;

    // std::cout << "Creating render pipeline..." << std::endl;
    RenderPipelineDescriptor pipelineDesc;

    // Vertex fetch
    std::vector<VertexAttribute> vertexAttribs(6);

    // Position attribute
    vertexAttribs[0].shaderLocation = 0;
    vertexAttribs[0].format = VertexFormat::Float32x3;
    vertexAttribs[0].offset = 0;

    // Normal attribute
    vertexAttribs[1].shaderLocation = 1;
    vertexAttribs[1].format = VertexFormat::Float32x3;
    vertexAttribs[1].offset = offsetof(VertexAttributes, normal);

    // Color attribute
    vertexAttribs[2].shaderLocation = 2;
    vertexAttribs[2].format = VertexFormat::Float32x3;
    vertexAttribs[2].offset = offsetof(VertexAttributes, color);

    // UV attribute
    vertexAttribs[3].shaderLocation = 3;
    vertexAttribs[3].format = VertexFormat::Float32x2;
    vertexAttribs[3].offset = offsetof(VertexAttributes, uv);

    // UV Tangent
    vertexAttribs[4].shaderLocation = 4;
    vertexAttribs[4].format = VertexFormat::Float32x3;
    vertexAttribs[4].offset = offsetof(VertexAttributes, tangent);

    // UV Bitangent
    vertexAttribs[5].shaderLocation = 5;
    vertexAttribs[5].format = VertexFormat::Float32x3;
    vertexAttribs[5].offset = offsetof(VertexAttributes, bitangent);

    VertexBufferLayout vertexBufferLayout;
    vertexBufferLayout.attributeCount = (uint32_t)vertexAttribs.size();
    vertexBufferLayout.attributes = vertexAttribs.data();
    vertexBufferLayout.arrayStride = sizeof(VertexAttributes);
    vertexBufferLayout.stepMode = VertexStepMode::Vertex;

    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;

    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;

    // pipelineDesc.primitive.topology = PrimitiveTopology::LineList; // if you want wireframes
    pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = FrontFace::CCW;
    pipelineDesc.primitive.cullMode = CullMode::None;

    FragmentState fragmentState;
    pipelineDesc.fragment = &fragmentState;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    BlendState blendState;
    blendState.color.srcFactor = BlendFactor::SrcAlpha;
    blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = BlendOperation::Add;
    blendState.alpha.srcFactor = BlendFactor::Zero;
    blendState.alpha.dstFactor = BlendFactor::One;
    blendState.alpha.operation = BlendOperation::Add;

    ColorTargetState colorTarget;
    colorTarget.format = mSwapChainFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    DepthStencilState depthStencilState = Default;
    depthStencilState.depthCompare = CompareFunction::Less;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.format = mDepthTextureFormat;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;

    pipelineDesc.depthStencil = &depthStencilState;

    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    // Create binding layouts
    // Just the uniforms for now: no texture or anything
    int binGroupEntriesCount = 3;
    std::vector<BindGroupLayoutEntry> bindingLayoutEntries(binGroupEntriesCount, Default);

    // The uniform buffer binding that we already had
    BindGroupLayoutEntry& bindingLayout = bindingLayoutEntries[0];
    bindingLayout.binding = 0;
    bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
    bindingLayout.buffer.type = BufferBindingType::Uniform;
    bindingLayout.buffer.minBindingSize = sizeof(SceneUniforms);

    // Shadow Sampler
    BindGroupLayoutEntry& samplerBindingLayout = bindingLayoutEntries[1];
    samplerBindingLayout.binding = 1;
    samplerBindingLayout.visibility = ShaderStage::Fragment;
    samplerBindingLayout.sampler.type = SamplerBindingType::Comparison;

    // The depth texture
    BindGroupLayoutEntry& baseColorTextureBindingLayout = bindingLayoutEntries[2];
    baseColorTextureBindingLayout.binding = 2;
    baseColorTextureBindingLayout.visibility = ShaderStage::Fragment;
    baseColorTextureBindingLayout.texture.sampleType = TextureSampleType::Depth;
    baseColorTextureBindingLayout.texture.viewDimension = TextureViewDimension::_2D;

    // Create a bind group layout
    BindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();
    bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
    BindGroupLayout bindGroupLayout = mDevice.createBindGroupLayout(bindGroupLayoutDesc);

    // Create the pipeline layout
    PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
    PipelineLayout layout = mDevice.createPipelineLayout(layoutDesc);
    pipelineDesc.layout = layout;

    mPipeline = mDevice.createRenderPipeline(pipelineDesc);
    // std::cout << "Render pipeline: " << m_pipeline << std::endl;

    // Create the sampler for the shadows
    SamplerDescriptor shadowSamplerDesc;
    shadowSamplerDesc.compare = CompareFunction::Less;
    shadowSamplerDesc.maxAnisotropy = 1;
    mShadowSampler = mDevice.createSampler(shadowSamplerDesc);

    // define vertex buffer
    BufferDescriptor bufferDesc;
    bufferDesc.size = mVertexData.size() * sizeof(VertexAttributes);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
    bufferDesc.mappedAtCreation = false;
    mVertexBuffer = mDevice.createBuffer(bufferDesc);
    mQueue.writeBuffer(mVertexBuffer, 0, mVertexData.data(), bufferDesc.size);
    mVertexCount = static_cast<int>(mVertexData.size());

    // Create index buffer
    // (we reuse the bufferDesc initialized for the vertexBuffer)
    bufferDesc.size = mIndexData.size() * sizeof(uint32_t);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Index;
    bufferDesc.mappedAtCreation = false;
    mIndexBuffer = mDevice.createBuffer(bufferDesc);
    mQueue.writeBuffer(mIndexBuffer, 0, mIndexData.data(), bufferDesc.size);
    mIndexCount = static_cast<int>(mIndexData.size());

    // Upload the initial value of the uniforms
    mUniforms.modelMatrix = mat4x4(1.0);
    mUniforms.viewMatrix = glm::lookAt(vec3(-2.0f, -3.0f, 2.0f), vec3(0.0f), vec3(0, 1, 0));
    // float near = 0.01f, far = 100.0f;
    // float size = 5.0f;
    // m_uniforms.projectionMatrix = glm::ortho(
    //     -size, size, -size, size, near, far);
    mUniforms.projectionMatrix = glm::perspective(
        glm::radians(fov),
        float(mSwapChainDesc.width) / float(mSwapChainDesc.height),
        near, far);
    mUniforms.invProjectionMatrix = glm::inverse(mUniforms.projectionMatrix);
    mUniforms.color = {0.0f, 1.0f, 0.4f, 1.0f};
    mUniforms.lightDirection = glm::normalize(-mSunPosition);
    mUniforms.viewPosition = vec4(0.0f);  // dunno how to init this one...
    mUniforms.time = 1.0f;
    mUniforms.fov = fov;
    mUniforms.width = mSwapChainDesc.width;
    mUniforms.height = mSwapChainDesc.height;
    mQueue.writeBuffer(mUniformBuffer, 0, &mUniforms, sizeof(SceneUniforms));

    // also write the base settings to the uniform
    setTerrainMaterialSettings();

    // Add the data to the actual bindings
    std::vector<BindGroupEntry> bindings(binGroupEntriesCount);

    // uniform
    bindings[0].binding = 0;
    bindings[0].buffer = mUniformBuffer;
    bindings[0].offset = 0;
    bindings[0].size = sizeof(SceneUniforms);

    // sampler
    bindings[1].binding = 1;
    bindings[1].sampler = mShadowSampler;

    // The shadow texture
    bindings[2].binding = 2;
    bindings[2].textureView = mShadowDepthTextureView;

    BindGroupDescriptor bindGroupDesc;
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    mBindGroup = mDevice.createBindGroup(bindGroupDesc);

    // Create the shadow pipeline after the planet one
    setShadowPipeline();
    return true;
}

// create the ocean pipeline (mostly a shader)
bool Renderer::setOceanPipeline() {
    // Load the shaders
    // std::cout << "Creating shader module..." << std::endl;
    string shaderPath = ASSETS_DIR "/ocean/ocean.wgsl";
    wgpu::ShaderModule shaderModule = ResourceManager::loadShaderModule(shaderPath, mDevice);
    // std::cout << "Shader module: " << shaderModule << std::endl;

    // std::cout << "Creating render pipeline..." << std::endl;
    RenderPipelineDescriptor pipelineDesc;
    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;

    // pipelineDesc.primitive.topology = PrimitiveTopology::LineList; // if you want wireframes
    pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = FrontFace::CCW;
    pipelineDesc.primitive.cullMode = CullMode::None;

    FragmentState fragmentState;
    pipelineDesc.fragment = &fragmentState;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    BlendState blendState;
    blendState.color.srcFactor = BlendFactor::SrcAlpha;
    blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = BlendOperation::Add;
    blendState.alpha.srcFactor = BlendFactor::Zero;
    blendState.alpha.dstFactor = BlendFactor::One;
    blendState.alpha.operation = BlendOperation::Add;

    ColorTargetState colorTarget;
    colorTarget.format = mSwapChainFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    // Create binding layouts
    // Just the uniforms for now: no texture or anything
    int bindGroupEntriesCount = 4;
    std::vector<BindGroupLayoutEntry> bindingLayoutEntries(bindGroupEntriesCount, Default);

    // The uniform buffer binding
    BindGroupLayoutEntry& bindingLayout = bindingLayoutEntries[0];
    bindingLayout.binding = 0;
    bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
    bindingLayout.buffer.type = BufferBindingType::Uniform;
    bindingLayout.buffer.minBindingSize = sizeof(SceneUniforms);

    // Planet scene depth texture sampler
    BindGroupLayoutEntry& samplerBindingLayout = bindingLayoutEntries[1];
    samplerBindingLayout.binding = 1;
    samplerBindingLayout.visibility = ShaderStage::Fragment;
    samplerBindingLayout.sampler.type = SamplerBindingType::Filtering;

    // The depth texture
    BindGroupLayoutEntry& baseColorTextureBindingLayout = bindingLayoutEntries[2];
    baseColorTextureBindingLayout.binding = 2;
    baseColorTextureBindingLayout.visibility = ShaderStage::Fragment;
    baseColorTextureBindingLayout.texture.sampleType = TextureSampleType::Depth;
    baseColorTextureBindingLayout.texture.viewDimension = TextureViewDimension::_2D;

    // The normal map
    BindGroupLayoutEntry& normalMapTextureBindingLayout = bindingLayoutEntries[3];
    normalMapTextureBindingLayout.binding = 3;
    normalMapTextureBindingLayout.visibility = ShaderStage::Fragment;
    normalMapTextureBindingLayout.texture.sampleType = TextureSampleType::Float;
    normalMapTextureBindingLayout.texture.viewDimension = TextureViewDimension::_2D;

    // Create a bind group layout
    BindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();
    bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
    BindGroupLayout bindGroupLayout = mDevice.createBindGroupLayout(bindGroupLayoutDesc);

    // Create the pipeline layout
    PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
    PipelineLayout layout = mDevice.createPipelineLayout(layoutDesc);
    pipelineDesc.layout = layout;

    mOceanPipeline = mDevice.createRenderPipeline(pipelineDesc);

    // Create a sampler for the textures
    SamplerDescriptor samplerDesc;
    samplerDesc.compare = CompareFunction::Undefined;
    samplerDesc.maxAnisotropy = 1;
    mSampler = mDevice.createSampler(samplerDesc);

    // set the default settings on the uniforms
    setOceanSettings();

    // Add the data to the actual bindings
    std::vector<BindGroupEntry> bindings(bindGroupEntriesCount);

    // uniform
    bindings[0].binding = 0;
    bindings[0].buffer = mUniformBuffer;
    bindings[0].offset = 0;
    bindings[0].size = sizeof(SceneUniforms);

    // depth texture stuff
    bindings[1].binding = 1;
    bindings[1].sampler = mSampler;
    bindings[2].binding = 2;
    bindings[2].textureView = mDepthTextureView;

    // the normal map
    string oceanNormalPath = ASSETS_DIR "/ocean/water_nm.jpg";
    mOceanNMTexture = ResourceManager::loadTexture(
        oceanNormalPath, mDevice, &mOceanNMTextureView);
    if (!mOceanNMTexture) {
        std::cerr << "Could not load texture!" << std::endl;
        return false;
    }
    bindings[3].binding = 3;
    bindings[3].textureView = mOceanNMTextureView;

    BindGroupDescriptor bindGroupDesc;
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    mOceanBindGroup = mDevice.createBindGroup(bindGroupDesc);
    return true;
}

// NOTE: The shadow pipeline MUST be called after the planets pipeline
// As it relies on values set with it before (light position, various initialized buffers...)
bool Renderer::setShadowPipeline() {
    // Load the shaders
    // std::cout << "Creating shader module..." << std::endl;
    string shaderPath = ASSETS_DIR "/planet/shadows.wgsl";
    wgpu::ShaderModule shaderModule = ResourceManager::loadShaderModule(shaderPath, mDevice);
    // std::cout << "Shader module: " << shaderModule << std::endl;

    // std::cout << "Creating render pipeline..." << std::endl;
    RenderPipelineDescriptor pipelineDesc;

    // Vertex fetch
    std::vector<VertexAttribute> vertexAttribs(6);

    // Position attribute
    vertexAttribs[0].shaderLocation = 0;
    vertexAttribs[0].format = VertexFormat::Float32x3;
    vertexAttribs[0].offset = 0;

    // Normal attribute
    vertexAttribs[1].shaderLocation = 1;
    vertexAttribs[1].format = VertexFormat::Float32x3;
    vertexAttribs[1].offset = offsetof(VertexAttributes, normal);

    // Color attribute
    vertexAttribs[2].shaderLocation = 2;
    vertexAttribs[2].format = VertexFormat::Float32x3;
    vertexAttribs[2].offset = offsetof(VertexAttributes, color);

    // UV attribute
    vertexAttribs[3].shaderLocation = 3;
    vertexAttribs[3].format = VertexFormat::Float32x2;
    vertexAttribs[3].offset = offsetof(VertexAttributes, uv);

    // UV Tangent
    vertexAttribs[4].shaderLocation = 4;
    vertexAttribs[4].format = VertexFormat::Float32x3;
    vertexAttribs[4].offset = offsetof(VertexAttributes, tangent);

    // UV Bitangent
    vertexAttribs[5].shaderLocation = 5;
    vertexAttribs[5].format = VertexFormat::Float32x3;
    vertexAttribs[5].offset = offsetof(VertexAttributes, bitangent);

    VertexBufferLayout vertexBufferLayout;
    vertexBufferLayout.attributeCount = (uint32_t)vertexAttribs.size();
    vertexBufferLayout.attributes = vertexAttribs.data();
    vertexBufferLayout.arrayStride = sizeof(VertexAttributes);
    vertexBufferLayout.stepMode = VertexStepMode::Vertex;

    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;

    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;

    // pipelineDesc.primitive.topology = PrimitiveTopology::LineList; // if you want wireframes
    pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = FrontFace::CCW;
    pipelineDesc.primitive.cullMode = CullMode::None;

    DepthStencilState depthStencilState = Default;
    depthStencilState.depthCompare = CompareFunction::Less;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.format = mShadowDepthTextureFormat;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;

    pipelineDesc.depthStencil = &depthStencilState;

    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    // Create binding layouts
    std::vector<BindGroupLayoutEntry> bindingLayoutEntries(1, Default);

    // The uniform buffer binding that we already had
    BindGroupLayoutEntry& bindingLayout = bindingLayoutEntries[0];
    bindingLayout.binding = 0;
    bindingLayout.visibility = ShaderStage::Vertex;
    bindingLayout.buffer.type = BufferBindingType::Uniform;
    bindingLayout.buffer.minBindingSize = sizeof(SceneUniforms);

    // Create a bind group layout
    BindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();
    bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
    BindGroupLayout bindGroupLayout = mDevice.createBindGroupLayout(bindGroupLayoutDesc);

    // Create the pipeline layout
    PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
    PipelineLayout layout = mDevice.createPipelineLayout(layoutDesc);
    pipelineDesc.layout = layout;

    mShadowPipeline = mDevice.createRenderPipeline(pipelineDesc);

    // the view matrix should be from the light's perspective
    auto viewMatrix = glm::lookAt(vec3(mSunPosition), vec3(0.0f), vec3(0, 1, 0));
    // auto viewMatrix = glm::lookAt(-vec3(m_uniforms.lightDirection), vec3(0.0f), vec3(0, 1, 0));

    // the projection should be ortholinear since the light source is infinitely far
    float near = 0.01f, far = 100.0f;
    float size = 5.0f;
    auto projectionMatrix = glm::ortho(
        -size, size, -size, size, near, far);

    // we set the lightViewProjMatrix once at pipeline creation, since it won't change
    mUniforms.lightViewProjMatrix = projectionMatrix * viewMatrix;
    mQueue.writeBuffer(
        mUniformBuffer,
        offsetof(SceneUniforms, lightViewProjMatrix),
        &mUniforms.lightViewProjMatrix,
        sizeof(SceneUniforms::lightViewProjMatrix));

    // Bing group for the uniform
    std::vector<BindGroupEntry> bindings(1);

    // uniform
    bindings[0].binding = 0;
    bindings[0].buffer = mUniformBuffer;
    bindings[0].offset = 0;
    bindings[0].size = sizeof(SceneUniforms);

    BindGroupDescriptor bindGroupDesc;
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    mShadowBindGroup = mDevice.createBindGroup(bindGroupDesc);

    return true;
}

bool Renderer::setSkyboxPipeline() {
    string shaderPath = ASSETS_DIR "/skybox/shader.wgsl";
    std::cout << "Creating shader module..." << std::endl;
    wgpu::ShaderModule shaderModule = ResourceManager::loadShaderModule(shaderPath, mDevice);
    std::cout << "Shader module: " << shaderModule << std::endl;

    std::cout << "Creating skybox render pipeline..." << std::endl;
    RenderPipelineDescriptor pipelineDesc;

    // Vertex attributes definition
    std::vector<VertexAttribute> vertexAttribs(6);

    // Position attribute
    vertexAttribs[0].shaderLocation = 0;
    vertexAttribs[0].format = VertexFormat::Float32x3;
    vertexAttribs[0].offset = 0;

    // Normal attribute
    vertexAttribs[1].shaderLocation = 1;
    vertexAttribs[1].format = VertexFormat::Float32x3;
    vertexAttribs[1].offset = offsetof(VertexAttributes, normal);

    // Color attribute
    vertexAttribs[2].shaderLocation = 2;
    vertexAttribs[2].format = VertexFormat::Float32x3;
    vertexAttribs[2].offset = offsetof(VertexAttributes, color);

    // UV attribute
    vertexAttribs[3].shaderLocation = 3;
    vertexAttribs[3].format = VertexFormat::Float32x2;
    vertexAttribs[3].offset = offsetof(VertexAttributes, uv);

    // UV Tangent
    vertexAttribs[4].shaderLocation = 4;
    vertexAttribs[4].format = VertexFormat::Float32x3;
    vertexAttribs[4].offset = offsetof(VertexAttributes, tangent);

    // UV Bitangent
    vertexAttribs[5].shaderLocation = 5;
    vertexAttribs[5].format = VertexFormat::Float32x3;
    vertexAttribs[5].offset = offsetof(VertexAttributes, bitangent);

    VertexBufferLayout vertexBufferLayout;
    vertexBufferLayout.attributeCount = (uint32_t)vertexAttribs.size();
    vertexBufferLayout.attributes = vertexAttribs.data();
    vertexBufferLayout.arrayStride = sizeof(VertexAttributes);
    vertexBufferLayout.stepMode = VertexStepMode::Vertex;

    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;

    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;

    pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = FrontFace::CCW;
    pipelineDesc.primitive.cullMode = CullMode::None;

    FragmentState fragmentState;
    pipelineDesc.fragment = &fragmentState;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    BlendState blendState;
    blendState.color.srcFactor = BlendFactor::SrcAlpha;
    blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = BlendOperation::Add;
    blendState.alpha.srcFactor = BlendFactor::Zero;
    blendState.alpha.dstFactor = BlendFactor::One;
    blendState.alpha.operation = BlendOperation::Add;

    ColorTargetState colorTarget;
    colorTarget.format = mSwapChainFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    DepthStencilState depthStencilState = Default;
    depthStencilState.depthCompare = CompareFunction::LessEqual;
    depthStencilState.depthWriteEnabled = false;
    depthStencilState.format = mDepthTextureFormat;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;

    pipelineDesc.depthStencil = &depthStencilState;

    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    int entriesCount = 3;
    std::vector<BindGroupLayoutEntry> bindingLayoutEntries(entriesCount, Default);

    // Uniform buffer
    BindGroupLayoutEntry& bindingLayout = bindingLayoutEntries[0];
    bindingLayout.binding = 0;
    bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
    bindingLayout.buffer.type = BufferBindingType::Uniform;
    bindingLayout.buffer.minBindingSize = sizeof(SceneUniforms);

    // Sampler
    BindGroupLayoutEntry& samplerBindingLayout = bindingLayoutEntries[1];
    samplerBindingLayout.binding = 1;
    samplerBindingLayout.visibility = ShaderStage::Fragment;
    samplerBindingLayout.sampler.type = SamplerBindingType::Filtering;

    // Base color of the skybox
    BindGroupLayoutEntry& baseColorTextureBindingLayout = bindingLayoutEntries[2];
    baseColorTextureBindingLayout.binding = 2;
    baseColorTextureBindingLayout.visibility = ShaderStage::Fragment;
    baseColorTextureBindingLayout.texture.sampleType = TextureSampleType::Float;
    baseColorTextureBindingLayout.texture.viewDimension = TextureViewDimension::Cube;

    // Create a bind group layout
    BindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();
    bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
    BindGroupLayout bindGroupLayout = mDevice.createBindGroupLayout(bindGroupLayoutDesc);

    // Create the pipeline layout
    PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
    PipelineLayout layout = mDevice.createPipelineLayout(layoutDesc);
    pipelineDesc.layout = layout;

    mSkyboxPipeline = mDevice.createRenderPipeline(pipelineDesc);
    std::cout << "Skybox pipeline: " << mSkyboxPipeline << std::endl;

    // Create a sampler
    SamplerDescriptor samplerDesc;
    samplerDesc.addressModeU = AddressMode::Repeat;
    samplerDesc.addressModeV = AddressMode::Repeat;
    samplerDesc.addressModeW = AddressMode::Repeat;
    samplerDesc.magFilter = FilterMode::Linear;
    samplerDesc.minFilter = FilterMode::Linear;
    samplerDesc.mipmapFilter = MipmapFilterMode::Linear;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 8.0f;
    samplerDesc.compare = CompareFunction::Undefined;
    samplerDesc.maxAnisotropy = 1;
    mSkyboxSampler = mDevice.createSampler(samplerDesc);

    // Create vertex buffer
    // Load mesh data from OBJ file
    string objPath = ASSETS_DIR "/skybox/unit_cube.obj";
    bool success = ResourceManager::loadGeometryFromObj(objPath, mSkyboxVertexData);
    if (!success) {
        std::cerr << "Could not load geometry!" << std::endl;
        return false;
    }
    BufferDescriptor bufferDesc;
    bufferDesc.size = mSkyboxVertexData.size() * sizeof(VertexAttributes);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
    bufferDesc.mappedAtCreation = false;
    mSkyboxVertexBuffer = mDevice.createBuffer(bufferDesc);
    mQueue.writeBuffer(mSkyboxVertexBuffer, 0, mSkyboxVertexData.data(), bufferDesc.size);

    mSkyboxVertexCount = static_cast<int>(mSkyboxVertexData.size());

    // Create uniform buffer
    bufferDesc.size = sizeof(SceneUniforms);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;
    mSkyboxUniformBuffer = mDevice.createBuffer(bufferDesc);

    // Upload the initial value of the uniforms
    mSkyboxUniforms.modelMatrix = glm::mat4(1.0f);
    mSkyboxUniforms.viewMatrix = glm::lookAt(vec3(1.0f), vec3(0.0f), vec3(0, 1, 0));
    mSkyboxUniforms.projectionMatrix = glm::perspective(
        glm::radians(fov),
        float(mSwapChainDesc.width) / float(mSwapChainDesc.height),
        near, far);
    mSkyboxUniforms.time = 1.0f;
    mQueue.writeBuffer(mSkyboxUniformBuffer, 0, &mSkyboxUniforms, sizeof(SceneUniforms));

    // Add the data to the actual bindings
    std::vector<BindGroupEntry> bindings(entriesCount);

    // uniform
    bindings[0].binding = 0;
    bindings[0].buffer = mSkyboxUniformBuffer;
    bindings[0].offset = 0;
    bindings[0].size = sizeof(SceneUniforms);

    // sampler
    bindings[1].binding = 1;
    bindings[1].sampler = mSkyboxSampler;

    // loab cubemaps
    string baseColorTexturePath = ASSETS_DIR "/skybox";
    mSkyboxTexture = ResourceManager::loadPrefilteredCubemap(
        baseColorTexturePath, mDevice, &mSkyboxTextureView);
    if (!mSkyboxTexture) {
        std::cerr << "Could not load texture!" << std::endl;
        return false;
    }
    bindings[2].binding = 2;
    bindings[2].textureView = mSkyboxTextureView;

    BindGroupDescriptor bindGroupDesc;
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    mSkyboxBindGroup = mDevice.createBindGroup(bindGroupDesc);
    return true;
}

// updates the view stuff given the new camera position
void Renderer::updateCamera(glm::vec3 position) {
    // update the view position
    mUniforms.viewPosition = glm::vec4(position.x, position.y, position.z, 1.0);
    // cout << m_uniforms.viewPosition.x << ' ' << m_uniforms.viewPosition.y << ' ' << m_uniforms.viewPosition.z << endl;
    mQueue.writeBuffer(
        mUniformBuffer,
        offsetof(SceneUniforms, viewPosition),
        &mUniforms.viewPosition,
        sizeof(SceneUniforms::viewPosition));

    // update the view matrix for the model
    mUniforms.viewMatrix = glm::lookAt(position, vec3(0.0f), vec3(0, 1, 0));
    mQueue.writeBuffer(
        mUniformBuffer,
        offsetof(SceneUniforms, viewMatrix),
        &mUniforms.viewMatrix,
        sizeof(SceneUniforms::viewMatrix));
    mUniforms.invViewMatrix = glm::inverse(mUniforms.viewMatrix);
    mQueue.writeBuffer(
        mUniformBuffer,
        offsetof(SceneUniforms, invViewMatrix),
        &mUniforms.invViewMatrix,
        sizeof(SceneUniforms::invViewMatrix));

    // update the view matrix for the skybox
    // we get rid of the translation part also with an intermediary mat3
    mSkyboxUniforms.viewMatrix = glm::mat4(glm::mat3(glm::lookAt(position, vec3(0.0f), vec3(0, 1, 0))));
    mQueue.writeBuffer(
        mSkyboxUniformBuffer,
        offsetof(SceneUniforms, viewMatrix),
        &mSkyboxUniforms.viewMatrix,
        sizeof(SceneUniforms::viewMatrix));
}

void Renderer::setOceanSettings() {
    mUniforms.oceanRadius = mGUISettings.oceanRadius;
    mQueue.writeBuffer(
        mUniformBuffer,
        offsetof(SceneUniforms, oceanRadius),
        &mUniforms.oceanRadius,
        sizeof(SceneUniforms::oceanRadius));
    mUniforms.oceanColor.r = mGUISettings.oceanColor[0];
    mUniforms.oceanColor.g = mGUISettings.oceanColor[1];
    mUniforms.oceanColor.b = mGUISettings.oceanColor[2];
    mQueue.writeBuffer(
        mUniformBuffer,
        offsetof(SceneUniforms, oceanColor),
        &mUniforms.oceanColor,
        sizeof(SceneUniforms::oceanColor));
    mUniforms.oceanShininess = mGUISettings.oceanShininess;
    mQueue.writeBuffer(
        mUniformBuffer,
        offsetof(SceneUniforms, oceanShininess),
        &mUniforms.oceanShininess,
        sizeof(SceneUniforms::oceanShininess));
    mUniforms.oceanKSpecular = mGUISettings.oceanKSpecular;
    mQueue.writeBuffer(
        mUniformBuffer,
        offsetof(SceneUniforms, oceanKSpecular),
        &mUniforms.oceanKSpecular,
        sizeof(SceneUniforms::oceanKSpecular));
}

void Renderer::setTerrainMaterialSettings() {
    mUniforms.baseColor.r = mGUISettings.baseColor[0];
    mUniforms.baseColor.g = mGUISettings.baseColor[1];
    mUniforms.baseColor.b = mGUISettings.baseColor[2];
    mQueue.writeBuffer(
        mUniformBuffer,
        offsetof(SceneUniforms, baseColor),
        &mUniforms.baseColor,
        sizeof(SceneUniforms::baseColor));
    mUniforms.terrainShininess = mGUISettings.terrainShininess;
    mQueue.writeBuffer(
        mUniformBuffer,
        offsetof(SceneUniforms, terrainShininess),
        &mUniforms.terrainShininess,
        sizeof(SceneUniforms::terrainShininess));
    mUniforms.terrainKSpecular = mGUISettings.terrainKSpecular;
    mQueue.writeBuffer(
        mUniformBuffer,
        offsetof(SceneUniforms, terrainKSpecular),
        &mUniforms.terrainKSpecular,
        sizeof(SceneUniforms::terrainKSpecular));
}

void Renderer::updateGui(RenderPassEncoder renderPass) {
    // Start the Dear ImGui frame
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
        // Build a demo UI
        bool planetSettingsChanged = false;
        ImGui::Begin("Settings");  // Create a window called "Hello, world!" and append into it.

        // Planet construction part
        ImGui::SeparatorText("Planet shape");
        planetSettingsChanged = ImGui::SliderInt("resolution", &(mGUISettings.resolution), 2, 500) || planetSettingsChanged;  // count of vertices per face
        planetSettingsChanged = ImGui::SliderFloat("radius", &(mGUISettings.radius), 1.0f, 10.0f) || planetSettingsChanged;
        planetSettingsChanged = ImGui::SliderFloat("noise frequency", &(mGUISettings.frequency), 0.001f, 5.0f) || planetSettingsChanged;
        planetSettingsChanged = ImGui::SliderInt("noise octaves", &(mGUISettings.octaves), 1, 10) || planetSettingsChanged;  // count of vertices per face

        // Planet terrain material
        ImGui::SeparatorText("Terrain material");
        bool terrainMaterialSettingsChanged = false;
        terrainMaterialSettingsChanged = ImGui::ColorEdit3("base color", mGUISettings.baseColor) || terrainMaterialSettingsChanged;                                // count of vertices per face
        terrainMaterialSettingsChanged = ImGui::SliderFloat("terrain shininess", &(mGUISettings.terrainShininess), 0.0, 256.0) || terrainMaterialSettingsChanged;  // count of vertices per face
        terrainMaterialSettingsChanged = ImGui::SliderFloat("terrain K specular", &(mGUISettings.terrainKSpecular), 0.0, 1.0) || terrainMaterialSettingsChanged;   // count of vertices per face
        if (terrainMaterialSettingsChanged) {
            setTerrainMaterialSettings();
        }

        // Ocean part
        ImGui::SeparatorText("Ocean");
        bool oceanSettingsChanged = false;
        oceanSettingsChanged = ImGui::SliderFloat("ocean radius", &(mGUISettings.oceanRadius), 1.0f, 10.0f) || oceanSettingsChanged;       // count of vertices per face
        oceanSettingsChanged = ImGui::ColorEdit3("ocean color", mGUISettings.oceanColor) || oceanSettingsChanged;                          // count of vertices per face
        oceanSettingsChanged = ImGui::SliderFloat("ocean shininess", &(mGUISettings.oceanShininess), 0.0, 256.0) || oceanSettingsChanged;  // count of vertices per face
        oceanSettingsChanged = ImGui::SliderFloat("ocean K specular", &(mGUISettings.oceanKSpecular), 0.0, 1.0) || oceanSettingsChanged;   // count of vertices per face
        if (oceanSettingsChanged) {
            setOceanSettings();
        }

        mGUISettings.planetSettingsChanged = planetSettingsChanged;
        ImGui::SeparatorText("Debug");
        ImGui::Text("View pos: (%.3f, %.3f, %.3f)", mUniforms.viewPosition.x, mUniforms.viewPosition.y, mUniforms.viewPosition.z);
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::End();
    }

    // Draw the UI
    ImGui::EndFrame();
    ImGui::Render();                                                  // Convert the UI defined above into low-level drawing commands
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);  // Execute the low-level drawing commands on the WebGPU backend
}

void Renderer::terminatePlanetPipeline() {
    // check if there's something to release
    if (mPipeline != nullptr) {
        mPipeline.release();
        mBindGroup.release();

        mVertexBuffer.destroy();
        mVertexBuffer.release();
        mIndexBuffer.destroy();
        mIndexBuffer.release();
    }
}

void Renderer::terminate() {
    terminatePlanetPipeline();

    mUniformBuffer.release();
    mSampler.release();

    mDepthTextureView.release();
    mDepthTexture.destroy();
    mDepthTexture.release();

    // TODO: should release the vertex buffer of the texture too
    mSkyboxTextureView.release();
    mSkyboxTexture.destroy();
    mSkyboxTexture.release();

    mSwapChain.release();
    mQueue.release();
    mDevice.release();
    mAdapter.release();
    mInstance.release();
}
