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
    ElevationGenerator elevationGenerator(radius);

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
        FaceGenerator faceGenerator(face, resolution, elevationGenerator);
        faceGenerator.generateFaceData(vertexData, indices);
    }

    // generate the normals
    for (uint32_t i = 0; i < indices.size(); i += 3) {
        auto &v1 = vertexData[indices[i]];
        auto &v2 = vertexData[indices[i+1]];
        auto &v3 = vertexData[indices[i+2]];
        auto edge1 = v2.position - v1.position;
        auto edge2 = v3.position - v1.position;
        auto face_normal = glm::cross(edge1, edge2);
        v1.normal += face_normal;
        v2.normal += face_normal;
        v3.normal += face_normal;
    }

    // normalize the normals
    for (uint32_t i = 0; i < vertexData.size(); i++) {
        vertexData[i].normal = glm::normalize(vertexData[i].normal);
    }

    auto end = chrono::steady_clock::now();
    cout << "Time to generate planet data: "
         << chrono::duration_cast<chrono::milliseconds>(end - start).count()
         << " ms" << endl;
}
