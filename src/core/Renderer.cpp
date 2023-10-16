#include "core/Renderer.h"

using namespace wgpu;
using VertexAttributes = ResourceManager::VertexAttributes;

bool Renderer::init(GLFWwindow* window) {
    m_instance = createInstance(InstanceDescriptor{});
    if (!m_instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        return false;
    }

    std::cout << "Requesting adapter..." << std::endl;
    m_surface = glfwGetWGPUSurface(m_instance, window);
    RequestAdapterOptions adapterOpts{};
    adapterOpts.compatibleSurface = m_surface;
    m_adapter = m_instance.requestAdapter(adapterOpts);
    std::cout << "Got adapter: " << m_adapter << std::endl;

    SupportedLimits supportedLimits;
    m_adapter.getLimits(&supportedLimits);

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
    requiredLimits.limits.maxUniformBufferBindingSize = 32 * 4 * sizeof(float);
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
    m_device = m_adapter.requestDevice(deviceDesc);
    std::cout << "Got device: " << m_device << std::endl;

    // Add an error callback for more debug info
    m_errorCallbackHandle = m_device.setUncapturedErrorCallback([](ErrorType type, char const* message) {
        std::cout << "Device error: type " << type;
        if (message) std::cout << " (message: " << message << ")";
        std::cout << std::endl;
    });

    m_queue = m_device.getQueue();

    // Create the uniform buffer that will be common to the planet, shadow and ocean pipeline
    BufferDescriptor bufferDesc;
    bufferDesc.size = sizeof(SceneUniforms);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;
    m_uniformBuffer = m_device.createBuffer(bufferDesc);

    buildSwapChain(window);
    buildShadowDepthTexture();
    return true;
}

void Renderer::onFrame() {
    // Update time in the uniform
    m_uniforms.time = static_cast<float>(glfwGetTime());
    m_queue.writeBuffer(m_uniformBuffer, offsetof(SceneUniforms, time), &m_uniforms.time, sizeof(SceneUniforms::time));

    // the "current textureview" could be seen as the "context" in the JS version ?
    TextureView nextTexture = m_swapChain.getCurrentTextureView();
    if (!nextTexture) {
        std::cerr << "Cannot acquire next swap chain texture" << std::endl;
        return;
    }

    CommandEncoderDescriptor commandEncoderDesc;
    commandEncoderDesc.label = "Command Encoder";
    CommandEncoder encoder = m_device.createCommandEncoder(commandEncoderDesc);

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
    shadowPass.setVertexBuffer(0, m_vertexBuffer, 0, m_vertexCount * sizeof(VertexAttributes));
    shadowPass.setIndexBuffer(m_indexBuffer, IndexFormat::Uint32, 0, m_indexCount * sizeof(uint32_t));
    shadowPass.setBindGroup(0, mShadowBindGroup, 0, nullptr);
    shadowPass.drawIndexed(m_indexCount, 1, 0, 0, 0);
    shadowPass.end();

    // SKYBOX + OCEAN + SCENE RENDER PASS
    RenderPassDepthStencilAttachment depthStencilAttachment;
    depthStencilAttachment.view = m_depthTextureView;
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
    renderPassColorAttachment.clearValue = Color{0.65, 0.67, 1, 0.0};

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
    renderPass.setPipeline(m_pipeline);
    renderPass.setVertexBuffer(0, m_vertexBuffer, 0, m_vertexCount * sizeof(VertexAttributes));
    renderPass.setIndexBuffer(m_indexBuffer, IndexFormat::Uint32, 0, m_indexCount * sizeof(uint32_t));
    renderPass.setBindGroup(0, m_bindGroup, 0, nullptr);
    renderPass.drawIndexed(m_indexCount, 1, 0, 0, 0);

    // We add the GUI drawing commands to the render pass
    updateGui(renderPass);

    renderPass.end();

    // OCEAN RENDER PASS
    renderPassColorAttachment.loadOp = LoadOp::Load; // Load the texture of the previous render passes

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

    nextTexture.release();

    CommandBufferDescriptor cmdBufferDescriptor{};
    cmdBufferDescriptor.label = "Command buffer";
    CommandBuffer command = encoder.finish(cmdBufferDescriptor);
    m_queue.submit(command);

    m_swapChain.present();

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
    if (m_swapChain != nullptr) {
        m_swapChain.release();
    }

    std::cout << "Creating swapchain..." << std::endl;
#ifdef WEBGPU_BACKEND_WGPU
    m_swapChainFormat = m_surface.getPreferredFormat(m_adapter);
#else
    m_swapChainFormat = TextureFormat::BGRA8Unorm;
#endif
    m_swapChainDesc.width = static_cast<uint32_t>(width);
    m_swapChainDesc.height = static_cast<uint32_t>(height);
    m_swapChainDesc.usage = TextureUsage::RenderAttachment;
    m_swapChainDesc.format = m_swapChainFormat;
    m_swapChainDesc.presentMode = PresentMode::Fifo;
    m_swapChain = m_device.createSwapChain(m_surface, m_swapChainDesc);
    std::cout << "Swapchain: " << m_swapChain << std::endl;

    buildDepthTexture();
}

// Build a new depth buffer
// This has to be re-called if the swap chain changes (e.g. on window resize)
void Renderer::buildDepthTexture() {
    // Destroy previously allocated texture
    if (m_depthTexture != nullptr) {
        m_depthTextureView.release();
        m_depthTexture.destroy();
        m_depthTexture.release();
    }

    // Create the depth texture
    TextureDescriptor depthTextureDesc;
    depthTextureDesc.dimension = TextureDimension::_2D;
    depthTextureDesc.format = m_depthTextureFormat;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = 1;

    depthTextureDesc.size = {m_swapChainDesc.width, m_swapChainDesc.height, 1};
    depthTextureDesc.usage = TextureUsage::RenderAttachment | TextureUsage::TextureBinding;
    depthTextureDesc.viewFormatCount = 1;
    depthTextureDesc.viewFormats = (WGPUTextureFormat*)&m_depthTextureFormat;
    m_depthTexture = m_device.createTexture(depthTextureDesc);
    std::cout << "Depth texture: " << m_depthTexture << std::endl;

    // Create the view of the depth texture manipulated by the rasterizer
    TextureViewDescriptor depthTextureViewDesc;
    depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
    depthTextureViewDesc.baseArrayLayer = 0;
    depthTextureViewDesc.arrayLayerCount = 1;
    depthTextureViewDesc.baseMipLevel = 0;
    depthTextureViewDesc.mipLevelCount = 1;
    depthTextureViewDesc.dimension = TextureViewDimension::_2D;
    depthTextureViewDesc.format = m_depthTextureFormat;
    m_depthTextureView = m_depthTexture.createView(depthTextureViewDesc);
    std::cout << "Depth texture view: " << m_depthTextureView << std::endl;
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
    mShadowDepthTexture = m_device.createTexture(depthTextureDesc);
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
    m_vertexData = vertexData;
    m_indexData = indices;

    // Load the shaders
    // std::cout << "Creating shader module..." << std::endl;
    string shaderPath = ASSETS_DIR "/planet/shader.wgsl";
    wgpu::ShaderModule shaderModule = ResourceManager::loadShaderModule(shaderPath, m_device);
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
    colorTarget.format = m_swapChainFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    DepthStencilState depthStencilState = Default;
    depthStencilState.depthCompare = CompareFunction::Less;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.format = m_depthTextureFormat;
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
    BindGroupLayout bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);

    // Create the pipeline layout
    PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
    PipelineLayout layout = m_device.createPipelineLayout(layoutDesc);
    pipelineDesc.layout = layout;

    m_pipeline = m_device.createRenderPipeline(pipelineDesc);
    // std::cout << "Render pipeline: " << m_pipeline << std::endl;

    // Create the sampler for the shadows
    SamplerDescriptor shadowSamplerDesc;
    shadowSamplerDesc.compare = CompareFunction::Less;
    shadowSamplerDesc.maxAnisotropy = 1;
    mShadowSampler = m_device.createSampler(shadowSamplerDesc);

    // define vertex buffer
    BufferDescriptor bufferDesc;
    bufferDesc.size = m_vertexData.size() * sizeof(VertexAttributes);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
    bufferDesc.mappedAtCreation = false;
    m_vertexBuffer = m_device.createBuffer(bufferDesc);
    m_queue.writeBuffer(m_vertexBuffer, 0, m_vertexData.data(), bufferDesc.size);
    m_vertexCount = static_cast<int>(m_vertexData.size());

    // Create index buffer
    // (we reuse the bufferDesc initialized for the vertexBuffer)
    bufferDesc.size = m_indexData.size() * sizeof(uint32_t);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Index;
    bufferDesc.mappedAtCreation = false;
    m_indexBuffer = m_device.createBuffer(bufferDesc);
    m_queue.writeBuffer(m_indexBuffer, 0, m_indexData.data(), bufferDesc.size);
    m_indexCount = static_cast<int>(m_indexData.size());

    // Upload the initial value of the uniforms
    m_uniforms.modelMatrix = mat4x4(1.0);
    m_uniforms.viewMatrix = glm::lookAt(vec3(-2.0f, -3.0f, 2.0f), vec3(0.0f), vec3(0, 1, 0));
    m_uniforms.projectionMatrix = glm::perspective(
        glm::radians(45.0f),
        float(m_swapChainDesc.width) / float(m_swapChainDesc.height),
        0.01f, 100.0f);
    m_uniforms.color = {0.0f, 1.0f, 0.4f, 1.0f};
    m_uniforms.lightDirection = glm::normalize(-mSunPosition);
    m_uniforms.baseColor = mPlanetAlbedo;
    m_uniforms.viewPosition = vec4(0.0f);  // dunno how to init this one...
    m_uniforms.time = 1.0f;
    m_queue.writeBuffer(m_uniformBuffer, 0, &m_uniforms, sizeof(SceneUniforms));

    // Add the data to the actual bindings
    std::vector<BindGroupEntry> bindings(binGroupEntriesCount);

    // uniform
    bindings[0].binding = 0;
    bindings[0].buffer = m_uniformBuffer;
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
    m_bindGroup = m_device.createBindGroup(bindGroupDesc);

    // Create the shadow pipeline after the planet one
    setShadowPipeline();
    return true;
}

// create the ocean pipeline (mostly a shader)
bool Renderer::setOceanPipeline() {
    // Load the shaders
    // std::cout << "Creating shader module..." << std::endl;
    string shaderPath = ASSETS_DIR "/planet/ocean.wgsl";
    wgpu::ShaderModule shaderModule = ResourceManager::loadShaderModule(shaderPath, m_device);
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
    colorTarget.format = m_swapChainFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    DepthStencilState depthStencilState = Default;
    depthStencilState.depthCompare = CompareFunction::Less;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.format = m_depthTextureFormat;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;

    // pipelineDesc.depthStencil = &depthStencilState;

    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    // Create binding layouts
    // Just the uniforms for now: no texture or anything
    int bindGroupEntriesCount = 3;
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
    BindGroupLayout bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);

    // Create the pipeline layout // TODO: should include uniforms. deactivated to test something
    PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
    PipelineLayout layout = m_device.createPipelineLayout(layoutDesc);
    pipelineDesc.layout = layout;

    mOceanPipeline = m_device.createRenderPipeline(pipelineDesc);

    // Create a sampler for the textures
    SamplerDescriptor samplerDesc;
    samplerDesc.compare = CompareFunction::Less;
    samplerDesc.maxAnisotropy = 1;
    m_sampler = m_device.createSampler(samplerDesc);

    // Add the data to the actual bindings
    std::vector<BindGroupEntry> bindings(bindGroupEntriesCount);

    // uniform
    bindings[0].binding = 0;
    bindings[0].buffer = m_uniformBuffer;
    bindings[0].offset = 0;
    bindings[0].size = sizeof(SceneUniforms);

    // sampler
    bindings[1].binding = 1;
    bindings[1].sampler = m_sampler;

    // The shadow texture
    bindings[2].binding = 2;
    bindings[2].textureView = m_depthTextureView;

    BindGroupDescriptor bindGroupDesc;
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    mOceanBindGroup = m_device.createBindGroup(bindGroupDesc);
    return true;
}

// NOTE: The shadow pipeline MUST be called after the planets pipeline
// As it relies on values set with it before (light position, various initialized buffers...)
bool Renderer::setShadowPipeline() {
    // Load the shaders
    // std::cout << "Creating shader module..." << std::endl;
    string shaderPath = ASSETS_DIR "/planet/shadows.wgsl";
    wgpu::ShaderModule shaderModule = ResourceManager::loadShaderModule(shaderPath, m_device);
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
    BindGroupLayout bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);

    // Create the pipeline layout
    PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
    PipelineLayout layout = m_device.createPipelineLayout(layoutDesc);
    pipelineDesc.layout = layout;

    mShadowPipeline = m_device.createRenderPipeline(pipelineDesc);

    // the view matrix should be from the light's perspective
    auto viewMatrix = glm::lookAt(vec3(mSunPosition), vec3(0.0f), vec3(0, 1, 0));
    // auto viewMatrix = glm::lookAt(-vec3(m_uniforms.lightDirection), vec3(0.0f), vec3(0, 1, 0));

    // the projection should be ortholinear since the light source is infinitely far
    float near = 0.01f, far = 100.0f;
    float size = 5.0f;
    auto projectionMatrix = glm::ortho(
        -size, size, -size, size, near, far);

    // we set the lightViewProjMatrix once at pipeline creation, since it won't change
    m_uniforms.lightViewProjMatrix = projectionMatrix * viewMatrix;
    m_queue.writeBuffer(
        m_uniformBuffer,
        offsetof(SceneUniforms, lightViewProjMatrix),
        &m_uniforms.lightViewProjMatrix,
        sizeof(SceneUniforms::lightViewProjMatrix));

    // Bing group for the uniform
    std::vector<BindGroupEntry> bindings(1);

    // uniform
    bindings[0].binding = 0;
    bindings[0].buffer = m_uniformBuffer;
    bindings[0].offset = 0;
    bindings[0].size = sizeof(SceneUniforms);

    BindGroupDescriptor bindGroupDesc;
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    mShadowBindGroup = m_device.createBindGroup(bindGroupDesc);

    return true;
}

bool Renderer::setSkyboxPipeline() {
    string shaderPath = ASSETS_DIR "/skybox/shader.wgsl";
    std::cout << "Creating shader module..." << std::endl;
    wgpu::ShaderModule shaderModule = ResourceManager::loadShaderModule(shaderPath, m_device);
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
    colorTarget.format = m_swapChainFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    DepthStencilState depthStencilState = Default;
    depthStencilState.depthCompare = CompareFunction::LessEqual;
    depthStencilState.depthWriteEnabled = false;
    depthStencilState.format = m_depthTextureFormat;
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
    BindGroupLayout bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);

    // Create the pipeline layout
    PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
    PipelineLayout layout = m_device.createPipelineLayout(layoutDesc);
    pipelineDesc.layout = layout;

    mSkyboxPipeline = m_device.createRenderPipeline(pipelineDesc);
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
    mSkyboxSampler = m_device.createSampler(samplerDesc);

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
    mSkyboxVertexBuffer = m_device.createBuffer(bufferDesc);
    m_queue.writeBuffer(mSkyboxVertexBuffer, 0, mSkyboxVertexData.data(), bufferDesc.size);

    mSkyboxVertexCount = static_cast<int>(mSkyboxVertexData.size());

    // Create uniform buffer
    bufferDesc.size = sizeof(SceneUniforms);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;
    mSkyboxUniformBuffer = m_device.createBuffer(bufferDesc);

    // Upload the initial value of the uniforms
    mSkyboxUniforms.modelMatrix = glm::mat4(1.0f);
    mSkyboxUniforms.viewMatrix = glm::lookAt(vec3(1.0f), vec3(0.0f), vec3(0, 1, 0));
    mSkyboxUniforms.projectionMatrix = glm::perspective(
        glm::radians(45.0f),
        float(m_swapChainDesc.width) / float(m_swapChainDesc.height),
        0.01f, 100.0f);
    mSkyboxUniforms.time = 1.0f;
    m_queue.writeBuffer(mSkyboxUniformBuffer, 0, &mSkyboxUniforms, sizeof(SceneUniforms));

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
        baseColorTexturePath, m_device, &mSkyboxTextureView);
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
    mSkyboxBindGroup = m_device.createBindGroup(bindGroupDesc);
    return true;
}

// updates the view stuff given the new camera position
void Renderer::updateCamera(glm::vec3 position) {
    // update the view position
    m_uniforms.viewPosition = glm::vec4(position.x, position.y, position.z, 1.0);
    // cout << m_uniforms.viewPosition.x << ' ' << m_uniforms.viewPosition.y << ' ' << m_uniforms.viewPosition.z << endl;
    m_queue.writeBuffer(
        m_uniformBuffer,
        offsetof(SceneUniforms, viewPosition),
        &m_uniforms.viewPosition,
        sizeof(SceneUniforms::viewPosition));

    // update the view matrix for the model
    m_uniforms.viewMatrix = glm::lookAt(position, vec3(0.0f), vec3(0, 1, 0));
    m_queue.writeBuffer(
        m_uniformBuffer,
        offsetof(SceneUniforms, viewMatrix),
        &m_uniforms.viewMatrix,
        sizeof(SceneUniforms::viewMatrix));

    // update the view matrix for the skybox
    // we get rid of the translation part also with an intermediary mat3
    mSkyboxUniforms.viewMatrix = glm::mat4(glm::mat3(glm::lookAt(position, vec3(0.0f), vec3(0, 1, 0))));
    m_queue.writeBuffer(
        mSkyboxUniformBuffer,
        offsetof(SceneUniforms, viewMatrix),
        &mSkyboxUniforms.viewMatrix,
        sizeof(SceneUniforms::viewMatrix));
}

void Renderer::resizeSwapChain(GLFWwindow* window) {
    buildSwapChain(window);

    // update the projection matrix so that the image keeps its aspect
    float ratio = m_swapChainDesc.width / (float)m_swapChainDesc.height;
    m_uniforms.projectionMatrix = glm::perspective(glm::radians(45.0f), ratio, 0.01f, 100.0f);
    m_queue.writeBuffer(
        m_uniformBuffer,
        offsetof(SceneUniforms, projectionMatrix),
        &m_uniforms.projectionMatrix,
        sizeof(SceneUniforms::projectionMatrix));
    mSkyboxUniforms.projectionMatrix = glm::perspective(glm::radians(45.0f), ratio, 0.01f, 100.0f);
    m_queue.writeBuffer(
        mSkyboxUniformBuffer,
        offsetof(SceneUniforms, projectionMatrix),
        &mSkyboxUniforms.projectionMatrix,
        sizeof(SceneUniforms::projectionMatrix));
}

void Renderer::updateGui(RenderPassEncoder renderPass) {
    // Start the Dear ImGui frame
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
        // Build a demo UI
        bool changed = false;
        // static int counter = 0;
        // static bool show_demo_window = true;
        // static bool show_another_window = false;
        // static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

        ImGui::Begin("Settings");  // Create a window called "Hello, world!" and append into it.

        // ImGui::Text("This is some useful text.");           // Display some text (you can use a format strings too)
        // ImGui::Checkbox("Demo Window", &show_demo_window);  // Edit bools storing our window open/close state
        // ImGui::Checkbox("Another Window", &show_another_window);

        // ImGui::ColorEdit3("clear color", (float*)&clear_color);  // Edit 3 floats representing a color

        // if (ImGui::Button("Button")) {  // Buttons return true when clicked (most widgets return true when edited/activated)
        //     counter++;
        // }
        // ImGui::SameLine();
        // ImGui::Text("counter = %d", counter);
        ImGui::SeparatorText("Base sphere");
        changed = ImGui::SliderInt("resolution", &(mGUISettings.resolution), 2, 500) || changed;  // count of vertices per face
        changed = ImGui::SliderFloat("radius", &(mGUISettings.radius), 1.0f, 100.0f) || changed;
        ImGui::SeparatorText("Noise");
        changed = ImGui::SliderFloat("frequency", &(mGUISettings.frequency), 0.001f, 5.0f) || changed;
        changed = ImGui::SliderInt("octaves", &(mGUISettings.octaves), 1, 50) || changed;  // count of vertices per face

        mGUISettings.changed = changed;
        ImGui::SeparatorText("Debug");
        ImGui::Text("View pos: (%.3f, %.3f, %.3f)", m_uniforms.viewPosition.x, m_uniforms.viewPosition.y, m_uniforms.viewPosition.z);
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::End();
    }

    // Draw the UI
    ImGui::EndFrame();
    // Convert the UI defined above into low-level drawing commands
    ImGui::Render();
    // Execute the low-level drawing commands on the WebGPU backend
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
}

void Renderer::terminatePlanetPipeline() {
    // check if there's something to release
    if (m_pipeline != nullptr) {
        m_pipeline.release();
        m_bindGroup.release();

        m_vertexBuffer.destroy();
        m_vertexBuffer.release();
        m_indexBuffer.destroy();
        m_indexBuffer.release();
    }
}

void Renderer::terminate() {
    terminatePlanetPipeline();

    m_uniformBuffer.release();
    m_sampler.release();

    m_depthTextureView.release();
    m_depthTexture.destroy();
    m_depthTexture.release();

    // TODO: should release the vertex buffer of the texture too
    mSkyboxTextureView.release();
    mSkyboxTexture.destroy();
    mSkyboxTexture.release();

    m_swapChain.release();
    m_queue.release();
    m_device.release();
    m_adapter.release();
    m_instance.release();
}
