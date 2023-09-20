
#pragma once

#include "glm/glm.hpp"
#include "procgen/FastNoiseLite.h"

// generates the elevation data for a point on the unit sphere of the procedural planet
class ElevationGenerator {
   public:
    ElevationGenerator(float radius) {
        mRadius = radius;
        mNoise.SetSeed(1337);
        mNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        mNoise.SetFrequency(1.0f);
    };

    // return the actual point on the sphere, from the point on the unit sphere
    glm::vec3 evaluate(glm::vec3 pointOnUnitSphere) {
        float noise = mNoise.GetNoise(pointOnUnitSphere.x, pointOnUnitSphere.y, pointOnUnitSphere.z);
        noise = (noise + 1) * 0.5f; // get between 0 and 1
        return pointOnUnitSphere * mRadius * (1 + noise);
    }

   private:
    FastNoiseLite mNoise;
    float mRadius;
};