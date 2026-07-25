#include <cstdio>

#include "app.h"
#include "renderer.h"
#include "scenes.h"

const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1080;

// Hold E / Q to brighten / darken. Exposure is multiplied rather than added so each step is
// an equal perceptual stop, and the value is printed so a good setting can be copied into
// the scene definition.
void updateExposure(const App& app, Renderer& renderer)
{
    const float rate = 1.5f; // stops per second

    float scale = 1.0f;
    if (app.isKeyDown(GLFW_KEY_E)) scale = 1.0f + rate * app.delta();
    if (app.isKeyDown(GLFW_KEY_Q)) scale = 1.0f / (1.0f + rate * app.delta());
    if (scale == 1.0f) {
        return;
    }

    renderer.setExposure(renderer.getExposure() * scale);
    std::printf("exposure %.3f\n", renderer.getExposure());
    std::fflush(stdout);
}

int main(int argc, char** argv)
{
    const SceneEntry* entry = findScene(argc > 1 ? argv[1] : "field");
    if (entry == nullptr) {
        std::printf("unknown scene '%s'\navailable scenes:\n", argv[1]);
        for (const SceneEntry& e : sceneCatalog()) {
            std::printf("  %-10s %s\n", e.name, e.description);
        }
        return -1;
    }

    // Declared first so it outlives the Renderer: GL objects must be destroyed while the
    // context is still current.
    App app(WINDOW_WIDTH, WINDOW_HEIGHT, "GLRT");
    if (!app.ok()) {
        return -1;
    }

    Scene scene = entry->build();
    app.camera = scene.camera.toCamera();

    Renderer renderer(scene, app.width(), app.height());
    std::printf("scene '%s' | exposure %.2f | maxDepth %d\n",
                entry->name, scene.exposure, scene.maxDepth);
    std::fflush(stdout);

    while (!app.shouldClose()) {
        app.beginFrame();

        if (app.consumeResize()) {
            renderer.resize(app.width(), app.height());
        }

        updateExposure(app, renderer);

        renderer.render(app.camera, app.time());
        app.endFrame();
    }

    return 0;
}
