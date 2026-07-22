#ifndef SPHERE_H
#define SPHERE_H

#include "aabb.h"

struct Sphere {
    glm::vec3 center;
    float radius;
    int material_index;
    float _pad[3];
    Sphere(const glm::vec3& c, float r, int matIdx)
        : center(c), radius(std::fmax(0.0f, r)), material_index(matIdx), _pad{0.0f, 0.0f, 0.0f} {}
};
static_assert(sizeof(Sphere) == 32, "Sphere layout must match GLSL std430 Sphere struct");

inline AABB bboxSphere(const Sphere& s)
{
    glm::vec3 r = glm::vec3(s.radius);
    return AABB(s.center - r, s.center + r);
}

#endif