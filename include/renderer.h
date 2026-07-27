#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>

#include "bvh.h"
#include "camera.h"
#include "gl_util.h"
#include "scene.h"
#include "shader.h"
#include "shader_compute.h"

namespace ssbo
{
    constexpr GLuint MATERIALS = 1;
    constexpr GLuint SPHERES   = 2;
    constexpr GLuint TRIANGLES = 3;
    constexpr GLuint QUADS     = 4;
    constexpr GLuint BVH_NODES = 5;
    constexpr GLuint TEXTURES  = 6;
    constexpr GLuint BVH_REFS  = 7;
    constexpr GLuint LIGHTS    = 8;
}

class Renderer
{
public:
    Renderer(const Scene& scene, int width, int height)
        : tracer("src/raytracer.comp"), present("src/quad.vs", "src/quad.fs"),
          fbWidth(width), fbHeight(height),
          exposure(scene.exposure), radianceClamp(scene.radianceClamp)
    {
        std::vector<PrimitiveRef> refs = buildRefs(scene.spheres, scene.triangles, scene.quads);
        BVHBuilder builder;
        builder.build(refs, 0, static_cast<int>(refs.size()));
        // printBVHStats(builder);

        std::vector<BVHPrimRef> lights = scene.collectLights();
        lightCount = static_cast<int>(lights.size());

        buffers = {
            uploadSSBO(ssbo::MATERIALS, scene.materials),
            uploadSSBO(ssbo::SPHERES,   scene.spheres),
            uploadSSBO(ssbo::TRIANGLES, scene.triangles),
            uploadSSBO(ssbo::QUADS,     scene.quads),
            uploadSSBO(ssbo::BVH_NODES, builder.nodes),
            uploadSSBO(ssbo::TEXTURES,  scene.textures),
            uploadSSBO(ssbo::BVH_REFS,  builder.primRefs),
            uploadSSBO(ssbo::LIGHTS,    lights),
        };

        createAccumTexture();
    }

    ~Renderer()
    {
        glDeleteBuffers(static_cast<GLsizei>(buffers.size()), buffers.data());
        glDeleteTextures(1, &accumTexture);
    }

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void resize(int width, int height)
    {
        if (width <= 0 || height <= 0 || (width == fbWidth && height == fbHeight)) {
            return;
        }
        fbWidth = width;
        fbHeight = height;
        glDeleteTextures(1, &accumTexture);
        createAccumTexture();
    }

    void render(const Camera& camera, float time)
    {
        if (fbWidth <= 0 || fbHeight <= 0) {
            return;
        }

        if (camera.Position != lastCamPos || camera.Front != lastCamFront || camera.Zoom != lastZoom) {
            accumFrame = 0;
            lastCamPos = camera.Position;
            lastCamFront = camera.Front;
            lastZoom = camera.Zoom;
        }

        tracer.use();
        tracer.setVec3("camPos", camera.Position);
        tracer.setVec3("camFront", camera.Front);
        tracer.setVec3("camRight", camera.Right);
        tracer.setVec3("camUp", camera.Up);
        tracer.setFloat("vfov", camera.Zoom);

        tracer.setInt("samplesPerPixel", SAMPLES_PER_PIXEL);
        tracer.setInt("maxDepth", maxDepth);
        tracer.setFloat("radianceClamp", radianceClamp);
        tracer.setFloat("time", time);
        tracer.setInt("accumFrame", accumFrame);
        tracer.setInt("lightCount", lightCount);

        glDispatchCompute((fbWidth  + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE,
                          (fbHeight + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        accumFrame++;

        glViewport(0, 0, fbWidth, fbHeight);
        glClear(GL_COLOR_BUFFER_BIT);

        present.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, accumTexture);
        present.setInt("screenTexture", 0);
        present.setFloat("exposure", exposure);
        quad.draw();
    }

    void setExposure(float value) { exposure = value; }
    float getExposure() const { return exposure; }

    int getMaxDepth() const { return maxDepth; }

private:
    static constexpr int WORKGROUP_SIZE = 8;
    static constexpr int SAMPLES_PER_PIXEL = 2;

    ComputeShader tracer;
    Shader present;
    FullscreenQuad quad;

    std::vector<GLuint> buffers;
    GLuint accumTexture = 0;

    int fbWidth = 0;
    int fbHeight = 0;
    int lightCount = 0;

    float exposure = 1.0f;
    int maxDepth = 8;
    float radianceClamp = 1.0e9f;

    int accumFrame = 0;
    glm::vec3 lastCamPos = glm::vec3(0.0f);
    glm::vec3 lastCamFront = glm::vec3(0.0f);
    float lastZoom = 0.0f;

    void createAccumTexture()
    {
        accumTexture = createFloatTexture(fbWidth, fbHeight);
        glBindImageTexture(0, accumTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
        accumFrame = 0;
    }
};

#endif
