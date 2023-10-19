#include "glm/glm.hpp"
#include "resource/ResourceManager.h"
#include "procgen/ElevationGenerator.hpp"

using VertexAttributes = ResourceManager::VertexAttributes;

// The planet is a cube inflated into a sphere. This class is used to generate one face of the cube
// It uses a generator that is in charge of making the final shape of the planet
class FaceGenerator {
   public:
    // TODO: this constructor is dirty: should I do like this ?
    FaceGenerator(
        glm::vec3 _face_normal,
        unsigned int _resolution,
        ElevationGenerator& _elevationGenerator) : elevationGenerator(_elevationGenerator) {
        face_normal = _face_normal;
        resolution = _resolution;

        // we pick a orthogonal vector to get 2 unit axis on the surface...
        // tbh I can't fully grasp the intuition on which axis to get
        // bit it would change the order of the triangles
        axis_a = glm::vec3(face_normal.y, face_normal.z, face_normal.x);
        // axis_a = Vector3(face_normal.x, face_normal.z, face_normal.y)
        axis_b = glm::cross(face_normal, axis_a);
    }

    // generate the vertex attributes for a face of the planet
    // we directly write in given arrays, they should be of the right size already
    void generateFaceData(
        std::vector<VertexAttributes>& vertexData,
        std::vector<uint32_t>& indices) {
        // start the index from the last of the previous face
        int tri_index = indices.size();
        int vert_index_offset = vertexData.size();

        // resize the vertex data to hold the new face
        int quad_count = int(pow(resolution - 1, 2));
        vertexData.resize(vert_index_offset + resolution * resolution);
        indices.resize(tri_index + quad_count * 2 * 3);
        for (unsigned int y = 0; y < resolution; y++) {
            for (unsigned int x = 0; x < resolution; x++) {
                int i = vert_index_offset + x + y * resolution;

                glm::vec2 ratio = glm::vec2(x, y) / float((resolution - 1));
                // don't know why this calculation is different from the sebastian lague code (b and a inverted ?)
                glm::vec3 point_on_unit_cube = face_normal + (2 * ratio.x - 1) * axis_a + (2 * ratio.y - 1) * axis_b;

                // normalizing from the center will create a sphere
                glm::vec3 point_on_unit_sphere = glm::normalize(point_on_unit_cube);

                // glm::vec3 point_on_planet = shape_generator.compute_elevation(point_on_unit_sphere)
                glm::vec3 point_on_planet = elevationGenerator.evaluate(point_on_unit_sphere);

                // build the vertex attributes
                VertexAttributes attributes = {
                    point_on_planet,  // position;
                    glm::vec3(0.0f),  // normal are computed later
                    glm::vec3(0.0f),  // color;
                    glm::vec2(0.0f),  // uv;
                    glm::vec3(0.0f),  // tangent;
                    glm::vec3(0.0f),  // bitangent;
                };
                vertexData[i] = attributes;

                // create the indexes
                // we skip the borders
                if (x != resolution - 1 && y != resolution - 1) {
                    // 1st triangle
                    indices[tri_index] = i;
                    indices[tri_index + 1] = i + resolution + 1;
                    indices[tri_index + 2] = i + resolution;

                    // 2nd
                    indices[tri_index + 3] = i;
                    indices[tri_index + 4] = i + 1;
                    indices[tri_index + 5] = i + resolution + 1;
                    tri_index += 6;
                }
            }
        }
    }

   private:
    glm::vec3 face_normal;
    glm::vec3 axis_a;
    glm::vec3 axis_b;
    unsigned int resolution;
    float radius;
    ElevationGenerator elevationGenerator;
};