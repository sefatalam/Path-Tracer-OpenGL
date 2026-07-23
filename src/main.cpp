#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>

#include <camera.h>
#include <shader.h>
#include <shader_compute.h>
#include <prims.h>
#include <texture.h>
#include <material.h>
#include <bvh.h>
#include <model.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
void renderQuad();

// Constants
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

const unsigned int TEXTURE_WIDTH = 512;
const unsigned int TEXTURE_HEIGHT = 512;

const int samplesPerPixel = 2;
const int maxDepth = 5;

// Camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool initialMouse = true;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

int main() {
    // GLFW Init
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create Window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "GLRT", NULL, NULL);
    if (window == NULL) 
    {
        std::cout << "Failed to create window" << "\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // cap to vsync so the compute shader doesn't peg the GPU at 100% uncapped
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Load OpenGL functions (GLAD)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << "\n";
        return -1;
    }

    if (!GLAD_GL_ARB_bindless_texture)
    {
        std::cout << "GL_ARB_bindless_texture not supported on this GPU/driver" << "\n";
        return -1;
    }

    // Shaders
    ComputeShader compShader("src/raytracer.comp");
    Shader quadShader("src/quad.vs", "src/quad.fs");


    // Create Output Texture + Attach to Framebuffer (Background)
    
    unsigned int sceneFBO, computeTexture, sceneTexture;
    glGenTextures(1, &computeTexture);
    glBindTexture(GL_TEXTURE_2D, computeTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(0, computeTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

    glGenTextures(1, &sceneTexture);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);

    glGenFramebuffers(1, &sceneFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "ERROR:FRAMEBUFFER:: Scene framebuffer incomplete" << "\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Scene Builder
    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<Sphere> spheres;
    std::vector<Triangle> triangles;
    std::vector<Quad> quads;

    textures.push_back(Texture::Solid(glm::vec3(0.8f, 0.8f, 0.0f)));
    materials.push_back(Material::Lambertian((int)textures.size() - 1));
    int material_ground = materials.size() - 1;

    textures.push_back(Texture::Solid(glm::vec3(0.1f, 0.2f, 0.5f)));
    materials.push_back(Material::Lambertian((int)textures.size() - 1));
    int material_center = materials.size() - 1;

    materials.push_back(Material::Dielectric(1.5f));
    int material_left = materials.size() - 1;

    textures.push_back(Texture::Image("src/textures/earth.jpg"));
    materials.push_back(Material::Metal((int)textures.size() - 1, 0.0f));
    int material_right = materials.size() - 1;

    textures.push_back(Texture::Solid(glm::vec3(0.2f, 0.8f, 0.3f)));
    materials.push_back(Material::Lambertian((int)textures.size() - 1));
    int material_triangle = materials.size() - 1;

    textures.push_back(Texture::Image("src/textures/earth.jpg"));
    materials.push_back(Material::Lambertian((int)textures.size() - 1));
    int material_quad = materials.size() - 1;

    // Lightbulb: a small emissive sphere. Strength > 1.0 is expected since the
    // texture color alone (max 1.0 per channel) would just look like a bright
    // diffuse surface, not an actual light source.
    textures.push_back(Texture::Solid(glm::vec3(1.0f, 1.0f, 1.0f)));
    materials.push_back(Material::DiffuseLight((int)textures.size() - 1, 8.0f));
    int material_light = materials.size() - 1;

    spheres.push_back(Sphere(glm::vec3( 0.0f, -100.5f, -1.0f), 100.0f, material_ground));
    spheres.push_back(Sphere(glm::vec3( 0.0f,    0.0f, -1.0f),   0.5f, material_center));
    spheres.push_back(Sphere(glm::vec3(-1.0f,    0.0f, -1.0f),   0.5f, material_left));
    spheres.push_back(Sphere(glm::vec3( 1.0f,    0.0f, -1.0f),   0.5f, material_right));
    spheres.push_back(Sphere(glm::vec3( 0.0f,    2.0f, -1.0f),   0.3f, material_light));

    triangles.push_back(Triangle(glm::vec3(1.7f, -0.2f, -1.0f),
                                  glm::vec3(2.3f, -0.2f, -1.0f),
                                  glm::vec3(2.0f,  0.6f, -1.0f),
                                  material_triangle));

    quads.push_back(Quad(glm::vec3(-2.5f, -0.5f, -1.3f),
                          glm::vec3( 0.6f,  0.0f,  0.0f),
                          glm::vec3( 0.0f,  0.6f,  0.0f),
                          material_quad));

    // Loads models

    // glm::mat4 modelTransform = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, -0.5f, -2.0f));
    // loadModel("src/models/sasha/source/Sasha.fbx", triangles, textures, materials, modelTransform, {"Outline"});

    auto refs = buildRefs(spheres, triangles, quads);
    BVHBuilder builder;
    builder.build(refs, 0, static_cast<int>(refs.size()));

    {
        int leafCount = 0;
        int maxLeafSize = 0;
        long long leafPrimTotal = 0;
        for (const BVHNode& n : builder.nodes) {
            if (n.prim_type == LEAF) {
                leafCount++;
                leafPrimTotal += n.count;
                maxLeafSize = std::max(maxLeafSize, n.count);
            }
        }
        std::cout << "BVH stats: " << builder.nodes.size() << " nodes, "
                  << leafCount << " leaves, " << builder.primRefs.size() << " prim refs, "
                  << "avg leaf size " << (leafCount ? (double)leafPrimTotal / leafCount : 0.0)
                  << ", max leaf size " << maxLeafSize << "\n";
    }

    // Collect emissive spheres/quads for direct light sampling (NEE covers sphere + quad lights only)
    std::vector<BVHPrimRef> lights;
    for (int i = 0; i < (int)spheres.size(); ++i) {
        if (materials[spheres[i].material_index].type == DIFFUSE_LIGHT) {
            lights.push_back({ SPHERE, i });
        }
    }
    for (int i = 0; i < (int)quads.size(); ++i) {
        if (materials[quads[i].material_index].type == DIFFUSE_LIGHT) {
            lights.push_back({ QUAD, i });
        }
    }

    // Send Scene to GPU
    unsigned int matSSBO;
    glGenBuffers(1, &matSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, matSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, materials.size() * sizeof(Material), materials.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, matSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    unsigned int sphereSSBO;
    glGenBuffers(1, &sphereSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphereSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, spheres.size() * sizeof(Sphere), spheres.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sphereSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    unsigned int triangleSSBO;
    glGenBuffers(1, &triangleSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, triangleSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, triangles.size() * sizeof(Triangle), triangles.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, triangleSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    unsigned int quadSSBO;
    glGenBuffers(1, &quadSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, quadSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, quads.size() * sizeof(Quad), quads.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, quadSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    unsigned int bvhSSBO;
    glGenBuffers(1, &bvhSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, builder.nodes.size() * sizeof(BVHNode), builder.nodes.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, bvhSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    unsigned int bvhPrimRefSSBO;
    glGenBuffers(1, &bvhPrimRefSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhPrimRefSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, builder.primRefs.size() * sizeof(BVHPrimRef), builder.primRefs.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, bvhPrimRefSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    unsigned int textureSSBO;
    glGenBuffers(1, &textureSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, textureSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, textures.size() * sizeof(Texture), textures.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, textureSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    unsigned int lightSSBO;
    glGenBuffers(1, &lightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, lights.size() * sizeof(BVHPrimRef), lights.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, lightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Render Loop
    glm::vec3 lastCamPos = camera.Position;
    glm::vec3 lastCamFront = camera.Front;
    int accumFrame = 0;

    while (!glfwWindowShouldClose(window))
    {

        // Timing per Frame
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Input
        processInput(window);

        // Reset temporal accumulation whenever the camera moves or looks around
        if (camera.Position != lastCamPos || camera.Front != lastCamFront) {
            accumFrame = 0;
            lastCamPos = camera.Position;
            lastCamFront = camera.Front;
        }

        // Render Commands
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Compute Shader Setup
        compShader.use();
        compShader.setVec3("camPos", camera.Position);
        compShader.setVec3("camFront", camera.Front);
        compShader.setVec3("camRight", camera.Right);
        compShader.setVec3("camUp", camera.Up);
        compShader.setFloat("vfov", camera.Zoom);

        compShader.setInt("samplesPerPixel", samplesPerPixel);
        compShader.setInt("maxDepth", maxDepth);
        compShader.setFloat("time", (float)glfwGetTime());
        compShader.setInt("accumFrame", accumFrame);
        compShader.setInt("lightCount", (int)lights.size());

        glDispatchCompute((SCR_WIDTH + 7) / 8, (SCR_HEIGHT + 7) / 8, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        accumFrame++;

        // Render Scene
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT);
        quadShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, computeTexture);
        quadShader.setInt("screenTexture", 0);
        renderQuad();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}


unsigned int quadVAO = 0;
unsigned int quadVBO;

void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
			// positions        // texture Coords
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};

        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);

        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);

        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(3*sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.ProcessKeyboard(DOWN, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.ProcessKeyboard(UP, deltaTime);
}


void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (initialMouse)
    {
        lastX = xpos;
        lastY = ypos;
        initialMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}