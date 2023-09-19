#pragma once

#include "scenes/Scene.hpp"

class Spaceship : public Scene {
   public:
    bool init(wgpu::Device device) override {
        // Base texture
        wgpu::TextureView textureView = nullptr;
        wgpu::Texture texture = ResourceManager::loadTexture(
            ASSETS_DIR "/spaceship/Material_baseColor.png", device, &textureView);
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

        // Normal map
        wgpu::TextureView normalTextureView = nullptr;
        wgpu::Texture normalTexture = ResourceManager::loadTexture(
            ASSETS_DIR "/spaceship/Material_normal.png", device, &normalTextureView);
        if (!texture) {
            std::cerr << "Could not load texture!" << std::endl;
            return false;
        }
        std::cout << "Texture view: " << normalTextureView << std::endl;
        std::cout << "Texture: " << normalTexture << std::endl;
        TextureDesc descNormal = {
            normalTextureView,
            normalTexture,
        };
        m_textureDescs.push_back(descNormal);

        // Load mesh data from OBJ file
        bool success = ResourceManager::loadGeometryFromObj(ASSETS_DIR "/spaceship/spaceship.obj", m_vertexData);
        if (!success) {
            std::cerr << "Could not load geometry!" << std::endl;
            return false;
        }

        // Load the shaders
        std::cout << "Creating shader module..." << std::endl;
        m_shaderModule = ResourceManager::loadShaderModule(ASSETS_DIR "/spaceship/shader.wgsl", device);
        std::cout << "Shader module: " << m_shaderModule << std::endl;
        return true;
    }

   private:
};