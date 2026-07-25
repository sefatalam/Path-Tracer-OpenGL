#ifndef SCENE_H
#define SCENE_H

#include <glm/glm.hpp>

#include <cmath>
#include <string>
#include <vector>

#include "bvh.h"
#include "camera.h"
#include "material.h"
#include "model.h"
#include "prims.h"
#include "texture.h"

// A reproducible starting viewpoint, so a scene always opens on the shot it was composed for.
struct CameraSetup
{
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;
    float fov = 45.0f;

    // Authoring helper: place the camera at `from` aimed at `target`. Deriving yaw/pitch here
    // means scenes are written in terms of what to look at, not Euler angles.
    static CameraSetup lookingAt(const glm::vec3& from, const glm::vec3& target, float fov = 45.0f)
    {
        glm::vec3 d = glm::normalize(target - from);
        CameraSetup c;
        c.position = from;
        c.yaw = glm::degrees(std::atan2(d.z, d.x));
        c.pitch = glm::degrees(std::asin(glm::clamp(d.y, -1.0f, 1.0f)));
        c.fov = fov;
        return c;
    }

    Camera toCamera() const
    {
        Camera cam(position, glm::vec3(0.0f, 1.0f, 0.0f), yaw, pitch);
        cam.Zoom = fov;
        return cam;
    }
};

// Authoring-side scene description. Primitives reference materials by index and materials
// reference textures by index, mirroring the flat SSBO layout the compute shader expects.
// The add* helpers return that index so scene code never has to say `size() - 1`.
struct Scene
{
    std::vector<Texture>  textures;
    std::vector<Material> materials;
    std::vector<Sphere>   spheres;
    std::vector<Triangle> triangles;
    std::vector<Quad>     quads;

    CameraSetup camera;

    // Look and quality settings that belong to the scene rather than the renderer: a closed
    // Cornell box needs far more bounces than an open field, and emitter strengths differ
    // enough between scenes that one global exposure can't serve them all.
    float exposure = 1.0f;
    int maxDepth = 5;

    // Per-sample radiance ceiling used to suppress fireflies. The default is high enough to be
    // effectively off; lower it toward a scene's brightest emitter to trade a little caustic
    // energy for a clean image.
    float radianceClamp = 1.0e9f;

    int addTexture(const Texture& t)   { textures.push_back(t);  return static_cast<int>(textures.size())  - 1; }
    int addMaterial(const Material& m) { materials.push_back(m); return static_cast<int>(materials.size()) - 1; }

    void add(const Sphere& s)   { spheres.push_back(s); }
    void add(const Triangle& t) { triangles.push_back(t); }
    void add(const Quad& q)     { quads.push_back(q); }

    // Appends a mesh file's triangles, materials and textures to this scene.
    bool loadMesh(const std::string& path, const glm::mat4& transform = glm::mat4(1.0f),
                  const std::vector<std::string>& skipMaterials = {})
    {
        return loadModel(path, triangles, textures, materials, transform, skipMaterials);
    }

    // Emissive spheres and quads, for direct light sampling (NEE). Triangles are excluded
    // because the shader's light sampler only handles sphere and quad geometry.
    std::vector<BVHPrimRef> collectLights() const
    {
        std::vector<BVHPrimRef> lights;
        for (int i = 0; i < static_cast<int>(spheres.size()); ++i) {
            if (materials[spheres[i].material_index].type == DIFFUSE_LIGHT) {
                lights.push_back({ SPHERE, i });
            }
        }
        for (int i = 0; i < static_cast<int>(quads.size()); ++i) {
            if (materials[quads[i].material_index].type == DIFFUSE_LIGHT) {
                lights.push_back({ QUAD, i });
            }
        }
        return lights;
    }
};

#endif
