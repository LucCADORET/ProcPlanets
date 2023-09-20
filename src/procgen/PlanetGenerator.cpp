#include "procgen/PlanetGenerator.h"

// Generates all the resources necessary to render the planet
// - vertex attributes
// - later on materials ??
void PlanetGenerator::generatePlanetData(
    std::vector<VertexAttributes> &vertexData,
    std::vector<uint32_t> &indices,
    GUISettings settings) {

    // settings of the planet
    unsigned int resolution = settings.resolution;
    float radius = settings.radius;

    // define the faces normals
    auto top = glm::vec3(0.0f, 1.0f, 0.0f);
    auto down = glm::vec3(0.0f, -1.0f, 0.0f);
    auto left = glm::vec3(-1.0f, 0.0f, 0.0f);
    auto right = glm::vec3(1.0f, 0.0f, 0.0f);
    auto front = glm::vec3(0.0f, 0.0f, 1.0f);
    auto back = glm::vec3(0.0f, 0.0f, -1.0f);
    std::vector<glm::vec3> faces{top, down, left, right, front, back};

    // make sure the vectors are empty
    vertexData.resize(0);
    indices.resize(0);

    // generate each face
    auto start = chrono::steady_clock::now();
    for (uint8_t i = 0; i < faces.size(); i++) {
        glm::vec3 face = faces[i];
        FaceGenerator faceGenerator(face, resolution, radius);
        faceGenerator.generateFaceData(vertexData, indices);
    }
    auto end = chrono::steady_clock::now();
    cout << "Time to generate planet vertices: "
         << chrono::duration_cast<chrono::milliseconds>(end - start).count()
         << " ms" << endl;
}
