#pragma once

#include "scenes/Scene.hpp"

class Boat : public Scene {
   public:
    bool init(wgpu::Device device) override {
        // setup the shader, textures, fetch the vertex buffer information... uniforms ?

        // Add this texture to the texture descs
        wgpu::TextureView textureView = nullptr;
        wgpu::Texture texture = ResourceManager::loadTexture(
            ASSETS_DIR "/boat/fourareen2K_albedo.jpg", device, &textureView);
        if (!texture) {
            std::cerr << "Could not load texture!" << std::endl;
            return false;
        }
        std::cout << "Texture: " << texture << std::endl;
        std::cout << "Texture view: " << textureView << std::endl;
        TextureDesc desc = {
            textureView,
            texture,
        };
        m_textureDescs.push_back(desc);

        // Load mesh data from OBJ file
        bool success = ResourceManager::loadGeometryFromObj(ASSETS_DIR "/boat/fourareen.obj", m_vertexData);
        if (!success) {
            std::cerr << "Could not load geometry!" << std::endl;
            return false;
        }

        // Load the shaders
        std::cout << "Creating shader module..." << std::endl;
        m_shaderModule = ResourceManager::loadShaderModule(ASSETS_DIR "/boat/shader.wgsl", device);
        std::cout << "Shader module: " << m_shaderModule << std::endl;
        return true;
    }

   private:
};