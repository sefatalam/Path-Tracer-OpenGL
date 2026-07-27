#ifndef SCENES_H
#define SCENES_H

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "scene.h"

// Helper Functions
inline Sphere sphereOnGround(float x, float z, float radius, float groundY, int material)
{
    return Sphere(glm::vec3(x, groundY + radius, z), radius, material);
}

inline void addGroundQuad(Scene& scene, float halfExtent, float y, int material)
{
    const float span = halfExtent * 2.0f;
    scene.add(Quad(glm::vec3(-halfExtent, y, -halfExtent),
                   glm::vec3(0.0f, 0.0f, span),
                   glm::vec3(span, 0.0f, 0.0f),
                   material));
}

inline void addBox(Scene& scene, const glm::vec3& baseCenter, const glm::vec3& size,
                   float yawDegrees, int material)
{
    const float hx = size.x * 0.5f;
    const float hz = size.z * 0.5f;
    const float c = std::cos(glm::radians(yawDegrees));
    const float s = std::sin(glm::radians(yawDegrees));

    auto corner = [&](float sx, float sy, float sz) {
        const float x = sx * hx;
        const float z = sz * hz;
        return baseCenter + glm::vec3(x * c - z * s, sy * size.y, x * s + z * c);
    };

    auto face = [&](const glm::vec3& o, const glm::vec3& a, const glm::vec3& b) {
        scene.add(Quad(o, a - o, b - o, material));
    };

    const glm::vec3 b00 = corner(-1, 0, -1), b10 = corner(1, 0, -1);
    const glm::vec3 b11 = corner(1, 0, 1),   b01 = corner(-1, 0, 1);
    const glm::vec3 t00 = corner(-1, 1, -1), t10 = corner(1, 1, -1);
    const glm::vec3 t01 = corner(-1, 1, 1);

    face(b00, b10, b01);
    face(t00, t01, t10);
    face(b00, b10, t00);
    face(b01, b11, t01);
    face(b00, b01, t00);
    face(b10, b11, t10);
}

/////////////////////////////////////////////////////////////////////
inline Scene buildSphereFieldScene()
{
    Scene scene;
    scene.exposure = 2.0f;
    scene.maxDepth = 8;
    scene.radianceClamp = 30.0f;
    scene.camera = CameraSetup::lookingAt(glm::vec3(6.1f, 2.7f, 8.5f),
                                          glm::vec3(0.0f, 0.75f, -0.4f), 36.0f);

    const float groundY = 0.0f;

    int groundMat = scene.addMaterial(Material::Lambertian(
        scene.addTexture(Texture::Solid(glm::vec3(0.30f, 0.30f, 0.32f)))));
    addGroundQuad(scene, 26.0f, groundY, groundMat);

    const glm::vec3 diffusePalette[] = {
        glm::vec3(0.72f, 0.28f, 0.22f),
        glm::vec3(0.85f, 0.62f, 0.25f),
        glm::vec3(0.24f, 0.46f, 0.43f),
        glm::vec3(0.60f, 0.62f, 0.66f),
        glm::vec3(0.36f, 0.30f, 0.46f),
    };
    std::vector<int> diffuseMats;
    for (const glm::vec3& c : diffusePalette) {
        diffuseMats.push_back(scene.addMaterial(Material::Lambertian(scene.addTexture(Texture::Solid(c)))));
    }

    const glm::vec3 metalPalette[] = {
        glm::vec3(0.95f, 0.93f, 0.88f),
        glm::vec3(0.90f, 0.72f, 0.38f),
        glm::vec3(0.80f, 0.82f, 0.85f),
    };
    std::vector<int> metalMats;
    for (const glm::vec3& c : metalPalette) {
        int tex = scene.addTexture(Texture::Solid(c));
        metalMats.push_back(scene.addMaterial(Material::Metal(tex, 0.0f)));
        metalMats.push_back(scene.addMaterial(Material::Metal(tex, 0.18f)));
    }

    int glassMat = scene.addMaterial(Material::Dielectric(1.5f));
    int accentMat = scene.addMaterial(Material::DiffuseLight(
        scene.addTexture(Texture::Solid(glm::vec3(1.0f, 0.75f, 0.45f))), 9.0f));

    struct Feature { glm::vec3 center; float radius; };
    std::vector<Feature> features;

    auto addFeature = [&](float x, float z, float radius, int material) {
        Sphere s = sphereOnGround(x, z, radius, groundY, material);
        scene.add(s);
        features.push_back({ s.center, radius });
    };

    addFeature( 0.10f,  0.20f, 1.00f, glassMat);
    addFeature( 2.75f, -0.90f, 0.85f, scene.addMaterial(Material::Metal(
        scene.addTexture(Texture::Solid(glm::vec3(0.94f, 0.90f, 0.84f))), 0.03f)));
    addFeature(-2.55f,  0.55f, 0.75f, scene.addMaterial(Material::Lambertian(
        scene.addTexture(Texture::Solid(glm::vec3(0.68f, 0.24f, 0.20f))))));

    std::mt19937 rng(20240719u);
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);
    auto rand01 = [&]() { return uni(rng); };

    std::vector<Feature> placed = features;

    for (int gx = -6; gx <= 6; ++gx) {
        for (int gz = -5; gz <= 3; ++gz) {
            const float radius = 0.13f + rand01() * 0.16f;
            const float x = gx + 0.85f * (rand01() - 0.5f);
            const float z = gz + 0.85f * (rand01() - 0.5f);
            const glm::vec3 center(x, groundY + radius, z);

            bool clear = true;
            for (const Feature& f : placed) {
                if (glm::distance(center, f.center) < f.radius + radius + 0.06f) {
                    clear = false;
                    break;
                }
            }
            if (!clear) {
                continue;
            }

            const float roll = rand01();
            int material;
            if (roll < 0.54f) {
                material = diffuseMats[static_cast<size_t>(rand01() * diffuseMats.size()) % diffuseMats.size()];
            } else if (roll < 0.82f) {
                material = metalMats[static_cast<size_t>(rand01() * metalMats.size()) % metalMats.size()];
            } else if (roll < 0.94f) {
                material = glassMat;
            } else {
                material = accentMat;
            }

            scene.add(Sphere(center, radius, material));
            placed.push_back({ center, radius });
        }
    }

    int keyLight = scene.addMaterial(Material::DiffuseLight(
        scene.addTexture(Texture::Solid(glm::vec3(1.0f, 0.96f, 0.90f))), 3.2f));
    int rimLight = scene.addMaterial(Material::DiffuseLight(
        scene.addTexture(Texture::Solid(glm::vec3(0.55f, 0.70f, 1.0f))), 26.0f));

    scene.add(Quad(glm::vec3(-5.0f, 6.5f, -4.5f),
                   glm::vec3(7.0f, 0.0f, 0.0f),
                   glm::vec3(0.0f, 0.0f, 6.0f), keyLight));
    scene.add(Quad(glm::vec3(-14.0f, 0.3f, -3.5f),
                   glm::vec3(0.0f, 0.0f, 7.0f),
                   glm::vec3(0.0f, 4.5f, 0.0f), rimLight));

    return scene;
}

/////////////////////////////////////////////////////////////////////
inline Scene buildOrreryScene()
{
    Scene scene;
    scene.exposure = 1.5f;
    scene.maxDepth = 8;
    scene.radianceClamp = 16.0f;
    scene.camera = CameraSetup::lookingAt(glm::vec3(3.0f, 5.5f, 13.5f),
                                          glm::vec3(0.0f, 0.4f, 0.0f), 36.0f);

    const float groundY = 0.0f;

    int groundMat = scene.addMaterial(Material::Lambertian(
        scene.addTexture(Texture::Solid(glm::vec3(0.26f, 0.26f, 0.29f)))));
    addGroundQuad(scene, 34.0f, groundY, groundMat);

    int chrome = scene.addMaterial(Material::Metal(
        scene.addTexture(Texture::Solid(glm::vec3(0.95f, 0.95f, 0.97f))), 0.0f));
    int brushed = scene.addMaterial(Material::Metal(
        scene.addTexture(Texture::Solid(glm::vec3(0.86f, 0.87f, 0.90f))), 0.14f));
    int brass = scene.addMaterial(Material::Metal(
        scene.addTexture(Texture::Solid(glm::vec3(0.92f, 0.74f, 0.40f))), 0.06f));
    int glass = scene.addMaterial(Material::Dielectric(1.5f));
    int ivory = scene.addMaterial(Material::Lambertian(
        scene.addTexture(Texture::Solid(glm::vec3(0.85f, 0.82f, 0.75f)))));
    int slate = scene.addMaterial(Material::Lambertian(
        scene.addTexture(Texture::Solid(glm::vec3(0.42f, 0.45f, 0.50f)))));
    int teal = scene.addMaterial(Material::Lambertian(
        scene.addTexture(Texture::Solid(glm::vec3(0.20f, 0.45f, 0.45f)))));

    const int ringMats[] = { chrome, brushed, brass, glass, ivory, ivory, slate, teal };
    const size_t ringMatCount = sizeof(ringMats) / sizeof(ringMats[0]);

    scene.add(sphereOnGround(0.0f, 0.0f, 1.15f, groundY, chrome));

    std::mt19937 rng(77712u);
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);
    auto rand01 = [&]() { return uni(rng); };

    const float twoPi = 6.28318530718f;
    for (int ring = 0; ring < 4; ++ring) {
        const float ringRadius = 1.95f + ring * 1.05f;
        const int count = 7 + ring * 4;
        const float radius = 0.44f - ring * 0.05f;

        for (int i = 0; i < count; ++i) {
            const float angle = (static_cast<float>(i) + 0.55f * (rand01() - 0.5f)) / count * twoPi
                                + ring * 0.37f;
            const float r = ringRadius + 0.30f * (rand01() - 0.5f);
            const float sphereRadius = radius * (0.78f + 0.44f * rand01());

            scene.add(sphereOnGround(std::cos(angle) * r, std::sin(angle) * r,
                                     sphereRadius, groundY,
                                     ringMats[static_cast<size_t>(rand01() * ringMatCount) % ringMatCount]));
        }
    }

    int warmKey = scene.addMaterial(Material::DiffuseLight(
        scene.addTexture(Texture::Solid(glm::vec3(1.0f, 0.62f, 0.30f))), 12.0f));
    int coolFill = scene.addMaterial(Material::DiffuseLight(
        scene.addTexture(Texture::Solid(glm::vec3(0.35f, 0.55f, 1.0f))), 12.0f));

    scene.add(Quad(glm::vec3(-8.5f, 0.2f, -1.0f),
                   glm::vec3(0.0f, 0.0f, 6.0f),
                   glm::vec3(0.0f, 6.0f, 0.0f), warmKey));
    scene.add(Quad(glm::vec3(8.5f, 0.2f, -1.0f),
                   glm::vec3(0.0f, 0.0f, 6.0f),
                   glm::vec3(0.0f, 6.0f, 0.0f), coolFill));
    scene.add(Quad(glm::vec3(-3.0f, 9.0f, -3.0f),
                   glm::vec3(6.0f, 0.0f, 0.0f),
                   glm::vec3(0.0f, 0.0f, 6.0f),
                   scene.addMaterial(Material::DiffuseLight(
                       scene.addTexture(Texture::Solid(glm::vec3(1.0f))), 4.0f))));

    return scene;
}

/////////////////////////////////////////////////////////////////////
inline Scene buildCornellBoxScene()
{
    Scene scene;
    scene.exposure = 1.0f;
    scene.maxDepth = 12;
    scene.radianceClamp = 18.0f;
    scene.camera = CameraSetup::lookingAt(glm::vec3(0.0f, 2.5f, 7.0f),
                                          glm::vec3(0.0f, 2.5f, -2.5f), 38.0f);

    int white = scene.addMaterial(Material::Lambertian(
        scene.addTexture(Texture::Solid(glm::vec3(0.73f, 0.73f, 0.73f)))));
    int red = scene.addMaterial(Material::Lambertian(
        scene.addTexture(Texture::Solid(glm::vec3(0.65f, 0.05f, 0.05f)))));
    int green = scene.addMaterial(Material::Lambertian(
        scene.addTexture(Texture::Solid(glm::vec3(0.12f, 0.45f, 0.15f)))));
    int lightMat = scene.addMaterial(Material::DiffuseLight(
        scene.addTexture(Texture::Solid(glm::vec3(1.0f, 0.95f, 0.88f))), 14.0f));

    scene.add(Quad(glm::vec3(-3.5f, 0.0f, -5.0f),
                   glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(7.0f, 0.0f, 0.0f), white));
    scene.add(Quad(glm::vec3(-3.5f, 5.0f, -5.0f),
                   glm::vec3(7.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 5.0f), white));
    scene.add(Quad(glm::vec3(-3.5f, 0.0f, -5.0f),
                   glm::vec3(7.0f, 0.0f, 0.0f), glm::vec3(0.0f, 5.0f, 0.0f), white));
    scene.add(Quad(glm::vec3(-3.5f, 0.0f, -5.0f),
                   glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(0.0f, 0.0f, 5.0f), red));
    scene.add(Quad(glm::vec3(3.5f, 0.0f, -5.0f),
                   glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(0.0f, 0.0f, 5.0f), green));

    scene.add(Quad(glm::vec3(-1.05f, 4.97f, -3.45f),
                   glm::vec3(2.1f, 0.0f, 0.0f),
                   glm::vec3(0.0f, 0.0f, 1.9f), lightMat));

    addBox(scene, glm::vec3(-1.45f, 0.0f, -3.05f), glm::vec3(1.6f, 3.0f, 1.6f),  18.0f, white);
    addBox(scene, glm::vec3( 1.45f, 0.0f, -1.55f), glm::vec3(1.6f, 1.5f, 1.6f), -17.0f, white);

    scene.add(Sphere(glm::vec3(1.45f, 1.5f + 0.58f, -1.55f), 0.58f,
                     scene.addMaterial(Material::Dielectric(1.5f))));

    return scene;
}

/////////////////////////////////////////////////////////////////////
inline Scene buildSpecularTestScene()
{
    Scene scene;
    scene.exposure = 2.5f;
    scene.maxDepth = 5;
    scene.camera = CameraSetup::lookingAt(glm::vec3(2.6f, 1.5f, 3.4f),
                                          glm::vec3(0.0f, 0.05f, -1.0f), 45.0f);

    int groundMat = scene.addMaterial(Material::Lambertian(
        scene.addTexture(Texture::Solid(glm::vec3(0.5f, 0.5f, 0.5f)))));

    int silver = scene.addTexture(Texture::Solid(glm::vec3(0.8f, 0.8f, 0.85f)));

    const float fuzzLevels[] = { 0.0f, 0.1f, 0.2f, 0.35f, 0.5f };

    int lightMat = scene.addMaterial(Material::DiffuseLight(
        scene.addTexture(Texture::Solid(glm::vec3(1.0f, 1.0f, 1.0f))), 6.0f));

    scene.add(Sphere(glm::vec3(0.0f, -100.5f, -1.0f), 100.0f, groundMat));

    const float spacing = 0.75f;
    const float startX  = -1.875f;
    for (int i = 0; i < 5; ++i) {
        int mat = scene.addMaterial(Material::Metal(silver, fuzzLevels[i]));
        scene.add(Sphere(glm::vec3(startX + i * spacing, 0.0f, -1.0f), 0.5f, mat));
    }
    scene.add(Sphere(glm::vec3(startX + 5 * spacing, 0.0f, -1.0f), 0.5f,
                     scene.addMaterial(Material::Dielectric(1.5f))));

    scene.add(Quad(glm::vec3(-2.15f, 2.0f, -0.65f),
                   glm::vec3( 0.3f,  0.0f,  0.0f),
                   glm::vec3( 0.0f,  0.0f,  0.3f), lightMat));
    scene.add(Quad(glm::vec3(-0.4f, 2.5f, -1.9f),
                   glm::vec3( 0.8f, 0.0f,  0.0f),
                   glm::vec3( 0.0f, 0.0f,  0.8f), lightMat));
    scene.add(Quad(glm::vec3(1.25f, 2.0f, -3.25f),
                   glm::vec3(1.5f,  0.0f,  0.0f),
                   glm::vec3(0.0f,  0.0f,  1.5f), lightMat));

    return scene;
}

struct SceneEntry
{
    const char* name;
    const char* description;
    Scene (*build)();
};

inline const std::vector<SceneEntry>& sceneCatalog()
{
    static const std::vector<SceneEntry> catalog = {
        { "field",    "sphere field over a lit plain (default)", &buildSphereFieldScene },
        { "orrery",   "chrome and glass rings, warm/cool lights", &buildOrreryScene },
        { "cornell",  "Cornell box with a glass sphere",          &buildCornellBoxScene },
        { "specular", "roughness comparison row (test scene)",    &buildSpecularTestScene },
    };
    return catalog;
}

inline const SceneEntry* findScene(const std::string& name)
{
    for (const SceneEntry& entry : sceneCatalog()) {
        if (name == entry.name) {
            return &entry;
        }
    }
    return nullptr;
}

#endif
