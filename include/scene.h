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

struct CameraSetup
{
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;
    float fov = 45.0f;

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

struct Scene
{
    std::vector<Texture>  textures;
    std::vector<Material> materials;
    std::vector<Sphere>   spheres;
    std::vector<Triangle> triangles;
    std::vector<Quad>     quads;

    CameraSetup camera;

    float exposure = 1.0f;
    int maxDepth = 5;

    float radianceClamp = 1.0e9f;

    int addTexture(const Texture& t)   { textures.push_back(t);  return static_cast<int>(textures.size())  - 1; }
    int addMaterial(const Material& m) { materials.push_back(m); return static_cast<int>(materials.size()) - 1; }

    void add(const Sphere& s)   { spheres.push_back(s); }
    void add(const Triangle& t) { triangles.push_back(t); }
    void add(const Quad& q)     { quads.push_back(q); }

    bool loadMesh(const std::string& path, const glm::mat4& transform = glm::mat4(1.0f),
                  const std::vector<std::string>& skipMaterials = {})
    {
        return loadModel(path, triangles, textures, materials, transform, skipMaterials);
    }

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
