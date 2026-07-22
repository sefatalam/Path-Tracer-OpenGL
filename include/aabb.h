#ifndef AABB_H
#define AABB_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    AABB(glm::vec3 min, glm::vec3 max) :
        min(min), max(max) {}
    
    AABB(const AABB& a, const AABB& b) :
        min(glm::min(a.min, b.min)), max(glm::max(a.max, b.max)) {}
};

static inline AABB empty()
{
    return AABB(glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX));
}

static inline float surfaceArea(const AABB& box)
{
    glm::vec3 d = glm::max(box.max - box.min, glm::vec3(0.0f));
    return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
}

#endif