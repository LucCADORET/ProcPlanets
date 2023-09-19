#pragma once

#include "resource/ResourceManager.h"
#include <vector>

using namespace std;
using VertexAttributes = ResourceManager::VertexAttributes;

// a typical scene has
// - vertex data
// - shaders associated to it
struct TextureDesc {
    wgpu::TextureView textureView;
    wgpu::Texture texture;
};

// - texture to apply to the vertex data
class Scene {
   public:
    // the textures associated with this model
    vector<TextureDesc> m_textureDescs;

    wgpu::ShaderModule m_shaderModule = nullptr;
    vector<VertexAttributes> m_vertexData;

    // should init all the data
    virtual bool init(wgpu::Device device){};
    void destroy() {
        for (auto iter = m_textureDescs.begin(); iter < m_textureDescs.end(); iter++) {
            iter->textureView.release();
            iter->texture.destroy();
            iter->texture.release();
        }
    };

   private:
};