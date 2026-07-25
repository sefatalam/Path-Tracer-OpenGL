#ifndef SCENES_H
#define SCENES_H

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "scene.h"

// ---------------------------------------------------------------------------------------------
// Placement helpers
//
// Rays that escape the scene return black -- there is no sky (see raytracer.comp) -- so every
// scene here has to light itself with emissive geometry. That rules out the outdoor daylight
// look of the Ray Tracing in One Weekend cover and pushes these scenes toward studio lighting.
// ---------------------------------------------------------------------------------------------

// A sphere resting exactly on the ground plane. Getting this wrong by even a little is the
// fastest way to make a render look off, so it is worth never computing it by hand.
inline Sphere sphereOnGround(float x, float z, float radius, float groundY, int material)
{
    return Sphere(glm::vec3(x, groundY + radius, z), radius, material);
}

// A square of ground centred on the origin. Preferred over the usual enormous ground *sphere*:
// a flat quad has a thin bounding box, whereas a radius-100 sphere produces an AABB that
// overlaps every other primitive and degrades the whole BVH.
inline void addGroundQuad(Scene& scene, float halfExtent, float y, int material)
{
    const float span = halfExtent * 2.0f;
    scene.add(Quad(glm::vec3(-halfExtent, y, -halfExtent),
                   glm::vec3(0.0f, 0.0f, span),
                   glm::vec3(span, 0.0f, 0.0f),
                   material));
}

// An axis-aligned box, optionally spun about Y, built from six quads. `baseCenter` is the
// middle of the bottom face, so boxes are placed by where they sit rather than by their centre.
inline void addBox(Scene& scene, const glm::vec3& baseCenter, const glm::vec3& size,
                   float yawDegrees, int material)
{
    const float hx = size.x * 0.5f;
    const float hz = size.z * 0.5f;
    const float c = std::cos(glm::radians(yawDegrees));
    const float s = std::sin(glm::radians(yawDegrees));

    // Corner of the unit box in local space (sx, sz in {-1, 1}, sy in {0, 1}), spun and placed.
    auto corner = [&](float sx, float sy, float sz) {
        const float x = sx * hx;
        const float z = sz * hz;
        return baseCenter + glm::vec3(x * c - z * s, sy * size.y, x * s + z * c);
    };

    // A face given as one corner plus the two corners adjacent to it. Quads are two-sided in
    // the shader, so winding doesn't matter here.
    auto face = [&](const glm::vec3& o, const glm::vec3& a, const glm::vec3& b) {
        scene.add(Quad(o, a - o, b - o, material));
    };

    // Each face is an origin plus its two adjacent corners, so the corner diagonally opposite
    // the origin is implied by u and v and never needed explicitly.
    const glm::vec3 b00 = corner(-1, 0, -1), b10 = corner(1, 0, -1);
    const glm::vec3 b11 = corner(1, 0, 1),   b01 = corner(-1, 0, 1);
    const glm::vec3 t00 = corner(-1, 1, -1), t10 = corner(1, 1, -1);
    const glm::vec3 t01 = corner(-1, 1, 1);

    face(b00, b10, b01); // bottom
    face(t00, t01, t10); // top
    face(b00, b10, t00); // -z
    face(b01, b11, t01); // +z
    face(b00, b01, t00); // -x
    face(b10, b11, t10); // +x
}

// ---------------------------------------------------------------------------------------------
// Sphere field -- the hero shot
//
// Ray Tracing in One Weekend in spirit, not in detail: three feature spheres over a field of
// small ones. The differences are deliberate. The feature spheres are off-axis rather than in
// a row, the small spheres draw from a fixed palette instead of fully random colours, and the
// lighting is a big soft key plus a rim light rather than a sky dome. Placement is procedural
// with a fixed seed, so the layout needs no hand-tuning and is identical on every run --
// which is what makes screenshots reproducible after a code change.
// ---------------------------------------------------------------------------------------------
inline Scene buildSphereFieldScene()
{
    Scene scene;
    scene.exposure = 2.0f;
    scene.maxDepth = 8;
    scene.radianceClamp = 30.0f; // above the rim panel's 26, so mirrored reflections of it survive
    scene.camera = CameraSetup::lookingAt(glm::vec3(6.1f, 2.7f, 8.5f),
                                          glm::vec3(0.0f, 0.75f, -0.4f), 36.0f);

    const float groundY = 0.0f;

    int groundMat = scene.addMaterial(Material::Lambertian(
        scene.addTexture(Texture::Solid(glm::vec3(0.30f, 0.30f, 0.32f)))));
    addGroundQuad(scene, 26.0f, groundY, groundMat);

    // A fixed palette rather than random colours, and materials built once and reused, so the
    // buffers stay small and the scene reads as designed instead of noisy.
    const glm::vec3 diffusePalette[] = {
        glm::vec3(0.72f, 0.28f, 0.22f), // terracotta
        glm::vec3(0.85f, 0.62f, 0.25f), // amber
        glm::vec3(0.24f, 0.46f, 0.43f), // deep teal
        glm::vec3(0.60f, 0.62f, 0.66f), // slate
        glm::vec3(0.36f, 0.30f, 0.46f), // plum
    };
    std::vector<int> diffuseMats;
    for (const glm::vec3& c : diffusePalette) {
        diffuseMats.push_back(scene.addMaterial(Material::Lambertian(scene.addTexture(Texture::Solid(c)))));
    }

    const glm::vec3 metalPalette[] = {
        glm::vec3(0.95f, 0.93f, 0.88f), // steel
        glm::vec3(0.90f, 0.72f, 0.38f), // brass
        glm::vec3(0.80f, 0.82f, 0.85f), // chrome
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

    // Feature spheres: off-axis, so the eye travels rather than scanning a row.
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

    // Procedural field: jittered grid, random radius, weighted material choice, and a rejection
    // test so nothing intersects a feature sphere or its neighbours.
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
                material = accentMat; // a few glowing marbles, for warm bounce light near the ground
            }

            scene.add(Sphere(center, radius, material));
            placed.push_back({ center, radius });
        }
    }

    int keyLight = scene.addMaterial(Material::DiffuseLight(
        scene.addTexture(Texture::Solid(glm::vec3(1.0f, 0.96f, 0.90f))), 3.2f));
    int rimLight = scene.addMaterial(Material::DiffuseLight(
        scene.addTexture(Texture::Solid(glm::vec3(0.55f, 0.70f, 1.0f))), 26.0f));

    // Large soft key overhead, slightly to the camera's left, for broad highlights.
    scene.add(Quad(glm::vec3(-5.0f, 6.5f, -4.5f),
                   glm::vec3(7.0f, 0.0f, 0.0f),
                   glm::vec3(0.0f, 0.0f, 6.0f), keyLight));
    // Cool side light for edge separation against the black background. Pushed well outside the
    // camera frustum -- with no sky, a light panel inside the frame reads as a floating white
    // card rather than as lighting.
    scene.add(Quad(glm::vec3(-14.0f, 0.3f, -3.5f),
                   glm::vec3(0.0f, 0.0f, 7.0f),
                   glm::vec3(0.0f, 4.5f, 0.0f), rimLight));

    return scene;
}

// ---------------------------------------------------------------------------------------------
// Orrery -- the second field variant
//
// Same procedural spirit, different composition: concentric rings instead of a grid, weighted
// heavily toward metal and glass, lit by a warm key on one side and a cool fill on the other so
// the neutral materials pick up colour from opposite directions. Shot from low down.
// ---------------------------------------------------------------------------------------------
inline Scene buildOrreryScene()
{
    Scene scene;
    scene.exposure = 1.5f;
    scene.maxDepth = 8;
    scene.radianceClamp = 16.0f; // above the coloured panels' 12
    // Raised and pulled well back: the rings only read as rings from above, and the camera has
    // to clear the outermost ring or a single foreground sphere swallows the frame.
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

    // Repeats act as weights. Diffuse materials are well represented on purpose: only Lambertian
    // surfaces use direct light sampling, so an all-metal field converges far more slowly.
    const int ringMats[] = { chrome, brushed, brass, glass, ivory, ivory, slate, teal };
    const size_t ringMatCount = sizeof(ringMats) / sizeof(ringMats[0]);

    // Anchor at the centre, then rings that grow denser and smaller outward -- the size
    // hierarchy is what keeps the eye moving instead of scanning a uniform field.
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

    // Opposed coloured panels, tall and just outside the frustum: neutral metal picks up warm on
    // one flank and cool on the other, which is what gives this scene its look. Large area
    // matters here beyond softness -- metals reach lights only by chance, so big emitters are
    // what keep the reflections from staying noisy.
    // Note the z range starts near the camera rather than behind the subject: the frustum widens
    // with distance, so a panel that clears the frame at its near end can still swing into view
    // at its far end. Checking the panel's corners, not its centre, is what matters.
    scene.add(Quad(glm::vec3(-8.5f, 0.2f, -1.0f),
                   glm::vec3(0.0f, 0.0f, 6.0f),
                   glm::vec3(0.0f, 6.0f, 0.0f), warmKey));
    scene.add(Quad(glm::vec3(8.5f, 0.2f, -1.0f),
                   glm::vec3(0.0f, 0.0f, 6.0f),
                   glm::vec3(0.0f, 6.0f, 0.0f), coolFill));
    // Overhead source so the tops of the spheres aren't left black.
    scene.add(Quad(glm::vec3(-3.0f, 9.0f, -3.0f),
                   glm::vec3(6.0f, 0.0f, 0.0f),
                   glm::vec3(0.0f, 0.0f, 6.0f),
                   scene.addMaterial(Material::DiffuseLight(
                       scene.addTexture(Texture::Solid(glm::vec3(1.0f))), 4.0f))));

    return scene;
}

// ---------------------------------------------------------------------------------------------
// Cornell box
//
// The standard GI test: a closed white room with one red and one green wall, lit by a single
// ceiling panel. Colour bleeding onto the boxes is the thing it proves. A glass sphere sits on
// the short box to show dielectrics against a bright diffuse background.
//
// Needs a high bounce count -- light only reaches most of the room after several bounces, so at
// the default depth the corners go black.
// ---------------------------------------------------------------------------------------------
inline Scene buildCornellBoxScene()
{
    Scene scene;
    scene.exposure = 1.0f;
    scene.maxDepth = 12;
    scene.radianceClamp = 18.0f; // just above the ceiling panel's 14, so the light itself is untouched
    // The classic Cornell box is a cube, which cannot fill a 4:3 frame -- fitting its width
    // crops the height and fitting its height leaves black bands at the sides. The room is
    // widened to 7x5 so the opening matches the window's aspect and the frame fills cleanly.
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

    // Room: x in [-3.5, 3.5], y in [0, 5], z in [-5, 0], open at the front so the camera sees in.
    scene.add(Quad(glm::vec3(-3.5f, 0.0f, -5.0f),   // floor
                   glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(7.0f, 0.0f, 0.0f), white));
    scene.add(Quad(glm::vec3(-3.5f, 5.0f, -5.0f),   // ceiling
                   glm::vec3(7.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 5.0f), white));
    scene.add(Quad(glm::vec3(-3.5f, 0.0f, -5.0f),   // back wall
                   glm::vec3(7.0f, 0.0f, 0.0f), glm::vec3(0.0f, 5.0f, 0.0f), white));
    scene.add(Quad(glm::vec3(-3.5f, 0.0f, -5.0f),   // left wall
                   glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(0.0f, 0.0f, 5.0f), red));
    scene.add(Quad(glm::vec3(3.5f, 0.0f, -5.0f),    // right wall
                   glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(0.0f, 0.0f, 5.0f), green));

    // Ceiling panel, inset and dropped a hair below the ceiling so it isn't coplanar with it.
    scene.add(Quad(glm::vec3(-1.05f, 4.97f, -3.45f),
                   glm::vec3(2.1f, 0.0f, 0.0f),
                   glm::vec3(0.0f, 0.0f, 1.9f), lightMat));

    addBox(scene, glm::vec3(-1.45f, 0.0f, -3.05f), glm::vec3(1.6f, 3.0f, 1.6f),  18.0f, white);
    addBox(scene, glm::vec3( 1.45f, 0.0f, -1.55f), glm::vec3(1.6f, 1.5f, 1.6f), -17.0f, white);

    scene.add(Sphere(glm::vec3(1.45f, 1.5f + 0.58f, -1.55f), 0.58f,
                     scene.addMaterial(Material::Dielectric(1.5f))));

    return scene;
}

// ---------------------------------------------------------------------------------------------
// Specular assessment scene: neutral gray ground, a row of metal spheres with increasing fuzz
// (roughness) plus one glass sphere, and three lights of different sizes but equal emission
// strength, so highlight *softness* can be compared side by side. This one is a measuring tool
// rather than a composition -- the even spacing that makes it easy to read also makes it a poor
// hero image.
// ---------------------------------------------------------------------------------------------
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

    // Fuzz increases left to right: 0.0 (mirror) -> 0.5 (very rough), then glass at the end.
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

    // Small, tight light -> sharp highlights.
    scene.add(Quad(glm::vec3(-2.15f, 2.0f, -0.65f),
                   glm::vec3( 0.3f,  0.0f,  0.0f),
                   glm::vec3( 0.0f,  0.0f,  0.3f), lightMat));
    // Medium light -> moderately soft highlights.
    scene.add(Quad(glm::vec3(-0.4f, 2.5f, -1.9f),
                   glm::vec3( 0.8f, 0.0f,  0.0f),
                   glm::vec3( 0.0f, 0.0f,  0.8f), lightMat));
    // Large, soft light -> broad highlights.
    scene.add(Quad(glm::vec3(1.25f, 2.0f, -3.25f),
                   glm::vec3(1.5f,  0.0f,  0.0f),
                   glm::vec3(0.0f,  0.0f,  1.5f), lightMat));

    // scene.loadMesh("src/models/sasha/source/Sasha.fbx",
    //                glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, -0.5f, -2.0f)), {"Outline"});

    return scene;
}

// ---------------------------------------------------------------------------------------------
// Catalogue, so scenes can be selected by name on the command line.
// ---------------------------------------------------------------------------------------------
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

// Returns nullptr if no scene by that name exists.
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
