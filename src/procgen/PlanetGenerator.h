#include "procgen/FaceGenerator.hpp"
#include "core/Renderer.h"
#include "procgen/ElevationGenerator.hpp"

class PlanetGenerator {
   public:
    void generatePlanetData(
        std::vector<VertexAttributes> &vertexData,
        std::vector<uint32_t> &indices,
        GUISettings settings);

   private:
};