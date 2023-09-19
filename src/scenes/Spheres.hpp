#pragma once

#include "scenes/Scene.hpp"

class Spheres : public Scene {
   public:
    bool init(wgpu::Device device) override {

        // Load mesh data from OBJ file
        bool success = ResourceManager::loadGeometryFromObj(ASSETS_DIR "/spheres/spheres.obj", m_vertexData);
        if (!success) {
            std::cerr << "Could not load geometry!" << std::endl;
            return false;
        }

        // Load the shaders
        std::cout << "Creating shader module..." << std::endl;
        m_shaderModule = ResourceManager::loadShaderModule(ASSETS_DIR "/spheres/shader.wgsl", device);
        std::cout << "Shader module: " << m_shaderModule << std::endl;
        return true;
    }

   private:
};