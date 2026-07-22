#ifndef TEXTURE_H
#define TEXTURE_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>

#include <iostream>

#include "stb_image.h"

enum TextureType
{
    SOLID,
    IMAGE
};

inline uint64_t loadBindlessTexture(const char* path);

struct Texture
{   
    glm::vec3 color;
    TextureType type;
    uint64_t handle;
    uint64_t _pad0;

    Texture(const glm::vec3& color, TextureType type, uint64_t handle) :
        color(color), type(type), handle(handle), _pad0(0.0f) {}

    static Texture Solid(const glm::vec3& color)
    {
        return Texture(color, SOLID, 0);
    }

    static Texture Image(const char* path)
    {
        return Texture(glm::vec3(0), IMAGE, loadBindlessTexture(path));
    }
};

static_assert(sizeof(Texture) == 32, "Texture layout must match GLSL std430 Texture struct");

inline uint64_t loadBindlessTexture(const char* path)
{
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &channels, 4);
    if (!data)
    {
        std::cout << "ERROR::TEXTURE:: failed to load image: " << path << "\n";
        return 0;
    }

    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    uint64_t handle = glGetTextureHandleARB(tex);
    glMakeTextureHandleResidentARB(handle);
    return handle;
}

#endif